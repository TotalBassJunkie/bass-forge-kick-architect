#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "fx/FxModule.h"
#include "fx/dsp/Biquad.h"

namespace bfka
{

/**
 * Finalizer - Ticket B-67, `FxType::finalizer`, XML-Kennung "final".
 *
 * Letzte Stufe des Racks vor MIX/OUT GAIN: Sub-Boost, Mono-Bass,
 * Aufwaertskompression und True-Peak-Clipping.
 *
 * ## Signalfluss (Reihenfolge)
 *
 * ```
 * In
 *   -> Subsonik-Hochpass 15 Hz (2. Ordnung, gegen DC/Subsonik)
 *   -> SUB-BOOST      Low-Shelf 2. Ordnung (RBJ) bei `freq`, `boost` dB
 *   -> MONO BASS      LR4-Crossover bei `monoF`, Tiefenanteil auf Mono
 *   -> UPWARD         Aufwaertskompression der Spitzen unter -6 dBFS
 *   -> TRUE-PEAK-CLIP Lookahead-Limiter, Ceiling 0 dBFS, 4x-Oversampling
 *   -> Out (100 % nass; MIX/OUT GAIN macht die Ketten-Engine)
 * ```
 *
 * Die Stufen 1 und 2 sind bis 15 Hz hinunter aktiv; der Subsonik-Hochpass
 * laeuft immer mit (Zustand), wird aber nur eingeblendet, wenn mindestens
 * eine Stufe arbeitet - sonst waere der Bypass-Nulltest nicht bitgenau
 * (Spec `docs/specs/fx-finalizer.md`, Abschnitt 7, O-F1).
 *
 * ## Latenz
 *
 * `latencySamples()` ist die Lookahead-Laenge
 * `jmax (1, (int) std::ceil (0.0015 * sampleRate))` und bleibt auch bei
 * `clip = 0` erhalten (72 Samples bei 48 kHz, 144 bei 96 kHz). Der
 * Kettenprozessor meldet die Summe der Modullatenzen an den Host.
 *
 * ## Threading
 *
 * `prepare()`/`reset()`/`setParam()` laufen auf dem Message-Thread,
 * `process()` auf dem Audio-Thread. `process()` allokiert nicht, loggt nicht
 * und nimmt keine Sperre; die Parameterziele liegen in `std::atomic<float>`
 * und werden sampleweise mit 20 ms geglaettet. Filterkoeffizienten werden
 * hoechstens einmal pro Block neu gesetzt.
 */
class FinalizerModule final : public FxModule
{
public:
    /** Hoechste unterstuetzte Kanalzahl (das Rack ist stereo). */
    static constexpr int maxChannels = 2;

    /** Faktor des True-Peak-Oversamplings (4x). */
    static constexpr int oversamplingFactor = 4;

    // --- Design-Konstanten (Spec: docs/specs/fx-finalizer.md, Abschnitt 4) --
    static constexpr float kSubsonicHz         = 15.0f;   ///< Subsonik-Hochpass, immer im Zustand
    static constexpr float kSubsonicQ          = 0.70710678f;
    static constexpr float kShelfQ             = 1.2f;    ///< Shelf-Slope (RBJ), siehe Spec 4.1
    static constexpr float kCrossoverQ         = 0.70710678f; ///< Butterworth-2 je LR4-Flanke
    static constexpr float kUpwardThresholdDb  = -6.0f;   ///< Schwelle der Aufwaertskompression
    static constexpr float kUpwardMaxLiftDb    = 12.0f;   ///< maximale Anhebung
    static constexpr float kUpwardAttackMs     = 5.0f;    ///< Huellkurven-Follower
    static constexpr float kUpwardReleaseMs    = 100.0f;
    static constexpr double kLookaheadSeconds  = 0.0015;  ///< 1,5 ms Lookahead (double: ceil(0.0015*48000) == 72)
    static constexpr float kCeiling            = 1.0f;    ///< 0 dBFS Ausgangsdecke
    static constexpr float kParamSmoothMs      = 20.0f;   ///< Glaettung aller Parameter
    static constexpr float kLimiterReleaseMs   = 50.0f;   ///< Erholzeit des Limiter-Gains
    static constexpr float kDenormalGuard      = 1.0e-15f;
    static constexpr float kCoeffEpsilon       = 1.0e-6f; ///< Hysterese der Koeffizienten

    /** Parameterindizes in der Reihenfolge von `params()`. */
    enum ParamIndex : int
    {
        pBoost    = 0,   ///< "boost"    0...12 dB  - Sub-Boost
        pFreq     = 1,   ///< "freq"    20...200 Hz - Shelf-Frequenz
        pClip     = 2,   ///< "clip"     0...100 %  - True-Peak-Clipping
        pUpward   = 3,   ///< "upward"   0...100 %  - Aufwaertskompression
        pMonoFreq = 4,   ///< "monoF"   60...500 Hz - Crossover des Mono-Basses
        pShelfOn  = 5,   ///< "shelfOn"  0/1       - Sub-Boost ein/aus
        pMonoOn   = 6,   ///< "monoOn"   0/1       - Mono-Bass ein/aus
        pClipOn   = 7,   ///< "clipOn"   0/1       - Clipping ein/aus
        numParams = 8
    };

