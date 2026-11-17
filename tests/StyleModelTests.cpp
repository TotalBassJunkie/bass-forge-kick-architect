#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "library/StyleModel.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    // Kleines genres.json-Fixture: gleiche Struktur wie das Referenzformat,
    // aber nur die Felder, die das Modell kennt. Die Pool-/Style-Ids sind
    // erfunden; es wird kein Referenz-Content ins Repo uebernommen.
    const char* kFixtureJson = R"json(
{
  "schemaVersion": 3,
  "comment": "Test-Fixture, wird vom Parser ignoriert",
  "roleRegistry": {
    "punch": { "category": "punch_body", "display": "PUNCH" },
    "punch_enhancer": { "category": "punch_enhance", "display": "PUNCH ENHANCER" },
    "click": { "category": "click", "display": "CLICK" },
    "rumble": { "category": "tail", "display": "RUMBLE" },
    "crunch": { "category": "tail", "display": "CRUNCH" },
    "top": { "category": "top", "display": "TOP" }
  },
  "kitPattern": "KICK_(\\d+)_",
  "categoriesRequired": [ "punch_body", "tail" ],
  "rules": [
    { "type": "exclude", "a": "melodic", "b": "atonal", "scope": "kick" }
  ],
  "pools": [
    { "id": "PUN-A", "folder": "Main - Punches/A", "role": "punch", "tonality": "neutral" },
    { "id": "PUN-B", "folder": "Main - Punches/B", "role": "punch", "tonality": "neutral",
      "fileWeights": { "Punch Heavy": 0.3 }, "uiOrder": 1 },
    { "id": "CLK", "folder": "Transients (Clicks)", "role": "click", "tonality": "neutral" },
    { "id": "RMB-ATO", "folder": "Main - Rumbles/Atonal", "role": "rumble", "tonality": "atonal" },
    { "id": "RMB-DUP", "folder": "Main - Rumbles/Schranz", "role": "rumble",
      "tonality": "atonal", "duplicateGain": 0.75 },
    { "id": "CRN-MEL", "folder": "Main - Crunches/Melodic", "role": "crunch",
      "tonality": "melodic" },
    { "id": "ENR", "folder": "Enhancers (Rumbles)", "role": "rumble_enhancer",
      "tonality": "atonal" },
    { "id": "PTP", "folder": "Transients (Punch Top)", "role": "punch_enhancer",
      "tonality": "neutral", "defaultGain": 0.7 },
    { "id": "TOP", "folder": "Transients (Newstyle Tops)", "role": "top", "tonality": "neutral" }
  ],
  "styles": [
    {
      "id": "test_style",
      "display": "TEST",
      "short": "TST",
      "accent": "#DE2A2A",
      "pools": { "PUN-A": 1, "PUN-B": 1, "CLK": 2, "RMB-DUP": 1, "ENR": 1, "TOP": 1, "PTP": 1 },
      "layerRules": {
        "punch": { "min": 1, "max": 1 },
        "click": { "min": 1, "max": 2 },
        "rumble": { "min": 1, "max": 1 },
        "rumble_enhancer": { "min": 1, "max": 1 },
        "top": { "min": 1, "max": 1 },
        "punch_enhancer": { "min": 0, "max": 2 }
      },
      "rules": [
        { "code": "3", "pools": [ "PUN-A", "PUN-B" ] },
        { "code": "1", "pools": [ "RMB-DUP" ] },
        { "code": "1", "pools": [ "ENR" ] },
        { "code": "1", "pools": [ "TOP" ] },
        { "code": "1-2", "pools": [ "CLK" ], "dbPair": [ -5, -5 ] },
        { "code": "2", "pools": [ "PTP" ], "db": [ -20, -12 ] }
      ],
      "defaultPreset": "Fixture One"
    },
    {
      "id": "dup_style",
      "display": "DUP",
      "accent": "#112233",
      "pools": { "PUN-A": 1, "RMB-DUP": 1 },
      "layerRules": { "punch": { "min": 1, "max": 1 }, "rumble": { "min": 2, "max": 2 } },
      "rules": [
        { "code": "1", "pools": [ "PUN-A" ] },
        { "code": "1-1", "pools": [ "RMB-DUP" ] }
      ]
    },
    {
      "id": "share_style",
      "display": "SHARE",
      "accent": "#445566",
      "pools": { "PUN-A": 1, "PUN-B": 1 },
      "layerRules": { "punch": { "min": 1, "max": 1 } },
      "rules": [
        {
          "code": "3",
          "pools": [ "PUN-A", "PUN-B" ],
          "shares": [
            { "pools": [ "PUN-A" ], "pct": 70 },
            { "pools": [ "PUN-B" ], "pct": 30 }
          ]
        }
      ]
    }
  ]
}
)json";

    // Minimal-JSON: nur die strukturell noetigen Felder -> alles andere Default.
    const char* kMinimalJson = R"json(
{
  "pools": [ { "id": "NUR-ID" } ],
  "styles": [ { "id": "nur_id" } ]
}
)json";

    // Absichtlich schmutziges JSON: unbekannte Rolle, doppelte id, Eintrag
    // ohne id, unbekanntes Feld, defaultValue 0, unlesbare Farbe, code "9".
    const char* kRoughJson = R"json(
{
  "roleRegistry": { "punch": { "display": "PUNCH" } },
  "pools": [
    { "id": "A", "role": "punch", "tonality": "neutral", "unbekanntesFeld": 7 },
    { "id": "A", "role": "punch" },
    { "role": "punch" },
    { "id": "B", "role": "quatsch", "tonality": "irgendwas", "defaultGain": 0 }
  ],
  "styles": [
    { "id": "s1",
      "accent": "keinefarbe",
      "pools": { "A": 1, "B": 2 },
      "layerRules": { "punch": { "min": 1 } },
      "rules": [
        { "code": "1", "pools": [ "A" ] },
        { "code": "9", "pools": [ "B" ] }
      ] }
  ]
}
)json";
} // namespace

