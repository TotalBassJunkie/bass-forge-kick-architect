#include "library/StyleModel.h"

#include <algorithm>
#include <cmath>

namespace bfka
{

namespace
{
    // -----------------------------------------------------------------------
    // Kleine Lese-Helfer. Sie nehmen bewusst juce::var-Werte an, damit die
    // Array-Sicht (getArray) nicht auf einem sterbenden Tempor aer landet.
    // -----------------------------------------------------------------------

    juce::var getMember (const juce::var& object, const char* name)
    {
        if (const auto* obj = object.getDynamicObject())
            return obj->getProperty (juce::Identifier (name));

        return {};
    }

    juce::String stringMember (const juce::var& object, const char* name, const juce::String& fallback = {})
    {
        const auto value = getMember (object, name);

        if (value.isString())
            return value.toString();

        return fallback;
    }

    float asFloat (const juce::var& value, float fallback)
    {
        if (value.isDouble() || value.isInt() || value.isInt64())
            return (float) (double) value;

        if (value.isString())
        {
            const auto text = value.toString().trim();

            if (text.isNotEmpty())
                return text.getFloatValue();
        }

        return fallback;
    }

    int asInt (const juce::var& value, int fallback)
    {
        if (value.isDouble() || value.isInt() || value.isInt64())
            return (int) (double) value;

        if (value.isString())
        {
            const auto text = value.toString().trim();

            if (text.isNotEmpty())
                return text.getIntValue();
        }

        return fallback;
    }

    float floatMember (const juce::var& object, const char* name, float fallback)
    {
        return asFloat (getMember (object, name), fallback);
    }

    int intMember (const juce::var& object, const char* name, int fallback)
    {
        return asInt (getMember (object, name), fallback);
    }

    /** Linearer Pegel-Faktor: unbrauchbare Werte fallen auf 1.0 zurueck. */
    float sanitiseGain (float gain) noexcept
    {
        if (! std::isfinite (gain) || gain <= 0.0f)
            return 1.0f;

        return gain;
    }

    /** "#RRGGBB" (oder "#AARRGGBB") -> Colour. Alles andere faellt zurueck. */
    juce::Colour parseColour (const juce::String& text, juce::Colour fallback)
    {
        auto hex = text.trim().retainCharacters ("0123456789abcdefABCDEF");

        if (hex.length() == 6)
            hex = "FF" + hex;   // volle Deckkraft ergaenzen

        if (hex.length() != 8)
            return fallback;

        return juce::Colour ((juce::uint32) hex.getHexValue32());
    }

    /** Liest "db"/"dbPair": Array mit zwei Zahlen in dB. */
    std::optional<std::pair<float, float>> readRange (const juce::var& member)
    {
        const auto isNumber = [] (const juce::var& value)
        {
            return value.isDouble() || value.isInt() || value.isInt64();
        };

        if (const auto* array = member.getArray())
        {
            if (array->size() >= 2 && isNumber ((*array)[0]) && isNumber ((*array)[1]))
            {
                return std::make_pair (asFloat ((*array)[0], 0.0f),
                                       asFloat ((*array)[1], 0.0f));
            }
        }

        return std::nullopt;
    }

