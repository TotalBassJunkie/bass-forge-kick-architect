#include "fx/modules/MidSideEqModule.h"

#include <cmath>

namespace bfka
{

namespace
{
    constexpr double kMinFreqHz         = 10.0;
    constexpr double kMaxFreqHz         = 20000.0;
    constexpr double kMinQ              = 0.1;
    constexpr double kMaxQ              = 10.0;
    constexpr double kMinGainDb         = -32.0;
    constexpr double kMaxGainDb         = 32.0;
    constexpr double kFlatEpsilonDb     = 1.0e-6;   ///< |g| darunter: Band aus
    constexpr double kSmoothingMs       = 20.0;     ///< Glaettung aller Parameter/Baender
    constexpr double kDynAttackMs       = 10.0;
    constexpr double kDynReleaseMs      = 200.0;
    constexpr double kDynThreshold      = 0.5;      ///< −6 dBFS
    constexpr double kMaxDynReductionDb = 6.0;
    constexpr double kShelfQ            = 0.70710678118654752440; // 1/sqrt(2)
    constexpr double kNeutralWidth      = 100.0;

    constexpr int kBandTypeCount = 6;

    /** Eckfrequenzen der festen Seiten-Shelves (Index 0 = Mid, 1 = Side). */
    constexpr double kShelfLowHz[2]  = { 90.0, 120.0 };
    constexpr double kShelfHighHz[2] = { 8000.0, 6000.0 };

    /** Zuordnung der Seiten-Parameter (Index 0 = Mid, 1 = Side). */
    constexpr int kLowGainParam[2]     = { MidSideEqModule::pMidLow,   MidSideEqModule::pSideLow };
    constexpr int kHighGainParam[2]    = { MidSideEqModule::pMidHigh,  MidSideEqModule::pSideHigh };
    constexpr int kDynParam[2]         = { MidSideEqModule::pMidDyn,   MidSideEqModule::pSideDyn };
    constexpr int kMuteParam[2]        = { MidSideEqModule::pMidMute,  MidSideEqModule::pSideMute };
    constexpr int kForeignSoloParam[2] = { MidSideEqModule::pSideSolo, MidSideEqModule::pMidSolo };

    bool isKnownKind (int kind) noexcept
    {
        return kind >= 0 && kind < kBandTypeCount;
    }

    /** Nur BELL/Shelves haben eine "Bandverstaerkung" (dyn wirkt dort). */
    bool isGainKind (int kind) noexcept
    {
        return kind == (int) MidSideEqBandKind::bell
            || kind == (int) MidSideEqBandKind::lowShelf
            || kind == (int) MidSideEqBandKind::highShelf;
    }

    dsp::BiquadKind dspKindOf (int kind) noexcept
    {
        switch (kind)
        {
            case (int) MidSideEqBandKind::lowShelf:  return dsp::BiquadKind::lowShelf;
            case (int) MidSideEqBandKind::highShelf: return dsp::BiquadKind::highShelf;
            case (int) MidSideEqBandKind::lowCut:    return dsp::BiquadKind::highPass;
            case (int) MidSideEqBandKind::highCut:   return dsp::BiquadKind::lowPass;
            case (int) MidSideEqBandKind::notch:     return dsp::BiquadKind::notch;
            case (int) MidSideEqBandKind::bell:      return dsp::BiquadKind::peaking;
            default:                                 return dsp::BiquadKind::peaking;
        }
    }

    /** Exponential-Glaettung mit Sprung auf den Zielwert, sobald der Rest unter der Toleranz liegt. */
    double smoothValue (double current, double target, double coeff, double epsilonScale) noexcept
    {
        const double diff  = target - current;
        const double scale = juce::jmax (1.0, std::abs (target));

        if (std::abs (diff) <= epsilonScale * scale)
            return target;

        return current + diff * coeff;
    }

    /** Blockkoeffizient der Glaettung: 1 − exp(−blockLen/(tau·Fs)). */
    double blockSmoothingCoefficient (double sampleRateToUse, int numSamples, double timeMs) noexcept
    {
        const double sr  = (sampleRateToUse > 0.0) ? sampleRateToUse : 48000.0;
        const double dt  = (double) juce::jmax (1, numSamples) / sr;
        const double tau = juce::jmax (0.1, timeMs) * 0.001;

        return juce::jlimit (0.0, 1.0, 1.0 - std::exp (-dt / tau));
    }

