#include "ui/SlotCard.h"

#include <cmath>

#include "plugin/PluginProcessor.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr int kBandHeight    = 16;
    constexpr int kNameHeight    = 20;
    constexpr int kFlagsHeight   = 20;
    constexpr int kWaveHeight    = 30;
    constexpr int kCaptionHeight = 12;
    constexpr int kBigKnobRow    = 74;
    constexpr int kSmallKnobRow  = 64;

    /** Luft zwischen den Bloecken, damit sich Beschriftungen nicht beruehren. */
    constexpr int kRowGap = 5;

    /** Groesse der drei grossen und der vier Detailregler. */
    constexpr size_t kBigKnobCount = 3;

    /** Farbe des Rollenbandes. */
    juce::Colour colourForRole (Role role) noexcept
    {
        switch (role)
        {
            case Role::punch:           return ui::theme::accent;
            case Role::crunch:          return ui::theme::warn;
            case Role::rumble:          return ui::theme::bandLow;
            case Role::click:           return ui::theme::bandHigh;
            case Role::top:             return ui::theme::bandMid;
            case Role::punchEnhancer:   return ui::theme::accent.brighter (0.25f);
            case Role::rumbleEnhancer:  return ui::theme::bandLow.brighter (0.25f);
            case Role::schranzEnhancer: return juce::Colour (0xffa06ce0);
            case Role::none:            break;
        }

        return ui::theme::textDim;
    }

    void styleKnob (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 12);
        slider.setColour (juce::Slider::rotarySliderFillColourId, ui::theme::accent);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, ui::theme::grid);
        slider.setColour (juce::Slider::textBoxTextColourId, ui::theme::text);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void styleButton (juce::TextButton& button, juce::Colour fill, juce::Colour text)
    {
        button.setColour (juce::TextButton::buttonColourId, fill);
        button.setColour (juce::TextButton::textColourOffId, text);
        button.setColour (juce::TextButton::textColourOnId, ui::theme::text);
    }

    /** Schaltet einen Knopf sichtbar um. */
    void showToggleState (juce::TextButton& button, bool on, juce::Colour onColour)
    {
        button.setColour (juce::TextButton::buttonColourId,
                          on ? onColour.withAlpha (0.45f) : ui::theme::panelAlt);
        button.setColour (juce::TextButton::textColourOffId,
                          on ? ui::theme::text : ui::theme::textDim);
    }

    /** Normalisierter APVTS-Wert (0, wenn der Parameter fehlt). */
    float readParam (PluginProcessor& proc, const juce::String& id)
    {
        if (auto* value = proc.parameters().getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }
} // namespace

SlotCard::SlotCard (PluginProcessor& processor, int slot, Listener& owner)
    : proc (processor),
      cardOwner (owner),
      slotIndex_ (slot)
{
    buildControls();
    setSize (320, kPreferredHeight);
}

SlotCard::~SlotCard() = default;

