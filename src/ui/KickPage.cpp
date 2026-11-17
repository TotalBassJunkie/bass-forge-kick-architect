#include "ui/KickPage.h"

#include <cmath>

#include "core/ParameterRegistry.h"
#include "library/SampleLibrary.h"
#include "plugin/PluginProcessor.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr int kTransportHeight = 30;
    constexpr int kVisualizerHeight = 150;
    constexpr int kStatusHeight = 20;
    constexpr int kPageGap = 6;
    constexpr int kColumns = 3;
    constexpr int kCardGap = 8;

    /** Nach dieser Zeit verschwindet eine Statusmeldung wieder (ms). */
    constexpr double kStatusTimeoutMs = 6000.0;

    /** dB -> linear (wie im Assembler). */
    float dbToLinear (float db)
    {
        return db <= -100.0f ? 0.0f : std::pow (10.0f, db * 0.05f);
    }

    float paramValue (PluginProcessor& proc, const juce::String& id)
    {
        if (auto* value = proc.parameters().getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    /** Normalisierter Parameter als Anzeigewert (0, wenn unbekannt). */
    float paramEngineering (PluginProcessor& proc, const juce::String& id, float fallback)
    {
        const auto* desc = ParameterRegistry::get().find (id);

        if (desc == nullptr)
            return fallback;

        return desc->toEngineering (paramValue (proc, id));
    }

    juce::Colour roleColour (Role role)
    {
        switch (role)
        {
            case Role::punch:           return ui::theme::accent;
            case Role::crunch:          return ui::theme::warn;
            case Role::rumble:          return ui::theme::bandLow;
            case Role::click:           return ui::theme::bandHigh;
            case Role::top:             return ui::theme::bandMid;
            case Role::schranzEnhancer: return juce::Colour (0xffa06ce0);
            case Role::none:            break;
            default:                    break;
        }

        return ui::theme::accentDim;
    }

    void styleBar (juce::Slider& slider, double minimum, double maximum)
    {
        slider.setSliderStyle (juce::Slider::LinearBar);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (minimum, maximum, 1.0);
        slider.setColour (juce::Slider::trackColourId, ui::theme::accent.withAlpha (0.35f));
        slider.setColour (juce::Slider::backgroundColourId, ui::theme::panelDeep);
        slider.setColour (juce::Slider::textBoxTextColourId, ui::theme::text);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void styleToolbarButton (juce::TextButton& button, juce::Colour fill, juce::Colour text)
    {
        button.setColour (juce::TextButton::buttonColourId, fill);
        button.setColour (juce::TextButton::textColourOffId, text);
        button.setColour (juce::TextButton::textColourOnId, ui::theme::text);
    }
} // namespace

// ---------------------------------------------------------------------------
// ValueBar
// ---------------------------------------------------------------------------
void ValueBar::paint (juce::Graphics& g)
{
    juce::Slider::paint (g);

    g.setFont (ui::theme::smallFont());
    g.setColour (isEnabled() ? ui::theme::text : ui::theme::textDim);
    g.drawFittedText (getTextFromValue (getValue ()), getLocalBounds().reduced (4, 0),
                      juce::Justification::centred, 1);
}

// ---------------------------------------------------------------------------
// KickVisualizer
// ---------------------------------------------------------------------------
KickVisualizer::KickVisualizer (PluginProcessor& processor)
    : proc (processor)
{
    setSize (900, kVisualizerHeight);
    refresh();
}

void KickVisualizer::refresh()
{
    snapshot   = proc.captureState();
    kickLength = juce::jmax (0.05, proc.getKickLengthSeconds());
    beatLength = juce::jmax (0.02, proc.getBeatLengthSeconds());

    repaint();
}

void KickVisualizer::setViewMode (ViewMode newMode)
{
    if (mode == newMode)
        return;

    mode = newMode;
    repaint();
}

void KickVisualizer::buildSumCurve ()
{
    sumCurve.fill (0.0f);

    constexpr auto kBuckets = kSumBuckets;

    if (! (kickLength > 0.0))
        return;

    // Solo-Logik wie im Renderer: ein belegter Solo-Slot schaltet die anderen ab.
    auto anySolo = false;

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        const auto prefix = "l" + juce::String (slot) + "_";

        if (proc.getSlotEnvelope (slot).valid && paramValue (proc, prefix + "solo") >= 0.5f)
        {
            anySolo = true;
            break;
        }
    }

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        const auto& envelope = proc.getSlotEnvelope (slot);

        if (! envelope.valid || ! (envelope.lengthSeconds > 0.0))
            continue;

        const auto prefix = "l" + juce::String (slot) + "_";

        if (paramValue (proc, prefix + "mute") >= 0.5f)
            continue;

        if (anySolo && paramValue (proc, prefix + "solo") < 0.5f)
            continue;

        const auto gain = dbToLinear (paramEngineering (proc, prefix + "vol", 0.0f));
        const auto positionSeconds = (paramEngineering (proc, prefix + "pos", 50.0f) - 50.0f) * 0.01f;

        const auto trimStart = paramEngineering (proc, prefix + "trimstart", 0.0f) * 0.01f
                               * (float) envelope.lengthSeconds;

        auto trimEnd = paramEngineering (proc, prefix + "trimend", 100.0f) * 0.01f
                       * (float) envelope.lengthSeconds;

        if (! (trimEnd > trimStart))
            trimEnd = (float) envelope.lengthSeconds;

        const auto window = juce::jmax (1.0e-4f, trimEnd - trimStart);
        const auto fadeIn  = paramEngineering (proc, prefix + "fadein", 0.0f) * 0.01f * window;
        const auto fadeOut = paramEngineering (proc, prefix + "fadeout", 0.0f) * 0.01f * window;

        for (int b = 0; b < (int) KickAssembler::SampleEnvelope::kBuckets; ++b)
        {
            const auto sampleTime = (float) (b + 0.5) / (float) KickAssembler::SampleEnvelope::kBuckets
                                    * (float) envelope.lengthSeconds;

            if (sampleTime < trimStart || sampleTime > trimEnd)
                continue;

            const auto local = sampleTime - trimStart;

            auto value = juce::jmax (std::abs (envelope.minimum[(size_t) b]),
                                     std::abs (envelope.maximum[(size_t) b]));

            if (fadeIn > 0.0f && local < fadeIn)
                value *= local / fadeIn;

            if (fadeOut > 0.0f && local > window - fadeOut)
                value *= juce::jmax (0.0f, (window - local) / fadeOut);

            const auto kickTime = positionSeconds + local;

            if (kickTime < 0.0f || (double) kickTime > kickLength)
                continue;

            const auto index = (size_t) juce::jlimit (
                0, kBuckets - 1, (int) ((double) kickTime / kickLength * (double) kBuckets));

            sumCurve[index] += value * gain;
        }
    }
}

