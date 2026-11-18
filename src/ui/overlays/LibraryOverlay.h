// B-100 - Library-Overlay mit vier Tabs, Suche, Vorschau und Favoriten.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace bfka
{

/** Ein Eintrag im Library-Overlay (Ticket B-100). */
struct LibraryItem
{
    juce::String name;
    juce::String subtitle;      ///< "woraus besteht der Preset"
    juce::String payload;       ///< Pfad/Id, was beim Doppelklick geliefert wird
    bool favourite = false;
};

/**
 * Vollflaechiges Library-Overlay (Ticket B-100).
 * Vier Tabs, Suchfeld, Trefferliste, Vorschau des gewaehlten Eintrags
 * und Favoriten-Stern. Liegt als Kind im OverlayHost.
 */
class LibraryOverlay final : public juce::Component
{
public:
    enum class Tab { presets = 0, sounds, fxChains, mySounds };

    LibraryOverlay();
    ~LibraryOverlay() override;

    /** Setzt die Eintraege je Tab (Index = Tab). */
    void setItems (Tab tab, std::vector<LibraryItem> items);

    /** Aktiver Tab. */
    void setTab (Tab tab);
    Tab  tab() const noexcept;

    /** Suchtext (filtert Name und Untertitel, case-insensitiv). */
    void setSearchText (const juce::String& text);
    juce::String searchText() const;

    /** Anzahl der sichtbaren (gefilterten) Eintraege im aktiven Tab. */
    int visibleCount() const;
    /** Name des sichtbaren Eintrags `index` (0-basiert). */
    juce::String visibleName (int index) const;

    /** Vorschau-Text (Untertitel) des gewaehlten Eintrags. */
    juce::String previewText() const;

    void toggleFavourite (int visibleIndex);
    bool isFavourite (int visibleIndex) const;

    std::function<void (const juce::String& payload)> onItemActivated;   // Doppelklick
    std::function<void()> onCloseRequested;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    class ListModel;

    void rebuildFilter();
    void selectTab (Tab tab);
    void paintRow (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected);

    std::array<std::vector<LibraryItem>, 4> itemsByTab;
    std::array<juce::StringArray, 4> filters;
    std::vector<LibraryItem> visible;        ///< gefilterte Eintraege des aktiven Tabs
    std::vector<int> visibleSource;          ///< Quellindex in itemsByTab je sichtbarem Eintrag
    Tab currentTab = Tab::presets;
    juce::String search;
    int selected = -1;
    juce::TextButton tabButtons[4];
    juce::TextEditor searchBox;
    juce::ListBox    list { "Library", nullptr };
    std::unique_ptr<juce::ListBoxModel> listModel;

    // Flaechen aus resized(), damit paint() dieselbe Aufteilung benutzt.
    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> searchArea;
    juce::Rectangle<int> listArea;
    juce::Rectangle<int> previewArea;
    int separatorY = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LibraryOverlay)
};

} // namespace bfka
