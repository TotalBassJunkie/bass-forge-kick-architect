#include "fx/modules/MultibandModule.h"

#include <algorithm>
#include <cmath>

namespace bfka
{

namespace
{
    constexpr double kParameterSmoothingMs = 20.0;

    constexpr double kButterworthQ = 0.70710678118654752440;

    /** Uebersetzung des Abwaerts-Kompressors (3:1). */
    constexpr double kDownwardRatioSlope = 1.0 - 1.0 / 3.0;

    /** Uebersetzung des Aufwaerts-Kompressors (2:1 aufwaerts). */
    constexpr double kUpwardRatioSlope = 1.0 - 1.0 / 2.0;

    constexpr double kMaxUpwardBoostDb = 24.0;
    constexpr double kMaxDownwardCutDb = 48.0;

    /** Release laeuft gegenlaeufig zu Attack (wie beim Original). */
    constexpr double kAttackFastMs  = 0.05;
    constexpr double kAttackSlowMs  = 20.0;
    constexpr double kReleaseFastMs = 500.0;
    constexpr double kReleaseSlowMs = 20.0;
} // namespace

MultibandModule::MultibandModule()
{
    descriptors = {
        { "depth",    "Depth",             0.0f,   100.0f,  50.0f,  "%"  },
        { "time",     "Time",              0.0f,   100.0f,  50.0f,  "%"  },
        { "x1",       "Low/Mid Crossover", 20.0f,  1000.0f, 120.0f, "Hz" },
        { "x2",       "Mid/High Crossover", 500.0f, 12000.0f, 2500.0f, "Hz" },
        { "gl",       "Low Gain",          -12.0f, 12.0f,   0.0f,   "dB" },
        { "gm",       "Mid Gain",          -12.0f, 12.0f,   0.0f,   "dB" },
        { "gh",       "High Gain",         -12.0f, 12.0f,   0.0f,   "dB" },
        { "linear",   "Linear Phase",      0.0f,   1.0f,    0.0f,   ""   },
        { "inGain",   "Input Gain",        -24.0f, 24.0f,   0.0f,   "dB" },
        { "cleanXov", "Clean Crossover",   0.0f,   1.0f,    0.0f,   ""   },
        { "thL",      "Threshold Low",     -60.0f, 0.0f,    -30.0f, "dB" },
        { "thM",      "Threshold Mid",     -60.0f, 0.0f,    -30.0f, "dB" },
        { "thH",      "Threshold High",    -60.0f, 0.0f,    -30.0f, "dB" },
        { "buL",      "Bypass Upward Low",  0.0f,  1.0f,    0.0f,   ""   },
        { "buM",      "Bypass Upward Mid",  0.0f,  1.0f,    0.0f,   ""   },
        { "buH",      "Bypass Upward High", 0.0f,  1.0f,    0.0f,   ""   },
        { "bdL",      "Bypass Downward Low",  0.0f, 1.0f,   0.0f,   ""   },
        { "bdM",      "Bypass Downward Mid",  0.0f, 1.0f,   0.0f,   ""   },
        { "bdH",      "Bypass Downward High", 0.0f, 1.0f,   0.0f,   ""   },
        { "upwdStr",  "Upward Strength",   0.0f,   100.0f,  50.0f,  "%"  },
        { "dnwdStr",  "Downward Strength", 0.0f,   100.0f,  50.0f,  "%"  }
    };

    for (int i = 0; i < numParams; ++i)
    {
        paramValues[i] = (double) descriptors[(size_t) i].defaultValue;
        paramTargets[i].store (descriptors[(size_t) i].defaultValue, std::memory_order_relaxed);
    }

    for (int b = 0; b < numBands; ++b)
    {
        bandInputDb[b]  = 0.0;
        thresholdsDb[b] = -30.0;
        lastBandGainDb[b].store (0.0f, std::memory_order_relaxed);
    }
}

void MultibandModule::prepare (double newSampleRate, int newMaxBlockSize)
{
    sampleRate   = (newSampleRate > 0.0) ? newSampleRate : 48000.0;
    maxBlockSize = juce::jmax (1, newMaxBlockSize);

    // Linearphasiger Split: ~10,7 ms Fenster (513 Taps bei 48 kHz), damit die
    // Crossover-Aufloesung auch bei 96 kHz brauchbar bleibt.
    int desiredTaps = (int) std::lround (sampleRate * 0.0106875);

    if (desiredTaps % 2 == 0)
        ++desiredTaps;

    linearPhase.taps = juce::jlimit (multibandMinLinearTaps, multibandMaxLinearTaps, desiredTaps);

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        linearPhase.history[(size_t) ch].assign ((size_t) linearPhase.taps, 0.0f);
        linearPhase.writePos[(size_t) ch] = 0;
    }

