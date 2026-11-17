#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "fx/FxModule.h"

namespace bfka
{

/**
 * Distortion-Modul des FX-Racks (Ticket B-63).
 *
 * Typ `FxType::distortion`, XML-Kennung `type="dist"`.
 *
 * Aufbau des Signalfugs (alles im Modul, MIX/OUT-GAIN macht die Kette):
 *
 *     Eingang
 *       |-- EQ-Chip PRE   (nur bei eqmode == 1, Tilt, linear, 0 dB Latenz)
 *       |-- 2x-Oversampling
 *       |      |-- shape()  (reine, gedaechtnislose Transferfunktion)
 *       |      |-- DRIVE-Blende:  y = x + drive/100 * (shape(x) - x)
 *       |      |-- AGC    (Huellkurven-Folger, max. +-12 dB Korrektur)
 *       |      |-- Ceiling: hart auf +-1,0
 *       |-- 2x-Downsampling
 *       |-- EQ-Chip POST  (nur bei eqmode == 2)
 *       |-- Ceiling: hart auf +-1,0 (0 dBFS, letzte Instanz)
 *     Ausgang
 *
 * Zentrale Entscheidungen (ausfuehrlich in docs/specs/fx-distortion.md):
 *
 *  - **`shape(x)` ist die Kurve zu 100 %** - ohne DRIVE-Blende, ohne AGC, ohne
 *    EQ, ohne Oversampling. Sie ist oeffentlich und damit direkt testbar. Die
 *    zehn Modi sind gedaechtnislos (kein Zustand ausser der Waveshaper-Kurve).
 *  - **DRIVE ist eine Blende** zwischen trockenem Eingang und Kurve:
 *    `drive = 0` -> Ausgang == Eingang, `drive = 100` -> exakt `shape(x)`.
 *  - **Ceiling**: nach Kurve, AGC und EQ wird hart auf [-1, +1] geklemmt. Kein
 *    True-Peak, kein Lookahead (bewusste Entscheidung, siehe Spec).
 *  - **Oversampling**: immer 2x (auch bei linearen Einstellungen), damit die
 *    Latenz konstant bleibt. Nur so darf der Trockenpfad der Kette fest
 *    kompensiert werden. Die Latenz wird ueber `latencySamples()` gemeldet.
 *  - **Ausnahme fuer `drive == 0`**: dann laeuft der Eingang ueber eine reine
 *    Verzoegerungsleitung um `latencySamples()` statt durch das
 *    Oversampling - so ist "clean" buchstaeblich clean (bitidentisch
 *    verzoegert) und der Nulltest der Kette bleibt erfuellbar.
 *
 * Threading wie im Vertrag (`FxModule.h`): `prepare()`/`reset()`/`setParam()`/
 * `setState()` auf dem Message-Thread, `process()` auf dem Audio-Thread ohne
 * Allokation, ohne Logging und ohne Sperre.
 */
class DistortionModule final : public FxModule
{
public:
    // -----------------------------------------------------------------------
    // Modi
    // -----------------------------------------------------------------------

    /** Die zehn Modi des Distortion-Moduls (Reihenfolge == `mode`-Parameter). */
    enum class Mode : int
    {
        hardClip       = 0, ///< HARD CLIP     - harter Schnitt am Ceiling
        softClip       = 1, ///< SOFT CLIP     - weiche Saettigung, rundes Ceiling
        saturate       = 2, ///< SATURATE      - dickes harmonisches Wachstum
        wavefold       = 3, ///< WAVEFOLD      - Wellenfalter, `folds` = Faltungen
        fuzz           = 4, ///< FUZZ          - gated, zerrissene Textur
        rectify        = 5, ///< RECTIFY       - Vollwellen-Gleichrichtung
        halfWave       = 6, ///< HALF WAVE     - eine Halbwelle, `flip` waehlt welche
        bitcrush       = 7, ///< BITCRUSH      - Amplituden-Quantisierung
        waveshaper     = 8, ///< WAVESHAPER    - freie Kurve fuer beide Haelften
        waveshaperAsym = 9  ///< WAVESHAPER ASYM - je eigene Kurve pro Haelfte
    };

    /** Anzahl der Modi (`mode` laeuft 0 ... 9). */
    static constexpr int kNumModes = 10;

