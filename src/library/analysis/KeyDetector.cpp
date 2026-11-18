// B-24: Key-Detector - Implementierung von Chroma-Profil und Template-Matching.
// Ablauf: Mono mischen, auf Blockmittel herunterbrechen, je MIDI-Note eine
// Goertzel-Energie messen, daraus das Chroma-Profil bilden und gegen die
// rotierten Krumhansl-Kessler-Profile korrelieren.

#include "library/analysis/KeyDetector.h"

#include <cmath>
#include <vector>

namespace bfka
{
    namespace
    {
        constexpr int kNumNotes          = 12;   ///< Chroma-Dimension
        constexpr int kLowestMidiNote    = 24;   ///< C1
        constexpr int kHighestMidiNote   = 96;   ///< C7
        constexpr int kTargetBlockSamples = 4096; ///< Zielgroesse nach dem Herunterbrechen

        /** Krumhansl-Kessler Dur-Profil, Index 0 = Grundton. */
        constexpr double kMajorTemplate[kNumNotes] =
        {
            6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88
        };

        /** Krumhansl-Kessler Moll-Profil, Index 0 = Grundton. */
        constexpr double kMinorTemplate[kNumNotes] =
        {
            6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17
        };

        /** Kanonische Notennamen (C=0 ... B=11). */
        const char* const kNoteNames[kNumNotes] =
        {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        /** Heruntergebrochener Mono-Block samt wirksamer Abtastrate. */
        struct DecimatedBlock
        {
            std::vector<double> samples;   ///< Blockmittel je Block
            double              rate = 0.0; ///< wirksame Abtastrate nach dem Mittel
        };

        /**
         * Mischt die Kanaele und bricht die ersten `seconds` auf etwa
         * kTargetBlockSamples Blockmittel herunter (Boxcar, kein Filter).
         * Der Rest des Fensters wird verworfen; damit bleibt die wirksame
         * Abtastrate exakt sampleRate / blockSize.
         */
        DecimatedBlock decimate (const juce::AudioBuffer<float>& buffer,
                                 double sampleRate,
                                 double seconds)
        {
            DecimatedBlock result;

            const int numSamples  = buffer.getNumSamples();
            const int numChannels = buffer.getNumChannels();

            if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0)
                return result;

            // 1. Nur der Anfang, mindestens ein Sample.
            int window = (int) std::llround (juce::jmax (0.0, seconds) * sampleRate);
            window = juce::jlimit (1, numSamples, window);

            // Kanalmittel: L und R tragen je zur Haelfte bei.
            std::vector<double> mono ((size_t) window, 0.0);
            const double channelScale = 1.0 / (double) numChannels;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* data = buffer.getReadPointer (ch);

                for (int i = 0; i < window; ++i)
                    mono[(size_t) i] += (double) data[i] * channelScale;
            }

            // 2. Blockmittel: 48000 Samples / 4096 Bloecke -> Blockgroesse 11.
            const int blockSize = juce::jmax (1, window / kTargetBlockSamples);
            const int numBlocks = juce::jmax (1, window / blockSize);

            result.samples.assign ((size_t) numBlocks, 0.0);

            for (int block = 0; block < numBlocks; ++block)
            {
                double sum = 0.0;
                const int start = block * blockSize;

                for (int i = 0; i < blockSize; ++i)
                    sum += mono[(size_t) (start + i)];

                result.samples[(size_t) block] = sum / (double) blockSize;
            }

            result.rate = sampleRate / (double) blockSize;

            return result;
        }

        /**
         * Goertzel-Energie einer Frequenz ueber den ganzen Block.
         * Normiert auf die Blocklaenge, damit die Werte unabhaengig von N bleiben.
         */
        double goertzelEnergy (const std::vector<double>& samples,
                               double frequency,
                               double sampleRate) noexcept
        {
            const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
            const double coeff = 2.0 * std::cos (omega);

            double s1 = 0.0;
            double s2 = 0.0;

            for (const double value : samples)
            {
                const double s0 = value + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }

            double magnitudeSquared = s1 * s1 + s2 * s2 - coeff * s1 * s2;

            // Rundungsfehler koennen den Ausdruck minimal negativ machen.
            if (magnitudeSquared < 0.0 || ! std::isfinite (magnitudeSquared))
                magnitudeSquared = 0.0;

            const double length = (double) samples.size();
            return magnitudeSquared / juce::jmax (1.0, length * length);
        }

