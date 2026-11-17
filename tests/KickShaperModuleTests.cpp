#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

#include "fx/modules/KickShaperModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr double kPi         = juce::MathConstants<double>::pi;

    using Pt = KickShaperModule::Pt;

    // -----------------------------------------------------------------------
    // Hilfen
    // -----------------------------------------------------------------------

    /** Setzt die fuenf Parameter (Anzeige-Einheiten) und prepare()t das Modul. */
    void configure (KickShaperModule& module,
                    float sPre,
                    float sPost,
                    float sLine,
                    float shape,
                    float amount)
    {
        module.setParam (KickShaperModule::idxSPre,   sPre);
        module.setParam (KickShaperModule::idxSPost,  sPost);
        module.setParam (KickShaperModule::idxSLine,  sLine);
        module.setParam (KickShaperModule::idxShape,  shape);
        module.setParam (KickShaperModule::idxAmount, amount);
        module.prepare (kSampleRate, kBlockSize);
    }

    /**
     * Verarbeitet den Puffer blockweise (wie die Kette) und gibt das Ergebnis
     * zurueck. Jeder Block wird frisch aus dem **Eingang** gefuellt, damit kein
     * Ausgang in den naechsten Block zuruecklaeuft.
     */
    juce::AudioBuffer<float> runModule (const juce::AudioBuffer<float>& input,
                                        const std::vector<Pt>& points,
                                        float sPre,
                                        float sPost,
                                        float sLine,
                                        float shape,
                                        float amount,
                                        int blockSize = kBlockSize)
    {
        KickShaperModule module;
        configure (module, sPre, sPost, sLine, shape, amount);

        if (! points.empty())
            module.setEnvelopePoints (points);

        const auto numChannels = input.getNumChannels();
        const auto total       = input.getNumSamples();

        juce::AudioBuffer<float> output (numChannels, total);

        for (int start = 0; start < total; start += blockSize)
        {
            const auto numSamples = juce::jmin (blockSize, total - start);

            juce::AudioBuffer<float> block (numChannels, numSamples);

            for (int ch = 0; ch < numChannels; ++ch)
                block.copyFrom (ch, 0, input, ch, start, numSamples);

            module.process (block);

            for (int ch = 0; ch < numChannels; ++ch)
                output.copyFrom (ch, start, block, ch, 0, numSamples);
        }

        return output;
    }

    /** Groesster Betrag einer Differenz zwischen zwei Puffern. */
    float maxAbsDifference (const juce::AudioBuffer<float>& a,
                           const juce::AudioBuffer<float>& b)
    {
        const auto numChannels = juce::jmin (a.getNumChannels(), b.getNumChannels());
        const auto numSamples  = juce::jmin (a.getNumSamples(),  b.getNumSamples());

        float maximum = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                maximum = juce::jmax (maximum, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));

        return maximum;
    }

    /** RMS eines Kanals zwischen zwei Zeitpunkten (Sekunden). */
    float rmsBetween (const juce::AudioBuffer<float>& buffer,
                      int channel,
                      double startSeconds,
                      double endSeconds)
    {
        const auto numSamples = buffer.getNumSamples();
        const auto start = juce::jlimit (0, numSamples, (int) std::lround (startSeconds * kSampleRate));
        const auto end   = juce::jlimit (start, numSamples, (int) std::lround (endSeconds * kSampleRate));

        const auto* data = buffer.getReadPointer (channel);

        double sum = 0.0;

        for (int i = start; i < end; ++i)
            sum += (double) data[i] * (double) data[i];

        const auto count = end - start;

        return (count > 0) ? (float) std::sqrt (sum / (double) count) : 0.0f;
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
    // Testsignale (Stereo, L und R bitgleich)
    // -----------------------------------------------------------------------

    juce::AudioBuffer<float> makeImpulse (int numSamples = 512, float amplitude = 0.4f)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        buffer.setSample (0, 0, amplitude);
        buffer.setSample (1, 0, amplitude);

        return buffer;
    }

    juce::AudioBuffer<float> makeSine (double frequencyHz, double seconds, float amplitude)
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

    juce::AudioBuffer<float> makeConstant (int numSamples, float value)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample (ch, i, value);

        return buffer;
    }
} // namespace

