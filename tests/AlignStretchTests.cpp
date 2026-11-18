#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "engine/AlignEngine.h"
#include "core/KickState.h"

// B-52/B-53/B-54/B-55: Tests fuer STRETCH PUNCH / STRETCH CRUNCH, Fades,
// Hand-Platzierung (pinned) und die Methodenwahl aus dem Zustand.

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
} // namespace

TEST_CASE ("ALIGN: Methode aus dem Zustand", "[engine][align]")
{
    KickState s;
    REQUIRE_FALSE (AlignEngine::usesStretch (s));

    s.align().method = AlignMethod::stretchPunch;
    REQUIRE (AlignEngine::usesStretch (s));
}

TEST_CASE ("STRETCH: Luecke wird durch Stauchen geschlossen", "[engine][align]")
{
    KickState s;
    s.align().tightness = AlignMode::tight;

    const std::vector<PartTiming> parts { makeAnchor(), makeTail() };

    auto r = AlignEngine::runStretchCrunch (s, parts);

    REQUIRE (r.changed);
    REQUIRE_THAT (r.stretchFactors[1], WithinAbs (0.5, 1e-6));
    REQUIRE_THAT (r.offsetsSeconds[1], WithinAbs (0.0, 1e-9));
}

TEST_CASE ("STRETCH: Fades skalieren mit", "[engine][align]")
{
    KickState s;
    s.align().tightness = AlignMode::tight;

    const std::vector<PartTiming> parts { makeAnchor(), makeTail() };

    auto r = AlignEngine::runStretchCrunch (s, parts);

    REQUIRE_THAT (r.fadeScaleFactors[1], WithinAbs (r.stretchFactors[1], 1e-9));
}

TEST_CASE ("STRETCH: Faktor bleibt in 0,25..1", "[engine][align]")
{
    KickState s;
    s.align().tightness = AlignMode::tight;

    // Sehr kurzer Tail: die noetige Kuerzung ist groesser als die Dauer.
    PartTiming tail;
    tail.startSeconds = 0.30;
    tail.endSeconds   = 0.32;
    tail.role         = Role::rumble;

    const std::vector<PartTiming> parts { makeAnchor(), tail };

    auto r = AlignEngine::runStretchCrunch (s, parts);

    REQUIRE (r.stretchFactors[1] >= 0.25);
    REQUIRE (r.stretchFactors[1] <= 1.0);
}

TEST_CASE ("STRETCH: pinned bleibt", "[engine][align]")
{
    KickState s;
    s.align().tightness = AlignMode::tight;

    const std::vector<PartTiming> parts { makeAnchor(), makeTail (true) };

    auto r = AlignEngine::runStretchCrunch (s, parts);

    REQUIRE_THAT (r.stretchFactors[1], WithinAbs (1.0, 1e-9));
}

TEST_CASE ("ALIGN: run waehlt die Methode", "[engine][align]")
{
    const std::vector<PartTiming> parts { makeAnchor(), makeTail() };

    KickState stretch;
    stretch.align().method = AlignMethod::stretchPunch;

    auto rStretch = AlignEngine::run (stretch, parts);
    REQUIRE (rStretch.stretchFactors[1] != 1.0);

    KickState move;
    move.align().method = AlignMethod::moveCrunch;

    auto rMove = AlignEngine::run (move, parts);
    REQUIRE (rMove.offsetsSeconds[1] != 0.0);
}

TEST_CASE ("ALIGN: ausgeschaltet", "[engine][align]")
{
    KickState s;
    s.align().enabled = false;
    s.align().method  = AlignMethod::stretchPunch;

    const std::vector<PartTiming> parts { makeAnchor(), makeTail() };

    auto r = AlignEngine::run (s, parts);

    REQUIRE_FALSE (r.changed);
}
