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
    // Testsignale
    // -----------------------------------------------------------------------

    /** Sinus auf allen Kanaelen; L und R sind bitgleich. */
    juce::AudioBuffer<float> makeSine (double frequencyHz, double seconds,
                                       int numChannels = 1, float amplitude = 0.5f)
    {
        const int numSamples = juce::jmax (1, (int) std::lround (seconds * kSampleRate));

        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float value = amplitude
                              * (float) std::sin (2.0 * kPi * frequencyHz * (double) i / kSampleRate);

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, value);
        }

        return buffer;
    }

    /** Reproduzierbares Rauschen; L und R sind bitgleich. */
    juce::AudioBuffer<float> makeNoise (int numSamples, int numChannels = 1,
                                        float amplitude = 0.3f, juce::int64 seed = 20261117)
    {
        juce::AudioBuffer<float> buffer (numChannels, juce::jmax (1, numSamples));
        juce::Random random (seed);

        for (int i = 0; i < numSamples; ++i)
        {
            const float value = amplitude * (random.nextFloat() * 2.0f - 1.0f);

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, value);
        }

        return buffer;
    }

    /** Stille Eingabe. */
    juce::AudioBuffer<float> makeSilence (int numSamples, int numChannels = 1)
    {
        juce::AudioBuffer<float> buffer (numChannels, juce::jmax (0, numSamples));
        buffer.clear();
        return buffer;
    }

    /** Dirac an `index` (Rest Stille). */
    juce::AudioBuffer<float> makeImpulse (int numSamples, int index,
                                          int numChannels = 1, float amplitude = 1.0f)
    {
        juce::AudioBuffer<float> buffer (numChannels, juce::jmax (1, numSamples));
        buffer.clear();

        if (index >= 0 && index < numSamples)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, index, amplitude);
        }

        return buffer;
    }

    // -----------------------------------------------------------------------
    // Messungen
    // -----------------------------------------------------------------------

    double rmsOf (const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        const int numSamples = buffer.getNumSamples();

        if (numSamples <= 0 || channel >= buffer.getNumChannels())
            return 0.0;

        const float* data = buffer.getReadPointer (channel);

        double sum = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const double value = (double) data[i];
            sum += value * value;
        }

        return std::sqrt (sum / (double) numSamples);
    }

    /** Groesster Betrag eines Kanals; `indexOut` liefert die Position. */
    double peakOf (const juce::AudioBuffer<float>& buffer, int channel = 0, int* indexOut = nullptr)
    {
        const int numSamples = buffer.getNumSamples();

        double peak = 0.0;
        int    peakIndex = -1;

        if (numSamples > 0 && channel < buffer.getNumChannels())
        {
            const float* data = buffer.getReadPointer (channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const double magnitude = std::abs ((double) data[i]);

                if (magnitude > peak)
                {
                    peak = magnitude;
                    peakIndex = i;
                }
            }
        }

        if (indexOut != nullptr)
            *indexOut = peakIndex;

        return peak;
    }

    /** Groesste Differenz zwischen zwei Kanaelen (0 = identisch). */
    double maxChannelDifference (const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();

        if (buffer.getNumChannels() < 2 || numSamples <= 0)
            return 0.0;

        const float* left  = buffer.getReadPointer (0);
        const float* right = buffer.getReadPointer (1);

        double difference = 0.0;

        for (int i = 0; i < numSamples; ++i)
            difference = juce::jmax (difference, std::abs ((double) left[i] - (double) right[i]));

        return difference;
    }

    /** Groesste Differenz zwischen zwei Puffern (gleiche Kanalzahl vorausgesetzt). */
    double maxDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        const int numSamples  = juce::jmin (a.getNumSamples(), b.getNumSamples());
        const int numChannels = juce::jmin (a.getNumChannels(), b.getNumChannels());

        if (numSamples <= 0 || numChannels <= 0)
            return 0.0;

        double difference = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* da = a.getReadPointer (ch);
            const float* db = b.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                difference = juce::jmax (difference, std::abs ((double) da[i] - (double) db[i]));
        }

        return difference;
    }

    /** Alle Samples endlich (kein NaN, kein Inf)? */
    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite ((double) data[i]))
                    return false;
        }

        return true;
    }

    /** Normierter Korrelationskoeffizient zwischen zwei Kanaelen. */
    double channelCorrelation (const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();

        if (buffer.getNumChannels() < 2 || numSamples <= 0)
            return 0.0;

        const float* left  = buffer.getReadPointer (0);
        const float* right = buffer.getReadPointer (1);

        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const double l = (double) left[i];
            const double r = (double) right[i];

            sumLR += l * r;
            sumLL += l * l;
            sumRR += r * r;
        }

        const double denominator = std::sqrt (sumLL * sumRR);

        return (denominator > 1.0e-12) ? sumLR / denominator : 0.0;
    }

    /**
     * Dominante Frequenz per FFT mit parabolischer Nachinterpolation.

     * Es wird ein Hann-gefenstertes 16384er-Fenster aus der Puffermitte
     * genommen; das blendet Einschwing- und Endeffekte aus.
     */
    double dominantFrequency (const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        const int numSamples = buffer.getNumSamples();

        if (numSamples < 64 || channel >= buffer.getNumChannels())
            return 0.0;

        constexpr int fftOrder = 14;                  // 16384
        constexpr int fftSize  = 1 << fftOrder;

        std::vector<float> data ((size_t) fftSize * 2, 0.0f);

        const int usable = juce::jmin (numSamples, fftSize);
        const int start  = juce::jmax (0, (numSamples - usable) / 2);
        const float* source = buffer.getReadPointer (channel);

        for (int i = 0; i < usable; ++i)
        {
            const double window = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) usable);
            data[(size_t) i] = (float) ((double) source[start + i] * window);
        }

        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (data.data());

        const int numBins = fftSize / 2;
        int   peakBin = 1;
        float peakValue = -1.0f;

        for (int bin = 1; bin < numBins - 1; ++bin)
        {
            if (data[(size_t) bin] > peakValue)
            {
                peakValue = data[(size_t) bin];
                peakBin   = bin;
            }
        }

        double delta = 0.0;

        if (peakBin > 0 && peakBin < numBins - 1)
        {
            const double left   = (double) data[(size_t) peakBin - 1];
            const double centre = (double) data[(size_t) peakBin];
            const double right  = (double) data[(size_t) peakBin + 1];
            const double denom  = left - 2.0 * centre + right;

            if (std::abs (denom) > 1.0e-20)
                delta = 0.5 * (left - right) / denom;

            delta = juce::jlimit (-0.5, 0.5, delta);
        }

        return ((double) peakBin + delta) * kSampleRate / (double) fftSize;
    }

    /**
     * Frequenz aus dem Abstand der Nulldurchgaenge (Gegenprobe zur FFT).

     * Gezaehlt werden nur Aufwaertsdurchgaenge; die Frequenz ergibt sich aus
     * erstem und letztem Durchgang, damit einzelne Stoerungen nicht zaehlen.
     */
    double zeroCrossingFrequency (const juce::AudioBuffer<float>& buffer, int channel,
                                  int start, int length)
    {
        const int numSamples = buffer.getNumSamples();

        if (channel >= buffer.getNumChannels() || start < 1)
            return 0.0;

        const int end = juce::jmin (numSamples, start + length);

        if (end <= start + 1)
            return 0.0;

        const float* data = buffer.getReadPointer (channel);

        int    crossings = 0;
        double firstCrossing = 0.0;
        double lastCrossing  = 0.0;

        for (int i = start; i < end; ++i)
        {
            const double previous = (double) data[i - 1];
            const double current  = (double) data[i];

            if (previous <= 0.0 && current > 0.0)
            {
                const double fraction = (current - previous > 0.0)
                                          ? (-previous / (current - previous))
                                          : 0.0;

                const double position = (double) i - 1.0 + fraction;

                if (crossings == 0)
                    firstCrossing = position;

                lastCrossing = position;
                ++crossings;
            }
        }

        if (crossings < 2 || lastCrossing <= firstCrossing)
            return 0.0;

        return (double) (crossings - 1) * kSampleRate / (lastCrossing - firstCrossing);
    }

    /** Streckt mit einer frisch vorbereiteten Engine (Standardparameter). */
    void stretchWith (WsolaStretcher& stretcher,
                      const juce::AudioBuffer<float>& input,
                      juce::AudioBuffer<float>& output,
                      double factor,
                      bool keepPitch)
    {
        stretcher.prepare (kSampleRate, juce::jmax (1, input.getNumChannels()));
        stretcher.process (input, output, factor, keepPitch);
    }
} // namespace

