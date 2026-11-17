#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <cmath>
#include <memory>

#include "core/KickState.h"
#include "core/ParameterRegistry.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "ui/FxPage.h"
#include "ui/KickPage.h"
#include "ui/SlotCard.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// KICK-Seite (P8): Slot-Karten, Wellenform, Transport und die Prozessor-Haken
// zum Laden/Leeren einzelner Slots.
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Zeichnet eine Komponente einmal; optional als PNG fuer die Sichtpruefung. */
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

        const auto target = juce::File ("C:/bfka-re").getChildFile ("ui-" + imageName + ".png");
        target.getParentDirectory().createDirectory();

        if (juce::FileOutputStream stream (target); stream.openedOk())
            juce::PNGImageFormat().writeImageToStream (image, stream);
    }

    juce::File testDirectory()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("bfka-kickpage-tests");
        dir.createDirectory();
        return dir;
    }

    /** Schreibt eine kurze WAV-Datei (0,25 s, 1 kHz) und liefert den Pfad. */
    juce::File writeTestSample (const juce::String& name, double seconds = 0.25,
                                double frequency = 1000.0)
    {
        const auto sampleRate = 48000.0;
        const auto numSamples = (int) (seconds * sampleRate);

        juce::AudioBuffer<float> buffer (1, numSamples);

        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, 0.8f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                             * frequency * (double) i / sampleRate));

        const auto file = testDirectory().getChildFile (name);
        file.deleteFile();

        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        REQUIRE (stream != nullptr);

        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), sampleRate, 1, 16, {}, 0));

        REQUIRE (writer != nullptr);
        stream.release();   // der Writer uebernimmt den Stream

        REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, numSamples));
        writer.reset();

        return file;
    }

    float readNormalised (PluginProcessor& proc, const juce::String& id)
    {
        auto* value = proc.parameters().getRawParameterValue (id);
        REQUIRE (value != nullptr);
        return value->load();
    }
} // namespace

TEST_CASE ("KICK-Seite: zwoelf Slot-Karten, Transport und Visualiser zeichnen",
           "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    KickPage page (proc);
    REQUIRE (page.getNumSlotCards() == KickState::kNumSlots);

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        INFO ("Slot " << slot);
        REQUIRE (page.getSlotCard (slot) != nullptr);
    }

    REQUIRE (page.getSlotCard (-1) == nullptr);
    REQUIRE (page.getSlotCard (KickState::kNumSlots) == nullptr);

    renderOnce (page, 1100, 620);
    page.setLoopEnabled (true);
    page.setLoopEnabled (false);
    renderOnce (page, 1100, 620);
}

TEST_CASE ("KICK-Seite: Sample laden fuellt Slot, Huelle und Karte", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    const auto file = writeTestSample ("kick-a.wav");
    REQUIRE (file.existsAsFile());

    juce::String error;
    REQUIRE (proc.setSlotSample (3, file, error));
    CHECK (error.isEmpty ());

    // Renderer und Huelle
    CHECK (proc.isSlotFilled (3));
    CHECK (proc.getNumFilledSlots() == 1);
    CHECK_FALSE (proc.isSlotFilled (2));

    const auto& envelope = proc.getSlotEnvelope (3);
    REQUIRE (envelope.valid);
    CHECK (envelope.lengthSeconds > 0.2);
    CHECK (envelope.lengthSeconds < 0.3);
    CHECK (envelope.peak > 0.5f);

    // Zustand: der Slot traegt die Referenz (und damit der Preset-Roundtrip)
    const auto state = proc.captureState ();
    REQUIRE (state.getSample (3).has_value ());
    CHECK (state.getSample (3)->fileName == "kick-a.wav");
    CHECK (state.getSample (3)->isValid ());

    // Karte zeigt Namen und Sample
    KickPage page (proc);
    page.syncFromProcessor ();
    auto* card = page.getSlotCard (3);
    REQUIRE (card != nullptr);
    CHECK (card->hasSample ());

    renderOnce (page, 1100, 620, "kickpage-loaded");

    // Fehlerfaelle
    juce::String missingError;
    CHECK_FALSE (proc.setSlotSample (0, juce::File ("C:/gibt/es/nicht.wav"), missingError));
    CHECK (missingError.isNotEmpty ());

    juce::String slotError;
    CHECK_FALSE (proc.setSlotSample (99, file, slotError));
    CHECK (slotError.isNotEmpty ());
}

TEST_CASE ("KICK-Seite: Slot leeren raeumt Zustand, Huelle und Renderer", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    const auto file = writeTestSample ("kick-b.wav");

    juce::String error;
    REQUIRE (proc.setSlotSample (5, file, error));
    REQUIRE (proc.isSlotFilled (5));

    proc.clearSlotSample (5);

    CHECK_FALSE (proc.isSlotFilled (5));
    CHECK_FALSE (proc.getSlotEnvelope (5).valid);
    CHECK_FALSE (proc.captureState ().getSample (5).has_value ());

    // Doppeltes Leeren darf nichts kaputt machen.
    proc.clearSlotSample (5);
    proc.clearSlotSample (-1);
    proc.clearSlotSample (99);
}

