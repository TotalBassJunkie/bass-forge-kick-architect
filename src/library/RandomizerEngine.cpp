#include "library/RandomizerEngine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <utility>

namespace bfka
{

namespace
{
    // =======================================================================
    // Gewaehlte Semantik des "code"-Feldes (B-71) - hier an einer Stelle,
    // damit der Rest der Engine nur noch die Tabelle benutzt.
    //
    // code ist "T" oder "T-T2":
    //   T  = Tier des ERSTEN Parts dieser Regel
    //   T2 = Tier des ZWEITEN Parts dieser Regel (nur wenn vorhanden)
    // Ein Tier ist eine Wahrscheinlichkeit, dass der Part ueberhaupt entsteht:
    //
    //   Tier 1 -> 1.00  "immer"                 (fester Pool / feste Belegung)
    //   Tier 2 -> 0.70  "haeufig, aber optional"
    //   Tier 3 -> 1.00  "immer, genau einer aus der Pool-Gruppe"
    //   Tier 4 -> 0.40  "selten/optional"
    //
    // Warum diese Lesart? Sie ist die einzige, die den kompletten Datenbestand
    // widerspruchsfrei erklaert:
    //  * Jede "3"-Regel hat >= 2 Pools (Gruppenwahl), jede "1"-Regel genau
    //    einen Pool. Beide Tiers sind "immer" -> 1 und 3 unterscheiden sich
    //    nur im Charakter der Wahl, nicht in der Pflicht.
    //  * "1-1" (schranz/ENS-SCZ) ergibt so ZWEI Parts - passend zu
    //    layerRules.schranz_enhancer min=2/max=2. Mit der Lesart "1 bis 1
    //    Teile" waere das Minimum dieser Rolle unerfuellbar.
    //  * "1-2" (CLK-STD) ergibt 1 oder 2 Clicks - passend zu click min=1/max=2.
    //  * "3-4" (rawstyle/PRV-*) ergibt 1 oder 2 Punch-Enhancer - passend zu
    //    punch_enhancer min=1/max=2.
    //  * "4-4" (industrial/PRV-*) ergibt 0..2 Enhancer - passend zu
    //    punch_enhancer min=0/max=2.
    //  * Tier 2/4 kommen im Datenbestand NUR bei Rollen mit min=0 vor; die
    //    konkreten Wahrscheinlichkeiten (0.70/0.40) sind Arbeitsannahmen und
    //    werden in B-77 statistisch gegen das Original geprueft.
    //
    // Die zweite Ziffer ist keine Obergrenze, sondern die Wahrscheinlichkeit
    // fuer den Partner-Part. Die Obergrenzen kommen ausschliesslich aus
    // layerRules (harte Grenze) und maxSlots.
    // =======================================================================
    constexpr float kTierProbability[] = { 0.0f, 1.0f, 0.70f, 1.0f, 0.40f };

    // Stabile Ausgabereihenfolge der Parts. Die echte Slot-Zuordnung ist
    // Sache von B-74; hier zaehlt nur, dass die Reihenfolge reproduzierbar ist
    // (punch zuerst, top zuletzt - wie die Slot-Karten im Original).
    constexpr Role kRoleOrder[] = {
        Role::punch, Role::punchEnhancer, Role::click, Role::crunch,
        Role::rumble, Role::rumbleEnhancer, Role::schranzEnhancer, Role::top
    };

    constexpr int kNumOrderedRoles = (int) (sizeof (kRoleOrder) / sizeof (kRoleOrder[0]));

    int roleOrderIndex (Role role)
    {
        for (int i = 0; i < kNumOrderedRoles; ++i)
            if (kRoleOrder[i] == role)
                return i;

        return kNumOrderedRoles;
    }

    /**
     * FREE BUILD hat keinen Style und damit keine layerRules. Dieses Geruest
     * ist die dokumentierte Arbeitsannahme (Konfidenz niedrig, siehe Spec):
     * ein Kick braucht Punch + Body, alles andere ist Beiwerk.
     */
    struct FreeBuildLayer
    {
        Role role;
        int  minCount;
        int  maxCount;
        int  tier;
    };

    constexpr FreeBuildLayer kFreeBuildLayers[] = {
        { Role::punch,            1, 1, 1 },
        { Role::click,            0, 2, 2 },
        { Role::crunch,           0, 1, 2 },
        { Role::rumble,           0, 1, 2 },
        { Role::punchEnhancer,    0, 1, 2 },
        { Role::rumbleEnhancer,   0, 1, 4 },
        { Role::schranzEnhancer,  0, 1, 4 },
        { Role::top,              0, 1, 2 }
    };

