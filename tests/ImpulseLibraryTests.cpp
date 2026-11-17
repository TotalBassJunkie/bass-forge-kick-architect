#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <memory>

#include "library/ImpulseLibrary.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// IR-Library des Convolver-Editors (Ticket B-152).
namespace
{
    juce::File testDirectory()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("bfka_ir_lib_test");

        dir.deleteRecursively();
        dir.createDirectory();

        return dir;
    }

    /** Schreibt eine kurze WAV-Datei (1 kHz Sinus) und liefert den Pfad. */
    juce::File writeTestWav (const juce::File& directory, const juce::String& name,
                             double seconds)
    {
        const auto sampleRate = 48000.0;
        const auto numSamples = (int) (seconds * sampleRate);

        juce::AudioBuffer<float> buffer (1, juce::jmax (1, numSamples));

        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, 0.8f * (float) std::sin (juce::MathConstants<double>::pi
                                                             * 2.0 * 1000.0
                                                             * (double) i / sampleRate));

        const auto file = directory.getChildFile (name);
        file.deleteFile();

        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        REQUIRE (stream != nullptr);

        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), sampleRate, 1, 16, {}, 0));

        REQUIRE (writer != nullptr);
        stream.release();   // der Writer uebernimmt den Stream

        REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, numSamples));
        writer.reset();

        return file;
    }
} // namespace

TEST_CASE ("IR-Library: zwei WAVs werden gefunden und vermessen", "[library][ir]")
{
    const auto dir = testDirectory();

    writeTestWav (dir, "a.wav", 0.5);
    writeTestWav (dir, "b.wav", 1.0);

    ImpulseLibrary lib;
    lib.setUserDirectory (dir);
    lib.setFactoryDirectory ({});

    REQUIRE (lib.scan() == 2);
    REQUIRE (lib.entries().size() == 2u);

    // Alphabetisch: a.wav (0,5 s) vor b.wav (1,0 s).
    REQUIRE (lib.entries()[0].name == "a");
    REQUIRE (lib.entries()[1].name == "b");

    for (const auto& entry : lib.entries())
    {
        REQUIRE (entry.name.isNotEmpty());
        REQUIRE (entry.seconds > 0.0);
        REQUIRE_FALSE (entry.factory);
    }

    REQUIRE_THAT (lib.entries()[0].seconds, WithinAbs (0.5, 0.02));
    REQUIRE_THAT (lib.entries()[1].seconds, WithinAbs (1.0, 0.02));
}

TEST_CASE ("IR-Library: fehlender Ordner liefert null Eintraege", "[library][ir]")
{
    const auto dir = testDirectory();

    ImpulseLibrary empty;
    empty.setUserDirectory (dir.getChildFile ("gibtsnicht"));
    empty.setFactoryDirectory ({});

    REQUIRE (empty.scan() == 0);
    REQUIRE (empty.entries().empty());
}

TEST_CASE ("IR-Library: readLengthSeconds liest die Laenge", "[library][ir]")
{
    const auto dir = testDirectory();
    const auto file = writeTestWav (dir, "a.wav", 0.5);

    double seconds = 0.0;

    REQUIRE (ImpulseLibrary::readLengthSeconds (file, seconds));
    REQUIRE_THAT (seconds, WithinAbs (0.5, 0.02));
}

TEST_CASE ("IR-Library: unlesbare Datei meldet false", "[library][ir]")
{
    double seconds = -1.0;

    REQUIRE_FALSE (ImpulseLibrary::readLengthSeconds (
        juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("bfka_ir_lib_test").getChildFile ("fehlt.wav"),
        seconds));

    REQUIRE (seconds < 0.0);
}

TEST_CASE ("IR-Library: User-Eintraege stehen vor Factory-Eintraegen", "[library][ir]")
{
    const auto root = testDirectory();

    const auto userDir    = root.getChildFile ("user");
    const auto factoryDir = root.getChildFile ("factory");

    userDir.createDirectory();
    factoryDir.createDirectory();

    // Absichtlich alphabetisch "falsch" herum: User gewinnt trotzdem.
    writeTestWav (userDir, "z_user.wav", 0.5);
    writeTestWav (factoryDir, "a_factory.wav", 1.0);

    ImpulseLibrary lib;
    lib.setUserDirectory (userDir);
    lib.setFactoryDirectory (factoryDir);

    REQUIRE (lib.scan() == 2);
    REQUIRE (lib.entries().size() == 2u);

    REQUIRE (lib.entries()[0].name == "z_user");
    REQUIRE_FALSE (lib.entries()[0].factory);

    REQUIRE (lib.entries()[1].name == "a_factory");
    REQUIRE (lib.entries()[1].factory);
}
