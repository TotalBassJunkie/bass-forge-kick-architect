#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "fx/FxModule.h"

namespace bfka
{

/**
 * Kick Shaper - Ticket B-64, `FxType::kickShaper`, XML-Kennung "shaper".
 *
 * Formt den **Amplitudenverlauf eines Kicks ueber eine Beat-Laenge**: eine
 * Huellkurve aus `<Pt>`-Punkten (Position 0...1 entlang der Kick-Laenge,
 * Ziel-Amplitudenfaktor 0...1) wird bei jedem Trigger von Punkt 0 an in
 * Echtzeit abgefahren; jeder Punkt wirkt als Ziel-Amplitudenfaktor auf das
 * Signal.
 *
 * Aufbau (Formeln, Modi und Konstanten: docs/specs/fx-kick-shaper.md):
 *
 *     Eingang
 *       |-- sPre            linearer Eingangsgain (0...3,5)
 *       |-- Umformung       shaped = x * env(phase)
 *       |                   y = x + a * (shaped - x),  a = amount/100
 *       |-- sPost           linearer Ausgangsgain (0...3,5)
 *     Ausgang
 *
 * Zentrale Entscheidungen:
 *
 *  - **`env` ist ein reiner Amplitudenfaktor (0...1)**, kein Gain in dB. Der
 *    Wert entsteht aus der interpolierten Punktfolge; die Interpolation
 *    zwischen zwei Punkten bestimmt der globale Parameter `shape` (8 Modi),
 *    der Punkt selbst traegt mit `t` einen lokalen Segmenttyp (halten,
 *    gespiegelt, Stufe).
 *  - **`amount = 0` reicht bitgenau durch** (die Umformung wird uebersprungen,
 *    es bleibt nur die Verstaerkung) und die **Default-Huellkurve ist neutral**
 *    (alle `y = 1`): dann gilt `shaped == x` exakt, also ebenfalls bitgenaues
 *    Durchreichen.
 *  - **`sLine >= 0,5` schaltet die Umformung hart ab** (lineare Sicherung,
 *    `amount` wird ignoriert); sPre/sPost wirken weiter.
 *  - **Trigger** ist eine Flanken-Erkennung auf dem Eingangssummenpegel mit
 *    Hysterese (-40 dBFS / -50 dBFS). Bei jedem Trigger startet die Uhr bei
 *    Position 0; Stille re-armiert.
 *  - **Kick-Laenge**: `kDefaultRenderLengthSeconds` = 0,5 s, aenderbar ueber
 *    `setRenderLength (double seconds)`.
 *
 * Latenz: 0 Samples (keine Verzoegerung, kein Lookahead).
 *
 * Threading wie im Kettenvertrag: `prepare()`/`reset()`/`setParam()`/
 * `setState()`/`setEnvelopePoints()`/`setRenderLength()` auf dem
 * Message-Thread, `process()` auf dem Audio-Thread. `process()` allokiert
 * nicht, loggt nicht und nimmt keine Sperre; die Parameterziele liegen in
 * `std::atomic`, geglaettet wird im Audio-Thread.
 */
class KickShaperModule final : public FxModule
{
public:
    /** Parameterindizes in der Reihenfolge von `params()`. */
    enum ParamIndex : int
    {
        idxSPre   = 0,    ///< "sPre"   0...3,5 x   - linearer Eingangsgain
        idxSPost  = 1,    ///< "sPost"  0...3,5 x   - linearer Ausgangsgain
        idxSLine  = 2,    ///< "sLine"  0...1       - 1 = lineare Sicherung (Bypass)
        idxShape  = 3,    ///< "shape"  0...7       - Segmentmodus der Interpolation
        idxAmount = 4,    ///< "amount" 0...100 %   - blendet die Umformung ein
        kNumParams = 5
    };

    /** Anzahl der Segmentmodi (`shape` laeuft 0 ... 7). */
    static constexpr int kNumShapes = 8;

    /** Anzahl der Punkt-Segmenttypen (`t` laeuft 0 ... 3). */
    static constexpr int kNumSegmentTypes = 4;

    /** Obergrenze der Huellkurven-Punkte im Zustand. */
    static constexpr int maxPoints = 32;

    /**
     * Ein Huellkurven-Punkt - genau ein `<Pt/>` im Zustand.
     *
     * `x` = normierte Position 0...1 entlang der Kick-Laenge, `y` =
     * Ziel-Amplitudenfaktor 0...1, `t` = Segmenttyp des Segments **ab diesem
     * Punkt** (Belegung: Spec Abschnitt 6).
     *
     * Die Werte werden als `double` gehalten, damit ein Zustand verlustfrei
     * hin- und hergeschrieben werden kann.
     */
    struct Pt
    {
        double x = 0.0; ///< 0 ... 1
        double y = 1.0; ///< 0 ... 1 (1 = neutral)
        int    t = 0;   ///< Segmenttyp 0 ... 3
    };

