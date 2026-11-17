#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include <cmath>
#include <memory>

#include "fx/modules/EqualizerModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr double kPi         = 3.14159265358979323846;

    std::unique_ptr<EqualizerModule> makeModule()
    {
        auto module = std::make_unique<EqualizerModule>();
        module->prepare (kSampleRate, kBlockSize);
        return module;
    }

    /** Baut einen <Module>-ValueTree mit <Band>-Kindern im Referenzformat. */
    juce::ValueTree makeState (const EqualizerBand* bands, int numBands)
    {
        juce::ValueTree root (juce::Identifier ("Module"));

        for (int i = 0; i < numBands; ++i)
        {
            const auto& band = bands[i];

            juce::ValueTree node (juce::Identifier ("Band"));
            node.setProperty (juce::Identifier ("kind"), juce::var (band.kind), nullptr);
            node.setProperty (juce::Identifier ("f"),    juce::var (band.freqHz), nullptr);
            node.setProperty (juce::Identifier ("q"),    juce::var (band.q), nullptr);
            node.setProperty (juce::Identifier ("g"),    juce::var (band.gainDb), nullptr);
            node.setProperty (juce::Identifier ("dyn"),  juce::var (band.dyn), nullptr);
            node.setProperty (juce::Identifier ("ch"),   juce::var (band.channel), nullptr);

            root.appendChild (node, nullptr);
        }

        return root;
    }

    /**
     * Misst den Amplitudengang bei `freqHz` als Sinus-Einschwingmessung:
     * erst einschwingen lassen, dann den RMS des eingeschwungenen Zustands
     * gegen den analytischen RMS des Eingangs (Amplitude 0,5) vergleichen.
     */
    double measureGainDb (EqualizerModule& module, double freqHz)
    {
        constexpr double amplitude     = 0.5;
        constexpr double settleSeconds = 0.30;
        constexpr double measureSeconds = 0.32;

        const int settleBlocks  = juce::jmax (1, (int) std::lround (settleSeconds * kSampleRate / (double) kBlockSize));
        const int measureBlocks = juce::jmax (1, (int) std::lround (measureSeconds * kSampleRate / (double) kBlockSize));

        juce::AudioBuffer<float> buffer (2, kBlockSize);

        const double increment = 2.0 * kPi * freqHz / kSampleRate;
        double phase = 0.0;
        double sumOfSquares = 0.0;

        long long measuredSamples = 0;

        for (int block = 0; block < settleBlocks + measureBlocks; ++block)
        {
            for (int s = 0; s < kBlockSize; ++s)
            {
                const auto value = (float) (amplitude * std::sin (phase));
                phase += increment;

                buffer.setSample (0, s, value);
                buffer.setSample (1, s, value);
            }

            module.process (buffer);

            if (block >= settleBlocks)
            {
                for (int s = 0; s < kBlockSize; ++s)
                {
                    const double y = (double) buffer.getSample (0, s);
                    sumOfSquares += y * y;
                    ++measuredSamples;
                }
            }
        }

        const double inputRms  = amplitude / std::sqrt (2.0);
        const double outputRms = std::sqrt (sumOfSquares / (double) measuredSamples);

        return 20.0 * std::log10 (juce::jmax (outputRms, 1.0e-12) / inputRms);
    }
} // namespace

// =========================================================================
// Kopfdaten
// =========================================================================

