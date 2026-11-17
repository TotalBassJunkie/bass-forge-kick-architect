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
    extraState = juce::ValueTree ("BFKAExtra");
    extraState.setProperty ("schemaVersion", 1, nullptr);

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
    KickState state;
    const auto& registry = ParameterRegistry::get();

    for (const auto& d : registry.all())
        if (auto* p = apvts.getRawParameterValue (d.id))
            state.setParam (d.id, p->load());

    // Slots/FX/Meta aus extraState
    juce::String error;
    if (auto restored = KickState::fromValueTree (extraState, error))
    {
        for (int i = 0; i < KickState::kNumSlots; ++i)
        {
            state.setSample (i, restored->getSample (i));
            state.setRole (i, restored->getRole (i));
            state.setLocked (i, restored->isLocked (i));
            state.setKeyTracked (i, restored->isKeyTracked (i));
        }

        state.fxChain()   = restored->fxChain();
        state.align()     = restored->align();
        state.setStyleId (restored->getStyleId());
        state.setPresetName (restored->getPresetName());
    }

    return state;
}

void PluginProcessor::applyState (const KickState& state)
{
    const auto& registry = ParameterRegistry::get();
    const auto& all = registry.all();

    for (size_t i = 0; i < all.size(); ++i)
        if (auto* p = apvts.getParameter (all[i].id))
            p->setValueNotifyingHost (state.values()[i]);

    juce::ValueTree extra ("BFKAExtra");
    extra.setProperty ("schemaVersion", 1, nullptr);

    juce::ValueTree slots ("Slots");
    for (int i = 0; i < KickState::kNumSlots; ++i)
    {
        juce::ValueTree s ("Slot");
        s.setProperty ("index", i, nullptr);
        s.setProperty ("role", (int) state.getRole (i), nullptr);
        s.setProperty ("locked", state.isLocked (i), nullptr);
        s.setProperty ("keyTracked", state.isKeyTracked (i), nullptr);

        if (const auto& sample = state.getSample (i); sample.has_value())
        {
            s.setProperty ("relPath", sample->relPath, nullptr);
            s.setProperty ("absPath", sample->absPath, nullptr);
            s.setProperty ("fileName", sample->fileName, nullptr);
            s.setProperty ("roleId", sample->roleId, nullptr);
        }

        slots.appendChild (s, nullptr);
    }
    extra.appendChild (slots, nullptr);

    extraState = extra;
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
