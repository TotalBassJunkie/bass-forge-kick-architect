#include <catch2/catch_test_macros.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <memory>

#include "core/KickState.h"
#include "fx/FxChainProcessor.h"
#include "fx/modules/ConvolverModule.h"
#include "fx/modules/MultibandModule.h"

using namespace bfka;

// CPU-Profil je Modul (Ticket B-149).
//
// Gemessen wird die Wanduhrzeit fuer die Verarbeitung eines Blocks bei
// 48 kHz/128 Samples, bezogen auf die Dauer des Blocks (= Anteil eines Kerns).
// Die Budgets unten gelten fuer den **Debug**-Build; im Release-Build liegt
// jeder Wert deutlich niedriger (Faktor 3 bis 8). Die Messung ist bewusst
// grob: sie soll Ausreisser und Regressionen finden, keine DSP-Benchmarks
// ersetzen.
namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr int    kWarmupBlocks  = 5;
    constexpr int    kMeasureBlocks = 400;

    FxModuleState makeModule (const juce::String& type)
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = true;
        state.mix     = 1.0f;
        state.outGain = 1.0f;
        return state;
    }

    void fillKick (juce::AudioBuffer<float>& buffer, int blockIndex)
    {
        constexpr double kPi = 3.14159265358979323846;
        const double phaseStep = 2.0 * kPi * 55.0 / kSampleRate;

        for (int s = 0; s < buffer.getNumSamples(); ++s)
        {
            const double t = (double) (blockIndex * kBlockSize + s) / kSampleRate;
            const double envelope = std::exp (-t * 8.0);
            const double value = 0.6 * envelope * (std::sin (phaseStep * (double) s)
                                                  + 0.4 * std::sin (3.0 * phaseStep * (double) s));

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, s, (float) value);
        }
    }

    /** Prozent eines Kerns fuer einen Block dieser Kette (Median aus 3 Laeufen). */
    double measureCpuPercent (FxChainProcessor& processor,
                              std::unique_ptr<juce::AudioBuffer<float>>& sidechain,
                              bool useSidechain)
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        std::array<double, 3> results {};

        for (auto& result : results)
        {
            for (int block = 0; block < kWarmupBlocks; ++block)
            {
                fillKick (buffer, block);
                processor.process (buffer, useSidechain ? sidechain.get() : nullptr);
            }

            const auto start = std::chrono::steady_clock::now();

            for (int block = 0; block < kMeasureBlocks; ++block)
            {
                fillKick (buffer, block);
                processor.process (buffer, useSidechain ? sidechain.get() : nullptr);
            }

            const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start);
            const double audioSeconds = (double) (kMeasureBlocks * kBlockSize) / kSampleRate;

            result = elapsed.count() / audioSeconds * 100.0;
        }

        std::sort (results.begin(), results.end());
        return results[1];
    }
} // namespace

TEST_CASE ("CPU: Profil je Modul", "[cpu][fx]")
{
    const juce::StringArray types { "filt", "eq", "mseq", "dist", "shaper",
                                    "trans", "reverb", "final", "ott", "dynEq" };

    juce::AudioBuffer<float> sidechainBuffer (2, kBlockSize);
    fillKick (sidechainBuffer, 0);

    juce::String report;
    int painted = 0;

    for (const auto& type : types)
    {
        FxChain chain;
        chain.modules.push_back (makeModule (type));

        FxChainProcessor processor;
        processor.prepare (kSampleRate, kBlockSize);
        processor.setChain (chain);

        std::unique_ptr<juce::AudioBuffer<float>> sidechain;

        if (type == "dynEq")
        {
            sidechain = std::make_unique<juce::AudioBuffer<float>> (sidechainBuffer);
        }

        const auto percent = measureCpuPercent (processor, sidechain, type == "dynEq");

        report << type << " " << juce::String (percent, 1) << " %  ";
        ++painted;

        INFO ("Modul " << type << ": " << juce::String (percent, 1) << " % eines Kerns (Debug)");

        // Debug-Budgets: rund doppelt bis dreifach ueber den gemessenen Werten
        // (siehe docs/specs/fx-chain-engine.md, Abschnitt 9) Ã¢â‚¬â€ eng genug, um
        // Regressionen zu finden.
        const auto budget = [] (const juce::String& type)
        {
            if (type == "final")  return 60.0;
            if (type == "dist")   return 15.0;
            if (type == "ott")    return 15.0;
            if (type == "reverb") return 10.0;
            if (type == "dynEq")  return 10.0;

            return 8.0;
        } (type);

        CHECK (percent < budget);
    }

    // Der Report landet in der Testausgabe (Catch2 gibt auf stdout aus).
    std::cout << "[CPU] " << report.toStdString() << std::endl;

    CHECK (painted == types.size());
}

TEST_CASE ("CPU: teure Konfigurationen bleiben messbar", "[cpu][fx]")
{
    // Convolver als echtes Stereo (4-Kanal-IR) und Multiband linearphasig:
    // beides sind die teuersten Einstellungen der Kette. Hier wird das Modul
    // direkt gemessen (ohne Ketten-Overhead); das Budget ist entsprechend eng.
    const auto measureModule = [] (FxModule& module, double budget)
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        std::array<double, 3> results {};

        for (auto& result : results)
        {
            for (int block = 0; block < kWarmupBlocks; ++block)
            {
                fillKick (buffer, block);
                module.process (buffer);
            }

            const auto start = std::chrono::steady_clock::now();

            for (int block = 0; block < kMeasureBlocks; ++block)
            {
                fillKick (buffer, block);
                module.process (buffer);
            }

            const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start);
            const double audioSeconds = (double) (kMeasureBlocks * kBlockSize) / kSampleRate;

            result = elapsed.count() / audioSeconds * 100.0;
        }

        std::sort (results.begin(), results.end());

        const auto percent = results[1];

        INFO (juce::String (percent, 1) << " % eines Kerns (Debug)");
        CHECK (percent < budget);

        return percent;
    };

    {
        // True-Stereo-IR: 4 Kanaele, 2 s Nachhall.
        const int irLength = (int) (2.0 * kSampleRate);
        juce::AudioBuffer<float> ir (4, irLength);
        ir.clear();

        for (int ch = 0; ch < 4; ++ch)
            for (int s = 0; s < irLength; ++s)
                ir.setSample (ch, s, (float) (std::exp (-(double) s / (kSampleRate * 0.35))
                                              * ((s % 97 == 0) ? 0.25 : 0.004)));

        auto convolver = std::make_unique<ConvolverModule>();
        convolver->prepare (kSampleRate, kBlockSize);
        convolver->loadImpulseFromBuffer (ir, kSampleRate);

        const auto percent = measureModule (*convolver, 160.0);
        std::cout << "[CPU] reverb (true stereo, 2 s IR) " << percent << " %" << std::endl;
    }

    {
        auto multiband = std::make_unique<MultibandModule>();
        multiband->prepare (kSampleRate, kBlockSize);

        // Linearphasiger Crossover (513 Taps bei 48 kHz, 256 Samples Latenz).
        const int linearIndex = [] (MultibandModule& module)
        {
            const auto& params = module.params();

            for (size_t i = 0; i < params.size(); ++i)
                if (params[i].id == "linear")
                    return (int) i;

            return -1;
        } (*multiband);

        REQUIRE (linearIndex >= 0);
        multiband->setParam (linearIndex, 1.0f);

        const auto percent = measureModule (*multiband, 160.0);
        std::cout << "[CPU] ott (linear) " << percent << " %" << std::endl;
    }
}
