// B-76: Verwaltung der nutzereigenen Styles eines Ordners.
#include "library/UserStyles.h"

#include <algorithm>
#include <utility>

namespace bfka
{

juce::File UserStyles::defaultDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BassForge")
               .getChildFile ("KickArchitect")
               .getChildFile ("Styles");
}

UserStyles::UserStyles (const juce::File& directory)
    : dir (directory == juce::File {} ? defaultDirectory() : directory)
{
}

int UserStyles::scan()
{
    items.clear();

    if (dir.createDirectory().failed())
        return 0;

    juce::Array<juce::File> found;
    dir.findChildFiles (found, juce::File::findFiles, false, StyleIO::fileWildcard());

    for (const auto& file : found)
    {
        juce::String error;
        auto style = StyleIO::load (file, error);

        // Ungueltige Dateien werden uebersprungen, nicht als Fehler gemeldet.
        if (style.has_value())
            items.push_back (std::move (*style));
    }

    std::stable_sort (items.begin(), items.end(),
                      [] (const UserStyle& a, const UserStyle& b)
                      {
                          const auto byName = a.name.compareIgnoreCase (b.name);

                          if (byName != 0)
                              return byName < 0;

                          return a.id.compareIgnoreCase (b.id) < 0;
                      });

    return (int) items.size();
}

bool UserStyles::addOrReplace (const UserStyle& style, juce::String& error)
{
    if (! style.isValid())
    {
        error = "Style braucht eine id und einen Namen.";
        return false;
    }

    const auto file = dir.getChildFile (style.id + "." + StyleIO::fileExtension());

    if (! StyleIO::save (file, style, error))
        return false;

    for (auto& item : items)
    {
        if (item.id == style.id)
        {
            item = style;
            return true;
        }
    }

    items.push_back (style);
    return true;
}

bool UserStyles::remove (const juce::String& id, juce::String& error)
{
    const auto it = std::find_if (items.begin(), items.end(),
                                  [&id] (const UserStyle& s) { return s.id == id; });

    if (it == items.end())
    {
        error = "Style nicht gefunden.";
        return false;
    }

    const auto file = dir.getChildFile (id + "." + StyleIO::fileExtension());

    if (file.existsAsFile() && ! file.deleteFile())
    {
        error = "Datei konnte nicht geloescht werden: " + file.getFullPathName();
        return false;
    }

    items.erase (it);
    return true;
}

const UserStyle* UserStyles::find (const juce::String& id) const noexcept
{
    for (const auto& item : items)
    {
        if (item.id == id)
            return &item;
    }

    return nullptr;
}

} // namespace bfka