void SlotCard::buildControls()
{
    styleButton (loadButton, ui::theme::panel.brighter (0.15f), ui::theme::text);
    loadButton.setTooltip ("Sample in diesen Slot laden");
    loadButton.onClick = [this] { cardOwner.slotCardLoadRequested (slotIndex_); };
    addAndMakeVisible (loadButton);

    styleButton (clearButton, ui::theme::panelAlt, ui::theme::textDim);
    clearButton.setTooltip ("Slot leeren");
    clearButton.onClick = [this] { cardOwner.slotCardClearRequested (slotIndex_); };
    addAndMakeVisible (clearButton);

    // MUTE und SOLO sind Slot-Parameter (APVTS), LOCK und P gehoeren dem Zustand.
    for (auto* button : { &muteButton, &soloButton, &lockButton, &keyButton })
    {
        styleButton (*button, ui::theme::panelAlt, ui::theme::textDim);
        addAndMakeVisible (*button);
    }

    muteButton.setTooltip ("Slot stumm schalten");
    soloButton.setTooltip ("Nur diesen Slot spielen");

    for (auto* button : { &muteButton, &soloButton })
    {
        const auto suffix = button == &muteButton ? juce::String ("mute") : juce::String ("solo");

        button->onClick = [this, suffix]
        {
            const auto id = "l" + juce::String (slotIndex_) + "_" + suffix;
            const auto* desc = ParameterRegistry::get().find (id);

            if (desc == nullptr)
                return;

            writeParam (*desc, readParam (proc, id) >= 0.5f ? 0.0 : 1.0);
            refreshFlags (proc.captureState());
            repaint();
        };
    }

    lockButton.setTooltip ("Slot gegen Randomizer/Stilwechsel sperren");
    lockButton.onClick = [this]
    {
        proc.setSlotLocked (slotIndex_, ! locked);
        locked = ! locked;
        repaint();
        cardOwner.slotCardChanged (slotIndex_);
    };

    keyButton.setTooltip ("KEY TRACK: Slot folgt der MIDI-Note");
    keyButton.onClick = [this]
    {
        proc.setSlotKeyTracked (slotIndex_, ! keyTracked);
        keyTracked = ! keyTracked;
        repaint();
        cardOwner.slotCardChanged (slotIndex_);
    };

    // --- Regler -----------------------------------------------------------
    // Zwei Reihen: die drei grossen Regler der Slot-Karte, darunter die
    // Detailregler (Trims und Fades) aus dem Referenzformat.
    static const char* const bigIds[] { "vol", "pitch", "stretch" };
    static const char* const smallIds[] { "trimstart", "trimend", "fadein", "fadeout" };

    const auto addKnob = [this] (const juce::String& suffix)
    {
        const auto id = "l" + juce::String (slotIndex_) + "_" + suffix;
        const auto* desc = ParameterRegistry::get().find (id);

        if (desc == nullptr)
            return;

        Knob knob;
        knob.desc = desc;
        knob.slider = std::make_unique<juce::Slider>();

        auto& slider = *knob.slider;
        styleKnob (slider);
        // ID = Parameter-ID: macht die Regler fuer Tests und Accessibility greifbar.
        slider.setComponentID (id);

        // Kein Raster: der Wert ist normalisiert (0..1), ein Intervall wuerde ihn
        // auf 4 Stellen runden — bei dB waere das ein sichtbarer Anzeigefehler
        // ("-0.00 dB" statt "0.00 dB").
        slider.setRange (0.0, 1.0, 0.0);
        slider.setDoubleClickReturnValue (true, desc->normalisedDefault());
        slider.setValue (readParam (proc, id), juce::dontSendNotification);

        // Anzeigetext in Anzeige-Einheiten (dB, Halbtoene, %).
        slider.textFromValueFunction = [desc] (double v) { return desc->format ((float) v); };
        slider.updateText();

        // Der Slider bleibt an derselben Adresse (unique_ptr wird nur verschoben),
        // deshalb genuegt der rohe Zeiger im Lambda.
        auto* const sliderPointer = &slider;

        slider.onValueChange = [this, desc, sliderPointer]
        {
            writeParam (*desc, sliderPointer->getValue());
            cardOwner.slotCardChanged (slotIndex_);
        };

        addAndMakeVisible (slider);
        knobs.push_back (std::move (knob));
    };

    for (const auto* suffix : bigIds)
        addKnob (suffix);

    for (const auto* suffix : smallIds)
        addKnob (suffix);
}

void SlotCard::writeParam (const ParamDesc& desc, double normalised)
{
    if (auto* parameter = proc.parameters().getParameter (desc.id))
        parameter->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, normalised));

    // Sofort in den Renderer: der naechste Kick klingt mit dem neuen Wert.
    proc.applySlotSettings (slotIndex_);
}

void SlotCard::refreshFlags (const KickState& state)
{
    const auto prefix = "l" + juce::String (slotIndex_) + "_";

    showToggleState (muteButton, readParam (proc, prefix + "mute") >= 0.5f, ui::theme::error);
    showToggleState (soloButton, readParam (proc, prefix + "solo") >= 0.5f, ui::theme::ok);
    showToggleState (lockButton, state.isLocked (slotIndex_), ui::theme::accent);
    showToggleState (keyButton, state.isKeyTracked (slotIndex_), ui::theme::bandHigh);

    locked     = state.isLocked (slotIndex_);
    keyTracked = state.isKeyTracked (slotIndex_);
}

void SlotCard::refreshKnobs()
{
    for (auto& knob : knobs)
    {
        if (knob.slider == nullptr || knob.desc == nullptr)
            continue;

        auto& slider = *knob.slider;

        if (slider.isMouseButtonDown())
            continue;

        const auto value = readParam (proc, knob.desc->id);

        if (std::abs ((float) slider.getValue() - value) > 1.0e-4f)
            slider.setValue (value, juce::dontSendNotification);

        // `textFromValueFunction` wirkt erst beim naechsten Text-Update.
        slider.updateText();
    }
}

