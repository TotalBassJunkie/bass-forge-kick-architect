#pragma once

#include <array>
#include <atomic>
#include <vector>

#include "fx/FxModule.h"
#include "fx/dsp/Biquad.h"

namespace bfka
{

/**
 * Dynamischer Equalizer (`FxType::dynamicEq`, Referenz-String `"dynEq"`).
 *
 * Funktionsintegration des **TDR Nova** (Tokyo Dawn Labs) als eigenstaendiges
 * Rack-Modul (Phase-2-Erweiterung, siehe docs/specs/fx-dynamic-eq.md).
 *
 * ## Funktionsumfang
 *
 * * Eingangs-Hochpass und -Tiefpass, 6 / 12 / 24 / 72 dB/Okt.
 * * Vier parametrische Baender: Low-Shelf / Bell / High-Shelf mit Frequenz
 *   (10 Hz … 20 kHz), Guete und Gain (± 18 dB).
 * * Je Band eine eigene Dynamik (Threshold, Ratio, Attack, Release) mit
 *   den Nova-Betriebsarten Off / On / Sticky.
 * * **SPLIT** je Band: ein = eigener Detektor samt eigener Zeitkonstanten,
 *   aus = das Band folgt dem W-Band-Detektor und dessen Ratio/Zeiten
 *   (verlinkte Betriebsart wie im Original).
 * * **W-Band** (Wideband): Gain ± 12 dB plus eigener Kompressor mit
 *   Threshold/Ratio/Attack/Release (Off/On/Sticky).
 * * **EQ Auto Gain**: langsame Pegelkompensation der statischen EQ-Kurve.
 * * **Dry Mix** (0 % = nass, 100 % = Bypass), **Output Gain**, **Delta**
 *   (Differenzsignal), Prozess-Ziel (Mono/Stereo/Sum/Diff/Left/Right/Dual Mono),
 *   Qualitaetsmodus und Sidechain-Modus (intern / intern flach).
 *
 * ## Architektur-Hinweis
 *
 * Das Original verarbeitet die Baender **parallel** (W-Band = Rest des
 * Spektrums). Dieser Nachbau nutzt eine serielle Biquad-Kaskade plus
 * Wideband-Stufe; die statische EQ-Kurve ist damit identisch, die
 * Band-Interaktionen unterscheiden sich geringfuegig. Diese Abweichung ist
 * bewusst und dokumentiert.
 *
 * ## Threading
 *
 * Wie alle Module: `prepare()`/`reset()`/`setParam()` auf dem Message-Thread,
 * `process()` ohne Allokation/Sperre/Log auf dem Audio-Thread.
 */
class DynamicEqModule final : public FxModule
{
public:
    static constexpr int numBands     = 4;
    static constexpr int maxChannels  = 2;

    // Prozess-Ziel (Nova "Process Target")
    enum class ChannelMode : int
    {
        mono = 0,      ///< Summe auf Mono, Ausgabe doppelt
        stereo,        ///< L/R, Detektoren gekoppelt
        sum,           ///< nur Mittenanteil verarbeiten
        diff,          ///< nur Seitenanteil verarbeiten
        left,          ///< nur linken Kanal verarbeiten
        right,         ///< nur rechten Kanal verarbeiten
        dualMono       ///< L/R unabhaengig (Detektoren entkoppelt)
    };

    DynamicEqModule();

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::dynamicEq; }

    const std::vector<FxParamDesc>& params() const noexcept override { return descriptors; }

    void prepare (double newSampleRate, int maxBlockSize) override;
    void reset() override;

    void setParam (int index, float value) override;
    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    int latencySamples() const noexcept override { return 0; }

    juce::String displayName() const override { return "Dynamic EQ"; }

    // --- Externer Sidechain (B-146) ---------------------------------------
    /**
     * Setzt den Detektor-Block. Wirkt nur, wenn `sidechain` den Wert 1
     * (EXT SC) hat; sonst bleibt der interne Detektor aktiv.
     */
    void setSidechainBuffer (const juce::AudioBuffer<float>* sidechain) noexcept override
    {
        sidechainBuffer = sidechain;
    }

    bool acceptsExternalSidechain() const noexcept override { return true; }

    /** Wurde im letzten Block wirklich der externe Detektor benutzt? */
    bool isUsingExternalSidechain() const noexcept
    {
        return usingExternalSidechain.load (std::memory_order_relaxed);
    }

    /** Sidechain-Modus des letzten Blocks (0 = Int SC, 1 = Ext SC, 2 = Int SC flat). */
    int sidechainMode() const noexcept { return lastSidechainMode.load (std::memory_order_relaxed); }

