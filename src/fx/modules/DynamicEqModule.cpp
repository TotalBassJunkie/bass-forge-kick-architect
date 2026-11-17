#include "fx/modules/DynamicEqModule.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace bfka
{

namespace
{
    constexpr double kParameterSmoothingMs = 20.0;
    constexpr double kAutoGainSmoothingMs   = 300.0;

    /** Butterworth-Q-Werte fuer 2. Ordnung (12 dB/Okt) und 4. Ordnung (24 dB/Okt). */
    constexpr double kButterworthQ1 = 0.70710678118654752440;
    constexpr double kButterworthQ4[2] = { 0.54119610014619698440, 1.30656296487637652790 };

    /** Butterworth-Q-Werte fuer 12. Ordnung (72 dB/Okt). */
    constexpr double kButterworthQ12[6] = { 0.50431448029007634180, 0.54119610014619698440,
                                            0.63023620700513221200, 0.82133981585229039160,
                                            1.30656296487637652790, 3.83064878777394030610 };

    constexpr double kDefaultFreqs[4] = { 100.0, 800.0, 4000.0, 12000.0 };

    int slopeStageCount (int type) noexcept
    {
        switch (type)
        {
            case 0:  return 0;   // einpolig, 6 dB/Okt
            case 1:  return 1;   // 12 dB/Okt
            case 2:  return 2;   // 24 dB/Okt
            default: return 6;   // 72 dB/Okt
        }
    }

    double onePoleCoefficient (double freqHz, double sampleRate) noexcept
    {
        const double clamped = std::clamp (freqHz, 1.0, sampleRate * 0.49);
        return 1.0 - std::exp (-2.0 * dsp::kPi * clamped / sampleRate);
    }
} // namespace

DynamicEqModule::DynamicEqModule()
{
    descriptors.reserve ((size_t) numParams);

    descriptors.push_back ({ "outGain",    "Output Gain",      -24.0f,   24.0f,    0.0f, "dB" });
    descriptors.push_back ({ "dryMix",     "Dry Mix",            0.0f,  100.0f,    0.0f, "%"  });
    descriptors.push_back ({ "hpActive",   "Highpass Active",    0.0f,    1.0f,    0.0f, ""   });
    descriptors.push_back ({ "hpFreq",     "Highpass Freq",     10.0f, 20000.0f,  20.0f, "Hz" });
    descriptors.push_back ({ "hpType",     "Highpass Slope",     0.0f,    3.0f,    1.0f, ""   });
    descriptors.push_back ({ "lpActive",   "Lowpass Active",     0.0f,    1.0f,    0.0f, ""   });
    descriptors.push_back ({ "lpFreq",     "Lowpass Freq",     100.0f, 40000.0f, 20000.0f, "Hz" });
    descriptors.push_back ({ "lpType",     "Lowpass Slope",      0.0f,    3.0f,    1.0f, ""   });
    descriptors.push_back ({ "wideGain",   "W-Band Gain",      -12.0f,   12.0f,    0.0f, "dB" });
    descriptors.push_back ({ "wideDyn",    "W-Band Dyn",         0.0f,    2.0f,    0.0f, ""   });
    descriptors.push_back ({ "wideThresh", "W-Band Threshold", -60.0f,    0.0f,  -20.0f, "dB" });
    descriptors.push_back ({ "wideRatio",  "W-Band Ratio",       1.0f,   20.0f,    2.0f, ""   });
    descriptors.push_back ({ "wideAttack", "W-Band Attack",      0.1f,  100.0f,   10.0f, "ms" });
    descriptors.push_back ({ "wideRelease","W-Band Release",     1.0f, 1000.0f,  100.0f, "ms" });
    descriptors.push_back ({ "autoGain",   "EQ Auto Gain",       0.0f,    1.0f,    1.0f, ""   });
    descriptors.push_back ({ "delta",      "Delta",              0.0f,    1.0f,    0.0f, ""   });
    descriptors.push_back ({ "channels",   "Channels",           0.0f,    6.0f,    1.0f, ""   });
    descriptors.push_back ({ "quality",    "Quality",            0.0f,    2.0f,    1.0f, ""   });
    descriptors.push_back ({ "sidechain",  "Sidechain",          0.0f,    1.0f,    0.0f, ""   });

    for (int b = 0; b < numBands; ++b)
    {
        const auto prefix = "Band " + juce::String (b + 1) + " ";

        descriptors.push_back ({ "b" + juce::String (b + 1) + "_active",  prefix + "Active",    0.0f, 2.0f,    0.0f, ""   });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_type",    prefix + "Type",      0.0f, 2.0f,    1.0f, ""   });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_freq",    prefix + "Frequency", 10.0f, 20000.0f, (float) kDefaultFreqs[b], "Hz" });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_gain",    prefix + "Gain",    -18.0f, 18.0f,    0.0f, "dB" });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_q",       prefix + "Q",         0.1f, 12.0f,    0.9f, ""   });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_dyn",     prefix + "Dyn",       0.0f, 2.0f,    0.0f, ""   });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_split",   prefix + "Split",     0.0f, 1.0f,    0.0f, ""   });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_thresh",  prefix + "Threshold", -60.0f, 0.0f, -20.0f, "dB" });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_ratio",   prefix + "Ratio",     1.0f, 20.0f,    2.0f, ""   });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_attack",  prefix + "Attack",    0.1f, 100.0f,  10.0f, "ms" });
        descriptors.push_back ({ "b" + juce::String (b + 1) + "_release", prefix + "Release",   1.0f, 1000.0f, 100.0f, "ms" });
    }

    for (int i = 0; i < numParams; ++i)
    {
        const double value = (double) descriptors[(size_t) i].defaultValue;
        paramValues[i] = value;
        paramTargets[i].store ((float) value, std::memory_order_relaxed);
    }

    for (int b = 0; b < numBands; ++b)
    {
        bands[b].freqHz      = kDefaultFreqs[b];
        bands[b].q           = 0.9;
        bands[b].gainDb      = 0.0;
        bands[b].thresholdDb = -20.0;
        bands[b].ratio       = 2.0;
        bands[b].attackMs    = 10.0;
        bands[b].releaseMs   = 100.0;
        bands[b].type        = 1;
        bands[b].active      = 0;
        bands[b].dyn         = 0;
        bands[b].split       = 0;

        lastBandDynamicDb[b].store (0.0f, std::memory_order_relaxed);
    }
}

