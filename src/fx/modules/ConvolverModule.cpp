#include "fx/modules/ConvolverModule.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Log.h"

namespace bfka
{

namespace
{
    constexpr double kParameterSmoothingMs = 20.0;

    /** Grenzen der 11-Punkt-Kurven (logarithmisch). */
    constexpr double kCurveLowHz  = 20.0;
    constexpr double kCurveHighHz = 20000.0;

    constexpr double kToneNeutral = 50.0;

    double lerp (double a, double b, double t) noexcept { return a + (b - a) * t; }
} // namespace

ConvolverModule::ConvolverModule()
{
    descriptors = {
        { "wet",       "Wet",         0.0f,   100.0f,  100.0f,  "%"  },
        { "stereo",    "Width",       0.0f,   200.0f,  100.0f,  "%"  },
        { "tone",      "Tone",        0.0f,   100.0f,  50.0f,   "%"  },
        { "speed",     "Stretch",     -12.0f, 12.0f,   0.0f,    "st" },
        { "predelay",  "Predelay",    0.0f,   500.0f,  0.0f,    "ms" },
        { "decay",     "Decay",       0.1f,   2.0f,    1.0f,    "x"  },
        { "modDepth",  "Mod Depth",   0.0f,   100.0f,  0.0f,    "%"  },
        { "modRate",   "Mod Rate",    0.05f,  10.0f,   0.5f,    "Hz" },
        { "mod90",     "Mod 90",      0.0f,   1.0f,    0.0f,    ""   },
        { "reverse",   "Reverse",     0.0f,   1.0f,    0.0f,    ""   },
        { "monoIn",    "Mono Input",  0.0f,   1.0f,    0.0f,    ""   },
        { "normalize", "Normalize",   0.0f,   1.0f,    1.0f,    ""   },
        { "irGain",    "IR Gain",     -24.0f, 24.0f,   0.0f,    "dB" },
        { "trueStereo","True Stereo", 0.0f,   1.0f,    1.0f,    ""   },
        { "zeroLat",   "Zero Latency", 0.0f,  1.0f,    1.0f,    ""   },
        { "envStart",  "Start",       0.0f,   1000.0f, 0.0f,    "ms" },
        { "envAttack", "Attack",      0.0f,   1000.0f, 0.0f,    "ms" },
        { "envHold",   "Hold",        0.0f,   2000.0f, 0.0f,    "ms" },
        { "envRelease","Release",     0.0f,   5000.0f, 0.0f,    "ms" }
    };

    for (int i = 0; i < numParams; ++i)
    {
        paramValues[i] = (double) descriptors[(size_t) i].defaultValue;
        paramTargets[i].store (descriptors[(size_t) i].defaultValue, std::memory_order_relaxed);
    }

    eqCurve.fill (0.0f);       // dB
    decayCurve.fill (1.0f);    // Faktor

    formatManager.registerBasicFormats();
}

ConvolverModule::~ConvolverModule()
{
    delete pendingKernel.exchange (nullptr);
}

double ConvolverModule::smoothingCoefficient (int numSamples, double timeMs) const noexcept
{
    const double blockSeconds = (double) numSamples / sampleRate;

    return 1.0 - std::exp (-blockSeconds / (std::max (0.001, timeMs) * 0.001));
}

void ConvolverModule::prepare (double newSampleRate, int newMaxBlockSize)
{
    sampleRate   = (newSampleRate > 0.0) ? newSampleRate : 48000.0;
    maxBlockSize = juce::jmax (1, newMaxBlockSize);

    const int blockSamples = maxBlockSize;

    inputA.setSize (maxChannels, blockSamples, false, true, true);
    inputB.setSize (maxChannels, blockSamples, false, true, true);
    outputA.setSize (maxChannels, blockSamples, false, true, true);
    outputB.setSize (maxChannels, blockSamples, false, true, true);
    wetBuffer.setSize (maxChannels, blockSamples, false, true, true);

    predelaySamples = (int) std::lround (0.5 * sampleRate);

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        predelayLines[(size_t) ch].assign ((size_t) juce::jmax (2, predelaySamples + 2), 0.0f);
        predelayWrite[(size_t) ch] = 0;
    }

