#include "ui/editors/ConvolverView.h"

#include "fx/modules/ConvolverModule.h"
#include "plugin/PluginProcessor.h"
#include "ui/UiTheme.h"

namespace bfka
{

namespace
{
    constexpr int kTitleHeight = 18;
    constexpr int kIrRowHeight = 28;
    constexpr int kCaptionHeight = 14;

    /** IR-Browser unter der IR-Zeile (Ticket B-152). */
    constexpr int kIrListGap    = 4;
    constexpr int kIrListHeight = 70;
    constexpr int kIrListBlock  = kIrListHeight + kIrListGap;

    constexpr double kEqMinDb   = -24.0;
    constexpr double kEqMaxDb   =  24.0;
    constexpr double kDecayMin  = 0.1;
    constexpr double kDecayMax  = 2.0;
} // namespace

ConvolverView::ConvolverView (PluginProcessor& processor, int slotIndex)
    : ModuleStructureView (processor, slotIndex)
{
    eqCurve.fill (0.0f);
    decayCurve.fill (1.0f);

    irButton.setColour (juce::TextButton::buttonColourId, ui::theme::panelAlt);
    irButton.setColour (juce::TextButton::textColourOffId, ui::theme::text);
    irButton.onClick = [this] { chooseImpulseResponse(); };
    addAndMakeVisible (irButton);

    // IR-Browser: nur der User-Ordner, kein Factory-Ordner vorgegeben.
    impulseLibrary.scan();

    irListModel = std::make_unique<IrListModel> (*this);
    irList.setModel (irListModel.get());
    irList.setRowHeight (18);
    irList.setColour (juce::ListBox::backgroundColourId, ui::theme::panelAlt);
    addAndMakeVisible (irList);

    irListRowCount = (int) impulseLibrary.entries().size();
    irList.updateContent();

    readCurvesFromModule();
}

// ---------------------------------------------------------------------------
// IR-Browser (Ticket B-152)
// ---------------------------------------------------------------------------
int ConvolverView::IrListModel::getNumRows()
{
    return (int) owner.impulseLibrary.entries().size();
}

void ConvolverView::IrListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                   int w, int h, bool selected)
{
    const auto& entries = owner.impulseLibrary.entries();

    if (row < 0 || row >= (int) entries.size())
        return;

    const auto& entry = entries[(size_t) row];

    if (selected)
        g.fillAll (ui::theme::accent.withAlpha (0.18f));

    g.setFont (ui::theme::smallFont());
    g.setColour (selected ? ui::theme::text
                          : (entry.factory ? ui::theme::textDim : ui::theme::text));

    auto area = juce::Rectangle<int> (0, 0, w, h).reduced (4, 0);

    g.drawText (entry.name, area.removeFromLeft (juce::jmax (0, area.getWidth() - 46)),
                juce::Justification::centredLeft, true);

    g.setColour (ui::theme::textDim);
    g.drawText (juce::String (entry.seconds, 2) + " s",
                juce::Rectangle<int> (0, 0, w, h).reduced (4, 0),
                juce::Justification::centredRight, false);
}

void ConvolverView::IrListModel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    owner.applyImpulseEntry (row);
}

void ConvolverView::applyImpulseEntry (int row)
{
    const auto& entries = impulseLibrary.entries();

    if (row < 0 || row >= (int) entries.size())
        return;

    const auto& entry = entries[(size_t) row];

    if (auto* convolver = moduleAs<ConvolverModule>())
    {
        convolver->setImpulsePath (entry.file.getFullPathName());

        // Wie beim Datei-Dialog: erst ins Modul, dann in den Ketten-Zustand.
        proc.writeBackModuleStates();
    }

    refresh();
}

juce::Rectangle<int> ConvolverView::irRow (juce::Rectangle<int> contentArea) const
{
    return contentArea.withTrimmedTop (kTitleHeight).removeFromTop (kIrRowHeight);
}

juce::Rectangle<int> ConvolverView::irListArea (juce::Rectangle<int> contentArea) const
{
    const auto row = irRow (contentArea);

    return row.withY (row.getBottom() + kIrListGap).withHeight (kIrListHeight);
}

juce::Rectangle<int> ConvolverView::eqArea (juce::Rectangle<int> contentArea) const
{
    // Die Beschriftung wird kCaptionHeight ueber der Flaeche gezeichnet, daher
    // muss der IR-Block inklusive Beschriftungszeile Platz lassen.
    auto area = contentArea.withTrimmedTop (kTitleHeight + kIrRowHeight + kIrListBlock
                                            + kCaptionHeight)
                           .withTrimmedBottom (4).withTrimmedLeft (8).withTrimmedRight (8);

    return area.removeFromTop (area.getHeight() / 2).withTrimmedTop (kCaptionHeight);
}

