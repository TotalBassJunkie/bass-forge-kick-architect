#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/UiScaler.h"

// B-89: Fenster-Resize, UIScaler und HiDPI (70..200 %).

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };
} // namespace

TEST_CASE ("Scaler: Klemmen und Raster", "[ui][scaler]")
{
    JuceGuiScope gui;

    UiScaler s;
    s.setPercent (45);
    REQUIRE (s.percent() == 70);

    s.setPercent (260);
    REQUIRE (s.percent() == 200);

    s.setPercent (134);
    REQUIRE (s.percent() == 130);
}

TEST_CASE ("Scaler: Faktor", "[ui][scaler]")
{
    JuceGuiScope gui;

    UiScaler s;
    s.setPercent (150);
    REQUIRE_THAT (s.scaleFactor(), WithinAbs (1.5, 1e-6));
}

TEST_CASE ("Scaler: Schrittweite", "[ui][scaler]")
{
    JuceGuiScope gui;

    UiScaler s;
    s.setPercent (100);
    REQUIRE (s.nextPercent() == 110);
    REQUIRE (s.previousPercent() == 90);

    s.setPercent (200);
    REQUIRE (s.nextPercent() == 200);

    s.setPercent (70);
    REQUIRE (s.previousPercent() == 70);
}

TEST_CASE ("Scaler: Fenstergroesse", "[ui][scaler]")
{
    JuceGuiScope gui;

    const auto b = UiScaler::windowBoundsFor (100);
    REQUIRE (b.getWidth() == 1180);
    REQUIRE (b.getHeight() == 760);

    const auto b2 = UiScaler::windowBoundsFor (200);
    REQUIRE (b2.getWidth() == 2360);
}

TEST_CASE ("Scaler: Lesbarkeit im erlaubten Bereich", "[ui][scaler]")
{
    JuceGuiScope gui;

    REQUIRE (UiScaler::isReadable (70));
    REQUIRE (UiScaler::isReadable (100));
    REQUIRE (UiScaler::isReadable (200));
    REQUIRE_FALSE (UiScaler::isReadable (50));
}

TEST_CASE ("Scaler: applyTo setzt Groesse und Transformation", "[ui][scaler]")
{
    JuceGuiScope gui;

    juce::Component root;
    UiScaler s;
    s.applyTo (root, 150);

    REQUIRE (root.getWidth() == 1770);
    REQUIRE (root.getHeight() == 1140);
    REQUIRE (root.getTransform().mat00 > 1.49f);
}
