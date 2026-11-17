#include "ui/editors/KickShaperView.h"

#include <algorithm>
#include <cmath>

#include "fx/modules/KickShaperModule.h"
#include "plugin/PluginProcessor.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr int kTitleHeight  = 18;
    constexpr int kStatusHeight = 14;
    constexpr int kAreaMargin   = 8;

    /** Anzahl der Segmente der gezeichneten Kurve: 128 Segmente = 129 Stuetzstellen. */
    constexpr int kCurveSegments = 128;

    /** Kleinster x-Abstand zweier Knoten; haelt die Reihenfolge beim Ziehen stabil. */
    constexpr double kMinPointGap = 0.002;

    /** Trefferradius um einen Knoten (Pixel). */
    constexpr float kNodeHitRadius = 9.0f;

    /** Knotenfarbe je Segmenttyp (0...3). */
    juce::Colour segmentColourFor (int segmentType)
    {
        switch (juce::jlimit (0, KickShaperModule::kNumSegmentTypes - 1, segmentType))
        {
            case 1:  return ui::theme::bandLow;
            case 2:  return ui::theme::bandHigh;
            case 3:  return ui::theme::error;
            default: break;
        }

        return ui::theme::ok;
    }
} // namespace

KickShaperView::KickShaperView (PluginProcessor& processor, int slotIndex)
    : ModuleStructureView (processor, slotIndex)
{
    readPointsFromModule();
}

// ---------------------------------------------------------------------------
// Geometrie
// ---------------------------------------------------------------------------
juce::Rectangle<int> KickShaperView::envelopeArea (juce::Rectangle<int> contentArea) const
{
    return contentArea.withTrimmedTop (kTitleHeight + 2)
                      .withTrimmedBottom (kStatusHeight + 10)
                      .withTrimmedLeft (kAreaMargin)
                      .withTrimmedRight (kAreaMargin);
}

juce::Rectangle<int> KickShaperView::statusRow (juce::Rectangle<int> contentArea) const
{
    return contentArea.removeFromBottom (kStatusHeight + 6).withTrimmedLeft (kAreaMargin);
}

float KickShaperView::xForPosition (double position, juce::Rectangle<int> area) const
{
    const auto norm = (float) juce::jlimit (0.0, 1.0, position);

    return (float) area.getX() + norm * (float) area.getWidth();
}

float KickShaperView::yForValue (double value, juce::Rectangle<int> area) const
{
    const auto norm = (float) juce::jlimit (0.0, 1.0, value);

    return (float) area.getBottom() - norm * (float) area.getHeight();
}

double KickShaperView::positionForX (float x, juce::Rectangle<int> area) const
{
    const auto norm = juce::jlimit (0.0f, 1.0f,
                                    (x - (float) area.getX()) / (float) juce::jmax (1, area.getWidth()));

    return (double) norm;
}

double KickShaperView::valueForY (float y, juce::Rectangle<int> area) const
{
    const auto norm = juce::jlimit (0.0f, 1.0f,
                                    ((float) area.getBottom() - y) / (float) juce::jmax (1, area.getHeight()));

    return (double) norm;
}

// ---------------------------------------------------------------------------
// Zustand
// ---------------------------------------------------------------------------
void KickShaperView::readPointsFromModule()
{
    const auto* module = moduleAs<KickShaperModule>();

    if (module == nullptr)
    {
        points.clear();
        return;
    }

    points = module->envelopePoints();

    // Ein korrupter Zustand kann nicht-endliche Werte tragen; ohne diese
    // Absicherung waere die Vergleichsfunktion unten keine gueltige
    // Ordnung mehr.
    for (auto& point : points)
    {
        if (! std::isfinite (point.x))
            point.x = 0.0;

        if (! std::isfinite (point.y))
            point.y = 1.0;
    }

    // Der Zustand haelt die Punkte in Schreibreihenfolge; fuer die Anzeige und
    // das Ziehen brauchen wir x-sortiert. `stable_sort` laesst gleiche x in
    // ihrer bisherigen Reihenfolge stehen.
    std::stable_sort (points.begin(), points.end(),
                      [] (const KickShaperModule::Pt& a, const KickShaperModule::Pt& b)
                      { return a.x < b.x; });
}

void KickShaperView::writePointsToModule()
{
    auto* module = moduleAs<KickShaperModule>();

    if (module == nullptr)
        return;

    module->setEnvelopePoints (points);

    // Ohne den Writeback bliebe die Kurve nur im Modul und kaeme nicht in den
    // Ketten-Zustand (und damit auch nicht ins Preset).
    proc.writeBackModuleStates();
}

