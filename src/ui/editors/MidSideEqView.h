#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "ui/ModuleStructureView.h"

namespace bfka
{

/**
 * Struktur-Editor des Mid/Side-EQ (`mseq`, Ticket B-92).
 *
 * Die Bandliste liegt **nicht** in den Skalarparametern, sondern als
 * `<Band kind="..." f="..." q="..." g="..." dyn="..." ch="..."/>` im `<Module>`-Baum von
 * `MidSideEqModule::getState()`. Der Editor haelt eine eigene Kopie (`Cache`)
 * und schreibt nach jeder Aenderung den kompletten Baum zurueck:
 * `MidSideEqModule::setState()` und danach
 * `PluginProcessor::writeBackModuleStates()`.
 *
 *  - Frequenz-/Gain-Plot: X logarithmisch 20 Hz ... 20 kHz, Y -18 ... +18 dB.
 *  - Linke Taste auf einem Knoten: `f` (X) und `g` (Y) ziehen.
 *  - Mausrad auf einem Knoten: `q` (Faktor 1,15 pro Raste, 0,1 ... 10).
 *  - Rechtsklick auf einem Knoten: `kind` zyklisch (Bell ... Notch).
 *  - Doppelklick auf freier Flaeche: neues Bell-Band (`ch` 0 in der unteren,
 *    1 in der oberen Plot-Haelfte), hoechstens `maxBands` Baender.
 *  - Strg+Klick oder Doppelklick auf einem Knoten: Band loeschen.
 *  - Zeile darunter: WIDTH-Slider und die vier Schalter `msolo`, `ssolo`,
 *    `mmute`, `smute` (reine Skalarparameter).
 *
 * `dyn` wird unveraendert mitgeschrieben, aber hier nicht bearbeitet.
 *
 * Threading: ausschliesslich Message-Thread (Maus und `refresh()` der FX-Seite).
 * Ist der Kettenplatz leer oder nicht vom Typ `MidSideEqModule`, zeichnet
 * `paintContent()` nur den Rahmen der Basisklasse und die Maus tut nichts.
 */
class MidSideEqView final : public ModuleStructureView
{
public:
    /** Hoechstzahl der Nutzerbaender (wie `MidSideEqModule::maxBands`). */
    static constexpr int maxBands = 12;

    MidSideEqView (PluginProcessor& processor, int slotIndex);
    ~MidSideEqView() override = default;

    int  preferredHeight() const override { return 196; }
    void refresh() override;

    /** Anzahl der aktuell angezeigten Bandknoten (Diagnose/Test). */
    int nodeCount() const noexcept { return (int) nodes.size(); }

    void paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;

private:
    /** Ein Bandknoten in Anzeige-Einheiten (Kopie des Struktur-Zustands). */
    struct Node
    {
        int    kind = 0;        ///< 0 Bell, 1 L-Shelf, 2 H-Shelf, 3 L-Cut, 4 H-Cut, 5 Notch
        double freq = 1000.0;   ///< `f` in Hz
        double q    = 0.9;      ///< `q`
        double gain = 0.0;      ///< `g` in dB
        double dyn  = 0.0;      ///< `dyn` in Prozent (nur durchgereicht)
        int    ch   = 0;        ///< 0 = Mid, 1 = Side
    };

    /** Ist an diesem Kettenplatz ein `MidSideEqModule` gebaut? */
    bool hasModule() const;

    juce::Rectangle<int> graphArea (juce::Rectangle<int> contentArea) const;
    juce::Rectangle<int> rowArea (juce::Rectangle<int> contentArea) const;

    /** Breite der Beschriftung "WIDTH" (0, wenn die Karte zu schmal ist). */
    static int widthCaptionWidth (int rowWidth) noexcept;

    void paintGraph (juce::Graphics& g, juce::Rectangle<int> area) const;

    /** Index des Knotens unter `position` (-1 = keiner). */
    int  nodeAt (juce::Point<int> position) const;

    void readBandsFromModule();
    void writeBandsToModule();
    void removeNode (int index);
    void refreshControls();

    std::vector<Node> nodes;

    juce::Slider     widthSlider;
    juce::TextButton midSoloButton  { "MID SOLO" };
    juce::TextButton sideSoloButton { "SIDE SOLO" };
    juce::TextButton midMuteButton  { "MID MUTE" };
    juce::TextButton sideMuteButton { "SIDE MUTE" };

    int  activeNode = -1;
    int  dragNode   = -1;
    bool dragging   = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidSideEqView)
};

} // namespace bfka
