#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace bfka
{

class PluginProcessor;
class FxPage;
class KickPage;

/**
 * Editor des Plugins.
 *
 * Zwei Seiten, umschaltbar im Kopf:
 *  - **KICK** (ab B-83/B-84/B-86): Transport, Visualiser und zwoelf Slot-Karten
 *    (siehe `KickPage`).
 *  - **FX** (ab B-90): Rack mit Karten, Struktur-Editoren und Modulparametern.
 *
 * Der Editor haelt nur den Rahmen (Titel, Versionszeile, Seitenumschalter) und
 * reicht die Flaeche an die Seiten weiter. Zustand lesen und schreiben die
 * Seiten selbst beim Prozessor.
 */
class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Die KICK-Seite (Diagnose/Test); immer vorhanden. */
    KickPage* kickPage() const noexcept { return kickPageComponent.get(); }

    /** Die FX-Seite (Diagnose/Test); immer vorhanden. */
    FxPage* fxPage() const noexcept { return fxPageComponent.get(); }

private:
    void timerCallback() override;

    void showPage (bool showFxPage);
    void resizedTabButtons();

    PluginProcessor& proc;

    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::Label noteLabel;

    juce::TextButton kickTabButton { "KICK" };
    juce::TextButton fxTabButton   { "FX" };

    std::unique_ptr<KickPage> kickPageComponent;
    std::unique_ptr<FxPage>   fxPageComponent;

    bool fxPageVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

} // namespace bfka
