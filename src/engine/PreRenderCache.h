#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Vorab-Rendering fuer Pitch und Stretch (Tickets B-45/B-47).
 *
 * Der Renderer bildet die Kombination nach, die `SampleVoice` sonst pro Sample
 * rechnet. Ergebnis und Schluessel werden in einem kleinen LRU-Cache gehalten,
 * damit ein Reglerzug nicht bei jeder Aenderung neu rendert.
 *
 * Der Cache haelt nur fertige Puffer; die Quelle selbst wird nicht kopiert.
 */
namespace bfka
{

/** Schluessel eines vorgerenderten Puffers (Ticket B-45). */
struct PreRenderKey
{
    juce::String sampleId;          ///< stabile Sample-Kennung (Pfad oder Hash)
    double       pitchSemitones = 0.0;
    double       stretchFactor  = 1.0;
    int          mode           = 0; ///< 0 = keepPitch, 1 = repitch (StretchMode)

    bool operator== (const PreRenderKey& other) const noexcept;
};

/** Ein vorgerendertes Ergebnis. */
struct PreRendered
{
    juce::AudioBuffer<float> buffer;      ///< fertiges Signal
    double sampleRate = 48000.0;          ///< Rate des Puffers
};

/**
 * Vorab-Renderer und LRU-Cache fuer Pitch/Stretch (B-45/B-47).
 *
 * Der Renderer wendet auf einen Quellpuffer dieselbe Kombination an, die
 * `SampleVoice` sonst pro Sample rechnet: bei `repitch` genuegt Resampling,
 * bei `keepPitch` laeuft Resampling + WSOLA-Rueckfuehrung.
 *
 * Threading: nur Message-Thread bzw. Offline-Pfade; `render()` allokiert.
 */
class PreRenderCache
{
public:
    /** Grenzen wie WsolaStretcher. */
    static constexpr double minStretch = 0.25;
    static constexpr double maxStretch = 8.0;

    explicit PreRenderCache (int maxEntriesToKeep = 16);

    /** Rendert Quelle + Pitch + Stretch zu einem neuen Puffer (B-45). */
    static PreRendered render (const juce::AudioBuffer<float>& source,
                               double sourceSampleRate,
                               double pitchSemitones,
                               double stretchFactor,
                               int mode);

    /** Holt einen Eintrag; `nullptr`, wenn nicht im Cache. Zaehlt als Zugriff. */
    std::shared_ptr<const PreRendered> get (const PreRenderKey& key);

    /** Legt einen Eintrag ab (ersetzt gleiche Schluessel) und verdraengt LRU. */
    void put (const PreRenderKey& key, PreRendered value);

    /** Bequemlichkeit: get(), sonst rendern, ablegen und liefern. */
    std::shared_ptr<const PreRendered> getOrRender (const PreRenderKey& key,
                                                    const juce::AudioBuffer<float>& source,
                                                    double sourceSampleRate);

    int          size() const noexcept;
    juce::int64  totalBytes() const noexcept;
    void         clear();
    int          maxEntries() const noexcept;

private:
    // Feldname weicht vom Vertragstext ab: ein Datenelement und eine Methode
    // duerfen in C++ nicht denselben Namen tragen. maxEntries() bleibt Zugriff.
    int maxEntryCount = 16;

    // LRU-Gewicht steht in `lastUse` (parallel zu `entries`); verdraengt wird
    // der Eintrag mit dem kleinsten Wert, also der am laengsten unbenutzte.
    std::vector<std::pair<PreRenderKey, std::shared_ptr<PreRendered>>> entries;
    std::vector<std::uint64_t> lastUse;
    std::uint64_t useCounter = 0;

    void touch (std::size_t index);
    void evictIfNeeded();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreRenderCache)
};

} // namespace bfka
