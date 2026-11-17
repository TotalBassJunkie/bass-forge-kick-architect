#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>

#include <chrono>
#include <cmath>
#include <utility>

#include "core/KickState.h"
#include "plugin/PluginProcessor.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Abnahme der Demo-Kette (Ticket B-145): Struktur, Laenge der Latenz,
// Nulltest bei Ketten-Bypass, Hoerpruefung und CPU-Sanity.
//
// Die Hoerpruefung braucht die Sample-Library. Fehlt sie (z. B. auf einem
// anderen Rechner), werden die betroffenen Faelle uebersprungen und mit WARN
// gemeldet - die Strukturtests laufen immer.
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

    /** Rendert `numBlocks` Bloecke und haengt sie aneinander. */
    juce::AudioBuffer<float> renderBlocks (PluginProcessor& proc, int numBlocks)
    {
        juce::AudioBuffer<float> result (2, numBlocks * kBlockSize);
        result.clear();

        juce::AudioBuffer<float> block (2, kBlockSize);
        juce::MidiBuffer midi;

        for (int b = 0; b < numBlocks; ++b)
        {
            block.clear();
            midi.clear();
            proc.processBlock (block, midi);

            for (int ch = 0; ch < 2; ++ch)
                result.copyFrom (ch, b * kBlockSize, block, ch, 0, kBlockSize);
        }

        return result;
    }

    float peakOf (const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                peak = juce::jmax (peak, std::abs (data[i]));
        }

        return peak;
    }

    double rmsOf (const juce::AudioBuffer<float>& buffer)
    {
        double sum = 0.0;
        int    count = 0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                sum += (double) data[i] * (double) data[i];
                ++count;
            }
        }

        return count > 0 ? std::sqrt (sum / (double) count) : 0.0;
    }

    float maxDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        const auto channels = juce::jmin (a.getNumChannels(), b.getNumChannels());
        const auto samples  = juce::jmin (a.getNumSamples(), b.getNumSamples());

        float maximum = 0.0f;

        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < samples; ++i)
                maximum = juce::jmax (maximum,
                                      std::abs (a.getSample (ch, i) - b.getSample (ch, i)));

        return maximum;
    }
} // namespace

TEST_CASE ("Demo-Kette baut Multiband und Dynamic EQ", "[fx][demo]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);

    proc.loadFxDemoChain();

    const auto chain = proc.getFxChain();
    REQUIRE (chain.modules.size() == 2);
    REQUIRE (chain.modules[0].type == "ott");
    REQUIRE (chain.modules[1].type == "dynEq");

    // Ohne linearphasigen Split und ohne Convolver keine Latenz.
    REQUIRE (proc.getLatencySamples() == 0);
    REQUIRE (proc.getEngineModuleCount() == 2);

    // Die Demo-Attribute stehen im Zustand und kommen im Modul an.
    REQUIRE_THAT ((double) chain.modules[0].data.getProperty (juce::Identifier ("depth")),
                  WithinAbs (80.0, 1.0e-9));

    const auto* multiband = proc.getModule (0);
    REQUIRE (multiband != nullptr);
    REQUIRE_THAT (multiband->getParam (0), WithinAbs (80.0f, 1.0e-4f));   // depth

    const auto* dynamicEq = proc.getModule (1);
    REQUIRE (dynamicEq != nullptr);
    REQUIRE (dynamicEq->params()[0].id == "outGain");
    REQUIRE_THAT (dynamicEq->getParam (0), WithinAbs (0.0f, 1.0e-4f));    // outGain
}

TEST_CASE ("Demo-Kette uebernimmt eine vorhandene IR", "[fx][demo]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);

    // Eine Kette mit Convolver und gesetzter IR vortaeuschen.
    FxChain start;
    FxModuleState reverb;
    reverb.type = "reverb";
    reverb.data = juce::ValueTree (juce::Identifier ("Module"));
    reverb.data.setProperty (juce::Identifier ("ir"), "C:/irs/kammer.wav", nullptr);
    start.modules.push_back (std::move (reverb));
    proc.setFxChain (start);

    proc.loadFxDemoChain();

    const auto chain = proc.getFxChain();
    REQUIRE (chain.modules.size() == 3);
    REQUIRE (chain.modules[1].type == "reverb");
    REQUIRE (chain.modules[1].data.getProperty (juce::Identifier ("ir")).toString() == "C:/irs/kammer.wav");

    // Convolver mit fester Latenz? Nein: zeroLat bleibt Werkswert (an) -> 0.
    REQUIRE (proc.getLatencySamples() == 0);
}