void KickVisualizer::paintGrid (juce::Graphics& g, juce::Rectangle<int> area) const
{
    // Viertelbeat-Raster, damit man die Kick-Laenge einordnen kann.
    const auto quarter = juce::jmax (0.01, beatLength * 0.25);
    const auto count   = (int) std::ceil (kickLength / quarter);

    g.setColour (ui::theme::grid);

    for (int i = 0; i <= count; ++i)
    {
        const auto seconds = (double) i * quarter;

        if (seconds > kickLength)
            break;

        const auto x = (float) area.getX()
                       + (float) (seconds / kickLength) * (float) area.getWidth();

        const auto isBeat = (i % 4) == 0;
        g.setColour (isBeat ? ui::theme::grid.brighter (0.35f) : ui::theme::grid);
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
    }
}

void KickVisualizer::paintParts (juce::Graphics& g, juce::Rectangle<int> area) const
{
    // Nur belegte Slots bekommen eine Zeile.
    std::vector<int> filled;

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
        if (proc.getSlotEnvelope (slot).valid)
            filled.push_back (slot);

    if (filled.empty())
    {
        g.setFont (ui::theme::bodyFont());
        g.setColour (ui::theme::textDim);
        g.drawText ("Noch kein Kick geladen - DEMO-KICK oder LOAD in einer Slot-Karte.",
                    area, juce::Justification::centred, false);
        return;
    }

    const auto laneHeight = juce::jmax (3, area.getHeight() / (int) filled.size());

    const auto xForTime = [this, &area] (double seconds)
    {
        return (float) area.getX()
               + (float) juce::jlimit (0.0, kickLength, seconds) / (float) kickLength
                     * (float) area.getWidth();
    };

    for (size_t i = 0; i < filled.size(); ++i)
    {
        const auto slot = filled[i];

        auto lane = juce::Rectangle<int> (area.getX(), area.getY() + (int) i * laneHeight,
                                          area.getWidth(), laneHeight).reduced (0, 1);

        const auto& envelope = proc.getSlotEnvelope (slot);

        if (! envelope.valid || ! (envelope.lengthSeconds > 0.0))
            continue;

        const auto prefix = "l" + juce::String (slot) + "_";
        const auto length = envelope.lengthSeconds;

        // Wie im Renderer: Position verschiebt den Part auf der Kick-Zeitachse,
        // Trims schneiden das Sample.
        const auto positionSeconds = (paramEngineering (proc, prefix + "pos", 50.0f) - 50.0f) * 0.01f;
        const auto trimStart = paramEngineering (proc, prefix + "trimstart", 0.0f) * 0.01f * (float) length;

        auto trimEnd = paramEngineering (proc, prefix + "trimend", 100.0f) * 0.01f * (float) length;

        if (! (trimEnd > trimStart))
            trimEnd = (float) length;

        const auto colour  = roleColour (snapshot.getRole (slot));
        const auto midY    = (float) lane.getCentreY();
        const auto halfH   = juce::jmax (1.0f, (float) lane.getHeight() * 0.5f - 0.5f);
        const auto peak    = juce::jmax (0.0001f, (float) envelope.peak);
        const auto buckets = (int) KickAssembler::SampleEnvelope::kBuckets;
        const auto barWidth = juce::jmax (1.0f, (float) area.getWidth() / (float) buckets);

        for (int b = 0; b < buckets; ++b)
        {
            const auto sampleTime = (float) (b + 0.5) / (float) buckets * (float) length;

            if (sampleTime < trimStart || sampleTime > trimEnd)
                continue;

            const auto kickTime = positionSeconds + (sampleTime - trimStart);

            if (kickTime < 0.0f || (double) kickTime > kickLength)
                continue;

            const auto x = xForTime (kickTime);

            const auto yTop = midY - juce::jlimit (-1.0f, 1.0f, envelope.maximum[(size_t) b] / peak) * halfH;
            const auto yBot = midY - juce::jlimit (-1.0f, 1.0f, envelope.minimum[(size_t) b] / peak) * halfH;

            g.setColour (colour.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (x, juce::jmin (yTop, yBot), barWidth,
                                                juce::jmax (1.0f, std::abs (yBot - yTop))));
        }

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim);
        g.drawText (juce::String (slot + 1), lane.removeFromLeft (16),
                    juce::Justification::centredLeft, false);
    }
}

