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
