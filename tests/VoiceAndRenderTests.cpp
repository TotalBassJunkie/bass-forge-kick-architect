#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

#include "engine/KickRenderer.h"
#include "engine/SampleVoice.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// ---------------------------------------------------------------------------
// Hinweis zur Allokationsfreiheit
//
// "renderNextBlock() allokiert nicht" laesst sich mit Catch2 nicht messen:
// es gibt keinen Haken, der malloc/free im Audio-Thread beobachtet. Die
// Zusicherung wird deshalb hier dokumentiert und konstruktiv eingehalten:
//
//  * SampleVoice haelt GAR KEINEN eigenen Puffer. Sie liest direkt aus dem
//    Quellbuffer und addiert in den Zielbuffer; es gibt keine Kopie, kein
//    juce::Array, kein juce::String, kein Log und keine Sperre im Renderpfad.
//  * KickRenderer legt seinen Summenpuffer (scratch) ausschliesslich in
//    prepare() an. renderNextBlock() arbeitet nur mit Zeigern auf diesem
//    Puffer und mit Zahlenwerten.
//  * Die Pruefung im Code-Review ist deshalb: im Renderpfad darf kein
//    Allokator, kein Log-Aufruf und kein Mutex stehen. Siehe
//    docs/specs/voice-and-render.md, Abschnitt 10.
// ---------------------------------------------------------------------------

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

    /** Gleichspannungsquelle (alle Kanaele = `value`). */
    juce::AudioBuffer<float> makeConstantSource (int    numChannels,
                                                 double sampleRate,
                                                 double seconds,
                                                 float  value)
    {
        const auto numSamples = juce::jmax (1, (int) std::lround (sampleRate * seconds));

        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int n = 0; n < numSamples; ++n)
                buffer.setSample (ch, n, value);

        return buffer;
    }

    /** Rendert `numSamples` in Bloecken von kBlockSize ab Sample 0. */
    void renderInBlocks (KickRenderer& renderer, juce::AudioBuffer<float>& target, int numSamples)
    {
        int done = 0;

        while (done < numSamples)
        {
            const auto blockSize = juce::jmin (kBlockSize, numSamples - done);
            renderer.renderNextBlock (target, done, blockSize);
            done += blockSize;
        }
    }

    /** RMS ueber einen Bereich (ueber alle ausgewerteten Kanaele gemittelt). */
    float rmsOf (const juce::AudioBuffer<float>& buffer,
                 int startSample,
                 int numSamples,
                 int numChannels)
    {
        const auto channels = juce::jmin (numChannels, buffer.getNumChannels());

        double sum   = 0.0;
        int    count = 0;

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* data = buffer.getReadPointer (ch, startSample);

            for (int n = 0; n < numSamples; ++n)
            {
                const auto value = (double) data[n];
                sum += value * value;
                ++count;
            }
        }

        return (count > 0) ? (float) std::sqrt (sum / (double) count) : 0.0f;
    }

    /** Kick-Renderer mit festen Einstellungen fuer die Summen-Tests. */
    void prepareRenderer (KickRenderer& renderer, bool cutToOneBeat, bool autoGain)
    {
        renderer.prepare (kSampleRate, kBlockSize);

        RenderSettings settings;
        settings.bpm              = 150.0;          // 1 Beat = 0,4 s
        settings.cutToOneBeat     = cutToOneBeat;
        settings.tailFadeSeconds  = 0.01;
        settings.autoGain         = autoGain;
        settings.autoGainTargetDb = 0.0f;

        renderer.setRenderSettings (settings);
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Fade-Kurven
// ---------------------------------------------------------------------------
TEST_CASE ("shapeGain: Endpunkte der linearen Kurve", "[engine][fade]")
{
    REQUIRE (SampleVoice::shapeGain (FadeShape::linear, 0.0f, 0.0f) == 0.0f);
    REQUIRE (SampleVoice::shapeGain (FadeShape::linear, 1.0f, 0.0f) == 1.0f);
    REQUIRE_THAT (SampleVoice::shapeGain (FadeShape::linear, 0.25f, 0.0f), WithinAbs (0.25f, 1.0e-6f));

    // Der Biegeparameter hat auf einer Geraden keine Wirkung.
    REQUIRE_THAT (SampleVoice::shapeGain (FadeShape::linear, 0.3f, 1.0f), WithinAbs (0.3f, 1.0e-6f));
    REQUIRE_THAT (SampleVoice::shapeGain (FadeShape::linear, 0.3f, -1.0f), WithinAbs (0.3f, 1.0e-6f));

    // Werte ausserhalb 0..1 werden geklemmt.
    REQUIRE (SampleVoice::shapeGain (FadeShape::linear, -2.0f, 0.0f) == 0.0f);
    REQUIRE (SampleVoice::shapeGain (FadeShape::linear, 2.0f, 0.0f) == 1.0f);
}

TEST_CASE ("shapeGain: alle Kurven sind monoton und treffen die Endpunkte", "[engine][fade]")
{
    const FadeShape shapes[] { FadeShape::linear, FadeShape::curve, FadeShape::sCurve,
                               FadeShape::fast, FadeShape::slow };
    const float amounts[] { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };

    constexpr int kSteps = 512;

    for (const auto shape : shapes)
    {
        for (const auto amount : amounts)
        {
            INFO ("shape = " << (int) shape << ", amount = " << amount);

            REQUIRE_THAT (SampleVoice::shapeGain (shape, 0.0f, amount), WithinAbs (0.0f, 1.0e-6f));
            REQUIRE_THAT (SampleVoice::shapeGain (shape, 1.0f, amount), WithinAbs (1.0f, 1.0e-6f));

            auto previous = 0.0f;

            for (int i = 0; i <= kSteps; ++i)
            {
                const auto t     = (float) i / (float) kSteps;
                const auto value = SampleVoice::shapeGain (shape, t, amount);

                INFO ("t = " << t << ", value = " << value);

                REQUIRE (value >= -1.0e-6f);
                REQUIRE (value <= 1.0f + 1.0e-6f);
                REQUIRE (value >= previous - 1.0e-6f);

                previous = value;
            }
        }
    }
}

TEST_CASE ("shapeGain: sCurve ist punktsymmetrisch, curve biegt in beide Richtungen", "[engine][fade]")
{
    // Smoothstep: g(0,5) = 0,5, und g(t) + g(1-t) = 1.
    REQUIRE_THAT (SampleVoice::shapeGain (FadeShape::sCurve, 0.5f, 0.0f), WithinAbs (0.5f, 1.0e-6f));
    REQUIRE_THAT (SampleVoice::shapeGain (FadeShape::sCurve, 0.25f, 0.0f)
                      + SampleVoice::shapeGain (FadeShape::sCurve, 0.75f, 0.0f),
                  WithinAbs (1.0f, 1.0e-6f));

    // curve mit amount > 0 ist konvex: der Wert liegt an 0,5 UNTER der Geraden.
    const auto convex = SampleVoice::shapeGain (FadeShape::curve, 0.5f, 0.75f);
    REQUIRE (convex < 0.5f);

    // curve mit amount < 0 ist konkav: der Wert liegt an 0,5 UEBER der Geraden.
    const auto concave = SampleVoice::shapeGain (FadeShape::curve, 0.5f, -0.75f);
    REQUIRE (concave > 0.5f);

    // amount = 0 entspricht der Geraden.
    REQUIRE_THAT (SampleVoice::shapeGain (FadeShape::curve, 0.5f, 0.0f), WithinAbs (0.5f, 1.0e-6f));

    // fast steigt schneller an als slow, beide schneller als die Gerade.
    const auto fast = SampleVoice::shapeGain (FadeShape::fast, 0.5f, 0.0f);
    const auto slow = SampleVoice::shapeGain (FadeShape::slow, 0.5f, 0.0f);

    REQUIRE (fast > slow);
    REQUIRE (slow > 0.5f);
}

// ---------------------------------------------------------------------------
// 2. Voice: Grundverhalten
// ---------------------------------------------------------------------------
TEST_CASE ("Voice mit Gleichspannung: Ausgabe ist konstant 1,0", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);
    voice.setSettings (VoiceSettings());

    voice.startAt (0);
    REQUIRE (voice.isActive());
    REQUIRE (voice.lengthSamples() == 48000);
    REQUIRE_THAT (voice.lengthSeconds(), WithinAbs (1.0, 1.0e-9));

    juce::AudioBuffer<float> target (2, 48000);
    target.clear();
    voice.renderNextBlock (target, 0, 48000);

    // Declick am Anfang: 32 Samples Anstieg, erstes Sample = 1/32.
    REQUIRE_THAT (target.getSample (0, 0), WithinAbs (1.0f / 32.0f, 1.0e-6f));
    REQUIRE_THAT (target.getSample (0, SampleVoice::kDeclickSamples - 1), WithinAbs (1.0f, 1.0e-6f));

    // Danach konstant 1,0 auf beiden Kanaelen.
    for (int n = SampleVoice::kDeclickSamples; n < 48000 - SampleVoice::kDeclickSamples; ++n)
    {
        INFO ("Sample " << n);
        REQUIRE_THAT (target.getSample (0, n), WithinAbs (1.0f, 1.0e-6f));
        REQUIRE_THAT (target.getSample (1, n), WithinAbs (1.0f, 1.0e-6f));
    }

    // Auslauf endet exakt auf 0 -> kein Klick nach der Voice.
    REQUIRE_THAT (target.getSample (0, 47999), WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_FALSE (voice.isActive());
}

TEST_CASE ("Voice rendert additiv und nicht ueberschreibend", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 0.5, 1.0f);

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);
    voice.setSettings (VoiceSettings());
    voice.startAt (0);

    juce::AudioBuffer<float> target (2, 24000);

    for (int ch = 0; ch < target.getNumChannels(); ++ch)
        for (int n = 0; n < target.getNumSamples(); ++n)
            target.setSample (ch, n, 0.25f);

    voice.renderNextBlock (target, 0, 24000);

    REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (1.25f, 1.0e-5f));
    REQUIRE_THAT (target.getSample (1, 12000), WithinAbs (1.25f, 1.0e-5f));
}

