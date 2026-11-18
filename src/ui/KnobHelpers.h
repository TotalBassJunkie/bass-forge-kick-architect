/*
 * B-87: Regler-Komfort als ein Helfer fuer alle Slider.
 *
 * Ein einziger Anhaenger pro Slider liefert Rechtsklick-Menue, Shift-Fein,
 * Ctrl-grobe Schritte und eine einheitliche Textanzeige. Damit muss kein
 * Aufrufer die Einzelteile selbst nachbauen.
 */
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bfka
{

/** Komfort-Einstellungen eines Reglers (Ticket B-87). */
struct KnobConfig
{
    double defaultValue = 0.0;
    bool   fineWithShift = true;    ///< Shift = 1/10 der Bewegung
    bool   coarseWithCtrl = true;   ///< Ctrl = 5x Schritt
    juce::String unit;              ///< z. B. "dB", "%", "Hz"
    int    decimals = 1;            ///< Nachkommastellen der Anzeige
};

/**
 * Macht aus einem beliebigen juce::Slider einen komfortablen Regler:
 * Rechtsklick-Menue (Reset / Wert eingeben / Kopieren / Einfuegen),
 * Shift-Fein, Ctrl-grobe Schritte, einheitliche Textanzeige.
 *
 * Das Objekt haengt am Slider und muss so lange leben wie dieser.
 */
class KnobHelpers final : private juce::MouseListener
{
public:
    KnobHelpers (juce::Slider& sliderToUse, KnobConfig config);
    ~KnobHelpers() override;

    const KnobConfig& config() const noexcept { return knobConfig; }

    /** Text fuer die Slider-Anzeige (Wert + Einheit, config.decimals). */
    static juce::String formatValue (double value, const KnobConfig&);
    /** Parst eine Nutzereingabe; onFailure bleibt der alte Wert. */
    static double parseValue (const juce::String& text, double fallback, const KnobConfig&);
    /** Skaliert eine Drag-Distanz: Shift = /10, Ctrl = *5. */
    static double dragScale (bool shiftDown, bool ctrlDown, const KnobConfig&);
    /** Setzt den Regler auf den Standardwert (mit Notification). */
    void resetToDefault();

    /** Zwischenablage fuer Copy/Paste (prozessweit, ein Wert). */
    static void copyValue (double value);
    static bool pasteValue (double& valueOut);
    static bool hasCopiedValue();

private:
    void mouseDown (const juce::MouseEvent&) override;
    void showMenu();

    juce::Slider& slider;
    KnobConfig    knobConfig;
    static double clipboardValue;
    static bool   clipboardValid;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobHelpers)
};

} // namespace bfka
