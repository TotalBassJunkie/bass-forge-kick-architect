#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/PitchShifter.h"

using namespace bfka;
using Catch::Matchers::WithinRel;

namespace
{
    /** Sinus auf zwei Kanaelen; L und R sind bitgleich. */
    juce::AudioBuffer<float> makeSine (double seconds, double hz, double rate)
    {
        const int numSamples = juce::jmax (1, (int) std::lround (seconds * rate));

        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float value = 0.5f
                              * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                  * hz * (double) i / rate);

            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample (ch, i, value);
        }

        return buffer;
    }

    /** Zaehlt die Aufwaertsdurchgaenge durch Null in Kanal 0. */
    int countZeroCrossings (const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();

        if (numSamples < 2 || buffer.getNumChannels() < 1)
            return 0;

        const float* data = buffer.getReadPointer (0);

        int crossings = 0;

        for (int i = 1; i < numSamples; ++i)
        {
            const double previous = (double) data[i - 1];
            const double current  = (double) data[i];

            if (previous <= 0.0 && current > 0.0)
                ++crossings;
        }

        return crossings;
    }
} // namespace

// ===========================================================================
// Pitch-Shift (Ticket B-44)
// ===========================================================================

TEST_CASE ("PitchShifter: Laenge bleibt erhalten", "[engine][pitch]")
{
    const double rate = 48000.0;

    auto input = makeSine (1.0, 100.0, rate);

    PitchShifter ps;
    ps.prepare (rate, 2);

    juce::AudioBuffer<float> output;
    ps.process (input, output, 12.0);

    REQUIRE (output.getNumSamples() == input.getNumSamples());
}

TEST_CASE ("PitchShifter: +12 Halbtoene verdoppelt die Frequenz", "[engine][pitch]")
{
    const double rate = 48000.0;

    auto input = makeSine (1.0, 100.0, rate);

    const int crossingsBefore = countZeroCrossings (input);
    REQUIRE (crossingsBefore > 10);

    PitchShifter ps;
    ps.prepare (rate, 2);

    juce::AudioBuffer<float> output;
    ps.process (input, output, 12.0);

    const int crossingsAfter = countZeroCrossings (output);
    REQUIRE (crossingsAfter > 10);

    REQUIRE_THAT ((double) crossingsAfter / (double) crossingsBefore,
                  WithinRel (2.0, 0.06));
}

TEST_CASE ("PitchShifter: 0 Halbtoene laesst das Signal nahezu unveraendert", "[engine][pitch]")
{
    const double rate = 48000.0;

    auto input = makeSine (1.0, 100.0, rate);

    const int crossingsBefore = countZeroCrossings (input);
    REQUIRE (crossingsBefore > 10);

    PitchShifter ps;
    ps.prepare (rate, 2);

    juce::AudioBuffer<float> output;
    ps.process (input, output, 0.0);

    const int crossingsAfter = countZeroCrossings (output);
    REQUIRE (crossingsAfter > 10);

    REQUIRE_THAT ((double) crossingsAfter / (double) crossingsBefore,
                  WithinRel (1.0, 0.06));
}

TEST_CASE ("PitchShifter: Bereich wird geklemmt", "[engine][pitch]")
{
    // 36 Halbtoene liegen ausserhalb von +-24 und werden auf +-24 geklemmt.
    // 24 Halbtoene sind zwei Oktaven, also Faktor 4 bzw. 0.25 - nicht 8.
    REQUIRE_THAT (PitchShifter::ratioForSemitones (36.0), WithinRel (4.0, 1.0e-9));
    REQUIRE_THAT (PitchShifter::ratioForSemitones (-36.0), WithinRel (0.25, 1.0e-9));
}
