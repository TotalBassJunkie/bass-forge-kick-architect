#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "fx/modules/MidSideEqModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    constexpr double kPi         = 3.14159265358979323846;

    /** Ein Parameter-Override fuer `measureGainDb`. */
    using ParamOverride = std::pair<int, float>;

    std::unique_ptr<MidSideEqModule> makeModule()
    {
        auto module = std::make_unique<MidSideEqModule>();
        module->prepare (kSampleRate, kBlockSize);
        return module;
    }

    /** Baut einen <Module>-ValueTree mit <Band>-Kindern im Referenzformat. */
    juce::ValueTree makeState (const MidSideEqBand* bands, int numBands)
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

    /** Verarbeitet den Puffer blockweise in Bloecken von kBlockSize Samples. */
    void processInBlocks (MidSideEqModule& module, juce::AudioBuffer<float>& buffer)
    {
        const int numChannels = buffer.getNumChannels();
        const int total       = buffer.getNumSamples();

        for (int start = 0; start < total; start += kBlockSize)
        {
            const int numSamples = juce::jmin (kBlockSize, total - start);

            juce::AudioBuffer<float> block (numChannels, numSamples);

            for (int ch = 0; ch < numChannels; ++ch)
                block.copyFrom (ch, 0, buffer, ch, start, numSamples);

            module.process (block);

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.copyFrom (ch, start, block, ch, 0, numSamples);
        }
    }

    /** RMS eines Kanals in einem Sample-Bereich. */
    double rmsBetween (const juce::AudioBuffer<float>& buffer, int channel, int start, int end)
    {
        const int from = juce::jlimit (0, buffer.getNumSamples(), start);
        const int to   = juce::jlimit (from, buffer.getNumSamples(), end);

        double sum = 0.0;

        for (int i = from; i < to; ++i)
        {
            const double value = (double) buffer.getSample (channel, i);
            sum += value * value;
        }

        const int count = to - from;

        return (count > 0) ? std::sqrt (sum / (double) count) : 0.0;
    }

    /**
     * Sinus durch das Modul und Pegelgewinn auf dem linken Kanal in dB,
     * gemessen im eingeschwungenen Fenster (zweite Haelfte des Signals).
     *
     * `sideSignal == false` speist L = R (reines Mitten-Signal), sonst
     * L = -R (reines Seiten-Signal).
     */
    double measureGainDb (const MidSideEqBand* bands,
                          int numBands,
                          double freqHz,
                          bool sideSignal,
                          float amplitude = 0.4f,
                          double seconds = 0.5,
                          const std::vector<ParamOverride>& overrides = {})
    {
        MidSideEqModule module;

        if (bands != nullptr && numBands > 0)
            module.setState (makeState (bands, numBands));

        for (const auto& paramOverride : overrides)
            module.setParam (paramOverride.first, paramOverride.second);

        // prepare() uebernimmt Bandliste und Parameter ohne Einlauf-Rampe.
        module.prepare (kSampleRate, kBlockSize);

        const int numSamples = (int) std::lround (seconds * kSampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = amplitude
                             * (float) std::sin (2.0 * kPi * freqHz * (double) i / kSampleRate);

            buffer.setSample (0, i, value);
            buffer.setSample (1, i, sideSignal ? -value : value);
        }

        juce::AudioBuffer<float> input (2, numSamples);
        input.makeCopyOf (buffer);

        processInBlocks (module, buffer);

        const int from = numSamples / 2;

        const double inputRms  = rmsBetween (input, 0, from, numSamples);
        const double outputRms = rmsBetween (buffer, 0, from, numSamples);

        return 20.0 * std::log10 (juce::jmax (outputRms, 1.0e-12) / juce::jmax (inputRms, 1.0e-12));
    }

    /**
     * Setzt bis zu zwei Parameter (Index < 0 = keiner), gibt einen konstanten
     * Block (L = 1, R = 0) ein und liefert den eingeschwungenen Ausgang.
     */
    void renderSoloCase (int paramA, float valueA,
                         int paramB, float valueB,
                         float& outLeft, float& outRight)
    {
        MidSideEqModule module;

        if (paramA >= 0)
            module.setParam (paramA, valueA);

        if (paramB >= 0)
            module.setParam (paramB, valueB);

        module.prepare (kSampleRate, kBlockSize);

        juce::AudioBuffer<float> buffer (2, kBlockSize);

        for (int s = 0; s < kBlockSize; ++s)
        {
            buffer.setSample (0, s, 1.0f);
            buffer.setSample (1, s, 0.0f);
        }

        module.process (buffer);

        outLeft  = buffer.getSample (0, kBlockSize - 1);
        outRight = buffer.getSample (1, kBlockSize - 1);
    }
} // namespace

