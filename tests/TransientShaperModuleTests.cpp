#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <limits>
#include <vector>

#include "fx/modules/TransientShaperModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr double kPi         = juce::MathConstants<double>::pi;

    // -----------------------------------------------------------------------
    // Hilfen
    // -----------------------------------------------------------------------

    /** Setzt die vier Parameter (Anzeige-Einheiten) und prepare()t das Modul. */
    void configure (TransientShaperModule& module,
                    float amount,
                    float attack,
                    float duck,
                    float speed,
                    double sampleRate = kSampleRate,
                    int blockSize = kBlockSize)
    {
        module.setParam (TransientShaperModule::idxAmount, amount);
        module.setParam (TransientShaperModule::idxAttack, attack);
        module.setParam (TransientShaperModule::idxDuck,   duck);
        module.setParam (TransientShaperModule::idxSpeed,  speed);
        module.prepare (sampleRate, blockSize);
    }

    /** Verarbeitet den Puffer blockweise (wie die Kette) und gibt das Ergebnis zurueck. */
    juce::AudioBuffer<float> runModule (const juce::AudioBuffer<float>& input,
                                       float amount,
                                       float attack,
                                       float duck,
                                       float speed,
                                       int blockSize = kBlockSize)
    {
        TransientShaperModule module;
        configure (module, amount, attack, duck, speed, kSampleRate, blockSize);

        juce::AudioBuffer<float> output (input.getNumChannels(), input.getNumSamples());

        for (int ch = 0; ch < input.getNumChannels(); ++ch)
            output.copyFrom (ch, 0, input, ch, 0, input.getNumSamples());

        const auto total = output.getNumSamples();

        for (int start = 0; start < total; start += blockSize)
        {
            const auto numSamples = juce::jmin (blockSize, total - start);

            juce::AudioBuffer<float> block (output.getNumChannels(), numSamples);

            for (int ch = 0; ch < output.getNumChannels(); ++ch)
                block.copyFrom (ch, 0, output, ch, start, numSamples);

            module.process (block);

            for (int ch = 0; ch < output.getNumChannels(); ++ch)
                output.copyFrom (ch, start, block, ch, 0, numSamples);
        }

        return output;
    }

    int sampleIndex (double seconds, int numSamples)
    {
        const auto index = (int) std::lround (seconds * kSampleRate);
        return juce::jlimit (0, numSamples, index);
    }

    /** RMS eines Kanals zwischen zwei Zeitpunkten (Sekunden). */
    float rmsBetween (const juce::AudioBuffer<float>& buffer,
                      int channel,
                      double startSeconds,
                      double endSeconds)
    {
        const auto start = sampleIndex (startSeconds, buffer.getNumSamples());
        const auto end   = juce::jmax (start, sampleIndex (endSeconds, buffer.getNumSamples()));
        const auto* data = buffer.getReadPointer (channel);

        double sum = 0.0;

        for (int i = start; i < end; ++i)
            sum += (double) data[i] * (double) data[i];

        const auto count = end - start;

        return (count > 0) ? (float) std::sqrt (sum / (double) count) : 0.0f;
    }

    /** Spitzenbetrag eines Kanals. */
    float peakOf (const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        float peak = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (buffer.getSample (channel, i)));

        return peak;
    }

    /** Pegel eines RMS-Werts in dBFS (Untergrenze -200 dB). */
    float dbfs (float rms)
    {
        return 20.0f * std::log10 (juce::jmax (rms, 1.0e-10f));
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

    /**
     * Kick-artiges Signal: 3 Samples Einschlag (0,7) plus ein abklingender
     * Koerper (0,35, tau = 120 ms, 55 Hz). Stereo, L und R bitgleich.
     */
    juce::AudioBuffer<float> makeKickSignal (double seconds = 0.400)
    {
        const auto numSamples = (int) std::lround (seconds * kSampleRate);
        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto t    = (double) i / kSampleRate;
            const auto body = 0.35 * std::exp (-t / 0.120)
                                   * std::sin (2.0 * kPi * 55.0 * t);

            const auto value = (float) (body + ((i < 3) ? 0.7 : 0.0));

            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);
        }

        return buffer;
    }

    /** Ein einzelner Impuls auf beiden Kanaelen, danach Stille. */
    juce::AudioBuffer<float> makeImpulse (int numSamples = 512, float amplitude = 0.4f)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        buffer.setSample (0, 0, amplitude);
        buffer.setSample (1, 0, amplitude);

        return buffer;
    }

    /** Konstanter Sinus auf beiden Kanaelen (kein Transient nach dem Einschwingen). */
    juce::AudioBuffer<float> makeSine (double frequencyHz, double seconds = 1.0, float amplitude = 0.5f)
    {
        const auto numSamples = (int) std::lround (seconds * kSampleRate);
        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = amplitude
                             * (float) std::sin (2.0 * kPi * frequencyHz * (double) i / kSampleRate);

            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);
        }

        return buffer;
    }

    /** Kurzer Einschlag (3 Samples), danach Stille - fuer den Huellkurven-Verlauf. */
    juce::AudioBuffer<float> makeBurst (double seconds = 0.100, float amplitude = 0.7f)
    {
        const auto numSamples = (int) std::lround (seconds * kSampleRate);
        juce::AudioBuffer<float> buffer (1, numSamples);
        buffer.clear();

        for (int i = 0; i < juce::jmin (3, numSamples); ++i)
            buffer.setSample (0, i, amplitude);

        return buffer;
    }
} // namespace

