#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include <cmath>
#include <memory>
#include <vector>

#include "fx/modules/ConvolverModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr double kPi         = 3.14159265358979323846;

    std::unique_ptr<ConvolverModule> makeModule()
    {
        auto module = std::make_unique<ConvolverModule>();
        module->prepare (kSampleRate, kBlockSize);
        return module;
    }

    void setParamById (ConvolverModule& module, const juce::String& id, float value)
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

    /** Stereo-IR mit einem Dirac bei `offset` (Laenge `length`). */
    juce::AudioBuffer<float> makeDiracIr (int length, int offset = 0)
    {
        juce::AudioBuffer<float> ir (2, length);
        ir.clear();

        if (offset >= 0 && offset < length)
        {
            ir.setSample (0, offset, 1.0f);
            ir.setSample (1, offset, 1.0f);
        }

        return ir;
    }

    juce::AudioBuffer<float> makeConstantIr (int length, float value)
    {
        juce::AudioBuffer<float> ir (2, length);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                ir.setSample (ch, i, value);

        return ir;
    }

    /** Neutralisiert alles, was die Test-IR sonst veraendern wuerde. */
    void setNeutralParams (ConvolverModule& module)
    {
        setParamById (module, "wet",       100.0f);
        setParamById (module, "stereo",    100.0f);
        setParamById (module, "tone",      50.0f);
        setParamById (module, "speed",     0.0f);
        setParamById (module, "predelay",  0.0f);
        setParamById (module, "decay",     1.0f);
        setParamById (module, "modDepth",  0.0f);
        setParamById (module, "mod90",     0.0f);
        setParamById (module, "reverse",   0.0f);
        setParamById (module, "monoIn",    0.0f);
        setParamById (module, "normalize", 0.0f);
        setParamById (module, "irGain",    0.0f);
        setParamById (module, "envStart",  0.0f);
        setParamById (module, "envAttack", 0.0f);
        setParamById (module, "envHold",   0.0f);
        setParamById (module, "envRelease", 0.0f);
    }

    int findPeakIndex (const std::vector<float>& signal)
    {
        int index = -1;
        float best = 0.0f;

        for (size_t i = 0; i < signal.size(); ++i)
        {
            if (std::abs (signal[i]) > best)
            {
                best = std::abs (signal[i]);
                index = (int) i;
            }
        }

        return index;
    }
} // namespace

// =========================================================================
// Kopfdaten
// =========================================================================

TEST_CASE ("Convolver: Parameterliste beginnt mit Referenz-Attributen", "[fx][convolver]")
{
    ConvolverModule module;

    CHECK (module.type() == FxType::convolver);
    CHECK (module.displayName() == "Convolver");
    CHECK (module.latencySamples() == 0);
    CHECK_FALSE (module.hasImpulseResponse());

    const juce::StringArray expectedIds
    {
        "wet", "stereo", "tone", "speed",
        "predelay", "decay", "modDepth", "modRate", "mod90",
        "reverse", "monoIn", "normalize", "irGain", "trueStereo", "zeroLat",
        "envStart", "envAttack", "envHold", "envRelease"
    };

    const auto& params = module.params();
    REQUIRE (params.size() == (size_t) expectedIds.size());

    for (int i = 0; i < expectedIds.size(); ++i)
    {
        INFO ("Index " << i);
        REQUIRE (params[(size_t) i].id == expectedIds[i]);
        REQUIRE (params[(size_t) i].minValue < params[(size_t) i].maxValue);
    }

    // Referenz-Bridge: p0..p3 bleiben wet/stereo/tone/speed.
    REQUIRE (params[0].id == "wet");
    REQUIRE (params[1].id == "stereo");
    REQUIRE (params[2].id == "tone");
    REQUIRE (params[3].id == "speed");
}

TEST_CASE ("Convolver: Parameter werden geklemmt", "[fx][convolver]")
{
    ConvolverModule module;

    module.setParam (0, 500.0f);
    REQUIRE_THAT (module.getParam (0), WithinAbs (100.0f, 1.0e-6f));

    module.setParam (3, -50.0f);
    REQUIRE_THAT (module.getParam (3), WithinAbs (-12.0f, 1.0e-6f));

    REQUIRE_THAT (module.getParam (-1), WithinAbs (0.0f, 1.0e-6f));
}

// =========================================================================
// Faltung
// =========================================================================

