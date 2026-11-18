#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include "core/PresetIO.h"
#include "core/Prefs.h"
#include "core/Version.h"
#include "ui/FxPage.h"
#include "ui/KickPage.h"
#include "ui/UiTheme.h"
#include "ui/overlays/DialogHost.h"
#include "ui/overlays/LibraryOverlay.h"
#include "ui/overlays/PresetManagerOverlay.h"
#include "ui/overlays/SettingsOverlay.h"

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

    // --- Kopf-Knoepfe fuer die Overlays (B-100/B-101/B-102) --------------
    for (auto* button : { &libraryButton, &gearButton, &presetsButton })
    {
        button->setColour (juce::TextButton::buttonColourId, theme::panelAlt);
        button->setColour (juce::TextButton::textColourOffId, theme::textDim);
        button->setColour (juce::TextButton::textColourOnId, theme::text);
        addAndMakeVisible (*button);
    }

    libraryButton.onClick = [this] { openLibrary(); };
    gearButton.onClick    = [this] { openSettings(); };
    presetsButton.onClick = [this] { openPresetManager(); };

    noteLabel.setText ("MIDI: -", juce::dontSendNotification);
    noteLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    noteLabel.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (noteLabel);

    // --- Seiten -----------------------------------------------------------
    kickPageComponent = std::make_unique<KickPage> (proc);
    addAndMakeVisible (*kickPageComponent);

    fxPageComponent = std::make_unique<FxPage> (proc);
    addChildComponent (*fxPageComponent);

    // --- Overlay-Schicht (B-105) ------------------------------------------
    // Liegt ueber den Seiten und wird erst beim Oeffnen sichtbar.
    prefs = std::make_unique<Prefs> (Prefs::defaultFile());
    prefs->load();

    presetManager = std::make_unique<PresetManager> (PresetIO::ensureUserPresetDirectory());
    presetManager->scan();

    overlayHostComponent = std::make_unique<OverlayHost>();
    addChildComponent (*overlayHostComponent);

    showPage (false);
    startTimerHz (8);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::openLibrary()
{
    if (overlayHostComponent == nullptr)
        return;

    auto overlay = std::make_unique<LibraryOverlay>();

    // PRESETS-Tab aus dem Nutzer-Ordner fuellen (B-100). Die uebrigen Tabs
    // bleiben leer, solange der Content-Index nicht angebunden ist.
    std::vector<LibraryItem> items;

    if (presetManager != nullptr)
    {
        presetManager->scan();

        for (const auto& entry : presetManager->entries())
            items.push_back ({ entry.name,
                               entry.file.getParentDirectory().getFileName(),
                               entry.file.getFullPathName(),
                               false });
    }

    overlay->setItems (LibraryOverlay::Tab::presets, std::move (items));
    overlay->onCloseRequested = [this]
    {
        if (overlayHostComponent != nullptr)
            overlayHostComponent->pop();
    };

    overlayHostComponent->push (std::move (overlay));
}

void PluginEditor::openSettings()
{
    if (overlayHostComponent == nullptr || prefs == nullptr)
        return;

    auto overlay = std::make_unique<SettingsOverlay> (*prefs);

    // Eine Zeile je Sektion aus prefs.json (B-101).
    std::vector<SettingsRow> rows;

    for (const auto& section : Prefs::sections())
        rows.push_back ({ section,
                          section.toLowerCase() + ".enabled",
                          section + " aktiv",
                          "bool" });

    overlay->setRows (std::move (rows));
    overlay->onCloseRequested = [this]
    {
        if (overlayHostComponent != nullptr)
            overlayHostComponent->pop();
    };

    overlayHostComponent->push (std::move (overlay));
}

void PluginEditor::openPresetManager()
{
    if (overlayHostComponent == nullptr || presetManager == nullptr)
        return;

    presetManager->scan();

    auto overlay = std::make_unique<PresetManagerOverlay> (*presetManager);
    auto* raw = overlay.get();

    raw->onPresetActivated = [this] (const juce::File& file)
    {
        juce::String error;

        if (const auto data = PresetIO::loadKickPreset (file, error))
        {
            proc.beginStateEdit ("Preset laden");
            proc.applyState (data->state);
            proc.endStateEdit();

            if (kickPageComponent != nullptr)
                kickPageComponent->syncFromProcessor();
        }

        if (overlayHostComponent != nullptr)
            overlayHostComponent->pop();
    };

    raw->onCloseRequested = [this]
    {
        if (overlayHostComponent != nullptr)
            overlayHostComponent->pop();
    };

    overlayHostComponent->push (std::move (overlay));
}

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

bool PluginEditor::keyPressed (const juce::KeyPress& key)
{
    const bool controlDown = key.getModifiers().isCommandDown();

    if (controlDown && key.getKeyCode() == 'Z')
    {
        const bool shiftDown = key.getModifiers().isShiftDown();
        const bool changed = shiftDown ? proc.redo() : proc.undo();

        if (changed)
        {
            if (kickPageComponent != nullptr)
                kickPageComponent->syncFromProcessor();

            if (fxPageComponent != nullptr)
                fxPageComponent->syncFromProcessor();
        }

        return true;
    }

    return false;
}

void PluginEditor::resizedTabButtons()
{
    auto header = getLocalBounds().reduced (16).removeFromTop (56);
    auto tabs = header.removeFromRight (140).withSizeKeepingCentre (140, 24);

    kickTabButton.setBounds (tabs.removeFromLeft (70));
    fxTabButton.setBounds (tabs);

    // Overlay-Knoepfe links neben den Seiten-Tabs (B-100/B-101/B-102).
    auto buttons = header.removeFromRight (232).withSizeKeepingCentre (232, 22);
    presetsButton.setBounds (buttons.removeFromRight (78).reduced (1, 0));
    gearButton.setBounds (buttons.removeFromRight (56).reduced (1, 0));
    libraryButton.setBounds (buttons.removeFromRight (84).reduced (1, 0));
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

    // Die Overlay-Schicht deckt den ganzen Editor ab (B-105).
    if (overlayHostComponent != nullptr)
        overlayHostComponent->setBoundsToParent (getLocalBounds());
}

} // namespace bfka
