/*
 * B-26: Analyse-Cache (binaer, versioniert, inkrementell).
 *
 * Dateiformat (little endian):
 *   Kopf : 6 Bytes "BFKAAC", int32 Version, int32 Anzahl Eintraege.
 *   Eintrag: int32 Schluessellaenge + UTF-8-Bytes, int64 fileSize,
 *            int64 modificationTime, int32 keyNote, double bpm,
 *            double onsetSeconds, double lengthSeconds, double confidence.
 *
 * Gelesen wird ueber juce::FileInputStream, geschrieben ueber
 * juce::FileOutputStream; beide Methoden arbeiten textfrei, daher sind keine
 * Zeilenenden oder Kodierungen im Spiel.
 */
#include "library/analysis/AnalysisCache.h"

#include <cstring>
#include <utility>

namespace bfka
{

namespace
{
    /** Laenge der Kennung im Dateikopf ("BFKAAC"). */
    constexpr int kMagicLength = 6;

    /** Obere Schranke fuer einen Schluessel (Schutz gegen kaputte Dateien). */
    constexpr int kMaxKeyBytes = 4096;

    /** Obere Schranke fuer die Eintragszahl (reserve() soll nicht abstuerzen). */
    constexpr int kMaxEntries = 100000;

    /** Feste Nutzlast je Eintrag nach den Schluessel-Bytes. */
    constexpr juce::int64 kEntryPayloadBytes = 52; // 2x int64 + 5x 4/8-Byte-Werte

    /** Kleinster moeglicher Eintrag: Laengenfeld + Nutzlast (leerer Schluessel). */
    constexpr juce::int64 kMinEntryBytes = 4 + kEntryPayloadBytes;

    /** Kopiert die Kennung in einen Rohpuffer ohne Nullbyte. */
    void copyMagic (char* target)
    {
        const auto text = AnalysisCache::magic();
        std::memcpy (target, text.toRawUTF8(), (size_t) kMagicLength);
    }

    /** Reichen die Restbytes fuer den naechsten Lesevorgang? */
    bool hasBytes (juce::InputStream& in, juce::int64 needed) noexcept
    {
        return in.getNumBytesRemaining() >= needed;
    }

