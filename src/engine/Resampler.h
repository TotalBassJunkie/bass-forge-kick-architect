#pragma once

#include <cmath>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Interpolierender Resampler (Laenge und Tonhoehe gemeinsam).
 *
 * Der Baustein ist bewusst allgemein gehalten: er traegt den REPITCH-Pfad des
 * Time-Stretchers und spaeter die Pitch-Shift-Stufe (Ticket B-44), die aus
 * "Resampling + WSOLA-Kompensation" besteht.
 *
 * `ratio` ist immer das Verhaeltnis Ausgabelaenge / Eingabelaenge:
 *   ratio = 2.0 -> doppelt so lang, eine Oktave tiefer
 *   ratio = 0.5 -> halb so lang, eine Oktave hoeher
 *
 * Beim Verkuerzen (Hochstimmen) laufen Frequenzen oberhalb der neuen
 * Nyquist-Frequenz in das Band zurueck (Aliasing). Deshalb laeuft dann ein
 * zweistufiger Biquad-Tiefpass (24 dB/Okt) ueber die Eingabe. Beim Verlaengern
 * ist kein Vorfilter noetig.
 *
 * Der Filterzustand laeuft ueber Aufrufe hinweg weiter (Blockverarbeitung).
 * Wer einen neuen Stream beginnt, ruft zuerst `reset()`.
 */
namespace bfka
{

/** Interpolationsart des Resamplers. */
enum class ResamplerQuality
{
    linear,   // zwei Stuetzstellen: billig, aber dumpfer und mit Aliasing
    cubic     // Catmull-Rom, vier Stuetzstellen: Standard
};

class Resampler
{
private:
    static constexpr int    kStages          = 2;      // zwei Biquads = 24 dB/Okt
    static constexpr double kAntiAliasCutoff = 0.45;   // Anteil der neuen Nyquist-Frequenz

    struct BiquadState
    {
        double s1 = 0.0;
        double s2 = 0.0;
    };

    struct BiquadCoeffs
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
    };

public:
    /** Obergrenze der Ausgabelaenge (Notbremse gegen Speicher-Overflow). */
    static constexpr int maxOutputLength = 1 << 26;   // 67.1 Mio. Samples

    /** Setzt den Filterzustand zurueck und legt die Kanalzahl fest. */
    void reset (int numChannelsToUse)
    {
        numChannels = juce::jmax (1, numChannelsToUse);
        states.assign ((size_t) numChannels * (size_t) kStages, BiquadState {});
    }

    void setQuality (ResamplerQuality newQuality) noexcept { quality = newQuality; }
    ResamplerQuality getQuality() const noexcept           { return quality; }

    void setAntiAliasEnabled (bool shouldBeEnabled) noexcept { antiAliasEnabled = shouldBeEnabled; }
    bool isAntiAliasEnabled() const noexcept                 { return antiAliasEnabled; }

    /** Ganzzahlige Ausgabelaenge fuer `ratio` (kaufmaennisch gerundet, mind. 1). */
    static int outputLengthFor (int inputLength, double ratio) noexcept
    {
        if (inputLength <= 0)
            return 0;

        if (! std::isfinite (ratio) || ratio <= 0.0)
            return inputLength;

        const double exact = std::floor ((double) inputLength * ratio + 0.5);

        if (exact < 1.0)
            return 1;

        if (exact > (double) maxOutputLength)
            return maxOutputLength;

        return (int) exact;
    }

    /**
     * Resampelt `in` nach `out`; `out` wird dimensioniert.

     * `ratio` darf nicht 0 oder negativ sein - dann bleibt die Laenge gleich.
     * `in` und `out` duerfen derselbe Puffer sein (wird abgefangen, kostet
     * dann aber eine Kopie).
     */
    void process (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out, double ratio)
    {
        processToLength (in, out, outputLengthFor (in.getNumSamples(), ratio));
    }

