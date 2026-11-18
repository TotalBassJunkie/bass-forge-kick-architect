// B-114: WAV-Ausgabe (16/24/32-bit, 32 = IEEE float).
#include "core/WavWriter.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace bfka
{

namespace
{
    /** Bittiefe, auf die ungueltige Angaben geklemmt werden. */
    constexpr int kFallbackBitDepth = 24;

    bool isSupportedBitDepth (int bitDepth)
    {
        return bitDepth == 16 || bitDepth == 24 || bitDepth == 32;
    }
}

bool WavWriter::write (const juce::File& file,
                       const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       int bitDepth,
                       juce::String& error)
{
    error.clear();

    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
    {
        error = "Puffer ist leer";
        return false;
    }

    // Unbekannte Bittiefen werden still auf den Standard geklemmt.
    if (! isSupportedBitDepth (bitDepth))
        bitDepth = kFallbackBitDepth;

    juce::WavAudioFormat format;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr)
    {
        error = "Datei nicht schreibbar";
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream.get(),
                                                                           sampleRate,
                                                                           (unsigned int) buffer.getNumChannels(),
                                                                           bitDepth,
                                                                           {},
                                                                           0));

    if (writer == nullptr)
    {
        error = "WAV-Writer konnte nicht angelegt werden";
        return false;
    }

    // Der Writer besitzt den Stream ab jetzt selbst.
    stream.release();

    if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
    {
        error = "Schreiben der Audiodaten fehlgeschlagen";
        return false;
    }

    return true;
}

} // namespace bfka