    /**
     * Kleinster Kreuzblendenfaktor, der die Bandverstaerkung hoechstens bis auf
     * die um 6 dB (bzw. bei kleineren Baendern: ganz) reduzierte Verstaerkung
     * bringt - dieselbe Loesung wie im Equalizer (docs/specs/fx-equalizer.md, 5).
     */
    double crossfadeMinimum (double gainDb) noexcept
    {
        const double absGain = std::abs (gainDb);

        if (absGain < 1.0e-6)
            return 1.0;

        const double reduced  = juce::jmax (0.0, absGain - juce::jmin (kMaxDynReductionDb, absGain));
        const double targetDb = (gainDb > 0.0) ? reduced : -reduced;

        const double gainBase   = std::pow (10.0, gainDb / 20.0);
        const double gainTarget = std::pow (10.0, targetDb / 20.0);
        const double denom      = gainBase - 1.0;

        if (std::abs (denom) < 1.0e-9)
            return 1.0;

        return juce::jlimit (0.0, 1.0, (gainTarget - 1.0) / denom);
    }

    /** Liest ein Zahlen-Attribut; ValueTrees aus XML tragen Zahlen oft als String. */
    double readDoubleProperty (const juce::var& value, double fallback) noexcept
    {
        if (value.isVoid() || value.isUndefined())
            return fallback;

        if (value.isDouble() || value.isInt() || value.isInt64() || value.isBool())
            return (double) value;

        if (value.isString())
            return value.toString().getDoubleValue();

        return fallback;
    }

    int readIntProperty (const juce::var& value, int fallback) noexcept
    {
        const double number = readDoubleProperty (value, (double) fallback);
        return (int) std::lround (number);
    }

    bool isBandChild (const juce::ValueTree& child)
    {
        return child.getType().toString().equalsIgnoreCase ("Band");
    }
} // namespace

// =========================================================================
// Konstruktion
// =========================================================================

MidSideEqModule::MidSideEqModule()
{
    descriptors.reserve ((size_t) numParams);

    descriptors.push_back ({ "midlow",   "Mid Low",      -18.0f,   18.0f,    0.0f, "dB" });
    descriptors.push_back ({ "midhigh",  "Mid High",     -18.0f,   18.0f,    0.0f, "dB" });
    descriptors.push_back ({ "sidelow",  "Side Low",     -18.0f,   18.0f,    0.0f, "dB" });
    descriptors.push_back ({ "sidehigh", "Side High",    -18.0f,   18.0f,    0.0f, "dB" });
    descriptors.push_back ({ "width",    "Width",          0.0f,  200.0f,  100.0f, "%"  });
    descriptors.push_back ({ "middyn",   "Mid Dynamic",    0.0f,  100.0f,    0.0f, "%"  });
    descriptors.push_back ({ "sidedyn",  "Side Dynamic",   0.0f,  100.0f,    0.0f, "%"  });
    descriptors.push_back ({ "msolo",    "Mid Solo",       0.0f,    1.0f,    0.0f, ""   });
    descriptors.push_back ({ "ssolo",    "Side Solo",      0.0f,    1.0f,    0.0f, ""   });
    descriptors.push_back ({ "mmute",    "Mid Mute",       0.0f,    1.0f,    0.0f, ""   });
    descriptors.push_back ({ "smute",    "Side Mute",      0.0f,    1.0f,    0.0f, ""   });

    for (int i = 0; i < numParams; ++i)
    {
        const auto defaultValue = descriptors[(size_t) i].defaultValue;

        paramTargets[i].store (defaultValue, std::memory_order_relaxed);
        paramValues[i] = (double) defaultValue;
    }

    reset();
}

// =========================================================================
// FxModule
// =========================================================================

FxType MidSideEqModule::type() const noexcept
{
    return FxType::midSideEq;
}

const std::vector<FxParamDesc>& MidSideEqModule::params() const noexcept
{
    return descriptors;
}

void MidSideEqModule::prepare (double newSampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);

    sampleRate = (newSampleRate > 0.0) ? newSampleRate : 48000.0;
    reset();
}