TEST_CASE ("Declick erzwingt eine Rampe auch ohne Fades", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 0.1, 1.0f);

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);
    voice.setSettings (VoiceSettings());      // fadeIn = fadeOut = 0
    voice.startAt (0);

    juce::AudioBuffer<float> target (1, 1000);
    target.clear();
    voice.renderNextBlock (target, 0, 1000);

    auto previous = 0.0f;

    for (int n = 0; n < SampleVoice::kDeclickSamples; ++n)
    {
        const auto value = target.getSample (0, n);
        INFO ("Sample " << n << " = " << value);

        REQUIRE (value > previous);
        previous = value;
    }

    // Die Rampe ist linear: 1/32, 2/32, ...
    for (int n = 0; n < SampleVoice::kDeclickSamples; ++n)
        REQUIRE_THAT (target.getSample (0, n),
                      WithinAbs ((float) (n + 1) / (float) SampleVoice::kDeclickSamples, 1.0e-6f));
}

TEST_CASE ("Trim begrenzt den gelesenen Bereich", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);

    VoiceSettings settings;
    settings.trimStartSeconds = 0.25;
    settings.trimEndSeconds   = 0.75;
    voice.setSettings (settings);

    REQUIRE (voice.lengthSamples() == 24000);
    REQUIRE_THAT (voice.lengthSeconds(), WithinAbs (0.5, 1.0e-9));

    voice.startAt (0);

    juce::AudioBuffer<float> target (2, 48000);
    target.clear();
    voice.renderNextBlock (target, 0, 48000);

    REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (1.0f, 1.0e-6f));    // Mitte des Fensters
    REQUIRE_THAT (target.getSample (0, 23999), WithinAbs (0.0f, 1.0e-6f));    // Fensterende laeuft aus
    REQUIRE_THAT (target.getSample (0, 24000), WithinAbs (0.0f, 1.0e-9f));    // dahinter still
    REQUIRE_THAT (target.getSample (0, 40000), WithinAbs (0.0f, 1.0e-9f));
    REQUIRE_FALSE (voice.isActive());
}

