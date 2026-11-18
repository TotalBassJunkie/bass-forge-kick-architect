// B-83b: FOLLOW DAW - Host-BPM aus dem Playhead, gedrosselt auf 4/Sekunde.
#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"

#include "core/FxChainIO.h"
#include "core/ParameterRegistry.h"
#include "core/Version.h"
#include "core/Log.h"
#include "core/WavWriter.h"
#include "engine/OfflineRenderer.h"
#include "fx/modules/KickShaperModule.h"
#include "library/SampleLibrary.h"

namespace bfka
{

PluginProcessor::PluginProcessor()
    : juce::AudioProcessor (BusesProperties()
                                // Detektor-Quellen (B-146), beide optional:
                                // `Input` ist der Haupt-Eingang, den der Host
                                // in-place mit dem Ausgang teilt; `Sidechain`
                                // ist der VST3-Aux-Bus und hat Vorrang.
                                .withInput ("Input", juce::AudioChannelSet::stereo(), false)
                                .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    // Wurzel `BFKAKick`: enthaelt Slots, `<FxChain>` und die Metadaten. Die
    // Parameterwerte kommen beim Capture ohnehin aus dem APVTS und werden dort
    // ueberschrieben; so gibt es nur ein Schema fuer den Gesamt-Zustand (B-143).
    extraState = KickState{}.toValueTree();

    log::i (juce::String (Version::productName) + " " + Version::get() + " gestartet");
}

PluginProcessor::~PluginProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& registry = ParameterRegistry::get();

    for (const auto& d : registry.all())
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { d.id, 1 },
            d.displayName,
            juce::NormalisableRange<float> (0.0f, 1.0f),
            d.normalisedDefault()));
    }

    return layout;
}

const juce::String PluginProcessor::getName() const
{
    return Version::productName;
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    // Bezugsrate der Tempo-Drosselung (B-83b): hoechstens 4 Uebernahmen/Sekunde.
    preparedSampleRateForTempo = (sampleRate > 0.0) ? sampleRate : 48000.0;

    renderer.prepare (sampleRate, samplesPerBlock);
    fxChain.prepare (sampleRate, samplesPerBlock);

    // Arbeitskopie fuer den externen Detektor (B-146). Wird im Audio-Thread
    // nur beschrieben, nie vergroessert.
    sidechainInput.setSize (FxChainProcessor::kMaxChannels, juce::jmax (1, samplesPerBlock),
                            false, true, true);
    sidechainInput.clear();

    // Kette aus dem Zustand bauen und die Latenz an den Host melden (B-143).
    rebuildEngineChain();

    if (contentRoot == juce::File())
    {
        contentRoot = defaultContentRoot();
        log::i ("Content-Root: " + contentRoot.getFullPathName()
                + (contentRoot.isDirectory() ? " (vorhanden)" : " (fehlt)"));
    }

    log::d ("prepareToPlay: " + juce::String (sampleRate) + " Hz, "
            + juce::String (samplesPerBlock) + " Samples");
}

void PluginProcessor::releaseResources()
{
    renderer.reset();
    fxChain.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (! (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()))
        return false;

    if (layouts.outputBuses.size() != 1)
        return false;

    // Der Sidechain-Eingang (B-146) ist optional: aus, mono oder stereo.
    for (int i = 0; i < layouts.inputBuses.size(); ++i)
    {
        const auto& bus = layouts.inputBuses.getReference (i);

        if (! (bus.isDisabled() || bus == juce::AudioChannelSet::mono()
               || bus == juce::AudioChannelSet::stereo()))
            return false;
    }

    return true;
}

int PluginProcessor::getSidechainChannels() const noexcept
{
    const auto index = sidechainBusIndex.load();

    if (index < 0)
        return 0;

    if (auto* bus = getBus (true, index); bus != nullptr && bus->isEnabled())
        return bus->getNumberOfChannels();

    return 0;
}

bool PluginProcessor::exportKickToFile (const juce::File& file, juce::String& error)
{
    const double rate = juce::jmax (8000.0, getSampleRate() > 0.0 ? getSampleRate() : 48000.0);

    suspendProcessing (true);
    const auto rendered = OfflineRenderer::render (renderer, fxChain, rate, 512, 0.0);
    suspendProcessing (false);

    if (! rendered.ok)
    {
        error = rendered.error.isNotEmpty() ? rendered.error
                                            : juce::String ("Render fehlgeschlagen.");
        return false;
    }

    updateModuleRenderLength();
    return WavWriter::write (file, rendered.buffer, rate, 24, error);
}

void PluginProcessor::beginStateEdit (const juce::String& name)
{
    undoHistory.beginEdit (name, captureState());
}

void PluginProcessor::endStateEdit()
{
    undoHistory.endEdit();
}

bool PluginProcessor::canUndo() const { return undoHistory.canUndo(); }
bool PluginProcessor::canRedo() const { return undoHistory.canRedo(); }

