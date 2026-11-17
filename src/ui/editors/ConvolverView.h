#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

#include "library/ImpulseLibrary.h"
#include "ui/ModuleStructureView.h"

namespace bfka
{

/**
 * Convolver-Editor (`reverb`, Ticket B-92).
 *
 *  - **IR-Picker**: Datei-Dialog (WAV/AIF/AIFF); die Auswahl geht ueber
 *    `ConvolverModule::setImpulsePath()` ins Modul und per
 *    `writeBackModuleStates()` als `ir`-Property in den Zustand.
 *  - **IR-Browser** (Ticket B-152): Liste aus `ImpulseLibrary` (User-Ordner,
 *    optional Factory-Ordner), Doppelklick laedt den Eintrag wie der Dialog.
 *  - **EQ-Kurve**: 11 Stuetzpunkte, X logarithmisch 20 Hz ... 20 kHz,
 *    Y -24 ... +24 dB.
 *  - **Decay-Kurve**: 11 Stuetzpunkte, Y Faktor 0,1 ... 2 (logarithmisch
 *    dargestellt).
 *
 * Nach jeder Kurvenaenderung wird `writeBackModuleStates()` gerufen, sonst
 * ginge die Kurve beim naechsten `setChain()` verloren.
 */
class ConvolverView final : public ModuleStructureView
{
public:
    ConvolverView (PluginProcessor& processor, int slotIndex);

    int  preferredHeight() const override { return 320; }
    void refresh() override;

    void paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    enum class Curve { none, eq, decay };

    static constexpr int numPoints = 11;

    /**
     * ListBox-Adapter fuer den IR-Browser (Ticket B-152).
     * Der Doppelklick laedt den Eintrag ins Convolver-Modul.
     */
    struct IrListModel final : juce::ListBoxModel
    {
        explicit IrListModel (ConvolverView& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

        ConvolverView& owner;
    };

    juce::Rectangle<int> irRow (juce::Rectangle<int> contentArea) const;
    juce::Rectangle<int> irListArea (juce::Rectangle<int> contentArea) const;
    juce::Rectangle<int> eqArea (juce::Rectangle<int> contentArea) const;
    juce::Rectangle<int> decayArea (juce::Rectangle<int> contentArea) const;

    void chooseImpulseResponse();
    void applyImpulseEntry (int row);
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

    // IR-Browser (Ticket B-152): User- und Factory-Ordner als Liste.
    // Der Model-Zeiger muss den ListBox ueberleben -> Model zuerst deklarieren.
    ImpulseLibrary impulseLibrary;
    std::unique_ptr<IrListModel> irListModel;
    juce::ListBox irList { "IR-Browser", nullptr };
    int irListRowCount = -1;   ///< zuletzt gezaehlte Zeilen (spart Repaints im 15-Hz-Timer)

    std::array<float, numPoints> eqCurve {};
    std::array<float, numPoints> decayCurve {};

    juce::String irInfo;
    Curve activeCurve = Curve::none;
    int   activePoint = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolverView)
};

} // namespace bfka
