#include "fx/modules/EqualizerModule.h"

#include <cmath>

namespace bfka
{

namespace
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;

    constexpr double kMinFreqHz         = 10.0;
    constexpr double kMaxFreqHz         = 20000.0;
    constexpr double kNyquistGuard      = 0.98;
    constexpr double kMinQ              = 0.1;
    constexpr double kMaxQ              = 10.0;
    constexpr double kMinGainDb         = -32.0;
    constexpr double kMaxGainDb         = 32.0;
    constexpr double kFlatEpsilonDb     = 1.0e-4;
    constexpr double kSmoothingMs       = 20.0;   ///< >= 5 ms gefordert
    constexpr double kDynAttackMs       = 10.0;
    constexpr double kDynReleaseMs      = 200.0;
    constexpr double kDynThreshold      = 0.5;    ///< −6 dBFS
    constexpr double kMaxDynReductionDb = 6.0;

    /// Q der klassischen Baender, fuer die es keinen eigenen Regler gibt.
    constexpr double kClassicBellQ  = 0.7;
    constexpr double kClassicShelfQ = 0.70710678118654752440; // 1/sqrt(2), max. Flankensteilheit ohne Ueberschwingen

    constexpr int kLowBand  = 0;
    constexpr int kMidBand  = 1;
    constexpr int kHighBand = 2;

    constexpr int kBandTypeCount = 6;

    bool isGainBandKind (int kind) noexcept
    {
        return kind == (int) EqualizerBandKind::bell
            || kind == (int) EqualizerBandKind::lowShelf
            || kind == (int) EqualizerBandKind::highShelf;
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

EqualizerModule::EqualizerModule()
{
    descriptors.reserve ((size_t) numParams);

    descriptors.push_back ({ "lowgain",  "Low Gain",      -18.0f,    18.0f,     0.0f, "dB" });
    descriptors.push_back ({ "lowfreq",  "Low Freq",       16.0f,   200.0f,   90.0f, "Hz" });
    descriptors.push_back ({ "midgain",  "Mid Gain",      -18.0f,    18.0f,     0.0f, "dB" });
    descriptors.push_back ({ "midfreq",  "Mid Freq",       20.0f,  5000.0f,  900.0f, "Hz" });
    descriptors.push_back ({ "midq",     "Mid Q",           0.1f,    10.0f,     0.9f, "" });
    descriptors.push_back ({ "highgain", "High Gain",     -18.0f,    18.0f,     0.0f, "dB" });
    descriptors.push_back ({ "highfreq", "High Freq",     800.0f, 20000.0f, 8000.0f, "Hz" });
    descriptors.push_back ({ "middyn",   "Mid Dynamic",     0.0f,   100.0f,     0.0f, "%" });

    for (int i = 0; i < numParams; ++i)
    {
        const auto defaultValue = descriptors[(size_t) i].defaultValue;

        paramTargets[i].store (defaultValue, std::memory_order_relaxed);
        paramValues[i] = (double) defaultValue;
    }

    chainBandCount   = applyTargets();
    for (int i = 0; i < chainBandCount; ++i)
        bandCurrent[i] = bandTarget[i];

    bandCountSnapshot.store (chainBandCount, std::memory_order_relaxed);

    appliedBandCount = -1;
    updateCoefficients();
}

// =========================================================================
// FxModule
// =========================================================================

void EqualizerModule::prepare (double newSampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);

    sampleRate = (newSampleRate > 0.0) ? newSampleRate : 48000.0;
    reset();
}

void EqualizerModule::reset()
{
    for (int i = 0; i < numParams; ++i)
        paramValues[i] = (double) paramTargets[i].load (std::memory_order_relaxed);

    adoptSharedBands (true);

    chainBandCount = applyTargets();
    for (int i = 0; i < chainBandCount; ++i)
        bandCurrent[i] = bandTarget[i];

    bandCountSnapshot.store (chainBandCount, std::memory_order_relaxed);

    resetStages();

    appliedBandCount = -1;
    updateCoefficients();
}

void EqualizerModule::setParam (int index, float value)
{
    if (index < 0 || index >= numParams)
        return;

    const auto& desc    = descriptors[(size_t) index];
    const auto  clamped = juce::jlimit (desc.minValue, desc.maxValue, value);

    paramTargets[index].store (clamped, std::memory_order_relaxed);
}

float EqualizerModule::getParam (int index) const
{
    if (index < 0 || index >= numParams)
        return 0.0f;

    return paramTargets[index].load (std::memory_order_relaxed);
}

// =========================================================================
// Zustand (Struktur) — Message-Thread
// =========================================================================

juce::ValueTree EqualizerModule::getState() const
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

void EqualizerModule::setState (const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    EqualizerBand bands[maxBands];
    int           count = 0;

    const int numChildren = state.getNumChildren();

    for (int i = 0; i < numChildren && count < maxBands; ++i)
    {
        const auto child = state.getChild (i);

        if (! isBandChild (child))
            continue;

        EqualizerBand band;

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
        const EqualizerBand band = (i < count) ? bands[i] : EqualizerBand {};

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

void EqualizerModule::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);

    if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0)
        return;

