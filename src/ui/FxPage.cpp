#include "ui/FxPage.h"

#include "core/PresetIO.h"
#include "fx/FxFactory.h"
#include "plugin/PluginProcessor.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr int kRowHeight   = 22;
    constexpr int kChainGap    = 8;
    constexpr int kButtonHeight = 22;
    constexpr int kButtonGap   = 3;

    /** Einheitliche Optik der Werkzeugleiste. */
    void styleToolbarButton (juce::TextButton& button, juce::Colour background,
                             juce::Colour textColour)
    {
        button.setColour (juce::TextButton::buttonColourId, background);
        button.setColour (juce::TextButton::textColourOffId, textColour);
        button.setColour (juce::TextButton::textColourOnId, textColour);
    }
} // namespace

FxPage::FxPage (PluginProcessor& processor)
    : proc (processor)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&cardHolder, false);
    viewport.setScrollBarsShown (true, false);

    // --- Ketten-Befehle ---------------------------------------------------
    styleToolbarButton (bypassButton, ui::theme::panelAlt, ui::theme::text);
    bypassButton.onClick = [this] { toggleBypassAll(); };
    addAndMakeVisible (bypassButton);

    styleToolbarButton (clearButton, ui::theme::panelAlt, ui::theme::text);
    clearButton.onClick = [this] { clearChain(); };
    addAndMakeVisible (clearButton);

    styleToolbarButton (defaultButton, ui::theme::panelAlt, ui::theme::text);
    defaultButton.setTooltip ("Werkskette laden (Hochpass + Finalizer)");
    defaultButton.onClick = [this] { applyDefaultChain(); };
    addAndMakeVisible (defaultButton);

    styleToolbarButton (lockButton, ui::theme::panelAlt, ui::theme::text);
    lockButton.setTooltip ("Kette gegen Loeschen/Umordnen sperren");
    lockButton.onClick = [this] { toggleLock(); };
    addAndMakeVisible (lockButton);

    // --- Presets (B-93) ---------------------------------------------------
    styleToolbarButton (saveButton, ui::theme::panelAlt, ui::theme::text);
    saveButton.setTooltip ("Kick-Preset als .bfkapreset speichern");
    saveButton.onClick = [this] { chooseSavePreset(); };
    addAndMakeVisible (saveButton);

    styleToolbarButton (loadButton, ui::theme::panelAlt, ui::theme::text);
    loadButton.setTooltip ("Kick-Preset laden");
    loadButton.onClick = [this] { chooseLoadPreset(); };
    addAndMakeVisible (loadButton);

    // --- A/B-Vergleich (B-94) ---------------------------------------------
    for (auto* button : { &slotAButton, &slotBButton })
    {
        styleToolbarButton (*button, ui::theme::panelAlt, ui::theme::textDim);
        button->setTooltip ("Klick: abrufen (leer: merken) - Shift-Klick: merken");
        addAndMakeVisible (*button);
    }

    slotAButton.onClick = [this] { recallOrStoreComparison (0); };
    slotBButton.onClick = [this] { recallOrStoreComparison (1); };

    styleToolbarButton (demoButton, ui::theme::accent.withAlpha (0.25f), ui::theme::text);
    demoButton.onClick = [this]
    {
        // Baut die Demokette (Multiband + optional Convolver + Dynamic EQ) und
        // startet sie einmal, damit man den Unterschied sofort hoert.
        proc.loadFxDemoChain();
        syncFromProcessor();
        proc.triggerKick();
    };
    addAndMakeVisible (demoButton);

    styleToolbarButton (loopButton, ui::theme::panelAlt, ui::theme::textDim);
    loopButton.onClick = [this]
    {
        loopEnabled = ! loopEnabled;
        lastTriggerMs = juce::Time::getMillisecondCounterHiRes();
        updateToolbarLook();
    };
    addAndMakeVisible (loopButton);

    // --- Modul-Presets (Seitenleiste) -------------------------------------
    styleToolbarButton (saveModuleButton, ui::theme::panelAlt, ui::theme::text);
    saveModuleButton.setTooltip ("Gewaehltes Modul als Modul-Preset speichern");
    saveModuleButton.onClick = [this] { chooseSaveModulePreset(); };
    addAndMakeVisible (saveModuleButton);

    styleToolbarButton (loadModuleButton, ui::theme::panelAlt, ui::theme::text);
    loadModuleButton.setTooltip ("Modul-Preset in den gewaehlten Platz laden");
    loadModuleButton.onClick = [this] { chooseLoadModulePreset(); };
    addAndMakeVisible (loadModuleButton);

    // `+ ADD MODULE`: ein Knopf je registriertem Modultyp.
    for (int i = 0; i < kNumFxTypes; ++i)
    {
        const auto type = (FxType) i;

        if (! FxFactory::get().isRegistered (type))
            continue;

        auto* button = addButtons.add (new juce::TextButton ("+ " + fxDisplayName (type)));
        styleToolbarButton (*button, ui::theme::panelAlt, ui::theme::text);
        button->onClick = [this, type] { addModule (type); };
        addAndMakeVisible (*button);
    }

    updateToolbarLook();
    syncFromProcessor();
    startTimerHz (15);
}

