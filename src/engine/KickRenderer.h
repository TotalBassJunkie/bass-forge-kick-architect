#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

#include "engine/SampleVoice.h"

namespace bfka
{

/**
 * Einstellungen des Kick-Renderers.
 *
 * `bpm` kommt aus dem Transport (FOLLOW DAW oder MANUAL), `cutToOneBeat`
 * entspricht dem Settings-Eintrag KICK LENGTH (1 BEAT vs. FULL SAMPLE).
 */
struct RenderSettings
{
    /** Tempo des Kicks; bestimmt die Beat-Laenge (60/bpm). */
    double bpm = 150.0;

    /** true = nach 60/bpm Sekunden ist der Kick zu Ende (KICK LENGTH 1 BEAT). */
    bool cutToOneBeat = true;

    /** Dauer des weichen Ausblendens am Beat-Ende (nur bei cutToOneBeat). */
    double tailFadeSeconds = 0.01;

    /** Autogain ein/aus (Settings-Eintrag AUTO GAIN). */
    bool autoGain = true;

    /** Zielpegel des "Bodys" in dBFS (0 dB = Vollaussteuerung). */
    float autoGainTargetDb = 0.0f;
};

/**
 * Der Kick-Renderer: summiert die zwoelf Sample-Voices zum fertigen Kick und
 * uebernimmt alles, was nicht Voice-DSP ist.
 *
 * Verantwortung je Block:
 * @code
 *   1. Scratch leeren, alle aktiven Voices additiv hineinrendern
 *   2. Body-RMS im Fenster 20..120 ms messen (VOR der Autogain-Stufe)
 *   3. Kick-Laenge (cutToOneBeat) und Autogain als Huelle aufmultiplizieren
 *   4. Scratch additiv in das Ziel addieren
 * @endcode
 *
 * Solo-Logik: Ist irgendein belegter Slot auf Solo, klingen nur Solo-Slots.
 * Mute schlaegt Solo (ein stummer Solo-Slot bleibt stumm).
 *
 * Threading-Vertrag:
 *  - `prepare()`, `reset()`, `setSlot()`, `clearSlot()`, `setRenderSettings()`
 *    und `noteOn()` laufen auf dem Message-Thread und duerfen allokieren; sie
 *    duerfen nicht parallel zu `renderNextBlock()` laufen.
 *  - `renderNextBlock()` laeuft auf dem Audio-Thread und allokiert nicht, loggt
 *    nicht und nimmt keine Sperre (der Scratch-Puffer wird in `prepare()`
 *    angelegt).
 */
class KickRenderer
{
public:
    /** Anzahl der Slots (wie KickState). */
    static constexpr int kNumSlots = 12;

    /** Interner Summenbus: der Kick ist stereo. */
    static constexpr int kChannels = 2;

    /** Plausibles Tempo-Fenster; schuetzt vor Division durch 0 und Unsinn. */
    static constexpr double kMinBpm = 20.0;
    static constexpr double kMaxBpm = 960.0;

    /** Messfenster des Autogains ("Body", bewusst ohne Transientenspitze). */
    static constexpr double kBodyWindowStartSeconds = 0.020;
    static constexpr double kBodyWindowEndSeconds   = 0.120;

    /** Mindestdauer der Autogain-Rampe (Glättung ueber Bloecke). */
    static constexpr double kAutoGainRampSeconds = 0.020;

    /** Grenzen der Autogain-Korrektur; ausserhalb wird geklemmt. */
    static constexpr float kMaxAutoGainDb = 24.0f;

    KickRenderer() = default;

    // --- Lebenszyklus ------------------------------------------------------
    /** Audio-Vorbereitung; legt den Summenpuffer an. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Haelt die Wiedergabe an und setzt die Messung zurueck (kein Allokieren). */
    void reset();

    bool   isPrepared() const noexcept { return prepared; }
    double sampleRate() const noexcept { return preparedSampleRate; }

    // --- Slots -------------------------------------------------------------
    /**
     * Belegt einen Slot. Der Quellbuffer wird NICHT kopiert und muss die Voice
     * ueberleben (nullptr = Slot leeren).
     */
    void setSlot (int slotIndex,
                  const juce::AudioBuffer<float>* sample,
                  double sourceSampleRate,
                  const VoiceSettings& settings);

    /** Leert einen Slot. */
    void clearSlot (int slotIndex);

    /** Einstellungen eines Slots (ungueltiger Index liefert eine leere Vorlage). */
    const VoiceSettings& slotSettings (int slotIndex) const noexcept;

    /**
     * Aendert nur die Einstellungen eines belegten Slots (B-154).
     *
     * Fuer Regler-Aenderungen aus der KICK-Seite: die Quelle bleibt, nur Vol,
     * Position, Trims, Fades und Mute/Solo werden uebernommen. Eine laufende
     * Voice rechnet ab dem naechsten Sample mit dem neuen Fenster; `mute` und
     * `solo` wirken beim naechsten `noteOn()`.
     *
     * Laeuft auf dem Message-Thread, allokiert nicht.
     */
    void setSlotSettings (int slotIndex, const VoiceSettings& newSettings);