// =========================================================================
// Parametertabelle
// =========================================================================

TEST_CASE ("Mid/Side EQ: Parameterliste in Referenzreihenfolge", "[fx][mseq]")
{
    MidSideEqModule module;

    CHECK (module.type() == FxType::midSideEq);
    CHECK (module.displayName() == "Mid/Side EQ");
    CHECK (module.latencySamples() == 0);

    const auto& params = module.params();
    REQUIRE (params.size() == 11);

    const juce::StringArray expectedIds { "midlow", "midhigh", "sidelow", "sidehigh",
                                          "width", "middyn", "sidedyn",
                                          "msolo", "ssolo", "mmute", "smute" };

    REQUIRE (params[0].id == "midlow");
    REQUIRE (params[10].id == "smute");

    for (int i = 0; i < (int) expectedIds.size(); ++i)
        CHECK (params[(size_t) i].id == expectedIds[i]);

    // Die Bridge bildet spaeter p0...p10 auf genau diese Reihenfolge ab.
    REQUIRE (params[4].id == "width");
    REQUIRE (params[7].id == "msolo");
    REQUIRE (params[8].id == "ssolo");
    REQUIRE (params[9].id == "mmute");

    // Vier dB-Gains
    for (int i = 0; i < 4; ++i)
    {
        REQUIRE (params[(size_t) i].minValue == -18.0f);
        REQUIRE (params[(size_t) i].maxValue == 18.0f);
        REQUIRE (params[(size_t) i].defaultValue == 0.0f);
        REQUIRE (params[(size_t) i].unit == "dB");
    }

    // Width und die beiden Dynamik-Regler
    REQUIRE (params[4].minValue == 0.0f);
    REQUIRE (params[4].maxValue == 200.0f);
    REQUIRE (params[4].defaultValue == 100.0f);
    REQUIRE (params[4].unit == "%");

    for (int i = 5; i < 7; ++i)
    {
        REQUIRE (params[(size_t) i].minValue == 0.0f);
        REQUIRE (params[(size_t) i].maxValue == 100.0f);
        REQUIRE (params[(size_t) i].defaultValue == 0.0f);
        REQUIRE (params[(size_t) i].unit == "%");
    }

    // Vier Schalter
    for (int i = 7; i < 11; ++i)
    {
        REQUIRE (params[(size_t) i].minValue == 0.0f);
        REQUIRE (params[(size_t) i].maxValue == 1.0f);
        REQUIRE (params[(size_t) i].defaultValue == 0.0f);
        REQUIRE (params[(size_t) i].unit.isEmpty());
    }

    // Werkswerte im Modul
    REQUIRE (module.getParam (0) == 0.0f);
    REQUIRE (module.getParam (1) == 0.0f);
    REQUIRE (module.getParam (2) == 0.0f);
    REQUIRE (module.getParam (3) == 0.0f);
    REQUIRE (module.getParam (4) == 100.0f);
    REQUIRE (module.getParam (5) == 0.0f);
    REQUIRE (module.getParam (6) == 0.0f);
    REQUIRE (module.getParam (7) == 0.0f);
    REQUIRE (module.getParam (8) == 0.0f);
    REQUIRE (module.getParam (9) == 0.0f);
    REQUIRE (module.getParam (10) == 0.0f);
}

