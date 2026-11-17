#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

#include "core/KickState.h"
#include "core/ParameterRegistry.h"

namespace bfka
{

class PluginProcessor;

/**
 * Karte eines Kick-Slots (Ticket B-86, Runde P8).
 *
 * Zeigt und bedient alles, was zu einem der zwoelf Slots gehoert:
 * Rollenband mit Pegel-Strip, Sample-Name, LOAD/X, MUTE/SOLO/LOCK/P und die
 * Slot-Parameter aus `ParameterRegistry` (VOL, PITCH, STRETCH, TRIM, FADES).
 * Dazu eine kleine Wellenform mit Trim-Fenster und Fade-Marken.
 *
 * Die Karte ist keine zweite Wahrheit: Parameter liegen im APVTS, Rolle/Sperre/
 * KEY-TRACK und der Sample-Verweis im `extraState` des Prozessors. `refresh()`
 * liest beides; jede Aenderung geht sofort an den Prozessor (und damit an den
 * Renderer), ohne die Kette neu zu bauen.
 *
 * Threading: nur Message-Thread.
 */
class SlotCard final : public juce::Component
{
public:
    /** Rueckmeldungen an die Seite. */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /** "LOAD" gedrueckt: Datei-Dialog oeffnen. */
        virtual void slotCardLoadRequested (int slot) = 0;

        /** "X" gedrueckt: Slot leeren. */
        virtual void slotCardClearRequested (int slot) = 0;

        /** Der Slot hat sich geaendert (Statuszeile/Visualiser nachziehen). */
        virtual void slotCardChanged (int slot) = 0;
    };

    SlotCard (PluginProcessor& processor, int slot, Listener& owner);
    ~SlotCard() override;

    /** Hoehe, die `resized()` erwartet. */
    static constexpr int kPreferredHeight = 262;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Liest Rolle, Sample-Name, Sperren und Parameter aus dem Zustand. */
    void refresh (const KickState& state);

    int  slotIndex() const noexcept { return slotIndex_; }
    bool hasSample() const noexcept { return stateHasSample; }

private:
    void buildControls();
    void refreshFlags (const KickState& state);
    void refreshKnobs();
    void writeParam (const ParamDesc& desc, double normalised);
    void paintBand (juce::Graphics& g, juce::Rectangle<int> area) const;
    void paintWaveform (juce::Graphics& g, juce::Rectangle<int> area) const;
    void paintKnobCaptions (juce::Graphics& g) const;

    /** Eine Regler-Zeile mit Beschriftung, Position merkt sich die Karte. */
    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        const ParamDesc* desc = nullptr;
        juce::Rectangle<int> area;
    };

    PluginProcessor& proc;
    Listener&        cardOwner;
    int              slotIndex_ = 0;

    // --- Anzeige-Zustand (aus refresh()) ----------------------------------
    bool        stateHasSample = false;
    juce::String sampleName;
    juce::String roleText;
    juce::String detailText;      ///< z. B. "420 ms" oder "leer"
    Role         role = Role::none;
    bool         locked = false;
    bool         keyTracked = false;
    double       sampleLength = 0.0;

    /** Anzeige-Werte der Wellenform (Prozent bzw. dB), in refresh() gelesen. */
    float trimStartPercent = 0.0f;
    float trimEndPercent   = 100.0f;
    float fadeInPercent    = 0.0f;
    float fadeOutPercent   = 0.0f;
    float volumeDb         = 0.0f;

    juce::Rectangle<int> bandArea;
    juce::Rectangle<int> nameArea;
    juce::Rectangle<int> waveArea;
    juce::Rectangle<int> flagsArea;

    juce::TextButton loadButton { "LOAD" };
    juce::TextButton clearButton { "X" };
    juce::TextButton muteButton { "MUTE" };
    juce::TextButton soloButton { "SOLO" };
    juce::TextButton lockButton { "LOCK" };
    juce::TextButton keyButton  { "P" };

    std::vector<Knob> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotCard)
};

} // namespace bfka
