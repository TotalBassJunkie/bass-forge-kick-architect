#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

#include "engine/KickAssembler.h"
#include "engine/KickRenderer.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 512;

    /** Schreibt ein kurzes WAV (Sinus) an den angegebenen Ort. */
    juce::File writeSine (const juce::File& root, const juce::String& relPath,
                          double frequency, double seconds, float amplitude)
    {
        const auto file = root.getChildFile (relPath);
        file.getParentDirectory().createDirectory();

        const auto numSamples = (int) (seconds * kSampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int n = 0; n < numSamples; ++n)
                data[n] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                        * frequency * n / kSampleRate);
        }

        juce::WavAudioFormat format;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

        if (stream == nullptr)
            return {};

        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), kSampleRate, 2, 24, {}, 0));

        if (writer == nullptr)
            return {};

        stream.release();   // der Writer besitzt den Stream jetzt
        writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        writer.reset();

        return file;
    }

    /** Erzeugt eine Mini-Library mit der erwarteten Ordnerstruktur. */
    juce::File makeMiniLibrary()
    {
        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("bfka-assembler-test");
        root.deleteRecursively();
        root.createDirectory();

        writeSine (root, "Main - Punches/Techno/Punch Test 1 150.wav", 80.0,  0.20, 0.5f);
        writeSine (root, "Main - Rumbles/Rumbles (Techno)/Rumble Test 1 150.wav", 50.0, 0.40, 0.4f);
        writeSine (root, "Transients (Clicks)/Transient Click Test 1 150.wav", 3000.0, 0.02, 0.6f);

        return root;
    }

    double peakOf (const juce::AudioBuffer<float>& buffer)
    {
        double peak = 0.0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            peak = juce::jmax (peak, (double) buffer.getMagnitude (ch, 0, buffer.getNumSamples()));

        return peak;
    }
}

TEST_CASE ("Assembler baut einen spielbaren Kick aus der Library", "[engine][assembler]")
{
    const auto root = makeMiniLibrary();
    REQUIRE (root.isDirectory());

    KickRenderer renderer;
    renderer.prepare (kSampleRate, kBlockSize);

    KickAssembler assembler;
    juce::StringArray warnings;

    const auto ok = assembler.buildDemoKick (root, renderer, warnings);

    INFO ("Warnungen: " << warnings.joinIntoString ("; "));
    REQUIRE (ok);
    REQUIRE (warnings.isEmpty());

    renderer.noteOn();
    REQUIRE (renderer.isPlaying());

    // Einen kompletten Beat rendern und pruefen, dass Ton herauskommt.
    // Der Renderer arbeitet blockweise (max. `maxBlockSize` Samples je Aufruf),
    // deshalb wird wie im Host in 512er-Blöcken gerendert.
    const auto beatSamples = (int) std::lround (renderer.beatLengthSeconds() * kSampleRate);
    REQUIRE (beatSamples > 0);

    juce::AudioBuffer<float> buffer (2, beatSamples);
    buffer.clear();

    for (int offset = 0; offset < beatSamples; offset += kBlockSize)
        renderer.renderNextBlock (buffer, offset, juce::jmin (kBlockSize, beatSamples - offset));

    const auto peak = peakOf (buffer);
    INFO ("Spitzenwert: " << peak);
    REQUIRE (peak > 0.01);
    REQUIRE (peak < 8.0);          // kein Ausreisser

    // Nach dem Beat laeuft er aus und ist danach still (KICK LENGTH = 1 BEAT).
    juce::AudioBuffer<float> after (2, 256);
    after.clear();
    renderer.renderNextBlock (after, 0, 256);
    INFO ("Peak nach dem Beat: " << peakOf (after));
    REQUIRE (peakOf (after) < 1.0e-4);
    REQUIRE_FALSE (renderer.isPlaying());

    root.deleteRecursively();
}

TEST_CASE ("Assembler meldet eine fehlende Library statt abzustuerzen", "[engine][assembler]")
{
    auto missing = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("bfka-gibt-es-nicht");

    KickRenderer renderer;
    renderer.prepare (kSampleRate, kBlockSize);

    KickAssembler assembler;
    juce::StringArray warnings;

    REQUIRE_FALSE (assembler.buildDemoKick (missing, renderer, warnings));
    REQUIRE_FALSE (warnings.isEmpty());
    REQUIRE_FALSE (renderer.isPlaying());
}

TEST_CASE ("Slot-Parameter werden in Anzeigewerte uebersetzt", "[engine][assembler]")
{
    KickState state;

    // Werkseinstellungen: 0 dB, kein Trim, keine Fades.
    const auto defaults = KickAssembler::voiceSettingsForSlot (state, 0, 1.0);
    REQUIRE_THAT (defaults.gainLinear, WithinAbs (1.0f, 1.0e-3));
    REQUIRE_THAT (defaults.trimStartSeconds, WithinAbs (0.0, 1.0e-9));
    REQUIRE_FALSE (defaults.mute);
    REQUIRE_FALSE (defaults.solo);

    // Mute und Solo werden durchgereicht.
    state.setParam ("l2_mute", 1.0f);
    state.setParam ("l2_solo", 1.0f);

    const auto muted = KickAssembler::voiceSettingsForSlot (state, 2, 1.0);
    REQUIRE (muted.mute);
    REQUIRE (muted.solo);

    // Trim-Ende 50 % einer 2-Sekunden-Datei = 1 Sekunde.
    state.setParam ("l3_trimend", 0.5f);
    const auto trimmed = KickAssembler::voiceSettingsForSlot (state, 3, 2.0);
    REQUIRE_THAT (trimmed.trimEndSeconds, WithinAbs (1.0, 1.0e-6));
}

TEST_CASE ("Sample-Referenzen werden relativ zur Library aufgeloest", "[engine][assembler]")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("bfka-resolve-test");
    root.deleteRecursively();
    root.createDirectory();

    const auto relative = writeSine (root, "Main - Punches/Techno/P 1 150.wav", 100.0, 0.05, 0.5f);
    REQUIRE (relative.existsAsFile());

    SampleRef ref;
    ref.relPath = "Main - Punches/Techno/P 1 150.wav";
    REQUIRE (KickAssembler::resolveSampleFile (ref, root) == relative);

    // Unbekannter Pfad -> leere Datei.
    SampleRef missing;
    missing.relPath = "gibt/es/nicht.wav";
    REQUIRE_FALSE (KickAssembler::resolveSampleFile (missing, root).existsAsFile());

    // Absolute Referenz gewinnt, wenn der relative Pfad nicht existiert.
    SampleRef absolute;
    absolute.relPath = "gibt/es/nicht.wav";
    absolute.absPath = relative.getFullPathName();
    REQUIRE (KickAssembler::resolveSampleFile (absolute, root) == relative);

    root.deleteRecursively();
}