TEST_CASE ("TrimEnd vor TrimStart bedeutet: bis zum Ende des Samples", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);

    VoiceSettings settings;
    settings.trimStartSeconds = 0.5;
    settings.trimEndSeconds   = 0.0;          // 0 = kein Ende angegeben
    voice.setSettings (settings);

    REQUIRE (voice.lengthSamples() == 24000);
}

TEST_CASE ("Position verschiebt den Start der Voice", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);

    VoiceSettings settings;
    settings.positionSeconds = 0.1;           // 4800 Samples
    voice.setSettings (settings);

    voice.startAt (0);

    juce::AudioBuffer<float> target (2, 9600);
    target.clear();
    voice.renderNextBlock (target, 0, 9600);

    REQUIRE_THAT (target.getSample (0, 0), WithinAbs (0.0f, 1.0e-9f));
    REQUIRE_THAT (target.getSample (0, 4799), WithinAbs (0.0f, 1.0e-9f));
    REQUIRE (target.getSample (0, 4800) > 0.0f);
    REQUIRE_THAT (target.getSample (0, 4850), WithinAbs (1.0f, 1.0e-6f));

    // Negative Positionen werden als 0 behandelt (kein Vorlauf).
    VoiceSettings negative = settings;
    negative.positionSeconds = -0.5;
    voice.setSettings (negative);
    voice.startAt (0);

    juce::AudioBuffer<float> target2 (2, 4800);
    target2.clear();
    voice.renderNextBlock (target2, 0, 4800);

    REQUIRE_THAT (target2.getSample (0, SampleVoice::kDeclickSamples), WithinAbs (1.0f, 1.0e-6f));
}

