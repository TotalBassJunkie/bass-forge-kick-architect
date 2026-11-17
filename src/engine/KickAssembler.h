#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <array>
#include <memory>

#include "core/KickState.h"
#include "engine/KickRenderer.h"

namespace bfka
{

/**
 * Baut aus einem Kick-Zustand bzw. aus der Sample-Library spielbare Slots.

 * Der Renderer kennt nur Sekunden und lineare Verstaerkungen; dieses Objekt
 * uebersetzt die normalisierten Parameter des Zustands (`lN_vol`, `lN_trimstart`,
 * ...) in `VoiceSettings` und haelt die geladenen Audiodaten am Leben.
 *
 * Threading: alle Methoden laufen auf dem Message-Thread. Der Renderer darf
 * waehrend `loadFromState()`/`buildDemoKick()` nicht im Audio-Thread laufen —
 * der Aufrufer muss den Tausch entsprechend serialisieren.
 */
class KickAssembler
{
public:
    /**
     * Grobe Wellenform eines geladenen Samples (B-156).
     *
     * Der Visualiser braucht keine Sample-genauen Daten, sondern eine
     * Min/Max-Huelle fuer die Anzeige. Sie wird beim Laden mitberechnet und
     * haelt damit den Audio-Thread frei.
     */
    struct SampleEnvelope
    {
        static constexpr int kBuckets = 256;

        bool   valid = false;
        double lengthSeconds = 0.0;   ///< Laenge des Samples in Sekunden
        double peak = 0.0;            ///< groesster Betrag im Sample (linear)

        /** Min/Max je Zeitabschnitt, in Sample-Reihenfolge. */
        std::array<float, (size_t) kBuckets> minimum {};
        std::array<float, (size_t) kBuckets> maximum {};
    };

    KickAssembler();

    /**
     * Laedt alle Slots des Zustands, die eine Sample-Referenz tragen.

     * @param contentRoot Wurzel der Sample-Library (die WAVs liegen relativ dazu).
     * @returns Anzahl der tatsaechlich geladenen Slots.
     */
    int loadFromState (const KickState& state,
                       const juce::File& contentRoot,
                       KickRenderer& renderer,
                       juce::StringArray& warnings);

    /**
     * Laedt genau einen Slot aus dem Zustand (B-154).
     *
     * Der Weg fuer die KICK-Seite: Sample zuweisen, nur diesen Slot neu laden,
     * alle anderen bleiben unangetastet.
     */
    bool loadOneSlot (int slot,
                      const KickState& state,
                      const juce::File& contentRoot,
                      KickRenderer& renderer,
                      juce::StringArray& warnings);

    /** Leert einen Slot (Renderer und gepufferte Audiodaten). */
    void clearSlot (int slot, KickRenderer& renderer);

    /**
     * Uebernimmt nur die Slot-Parameter (Vol, Position, Trims, Fades, Mute/Solo)
     * fuer einen bereits geladenen Slot in den Renderer (B-154).
     *
     * @returns false, wenn der Slot nicht geladen ist.
     */
    bool applySlotSettings (int slot, const KickState& state, KickRenderer& renderer);

    /** Wellenform eines Slots; `valid == false`, solange nichts geladen ist. */
    const SampleEnvelope& envelope (int slot) const noexcept;

    /** Laenge des geladenen Samples in Sekunden (0, wenn leer). */
    double slotLengthSeconds (int slot) const noexcept;

    /**
     * Baut einen hoerbaren Test-Kick aus der Library: Punch, Rumble und Click.

     * Dient nur der Entwicklung und dem A/B-Vergleich — nicht der 1:1-Wiedergabe.
     * @returns false, wenn die Library nicht gefunden wurde.
     */
    bool buildDemoKick (const juce::File& contentRoot,
                        KickRenderer& renderer,
                        juce::StringArray& warnings);

    /**
     * Sucht je Demo-Slot eine Datei in der Library (B-154).
     *
     * Der Weg fuer die KICK-Seite: erst die Referenzen in den Zustand schreiben,
     * dann ueber `loadOneSlot()` laden — so zeigen die Slot-Karten denselben
     * Stand, den der Renderer spielt.
     *
     * @param files  Ausgabe: Datei je Demo-Slot (`File()` = nichts gefunden).
     */
    static void findDemoSamples (const juce::File& contentRoot,
                                 std::array<juce::File, 3>& files);

    /** Uebersetzt die Slot-Parameter eines Slots in `VoiceSettings`. */
    static VoiceSettings voiceSettingsForSlot (const KickState& state,
                                               int slot,
                                               double trimmedLengthSeconds);

    /** Datei zu einer Sample-Referenz aufloesen (relativ zur Library, sonst absolut). */
    static juce::File resolveSampleFile (const SampleRef& ref, const juce::File& contentRoot);

private:
    bool loadSlot (int slot,
                   const juce::File& file,
                   const KickState& state,
                   KickRenderer& renderer,
                   juce::StringArray& warnings);

    bool loadSlotWithSettings (int slot,
                               const juce::File& file,
                               const VoiceSettings& settings,
                               KickRenderer& renderer,
                               juce::StringArray& warnings);

    /** Sucht die erste WAV-Datei unterhalb eines Library-Unterordners. */
    static juce::File findFirstSample (const juce::File& contentRoot, const juce::String& subFolder);

    /** Rechnet die Anzeige-Huelle eines geladenen Samples. */
    static void buildEnvelope (const juce::AudioBuffer<float>& buffer,
                               double sampleRate,
                               SampleEnvelope& target);

    juce::AudioFormatManager formats;

    /** Die geladenen Audiodaten muessen den Renderer ueberleben. */
    std::array<std::unique_ptr<juce::AudioBuffer<float>>, (size_t) KickState::kNumSlots> buffers;
    std::array<double, (size_t) KickState::kNumSlots> sourceRates {};

    /** Anzeige-Huellen der geladenen Slots (B-156). */
    std::array<SampleEnvelope, (size_t) KickState::kNumSlots> envelopes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickAssembler)
};

} // namespace bfka