    modLineSize = (int) std::lround (0.03 * sampleRate) + 4;

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        modLines[(size_t) ch].assign ((size_t) modLineSize, 0.0f);
        modWrite[(size_t) ch] = 0;
    }

    prepared = true;

    createEngines();

    for (auto& filter : toneLow)
        filter.reset();

    for (auto& filter : toneHigh)
        filter.reset();

    for (auto& channelBands : eqBands)
        for (auto& band : channelBands)
            band.reset();

    modPhase = 0.0;

    if (rawIr.getNumSamples() > 0)
    {
        // Baut das Kernel neu und laedt es synchron in die frische Engine.
        rebuildKernel (true);
    }
    else if (auto* kernel = pendingKernel.exchange (nullptr))
    {
        loadKernelIntoEngines (*kernel);
        reprepareEngines();
        delete kernel;
    }

    reset();
}

void ConvolverModule::createEngines()
{
    convolutionA.reset();

    convolutionB.reset();

    const bool zeroLatency = paramTargets[pZeroLat].load (std::memory_order_relaxed) > 0.5f;

    if (zeroLatency)
    {
        convolutionA = std::make_unique<juce::dsp::Convolution> (juce::dsp::Convolution::NonUniform { 1024 });
        convolutionB = std::make_unique<juce::dsp::Convolution> (juce::dsp::Convolution::NonUniform { 1024 });
    }
    else
    {
        convolutionA = std::make_unique<juce::dsp::Convolution> (juce::dsp::Convolution::Latency { 2048 });
        convolutionB = std::make_unique<juce::dsp::Convolution> (juce::dsp::Convolution::Latency { 2048 });
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) maxBlockSize;
    spec.numChannels      = maxChannels;

    convolutionA->prepare (spec);
    convolutionB->prepare (spec);

    engineIsZeroLatency = zeroLatency;
    reportedLatency     = convolutionA->getLatency();
    bKernelLoaded       = false;
}

void ConvolverModule::reprepareEngines()
{
    if (! prepared || convolutionA == nullptr)
        return;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) maxBlockSize;
    spec.numChannels      = maxChannels;

    // prepare() zieht die zuletzt geladene IR synchron in die Engine.
    convolutionA->prepare (spec);

    if (convolutionB != nullptr)
        convolutionB->prepare (spec);

    reportedLatency = convolutionA->getLatency();
}

void ConvolverModule::loadKernelIntoEngines (IrKernel& kernel)
{
    if (convolutionA == nullptr || kernel.a.getNumSamples() <= 0)
        return;

    convolutionA->loadImpulseResponse (std::move (kernel.a), kernel.sampleRate,
                                       juce::dsp::Convolution::Stereo::yes,
                                       juce::dsp::Convolution::Trim::no,
                                       juce::dsp::Convolution::Normalise::no);

    bKernelLoaded = kernel.trueStereo;

    if (kernel.trueStereo && convolutionB != nullptr)
    {
        convolutionB->loadImpulseResponse (std::move (kernel.b), kernel.sampleRate,
                                           juce::dsp::Convolution::Stereo::yes,
                                           juce::dsp::Convolution::Trim::no,
                                           juce::dsp::Convolution::Normalise::no);
    }
}

void ConvolverModule::reset()
{
    if (convolutionA != nullptr)
        convolutionA->reset();

    if (convolutionB != nullptr)
        convolutionB->reset();

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        std::fill (predelayLines[(size_t) ch].begin(), predelayLines[(size_t) ch].end(), 0.0f);
        predelayWrite[(size_t) ch] = 0;

        std::fill (modLines[(size_t) ch].begin(), modLines[(size_t) ch].end(), 0.0f);
        modWrite[(size_t) ch] = 0;

        toneLow[(size_t) ch].reset();
        toneHigh[(size_t) ch].reset();

        for (auto& band : eqBands[(size_t) ch])
            band.reset();
    }

    modPhase = 0.0;

    inputA.clear();
    inputB.clear();
    outputA.clear();
    outputB.clear();
    wetBuffer.clear();
}

void ConvolverModule::setParam (int index, float value)
{
    if (index < 0 || index >= (int) numParams)
        return;

    const auto& desc = descriptors[(size_t) index];
    const float clamped = juce::jlimit (desc.minValue, desc.maxValue, value);
    paramTargets[(size_t) index].store (clamped, std::memory_order_relaxed);

    // Parameter, die die IR selbst veraendern, bauen das Kernel neu.
    switch ((ParamIndex) index)
    {
        case pSpeed:
        case pDecay:
        case pReverse:
        case pNormalize:
        case pEnvStart:
        case pEnvAttack:
        case pEnvHold:
        case pEnvRelease:
        case pTrueStereo:
            if (rawIr.getNumSamples() > 0)
                rebuildKernel (false);
            break;

        default:
            break;
    }
}