// ---------------------------------------------------------------------------
// Parameter und Metadaten
// ---------------------------------------------------------------------------
TEST_CASE ("Kick Shaper: Parametertabelle und Metadaten", "[fx][kickshaper]")
{
    KickShaperModule module;

    REQUIRE (module.type() == FxType::kickShaper);
    REQUIRE (module.displayName() == "Kick Shaper");
    REQUIRE (module.latencySamples() == 0);

    const auto& params = module.params();

    REQUIRE (params.size() == 5);

    // Kennungen und Reihenfolge exakt wie im Auftrag/Ticket B-64.
    REQUIRE (params[0].id == "sPre");
    REQUIRE (params[1].id == "sPost");
    REQUIRE (params[2].id == "sLine");
    REQUIRE (params[3].id == "shape");
    REQUIRE (params[4].id == "amount");

    REQUIRE (params[0].name == "Pre Gain");
    REQUIRE (params[1].name == "Post Gain");
    REQUIRE (params[2].name == "Linear");
    REQUIRE (params[3].name == "Shape");
    REQUIRE (params[4].name == "Amount");

    REQUIRE (params[0].minValue == 0.0f);
    REQUIRE (params[0].maxValue == 3.5f);
    REQUIRE (params[0].defaultValue == 1.0f);
    REQUIRE (params[0].unit == "x");

    REQUIRE (params[1].minValue == 0.0f);
    REQUIRE (params[1].maxValue == 3.5f);
    REQUIRE (params[1].defaultValue == 1.0f);
    REQUIRE (params[1].unit == "x");

    REQUIRE (params[2].minValue == 0.0f);
    REQUIRE (params[2].maxValue == 1.0f);
    REQUIRE (params[2].defaultValue == 1.0f);
    REQUIRE (params[2].unit == "");

    // shape hat acht Modi (0...7), nicht 0...2.
    REQUIRE (params[3].minValue == 0.0f);
    REQUIRE (params[3].maxValue == 7.0f);
    REQUIRE (params[3].defaultValue == 0.0f);
    REQUIRE (params[3].unit == "");

    REQUIRE (params[4].minValue == 0.0f);
    REQUIRE (params[4].maxValue == 100.0f);
    REQUIRE (params[4].defaultValue == 100.0f);
    REQUIRE (params[4].unit == "%");

    // MIX und OUT GAIN gehoeren zur Kette, nicht zum Modul.
    for (const auto& desc : params)
    {
        REQUIRE (desc.id != "mix");
        REQUIRE (desc.id != "outGain");
        REQUIRE (desc.id != "on");
    }

    // Defaults stehen im Modul an.
    REQUIRE (module.getParam (KickShaperModule::idxSPre)   == 1.0f);
    REQUIRE (module.getParam (KickShaperModule::idxSPost)  == 1.0f);
    REQUIRE (module.getParam (KickShaperModule::idxSLine)  == 1.0f);
    REQUIRE (module.getParam (KickShaperModule::idxShape)  == 0.0f);
    REQUIRE (module.getParam (KickShaperModule::idxAmount) == 100.0f);
}

TEST_CASE ("Kick Shaper: setParam klemmt und ignoriert ungueltige Indizes", "[fx][kickshaper]")
{
    KickShaperModule module;

    // Klemmen auf den deklarierten Bereich.
    module.setParam (KickShaperModule::idxAmount, 500.0f);
    REQUIRE (module.getParam (KickShaperModule::idxAmount) == 100.0f);

    module.setParam (KickShaperModule::idxSPre, -1.0f);
    REQUIRE (module.getParam (KickShaperModule::idxSPre) == 0.0f);

    module.setParam (KickShaperModule::idxShape, 99.0f);
    REQUIRE (module.getParam (KickShaperModule::idxShape) == 7.0f);

    module.setParam (KickShaperModule::idxShape, -5.0f);
    REQUIRE (module.getParam (KickShaperModule::idxShape) == 0.0f);

    module.setParam (KickShaperModule::idxSPost, 10.0f);
    REQUIRE (module.getParam (KickShaperModule::idxSPost) == 3.5f);

    // Ungueltige Indizes sind ein No-op, getParam() liefert dort 0.
    const auto before = module.getParam (KickShaperModule::idxSLine);
    module.setParam (99, 0.0f);
    module.setParam (-1, 0.0f);
    REQUIRE (module.getParam (KickShaperModule::idxSLine) == before);
    REQUIRE (module.getParam (99) == 0.0f);

    // Nicht-endliche Werte werden nicht uebernommen.
    const auto amountBefore = module.getParam (KickShaperModule::idxAmount);

    module.setParam (KickShaperModule::idxAmount, std::nanf (""));
    REQUIRE (module.getParam (KickShaperModule::idxAmount) == amountBefore);
}

