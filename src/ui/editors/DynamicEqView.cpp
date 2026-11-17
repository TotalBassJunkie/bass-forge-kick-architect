#include "ui/editors/DynamicEqView.h"

#include "fx/modules/DynamicEqModule.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr double kMinHz  = 10.0;
    constexpr double kMaxHz  = 20000.0;
    constexpr double kMaxDb  = 18.0;
    constexpr int    kTitleHeight  = 18;
    constexpr int    kFooterHeight = 14;
    constexpr int    kNodeRadius   = 7;
    constexpr int    kHandleTolerance = 10;
} // namespace

DynamicEqView::DynamicEqView (PluginProcessor& processor, int slotIndex)
    : ModuleStructureView (processor, slotIndex)
{
    refresh();
}

juce::String DynamicEqView::paramId (int band, const char* suffix)
{
    return "b" + juce::String (band + 1) + "_" + suffix;
}

juce::Rectangle<int> DynamicEqView::graphArea (juce::Rectangle<int> contentArea) const
{
    return contentArea.withTrimmedTop (kTitleHeight)
                      .withTrimmedBottom (kFooterHeight + 4)
                      .withTrimmedLeft (8).withTrimmedRight (8);
}

float DynamicEqView::xForFrequency (double hz, juce::Rectangle<int> area) const
{
    return (float) area.getX() + ui::theme::freqToNorm (hz, kMinHz, kMaxHz) * (float) area.getWidth();
}

double DynamicEqView::frequencyForX (float x, juce::Rectangle<int> area) const
{
    const auto norm = juce::jlimit (0.0f, 1.0f,
                                    (x - (float) area.getX()) / (float) juce::jmax (1, area.getWidth()));
    return ui::theme::normToFreq (norm, kMinHz, kMaxHz);
}

float DynamicEqView::yForGain (double gainDb, juce::Rectangle<int> area) const
{
    const auto norm = (float) ((juce::jlimit (-kMaxDb, kMaxDb, gainDb) + kMaxDb) / (2.0 * kMaxDb));
    return (float) area.getBottom() - norm * (float) area.getHeight();
}

double DynamicEqView::gainForY (float y, juce::Rectangle<int> area) const
{
    const auto norm = juce::jlimit (0.0f, 1.0f,
                                    ((float) area.getBottom() - y) / (float) juce::jmax (1, area.getHeight()));
    return (double) norm * 2.0 * kMaxDb - kMaxDb;
}

void DynamicEqView::refresh()
{
    bool changed = false;

    for (int band = 0; band < numBands; ++band)
    {
        auto& state = bands[band];

        const auto freq = getParamValue (paramId (band, "freq"));
        const auto gain = getParamValue (paramId (band, "gain"));
        const auto q    = getParamValue (paramId (band, "q"));
        const auto type = (int) getParamValue (paramId (band, "type"));
        const auto active = (int) getParamValue (paramId (band, "active"));

        if (std::abs (freq - (float) state.freqHz) > 0.5f
            || std::abs (gain - (float) state.gainDb) > 0.01f
            || std::abs (q - (float) state.q) > 1.0e-4f
            || type != state.type
            || active != state.active)
        {
            state.freqHz = freq;
            state.gainDb = gain;
            state.q      = q;
            state.type   = type;
            state.active = active;
            changed = true;
        }

        if (const auto* module = moduleAs<DynamicEqModule>())
        {
            const auto gr = module->bandDynamicGainDb (band);

            if (std::abs (gr - state.grDb) > 0.05f)
            {
                state.grDb = gr;
                changed = true;
            }
        }
    }

    const auto hp = (double) getParamValue ("hpFreq");
    const auto lp = (double) getParamValue ("lpFreq");

    if (std::abs (hp - highPassHz) > 0.5 || std::abs (lp - lowPassHz) > 0.5)
    {
        highPassHz = hp;
        lowPassHz  = lp;
        changed = true;
    }

    if (changed)
        repaint();
}