    /** Liest genau `count` Bytes; true nur bei vollstaendigem Lesevorgang. */
    bool readExact (juce::InputStream& in, void* target, int count) noexcept
    {
        return (juce::int64) in.read (target, count) == (juce::int64) count;
    }
} // namespace

juce::String AnalysisCache::magic()
{
    return juce::String ("BFKAAC");
}

AnalysisCache::AnalysisCache (const juce::File& file)
    : cacheFile (file)
{
}

bool AnalysisCache::load()
{
    entries.clear();

    if (! cacheFile.existsAsFile())
        return false;

    juce::FileInputStream in (cacheFile);

    if (! in.openedOk())
        return false;

    char header[kMagicLength] = {};

    if (! readExact (in, header, kMagicLength))
        return false;

    char expected[kMagicLength] = {};
    copyMagic (expected);

    if (std::memcmp (header, expected, (size_t) kMagicLength) != 0)
        return false;

    // Version und Anzahl brauchen 8 weitere Bytes.
    if (! hasBytes (in, 8))
        return false;

    const int version = in.readInt();

    if (version != kVersion)
        return false;

    const int count = in.readInt();

    if (count < 0 || count > kMaxEntries)
        return false;

    std::vector<Entry> loaded;

    // reserve() nie mit der ungeprueften Anzahl fuellen: eine kaputte Datei
    // mit riesigem Zaehler soll keinen Speicher anfordern. Die Obergrenze
    // richtet sich nach den tatsaechlich vorhandenen Restbytes.
    const auto byBytes = (size_t) juce::jmax ((juce::int64) 0, in.getNumBytesRemaining() / kMinEntryBytes);
    loaded.reserve (juce::jmin ((size_t) count, byBytes));

    for (int i = 0; i < count; ++i)
    {
        // Schluessellaenge selbst muss vorhanden sein.
        if (! hasBytes (in, 4))
        {
            entries.clear();
            return false;
        }

        const int keyLength = in.readInt();

        if (keyLength < 0 || keyLength > kMaxKeyBytes)
        {
            entries.clear();
            return false;
        }

        juce::String key;

        if (keyLength > 0)
        {
            if (! hasBytes (in, keyLength))
            {
                entries.clear();
                return false;
            }

            std::vector<char> bytes ((size_t) keyLength + 1, 0);

            if (! readExact (in, bytes.data(), keyLength))
            {
                entries.clear();
                return false;
            }

            key = juce::String::fromUTF8 (bytes.data(), keyLength);
        }

        // Restliche Felder muessen vollstaendig da sein (keine Debug-Asserts
        // in readInt/readDouble bei abgeschnittenen Dateien).
        if (! hasBytes (in, kEntryPayloadBytes))
        {
            entries.clear();
            return false;
        }

        Entry entry;
        entry.sampleKey         = key;
        entry.fileSize          = in.readInt64();
        entry.modificationTime  = in.readInt64();
        entry.analysis.keyNote        = in.readInt();
        entry.analysis.bpm            = in.readDouble();
        entry.analysis.onsetSeconds   = in.readDouble();
        entry.analysis.lengthSeconds  = in.readDouble();
        entry.analysis.confidence     = in.readDouble();

        loaded.push_back (std::move (entry));
    }

    entries = std::move (loaded);
    return true;
}

bool AnalysisCache::save() const
{
    const auto parent = cacheFile.getParentDirectory();

    if (! parent.isDirectory() && parent.getFullPathName().isNotEmpty())
        parent.createDirectory();

    // FileOutputStream oeffnet mit Truncate; ein alter, laengerer Stand kann
    // damit keine Reste hinterlassen.
    juce::FileOutputStream out (cacheFile);

    if (! out.openedOk())
        return false;

    char header[kMagicLength] = {};
    copyMagic (header);

    // Jeder Schreibvorgang meldet seinen Erfolg; bei einem Fehler werden die
    // restlichen Aufrufe uebersprungen (Kurzschluss).
    bool ok = out.write (header, (size_t) kMagicLength);
    ok = ok && out.writeInt (kVersion);
    ok = ok && out.writeInt ((int) entries.size());

    for (const auto& entry : entries)
    {
        const auto keyLength = (int) entry.sampleKey.getNumBytesAsUTF8();
        const auto utf8 = entry.sampleKey.toRawUTF8();

        ok = ok && out.writeInt (keyLength);
        ok = ok && out.write (utf8, (size_t) keyLength);

        ok = ok && out.writeInt64 (entry.fileSize);
        ok = ok && out.writeInt64 (entry.modificationTime);
        ok = ok && out.writeInt (entry.analysis.keyNote);
        ok = ok && out.writeDouble (entry.analysis.bpm);
        ok = ok && out.writeDouble (entry.analysis.onsetSeconds);
        ok = ok && out.writeDouble (entry.analysis.lengthSeconds);
        ok = ok && out.writeDouble (entry.analysis.confidence);
    }

    out.flush();

    return ok;
}

std::optional<SampleAnalysis> AnalysisCache::lookup (const juce::String& sampleKey,
                                                    juce::int64 fileSize,
                                                    juce::int64 modificationTime) const
{
    for (const auto& entry : entries)
    {
        if (entry.sampleKey != sampleKey)
            continue;

        if (entry.fileSize != fileSize || entry.modificationTime != modificationTime)
            return std::nullopt;

        return entry.analysis;
    }

    return std::nullopt;
}

void AnalysisCache::store (const juce::String& sampleKey,
                           juce::int64 fileSize,
                           juce::int64 modificationTime,
                           const SampleAnalysis& analysis)
{
    for (auto& entry : entries)
    {
        if (entry.sampleKey != sampleKey)
            continue;

        entry.fileSize         = fileSize;
        entry.modificationTime = modificationTime;
        entry.analysis         = analysis;
        return;
    }

    Entry entry;
    entry.sampleKey        = sampleKey;
    entry.fileSize         = fileSize;
    entry.modificationTime = modificationTime;
    entry.analysis         = analysis;

    entries.push_back (std::move (entry));
}

void AnalysisCache::clear()
{
    entries.clear();
}

int AnalysisCache::size() const noexcept
{
    return (int) entries.size();
}

} // namespace bfka