    FinalizerModule();

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::finalizer; }

    const std::vector<FxParamDesc>& params() const noexcept override;

    void prepare (double sampleRate, int maxBlockSize) override;

    void reset() override;

    void setParam (int index, float value) override;

    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    /** Lookahead-Laenge in Samples (auch bei `clip = 0`). */
    int latencySamples() const noexcept override { return reportedLatency; }

    juce::String displayName() const override { return "Finalizer"; }

    // --- Hilfsfunktionen (oeffentlich fuer Tests und Dokumentation) --------
    /** Lookahead-Laenge, die eine Sample-Rate ergibt (>= 1 Sample). */
    static int lookaheadSamplesFor (double sampleRate) noexcept;

    // --- Diagnose (nur Tests/Doku; atomar, ohne Sperre) -------------------
    /** Zuletzt angewendete Aufwaerts-Anhebung in dB (0 = neutral). */
    float currentUpwardGainDb() const noexcept
    {
        return diagUpwardGainDb.load (std::memory_order_relaxed);
    }

    /** Zuletzt angewendeter Limiter-Gain (1 = unbeteiligt). */
    float currentLimiterGain() const noexcept
    {
        return diagLimiterGain.load (std::memory_order_relaxed);
    }

    /** Sample-Rate aus dem letzten `prepare()` (Diagnose). */
    double sampleRate() const noexcept { return preparedSampleRate; }

    /** Blockgroesse aus dem letzten `prepare()` (Diagnose). */
    int blockSize() const noexcept { return preparedBlockSize; }

private:
    /** Setzt Filter, Verzoegerungen, Huellkurven und den Limiter zurueck. */
    void resetSignalState() noexcept;

    /** Uebernimmt die Parameterziele in die Glaettung (nach prepare()). */
    void syncSmoothedToTargets() noexcept;

    /** Setzt Shelf- und Crossover-Koeffizienten, wenn sich Werte geaendert haben. */
    void updateFilterCoefficients() noexcept;

    /**
     * Spitzenwert eines Kanals inklusive 4x-Oversampling: Catmull-Rom zwischen
     * `x[n-1]` und `x[n]`, vierter Stuetzpunkt linear extrapoliert. Kein
     * zusaetzliches Sample Latenz, keine Tabellen, keine Allokation.
     */
    float oversampledPeak (int channel, float sample) noexcept;

    double preparedSampleRate = 48000.0;
    int    preparedBlockSize  = 512;

    /** Lookahead in Samples; 0 vor dem ersten `prepare()`. */
    int lookahead       = 0;
    int reportedLatency = 0;

    // Parameterziele (Message-/Host-Thread -> Audio-Thread), Anzeige-Einheiten
    std::atomic<float> paramTargets[numParams];

    // Geglaettete Arbeitswerte (Audio-Thread)
    double smoothedBoostDb      = 0.0;   ///< dB
    double smoothedShelfHz      = 50.0;  ///< Hz
    double smoothedMonoHz       = 120.0; ///< Hz
    double smoothedClipAmount   = 0.0;   ///< 0...1 (clip/100, sonst 0)
    double smoothedUpwardAmount = 0.0;   ///< 0...1 (upward/100)
    double smoothedEngage       = 0.0;   ///< 0...1 Subsonik-Hochpass eingeblendet
    double smoothedShelfMix     = 0.0;   ///< 0...1 Sub-Boost eingeblendet
    double smoothedMonoMix      = 0.0;   ///< 0...1 Mono-Bass eingeblendet

    /** One-Pole-Koeffizient der Parameterglaettung (20 ms), pro Sample benutzt. */
    double paramSmoothCoeff = 1.0;

    // Zuletzt gesetzte Koeffizienten (Hysterese, einmal pro Block)
    double appliedBoostDb  = -1.0e9;
    double appliedShelfHz  = -1.0;
    double appliedMonoHz   = -1.0;

    // --- Signalzustand ----------------------------------------------------
    dsp::Biquad subsonic[maxChannels];
    dsp::Biquad shelf[maxChannels];
    dsp::Biquad monoLowPass[maxChannels][2];
    dsp::Biquad monoHighPass[maxChannels][2];

    dsp::EnvelopeFollower upwardFollower;

    // Lookahead-Verzoegerung (Laenge = lookahead je Kanal)
    std::array<std::vector<float>, (size_t) maxChannels> delayLine;
    int delayPos = 0;

    /** Benoetigter Gain der letzten `lookahead + 1` Samples (Fenster-Minimum). */
    std::vector<float> gainRing;
    int   gainPos     = 0;
    float limiterGain = 1.0f;
    float limiterReleaseCoeff = 0.0f;

    /** Verlauf fuer das Oversampling: [0] = x[n-1], [1] = x[n-2]. */
    std::array<std::array<float, 2>, (size_t) maxChannels> peakHistory {};

    // Diagnose (Audio-Thread schreibt, Tests lesen)
    std::atomic<float> diagUpwardGainDb { 0.0f };
    std::atomic<float> diagLimiterGain  { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinalizerModule)
};

} // namespace bfka
