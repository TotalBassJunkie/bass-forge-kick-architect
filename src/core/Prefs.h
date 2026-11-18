// B-101 - Nutzer-Einstellungen als JSON (prefs.json).
#pragma once

#include <juce_core/juce_core.h>

namespace bfka
{

/**
 * Nutzer-Einstellungen als JSON (Ticket B-101). Acht Sektionen.
 *
 * Schluessel sind Punkt-Pfade, z. B. "audio.sampleRate" oder "ui.accent".
 * Die Ablage ist bewusst flach: der Punkt-Pfad ist der Property-Name im
 * JSON-Objekt, es gibt keine Verschachtelung. Damit bleiben alte Dateien
 * lesbar, wenn spaeter Zeilen hinzukommen.
 */
class Prefs
{
public:
    explicit Prefs (const juce::File& file = {});

    /** AppData/BassForge/KickArchitect/prefs.json */
    static juce::File defaultFile();

    /** Die acht Sektionsnamen in Anzeige-Reihenfolge. */
    static const juce::StringArray& sections();

    /** false = fehlt/kaputt (Defaults bleiben). */
    bool load();

    /** Legt Elternordner an. */
    bool save() const;

    juce::var  get (const juce::String& key, const juce::var& fallback = {}) const;
    void       set (const juce::String& key, const juce::var& value);
    bool       contains (const juce::String& key) const;
    int        size() const;

    /** Leert den Bestand; die acht Sektionen bleiben als Namen bestehen. */
    void resetToDefaults();

    juce::File file() const noexcept;

private:
    juce::File prefsFile;
    juce::DynamicObject::Ptr data;
};

} // namespace bfka