FxPage::~FxPage() = default;

// ---------------------------------------------------------------------------
// Zustand <-> Seite
// ---------------------------------------------------------------------------
void FxPage::syncFromProcessor()
{
    model.chain() = proc.getFxChain();

    if (selectedSlot >= model.size())
        selectedSlot = model.size() - 1;

    rebuildCards();
    repaint();
}

void FxPage::applyChainToProcessor()
{
    // Add/Remove/Move/Preset aendern die Modulinstanzen: der Audio-Thread darf
    // waehrend des Rebuilds nicht verarbeiten.
    proc.suspendProcessing (true);
    proc.setFxChain (model.chain());
    proc.suspendProcessing (false);

    rebuildCards();
    updateToolbarLook();
    repaint();
}

void FxPage::rebuildCards()
{
    cards.clear();

    for (int i = 0; i < model.size(); ++i)
    {
        // Die Listener-Rolle ist ein privater Basis-Teil dieser Seite; die
        // Umwandlung darf deshalb nur hier (im Member-Kontext) passieren.
        auto card = std::make_unique<ModuleCard> (proc, i, static_cast<ModuleCard::Listener&> (*this));
        cardHolder.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }

    layoutCards();

    bypassButton.setButtonText (model.isChainBypassed() ? "BYPASS: ON" : "BYPASS ALL");
    bypassButton.setColour (juce::TextButton::buttonColourId,
                            model.isChainBypassed() ? ui::theme::warn.withAlpha (0.35f)
                                                    : ui::theme::panelAlt);

    updateToolbarLook();
}

void FxPage::layoutCards()
{
    const int width = juce::jmax (240, viewport.getWidth() - 16);

    int y = 0;

    for (auto& card : cards)
    {
        const int height = card->preferredHeight();
        card->setBounds (0, y, width, height);
        y += height + kChainGap;
    }

    cardHolder.setSize (width, juce::jmax (y, viewport.getHeight()));
}

// ---------------------------------------------------------------------------
// Struktur-Aenderungen
// ---------------------------------------------------------------------------
void FxPage::addModule (FxType type)
{
    const auto index = model.addModule (type);

    if (index < 0)
    {
        if (model.isLocked())
            setStatus ("Kette ist gesperrt - erst LOCK loesen.", true);

        return;
    }

    selectedSlot = index;
    applyChainToProcessor();
    selectSlot (index);
}

void FxPage::clearChain()
{
    if (model.isLocked())
    {
        setStatus ("Kette ist gesperrt - erst LOCK loesen.", true);
        return;
    }

    model.clear();
    selectedSlot = -1;
    applyChainToProcessor();
}

void FxPage::toggleBypassAll()
{
    model.setChainBypass (! model.isChainBypassed());
    applyChainToProcessor();
}

// ---------------------------------------------------------------------------
// Presets und A/B (B-93/B-94)
// ---------------------------------------------------------------------------
void FxPage::setStatus (const juce::String& text, bool isError)
{
    statusText     = text;
    statusIsError  = isError;

    repaint (statusArea);
}

void FxPage::chooseSavePreset()
{
    auto directory = PresetIO::ensureUserPresetDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Kick-Preset speichern", directory,
                                                  PresetIO::fileWildcard());

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();

                              if (file == juce::File())
                                  return;

                              PresetInfo info;
                              info.styleId = proc.captureState().getStyleId();

                              juce::String error;

                              if (proc.savePreset (file, info, error))
                                  setStatus ("Preset gespeichert: " + PresetIO::withPresetExtension (file).getFileName());
                              else
                                  setStatus ("Fehler: " + error, true);
                          });
}

