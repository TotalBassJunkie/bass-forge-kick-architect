#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include "core/Version.h"
#include "ui/FxPage.h"
#include "ui/KickPage.h"
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

    versionLabel.setText ("Version " + Version::get(),
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

    // --- Seiten -----------------------------------------------------------
    kickPageComponent = std::make_unique<KickPage> (proc);
    addAndMakeVisible (*kickPageComponent);

    fxPageComponent = std::make_unique<FxPage> (proc);
    addChildComponent (*fxPageComponent);

    showPage (false);
    startTimerHz (8);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::showPage (bool showFxPage)
{
    fxPageVisible = showFxPage;

    if (kickPageComponent != nullptr)
    {
        kickPageComponent->setVisible (! showFxPage);

        if (! showFxPage)
            kickPageComponent->syncFromProcessor();
    }

    if (fxPageComponent != nullptr)
    {
        fxPageComponent->setVisible (showFxPage);

        if (showFxPage)
            fxPageComponent->syncFromProcessor();
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
}

void PluginEditor::resizedTabButtons()
{
    auto header = getLocalBounds().reduced (16).removeFromTop (56);
    auto tabs = header.removeFromRight (140).withSizeKeepingCentre (140, 24);

    kickTabButton.setBounds (tabs.removeFromLeft (70));
    fxTabButton.setBounds (tabs);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (56);
    titleLabel.setBounds (header.removeFromTop (32));

    // Zweite Kopfzeile: Version links, zuletzt gespielte Note daneben.
    auto infoRow = header;
    versionLabel.setBounds (infoRow.removeFromLeft (200));
    infoRow.removeFromLeft (12);
    noteLabel.setBounds (infoRow.removeFromLeft (160));

    resizedTabButtons();

    auto body = area.withTrimmedTop (8);

    if (fxPageComponent != nullptr)
        fxPageComponent->setBounds (body);

    if (kickPageComponent != nullptr)
        kickPageComponent->setBounds (body);
}

} // namespace bfka