// ---------------------------------------------------------------------------
// Parametertabelle
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: Parametertabelle und Metadaten", "[fx][trans]")
{
    TransientShaperModule module;

    REQUIRE (module.type() == FxType::transientShaper);
    REQUIRE (module.displayName() == "Transient Shaper");
    REQUIRE (module.latencySamples() == 0);

    const auto& params = module.params();

    REQUIRE (params.size() == 4);

    REQUIRE (params[0].id == "amount");
    REQUIRE (params[1].id == "attack");
    REQUIRE (params[2].id == "duck");
    REQUIRE (params[3].id == "speed");

    REQUIRE (params[0].name == "Amount");
    REQUIRE (params[1].name == "Attack");
    REQUIRE (params[2].name == "Duck");
    REQUIRE (params[3].name == "Speed");

    // Bereiche und Defaults exakt wie im Modulvertrag.
    REQUIRE (params[0].minValue == 0.0f);
    REQUIRE (params[0].maxValue == 100.0f);
    REQUIRE (params[0].defaultValue == 100.0f);
    REQUIRE (params[0].unit == "%");

    REQUIRE (params[1].minValue == 0.0f);
    REQUIRE (params[1].maxValue == 100.0f);
    REQUIRE (params[1].defaultValue == 0.0f);
    REQUIRE (params[1].unit == "%");

    REQUIRE (params[2].minValue == 0.0f);
    REQUIRE (params[2].maxValue == 100.0f);
    REQUIRE (params[2].defaultValue == 0.0f);
    REQUIRE (params[2].unit == "%");

    REQUIRE (params[3].minValue == 16.0f);
    REQUIRE (params[3].maxValue == 200.0f);
    REQUIRE (params[3].defaultValue == 48.0f);
    REQUIRE (params[3].unit == "ms");

    // MIX und OUT GAIN gehoeren zur Kette, nicht zum Modul.
    for (const auto& desc : params)
    {
        REQUIRE (desc.id != "mix");
        REQUIRE (desc.id != "outGain");
        REQUIRE (desc.id != "on");
    }

    // Defaults sind im Modul gesetzt.
    REQUIRE (module.getParam (TransientShaperModule::idxAmount) == 100.0f);
    REQUIRE (module.getParam (TransientShaperModule::idxAttack) == 0.0f);
    REQUIRE (module.getParam (TransientShaperModule::idxDuck) == 0.0f);
    REQUIRE (module.getParam (TransientShaperModule::idxSpeed) == 48.0f);

    // Klemmen und ungueltige Indizes.
    module.setParam (TransientShaperModule::idxSpeed, -100.0f);
    REQUIRE (module.getParam (TransientShaperModule::idxSpeed) == 16.0f);

    module.setParam (TransientShaperModule::idxSpeed, 1000.0f);
    REQUIRE (module.getParam (TransientShaperModule::idxSpeed) == 200.0f);

    module.setParam (TransientShaperModule::idxAttack, 250.0f);
    REQUIRE (module.getParam (TransientShaperModule::idxAttack) == 100.0f);

    const auto before = module.getParam (TransientShaperModule::idxDuck);
    module.setParam (99, 50.0f);          // ungueltiger Index: keine Wirkung
    module.setParam (-1, 50.0f);
    REQUIRE (module.getParam (TransientShaperModule::idxDuck) == before);
    REQUIRE (module.getParam (99) == 0.0f);

    // Nicht-endliche Werte werden nicht uebernommen.
    module.setParam (TransientShaperModule::idxDuck, std::nanf (""));
    REQUIRE (module.getParam (TransientShaperModule::idxDuck) == before);
}