void KickVisualizer::paintSum (juce::Graphics& g, juce::Rectangle<int> area)
{
    buildSumCurve ();

    auto peak = 0.0f;

    for (const auto value : sumCurve)
        peak = juce::jmax (peak, value);

    if (peak <= 0.0f)
    {
        g.setFont (ui::theme::bodyFont());
        g.setColour (ui::theme::textDim);
        g.drawText ("Kein Signal - Slots laden oder MUTE/SOLO pruefen.",
                    area, juce::Justification::centred, false);
        return;
    }

    const auto midY  = (float) area.getCentreY();
    const auto halfH = (float) area.getHeight() * 0.5f - 2.0f;
    const auto width = (float) area.getWidth() / (float) kSumBuckets;

    for (int i = 0; i < kSumBuckets; ++i)
    {
        const auto value = juce::jlimit (0.0f, 1.0f, sumCurve[(size_t) i] / peak);
        const auto x = (float) area.getX() + (float) i * width;
        const auto h = juce::jmax (1.0f, value * halfH);

        g.setColour (ui::theme::accent.withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (x, midY - h, juce::jmax (1.0f, width), h * 2.0f));
    }
}

void KickVisualizer::paint (juce::Graphics& g)
{
    ui::theme::paintPanel (g, getLocalBounds().toFloat(), ui::theme::panel, 8.0f);

    auto area = getLocalBounds().reduced (8);

    g.setFont (ui::theme::sectionFont());
    g.setColour (ui::theme::textDim);
    g.drawText (mode == ViewMode::parts ? "VISUALISER - SHOW PARTS" : "VISUALISER - SUM",
                area.removeFromTop (14), juce::Justification::centredLeft, false);

    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText (juce::String (kickLength * 1000.0, 0) + " ms  |  "
                    + juce::String (60.0 / beatLength, 1) + " BPM",
                getLocalBounds().reduced (8).removeFromTop (14),
                juce::Justification::centredRight, false);

    area.removeFromTop (2);

    // Zeitraster unten.
    auto ruler = area.removeFromBottom (12);
    auto plot  = area;

    ui::theme::paintPanel (g, plot.toFloat(), ui::theme::panelDeep, 5.0f);

    paintGrid (g, plot);

    if (mode == ViewMode::parts)
        paintParts (g, plot.reduced (2));
    else
        paintSum (g, plot.reduced (2));

    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText ("0", ruler.removeFromLeft (18), juce::Justification::centredLeft, false);
    g.drawText (juce::String (kickLength * 1000.0, 0) + " ms", ruler,
                juce::Justification::centredRight, false);
}

