#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

#include "ui/overlays/DialogHost.h"

using namespace bfka;

// Der Overlay-Host stapelt Overlays im Editor-Fenster. Kein Modal-Loop:
// Escape schliesst, Klicks bleiben im Fenster. Die Tests pruefen die
// Stapelverwaltung und einen Zeichenlauf ohne Absturz.
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Minimales Overlay, das nur eine Flaeche fuellt. */
    struct DummyOverlay : juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::red);
        }
    };
} // namespace

TEST_CASE ("Overlay: push/pop verwaltet den Stapel", "[ui][overlay]")
{
    JuceGuiScope gui;

    OverlayHost h;
    REQUIRE_FALSE (h.isOpen());

    h.push (std::make_unique<DummyOverlay>());
    REQUIRE (h.isOpen());
    REQUIRE (h.depth() == 1);

    h.push (std::make_unique<DummyOverlay>());
    REQUIRE (h.depth() == 2);

    h.pop();
    REQUIRE (h.depth() == 1);

    h.pop();
    REQUIRE_FALSE (h.isOpen());
}

TEST_CASE ("Overlay: Escape schliesst das oberste", "[ui][overlay]")
{
    JuceGuiScope gui;

    OverlayHost h;
    h.push (std::make_unique<DummyOverlay>());
    h.push (std::make_unique<DummyOverlay>());
    REQUIRE (h.depth() == 2);

    REQUIRE (h.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE (h.depth() == 1);
}

TEST_CASE ("Overlay: onAllClosed feuert einmal", "[ui][overlay]")
{
    JuceGuiScope gui;

    OverlayHost h;
    int closed = 0;
    h.onAllClosed = [&closed] { ++closed; };

    h.push (std::make_unique<DummyOverlay>());
    h.pop();
    REQUIRE (closed == 1);

    // Zweites pop auf leerem Stapel darf nicht erneut melden.
    h.pop();
    REQUIRE (closed == 1);
}

TEST_CASE ("Overlay: nullptr wird ignoriert", "[ui][overlay]")
{
    JuceGuiScope gui;

    OverlayHost h;
    h.push (nullptr);
    REQUIRE_FALSE (h.isOpen());
}

TEST_CASE ("Overlay: rendert ohne Absturz", "[ui][overlay]")
{
    JuceGuiScope gui;

    OverlayHost h;
    h.push (std::make_unique<DummyOverlay>());
    h.setBoundsToParent ({ 0, 0, 800, 600 });

    juce::Image image (juce::Image::ARGB, 800, 600, true);
    juce::Graphics g (image);

    h.paintEntireComponent (g, true);

    REQUIRE (h.depth() == 1);
}
