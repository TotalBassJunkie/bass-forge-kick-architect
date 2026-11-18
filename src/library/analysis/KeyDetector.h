// B-24: Key-Detection ueber Chroma-Profil (HPCP-artig) und Template-Matching.
// Schaetzt den Grundton eines Kicks; das Ergebnis ist ein Vorschlag fuer den
// Key-Chip, keine harte Zuweisung.
// Reiner Offline-Baustein: Aufruf aus Message- oder Worker-Thread,
// niemals aus dem Audio-Thread.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <array>

namespace bfka
{
    /** Ergebnis der Tonart-Schaetzung (Ticket B-24). */
    struct KeyDetectionResult
    {
        int    keyNote    = -1;    ///< 0..11 (C=0 ... B=11), -1 = unbekannt
        bool   minor      = false;
        double confidence = 0.0;   ///< 0..1
        bool   found      = false;
    };

    /**
     * Schaetzt die Tonart eines Kicks aus seinem Chroma-Profil (B-24).
     * Nur fuer Einzelklaenge/Kicks gedacht, nicht fuer ganze Songs.
     */
    class KeyDetector
    {
    public:
        struct Settings
        {
            double minFrequencyHz = 30.0;
            double maxFrequencyHz = 2000.0;
            double analysisSeconds = 0.5;    ///< nur der Anfang wird ausgewertet
        };

        static KeyDetectionResult detect (const juce::AudioBuffer<float>& buffer,
                                          double sampleRate,
                                          const Settings& = {});

        /** 12-dimensionales Chroma-Profil (C..B), auf Summe 1 normiert. */
        static std::array<double, 12> chromaProfile (const juce::AudioBuffer<float>& buffer,
                                                     double sampleRate,
                                                     const Settings& = {});
        /** Naechster Notenname zu einem Chroma-Index ("C", "C#", ...). */
        static juce::String noteName (int keyNote);
    };
} // namespace bfka