// ---------------------------------------------------------------------------
// KickPage
// ---------------------------------------------------------------------------
KickPage::KickPage (PluginProcessor& processor)
    : proc (processor)
{
    setSize (1100, 620);

    // --- Transport --------------------------------------------------------
    styleToolbarButton (playButton, ui::theme::accent.withAlpha (0.25f), ui::theme::text);
    playButton.setTooltip ("Kick einmal ausloesen");
    playButton.onClick = [this] { proc.triggerKick(); };
    addAndMakeVisible (playButton);

    styleToolbarButton (loopButton, ui::theme::panelAlt, ui::theme::textDim);
    loopButton.setTooltip ("Kick alle 1,5 s wiederholen");
    loopButton.onClick = [this] { toggleLoop(); };
    addAndMakeVisible (loopButton);

    styleToolbarButton (demoButton, ui::theme::panel.brighter (0.15f), ui::theme::text);
    demoButton.setTooltip ("Punch, Rumble und Click aus der Library laden");
    demoButton.onClick = [this]
    {
        const auto ok = proc.startDemoKick();
        syncFromProcessor();
        setStatus (proc.getLastLoadReport(), ! ok);
    };
    addAndMakeVisible (demoButton);

    styleBar (bpmSlider, 20.0, 960.0);
    bpmSlider.textFromValueFunction = [] (double v) { return juce::String (v, 0) + " BPM"; };
    bpmSlider.setValue (proc.getBpm(), juce::dontSendNotification);
    bpmSlider.onValueChange = [this]
    {
        proc.setBpm (bpmSlider.getValue());
        vis->refresh();
        repaint();
    };
    addAndMakeVisible (bpmSlider);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setFont (ui::theme::smallFont());
    bpmLabel.setColour (juce::Label::textColourId, ui::theme::textDim);
    addAndMakeVisible (bpmLabel);

    styleBar (masterSlider, 0.0, 1.0);
    masterSlider.setRange (0.0, 1.0, 0.0);
    masterSlider.textFromValueFunction = [] (double v)
    {
        return juce::String (juce::Decibels::gainToDecibels (v), 1) + " dB";
    };
    masterSlider.setValue (paramValue (proc, "master_vol"), juce::dontSendNotification);
    masterSlider.onValueChange = [this]
    {
        if (auto* parameter = proc.parameters().getParameter ("master_vol"))
            parameter->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, masterSlider.getValue()));
    };
    addAndMakeVisible (masterSlider);

    masterLabel.setText ("MASTER", juce::dontSendNotification);
    masterLabel.setFont (ui::theme::smallFont());
    masterLabel.setColour (juce::Label::textColourId, ui::theme::textDim);
    addAndMakeVisible (masterLabel);

    styleToolbarButton (viewButton, ui::theme::panelAlt, ui::theme::text);
    viewButton.setTooltip ("Zwischen Parts- und Summen-Ansicht umschalten");
    viewButton.onClick = [this]
    {
        const auto next = vis->viewMode() == KickVisualizer::ViewMode::parts
                              ? KickVisualizer::ViewMode::sum
                              : KickVisualizer::ViewMode::parts;

        vis->setViewMode (next);
        viewButton.setButtonText (next == KickVisualizer::ViewMode::parts ? "VIEW: PARTS"
                                                                         : "VIEW: SUM");
    };
    addAndMakeVisible (viewButton);

    // --- Visualiser -------------------------------------------------------
    vis = std::make_unique<KickVisualizer> (proc);
    addAndMakeVisible (*vis);

    // --- Slot-Karten ------------------------------------------------------
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&cardHolder, false);
    viewport.setScrollBarsShown (true, false);

    buildCards();

    startTimerHz (refreshRateHz);
    syncFromProcessor();

    // `setSize()` oben lief, bevor es die Kinder gab - ohne diesen Aufruf bleibt
    // die Seite bis zum ersten Groessenaenderung unausgelegt (Tests, die die
    // Seite direkt rendern, saehen eine leere Flaeche).
    resized();
}

