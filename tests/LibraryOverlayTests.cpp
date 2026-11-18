// B-100 - Tests fuer das Library-Overlay.
#include <catch2/catch_test_macros.hpp>

#include <array>

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/overlays/LibraryOverlay.h"

using bfka::LibraryItem;
using bfka::LibraryOverlay;
using Tab = bfka::LibraryOverlay::Tab;

namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };
} // namespace

TEST_CASE ("Library: Tabs und Filter", "[ui][library]")
{
    JuceGuiScope scope;
    LibraryOverlay o;

    o.setItems (Tab::presets, { { "Rumble 150", "punch+rumble", "a" },
                               { "Techno Kick", "punch", "b" } });
    o.setItems (Tab::sounds, { { "Punch 01", "PUN-TEC", "c" } });

    REQUIRE (o.tab() == Tab::presets);
    REQUIRE (o.visibleCount() == 2);

    o.setTab (Tab::sounds);
    REQUIRE (o.visibleCount() == 1);
}

TEST_CASE ("Library: Suche filtert", "[ui][library]")
{
    JuceGuiScope scope;
    LibraryOverlay o;

    o.setItems (Tab::presets, { { "Rumble 150", "punch+rumble", "a" },
                               { "Techno Kick", "punch", "b" } });

    o.setSearchText ("rumble");
    REQUIRE (o.visibleCount() == 1);
    REQUIRE (o.visibleName (0) == "Rumble 150");

    o.setSearchText ("");
    REQUIRE (o.visibleCount() == 2);
}

TEST_CASE ("Library: Vorschau folgt der Auswahl", "[ui][library]")
{
    JuceGuiScope scope;
    LibraryOverlay o;

    o.setItems (Tab::presets, { { "Rumble 150", "punch+rumble", "a" },
                               { "Techno Kick", "punch", "b" } });

    REQUIRE (o.previewText() == "punch+rumble");
}

TEST_CASE ("Library: Favorit umschalten", "[ui][library]")
{
    JuceGuiScope scope;
    LibraryOverlay o;

    o.setItems (Tab::presets, { { "Rumble 150", "punch+rumble", "a" },
                               { "Techno Kick", "punch", "b" } });

    REQUIRE_FALSE (o.isFavourite (0));
    o.toggleFavourite (0);
    REQUIRE (o.isFavourite (0));
    o.toggleFavourite (0);
    REQUIRE_FALSE (o.isFavourite (0));
}

TEST_CASE ("Library: Escape meldet Schliessen", "[ui][library]")
{
    JuceGuiScope scope;
    LibraryOverlay o;

    int closed = 0;
    o.onCloseRequested = [&] { ++closed; };

    REQUIRE (o.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE (closed == 1);
}

TEST_CASE ("Library: rendert", "[ui][library]")
{
    JuceGuiScope scope;
    LibraryOverlay o;

    o.setItems (Tab::presets, { { "Rumble 150", "punch+rumble", "a" } });
    o.setSize (900, 600);

    juce::Image image (juce::Image::ARGB, 900, 600, true);
    juce::Graphics g (image);

    o.paintEntireComponent (g, true);
}