// ---------------------------------------------------------------------------
// amount == 0
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: amount 0 reicht bitgenau durch", "[fx][trans]")
{
    const auto input = makeKickSignal();

    // amount == 0 muss auch bei aufgedrehtem ATTACK/DUCK und heissem Signal
    // unveraendert durchreichen (kein Limiter, keine Glaettung).
    for (const auto gain : { 1.0f, 3.0f })
    {
        juce::AudioBuffer<float> hot (input.getNumChannels(), input.getNumSamples());

        for (int ch = 0; ch < input.getNumChannels(); ++ch)
            for (int i = 0; i < input.getNumSamples(); ++i)
                hot.setSample (ch, i, input.getSample (ch, i) * gain);

        const auto output = runModule (hot, 0.0f, 100.0f, 100.0f, 16.0f);

        float maxDiff = 0.0f;

        for (int ch = 0; ch < hot.getNumChannels(); ++ch)
            for (int i = 0; i < hot.getNumSamples(); ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (output.getSample (ch, i)
                                                         - hot.getSample (ch, i)));

        INFO ("Verstaerkung " << gain);
        REQUIRE (maxDiff <= 1.0e-6f);
    }

    // Auch ueber viele Bloecke bleibt es ein Durchreichen (kein Zustandsleck).
    TransientShaperModule module;
    configure (module, 0.0f, 100.0f, 100.0f, 48.0f);

    juce::AudioBuffer<float> blocks (2, kBlockSize);

    for (int b = 0; b < 50; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                blocks.setSample (ch, i, 0.5f * std::sin (0.01f * (float) (b * kBlockSize + i)));

        juce::AudioBuffer<float> reference (2, kBlockSize);
        reference.makeCopyOf (blocks);

        module.process (blocks);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                REQUIRE (blocks.getSample (ch, i) == reference.getSample (ch, i));
    }

    // Nach dem Aufdrehen von AMOUNT arbeitet das Modul wieder (kein Einrasten
    // mit alter Huellkurven-Energie). prepare() uebernimmt die Parameter in die
    // Glaettung, damit der Test ab dem ersten Sample die volle Wirkung sieht.
    module.setParam (TransientShaperModule::idxAmount, 100.0f);
    module.setParam (TransientShaperModule::idxAttack, 100.0f);
    module.prepare (kSampleRate, kBlockSize);

    auto impulse = makeImpulse();
    module.process (impulse);

    REQUIRE (peakOf (impulse) > 0.4f * 1.2f);
}

