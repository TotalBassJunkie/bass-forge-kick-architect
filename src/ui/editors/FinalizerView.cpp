#include "ui/editors/FinalizerView.h"

#include "fx/modules/FinalizerModule.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    // Masse innerhalb von 190 px (Rahmen und Titelraum zeichnet die Basisklasse).
    constexpr int kTitleHeight  = 18;
    constexpr int kAxisHeight   = 12;   ///< Frequenzbeschriftung unter dem Plot
    constexpr int kDbLabelWidth = 26;   ///< Pegelbeschriftung links vom Plot
    constexpr int kLineHeight   = 12;   ///< Zeilen "LAT ...", "CLIP ...", "UP ..."
    constexpr int kBarCaption   = 11;   ///< Beschriftung ueber den Balken
    constexpr int kClipBoxSize  = 10;

    // Achsen des Sub-Shelf-Plots (Spec: 20 Hz ... 1 kHz, -3 ... +12 dB).
    constexpr double kMinHz = 20.0;
    constexpr double kMaxHz = 1000.0;
    constexpr double kMinDb = -3.0;
    constexpr double kMaxDb = 12.0;

    // Skalen der beiden Balken im Limiter-Bereich.
    constexpr float kMaxUpwardDb    = 12.0f;
    constexpr float kMaxReductionDb = 12.0f;

    /** Breite des Shelf-Plots in Prozent der Inhaltsbreite. */
    constexpr int kPlotPercent = 55;
} // namespace

FinalizerView::FinalizerView (PluginProcessor& processor, int slotIndex)
    : ModuleStructureView (processor, slotIndex)
{
    // Reine Anzeige: keine Kindkomponenten, keine Maus-Interaktion.
}

juce::Rectangle<int> FinalizerView::bodyArea (juce::Rectangle<int> contentArea) const
{
    return contentArea.withTrimmedTop (kTitleHeight)
                      .withTrimmedBottom (4)
                      .withTrimmedLeft (8)
                      .withTrimmedRight (8);
}

juce::Rectangle<int> FinalizerView::plotArea (juce::Rectangle<int> contentArea) const
{
    auto area = bodyArea (contentArea);
    const auto leftWidth = (area.getWidth() * kPlotPercent) / 100;

    return area.removeFromLeft (leftWidth).withTrimmedRight (6);
}

juce::Rectangle<int> FinalizerView::limiterArea (juce::Rectangle<int> contentArea) const
{
    auto area = bodyArea (contentArea);
    const auto leftWidth = (area.getWidth() * kPlotPercent) / 100;

    area.removeFromLeft (leftWidth);

    return area.withTrimmedLeft (6);
}

float FinalizerView::frequencyToX (double hz, juce::Rectangle<int> area) const
{
    // freqToNorm() klemmt selbst auf [20 Hz, 1 kHz]; nie Division durch 0.
    return (float) area.getX() + ui::theme::freqToNorm (hz, kMinHz, kMaxHz) * (float) area.getWidth();
}

float FinalizerView::levelToY (float db, juce::Rectangle<int> area) const
{
    // kMaxDb != kMinDb, der Nenner ist damit konstant 15 dB.
    const auto clamped = (double) juce::jlimit ((float) kMinDb, (float) kMaxDb, db);
    const auto value01 = (clamped - kMinDb) / (kMaxDb - kMinDb);

    return (float) area.getBottom() - (float) value01 * (float) area.getHeight();
}

void FinalizerView::refresh()
{
    // Werte werden direkt beim Zeichnen gelesen; hier genuegt das Neuzeichnen.
    repaint();
}

void FinalizerView::paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea)
{
    // Leerer oder falscher Kettenplatz: es bleibt beim Rahmen der Basisklasse.
    if (proc.getModule (moduleSlot) == nullptr || moduleAs<FinalizerModule>() == nullptr)
        return;

    paintTitle (g, contentArea, "SUB SHELF");

    paintShelf (g, plotArea (contentArea));
    paintLimiter (g, limiterArea (contentArea));
}

