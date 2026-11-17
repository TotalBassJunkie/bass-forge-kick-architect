#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "library/RandomizerEngine.h"
#include "library/StyleModel.h"

#include <set>

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    // Kleines genres.json-Fixture fuer die Engine-Tests. Die Ids sind
    // erfunden; Struktur und Regel-Codes entsprechen dem Referenzformat.
    const char* kFixtureJson = R"json(
{
  "schemaVersion": 3,
  "roleRegistry": {
    "punch": { "display": "PUNCH" },
    "punch_enhancer": { "display": "PUNCH ENHANCER" },
    "click": { "display": "CLICK" },
    "rumble": { "display": "RUMBLE" },
    "crunch": { "display": "CRUNCH" },
    "rumble_enhancer": { "display": "RUMBLE ENHANCER" },
    "top": { "display": "TOP" }
  },
  "rules": [
    { "type": "exclude", "a": "melodic", "b": "atonal", "scope": "kick" }
  ],
  "pools": [
    { "id": "PUN-A", "folder": "Main - Punches/A", "role": "punch", "tonality": "neutral" },
    { "id": "PUN-B", "folder": "Main - Punches/B", "role": "punch", "tonality": "neutral",
      "fileWeights": { "Punch Heavy": 0.3 } },
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
      "id": "kit",
      "display": "KIT",
      "short": "KIT",
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
      "defaultPreset": "Kit"
    },
    {
      "id": "dup",
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
      "id": "excl",
      "display": "EXCL",
      "accent": "#445566",
      "pools": { "PUN-A": 1, "CLK": 1, "RMB-ATO": 1, "CRN-MEL": 1 },
      "layerRules": {
        "punch": { "min": 1, "max": 1 },
        "click": { "min": 1, "max": 1 },
        "rumble": { "min": 0, "max": 1 },
        "crunch": { "min": 0, "max": 1 }
      },
      "rules": [
        { "code": "1", "pools": [ "PUN-A" ] },
        { "code": "1", "pools": [ "RMB-ATO" ] },
        { "code": "1", "pools": [ "CRN-MEL" ] },
        { "code": "1", "pools": [ "CLK" ] }
      ]
    },
    {
      "id": "excl2",
      "display": "EXCL2",
      "accent": "#445566",
      "pools": { "PUN-A": 1, "CLK": 1, "RMB-ATO": 1, "CRN-MEL": 1 },
      "layerRules": {
        "punch": { "min": 1, "max": 1 },
        "click": { "min": 1, "max": 1 },
        "rumble": { "min": 0, "max": 1 },
        "crunch": { "min": 0, "max": 1 },
        "rumble_enhancer": { "min": 0, "max": 0 }
      },
      "rules": [
        { "code": "1", "pools": [ "PUN-A" ] },
        { "code": "1", "pools": [ "RMB-ATO" ] },
        { "code": "1", "pools": [ "CRN-MEL" ] },
        { "code": "1", "pools": [ "CLK" ] }
      ]
    },
    {
      "id": "min",
      "display": "MIN",
      "accent": "#778899",
      "pools": { "PUN-A": 1, "CLK": 1, "RMB-ATO": 1 },
      "layerRules": {
        "punch": { "min": 1, "max": 1 },
        "click": { "min": 1, "max": 1 },
        "rumble": { "min": 1, "max": 1 }
      },
      "rules": [
        { "code": "1", "pools": [ "PUN-A" ] },
        { "code": "2", "pools": [ "RMB-ATO" ] },
        { "code": "1", "pools": [ "CLK" ] }
      ]
    },
    {
      "id": "weight",
      "display": "WEIGHT",
      "accent": "#AABBCC",
      "pools": { "PUN-B": 1 },
      "layerRules": { "punch": { "min": 1, "max": 1 } },
      "rules": [ { "code": "1", "pools": [ "PUN-B" ] } ]
    },
    {
      "id": "share",
      "display": "SHARE",
      "accent": "#DDEEFF",
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
    },
    {
      "id": "triple",
      "display": "TRIPLE",
      "accent": "#010203",
      "pools": { "PUN-A": 1, "CRN-MEL": 1, "CLK": 1 },
      "layerRules": {
        "punch": { "min": 1, "max": 1 },
        "crunch": { "min": 1, "max": 1 },
        "click": { "min": 1, "max": 1 }
      },
      "rules": [
        { "code": "1", "pools": [ "PUN-A" ] },
        { "code": "1", "pools": [ "CRN-MEL" ] },
        { "code": "1", "pools": [ "CLK" ] }
      ]
    }
  ]
}
)json";

    /** Liefert je Pool drei erfundene Dateien; PUN-B bekommt sprechende Namen. */
    class TestPoolResolver : public PoolResolver
    {
    public:
        std::vector<juce::String> samplePathsForPool (const juce::String& poolId) const override
        {
            if (emptyPools.contains (poolId))
                return {};

            std::vector<juce::String> result;

            if (poolId == "PUN-B")
            {
                result.push_back (poolId + "/Punch Heavy 13 150.wav");
                result.push_back (poolId + "/Punch Normal 1 150.wav");
                result.push_back (poolId + "/Punch Normal 2 150.wav");
                return result;
            }

            for (int i = 1; i <= samplesPerPool; ++i)
                result.push_back (poolId + "/" + poolId + " " + juce::String (i) + " 150.wav");

            return result;
        }

        int samplesPerPool = 3;
        juce::StringArray emptyPools;
    };

    /** No-Repeat-Tracker, der nur einen Pool beobachtet. */
    class TestTracker : public NoRepeatTracker
    {
    public:
        bool isBlocked (const juce::String& poolId, const juce::String& samplePath) const override
        {
            if (poolId != watchedPool)
                return false;

            return blocked.contains (samplePath);
        }

        void markUsed (const juce::String& poolId, const juce::String& samplePath) override
        {
            if (poolId == watchedPool)
                blocked.add (samplePath);
        }

        juce::String watchedPool { "RMB-ATO" };
        juce::StringArray blocked;
    };

    int countRole (const std::vector<RolledPart>& parts, Role role)
    {
        int count = 0;

        for (const auto& part : parts)
            if (part.role == role)
                ++count;

        return count;
    }

    int countPool (const std::vector<RolledPart>& parts, const juce::String& poolId)
    {
        int count = 0;

        for (const auto& part : parts)
            if (part.poolId == poolId)
                ++count;

        return count;
    }

    juce::String sampleOfPool (const std::vector<RolledPart>& parts, const juce::String& poolId)
    {
        for (const auto& part : parts)
            if (part.poolId == poolId)
                return part.samplePath;

        return {};
    }

    bool hasTonality (const StyleModel& model, const std::vector<RolledPart>& parts, Tonality tonality)
    {
        for (const auto& part : parts)
        {
            const auto* pool = model.findPool (part.poolId);

            if (pool != nullptr && pool->tonality == tonality)
                return true;
        }

        return false;
    }

    juce::String signatureOf (const std::vector<RolledPart>& parts)
    {
        juce::String signature;

        for (const auto& part : parts)
        {
            signature << part.poolId << ':' << juce::String (part.gainDb, 4)
                      << (part.optional ? ":opt" : ":req") << '|';
        }

        return signature;
    }

    const StyleModel& fixtureModel()
    {
        static StyleModel model;
        static bool loaded = false;

        if (! loaded)
        {
            juce::String error;
            model.parseFromJson (juce::String (kFixtureJson), error);
            loaded = true;
        }

        return model;
    }
} // namespace

