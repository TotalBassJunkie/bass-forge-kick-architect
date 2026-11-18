// B-56: PHASE ALIGN - Korrelation im Low-End, Nudge und Polaritaets-Flip.

#include "engine/PhaseAlign.h"

#include <cmath>
#include <vector>

namespace bfka
{

namespace
{
    /** Unterhalb dieser Normierungssumme gilt ein Fenster als "kein Signal". */
    constexpr double kNormEpsilon = 1.0e-12;

    /** Kleinste Grenzfrequenz des Low-End-Filters; haelt `a` positiv. */
    constexpr double kMinCutoffHz = 1.0;

    /**
     * Einpoliger Tiefpass in-place (Startzustand 0).

     * `a = 1 - exp (-2*pi*cutoff/sampleRate)`; die Koeffizientenform ist die
     * des Tickets, damit die Phase im Durchlassbereich nicht zusaetzlich
     * verdreht wird und der Vergleich beider Kanaele fair bleibt.
     */
    void lowpassInPlace (std::vector<double>& data, double cutoffHz, double sampleRate) noexcept
    {
        if (data.empty())
            return;

        const double rate = (sampleRate > 0.0) ? sampleRate : 48000.0;
        const double nyquistGuard = 0.49 * rate;

        if (nyquistGuard <= 0.0)
            return;

        const double cutoff = juce::jlimit (kMinCutoffHz,
                                            juce::jmax (kMinCutoffHz, nyquistGuard),
                                            cutoffHz);

        const double a = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * cutoff / rate);

        double y = 0.0;

        for (auto& value : data)
        {
            y += a * (value - y);
            value = y;
        }
    }

    /** Kanalmittel eines Puffers, gekuerzt auf `length` Samples. */
    std::vector<double> monoMix (const juce::AudioBuffer<float>& buffer, int length)
    {
        const int usable = juce::jmax (0, juce::jmin (length, buffer.getNumSamples()));
        std::vector<double> mono ((size_t) usable, 0.0);

        const int numChannels = buffer.getNumChannels();

        if (numChannels <= 0 || usable <= 0)
            return mono;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < usable; ++i)
                mono[(size_t) i] += (double) data[i];
        }

        const double scale = 1.0 / (double) numChannels;

        for (int i = 0; i < usable; ++i)
            mono[(size_t) i] *= scale;

        return mono;
    }

    /**
     * Normierte Kreuzkorrelation fuer die Verschiebung `d`.

     * Verglichen wird `ref[i]` mit `tgt[i + d]` ueber den Ueberlappungsbereich;
     * ausserhalb des Puffers wird nichts gelesen. Rueckgabe 0, wenn eines der
     * beiden Signale im Fenster keine Energie traegt (kein 0/0, kein Inf).
     */
    double correlationAt (const std::vector<double>& ref,
                          const std::vector<double>& tgt,
                          int d) noexcept
    {
        const int n     = (int) ref.size();
        const int start = juce::jmax (0, -d);
        const int end   = juce::jmin (n, n - d);

        if (end - start < 1)
            return 0.0;

        double num = 0.0;
        double sumRef = 0.0;
        double sumTgt = 0.0;

        for (int i = start; i < end; ++i)
        {
            const double r = ref[(size_t) i];
            const double t = tgt[(size_t) (i + d)];

            num    += r * t;
            sumRef += r * r;
            sumTgt += t * t;
        }

        const double den = std::sqrt (sumRef * sumTgt);

        return (den > kNormEpsilon) ? num / den : 0.0;
    }
} // namespace

PhaseAlignResult PhaseAlign::align (const juce::AudioBuffer<float>& reference,
                                    const juce::AudioBuffer<float>& target,
                                    int    maxNudgeSamples,
                                    double lowEndCutoffHz,
                                    double sampleRate)
{
    PhaseAlignResult result;

    // Guard: ein Suchfenster von 0 oder weniger Samples hat keinen Sinn.
    const int limit = juce::jmax (1, maxNudgeSamples);

    const int n = juce::jmin (reference.getNumSamples(), target.getNumSamples());

    // Leere Puffer: alles bleibt 0, `improved` bleibt false.
    if (n <= 0 || reference.getNumChannels() <= 0 || target.getNumChannels() <= 0)
        return result;

    std::vector<double> ref = monoMix (reference, n);
    std::vector<double> tgt = monoMix (target, n);

    lowpassInPlace (ref, lowEndCutoffHz, sampleRate);
    lowpassInPlace (tgt, lowEndCutoffHz, sampleRate);

    result.correlationBefore = correlationAt (ref, tgt, 0);

    int    bestShift     = 0;
    double bestCorr      = result.correlationBefore;
    double bestMagnitude = std::abs (bestCorr);

    for (int d = -limit; d <= limit; ++d)
    {
        const double corr      = correlationAt (ref, tgt, d);
        const double magnitude = std::abs (corr);

        // Echte Verbesserung, kein Gleichstand: der erste Treffer gewinnt.
        if (magnitude > bestMagnitude + 1.0e-12)
        {
            bestMagnitude = magnitude;
            bestCorr      = corr;
            bestShift     = d;
        }
    }

    // `d` zieht das Ziel nach vorn auf die Referenz; der Nudge ist die
    // anzuwendende Korrektur und zeigt deshalb in die Gegenrichtung.
    result.nudgeSamples     = -bestShift;
    result.polarityFlip     = (bestCorr < 0.0);
    result.correlationAfter = bestMagnitude;
    result.improved         = std::abs (result.correlationAfter)
                              > std::abs (result.correlationBefore) + 1.0e-9;

    return result;
}

juce::AudioBuffer<float> PhaseAlign::apply (const juce::AudioBuffer<float>& target,
                                            int nudgeSamples,
                                            bool polarityFlip)
{
    const int numChannels = juce::jmax (1, target.getNumChannels());
    const int numSamples  = juce::jmax (0, target.getNumSamples());

    juce::AudioBuffer<float> out (numChannels, numSamples);
    out.clear();

    if (target.getNumChannels() <= 0 || numSamples <= 0)
        return out;

    const float gain = polarityFlip ? -1.0f : 1.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* source = target.getReadPointer (ch);
        float*       dest   = out.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const int sourceIndex = i - nudgeSamples;

            // Vor dem Anfang und hinter dem Ende steht Stille.
            if (sourceIndex >= 0 && sourceIndex < numSamples)
                dest[i] = gain * source[sourceIndex];
        }
    }

    return out;
}

} // namespace bfka
