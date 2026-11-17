#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"

#include "core/FxChainIO.h"
#include "core/ParameterRegistry.h"
#include "core/Version.h"
#include "core/Log.h"
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
}

void PluginProcessor::triggerKick()
{
    renderer.noteOn();
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

    juce::StringArray warnings;
    const auto ok = assembler.buildDemoKick (contentRoot, renderer, warnings);

    lastLoadReport = ok ? ("Demo-Kick aus " + contentRoot.getFileName())
                        : "Demo-Kick konnte nicht gebaut werden.";

    if (! warnings.isEmpty())
        lastLoadReport << " | " << warnings.joinIntoString ("; ");

    log::i (lastLoadReport);

    if (ok)
        renderer.noteOn();

    return ok;
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

    setFxChain (state.fxChain());
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

        // Kinder, die das Modul selbst liefert, ersetzen; alles andere (die
        // skalaren Attribute) bleibt.
        for (int c = data.getNumChildren(); --c >= 0;)
        {
            const auto child = data.getChild (c);

            if (state.getChildWithName (child.getType()).isValid())
                data.removeChild (c, nullptr);
        }

        for (int c = 0; c < state.getNumChildren(); ++c)
            data.appendChild (state.getChild (c).createCopy(), nullptr);

        // Der IR-Pfad steht beim Convolver als String-Property im Zustand.
        const auto ir = state.getProperty (juce::Identifier ("ir"));

        if (ir.isString())
            data.setProperty (juce::Identifier ("ir"), ir, nullptr);
    }

    storeFxChainInExtraState();
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
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
