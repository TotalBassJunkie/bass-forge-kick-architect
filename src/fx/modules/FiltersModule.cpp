#include "fx/modules/FiltersModule.h"

#include <cmath>

namespace bfka
{

namespace
{
    constexpr double kPi = juce::MathConstants<double>::pi;

    /** Butterworth-Guete einer Kaskade aus `poles` Polen. */
    double butterworthQ (int poleIndex, int poles) noexcept
    {
        // Q_k = 1 / (2 * cos(pi * (2k+1) / (2N)))  fuer k = 0..N/2-1
        const auto k = (double) poleIndex;
        const auto n = (double) poles;
        const auto angle = kPi * (2.0 * k + 1.0) / (2.0 * n);
        const auto c = std::cos (angle);

        if (std::abs (c) < 1.0e-9)
            return 100.0;

        return 1.0 / (2.0 * std::abs (c));
    }

    /** Resonanzanhebung: RESO 0 -> neutrale Guete, RESO 1 -> deutlich spitz. */
    double applyResonance (double q, double resonance, int stages) noexcept
    {
        // Die Anhebung sitzt auf der letzten Stufe; sie wird mit der Anzahl der
        // Stufen skaliert, damit ein 30-dB-Filter nicht zurueckhaltender klingt
        // als ein 12-dB-Filter.
        const auto boost = 1.0 + 5.0 * juce::jlimit (0.0, 1.0, resonance) * (double) stages;
        return juce::jlimit (0.4, 30.0, q * boost);
    }

    void makeLowPass (double sr, double freq, double q,
                      double& b0, double& b1, double& b2, double& a0, double& a1, double& a2) noexcept
    {
        const auto w0 = 2.0 * kPi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const auto cosw = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * juce::jmax (0.1, q));

        b0 = (1.0 - cosw) * 0.5;
        b1 = 1.0 - cosw;
        b2 = (1.0 - cosw) * 0.5;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosw;
        a2 = 1.0 - alpha;
    }

    void makeHighPass (double sr, double freq, double q,
                       double& b0, double& b1, double& b2, double& a0, double& a1, double& a2) noexcept
    {
        const auto w0 = 2.0 * kPi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const auto cosw = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * juce::jmax (0.1, q));

        b0 = (1.0 + cosw) * 0.5;
        b1 = -(1.0 + cosw);
        b2 = (1.0 + cosw) * 0.5;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosw;
        a2 = 1.0 - alpha;
    }

    void makeBandPass (double sr, double freq, double q,
                       double& b0, double& b1, double& b2, double& a0, double& a1, double& a2) noexcept
    {
        const auto w0 = 2.0 * kPi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const auto cosw = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * juce::jmax (0.1, q));

        b0 = alpha;
        b1 = 0.0;
        b2 = -alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosw;
        a2 = 1.0 - alpha;
    }

    void makeNotch (double sr, double freq, double q,
                    double& b0, double& b1, double& b2, double& a0, double& a1, double& a2) noexcept
    {
        const auto w0 = 2.0 * kPi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const auto cosw = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * juce::jmax (0.1, q));

        b0 = 1.0;
        b1 = -2.0 * cosw;
        b2 = 1.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosw;
        a2 = 1.0 - alpha;
    }

    void makeAllPass (double sr, double freq, double q,
                      double& b0, double& b1, double& b2, double& a0, double& a1, double& a2) noexcept
    {
        const auto w0 = 2.0 * kPi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const auto cosw = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * juce::jmax (0.1, q));

        b0 = 1.0 - alpha;
        b1 = -2.0 * cosw;
        b2 = 1.0 + alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cosw;
        a2 = 1.0 - alpha;
    }
}

// ---------------------------------------------------------------------------

float FiltersModule::Biquad::process (float in, int channel) noexcept
{
    const auto ch = (size_t) juce::jlimit (0, kMaxChannels - 1, channel);
    const auto x = (double) in;
    const auto y = b0 * x + z1[ch];

    z1[ch] = b1 * x - a1 * y + z2[ch];
    z2[ch] = b2 * x - a2 * y;

    // Schutz gegen Entgleiten bei extremen Gueten.
    if (! std::isfinite (y))
    {
        z1[ch] = z2[ch] = 0.0;
        return 0.0f;
    }

    return (float) y;
}

void FiltersModule::Biquad::reset() noexcept
{
    for (auto& z : z1) z = 0.0;
    for (auto& z : z2) z = 0.0;
}

// ---------------------------------------------------------------------------

FiltersModule::FiltersModule()
{
    descriptors = {
        { "type",     "Type",      0.0f,     5.0f,     0.0f, "" },
        { "slope",    "Slope",     0.0f,     4.0f,     1.0f, "" },
        { "cutoff",   "Cutoff",   20.0f, 20000.0f,  1000.0f, "Hz" },
        { "reso",     "Resonance", 0.0f,   100.0f,    20.0f, "%" },
        { "drive",    "Drive",     0.0f,   100.0f,     0.0f, "%" },
        { "polarity", "Polarity",  0.0f,     1.0f,     1.0f, "" },
    };

    values.fill (0.0f);

    for (size_t i = 0; i < descriptors.size(); ++i)
        values[i] = descriptors[i].defaultValue;
}