void KickShaperView::refresh()
{
    // Waehrend des Ziehens ist die lokale Kopie die Wahrheit: ein Neuladen
    // wuerde den Punkt unter der Maus bei jedem Timer-Tick zuruecksetzen.
    if (draggedPoint < 0)
        readPointsFromModule();

    repaint();
}

// ---------------------------------------------------------------------------
// Zeichnen
// ---------------------------------------------------------------------------
void KickShaperView::paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea)
{
    auto* module = moduleAs<KickShaperModule>();

    // Leerer Platz oder falscher Modultyp: nur der Rahmen der Basisklasse.
    if (module == nullptr)
        return;

    paintTitle (g, contentArea, "SHAPER ENVELOPE");

    const auto area = envelopeArea (contentArea);

    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return;

    ui::theme::paintPanel (g, area.toFloat().expanded (1.0f), ui::theme::panelAlt, 4.0f);

    // Raster bei 0/25/50/75/100 Prozent, horizontal und vertikal.
    g.setColour (ui::theme::grid.withAlpha (0.55f));

    for (int step = 0; step <= 4; ++step)
    {
        const auto fraction = (float) step / 4.0f;

        g.drawVerticalLine ((int) ((float) area.getX() + fraction * (float) area.getWidth()),
                            (float) area.getY(), (float) area.getBottom());
        g.drawHorizontalLine ((int) ((float) area.getY() + fraction * (float) area.getHeight()),
                              (float) area.getX(), (float) area.getRight());
    }

    // Die Kurve, die das Modul tatsaechlich auswertet (129 Stuetzstellen).
    juce::Path curve;

    for (int i = 0; i <= kCurveSegments; ++i)
    {
        const auto position = (double) i / (double) kCurveSegments;
        const auto x = xForPosition (position, area);
        const auto y = yForValue ((double) module->evaluateEnvelope (position), area);

        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    g.setColour (ui::theme::accent);
    g.strokePath (curve, juce::PathStrokeType (1.6f));

    // Abspielmarke der Huellkurven-Uhr.
    const auto playhead = (double) juce::jlimit (0.0f, 1.0f, module->currentEnvelopePosition());

    g.setColour (ui::theme::warn.withAlpha (0.85f));
    g.drawVerticalLine ((int) xForPosition (playhead, area),
                        (float) area.getY(), (float) area.getBottom());

    // Knoten der Huellkurve samt Segmenttyp.
    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto& point = points[(size_t) i];
        const auto x = xForPosition (point.x, area);
        const auto y = yForValue (point.y, area);
        const auto colour = segmentColourFor (point.t);
        const auto radius = i == draggedPoint ? 5.0f : 4.0f;

        g.setColour (colour.withAlpha (0.92f));
        g.fillEllipse (x - radius, y - radius, 2.0f * radius, 2.0f * radius);

        g.setColour (ui::theme::panelDeep);
        g.drawEllipse (x - radius, y - radius, 2.0f * radius, 2.0f * radius, 1.0f);

        g.setFont (ui::theme::smallFont());
        g.setColour (colour);

        const auto labelX = juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - 12),
                                          (int) x + 5);
        const auto labelY = juce::jlimit (area.getY(), juce::jmax (area.getY(), area.getBottom() - 12),
                                          (int) y - 13);

        g.drawText (juce::String (point.t), juce::Rectangle<int> (labelX, labelY, 12, 11),
                    juce::Justification::centredLeft, false);
    }

    // Statuszeile (reine Anzeige, kein Bedienelement).
    const auto envelopeValue = (double) module->evaluateEnvelope (playhead);
    const auto status = "LEN "
                        + juce::String (juce::roundToInt (module->renderLengthSeconds() * 1000.0))
                        + " ms   POS " + juce::String (playhead, 2)
                        + "   ENV " + juce::String (envelopeValue, 2);

    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText (status, statusRow (contentArea), juce::Justification::centredLeft, false);
}

