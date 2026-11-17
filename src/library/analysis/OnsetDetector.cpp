// B-23: Onset-Detektor - Implementierung des Huellkurven-Flusses.

#include "library/analysis/OnsetDetector.h"

#include <cmath>
#include <vector>

namespace bfka
{
    namespace
    {
        /** Einpol-Koeffizient aus einer Zeitkonstante in Millisekunden. */
        double coefficientForMs (double milliseconds, double sampleRate) noexcept
        {
            const double safeMs = juce::jmax (1.0e-6, milliseconds);
            return std::exp (-1.0 / (safeMs * 0.001 * sampleRate));
        }
    } // namespace

    OnsetResult OnsetDetector::detect (const juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       const Settings& settings)
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        // 8. Nichts zu untersuchen.
        if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0)
            return OnsetResult {};

        const double attackCoef  = coefficientForMs (settings.envelopeAttackMs, sampleRate);
        const double releaseCoef = coefficientForMs (settings.envelopeReleaseMs, sampleRate);

        // 1. Kanalmittel: je Sample den Betrag ueber alle Kanaele mitteln.
        std::vector<double> rectified ((size_t) numSamples, 0.0);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                rectified[(size_t) i] += std::abs ((double) data[i]);
        }

        const double channelScale = 1.0 / (double) numChannels;

        for (int i = 0; i < numSamples; ++i)
            rectified[(size_t) i] *= channelScale;

        // 2./3. Huellkurve (Attack/Release) und positiver Fluss.
        std::vector<double> flux ((size_t) numSamples, 0.0);

        double envelope = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const double value = rectified[(size_t) i];
            const double coef  = (value > envelope) ? attackCoef : releaseCoef;
            const double next  = coef * envelope + (1.0 - coef) * value;

            flux[(size_t) i] = juce::jmax (0.0, next - envelope);
            envelope = next;
        }

        // 4. Nur der Anfang wird untersucht.
        int windowSamples = (int) std::llround (settings.searchWindowMs * 0.001 * sampleRate);
        windowSamples = juce::jlimit (1, numSamples, windowSamples);

        double maxFlux  = 0.0;
        int    maxIndex = 0;

        for (int i = 0; i < windowSamples; ++i)
        {
            if (flux[(size_t) i] > maxFlux)
            {
                maxFlux  = flux[(size_t) i];
                maxIndex = i;
            }
        }

        double fluxSum = 0.0;

        for (int i = 0; i < windowSamples; ++i)
            fluxSum += flux[(size_t) i];

        const double meanFlux = fluxSum / (double) windowSamples;

        // 6. Entscheidung: Spitze deutlich ueber dem Rauschboden?
        const bool found = (maxFlux > settings.minFluxFactor * meanFlux)
                        && (maxFlux > 1.0e-6);

        if (! found)
            return OnsetResult {};

        // 5. Rueckwaertssuche ab dem 10-%-Punkt vor dem Maximum: der erste
        //    Index, dessen Fluss ein Viertel der Spitze erreicht.
        const int backOff   = (int) std::llround (0.1 * (double) windowSamples);
        const int scanStart = juce::jmax (0, maxIndex - backOff);

        int onsetSample = maxIndex;

        for (int i = scanStart; i <= maxIndex; ++i)
        {
            if (flux[(size_t) i] >= 0.25 * maxFlux)
            {
                onsetSample = i;
                break;
            }
        }

        const int preRollSamples = (int) std::llround (settings.preRollMs * 0.001 * sampleRate);

        OnsetResult result;
        result.onsetSample  = juce::jmax (0, onsetSample - preRollSamples);
        result.onsetSeconds = (double) result.onsetSample / sampleRate;

        // 7. Confidence: Abstand der Spitze zum Rauschboden, auf 0..1 geklemmt.
        result.confidence = juce::jlimit (0.0, 1.0,
                                          maxFlux
                                            / juce::jmax (1.0e-9, settings.minFluxFactor * meanFlux)
                                            / 4.0);
        result.found = true;

        return result;
    }
} // namespace bfka