FiltersModule::~FiltersModule() = default;

void FiltersModule::prepare (double sampleRate, int maxBlockSize)
{
    preparedSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    preparedBlockSize  = juce::jmax (1, maxBlockSize);

    combBuffer.setSize (kMaxChannels, juce::jmax (2, (int) preparedSampleRate), false, true, true);

    reset();
}

void FiltersModule::reset()
{
    for (auto& f : filters)
        f.reset();

    for (auto& a : allpasses)
        a.reset();

    combBuffer.clear();
    combWritePos = 0;
}

void FiltersModule::setParam (int index, float value)
{
    if (! juce::isPositiveAndBelow (index, (int) numParams))
        return;

    const auto& desc = descriptors[(size_t) index];
    const auto clamped = juce::jlimit (desc.minValue, desc.maxValue, value);

    values[(size_t) index] = clamped;

    // DISPLAY-Einheiten: cutoff in Hz -> logarithmisch ist Sache der UI.
}

float FiltersModule::getParam (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) numParams))
        return 0.0f;

    return values[(size_t) index];
}

void FiltersModule::updateCoefficients (const BlockParams& params)
{
    const auto sr = preparedSampleRate;

    const auto assign = [&] (Biquad& target, double b0, double b1, double b2,
                             double a0, double a1, double a2)
    {
        target.b0 = b0 / a0;
        target.b1 = b1 / a0;
        target.b2 = b2 / a0;
        target.a1 = a1 / a0;
        target.a2 = a2 / a0;

        if (! std::isfinite (target.b0) || ! std::isfinite (target.a1)
            || ! std::isfinite (target.a2))
        {
            target.b0 = 1.0;
            target.b1 = target.b2 = target.a1 = target.a2 = 0.0;
        }
    };

    activeStages = 0;
    activeNotches = 0;
    bypassFilters = false;

    if (params.type == FilterType::phaser)
    {
        // SLOPE waehlt die Anzahl der Kerben (1..6).
        activeNotches = juce::jlimit (1, kMaxPhaserNotches, params.poles + 1);

        for (int i = 0; i < activeNotches; ++i)
        {
            // Die Kerben liegen gestaffelt um CUTOFF (ca. eine Terz Abstand).
            const auto offset = std::pow (2.0, (double) (i - activeNotches / 2) / 6.0);
            const auto freq = juce::jlimit (20.0, sr * 0.45, (double) params.cutoff * offset);
            double b0, b1, b2, a0, a1, a2;

            const auto q = 0.5 + 1.5 * (double) params.reso;
            makeAllPass (sr, freq, q, b0, b1, b2, a0, a1, a2);
            assign (allpasses[(size_t) i], b0, b1, b2, a0, a1, a2);
        }

        return;
    }

    if (params.type == FilterType::comb)
    {
        activeStages = 0;
        return;
    }

    // LP/HP: Butterworth-Kaskade. Ein Biquad bringt 12 dB/Oktave, eine
    // First-Order-Stufe 6 dB/Oktave. Aus `poles` (1..5) folgt damit genau die
    // gewuenschte Flanke: 1 -> 6 dB, 2 -> 12 dB, 3 -> 18 dB, 4 -> 24 dB, 5 -> 30 dB.
    if (params.type == FilterType::band)
    {
        const auto biquads = (params.poles >= 2) ? 2 : 1;

        for (int i = 0; i < biquads; ++i)
        {
            double b0, b1, b2, a0, a1, a2;
            auto q = (biquads == 2) ? butterworthQ (i, 4) : 0.7071;

            if (i == biquads - 1)
                q = applyResonance (q, (double) params.reso, biquads);

            makeBandPass (sr, params.cutoff, q, b0, b1, b2, a0, a1, a2);
            assign (filters[(size_t) i], b0, b1, b2, a0, a1, a2);
        }

        activeStages = biquads;
        return;
    }

    if (params.type == FilterType::notch)
    {
        // Eine Kerbe genuegt; SLOPE macht sie breiter, RESO schmaler.
        double b0, b1, b2, a0, a1, a2;
        const auto width = 0.35 + 0.35 * (double) params.poles + 4.0 * (double) params.reso;
        const auto q = juce::jlimit (0.2, 20.0, width);

        makeNotch (sr, params.cutoff, q, b0, b1, b2, a0, a1, a2);
        assign (filters[0], b0, b1, b2, a0, a1, a2);
        activeStages = 1;
        return;
    }

    const auto poles = juce::jlimit (1, 5, params.poles);
    const auto biquadCount = poles / 2;
    const auto hasFirstOrder = (poles % 2) != 0;

    int stageIndex = 0;

    for (int i = 0; i < biquadCount; ++i)
    {
        double b0, b1, b2, a0, a1, a2;

        auto q = butterworthQ (i, biquadCount * 2);

        // Die Resonanz sitzt auf der letzten Stufe.
        if (i == biquadCount - 1 && ! hasFirstOrder)
            q = applyResonance (q, (double) params.reso, poles);

        if (params.type == FilterType::lowPass)
            makeLowPass (sr, params.cutoff, q, b0, b1, b2, a0, a1, a2);
        else
            makeHighPass (sr, params.cutoff, q, b0, b1, b2, a0, a1, a2);

        assign (filters[(size_t) stageIndex++], b0, b1, b2, a0, a1, a2);
    }

    if (hasFirstOrder && stageIndex < kMaxStages)
    {
        // First-Order-Stufe: 6 dB/Oktave, als Biquad mit b2 = a2 = 0 geschrieben.
        const auto w = std::exp (-2.0 * kPi * juce::jlimit (10.0, sr * 0.49, (double) params.cutoff) / sr);
        Biquad& stage = filters[(size_t) stageIndex++];

        if (params.type == FilterType::lowPass)
        {
            stage.b0 = 1.0 - w;
            stage.a1 = -w;
        }
        else
        {
            // Hochpass erster Ordnung: y = k*(x - x[n-1]) + w*y[n-1]
            stage.b0 = (1.0 + w) * 0.5;
            stage.b1 = -(1.0 + w) * 0.5;
            stage.a1 = -w;
        }

        stage.b2 = 0.0;
        stage.a2 = 0.0;
    }

    activeStages = stageIndex;
}