float ConvolverModule::getParam (int index) const
{
    if (index < 0 || index >= (int) numParams)
        return 0.0f;

    return paramTargets[(size_t) index].load (std::memory_order_relaxed);
}

// =========================================================================
// IR-Aufbereitung (Message-Thread)
// =========================================================================

void ConvolverModule::applyReverse (juce::AudioBuffer<float>& ir)
{
    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        ir.reverse (ch, 0, ir.getNumSamples());
}

void ConvolverModule::applyStretch (juce::AudioBuffer<float>& ir, double semitones)
{
    if (std::abs (semitones) < 1.0e-6)
        return;

    const double factor = std::pow (2.0, semitones / 12.0);
    const int    length = ir.getNumSamples();

    if (length <= 1 || ir.getNumChannels() <= 0)
        return;

    const int newLength = juce::jmax (1, (int) std::lround ((double) length / factor));

    juce::AudioBuffer<float> stretched (ir.getNumChannels(), newLength);

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        const auto* source = ir.getReadPointer (ch);
        auto*       target = stretched.getWritePointer (ch);

        for (int i = 0; i < newLength; ++i)
        {
            const double position = (double) i * factor;
            const int    index    = (int) std::floor (position);
            const double fraction = position - (double) index;

            const float a = (index >= 0 && index < length) ? source[index] : 0.0f;
            const float b = (index + 1 >= 0 && index + 1 < length) ? source[index + 1] : a;

            target[i] = (float) lerp ((double) a, (double) b, fraction);
        }
    }

    ir = std::move (stretched);
}

void ConvolverModule::applyDecayScale (juce::AudioBuffer<float>& ir, double decay)
{
    if (std::abs (decay - 1.0) < 1.0e-6)
        return;

    const int    length    = ir.getNumSamples();
    const double clamped   = std::clamp (decay, 0.01, 100.0);
    const double logDecay  = std::log (clamped);

    if (length <= 0)
        return;

    for (int i = 0; i < length; ++i)
    {
        const double position = (length > 1) ? (double) i / (double) (length - 1) : 0.0;
        const float  gain     = (float) std::exp (logDecay * position);

        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
            ir.getWritePointer (ch)[i] *= gain;
    }
}

void ConvolverModule::applyNormalisation (juce::AudioBuffer<float>& ir)
{
    double totalEnergy = 0.0;

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        const auto* data = ir.getReadPointer (ch);

        for (int i = 0; i < ir.getNumSamples(); ++i)
            totalEnergy += (double) data[i] * (double) data[i];
    }

    const double norm = std::sqrt (totalEnergy);

    if (norm > 1.0e-9)
        ir.applyGain ((float) (1.0 / norm));
}

void ConvolverModule::applyTimeEnvelope (juce::AudioBuffer<float>& ir, double sampleRateToUse,
                                         double startMs, double attackMs, double holdMs, double releaseMs)
{
    if (ir.getNumSamples() <= 0)
        return;

    const int startSamples = (int) std::lround (startMs * 0.001 * sampleRateToUse);

    if (startSamples > 0)
    {
        const int remaining = ir.getNumSamples() - startSamples;

        if (remaining <= 0)
            return;

        juce::AudioBuffer<float> cropped (ir.getNumChannels(), remaining);

        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
            cropped.copyFrom (ch, 0, ir, ch, startSamples, remaining);

        ir = std::move (cropped);
    }

    const int attackSamples  = (int) std::lround (attackMs  * 0.001 * sampleRateToUse);
    const int holdSamples    = (int) std::lround (holdMs    * 0.001 * sampleRateToUse);
    const int releaseSamples = (int) std::lround (releaseMs * 0.001 * sampleRateToUse);

    if (attackSamples <= 0 && releaseSamples <= 0)
        return;

    const int length = ir.getNumSamples();

    for (int i = 0; i < length; ++i)
    {
        double gain = 1.0;

        if (attackSamples > 0 && i < attackSamples)
        {
            gain = (double) i / (double) attackSamples;
        }
        else if (releaseSamples > 0)
        {
            const long long releaseStart = (long long) attackSamples + (long long) holdSamples;

            if ((long long) i >= releaseStart)
                gain = std::max (0.0, 1.0 - (double) (i - releaseStart) / (double) releaseSamples);
        }

        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
            ir.getWritePointer (ch)[i] *= (float) gain;
    }
}