TEST_CASE ("Fixture wird geladen", "[randomizer]")
{
    const auto& model = fixtureModel();

    REQUIRE (model.styles().size() == (size_t) 8);
    REQUIRE (model.pools().size() == (size_t) 9);
    REQUIRE (model.findStyle ("kit") != nullptr);
}

TEST_CASE ("Gleicher Seed liefert dasselbe Ergebnis", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    RollRequest request;
    request.styleId = "kit";
    request.maxSlots = 12;
    request.seed = 4711;

    const auto first  = engine.roll (request);
    const auto second = engine.roll (request);

    REQUIRE (first.size() == second.size());
    REQUIRE (first.size() > 0);

    for (size_t i = 0; i < first.size(); ++i)
    {
        REQUIRE (first[i].role == second[i].role);
        REQUIRE (first[i].poolId == second[i].poolId);
        REQUIRE (first[i].samplePath == second[i].samplePath);
        REQUIRE (first[i].optional == second[i].optional);
        REQUIRE_THAT (first[i].gainDb, WithinAbs (second[i].gainDb, 0.0));
    }
}

TEST_CASE ("Verschiedene Seeds liefern verschiedene Ergebnisse", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    std::set<juce::String> signatures;

    for (std::uint64_t seed = 1; seed <= 32; ++seed)
    {
        RollRequest request;
        request.styleId = "kit";
        request.seed = seed;

        signatures.insert (signatureOf (engine.roll (request)));
    }

    REQUIRE (signatures.size() > 1);
}

