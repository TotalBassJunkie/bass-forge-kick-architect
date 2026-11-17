#include "ui/FxPage.h"

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
} // namespace

FxPage::FxPage (PluginProcessor& processor)
    : proc (processor)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&cardHolder, false);
    viewport.setScrollBarsShown (true, false);

    bypassButton.setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
    bypassButton.setColour (juce::TextButton::textColourOffId, ui::theme::text);
    bypassButton.onClick = [this] { toggleBypassAll(); };
    addAndMakeVisible (bypassButton);

    clearButton.setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
    clearButton.setColour (juce::TextButton::textColourOffId, ui::theme::text);
    clearButton.onClick = [this] { clearChain(); };
    addAndMakeVisible (clearButton);

    demoButton.setColour (juce::TextButton::buttonColourId, ui::theme::accent.withAlpha (0.25f));
    demoButton.setColour (juce::TextButton::textColourOffId, ui::theme::text);
    demoButton.onClick = [this]
    {
        // Baut die Demokette (Multiband + optional Convolver + Dynamic EQ) und
        // startet sie einmal, damit man den Unterschied sofort hoert.
        proc.loadFxDemoChain();
        syncFromProcessor();
        proc.triggerKick();
    };
    addAndMakeVisible (demoButton);

    loopButton.setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
    loopButton.setColour (juce::TextButton::textColourOffId, ui::theme::textDim);
    loopButton.onClick = [this]
    {
        loopEnabled = ! loopEnabled;
        lastTriggerMs = juce::Time::getMillisecondCounterHiRes();
        loopButton.setColour (juce::TextButton::buttonColourId,
                              loopEnabled ? ui::theme::ok.withAlpha (0.35f) : ui::theme::panelAlt);
        loopButton.setColour (juce::TextButton::textColourOffId,
                              loopEnabled ? ui::theme::text : ui::theme::textDim);
    };
    addAndMakeVisible (loopButton);

    // `+ ADD MODULE`: ein Knopf je registriertem Modultyp.
    for (int i = 0; i < kNumFxTypes; ++i)
    {
        const auto type = (FxType) i;

        if (! FxFactory::get().isRegistered (type))
            continue;

        auto* button = addButtons.add (new juce::TextButton ("+ " + fxDisplayName (type)));
        button->setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
        button->setColour (juce::TextButton::textColourOffId, ui::theme::text);
        button->onClick = [this, type] { addModule (type); };
        addAndMakeVisible (*button);
    }

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
        return;

    selectedSlot = index;
    applyChainToProcessor();
    selectSlot (index);
}

void FxPage::clearChain()
{
    model.clear();
    selectedSlot = -1;
    applyChainToProcessor();
}

void FxPage::toggleBypassAll()
{
    model.setChainBypass (! model.isChainBypassed());
    applyChainToProcessor();
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
    const int buttonCount = addButtons.size();
    const int height = buttonCount * (kButtonHeight + kButtonGap);

    const int contentTop    = sidebarBounds.getY() + 26;
    const int addAreaTop    = sidebarBounds.getBottom() - height - 8;

    listArea = { sidebarBounds.getX() + 8, contentTop,
                 sidebarBounds.getWidth() - 16, juce::jmax (0, addAreaTop - 26 - contentTop) };

    addArea = { sidebarBounds.getX() + 8, addAreaTop,
                sidebarBounds.getWidth() - 16, height };

    int y = addArea.getY();

    for (auto* button : addButtons)
    {
        button->setBounds (addArea.getX(), y, addArea.getWidth(), kButtonHeight);
        y += kButtonHeight + kButtonGap;
    }
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
    g.drawText ("+ ADD MODULE", addArea.withY (addArea.getY() - 22).withHeight (20),
                juce::Justification::centredLeft, false);
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
    g.drawText (juce::String (proc.getEngineModuleCount()) + " Module  ·  Latenz "
                    + juce::String (proc.getLatencySamples()) + " Samples",
                toolbar.withTrimmedRight (8), juce::Justification::centredRight, false);

    paintSidebar (g);
}

void FxPage::resized()
{
    auto area = getLocalBounds();

    auto toolbar = area.removeFromTop (28);
    toolbar.removeFromLeft (80);

    bypassButton.setBounds (toolbar.removeFromLeft (110).reduced (1));
    clearButton.setBounds (toolbar.removeFromLeft (70).reduced (1));
    demoButton.setBounds (toolbar.removeFromLeft (86).reduced (1));
    loopButton.setBounds (toolbar.removeFromLeft (56).reduced (1));

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
