#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

#include "core/KickState.h"

// B-50/B-51: ALIGN - Gap-Analyse (Ende Anker <-> Beginn Tails) und MOVE CRUNCH.
// B-52/B-53: STRETCH PUNCH / STRETCH CRUNCH - Tails werden gestaucht statt
// verschoben; die Fades schrumpfen mit.
// B-54/B-55: Hand-Platzierung (pinned) gilt in beiden Pfaden; die Anker-Wahl
// bleibt unveraendert (isAnchor -> fruehester Punch -> fruehester Part).
//
// Reine Zahlenlogik auf der Kick-Achse: es wird kein Audio bewegt, sondern nur
// eine Verschiebung bzw. ein Streckfaktor je Part berechnet. Der Aufrufer setzt
// die Werte um.
// Nur Message-Thread; darf allokieren. Im Audio-Thread hat der Baustein nichts
// zu suchen (keine Allokation im Hot-Path).

namespace bfka
{

/** Zeitlage eines Parts auf der Kick-Achse (Ticket B-50). */
struct PartTiming
{
    double startSeconds  = 0.0;   ///< Beginn in der Kick-Achse
    double endSeconds    = 0.0;   ///< Ende (inkl. Fades)
    Role   role          = Role::none;
    bool   pinned        = false; ///< vom Nutzer gesetzt: NICHT verschieben
    bool   isAnchor      = false; ///< Anker des Kicks (Punch)
};

/** Ergebnis eines ALIGN-Laufs (Ticket B-50/B-51, B-52..B-55). */
struct AlignResult
{
    std::vector<double> offsetsSeconds;    ///< Verschiebung je Part (>0 = spaeter)
    std::vector<double> stretchFactors;    ///< 1.0 = unveraendert (nur STRETCH-Pfad)
    std::vector<double> fadeScaleFactors;  ///< 1.0 = Fades unveraendert (B-53)
    std::vector<double> gapsBeforeSeconds; ///< Luecke je Tail vor dem Lauf
    std::vector<double> gapsAfterSeconds;  ///< Luecke je Tail nach dem Lauf
    juce::StringArray   report;            ///< Klartextzeilen fuer LAST PASS
    bool   changed = false;
};

/**
 * ALIGN-Engine (Tickets B-50..B-55).
 *
 * Der Anker ist der Part mit `isAnchor`; fehlt einer, gilt der Punch-Part
 * mit dem fruehesten Start (sonst der frueheste Part ueberhaupt).
 * Tails (alles ausser dem Anker) werden so verschoben, dass ihr Beginn
 * hoechstens `toleranceSeconds` nach dem Ende des Ankers liegt.
 * `pinned`-Parts bleiben unangetastet.
 *
 * Toleranzen: TIGHT 0 ms, NORMAL 10 ms, GENTLE 25 ms.
 * Nur Message-Thread; darf allokieren.
 */
class AlignEngine
{
public:
    /** Toleranz in Sekunden fuer einen Modus. */
    static double toleranceFor (AlignMode mode) noexcept;

    /** Index des Ankers; -1, wenn die Liste leer ist. */
    static int findAnchor (const std::vector<PartTiming>& parts) noexcept;

    /** Fuehrt MOVE CRUNCH aus (B-51). */
    static AlignResult runMoveCrunch (const KickState& state,
                                      const std::vector<PartTiming>& parts);

    /**
     * STRETCH CRUNCH (B-52/B-53): Tails werden nicht verschoben, sondern
     * gestaucht, bis die Luecke hoechstens `tolerance` betraegt.
     * `stretchFactors[i] < 1` = gestaucht. Nur bei `keepPitch` sinnvoll.
     */
    static AlignResult runStretchCrunch (const KickState& state,
                                         const std::vector<PartTiming>& parts);

    /** true, wenn der Zustand STRETCH statt MOVE verlangt (`method == stretchPunch`). */
    static bool usesStretch (const KickState& state) noexcept;

    /** Waehlt die Methode aus dem Zustand und ruft die passende Funktion. */
    static AlignResult run (const KickState& state,
                            const std::vector<PartTiming>& parts);
};

} // namespace bfka
