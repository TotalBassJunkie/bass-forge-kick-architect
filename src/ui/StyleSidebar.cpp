// B-82: Linke Style-Sidebar der KICK-Seite.
#include "ui/StyleSidebar.h"

#include "library/StyleModel.h"
#include "library/UserStyles.h"
#include "ui/UiTheme.h"

#include <juce_graphics/juce_graphics.h>

namespace bfka
{

namespace
{
    constexpr int kTitleHeight  = 20;
    constexpr int kRollHeight   = 30;
    constexpr int kAddHeight    = 24;
    constexpr int kChaosHeight  = 22;
    constexpr int kRowHeight    = 22;
    constexpr int kLeftInset    = 9;   ///< Platz fuer die Akzentlinie
    constexpr int kRightInset   = 8;
    constexpr int kGap          = 4;
} // namespace

// --- Listenmodell ----------------------------------------------------------

class StyleSidebar::StyleListModel final : public juce::ListBoxModel
{
public:
    explicit StyleListModel (StyleSidebar& ownerToUse) : owner (ownerToUse) {}

    int getNumRows() override { return owner.rowCount(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override
    {
        if (row < 0 || row >= owner.rowCount())
            return;

        const auto isFree = (row == 0);
        const auto isActive = (owner.rowStyleId (row) == owner.activeStyle());

        auto area = juce::Rectangle<int> (0, 0, width, height);

        if (isActive)
        {
            g.setColour (owner.accent().withAlpha (0.22f));
            g.fillRect (area);
            g.setColour (owner.accent());
            g.fillRect (area.getX(), area.getY(), 3, area.getHeight());
        }
        else if (rowIsSelected)
        {
            g.setColour (ui::theme::panel.withAlpha (0.9f));
            g.fillRect (area);
        }

        g.setColour (isActive ? ui::theme::text : ui::theme::textDim);
        g.setFont (isFree ? ui::theme::cardTitleFont() : ui::theme::bodyFont());
        g.drawText (owner.rowName (row), area.reduced (6, 0),
                    juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        chooseRow (row);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        chooseRow (row);
    }

private:
    /** Klick/Doubleclick: uebernimmt die Zeile und meldet sie nach aussen.
        Die Auswahl selbst hat der ListBox bereits gesetzt; ein zusaetzliches
        selectRow() aus dem Callback heraus waere re-entrant. */
    void chooseRow (int row)
    {
        if (row < 0 || row >= owner.rowCount())
            return;

        owner.activeId = owner.rowStyleId (row);
        owner.list.repaint();
        owner.repaint();

        if (owner.onStyleSelected)
            owner.onStyleSelected (owner.activeId);
    }

    StyleSidebar& owner;
};

// --- StyleSidebar ----------------------------------------------------------

StyleSidebar::StyleSidebar()
{
    listModel = std::make_unique<StyleListModel> (*this);
    list.setModel (listModel.get());
    list.setRowHeight (kRowHeight);
    list.setColour (juce::ListBox::backgroundColourId, ui::theme::panelAlt);
    list.setColour (juce::ListBox::outlineColourId, ui::theme::grid);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    rollButton.onClick = [this]
    {
        if (onRandomizeRequested)
            onRandomizeRequested();
    };
    addAndMakeVisible (rollButton);

    // "+ ADD STYLE" hat (noch) keinen Callback im Vertrag; der Knopf bleibt sichtbar.
    addAndMakeVisible (addStyleButton);

    auto setupChaos = [this] (juce::TextButton& button, bool ChaosFlags::* flag)
    {
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonColourId, ui::theme::panelDeep);
        button.setColour (juce::TextButton::buttonOnColourId, ui::theme::accent);
        button.setColour (juce::TextButton::textColourOffId, ui::theme::textDim);
        button.setColour (juce::TextButton::textColourOnId, ui::theme::background);

        button.setToggleState (chaosFlags.*flag, juce::dontSendNotification);

        auto* buttonPtr = &button;   // Zeiger, nicht Referenz: der Callback lebt laenger.

        button.onClick = [this, buttonPtr, flag]
        {
            chaosFlags.*flag = buttonPtr->getToggleState();

            if (onChaosToggled)
                onChaosToggled (chaosFlags);

            repaint();
        };

        addAndMakeVisible (button);
    };

    setupChaos (chaosPitch,   &ChaosFlags::pitch);
    setupChaos (chaosStretch, &ChaosFlags::stretch);
    setupChaos (chaosChain,   &ChaosFlags::fxChain);
    setupChaos (chaosModules, &ChaosFlags::fxModules);

    addStyleButton.setColour (juce::TextButton::buttonColourId, ui::theme::panelDeep);
    addStyleButton.setColour (juce::TextButton::textColourOffId, ui::theme::textDim);

    rollButton.setColour (juce::TextButton::buttonColourId, ui::theme::accent);
    rollButton.setColour (juce::TextButton::textColourOffId, ui::theme::background);
}

StyleSidebar::~StyleSidebar()
{
    // Das Listenmodell haelt eine Referenz auf *this; vor dem Abbau abhaengen.
    // listModel selbst wird danach automatisch abgebaut (Mitglieder in
    // umgekehrter Reihenfolge, also erst `list`).
    list.setModel (nullptr);
}

void StyleSidebar::setModel (const StyleModel* model, const UserStyles* userStyles)
{
    styleIds.clear();
    styleNames.clear();

    if (model != nullptr)
    {
        for (const auto& styleDef : model->styles())
        {
            styleIds.push_back (styleDef.id);
            styleNames.push_back (styleDef.display.isNotEmpty() ? styleDef.display
                                                                : styleDef.id);
        }
    }

    if (userStyles != nullptr)
    {
        for (const auto& userStyle : userStyles->entries())
        {
            styleIds.push_back (userStyle.id);
            styleNames.push_back (userStyle.name.isNotEmpty() ? userStyle.name
                                                              : userStyle.id);
        }
    }

    if (listModel != nullptr)
        list.updateContent();

    list.repaint();
    repaint();
}

void StyleSidebar::setActiveStyle (const juce::String& styleId)
{
    activeId = styleId;

    int row = 0;

    if (styleId.isNotEmpty())
    {
        row = -1;

        for (int i = 0; i < (int) styleIds.size(); ++i)
        {
            if (styleIds[(size_t) i] == styleId)
            {
                row = i + 1;
                break;
            }
        }
    }

    // Auswahl ohne Callback: selectRow() meldet nichts nach aussen.
    if (row >= 0 && row < rowCount())
        list.selectRow (row);
    else
        list.deselectAllRows();

    repaint();
}

juce::String StyleSidebar::activeStyle() const
{
    return activeId;
}

void StyleSidebar::setAccentFor (juce::Colour accent)
{
    accentColour = accent;
    list.setColour (juce::ListBox::backgroundColourId, ui::theme::panelAlt);
    repaint();
}

juce::Colour StyleSidebar::accent() const
{
    return accentColour;
}

void StyleSidebar::setRollCount (int count)
{
    rollCounter = juce::jmax (0, count);
    repaint();
}

int StyleSidebar::rollCount() const noexcept
{
    return rollCounter;
}

int StyleSidebar::rowCount() const
{
    return 1 + (int) styleIds.size();
}

juce::String StyleSidebar::rowName (int row) const
{
    if (row == 0)
        return "FREE BUILD";

    const auto index = row - 1;

    if (index >= 0 && index < (int) styleNames.size())
        return styleNames[(size_t) index];

    return {};
}

juce::String StyleSidebar::rowStyleId (int row) const
{
    if (row <= 0)
        return {};

    const auto index = row - 1;

    if (index < (int) styleIds.size())
        return styleIds[(size_t) index];

    return {};
}

int StyleSidebar::preferredWidth() const noexcept
{
    return ui::theme::sidebarWidth;
}

void StyleSidebar::paint (juce::Graphics& g)
{
    g.fillAll (ui::theme::panelAlt);

    // Linke Akzentlinie.
    g.setColour (accentColour);
    g.fillRect (0, 0, 3, getHeight());

    auto r = getLocalBounds();
    r.removeFromLeft (kLeftInset);
    r.removeFromRight (kRightInset);
    r.removeFromTop (6);

    g.setColour (ui::theme::text);
    g.setFont (ui::theme::sectionFont());
    g.drawText ("STYLE", r.removeFromTop (kTitleHeight),
                juce::Justification::centredLeft, false);

    // Roll-Zaehler unten rechts, mit eigenem Hintergrund gegen die Liste.
    const auto footer = juce::Rectangle<int> (getWidth() - kRightInset - 70,
                                              getHeight() - 18, 70, 16);
    g.setColour (ui::theme::panelAlt);
    g.fillRect (footer);
    g.setColour (ui::theme::textDim);
    g.setFont (ui::theme::smallFont());
    g.drawText ("ROLLS " + juce::String (rollCounter), footer,
                juce::Justification::centredRight, false);
}

void StyleSidebar::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (kLeftInset);
    r.removeFromRight (kRightInset);
    r.removeFromTop (6);

    r.removeFromTop (kTitleHeight);   // Titel wird in paint() gezeichnet
    r.removeFromTop (kGap);

    rollButton.setBounds (r.removeFromTop (kRollHeight));
    r.removeFromTop (kGap);
    addStyleButton.setBounds (r.removeFromTop (kAddHeight));
    r.removeFromTop (kGap + 2);

    const auto halfWidth = (r.getWidth() - kGap) / 2;

    auto chaosRowOne = r.removeFromTop (kChaosHeight);
    chaosPitch.setBounds (chaosRowOne.removeFromLeft (halfWidth));
    chaosStretch.setBounds (chaosRowOne.removeFromRight (halfWidth));

    r.removeFromTop (kGap);

    auto chaosRowTwo = r.removeFromTop (kChaosHeight);
    chaosChain.setBounds (chaosRowTwo.removeFromLeft (halfWidth));
    chaosModules.setBounds (chaosRowTwo.removeFromRight (halfWidth));

    r.removeFromTop (kGap + 2);

    // Rest fuer die Liste; der Roll-Zaehler liegt unten rechts darueber.
    list.setBounds (r.withTrimmedBottom (18));
}

} // namespace bfka