// ---------------------------------------------------------------------------
// ATTACK
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: ATTACK hebt den Einschlag an", "[fx][trans]")
{
    const auto impulse = makeImpulse();

    const auto flat    = runModule (impulse, 100.0f, 0.0f,   0.0f, 48.0f);
    const auto boosted = runModule (impulse, 100.0f, 100.0f, 0.0f, 48.0f);

    const auto peakFlat    = peakOf (flat);
    const auto peakBoosted = peakOf (boosted);

    // Messung (48 kHz): 0,4000 -> 0,7866, also Faktor 1,97.
    REQUIRE_THAT (peakFlat, WithinAbs (0.4f, 1.0e-6f));
    REQUIRE (peakBoosted > peakFlat * 1.2f);

    // Die Anhebung laeuft in die weiche Saettigung, nicht in einen Sprung.
    REQUIRE (peakBoosted <= TransientShaperModule::kSoftCeiling);

    // Nach dem Einschlag bleibt Stille still (kein Nachklingen, keine NaN).
    REQUIRE (rmsBetween (boosted, 0, 64.0 / kSampleRate, 512.0 / kSampleRate) < 1.0e-9f);
    REQUIRE (allFinite (boosted));

    // Ohne ATTACK (Default) bleibt der Impuls unveraendert.
    for (int i = 0; i < flat.getNumSamples(); ++i)
        REQUIRE (flat.getSample (0, i) == impulse.getSample (0, i));
}

// ---------------------------------------------------------------------------
// DUCK
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: DUCK senkt den Pegel nach dem Einschlag", "[fx][trans]")
{
    const auto kick = makeKickSignal();

    const auto withoutDuck = runModule (kick, 100.0f, 0.0f,   0.0f, 48.0f);
    const auto withDuck    = runModule (kick, 100.0f, 0.0f, 100.0f, 48.0f);

    const auto rmsNoDuck = rmsBetween (withoutDuck, 0, 0.080, 0.200);
    const auto rmsDuck   = rmsBetween (withDuck,    0, 0.080, 0.200);

    // Messung (48 kHz): -21,68 dB -> -26,15 dB, also -4,48 dB.
    REQUIRE (rmsDuck < rmsNoDuck * 0.8f);
    REQUIRE (rmsDuck > 0.0f);

    // DUCK wirkt auf den Dauer-Anteil und laesst den Einschlag selbst stehen
    // (Messung: 0,7050 -> 0,6958, also -0,13 dB).
    REQUIRE (peakOf (withDuck) >= peakOf (withoutDuck) * 0.97f);
    REQUIRE (allFinite (withoutDuck));
    REQUIRE (allFinite (withDuck));
}

// ---------------------------------------------------------------------------
// Dauer-Signale
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: Dauer-Sinus wird nicht verbogen", "[fx][trans]")
{
    const std::vector<double> frequencies { 60.0, 100.0, 1000.0 };

    for (const auto frequency : frequencies)
    {
        const auto sine = makeSine (frequency);

        // ATTACK darf ein Dauer-Signal nicht dauerhaft anheben: nach dem
        // Einschwingen muss der Pegel praktisch unveraendert sein.
        const auto shaped = runModule (sine, 100.0f, 100.0f, 0.0f, 48.0f);

        const auto inLevel  = dbfs (rmsBetween (sine,   0, 0.5, 1.0));
        const auto outLevel = dbfs (rmsBetween (shaped, 0, 0.5, 1.0));

        INFO ("Frequenz " << frequency << " Hz, Eingang " << inLevel << " dB, Ausgang " << outLevel << " dB");
        REQUIRE (std::abs (outLevel - inLevel) <= 3.0f);
        REQUIRE (allFinite (shaped));
    }

    // DUCK senkt den Dauer-Anteil, aber gleichmaessig: kein Pumpen, keine
    // Verbiegung des Signalverlaufs (Streuung der 20-ms-Fensterpegel < 1 dB).
    const auto sine = makeSine (100.0);
    const auto ducked = runModule (sine, 100.0f, 0.0f, 100.0f, 48.0f);

    float minLevel = 1.0e9f;
    float maxLevel = -1.0e9f;

    for (int i = 0; i < 25; ++i)
    {
        const auto start = 0.40 + 0.02 * (double) i;
        const auto level = dbfs (rmsBetween (ducked, 0, start, start + 0.02));

        minLevel = juce::jmin (minLevel, level);
        maxLevel = juce::jmax (maxLevel, level);
    }

    REQUIRE (maxLevel - minLevel < 1.0f);

    // Default-Parameter: reines Durchreichen (gain ist exakt 1).
    const auto untouched = runModule (sine, 100.0f, 0.0f, 0.0f, 48.0f);

    for (int i = 0; i < sine.getNumSamples(); ++i)
        REQUIRE (untouched.getSample (0, i) == sine.getSample (0, i));
}