    constexpr int kNumFreeBuildLayers = (int) (sizeof (kFreeBuildLayers) / sizeof (kFreeBuildLayers[0]));

    /**
     * Ist im Modell keine globale Ausschlussregel definiert, gilt die
     * dokumentierte Grundregel des Referenzformats weiter.
     */
    std::vector<ExclusionRule> fallbackExclusions()
    {
        std::vector<ExclusionRule> rules;

        ExclusionRule rule;
        rule.a     = Tonality::melodic;
        rule.b     = Tonality::atonal;
        rule.scope = "kick";

        rules.push_back (rule);
        return rules;
    }
} // namespace

// ---------------------------------------------------------------------------
// Aufbau
// ---------------------------------------------------------------------------

RandomizerEngine::RandomizerEngine (const StyleModel& modelToUse, PoolResolver& resolverToUse)
    : model (modelToUse),
      resolver (resolverToUse),
      rng (0)
{
}

// ---------------------------------------------------------------------------
// Zufall
// ---------------------------------------------------------------------------

double RandomizerEngine::nextUnit()
{
    // 53 Bit Mantisse direkt aus dem Generator: reproduzierbar auf jeder
    // Standardbibliothek (std::uniform_real_distribution waere es nicht).
    return (double) (rng() >> 11) * (1.0 / 9007199254740992.0);
}

float RandomizerEngine::randomFloat (float lo, float hi)
{
    if (hi < lo)
        std::swap (lo, hi);

    return lo + (hi - lo) * (float) nextUnit();
}

int RandomizerEngine::pickUniform (int count)
{
    if (count <= 0)
        return -1;

    const auto index = (int) (nextUnit() * (double) count);
    return juce::jlimit (0, count - 1, index);
}

int RandomizerEngine::pickWeighted (const std::vector<float>& weights)
{
    if (weights.empty())
        return -1;

    float total = 0.0f;

    for (const auto weight : weights)
        if (std::isfinite (weight) && weight > 0.0f)
            total += weight;

    if (total <= 0.0f)
        return pickUniform ((int) weights.size());   // keine brauchbaren Gewichte

    const auto target = (float) (nextUnit() * (double) total);
    float accumulated = 0.0f;

    for (int i = 0; i < (int) weights.size(); ++i)
    {
        const auto weight = weights[(size_t) i];
        accumulated += (std::isfinite (weight) && weight > 0.0f) ? weight : 0.0f;

        if (target < accumulated)
            return i;
    }

    return (int) weights.size() - 1;
}

float RandomizerEngine::probabilityForTier (int tier) noexcept
{
    if (tier < 1 || tier > 4)
        return 0.0f;

    return kTierProbability[tier];
}

// ---------------------------------------------------------------------------
// code-Parser
// ---------------------------------------------------------------------------

std::vector<int> RandomizerEngine::parseRuleCode (const juce::String& code, juce::StringArray* warningsOut)
{
    std::vector<int> tiers;

    const auto text = code.trim();

    if (text.isEmpty())
    {
        // Fehlender code wird als Pflicht-Regel gelesen (konservativ).
        tiers.push_back (1);

        if (warningsOut != nullptr)
            warningsOut->add ("Regel ohne code -> wie '1' (Pflicht) behandelt.");

        return tiers;
    }

    const auto tokens = juce::StringArray::fromTokens (text, "-", "");

    for (int i = 0; i < tokens.size() && i < 2; ++i)
    {
        const auto token = tokens[i].trim();

        if (token.isEmpty())
            continue;

        if (! token.containsOnly ("0123456789"))
        {
            if (warningsOut != nullptr)
                warningsOut->add ("Regel-code '" + text + "': ungueltiges Zeichen ignoriert.");

            continue;
        }

        tiers.push_back (juce::jlimit (1, 4, token.getIntValue()));
    }

    if (tokens.size() > 2 && warningsOut != nullptr)
        warningsOut->add ("Regel-code '" + text + "' hat mehr als zwei Teile; Rest ignoriert.");

    if (tiers.empty())
    {
        tiers.push_back (1);

        if (warningsOut != nullptr)
            warningsOut->add ("Regel-code '" + text + "' unlesbar -> wie '1' (Pflicht) behandelt.");
    }

    return tiers;
}

// ---------------------------------------------------------------------------
// Pool-Auswahl
// ---------------------------------------------------------------------------

float RandomizerEngine::weightForPoolInRule (const StyleDef& style, const StyleRule& rule,
                                             const juce::String& poolId) const
{
    // shares: explizite Prozentverteilung. Der Gruppen-Prozentsatz wird
    // gleichmaessig auf die Pools der Gruppe verteilt (die Pools einer Gruppe
    // haben im Datenbestand identische Style-Gewichte, daher ist das
    // gleichwertig zu einer gewichteten Aufteilung).
    if (! rule.shares.empty())
    {
        for (const auto& share : rule.shares)
        {
            for (const auto& member : share.pools)
            {
                if (member != poolId)
                    continue;

                const auto groupSize = (float) juce::jmax (1, (int) share.pools.size());
                return juce::jmax (0.0f, share.percent) / groupSize;
            }
        }

        // Pool steht in keiner Gruppe -> innerhalb dieser Regel nicht waehlbar.
        return 0.0f;
    }

    // poolWeights des Styles. Fehlt der Pool in der Map, gilt 1 (neutral);
    // eine explizite 0 sperrt den Pool.
    const auto weight = style.poolWeight (poolId);

    if (weight < 0)
        return 1.0f;

    return (float) weight;
}

juce::String RandomizerEngine::choosePoolInRule (const StyleDef& style, const StyleRule& rule,
                                                 const juce::String& excludePoolId,
                                                 bool restrictTonality, Tonality tonality)
{
    std::vector<juce::String> ids;
    std::vector<float> weights;

    // Erster Versuch mit Ausschluss (Partner-Part soll moeglichst ein anderer
    // Pool sein), zweiter Versuch ohne - z. B. bei Ein-Pool-Gruppen wie
    // "1-1" auf ENS-SCZ, wo der Partner zwangslaeufig aus demselben Pool kommt.
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        ids.clear();
        weights.clear();

        const bool useExclusion = (attempt == 0);

        for (const auto& poolId : rule.pools)
        {
            if (useExclusion && excludePoolId.isNotEmpty() && poolId == excludePoolId)
                continue;

            const auto* pool = model.findPool (poolId);

            if (pool == nullptr)
                continue;

            if (restrictTonality && pool->tonality != tonality)
                continue;

            const auto weight = weightForPoolInRule (style, rule, poolId);

            if (weight <= 0.0f)
                continue;

            ids.push_back (poolId);
            weights.push_back (weight);
        }

        if (! ids.empty())
            break;

        if (excludePoolId.isEmpty() || restrictTonality)
            break;   // ein zweiter Versuch bringt hier nichts
    }

