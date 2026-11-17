#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "fx/modules/KickShaperModule.h"
#include "ui/ModuleStructureView.h"

namespace bfka
{

/**
 * Struktur-Editor des Kick Shapers (`shaper`, Ticket B-64 / B-92).
 *
 * Die Ansicht zeigt die Huellkurve als Amplitudenverlauf ueber die Kick-Laenge:
 * links Position 0, rechts Position 1, unten Wert 0, oben Wert 1 (neutral).
 *
 *  - **Kurve**: 129 Stuetzstellen aus `KickShaperModule::evaluateEnvelope()`,
 *    also genau die Form, die das Modul gerade auswertet.
 *  - **Knoten**: die `<Pt>`-Punkte des Zustands. Linke Taste zieht den Punkt
 *    (`x` 0...1, auf die Nachbarn begrenzt, damit die Reihenfolge sortiert
 *    bleibt; `y` 0...1). Rechtsklick zykelt den Segmenttyp `t` (0 -> 1 -> 2 ->
 *    3 -> 0), Strg+Klick loescht den Punkt (mindestens einer bleibt stehen),
 *    Doppelklick auf freie Flaeche fuegt einen Punkt an der Mausposition ein
 *    (hoechstens `KickShaperModule::maxPoints`). Der Typ steht klein am Knoten.
 *  - **Statuszeile**: Laenge, Abspielposition und Huellkurvenwert; eine duenne
 *    senkrechte Marke zeigt die Position der Huellkurven-Uhr.
 *
 * Zustand: Die lokale Punktkopie wird im Konstruktor und in `refresh()` neu aus
 * dem Modul geladen, waehrend des Ziehens aber nicht - sonst wuerde der Punkt
 * unter der Maus bei jedem Timer-Tick zurueckspringen. Jede Aenderung geht
 * erst ins Modul (`setEnvelopePoints()`), danach per
 * `writeBackModuleStates()` in den Ketten-Zustand (und damit ins Preset).
 *
 * Ohne Modul am Kettenplatz zeichnet `paintContent()` nur den Rahmen, die
 * Maus-Handler tun nichts.
 *
 * Threading: nur Message-Thread.
 */
class KickShaperView final : public ModuleStructureView
{
public:
    KickShaperView (PluginProcessor& processor, int slotIndex);

    int  preferredHeight() const override { return 200; }
    void refresh() override;

    void paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea) override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    juce::Rectangle<int> envelopeArea (juce::Rectangle<int> contentArea) const;
    juce::Rectangle<int> statusRow (juce::Rectangle<int> contentArea) const;

    /** Position 0...1 -> Pixel-X, Wert 0...1 -> Pixel-Y (und die Umkehrungen). */
    float  xForPosition (double position, juce::Rectangle<int> area) const;
    float  yForValue (double value, juce::Rectangle<int> area) const;
    double positionForX (float x, juce::Rectangle<int> area) const;
    double valueForY (float y, juce::Rectangle<int> area) const;

    /** Index des getroffenen Knotens oder -1. */
    int pointAt (juce::Point<int> position) const;

    /** Laedt die Zustandsreihenfolge und sortiert sie stabil nach `x`. */
    void readPointsFromModule();
    /** Schreibt die lokale Kopie ins Modul und danach in den Zustand. */
    void writePointsToModule();
    /** Setzt den Punkt `index` auf die Mausposition (geklemmt, ohne Umsortieren). */
    void dragPointTo (int index, juce::Point<int> position);

    std::vector<KickShaperModule::Pt> points;
    int draggedPoint = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickShaperView)
};

} // namespace bfka
