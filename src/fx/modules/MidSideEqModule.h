#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "fx/FxModule.h"
#include "fx/dsp/Biquad.h"

namespace bfka
{

/**
 * Bandtypen des Mid/Side-EQ - dieselben `kind`-Zahlen wie im Referenzformat
 * und im Equalizer-Modul (`<Band kind="…" f="…" q="…" g="…" dyn="…" ch="…"/>`):
 *
 *   0 = BELL      Glocken-/Peaking-Filter (verstaerkt oder absenkend)
 *   1 = L-SHF     Low Shelf (Tiefen-Shelf)
 *   2 = H-SHF     High Shelf (Hoehen-Shelf)
 *   3 = L-CUT     Low Cut  = Hochpass (nimmt Tiefen weg)
 *   4 = H-CUT     High Cut = Tiefpass (nimmt Hoehen weg)
 *   5 = NOTCH     Kerbfilter
 *
 * Die numerische Zuordnung ist aus dem Equalizer-Modul uebernommen
 * (docs/specs/fx-equalizer.md, offener Punkt O1: 3…5 sind dort nicht belegt).
 */
enum class MidSideEqBandKind : int
{
    bell      = 0,
    lowShelf  = 1,
    highShelf = 2,
    lowCut    = 3,
    highCut   = 4,
    notch     = 5
};

/** Ein Band des Mid/Side-EQ in Anzeige-Einheiten (Attribute des Referenz-XML). */
struct MidSideEqBand
{
    int    kind    = 0;        ///< `kind`, siehe MidSideEqBandKind (0…5)
    double freqHz  = 1000.0;   ///< `f`, 10 Hz … 20 kHz (zusaetzlich unter Nyquist geklemmt)
    double q       = 0.9;      ///< `q`, 0,1 … 10
    double gainDb  = 0.0;      ///< `g`, −32 … +32 dB (nur BELL/L-SHF/H-SHF wirksam)
    double dyn     = 0.0;      ///< `dyn`, 0 … 100 %
    int    channel = 0;        ///< `ch`: 0 = Mid, 1 = Side
};

/**
 * Mid/Side-EQ (`FxType::midSideEq`, Referenz-String `"mseq"`) - Ticket B-62.
 *
 * ## Aufbau
 *
 * Das Modul rechnet die Summe in die Mitten-/Seiten-Darstellung um
 * (`M = (L+R)/2`, `S = (L-R)/2`) und filtert beide Anteile getrennt:
 *
 *   Mid-Kette   L-SHF (90 Hz) -> H-SHF (8 kHz) -> Baender mit ch = 0
 *   Side-Kette  L-SHF (120 Hz) -> H-SHF (6 kHz) -> Baender mit ch = 1
 *
 * Die vier Regler `midlow`, `midhigh`, `sidelow`, `sidehigh` sind die Gains
 * dieser festen Shelves (Q = 0,7071); die Baender aus `setState()` kommen
 * dahinter in Dokumentreihenfolge. Ein Band mit 0 dB Verstaerkung wird
 * vollstaendig uebersprungen; ist die ganze Kaskade flach und stehen `width`,
 * Solo und Mute auf Werkszustand, **beruehrt `process()` den Puffer nicht** -
 * bitidentisches Durchreichen.
 *
 * Rueckmatrix mit `w = width/100`:
 *
 *   `L = M + w·S`, `R = M − w·S`     (width 100 % => w = 1, neutral)
 *
 * ## DSP
 *
 * Biquads aus `bfka::dsp` (RBJ-Cookbook, Transposed Direct Form II,
 * `double`-Koeffizienten und -Zustaende). Koeffizienten werden hoechstens
 * einmal pro Block gesetzt, nie pro Sample. Alle Parameter und Bandwerte
 * werden mit 20 ms geglaettet.
 *
 * ## Dynamik (`middyn`, `sidedyn`) und das Band-Attribut `dyn`
 *
 * Huelle des **Ausgangs der jeweiligen Seite** (Attack 10 ms, Release 200 ms,
 * Schwelle −6 dBFS) regelt die Bandverstaerkung dieser Seite gegen: bis
 * hoechstens 0 dB und maximal 6 dB, als Kreuzblende zwischen Band-Ein- und
 * -Ausgang (`x + k·(y − x)`, pro Sample stetig). Wirksam ist je Band
 * `max (Seiten-Regler, Band-`dyn`)/100`.
 *
 * ## Solo/Mute (Matrix)
 *
 *   `midKeep  = (1 − mmute)·(1 − ssolo)`   `sideKeep = (1 − smute)·(1 − msolo)`
 *   `L = midKeep·M + w·sideKeep·S`         `R = midKeep·M − w·sideKeep·S`
 *
 * Damit gilt: `msolo` = nur Mid (S auf 0), `ssolo` = nur Side (M auf 0),
 * `mmute`/`smute` setzen die jeweilige Seite auf 0, und Solo + Mute derselben
 * Seite ergeben 0 auf beiden Seiten.
 *
 * ## Threading
 *
 * `prepare()`/`reset()`/`setParam()` laufen auf dem Message-Thread, `process()`
 * auf dem Audio-Thread. `process()` allokiert nicht, loggt nicht und nimmt
 * keine Sperre. Parameter liegen in `std::atomic<float>`; die Bandliste kommt
 * ueber `setState()` und wird ueber einen lock-freien Snapshot (Versionzaehler
 * + atomare Einzelfelder, `adoptSharedBands`) uebergeben.
 *
 * Latenz: 0 Samples (Minimum-Phase).
 */
class MidSideEqModule final : public FxModule
{
public:
    /** Hoechstzahl der Nutzerbaender (wie beim Equalizer). */
    static constexpr int maxBands = 12;
    /** Hoechstzahl gleichzeitig verarbeiteter Kanaele (nur 0/1 sind L/R). */
    static constexpr int maxChannels = 8;
    /** Feste Shelves je Seite (Low- und High-Shelf). */
    static constexpr int numShelves = 2;
    /** Kaskadenplaetze je Seite: 2 Shelves + 12 Nutzerbaender. */
    static constexpr int maxStagesPerSide = numShelves + maxBands;
    /** Anzahl der Skalar-Parameter. */
    static constexpr int numParams = 11;