void FxPage::chooseLoadPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Kick-Preset laden",
                                                  PresetIO::userPresetDirectory(),
                                                  PresetIO::fileWildcard());

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();

                              if (file == juce::File())
                                  return;

                              juce::String error;

                              if (proc.loadPreset (file, error))
                              {
                                  syncFromProcessor();
                                  setStatus ("Preset geladen: " + file.getFileName());
                              }
                              else
                              {
                                  setStatus ("Fehler: " + error, true);
                              }
                          });
}

void FxPage::chooseSaveModulePreset()
{
    const auto* moduleState = model.module (selectedSlot);

    if (moduleState == nullptr)
    {
        setStatus ("Kein Modul gewaehlt.", true);
        return;
    }

    const auto parsed = fxTypeFromString (moduleState->type);
    const auto name = (parsed.has_value() ? fxDisplayName (*parsed) : moduleState->type)
                      + juce::String ("-") + juce::String (selectedSlot + 1);

    chooser = std::make_unique<juce::FileChooser> ("Modul-Preset speichern",
                                                  PresetIO::ensureUserPresetDirectory(),
                                                  PresetIO::fileWildcard());

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, name] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();

                              if (file == juce::File())
                                  return;

                              PresetInfo info;
                              info.name = name;

                              juce::String error;

                              if (proc.saveModulePreset (selectedSlot, file, info, error))
                                  setStatus ("Modul-Preset gespeichert: "
                                             + PresetIO::withPresetExtension (file).getFileName());
                              else
                                  setStatus ("Fehler: " + error, true);
                          });
}

void FxPage::chooseLoadModulePreset()
{
    if (model.module (selectedSlot) == nullptr)
    {
        setStatus ("Kein Modul gewaehlt.", true);
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Modul-Preset laden",
                                                  PresetIO::userPresetDirectory(),
                                                  PresetIO::fileWildcard());

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();

                              if (file == juce::File())
                                  return;

                              juce::String error;

                              if (proc.loadModulePreset (selectedSlot, file, error))
                              {
                                  syncFromProcessor();
                                  setStatus ("Modul-Preset geladen: " + file.getFileName());
                              }
                              else
                              {
                                  setStatus ("Fehler: " + error, true);
                              }
                          });
}

void FxPage::recallOrStoreComparison (int index)
{
    const auto shift = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();

    if (shift || ! proc.hasComparisonState (index))
    {
        proc.storeComparisonState (index);
        setStatus (juce::String (index == 0 ? "A" : "B") + " gemerkt.");
    }
    else if (proc.restoreComparisonState (index))
    {
        syncFromProcessor();
        setStatus (juce::String (index == 0 ? "A" : "B") + " abgerufen.");
    }
    else
    {
        setStatus ("Vergleichszustand nicht verfuegbar.", true);
    }

    updateToolbarLook();
}

void FxPage::toggleLock()
{
    model.setLocked (! model.isLocked());

    setStatus (model.isLocked() ? "Kette gesperrt (nur Regler)." : "Kette entsperrt.");
    updateToolbarLook();
}

void FxPage::applyDefaultChain()
{
    if (model.isLocked())
    {
        setStatus ("Kette ist gesperrt - erst LOCK loesen.", true);
        return;
    }

    model.chain() = FxChainModel::makeDefaultChain();
    selectedSlot = model.size() > 0 ? 0 : -1;

    applyChainToProcessor();
    setStatus ("Werkskette geladen.");
}

void FxPage::updateToolbarLook()
{
    const auto locked = model.isLocked();

    lockButton.setButtonText (locked ? "LOCKED" : "LOCK");
    lockButton.setColour (juce::TextButton::buttonColourId,
                          locked ? ui::theme::warn.withAlpha (0.35f) : ui::theme::panelAlt);

    // A/B: belegt = heller, aktiv = Akzent.
    const auto styleSlot = [this] (juce::TextButton& button, int index)
    {
        const auto hasState = proc.hasComparisonState (index);
        const auto isActive = proc.activeComparisonSlot() == index;

        button.setColour (juce::TextButton::buttonColourId,
                          isActive ? ui::theme::accent.withAlpha (0.35f)
                                   : (hasState ? ui::theme::panel.brighter (0.12f) : ui::theme::panelAlt));
        button.setColour (juce::TextButton::textColourOffId,
                          hasState ? ui::theme::text : ui::theme::textDim);
    };

    styleSlot (slotAButton, 0);
    styleSlot (slotBButton, 1);

    loopButton.setColour (juce::TextButton::buttonColourId,
                          loopEnabled ? ui::theme::ok.withAlpha (0.35f) : ui::theme::panelAlt);
    loopButton.setColour (juce::TextButton::textColourOffId,
                          loopEnabled ? ui::theme::text : ui::theme::textDim);
}

