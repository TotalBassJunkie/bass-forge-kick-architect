#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "engine/SampleVoice.h"
#include "core/KickState.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ---------------------------------------------------------------------------
// Pitch (B-42) und Stretch (B-42/B-43) in der Sample-Voice
//
// REPITCH: Pitch und Stretch wirken als Resampling auf Leseschritt UND Laenge.
// KEEP PITCH: ein vorgerenderter Puffer enthaelt Pitch/Stretch bereits; die
// Voice wendet beide Faktoren dann nicht erneut an. Ohne Puffer faellt KEEP
// PITCH auf REPITCH zurueck (kein stiller Klang, aber keine Tonhoehenhaltung).
// ---------------------------------------------------------------------------

namespace
{
    /** Stereo-Sinus, beide Kanaele identisch. */
    juce::AudioBuffer<float> makeSine (double seconds, double freqHz, double sampleRate)
    {
        const auto numSamples = juce::jmax (1, (int) std::lround (sampleRate * seconds));

        juce::AudioBuffer<float> buffer (2, numSamples);

        const auto step = 2.0 * 3.14159265358979323846 * freqHz / sampleRate;

        for (int n = 0; n < numSamples; ++n)
        {
            const auto value = (float) std::sin (step * (double) n);

            buffer.setSample (0, n, value);
            buffer.setSample (1, n, value);
        }

        return buffer;
    }
} // namespace

TEST_CASE ("Pitch: +12 Halbtoene halbiert die Laenge (REPITCH)", "[engine][pitch]")
{
    auto buf = makeSine (1.0, 100.0, 48000.0);

    SampleVoice v;
    v.prepare (48000.0, 128);
    v.setSource (&buf, 48000.0);

    VoiceSettings s;
    s.mode           = StretchMode::repitch;
    s.pitchSemitones = 12.0;
    v.setSettings (s);

    REQUIRE_THAT (v.lengthSeconds(), WithinRel (0.5, 0.02));
}

TEST_CASE ("Stretch: Faktor 2 verdoppelt die Laenge (REPITCH)", "[engine][pitch]")
{
    auto buf = makeSine (1.0, 100.0, 48000.0);

    SampleVoice v;
    v.prepare (48000.0, 128);
    v.setSource (&buf, 48000.0);

    VoiceSettings s;
    s.mode           = StretchMode::repitch;
    s.pitchSemitones = 0.0;
    s.stretchFactor  = 2.0;
    v.setSettings (s);

    REQUIRE_THAT (v.lengthSeconds(), WithinRel (2.0, 0.02));
}

TEST_CASE ("KEEP PITCH ohne Puffer faellt auf REPITCH zurueck", "[engine][pitch]")
{
    auto buf = makeSine (1.0, 100.0, 48000.0);

    SampleVoice v;
    v.prepare (48000.0, 128);
    v.setSource (&buf, 48000.0);

    VoiceSettings s;
    s.mode           = StretchMode::keepPitch;
    s.pitchSemitones = 12.0;
    v.setSettings (s);

    // Ohne vorgerenderten Puffer bleibt nur Resampling: Laenge halbiert.
    REQUIRE_THAT (v.lengthSeconds(), WithinRel (0.5, 0.02));
    REQUIRE_FALSE (v.isPitchAccurate());
}

TEST_CASE ("KEEP PITCH mit Puffer haelt Laenge und Tonhoehe", "[engine][pitch]")
{
    auto buf = makeSine (1.0, 100.0, 48000.0);
    auto pre = makeSine (1.0, 100.0, 48000.0);

    SampleVoice v;
    v.prepare (48000.0, 128);
    v.setSource (&buf, 48000.0);
    v.setPreRendered (&pre, 48000.0);

    VoiceSettings s;
    s.mode           = StretchMode::keepPitch;
    s.pitchSemitones = 12.0;
    v.setSettings (s);

    REQUIRE (v.isPitchAccurate());
    REQUIRE_THAT (v.lengthSeconds(), WithinRel (1.0, 0.02));

    // Nulldurchgaenge vor/nach vergleichen: renderNextBlock in einen Puffer,
    // zaehlen. 100 Hz ueber 1 s ergeben rund 200 Nulldurchgaenge.
    juce::AudioBuffer<float> out (2, 48000);
    out.clear();
    v.startAt (0);
    v.renderNextBlock (out, 0, 48000);

    int crossings = 0;

    for (int i = 1; i < out.getNumSamples(); ++i)
        if ((out.getSample (0, i - 1) < 0.0f) != (out.getSample (0, i) < 0.0f))
            ++crossings;

    REQUIRE (crossings >= 190);   // Toleranz nach unten
    REQUIRE (crossings <= 210);
}