// ---------------------------------------------------------------------------
// Sub-Shelf-Kurve und Mono-Marker
// ---------------------------------------------------------------------------
void FinalizerView::paintShelf (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (area.getWidth() < 16 || area.getHeight() < 24)
        return;

    ui::theme::paintPanel (g, area.toFloat().expanded (1.0f), ui::theme::panelAlt, 4.0f);

    const auto inner = area.withTrimmedLeft (kDbLabelWidth).withTrimmedBottom (kAxisHeight);

    if (inner.getWidth() < 8 || inner.getHeight() < 24)
        return;

    const auto boostDb = juce::jlimit (0.0f, 12.0f, getParamValue ("boost"));
    const auto shelfHz = juce::jlimit (20.0f, 200.0f, getParamValue ("freq"));
    const auto monoHz  = juce::jlimit (60.0f, 500.0f, getParamValue ("monoF"));
    const auto shelfOn = getParamValue ("shelfOn") >= 0.5f;
    const auto monoOn  = getParamValue ("monoOn") >= 0.5f;

    // --- Frequenzachse (20 Hz ... 1 kHz, logarithmisch) -------------------
    for (const double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0 })
    {
        const auto x = frequencyToX (hz, inner);

        g.setColour (ui::theme::grid.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, (float) inner.getY(), (float) inner.getBottom());

        auto box = juce::Rectangle<int> ((int) x - 12, inner.getBottom() + 1, 24, kAxisHeight - 1);
        box.setX (juce::jlimit (area.getX() + 1,
                                juce::jmax (area.getX() + 1, area.getRight() - box.getWidth() - 1),
                                box.getX()));

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim.withAlpha (0.85f));
        g.drawText (hz >= 1000.0 ? juce::String ("1k") : juce::String (juce::roundToInt (hz)),
                    box, juce::Justification::centred, false);
    }

    // --- Pegelachse (-3 ... +12 dB) mit hervorgehobener 0-dB-Linie --------
    for (const double db : { 12.0, 0.0, -3.0 })
    {
        const auto y = levelToY ((float) db, inner);
        const auto isZero = std::abs (db) < 0.5;

        g.setColour (isZero ? ui::theme::grid : ui::theme::grid.withAlpha (0.35f));
        g.drawHorizontalLine ((int) y, (float) inner.getX(), (float) inner.getRight());

        const auto label = isZero ? juce::String ("0")
                                  : juce::String (db > 0.0 ? "+" : "-")
                                        + juce::String (juce::roundToInt (std::abs (db)));

        auto labelBox = juce::Rectangle<int> (area.getX() + 2, (int) y - 6, kDbLabelWidth - 4, 12);
        labelBox.setY (juce::jlimit (area.getY() + 1,
                                     juce::jmax (area.getY() + 1,
                                                 area.getBottom() - labelBox.getHeight() - 1),
                                     labelBox.getY()));

        g.setFont (ui::theme::smallFont());
        g.setColour (isZero ? ui::theme::text : ui::theme::textDim.withAlpha (0.8f));
        g.drawText (label, labelBox, juce::Justification::centredRight, false);
    }

    // --- Kurve: g(f) = boost / (1 + (f/freq)^2), monoton fallend ----------
    // shelfHz ist auf >= 20 Hz geklemmt, boost auf >= 0 dB -> keine
    // Division durch 0 und kein Sprung.
    const auto levelFor = [&] (double hz) -> float
    {
        if (! shelfOn)
            return 0.0f;

        const auto ratio = hz / (double) shelfHz;

        return (float) ((double) boostDb / (1.0 + ratio * ratio));
    };

    const auto steps = juce::jlimit (2, 512, inner.getWidth());

    juce::Path path;
    float prevX = (float) inner.getX();
    float prevY = 0.0f;

    g.setColour (shelfOn ? ui::theme::accent : ui::theme::accent.withAlpha (0.35f));

    for (int i = 0; i < steps; ++i)
    {
        const auto t  = (float) i / (float) (steps - 1);
        const auto hz = ui::theme::normToFreq (t, kMinHz, kMaxHz);
        const auto x  = (float) inner.getX() + t * (float) inner.getWidth();
        const auto y  = levelToY (levelFor (hz), inner);

        if (i == 0)
        {
            path.startNewSubPath (x, y);
        }
        else if (shelfOn)
        {
            path.lineTo (x, y);
        }
        else if (((i / 3) % 2) == 0)
        {
            // Aus: flach und gestrichelt (3 px an, 3 px aus).
            g.drawLine (prevX, prevY, x, y, 1.2f);
        }

        prevX = x;
        prevY = y;
    }

    if (shelfOn)
        g.strokePath (path, juce::PathStrokeType (1.6f));

    // --- Mono-Bass-Marker ------------------------------------------------
    const auto monoX = frequencyToX ((double) monoHz, inner);
    const auto monoColour = monoOn ? ui::theme::bandLow : ui::theme::bandLow.withAlpha (0.35f);

    g.setColour (monoColour);
    g.drawVerticalLine ((int) monoX, (float) inner.getY(), (float) inner.getBottom());

    const auto monoText = juce::String ("MONO ") + juce::String (juce::roundToInt (monoHz)) + " Hz";

    auto monoBox = juce::Rectangle<int> ((int) monoX + 4, inner.getY() + 2, 78, 11);

    if (monoBox.getRight() > inner.getRight() - 2)
        monoBox.setX ((int) monoX - 4 - monoBox.getWidth());

    monoBox.setX (juce::jlimit (inner.getX() + 1,
                                juce::jmax (inner.getX() + 1, inner.getRight() - monoBox.getWidth()),
                                monoBox.getX()));

    g.setFont (ui::theme::smallFont());
    g.setColour (monoColour);
    g.drawText (monoText, monoBox, juce::Justification::centredLeft, false);
}

