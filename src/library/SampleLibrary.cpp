#include "library/SampleLibrary.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <memory>
#include <mutex>

#include "core/Log.h"

namespace bfka
{

namespace
{
    /** Obergrenze fuer Warnungen je Kategorie: bei kaputten Libraries soll die Liste
        benutzbar bleiben. */
    constexpr int kMaxWarnings = 50;

    /** Vereinheitlicht Pfadtrenner auf '/' und entfernt Leerraum/abschliessende Trenner. */
    juce::String normaliseSlashes (const juce::String& path)
    {
        auto s = path.trim();

        // Bewusst zeichenweise: unabhaengig von der String-API-Variante und
        // ohne Annahme ueber die Zeichenbreite.
        for (int i = 0; i < s.length(); ++i)
            if (s[i] == '\\')
                s = s.replaceSection (i, 1, "/");

        while (s.length() > 1 && s.endsWithChar ('/'))
            s = s.dropLastCharacters (1);

        return s;
    }

    /**
     * Entfernt Ordneranteil und Dateiendung.

     * Die Endung faellt nur weg, wenn der Teil nach dem letzten Punkt keine Ziffer
     * enthaelt - sonst wuerde aus "Punch 150.bpm" die Zahl 150 verschwinden.
     */
    juce::String stripDirectoryAndExtension (const juce::String& nameOrPath)
    {
        auto s = normaliseSlashes (nameOrPath);
        s = s.fromLastOccurrenceOf ("/", false, false);

        const auto dot = s.lastIndexOfChar ('.');

        if (dot > 0 && dot < s.length() - 1)
        {
            const auto tail = s.substring (dot + 1);

            bool tailHasDigit = false;
            for (int i = 0; i < tail.length(); ++i)
            {
                if (juce::CharacterFunctions::isDigit (tail[i]))
                {
                    tailHasDigit = true;
                    break;
                }
            }

            if (! tailHasDigit)
                s = s.substring (0, dot);
        }

        return s;
    }

    /** Alle zusammenhaengenden Zahlengruppen im Text, in Lese-Reihenfolge. */
    std::vector<int> collectNumbers (const juce::String& text)
    {
        std::vector<int> numbers;

        long long current = 0;
        bool inNumber = false;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];

            if (juce::CharacterFunctions::isDigit (c))
            {
                if (current <= 1000000) // Ueberlauf uninteressant, Wert bleibt unplausibel
                    current = current * 10 + (long long) (c - '0');

                inNumber = true;
            }
            else if (inNumber)
            {
                numbers.push_back ((int) current);
                current = 0;
                inNumber = false;
            }
        }

        if (inNumber)
            numbers.push_back ((int) current);

        return numbers;
    }

    /** Schneidet Satzzeichen von einem Token ab, damit "(A)" wie "A" wirkt. */
    juce::String trimTokenPunctuation (const juce::String& token)
    {
        static const juce::String punctuation (",.;:!?()[]{}<>\"'-");

        auto s = token;

        while (s.isNotEmpty() && punctuation.containsChar (s[0]))
            s = s.substring (1);

        while (s.isNotEmpty() && punctuation.containsChar (s[s.length() - 1]))
            s = s.dropLastCharacters (1);

        return s;
    }

    /** Grundton eines Tokens als MIDI-Note 0..11 (C=0 ... B=11), sonst -1. */
    int noteFromToken (const juce::String& rawToken)
    {
        const auto token = trimTokenPunctuation (rawToken);

        if (token.length() != 1 && token.length() != 2)
            return -1;

        int pitchClass = -1;

        switch (juce::CharacterFunctions::toUpperCase (token[0]))
        {
            case 'C': pitchClass = 0;  break;
            case 'D': pitchClass = 2;  break;
            case 'E': pitchClass = 4;  break;
            case 'F': pitchClass = 5;  break;
            case 'G': pitchClass = 7;  break;
            case 'A': pitchClass = 9;  break;
            case 'B': pitchClass = 11; break;
            default:  return -1;
        }

        if (token.length() == 1)
            return pitchClass;

        // Vorzeichen: '#' (auch U+266F) erhoeht, 'b' (auch U+266D) senkt.
        const auto accidental = token[1];

        if (accidental == '#' || accidental == (juce::juce_wchar) 0x266F)
            return (pitchClass + 1) % 12;

        if (accidental == 'b' || accidental == (juce::juce_wchar) 0x266D)
            return (pitchClass + 11) % 12;

        return -1;
    }

    /**
     * Praefix-Vergleich auf Segmentgrenze.

     * "Main - Punches" passt auf "Main - Punches/Zaag", aber
     * "Main - Crunches/Industrial" nicht auf "Main - Crunches/Industrial Heavy".
     */
    bool folderMatchesPrefix (const juce::String& folder, const juce::String& prefix)
    {
        if (prefix.isEmpty())
            return true;

        if (! folder.startsWithIgnoreCase (prefix))
            return false;

        return folder.length() == prefix.length()
               || folder[prefix.length()] == '/';
    }

    /** Ordneranteil eines relativen Pfades ("" wenn die Datei im Root liegt). */
    juce::String folderOfRelativePath (const juce::String& relPath)
    {
        const auto slash = relPath.lastIndexOfChar ('/');
        return slash > 0 ? relPath.substring (0, slash) : juce::String();
    }

    juce::String makeRelativePath (const juce::File& root, const juce::File& file)
    {
        return normaliseSlashes (file.getRelativePathFrom (root));
    }
}