TEST_CASE ("Equalizer: Parameterliste in Referenzreihenfolge", "[fx][equalizer]")
{
    EqualizerModule eq;

    CHECK (eq.type() == FxType::equalizer);
    CHECK (eq.displayName() == "Equalizer");
    CHECK (eq.latencySamples() == 0);

    const auto& params = eq.params();
    REQUIRE (params.size() == 8);

    const juce::StringArray expectedIds { "lowgain", "lowfreq", "midgain", "midfreq",
                                          "midq", "highgain", "highfreq", "middyn" };

    for (int i = 0; i < (int) expectedIds.size(); ++i)
        CHECK (params[(size_t) i].id == expectedIds[i]);

    // Werkswerte
    REQUIRE_THAT (eq.getParam (0), WithinAbs (0.0f, 1.0e-6f));      // lowgain
    REQUIRE_THAT (eq.getParam (1), WithinAbs (90.0f, 1.0e-6f));     // lowfreq
    REQUIRE_THAT (eq.getParam (2), WithinAbs (0.0f, 1.0e-6f));      // midgain
    REQUIRE_THAT (eq.getParam (3), WithinAbs (900.0f, 1.0e-6f));    // midfreq
    REQUIRE_THAT (eq.getParam (4), WithinAbs (0.9f, 1.0e-6f));      // midq
    REQUIRE_THAT (eq.getParam (5), WithinAbs (0.0f, 1.0e-6f));      // highgain
    REQUIRE_THAT (eq.getParam (6), WithinAbs (8000.0f, 1.0e-6f));   // highfreq
    REQUIRE_THAT (eq.getParam (7), WithinAbs (0.0f, 1.0e-6f));      // middyn

    // Bereiche und Klemmung
    REQUIRE_THAT (params[0].minValue, WithinAbs (-18.0f, 1.0e-6f));
    REQUIRE_THAT (params[0].maxValue, WithinAbs (18.0f, 1.0e-6f));
    REQUIRE_THAT (params[4].minValue, WithinAbs (0.1f, 1.0e-6f));
    REQUIRE_THAT (params[7].maxValue, WithinAbs (100.0f, 1.0e-6f));

    eq.setParam (0, 1000.0f);
    REQUIRE_THAT (eq.getParam (0), WithinAbs (18.0f, 1.0e-6f));

    eq.setParam (4, 0.0f);
    REQUIRE_THAT (eq.getParam (4), WithinAbs (0.1f, 1.0e-6f));

    eq.setParam (99, 5.0f);   // ungueltiger Index bleibt folgenlos
    REQUIRE_THAT (eq.getParam (99), WithinAbs (0.0f, 1.0e-6f));
}

// =========================================================================
// Bandtypen
// =========================================================================

TEST_CASE ("Equalizer: BELL 1 kHz +12 dB, 100 Hz unveraendert", "[fx][equalizer]")
{
    const EqualizerBand bands[] = { { 0, 1000.0, 0.9, 12.0, 0.0, 0 } };

    auto atBell = makeModule();
    atBell->setState (makeState (bands, 1));
    REQUIRE_THAT (measureGainDb (*atBell, 1000.0), WithinAbs (12.0, 1.5));

    auto atLow = makeModule();
    atLow->setState (makeState (bands, 1));
    REQUIRE_THAT (measureGainDb (*atLow, 100.0), WithinAbs (0.0, 0.5));
}

TEST_CASE ("Equalizer: H-SHF +12 dB ueber der Eckfrequenz", "[fx][equalizer]")
{
    // Eckfrequenz 1 kHz: 10 kHz liegt weit im Shelf-Plateau, 100 Hz weit darunter.
    const EqualizerBand bands[] = { { 2, 1000.0, 0.9, 12.0, 0.0, 0 } };

    auto atHigh = makeModule();
    atHigh->setState (makeState (bands, 1));
    REQUIRE_THAT (measureGainDb (*atHigh, 10000.0), WithinAbs (12.0, 1.5));

    auto atLow = makeModule();
    atLow->setState (makeState (bands, 1));
    REQUIRE_THAT (measureGainDb (*atLow, 100.0), WithinAbs (0.0, 0.5));
}

TEST_CASE ("Equalizer: L-CUT 500 Hz nimmt die Tiefen weg", "[fx][equalizer]")
{
    const EqualizerBand bands[] = { { 3, 500.0, 0.9, 0.0, 0.0, 0 } };

    auto eq = makeModule();
    eq->setState (makeState (bands, 1));

    const double gain100  = measureGainDb (*eq, 100.0);
    const double gain5000 = measureGainDb (*eq, 5000.0);

    CHECK (gain100 < gain5000 - 15.0);          // deutlich leiser
    REQUIRE_THAT (gain5000, WithinAbs (0.0, 1.0)); // Durchlass unveraendert
}