void DynamicEqView::paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea)
{
    paintTitle (g, contentArea, "DYNAMIC EQ - Knoten ziehen, Rad = Q, Rechtsklick = Typ");

    const auto graph = graphArea (contentArea);

    if (graph.getWidth() <= 0 || graph.getHeight() <= 0)
        return;

    ui::theme::paintPanel (g, graph.toFloat().expanded (1.0f), ui::theme::panelAlt, 4.0f);

    // Frequenzraster
    g.setFont (ui::theme::smallFont());

    for (const double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const auto x = xForFrequency (hz, graph);

        g.setColour (ui::theme::grid.withAlpha (0.55f));
        g.drawVerticalLine ((int) x, (float) graph.getY(), (float) graph.getBottom());

        g.setColour (ui::theme::textDim.withAlpha (0.55f));
        g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0),
                    juce::Rectangle<int> ((int) x - 10, graph.getBottom() - 11, 20, 11),
                    juce::Justification::centred, false);
    }

    // Gain-Raster und Nulllinie
    for (const double db : { -12.0, -6.0, 0.0, 6.0, 12.0 })
    {
        const auto y = yForGain (db, graph);

        g.setColour (db == 0.0 ? ui::theme::grid : ui::theme::grid.withAlpha (0.45f));
        g.drawHorizontalLine ((int) y, (float) graph.getX(), (float) graph.getRight());

        g.setColour (ui::theme::textDim.withAlpha (0.5f));
        g.drawText (juce::String (db, 0), juce::Rectangle<int> (graph.getX() + 1, (int) y - 10, 24, 10),
                    juce::Justification::left, false);
    }

    // HP/LP-Kanten
    const auto drawEdge = [&] (double hz, juce::Colour colour, const juce::String& label)
    {
        const auto x = xForFrequency (hz, graph);

        g.setColour (colour.withAlpha (0.75f));
        g.drawVerticalLine ((int) x, (float) graph.getY(), (float) graph.getBottom());

        g.setColour (colour);
        g.fillEllipse (x - 4.0f, (float) graph.getBottom() - 8.0f, 8.0f, 8.0f);

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim);

        const auto labelX = juce::jlimit (graph.getX(), juce::jmax (graph.getX(), graph.getRight() - 48),
                                          (int) x - 24);
        g.drawText (label, juce::Rectangle<int> (labelX, graph.getY() + 1, 48, 11),
                    juce::Justification::centred, false);
    };

    drawEdge (highPassHz, ui::theme::bandLow, "HP " + juce::String (highPassHz, 0));
    drawEdge (lowPassHz, ui::theme::bandHigh, "LP " + juce::String (lowPassHz, 0));

    // Baender
    for (int band = 0; band < numBands; ++band)
    {
        const auto& state = bands[band];
        const auto x = xForFrequency (state.freqHz, graph);
        const auto y = yForGain (state.gainDb, graph);

        const auto colour = state.active == 0 ? ui::theme::textDim
                                              : (band == selectedBand ? ui::theme::accent
                                                                      : ui::theme::ok);

        g.setColour (colour.withAlpha (state.active == 0 ? 0.5f : 0.85f));
        g.fillEllipse (x - (float) kNodeRadius, y - (float) kNodeRadius,
                       (float) (2 * kNodeRadius), (float) (2 * kNodeRadius));

        g.setColour (ui::theme::panelDeep);
        g.drawEllipse (x - (float) kNodeRadius, y - (float) kNodeRadius,
                       (float) (2 * kNodeRadius), (float) (2 * kNodeRadius), 1.0f);

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim);
        g.drawText (juce::String (band + 1),
                    juce::Rectangle<int> ((int) x - 6, (int) y - 14, 12, 11),
                    juce::Justification::centred, false);

        // Dynamischer Regelweg als kleiner Balken unter dem Knoten
        const auto barTop = y + (float) kNodeRadius + 2.0f;
        const auto barHeight = 26.0f;
        const auto amount = juce::jlimit (-24.0f, 24.0f, state.grDb) / 24.0f;

        g.setColour (ui::theme::panelDeep);
        g.fillRect (x - 1.5f, barTop, 3.0f, barHeight);

        g.setColour (ui::theme::accent);
        g.fillRect (x - 1.5f, barTop, 3.0f, barHeight * std::abs (amount));
    }

    // Footer-Hinweis auf die Nova-Abweichung N1
    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim.withAlpha (0.8f));
    g.drawText ("Hinweis: DSP seriell statt parallel (Nova-Abweichung N1)",
                contentArea.removeFromBottom (kFooterHeight),
                juce::Justification::bottomLeft, false);
}