TEST_CASE ("Fade-In ueber 50 Prozent der Slot-Laenge", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);   // Slot-Laenge = 1 s

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);

    VoiceSettings settings;
    settings.fadeIn.lengthSeconds = 0.5;      // 50 % von 1 s
    settings.fadeIn.shape         = FadeShape::linear;
    voice.setSettings (settings);

    voice.startAt (0);

    juce::AudioBuffer<float> target (2, 48000);
    target.clear();
    voice.renderNextBlock (target, 0, 48000);

    // Nach 25 % der Slot-Laenge ist die Haelfte des Fades erreicht.
    REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (0.5f, 1.0e-3f));
    // Nach 50 % ist der Fade fertig.
    REQUIRE_THAT (target.getSample (0, 24000), WithinAbs (1.0f, 1.0e-6f));
    // Und er bleibt fertig.
    REQUIRE_THAT (target.getSample (0, 36000), WithinAbs (1.0f, 1.0e-6f));

    // Ein Fadeschritt davor liegt unter 0,5.
    REQUIRE (target.getSample (0, 6000) < 0.5f);
}

TEST_CASE ("Fade-Out laeuft auf 0 und nutzt die gewaehlte Kurvenform", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);

    VoiceSettings settings;
    settings.fadeOut.lengthSeconds = 0.5;
    settings.fadeOut.shape         = FadeShape::linear;
    voice.setSettings (settings);

    voice.startAt (0);

    juce::AudioBuffer<float> target (2, 48000);
    target.clear();
    voice.renderNextBlock (target, 0, 48000);

    // Beginn des Fade-Outs (0,5 s): noch voller Pegel.
    REQUIRE_THAT (target.getSample (0, 24000), WithinAbs (1.0f, 1.0e-6f));
    // Mitte des Fade-Outs (0,75 s): halber Pegel.
    REQUIRE_THAT (target.getSample (0, 36000), WithinAbs (0.5f, 1.0e-3f));
    // Ende: exakt 0.
    REQUIRE_THAT (target.getSample (0, 47999), WithinAbs (0.0f, 1.0e-6f));

    // Mit der Kurve "fast" liegt der Pegel in der Fade-Mitte darueber.
    settings.fadeOut.shape = FadeShape::fast;
    voice.setSettings (settings);
    voice.startAt (0);

    juce::AudioBuffer<float> targetFast (2, 48000);
    targetFast.clear();
    voice.renderNextBlock (targetFast, 0, 48000);

    REQUIRE (targetFast.getSample (0, 36000) > target.getSample (0, 36000));
}

TEST_CASE ("Stereo bleibt getrennt, Mono wird auf beide Kanaele verteilt", "[engine][voice]")
{
    constexpr int kLength = 24000;    // 0,5 s

    SECTION ("Mono-Quelle auf Stereo-Ziel")
    {
        auto source = makeConstantSource (1, kSampleRate, 0.5, 0.75f);

        SampleVoice voice;
        voice.prepare (kSampleRate, kBlockSize);
        voice.setSource (&source, kSampleRate);
        voice.setSettings (VoiceSettings());
        voice.startAt (0);

        juce::AudioBuffer<float> target (2, kLength);
        target.clear();
        voice.renderNextBlock (target, 0, kLength);

        REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (0.75f, 1.0e-6f));
        REQUIRE_THAT (target.getSample (1, 12000), WithinAbs (0.75f, 1.0e-6f));
    }

    SECTION ("Stereo-Quelle auf Stereo-Ziel")
    {
        juce::AudioBuffer<float> source (2, kLength);

        for (int n = 0; n < kLength; ++n)
        {
            source.setSample (0, n, 1.0f);
            source.setSample (1, n, -0.5f);
        }

        SampleVoice voice;
        voice.prepare (kSampleRate, kBlockSize);
        voice.setSource (&source, kSampleRate);
        voice.setSettings (VoiceSettings());
        voice.startAt (0);

        juce::AudioBuffer<float> target (2, kLength);
        target.clear();
        voice.renderNextBlock (target, 0, kLength);

        REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (1.0f, 1.0e-6f));
        REQUIRE_THAT (target.getSample (1, 12000), WithinAbs (-0.5f, 1.0e-6f));
    }

    SECTION ("Mono-Ziel bekommt den Mittelwert einer Stereo-Quelle")
    {
        juce::AudioBuffer<float> source (2, kLength);

        for (int n = 0; n < kLength; ++n)
        {
            source.setSample (0, n, 1.0f);
            source.setSample (1, n, 0.0f);
        }

        SampleVoice voice;
        voice.prepare (kSampleRate, kBlockSize);
        voice.setSource (&source, kSampleRate);
        voice.setSettings (VoiceSettings());
        voice.startAt (0);

        juce::AudioBuffer<float> target (1, kLength);
        target.clear();
        voice.renderNextBlock (target, 0, kLength);

        REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (0.5f, 1.0e-6f));
    }
}

