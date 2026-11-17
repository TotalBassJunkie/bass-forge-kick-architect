#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include "core/KickState.h"
#include "plugin/PluginProcessor.h"
#include "ui/ModuleCard.h"

using namespace bfka;

// Seitenaufteilung der Modulkarten (B-151): Module mit vielen Parametern
// zeigen hoechstens `paramsPerPage` Regler je Seite. Die Tests pruefen die
// Seitenlogik und dass ein Seitenwechsel tatsaechlich neu zeichnet.
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    struct CardListener final : bfka::ModuleCard::Listener
    {
        void moduleCardMoveRequested (int, int) override {}
        void moduleCardDuplicateRequested (int) override {}
        void moduleCardRemoveRequested (int) override {}
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
} // namespace

TEST_CASE ("Karte: dynEq bekommt Seiten", "[ui][card]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("dynEq"));
    proc.setFxChain (chain);

    CardListener listener;
    ModuleCard card (proc, 0, listener);

    REQUIRE (proc.getModule (0) != nullptr);

    const int numParams = (int) proc.getModule (0)->params().size();

    REQUIRE (numParams > 12);
    REQUIRE (card.pageCount() == (numParams + ModuleCard::paramsPerPage - 1) / ModuleCard::paramsPerPage);
    REQUIRE (card.currentPage() == 0);
}

TEST_CASE ("Karte: Seitenindex wird geklemmt", "[ui][card]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("dynEq"));
    proc.setFxChain (chain);

    CardListener listener;
    ModuleCard card (proc, 0, listener);

    card.setPage (999);
    REQUIRE (card.currentPage() == card.pageCount() - 1);

    card.setPage (-5);
    REQUIRE (card.currentPage() == 0);
}

TEST_CASE ("Karte: Seitenwechsel rendert", "[ui][card]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("dynEq"));
    proc.setFxChain (chain);

    CardListener listener;
    ModuleCard card (proc, 0, listener);

    card.setSize (640, card.preferredHeight());

    juce::Image image (juce::Image::ARGB, 640, card.preferredHeight(), true);
    juce::Graphics g (image);
    card.paintEntireComponent (g, true);

    card.setPage (1);

    juce::Image image2 (juce::Image::ARGB, 640, card.preferredHeight(), true);
    juce::Graphics g2 (image2);
    card.paintEntireComponent (g2, true);

    REQUIRE (card.currentPage() == 1);
}

TEST_CASE ("Karte: Hoehe ohne Struktur-Editor bleibt klein", "[ui][card]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("dist"));
    proc.setFxChain (chain);

    CardListener listener;
    ModuleCard card (proc, 0, listener);

    // "dist" hat keinen Struktur-Editor: Kopf, MIX, Display und eine
    // Reglerzeile. Die Karte bleibt damit deutlich unter dem Kartenmaximum.
    REQUIRE (card.preferredHeight() <= 420);
}
