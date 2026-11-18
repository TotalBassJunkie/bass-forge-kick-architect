#include "engine/KeyTracker.h"

// B-120: KEY TRACK - Noten zu Transposition, Oktav-Angleich der Parts.

namespace bfka
{

namespace
{
    /** Beliebige Ganzzahl auf eine Tonklasse 0..11 abbilden (auch negativ). */
    int pitchClass (int value) noexcept
    {
        return ((value % 12) + 12) % 12;
    }
}

int KeyTracker::shortestTransposition (int fromRoot, int toRoot) noexcept
{
    const int from = pitchClass (fromRoot);
    const int to   = pitchClass (toRoot);

    int delta = (to - from + 12) % 12;

    if (delta > 6)
        delta -= 12;

    return delta;
}

int KeyTracker::rootForNote (int midiNote) noexcept
{
    return pitchClass (midiNote);
}

int KeyTracker::octaveForNote (int midiNote) noexcept
{
    const int note = midiNote < 0 ? 0 : midiNote;

    return note / 12 - 1;
}

int KeyTracker::transpositionForPart (int partRoot, int playedNote, Role role) noexcept
{
    int value = shortestTransposition (partRoot, rootForNote (playedNote));

    // PUNCH und CLICK bleiben in ihrer Lage: sie tragen den Anschlag und
    // sollen nicht ueber die Oktave springen.
    const bool keepsRegister = (role == Role::punch || role == Role::click);

    if (! keepsRegister)
    {
        // Oktav-Angleich: die Lage so weit schieben, dass die gespielte Note
        // im spielbaren Fenster [24, 84] liegt. Die Schleifen sind auf acht
        // Durchlaeufe begrenzt, damit absurde Eingaben nicht haengen.
        for (int guard = 0; guard < 8 && playedNote + value < 24; ++guard)
            value += 12;

        for (int guard = 0; guard < 8 && playedNote + value > 84; ++guard)
            value -= 12;
    }

    return juce::jlimit (-24, 24, value);
}

int KeyTracker::applyToState (KickState& state, int playedNote, bool onlyTracked)
{
    int applied = 0;

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        if (onlyTracked && ! state.isKeyTracked (slot))
            continue;

        // Offener Punkt: der Grundton eines Parts (Sample-Key) steht noch nicht
        // im Zustand. Bis zur Anbindung der Sample-Keys wird C (0) als Referenz
        // genommen, damit die Funktion deterministisch bleibt.
        const int value = transpositionForPart (0, playedNote, state.getRole (slot));

        // Engineering-Wert (-24..+24 Halbtoene) auf 0..1 normieren.
        const float normalised = juce::jlimit (0.0f, 1.0f,
                                               (float) ((value + 24.0) / 48.0));

        state.setParam ("l" + juce::String (slot) + "_pitch", normalised);
        ++applied;
    }

    return applied;
}

int KeyTracker::applyToState (KickState& state, int playedNote)
{
    return applyToState (state, playedNote, false);
}

} // namespace bfka