TEST_CASE ("Pflichtteile sind immer vorhanden", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    const auto* style = model.findStyle ("kit");
    REQUIRE (style != nullptr);

    for (std::uint64_t seed = 1; seed <= 64; ++seed)
    {
        RollRequest request;
        request.styleId = "kit";
        request.seed = seed;

        const auto parts = engine.roll (request);

        REQUIRE (countRole (parts, Role::punch) == style->minFor (Role::punch));
        REQUIRE (countRole (parts, Role::rumble) == 1);
        REQUIRE (countRole (parts, Role::rumbleEnhancer) == 1);
        REQUIRE (countRole (parts, Role::top) == 1);
        REQUIRE (countRole (parts, Role::click) >= style->minFor (Role::click));
        REQUIRE (countRole (parts, Role::click) <= style->maxFor (Role::click));

        // Genau die Pflichtteile sind als "required" markiert (fuenf Rollen
        // mit Minimum 1), alles darueber hinaus ist optional.
        int required = 0;
        int optional = 0;

        for (const auto& part : parts)
        {
            if (part.optional)
                ++optional;
            else
                ++required;
        }

        REQUIRE (required == 5);
        REQUIRE (required + optional == (int) parts.size());

        // Der Punch kommt zuerst (stabile Reihenfolge).
        REQUIRE (! parts.empty());
        REQUIRE (parts.front().role == Role::punch);
    }
}

TEST_CASE ("Obergrenze maxSlots wird eingehalten", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    for (std::uint64_t seed = 1; seed <= 32; ++seed)
    {
        RollRequest request;
        request.styleId = "kit";
        request.maxSlots = 6;
        request.seed = seed;

        const auto parts = engine.roll (request);

        REQUIRE ((int) parts.size() <= 6);
        REQUIRE ((int) parts.size() >= 5);   // Summe der Pflichtrollen
    }

    // Ein Style mit nur drei Pflichtteilen bleibt auch bei kleinem Deckel
    // vollstaendig (Pflicht schlaegt Deckel, siehe Spec).
    for (std::uint64_t seed = 1; seed <= 16; ++seed)
    {
        RollRequest request;
        request.styleId = "triple";
        request.maxSlots = 4;
        request.seed = seed;

        REQUIRE ((int) engine.roll (request).size() == 3);

        request.maxSlots = 0;
        REQUIRE ((int) engine.roll (request).size() == 3);
    }
}

TEST_CASE ("layerRules-Minimum wird nachgefuellt", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    // Die Rumble-Regel ist nur "code 2" (Wahrscheinlichkeit), aber die Rolle
    // ist Pflicht -> die Engine muss nachlegen.
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        RollRequest request;
        request.styleId = "min";
        request.seed = seed;

        const auto parts = engine.roll (request);

        REQUIRE (countRole (parts, Role::rumble) == 1);
        REQUIRE (countPool (parts, "RMB-ATO") == 1);
        REQUIRE (countRole (parts, Role::punch) == 1);
        REQUIRE (countRole (parts, Role::click) == 1);
    }
}

TEST_CASE ("Ausschlussregel melodic x atonal greift", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    for (std::uint64_t seed = 1; seed <= 16; ++seed)
    {
        RollRequest request;
        request.styleId = "excl";
        request.seed = seed;

        const auto parts = engine.roll (request);

        REQUIRE (! (hasTonality (model, parts, Tonality::melodic)
                    && hasTonality (model, parts, Tonality::atonal)));

        // Der melodische Crunch faellt weg (Ersatz ist der atonale Enhancer).
        REQUIRE (countRole (parts, Role::crunch) == 0);
        REQUIRE (countPool (parts, "RMB-ATO") == 1);
    }

    // Ohne passenden Ersatzpool wird der Part entfernt statt umgestellt.
    for (std::uint64_t seed = 1; seed <= 16; ++seed)
    {
        RollRequest request;
        request.styleId = "excl2";
        request.seed = seed;

        const auto parts = engine.roll (request);

        REQUIRE (! (hasTonality (model, parts, Tonality::melodic)
                    && hasTonality (model, parts, Tonality::atonal)));
        REQUIRE (countRole (parts, Role::crunch) == 0);
        REQUIRE ((int) parts.size() == 3);
    }
}

TEST_CASE ("duplicateGain macht die zweite Pool-Belegung leiser", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    RollRequest request;
    request.styleId = "dup";
    request.seed = 1;

    const auto parts = engine.roll (request);

    REQUIRE (countRole (parts, Role::rumble) == 2);
    REQUIRE (countPool (parts, "RMB-DUP") == 2);

    // Reihenfolge im Ergebnis: punch, dann die beiden Rumbles (Regel 1-1).
    REQUIRE (countRole (parts, Role::punch) == 1);
    REQUIRE (parts[1].poolId == "RMB-DUP");
    REQUIRE_THAT (parts[1].gainDb, WithinAbs (0.0f, 1.0e-5));
    REQUIRE_THAT (parts[2].gainDb, WithinAbs (-2.4988f, 1.0e-3));
}

