#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/ModuleStructureView.h"

namespace bfka
{

/**
 * Crossover-Ansicht des Multiband-Kompressors (`ott`, Ticket B-92).
 *
 * Zwei ziehbare Grenzen teilen die logarithmische Frequenzachse (20 Hz …
 * 12 kHz) in Low / Mid / High; die linke Grenze ist `x1` (20…1000 Hz), die
 * rechte `x2` (500…12000 Hz). Ziehen schreibt sofort in das Modul
 * (`setModuleParam`), die Latenz aendert sich dabei nicht.
 *
 * Rechts der Grafik zeigen drei Balken den Regelweg je Band
 * (`MultibandModule::bandGainDb`), damit man sieht, was das Modul gerade tut.
 */
class MultibandView final : public ModuleStructureView
{
public:
    MultibandView (PluginProcessor& processor, int slotIndex);

    int  preferredHeight() const override { return 132; }
    void refresh() override;

    void paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea) override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;

private:
    enum class Handle { none, lowXOver, highXOver };

    juce::Rectangle<int> graphArea (juce::Rectangle<int> contentArea) const;
    juce::Rectangle<int> meterArea (juce::Rectangle<int> contentArea) const;

    Handle handleAt (juce::Point<int> position) const;
    float  xForFrequency (float hz, juce::Rectangle<int> area) const;
    float  frequencyForX (float x, juce::Rectangle<int> area) const;

    Handle  activeHandle = Handle::none;
    double  lowXOverHz  = 120.0;
    double  highXOverHz = 2500.0;
    float   bandGainDb[3] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandView)
};

} // namespace bfka
