#include "fx/FxChainProcessor.h"

#include <atomic>
#include <memory>
#include <vector>

#include "core/Log.h"
#include "fx/FxFactory.h"

namespace bfka
{

namespace
{
    constexpr int kChannels = FxChainProcessor::kMaxChannels;

    /**
     * Verzoegerungslinie fuer den Latenzausgleich.
     *
     * Ein Modul mit Latenz L liefert am Ausgang das Signal von vor L Samples.
     * Damit der Trockenpfad der MIX-Blende phasengleich bleibt (und ein
     * uebersprungenes Modul die Kette nicht verschiebt), laeuft das trockene
     * Signal durch dieselbe Verzoegerung.
     */
    class LatencyLine
    {
    public:
        /** Legt die Linie fuer `latencySamplesToUse` Samples an (0 = Durchreichen). */
        void prepare (int numChannels, int latencySamplesToUse)
        {
            latencySamples = juce::jmax (0, latencySamplesToUse);
            writePos = 0;

            if (latencySamples > 0)
            {
                ring.setSize (juce::jmax (1, numChannels), latencySamples, false, true, true);
                ring.clear();
            }
            else
            {
                ring.setSize (0, 0, false, true, true);
            }
        }

        void reset() noexcept
        {
            writePos = 0;
            ring.clear();
        }

        int latency() const noexcept { return latencySamples; }

        /** Schiebt das Signal in-place um `latencySamples` nach hinten. */
        void process (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples) noexcept
        {
            if (latencySamples <= 0 || ring.getNumSamples() != latencySamples)
                return;

            const auto length   = latencySamples;
            const auto channels = juce::jmin (numChannels, ring.getNumChannels(), buffer.getNumChannels());

            // Achtung: die Ringlaenge ist die Latenz, nicht die Blockgroesse. Der
            // Block darf beliebig viele Samples haben; der Ring wird nur ueber
            // den Schreibzeiger (Modulo) durchlaufen.
            const auto samples = juce::jmin (numSamples, buffer.getNumSamples());

            if (samples <= 0)
                return;

            for (int ch = 0; ch < channels; ++ch)
            {
                auto* io   = buffer.getWritePointer (ch);
                auto* line = ring.getWritePointer (ch);
                auto pos   = writePos;

                for (int n = 0; n < samples; ++n)
                {
                    const auto in = io[n];
                    io[n]  = line[pos];
                    line[pos] = in;

                    if (++pos >= length)
                        pos = 0;
                }
            }

            writePos = (writePos + samples) % length;
        }

    private:
        juce::AudioBuffer<float> ring;
        int latencySamples = 0;
        int writePos       = 0;
    };

    /** Kopiert die genutzten Kanaele/Samples von `source` nach `dest`. */
    void copyInto (juce::AudioBuffer<float>& dest,
                   const juce::AudioBuffer<float>& source,
                   int numChannels,
                   int numSamples) noexcept
    {
        const auto channels = juce::jmin (numChannels, dest.getNumChannels(), source.getNumChannels());
        const auto samples  = juce::jmin (numSamples, dest.getNumSamples(), source.getNumSamples());

        for (int ch = 0; ch < channels; ++ch)
            dest.copyFrom (ch, 0, source, ch, 0, samples);
    }

    /**
     * out = trocken * (1 - mix) + nass * mix, wobei `mix` sampleweise von
     * `startMix` auf `endMix` laeuft (B-148, schliesst O4).
     *
     * Ein Sprung pro Block (frueher: konstanter Blockwert) hoert man bei
     * Automation als Zipper; die Rampe ist ein Sample lang linear.
     */
    void blendInPlaceRamp (juce::AudioBuffer<float>& wet,
                           const juce::AudioBuffer<float>& dry,
                           int numChannels,
                           int numSamples,
                           float startMix,
                           float endMix) noexcept
    {
        const auto channels  = juce::jmin (numChannels, wet.getNumChannels(), dry.getNumChannels());
        const auto samples   = juce::jmin (numSamples, wet.getNumSamples(), dry.getNumSamples());
        const auto increment = (endMix - startMix) / (float) juce::jmax (1, samples);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* w = wet.getWritePointer (ch);
            const auto* d = dry.getReadPointer (ch);

            float mix = startMix;

            for (int n = 0; n < samples; ++n)
            {
                mix += increment;
                w[n] = d[n] * (1.0f - mix) + w[n] * mix;
            }
        }
    }