void MidSideEqModule::reset()
{
    // Glaettung auf die aktuellen Ziele setzen: nach prepare()/reset() gibt es
    // keine Einlauf-Rampe, und ein Test mit Werkswerten ist ab dem ersten
    // Sample bitgenau.
    for (int i = 0; i < numParams; ++i)
        paramValues[i] = (double) paramTargets[i].load (std::memory_order_relaxed);

    adoptSharedBands (true);
    rebuildTargets();

    for (auto& chain : sideChains)
    {
        chain.count     = chain.targetCount;
        chain.userCount = chain.targetUserCount;

        for (int i = 0; i < maxStagesPerSide; ++i)
        {
            chain.current[i] = chain.target[i];
            chain.stages[i]  = StageRuntime {};
        }

        chain.envelope.prepare (sampleRate);
        chain.envelope.setTimes (kDynAttackMs, kDynReleaseMs);
        chain.envelope.reset (0.0);
    }

    updateCoefficients();
}

void MidSideEqModule::setParam (int index, float value)
{
    if (index < 0 || index >= numParams)
        return;

    // NaN/Inf nicht uebernehmen (wuerde die Kette vergiften).
    if (! std::isfinite (value))
        return;

    const auto& desc    = descriptors[(size_t) index];
    const auto  clamped = juce::jlimit (desc.minValue, desc.maxValue, value);

    paramTargets[index].store (clamped, std::memory_order_relaxed);
}

float MidSideEqModule::getParam (int index) const
{
    if (index < 0 || index >= numParams)
        return 0.0f;

    return paramTargets[index].load (std::memory_order_relaxed);
}

int MidSideEqModule::latencySamples() const noexcept
{
    return 0;
}

juce::String MidSideEqModule::displayName() const
{
    return "Mid/Side EQ";
}

int MidSideEqModule::activeStageCount() const noexcept
{
    return activeStageSnapshot.load (std::memory_order_relaxed);
}

// =========================================================================
// Zustand (Struktur) — Message-Thread
// =========================================================================

juce::ValueTree MidSideEqModule::getState() const
{
    juce::ValueTree root (juce::Identifier ("Module"));

    const auto count = (int) juce::jlimit (0, maxBands, sharedBandCount.load (std::memory_order_relaxed));

    for (int i = 0; i < count; ++i)
    {
        const auto& band = sharedBands[(size_t) i];

        juce::ValueTree node (juce::Identifier ("Band"));
        node.setProperty (juce::Identifier ("kind"), juce::var (band.kind.load (std::memory_order_relaxed)), nullptr);
        node.setProperty (juce::Identifier ("f"),    juce::var (band.freq.load (std::memory_order_relaxed)), nullptr);
        node.setProperty (juce::Identifier ("q"),    juce::var (band.q.load (std::memory_order_relaxed)), nullptr);
        node.setProperty (juce::Identifier ("g"),    juce::var (band.gainDb.load (std::memory_order_relaxed)), nullptr);
        node.setProperty (juce::Identifier ("dyn"),  juce::var (band.dyn.load (std::memory_order_relaxed)), nullptr);
        node.setProperty (juce::Identifier ("ch"),   juce::var (band.channel.load (std::memory_order_relaxed)), nullptr);

        root.appendChild (node, nullptr);
    }

    return root;
}

