#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"

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

    juce::String error;
    KickState state;

    if (auto stored = KickState::fromValueTree (extraState, error))
        state = *stored;

    state.fxChain() = chain;
    extraState = state.toValueTree();

    // Vor `prepareToPlay()` gibt es noch keine Engine; `prepareToPlay()` baut
    // die Kette dann aus dem Zustand.
    if (fxChain.isPrepared())
    {
        fxChain.setChain (chain);
        setLatencySamples (fxChain.totalLatencySamples());
    }
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
