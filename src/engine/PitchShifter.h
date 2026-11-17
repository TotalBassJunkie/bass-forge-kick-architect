#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/Resampler.h"
#include "engine/WsolaStretcher.h"

namespace bfka
{

/**
 * Laengenneutraler Pitch-Shift (Ticket B-44).
 *
 * Signalweg: Resampling um 2^(-semitones/12) (verkuerzt und transponiert),
 * danach WSOLA-Zeitdehnung um 2^(semitones/12) mit erhaltenem Pitch. Die
 * Ausgabelaenge entspricht damit der Eingabelaenge (Rundung +-1 Sample).
 *
 * Kein Echtzeit-Baustein: `process()` allokiert und laeuft NICHT im
 * Audio-Thread (Vorrendering, Export, ALIGN).
 */
class PitchShifter
{
public:
    /** Grenzen des Shift-Bereichs in Halbtoenen (wie ParameterRegistry). */
    static constexpr double minSemitones = -24.0;
    static constexpr double maxSemitones =  24.0;

    /** Frequenzverhaeltnis 2^(semitones/12), geklemmt auf den Bereich. */
    static double ratioForSemitones (double semitones) noexcept;

    PitchShifter() = default;

    /** Legt Rate und Kanalzahl fest und bereitet die Stufen vor. */
    void prepare (double sampleRateToUse, int numChannelsToUse);

    /** Setzt beide Stufen zurueck (neuer Stream). */
    void reset();

    /**
     * Verschiebt `input` um `semitones` und schreibt das Ergebnis nach
     * `output` (wird dimensioniert). Die Ausgabelaenge ist
     * `input.getNumSamples()` (+-1 durch Rundung).
     */
    void process (const juce::AudioBuffer<float>& input,
                  juce::AudioBuffer<float>& output,
                  double semitones);

    /** Ausgabelaenge fuer eine gegebene Eingabelaenge (Diagnose/Test). */
    static int outputLengthFor (int inputLength) noexcept;

private:
    Resampler      resampler;
    WsolaStretcher stretcher;
    double sampleRate = 48000.0;
    int    numChannels = 2;
    bool   prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShifter)
};

} // namespace bfka