TEST_CASE ("Demo-Kette: Ketten-Bypass ist ein Nulltest", "[fx][demo]")
{
    JuceGuiScope gui;

    PluginProcessor withoutChain;
    withoutChain.prepareToPlay (kSampleRate, kBlockSize);

    if (! withoutChain.startDemoKick())
    {
        WARN ("Sample-Library nicht vorhanden - Hoerpruefung uebersprungen.");
        return;
    }

    withoutChain.triggerKick();
    const auto reference = renderBlocks (withoutChain, 40);

    // Dieselbe Demo, aber mit komplett bypasster Kette.
    PluginProcessor withBypassedChain;
    withBypassedChain.prepareToPlay (kSampleRate, kBlockSize);

    if (! withBypassedChain.startDemoKick())
        return;

    withBypassedChain.loadFxDemoChain();

    FxChain bypassed = withBypassedChain.getFxChain();
    bypassed.bypassAll = true;
    withBypassedChain.setFxChain (bypassed);

    withBypassedChain.triggerKick();
    const auto routed = renderBlocks (withBypassedChain, 40);

    REQUIRE (peakOf (reference) > 0.001f);      // es klingt wirklich etwas
    REQUIRE (maxDifference (reference, routed) == 0.0f);
}

TEST_CASE ("Demo-Kette: mit Kette klingt es anders", "[fx][demo]")
{
    JuceGuiScope gui;

    PluginProcessor withoutChain;
    withoutChain.prepareToPlay (kSampleRate, kBlockSize);

    if (! withoutChain.startDemoKick())
    {
        WARN ("Sample-Library nicht vorhanden - Hoerpruefung uebersprungen.");
        return;
    }

    withoutChain.triggerKick();
    const auto reference = renderBlocks (withoutChain, 40);

    PluginProcessor withChain;
    withChain.prepareToPlay (kSampleRate, kBlockSize);

    if (! withChain.startDemoKick())
        return;

    withChain.loadFxDemoChain();
    withChain.triggerKick();
    const auto processed = renderBlocks (withChain, 40);

    REQUIRE (peakOf (reference) > 0.001f);
    REQUIRE (peakOf (processed) > 0.001f);

    const auto referenceRms = rmsOf (reference);
    const auto processedRms = rmsOf (processed);

    INFO ("RMS ohne Kette " << referenceRms << ", mit Kette " << processedRms);
    REQUIRE (referenceRms > 0.0);
    REQUIRE (std::abs (processedRms - referenceRms) / referenceRms > 0.01);
}

TEST_CASE ("Demo-Kette: CPU-Sanity (kein Ausreisser)", "[fx][demo][perf]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);
    proc.loadFxDemoChain();

    proc.triggerKick();

    constexpr int numBlocks = 200;                     // 0,53 s Audio
    const auto start = std::chrono::steady_clock::now();

    const auto rendered = renderBlocks (proc, numBlocks);

    const auto elapsedMs = std::chrono::duration<double, std::milli> (
                               std::chrono::steady_clock::now() - start).count();

    const auto audioMs  = 1000.0 * (double) (numBlocks * kBlockSize) / kSampleRate;
    const auto cpuShare = 100.0 * elapsedMs / audioMs;

    WARN ("Demo-Kette: " << cpuShare << " % eines Kerns (Debug-Build, "
          << elapsedMs << " ms fuer " << audioMs << " ms Audio)");

    // Sehr grosszuegige Obergrenze: faengt nur pathologische Ausreisser
    // (Endlosschleife, Allokation im Audio-Thread), nicht normale Streuung.
    REQUIRE (elapsedMs < 10000.0);
    REQUIRE (peakOf (rendered) <= 32.0f);              // kein Numerik-Ausreisser
}
