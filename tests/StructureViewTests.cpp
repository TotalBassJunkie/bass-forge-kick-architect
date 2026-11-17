#include <catch2/catch_test_macros.hpp>

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <memory>

#include "fx/modules/KickShaperModule.h"
#include "fx/modules/MidSideEqModule.h"
#include "plugin/PluginProcessor.h"
#include "ui/FxPage.h"
#include "ui/editors/MidSideEqView.h"
#include "ui/ModuleStructureView.h"

using namespace bfka;

// Struktur-Editoren (B-92b) und das Rueckschreiben der Modul-Zustaende.
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

    /** Zeichnet eine Komponente einmal komplett; optional als PNG gesichert. */
    void renderOnce (juce::Component& component, int width, int height,
                     const juce::String& imageName = {})
    {
        component.setSize (juce::jmax (1, width), juce::jmax (1, height));

        juce::Image image (juce::Image::ARGB, juce::jmax (1, width), juce::jmax (1, height), true);
        {
            juce::Graphics g (image);
            component.paintEntireComponent (g, true);
        }

        if (imageName.isEmpty()
            || juce::SystemStats::getEnvironmentVariable ("BFKA_RENDER_UI", { }).isEmpty())
            return;

        // Nur fuer die Sichtpruefung von Hand (C:\bfka-re\ui-<name>.png).
        const auto target = juce::File ("C:/bfka-re").getChildFile ("ui-" + imageName + ".png");
        target.getParentDirectory().createDirectory();

        if (juce::FileOutputStream stream (target); stream.openedOk())
            juce::PNGImageFormat().writeImageToStream (image, stream);
    }

    juce::ValueTree makeBandTree (const juce::String& type, int kind, double freq, double gain, int ch)
    {
        juce::ValueTree band (juce::Identifier ("Band"));
        band.setProperty ("kind", kind, nullptr);
        band.setProperty ("f", freq, nullptr);
        band.setProperty ("q", 0.9, nullptr);
        band.setProperty ("g", gain, nullptr);
        band.setProperty ("dyn", 0.0, nullptr);
        band.setProperty ("ch", ch, nullptr);
        juce::ignoreUnused (type);
        return band;
    }
} // namespace

TEST_CASE ("Struktur-Editoren: Fabrik kennt mseq, shaper und final", "[ui][editor]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    const juce::StringArray withView { "mseq", "shaper", "final", "ott", "reverb", "dynEq" };
    const juce::StringArray withoutView { "eq", "dist", "filt", "trans" };

    for (const auto& type : withView)
    {
        FxChain chain;
        chain.modules.push_back (makeModule (type));
        proc.setFxChain (chain);

        // Damit die Bilder etwas zeigen: Beispiel-Struktur setzen.
        if (auto* mseq = dynamic_cast<MidSideEqModule*> (proc.getModule (0)))
        {
            juce::ValueTree tree (juce::Identifier ("Module"));
            tree.appendChild (makeBandTree (type, 1, 90.0, -3.0, 0), nullptr);
            tree.appendChild (makeBandTree (type, 0, 700.0, 2.5, 0), nullptr);
            tree.appendChild (makeBandTree (type, 2, 6000.0, 4.0, 1), nullptr);
            mseq->setState (tree);
            proc.writeBackModuleStates();
            CHECK (mseq->getState().getNumChildren() == 3);
        }
        else if (auto* shaper = dynamic_cast<KickShaperModule*> (proc.getModule (0)))
        {
            shaper->setEnvelopePoints ({ { 0.0, 0.2, 0 }, { 0.25, 1.0, 0 },
                                         { 0.6, 0.35, 1 }, { 1.0, 0.0, 0 } });
            proc.writeBackModuleStates();
            CHECK (shaper->envelopePoints().size() == 4);
        }

        auto view = createStructureView (proc, 0);

        INFO ("Typ " << type);

        REQUIRE (view != nullptr);
        CHECK (view->preferredHeight() > 80);

        // Gegenprobe: die Beispiel-Struktur steht wirklich im Modul.
        if (type == "mseq")
        {
            auto* checkModule = dynamic_cast<MidSideEqModule*> (proc.getModule (0));
            REQUIRE (checkModule != nullptr);
            CHECK (checkModule->getState().getNumChildren() == 3);

            auto* mseqView = dynamic_cast<MidSideEqView*> (view.get());
            REQUIRE (mseqView != nullptr);
            CHECK (mseqView->nodeCount() == 3);
        }
        else if (type == "shaper")
        {
            auto* checkModule = dynamic_cast<KickShaperModule*> (proc.getModule (0));
            REQUIRE (checkModule != nullptr);
            CHECK (checkModule->envelopePoints().size() == 4);
        }

        renderOnce (*view, 820, view->preferredHeight(), type);
        view->refresh();
        renderOnce (*view, 500, view->preferredHeight(), type + "-small");
    }

    // Module ohne Struktur-Editor zeigen nur Parameter und Wellenform.
    for (const auto& type : withoutView)
    {
        FxChain chain;
        chain.modules.push_back (makeModule (type));
        proc.setFxChain (chain);

        INFO ("Typ " << type);
        CHECK (createStructureView (proc, 0) == nullptr);
    }
}

