#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bfka
{

class PluginProcessor;

/**
 * Editor der Phase P0.

 * Zeigt Produkt/Version, einen Ausschnitt der Parameter (Master-Volume,
 * Kick-Länge) und die zuletzt gespielte MIDI-Note. Dient als Geruest fuer die
 * eigentliche KICK-Seite (P8) und die FX-Seite (P9).
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

    PluginProcessor& proc;

    juce::Label      titleLabel;
    juce::Label      versionLabel;
    juce::Label      noteLabel;

    juce::Slider     masterSlider;
    juce::Label      masterLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

} // namespace bfka
