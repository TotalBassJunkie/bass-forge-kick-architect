#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "engine/AlignEngine.h"
#include "core/KickState.h"

// B-50/B-51: Tests fuer ALIGN (Gap-Analyse am Anker, MOVE CRUNCH).

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    /** Anker-Part: Punch von 0.0 bis 0.20 s. */
    PartTiming makeAnchor()
    {
        PartTiming anchor;
        anchor.startSeconds = 0.0;
        anchor.endSeconds   = 0.20;
        anchor.role         = Role::punch;
        anchor.pinned       = false;
        anchor.isAnchor     = true;
        return anchor;
    }

    /** Tail-Part: Rumble beginnt bei 0.30 s, Luecke 0.10 s. */
    PartTiming makeTail (bool pinned = false)
    {
        PartTiming tail;
        tail.startSeconds = 0.30;
        tail.endSeconds   = 0.50;
        tail.role         = Role::rumble;
        tail.pinned       = pinned;
        tail.isAnchor     = false;
        return tail;
    }

    /** Zustand mit eingeschaltetem ALIGN im gewuenschten Modus. */
    KickState makeState (AlignMode mode)
    {
        KickState state;
        state.align().enabled   = true;
        state.align().tightness = mode;
        return state;
    }
} // namespace

TEST_CASE ("ALIGN: Toleranzen", "[engine][align]")
{
    REQUIRE_THAT (AlignEngine::toleranceFor (AlignMode::tight),  WithinAbs (0.0,   1e-9));
    REQUIRE_THAT (AlignEngine::toleranceFor (AlignMode::normal), WithinAbs (0.010, 1e-9));
    REQUIRE_THAT (AlignEngine::toleranceFor (AlignMode::gentle), WithinAbs (0.025, 1e-9));
}

TEST_CASE ("ALIGN: Anker wird gefunden", "[engine][align]")
{
    // Drei Parts: Punch bei 0.0, der gesetzte Anker ist der Spaetere.
    PartTiming punch;
    punch.startSeconds = 0.0;
    punch.endSeconds   = 0.10;
    punch.role         = Role::punch;

    PartTiming mid;
    mid.startSeconds = 0.20;
    mid.endSeconds   = 0.30;
    mid.role         = Role::rumble;

    PartTiming marked;
    marked.startSeconds = 0.40;
    marked.endSeconds   = 0.50;
    marked.role         = Role::click;
    marked.isAnchor     = true;

    const std::vector<PartTiming> withMark { punch, mid, marked };
    REQUIRE (AlignEngine::findAnchor (withMark) == 2);

    // Ohne gesetzten Anker gewinnt der Punch mit dem fruehesten Start.
    PartTiming marked2 = marked;
    marked2.isAnchor = false;
    const std::vector<PartTiming> withoutMark { mid, punch, marked2 };
    REQUIRE (AlignEngine::findAnchor (withoutMark) == 1);

    // Leere Liste: kein Anker.
    REQUIRE (AlignEngine::findAnchor ({}) == -1);
}

TEST_CASE ("ALIGN: Luecke wird geschlossen", "[engine][align]")
{
    KickState state = makeState (AlignMode::normal);

    const std::vector<PartTiming> parts { makeAnchor(), makeTail() };

    auto r = AlignEngine::runMoveCrunch (state, parts);

    REQUIRE (r.changed);
    REQUIRE_THAT (r.offsetsSeconds[1], WithinAbs (-0.090, 1e-6));
    REQUIRE (r.gapsAfterSeconds[1] <= 0.010 + 1e-9);
}

TEST_CASE ("ALIGN: TIGHT schliesst ganz", "[engine][align]")
{
    KickState state = makeState (AlignMode::tight);

    const std::vector<PartTiming> parts { makeAnchor(), makeTail() };

    auto r = AlignEngine::runMoveCrunch (state, parts);

    REQUIRE_THAT (r.offsetsSeconds[1], WithinAbs (-0.10, 1e-6));
    REQUIRE_THAT (r.gapsAfterSeconds[1], WithinAbs (0.0, 1e-9));
}

TEST_CASE ("ALIGN: pinned bleibt stehen", "[engine][align]")
{
    KickState state = makeState (AlignMode::normal);

    const std::vector<PartTiming> parts { makeAnchor(), makeTail (true) };

    auto r = AlignEngine::runMoveCrunch (state, parts);

    REQUIRE_THAT (r.offsetsSeconds[1], WithinAbs (0.0, 1e-9));
    REQUIRE_THAT (r.gapsAfterSeconds[1], WithinAbs (0.10, 1e-6));
}

TEST_CASE ("ALIGN: ausgeschaltet aendert nichts", "[engine][align]")
{
    KickState state = makeState (AlignMode::normal);
    state.align().enabled = false;

    const std::vector<PartTiming> parts { makeAnchor(), makeTail() };

    auto r = AlignEngine::runMoveCrunch (state, parts);

    REQUIRE_FALSE (r.changed);
    REQUIRE (r.offsetsSeconds.size() == parts.size());

    for (const double offset : r.offsetsSeconds)
        REQUIRE_THAT (offset, WithinAbs (0.0, 1e-9));
}

TEST_CASE ("ALIGN: Report ist gefuellt", "[engine][align]")
{
    KickState state = makeState (AlignMode::normal);

    const std::vector<PartTiming> parts { makeAnchor(), makeTail() };

    auto r = AlignEngine::runMoveCrunch (state, parts);

    REQUIRE (r.report.size() >= 2);
}