    firLow.assign ((size_t) linearPhase.taps, 0.0f);
    firHigh.assign ((size_t) linearPhase.taps, 0.0f);

    for (auto& follower : followers)
        follower.prepare (sampleRate);

    linearMode = paramTargets[pLinear].load (std::memory_order_relaxed) > 0.5f;
    reportedLatency = linearMode ? (linearPhase.taps - 1) / 2 : 0;

    appliedLowXOver  = -1.0;
    appliedHighXOver = -1.0;
    appliedStages    = -1;

    resetSignalState();
}

void MultibandModule::reset()
{
    resetSignalState();
}

void MultibandModule::resetSignalState() noexcept
{
    for (auto& follower : followers)
        follower.reset();

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        auto& filters = minPhase[(size_t) ch];

        for (int stage = 0; stage < 4; ++stage)
        {
            filters.lowLowPass[stage].reset();
            filters.lowHighPass[stage].reset();
            filters.highLowPass[stage].reset();
            filters.highHighPass[stage].reset();
        }

        if (! linearPhase.history[(size_t) ch].empty())
        {
            std::fill (linearPhase.history[(size_t) ch].begin(),
                       linearPhase.history[(size_t) ch].end(), 0.0f);
            linearPhase.writePos[(size_t) ch] = 0;
        }
    }

    for (int b = 0; b < numBands; ++b)
        lastBandGainDb[b].store (0.0f, std::memory_order_relaxed);
}

void MultibandModule::setParam (int index, float value)
{
    if (index < 0 || index >= (int) numParams)
        return;

    const auto& desc = descriptors[(size_t) index];
    const float clamped = juce::jlimit (desc.minValue, desc.maxValue, value);

    paramTargets[(size_t) index].store (clamped, std::memory_order_relaxed);

    if (index == (int) pLinear)
    {
        linearMode = clamped > 0.5f;
        reportedLatency = linearMode ? (linearPhase.taps - 1) / 2 : 0;
    }
}

float MultibandModule::getParam (int index) const
{
    if (index < 0 || index >= (int) numParams)
        return 0.0f;

    return paramTargets[(size_t) index].load (std::memory_order_relaxed);
}

float MultibandModule::bandGainDb (int band) const noexcept
{
    if (band < 0 || band >= numBands)
        return 0.0f;

    return lastBandGainDb[band].load (std::memory_order_relaxed);
}

double MultibandModule::smoothingCoefficient (int numSamples, double timeMs) const noexcept
{
    const double blockSeconds = (double) numSamples / sampleRate;

    return 1.0 - std::exp (-blockSeconds / (std::max (0.001, timeMs) * 0.001));
}

std::vector<float> MultibandModule::designLowPassFIR (int taps, double cutoffHz, double sampleRateToUse)
{
    taps = (taps % 2 == 0) ? taps + 1 : taps;

    std::vector<float> coefficients ((size_t) taps, 0.0f);

    const double nyquist = 0.5 * sampleRateToUse;
    const double fc      = std::clamp (cutoffHz, 10.0, nyquist * 0.95) / sampleRateToUse;
    const int    mid     = (taps - 1) / 2;

    double sum = 0.0;

    for (int n = 0; n < taps; ++n)
    {
        const double x = (double) (n - mid);

        const double sinc = (std::abs (x) < 1.0e-9)
                                ? 2.0 * fc
                                : std::sin (2.0 * dsp::kPi * fc * x) / (dsp::kPi * x);

        const double window = 0.5 - 0.5 * std::cos (2.0 * dsp::kPi * (double) n / (double) (taps - 1));
        const double value  = sinc * window;

        coefficients[(size_t) n] = (float) value;
        sum += value;
    }

    if (std::abs (sum) > 1.0e-12)
        for (auto& coefficient : coefficients)
            coefficient = (float) ((double) coefficient / sum);

    return coefficients;
}

void MultibandModule::updateLinearPhaseCoefficients (double lowHz, double highHz)
{
    if ((int) firLow.size() != linearPhase.taps)
        return;

    firLow  = designLowPassFIR (linearPhase.taps, lowHz,  sampleRate);
    firHigh = designLowPassFIR (linearPhase.taps, highHz, sampleRate);
}