    // --- Design-Konstanten (Spec: docs/specs/fx-kick-shaper.md, Abschnitt 3) ---
    static constexpr float  kParamSmoothMs          = 20.0f;  ///< Glaettung von sPre/sPost/amount
    static constexpr float  kTriggerHighLevel       = 0.01f;  ///< -40 dBFS (Trigger-Schwelle)
    static constexpr float  kTriggerLowLevel        = 0.0031623f; ///< -50 dBFS (Hysterese)

    /**
     * Mindestdauer unter der Hysterese-Schwelle, bevor neu getriggert wird.
     *
     * Ohne diese Sperre wuerde ein gehaltenes Signal (z. B. ein Sinus) bei
     * jedem Nulldurchgang unter die Schwelle fallen und die Huellkurve
     * zuruecksetzen — die Umformung waere dann nie zu hoeren.
     */
    static constexpr double kRearmHoldSeconds       = 0.01;   ///< 10 ms Stille vor einem neuen Trigger
    static constexpr double kDefaultRenderLengthSeconds = 0.5;///< Vorgabe fuer die Kick-Laenge
    static constexpr double kMinRenderLengthSeconds = 0.05;   ///< 50 ms
    static constexpr double kMaxRenderLengthSeconds = 10.0;   ///< 10 s
    static constexpr float  kGainMin                = 0.0f;   ///< Bereich von sPre/sPost
    static constexpr float  kGainMax                = 3.5f;
    static constexpr double kApproachConstant       = 3.0;    ///< k der exponentiellen Annaeherung
    static constexpr double kPointEpsilon           = 1.0e-9; ///< entartete x-Positionen

    KickShaperModule();
    ~KickShaperModule() override = default;

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::kickShaper; }

    const std::vector<FxParamDesc>& params() const noexcept override;

    void prepare (double sampleRate, int maxBlockSize) override;

    void reset() override;

    void setParam (int index, float value) override;

    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    /** Latenzfrei (keine Verzoegerung, kein Lookahead). */
    int latencySamples() const noexcept override { return 0; }

    juce::String displayName() const override { return "Kick Shaper"; }

    juce::ValueTree getState() const override;

    void setState (const juce::ValueTree& state) override;

    // -----------------------------------------------------------------------
    // Kick-Laenge und Huellkurve (Message-Thread)
    // -----------------------------------------------------------------------

    /**
     * Setzt die Kick-Laenge in Sekunden (Default 0,5 s).
     *
     * Geklemmt auf `kMinRenderLengthSeconds` ... `kMaxRenderLengthSeconds`;
     * nicht-endliche Werte werden verworfen. Bestimmt, wie schnell die
     * Huellkurven-Uhr von Position 0 nach 1 laeuft.
     */
    void setRenderLength (double seconds);

    /** Zuletzt gesetzte Kick-Laenge in Sekunden. */
    double renderLengthSeconds() const noexcept
    {
        return lengthSeconds.load (std::memory_order_relaxed);
    }

    /**
     * Die aktuellen Huellkurven-Punkte - in der Reihenfolge, in der sie gesetzt
     * wurden (also so, wie `getState()` sie schreibt).
     */
    const std::vector<Pt>& envelopePoints() const noexcept { return points; }

    /**
     * Ersetzt die Huellkurve (Message-Thread; baut die Laufzeit-Kopie neu).
     *
     * Eine leere Liste setzt die Werkskurve. `x`/`y` werden auf 0...1, `t` auf
     * 0...3 geklemmt; mehr als `maxPoints` Punkte werden verworfen. Fuer die
     * Auswertung wird nach `x` sortiert (der Zustand bleibt unsortiert).
     */
    void setEnvelopePoints (const std::vector<Pt>& newPoints);

    // -----------------------------------------------------------------------
    // Hilfsfunktionen (oeffentlich fuer Tests und Dokumentation)
    // -----------------------------------------------------------------------

    /** One-Pole-Koeffizient `1 - exp(-1/(tau*sampleRate))` einer Zeitkonstante in ms. */
    static float onePoleCoefficient (float timeMs, double sampleRate) noexcept;

    /**
     * Huellkurvenwert (0...1) an Position `position` (0...1) fuer den aktuell
     * gesetzten `shape`-Modus. Gedaechtnislos und damit direkt pruefbar.
     */
    float evaluateEnvelope (double position) const noexcept;

    /** Wie `evaluateEnvelope()`, aber mit explizitem `shape`-Modus (0...7). */
    float evaluateEnvelopeForShape (int shapeMode, double position) const noexcept;