double ConvolverModule::curveValueAt (const std::array<float, numCurvePoints>& curve, double position01) noexcept
{
    const double position = juce::jlimit (0.0, 1.0, position01);
    const double scaled   = position * (double) (numCurvePoints - 1);

    const int    index    = (int) std::floor (scaled);
    const double fraction = scaled - (double) index;

    if (index >= numCurvePoints - 1)
        return (double) curve[(size_t) (numCurvePoints - 1)];

    return lerp ((double) curve[(size_t) index], (double) curve[(size_t) index + 1], fraction);
}

void ConvolverModule::applyDecayCurve (juce::AudioBuffer<float>& ir, double sampleRateToUse,
                                       const std::array<float, numCurvePoints>& curve)
{
    bool flat = true;

    for (const auto value : curve)
        if (std::abs (value - 1.0f) > 1.0e-6f)
            flat = false;

    if (flat || ir.getNumSamples() <= 1)
        return;

    // Drei Baender (Tiefen < 400 Hz, Mitten, Hoehen > 4 kHz) werden getrennt
    // exponentiell gedaempft und wieder summiert.
    constexpr double lowHz  = 400.0;
    constexpr double highHz = 4000.0;

    const double positionLow  = std::log (100.0 / kCurveLowHz) / std::log (kCurveHighHz / kCurveLowHz);
    const double positionMid  = std::log (1200.0 / kCurveLowHz) / std::log (kCurveHighHz / kCurveLowHz);
    const double positionHigh = std::log (8000.0 / kCurveLowHz) / std::log (kCurveHighHz / kCurveLowHz);

    const double factorLow  = std::clamp (curveValueAt (curve, positionLow),  0.01, 100.0);
    const double factorMid  = std::clamp (curveValueAt (curve, positionMid),  0.01, 100.0);
    const double factorHigh = std::clamp (curveValueAt (curve, positionHigh), 0.01, 100.0);

    const int    length  = ir.getNumSamples();
    const double invLen  = (length > 1) ? 1.0 / (double) (length - 1) : 0.0;
    const double logLow  = std::log (factorLow);
    const double logMid  = std::log (factorMid);
    const double logHigh = std::log (factorHigh);

    const double q = 0.70710678118654752440;

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        const auto low1 = dsp::designBiquad (dsp::BiquadKind::lowPass,  sampleRateToUse, lowHz,  q, 0.0);
        const auto low2 = low1;
        const auto hp1  = dsp::designBiquad (dsp::BiquadKind::highPass, sampleRateToUse, lowHz,  q, 0.0);
        const auto hp2  = hp1;
        const auto lp3  = dsp::designBiquad (dsp::BiquadKind::lowPass,  sampleRateToUse, highHz, q, 0.0);
        const auto lp4  = lp3;
        const auto hp3  = dsp::designBiquad (dsp::BiquadKind::highPass, sampleRateToUse, highHz, q, 0.0);
        const auto hp4  = hp3;

        dsp::Biquad lowFilterA, lowFilterB;
        dsp::Biquad midHighA, midHighB, midLowA, midLowB;
        dsp::Biquad highFilterA, highFilterB;

        lowFilterA.setCoefficients (low1);
        lowFilterB.setCoefficients (low2);
        midHighA.setCoefficients (hp1);
        midHighB.setCoefficients (hp2);
        midLowA.setCoefficients (lp3);
        midLowB.setCoefficients (lp4);
        highFilterA.setCoefficients (hp3);
        highFilterB.setCoefficients (hp4);

        auto* data = ir.getWritePointer (ch);

        for (int i = 0; i < length; ++i)
        {
            const double input = (double) data[i];
            const double position = (double) i * invLen;

            const double low  = lowFilterB.processSampleDouble (lowFilterA.processSampleDouble (input));
            const double hp   = midHighB.processSampleDouble (midHighA.processSampleDouble (input));
            const double mid  = midLowB.processSampleDouble (midLowA.processSampleDouble (hp));
            const double high = highFilterB.processSampleDouble (highFilterA.processSampleDouble (hp));

            const double lowGain  = std::exp (logLow  * position);
            const double midGain  = std::exp (logMid  * position);
            const double highGain = std::exp (logHigh * position);

            data[i] = (float) (low * lowGain + mid * midGain + high * highGain);
        }
    }
}

