#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace bfka
{

class PluginProcessor;
class FxPage;

/**
 * Editor des Plugins (P0-Skelett + FX-Seite).
 *
 * Zwei Seiten, umschaltbar im Kopf:
 *  - **KICK** (Phase P0): Produkt/Version, Master-Volume, Demo-Kick, Play,
 *    zuletzt gespielte MIDI-Note.
 *  - **FX** (ab B-90): Rack mit Karten, Struktur-Editoren und Modulparametern.
 */
class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor& processor);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    void showPage (bool showFxPage);
    void layoutKickPage (juce::Rectangle<int> area);
    void resizedTabButtons();

    PluginProcessor& proc;

    juce::Label      titleLabel;
    juce::Label      versionLabel;
    juce::Label      noteLabel;

    juce::TextButton kickTabButton { "KICK" };
    juce::TextButton fxTabButton   { "FX" };

    juce::Slider     masterSlider;
    juce::Label      masterLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    juce::TextButton demoButton { "Demo-Kick laden" };
    juce::TextButton playButton { "Play" };
    juce::Label      statusLabel;

    std::unique_ptr<FxPage> fxPage;
    bool fxPageVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

} // namespace bfka
