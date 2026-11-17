#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <memory>

#include "fx/FxChainProcessor.h"
#include "fx/modules/DynamicEqModule.h"
#include "plugin/PluginProcessor.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Externer Sidechain (Ticket B-146): Detektor-Modul, Ketten-Engine und
// Bus-Layout des Prozessors.
namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    int paramIndex (const juce::String& id)
    {
        DynamicEqModule module;

        const auto& params = module.params();

        for (size_t i = 0; i < params.size(); ++i)
            if (params[i].id == id)
                return (int) i;

        return -1;
    }

    void setParamById (DynamicEqModule& module, const juce::String& id, float value)
    {
        const int index = paramIndex (id);

        if (index >= 0)
            module.setParam (index, value);
    }

    FxModuleState makeModule (const juce::String& type)
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = true;
        state.mix     = 1.0f;
        state.outGain = 1.0f;
        return state;
    }

    void setAttribute (FxModuleState& state, const juce::String& id, double value)
    {
        if (! state.data.isValid())
            state.data = juce::ValueTree (juce::Identifier ("Module"));

        state.data.setProperty (juce::Identifier (id), juce::var (value), nullptr);
    }

    /** Ein Band mit eigener Dynamik, sonst neutral. */
    FxModuleState makeDynEqState()
    {
        auto state = makeModule ("dynEq");

        setAttribute (state, "autoGain", 0.0);
        setAttribute (state, "delta",    0.0);
        setAttribute (state, "dryMix",   0.0);
        setAttribute (state, "channels", 1.0);
        setAttribute (state, "outGain",  0.0);
        setAttribute (state, "wideDyn",  0.0);
        setAttribute (state, "hpActive", 0.0);
        setAttribute (state, "lpActive", 0.0);

        // Band 2: Bell, mit eigener Dynamik (SPLIT), deutliche Absenkung.
        setAttribute (state, "b2_active",  1.0);
        setAttribute (state, "b2_type",    0.0);
        setAttribute (state, "b2_freq",    1000.0);
        setAttribute (state, "b2_gain",    -12.0);
        setAttribute (state, "b2_q",       1.4);
        setAttribute (state, "b2_dyn",     1.0);
        setAttribute (state, "b2_split",   1.0);
        setAttribute (state, "b2_thresh",  -30.0);
        setAttribute (state, "b2_ratio",   8.0);

        // Externer Sidechain (1 = Ext SC).
        setAttribute (state, "sidechain", 1.0);
        return state;
    }

    void fillConstant (juce::AudioBuffer<float>& buffer, float value, int numSamples)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int s = 0; s < numSamples; ++s)
                buffer.setSample (ch, s, value);
    }

    /** Sinus im Band von Band 2 (1 kHz) — das Bandpass-Detektor sieht Energie. */
    void fillSine (juce::AudioBuffer<float>& buffer, double freqHz, double amplitude, int numSamples)
    {
        constexpr double kPi = 3.14159265358979323846;
        const double increment = 2.0 * kPi * freqHz / kSampleRate;
        double phase = 0.0;

        for (int s = 0; s < numSamples; ++s)
        {
            const auto value = (float) (amplitude * std::sin (phase));
            phase += increment;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, s, value);

            if (phase > 2.0 * kPi)
                phase -= 2.0 * kPi;
        }
    }
} // namespace

// =========================================================================
// Modul: Detektorquelle
// =========================================================================
TEST_CASE ("dynEq: externer Sidechain steuert den Detektor", "[fx][sidechain]")
{
    DynamicEqModule module;
    module.prepare (kSampleRate, kBlockSize);
    setParamById (module, "autoGain", 0.0f);
    setParamById (module, "channels", 1.0f);
    setParamById (module, "sidechain", 1.0f);

    setParamById (module, "b2_active", 1.0f);
    setParamById (module, "b2_type",   0.0f);
    setParamById (module, "b2_freq",   1000.0f);
    setParamById (module, "b2_gain",  -12.0f);
    setParamById (module, "b2_q",      1.4f);
    setParamById (module, "b2_dyn",    1.0f);
    setParamById (module, "b2_split",  1.0f);
    setParamById (module, "b2_thresh",-30.0f);
    setParamById (module, "b2_ratio",  8.0f);

    // Nutzsignal: konstante, leise Gleichspannung (kein Detektor-Pegel).
    // Detektor: lautes Signal -> das Band muss deutlich abregeln.
    juce::AudioBuffer<float> audio (2, kBlockSize);
    juce::AudioBuffer<float> sidechain (2, kBlockSize);

    const auto render = [&] (bool withSidechain)
    {
        float worstGain = 0.0f;

        for (int block = 0; block < 40; ++block)
        {
            fillConstant (audio, 0.01f, kBlockSize);
            fillSine (sidechain, 1000.0, 0.5, kBlockSize);

            module.setSidechainBuffer (withSidechain ? &sidechain : nullptr);
            module.process (audio);

            worstGain = juce::jmin (worstGain, module.bandDynamicGainDb (1));
        }

        return worstGain;
    };

    const auto internalGain = render (false);
    REQUIRE (module.isUsingExternalSidechain() == false);

    const auto externalGain = render (true);

    CHECK (module.isUsingExternalSidechain());
    CHECK (module.sidechainMode() == 1);
    // Ohne externes Signal bleibt die Dynamik aus (interner Detektor ist
    // leise, das Band statisch); mit Sidechain regelt das Band deutlich ab.
    CHECK (externalGain < internalGain - 3.0f);

    // Der Zeiger gilt nur fuer einen Block.
    fillConstant (audio, 0.01f, kBlockSize);
    module.process (audio);
    CHECK_FALSE (module.isUsingExternalSidechain());
}

