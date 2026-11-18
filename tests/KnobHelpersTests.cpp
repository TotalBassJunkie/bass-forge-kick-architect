/*
 * B-87: Tests fuer den Regler-Komfort (KnobHelpers).
 *
 * Deckt Anzeige, Eingabe-Parsen, Shift/Ctrl-Skalierung, Zwischenablage und
 * das Anhaengen an einen echten juce::Slider ab.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/KnobHelpers.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };
} // namespace

TEST_CASE ("Knob: Anzeige mit Einheit", "[ui][knob]")
{
    KnobConfig c;
    c.unit     = "dB";
    c.decimals = 1;

    REQUIRE (KnobHelpers::formatValue (-3.25, c) == "-3.3 dB");

    c.unit = "";
    REQUIRE (KnobHelpers::formatValue (1.0, c) == "1.0");
}

TEST_CASE ("Knob: Eingabe parsen", "[ui][knob]")
{
    REQUIRE_THAT (KnobHelpers::parseValue ("2,5", 0.0, {}), WithinAbs (2.5, 1e-9));

    REQUIRE_THAT (KnobHelpers::parseValue ("-6 dB", 0.0, { 0.0, true, true, "dB", 1 }),
                  WithinAbs (-6.0, 1e-9));

    REQUIRE_THAT (KnobHelpers::parseValue ("abc", 7.0, {}), WithinAbs (7.0, 1e-9));
}

TEST_CASE ("Knob: Fein und grob", "[ui][knob]")
{
    REQUIRE_THAT (KnobHelpers::dragScale (true, false, {}), WithinAbs (0.1, 1e-9));
    REQUIRE_THAT (KnobHelpers::dragScale (false, true, {}), WithinAbs (5.0, 1e-9));
    REQUIRE_THAT (KnobHelpers::dragScale (false, false, {}), WithinAbs (1.0, 1e-9));

    // Shift gewinnt, wenn beide Tasten gedrueckt sind.
    REQUIRE_THAT (KnobHelpers::dragScale (true, true, {}), WithinAbs (0.1, 1e-9));

    // Abgeschaltete Flags lassen die Bewegung unveraendert.
    KnobConfig plain;
    plain.fineWithShift  = false;
    plain.coarseWithCtrl = false;
    REQUIRE_THAT (KnobHelpers::dragScale (true, true, plain), WithinAbs (1.0, 1e-9));
}

TEST_CASE ("Knob: Zwischenablage", "[ui][knob]")
{
    KnobHelpers::copyValue (-4.5);
    REQUIRE (KnobHelpers::hasCopiedValue());

    double v = 0.0;
    REQUIRE (KnobHelpers::pasteValue (v));
    REQUIRE_THAT (v, WithinAbs (-4.5, 1e-9));
}

TEST_CASE ("Knob: Reset setzt den Standard", "[ui][knob]")
{
    JuceGuiScope gui;

    juce::Slider s;
    s.setRange (0.0, 10.0, 0.0);
    s.setValue (5.0);

    KnobConfig c;
    c.defaultValue = 2.0;

    KnobHelpers h (s, c);
    h.resetToDefault();

    REQUIRE_THAT (s.getValue(), WithinAbs (2.0, 1e-9));
}

TEST_CASE ("Knob: Anhaengen setzt die Textfunktion", "[ui][knob]")
{
    JuceGuiScope gui;

    juce::Slider s;
    s.setRange (0.0, 10.0, 0.0);

    KnobConfig c;
    c.unit = "Hz";

    KnobHelpers h (s, c);

    REQUIRE (h.config().unit == "Hz");

    // JUCE 8.0.7 hat keinen Getter fuer die Funktion; das Feld selbst ist
    // oeffentlich, deshalb zusaetzlich die Wirkung am erzeugten Text.
    REQUIRE (s.textFromValueFunction != nullptr);
    REQUIRE (s.getTextFromValue (5.0) == "5.0 Hz");
}
