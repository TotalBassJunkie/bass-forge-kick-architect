#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "core/KickState.h"
#include "engine/KickAssembler.h"
#include "engine/KickRenderer.h"
#include "fx/FxChainProcessor.h"

namespace bfka
{

/**
 * Der Plugin-Prozessor.

 * Phase P0: Rahmen, Parameterbaum, Zustandsspeicherung und ein stummer
 * Audio-Pfad. Die Kick-Engine (P3) und das FX-Rack (P6) haengen sich spaeter
 * in `processBlock` bzw. `prepareToPlay` ein.
 */
class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    // --- juce::AudioProcessor ---------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Zugriff fuer die UI ----------------------------------------------
    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts; }

    /** Baut den vollstaendigen Zustand (Parameter + Slots + FX) fuer Export/Test. */
    KickState captureState() const;

    /** Uebernimmt einen Zustand (Parameter + Slots + FX). */
    void applyState (const KickState& state);

    // --- FX-Kette (B-143) -------------------------------------------------
    /**
     * Uebernimmt eine neue Kette: schreibt sie in `extraState`, baut die
     * Engine-Module neu und meldet die neue Gesamtlatenz an den Host.
     *
     * Nur vom Message-Thread aufrufen — `FxChainProcessor::setChain()`
     * allokiert und bereitet die Module vor. In der UI immer
     * `suspendProcessing (true)` … `setFxChain (…)` … `suspendProcessing (false)`
     * verwenden.
     */
    void setFxChain (const FxChain& chain);

    /** Die Kette, wie sie zuletzt gesetzt wurde (Zustand, nicht die Engine). */
    const FxChain& getFxChain() const noexcept { return currentChain; }

    /** Anzahl der im Engine-Rack gebauten Plaetze (Diagnose/Test). */
    int getEngineModuleCount() const noexcept { return fxChain.numModules(); }

    // --- Live-Bedienung der Module (B-144) --------------------------------
    // Alle Methoden laufen auf dem Message-Thread. Regler-Aenderungen wirken
    // sofort (atomar im Modul) und landen gleichzeitig im Ketten-Zustand, damit
    // der Wert im Preset steht. Ein Rebuild der Engine findet nur bei
    // `setFxChain()` statt (Add/Remove/Move/Preset).
    //
    // Fuer Struktur-Aenderungen (Bänder, Kurven, IR-Pfad) gilt: erst in das
    // Modul schreiben, dann `writeBackModuleStates()` aufrufen.

    /** Setzt Parameter `index` eines Moduls in Anzeige-Einheiten (dB, Hz, %). */
    void setModuleParam (int slot, int index, float engineeringValue);

    /** Letzter Wert eines Modulparameters in Anzeige-Einheiten (0 bei Fehlern). */
    float getModuleParam (int slot, int index) const;

    /**
     * Setzt ein Modul-Attribut (`FxParamDesc::id`) im Ketten-Zustand.
     *
     * Ist die Kennung ein Parameter des Moduls, wirkt der Wert sofort auf den
     * DSP; andere Attribute landen nur im Zustand (Snapshot, z. B. `ir`).
     */
    void setModuleAttribute (int slot, const juce::String& id, double engineeringValue);

    /** MIX 0…1, OUT GAIN 0…2 und Power je Kettenplatz (Engine + Zustand). */
    void setModuleMix (int slot, float mix01);
    void setModuleOutGain (int slot, float gain);
    void setModuleEnabled (int slot, bool shouldBeEnabled);

    /**
     * Uebernimmt fuer jedes Modul `getState()` in den Ketten-Zustand (O6).
     *
     * Regel: Skalare Attribute in `data` bleiben erhalten (sie kommen aus
     * `setModuleAttribute()`/`setModuleParam()`), Kinder mit gleichem Namen wie
     * die von `getState()` gelieferten (`Band`, `WNode`, `Pt`, `Eq`, `Dc`)
     * werden ersetzt, die String-Property `ir` wird uebernommen. Kein
     * Engine-Rebuild.
     */
    void writeBackModuleStates();

    /** Anzeigedaten (pre/post) eines Kettenplatzes fuer die Karten-Wellenform. */
    const ModuleDisplay& getModuleDisplay (int slot) const { return fxChain.displayFor (slot); }

    /** Modulzeiger fuer Diagnose und Spezial-Editoren (nullptr = unbekannter Typ). */
    FxModule* getModule (int slot) { return fxChain.moduleAt (slot); }
    const FxModule* getModule (int slot) const { return fxChain.moduleAt (slot); }

    /** Letzte gespielte MIDI-Note (Anzeige/Test). */
    int getLastNote() const noexcept { return lastNote.load(); }

    /**
     * Baut einen hoerbaren Test-Kick aus der Sample-Library und startet ihn.

     * Entwicklungsweg fuer den A/B-Vergleich; die vollstaendige Preset- und
     * Library-Verwaltung kommt mit der UI (P8/P10).
     * @returns false, wenn keine Library gefunden wurde.
     */
    bool startDemoKick();

    /** Startet den aktuellen Kick (wie eine Note). */
    void triggerKick();

    /** Meldungen des letzten Ladevorgangs. */
    juce::String getLastLoadReport() const { return lastLoadReport; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /**
     * Baut `currentChain` aus `extraState` und schiebt sie in die Engine;
     * setzt danach die Host-Latenz. Laeuft auf dem Message-Thread.
     */
    void rebuildEngineChain();

    /** Schreibt `currentChain` als `<FxChain>` in `extraState` (ohne Rebuild). */
    void storeFxChainInExtraState();

    /** Prueft `slot` gegen `currentChain`; liefert den Modulzustand oder nullptr. */
    FxModuleState* moduleStateFor (int slot) noexcept;

    juce::AudioProcessorValueTreeState apvts;

    /** Nicht-Parameter-Zustand: Samples, Rollen, Locks, FX-Kette, Metadaten.
     *
     * Einzige Wahrheit fuer die FX-Kette: jede Aenderung an der Kette landet
     * zuerst hier (siehe `setFxChain()`), `captureState()` liest sie von hier.
     */
    juce::ValueTree extraState;

    KickRenderer     renderer;
    FxChainProcessor fxChain;
    KickAssembler    assembler;
    juce::File       contentRoot;
    juce::String     lastLoadReport;

    /** Die aktuell gesetzte Kette (Spiegel von `extraState`, Zustand + Engine). */
    FxChain currentChain;

    double currentSampleRate = 48000.0;
    int    currentBlockSize  = 512;
    std::atomic<int> lastNote { -1 };
    std::atomic<float> masterGain { 0.8333333f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

} // namespace bfka
