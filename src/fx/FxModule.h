#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <optional>
#include <vector>

/**
 * Schnittstelle des FX-Racks (Ticket B-69).
 *
 * Dieses Header ist der Vertrag zwischen der Ketten-Engine und den zehn
 * Modulen (neun Referenztypen + `dynEq` aus Phase 2). Es ist bewusst
 * header-only und enthaelt keinen DSP.
 *
 * Grundregeln (Details in docs/specs/fx-chain-engine.md):
 *
 *  - Ein Modul bearbeitet die Summe **in-place** und bekommt das Signal so, wie
 *    es am Modul ankommt (also bereits durch die Vormodule gelaufen).
 *  - MIX und OUT GAIN macht **die Ketten-Engine**, nicht das Modul. Ein Modul
 *    liefert ausschliesslich sein zu 100 % nasses Signal.
 *  - Parameterwerte sind **Anzeige-Einheiten** (Prozent, dB, Hz, Modus-Index),
 *    nicht auf 0..1 normalisiert. Die Normalisierung fuer Host-Automation und
 *    Presets passiert in `bfka::ParamDesc` / `FxChainProcessor`.
 */
namespace bfka
{

/** Die zehn Modultypen des Racks (neun Referenztypen + `dynEq`). */
enum class FxType
{
    filters = 0,        ///< "filt"  — Filters (LP/HP/Band/Notch/Phaser/Comb)
    equalizer,          ///< "eq"    — Equalizer
    midSideEq,          ///< "mseq"  — Mid/Side-Equalizer
    distortion,         ///< "dist"  — Distortion
    kickShaper,         ///< "shaper"— Kick Shaper
    transientShaper,    ///< "trans" — Transient Shaper
    convolver,          ///< "reverb"— Convolver
    finalizer,          ///< "final" — Finalizer
    multiband,          ///< "ott"   — Multiband-Kompressor
    dynamicEq           ///< "dynEq" — Dynamischer Equalizer (Phase-2-Modul)
};

/** Alle Typen in Anzeige-/Registrier-Reihenfolge (Phase 2: zehn). */
inline constexpr int kNumFxTypes = 10;

/**
 * XML-Kennung eines Modultyps, exakt wie im Referenzformat (`type="…"`).
 * Unbekannte Enum-Werte liefern einen leeren String.
 */
inline juce::String toString (FxType type)
{
    switch (type)
    {
        case FxType::filters:         return "filt";
        case FxType::equalizer:       return "eq";
        case FxType::midSideEq:       return "mseq";
        case FxType::distortion:      return "dist";
        case FxType::kickShaper:      return "shaper";
        case FxType::transientShaper: return "trans";
        case FxType::convolver:       return "reverb";
        case FxType::finalizer:       return "final";
        case FxType::multiband:       return "ott";
        case FxType::dynamicEq:       return "dynEq";
    }

    return {};
}

/** Umkehrung von toString(); unbekannte Kennung -> std::nullopt. */
inline std::optional<FxType> fxTypeFromString (const juce::String& typeString)
{
    const auto key = typeString.trim().toLowerCase();

    if (key == "filt")   return FxType::filters;
    if (key == "eq")     return FxType::equalizer;
    if (key == "mseq")   return FxType::midSideEq;
    if (key == "dist")   return FxType::distortion;
    if (key == "shaper") return FxType::kickShaper;
    if (key == "trans")  return FxType::transientShaper;
    if (key == "reverb") return FxType::convolver;
    if (key == "final")  return FxType::finalizer;
    if (key == "ott")    return FxType::multiband;
    if (key == "dyneq")  return FxType::dynamicEq;

    return std::nullopt;
}

/** Anzeigename des Modultyps (Rack-Kopf, `+ ADD MODULE`-Liste). */
inline juce::String fxDisplayName (FxType type)
{
    switch (type)
    {
        case FxType::filters:         return "Filters";
        case FxType::equalizer:       return "Equalizer";
        case FxType::midSideEq:       return "Mid/Side EQ";
        case FxType::distortion:      return "Distortion";
        case FxType::kickShaper:      return "Kick Shaper";
        case FxType::transientShaper: return "Transient Shaper";
        case FxType::convolver:       return "Convolver";
        case FxType::finalizer:       return "Finalizer";
        case FxType::multiband:       return "Multiband";
        case FxType::dynamicEq:       return "Dynamic EQ";
    }

    return {};
}

/**
 * Beschreibung eines Modulparameters.
 *
 * `id` ist der Attributname aus dem Referenz-XML (z. B. "drive", "mode",
 * "folds"). `minValue`/`maxValue`/`defaultValue` sind in **Anzeige-Einheiten**
 * (dieselben Einheiten, die im XML stehen); `unit` dient nur der Anzeige.
 */
struct FxParamDesc
{
    juce::String id;            ///< XML-Attributname, zugleich stabile Kennung
    juce::String name;          ///< Anzeigename im Modulkopf
    float minValue   = 0.0f;
    float maxValue   = 1.0f;
    float defaultValue = 0.0f;
    juce::String unit;          ///< z. B. "%", "dB", "Hz"
};

/**
 * Basisklasse aller Module.
 *
 * Lebenszyklus: Konstruktor -> `prepare()` -> beliebig oft `process()`/`reset()`
 * -> Destruktor. `prepare()` darf mehrfach aufgerufen werden (neue Sample-Rate
 * oder Blockgroesse) und muss dabei den internen Zustand zuruecksetzen.
 *
 * Threading: `prepare()`, `reset()` und `setParam()` laufen auf dem Message-
 * Thread, `process()` auf dem Audio-Thread. Ein Modul darf in `process()` nicht
 * allokieren, nicht loggen und keinen Mutex nehmen. Parameteraenderungen
 * waehrend `process()` muessen entweder atomar oder gepuffert sein.
 */
class FxModule
{
public:
    virtual ~FxModule() = default;

