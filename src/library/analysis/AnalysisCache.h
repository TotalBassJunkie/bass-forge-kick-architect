/*
 * B-26: Analyse-Cache (binaer, versioniert, inkrementell).
 *
 * Der Cache haelt Analyse-Ergebnisse je Sample-Pfad und verwirft einen Eintrag,
 * sobald Dateigroesse oder Aenderungszeit nicht mehr passen. Das Binaerformat
 * ist im Ticket B-26 festgelegt (Kopf "BFKAAC", Version, Eintraege).
 */
#pragma once

#include <juce_core/juce_core.h>

#include <optional>
#include <vector>

namespace bfka
{

/** Analyse-Ergebnis eines Samples (Ticket B-26, spaeter B-23/B-24). */
struct SampleAnalysis
{
    int    keyNote        = -1;     ///< 0..11 (C=0), -1 = unbekannt
    double bpm            = 0.0;
    double onsetSeconds   = 0.0;
    double lengthSeconds  = 0.0;
    double confidence     = 0.0;    ///< 0..1
};

/**
 * Binaerer, versionierter Analyse-Cache (Ticket B-26).
 *
 * Der Schluessel ist der Sample-Pfad; zusaetzlich werden Dateigroesse und
 * Aenderungszeit gespeichert. Aendert sich eine der beiden, gilt der
 * Eintrag als veraltet (inkrementeller Neuaufbau).
 *
 * Nur Message-Thread; `load()`/`save()` duerfen allokieren.
 */
class AnalysisCache
{
public:
    /** Aktuelle Formatversion; aeltere Dateien werden verworfen. */
    static constexpr int kVersion = 1;
    /** Kennung im Dateikopf ("BFKAAC"). */
    static juce::String magic();

    /** Cache, der an `file` gebunden ist (Datei muss nicht existieren). */
    explicit AnalysisCache (const juce::File& file);

    /** Liest die Datei; false = fehlt/kaputt/andere Version (Cache bleibt leer). */
    bool load();

    /** Schreibt die Datei; legt den Elternordner an. */
    bool save() const;

    /** Eintrag, der zu Groesse und Aenderungszeit passt. */
    std::optional<SampleAnalysis> lookup (const juce::String& sampleKey,
                                          juce::int64 fileSize,
                                          juce::int64 modificationTime) const;

    void store (const juce::String& sampleKey,
                juce::int64 fileSize,
                juce::int64 modificationTime,
                const SampleAnalysis& analysis);

    void clear();
    int  size() const noexcept;
    juce::File file() const noexcept { return cacheFile; }

private:
    struct Entry
    {
        juce::String sampleKey;
        juce::int64  fileSize = 0;
        juce::int64  modificationTime = 0;
        SampleAnalysis analysis;
    };
    juce::File cacheFile;
    std::vector<Entry> entries;
};

} // namespace bfka
