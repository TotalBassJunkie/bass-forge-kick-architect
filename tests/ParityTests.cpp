// B-130: Tests fuer den Parity-Harness.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/ParityHarness.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    /** Dirac bei 0 (Amplitude 1), Rest 0; mono. */
    juce::AudioBuffer<float> makeImpulse (int samples, double rate)
    {
        juce::AudioBuffer<float> buffer (1, samples);
        buffer.clear();

        if (samples > 0)
            buffer.setSample (0, 0, 1.0f);

        juce::ignoreUnused (rate);
        return buffer;
    }

    /** Sinus mit `hz`, `samples` Laenge, Amplitude `amp`; mono. */
    juce::AudioBuffer<float> makeSine (double hz, int samples, double rate, float amp)
    {
        juce::AudioBuffer<float> buffer (1, samples);

        for (int s = 0; s < samples; ++s)
            buffer.setSample (0, s, amp * (float) std::sin (2.0 * kPi * hz * (double) s / rate));

        return buffer;
    }
} // namespace

TEST_CASE ("Parity: Stille", "[engine][parity]")
{
    constexpr double rate = 48000.0;

    juce::AudioBuffer<float> silence (1, (int) rate);
    silence.clear();

    const auto m = ParityHarness::measure (silence, rate);

    REQUIRE (m.lengthSeconds == 1.0);
    REQUIRE (m.rmsDb == -100.0);
    REQUIRE_FALSE (m.clipped);
}

TEST_CASE ("Parity: Sinus misst RMS und Crest", "[engine][parity]")
{
    constexpr double rate = 48000.0;

    const auto sine = makeSine (1000.0, (int) rate, rate, 0.5f);
    const auto m    = ParityHarness::measure (sine, rate);

    REQUIRE_THAT (m.peakAmplitude, WithinAbs (0.5, 0.02));
    REQUIRE_THAT (m.rmsDb, WithinAbs (-9.03, 0.5));
    REQUIRE_THAT (m.crestFactor, WithinAbs (1.414, 0.1));
    REQUIRE_FALSE (m.clipped);
}

TEST_CASE ("Parity: Transient liegt am Anfang", "[engine][parity]")
{
    constexpr double rate = 48000.0;

    const auto impulse = makeImpulse ((int) rate, rate);
    const auto m       = ParityHarness::measure (impulse, rate);

    REQUIRE (m.transientSeconds < 0.001);
    REQUIRE (m.crestFactor > 100.0);
    REQUIRE (m.clipped);
}

TEST_CASE ("Parity: Toleranz acceptiert gleiche Messung", "[engine][parity]")
{
    constexpr double rate = 48000.0;

    const auto sine = makeSine (200.0, (int) (0.5 * rate), rate, 0.7f);
    const auto m    = ParityHarness::measure (sine, rate);

    REQUIRE (ParityHarness::withinTolerance (m, m, 0.05));
}

TEST_CASE ("Parity: Toleranz lehnt fremde Laenge ab", "[engine][parity]")
{
    constexpr double rate = 48000.0;

    const auto shortSine = makeSine (200.0, (int) (0.5 * rate), rate, 0.7f);
    const auto longSine  = makeSine (200.0, (int) (rate),       rate, 0.7f);

    const auto a = ParityHarness::measure (shortSine, rate);
    const auto b = ParityHarness::measure (longSine, rate);

    REQUIRE_FALSE (ParityHarness::withinTolerance (a, b, 0.05));
}

TEST_CASE ("Parity: Report enthaelt alle Zeilen", "[engine][parity]")
{
    constexpr double rate = 48000.0;

    const auto sine = makeSine (1000.0, (int) (0.25 * rate), rate, 0.5f);
    const auto rep  = ParityHarness::report (ParityHarness::measure (sine, rate));

    REQUIRE (rep.contains ("LENGTH"));
    REQUIRE (rep.contains ("CENTROID"));
    REQUIRE (rep.contains ("CLIPPED"));
}