    if (ids.empty())
        return {};

    const auto index = pickWeighted (weights);
    return ids[(size_t) index];
}

// ---------------------------------------------------------------------------
// Teile sammeln
// ---------------------------------------------------------------------------

void RandomizerEngine::collectFromStyle (const StyleDef& style)
{
    for (int ruleIndex = 0; ruleIndex < (int) style.rules.size(); ++ruleIndex)
    {
        const auto& rule  = style.rules[(size_t) ruleIndex];
        const auto  tiers = parseRuleCode (rule.code, &warnings);

        if (tiers.empty())
            continue;

        juce::String previousPoolId;

        for (int slot = 0; slot < (int) tiers.size(); ++slot)
        {
            const int tier = tiers[(size_t) slot];

            // Tier-Wuerfel: faellt der Wurf aus, entsteht auch kein Partner.
            if (nextUnit() >= (double) probabilityForTier (tier))
                break;

            // Partner-Part (slot > 0) moeglichst aus einem anderen Pool.
            const auto poolId = choosePoolInRule (style, rule,
                                                  slot > 0 ? previousPoolId : juce::String(),
                                                  false, Tonality::neutral);

            if (poolId.isEmpty())
                break;

            const auto* pool = model.findPool (poolId);

            if (pool == nullptr)
                break;

            Part part;
            part.role      = pool->role;
            part.poolId    = poolId;
            part.tier      = tier;
            part.ruleIndex = ruleIndex;

            // Pegel: der ERSTE Part einer Regel kommt aus db, der ZWEITE aus
            // dbPair; fehlt dbPair, gilt fuer den Partner ebenfalls db.
            // Ist gar kein Bereich gesetzt, gilt der Pool-Startpegel.
            const auto& firstRange  = rule.db;
            const auto& secondRange = rule.dbPair.has_value() ? rule.dbPair : rule.db;

            if (slot == 0)
            {
                part.gainDb = firstRange.has_value()
                                  ? randomFloat (firstRange->first, firstRange->second)
                                  : gainToDb (pool->defaultGain);
            }
            else
            {
                part.gainDb = secondRange.has_value()
                                  ? randomFloat (secondRange->first, secondRange->second)
                                  : gainToDb (pool->defaultGain);
            }

            parts.push_back (std::move (part));
            previousPoolId = poolId;
        }
    }
}