    /** Fuellt das Display-Fenster mit den letzten "window" Samples des Blocks. */
    void captureDisplay (juce::AudioBuffer<float>& dest,
                         const juce::AudioBuffer<float>& source,
                         int numChannels,
                         int numSamples) noexcept
    {
        const auto window = dest.getNumSamples();

        if (window <= 0)
            return;

        const auto count     = juce::jmin (numSamples, window);
        const auto srcOffset = juce::jmax (0, numSamples - count);
        const auto channels  = dest.getNumChannels();

        for (int ch = 0; ch < channels; ++ch)
        {
            if (ch < numChannels && ch < source.getNumChannels())
            {
                dest.copyFrom (ch, 0, source, ch, srcOffset, count);
                dest.clear (ch, count, window - count);
            }
            else
            {
                dest.clear (ch, 0, window);
            }
        }
    }

    /**
     * Uebernimmt die Parameter aus dem Modulzustand.
     *
     * Ein Attribut des ValueTree heisst genau wie `FxParamDesc::id` und steht in
     * Anzeige-Einheiten (wie im Referenz-XML). Werte werden auf den
     * deklarierten Bereich geklemmt; fehlende Attribute lassen den Default des
     * Moduls stehen.
     */
    void applyModuleParams (FxModule& module, const juce::ValueTree& data)
    {
        if (! data.isValid())
            return;

        const auto& descriptors = module.params();

        for (size_t i = 0; i < descriptors.size(); ++i)
        {
            const auto& desc = descriptors[i];
            const juce::Identifier attribute (desc.id);

            if (! data.hasProperty (attribute))
                continue;

            const auto& value = data.getProperty (attribute);

            if (value.isVoid())
                continue;

            if (! (value.isDouble() || value.isInt() || value.isInt64() || value.isBool()))
                continue;

            const auto engineering = (float) (double) value;
            module.setParam ((int) i, juce::jlimit (desc.minValue, desc.maxValue, engineering));
        }
    }
} // namespace

// ---------------------------------------------------------------------------
// Laufzeitdaten eines Kettenplatzes
// ---------------------------------------------------------------------------
struct FxChainProcessor::SlotState
{
    std::atomic<bool>  enabled { true };
    std::atomic<float> mix     { 1.0f };
    std::atomic<float> outGain { 1.0f };

    /** Zuletzt angewendete Werte (Audio-Thread); Ziel sind die Atomics (B-148). */
    float mixCurrent  = 1.0f;
    float gainCurrent = 1.0f;

    int         latency = 0;      ///< Latenz des Moduls in Samples (0 = keine)
    LatencyLine latencyLine;      ///< Latenzausgleich fuer den Trockenpfad

    /**
     * Latenz der Vormodule (B-146): der externe Sidechain muss um denselben
     * Betrag laufen wie das Nutzsignal, damit Detektor und Signal fluchten.
     */
    int         sidechainLatency = 0;
    LatencyLine sidechainLine;

    juce::AudioBuffer<float> dry; ///< trockener Zwischenpuffer (in prepare reserviert)
    ModuleDisplay            display;
};

// ---------------------------------------------------------------------------
// Lebenszyklus
// ---------------------------------------------------------------------------
FxChainProcessor::FxChainProcessor() = default;
FxChainProcessor::~FxChainProcessor() = default;

void FxChainProcessor::prepare (double sampleRate, int maxBlockSize)
{
    preparedSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    preparedBlockSize  = juce::jmax (1, maxBlockSize);
    displayWindow      = juce::jmin (preparedBlockSize, kMaxDisplaySamples);
    prepared           = true;

    applyPreparation();
}

void FxChainProcessor::applyPreparation()
{
    cachedLatency = 0;

    // Module zuerst vorbereiten, damit ihre Latenz bekannt ist.
    for (auto& module : moduleList)
        if (module != nullptr)
            module->prepare (preparedSampleRate, preparedBlockSize);

    sidechainWork.setSize (kChannels, juce::jmax (1, preparedBlockSize), false, true, true);
    sidechainWork.clear();

    int latencyBefore = 0;

    for (size_t i = 0; i < slotList.size(); ++i)
    {
        auto* slot = slotList[i].get();

        if (slot == nullptr)
            continue;

        auto* module = (i < moduleList.size()) ? moduleList[i].get() : nullptr;

        slot->latency   = (module != nullptr) ? juce::jmax (0, module->latencySamples()) : 0;
        cachedLatency  += slot->latency;

        // Der Sidechain dieses Platzes laeuft um die Latenz der Vormodule
        // verzoegert (B-146).
        slot->sidechainLatency = latencyBefore;
        slot->sidechainLine.prepare (kChannels, slot->sidechainLatency);

        latencyBefore += slot->latency;

        slot->latencyLine.prepare (kChannels, slot->latency);
        slot->dry.setSize (kChannels, preparedBlockSize, false, true, true);

        slot->display.pre.setSize  (kChannels, juce::jmax (1, displayWindow), false, true, true);
        slot->display.post.setSize (kChannels, juce::jmax (1, displayWindow), false, true, true);
        slot->display.valid = false;
    }

    reset();
}