    // --- Diagnose (Tests / spaetere Anzeige) ------------------------------
    /** Mittlerer dynamischer Regelweg des letzten Blocks in dB (<= 0). */
    float bandDynamicGainDb (int band) const noexcept;

private:
    enum ParamIndex
    {
        pOutGain = 0,
        pDryMix,
        pHpActive,
        pHpFreq,
        pHpType,
        pLpActive,
        pLpFreq,
        pLpType,
        pWideGain,
        pWideDyn,
        pWideThresh,
        pWideRatio,
        pWideAttack,
        pWideRelease,
        pAutoGain,
        pDelta,
        pChannels,
        pQuality,
        pSidechain,
        pBandStart
    };

    enum BandParamIndex
    {
        bpActive = 0,
        bpType,
        bpFreq,
        bpGain,
        bpQ,
        bpDyn,
        bpSplit,
        bpThreshold,
        bpRatio,
        bpAttack,
        bpRelease,
        bpCount
    };

    static constexpr int numParams = pBandStart + numBands * bpCount;

    static int bandParam (int band, BandParamIndex which) noexcept
    {
        return pBandStart + band * bpCount + (int) which;
    }

    /** Filter-Kaskade eines Ein-/Ausgangsfilters (bis 6 Biquads = 72 dB/Okt). */
    struct FilterCascade
    {
        dsp::Biquad stages[6];
        int         stageCount = 0;
    };

    /** Einpoliger 6-dB/Okt-Filter (nur fuer HP/LP mit 6 dB/Okt). */
    struct OnePole
    {
        double state = 0.0;

        void reset() noexcept { state = 0.0; }

        double processLowPass (double x, double coeff) noexcept
        {
            state += coeff * (x - state);
            return state;
        }

        double processHighPass (double x, double coeff) noexcept
        {
            state += coeff * (x - state);
            return x - state;
        }
    };

    struct BandRuntime
    {
        // geglaettete Arbeitswerte
        double freqHz     = 1000.0;
        double q          = 0.9;
        double gainDb     = 0.0;
        double thresholdDb = -20.0;
        double ratio      = 2.0;
        double attackMs   = 10.0;
        double releaseMs  = 100.0;

        int type    = 1;   // 0 Low-Shelf, 1 Bell, 2 High-Shelf
        int active  = 0;   // 0 Off, 1 On, 2 Sticky
        int dyn     = 0;
        int split   = 0;

        dsp::Biquad filter[maxChannels];
        dsp::Biquad detector[maxChannels];
        dsp::EnvelopeFollower follower;

        double heldDynamicDb = 0.0;
        double appliedDynamicDb = 0.0;
    };

    double smoothingCoefficient (int numSamples, double timeMs) const noexcept;

    void  updateFilterCoefficients();
    void  updateHpLpCoefficients();
    void  computeAutoGainTarget();

    static double biquadMagnitudeDb (const dsp::BiquadCoeffs& coeffs, double sampleRateToUse, double freqHz) noexcept;

    // --- Konfiguration ----------------------------------------------------
    double sampleRate   = 48000.0;
    int    maxBlockSize = 512;

    std::vector<FxParamDesc> descriptors;

    std::atomic<float> paramTargets[numParams];
    double             paramValues[numParams] {};

    // --- Signalzustand ----------------------------------------------------
    std::array<FilterCascade, (size_t) maxChannels> hpFilters {};
    std::array<FilterCascade, (size_t) maxChannels> lpFilters {};
    std::array<OnePole, (size_t) maxChannels> hpOnePole {};
    std::array<OnePole, (size_t) maxChannels> lpOnePole {};

    dsp::Biquad sidechainHighPass;
    dsp::EnvelopeFollower wideFollower;

    /** Detektor-Block des externen Sidechains (nur gueltig waehrend `process()`). */
    const juce::AudioBuffer<float>* sidechainBuffer = nullptr;

    /** Kopie des externen Sidechains fuer den laufenden Block (in `prepare()` angelegt). */
    juce::AudioBuffer<float> sidechainCopy;

    std::atomic<bool> usingExternalSidechain { false };
    std::atomic<int>  lastSidechainMode { 0 };

    BandRuntime bands[numBands];

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> workBuffer;

    double wideReductionDb = 0.0;
    double heldWideReductionDb = 0.0;

    double outputGainDb = 0.0;
    double wideGainDb   = 0.0;

    double autoGainDb       = 0.0;
    double autoGainTargetDb = 0.0;

    bool  prepared = false;

    /** Blockweite Mittelwerte des dynamischen Regelwegs je Band. */
    std::atomic<float> lastBandDynamicDb[numBands];
};

} // namespace bfka
