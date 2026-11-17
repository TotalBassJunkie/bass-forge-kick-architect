#include "fx/modules/TransientShaperModule.h"

#include <cmath>
#include <cstddef>

namespace bfka
{

namespace
{
    /** Parametertabelle: Reihenfolge = params()-Reihenfolge = XML-Reihenfolge. */
    const std::vector<FxParamDesc>& descriptorTable()
    {
        static const std::vector<FxParamDesc> table
        {
            { "amount", "Amount",  0.0f,                100.0f,               100.0f,                   "%"  },
            { "attack", "Attack",  0.0f,                100.0f,               0.0f,                     "%"  },
            { "duck",   "Duck",    0.0f,                100.0f,               0.0f,                     "%"  },
            { "speed",  "Speed",   TransientShaperModule::kSpeedMinMs,
                                   TransientShaperModule::kSpeedMaxMs,      48.0f,                    "ms" }
        };

        return table;
    }

    /**
     * Huellkurven-Wert begrenzen: Denormals flushen (verhindert CPU-Einbrueche
     * bei Stille) und nicht-endliche Werte verwerfen (das Modul erholt sich
     * damit selbst, falls ein NaN durch den Puffer kommt).
     */
    inline float sanitizeEnvelope (float value) noexcept
    {
        if (! std::isfinite (value))
            return 0.0f;

        return (std::abs (value) < TransientShaperModule::kDenormalGuard) ? 0.0f : value;
    }
} // namespace

// ---------------------------------------------------------------------------
// Konstruktion
// ---------------------------------------------------------------------------

TransientShaperModule::TransientShaperModule()
{
    const auto& table = descriptorTable();

    for (size_t i = 0; i < values.size() && i < table.size(); ++i)
        values[i] = table[i].defaultValue;

    publishTargets();
    resetInternal();
}

// ---------------------------------------------------------------------------
// Parameter
// ---------------------------------------------------------------------------

const std::vector<FxParamDesc>& TransientShaperModule::params() const noexcept
{
    return descriptorTable();
}

void TransientShaperModule::setParam (int index, float value)
{
    if (index < 0 || index >= static_cast<int> (values.size()))
        return;

    // NaN/Inf nicht uebernehmen (wuerde die Kette vergiften).
    if (! std::isfinite (value))
        return;

    const auto clamped = juce::jlimit (descriptorTable()[static_cast<size_t> (index)].minValue,
                                       descriptorTable()[static_cast<size_t> (index)].maxValue,
                                       value);

    values[static_cast<size_t> (index)] = clamped;
    publishTargets();
}

float TransientShaperModule::getParam (int index) const
{
    if (index < 0 || index >= static_cast<int> (values.size()))
        return 0.0f;

    return values[static_cast<size_t> (index)];
}

void TransientShaperModule::publishTargets() noexcept
{
    targetAmount.store  (values[static_cast<size_t> (idxAmount)] * 0.01f,
                         std::memory_order_relaxed);
    targetAttack.store  (values[static_cast<size_t> (idxAttack)] * 0.01f,
                         std::memory_order_relaxed);
    targetDuck.store    (values[static_cast<size_t> (idxDuck)] * 0.01f,
                         std::memory_order_relaxed);
    targetSpeedMs.store (values[static_cast<size_t> (idxSpeed)],
                         std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Vorbereitung / Reset
// ---------------------------------------------------------------------------

void TransientShaperModule::prepare (double sampleRate, int maxBlockSize)
{
    preparedSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    preparedBlockSize  = juce::jmax (1, maxBlockSize);

    // Enthaelt reset(): Huellkurven und Glaettung starten bei jedem prepare()
    // frisch, damit sich nichts aus einem alten Zustand fortschreibt.
    resetInternal();
}

void TransientShaperModule::reset()
{
    resetInternal();
}

void TransientShaperModule::resetInternal() noexcept
{
    fastEnvelope = 0.0f;
    slowEnvelope = 0.0f;

    // Glaettung auf die aktuellen Ziele setzen: nach prepare()/reset() gibt es
    // keine Einlauf-Rampe, und ein Test mit amount == 0 ist ab dem ersten
    // Sample bitgenau.
    smoothedAmount  = juce::jlimit (0.0f, 1.0f,
                                    targetAmount.load (std::memory_order_relaxed));
    smoothedAttack  = juce::jlimit (0.0f, 1.0f,
                                    targetAttack.load (std::memory_order_relaxed));
    smoothedDuck    = juce::jlimit (0.0f, 1.0f,
                                    targetDuck.load (std::memory_order_relaxed));
    smoothedSpeedMs = juce::jlimit (kSpeedMinMs, kSpeedMaxMs,
                                    targetSpeedMs.load (std::memory_order_relaxed));

    updateCoefficients();

    diagGain.store (1.0f, std::memory_order_relaxed);
    diagFast.store (0.0f, std::memory_order_relaxed);
    diagSlow.store (0.0f, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Koeffizienten und Saettigung
// ---------------------------------------------------------------------------

float TransientShaperModule::onePoleCoefficient (float timeMs, double sampleRate) noexcept
{
    if (timeMs <= 0.0f || sampleRate <= 0.0)
        return 1.0f;

    const auto samples = static_cast<double> (timeMs) * 0.001 * sampleRate;

    // Sehr kurze Zeitkonstanten (unter einem Sample) sind "sofort".
    if (samples <= 1.0)
        return 1.0f;

    return static_cast<float> (1.0 - std::exp (-1.0 / samples));
}

float TransientShaperModule::softLimit (float sample) noexcept
{
    if (! std::isfinite (sample))
        return 0.0f;

    const auto magnitude = std::abs (sample);

    if (magnitude <= kSoftKneeLevel)
        return sample;   // bitgenau: unter 0 dBFS macht die Stufe nichts

    const auto limited = kSoftKneeLevel
                       + std::tanh ((magnitude - kSoftKneeLevel) * kSoftKneeHardness)
                             / kSoftKneeHardness;

    return (sample < 0.0f) ? -limited : limited;
}

void TransientShaperModule::updateCoefficients() noexcept
{
    fastAttackCoeff  = onePoleCoefficient (kFastAttackMs,  preparedSampleRate);
    slowAttackCoeff  = onePoleCoefficient (kSlowAttackMs,  preparedSampleRate);
    slowReleaseCoeff = onePoleCoefficient (kSlowReleaseMs, preparedSampleRate);
    paramSmoothCoeff = onePoleCoefficient (kParamSmoothMs, preparedSampleRate);

    fastReleaseCoeff = onePoleCoefficient (smoothedSpeedMs, preparedSampleRate);
    coeffSpeedMs     = smoothedSpeedMs;
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

void TransientShaperModule::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples  = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    // Parameterziele einmal pro Block lesen (atomar, ohne Sperre).
    const auto newAmount  = juce::jlimit (0.0f, 1.0f,
                                          targetAmount.load (std::memory_order_relaxed));
    const auto newAttack  = juce::jlimit (0.0f, 1.0f,
                                          targetAttack.load (std::memory_order_relaxed));
    const auto newDuck    = juce::jlimit (0.0f, 1.0f,
                                          targetDuck.load (std::memory_order_relaxed));
    const auto newSpeedMs = juce::jlimit (kSpeedMinMs, kSpeedMaxMs,
                                          targetSpeedMs.load (std::memory_order_relaxed));

    // AMOUNT == 0: bitgenaues Durchreichen. Der Puffer wird nicht angefasst
    // (also auch nicht limitiert), und die Huellkurven werden geleert, damit
    // ein erneutes Aufdrehen von AMOUNT nicht mit alter Energie einrastet.
    if (newAmount <= 0.0f && smoothedAmount <= 0.0f)
    {
        fastEnvelope = 0.0f;
        slowEnvelope = 0.0f;

        diagGain.store (1.0f, std::memory_order_relaxed);
        diagFast.store (0.0f, std::memory_order_relaxed);
        diagSlow.store (0.0f, std::memory_order_relaxed);
        return;
    }

    // Release-Zeit des schnellen Followers folgt der geglaetteten SPEED-
    // Einstellung. Neuberechnung nur bei messbarer Aenderung und nur einmal
    // pro Block: ein std::exp pro Block statt pro Sample.
    if (std::abs (smoothedSpeedMs - coeffSpeedMs) > kSpeedRecalcMs)
    {
        fastReleaseCoeff = onePoleCoefficient (smoothedSpeedMs, preparedSampleRate);
        coeffSpeedMs     = smoothedSpeedMs;
    }

    // Koeffizienten und geglaettete Werte in Register-nahe Lokale heben.
    const auto paramCoeff   = paramSmoothCoeff;
    const auto fastAttack   = fastAttackCoeff;
    const auto fastRelease  = fastReleaseCoeff;
    const auto slowAttack   = slowAttackCoeff;
    const auto slowRelease  = slowReleaseCoeff;

    auto amount  = smoothedAmount;
    auto attack  = smoothedAttack;
    auto duck    = smoothedDuck;
    auto speedMs = smoothedSpeedMs;

    auto fast = fastEnvelope;
    auto slow = slowEnvelope;
    auto gain = 1.0f;

    float* const* channelData = buffer.getArrayOfWritePointers();

    const auto attackGainK = kAttackGain;
    const auto duckGainK   = kDuckGain;

    for (int n = 0; n < numSamples; ++n)
    {
        // --- Parameter glaetten (One-Pole, kParamSmoothMs) ------------------
        amount  += (newAmount  - amount)  * paramCoeff;
        attack  += (newAttack  - attack)  * paramCoeff;
        duck    += (newDuck    - duck)    * paramCoeff;
        speedMs += (newSpeedMs - speedMs) * paramCoeff;

        // --- Gemeinsamer Detektor: Maximum der Kanal-Betraege --------------
        // Ein Detektor fuer alle Kanaele haelt die Stereobasis zusammen.
        auto detector = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
            detector = juce::jmax (detector, std::abs (channelData[ch][n]));

        // --- Schneller Follower (Attack 0,5 ms / Release = SPEED) ----------
        fast += (detector - fast) * ((detector > fast) ? fastAttack : fastRelease);
        fast  = sanitizeEnvelope (fast);

        // --- Langsamer Follower (Attack 30 ms / Release 300 ms) ------------
        slow += (detector - slow) * ((detector > slow) ? slowAttack : slowRelease);
        slow  = sanitizeEnvelope (slow);

        // --- Transienten-Anteil --------------------------------------------
        // diff = 1 reiner Einschlag, 0 Dauer/Stille, < 0 Nachschwingen.
        const auto envelopeSum = fast + slow;
        const auto diff = (envelopeSum > kEnvelopeEpsilon)
                            ? (fast - slow) / envelopeSum
                            : 0.0f;

        const auto attackShare  = juce::jmax (0.0f, diff);
        const auto sustainShare = 0.5f * (1.0f - diff);

        // --- Verstaerkung ---------------------------------------------------
        const auto effect = attack * attackGainK * attackShare
                          - duck   * duckGainK   * sustainShare;

        gain = juce::jlimit (kGainMin, kGainMax, 1.0f + amount * effect);

        // --- Anwenden: identische Verstaerkung auf alle Kanaele -------------
        for (int ch = 0; ch < numChannels; ++ch)
            channelData[ch][n] = softLimit (channelData[ch][n] * gain);
    }

    smoothedAmount  = amount;
    smoothedAttack  = attack;
    smoothedDuck    = duck;
    smoothedSpeedMs = speedMs;
    fastEnvelope    = fast;
    slowEnvelope    = slow;

    diagGain.store (gain, std::memory_order_relaxed);
    diagFast.store (fast, std::memory_order_relaxed);
    diagSlow.store (slow, std::memory_order_relaxed);
}

} // namespace bfka
