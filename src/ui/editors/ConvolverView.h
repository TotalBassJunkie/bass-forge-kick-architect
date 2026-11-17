#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

#include "ui/ModuleStructureView.h"

namespace bfka
{

/**
 * Convolver-Editor (`reverb`, Ticket B-92).
 *
 *  - **IR-Picker**: Datei-Dialog (WAV/AIF/AIFF); die Auswahl geht ueber
 *    `ConvolverModule::setImpulsePath()` ins Modul und per
 *    `writeBackModuleStates()` als `ir`-Property in den Zustand.
 *  - **EQ-Kurve**: 11 Stuetzpunkte, X logarithmisch 20 Hz … 20 kHz,
 *    Y −24 … +24 dB.
 *  - **Decay-Kurve**: 11 Stuetzpunkte, Y Faktor 0,1 … 2 (logarithmisch
 *    dargestellt).
 *
 * Nach jeder Kurvenaenderung wird `writeBackModuleStates()` gerufen, sonst
 * ginge die Kurve beim naechsten `setChain()` verloren.
 */
class ConvolverView final : public ModuleStructureView
{
public:
    ConvolverView (PluginProcessor& processor, int slotIndex);

    int  preferredHeight() const override { return 244; }
    void refresh() override;

    void paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    enum class Curve { none, eq, decay };

    static constexpr int numPoints = 11;

    juce::Rectangle<int> irRow (juce::Rectangle<int> contentArea) const;
    juce::Rectangle<int> eqArea (juce::Rectangle<int> contentArea) const;
    juce::Rectangle<int> decayArea (juce::Rectangle<int> contentArea) const;

    void chooseImpulseResponse();
    void readCurvesFromModule();
    void writeCurvesToModule();

    void paintCurve (juce::Graphics& g, juce::Rectangle<int> area,
                     const std::array<float, numPoints>& values,
                     double minValue, double maxValue, bool logarithmic,
                     juce::Colour colour, const juce::String& caption);

    Curve curveAt (juce::Point<int> position) const;
    int   pointIndexForX (float x, juce::Rectangle<int> area) const;

    float valueToY (float value, juce::Rectangle<int> area, double minValue,
                   double maxValue, bool logarithmic) const;
    float yToValue (float y, juce::Rectangle<int> area, double minValue,
                    double maxValue, bool logarithmic) const;

    juce::TextButton irButton { "IR waehlen ..." };
    std::unique_ptr<juce::FileChooser> fileChooser;

    std::array<float, numPoints> eqCurve {};
    std::array<float, numPoints> decayCurve {};

    juce::String irInfo;
    Curve activeCurve = Curve::none;
    int   activePoint = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolverView)
};

} // namespace bfka