// ---------------------------------------------------------------------------
// Maus
// ---------------------------------------------------------------------------
int KickShaperView::pointAt (juce::Point<int> position) const
{
    const auto area = envelopeArea (getLocalBounds().reduced (1));

    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return -1;

    if (! area.expanded ((int) kNodeHitRadius, (int) kNodeHitRadius).contains (position))
        return -1;

    int  best = -1;
    auto bestDistance = kNodeHitRadius + 1.0f;

    for (int i = 0; i < (int) points.size(); ++i)
    {
        const auto& point = points[(size_t) i];

        const auto distance = juce::jmax (std::abs ((float) position.x - xForPosition (point.x, area)),
                                          std::abs ((float) position.y - yForValue (point.y, area)));

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }

    return best;
}

void KickShaperView::dragPointTo (int index, juce::Point<int> position)
{
    if (index < 0 || index >= (int) points.size())
        return;

    const auto area = envelopeArea (getLocalBounds().reduced (1));
    auto& point = points[(size_t) index];

    // x bleibt zwischen den Nachbarn, damit die Punkte sortiert bleiben und
    // beim Ziehen nicht die Reihenfolge tauschen.
    auto minX = 0.0;
    auto maxX = 1.0;

    if (index > 0)
        minX = points[(size_t) index - 1].x + kMinPointGap;

    if (index + 1 < (int) points.size())
        maxX = points[(size_t) (index + 1)].x - kMinPointGap;

    minX = juce::jlimit (0.0, 1.0, minX);
    maxX = juce::jlimit (0.0, 1.0, maxX);

    if (minX > maxX)
    {
        const auto middle = 0.5 * (minX + maxX);
        minX = middle;
        maxX = middle;
    }

    point.x = juce::jlimit (minX, maxX, positionForX ((float) position.x, area));
    point.y = juce::jlimit (0.0, 1.0, valueForY ((float) position.y, area));
    point.t = juce::jlimit (0, KickShaperModule::kNumSegmentTypes - 1, point.t);

    writePointsToModule();
    repaint();
}

void KickShaperView::mouseDown (const juce::MouseEvent& event)
{
    if (moduleAs<KickShaperModule>() == nullptr)
        return;

    const auto area = envelopeArea (getLocalBounds().reduced (1));

    if (! area.contains (event.getPosition()))
        return;

    const auto index = pointAt (event.getPosition());

    if (index < 0)
        return;

    if (event.mods.isRightButtonDown())
    {
        // Rechtsklick zykelt den Segmenttyp des Segments ab diesem Punkt.
        auto& point = points[(size_t) index];
        point.t = (point.t + 1) % KickShaperModule::kNumSegmentTypes;
        draggedPoint = -1;
        writePointsToModule();
        repaint();
        return;
    }

    if (event.mods.isCtrlDown() || event.mods.isCommandDown())
    {
        // Strg+Klick loescht den Punkt; der letzte Punkt bleibt stehen.
        if (points.size() > (size_t) 1)
        {
            points.erase (points.begin() + index);
            draggedPoint = -1;
            writePointsToModule();
            repaint();
        }

        return;
    }

    draggedPoint = index;
    dragPointTo (index, event.getPosition());
}

void KickShaperView::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedPoint < 0)
        return;

    if (moduleAs<KickShaperModule>() == nullptr)
        return;

    dragPointTo (draggedPoint, event.getPosition());
}

void KickShaperView::mouseUp (const juce::MouseEvent&)
{
    draggedPoint = -1;
}

void KickShaperView::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (moduleAs<KickShaperModule>() == nullptr)
        return;

    const auto area = envelopeArea (getLocalBounds().reduced (1));

    if (! area.contains (event.getPosition()))
        return;

    if ((int) points.size() >= KickShaperModule::maxPoints)
        return;

    // Doppelklick auf einen Knoten fuegt nichts ein.
    if (pointAt (event.getPosition()) >= 0)
        return;

    KickShaperModule::Pt point;
    point.x = juce::jlimit (0.0, 1.0, positionForX ((float) event.getPosition().x, area));
    point.y = juce::jlimit (0.0, 1.0, valueForY ((float) event.getPosition().y, area));
    point.t = 0;

    // Stabile Einfuegung: hinter bereits gleich liegenden Punkten.
    const auto insertAt = std::upper_bound (points.begin(), points.end(), point,
                                            [] (const KickShaperModule::Pt& lhs,
                                                const KickShaperModule::Pt& rhs)
                                            { return lhs.x < rhs.x; });

    points.insert (insertAt, point);

    if (points.size() > (size_t) KickShaperModule::maxPoints)
        points.resize ((size_t) KickShaperModule::maxPoints);

    draggedPoint = -1;
    writePointsToModule();
    repaint();
}

} // namespace bfka
