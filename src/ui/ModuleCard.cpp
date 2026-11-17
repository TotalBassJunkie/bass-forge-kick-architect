#include "ui/ModuleCard.h"

#include <cmath>

#include "plugin/PluginProcessor.h"
#include "ui/ModuleStructureView.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    const int kHeaderHeight   = 26;
    const int kMixRowHeight   = 78;
    const int kLabelHeight    = 13;
    const int kDisplayHeight  = 54;
    const int kKnobWidth      = 88;
    const int kKnobHeight     = 86;
    const int kColumns        = 6;
    const int kPadding        = 8;

    /** Rundes Rotary-Setup im Look des Themas. */
    void styleKnob (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
        slider.setColour (juce::Slider::rotarySliderFillColourId, ui::theme::accent);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, ui::theme::grid);
        slider.setColour (juce::Slider::textBoxTextColourId, ui::theme::text);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void styleMiniButton (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
        button.setColour (juce::TextButton::textColourOffId, ui::theme::text);
        button.setColour (juce::TextButton::textColourOnId, ui::theme::accent);
        button.setConnectedEdges (0);
    }

    void styleCaption (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (ui::theme::smallFont());
        label.setColour (juce::Label::textColourId, ui::theme::textDim);
        label.setJustificationType (juce::Justification::centred);
        label.setInterceptsMouseClicks (false, false);
    }
} // namespace

ModuleCard::ModuleCard (PluginProcessor& processor, int slot, Listener& owner)
    : proc (processor),
      cardOwner (owner),
      moduleSlot (slot)
{
    const auto& chain = proc.getFxChain();
    const auto* moduleState = (slot >= 0 && slot < (int) chain.modules.size())
                                  ? &chain.modules[(size_t) slot]
                                  : nullptr;

    typeString = moduleState != nullptr ? moduleState->type : juce::String();

    if (const auto* module = proc.getModule (slot))
    {
        cardTitle   = module->displayName();
        descriptors = module->params();
    }
    else
    {
        const auto parsed = fxTypeFromString (typeString);
        cardTitle = parsed.has_value() ? fxDisplayName (*parsed) : typeString;
    }

    buildControls();

    structureView = createStructureView (proc, moduleSlot);

    if (structureView != nullptr)
        addAndMakeVisible (*structureView);

    setSize (100, preferredHeight());
}

ModuleCard::~ModuleCard() = default;

void ModuleCard::buildControls()
{
    // --- Kopfzeile --------------------------------------------------------
    powerButton.setButtonText ("ON");
    upButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x25B2));
    downButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x25BC));
    duplicateButton.setButtonText ("CP");
    removeButton.setButtonText ("X");

    for (auto* button : { &powerButton, &upButton, &downButton, &duplicateButton, &removeButton })
    {
        styleMiniButton (*button);
        addAndMakeVisible (*button);
    }

    powerButton.onClick = [this]
    {
        const auto& chain = proc.getFxChain();

        if (moduleSlot < 0 || moduleSlot >= (int) chain.modules.size())
            return;

        proc.setModuleEnabled (moduleSlot, ! chain.modules[(size_t) moduleSlot].enabled);
        updatePowerLook();
        repaint();
    };

    upButton.onClick        = [this] { cardOwner.moduleCardMoveRequested (moduleSlot, -1); };
    downButton.onClick      = [this] { cardOwner.moduleCardMoveRequested (moduleSlot, 1); };
    duplicateButton.onClick = [this] { cardOwner.moduleCardDuplicateRequested (moduleSlot); };
    removeButton.onClick    = [this] { cardOwner.moduleCardRemoveRequested (moduleSlot); };

    // --- MIX / OUT --------------------------------------------------------
    styleCaption (mixLabel, "MIX");
    styleCaption (outLabel, "OUT GAIN");
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (outLabel);

    styleKnob (mixSlider);
    styleKnob (outSlider);

    mixSlider.setRange (0.0, 1.0, 0.001);
    mixSlider.textFromValueFunction = [] (double v)
    {
        return juce::String (juce::roundToInt (v * 100.0)) + " %";
    };

    outSlider.setRange (0.0, 2.0, 0.001);
    outSlider.textFromValueFunction = [] (double v)
    {
        return juce::String (juce::Decibels::gainToDecibels (v), 1) + " dB";
    };

    // Startwerte aus dem Kettenzustand (nicht aus dem Modul: MIX/OUT gehoeren
    // der Kette). `updateText()` unten zieht die Textfelder nach.
    {
        const auto& chain = proc.getFxChain();

        if (moduleSlot >= 0 && moduleSlot < (int) chain.modules.size())
        {
            mixSlider.setValue (chain.modules[(size_t) moduleSlot].mix, juce::dontSendNotification);
            outSlider.setValue (chain.modules[(size_t) moduleSlot].outGain, juce::dontSendNotification);
        }
    }

    mixSlider.onValueChange = [this]
    {
        proc.setModuleMix (moduleSlot, (float) mixSlider.getValue());
    };

    outSlider.onValueChange = [this]
    {
        proc.setModuleOutGain (moduleSlot, (float) outSlider.getValue());
    };

    addAndMakeVisible (mixSlider);
    addAndMakeVisible (outSlider);

    // --- Parameter-Raster ------------------------------------------------
    for (size_t i = 0; i < descriptors.size(); ++i)
    {
        const auto& desc = descriptors[i];

        auto slider = std::make_unique<juce::Slider>();
        styleKnob (*slider);

        slider->setRange (desc.minValue, desc.maxValue, 0.001);
        slider->setValue (proc.getModuleParam (moduleSlot, (int) i), juce::dontSendNotification);

        // Schalter und Modus-Indizes ohne Einheit: ganzzahlig, 0/1 als on/off.
        if (desc.unit.isEmpty())
        {
            if (desc.minValue == 0.0f && desc.maxValue == 1.0f)
            {
                slider->setRange (0.0, 1.0, 1.0);
                slider->textFromValueFunction = [] (double v)
                {
                    return juce::String (v >= 0.5 ? "on" : "off");
                };
            }
            else
            {
                slider->setRange (desc.minValue, desc.maxValue, 1.0);
                slider->textFromValueFunction = [] (double v)
                {
                    return juce::String (juce::roundToInt (v));
                };
            }
        }
        else
        {
            const auto unit = desc.unit;
            slider->textFromValueFunction = [unit] (double v)
            {
                return juce::String (v, std::abs (v) >= 100.0 ? 0 : 2) + " " + unit;
            };
        }

        const int index = (int) i;
        auto* raw = slider.get();

        slider->onValueChange = [this, index, raw]
        {
            proc.setModuleParam (moduleSlot, index, (float) raw->getValue());
        };

        addAndMakeVisible (*slider);
        knobs.push_back (std::move (slider));
    }

    knobAreas.assign (knobs.size(), {});

    // `textFromValueFunction` wirkt erst beim naechsten Text-Update; `setValue`
    // lief davor, deshalb hier einmal explizit nachziehen.
    mixSlider.updateText();
    outSlider.updateText();

    for (auto& knob : knobs)
        knob->updateText();

    updatePowerLook();
}

