#include "fx/modules/FinalizerModule.h"

#include <algorithm>
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
            { "boost",   "Sub Boost",   0.0f,  12.0f,   0.0f, "dB" },
            { "freq",    "Sub Freq",   20.0f,  200.0f,  50.0f, "Hz" },
            { "clip",    "Clip",        0.0f,  100.0f,   0.0f, "%"  },
            { "upward",  "Upward",      0.0f,  100.0f,   0.0f, "%"  },
            { "monoF",   "Mono Freq",  60.0f,  500.0f, 120.0f, "Hz" },
            { "shelfOn", "Shelf On",    0.0f,    1.0f,   1.0f, ""   },
            { "monoOn",  "Mono On",     0.0f,    1.0f,   1.0f, ""   },
            { "clipOn",  "Clip On",     0.0f,    1.0f,   1.0f, ""   }
        };

        return table;
    }
} // namespace

// ---------------------------------------------------------------------------
// Konstruktion
// ---------------------------------------------------------------------------

FinalizerModule::FinalizerModule()
{
    const auto& table = descriptorTable();

    for (size_t i = 0; i < (size_t) numParams && i < table.size(); ++i)
        paramTargets[i].store (table[i].defaultValue, std::memory_order_relaxed);

    syncSmoothedToTargets();
    resetSignalState();
}

// ---------------------------------------------------------------------------
// Parameter
// ---------------------------------------------------------------------------

const std::vector<FxParamDesc>& FinalizerModule::params() const noexcept
{
    return descriptorTable();
}

void FinalizerModule::setParam (int index, float value)
{
    if (index < 0 || index >= (int) numParams)
        return;

    // NaN/Inf nicht uebernehmen (wuerde die Kette vergiften).
    if (! std::isfinite (value))
        return;

    const auto& desc = descriptorTable()[(size_t) index];

    paramTargets[(size_t) index].store (juce::jlimit (desc.minValue, desc.maxValue, value),
                                        std::memory_order_relaxed);
}

float FinalizerModule::getParam (int index) const
{
    if (index < 0 || index >= (int) numParams)
        return 0.0f;

    return paramTargets[(size_t) index].load (std::memory_order_relaxed);
}

void FinalizerModule::syncSmoothedToTargets() noexcept
{
    const auto read = [this] (int index)
    {
        return (double) paramTargets[(size_t) index].load (std::memory_order_relaxed);
    };

    const bool shelfOn = read ((int) pShelfOn) >= 0.5;
    const bool monoOn  = read ((int) pMonoOn)  >= 0.5;
    const bool clipOn  = read ((int) pClipOn)  >= 0.5;

    const double clipRaw   = read ((int) pClip);
    const double upwardRaw = read ((int) pUpward);

    smoothedBoostDb      = read ((int) pBoost);
    smoothedShelfHz      = read ((int) pFreq);
    smoothedMonoHz       = read ((int) pMonoFreq);
    smoothedShelfMix     = shelfOn ? 1.0 : 0.0;
    smoothedMonoMix      = monoOn ? 1.0 : 0.0;
    smoothedClipAmount   = (clipOn && clipRaw > 0.0)
                               ? juce::jlimit (0.0, 1.0, clipRaw * 0.01)
                               : 0.0;
    smoothedUpwardAmount = juce::jlimit (0.0, 1.0, upwardRaw * 0.01);

    const bool engaged = shelfOn || monoOn
                         || smoothedClipAmount > 0.0
                         || smoothedUpwardAmount > 0.0;

    smoothedEngage = engaged ? 1.0 : 0.0;
}

// ---------------------------------------------------------------------------
// Vorbereitung / Reset
// ---------------------------------------------------------------------------

int FinalizerModule::lookaheadSamplesFor (double sampleRateToUse) noexcept
{
    const double rate = (sampleRateToUse > 0.0) ? sampleRateToUse : 48000.0;

    return juce::jmax (1, (int) std::ceil (kLookaheadSeconds * rate));
}

