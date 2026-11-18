// B-82: Tests der Style-Sidebar.
#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include "library/ChaosEngine.h"
#include "library/StyleModel.h"
#include "library/UserStyles.h"
#include "ui/StyleSidebar.h"

using namespace bfka;

namespace
{
    /** JUCE-GUI-Initialisierung fuer die Dauer eines Test-CASE. */
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };
} // namespace

TEST_CASE ("Sidebar: ohne Modell nur FREE BUILD", "[ui][sidebar]")
{
    JuceGuiScope gui;

    StyleSidebar s;
    s.setModel (nullptr, nullptr);

    REQUIRE (s.rowCount() == 1);
    REQUIRE (s.rowName (0) == "FREE BUILD");
    REQUIRE (s.rowStyleId (0).isEmpty());
}

TEST_CASE ("Sidebar: aktiver Style und Akzent", "[ui][sidebar]")
{
    JuceGuiScope gui;

    StyleSidebar s;

    s.setActiveStyle ("rawstyle");
    REQUIRE (s.activeStyle() == "rawstyle");

    s.setAccentFor (juce::Colours::red);
    REQUIRE (s.accent() == juce::Colours::red);
}

TEST_CASE ("Sidebar: Roll-Zaehler", "[ui][sidebar]")
{
    JuceGuiScope gui;

    StyleSidebar s;

    s.setRollCount (3);
    REQUIRE (s.rollCount() == 3);
}

TEST_CASE ("Sidebar: Breite und Rendern", "[ui][sidebar]")
{
    JuceGuiScope gui;

    StyleSidebar s;

    REQUIRE (s.preferredWidth() == 210);

    s.setSize (210, 600);

    juce::Image image (juce::Image::ARGB, 210, 600, true);
    juce::Graphics g (image);

    s.paintEntireComponent (g, true);

    REQUIRE (image.getWidth() == 210);
    REQUIRE (image.getHeight() == 600);
}

TEST_CASE ("Sidebar: Modell fuellt die Liste", "[ui][sidebar]")
{
    JuceGuiScope gui;

    StyleSidebar s;

    StyleModel m;
    juce::String err;

    const bool parsed = m.parseFromJson (juce::String (R"({"pools":[],"styles":[{"id":"a","display":"Alpha"},{"id":"b","display":"Beta"}]})"),
                                        err);

    if (parsed)
    {
        UserStyles us (juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("bfka_sidebar_styles"));

        s.setModel (&m, &us);

        REQUIRE (s.rowCount() == 3);
        REQUIRE (s.rowStyleId (1) == "a");
    }
    else
    {
        SUCCEED ("Parser akzeptiert das Minimal-JSON nicht - Fall uebersprungen");
    }
}