void ModuleCard::updatePowerLook()
{
    const auto& chain = proc.getFxChain();
    const bool enabled = moduleSlot >= 0 && moduleSlot < (int) chain.modules.size()
                             && chain.modules[(size_t) moduleSlot].enabled;

    powerButton.setButtonText (enabled ? "ON" : "OFF");
    powerButton.setColour (juce::TextButton::buttonColourId,
                           enabled ? ui::theme::ok.withAlpha (0.35f) : ui::theme::panelAlt);
    powerButton.setColour (juce::TextButton::textColourOffId,
                           enabled ? ui::theme::text : ui::theme::textDim);
}

int ModuleCard::preferredHeight() const
{
    const int knobRows = descriptors.empty()
                             ? 0
                             : ((int) descriptors.size() + kColumns - 1) / kColumns;

    int height = kPadding + kHeaderHeight + 6 + kMixRowHeight + 6;

    if (structureView != nullptr)
        height += structureView->preferredHeight() + 6;

    height += kDisplayHeight + 8 + knobRows * kKnobHeight + kPadding;

    return height;
}

void ModuleCard::resized()
{
    auto area = getLocalBounds().reduced (kPadding);

    // Kopfzeile
    auto header = area.removeFromTop (kHeaderHeight);
    removeButton.setBounds (header.removeFromRight (26).reduced (1));
    duplicateButton.setBounds (header.removeFromRight (30).reduced (1));
    downButton.setBounds (header.removeFromRight (26).reduced (1));
    upButton.setBounds (header.removeFromRight (26).reduced (1));
    powerButton.setBounds (header.removeFromRight (44).reduced (1));

    area.removeFromTop (6);

    // MIX / OUT
    auto mixRow = area.removeFromTop (kMixRowHeight);
    auto mixCell = mixRow.removeFromLeft (kKnobWidth);
    mixLabel.setBounds (mixCell.removeFromTop (kLabelHeight));
    mixSlider.setBounds (mixCell);

    auto outCell = mixRow.removeFromLeft (kKnobWidth);
    outLabel.setBounds (outCell.removeFromTop (kLabelHeight));
    outSlider.setBounds (outCell);

    area.removeFromTop (6);

    // Struktur-Editor
    if (structureView != nullptr)
    {
        structureView->setBounds (area.removeFromTop (structureView->preferredHeight()));
        area.removeFromTop (6);
    }

    // Wellenform
    area.removeFromTop (kDisplayHeight + 8);

    // Parameter-Raster
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int row = (int) i / kColumns;
        const int col = (int) i % kColumns;

        const int cellWidth  = juce::jmax (60, (getWidth() - 2 * kPadding) / kColumns);
        const int rowTop     = area.getY() + row * kKnobHeight;

        juce::Rectangle<int> cell (area.getX() + col * cellWidth, rowTop, cellWidth, kKnobHeight);

        knobAreas[i] = cell;
        knobs[i]->setBounds (cell.withTrimmedTop (kLabelHeight));
    }
}