// ---------------------------------------------------------------------------
// amount == 0 und amount == 100
// ---------------------------------------------------------------------------
TEST_CASE ("Kick Shaper: amount 0 reicht bitgenau durch", "[fx][kickshaper]")
{
    // Nicht-neutrale Huellkurve: nur AMOUNT (bzw. sLine) verhindert die Formung.
    const std::vector<Pt> curve { { 0.0, 0.0, 0 }, { 0.35, 1.0, 0 }, { 1.0, 0.25, 0 } };

    const auto impulse = makeImpulse (1024, 0.4f);
    const auto sine    = makeSine (80.0, 0.25, 0.7f);

    for (const auto sLine : { 0.0f, 1.0f })
    {
        INFO ("sLine " << sLine);

        const auto shapedImpulse = runModule (impulse, curve, 1.0f, 1.0f, sLine, 3.0f, 0.0f);
        REQUIRE (maxAbsDifference (shapedImpulse, impulse) == 0.0f);

        // 0,25 s = 94 Bloecke: auch ueber viele Bloecke kein Zustandsleck.
        const auto shapedSine = runModule (sine, curve, 1.0f, 1.0f, sLine, 3.0f, 0.0f);
        REQUIRE (maxAbsDifference (shapedSine, sine) == 0.0f);
    }

    // Auch ein heisses Signal (Spitze 2,5) bleibt bei amount = 0 unberuehrt.
    juce::AudioBuffer<float> hot (2, 4096);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i)
            hot.setSample (ch, i, 2.5f * (float) std::sin (0.01 * (double) i));

    const auto hotOut = runModule (hot, curve, 1.0f, 1.0f, 0.0f, 7.0f, 0.0f);
    REQUIRE (maxAbsDifference (hotOut, hot) == 0.0f);
}

TEST_CASE ("Kick Shaper: amount 100 formt das Signal", "[fx][kickshaper]")
{
    // Huellkurve, die die Kick-Mitte absenkt: 1,0 -> 0,15 -> 1,0.
    const std::vector<Pt> curve { { 0.0, 1.0, 0 }, { 0.5, 0.15, 0 }, { 1.0, 1.0, 0 } };

    const auto input  = makeSine (80.0, 0.25, 0.5f);
    const auto shaped = runModule (input, curve, 1.0f, 1.0f, 0.0f, 1.0f, 100.0f);

    REQUIRE (maxAbsDifference (shaped, input) > 0.01f);
    REQUIRE (allFinite (shaped));

    // Dokumentierte Folge der Huellkurve: bei Position 0,25 (Sekunde 0,125) ist
    // der Faktor 1,0 + (0,15 - 1,0) * 0,5 = 0,575, das Fenster ist also leiser.
    const auto inputLevel  = rmsBetween (input,  0, 0.12, 0.13);
    const auto shapedLevel = rmsBetween (shaped, 0, 0.12, 0.13);

    INFO ("Eingang " << inputLevel << ", Ausgang " << shapedLevel);
    REQUIRE (shapedLevel < inputLevel * 0.75f);
    REQUIRE (shapedLevel > inputLevel * 0.4f);
}