TEST_CASE ("Quelle mit 44,1 kHz wird auf die Ausgaberate umgerechnet", "[engine][voice]")
{
    constexpr double kSourceRate = 44100.0;

    auto source = makeConstantSource (1, kSourceRate, 1.0, 1.0f);    // 44100 Samples = 1 s

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSourceRate);
    voice.setSettings (VoiceSettings());

    // 1 s bleibt 1 s: 48000 Ausgabe-Samples, Schritt 44100/48000.
    REQUIRE (voice.lengthSamples() == 48000);
    REQUIRE_THAT (voice.readStep(), WithinAbs (kSourceRate / kSampleRate, 1.0e-12));

    voice.startAt (0);

    juce::AudioBuffer<float> target (2, 48000);
    target.clear();
    voice.renderNextBlock (target, 0, 48000);

    // Kurz vor dem Ende liegt weiterhin Pegel an, danach ist Ruhe.
    REQUIRE_THAT (target.getSample (0, 47000), WithinAbs (1.0f, 1.0e-3f));
    REQUIRE_FALSE (voice.isActive());
}

TEST_CASE ("Stumme Voice rendert nichts", "[engine][voice]")
{
    auto source = makeConstantSource (1, kSampleRate, 0.5, 1.0f);

    SampleVoice voice;
    voice.prepare (kSampleRate, kBlockSize);
    voice.setSource (&source, kSampleRate);

    VoiceSettings settings;
    settings.mute = true;
    voice.setSettings (settings);

    voice.startAt (0);
    REQUIRE_FALSE (voice.isActive());

    juce::AudioBuffer<float> target (2, 4800);
    target.clear();
    voice.renderNextBlock (target, 0, 4800);

    REQUIRE_THAT (target.getSample (0, 100), WithinAbs (0.0f, 1.0e-9f));
}

// ---------------------------------------------------------------------------
// 3. Renderer: Kick-Laenge
// ---------------------------------------------------------------------------
TEST_CASE ("beatLengthSeconds ist 60/bpm", "[engine][render]")
{
    KickRenderer renderer;

    RenderSettings settings;
    settings.bpm = 150.0;
    renderer.setRenderSettings (settings);
    REQUIRE_THAT (renderer.beatLengthSeconds(), WithinAbs (0.4, 1.0e-12));

    settings.bpm = 120.0;
    renderer.setRenderSettings (settings);
    REQUIRE_THAT (renderer.beatLengthSeconds(), WithinAbs (0.5, 1.0e-12));

    settings.bpm = 160.0;
    renderer.setRenderSettings (settings);
    REQUIRE_THAT (renderer.beatLengthSeconds(), WithinAbs (0.375, 1.0e-12));

    // Unsinniges Tempo wird geklemmt, keine Division durch 0.
    settings.bpm = 0.0;
    renderer.setRenderSettings (settings);
    REQUIRE (renderer.beatLengthSeconds() > 0.0);
}

TEST_CASE ("KICK LENGTH 1 BEAT schneidet bei 60/bpm ab", "[engine][render]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);

    KickRenderer renderer;
    prepareRenderer (renderer, true, false);      // cutToOneBeat, ohne Autogain

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    renderer.noteOn();

    REQUIRE (renderer.beatLengthSamples() == 19200);   // 0,4 s bei 48 kHz

    juce::AudioBuffer<float> target (2, 28800);        // 0,6 s
    target.clear();
    renderInBlocks (renderer, target, 28800);

    // Vor dem Beat liegt Pegel an ...
    REQUIRE_THAT (target.getSample (0, 9600), WithinAbs (1.0f, 1.0e-6f));     // 0,2 s
    // ... das letzte Sample vor dem Beat laeuft auf 0 ...
    REQUIRE_THAT (target.getSample (0, 19199), WithinAbs (0.0f, 1.0e-6f));
    // ... und ab dem Beat ist der Ausgang still.
    for (int n = 19200; n < 28800; ++n)
    {
        INFO ("Sample " << n);
        REQUIRE_THAT (target.getSample (0, n), WithinAbs (0.0f, 1.0e-6f));
        REQUIRE_THAT (target.getSample (1, n), WithinAbs (0.0f, 1.0e-6f));
    }

    REQUIRE_FALSE (renderer.isPlaying());
}

