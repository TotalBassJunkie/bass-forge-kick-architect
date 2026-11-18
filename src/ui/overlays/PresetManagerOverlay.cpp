/*
 * Datei: src/ui/overlays/PresetManagerOverlay.cpp
 * Ticket: B-102 -- Preset-Manager (browsen, speichern, umbenennen,
 *         duplizieren, Papierkorb) samt Overlay-Fenster.
 *
 * Grundsaetze:
 *  - "Loeschen" gibt es nicht: moveToTrash() verschiebt die Datei in den
 *    Unterordner `_Papierkorb`. Nur der Mensch raeumt dort auf.
 *  - Der Manager liest nur Dateinamen, keine Preset-Inhalte; dadurch bleibt
 *    er unabhaengig von Format und Engine.
 *  - Alle Methoden laufen auf dem Message-Thread.
 */
#include "ui/overlays/PresetManagerOverlay.h"

#include "core/PresetIO.h"
#include "ui/UiTheme.h"

#include <algorithm>

namespace bfka
{

namespace
{
    /** Zeichen, die in Dateinamen nichts zu suchen haben (wie ExportNaming). */
    bool isForbiddenNameChar (juce::juce_wchar c) noexcept
    {
        if (c < 32)
            return true;

        switch (c)
        {
            case '<': case '>': case ':': case '"':
            case '/': case '\\': case '|': case '?':
            case '*':
                return true;
            default:
                break;
        }

        return false;
    }
} // namespace

// =========================================================================
// PresetManager
// =========================================================================
PresetManager::PresetManager (const juce::File& directory)
    : dir (directory)
{
}

int PresetManager::scan()
{
    items.clear();

    // Ordner darf fehlen (erster Start) -- dann eben leer.
    if (! dir.isDirectory())
        (void) dir.createDirectory();

    juce::Array<juce::File> found;
    dir.findChildFiles (found, juce::File::findFiles, false, "*.bfkapreset");

    for (const auto& file : found)
    {
        PresetEntry entry;
        entry.name    = file.getFileNameWithoutExtension();
        entry.file    = file;
        entry.isTrash = false;
        items.push_back (entry);
    }

    std::stable_sort (items.begin(), items.end(),
                      [] (const PresetEntry& a, const PresetEntry& b)
                      {
                          return a.name.compareIgnoreCase (b.name) < 0;
                      });

    return (int) items.size();
}

juce::String PresetManager::sanitiseName (const juce::String& raw) const
{
    juce::String out;

    for (int i = 0; i < raw.length(); ++i)
    {
        const auto c = raw[i];

        out += (isForbiddenNameChar (c)
                    ? juce::String ("_")
                    : juce::String::charToString (c));
    }

    out = out.trim();

    // Punkt am Ende ist unter Windows unzulaessig (und verwirrt die Endung).
    while (out.endsWithChar ('.'))
        out = out.dropLastCharacters (1);

    out = out.trim();

    if (out.isEmpty() || out == "." || out == "..")
        return "preset";

    return out;
}

bool PresetManager::rename (int index, const juce::String& newName, juce::String& error)
{
    error.clear();

    if (index < 0 || index >= (int) items.size())
    {
        error = "Ungueltiger Preset-Index.";
        return false;
    }

    const auto source = items[(size_t) index].file;
    const auto clean  = sanitiseName (newName);

    if (! source.existsAsFile())
    {
        error = "Preset-Datei fehlt.";
        return false;
    }

    const auto target = dir.getChildFile (clean + PresetIO::fileExtension());

    // Derselbe Name: nichts zu tun, aber auch kein Fehler.
    if (target == source)
    {
        scan();
        return true;
    }

    if (target.existsAsFile())
    {
        error = "Name schon vergeben.";
        return false;
    }

    if (! source.moveFileTo (target))
    {
        error = "Umbenennen fehlgeschlagen.";
        return false;
    }

    scan();
    return true;
}

bool PresetManager::duplicate (int index, juce::String& error)
{
    error.clear();

    if (index < 0 || index >= (int) items.size())
    {
        error = "Ungueltiger Preset-Index.";
        return false;
    }

    const auto source = items[(size_t) index].file;

    if (! source.existsAsFile())
    {
        error = "Preset-Datei fehlt.";
        return false;
    }

    const auto base = items[(size_t) index].name;

    // `<name>_copy`, bei Kollision `_copy2`, `_copy3`, ...
    juce::File target = dir.getChildFile (base + "_copy" + PresetIO::fileExtension());

    int counter = 2;
    while (target.existsAsFile())
        target = dir.getChildFile (base + "_copy" + juce::String (counter++)
                                   + PresetIO::fileExtension());

    if (! source.copyFileTo (target))
    {
        error = "Duplizieren fehlgeschlagen.";
        return false;
    }

    scan();
    return true;
}

bool PresetManager::moveToTrash (int index, juce::String& error)
{
    error.clear();

    if (index < 0 || index >= (int) items.size())
    {
        error = "Ungueltiger Preset-Index.";
        return false;
    }

    const auto source = items[(size_t) index].file;

    if (! source.existsAsFile())
    {
        error = "Preset-Datei fehlt.";
        return false;
    }

    const auto trash = trashDirectory();
    (void) trash.createDirectory();

    // Zeitstempel haelt gleichnamige Presets auseinander.
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d%H%M%S");
    const auto base  = items[(size_t) index].name + "_" + stamp;

    juce::File target = trash.getChildFile (base + PresetIO::fileExtension());

    int counter = 2;
    while (target.existsAsFile())
        target = trash.getChildFile (base + "_" + juce::String (counter++)
                                     + PresetIO::fileExtension());

    if (! source.moveFileTo (target))
    {
        error = "In den Papierkorb verschieben fehlgeschlagen.";
        return false;
    }

    scan();
    return true;
}

juce::File PresetManager::trashDirectory() const
{
    return dir.getChildFile ("_Papierkorb");
}

int PresetManager::trashCount() const
{
    const auto trash = trashDirectory();

    if (! trash.isDirectory())
        return 0;

    juce::Array<juce::File> found;
    trash.findChildFiles (found, juce::File::findFiles, false, "*.bfkapreset");

    return (int) found.size();
}

// =========================================================================
// PresetManagerOverlay
// =========================================================================
PresetManagerOverlay::PresetManagerOverlay (PresetManager& managerToUse)
    : managerRef (managerToUse)
{
    setOpaque (false);
    setWantsKeyboardFocus (true);

    titleLabel.setText ("PRESETS", juce::dontSendNotification);
    titleLabel.setFont (ui::theme::titleFont());
    titleLabel.setColour (juce::Label::textColourId, ui::theme::text);
    addAndMakeVisible (titleLabel);

    nameEditor.setTextToShowWhenEmpty ("Name ...", ui::theme::textDim);
    nameEditor.setFont (ui::theme::bodyFont());
    nameEditor.setColour (juce::TextEditor::backgroundColourId, ui::theme::panelDeep);
    nameEditor.setColour (juce::TextEditor::outlineColourId, ui::theme::grid);
    nameEditor.setColour (juce::TextEditor::textColourId, ui::theme::text);
    nameEditor.onReturnKey = [this] { activateRename(); };
    addAndMakeVisible (nameEditor);

    for (auto* button : { &renameButton, &duplicateButton, &trashButton, &closeButton })
    {
        button->setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
        button->setColour (juce::TextButton::textColourOffId, ui::theme::text);
        addAndMakeVisible (button);
    }

    renameButton.onClick    = [this] { activateRename(); };
    duplicateButton.onClick = [this] { activateDuplicate(); };
    trashButton.onClick     = [this] { activateTrash(); };
    closeButton.onClick     = [this] { if (onCloseRequested) onCloseRequested(); };

    statusLabel.setFont (ui::theme::smallFont());
    statusLabel.setColour (juce::Label::textColourId, ui::theme::textDim);
    addAndMakeVisible (statusLabel);

    managerRef.refresh();
    syncFromManager();
}

PresetManagerOverlay::~PresetManagerOverlay() = default;

int PresetManagerOverlay::rowCount() const
{
    return managerRef.size();
}

juce::String PresetManagerOverlay::rowName (int row) const
{
    const auto& current = managerRef.entries();

    if (row < 0 || row >= (int) current.size())
        return {};

    return current[(size_t) row].name;
}

void PresetManagerOverlay::selectRow (int row)
{
    const int count = managerRef.size();

    if (count <= 0)
        selected = -1;
    else
        selected = juce::jlimit (0, count - 1, row);

    updateStatusText();
    repaint();
}

void PresetManagerOverlay::syncFromManager()
{
    const int count = managerRef.size();

    if (count <= 0)
        selected = -1;
    else if (selected < 0)
        selected = 0;
    else if (selected >= count)
        selected = count - 1;

    updateStatusText();
    repaint();
}

void PresetManagerOverlay::setStatus (const juce::String& text, bool isError)
{
    statusLabel.setText (text, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId,
                           isError ? ui::theme::error : ui::theme::textDim);
}

void PresetManagerOverlay::updateStatusText()
{
    const auto count = managerRef.size();
    const auto trash = managerRef.trashCount();

    juce::String text;
    text << count << (count == 1 ? " Preset" : " Presets");

    if (trash > 0)
        text << "  |  Papierkorb: " << trash;

    setStatus (text, false);
}

bool PresetManagerOverlay::renameSelected (const juce::String& newName, juce::String& error)
{
    error.clear();

    if (selected < 0 || selected >= managerRef.size())
    {
        error = "Kein Preset ausgewaehlt.";
        return false;
    }

    juce::String localError;

    if (! managerRef.rename (selected, newName, localError))
    {
        error = localError;
        setStatus (localError, true);
        return false;
    }

    syncFromManager();
    setStatus ("Umbenannt.", false);

    return true;
}

bool PresetManagerOverlay::duplicateSelected (juce::String& error)
{
    error.clear();

    const int row = selected;

    if (row < 0 || row >= managerRef.size())
    {
        error = "Kein Preset ausgewaehlt.";
        return false;
    }

    const auto sourceName = rowName (row);
    juce::String localError;

    if (! managerRef.duplicate (row, localError))
    {
        error = localError;
        setStatus (localError, true);
        return false;
    }

    syncFromManager();
    setStatus ("Dupliziert: " + sourceName, false);

    return true;
}

bool PresetManagerOverlay::trashSelected (juce::String& error)
{
    error.clear();

    const int row = selected;

    if (row < 0 || row >= managerRef.size())
    {
        error = "Kein Preset ausgewaehlt.";
        return false;
    }

    const auto sourceName = rowName (row);
    juce::String localError;

    if (! managerRef.moveToTrash (row, localError))
    {
        error = localError;
        setStatus (localError, true);
        return false;
    }

    // Auswahl rueckt auf den naechsten Eintrag (nicht auf -1), damit
    // mehrere Presets hintereinander weggeraeumt werden koennen.
    syncFromManager();
    setStatus ("In den Papierkorb: " + sourceName, false);

    return true;
}

void PresetManagerOverlay::activateRename()
{
    juce::String error;

    if (renameSelected (nameEditor.getText(), error))
        nameEditor.clear();
}

void PresetManagerOverlay::activateDuplicate()
{
    juce::String error;
    duplicateSelected (error);
}

void PresetManagerOverlay::activateTrash()
{
    juce::String error;
    trashSelected (error);
}

juce::Rectangle<int> PresetManagerOverlay::listArea() const
{
    auto area = getLocalBounds().reduced (kPad);

    // Kopfzeile oben wegnehmen, Bedienspalte rechts freihalten.
    area.removeFromTop (28);
    area.removeFromRight (kSideWidth);

    return area.withTrimmedRight (kPad / 2);
}

juce::Rectangle<int> PresetManagerOverlay::rowArea (int row) const
{
    const auto list = listArea();

    return { list.getX(), list.getY() + row * kRowHeight,
             list.getWidth(), kRowHeight };
}

int PresetManagerOverlay::rowAt (juce::Point<int> position) const
{
    const auto list = listArea();

    if (! list.contains (position))
        return -1;

    const int row = (position.y - list.getY()) / kRowHeight;

    if (row < 0 || row >= managerRef.size())
        return -1;

    return row;
}

void PresetManagerOverlay::paint (juce::Graphics& g)
{
    // Der OverlayHost dunkelt den Hintergrund bereits ab; hier nur die Flaeche.
    auto area = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (ui::theme::panel);
    g.fillRoundedRectangle (area, 10.0f);

    g.setColour (ui::theme::grid);
    g.drawRoundedRectangle (area, 10.0f, 1.0f);

    const auto list = listArea();

    // Listenrahmen.
    g.setColour (ui::theme::panelDeep);
    g.fillRoundedRectangle (list.toFloat(), 6.0f);
    g.setColour (ui::theme::grid);
    g.drawRoundedRectangle (list.toFloat(), 6.0f, 1.0f);

    const auto& entries = managerRef.entries();

    g.setFont (ui::theme::bodyFont());

    for (int row = 0; row < (int) entries.size(); ++row)
    {
        auto line = rowArea (row);

        if (line.getBottom() > list.getBottom())
            break;

        const bool isSelected = (row == selected);

        if (isSelected)
        {
            g.setColour (ui::theme::accent.withAlpha (0.18f));
            g.fillRect (line.reduced (2, 1));
            g.setColour (ui::theme::accent);
            g.fillRect (line.getX() + 2, line.getY() + 3, 2, line.getHeight() - 6);
        }

        g.setColour (isSelected ? ui::theme::text : ui::theme::textDim);
        g.drawText (entries[(size_t) row].name,
                    line.reduced (10, 0),
                    juce::Justification::centredLeft, true);
    }

    if (entries.empty())
    {
        g.setColour (ui::theme::textDim);
        g.setFont (ui::theme::smallFont());
        g.drawText ("Keine Presets in diesem Ordner.",
                    list.reduced (10, 0),
                    juce::Justification::centredLeft, true);
    }

    // Trennlinie zur Bedienspalte.
    g.setColour (ui::theme::grid);
    g.drawVerticalLine (list.getRight() + kPad / 2,
                        (float) list.getY(), (float) list.getBottom());
}

void PresetManagerOverlay::resized()
{
    auto area = getLocalBounds().reduced (kPad);

    titleLabel.setBounds (area.removeFromTop (28));

    // Bedienspalte rechts: exakt kSideWidth breit.
    auto side = area.removeFromRight (kSideWidth);

    nameEditor.setBounds (side.removeFromTop (26));
    side.removeFromTop (kPad);

    renameButton.setBounds    (side.removeFromTop (30));
    side.removeFromTop (6);
    duplicateButton.setBounds (side.removeFromTop (30));
    side.removeFromTop (6);
    trashButton.setBounds     (side.removeFromTop (30));

    closeButton.setBounds (side.removeFromBottom (30));
    statusLabel.setBounds (side.removeFromBottom (juce::jmin (28, side.getHeight())));
}

bool PresetManagerOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (onCloseRequested)
            onCloseRequested();

        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        activateRename();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::upKey)
    {
        selectRow (selected - 1);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::downKey)
    {
        selectRow (selected + 1);
        return true;
    }

    return false;
}

void PresetManagerOverlay::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    const int row = rowAt (event.getPosition());

    if (row >= 0)
        selectRow (row);
}

void PresetManagerOverlay::mouseDoubleClick (const juce::MouseEvent& event)
{
    const int row = rowAt (event.getPosition());

    if (row < 0)
        return;

    selectRow (row);

    const auto& entries = managerRef.entries();

    if ((size_t) row < entries.size() && onPresetActivated)
        onPresetActivated (entries[(size_t) row].file);
}

} // namespace bfka
