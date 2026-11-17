#pragma once

#include "core/KickState.h"

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace bfka
{

/**
 * Klangfarbe eines Pools.
 *
 * Die Tonalitaet steuert die globale Ausschlussregel ("melodic x atonal").
 * Sie kommt im Referenzformat direkt aus dem Ordnernamen: melodische Ordner
 * enden auf "(Melodic)", alles andere ist neutral oder bewusst atonal.
 */
enum class Tonality
{
    neutral = 0,   ///< tonal nicht festgelegt
    melodic,       ///< traegt einen Grundton (Key im Dateinamen)
    atonal         ///< bewusst ohne Grundton
};

/** Tonalitaet als Text ("neutral", "melodic", "atonal"). */
juce::String tonalityToString (Tonality tonality);
Tonality     tonalityFromString (const juce::String& text);

/** Linearer Pegel-Faktor -> dB. Werte <= 0 gelten als stumm (-100 dB). */
float gainToDb (float linearGain) noexcept;

/** dB -> linearer Pegel-Faktor. */
float dbToGain (float db) noexcept;

/**
 * Ein Sample-Pool.
 *
 * Ein Pool ist genau ein Ordner der Library. Die id ist der Schluessel, ueber
 * den Styles ihre Gewichte und Regeln zuordnen ("PUN-TEC").
 */
struct PoolDef
{
    juce::String id;        ///< "PUN-TEC"
    juce::String folder;    ///< "Main - Punches/Techno"
    Role         role     = Role::none;
    Tonality     tonality = Tonality::neutral;

    /// Startpegel als LINEARER Faktor (nicht dB). 0.7 entspricht ca. -3.1 dB.
    float defaultGain = 1.0f;

    /// Pegel-Faktor fuer die ZWEITE und jede weitere Belegung desselben Pools
    /// im selben Kick. 0.75 entspricht ca. -2.5 dB.
    float duplicateGain = 1.0f;

    /// Gewicht je Dateiname bzw. Dateinamen-Praefix; fehlt ein Treffer, gilt 1.0.
    std::map<juce::String, float> fileWeights;

    /// Redaktioneller Sortier-Hinweis (R33: nur die vier neuen Pools gesetzt).
    int uiOrder = 0;
};

/** Wie oft eine Rolle im Kick vorkommen darf. minCount > 0 = Pflichtrolle. */
struct LayerRule
{
    int minCount = 0;
    int maxCount = 0;
};

/** Prozentuale Verteilung innerhalb einer Regel. */
struct RuleShare
{
    std::vector<juce::String> pools;
    float percent = 0.0f;
};

/**
 * Eine Randomizer-Regel eines Styles.
 *
 * Die Semantik des code-Feldes ist in docs/specs/randomizer-rules.md
 * festgehalten und in RandomizerEngine.cpp kommentiert (B-71).
 */
struct StyleRule
{
    juce::String code;                    ///< "1" | "2" | "3" | "4" | "a-b"
    std::vector<juce::String> pools;
    std::optional<std::pair<float, float>> db;      ///< Pegel (dB) fuer den ERSTEN Part
    std::optional<std::pair<float, float>> dbPair;  ///< Pegel (dB) fuer den ZWEITEN Part
    std::vector<RuleShare> shares;
};

/** Ein kompletter Style (Factory-Style oder spaeter ein eigener). */
struct StyleDef
{
    juce::String id, display, shortName;
    juce::Colour accent;
    std::map<juce::String, int> poolWeights;
    std::map<Role, LayerRule>   layerRules;
    std::vector<StyleRule>      rules;
    std::vector<juce::String>   addable;
    juce::String                defaultPreset;

    bool hasLayerRule (Role role) const;
    int  minFor (Role role) const;    ///< 0, wenn keine Regel existiert
    int  maxFor (Role role) const;    ///< -1 = unbegrenzt (keine Regel)
    int  poolWeight (const juce::String& poolId) const;  ///< -1 = nicht im Style
};

/** Eine globale Ausschlussregel aus genres.json -> rules[]. */
struct ExclusionRule
{
    Tonality a = Tonality::neutral;
    Tonality b = Tonality::neutral;
    juce::String scope;   ///< "kick" = gilt fuer den ganzen Kick
};

/**
 * Das Style-/Pool-/Regel-Modell aus genres.json.
 *
 * Der Parser ist bewusst nachsichtig: unbekannte Felder werden ignoriert,
 * fehlende Felder mit Defaults gefuellt, und es wird nie eine Exception
 * geworfen. Strukturelle Fehler (kein JSON-Objekt, "pools"/"styles" fehlt)
 * liefern false und eine deutsche Fehlermeldung; das Modell bleibt dann
 * unveraendert. Weiche Auffaelligkeiten (unbekannte Rolle, doppelte id,
 * unlesbare Regel) landen in warnings().
 */
class StyleModel
{
public:
    StyleModel() = default;

    /** Parst den Text einer genres.json. */
    bool parseFromJson (const juce::String& jsonText, juce::String& error);

    /** Liest die Datei und ruft parseFromJson. */
    bool reloadFromFile (const juce::File& file, juce::String& error);

    bool isEmpty() const noexcept { return poolList.empty() && styleList.empty(); }

    const std::vector<juce::String>&    roleRegistry () const noexcept { return roleIds; }
    const std::vector<PoolDef>&         pools        () const noexcept { return poolList; }
    const std::vector<StyleDef>&        styles       () const noexcept { return styleList; }
    const std::vector<ExclusionRule>&   exclusions   () const noexcept { return exclusionRules; }

    /** nullptr, wenn die id unbekannt ist. */
    const PoolDef*  findPool  (const juce::String& poolId)  const;
    const StyleDef* findStyle (const juce::String& styleId) const;

    /** Alle Pools einer Rolle in Dateireihenfolge. */
    std::vector<const PoolDef*> poolsForRole (Role role) const;

    /**
     * Startpegel einer Rolle als LINEARER Faktor.
     * Mittelwert der defaultGain aller Pools dieser Rolle; 1.0 wenn es keinen
     * Pool gibt. Nur ein Notnagel fuer Aufrufer ohne konkreten Pool.
     */
    float defaultGainForRole (Role role) const;

    /** Nicht-fatale Hinweise des letzten Parse-Laufs. */
    const juce::StringArray& warnings() const noexcept { return warningList; }

private:
    std::vector<juce::String>  roleIds;
    std::vector<PoolDef>       poolList;
    std::vector<StyleDef>      styleList;
    std::vector<ExclusionRule> exclusionRules;
    juce::StringArray          warningList;
};

} // namespace bfka
