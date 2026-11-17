#pragma once

#include <juce_core/juce_core.h>

#include "core/KickState.h"
#include "fx/FxModule.h"

namespace bfka
{

/**
 * Reine Logik der FX-Kette fuer die FX-Seite (Ticket B-90).
 *
 * Das Modell haelt eine `FxChain`, kennt aber weder GUI noch Prozessor; damit
 * sind Add/Remove/Move/Duplicate ohne Plugin-Instanz testbar
 * (tests/FxChainModelTests.cpp). Die Seite uebernimmt das Ergebnis anschliessend
 * mit `PluginProcessor::setFxChain()`.
 *
 * Threading: nur Message-Thread.
 */
class FxChainModel
{
public:
    FxChainModel() = default;
    explicit FxChainModel (FxChain initialChain);

    /**
     * Werkskette fuer den Knopf DEFAULT (Ticket B-94).
     *
     * Saemtliche Werte sind Modul-Defaults; die Kette ist bewusst kurz und
     * neutral (Hochpass gegen Subsonic, Finalizer fuer den Master).
     */
    static FxChain makeDefaultChain();

    // --- Sperre (B-94) ----------------------------------------------------
    /**
     * Sperrt alle Struktur-Aenderungen (Add/Remove/Move/Duplicate/Clear).
     * Regler, MIX/OUT und Bypass bleiben bedienbar — die Sperre schuetzt die
     * Kette vor versehentlichem Loeschen, nicht vor Klangregelung.
     */
    void setLocked (bool shouldBeLocked) noexcept { locked = shouldBeLocked; }
    bool isLocked() const noexcept { return locked; }

    // --- Struktur ---------------------------------------------------------
    /**
     * Haengt ein Modul mit den Werks-Defaults hinten an.
     *
     * Das neue Modul bekommt `mix = 1`, `outGain = 1`, `enabled = true` und
     * **kein** `data` — die Module starten so mit ihren eigenen Defaults.
     * @returns Index des neuen Kettenplatzes, -1 wenn der Typ unbekannt oder
     *          die Kette gesperrt ist.
     */
    int addModule (FxType type);

    /** Entfernt einen Kettenplatz; `false` bei ungueltigem Index oder Sperre. */
    bool removeModule (int index);

    /** Verschiebt einen Platz; `from`/`to` werden geklemmt, `false` wenn unveraendert. */
    bool moveModule (int from, int to);

    /**
     * Kopiert einen Kettenplatz direkt hinter das Original (Zustand kopiert,
     * gleiche Instanzparameter).
     * @returns Index der Kopie, -1 bei ungueltigem Index oder Sperre.
     */
    int duplicateModule (int index);

    /** Entfernt alle Plaetze (Bypass-Flag bleibt); bei Sperre nichts. */
    void clear();

    /** Ketten-Bypass (Power-Schalter der Seite). */
    void setChainBypass (bool shouldBypassAll);
    bool isChainBypassed() const noexcept { return chainData.bypassAll; }

    // --- Zugriff ----------------------------------------------------------
    FxChain&       chain()       noexcept { return chainData; }
    const FxChain& chain() const noexcept { return chainData; }

    int  size() const noexcept { return (int) chainData.modules.size(); }
    bool isValidIndex (int index) const noexcept;

    /** Kettenplatz oder nullptr. */
    FxModuleState*       module (int index) noexcept;
    const FxModuleState* module (int index) const noexcept;

private:
    FxChain chainData;
    bool    locked = false;
};

} // namespace bfka