// ---------------------------------------------------------------------------
// SPEED
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: SPEED aendert die Erholzeit", "[fx][trans]")
{
    // (a) Huellkurven-Verlauf: nach einem Einschlag ist der schnelle Follower
    //     bei SPEED = 16 ms nach 100 ms praktisch erholt, bei 200 ms nicht.
    auto fastEnvelopeAfter100ms = [] (float speed)
    {
        TransientShaperModule module;
        configure (module, 100.0f, 0.0f, 0.0f, speed);

        auto burst = makeBurst (0.100);

        for (int start = 0; start < burst.getNumSamples(); start += kBlockSize)
        {
            const auto numSamples = juce::jmin (kBlockSize, burst.getNumSamples() - start);
            juce::AudioBuffer<float> block (burst.getNumChannels(), numSamples);

            for (int ch = 0; ch < burst.getNumChannels(); ++ch)
                block.copyFrom (ch, 0, burst, ch, start, numSamples);

            module.process (block);
        }

        return module.currentFastEnvelope();
    };

    const auto envelopeFast = fastEnvelopeAfter100ms (16.0f);
    const auto envelopeSlow = fastEnvelopeAfter100ms (200.0f);

    // Messung (48 kHz): 1,59e-4 gegen 4,99e-2, also Faktor 313.
    REQUIRE (envelopeFast < envelopeSlow * 0.2f);
    REQUIRE (envelopeFast >= 0.0f);

    // (b) Pegelverlauf: nach dem Einschlag duckt SPEED = 16 ms den Koerper
    //     deutlich staerker weg als SPEED = 200 ms (Messung: -33,4 gegen
    //     -30,6 dB im Fenster 150...300 ms).
    const auto kick = makeKickSignal();

    const auto fast = runModule (kick, 100.0f, 0.0f, 100.0f, 16.0f);
    const auto slow = runModule (kick, 100.0f, 0.0f, 100.0f, 200.0f);

    const auto levelFast = dbfs (rmsBetween (fast, 0, 0.150, 0.300));
    const auto levelSlow = dbfs (rmsBetween (slow, 0, 0.150, 0.300));

    INFO ("SPEED 16 ms: " << levelFast << " dB, SPEED 200 ms: " << levelSlow << " dB");
    REQUIRE (levelSlow > levelFast + 1.0f);
    REQUIRE (allFinite (fast));
    REQUIRE (allFinite (slow));
}