// ===========================================================================
// WSOLA-Kern (Ticket B-40)
// ===========================================================================

TEST_CASE ("WSOLA: Faktor 1.0 gibt die Eingabe praktisch unveraendert zurueck", "[wsola]")
{
    WsolaStretcher stretcher;

    auto input = makeNoise (24000, 1, 0.3f);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 1.0, true);

    REQUIRE (output.getNumChannels() == 1);
    REQUIRE (output.getNumSamples() == input.getNumSamples());

    const double inputRms  = rmsOf (input);
    const double outputRms = rmsOf (output);

    REQUIRE (inputRms > 0.01);
    REQUIRE_THAT (outputRms, WithinRel (inputRms, 0.01));   // RMS-Abweichung < 1 %

    // Bei Faktor 1 ist die Rekonstruktion mathematisch exakt (Fenster und
    // Normierung heben sich auf) - nur Rundung bleibt uebrig.
    REQUIRE (maxDifference (input, output) < 1.0e-4);
    REQUIRE (allFinite (output));
}

TEST_CASE ("WSOLA: Faktor 2.0 erzeugt etwa doppelt so viele Samples", "[wsola]")
{
    WsolaStretcher stretcher;

    auto input = makeNoise (24000, 1, 0.3f);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 2.0, true);

    REQUIRE_THAT ((double) output.getNumSamples(),
                  WithinRel (2.0 * (double) input.getNumSamples(), 0.05));

    // Die Engine rundet kaufmaennisch, die Laenge ist also samplegenau.
    REQUIRE (output.getNumSamples() == WsolaStretcher::outputLengthFor (input.getNumSamples(), 2.0));
    REQUIRE (output.getNumSamples() == 48000);
}

