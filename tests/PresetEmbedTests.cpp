#include <catch2/catch_test_macros.hpp>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <memory>

#include "core/KickState.h"
#include "core/PresetIO.h"

using namespace bfka;

// Eingebettete Slot-Samples im Preset (B-95).
namespace
{
    /** Frischer Arbeitsordner je Test. */
    juce::File freshTempDir()
    {
        auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("bfka_preset_embed_test");

        directory.deleteRecursively();
        directory.createDirectory();
        return directory;
    }

    /** Schreibt einen kurzen Stereo-Sinus als 16-bit-WAV. */
    juce::File writeTestWav (const juce::File& directory)
    {
        constexpr double sampleRate = 48000.0;
        constexpr double seconds    = 0.25;
        constexpr int    numChannels = 2;

        const auto numSamples = (int) (seconds * sampleRate);

        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int n = 0; n < numSamples; ++n)
                data[n] = 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                   * 90.0 * (double) n / sampleRate);
        }

        const auto file = directory.getChildFile ("testkick.wav");
        file.deleteFile();

        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

        if (stream == nullptr)
            return {};

        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), sampleRate, (unsigned int) numChannels, 16, {}, 0));

        if (writer == nullptr)
            return {};

        stream.release();   // der Writer uebernimmt den Stream

        if (! writer->writeFromAudioSampleBuffer (buffer, 0, numSamples))
            return {};

        writer.reset();
        return file;
    }

    PresetInfo makeInfo()
    {
        PresetInfo info;
        info.name    = "Embed Kick";
        info.styleId = "rawstyle";
        return info;
    }
} // namespace

// =========================================================================
// Einbetten und Wiederherstellen
// =========================================================================
TEST_CASE ("Preset: eingebettetes Sample ueberlebt das Loeschen der Quelle", "[preset][embed]")
{
    const auto tempDir = freshTempDir();

    const auto wav = writeTestWav (tempDir);
    REQUIRE (wav.existsAsFile());

    KickState state;
    SampleRef ref;
    ref.absPath  = wav.getFullPathName();
    ref.fileName = "testkick.wav";
    state.setSample (0, ref);

    const auto presetFile = tempDir.getChildFile ("Embed.bfkapreset");

    PresetWriteOptions opt;
    opt.embedSamples = true;

    juce::String error;
    REQUIRE (PresetIO::saveKickPreset (presetFile, state, makeInfo(), opt, error));
    INFO (error);
    REQUIRE (presetFile.existsAsFile());

    // Quelle verschwindet: das Preset muss allein tragen.
    REQUIRE (wav.deleteFile());

    auto loaded = PresetIO::loadKickPreset (presetFile, error);
    REQUIRE (loaded.has_value());
    INFO (error);

    REQUIRE (loaded->state.getSample (0).has_value());
    REQUIRE (loaded->state.getSample (0)->absPath.isNotEmpty());
    REQUIRE (juce::File (loaded->state.getSample (0)->absPath).existsAsFile());
    CHECK (loaded->state.getSample (0)->fileName == "testkick.wav");
}

TEST_CASE ("Preset: ohne Option steht kein <Samples> in der Datei", "[preset][embed]")
{
    const auto tempDir = freshTempDir();

    const auto wav = writeTestWav (tempDir);
    REQUIRE (wav.existsAsFile());

    KickState state;
    SampleRef ref;
    ref.absPath  = wav.getFullPathName();
    ref.fileName = "testkick.wav";
    state.setSample (0, ref);

    const auto presetFile = tempDir.getChildFile ("NoEmbed.bfkapreset");

    PresetWriteOptions opt;
    opt.embedSamples = false;

    juce::String error;
    REQUIRE (PresetIO::saveKickPreset (presetFile, state, makeInfo(), opt, error));
    INFO (error);

    auto xml = juce::XmlDocument::parse (presetFile);
    REQUIRE (xml != nullptr);
    REQUIRE (xml->getChildByName ("Samples") == nullptr);
}

TEST_CASE ("Preset: zu grosses Embed-Limit schreibt keine Datei", "[preset][embed]")
{
    const auto tempDir = freshTempDir();

    const auto wav = writeTestWav (tempDir);
    REQUIRE (wav.existsAsFile());

    KickState state;
    SampleRef ref;
    ref.absPath  = wav.getFullPathName();
    ref.fileName = "testkick.wav";
    state.setSample (0, ref);

    const auto presetFile = tempDir.getChildFile ("TooBig.bfkapreset");

    PresetWriteOptions opt;
    opt.embedSamples     = true;
    opt.maxEmbeddedBytes = 1;

    juce::String error;
    REQUIRE_FALSE (PresetIO::saveKickPreset (presetFile, state, makeInfo(), opt, error));
    REQUIRE (error.isNotEmpty());
    REQUIRE_FALSE (presetFile.existsAsFile());
}