void ModuleCard::refresh()
{
    const auto& chain = proc.getFxChain();

    if (moduleSlot < 0 || moduleSlot >= (int) chain.modules.size())
        return;

    const auto& moduleState = chain.modules[(size_t) moduleSlot];

    if (std::abs ((float) mixSlider.getValue() - moduleState.mix) > 1.0e-4f
        && ! mixSlider.isMouseButtonDown())
        mixSlider.setValue (moduleState.mix, juce::dontSendNotification);

    if (std::abs ((float) outSlider.getValue() - moduleState.outGain) > 1.0e-4f
        && ! outSlider.isMouseButtonDown())
        outSlider.setValue (moduleState.outGain, juce::dontSendNotification);

    updatePowerLook();

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = *knobs[i];

        if (knob.isMouseButtonDown())
            continue;

        const auto value = proc.getModuleParam (moduleSlot, (int) i);

        if (std::abs ((float) knob.getValue() - value) > 1.0e-4f)
            knob.setValue (value, juce::dontSendNotification);
    }

    if (structureView != nullptr)
        structureView->refresh();

    repaint();
}

void ModuleCard::paintDisplay (juce::Graphics& g, juce::Rectangle<int> area)
{
    ui::theme::paintPanel (g, area.toFloat(), ui::theme::panelDeep, 6.0f);

    const auto inner = area.reduced (4).toFloat();

    g.setColour (ui::theme::textDim);
    g.setFont (ui::theme::smallFont());
    g.drawText ("PRE / POST", area.reduced (6, 3), juce::Justification::topRight, false);

    const auto& display = proc.getModuleDisplay (moduleSlot);

    if (! display.valid || display.post.getNumSamples() <= 0)
    {
        g.setColour (ui::theme::textDim.withAlpha (0.5f));
        g.drawText ("kein Signal", area, juce::Justification::centred, false);
        return;
    }

    const auto numSamples = display.post.getNumSamples();
    const auto channels   = juce::jmin (2, display.post.getNumChannels());

    const auto drawBuffer = [&] (const juce::AudioBuffer<float>& buffer, juce::Colour colour,
                                 float thickness)
    {
        juce::Path path;

        for (int x = 0; x < (int) inner.getWidth(); ++x)
        {
            const int index = juce::jlimit (0, numSamples - 1,
                                            (int) ((float) x / inner.getWidth() * (float) numSamples));

            float sample = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                sample += buffer.getSample (ch, index);

            sample /= (float) juce::jmax (1, channels);

            const auto y = inner.getCentreY()
                           - juce::jlimit (-1.0f, 1.0f, sample) * inner.getHeight() * 0.45f;

            if (x == 0)
                path.startNewSubPath (inner.getX(), y);
            else
                path.lineTo (inner.getX() + (float) x, y);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness));
    };

    drawBuffer (display.post, ui::theme::accent, 1.4f);
    drawBuffer (display.pre, ui::theme::textDim.withAlpha (0.45f), 1.0f);

    g.setColour (ui::theme::grid.withAlpha (0.6f));
    g.drawHorizontalLine ((int) inner.getCentreY(), inner.getX(), inner.getRight());
}

void ModuleCard::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& chain = proc.getFxChain();
    const bool enabled = moduleSlot >= 0 && moduleSlot < (int) chain.modules.size()
                             && chain.modules[(size_t) moduleSlot].enabled;

    g.setColour (ui::theme::panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (enabled ? ui::theme::accent.withAlpha (0.45f) : ui::theme::grid);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    auto area = getLocalBounds().reduced (kPadding);
    auto header = area.removeFromTop (kHeaderHeight);

    g.setFont (ui::theme::cardTitleFont());
    g.setColour (enabled ? ui::theme::text : ui::theme::textDim);
    g.drawText (cardTitle, header.withTrimmedRight (170), juce::Justification::centredLeft, true);

    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText (juce::String (moduleSlot + 1) + "  " + typeString,
                header.withTrimmedRight (170), juce::Justification::centredRight, true);

    area.removeFromTop (6);
    area.removeFromTop (kMixRowHeight);
    area.removeFromTop (6);

    if (structureView != nullptr)
        area.removeFromTop (structureView->preferredHeight() + 6);

    paintDisplay (g, area.removeFromTop (kDisplayHeight));
    area.removeFromTop (8);

    // Parameter-Namen ueber den Reglern
    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);

    for (size_t i = 0; i < knobs.size() && i < descriptors.size() && i < knobAreas.size(); ++i)
        g.drawText (descriptors[i].name,
                    knobAreas[i].withHeight (kLabelHeight),
                    juce::Justification::centred, false);

    if (descriptors.empty())
    {
        g.setFont (ui::theme::bodyFont());
        g.setColour (ui::theme::warn);
        g.drawText ("Fuer diesen Typ gibt es kein Modul - keine Parameter.",
                    area.removeFromTop (24), juce::Justification::centredLeft, false);
    }
}

} // namespace bfka