juce::String PluginProcessor::undoName() const { return undoHistory.undoName(); }
juce::String PluginProcessor::redoName() const { return undoHistory.redoName(); }

bool PluginProcessor::undo()
{
    // `undo()` braucht den AKTUELLEN Zustand, um ihn fuer Redo zu sichern.
    KickState current = captureState();

    if (! undoHistory.undo (current))
        return false;

    applyState (current);
    return true;
}

bool PluginProcessor::redo()
{
    KickState current = captureState();

    if (! undoHistory.redo (current))
        return false;

    applyState (current);
    return true;
}

double PluginProcessor::getTailLengthSeconds() const
{
    // B-150: Kick-Laenge plus der Nachhall der FX-Kette. Der Host nutzt den
    // Wert, um die Spur nach dem letzten Event nicht abzuschneiden. Die Kette
    // ist seriell, deshalb addieren sich die Modul-Tails.
    const double total = renderer.kickLengthSeconds() + fxChain.tailSeconds();

    return juce::jlimit (0.05, 60.0, total);
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    // Nur den Ausgangsbus beschreiben. Mit aktivem Sidechain traegt `buffer`
    // vorne die Eingangs- und danach die Ausgangskanaele; ein `clear()` auf dem
    // ganzen Buffer wuerde den Detektor loeschen (B-146).
    auto outBuffer = getBusBuffer (buffer, false, 0);

    if (outBuffer.getNumChannels() == 0 || outBuffer.getNumSamples() == 0)
        outBuffer = buffer;

    const auto numSamples = outBuffer.getNumSamples();
    const auto outOffset  = getChannelIndexInProcessBlockBuffer (false, 0, 0);

    // Detektor-Quelle bestimmen: Aux-Bus (Index 1) vor Haupt-Eingang (Index 0).
    // Der Haupt-Eingang teilt sich die Kanaele mit dem Ausgang (In-Place-
    // Verarbeitung); deshalb wird er **vor** dem Leeren kopiert.
    int busIndex = -1;

    for (int i = getBusCount (true) - 1; i >= 0; --i)
    {
        auto* bus = getBus (true, i);

        if (bus == nullptr || ! bus->isEnabled() || bus->getNumberOfChannels() <= 0)
            continue;

        const auto offset = getChannelIndexInProcessBlockBuffer (true, i, 0);

        // Ein Aux-Bus, der auf denselben Kanaelen wie der Ausgang liegt, kann
        // von JUCE nicht getrennt werden (kein Haupt-Eingang belegt) — dann
        // lieber gar kein Sidechain als ein ueberschriebener Detektor.
        if (i > 0 && offset == outOffset)
            continue;

        busIndex = i;
        break;
    }

    const juce::AudioBuffer<float>* sidechain = nullptr;

    const bool canCopySidechain = busIndex >= 0
                                  && sidechainInput.getNumSamples() >= numSamples
                                  && sidechainInput.getNumChannels() > 0;

    if (canCopySidechain)
    {
        auto scBuffer = getBusBuffer (buffer, true, busIndex);

        if (scBuffer.getNumChannels() > 0 && scBuffer.getNumSamples() >= numSamples)
        {
            const auto channels = juce::jmin (sidechainInput.getNumChannels(),
                                             scBuffer.getNumChannels());

            for (int ch = 0; ch < channels; ++ch)
                sidechainInput.copyFrom (ch, 0, scBuffer, ch, 0, numSamples);

            for (int ch = channels; ch < sidechainInput.getNumChannels(); ++ch)
                sidechainInput.copyFrom (ch, 0, scBuffer, 0, 0, numSamples);

            sidechain = &sidechainInput;
        }
    }

    sidechainBusIndex.store (sidechain != nullptr ? busIndex : -1, std::memory_order_relaxed);
    sidechainActive.store (sidechain != nullptr, std::memory_order_relaxed);

    outBuffer.clear();

    if (auto* masterParam = apvts.getRawParameterValue ("master_vol"))
        masterGain.store (masterParam->load (std::memory_order_relaxed),
                          std::memory_order_relaxed);

    // --- FOLLOW DAW (B-83b) ------------------------------------------------
    // Tempo aus dem Host-Playhead. Nur wenn der Zustand FOLLOW DAW verlangt;
    // Uebernahmen sind auf vier pro Sekunde gedrosselt (kein Allokieren).
    if (hostFollowEnabled.load (std::memory_order_relaxed))
    {
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                if (auto bpmOpt = pos->getBpm())
                {
                    const double bpm = *bpmOpt;

                    if (bpm > 0.0)
                    {
                        lastHostBpm.store (bpm, std::memory_order_relaxed);

                        const juce::int64 blockStart = totalSamplesProcessed.load (std::memory_order_relaxed);

                        if (blockStart - lastHostBpmSampleTime
                                >= (juce::int64) (preparedSampleRateForTempo / 4.0))
                        {
                            lastHostBpmSampleTime = blockStart;
                            applyEffectiveBpm (bpm);
                            hostBpmApplyCounter.fetch_add (1, std::memory_order_relaxed);
                        }
                    }
                }
            }
        }
    }

    // 1) Kick rendern (additiv).
    renderer.renderNextBlock (outBuffer, 0, numSamples);

    // 2) FX-Kette (in-place, Module in Reihenfolge, optional mit Sidechain).
    if (fxChain.numModules() > 0)
        fxChain.process (outBuffer, sidechain);

    // 3) Master-Fader.
    const auto gain = juce::jlimit (0.0f, 2.0f, masterGain.load (std::memory_order_relaxed));

    if (! juce::approximatelyEqual (gain, 1.0f))
        outBuffer.applyGain (0, numSamples, gain);

    // 4) MIDI: jede Note-On startet den Kick neu.
    for (const auto meta : midi)
    {
        const auto message = meta.getMessage();

        if (message.isNoteOn())
        {
            lastNote.store (message.getNoteNumber());
            renderer.noteOn();
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            lastNote.store (-1);
        }
    }

    // Sample-Zaehler fuer die Tempo-Drosselung (B-83b), audio-thread-lokal.
    totalSamplesProcessed.fetch_add ((juce::int64) buffer.getNumSamples(),
                                     std::memory_order_relaxed);
}