// ---------------------------------------------------------------------------
// Stereo
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: Stereo-Kohaerenz", "[fx][trans]")
{
    const auto kick = makeKickSignal();

    // Identisches Signal auf L/R bleibt bitgleich (gemeinsame Huellkurve).
    const auto output = runModule (kick, 100.0f, 80.0f, 60.0f, 64.0f);

    for (int i = 0; i < output.getNumSamples(); ++i)
        REQUIRE (output.getSample (0, i) == output.getSample (1, i));

    // Ein Einschlag auf nur einem Kanal wirkt auch auf den anderen: der linke
    // Dauer-Ton wird angehoben, obwohl rechts nur der Einschlag sitzt.
    // (Ein kanalweises Design wuerde links nichts tun.)
    const auto numSamples = (int) std::lround (0.250 * kSampleRate);
    const auto burstStart = (int) std::lround (0.100 * kSampleRate);

    juce::AudioBuffer<float> withBurst (2, numSamples);
    juce::AudioBuffer<float> withoutBurst (2, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto tone = 0.05f * (float) std::sin (2.0 * kPi * 80.0 * (double) i / kSampleRate);

        withBurst.setSample (0, i, tone);
        withoutBurst.setSample (0, i, tone);
        withBurst.setSample (1, i, 0.0f);
        withoutBurst.setSample (1, i, 0.0f);
    }

    for (int i = 0; i < 3; ++i)
        withBurst.setSample (1, burstStart + i, 0.6f);

    const auto shapedWith    = runModule (withBurst,    100.0f, 100.0f, 0.0f, 48.0f);
    const auto shapedWithout = runModule (withoutBurst, 100.0f, 100.0f, 0.0f, 48.0f);

    const auto levelWith    = dbfs (rmsBetween (shapedWith,    0, 0.100, 0.110));
    const auto levelWithout = dbfs (rmsBetween (shapedWithout, 0, 0.100, 0.110));

    // Messung: -25,5 gegen -27,8 dB, also +2,27 dB.
    INFO ("links mit Burst rechts: " << levelWith << " dB, ohne: " << levelWithout << " dB");
    REQUIRE (levelWith > levelWithout + 1.0f);
    REQUIRE (allFinite (shapedWith));
}

// ---------------------------------------------------------------------------
// Stabilitaet
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: 100 Bloecke Rauschen bleiben endlich", "[fx][trans]")
{
    TransientShaperModule module;
    configure (module, 100.0f, 100.0f, 100.0f, 16.0f);

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

        for (int ch = 0; ch < 2; ++ch)
            peak = juce::jmax (peak, peakOf (buffer, ch));
    }

    REQUIRE (finite);
    REQUIRE (std::isfinite (module.currentFastEnvelope()));
    REQUIRE (std::isfinite (module.currentSlowEnvelope()));
    REQUIRE (std::isfinite (module.currentGain()));

    // Der Ausgang wird auf die Saettigungs-Asymptote begrenzt (1,25 = +1,94 dBFS).
    REQUIRE (peak <= TransientShaperModule::kSoftCeiling + 1.0e-4f);
    REQUIRE (module.currentGain() >= TransientShaperModule::kGainMin);
    REQUIRE (module.currentGain() <= TransientShaperModule::kGainMax);
}

