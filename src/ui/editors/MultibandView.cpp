#include "ui/editors/MultibandView.h"

#include "fx/modules/MultibandModule.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr double kMinHz = 20.0;
    constexpr double kMaxHz = 12000.0;

    constexpr float kLowMinHz  = 20.0f;
    constexpr float kLowMaxHz  = 1000.0f;
    constexpr float kHighMinHz = 500.0f;
    constexpr float kHighMaxHz = 12000.0f;

    constexpr int kMeterWidth = 54;
    constexpr int kTitleHeight = 18;
    constexpr int kHandleTolerance = 10;
} // namespace

MultibandView::MultibandView (PluginProcessor& processor, int slotIndex)
    : ModuleStructureView (processor, slotIndex)
{
    lowXOverHz  = getParamValue ("x1");
    highXOverHz = getParamValue ("x2");

    setMouseCursor (juce::MouseCursor::NormalCursor);
}

juce::Rectangle<int> MultibandView::graphArea (juce::Rectangle<int> contentArea) const
{
    return contentArea.withTrimmedTop (kTitleHeight).withTrimmedBottom (6)
                      .withTrimmedLeft (8).withTrimmedRight (kMeterWidth + 8);
}

juce::Rectangle<int> MultibandView::meterArea (juce::Rectangle<int> contentArea) const
{
    return contentArea.withTrimmedTop (kTitleHeight).withTrimmedBottom (6)
                      .withTrimmedRight (6)
                      .removeFromRight (kMeterWidth);
}

float MultibandView::xForFrequency (float hz, juce::Rectangle<int> area) const
{
    return area.getX() + ui::theme::freqToNorm (hz, kMinHz, kMaxHz) * (float) area.getWidth();
}

float MultibandView::frequencyForX (float x, juce::Rectangle<int> area) const
{
    const auto norm = juce::jlimit (0.0f, 1.0f,
                                    (x - (float) area.getX()) / (float) juce::jmax (1, area.getWidth()));

    return (float) ui::theme::normToFreq (norm, kMinHz, kMaxHz);
}

void MultibandView::refresh()
{
    const auto low  = getParamValue ("x1");
    const auto high = getParamValue ("x2");

    bool changed = std::abs (low - (float) lowXOverHz) > 0.5f
                || std::abs (high - (float) highXOverHz) > 0.5f;

    lowXOverHz  = low;
    highXOverHz = high;

    if (const auto* module = moduleAs<MultibandModule>())
    {
        for (int band = 0; band < MultibandModule::numBands; ++band)
        {
            const auto gain = module->bandGainDb (band);

            if (std::abs (gain - bandGainDb[band]) > 0.05f)
            {
                bandGainDb[band] = gain;
                changed = true;
            }
        }
    }

    if (changed)
        repaint();
}