TEST_CASE ("Kick Shaper: sLine schaltet die Umformung hart ab", "[fx][kickshaper]")
{
    const std::vector<Pt> curve { { 0.0, 0.0, 0 }, { 0.5, 1.0, 0 }, { 1.0, 0.0, 0 } };

    const auto input = makeSine (80.0, 0.25, 0.5f);

    // sLine = 1: Sicherung an, amount wird ignoriert.
    const auto linear = runModule (input, curve, 1.0f, 1.0f, 1.0f, 2.0f, 100.0f);
    REQUIRE (maxAbsDifference (linear, input) == 0.0f);

    // Die Schwelle liegt bei 0,5: auch 0,5 ist "Sicherung an".
    const auto half = runModule (input, curve, 1.0f, 1.0f, 0.5f, 2.0f, 100.0f);
    REQUIRE (maxAbsDifference (half, input) == 0.0f);

    // 0,49 ist "Sicherung aus": die Umformung wirkt wieder.
    const auto shaped = runModule (input, curve, 1.0f, 1.0f, 0.49f, 2.0f, 100.0f);
    REQUIRE (maxAbsDifference (shaped, input) > 0.01f);

    // sPre/sPost wirken auch bei Sicherung - nur die Umformung entfaellt.
    const auto gained = runModule (input, curve, 2.0f, 2.0f, 1.0f, 2.0f, 100.0f);

    for (int ch = 0; ch < input.getNumChannels(); ++ch)
        for (int i = 0; i < input.getNumSamples(); ++i)
            REQUIRE_THAT (gained.getSample (ch, i),
                          WithinAbs (input.getSample (ch, i) * 4.0f, 1.0e-6f));
}

// ---------------------------------------------------------------------------
// Struktur-Zustand
// ---------------------------------------------------------------------------
TEST_CASE ("Kick Shaper: Pt-Huellkurve als Struktur-Zustand", "[fx][kickshaper]")
{
    KickShaperModule module;

    // (a) Werkszustand: zwei neutrale Punkte, Skalare als Attribute.
    const auto defaults = module.getState();

    REQUIRE (defaults.hasType (juce::Identifier ("Module")));
    REQUIRE (defaults.getNumChildren() == 2);
    REQUIRE ((double) defaults.getProperty ("sPre")   == 1.0);
    REQUIRE ((double) defaults.getProperty ("sPost")  == 1.0);
    REQUIRE ((double) defaults.getProperty ("sLine")  == 1.0);
    REQUIRE ((double) defaults.getProperty ("shape")  == 0.0);
    REQUIRE ((double) defaults.getProperty ("amount") == 100.0);

    for (int i = 0; i < 2; ++i)
    {
        const auto child = defaults.getChild (i);
        REQUIRE (child.hasType (juce::Identifier ("Pt")));
        REQUIRE ((double) child.getProperty ("y") == 1.0);   // neutral
    }

    // (b) Vier Punkte setState -> dieselben vier Punkte in getState.
    juce::ValueTree state ("Module");
    state.setProperty ("sPre", 2.5, nullptr);
    state.setProperty ("shape", 4, nullptr);
    state.setProperty ("amount", 37.5, nullptr);

    const std::vector<double> expectedX { 0.0, 0.25, 0.6, 1.0 };
    const std::vector<double> expectedY { 1.0, 0.8, 0.3, 0.1 };
    const std::vector<int>    expectedT { 0, 1, 2, 3 };

    for (size_t i = 0; i < expectedX.size(); ++i)
    {
        juce::ValueTree child ("Pt");
        child.setProperty ("x", expectedX[i], nullptr);
        child.setProperty ("y", expectedY[i], nullptr);
        child.setProperty ("t", expectedT[i], nullptr);
        state.appendChild (child, nullptr);
    }

    module.setState (state);

    // Skalare sind angekommen (Anzeige-Einheiten).
    REQUIRE (module.getParam (KickShaperModule::idxSPre)   == 2.5f);
    REQUIRE (module.getParam (KickShaperModule::idxShape)  == 4.0f);
    REQUIRE (module.getParam (KickShaperModule::idxAmount) == 37.5f);

    const auto points = module.envelopePoints();
    REQUIRE (points.size() == expectedX.size());

    // Roundtrip: getState() liefert dieselben vier Punkte mit x/y/t.
    const auto roundTrip = module.getState();
    REQUIRE (roundTrip.getNumChildren() == (int) expectedX.size());

    for (int i = 0; i < (int) expectedX.size(); ++i)
    {
        const auto child = roundTrip.getChild (i);

        INFO ("Punkt " << i);
        REQUIRE (child.hasType (juce::Identifier ("Pt")));
        REQUIRE_THAT ((double) child.getProperty ("x"), WithinAbs (expectedX[(size_t) i], 1.0e-6));
        REQUIRE_THAT ((double) child.getProperty ("y"), WithinAbs (expectedY[(size_t) i], 1.0e-6));
        REQUIRE ((int) child.getProperty ("t") == expectedT[(size_t) i]);
    }

    // (c) Leerer Zustand setzt die Werkskurve zurueck.
    module.setState (juce::ValueTree ("Module"));
    REQUIRE (module.envelopePoints().size() == 2);

    // (d) Und ein erneutes setState() mit dem Roundtrip-Zustand ist stabil.
    module.setState (roundTrip);
    REQUIRE (module.envelopePoints().size() == expectedX.size());
}

