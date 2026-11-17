#include "engine/WsolaStretcher.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "core/Log.h"

namespace bfka
{

namespace
{
    /** Kleinste sinnvolle Framegroesse; darunter traegt keine Ueberlappung. */
    constexpr int kMinFrameSize = 32;

    /** Unterhalb dieser Eingabelaenge gibt es kein verwertbares Fenster. */
    constexpr int kTinyInputLength = 8;

    /** Ab hier gilt eine Normierungssumme als "kein Signal" (verhindert 0/0 und Inf). */
    constexpr float kNormEpsilon = 1.0e-12f;

    /** Anzahl der Rasterpunkte der groben Aehnlichkeitssuche. */
    constexpr int kCoarseSteps = 32;

    /** Unter dieser Energie gilt ein Abschnitt als Stille (Korrelation unbrauchbar). */
    constexpr double kSilenceEnergy = 1.0e-24;

    /** Obergrenze der Ausgabelaenge (Notbremse gegen Speicher-Overflow). */
    constexpr int kMaxOutputLength = 1 << 26;

    void sanitiseSettings (WsolaSettings& settings)
    {
        settings.frameSize    = juce::jlimit (kMinFrameSize, 1 << 16, settings.frameSize);
        settings.overlapSize  = juce::jlimit (1, juce::jmax (1, settings.frameSize / 2),
                                             settings.overlapSize);
        settings.searchRadius = juce::jlimit (0, settings.frameSize, settings.searchRadius);
        settings.tolerance    = juce::jlimit (0.0f, 1.0f, settings.tolerance);
    }

    /**
     * Periodisches Hann-Fenster der Laenge `size`.

     * w[n] = 0,5 - 0,5 * cos (2*pi*n / size),  n = 0 .. size-1

     * Der Nenner ist `size` (nicht size-1), damit Nachbarfenster bei einem Hop
     * von size/2 exakt zu 1 summieren.
     */
    std::vector<float> makeHannWindow (int size)
    {
        const int length = juce::jmax (1, size);

        std::vector<float> window ((size_t) length, 0.0f);
        const double step = 2.0 * juce::MathConstants<double>::pi / (double) length;

        for (int i = 0; i < length; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (step * (double) i));

        return window;
    }

    /** Energie des Abschnitts [start, start+length) aus den Praefixsummen. */
    double energyIn (const std::vector<double>& prefixSquares, int start, int length) noexcept
    {
        const size_t from = (size_t) juce::jmax (0, start);
        const size_t to   = from + (size_t) juce::jmax (0, length);

        return prefixSquares[to] - prefixSquares[from];
    }

    struct GrainMatch
    {
        int    offset { 0 };        ///< Verschiebung gegenueber der Nominalposition
        double similarity { 0.0 };  ///< normierte Kreuzkorrelation (-1 .. 1)
    };

