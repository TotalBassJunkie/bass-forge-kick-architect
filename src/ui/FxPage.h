#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "ui/FxChainModel.h"
#include "ui/ModuleCard.h"

namespace bfka
{

class PluginProcessor;

/**
 * Die FX-Seite des Editors (Tickets B-90/B-91).
 *
 * Links die Kettenliste mit `+ ADD MODULE`-Raster, rechts die Karten der
 * Module. Struktur-Aenderungen (Add/Remove/Move/Duplicate/Clear/Preset) laufen
 * immer ueber `FxChainModel` und werden mit
 * `suspendProcessing(true) … setFxChain(…) … suspendProcessing(false)` an den
 * Prozessor gereicht — Regler-Aenderungen dagegen gehen direkt und ohne
 * Rebuild an die Engine (siehe `ModuleCard`).
 *
 * Die Seite liest den Zustand beim Anzeigen und bei `syncFromProcessor()` neu
 * ein; sie ist keine zweite Wahrheit fuer die Kette.
 */
class FxPage final : public juce::Component,
                     private juce::Timer,
                     private ModuleCard::Listener
{
public:
    explicit FxPage (PluginProcessor& processor);
    ~FxPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

    /** Liest die Kette aus dem Prozessor und baut die Karten neu. */
    void syncFromProcessor();

    /** Markiert einen Kettenplatz und scrollt ihn in die Ansicht. */
    void selectSlot (int slot);

private:
    void timerCallback() override;

    // --- ModuleCard::Listener ---------------------------------------------
    void moduleCardMoveRequested (int slot, int delta) override;
    void moduleCardDuplicateRequested (int slot) override;
    void moduleCardRemoveRequested (int slot) override;

    void rebuildCards();
    void layoutCards();
    void applyChainToProcessor();

    void addModule (FxType type);
    void clearChain();
    void toggleBypassAll();

    // --- Presets und A/B (B-93/B-94) --------------------------------------
    void chooseSavePreset();
    void chooseLoadPreset();
    void chooseSaveModulePreset();
    void chooseLoadModulePreset();
    void recallOrStoreComparison (int index);
    void toggleLock();
    void applyDefaultChain();
    void setStatus (const juce::String& text, bool isError = false);
    void updateToolbarLook();

    juce::Rectangle<int> chainListArea() const;
    void paintSidebar (juce::Graphics& g);
    void resizedSidebar();

    PluginProcessor& proc;
    FxChainModel     model;

    juce::Viewport  viewport;
    juce::Component cardHolder;
    std::vector<std::unique_ptr<ModuleCard>> cards;

    juce::TextButton bypassButton { "BYPASS ALL" };
    juce::TextButton clearButton  { "CLEAR" };
    juce::TextButton defaultButton { "DEFAULT" };
    juce::TextButton lockButton   { "LOCK" };
    juce::TextButton saveButton   { "SAVE" };
    juce::TextButton loadButton   { "LOAD" };
    juce::TextButton slotAButton  { "A" };
    juce::TextButton slotBButton  { "B" };
    juce::TextButton demoButton   { "FX-DEMO" };
    juce::TextButton loopButton   { "LOOP" };

    juce::TextButton saveModuleButton { "SAVE MODULE" };
    juce::TextButton loadModuleButton { "LOAD MODULE" };

    juce::OwnedArray<juce::TextButton> addButtons;

    /** Datei-Dialog (nur einer gleichzeitig; JUCE-Vertrag). */
    std::unique_ptr<juce::FileChooser> chooser;

    /** Rueckmeldung nach Speichern/Laden (Seitenleiste unten). */
    juce::String statusText;
    bool         statusIsError = false;

    /** Loop-Wiedergabe (Demo-Kick alle 1,5 s). */
    bool   loopEnabled = false;
    double lastTriggerMs = 0.0;

    juce::Rectangle<int> sidebarBounds;
    juce::Rectangle<int> listArea;
    juce::Rectangle<int> addArea;
    juce::Rectangle<int> addLabelArea;
    juce::Rectangle<int> modulePresetArea;
    juce::Rectangle<int> moduleLabelArea;
    juce::Rectangle<int> statusArea;

    int selectedSlot = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxPage)
};

} // namespace bfka