TEST_CASE ("Mid/Side EQ: Parameter klemmen auf den deklarierten Bereich", "[fx][mseq]")
{
    MidSideEqModule module;

    module.setParam (0, 99.0f);
    REQUIRE (module.getParam (0) == 18.0f);

    module.setParam (0, -99.0f);
    REQUIRE (module.getParam (0) == -18.0f);

    // width: 0...200 %
    module.setParam (4, -5.0f);
    REQUIRE (module.getParam (4) == 0.0f);

    module.setParam (4, 999.0f);
    REQUIRE (module.getParam (4) == 200.0f);

    // middyn/sidedyn: 0...100 %
    module.setParam (5, 250.0f);
    REQUIRE (module.getParam (5) == 100.0f);

    module.setParam (6, -1.0f);
    REQUIRE (module.getParam (6) == 0.0f);

    // Schalter: 0/1
    module.setParam (9, 5.0f);
    REQUIRE (module.getParam (9) == 1.0f);

    // Ungueltige Indizes bleiben folgenlos.
    module.setParam (11, 1.0f);
    module.setParam (-1, 1.0f);
    REQUIRE (module.getParam (11) == 0.0f);
    REQUIRE (module.getParam (-1) == 0.0f);
}

// =========================================================================
// Neutralzustand
// =========================================================================

TEST_CASE ("Mid/Side EQ: Werkszustand reicht bitidentisch durch", "[fx][mseq]")
{
    auto module = makeModule();

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::AudioBuffer<float> reference (2, kBlockSize);

    // Sinusplus Impuls, 80 Bloecke (213 ms) laufen lassen.
    double phase = 0.0;
    const double increment = 2.0 * kPi * 997.0 / kSampleRate;

    float maxDifference = 0.0f;

    for (int block = 0; block < 80; ++block)
    {
        for (int s = 0; s < kBlockSize; ++s)
        {
            const auto value = (float) (0.5 * std::sin (phase));
            phase += increment;

            buffer.setSample (0, s, value);
            buffer.setSample (1, s, -0.3f * value);
        }

        if (block == 0)
            buffer.setSample (0, 0, 0.9f);   // Impuls

        reference.makeCopyOf (buffer);
        module->process (buffer);

        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < kBlockSize; ++s)
                maxDifference = juce::jmax (maxDifference,
                                            std::abs (buffer.getSample (ch, s)
                                                      - reference.getSample (ch, s)));
    }

    REQUIRE (maxDifference < 1.0e-7f);
    CHECK (module->activeStageCount() == 0);
}

// =========================================================================
// M/S-Matrix
// =========================================================================

