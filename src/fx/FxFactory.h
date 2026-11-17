#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>
#include <vector>

#include "fx/FxModule.h"

namespace bfka
{

/**
 * Registry der FX-Modultypen (Singleton).
 *
 * Der Konstruktor registriert alle neun Typen mit `PassThroughModule` als
 * Platzhalter. Die echten Module ersetzen ihren Eintrag spaeter einmalig beim
 * Start:
 *
 *     FxFactory::get().registerType (FxType::distortion,
 *                                    [] { return std::make_unique<DistortionModule>(); });
 *
 * Threading: `registerType()` ist **nicht** gegen gleichzeitige Aufrufe von
 * `create()` geschuetzt. Registrierung passiert beim Start/auf dem Message-
 * Thread, `create()` wird von `FxChainProcessor::setChain()` (ebenfalls
 * Message-Thread) gerufen — nie aus dem Audio-Thread, weil dabei allokiert wird.
 */
class FxFactory
{
public:
    using Creator = std::function<std::unique_ptr<FxModule>()>;

    /** Die eine Registry-Instanz. */
    static FxFactory& get();

    /**
     * Registriert (oder ersetzt) den Erzeuger fuer einen Typ.
     * Ein leerer Creator entfernt den Eintrag.
     */
    void registerType (FxType type, Creator creator);

    /** Ist fuer diesen Typ ein Creator hinterlegt? */
    bool isRegistered (FxType type) const;

    /**
     * Der aktuell hinterlegte Erzeuger fuer einen Typ; leer, wenn keiner.
     *
     * Damit laesst sich eine Registrierung befristet ersetzen und danach exakt
     * wiederherstellen (Tests, A/B-Austausch einzelner Module), ohne den
     * echten Erzeuger zu kennen.
     */
    Creator creatorFor (FxType type) const;

    /**
     * Erzeugt eine Modulinstanz aus der XML-Kennung ("eq", "dist", …).
     * Unbekannte Kennungen und nicht registrierte Typen liefern `nullptr`;
     * eine Meldung macht der Aufrufer (er kennt den Kettenplatz).
     */
    std::unique_ptr<FxModule> create (const juce::String& typeString) const;

    /** Alle registrierten Kennungen in Registrier-Reihenfolge ("filt", "eq", …). */
    juce::StringArray availableTypes() const;

private:
    FxFactory();

    struct Entry
    {
        FxType  type = FxType::filters;
        Creator creator;
    };

    std::vector<Entry> entries;

    JUCE_DECLARE_NON_COPYABLE (FxFactory)
};

} // namespace bfka
