#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include "core/ParameterRegistry.h"
#include "core/Version.h"
#include "ui/FxPage.h"
#include "ui/UiTheme.h"

namespace bfka
{

using namespace ui;

PluginEditor::PluginEditor (PluginProcessor& processor)
    : juce::AudioProcessorEditor (&processor),
      proc (processor)
{
    setSize (1180, 720);

    titleLabel.setText (Version::productName, juce::dontSendNotification);
    titleLabel.setFont (theme::titleFont());
    titleLabel.setColour (juce::Label::textColourId, theme::text);
    addAndMakeVisible (titleLabel);

    versionLabel.setText ("Version " + Version::get() + "  -  Phase P0 (Skelett)",
                          juce::dontSendNotification);
    versionLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    versionLabel.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (versionLabel);

    // --- Seiten-Umschaltung ----------------------------------------------
    for (auto* tab : { &kickTabButton, &fxTabButton })
    {
        tab->setColour (juce::TextButton::buttonColourId, theme::panelAlt);
        tab->setColour (juce::TextButton::textColourOffId, theme::textDim);
        tab->setColour (juce::TextButton::textColourOnId, theme::text);
        tab->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        addAndMakeVisible (*tab);
    }

    kickTabButton.onClick = [this] { showPage (false); };
    fxTabButton.onClick   = [this] { showPage (true); };

    noteLabel.setText ("MIDI: -", juce::dontSendNotification);
    noteLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    noteLabel.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (noteLabel);

    masterLabel.setText ("MASTER", juce::dontSendNotification);
    masterLabel.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    masterLabel.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (masterLabel);

    masterSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    masterSlider.setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
    masterSlider.setColour (juce::Slider::textBoxTextColourId, theme::text);
    addAndMakeVisible (masterSlider);

    if (proc.parameters().getParameter ("master_vol") != nullptr)
    {
        masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.parameters(), "master_vol", masterSlider);
    }

    demoButton.setColour (juce::TextButton::buttonColourId, theme::panel.brighter (0.15f));
    demoButton.setColour (juce::TextButton::textColourOffId, theme::text);
    demoButton.onClick = [this]
    {
        const auto ok = proc.startDemoKick();
        statusLabel.setText (proc.getLastLoadReport(), juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, ok ? theme::textDim : juce::Colours::orange);
    };
    addAndMakeVisible (demoButton);

    playButton.setColour (juce::TextButton::buttonColourId, theme::accent.withAlpha (0.25f));
    playButton.setColour (juce::TextButton::textColourOffId, theme::text);
    playButton.onClick = [this] { proc.triggerKick(); };
    addAndMakeVisible (playButton);

    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setColour (juce::Label::textColourId, theme::textDim);
    statusLabel.setText ("Noch kein Kick geladen - 'Demo-Kick laden' holt Punch, Rumble und Click aus der Library.",
                         juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    // --- FX-Seite ---------------------------------------------------------
    fxPage = std::make_unique<FxPage> (proc);
    addChildComponent (*fxPage);

    showPage (false);
    startTimerHz (8);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::showPage (bool showFxPage)
{
    fxPageVisible = showFxPage;

    const auto setKickControlsVisible = [showFxPage] (juce::Array<juce::Component*> components)
    {
        for (auto* component : components)
            if (component != nullptr)
                component->setVisible (! showFxPage);
    };

    setKickControlsVisible ({ &noteLabel, &masterLabel, &masterSlider,
                              &demoButton, &playButton, &statusLabel });

    if (fxPage != nullptr)
    {
        fxPage->setVisible (showFxPage);

        if (showFxPage)
            fxPage->syncFromProcessor();
    }

    kickTabButton.setToggleState (! showFxPage, juce::dontSendNotification);
    fxTabButton.setToggleState (showFxPage, juce::dontSendNotification);

    kickTabButton.setColour (juce::TextButton::buttonColourId,
                             showFxPage ? theme::panelAlt : theme::accent.withAlpha (0.30f));
    fxTabButton.setColour (juce::TextButton::buttonColourId,
                           showFxPage ? theme::accent.withAlpha (0.30f) : theme::panelAlt);

    resized();
}

void PluginEditor::timerCallback()
{
    const auto note = proc.getLastNote();
    noteLabel.setText (note < 0 ? juce::String ("MIDI: -") : "MIDI: " + juce::String (note),
                       juce::dontSendNotification);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    if (! fxPageVisible)
    {
        auto panel = getLocalBounds().reduced (16).withTrimmedTop (100).toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (panel, 10.0f);
        g.setColour (theme::accent.withAlpha (0.35f));
        g.drawRoundedRectangle (panel, 10.0f, 1.0f);
    }
}

void PluginEditor::resizedTabButtons()
{
    auto header = getLocalBounds().reduced (16).removeFromTop (56);
    auto tabs = header.removeFromRight (140).withSizeKeepingCentre (140, 24);

    kickTabButton.setBounds (tabs.removeFromLeft (70));
    fxTabButton.setBounds (tabs);
}

void PluginEditor::layoutKickPage (juce::Rectangle<int> body)
{
    auto left = body.removeFromLeft (180);

    masterLabel.setBounds (left.removeFromTop (16));
    masterSlider.setBounds (left.removeFromTop (120).withSizeKeepingCentre (120, 110));

    auto buttons = body.removeFromTop (40).removeFromLeft (320);
    demoButton.setBounds (buttons.removeFromLeft (160).reduced (2));
    playButton.setBounds (buttons.removeFromLeft (100).reduced (2));

    noteLabel.setBounds (body.removeFromBottom (24));
    statusLabel.setBounds (body.removeFromTop (40));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (56);
    titleLabel.setBounds (header.removeFromTop (32));
    versionLabel.setBounds (header);

    resizedTabButtons();

    auto body = area.withTrimmedTop (8);

    if (fxPage != nullptr)
        fxPage->setBounds (body);

    if (! fxPageVisible)
        layoutKickPage (body);
}

} // namespace bfka
