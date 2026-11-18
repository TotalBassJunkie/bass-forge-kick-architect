// B-16 -- Zustands-Migration ueber Schema-Versionen (v1 -> v2).

#include <catch2/catch_test_macros.hpp>

#include <juce_data_structures/juce_data_structures.h>

#include "core/KickState.h"

using namespace bfka;

TEST_CASE ("Migration: aktueller Zustand traegt die Version", "[core][migration]")
{
    KickState s;
    auto tree = s.toValueTree();

    REQUIRE (KickState::schemaVersionOf (tree) == KickState::currentSchemaVersion);
}

TEST_CASE ("Migration: v1 wird auf v2 gehoben", "[core][migration]")
{
    // Ein v1-Baum kennt das Feld `schemaVersion` noch nicht.
    KickState s;
    auto t = s.toValueTree();
    t.removeProperty ("schemaVersion", nullptr);

    REQUIRE (KickState::schemaVersionOf (t) == 0);

    auto m = KickState::migrateTo (t, 2);

    REQUIRE (KickState::schemaVersionOf (m) == 2);

    // Die Stufe 1 -> 2 ergaenzt die neuen Schalter.
    REQUIRE (m.hasProperty (juce::Identifier ("bpmMode")));
    REQUIRE (m.hasProperty (juce::Identifier ("stretchMode")));
    REQUIRE ((bool) m.getProperty ("alignPhase", false));
    REQUIRE ((bool) m.getProperty ("alignStretchCrunch", false));
}

TEST_CASE ("Migration: unbekannte Felder bleiben", "[core][migration]")
{
    KickState s;
    auto t = s.toValueTree();
    t.removeProperty ("schemaVersion", nullptr);
    t.setProperty ("meinFeld", "bleibt", nullptr);

    auto m = KickState::migrateTo (t, 2);

    REQUIRE (m.getProperty ("meinFeld").toString() == "bleibt");
}

TEST_CASE ("Migration: vorhandene Werte werden nicht ueberschrieben", "[core][migration]")
{
    KickState s;
    auto t = s.toValueTree();
    t.removeProperty ("schemaVersion", nullptr);
    t.setProperty ("bpmMode", "native", nullptr);

    auto m = KickState::migrateTo (t, 2);

    REQUIRE (m.getProperty ("bpmMode").toString() == "native");
}

TEST_CASE ("Migration: Rundweg laedt weiter", "[core][migration]")
{
    KickState s;

    juce::String error;
    auto state = KickState::fromValueTree (s.toValueTree(), error);

    REQUIRE (state.has_value());
    REQUIRE (error.isEmpty());
}

TEST_CASE ("Migration: Version 0 ist harmlos", "[core][migration]")
{
    // Ohne Zielversion darf nichts passieren - und nichts werfen.
    REQUIRE_NOTHROW (KickState::migrateTo (juce::ValueTree ("BFKAKick"), 0));
}