    /**
     * Sucht das Grain mit der besten Uebereinstimmung zur Ueberlappungszone des
     * vorigen Grains.

     * `templatePos` ist die Quellposition, an der der vorige Grain seine
     * Ueberlappungszone hatte; `nominalPos` die Position, die die lineare
     * Zeitabbildung verlangt. Gesucht wird im Bereich
     * nominalPos +/- radius, bewertet mit der normierten Kreuzkorrelation
     * ueber `overlapLength` Samples.

     * Die Suche laeuft zweistufig (Grob raster, dann sampleweise fein), damit
     * ein Kick nicht zehntausende Kandidaten durchrechnen muss. Sie ist
     * vollstaendig deterministisch: gleiche Eingabe, gleiche Entscheidung.
     */
    GrainMatch findBestGrain (const float* mono,
                              const std::vector<double>& prefixSquares,
                              int templatePos,
                              int nominalPos,
                              int overlapLength,
                              int radius,
                              double tolerance,
                              int paddedLength)
    {
        GrainMatch match;

        if (radius <= 0 || overlapLength <= 0)
            return match;

        if (templatePos < 0 || templatePos + overlapLength > paddedLength)
            return match;

        // Stille im Referenzfenster: es gibt nichts zu korrelieren -> Nominalposition.
        const double templateEnergy = energyIn (prefixSquares, templatePos, overlapLength);

        if (! (templateEnergy > kSilenceEnergy))
            return match;

        const double templateScale = 1.0 / std::sqrt (templateEnergy);
        const double radiusD       = (double) juce::jmax (1, radius);

        int    bestOffset = 0;
        double bestScore  = -1.0e30;
        double bestSim    = 0.0;

        const auto evaluate = [&] (int offset)
        {
            const int candidate = nominalPos + offset;

            if (candidate < 0 || candidate + overlapLength > paddedLength)
                return;

            const double candidateEnergy = energyIn (prefixSquares, candidate, overlapLength);

            double similarity = 0.0;

            if (candidateEnergy > kSilenceEnergy)
            {
                const float* reference = mono + templatePos;
                const float* test      = mono + candidate;

                double dot = 0.0;

                for (int n = 0; n < overlapLength; ++n)
                    dot += (double) reference[n] * (double) test[n];

                similarity = dot * templateScale / std::sqrt (candidateEnergy);
                similarity = juce::jlimit (-1.0, 1.0, similarity);
            }

            // Laufzeitbindung: ein weiter Sprung muss sich durch Aehnlichkeit
            // bezahlt machen. tolerance = 0 sucht frei, tolerance = 1 blockiert
            // Sprunge praktisch vollstaendig.
            const double normalised = (double) offset / radiusD;
            const double score      = similarity - tolerance * normalised * normalised;

            if (score > bestScore)
            {
                bestScore  = score;
                bestOffset = offset;
                bestSim    = similarity;
            }
        };

        // Grobe Stufe: beginnt bei 0 (Nominalposition), dann symmetrisch nach aussen.
        // Die Reihenfolge ist Teil der Determinismus-Zusage: bei Gleichstand gewinnt
        // der kleinere Betrag.
        const int step = juce::jmax (1, radius / kCoarseSteps);

        evaluate (0);

        for (int offset = step; offset <= radius; offset += step)
        {
            evaluate (offset);
            evaluate (-offset);
        }

        // Feine Stufe: Raster zwischen den Nachbarn der Grobsuche.
        const int fineLow  = juce::jmax (-radius, bestOffset - step + 1);
        const int fineHigh = juce::jmin (radius, bestOffset + step - 1);

        for (int offset = fineLow; offset <= fineHigh; ++offset)
            evaluate (offset);

        match.offset     = bestOffset;
        match.similarity = bestSim;
        return match;
    }
} // namespace

// ---------------------------------------------------------------------------
// Vorbereitung
// ---------------------------------------------------------------------------

void WsolaStretcher::prepare (double sampleRateToUse, int numChannelsToUse,
                              const WsolaSettings& settingsToUse)
{
    sampleRate_  = (sampleRateToUse > 0.0 ? sampleRateToUse : 48000.0);
    numChannels_ = juce::jmax (1, numChannelsToUse);
    settings_    = settingsToUse;

    sanitiseSettings (settings_);

    hann_     = makeHannWindow (settings_.frameSize);
    prepared_ = true;

    reset();
}

void WsolaStretcher::reset()
{
    lastSimilarity_ = 0.0f;
    meanSimilarity_ = 0.0f;

    resampler_.reset (numChannels_);
}

// ---------------------------------------------------------------------------
// Hilfswerte
// ---------------------------------------------------------------------------

double WsolaStretcher::clampFactor (double factor) noexcept
{
    if (! std::isfinite (factor))
        return 1.0;

    return juce::jlimit (minFactor, maxFactor, factor);
}

int WsolaStretcher::outputLengthFor (int inputLength, double factor) noexcept
{
    if (inputLength <= 0)
        return 0;

    const double clamped = clampFactor (factor);
    const double exact   = std::floor ((double) inputLength * clamped + 0.5);

    if (exact < 1.0)
        return 1;

    if (exact > (double) kMaxOutputLength)
        return kMaxOutputLength;

    return (int) exact;
}

// ---------------------------------------------------------------------------
// Verarbeitung
// ---------------------------------------------------------------------------

void WsolaStretcher::process (const juce::AudioBuffer<float>& input,
                              juce::AudioBuffer<float>& output,
                              double factor,
                              bool keepPitch)
{
    if (! prepared_)
        prepare (sampleRate_, juce::jmax (1, input.getNumChannels()), settings_);

    const int    numCh   = input.getNumChannels();
    const int    numIn   = input.getNumSamples();
    const double stretched = clampFactor (factor);
    const int    outLen  = outputLengthFor (numIn, stretched);

    lastSimilarity_ = 0.0f;
    meanSimilarity_ = 0.0f;

    // Leere Eingabe -> leere Ausgabe. Kein Sonderfall im DSP, nur eine gueltige
    // Puffergroesse (0 Samples).
    if (numCh <= 0 || numIn <= 0 || outLen <= 0)
    {
        output.setSize (juce::jmax (1, numCh), juce::jmax (0, outLen), false, true, true);
        output.clear();
        return;
    }

    // -----------------------------------------------------------------------
    // REPITCH: Laenge und Tonhoehe gemeinsam ueber Resampling.
    // -----------------------------------------------------------------------
    if (! keepPitch)
    {
        // Offline-Semantik: kein Filterzustand aus einem frueheren Aufruf, damit
        // dieselbe Eingabe immer dieselbe Ausgabe ergibt.
        resampler_.reset (numCh);
        resampler_.processToLength (input, output, outLen);

        if (log::isDebugEnabled())
            log::d ("WsolaStretcher: REPITCH x" + juce::String (stretched, 3)
                    + ", " + juce::String (numIn) + " -> " + juce::String (outLen) + " Samples");

        return;
    }

    // Sehr kurze Eingaben (unter einem Bruchteil eines Fensters): das Fenster
    // waere breiter als das Signal. Notpfad ohne Ueberlappung, aber sauber.
    if (numIn < kTinyInputLength)
    {
        resampler_.reset (numCh);
        resampler_.processToLength (input, output, outLen);

        if (log::isDebugEnabled())
            log::d ("WsolaStretcher: Notpfad (nur " + juce::String (numIn) + " Samples)");

        return;
    }

    // -----------------------------------------------------------------------
    // Arbeitsgroessen (an kurze Eingaben angepasst)
    // -----------------------------------------------------------------------
    const int frame   = juce::jmax (2, juce::jmin (settings_.frameSize, numIn));
    const int overlap = juce::jlimit (1, juce::jmax (1, frame / 2), settings_.overlapSize);
    const int hop     = juce::jmax (1, frame - overlap);   // Synthese-Hop
    const int radius  = juce::jlimit (0, frame, settings_.searchRadius);

    // Bei Faktor 1 ist die Quellabbildung bereits identisch - die Suche koennte
    // nur schaden, weil sie das Raster verlaesst.
    const bool searchActive = (radius > 0) && (std::abs (stretched - 1.0) > 1.0e-9);
    const int  searchRadius = searchActive ? radius : 0;

    const double hopIn        = (double) hop / stretched;   // Quellvorschub je Grain
    const int    pad          = searchRadius + frame + overlap + 8;
    const int    paddedLength = numIn + 2 * pad;

    // Hann-Fenster fuer die tatsaechliche Framegroesse.
    std::vector<float> hann = ((int) hann_.size() == frame) ? hann_ : makeHannWindow (frame);
    const float* window = hann.data();

    // -----------------------------------------------------------------------
    // Quellsignal mit Nullrand (Grain-Suche darf ueber die Raender schauen)
    // -----------------------------------------------------------------------
    juce::AudioBuffer<float> source (numCh, paddedLength);
    source.clear();

    for (int ch = 0; ch < numCh; ++ch)
        source.copyFrom (ch, pad, input, ch, 0, numIn);

    // Summensignal fuer die gemeinsame Stereo-Suche (siehe stretching.md, 5).
    std::vector<float> mono ((size_t) paddedLength, 0.0f);
    {
        const double scale = 1.0 / (double) numCh;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* channelData = source.getReadPointer (ch);

            for (int n = 0; n < paddedLength; ++n)
                mono[(size_t) n] += (float) ((double) channelData[n] * scale);
        }
    }