    // ---- 1. Parameter, Bandliste und Koeffizienten nachfuehren (pro Block) ----
    smoothParameters (numSamples);
    adoptSharedBands (false);

    const int targetCount = applyTargets();

    if (targetCount > chainBandCount)
        for (int i = chainBandCount; i < targetCount; ++i)
            bandCurrent[i] = bandTarget[i];

    chainBandCount = targetCount;
    bandCountSnapshot.store (chainBandCount, std::memory_order_relaxed);

    smoothBands (numSamples);
    updateCoefficients();

    int order[maxChainBands] {};
    int stageCount = 0;

    for (int i = 0; i < chainBandCount; ++i)
        if (bandStages[(size_t) i].active)
            order[stageCount++] = i;

    if (stageCount == 0)
        return; // flache Kette: bitidentisches Durchreichen

    float* channels[maxChannels] {};
    for (int ch = 0; ch < numChannels; ++ch)
        channels[ch] = buffer.getWritePointer (ch);

    // Detektor-Koeffizienten der Huelle (10 ms / 200 ms)
    const double attackCoeff  = 1.0 - std::exp (-1.0 / (kDynAttackMs * 0.001 * sampleRate));
    const double releaseCoeff = 1.0 - std::exp (-1.0 / (kDynReleaseMs * 0.001 * sampleRate));

    double work[maxChannels] {};
    double bandOut[maxChannels] {};

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            work[ch] = (double) channels[ch][n];

        for (int k = 0; k < stageCount; ++k)
        {
            auto&       stage = bandStages[(size_t) order[k]];
            const auto& c     = stage.coeffs;

            double peak = 0.0;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const double x  = work[ch];
                auto&        st = stage.state[(size_t) ch];

                // Transposed Direct Form II
                const double y = c.b0 * x + st.z1;
                st.z1 = c.b1 * x - c.a1 * y + st.z2;
                st.z2 = c.b2 * x - c.a2 * y;

                bandOut[ch] = y;
                peak = juce::jmax (peak, std::abs (y));
            }

            if (stage.dynActive)
            {
                const double coeff = (peak > stage.envelope) ? attackCoeff : releaseCoeff;
                stage.envelope += (peak - stage.envelope) * coeff;

                if (stage.envelope < 1.0e-12)
                    stage.envelope = 0.0;

                const double over        = (stage.envelope - kDynThreshold) / kDynThreshold;
                const double levelFactor = juce::jlimit (0.0, 1.0, over);
                const double amount      = levelFactor * stage.dynAmount;
                const double fade        = 1.0 - amount * (1.0 - stage.fadeMin);

                for (int ch = 0; ch < numChannels; ++ch)
                    work[ch] += fade * (bandOut[ch] - work[ch]);
            }
            else
            {
                for (int ch = 0; ch < numChannels; ++ch)
                    work[ch] = bandOut[ch];
            }
        }

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][n] = (float) work[ch];
    }
}

