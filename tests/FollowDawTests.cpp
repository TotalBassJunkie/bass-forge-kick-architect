// B-83b: FOLLOW DAW - BPM aus dem Host-Playhead, gedrosselt auf 4/Sekunde.
//
// Geprueft wird nur das, was der Vertrag zusichert: das Host-Tempo landet im
// Renderer, MANUAL laesst den Host links liegen, ungueltige Werte (0 BPM)
// werden ignoriert und ein fehlender Playhead ist kein Fehlerfall.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>

#include "core/KickState.h"
#include "plugin/PluginProcessor.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

    /** JUCE braucht fuer `PluginProcessor` eine GUI-Initialisierung. */
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Minimaler Playhead: liefert nur ein konstantes Tempo. */
    struct FakePlayHead final : juce::AudioPlayHead
    {
        double bpm = 128.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setIsPlaying (true);
            return info;
        }
    };

    /** Setzt den Zustand auf FOLLOW DAW und haengt den Playhead ein. */
    void prepareFollowMode (PluginProcessor& proc, FakePlayHead& playHead)
    {
        proc.prepareToPlay (kSampleRate, kBlockSize);

        KickState state = proc.captureState();
        state.bpmMode() = BpmMode::syncBpm;
        proc.applyState (state);

        proc.setPlayHead (&playHead);
    }
} // namespace

TEST_CASE ("FOLLOW DAW: Host-BPM wird uebernommen", "[plugin][bpm]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);

    KickState st = proc.captureState();
    st.bpmMode() = BpmMode::syncBpm;
    proc.applyState (st);

    FakePlayHead ph;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int i = 0; i < 4; ++i)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);
    }

    REQUIRE_THAT (proc.hostBpm(), WithinAbs (128.0, 1e-9));
    REQUIRE (proc.hostBpmApplyCount() >= 1);
    REQUIRE_THAT (proc.getBpm(), WithinAbs (128.0, 0.5));
}

TEST_CASE ("FOLLOW DAW: Drosselung auf 4 pro Sekunde", "[plugin][bpm]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    FakePlayHead ph;
    prepareFollowMode (proc, ph);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    // 100 Bloecke a 128 Samples = 0,27 s bei 48 kHz: hoechstens zwei Uebernahmen.
    for (int i = 0; i < 100; ++i)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);
    }

    REQUIRE (proc.hostBpmApplyCount() <= 2);
}

TEST_CASE ("FOLLOW DAW: MANUAL ignoriert den Host", "[plugin][bpm]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);

    KickState st = proc.captureState();
    st.bpmMode() = BpmMode::native;
    st.setBpm (140.0);
    proc.applyState (st);

    FakePlayHead ph;
    proc.setPlayHead (&ph);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int i = 0; i < 4; ++i)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);
    }

    REQUIRE_THAT (proc.getBpm(), WithinAbs (140.0, 0.5));
    REQUIRE (proc.hostBpmApplyCount() == 0);
}

TEST_CASE ("FOLLOW DAW: ungueltiges Host-BPM wird ignoriert", "[plugin][bpm]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    FakePlayHead ph;
    prepareFollowMode (proc, ph);

    ph.bpm = 0.0;

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    for (int i = 0; i < 4; ++i)
    {
        buffer.clear();
        proc.processBlock (buffer, midi);
    }

    REQUIRE (proc.hostBpmApplyCount() == 0);
}

TEST_CASE ("FOLLOW DAW: ohne Playhead kein Absturz", "[plugin][bpm]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    FakePlayHead ph;
    prepareFollowMode (proc, ph);

    proc.setPlayHead (nullptr);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    buffer.clear();
    juce::MidiBuffer midi;

    REQUIRE_NOTHROW (proc.processBlock (buffer, midi));
    REQUIRE (proc.hostBpmApplyCount() == 0);
}