void RandomizerEngine::collectFreeBuild()
{
    for (int i = 0; i < kNumFreeBuildLayers; ++i)
    {
        const auto& layer = kFreeBuildLayers[i];

        // Pflicht-Rollen werden immer gelegt, optionale nach Tier-Wuerfel.
        if (layer.minCount <= 0 && nextUnit() >= (double) probabilityForTier (layer.tier))
            continue;

        std::vector<juce::String> ids;
        std::vector<float> weights;

        for (const auto* pool : model.poolsForRole (layer.role))
        {
            if (pool == nullptr)
                continue;

            ids.push_back (pool->id);
            weights.push_back (1.0f);   // FREE BUILD: alle Pools gleichberechtigt
        }

        if (ids.empty())
        {
            warnings.add ("FREE BUILD: kein Pool fuer Rolle '" + roleToString (layer.role) + "'.");
            continue;
        }

        const int wanted = juce::jmax (1, layer.minCount);

        for (int n = 0; n < wanted; ++n)
        {
            const auto index = pickWeighted (weights);

            if (index < 0)
                break;

            const auto* pool = model.findPool (ids[(size_t) index]);

            if (pool == nullptr)
                break;

            Part part;
            part.role      = pool->role;
            part.poolId    = ids[(size_t) index];
            part.tier      = layer.tier;
            part.ruleIndex = -1;
            part.gainDb    = gainToDb (pool->defaultGain);

            parts.push_back (std::move (part));
        }
    }
}

// ---------------------------------------------------------------------------
// layerRules durchsetzen
// ---------------------------------------------------------------------------

int RandomizerEngine::countRole (Role role) const
{
    int count = 0;

    for (const auto& part : parts)
        if (part.role == role)
            ++count;

    return count;
}

int RandomizerEngine::minForRole (Role role) const
{
    if (currentStyle != nullptr)
        return currentStyle->minFor (role);

    for (int i = 0; i < kNumFreeBuildLayers; ++i)
        if (kFreeBuildLayers[i].role == role)
            return juce::jmax (0, kFreeBuildLayers[i].minCount);

    return 0;
}

int RandomizerEngine::maxForRole (Role role) const
{
    if (currentStyle != nullptr)
        return currentStyle->maxFor (role);

    for (int i = 0; i < kNumFreeBuildLayers; ++i)
        if (kFreeBuildLayers[i].role == role)
            return juce::jmax (0, kFreeBuildLayers[i].maxCount);

    return -1;   // unbegrenzt
}

bool RandomizerEngine::roleHasCapacity (Role role) const
{
    const auto cap = maxForRole (role);

    if (cap < 0)
        return true;

    return countRole (role) + 1 <= cap;
}

/**
 * Haengt einen Part einer Rolle an, der durch die Regeln NICHT gedeckt war.
 * Quelle sind die Regel-Pools dieser Rolle (in Regel-Reihenfolge, gewichtet),
 * danach Pools aus der pools-Map des Styles, die in keiner Regel stehen.
 */
