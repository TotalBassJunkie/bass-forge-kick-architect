#pragma once

// B-103: "+ ADD PART"-Picker (zweistufig, Gruppensymbole, "NO FREE SLOT").
// B-104: "+ ADD STYLE"-Overlay (Name, Basis-Style, Akzentfarbe).

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace bfka
{

/** Ein Eintrag im ADD-PART-Picker (Ticket B-103). */
struct AddPartItem
{
    juce::String id;        ///< Pool-Id, wird beim Waehlen geliefert
    juce::String name;      ///< "Punch Techno"
    juce::String symbol;    ///< 1-2 Zeichen, z. B. "PU"
    int          freeSlots = 0;   ///< freie Karten, 0 = nicht waehlbar
};

/** Eine Gruppe (Rolle) im Picker. */
struct AddPartGroup
{
    juce::String name;                     ///< "PUNCH"
    std::vector<AddPartItem> items;
};

/**
 * Zweistufiger "+ ADD PART"-Picker (Ticket B-103).
 * Stufe 1: Gruppen mit Symbol; Stufe 2: Eintraege der Gruppe.
 * Ist kein Platz frei, zeigt der Picker "NO FREE SLOT" und waehlt nichts.
 */
class AddPartOverlay final : public juce::Component
{
public:
    AddPartOverlay();
    ~AddPartOverlay() override;

    void setGroups (std::vector<AddPartGroup> groups);
    int  groupCount() const;
    juce::String groupName (int index) const;

    /** 0 = Gruppenliste, 1 = Eintraege der gewaehlten Gruppe. */
    int  stage() const noexcept;
    void selectGroup (int index);
    int  selectedGroup() const noexcept;
    /** Gruppenindex wieder verlassen (zurueck zu Stufe 0). */
    void goBack();

    int  itemCount() const;
    juce::String itemName (int index) const;
    /** Waehlt Eintrag `index` der Gruppe; false bei "NO FREE SLOT". */
    bool chooseItem (int index, juce::String& chosenId);

    /** Freie Karten gesamt; 0 -> "NO FREE SLOT" wird gezeichnet. */
    void setFreeSlots (int slots);
    int  freeSlots() const noexcept;
    bool showsNoFreeSlot() const noexcept;

    std::function<void (const juce::String& poolId)> onPartChosen;
    std::function<void()> onCloseRequested;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    std::vector<AddPartGroup> groups_;
    int currentGroup = -1;
    int freeSlotCount = 0;
    // Buttons werden bei jedem Gruppenwechsel neu gebaut; juce::TextButton ist
    // weder kopier- noch verschiebbar, daher als Besitzerzeiger im vector.
    std::vector<std::unique_ptr<juce::TextButton>> groupButtons, itemButtons;
};

/** "+ ADD STYLE"-Overlay (Ticket B-104): Name, Basis-Style, Akzentfarbe. */
class AddStyleOverlay final : public juce::Component
{
public:
    AddStyleOverlay();
    ~AddStyleOverlay() override;

    void setBaseStyles (juce::StringArray baseStyleIds);
    void setName (const juce::String& name);
    juce::String name() const;
    void setBaseStyle (const juce::String& styleId);
    juce::String baseStyle() const;
    void setAccentHex (const juce::String& hex);
    juce::String accentHex() const;

    /** true, wenn Name nicht leer ist und ein Basis-Style gewaehlt wurde. */
    bool canCreate() const;
    bool create (juce::String& idOut, juce::String& error) const;

    std::function<void (const juce::String& id)> onStyleCreated;
    std::function<void()> onCloseRequested;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::StringArray bases;
    juce::String styleName, baseId, accent = "#e0663a";
    juce::TextEditor nameEditor;
    juce::ComboBox   baseBox;
};

} // namespace bfka