void DynamicEqModule::prepare (double newSampleRate, int newMaxBlockSize)
{
    sampleRate   = (newSampleRate > 0.0) ? newSampleRate : 48000.0;
    maxBlockSize = juce::jmax (1, newMaxBlockSize);

    dryBuffer.setSize (maxChannels, maxBlockSize, false, true, true);
    workBuffer.setSize (maxChannels, maxBlockSize, false, true, true);

    wideFollower.prepare (sampleRate);

    for (auto& band : bands)
        band.follower.prepare (sampleRate);

    autoGainDb = 0.0;
    prepared   = true;

    reset();
}

void DynamicEqModule::reset()
{
    for (auto& cascade : hpFilters)
        for (auto& stage : cascade.stages)
            stage.reset();

    for (auto& cascade : lpFilters)
        for (auto& stage : cascade.stages)
            stage.reset();

    for (auto& filter : hpOnePole)
        filter.reset();

    for (auto& filter : lpOnePole)
        filter.reset();

    sidechainHighPass.reset();
    wideFollower.reset();

    for (auto& band : bands)
    {
        for (auto& filter : band.filter)
            filter.reset();

        for (auto& filter : band.detector)
            filter.reset();

        band.follower.reset();
        band.heldDynamicDb      = 0.0;
        band.appliedDynamicDb   = 0.0;
    }

    wideReductionDb     = 0.0;
    heldWideReductionDb = 0.0;

    dryBuffer.clear();
    workBuffer.clear();

    for (int b = 0; b < numBands; ++b)
        lastBandDynamicDb[b].store (0.0f, std::memory_order_relaxed);
}

void DynamicEqModule::setParam (int index, float value)
{
    if (index < 0 || index >= numParams)
        return;

    const auto& desc = descriptors[(size_t) index];
    paramTargets[(size_t) index].store (juce::jlimit (desc.minValue, desc.maxValue, value),
                                        std::memory_order_relaxed);
}

float DynamicEqModule::getParam (int index) const
{
    if (index < 0 || index >= numParams)
        return 0.0f;

    return paramTargets[(size_t) index].load (std::memory_order_relaxed);
}

float DynamicEqModule::bandDynamicGainDb (int band) const noexcept
{
    if (band < 0 || band >= numBands)
        return 0.0f;

    return lastBandDynamicDb[band].load (std::memory_order_relaxed);
}

