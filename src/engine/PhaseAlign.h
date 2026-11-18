#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// B-56: PHASE ALIGN - Korrelation im Low-End, Nudge und Polaritaets-Flip.
//
// Richtet zwei Signale im Tieftonbereich phasenkohaerent aus (typisch: Kick
// gegen Bass). Der Baustein laeuft offline (Vorrendering, Export, ALIGN) und
// darf allokieren; im Audio-Thread hat er nichts zu suchen.
//
// Vorzeichen des Nudge: er beschreibt die anzuwendende Korrektur, nicht die
// Lage des Ziels. Ist das Ziel spaeter als die Referenz, wird der Nudge
// negativ (die ersten Samples werden abgeschnitten). Positiv heisst: das Ziel
// beginnt spaeter, vorne wird mit Stille aufgefuellt.

namespace bfka
{

/** Ergebnis eines PHASE-ALIGN-Laufs (Ticket B-56). */
struct PhaseAlignResult
{
    int    nudgeSamples     = 0;      ///< Verschiebung des Ziels in Samples (>0 = spaeter)
    bool   polarityFlip     = false;  ///< Ziel invertieren
    double correlationBefore = 0.0;   ///< -1..1, im Low-End
    double correlationAfter  = 0.0;   ///< -1..1 nach der Korrektur
    bool   improved         = false;  ///< true, wenn |after| > |before|
};

/**
 * Richtet zwei Signale im Low-End phasenkohaerent aus (B-56).
 *
 * Sucht im Fenster +-maxNudgeSamples die Verschiebung mit der groessten
 * Betragskorrelation und prueft zusaetzlich, ob ein Polaritaets-Flip das
 * Ergebnis verbessert. Beide Puffer muessen dieselbe Rate haben.
 */
class PhaseAlign
{
public:
    static PhaseAlignResult align (const juce::AudioBuffer<float>& reference,
                                   const juce::AudioBuffer<float>& target,
                                   int    maxNudgeSamples = 256,
                                   double lowEndCutoffHz = 200.0,
                                   double sampleRate      = 48000.0);

    /** Verschiebt `target` um `nudgeSamples` (positiv = spaeter) und optional invertiert. */
    static juce::AudioBuffer<float> apply (const juce::AudioBuffer<float>& target,
                                           int nudgeSamples,
                                           bool polarityFlip);
};

} // namespace bfka
