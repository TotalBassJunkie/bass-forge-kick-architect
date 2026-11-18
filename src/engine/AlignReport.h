#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "core/KickState.h"
#include "engine/AlignEngine.h"

// B-57/B-58: LAST PASS als Anzeige-Objekt und RESULT LINE fuer den Transport.
//
// Der Report ist reine Formatierung: er liest ein AlignResult und erzeugt
// Zeilen fuer das LAST PASS-Fenster und eine Kurzzeile fuer den Transport.
// Kein Audio, kein Zustand, keine Allokation im Hot-Path (nur Message-Thread).

namespace bfka
{

/** Anzeige-Einstellungen des Reports (Ticket B-58). */
struct AlignReportSettings
{
    bool   showResultLine = true;   ///< RESULT LINE im Transport
    bool   showGapRows    = true;   ///< einzelne Gap-Zeilen im Report
    double alignBlend     = 1.0;    ///< 0 = kein ALIGN, 1 = voll (wird geklemmt)
};

/** Ergebnis des letzten ALIGN-Laufs als Anzeige-Objekt (Ticket B-57). */
struct LastPassReport
{
    juce::StringArray lines;        ///< Zeilen fuer das LAST PASS-Fenster
    juce::String      resultLine;   ///< eine Zeile fuer den Transport
    double maxGapMsBefore = 0.0;
    double maxGapMsAfter  = 0.0;
    int    movedParts     = 0;
    bool   ok             = false;  ///< true, wenn ALIGN gelaufen ist
};

/**
 * Baut aus einem ALIGN-Ergebnis die Anzeige-Objekte (B-57/B-58).
 * Reine Formatierung, kein Audio, kein Zustand.
 */
class AlignReport
{
public:
    /** Uebernimmt Blend aus den AlignSettings, geklemmt auf 0..1. */
    static AlignReportSettings settingsFrom (const AlignSettings& align, double blend);

    /** Baut Report + RESULT LINE. */
    static LastPassReport build (const AlignResult& result, const AlignReportSettings& settings);

    /** Kurzzeile fuer den Transport, z. B. "ALIGN 3.4 -> 0.0 ms". */
    static juce::String resultLineFor (const AlignResult& result);
};

} // namespace bfka