TEST_CASE ("genres.json wird gelesen", "[style]")
{
    StyleModel model;
    juce::String error;

    REQUIRE (model.parseFromJson (juce::String (kFixtureJson), error));
    REQUIRE (error.isEmpty());
    REQUIRE (model.warnings().isEmpty());

    REQUIRE (model.roleRegistry().size() == (size_t) 6);
    REQUIRE (model.pools().size() == (size_t) 9);
    REQUIRE (model.styles().size() == (size_t) 3);
}

TEST_CASE ("Pool-Felder werden korrekt abgebildet", "[style]")
{
    StyleModel model;
    juce::String error;
    REQUIRE (model.parseFromJson (juce::String (kFixtureJson), error));

    const auto* punchA = model.findPool ("PUN-A");
    REQUIRE (punchA != nullptr);
    REQUIRE (punchA->role == Role::punch);
    REQUIRE (punchA->tonality == Tonality::neutral);
    REQUIRE (punchA->folder == "Main - Punches/A");
    REQUIRE_THAT (punchA->defaultGain, WithinAbs (1.0f, 1.0e-6));
    REQUIRE_THAT (punchA->duplicateGain, WithinAbs (1.0f, 1.0e-6));
    REQUIRE (punchA->uiOrder == 0);
    REQUIRE (punchA->fileWeights.empty());

    const auto* punchB = model.findPool ("PUN-B");
    REQUIRE (punchB != nullptr);
    REQUIRE (punchB->uiOrder == 1);
    REQUIRE (punchB->fileWeights.size() == (size_t) 1);
    REQUIRE_THAT (punchB->fileWeights.at ("Punch Heavy"), WithinAbs (0.3f, 1.0e-6));

    // Rollen-Mapping des Referenzformats
    const auto* ptp = model.findPool ("PTP");
    REQUIRE (ptp != nullptr);
    REQUIRE (ptp->role == Role::punchEnhancer);
    REQUIRE_THAT (ptp->defaultGain, WithinAbs (0.7f, 1.0e-6));

    const auto* enr = model.findPool ("ENR");
    REQUIRE (enr != nullptr);
    REQUIRE (enr->role == Role::rumbleEnhancer);
    REQUIRE (enr->tonality == Tonality::atonal);

    const auto* crn = model.findPool ("CRN-MEL");
    REQUIRE (crn != nullptr);
    REQUIRE (crn->role == Role::crunch);
    REQUIRE (crn->tonality == Tonality::melodic);

    const auto* dup = model.findPool ("RMB-DUP");
    REQUIRE (dup != nullptr);
    REQUIRE (dup->role == Role::rumble);
    REQUIRE_THAT (dup->duplicateGain, WithinAbs (0.75f, 1.0e-6));

    REQUIRE (model.findPool ("gibt-es-nicht") == nullptr);

    const auto punchPools = model.poolsForRole (Role::punch);
    REQUIRE (punchPools.size() == (size_t) 2);
}