TEST_CASE ("Equalizer: NOTCH 1 kHz senkt mindestens 30 dB", "[fx][equalizer]")
{
    const EqualizerBand bands[] = { { 5, 1000.0, 0.9, 0.0, 0.0, 0 } };

    auto eq = makeModule();
    eq->setState (makeState (bands, 1));

    const double gain100  = measureGainDb (*eq, 100.0);
    const double gain1000 = measureGainDb (*eq, 1000.0);

    CHECK (gain1000 <= gain100 - 30.0);
}

// =========================================================================
// Grenzfaelle
// =========================================================================

TEST_CASE ("Equalizer: 0 Baender ist ein Durchreichen", "[fx][equalizer]")
{
    auto eq = makeModule();

    juce::AudioBuffer<float> input (2, kBlockSize);
    juce::Random random (4711);

    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < kBlockSize; ++s)
            input.setSample (ch, s, random.nextFloat() * 2.0f - 1.0f);

    juce::AudioBuffer<float> output (input);
    eq->process (output);

    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < kBlockSize; ++s)
            REQUIRE_THAT (output.getSample (ch, s), WithinAbs (input.getSample (ch, s), 1.0e-6f));

    CHECK (eq->getState().getNumChildren() == 0);
    // Die klassischen Regler sind Teil der Kaskade, stehen aber auf 0 dB.
    CHECK (eq->activeBandCount() == EqualizerModule::numClassicBands);
}

TEST_CASE ("Equalizer: 12 Baender gleichzeitig bleiben endlich", "[fx][equalizer]")
{
    const EqualizerBand bands[] =
    {
        { 0,    40.0,  1.2,  32.0,   0.0, 0 },
        { 0,    70.0,  2.0,  32.0,   0.0, 0 },
        { 1,   120.0,  0.9,  32.0,   0.0, 0 },
        { 0,   250.0,  4.0,  32.0,   0.0, 0 },
        { 2,   400.0,  0.7,  32.0,   0.0, 0 },
        { 0,   800.0, 10.0,  32.0,   0.0, 0 },
        { 3,  1200.0,  0.7,  32.0,   0.0, 0 },   // L-CUT, g wird ignoriert
        { 4,  2000.0,  0.7,  32.0,   0.0, 0 },   // H-CUT, g wird ignoriert
        { 0,  3200.0,  0.5, -32.0,   0.0, 0 },
        { 5,  5000.0, 10.0,   0.0,   0.0, 0 },   // NOTCH
        { 0,  9000.0,  0.1,  32.0, 100.0, 0 },   // dynamisches Band
        { 0, 19000.0,  0.9,  32.0,   0.0, 0 }
    };

    auto eq = makeModule();
    eq->setState (makeState (bands, 12));

    // Ein Block Uebernahme: die Bandliste wird lock-frei eingelesen.
    juce::AudioBuffer<float> warmUp (2, kBlockSize);
    warmUp.clear();
    eq->process (warmUp);
    CHECK (eq->activeBandCount() == EqualizerModule::maxChainBands);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    const double increment = 2.0 * kPi * 60.0 / kSampleRate;
    double phase = 0.0;
    double peak  = 0.0;
    bool   allFinite = true;

    for (int block = 0; block < 400; ++block)   // ~1,07 s
    {
        for (int s = 0; s < kBlockSize; ++s)
        {
            const auto value = (float) (0.01 * std::sin (phase));
            phase += increment;

            buffer.setSample (0, s, value);
            buffer.setSample (1, s, value);
        }

        eq->process (buffer);

        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < kBlockSize; ++s)
            {
                const auto y = buffer.getSample (ch, s);

                if (! std::isfinite (y))
                    allFinite = false;

                peak = juce::jmax (peak, (double) std::abs (y));
            }
    }

    REQUIRE (allFinite);
    CHECK (peak < 1.0e4);     // keine Aufschaukelung
}

