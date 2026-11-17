#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "core/KickState.h"
#include "ui/SlotCard.h"

namespace bfka
{

class PluginProcessor;

/**
 * Schlanker Wertebalken (LinearBar) fuer den Transport.
 *
 * `juce::Slider::LinearBar` zeichnet nur den Balken; den Wert schreibt sonst
 * eine Textbox. Diese Variante schreibt ihn direkt in den Balken, damit BPM und
 * MASTER in einer 30 px hohen Leiste Platz haben.
 */
class ValueBar final : public juce::Slider
{
public:
    ValueBar() = default;

    void paint (juce::Graphics& g) override;
};

/**
 * Wellenform-Anzeige der KICK-Seite (Ticket B-84, Runde P8).
 *
 * Zwei Sichten wie im Referenzprodukt:
 *  - **PARTS**: eine Zeile je belegtem Slot, Farbe nach Rolle.
 *  - **SUM**: Summe der Slots mit Lautstaerke, Position, Trims und Fades —
 *    eine Naeherung aus den Anzeige-Huellen, kein gerenderter Kick.
 *
 * Dazu ein Zeitraster mit Beat-Unterteilung. Die Huellen kommen aus
 * `KickAssembler`; hier wird nur gezeichnet, nie gerechnet oder geladen.
 */
class KickVisualizer final : public juce::Component
{
public:
    enum class ViewMode { parts, sum };

    explicit KickVisualizer (PluginProcessor& processor);
    ~KickVisualizer() override = default;

    void paint (juce::Graphics& g) override;

    /** Liest Zustand, Huellen und Laengen neu (Message-Thread). */
    void refresh();

    void     setViewMode (ViewMode newMode);
    ViewMode viewMode() const noexcept { return mode; }

    /** Laenge des dargestellten Fensters in Sekunden (Kick-Laenge). */
    double windowSeconds() const noexcept { return kickLength; }

private:
    void paintParts (juce::Graphics& g, juce::Rectangle<int> area) const;
    void paintSum (juce::Graphics& g, juce::Rectangle<int> area) const;
    void paintGrid (juce::Graphics& g, juce::Rectangle<int> area) const;

    /** Summenhuelle ueber das Fenster [0, kickLength], 0..1 skaliert. */
    void buildSumCurve (std::vector<float>& out) const;

    PluginProcessor& proc;
    ViewMode mode = ViewMode::parts;

    /** Zwischengespeicherter Zustand (nicht bei jedem Repaint neu lesen). */
    KickState snapshot;
    double    kickLength = 1.0;
    double    beatLength = 0.4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickVisualizer)
};

/**
 * Die KICK-Seite (Tickets B-83/B-84/B-86, Runde P8).
 *
 * Aufbau von oben nach unten: Transportzeile (PLAY, LOOP, BPM, MASTER,
 * DEMO-KICK), Visualiser, Raster mit zwoelf Slot-Karten (scrollbar) und eine
 * Statuszeile. Die Seite liest den Zustand beim Anzeigen und im Timer; sie ist
 * keine zweite Wahrheit fuer Slots, Parameter oder BPM.
 */
class KickPage final : public juce::Component,
                       private juce::Timer,
                       private SlotCard::Listener
{
public:
    explicit KickPage (PluginProcessor& processor);
    ~KickPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Liest Slots, BPM und Status neu (nach Presetwechsel, Demo-Kick, ...). */
    void syncFromProcessor();

    /** Statuszeile setzen (`isError` faerbt rot). */
    void setStatus (const juce::String& text, bool isError = false);

    /** Anzahl der gebauten Slot-Karten (Test/Diagnose). */
    int getNumSlotCards() const noexcept { return (int) cards.size(); }

    /** Karte eines Slots (Diagnose/Test); nullptr bei ungueltigem Index. */
    SlotCard* getSlotCard (int slot) const;

    /** Visualiser der Seite (Diagnose/Test). */
    KickVisualizer& visualizer() noexcept { return *vis; }

    /** Loopt die Wiedergabe (Demo-Kick alle 1,5 s). */
    void setLoopEnabled (bool shouldLoop);
    bool isLoopEnabled() const noexcept { return loopEnabled; }

    /** Taktzeit des Timers (Hz); Tests koennen sie erhoehen. */
    void setRefreshRateHz (int hz);

private:
    void timerCallback() override;

    // --- SlotCard::Listener ------------------------------------------------
    void slotCardLoadRequested (int slot) override;
    void slotCardClearRequested (int slot) override;
    void slotCardChanged (int slot) override;

    void buildCards();
    void layoutCards();
    void chooseSampleForSlot (int slot);
    void toggleLoop();
    void updateToolbarLook();

    juce::Rectangle<int> transportArea;
    juce::Rectangle<int> visualizerArea;
    juce::Rectangle<int> statusArea;

    PluginProcessor& proc;

    std::unique_ptr<KickVisualizer> vis;

    juce::Viewport  viewport;
    juce::Component cardHolder;
    std::vector<std::unique_ptr<SlotCard>> cards;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton loopButton { "LOOP" };
    juce::TextButton demoButton { "DEMO-KICK" };
    juce::TextButton viewButton { "VIEW: PARTS" };

    ValueBar bpmSlider;
    ValueBar masterSlider;    juce::Label  bpmLabel;
    juce::Label  masterLabel;

    juce::String statusText;
    bool         statusIsError = false;

    bool   loopEnabled = false;
    double lastTriggerMs = 0.0;
    int    refreshRateHz = 8;

    /** Datei-Dialog (nur einer gleichzeitig; JUCE-Vertrag). */
    std::unique_ptr<juce::FileChooser> chooser;
    int                                pendingLoadSlot = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickPage)
};

} // namespace bfka