// ---------------------------------------------------------------------------
// Ordner-Tabelle
// ---------------------------------------------------------------------------

const std::vector<PoolDefinition>& SampleLibrary::poolDefinitions()
{
    // Quelle: genres.json des Referenzprodukts (schemaVersion 3, Content-Revision R33),
    // verifiziert gegen die Ordnernamen auf der Platte. Reihenfolge ist unwichtig -
    // ausgewertet wird immer der laengste passende Praefix.
    static const std::vector<PoolDefinition> table
    {
        // --- Haupt-Pools: Bodies -------------------------------------------
        { "Main - Punches/Clean EDM",                           Role::punch,           false },
        { "Main - Punches/Industrial",                          Role::punch,           false },
        { "Main - Punches/Industrial (Extended)",               Role::punch,           false },
        { "Main - Punches/Newstyle",                            Role::punch,           false },
        { "Main - Punches/Rawstyle",                            Role::punch,           false },
        { "Main - Punches/Techno",                              Role::punch,           false },
        { "Main - Punches/Techno (Heavy)",                      Role::punch,           false },
        { "Main - Punches/Zaag",                                Role::punch,           false },

        // "Industrial Heavy" liegt unter Crunches, ist laut genres.json aber RUMBLE.
        { "Main - Crunches/Industrial Heavy",                   Role::rumble,          false },
        { "Main - Crunches/Newstyle & Midintro (Melodic)",      Role::crunch,          true  },
        { "Main - Crunches/Psy Style (Melodic)",                Role::crunch,          true  },
        { "Main - Crunches/Rawstyle (Melodic)",                 Role::crunch,          true  },
        { "Main - Crunches/Reverse Bass (Melodic)",             Role::crunch,          true  },
        { "Main - Crunches/Zaag (Melodic)",                     Role::crunch,          true  },

        // --- Haupt-Pools: Tails --------------------------------------------
        { "Main - Rumbles/Rumble (Rawstyle)",                   Role::rumble,          false },
        { "Main - Rumbles/Rumbles (Bigroom)",                   Role::rumble,          false },
        { "Main - Rumbles/Rumbles (Techno)",                    Role::rumble,          false },
        { "Main - Rumbles/Schranz",                             Role::rumble,          false },

        // --- Transienten ---------------------------------------------------
        { "Transients (Clicks)",                                Role::click,           false },
        { "Transients (Clicks) [Zaag]",                         Role::click,           false },
        { "Transients (Percs)",                                 Role::punchEnhancer,   false },
        { "Transients (Punch Top)",                             Role::punchEnhancer,   false },
        { "Transients (Newstyle Tops)",                         Role::top,             false },

        // --- Enhancer ------------------------------------------------------
        { "Enhancers (Rumbles)",                                Role::rumbleEnhancer,  false },
        { "Enhancers (Schranz)",                                Role::schranzEnhancer, false },
        { "Enhancers (Tops)",                                   Role::top,             false },
        { "Enhancers (Punch Reverbs)",                          Role::punchEnhancer,   false },

        // --- Impulsantworten: indexiert, aber kein Kick-Pool ----------------
        { "Impulses",                                           Role::none,            false },

        // --- Auffangzeilen fuer neue/expandierte Unterordner ----------------
        { "Main - Punches",                                     Role::punch,           false },
        { "Main - Crunches",                                    Role::crunch,          false },
        { "Main - Rumbles",                                     Role::rumble,          false }
    };

    return table;
}

