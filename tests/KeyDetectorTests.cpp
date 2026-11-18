// B-24: Tests fuer den Key-Detector (Chroma-Profil + Template-Matching).
// Toleranz: Grundton exakt, Geschlecht (Dur/Moll) optional.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>

#include "library/analysis/KeyDetector.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kPi = juce::MathConstants<double>::pi;

    /** Sinus auf zwei Kanaelen (L und R bitgleich), Amplitude 0.5. */
    juce::AudioBuffer<float> makeTone (double hz, double seconds, double rate)
    {
        const int numSamples = juce::jmax (1, (int) std::lround (seconds * rate));

        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float value = 0.5f * (float) std::sin (2.0 * kPi * hz * (double) i / rate);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, value);
        }

        return buffer;
    }
} // namespace

TEST_CASE ("Key: A 440 wird als A erkannt", "[library][key]")
{
    auto buf = makeTone (440.0, 0.5, 48000.0);
    auto r   = KeyDetector::detect (buf, 48000.0);

    REQUIRE (r.found);
    REQUIRE (r.keyNote == 9);   // A
}

TEST_CASE ("Key: C 261,63 wird als C erkannt", "[library][key]")
{
    auto buf = makeTone (261.63, 0.5, 48000.0);
    auto r   = KeyDetector::detect (buf, 48000.0);

    REQUIRE (r.found);
    REQUIRE (r.keyNote == 0);   // C
}

TEST_CASE ("Key: G 392 wird als G erkannt", "[library][key]")
{
    auto buf = makeTone (392.0, 0.5, 48000.0);
    auto r   = KeyDetector::detect (buf, 48000.0);

    REQUIRE (r.found);
    REQUIRE (r.keyNote == 7);   // G
}

TEST_CASE ("Key: Stille findet nichts", "[library][key]")
{
    juce::AudioBuffer<float> buf (2, (int) std::lround (0.5 * 48000.0));
    buf.clear();

    auto r = KeyDetector::detect (buf, 48000.0);

    REQUIRE_FALSE (r.found);
    REQUIRE (r.keyNote == -1);
}

TEST_CASE ("Key: Chroma ist normiert", "[library][key]")
{
    auto buf = makeTone (440.0, 0.5, 48000.0);

    const auto chroma = KeyDetector::chromaProfile (buf, 48000.0);

    double sum = 0.0;

    for (const double value : chroma)
        sum += value;

    REQUIRE_THAT (sum, WithinAbs (1.0, 1.0e-6));

    // Stille hat kein Profil: alle Zwoelf Werte bleiben 0.
    juce::AudioBuffer<float> silence (2, (int) std::lround (0.5 * 48000.0));
    silence.clear();

    const auto emptyChroma = KeyDetector::chromaProfile (silence, 48000.0);

    for (const double value : emptyChroma)
        REQUIRE (value == 0.0);
}

TEST_CASE ("Key: Notennamen", "[library][key]")
{
    REQUIRE (KeyDetector::noteName (0) == "C");
    REQUIRE (KeyDetector::noteName (9) == "A");
    REQUIRE (KeyDetector::noteName (11) == "B");
}
