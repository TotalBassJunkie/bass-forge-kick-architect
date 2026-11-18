// B-110: Tests fuer den Offline-Renderer.

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <juce_events/juce_events.h>

#include "engine/OfflineRenderer.h"
#include "engine/KickRenderer.h"
#include "fx/FxChainProcessor.h"
#include "core/KickState.h"

using namespace bfka;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 512;

    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Sinus-Sample mit `seconds` Dauer (60 Hz, 48 kHz). */
    [[maybe_unused]] juce::AudioBuffer<float> makeSample (double seconds)
    {
        const int samples = (int) std::lround (seconds * kSampleRate);
        juce::AudioBuffer<float> buffer (2, samples);

        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < samples; ++s)
                buffer.setSample (ch, s, (float) (0.5 * std::sin (2.0 * 3.14159265358979 * 60.0
                                                                  * (double) s / kSampleRate)));

        return buffer;
    }

    /** Renderer ohne Slots, aber einsatzbereit. */
    void prepareEmptyRenderer (KickRenderer& renderer)
    {
        renderer.prepare (kSampleRate, kBlockSize);
    }

    /** Settings mit benannten Zuweisungen (Feldreihenfolge im Header). */
    RenderSettings makeSettings()
    {
        RenderSettings settings;
        settings.bpm              = 150.0;
        settings.cutToOneBeat     = true;
        settings.tailFadeSeconds  = 0.01;
        settings.autoGain         = false;
        settings.autoGainTargetDb = 0.0f;
        return settings;
    }
} // namespace

TEST_CASE ("Offline: leere Kette liefert die Kick-Laenge", "[engine][offline]")
{
    JuceGuiScope gui;

    KickRenderer r;
    prepareEmptyRenderer (r);
    r.setRenderSettings (makeSettings());

    FxChainProcessor fx;
    fx.prepare (kSampleRate, kBlockSize);

    FxChain leer;
    fx.setChain (leer);

    auto res = OfflineRenderer::render (r, fx, kSampleRate, kBlockSize, 0.5);

    REQUIRE (res.ok);
    REQUIRE (res.buffer.getNumChannels() == 2);
    REQUIRE (res.buffer.getNumSamples() == 24000);
    REQUIRE (res.sampleRate == kSampleRate);
}

TEST_CASE ("Offline: Stille ohne Slots", "[engine][offline]")
{
    JuceGuiScope gui;

    KickRenderer r;
    prepareEmptyRenderer (r);
    r.setRenderSettings (makeSettings());

    FxChainProcessor fx;
    fx.prepare (kSampleRate, kBlockSize);

    FxChain leer;
    fx.setChain (leer);

    auto res = OfflineRenderer::render (r, fx, kSampleRate, kBlockSize, 0.2);

    REQUIRE (res.ok);

    bool allZero = true;

    for (int ch = 0; ch < res.buffer.getNumChannels() && allZero; ++ch)
        for (int s = 0; s < res.buffer.getNumSamples(); ++s)
            if (res.buffer.getSample (ch, s) != 0.0f)
            {
                allZero = false;
                break;
            }

    REQUIRE (allZero);
}

TEST_CASE ("Offline: Blockanzahl", "[engine][offline]")
{
    REQUIRE (OfflineRenderer::blockCountFor (0.5, 48000.0, 512) == 47);
    REQUIRE (OfflineRenderer::blockCountFor (0.0, 48000.0, 512) == 0);
}

TEST_CASE ("Offline: ungueltige Rate ist ein Fehler", "[engine][offline]")
{
    JuceGuiScope gui;

    KickRenderer r;
    prepareEmptyRenderer (r);

    FxChainProcessor fx;
    fx.prepare (kSampleRate, kBlockSize);

    REQUIRE_FALSE (OfflineRenderer::render (r, fx, 0.0, kBlockSize, 0.1).ok);
}
