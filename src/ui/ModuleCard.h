#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "fx/FxModule.h"

namespace bfka
{

class PluginProcessor;
class ModuleStructureView;

/**
 * Eine Karte des FX-Racks (Tickets B-90/B-91).
 *
 * Aufbau von oben nach unten:
 *  - Kopf: Modulname + Typ-String, Power, ▲/▼, Duplicate, X,
 *  - MIX und OUT GAIN (gehen an die Ketten-Engine **und** in den Zustand),
 *  - optionaler Struktur-Editor (`ModuleStructureView`, Step 3),
 *  - Wellenform vor/nach dem Modul (`FxChainProcessor::displayFor`),
 *  - Parameter-Raster: ein Rotary je `FxParamDesc` in Anzeige-Einheiten.
 *
 * Der Kartenslot ist nur so lange gueltig, wie die Seite die Karten nicht neu
 * baut; bei jeder Struktur-Aenderung (Add/Remove/Move/Duplicate/Preset) baut
 * `FxPage` alle Karten neu.
 */
class ModuleCard final : public juce::Component
{
public:
    /** Rueckrufe der Karte an die Seite (Struktur-Aenderungen). */
    struct Listener
    {
        virtual ~Listener() = default;

        virtual void moduleCardMoveRequested (int slot, int delta) = 0;
        virtual void moduleCardDuplicateRequested (int slot) = 0;
        virtual void moduleCardRemoveRequested (int slot) = 0;
    };

    ModuleCard (PluginProcessor& processor, int slot, Listener& owner);
    ~ModuleCard() override;

    /** Vom Timer der Seite (ca. 15 Hz): Anzeige, Werte, Struktur. */
    void refresh();

    int  preferredHeight() const;
    int  slot() const noexcept { return moduleSlot; }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void buildControls();
    void updatePowerLook();
    void paintDisplay (juce::Graphics& g, juce::Rectangle<int> area);
    void paintKnobLabels (juce::Graphics& g);

    /** Sucht den Parameterindex zur Kennung; -1 wenn unbekannt. */
    int paramIndexFor (const juce::String& id) const;

    PluginProcessor& proc;
    Listener&        cardOwner;
    int              moduleSlot = 0;

    juce::String cardTitle;
    juce::String typeString;
    std::vector<FxParamDesc> descriptors;

    std::unique_ptr<ModuleStructureView> structureView;

    juce::TextButton powerButton;
    juce::TextButton upButton;
    juce::TextButton downButton;
    juce::TextButton duplicateButton;
    juce::TextButton removeButton;

    juce::Slider mixSlider;
    juce::Slider outSlider;
    juce::Label  mixLabel;
    juce::Label  outLabel;

    std::vector<std::unique_ptr<juce::Slider>> knobs;

    /** Layout der Parameter-Zellen aus `resized()` (fuer die Namen im Kopf). */
    std::vector<juce::Rectangle<int>> knobAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleCard)
};

} // namespace bfka