TEST_CASE ("Mid/Side EQ: width 100 laesst ein Links-Signal unveraendert", "[fx][mseq]")
{
    // Reines Links-Signal: M = L/2, S = L/2. Mit width = 100 % (w = 1) ist die
    // Rueckmatrix M + S = L (Halbieren und Addieren sind in Gleitkomma exakt).
    auto module = makeModule();

    juce::AudioBuffer<float> buffer (2, kBlockSize);

    double phase = 0.0;
    const double increment = 2.0 * kPi * 440.0 / kSampleRate;

    for (int s = 0; s < kBlockSize; ++s)
    {
        const auto value = (float) (0.8 * std::sin (phase));
        phase += increment;

        buffer.setSample (0, s, value);
        buffer.setSample (1, s, 0.0f);
    }

    juce::AudioBuffer<float> reference (buffer);
    module->process (buffer);

    for (int s = 0; s < kBlockSize; ++s)
    {
        REQUIRE_THAT (buffer.getSample (0, s), WithinAbs (reference.getSample (0, s), 1.0e-7f));
        REQUIRE_THAT (buffer.getSample (1, s), WithinAbs (reference.getSample (1, s), 1.0e-7f));
    }

    // Matrix wirklich aktiv (width = 200 %): L = 1,5·L, R = -0,5·L.
    auto wide = makeModule();
    wide->setParam (MidSideEqModule::pWidth, 200.0f);
    wide->prepare (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> wideBuffer (2, kBlockSize);
    wideBuffer.makeCopyOf (reference);

    wide->process (wideBuffer);

    for (int s = 0; s < kBlockSize; ++s)
    {
        const auto input = reference.getSample (0, s);

        REQUIRE_THAT (wideBuffer.getSample (0, s), WithinAbs (1.5f * input, 1.0e-6f));
        REQUIRE_THAT (wideBuffer.getSample (1, s), WithinAbs (-0.5f * input, 1.0e-6f));
    }
}

TEST_CASE ("Mid/Side EQ: Mid-Solo ergibt die Monosumme, Side-Solo den Seitenanteil", "[fx][mseq]")
{
    // Eingang L = 1, R = 0  =>  M = 0,5, S = 0,5
    float left  = 0.0f;
    float right = 0.0f;

    renderSoloCase (MidSideEqModule::pMidSolo, 1.0f, -1, 0.0f, left, right);
    REQUIRE_THAT (left,  WithinAbs (0.5f, 1.0e-6f));
    REQUIRE_THAT (right, WithinAbs (0.5f, 1.0e-6f));

    renderSoloCase (MidSideEqModule::pSideSolo, 1.0f, -1, 0.0f, left, right);
    REQUIRE_THAT (left,  WithinAbs (0.5f, 1.0e-6f));
    REQUIRE_THAT (right, WithinAbs (-0.5f, 1.0e-6f));
}

TEST_CASE ("Mid/Side EQ: Solo- und Mute-Matrix je Seite", "[fx][mseq]")
{
    // Eingang L = 1, R = 0  =>  M = 0,5, S = 0,5
    float left  = 0.0f;
    float right = 0.0f;

    // Mid stumm -> nur Side
    renderSoloCase (MidSideEqModule::pMidMute, 1.0f, -1, 0.0f, left, right);
    REQUIRE_THAT (left,  WithinAbs (0.5f, 1.0e-6f));
    REQUIRE_THAT (right, WithinAbs (-0.5f, 1.0e-6f));

    // Side stumm -> Mono (nur Mid)
    renderSoloCase (MidSideEqModule::pSideMute, 1.0f, -1, 0.0f, left, right);
    REQUIRE_THAT (left,  WithinAbs (0.5f, 1.0e-6f));
    REQUIRE_THAT (right, WithinAbs (0.5f, 1.0e-6f));

    // Mid-Solo und Mid-Mute zusammen -> beide Seiten 0
    renderSoloCase (MidSideEqModule::pMidSolo, 1.0f, MidSideEqModule::pMidMute, 1.0f, left, right);
    REQUIRE_THAT (left,  WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_THAT (right, WithinAbs (0.0f, 1.0e-6f));

    // Side-Solo und Side-Mute zusammen -> beide Seiten 0
    renderSoloCase (MidSideEqModule::pSideSolo, 1.0f, MidSideEqModule::pSideMute, 1.0f, left, right);
    REQUIRE_THAT (left,  WithinAbs (0.0f, 1.0e-6f));
    REQUIRE_THAT (right, WithinAbs (0.0f, 1.0e-6f));
}

// =========================================================================
// Bandseiten
// =========================================================================

TEST_CASE ("Mid/Side EQ: Band-Boost wirkt nur auf der eingestellten Seite", "[fx][mseq]")
{
    const MidSideEqBand sideBoost[] = { { 0, 1000.0, 0.9, 6.0, 0.0, 1 } };
    const MidSideEqBand midBoost[]  = { { 0, 1000.0, 0.9, 6.0, 0.0, 0 } };

    // Mitten-Signal (L = R): ein Seiten-Band darf den Mittenanteil nicht anfassen.
    REQUIRE_THAT (measureGainDb (sideBoost, 1, 1000.0, false), WithinAbs (0.0, 0.05));
    REQUIRE_THAT (measureGainDb (midBoost,  1, 1000.0, false), WithinAbs (6.0, 1.5));

    // Seiten-Signal (L = -R): umgekehrt.
    REQUIRE_THAT (measureGainDb (midBoost,  1, 1000.0, true), WithinAbs (0.0, 0.05));
    REQUIRE_THAT (measureGainDb (sideBoost, 1, 1000.0, true), WithinAbs (6.0, 1.5));
}

TEST_CASE ("Mid/Side EQ: Mid- und Side-Low-Shelf wirken auf ihrer Seite", "[fx][mseq]")
{
    // Low-Shelf 90 Hz (Mid) bzw. 120 Hz (Side), Q = 0,7071. Bei 40 Hz liegt die
    // Verstaerkung zwischen der halben (Eckfrequenz) und der vollen Anhebung.
    const double midLow  = measureGainDb (nullptr, 0, 40.0, false, 0.4f, 0.6,
                                          { { (int) MidSideEqModule::pMidLow, 12.0f } });
    const double sideLow = measureGainDb (nullptr, 0, 40.0, true, 0.4f, 0.6,
                                          { { (int) MidSideEqModule::pSideLow, 12.0f } });

    REQUIRE (midLow  > 7.0);
    REQUIRE (midLow  < 12.5);
    REQUIRE (sideLow > 7.0);
    REQUIRE (sideLow < 12.5);

    // Die jeweils andere Seite bleibt unberuehrt.
    REQUIRE_THAT (measureGainDb (nullptr, 0, 40.0, true, 0.4f, 0.6,
                                 { { (int) MidSideEqModule::pMidLow, 12.0f } }),
                  WithinAbs (0.0, 0.05));

    REQUIRE_THAT (measureGainDb (nullptr, 0, 40.0, false, 0.4f, 0.6,
                                 { { (int) MidSideEqModule::pSideLow, 12.0f } }),
                  WithinAbs (0.0, 0.05));
}

// =========================================================================
// Dynamische Gegenregelung
// =========================================================================

TEST_CASE ("Mid/Side EQ: middyn regelt die Bandverstaerkung zurueck", "[fx][mseq]")
{
    const MidSideEqBand boost[] = { { 0, 1000.0, 0.9, 6.0, 0.0, 0 } };

    // Lautes Mitten-Signal (0,7 = ueber der Schwelle von -6 dBFS).
    const double statisch  = measureGainDb (boost, 1, 1000.0, false, 0.7f, 0.6);
    const double dynamisch = measureGainDb (boost, 1, 1000.0, false, 0.7f, 0.6,
                                            { { (int) MidSideEqModule::pMidDyn, 100.0f } });

    REQUIRE_THAT (statisch, WithinAbs (6.0, 1.5));

    // Nach dem Einschwingen ist die Anhebung messbar zurueckgegangen ...
    CHECK (dynamisch < statisch - 2.0);
    // ... und liegt nahe der 0-dB-Grenze (max. 6 dB Gegenregelung).
    CHECK (dynamisch < 3.0);
    CHECK (dynamisch > -1.0);

    // sidedyn laesst das Mitten-Band unberuehrt (eigene Seite).
    REQUIRE_THAT (measureGainDb (boost, 1, 1000.0, false, 0.7f, 0.6,
                                 { { (int) MidSideEqModule::pSideDyn, 100.0f } }),
                  WithinAbs (statisch, 0.1));
}

// =========================================================================
// Zustandsformat
// =========================================================================

TEST_CASE ("Mid/Side EQ: getState/setState Roundtrip mit drei Baendern", "[fx][mseq]")
{
    const MidSideEqBand bands[] =
    {
        { 1,    71.64, 0.676, 15.70,  0.0, 0 },
        { 0,   440.0,  1.2,   -3.5,  50.0, 1 },
        { 4,  9000.0,  0.7,    0.0,   0.0, 1 }
    };

    auto module = makeModule();
    module->setState (makeState (bands, 3));

    const auto state = module->getState();
    REQUIRE (state.getType() == juce::Identifier ("Module"));
    REQUIRE (state.getNumChildren() == 3);

    for (int i = 0; i < 3; ++i)
    {
        const auto node = state.getChild (i);

        REQUIRE (node.hasType (juce::Identifier ("Band")));

        CHECK ((int) node.getProperty (juce::Identifier ("kind")) == bands[i].kind);
        REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("f")),
                      WithinAbs (bands[i].freqHz, 1.0e-6));
        REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("q")),
                      WithinAbs (bands[i].q, 1.0e-6));
        REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("g")),
                      WithinAbs (bands[i].gainDb, 1.0e-6));
        REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("dyn")),
                      WithinAbs (bands[i].dyn, 1.0e-6));
        CHECK ((int) node.getProperty (juce::Identifier ("ch")) == bands[i].channel);
    }
}

