// B-23: Tests fuer den Onset-Detektor (Huellkurven-Fluss).
// Toleranz der Onset-Zeit: +/-10 ms.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>

#include "library/analysis/OnsetDetector.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kPi = juce::MathConstants<double>::pi;

    /** Stille, danach ein 80-Hz-Sinus mit Amplitude 0.8 auf zwei Kanaelen. */
    juce::AudioBuffer<float> makeBurst (double silenceSeconds, double burstSeconds, double rate)
    {
        const int silenceSamples = juce::jmax (0, (int) std::lround (silenceSeconds * rate));
        const int burstSamples   = juce::jmax (0, (int) std::lround (burstSeconds * rate));
        const int numSamples     = silenceSamples + burstSamples;

        juce::AudioBuffer<float> buffer (2, juce::jmax (1, numSamples));
        buffer.clear();

        for (int i = 0; i < burstSamples; ++i)
        {
            const float value = 0.8f
                              * (float) std::sin (2.0 * kPi * 80.0 * (double) i / rate);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, silenceSamples + i, value);
        }

        return buffer;
    }
} // namespace

TEST_CASE ("Onset: Stille vor dem Burst wird erkannt", "[library][onset]")
{
    auto buf = makeBurst (0.050, 0.200, 48000.0);
    auto r   = OnsetDetector::detect (buf, 48000.0);

    REQUIRE (r.found);
    REQUIRE_THAT (r.onsetSeconds, WithinAbs (0.050, 0.010));
}

TEST_CASE ("Onset: sofortiger Beginn", "[library][onset]")
{
    auto buf = makeBurst (0.0, 0.200, 48000.0);
    auto r   = OnsetDetector::detect (buf, 48000.0);

    REQUIRE (r.found);
    REQUIRE (r.onsetSeconds <= 0.010);
}

TEST_CASE ("Onset: reine Stille findet nichts", "[library][onset]")
{
    const int numSamples = (int) std::lround (0.5 * 48000.0);

    juce::AudioBuffer<float> buf (2, numSamples);
    buf.clear();

    auto r = OnsetDetector::detect (buf, 48000.0);

    REQUIRE_FALSE (r.found);
    REQUIRE (r.onsetSample == 0);
}

TEST_CASE ("Onset: Confidence liegt in 0..1", "[library][onset]")
{
    auto buf = makeBurst (0.050, 0.200, 48000.0);
    auto r   = OnsetDetector::detect (buf, 48000.0);

    REQUIRE (r.confidence >= 0.0);
    REQUIRE (r.confidence <= 1.0);
}

TEST_CASE ("Onset: leerer Puffer ist harmlos", "[library][onset]")
{
    juce::AudioBuffer<float> leer;

    REQUIRE_NOTHROW (OnsetDetector::detect (leer, 48000.0));
    REQUIRE_FALSE (OnsetDetector::detect (leer, 48000.0).found);
}
