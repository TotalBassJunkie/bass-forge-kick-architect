#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>

#include <utility>

#include "core/KickState.h"
#include "fx/modules/ConvolverModule.h"
#include "fx/modules/EqualizerModule.h"
#include "plugin/PluginProcessor.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Die Tests bauen echte PluginProcessor-Instanzen (APVTS-Timer).
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    FxModuleState makeModule (const juce::String& type, const juce::String& ir = {})
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = true;
        state.mix     = 1.0f;
        state.outGain = 1.0f;
        state.data    = juce::ValueTree (juce::Identifier ("Module"));

        if (ir.isNotEmpty())
            state.data.setProperty (juce::Identifier ("ir"), ir, nullptr);

        return state;
    }

    void setAttribute (FxModuleState& state, const juce::String& id, double value)
    {
        state.data.setProperty (juce::Identifier (id), juce::var (value), nullptr);
    }

    juce::ValueTree makeBand (int kind, double freq, double q, double gain, double dyn, int ch)
    {
        juce::ValueTree band (juce::Identifier ("Band"));
        band.setProperty (juce::Identifier ("kind"), kind,  nullptr);
        band.setProperty (juce::Identifier ("f"),    freq,  nullptr);
        band.setProperty (juce::Identifier ("q"),    q,     nullptr);
        band.setProperty (juce::Identifier ("g"),    gain,  nullptr);
        band.setProperty (juce::Identifier ("dyn"),  dyn,   nullptr);
        band.setProperty (juce::Identifier ("ch"),   ch,    nullptr);
        return band;
    }
} // namespace

TEST_CASE ("WriteBack: EQ-Baender landen im Zustand, Attribute bleiben", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    auto eq = makeModule ("eq");
    setAttribute (eq, "lowgain", 3.0);
    setAttribute (eq, "midfreq", 800.0);
    eq.data.appendChild (makeBand (1, 80.0, 0.7, 6.0, 0.0, 0), nullptr);
    eq.data.appendChild (makeBand (0, 1200.0, 1.2, -4.0, 50.0, 0), nullptr);

    FxChain chain;
    chain.modules.push_back (std::move (eq));
    proc.setFxChain (chain);

    auto* module = proc.getModule (0);
    REQUIRE (module != nullptr);
    REQUIRE (dynamic_cast<EqualizerModule*> (module) != nullptr);

    // Ein Struktur-Edit, wie ihn die UI macht: Bandliste im Modul ersetzen.
    juce::ValueTree edited (juce::Identifier ("Module"));
    edited.appendChild (makeBand (0, 250.0, 1.0, 3.0, 0.0, 0), nullptr);
    edited.appendChild (makeBand (2, 6000.0, 0.8, -2.0, 20.0, 0), nullptr);
    edited.appendChild (makeBand (4, 12000.0, 0.7, 0.0, 0.0, 0), nullptr);
    module->setState (edited);

    proc.writeBackModuleStates();

    const auto state = proc.captureState();
    REQUIRE (state.fxChain().modules.size() == 1);

    const auto& data = state.fxChain().modules[0].data;

    // Kinder: genau die Baender des Moduls.
    REQUIRE (data.getNumChildren() == 3);
    REQUIRE (data.getChildWithName (juce::Identifier ("Band")).isValid());

    const auto band0 = data.getChild (0);
    const auto band1 = data.getChild (1);

    REQUIRE (band0.hasType (juce::Identifier ("Band")));
    REQUIRE ((int) band0.getProperty (juce::Identifier ("kind")) == 0);
    REQUIRE_THAT ((double) band0.getProperty (juce::Identifier ("f")), WithinAbs (250.0, 1.0e-6));
    REQUIRE_THAT ((double) band0.getProperty (juce::Identifier ("g")), WithinAbs (3.0, 1.0e-6));

    REQUIRE ((int) band1.getProperty (juce::Identifier ("kind")) == 2);
    REQUIRE_THAT ((double) band1.getProperty (juce::Identifier ("f")), WithinAbs (6000.0, 1.0e-6));
    REQUIRE_THAT ((double) band1.getProperty (juce::Identifier ("dyn")), WithinAbs (20.0, 1.0e-6));

    // Skalare Attribute sind unangetastet.
    REQUIRE_THAT ((double) data.getProperty (juce::Identifier ("lowgain")), WithinAbs (3.0, 1.0e-9));
    REQUIRE_THAT ((double) data.getProperty (juce::Identifier ("midfreq")), WithinAbs (800.0, 1.0e-9));
}

