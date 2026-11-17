#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "fx/FxModule.h"

namespace bfka
{

/**
 * Filters-Modul des FX-Racks (`FxType::filters`, XML-Kennung `"filt"`,
 * Ticket B-60).
 *
 * Klang: ein Filter-Block mit sechs Charakteren und drei stetigen Reglern.
 *
 *   TYPE  0 LP       Tiefpass, echte Flanken 6/12/18/24/30 dB pro Oktave
 *         1 HP       Hochpass, gleiche Flanken
 *         2 BAND     Bandpass, 12 (SLOPE 0) oder 24 dB (SLOPE ab 1)
 *         3 NOTCH    Kerbe, die immer bis zum Boden schneidet
 *         4 PHASER   1…6 Allpass-Kerben um CUTOFF, Polaritaets-Chip
 *         5 COMB     Rueckgekoppelter Kamm, auf CUTOFF gestimmt
 *
 *   SLOPE 0…4  = 6/12/18/24/30 dB pro Oktave; bei BAND nur 0/1, bei PHASER die
 *                Kerbenanzahl (1/2/3/4/6), bei NOTCH die Breite.
 *   CUTOFF    20 Hz … 20 kHz, logarithmisch (auch gesmootht wird logarithmisch).
 *   RESO      0…100 %: gemeinsames resonantes Knie aller Sektionen bzw.
 *                Guete/Schaerfe/Rueckkopplung.
 *   DRIVE     0…100 %: weiche tanh-Saettigung **vor** dem Filter; 0 % ist
 *                bitidentisch zum Eingang (keine Kennlinie).
 *   POLARITY  0 = +, 1 = −; wirkt bei PHASER (Vorzeichen des Nassanteils) und
 *                bei COMB (Vorzeichen der Rueckkopplung). Bei LP/HP/BAND/NOTCH
 *                bleibt das Chip ohne Wirkung (im Handbuch nur fuer
 *                PHASER/COMB beschrieben).
 *
 * Vertrag (docs/specs/fx-chain-engine.md):
 *  - Das Modul bearbeitet die Summe **in-place** und liefert sein zu 100 %
 *    nasses Signal. MIX und OUT GAIN macht die Ketten-Engine.
 *  - `setParam()` schreibt nur Atome (Anzeige-Einheiten, geklemmt);
 *    `process()` liest sie einmal pro Block. Es gibt kein Allokieren, kein
 *    Logging und keinen Lock im Audio-Thread.
 *  - Latenz: 0 (Minimum-Phase, kein Lookahead).
 *
 * Stabilitaets- und Klick-Regeln (Details in docs/specs/fx-filters.md):
 *  - Alle Biquads rechnen in `double` (Transposed Direct Form II), Koeffizienten
 *    werden nur neu berechnet, wenn sich ein geglaetteter Wert messbar bewegt
 *    hat — nie pro Sample.
 *  - CUTOFF/RESO/DRIVE/POLARITY laufen durch ein Ein-Pol-Glaettung
 *    (Zeitkonstante 5 ms), abgetastet auf 32-Sample-Regelbloecken. Der
 *    Verstaerkungsfaktor von DRIVE und das Vorzeichen von POLARITY werden
 *    zusaetzlich linear ueber den Regelblock interpoliert (kein Zipper).
 *  - Typ-/SLOPE-Wechsel sind Topologiewechsel. Sie laufen ueber eine
 *    5-ms-Ausblendung, schalten im Nullpunkt um (Zustand geloescht,
 *    Koeffizienten neu) und blenden 5 ms wieder ein.
 */
class FiltersModule final : public FxModule
{
public:
    FiltersModule();

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::filters; }

    const std::vector<FxParamDesc>& params() const noexcept override { return descriptors; }

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() override;
    void setParam (int index, float value) override;
    float getParam (int index) const override;
    void process (juce::AudioBuffer<float>& buffer) override;
    int latencySamples() const noexcept override { return 0; }   // Minimum-Phase

    juce::String displayName() const override { return "Filters"; }

