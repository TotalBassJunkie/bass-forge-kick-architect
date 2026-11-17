#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <memory>

#include "fx/modules/DynamicEqModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr double kPi         = 3.14159265358979323846;

    std::unique_ptr<DynamicEqModule> makeModule()
    {
        auto module = std::make_unique<DynamicEqModule>();
        module->prepare (kSampleRate, kBlockSize);
        return module;
    }

    int paramIndex (const juce::String& id)
    {
        DynamicEqModule module;

        const auto& params = module.params();

        for (size_t i = 0; i < params.size(); ++i)
            if (params[i].id == id)
                return (int) i;

        FAIL ("Unbekannte Parameter-ID: " << id);
        return -1;
    }

    void setParamById (DynamicEqModule& module, const juce::String& id, float value)
    {
        const int index = paramIndex (id);

        if (index >= 0)
            module.setParam (index, value);
    }

    /** Neutralisiert alles, was die Messung sonst beeinflusst. */
    void setNeutralParams (DynamicEqModule& module)
    {
        setParamById (module, "autoGain", 0.0f);
        setParamById (module, "delta",    0.0f);
        setParamById (module, "dryMix",   0.0f);
        setParamById (module, "channels", 1.0f);
        setParamById (module, "outGain",  0.0f);
        setParamById (module, "wideDyn",  0.0f);
        setParamById (module, "hpActive", 0.0f);
        setParamById (module, "lpActive", 0.0f);
    }

    double measureSineGainDb (DynamicEqModule& module, double freqHz, double amplitude,
                              double settleSeconds = 0.4, double measureSeconds = 0.4)
    {
        const int settleBlocks  = juce::jmax (1, (int) std::lround (settleSeconds * kSampleRate / (double) kBlockSize));
        const int measureBlocks = juce::jmax (1, (int) std::lround (measureSeconds * kSampleRate / (double) kBlockSize));

        juce::AudioBuffer<float> buffer (2, kBlockSize);

        const double increment = 2.0 * kPi * freqHz / kSampleRate;
        double phase = 0.0;
        double sumOfSquares = 0.0;
        long long measuredSamples = 0;

        for (int block = 0; block < settleBlocks + measureBlocks; ++block)
        {
            for (int s = 0; s < kBlockSize; ++s)
            {
                const auto value = (float) (amplitude * std::sin (phase));
                phase += increment;

                buffer.setSample (0, s, value);
                buffer.setSample (1, s, value);
            }

            module.process (buffer);

            if (block >= settleBlocks)
            {
                for (int s = 0; s < kBlockSize; ++s)
                {
                    const double y = (double) buffer.getSample (0, s);
                    sumOfSquares += y * y;
                    ++measuredSamples;
                }
            }
        }

        const double inputRms  = amplitude / std::sqrt (2.0);
        const double outputRms = std::sqrt (sumOfSquares / (double) juce::jmax (1LL, measuredSamples));

        return 20.0 * std::log10 (juce::jmax (outputRms, 1.0e-12) / inputRms);
    }
} // namespace

// =========================================================================
// Kopfdaten
// =========================================================================

TEST_CASE ("Dynamic EQ: Parameterliste deckt alle Nova-Funktionen ab", "[fx][dyneq]")
{
    DynamicEqModule module;

    CHECK (module.type() == FxType::dynamicEq);
    CHECK (module.displayName() == "Dynamic EQ");
    CHECK (module.latencySamples() == 0);

    const juce::StringArray globals
    {
        "outGain", "dryMix", "hpActive", "hpFreq", "hpType", "lpActive", "lpFreq", "lpType",
        "wideGain", "wideDyn", "wideThresh", "wideRatio", "wideAttack", "wideRelease",
        "autoGain", "delta", "channels", "quality", "sidechain"
    };

    const auto& params = module.params();

    REQUIRE (params.size() == (size_t) (globals.size() + 4 * 11));

    for (int i = 0; i < globals.size(); ++i)
    {
        INFO ("Global " << i);
        REQUIRE (params[(size_t) i].id == globals[i]);
    }

    const juce::StringArray bandParts
    {
        "active", "type", "freq", "gain", "q", "dyn", "split", "thresh", "ratio", "attack", "release"
    };

    int index = globals.size();

    for (int band = 1; band <= 4; ++band)
    {
        for (const auto& part : bandParts)
        {
            const juce::String expected = "b" + juce::String (band) + "_" + part;
            INFO ("Band-Parameter " << expected);
            REQUIRE (params[(size_t) index].id == expected);
            ++index;
        }
    }
}