TEST_CASE ("Kick Shaper: hoechstens 32 Punkte, Klemmen, unbekannte Kinder", "[fx][kickshaper]")
{
    KickShaperModule module;

    juce::ValueTree state ("Module");

    // Unbekanntes Kind zuerst: es wird ignoriert, ohne die Punkte zu stoeren.
    juce::ValueTree unknown ("Unknown");
    state.appendChild (unknown, nullptr);

    // 40 Punkte mit Werten ausserhalb der Bereiche.
    for (int i = 0; i < 40; ++i)
    {
        juce::ValueTree child ("Pt");
        child.setProperty ("x", (i % 2 == 0) ? -0.5 : 1.5, nullptr);   // -> 0 bzw. 1
        child.setProperty ("y", 2.0, nullptr);                         // -> 1
        child.setProperty ("t", 9, nullptr);                           // -> 3
        state.appendChild (child, nullptr);
    }

    module.setState (state);

    REQUIRE (module.envelopePoints().size() == (size_t) KickShaperModule::maxPoints);

    for (const auto& point : module.envelopePoints())
    {
        REQUIRE (point.x >= 0.0);
        REQUIRE (point.x <= 1.0);
        REQUIRE (point.y >= 0.0);
        REQUIRE (point.y <= 1.0);
        REQUIRE (point.t >= 0);
        REQUIRE (point.t < KickShaperModule::kNumSegmentTypes);
    }

    // Eigene Liste ueber die API: gleiche Grenzen.
    module.setEnvelopePoints ({ { 2.0, -3.0, -1 } });
    REQUIRE (module.envelopePoints().size() == 1);
    REQUIRE (module.envelopePoints()[0].x == 1.0);
    REQUIRE (module.envelopePoints()[0].y == 0.0);
    REQUIRE (module.envelopePoints()[0].t == 0);
}