void ConvolverModule::rebuildKernel (bool synchronous)
{
    if (rawIr.getNumSamples() <= 0 || rawIr.getNumChannels() <= 0)
        return;

    juce::AudioBuffer<float> ir = rawIr;

    if (paramTargets[pReverse].load (std::memory_order_relaxed) > 0.5f)
        applyReverse (ir);

    applyStretch (ir, (double) paramTargets[pSpeed].load (std::memory_order_relaxed));

    applyDecayScale (ir, (double) paramTargets[pDecay].load (std::memory_order_relaxed));

    applyDecayCurve (ir, rawIrSampleRate, decayCurve);

    applyTimeEnvelope (ir, rawIrSampleRate,
                       (double) paramTargets[pEnvStart].load (std::memory_order_relaxed),
                       (double) paramTargets[pEnvAttack].load (std::memory_order_relaxed),
                       (double) paramTargets[pEnvHold].load (std::memory_order_relaxed),
                       (double) paramTargets[pEnvRelease].load (std::memory_order_relaxed));

    if (paramTargets[pNormalize].load (std::memory_order_relaxed) > 0.5f)
        applyNormalisation (ir);

    processedIrLength   = ir.getNumSamples();
    processedIrChannels = ir.getNumChannels();

    auto kernel = std::make_unique<IrKernel>();
    kernel->sampleRate = rawIrSampleRate;

    const int channels = ir.getNumChannels();
    const int length   = ir.getNumSamples();

    const bool wantsTrueStereo = channels >= 4
                                 && paramTargets[pTrueStereo].load (std::memory_order_relaxed) > 0.5f;

    auto makeStereoPair = [] (const juce::AudioBuffer<float>& source, int channelA, int channelB, int len)
    {
        juce::AudioBuffer<float> result (2, len);

        for (int i = 0; i < len; ++i)
        {
            result.getWritePointer (0)[i] = source.getReadPointer (channelA)[i];

            const int second = (channelB >= 0 && channelB < source.getNumChannels()) ? channelB : channelA;
            result.getWritePointer (1)[i] = source.getReadPointer (second)[i];
        }

        return result;
    };

    if (wantsTrueStereo)
    {
        kernel->trueStereo = true;
        kernel->a = makeStereoPair (ir, 0, 1, length);   // L->L, L->R
        kernel->b = makeStereoPair (ir, 2, 3, length);   // R->L, R->R
    }
    else if (channels >= 2)
    {
        // Stereo-IR: L->L und R->R (bei 4-Kanal-IR die Kanaele 0 und 3).
        const int rightChannel = (channels >= 4) ? 3 : 1;
        kernel->trueStereo = false;
        kernel->a = makeStereoPair (ir, 0, rightChannel, length);
    }
    else
    {
        kernel->trueStereo = false;
        kernel->a = makeStereoPair (ir, 0, 0, length);
    }

    if (synchronous && convolutionA != nullptr)
    {
        loadKernelIntoEngines (*kernel);
        reprepareEngines();
    }
    else if (auto* oldKernel = pendingKernel.exchange (kernel.release()))
    {
        delete oldKernel;
    }
}

bool ConvolverModule::loadImpulseFile (const juce::String& path)
{
    juce::File file (path);

    if (! file.existsAsFile())
    {
        log::w ("Convolver: IR-Datei nicht gefunden: " + path);
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        log::w ("Convolver: IR-Datei konnte nicht gelesen werden: " + path);
        return false;
    }

    const int numChannels = juce::jlimit (1, 4, (int) reader->numChannels);
    const int numSamples  = (int) juce::jmin ((juce::int64) 1 << 24, reader->lengthInSamples);

    if (numSamples <= 0)
        return false;

    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    reader->read (&buffer, 0, numSamples, 0, true, true);

    rawIr            = std::move (buffer);
    rawIrSampleRate  = reader->sampleRate;
    rawIrChannels    = numChannels;

    rebuildKernel (false);
    return true;
}

