/*
 * Datei: src/ui/overlays/PresetManagerOverlay.h
 * Ticket: B-102 -- Preset-Manager (browsen, speichern, umbenennen,
 *         duplizieren, Papierkorb) samt Overlay-Fenster.
 *
 * Der Manager arbeitet ausschliesslich auf Dateien in einem Ordner. Das
 * endgueltige Loeschen bleibt bewusst dem Menschen (Abschnitt 8 des Plans):
 * "Papierkorb" heisst hier nur "in den Unterordner _Papierkorb verschieben".
 */
#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace bfka
{

/** Ein Preset-Eintrag im Manager (Ticket B-102). */
struct PresetEntry
{
    juce::String name;      ///< Dateiname ohne Endung
    juce::File   file;
    bool         isTrash = false;
};

/**
 * Preset-Verwaltung ueber einen Ordner (Ticket B-102).
 * Umbenennen/Duplizieren/Papierkorb arbeiten nur auf Dateien; das
 * endgueltige Loeschen bleibt dem Menschen (Abschnitt 8 des Plans).
 */
class PresetManager
{
public:
    explicit PresetManager (const juce::File& directory);

    /** Liest alle `.bfkapreset` im Ordner; liefert die Anzahl. */
    int scan();
    void refresh() { scan(); }

    const std::vector<PresetEntry>& entries() const noexcept { return items; }
    int size() const noexcept { return (int) items.size(); }

    bool rename (int index, const juce::String& newName, juce::String& error);
    bool duplicate (int index, juce::String& error);
    /** Verschiebt in den Unterordner "_Papierkorb" (nie loeschen). */
    bool moveToTrash (int index, juce::String& error);

    /** Papierkorb-Ordner des Managers. */
    juce::File trashDirectory() const;
    /** Leert den Papierkorb NICHT; nur Pfad + Anzahl. */
    int trashCount() const;

    juce::File directory() const noexcept { return dir; }

private:
    juce::File dir;
    std::vector<PresetEntry> items;
    juce::String sanitiseName (const juce::String& raw) const;
};

/** Overlay-Fenster um den Manager (Ticket B-102). */
class PresetManagerOverlay final : public juce::Component
{
public:
    explicit PresetManagerOverlay (PresetManager& managerToUse);
    ~PresetManagerOverlay() override;

    int  rowCount() const;
    juce::String rowName (int row) const;
    void selectRow (int row);
    int  selectedRow() const noexcept { return selected; }
    /** Fuehrt eine Aktion auf der Auswahl aus und gibt das Ergebnis zurueck. */
    bool renameSelected (const juce::String& newName, juce::String& error);
    bool duplicateSelected (juce::String& error);
    bool trashSelected (juce::String& error);

    std::function<void (const juce::File& file)> onPresetActivated;   // Doppelklick
    std::function<void()> onCloseRequested;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    /** Uebernimmt den Manager-Stand in die Liste und klemmt die Auswahl. */
    void syncFromManager();
    void setStatus (const juce::String& text, bool isError);
    void updateStatusText();

    /** Rechteck der Liste (links) bzw. Zeile darin. */
    juce::Rectangle<int> listArea() const;
    juce::Rectangle<int> rowArea (int row) const;
    int rowAt (juce::Point<int> position) const;

    void activateRename();
    void activateDuplicate();
    void activateTrash();

    PresetManager& managerRef;

    juce::Label      titleLabel;
    juce::TextEditor nameEditor;
    juce::TextButton renameButton    { "RENAME" };
    juce::TextButton duplicateButton { "DUPLICATE" };
    juce::TextButton trashButton     { "TRASH" };
    juce::TextButton closeButton     { "SCHLIESSEN" };
    juce::Label      statusLabel;

    int selected = -1;

    static constexpr int kRowHeight   = 24;
    static constexpr int kSideWidth   = 200;
    static constexpr int kPad         = 12;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManagerOverlay)
};

} // namespace bfka