TEST_CASE ("WSOLA: Faktor 0.5 erzeugt etwa halb so viele Samples", "[wsola]")
{
    WsolaStretcher stretcher;

    auto input = makeNoise (24000, 1, 0.3f);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 0.5, true);

    REQUIRE_THAT ((double) output.getNumSamples(),
                  WithinRel (0.5 * (double) input.getNumSamples(), 0.05));
    REQUIRE (output.getNumSamples() == 12000);
}

TEST_CASE ("WSOLA: 50-%-Konfiguration liefert dieselbe Laenge", "[wsola]")
{
    WsolaStretcher stretcher;
    stretcher.prepare (kSampleRate, 1, WsolaStretcher::fiftyPercentOverlap());

    REQUIRE (stretcher.settings().overlapSize == 1024);
    REQUIRE (stretcher.settings().frameSize == 2048);

    auto input = makeSine (220.0, 0.5);
    juce::AudioBuffer<float> output;

    stretcher.process (input, output, 1.5, true);

    REQUIRE (output.getNumSamples() == 36000);
    REQUIRE (allFinite (output));
    REQUIRE (peakOf (output) > 0.1);
}

// ===========================================================================
// KEEP PITCH vs. REPITCH (Ticket B-42)
// ===========================================================================

TEST_CASE ("REPITCH: Laenge verdoppelt und Tonhoehe halbiert", "[repitch]")
{
    WsolaStretcher stretcher;

    const double sourceFrequency = 300.0;
    auto input = makeSine (sourceFrequency, 1.0);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 2.0, false);

    REQUIRE (output.getNumSamples() == 2 * input.getNumSamples());

    // FFT-Peak
    const double measured = dominantFrequency (output);
    REQUIRE_THAT (measured, WithinRel (sourceFrequency / 2.0, 0.03));

    // Gegenprobe ueber die Nulldurchgaenge (Mitte des Signals)
    const double crossings = zeroCrossingFrequency (output, 0,
                                                    output.getNumSamples() / 4,
                                                    output.getNumSamples() / 2);
    REQUIRE_THAT (crossings, WithinRel (sourceFrequency / 2.0, 0.03));

    // Kein Grain-Suchlauf im REPITCH-Pfad.
    REQUIRE (stretcher.lastSimilarity() == 0.0f);
}