    // Praefixsummen der Energie: Kandidatenenergie in O(1) je Kandidat.
    std::vector<double> prefixSquares ((size_t) paddedLength + 1, 0.0);

    for (int n = 0; n < paddedLength; ++n)
    {
        const double value = (double) mono[(size_t) n];
        prefixSquares[(size_t) n + 1] = prefixSquares[(size_t) n] + value * value;
    }

    // -----------------------------------------------------------------------
    // Ausgabeakkumulator und Normierungssumme
    // -----------------------------------------------------------------------
    juce::AudioBuffer<float> accum (numCh, outLen);
    accum.clear();

    std::vector<float> normSum ((size_t) outLen, 0.0f);

    std::vector<float*>       accumPtrs  ((size_t) numCh, nullptr);
    std::vector<const float*> sourcePtrs ((size_t) numCh, nullptr);

    for (int ch = 0; ch < numCh; ++ch)
    {
        accumPtrs[(size_t) ch]  = accum.getWritePointer (ch);
        sourcePtrs[(size_t) ch] = source.getReadPointer (ch);
    }

    // -----------------------------------------------------------------------
    // Grain-Schleife: Ueberlappungsadddition mit Aehnlichkeitssuche
    // -----------------------------------------------------------------------
    const int numGrains = (outLen + hop - 1) / hop;