void MidSideEqModule::setState (const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    MidSideEqBand bands[maxBands];
    int           count = 0;

    const int numChildren = state.getNumChildren();

    for (int i = 0; i < numChildren && count < maxBands; ++i)
    {
        const auto child = state.getChild (i);

        if (! isBandChild (child))
            continue;

        MidSideEqBand band;

        band.kind    = juce::jlimit (0, kBandTypeCount - 1,
                                     readIntProperty (child.getProperty (juce::Identifier ("kind")), band.kind));
        band.freqHz  = juce::jlimit (kMinFreqHz, kMaxFreqHz,
                                     readDoubleProperty (child.getProperty (juce::Identifier ("f")), band.freqHz));
        band.q       = juce::jlimit (kMinQ, kMaxQ,
                                     readDoubleProperty (child.getProperty (juce::Identifier ("q")), band.q));
        band.gainDb  = juce::jlimit (kMinGainDb, kMaxGainDb,
                                     readDoubleProperty (child.getProperty (juce::Identifier ("g")), band.gainDb));
        band.dyn     = juce::jlimit (0.0, 100.0,
                                     readDoubleProperty (child.getProperty (juce::Identifier ("dyn")), band.dyn));
        band.channel = juce::jlimit (0, 1,
                                     readIntProperty (child.getProperty (juce::Identifier ("ch")), band.channel));

        bands[count++] = band;
    }

    // Der Audio-Thread liest diesen Snapshot nur ueber atomare Felder; die
    // Erhoehung der Version am Ende ist die Freigabe (release).
    for (int i = 0; i < maxBands; ++i)
    {
        const MidSideEqBand band = (i < count) ? bands[i] : MidSideEqBand {};

        sharedBands[(size_t) i].kind.store    (band.kind, std::memory_order_relaxed);
        sharedBands[(size_t) i].freq.store    (band.freqHz, std::memory_order_relaxed);
        sharedBands[(size_t) i].q.store       (band.q, std::memory_order_relaxed);
        sharedBands[(size_t) i].gainDb.store  (band.gainDb, std::memory_order_relaxed);
        sharedBands[(size_t) i].dyn.store     (band.dyn, std::memory_order_relaxed);
        sharedBands[(size_t) i].channel.store (band.channel, std::memory_order_relaxed);
    }

    sharedBandCount.store (count, std::memory_order_relaxed);
    sharedVersion.fetch_add (1, std::memory_order_release);
}

// =========================================================================
// Verarbeitung
// =========================================================================

void MidSideEqModule::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);

    if (numSamples <= 0 || numChannels <= 0 || ! (sampleRate > 0.0))
        return;

    // ---- 1. Parameter, Bandliste und Koeffizienten nachfuehren (pro Block) ----
    smoothParameters (numSamples);
    adoptSharedBands (false);
    rebuildTargets();
    smoothBands (numSamples);
    updateCoefficients();

    // ---- 2. Neutralzustand: bitidentisches Durchreichen ----------------------
    // Keine wirksame Stufe, width = 100 %, kein Solo/Mute -> der Puffer bleibt
    // unberuehrt (nicht einmal die MS-Matrix laeuft).
    if (isNeutral())
        return;

    SideChain& mid  = sideChains[0];
    SideChain& side = sideChains[1];

    const bool   stereo     = (numChannels >= 2);
    const double widthScale = juce::jlimit (0.0, 2.0, paramValues[pWidth] * 0.01);
    const double midGate    = gateValue (0);
    const double sideGate   = gateValue (1);

    float* const left  = buffer.getWritePointer (0);
    float* const right = stereo ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const double inLeft  = (double) left[n];
        const double inRight = (right != nullptr) ? (double) right[n] : 0.0;

        // MS-Matrix (Eingang)
        const double midIn  = stereo ? 0.5 * (inLeft + inRight) : inLeft;
        const double sideIn = stereo ? 0.5 * (inLeft - inRight) : 0.0;

        // Pegelfaktor der Gegenregelung aus der Huelle des Seiten-Ausgangs
        // (ein Sample Verzoegerung, damit kein Kreis ohne Verzoegerung entsteht).
        double midLevel  = 0.0;
        double sideLevel = 0.0;

        if (mid.dynActive)
            midLevel = juce::jlimit (0.0, 1.0,
                                     (mid.envelope.value() - kDynThreshold) / kDynThreshold);

        if (side.dynActive)
            sideLevel = juce::jlimit (0.0, 1.0,
                                      (side.envelope.value() - kDynThreshold) / kDynThreshold);

        // Biquad-Kaskade je Seite
        const double midSignal  = processCascade (mid, midIn, midLevel);
        const double sideSignal = processCascade (side, sideIn, sideLevel);

        if (mid.dynActive)
            mid.envelope.process (midSignal);

        if (side.dynActive)
            side.envelope.process (sideSignal);

        // Rueckmatrix mit Solo/Mute-Toren und Breite
        const double gatedMid  = midGate * midSignal;
        const double gatedSide = sideGate * sideSignal;

        left[n] = (float) (gatedMid + widthScale * gatedSide);

        if (right != nullptr)
            right[n] = (float) (gatedMid - widthScale * gatedSide);
    }
}