    // getState()/setState() bleiben absichtlich bei den Defaults der
    // Basisklasse: das Modul hat **keine** Listen- oder Kurvenstruktur. Alle
    // sechs Parameter sind Skalare und laufen vollstaendig ueber `params()` /
    // `setParam()` (bzw. die XML-Attribute `type`, `slope`, `cutoff`, `reso`,
    // `drive`, `polarity`). Ein `<Module>`-ValueTree waere hier leer.

    // --- nur fuer Tests / Diagnose ----------------------------------------
    /** Zuletzt vorbereitete Sample-Rate (0, wenn noch nie `prepare()` lief). */
    double preparedSampleRate() const noexcept { return isPrepared ? sampleRate : 0.0; }

    /** Zuletzt vorbereitete Blockgroesse. */
    int preparedBlockSize() const noexcept { return maxBlockSize; }

    /** Filtertyp, mit dem der DSP gerade rechnet (waehrend einer Blende alt). */
    int activeFilterType() const noexcept { return activeTypeIndex; }

    /** Koeffizienten-Sektionen der aktiven Topologie (0 bei PHASER/COMB). */
    int activeSectionCount() const noexcept { return numSections; }

private:
    // --- Parameter --------------------------------------------------------
    enum ParamIndex
    {
        pType = 0,
        pSlope,
        pCutoff,
        pReso,
        pDrive,
        pPolarity
    };

    static constexpr int kNumParams = 6;

    /** Werte von `type`. */
    enum TypeIndex
    {
        tLowPass = 0,
        tHighPass,
        tBandPass,
        tNotch,
        tPhaser,
        tComb,
        kNumTypes
    };

    // --- Grenzen / Zeitkonstanten ----------------------------------------
    static constexpr int    kMaxSections     = 3;    ///< 5. Ordnung = 2 Biquads + 1 First-Order
    static constexpr int    kMaxPhaserStages = 6;
    static constexpr int    kMaxChannels     = 8;
    static constexpr int    kControlSamples  = 32;   ///< Regelblock fuer Glaettung/Koeffizienten

    static constexpr double kSmoothingSeconds = 0.005;   ///< Zeitkonstante der Glaettung
    static constexpr double kFadeSeconds      = 0.005;   ///< pro Richtung (aus + ein)
    static constexpr double kMinCutoffHz      = 20.0;
    static constexpr double kMaxCutoffHz      = 20000.0;

    static constexpr double kResonanceKneeDb  = 18.0;    ///< Knie-Anhebung bei RESO 100
    static constexpr double kMaxQ             = 12.0;    ///< Stabilitatsdeckel je Biquad
    static constexpr double kMaxDriveGain     = 16.0;    ///< Vorverstaerkung bei DRIVE 100 (+24 dB)
    static constexpr double kMaxCombFeedback  = 0.9;     ///< |g| < 1: Kamm bleibt stabil
    static constexpr double kPhaserSpreadOctaves = 1.0 / 3.0;   ///< Staffelung der Allpaesse

    // --- Koeffizienten/Zustand -------------------------------------------
    /** Biquad-Koeffizienten, a0 schon auf 1 normiert. */
    struct BiquadCoefficients
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    /** Transposed Direct Form II: zwei Verzoegerungsglieder je Kanal und Sektion. */
    struct BiquadState
    {
        double z1 = 0.0, z2 = 0.0;
    };

    /** Blendenphase eines Topologiewechsels. */
    enum class FadePhase
    {
        steady,   ///< keine Blende, Verstaerkung 1
        out,      ///< auf 0 ausblenden (alte Topologie laeuft weiter)
        in        ///< von 0 einblenden (neue Topologie laeuft)
    };

    /** Zielwerte eines Blocks, einmal pro `process()` gelesen. */
    struct Targets
    {
        int    type = tLowPass;
        int    slope = 1;
        double cutoffPosition = 0.0;   ///< 0…1 logarithmisch
        double reso = 0.2;             ///< 0…1
        double drive = 0.0;            ///< 0…1
        double polarity = 1.0;         ///< +1 oder −1
    };

    // --- Parameter (Message-Thread) --------------------------------------
    std::vector<FxParamDesc> descriptors;
    std::vector<float>       values;