TEST_CASE ("Equalizer: Frequenzen an den Raendern bleiben stabil", "[fx][equalizer]")
{
    const EqualizerBand bands[] =
    {
        { 0,      0.5,   0.0,  32.0, 0.0, 0 },   // unter dem erlaubten Bereich
        { 2,  96000.0, 100.0, -32.0, 0.0, 0 },   // ueber Nyquist/2
        { 3,      0.1,   0.0,   0.0, 0.0, 0 },
        { 4, 999999.0, 100.0,   0.0, 0.0, 0 }
    };

    auto eq = makeModule();
    eq->setState (makeState (bands, 4));

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);

    bool allFinite = true;
    double peak = 0.0;

    for (int block = 0; block < 200; ++block)
    {
        eq->process (buffer);

        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < kBlockSize; ++s)
            {
                const auto y = buffer.getSample (ch, s);

                if (! std::isfinite (y))
                    allFinite = false;

                peak = juce::jmax (peak, (double) std::abs (y));
            }

        buffer.clear();
    }

    REQUIRE (allFinite);
    CHECK (peak < 1.0e4);
}

// =========================================================================
// Dynamische Gegenregelung
// =========================================================================

TEST_CASE ("Equalizer: dyn regelt maximal 6 dB gegen", "[fx][equalizer]")
{
    const EqualizerBand staticBands[] = { { 0, 1000.0, 0.9, 12.0, 0.0, 0 } };
    const EqualizerBand dynBands[]    = { { 0, 1000.0, 0.9, 12.0, 100.0, 0 } };

    auto ohne = makeModule();
    ohne->setState (makeState (staticBands, 1));
    const double statisch = measureGainDb (*ohne, 1000.0);
    REQUIRE_THAT (statisch, WithinAbs (12.0, 1.5));

    auto mit = makeModule();
    mit->setState (makeState (dynBands, 1));
    const double dynamisch = measureGainDb (*mit, 1000.0);

    CHECK (dynamisch < statisch - 4.0);   // Gegenregelung wirkt
    CHECK (dynamisch > statisch - 8.0);   // aber hoechstens 6 dB
}

// =========================================================================
// Klassische Regler (Baender 0…2 der Kette)
// =========================================================================

TEST_CASE ("Equalizer: klassische Regler wirken als BELL und H-SHF", "[fx][equalizer]")
{
    auto low = makeModule();
    low->setParam (0, 12.0f);                       // lowgain
    REQUIRE_THAT (measureGainDb (*low, 90.0), WithinAbs (12.0, 1.5));

    auto high = makeModule();
    high->setParam (5, 12.0f);                      // highgain
    REQUIRE_THAT (measureGainDb (*high, 16000.0), WithinAbs (12.0, 1.5));

    auto mid = makeModule();
    mid->setParam (2, -12.0f);                      // midgain
    REQUIRE_THAT (measureGainDb (*mid, 900.0), WithinAbs (-12.0, 1.5));
}

// =========================================================================
// Zustandsformat
// =========================================================================

