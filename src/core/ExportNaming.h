// B-112: Dateinamen fuer Exporte (Bereinigung, Basisname, Kollisionsschutz).
#pragma once

#include <juce_core/juce_core.h>

namespace bfka
{

/** Dateinamen fuer Exporte (Ticket B-112). */
class ExportNaming
{
public:
    /** Entfernt Zeichen, die Windows nicht erlaubt, und kappt auf 80 Zeichen. */
    static juce::String sanitise (const juce::String& raw);

    /** Basisname aus Preset-Name + Suffix, z. B. "Rumble 150" + "kick" -> "Rumble 150_kick". */
    static juce::String defaultBaseName (const juce::String& presetName, const juce::String& suffix);

    /** Freier Dateiname im Ordner; vorhandene Namen bekommen "_2", "_3", ... */
    static juce::File uniqueFile (const juce::File& directory,
                                  const juce::String& baseName,
                                  const juce::String& extension);
};

} // namespace bfka