TEST_CASE ("dynEq: Ext SC ohne Bus faellt auf intern flach zurueck", "[fx][sidechain]")
{
    DynamicEqModule module;
    module.prepare (kSampleRate, kBlockSize);
    setParamById (module, "sidechain", 1.0f);
    setParamById (module, "b2_active", 1.0f);
    setParamById (module, "b2_dyn", 1.0f);
    setParamById (module, "b2_thresh", -12.0f);

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    // Parameter werden mit 20 ms geglaettet: 12 Bloecke à 128 Samples sind
    // rund 32 ms, danach steht der Zielwert.
    for (int block = 0; block < 12; ++block)
    {
        fillConstant (buffer, 0.25f, kBlockSize);
        module.process (buffer);
    }

    // Ohne zweiten Eingang darf der Modus nicht "extern" melden; er arbeitet
    // als "Int SC flat" (Modus 2) weiter.
    CHECK_FALSE (module.isUsingExternalSidechain());
    CHECK (module.sidechainMode() == 2);

    // Und "Int SC" (0) filtert den Detektor weiterhin bei 60 Hz.
    setParamById (module, "sidechain", 0.0f);

    for (int block = 0; block < 12; ++block)
    {
        fillConstant (buffer, 0.25f, kBlockSize);
        module.process (buffer);
    }

    CHECK (module.sidechainMode() == 0);
}

// =========================================================================
// Kette: Weitergabe und Latenzausgleich
// =========================================================================
TEST_CASE ("Sidechain: Ketten-Engine reicht den Detektor weiter", "[fx][sidechain]")
{
    FxChain chain;
    chain.modules.push_back (makeDynEqState());

    FxChainProcessor processor;
    processor.prepare (kSampleRate, kBlockSize);
    processor.setChain (chain);

    auto* module = dynamic_cast<DynamicEqModule*> (processor.moduleAt (0));
    REQUIRE (module != nullptr);
    REQUIRE (module->acceptsExternalSidechain());

    juce::AudioBuffer<float> audio (2, kBlockSize);
    juce::AudioBuffer<float> sidechain (2, kBlockSize);
    fillConstant (audio, 0.01f, kBlockSize);
    fillSine (sidechain, 1000.0, 0.5, kBlockSize);

    // Die Parameter werden mit 20 ms geglaettet; erst danach steht der Modus.
    for (int block = 0; block < 12; ++block)
    {
        fillConstant (audio, 0.01f, kBlockSize);
        processor.process (audio, &sidechain);
    }

    CHECK (module->isUsingExternalSidechain());

    // Ohne Sidechain argument: interner Detektor.
    for (int block = 0; block < 12; ++block)
    {
        fillConstant (audio, 0.01f, kBlockSize);
        processor.process (audio);
    }

    CHECK_FALSE (module->isUsingExternalSidechain());
}

TEST_CASE ("Sidechain: Latenz der Vormodule wird ausgeglichen", "[fx][sidechain]")
{
    FxChain chain;

    auto multiband = makeModule ("ott");
    setAttribute (multiband, "linear", 1.0);     // 256 Samples Latenz
    chain.modules.push_back (std::move (multiband));
    chain.modules.push_back (makeDynEqState());

    FxChainProcessor processor;
    processor.prepare (kSampleRate, kBlockSize);
    processor.setChain (chain);

    CHECK (processor.totalLatencySamples() == 256);
    CHECK (processor.sidechainLatencyFor (0) == 0);
    CHECK (processor.sidechainLatencyFor (1) == 256);
    CHECK (processor.sidechainLatencyFor (2) == 0);

    juce::AudioBuffer<float> audio (2, kBlockSize);
    juce::AudioBuffer<float> sidechain (2, kBlockSize);
    fillConstant (audio, 0.01f, kBlockSize);
    fillSine (sidechain, 1000.0, 0.5, kBlockSize);

    auto* module = dynamic_cast<DynamicEqModule*> (processor.moduleAt (1));
    REQUIRE (module != nullptr);

    for (int block = 0; block < 12; ++block)
    {
        fillConstant (audio, 0.01f, kBlockSize);
        processor.process (audio, &sidechain);
    }

    CHECK (module->isUsingExternalSidechain());
}

