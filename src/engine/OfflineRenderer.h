#pragma once

// B-110: Offline-Renderer fuer Kick + FX-Kette.

#include <juce_audio_basics/juce_audio_basics.h>

namespace bfka
{

class KickRenderer;
class FxChainProcessor;

/** Ergebnis eines Offline-Renders (Ticket B-110). */
struct OfflineRenderResult
{
    juce::AudioBuffer<float> buffer;   ///< stereo, fertig inkl. FX
    double sampleRate = 48000.0;
    bool   ok         = false;
    juce::String error;
};

/**
 * Rendert einen Kick samt FX-Kette offline in einen Puffer (B-37/B-110).
 *
 * Reihenfolge je Block: Renderer -> Kette. Beide werden vorher mit
 * Rate/Blockgroesse vorbereitet. `lengthSeconds <= 0` bedeutet
 * `renderer.kickLengthSeconds() + fx.tailSeconds()`, mindestens 0.05 s.
 * Nach dem Rendern wird `fx.reset()` NICHT gerufen (Zustand bleibt beim
 * Aufrufer erhalten); der Renderer wird gestoppt.
 *
 * Nur Message-/Worker-Thread; allokiert.
 */
class OfflineRenderer
{
public:
    static OfflineRenderResult render (KickRenderer& renderer,
                                       FxChainProcessor& fxChain,
                                       double sampleRate = 48000.0,
                                       int blockSize = 512,
                                       double lengthSeconds = 0.0);

    /** Anzahl der Bloecke fuer eine Laenge (Diagnose/Test). */
    static int blockCountFor (double seconds, double sampleRate, int blockSize) noexcept;
};

} // namespace bfka
