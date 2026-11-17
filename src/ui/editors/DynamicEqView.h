#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/ModuleStructureView.h"

namespace bfka
{

/**
 * Dynamic-EQ-Editor (`dynEq`, Ticket B-92).
 *
 * Vier Bandknoten im Frequenz-/Gain-Feld:
 *  - X = `bN_freq` (logarithmisch 10 Hz … 20 kHz),
 *  - Y = `bN_gain` (±18 dB),
 *  - Mausrad ueber einem Knoten = `bN_q`,
 *  - Rechtsklick auf einen Knoten = `bN_type` (Bell / Low-Shelf / High-Shelf).
 *
 * Links und rechts liegen die ziehbaren HP-/LP-Kanten (`hpFreq`/`lpFreq`).
 * Unter jedem Knoten zeigt ein Balken den dynamischen Regelweg
 * (`DynamicEqModule::bandDynamicGainDb`).
 *
 * Der DSP des Nachbaus ist **seriell** statt parallel (Nova-Abweichung N1,
 * docs/specs/fx-dynamic-eq.md); der Footer weist darauf hin.
 */
class DynamicEqView final : public ModuleStructureView
{
public:
    DynamicEqView (PluginProcessor& processor, int slotIndex);

    int  preferredHeight() const override { return 216; }
    void refresh() override;

    void paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea) override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;

private:
    static constexpr int numBands = 4;

    enum class Drag { none, band, highPass, lowPass };

    juce::Rectangle<int> graphArea (juce::Rectangle<int> contentArea) const;

    float xForFrequency (double hz, juce::Rectangle<int> area) const;
    double frequencyForX (float x, juce::Rectangle<int> area) const;
    float yForGain (double gainDb, juce::Rectangle<int> area) const;
    double gainForY (float y, juce::Rectangle<int> area) const;

    int bandAt (juce::Point<int> position) const;

    static juce::String paramId (int band, const char* suffix);

    struct BandState
    {
        double freqHz = 1000.0;
        double gainDb = 0.0;
        double q      = 0.9;
        int    type   = 1;
        int    active = 0;
        float  grDb   = 0.0f;
    };

    BandState bands[numBands];
    double    highPassHz = 20.0;
    double    lowPassHz  = 20000.0;

    Drag dragMode = Drag::none;
    int  dragBand = -1;
    int  selectedBand = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicEqView)
};

} // namespace bfka
