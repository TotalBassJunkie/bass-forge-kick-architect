#pragma once

// B-130: Parity-Harness fuer messbare Kick-Invarianten ohne das Original.

#include <juce_audio_basics/juce_audio_basics.h>

namespace bfka
{

/** Messwerte eines gerenderten Kicks (Ticket B-130). */
struct ParityMetrics
{
    double lengthSeconds      = 0.0;
    double peakAmplitude      = 0.0;
    double rmsDb              = -100.0;
    double dcOffset           = 0.0;
    double crestFactor        = 0.0;   ///< peak / rms (linear)
    double transientSeconds   = 0.0;   ///< Zeit bis zum Peak
    double spectralCentroidHz = 0.0;
    double highBandRatio      = 0.0;   ///< Energie > 2 kHz / Gesamt
    int    latencySamples     = 0;
    bool   clipped            = false; ///< irgendein Sample >= 1.0
};

/**
 * Misst Invarianten eines Kicks, die sich ohne das Original pruefen lassen
 * (B-130). Der echte "klingt wie das Original"-Vergleich bleibt beim
 * Menschen; hier entstehen nur Zahlen.
 *
 * Messvorschrift:
 *  - Mono-Mittel ueber alle Kanaele.
 *  - `lengthSeconds = numSamples / sampleRate` (Rate <= 0 -> 0).
 *  - `peakAmplitude` = Maximum der Betraege des Mono-Signals.
 *  - `rmsDb = 20 * log10 (sqrt (mean (x^2)))`, bei 0 (und als Untergrenze)
 *    genau -100.
 *  - `dcOffset` = Mittelwert mit Vorzeichen.
 *  - `crestFactor` = peak / rms, bei rms 0 -> 0.
 *  - `transientSeconds` = Index des ersten Betragsmaximums / sampleRate.
 *  - `highBandRatio` = Energie der ersten Differenz (Hochpass ~ Nyquist/2)
 *    geteilt durch die Gesamtenergie, auf [0,1] geklemmt.
 *  - `spectralCentroidHz = 2000 + highBandRatio * 18000` (grobe, dokumentierte
 *    Naeherung ohne FFT; ausreichend fuer Invarianten).
 *  - `clipped` = irgendein Sample mit `|x| >= 1.0`.
 *  - Leerer Puffer: alle Werte 0, `rmsDb` bleibt -100, kein Absturz.
 *
 * Nur Worker-/Test-Thread; allokiert nicht.
 */
class ParityHarness
{
public:
    static ParityMetrics measure (const juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  int latencySamples = 0);

    /** Mehrzeiliger Klartext-Report (LAST PASS-Stil). */
    static juce::String report (const ParityMetrics& metrics);

    /**
     * Vergleicht zwei Messungen. true, wenn Laenge, RMS und Centroid
     * innerhalb `relativeTolerance` (0..1) liegen und die Polaritaet
     * (dcOffset-Vorzeichen) nicht kippt.
     *
     * Je Wert gilt `|a-b| / max (1e-9, |b|) <= t`; bei `rmsDb` ist
     * zusaetzlich eine Absolutdifferenz bis 6 dB erlaubt. `t` wird auf
     * [0,1] geklemmt. Ein dcOffset nahe 0 zaehlt als neutrale Polaritaet.
     */
    static bool withinTolerance (const ParityMetrics& actual,
                                 const ParityMetrics& reference,
                                 double relativeTolerance);
};

} // namespace bfka
