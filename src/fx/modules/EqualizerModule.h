#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "fx/FxModule.h"

namespace bfka
{

/**
 * Bandtypen des Equalizers — exakt die `kind`-Zahlen des Referenzformats
 * (`<Band kind="…" f="…" q="…" g="…" dyn="…" ch="…"/>`).
 *
 *   0 = BELL      Glocken-/Peaking-Filter (verstaerkt oder absenkend)
 *   1 = L-SHF     Low Shelf (Tiefen-Shelf)
 *   2 = H-SHF     High Shelf (Hoehen-Shelf)
 *   3 = L-CUT     Low Cut  = Hochpass (nimmt Tiefen weg)
 *   4 = H-CUT     High Cut = Tiefpass (nimmt Hoehen weg)
 *   5 = NOTCH     Kerbfilter
 *
 * 0…2 sind aus den Referenzdaten belegt (01-BESTANDSAUFNAHME, 6.2), 3…5 sind
 * aus dem Handbuch abgeleitet. Die numerische Zuordnung von 3…5 ist dort
 * **nicht** belegt — siehe docs/specs/fx-equalizer.md, offener Punkt O1.
 */
enum class EqualizerBandKind : int
{
    bell      = 0,
    lowShelf  = 1,
    highShelf = 2,
    lowCut    = 3,
    highCut   = 4,
    notch     = 5
};

/** Ein EQ-Band in Anzeige-Einheiten (genau die Attribute des Referenz-XML). */
struct EqualizerBand
{
    int    kind   = 0;          ///< `kind`, siehe EqualizerBandKind (0…5)
    double freqHz = 1000.0;     ///< `f`, 10 Hz … 20 kHz (zusaetzlich unter Nyquist geklemmt)
    double q      = 0.9;        ///< `q`, 0.1 … 10
    double gainDb = 0.0;        ///< `g`, −32 … +32 dB (nur BELL/L-SHF/H-SHF wirksam)
    double dyn    = 0.0;        ///< `dyn`, 0 … 100 % (dynamische Gegenregelung)
    int    channel = 0;         ///< `ch`, nur im Mid/Side-Modul bedeutsam (0 = beide)
};

/**
 * Equalizer (`FxType::equalizer`, Referenz-String `"eq"`) — Ticket B-61.
 *
 * ## Aufbau
 *
 * Die Kaskade besteht aus **drei klassischen Stufen** (den Reglern
 * LOW/MID/HIGH) plus **bis zu 12 freien Baendern** aus dem Zustand:
 *
 *   Stufe 0  LOW   BELL      f = lowfreq,  g = lowgain,  Q = 0,7
 *   Stufe 1  MID   BELL      f = midfreq,  g = midgain,  Q = midq, dyn = middyn
 *   Stufe 2  HIGH  H-SHF     f = highfreq, g = highgain, Q = 0,7071
 *   Stufe 3… die Baender aus `setState()` in Reihenfolge
 *
 * Damit sind die klassischen Regler "Teil derselben Kette wie die 12 Baender"
 * (Handbuch) und belegen die Baender 0…2. Ein Band mit 0 dB Verstaerkung wird
 * vollstaendig uebersprungen; sind alle Baender flach (Werkszustand), ist das
 * Modul **bitidentisch** ein Durchreichen.
 *
 * ## DSP
 *
 * Biquad-Kaskade in Transposed Direct Form II mit `double`-Koeffizienten und
 * `double`-Zustaenden, RBJ-Cookbook-Formeln fuer alle sechs Typen. Die
 * Koeffizienten werden hoechstens einmal pro Block neu gesetzt, nie pro Sample.
 * Parameter und Bandwerte werden mit 20 ms (>= der geforderten 5 ms) geglaettet,
 * damit Ziehen nicht klickt.
 *
 * ## `dyn` (dynamische Bandverstaerkung)
 *
 * Huelle des Band-Ausgangs (Attack 10 ms, Release 200 ms, Schwelle −6 dBFS)
 * regelt die Bandwirkung gegen, maximal 6 dB bzw. hoechstens bis auf 0 dB
 * Bandverstaerkung. Die Gegenregelung laeuft als **Kreuzblende** zwischen
 * Bandausgang und Band-Eingang (`x + k·(y − x)`) und ist damit pro Sample
 * stetig — kein Zipper, keine Rueckkopplung, und die Spektralform bleibt
 * erhalten. Nur fuer BELL/L-SHF/H-SHF wirksam (nur dort gibt es eine
 * "Bandverstaerkung").
 *
 * ## Threading
 *
 * `process()` allokiert nicht, loggt nicht, sperrt nicht. `setParam()` schreibt
 * in `std::atomic<float>`. Die Bandliste kommt ueber `setState()` aus dem
 * Message-Thread und wird ueber einen lock-freien Snapshot (Versionzaehler +
 * atomare Felder) uebergeben; der Audio-Thread uebernimmt sie beim naechsten
 * Block.
 *
 * Latenz: 0 Samples (Minimum-Phase).
 */
class EqualizerModule final : public FxModule
{
public:
    /** Nutzerbaender laut Handbuch (Klick in den Graphen). */
    static constexpr int maxBands = 12;
    /** Die drei klassischen Regler belegen die Baender 0…2 der Kette. */
    static constexpr int numClassicBands = 3;
    /** Gesamtzahl der Biquad-Stufen in der Kaskade. */
    static constexpr int maxChainBands = numClassicBands + maxBands;
    /** Hoechstzahl gleichzeitig verarbeiteter Kanaele (Kettensignal ist stereo). */
    static constexpr int maxChannels = 8;