bool RandomizerEngine::addPartForRole (Role role)
{
    std::vector<juce::String> ids;
    std::vector<float> weights;
    std::vector<int> tiers;
    std::vector<int> ruleIndices;

    auto consider = [&ids, &weights, &tiers, &ruleIndices] (const juce::String& poolId,
                                                            float weight, int tier, int ruleIndex)
    {
        if (weight <= 0.0f)
            return;

        for (const auto& existing : ids)
            if (existing == poolId)
                return;

        ids.push_back (poolId);
        weights.push_back (weight);
        tiers.push_back (tier);
        ruleIndices.push_back (ruleIndex);
    };

    if (currentStyle != nullptr)
    {
        for (int ruleIndex = 0; ruleIndex < (int) currentStyle->rules.size(); ++ruleIndex)
        {
            const auto& rule  = currentStyle->rules[(size_t) ruleIndex];
            const auto  codeTiers = parseRuleCode (rule.code, nullptr);
            const int   tier = codeTiers.empty() ? 1 : codeTiers.front();

            for (const auto& poolId : rule.pools)
            {
                const auto* pool = model.findPool (poolId);

                if (pool == nullptr || pool->role != role)
                    continue;

                consider (poolId, weightForPoolInRule (*currentStyle, rule, poolId), tier, ruleIndex);
            }
        }

        for (const auto& entry : currentStyle->poolWeights)
        {
            const auto* pool = model.findPool (entry.first);

            if (pool == nullptr || pool->role != role)
                continue;

            consider (entry.first, (float) juce::jmax (0, entry.second), 2, -1);
        }
    }
    else
    {
        for (const auto* pool : model.poolsForRole (role))
            if (pool != nullptr)
                consider (pool->id, 1.0f, 2, -1);
    }

    if (ids.empty())
    {
        warnings.add ("Keine Quelle fuer Rolle '" + roleToString (role) + "' -> Minimum bleibt offen.");
        return false;
    }

    const auto choice = pickWeighted (weights);

    if (choice < 0)
        return false;

    const auto* pool = model.findPool (ids[(size_t) choice]);

    if (pool == nullptr)
        return false;

    Part part;
    part.role      = role;
    part.poolId    = ids[(size_t) choice];
    part.tier      = tiers[(size_t) choice];
    part.ruleIndex = ruleIndices[(size_t) choice];

    if (currentStyle != nullptr && part.ruleIndex >= 0)
    {
        const auto& rule = currentStyle->rules[(size_t) part.ruleIndex];
        part.gainDb = rule.db.has_value()
                          ? randomFloat (rule.db->first, rule.db->second)
                          : gainToDb (pool->defaultGain);
    }
    else
    {
        part.gainDb = gainToDb (pool->defaultGain);
    }

    parts.push_back (std::move (part));
    return true;
}

void RandomizerEngine::enforceLayerRules()
{
    if (currentStyle == nullptr)
        return;

    // std::map<Role, LayerRule> ist nach Rollenwert sortiert -> deterministisch.
    for (const auto& entry : currentStyle->layerRules)
    {
        const Role role = entry.first;

        // 1) Minimum: fehlende Parts nachlegen (Regel-Tier wird ignoriert,
        //    weil das Minimum hart ist).
        int guard = 0;

        while (countRole (role) < juce::jmax (0, entry.second.minCount) && guard++ < 32)
            if (! addPartForRole (role))
                break;

        // 2) Maximum: Ueberhang entfernen, zuerst die am wenigsten
        //    wichtigen Parts (hoechstes Tier, spaeteste Position).
        const int cap = juce::jmax (0, entry.second.maxCount);

        while (countRole (role) > cap)
        {
            int worst = -1;
            int worstTier = -1;

            for (int i = 0; i < (int) parts.size(); ++i)
            {
                if (parts[(size_t) i].role != role)
                    continue;

                if (parts[(size_t) i].tier >= worstTier)
                {
                    worst     = i;
                    worstTier = parts[(size_t) i].tier;
                }
            }

            if (worst < 0)
                break;

            parts.erase (parts.begin() + worst);
        }
    }
}

void RandomizerEngine::enforceFreeBuildRules()
{
    for (int i = 0; i < kNumFreeBuildLayers; ++i)
    {
        const auto& layer = kFreeBuildLayers[i];
        const Role role = layer.role;

        while (countRole (role) < juce::jmax (0, layer.minCount))
            if (! addPartForRole (role))
                break;

        const int cap = juce::jmax (0, layer.maxCount);

        while (countRole (role) > cap)
        {
            int worst = -1;
            int worstTier = -1;

            for (int p = 0; p < (int) parts.size(); ++p)
            {
                if (parts[(size_t) p].role != role)
                    continue;

                if (parts[(size_t) p].tier >= worstTier)
                {
                    worst     = p;
                    worstTier = parts[(size_t) p].tier;
                }
            }

            if (worst < 0)
                break;

            parts.erase (parts.begin() + worst);
        }
    }
}

// ---------------------------------------------------------------------------
// Ausschlussregel "melodic x atonal"
// ---------------------------------------------------------------------------

Tonality RandomizerEngine::tonalityOf (const Part& part) const
{
    const auto* pool = model.findPool (part.poolId);
    return pool != nullptr ? pool->tonality : Tonality::neutral;
}

bool RandomizerEngine::hasTonality (Tonality tonality) const
{
    for (const auto& part : parts)
        if (tonalityOf (part) == tonality)
            return true;

    return false;
}

/**
 * Zaehlt die Parts einer Tonalitaet, die von ihrem Rollen-Minimum gefordert
 * sind (die ersten minCount Parts je Rolle in Roll-Reihenfolge).
 */
