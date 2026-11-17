#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include "core/ParameterRegistry.h"
#include "core/Version.h"

namespace bfka
{

namespace
{
    const juce::Colour kBackground { 0xff14161a };
    const juce::Colour kPanel      { 0xff1d2026 };
    const juce::Colour kAccent     { 0xffff7a2f };
    const juce::Colour kText       { 0xffe8eaed };
    const juce::Colour kTextDim    { 0xff8b919b };
}

PluginEditor::PluginEditor (PluginProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      proc (processor)
{
    setSize (720, 420);

    titleLabel.setText (Version::productName, juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (24.0f).withStyle ("Bold")));
    titleLabel.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (titleLabel);

    versionLabel.setText ("Version " + Version::get() + "  ·  Phase P0 (Skelett)",
                          juce::dontSendNotification);
    versionLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    versionLabel.setColour (juce::Label::textColourId, kTextDim);
    addAndMakeVisible (versionLabel);

    noteLabel.setText ("MIDI: -", juce::dontSendNotification);
    noteLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    noteLabel.setColour (juce::Label::textColourId, kTextDim);
    addAndMakeVisible (noteLabel);

    masterLabel.setText ("MASTER", juce::dontSendNotification);
    masterLabel.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    masterLabel.setColour (juce::Label::textColourId, kTextDim);
    addAndMakeVisible (masterLabel);

    masterSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    masterSlider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    masterSlider.setColour (juce::Slider::textBoxTextColourId, kText);
    addAndMakeVisible (masterSlider);

    if (auto* param = proc.parameters().getParameter ("master_vol"))
    {
        masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.parameters(), "master_vol", masterSlider);
        juce::ignoreUnused (param);
    }

    demoButton.setColour (juce::TextButton::buttonColourId, kPanel.brighter (0.15f));
    demoButton.setColour (juce::TextButton::textColourOffId, kText);
    demoButton.onClick = [this]
    {
        const auto ok = proc.startDemoKick();
        statusLabel.setText (proc.getLastLoadReport(),
                             juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, ok ? kTextDim : juce::Colours::orange);
    };
    addAndMakeVisible (demoButton);

    playButton.setColour (juce::TextButton::buttonColourId, kAccent.withAlpha (0.25f));
    playButton.setColour (juce::TextButton::textColourOffId, kText);
    playButton.onClick = [this] { proc.triggerKick(); };
    addAndMakeVisible (playButton);

    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setColour (juce::Label::textColourId, kTextDim);
    statusLabel.setText ("Noch kein Kick geladen — 'Demo-Kick laden' holt Punch, Rumble und Click aus der Library.",
                         juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    startTimerHz (8);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::timerCallback()
{
    const auto note = proc.getLastNote();
    noteLabel.setText (note < 0 ? juce::String ("MIDI: -") : "MIDI: " + juce::String (note),
                       juce::dontSendNotification);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    auto panel = getLocalBounds().reduced (16).withTrimmedTop (72).toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (kAccent.withAlpha (0.35f));
    g.drawRoundedRectangle (panel, 10.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (56);
    titleLabel.setBounds (header.removeFromTop (32));
    versionLabel.setBounds (header);

    auto body = area.withTrimmedTop (24);
    auto left = body.removeFromLeft (180);

    masterLabel.setBounds (left.removeFromTop (16));
    masterSlider.setBounds (left.removeFromTop (120).withSizeKeepingCentre (120, 110));

    auto buttons = body.removeFromTop (40).removeFromLeft (320);
    demoButton.setBounds (buttons.removeFromLeft (160).reduced (2));
    playButton.setBounds (buttons.removeFromLeft (100).reduced (2));

    noteLabel.setBounds (body.removeFromBottom (24));
    statusLabel.setBounds (body.removeFromTop (40));
}

} // namespace bfka
