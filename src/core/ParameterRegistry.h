#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace bfka
{

/**
 * Kurvenform zwischen Anzeigewert (engineering) und gespeichertem Wert (0..1).

 * Der gespeicherte Zustand ist IMMER normalisiert (0..1). Das entspricht dem
 * Referenzformat und macht Host-Automation unabhaengig von der Einheit.
 * Die Messwerte der Kurven werden im Rahmen von Ticket B-11 ermittelt; bis
 * dahin gelten dokumentierte Arbeitsannahmen.
 */
enum class ParamCurve
{
    linear,       ///< geradlinig
    decibel,      ///< dB-Werte, geradlinig ueber den dB-Bereich
    frequency,    ///< Hz, logarithmisch
    semitones,    ///< Halbtoene, geradlinig
    stretch,      ///< Streckfaktor 0.25 .. 8, logarithmisch
    percent,      ///< 0..100 %
    boolean       ///< 0/1, keine Interpolation
};

/** Bereich, in dem ein Parameter lebt. */
enum class ParamDomain
{
    slot,    ///< 12 Slots
    global,  ///< Kick-Globals
    fx       ///< FX-Slots (8 x 14)
};

/** Beschreibung eines einzelnen Parameters. */
struct ParamDesc
{
    juce::String id;            ///< kanonische ID, z. B. "l0_vol"
    juce::String displayName;   ///< Anzeigename in der UI
    ParamDomain  domain = ParamDomain::global;
    int          slot   = -1;   ///< Slot-Index (domain == slot: 0..11, fx: 0..7)
    int          index  = 0;    ///< laufende Nummer innerhalb der Domain
    ParamCurve   curve  = ParamCurve::linear;

    float engMin = 0.0f;
    float engMax = 1.0f;
    float engDefault = 0.0f;
    int   decimals = 2;
    juce::String unit;

    /** 0..1 -> Anzeigewert. */
    float toEngineering (float normalised) const noexcept;

    /** Anzeigewert -> 0..1 (geklemmt). */
    float toNormalised (float engineering) const noexcept;

    /** Default als normalisierter Wert. */
    float normalisedDefault() const noexcept { return toNormalised (engDefault); }

    /** Formatierter Anzeigewert, z. B. "+3,00 dB". */
    juce::String format (float normalised) const;
};

/**
 * Die vollstaendige Parametertabelle.

 * Enthaelt je Slot 19 Parameter (exakt die IDs des Referenzformats), 12 Globals
 * und 8 x 14 FX-Proxy-Parameter. Die Tabelle ist die einzige Quelle der Wahrheit
 * fuer IDs, Bereiche und Defaults.
 */
class ParameterRegistry
{
public:
    static const ParameterRegistry& get();

    const std::vector<ParamDesc>& all() const noexcept { return params; }

    /** nullptr, wenn die ID unbekannt ist. */
    const ParamDesc* find (const juce::String& id) const noexcept;

    /** Alle Parameter eines Slots (domain == slot). */
    std::vector<const ParamDesc*> forSlot (int slotIndex) const;

    /** Anzahl Parameter je Domain. */
    int numSlotParams()   const noexcept { return 19; }
    int numGlobalParams() const noexcept { return 12; }
    int numFxParams()     const noexcept { return 14; }
    int numSlots()        const noexcept { return 12; }
    int numFxSlots()      const noexcept { return 8; }

    /** Anzahl aller Parameter (Index im KickState). */
    int size() const noexcept { return (int) params.size(); }

private:
    ParameterRegistry();
    std::vector<ParamDesc> params;
    std::vector<int> slotIndexStart; // Startindex je Slot in `params`
};

} // namespace bfka