TEST_CASE ("WriteBack: Convolver-Kurven landen als <Eq>/<Dc> im Zustand", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    auto reverb = makeModule ("reverb", "C:/irs/plate.wav");
    setAttribute (reverb, "wet", 40.0);

    FxChain chain;
    chain.modules.push_back (std::move (reverb));
    proc.setFxChain (chain);

    auto* module = dynamic_cast<ConvolverModule*> (proc.getModule (0));
    REQUIRE (module != nullptr);

    float eqValues[ConvolverModule::numCurvePoints];
    float decayValues[ConvolverModule::numCurvePoints];

    for (int i = 0; i < ConvolverModule::numCurvePoints; ++i)
    {
        eqValues[i]    = (float) i - 5.0f;              // -5 … +5 dB
        decayValues[i] = 0.5f + 0.1f * (float) i;       // 0,5 … 1,5 x
    }

    module->setEqCurve (eqValues, ConvolverModule::numCurvePoints);
    module->setDecayCurve (decayValues, ConvolverModule::numCurvePoints);

    proc.writeBackModuleStates();

    const auto state = proc.captureState();
    REQUIRE (state.fxChain().modules.size() == 1);

    const auto& data = state.fxChain().modules[0].data;

    const auto eq = data.getChildWithName (juce::Identifier ("Eq"));
    const auto dc = data.getChildWithName (juce::Identifier ("Dc"));

    REQUIRE (eq.isValid());
    REQUIRE (dc.isValid());
    REQUIRE (eq.getNumChildren() == ConvolverModule::numCurvePoints);
    REQUIRE (dc.getNumChildren() == ConvolverModule::numCurvePoints);
    REQUIRE_THAT ((double) eq.getChild (0).getProperty (juce::Identifier ("y")),
                  WithinAbs (-5.0, 1.0e-5));
    REQUIRE_THAT ((double) dc.getChild (0).getProperty (juce::Identifier ("y")),
                  WithinAbs (0.5, 1.0e-5));

    // IR-Pfad und das skalare Attribut sind weiterhin da.
    REQUIRE (data.getProperty (juce::Identifier ("ir")).toString() == "C:/irs/plate.wav");
    REQUIRE_THAT ((double) data.getProperty (juce::Identifier ("wet")), WithinAbs (40.0, 1.0e-9));
}

TEST_CASE ("WriteBack: Module ohne Kinder lassen ihre Attribute in Ruhe", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    auto ott = makeModule ("ott");
    setAttribute (ott, "depth", 80.0);
    setAttribute (ott, "linear", 1.0);

    auto dynEq = makeModule ("dynEq");
    setAttribute (dynEq, "b1_gain", 3.5);

    FxChain chain;
    chain.modules.push_back (std::move (ott));
    chain.modules.push_back (std::move (dynEq));
    proc.setFxChain (chain);

    proc.writeBackModuleStates();

    const auto state = proc.captureState();
    REQUIRE (state.fxChain().modules.size() == 2);

    const auto& ottData = state.fxChain().modules[0].data;
    REQUIRE (ottData.getNumChildren() == 0);
    REQUIRE_THAT ((double) ottData.getProperty (juce::Identifier ("depth")), WithinAbs (80.0, 1.0e-9));
    REQUIRE_THAT ((double) ottData.getProperty (juce::Identifier ("linear")), WithinAbs (1.0, 1.0e-9));

    const auto& dynData = state.fxChain().modules[1].data;
    REQUIRE (dynData.getNumChildren() == 0);
    REQUIRE_THAT ((double) dynData.getProperty (juce::Identifier ("b1_gain")), WithinAbs (3.5, 1.0e-9));
}

