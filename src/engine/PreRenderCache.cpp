#include "engine/PreRenderCache.h"

#include <cmath>
#include <cstddef>
#include <utility>

#include "engine/Resampler.h"
#include "engine/WsolaStretcher.h"

namespace bfka
{

namespace
{
    /** Klemmt einen Faktor auf einen endlichen Wert in [low, high]. */
    double sanitise (double value, double fallback, double low, double high) noexcept
    {
        if (! std::isfinite (value))
            return fallback;

        return juce::jlimit (low, high, value);
    }

    /**
     * Bringt `buffer` auf exakt `targetLength` Samples.

     * Zu lange Puffer werden abgeschnitten, zu kurze mit Stille aufgefuellt.
     * Die Kanalzahl bleibt erhalten; ein leerer Puffer wird neu angelegt.
     */
    void fitLength (juce::AudioBuffer<float>& buffer, int targetLength, int numChannels)
    {
        const int wanted = juce::jmax (0, targetLength);

        if (buffer.getNumChannels() <= 0)
        {
            buffer.setSize (juce::jmax (1, numChannels), wanted);
            buffer.clear();
            return;
        }

        if (buffer.getNumSamples() == wanted)
            return;

        if (buffer.getNumSamples() > wanted)
        {
            buffer.setSize (buffer.getNumChannels(), wanted, true, true, true);
            return;
        }

        const int previous = buffer.getNumSamples();

        buffer.setSize (buffer.getNumChannels(), wanted, true, true, true);

        // Neu angehaengte Samples sind bei `clearExtraSpace` bereits still;
        // das explizite Loeschen macht die Absicht sichtbar (B-47).
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, previous, wanted - previous);
    }
} // namespace

// ---------------------------------------------------------------------------
// Schluessel
// ---------------------------------------------------------------------------

bool PreRenderKey::operator== (const PreRenderKey& other) const noexcept
{
    return mode == other.mode
        && pitchSemitones == other.pitchSemitones
        && stretchFactor == other.stretchFactor
        && sampleId == other.sampleId;
}

// ---------------------------------------------------------------------------
// Cache
// ---------------------------------------------------------------------------

PreRenderCache::PreRenderCache (int maxEntriesToKeep)
    : maxEntryCount (juce::jmax (1, maxEntriesToKeep))
{
}

void PreRenderCache::touch (std::size_t index)
{
    if (index >= lastUse.size())
        return;

    ++useCounter;
    lastUse[index] = useCounter;
}

std::shared_ptr<const PreRendered> PreRenderCache::get (const PreRenderKey& key)
{
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].first == key)
        {
            touch (i);
            return entries[i].second;
        }
    }

    return nullptr;
}

void PreRenderCache::put (const PreRenderKey& key, PreRendered value)
{
    // PreRendered ist ein Aggregat (kein eigener Konstruktor), deshalb erst
    // anlegen und dann verschieben statt make_shared mit einem Argument.
    auto shared = std::make_shared<PreRendered>();
    *shared = std::move (value);

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].first == key)
        {
            // Gleicher Schluessel: ersetzen, Position im LRU bleibt erhalten.
            entries[i].second = shared;
            touch (i);
            evictIfNeeded();
            return;
        }
    }

    entries.emplace_back (key, shared);
    lastUse.push_back (0);

    touch (entries.size() - 1);

    evictIfNeeded();
}

void PreRenderCache::evictIfNeeded()
{
    while ((int) entries.size() > maxEntryCount)
    {
        std::size_t oldest = 0;

        for (std::size_t i = 1; i < lastUse.size(); ++i)
            if (lastUse[i] < lastUse[oldest])
                oldest = i;

        entries.erase (entries.begin() + (std::ptrdiff_t) oldest);

        if (oldest < lastUse.size())
            lastUse.erase (lastUse.begin() + (std::ptrdiff_t) oldest);
    }
}

std::shared_ptr<const PreRendered> PreRenderCache::getOrRender (const PreRenderKey& key,
                                                               const juce::AudioBuffer<float>& source,
                                                               double sourceSampleRate)
{
    if (auto hit = get (key))
        return hit;

    put (key, render (source, sourceSampleRate, key.pitchSemitones,
                      key.stretchFactor, key.mode));

    return get (key);
}

int PreRenderCache::size() const noexcept
{
    return (int) entries.size();
}

