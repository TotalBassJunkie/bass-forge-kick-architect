// B-114: WAV-Ausgabe (16/24/32-bit, 32 = IEEE float).
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace bfka
{

/** WAV-Ausgabe (Ticket B-114). */
class WavWriter
{
public:
    /** Schreibt 16/24/32-bit WAV; 32 = IEEE float. false + error bei Fehlern. */
    static bool write (const juce::File& file,
                       const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       int bitDepth,
                       juce::String& error);
};

} // namespace bfka
