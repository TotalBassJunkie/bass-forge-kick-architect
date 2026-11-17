#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "core/ParameterRegistry.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Registry deckt alle Original-IDs ab", "[registry]")
{
    const auto& registry = ParameterRegistry::get();

    REQUIRE (registry.numSlots() == 12);
    REQUIRE (registry.numFxSlots() == 8);
    REQUIRE (registry.size() == 12 * 20 + 12 + 8 * 14);

    // Stichproben aus dem Referenzformat
    for (const char* id : { "l0_vol", "l0_pitch", "l0_stretch", "l0_ficurve",
                            "l11_foshape", "l0_fistart", "l11_fistart",
                            "master_vol", "root_note",
                            "fx0_p0", "fx7_p13" })
    {
        INFO ("fehlende ID: " << id);
        REQUIRE (registry.find (id) != nullptr);
    }

    REQUIRE (registry.find ("gibt_es_nicht") == nullptr);
}

TEST_CASE ("Normalisierung ist umkehrbar", "[registry]")
{
    const auto& registry = ParameterRegistry::get();

    for (const auto& p : registry.all())
    {
        // Bei Schaltern ist die Abbildung absichtlich nicht bijektiv:
        // alles unter 0.5 wird zu 0, alles darueber zu 1.
        if (p.curve == ParamCurve::boolean)
            continue;

        for (float n : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto eng = p.toEngineering (n);
            const auto back = p.toNormalised (eng);
            INFO ("Parameter " << p.id << " bei " << n);
            REQUIRE_THAT (back, WithinAbs (n, 1.0e-4));
        }
    }
}

TEST_CASE ("Schalter runden auf 0 oder 1", "[registry]")
{
    const auto& registry = ParameterRegistry::get();
    const auto* mute = registry.find ("l0_mute");
    REQUIRE (mute != nullptr);

    REQUIRE_THAT (mute->toEngineering (0.0f),  WithinAbs (0.0f, 1.0e-6));
    REQUIRE_THAT (mute->toEngineering (1.0f),  WithinAbs (1.0f, 1.0e-6));
    // 0.5 gilt als "an"
    REQUIRE_THAT (mute->toEngineering (0.5f),  WithinAbs (1.0f, 1.0e-6));
}

TEST_CASE ("Defaults liegen im gueltigen Bereich", "[registry]")
{
    const auto& registry = ParameterRegistry::get();

    for (const auto& p : registry.all())
    {
        const auto d = p.normalisedDefault();
        INFO ("Parameter " << p.id << " Default " << d);
        REQUIRE (d >= 0.0f);
        REQUIRE (d <= 1.0f);
        REQUIRE (p.engMin <= p.engMax);
    }
}

TEST_CASE ("Stretch folgt der gemessenen Potenzkurve", "[registry]")
{
    const auto& registry = ParameterRegistry::get();
    const auto* stretch = registry.find ("l0_stretch");
    REQUIRE (stretch != nullptr);

    // Bereich aus dem Handbuch: 0.25x .. 8x
    REQUIRE_THAT (stretch->toEngineering (0.0f), WithinAbs (0.25f, 1.0e-6));
    REQUIRE_THAT (stretch->toEngineering (1.0f), WithinAbs (8.0f, 1.0e-3));

    // Gemessen an den Werk-Presets: 1.0x wird als 0.374989420 gespeichert.
    // Eine Logarithmus-Kurve wuerde 0.4 liefern und ist damit widerlegt.
    REQUIRE_THAT (stretch->toNormalised (1.0f), WithinAbs (0.374989f, 5.0e-5));
    REQUIRE_THAT (stretch->toEngineering (0.374989f), WithinAbs (1.0f, 1.0e-3));
}
