#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include "core/KickState.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "ui/KickPage.h"
#include "ui/overlays/DialogHost.h"

using namespace bfka;

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