    // --- Parameter (Audio-Thread, atomar) --------------------------------
    std::atomic<int>   targetTypeIndex { tLowPass };
    std::atomic<int>   targetSlopeIndex { 1 };
    std::atomic<float> targetCutoffHz { 1000.0f };
    std::atomic<float> targetResoPercent { 20.0f };
    std::atomic<float> targetDrivePercent { 0.0f };
    std::atomic<int>   targetPolarity { 0 };

    /** Hat der Nutzer POLARITY schon selbst gestellt? (dann kein PHASER-Startwert) */
    bool polaritySetByUser = false;

    // --- Laufzeit ---------------------------------------------------------
    double sampleRate     = 48000.0;
    int    maxBlockSize   = 512;
    bool   isPrepared     = false;

    double controlAlpha      = 0.12;   ///< Ein-Pol-Koeffizient je Regelblock
    int    fadeLengthSamples = 240;

    // Aktive Topologie (waehrend einer Ausblendung noch die alte)
    int activeTypeIndex  = tLowPass;
    int activeSlopeIndex = 1;
    int pendingTypeIndex = tLowPass;
    int pendingSlopeIndex = 1;

    // Koeffizienten der aktiven Topologie
    std::array<BiquadCoefficients, kMaxSections> coefficients {};
    int    numSections = 0;

    std::array<double, kMaxPhaserStages> allpassCoefficients {};
    int    numPhaserStages = 0;

    double combDelaySamples = 1.0;
    double combFeedback     = 0.0;

    // Zustand je Kanal
    std::array<std::array<BiquadState, kMaxSections>, kMaxChannels> biquadState {};
    std::array<std::array<double, kMaxPhaserStages>, kMaxChannels>  allpassState {};

    std::vector<float> combBuffer;          ///< kMaxChannels * combBufferLength
    int    combBufferLength = 16;           ///< in prepare() gesetzt
    int    combWriteIndex   = 0;

    // Geglaettete Werte (Audio-Thread)
    double smoothedCutoffPosition = 0.5;
    double smoothedReso           = 0.2;
    double smoothedDrive          = 0.0;
    double smoothedPolarity       = 1.0;

    // Zuletzt fuer die Koeffizienten verwendete Werte
    int    coefficientTypeIndex  = -1;
    int    coefficientSlopeIndex = -1;
    double coefficientCutoff     = -1.0;
    double coefficientReso       = -1.0;

    // Blende
    FadePhase fadePhase     = FadePhase::steady;
    double    fadeGain      = 1.0;   ///< Stand am Anfang des aktuellen Blocks
    double    fadeChunkStart = 1.0;
    double    fadeChunkStep  = 0.0;

    // Drive-Rampe (pro Sample linear, damit die Kennlinie nicht springt)
    double driveGainCurrent = 1.0;
    double driveGainStart   = 1.0;
    double driveGainStep    = 0.0;
    double driveMakeupStart = 1.0;
    double driveMakeupStep  = 0.0;
    bool   driveActive      = false;

    // --- Helfer -----------------------------------------------------------
    void updateTargetsFromDisplay();

    void prepareControlChunk (int numSamples, const Targets& targets);
    void renderChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                      int numChannels);

    void updateCoefficients();
    void clearDspState();

    static double cutoffPositionFromHz (double hz) noexcept;
    static double cutoffHzFromPosition (double position) noexcept;
    static double approach (double current, double target, double alpha) noexcept;

    static BiquadCoefficients makeLowPass1  (double cutoffHz, double fs) noexcept;
    static BiquadCoefficients makeHighPass1 (double cutoffHz, double fs) noexcept;
    static BiquadCoefficients makeLowPass2  (double cutoffHz, double q, double fs) noexcept;
    static BiquadCoefficients makeHighPass2 (double cutoffHz, double q, double fs) noexcept;
    static BiquadCoefficients makeBandPass2 (double cutoffHz, double q, double fs) noexcept;
    static BiquadCoefficients makeNotch2    (double cutoffHz, double q, double fs) noexcept;

    JUCE_DECLARE_NON_COPYABLE (FiltersModule)
};

} // namespace bfka