int RandomizerEngine::countRequiredTonality (Tonality tonality) const
{
    std::map<Role, int> seen;
    int required = 0;

    for (const auto& part : parts)
    {
        if (tonalityOf (part) != tonality)
            continue;

        const int index = seen[part.role]++;

        if (index < minForRole (part.role))
            ++required;
    }

    return required;
}

bool RandomizerEngine::isRedundantPart (size_t index) const
{
    if (index >= parts.size())
        return false;

    const auto role = parts[index].role;
    return countRole (role) > minForRole (role);
}

void RandomizerEngine::resolveExclusions()
{
    auto rules = model.exclusions();

    if (rules.empty())
        rules = fallbackExclusions();

    for (const auto& rule : rules)
    {
        if (rule.a == rule.b)
            continue;

        // Gewinnerseite wird EINMAL je Regel bestimmt; danach wird nur noch
        // die Verliererseite abgearbeitet. Damit kann die Entscheidung nicht
        // zwischen zwei Durchlaeufen hin- und herspringen.
        const Tonality keep = decideKeepTonality (rule);
        const Tonality lose = (keep == rule.a) ? rule.b : rule.a;

        int guard = 0;

        while (hasTonality (rule.a) && hasTonality (rule.b) && guard++ < 128)
            if (! fixOneLosingPart (keep, lose))
                break;

        if (hasTonality (rule.a) && hasTonality (rule.b))
            warnings.add ("Ausschlussregel '" + tonalityToString (rule.a) + " x "
                          + tonalityToString (rule.b) + "' nicht aufloesbar (Pflichtteile kollidieren).");
    }
}

Tonality RandomizerEngine::decideKeepTonality (const ExclusionRule& rule) const
{
    // Bevorzugt bleibt die Seite mit mehr Pflicht-Parts. Bei Gleichstand
    // bleibt die Tonalitaet des ersten Parts (deterministisch).
    const int requiredA = countRequiredTonality (rule.a);
    const int requiredB = countRequiredTonality (rule.b);

    if (requiredA != requiredB)
        return requiredB > requiredA ? rule.b : rule.a;

    for (const auto& part : parts)
    {
        const auto tonality = tonalityOf (part);

        if (tonality == rule.a || tonality == rule.b)
            return tonality;
    }

    return rule.a;
}

/**
 * Behandelt GENAU einen Part der Verliererseite: bevorzugt umstellen, sonst
 * entfernen (nur wenn die Rolle ihr Minimum danach noch haelt). Pflicht-Parts
 * ohne Ersatz bleiben stehen -> Praezedenz "Pflicht schlaegt Ausschluss".
 */
bool RandomizerEngine::fixOneLosingPart (Tonality keep, Tonality lose)
{
    // Rueckwaerts, damit die Indizes beim Entfernen gueltig bleiben.
    for (int i = (int) parts.size() - 1; i >= 0; --i)
    {
        if (tonalityOf (parts[(size_t) i]) != lose)
            continue;

        if (tryReplacePart ((size_t) i, keep))
            return true;

        if (isRedundantPart ((size_t) i))
        {
            parts.erase (parts.begin() + i);
            return true;
        }
    }

    return false;
}

/**
 * Ersetzt einen Part durch einen Pool der gewuenschten Tonalitaet.
 * Suchreihenfolge: gleiche Regel -> gleiche Rolle -> jede Rolle mit
 * freier Kapazitaet. Der Pegel bleibt erhalten (nur der Pool wechselt).
 */
bool RandomizerEngine::tryReplacePart (size_t index, Tonality wantedTonality)
{
    if (index >= parts.size())
        return false;

    const auto role = parts[index].role;
    const auto ruleIndex = parts[index].ruleIndex;

    auto roleAllowed = [this, role] (Role candidateRole)
    {
        if (candidateRole == role)
            return true;

        return roleHasCapacity (candidateRole);
    };

    std::vector<juce::String> ids;
    std::vector<float> weights;

    auto consider = [&] (const juce::String& poolId, float weight)
    {
        if (weight <= 0.0f)
            return;

        const auto* pool = model.findPool (poolId);

        if (pool == nullptr || pool->tonality != wantedTonality)
            return;

        if (! roleAllowed (pool->role))
            return;

        for (const auto& existing : ids)
            if (existing == poolId)
                return;

        ids.push_back (poolId);
        weights.push_back (weight);
    };

    // 1) Pools derselben Regel (z. B. rawstyle: melodic Crunch -> atonal Rumble)
    if (currentStyle != nullptr && ruleIndex >= 0 && ruleIndex < (int) currentStyle->rules.size())
    {
        const auto& rule = currentStyle->rules[(size_t) ruleIndex];

        for (const auto& poolId : rule.pools)
            consider (poolId, weightForPoolInRule (*currentStyle, rule, poolId));
    }

    // 2) alle Pools derselben Rolle
    for (const auto* pool : model.poolsForRole (role))
        if (pool != nullptr)
            consider (pool->id, 1.0f);

    // 3) jede Rolle, die noch Kapazitaet hat
    for (const auto& pool : model.pools())
        consider (pool.id, 1.0f);

    if (ids.empty())
        return false;

    const auto choice = pickWeighted (weights);

    if (choice < 0)
        return false;

    const auto* replacement = model.findPool (ids[(size_t) choice]);

    if (replacement == nullptr)
        return false;

    parts[index].poolId     = replacement->id;
    parts[index].role       = replacement->role;
    parts[index].samplePath = {};   // Sample wird spaeter neu gezogen

    warnings.add ("Ausschlussregel: Part auf Pool '" + replacement->id + "' umgestellt.");
    return true;
}

