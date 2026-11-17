#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <optional>
#include <vector>

/**
 * Schnittstelle des FX-Racks (Ticket B-69).
 *
 * Dieses Header ist der Vertrag zwischen der Ketten-Engine und den neun
 * Modulen. Es ist bewusst header-only und enthaelt keinen DSP.
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

/** Die neun Modultypen des Racks. */
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
    multiband           ///< "ott"   — Multiband-Kompressor
};

/** Alle neun Typen in Anzeige-/Registrier-Reihenfolge. */
inline constexpr int kNumFxTypes = 9;

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
 * Basisklasse aller neun Module.
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

    /** Anzeigename dieses Moduls (Kartenkopf). */
    virtual juce::String displayName() const = 0;
};

} // namespace bfka
