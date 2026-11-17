#include "ui/editors/MidSideEqView.h"

#include <cmath>

#include "fx/modules/MidSideEqModule.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    // --- Anzeigebereiche -------------------------------------------------------
    constexpr double kMinHz    = 20.0;
    constexpr double kMaxHz    = 20000.0;
    constexpr double kMinDb    = -18.0;
    constexpr double kMaxDb    = 18.0;
    constexpr double kMinQ     = 0.1;
    constexpr double kMaxQ     = 10.0;
    constexpr double kDefaultQ = 0.9;

    constexpr int kTitleHeight       = 18;
    constexpr int kRowHeight         = 40;
    constexpr int kBottomPadding     = 8;
    constexpr int kControlHeight     = 24;
    constexpr int kWidthCaptionWidth = 42;
    constexpr int kWideRowWidth      = 380;
    constexpr int kNodeRadius        = 7;
    constexpr int kHitTolerance      = 8;
    constexpr int kNumKinds          = 6;
    constexpr int kSidePadding       = 8;

    // --- Abbildungen ----------------------------------------------------------

    float xForFrequency (double hz, juce::Rectangle<int> area)
    {
        return (float) area.getX()
               + ui::theme::freqToNorm (hz, kMinHz, kMaxHz) * (float) area.getWidth();
    }

    double frequencyForX (float x, juce::Rectangle<int> area)
    {
        const auto norm = juce::jlimit (0.0f, 1.0f,
                                        (x - (float) area.getX())
                                            / (float) juce::jmax (1, area.getWidth()));

        return ui::theme::normToFreq (norm, kMinHz, kMaxHz);
    }

    float yForGain (double gainDb, juce::Rectangle<int> area)
    {
        const auto norm = (float) ((juce::jlimit (kMinDb, kMaxDb, gainDb) - kMinDb)
                                   / (kMaxDb - kMinDb));

        return (float) area.getBottom() - norm * (float) area.getHeight();
    }

    double gainForY (float y, juce::Rectangle<int> area)
    {
        const auto norm = juce::jlimit (0.0f, 1.0f,
                                        ((float) area.getBottom() - y)
                                            / (float) juce::jmax (1, area.getHeight()));

        return kMinDb + (double) norm * (kMaxDb - kMinDb);
    }

    juce::String formatHz (double hz)
    {
        return hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0);
    }

    // --- Zustands-Lesen -------------------------------------------------------

    /** Zahlen-Attribut lesen; XML-Zustaende tragen Zahlen oft als String. */
    double readDoubleProperty (const juce::ValueTree& tree, const char* name, double fallback)
    {
        const auto value = tree.getProperty (juce::Identifier (name));

        if (value.isVoid() || value.isUndefined())
            return fallback;

        if (value.isDouble() || value.isInt() || value.isInt64() || value.isBool())
            return (double) value;

        if (value.isString())
            return value.toString().getDoubleValue();

        return fallback;
    }

    int readIntProperty (const juce::ValueTree& tree, const char* name, int fallback)
    {
        return (int) std::lround (readDoubleProperty (tree, name, (double) fallback));
    }

    // --- Zeichnen -------------------------------------------------------------

    /** Kurze Formmarkierung im Knoten, damit die Bandtypen unterscheidbar sind. */
    void drawKindMarker (juce::Graphics& g, int kind, float x, float y)
    {
        juce::Path path;

        switch (kind)
        {
            case 1:   // Low-Shelf: Ecke oben links
                path.startNewSubPath (x - 4.0f, y - 3.5f);
                path.lineTo (x - 0.5f, y - 3.5f);
                path.lineTo (x - 0.5f, y + 3.5f);
                path.lineTo (x + 4.0f, y + 3.5f);
                break;

            case 2:   // High-Shelf: Ecke oben rechts
                path.startNewSubPath (x - 4.0f, y + 3.5f);
                path.lineTo (x + 0.5f, y + 3.5f);
                path.lineTo (x + 0.5f, y - 3.5f);
                path.lineTo (x + 4.0f, y - 3.5f);
                break;

            case 3:   // Low-Cut (Hochpass): Schraege nach rechts oben
                path.startNewSubPath (x - 3.5f, y + 3.5f);
                path.lineTo (x + 3.5f, y - 3.5f);
                break;

            case 4:   // High-Cut (Tiefpass): Schraege nach rechts unten
                path.startNewSubPath (x - 3.5f, y - 3.5f);
                path.lineTo (x + 3.5f, y + 3.5f);
                break;

            case 5:   // Notch: Kerbe
                path.startNewSubPath (x - 4.0f, y + 3.0f);
                path.lineTo (x, y - 3.5f);
                path.lineTo (x + 4.0f, y + 3.0f);
                break;

            case 0:   // Bell: Waagerechte
            default:
                path.startNewSubPath (x - 4.0f, y);
                path.lineTo (x + 4.0f, y);
                break;
        }

        g.strokePath (path, juce::PathStrokeType (1.4f));
    }
} // namespace