// ---------------------------------------------------------------------------
// Die acht Segmentmodi
// ---------------------------------------------------------------------------
TEST_CASE ("Kick Shaper: die acht Segmentmodi", "[fx][kickshaper]")
{
    // Gemeinsame Huellkurve: (0 | 0,2) -> (0,5 | 1,0) -> (1,0 | 0,5), alle
    // t = 0 ("folgt shape"). Im ersten Segment gilt u = x / 0,5 = 2x.
    const std::vector<Pt> curve { { 0.0, 0.2, 0 }, { 0.5, 1.0, 0 }, { 1.0, 0.5, 0 } };

    // Erwartungswert direkt aus der dokumentierten Definition (Spec 5).
    auto expectedBend = [] (int shapeMode, double u)
    {
        const auto x = juce::jlimit (0.0, 1.0, u);

        switch (shapeMode)
        {
            case 0: return 0.0;                                        // Halten
            case 1: return x;                                          // lineare Rampe
            case 2: return 1.0 - std::abs (2.0 * x - 1.0);             // Rampe mit Spiegelung
            case 3: return (x < 1.0) ? 0.0 : 1.0;                      // Stufen
            case 4:                                                    // exponentielle Annaeherung
            {
                const auto k = (double) KickShaperModule::kApproachConstant;
                return (1.0 - std::exp (-k * x)) / (1.0 - std::exp (-k));
            }
            case 5:                                                    // Halten + Spiegel
            {
                if (x < 0.5)
                    return 0.0;

                const auto v = 2.0 * x - 1.0;
                return v * v;
            }
            case 6: return std::sin (x * juce::MathConstants<double>::halfPi); // Halbwellen-Rampe
            case 7: return 1.0 - x;                                    // invertierte Rampe
            default: break;
        }

        return x;
    };

    auto expectedValue = [expectedBend] (int shapeMode, double u)
    {
        return 0.2 + (1.0 - 0.2) * expectedBend (shapeMode, u);
    };

    KickShaperModule module;
    module.setEnvelopePoints (curve);

    REQUIRE (module.envelopePoints().size() == 3);

    // (a) Wert an zwei Stuetzstellen je Modus (Position 0,125 -> u = 0,25;
    //     Position 0,25 -> u = 0,5).
    for (int shape = 0; shape < KickShaperModule::kNumShapes; ++shape)
    {
        INFO ("shape " << shape);

        module.setParam (KickShaperModule::idxShape, (float) shape);

        REQUIRE_THAT (module.evaluateEnvelopeForShape (shape, 0.125),
                      WithinAbs ((float) expectedValue (shape, 0.25), 1.0e-6f));
        REQUIRE_THAT (module.evaluateEnvelopeForShape (shape, 0.25),
                      WithinAbs ((float) expectedValue (shape, 0.5), 1.0e-6f));

        // evaluateEnvelope() folgt dem gesetzten Parameter.
        REQUIRE_THAT (module.evaluateEnvelope (0.125),
                      WithinAbs (module.evaluateEnvelopeForShape (shape, 0.125), 1.0e-6f));
    }

    // (b) Referenzwerte, die direkt aus der Definition folgen.
    module.setParam (KickShaperModule::idxShape, 1.0f);   // lineare Rampe
    REQUIRE_THAT (module.evaluateEnvelope (0.125), WithinAbs (0.4f, 1.0e-6f));
    REQUIRE_THAT (module.evaluateEnvelope (0.25),  WithinAbs (0.6f, 1.0e-6f));

    module.setParam (KickShaperModule::idxShape, 0.0f);   // Halten
    REQUIRE_THAT (module.evaluateEnvelope (0.125), WithinAbs (0.2f, 1.0e-6f));
    REQUIRE_THAT (module.evaluateEnvelope (0.40),  WithinAbs (0.2f, 1.0e-6f));

    module.setParam (KickShaperModule::idxShape, 2.0f);   // Rampe mit Spiegelung
    REQUIRE_THAT (module.evaluateEnvelope (0.25), WithinAbs (1.0f, 1.0e-6f));

    module.setParam (KickShaperModule::idxShape, 7.0f);   // invertierte Rampe
    REQUIRE_THAT (module.evaluateEnvelope (0.125), WithinAbs (0.8f, 1.0e-6f));

    // (c) Hoerbar: gleiche Huellkurve, acht verschiedene shape -> verschiedene
    //     Ausgaenge. Der Eingang ist DC 1,0, damit out[n] = env(n) gilt; die
    //     Probe bei Sample 6000 entspricht Position 0,25 (0,5 s Laenge).
    std::vector<float> probeValues;

    for (int shape = 0; shape < KickShaperModule::kNumShapes; ++shape)
    {
        const auto dc     = makeConstant (12001, 1.0f);
        const auto output = runModule (dc, curve, 1.0f, 1.0f, 0.0f, (float) shape, 100.0f);

        const auto probe = output.getSample (0, 6000);

        INFO ("shape " << shape << " -> " << probe);
        REQUIRE_THAT (probe, WithinAbs ((float) expectedValue (shape, 0.5), 1.0e-5f));

        probeValues.push_back (probe);
    }

    // Mindestens zwei Modi unterscheiden sich messbar.
    bool distinct = false;

    for (size_t i = 0; i < probeValues.size() && ! distinct; ++i)
        for (size_t j = i + 1; j < probeValues.size(); ++j)
            if (std::abs (probeValues[i] - probeValues[j]) > 1.0e-4f)
                distinct = true;

    REQUIRE (distinct);

    // Konkret: Halten (0,2) gegen Spiegel-Rampe (1,0) gegen invertiert (0,6).
    REQUIRE_THAT (probeValues[0], WithinAbs (0.2f, 1.0e-5f));
    REQUIRE_THAT (probeValues[2], WithinAbs (1.0f, 1.0e-5f));
    REQUIRE_THAT (probeValues[7], WithinAbs (0.6f, 1.0e-5f));
    REQUIRE (std::abs (probeValues[2] - probeValues[0]) > 1.0e-4f);
}

