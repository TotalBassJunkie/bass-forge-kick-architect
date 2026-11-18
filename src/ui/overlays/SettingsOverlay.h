// B-101 - Settings-Overlay mit acht Sektionen.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "core/Prefs.h"

namespace bfka
{

/** Eine Zeile im Settings-Overlay. */
struct SettingsRow
{
    juce::String section;
    juce::String key;
    juce::String label;
    juce::String type; // "bool" | "number" | "text"
};

/**
 * Settings-Seite als Overlay (Ticket B-101).
 *
 * Links stehen die acht Sektionen aus Prefs::sections(), rechts die Zeilen
 * der aktiven Sektion mit Label und Wert. Kein Modal-Loop: das Overlay wird
 * vom OverlayHost gestapelt, Escape meldet nur den Schliesswunsch.
 */
class SettingsOverlay final : public juce::Component
{
public:
    explicit SettingsOverlay (Prefs& prefsToUse);

    void setRows (std::vector<SettingsRow> rows);
    int  rowCount() const;
    int  rowCountForSection (const juce::String& section) const;

    juce::String sectionAt (int row) const;
    juce::String keyAt (int row) const;

    /** Setzt eine Zeile auf einen Wert (schreibt in Prefs). */
    void setRowValue (int row, const juce::var& value);
    juce::var rowValue (int row) const;

    std::function<void()> onCloseRequested;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void updateLayout();
    void setActiveSection (const juce::String& section);
    std::vector<int> visibleRowIndices() const;
    juce::String displayValue (const SettingsRow& row) const;

    Prefs& prefs;
    std::vector<SettingsRow> rows;
    juce::String activeSection;

    juce::Rectangle<int> titleArea;
    juce::Rectangle<int> sidebarArea;
    juce::Rectangle<int> rowsArea;
    std::vector<juce::Rectangle<int>> sectionBounds;
    std::vector<juce::Rectangle<int>> rowBounds;

    static constexpr int kPadding      = 16;
    static constexpr int kTitleHeight  = 38;
    static constexpr int kSidebarWidth = 150;
    static constexpr int kSectionH     = 28;
    static constexpr int kRowHeight    = 26;
    static constexpr int kRowGap       = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsOverlay)
};

} // namespace bfka