// =========================================================================
// Glaettung
// =========================================================================

double EqualizerModule::smoothingCoefficient (int numSamples, double timeMs) const noexcept
{
    const double sr = (sampleRate > 0.0) ? sampleRate : 48000.0;
    const double dt = (double) juce::jmax (1, numSamples) / sr;
    const double tau = juce::jmax (0.1, timeMs) * 0.001;

    return juce::jlimit (0.0, 1.0, 1.0 - std::exp (-dt / tau));
}

void EqualizerModule::smoothParameters (int numSamples) noexcept
{
    const double coeff = smoothingCoefficient (numSamples, kSmoothingMs);

    for (int i = 0; i < numParams; ++i)
        paramValues[i] = smoothValue (paramValues[i],
                                      (double) paramTargets[i].load (std::memory_order_relaxed),
                                      coeff, 1.0e-6);
}

void EqualizerModule::smoothBands (int numSamples) noexcept
{
    const double coeff = smoothingCoefficient (numSamples, kSmoothingMs);

    for (int i = 0; i < chainBandCount; ++i)
    {
        auto&       current = bandCurrent[(size_t) i];
        const auto& target  = bandTarget[(size_t) i];

        if (current.kind != target.kind)
        {
            // Typwechsel ist keine Zieh-Bewegung: Werte uebernehmen und Stufe
            // saeubern, damit kein Zustand aus dem alten Filter weiterlaeuft.
            current = target;
            for (auto& state : bandStages[(size_t) i].state)
                state.clear();

            bandStages[(size_t) i].envelope = 0.0;
            continue;
        }

        current.freqHz = smoothValue (current.freqHz, target.freqHz, coeff, 1.0e-6);
        current.q      = smoothValue (current.q,      target.q,      coeff, 1.0e-6);
        current.gainDb = smoothValue (current.gainDb, target.gainDb, coeff, 1.0e-6);
        current.dyn    = smoothValue (current.dyn,    target.dyn,    coeff, 1.0e-4);
        current.channel = target.channel;
    }
}

// =========================================================================
// Bandtabelle und Koeffizienten (Audio-Thread)
// =========================================================================

int EqualizerModule::applyTargets() noexcept
{
    // Baender 0…2: die klassischen Regler. Sie sind immer Teil der Kaskade;
    // bei 0 dB Verstaerkung werden sie uebersprungen (Werkszustand = flach).
    bandTarget[kLowBand].kind   = (int) EqualizerBandKind::bell;
    bandTarget[kLowBand].freqHz = paramValues[pLowFreq];
    bandTarget[kLowBand].q      = kClassicBellQ;
    bandTarget[kLowBand].gainDb = paramValues[pLowGain];
    bandTarget[kLowBand].dyn    = 0.0;
    bandTarget[kLowBand].channel = 0;

    bandTarget[kMidBand].kind   = (int) EqualizerBandKind::bell;
    bandTarget[kMidBand].freqHz = paramValues[pMidFreq];
    bandTarget[kMidBand].q      = paramValues[pMidQ];
    bandTarget[kMidBand].gainDb = paramValues[pMidGain];
    bandTarget[kMidBand].dyn    = paramValues[pMidDyn];
    bandTarget[kMidBand].channel = 0;

    bandTarget[kHighBand].kind   = (int) EqualizerBandKind::highShelf;
    bandTarget[kHighBand].freqHz = paramValues[pHighFreq];
    bandTarget[kHighBand].q      = kClassicShelfQ;
    bandTarget[kHighBand].gainDb = paramValues[pHighGain];
    bandTarget[kHighBand].dyn    = 0.0;
    bandTarget[kHighBand].channel = 0;

    // Baender 3…: die freien Baender aus dem Zustand.
    const int userBands = juce::jlimit (0, maxBands, pendingBandCount);

    for (int i = 0; i < userBands; ++i)
        bandTarget[(size_t) (numClassicBands + i)] = pendingBands[(size_t) i];

    return juce::jlimit (numClassicBands, maxChainBands, numClassicBands + userBands);
}