const PoolDefinition* SampleLibrary::definitionForFolder (const juce::String& folderRelPath)
{
    const auto folder = normaliseSlashes (folderRelPath);

    const PoolDefinition* best = nullptr;
    int bestLength = -1;

    for (const auto& def : poolDefinitions())
    {
        const juce::String prefix (def.folderPrefix);

        if (! folderMatchesPrefix (folder, prefix))
            continue;

        if (prefix.length() > bestLength)
        {
            bestLength = prefix.length();
            best = &def;
        }
    }

    return best;
}

Role SampleLibrary::roleForFolder (const juce::String& folderRelPath)
{
    const auto* def = definitionForFolder (folderRelPath);
    return def != nullptr ? def->role : Role::none;
}

// ---------------------------------------------------------------------------
// Dateinamen-Regeln
// ---------------------------------------------------------------------------

double SampleLibrary::parseBpmFromFileName (const juce::String& fileNameOrPath)
{
    const auto numbers = collectNumbers (stripDirectoryAndExtension (fileNameOrPath));

    // Namenskonvention der Library: "<Pool> <Index> <BPM>" bzw.
    // "<Pool> <Index> <Tonart> <BPM>". Reine Indexnamen ("Punch Clean 100",
    // "Enhancer Newstyle Top 10 E") kodieren kein Tempo - dort gilt der Fallback.
    if (numbers.size() < 2)
        return kDefaultBpm;

    const auto last = numbers.back();

    // Indexnummern wie "Punch Techno 160" liegen im gleichen Zahlenraum wie Tempi.
    // Nur ein plausibles Tempo zaehlt, alles andere ist eine Nummer.
    if (last < kMinPlausibleBpm || last > kMaxPlausibleBpm)
        return kDefaultBpm;

    return (double) last;
}

int SampleLibrary::parseKeyFromFileName (const juce::String& fileNameOrPath)
{
    const auto tokens = juce::StringArray::fromTokens (stripDirectoryAndExtension (fileNameOrPath),
                                                      " \t\r\n",
                                                      juce::String());

    // Von hinten suchen: die Tonart steht hinter dem Index und vor dem Tempo.
    for (int i = tokens.size() - 1; i >= 0; --i)
    {
        const auto note = noteFromToken (tokens[i]);

        if (note >= 0)
            return note;
    }

    return -1;
}

int SampleLibrary::withOctave (int keyNote, int octave) noexcept
{
    if (keyNote < 0)
        return -1;

    return juce::jlimit (0, 127, keyNote + 12 * octave);
}

// ---------------------------------------------------------------------------
// Aufbau des Index
// ---------------------------------------------------------------------------

bool SampleLibrary::loadFromRoot (const juce::File& root,
                                  LibraryScanResult& result,
                                  const std::function<bool()>& shouldAbort)
{
    result.samples.clear();
    result.warnings.clear();

    if (! root.isDirectory())
    {
        result.warnings.add ("Content-Root ist kein lesbarer Ordner: " + root.getFullPathName());
        log::w ("SampleLibrary: " + result.warnings[0]);
        return false;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::vector<SampleInfo> scanned;
    juce::StringArray unmappedFolders;
    juce::StringArray unreadableFiles;
    int suppressedWarnings = 0;

    scanned.reserve (4096);

    for (const auto& entry : juce::RangedDirectoryIterator (root, true, "*", juce::File::findFiles))
    {
        if (shouldAbort && shouldAbort())
        {
            result.warnings.add ("Scan abgebrochen; der vorhandene Index bleibt unveraendert.");
            log::i ("SampleLibrary: Scan abgebrochen.");
            return false;
        }

        const auto file = entry.getFile();

        // Endung case-insensitiv pruefen (.wav, .WAV, .Wav ...).
        if (file.getFileExtension().toLowerCase() != ".wav")
            continue;

        SampleInfo info;
        info.file       = file;
        info.relPath    = makeRelativePath (root, file);
        info.fileName   = file.getFileName();
        info.poolFolder = folderOfRelativePath (info.relPath);
        info.bpm        = parseBpmFromFileName (info.fileName);
        info.keyNote    = parseKeyFromFileName (info.fileName);

        if (const auto* def = definitionForFolder (info.poolFolder); def != nullptr)
        {
            info.role = def->role;
        }
        else
        {
            info.role = Role::none;

            if (info.poolFolder.isNotEmpty())
            {
                if (unmappedFolders.size() < kMaxWarnings)
                {
                    if (! unmappedFolders.contains (info.poolFolder, true))
                        unmappedFolders.add (info.poolFolder);
                }
                else
                {
                    ++suppressedWarnings;
                }
            }
        }

        // Metadaten aus dem Header: createReaderFor() liest nur den Kopf, nicht die Daten.
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader != nullptr)
        {
            info.numChannels = (int) reader->numChannels;
            info.sampleRate  = reader->sampleRate;

            if (reader->sampleRate > 0.0)
                info.lengthSeconds = (double) reader->lengthInSamples / reader->sampleRate;
        }
        else if (unreadableFiles.size() < kMaxWarnings)
        {
            unreadableFiles.add (info.relPath);
        }
        else
        {
            ++suppressedWarnings;
        }

        scanned.push_back (std::move (info));
    }

    // Die Reihenfolge des Dateisystems ist nicht reproduzierbar: nach relativem
    // Pfad sortieren, damit Index und Tests stabil bleiben.
    std::sort (scanned.begin(), scanned.end(),
               [] (const SampleInfo& a, const SampleInfo& b)
               {
                   return a.relPath.compareIgnoreCase (b.relPath) < 0;
               });

    for (const auto& folder : unmappedFolders)
        result.warnings.add ("Ordner ohne Pool-Zuordnung (Rolle none): " + folder);

    for (const auto& path : unreadableFiles)
        result.warnings.add ("WAV-Header nicht lesbar (Laenge/Rate unbekannt): " + path);

    if (suppressedWarnings > 0)
        result.warnings.add (juce::String (suppressedWarnings) + " weitere Warnungen unterdrueckt.");

    if (scanned.empty())
        result.warnings.add ("Keine WAV-Dateien unter " + root.getFullPathName() + " gefunden.");

    const auto numFiles = (int) scanned.size();

    {
        const std::unique_lock lock (mutex);
        index       = scanned;
        scannedRoot = root;
    }

    result.samples = std::move (scanned);

    log::i ("SampleLibrary: " + juce::String (numFiles) + " WAV-Dateien indexiert aus "
            + root.getFullPathName());

    return true;
}