// ---------------------------------------------------------------------------
// Obergrenze
// ---------------------------------------------------------------------------

void RandomizerEngine::applySlotLimit (int slotLimit)
{
    // Die Summe der Rollen-Minima ist die Untergrenze: ein Kick ohne seinen
    // Pflicht-Punch waere kein Kick. Bei den Standardwerten (6/12 Slots) liegt
    // maxSlots immer ueber dieser Summe, der Deckel greift also normal.
    int sumMin = 0;

    for (int i = 0; i < kNumOrderedRoles; ++i)
        sumMin += minForRole (kRoleOrder[i]);

    const int limit = juce::jmax (slotLimit, sumMin);

    while ((int) parts.size() > limit)
    {
        int worst = -1;
        int worstTier = -1;

        for (int i = 0; i < (int) parts.size(); ++i)
        {
            if (! isRedundantPart ((size_t) i))
                continue;   // Pflicht-Part

            if (parts[(size_t) i].tier >= worstTier)
            {
                worst     = i;
                worstTier = parts[(size_t) i].tier;
            }
        }

        if (worst < 0)
            break;   // nur noch Pflicht-Parts -> Deckel wird ueberschritten

        parts.erase (parts.begin() + worst);
    }
}

// ---------------------------------------------------------------------------
// Samples
// ---------------------------------------------------------------------------

float RandomizerEngine::fileWeightFor (const PoolDef* pool, const juce::String& path) const
{
    if (pool == nullptr || pool->fileWeights.empty() || path.isEmpty())
        return 1.0f;

    const juce::File file (path);
    const auto fileName = file.getFileName();
    const auto baseName = file.getFileNameWithoutExtension();

    // exakter Treffer auf Dateiname oder Dateiname ohne Endung
    for (const auto& entry : pool->fileWeights)
    {
        if (entry.second <= 0.0f || entry.first.isEmpty())
            continue;

        if (entry.first.equalsIgnoreCase (fileName) || entry.first.equalsIgnoreCase (baseName))
            return entry.second;
    }

    // Praefix-Treffer: "Punch Heavy Raw" meint alle "Punch Heavy Raw ....wav"
    for (const auto& entry : pool->fileWeights)
    {
        if (entry.second <= 0.0f || entry.first.isEmpty())
            continue;

        if (fileName.startsWithIgnoreCase (entry.first))
            return entry.second;
    }

    return 1.0f;
}