void EqualizerModule::resetStages() noexcept
{
    for (int i = 0; i < maxChainBands; ++i)
    {
        auto& stage = bandStages[(size_t) i];

        stage.coeffs     = BiquadCoeffs {};
        stage.active     = false;
        stage.dynActive  = false;
        stage.dynAmount  = 0.0;
        stage.fadeMin    = 1.0;
        stage.envelope   = 0.0;
        stage.appliedKind   = -1;
        stage.appliedFreqHz = 0.0;
        stage.appliedQ      = 0.0;
        stage.appliedGainDb = 0.0;
        stage.appliedDyn    = 0.0;

        for (auto& state : stage.state)
            state.clear();
    }
}

void EqualizerModule::updateCoefficients() noexcept
{
    bool dirty = (chainBandCount != appliedBandCount);

    if (! dirty)
    {
        for (int i = 0; i < chainBandCount; ++i)
        {
            const auto& stage = bandStages[(size_t) i];
            const auto& band  = bandCurrent[(size_t) i];

            if (stage.appliedKind   != band.kind
                || stage.appliedFreqHz != band.freqHz
                || stage.appliedQ      != band.q
                || stage.appliedGainDb != band.gainDb
                || stage.appliedDyn    != band.dyn)
            {
                dirty = true;
                break;
            }
        }
    }

    if (! dirty)
        return;

    appliedBandCount = chainBandCount;

    for (int i = 0; i < chainBandCount; ++i)
    {
        auto&       stage = bandStages[(size_t) i];
        const auto& band  = bandCurrent[(size_t) i];

        stage.appliedKind   = band.kind;
        stage.appliedFreqHz = band.freqHz;
        stage.appliedQ      = band.q;
        stage.appliedGainDb = band.gainDb;
        stage.appliedDyn    = band.dyn;

        const bool known    = (band.kind >= (int) EqualizerBandKind::bell)
                           && (band.kind <= (int) EqualizerBandKind::notch);
        const bool gainType = known && isGainBandKind (band.kind);
        const double gainDb = gainType ? juce::jlimit (kMinGainDb, kMaxGainDb, band.gainDb) : 0.0;

        stage.active    = known && (gainType ? (std::abs (gainDb) > kFlatEpsilonDb) : true);
        stage.dynActive = false;
        stage.dynAmount = 0.0;
        stage.fadeMin   = 1.0;

        if (! stage.active)
        {
            stage.coeffs   = BiquadCoeffs {};
            stage.envelope = 0.0;

            for (auto& state : stage.state)
                state.clear();

            continue;
        }

        const double freq = juce::jlimit (kMinFreqHz, maxFreqForSampleRate(), band.freqHz);
        const double qq   = juce::jlimit (kMinQ, kMaxQ, band.q);
        const auto   designed = designCoefficients (band.kind, freq, qq, gainDb, sampleRate);

        if (! isValid (designed))
        {
            stage.active = false;
            stage.coeffs = BiquadCoeffs {};
            continue;
        }

        stage.coeffs = designed;

        if (gainType && band.dyn > 0.0)
        {
            stage.dynActive = true;
            stage.dynAmount = juce::jlimit (0.0, 1.0, band.dyn * 0.01);
            stage.fadeMin   = crossfadeMinimum (gainDb);
        }
        else
        {
            stage.envelope = 0.0;
        }
    }
}

double EqualizerModule::maxFreqForSampleRate() const noexcept
{
    const double sr = (sampleRate > 0.0) ? sampleRate : 48000.0;
    const double limit = sr * 0.5 * kNyquistGuard;

    return juce::jmax (kMinFreqHz, juce::jmin (kMaxFreqHz, limit));
}

/**
 * Kleinstes Kreuzblendenverhaeltnis, das die Bandverstaerkung hoechstens bis
 * auf die um 6 dB (bzw. bei kleineren Baendern: ganz) reduzierte Verstaerkung
 * bringt.
 *
 * Das Band wirkt am Zentrum wie der Faktor `G`. Die Blende
 * `out = x + k·(y − x)` ergibt dort `1 + k·(G − 1)`. Damit die Gegenregelung
 * exakt `min(6 dB, |g|)` wegnimmt, wird `k` aus dem Ziel-Gain geloest.
 * Ergebnis liegt immer in [0, 1] (0 = Band ganz aus, 1 = voll wirksam).
 */