void FxChainProcessor::reset()
{
    for (auto& module : moduleList)
        if (module != nullptr)
            module->reset();

    for (auto& slot : slotList)
    {
        if (slot == nullptr)
            continue;

        slot->latencyLine.reset();
        slot->sidechainLine.reset();
        slot->dry.clear();

        // Kein Rampen aus einem alten Stand nach einem Reset (B-148).
        slot->mixCurrent  = juce::jlimit (0.0f, 1.0f, slot->mix.load (std::memory_order_relaxed));
        slot->gainCurrent = slot->outGain.load (std::memory_order_relaxed);
        slot->display.pre.clear();
        slot->display.post.clear();
        slot->display.valid = false;
    }

    sidechainWork.clear();
}

void FxChainProcessor::setChain (const FxChain& chain)
{
    moduleList.clear();
    slotList.clear();

    bypassAll.store (chain.bypassAll, std::memory_order_relaxed);

    moduleList.reserve (chain.modules.size());
    slotList.reserve (chain.modules.size());

    for (size_t i = 0; i < chain.modules.size(); ++i)
    {
        const auto& state = chain.modules[i];

        auto slot = std::make_unique<SlotState>();
        slot->enabled.store (state.enabled, std::memory_order_relaxed);
        slot->mix.store (juce::jlimit (0.0f, 1.0f, state.mix), std::memory_order_relaxed);
        slot->outGain.store (juce::jmax (0.0f, state.outGain), std::memory_order_relaxed);

        // Der erste Block soll exakt den gesetzten Wert haben, nicht von einem
        // alten Stand hochrampen (B-148).
        slot->mixCurrent  = juce::jlimit (0.0f, 1.0f, state.mix);
        slot->gainCurrent = juce::jmax (0.0f, state.outGain);

        auto module = FxFactory::get().create (state.type);

        if (module == nullptr)
        {
            log::w ("FX-Kette: Modultyp \"" + state.type
                    + "\" an Position " + juce::String ((int) i)
                    + " ist unbekannt - der Platz wird uebersprungen.");
        }
        else
        {
            applyModuleParams (*module, state.data);

            // Struktur-Zustand (Baender, Kurven, IR-Pfad) an das Modul geben.
            if (state.data.isValid())
                module->setState (state.data);
        }

        moduleList.push_back (std::move (module));
        slotList.push_back (std::move (slot));
    }

    if (prepared)
        applyPreparation();
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
void FxChainProcessor::process (juce::AudioBuffer<float>& buffer,
                                const juce::AudioBuffer<float>* sidechain)
{
    if (! prepared)
        return;

    const auto numChannels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const auto numSamples  = juce::jmin (buffer.getNumSamples(), preparedBlockSize);

    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto chainBypassed = bypassAll.load (std::memory_order_relaxed);

    // Ein externer Sidechain liegt nur an, wenn er lang genug ist und die
    // Arbeitskopie Platz bietet (B-146).
    const bool haveSidechain = sidechain != nullptr
                               && sidechain->getNumChannels() > 0
                               && sidechain->getNumSamples() >= numSamples
                               && sidechainWork.getNumSamples() >= numSamples;

    for (size_t i = 0; i < moduleList.size(); ++i)
    {
        auto* slot   = slotList[i].get();
        auto* module = moduleList[i].get();

        if (slot == nullptr || module == nullptr)
            continue;

        // Externer Sidechain an das Modul reichen (verzoegert um die Latenz
        // der Vormodule). Module ohne Detektor ignorieren den Zeiger.
        if (module->acceptsExternalSidechain())
        {
            if (haveSidechain)
            {
                const auto scChannels = juce::jmin (kChannels, sidechain->getNumChannels(),
                                                    sidechainWork.getNumChannels());

                for (int ch = 0; ch < scChannels; ++ch)
                    sidechainWork.copyFrom (ch, 0, *sidechain, ch, 0, numSamples);

                if (scChannels == 1)
                    sidechainWork.copyFrom (1, 0, *sidechain, 0, 0, numSamples);

                if (slot->sidechainLatency > 0)
                    slot->sidechainLine.process (sidechainWork, kChannels, numSamples);

                module->setSidechainBuffer (&sidechainWork);
            }
            else
            {
                module->setSidechainBuffer (nullptr);
            }
        }

        // Display: Signal, wie es am Modul ankommt.
        captureDisplay (slot->display.pre, buffer, numChannels, numSamples);
        slot->display.valid = true;

        const auto active     = (! chainBypassed) && slot->enabled.load (std::memory_order_relaxed);
        const auto targetMix  = juce::jlimit (0.0f, 1.0f, slot->mix.load (std::memory_order_relaxed));
        const auto targetGain = slot->outGain.load (std::memory_order_relaxed);

        // Ein ausgeschaltetes Modul oder ein Platz, der schon vollstaendig
        // trocken ist, wird uebersprungen (die Rampe ist dann zu Ende).
        if (! active || (slot->mixCurrent <= 0.0f && targetMix <= 0.0f))
        {
            // Modul uebersprungen: keine Bearbeitung, kein OUT GAIN — aber die
            // Latenz bleibt erhalten, damit die Kette nicht springt.
            slot->latencyLine.process (buffer, numChannels, numSamples);
            module->setSidechainBuffer (nullptr);

            slot->mixCurrent  = targetMix;
            slot->gainCurrent = targetGain;

            captureDisplay (slot->display.post, buffer, numChannels, numSamples);
            continue;
        }

        const float startMix  = slot->mixCurrent;
        const float startGain = slot->gainCurrent;

        slot->mixCurrent  = targetMix;
        slot->gainCurrent = targetGain;

        const auto needDry = (startMix < 1.0f) || (targetMix < 1.0f) || (slot->latency > 0);

        if (needDry)
            copyInto (slot->dry, buffer, numChannels, numSamples);

        // Trockenpfad auf die Modullatenz schieben, damit die Blende fluchtet.
        if (slot->latency > 0)
            slot->latencyLine.process (slot->dry, numChannels, numSamples);

        module->process (buffer);
        module->setSidechainBuffer (nullptr);

        if (needDry && (startMix < 1.0f || targetMix < 1.0f))
            blendInPlaceRamp (buffer, slot->dry, numChannels, numSamples, startMix, targetMix);

        if (startGain != 1.0f || targetGain != 1.0f)
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.applyGainRamp (ch, 0, numSamples, startGain, targetGain);

        captureDisplay (slot->display.post, buffer, numChannels, numSamples);
    }
}

// ---------------------------------------------------------------------------
// Zugriff
// ---------------------------------------------------------------------------
FxModule* FxChainProcessor::moduleAt (int moduleIndex) noexcept
{
    if (moduleIndex < 0 || moduleIndex >= (int) moduleList.size())
        return nullptr;

    return moduleList[(size_t) moduleIndex].get();
}

const FxModule* FxChainProcessor::moduleAt (int moduleIndex) const noexcept
{
    return const_cast<FxChainProcessor*> (this)->moduleAt (moduleIndex);
}

int FxChainProcessor::sidechainLatencyFor (int moduleIndex) const noexcept
{
    if (moduleIndex < 0 || moduleIndex >= (int) slotList.size())
        return 0;

    const auto* slot = slotList[(size_t) moduleIndex].get();

    return slot != nullptr ? slot->sidechainLatency : 0;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
const ModuleDisplay& FxChainProcessor::displayFor (int moduleIndex) const
{
    static const ModuleDisplay invalidDisplay;

    if (moduleIndex < 0 || moduleIndex >= (int) slotList.size())
        return invalidDisplay;

    const auto* slot = slotList[(size_t) moduleIndex].get();

    if (slot == nullptr)
        return invalidDisplay;

    return slot->display;
}

// ---------------------------------------------------------------------------
// Live-Bedienung
// ---------------------------------------------------------------------------
void FxChainProcessor::setModuleEnabled (int moduleIndex, bool shouldBeEnabled) noexcept
{
    if (moduleIndex < 0 || moduleIndex >= (int) slotList.size())
        return;

    if (auto* slot = slotList[(size_t) moduleIndex].get())
        slot->enabled.store (shouldBeEnabled, std::memory_order_relaxed);
}

void FxChainProcessor::setModuleMix (int moduleIndex, float newMix) noexcept
{
    if (moduleIndex < 0 || moduleIndex >= (int) slotList.size())
        return;

    if (auto* slot = slotList[(size_t) moduleIndex].get())
        slot->mix.store (juce::jlimit (0.0f, 1.0f, newMix), std::memory_order_relaxed);
}

void FxChainProcessor::setModuleOutGain (int moduleIndex, float newOutGain) noexcept
{
    if (moduleIndex < 0 || moduleIndex >= (int) slotList.size())
        return;

    if (auto* slot = slotList[(size_t) moduleIndex].get())
        slot->outGain.store (juce::jmax (0.0f, newOutGain), std::memory_order_relaxed);
}

} // namespace bfka
