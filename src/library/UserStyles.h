// B-76: Verwaltung der nutzereigenen Styles eines Ordners.
#pragma once

#include <juce_core/juce_core.h>

#include <vector>

#include "core/StyleIO.h"

namespace bfka
{

/**
 * Verwaltet die nutzereigenen Styles eines Ordners (Ticket B-76).
 * Nur Message-Thread; `scan()` liest die Dateien.
 */
class UserStyles
{
public:
    static juce::File defaultDirectory();   ///< Dokumente/BassForge/KickArchitect/Styles

    explicit UserStyles (const juce::File& directory = {});

    /** Liest alle `.bfkastyle` im Ordner; liefert die Anzahl gueltiger Styles. */
    int scan();
    void refresh() { scan(); }

    const std::vector<UserStyle>& entries() const noexcept { return items; }

    /** Legt einen Style an bzw. ueberschreibt ihn (id) und schreibt die Datei. */
    bool addOrReplace (const UserStyle& style, juce::String& error);

    /** Loescht die Datei zu `id` (die Bestaetigung macht die UI). */
    bool remove (const juce::String& id, juce::String& error);

    const UserStyle* find (const juce::String& id) const noexcept;
    juce::File directory() const noexcept { return dir; }
    int size() const noexcept { return (int) items.size(); }

private:
    juce::File dir;
    std::vector<UserStyle> items;
};

} // namespace bfka
