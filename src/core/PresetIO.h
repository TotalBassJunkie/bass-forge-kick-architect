#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <optional>

#include "core/KickState.h"

namespace bfka
{

/**
 * Kopfdaten eines Presets (Wurzel-Element `<BFKAPreset>`).
 *
 * Das Format ist in `docs/04-SPEC-STATE-AND-PRESET-FORMAT.md` (Abschnitt 4)
 * festgelegt: `<BFKAPreset version name author styleId created kind>` mit dem
 * `BFKAKick`-Zustand (Kick-Preset) oder einem `<Module>` (Modul-Preset) als
 * Kind. Der Kopf ist reine Metadaten — er beeinflusst den Klang nicht.
 */
struct PresetInfo
{
    /** Hoechste bekannte Schema-Version des Preset-Kopfes. */
    static constexpr int schemaVersion = 1;

    juce::String name;          ///< Anzeigename ("Rumble 150").
    juce::String author;        ///< frei, optional.
    juce::String styleId;       ///< Style-Kennung, z. B. "rawstyle".
    juce::String created;       ///< ISO-8601, vom Schreiber gesetzt.
    juce::String appVersion;    ///< BFKA-Version, die das Preset geschrieben hat.

    bool         isModule   = false;   ///< true = Modul-Preset statt Kick.
    juce::String moduleType;           ///< Typ-Kennung des Modul-Presets ("dynEq").
};

/** Inhalt einer geladenen `.bfkapreset`-Datei. */
struct PresetData
{
    PresetInfo   info;
    KickState    state;     ///< nur bei Kick-Presets gefuellt
    FxModuleState module;   ///< nur bei Modul-Presets gefuellt
};

/**
 * Lesen und Schreiben von `.bfkapreset`-Dateien (Tickets B-93/B-94).
 *
 * Bewusst frei von `PluginProcessor`- und GUI-Abhaengigkeiten: damit ist das
 * Format ohne Plugin-Instanz testbar (`tests/PresetIOTests.cpp`). Der Prozessor
 * setzt nur `captureState()`/`applyState()` drumherum.
 *
 * Alle Methoden laufen auf dem Message-Thread, duerfen allokieren und fassen
 * keine Engine-Module an.
 */
class PresetIO
{
public:
    /** Endung und Wildcard kommen aus `Version` (eine Quelle der Wahrheit). */
    static juce::String fileExtension();
    static juce::String fileWildcard();

    /**
     * Ergaenzt eine fehlende Endung. Ein bereits vollstaendiger Name bleibt
     * unveraendert (kein doppeltes `.bfkapreset`).
     */
    static juce::File withPresetExtension (const juce::File& file);

    /** Standardordner fuer User-Presets (`Dokumente/BassForge/KickArchitect/Presets`). */
    static juce::File userPresetDirectory();

    /** Wie oben, legt den Ordner bei Bedarf an (nur Message-Thread). */
    static juce::File ensureUserPresetDirectory();

    /**
     * Schreibt ein Kick-Preset.
     *
     * @param file  Ziel; eine fehlende Endung wird ergaenzt.
     * @param state Vollstaendiger Kick-Zustand (Slots, Parameter, FX-Kette).
     * @param info  Kopfdaten; `name` leer = Dateiname ohne Endung.
     * @param error Klartextmeldung, wenn `false` zurueckkommt.
     */
    static bool saveKickPreset (const juce::File& file,
                                const KickState& state,
                                const PresetInfo& info,
                                juce::String& error);

    /**
     * Schreibt ein Modul-Preset (ein Kettenplatz als eigenstaendige Datei).
     *
     * Der Modulzustand inklusive Struktur (`<Band>`, `<Pt>`, `<Eq>`, `ir`) wird
     * vollstaendig uebernommen; der Kettenplatz behaelt beim Laden `enabled`,
     * `mix` und `outGain` — ein Modul-Preset beschreibt nur das Modul selbst.
     */
    static bool saveModulePreset (const juce::File& file,
                                  const FxModuleState& module,
                                  const PresetInfo& info,
                                  juce::String& error);

    /** Liest ein Kick-Preset (Wurzel `<BFKAPreset kind="kick">`). */
    static std::optional<PresetData> loadKickPreset (const juce::File& file, juce::String& error);

    /** Liest ein Modul-Preset (Wurzel `<BFKAPreset kind="module">`). */
    static std::optional<PresetData> loadModulePreset (const juce::File& file, juce::String& error);

    /** Baut den Preset-Baum (ohne Datei-Zugriff) — fuer Tests und Export. */
    static juce::ValueTree kickPresetToValueTree (const KickState& state, const PresetInfo& info);
    static juce::ValueTree modulePresetToValueTree (const FxModuleState& module, const PresetInfo& info);

    /**
     * Liest einen Preset-Baum (Wurzel `<BFKAPreset>`).
     *
     * Nimmt auch eine nackte `BFKAKick`-Wurzel an (das ist der Inhalt von
     * `getStateInformation()`), damit Host-Zustand und Preset-Datei dasselbe
     * Lesen benutzen koennen.
     */
    static std::optional<PresetData> fromValueTree (const juce::ValueTree& tree, juce::String& error);

    /** Prueft, ob `file` ueberhaupt nach einem Preset aussieht (Endung). */
    static bool hasPresetExtension (const juce::File& file);
};

} // namespace bfka