// =========================================================================
// Konstruktion
// =========================================================================
MidSideEqView::MidSideEqView (PluginProcessor& processor, int slotIndex)
    : ModuleStructureView (processor, slotIndex)
{
    widthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    widthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, kControlHeight - 4);
    widthSlider.setRange (0.0, 200.0, 0.5);
    widthSlider.setValue (100.0, juce::dontSendNotification);
    widthSlider.setNumDecimalPlacesToDisplay (0);
    widthSlider.setTextValueSuffix (" %");
    widthSlider.setColour (juce::Slider::backgroundColourId, ui::theme::panelDeep);
    widthSlider.setColour (juce::Slider::trackColourId, ui::theme::accent.withAlpha (0.65f));
    widthSlider.setColour (juce::Slider::thumbColourId, ui::theme::accent);
    widthSlider.setColour (juce::Slider::textBoxTextColourId, ui::theme::text);
    widthSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    widthSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    widthSlider.onValueChange = [this] { setParamValue ("width", (float) widthSlider.getValue()); };
    addAndMakeVisible (widthSlider);

    const auto setupToggle = [this] (juce::TextButton& button, const juce::String& paramId)
    {
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
        button.setColour (juce::TextButton::buttonOnColourId, ui::theme::accent.withAlpha (0.9f));
        button.setColour (juce::TextButton::textColourOffId, ui::theme::textDim);
        button.setColour (juce::TextButton::textColourOnId, ui::theme::panelDeep);
        button.onClick = [this, &button, paramId]
        {
            setParamValue (paramId, button.getToggleState() ? 1.0f : 0.0f);
        };

        addAndMakeVisible (button);
    };

    setupToggle (midSoloButton,  "msolo");
    setupToggle (sideSoloButton, "ssolo");
    setupToggle (midMuteButton,  "mmute");
    setupToggle (sideMuteButton, "smute");

    readBandsFromModule();
    refreshControls();
}

bool MidSideEqView::hasModule() const
{
    return moduleAs<MidSideEqModule>() != nullptr;
}

// =========================================================================
// Layout
// =========================================================================
juce::Rectangle<int> MidSideEqView::graphArea (juce::Rectangle<int> contentArea) const
{
    return contentArea.withTrimmedTop (kTitleHeight)
                      .withTrimmedBottom (kRowHeight + kBottomPadding)
                      .withTrimmedLeft (kSidePadding)
                      .withTrimmedRight (kSidePadding);
}

juce::Rectangle<int> MidSideEqView::rowArea (juce::Rectangle<int> contentArea) const
{
    auto area = contentArea.withTrimmedLeft (kSidePadding)
                           .withTrimmedRight (kSidePadding)
                           .withTrimmedBottom (kBottomPadding);

    return area.removeFromBottom (juce::jmin (kRowHeight, juce::jmax (0, area.getHeight())));
}

int MidSideEqView::widthCaptionWidth (int rowWidth) noexcept
{
    return rowWidth >= kWideRowWidth ? kWidthCaptionWidth : 0;
}