    // -----------------------------------------------------------------------
    // Diagnose (nur Tests/Doku; atomar, ohne Sperre)
    // -----------------------------------------------------------------------

    /** Position der Huellkurven-Uhr am Ende des letzten Blocks (0...1). */
    float currentEnvelopePosition() const noexcept
    {
        return diagPosition.load (std::memory_order_relaxed);
    }

    /** Der zuletzt angewendete Huellkurvenwert `env` (0...1). */
    float currentEnvelopeTarget() const noexcept
    {
        return diagEnvelope.load (std::memory_order_relaxed);
    }

    /** Geglaetteter `amount` als 0...1 (1 = volle Umformung). */
    float currentShapeAmount() const noexcept
    {
        return diagAmount.load (std::memory_order_relaxed);
    }

    /** Geglaetteter Eingangsgain (`sPre`). */
    float currentPreGain() const noexcept
    {
        return diagPreGain.load (std::memory_order_relaxed);
    }

    /** Geglaetteter Ausgangsgain (`sPost`). */
    float currentPostGain() const noexcept
    {
        return diagPostGain.load (std::memory_order_relaxed);
    }

    /** Liegt am Blockende ein Trigger an (Pegel ueber der Schwelle)? */
    bool isTriggerActive() const noexcept
    {
        return diagTrigger.load (std::memory_order_relaxed);
    }

    /** Sample-Rate aus dem letzten `prepare()` (Diagnose). */
    double preparedSampleRateHz() const noexcept { return preparedSampleRate; }

    /** Blockgroesse aus dem letzten `prepare()` (Diagnose). */
    int blockSize() const noexcept { return preparedBlockSize; }

private:
    /** Setzt Uhr, Trigger, Glaettung und Koeffizienten auf den Anfangszustand. */
    void resetInternal() noexcept;

    /** Schreibt die Anzeige-Werte als Ziele in die Atomics. */
    void publishTargets() noexcept;

    /** Berechnet die von der Sample-Rate abhaengigen Koeffizienten neu. */
    void updateCoefficients() noexcept;

    /** Baut aus `points` die sortierte, entartungsfreie Laufzeit-Kopie `path`. */
    void rebuildPath();

    /** Interpolationsfunktion `f(u)` eines `shape`-Modus (0...7), u in 0...1. */
    static double shapeBend (int shapeMode, double u) noexcept;

    /** Segmenttyp `t` (0...3) ueber die `shape`-Funktion, u in 0...1. */
    static double segmentBend (int shapeMode, int segmentType, double u) noexcept;

    /** Wert der (sortierten) Punktfolge an Position `position`. */
    float evaluatePath (const std::vector<Pt>& curve, int shapeMode, double position) const noexcept;

    double preparedSampleRate = 48000.0;
    int    preparedBlockSize  = 512;

    float paramSmoothCoeff = 1.0f;

    // Geglaettete Parameter (Audio-Thread)
    float smoothedPreGain  = 1.0f;
    float smoothedPostGain = 1.0f;
    float smoothedAmount   = 1.0f;   ///< 0...1

    // Huellkurven-Uhr und Trigger (Audio-Thread)
    double envelopePhase = 0.0;      ///< 0...1
    bool   triggerActive = false;

    /** Samples unter der Hysterese-Schwelle (Sperre fuer neues Triggern). */
    int    samplesBelowLow = 0;
    int    rearmHoldSamples = 0;     ///< aus `kRearmHoldSeconds` und der Samplerate

    // Parameterziele (Message-/Host-Thread -> Audio-Thread)
    std::atomic<float>  targetPreGain  { 1.0f };
    std::atomic<float>  targetPostGain { 1.0f };
    std::atomic<float>  targetAmount   { 1.0f };
    std::atomic<int>    targetShape    { 0 };
    std::atomic<bool>   targetLinear   { true };   ///< sLine >= 0,5
    std::atomic<double> lengthSeconds  { kDefaultRenderLengthSeconds };

    // Anzeige-Werte (Message-Thread, in Anzeige-Einheiten)
    std::array<float, (size_t) kNumParams> values { 1.0f, 1.0f, 1.0f, 0.0f, 100.0f };

    // Huellkurve: Rohform (Zustand) und sortierte Laufzeit-Kopie
    std::vector<Pt> points;
    std::vector<Pt> path;

    // Diagnose (Audio-Thread schreibt, Tests lesen)
    std::atomic<float> diagPosition  { 0.0f };
    std::atomic<float> diagEnvelope  { 1.0f };
    std::atomic<float> diagAmount    { 1.0f };
    std::atomic<float> diagPreGain   { 1.0f };
    std::atomic<float> diagPostGain  { 1.0f };
    std::atomic<bool>  diagTrigger   { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickShaperModule)
};

} // namespace bfka
