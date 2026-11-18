// B-50/B-51: ALIGN - Gap-Analyse (Ende Anker <-> Beginn Tails) und MOVE CRUNCH.

#include "engine/AlignEngine.h"

#include <algorithm>
#include <cmath>

namespace bfka
{

namespace
{
    /** Toleranzen in Sekunden: TIGHT 0 ms, NORMAL 10 ms, GENTLE 25 ms. */
    constexpr double kToleranceTight  = 0.0;
    constexpr double kToleranceNormal = 0.010;
    constexpr double kToleranceGentle = 0.025;

    /** Modus als Text fuer die Report-Kopfzeile. */
    juce::String modeToString (AlignMode mode)
    {
        switch (mode)
        {
            case AlignMode::tight:  return "TIGHT";
            case AlignMode::normal: return "NORMAL";
            case AlignMode::gentle: return "GENTLE";
        }

        return "NORMAL";
    }

    /** Methode als Text fuer die Report-Kopfzeile. */
    juce::String methodToString (AlignMethod method)
    {
        switch (method)
        {
            case AlignMethod::moveCrunch:    return "MOVE CRUNCH";
            case AlignMethod::stretchPunch:  return "STRETCH PUNCH";
        }

        return "MOVE CRUNCH";
    }

    /** Sekunden als Millisekunden mit einer Nachkommastelle und Vorzeichen. */
    juce::String milliseconds (double seconds)
    {
        const double ms = seconds * 1000.0;

        // -0.0 darf nicht als "-0.0" im Report stehen.
        const double shown = (std::abs (ms) < 0.05) ? 0.0 : ms;

        return (shown > 0.0 ? "+" : "") + juce::String (shown, 1) + " ms";
    }
} // namespace

double AlignEngine::toleranceFor (AlignMode mode) noexcept
{
    switch (mode)
    {
        case AlignMode::tight:  return kToleranceTight;
        case AlignMode::gentle: return kToleranceGentle;
        case AlignMode::normal: break;
    }

    return kToleranceNormal;
}

int AlignEngine::findAnchor (const std::vector<PartTiming>& parts) noexcept
{
    if (parts.empty())
        return -1;

    // 1. Explizit gesetzter Anker gewinnt; der erste Treffer zaehlt.
    for (size_t i = 0; i < parts.size(); ++i)
        if (parts[i].isAnchor)
            return (int) i;

    // 2. Sonst der Punch mit dem fruehesten Start.
    int    best      = -1;
    double bestStart = 0.0;

    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (parts[i].role != Role::punch)
            continue;

        if (best < 0 || parts[i].startSeconds < bestStart)
        {
            best      = (int) i;
            bestStart = parts[i].startSeconds;
        }
    }

    if (best >= 0)
        return best;

    // 3. Sonst der frueheste Part ueberhaupt.
    for (size_t i = 0; i < parts.size(); ++i)
        if (best < 0 || parts[i].startSeconds < bestStart)
        {
            best      = (int) i;
            bestStart = parts[i].startSeconds;
        }

    return best;
}

AlignResult AlignEngine::runMoveCrunch (const KickState& state,
                                        const std::vector<PartTiming>& parts)
{
    AlignResult result;

    result.offsetsSeconds.assign (parts.size(), 0.0);
    result.stretchFactors.assign (parts.size(), 1.0);
    result.gapsBeforeSeconds.assign (parts.size(), 0.0);
    result.gapsAfterSeconds.assign (parts.size(), 0.0);

    const auto& settings = state.align();

    // Aus oder kein Anker: nichts anfassen, nur die Luegen auf 0 setzen.
    if (! settings.enabled || parts.empty())
    {
        result.report.add ("ALIGN aus");
        result.changed = false;
        return result;
    }

    const int anchor = findAnchor (parts);

    if (anchor < 0)
    {
        result.report.add ("ALIGN aus");
        result.changed = false;
        return result;
    }

    const double tolerance = toleranceFor (settings.tightness);
    const double anchorEnd = parts[(size_t) anchor].endSeconds;

    result.report.add ("ALIGN " + modeToString (settings.tightness)
                       + " / " + methodToString (settings.method));

    double gapMaxBefore = 0.0;
    double gapMaxAfter  = 0.0;

    for (size_t i = 0; i < parts.size(); ++i)
    {
        const int index = (int) i;

        // Der Anker selbst bleibt stehen und traegt keine Luecke.
        if (index == anchor)
            continue;

        const double gap = parts[i].startSeconds - anchorEnd;

        result.gapsBeforeSeconds[i] = std::max (0.0, gap);

        if (parts[i].pinned)
        {
            // Gepinnte Parts bleiben, wo sie sind; ihre Luecke bleibt bestehen.
            result.gapsAfterSeconds[i] = std::max (0.0, gap);
            result.report.add ("PINNED " + juce::String (index));

            gapMaxBefore = std::max (gapMaxBefore, result.gapsBeforeSeconds[i]);
            gapMaxAfter  = std::max (gapMaxAfter,  result.gapsAfterSeconds[i]);
            continue;
        }

        double offset = 0.0;

        // Schon dicht genug: innerhalb der Toleranz wird nicht gezogen.
        if (gap > tolerance)
            offset = -(gap - tolerance);   // nach vorne ziehen, also negativ

        result.offsetsSeconds[i] = offset;
        result.gapsAfterSeconds[i] = std::max (0.0, gap + offset);

        if (offset != 0.0)
            result.report.add ("MOVE " + juce::String (index) + ": "
                               + milliseconds (offset));

        gapMaxBefore = std::max (gapMaxBefore, result.gapsBeforeSeconds[i]);
        gapMaxAfter  = std::max (gapMaxAfter,  result.gapsAfterSeconds[i]);
    }

    result.report.add ("GAP max " + juce::String (gapMaxBefore * 1000.0, 1)
                       + " ms -> " + juce::String (gapMaxAfter * 1000.0, 1) + " ms");

    for (const double offset : result.offsetsSeconds)
        if (offset != 0.0)
        {
            result.changed = true;
            break;
        }

    return result;
}

} // namespace bfka