juce::Rectangle<int> ConvolverView::decayArea (juce::Rectangle<int> contentArea) const
{
    auto area = contentArea.withTrimmedTop (kTitleHeight + kIrRowHeight + kIrListBlock
                                            + kCaptionHeight)
                           .withTrimmedBottom (4).withTrimmedLeft (8).withTrimmedRight (8);

    area.removeFromTop (area.getHeight() / 2);

    return area.withTrimmedTop (kCaptionHeight);
}

void ConvolverView::resized()
{
    const auto content = getLocalBounds().reduced (1);
    auto row = irRow (content);

    irButton.setBounds (row.removeFromLeft (110).withSizeKeepingCentre (104, 20));

    irList.setBounds (irListArea (content));
}

void ConvolverView::chooseImpulseResponse()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Impulse Response waehlen",
                                                      juce::File {},
                                                      "*.wav;*.aif;*.aiff");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (file == juce::File {} || ! file.existsAsFile())
            return;

        if (auto* convolver = moduleAs<ConvolverModule>())
        {
            convolver->setImpulsePath (file.getFullPathName());

            // IR-Pfad liegt als `ir`-Property im Modulzustand; der Writeback
            // zieht ihn in den Ketten-Zustand (und damit ins Preset).
            proc.writeBackModuleStates();
        }

        refresh();
    });
}

void ConvolverView::readCurvesFromModule()
{
    const auto* convolver = moduleAs<ConvolverModule>();

    if (convolver == nullptr)
        return;

    const auto state = convolver->getState();

    const auto readCurve = [&state] (const char* childName, std::array<float, numPoints>& target)
    {
        const auto child = state.getChildWithName (juce::Identifier (childName));

        if (! child.isValid())
            return;

        for (int i = 0; i < numPoints && i < child.getNumChildren(); ++i)
        {
            const auto point = child.getChild (i);
            const auto value = point.getProperty (juce::Identifier ("y"));

            if (value.isDouble() || value.isInt())
                target[(size_t) i] = (float) (double) value;
        }
    };

    readCurve ("Eq", eqCurve);
    readCurve ("Dc", decayCurve);

    if (! convolver->hasImpulseResponse())
    {
        irInfo = impulseLibrary.entries().empty()
                     ? "Keine IRs gefunden"
                     : "keine IR gewaehlt - das Modul ist neutral";
        return;
    }

    const auto fileName = juce::File (convolver->impulsePath()).getFileName();

    irInfo = juce::String (convolver->currentIrLength()) + " Samples / "
             + juce::String (convolver->currentIrChannels()) + " Kan. / "
             + juce::String (convolver->currentIrSampleRate() / 1000.0, 1) + " kHz"
             + (convolver->isTrueStereo() ? " / True Stereo" : juce::String())
             + (fileName.isNotEmpty() ? "  - " + fileName : juce::String());
}

void ConvolverView::writeCurvesToModule()
{
    auto* convolver = moduleAs<ConvolverModule>();

    if (convolver == nullptr)
        return;

    convolver->setEqCurve (eqCurve.data(), numPoints);
    convolver->setDecayCurve (decayCurve.data(), numPoints);

    // Ohne den Writeback ginge die Kurve beim naechsten setChain() verloren.
    proc.writeBackModuleStates();
}

void ConvolverView::refresh()
{
    readCurvesFromModule();

    // `refresh()` laeuft mit dem 15-Hz-Timer der Seite; die Liste wird nur bei
    // geaenderter Zeilenzahl neu aufgebaut, sonst flackert sie.
    const auto rows = (int) impulseLibrary.entries().size();

    if (rows != irListRowCount)
    {
        irListRowCount = rows;
        irList.updateContent();
    }

    repaint();
}

// ---------------------------------------------------------------------------
// Zeichnen
// ---------------------------------------------------------------------------
float ConvolverView::valueToY (float value, juce::Rectangle<int> area, double minValue,
                             double maxValue, bool logarithmic) const
{
    const auto value01 = logarithmic
                             ? (float) (std::log (juce::jmax ((double) value, minValue) / minValue)
                                        / std::log (maxValue / minValue))
                             : (float) ((value - minValue) / (maxValue - minValue));

    return area.getBottom() - juce::jlimit (0.0f, 1.0f, value01) * (float) area.getHeight();
}

float ConvolverView::yToValue (float y, juce::Rectangle<int> area, double minValue,
                               double maxValue, bool logarithmic) const
{
    const auto value01 = juce::jlimit (0.0f, 1.0f,
                                       ((float) area.getBottom() - y) / (float) juce::jmax (1, area.getHeight()));

    return logarithmic
               ? (float) (minValue * std::pow (maxValue / minValue, (double) value01))
               : (float) (minValue + (double) value01 * (maxValue - minValue));
}