TEST_CASE ("Struktur-Editoren: FX-Seite mit allen Editoren zeichnet", "[ui][editor]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;

    const juce::StringArray types { "mseq", "shaper", "final", "dynEq" };

    for (const auto& type : types)
        chain.modules.push_back (makeModule (type));

    proc.setFxChain (chain);

    // Beispiel-Struktur fuer die beiden Kurven-Editoren.
    if (auto* mseq = dynamic_cast<MidSideEqModule*> (proc.getModule (0)))
    {
        juce::ValueTree tree (juce::Identifier ("Module"));
        tree.appendChild (makeBandTree ("mseq", 1, 90.0, -3.0, 0), nullptr);
        tree.appendChild (makeBandTree ("mseq", 0, 700.0, 2.5, 0), nullptr);
        tree.appendChild (makeBandTree ("mseq", 2, 6000.0, 4.0, 1), nullptr);
        mseq->setState (tree);
    }

    if (auto* shaper = dynamic_cast<KickShaperModule*> (proc.getModule (1)))
        shaper->setEnvelopePoints ({ { 0.0, 0.2, 0 }, { 0.25, 1.0, 0 },
                                     { 0.6, 0.35, 1 }, { 1.0, 0.0, 0 } });

    proc.writeBackModuleStates();

    juce::Component host;
    host.setSize (1100, 900);

    std::vector<std::unique_ptr<ModuleStructureView>> views;

    for (int slot = 0; slot < (int) chain.modules.size(); ++slot)
    {
        auto view = createStructureView (proc, slot);
        REQUIRE (view != nullptr);
        renderOnce (*view, 1000, view->preferredHeight(), "card-" + types[slot]);
        views.push_back (std::move (view));
    }

    // Die komplette Seite (Werkzeugleiste, Seitenleiste, Karten) einmal zeichnen.
    FxPage page (proc);
    renderOnce (page, 1148, 640, "fxpage");
}

TEST_CASE ("Rueckschreiben: geloeschte Baender verschwinden aus dem Zustand", "[ui][editor]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("mseq"));
    proc.setFxChain (chain);

    auto* module = dynamic_cast<MidSideEqModule*> (proc.getModule (0));
    REQUIRE (module != nullptr);

    // Zwei Baender in das Modul schreiben und zuruecknehmen lassen.
    juce::ValueTree twoBands (juce::Identifier ("Module"));
    twoBands.appendChild (makeBandTree ("mseq", 1, 90.0, -4.0, 0), nullptr);
    twoBands.appendChild (makeBandTree ("mseq", 2, 6000.0, 3.0, 1), nullptr);

    module->setState (twoBands);
    proc.writeBackModuleStates();

    auto bandsInState = [&proc] ()
    {
        int count = 0;

        for (const auto& child : proc.getFxChain().modules[0].data)
            if (child.hasType (juce::Identifier ("Band")))
                ++count;

        return count;
    };

    REQUIRE (bandsInState() == 2);

    // Alle Baender loeschen (so macht es der Editor): die alten <Band>-Kinder
    // duerfen nicht im Zustand stehen bleiben.
    juce::ValueTree empty (juce::Identifier ("Module"));
    module->setState (empty);
    proc.writeBackModuleStates();

    CHECK (bandsInState() == 0);
}