/**
 * Verarbeitet ein Sample durch die Kaskade einer Seite.
 *
 * `level` (0…1) ist der Pegelfaktor der Gegenregelung; jedes Band blendet
 * zwischen seinem Eingang `x` und seinem Ausgang `y`:
 * `out = x + k·(y − x)` mit `k = 1 − level·dynAmount·(1 − fadeMin)`.
 */
double MidSideEqModule::processCascade (MidSideEqModule::SideChain& chain,
                                        double input,
                                        double level) noexcept
{
    double signal = input;

    for (int i = 0; i < chain.count; ++i)
    {
        StageRuntime& stage = chain.stages[i];

        if (! stage.active)
            continue;

        const double filtered = stage.biquad.processSampleDouble (signal);

        if (stage.dynActive)
        {
            const double fade = juce::jlimit (0.0, 1.0,
                                              1.0 - level * stage.dynAmount * (1.0 - stage.fadeMin));

            signal += fade * (filtered - signal);
        }
        else
        {
            signal = filtered;
        }
    }

    return signal;
}

// =========================================================================
// Solo/Mute
// =========================================================================

/**
 * Faktor, mit dem der Anteil einer Seite in die Rueckmatrix eingeht.
 *
 * Eigenes Mute und das Solo der **anderen** Seite setzen den Anteil auf 0;
 * damit ergeben Solo und Mute derselben Seite zusammen 0 auf beiden Seiten.
 */
double MidSideEqModule::gateValue (int side) const noexcept
{
    const int index = juce::jlimit (0, 1, side);

    const double mute        = paramValues[kMuteParam[index]];
    const double foreignSolo = paramValues[kForeignSoloParam[index]];

    const double keepOwn     = juce::jlimit (0.0, 1.0, 1.0 - mute);
    const double keepForeign = juce::jlimit (0.0, 1.0, 1.0 - foreignSolo);

    return keepOwn * keepForeign;
}

bool MidSideEqModule::isNeutral() const noexcept
{
    if (sideChains[0].activeCount > 0 || sideChains[1].activeCount > 0)
        return false;

    if (paramValues[pWidth] != kNeutralWidth)
        return false;

    if (gateValue (0) != 1.0 || gateValue (1) != 1.0)
        return false;

    return true;
}

// =========================================================================
// Glaettung
// =========================================================================

void MidSideEqModule::smoothParameters (int numSamples) noexcept
{
    const double coeff = blockSmoothingCoefficient (sampleRate, numSamples, kSmoothingMs);

    for (int i = 0; i < numParams; ++i)
        paramValues[i] = smoothValue (paramValues[i],
                                      (double) paramTargets[i].load (std::memory_order_relaxed),
                                      coeff, 1.0e-6);
}

/**
 * Fuehrt die geglaetteten Bandwerte nach.
 *
 * Die festen Shelves werden immer weich nachgefuehrt; bei den Nutzerbaendern
 * ist ein Strukturwechsel (Anzahl, Typ oder Kanal an einer Position) keine
 * Ziehbewegung: dort werden die Zielwerte uebernommen und der Filterzustand
 * geloescht (dokumentierte Klickmoeglichkeit, siehe Spec O-M6).
 */