double EqualizerModule::crossfadeMinimum (double gainDb) noexcept
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

bool EqualizerModule::isValid (const BiquadCoeffs& c) noexcept
{
    return std::isfinite (c.b0) && std::isfinite (c.b1) && std::isfinite (c.b2)
        && std::isfinite (c.a1) && std::isfinite (c.a2);
}

EqualizerModule::BiquadCoeffs EqualizerModule::designCoefficients (int kind, double freqHz, double q,
                                                                   double gainDb, double sr) noexcept
{
    BiquadCoeffs c;

    if (! (sr > 0.0))
        return c;

    const double nyquist = sr * 0.5;
    const double f     = juce::jlimit (kMinFreqHz, juce::jmin (kMaxFreqHz, nyquist * kNyquistGuard), freqHz);
    const double w0    = kTwoPi * f / sr;
    const double cosw  = std::cos (w0);
    const double sinw  = std::sin (w0);
    const double qq    = juce::jlimit (kMinQ, kMaxQ, q);
    const double alpha = sinw / (2.0 * qq);
    const double A     = std::pow (10.0, juce::jlimit (kMinGainDb, kMaxGainDb, gainDb) / 40.0);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (kind)
    {
        case (int) EqualizerBandKind::bell: // RBJ peakingEQ
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cosw;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha / A;
            break;

        case (int) EqualizerBandKind::lowShelf: // RBJ lowShelf (Q-Variante)
        {
            const double sqrtA  = std::sqrt (A);
            const double twoSqA = 2.0 * sqrtA * alpha;

            b0 = A * ((A + 1.0) - (A - 1.0) * cosw + twoSqA);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosw);
            b2 = A * ((A + 1.0) - (A - 1.0) * cosw - twoSqA);
            a0 = (A + 1.0) + (A - 1.0) * cosw + twoSqA;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw);
            a2 = (A + 1.0) + (A - 1.0) * cosw - twoSqA;
            break;
        }

        case (int) EqualizerBandKind::highShelf: // RBJ highShelf (Q-Variante)
        {
            const double sqrtA  = std::sqrt (A);
            const double twoSqA = 2.0 * sqrtA * alpha;

            b0 = A * ((A + 1.0) + (A - 1.0) * cosw + twoSqA);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw);
            b2 = A * ((A + 1.0) + (A - 1.0) * cosw - twoSqA);
            a0 = (A + 1.0) - (A - 1.0) * cosw + twoSqA;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosw);
            a2 = (A + 1.0) - (A - 1.0) * cosw - twoSqA;
            break;
        }

        case (int) EqualizerBandKind::lowCut: // RBJ highpass (Low Cut)
            b0 = (1.0 + cosw) * 0.5;
            b1 = -(1.0 + cosw);
            b2 = (1.0 + cosw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha;
            break;

        case (int) EqualizerBandKind::highCut: // RBJ lowpass (High Cut)
            b0 = (1.0 - cosw) * 0.5;
            b1 = 1.0 - cosw;
            b2 = (1.0 - cosw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha;
            break;

        case (int) EqualizerBandKind::notch: // RBJ notch
            b0 = 1.0;
            b1 = -2.0 * cosw;
            b2 = 1.0;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha;
            break;

        default:
            return c; // unbekannter Typ -> Identitaet
    }

    if (! (std::abs (a0) > 1.0e-12))
        return c;

    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;

    return c;
}

// =========================================================================
// Snapshot vom Message-Thread (lock-frei)
// =========================================================================

void EqualizerModule::adoptSharedBands (bool force) noexcept
{
    const auto versionBefore = sharedVersion.load (std::memory_order_acquire);

    if (! force && versionBefore == adoptedVersion)
        return;

    EqualizerBand scratch[maxBands];
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