// ---------------------------------------------------------------------------
// Limiter/Clip-Bereich
// ---------------------------------------------------------------------------
void FinalizerView::paintLimiter (juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (area.getWidth() < 24 || area.getHeight() < 48)
        return;

    ui::theme::paintPanel (g, area.toFloat().expanded (1.0f), ui::theme::panelAlt, 4.0f);

    const auto* module = moduleAs<FinalizerModule>();

    if (module == nullptr)
        return;

    // Diagnose in Anzeige-Einheiten (atomar, ohne Sperre).
    const auto upwardDb    = juce::jlimit (0.0f, kMaxUpwardDb, module->currentUpwardGainDb());
    const auto limiterGain = juce::jlimit (0.0f, 1.0f, module->currentLimiterGain());

    // 1 - gain ist der lineare Rest; als Pegel sind das -20*log10(gain) dB.
    const auto reductionDb = juce::jlimit (0.0f, kMaxReductionDb,
                                           -juce::Decibels::gainToDecibels (limiterGain, -60.0f));

    auto block = area.reduced (2);

    if (block.getWidth() < 12 || block.getHeight() < 4 * kLineHeight)
        return;

    auto clipRow = block.removeFromBottom (kLineHeight);
    auto lines   = block.removeFromBottom (3 * kLineHeight);
    block.removeFromBottom (2);

    // --- zwei Balken ------------------------------------------------------
    const auto gap = 6;
    const auto barWidth = juce::jmax (6, (block.getWidth() - gap) / 2);

    auto upColumn = block.removeFromLeft (barWidth);
    block.removeFromLeft (gap);
    auto limColumn = block.removeFromLeft (barWidth);

    const auto drawBar = [&g] (juce::Rectangle<int> column, float value01,
                               juce::Colour colour, const juce::String& caption)
    {
        if (column.getWidth() < 4 || column.getHeight() < kBarCaption + 4)
            return;

        auto captionArea = column.removeFromTop (kBarCaption);

        g.setFont (ui::theme::smallFont());
        g.setColour (ui::theme::textDim);
        g.drawText (caption, captionArea, juce::Justification::centred, false);

        g.setColour (ui::theme::panelDeep);
        g.fillRoundedRectangle (column.toFloat(), 2.0f);

        const auto norm   = juce::jlimit (0.0f, 1.0f, value01);
        const auto filled = (float) column.getHeight() * norm;

        if (filled > 1.5f)
        {
            const juce::Rectangle<float> bar ((float) column.getX() + 1.0f,
                                             (float) column.getBottom() - filled,
                                             (float) column.getWidth() - 2.0f,
                                             filled - 1.0f);

            g.setColour (colour);
            g.fillRoundedRectangle (bar, 1.5f);
        }

        g.setColour (ui::theme::grid);
        g.drawRoundedRectangle (column.toFloat(), 2.0f, 1.0f);
    };

    drawBar (upColumn, upwardDb / kMaxUpwardDb, ui::theme::bandMid, "UP");
    drawBar (limColumn, reductionDb / kMaxReductionDb, ui::theme::bandHigh, "RED");

    // --- Clip-Indikator ---------------------------------------------------
    const auto clipping = limiterGain < 0.999f;
    const auto boxSize  = juce::jmin (kClipBoxSize, juce::jmax (4, clipRow.getHeight() - 2));

    const juce::Rectangle<int> box (clipRow.getX(),
                                    clipRow.getY() + (clipRow.getHeight() - boxSize) / 2,
                                    boxSize, boxSize);

    g.setColour (clipping ? ui::theme::error : ui::theme::panelDeep);
    g.fillRect (box);
    g.setColour (clipping ? ui::theme::error : ui::theme::grid);
    g.drawRect (box, 1);

    g.setFont (ui::theme::smallFont());
    g.setColour (clipping ? ui::theme::error : ui::theme::textDim);

    const auto labelX = box.getRight() + 4;
    const auto labelW = juce::jmax (0, clipRow.getRight() - labelX);

    g.drawText ("LIM", juce::Rectangle<int> (labelX, clipRow.getY(), labelW, clipRow.getHeight()),
                juce::Justification::centredLeft, false);

    // --- Zeilen LAT / CLIP / UP -------------------------------------------
    const auto clipPercent = getParamValue ("clip");

    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);

    const auto drawRow = [&g] (juce::Rectangle<int> row, const juce::String& text)
    {
        g.drawText (text, row, juce::Justification::centredLeft, false);
    };

    drawRow (lines.removeFromTop (kLineHeight),
             juce::String ("LAT ") + juce::String (module->latencySamples()) + " smp");
    drawRow (lines.removeFromTop (kLineHeight),
             juce::String ("CLIP ") + juce::String (juce::roundToInt (clipPercent)) + " %");
    drawRow (lines.removeFromTop (kLineHeight),
             juce::String ("UP ") + juce::String (upwardDb, 1) + " dB");
}

} // namespace bfka
