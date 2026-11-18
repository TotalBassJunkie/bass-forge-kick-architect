// B-121/B-122: Tests fuer P-Flag-Prioritaet, Slot-Sortierung und KEY-Chip.

#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include "core/KickState.h"
#include "engine/KeyState.h"
#include "ui/KeyChip.h"

using namespace bfka;

namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Zeichnet eine Komponente einmal in ein Bild. */
    void renderOnce (juce::Component& component, int width, int height)
    {
        component.setSize (juce::jmax (1, width), juce::jmax (1, height));

        juce::Image image (juce::Image::ARGB, juce::jmax (1, width), juce::jmax (1, height), true);
        juce::Graphics g (image);
        component.paintEntireComponent (g, true);
    }
}

TEST_CASE ("Key: Chip-Text je Modus", "[ui][key]")
{
    KeyStateHolder s;

    s.setDetectedRoot (10);
    REQUIRE (s.chipText() == "A#");

    s.setMode (KeyChipMode::fixed);
    s.setFixedRoot (7);
    REQUIRE (s.chipText() == "FIX G");

    s.setMode (KeyChipMode::off);
    REQUIRE (s.chipText() == "X");
}

TEST_CASE ("Key: erkannt plus fest", "[ui][key]")
{
    KeyStateHolder s;
    s.setMode (KeyChipMode::autoDetect);
    s.setDetectedRoot (10);
    s.setFixedRoot (8);

    REQUIRE (s.chipText() == "A#+G#");
}

TEST_CASE ("Key: effectiveRoot folgt dem Modus", "[ui][key]")
{
    KeyStateHolder s;

    s.setMode (KeyChipMode::autoDetect);
    s.setDetectedRoot (3);
    REQUIRE (s.effectiveRoot() == 3);

    s.setMode (KeyChipMode::fixed);
    s.setFixedRoot (5);
    REQUIRE (s.effectiveRoot() == 5);

    s.setMode (KeyChipMode::off);
    REQUIRE (s.effectiveRoot() == -1);
}

TEST_CASE ("Key: P-Prioritaet", "[ui][key]")
{
    REQUIRE (KeyStateHolder::priorityForRole (Role::crunch) == 0);
    REQUIRE (KeyStateHolder::priorityForRole (Role::rumble) == 1);
    REQUIRE (KeyStateHolder::priorityForRole (Role::click) == 2);
}

TEST_CASE ("Key: Slot-Sortierung stellt Crunch voran", "[ui][key]")
{
    KickState state;
    state.setRole (0, Role::click);
    state.setRole (1, Role::rumble);
    state.setRole (2, Role::crunch);

    const auto order = KeyStateHolder::slotsByPriority (state);

    REQUIRE (order.size() == 12);
    REQUIRE (order[0] == 2);
    REQUIRE (order[1] == 1);
}

TEST_CASE ("Key: Chip rendert und schaltet weiter", "[ui][key]")
{
    JuceGuiScope gui;

    KeyStateHolder s;
    KeyChip chip (s);
    chip.setSize (96, 24);

    renderOnce (chip, 96, 24);
    REQUIRE (chip.preferredWidth() == 96);

    // Modus-Klemmung direkt ueber den Zustand (MouseEvent ist nicht baubar).
    s.setMode ((KeyChipMode) 7);
    REQUIRE (s.mode() == KeyChipMode::off);

    s.setMode ((KeyChipMode) -3);
    REQUIRE (s.mode() == KeyChipMode::autoDetect);
}