    /** Parameterindizes in der Reihenfolge von `params()`. */
    enum ParamIndex : int
    {
        pMidLow    = 0,   ///< "midlow"   -18…18 dB  Low-Shelf der Mid-Seite
        pMidHigh   = 1,   ///< "midhigh"  -18…18 dB  High-Shelf der Mid-Seite
        pSideLow   = 2,   ///< "sidelow"  -18…18 dB  Low-Shelf der Side-Seite
        pSideHigh  = 3,   ///< "sidehigh" -18…18 dB  High-Shelf der Side-Seite
        pWidth     = 4,   ///< "width"      0…200 %  skaliert S vor der Rueckmatrix
        pMidDyn    = 5,   ///< "middyn"     0…100 %  dynamische Gegenregelung Mid
        pSideDyn   = 6,   ///< "sidedyn"    0…100 %  dynamische Gegenregelung Side
        pMidSolo   = 7,   ///< "msolo"      0/1      nur Mid hoerbar
        pSideSolo  = 8,   ///< "ssolo"      0/1      nur Side hoerbar
        pMidMute   = 9,   ///< "mmute"      0/1      Mid stumm
        pSideMute  = 10   ///< "smute"      0/1      Side stumm
    };

    MidSideEqModule();

    // --- FxModule ---------------------------------------------------------
    FxType type() const noexcept override;

    const std::vector<FxParamDesc>& params() const noexcept override;

    void prepare (double newSampleRate, int maxBlockSize) override;

    void reset() override;

    void setParam (int index, float value) override;

    float getParam (int index) const override;

    void process (juce::AudioBuffer<float>& buffer) override;

    int latencySamples() const noexcept override;

    juce::String displayName() const override;

    juce::ValueTree getState() const override;

    void setState (const juce::ValueTree& state) override;

    // --- nur fuer Tests/Diagnose -----------------------------------------
    /** Anzahl der aktuell wirksamen Biquad-Stufen beider Seiten (0 = flach). */
    int activeStageCount() const noexcept;

private:
    /**
     * Laufzeit einer Kaskadenstufe (Audio-Thread).
     *
     * `applied*` sind die zuletzt in Koeffizienten umgesetzten Werte; nur bei
     * Abweichung wird neu entworfen (hoechstens einmal pro Block).
     */
    struct StageRuntime
    {
        dsp::Biquad biquad;

        bool   active    = false;   ///< Stufe ist wirksam
        bool   dynActive = false;   ///< dynamische Gegenregelung aktiv
        double dynAmount = 0.0;     ///< max (Seiten-dyn, Band-dyn)/100 (0…1)
        double fadeMin   = 1.0;     ///< kleinster Kreuzblendenfaktor (max. Gegenregelung)

        int    appliedKind      = -1;
        double appliedFreqHz    = 0.0;
        double appliedQ         = 0.0;
        double appliedGainDb    = 0.0;
        double appliedDynAmount = 0.0;
    };

    /** Eine Seite (Mid oder Side): Zielwerte, geglaettete Werte, Stufen, Huelle. */
    struct SideChain
    {
        MidSideEqBand target[maxStagesPerSide] {};
        MidSideEqBand current[maxStagesPerSide] {};
        StageRuntime  stages[maxStagesPerSide] {};

        int count           = 0;   ///< belegte Plaetze in dieser Kaskade
        int targetCount     = 0;   ///< belegte Plaetze laut Zielaufbau
        int userCount       = 0;   ///< Nutzerbaender dieser Seite (Ist)
        int targetUserCount = 0;   ///< Nutzerbaender dieser Seite (Soll)
        int activeCount     = 0;   ///< wirksame Stufen (Diagnose)

        bool dynActive = false;    ///< mindestens eine Stufe mit dynAmount > 0

        dsp::EnvelopeFollower envelope;   ///< Huelle des Seiten-Ausgangs (fuer dyn)
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
    void smoothBands (int numSamples) noexcept;
    void adoptSharedBands (bool force) noexcept;
    void rebuildTargets() noexcept;
    void updateCoefficients() noexcept;

    double processCascade (SideChain& chain, double input, double level) noexcept;
    double gateValue (int side) const noexcept;
    bool   isNeutral() const noexcept;

    // --- Zustand ----------------------------------------------------------
    double sampleRate = 48000.0;

    std::vector<FxParamDesc> descriptors;

    std::atomic<float> paramTargets[numParams];
    double             paramValues[numParams] {};

    // Snapshot Message-Thread -> Audio-Thread (lock-frei)
    SharedBand                 sharedBands[maxBands];
    std::atomic<int>           sharedBandCount { 0 };
    std::atomic<std::uint32_t> sharedVersion { 0 };

    MidSideEqBand pendingBands[maxBands] {};
    int           pendingBandCount = 0;
    std::uint32_t adoptedVersion   = 0;

    // Audio-Thread; Index 0 = Mid, 1 = Side
    SideChain sideChains[2];

    /** Momentaufnahme fuer `activeStageCount()` (Diagnose aus dem UI-Thread). */
    std::atomic<int> activeStageSnapshot { 0 };
};

} // namespace bfka
