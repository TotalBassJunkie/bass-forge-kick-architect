#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_data_structures/juce_data_structures.h>

#include <utility>

#include "core/FxChainIO.h"
#include "core/KickState.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    FxModuleState makeModule (const juce::String& type,
                              float mix = 1.0f,
                              float outGain = 1.0f,
                              bool enabled = true)
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = enabled;
        state.mix     = mix;
        state.outGain = outGain;
        return state;
    }

    void setAttribute (FxModuleState& state, const juce::String& id, double value)
    {
        if (! state.data.isValid())
            state.data = juce::ValueTree (juce::Identifier ("Module"));

        state.data.setProperty (juce::Identifier (id), juce::var (value), nullptr);
    }

    /** Die drei Module, mit denen die Serialisierung abgenommen wird. */
    FxChain makeReferenceChain()
    {
        FxChain chain;
        chain.bypassAll = true;

        auto ott = makeModule ("ott", 0.75f, 1.25f, true);
        setAttribute (ott, "depth", 80.0);
        setAttribute (ott, "time", 35.0);
        setAttribute (ott, "x1", 140.0);
        setAttribute (ott, "linear", 1.0);

        auto reverb = makeModule ("reverb", 0.5f, 0.9f, false);
        setAttribute (reverb, "wet", 30.0);
        reverb.data.setProperty ("ir", "C:/irs/plate.wav", nullptr);
        reverb.data.appendChild (juce::ValueTree ("Eq"), nullptr);
        {
            auto eq = reverb.data.getChild (0);
            for (int i = 0; i < 11; ++i)
            {
                juce::ValueTree pt ("Pt");
                pt.setProperty ("y", (double) i - 5.0, nullptr);
                eq.appendChild (pt, nullptr);
            }
        }
        reverb.data.appendChild (juce::ValueTree ("Dc"), nullptr);

        auto dynEq = makeModule ("dynEq");
        setAttribute (dynEq, "b1_freq", 60.0);
        setAttribute (dynEq, "b1_gain", 5.0);
        setAttribute (dynEq, "outGain", -2.0);

        chain.modules.push_back (std::move (ott));
        chain.modules.push_back (std::move (reverb));
        chain.modules.push_back (std::move (dynEq));

        return chain;
    }
} // namespace

TEST_CASE ("FxChainIO: drei Module ueberleben den Roundtrip", "[fx][state]")
{
    const auto chain = makeReferenceChain();
    const auto tree  = fxChainToValueTree (chain);

    REQUIRE (tree.hasType (juce::Identifier ("FxChain")));
    REQUIRE ((bool) tree.getProperty ("bypassAll"));

    const auto restored = fxChainFromValueTree (tree);
    REQUIRE (restored.has_value());
    REQUIRE (restored->bypassAll);
    REQUIRE (restored->modules.size() == 3);

    // Typen in Kettenreihenfolge.
    REQUIRE (restored->modules[0].type == "ott");
    REQUIRE (restored->modules[1].type == "reverb");
    REQUIRE (restored->modules[2].type == "dynEq");

    // MIX / OUT GAIN / Schaltzustand.
    REQUIRE_THAT (restored->modules[0].mix,     WithinAbs (0.75f, 1.0e-6));
    REQUIRE_THAT (restored->modules[0].outGain, WithinAbs (1.25f, 1.0e-6));
    REQUIRE (restored->modules[0].enabled);
    REQUIRE_THAT (restored->modules[1].mix,     WithinAbs (0.5f, 1.0e-6));
    REQUIRE_THAT (restored->modules[1].outGain, WithinAbs (0.9f, 1.0e-6));
    REQUIRE (! restored->modules[1].enabled);
    REQUIRE (restored->modules[2].enabled);

    // Attribute in Anzeige-Einheiten.
    const auto& ott = restored->modules[0].data;
    REQUIRE_THAT ((double) ott.getProperty ("depth"), WithinAbs (80.0, 1.0e-9));
    REQUIRE_THAT ((double) ott.getProperty ("time"),  WithinAbs (35.0, 1.0e-9));
    REQUIRE_THAT ((double) ott.getProperty ("x1"),    WithinAbs (140.0, 1.0e-9));
    REQUIRE_THAT ((double) ott.getProperty ("linear"), WithinAbs (1.0, 1.0e-9));

    // Convolver-Struktur: ir-Property plus <Eq> mit 11 Punkten und <Dc>.
    const auto& reverb = restored->modules[1].data;
    REQUIRE (reverb.getProperty ("ir").toString() == "C:/irs/plate.wav");
    REQUIRE (reverb.getChildWithName (juce::Identifier ("Eq")).isValid());
    REQUIRE (reverb.getChildWithName (juce::Identifier ("Eq")).getNumChildren() == 11);
    REQUIRE (reverb.getChildWithName (juce::Identifier ("Dc")).isValid());
    REQUIRE_THAT ((double) reverb.getChildWithName (juce::Identifier ("Eq"))
                      .getChild (0).getProperty ("y"),
                  WithinAbs (-5.0, 1.0e-9));

    // dynEq-Attribute.
    REQUIRE_THAT ((double) restored->modules[2].data.getProperty ("b1_freq"), WithinAbs (60.0, 1.0e-9));
    REQUIRE_THAT ((double) restored->modules[2].data.getProperty ("outGain"), WithinAbs (-2.0, 1.0e-9));
}