KickPage::~KickPage() = default;

void KickPage::buildCards()
{
    cards.clear();

    for (int slot = 0; slot < KickState::kNumSlots; ++slot)
    {
        // Der Listener ist eine private Basis; die Umwandlung ist hier erlaubt,
        // in der Vorlagen-Instanziierung von make_unique aber nicht.
        auto card = std::make_unique<SlotCard> (proc, slot,
                                                static_cast<SlotCard::Listener&> (*this));
        cardHolder.addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }
}

SlotCard* KickPage::getSlotCard (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, (int) cards.size()))
        return nullptr;

    return cards[(size_t) slot].get();
}

void KickPage::setStatus (const juce::String& text, bool isError)
{
    statusText    = text;
    statusIsError = isError;
    statusSetMs   = juce::Time::getMillisecondCounterHiRes ();
    repaint (statusArea);
}

void KickPage::setLoopEnabled (bool shouldLoop)
{
    loopEnabled   = shouldLoop;
    lastTriggerMs = juce::Time::getMillisecondCounterHiRes();
    updateToolbarLook();
}

void KickPage::setRefreshRateHz (int hz)
{
    refreshRateHz = juce::jlimit (1, 60, hz);
    startTimerHz (refreshRateHz);
}

void KickPage::toggleLoop()
{
    setLoopEnabled (! loopEnabled);
}

void KickPage::updateToolbarLook()
{
    styleToolbarButton (loopButton,
                        loopEnabled ? ui::theme::ok.withAlpha (0.35f) : ui::theme::panelAlt,
                        loopEnabled ? ui::theme::text : ui::theme::textDim);
}

void KickPage::syncFromProcessor()
{
    const auto state = proc.captureState();

    for (auto& card : cards)
        if (card != nullptr)
            card->refresh (state);

    if (vis != nullptr)
        vis->refresh();

    bpmSlider.setValue (proc.getBpm(), juce::dontSendNotification);
    bpmSlider.updateText();

    masterSlider.setValue (paramValue (proc, "master_vol"), juce::dontSendNotification);
    masterSlider.updateText();

    updateToolbarLook();
    repaint();
}

void KickPage::timerCallback()
{
    if (! isShowing())
        return;

    // Meldungen verschwinden wieder, damit die Zusammenfassung sichtbar wird.
    if (statusText.isNotEmpty()
        && juce::Time::getMillisecondCounterHiRes () - statusSetMs > kStatusTimeoutMs)
    {
        statusText.clear ();
        statusIsError = false;
        repaint (statusArea);
    }

    syncFromProcessor();

    if (loopEnabled)
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (now - lastTriggerMs > 1500.0)
        {
            lastTriggerMs = now;
            proc.triggerKick();
        }
    }
}

// ---------------------------------------------------------------------------
// SlotCard::Listener
// ---------------------------------------------------------------------------
void KickPage::slotCardLoadRequested (int slot)
{
    chooseSampleForSlot (slot);
}

void KickPage::slotCardClearRequested (int slot)
{
    proc.clearSlotSample (slot);
    setStatus ("Slot " + juce::String (slot + 1) + " geleert.");
    syncFromProcessor();
}

void KickPage::slotCardChanged (int slot)
{
    juce::ignoreUnused (slot);

    if (vis != nullptr)
        vis->refresh();
}