void FiltersModule::processFilters (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int n = 0; n < numSamples; ++n)
        {
            auto x = data[n];

            for (int s = 0; s < activeStages; ++s)
                x = filters[(size_t) s].process (x, ch);

            data[n] = x;
        }
    }
}

void FiltersModule::processComb (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples,
                                 const BlockParams& params)
{
    const auto maxDelay = juce::jmax (2, combBuffer.getNumSamples());
    auto delay = juce::jlimit (2, maxDelay,
                              (int) std::lround (preparedSampleRate / juce::jmax (20.0f, params.cutoff)));

    const auto feedback = juce::jlimit (0.0f, 0.9f, params.reso * 0.9f);
    const auto sign = params.invert ? -1.0f : 1.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* ring = combBuffer.getWritePointer (ch);
            auto* data = buffer.getWritePointer (ch);

            const auto readPos = (combWritePos + maxDelay - delay) % maxDelay;
            const auto delayed = ring[readPos];
            const auto in = data[n];

            ring[combWritePos] = in + sign * feedback * delayed;
            data[n] = in + 0.5f * sign * delayed;
        }

        combWritePos = (combWritePos + 1) % maxDelay;
    }
}

void FiltersModule::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = juce::jmin (buffer.getNumChannels(), kMaxChannels);
    const auto numSamples  = buffer.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0)
        return;

    BlockParams params;
    params.type   = (FilterType) juce::jlimit (0, 5, (int) std::lround (values[(size_t) paramType]));
    params.poles  = 1 + juce::jlimit (0, 4, (int) std::lround (values[(size_t) paramSlope]));
    params.cutoff = juce::jlimit (20.0f, (float) (preparedSampleRate * 0.49),
                                  values[(size_t) paramCutoff]);
    params.reso   = juce::jlimit (0.0f, 100.0f, values[(size_t) paramReso]) * 0.01f;
    params.drive  = juce::jlimit (0.0f, 100.0f, values[(size_t) paramDrive]) * 0.01f;
    params.invert = values[(size_t) paramPolarity] >= 0.5f;

    // 1) DRIVE: sanfte Saettigung vor dem Filter.
    if (params.drive > 0.0f)
    {
        const auto gain = 1.0f + 5.0f * params.drive;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int n = 0; n < numSamples; ++n)
            {
                const auto driven = std::tanh (data[n] * gain);
                data[n] = data[n] + params.drive * (driven - data[n]);
            }
        }
    }

    // 2) Filter.
    updateCoefficients (params);

    if (params.type == FilterType::comb)
    {
        processComb (buffer, numChannels, numSamples, params);
        return;
    }

    if (params.type == FilterType::phaser)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int n = 0; n < numSamples; ++n)
            {
                auto x = data[n];

                for (int i = 0; i < activeNotches; ++i)
                    x = allpasses[(size_t) i].process (x, ch);

                // Klassische Phaser-Mischung: trocken + nasse Allpass-Summe.
                const auto wet = params.invert ? -x : x;
                data[n] = 0.5f * data[n] + 0.5f * wet;
            }
        }

        return;
    }

    processFilters (buffer, numChannels, numSamples);
}

} // namespace bfka
