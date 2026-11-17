#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "core/KickState.h"   // StretchMode (B-43)

namespace bfka
{

/**
 * Kurvenform einer Fade-Strecke.
 *
 * Die Namen entsprechen den fuenf Fade-Modi des Referenzprodukts (Rechtsklick im
 * Visualiser: Linear / Curve / S-Curve / Fast / Slow). Jede Kurve liefert fuer den
 * normierten Fade-Fortschritt s = 0 den Wert 0, fuer s = 1 den Wert 1 und ist
 * monoton steigend; unterschieden wird nur der Verlauf dazwischen.
 *
 * Die geschlossenen Formeln stehen in `docs/specs/voice-and-render.md`, Abschnitt 3.
 */
enum class FadeShape
{
    linear,   ///< g(s) = s
    curve,    ///< Potenzkurve g(s) = s^p, p = 4^amount  (amount -1..+1)
    sCurve,   ///< Smoothstep g(s) = u^2 (3 - 2u) mit u = s^p
    fast,     ///< g(s) = (1 - e^(-k s)) / (1 - e^(-k)), k = 4 * 2^amount
    slow      ///< g(s) = ln (1 + c s) / ln (1 + c), c = 4 * 4^amount
};

/**
 * Eine Fade-Strecke eines Slots.
 *
 * `lengthSeconds` ist die Dauer des Fades in Sekunden, bezogen auf die **getrimmte
 * Slot-Laenge**: der Aufrufer bildet "Fade In = 50 %" also auf
 * `0,5 * getrimmte Dauer` ab. Ein Wert von 0 schaltet den Fade ab. Werte groesser
 * als die Slot-Laenge werden auf die Slot-Laenge geklemmt.
 */
struct FadeSpec
{
    double    lengthSeconds = 0.0;
    FadeShape shape         = FadeShape::linear;
    float     shapeAmount   = 0.0f;   ///< Biegeparameter -1..+1 (nur curve/sCurve/fast/slow)
};

/**
 * Alle Einstellungen einer Voice.
 *
 * Die Werte kommen aus dem Kick-Zustand: `pos` -> `positionSeconds`,
 * `trimstart`/`trimend` -> `trimStartSeconds`/`trimEndSeconds`, `fadein`/`fadeout`
 * -> `fadeIn`/`fadeOut` (L = Prozent * getrimmte Dauer), `vol` -> `gainLinear`,
 * `mute`/`solo` direkt, `pitch`/`stretch` und die Betriebsart aus dem
 * Kick-Zustand (B-42/B-43).
 */
struct VoiceSettings
{
    /** Startversatz der Wiedergabe in Sekunden (aus `pos`); negativ = 0. */
    double positionSeconds = 0.0;

    /** Beginn des gelesenen Bereichs, in Sekunden ab Sample-Anfang. */
    double trimStartSeconds = 0.0;

    /** Ende des gelesenen Bereichs, in Sekunden ab Sample-Anfang.
        0 oder kleiner als der Start bedeutet "bis zum Ende des Samples". */
    double trimEndSeconds = 0.0;

    FadeSpec fadeIn;
    FadeSpec fadeOut;

    float gainLinear = 1.0f;   ///< linearer Pegel (aus `vol`)

    /** Stummschaltung: die Voice wird gar nicht erst gestartet. */
    bool mute = false;

    /** Solo wird hier nur mitgefuehrt. Die Aufloesung braucht die Summe aller
        Slots und passiert in `KickRenderer::noteOn()`. */
    bool solo = false;

    /** Pitch in Halbtoenen, -24..+24 (B-42). Bei KEEP PITCH laengenneutral. */
    double pitchSemitones = 0.0;

    /** Zeitfaktor 0.25..8.0; > 1 = laenger (B-42). */
    double stretchFactor  = 1.0;