TEST_CASE ("WriteBack: unbekannte Modultypen bleiben unveraendert", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    auto ghost = makeModule ("voelligNeu");
    setAttribute (ghost, "x", 42.0);

    FxChain chain;
    chain.modules.push_back (std::move (ghost));
    proc.setFxChain (chain);

    REQUIRE (proc.getModule (0) == nullptr);   // Platzhalter, kein Modul

    proc.writeBackModuleStates();

    const auto state = proc.captureState();
    REQUIRE (state.fxChain().modules.size() == 1);
    REQUIRE (state.fxChain().modules[0].type == "voelligNeu");
    REQUIRE_THAT ((double) state.fxChain().modules[0].data.getProperty (juce::Identifier ("x")),
                  WithinAbs (42.0, 1.0e-9));
}

TEST_CASE ("Live-Bedienung: Regler wirken sofort und landen im Zustand", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("ott"));
    proc.setFxChain (chain);

    auto* module = proc.getModule (0);
    REQUIRE (module != nullptr);
    REQUIRE (module->params()[0].id == "depth");

    const int latencyBefore = proc.getLatencySamples();

    proc.setModuleParam (0, 0, 80.0f);

    // Sofort im DSP ...
    REQUIRE_THAT (proc.getModuleParam (0, 0), WithinAbs (80.0f, 1.0e-4f));
    REQUIRE_THAT (module->getParam (0), WithinAbs (80.0f, 1.0e-4f));

    // ... und im Zustand (ohne Rebuild, ohne Latenzaenderung).
    REQUIRE (proc.getEngineModuleCount() == 1);
    REQUIRE (proc.getLatencySamples() == latencyBefore);

    auto state = proc.captureState();
    REQUIRE_THAT ((double) state.fxChain().modules[0].data.getProperty (juce::Identifier ("depth")),
                  WithinAbs (80.0, 1.0e-6));

    // Klemmen auf den deklarierten Bereich (depth: 0…100).
    proc.setModuleParam (0, 0, 500.0f);
    REQUIRE_THAT (proc.getModuleParam (0, 0), WithinAbs (100.0f, 1.0e-4f));

    // Attribut-Weg: setzt Wert + DSP fuer echte Parameter, nur Zustand sonst.
    proc.setModuleAttribute (0, "thL", -40.0);
    REQUIRE_THAT (proc.getModuleParam (0, 10), WithinAbs (-40.0f, 1.0e-4f));

    proc.setModuleAttribute (0, "nurImZustand", 7.0);
    state = proc.captureState();
    REQUIRE_THAT ((double) state.fxChain().modules[0].data.getProperty (juce::Identifier ("nurImZustand")),
                  WithinAbs (7.0, 1.0e-9));
}

TEST_CASE ("Live-Bedienung: MIX, OUT GAIN und Power aktualisieren Zustand und Engine", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("eq"));
    proc.setFxChain (chain);

    proc.setModuleMix (0, 0.25f);
    proc.setModuleOutGain (0, 1.5f);
    proc.setModuleEnabled (0, false);

    auto state = proc.captureState();
    REQUIRE (state.fxChain().modules.size() == 1);
    REQUIRE_THAT (state.fxChain().modules[0].mix, WithinAbs (0.25f, 1.0e-6));
    REQUIRE_THAT (state.fxChain().modules[0].outGain, WithinAbs (1.5f, 1.0e-6));
    REQUIRE (! state.fxChain().modules[0].enabled);

    // Klemmen: MIX 0…1, OUT 0…2.
    proc.setModuleMix (0, 3.0f);
    proc.setModuleOutGain (0, -1.0f);

    state = proc.captureState();
    REQUIRE_THAT (state.fxChain().modules[0].mix, WithinAbs (1.0f, 1.0e-6));
    REQUIRE_THAT (state.fxChain().modules[0].outGain, WithinAbs (0.0f, 1.0e-6));

    // Ungueltige Slots sind stille No-Ops.
    proc.setModuleMix (9, 0.5f);
    proc.setModuleParam (9, 0, 1.0f);
    proc.setModuleEnabled (9, true);
    REQUIRE (proc.getEngineModuleCount() == 1);
}