TEST_CASE ("Dynamic EQ: Parameter werden geklemmt", "[fx][dyneq]")
{
    DynamicEqModule module;

    module.setParam (paramIndex ("outGain"), 100.0f);
    REQUIRE_THAT (module.getParam (paramIndex ("outGain")), WithinAbs (24.0f, 1.0e-6f));

    module.setParam (paramIndex ("b1_ratio"), 0.0f);
    REQUIRE_THAT (module.getParam (paramIndex ("b1_ratio")), WithinAbs (1.0f, 1.0e-6f));

    REQUIRE_THAT (module.getParam (9999), WithinAbs (0.0f, 1.0e-6f));
}

// =========================================================================
// Statischer EQ
// =========================================================================

TEST_CASE ("Dynamic EQ: statisches Bell-Band hebt die Mitten an", "[fx][dyneq]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "b1_active", 1.0f);
    setParamById (*module, "b1_type",   1.0f);   // Bell
    setParamById (*module, "b1_freq",   1000.0f);
    setParamById (*module, "b1_gain",   6.0f);
    setParamById (*module, "b1_q",      0.9f);

    const double gainAt1k  = measureSineGainDb (*module, 1000.0, 0.2);
    const double gainAt100 = measureSineGainDb (*module, 100.0, 0.2);

    REQUIRE_THAT (gainAt1k, WithinAbs (6.0, 1.0));
    REQUIRE (std::abs (gainAt100) < 1.5);
}

TEST_CASE ("Dynamic EQ: Hochpass dämpft die Tiefen", "[fx][dyneq]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "hpActive", 1.0f);
    setParamById (*module, "hpFreq",   1000.0f);
    setParamById (*module, "hpType",   1.0f);   // 12 dB/Okt

    const double gainAt100  = measureSineGainDb (*module, 100.0, 0.2);
    const double gainAt8k   = measureSineGainDb (*module, 8000.0, 0.2);

    REQUIRE (gainAt100 < -12.0);
    REQUIRE (std::abs (gainAt8k) < 1.0);
}

TEST_CASE ("Dynamic EQ: Tiefpass dämpft die Hoehen", "[fx][dyneq]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "lpActive", 1.0f);
    setParamById (*module, "lpFreq",   1000.0f);
    setParamById (*module, "lpType",   1.0f);

    const double gainAt8k  = measureSineGainDb (*module, 8000.0, 0.2);

    REQUIRE (gainAt8k < -12.0);
}

// =========================================================================
// Dynamik
// =========================================================================

TEST_CASE ("Dynamic EQ: laute Signale loesen die Band-Dynamik aus", "[fx][dyneq]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "b1_active",  1.0f);
    setParamById (*module, "b1_type",    1.0f);
    setParamById (*module, "b1_freq",    1000.0f);
    setParamById (*module, "b1_gain",    12.0f);
    setParamById (*module, "b1_q",       0.9f);
    setParamById (*module, "b1_dyn",     1.0f);   // On
    setParamById (*module, "b1_split",   1.0f);   // eigener Detektor
    setParamById (*module, "b1_thresh",  -30.0f);
    setParamById (*module, "b1_ratio",   4.0f);
    setParamById (*module, "b1_attack",  5.0f);
    setParamById (*module, "b1_release", 80.0f);

    const double loudGain = measureSineGainDb (*module, 1000.0, 0.5);

    // Statisch +12 dB, dynamisch ~18 dB Absenkung -> etwa -6 dB.
    REQUIRE (loudGain < -1.0);
    REQUIRE (module->bandDynamicGainDb (0) < -5.0f);

    module->reset();

    // Leises Signal bleibt unter der Schwelle -> statische Anhebung bleibt.
    const double quietGain = measureSineGainDb (*module, 1000.0, 0.003);

    REQUIRE (quietGain > 8.0);
}