void MultibandView::paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea)
{
    paintTitle (g, contentArea, "CROSSOVER - x1 / x2 ziehen");

    const auto graph = graphArea (contentArea);
    const auto meter = meterArea (contentArea);

    if (graph.getWidth() <= 0 || graph.getHeight() <= 0)
        return;

    const auto lowX  = xForFrequency ((float) lowXOverHz, graph);
    const auto highX = xForFrequency ((float) highXOverHz, graph);

    // Baender als Flaechen
    const auto fillBand = [&] (float fromX, float toX, juce::Colour colour)
    {
        g.setColour (colour.withAlpha (0.22f));
        g.fillRect (juce::Rectangle<float> (fromX, (float) graph.getY(),
                                            juce::jmax (0.0f, toX - fromX), (float) graph.getHeight()));
    };

    fillBand ((float) graph.getX(), lowX, ui::theme::bandLow);
    fillBand (lowX, highX, ui::theme::bandMid);
    fillBand (highX, (float) graph.getRight(), ui::theme::bandHigh);

    // Frequenzraster
    for (const double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const auto x = xForFrequency ((float) hz, graph);

        g.setColour (ui::theme::grid.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, (float) graph.getY(), (float) graph.getBottom());

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim.withAlpha (0.6f));
        g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0),
                    juce::Rectangle<int> ((int) x - 12, graph.getBottom() - 12, 24, 12),
                    juce::Justification::centred, false);
    }

    // Grenzen
    const auto drawHandle = [&] (float x, juce::Colour colour, const juce::String& label)
    {
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x - 1.5f, (float) graph.getY(), 3.0f, (float) graph.getHeight()));
        g.fillEllipse (x - 6.0f, (float) graph.getY() - 4.0f, 12.0f, 10.0f);

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::text);

        const auto labelX = juce::jlimit (graph.getX(),
                                          juce::jmax (graph.getX(), graph.getRight() - 68),
                                          (int) x - 34);
        g.drawText (label, juce::Rectangle<int> (labelX, graph.getY() + 4, 68, 12),
                    juce::Justification::centred, false);
    };

    drawHandle (lowX, ui::theme::bandLow, juce::String (lowXOverHz, 0) + " Hz");
    drawHandle (highX, ui::theme::bandHigh, juce::String (highXOverHz, 0) + " Hz");

    // Regelweg je Band
    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText ("GR", meter.withHeight (12), juce::Justification::centred, false);

    const auto barArea = meter.withTrimmedTop (14).withTrimmedBottom (12);
    const auto barHeight = (float) barArea.getHeight() / 3.0f;

    for (int band = 0; band < 3; ++band)
    {
        const auto top = (float) barArea.getY() + (float) band * barHeight;
        const auto barRect = juce::Rectangle<float> ((float) barArea.getX(), top + 1.0f,
                                                     (float) barArea.getWidth(), barHeight - 2.0f);

        g.setColour (ui::theme::panelAlt);
        g.fillRoundedRectangle (barRect, 2.0f);

        // 0 dB in der Mitte; negativer Regelweg waechst nach links.
        const auto centre = barRect.getX() + barRect.getWidth() * 0.5f;
        const auto amount = juce::jlimit (-24.0f, 24.0f, bandGainDb[band]) / 24.0f;
        const auto width  = barRect.getWidth() * 0.5f * std::abs (amount);

        g.setColour (bandGainDb[band] < -0.05f ? ui::theme::accent : ui::theme::ok);
        g.fillRect (juce::Rectangle<float> (amount < 0.0f ? centre - width : centre, barRect.getY(),
                                            width, barRect.getHeight()));

        g.setColour (ui::theme::grid);
        g.drawHorizontalLine ((int) centre, barRect.getX(), barRect.getRight());
    }
}

MultibandView::Handle MultibandView::handleAt (juce::Point<int> position) const
{
    const auto graph = graphArea (getLocalBounds().reduced (1));

    if (! graph.expanded (0, 12).contains (position))
        return Handle::none;

    const auto lowX  = xForFrequency ((float) lowXOverHz, graph);
    const auto highX = xForFrequency ((float) highXOverHz, graph);

    const auto distanceLow  = std::abs ((float) position.x - lowX);
    const auto distanceHigh = std::abs ((float) position.x - highX);

    if (juce::jmin (distanceLow, distanceHigh) > (float) kHandleTolerance)
        return Handle::none;

    return distanceLow <= distanceHigh ? Handle::lowXOver : Handle::highXOver;
}

void MultibandView::mouseDown (const juce::MouseEvent& event)
{
    activeHandle = handleAt (event.getPosition());
}

void MultibandView::mouseDrag (const juce::MouseEvent& event)
{
    if (activeHandle == Handle::none)
        return;

    const auto graph = graphArea (getLocalBounds().reduced (1));
    const auto hz    = frequencyForX ((float) event.getPosition().x, graph);

    if (activeHandle == Handle::lowXOver)
    {
        const auto value = juce::jlimit (kLowMinHz,
                                         juce::jmin (kLowMaxHz, (float) highXOverHz * 0.95f),
                                         hz);
        setParamValue ("x1", value);
        lowXOverHz = value;
    }
    else
    {
        const auto value = juce::jlimit (juce::jmax (kHighMinHz, (float) lowXOverHz * 1.05f),
                                         kHighMaxHz, hz);
        setParamValue ("x2", value);
        highXOverHz = value;
    }

    repaint();
}

void MultibandView::mouseUp (const juce::MouseEvent&)
{
    activeHandle = Handle::none;
}

void MultibandView::mouseMove (const juce::MouseEvent& event)
{
    const auto handle = handleAt (event.getPosition());

    setMouseCursor (handle == Handle::none ? juce::MouseCursor::NormalCursor
                                          : juce::MouseCursor::LeftRightResizeCursor);
}

} // namespace bfka