TEST_CASE ("FULL SAMPLE laesst die Voice ueber den Beat hinaus klingen", "[engine][render]")
{
    auto source = makeConstantSource (1, kSampleRate, 0.6, 1.0f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, false);

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    renderer.noteOn();

    juce::AudioBuffer<float> target (2, 28800);       // 0,6 s
    target.clear();
    renderInBlocks (renderer, target, 28800);

    // Jenseits des Beats (0,4 s) klingt die Voice weiter.
    REQUIRE_THAT (target.getSample (0, 24000), WithinAbs (1.0f, 1.0e-6f));    // 0,5 s
    REQUIRE_FALSE (renderer.isPlaying());            // am Sample-Ende ist sie fertig
}

// ---------------------------------------------------------------------------
// 4. Renderer: Summe, Mute, Solo
// ---------------------------------------------------------------------------
TEST_CASE ("Renderer summiert die Slots", "[engine][render]")
{
    auto first  = makeConstantSource (1, kSampleRate, 1.0, 1.0f);
    auto second = makeConstantSource (1, kSampleRate, 1.0, 0.25f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, false);

    renderer.setSlot (0, &first, kSampleRate, VoiceSettings());
    renderer.setSlot (1, &second, kSampleRate, VoiceSettings());

    REQUIRE (renderer.isSlotFilled (0));
    REQUIRE (renderer.isSlotFilled (1));

    renderer.noteOn();
    REQUIRE (renderer.numActiveVoices() == 2);

    juce::AudioBuffer<float> target (2, 24000);
    target.clear();
    renderInBlocks (renderer, target, 24000);

    REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (1.25f, 1.0e-6f));
    REQUIRE_THAT (target.getSample (1, 12000), WithinAbs (1.25f, 1.0e-6f));
}

TEST_CASE ("Renderer addiert auf einen bereits gefuellten Zielpuffer", "[engine][render]")
{
    auto source = makeConstantSource (1, kSampleRate, 0.5, 1.0f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, false);

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    renderer.noteOn();

    juce::AudioBuffer<float> target (2, 24000);

    for (int ch = 0; ch < target.getNumChannels(); ++ch)
        for (int n = 0; n < target.getNumSamples(); ++n)
            target.setSample (ch, n, 0.25f);

    renderInBlocks (renderer, target, 24000);

    REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (1.25f, 1.0e-5f));
}

TEST_CASE ("Mute und Solo wirken auf die Summe", "[engine][render]")
{
    auto first  = makeConstantSource (1, kSampleRate, 1.0, 1.0f);
    auto second = makeConstantSource (1, kSampleRate, 1.0, 0.25f);

    SECTION ("Solo auf Slot 0 unterdrueckt Slot 1")
    {
        KickRenderer renderer;
        prepareRenderer (renderer, false, false);

        VoiceSettings soloSettings;
        soloSettings.solo = true;

        renderer.setSlot (0, &first, kSampleRate, soloSettings);
        renderer.setSlot (1, &second, kSampleRate, VoiceSettings());

        renderer.noteOn();
        REQUIRE (renderer.numActiveVoices() == 1);

        juce::AudioBuffer<float> target (2, 24000);
        target.clear();
        renderInBlocks (renderer, target, 24000);

        // Nur Slot 0 (1,0) ist zu hoeren, nicht die Summe 1,25.
        REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (1.0f, 1.0e-6f));
    }

    SECTION ("Mute auf Slot 0 laesst nur Slot 1 klingen")
    {
        KickRenderer renderer;
        prepareRenderer (renderer, false, false);

        VoiceSettings muteSettings;
        muteSettings.mute = true;

        renderer.setSlot (0, &first, kSampleRate, muteSettings);
        renderer.setSlot (1, &second, kSampleRate, VoiceSettings());

        renderer.noteOn();
        REQUIRE (renderer.numActiveVoices() == 1);

        juce::AudioBuffer<float> target (2, 24000);
        target.clear();
        renderInBlocks (renderer, target, 24000);

        REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (0.25f, 1.0e-6f));
    }

    SECTION ("Mute schlaegt Solo")
    {
        KickRenderer renderer;
        prepareRenderer (renderer, false, false);

        VoiceSettings both;
        both.mute = true;
        both.solo = true;

        renderer.setSlot (0, &first, kSampleRate, both);
        renderer.setSlot (1, &second, kSampleRate, VoiceSettings());

        renderer.noteOn();

        // Slot 0 ist Solo, aber stumm -> es klingt nichts.
        REQUIRE (renderer.numActiveVoices() == 0);

        juce::AudioBuffer<float> target (2, 4800);
        target.clear();
        renderInBlocks (renderer, target, 4800);

        REQUIRE_THAT (target.getSample (0, 2400), WithinAbs (0.0f, 1.0e-9f));
    }

    SECTION ("Ein leerer Solo-Slot schaltet den Kick nicht stumm")
    {
        KickRenderer renderer;
        prepareRenderer (renderer, false, false);

        renderer.setSlot (0, &first, kSampleRate, VoiceSettings());

        VoiceSettings emptySolo;
        emptySolo.solo = true;
        renderer.setSlot (3, nullptr, kSampleRate, emptySolo);

        renderer.noteOn();
        REQUIRE (renderer.numActiveVoices() == 1);

        juce::AudioBuffer<float> target (2, 4800);
        target.clear();
        renderInBlocks (renderer, target, 4800);

        REQUIRE_THAT (target.getSample (0, 2400), WithinAbs (1.0f, 1.0e-6f));
    }
}