// ---------------------------------------------------------------------------
// Grenzfaelle: Stille, DC, sehr kurze Bloecke, Aufruf ohne prepare()
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: Stille, DC und sehr kurze Bloecke", "[fx][trans]")
{
    // Stille bleibt Stille (und die Huellkurven laufen nicht weg).
    juce::AudioBuffer<float> silence (2, 256);
    silence.clear();

    TransientShaperModule module;
    configure (module, 100.0f, 100.0f, 100.0f, 16.0f);
    module.process (silence);

    REQUIRE (peakOf (silence, 0) == 0.0f);
    REQUIRE (peakOf (silence, 1) == 0.0f);
    REQUIRE (module.currentFastEnvelope() == 0.0f);
    REQUIRE (module.currentSlowEnvelope() == 0.0f);

    // DC: kein Drift, kein Pumpen - der Gleichanteil kommt konstant durch.
    const auto dcSamples = (int) std::lround (0.200 * kSampleRate);
    juce::AudioBuffer<float> dc (2, dcSamples);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < dcSamples; ++i)
            dc.setSample (ch, i, 0.5f);

    const auto dcOut = runModule (dc, 100.0f, 100.0f, 0.0f, 48.0f);

    REQUIRE_THAT (dcOut.getSample (0, dcSamples - 1), WithinAbs (0.5f, 1.0e-2f));
    REQUIRE (allFinite (dcOut));

    float minTail = 1.0e9f;
    float maxTail = 0.0f;

    for (int i = dcSamples - 2400; i < dcSamples; ++i)
    {
        const auto value = std::abs (dcOut.getSample (0, i));
        minTail = juce::jmin (minTail, value);
        maxTail = juce::jmax (maxTail, value);
    }

    REQUIRE (maxTail / minTail < 1.01f);   // < 0,09 dB Restwelligkeit

    // Einzelner Sample-Block und ein sieben-Sample-Block: endlich, kein Absturz.
    juce::AudioBuffer<float> single (2, 1);
    single.setSample (0, 0, 0.5f);
    single.setSample (1, 0, 0.5f);

    module.process (single);
    REQUIRE (std::isfinite (single.getSample (0, 0)));
    REQUIRE (std::abs (single.getSample (0, 0)) <= TransientShaperModule::kSoftCeiling);

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

    // Aufruf ohne prepare(): blieb bisher folgenlos, muss aber stabil sein.
    TransientShaperModule unprepared;
    unprepared.setParam (TransientShaperModule::idxAmount, 100.0f);
    unprepared.setParam (TransientShaperModule::idxAttack, 100.0f);
    unprepared.setParam (TransientShaperModule::idxDuck, 100.0f);

    auto kick = makeKickSignal();

    juce::AudioBuffer<float> unpreparedOut (2, kick.getNumSamples());
    unpreparedOut.makeCopyOf (kick);
    unprepared.process (unpreparedOut);

    REQUIRE (allFinite (unpreparedOut));
    REQUIRE (peakOf (unpreparedOut) <= TransientShaperModule::kSoftCeiling + 1.0e-4f);
}

// ---------------------------------------------------------------------------
// Saettigung als Einzelfunktion
// ---------------------------------------------------------------------------
TEST_CASE ("Transient Shaper: weiche Saettigung ab 0 dBFS", "[fx][trans]")
{
    using Module = TransientShaperModule;

    // Unter 0 dBFS bitgenau.
    for (const auto value : { -1.0f, -0.5f, -0.0f, 0.0f, 0.25f, 0.5f, 1.0f })
        REQUIRE (Module::softLimit (value) == value);

    // Ueber 0 dBFS monoton, nach oben durch die Asymptote begrenzt.
    float previous = Module::softLimit (1.0f);

    REQUIRE (previous == 1.0f);

    for (float value = 1.0f; value < 8.0f; value += 0.1f)
    {
        const auto limited = Module::softLimit (value);

        REQUIRE (limited >= previous);                  // monoton nicht fallend
        REQUIRE (limited <= Module::kSoftCeiling);      // harte Obergrenze
        previous = limited;
    }

    // Weit jenseits von 0 dBFS bleibt es bei der Asymptote (1,25 = +1,94 dBFS).
    REQUIRE (Module::softLimit (2.0f) < Module::kSoftCeiling);
    REQUIRE (Module::softLimit (1.0e9f) == Module::kSoftCeiling);

    // Symmetrie.
    REQUIRE (Module::softLimit (-1.6f) == -Module::softLimit (1.6f));

    // Nicht-endliche Werte werden verworfen (kein NaN im Ausgang).
    REQUIRE (Module::softLimit (std::nanf ("")) == 0.0f);
    REQUIRE (Module::softLimit (std::numeric_limits<float>::infinity()) == 0.0f);

    // Der Koeffizienten-Helfer bleibt im gueltigen Bereich.
    REQUIRE (Module::onePoleCoefficient (0.0f, kSampleRate) == 1.0f);
    REQUIRE (Module::onePoleCoefficient (-5.0f, kSampleRate) == 1.0f);
    REQUIRE (Module::onePoleCoefficient (10.0f, 0.0) == 1.0f);

    const auto coefficient = Module::onePoleCoefficient (10.0f, kSampleRate);
    REQUIRE (coefficient > 0.0f);
    REQUIRE (coefficient < 1.0f);
}