TEST_CASE ("Style-Felder werden korrekt abgebildet", "[style]")
{
    StyleModel model;
    juce::String error;
    REQUIRE (model.parseFromJson (juce::String (kFixtureJson), error));

    const auto* style = model.findStyle ("test_style");
    REQUIRE (style != nullptr);

    REQUIRE (style->id == "test_style");
    REQUIRE (style->display == "TEST");
    REQUIRE (style->shortName == "TST");
    REQUIRE (style->accent == juce::Colour ((juce::uint32) 0xffde2a2a));
    REQUIRE (style->defaultPreset == "Fixture One");

    // pools-Gewichte
    REQUIRE (style->poolWeight ("CLK") == 2);
    REQUIRE (style->poolWeight ("PUN-A") == 1);
    REQUIRE (style->poolWeight ("FREMD") == -1);

    // layerRules
    REQUIRE (style->hasLayerRule (Role::punch));
    REQUIRE (style->minFor (Role::punch) == 1);
    REQUIRE (style->maxFor (Role::punch) == 1);
    REQUIRE (style->minFor (Role::click) == 1);
    REQUIRE (style->maxFor (Role::click) == 2);
    REQUIRE (style->minFor (Role::punchEnhancer) == 0);
    REQUIRE (style->maxFor (Role::punchEnhancer) == 2);
    REQUIRE (style->maxFor (Role::schranzEnhancer) == -1);
    REQUIRE (! style->hasLayerRule (Role::schranzEnhancer));

    // rules
    REQUIRE (style->rules.size() == (size_t) 6);
    REQUIRE (style->rules[0].code == "3");
    REQUIRE (style->rules[0].pools.size() == (size_t) 2);
    REQUIRE (! style->rules[0].db.has_value());
    REQUIRE (! style->rules[0].dbPair.has_value());

    REQUIRE (style->rules[4].code == "1-2");
    REQUIRE (style->rules[4].dbPair.has_value());
    REQUIRE_THAT (style->rules[4].dbPair->first, WithinAbs (-5.0f, 1.0e-6));
    REQUIRE_THAT (style->rules[4].dbPair->second, WithinAbs (-5.0f, 1.0e-6));

    REQUIRE (style->rules[5].code == "2");
    REQUIRE (style->rules[5].db.has_value());
    REQUIRE_THAT (style->rules[5].db->first, WithinAbs (-20.0f, 1.0e-6));
    REQUIRE_THAT (style->rules[5].db->second, WithinAbs (-12.0f, 1.0e-6));

    // shares
    const auto* shareStyle = model.findStyle ("share_style");
    REQUIRE (shareStyle != nullptr);
    REQUIRE (shareStyle->rules.size() == (size_t) 1);
    REQUIRE (shareStyle->rules[0].shares.size() == (size_t) 2);
    REQUIRE (shareStyle->rules[0].shares[0].pools.size() == (size_t) 1);
    REQUIRE (shareStyle->rules[0].shares[0].pools[0] == "PUN-A");
    REQUIRE_THAT (shareStyle->rules[0].shares[0].percent, WithinAbs (70.0f, 1.0e-6));

    // addable fehlt im Fixture -> leer
    REQUIRE (style->addable.empty());

    // Rolle ohne Pools
    REQUIRE_THAT (model.defaultGainForRole (Role::punch), WithinAbs (1.0f, 1.0e-6));
    REQUIRE_THAT (model.defaultGainForRole (Role::punchEnhancer), WithinAbs (0.7f, 1.0e-6));
    REQUIRE_THAT (model.defaultGainForRole (Role::schranzEnhancer), WithinAbs (1.0f, 1.0e-6));
}

