#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include "core/KickState.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "ui/KickPage.h"
#include "ui/overlays/DialogHost.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Integration der Nacht-Wellen: die neuen Bedienelemente haengen wirklich in
// der KICK-Seite und im Editor (B-82, B-110, B-122, B-100/B-101/B-102, B-105).
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    void renderOnce (juce::Component& component, int width, int height)
    {
        component.setSize (width, height);

        juce::Image image (juce::Image::ARGB, juce::jmax (1, width), juce::jmax (1, height), true);
        juce::Graphics g (image);

        component.paintEntireComponent (g, true);
    }
} // namespace

TEST_CASE ("KICK-Seite: Sidebar, KEY-Chip und DRAG sind einghaengt", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    KickPage page (proc);

    REQUIRE (page.styleSidebar() != nullptr);
    REQUIRE (page.keyChipView() != nullptr);
    REQUIRE (page.styleSidebar()->preferredWidth() == 210);

    // Bei 1180 px Breite muss die Sidebar sichtbar bleiben und der
    // Kartenbereich mindestens 480 px behalten.
    renderOnce (page, 1180, 720);
    REQUIRE (page.styleSidebar()->isVisible());
    REQUIRE (page.styleSidebar()->getWidth() == 210);
}

TEST_CASE ("KICK-Seite: KEY-Chip folgt dem Zustand", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    KickPage page (proc);

    page.keyState().setDetectedRoot (9);
    renderOnce (page, 1180, 720);
    REQUIRE (page.keyChipView()->isVisible());
    REQUIRE (page.keyState().chipText() == "A");
}

TEST_CASE ("KICK-Seite: Export schreibt eine WAV-Datei", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    KickPage page (proc);

    juce::String path, error;
    const bool ok = page.exportKick (path, error);

    REQUIRE (ok);
    REQUIRE (error.isEmpty());
    REQUIRE (path.endsWithIgnoreCase (".wav"));

    const juce::File written (path);
    REQUIRE (written.existsAsFile());
    REQUIRE (written.getSize() > 1024);

    written.deleteFile();
}

TEST_CASE ("Editor: Overlay-Host deckt den Editor ab", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    PluginEditor editor (proc);

    REQUIRE (editor.overlayHost() != nullptr);
    REQUIRE_FALSE (editor.overlayHost()->isOpen());

    renderOnce (editor, 1180, 720);
    REQUIRE (editor.overlayHost()->getWidth() == 1180);
    REQUIRE (editor.overlayHost()->getHeight() == 720);
}

TEST_CASE ("Editor: Overlays oeffnen und schliessen", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    PluginEditor editor (proc);

    editor.openSettings();
    REQUIRE (editor.overlayHost()->isOpen());
    REQUIRE (editor.overlayHost()->depth() == 1);

    editor.overlayHost()->pop();
    REQUIRE_FALSE (editor.overlayHost()->isOpen());

    editor.openLibrary();
    REQUIRE (editor.overlayHost()->depth() == 1);

    editor.overlayHost()->pop();

    editor.openPresetManager();
    REQUIRE (editor.overlayHost()->depth() == 1);

    editor.overlayHost()->pop();
    REQUIRE_FALSE (editor.overlayHost()->isOpen());
}

TEST_CASE ("Editor: Overlay faengt Escape ab", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    PluginEditor editor (proc);

    editor.openSettings();
    REQUIRE (editor.overlayHost()->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE_FALSE (editor.overlayHost()->isOpen());
}

TEST_CASE ("Editor: jedes Overlay hat einen Schliessen-Knopf", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    PluginEditor editor (proc);

    // Der Knopf gehoert dem Host, damit ihn kein Overlay vergessen kann.
    REQUIRE (editor.overlayHost()->closeButton() != nullptr);
    REQUIRE_FALSE (editor.overlayHost()->closeButton()->isVisible());

    // `Button::triggerClick()` stellt die Nachricht asynchron ein; im Test muss
    // die Message-Queue von Hand laufen.
    const auto clickClose = [&editor]
    {
        editor.overlayHost()->closeButton()->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    };

    editor.openLibrary();
    REQUIRE (editor.overlayHost()->closeButton()->isVisible());
    REQUIRE (editor.overlayHost()->closeButton()->getWidth() >= 24);

    // Klick schliesst das Overlay (nicht das ganze Plugin).
    clickClose();
    REQUIRE_FALSE (editor.overlayHost()->isOpen());
    REQUIRE_FALSE (editor.overlayHost()->closeButton()->isVisible());

    // Auch fuer die anderen Overlays.
    editor.openSettings();
    REQUIRE (editor.overlayHost()->closeButton()->isVisible());
    clickClose();
    REQUIRE_FALSE (editor.overlayHost()->isOpen());

    editor.openPresetManager();
    REQUIRE (editor.overlayHost()->closeButton()->isVisible());
    clickClose();
    REQUIRE_FALSE (editor.overlayHost()->isOpen());
}

TEST_CASE ("Editor: Schliessen-Knopf sitzt in der Kopfleiste", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    PluginEditor editor (proc);

    renderOnce (editor, 1180, 720);
    editor.openLibrary();

    auto* button = editor.overlayHost()->closeButton();
    REQUIRE (button != nullptr);
    REQUIRE (button->getWidth() == 28);
    REQUIRE (button->getHeight() == 28);

    // Oben rechts, oberhalb der Overlay-Flaeche (24 px Rand + 28 px Leiste).
    REQUIRE (button->getRight() <= 1180 - 24);
    REQUIRE (button->getBottom() <= 24 + 36);
}

TEST_CASE ("Undo: Zustandsaenderung laesst sich zuruecknehmen", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    const double before = proc.getBpm();

    proc.beginStateEdit ("Test");
    auto state = proc.captureState();
    state.setBpm (before + 20.0);
    proc.applyState (state);
    proc.endStateEdit();

    REQUIRE (proc.canUndo());
    REQUIRE (proc.undoName() == "Test");

    REQUIRE (proc.undo());
    REQUIRE_THAT (proc.getBpm(), WithinAbs (before, 0.5));

    REQUIRE (proc.canRedo());
    REQUIRE (proc.redo());
    REQUIRE_THAT (proc.getBpm(), WithinAbs (before + 20.0, 0.5));
}

TEST_CASE ("Editor: Strg+Z nimmt die letzte Aenderung zurueck", "[ui][integration]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    PluginEditor editor (proc);

    const double before = proc.getBpm();

    proc.beginStateEdit ("Test");
    auto state = proc.captureState();
    state.setBpm (before + 10.0);
    proc.applyState (state);
    proc.endStateEdit();

    REQUIRE (editor.keyPressed (juce::KeyPress ('Z', juce::ModifierKeys::commandModifier, 0)));
    REQUIRE_THAT (proc.getBpm(), WithinAbs (before, 0.5));
}