TEST_CASE ("Mid/Side EQ: setState begrenzt auf 12 Baender", "[fx][mseq]")
{
    MidSideEqBand many[20];

    for (int i = 0; i < 20; ++i)
    {
        many[i].kind    = 0;
        many[i].freqHz  = 100.0 + 100.0 * (double) i;
        many[i].q       = 1.0;
        many[i].gainDb  = 3.0;
        many[i].channel = i % 2;
    }

    auto module = makeModule();
    module->setState (makeState (many, 20));

    const auto state = module->getState();
    REQUIRE (state.getNumChildren() == MidSideEqModule::maxBands);

    REQUIRE_THAT ((double) state.getChild (0).getProperty (juce::Identifier ("f")),
                  WithinAbs (100.0, 1.0e-9));
    REQUIRE_THAT ((double) state.getChild (11).getProperty (juce::Identifier ("f")),
                  WithinAbs (1200.0, 1.0e-9));
}

TEST_CASE ("Mid/Side EQ: unbekannte Kinder und Attribute werden ignoriert", "[fx][mseq]")
{
    juce::ValueTree root (juce::Identifier ("Module"));

    juce::ValueTree fremd (juce::Identifier ("WNode"));
    fremd.setProperty (juce::Identifier ("x"), juce::var (1.0), nullptr);
    root.appendChild (fremd, nullptr);

    juce::ValueTree band (juce::Identifier ("Band"));
    band.setProperty (juce::Identifier ("kind"), juce::var (0), nullptr);
    band.setProperty (juce::Identifier ("f"),    juce::var (250.0), nullptr);
    band.setProperty (juce::Identifier ("q"),    juce::var (1.5), nullptr);
    band.setProperty (juce::Identifier ("g"),    juce::var ("-6.5"), nullptr);  // Zahl als String
    band.setProperty (juce::Identifier ("dyn"),  juce::var (25.0), nullptr);
    band.setProperty (juce::Identifier ("ch"),   juce::var (1), nullptr);
    band.setProperty (juce::Identifier ("colour"), juce::var ("rot"), nullptr);
    root.appendChild (band, nullptr);

    auto module = makeModule();
    module->setState (root);

    const auto state = module->getState();
    REQUIRE (state.getNumChildren() == 1);

    const auto node = state.getChild (0);
    REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("f")), WithinAbs (250.0, 1.0e-9));
    REQUIRE_THAT ((double) node.getProperty (juce::Identifier ("g")), WithinAbs (-6.5, 1.0e-9));
    CHECK ((int) node.getProperty (juce::Identifier ("ch")) == 1);
    CHECK (node.getProperty (juce::Identifier ("colour")).isVoid());
}