void ConvolverModule::loadImpulseFromBuffer (const juce::AudioBuffer<float>& ir, double irSampleRate)
{
    if (ir.getNumSamples() <= 0)
    {
        rawIr.setSize (0, 0);
        rawIrSampleRate = 0.0;
        rawIrChannels   = 0;
        processedIrLength = 0;
        processedIrChannels = 0;
        return;
    }

    rawIr = ir;
    rawIrSampleRate = (irSampleRate > 0.0) ? irSampleRate : sampleRate;
    rawIrChannels   = ir.getNumChannels();

    rebuildKernel (false);
}

void ConvolverModule::setImpulsePath (const juce::String& path)
{
    irPath = path;

    if (path.isEmpty())
    {
        rawIr.setSize (0, 0);
        rawIrSampleRate   = 0.0;
        rawIrChannels     = 0;
        processedIrLength = 0;
        processedIrChannels = 0;
        return;
    }

    loadImpulseFile (path);
}

void ConvolverModule::setEqCurve (const float* values, int count)
{
    if (values == nullptr || count <= 0)
        return;

    for (int i = 0; i < numCurvePoints; ++i)
        eqCurve[(size_t) i] = values[juce::jmin (i, count - 1)];
}

void ConvolverModule::setDecayCurve (const float* values, int count)
{
    if (values == nullptr || count <= 0)
        return;

    for (int i = 0; i < numCurvePoints; ++i)
        decayCurve[(size_t) i] = values[juce::jmin (i, count - 1)];

    if (rawIr.getNumSamples() > 0)
        rebuildKernel (false);
}

juce::ValueTree ConvolverModule::getState() const
{
    juce::ValueTree root (juce::Identifier ("Module"));

    if (irPath.isNotEmpty())
        root.setProperty (juce::Identifier ("ir"), juce::var (irPath), nullptr);

    juce::ValueTree eq (juce::Identifier ("Eq"));

    for (int i = 0; i < numCurvePoints; ++i)
    {
        juce::ValueTree point (juce::Identifier ("Pt"));
        point.setProperty (juce::Identifier ("y"), juce::var (eqCurve[(size_t) i]), nullptr);
        eq.appendChild (point, nullptr);
    }

    root.appendChild (eq, nullptr);

    juce::ValueTree decay (juce::Identifier ("Dc"));

    for (int i = 0; i < numCurvePoints; ++i)
    {
        juce::ValueTree point (juce::Identifier ("Pt"));
        point.setProperty (juce::Identifier ("y"), juce::var (decayCurve[(size_t) i]), nullptr);
        decay.appendChild (point, nullptr);
    }

    root.appendChild (decay, nullptr);

    return root;
}

void ConvolverModule::setState (const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    const auto pathValue = state.getProperty (juce::Identifier ("ir"));

    if (pathValue.isString())
    {
        const auto path = pathValue.toString();

        if (path != irPath)
            setImpulsePath (path);
    }

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        const auto child = state.getChild (i);
        const auto name  = child.getType().toString();

        const bool readEq = name.equalsIgnoreCase ("Eq");
        const bool readDc = name.equalsIgnoreCase ("Dc");

        if (! readEq && ! readDc)
            continue;

        float values[numCurvePoints] {};

        for (int p = 0; p < numCurvePoints; ++p)
            values[p] = (readEq ? eqCurve : decayCurve)[(size_t) p];

        int count = juce::jmin (numCurvePoints, child.getNumChildren());

        for (int p = 0; p < count; ++p)
        {
            const auto point = child.getChild (p);
            const auto value = point.getProperty (juce::Identifier ("y"));

            if (value.isDouble() || value.isInt())
                values[p] = (float) (double) value;
        }

        if (readEq)
            setEqCurve (values, numCurvePoints);
        else
            setDecayCurve (values, numCurvePoints);
    }
}

// =========================================================================
// Verarbeitung (Audio-Thread)
// =========================================================================

void ConvolverModule::updateEqFilters()
{
    for (int i = 0; i < numCurvePoints; ++i)
    {
        const double position = (double) i / (double) (numCurvePoints - 1);
        const double freq     = kCurveLowHz * std::pow (kCurveHighHz / kCurveLowHz, position);
        const double gainDb   = (double) eqCurve[(size_t) i];

        for (auto& channelBands : eqBands)
            channelBands[(size_t) i].setCoefficients (dsp::designBiquad (dsp::BiquadKind::peaking,
                                                                        sampleRate, freq, 1.2, gainDb));
    }
}