void SampleLibrary::clear()
{
    const std::unique_lock lock (mutex);
    index.clear();
    scannedRoot = juce::File();
}

// ---------------------------------------------------------------------------
// Zugriff
// ---------------------------------------------------------------------------

const std::vector<SampleInfo>& SampleLibrary::samples() const
{
    // Der Verweis ueberlebt die Sperre bewusst: der Index wird nur unter
    // unique_lock ersetzt, und Ladende halten den alten Stand bis zum Neuaufbau.
    const std::shared_lock lock (mutex);
    return index;
}

std::size_t SampleLibrary::size() const
{
    const std::shared_lock lock (mutex);
    return index.size();
}

std::vector<const SampleInfo*> SampleLibrary::byPool (const juce::String& folderPrefix) const
{
    const auto prefix = normaliseSlashes (folderPrefix);

    std::vector<const SampleInfo*> result;
    const std::shared_lock lock (mutex);

    for (const auto& info : index)
        if (prefix.isEmpty() || info.poolFolder.startsWithIgnoreCase (prefix))
            result.push_back (&info);

    return result;
}

std::vector<const SampleInfo*> SampleLibrary::byRole (Role role) const
{
    std::vector<const SampleInfo*> result;
    const std::shared_lock lock (mutex);

    for (const auto& info : index)
        if (info.role == role)
            result.push_back (&info);

    return result;
}

juce::File SampleLibrary::contentRoot() const
{
    const std::shared_lock lock (mutex);
    return scannedRoot;
}

// ---------------------------------------------------------------------------
// Content-Root
// ---------------------------------------------------------------------------

std::vector<juce::File> contentRootCandidates()
{
    const auto commonDocs = juce::File::getSpecialLocation (juce::File::commonDocumentsDirectory);
    const auto userDocs   = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    std::vector<juce::File> candidates;
    candidates.reserve (3);

    // 1. eigener Content-Root (Schreibziel fuer eigene Libraries/IRs)
    candidates.push_back (commonDocs.getChildFile ("BassForge").getChildFile ("KickArchitect"));

    // 2. installierte Referenz-Library (nur lesen!)
    candidates.push_back (commonDocs.getChildFile ("Teknovault")
                                    .getChildFile ("The Teknovault Kick Builder"));

    // 3. Nutzer-Dokumente als Rueckfallebene
    candidates.push_back (userDocs.getChildFile ("BassForge").getChildFile ("KickArchitect"));

    return candidates;
}

juce::File defaultContentRoot()
{
    const auto candidates = contentRootCandidates();

    for (const auto& candidate : candidates)
        if (candidate.isDirectory())
            return candidate;

    // Keiner vorhanden: der Pfad, den der Nutzer anlegen soll.
    return candidates.front();
}

} // namespace bfka
