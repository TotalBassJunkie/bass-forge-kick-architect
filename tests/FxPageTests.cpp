#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <memory>
#include <utility>

#include "core/KickState.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "ui/FxPage.h"

using namespace bfka;

// Rauchtest der Oberflaeche: baut die echten Komponenten, laid sie aus und
// zeichnet sie in ein Bild. Faengt Layout-/Nullzeiger-Fehler, die ein reiner
// Logik-Test nicht sieht.
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    FxModuleState makeModule (const juce::String& type)
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = true;
        state.mix     = 1.0f;
        state.outGain = 1.0f;
        return state;
    }

    /** Zeichnet eine Komponente einmal komplett in ein Bild. */
    void renderOnce (juce::Component& component, int width, int height)
    {
        component.setSize (width, height);

        juce::Image image (juce::Image::ARGB, juce::jmax (1, width), juce::jmax (1, height), true);
        juce::Graphics g (image);

        component.paintEntireComponent (g, true);
    }
} // namespace

TEST_CASE ("FX-Seite: leere Kette zeichnet ohne Karten", "[ui][editor]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxPage page (proc);
    renderOnce (page, 1000, 620);

    REQUIRE (proc.getEngineModuleCount() == 0);
}

TEST_CASE ("FX-Seite: Karten aus dem Zustand, inkl. unbekanntem Typ", "[ui][editor]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("ott"));
    chain.modules.push_back (makeModule ("eq"));
    chain.modules.push_back (makeModule ("reverb"));
    chain.modules.push_back (makeModule ("voelligNeu"));
    proc.setFxChain (chain);

    FxPage page (proc);
    renderOnce (page, 1000, 620);

    page.selectSlot (3);
    page.selectSlot (-1);
    page.selectSlot (99);
    page.selectSlot (0);

    renderOnce (page, 1000, 620);

    REQUIRE (proc.getEngineModuleCount() == 4);
}

TEST_CASE ("FX-Seite: syncFromProcessor nach Zustandswechsel", "[ui][editor]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxPage page (proc);
    renderOnce (page, 1000, 620);

    // Zustand von aussen setzen (z. B. Preset-Laden) und neu einlesen.
    FxChain chain;
    chain.bypassAll = true;
    chain.modules.push_back (makeModule ("dynEq"));
    proc.setFxChain (chain);

    page.syncFromProcessor();
    renderOnce (page, 1000, 620);

    REQUIRE (proc.getFxChain().modules.size() == 1);
}

TEST_CASE ("Editor: KICK-Seite und FX-Seite werden gebaut und gezeichnet", "[ui][editor]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("multiband"));
    proc.setFxChain (chain);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    REQUIRE (editor != nullptr);

    renderOnce (*editor, 1180, 720);
    renderOnce (*editor, 900, 560);

    REQUIRE (proc.getEngineModuleCount() == 1);
}