// ---------------------------------------------------------------------------
// Maus
// ---------------------------------------------------------------------------
int DynamicEqView::bandAt (juce::Point<int> position) const
{
    const auto graph = graphArea (getLocalBounds().reduced (1));

    if (! graph.expanded (kNodeRadius + 2, kNodeRadius + 2).contains (position))
        return -1;

    int best = -1;
    auto bestDistance = (float) (kNodeRadius + 6);

    for (int band = 0; band < numBands; ++band)
    {
        const auto x = xForFrequency (bands[band].freqHz, graph);
        const auto y = yForGain (bands[band].gainDb, graph);
        const auto distance = juce::jmax (std::abs ((float) position.x - x),
                                          std::abs ((float) position.y - y));

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = band;
        }
    }

    return best;
}

void DynamicEqView::mouseDown (const juce::MouseEvent& event)
{
    const auto graph = graphArea (getLocalBounds().reduced (1));
    const auto band  = bandAt (event.getPosition());

    if (band >= 0)
    {
        selectedBand = band;

        if (event.mods.isRightButtonDown())
        {
            // Rechtsklick zykelt den Bandtyp: Bell -> Low-Shelf -> High-Shelf.
            const auto next = (bands[band].type + 1) % 3;
            setParamValue (paramId (band, "type"), (float) next);
            bands[band].type = next;
            repaint();
            return;
        }

        dragMode = Drag::band;
        dragBand = band;
        mouseDrag (event);
        return;
    }

    if (std::abs ((float) event.getPosition().x - xForFrequency (highPassHz, graph))
        <= (float) kHandleTolerance)
    {
        dragMode = Drag::highPass;
        mouseDrag (event);
        return;
    }

    if (std::abs ((float) event.getPosition().x - xForFrequency (lowPassHz, graph))
        <= (float) kHandleTolerance)
    {
        dragMode = Drag::lowPass;
        mouseDrag (event);
    }
}

void DynamicEqView::mouseDrag (const juce::MouseEvent& event)
{
    const auto graph = graphArea (getLocalBounds().reduced (1));

    if (dragMode == Drag::band && dragBand >= 0)
    {
        const auto hz   = frequencyForX ((float) event.getPosition().x, graph);
        const auto gain = gainForY ((float) event.getPosition().y, graph);

        setParamValue (paramId (dragBand, "freq"), (float) hz);
        setParamValue (paramId (dragBand, "gain"), (float) gain);

        bands[dragBand].freqHz = hz;
        bands[dragBand].gainDb = gain;
        repaint();
    }
    else if (dragMode == Drag::highPass)
    {
        const auto hz = juce::jlimit (10.0, juce::jmax (11.0, lowPassHz * 0.95), frequencyForX ((float) event.getPosition().x, graph));
        setParamValue ("hpFreq", (float) hz);
        highPassHz = hz;
        repaint();
    }
    else if (dragMode == Drag::lowPass)
    {
        const auto hz = juce::jlimit (juce::jmin (19999.0, highPassHz * 1.05), 20000.0, frequencyForX ((float) event.getPosition().x, graph));
        setParamValue ("lpFreq", (float) hz);
        lowPassHz = hz;
        repaint();
    }
}

void DynamicEqView::mouseUp (const juce::MouseEvent&)
{
    dragMode = Drag::none;
    dragBand = -1;
}

void DynamicEqView::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (bandAt (event.getPosition()) >= 0 ? juce::MouseCursor::DraggingHandCursor
                                                      : juce::MouseCursor::NormalCursor);
}

void DynamicEqView::mouseWheelMove (const juce::MouseEvent& event,
                                   const juce::MouseWheelDetails& wheel)
{
    const auto band = bandAt (event.getPosition()) >= 0 ? bandAt (event.getPosition()) : selectedBand;

    if (band < 0)
        return;

    const auto delta = wheel.deltaY > 0.0f ? 1.2 : 0.8;
    const auto value = juce::jlimit (0.1, 12.0, bands[band].q * (double) delta);

    setParamValue (paramId (band, "q"), (float) value);
    bands[band].q = value;
    selectedBand = band;
    repaint();
}

} // namespace bfka