void MidSideEqModule::smoothBands (int numSamples) noexcept
{
    const double coeff = blockSmoothingCoefficient (sampleRate, numSamples, kSmoothingMs);

    for (auto& chain : sideChains)
    {
        // 1) Feste Shelves (Plaetze 0 und 1)
        for (int i = 0; i < numShelves; ++i)
        {
            MidSideEqBand&       current = chain.current[i];
            const MidSideEqBand& target  = chain.target[i];

            current.kind    = target.kind;
            current.dyn     = target.dyn;
            current.channel = target.channel;
            current.freqHz  = smoothValue (current.freqHz, target.freqHz, coeff, 1.0e-6);
            current.q       = smoothValue (current.q,      target.q,      coeff, 1.0e-6);
            current.gainDb  = smoothValue (current.gainDb, target.gainDb, coeff, 1.0e-6);
        }

        // 2) Nutzerbaender (Plaetze ab `numShelves`)
        bool structureChanged = (chain.targetUserCount != chain.userCount);

        if (! structureChanged)
        {
            for (int i = numShelves; i < chain.targetCount; ++i)
            {
                if (chain.current[i].kind != chain.target[i].kind
                    || chain.current[i].channel != chain.target[i].channel)
                {
                    structureChanged = true;
                    break;
                }
            }
        }

        if (structureChanged)
        {
            for (int i = numShelves; i < maxStagesPerSide; ++i)
            {
                chain.current[i] = chain.target[i];
                chain.stages[i].biquad.reset();

                chain.stages[i].appliedKind      = -1;   // Koeffizienten neu entwerfen
                chain.stages[i].appliedFreqHz    = 0.0;
                chain.stages[i].appliedQ         = 0.0;
                chain.stages[i].appliedGainDb    = 0.0;
                chain.stages[i].appliedDynAmount = 0.0;
            }
        }
        else
        {
            for (int i = numShelves; i < chain.targetCount; ++i)
            {
                MidSideEqBand&       current = chain.current[i];
                const MidSideEqBand& target  = chain.target[i];

                current.kind    = target.kind;
                current.channel = target.channel;
                current.freqHz  = smoothValue (current.freqHz, target.freqHz, coeff, 1.0e-6);
                current.q       = smoothValue (current.q,      target.q,      coeff, 1.0e-6);
                current.gainDb  = smoothValue (current.gainDb, target.gainDb, coeff, 1.0e-6);
                current.dyn     = smoothValue (current.dyn,    target.dyn,    coeff, 1.0e-4);
            }
        }

        chain.count     = chain.targetCount;
        chain.userCount = chain.targetUserCount;
    }
}

// =========================================================================
// Zielaufbau und Koeffizienten (Audio-Thread)
// =========================================================================

/** Baut die Ziel-Kaskade je Seite: zwei feste Shelves, dann die Baender der Seite. */
void MidSideEqModule::rebuildTargets() noexcept
{
    const int bandCount = juce::jlimit (0, maxBands, pendingBandCount);

    for (int side = 0; side < 2; ++side)
    {
        SideChain& chain = sideChains[side];

        MidSideEqBand& low = chain.target[0];
        low.kind    = (int) MidSideEqBandKind::lowShelf;
        low.freqHz  = kShelfLowHz[side];
        low.q       = kShelfQ;
        low.gainDb  = paramValues[kLowGainParam[side]];
        low.dyn     = 0.0;
        low.channel = side;

        MidSideEqBand& high = chain.target[1];
        high.kind    = (int) MidSideEqBandKind::highShelf;
        high.freqHz  = kShelfHighHz[side];
        high.q       = kShelfQ;
        high.gainDb  = paramValues[kHighGainParam[side]];
        high.dyn     = 0.0;
        high.channel = side;

        int slot = numShelves;

        for (int i = 0; i < bandCount && slot < maxStagesPerSide; ++i)
        {
            const MidSideEqBand& band = pendingBands[i];

            if (band.channel != side)
                continue;

            chain.target[slot] = band;
            ++slot;
        }

        chain.targetCount     = slot;
        chain.targetUserCount = slot - numShelves;
    }
}

/**
 * Setzt die Koeffizienten der Kaskade - hoechstens einmal pro Block und nur,
 * wenn sich ein wirksamer Wert geaendert hat.
 */
