// B-100 - Library-Overlay: vier Tabs, Suche, Vorschau, Favoriten.
#include "ui/overlays/LibraryOverlay.h"

#include "ui/UiTheme.h"

#include <cmath>
#include <utility>

namespace bfka
{

namespace
{
    constexpr int kTabBarHeight    = 26;
    constexpr int kSearchHeight    = 24;
    constexpr int kOuterInset      = 14;
    constexpr int kTitleReserve    = 120;
    constexpr int kTabGap          = 6;
    constexpr int kRowHeight       = 34;

    /** Fuenfzackiger Stern fuer den Favoriten-Marker. */
    juce::Path makeStar (juce::Rectangle<float> area)
    {
        juce::Path p;
        const auto c  = area.getCentre();
        const auto r  = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const auto ri = r * 0.45f;

        for (int i = 0; i < 10; ++i)
        {
            const auto radius = (i % 2 == 0) ? r : ri;
            const auto angle  = juce::MathConstants<float>::pi * 0.5f
                              + (float) i * juce::MathConstants<float>::pi / 5.0f;
            const juce::Point<float> pt { c.x + std::sin (angle) * radius,
                                          c.y - std::cos (angle) * radius };

            if (i == 0)
                p.startNewSubPath (pt);
            else
                p.lineTo (pt);
        }

        p.closeSubPath();
        return p;
    }
} // namespace

// ---------------------------------------------------------------------------
// Listenmodell
// ---------------------------------------------------------------------------

class LibraryOverlay::ListModel final : public juce::ListBoxModel
{
public:
    explicit ListModel (LibraryOverlay& o) : owner (o) {}

    int getNumRows() override
    {
        return (int) owner.visible.size();
    }

    void paintListBoxItem (int rowNumber, juce::Graphics& g,
                           int width, int height, bool rowIsSelected) override
    {
        owner.paintRow (rowNumber, g, width, height, rowIsSelected);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow (row, (int) owner.visible.size()))
        {
            owner.selected = row;
            owner.list.repaint();
        }
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (juce::isPositiveAndBelow (row, (int) owner.visible.size()))
        {
            owner.selected = row;
            owner.list.repaint();

            if (owner.onItemActivated)
                owner.onItemActivated (owner.visible[(size_t) row].payload);
        }
    }

private:
    LibraryOverlay& owner;
};

// ---------------------------------------------------------------------------
// Aufbau
// ---------------------------------------------------------------------------

LibraryOverlay::LibraryOverlay()
{
    static const char* const tabNames[4] = { "PRESETS", "SOUNDS", "FX CHAINS", "MY SOUNDS" };

    for (int i = 0; i < 4; ++i)
    {
        auto& b = tabButtons[i];
        b.setButtonText (tabNames[i]);
        b.setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
        b.setColour (juce::TextButton::buttonOnColourId, ui::theme::accent);
        b.setColour (juce::TextButton::textColourOffId, ui::theme::textDim);
        b.setColour (juce::TextButton::textColourOnId, ui::theme::background);
        b.onClick = [this, i] { setTab ((Tab) i); };
        addAndMakeVisible (b);
    }

    searchBox.setMultiLine (false);
    searchBox.setReturnKeyStartsNewLine (false);
    searchBox.setColour (juce::TextEditor::backgroundColourId, ui::theme::panelDeep);
    searchBox.setColour (juce::TextEditor::textColourId, ui::theme::text);
    searchBox.setColour (juce::TextEditor::outlineColourId, ui::theme::grid);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, ui::theme::accent);
    searchBox.setTextToShowWhenEmpty ("Suchen...", ui::theme::textDim);
    searchBox.onTextChange = [this]
    {
        search = searchBox.getText();
        rebuildFilter();
    };
    addAndMakeVisible (searchBox);

    listModel = std::make_unique<ListModel> (*this);
    list.setModel (listModel.get());
    list.setRowHeight (kRowHeight);
    list.setColour (juce::ListBox::backgroundColourId, ui::theme::panelDeep);
    list.setColour (juce::ListBox::outlineColourId, ui::theme::grid);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    selectTab (Tab::presets);
    rebuildFilter();
}