TEST_CASE ("Globale Ausschlussregel wird gelesen", "[style]")
{
    StyleModel model;
    juce::String error;
    REQUIRE (model.parseFromJson (juce::String (kFixtureJson), error));

    REQUIRE (model.exclusions().size() == (size_t) 1);
    REQUIRE (model.exclusions()[0].a == Tonality::melodic);
    REQUIRE (model.exclusions()[0].b == Tonality::atonal);
    REQUIRE (model.exclusions()[0].scope == "kick");
}

TEST_CASE ("Fehlende Felder bekommen Defaults", "[style]")
{
    StyleModel model;
    juce::String error;
    REQUIRE (model.parseFromJson (juce::String (kMinimalJson), error));

    REQUIRE (model.pools().size() == (size_t) 1);
    REQUIRE (model.pools()[0].id == "NUR-ID");
    REQUIRE (model.pools()[0].role == Role::none);
    REQUIRE (model.pools()[0].tonality == Tonality::neutral);
    REQUIRE (model.pools()[0].folder.isEmpty());
    REQUIRE_THAT (model.pools()[0].defaultGain, WithinAbs (1.0f, 1.0e-6));
    REQUIRE_THAT (model.pools()[0].duplicateGain, WithinAbs (1.0f, 1.0e-6));
    REQUIRE (model.pools()[0].uiOrder == 0);

    REQUIRE (model.styles().size() == (size_t) 1);
    const auto& style = model.styles()[0];
    REQUIRE (style.display == "nur_id");
    REQUIRE (style.shortName == "nur_id");
    REQUIRE (style.accent == juce::Colours::grey);
    REQUIRE (style.poolWeights.empty());
    REQUIRE (style.layerRules.empty());
    REQUIRE (style.rules.empty());
    REQUIRE (style.addable.empty());
    REQUIRE (style.defaultPreset.isEmpty());

    // Fehlende Struktur wird als Hinweis gemeldet, nicht als Fehler.
    REQUIRE (model.warnings().size() > 0);
    REQUIRE (model.exclusions().empty());
}

TEST_CASE ("Sauberes Parsen bei schmutziger Datei", "[style]")
{
    StyleModel model;
    juce::String error;

    REQUIRE (model.parseFromJson (juce::String (kRoughJson), error));

    // Eintrag ohne id und doppelte id fallen weg -> 2 Pools.
    REQUIRE (model.pools().size() == (size_t) 2);
    REQUIRE (model.findPool ("A") != nullptr);
    REQUIRE (model.findPool ("B") != nullptr);

    // unbekannte Rolle -> Role::none, unbekannte Tonalitaet -> neutral
    REQUIRE (model.findPool ("B")->role == Role::none);
    REQUIRE (model.findPool ("B")->tonality == Tonality::neutral);

    // defaultGain 0 ist unbrauchbar -> 1.0
    REQUIRE_THAT (model.findPool ("B")->defaultGain, WithinAbs (1.0f, 1.0e-6));

    const auto* style = model.findStyle ("s1");
    REQUIRE (style != nullptr);
    REQUIRE (style->accent == juce::Colours::grey);   // "keinefarbe"
    REQUIRE (style->shortName == "s1");               // folgt display
    REQUIRE (style->poolWeights.size() == (size_t) 2);
    REQUIRE (style->minFor (Role::punch) == 1);
    REQUIRE (style->maxFor (Role::punch) == 1);       // max fehlt -> = min
    REQUIRE (style->rules.size() == (size_t) 2);
    REQUIRE (style->rules[1].code == "9");            // Code bleibt Text

    REQUIRE (model.warnings().size() > 0);
}