// ---------------------------------------------------------------------------
// Trigger
// ---------------------------------------------------------------------------
TEST_CASE ("Kick Shaper: Trigger startet die Huellkurve bei Position 0", "[fx][kickshaper]")
{
    KickShaperModule module;
    configure (module, 1.0f, 1.0f, 0.0f, 1.0f, 100.0f);   // sLine 0 -> Umformung aktiv

    auto makeLevel = [] (float value, int numSamples)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample (ch, i, value);

        return buffer;
    };

    // Ein Block ueber der Schwelle (-40 dBFS) startet die Huellkurve bei 0.
    auto loud = makeLevel (0.5f, 64);
    module.process (loud);

    REQUIRE (module.isTriggerActive());
    REQUIRE (module.currentEnvelopePosition() > 0.0f);
    REQUIRE (module.currentEnvelopePosition() < 0.01f);
    REQUIRE (module.currentEnvelopeTarget() >= 0.0f);
    REQUIRE (module.currentEnvelopeTarget() <= 1.0f);

    // Stille re-armiert und parkt die Uhr wieder bei 0.
    auto silence = makeLevel (0.0f, 64);
    module.process (silence);

    REQUIRE (! module.isTriggerActive());
    REQUIRE (module.currentEnvelopePosition() == 0.0f);

    // Ein leiser Block unter der Schwelle (-60 dBFS) triggert nicht.
    auto quiet = makeLevel (0.001f, 64);
    module.process (quiet);

    REQUIRE (! module.isTriggerActive());
    REQUIRE (module.currentEnvelopePosition() == 0.0f);

    // Der naechste Block ueber der Schwelle startet erneut bei 0.
    loud = makeLevel (0.5f, 64);
    module.process (loud);

    REQUIRE (module.isTriggerActive());
    REQUIRE (module.currentEnvelopePosition() < 0.01f);
    REQUIRE (module.currentEnvelopePosition() > 0.0f);
}

TEST_CASE ("Kick Shaper: setRenderLength setzt die Laenge der Huellkurve", "[fx][kickshaper]")
{
    KickShaperModule module;
    configure (module, 1.0f, 1.0f, 0.0f, 1.0f, 100.0f);

    REQUIRE (module.renderLengthSeconds() == KickShaperModule::kDefaultRenderLengthSeconds);

    // 2400 Samples bei 48 kHz = 0,05 s = 5 % einer 1-s-Huellkurve.
    module.setRenderLength (1.0);
    REQUIRE (module.renderLengthSeconds() == 1.0);

    auto loud = makeConstant (2400, 0.5f);
    module.process (loud);

    REQUIRE_THAT (module.currentEnvelopePosition(), WithinAbs (0.05f, 1.0e-3f));

    // Klemmen auf den gueltigen Bereich; NaN wird verworfen.
    module.setRenderLength (0.0);
    REQUIRE (module.renderLengthSeconds() == KickShaperModule::kMinRenderLengthSeconds);

    module.setRenderLength (1.0e6);
    REQUIRE (module.renderLengthSeconds() == KickShaperModule::kMaxRenderLengthSeconds);

    module.setRenderLength (std::nan (""));
    REQUIRE (module.renderLengthSeconds() == KickShaperModule::kMaxRenderLengthSeconds);
}

// ---------------------------------------------------------------------------
// Neutralitaet, Stabilitaet, Grenzfaelle
// ---------------------------------------------------------------------------
TEST_CASE ("Kick Shaper: Default-Huellkurve ist neutral", "[fx][kickshaper]")
{
    // Ohne <Pt>-Kinder gilt die Werkskurve mit y = 1 - dann ist shaped == x
    // exakt, also bleibt auch bei amount = 100 und sLine = 0 alles unveraendert.
    const auto input  = makeSine (55.0, 0.25, 0.8f);
    const auto output = runModule (input, {}, 1.0f, 1.0f, 0.0f, 5.0f, 100.0f);

    REQUIRE (maxAbsDifference (output, input) == 0.0f);
}