// =========================================================================
// Prozessor: Bus-Layout
// =========================================================================
TEST_CASE ("Sidechain: Prozessor-Layout mit zweitem Eingang", "[fx][sidechain]")
{
    JuceGuiScope guiScope;

    PluginProcessor processor;

    // Standard: beide Detektor-Busse sind aus.
    CHECK (processor.getSidechainChannels() == 0);
    CHECK_FALSE (processor.isSidechainActive());

    // `Input` (Haupt-Eingang) und `Sidechain` (Aux) aktiv, Stereo-Ausgang.
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());

    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (kSampleRate, kBlockSize);

    // Puffer: 2 Kanaele Haupt-Eingang/Ausgang, danach 2 Kanaele Aux-Sidechain.
    juce::AudioBuffer<float> buffer (4, kBlockSize);
    fillConstant (buffer, 0.0f, kBlockSize);

    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < kBlockSize; ++s)
            buffer.setSample (ch, s, 0.25f);          // Haupt-Eingang

    for (int ch = 2; ch < 4; ++ch)
        for (int s = 0; s < kBlockSize; ++s)
            buffer.setSample (ch, s, 0.5f);           // Sidechain-Aux-Bus

    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    CHECK (processor.isSidechainActive());
    CHECK (processor.getSidechainChannels() == 2);

    // Der Aux-Bus bleibt unangetastet (nur lesen).
    CHECK_THAT (buffer.getSample (2, 0), WithinAbs (0.5f, 1.0e-6f));
    CHECK_THAT (buffer.getSample (3, kBlockSize - 1), WithinAbs (0.5f, 1.0e-6f));

    // Der Ausgang ist ein gueltiges Audiosignal (leerer Kick = Stille).
    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < kBlockSize; ++s)
            CHECK (std::isfinite (buffer.getSample (ch, s)));
}

TEST_CASE ("Sidechain: Haupt-Eingang dient als Detektor-Ersatz", "[fx][sidechain]")
{
    JuceGuiScope guiScope;

    PluginProcessor processor;

    // Nur der Haupt-Eingang ist aktiv; er teilt sich die Kanaele mit dem
    // Ausgang, wird also vor dem Leeren als Detektor kopiert.
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.inputBuses.add (juce::AudioChannelSet::disabled());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());

    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillConstant (buffer, 0.3f, kBlockSize);

    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    CHECK (processor.isSidechainActive());
    CHECK (processor.getSidechainChannels() == 2);

    // Der Ausgang ueberschreibt den Eingang (In-Place): leerer Kick = Stille.
    CHECK_THAT (buffer.getSample (0, 0), WithinAbs (0.0f, 1.0e-6f));
}

TEST_CASE ("Sidechain: ungueltige Bus-Layouts werden abgelehnt", "[fx][sidechain]")
{
    JuceGuiScope guiScope;

    PluginProcessor processor;

    juce::AudioProcessor::BusesLayout fiveOne;
    fiveOne.inputBuses.add (juce::AudioChannelSet::create5point1());
    fiveOne.inputBuses.add (juce::AudioChannelSet::disabled());
    fiveOne.outputBuses.add (juce::AudioChannelSet::stereo());

    CHECK_FALSE (processor.checkBusesLayoutSupported (fiveOne));

    juce::AudioProcessor::BusesLayout noOutput;
    noOutput.inputBuses.add (juce::AudioChannelSet::disabled());
    noOutput.inputBuses.add (juce::AudioChannelSet::disabled());
    noOutput.outputBuses.add (juce::AudioChannelSet::disabled());

    CHECK_FALSE (processor.checkBusesLayoutSupported (noOutput));

    // Mono-Ausgang mit ausgeschalteten Eingaengen bleibt erlaubt.
    juce::AudioProcessor::BusesLayout mono;
    mono.inputBuses.add (juce::AudioChannelSet::disabled());
    mono.inputBuses.add (juce::AudioChannelSet::disabled());
    mono.outputBuses.add (juce::AudioChannelSet::mono());

    CHECK (processor.checkBusesLayoutSupported (mono));
}