void FinalizerModule::prepare (double newSampleRate, int newMaxBlockSize)
{
    preparedSampleRate = (newSampleRate > 0.0) ? newSampleRate : 48000.0;
    preparedBlockSize  = juce::jmax (1, newMaxBlockSize);

    // Latenz ist die Lookahead-Laenge; sie haengt nur an der Sample-Rate.
    lookahead       = lookaheadSamplesFor (preparedSampleRate);
    reportedLatency = lookahead;

    paramSmoothCoeff    = 1.0 - std::exp (-1.0 / ((double) kParamSmoothMs * 0.001 * preparedSampleRate));
    limiterReleaseCoeff = (float) (1.0 - std::exp (-1.0 / ((double) kLimiterReleaseMs * 0.001 * preparedSampleRate)));

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        delayLine[(size_t) ch].assign ((size_t) lookahead, 0.0f);

        subsonic[ch].setCoefficients (dsp::designBiquad (dsp::BiquadKind::highPass,
                                                         preparedSampleRate,
                                                         (double) kSubsonicHz,
                                                         (double) kSubsonicQ,
                                                         0.0));
    }

    // Ein Eintrag mehr als der Lookahead: das Fenster enthaelt das Sample, das
    // gerade ausgegeben wird, und die `lookahead` Samples danach.
    gainRing.assign ((size_t) lookahead + 1, 1.0f);

    upwardFollower.prepare (preparedSampleRate);
    upwardFollower.setTimes ((double) kUpwardAttackMs, (double) kUpwardReleaseMs);

    appliedBoostDb = -1.0e9;
    appliedShelfHz = -1.0;
    appliedMonoHz  = -1.0;

    syncSmoothedToTargets();
    updateFilterCoefficients();
    resetSignalState();
}

void FinalizerModule::reset()
{
    resetSignalState();
}

void FinalizerModule::resetSignalState() noexcept
{
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        subsonic[ch].reset();
        shelf[ch].reset();

        for (int stage = 0; stage < 2; ++stage)
        {
            monoLowPass[ch][stage].reset();
            monoHighPass[ch][stage].reset();
        }

        auto& line = delayLine[(size_t) ch];
        std::fill (line.begin(), line.end(), 0.0f);

        peakHistory[(size_t) ch] = { 0.0f, 0.0f };
    }

    upwardFollower.reset();

    std::fill (gainRing.begin(), gainRing.end(), 1.0f);

    delayPos    = 0;
    gainPos     = 0;
    limiterGain = 1.0f;

    diagUpwardGainDb.store (0.0f, std::memory_order_relaxed);
    diagLimiterGain.store (1.0f, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Koeffizienten
// ---------------------------------------------------------------------------

void FinalizerModule::updateFilterCoefficients() noexcept
{
    // Shelf: Boost und Frequenz kommen geglaettet herein; neu gesetzt wird nur
    // bei messbarer Aenderung und nur einmal pro Block (Aufruf aus process()).
    const double shelfHz = juce::jlimit (20.0, 200.0, smoothedShelfHz);

    if (std::abs (shelfHz - appliedShelfHz) > (double) kCoeffEpsilon
        || std::abs (smoothedBoostDb - appliedBoostDb) > (double) kCoeffEpsilon)
    {
        const auto coeffs = dsp::designBiquad (dsp::BiquadKind::lowShelf,
                                              preparedSampleRate,
                                              shelfHz,
                                              (double) kShelfQ,
                                              smoothedBoostDb);

        for (int ch = 0; ch < maxChannels; ++ch)
            shelf[ch].setCoefficients (coeffs);

        appliedShelfHz = shelfHz;
        appliedBoostDb = smoothedBoostDb;
    }

    // Mono-Bass: LR4 = zwei kaskadierte Butterworth-2-Filter je Flanke.
    const double monoHz = juce::jlimit (60.0, 500.0, smoothedMonoHz);

    if (std::abs (monoHz - appliedMonoHz) > (double) kCoeffEpsilon)
    {
        const auto lowCoeffs  = dsp::designBiquad (dsp::BiquadKind::lowPass,
                                                  preparedSampleRate, monoHz,
                                                  (double) kCrossoverQ, 0.0);
        const auto highCoeffs = dsp::designBiquad (dsp::BiquadKind::highPass,
                                                  preparedSampleRate, monoHz,
                                                  (double) kCrossoverQ, 0.0);

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            for (int stage = 0; stage < 2; ++stage)
            {
                monoLowPass[ch][stage].setCoefficients (lowCoeffs);
                monoHighPass[ch][stage].setCoefficients (highCoeffs);
            }
        }

        appliedMonoHz = monoHz;
    }
}