TEST_CASE ("Dynamic EQ: verlinkte Baender folgen dem W-Band", "[fx][dyneq]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "b1_active",  1.0f);
    setParamById (*module, "b1_type",    1.0f);
    setParamById (*module, "b1_freq",    1000.0f);
    setParamById (*module, "b1_gain",    12.0f);
    setParamById (*module, "b1_dyn",     1.0f);
    setParamById (*module, "b1_split",   0.0f);   // verlinkt
    setParamById (*module, "b1_thresh",  -60.0f); // Band-Schwelle wird ignoriert
    setParamById (*module, "wideDyn",    1.0f);
    setParamById (*module, "wideThresh", -10.0f);
    setParamById (*module, "wideRatio",  4.0f);

    const double loudGain = measureSineGainDb (*module, 1000.0, 0.5);

    // W-Band-Level (~ -6 dB) liegt ueber -10 dB -> das Band wird mitgeregelt.
    REQUIRE (module->bandDynamicGainDb (0) < -1.0f);
    REQUIRE (loudGain < 10.0);
}

// =========================================================================
// Master-Funktionen
// =========================================================================

TEST_CASE ("Dynamic EQ: Dry Mix 100 % ist Bypass", "[fx][dyneq]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "b1_active", 1.0f);
    setParamById (*module, "b1_type",   1.0f);
    setParamById (*module, "b1_freq",   1000.0f);
    setParamById (*module, "b1_gain",   12.0f);
    setParamById (*module, "dryMix",    100.0f);

    const double gain = measureSineGainDb (*module, 1000.0, 0.2);

    REQUIRE (std::abs (gain) < 0.5);
}

TEST_CASE ("Dynamic EQ: Delta liefert die Differenz", "[fx][dyneq]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "b1_active", 1.0f);
    setParamById (*module, "b1_type",   1.0f);
    setParamById (*module, "b1_freq",   1000.0f);
    setParamById (*module, "b1_gain",   12.0f);
    setParamById (*module, "delta",     1.0f);

    // dry - wet = x - 4x = -3x  ->  +9,54 dB.
    const double gain = measureSineGainDb (*module, 1000.0, 0.2);

    REQUIRE_THAT (gain, WithinAbs (20.0 * std::log10 (3.0), 1.5));
}

TEST_CASE ("Dynamic EQ: Mono-Ziel summiert beide Kanaele", "[fx][dyneq]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "channels", 0.0f);   // Mono

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    for (int block = 0; block < 20; ++block)
    {
        for (int s = 0; s < kBlockSize; ++s)
        {
            buffer.setSample (0, s, 1.0f);
            buffer.setSample (1, s, 0.0f);
        }

        module->process (buffer);

        if (block > 12)
        {
            for (int s = 0; s < kBlockSize; s += 16)
                REQUIRE_THAT (buffer.getSample (0, s), WithinAbs (buffer.getSample (1, s), 1.0e-4f));
        }
    }
}

TEST_CASE ("Dynamic EQ: EQ Auto Gain kompensiert statische Anhebung", "[fx][dyneq]")
{
    auto measureHighShelf = [] (bool autoGain)
    {
        auto module = makeModule();
        setNeutralParams (*module);
        setParamById (*module, "autoGain", autoGain ? 1.0f : 0.0f);
        setParamById (*module, "b1_active", 1.0f);
        setParamById (*module, "b1_type",   2.0f);   // High Shelf
        setParamById (*module, "b1_freq",   4000.0f);
        setParamById (*module, "b1_gain",   12.0f);

        // 2 s einschwingen, danach oberhalb der Shelf-Ecke messen.
        return measureSineGainDb (*module, 12000.0, 0.1, 2.0, 0.3);
    };

    const double without = measureHighShelf (false);
    const double with    = measureHighShelf (true);

    REQUIRE (without > 9.0);
    REQUIRE (with < without - 2.0);
}