TEST_CASE ("FxChainIO: unbekannter Modultyp bleibt erhalten", "[fx][state]")
{
    FxChain chain;
    chain.modules.push_back (makeModule ("ott"));
    chain.modules.push_back (makeModule ("voelligNeu"));
    chain.modules.push_back (makeModule ("final"));

    const auto restored = fxChainFromValueTree (fxChainToValueTree (chain));

    REQUIRE (restored.has_value());
    REQUIRE (restored->modules.size() == 3);
    REQUIRE (restored->modules[1].type == "voelligNeu");
    REQUIRE (restored->modules[2].type == "final");
}

TEST_CASE ("FxChainIO: leere Kette und fehlender Baum", "[fx][state]")
{
    const FxChain empty;
    const auto tree = fxChainToValueTree (empty);

    REQUIRE (tree.isValid());
    REQUIRE (tree.getNumChildren() == 0);
    REQUIRE (! (bool) tree.getProperty ("bypassAll"));

    const auto restored = fxChainFromValueTree (tree);
    REQUIRE (restored.has_value());
    REQUIRE (restored->modules.empty());
    REQUIRE (! restored->bypassAll);

    // Kein <FxChain> im Baum -> nullopt (nicht "leere Kette").
    REQUIRE (! fxChainFromValueTree (juce::ValueTree ("SomethingElse")).has_value());
    REQUIRE (! fxChainFromValueTree (juce::ValueTree()).has_value());
}

TEST_CASE ("FxChainIO: fehlende Attribute bekommen Defaults", "[fx][state]")
{
    juce::ValueTree fx ("FxChain");
    juce::ValueTree module ("Module");
    module.setProperty ("type", "eq", nullptr);
    fx.appendChild (module, nullptr);

    const auto restored = fxChainFromValueTree (fx);
    REQUIRE (restored.has_value());
    REQUIRE (restored->modules.size() == 1);
    REQUIRE (restored->modules[0].type == "eq");
    REQUIRE (restored->modules[0].enabled);
    REQUIRE_THAT (restored->modules[0].mix,     WithinAbs (1.0f, 1.0e-6));
    REQUIRE_THAT (restored->modules[0].outGain, WithinAbs (1.0f, 1.0e-6));
    REQUIRE (! restored->modules[0].data.isValid());
}

TEST_CASE ("FxChainIO: Kick-Zustand traegt die Kette", "[fx][state]")
{
    KickState state;
    state.fxChain() = makeReferenceChain();

    const auto tree = state.toValueTree();
    REQUIRE (tree.hasType (juce::Identifier ("BFKAKick")));

    juce::String error;
    const auto restored = KickState::fromValueTree (tree, error);

    REQUIRE (restored.has_value());
    REQUIRE (error.isEmpty());
    REQUIRE (restored->fxChain().bypassAll);
    REQUIRE (restored->fxChain().modules.size() == 3);
    REQUIRE (restored->fxChain().modules[1].type == "reverb");
    REQUIRE (restored->fxChain().modules[1].data.getProperty ("ir").toString() == "C:/irs/plate.wav");
}
