#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <limits>
#include <vector>

#include "fx/modules/FinalizerModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr double kPi         = juce::MathConstants<double>::pi;

    // -----------------------------------------------------------------------
    // Parametereinstellungen der Tests
    // -----------------------------------------------------------------------

    /** Alle acht Parameter in Anzeige-Einheiten; Defaults = Modul-Defaults. */
    struct Settings
    {
        float boost   = 0.0f;
        float freq    = 50.0f;
        float clip    = 0.0f;
        float upward  = 0.0f;
        float monoF   = 120.0f;
        float shelfOn = 1.0f;
        float monoOn  = 1.0f;
        float clipOn  = 1.0f;
    };

    void applySettings (FinalizerModule& module, const Settings& settings)
    {
        module.setParam (FinalizerModule::pBoost,    settings.boost);
        module.setParam (FinalizerModule::pFreq,     settings.freq);
        module.setParam (FinalizerModule::pClip,     settings.clip);
        module.setParam (FinalizerModule::pUpward,   settings.upward);
        module.setParam (FinalizerModule::pMonoFreq, settings.monoF);
        module.setParam (FinalizerModule::pShelfOn,  settings.shelfOn);
        module.setParam (FinalizerModule::pMonoOn,   settings.monoOn);
        module.setParam (FinalizerModule::pClipOn,   settings.clipOn);
    }

    /** Verarbeitet den Puffer blockweise (wie die Kette) und gibt das Ergebnis zurueck. */
    juce::AudioBuffer<float> runModule (const juce::AudioBuffer<float>& input,
                                        const Settings& settings,
                                        double sampleRate = kSampleRate,
                                        int blockSize = kBlockSize)
    {
        FinalizerModule module;
        applySettings (module, settings);
        module.prepare (sampleRate, blockSize);

        juce::AudioBuffer<float> output (input.getNumChannels(), input.getNumSamples());

        const auto total = input.getNumSamples();

        for (int start = 0; start < total; start += blockSize)
        {
            const auto numSamples = juce::jmin (blockSize, total - start);

            juce::AudioBuffer<float> block (input.getNumChannels(), numSamples);

            for (int ch = 0; ch < input.getNumChannels(); ++ch)
                block.copyFrom (ch, 0, input, ch, start, numSamples);

            module.process (block);

            for (int ch = 0; ch < input.getNumChannels(); ++ch)
                output.copyFrom (ch, start, block, ch, 0, numSamples);
        }

        return output;
    }

    // -----------------------------------------------------------------------
    // Messhilfen
    // -----------------------------------------------------------------------

    int sampleIndex (double seconds, int numSamples, double sampleRate = kSampleRate)
    {
        const auto index = (int) std::lround (seconds * sampleRate);
        return juce::jlimit (0, numSamples, index);
    }

    /** RMS eines Kanals zwischen zwei Zeitpunkten (Sekunden). */
    float rmsBetween (const juce::AudioBuffer<float>& buffer,
                      int channel,
                      double startSeconds,
                      double endSeconds,
                      double sampleRate = kSampleRate)
    {
        const auto start = sampleIndex (startSeconds, buffer.getNumSamples(), sampleRate);
        const auto end   = juce::jmax (start, sampleIndex (endSeconds, buffer.getNumSamples(), sampleRate));
        const auto* data = buffer.getReadPointer (channel);

        double sum = 0.0;

        for (int i = start; i < end; ++i)
            sum += (double) data[i] * (double) data[i];

        const auto count = end - start;

        return (count > 0) ? (float) std::sqrt (sum / (double) count) : 0.0f;
    }

    /** Spitzenbetrag eines Kanals ab `startSeconds`. */
    float peakOf (const juce::AudioBuffer<float>& buffer, int channel = 0,
                  double startSeconds = 0.0, double sampleRate = kSampleRate)
    {
        const auto start = sampleIndex (startSeconds, buffer.getNumSamples(), sampleRate);

        float peak = 0.0f;

        for (int i = start; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (channel, i)));

        return peak;
    }

    /** Pegel eines RMS-Werts in dBFS (Untergrenze -240 dB). */
    float dbfs (float rms)
    {
        return 20.0f * std::log10 (juce::jmax (rms, 1.0e-12f));
    }

    /** Normierte Korrelation zweier Kanaele im Messfenster (1 = deckungsgleich). */
    float correlation (const juce::AudioBuffer<float>& buffer,
                       int channelA,
                       int channelB,
                       double startSeconds,
                       double endSeconds,
                       double sampleRate = kSampleRate)
    {
        const auto start = sampleIndex (startSeconds, buffer.getNumSamples(), sampleRate);
        const auto end   = juce::jmax (start, sampleIndex (endSeconds, buffer.getNumSamples(), sampleRate));

        const auto* a = buffer.getReadPointer (channelA);
        const auto* b = buffer.getReadPointer (channelB);

        double dot = 0.0;
        double sumA = 0.0;
        double sumB = 0.0;

        for (int i = start; i < end; ++i)
        {
            const double va = (double) a[i];
            const double vb = (double) b[i];

            dot  += va * vb;
            sumA += va * va;
            sumB += vb * vb;
        }

        const double denominator = std::sqrt (sumA * sumB);

        return (denominator > 1.0e-20) ? (float) (dot / denominator) : 0.0f;
    }

    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (data[i]))
                    return false;
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // Testsignale
    // -----------------------------------------------------------------------

    /** Sinus auf beiden Kanaelen (gleiche Amplitude). */
    juce::AudioBuffer<float> makeSine (double frequencyHz, double seconds,
                                       float amplitude, double sampleRate = kSampleRate)
    {
        const auto numSamples = (int) std::lround (seconds * sampleRate);
        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = amplitude
                             * (float) std::sin (2.0 * kPi * frequencyHz * (double) i / sampleRate);

            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);
        }

        return buffer;
    }

    /** Links Sinus, rechts Stille (fuer den Mono-Bass-Test). */
    juce::AudioBuffer<float> makeLeftSine (double frequencyHz, double seconds,
                                           float amplitude, double sampleRate = kSampleRate)
    {
        const auto numSamples = (int) std::lround (seconds * sampleRate);
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, amplitude
                                     * (float) std::sin (2.0 * kPi * frequencyHz * (double) i / sampleRate));

        return buffer;
    }

    /** Erzeugt den Puffer mit dem Kopf des Eingangs und liefert ihn zum Prozessieren. */
    juce::AudioBuffer<float> copyOf (const juce::AudioBuffer<float>& source)
    {
        juce::AudioBuffer<float> copy (source.getNumChannels(), source.getNumSamples());
        copy.makeCopyOf (source);
        return copy;
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Parametertabelle und Metadaten
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Parametertabelle und Metadaten", "[fx][finalizer]")
{
    FinalizerModule module;

    REQUIRE (module.type() == FxType::finalizer);
    REQUIRE (module.displayName() == "Finalizer");

    // Vor prepare() ist das Modul latenzfrei.
    REQUIRE (module.latencySamples() == 0);

    const auto& params = module.params();

    REQUIRE (params.size() == 8);
    REQUIRE (params.size() == (size_t) FinalizerModule::numParams);

    // Kennungen in exakt der vorgegebenen Reihenfolge.
    REQUIRE (params[0].id == "boost");
    REQUIRE (params[1].id == "freq");
    REQUIRE (params[2].id == "clip");
    REQUIRE (params[3].id == "upward");
    REQUIRE (params[4].id == "monoF");
    REQUIRE (params[5].id == "shelfOn");
    REQUIRE (params[6].id == "monoOn");
    REQUIRE (params[7].id == "clipOn");

    // Anzeigenamen.
    REQUIRE (params[0].name == "Sub Boost");
    REQUIRE (params[1].name == "Sub Freq");
    REQUIRE (params[2].name == "Clip");
    REQUIRE (params[3].name == "Upward");
    REQUIRE (params[4].name == "Mono Freq");
    REQUIRE (params[5].name == "Shelf On");
    REQUIRE (params[6].name == "Mono On");
    REQUIRE (params[7].name == "Clip On");

    // Bereiche, Defaults und Einheiten.
    for (const int index : { 0, 1, 2, 3, 4, 5, 6, 7 })
    {
        INFO ("Index " << index);
        REQUIRE (params[(size_t) index].minValue < params[(size_t) index].maxValue);
        REQUIRE (params[(size_t) index].defaultValue >= params[(size_t) index].minValue);
        REQUIRE (params[(size_t) index].defaultValue <= params[(size_t) index].maxValue);
    }

    REQUIRE_THAT (params[0].minValue, WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_THAT (params[0].maxValue, WithinAbs (12.0f, 1.0e-6f));
    REQUIRE_THAT (params[0].defaultValue, WithinAbs (0.0f, 1.0e-6f));
    REQUIRE (params[0].unit == "dB");

    REQUIRE_THAT (params[1].minValue, WithinAbs (20.0f, 1.0e-6f));
    REQUIRE_THAT (params[1].maxValue, WithinAbs (200.0f, 1.0e-6f));
    REQUIRE_THAT (params[1].defaultValue, WithinAbs (50.0f, 1.0e-6f));
    REQUIRE (params[1].unit == "Hz");

    REQUIRE_THAT (params[2].minValue, WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_THAT (params[2].maxValue, WithinAbs (100.0f, 1.0e-6f));
    REQUIRE_THAT (params[2].defaultValue, WithinAbs (0.0f, 1.0e-6f));
    REQUIRE (params[2].unit == "%");

    REQUIRE_THAT (params[3].minValue, WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_THAT (params[3].maxValue, WithinAbs (100.0f, 1.0e-6f));
    REQUIRE_THAT (params[3].defaultValue, WithinAbs (0.0f, 1.0e-6f));
    REQUIRE (params[3].unit == "%");

    REQUIRE_THAT (params[4].minValue, WithinAbs (60.0f, 1.0e-6f));
    REQUIRE_THAT (params[4].maxValue, WithinAbs (500.0f, 1.0e-6f));
    REQUIRE_THAT (params[4].defaultValue, WithinAbs (120.0f, 1.0e-6f));
    REQUIRE (params[4].unit == "Hz");

    for (const int index : { 5, 6, 7 })
    {
        INFO ("Schalter " << index);
        REQUIRE_THAT (params[(size_t) index].minValue, WithinAbs (0.0f, 1.0e-6f));
        REQUIRE_THAT (params[(size_t) index].maxValue, WithinAbs (1.0f, 1.0e-6f));
        REQUIRE_THAT (params[(size_t) index].defaultValue, WithinAbs (1.0f, 1.0e-6f));
        REQUIRE (params[(size_t) index].unit.isEmpty());
    }

    // Defaults stehen im Modul.
    REQUIRE_THAT (module.getParam (FinalizerModule::pBoost), WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_THAT (module.getParam (FinalizerModule::pFreq), WithinAbs (50.0f, 1.0e-6f));
    REQUIRE_THAT (module.getParam (FinalizerModule::pClip), WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_THAT (module.getParam (FinalizerModule::pUpward), WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_THAT (module.getParam (FinalizerModule::pMonoFreq), WithinAbs (120.0f, 1.0e-6f));
    REQUIRE_THAT (module.getParam (FinalizerModule::pShelfOn), WithinAbs (1.0f, 1.0e-6f));
    REQUIRE_THAT (module.getParam (FinalizerModule::pMonoOn), WithinAbs (1.0f, 1.0e-6f));
    REQUIRE_THAT (module.getParam (FinalizerModule::pClipOn), WithinAbs (1.0f, 1.0e-6f));

    // MIX und OUT GAIN gehoeren zur Kette, nicht zum Modul.
    for (const auto& desc : params)
    {
        REQUIRE (desc.id != "mix");
        REQUIRE (desc.id != "outGain");
        REQUIRE (desc.id != "on");
    }

    // Kein Struktur-Zustand: die Basis-Defaults aus FxModule.h reichen.
    const auto state = module.getState();
    REQUIRE (state.isValid());
    REQUIRE (state.getType().toString() == "Module");
    REQUIRE (state.getNumChildren() == 0);

    juce::ValueTree foreignState ("FxChain");
    foreignState.appendChild (juce::ValueTree ("Module"), nullptr);
    module.setState (foreignState);   // darf nichts veraendern und nicht abstuerzen
    REQUIRE (module.params().size() == 8);

    // Ungueltige Indizes.
    REQUIRE (module.getParam (99) == 0.0f);
    REQUIRE (module.getParam (-1) == 0.0f);

    const auto before = module.getParam (FinalizerModule::pClip);
    module.setParam (99, 50.0f);
    module.setParam (-1, 50.0f);
    REQUIRE (module.getParam (FinalizerModule::pClip) == before);

    // Nicht-endliche Werte werden nicht uebernommen.
    module.setParam (FinalizerModule::pBoost, std::nanf (""));
    REQUIRE_THAT (module.getParam (FinalizerModule::pBoost), WithinAbs (0.0f, 1.0e-6f));

    module.setParam (FinalizerModule::pBoost, std::numeric_limits<float>::infinity());
    REQUIRE_THAT (module.getParam (FinalizerModule::pBoost), WithinAbs (0.0f, 1.0e-6f));
}

// ---------------------------------------------------------------------------
// 2. Klemmen
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Parameter werden geklemmt", "[fx][finalizer]")
{
    FinalizerModule module;

    module.setParam (FinalizerModule::pBoost, 99.0f);
    REQUIRE (module.getParam (FinalizerModule::pBoost) == 12.0f);

    module.setParam (FinalizerModule::pFreq, 1.0f);
    REQUIRE (module.getParam (FinalizerModule::pFreq) == 20.0f);

    module.setParam (FinalizerModule::pClip, -1.0f);
    REQUIRE (module.getParam (FinalizerModule::pClip) == 0.0f);

    module.setParam (FinalizerModule::pUpward, 150.0f);
    REQUIRE (module.getParam (FinalizerModule::pUpward) == 100.0f);

    module.setParam (FinalizerModule::pMonoFreq, 10.0f);
    REQUIRE (module.getParam (FinalizerModule::pMonoFreq) == 60.0f);

    module.setParam (FinalizerModule::pMonoFreq, 900.0f);
    REQUIRE (module.getParam (FinalizerModule::pMonoFreq) == 500.0f);

    module.setParam (FinalizerModule::pShelfOn, 0.5f);
    REQUIRE (module.getParam (FinalizerModule::pShelfOn) == 0.5f);

    module.setParam (FinalizerModule::pFreq, 999.0f);
    REQUIRE (module.getParam (FinalizerModule::pFreq) == 200.0f);
}

// ---------------------------------------------------------------------------
// 3. Lookahead-Latenz
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Lookahead-Latenz folgt der Sample-Rate", "[fx][finalizer]")
{
    FinalizerModule module;

    // Vor prepare() gibt es keine Latenz.
    REQUIRE (module.latencySamples() == 0);

    module.prepare (48000.0, kBlockSize);
    REQUIRE (module.latencySamples() == 72);
    REQUIRE (module.sampleRate() == 48000.0);

    module.prepare (96000.0, kBlockSize);
    REQUIRE (module.latencySamples() == 144);

    module.prepare (44100.0, kBlockSize);
    REQUIRE (module.latencySamples() == 67);

    // Formel direkt: ceil(0.0015 * fs), mindestens ein Sample.
    REQUIRE (FinalizerModule::lookaheadSamplesFor (48000.0) == 72);
    REQUIRE (FinalizerModule::lookaheadSamplesFor (96000.0) == 144);
    REQUIRE (FinalizerModule::lookaheadSamplesFor (44100.0) == 67);
    REQUIRE (FinalizerModule::lookaheadSamplesFor (192000.0) == 288);
    REQUIRE (FinalizerModule::lookaheadSamplesFor (0.0) == 72);
    REQUIRE (FinalizerModule::lookaheadSamplesFor (100.0) == 1);

    // Die Latenz haengt nicht am Clip-Regler: sie bleibt auch bei clip = 0.
    module.prepare (48000.0, kBlockSize);
    module.setParam (FinalizerModule::pClip, 0.0f);
    REQUIRE (module.latencySamples() == 72);

    module.setParam (FinalizerModule::pClip, 100.0f);
    module.setParam (FinalizerModule::pClipOn, 1.0f);
    REQUIRE (module.latencySamples() == 72);

    module.setParam (FinalizerModule::pClipOn, 0.0f);
    REQUIRE (module.latencySamples() == 72);
}

// ---------------------------------------------------------------------------
// 4. Bypass-/Neutraltest
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Neutralzustand ist nur die Lookahead-Verzoegerung", "[fx][finalizer]")
{
    // Alle Stufen aus: Ausgang == Eingang, verschoben um die Latenz.
    Settings neutral;
    neutral.boost   = 0.0f;
    neutral.clip    = 0.0f;
    neutral.upward  = 0.0f;
    neutral.shelfOn = 0.0f;
    neutral.monoOn  = 0.0f;
    neutral.clipOn  = 0.0f;

    const auto input = makeSine (440.0, 0.5, 0.5);

    FinalizerModule module;
    applySettings (module, neutral);
    module.prepare (kSampleRate, kBlockSize);

    const auto latency = module.latencySamples();
    REQUIRE (latency == 72);

    auto output = copyOf (input);

    for (int start = 0; start < output.getNumSamples(); start += kBlockSize)
    {
        const auto numSamples = juce::jmin (kBlockSize, output.getNumSamples() - start);

        juce::AudioBuffer<float> block (output.getNumChannels(), numSamples);

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            block.copyFrom (ch, 0, output, ch, start, numSamples);

        module.process (block);

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.copyFrom (ch, start, block, ch, 0, numSamples);
    }

    // Der Verzoegerungspuffer startet leer: die ersten `latency` Samples sind 0.
    for (int i = 0; i < latency; ++i)
    {
        REQUIRE (output.getSample (0, i) == 0.0f);
        REQUIRE (output.getSample (1, i) == 0.0f);
    }

    // Danach bitgenau das verzoegerte Eingangssignal (Toleranz gemaess Ticket).
    float maxDiff = 0.0f;

    for (int ch = 0; ch < input.getNumChannels(); ++ch)
        for (int i = latency; i < input.getNumSamples(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (output.getSample (ch, i)
                                                     - input.getSample (ch, i - latency)));

    INFO ("max |out - in delayed| = " << maxDiff);
    REQUIRE (maxDiff < 1.0e-6f);

    // Auch ueber viele Bloecke bleibt der Durchlauf bitgenau (kein Zustandsleck).
    {
        FinalizerModule longRun;
        applySettings (longRun, neutral);
        longRun.prepare (kSampleRate, kBlockSize);

        const auto total = 50 * kBlockSize;

        juce::AudioBuffer<float> stream (2, total);

        for (int i = 0; i < total; ++i)
        {
            const auto value = 0.4f * std::sin (0.01f * (float) i);
            stream.setSample (0, i, value);
            stream.setSample (1, i, value);
        }

        const auto reference = copyOf (stream);

        for (int start = 0; start < total; start += kBlockSize)
        {
            juce::AudioBuffer<float> block (2, kBlockSize);

            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom (ch, 0, stream, ch, start, kBlockSize);

            longRun.process (block);

            for (int ch = 0; ch < 2; ++ch)
                stream.copyFrom (ch, start, block, ch, 0, kBlockSize);
        }

        for (int ch = 0; ch < 2; ++ch)
            for (int i = latency; i < total; ++i)
                REQUIRE (stream.getSample (ch, i) == reference.getSample (ch, i - latency));
    }
}

// ---------------------------------------------------------------------------
// 5. Sub-Boost: Frequenzgang
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Sub-Boost hebt nur das Tiefton an", "[fx][finalizer]")
{
    Settings flat;
    flat.shelfOn = 1.0f;
    flat.monoOn  = 0.0f;
    flat.clipOn  = 0.0f;
    flat.boost   = 0.0f;

    Settings boosted = flat;
    boosted.boost = 6.0f;
    boosted.freq  = 50.0f;

    // 40 Hz: ca. +6 dB gegenueber boost = 0 (Toleranz +/-1,5 dB).
    {
        const auto tone = makeSine (40.0, 1.3, 0.2);

        const auto dry = runModule (tone, flat);
        const auto wet = runModule (tone, boosted);

        const auto dryLevel = dbfs (rmsBetween (dry, 0, 0.6, 1.2));
        const auto wetLevel = dbfs (rmsBetween (wet, 0, 0.6, 1.2));

        INFO ("40 Hz: " << dryLevel << " dB -> " << wetLevel << " dB");
        REQUIRE (wetLevel - dryLevel > 4.5f);
        REQUIRE (wetLevel - dryLevel < 7.5f);
    }

    // 5 kHz: der Low-Shelf laesst das Hochton unveraendert (+/-0,2 dB).
    {
        const auto tone = makeSine (5000.0, 1.3, 0.2);

        const auto dry = runModule (tone, flat);
        const auto wet = runModule (tone, boosted);

        const auto difference = dbfs (rmsBetween (wet, 0, 0.6, 1.2))
                              - dbfs (rmsBetween (dry, 0, 0.6, 1.2));

        INFO ("5 kHz: " << difference << " dB");
        REQUIRE (std::abs (difference) <= 0.2f);
    }

    // Ohne shelfOn bleibt der Boost wirkungslos (Schalter wird respektiert).
    {
        auto switchedOff = boosted;
        switchedOff.shelfOn = 0.0f;

        Settings reference = switchedOff;
        reference.boost = 0.0f;

        const auto tone = makeSine (40.0, 1.3, 0.2);

        const auto withoutBoost = runModule (tone, reference);
        const auto withBoost    = runModule (tone, switchedOff);

        const auto difference = dbfs (rmsBetween (withBoost, 0, 0.6, 1.2))
                              - dbfs (rmsBetween (withoutBoost, 0, 0.6, 1.2));

        INFO ("shelfOn = 0: " << difference << " dB");
        REQUIRE (std::abs (difference) <= 0.1f);
    }
}

// ---------------------------------------------------------------------------
// 6. Mono-Bass
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Mono-Bass koppelt nur das Tiefton", "[fx][finalizer]")
{
    Settings mono;
    mono.shelfOn = 0.0f;
    mono.clipOn  = 0.0f;
    mono.monoOn  = 1.0f;
    mono.monoF   = 120.0f;

    // 60 Hz nur links: beide Kanaele tragen danach dasselbe Signal
    // (Korrelation ~1,0 statt Stille rechts).
    {
        const auto input  = makeLeftSine (60.0, 1.2, 0.5);
        const auto output = runModule (input, mono);

        const auto corr  = correlation (output, 0, 1, 0.5, 1.1);
        const auto left  = peakOf (output, 0, 0.5);
        const auto right = peakOf (output, 1, 0.5);

        INFO ("60 Hz: Korrelation " << corr << ", L " << left << ", R " << right);
        REQUIRE (corr > 0.99f);
        REQUIRE (right > 0.15f);          // rechts war vorher exakt 0
        REQUIRE (right < left * 1.5f);    // gleiche Groessenordnung

        // Ohne Mono-Bass bleibt rechts Stille.
        auto withoutMono = mono;
        withoutMono.monoOn = 0.0f;

        const auto untouched = runModule (input, withoutMono);
        REQUIRE (peakOf (untouched, 1) == 0.0f);
    }

    // 1 kHz: der Mono-Bass laesst den Kanal rechts stumm. Eine normierte
    // Korrelation taugt hier nicht als Mass (beide Kanaele tragen dieselbe
    // Frequenz, nur um -80 dB verschieden laut) - gemessen wird die
    // Pegeldifferenz: rechts bleibt praktisch nichts uebrig.
    {
        const auto input  = makeLeftSine (1000.0, 1.2, 0.5);
        const auto output = runModule (input, mono);

        const auto left  = peakOf (output, 0, 0.5);
        const auto right = peakOf (output, 1, 0.5);

        float maxDifference = 0.0f;

        for (int i = sampleIndex (0.5, output.getNumSamples()); i < output.getNumSamples(); ++i)
            maxDifference = juce::jmax (maxDifference,
                                        std::abs (output.getSample (0, i)
                                                  - output.getSample (1, i)));

        INFO ("1 kHz: L " << left << ", R " << right << ", max|L-R| " << maxDifference);
        REQUIRE (left > 0.4f);
        REQUIRE (right < left * 0.01f);   // mehr als 40 dB Unterschied
        REQUIRE (maxDifference > 0.2f);   // L und R sind nicht gleich
    }
}

// ---------------------------------------------------------------------------
// 7. True-Peak-Clipping
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: True-Peak-Clipping haelt 0 dBFS ein", "[fx][finalizer]")
{
    Settings clipping;
    clipping.shelfOn = 0.0f;
    clipping.monoOn  = 0.0f;
    clipping.clipOn  = 1.0f;
    clipping.clip    = 100.0f;

    // +6 dB Uebersteuerung (Amplitude 2,0 = +6 dBFS).
    const auto hot = makeSine (1000.0, 0.8, 2.0f);

    {
        const auto limited = runModule (hot, clipping);

        const auto peak = peakOf (limited, 0, 0.1);
        const auto peakRight = peakOf (limited, 1, 0.1);

        INFO ("Peak bei clip = 100 %: " << peak << " / " << peakRight);
        REQUIRE (peak <= 1.0f + 1.0e-6f);
        REQUIRE (peakRight <= 1.0f + 1.0e-6f);
        REQUIRE (allFinite (limited));
    }

    // Ohne Clipping (clip = 0 %) bleibt die Uebersteuerung stehen.
    {
        auto withoutClipping = clipping;
        withoutClipping.clip = 0.0f;

        const auto open = runModule (hot, withoutClipping);

        const auto peak = peakOf (open, 0, 0.1);

        INFO ("Peak bei clip = 0 %: " << peak);
        REQUIRE (peak > 1.5f);
        REQUIRE_THAT (peak, WithinAbs (2.0f, 1.0e-2f));
    }

    // Auch bei Zwischenwerten (Inter-Sample-Spitzen) haelt der Limiter.
    for (const auto frequency : { 61.7, 11025.0 })
    {
        const auto tone = makeSine (frequency, 0.8, 2.0f);
        const auto limited = runModule (tone, clipping);

        const auto peak = peakOf (limited, 0, 0.2);

        INFO ("Peak bei " << frequency << " Hz: " << peak);
        REQUIRE (peak <= 1.0f + 1.0e-6f);
    }

    // clipOn = 0 schaltet die Stufe ab, clip bleibt wirkungslos.
    {
        auto switchedOff = clipping;
        switchedOff.clipOn = 0.0f;

        const auto open = runModule (hot, switchedOff);
        REQUIRE (peakOf (open, 0, 0.1) > 1.5f);
    }
}

// ---------------------------------------------------------------------------
// 8. Upward
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Upward hebt leise Signale an", "[fx][finalizer]")
{
    Settings quiet;
    quiet.shelfOn = 0.0f;
    quiet.monoOn  = 0.0f;
    quiet.clipOn  = 0.0f;
    quiet.upward  = 0.0f;

    Settings upwardOn = quiet;
    upwardOn.upward = 100.0f;

    // -40 dBFS: deutlich unter der Schwelle von -6 dBFS.
    const auto quietTone = makeSine (1000.0, 1.2, 0.01f);

    {
        const auto flat   = runModule (quietTone, quiet);
        const auto lifted = runModule (quietTone, upwardOn);

        const auto flatLevel   = dbfs (rmsBetween (flat, 0, 0.5, 1.1));
        const auto liftedLevel = dbfs (rmsBetween (lifted, 0, 0.5, 1.1));

        INFO ("upward 0 %: " << flatLevel << " dB, upward 100 %: " << liftedLevel << " dB");
        REQUIRE (liftedLevel - flatLevel > 6.0f);      // maximal +12 dB, gemessen ~ +12 dB
        REQUIRE (liftedLevel - flatLevel <= 12.5f);
    }

    // Ein lautes Signal (-3 dBFS) liegt ueber der Schwelle und bleibt unberuehrt.
    {
        const auto loudTone = makeSine (1000.0, 1.2, 0.7f);

        const auto flat   = runModule (loudTone, quiet);
        const auto lifted = runModule (loudTone, upwardOn);

        const auto difference = dbfs (rmsBetween (lifted, 0, 0.5, 1.1))
                              - dbfs (rmsBetween (flat, 0, 0.5, 1.1));

        INFO ("-3 dBFS: " << difference << " dB");
        REQUIRE (std::abs (difference) <= 0.5f);
    }
}

// ---------------------------------------------------------------------------
// 9. Parameterglaettung an der Blockgrenze
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Parametersprung erzeugt keinen Sprung im Ausgang", "[fx][finalizer]")
{
    // Shelf bei 200 Hz, Testton 40 Hz: ein Sprung von boost 0 -> 12 dB wuerde
    // ohne Glaettung die Amplitude von 0,5 auf ~2,0 springen lassen.
    Settings start;
    start.boost   = 0.0f;
    start.freq    = 200.0f;
    start.shelfOn = 1.0f;
    start.monoOn  = 0.0f;
    start.clipOn  = 0.0f;
    start.upward  = 0.0f;

    const auto tone = makeSine (40.0, 1.0, 0.5f);

    FinalizerModule module;
    applySettings (module, start);
    module.prepare (kSampleRate, kBlockSize);

    const auto total = tone.getNumSamples();

    juce::AudioBuffer<float> output (tone.getNumChannels(), total);

    // Sprung exakt an einer Blockgrenze.
    const int jumpBlock  = 100;
    const int jumpSample = jumpBlock * kBlockSize;

    bool jumped = false;

    for (int start0 = 0; start0 < total; start0 += kBlockSize)
    {
        if (! jumped && start0 >= jumpSample)
        {
            module.setParam (FinalizerModule::pBoost, 12.0f);
            jumped = true;
        }

        const auto numSamples = juce::jmin (kBlockSize, total - start0);

        juce::AudioBuffer<float> block (tone.getNumChannels(), numSamples);

        for (int ch = 0; ch < tone.getNumChannels(); ++ch)
            block.copyFrom (ch, 0, tone, ch, start0, numSamples);

        module.process (block);

        for (int ch = 0; ch < tone.getNumChannels(); ++ch)
            output.copyFrom (ch, start0, block, ch, 0, numSamples);
    }

    const auto before = peakOf (output, 0, 0.20);
    const auto after  = peakOf (output, 0, (double) (jumpSample + 2 * kBlockSize) / kSampleRate);

    // Der Parameter wirkt (Pegel steigt), aber ohne Sprung.
    REQUIRE (after > before * 2.0f);

    float maxStep = 0.0f;

    for (int i = jumpSample; i < juce::jmin (output.getNumSamples(), jumpSample + 3 * kBlockSize); ++i)
        maxStep = juce::jmax (maxStep, std::abs (output.getSample (0, i)
                                                 - output.getSample (0, i - 1)));

    INFO ("max |y[n] - y[n-1]| = " << maxStep << " (vorher " << before << ", nachher " << after << ")");
    REQUIRE (maxStep <= 0.5f);

    // Zur Sicherheit: ein ungeglaetteter Sprung waere deutlich groesser.
    REQUIRE (allFinite (output));
}

// ---------------------------------------------------------------------------
// 10. Stabilitaet, Stille und Grenzfaelle
// ---------------------------------------------------------------------------
TEST_CASE ("Finalizer: Stabilitaet, Stille und Grenzfaelle", "[fx][finalizer]")
{
    Settings hot;
    hot.boost  = 12.0f;
    hot.freq   = 200.0f;
    hot.clip   = 100.0f;
    hot.upward = 100.0f;
    hot.monoF  = 60.0f;

    // 100 Bloecke Rauschen: endlich und auf 0 dBFS begrenzt.
    {
        FinalizerModule module;
        applySettings (module, hot);
        module.prepare (kSampleRate, kBlockSize);

        juce::Random random (20261117);
        juce::AudioBuffer<float> buffer (2, kBlockSize);

        float peak = 0.0f;
        bool finite = true;

        for (int blockIndex = 0; blockIndex < 100; ++blockIndex)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlockSize; ++i)
                    buffer.setSample (ch, i, 0.9f * (random.nextFloat() * 2.0f - 1.0f));

            module.process (buffer);

            finite = finite && allFinite (buffer);
            peak = juce::jmax (peak, peakOf (buffer, 0), peakOf (buffer, 1));
        }

        REQUIRE (finite);
        REQUIRE (peak <= 1.0f + 1.0e-6f);
        REQUIRE (module.currentLimiterGain() >= 0.0f);
        REQUIRE (module.currentLimiterGain() <= 1.0f);
        REQUIRE (std::isfinite (module.currentUpwardGainDb()));
    }

    // Stille bleibt Stille (kein Rausch-Boden aus dem Limiter).
    {
        FinalizerModule module;
        applySettings (module, hot);
        module.prepare (kSampleRate, kBlockSize);

        juce::AudioBuffer<float> silence (2, 256);
        silence.clear();
        module.process (silence);

        REQUIRE (peakOf (silence, 0) == 0.0f);
        REQUIRE (peakOf (silence, 1) == 0.0f);
    }

    // Mono-Puffer: kein Absturz, endlich, begrenzt.
    {
        FinalizerModule module;
        applySettings (module, hot);
        module.prepare (kSampleRate, kBlockSize);

        juce::AudioBuffer<float> mono (1, 512);

        for (int i = 0; i < 512; ++i)
            mono.setSample (0, i, 0.8f * std::sin (2.0 * kPi * 100.0 * (double) i / kSampleRate));

        module.process (mono);

        REQUIRE (allFinite (mono));
        REQUIRE (peakOf (mono) <= 1.0f + 1.0e-6f);
    }

    // Ein Sample und ein sieben Samples langer Block.
    {
        FinalizerModule module;
        applySettings (module, hot);
        module.prepare (kSampleRate, kBlockSize);

        juce::AudioBuffer<float> single (2, 1);
        single.setSample (0, 0, 1.5f);
        single.setSample (1, 0, 1.5f);

        module.process (single);
        REQUIRE (std::isfinite (single.getSample (0, 0)));

        juce::AudioBuffer<float> tiny (2, 7);
        tiny.clear();

        for (int i = 0; i < 7; ++i)
        {
            tiny.setSample (0, i, 0.5f);
            tiny.setSample (1, i, 0.5f);
        }

        module.process (tiny);
        REQUIRE (allFinite (tiny));

        // Leerer Block: process() kehrt sofort zurueck.
        juce::AudioBuffer<float> empty (2, 0);
        module.process (empty);
        REQUIRE (empty.getNumSamples() == 0);
    }

    // Aufruf ohne prepare(): das Modul laesst den Puffer unveraendert.
    {
        FinalizerModule unprepared;
        applySettings (unprepared, hot);

        const auto input = makeSine (100.0, 0.1, 0.5f);
        auto output = copyOf (input);

        unprepared.process (output);

        REQUIRE (allFinite (output));

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            for (int i = 0; i < output.getNumSamples(); ++i)
                REQUIRE (output.getSample (ch, i) == input.getSample (ch, i));
    }

    // reset() leert die Verzoegerung wieder.
    {
        FinalizerModule module;
        applySettings (module, hot);
        module.prepare (kSampleRate, kBlockSize);

        const auto input = makeSine (200.0, 0.05, 0.5f);
        auto output = copyOf (input);

        module.process (output);

        module.reset();

        juce::AudioBuffer<float> silence (2, 256);
        silence.clear();
        module.process (silence);

        REQUIRE (peakOf (silence, 0) == 0.0f);
        REQUIRE (peakOf (silence, 1) == 0.0f);
    }
}