void MidSideEqModule::updateCoefficients() noexcept
{
    int totalActive = 0;

    for (int side = 0; side < 2; ++side)
    {
        SideChain&   chain   = sideChains[side];
        const double sideDyn = juce::jlimit (0.0, 100.0, paramValues[kDynParam[side]]);

        int activeCount = 0;
        chain.dynActive = false;

        for (int i = 0; i < chain.count; ++i)
        {
            StageRuntime&        stage = chain.stages[i];
            const MidSideEqBand& band  = chain.current[i];

            const bool   known    = isKnownKind (band.kind);
            const bool   gainKind = known && isGainKind (band.kind);
            const double gainDb   = gainKind ? juce::jlimit (kMinGainDb, kMaxGainDb, band.gainDb) : 0.0;

            const double dynPercent = gainKind
                                        ? juce::jlimit (0.0, 100.0, juce::jmax (sideDyn, band.dyn))
                                        : 0.0;
            const double dynAmount  = juce::jlimit (0.0, 1.0, dynPercent * 0.01);

            // Flache Gain-Baender sind mathematisch die Identitaet -> aus der Kaskade.
            const bool shouldBeActive = known && (gainKind ? (std::abs (gainDb) > kFlatEpsilonDb) : true);

            if (! shouldBeActive)
            {
                if (stage.active)
                    stage.biquad.reset();

                stage.active           = false;
                stage.dynActive        = false;
                stage.dynAmount        = 0.0;
                stage.fadeMin          = 1.0;
                stage.appliedKind      = -1;   // beim Wiedereinschalten neu entwerfen
                stage.appliedFreqHz    = 0.0;
                stage.appliedQ         = 0.0;
                stage.appliedGainDb    = 0.0;
                stage.appliedDynAmount = 0.0;
                continue;
            }

            const bool dirty = (stage.appliedKind      != band.kind)
                            || (stage.appliedFreqHz    != band.freqHz)
                            || (stage.appliedQ         != band.q)
                            || (stage.appliedGainDb    != gainDb)
                            || (stage.appliedDynAmount != dynAmount);

            if (dirty)
            {
                // Klemmen von Frequenz/Q/Flankensteilheit macht `dsp::designBiquad`.
                const dsp::BiquadCoeffs designed = dsp::designBiquad (dspKindOf (band.kind),
                                                                     sampleRate,
                                                                     band.freqHz,
                                                                     band.q,
                                                                     gainDb);

                const bool finite = std::isfinite (designed.b0)
                                 && std::isfinite (designed.b1)
                                 && std::isfinite (designed.b2)
                                 && std::isfinite (designed.a1)
                                 && std::isfinite (designed.a2);

                if (! finite)
                {
                    stage.active    = false;
                    stage.dynActive = false;
                    stage.dynAmount = 0.0;
                    stage.fadeMin   = 1.0;
                    stage.biquad.reset();
                    continue;
                }

                stage.biquad.setCoefficients (designed);

                stage.appliedKind      = band.kind;
                stage.appliedFreqHz    = band.freqHz;
                stage.appliedQ         = band.q;
                stage.appliedGainDb    = gainDb;
                stage.appliedDynAmount = dynAmount;
            }

            stage.active    = true;
            stage.dynAmount = dynAmount;
            stage.fadeMin   = crossfadeMinimum (gainDb);
            stage.dynActive = (dynAmount > 0.0);

            if (stage.dynActive)
                chain.dynActive = true;

            ++activeCount;
        }

        chain.activeCount = activeCount;
        totalActive += activeCount;
    }

    activeStageSnapshot.store (totalActive, std::memory_order_relaxed);
}

// =========================================================================
// Snapshot vom Message-Thread (lock-frei)
// =========================================================================

void MidSideEqModule::adoptSharedBands (bool force) noexcept
{
    const auto versionBefore = sharedVersion.load (std::memory_order_acquire);

    if (! force && versionBefore == adoptedVersion)
        return;

    MidSideEqBand scratch[maxBands];
    const int     count = juce::jlimit (0, maxBands, sharedBandCount.load (std::memory_order_relaxed));

    for (int i = 0; i < maxBands; ++i)
    {
        const auto& src = sharedBands[(size_t) i];

        scratch[i].kind    = src.kind.load (std::memory_order_relaxed);
        scratch[i].freqHz  = src.freq.load (std::memory_order_relaxed);
        scratch[i].q       = src.q.load (std::memory_order_relaxed);
        scratch[i].gainDb  = src.gainDb.load (std::memory_order_relaxed);
        scratch[i].dyn     = src.dyn.load (std::memory_order_relaxed);
        scratch[i].channel = src.channel.load (std::memory_order_relaxed);
    }

    const auto versionAfter = sharedVersion.load (std::memory_order_acquire);

    // Der Schreiber war waehrend des Lesens aktiv -> naechster Block noch einmal.
    if (! force && versionBefore != versionAfter)
        return;

    for (int i = 0; i < maxBands; ++i)
        pendingBands[i] = scratch[i];

    pendingBandCount = count;
    adoptedVersion   = versionAfter;
}

} // namespace bfka
