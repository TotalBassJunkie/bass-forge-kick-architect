#pragma once

#include "core/KickState.h"
#include "library/StyleModel.h"

#include <cstdint>
#include <random>
#include <vector>

namespace bfka
{

/** Ein Roll-Auftrag. */
struct RollRequest
{
    juce::String styleId;                    ///< Style-id aus genres.json
    bool freeBuild = false;                  ///< FREE BUILD: ohne Style-Regeln
    int  maxSlots  = KickState::kNumSlots;   ///< Obergrenze der Parts (max. 12)
    std::uint64_t seed = 0;                  ///< einzige Zufallsquelle
};

/** Ein gerollter Part = eine zu belegende Slot-Karte. */
struct RolledPart
{
    Role         role = Role::none;
    juce::String poolId;
    juce::String samplePath;
    float        gainDb = 0.0f;
    bool         optional = false;   ///< true = nicht durch layerRules-min gefordert
};

/**
 * Liefert die Sample-Pfade eines Pools.
 *
 * Die Engine kennt keine Dateisystem-Layouts; die Library-Schicht
 * (SampleLibrary/Scanner) implementiert diese Schnittstelle.
 */
class PoolResolver
{
public:
    virtual ~PoolResolver() = default;

    /** Alle Sample-Pfade eines Pools; leer, wenn der Pool unbekannt ist. */
    virtual std::vector<juce::String> samplePathsForPool (const juce::String& poolId) const = 0;
};

/**
 * Merkt sich bereits verwendete Samples je Pool (No-Repeat, B-73).
 *
 * Die Engine fragt vor der Auswahl isBlocked() und markiert das gewaehlte
 * Sample danach mit markUsed(). Ein Tracker ist optional.
 */
class NoRepeatTracker
{
public:
    virtual ~NoRepeatTracker() = default;

    virtual bool isBlocked (const juce::String& poolId, const juce::String& samplePath) const = 0;
    virtual void markUsed  (const juce::String& poolId, const juce::String& samplePath) = 0;
};

/**
 * Regel-Engine des Style-Randomizers (B-70/B-71).
 *
 * Ein roll() ist vollstaendig deterministisch: gleiche Anfrage (inkl. seed)
 * und gleiches Modell liefern exakt dasselbe Ergebnis. Die gewaehlte Semantik
 * der Felder code/db/dbPair/shares/layerRules/poolWeights ist in
 * docs/specs/randomizer-rules.md dokumentiert und in der .cpp an jeder
 * Entscheidungsstelle kommentiert.
 */
class RandomizerEngine
{
public:
    RandomizerEngine (const StyleModel& modelToUse, PoolResolver& resolverToUse);

    /** Optionaler No-Repeat-Tracker (nicht besitzend, darf nullptr sein). */
    void setNoRepeatTracker (NoRepeatTracker* newTracker) noexcept { tracker = newTracker; }

    /** Rollt einen Kick. Wirft nie; Probleme landen in lastWarnings(). */
    std::vector<RolledPart> roll (const RollRequest& request);

    /** Wahrscheinlichkeit eines code-Tiers (1..4). Ausserhalb: 0. */
    static float probabilityForTier (int tier) noexcept;

    /** Nicht-fatale Hinweise des letzten roll(). */
    const juce::StringArray& lastWarnings() const noexcept { return warnings; }

private:
    /** Interner Part waehrend des Rolls (noch ohne RolledPart-Projektion). */
    struct Part
    {
        Role         role = Role::none;
        juce::String poolId;
        float        gainDb = 0.0f;
        int          tier = 1;        ///< 1/3 = immer, 2/4 = optional
        int          ruleIndex = -1;  ///< -1 = nicht aus einer Regel
        bool         optional = false;
        juce::String samplePath;
    };

    // --- Zufall (deterministisch, ohne std-Distributionen) -----------------
    double nextUnit();                       ///< [0, 1)
    float  randomFloat (float lo, float hi);
    int    pickWeighted (const std::vector<float>& weights);   ///< -1 bei leer
    int    pickUniform (int count);

    // --- Regel-/Pool-Auswahl ----------------------------------------------
    static std::vector<int> parseRuleCode (const juce::String& code, juce::StringArray* warningsOut);

    float weightForPoolInRule (const StyleDef& style, const StyleRule& rule,
                               const juce::String& poolId) const;

    juce::String choosePoolInRule (const StyleDef& style, const StyleRule& rule,
                                   const juce::String& excludePoolId,
                                   bool restrictTonality, Tonality tonality);

    void collectFromStyle (const StyleDef& style);
    void collectFreeBuild();

    void enforceLayerRules();
    void enforceFreeBuildRules();

    bool addPartForRole (Role role);

    // --- Ausschlussregel ---------------------------------------------------
    void resolveExclusions();
    Tonality decideKeepTonality (const ExclusionRule& rule) const;
    bool fixOneLosingPart (Tonality keep, Tonality lose);
    bool tryReplacePart (size_t index, Tonality wantedTonality);

    // --- Grenzen, Pegel, Markierung ---------------------------------------
    void applySlotLimit (int slotLimit);
    void resolveSamples();
    void markMandatory();
    void sortParts();

    bool  isRedundantPart (size_t index) const;
    int   countRole (Role role) const;
    int   countRequiredTonality (Tonality tonality) const;
    Tonality tonalityOf (const Part& part) const;
    bool  hasTonality (Tonality tonality) const;
    bool  roleHasCapacity (Role role) const;
    int   minForRole (Role role) const;
    int   maxForRole (Role role) const;   ///< -1 = unbegrenzt
    float fileWeightFor (const PoolDef* pool, const juce::String& path) const;

    std::vector<RolledPart> toResult() const;

    const StyleModel& model;
    PoolResolver&     resolver;
    NoRepeatTracker*  tracker = nullptr;

    std::mt19937_64   rng;
    juce::StringArray warnings;

    const StyleDef*  currentStyle = nullptr;
    std::vector<Part> parts;
};

} // namespace bfka
