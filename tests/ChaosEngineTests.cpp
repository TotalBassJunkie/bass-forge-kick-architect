// B-75: Tests der CHAOS-Engine (Determinismus, Grenzen, Flags).
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "library/ChaosEngine.h"
#include "library/RandomizerEngine.h"
#include "core/KickState.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    // Der CHAOS-Wuerfel braucht die Engine nur fuer den Vertrag; die
    // Sample-Aufloesung wird hier nie benutzt.
    struct NullResolver : bfka::PoolResolver
    {
        std::vector<juce::String> samplePathsForPool (const juce::String&) const override
        {
            return {};
        }
    };

    juce::String pitchId (int slot)
    {
        return "l" + juce::String (slot) + "_pitch";
    }

    juce::String stretchId (int slot)
    {
        return "l" + juce::String (slot) + "_stretch";
    }

    /** Setzt alle Slot-Pitches auf 0 Halbtoene (normalisiert 0.5). */
    void resetPitchesToZero (KickState& state)
    {
        for (int slot = 0; slot < KickState::kNumSlots; ++slot)
            state.setParam (pitchId (slot), 0.5f);
    }
} // namespace

// ---------------------------------------------------------------------------

TEST_CASE ("CHAOS: gleicher Seed liefert gleiches Ergebnis", "[library][chaos]")
{
    NullResolver resolver;
    bfka::StyleModel model;
    RandomizerEngine engine (model, resolver);

    KickState a;
    KickState b;

    ChaosEngine c1 (engine, 12345);
    c1.applyTo (a);

    ChaosEngine c2 (engine, 12345);
    c2.applyTo (b);

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        REQUIRE_THAT (a.getParam (pitchId (slot)),
                      WithinAbs (b.getParam (pitchId (slot)), 1e-6));
}

TEST_CASE ("CHAOS: anderer Seed aendert etwas", "[library][chaos]")
{
    NullResolver resolver;
    bfka::StyleModel model;
    RandomizerEngine engine (model, resolver);

    KickState a;
    KickState b;

    ChaosEngine c1 (engine, 1);
    c1.applyTo (a);

    ChaosEngine c2 (engine, 2);
    c2.applyTo (b);

    bool anyDifference = false;

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        if (std::abs (a.getParam (pitchId (slot))
                      - b.getParam (pitchId (slot))) > 1e-6f)
            anyDifference = true;

    REQUIRE (anyDifference);
}

TEST_CASE ("CHAOS: Pitch bleibt in +-3 Halbtoenen", "[library][chaos]")
{
    NullResolver resolver;
    bfka::StyleModel model;
    RandomizerEngine engine (model, resolver);

    KickState state;
    resetPitchesToZero (state);

    ChaosEngine chaos (engine, 777);
    chaos.applyTo (state);

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        const double engineering = (double) state.getParam (pitchId (slot)) * 48.0 - 24.0;
        REQUIRE (std::abs (engineering) <= 3.0 + 1e-6);
    }
}

TEST_CASE ("CHAOS: Flags schalten Bereiche ab", "[library][chaos]")
{
    NullResolver resolver;
    bfka::StyleModel model;
    RandomizerEngine engine (model, resolver);

    KickState state;
    resetPitchesToZero (state);

    std::vector<float> stretchBefore ((size_t) KickState::kNumSlots, 0.0f);

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        state.setParam (stretchId (slot), 0.5f);
        stretchBefore[(size_t) slot] = state.getParam (stretchId (slot));
    }

    ChaosFlags onlyPitch;
    onlyPitch.pitch     = true;
    onlyPitch.stretch   = false;
    onlyPitch.fxChain   = false;
    onlyPitch.fxModules = false;

    ChaosEngine chaos (engine, 4242);
    chaos.applyTo (state, onlyPitch);

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        REQUIRE_THAT (state.getParam (stretchId (slot)),
                      WithinAbs (stretchBefore[(size_t) slot], 1e-6));
}

TEST_CASE ("CHAOS: Rueckgabewert zaehlt Aenderungen", "[library][chaos]")
{
    NullResolver resolver;
    bfka::StyleModel model;
    RandomizerEngine engine (model, resolver);

    KickState a;

    ChaosEngine c (engine, 99);
    REQUIRE (c.applyTo (a) > 0);
}
