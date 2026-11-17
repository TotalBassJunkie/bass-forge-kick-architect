#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "fx/FxModule.h"

namespace bfka
{

/**
 * Transient Shaper — Ticket B-65, `FxType::transientShaper`, XML-Kennung "trans".
 *
 * Handbuch-Verhalten des Referenzprodukts: ATTACK (wie scharf der Einschlag
 * ist), DUCK (der Einbruch nach dem Einschlag), SPEED (wie schnell er sich
 * erholt). Einsatz: mehr Snap geben oder Droehnen zaehmen.
 *
 * Aufbau (Details, Formeln und Messwerte: docs/specs/fx-transient-shaper.md):
 *
 *  - **Dualer Huellkurvenfolger** auf einem gemeinsamen Detektor
 *    `det = max(|L|, |R|, …)`. Der maximale Betrag ueber die Kanaele haelt die
 *    Stereobasis zusammen: identisches Signal auf L/R bleibt bitgleich, und
 *    ein Einschlag auf nur einem Kanal wirkt auf beide (kein Auseinanderdriften).
 *  - **Schneller Follower**: Attack 0,5 ms, Release = SPEED (16…200 ms).
 *  - **Langsamer Follower**: Attack 30 ms, Release 300 ms.
 *  - **Transienten-Anteil** `diff = (fast − slow) / (fast + slow)`, also
 *    normiert auf −1…+1 und **0 bei Stille** (kein 0/0):
 *      `attackShare  = max(0, diff)`  → 1 = reiner Einschlag,
 *      `sustainShare = (1 − diff)/2`  → 1 = reiner Dauer-Anteil.
 *  - **Verstaerkung** (Anzeige-Einheiten 0…100 %):
 *      `effect = attack/100 · kAttackGain · attackShare
 *              − duck/100   · kDuckGain   · sustainShare`
 *      `gain   = clamp(1 + amount/100 · effect, 0, 4)`
 *    `amount` skaliert die Gesamtwirkung; `amount == 0` reicht **bitgenau** durch.
 *  - **Weiche Saettigung ab 0 dBFS** auf dem Ausgang (C¹-stetig am Knie,
 *    Asymptote `kSoftCeiling` = 1,25 ≈ +1,94 dBFS), damit ATTACK nicht clippt.
 *
 * Latenz: 0 Samples (kein Lookahead; die Einschlag-Anhebung baut sich in der
 * 0,5-ms-Attack-Zeit des schnellen Followers auf, siehe Spec Abschnitt 6).
 *
 * Threading wie im Kettenvertrag: `prepare()`/`reset()`/`setParam()` auf dem
 * Message-Thread, `process()` auf dem Audio-Thread. `process()` allokiert
 * nicht, loggt nicht und nimmt keine Sperre; die Parameterziele liegen in
 * `std::atomic<float>`, geglaettet wird im Audio-Thread.
 */
class TransientShaperModule final : public FxModule
{
public:
    /** Parameterindizes in der Reihenfolge von `params()`. */
    enum ParamIndex : int
    {
        idxAmount = 0,   ///< "amount"  0…100 %   – Grundpegel/Wirkungsgrad
        idxAttack = 1,   ///< "attack"  0…100 %   – Anhebung des Einschlags
        idxDuck   = 2,   ///< "duck"    0…100 %   – Absenkung nach dem Einschlag
        idxSpeed  = 3,   ///< "speed"  16…200 ms  – Erholzeit des schnellen Followers
        kNumParams = 4
    };

    // --- Design-Konstanten (Spec: docs/specs/fx-transient-shaper.md, 3.2) ---
    static constexpr float kFastAttackMs     = 0.5f;   ///< schneller Follower, Attack
    static constexpr float kSlowAttackMs     = 30.0f;  ///< langsamer Follower, Attack
    static constexpr float kSlowReleaseMs    = 300.0f; ///< langsamer Follower, Release
    static constexpr float kSpeedMinMs       = 16.0f;  ///< untere Grenze von SPEED
    static constexpr float kSpeedMaxMs       = 200.0f; ///< obere Grenze von SPEED
    static constexpr float kAttackGain       = 1.0f;   ///< kA: max. +6 dB Einschlag-Anhebung
    static constexpr float kDuckGain         = 0.75f;  ///< kD: max. −7,5 dB Absenkung (sustain 1)
    static constexpr float kParamSmoothMs    = 8.0f;   ///< Glaettung aller Parameter (One-Pole)
    static constexpr float kSoftKneeLevel    = 1.0f;   ///< Knie des Limiters = 0 dBFS
    static constexpr float kSoftKneeHardness = 4.0f;   ///< Kruemmung; groesser = frueher flach
    static constexpr float kSoftCeiling      = 1.0f + 1.0f / kSoftKneeHardness; ///< 1,25
    static constexpr float kGainMin          = 0.0f;   ///< Verstaerkungsgrenzen
    static constexpr float kGainMax          = 4.0f;
    static constexpr float kEnvelopeEpsilon  = 1e-12f; ///< Schutz gegen 0/0 im Diff
    static constexpr float kDenormalGuard    = 1e-15f; ///< Huellkurven-Denormals flushen
    static constexpr float kSpeedRecalcMs    = 0.05f;  ///< Hysterese der Koeffizienten-Neuberechnung