TEST_CASE ("Unbrauchbare Eingaben liefern false und lassen das Modell heil", "[style]")
{
    StyleModel model;
    juce::String error;
    REQUIRE (model.parseFromJson (juce::String (kFixtureJson), error));
    REQUIRE (model.pools().size() == (size_t) 9);

    SECTION ("kein JSON")
    {
        error.clear();
        REQUIRE (! model.parseFromJson ("{ \"pools\": [ }", error));
        REQUIRE (error.isNotEmpty());
    }

    SECTION ("Wurzel ist ein Array")
    {
        error.clear();
        REQUIRE (! model.parseFromJson ("[ 1, 2, 3 ]", error));
        REQUIRE (error.isNotEmpty());
    }

    SECTION ("leerer Text")
    {
        error.clear();
        REQUIRE (! model.parseFromJson ("   ", error));
        REQUIRE (error.isNotEmpty());
    }

    SECTION ("pools fehlt")
    {
        error.clear();
        REQUIRE (! model.parseFromJson ("{ \"styles\": [] }", error));
        REQUIRE (error.isNotEmpty());
    }

    SECTION ("styles fehlt")
    {
        error.clear();
        REQUIRE (! model.parseFromJson ("{ \"pools\": [] }", error));
        REQUIRE (error.isNotEmpty());
    }

    // Nach jedem Fehlversuch steht das zuvor geladene Modell unveraendert da.
    REQUIRE (model.pools().size() == (size_t) 9);
    REQUIRE (model.styles().size() == (size_t) 3);
}

TEST_CASE ("reloadFromFile liest eine Datei", "[style]")
{
    const auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("bfka_stylemodel_" + juce::Uuid().toString() + ".json");

    REQUIRE (tempFile.replaceWithText (juce::String (kFixtureJson)));

    StyleModel model;
    juce::String error;
    REQUIRE (model.reloadFromFile (tempFile, error));
    REQUIRE (error.isEmpty());
    REQUIRE (model.pools().size() == (size_t) 9);
    REQUIRE (model.findStyle ("test_style") != nullptr);

    tempFile.deleteFile();

    StyleModel missing;
    error.clear();
    REQUIRE (! missing.reloadFromFile (tempFile, error));
    REQUIRE (error.isNotEmpty());
}

TEST_CASE ("Tonalitaet und Pegel-Helfer", "[style]")
{
    REQUIRE (tonalityFromString ("melodic") == Tonality::melodic);
    REQUIRE (tonalityFromString (" ATONAL ") == Tonality::atonal);
    REQUIRE (tonalityFromString ("irgendwas") == Tonality::neutral);
    REQUIRE (tonalityToString (Tonality::atonal) == "atonal");

    REQUIRE_THAT (gainToDb (1.0f), WithinAbs (0.0f, 1.0e-5));
    REQUIRE_THAT (gainToDb (0.7f), WithinAbs (-3.0980f, 1.0e-3));
    REQUIRE_THAT (gainToDb (0.75f), WithinAbs (-2.4988f, 1.0e-3));
    REQUIRE_THAT (dbToGain (0.0f), WithinAbs (1.0f, 1.0e-6));
    REQUIRE (gainToDb (0.0f) <= -99.0f);
}
