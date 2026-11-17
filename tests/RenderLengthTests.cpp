#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>

#include "engine/KickRenderer.h"
#include "fx/modules/FinalizerModule.h"
#include "fx/modules/KickShaperModule.h"
#include "plugin/PluginProcessor.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Kick-Laenge fuer `shaper` und die gemeldete Finalizer-Latenz (B-147).
namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Sinus-Sample mit `seconds` Dauer. */
    juce::AudioBuffer<float> makeSample (double seconds)
    {
        const int samples = (int) std::lround (seconds * kSampleRate);
        juce::AudioBuffer<float> buffer (2, samples);

        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < samples; ++s)
                buffer.setSample (ch, s, (float) (0.5 * std::sin (2.0 * 3.14159265358979 * 60.0
                                                                  * (double) s / kSampleRate)));

        return buffer;
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
} // namespace

TEST_CASE ("Renderer: Kick-Laenge folgt BPM und KICK LENGTH", "[render][shaper]")
{
    KickRenderer renderer;
    renderer.prepare (kSampleRate, kBlockSize);

    RenderSettings settings;
    settings.bpm = 150.0;
    settings.cutToOneBeat = true;
    renderer.setRenderSettings (settings);

    CHECK_THAT (renderer.kickLengthSeconds(), WithinAbs (0.4, 1.0e-9));

    // KICK LENGTH = FULL SAMPLE: die laengste Voice bestimmt die Laenge.
    auto source = makeSample (1.25);
    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    settings.cutToOneBeat = false;
    renderer.setRenderSettings (settings);

    CHECK_THAT (renderer.kickLengthSeconds(), WithinAbs (1.25, 1.0e-9));

    // Ohne belegten Slot faellt die Laenge auf den Beat zurueck.
    renderer.clearSlot (0);
    CHECK_THAT (renderer.kickLengthSeconds(), WithinAbs (0.4, 1.0e-9));
}

TEST_CASE ("Renderer: Kick-Laenge haengt am Tempo", "[render][shaper]")
{
    KickRenderer renderer;
    renderer.prepare (kSampleRate, kBlockSize);

    RenderSettings settings;
    settings.cutToOneBeat = true;

    settings.bpm = 120.0;
    renderer.setRenderSettings (settings);
    CHECK_THAT (renderer.kickLengthSeconds(), WithinAbs (0.5, 1.0e-9));

    settings.bpm = 160.0;
    renderer.setRenderSettings (settings);
    CHECK_THAT (renderer.kickLengthSeconds(), WithinAbs (0.375, 1.0e-9));
}

TEST_CASE ("Prozessor: shaper bekommt die Kick-Laenge", "[render][shaper]")
{
    JuceGuiScope guiScope;

    PluginProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);

    KickState state;
    state.setBpm (150.0);
    state.fxChain().modules.push_back (makeModule ("shaper"));

    proc.applyState (state);

    auto* shaper = dynamic_cast<KickShaperModule*> (proc.getModule (0));
    REQUIRE (shaper != nullptr);
    CHECK_THAT (shaper->renderLengthSeconds(), WithinAbs (0.4, 1.0e-6));

    // Anderes Tempo, neuer Zustand: die Laenge zieht nach.
    state.setBpm (120.0);
    proc.applyState (state);

    shaper = dynamic_cast<KickShaperModule*> (proc.getModule (0));
    REQUIRE (shaper != nullptr);
    CHECK_THAT (shaper->renderLengthSeconds(), WithinAbs (0.5, 1.0e-6));

    // Kette ohne shaper bleibt unveraendert.
    state.fxChain().modules.clear();
    state.fxChain().modules.push_back (makeModule ("final"));
    proc.applyState (state);
    CHECK (proc.getEngineModuleCount() == 1);
    CHECK (proc.getModule (0) != nullptr);
}

TEST_CASE ("Prozessor: Finalizer-Latenz wird an den Host gemeldet", "[render][final]")
{
    JuceGuiScope guiScope;

    PluginProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);

    CHECK (proc.getLatencySamples() == 0);

    KickState state;
    state.fxChain().modules.push_back (makeModule ("final"));
    proc.applyState (state);

    const auto expected = FinalizerModule::lookaheadSamplesFor (kSampleRate);

    CHECK (expected == 72);
    CHECK (proc.getLatencySamples() == expected);

    auto* module = dynamic_cast<FinalizerModule*> (proc.getModule (0));
    REQUIRE (module != nullptr);
    CHECK (module->latencySamples() == expected);

    // Zwei Finalizer in Reihe verdoppeln die gemeldete Latenz.
    state.fxChain().modules.push_back (makeModule ("final"));
    proc.applyState (state);
    CHECK (proc.getLatencySamples() == 2 * expected);

    // Ohne Modul wieder 0.
    state.fxChain().modules.clear();
    proc.applyState (state);
    CHECK (proc.getLatencySamples() == 0);
}
