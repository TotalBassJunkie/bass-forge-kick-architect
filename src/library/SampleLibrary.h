#pragma once

#include <juce_core/juce_core.h>

#include <cstddef>
#include <functional>
#include <shared_mutex>
#include <vector>

#include "core/KickState.h"

/**
 * Sample-Library-Index (Ticket B-20/B-21/B-22).

 * Der Index liest die installierte Content-Library nur und legt sie als flache
 * Liste ab: Pfad, Pool-Ordner, Rolle, Tempo und Grundton aus dem Dateinamen sowie
 * Laenge/Kanaele/Abtastrate aus dem WAV-Header. Audiodaten werden nicht dekodiert.
 *
 * Regeln und Fallstricke: docs/specs/library-and-analysis.md.
 */
namespace bfka
{

/** Ein Eintrag des Sample-Index. */
struct SampleInfo
{
    juce::File   file;                ///< absoluter Verweis auf die Datei
    juce::String relPath;             ///< Pfad relativ zum Content-Root, Trenner '/'
    juce::String fileName;            ///< reiner Dateiname mit Endung
    juce::String poolFolder;          ///< Ordner relativ zum Root, Trenner '/' (Pool-Kennung)
    Role         role = Role::none;   ///< Rolle des Pools
    double       bpm = 150.0;         ///< Tempo aus dem Dateinamen, Fallback 150
    int          keyNote = -1;        ///< Grundton 0..11 (C=0 ... B=11), -1 = unbekannt
    double       lengthSeconds = 0.0; ///< Laenge in Sekunden, 0 = Header nicht lesbar
    int          numChannels = 0;     ///< Kanaele, 0 = Header nicht lesbar
    double       sampleRate = 0.0;    ///< Abtastrate in Hz, 0 = Header nicht lesbar
};

/** Ergebnis eines Library-Scans. */
struct LibraryScanResult
{
    std::vector<SampleInfo> samples;  ///< alle gefundenen WAV-Dateien
    juce::StringArray       warnings; ///< Klartextmeldungen (je Kategorie hoechstens 50)
};

/** Ein Eintrag der Ordner-Tabelle: Praefix im Content-Root -> Rolle. */
struct PoolDefinition
{
    const char* folderPrefix; ///< Ordner relativ zum Root, Trenner '/'
    Role        role;         ///< Rolle der Dateien in diesem Ordner
    bool        melodic;      ///< Pool traegt den Grundton im Dateinamen
};

/**
 * Index der Sample-Library.

 * Der Index ist nach loadFromRoot() unveraenderlich. Alle Zugriffe laufen ueber ein
 * std::shared_mutex, damit ein Neuaufbau im Hintergrund (B-27) nicht mit Lesern kollidiert.
 */
class SampleLibrary
{
public:
    /** Fallback-Tempo, wenn der Dateiname kein Tempo kodiert (Handbuch: 150). */
    static constexpr double kDefaultBpm = 150.0;

    /** Plausibles Tempo-Fenster. Zahlen ausserhalb sind Indexnummern, kein Tempo. */
    static constexpr int kMinPlausibleBpm = 60;
    static constexpr int kMaxPlausibleBpm = 300;

    /** MIDI-Note der Note C im Index; die Anzeige-Oktave liefert withOctave(). */
    static constexpr int kKeyNoteC = 0;

    SampleLibrary() = default;

    // --- Aufbau ------------------------------------------------------------
    /**
     * Scannt root rekursiv nach *.wav (Endung case-insensitiv) und ersetzt den Index.

     * Schlaegt der Scan fehl oder wird er abgebrochen, bleibt der bisherige Index
     * unveraendert (der Aufbau laeuft transaktional).

     * @param root         Content-Root der Library
     * @param result       gefuellte Liste plus Warnungen
     * @param shouldAbort  optional; true bricht ab
     * @returns true, wenn der Scan vollstaendig durchlief (Warnungen sind erlaubt)
     */
    bool loadFromRoot (const juce::File& root,
                       LibraryScanResult& result,
                       const std::function<bool()>& shouldAbort = {});

    /** Wirft den Index weg. */
    void clear();

    // --- Zugriff -----------------------------------------------------------
    /** Der vollstaendige Index. Der Verweis gilt bis zum naechsten erfolgreichen Scan. */
    const std::vector<SampleInfo>& samples() const;

    /** Anzahl indexierter Dateien. */
    std::size_t size() const;

    /** Alle Eintraege, deren poolFolder mit folderPrefix beginnt (case-insensitiv). */
    std::vector<const SampleInfo*> byPool (const juce::String& folderPrefix) const;

    /** Alle Eintraege einer Rolle, in Index-Reihenfolge. */
    std::vector<const SampleInfo*> byRole (Role role) const;

    /** Content-Root des letzten erfolgreichen Scans (leer, wenn noch keiner lief). */
    juce::File contentRoot() const;

    // --- Dateinamen-Regeln -------------------------------------------------
    /** Tempo aus dem Dateinamen; Fallback kDefaultBpm. */
    static double parseBpmFromFileName (const juce::String& fileNameOrPath);

    /** Grundton aus dem Dateinamen als MIDI-Note 0..11 (C=0), -1 wenn unbekannt. */
    static int parseKeyFromFileName (const juce::String& fileNameOrPath);

    /** MIDI-Note fuer keyNote mit Oktav-Offset (KEY TRACK OCTAVE). -1 bleibt -1. */
    static int withOctave (int keyNote, int octave) noexcept;

    // --- Ordner-Regeln -----------------------------------------------------
    /** Rolle eines Pool-Ordners (laengster passender Praefix), sonst Role::none. */
    static Role roleForFolder (const juce::String& folderRelPath);

    /** Tabelleneintrag zu einem Pool-Ordner, nullptr wenn unbekannt. */
    static const PoolDefinition* definitionForFolder (const juce::String& folderRelPath);

    /** Die Ordner-Tabelle (Quelle: genres.json, Content-Revision R33). */
    static const std::vector<PoolDefinition>& poolDefinitions();

private:
    mutable std::shared_mutex mutex;
    std::vector<SampleInfo>   index;
    juce::File                scannedRoot;
};

/**
 * Erster existierender Content-Root-Kandidat:

 *   1. %PUBLIC%\Documents\BassForge\KickArchitect
 *   2. %PUBLIC%\Documents\Teknovault\The Teknovault Kick Builder  (nur lesen)
 *   3. %USERPROFILE%\Documents\BassForge\KickArchitect

 * Existiert keiner, kommt Kandidat 1 zurueck (der Pfad, den der Nutzer anlegen soll).
 */
juce::File defaultContentRoot();

/** Die Kandidatenliste, aus der defaultContentRoot() waehlt (Reihenfolge = Prioritaet). */
std::vector<juce::File> contentRootCandidates();

} // namespace bfka