// ---------------------------------------------------------------------------
// 5. Autogain
// ---------------------------------------------------------------------------
TEST_CASE ("Autogain pegelt den Body auf den Zielwert", "[engine][render][autogain]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 0.5f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, true);          // ohne Beat-Schnitt, mit Autogain

    REQUIRE (renderer.renderSettings().autoGainTargetDb == 0.0f);
    REQUIRE_FALSE (renderer.hasBodyMeasurement());

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    REQUIRE_FALSE (renderer.hasBodyMeasurement());

    // --- Kick 1: laeuft mit Einheitsverstaerkung, danach wird gemessen -----
    renderer.noteOn();

    {
        juce::AudioBuffer<float> target (2, 48000);
        target.clear();
        renderInBlocks (renderer, target, 48000);
    }

    REQUIRE (renderer.hasBodyMeasurement());

    // Gleichspannung 0,5 -> 20*log10(0,5) = -6,0206 dBFS.
    REQUIRE_THAT (renderer.bodyRmsDb(), WithinAbs (-6.0206, 1.0e-3));

    // Korrektur auf 0 dBFS sind +6,0206 dB = Faktor 2,0.
    REQUIRE_THAT (renderer.autoGainLinear(), WithinAbs (2.0, 1.0e-3));
    REQUIRE_THAT (renderer.autoGainTargetLinear(), WithinAbs (2.0, 1.0e-3));

    // --- Kick 2: der Body liegt jetzt auf 0 dBFS --------------------------
    renderer.noteOn();

    juce::AudioBuffer<float> target (2, 48000);
    target.clear();
    renderInBlocks (renderer, target, 48000);

    // Fenster 20..120 ms = Samples 960..5759 (Body, ohne Transientenspitze).
    const auto bodyRms = rmsOf (target, 960, 4800, 2);
    REQUIRE_THAT (bodyRms, WithinAbs (1.0f, 5.0e-3f));

    // Der Kick bleibt stabil: die zweite Messung fordert keine neue Korrektur.
    REQUIRE_THAT (renderer.autoGainTargetLinear(), WithinAbs (2.0, 1.0e-3));
}

TEST_CASE ("Autogain laesst den Kick bei abgeschalteter Regelung in Ruhe", "[engine][render][autogain]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 0.5f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, false);         // autoGain = false

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    renderer.noteOn();

    juce::AudioBuffer<float> target (2, 24000);
    target.clear();
    renderInBlocks (renderer, target, 24000);

    REQUIRE_FALSE (renderer.hasBodyMeasurement());
    REQUIRE_THAT (renderer.autoGainLinear(), WithinAbs (1.0f, 1.0e-6f));
    REQUIRE_THAT (target.getSample (0, 12000), WithinAbs (0.5f, 1.0e-6f));
}

TEST_CASE ("Autogain kompensiert ein sehr leises Sample, aber nur bis zur Grenze",
           "[engine][render][autogain]")
{
    // 0,001 entspricht -60 dBFS; die Zielkorrektur waere +60 dB, geklemmt auf +24 dB.
    auto source = makeConstantSource (1, kSampleRate, 1.0, 0.001f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, true);

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    renderer.noteOn();

    juce::AudioBuffer<float> target (2, 48000);
    target.clear();
    renderInBlocks (renderer, target, 48000);

    REQUIRE (renderer.hasBodyMeasurement());
    REQUIRE_THAT (renderer.bodyRmsDb(), WithinAbs (-60.0, 1.0e-2));

    // 10^(24/20) = 15,849
    REQUIRE_THAT (renderer.autoGainLinear(), WithinAbs (15.849, 1.0e-2));
}