void SlotCard::refresh (const KickState& state)
{
    const auto& sample = state.getSample (slotIndex_);

    stateHasSample = sample.has_value() && sample->isValid();
    sampleName     = stateHasSample ? sample->fileName : juce::String();
    role           = state.getRole (slotIndex_);
    roleText       = roleToString (role);
    sampleLength   = proc.getSlotEnvelope (slotIndex_).lengthSeconds;

    if (stateHasSample)
        detailText = sampleLength > 0.0
                         ? juce::String (sampleLength * 1000.0, 0) + " ms"
                         : juce::String ("-");
    else
        detailText = "leer";

    // Anzeige-Werte fuer die Wellenform (kein Zustandslesen beim Zeichnen).
    const auto prefix = "l" + juce::String (slotIndex_) + "_";

    trimStartPercent = state.getParam (prefix + "trimstart") * 100.0f;
    trimEndPercent   = state.getParam (prefix + "trimend") * 100.0f;
    fadeInPercent    = state.getParam (prefix + "fadein") * 100.0f;
    fadeOutPercent   = state.getParam (prefix + "fadeout") * 100.0f;

    if (const auto* volume = ParameterRegistry::get().find (prefix + "vol"))
        volumeDb = volume->toEngineering (state.getParam (volume->id));

    refreshFlags (state);
    refreshKnobs();

    clearButton.setEnabled (stateHasSample);
    clearButton.setAlpha (stateHasSample ? 1.0f : 0.4f);

    repaint();
}

// ---------------------------------------------------------------------------
// Zeichnen
// ---------------------------------------------------------------------------
void SlotCard::paintBand (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const auto colour = colourForRole (role);

    g.setColour (colour.withAlpha (stateHasSample ? 0.30f : 0.12f));
    g.fillRoundedRectangle (area.toFloat(), 4.0f);

    g.setFont (ui::theme::sectionFont());
    g.setColour (stateHasSample ? ui::theme::text : ui::theme::textDim);
    g.drawText (roleText.isEmpty() ? juce::String ("SLOT " + juce::String (slotIndex_ + 1)) : roleText,
                area.reduced (7, 0), juce::Justification::centredLeft, false);

    // Pegel-Strip rechts: VOL in dB auf -60..+10 abgebildet.
    auto strip = area.reduced (6, 4).removeFromRight (46).toFloat();

    g.setColour (ui::theme::panelDeep);
    g.fillRect (strip);

    const auto fraction = juce::jlimit (0.0f, 1.0f, (volumeDb + 60.0f) / 70.0f);

    g.setColour (volumeDb > 0.5f ? ui::theme::warn : ui::theme::accent);
    g.fillRect (strip.withWidth (strip.getWidth() * fraction));

    // Dauer rechts neben dem Rollennamen.
    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText (detailText, area.reduced (7, 0).withTrimmedRight (52),
                juce::Justification::centredRight, false);
}

void SlotCard::paintWaveform (juce::Graphics& g, juce::Rectangle<int> area) const
{
    ui::theme::paintPanel (g, area.toFloat(), ui::theme::panelDeep, 5.0f);

    const auto inner = area.reduced (3);

    if (! stateHasSample || sampleLength <= 0.0)
    {
        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim.withAlpha (0.7f));
        g.drawText ("kein Sample", inner, juce::Justification::centred, false);
        return;
    }

    const auto& envelope = proc.getSlotEnvelope (slotIndex_);

    if (! envelope.valid)
    {
        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim.withAlpha (0.7f));
        g.drawText ("nicht geladen", inner, juce::Justification::centred, false);
        return;
    }

    const auto toX = [&inner] (float percent)
    {
        return (float) inner.getX()
               + juce::jlimit (0.0f, 1.0f, percent * 0.01f) * (float) inner.getWidth();
    };

    const auto midY  = (float) inner.getCentreY();
    const auto halfH = (float) inner.getHeight() * 0.5f - 1.0f;
    const auto peak  = juce::jmax (0.0001f, (float) envelope.peak);

    const auto colour  = colourForRole (role);
    const auto buckets = (int) KickAssembler::SampleEnvelope::kBuckets;
    const auto barWidth = juce::jmax (1.0f, (float) inner.getWidth() / (float) buckets);

    for (int b = 0; b < buckets; ++b)
    {
        const auto bucketMin = envelope.minimum[(size_t) b];
        const auto bucketMax = envelope.maximum[(size_t) b];

        const auto x = (float) inner.getX() + (float) b * barWidth;

        const auto yTop = midY - juce::jlimit (-1.0f, 1.0f, bucketMax / peak) * halfH;
        const auto yBot = midY - juce::jlimit (-1.0f, 1.0f, bucketMin / peak) * halfH;

        g.setColour (colour.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (x, juce::jmin (yTop, yBot), barWidth,
                                            juce::jmax (1.0f, std::abs (yBot - yTop))));
    }

    // Trim-Fenster abdunkeln.
    g.setColour (ui::theme::background.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> ((float) inner.getX(), (float) inner.getY(),
                                        juce::jmax (0.0f, toX (trimStartPercent) - (float) inner.getX()),
                                        (float) inner.getHeight()));
    g.fillRect (juce::Rectangle<float> (toX (trimEndPercent), (float) inner.getY(),
                                        juce::jmax (0.0f, (float) inner.getRight() - toX (trimEndPercent)),
                                        (float) inner.getHeight()));

    // Trim-Grenzen und Fade-Marken (Fades relativ zum getrimmten Fenster).
    const auto windowSize = juce::jmax (0.01f, trimEndPercent - trimStartPercent);

    g.setColour (ui::theme::text.withAlpha (0.35f));
    g.drawVerticalLine ((int) toX (trimStartPercent), (float) inner.getY(), (float) inner.getBottom());
    g.drawVerticalLine ((int) toX (trimEndPercent), (float) inner.getY(), (float) inner.getBottom());

    g.setColour (ui::theme::ok.withAlpha (0.7f));

    if (fadeInPercent > 0.01f)
        g.drawVerticalLine ((int) toX (trimStartPercent + windowSize * fadeInPercent * 0.01f),
                            (float) inner.getY(), (float) inner.getBottom());

    if (fadeOutPercent > 0.01f)
        g.drawVerticalLine ((int) toX (trimEndPercent - windowSize * fadeOutPercent * 0.01f),
                            (float) inner.getY(), (float) inner.getBottom());
}