void MidSideEqView::resized()
{
    auto row = rowArea (getLocalBounds().reduced (1));
    auto line = row.withSizeKeepingCentre (row.getWidth(), juce::jmin (kControlHeight, row.getHeight()));

    line.removeFromLeft (widthCaptionWidth (line.getWidth()));

    const auto gap = 6;

    // Der Slider gibt zuerst Platz ab, damit die Schalter lesbar bleiben.
    const auto sliderWidth = juce::jlimit (70, 150, line.getWidth() / 3);
    widthSlider.setBounds (line.removeFromLeft (juce::jmin (sliderWidth, line.getWidth())).reduced (0, 1));

    line.removeFromLeft (8);

    juce::TextButton* toggles[] = { &midSoloButton, &sideSoloButton, &midMuteButton, &sideMuteButton };

    const auto count  = (int) (sizeof (toggles) / sizeof (toggles[0]));
    const auto usable = juce::jmax (0, line.getWidth() - gap * (count - 1));
    const auto base   = usable / count;
    const auto rest   = usable - base * count;

    for (int i = 0; i < count; ++i)
    {
        const auto width = base + (i < rest ? 1 : 0);

        if (width > 0)
            toggles[i]->setBounds (line.removeFromLeft (width)
                                       .withSizeKeepingCentre (width, juce::jmin (kControlHeight,
                                                                                 line.getHeight())));

        if (i + 1 < count)
            line.removeFromLeft (gap);
    }
}

// =========================================================================
// Zustand lesen / schreiben
// =========================================================================
void MidSideEqView::readBandsFromModule()
{
    nodes.clear();

    const auto* module = moduleAs<MidSideEqModule>();

    if (module == nullptr)
        return;

    const auto state = module->getState();
    const auto count = juce::jmin (maxBands, state.getNumChildren());

    for (int i = 0; i < count; ++i)
    {
        const auto child = state.getChild (i);

        if (! child.getType().toString().equalsIgnoreCase ("Band"))
            continue;

        Node node;
        node.kind = juce::jlimit (0, kNumKinds - 1, readIntProperty (child, "kind", 0));
        node.freq = juce::jlimit (kMinHz, kMaxHz, readDoubleProperty (child, "f", 1000.0));
        node.q    = juce::jlimit (kMinQ, kMaxQ, readDoubleProperty (child, "q", kDefaultQ));
        node.gain = juce::jlimit (kMinDb, kMaxDb, readDoubleProperty (child, "g", 0.0));
        node.dyn  = juce::jlimit (0.0, 100.0, readDoubleProperty (child, "dyn", 0.0));
        node.ch   = juce::jlimit (0, 1, readIntProperty (child, "ch", 0));

        nodes.push_back (node);
    }
}

void MidSideEqView::writeBandsToModule()
{
    auto* module = moduleAs<MidSideEqModule>();

    if (module == nullptr)
        return;

    juce::ValueTree tree (juce::Identifier ("Module"));

    for (const auto& node : nodes)
    {
        juce::ValueTree band (juce::Identifier ("Band"));

        band.setProperty (juce::Identifier ("kind"), juce::var (node.kind), nullptr);
        band.setProperty (juce::Identifier ("f"),    juce::var (node.freq), nullptr);
        band.setProperty (juce::Identifier ("q"),    juce::var (node.q),    nullptr);
        band.setProperty (juce::Identifier ("g"),    juce::var (node.gain), nullptr);
        band.setProperty (juce::Identifier ("dyn"),  juce::var (node.dyn),  nullptr);
        band.setProperty (juce::Identifier ("ch"),   juce::var (node.ch),   nullptr);

        tree.appendChild (band, nullptr);
    }

    module->setState (tree);

    // Erst der Writeback bringt die Baender in den Ketten-Zustand (Preset/Snapshot).
    proc.writeBackModuleStates();

    repaint();
}

void MidSideEqView::removeNode (int index)
{
    if (index < 0 || index >= (int) nodes.size())
        return;

    nodes.erase (nodes.begin() + index);

    activeNode = -1;
    dragging   = false;
    dragNode   = -1;

    writeBandsToModule();
}

void MidSideEqView::refreshControls()
{
    const auto width = (double) getParamValue ("width");

    if (std::abs (width - widthSlider.getValue()) > 0.05)
        widthSlider.setValue (width, juce::dontSendNotification);

    const auto syncToggle = [] (juce::TextButton& button, float value)
    {
        const auto shouldBeOn = value > 0.5f;

        if (button.getToggleState() != shouldBeOn)
            button.setToggleState (shouldBeOn, juce::dontSendNotification);
    };

    syncToggle (midSoloButton,  getParamValue ("msolo"));
    syncToggle (sideSoloButton, getParamValue ("ssolo"));
    syncToggle (midMuteButton,  getParamValue ("mmute"));
    syncToggle (sideMuteButton, getParamValue ("smute"));
}