void PluginProcessor::triggerKick()
{
    renderer.noteOn();
}

void PluginProcessor::updateModuleRenderLength()
{
    const auto seconds = renderer.kickLengthSeconds();

    for (int slot = 0; slot < fxChain.numModules(); ++slot)
    {
        if (auto* shaper = dynamic_cast<KickShaperModule*> (fxChain.moduleAt (slot)))
            shaper->setRenderLength (seconds);
    }
}

// ---------------------------------------------------------------------------
// KICK-Seite: Slots (B-154)
// ---------------------------------------------------------------------------
bool PluginProcessor::setSlotSample (int slot, const juce::File& file, juce::String& error)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
    {
        error = "Slot " + juce::String (slot) + " liegt ausserhalb von 1..12.";
        return false;
    }

    if (! file.existsAsFile())
    {
        error = "Datei nicht gefunden: " + file.getFileName();
        return false;
    }

    if (! contentRoot.isDirectory())
        contentRoot = defaultContentRoot();

    SampleRef ref;
    ref.fileName = file.getFileName();
    ref.absPath  = file.getFullPathName();

    // Innerhalb der Library relativ ablegen: nur so ueberlebt das Preset einen
    // Umzug der Library.
    if (contentRoot.isDirectory() && file.isAChildOf (contentRoot))
        ref.relPath = file.getRelativePathFrom (contentRoot);

    auto state = captureState();
    state.setSample (slot, ref);
    extraState = state.toValueTree();

    juce::StringArray warnings;
    const auto ok = assembler.loadOneSlot (slot, state, contentRoot, renderer, warnings);

    if (! ok)
    {
        error = warnings.isEmpty() ? "Sample konnte nicht geladen werden: " + ref.fileName
                                   : warnings.joinIntoString ("; ");
        log::w ("setSlotSample(" + juce::String (slot) + "): " + error);
        return false;
    }

    log::i ("Slot " + juce::String (slot) + " geladen: " + ref.fileName);
    return true;
}

void PluginProcessor::clearSlotSample (int slot)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return;

    auto state = captureState();
    state.clearSample (slot);
    extraState = state.toValueTree();

    assembler.clearSlot (slot, renderer);
}

void PluginProcessor::applySlotSettings (int slot)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return;

    // Der Zustand traegt die Parameterwerte (APVTS ist die Wahrheit).
    assembler.applySlotSettings (slot, captureState(), renderer);
}

void PluginProcessor::applyAllSlotSettings()
{
    const auto state = captureState();

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        assembler.applySlotSettings (slot, state, renderer);
}

bool PluginProcessor::isSlotFilled (int slot) const
{
    return renderer.isSlotFilled (slot);
}

const KickAssembler::SampleEnvelope& PluginProcessor::getSlotEnvelope (int slot) const
{
    return assembler.envelope (slot);
}

void PluginProcessor::setSlotRole (int slot, Role role)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return;

    auto state = captureState();
    state.setRole (slot, role);
    extraState = state.toValueTree();
}

void PluginProcessor::setSlotLocked (int slot, bool shouldBeLocked)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return;

    auto state = captureState();
    state.setLocked (slot, shouldBeLocked);
    extraState = state.toValueTree();
}

void PluginProcessor::setSlotKeyTracked (int slot, bool shouldTrack)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return;

    auto state = captureState();
    state.setKeyTracked (slot, shouldTrack);
    extraState = state.toValueTree();
}

