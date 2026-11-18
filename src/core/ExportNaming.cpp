// B-112: Dateinamen fuer Exporte (Bereinigung, Basisname, Kollisionsschutz).
#include "core/ExportNaming.h"

namespace bfka
{

namespace
{
    /** Laenge, auf die ein Dateiname hoechstens gekappt wird. */
    constexpr int kMaxNameLength = 80;

    /** Obergrenze der Kollisionsnummern ("_2" ... "_999"). */
    constexpr int kMaxCollisionIndex = 999;

    /** Zeichen, die Windows in Dateinamen nicht zulaesst. */
    const juce::String kForbiddenChars { "<>:\"/\\|?*" };
}

juce::String ExportNaming::sanitise (const juce::String& raw)
{
    juce::String text = raw;

    // Verbotene Zeichen und Steuerzeichen einzeln durch Unterstrich ersetzen.
    for (int i = 0; i < text.length(); ++i)
    {
        const juce::juce_wchar c = text[i];
        const bool isForbidden = c < 32 || kForbiddenChars.containsChar (c);

        if (isForbidden)
            text = text.replaceSection (i, 1, "_");
    }

    // Fuehrende und abschliessende Leerzeichen sowie Punkte entfernen.
    text = text.trimCharactersAtStart (" .");
    text = text.trimCharactersAtEnd (" .");

    if (text.length() > kMaxNameLength)
    {
        text = text.substring (0, kMaxNameLength);

        // Nach dem Kappen kann wieder ein Trennzeichen am Rand stehen.
        text = text.trimCharactersAtStart (" .");
        text = text.trimCharactersAtEnd (" .");
    }

    return text.isEmpty() ? juce::String ("export") : text;
}

juce::String ExportNaming::defaultBaseName (const juce::String& presetName, const juce::String& suffix)
{
    juce::String name = presetName.trim();

    if (name.isEmpty())
        name = "BassForge";

    const juce::String cleanSuffix = suffix.trim();

    return sanitise (cleanSuffix.isEmpty() ? name : name + "_" + cleanSuffix);
}

juce::File ExportNaming::uniqueFile (const juce::File& directory,
                                     const juce::String& baseName,
                                     const juce::String& extension)
{
    directory.createDirectory();

    const juce::String base = sanitise (baseName);

    juce::String ext = extension.trim();

    if (ext.startsWithChar ('.'))
        ext = ext.substring (1);

    const auto makeFile = [&directory, &base, &ext] (int index)
    {
        const juce::String stem = index <= 1 ? base : base + "_" + juce::String (index);
        const juce::String name = ext.isEmpty() ? stem : stem + "." + ext;

        return directory.getChildFile (name);
    };

    for (int index = 1; index <= kMaxCollisionIndex; ++index)
    {
        const auto candidate = makeFile (index);

        if (! candidate.exists())
            return candidate;
    }

    // Alles belegt: der letzte Name wird trotzdem geliefert.
    return makeFile (kMaxCollisionIndex);
}

} // namespace bfka