juce::int64 PreRenderCache::totalBytes() const noexcept
{
    juce::int64 bytes = 0;

    for (const auto& entry : entries)
    {
        if (entry.second == nullptr)
            continue;

        bytes += (juce::int64) entry.second->buffer.getNumSamples()
               * (juce::int64) entry.second->buffer.getNumChannels()
               * (juce::int64) sizeof (float);
    }

    return bytes;
}

void PreRenderCache::clear()
{
    entries.clear();
    lastUse.clear();
    useCounter = 0;
}

int PreRenderCache::maxEntries() const noexcept
{
    return maxEntryCount;
}

// ---------------------------------------------------------------------------
// Renderer (Ticket B-45)
// ---------------------------------------------------------------------------

PreRendered PreRenderCache::render (const juce::AudioBuffer<float>& source,
                                    double sourceSampleRate,
                                    double pitchSemitones,
                                    double stretchFactor,
                                    int mode)
{
    PreRendered result;

    // Die Ausgabe bleibt bei der Quellrate, auch im Fehlerfall (B-47).
    result.sampleRate = sourceSampleRate;

    const int numChannels = source.getNumChannels();
    const int numSamples  = source.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0 || ! std::isfinite (sourceSampleRate)
        || sourceSampleRate <= 0.0)
    {
        const int safeChannels = juce::jmax (1, numChannels);
        result.buffer.setSize (safeChannels, 0);
        result.buffer.clear();
        return result;
    }

    const double pitch   = sanitise (pitchSemitones, 0.0, -24.0, 24.0);
    const double stretch = sanitise (stretchFactor, 1.0, minStretch, maxStretch);

    const double ratio = std::pow (2.0, pitch / 12.0);

    if (mode == 1)
    {
        // REPITCH: Tonhoehe und Laenge haengen zusammen, also nur Resampling.
        // Stufe 1 stimmt die Tonhoehe. Stufe 2 skaliert die Zeit.
        // Abweichung von der Ticket-Formulierung "1.0 / stretch": der Resampler
        // rechnet Ausgabelaenge/Eingabelaenge, also bedeutet 1/stretch eine
        // Verkuerzung. Gewollt und ueberall sonst im Projekt so definiert ist
        // "stretch > 1 = laenger", deshalb geht hier `stretch` in den Aufruf.
        Resampler resampler;
        resampler.setQuality (ResamplerQuality::cubic);
        resampler.reset (numChannels);

        juce::AudioBuffer<float> stage1;

        if (ratio != 1.0)
            resampler.process (source, stage1, 1.0 / ratio);
        else
            stage1.makeCopyOf (source);

        if (stretch != 1.0)
        {
            Resampler timeStage;
            timeStage.setQuality (ResamplerQuality::cubic);
            timeStage.reset (numChannels);
            timeStage.process (stage1, result.buffer, stretch);
        }
        else
        {
            result.buffer.makeCopyOf (stage1);
        }

        return result;
    }

    // KEEP PITCH: Resampling verschiebt die Tonhoehe, WSOLA holt die Laenge
    // zurueck. Der WSOLA-Faktor enthaelt deshalb die Pitch-Ratio, damit die
    // Endlaenge genau `source * stretch` trifft.
    juce::AudioBuffer<float> shifted;

    if (ratio != 1.0)
    {
        Resampler resampler;
        resampler.setQuality (ResamplerQuality::cubic);
        resampler.reset (numChannels);
        resampler.process (source, shifted, 1.0 / ratio);
    }
    else
    {
        shifted.makeCopyOf (source);
    }

    const int targetLength = juce::jlimit (0, Resampler::maxOutputLength,
                                           (int) std::lround ((double) numSamples * stretch));

    if (shifted.getNumSamples() <= 0)
    {
        result.buffer.setSize (numChannels, targetLength);
        result.buffer.clear();
        return result;
    }

    if (targetLength <= 0)
    {
        result.buffer.setSize (numChannels, 0);
        result.buffer.clear();
        return result;
    }

    WsolaStretcher stretcher;
    stretcher.prepare (sourceSampleRate, numChannels);

    juce::AudioBuffer<float> stretched;
    stretcher.process (shifted, stretched, stretch * ratio, true);

    fitLength (stretched, targetLength, numChannels);
    result.buffer.makeCopyOf (stretched);

    return result;
}

} // namespace bfka