void KickPage::chooseSampleForSlot (int slot)
{
    if (! juce::isPositiveAndBelow (slot, KickState::kNumSlots))
        return;

    pendingLoadSlot = slot;

    auto startDirectory = defaultContentRoot();

    if (! startDirectory.isDirectory())
        startDirectory = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Sample fuer Slot " + juce::String (slot + 1),
                                                  startDirectory,
                                                  "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              const auto slotIndex = pendingLoadSlot;
                              pendingLoadSlot = -1;

                              if (file == juce::File() || slotIndex < 0)
                                  return;

                              juce::String error;

                              if (proc.setSlotSample (slotIndex, file, error))
                              {
                                  setStatus ("Slot " + juce::String (slotIndex + 1)
                                             + ": " + file.getFileName());
                                  syncFromProcessor();
                              }
                              else
                              {
                                  setStatus (error, true);
                              }
                          });
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void KickPage::resized()
{
    auto area = getLocalBounds().reduced (8);

    transportArea = area.removeFromTop (kTransportHeight);
    area.removeFromTop (kPageGap);

    visualizerArea = area.removeFromTop (juce::jmin (kVisualizerHeight, area.getHeight() / 3));
    area.removeFromTop (kPageGap);

    statusArea = area.removeFromBottom (kStatusHeight);

    // Transport: links die Knoepfe, rechts BPM und MASTER.
    auto transport = transportArea;

    playButton.setBounds (transport.removeFromLeft (62).reduced (1, 2));
    transport.removeFromLeft (4);
    loopButton.setBounds (transport.removeFromLeft (58).reduced (1, 2));
    transport.removeFromLeft (4);
    demoButton.setBounds (transport.removeFromLeft (104).reduced (1, 2));

    viewButton.setBounds (transport.removeFromRight (104).reduced (1, 2));
    transport.removeFromRight (8);

    auto masterCell = transport.removeFromRight (150);
    masterLabel.setBounds (masterCell.removeFromTop (11));
    masterSlider.setBounds (masterCell.removeFromTop (18));

    transport.removeFromRight (8);

    auto bpmCell = transport.removeFromRight (140);
    bpmLabel.setBounds (bpmCell.removeFromTop (11));
    bpmSlider.setBounds (bpmCell.removeFromTop (18));

    if (vis != nullptr)
        vis->setBounds (visualizerArea);

    viewport.setBounds (area);

    layoutCards();
}

void KickPage::layoutCards()
{
    const auto available = viewport.getWidth() - 12;   // Platz fuer die Scrollbar

    if (available <= 0)
        return;

    const auto cardWidth = juce::jmax (220, (available - (kColumns + 1) * kCardGap) / kColumns);
    const auto rows      = (KickState::kNumSlots + kColumns - 1) / kColumns;
    const auto cardHeight = SlotCard::kPreferredHeight;

    cardHolder.setSize (available, rows * (cardHeight + kCardGap) + kCardGap);

    for (int i = 0; i < (int) cards.size(); ++i)
    {
        const auto row = i / kColumns;
        const auto col = i % kColumns;

        cards[(size_t) i]->setBounds (kCardGap + col * (cardWidth + kCardGap),
                                      kCardGap + row * (cardHeight + kCardGap),
                                      cardWidth, cardHeight);
    }
}

void KickPage::paint (juce::Graphics& g)
{
    g.fillAll (ui::theme::background);

    // Statuszeile: Meldung, sonst die Zusammenfassung des Zustands.
    auto status = statusArea;

    if (statusText.isEmpty())
    {
        const auto filled = proc.getNumFilledSlots();

        juce::String info;
        info << filled << "/12 Slots belegt  |  " << juce::String (proc.getBpm(), 1) << " BPM"
             << "  |  Kick " << juce::String (proc.getKickLengthSeconds() * 1000.0, 0) << " ms"
             << "  |  Latenz " << juce::String (proc.getLatencySamples()) << " Samples";

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim);
        g.drawText (info, status, juce::Justification::centredLeft, true);
        return;
    }

    g.setFont (ui::theme::smallFont());
    g.setColour (statusIsError ? ui::theme::error : ui::theme::textDim);
    g.drawText (statusText, status, juce::Justification::centredLeft, true);
}

} // namespace bfka