void RandomizerEngine::resolveSamples()
{
    std::map<juce::String, int> poolUseCount;

    for (size_t i = 0; i < parts.size(); )
    {
        auto& part = parts[i];

        const auto* pool  = model.findPool (part.poolId);
        const auto  paths = resolver.samplePathsForPool (part.poolId);

        std::vector<juce::String> usable;
        std::vector<float> weights;
        int blockedCount = 0;

        for (const auto& path : paths)
        {
            if (path.isEmpty())
                continue;

            if (tracker != nullptr && tracker->isBlocked (part.poolId, path))
            {
                ++blockedCount;
                continue;
            }

            usable.push_back (path);
            weights.push_back (fileWeightFor (pool, path));
        }

        if (usable.empty() && blockedCount > 0)
        {
            // No-Repeat hat den Pool leergemacht: lieber wiederholen als stumm
            // bleiben. Der Tracker wird fuer diesen Part bewusst ignoriert.
            warnings.add ("No-Repeat: Pool '" + part.poolId + "' war leergelaufen, Wiederholung erlaubt.");

            for (const auto& path : paths)
            {
                if (path.isEmpty())
                    continue;

                usable.push_back (path);
                weights.push_back (fileWeightFor (pool, path));
            }
        }

        if (usable.empty())
        {
            warnings.add ("Pool '" + part.poolId + "' liefert kein Sample -> Part entfernt.");
            parts.erase (parts.begin() + (std::ptrdiff_t) i);
            continue;   // gleicher Index zeigt jetzt auf den naechsten Part
        }

        const auto choice = pickWeighted (weights);

        part.samplePath = (choice >= 0) ? usable[(size_t) choice] : usable.front();

        if (tracker != nullptr && part.samplePath.isNotEmpty())
            tracker->markUsed (part.poolId, part.samplePath);

        // duplicateGain: jede weitere Belegung desselben Pools im selben Kick
        // wird leiser (z. B. zwei Schranz-Enhancer mit 0.75 = -2.5 dB).
        int& poolUseCountForPart = poolUseCount[part.poolId];
        ++poolUseCountForPart;

        if (poolUseCountForPart > 1 && pool != nullptr && pool->duplicateGain != 1.0f)
            part.gainDb += gainToDb (pool->duplicateGain);

        ++i;
    }
}

// ---------------------------------------------------------------------------
// Abschluss
// ---------------------------------------------------------------------------

void RandomizerEngine::markMandatory()
{
    // Pflicht ist, was ein Rollen-Minimum braucht. Welche konkreten Parts das
    // sind, entscheidet das Tier: die wichtigsten (kleinstes Tier) zuerst.
    for (int i = 0; i < kNumOrderedRoles; ++i)
    {
        const Role role = kRoleOrder[i];

        std::vector<int> indices;

        for (int p = 0; p < (int) parts.size(); ++p)
            if (parts[(size_t) p].role == role)
                indices.push_back (p);

        std::stable_sort (indices.begin(), indices.end(),
                          [this] (int a, int b) { return parts[(size_t) a].tier < parts[(size_t) b].tier; });

        const int minCount = minForRole (role);

        for (int k = 0; k < (int) indices.size(); ++k)
            parts[(size_t) indices[(size_t) k]].optional = (k >= minCount);
    }

    // Alles, was nicht in der Rollenordnung steht, ist nie Pflicht.
    for (auto& part : parts)
        if (roleOrderIndex (part.role) >= kNumOrderedRoles)
            part.optional = true;
}

void RandomizerEngine::sortParts()
{
    std::stable_sort (parts.begin(), parts.end(),
                      [] (const Part& a, const Part& b)
                      {
                          return roleOrderIndex (a.role) < roleOrderIndex (b.role);
                      });
}

std::vector<RolledPart> RandomizerEngine::toResult() const
{
    std::vector<RolledPart> result;
    result.reserve (parts.size());

    for (const auto& part : parts)
    {
        RolledPart rolled;
        rolled.role       = part.role;
        rolled.poolId     = part.poolId;
        rolled.samplePath = part.samplePath;
        rolled.gainDb     = part.gainDb;
        rolled.optional   = part.optional;

        result.push_back (std::move (rolled));
    }

    return result;
}

// ---------------------------------------------------------------------------
// roll()
// ---------------------------------------------------------------------------

std::vector<RolledPart> RandomizerEngine::roll (const RollRequest& request)
{
    warnings.clear();
    parts.clear();

    rng.seed (request.seed);

    currentStyle = request.freeBuild ? nullptr : model.findStyle (request.styleId);

    if (! request.freeBuild && currentStyle == nullptr)
        warnings.add ("Unbekannter Style '" + request.styleId + "' -> FREE BUILD verwendet.");

    const int slotLimit = juce::jlimit (0, KickState::kNumSlots, request.maxSlots);

    // 1) Regel-Text -> Parts
    if (currentStyle != nullptr)
    {
        collectFromStyle (*currentStyle);
        // 2) Pflicht-/Optionalgrenzen der layerRules
        enforceLayerRules();
    }
    else
    {
        collectFreeBuild();
        enforceFreeBuildRules();
    }

    // 3) globale Ausschlussregel
    resolveExclusions();

    // 4) Obergrenze (erst nach dem Ausschluss: der kann Parts entfernen)
    applySlotLimit (slotLimit);

    // 5) Samples waehlen + No-Repeat + duplicateGain
    resolveSamples();

    // 6) Pflicht markieren, Reihenfolge fixieren
    markMandatory();
    sortParts();

    return toResult();
}

} // namespace bfka
