// B-76: Eigene Styles als `.bfkastyle` (IO).
#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <optional>

namespace bfka
{

/** Ein nutzereigener Style (Ticket B-76), Dateiendung `.bfkastyle`. */
struct UserStyle
{
    juce::String id;          ///< stabile Kennung, z. B. "user-rawstyle"
    juce::String name;        ///< Anzeigename
    juce::String basedOn;     ///< Factory-Style, auf dem er basiert (optional)
    juce::String accentColor; ///< "#RRGGBB" (optional)
    bool  bothLibraries = false;
    bool  freeBuild     = false;
    juce::String notes;       ///< freier Text

    bool isValid() const noexcept { return id.isNotEmpty() && name.isNotEmpty(); }
};

/**
 * Lesen/Schreiben von `.bfkastyle` (Ticket B-76).
 *
 * Format: Wurzel `<BFKAStyle version="1" id name basedOn accentColor
 * bothLibraries freeBuild>` mit `<Notes>` als Kind (Text). Fehlende Attribute
 * bleiben leer bzw. false; `load` prueft nur Wurzel, `id` und `name`.
 *
 * Alle Methoden laufen auf dem Message-Thread; kein GUI, keine Engine.
 */
class StyleIO
{
public:
    static juce::String fileExtension();   ///< "bfkastyle"
    static juce::String fileWildcard();    ///< "*.bfkastyle"

    static bool save (const juce::File& file, const UserStyle& style, juce::String& error);
    static std::optional<UserStyle> load (const juce::File& file, juce::String& error);

    /** XML-Baum ohne Dateizugriff (Test/Export). */
    static juce::ValueTree toValueTree (const UserStyle& style);
    static std::optional<UserStyle> fromValueTree (const juce::ValueTree& tree, juce::String& error);
};

} // namespace bfka
