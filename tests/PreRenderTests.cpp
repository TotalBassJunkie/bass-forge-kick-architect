#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "engine/PreRenderCache.h"

using namespace bfka;
using Catch::Matchers::WithinRel;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kPi         = 3.14159265358979323846;

    /** Sinus auf allen Kanaelen; L und R sind bitgleich. */
    juce::AudioBuffer<float> makeSine (double frequencyHz, double seconds,
                                       int numChannels = 1, float amplitude = 0.5f)
    {
        const int numSamples = juce::jmax (1, (int) std::lround (seconds * kSampleRate));

        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float value = amplitude
                              * (float) std::sin (2.0 * kPi * frequencyHz * (double) i / kSampleRate);

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, value);
        }

        return buffer;
    }

    /** Konstantwert auf allen Kanaelen (DC). */
    juce::AudioBuffer<float> makeDc (int numSamples, float value, int numChannels = 1)
    {
        juce::AudioBuffer<float> buffer (numChannels, juce::jmax (0, numSamples));

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, value);

        return buffer;
    }

    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite ((double) data[i]))
                    return false;
        }

        return true;
    }

    double peakMagnitude (const juce::AudioBuffer<float>& buffer)
    {
        double peak = 0.0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                peak = juce::jmax (peak, std::abs ((double) data[i]));
        }

        return peak;
    }
} // namespace

// ===========================================================================
// Schluessel (Ticket B-45)
// ===========================================================================

TEST_CASE ("PreRender: Schluessel unterscheiden Pitch und Stretch", "[engine][prerender]")
{
    PreRenderKey a;
    a.sampleId       = "kick.wav";
    a.pitchSemitones = 12.0;
    a.stretchFactor  = 1.0;
    a.mode           = 1;

    PreRenderKey b = a;
    b.pitchSemitones = 7.0;

    REQUIRE_FALSE (a == b);

    // Gleiche Werte in allen vier Feldern: gleicher Schluessel.
    PreRenderKey c = a;
    REQUIRE (a == c);

    a.pitchSemitones = 7.0;
    REQUIRE (a == b);

    // Auch Stretch und Kennung muessen unterscheiden.
    PreRenderKey d = c;
    d.stretchFactor = 2.0;
    REQUIRE_FALSE (c == d);

    PreRenderKey e = c;
    e.sampleId = "snare.wav";
    REQUIRE_FALSE (c == e);
}

// ===========================================================================
// Cache und Verdraengung (Ticket B-47)
// ===========================================================================

TEST_CASE ("PreRender: Treffer und Verdraengung", "[engine][prerender]")
{
    PreRenderCache cache (2);

    REQUIRE (cache.maxEntries() == 2);
    REQUIRE (cache.size() == 0);

    auto makeEntry = [] (float value)
    {
        PreRendered rendered;
        rendered.buffer.setSize (1, 64);
        rendered.buffer.clear();
        rendered.buffer.setSample (0, 0, value);
        rendered.sampleRate = kSampleRate;
        return rendered;
    };

    PreRenderKey first;
    first.sampleId = "first";

    PreRenderKey second;
    second.sampleId = "second";

    PreRenderKey third;
    third.sampleId = "third";

    cache.put (first,  makeEntry (0.1f));
    cache.put (second, makeEntry (0.2f));
    cache.put (third,  makeEntry (0.3f));

    REQUIRE (cache.size() == 2);

    // Der zuerst abgelegte Eintrag ist der aelteste und wird verdraengt.
    REQUIRE (cache.get (first) == nullptr);
    REQUIRE (cache.get (second) != nullptr);
    REQUIRE (cache.get (third) != nullptr);

    // Bytes werden aus Laenge * Kanaele * sizeof (float) summiert.
    REQUIRE (cache.totalBytes() == 2 * 64 * (juce::int64) sizeof (float));

    cache.clear();
    REQUIRE (cache.size() == 0);
    REQUIRE (cache.totalBytes() == 0);
}

// ===========================================================================
// Rendering (Ticket B-45)
// ===========================================================================

