#pragma once
// B-121/B-122: Tonart-Zustand des Kicks, Chip-Text und P-Flag-Prioritaet.

#include <juce_core/juce_core.h>

#include <vector>

#include "core/KickState.h"

namespace bfka
{

/** Anzeige-Modus des KEY-Chips (Ticket B-122). */
enum class KeyChipMode
{
    autoDetect = 0,   ///< Grundton kommt aus der Analyse
    fixed      = 1,   ///< vom Nutzer festgelegt
    off        = 2    ///< "X": keine Tonart
};

/** Notenname zu einem Grundton 0..11 (C=0). Ausserhalb: "" bzw. "?" je Aufrufer. */
const char* noteName (int keyNote) noexcept;

/**
 * Tonart-Zustand des Kicks (B-122).
 * Traegt den Anzeigemodus, den festen Grundton und die erkannte Tonart.
 */
class KeyStateHolder
{
public:
    void setMode (KeyChipMode newMode) noexcept;
    KeyChipMode mode() const noexcept;

    /** Fester Grundton 0..11 (C=0), nur bei `fixed` wirksam. */
    void setFixedRoot (int keyNote) noexcept;
    int  fixedRoot() const noexcept;

    /** Ergebnis der Analyse (0..11, -1 = unbekannt). */
    void setDetectedRoot (int keyNote) noexcept;
    int  detectedRoot() const noexcept;

    /** Wirksamer Grundton je nach Modus; -1 bei `off`. */
    int effectiveRoot() const noexcept;

    /** Chip-Text: "AUTO A#", "FIX G", "X", "A#+G#" (erkannt + fest). */
    juce::String chipText() const;

    /** Prioritaet eines Parts nach dem P-Flag (B-121): kleiner = wichtiger. */
    static int priorityForRole (Role role) noexcept;   // Crunch 0, Rumble 1, Rest 2

    /** Sortiert Slot-Indizes nach P-Prioritaet (stabil, Crunch zuerst). */
    static std::vector<int> slotsByPriority (const KickState& state);

private:
    KeyChipMode chipMode     = KeyChipMode::autoDetect;
    int         fixedNote    = -1;
    int         detectedNote = -1;
};

} // namespace bfka