    /** Liest ein Array aus Strings (z. B. "pools" oder "addable"). */
    std::vector<juce::String> readStringArray (const juce::var& member)
    {
        std::vector<juce::String> result;

        if (const auto* array = member.getArray())
        {
            result.reserve ((size_t) array->size());

            for (const auto& entry : *array)
                if (entry.isString())
                    result.push_back (entry.toString());
        }

        return result;
    }

} // namespace

// ---------------------------------------------------------------------------
// Freie Helfer
// ---------------------------------------------------------------------------

juce::String tonalityToString (Tonality tonality)
{
    switch (tonality)
    {
        case Tonality::melodic: return "melodic";
        case Tonality::atonal:  return "atonal";
        case Tonality::neutral: break;
    }

    return "neutral";
}

Tonality tonalityFromString (const juce::String& text)
{
    const auto lower = text.trim().toLowerCase();

    if (lower == "melodic") return Tonality::melodic;
    if (lower == "atonal")  return Tonality::atonal;

    return Tonality::neutral;
}

float gainToDb (float linearGain) noexcept
{
    if (! (linearGain > 0.0f))
        return -100.0f;   // stumm

    return 20.0f * std::log10 (linearGain);
}

float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

// ---------------------------------------------------------------------------
// StyleDef
// ---------------------------------------------------------------------------

bool StyleDef::hasLayerRule (Role role) const
{
    return layerRules.find (role) != layerRules.end();
}

int StyleDef::minFor (Role role) const
{
    const auto it = layerRules.find (role);

    if (it == layerRules.end())
        return 0;

    return juce::jmax (0, it->second.minCount);
}

int StyleDef::maxFor (Role role) const
{
    const auto it = layerRules.find (role);

    if (it == layerRules.end())
        return -1;   // keine Regel -> unbegrenzt

    return juce::jmax (0, it->second.maxCount);
}

int StyleDef::poolWeight (const juce::String& poolId) const
{
    const auto it = poolWeights.find (poolId);

    if (it == poolWeights.end())
        return -1;

    return it->second;
}

// ---------------------------------------------------------------------------
// StyleModel
// ---------------------------------------------------------------------------

bool StyleModel::parseFromJson (const juce::String& jsonText, juce::String& error)
{
    error.clear();

    if (jsonText.trim().isEmpty())
    {
        error = "Eingabe ist leer.";
        return false;
    }

    juce::var root;
    const auto parseError = juce::JSON::parse (jsonText, root);

    if (! parseError.isVoid())
    {
        error = "JSON-Fehler: " + parseError.toString();
        return false;
    }

    if (root.getDynamicObject() == nullptr)
    {
        error = "Wurzel ist kein JSON-Objekt.";
        return false;
    }

    // Erst vollstaendig in temporaere Container parsen; bei einem Fehler
    // bleibt das bisherige Modell unveraendert ("nie halb geladen").
    juce::StringArray          newWarnings;
    std::vector<juce::String>  newRoleIds;
    std::vector<PoolDef>       newPools;
    std::vector<StyleDef>      newStyles;
    std::vector<ExclusionRule> newExclusions;

    // --- roleRegistry ------------------------------------------------------
    {
        const auto registry = getMember (root, "roleRegistry");

        if (const auto* obj = registry.getDynamicObject())
        {
            const auto& properties = obj->getProperties();
            newRoleIds.reserve ((size_t) properties.size());

            for (int i = 0; i < properties.size(); ++i)
                newRoleIds.push_back (properties.getName (i).toString());
        }
        else
        {
            newWarnings.add ("Kein roleRegistry gefunden.");
        }
    }

    // --- pools -------------------------------------------------------------
    {
        const auto poolsMember = getMember (root, "pools");
        const auto* poolArray = poolsMember.getArray();

        if (poolArray == nullptr)
        {
            error = "Feld 'pools' fehlt oder ist kein Array.";
            return false;
        }

        for (const auto& entry : *poolArray)
        {
            if (entry.getDynamicObject() == nullptr)
            {
                newWarnings.add ("Pool-Eintrag ohne Objekt uebersprungen.");
                continue;
            }

            PoolDef pool;
            pool.id = stringMember (entry, "id").trim();

            if (pool.id.isEmpty())
            {
                newWarnings.add ("Pool ohne id uebersprungen.");
                continue;
            }

            const bool alreadyThere = std::any_of (newPools.begin(), newPools.end(),
                                                   [&pool] (const PoolDef& other) { return other.id == pool.id; });

            if (alreadyThere)
            {
                newWarnings.add ("Doppelte Pool-id '" + pool.id + "' ignoriert.");
                continue;
            }

            pool.folder = stringMember (entry, "folder");

            const auto roleText = stringMember (entry, "role");
            pool.role = roleFromString (roleText);

            if (pool.role == Role::none && roleText.isNotEmpty())
                newWarnings.add ("Pool '" + pool.id + "': unbekannte Rolle '" + roleText + "'.");

            pool.tonality    = tonalityFromString (stringMember (entry, "tonality"));
            pool.defaultGain = sanitiseGain (floatMember (entry, "defaultGain", 1.0f));
            pool.duplicateGain = sanitiseGain (floatMember (entry, "duplicateGain", 1.0f));
            pool.uiOrder     = intMember (entry, "uiOrder", 0);

            const auto weightsMember = getMember (entry, "fileWeights");

            if (const auto* weightObj = weightsMember.getDynamicObject())
            {
                const auto& properties = weightObj->getProperties();

                for (int i = 0; i < properties.size(); ++i)
                {
                    const auto name  = properties.getName (i).toString();
                    const auto value = asFloat (properties.getValueAt (i), -1.0f);

                    if (name.isNotEmpty() && value > 0.0f)
                        pool.fileWeights[name] = value;
                }
            }

            newPools.push_back (std::move (pool));
        }
    }

    // --- styles ------------------------------------------------------------
    {
        const auto stylesMember = getMember (root, "styles");
        const auto* styleArray = stylesMember.getArray();

        if (styleArray == nullptr)
        {
            error = "Feld 'styles' fehlt oder ist kein Array.";
            return false;
        }

        for (const auto& entry : *styleArray)
        {
            if (entry.getDynamicObject() == nullptr)
            {
                newWarnings.add ("Style-Eintrag ohne Objekt uebersprungen.");
                continue;
            }

            StyleDef style;
            style.id = stringMember (entry, "id").trim();

            if (style.id.isEmpty())
            {
                newWarnings.add ("Style ohne id uebersprungen.");
                continue;
            }

            style.display   = stringMember (entry, "display", style.id);
            style.shortName = stringMember (entry, "short", style.display);
            style.accent    = parseColour (stringMember (entry, "accent"), juce::Colours::grey);
            style.defaultPreset = stringMember (entry, "defaultPreset");

            // pools: Map poolId -> Gewicht
            {
                const auto poolsOfStyle = getMember (entry, "pools");

                if (const auto* obj = poolsOfStyle.getDynamicObject())
                {
                    const auto& properties = obj->getProperties();

                    for (int i = 0; i < properties.size(); ++i)
                    {
                        const auto poolId = properties.getName (i).toString();
                        const auto weight = asInt (properties.getValueAt (i), 1);

                        style.poolWeights[poolId] = weight;
                    }
                }
                else
                {
                    newWarnings.add ("Style '" + style.id + "' hat keine pools-Map.");
                }
            }

            // layerRules: Rolle -> { min, max }
            {
                const auto layerMember = getMember (entry, "layerRules");

                if (const auto* obj = layerMember.getDynamicObject())
                {
                    const auto& properties = obj->getProperties();

                    for (int i = 0; i < properties.size(); ++i)
                    {
                        const auto roleName = properties.getName (i).toString();
                        const auto role     = roleFromString (roleName);

                        if (role == Role::none)
                        {
                            newWarnings.add ("Style '" + style.id + "': unbekannte Rolle '" + roleName + "' in layerRules.");
                            continue;
                        }

                        const auto layerEntry = properties.getValueAt (i);

                        LayerRule layer;
                        layer.minCount = juce::jmax (0, intMember (layerEntry, "min", 0));
                        layer.maxCount = juce::jmax (layer.minCount, intMember (layerEntry, "max", layer.minCount));

                        style.layerRules[role] = layer;
                    }
                }
            }

            // rules
            {
                const auto rulesMember = getMember (entry, "rules");
                const auto* ruleArray  = rulesMember.getArray();

                if (ruleArray != nullptr)
                {
                    for (const auto& ruleEntry : *ruleArray)
                    {
                        if (ruleEntry.getDynamicObject() == nullptr)
                        {
                            newWarnings.add ("Style '" + style.id + "': Regel ohne Objekt uebersprungen.");
                            continue;
                        }

                        StyleRule rule;
                        rule.code  = stringMember (ruleEntry, "code", "1");
                        rule.pools = readStringArray (getMember (ruleEntry, "pools"));

                        if (rule.pools.empty())
                        {
                            newWarnings.add ("Style '" + style.id + "': Regel ohne Pools uebersprungen.");
                            continue;
                        }

                        rule.db     = readRange (getMember (ruleEntry, "db"));
                        rule.dbPair = readRange (getMember (ruleEntry, "dbPair"));

                        const auto sharesMember = getMember (ruleEntry, "shares");

                        if (const auto* shareArray = sharesMember.getArray())
                        {
                            for (const auto& shareEntry : *shareArray)
                            {
                                if (shareEntry.getDynamicObject() == nullptr)
                                    continue;

                                RuleShare share;
                                share.pools   = readStringArray (getMember (shareEntry, "pools"));
                                share.percent = floatMember (shareEntry, "pct", 0.0f);

                                if (! share.pools.empty() && share.percent > 0.0f)
                                    rule.shares.push_back (std::move (share));
                            }
                        }

                        style.rules.push_back (std::move (rule));
                    }
                }
                else
                {
                    newWarnings.add ("Style '" + style.id + "' hat keine rules-Liste.");
                }
            }

            style.addable = readStringArray (getMember (entry, "addable"));

            newStyles.push_back (std::move (style));
        }
    }

    if (newStyles.empty())
        newWarnings.add ("Es wurden keine Styles definiert.");

    if (newPools.empty())
        newWarnings.add ("Es wurden keine Pools definiert.");

    // --- globale Regeln (nur "exclude" ist bekannt) ------------------------
    {
        const auto rulesMember = getMember (root, "rules");
        const auto* ruleArray  = rulesMember.getArray();

        if (ruleArray != nullptr)
        {
            for (const auto& entry : *ruleArray)
            {
                if (entry.getDynamicObject() == nullptr)
                    continue;

                const auto type = stringMember (entry, "type").trim().toLowerCase();

                if (type != "exclude")
                {
                    newWarnings.add ("Unbekannter globaler Regeltyp '" + type + "' ignoriert.");
                    continue;
                }

                ExclusionRule rule;
                rule.a = tonalityFromString (stringMember (entry, "a"));
                rule.b = tonalityFromString (stringMember (entry, "b"));
                rule.scope = stringMember (entry, "scope", "kick");

                if (rule.a == rule.b || rule.a == Tonality::neutral || rule.b == Tonality::neutral)
                {
                    newWarnings.add ("Ausschlussregel ohne zwei verschiedene Tonalitaeten ignoriert.");
                    continue;
                }

                newExclusions.push_back (std::move (rule));
            }
        }
    }

    // Ab hier ist der Lauf erfolgreich -> umschalten.
    roleIds        = std::move (newRoleIds);
    poolList       = std::move (newPools);
    styleList      = std::move (newStyles);
    exclusionRules = std::move (newExclusions);
    warningList    = std::move (newWarnings);

    return true;
}

bool StyleModel::reloadFromFile (const juce::File& file, juce::String& error)
{
    error.clear();

    if (! file.existsAsFile())
    {
        error = "Datei nicht gefunden: " + file.getFullPathName();
        return false;
    }

    const auto text = file.loadFileAsString();

    if (text.trim().isEmpty())
    {
        error = "Datei ist leer: " + file.getFullPathName();
        return false;
    }

    if (! parseFromJson (text, error))
    {
        error = file.getFileName() + ": " + error;
        return false;
    }

    return true;
}

const PoolDef* StyleModel::findPool (const juce::String& poolId) const
{
    for (const auto& pool : poolList)
        if (pool.id == poolId)
            return &pool;

    return nullptr;
}

const StyleDef* StyleModel::findStyle (const juce::String& styleId) const
{
    for (const auto& style : styleList)
        if (style.id == styleId)
            return &style;

    return nullptr;
}

std::vector<const PoolDef*> StyleModel::poolsForRole (Role role) const
{
    std::vector<const PoolDef*> result;

    for (const auto& pool : poolList)
        if (pool.role == role)
            result.push_back (&pool);

    return result;
}

float StyleModel::defaultGainForRole (Role role) const
{
    float sum   = 0.0f;
    int   count = 0;

    for (const auto& pool : poolList)
    {
        if (pool.role != role)
            continue;

        sum += pool.defaultGain;
        ++count;
    }

    if (count <= 0)
        return 1.0f;

    return sum / (float) count;
}

} // namespace bfka