    /**
     * Wie process(), aber mit exakt vorgegebener Ausgabelaenge.

     * Der Offline-Renderer braucht die Laenge samplegenau (KICK LENGTH, ALIGN),
     * deshalb setzt er sie selbst und nicht ueber ein gerundetes Verhaeltnis.
     */
    void processToLength (const juce::AudioBuffer<float>& in,
                          juce::AudioBuffer<float>& out,
                          int outputLength)
    {
        const int numIn  = in.getNumSamples();
        const int numCh  = in.getNumChannels();
        const int outLen = juce::jlimit (0, maxOutputLength, outputLength);

        // Gleicher Puffer fuer Ein- und Ausgabe: Inhalt retten, bevor dimensioniert wird.
        juce::AudioBuffer<float> aliasCopy;
        const juce::AudioBuffer<float>* source = &in;

        if (&in == &out)
        {
            aliasCopy.makeCopyOf (in);
            source = &aliasCopy;
        }

        if (numCh <= 0 || numIn <= 0 || outLen <= 0)
        {
            out.setSize (juce::jmax (1, numCh), juce::jmax (0, outLen), false, true, true);
            out.clear();
            return;
        }

        out.setSize (numCh, outLen, false, true, true);

        const double ratio = (double) outLen / (double) numIn;

        // Anti-Alias-Vorfilter auf der Eingangsseite (nur beim Verkuerzen).
        juce::AudioBuffer<float> filtered;

        if (antiAliasEnabled && ratio < 0.99)
        {
            if ((int) states.size() != numCh * kStages)
                reset (numCh);

            filtered.makeCopyOf (*source);
            applyAntiAlias (filtered, kAntiAliasCutoff * ratio);
            source = &filtered;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* inData    = source->getReadPointer (ch);
            float*       outData   = out.getWritePointer (ch);
            const int    lastIndex = numIn - 1;

            for (int i = 0; i < outLen; ++i)
            {
                // Position in der Quelle. Division statt Inkrement, damit die
                // Abbildung exakt und reproduzierbar bleibt.
                const double position = (double) i / ratio;
                const int    index    = (int) std::floor (position);
                const double fraction = position - (double) index;

                if (quality == ResamplerQuality::linear)
                {
                    const double a = (double) inData[juce::jlimit (0, lastIndex, index)];
                    const double b = (double) inData[juce::jlimit (0, lastIndex, index + 1)];

                    outData[i] = (float) ((1.0 - fraction) * a + fraction * b);
                }
                else
                {
                    const double p0 = (double) inData[juce::jlimit (0, lastIndex, index - 1)];
                    const double p1 = (double) inData[juce::jlimit (0, lastIndex, index)];
                    const double p2 = (double) inData[juce::jlimit (0, lastIndex, index + 1)];
                    const double p3 = (double) inData[juce::jlimit (0, lastIndex, index + 2)];

                    outData[i] = (float) catmullRom (p0, p1, p2, p3, fraction);
                }
            }
        }
    }

private:
    /** Kubische Interpolation nach Catmull-Rom (tension 0,5). */
    static double catmullRom (double p0, double p1, double p2, double p3, double t) noexcept
    {
        const double t2 = t * t;
        const double t3 = t2 * t;

        return 0.5 * ((2.0 * p1)
                      + (-p0 + p2) * t
                      + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
                      + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
    }

    /** RBJ-Tiefpass, Koeffizienten normiert (a0 = 1). */
    static BiquadCoeffs makeLowpass (double normalisedCutoff, double q) noexcept
    {
        const double guard  = juce::jlimit (1.0e-5, 0.49, normalisedCutoff);
        const double w0     = 2.0 * juce::MathConstants<double>::pi * guard;
        const double cosW0  = std::cos (w0);
        const double alpha  = std::sin (w0) / (2.0 * q);
        const double a0     = 1.0 + alpha;

        BiquadCoeffs c;
        c.b0 = (1.0 - cosW0) * 0.5 / a0;
        c.b1 = (1.0 - cosW0) / a0;
        c.b2 = c.b0;
        c.a1 = (-2.0 * cosW0) / a0;
        c.a2 = (1.0 - alpha) / a0;
        return c;
    }

    /**
     * Filtert `buffer` in-place.

     * `normalisedCutoff` ist die Grenzfrequenz in Zyklen je Eingabe-Sample
     * (0,5 = Nyquist). Der Bezug auf die Eingangsrate macht den Aufruf
     * unabhaengig von der Projekt-Abtastrate.
     */
    void applyAntiAlias (juce::AudioBuffer<float>& buffer, double normalisedCutoff)
    {
        const int numCh = buffer.getNumChannels();
        const int n     = buffer.getNumSamples();

        if (numCh <= 0 || n <= 0)
            return;

        if ((int) states.size() != numCh * kStages)
            reset (numCh);

        const BiquadCoeffs c = makeLowpass (normalisedCutoff, 0.70710678118654752);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int stage = 0; stage < kStages; ++stage)
            {
                auto& state = states[(size_t) ch * (size_t) kStages + (size_t) stage];

                for (int i = 0; i < n; ++i)
                {
                    const double x = (double) data[i];
                    const double y = c.b0 * x + state.s1;

                    state.s1 = c.b1 * x - c.a1 * y + state.s2;
                    state.s2 = c.b2 * x - c.a2 * y;

                    data[i] = (float) y;
                }
            }
        }
    }

    ResamplerQuality quality { ResamplerQuality::cubic };
    bool             antiAliasEnabled { true };
    int              numChannels { 0 };
    std::vector<BiquadState> states;
};

} // namespace bfka
