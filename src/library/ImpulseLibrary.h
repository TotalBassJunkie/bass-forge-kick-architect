#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace bfka
{

/** Ein Impulse-Response-Eintrag des IR-Browsers (Ticket B-152). */
struct ImpulseInfo
{
    juce::String name;              ///< Dateiname ohne Endung
    juce::File   file;              ///< vollstaendiger Pfad
    double       seconds = 0.0;     ///< Laenge aus dem Reader
    bool         factory = false;   ///< true = aus dem Factory-Ordner
};

/**
 * Verzeichnis der Impulse-Responses fuer den Convolver (Ticket B-152).
 * Scannt den User-Ordner und optional einen Factory-Ordner nach
 * wav/aif/aiff/flac. Rein lesend, laeuft auf dem Message-Thread.
 */
class ImpulseLibrary
{
public:
    /** Dokumente/BassForge/KickArchitect/IRs */
    static juce::File defaultDirectory();

    /** Optionaler Factory-Ordner (leer = keiner). */
    void setFactoryDirectory (const juce::File& directory);
    void setUserDirectory (const juce::File& directory);

    /** Scannt beide Ordner neu; liefert die Anzahl der Eintraege. */
    int scan();
    void refresh() { scan(); }

    /** Eintraege in Anzeige-Reihenfolge (User zuerst, dann Factory), alphabetisch. */
    const std::vector<ImpulseInfo>& entries() const noexcept { return items; }

    /** Laenge einer Audiodatei in Sekunden; false bei unlesbarer Datei. */
    static bool readLengthSeconds (const juce::File& file, double& secondsOut);

private:
    juce::File userDir;
    juce::File factoryDir;
    std::vector<ImpulseInfo> items;
};

} // namespace bfka
