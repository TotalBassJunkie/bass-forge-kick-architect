// B-101 - Tests fuer Prefs (prefs.json) und das Settings-Overlay.

#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include "core/Prefs.h"
#include "ui/overlays/SettingsOverlay.h"

using namespace bfka;

namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Frisches Arbeitsverzeichnis je Test (wird am Ende geloescht). */
    struct TempDirectory
    {
        juce::File directory;

        TempDirectory()
        {
            directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("bfka-prefs-tests")
                            .getChildFile (juce::String (juce::Random::getSystemRandom().nextInt64()));

            directory.createDirectory();
        }

        ~TempDirectory()
        {
            if (directory.isDirectory())
                directory.deleteRecursively();
        }

        juce::File file (const juce::String& name) const
        {
            return directory.getChildFile (name);
        }
    };
} // namespace

TEST_CASE ("Prefs: acht Sektionen", "[core][prefs]")
{
    REQUIRE (Prefs::sections().size() == 8);
    REQUIRE (Prefs::sections()[0] == "Audio");
    CHECK (Prefs::sections()[7] == "Debug");
}

TEST_CASE ("Prefs: setzen, lesen, speichern", "[core][prefs]")
{
    TempDirectory temp;
    const auto file = temp.file ("prefs.json");

    Prefs p (file);
    p.set ("audio.sampleRate", 48000);

    REQUIRE (p.contains ("audio.sampleRate"));
    REQUIRE ((int) p.get ("audio.sampleRate", 0) == 48000);

    REQUIRE (p.save());
    REQUIRE (file.existsAsFile());

    Prefs q (file);
    REQUIRE (q.load());
    REQUIRE ((int) q.get ("audio.sampleRate", 0) == 48000);
}

TEST_CASE ("Prefs: fehlende Datei behaelt Defaults", "[core][prefs]")
{
    TempDirectory temp;
    Prefs p (temp.file ("gibtsnicht.json"));

    REQUIRE_FALSE (p.load());
    REQUIRE (p.get ("x", 7).equals (7));
    REQUIRE (p.size() == 0);
}

TEST_CASE ("Prefs: kaputte Datei", "[core][prefs]")
{
    TempDirectory temp;
    const auto file = temp.file ("kaputt.json");
    file.replaceWithText ("{{{");

    Prefs p (file);
    p.set ("audio.sampleRate", 44100);

    REQUIRE_FALSE (p.load());

    // Bestand bleibt unveraendert.
    REQUIRE ((int) p.get ("audio.sampleRate", 0) == 44100);
}

TEST_CASE ("Prefs: resetToDefaults", "[core][prefs]")
{
    Prefs p { juce::File() };
    p.set ("ui.accent", "orange");
    p.set ("audio.sampleRate", 48000);
    REQUIRE (p.size() == 2);

    p.resetToDefaults();
    REQUIRE (p.size() == 0);
    REQUIRE_FALSE (p.contains ("ui.accent"));
}

TEST_CASE ("Settings: Zeilen und Sektionen", "[ui][settings]")
{
    JuceGuiScope guiScope;
    TempDirectory temp;

    Prefs p (temp.file ("prefs.json"));
    SettingsOverlay o (p);

    o.setRows ({ { "Audio", "audio.sr",  "Sample Rate", "number" },
                 { "Audio", "audio.buf", "Buffer",      "number" },
                 { "MIDI",  "midi.in",  "Input",        "text" } });

    REQUIRE (o.rowCount() == 3);
    REQUIRE (o.rowCountForSection ("Audio") == 2);
    REQUIRE (o.rowCountForSection ("MIDI") == 1);
    REQUIRE (o.keyAt (0) == "audio.sr");
    REQUIRE (o.sectionAt (2) == "MIDI");

    o.setRowValue (0, 44100);
    REQUIRE ((int) o.rowValue (0) == 44100);
}

TEST_CASE ("Settings: rendert und Escape", "[ui][settings]")
{
    JuceGuiScope guiScope;

    Prefs p { juce::File() };
    SettingsOverlay o (p);

    o.setRows ({ { "Audio", "audio.sr", "Sample Rate", "number" } });
    o.setSize (800, 560);

    juce::Image image (juce::Image::ARGB, 800, 560, true);
    juce::Graphics g (image);
    o.paintEntireComponent (g, true);

    int closed = 0;
    o.onCloseRequested = [&] { ++closed; };

    REQUIRE (o.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE (closed == 1);
}