void MultibandModule::updateMinimumPhaseCoefficients (double lowHz, double highHz, int stages) noexcept
{
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        auto& filters = minPhase[(size_t) ch];

        for (int stage = 0; stage < stages; ++stage)
        {
            const auto lowLow  = dsp::designBiquad (dsp::BiquadKind::lowPass,  sampleRate, lowHz,  kButterworthQ, 0.0);
            const auto lowHigh = dsp::designBiquad (dsp::BiquadKind::highPass, sampleRate, lowHz,  kButterworthQ, 0.0);
            const auto highLow = dsp::designBiquad (dsp::BiquadKind::lowPass,  sampleRate, highHz, kButterworthQ, 0.0);
            const auto highHi  = dsp::designBiquad (dsp::BiquadKind::highPass, sampleRate, highHz, kButterworthQ, 0.0);

            filters.lowLowPass[stage].setCoefficients (lowLow);
            filters.lowHighPass[stage].setCoefficients (lowHigh);
            filters.highLowPass[stage].setCoefficients (highLow);
            filters.highHighPass[stage].setCoefficients (highHi);
        }
    }
}

void MultibandModule::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);

    if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0)
        return;

    // ---- 1. Parameter glaetten -------------------------------------------
    const double smoothing = smoothingCoefficient (numSamples, kParameterSmoothingMs);

    for (int i = 0; i < numParams; ++i)
    {
        const double target = (double) paramTargets[(size_t) i].load (std::memory_order_relaxed);
        paramValues[i] = dsp::smoothTowards (paramValues[i], target, smoothing);
    }

    depthAmount  = juce::jlimit (0.0, 1.0, paramValues[pDepth] / 100.0);
    upStrength   = juce::jlimit (0.0, 1.0, paramValues[pUpStrength] / 100.0);
    dnStrength   = juce::jlimit (0.0, 1.0, paramValues[pDnStrength] / 100.0);
    inputGainDb  = paramValues[pInGain];

    const double time01 = juce::jlimit (0.0, 1.0, paramValues[pTime] / 100.0);
    attackMs  = kAttackFastMs  + time01 * (kAttackSlowMs  - kAttackFastMs);
    releaseMs = kReleaseFastMs - time01 * (kReleaseFastMs - kReleaseSlowMs);

    for (auto& follower : followers)
        follower.setTimes (attackMs, releaseMs);

    const double expectedLow  = paramValues[pLowXOver];
    const double expectedHigh = paramValues[pHighXOver];

    lowXOver  = juce::jlimit (20.0, 1000.0, expectedLow);
    highXOver = juce::jlimit (500.0, 12000.0, expectedHigh);

    // X1 muss unter X2 liegen; sonst Kollaps der Baender.
    if (lowXOver > highXOver * 0.8)
        lowXOver = highXOver * 0.8;

    const int stages = (paramValues[pCleanXov] > 0.5) ? 4 : 2;

    bandInputDb[0] = paramValues[pLowGain];
    bandInputDb[1] = paramValues[pMidGain];
    bandInputDb[2] = paramValues[pHighGain];

    thresholdsDb[0] = paramValues[pThreshLow];
    thresholdsDb[1] = paramValues[pThreshMid];
    thresholdsDb[2] = paramValues[pThreshHigh];

    // ---- 2. Crossover-Koeffizienten bei Aenderung neu setzen -------------
    if (std::abs (lowXOver - appliedLowXOver) > 1.0e-6
        || std::abs (highXOver - appliedHighXOver) > 1.0e-6
        || appliedStages != stages)
    {
        updateMinimumPhaseCoefficients (lowXOver, highXOver, stages);
        updateLinearPhaseCoefficients (lowXOver, highXOver);

        appliedLowXOver  = lowXOver;
        appliedHighXOver = highXOver;
        appliedStages    = stages;
    }

    const double inputGain    = dsp::dbToGain (inputGainDb);
    const bool   upBypass[numBands] = { paramValues[pUpBypassLow]  > 0.5,
                                        paramValues[pUpBypassMid]  > 0.5,
                                        paramValues[pUpBypassHigh] > 0.5 };
    const bool   dnBypass[numBands] = { paramValues[pDnBypassLow]  > 0.5,
                                        paramValues[pDnBypassMid]  > 0.5,
                                        paramValues[pDnBypassHigh] > 0.5 };

    double bandInputLin[numBands];
    for (int b = 0; b < numBands; ++b)
        bandInputLin[b] = dsp::dbToGain (bandInputDb[b]);

    double gainAccumulator[numBands] { 0.0, 0.0, 0.0 };

    const bool   useLinear = linearMode;
    const int    taps      = linearPhase.taps;
    const float* firLowPtr  = firLow.data();
    const float* firHighPtr = firHigh.data();

    for (int n = 0; n < numSamples; ++n)
    {
        float bandL[numBands] { 0.0f, 0.0f, 0.0f };
        float bandR[numBands] { 0.0f, 0.0f, 0.0f };

        if (useLinear)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto*  history = linearPhase.history[(size_t) ch].data();
                int&   writePos = linearPhase.writePos[(size_t) ch];

                const float input = buffer.getSample (ch, n) * (float) inputGain;

                history[writePos] = input;

                double lowAcc  = 0.0;
                double highAcc = 0.0;

                int index = writePos;

                for (int k = 0; k < taps; ++k)
                {
                    const double value = (double) history[index];

                    lowAcc  += (double) firLowPtr[k]  * value;
                    highAcc += (double) firHighPtr[k] * value;

                    if (--index < 0)
                        index = taps - 1;
                }

                int delayedIndex = writePos - (taps - 1) / 2;

                while (delayedIndex < 0)
                    delayedIndex += taps;

                const double delayedInput = (double) history[delayedIndex];

                if (++writePos >= taps)
                    writePos = 0;

                const double low  = lowAcc;
                const double mid  = highAcc - lowAcc;
                const double high = delayedInput - highAcc;

                float* target = (ch == 0) ? bandL : bandR;
                target[0] = (float) low;
                target[1] = (float) mid;
                target[2] = (float) high;
            }
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& filters = minPhase[(size_t) ch];

                const double input = (double) (buffer.getSample (ch, n) * (float) inputGain);

                double lowSide  = input;
                double highSide = input;

                for (int stage = 0; stage < stages; ++stage)
                {
                    lowSide  = filters.lowLowPass[stage].processSampleDouble (lowSide);
                    highSide = filters.lowHighPass[stage].processSampleDouble (highSide);
                }

                // `highSide` ist oberhalb von X1; daraus Mid und High trennen.
                double mid  = highSide;
                double high = highSide;

                for (int stage = 0; stage < stages; ++stage)
                {
                    mid  = filters.highLowPass[stage].processSampleDouble (mid);
                    high = filters.highHighPass[stage].processSampleDouble (high);
                }

                float* target = (ch == 0) ? bandL : bandR;
                target[0] = (float) lowSide;
                target[1] = (float) mid;
                target[2] = (float) high;
            }

            if (numChannels == 1)
                bandR[0] = bandR[1] = bandR[2] = 0.0f;
        }

        float outputL = 0.0f;
        float outputR = 0.0f;

        for (int b = 0; b < numBands; ++b)
        {
            const float lowSample = (float) ((double) bandL[b] * bandInputLin[b]);
            const float rightSample = (numChannels > 1)
                                          ? (float) ((double) bandR[b] * bandInputLin[b])
                                          : lowSample;

            const double level = std::max (std::abs ((double) lowSample),
                                           std::abs ((double) rightSample));

            const double envelope = followers[b].process (level);
            const double levelDb  = dsp::gainToDb (envelope);

            double downwardDb = 0.0;
            double upwardDb   = 0.0;

            if (! dnBypass[b])
                downwardDb = std::max (0.0, levelDb - thresholdsDb[b]) * kDownwardRatioSlope;

            if (! upBypass[b])
                upwardDb = std::min (kMaxUpwardBoostDb,
                                     std::max (0.0, thresholdsDb[b] - levelDb) * kUpwardRatioSlope);

            const double appliedDb = juce::jlimit (-kMaxDownwardCutDb, kMaxUpwardBoostDb,
                                                   (-downwardDb * dnStrength + upwardDb * upStrength) * depthAmount);

            const double gain = dsp::dbToGain (appliedDb);

            outputL += (float) ((double) lowSample  * gain);
            outputR += (float) ((double) rightSample * gain);

            gainAccumulator[b] += appliedDb;
        }

        buffer.setSample (0, n, outputL);

        if (numChannels > 1)
            buffer.setSample (1, n, outputR);
    }

    const double inverseSamples = 1.0 / (double) juce::jmax (1, numSamples);

    for (int b = 0; b < numBands; ++b)
        lastBandGainDb[b].store ((float) (gainAccumulator[b] * inverseSamples), std::memory_order_relaxed);
}

} // namespace bfka