double DynamicEqModule::smoothingCoefficient (int numSamples, double timeMs) const noexcept
{
    const double blockSeconds = (double) numSamples / sampleRate;

    return 1.0 - std::exp (-blockSeconds / (std::max (0.001, timeMs) * 0.001));
}

double DynamicEqModule::biquadMagnitudeDb (const dsp::BiquadCoeffs& coefficients,
                                           double sampleRateToUse,
                                           double freqHz) noexcept
{
    const double w = 2.0 * dsp::kPi * std::clamp (freqHz, 1.0, sampleRateToUse * 0.49) / sampleRateToUse;

    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;

    const std::complex<double> numerator   = coefficients.b0 + coefficients.b1 * z1 + coefficients.b2 * z2;
    const std::complex<double> denominator = 1.0 + coefficients.a1 * z1 + coefficients.a2 * z2;

    return 20.0 * std::log10 (std::max (1.0e-12, std::abs (numerator) / std::max (1.0e-12, std::abs (denominator))));
}

void DynamicEqModule::updateHpLpCoefficients()
{
    const bool hpOn = paramValues[pHpActive] > 0.5;
    const bool lpOn = paramValues[pLpActive] > 0.5;

    if (hpOn)
    {
        const int    type   = (int) std::lround (paramValues[pHpType]);
        const int    stages = slopeStageCount (type);
        const double freq   = paramValues[pHpFreq];

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            auto& cascade = hpFilters[(size_t) ch];
            cascade.stageCount = stages;

            if (stages != 0)
            {
                for (int s = 0; s < stages; ++s)
                {
                    const double q = (stages == 1) ? kButterworthQ1
                                    : (stages == 2) ? kButterworthQ4[s]
                                                    : kButterworthQ12[s];

                    cascade.stages[s].setCoefficients (dsp::designBiquad (dsp::BiquadKind::highPass,
                                                                         sampleRate, freq, q, 0.0));
                }
            }
        }
    }

    if (lpOn)
    {
        const int    type   = (int) std::lround (paramValues[pLpType]);
        const int    stages = slopeStageCount (type);
        const double freq   = paramValues[pLpFreq];

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            auto& cascade = lpFilters[(size_t) ch];
            cascade.stageCount = stages;

            if (stages != 0)
            {
                for (int s = 0; s < stages; ++s)
                {
                    const double q = (stages == 1) ? kButterworthQ1
                                    : (stages == 2) ? kButterworthQ4[s]
                                                    : kButterworthQ12[s];

                    cascade.stages[s].setCoefficients (dsp::designBiquad (dsp::BiquadKind::lowPass,
                                                                         sampleRate, freq, q, 0.0));
                }
            }
        }
    }
}

void DynamicEqModule::updateFilterCoefficients()
{
    for (int b = 0; b < numBands; ++b)
    {
        auto& band = bands[b];

        const auto kind = (band.type == 0) ? dsp::BiquadKind::lowShelf
                        : (band.type == 2) ? dsp::BiquadKind::highShelf
                                           : dsp::BiquadKind::peaking;

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            band.filter[ch].setCoefficients (dsp::designBiquad (kind, sampleRate,
                                                                band.freqHz, band.q, band.gainDb));
            band.detector[ch].setCoefficients (dsp::designBiquad (dsp::BiquadKind::bandPass, sampleRate,
                                                                  band.freqHz,
                                                                  std::max (0.3, band.q), 0.0));
        }
    }
}

void DynamicEqModule::computeAutoGainTarget()
{
    constexpr int numPoints = 24;

    double sumDb = 0.0;
    int    count = 0;

    for (int i = 0; i < numPoints; ++i)
    {
        const double position = (double) i / (double) (numPoints - 1);
        const double freq     = 20.0 * std::pow (1000.0, position); // 20 Hz ... 20 kHz

        double magnitude = 0.0;

        for (const auto& band : bands)
        {
            if (band.active == 0)
                continue;

            magnitude += biquadMagnitudeDb (band.filter[0].getCoefficients(), sampleRate, freq);
        }

        sumDb += magnitude;
        ++count;
    }

    autoGainTargetDb = (count > 0) ? -(sumDb / (double) count) : 0.0;
}

