// B-112/B-114: Tests fuer Export-Dateinamen, Kollisionsschutz und WAV-Ausgabe.
#include <catch2/catch_test_macros.hpp>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <memory>

#include "core/ExportNaming.h"
#include "core/WavWriter.h"

using namespace bfka;

namespace
{
    /** Frisches Arbeitsverzeichnis je Test (wird am Ende geloescht). */
    struct TempDirectory
    {
        juce::File directory;

        TempDirectory()
        {
            directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("bfka-export-tests")
                            .getChildFile (juce::String (juce::Random::getSystemRandom().nextInt64()));

            directory.createDirectory();
        }

        ~TempDirectory()
        {
            if (directory.isDirectory())
                directory.deleteRecursively();
        }

        juce::File file (const juce::String& name) const
        {
            return directory.getChildFile (name);
        }
    };

    /** Sinus-Puffer fuer die WAV-Tests. */
    juce::AudioBuffer<float> makeSine (int numChannels, double sampleRate, double seconds)
    {
        const int numSamples = (int) std::lround (sampleRate * seconds);

        juce::AudioBuffer<float> buffer (numChannels, numSamples);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* samples = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
                samples[i] = 0.5f * (float) std::sin (2.0 * 3.141592653589793 * 100.0 * (double) i / sampleRate);
        }

        return buffer;
    }

    /** Liest die Kopfdaten einer geschriebenen Datei. */
    std::unique_ptr<juce::AudioFormatReader> makeReader (const juce::File& file)
    {
        juce::AudioFormatManager manager;
        manager.registerBasicFormats();

        return std::unique_ptr<juce::AudioFormatReader> (manager.createReaderFor (file));
    }
} // namespace

TEST_CASE ("Export: Dateinamen werden bereinigt", "[core][export]")
{
    REQUIRE (ExportNaming::sanitise ("a<b>c:d\"e/f\\g|h?i*j") == "a_b_c_d_e_f_g_h_i_j");
    REQUIRE (ExportNaming::sanitise ("  . ") == "export");
}

TEST_CASE ("Export: Basisname aus Preset und Suffix", "[core][export]")
{
    REQUIRE (ExportNaming::defaultBaseName ("Rumble 150", "kick") == "Rumble 150_kick");
    REQUIRE (ExportNaming::defaultBaseName ("", "stem") == "BassForge_stem");
}

TEST_CASE ("Export: Kollisionen bekommen eine Nummer", "[core][export]")
{
    TempDirectory temp;

    auto a = ExportNaming::uniqueFile (temp.directory, "kick", "wav");
    REQUIRE (a.getFileName() == "kick.wav");

    REQUIRE (a.create());

    auto b = ExportNaming::uniqueFile (temp.directory, "kick", "wav");
    REQUIRE (b.getFileName() == "kick_2.wav");
}

TEST_CASE ("WAV: 16 bit Rundweg", "[core][export]")
{
    TempDirectory temp;

    const auto buffer = makeSine (2, 48000.0, 0.25);
    const auto file   = temp.file ("rundweg16.wav");

    juce::String error;
    REQUIRE (WavWriter::write (file, buffer, 48000.0, 16, error));
    INFO (error);

    auto reader = makeReader (file);
    REQUIRE (reader != nullptr);

    REQUIRE (reader->numChannels == 2);
    REQUIRE (reader->bitsPerSample == 16);
    REQUIRE (reader->lengthInSamples == buffer.getNumSamples());
}

TEST_CASE ("WAV: 24 und 32 bit schreiben", "[core][export]")
{
    TempDirectory temp;

    const auto buffer = makeSine (2, 48000.0, 0.1);
    juce::String error;

    for (const int bitDepth : { 24, 32 })
    {
        const auto file = temp.file ("tiefe" + juce::String (bitDepth) + ".wav");

        REQUIRE (WavWriter::write (file, buffer, 48000.0, bitDepth, error));
        INFO (error);

        auto reader = makeReader (file);
        REQUIRE (reader != nullptr);

        REQUIRE (reader->bitsPerSample == bitDepth);
        REQUIRE (reader->lengthInSamples == buffer.getNumSamples());
    }
}

TEST_CASE ("WAV: ungueltige Bittiefe wird geklemmt", "[core][export]")
{
    TempDirectory temp;

    const auto buffer = makeSine (1, 48000.0, 0.1);
    const auto file   = temp.file ("geklemmt.wav");

    juce::String error;
    REQUIRE (WavWriter::write (file, buffer, 48000.0, 7, error));
    INFO (error);

    auto reader = makeReader (file);
    REQUIRE (reader != nullptr);
    REQUIRE (reader->bitsPerSample == 24);
}

TEST_CASE ("WAV: leerer Puffer ist ein Fehler", "[core][export]")
{
    TempDirectory temp;

    juce::AudioBuffer<float> leer;
    const auto file = temp.file ("leer.wav");

    juce::String error;
    REQUIRE_FALSE (WavWriter::write (file, leer, 48000.0, 16, error));
    REQUIRE (error.isNotEmpty());
}
