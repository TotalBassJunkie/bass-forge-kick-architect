// B-123/B-124: MIDI-Modi, Note-On-Retry, All-Notes-Off und Key-Guete.
//
// Alle Funktionen sind zustandslos bis auf den uebergebenen MidiTriggerState.
// Die Hoehen-Auswahl im Modus MONO macht der Aufrufer; hier wird nur gezaehlt.

#include "engine/MidiModes.h"

#include <algorithm>
#include <cmath>

namespace bfka
{

namespace
{
    /** Groesste sinnvolle Distanz auf dem Zirkel (Gegenpol). */
    constexpr int kMaxCircleDistance = 6;

    /** Klemmt einen Modus-Index auf den gueltigen Bereich. */
    int clampModeIndex (int index) noexcept
    {
        return juce::jlimit (0, MidiModes::kModeCount - 1, index);
    }

    /**
     * Normalisierter Parameter-Wert als Modus-Index; robust gegen NaN.
     *
     * "midi_mode" hat laut ParameterRegistry den Wertebereich 0..3, also
     * (kModeCount - 1) Stufen; 0.75 ergibt damit Index 2 (RETRIGGER).
     */
    int modeIndexFromNormalised (float normalised) noexcept
    {
        if (! std::isfinite (normalised))
            return 0;

        const auto clamped = juce::jlimit (0.0f, 1.0f, normalised);
        return clampModeIndex ((int) (clamped * (float) (MidiModes::kModeCount - 1)));
    }
}

MidiMode MidiModes::fromIndex (int index) noexcept
{
    return (MidiMode) clampModeIndex (index);
}

int MidiModes::toIndex (MidiMode mode) noexcept
{
    return clampModeIndex ((int) mode);
}

juce::String MidiModes::nameFor (MidiMode mode)
{
    switch (mode)
    {
        case MidiMode::poly:      return "POLY";
        case MidiMode::mono:      return "MONO";
        case MidiMode::retrigger: return "RETRIGGER";
        case MidiMode::single:    return "SINGLE";
    }

    // Unbekannter Wert: gleiche Voreinstellung wie beim Trigger-Verhalten.
    return "POLY";
}

MidiMode MidiModes::fromState (const KickState& state)
{
    const auto normalised = state.getParam ("midi_mode");
    return fromIndex (modeIndexFromNormalised (normalised));
}

bool MidiModes::shouldTrigger (MidiMode mode, int note, const MidiTriggerState& state) noexcept
{
    juce::ignoreUnused (note);

    switch (mode)
    {
        case MidiMode::single:
            // Nur die erste Note loest aus, bis wieder alle losgelassen sind.
            return ! state.anyHeld;

        case MidiMode::poly:
        case MidiMode::mono:
        case MidiMode::retrigger:
            return true;
    }

    // Ungueltiger Modus: im Zweifel ausloesen, damit nichts stumm bleibt.
    return true;
}

void MidiModes::onNoteOn (MidiMode mode, int note, int velocity, MidiTriggerState& state)
{
    juce::ignoreUnused (mode);

    state.lastNote     = note;
    state.lastVelocity = velocity;
    ++state.heldCount;
    state.anyHeld     = true;
    state.highestHeld = std::max (state.highestHeld, note);
    state.pendingNote = -1;
    state.retryCount  = 0;
}

void MidiModes::onNoteOff (MidiMode mode, int note, MidiTriggerState& state)
{
    juce::ignoreUnused (mode);

    state.heldCount = std::max (0, state.heldCount - 1);
    state.anyHeld   = state.heldCount > 0;

    if (state.highestHeld == note)
    {
        // Vereinfachung (dokumentiert): wir fuehren keine Notenliste. Solange
        // noch eine Note gehalten wird, bleibt der alte Hoehen-Wert stehen;
        // erst wenn nichts mehr gehalten wird, faellt er auf -1 zurueck.
        state.highestHeld = state.anyHeld ? state.highestHeld : -1;
    }
}

void MidiModes::allNotesOff (MidiTriggerState& state) noexcept
{
    state = MidiTriggerState{};
    state.needsAllNotesOff = false;
}

int MidiModes::nextRetry (MidiTriggerState& state, int maxRetries)
{
    if (state.retryCount >= maxRetries)
        return -1;

    ++state.retryCount;
    return state.lastNote;
}

double MidiModes::detectionQuality (int detectedRoot, int expectedRoot) noexcept
{
    if (detectedRoot < 0 || expectedRoot < 0)
        return 0.0;

    const int rawDistance = std::abs (detectedRoot - expectedRoot);
    const int distance    = std::min (rawDistance, 12 - rawDistance);

    const double quality = 1.0 - (double) distance / (double) kMaxCircleDistance;
    return juce::jlimit (0.0, 1.0, quality);
}

} // namespace bfka
