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
     * Baut einen hoerbaren Test-Kick aus der Library: Punch, Rumble und Click.

     * Dient nur der Entwicklung und dem A/B-Vergleich — nicht der 1:1-Wiedergabe.
     * @returns false, wenn die Library nicht gefunden wurde.
     */
    bool buildDemoKick (const juce::File& contentRoot,
                        KickRenderer& renderer,
                        juce::StringArray& warnings);

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

    juce::AudioFormatManager formats;

    /** Die geladenen Audiodaten muessen den Renderer ueberleben. */
    std::array<std::unique_ptr<juce::AudioBuffer<float>>, (size_t) KickState::kNumSlots> buffers;
    std::array<double, (size_t) KickState::kNumSlots> sourceRates {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickAssembler)
};

} // namespace bfka
