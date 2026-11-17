#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>

#include "core/KickState.h"
#include "fx/FxChainProcessor.h"
#include "fx/modules/ConvolverModule.h"
#include "plugin/PluginProcessor.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Tail-Laenge des Plugins aus Kick-Laenge und FX-Ketten-Nachhall (B-150).
namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };
} // namespace

TEST_CASE ("Tail: leere Kette ist null", "[fx][tail]")
{
    FxChainProcessor fx;
    fx.prepare (kSampleRate, kBlockSize);

    FxChain chain;
    fx.setChain (chain);

    REQUIRE_THAT (fx.tailSeconds(), WithinAbs (0.0, 1e-9));
}

TEST_CASE ("Tail: Convolver mit 2-Sekunden-IR", "[fx][tail]")
{
    FxChain chain;
    FxModuleState state;
    state.type    = "reverb";
    state.enabled = true;
    state.mix     = 1.0f;
    state.outGain = 1.0f;
    chain.modules.push_back (state);

    FxChainProcessor fx;
    fx.prepare (kSampleRate, kBlockSize);
    fx.setChain (chain);

    auto* convolver = dynamic_cast<ConvolverModule*> (fx.moduleAt (0));
    REQUIRE (convolver != nullptr);

    juce::AudioBuffer<float> ir (2, 96000);
    ir.clear();
    ir.setSample (0, 0, 1.0f);
    ir.setSample (1, 0, 1.0f);
    convolver->loadImpulseFromBuffer (ir, kSampleRate);

    REQUIRE_THAT (fx.tailSeconds(), Catch::Matchers::WithinAbs (2.0, 0.2));
}

TEST_CASE ("Tail: PluginProcessor klemmt auf [0.05, 60]", "[fx][tail]")
{
    JuceGuiScope guiScope;

    PluginProcessor proc;

    REQUIRE (proc.getTailLengthSeconds() >= 0.05);
    REQUIRE (proc.getTailLengthSeconds() <= 60.0);
}

TEST_CASE ("Tail: Convolver ohne IR ist null", "[fx][tail]")
{
    ConvolverModule convolver;
    convolver.prepare (kSampleRate, kBlockSize);

    REQUIRE_THAT (convolver.tailSeconds(), WithinAbs (0.0, 1e-9));
}