TEST_CASE ("KICK-Seite: Regler wirken live auf die Voice-Einstellungen", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    const auto file = writeTestSample ("kick-c.wav");

    juce::String error;
    REQUIRE (proc.setSlotSample (0, file, error));

    CHECK_THAT (proc.getSlotVoiceSettings (0).gainLinear,
                Catch::Matchers::WithinAbs (1.0, 0.01));

    // VOL auf -6 dB
    const auto* volume = ParameterRegistry::get().find ("l0_vol");
    REQUIRE (volume != nullptr);

    if (auto* parameter = proc.parameters().getParameter (volume->id))
        parameter->setValueNotifyingHost (volume->toNormalised (-6.0f));

    proc.applySlotSettings (0);

    // -6 dB -> etwa 0,501 linear
    CHECK_THAT (proc.getSlotVoiceSettings (0).gainLinear,
                Catch::Matchers::WithinAbs (0.501, 0.01));

    // FADE IN auf 25 % der getrimmten Laenge (Sample ist 0,25 s lang)
    const auto* fadeIn = ParameterRegistry::get().find ("l0_fadein");
    REQUIRE (fadeIn != nullptr);

    if (auto* parameter = proc.parameters().getParameter (fadeIn->id))
        parameter->setValueNotifyingHost (fadeIn->toNormalised (25.0f));

    proc.applySlotSettings (0);

    CHECK_THAT (proc.getSlotVoiceSettings (0).fadeIn.lengthSeconds,
                Catch::Matchers::WithinAbs (0.25 * 0.25, 0.01));

    // MUTE und SOLO sind Slot-Parameter und landen im Renderer.
    const auto* mute = ParameterRegistry::get().find ("l0_mute");
    REQUIRE (mute != nullptr);

    if (auto* parameter = proc.parameters().getParameter (mute->id))
        parameter->setValueNotifyingHost (1.0f);

    proc.applySlotSettings (0);

    CHECK (proc.getSlotVoiceSettings (0).mute);
    CHECK (readNormalised (proc, "l0_mute") > 0.5f);

    // Ein zweiter Aufruf darf den Zustand nicht veraendern (idempotent).
    proc.applyAllSlotSettings ();
    CHECK (proc.getSlotVoiceSettings (0).mute);
    CHECK (proc.getSlotVoiceSettings (1).mute == false);
}

TEST_CASE ("KICK-Seite: BPM setzt Tempo, Kick-Laenge und Transport", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    proc.setBpm (120.0);

    CHECK_THAT (proc.getBpm (), Catch::Matchers::WithinAbs (120.0, 1.0e-9));
    CHECK_THAT (proc.getBeatLengthSeconds (), Catch::Matchers::WithinAbs (0.5, 1.0e-9));
    CHECK_THAT (proc.getKickLengthSeconds (), Catch::Matchers::WithinAbs (0.5, 1.0e-9));

    // Der Zustand traegt das Tempo (Preset-Roundtrip).
    CHECK_THAT (proc.captureState ().getBpm (), Catch::Matchers::WithinAbs (120.0, 1.0e-9));

    // Untergrenze/Obergrenze werden geklemmt.
    proc.setBpm (5.0);
    CHECK_THAT (proc.getBpm (), Catch::Matchers::WithinAbs (20.0, 1.0e-9));

    proc.setBpm (5000.0);
    CHECK_THAT (proc.getBpm (), Catch::Matchers::WithinAbs (960.0, 1.0e-9));
}

TEST_CASE ("KICK-Seite: Rolle, Sperre und KEY-TRACK liegen im Zustand", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    proc.setSlotRole (3, Role::rumble);
    proc.setSlotLocked (3, true);
    proc.setSlotKeyTracked (3, true);

    auto state = proc.captureState ();
    CHECK (state.getRole (3) == Role::rumble);
    CHECK (state.isLocked (3));
    CHECK (state.isKeyTracked (3));
    CHECK (roleToString (state.getRole (3)) == "RUMBLE");

    proc.setSlotLocked (3, false);
    state = proc.captureState ();
    CHECK_FALSE (state.isLocked (3));
    CHECK (state.isKeyTracked (3));

    // Ausserhalb des Bereichs: kein Absturz, keine Aenderung
    proc.setSlotRole (99, Role::punch);
    proc.setSlotLocked (-1, true);
    CHECK (proc.captureState ().getRole (0) == Role::none);
}