void DynamicEqModule::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);

    if (! prepared || numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0)
        return;

    if (numSamples > workBuffer.getNumSamples())
        return; // Vertrag: Bloecke nie groesser als in prepare()

    // ---- 1. Parameter glaetten -------------------------------------------
    const double smoothing = smoothingCoefficient (numSamples, kParameterSmoothingMs);

    for (int i = 0; i < numParams; ++i)
    {
        const double target = (double) paramTargets[(size_t) i].load (std::memory_order_relaxed);
        paramValues[i] = dsp::smoothTowards (paramValues[i], target, smoothing);
    }

    for (int b = 0; b < numBands; ++b)
    {
        auto& band = bands[b];

        band.active      = (int) std::lround (paramValues[bandParam (b, bpActive)]);
        band.type        = (int) std::lround (paramValues[bandParam (b, bpType)]);
        band.dyn         = (int) std::lround (paramValues[bandParam (b, bpDyn)]);
        band.split       = (int) std::lround (paramValues[bandParam (b, bpSplit)]);
        band.freqHz      = paramValues[bandParam (b, bpFreq)];
        band.gainDb      = paramValues[bandParam (b, bpGain)];
        band.q           = paramValues[bandParam (b, bpQ)];
        band.thresholdDb = paramValues[bandParam (b, bpThreshold)];
        band.ratio       = std::max (1.0, (double) paramValues[bandParam (b, bpRatio)]);
        band.attackMs    = paramValues[bandParam (b, bpAttack)];
        band.releaseMs   = paramValues[bandParam (b, bpRelease)];

        const bool useOwnTimes = (band.split != 0) || (band.dyn == 2);
        band.follower.setTimes (useOwnTimes ? band.attackMs : (double) paramValues[pWideAttack],
                                useOwnTimes ? band.releaseMs : (double) paramValues[pWideRelease]);
    }

    const int wideDyn    = (int) std::lround (paramValues[pWideDyn]);
    const int channelMode = juce::jlimit (0, 6, (int) std::lround (paramValues[pChannels]));
    const int sidechainMode = (int) std::lround (paramValues[pSidechain]);

    const double wideThreshold = paramValues[pWideThresh];
    const double wideRatio     = std::max (1.0, (double) paramValues[pWideRatio]);

    wideFollower.setTimes (paramValues[pWideAttack], paramValues[pWideRelease]);

    sidechainHighPass.setCoefficients (sidechainMode == 0
                                           ? dsp::designBiquad (dsp::BiquadKind::highPass, sampleRate, 60.0, kButterworthQ1, 0.0)
                                           : dsp::BiquadCoeffs {});

    updateHpLpCoefficients();
    updateFilterCoefficients();

    // ---- 2. Ein-/Ausgangspegel und Prozessziel ---------------------------
    outputGainDb = paramValues[pOutGain];
    wideGainDb   = paramValues[pWideGain];

    const bool hpOn = paramValues[pHpActive] > 0.5;
    const bool lpOn = paramValues[pLpActive] > 0.5;
    const int  hpType = (int) std::lround (paramValues[pHpType]);
    const int  lpType = (int) std::lround (paramValues[pLpType]);
    const double hpFreq = paramValues[pHpFreq];
    const double lpFreq = paramValues[pLpFreq];

    const double hpOneCoeff = onePoleCoefficient (hpFreq, sampleRate);
    const double lpOneCoeff = onePoleCoefficient (lpFreq, sampleRate);

    // Trockenkopie des Originals
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    if (numChannels == 1)
        dryBuffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    // Arbeitspuffer je Prozessziel fuellen
    bool processChannel[maxChannels] { false, false };

    switch ((ChannelMode) channelMode)
    {
        case ChannelMode::mono:
        {
            for (int n = 0; n < numSamples; ++n)
            {
                const float right = (numChannels > 1) ? buffer.getSample (1, n) : buffer.getSample (0, n);
                const float mono  = 0.5f * (buffer.getSample (0, n) + right);
                workBuffer.setSample (0, n, mono);
                workBuffer.setSample (1, n, mono);
            }
            processChannel[0] = true;
            processChannel[1] = true;
            break;
        }

        case ChannelMode::sum:
        {
            for (int n = 0; n < numSamples; ++n)
            {
                const float left  = buffer.getSample (0, n);
                const float right = (numChannels > 1) ? buffer.getSample (1, n) : left;
                workBuffer.setSample (0, n, 0.5f * (left + right));
                workBuffer.setSample (1, n, 0.5f * (left - right));
            }
            processChannel[0] = true;
            break;
        }

        case ChannelMode::diff:
        {
            for (int n = 0; n < numSamples; ++n)
            {
                const float left  = buffer.getSample (0, n);
                const float right = (numChannels > 1) ? buffer.getSample (1, n) : left;
                workBuffer.setSample (0, n, 0.5f * (left + right));
                workBuffer.setSample (1, n, 0.5f * (left - right));
            }
            processChannel[1] = true;
            break;
        }

        case ChannelMode::left:
        {
            for (int n = 0; n < numSamples; ++n)
            {
                workBuffer.setSample (0, n, buffer.getSample (0, n));
                workBuffer.setSample (1, n, (numChannels > 1) ? buffer.getSample (1, n) : 0.0f);
            }
            processChannel[0] = true;
            break;
        }

        case ChannelMode::right:
        {
            for (int n = 0; n < numSamples; ++n)
            {
                workBuffer.setSample (0, n, buffer.getSample (0, n));
                workBuffer.setSample (1, n, (numChannels > 1) ? buffer.getSample (1, n) : buffer.getSample (0, n));
            }
            processChannel[1] = true;
            break;
        }

        case ChannelMode::stereo:
        case ChannelMode::dualMono:
        default:
        {
            for (int n = 0; n < numSamples; ++n)
            {
                workBuffer.setSample (0, n, buffer.getSample (0, n));
                workBuffer.setSample (1, n, buffer.getSample (1, n));
            }
            processChannel[0] = true;
            processChannel[1] = true;
            break;
        }
    }

    // ---- 3. Sampleweise Verarbeitung -------------------------------------
    double dynamicAccumulator[numBands] { 0.0, 0.0, 0.0, 0.0 };

    for (int n = 0; n < numSamples; ++n)
    {
        double samples[maxChannels] { 0.0, 0.0 };

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            if (! processChannel[ch])
            {
                samples[ch] = (double) workBuffer.getSample (ch, n);
                continue;
            }

            double value = (double) workBuffer.getSample (ch, n);

            if (hpOn)
            {
                if (hpType == 0)
                {
                    value = hpOnePole[(size_t) ch].processHighPass (value, hpOneCoeff);
                }
                else
                {
                    for (int s = 0; s < hpFilters[(size_t) ch].stageCount; ++s)
                        value = hpFilters[(size_t) ch].stages[s].processSampleDouble (value);
                }
            }

            if (lpOn)
            {
                if (lpType == 0)
                {
                    value = lpOnePole[(size_t) ch].processLowPass (value, lpOneCoeff);
                }
                else
                {
                    for (int s = 0; s < lpFilters[(size_t) ch].stageCount; ++s)
                        value = lpFilters[(size_t) ch].stages[s].processSampleDouble (value);
                }
            }

            samples[ch] = value;
        }

        // ---- W-Band-Detektor (stereo-gekoppelt, optional gefiltert) ------
        double detectorInput = 0.0;

        for (int ch = 0; ch < maxChannels; ++ch)
            if (processChannel[ch])
                detectorInput = std::max (detectorInput, std::abs (samples[ch]));

        if (sidechainMode == 0)
            detectorInput = sidechainHighPass.processSampleDouble (detectorInput);

        const double wideEnvelope = wideFollower.process (detectorInput);
        const double wideLevelDb  = dsp::gainToDb (wideEnvelope);

        double wideReduction = 0.0;

        if (wideDyn != 0)
        {
            wideReduction = std::max (0.0, wideLevelDb - wideThreshold) * (1.0 - 1.0 / wideRatio);

            if (wideDyn == 2 && wideLevelDb < wideThreshold)
                wideReduction = heldWideReductionDb;
        }

        heldWideReductionDb = wideReduction;
        wideReductionDb     = wideReduction;

        const double wideGain = dsp::dbToGain (wideGainDb - wideReduction);

        for (int ch = 0; ch < maxChannels; ++ch)
            if (processChannel[ch])
                samples[ch] *= wideGain;

        // ---- vier dynamische Baender ------------------------------------
        for (int b = 0; b < numBands; ++b)
        {
            auto& band = bands[b];

            if (band.active == 0)
                continue;

            double bandDetector = 0.0;

            for (int ch = 0; ch < maxChannels; ++ch)
                if (processChannel[ch])
                    bandDetector = std::max (bandDetector,
                                             std::abs (band.detector[ch].processSampleDouble (samples[ch])));

            double dynamicDb = 0.0;

            if (band.dyn != 0)
            {
                double levelDb = wideLevelDb;

                if (band.split != 0)
                    levelDb = dsp::gainToDb (band.follower.process (bandDetector));

                dynamicDb = std::max (0.0, levelDb - band.thresholdDb) * (1.0 - 1.0 / band.ratio);

                if (band.dyn == 2 && levelDb < band.thresholdDb)
                    dynamicDb = band.heldDynamicDb;
            }

            band.heldDynamicDb = dynamicDb;

            const double staticGain = dsp::dbToGain (band.gainDb);
            const double targetGain = dsp::dbToGain (band.gainDb - dynamicDb);

            double blend = 1.0;

            if (std::abs (staticGain - 1.0) > 1.0e-6)
                blend = (targetGain - 1.0) / (staticGain - 1.0);

            // Negativ = das Band kippt in die Gegenrichtung (aus einer Anhebung
            // wird bei starker Dynamik eine Absenkung, wie im Original).
            blend = juce::jlimit (-16.0, 16.0, blend);

            for (int ch = 0; ch < maxChannels; ++ch)
            {
                if (! processChannel[ch])
                    continue;

                const double input = samples[ch];
                const double bandOut = band.filter[ch].processSampleDouble (input);

                samples[ch] = input + (bandOut - input) * blend;
            }

            band.appliedDynamicDb = dynamicDb;
            dynamicAccumulator[b] -= dynamicDb;
        }

        // ---- zurueckschreiben -------------------------------------------
        for (int ch = 0; ch < maxChannels; ++ch)
            if (processChannel[ch])
                workBuffer.setSample (ch, n, (float) samples[ch]);
    }

    // ---- 4. Auto-Gain (statische EQ-Kompensation) ------------------------
    const bool autoGainOn = paramValues[pAutoGain] > 0.5;

    if (autoGainOn)
    {
        computeAutoGainTarget();
        autoGainDb = dsp::smoothTowards (autoGainDb, autoGainTargetDb,
                                         smoothingCoefficient (numSamples, kAutoGainSmoothingMs));
    }

    // ---- 5. Rekonstruktion, Ausgang, Blende ------------------------------
    const double outputGain   = dsp::dbToGain (outputGainDb);
    const double autoGain     = autoGainOn ? dsp::dbToGain (autoGainDb) : 1.0;
    const double dryMixAmount = juce::jlimit (0.0, 1.0, (double) paramValues[pDryMix] / 100.0);
    const bool   deltaOn      = paramValues[pDelta] > 0.5;

    for (int n = 0; n < numSamples; ++n)
    {
        double left  = (double) workBuffer.getSample (0, n);
        double right = (numChannels > 1) ? (double) workBuffer.getSample (1, n) : left;

        switch ((ChannelMode) channelMode)
        {
            case ChannelMode::mono:
            {
                const double mono = left;
                left  = mono;
                right = mono;
                break;
            }

            case ChannelMode::sum:
            case ChannelMode::diff:
            {
                const double mid  = left;
                const double side = right;
                left  = mid + side;
                right = mid - side;
                break;
            }

            case ChannelMode::left:
            {
                right = (double) dryBuffer.getSample (1, n);
                break;
            }

            case ChannelMode::right:
            {
                left = (double) dryBuffer.getSample (0, n);
                break;
            }

            case ChannelMode::stereo:
            case ChannelMode::dualMono:
            default:
                break;
        }

        left  *= outputGain * autoGain;
        right *= outputGain * autoGain;

        const double drySampleL = (double) dryBuffer.getSample (0, n);
        const double drySampleR = (numChannels > 1) ? (double) dryBuffer.getSample (1, n)
                                                    : (double) dryBuffer.getSample (0, n);

        if (deltaOn)
        {
            left  = drySampleL - left;
            right = drySampleR - right;
        }
        else if (dryMixAmount > 0.0)
        {
            left  = left * (1.0 - dryMixAmount) + drySampleL * dryMixAmount;
            right = right * (1.0 - dryMixAmount) + drySampleR * dryMixAmount;
        }

        buffer.setSample (0, n, (float) left);

        if (numChannels > 1)
            buffer.setSample (1, n, (float) right);
    }

    const double inverseSamples = 1.0 / (double) juce::jmax (1, numSamples);

    for (int b = 0; b < numBands; ++b)
        lastBandDynamicDb[b].store ((float) (dynamicAccumulator[b] * inverseSamples),
                                    std::memory_order_relaxed);
}

} // namespace bfka