// ---------------------------------------------------------------------------
// 6. Renderer: Hilfslogik
// ---------------------------------------------------------------------------
TEST_CASE ("clearSlot entfernt die Belegung", "[engine][render]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, false);

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    REQUIRE (renderer.isSlotFilled (0));

    renderer.clearSlot (0);
    REQUIRE_FALSE (renderer.isSlotFilled (0));

    renderer.noteOn();
    REQUIRE (renderer.numActiveVoices() == 0);

    juce::AudioBuffer<float> target (2, 4800);
    target.clear();
    renderInBlocks (renderer, target, 4800);

    REQUIRE_THAT (target.getSample (0, 2400), WithinAbs (0.0f, 1.0e-9f));
}

TEST_CASE ("Unbelegte Slots und ungueltige Indizes sind harmlos", "[engine][render]")
{
    KickRenderer renderer;
    prepareRenderer (renderer, false, false);

    renderer.setSlot (-1, nullptr, kSampleRate, VoiceSettings());
    renderer.setSlot (99, nullptr, kSampleRate, VoiceSettings());
    renderer.clearSlot (-5);
    renderer.clearSlot (12);

    REQUIRE_FALSE (renderer.isSlotFilled (-1));
    REQUIRE_FALSE (renderer.isSlotFilled (12));
    REQUIRE_FALSE (renderer.isSlotFilled (3));

    // Ohne Belegung rendert der Renderer Stille und beendet den Kick.
    renderer.noteOn();

    juce::AudioBuffer<float> target (2, 512);
    target.clear();
    renderInBlocks (renderer, target, 512);

    REQUIRE_FALSE (renderer.isPlaying());
    REQUIRE_THAT (target.getSample (0, 100), WithinAbs (0.0f, 1.0e-9f));
}

TEST_CASE ("renderNextBlock ohne prepare und ohne noteOn ist folgenlos", "[engine][render]")
{
    KickRenderer renderer;

    juce::AudioBuffer<float> target (2, 256);
    target.clear();

    REQUIRE_FALSE (renderer.isPrepared());
    renderer.renderNextBlock (target, 0, 256);       // kein prepare()

    renderer.prepare (kSampleRate, kBlockSize);
    renderer.renderNextBlock (target, 0, 256);       // kein noteOn()

    REQUIRE_THAT (target.getSample (0, 0), WithinAbs (0.0f, 1.0e-9f));
    REQUIRE_FALSE (renderer.isPlaying());
}

TEST_CASE ("reset haelt die Wiedergabe an", "[engine][render]")
{
    auto source = makeConstantSource (1, kSampleRate, 1.0, 1.0f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, false);

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());
    renderer.noteOn();
    REQUIRE (renderer.isPlaying());

    renderer.reset();
    REQUIRE_FALSE (renderer.isPlaying());
    REQUIRE (renderer.numActiveVoices() == 0);

    juce::AudioBuffer<float> target (2, 512);
    target.clear();
    renderer.renderNextBlock (target, 0, 512);

    REQUIRE_THAT (target.getSample (0, 100), WithinAbs (0.0f, 1.0e-9f));
}

TEST_CASE ("Ein zweiter noteOn startet den Kick neu", "[engine][render]")
{
    auto source = makeConstantSource (1, kSampleRate, 0.2, 1.0f);

    KickRenderer renderer;
    prepareRenderer (renderer, false, false);

    renderer.setSlot (0, &source, kSampleRate, VoiceSettings());

    juce::AudioBuffer<float> first (2, 9600);        // 0,2 s
    first.clear();
    renderer.noteOn();
    renderInBlocks (renderer, first, 9600);
    REQUIRE_FALSE (renderer.isPlaying());

    juce::AudioBuffer<float> second (2, 9600);
    second.clear();
    renderer.noteOn();
    renderInBlocks (renderer, second, 9600);

    // Beide Durchlaeufe klingen identisch (frischer Start bei 0).
    REQUIRE_THAT (second.getSample (0, 4800), WithinAbs (first.getSample (0, 4800), 1.0e-9f));
    REQUIRE_THAT (second.getSample (0, 4800), WithinAbs (1.0f, 1.0e-6f));
}