    TransientShaperModule();

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::transientShaper; }

    const std::vector<FxParamDesc>& params() const noexcept override;

    void prepare (double sampleRate, int maxBlockSize) override;

    void reset() override;

    void setParam (int index, float value) override;

    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    /** Latenzfrei (Minimum-Phase, kein Lookahead). */
    int latencySamples() const noexcept override { return 0; }

    juce::String displayName() const override { return "Transient Shaper"; }

    // --- Hilfsfunktionen (oeffentlich fuer Tests und Dokumentation) --------
    /** One-Pole-Koeffizient `1 − exp(−1/(tau·fs))` einer Zeitkonstante in ms. */
    static float onePoleCoefficient (float timeMs, double sampleRate) noexcept;

    /**
     * Weiche Saettigung des Ausgangs (Abschnitt 4 der Spec).
     * |x| ≤ 1        -> unveraendert (bitgenau)
     * |x| > 1        -> `sign(x) · (1 + tanh(k·(|x|−1))/k)`, C¹-stetig am Knie,
     *                   Asymptote 1,25. Nicht-endliche Werte werden zu 0.
     */
    static float softLimit (float sample) noexcept;

    // --- Diagnose (nur Tests/Doku; atomar, ohne Sperre) -------------------
    /** Verstaerkung, die zuletzt auf den Puffer angewendet wurde. */
    float currentGain() const noexcept { return diagGain.load (std::memory_order_relaxed); }

    /** Schneller Follower am Ende des letzten Blocks. */
    float currentFastEnvelope() const noexcept { return diagFast.load (std::memory_order_relaxed); }

    /** Langsamer Follower am Ende des letzten Blocks. */
    float currentSlowEnvelope() const noexcept { return diagSlow.load (std::memory_order_relaxed); }

    /** Sample-Rate aus dem letzten `prepare()` (Diagnose). */
    double sampleRate() const noexcept { return preparedSampleRate; }

private:
    /** Setzt Huellkurven, Glaettung und Koeffizienten auf den Anfangszustand. */
    void resetInternal() noexcept;

    /** Schreibt die Anzeige-Werte als normierte Ziele in die Atomics. */
    void publishTargets() noexcept;

    /** Berechnet die von der Sample-Rate abhaengigen Koeffizienten neu. */
    void updateCoefficients() noexcept;

    double preparedSampleRate = 48000.0;
    int    preparedBlockSize  = 512;

    // Koeffizienten (Klasse: einmal pro prepare() bzw. bei Bedarf pro Block)
    float fastAttackCoeff   = 1.0f;
    float fastReleaseCoeff  = 1.0f;
    float slowAttackCoeff   = 1.0f;
    float slowReleaseCoeff  = 1.0f;
    float paramSmoothCoeff  = 1.0f;
    float coeffSpeedMs      = 0.0f;   ///< Speed, zu der `fastReleaseCoeff` gehoert

    // Geglaettete Parameter (Audio-Thread)
    float smoothedAmount   = 1.0f;    ///< 0…1
    float smoothedAttack   = 0.0f;    ///< 0…1
    float smoothedDuck     = 0.0f;    ///< 0…1
    float smoothedSpeedMs  = 48.0f;   ///< 16…200 ms

    // Huellkurven-Zustand (Audio-Thread)
    float fastEnvelope = 0.0f;
    float slowEnvelope = 0.0f;

    // Parameterziele (Message-/Host-Thread -> Audio-Thread)
    std::atomic<float> targetAmount  { 1.0f };
    std::atomic<float> targetAttack  { 0.0f };
    std::atomic<float> targetDuck    { 0.0f };
    std::atomic<float> targetSpeedMs { 48.0f };

    // Anzeige-Werte (Message-Thread, in Anzeige-Einheiten)
    std::array<float, static_cast<size_t> (kNumParams)> values { 100.0f, 0.0f, 0.0f, 48.0f };

    // Diagnose (Audio-Thread schreibt, Tests lesen)
    std::atomic<float> diagGain { 1.0f };
    std::atomic<float> diagFast { 0.0f };
    std::atomic<float> diagSlow { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientShaperModule)
};

} // namespace bfka