    /** Wie Pitch/Stretch wirken (B-43). */
    StretchMode mode      = StretchMode::keepPitch;
};

/**
 * Eine Sample-Voice: liest einen getrimmten Ausschnitt eines Samples, verschiebt
 * ihn um `positionSeconds` auf der Kick-Zeitachse, blendet ein/aus und addiert
 * das Ergebnis auf einen Zielpuffer.
 *
 * Signalweg je Ausgabe-Sample:
 * @code
 *   Quelle[trimStart + t*step] -> lineare Interpolation
 *      * Fade-In * Fade-Out * gainLinear
 *      -> ADDITION auf target (nie ueberschreiben)
 * @endcode
 *
 * Threading-Vertrag:
 *  - `setSource()`, `prepare()`, `setSettings()`, `reset()`, `startAt()` laufen auf
 *    dem Message-Thread (oder zu Beginn eines Blocks) und duerfen nicht parallel zu
 *    `renderNextBlock()` laufen.
 *  - `renderNextBlock()` laeuft auf dem Audio-Thread und allokiert nicht, loggt
 *    nicht und nimmt keine Sperre: die Voice haelt **keinen** eigenen Puffer,
 *    sie liest direkt aus dem Quellbuffer und schreibt additiv in das Ziel.
 *
 * Der Quellbuffer wird **nicht** kopiert und nicht besessen: er muss die Voice
 * ueberleben und darf waehrend der Wiedergabe nicht veraendert oder neu
 * allokiert werden (Library-Puffer sind nach dem Laden stabil).
 */
class SampleVoice
{
public:
    /** Mindestlaenge der Ein-/Ausblendrampe gegen Klicks, in Samples. */
    static constexpr int kDeclickSamples = 32;

    /** Hoechste Kanalzahl, die die Voice bedient (Quelle und Ziel). */
    static constexpr int kMaxChannels = 2;

    /**
     * Fade-Kurve: normierter Fortschritt 0..1 -> Verstaerkungsfaktor 0..1.

     * @param shape   Kurvenform
     * @param t       Fortschritt innerhalb der Fade-Strecke (wird auf 0..1 geklemmt)
     * @param amount  Biegeparameter -1..+1 (wird geklemmt)
     */
    static float shapeGain (FadeShape shape, float t, float amount) noexcept;

    SampleVoice() = default;

    // --- Konfiguration -----------------------------------------------------
    /** Setzt den Quellbuffer (nicht besessen, nullptr = keine Wiedergabe). */
    void setSource (const juce::AudioBuffer<float>* sample, double sourceSampleRate);

    /** Setzt die Ausgaberate; `maxBlockSize` wird nicht benoetigt (kein Puffer). */
    void prepare (double outputSampleRate, int maxBlockSize);

    /** Uebernimmt die Slot-Einstellungen und rechnet die Fenster neu. */
    void setSettings (const VoiceSettings& newSettings);

    const VoiceSettings& settings() const noexcept { return voiceSettings; }

    /**
     * Uebergibt einen vorgerenderten Puffer (B-43, KEEP PITCH).
     * Der Puffer ist NICHT besessen und muss die Voice ueberleben. Bei
     * `mode == StretchMode::keepPitch` liest die Voice ausschliesslich diesen
     * Puffer und wendet pitchRatio/stretchRatio NICHT erneut an (der Puffer
     * enthaelt beides schon). `nullptr` schaltet zurueck auf die Rohquelle.
     */
    void setPreRendered (const juce::AudioBuffer<float>* buffer, double sampleRate);

    /** true, wenn die Tonhoehe wirklich erhalten bleibt (KEEP PITCH mit Puffer). */
    bool isPitchAccurate() const noexcept { return pitchAccurate; }

    // --- Wiedergabe --------------------------------------------------------
    /**
     * Startet die Voice zum Zeitpunkt `outputSamplePosition` der lokalen Sample-Zeit
     * dieser Voice (0 = sofort beim naechsten `renderNextBlock`). Der
     * Kick-Renderer ruft immer `startAt (0)` und beginnt mit dem naechsten Block.
     */
    void startAt (juce::int64 outputSamplePosition);

