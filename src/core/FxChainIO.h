#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <optional>

#include "core/KickState.h"

namespace bfka
{

/**
 * Serialisierung der FX-Kette (Ticket B-143).
 *
 * Das Schema ist das Referenzformat `<FxChain bypassAll="…">` mit einem
 * `<Module type="…" on="…" mix="…" outGain="…">` je Kettenplatz. Der
 * modulspezifische Struktur-Zustand (`FxModuleState::data`) haengt als **erstes
 * Kind** unter `<Module>` — so, wie ihn `FxChainProcessor::setChain()` an die
 * Module reicht.
 *
 * Die Funktionen sind bewusst frei von `PluginProcessor`-Abhaengigkeiten: der
 * Zustand ist damit ohne Plugin-Klasse testbar (tests/FxChainIOTests.cpp).
 * `KickState::toValueTree()`/`fromValueTree()` rufen sie auf, damit das Schema
 * an genau einer Stelle steht.
 *
 * Beide Funktionen laufen auf dem Message-Thread und duerfen allokieren.
 */

/**
 * Baut den `<FxChain>`-Baum aus der Kette.
 *
 * Die Reihenfolge der Kinder ist die Kettenreihenfolge; ein Modul ohne
 * `type` wird mitgeschrieben (er bleibt so erhalten, auch wenn ihn die Engine
 * spaeter nicht erzeugen kann).
 */
juce::ValueTree fxChainToValueTree (const FxChain& chain);

/**
 * Liest eine Kette **tolerant** aus einem Baum.
 *
 * Akzeptiert sowohl einen `<FxChain>`-Baum selbst als auch jeden Baum, der ein
 * `<FxChain>`-Kind traegt (z. B. die `BFKAKick`-Wurzel). Rueckgabe:
 *  - `std::nullopt`, wenn kein `<FxChain>` gefunden wurde (Aufrufer entscheidet,
 *    was das bedeutet — eine leere Kette ist etwas anderes als kein Baum),
 *  - sonst die Kette; unbekannte Modultypen bleiben als Text erhalten,
 *    fehlende Attribute werden auf `on=true`, `mix=1`, `outGain=1` gesetzt.
 */
std::optional<FxChain> fxChainFromValueTree (const juce::ValueTree& tree);

} // namespace bfka
