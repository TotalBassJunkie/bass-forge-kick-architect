/*
 * Datei: tests/PresetManagerTests.cpp
 * Ticket: B-102 -- Tests fuer PresetManager und PresetManagerOverlay.
 *
 * Die Tests arbeiten auf einem temporaeren Ordner mit leeren
 * `.bfkapreset`-Dateien; der Manager liest nur Namen, keine Inhalte.
 */
#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include "ui/overlays/PresetManagerOverlay.h"
#include "core/PresetIO.h"

using namespace bfka;

// Preset-Manager und Overlay (B-102).
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
                            .getChildFile ("bfka-manager-tests")
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

    /** Ordner mit drei leeren Presets; Inhalt ist fuer den Manager egal. */
    juce::File makePresetFolder (const TempDirectory& temp)
    {
        for (const auto* name : { "Alpha", "Beta", "Gamma" })
            temp.file (juce::String (name) + ".bfkapreset").replaceWithText ("preset");

        return temp.directory;
    }

    /** Prueft, ob ein Eintrag mit diesem Namen in der Liste steht. */
    bool hasEntry (const PresetManager& manager, const juce::String& name)
    {
        for (const auto& entry : manager.entries())
            if (entry.name == name)
                return true;

        return false;
    }
} // namespace

TEST_CASE ("Preset-Manager: scan findet Dateien", "[ui][manager]")
{
    TempDirectory temp;
    PresetManager m (makePresetFolder (temp));

    REQUIRE (m.scan() == 3);
    CHECK (m.size() == 3);

    // Nach Namen sortiert und nicht als Papierkorb markiert.
    CHECK (m.entries()[0].name == "Alpha");
    CHECK (m.entries()[2].name == "Gamma");

    for (const auto& entry : m.entries())
    {
        CHECK_FALSE (entry.isTrash);
        CHECK (entry.file.existsAsFile());
    }
}

TEST_CASE ("Preset-Manager: umbenennen", "[ui][manager]")
{
    TempDirectory temp;
    PresetManager m (makePresetFolder (temp));

    REQUIRE (m.scan() == 3);

    juce::String error;
    REQUIRE (m.rename (0, "Neu", error));
    INFO (error);

    REQUIRE (m.scan() == 3);
    CHECK (temp.file ("Neu.bfkapreset").existsAsFile());
    CHECK_FALSE (temp.file ("Alpha.bfkapreset").existsAsFile());
    CHECK (hasEntry (m, "Neu"));
}

TEST_CASE ("Preset-Manager: Kollision beim Umbenennen", "[ui][manager]")
{
    TempDirectory temp;
    PresetManager m (makePresetFolder (temp));

    REQUIRE (m.scan() == 3);

    juce::String error;
    REQUIRE_FALSE (m.rename (0, "Beta", error));
    CHECK (error.isNotEmpty());

    // Nichts wurde angefasst.
    REQUIRE (m.scan() == 3);
    CHECK (temp.file ("Alpha.bfkapreset").existsAsFile());
    CHECK (temp.file ("Beta.bfkapreset").existsAsFile());
}

TEST_CASE ("Preset-Manager: duplizieren", "[ui][manager]")
{
    TempDirectory temp;
    PresetManager m (makePresetFolder (temp));

    REQUIRE (m.scan() == 3);

    juce::String error;
    REQUIRE (m.duplicate (0, error));
    INFO (error);

    REQUIRE (m.scan() == 4);

    bool foundCopy = false;

    for (const auto& entry : m.entries())
        if (entry.name.contains ("_copy"))
            foundCopy = true;

    CHECK (foundCopy);

    // Zweite Kopie bekommt _copy2, keine Kollision.
    REQUIRE (m.duplicate (0, error));
    INFO (error);
    REQUIRE (m.scan() == 5);
    CHECK (temp.file ("Alpha_copy2.bfkapreset").existsAsFile());
}

TEST_CASE ("Preset-Manager: Papierkorb statt Loeschen", "[ui][manager]")
{
    TempDirectory temp;
    PresetManager m (makePresetFolder (temp));

    REQUIRE (m.scan() == 3);
    CHECK (m.trashCount() == 0);

    juce::String error;
    REQUIRE (m.moveToTrash (0, error));
    INFO (error);

    REQUIRE (m.scan() == 2);
    REQUIRE (m.trashCount() == 1);

    // Die Datei lebt im Papierkorb weiter -- nichts ist geloescht.
    CHECK (m.trashDirectory().isDirectory());
    CHECK (m.trashDirectory() == temp.directory.getChildFile ("_Papierkorb"));
    CHECK_FALSE (temp.file ("Alpha.bfkapreset").existsAsFile());

    juce::Array<juce::File> trashFiles;
    m.trashDirectory().findChildFiles (trashFiles, juce::File::findFiles, false, "*.bfkapreset");
    REQUIRE (trashFiles.size() == 1);
    CHECK (trashFiles[0].existsAsFile());
}

TEST_CASE ("Preset-Manager: ungueltiger Index", "[ui][manager]")
{
    TempDirectory temp;
    PresetManager m (makePresetFolder (temp));

    REQUIRE (m.scan() == 3);

    juce::String error;
    REQUIRE_FALSE (m.rename (99, "x", error));
    CHECK (error.isNotEmpty());

    error.clear();
    REQUIRE_FALSE (m.rename (-1, "x", error));
    CHECK (error.isNotEmpty());

    error.clear();
    REQUIRE_FALSE (m.duplicate (99, error));
    CHECK (error.isNotEmpty());

    error.clear();
    REQUIRE_FALSE (m.moveToTrash (99, error));
    CHECK (error.isNotEmpty());

    // Bestand unveraendert.
    REQUIRE (m.scan() == 3);
    CHECK (m.trashCount() == 0);
}

TEST_CASE ("Preset-Overlay: Zeilen und Auswahl", "[ui][manager]")
{
    JuceGuiScope guiScope;

    TempDirectory temp;
    PresetManager m (makePresetFolder (temp));

    PresetManagerOverlay o (m);

    REQUIRE (o.rowCount() == 3);
    CHECK (o.selectedRow() == 0);

    o.selectRow (1);
    REQUIRE (o.selectedRow() == 1);
    REQUIRE (o.rowName (1).isNotEmpty());
    CHECK (o.rowName (1) == "Beta");
    CHECK (o.rowName (99).isEmpty());

    // Auswahl wird geklemmt, nicht verworfen.
    o.selectRow (500);
    CHECK (o.selectedRow() == 2);
    o.selectRow (-5);
    CHECK (o.selectedRow() == 0);

    o.selectRow (1);
    juce::String error;
    REQUIRE (o.duplicateSelected (error));
    INFO (error);
    CHECK (o.rowCount() == 4);
}

TEST_CASE ("Preset-Overlay: rendert und Escape", "[ui][manager]")
{
    JuceGuiScope guiScope;

    TempDirectory temp;
    PresetManager m (makePresetFolder (temp));

    PresetManagerOverlay o (m);
    o.setSize (760, 520);

    juce::Image image (juce::Image::ARGB, 760, 520, true);
    juce::Graphics g (image);
    o.paint (g);

    int closed = 0;
    o.onCloseRequested = [&closed] { ++closed; };

    CHECK (o.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (closed == 1);

    // Andere Tasten schliessen nicht.
    CHECK_FALSE (o.keyPressed (juce::KeyPress ('x')));
    CHECK (closed == 1);

    // Schliessen-Knopf existiert und ist verdrahtet.
    o.onCloseRequested();
    CHECK (closed == 2);
}