TEST_CASE ("Equalizer: getState/setState Round-Trip mit drei Baendern", "[fx][equalizer]")
{
    const EqualizerBand bands[] =
    {
        { 1,    71.64, 0.676, 15.70,  0.0, 0 },
        { 0,   440.0,  1.2,   -3.5,  50.0, 0 },
        { 4,  9000.0,  0.7,    0.0,   0.0, 1 }
    };

    auto eq = makeModule();
    eq->setState (makeState (bands, 3));

    const auto state = eq->getState();
    REQUIRE (state.getType() == juce::Identifier ("Module"));
    REQUIRE (state.getNumChildren() == 3);

    for (int i = 0; i < 3; ++i)
    {
        const auto node = state.getChild (i);

        REQUIRE (node.hasType (juce::Identifier ("Band")));

        CHECK ((int) node.getProperty (juce::Identifier ("kind")) == bands[i].kind);
        REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("f")), WithinAbs (bands[i].freqHz, 1.0e-9));
        REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("q")), WithinAbs (bands[i].q, 1.0e-9));
        REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("g")), WithinAbs (bands[i].gainDb, 1.0e-9));
        REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("dyn")), WithinAbs (bands[i].dyn, 1.0e-9));
        CHECK ((int) node.getProperty (juce::Identifier ("ch")) == bands[i].channel);
    }
}

TEST_CASE ("Equalizer: setState begrenzt auf 12 Baender", "[fx][equalizer]")
{
    EqualizerBand many[20];

    for (int i = 0; i < 20; ++i)
    {
        many[i].kind   = 0;
        many[i].freqHz = 100.0 + 100.0 * (double) i;
        many[i].q      = 1.0;
        many[i].gainDb = 3.0;
    }

    auto eq = makeModule();
    eq->setState (makeState (many, 20));

    const auto state = eq->getState();
    REQUIRE (state.getNumChildren() == EqualizerModule::maxBands);

    REQUIRE_THAT ((double) state.getChild (0).getProperty (juce::Identifier ("f")), WithinAbs (100.0, 1.0e-9));
    REQUIRE_THAT ((double) state.getChild (11).getProperty (juce::Identifier ("f")), WithinAbs (1200.0, 1.0e-9));
}

TEST_CASE ("Equalizer: setState ignoriert fremde Kinder und unbekannte Attribute", "[fx][equalizer]")
{
    juce::ValueTree root (juce::Identifier ("Module"));

    juce::ValueTree fremd (juce::Identifier ("WNode"));
    fremd.setProperty (juce::Identifier ("x"), juce::var (1.0), nullptr);
    root.appendChild (fremd, nullptr);

    juce::ValueTree band (juce::Identifier ("Band"));
    band.setProperty (juce::Identifier ("kind"), juce::var (0), nullptr);
    band.setProperty (juce::Identifier ("f"),    juce::var (250.0), nullptr);
    band.setProperty (juce::Identifier ("q"),    juce::var (1.5), nullptr);
    band.setProperty (juce::Identifier ("g"),    juce::var ("-6.5"), nullptr); // Zahlen als String (aus XML)
    band.setProperty (juce::Identifier ("dyn"),  juce::var (25.0), nullptr);
    band.setProperty (juce::Identifier ("ch"),   juce::var (0), nullptr);
    band.setProperty (juce::Identifier ("colour"), juce::var ("rot"), nullptr);
    root.appendChild (band, nullptr);

    auto eq = makeModule();
    eq->setState (root);

    const auto state = eq->getState();
    REQUIRE (state.getNumChildren() == 1);

    const auto node = state.getChild (0);
    REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("f")), WithinAbs (250.0, 1.0e-9));
    REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("g")), WithinAbs (-6.5, 1.0e-9));
    CHECK (node.getProperty (juce::Identifier ("colour")).isVoid());
}

TEST_CASE ("Equalizer: leeres setState loescht die Baender", "[fx][equalizer]")
{
    const EqualizerBand bands[] = { { 0, 500.0, 1.0, 6.0, 0.0, 0 },
                                    { 5, 2000.0, 2.0, 0.0, 0.0, 0 } };

    auto eq = makeModule();
    eq->setState (makeState (bands, 2));
    REQUIRE (eq->getState().getNumChildren() == 2);

    eq->setState (juce::ValueTree (juce::Identifier ("Module")));
    CHECK (eq->getState().getNumChildren() == 0);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    eq->process (buffer);

    REQUIRE_THAT (buffer.getSample (0, 0), WithinAbs (1.0f, 1.0e-6f));
}