        /** Pearson-Korrelation zwischen Chroma und einem Profil (12 Werte). */
        double correlation (const std::array<double, kNumNotes>& chroma,
                            const double* profile) noexcept
        {
            double chromaMean = 0.0;
            double profileMean = 0.0;

            for (int i = 0; i < kNumNotes; ++i)
            {
                chromaMean  += chroma[(size_t) i];
                profileMean += profile[i];
            }

            chromaMean  /= (double) kNumNotes;
            profileMean /= (double) kNumNotes;

            double numerator = 0.0;
            double chromaVariance = 0.0;
            double profileVariance = 0.0;

            for (int i = 0; i < kNumNotes; ++i)
            {
                const double a = chroma[(size_t) i] - chromaMean;
                const double b = profile[i] - profileMean;

                numerator        += a * b;
                chromaVariance   += a * a;
                profileVariance  += b * b;
            }

            const double denominator = std::sqrt (chromaVariance * profileVariance);
            return (denominator > 1.0e-12) ? numerator / denominator : 0.0;
        }
    } // namespace

    std::array<double, 12> KeyDetector::chromaProfile (const juce::AudioBuffer<float>& buffer,
                                                       double sampleRate,
                                                       const Settings& settings)
    {
        std::array<double, 12> chroma {};
        chroma.fill (0.0);

        // 6. Nichts auszuwerten.
        if (sampleRate <= 0.0 || buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
            return chroma;

        const DecimatedBlock block = decimate (buffer, sampleRate, settings.analysisSeconds);

        if (block.samples.empty() || block.rate <= 0.0)
            return chroma;

        const double nyquist = 0.5 * block.rate;

        // 3. Energie je MIDI-Note sammeln; jede Note faellt auf genau ein Chroma.
        for (int note = kLowestMidiNote; note <= kHighestMidiNote; ++note)
        {
            const double frequency = 440.0 * std::pow (2.0, ((double) note - 69.0) / 12.0);

            if (frequency < settings.minFrequencyHz || frequency > settings.maxFrequencyHz)
                continue;

            // Oberhalb der Nyquist-Frequenz des Blocks wuerde das Filter falten.
            if (frequency >= nyquist)
                continue;

            chroma[(size_t) (note % kNumNotes)] += goertzelEnergy (block.samples, frequency, block.rate);
        }

        // 3. Auf Summe 1 normieren; Stille bleibt bei 0.
        double sum = 0.0;

        for (const double value : chroma)
            sum += value;

        if (sum > 0.0)
        {
            for (auto& value : chroma)
                value /= sum;
        }

        return chroma;
    }

    KeyDetectionResult KeyDetector::detect (const juce::AudioBuffer<float>& buffer,
                                            double sampleRate,
                                            const Settings& settings)
    {
        // 6. Leerer Puffer oder unsinnige Rate -> Default-Ergebnis.
        if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0 || sampleRate <= 0.0)
            return KeyDetectionResult {};

        const std::array<double, 12> chroma = chromaProfile (buffer, sampleRate, settings);

        double chromaSum = 0.0;

        for (const double value : chroma)
            chromaSum += value;

        if (! (chromaSum > 0.0))
            return KeyDetectionResult {};

        // 4. Template-Matching: 12 Grundtoene x 2 Geschlechter.
        double bestScore  = -1.0e9;
        int    bestKey    = -1;
        bool   bestMinor  = false;
        double scoreSum   = 0.0;
        int    scoreCount = 0;

        for (int gender = 0; gender < 2; ++gender)
        {
            const double* const source = (gender == 0) ? kMajorTemplate : kMinorTemplate;
            const bool minor = (gender == 1);

            for (int root = 0; root < kNumNotes; ++root)
            {
                // Template um den Grundton rotieren: Stufe i gehoert zu (root + i).
                double rotated[kNumNotes];

                for (int i = 0; i < kNumNotes; ++i)
                    rotated[i] = source[((i - root) + kNumNotes) % kNumNotes];

                const double score = correlation (chroma, rotated);

                scoreSum += score;
                ++scoreCount;

                if (score > bestScore)
                {
                    bestScore = score;
                    bestKey   = root;
                    bestMinor = minor;
                }
            }
        }

        const double meanScore = (scoreCount > 0) ? scoreSum / (double) scoreCount : 0.0;

        // 5. Confidence: Vorsprung der Spitze vor dem Mittelfeld.
        const double confidence = juce::jlimit (0.0, 1.0,
                                                (bestScore - meanScore)
                                                  / juce::jmax (1.0e-9, bestScore));

        const bool found = (chromaSum > 0.0) && confidence > 0.05;

        if (! found)
        {
            // Ohne belastbaren Vorsprung bleibt der Grundton unbekannt (-1).
            KeyDetectionResult weak;
            weak.confidence = confidence;
            return weak;
        }

        KeyDetectionResult result;
        result.keyNote    = bestKey;
        result.minor      = bestMinor;
        result.confidence = confidence;
        result.found      = true;

        return result;
    }

    juce::String KeyDetector::noteName (int keyNote)
    {
        if (keyNote < 0 || keyNote >= kNumNotes)
            return {};

        return juce::String (kNoteNames[keyNote]);
    }
} // namespace bfka
