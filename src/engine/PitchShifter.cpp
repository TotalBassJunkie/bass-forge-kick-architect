#include "engine/PitchShifter.h"

#include <cmath>

namespace bfka
{

// ---------------------------------------------------------------------------
// PitchShifter (Ticket B-44)
// ---------------------------------------------------------------------------

double PitchShifter::ratioForSemitones (double semitones) noexcept
{
    const double clamped = juce::jlimit (minSemitones, maxSemitones, semitones);
    return std::pow (2.0, clamped / 12.0);
}

void PitchShifter::prepare (double sampleRateToUse, int numChannelsToUse)
{
    sampleRate  = (sampleRateToUse > 0.0) ? sampleRateToUse : 48000.0;
    numChannels = juce::jmax (1, numChannelsToUse);

    resampler.reset (numChannels);
    stretcher.prepare (sampleRate, numChannels, WsolaStretcher::fiftyPercentOverlap());

    prepared = true;
}

void PitchShifter::reset()
{
    // Nur die Stufen zuruecksetzen; `prepared` bleibt, damit ein weiterer
    // process()-Aufruf ohne neues prepare() gueltig bleibt.
    resampler.reset (numChannels);
    stretcher.reset();
}

int PitchShifter::outputLengthFor (int inputLength) noexcept
{
    return juce::jmax (0, inputLength);
}

void PitchShifter::process (const juce::AudioBuffer<float>& input,
                            juce::AudioBuffer<float>& output,
                            double semitones)
{
    const int numIn = input.getNumSamples();
    const int numCh = juce::jmax (1, input.getNumChannels());

    // Ohne prepare() oder ohne Material gibt es nichts zu rechnen: die Ausgabe
    // ist eine Kopie der Eingabe (gleiche Laenge).
    if (! prepared || numIn <= 0)
    {
        output.makeCopyOf (input);
        return;
    }

    const double ratio = ratioForSemitones (semitones);
    const int    targetLength = numIn;

    // Stufe 1: Resampling um 1/r. Die Laenge sinkt auf L/r, die Tonhoehe steigt um r.
    juce::AudioBuffer<float> transposed;
    resampler.process (input, transposed, 1.0 / ratio);

    // Stufe 2: WSOLA mit factor = r bei erhaltenem Pitch. Die Laenge steigt
    // wieder auf (L/r) * r = L, die Tonhoehe bleibt um r verschoben.
    stretcher.process (transposed, output, ratio, true);

    // Laenge samplegenau auf die Eingabelaenge bringen: WSOLA rundet
    // kaufmaennisch, deshalb kann die Ausgabe um +-1 Sample abweichen.
    if (output.getNumSamples() != targetLength)
    {
        const int channels = juce::jmax (1, output.getNumChannels());

        // keepExistingContent = true, clearExtraSpace = true: beim Auffuellen
        // entsteht Stille, beim Abschneiden bleibt der Anfang erhalten.
        output.setSize (channels, targetLength, true, true, false);
    }

    // Sicherheitsnetz fuer den Fall, dass die Stufen keine Kanaele geliefert haben.
    if (output.getNumChannels() == 0)
    {
        output.setSize (numCh, targetLength, false, true, false);
        output.clear();
    }
}

} // namespace bfka