TEST_CASE ("KEEP PITCH: Laenge verdoppelt und Tonhoehe bleibt", "[wsola]")
{
    WsolaStretcher stretcher;

    const double sourceFrequency = 220.0;
    auto input = makeSine (sourceFrequency, 1.0);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 2.0, true);

    REQUIRE (output.getNumSamples() == 2 * input.getNumSamples());

    const double measured = dominantFrequency (output);
    REQUIRE_THAT (measured, WithinRel (sourceFrequency, 0.03));
    REQUIRE (allFinite (output));
}

TEST_CASE ("KEEP PITCH: Faktor 0.5 senkt die Tonhoehe ebenfalls nicht", "[wsola]")
{
    WsolaStretcher stretcher;

    const double sourceFrequency = 220.0;
    auto input = makeSine (sourceFrequency, 1.0);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 0.5, true);

    REQUIRE (output.getNumSamples() == 24000);
    REQUIRE_THAT (dominantFrequency (output), WithinRel (sourceFrequency, 0.03));
}

// ===========================================================================
// Stereo und Phasenkohaerenz
// ===========================================================================

TEST_CASE ("Stereo: identisches L/R bleibt nach dem Stretchen identisch", "[wsola][stereo]")
{
    WsolaStretcher stretcher;

    auto input = makeSine (150.0, 0.5, 2);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 1.5, true);

    REQUIRE (output.getNumChannels() == 2);
    REQUIRE (maxChannelDifference (input) == 0.0);
    REQUIRE (maxChannelDifference (output) <= 1.0e-9);
}

TEST_CASE ("Stereo: REPITCH erhaelt die Kanalidentitaet ebenfalls", "[repitch][stereo]")
{
    WsolaStretcher stretcher;

    auto input = makeNoise (12000, 2, 0.4f);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 0.75, false);

    REQUIRE (maxChannelDifference (output) <= 1.0e-9);
}

TEST_CASE ("Stereo: 90-Grad-Phase zwischen L und R bleibt erhalten", "[wsola][stereo]")
{
    // Zwei Sinus-Signale gleicher Frequenz, 90 Grad versetzt: die Korrelation
    // ist ~0. Eine getrennte Kanal-Suche wuerde die Phase zufaellig verdrehen und
    // den Bass duenn machen - die gemeinsame Suche darf das nicht tun.
    const int numSamples = 24000;
    juce::AudioBuffer<float> input (2, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const double phase = 2.0 * kPi * 120.0 * (double) i / kSampleRate;
        input.setSample (0, i, 0.4f * (float) std::sin (phase));
        input.setSample (1, i, 0.4f * (float) std::sin (phase + 0.5 * kPi));
    }

    const double before = channelCorrelation (input);
    REQUIRE (std::abs (before) < 1.0e-6);

    WsolaStretcher stretcher;
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 2.0, true);

    REQUIRE_THAT (channelCorrelation (output), WithinAbs (before, 0.1));
}

// ===========================================================================
// Grenzfaelle (Ticket B-47)
// ===========================================================================

TEST_CASE ("Stille bleibt Stille (keine NaN, keine Inf)", "[wsola][edge]")
{
    WsolaStretcher stretcher;

    auto input = makeSilence (4800, 2);

    for (const double factor : { 0.25, 0.5, 1.0, 2.0, 8.0 })
    {
        for (const bool keepPitch : { true, false })
        {
            juce::AudioBuffer<float> output;
            stretchWith (stretcher, input, output, factor, keepPitch);

            REQUIRE (output.getNumSamples() == WsolaStretcher::outputLengthFor (4800, factor));
            REQUIRE (output.getNumChannels() == 2);
            REQUIRE (allFinite (output));

            for (int ch = 0; ch < output.getNumChannels(); ++ch)
                for (int i = 0; i < output.getNumSamples(); ++i)
                    REQUIRE (output.getSample (ch, i) == 0.0f);
        }
    }
}