int PluginProcessor::getNumFilledSlots() const
{
    auto count = 0;

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        if (renderer.isSlotFilled (slot))
            ++count;

    return count;
}

double PluginProcessor::getBpm() const
{
    juce::String error;

    if (auto stored = KickState::fromValueTree (extraState, error))
    {
        // FOLLOW DAW mit bekanntem Host-Tempo: das Host-BPM ist wirksam (B-83b).
        if (stored->bpmMode() == BpmMode::syncBpm)
        {
            const auto host = lastHostBpm.load (std::memory_order_relaxed);

            if (host > 0.0)
                return host;
        }

        return stored->getBpm();
    }

    return 150.0;
}

double PluginProcessor::hostBpm() const noexcept
{
    return lastHostBpm.load (std::memory_order_relaxed);
}

bool PluginProcessor::isFollowingHost() const noexcept
{
    return hostFollowEnabled.load (std::memory_order_relaxed);
}

int PluginProcessor::hostBpmApplyCount() const noexcept
{
    return hostBpmApplyCounter.load (std::memory_order_relaxed);
}

void PluginProcessor::applyEffectiveBpm (double bpm)
{
    // Klemmen wie `setBpm()`, aber ohne den Zustand anzufassen: das wirksame
    // Tempo ist eine Laufzeitgroesse, kein Presetwert (B-83b).
    const auto tempo = juce::jlimit (KickRenderer::kMinBpm, KickRenderer::kMaxBpm, bpm);

    auto settings = renderer.renderSettings();
    settings.bpm = tempo;
    renderer.setRenderSettings (settings);

    // Die Kick-Laenge haengt am Tempo (B-147). Iteriert nur ueber die Module.
    updateModuleRenderLength();
}

void PluginProcessor::setBpm (double bpm)
{
    auto state = captureState();
    state.setBpm (juce::jlimit (KickRenderer::kMinBpm, KickRenderer::kMaxBpm, bpm));
    extraState = state.toValueTree();

    auto settings = renderer.renderSettings();
    settings.bpm = state.getBpm();
    renderer.setRenderSettings (settings);

    // Die Kick-Laenge haengt am Tempo (B-147).
    updateModuleRenderLength();
}

namespace
{
    /** Neuer Kettenplatz mit Werkswerten (kein `data`: Modul-Defaults gelten). */
    FxModuleState makeDemoModule (const juce::String& type)
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = true;
        state.mix     = 1.0f;
        state.outGain = 1.0f;
        state.data    = juce::ValueTree (juce::Identifier ("Module"));
        return state;
    }

    /** Setzt ein Attribut in Anzeige-Einheiten (wie im Referenz-XML). */
    void setDemoAttribute (FxModuleState& state, const juce::String& id, double value)
    {
        if (! state.data.isValid())
            state.data = juce::ValueTree (juce::Identifier ("Module"));

        state.data.setProperty (juce::Identifier (id), juce::var (value), nullptr);
    }
} // namespace