LibraryOverlay::~LibraryOverlay()
{
    list.setModel (nullptr);
}

// ---------------------------------------------------------------------------
// Daten
// ---------------------------------------------------------------------------

void LibraryOverlay::setItems (Tab tab, std::vector<LibraryItem> items)
{
    itemsByTab[(size_t) tab] = std::move (items);
    rebuildFilter();
}

void LibraryOverlay::setTab (Tab tab)
{
    selectTab (tab);
    rebuildFilter();
}

LibraryOverlay::Tab LibraryOverlay::tab() const noexcept
{
    return currentTab;
}

void LibraryOverlay::setSearchText (const juce::String& text)
{
    search = text;

    if (searchBox.getText() != text)
        searchBox.setText (text, juce::dontSendNotification);

    rebuildFilter();
}

juce::String LibraryOverlay::searchText() const
{
    return search;
}

void LibraryOverlay::selectTab (Tab tab)
{
    const int idx = juce::jlimit (0, 3, (int) tab);
    currentTab = (Tab) idx;

    for (int i = 0; i < 4; ++i)
        tabButtons[i].setToggleState (i == idx, juce::dontSendNotification);
}

void LibraryOverlay::rebuildFilter()
{
    visible.clear();
    visibleSource.clear();

    const auto& source = itemsByTab[(size_t) currentTab];
    const auto  needle = search.trim();
    const bool  all    = needle.isEmpty();

    for (int i = 0; i < (int) source.size(); ++i)
    {
        const auto& item = source[(size_t) i];

        if (all
            || item.name.containsIgnoreCase (needle)
            || item.subtitle.containsIgnoreCase (needle))
        {
            visible.push_back (item);
            visibleSource.push_back (i);
        }
    }

    // Spiegel der sichtbaren Namen je Tab (Feld aus dem Vertrag), damit ein
    // Tabwechsel die letzte Trefferliste nachvollziehbar haelt.
    auto& names = filters[(size_t) currentTab];
    names.clear();

    for (const auto& item : visible)
        names.add (item.name);

    selected = visible.empty() ? -1 : 0;

    list.updateContent();

    if (selected >= 0)
        list.selectRow (selected, false, true);
    else
        list.deselectAllRows();

    list.repaint();
    repaint();
}

int LibraryOverlay::visibleCount() const
{
    return (int) visible.size();
}

juce::String LibraryOverlay::visibleName (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) visible.size()))
        return {};

    return visible[(size_t) index].name;
}

juce::String LibraryOverlay::previewText() const
{
    if (! juce::isPositiveAndBelow (selected, (int) visible.size()))
        return {};

    return visible[(size_t) selected].subtitle;
}

void LibraryOverlay::toggleFavourite (int visibleIndex)
{
    if (! juce::isPositiveAndBelow (visibleIndex, (int) visible.size()))
        return;

    // Schreibender Zugriff auf die Quelle, damit der Zustand Tab- und Filter-
    // wechsel ueberlebt.
    const auto sourceIndex = visibleSource[(size_t) visibleIndex];
    auto& item = itemsByTab[(size_t) currentTab][(size_t) sourceIndex];

    item.favourite = ! item.favourite;
    visible[(size_t) visibleIndex].favourite = item.favourite;

    list.repaintRow (visibleIndex);
}

bool LibraryOverlay::isFavourite (int visibleIndex) const
{
    if (! juce::isPositiveAndBelow (visibleIndex, (int) visible.size()))
        return false;

    return visible[(size_t) visibleIndex].favourite;
}

// ---------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------

