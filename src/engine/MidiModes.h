#pragma once
// B-123/B-124: MIDI-Modi, Note-On-Retry, All-Notes-Off und Guete der Key-Erkennung.
//
// Reine Logik ohne Audio: der Baustein entscheidet nur, ob ein Note-On einen
// Kick ausloest, und fuehrt den Halte-Zustand mit. Kein Allokieren, keine
// Sperren, aufrufbar aus dem Audio- und dem Message-Thread.

#include <juce_core/juce_core.h>

#include "core/KickState.h"

namespace bfka
{

/** Verhalten bei mehreren gleichzeitigen Noten (Ticket B-123). */
enum class MidiMode
{
    poly = 0,        ///< jede Note loest aus
    mono,            ///< die hoechste gehaltene Note loest aus
    retrigger,       ///< Note-On auf derselben Note loest erneut aus
    single           ///< nur die erste Note loest aus, bis alle losgelassen sind
};

/** Laufzeitzustand des MIDI-Modus (Ticket B-123). */
struct MidiTriggerState
{
    int  lastNote       = -1;
    int  lastVelocity   = 0;
    bool anyHeld        = false;
    int  heldCount      = 0;
    int  highestHeld    = -1;
    int  retryCount     = 0;   ///< Note-On-Wiederholungen ohne Wirkung
    int  pendingNote    = -1;  ///< Note, die auf ein Retry wartet
    bool needsAllNotesOff = false;
};

/** MIDI-Modi des Plugins (Tickets B-123/B-124). */
class MidiModes
{
public:
    static constexpr int kModeCount = 4;

    /** Index aus dem Zustand, geklemmt auf 0..3. */
    static MidiMode fromIndex (int index) noexcept;
    static int       toIndex (MidiMode mode) noexcept;
    static juce::String nameFor (MidiMode mode);

    /** Text aus dem Zustand ("midi_mode" ist ein Index 0..3). */
    static MidiMode fromState (const KickState& state);

    /** true, wenn dieser Note-On einen Kick ausloest. */
    static bool shouldTrigger (MidiMode mode, int note, const MidiTriggerState& state) noexcept;

    static void onNoteOn (MidiMode mode, int note, int velocity, MidiTriggerState& state);
    static void onNoteOff (MidiMode mode, int note, MidiTriggerState& state);

    /** All-Notes-Off: setzt den Zustand zurueck und meldet Handlungsbedarf. */
    static void allNotesOff (MidiTriggerState& state) noexcept;

    /**
     * Note-On-Retry (B-123): wenn ein Trigger nicht gewirkt hat, wird die
     * Note bis zu `maxRetries` mal erneut angeboten.
     * Rueckgabe: Note, die erneut gesendet werden soll (-1 = keine).
     */
    static int nextRetry (MidiTriggerState& state, int maxRetries);

    /** Guete der Key-Erkennung (B-124): 1.0 = passt, 0.0 = daneben. */
    static double detectionQuality (int detectedRoot, int expectedRoot) noexcept;
};

} // namespace bfka