    /** Haelt die Voice an (kein Allokieren). */
    void reset();

    /** Laeuft die Voice gerade? */
    bool isActive() const noexcept { return active; }

    // --- Abgeleitete Werte (nach setSource/setSettings/startAt gueltig) -----
    /** Laenge des getrimmten Bereichs in Sekunden (Ausgabezeit). */
    double lengthSeconds() const noexcept { return voiceLengthSeconds; }

    /** Laenge des getrimmten Bereichs in Ausgabe-Samples (aufgerundet). */
    juce::int64 lengthSamples() const noexcept { return voiceLengthSamples; }

    /** Quell-Samples je Ausgabe-Sample (inkl. Erweiterungspunkt Pitch/Stretch). */
    double readStep() const noexcept { return stepPerOutputSample; }

    /** true, wenn Quelle und Trim-Fenster eine Wiedergabe zulassen. */
    bool hasPlayableWindow() const noexcept { return playable; }

    // --- Audio -------------------------------------------------------------
    /**
     * Addiert den Beitrag der Voice in `target` (additiv, NICHT ueberschreibend).

     * Mono-Quellen werden auf beide Kanaele verteilt; ein Mono-Ziel bekommt bei
     * Stereo-Quellen den Mittelwert aus links und rechts. Kanaele des Ziels
     * jenseits der Quellkanalzahl erhalten den letzten Quellkanal.

     * Allokationsfrei, ohne Log, ohne Sperre.
     */
    void renderNextBlock (juce::AudioBuffer<float>& target, int startSample, int numSamples);

private:
    void  updateDerived() noexcept;
    float gainForSample (double timeInVoice, juce::int64 sampleInVoice) const noexcept;

    // --- Quelle und Raten --------------------------------------------------
    const juce::AudioBuffer<float>* source = nullptr;
    double sourceRate = 48000.0;
    double outputRate = 48000.0;

    VoiceSettings voiceSettings;

    // --- Abgeleitete Werte -------------------------------------------------
    double      stepPerOutputSample = 1.0;   ///< Quell-Samples je Ausgabe-Sample
    double      readStartSourcePos  = 0.0;   ///< erster gelesener Quell-Index
    double      voiceLengthSeconds  = 0.0;   ///< Laenge des Fensters in Ausgabezeit
    juce::int64 voiceLengthSamples  = 0;     ///< Laenge in Ausgabe-Samples
    juce::int64 startDelaySamples   = 0;     ///< Verzoegerung aus `positionSeconds`
    double      fadeInSeconds       = 0.0;   ///< Dauer des Fades, geklemmt
    double      fadeOutSeconds      = 0.0;   ///< Dauer des Fades, geklemmt
    bool        playable            = false;

    // --- Laufzeit ----------------------------------------------------------
    juce::int64 elapsed = 0;      ///< Ausgabe-Samples seit startAt()
    bool        active  = false;  ///< wird gerade wiedergegeben

    // --- Erweiterungspunkt B-42/B-43 (Pitch/Stretch) -----------------------
    // `pitchRatio`/`stretchRatio` wirken ausschliesslich in updateDerived() auf
    // `stepPerOutputSample` und damit auf Lesegeschwindigkeit UND Laenge. Bei
    // KEEP PITCH mit vorgerendertem Puffer werden beide auf 1,0 gesetzt: der
    // Puffer enthaelt Pitch und Stretch bereits. Siehe
    // docs/specs/voice-and-render.md, Abschnitt 9.
    double pitchRatio   = 1.0;   ///< 2^(Halbtoene/12); B-42
    double stretchRatio = 1.0;   ///< Zeitfaktor >1 = laenger; B-42

    const juce::AudioBuffer<float>* preRendered = nullptr;
    double preRenderedRate = 0.0;
    const juce::AudioBuffer<float>* activeSource = nullptr;   ///< effektive Quelle
    double activeRate = 48000.0;                              ///< Rate der effektiven Quelle
    bool   pitchAccurate = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleVoice)
};

} // namespace bfka