    /**
     * Segmentmodi der Waveshaper-Kurve (Reihenfolge == `seg`-Attribut).
     *
     * Ein Segment zwischen zwei Knoten traegt den Modus und den Biegegrad des
     * **rechten** Knotens ("Biegegriff des Segments davor").
     */
    enum class SegmentMode : int
    {
        linear     = 0, ///< Linear
        curve      = 1, ///< Curve        - `ten` biegt die Gerade
        halfSine   = 2, ///< Half Sine    - sin(t*pi/2)
        sine       = 3, ///< Sine         - S-Kurve 0,5 - 0,5*cos(pi*t)
        doubleWave = 4, ///< Double Wave  - t + 0,25*sin(2*pi*t)
        square     = 5, ///< Square       - Sprung bei `ten`
        steps      = 6, ///< Steps        - Treppen (floor), 4 Stufen
        blocks     = 7  ///< Blocks       - Bloecke (round), 4 Stufen
    };

    /** Anzahl der Segmentmodi (`seg` laeuft 0 ... 7). */
    static constexpr int kNumSegmentModes = 8;

    /**
     * Ein Knoten der Waveshaper-Kurve - genau ein `<WNode/>` im Zustand.
     *
     * `x`/`y` sind auf [-1, +1] beschraenkt, `ten` ist der Biegegrad des
     * Segments davor (0,5 = keine Biegung), `seg` dessen Modus.
     *
     * Die Werte werden als `double` gehalten, damit ein Zustand verlustfrei
     * hin- und hergeschrieben werden kann (das Referenzformat schreibt z. B.
     * `ten="0.95"`; ueber `float` wuerde daraus `0.9499999...`).
     */
    struct WNode
    {
        double x   = 0.0; ///< -1 ... +1
        double y   = 0.0; ///< -1 ... +1
        double ten = 0.5; ///< 0 ... 1 (Biegegrad des Segments davor)
        int    seg = 0;   ///< SegmentMode 0 ... 7
    };

    // -----------------------------------------------------------------------
    // Parameter-Indizes (Reihenfolge von params())
    // -----------------------------------------------------------------------
    static constexpr int paramDrive  = 0;
    static constexpr int paramMode   = 1;
    static constexpr int paramFolds  = 2;
    static constexpr int paramAgc    = 3;
    static constexpr int paramEqMode = 4;
    static constexpr int paramFlip   = 5;
    static constexpr int kNumParams  = 6;

    /** Das Rack ist stereo; groessere Buffer werden nur in den ersten 2 Kanaelen bearbeitet. */
    static constexpr int kMaxProcessingChannels = 2;

    /** Stuetzstellen der gebackenen Wellenformer-Tabellen (linear interpoliert). */
    static constexpr int kCurveTableSize = 2049;

    // -----------------------------------------------------------------------
    // Lebenszyklus
    // -----------------------------------------------------------------------

    DistortionModule();
    ~DistortionModule() override = default;

    // -----------------------------------------------------------------------
    // FxModule
    // -----------------------------------------------------------------------

    FxType type() const noexcept override { return FxType::distortion; }

