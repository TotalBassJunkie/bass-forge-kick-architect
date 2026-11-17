#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_data_structures/juce_data_structures.h>

#include "fx/FxModule.h"
#include "ui/FxChainModel.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    FxModuleState makeModule (const juce::String& type)
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = true;
        state.mix     = 1.0f;
        state.outGain = 1.0f;
        return state;
    }
} // namespace

TEST_CASE ("FxChainModel: leere Kette und Add", "[ui][model]")
{
    FxChainModel model;

    REQUIRE (model.size() == 0);
    REQUIRE (model.module (0) == nullptr);
    REQUIRE (! model.isValidIndex (0));

    const auto index = model.addModule (FxType::multiband);
    REQUIRE (index == 0);
    REQUIRE (model.size() == 1);

    const auto* added = model.module (0);
    REQUIRE (added != nullptr);
    REQUIRE (added->type == "ott");
    REQUIRE (added->enabled);
    REQUIRE_THAT (added->mix, WithinAbs (1.0f, 1.0e-6));
    REQUIRE_THAT (added->outGain, WithinAbs (1.0f, 1.0e-6));

    // Kein `data`: die Modul-Defaults gelten.
    REQUIRE (! added->data.isValid());

    // Weitere Typen haengen hinten an.
    REQUIRE (model.addModule (FxType::convolver) == 1);
    REQUIRE (model.addModule (FxType::dynamicEq) == 2);
    REQUIRE (model.size() == 3);
    REQUIRE (model.module (1)->type == "reverb");
    REQUIRE (model.module (2)->type == "dynEq");
}

TEST_CASE ("FxChainModel: Remove haelt die Reihenfolge konsistent", "[ui][model]")
{
    FxChainModel model;
    model.addModule (FxType::equalizer);
    model.addModule (FxType::distortion);
    model.addModule (FxType::transientShaper);

    REQUIRE (! model.removeModule (-1));
    REQUIRE (! model.removeModule (3));

    REQUIRE (model.removeModule (1));      // dist
    REQUIRE (model.size() == 2);
    REQUIRE (model.module (0)->type == "eq");
    REQUIRE (model.module (1)->type == "trans");

    REQUIRE (model.removeModule (1));
    REQUIRE (model.removeModule (0));
    REQUIRE (model.size() == 0);
    REQUIRE (! model.removeModule (0));
}

TEST_CASE ("FxChainModel: Move klemmt und meldet Aenderungen", "[ui][model]")
{
    FxChainModel model;
    model.addModule (FxType::equalizer);
    model.addModule (FxType::distortion);
    model.addModule (FxType::transientShaper);

    REQUIRE (model.moveModule (0, 2));
    REQUIRE (model.module (0)->type == "dist");
    REQUIRE (model.module (1)->type == "trans");
    REQUIRE (model.module (2)->type == "eq");

    // Vorwaerts ueber das Ende: wird auf den letzten Platz geklemmt.
    REQUIRE (model.moveModule (0, 99));
    REQUIRE (model.module (2)->type == "dist");

    // Rueckwaerts unter 0: wird auf 0 geklemmt.
    REQUIRE (model.moveModule (2, -5));
    REQUIRE (model.module (0)->type == "dist");

    REQUIRE (! model.moveModule (0, 0));       // unveraendert
    REQUIRE (! model.moveModule (7, 0));       // ungueltig
}

TEST_CASE ("FxChainModel: Duplicate kopiert den Zustand tief", "[ui][model]")
{
    FxChainModel model;

    auto eq = makeModule ("eq");
    eq.data = juce::ValueTree (juce::Identifier ("Module"));
    eq.data.setProperty (juce::Identifier ("lowgain"), 4.5, nullptr);
    eq.mix = 0.5f;
    eq.outGain = 1.25f;
    eq.enabled = false;

    model.chain().modules.push_back (std::move (eq));
    model.addModule (FxType::distortion);

    const auto copy = model.duplicateModule (0);
    REQUIRE (copy == 1);
    REQUIRE (model.size() == 3);

    REQUIRE (model.module (1)->type == "eq");
    REQUIRE (model.module (1)->mix == eq.mix);
    REQUIRE_THAT (model.module (1)->outGain, WithinAbs (1.25f, 1.0e-6));
    REQUIRE (! model.module (1)->enabled);
    REQUIRE_THAT ((double) model.module (1)->data.getProperty (juce::Identifier ("lowgain")),
                  WithinAbs (4.5, 1.0e-9));

    // Tiefe Kopie: eine Aenderung am Original darf die Kopie nicht treffen.
    model.module (0)->data.setProperty (juce::Identifier ("lowgain"), 1.0, nullptr);
    REQUIRE_THAT ((double) model.module (1)->data.getProperty (juce::Identifier ("lowgain")),
                  WithinAbs (4.5, 1.0e-9));

    // Der Distortion-Platz ist nach hinten gerueckt.
    REQUIRE (model.module (2)->type == "dist");

    REQUIRE (model.duplicateModule (-1) == -1);
    REQUIRE (model.duplicateModule (99) == -1);
}

TEST_CASE ("FxChainModel: Clear und Ketten-Bypass", "[ui][model]")
{
    FxChainModel model;
    model.addModule (FxType::equalizer);
    model.addModule (FxType::convolver);

    REQUIRE (! model.isChainBypassed());
    model.setChainBypass (true);
    REQUIRE (model.isChainBypassed());

    model.clear();
    REQUIRE (model.size() == 0);

    // Bypass bleibt nach Clear gesetzt (eigenstaendiger Schalter).
    REQUIRE (model.isChainBypassed());
}

TEST_CASE ("FxChainModel: Add mit unbekanntem Typ aendert nichts", "[ui][model]")
{
    FxChainModel model;

    REQUIRE (model.addModule ((FxType) 99) == -1);
    REQUIRE (model.size() == 0);
}
