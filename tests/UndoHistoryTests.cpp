// B-15 -- Tests fuer Undo/Redo ueber vollstaendige Kick-Zustaende.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/UndoHistory.h"
#include "core/KickState.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Undo: drei Schritte, Rueckwaerts in Reihenfolge", "[core][undo]")
{
    KickState s;
    UndoHistory h (50);

    s.setBpm (120.0);
    h.beginEdit ("A", s);
    s.setBpm (130.0);
    h.endEdit();

    h.beginEdit ("B", s);
    s.setBpm (140.0);
    h.endEdit();

    h.beginEdit ("C", s);
    s.setBpm (150.0);
    h.endEdit();

    REQUIRE (h.undoDepth() == 3);
    REQUIRE (h.undoName() == "C");

    REQUIRE (h.undo (s));
    REQUIRE_THAT (s.getBpm(), WithinAbs (140.0, 1e-9));
    REQUIRE (h.undo (s));
    REQUIRE_THAT (s.getBpm(), WithinAbs (130.0, 1e-9));
    REQUIRE (h.undo (s));
    REQUIRE_THAT (s.getBpm(), WithinAbs (120.0, 1e-9));
    REQUIRE_FALSE (h.undo (s));
}

TEST_CASE ("Redo stellt wieder her", "[core][undo]")
{
    KickState s;
    UndoHistory h (50);

    s.setBpm (120.0);
    h.beginEdit ("A", s);
    s.setBpm (130.0);
    h.endEdit();

    h.beginEdit ("B", s);
    s.setBpm (140.0);
    h.endEdit();

    REQUIRE (h.undo (s));
    REQUIRE (h.undo (s));
    REQUIRE_THAT (s.getBpm(), WithinAbs (120.0, 1e-9));

    REQUIRE (h.canRedo());
    REQUIRE (h.redo (s));
    REQUIRE_THAT (s.getBpm(), WithinAbs (130.0, 1e-9));
    REQUIRE (h.redo (s));
    REQUIRE_THAT (s.getBpm(), WithinAbs (140.0, 1e-9));
    REQUIRE_FALSE (h.canRedo());
}

TEST_CASE ("Neuer Edit loescht Redo", "[core][undo]")
{
    KickState s;
    UndoHistory h (50);

    s.setBpm (120.0);
    h.beginEdit ("A", s);
    s.setBpm (130.0);
    h.endEdit();

    REQUIRE (h.undo (s));
    REQUIRE (h.canRedo());

    h.beginEdit ("B", s);
    s.setBpm (140.0);
    h.endEdit();

    REQUIRE_FALSE (h.canRedo());
}

TEST_CASE ("Ringpuffer auf maxSteps", "[core][undo]")
{
    KickState s;
    UndoHistory h (5);

    for (int i = 0; i < 8; ++i)
    {
        h.beginEdit (juce::String (i), s);
        s.setBpm (120.0 + (double) i);
        h.endEdit();
    }

    REQUIRE (h.undoDepth() == 5);

    for (int i = 0; i < 5; ++i)
        REQUIRE (h.undo (s));

    REQUIRE_FALSE (h.undo (s));
}

TEST_CASE ("endEdit ohne beginEdit ist harmlos", "[core][undo]")
{
    KickState s;
    UndoHistory h (50);

    REQUIRE_NOTHROW (h.endEdit());
    REQUIRE (h.undoDepth() == 0);
}