    const std::vector<FxParamDesc>& params() const noexcept override { return descriptors; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() override;

    void setParam (int index, float value) override;
    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    int latencySamples() const noexcept override { return reportedLatency; }

    juce::String displayName() const override { return "Distortion"; }

    juce::ValueTree getState() const override;
    void setState (const juce::ValueTree& state) override;

    // -----------------------------------------------------------------------
    // Nur fuer Tests, UI (B-92) und Analyse - nicht Teil des FxModule-Vertrags
    // -----------------------------------------------------------------------

    /**
     * Die reine Transferfunktion des aktuell gesetzten Modus.
     *
     * Gedaechtnislos, ohne DRIVE-Blende, ohne AGC, ohne EQ, ohne Oversampling.
     * Liefert immer einen endlichen Wert in [-1, +1].
     */
    float shape (float x) const noexcept;

    /**
     * Wie `shape()` (gleiche Formel, gleiche Tabellen), aber mit explizitem
     * Modus-Index - praktisch fuer Tabellen-Tests ueber alle Modi.
     */
    float shapeForMode (int mode, float x) const noexcept;

    /** Die aktuellen Kurven-Knoten (Reihenfolge wie im Zustand). */
    const std::vector<WNode>& curveNodes() const noexcept { return nodes; }

    /**
     * Ersetzt die Waveshaper-Kurve (Message-Thread; baut die Tabellen neu).
     *
     * Eine leere Liste setzt die Werkskurve. Koordinaten werden auf [-1, +1],
     * `ten` bleibt roh (Referenzdaten enthalten negative Werte), `seg` wird
     * auf 0 ... 7 geklemmt.
     */
    void setCurveNodes (const std::vector<WNode>& newNodes);

    /** Der zuletzt vorbereitete Sample-Rate (Diagnose). */
    double preparedSampleRateHz() const noexcept { return preparedSampleRate; }

private:
    // -----------------------------------------------------------------------
    // Laufzeit-Parameter eines Blocks (einmal pro Block aus den Atomics gelesen)
    // -----------------------------------------------------------------------
    struct Params
    {
        float drive  = 0.0f;
        Mode  mode   = Mode::hardClip;
        float folds  = 2.0f;
        bool  agc    = true;
        int   eqMode = 0;
        bool  flip   = false;
    };

    /** Kanal-Zustand fuer AGC und Tilt-EQ. */
    struct ChannelState
    {
        float agcEnvIn  = 0.0f;
        float agcEnvOut = 0.0f;
        float agcGain   = 1.0f;
        float tiltLow   = 0.0f;
    };

    static Mode toMode (int modeIndex) noexcept;

    Mode  currentMode() const noexcept { return toMode ((int) std::lround (getParam (paramMode))); }
    float currentFolds() const noexcept { return getParam (paramFolds); }
    bool  currentFlip() const noexcept { return getParam (paramFlip) >= 0.5f; }

    /** Blockweise Umsetzung; `numSamples <= preparedBlockSize`. */
    void processChunk (juce::AudioBuffer<float>& buffer,
                       int numChannels,
                       int startSample,
                       int numSamples,
                       const Params& params);

    /** 2x-Oversampling + Kurve + DRIVE-Blende + AGC + Ceiling (in-place). */
    void applyShaping (juce::AudioBuffer<float>& buffer,
                       int numChannels,
                       int startSample,
                       int numSamples,
                       const Params& params);

    /** Ein Sample im ueberabgetasteten Bereich. */
    float processShapedSample (float x, const Params& params, ChannelState& state) const noexcept;

    /** Tilt des EQ-Chips (eqmode PRE/POST). */
    void applyTilt (juce::AudioBuffer<float>& buffer, int numChannels, int startSample, int numSamples);

    /** Fuellt die Verzoegerungsleitung mit dem Eingang (Kontinuitaet beim Umschalten). */
    void feedDelayLine (juce::AudioBuffer<float>& buffer, int numChannels, int startSample, int numSamples);

    /** Reines Durchreichen um `reportedLatency` verzoegert (DRIVE 0). */
    void copyDelayed (juce::AudioBuffer<float>& buffer, int numChannels, int startSample, int numSamples);

    /** Hartes 0-dBFS-Ceiling. */
    void clampRange (juce::AudioBuffer<float>& buffer, int numChannels, int startSample, int numSamples) const noexcept;

    // --- Transferfunktionen -----------------------------------------------
    float shapeImpl (float x, Mode mode, float folds, bool flip) const noexcept;

    // --- Kurven-Interpolation ---------------------------------------------
    void rebuildCurveTables();

    float evalCurvePath (const std::vector<WNode>& path, float x) const noexcept;
    static float evalSegment (const WNode& a, const WNode& b, float t) noexcept;
    static float lookupTable (const std::array<float, kCurveTableSize>& table, float position) noexcept;

    // --- Daten ------------------------------------------------------------
    std::vector<FxParamDesc> descriptors;
    std::array<std::atomic<float>, (size_t) kNumParams> paramValues;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    double preparedSampleRate = 48000.0;
    int    preparedBlockSize  = 512;
    int    reportedLatency    = 0;

    // Waveshaper-Kurve + gebackene Tabellen
    std::vector<WNode> nodes;
    std::array<float, (size_t) kCurveTableSize> symTable {};
    std::array<float, (size_t) kCurveTableSize> asymNegTable {};
    std::array<float, (size_t) kCurveTableSize> asymPosTable {};

    // Verzoegerungsleitung fuer den reinen Durchreich-Fall (DRIVE 0)
    juce::AudioBuffer<float> delayRing;
    int delayWritePos = 0;

    std::array<ChannelState, (size_t) kMaxProcessingChannels> channelState {};

    // Koeffizienten (in prepare() aus der Sample-Rate berechnet)
    float agcAttackCoef  = 0.01f;
    float agcReleaseCoef = 0.0005f;
    float agcSmoothCoef  = 0.005f;
    float tiltCoef       = 0.1f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionModule)
};

} // namespace bfka