    EqualizerModule();

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override { return FxType::equalizer; }

    const std::vector<FxParamDesc>& params() const noexcept override { return descriptors; }

    void prepare (double newSampleRate, int maxBlockSize) override;
    void reset() override;

    void setParam (int index, float value) override;
    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    int latencySamples() const noexcept override { return 0; }

    juce::String displayName() const override { return "Equalizer"; }

    juce::ValueTree getState() const override;
    void setState (const juce::ValueTree& state) override;

    // --- nur fuer Tests/Diagnose -----------------------------------------
    /** Anzahl der Baender in der aktuellen Kaskade (3 klassische + Nutzerbaender). */
    int activeBandCount() const noexcept { return bandCountSnapshot.load (std::memory_order_relaxed); }

private:
    // ---------------------------------------------------------------------
    // Parameter
    // ---------------------------------------------------------------------
    static constexpr int numParams = 8;

    enum ParamIndex
    {
        pLowGain = 0,
        pLowFreq,
        pMidGain,
        pMidFreq,
        pMidQ,
        pHighGain,
        pHighFreq,
        pMidDyn
    };

    // ---------------------------------------------------------------------
    // Biquad
    // ---------------------------------------------------------------------
    struct BiquadCoeffs
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    struct BiquadState
    {
        double z1 = 0.0, z2 = 0.0;

        void clear() noexcept { z1 = 0.0; z2 = 0.0; }
    };

    /** Laufzeit einer Kaskadenstufe (Audio-Thread). */
    struct BandStage
    {
        BiquadCoeffs coeffs;                                  ///< statisch, pro Block gesetzt
        std::array<BiquadState, (size_t) maxChannels> state {}; ///< TDF2-Zustaende je Kanal

        bool   active    = false;   ///< Stufe wird verarbeitet
        bool   dynActive = false;   ///< dynamische Gegenregelung aktiv
        double dynAmount = 0.0;     ///< dyn/100, geglaettet
        double fadeMin   = 1.0;     ///< kleinster Kreuzblendenfaktor (max. Gegenregelung)
        double envelope  = 0.0;     ///< Huelle des Band-Ausgangs

        // zuletzt angewendete Werte (Aenderungserkennung, kein Neu-Setzen noetig)
        int    appliedKind   = -1;
        double appliedFreqHz = 0.0;
        double appliedQ      = 0.0;
        double appliedGainDb = 0.0;
        double appliedDyn    = 0.0;
    };

    /**
     * Ein Snapshot-Band des Message-Threads.
     *
     * Alle Felder sind einzeln atomar (kein Datenrennen/UB), sodass der
     * Audio-Thread ohne Sperre lesen darf. `double` ist auf x64/MSVC lock-frei
     * und erhaelt die Werte verlustfrei fuer den Zustands-Round-Trip.
     */
    struct SharedBand
    {
        std::atomic<int>    kind { 0 };
        std::atomic<double> freq { 1000.0 };
        std::atomic<double> q { 0.9 };
        std::atomic<double> gainDb { 0.0 };
        std::atomic<double> dyn { 0.0 };
        std::atomic<int>    channel { 0 };
    };

    // --- Helfer (Audio-Thread, allokationsfrei) ---------------------------
    void smoothParameters (int numSamples) noexcept;
    void adoptSharedBands (bool force) noexcept;
    int  applyTargets() noexcept;
    void smoothBands (int numSamples) noexcept;
    void resetStages() noexcept;
    void updateCoefficients() noexcept;

    double smoothingCoefficient (int numSamples, double timeMs) const noexcept;
    double maxFreqForSampleRate() const noexcept;

    static BiquadCoeffs designCoefficients (int kind, double freqHz, double q,
                                            double gainDb, double sampleRate) noexcept;
    static bool isValid (const BiquadCoeffs& c) noexcept;
    static double crossfadeMinimum (double gainDb) noexcept;

    // --- Zustand ----------------------------------------------------------
    double sampleRate = 48000.0;

    std::vector<FxParamDesc> descriptors;

    std::atomic<float> paramTargets[numParams];
    double             paramValues[numParams] {};

    // Snapshot Message-Thread -> Audio-Thread (lock-frei)
    SharedBand            sharedBands[maxBands];
    std::atomic<int>      sharedBandCount { 0 };
    std::atomic<std::uint32_t> sharedVersion { 0 };

    EqualizerBand pendingBands[maxBands] {};
    int           pendingBandCount = 0;
    std::uint32_t adoptedVersion   = 0;

    // Audio-Thread
    EqualizerBand bandTarget[maxChainBands] {};
    EqualizerBand bandCurrent[maxChainBands] {};
    BandStage     bandStages[maxChainBands] {};

    int chainBandCount   = 0;
    int appliedBandCount = -1;

    /** Momentaufnahme fuer `activeBandCount()` (Diagnose aus dem UI-Thread). */
    std::atomic<int> bandCountSnapshot { 0 };
};

} // namespace bfka