TEST_CASE ("Kick Shaper: Stabilitaet, Stille, Mono und kurze Bloecke", "[fx][kickshaper]")
{
    // 100 Bloecke Rauschen mit allen Parametern am Anschlag bleiben endlich.
    KickShaperModule module;
    configure (module, 3.5f, 3.5f, 0.0f, 5.0f, 100.0f);

    juce::Random random (20261117);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    bool finite = true;

    for (int blockIndex = 0; blockIndex < 100; ++blockIndex)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                buffer.setSample (ch, i, 0.9f * (random.nextFloat() * 2.0f - 1.0f));

        module.process (buffer);
        finite = finite && allFinite (buffer);
    }

    REQUIRE (finite);
    REQUIRE (std::isfinite (module.currentEnvelopePosition()));
    REQUIRE (std::isfinite (module.currentEnvelopeTarget()));
    REQUIRE (std::isfinite (module.currentShapeAmount()));
    REQUIRE (std::isfinite (module.currentPreGain()));
    REQUIRE (std::isfinite (module.currentPostGain()));
    REQUIRE (module.currentEnvelopePosition() >= 0.0f);
    REQUIRE (module.currentEnvelopePosition() <= 1.0f);
    REQUIRE (module.currentEnvelopeTarget() >= 0.0f);
    REQUIRE (module.currentEnvelopeTarget() <= 1.0f);

    // Stille bleibt Stille (keine Denormals, kein Rauschen).
    juce::AudioBuffer<float> silence (2, 256);
    silence.clear();
    module.process (silence);

    REQUIRE (silence.getMagnitude (0, 256) == 0.0f);
    REQUIRE (! module.isTriggerActive());
    REQUIRE (module.currentEnvelopePosition() == 0.0f);

    // DC bleibt DC: das Modul hat keinen Filter im Signalpfad.
    KickShaperModule dcModule;
    configure (dcModule, 1.0f, 1.0f, 0.0f, 1.0f, 100.0f);

    auto dc = makeConstant (4096, 0.4f);
    dcModule.process (dc);

    REQUIRE_THAT (dc.getSample (0, 4095), WithinAbs (0.4f, 1.0e-6f));
    REQUIRE_THAT (dc.getSample (1, 4095), WithinAbs (0.4f, 1.0e-6f));

    // Mono-Buffer funktioniert.
    KickShaperModule monoModule;
    configure (monoModule, 1.0f, 1.0f, 0.0f, 3.0f, 100.0f);

    juce::AudioBuffer<float> mono (1, 512);

    for (int i = 0; i < 512; ++i)
        mono.setSample (0, i, 0.5f * (float) std::sin (0.02 * (double) i));

    monoModule.process (mono);
    REQUIRE (allFinite (mono));
    REQUIRE (mono.getMagnitude (0, 512) > 0.0f);

    // Sehr kurze und leere Bloecke: kein Absturz.
    juce::AudioBuffer<float> single (2, 1);
    single.setSample (0, 0, 0.5f);
    single.setSample (1, 0, 0.5f);
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

    juce::AudioBuffer<float> empty (2, 0);
    module.process (empty);
    REQUIRE (empty.getNumSamples() == 0);

    // Aufruf ohne prepare(): stabil und endlich.
    KickShaperModule unprepared;
    unprepared.setParam (KickShaperModule::idxAmount, 100.0f);
    unprepared.setParam (KickShaperModule::idxSLine, 0.0f);
    unprepared.setParam (KickShaperModule::idxSPre, 2.0f);

    auto kick = makeSine (55.0, 0.25, 0.9f);
    juce::AudioBuffer<float> unpreparedOut (2, kick.getNumSamples());
    unpreparedOut.makeCopyOf (kick);
    unprepared.process (unpreparedOut);

    REQUIRE (allFinite (unpreparedOut));
}

TEST_CASE ("Kick Shaper: Latenz ist 0", "[fx][kickshaper]")
{
    KickShaperModule module;

    REQUIRE (module.latencySamples() == 0);   // vor prepare()

    module.prepare (kSampleRate, kBlockSize);
    REQUIRE (module.latencySamples() == 0);

    module.prepare (kSampleRate, 64);
    REQUIRE (module.latencySamples() == 0);

    module.prepare (96000.0, 64);
    REQUIRE (module.latencySamples() == 0);

    REQUIRE (module.preparedSampleRateHz() == 96000.0);
    REQUIRE (module.blockSize() == 64);
}