void PluginProcessor::loadFxDemoChain()
{
    FxChain chain;

    // 1) Multiband: deutliche Dynamik, Linearphase aus (Latenz 0).
    auto multiband = makeDemoModule ("ott");
    setDemoAttribute (multiband, "depth",   80.0);
    setDemoAttribute (multiband, "time",    45.0);
    setDemoAttribute (multiband, "upwdStr", 60.0);
    setDemoAttribute (multiband, "dnwdStr", 100.0);
    setDemoAttribute (multiband, "thL",     -30.0);
    setDemoAttribute (multiband, "thM",     -28.0);
    setDemoAttribute (multiband, "thH",     -26.0);
    chain.modules.push_back (std::move (multiband));

    // 2) Convolver nur, wenn eine IR vorhanden ist. IRs werden nicht
    //    mitgeliefert; eine bereits gewaehlte IR der aktuellen Kette wird
    //    weiterverwendet.
    juce::String irPath;

    for (const auto& module : currentChain.modules)
    {
        if (module.type != "reverb")
            continue;

        const auto ir = module.data.getProperty (juce::Identifier ("ir"));

        if (ir.isString() && ir.toString().isNotEmpty())
        {
            irPath = ir.toString();
            break;
        }
    }

    if (irPath.isNotEmpty())
    {
        auto convolver = makeDemoModule ("reverb");
        convolver.data.setProperty (juce::Identifier ("ir"), irPath, nullptr);
        setDemoAttribute (convolver, "wet",      30.0);
        setDemoAttribute (convolver, "predelay", 20.0);
        setDemoAttribute (convolver, "stereo",   110.0);
        chain.modules.push_back (std::move (convolver));
    }

    // 3) Dynamic EQ: Low-Shelf an, eine Bell-Absenkung und eine Bell-Anhebung
    //    mit Dynamik, High-Shelf leicht an.
    auto dynamicEq = makeDemoModule ("dynEq");

    setDemoAttribute (dynamicEq, "b1_active", 1.0);
    setDemoAttribute (dynamicEq, "b1_type",   1.0);      // Low-Shelf
    setDemoAttribute (dynamicEq, "b1_freq",   60.0);
    setDemoAttribute (dynamicEq, "b1_gain",   5.0);

    setDemoAttribute (dynamicEq, "b2_active",  1.0);
    setDemoAttribute (dynamicEq, "b2_type",    0.0);     // Bell
    setDemoAttribute (dynamicEq, "b2_freq",    400.0);
    setDemoAttribute (dynamicEq, "b2_gain",    -4.0);
    setDemoAttribute (dynamicEq, "b2_dyn",     1.0);
    setDemoAttribute (dynamicEq, "b2_split",   1.0);
    setDemoAttribute (dynamicEq, "b2_thresh",  -24.0);
    setDemoAttribute (dynamicEq, "b2_ratio",   3.0);

    setDemoAttribute (dynamicEq, "b3_active", 1.0);
    setDemoAttribute (dynamicEq, "b3_type",   0.0);
    setDemoAttribute (dynamicEq, "b3_freq",   3000.0);
    setDemoAttribute (dynamicEq, "b3_gain",   4.0);
    setDemoAttribute (dynamicEq, "b3_dyn",    1.0);
    setDemoAttribute (dynamicEq, "b3_thresh", -26.0);
    setDemoAttribute (dynamicEq, "b3_ratio",  2.0);

    setDemoAttribute (dynamicEq, "b4_active", 1.0);
    setDemoAttribute (dynamicEq, "b4_type",   2.0);      // High-Shelf
    setDemoAttribute (dynamicEq, "b4_freq",   8000.0);
    setDemoAttribute (dynamicEq, "b4_gain",   3.0);

    setDemoAttribute (dynamicEq, "outGain", 0.0);
    chain.modules.push_back (std::move (dynamicEq));

    setFxChain (chain);

    log::i ("FX-Demo-Kette: " + juce::String ((int) chain.modules.size())
            + " Module, Latenz " + juce::String (getLatencySamples()) + " Samples");
}