void MidSideEqView::refresh()
{
    if (! hasModule())
    {
        nodes.clear();
        return;
    }

    refreshControls();

    // Waehrend des Ziehens ist der Cache die Wahrheit - nicht neu einlesen.
    if (! dragging)
        readBandsFromModule();

    repaint();
}

// =========================================================================
// Zeichnen
// =========================================================================
void MidSideEqView::paintGraph (juce::Graphics& g, juce::Rectangle<int> area) const
{
    ui::theme::paintPanel (g, area.toFloat().expanded (1.0f), ui::theme::panelAlt, 4.0f);

    // Frequenzraster (50 Hz ... 10 kHz) mit Beschriftung.
    g.setFont (ui::theme::smallFont());

    for (const double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const auto x = xForFrequency (hz, area);

        g.setColour (ui::theme::grid.withAlpha (0.55f));
        g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());

        g.setColour (ui::theme::textDim.withAlpha (0.55f));
        g.drawText (formatHz (hz),
                    juce::Rectangle<int> ((int) x - 12, area.getBottom() - 12, 24, 11),
                    juce::Justification::centred, false);
    }

    // Gain-Raster und 0-dB-Linie.
    for (const double db : { -12.0, -6.0, 0.0, 6.0, 12.0 })
    {
        const auto y      = yForGain (db, area);
        const auto isZero = std::abs (db) < 1.0e-9;

        g.setColour (isZero ? ui::theme::grid.brighter (0.35f) : ui::theme::grid.withAlpha (0.4f));
        g.drawHorizontalLine ((int) y, (float) area.getX(), (float) area.getRight());
    }

    // Bandknoten: Mid in bandLow, Side in bandHigh.
    int perChannel[2] = { 0, 0 };

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& node    = nodes[i];
        const auto  channel = node.ch == 1 ? 1 : 0;

        ++perChannel[channel];

        const auto x           = xForFrequency (node.freq, area);
        const auto y           = yForGain (node.gain, area);
        const auto colour      = channel == 1 ? ui::theme::bandHigh : ui::theme::bandLow;
        const auto highlighted = dragging ? (dragNode == (int) i) : (activeNode == (int) i);
        const auto radius      = (float) kNodeRadius;

        g.setColour (colour.withAlpha (0.9f));
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour (highlighted ? ui::theme::accent : ui::theme::panelDeep);
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f,
                       highlighted ? 2.0f : 1.2f);

        // Formmarkierung je `kind`.
        g.setColour (ui::theme::panelDeep);
        drawKindMarker (g, node.kind, x, y);

        // "M1"/"S2" und der dB-Wert daneben, innerhalb des Plots gehalten.
        const auto label      = juce::String (channel == 1 ? "S" : "M")
                                    + juce::String (perChannel[channel]);
        const auto labelX     = juce::jlimit (area.getX() + 2,
                                              juce::jmax (area.getX() + 2, area.getRight() - 54),
                                              (int) x + kNodeRadius + 3);
        const auto labelY     = juce::jlimit (area.getY() + 2,
                                              juce::jmax (area.getY() + 2, area.getBottom() - 26),
                                              (int) y - 13);

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::text);
        g.drawText (label, juce::Rectangle<int> (labelX, labelY, 48, 11),
                    juce::Justification::left, false);

        g.setColour (ui::theme::textDim);
        g.drawText (juce::String (node.gain, 1) + " dB",
                    juce::Rectangle<int> (labelX, labelY + 10, 52, 11),
                    juce::Justification::left, false);
    }
}

void MidSideEqView::paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea)
{
    // Leerer Platz oder falscher Modultyp: nur der Rahmen der Basisklasse.
    if (! hasModule())
        return;

    paintTitle (g, contentArea, "MID/SIDE");

    const auto graph = graphArea (contentArea);

    if (graph.getWidth() > 16 && graph.getHeight() > 16)
        paintGraph (g, graph);

    auto       row     = rowArea (contentArea);
    const auto caption = widthCaptionWidth (row.getWidth());

    if (caption > 0)
    {
        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim);
        g.drawText ("WIDTH", row.removeFromLeft (caption),
                    juce::Justification::centredLeft, false);
    }
}

