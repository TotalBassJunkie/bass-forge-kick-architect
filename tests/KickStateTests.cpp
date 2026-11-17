#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/KickState.h"
#include "core/ParameterRegistry.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Frischer Zustand hat Defaults", "[state]")
{
    KickState state;
    const auto& registry = ParameterRegistry::get();

    REQUIRE (state.values().size() == (size_t) registry.size());
    REQUIRE (state.getStyleId() == "free_build");
    REQUIRE (state.isDirty() == false);
    REQUIRE (state.getBpm() == 150.0);

    for (int i = 0; i < KickState::kNumSlots; ++i)
        REQUIRE (! state.getSample (i).has_value());
}

TEST_CASE ("Parameter lesen und schreiben", "[state]")
{
    KickState state;

    state.setParam ("master_vol", 0.75f);
    REQUIRE_THAT (state.getParam ("master_vol"), WithinAbs (0.75f, 1.0e-6));
    REQUIRE (state.isDirty());

    state.setParam ("master_vol", 5.0f);   // wird geklemmt
    REQUIRE_THAT (state.getParam ("master_vol"), WithinAbs (1.0f, 1.0e-6));
}

TEST_CASE ("Slots, Rollen und Locks", "[state]")
{
    KickState state;

    SampleRef ref;
    ref.relPath = "Main - Punches/Techno/Punch Techno 1 150.wav";
    ref.fileName = "Punch Techno 1 150.wav";
    ref.roleId = "punch";

    state.setSample (0, ref);
    state.setRole (0, Role::punch);
    state.setLocked (0, true);

    REQUIRE (state.getSample (0).has_value());
    REQUIRE (state.getSample (0)->fileName == "Punch Techno 1 150.wav");
    REQUIRE (state.getRole (0) == Role::punch);
    REQUIRE (state.isLocked (0));

    state.clearSample (0);
    REQUIRE (! state.getSample (0).has_value());
}

TEST_CASE ("Zustand ueberlebt ValueTree-Roundtrip", "[state]")
{
    KickState state;
    state.setParam ("master_vol", 0.42f);
    state.setStyleId ("rawstyle");
    state.setPresetName ("Test");
    state.setBpm (160.0);

    SampleRef ref;
    ref.relPath = "Main - Rumbles/Schranz/Rumble Schranz 1 150.wav";
    ref.fileName = "Rumble Schranz 1 150.wav";
    ref.roleId = "rumble";
    state.setSample (3, ref);
    state.setRole (3, Role::rumble);
    state.setKeyTracked (3, true);

    const auto tree = state.toValueTree();
    juce::String error;
    const auto restored = KickState::fromValueTree (tree, error);

    REQUIRE (restored.has_value());
    REQUIRE (error.isEmpty());
    REQUIRE_THAT (restored->getParam ("master_vol"), WithinAbs (0.42f, 1.0e-6));
    REQUIRE (restored->getStyleId() == "rawstyle");
    REQUIRE (restored->getPresetName() == "Test");
    REQUIRE (restored->getBpm() == 160.0);
    REQUIRE (restored->getSample (3).has_value());
    REQUIRE (restored->getRole (3) == Role::rumble);
    REQUIRE (restored->isKeyTracked (3));
}

TEST_CASE ("Rollen-Namen sind eindeutig", "[state]")
{
    REQUIRE (roleFromString ("punch")  == Role::punch);
    REQUIRE (roleFromString ("rumble") == Role::rumble);
    REQUIRE (roleFromString ("PUNCH")  == Role::punch);
    REQUIRE (roleFromString ("quatsch") == Role::none);
    REQUIRE (roleToString (Role::schranzEnhancer) == "TAIL+");
}
