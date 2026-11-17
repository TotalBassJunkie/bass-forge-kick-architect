#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>

#include <memory>
#include <utility>

#include "core/KickState.h"
#include "plugin/PluginProcessor.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Die Tests bauen echte PluginProcessor-Instanzen. Der APVTS startet einen
// Timer, deshalb braucht die Konsolen-App einen MessageManager.
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    FxModuleState makeModule (const juce::String& type,
                              float mix = 1.0f,
                              float outGain = 1.0f,
                              bool enabled = true)
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = enabled;
        state.mix     = mix;
        state.outGain = outGain;
        return state;
    }

    void setAttribute (FxModuleState& state, const juce::String& id, double value)
    {
        if (! state.data.isValid())
            state.data = juce::ValueTree (juce::Identifier ("Module"));

        state.data.setProperty (juce::Identifier (id), juce::var (value), nullptr);
    }

    /** 256 (Multiband, linearphasig) + 2048 (Convolver, fester Kopf) = 2304. */
    FxChain makeLatencyChain (bool bypassAll = false)
    {
        FxChain chain;
        chain.bypassAll = bypassAll;

        auto ott = makeModule ("ott");
        setAttribute (ott, "linear", 1.0);
        chain.modules.push_back (std::move (ott));

        auto reverb = makeModule ("reverb");
        setAttribute (reverb, "zeroLat", 0.0);
        chain.modules.push_back (std::move (reverb));

        return chain;
    }
} // namespace

TEST_CASE ("Prozessor: Zustand mit Slots und FX ueberlebt Speichern/Laden", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor source;
    source.prepareToPlay (48000.0, 128);

    KickState state;

    SampleRef ref;
    ref.relPath  = "Main - Punches/Techno/Punch Techno 1 150.wav";
    ref.fileName = "Punch Techno 1 150.wav";
    ref.roleId   = "punch";

    state.setSample (2, ref);
    state.setRole (2, Role::punch);
    state.setLocked (2, true);
    state.setKeyTracked (2, true);
    state.setStyleId ("techno");
    state.setPresetName ("Roundtrip");

    auto ott = makeModule ("ott", 0.6f, 1.1f, true);
    setAttribute (ott, "depth", 80.0);
    state.fxChain().modules.push_back (std::move (ott));

    auto reverb = makeModule ("reverb", 0.4f, 0.95f, false);
    setAttribute (reverb, "wet", 25.0);
    reverb.data.setProperty ("ir", "C:/irs/hall.wav", nullptr);
    reverb.data.appendChild (juce::ValueTree ("Eq"), nullptr);
    state.fxChain().modules.push_back (std::move (reverb));

    source.applyState (state);

    juce::MemoryBlock block;
    source.getStateInformation (block);
    REQUIRE (block.getSize() > 0);

    PluginProcessor target;
    target.setStateInformation (block.getData(), (int) block.getSize());

    const auto restored = target.captureState();

    // Slot-Inhalt
    REQUIRE (restored.getSample (2).has_value());
    REQUIRE (restored.getSample (2)->fileName == "Punch Techno 1 150.wav");
    REQUIRE (restored.getRole (2) == Role::punch);
    REQUIRE (restored.isLocked (2));
    REQUIRE (restored.isKeyTracked (2));

    // Meta
    REQUIRE (restored.getStyleId() == "techno");
    REQUIRE (restored.getPresetName() == "Roundtrip");

    // FX-Kette
    REQUIRE (restored.fxChain().modules.size() == 2);
    REQUIRE (restored.fxChain().modules[0].type == "ott");
    REQUIRE_THAT (restored.fxChain().modules[0].mix, WithinAbs (0.6f, 1.0e-6));
    REQUIRE_THAT (restored.fxChain().modules[0].outGain, WithinAbs (1.1f, 1.0e-6));
    REQUIRE_THAT ((double) restored.fxChain().modules[0].data.getProperty ("depth"),
                  WithinAbs (80.0, 1.0e-9));
    REQUIRE (restored.fxChain().modules[1].type == "reverb");
    REQUIRE (! restored.fxChain().modules[1].enabled);
    REQUIRE (restored.fxChain().modules[1].data.getProperty ("ir").toString() == "C:/irs/hall.wav");

    // Und der geladene Prozessor baut die Kette auch wirklich.
    target.prepareToPlay (48000.0, 128);
    REQUIRE (target.getEngineModuleCount() == 2);
}

TEST_CASE ("Prozessor: prepareToPlay baut die Kette und meldet die Latenz", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;

    // Vor prepareToPlay: Kette nur im Zustand, keine Engine, keine Latenz.
    proc.applyState (KickState {});
    proc.setFxChain (makeLatencyChain());
    REQUIRE (proc.getEngineModuleCount() == 0);
    REQUIRE (proc.getLatencySamples() == 0);

    proc.prepareToPlay (48000.0, 128);

    REQUIRE (proc.getEngineModuleCount() == 2);
    REQUIRE (proc.getLatencySamples() == 2304);
}

TEST_CASE ("Prozessor: setFxChain aktualisiert Latenz und Engine", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    REQUIRE (proc.getEngineModuleCount() == 0);
    REQUIRE (proc.getLatencySamples() == 0);

    proc.setFxChain (makeLatencyChain());
    REQUIRE (proc.getEngineModuleCount() == 2);
    REQUIRE (proc.getLatencySamples() == 2304);

    // Ketten-Bypass haelt die Latenz (die Module bleiben im Raster).
    proc.setFxChain (makeLatencyChain (true));
    REQUIRE (proc.getEngineModuleCount() == 2);
    REQUIRE (proc.getLatencySamples() == 2304);

    // Latenzfreie Kette danach: 0.
    FxChain plain;
    plain.modules.push_back (makeModule ("eq"));
    proc.setFxChain (plain);
    REQUIRE (proc.getEngineModuleCount() == 1);
    REQUIRE (proc.getLatencySamples() == 0);
}

TEST_CASE ("Prozessor: unbekannter Modultyp behaelt seinen Kettenplatz", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;
    chain.modules.push_back (makeModule ("voelligNeu"));
    chain.modules.push_back (makeModule ("eq"));

    proc.setFxChain (chain);

    REQUIRE (proc.getEngineModuleCount() == 2);
    REQUIRE (proc.getLatencySamples() == 0);

    // Der Zustand kennt den unbekannten Typ weiterhin.
    const auto state = proc.captureState();
    REQUIRE (state.fxChain().modules.size() == 2);
    REQUIRE (state.fxChain().modules[0].type == "voelligNeu");
}

TEST_CASE ("Prozessor: frischer Zustand hat eine leere Kette", "[state][plugin]")
{
    JuceGuiScope gui;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    const auto state = proc.captureState();
    REQUIRE (state.fxChain().modules.empty());
    REQUIRE (! state.fxChain().bypassAll);
    REQUIRE (proc.getEngineModuleCount() == 0);
    REQUIRE (proc.getLatencySamples() == 0);
}
