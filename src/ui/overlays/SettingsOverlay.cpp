// B-101 - Settings-Overlay mit acht Sektionen.

#include "ui/overlays/SettingsOverlay.h"

#include "ui/UiTheme.h"

#include <utility>

namespace bfka
{

SettingsOverlay::SettingsOverlay (Prefs& prefsToUse)
    : prefs (prefsToUse)
{
    const auto& names = Prefs::sections();

    if (names.size() > 0)
        activeSection = names[0];

    setWantsKeyboardFocus (true);
}

void SettingsOverlay::setRows (std::vector<SettingsRow> newRows)
{
    rows = std::move (newRows);

    // Die aktive Sektion bleibt, solange es sie gibt; sonst die erste.
    if (rowCountForSection (activeSection) == 0 && ! rows.empty())
        activeSection = rows.front().section;

    updateLayout();
}

int SettingsOverlay::rowCount() const
{
    return (int) rows.size();
}

int SettingsOverlay::rowCountForSection (const juce::String& section) const
{
    int count = 0;

    for (const auto& row : rows)
        if (row.section == section)
            ++count;

    return count;
}

juce::String SettingsOverlay::sectionAt (int row) const
{
    if (row < 0 || row >= (int) rows.size())
        return {};

    return rows[(size_t) row].section;
}

juce::String SettingsOverlay::keyAt (int row) const
{
    if (row < 0 || row >= (int) rows.size())
        return {};

    return rows[(size_t) row].key;
}

void SettingsOverlay::setRowValue (int row, const juce::var& value)
{
    if (row < 0 || row >= (int) rows.size())
        return;

    prefs.set (rows[(size_t) row].key, value);
    repaint();
}

juce::var SettingsOverlay::rowValue (int row) const
{
    if (row < 0 || row >= (int) rows.size())
        return {};

    return prefs.get (rows[(size_t) row].key);
}

void SettingsOverlay::setActiveSection (const juce::String& section)
{
    if (section == activeSection)
        return;

    activeSection = section;
    updateLayout();
    repaint();
}

std::vector<int> SettingsOverlay::visibleRowIndices() const
{
    std::vector<int> indices;

    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].section == activeSection)
            indices.push_back (i);

    return indices;
}

juce::String SettingsOverlay::displayValue (const SettingsRow& row) const
{
    const auto value = prefs.get (row.key);

    if (row.type == "bool")
        return value.isVoid() ? juce::String ("Off")
                              : juce::String ((bool) value ? "On" : "Off");

    if (value.isVoid())
        return {};

    return value.toString();
}

void SettingsOverlay::updateLayout()
{
    const auto area = getLocalBounds();

    titleArea   = area.withHeight (kTitleHeight).reduced (kPadding, 0);
    sidebarArea = area.withTrimmedTop (kTitleHeight).withWidth (kSidebarWidth)
                      .reduced (kPadding, kPadding);
    rowsArea    = area.withTrimmedTop (kTitleHeight).withTrimmedLeft (kSidebarWidth)
                      .reduced (kPadding, kPadding);

    sectionBounds.clear();

    auto sidebar = sidebarArea;
    const auto& names = Prefs::sections();

    for (int i = 0; i < names.size(); ++i)
    {
        sectionBounds.push_back (sidebar.removeFromTop (kSectionH));
        sidebar.removeFromTop (2);
    }

    rowBounds.clear();

    auto list = rowsArea;
    const auto visible = visibleRowIndices();

    for (size_t i = 0; i < visible.size(); ++i)
    {
        rowBounds.push_back (list.removeFromTop (kRowHeight));
        list.removeFromTop (kRowGap);
    }
}

void SettingsOverlay::resized()
{
    updateLayout();
}

void SettingsOverlay::paint (juce::Graphics& g)
{
    using namespace ui::theme;

    g.fillAll (background);
    paintPanel (g, getLocalBounds().toFloat().reduced (6.0f), panel, 10.0f);

    // Titel
    g.setColour (text);
    g.setFont (titleFont());
    g.drawText ("SETTINGS", titleArea.withTrimmedLeft (kPadding), juce::Justification::centredLeft, false);

    // Sektionsliste links
    const auto& names = Prefs::sections();

    for (int i = 0; i < (int) sectionBounds.size() && i < names.size(); ++i)
    {
        const auto& bounds = sectionBounds[(size_t) i];
        const auto isActive = names[i] == activeSection;

        if (isActive)
        {
            g.setColour (panelAlt);
            g.fillRoundedRectangle (bounds.toFloat(), 5.0f);
        }

        g.setColour (isActive ? accent : textDim);
        g.setFont (cardTitleFont());
        g.drawText (names[i], bounds.reduced (8, 0), juce::Justification::centredLeft, false);
    }

    // Trennlinie zwischen Liste und Zeilen
    g.setColour (grid);
    const auto dividerX = (float) (sidebarArea.getRight() + kPadding / 2);
    g.drawLine (dividerX, (float) rowsArea.getY(), dividerX, (float) rowsArea.getBottom(), 1.0f);

    // Zeilen der aktiven Sektion
    const auto visible = visibleRowIndices();

    if (visible.empty())
    {
        g.setColour (textDim);
        g.setFont (bodyFont());
        g.drawText ("Keine Eintraege in dieser Sektion.",
                    rowsArea, juce::Justification::centredTop, false);
        return;
    }

    for (size_t i = 0; i < visible.size() && i < rowBounds.size(); ++i)
    {
        const auto& row = rows[(size_t) visible[i]];
        const auto& bounds = rowBounds[i];

        paintPanel (g, bounds.toFloat(), panelAlt, 5.0f);

        g.setColour (text);
        g.setFont (bodyFont());
        g.drawText (row.label, bounds.reduced (10, 0).withTrimmedRight (bounds.getWidth() / 2),
                    juce::Justification::centredLeft, false);

        g.setColour (accent);
        g.drawText (displayValue (row), bounds.reduced (10, 0).withTrimmedLeft (bounds.getWidth() / 2),
                    juce::Justification::centredRight, false);
    }
}

void SettingsOverlay::mouseDown (const juce::MouseEvent& event)
{
    const auto position = event.getPosition();
    const auto& names = Prefs::sections();

    for (int i = 0; i < (int) sectionBounds.size() && i < names.size(); ++i)
    {
        if (sectionBounds[(size_t) i].contains (position))
        {
            setActiveSection (names[i]);
            return;
        }
    }
}

bool SettingsOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress (juce::KeyPress::escapeKey))
    {
        if (onCloseRequested != nullptr)
            onCloseRequested();

        return true;
    }

    return false;
}

} // namespace bfka