void ConvolverView::paintCurve (juce::Graphics& g, juce::Rectangle<int> area,
                                const std::array<float, numPoints>& values,
                                double minValue, double maxValue, bool logarithmic,
                                juce::Colour colour, const juce::String& caption)
{
    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText (caption, area.withY (area.getY() - kCaptionHeight).withHeight (kCaptionHeight),
                juce::Justification::topLeft, false);

    ui::theme::paintPanel (g, area.toFloat().expanded (1.0f), ui::theme::panelAlt, 4.0f);

    // Raster
    g.setColour (ui::theme::grid.withAlpha (0.6f));

    for (int line = 1; line < 4; ++line)
    {
        const auto y = (float) area.getY() + (float) area.getHeight() * (float) line / 4.0f;
        g.drawHorizontalLine ((int) y, (float) area.getX(), (float) area.getRight());
    }

    // Nulllinie
    const auto zeroY = (float) valueToY (logarithmic ? (float) std::sqrt (minValue * maxValue) : 0.0f,
                                         area, minValue, maxValue, logarithmic);
    g.setColour (ui::theme::grid);
    g.drawHorizontalLine ((int) zeroY, (float) area.getX(), (float) area.getRight());

    // Kurve
    juce::Path path;

    for (int i = 0; i < numPoints; ++i)
    {
        const auto x = (float) area.getX() + (float) area.getWidth() * (float) i / (float) (numPoints - 1);
        const auto y = (float) valueToY (values[(size_t) i], area, minValue, maxValue, logarithmic);

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (1.6f));

    // Punkte
    for (int i = 0; i < numPoints; ++i)
    {
        const auto x = (float) area.getX() + (float) area.getWidth() * (float) i / (float) (numPoints - 1);
        const auto y = (float) valueToY (values[(size_t) i], area, minValue, maxValue, logarithmic);

        g.setColour (activeCurve == Curve::eq && activePoint == i ? ui::theme::accent : colour);
        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    }
}

void ConvolverView::paintContent (juce::Graphics& g, juce::Rectangle<int> contentArea)
{
    paintTitle (g, contentArea, "IMPULSE - EQ - DECAY");

    auto row = irRow (contentArea);
    row.removeFromLeft (114);

    g.setFont (ui::theme::smallFont());
    g.setColour (ui::theme::textDim);
    g.drawText (irInfo, row.withTrimmedRight (6), juce::Justification::centredLeft, true);

    paintCurve (g, eqArea (contentArea), eqCurve, kEqMinDb, kEqMaxDb, false,
                ui::theme::accent, "EQ +/-24 dB (20 Hz - 20 kHz)");

    paintCurve (g, decayArea (contentArea), decayCurve, kDecayMin, kDecayMax, true,
                ui::theme::bandLow, "DECAY 0,1x - 2x");
}

// ---------------------------------------------------------------------------
// Maus
// ---------------------------------------------------------------------------
ConvolverView::Curve ConvolverView::curveAt (juce::Point<int> position) const
{
    const auto content = getLocalBounds().reduced (1);

    if (eqArea (content).contains (position))
        return Curve::eq;

    if (decayArea (content).contains (position))
        return Curve::decay;

    return Curve::none;
}

int ConvolverView::pointIndexForX (float x, juce::Rectangle<int> area) const
{
    if (area.getWidth() <= 0)
        return -1;

    const auto norm = juce::jlimit (0.0f, 1.0f, (x - (float) area.getX()) / (float) area.getWidth());

    return juce::jlimit (0, numPoints - 1, juce::roundToInt (norm * (float) (numPoints - 1)));
}

void ConvolverView::mouseDown (const juce::MouseEvent& event)
{
    activeCurve = curveAt (event.getPosition());
    activePoint = -1;

    if (activeCurve == Curve::none)
        return;

    const auto content = getLocalBounds().reduced (1);
    const auto area = activeCurve == Curve::eq ? eqArea (content) : decayArea (content);

    activePoint = pointIndexForX ((float) event.getPosition().x, area);
    mouseDrag (event);
}

void ConvolverView::mouseDrag (const juce::MouseEvent& event)
{
    if (activeCurve == Curve::none || activePoint < 0)
        return;

    const auto content = getLocalBounds().reduced (1);

    if (activeCurve == Curve::eq)
    {
        const auto value = yToValue ((float) event.getPosition().y, eqArea (content),
                                     kEqMinDb, kEqMaxDb, false);
        eqCurve[(size_t) activePoint] = juce::jlimit ((float) kEqMinDb, (float) kEqMaxDb, value);
    }
    else
    {
        const auto value = yToValue ((float) event.getPosition().y, decayArea (content),
                                     kDecayMin, kDecayMax, true);
        decayCurve[(size_t) activePoint] = juce::jlimit ((float) kDecayMin, (float) kDecayMax, value);
    }

    writeCurvesToModule();
    repaint();
}

void ConvolverView::mouseUp (const juce::MouseEvent&)
{
    activeCurve = Curve::none;
    activePoint = -1;
}

} // namespace bfka