TEST_CASE ("PreRender: repitch +12 halbiert die Laenge", "[engine][prerender]")
{
    auto buf = makeSine (100.0, 1.0);

    const auto out = PreRenderCache::render (buf, kSampleRate, 12.0, 1.0, 1);

    REQUIRE (out.sampleRate == kSampleRate);
    REQUIRE_THAT ((double) out.buffer.getNumSamples(), WithinRel (24000.0, 0.02));
    REQUIRE (allFinite (out.buffer));
}

TEST_CASE ("PreRender: keepPitch +12 haelt die Laenge", "[engine][prerender]")
{
    auto buf = makeSine (100.0, 1.0);

    const auto out = PreRenderCache::render (buf, kSampleRate, 12.0, 1.0, 0);

    REQUIRE (out.sampleRate == kSampleRate);
    REQUIRE_THAT ((double) out.buffer.getNumSamples(), WithinRel (48000.0, 0.02));
    REQUIRE (out.buffer.getNumChannels() == 1);
    REQUIRE (allFinite (out.buffer));
}

TEST_CASE ("PreRender: Grenzfaelle 0,25x und 8x", "[engine][prerender]")
{
    auto buf = makeSine (100.0, 0.25);
    const int sourceLength = buf.getNumSamples();

    const auto longer = PreRenderCache::render (buf, kSampleRate, 0.0, 8.0, 0);
    REQUIRE_THAT ((double) longer.buffer.getNumSamples(),
                  WithinRel (8.0 * (double) sourceLength, 0.03));

    const auto shorter = PreRenderCache::render (buf, kSampleRate, 0.0, 0.25, 0);
    REQUIRE_THAT ((double) shorter.buffer.getNumSamples(),
                  WithinRel (0.25 * (double) sourceLength, 0.03));

    // Ausserhalb der Grenzen wird geklemmt, nicht verworfen.
    const auto clamped = PreRenderCache::render (buf, kSampleRate, 0.0, 100.0, 0);
    REQUIRE (clamped.buffer.getNumSamples() == longer.buffer.getNumSamples());

    REQUIRE (allFinite (longer.buffer));
    REQUIRE (allFinite (shorter.buffer));
}

TEST_CASE ("PreRender: Stille und DC bleiben endlich", "[engine][prerender]")
{
    juce::AudioBuffer<float> silence (2, 9600);
    silence.clear();

    auto dc = makeDc (9600, 0.5f, 2);

    for (const int mode : { 0, 1 })
    {
        for (const double stretch : { 1.0, 2.0 })
        {
            const auto silentOut = PreRenderCache::render (silence, kSampleRate, 0.0, stretch, mode);
            REQUIRE (allFinite (silentOut.buffer));
            REQUIRE (peakMagnitude (silentOut.buffer) == 0.0);

            const auto dcOut = PreRenderCache::render (dc, kSampleRate, 0.0, stretch, mode);
            REQUIRE (allFinite (dcOut.buffer));
            REQUIRE (dcOut.buffer.getNumChannels() == 2);
            REQUIRE (peakMagnitude (dcOut.buffer) <= 1.0);
        }
    }
}

TEST_CASE ("PreRender: sehr kurzes Sample", "[engine][prerender]")
{
    juce::AudioBuffer<float> tiny (1, 32);

    for (int i = 0; i < tiny.getNumSamples(); ++i)
        tiny.setSample (0, i, 0.5f * (float) std::sin (0.3 * (double) i));

    for (const int mode : { 0, 1 })
    {
        const auto out = PreRenderCache::render (tiny, kSampleRate, 0.0, 1.0, mode);

        REQUIRE (out.buffer.getNumSamples() > 0);
        REQUIRE (allFinite (out.buffer));
    }

    // Leere Quelle und ungueltige Rate duerfen nicht werfen.
    juce::AudioBuffer<float> empty (1, 0);
    const auto emptyOut = PreRenderCache::render (empty, kSampleRate, 0.0, 1.0, 0);
    REQUIRE (emptyOut.buffer.getNumSamples() == 0);
    REQUIRE (emptyOut.sampleRate == kSampleRate);

    const auto badRate = PreRenderCache::render (tiny, 0.0, 0.0, 1.0, 0);
    REQUIRE (badRate.buffer.getNumSamples() == 0);
}
