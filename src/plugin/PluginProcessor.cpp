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

    if (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        return layouts.getMainInputChannelSet().isDisabled();

    return false;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    const auto numSamples = buffer.getNumSamples();
    buffer.clear();

    if (auto* masterParam = apvts.getRawParameterValue ("master_vol"))
        masterGain.store (masterParam->load (std::memory_order_relaxed),
                          std::memory_order_relaxed);

    // 1) Kick rendern (additiv).
    renderer.renderNextBlock (buffer, 0, numSamples);

    // 2) FX-Kette (in-place, Module in Reihenfolge).
    if (fxChain.numModules() > 0)
        fxChain.process (buffer);

    // 3) Master-Fader.
    const auto gain = juce::jlimit (0.0f, 2.0f, masterGain.load (std::memory_order_relaxed));

    if (! juce::approximatelyEqual (gain, 1.0f))
        buffer.applyGain (0, numSamples, gain);

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