void ConvolverModule::applyEqAndTone (juce::AudioBuffer<float>& buffer, int numSamples)
{
    const double toneDb = ((double) paramValues[pTone] - kToneNeutral) / kToneNeutral * 12.0;
    toneGainDb = toneDb;

    if (std::abs (toneDb) > 1.0e-3)
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            toneLow[(size_t) ch].setCoefficients (dsp::designBiquad (dsp::BiquadKind::lowShelf,
                                                                    sampleRate, 250.0, 0.7071, -toneDb));
            toneHigh[(size_t) ch].setCoefficients (dsp::designBiquad (dsp::BiquadKind::highShelf,
                                                                     sampleRate, 3500.0, 0.7071, toneDb));
        }

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
                data[i] = toneHigh[(size_t) ch].processSample (toneLow[(size_t) ch].processSample (data[i]));
        }
    }

    bool eqActive = false;

    for (const auto value : eqCurve)
        if (std::abs (value) > 1.0e-3f)
            eqActive = true;

    if (! eqActive)
        return;

    updateEqFilters();

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            double value = (double) data[i];

            for (auto& band : eqBands[(size_t) ch])
                value = band.processSampleDouble (value);

            data[i] = (float) value;
        }
    }
}

void ConvolverModule::applyModulation (juce::AudioBuffer<float>& buffer, int numSamples)
{
    const double depthMs = (double) paramValues[pModDepth] / 100.0 * 5.0;

    if (depthMs <= 1.0e-4)
        return;

    const double rate   = juce::jmax (0.01, (double) paramValues[pModRate]);
    const double phaseInc = 2.0 * dsp::kPi * rate / sampleRate;
    const bool   quadrature = paramValues[pMod90] > 0.5;

    const double maxDelaySamples = depthMs * 0.001 * sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const double lfoL = std::sin (modPhase);
        const double lfoR = quadrature ? std::cos (modPhase) : std::sin (modPhase + dsp::kPi * 0.5);

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            auto& line = modLines[(size_t) ch];
            const int write = modWrite[(size_t) ch];

            line[(size_t) write] = buffer.getSample (ch, i);

            const double lfo     = (ch == 0) ? lfoL : lfoR;
            const double delay   = std::max (0.0, 0.5 * maxDelaySamples * (1.0 + lfo));
            const double readPos = (double) write - delay;

            double wrapped = readPos;

            while (wrapped < 0.0)
                wrapped += (double) modLineSize;

            const int    index    = (int) wrapped;
            const int    next     = (index + 1) % modLineSize;
            const double fraction = wrapped - (double) index;

            const double delayed = lerp ((double) line[(size_t) index],
                                         (double) line[(size_t) next], fraction);

            buffer.setSample (ch, i, (float) delayed);
            modWrite[(size_t) ch] = (write + 1) % modLineSize;
        }

        modPhase += phaseInc;

        if (modPhase >= 2.0 * dsp::kPi)
            modPhase -= 2.0 * dsp::kPi;
    }
}

void ConvolverModule::applyWidth (juce::AudioBuffer<float>& buffer, int numSamples)
{
    const double width = juce::jlimit (0.0, 2.0, (double) paramValues[pStereo] / 100.0);

    if (std::abs (width - 1.0) < 1.0e-4)
        return;

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const double mid  = 0.5 * ((double) left[i] + (double) right[i]);
        const double side = 0.5 * ((double) left[i] - (double) right[i]);

        left[i]  = (float) (mid + width * side);
        right[i] = (float) (mid - width * side);
    }
}

