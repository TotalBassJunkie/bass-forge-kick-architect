#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>

#include "fx/modules/ConvolverModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Sampleweise Rampen (B-148): der Convolver darf WET/WIDTH nicht pro Block
// springen lassen.
namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

    juce::AudioBuffer<float> makeDiracIr (int length)
    {
        juce::AudioBuffer<float> ir (2, length);
        ir.clear();
        ir.setSample (0, 0, 1.0f);
        ir.setSample (1, 0, 1.0f);
        return ir;
    }

    int paramIndex (const juce::String& id)
    {
        ConvolverModule module;

        const auto& params = module.params();

        for (size_t i = 0; i < params.size(); ++i)
            if (params[i].id == id)
                return (int) i;

        return -1;
    }

    void setParamById (ConvolverModule& module, const juce::String& id, float value)
    {
        const int index = paramIndex (id);

        if (index >= 0)
            module.setParam (index, value);
    }

    void fill (juce::AudioBuffer<float>& buffer, float value)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                buffer.setSample (ch, s, value);
    }
} // namespace

TEST_CASE ("Convolver: WET rampt sampleweise auf 0", "[fx][smoothing]")
{
    ConvolverModule module;
    module.prepare (kSampleRate, kBlockSize);
    module.loadImpulseFromBuffer (makeDiracIr (512), kSampleRate);

    setParamById (module, "wet", 100.0f);
    setParamById (module, "stereo", 100.0f);
    setParamById (module, "tone", 50.0f);

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    // Einschwingen: konstantes Signal, volle Blende.
    float settled = 0.0f;

    for (int block = 0; block < 60; ++block)
    {
        fill (buffer, 0.5f);
        module.process (buffer);
        settled = buffer.getSample (0, kBlockSize - 1);
    }

    REQUIRE (settled > 0.1f);

    // WET auf 0: innerhalb des Blocks muss der Pegel sinken (Rampe), nicht
    // blockweise konstant sein.
    setParamById (module, "wet", 0.0f);

    fill (buffer, 0.5f);
    module.process (buffer);

    const auto first = buffer.getSample (0, 0);
    const auto last  = buffer.getSample (0, kBlockSize - 1);

    CHECK (first > last + 1.0e-3f);

    // Und monoton fallen (kein Sprung mitten im Block).
    for (int s = 1; s < kBlockSize; ++s)
        REQUIRE (buffer.getSample (0, s) <= buffer.getSample (0, s - 1) + 1.0e-6f);

    // Nach dem Einschwingen ist WET 0 auch wirklich still.
    for (int block = 0; block < 120; ++block)
    {
        fill (buffer, 0.5f);
        module.process (buffer);
    }

    CHECK_THAT (buffer.getSample (0, 0), WithinAbs (0.0f, 1.0e-5f));
}

TEST_CASE ("Convolver: WIDTH rampt sampleweise", "[fx][smoothing]")
{
    ConvolverModule module;
    module.prepare (kSampleRate, kBlockSize);
    module.loadImpulseFromBuffer (makeDiracIr (512), kSampleRate);

    setParamById (module, "wet", 100.0f);
    setParamById (module, "stereo", 100.0f);

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    // Eingang: links 1, rechts 0 -> Seitenanteil = 0,5.
    const auto processBlock = [&module, &buffer] ()
    {
        for (int s = 0; s < kBlockSize; ++s)
        {
            buffer.setSample (0, s, 1.0f);
            buffer.setSample (1, s, 0.0f);
        }

        module.process (buffer);
    };

    for (int block = 0; block < 60; ++block)
        processBlock();

    const auto sideEnergy = buffer.getSample (0, kBlockSize - 1)
                            - buffer.getSample (1, kBlockSize - 1);

    REQUIRE (sideEnergy > 0.1f);

    // WIDTH von 100 % auf 0 %: der Seitenanteil laeuft innerhalb des Blocks
    // zusammen (Rampe), statt blockweise konstant zu bleiben.
    setParamById (module, "stereo", 0.0f);
    processBlock();

    const auto diffFirst = buffer.getSample (0, 0) - buffer.getSample (1, 0);
    const auto diffLast  = buffer.getSample (0, kBlockSize - 1)
                           - buffer.getSample (1, kBlockSize - 1);

    CHECK (diffFirst > diffLast + 1.0e-3f);
}