    double similaritySum = 0.0;
    int    similarityCount = 0;
    int    previousPos = 0;

    for (int grain = 0; grain < numGrains; ++grain)
    {
        const int outPos   = grain * hop;
        const int writable = juce::jmin (frame, outLen - outPos);

        if (writable <= 0)
            break;

        // Nominalposition aus der linearen Zeitabbildung (nicht aufsummiert,
        // damit sich kein Rundungsdrift einschleicht).
        int readPos = (int) std::llround ((double) grain * hopIn);
        readPos = juce::jmax (0, readPos);

        if (grain > 0 && searchRadius > 0)
        {
            // Referenz: der Ueberlappungsbereich des vorigen Grains im Quellsignal.
            const int templatePos = previousPos + hop;

            const GrainMatch found = findBestGrain (mono.data(), prefixSquares,
                                                    templatePos, readPos, overlap,
                                                    searchRadius, (double) settings_.tolerance,
                                                    paddedLength);

            readPos = juce::jmax (0, readPos + found.offset);

            lastSimilarity_ = (float) juce::jlimit (0.0, 1.0, found.similarity);
            similaritySum  += (double) lastSimilarity_;
            ++similarityCount;
        }

        for (int n = 0; n < writable; ++n)
        {
            // Der erste Grain blendet nicht ein: sein Kopf bleibt ungefenstert,
            // damit der Anschlag (Transient) des Kicks samplegenau erhalten bleibt.
            const float weight = (grain == 0 && n < hop) ? 1.0f : window[n];

            const int outIndex = outPos + n;
            const int srcIndex = juce::jlimit (0, paddedLength - 1, pad + readPos + n);

            normSum[(size_t) outIndex] += weight;

            for (int ch = 0; ch < numCh; ++ch)
                accumPtrs[(size_t) ch][outIndex] += weight * sourcePtrs[(size_t) ch][srcIndex];
        }

        previousPos = readPos;
    }

    // -----------------------------------------------------------------------
    // Energienormalisierung: Summe der Fenstergewichte teilt aus
    // -----------------------------------------------------------------------
    output.setSize (numCh, outLen, false, true, true);

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* accData = accum.getReadPointer (ch);
        float*       outData = output.getWritePointer (ch);

        for (int n = 0; n < outLen; ++n)
        {
            const float weightSum = normSum[(size_t) n];

            // Kein Signal im Fenster: exakt 0 statt 0/0 (kein NaN, kein Inf).
            outData[n] = (weightSum > kNormEpsilon) ? accData[n] / weightSum : 0.0f;
        }
    }

    meanSimilarity_ = (similarityCount > 0)
                        ? (float) (similaritySum / (double) similarityCount)
                        : 0.0f;

    if (log::isDebugEnabled())
        log::d ("WsolaStretcher: KEEP PITCH x" + juce::String (stretched, 3)
                + ", " + juce::String (numIn) + " -> " + juce::String (outLen) + " Samples"
                + ", Frame " + juce::String (frame) + ", Hop " + juce::String (hop)
                + ", Suche " + (searchRadius > 0 ? "an" : "aus")
                + ", Aehnlichkeit " + juce::String ((double) meanSimilarity_, 3));
}

} // namespace bfka