TEST_CASE ("No-Repeat-Tracker verhindert Wiederholung", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    TestTracker tracker;

    RandomizerEngine engine (model, resolver);
    engine.setNoRepeatTracker (&tracker);

    RollRequest request;
    request.styleId = "min";
    request.seed = 11;

    const auto first = engine.roll (request);
    const auto firstSample = sampleOfPool (first, "RMB-ATO");
    REQUIRE (firstSample.isNotEmpty());

    request.seed = 12;
    const auto second = engine.roll (request);
    const auto secondSample = sampleOfPool (second, "RMB-ATO");
    REQUIRE (secondSample.isNotEmpty());

    REQUIRE (firstSample != secondSample);
    REQUIRE (! tracker.blocked.isEmpty());
}

TEST_CASE ("fileWeights sind nur Gewichte, keine Sperre", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    int heavyCount = 0;
    int normalCount = 0;

    for (std::uint64_t seed = 1; seed <= 300; ++seed)
    {
        RollRequest request;
        request.styleId = "weight";
        request.seed = seed;

        const auto parts = engine.roll (request);
        REQUIRE (parts.size() == (size_t) 1);

        if (parts[0].samplePath.contains ("Punch Heavy"))
            ++heavyCount;
        else
            ++normalCount;
    }

    // Gewicht 0.3 gegen 1.0/1.0 -> deutlich seltener, aber nicht ausgeschlossen.
    REQUIRE (heavyCount > 0);
    REQUIRE (normalCount > 0);
    REQUIRE (heavyCount < normalCount);
}

TEST_CASE ("shares steuern die Verteilung", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    int countA = 0;
    int countB = 0;

    for (std::uint64_t seed = 1; seed <= 300; ++seed)
    {
        RollRequest request;
        request.styleId = "share";
        request.seed = seed;

        const auto parts = engine.roll (request);
        REQUIRE (parts.size() == (size_t) 1);

        if (parts[0].poolId == "PUN-A")
            ++countA;
        else if (parts[0].poolId == "PUN-B")
            ++countB;
    }

    REQUIRE (countA + countB == 300);
    REQUIRE (countA > countB);   // 70 % gegen 30 %
}

TEST_CASE ("Pool ohne Samples wird gemeldet, nicht stillschweigend verbaut", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    resolver.emptyPools.add ("RMB-DUP");

    RandomizerEngine engine (model, resolver);

    RollRequest request;
    request.styleId = "kit";
    request.seed = 3;

    const auto parts = engine.roll (request);

    for (const auto& part : parts)
        REQUIRE (part.samplePath.isNotEmpty());

    REQUIRE (engine.lastWarnings().size() > 0);
}

TEST_CASE ("FREE BUILD und unbekannter Style liefern ein Grundgeruest", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    RollRequest request;
    request.freeBuild = true;
    request.styleId = "";
    request.seed = 7;

    const auto freeParts = engine.roll (request);

    REQUIRE (! freeParts.empty());
    REQUIRE ((int) freeParts.size() <= KickState::kNumSlots);
    REQUIRE (countRole (freeParts, Role::punch) >= 1);
    REQUIRE (freeParts.front().role == Role::punch);

    for (const auto& part : freeParts)
    {
        REQUIRE (part.poolId.isNotEmpty());
        REQUIRE (part.samplePath.isNotEmpty());
    }

    // Unbekannte Style-id faellt auf FREE BUILD zurueck und meldet das.
    request.freeBuild = false;
    request.styleId = "gibt-es-nicht";
    request.seed = 8;

    const auto fallbackParts = engine.roll (request);

    REQUIRE (! fallbackParts.empty());
    REQUIRE (engine.lastWarnings().size() > 0);
}

TEST_CASE ("Rollen-Reihenfolge ist stabil und aufsteigend", "[randomizer]")
{
    const auto& model = fixtureModel();
    TestPoolResolver resolver;
    RandomizerEngine engine (model, resolver);

    RollRequest request;
    request.styleId = "kit";
    request.seed = 5;

    const auto parts = engine.roll (request);
    REQUIRE (! parts.empty());

    int lastIndex = -1;

    for (const auto& part : parts)
    {
        int index = 99;

        switch (part.role)
        {
            case Role::punch:            index = 0; break;
            case Role::punchEnhancer:    index = 1; break;
            case Role::click:            index = 2; break;
            case Role::crunch:           index = 3; break;
            case Role::rumble:           index = 4; break;
            case Role::rumbleEnhancer:   index = 5; break;
            case Role::schranzEnhancer:  index = 6; break;
            case Role::top:              index = 7; break;
            case Role::none:             index = 99; break;
        }

        REQUIRE (index >= lastIndex);
        lastIndex = index;
    }
}
