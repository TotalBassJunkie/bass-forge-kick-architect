#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <memory>
#include <vector>

#include "fx/modules/MultibandModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr double kPi         = 3.14159265358979323846;

    std::unique_ptr<MultibandModule> makeModule()
    {
        auto module = std::make_unique<MultibandModule>();
        module->prepare (kSampleRate, kBlockSize);
        return module;
    }

    /** Setzt einen Parameter per ID (Index in der Parametertabelle). */
    void setParamById (MultibandModule& module, const juce::String& id, float value)
    {
        const auto& params = module.params();

        for (size_t i = 0; i < params.size(); ++i)
        {
            if (params[i].id == id)
            {
                module.setParam ((int) i, value);
                return;
            }
        }

        FAIL ("Unbekannte Parameter-ID: " << id);
    }

    /** Laesst das Modul einschwingen und misst danach den RMS-Gewinn in dB. */
    double measureSineGainDb (MultibandModule& module, double freqHz, double amplitude,
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

TEST_CASE ("Multiband: Parameterliste in Referenz- und OTT-Reihenfolge", "[fx][multiband]")
{
    MultibandModule module;

    CHECK (module.type() == FxType::multiband);
    CHECK (module.displayName() == "Multiband");
    CHECK (module.latencySamples() == 0);

    const juce::StringArray expectedIds
    {
        "depth", "time", "x1", "x2", "gl", "gm", "gh", "linear",
        "inGain", "cleanXov",
        "thL", "thM", "thH",
        "buL", "buM", "buH",
        "bdL", "bdM", "bdH",
        "upwdStr", "dnwdStr"
    };

    const auto& params = module.params();
    REQUIRE (params.size() == (size_t) expectedIds.size());

    for (int i = 0; i < expectedIds.size(); ++i)
    {
        INFO ("Index " << i);
        REQUIRE (params[(size_t) i].id == expectedIds[i]);
        REQUIRE (params[(size_t) i].minValue < params[(size_t) i].maxValue);
        REQUIRE (params[(size_t) i].defaultValue >= params[(size_t) i].minValue);
        REQUIRE (params[(size_t) i].defaultValue <= params[(size_t) i].maxValue);
    }

    // Referenz-Attribute zuerst -> Bridge-Plaetze p0..p7 bleiben stabil.
    REQUIRE (params[0].id == "depth");
    REQUIRE (params[7].id == "linear");

    // Defaults wie im Original (neutral).
    REQUIRE_THAT (module.getParam (0), WithinAbs (50.0f, 1.0e-6f));   // depth 50 %
    REQUIRE_THAT (module.getParam (1), WithinAbs (50.0f, 1.0e-6f));   // time 50 %
    REQUIRE_THAT (module.getParam (2), WithinAbs (120.0f, 1.0e-6f));  // x1
    REQUIRE_THAT (module.getParam (3), WithinAbs (2500.0f, 1.0e-6f)); // x2
}

TEST_CASE ("Multiband: Parameter werden geklemmt", "[fx][multiband]")
{
    MultibandModule module;

    module.setParam (0, 500.0f);
    REQUIRE_THAT (module.getParam (0), WithinAbs (100.0f, 1.0e-6f));

    module.setParam (0, -20.0f);
    REQUIRE_THAT (module.getParam (0), WithinAbs (0.0f, 1.0e-6f));

    module.setParam (2, 5.0f);   // x1 min 20 Hz
    REQUIRE_THAT (module.getParam (2), WithinAbs (20.0f, 1.0e-6f));

    REQUIRE_THAT (module.getParam (999), WithinAbs (0.0f, 1.0e-6f));
    module.setParam (999, 1.0f); // darf nicht abstuerzen
}

// =========================================================================
// Linearphasiger Split
// =========================================================================

TEST_CASE ("Multiband: Linearphase-Split meldet Latenz und rekonstruiert", "[fx][multiband]")
{
    auto module = makeModule();

    setParamById (*module, "depth",    0.0f);
    setParamById (*module, "upwdStr",  0.0f);
    setParamById (*module, "dnwdStr",  0.0f);
    setParamById (*module, "inGain",   0.0f);
    setParamById (*module, "linear",   1.0f);

    // Latenz ist die halbe Fensterlaenge.
    REQUIRE (module->latencySamples() == (MultibandModule::linearPhaseTaps - 1) / 2);

    module->reset();

    const int latency = module->latencySamples();
    const int totalSamples = 160 * kBlockSize;

    std::vector<float> input ((size_t) totalSamples, 0.0f);
    std::vector<float> output ((size_t) totalSamples, 0.0f);

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    const double increment = 2.0 * kPi * 1000.0 / kSampleRate;
    double phase = 0.0;

    for (int block = 0; block < totalSamples / kBlockSize; ++block)
    {
        for (int s = 0; s < kBlockSize; ++s)
        {
            const float value = (float) (0.3 * std::sin (phase));
            phase += increment;

            input[(size_t) (block * kBlockSize + s)] = value;
            buffer.setSample (0, s, value);
            buffer.setSample (1, s, value);
        }

        module->process (buffer);

        for (int s = 0; s < kBlockSize; ++s)
            output[(size_t) (block * kBlockSize + s)] = buffer.getSample (0, s);
    }

    // Nach dem Einschwingen ist die Rekonstruktion exakt (nur um latency verz.
    double maxError = 0.0;

    for (int i = 20000; i < totalSamples; ++i)
        maxError = std::max (maxError, std::abs ((double) output[(size_t) i] - (double) input[(size_t) (i - latency)]));

    REQUIRE (maxError < 5.0e-4);
}

TEST_CASE ("Multiband: Linearphase verschiebt einen Impuls um die Latenz", "[fx][multiband]")
{
    MultibandModule module;
    setParamById (module, "depth", 0.0f);
    setParamById (module, "linear", 1.0f);
    setParamById (module, "upwdStr", 0.0f);
    setParamById (module, "dnwdStr", 0.0f);
    module.prepare (kSampleRate, kBlockSize);

    const int latency = module.latencySamples();
    REQUIRE (latency > 0);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    int peakIndex = -1;
    float peakValue = 0.0f;

    // Parameter erst einschwingen lassen (Glättung 20 ms), dann Impuls.
    for (int block = 0; block < 80; ++block)
    {
        buffer.clear();
        module.process (buffer);
    }

    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();

        if (block == 0)
        {
            buffer.setSample (0, 0, 1.0f);
            buffer.setSample (1, 0, 1.0f);
        }

        module.process (buffer);

        for (int s = 0; s < kBlockSize; ++s)
        {
            const float value = std::abs (buffer.getSample (0, s));

            if (value > peakValue)
            {
                peakValue = value;
                peakIndex = block * kBlockSize + s;
            }
        }
    }

    REQUIRE (peakIndex == latency);
    REQUIRE_THAT (peakValue, WithinAbs (1.0f, 1.0e-3f));
}