void LibraryOverlay::resized()
{
    auto area = getLocalBounds().reduced (kOuterInset, kOuterInset - 2);

    headerArea = area.removeFromTop (kTabBarHeight);

    auto buttonRow = headerArea.withTrimmedLeft (kTitleReserve);
    const int buttonW = juce::jmax (40, (buttonRow.getWidth() - 3 * kTabGap) / 4);

    for (int i = 0; i < 4; ++i)
    {
        tabButtons[i].setBounds (buttonRow.removeFromLeft (buttonW));

        if (i < 3)
            buttonRow.removeFromLeft (kTabGap);
    }

    area.removeFromTop (8);

    searchArea = area.removeFromTop (kSearchHeight)
                     .withWidth (juce::jmin (360, area.getWidth()));
    searchBox.setBounds (searchArea);

    separatorY = searchArea.getBottom() + 9;
    area.removeFromTop (18);

    auto content = area;
    auto gap = 12;

    listArea    = content.removeFromLeft (juce::roundToInt (content.getWidth() * 0.6f) - gap / 2);
    content.removeFromLeft (gap);
    previewArea = content;

    list.setBounds (listArea);
}

void LibraryOverlay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (ui::theme::panel);
    g.fillRect (bounds);

    g.setColour (ui::theme::grid);
    g.drawRect (bounds, 1.0f);

    // Titel
    g.setColour (ui::theme::text);
    g.setFont (ui::theme::titleFont());
    g.drawText ("LIBRARY", headerArea.withWidth (kTitleReserve).withHeight (kTabBarHeight),
                juce::Justification::centredLeft, false);

    // Trennlinie unter dem Suchfeld
    g.setColour (ui::theme::grid);
    g.drawHorizontalLine (separatorY, (float) kOuterInset,
                          (float) (getWidth() - kOuterInset));

    // Vorschauflaeche
    ui::theme::paintPanel (g, previewArea.toFloat(), ui::theme::panelAlt, 6.0f);

    g.setColour (ui::theme::textDim);
    auto preview = previewArea.reduced (12, 10);

    const bool hasSelection = juce::isPositiveAndBelow (selected, (int) visible.size());
    const juce::String header = hasSelection ? visible[(size_t) selected].name
                                             : juce::String ("Keine Auswahl");

    g.setColour (hasSelection ? ui::theme::text : ui::theme::textDim);
    g.setFont (ui::theme::cardTitleFont());
    g.drawText (header, preview.removeFromTop (22), juce::Justification::centredLeft, true);

    g.setColour (ui::theme::textDim);
    g.setFont (ui::theme::bodyFont());
    g.drawFittedText (previewText(), preview, juce::Justification::topLeft, 10);
}

void LibraryOverlay::paintRow (int rowNumber, juce::Graphics& g, int width, int height,
                               bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, (int) visible.size()))
        return;

    const auto& item = visible[(size_t) rowNumber];
    auto row = juce::Rectangle<int> (0, 0, width, height);

    if (rowIsSelected)
    {
        g.setColour (ui::theme::accent.withAlpha (0.18f));
        g.fillRect (row);
    }

    auto starArea = row.removeFromRight (height).toFloat().reduced (9.0f);

    if (item.favourite)
    {
        g.setColour (ui::theme::warn);
        g.fillPath (makeStar (starArea));
    }

    auto text = row.reduced (10, 2);

    g.setColour (rowIsSelected ? ui::theme::text : ui::theme::text.brighter (0.1f));
    g.setFont (ui::theme::cardTitleFont());
    g.drawText (item.name, text.removeFromTop (height / 2 - 1),
                juce::Justification::centredLeft, true);

    g.setColour (ui::theme::textDim);
    g.setFont (ui::theme::smallFont());
    g.drawText (item.subtitle, text, juce::Justification::centredLeft, true);

    g.setColour (ui::theme::grid);
    g.drawHorizontalLine (height - 1, 6.0f, (float) width - 6.0f);
}

// ---------------------------------------------------------------------------
// Eingaben
// ---------------------------------------------------------------------------

bool LibraryOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (onCloseRequested)
            onCloseRequested();

        return true;
    }

    return false;
}

} // namespace bfka