    /** Der Typ dieses Moduls. */
    virtual FxType type() const noexcept = 0;

    /**
     * Parametertabelle in Anzeige-Reihenfolge. Der Index ist die `index`-Nummer
     * fuer `setParam()`/`getParam()` und bestimmt die Zuordnung zu den
     * Host-Proxy-Parametern `fx<N>_p<M>`.
     */
    virtual const std::vector<FxParamDesc>& params() const noexcept = 0;

    /** Audio-Vorbereitung; darf intern allokieren und muss `reset()` einschliessen. */
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;

    /** Setzt alle internen Zustaende (Filter, Huellkurven, Verzoegerungen) zurueck. */
    virtual void reset() = 0;

    /** Setzt Parameter `index` auf einen Wert in Anzeige-Einheiten (wird geklemmt). */
    virtual void setParam (int index, float value) = 0;

    /** Letzter gesetzter Wert in Anzeige-Einheiten; 0.0f bei ungueltigem Index. */
    virtual float getParam (int index) const = 0;

    /**
     * Bearbeitet die Summe **in-place**.
     *
     * Das Buffer enthaelt das trockene Signal; das Modul schreibt sein nasses
     * Signal zurueck. MIX und OUT GAIN werden von `FxChainProcessor` angewendet.
     */
    virtual void process (juce::AudioBuffer<float>& buffer) = 0;

    /**
     * Latenz des Moduls in Samples bei der zuletzt vorbereiteten Sample-Rate.
     * Wird nach `prepare()` abgefragt; Standard 0 (latenzfrei).
     */
    virtual int latencySamples() const noexcept { return 0; }

    /**
     * Nachhall-/Ausklangzeit des Moduls in Sekunden (B-150).
     * Wird von PluginProcessor::getTailLengthSeconds() aufsummiert.
     * Latenzfreie Module liefern 0.
     */
    virtual double tailSeconds() const noexcept { return 0.0; }

    /** Anzeigename dieses Moduls (Kartenkopf). */
    virtual juce::String displayName() const = 0;

    // -----------------------------------------------------------------------
    // Externer Sidechain (B-146)
    // -----------------------------------------------------------------------
    // Ein Modul mit Detektor (bisher nur `dynEq`) kann statt des eigenen
    // Eingangssignals ein zweites Signal auswerten. Die Ketten-Engine setzt den
    // Zeiger vor jedem `process()`-Aufruf und nimmt ihn danach wieder weg; das
    // Signal ist bereits um die Latenz der Vormodule verzoegert, damit Detektor
    // und Nutzsignal zeitlich fluchten.

    /**
     * Meldet den Detektor-Block fuer den naechsten `process()`-Aufruf an.
     *
     * `sidechain` ist `nullptr`, wenn kein externes Signal anliegt — dann
     * arbeitet das Modul wie bisher mit dem internen Detektor. Der Buffer hat
     * dieselbe Samplezahl wie der Block, darf vom Modul nur gelesen werden und
     * ist hoechstens bis zum Ende des `process()`-Aufrufs gueltig (nicht
     * speichern, nur kopieren).
     *
     * Laeuft auf dem Audio-Thread; die Standard-Implementierung ignoriert den
     * Zeiger (Module ohne Sidechain).
     */
    virtual void setSidechainBuffer (const juce::AudioBuffer<float>* /*sidechain*/) noexcept {}

    /** true, wenn das Modul ein externes Detektor-Signal auswerten kann. */
    virtual bool acceptsExternalSidechain() const noexcept { return false; }

    // -----------------------------------------------------------------------
    // Struktur-Zustand (Bänder, Kurven, Hüllkurven, IR-Pfad)
    // -----------------------------------------------------------------------
    // Die 14 Host-Proxy-Parameter `fx<N>_p<M>` tragen nur Skalare. Alles, was
    // eine Liste ist — EQ-Bänder, Waveshaper-Knoten, Kick-Shaper-Hüllkurven,
    // Reverb-Hüllkurven, der IR-Pfad und die Band-Splits des Multiband —
    // steckt im `<FxChain>`-Snapshot des Referenzformats und wird hier
    // durchgereicht.
    //
    // Beide Methoden laufen auf dem Message-Thread, nicht im Audio-Thread.
    // Module, die keine Struktur brauchen, lassen die Defaults stehen.

    /** Baut den Struktur-Zustand in `FxModuleState::data`-Form auf (<Module>-ValueTree). */
    virtual juce::ValueTree getState() const { return juce::ValueTree ("Module"); }

    /** Liest den Struktur-Zustand; unbekannte Kinder werden ignoriert. */
    virtual void setState (const juce::ValueTree& /*state*/) {}
};

} // namespace bfka