TEST_CASE ("Convolver: Dirac-IR reicht das Signal durch", "[fx][convolver]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    module->loadImpulseFromBuffer (makeDiracIr (512), kSampleRate);

    REQUIRE (module->hasImpulseResponse());
    REQUIRE (module->currentIrLength() == 512);
    REQUIRE_FALSE (module->isTrueStereo());

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    const double increment = 2.0 * kPi * 440.0 / kSampleRate;
    double phase = 0.0;
    double maxError = 0.0;

    for (int block = 0; block < 40; ++block)
    {
        for (int s = 0; s < kBlockSize; ++s)
        {
            const float value = (float) (0.25 * std::sin (phase));
            phase += increment;

            buffer.setSample (0, s, value);
            buffer.setSample (1, s, value);
        }

        juce::AudioBuffer<float> reference = buffer;

        module->process (buffer);

        if (block > 2)
        {
            for (int s = 0; s < kBlockSize; ++s)
            {
                maxError = std::max (maxError, std::abs ((double) buffer.getSample (0, s)
                                                         - (double) reference.getSample (0, s)));
            }
        }
    }

    REQUIRE (maxError < 2.0e-3);
}

TEST_CASE ("Convolver: Predelay verzoegert den nassen Anteil", "[fx][convolver]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "predelay", 100.0f);
    module->loadImpulseFromBuffer (makeDiracIr (512), kSampleRate);

    std::vector<float> output;
    juce::AudioBuffer<float> buffer (2, kBlockSize);

    for (int block = 0; block < 60; ++block)
    {
        buffer.clear();

        if (block == 0)
        {
            buffer.setSample (0, 0, 1.0f);
            buffer.setSample (1, 0, 1.0f);
        }

        module->process (buffer);

        for (int s = 0; s < kBlockSize; ++s)
            output.push_back (buffer.getSample (0, s));
    }

    const int peak = findPeakIndex (output);

    REQUIRE (peak >= 4700);
    REQUIRE (peak <= 4900);
}

TEST_CASE ("Convolver: Stretch veraendert die IR-Laenge", "[fx][convolver]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    module->loadImpulseFromBuffer (makeConstantIr (4800, 0.01f), kSampleRate);

    REQUIRE (module->currentIrLength() == 4800);

    setParamById (*module, "speed", 12.0f);

    // Eine Oktave schneller -> halbe IR-Laenge.
    REQUIRE (module->currentIrLength() >= 2300);
    REQUIRE (module->currentIrLength() <= 2500);
}

TEST_CASE ("Convolver: Decay skaliert die IR-Huellkurve", "[fx][convolver]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    module->loadImpulseFromBuffer (makeConstantIr (48000, 0.1f), kSampleRate);

    setParamById (*module, "decay", 0.5f);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    std::vector<float> output;

    for (int block = 0; block < 384; ++block)  // 1,024 s
    {
        buffer.clear();

        if (block == 0)
        {
            buffer.setSample (0, 0, 1.0f);
            buffer.setSample (1, 0, 1.0f);
        }

        module->process (buffer);

        for (int s = 0; s < kBlockSize; ++s)
            output.push_back (buffer.getSample (0, s));
    }

    const float head = std::abs (output[100]);
    const float tail = std::abs (output[40000]);

    REQUIRE (head > 0.01f);
    REQUIRE (tail < head * 0.8f);
}

TEST_CASE ("Convolver: Normalize skaliert die IR auf Einheitsenergie", "[fx][convolver]")
{
    // IR [1, 1] auf beiden Kanaelen: Energie 2 -> Normierung 1/sqrt(2).
    juce::AudioBuffer<float> ir (2, 512);
    ir.clear();
    ir.setSample (0, 0, 1.0f);
    ir.setSample (0, 1, 1.0f);
    ir.setSample (1, 0, 1.0f);
    ir.setSample (1, 1, 1.0f);

    auto renderPeak = [&ir] (bool normalize)
    {
        auto module = makeModule();
        setNeutralParams (*module);
        setParamById (*module, "normalize", normalize ? 1.0f : 0.0f);
        module->loadImpulseFromBuffer (ir, kSampleRate);

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        float peak = 0.0f;

        for (int block = 0; block < 8; ++block)
        {
            buffer.clear();

            if (block == 0)
            {
                buffer.setSample (0, 0, 1.0f);
                buffer.setSample (1, 0, 1.0f);
            }

            module->process (buffer);

            for (int s = 0; s < kBlockSize; ++s)
                peak = std::max (peak, std::abs (buffer.getSample (0, s)));
        }

        return peak;
    };

    const float withoutNormalization = renderPeak (false);
    const float withNormalization    = renderPeak (true);

    REQUIRE_THAT (withoutNormalization, WithinAbs (1.0f, 1.0e-2f));
    REQUIRE_THAT (withNormalization, WithinAbs (0.5f, 1.0e-2f));
}

