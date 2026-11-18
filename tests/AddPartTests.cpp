// B-103: Tests fuer den zweistufigen "+ ADD PART"-Picker.
// B-104: Tests fuer das "+ ADD STYLE"-Overlay.

#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/overlays/AddPartOverlay.h"

namespace
{
    /** JUCE-GUI-Initialisierung fuer Tests, die Komponenten anlegen. */
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Zwei-Gruppen-Aufbau fuer die meisten Faelle (PUNCH hat Platz, RUMBLE nicht). */
    inline std::vector<bfka::AddPartGroup> twoGroups()
    {
        return { { "PUNCH",  { { "p1", "Punch Techno", "PU", 2 } } },
                 { "RUMBLE", { { "r1", "Rumble Deep",  "RU", 0 } } } };
    }
} // namespace

TEST_CASE ("AddPart: zweistufig", "[ui][addpart]")
{
    JuceGuiScope gui;

    bfka::AddPartOverlay o;
    o.setGroups (twoGroups());

    REQUIRE (o.groupCount() == 2);
    REQUIRE (o.stage() == 0);

    o.selectGroup (0);
    REQUIRE (o.stage() == 1);
    REQUIRE (o.selectedGroup() == 0);
    REQUIRE (o.itemCount() == 1);
    REQUIRE (o.itemName (0) == "Punch Techno");

    o.goBack();
    REQUIRE (o.stage() == 0);
    REQUIRE (o.selectedGroup() == -1);
}

TEST_CASE ("AddPart: ohne freien Slot wird nichts gewaehlt", "[ui][addpart]")
{
    JuceGuiScope gui;

    bfka::AddPartOverlay o;
    o.setGroups (twoGroups());
    o.selectGroup (0);
    o.setFreeSlots (0);

    REQUIRE (o.showsNoFreeSlot());

    juce::String id;
    REQUIRE_FALSE (o.chooseItem (0, id));
    REQUIRE (id.isEmpty());
}

TEST_CASE ("AddPart: mit freiem Slot wird gewaehlt", "[ui][addpart]")
{
    JuceGuiScope gui;

    bfka::AddPartOverlay o;
    o.setGroups (twoGroups());
    o.selectGroup (0);
    o.setFreeSlots (2);

    juce::String id;
    REQUIRE (o.chooseItem (0, id));
    REQUIRE (id == "p1");
    REQUIRE (o.freeSlots() == 2);
}

TEST_CASE ("AddPart: Eintrag ohne eigenen Platz wird abgelehnt", "[ui][addpart]")
{
    JuceGuiScope gui;

    bfka::AddPartOverlay o;
    o.setGroups (twoGroups());
    o.selectGroup (1);          // RUMBLE, dessen einziges Item freeSlots == 0 hat
    o.setFreeSlots (2);

    juce::String id;
    REQUIRE_FALSE (o.chooseItem (0, id));
    REQUIRE (id.isEmpty());
}

TEST_CASE ("AddPart: Escape geht eine Stufe zurueck", "[ui][addpart]")
{
    JuceGuiScope gui;

    bfka::AddPartOverlay o;
    o.setGroups ({ { "PUNCH", { { "p1", "Punch Techno", "PU", 2 } } } });

    bool closeRequested = false;
    o.onCloseRequested = [&closeRequested] { closeRequested = true; };

    o.selectGroup (0);
    REQUIRE (o.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE (o.stage() == 0);
    REQUIRE_FALSE (closeRequested);

    // Stufe 0: Escape schliesst.
    REQUIRE (o.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE (closeRequested);
}

TEST_CASE ("AddPart: rendert", "[ui][addpart]")
{
    JuceGuiScope gui;

    bfka::AddPartOverlay o;
    o.setGroups ({ { "PUNCH",  { { "p1", "Punch Techno", "PU", 2 } } },
                   { "RUMBLE", { { "r1", "Rumble Deep",  "RU", 0 } } } });
    o.setSize (560, 420);
    o.setFreeSlots (0);         // zeichnet "NO FREE SLOT"

    juce::Image img (juce::Image::ARGB, 560, 420, true);
    juce::Graphics g (img);
    o.paintEntireComponent (g, true);

    // Stufe 2 ebenfalls zeichnen.
    o.selectGroup (0);
    o.paintEntireComponent (g, true);

    REQUIRE (o.getWidth() == 560);
    REQUIRE (o.showsNoFreeSlot());
}

TEST_CASE ("AddStyle: id aus Namen", "[ui][addstyle]")
{
    JuceGuiScope gui;

    bfka::AddStyleOverlay s;
    s.setBaseStyles ({ "rawstyle" });
    s.setName ("Mein Stil 2");
    s.setBaseStyle ("rawstyle");

    REQUIRE (s.canCreate());

    juce::String id, err;
    REQUIRE (s.create (id, err));
    REQUIRE (id == "user-mein-stil-2");
    REQUIRE (err.isEmpty());
}

TEST_CASE ("AddStyle: ohne Namen kein Anlegen", "[ui][addstyle]")
{
    JuceGuiScope gui;

    bfka::AddStyleOverlay s;
    s.setBaseStyles ({ "rawstyle" });
    s.setBaseStyle ("rawstyle");

    juce::String id, err;
    REQUIRE_FALSE (s.canCreate());
    REQUIRE_FALSE (s.create (id, err));
    REQUIRE (err.isNotEmpty());
}
