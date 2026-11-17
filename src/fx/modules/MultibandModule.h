#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "fx/FxModule.h"
#include "fx/dsp/Biquad.h"

namespace bfka
{

/**
 * Multiband-Kompressor (`FxType::multiband`, Referenz-String `"ott"`).
 *
 * Der Baustein ist die funktionale Integration des **Xfer Records OTT 1.37**
 * (Multiband Up/Down Compressor) in das Rack, erweitert um die Attribute, die
 * das Referenzformat fuer den `ott`-Typ bereits kennt (THRESHOLD, Band-Gains,
 * Crossovers, LINEAR PHASE).
 *
 * ## Signalfluss
 *
 * ```
 * In Gain
 *   -> 3-Band-Split (Low | Mid | High)
 *        Low/Mid/High Input-Gain
 *        pro Band: Downward- und Upward-Kompressor (stereo-gekoppelt)
 *   -> Summe der Baender -> Ketten-MIX / OUT GAIN
 * ```
 *
 * Der **OUT GAIN** der OTT-Karte ist der `outGain` der Ketten-Karte (wie im
 * Referenzformat); das Modul selbst liefert 100 % nass.
 *
 * ## Crossover
 *
 * * Minimum-Phase: Linkwitz-Riley 4. Ordnung (je zwei kaskadierte
 *   Butterworth-2-Stufen). `cleanXov` schaltet auf LR 8. Ordnung (vier Stufen).
 * * `linear`: linearphasiger FIR-Split (fensterter Sinc, 513 Taps,
 *   Hann-Fenster). Die Baender rekonstruieren das Eingangssignal exakt
 *   (Low + Mid + High == Eingang). Latenz = (Taps-1)/2 = 256 Samples.
 *
 * ## Dynamik
 *
 * * Downward: fester Uebersetzungsfaktor 3:1, Schwelle je Band (`thL/M/H`),
 *   Regelweg `(Pegel - Schwelle) * 2/3`.
 * * Upward: fester Uebersetzungsfaktor 2:1 in Aufwaertsrichtung,
 *   Regelweg `(Schwelle - Pegel) * 1/2`, maximal +24 dB.
 * * `depth` blendet beide Regelwege (0 % = neutral, 100 % = voll),
 *   `upwdStr`/`dnwdStr` skalieren Aufwaerts-/Abwaertsanteil getrennt,
 *   `buL..buH` / `bdL..bdH` schalten die Richtungen je Band ab.
 * * `time` steuert Attack (0,05 … 20 ms) und Release (500 … 20 ms) aller
 *   Baender gemeinsam; wie beim Original laeuft Release gegenlaeufig zu Attack.
 *
 * ## Threading
 *
 * `prepare()`/`reset()`/`setParam()` auf dem Message-Thread, `process()` auf
 * dem Audio-Thread ohne Allokation, ohne Log, ohne Sperre. Parameter werden
 * ueber `std::atomic<float>` uebergeben und pro Block mit 20 ms geglaettet.
 */
class MultibandModule final : public FxModule
{
public:
    static constexpr int numBands    = 3;
    static constexpr int maxChannels = 2;

    /** Taps des linearphasigen FIR-Splits (ungerade). */
    static constexpr int linearPhaseTaps = 513;

    MultibandModule();

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::multiband; }

    const std::vector<FxParamDesc>& params() const noexcept override { return descriptors; }

    void prepare (double newSampleRate, int maxBlockSize) override;
    void reset() override;

    void setParam (int index, float value) override;
    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    int latencySamples() const noexcept override { return reportedLatency; }

    juce::String displayName() const override { return "Multiband"; }

    // --- Diagnose (Tests / spaetere Anzeige) ------------------------------
    /** Mittlerer Regelweg des letzten Blocks in dB (negativ = Absenkung). */
    float bandGainDb (int band) const noexcept;

    /** Aktuell wirksame Crossover-Frequenzen in Hz (geglaettet). */
    double lowCrossoverHz() const noexcept { return lowXOver; }
    double highCrossoverHz() const noexcept { return highXOver; }

private:
    enum ParamIndex
    {
        pDepth = 0,
        pTime,
        pLowXOver,
        pHighXOver,
        pLowGain,
        pMidGain,
        pHighGain,
        pLinear,
        pInGain,
        pCleanXov,
        pThreshLow,
        pThreshMid,
        pThreshHigh,
        pUpBypassLow,
        pUpBypassMid,
        pUpBypassHigh,
        pDnBypassLow,
        pDnBypassMid,
        pDnBypassHigh,
        pUpStrength,
        pDnStrength,
        numParams
    };

    /** Ein Band des minimumphasigen Splits (zwei bzw. vier Stufen je Flanke). */
    struct CrossoverFilters
    {
        dsp::Biquad lowLowPass[4];
        dsp::Biquad lowHighPass[4];
        dsp::Biquad highLowPass[4];
        dsp::Biquad highHighPass[4];
    };

    /** Linearphasiger Split: ein gemeinsamer Verlaufspuffer je Kanal. */
    struct LinearPhaseState
    {
        std::array<std::vector<float>, (size_t) maxChannels> history;
        std::array<int, (size_t) maxChannels> writePos {};
        int taps = linearPhaseTaps;
    };

    void   updateLinearPhaseCoefficients (double lowHz, double highHz);
    void   updateMinimumPhaseCoefficients (double lowHz, double highHz, int stages) noexcept;
    void   resetSignalState() noexcept;

    static std::vector<float> designLowPassFIR (int taps, double cutoffHz, double sampleRate);

    double smoothingCoefficient (int numSamples, double timeMs) const noexcept;

    // --- Konfiguration ----------------------------------------------------
    double sampleRate   = 48000.0;
    int    maxBlockSize = 512;

    std::vector<FxParamDesc> descriptors;

    std::atomic<float> paramTargets[numParams];
    double             paramValues[numParams] {};

    // --- Signalzustand ----------------------------------------------------
    dsp::EnvelopeFollower followers[numBands];
    CrossoverFilters      minPhase[maxChannels];
    LinearPhaseState      linearPhase;

    std::vector<float> firLow;   ///< Tiefpass bei x1
    std::vector<float> firHigh;  ///< Tiefpass bei x2

    double appliedLowXOver  = -1.0;
    double appliedHighXOver = -1.0;
    int    appliedStages    = -1;
    bool   linearMode       = false;

    int reportedLatency = 0;

    // Geglaettete Arbeitswerte
    double lowXOver    = 120.0;
    double highXOver   = 2500.0;
    double depthAmount = 0.5;
    double upStrength  = 0.5;
    double dnStrength  = 0.5;
    double bandInputDb[numBands] {};
    double thresholdsDb[numBands] {};
    double attackMs  = 2.0;
    double releaseMs = 250.0;
    double inputGainDb = 0.0;

    /** Blockweite Mittelwerte des Regelwegs (negativ = Absenkung). */
    std::atomic<float> lastBandGainDb[numBands];
};

} // namespace bfka
