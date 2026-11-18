#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/PhaseAlign.h"

// B-56: Tests fuer PHASE ALIGN (Korrelation im Low-End, Nudge, Polaritaet).

using namespace bfka;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr double kPi         = juce::MathConstants<double>::pi;

    /** Reproduzierbares Rauschen; L und R sind bitgleich. */
    juce::AudioBuffer<float> makeNoise (int samples, unsigned seed)
    {
        const int length = juce::jmax (1, samples);

        juce::AudioBuffer<float> buffer (2, length);
        juce::Random random ((juce::int64) seed);

        for (int i = 0; i < length; ++i)
        {
            const float value = 0.3f * (random.nextFloat() * 2.0f - 1.0f);

            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample (ch, i, value);
        }

        return buffer;
    }

    /** Sinus auf beiden Kanaelen; L und R sind bitgleich. */
    juce::AudioBuffer<float> makeSine (double hz, int samples, double rate)
    {
        const int length = juce::jmax (1, samples);

        juce::AudioBuffer<float> buffer (2, length);

        for (int i = 0; i < length; ++i)
        {
            const float value = 0.4f
                              * (float) std::sin (2.0 * kPi * hz * (double) i / rate);

            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample (ch, i, value);
        }

        return buffer;
    }

    /** Schiebt `buffer` um `delay` Samples nach hinten (vorne Stille). */
    juce::AudioBuffer<float> delayed (const juce::AudioBuffer<float>& buffer, int delay)
    {
        const int length = buffer.getNumSamples();

        juce::AudioBuffer<float> out (buffer.getNumChannels(), length);
        out.clear();

        for (int i = 0; i < length; ++i)
        {
            const int sourceIndex = i - delay;

            if (sourceIndex >= 0 && sourceIndex < length)
                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.setSample (ch, i, buffer.getSample (ch, sourceIndex));
        }

        return out;
    }
} // namespace

// ===========================================================================
// Suche und Polaritaet
// ===========================================================================

TEST_CASE ("Phase: Verschiebung wird gefunden", "[engine][phase]")
{
    auto ref = makeNoise (4096, 20261118u);
    auto tgt = delayed (ref, 10);

    auto r = PhaseAlign::align (ref, tgt, 64, 200.0, 48000.0);

    REQUIRE (r.nudgeSamples <= 0);
    REQUIRE (std::abs (r.nudgeSamples) == 10);
    REQUIRE (r.correlationAfter > 0.9);
}

TEST_CASE ("Phase: invertiertes Signal wird geflippt", "[engine][phase]")
{
    auto ref = makeNoise (4096, 20261118u);

    juce::AudioBuffer<float> tgt (ref.getNumChannels(), ref.getNumSamples());

    for (int ch = 0; ch < ref.getNumChannels(); ++ch)
        for (int i = 0; i < ref.getNumSamples(); ++i)
            tgt.setSample (ch, i, -ref.getSample (ch, i));

    auto r = PhaseAlign::align (ref, tgt, 64, 200.0, 48000.0);

    REQUIRE (r.polarityFlip);
    REQUIRE (r.correlationBefore < 0.0);
    REQUIRE (r.correlationAfter > 0.9);
}

TEST_CASE ("Phase: verbessert die Korrelation", "[engine][phase]")
{
    auto ref = makeNoise (4096, 20261118u);
    auto tgt = delayed (ref, 10);

    auto r = PhaseAlign::align (ref, tgt, 64, 200.0, 48000.0);

    REQUIRE (r.improved);
    REQUIRE (std::abs (r.correlationAfter) > std::abs (r.correlationBefore));
}

// ===========================================================================
// Anwendung der Korrektur
// ===========================================================================

TEST_CASE ("Phase: apply verschiebt", "[engine][phase]")
{
    // Impuls bei Index 0: die Verschiebung ist damit direkt ablesbar.
    juce::AudioBuffer<float> tgt (2, 512);
    tgt.clear();
    tgt.setSample (0, 0, 1.0f);
    tgt.setSample (1, 0, 1.0f);

    auto out = PhaseAlign::apply (tgt, 5, false);

    REQUIRE (out.getNumSamples() == tgt.getNumSamples());
    REQUIRE (std::abs (out.getSample (0, 5) - tgt.getSample (0, 0)) < 1e-6);
    REQUIRE (out.getSample (0, 0) == 0.0f);
}

TEST_CASE ("Phase: apply invertiert", "[engine][phase]")
{
    // Konstanter Offset statt Sinus: der erste Sample ist nicht null.
    juce::AudioBuffer<float> tgt (2, 512);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            tgt.setSample (ch, i, 0.25f);

    auto out = PhaseAlign::apply (tgt, 0, true);

    REQUIRE (std::abs (out.getSample (0, 0) + tgt.getSample (0, 0)) < 1e-6);
    REQUIRE (out.getSample (0, 0) < 0.0f);
}

// ===========================================================================
// Tiefton (der Filter soll das Nutzband nicht verdrehen)
// ===========================================================================

TEST_CASE ("Phase: Tiefton-Sinus wird ebenfalls ausgerichtet", "[engine][phase]")
{
    auto ref = makeSine (60.0, 4096, kSampleRate);
    auto tgt = delayed (ref, 10);

    auto r = PhaseAlign::align (ref, tgt, 64, 200.0, kSampleRate);

    REQUIRE (r.nudgeSamples == -10);
    REQUIRE (r.correlationAfter > 0.99);
    REQUIRE_FALSE (r.polarityFlip);
}

// ===========================================================================
// Grenzfaelle
// ===========================================================================

TEST_CASE ("Phase: leere Puffer sind harmlos", "[engine][phase]")
{
    REQUIRE_NOTHROW (PhaseAlign::align ({}, {}, 16, 200.0, 48000.0));
}