TEST_CASE ("Faktor 0.01 wird auf 0.25 geklemmt", "[wsola][edge]")
{
    WsolaStretcher stretcher;

    auto input = makeNoise (8000, 1, 0.3f);
    juce::AudioBuffer<float> output;

    REQUIRE (WsolaStretcher::clampFactor (0.01) == 0.25);
    REQUIRE (WsolaStretcher::clampFactor (100.0) == WsolaStretcher::maxFactor);

    stretchWith (stretcher, input, output, 0.01, true);

    REQUIRE (output.getNumSamples() == WsolaStretcher::outputLengthFor (8000, 0.25));
    REQUIRE_THAT ((double) output.getNumSamples(), WithinRel (0.25 * 8000.0, 0.05));
    REQUIRE (allFinite (output));
}

TEST_CASE ("Extremfaktoren 0.25 und 8 liefern plausible Ergebnisse", "[wsola][edge]")
{
    WsolaStretcher stretcher;

    auto input = makeSine (110.0, 0.5, 2);
    const int inputLength = input.getNumSamples();

    for (const double factor : { 0.25, 8.0 })
    {
        juce::AudioBuffer<float> output;
        stretchWith (stretcher, input, output, factor, true);

        REQUIRE (output.getNumSamples() == WsolaStretcher::outputLengthFor (inputLength, factor));
        REQUIRE (allFinite (output));
        REQUIRE (peakOf (output) > 0.05);
        REQUIRE (peakOf (output) <= 1.5);
    }
}

TEST_CASE ("Sehr kurze Eingaben stuerzen nicht ab", "[wsola][edge]")
{
    WsolaStretcher stretcher;

    for (const int length : { 0, 1, 2, 7, 8, 40 })
    {
        juce::AudioBuffer<float> input (1, juce::jmax (0, length));

        for (int i = 0; i < length; ++i)
            input.setSample (0, i, 0.5f * (float) std::sin (0.3 * (double) i));

        for (const bool keepPitch : { true, false })
        {
            juce::AudioBuffer<float> output;
            stretchWith (stretcher, input, output, 2.0, keepPitch);

            REQUIRE (output.getNumSamples() == WsolaStretcher::outputLengthFor (length, 2.0));
            REQUIRE (allFinite (output));
        }
    }
}

TEST_CASE ("Impuls bleibt bei Faktor 1.0 samplegenau erhalten", "[wsola][edge]")
{
    WsolaStretcher stretcher;

    auto input = makeImpulse (4096, 512);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 1.0, true);

    int peakIndex = -1;
    const double peak = peakOf (output, 0, &peakIndex);

    REQUIRE (peakIndex == 512);
    REQUIRE_THAT (peak, WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("Impuls bleibt bei Faktor 2.0 einmalig (kein Doppel-Transient)", "[wsola][edge]")
{
    WsolaStretcher stretcher;

    auto input = makeImpulse (4096, 512);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 2.0, true);

    int peakIndex = -1;
    const double peak = peakOf (output, 0, &peakIndex);

    REQUIRE (peakIndex == 512);
    REQUIRE_THAT (peak, WithinAbs (1.0, 1.0e-6));

    // Ausserhalb des Anschlags darf kein zweiter Ausschlag stehen (kein Doppel-Transient).
    double secondPeak = 0.0;

    for (int i = 0; i < output.getNumSamples(); ++i)
        if (std::abs (i - 512) > 4)
            secondPeak = juce::jmax (secondPeak, std::abs ((double) output.getSample (0, i)));

    REQUIRE (secondPeak < 0.05);
}

// ===========================================================================
// Determinismus und Diagnose
// ===========================================================================

