#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/ModuleStructureView.h"

namespace bfka
{

/**
 * Finalizer-Editor (`finalizer`, Ticket B-92) - **reine Anzeige**.
 *
 * Es gibt keine Struktur im Zustand und keine Maus-Interaktion: der Editor
 * liest die Parameter und die Diagnosewerte des Moduls und zeichnet daraus
 * drei Bereiche.
 *
 *  1. **SUB SHELF** - Low-Shelf-Verlauf, Frequenz 20 Hz ... 1 kHz
 *     logarithmisch, Pegel -3 ... +12 dB, Kurve
 *     `g(f) = boost / (1 + (f/freq)^2)`. Bei `shelfOn == 0` flach und
 *     gestrichelt.
 *  2. **Mono-Bass** - senkrechte Linie bei `monoF` Hz mit Beschriftung
 *     "MONO <hz> Hz", bei `monoOn == 0` gedimmt.
 *  3. **Limiter/Clip** - je ein Balken fuer die Aufwaerts-Anhebung
 *     (`currentUpwardGainDb()`) und die Limiter-Reduktion
 *     (`1 - currentLimiterGain()` linear, angezeigt als -20*log10(gain) dB),
 *     dazu ein Clip-Kaestchen und die Zeilen LAT/CLIP/UP.
 *
 * Die Werte werden beim Zeichnen gelesen; `refresh()` loest nur ein
 * Neuzeichnen aus (die FX-Seite ruft es ca. 15 Hz und repaintet selbst).
 * Alle Anzeige-Strings sind ASCII (MSVC liest die Quellen ohne `/utf-8`).
 */
class FinalizerView final : public ModuleStructureView
{
public:
    FinalizerView (PluginProcessor& processor, int slotIndex);

    int  preferredHeight() const override { return 190; }
    void refresh() override;

    void paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea) override;

private:
    /** Inhalt unter der Ueberschrift, ohne die Raender der Karte. */
    juce::Rectangle<int> bodyArea (juce::Rectangle<int> contentArea) const;
    /** Linker Bereich: Sub-Shelf-Kurve (ca. 55 % Breite). */
    juce::Rectangle<int> plotArea (juce::Rectangle<int> contentArea) const;
    /** Rechter Bereich: Limiter/Clip (ca. 45 % Breite). */
    juce::Rectangle<int> limiterArea (juce::Rectangle<int> contentArea) const;

    /** Frequenz (Hz) -> X innerhalb von `area` (logarithmisch). */
    float frequencyToX (double hz, juce::Rectangle<int> area) const;
    /** Pegel (dB) -> Y innerhalb von `area`. */
    float levelToY (float db, juce::Rectangle<int> area) const;

    void paintShelf (juce::Graphics& g, juce::Rectangle<int> area) const;
    void paintLimiter (juce::Graphics& g, juce::Rectangle<int> area) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinalizerView)
};

} // namespace bfka