bool PluginProcessor::startDemoKick()
{
    if (! contentRoot.isDirectory())
        contentRoot = defaultContentRoot();

    if (! contentRoot.isDirectory())
    {
        lastLoadReport = "Keine Sample-Library gefunden.";
        log::w (lastLoadReport);
        return false;
    }

    // Erst die Referenzen in den Zustand schreiben, dann laden: so zeigen die
    // Slot-Karten der KICK-Seite denselben Kick, den der Renderer spielt (B-154).
    std::array<juce::File, 3> demoFiles;
    KickAssembler::findDemoSamples (contentRoot, demoFiles);

    // Startpegel der Demo (dB) landen in den Slot-Parametern, nicht im Renderer.
    static const float demoGainDb[] { 0.0f, -6.0f, -12.0f };

    // Rollen passend zu den Demo-Ordnern, damit die Slot-Karten sie zeigen.
    static const Role demoRoles[] { Role::punch, Role::rumble, Role::click };

    auto state = captureState();
    juce::StringArray warnings;
    auto found = 0;

    for (size_t i = 0; i < demoFiles.size(); ++i)
    {
        if (! demoFiles[i].existsAsFile())
        {
            warnings.add ("Demo-Ordner " + juce::String ((int) i + 1) + " ist leer");
            continue;
        }

        const auto slot = (int) i;

        SampleRef ref;
        ref.fileName = demoFiles[i].getFileName();
        ref.absPath  = demoFiles[i].getFullPathName();

        if (demoFiles[i].isAChildOf (contentRoot))
            ref.relPath = demoFiles[i].getRelativePathFrom (contentRoot);

        state.setSample (slot, ref);
        state.setRole (slot, demoRoles[i]);

        if (const auto* volume = ParameterRegistry::get().find ("l" + juce::String (slot) + "_vol"))
            state.setParam (volume->id, volume->toNormalised (demoGainDb[i]));

        ++found;
    }

    if (found == 0)
    {
        lastLoadReport = "Demo-Kick: keine Samples in der Library gefunden.";
        log::w (lastLoadReport);
        return false;
    }

    // `applyState()` laedt die Sample-Referenzen selbst in den Renderer (B-154).
    applyState (state);

    const auto loaded = getNumFilledSlots();

    // Ein neuer Kick bringt ein neues Tempo - die Shaper-Huellkurve laeuft
    // ueber die Kick-Laenge (B-147).
    updateModuleRenderLength();

    lastLoadReport = loaded > 0 ? ("Demo-Kick aus " + contentRoot.getFileName())
                                : "Demo-Kick konnte nicht gebaut werden.";

    if (! warnings.isEmpty())
        lastLoadReport << " | " << warnings.joinIntoString ("; ");

    log::i (lastLoadReport);

    if (loaded > 0)
        renderer.noteOn();

    return loaded > 0;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

KickState PluginProcessor::captureState() const
{
    // Basis: der gespeicherte Nicht-Parameter-Zustand. Die Wurzel ist ein
    // `BFKAKick` (Konstruktor, `applyState()`, `setFxChain()`), damit Slots,
    // Rollen, Locks, Sample-Pfade und die FX-Kette verlustfrei zurueckkommen.
    KickState state;
    juce::String error;

    if (auto stored = KickState::fromValueTree (extraState, error))
        state = *stored;
    else
        log::w ("captureState: extraState nicht lesbar (" + error + ") - nur Parameter");

    // Parameter sind die Wahrheit im APVTS (Host-Automation, Editor).
    const auto& registry = ParameterRegistry::get();

    for (const auto& d : registry.all())
        if (auto* p = apvts.getRawParameterValue (d.id))
            state.setParam (d.id, p->load());

    return state;
}

void PluginProcessor::applyState (const KickState& state)
{
    const auto& registry = ParameterRegistry::get();
    const auto& all = registry.all();

    for (size_t i = 0; i < all.size(); ++i)
        if (auto* p = apvts.getParameter (all[i].id))
            p->setValueNotifyingHost (state.values()[i]);

    // Der komplette Zustand inklusive `<FxChain>`; danach die Engine bauen.
    extraState = state.toValueTree();

    // Spiegel fuer den Audio-Thread (B-83b): `processBlock()` darf den Zustand
    // nicht lesen, weil `fromValueTree()` allokiert.
    hostFollowEnabled.store (state.bpmMode() == BpmMode::syncBpm,
                             std::memory_order_relaxed);

    // Samples des Zustands in den Renderer (B-154). Ohne diesen Schritt blieben
    // die Slots eines geladenen Presets stumm, obwohl die Karten sie zeigen.
    {
        juce::StringArray warnings;
        assembler.loadFromState (state, contentRoot, renderer, warnings);

        for (const auto& warning : warnings)
            log::d ("applyState: " + warning);
    }

    // Tempo aus dem Zustand an den Renderer: daraus ergibt sich die Kick-Laenge
    // fuer die Shaper-Huellkurve (B-147).
    auto renderSettings = renderer.renderSettings();
    renderSettings.bpm = state.getBpm();
    renderer.setRenderSettings (renderSettings);

    setFxChain (state.fxChain());

    updateModuleRenderLength();
}

void PluginProcessor::rebuildEngineChain()
{
    currentChain = {};

    juce::String error;

    if (auto stored = KickState::fromValueTree (extraState, error))
        currentChain = stored->fxChain();
    else
        log::w ("rebuildEngineChain: Zustand nicht lesbar (" + error + ")");

    fxChain.setChain (currentChain);

    // Nach jedem Rebuild neu melden: Modullatenzen koennen sich mit der
    // Sample-Rate (Multiband-FIR) oder dem Modus (Convolver) aendern.
    setLatencySamples (fxChain.totalLatencySamples());

    // Neue Modulinstanzen kennen die Kick-Laenge noch nicht (B-147).
    updateModuleRenderLength();
}

void PluginProcessor::setFxChain (const FxChain& chain)
{
    currentChain = chain;
    storeFxChainInExtraState();

    // Vor `prepareToPlay()` gibt es noch keine Engine; `prepareToPlay()` baut
    // die Kette dann aus dem Zustand.
    if (fxChain.isPrepared())
    {
        fxChain.setChain (chain);
        setLatencySamples (fxChain.totalLatencySamples());
        updateModuleRenderLength();
    }
}

void PluginProcessor::storeFxChainInExtraState()
{
    const juce::Identifier fxType ("FxChain");
    const juce::Identifier rootType ("BFKAKick");

    if (! extraState.hasType (rootType))
    {
        // Kein lesbarer Gesamt-Zustand: aus einem frischen KickState aufbauen,
        // damit Slots/Parameter/Struktur nicht verloren gehen.
        KickState fresh;
        const auto& registry = ParameterRegistry::get();

        for (const auto& d : registry.all())
            if (auto* p = apvts.getRawParameterValue (d.id))
                fresh.setParam (d.id, p->load());

        extraState = fresh.toValueTree();
    }

    for (int i = extraState.getNumChildren(); --i >= 0;)
        if (extraState.getChild (i).hasType (fxType))
            extraState.removeChild (i, nullptr);

    extraState.appendChild (fxChainToValueTree (currentChain), nullptr);
}

FxModuleState* PluginProcessor::moduleStateFor (int slot) noexcept
{
    if (! juce::isPositiveAndBelow (slot, (int) currentChain.modules.size()))
        return nullptr;

    return &currentChain.modules[(size_t) slot];
}

void PluginProcessor::setModuleParam (int slot, int index, float engineeringValue)
{
    auto* moduleState = moduleStateFor (slot);
    auto* module      = fxChain.moduleAt (slot);

    if (moduleState == nullptr || module == nullptr)
        return;

    const auto& descriptors = module->params();

    if (! juce::isPositiveAndBelow (index, (int) descriptors.size()))
        return;

    const auto& desc     = descriptors[(size_t) index];
    const auto  clamped  = juce::jlimit (desc.minValue, desc.maxValue, engineeringValue);

    // Sofort hoerbar: atomar im Modul, kein Rebuild, kein Klick.
    module->setParam (index, clamped);

    // Gleichzeitig in den Zustand, damit der Wert im Preset landet.
    if (! moduleState->data.isValid())
        moduleState->data = juce::ValueTree (juce::Identifier ("Module"));

    moduleState->data.setProperty (juce::Identifier (desc.id), juce::var ((double) clamped), nullptr);

    storeFxChainInExtraState();
}

float PluginProcessor::getModuleParam (int slot, int index) const
{
    const auto* module = getModule (slot);

    return module != nullptr ? module->getParam (index) : 0.0f;
}

void PluginProcessor::setModuleAttribute (int slot, const juce::String& id, double engineeringValue)
{
    auto* moduleState = moduleStateFor (slot);

    if (moduleState == nullptr || id.isEmpty())
        return;

    if (! moduleState->data.isValid())
        moduleState->data = juce::ValueTree (juce::Identifier ("Module"));

    moduleState->data.setProperty (juce::Identifier (id), juce::var (engineeringValue), nullptr);

    // Ist die Kennung ein echter Parameter, soll die Aenderung auch klingen
    // (die Attribute sind die Anzeige-Einheiten derselben Werte).
    if (auto* module = fxChain.moduleAt (slot))
    {
        const auto& descriptors = module->params();

        for (size_t i = 0; i < descriptors.size(); ++i)
        {
            if (descriptors[i].id != id)
                continue;

            module->setParam ((int) i, juce::jlimit (descriptors[i].minValue,
                                                     descriptors[i].maxValue,
                                                     (float) engineeringValue));
            break;
        }
    }

    storeFxChainInExtraState();
}

void PluginProcessor::setModuleMix (int slot, float mix01)
{
    auto* moduleState = moduleStateFor (slot);

    if (moduleState == nullptr)
        return;

    const auto value = juce::jlimit (0.0f, 1.0f, mix01);

    fxChain.setModuleMix (slot, value);
    moduleState->mix = value;

    storeFxChainInExtraState();
}

void PluginProcessor::setModuleOutGain (int slot, float gain)
{
    auto* moduleState = moduleStateFor (slot);

    if (moduleState == nullptr)
        return;

    const auto value = juce::jlimit (0.0f, 2.0f, gain);

    fxChain.setModuleOutGain (slot, value);
    moduleState->outGain = value;

    storeFxChainInExtraState();
}

void PluginProcessor::setModuleEnabled (int slot, bool shouldBeEnabled)
{
    auto* moduleState = moduleStateFor (slot);

    if (moduleState == nullptr)
        return;

    fxChain.setModuleEnabled (slot, shouldBeEnabled);
    moduleState->enabled = shouldBeEnabled;

    storeFxChainInExtraState();
}

void PluginProcessor::writeBackModuleStates()
{
    // Struktur-Kinder, die ein Modul selbst besitzt. Sie werden **immer**
    // entfernt und danach neu geschrieben: liefert `getState()` bei leerer
    // Struktur (z. B. alle Baender geloescht) gar kein `<Band>` mehr, bliebe
    // sonst die alte Liste im Zustand und im Preset stehen.
    static const juce::Identifier structureNames[]
    {
        juce::Identifier ("Band"),   // eq, mseq
        juce::Identifier ("WNode"),  // dist
        juce::Identifier ("Pt"),     // shaper
        juce::Identifier ("Eq"),     // reverb: EQ-Kurve
        juce::Identifier ("Dc")      // reverb: Decay-Kurve
    };

    const auto isStructureChild = [] (const juce::ValueTree& child)
    {
        for (const auto& name : structureNames)
            if (child.hasType (name))
                return true;

        return false;
    };

    const auto& modules = fxChain.modules();

    for (size_t i = 0; i < modules.size() && i < currentChain.modules.size(); ++i)
    {
        const auto* module = modules[i].get();

        if (module == nullptr)
            continue;

        // Der Struktur-Zustand des Moduls (Bänder, Kurven, IR-Pfad).
        const auto state = module->getState();

        if (! state.isValid())
            continue;

        auto& data = currentChain.modules[i].data;

        if (! data.isValid())
            data = juce::ValueTree (juce::Identifier ("Module"));

        // Kinder, die das Modul selbst liefert, ersetzen; die skalaren
        // Attribute bleiben.
        for (int c = data.getNumChildren(); --c >= 0;)
            if (isStructureChild (data.getChild (c)))
                data.removeChild (c, nullptr);

        for (int c = 0; c < state.getNumChildren(); ++c)
            data.appendChild (state.getChild (c).createCopy(), nullptr);

        // Der IR-Pfad steht beim Convolver als String-Property im Zustand.
        const auto ir = state.getProperty (juce::Identifier ("ir"));

        if (ir.isString())
            data.setProperty (juce::Identifier ("ir"), ir, nullptr);
    }

    storeFxChainInExtraState();
}

// ---------------------------------------------------------------------------
// Presets (B-93) und A/B-Vergleich (B-94)
// ---------------------------------------------------------------------------
bool PluginProcessor::savePreset (const juce::File& file, const PresetInfo& info,
                                  juce::String& error) const
{
    // Die Struktur der Module steht erst nach dem Rueckschreiben im Zustand;
    // die UI ruft es nach jeder Struktur-Aenderung, hier nur zur Sicherheit
    // beim Speichern eines frisch veraenderten Moduls.
    return PresetIO::saveKickPreset (file, captureState(), info, error);
}

bool PluginProcessor::loadPreset (const juce::File& file, juce::String& error)
{
    auto data = PresetIO::loadKickPreset (file, error);

    if (! data.has_value())
        return false;

    // Parameter + Kette uebernehmen; der Rebuild der Engine darf nicht mit dem
    // Audio-Thread kollidieren.
    suspendProcessing (true);
    applyState (data->state);
    suspendProcessing (false);

    lastLoadReport = "Preset geladen: " + file.getFileName();
    log::i (lastLoadReport);
    return true;
}

std::optional<PresetInfo> PluginProcessor::readPresetInfo (const juce::File& file, juce::String& error)
{
    auto tree = juce::XmlDocument::parse (file);

    if (tree == nullptr)
    {
        error = "Datei ist kein gueltiges XML: " + file.getFileName();
        return std::nullopt;
    }

    auto data = PresetIO::fromValueTree (juce::ValueTree::fromXml (*tree), error);

    if (! data.has_value())
        return std::nullopt;

    return data->info;
}

bool PluginProcessor::saveModulePreset (int slot, const juce::File& file,
                                        const PresetInfo& info, juce::String& error) const
{
    if (! juce::isPositiveAndBelow (slot, (int) currentChain.modules.size()))
    {
        error = "Kein Modul an Kettenplatz " + juce::String (slot) + ".";
        return false;
    }

    auto header = info;

    if (header.name.isEmpty())
        header.name = file.getFileNameWithoutExtension();

    return PresetIO::saveModulePreset (file, currentChain.modules[(size_t) slot], header, error);
}

bool PluginProcessor::loadModulePreset (int slot, const juce::File& file, juce::String& error)
{
    if (! juce::isPositiveAndBelow (slot, (int) currentChain.modules.size()))
    {
        error = "Kein Modul an Kettenplatz " + juce::String (slot) + ".";
        return false;
    }

    auto data = PresetIO::loadModulePreset (file, error);

    if (! data.has_value())
        return false;

    auto loaded = data->module;
    const auto& current = currentChain.modules[(size_t) slot];

    // Kettenplatz-Werte bleiben: ein Modul-Preset beschreibt das Modul, nicht
    // seinen Pegel in der Kette.
    loaded.enabled = current.enabled;
    loaded.mix     = current.mix;
    loaded.outGain = current.outGain;

    currentChain.modules[(size_t) slot] = std::move (loaded);

    suspendProcessing (true);
    setFxChain (currentChain);
    suspendProcessing (false);

    lastLoadReport = "Modul-Preset geladen: " + file.getFileName();
    log::i (lastLoadReport);
    return true;
}

void PluginProcessor::storeComparisonState (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumComparisonSlots))
        return;

    comparisonStates[(size_t) index] = captureState();
    activeComparison.store (index);
}

bool PluginProcessor::restoreComparisonState (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumComparisonSlots))
        return false;

    const auto& stored = comparisonStates[(size_t) index];

    if (! stored.has_value())
        return false;

    suspendProcessing (true);
    applyState (*stored);
    suspendProcessing (false);

    activeComparison.store (index);
    return true;
}

bool PluginProcessor::hasComparisonState (int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, kNumComparisonSlots))
        return false;

    return comparisonStates[(size_t) index].has_value();
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData){
    auto state = captureState();
    auto tree = state.toValueTree();

    // Aktuelle Parameterwerte einsetzen (KickState haelt sie bereits).
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    juce::String error;

    if (auto state = KickState::fromValueTree (tree, error))
    {
        applyState (*state);
    }
    else
    {
        log::w ("Zustand konnte nicht gelesen werden: " + error);
    }
}

} // namespace bfka

// ---------------------------------------------------------------------------
// JUCE-Plugin-Einstiegspunkt
// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new bfka::PluginProcessor();
}
