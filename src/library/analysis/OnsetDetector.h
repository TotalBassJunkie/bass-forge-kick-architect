// B-23: Onset-/Transienten-Detektor ueber den Huellkurven-Fluss.
// Liefert den Beginn des ersten Transienten fuer Trim-Vorschlaege.
// Reiner Offline-Baustein: Aufruf aus Message- oder Worker-Thread,
// niemals aus dem Audio-Thread.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace bfka
{
    /** Ergebnis einer Onset-Suche (Ticket B-23). */
    struct OnsetResult
    {
        int    onsetSample  = 0;      ///< Index des erkannten Beginns
        double onsetSeconds = 0.0;    ///< onsetSample / sampleRate
        double confidence   = 0.0;    ///< 0..1, Staerke des Anstiegs
        bool   found        = false;  ///< false = kein Anstieg gefunden
    };

    /**
     * Findet den Beginn des ersten Transienten ueber den Huellkurven-Fluss.
     * Offline-Baustein (Message-/Worker-Thread), allokiert erlaubt.
     */
    class OnsetDetector
    {
    public:
        struct Settings
        {
            double envelopeAttackMs  = 1.0;    ///< Huellkurven-Attack
            double envelopeReleaseMs = 20.0;   ///< Huellkurven-Release
            double minFluxFactor     = 4.0;    ///< Spitze muss x-mal ueber dem Rauschen liegen
            double searchWindowMs    = 200.0;  ///< nur der Anfang wird untersucht
            double preRollMs         = 2.0;    ///< Sicherheitsabstand vor dem Onset
        };

        static OnsetResult detect (const juce::AudioBuffer<float>& buffer,
                                   double sampleRate,
                                   const Settings& = {});
    };
} // namespace bfka