void SlotCard::paintKnobCaptions (juce::Graphics& g) const
{
    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);

    for (const auto& knob : knobs)
    {
        if (knob.desc == nullptr || knob.area.isEmpty())
            continue;

        g.drawText (knob.desc->displayName.toUpperCase(), knob.area.withHeight (kCaptionHeight),
                    juce::Justification::centred, false);
    }
}

void SlotCard::paint (juce::Graphics& g)
{
    ui::theme::paintPanel (g, getLocalBounds().toFloat(), ui::theme::panel, 8.0f);

    paintBand (g, bandArea);
    paintWaveform (g, waveArea);
    paintKnobCaptions (g);

    // Sample-Name links in der Namenszeile (LOAD/X sitzen rechts).
    g.setFont (ui::theme::bodyFont());
    g.setColour (stateHasSample ? ui::theme::text : ui::theme::textDim);
    g.drawFittedText (stateHasSample ? sampleName : juce::String ("leer - LOAD druecken"),
                      nameArea, juce::Justification::centredLeft, 1);
}

void SlotCard::resized()
{
    auto area = getLocalBounds().reduced (8);

    bandArea = area.removeFromTop (kBandHeight);
    area.removeFromTop (kRowGap - 1);

    // Namenszeile: LOAD/X rechts, Name links (gemalt).
    auto nameRow = area.removeFromTop (kNameHeight);

    clearButton.setBounds (nameRow.removeFromRight (24).reduced (0, 1));
    nameRow.removeFromRight (3);
    loadButton.setBounds (nameRow.removeFromRight (52).reduced (0, 1));
    nameRow.removeFromRight (4);
    nameArea = nameRow;

    area.removeFromTop (3);

    // Flags-Zeile.
    flagsArea = area.removeFromTop (kFlagsHeight);

    auto flagRow = flagsArea;

    const auto placeFlag = [] (juce::Rectangle<int>& row, juce::TextButton& button, int width)
    {
        button.setBounds (row.removeFromLeft (width));
        row.removeFromLeft (4);
    };

    placeFlag (flagRow, muteButton, 50);
    placeFlag (flagRow, soloButton, 46);
    placeFlag (flagRow, lockButton, 50);
    placeFlag (flagRow, keyButton, 28);

    area.removeFromTop (kRowGap);

    waveArea = area.removeFromTop (kWaveHeight);
    area.removeFromTop (kRowGap);

    auto bigRow   = area.removeFromTop (kBigKnobRow);
    area.removeFromTop (kRowGap);

    auto smallRow = area.removeFromTop (kSmallKnobRow);

    const auto layoutRow = [] (juce::Rectangle<int> row, std::vector<Knob>& all,
                               size_t firstIndex, size_t count)
    {
        if (count == 0 || row.getWidth() <= 0)
            return;

        const auto cellWidth = juce::jmax (1, row.getWidth() / (int) count);

        for (size_t i = 0; i < count; ++i)
        {
            auto& knob = all[firstIndex + i];
            knob.area = juce::Rectangle<int> (row.getX() + (int) i * cellWidth, row.getY(),
                                              cellWidth, row.getHeight());

            if (knob.slider != nullptr)
                knob.slider->setBounds (knob.area.withTrimmedTop (kCaptionHeight).reduced (1, 0));
        }
    };

    const auto bigCount = juce::jmin (kBigKnobCount, knobs.size());
    layoutRow (bigRow, knobs, 0, bigCount);

    if (knobs.size() > bigCount)
        layoutRow (smallRow, knobs, bigCount, knobs.size() - bigCount);
}

} // namespace bfka