TEST_CASE ("Determinismus: gleiche Eingabe ergibt bitgleiche Ausgabe", "[wsola]")
{
    auto input = makeSine (123.0, 0.4, 2, 0.6f);

    WsolaStretcher first;
    WsolaStretcher second;

    juce::AudioBuffer<float> outputA;
    juce::AudioBuffer<float> outputB;
    juce::AudioBuffer<float> outputC;

    stretchWith (first, input, outputA, 1.7, true);
    stretchWith (second, input, outputB, 1.7, true);

    // Derselbe Aufruf noch einmal: kein Zustand aus dem vorherigen Lauf.
    first.process (input, outputC, 1.7, true);

    REQUIRE (outputA.getNumSamples() == outputB.getNumSamples());
    REQUIRE (maxDifference (outputA, outputB) == 0.0);
    REQUIRE (maxDifference (outputA, outputC) == 0.0);
}

TEST_CASE ("Diagnose: Aehnlichkeitsmass liegt im gueltigen Bereich", "[wsola]")
{
    WsolaStretcher stretcher;

    auto input = makeSine (200.0, 1.0);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 2.0, true);

    const float last = stretcher.lastSimilarity();
    const float mean = stretcher.meanSimilarity();

    REQUIRE (last >= 0.0f);
    REQUIRE (last <= 1.0f);
    REQUIRE (mean >= 0.0f);
    REQUIRE (mean <= 1.0f);

    // Ein periodisches Signal muss sich gut wiederfinden lassen.
    REQUIRE ((double) mean > 0.5);
}

TEST_CASE ("Diagnose: REPITCH meldet keine Grain-Suche", "[repitch]")
{
    WsolaStretcher stretcher;

    auto input = makeSine (300.0, 0.5);
    juce::AudioBuffer<float> output;

    stretchWith (stretcher, input, output, 1.5, false);

    REQUIRE (stretcher.lastSimilarity() == 0.0f);
    REQUIRE (stretcher.meanSimilarity() == 0.0f);
}

// ===========================================================================
// Resampler (Baustein fuer REPITCH und Pitch-Shift)
// ===========================================================================

TEST_CASE ("Resampler: Verhaeltnis 1 ist die Identitaet", "[resampler]")
{
    Resampler resampler;
    resampler.reset (2);

    auto input = makeNoise (1000, 2, 0.5f);
    juce::AudioBuffer<float> output;

    resampler.process (input, output, 1.0);

    REQUIRE (output.getNumSamples() == input.getNumSamples());
    REQUIRE (maxDifference (input, output) == 0.0);
}

TEST_CASE ("Resampler: Laenge folgt dem Verhaeltnis", "[resampler]")
{
    REQUIRE (Resampler::outputLengthFor (1000, 2.0) == 2000);
    REQUIRE (Resampler::outputLengthFor (1000, 0.5) == 500);
    REQUIRE (Resampler::outputLengthFor (0, 2.0) == 0);
    REQUIRE (Resampler::outputLengthFor (1, 0.25) == 1);      // mindestens ein Sample
    REQUIRE (Resampler::outputLengthFor (1000, -3.0) == 1000); // unsinnig -> unveraendert

    Resampler resampler;
    resampler.reset (1);

    auto input = makeSine (100.0, 0.25);
    juce::AudioBuffer<float> output;

    resampler.process (input, output, 3.0);
    REQUIRE (output.getNumSamples() == Resampler::outputLengthFor (input.getNumSamples(), 3.0));
}

TEST_CASE ("Resampler: kubisch und linear aendern die Tonhoehe gleich", "[resampler]")
{
    auto input = makeSine (300.0, 1.0);

    for (const auto quality : { ResamplerQuality::linear, ResamplerQuality::cubic })
    {
        Resampler resampler;
        resampler.reset (1);
        resampler.setQuality (quality);

        juce::AudioBuffer<float> output;
        resampler.process (input, output, 2.0);

        REQUIRE (output.getNumSamples() == 2 * input.getNumSamples());
        REQUIRE_THAT (dominantFrequency (output), WithinRel (150.0, 0.03));
        REQUIRE (allFinite (output));
    }
}

TEST_CASE ("Resampler: leere Eingabe ergibt leere Ausgabe", "[resampler]")
{
    Resampler resampler;
    resampler.reset (2);

    juce::AudioBuffer<float> input (2, 0);
    juce::AudioBuffer<float> output;

    resampler.process (input, output, 2.0);

    REQUIRE (output.getNumSamples() == 0);
    REQUIRE (allFinite (output));
}