// =========================================================================
// Dynamik
// =========================================================================

TEST_CASE ("Multiband: Abwaerts-Kompression senkt laute Baender", "[fx][multiband]")
{
    auto module = makeModule();

    setParamById (*module, "depth",   100.0f);
    setParamById (*module, "dnwdStr", 100.0f);
    setParamById (*module, "upwdStr", 0.0f);
    setParamById (*module, "thL",     -30.0f);

    const double gainDb = measureSineGainDb (*module, 100.0, 0.3);

    REQUIRE (gainDb < -3.0);
    REQUIRE (module->bandGainDb (0) < -2.0f);
}

TEST_CASE ("Multiband: Aufwaerts-Kompression hebt leise Signale an", "[fx][multiband]")
{
    auto module = makeModule();

    setParamById (*module, "depth",   100.0f);
    setParamById (*module, "upwdStr", 100.0f);
    setParamById (*module, "dnwdStr", 0.0f);
    setParamById (*module, "thL",     -30.0f);

    const double gainDb = measureSineGainDb (*module, 100.0, 0.005);

    REQUIRE (gainDb > 3.0);
    REQUIRE (module->bandGainDb (0) > 2.0f);
}

TEST_CASE ("Multiband: Richtungs-Bypass je Band schaltet die Regelung ab", "[fx][multiband]")
{
    auto module = makeModule();

    setParamById (*module, "depth",   100.0f);
    setParamById (*module, "upwdStr", 100.0f);
    setParamById (*module, "dnwdStr", 100.0f);
    setParamById (*module, "buL",     1.0f);
    setParamById (*module, "bdL",     1.0f);

    (void) measureSineGainDb (*module, 100.0, 0.3);

    REQUIRE (std::abs (module->bandGainDb (0)) < 0.01f);
}

TEST_CASE ("Multiband: Band-Gain wirkt als Pegel im Band", "[fx][multiband]")
{
    auto module = makeModule();

    // Ohne Dynamik nur Band-Gain: -12 dB im Tiefband. Crossover hoch genug,
    // damit das Tiefband das 60-Hz-Signal praktisch allein fuehrt.
    setParamById (*module, "depth",   0.0f);
    setParamById (*module, "upwdStr", 0.0f);
    setParamById (*module, "dnwdStr", 0.0f);
    setParamById (*module, "x1",      200.0f);
    setParamById (*module, "gl",      -12.0f);

    const double gainDb = measureSineGainDb (*module, 60.0, 0.3);

    // Im Tiefband wirkt die Absenkung praktisch vollstaendig.
    REQUIRE (gainDb < -9.0);
    REQUIRE (gainDb > -15.0);
}

TEST_CASE ("Multiband: Input-Gain skaliert das Eingangssignal", "[fx][multiband]")
{
    auto module = makeModule();

    setParamById (*module, "depth",  0.0f);
    setParamById (*module, "inGain", -6.0f);

    const double gainDb = measureSineGainDb (*module, 100.0, 0.2);

    REQUIRE_THAT (gainDb, WithinAbs (-6.0, 0.3));
}
