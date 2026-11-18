// B-130: Umsetzung des Parity-Harness (Impuls-/Spektrum-Invarianten).

#include "engine/ParityHarness.h"

#include <cmath>

namespace bfka
{

namespace
{
    /** Untergrenze fuer rmsDb (Stille). */
    constexpr double kMinDb = -100.0;

    /** Energie, unterhalb derer ein Puffer als still gilt. */
    constexpr double kTinyEnergy = 1.0e-20;

    /** Ab diesem Betrag gilt ein dcOffset als "neutral" (Polaritaet egal). */
    constexpr double kDcEpsilon = 1.0e-6;

    /** Fuss der Centroid-Naeherung. */
    constexpr double kHighBandBaseHz = 2000.0;

    /** Spanne der Centroid-Naeherung (2 kHz .. 20 kHz). */
    constexpr double kHighBandSpanHz = 18000.0;

    /** Absolute RMS-Toleranz in dB, zusaetzlich zur relativen. */
    constexpr double kRmsAbsoluteDb = 6.0;
} // namespace

ParityMetrics ParityHarness::measure (const juce::AudioBuffer<float>& buffer,
                                      double sampleRate,
                                      int latencySamples)
{
    ParityMetrics m;
    m.latencySamples = latencySamples;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (sampleRate > 0.0)
        m.lengthSeconds = (double) numSamples / sampleRate;

    // Leerer Puffer: alles 0, rmsDb bleibt die dokumentierte Untergrenze.
    if (numSamples <= 0 || numChannels <= 0)
        return m;

    double sum         = 0.0;   // Summe mono (dcOffset)
    double sumSquares  = 0.0;   // Summe mono^2 (RMS/Energie)
    double diffSquares = 0.0;   // Energie der ersten Differenz (Hochband)
    double peak        = 0.0;
    int    peakIndex   = 0;
    bool   clipped     = false;
    double previous    = 0.0;   // x[-1] = 0

    const double invChannels = 1.0 / (double) numChannels;

    for (int s = 0; s < numSamples; ++s)
    {
        double mono = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const double value = (double) buffer.getSample (ch, s);
            mono += value;

            if (std::abs (value) >= 1.0)
                clipped = true;
        }

        mono *= invChannels;

        sum        += mono;
        sumSquares += mono * mono;

        // Erstes Maximum gewinnt: der Transient bleibt am Anfang.
        const double magnitude = std::abs (mono);

        if (magnitude > peak)
        {
            peak      = magnitude;
            peakIndex = s;
        }

        const double diff = mono - previous;
        diffSquares += diff * diff;
        previous     = mono;
    }

    m.peakAmplitude = peak;
    m.clipped       = clipped;
    m.dcOffset      = sum / (double) numSamples;

    const double meanSquare = sumSquares / (double) numSamples;
    const double rms        = std::sqrt (meanSquare);

    if (rms > 0.0)
    {
        m.rmsDb       = juce::jmax (kMinDb, 20.0 * std::log10 (rms));
        m.crestFactor = peak / rms;
    }
    else
    {
        m.rmsDb       = kMinDb;
        m.crestFactor = 0.0;
    }

    if (sampleRate > 0.0)
        m.transientSeconds = (double) peakIndex / sampleRate;

    if (sumSquares > kTinyEnergy)
    {
        // Die erste Differenz kann bei sehr hoehen Lagen mehr Energie als das
        // Signal selbst tragen (Verstaerkung bis 2 bei Nyquist); der Anteil
        // wird deshalb auf [0,1] geklemmt.
        const double ratio = juce::jlimit (0.0, 1.0, diffSquares / sumSquares);

        m.highBandRatio      = ratio;
        m.spectralCentroidHz = kHighBandBaseHz + ratio * kHighBandSpanHz;
    }
    else
    {
        m.highBandRatio      = 0.0;
        m.spectralCentroidHz = 0.0;
    }

    return m;
}

juce::String ParityHarness::report (const ParityMetrics& metrics)
{
    juce::String text;

    text << "LENGTH "    << juce::String (metrics.lengthSeconds, 4)              << " s\n"
         << "PEAK "      << juce::String (metrics.peakAmplitude, 4)               << "\n"
         << "RMS "       << juce::String (metrics.rmsDb, 2)                       << " dB\n"
         << "DC "        << juce::String (metrics.dcOffset, 6)                    << "\n"
         << "CREST "     << juce::String (metrics.crestFactor, 3)                 << "\n"
         << "TRANSIENT " << juce::String (metrics.transientSeconds * 1000.0, 2)   << " ms\n"
         << "CENTROID "  << juce::String (metrics.spectralCentroidHz, 1)          << " Hz\n"
         << "HIGHRATIO " << juce::String (metrics.highBandRatio, 4)               << "\n"
         << "LATENCY "   << metrics.latencySamples                                << "\n"
         << "CLIPPED "   << (metrics.clipped ? "yes" : "no");

    return text;
}

bool ParityHarness::withinTolerance (const ParityMetrics& actual,
                                     const ParityMetrics& reference,
                                     double relativeTolerance)
{
    const double tolerance = juce::jlimit (0.0, 1.0, relativeTolerance);

    auto relativeOk = [tolerance] (double a, double b)
    {
        return std::abs (a - b) / juce::jmax (1.0e-9, std::abs (b)) <= tolerance;
    };

    const bool lengthOk   = relativeOk (actual.lengthSeconds, reference.lengthSeconds);
    const bool centroidOk = relativeOk (actual.spectralCentroidHz, reference.spectralCentroidHz);

    // RMS: relativ oder absolut innerhalb 6 dB.
    const bool rmsOk = relativeOk (actual.rmsDb, reference.rmsDb)
                       || std::abs (actual.rmsDb - reference.rmsDb) <= kRmsAbsoluteDb;

    // Polaritaet: gleiches Vorzeichen, oder eine Seite ist praktisch 0.
    const bool actualNeutral    = std::abs (actual.dcOffset)    <= kDcEpsilon;
    const bool referenceNeutral = std::abs (reference.dcOffset) <= kDcEpsilon;
    const bool polaritySame     = (actual.dcOffset >= 0.0) == (reference.dcOffset >= 0.0);
    const bool polarityOk       = actualNeutral || referenceNeutral || polaritySame;

    return lengthOk && rmsOk && centroidOk && polarityOk;
}

} // namespace bfka