void ConvolverModule::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);

    if (! prepared || numSamples <= 0 || numChannels <= 0 || convolutionA == nullptr)
        return;

    if (numSamples > maxBlockSize)
        return;

    // Neues IR-Kernel wait-free uebernehmen (JUCE-Pfad fuer den Audio-Thread;
    // die Ueberblendung zwischen alter und neuer Engine macht JUCE selbst).
    if (auto* kernel = pendingKernel.exchange (nullptr))
    {
        loadKernelIntoEngines (*kernel);
        delete kernel;
    }

    // Ohne IR ist der Convolver neutral (Durchreichen, kein Pegelverlust).
    if (! hasImpulseResponse())
        return;

    // ---- 1. Parameter glaetten -------------------------------------------
    const double smoothing = smoothingCoefficient (numSamples, kParameterSmoothingMs);

    for (int i = 0; i < numParams; ++i)
    {
        const double target = (double) paramTargets[(size_t) i].load (std::memory_order_relaxed);
        paramValues[i] = dsp::smoothTowards (paramValues[i], target, smoothing);
    }

    const bool monoInput = paramValues[pMonoIn] > 0.5;

    // ---- 2. Eingang fuellen ---------------------------------------------
    const bool useTrueStereo = bKernelLoaded
                               && (rawIrChannels >= 4)
                               && (paramValues[pTrueStereo] > 0.5);

    for (int i = 0; i < numSamples; ++i)
    {
        double left  = (double) buffer.getSample (0, i);
        double right = (numChannels > 1) ? (double) buffer.getSample (1, i) : left;

        if (monoInput)
            left = right = 0.5 * (left + right);

        if (useTrueStereo)
        {
            inputA.setSample (0, i, (float) left);
            inputA.setSample (1, i, (float) left);
            inputB.setSample (0, i, (float) right);
            inputB.setSample (1, i, (float) right);
        }
        else
        {
            inputA.setSample (0, i, (float) left);
            inputA.setSample (1, i, (float) right);
        }
    }

    // ---- 3. Faltung ------------------------------------------------------
    juce::dsp::AudioBlock<float> blockInA (inputA);
    juce::dsp::AudioBlock<float> blockOutA (outputA);

    ConvolutionContext contextA;
    contextA.inputBlock  = juce::dsp::AudioBlock<const float> (blockInA);
    contextA.outputBlock = blockOutA;
    contextA.isBypassed  = false;

    convolutionA->process (contextA);

    if (useTrueStereo && convolutionB != nullptr)
    {
        juce::dsp::AudioBlock<float> blockInB (inputB);
        juce::dsp::AudioBlock<float> blockOutB (outputB);

        ConvolutionContext contextB;
        contextB.inputBlock  = juce::dsp::AudioBlock<const float> (blockInB);
        contextB.outputBlock = blockOutB;
        contextB.isBypassed  = false;

        convolutionB->process (contextB);
    }

    // ---- 4. Nass-Signal zusammensetzen -----------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        double left  = (double) outputA.getSample (0, i);
        double right = (double) outputA.getSample (1, i);

        if (useTrueStereo)
        {
            left  += (double) outputB.getSample (0, i);
            right += (double) outputB.getSample (1, i);
        }

        wetBuffer.setSample (0, i, (float) left);
        wetBuffer.setSample (1, i, (float) right);
    }

    // ---- 5. Predelay -----------------------------------------------------
    const int predelay = juce::jlimit (0, juce::jmax (0, predelaySamples),
                                       (int) std::lround ((double) paramValues[pPredelay] * 0.001 * sampleRate));

    if (predelay > 0)
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            auto& line = predelayLines[(size_t) ch];
            const int size = (int) line.size();
            int write = predelayWrite[(size_t) ch];

            for (int i = 0; i < numSamples; ++i)
            {
                const float input = wetBuffer.getSample (ch, i);
                line[(size_t) write] = input;

                int read = write - predelay;

                while (read < 0)
                    read += size;

                wetBuffer.setSample (ch, i, line[(size_t) read]);

                write = (write + 1) % size;
            }

            predelayWrite[(size_t) ch] = write;
        }
    }

    // ---- 6. Modulation, Tone/EQ, Width -----------------------------------
    applyModulation (wetBuffer, numSamples);
    applyEqAndTone (wetBuffer, numSamples);
    applyWidth (wetBuffer, numSamples);

    // ---- 7. Pegel --------------------------------------------------------
    const double wetGain = juce::jlimit (0.0, 1.0, (double) paramValues[pWet] / 100.0)
                           * dsp::dbToGain ((double) paramValues[pIrGain]);

    for (int ch = 0; ch < juce::jmin (numChannels, maxChannels); ++ch)
    {
        auto* source = wetBuffer.getReadPointer (ch);
        auto* target = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
            target[i] = (float) ((double) source[i] * wetGain);
    }
}

} // namespace bfka
