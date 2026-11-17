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

    /** out = trocken * (1 - mix) + nass * mix, in-place auf `wet`. */
    void blendInPlace (juce::AudioBuffer<float>& wet,
                       const juce::AudioBuffer<float>& dry,
                       int numChannels,
                       int numSamples,
                       float mix) noexcept
    {
        const auto dryGain  = 1.0f - mix;
        const auto channels = juce::jmin (numChannels, wet.getNumChannels(), dry.getNumChannels());
        const auto samples  = juce::jmin (numSamples, wet.getNumSamples(), dry.getNumSamples());

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* w = wet.getWritePointer (ch);
            const auto* d = dry.getReadPointer (ch);

            for (int n = 0; n < samples; ++n)
                w[n] = d[n] * dryGain + w[n] * mix;
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

    int         latency = 0;      ///< Latenz des Moduls in Samples (0 = keine)
    LatencyLine latencyLine;      ///< Latenzausgleich fuer den Trockenpfad

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

    for (size_t i = 0; i < slotList.size(); ++i)
    {
        auto* slot = slotList[i].get();

        if (slot == nullptr)
            continue;

        auto* module = (i < moduleList.size()) ? moduleList[i].get() : nullptr;

        slot->latency   = (module != nullptr) ? juce::jmax (0, module->latencySamples()) : 0;
        cachedLatency  += slot->latency;

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
        slot->dry.clear();
        slot->display.pre.clear();
        slot->display.post.clear();
        slot->display.valid = false;
    }
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
void FxChainProcessor::process (juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    const auto numChannels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const auto numSamples  = juce::jmin (buffer.getNumSamples(), preparedBlockSize);

    if (numChannels <= 0 || numSamples <= 0)
        return;

    const auto chainBypassed = bypassAll.load (std::memory_order_relaxed);

    for (size_t i = 0; i < moduleList.size(); ++i)
    {
        auto* slot   = slotList[i].get();
        auto* module = moduleList[i].get();

        if (slot == nullptr || module == nullptr)
            continue;

        // Display: Signal, wie es am Modul ankommt.
        captureDisplay (slot->display.pre, buffer, numChannels, numSamples);
        slot->display.valid = true;

        const auto active = (! chainBypassed) && slot->enabled.load (std::memory_order_relaxed);
        const auto mix    = juce::jlimit (0.0f, 1.0f, slot->mix.load (std::memory_order_relaxed));
        const auto gain   = slot->outGain.load (std::memory_order_relaxed);

        if (! active || mix <= 0.0f)
        {
            // Modul uebersprungen: keine Bearbeitung, kein OUT GAIN — aber die
            // Latenz bleibt erhalten, damit die Kette nicht springt.
            slot->latencyLine.process (buffer, numChannels, numSamples);

            captureDisplay (slot->display.post, buffer, numChannels, numSamples);
            continue;
        }

        const auto needDry = (mix < 1.0f) || (slot->latency > 0);

        if (needDry)
            copyInto (slot->dry, buffer, numChannels, numSamples);

        // Trockenpfad auf die Modullatenz schieben, damit die Blende fluchtet.
        if (slot->latency > 0)
            slot->latencyLine.process (slot->dry, numChannels, numSamples);

        module->process (buffer);

        if (mix < 1.0f)
            blendInPlace (buffer, slot->dry, numChannels, numSamples, mix);

        if (gain != 1.0f)
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.applyGain (ch, 0, numSamples, gain);

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