// =========================================================================
// Maus
// =========================================================================
int MidSideEqView::nodeAt (juce::Point<int> position) const
{
    const auto graph = graphArea (getLocalBounds().reduced (1));

    if (! graph.expanded (kNodeRadius + 4, kNodeRadius + 4).contains (position))
        return -1;

    int  best         = -1;
    auto bestDistance = (float) (kNodeRadius + kHitTolerance);

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto x        = xForFrequency (nodes[i].freq, graph);
        const auto y        = yForGain (nodes[i].gain, graph);
        const auto distance = juce::jmax (std::abs ((float) position.x - x),
                                          std::abs ((float) position.y - y));

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best         = (int) i;
        }
    }

    return best;
}

void MidSideEqView::mouseDown (const juce::MouseEvent& event)
{
    if (! hasModule())
        return;

    const auto index = nodeAt (event.getPosition());

    if (index < 0)
    {
        activeNode = -1;
        return;
    }

    activeNode = index;

    // Rechtsklick: Bandtyp zyklisch weiterschalten (Bell ... Notch).
    if (event.mods.isRightButtonDown())
    {
        auto& node = nodes[(size_t) index];
        node.kind = (node.kind + 1) % kNumKinds;
        writeBandsToModule();
        return;
    }

    // Strg (bzw. Cmd) + Klick: Band loeschen.
    if (event.mods.isCommandDown() || event.mods.isCtrlDown())
    {
        removeNode (index);
        return;
    }

    dragging = true;
    dragNode = index;
}

void MidSideEqView::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging || dragNode < 0 || dragNode >= (int) nodes.size() || ! hasModule())
        return;

    const auto graph = graphArea (getLocalBounds().reduced (1));

    if (graph.getWidth() <= 0 || graph.getHeight() <= 0)
        return;

    auto& node = nodes[(size_t) dragNode];

    node.freq = juce::jlimit (kMinHz, kMaxHz, frequencyForX ((float) event.getPosition().x, graph));
    node.gain = juce::jlimit (kMinDb, kMaxDb, gainForY ((float) event.getPosition().y, graph));

    writeBandsToModule();
}

void MidSideEqView::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    dragNode = -1;
}

void MidSideEqView::mouseDoubleClick (const juce::MouseEvent& event)
{
    // Ein Doppelklick darf kein Ziehen offen lassen.
    dragging = false;
    dragNode = -1;

    // Rechts-Doppelklick ist kein Loeschen (Rechtsklick schaltet den Bandtyp).
    if (! hasModule() || event.mods.isRightButtonDown())
        return;

    const auto index = nodeAt (event.getPosition());

    // Doppelklick auf einem Knoten: Band loeschen.
    if (index >= 0)
    {
        removeNode (index);
        return;
    }

    const auto graph = graphArea (getLocalBounds().reduced (1));

    if (! graph.contains (event.getPosition()) || (int) nodes.size() >= maxBands)
        return;

    // Doppelklick auf freier Flaeche: neues Bell-Band anlegen.
    Node node;
    node.kind = 0;
    node.freq = juce::jlimit (kMinHz, kMaxHz, frequencyForX ((float) event.getPosition().x, graph));
    node.gain = juce::jlimit (kMinDb, kMaxDb, gainForY ((float) event.getPosition().y, graph));
    node.q    = kDefaultQ;
    node.dyn  = 0.0;
    node.ch   = (float) event.getPosition().y < (float) graph.getCentreY() ? 1 : 0;

    nodes.push_back (node);
    activeNode = (int) nodes.size() - 1;

    writeBandsToModule();
}

void MidSideEqView::mouseMove (const juce::MouseEvent& event)
{
    const auto onNode = hasModule() && nodeAt (event.getPosition()) >= 0;

    setMouseCursor (onNode ? juce::MouseCursor::DraggingHandCursor
                           : juce::MouseCursor::NormalCursor);
}

void MidSideEqView::mouseWheelMove (const juce::MouseEvent& event,
                                    const juce::MouseWheelDetails& wheel)
{
    if (! hasModule())
        return;

    const auto index = nodeAt (event.getPosition());

    if (index < 0)
    {
        ModuleStructureView::mouseWheelMove (event, wheel);
        return;
    }

    const auto factor = wheel.deltaY > 0.0f ? 1.15 : (1.0 / 1.15);

    auto& node = nodes[(size_t) index];
    node.q = juce::jlimit (kMinQ, kMaxQ, node.q * factor);

    activeNode = index;

    writeBandsToModule();
}

} // namespace bfka