TEST_CASE ("KICK-Seite: Visualiser zeichnet Parts und Summe", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    const auto fileA = writeTestSample ("kick-d.wav", 0.2, 80.0);
    const auto fileB = writeTestSample ("kick-e.wav", 0.4, 250.0);

    juce::String error;
    REQUIRE (proc.setSlotSample (0, fileA, error));
    REQUIRE (proc.setSlotSample (1, fileB, error));

    proc.setSlotRole (0, Role::punch);
    proc.setSlotRole (1, Role::rumble);

    KickPage page (proc);
    page.syncFromProcessor ();

    auto& vis = page.visualizer ();
    CHECK (vis.viewMode () == KickVisualizer::ViewMode::parts);
    REQUIRE (vis.windowSeconds () > 0.0);

    renderOnce (page, 1100, 620, "kickpage-parts");

    vis.setViewMode (KickVisualizer::ViewMode::sum);
    REQUIRE (vis.viewMode () == KickVisualizer::ViewMode::sum);
    renderOnce (page, 1100, 620, "kickpage-sum");

    // MUTE darf die Summe nicht sprengen.
    if (auto* parameter = proc.parameters().getParameter ("l0_mute"))
        parameter->setValueNotifyingHost (1.0f);

    proc.applySlotSettings (0);
    vis.refresh ();
    renderOnce (page, 1100, 620, "kickpage-sum-muted");
}

TEST_CASE ("KICK-Seite: Anzeigetexte der Regler ohne Rundungsrauschen", "[ui][kickpage]")
{
    // Die Slot-Karte zeigt `ParamDesc::format()` direkt im Textfeld. Werte, die
    // aus der Kurvenumkehr stammen, duerfen nicht als "-0.00" erscheinen.
    const auto* volume = ParameterRegistry::get().find ("l0_vol");
    REQUIRE (volume != nullptr);
    CHECK (volume->format (volume->normalisedDefault()) == "0.00 dB");

    const auto* pitch = ParameterRegistry::get().find ("l0_pitch");
    REQUIRE (pitch != nullptr);
    CHECK (pitch->format (pitch->normalisedDefault()) == "0.00 st");

    const auto* stretch = ParameterRegistry::get().find ("l0_stretch");
    REQUIRE (stretch != nullptr);
    CHECK (stretch->format (stretch->normalisedDefault()) == "1.000 x");

    const auto* trimStart = ParameterRegistry::get().find ("l0_trimstart");
    REQUIRE (trimStart != nullptr);
    CHECK (trimStart->format (trimStart->normalisedDefault()) == "0.0 %");
}

TEST_CASE ("KICK-Seite: Reglertexte der Karte in Anzeige-Einheiten", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    KickPage page (proc);
    page.syncFromProcessor ();

    auto* card = page.getSlotCard (0);
    REQUIRE (card != nullptr);

    const auto textOf = [card] (const juce::String& id)
    {
        auto* slider = dynamic_cast<juce::Slider*> (card->findChildWithID (id));
        REQUIRE (slider != nullptr);
        return slider->getTextFromValue (slider->getValue ());
    };

    CHECK (textOf ("l0_vol") == "0.00 dB");
    CHECK (textOf ("l0_pitch") == "0.00 st");
    CHECK (textOf ("l0_stretch") == "1.000 x");
    CHECK (textOf ("l0_trimend") == "100.0 %");
    CHECK (textOf ("l0_fadein") == "0.0 %");
}

TEST_CASE ("KICK-Seite: Demo-Kick landet im Zustand und in den Karten", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    if (! proc.startDemoKick ())
    {
        WARN ("Sample-Library nicht vorhanden - Demo-Kick-Test uebersprungen.");
        return;
    }

    // Der Demo-Kick schreibt seine Referenzen in den Zustand (B-154): die Karten
    // zeigen genau das, was der Renderer spielt.
    const auto state = proc.captureState ();
    auto samplesInState = 0;

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        if (! state.getSample (slot).has_value ())
            continue;

        ++samplesInState;
        CHECK (state.getSample (slot)->isValid ());
        CHECK (state.getSample (slot)->fileName.isNotEmpty ());
        CHECK (proc.getSlotEnvelope (slot).valid);
    }

    CHECK (samplesInState >= 1);
    CHECK (proc.getNumFilledSlots () >= 1);

    KickPage page (proc);
    page.syncFromProcessor ();

    auto cardsWithSample = 0;

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        if (page.getSlotCard (slot)->hasSample ())
            ++cardsWithSample;

    CHECK (cardsWithSample == samplesInState);

    renderOnce (page, 1100, 620, "kickpage-demo");
}

TEST_CASE ("KICK-Seite: Editor baut beide Seiten", "[ui][kickpage]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    PluginEditor editor (proc);

    REQUIRE (editor.kickPage () != nullptr);
    REQUIRE (editor.fxPage () != nullptr);
    CHECK (editor.kickPage ()->getNumSlotCards () == KickState::kNumSlots);

    renderOnce (editor, 1180, 720, "editor-kick");

    // Die FX-Seite im Editor bleibt erreichbar und zeichnet (Regressionsschutz
    // fuer den Umbau des Editors auf zwei Seiten-Komponenten).
    renderOnce (*editor.fxPage (), 1100, 620, "editor-fx-page");

    // Zwei Karten einzeln zeichnen (Layout/Skalierung).
    auto* firstCard = editor.kickPage ()->getSlotCard (0);
    auto* lastCard  = editor.kickPage ()->getSlotCard (11);
    REQUIRE (firstCard != nullptr);
    REQUIRE (lastCard != nullptr);

    renderOnce (*firstCard, 320, SlotCard::kPreferredHeight);
    renderOnce (*lastCard, 240, SlotCard::kPreferredHeight);
}