void FxPage::moduleCardMoveRequested (int slot, int delta)
{
    if (! model.moveModule (slot, slot + delta))
        return;

    selectedSlot = juce::jlimit (0, model.size() - 1, slot + delta);
    applyChainToProcessor();
    selectSlot (selectedSlot);
}

void FxPage::moduleCardDuplicateRequested (int slot)
{
    const auto copy = model.duplicateModule (slot);

    if (copy < 0)
        return;

    selectedSlot = copy;
    applyChainToProcessor();
    selectSlot (copy);
}

void FxPage::moduleCardRemoveRequested (int slot)
{
    if (! model.removeModule (slot))
        return;

    selectedSlot = juce::jlimit (-1, model.size() - 1, slot - 1);
    applyChainToProcessor();
}

void FxPage::selectSlot (int slot)
{
    if (! juce::isPositiveAndBelow (slot, (int) cards.size()))
        return;

    selectedSlot = slot;

    const auto& card = cards[(size_t) slot];

    if (viewport.getViewPositionY() > card->getY()
        || viewport.getViewPositionY() + viewport.getHeight() < card->getBottom())
        viewport.setViewPosition (0, juce::jmax (0, card->getY() - 8));

    repaint();
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------
juce::Rectangle<int> FxPage::chainListArea() const
{
    return listArea;
}

void FxPage::resizedSidebar()
{
    // Die Modulliste hat 12 Eintraege; die "+ ADD MODULE"-Knoepfe brauchen
    // deshalb einen eigenen, kompakteren Rhythmus als die Leiste oben.
    constexpr int kAddButtonHeight = 20;
    constexpr int kAddButtonGap    = 2;
    constexpr int kLabelHeight     = 14;
    constexpr int kStatusHeight    = 26;

    const int buttonCount  = addButtons.size();
    const int addHeight    = buttonCount * (kAddButtonHeight + kAddButtonGap);
    const int moduleHeight = 2 * (kButtonHeight + kButtonGap);

    const int contentTop = sidebarBounds.getY() + 26;
    const int left       = sidebarBounds.getX() + 8;
    const int width      = sidebarBounds.getWidth() - 16;

    int bottom = sidebarBounds.getBottom() - 8;

    // unten: "+ ADD MODULE"
    addArea      = { left, bottom - addHeight, width, addHeight };
    addLabelArea = { left, addArea.getY() - kLabelHeight, width, kLabelHeight };
    bottom       = addLabelArea.getY() - 4;

    // darueber: Modul-Presets (SAVE/LOAD)
    modulePresetArea = { left, bottom - moduleHeight, width, moduleHeight };
    moduleLabelArea  = { left, modulePresetArea.getY() - kLabelHeight, width, kLabelHeight };
    bottom           = moduleLabelArea.getY() - 4;

    // darueber: Statuszeile der Preset-Aktionen
    statusArea = { left, bottom - kStatusHeight, width, kStatusHeight };
    bottom     = statusArea.getY() - 4;

    listArea = { left, contentTop, width, juce::jmax (0, bottom - contentTop) };

    int y = addArea.getY();

    for (auto* button : addButtons)
    {
        button->setBounds (addArea.getX(), y, addArea.getWidth(), kAddButtonHeight);
        y += kAddButtonHeight + kAddButtonGap;
    }

    saveModuleButton.setBounds (modulePresetArea.getX(), modulePresetArea.getY(),
                                modulePresetArea.getWidth(), kButtonHeight);
    loadModuleButton.setBounds (modulePresetArea.getX(),
                                modulePresetArea.getY() + kButtonHeight + kButtonGap,
                                modulePresetArea.getWidth(), kButtonHeight);
}

void FxPage::paintSidebar (juce::Graphics& g)
{
    ui::theme::paintPanel (g, sidebarBounds.toFloat(), ui::theme::panel, 8.0f);

    g.setFont (ui::theme::sectionFont());
    g.setColour (ui::theme::textDim);
    g.drawText ("FX CHAIN", sidebarBounds.withHeight (26).reduced (10, 6),
                juce::Justification::centredLeft, false);

    auto list = chainListArea();

    if (model.size() == 0)
    {
        g.setFont (ui::theme::bodyFont());
        g.setColour (ui::theme::textDim);
        g.drawFittedText ("Leer - unten ein Modul hinzufuegen.",
                          list.reduced (10, 6), juce::Justification::topLeft, 3);
        return;
    }

    for (int i = 0; i < model.size(); ++i)
    {
        auto row = list.removeFromTop (kRowHeight);

        if (i == selectedSlot)
        {
            g.setColour (ui::theme::accent.withAlpha (0.22f));
            g.fillRoundedRectangle (row.reduced (4, 1).toFloat(), 4.0f);
        }

        const auto* moduleState = model.module (i);

        if (moduleState == nullptr)
            continue;

        const auto parsed = fxTypeFromString (moduleState->type);
        const auto name = parsed.has_value() ? fxDisplayName (*parsed) : moduleState->type;

        g.setFont (ui::theme::bodyFont());
        g.setColour (moduleState->enabled ? ui::theme::text : ui::theme::textDim);
        g.drawText (juce::String (i + 1) + ".  " + name, row.reduced (10, 1),
                    juce::Justification::centredLeft, true);

        if (! moduleState->enabled)
        {
            g.setColour (ui::theme::textDim);
            g.drawText ("off", row.reduced (10, 1), juce::Justification::centredRight, false);
        }
    }

    g.setFont (ui::theme::sectionFont());
    g.setColour (ui::theme::textDim);
    g.drawText ("+ ADD MODULE", addLabelArea, juce::Justification::centredLeft, false);

    // Modul-Preset-Knoepfe: Beschriftung steht darueber.
    g.drawText ("MODULE PRESET", moduleLabelArea, juce::Justification::centredLeft, false);

    // Statuszeile der Preset-Aktionen (zwei Zeilen, gekuerzt).
    if (statusText.isNotEmpty())
    {
        g.setFont (ui::theme::smallFont());
        g.setColour (statusIsError ? ui::theme::error : ui::theme::textDim);
        g.drawFittedText (statusText, statusArea, juce::Justification::topLeft, 2);
    }
}

void FxPage::mouseDown (const juce::MouseEvent& event)
{
    if (! chainListArea().contains (event.getPosition()))
        return;

    const int index = (event.getPosition().y - chainListArea().getY()) / kRowHeight;

    if (juce::isPositiveAndBelow (index, model.size()))
        selectSlot (index);
}

// ---------------------------------------------------------------------------
// Standard-Component
// ---------------------------------------------------------------------------
void FxPage::paint (juce::Graphics& g)
{
    g.fillAll (ui::theme::background);

    // Toolbar
    auto toolbar = getLocalBounds().removeFromTop (28);
    g.setFont (ui::theme::sectionFont());
    g.setColour (ui::theme::textDim);
    g.drawText ("FX RACK", toolbar.reduced (4, 0), juce::Justification::centredLeft, false);

    // Status rechts: Modulzahl und gemeldete Latenz
    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText (juce::String (proc.getEngineModuleCount()) + " Module  -  Latenz "
                    + juce::String (proc.getLatencySamples()) + " Samples",
                toolbar.withTrimmedRight (8), juce::Justification::centredRight, false);

    paintSidebar (g);
}

void FxPage::resized()
{
    auto area = getLocalBounds();

    auto toolbar = area.removeFromTop (28);
    toolbar.removeFromLeft (80);

    // Presets und Kette links, Demo-Aktionen dahinter.
    saveButton.setBounds (toolbar.removeFromLeft (54).reduced (1));
    loadButton.setBounds (toolbar.removeFromLeft (54).reduced (1));
    slotAButton.setBounds (toolbar.removeFromLeft (30).reduced (1));
    slotBButton.setBounds (toolbar.removeFromLeft (30).reduced (1));
    lockButton.setBounds (toolbar.removeFromLeft (62).reduced (1));
    defaultButton.setBounds (toolbar.removeFromLeft (76).reduced (1));

    toolbar.removeFromLeft (8);

    bypassButton.setBounds (toolbar.removeFromLeft (96).reduced (1));
    clearButton.setBounds (toolbar.removeFromLeft (62).reduced (1));

    toolbar.removeFromLeft (8);

    demoButton.setBounds (toolbar.removeFromLeft (80).reduced (1));
    loopButton.setBounds (toolbar.removeFromLeft (52).reduced (1));

    sidebarBounds = area.removeFromLeft (ui::theme::sidebarWidth).reduced (0, 2);
    resizedSidebar();

    area.removeFromLeft (6);
    viewport.setBounds (area);

    layoutCards();
}

void FxPage::timerCallback()
{
    for (auto& card : cards)
        card->refresh();

    repaint();
}

} // namespace bfka
