// B-57/B-58: LAST PASS-Report und RESULT LINE.

#include "engine/AlignReport.h"

#include <algorithm>
#include <cmath>

namespace bfka
{

namespace
{
    /** Millisekunden aus Sekunden, eine Nachkommastelle, ohne Vorzeichen. */
    juce::String milliseconds (double seconds)
    {
        const double ms = seconds * 1000.0;

        // -0.0 darf nicht als "-0.0" im Report stehen.
        const double shown = (std::abs (ms) < 0.05) ? 0.0 : ms;

        return juce::String (shown, 1) + " ms";
    }

    /** Groesster Wert eines Vektors in Millisekunden; 0 bei leerem Vektor. */
    double maxGapMs (const std::vector<double>& gaps) noexcept
    {
        double maximum = 0.0;

        for (const double gap : gaps)
            maximum = std::max (maximum, gap);

        return maximum * 1000.0;
    }
} // namespace

AlignReportSettings AlignReport::settingsFrom (const AlignSettings& align, double blend)
{
    // Die AlignSettings tragen noch keinen Blend; die Anzeige kommt vom Aufrufer.
    (void) align;

    AlignReportSettings settings;
    settings.showResultLine = true;
    settings.showGapRows    = true;
    settings.alignBlend     = juce::jlimit (0.0, 1.0, blend);

    return settings;
}

LastPassReport AlignReport::build (const AlignResult& result,
                                   const AlignReportSettings& settings)
{
    LastPassReport report;

    report.ok = result.changed || ! result.report.isEmpty();

    // Ohne Lauf gibt es nichts zu zeigen: fester Platzhalter statt Zahlen.
    if (! report.ok)
    {
        report.lines.clear();
        report.lines.add ("LAST PASS: kein Lauf");
        report.resultLine = "ALIGN --";
        return report;
    }

    report.maxGapMsBefore = maxGapMs (result.gapsBeforeSeconds);
    report.maxGapMsAfter  = maxGapMs (result.gapsAfterSeconds);

    for (const double offset : result.offsetsSeconds)
        if (std::abs (offset) > 1e-9)
            ++report.movedParts;

    report.lines.add ("LAST PASS");

    const double blend = juce::jlimit (0.0, 1.0, settings.alignBlend);
    const int    blendPercent = (int) std::lround (blend * 100.0);

    report.lines.add ("BLEND " + juce::String (blendPercent) + " %");

    if (settings.showGapRows)
    {
        for (size_t i = 0; i < result.gapsBeforeSeconds.size(); ++i)
        {
            const double gapBefore = result.gapsBeforeSeconds[i];

            // Nur echte Luecken bekommen eine Zeile; der Anker traegt 0.
            if (gapBefore <= 0.0)
                continue;

            const double gapAfter = (i < result.gapsAfterSeconds.size())
                                        ? result.gapsAfterSeconds[i]
                                        : 0.0;

            report.lines.add ("PART " + juce::String ((int) i) + ": "
                              + milliseconds (gapBefore) + " -> "
                              + milliseconds (gapAfter));
        }
    }

    for (const auto& line : result.report)
        report.lines.add (line);

    if (! settings.showResultLine)
        report.resultLine.clear();
    else
        report.resultLine = resultLineFor (result);

    return report;
}

juce::String AlignReport::resultLineFor (const AlignResult& result)
{
    // Ohne Aenderung gibt es keine Zahlen: die Zeile bleibt ein Platzhalter.
    if (! result.changed)
        return "ALIGN --";

    return "ALIGN " + juce::String (maxGapMs (result.gapsBeforeSeconds), 1)
           + " -> " + juce::String (maxGapMs (result.gapsAfterSeconds), 1) + " ms";
}

} // namespace bfka
