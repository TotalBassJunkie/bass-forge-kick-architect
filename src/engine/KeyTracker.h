#pragma once

// B-120: KEY TRACK - Noten zu Transposition, Oktav-Angleich der Parts.

#include "core/KickState.h"

namespace bfka
{

/**
 * KEY TRACK (Ticket B-120): rechnet aus der gespielten MIDI-Note die
 * Transposition fuer jeden Part.
 *
 * Regel: Jeder Part wird so verschoben, dass sein eigener Grundton auf dem
 * gespielten Grundton liegt. Der Weg wird als kuerzeste Verschiebung
 * gewaehlt (Ergebnis immer in [-6, +6] Halbtoenen); die Oktave wird
 * angeglichen, damit Rumble-Parts nicht in unspielbare Lagen rutschen.
 */
class KeyTracker
{
public:
    /** Kuerzeste Transposition von `fromRoot` nach `toRoot` in Halbtoenen (-6..+6). */
    static int shortestTransposition (int fromRoot, int toRoot) noexcept;

    /** MIDI-Note -> Grundton 0..11 (C=0). */
    static int rootForNote (int midiNote) noexcept;

    /** MIDI-Note -> Oktave (60 = 4). */
    static int octaveForNote (int midiNote) noexcept;

    /** Transposition fuer einen Part mit `partRoot`, inkl. Oktav-Angleich. */
    static int transpositionForPart (int partRoot, int playedNote, Role role) noexcept;

    /** Setzt `lN_pitch` aller KEY-TRACK-Parts passend zur gespielten Note. */
    static int applyToState (KickState& state, int playedNote);

    /** Wie oben, aber nur fuer Slots mit `isKeyTracked() == true`. */
    static int applyToState (KickState& state, int playedNote, bool onlyTracked);
};

} // namespace bfka
