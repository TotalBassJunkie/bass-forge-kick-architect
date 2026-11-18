/*
 * B-87: Regler-Komfort als ein Helfer fuer alle Slider.
 *
 * Umsetzung des Vertrags: Anzeige, Parsen, Drag-Skalierung, Rechtsklick-Menue
 * und eine prozessweite Ein-Wert-Zwischenablage.
 */
#include "ui/KnobHelpers.h"

#include <cmath>
#include <utility>

namespace bfka
{

namespace
{
    /**
     * Sucht das Textfeld (Label) eines Sliders.
     *
     * JUCE 8.0.7 hat keinen oeffentlichen Zugriff auf das interne Label
     * (kein Slider::getTextBox()); der Look&Feel haengt es als Kind an den
     * Slider. Deshalb hier der Umweg ueber die Kindliste.
     */
    juce::Label* findSliderTextBox (juce::Component* parent)
    {
        if (parent == nullptr)
            return nullptr;

        for (auto* child : parent->getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
                return label;

        return nullptr;
    }
} // namespace

double KnobHelpers::clipboardValue = 0.0;
bool   KnobHelpers::clipboardValid = false;

// ---------------------------------------------------------------------------
// Anzeige und Eingabe
// ---------------------------------------------------------------------------

juce::String KnobHelpers::formatValue (double value, const KnobConfig& config)
{
    // Vor dem Formatieren kaufmaennisch runden: damit liegt z. B. -3.25 schon
    // als -3.3 fest, unabhaengig davon, wie die Laufzeit exakte Halbwerte
    // behandelt (MSVC und glibc unterscheiden sich hier).
    const auto decimals = juce::jmax (0, config.decimals);
    const auto scale    = std::pow (10.0, (double) decimals);
    const auto rounded  = std::round (value * scale) / scale;

    auto text = juce::String (rounded, decimals);

    if (config.unit.isNotEmpty())
        text << " " << config.unit;

    return text;
}

double KnobHelpers::parseValue (const juce::String& text, double fallback, const KnobConfig& config)
{
    auto cleaned = text.trim();

    // Deutsche Schreibweise: Komma als Dezimaltrenner.
    cleaned = cleaned.replaceCharacter (',', '.');

    // Eine mitgeschriebene Einheit ("-6 dB") wird abgeschnitten.
    if (config.unit.isNotEmpty())
        cleaned = cleaned.replace (config.unit, "", true);

    cleaned = cleaned.removeCharacters (" \t");
    cleaned = cleaned.trim();

    // Ohne Ziffer gibt es nichts zu lesen ("abc", "").
    if (! cleaned.containsAnyOf ("0123456789"))
        return fallback;

    // Nur Zeichen, die getDoubleValue() versteht.
    if (! cleaned.containsOnly ("0123456789+-.eE"))
        return fallback;

    return cleaned.getDoubleValue();
}

double KnobHelpers::dragScale (bool shiftDown, bool ctrlDown, const KnobConfig& config)
{
    // Shift gewinnt, wenn beides gedrueckt ist.
    if (shiftDown && config.fineWithShift)
        return 0.1;

    if (ctrlDown && config.coarseWithCtrl)
        return 5.0;

    return 1.0;
}

// ---------------------------------------------------------------------------
// Zwischenablage
// ---------------------------------------------------------------------------

void KnobHelpers::copyValue (double value)
{
    clipboardValue = value;
    clipboardValid = true;
}

bool KnobHelpers::pasteValue (double& valueOut)
{
    if (! clipboardValid)
        return false;

    valueOut = clipboardValue;
    return true;
}

bool KnobHelpers::hasCopiedValue()
{
    return clipboardValid;
}

// ---------------------------------------------------------------------------
// Anhaenger am Slider
// ---------------------------------------------------------------------------

KnobHelpers::KnobHelpers (juce::Slider& sliderToUse, KnobConfig config)
    : slider (sliderToUse),
      knobConfig (std::move (config))
{
    slider.addMouseListener (this, true);
    slider.setPopupMenuEnabled (false);

    slider.textFromValueFunction = [this] (double v) { return formatValue (v, knobConfig); };
    slider.valueFromTextFunction = [this] (const juce::String& t)
    {
        return parseValue (t, slider.getValue(), knobConfig);
    };

    slider.updateText();
}

KnobHelpers::~KnobHelpers()
{
    slider.removeMouseListener (this);

    // Beide Lambdas haengen an `this`. Selbst wenn laut Vertrag der Helfer so
    // lange lebt wie der Slider: hier loesen, damit ein Slider, der den Helfer
    // ueberlebt, nicht in eine tote Lambda laeuft.
    slider.textFromValueFunction = nullptr;
    slider.valueFromTextFunction = nullptr;
}

void KnobHelpers::resetToDefault()
{
    slider.setValue (knobConfig.defaultValue, juce::sendNotificationSync);
}

void KnobHelpers::mouseDown (const juce::MouseEvent& event)
{
    // Nur der Rechtsklick oeffnet das Menue; Ziehen und Klicken bleiben beim
    // Slider selbst.
    if (event.mods.isPopupMenu())
        showMenu();
}

void KnobHelpers::showMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Reset");
    m.addItem (2, "Wert eingeben ...");
    m.addItem (3, "Kopieren");
    m.addItem (4, "Einfuegen", hasCopiedValue());

    // Das Menue antwortet asynchron. Der SafePointer auf den Slider ist die
    // Lebensversicherung: laut Vertrag lebt dieser Helfer genau so lange wie
    // der Slider, also ist `this` erst dann weg, wenn auch der SafePointer
    // null ist.
    juce::Component::SafePointer<juce::Slider> safeSlider (&slider);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
                     [this, safeSlider] (int result)
                     {
                         if (safeSlider == nullptr)
                             return;

                         switch (result)
                         {
                             case 1:
                                 resetToDefault();
                                 break;

                             case 2:
                                 // Kein modaler Dialog: direkt im Textfeld des
                                 // Reglers eingeben lassen.
                                 safeSlider->setTextBoxStyle (juce::Slider::TextBoxBelow,
                                                              false, 64, 14);
                                 safeSlider->showTextBox();

                                 if (auto* box = findSliderTextBox (safeSlider.getComponent()))
                                 {
                                     if (auto* editor = box->getCurrentTextEditor())
                                         editor->grabKeyboardFocus();
                                     else
                                         box->grabKeyboardFocus();
                                 }

                                 break;

                             case 3:
                                 copyValue (safeSlider->getValue());
                                 break;

                             case 4:
                             {
                                 double v = 0.0;

                                 if (pasteValue (v))
                                     safeSlider->setValue (v, juce::sendNotificationSync);

                                 break;
                             }

                             default:
                                 break;
                         }
                     });
}

} // namespace bfka
