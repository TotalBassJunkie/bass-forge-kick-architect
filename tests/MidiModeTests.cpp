// B-123/B-124: Tests fuer MIDI-Modi, Note-On-Retry, All-Notes-Off und Key-Guete.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "engine/MidiModes.h"
#include "core/KickState.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

TEST_CASE ("MIDI: Modus-Index und Name", "[engine][midi]")
{
    REQUIRE (MidiModes::fromIndex (0) == MidiMode::poly);
    REQUIRE (MidiModes::fromIndex (9) == MidiMode::single);
    REQUIRE (MidiModes::fromIndex (-3) == MidiMode::poly);

    REQUIRE (MidiModes::nameFor (MidiMode::poly) == "POLY");
    REQUIRE (MidiModes::nameFor (MidiMode::mono) == "MONO");
    REQUIRE (MidiModes::nameFor (MidiMode::retrigger) == "RETRIGGER");
    REQUIRE (MidiModes::nameFor (MidiMode::single) == "SINGLE");

    REQUIRE (MidiModes::toIndex (MidiMode::retrigger) == 2);
    REQUIRE (MidiModes::toIndex (MidiMode::single) == 3);
}

TEST_CASE ("MIDI: SINGLE triggert nur die erste Note", "[engine][midi]")
{
    MidiTriggerState st;

    REQUIRE (MidiModes::shouldTrigger (MidiMode::single, 36, st));
    MidiModes::onNoteOn (MidiMode::single, 36, 100, st);

    REQUIRE (st.anyHeld);
    REQUIRE (st.heldCount == 1);
    REQUIRE (st.lastNote == 36);
    REQUIRE (st.lastVelocity == 100);

    REQUIRE_FALSE (MidiModes::shouldTrigger (MidiMode::single, 38, st));

    MidiModes::onNoteOff (MidiMode::single, 36, st);

    REQUIRE_FALSE (st.anyHeld);
    REQUIRE (st.heldCount == 0);
    REQUIRE (MidiModes::shouldTrigger (MidiMode::single, 38, st));
}

TEST_CASE ("MIDI: POLY triggert immer", "[engine][midi]")
{
    MidiTriggerState st;

    REQUIRE (MidiModes::shouldTrigger (MidiMode::poly, 36, st));

    MidiModes::onNoteOn (MidiMode::poly, 36, 90, st);
    REQUIRE (MidiModes::shouldTrigger (MidiMode::poly, 36, st));
    REQUIRE (MidiModes::shouldTrigger (MidiMode::poly, 38, st));

    // MONO und RETRIGGER triggern ebenfalls immer; die Auswahl macht der Aufrufer.
    REQUIRE (MidiModes::shouldTrigger (MidiMode::mono, 38, st));
    REQUIRE (MidiModes::shouldTrigger (MidiMode::retrigger, 38, st));
}

TEST_CASE ("MIDI: All-Notes-Off setzt zurueck", "[engine][midi]")
{
    MidiTriggerState st;
    MidiModes::onNoteOn (MidiMode::poly, 36, 111, st);
    REQUIRE (st.heldCount == 1);

    MidiModes::allNotesOff (st);

    REQUIRE_FALSE (st.anyHeld);
    REQUIRE (st.heldCount == 0);
    REQUIRE (st.lastNote == -1);
    REQUIRE (st.lastVelocity == 0);
    REQUIRE (st.highestHeld == -1);
    REQUIRE_FALSE (st.needsAllNotesOff);
}

TEST_CASE ("MIDI: Note-On-Retry ist begrenzt", "[engine][midi]")
{
    MidiTriggerState st;
    st.lastNote = 36;

    REQUIRE (MidiModes::nextRetry (st, 2) == 36);
    REQUIRE (st.retryCount == 1);
    REQUIRE (MidiModes::nextRetry (st, 2) == 36);
    REQUIRE (st.retryCount == 2);
    REQUIRE (MidiModes::nextRetry (st, 2) == -1);
    REQUIRE (st.retryCount == 2);

    // Ohne erlaubte Wiederholung kommt sofort -1.
    MidiTriggerState fresh;
    fresh.lastNote = 40;
    REQUIRE (MidiModes::nextRetry (fresh, 0) == -1);
}

TEST_CASE ("MIDI: Erkennungsguete", "[engine][midi]")
{
    REQUIRE_THAT (MidiModes::detectionQuality (9, 9), WithinAbs (1.0, 1e-9));
    REQUIRE_THAT (MidiModes::detectionQuality (9, 10), WithinAbs (1.0 - 1.0 / 6.0, 1e-6));
    REQUIRE_THAT (MidiModes::detectionQuality (-1, 9), WithinAbs (0.0, 1e-9));
    REQUIRE_THAT (MidiModes::detectionQuality (0, 6), WithinAbs (0.0, 1e-9));
    REQUIRE_THAT (MidiModes::detectionQuality (11, 0), WithinAbs (1.0 - 1.0 / 6.0, 1e-6));
    REQUIRE_THAT (MidiModes::detectionQuality (2, 8), WithinAbs (1.0 - 6.0 / 6.0, 1e-9));
}

TEST_CASE ("MIDI: Modus aus dem Zustand", "[engine][midi]")
{
    KickState s;

    s.setParam ("midi_mode", 0.75f);
    REQUIRE (MidiModes::fromState (s) == MidiMode::retrigger);

    s.setParam ("midi_mode", 0.0f);
    REQUIRE (MidiModes::fromState (s) == MidiMode::poly);

    s.setParam ("midi_mode", 1.0f);   // wird geklemmt
    REQUIRE (MidiModes::fromState (s) == MidiMode::single);
}
