#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/Resampler.h"

/**
 * Zeitdehnung mit Waveform-Similarity Overlap-Add (WSOLA).
 *
 * Eigene, lizenzreine Implementierung nach Verhelst/Roelands - keine
 * Fremdbibliothek (kein Rubber Band, kein SoundTouch).
 *
 * Zwei Betriebsarten, wie im Original getrennt:
 *   KEEP PITCH  Laenge aendern, Tonhoehe halten        -> WSOLA
 *   REPITCH     Laenge und Tonhoehe gemeinsam aendern  -> Resampling
 *
 * `process()` ist ein Offline-Renderer-Baustein: es laeuft NICHT im
 * Audio-Thread und darf allokieren. Es ist zustandsfrei bezueglich der
 * Audiodaten (jeder Aufruf startet mit frischem Filterzustand), damit
 * dieselbe Eingabe mit demselben Faktor bitgleich dieselbe Ausgabe liefert.
 */
namespace bfka
{

/**
 * Parameter der WSOLA-Engine.

 * Alle Laengen sind Samples bei Quellrate. `prepare()` klemmt die Werte in
 * sinnvolle Bereiche (siehe docs/specs/stretching.md, Abschnitt 3).
 */
struct WsolaSettings
{
    int   frameSize    = 2048;   // Analysefenster (Samples bei Quellrate)
    int   overlapSize  = 512;    // Ueberlappung (Samples), maximal frameSize/2
    int   searchRadius = 512;    // Suchbereich der Aehnlichkeitssuche (Samples)
    float tolerance    = 0.2f;   // Gewicht der Laufzeitbindung (0 = freie Suche)
};

class WsolaStretcher
{
public:
    /** Erlaubter Streckfaktor; alles ausserhalb wird geklemmt. */
    static constexpr double minFactor = 0.25;
    static constexpr double maxFactor = 8.0;

    WsolaStretcher() = default;

    /** Legt Rate, Kanalzahl und Parameter fest und baut das Hann-Fenster. */
    void prepare (double sampleRate, int numChannels, const WsolaSettings& = {});

    /** Setzt Diagnosewerte und den Resampler-Filterzustand zurueck. */
    void reset();

    /** Streckt `input` um `factor` (0.25 .. 8.0). factor > 1 = laenger.
        Ergebnis wird in `output` geschrieben (wird bei Bedarf dimensioniert — diese
        Methode darf allokieren, sie laeuft NICHT im Audio-Thread). */
    void process (const juce::AudioBuffer<float>& input,
                  juce::AudioBuffer<float>& output,
                  double factor,
                  bool keepPitch);

    /** Nur Analyse/Diagnose: Korrelationsguete der letzten Grain-Suche (0..1).
        0 bedeutet "keine Suche erfolgt" (REPITCH, Faktor 1, Stille). */
    float lastSimilarity() const noexcept { return lastSimilarity_; }

    /** Mittel der Korrelationsguete ueber alle Suchen des letzten Aufrufs (0..1). */
    float meanSimilarity() const noexcept { return meanSimilarity_; }

    /** Ergebnislaenge fuer `factor` (geklemmt), kaufmaennisch gerundet. */
    static int outputLengthFor (int inputLength, double factor) noexcept;

    /** Klemmt `factor` auf minFactor .. maxFactor. */
    static double clampFactor (double factor) noexcept;

    /**
     * Vorgabe fuer die klassische WSOLA-Konfiguration mit 50 % Ueberlappung.

     * Die Struktur `WsolaSettings` hat als Vorgabe 512 Samples Ueberlappung bei
     * 2048er Fenster (= 25 %). Fuer Bass-Anteile empfehlen wir 50 %, weil das
     * Kreuzblendenfenster dann laenger als eine Periode typischer Sub-Baesse ist.
     */
    static WsolaSettings fiftyPercentOverlap (int frameSize = 2048,
                                              int searchRadius = 512,
                                              float tolerance = 0.2f) noexcept
    {
        WsolaSettings settings;

        settings.frameSize    = frameSize;
        settings.overlapSize  = juce::jmax (1, frameSize / 2);
        settings.searchRadius = searchRadius;
        settings.tolerance    = tolerance;
        return settings;
    }

    const WsolaSettings& settings() const noexcept { return settings_; }
    double sampleRate() const noexcept             { return sampleRate_; }
    int numChannels() const noexcept               { return numChannels_; }

private:
    double        sampleRate_ { 48000.0 };
    int           numChannels_ { 2 };
    WsolaSettings settings_ { };
    bool          prepared_ { false };
    std::vector<float> hann_;      ///< Hann-Fenster fuer settings_.frameSize

    Resampler resampler_;

    float lastSimilarity_ { 0.0f };
    float meanSimilarity_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WsolaStretcher)
};

} // namespace bfka
