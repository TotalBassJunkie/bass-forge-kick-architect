#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_core/juce_core.h>

#include "engine/AlignReport.h"
#include "engine/AlignEngine.h"
#include "core/KickState.h"

// B-57/B-58: Tests fuer LAST PASS-Report, RESULT LINE und ALIGN BLEND.

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    /** Ergebnis eines Laufs mit einer bewegten Tail (Luecke 100 ms -> 10 ms). */
    AlignResult makeFilledResult()
    {
        AlignResult result;
        result.changed         = true;
        result.offsetsSeconds  = { 0.0, -0.09 };
        result.gapsBeforeSeconds = { 0.0, 0.10 };
        result.gapsAfterSeconds  = { 0.0, 0.01 };
        result.report.add ("ALIGN NORMAL / MOVE CRUNCH");

        return result;
    }
} // namespace

TEST_CASE ("AlignReport: Einstellungen klemmen den Blend", "[engine][alignreport]")
{
    AlignSettings a;

    auto s = AlignReport::settingsFrom (a, 1.7);
    REQUIRE_THAT (s.alignBlend, WithinAbs (1.0, 1e-9));

    s = AlignReport::settingsFrom (a, -0.3);
    REQUIRE_THAT (s.alignBlend, WithinAbs (0.0, 1e-9));
}

TEST_CASE ("AlignReport: leerer Lauf", "[engine][alignreport]")
{
    // Ohne Aenderung und ohne Report-Zeilen ist nichts gelaufen.
    AlignResult r;
    auto rep = AlignReport::build (r, {});

    REQUIRE_FALSE (rep.ok);
    REQUIRE (rep.resultLine == "ALIGN --");
}

TEST_CASE ("AlignReport: gefuellter Lauf", "[engine][alignreport]")
{
    const auto r = makeFilledResult();
    auto rep = AlignReport::build (r, {});

    REQUIRE (rep.ok);
    REQUIRE_THAT (rep.maxGapMsBefore, WithinAbs (100.0, 1e-6));
    REQUIRE_THAT (rep.maxGapMsAfter,  WithinAbs (10.0,  1e-6));
    REQUIRE (rep.movedParts == 1);
    REQUIRE (rep.resultLine.contains ("ALIGN"));
    REQUIRE (rep.resultLine.contains ("100.0"));
    REQUIRE (rep.lines.size() >= 3);
}

TEST_CASE ("AlignReport: RESULT LINE abschaltbar", "[engine][alignreport]")
{
    const auto r = makeFilledResult();

    AlignReportSettings st;
    st.showResultLine = false;

    auto rep = AlignReport::build (r, st);
    REQUIRE (rep.resultLine.isEmpty());
}

TEST_CASE ("AlignReport: Gap-Zeilen abschaltbar", "[engine][alignreport]")
{
    const auto r = makeFilledResult();

    AlignReportSettings st;
    st.showGapRows = false;

    auto rep = AlignReport::build (r, st);
    REQUIRE (rep.lines.size() < 3 + r.report.size() + 2);
}
