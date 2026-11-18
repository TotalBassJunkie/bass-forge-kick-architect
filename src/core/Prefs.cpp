// B-101 - Nutzer-Einstellungen als JSON (prefs.json).

#include "core/Prefs.h"

namespace bfka
{

Prefs::Prefs (const juce::File& file)
    : prefsFile (file.getFullPathName().isEmpty() ? defaultFile() : file),
      data (new juce::DynamicObject())
{
}

juce::File Prefs::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("BassForge")
               .getChildFile ("KickArchitect")
               .getChildFile ("prefs.json");
}

const juce::StringArray& Prefs::sections()
{
    static const juce::StringArray names { "Audio", "MIDI", "Library", "Engine",
                                           "Interface", "Presets", "Export", "Debug" };
    return names;
}

bool Prefs::load()
{
    if (! prefsFile.existsAsFile())
        return false;

    juce::var parsed;
    const auto result = juce::JSON::parse (prefsFile.loadFileAsString(), parsed);

    if (result.failed())
        return false;

    auto* object = parsed.getDynamicObject();

    // Kein Objekt (Array, Zahl, Text): die Datei ist nicht unsere.
    if (object == nullptr)
        return false;

    // Erst nach der Pruefung uebernehmen, damit Kaputtes die Defaults nicht
    // halb ueberschreibt.
    data = new juce::DynamicObject();

    for (const auto& property : object->getProperties())
        data->setProperty (property.name, property.value);

    return true;
}

bool Prefs::save() const
{
    if (prefsFile.getFullPathName().isEmpty())
        return false;

    const auto parent = prefsFile.getParentDirectory();

    if (! parent.isDirectory() && ! parent.createDirectory().wasOk())
        return false;

    return prefsFile.replaceWithText (juce::JSON::toString (juce::var (data.get()), false));
}

juce::var Prefs::get (const juce::String& key, const juce::var& fallback) const
{
    if (data == nullptr || ! data->hasProperty (key))
        return fallback;

    return data->getProperty (key);
}

void Prefs::set (const juce::String& key, const juce::var& value)
{
    if (data == nullptr)
        data = new juce::DynamicObject();

    data->setProperty (key, value);
}

bool Prefs::contains (const juce::String& key) const
{
    return data != nullptr && data->hasProperty (key);
}

int Prefs::size() const
{
    return data != nullptr ? data->getProperties().size() : 0;
}

void Prefs::resetToDefaults()
{
    data = new juce::DynamicObject();
}

juce::File Prefs::file() const noexcept
{
    return prefsFile;
}

} // namespace bfka