// =========================================================================
// Stereoverarbeitung
// =========================================================================

TEST_CASE ("Convolver: Width 0 mischt auf Mono", "[fx][convolver]")
{
    auto module = makeModule();

    setNeutralParams (*module);
    setParamById (*module, "stereo", 0.0f);
    module->loadImpulseFromBuffer (makeDiracIr (512), kSampleRate);

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    for (int block = 0; block < 20; ++block)
    {
        for (int s = 0; s < kBlockSize; ++s)
        {
            buffer.setSample (0, s, 1.0f);
            buffer.setSample (1, s, 0.0f);
        }

        module->process (buffer);

        if (block > 40)
        {
            for (int s = 0; s < kBlockSize; s += 16)
            {
                REQUIRE_THAT (buffer.getSample (0, s), WithinAbs (buffer.getSample (1, s), 1.0e-4f));
                REQUIRE_THAT (buffer.getSample (0, s), WithinAbs (0.5f, 1.0e-2f));
            }
        }
    }
}

TEST_CASE ("Convolver: True Stereo verarbeitet die Kreuzantworten", "[fx][convolver]")
{
    // 4-Kanal-IR: nur L->R und R->L enthalten einen Dirac.
    juce::AudioBuffer<float> ir (4, 512);
    ir.clear();
    ir.setSample (1, 0, 1.0f);   // L->R
    ir.setSample (2, 0, 1.0f);   // R->L

    auto module = makeModule();
    setNeutralParams (*module);
    module->loadImpulseFromBuffer (ir, kSampleRate);

    REQUIRE (module->isTrueStereo());

    auto renderImpulse = [&module] ()
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);

        module->process (buffer);

        std::pair<float, float> result { buffer.getSample (0, 0), buffer.getSample (1, 0) };

        // Rest des Blocks in den Zustand spielen, damit der naechste Lauf
        // identisch startet.
        juce::AudioBuffer<float> tail (2, kBlockSize);
        tail.clear();
        module->process (tail);

        return result;
    };

    const auto trueStereoResult = renderImpulse();

    // L-Eingang erzeugt nur den R-Ausgang (L->R), R-Eingang den L-Ausgang.
    REQUIRE_THAT (trueStereoResult.first, WithinAbs (1.0f, 2.0e-2f));   // R->L
    REQUIRE_THAT (trueStereoResult.second, WithinAbs (1.0f, 2.0e-2f));  // L->R

    // True Stereo abschalten -> es bleiben nur die Kanaele 0/3 (hier leer).
    module->reset();
    setParamById (*module, "trueStereo", 0.0f);
    module->reset();

    const auto stereoResult = renderImpulse();

    REQUIRE (std::abs (stereoResult.first) < 2.0e-2f);
    REQUIRE (std::abs (stereoResult.second) < 2.0e-2f);
}

// =========================================================================
// Zustand
// =========================================================================

TEST_CASE ("Convolver: Zustand haelt IR-Pfad und Kurven", "[fx][convolver]")
{
    ConvolverModule module;
    module.setImpulsePath ("C:\\nicht\\vorhanden.wav");

    REQUIRE (module.impulsePath() == "C:\\nicht\\vorhanden.wav");

    const auto state = module.getState();
    REQUIRE (state.hasProperty (juce::Identifier ("ir")));

    const auto eq = state.getChildWithName (juce::Identifier ("Eq"));
    const auto dc = state.getChildWithName (juce::Identifier ("Dc"));
    REQUIRE (eq.isValid());
    REQUIRE (dc.isValid());
    REQUIRE (eq.getNumChildren() == ConvolverModule::numCurvePoints);
    REQUIRE (dc.getNumChildren() == ConvolverModule::numCurvePoints);

    ConvolverModule restored;
    restored.setState (state);

    REQUIRE (restored.impulsePath() == "C:\\nicht\\vorhanden.wav");
}
