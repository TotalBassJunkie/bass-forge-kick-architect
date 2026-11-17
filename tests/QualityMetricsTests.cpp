#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <vector>

#include "engine/Resampler.h"
#include "engine/WsolaStretcher.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kPi         = juce::MathConstants<double>::pi;

    // -----------------------------------------------------------------------
    // Helper functions
    // -----------------------------------------------------------------------

    juce::AudioBuffer<float> makeSine (double seconds, double hz, double rate)
    {
        const int numSamples = juce::jmax (1, (int) std::lround (seconds * rate));

        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float value = (float) std::sin (2.0 * kPi * hz * (double) i / rate);

            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample (ch, i, value);
        }

        return buffer;
    }

    juce::AudioBuffer<float> makeTransient (double seconds, double rate)
    {
        const int numSamples = juce::jmax (1, (int) std::lround (seconds * rate));

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        // Silent at first, then a single sharp sample at 10% mark
        const int impulseIndex = (int) std::lround (0.1 * (double) numSamples);
        const int impulseSize  = 4;  // Dirac-like peak

        if (impulseIndex < numSamples)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = impulseIndex; i < juce::jmin (numSamples, impulseIndex + impulseSize); ++i)
                    buffer.setSample (ch, i, 1.0f);
            }
        }

        return buffer;
    }

    double peakAmplitude (const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        double peak = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax (peak, std::abs ((double) data[i]));
        }

        return peak;
    }

    double rms (const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        double sum = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const double value = (double) data[i];
                sum += value * value;
            }
        }

        const double count = (double) (numChannels * numSamples);
        return count > 0.0 ? std::sqrt (sum / count) : 0.0;
    }

    double highBandEnergyRatio (const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        double totalEnergy = 0.0;
        double highBandEnergy = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 1; i < numSamples; ++i)
            {
                const double diff = (double) data[i] - (double) data[i - 1];
                totalEnergy += diff * diff;
            }

            for (int i = 1; i < numSamples; ++i)
            {
                const double diff = (double) data[i] - (double) data[i - 1];
                const double absDiff = std::abs (diff);
                if (absDiff > 0.1)  // rough high-band threshold
                    highBandEnergy += diff * diff;
            }
        }

        return totalEnergy > 1.0e-12 ? (highBandEnergy / totalEnergy) : 0.0;
    }

    // -----------------------------------------------------------------------
    // Tests
    // -----------------------------------------------------------------------

    TEST_CASE ("Quality: Transient retention in WSOLA", "[engine][quality]")
    {
        WsolaStretcher stretcher;

        auto input = makeTransient (0.5, kSampleRate);
        juce::AudioBuffer<float> output;

        stretcher.prepare (kSampleRate, juce::jmax (1, input.getNumChannels()));
        stretcher.process (input, output, 2.0, true);

        REQUIRE (output.getNumSamples() == WsolaStretcher::outputLengthFor (input.getNumSamples(), 2.0));
        REQUIRE (peakAmplitude (output) > 0.5 * peakAmplitude (input));
    }

    TEST_CASE ("Quality: Aliasing limited when pitching up", "[engine][quality]")
    {
        Resampler resampler;
        resampler.reset (2);

        auto input = makeSine (200.0, 0.5, kSampleRate);
        juce::AudioBuffer<float> output;

        resampler.process (input, output, 0.5);  // one octave higher

        REQUIRE (highBandEnergyRatio (output) < 0.2);
    }

    TEST_CASE ("Quality: Phase coherence at factor 1.0", "[engine][quality]")
    {
        WsolaStretcher stretcher;

        auto input = makeSine (100.0, 2.0, kSampleRate);  // 100 Hz for 2 seconds
        juce::AudioBuffer<float> output;

        stretcher.prepare (kSampleRate, juce::jmax (1, input.getNumChannels()));
        stretcher.process (input, output, 1.0, true);

        // First 2000 samples only
        const int samplesToCompare = juce::jmin (2000, input.getNumSamples(), output.getNumSamples());
        double maxDiff = 0.0;

        for (int ch = 0; ch < input.getNumChannels() && ch < output.getNumChannels(); ++ch)
        {
            const float* inData  = input.getReadPointer (ch);
            const float* outData = output.getReadPointer (ch);

            for (int i = 0; i < samplesToCompare; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs ((double) inData[i] - (double) outData[i]));
        }

        INFO ("Max absolute difference in first " << samplesToCompare << " samples: " << maxDiff);
        REQUIRE (maxDiff < 0.02);
    }

    TEST_CASE ("Quality: Silence remains silent", "[engine][quality]")
    {
        // Test with WsolaStretcher
        {
            WsolaStretcher stretcher;
            auto input = juce::AudioBuffer<float> (2, 1000);
            input.clear();
            juce::AudioBuffer<float> output;

            stretcher.prepare (kSampleRate, juce::jmax (1, input.getNumChannels()));
            stretcher.process (input, output, 2.0, true);

            REQUIRE (peakAmplitude (output) == 0.0f);
        }

        // Test with Resampler
        {
            Resampler resampler;
            resampler.reset (2);

            juce::AudioBuffer<float> input (2, 1000);
            input.clear();
            juce::AudioBuffer<float> output;

            resampler.process (input, output, 0.5);

            REQUIRE (peakAmplitude (output) == 0.0f);
        }
    }

    TEST_CASE ("Quality: No out-of-range amplitudes", "[engine][quality]")
    {
        auto input = makeSine (1000.0, 0.9, kSampleRate);  // 1 kHz, amplitude 0.9

        // Test WSOLA stretch (compress)
        {
            WsolaStretcher stretcher;
            juce::AudioBuffer<float> output;

            stretcher.prepare (kSampleRate, juce::jmax (1, input.getNumChannels()));
            stretcher.process (input, output, 0.5, true);

            REQUIRE (peakAmplitude (output) <= 1.5 * 0.9);
        }

        // Test WSOLA stretch (expand)
        {
            WsolaStretcher stretcher;
            juce::AudioBuffer<float> output;

            stretcher.prepare (kSampleRate, juce::jmax (1, input.getNumChannels()));
            stretcher.process (input, output, 2.0, true);

            REQUIRE (peakAmplitude (output) <= 1.5 * 0.9);
        }
    }
} // namespace