// ---------------------------------------------------------------------------
// True-Peak-Erkennung (4x-Oversampling)
// ---------------------------------------------------------------------------

float FinalizerModule::oversampledPeak (int channel, float sample) noexcept
{
    auto& history = peakHistory[(size_t) channel];

    const double p0 = (double) history[1];   // x[n-2]
    const double p1 = (double) history[0];   // x[n-1]
    const double p2 = (double) sample;       // x[n]
    const double p3 = 2.0 * p2 - p1;         // extrapolierter Stuetzpunkt x[n+1]

    double peak = std::abs (p2);

    // Catmull-Rom zwischen p1 (t = 0) und p2 (t = 1); die drei Zwischenwerte
    // sind die 4x-oversampelten Punkte des Segments.
    for (int step = 1; step < oversamplingFactor; ++step)
    {
        const double t  = (double) step / (double) oversamplingFactor;
        const double t2 = t * t;
        const double t3 = t2 * t;

        const double value = 0.5 * ((2.0 * p1)
                                    + (-p0 + p2) * t
                                    + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
                                    + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);

        peak = juce::jmax (peak, std::abs (value));
    }

    history[1] = history[0];
    history[0] = sample;

    return (float) peak;
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

void FinalizerModule::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);

    if (numSamples <= 0 || numChannels <= 0 || lookahead <= 0 || preparedSampleRate <= 0.0)
        return;

    // --- Ziele einmal pro Block lesen (atomar, ohne Sperre) ---------------
    const auto readTarget = [this] (int index)
    {
        return paramTargets[(size_t) index].load (std::memory_order_relaxed);
    };

    const double targetBoost    = (double) readTarget ((int) pBoost);
    const double targetShelfHz  = (double) readTarget ((int) pFreq);
    const double targetClip     = (double) readTarget ((int) pClip);
    const double targetUpward   = (double) readTarget ((int) pUpward);
    const double targetMonoHz   = (double) readTarget ((int) pMonoFreq);

    const bool shelfOn = readTarget ((int) pShelfOn) >= 0.5f;
    const bool monoOn  = readTarget ((int) pMonoOn)  >= 0.5f;
    const bool clipOn  = readTarget ((int) pClipOn)  >= 0.5f;

    const bool clipEngaged = clipOn && targetClip > 0.0;
    const bool monoEngaged = monoOn && numChannels > 1;
    const bool engaged     = shelfOn || monoEngaged || clipEngaged || targetUpward > 0.0;

    const double clipTarget   = clipEngaged
                                    ? juce::jlimit (0.0, 1.0, targetClip * 0.01)
                                    : 0.0;
    const double upwardTarget = juce::jlimit (0.0, 1.0, targetUpward * 0.01);

    const double engageTarget   = engaged ? 1.0 : 0.0;
    const double shelfMixTarget = shelfOn ? 1.0 : 0.0;
    const double monoMixTarget  = monoEngaged ? 1.0 : 0.0;

    // Filterkoeffizienten hoechstens einmal pro Block neu setzen.
    updateFilterCoefficients();

    // Ein Mono-Eingang braucht keinen Mono-Bass; die Einblendung wird ohne
    // Rampe abgeschaltet, damit ein Kanalwechsel keinen Ausschlag erzeugt.
    if (numChannels < 2)
        smoothedMonoMix = 0.0;

    float* channelData[maxChannels] = { nullptr, nullptr };

    for (int ch = 0; ch < numChannels; ++ch)
        channelData[ch] = buffer.getWritePointer (ch);

    const double smooth = paramSmoothCoeff;

    double lastUpwardGainDb = 0.0;

    for (int n = 0; n < numSamples; ++n)
    {
        // --- Parameter sampleweise glaetten (20 ms) -----------------------
        smoothedBoostDb      += (targetBoost     - smoothedBoostDb)      * smooth;
        smoothedShelfHz      += (targetShelfHz   - smoothedShelfHz)      * smooth;
        smoothedMonoHz       += (targetMonoHz    - smoothedMonoHz)       * smooth;
        smoothedClipAmount   += (clipTarget      - smoothedClipAmount)   * smooth;
        smoothedUpwardAmount += (upwardTarget    - smoothedUpwardAmount) * smooth;
        smoothedEngage       += (engageTarget    - smoothedEngage)       * smooth;
        smoothedShelfMix     += (shelfMixTarget  - smoothedShelfMix)     * smooth;
        smoothedMonoMix      += (monoMixTarget   - smoothedMonoMix)      * smooth;

        // --- 1. Subsonik-Hochpass (Zustand laeuft immer) -------------------
        double shelfInput[maxChannels] = { 0.0, 0.0 };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const double x          = (double) channelData[ch][n];
            const double highPassed = subsonic[ch].processSampleDouble (x);

            shelfInput[ch] = x + smoothedEngage * (highPassed - x);
        }

        // --- 1b. SUB-BOOST (Low-Shelf, eingeblendet) ----------------------
        double low[maxChannels]  = { 0.0, 0.0 };
        double high[maxChannels] = { 0.0, 0.0 };
        double shelfOut[maxChannels] = { 0.0, 0.0 };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const double input   = shelfInput[ch];
            const double shelved = shelf[ch].processSampleDouble (input);

            const double blended = input + smoothedShelfMix * (shelved - input);
            shelfOut[ch] = blended;

            // --- 2. MONO BASS: LR4-Crossover -------------------------------
            double lowPart  = blended;
            double highPart = blended;

            for (int stage = 0; stage < 2; ++stage)
            {
                lowPart  = monoLowPass[ch][stage].processSampleDouble (lowPart);
                highPart = monoHighPass[ch][stage].processSampleDouble (highPart);
            }

            low[ch]  = lowPart;
            high[ch] = highPart;
        }

        const double monoLow = 0.5 * (low[0] + low[1]);

        double processed[maxChannels] = { 0.0, 0.0 };

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const double monoValue = monoLow + high[ch];
            processed[ch] = shelfOut[ch] + smoothedMonoMix * (monoValue - shelfOut[ch]);
        }

        // --- 3. UPWARD: Aufwaertskompression unterhalb -6 dBFS -------------
        double detector = 0.0;

        for (int ch = 0; ch < numChannels; ++ch)
            detector = juce::jmax (detector, std::abs (processed[ch]));

        double envelope = upwardFollower.process (detector);

        if (! std::isfinite (envelope) || envelope < (double) kDenormalGuard)
            envelope = 0.0;

        const double levelDb = dsp::gainToDb (envelope);
        const double liftDb  = juce::jlimit (0.0, (double) kUpwardMaxLiftDb,
                                            (double) kUpwardThresholdDb - levelDb);

        lastUpwardGainDb = smoothedUpwardAmount * liftDb;

        const double upwardGain = dsp::dbToGain (lastUpwardGainDb);

        float y[maxChannels] = { 0.0f, 0.0f };

        for (int ch = 0; ch < numChannels; ++ch)
            y[ch] = (float) (processed[ch] * upwardGain);

        // --- 4. TRUE-PEAK-CLIP --------------------------------------------
        float peak = oversampledPeak (0, y[0]);

        if (numChannels > 1)
            peak = juce::jmax (peak, oversampledPeak (1, y[1]));

        const float required = (peak > kCeiling) ? (kCeiling / peak) : 1.0f;

        gainRing[(size_t) gainPos] = required;
        gainPos = (gainPos + 1) % (lookahead + 1);

        float windowMin = 1.0f;

        for (size_t i = 0; i < gainRing.size(); ++i)
            windowMin = juce::jmin (windowMin, gainRing[i]);

        const float ramped  = juce::jmin (1.0f, limiterGain
                                                    + (1.0f - limiterGain) * limiterReleaseCoeff);
        const float applied = juce::jmin (windowMin, ramped);

        limiterGain = applied;

        const float blend = (float) smoothedClipAmount;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& line = delayLine[(size_t) ch];

            const float dry     = line[(size_t) delayPos];
            line[(size_t) delayPos] = y[ch];

            const float limited = dry * applied;

            channelData[ch][n] = dry + blend * (limited - dry);
        }

        delayPos = (delayPos + 1) % lookahead;
    }

    diagUpwardGainDb.store ((float) lastUpwardGainDb, std::memory_order_relaxed);
    diagLimiterGain.store (limiterGain, std::memory_order_relaxed);
}

} // namespace bfka