    /** Ist der Slot belegt? */
    bool isSlotFilled (int slotIndex) const noexcept;

    // --- Einstellungen -----------------------------------------------------
    void setRenderSettings (const RenderSettings& newSettings);
    const RenderSettings& renderSettings() const noexcept { return settings; }

    // --- Wiedergabe --------------------------------------------------------
    /** Startet alle klingenden Voices bei 0 (Trigger des Kicks). */
    void noteOn();

    /** Addiert den naechsten Block des Kicks in `target` (additiv). */
    void renderNextBlock (juce::AudioBuffer<float>& target, int startSample, int numSamples);

    /** Laeuft gerade ein Kick? */
    bool isPlaying() const noexcept { return playing; }

    /** Anzahl der gerade klingenden Voices. */
    int numActiveVoices() const noexcept;

    // --- Laenge ------------------------------------------------------------
    /** Beat-Laenge in Sekunden (60/bpm), geklemmt auf [kMinBpm, kMaxBpm]. */
    double beatLengthSeconds() const noexcept;

    /**
     * Laenge des Kicks in Sekunden, wie er gerade klingt (B-147).
     *
     * Bei `cutToOneBeat` ist das die Beat-Laenge; bei KICK LENGTH = FULL SAMPLE
     * die laengste belegte Voice (mindestens die Beat-Laenge, damit die
     * Hüllkurven von `shaper` nicht in die Stille laufen).
     */
    double kickLengthSeconds() const noexcept;

    /** Beat-Laenge in Ausgabe-Samples. */
    juce::int64 beatLengthSamples() const noexcept { return beatSamples; }

    // --- Autogain ----------------------------------------------------------
    /** Aktuell angewandte Kick-Verstaerkung (linear, rampenbehaftet). */
    float autoGainLinear() const noexcept { return autoGainCurrent; }

    /** Zielverstaerkung der laufenden Rampe (linear). */
    float autoGainTargetLinear() const noexcept { return autoGainTarget; }

    /** Wurde fuer den letzten Kick ein Body-Pegel gemessen? */
    bool hasBodyMeasurement() const noexcept { return bodyMeasured; }

    /** Gemessener Body-RMS des letzten Kicks in dBFS (0, wenn nicht gemessen). */
    float bodyRmsDb() const noexcept { return bodyRmsDbValue; }

    /** Setzt die Autogain-Kalibrierung zurueck (Verstaerkung bleibt). */
    void resetAutoGain() noexcept;

private:
    void updateTimeline() noexcept;
    void updateSoloState() noexcept;
    void resetBodyMeasurement() noexcept;
    void accumulateBodyEnergy (int numSamples) noexcept;
    void finaliseBodyMeasurement() noexcept;
    void startAutoGainRamp (float targetLinear) noexcept;
    void applyOutputGain (int numSamples) noexcept;
    void addToTarget (juce::AudioBuffer<float>& target, int startSample, int numSamples) const noexcept;
    void finishKick() noexcept;
    bool anyVoiceActive() const noexcept;

    // --- Slots -------------------------------------------------------------
    std::array<SampleVoice, (size_t) kNumSlots>   voices;
    std::array<VoiceSettings, (size_t) kNumSlots> voiceSettings {};
    std::array<bool, (size_t) kNumSlots>          slotHasSample {};
    bool anySolo = false;

    // --- Konfiguration -----------------------------------------------------
    RenderSettings settings;

    /** Summenpuffer, in prepare() angelegt; nie im Audio-Thread vergroessert. */
    juce::AudioBuffer<float> scratch;

    double preparedSampleRate = 48000.0;
    int    preparedBlockSize  = 512;
    bool   prepared           = false;

    // --- Laufzeit ----------------------------------------------------------
    bool        playing             = false;
    juce::int64 samplesSinceTrigger = 0;

    juce::int64 beatSamples      = 0;   ///< Laenge des Kicks in Samples
    juce::int64 tailFadeSamples  = 0;   ///< Dauer des Ausblendens am Beat-Ende
    juce::int64 bodyStartSample  = 0;   ///< Beginn des Messfensters
    juce::int64 bodyEndSample    = 0;   ///< Ende des Messfensters (exklusiv)

    // --- Autogain-Messung --------------------------------------------------
    double      bodySumSquares = 0.0;   ///< Summe der Quadrate (alle Kanaele)
    juce::int64 bodyValues     = 0;     ///< Anzahl ausgewerteter Werte
    bool        bodyFinalised  = false; ///< Messung abgeschlossen
    bool        bodyMeasured   = false; ///< gueltiger Messwert vorhanden
    float       bodyRmsDbValue = 0.0f;  ///< Messwert des letzten Kicks

    // --- Autogain-Stufe ----------------------------------------------------
    float autoGainCurrent       = 1.0f;
    float autoGainTarget        = 1.0f;
    float autoGainRampStep      = 0.0f;
    int   autoGainRampRemaining = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickRenderer)
};

} // namespace bfka
