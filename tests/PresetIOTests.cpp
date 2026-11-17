#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_events/juce_events.h>

#include <memory>

#include "core/PresetIO.h"
#include "plugin/PluginProcessor.h"
#include "ui/FxChainModel.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

// Preset-IO (B-93) und A/B-Vergleich (B-94).
namespace
{
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Frisches Arbeitsverzeichnis je Test (wird am Ende geloescht). */
    struct TempDirectory
    {
        juce::File directory;

        TempDirectory()
        {
            directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("bfka-preset-tests")
                            .getChildFile (juce::String (juce::Random::getSystemRandom().nextInt64()));

            directory.createDirectory();
        }

        ~TempDirectory()
        {
            if (directory.isDirectory())
                directory.deleteRecursively();
        }

        juce::File file (const juce::String& name) const
        {
            return directory.getChildFile (name);
        }
    };

    FxModuleState makeModule (const juce::String& type)
    {
        FxModuleState state;
        state.type    = type;
        state.enabled = true;
        state.mix     = 1.0f;
        state.outGain = 1.0f;
        return state;
    }

    void setAttribute (FxModuleState& state, const juce::String& id, double value)
    {
        if (! state.data.isValid())
            state.data = juce::ValueTree (juce::Identifier ("Module"));

        state.data.setProperty (juce::Identifier (id), juce::var (value), nullptr);
    }

    /** Band-Kind hinzufuegen, wie es das Convolver-/EQ-Modul liefert. */
    void addBand (FxModuleState& state, int kind, double freq, double gain)
    {
        if (! state.data.isValid())
            state.data = juce::ValueTree (juce::Identifier ("Module"));

        juce::ValueTree band ("Band");
        band.setProperty ("kind", kind, nullptr);
        band.setProperty ("f", freq, nullptr);
        band.setProperty ("g", gain, nullptr);
        state.data.appendChild (band, nullptr);
    }

    PresetInfo makeInfo()
    {
        PresetInfo info;
        info.name    = "Rumble 150";
        info.author  = "TBJ";
        info.styleId = "rawstyle";
        return info;
    }
} // namespace

// =========================================================================
// Kick-Preset
// =========================================================================
TEST_CASE ("Preset: Kick-Preset Roundtrip mit Kette", "[preset]")
{
    TempDirectory temp;

    KickState state;
    state.setPresetName ("Testkick");
    state.setStyleId ("rawstyle");
    state.setBpm (155.0);
    state.setParam ("l0_vol", 0.25f);
    state.setParam ("master_vol", 0.75f);

    FxChain& chain = state.fxChain();

    auto eq = makeModule ("eq");
    setAttribute (eq, "amount", 30.0);
    addBand (eq, 1, 71.64, 15.7);
    chain.modules.push_back (eq);

    auto shaper = makeModule ("shaper");
    setAttribute (shaper, "amount", 80.0);
    chain.modules.push_back (shaper);

    auto info = makeInfo();
    juce::String error;

    const auto file = temp.file ("Mein Kick");

    REQUIRE (PresetIO::saveKickPreset (file, state, info, error));
    INFO (error);

    // Endung wurde ergaenzt.
    const auto saved = PresetIO::withPresetExtension (file);
    REQUIRE (saved.existsAsFile());
    CHECK (saved.getFileExtension() == ".bfkapreset");

    auto loaded = PresetIO::loadKickPreset (saved, error);
    REQUIRE (loaded.has_value());
    INFO (error);

    CHECK (loaded->info.name == "Rumble 150");
    CHECK (loaded->info.author == "TBJ");
    CHECK (loaded->info.styleId == "rawstyle");
    CHECK (loaded->info.created.isNotEmpty());
    CHECK_FALSE (loaded->info.isModule);

    CHECK (loaded->state.getStyleId() == "rawstyle");
    CHECK_THAT (loaded->state.getBpm(), WithinAbs (155.0, 1.0e-9));
    CHECK_THAT (loaded->state.getParam ("l0_vol"), WithinAbs (0.25f, 1.0e-6f));
    CHECK_THAT (loaded->state.getParam ("master_vol"), WithinAbs (0.75f, 1.0e-6f));

    REQUIRE (loaded->state.fxChain().modules.size() == 2);
    CHECK (loaded->state.fxChain().modules[0].type == "eq");
    CHECK (loaded->state.fxChain().modules[1].type == "shaper");

    // Struktur (Band) kommt mit.
    const auto band = loaded->state.fxChain().modules[0].data.getChild (0);
    REQUIRE (band.isValid());
    CHECK (band.hasType (juce::Identifier ("Band")));
    CHECK_THAT ((double) band.getProperty ("f"), WithinAbs (71.64, 1.0e-6));
}

TEST_CASE ("Preset: fehlende Endung und unbekannte Wurzel", "[preset]")
{
    TempDirectory temp;

    KickState state;
    auto info = makeInfo();
    juce::String error;

    // Ohne Endung: wird beim Speichern ergaenzt.
    const auto file = temp.file ("ohne_endung");
    REQUIRE (PresetIO::saveKickPreset (file, state, info, error));
    CHECK (PresetIO::hasPresetExtension (temp.file ("ohne_endung.bfkapreset")));
    CHECK (temp.file ("ohne_endung.bfkapreset").existsAsFile());

    // Datei mit falscher Wurzel wird abgelehnt.
    const auto broken = temp.file ("broken.bfkapreset");
    broken.replaceWithText ("<EtwasAnderes/>");

    CHECK_FALSE (PresetIO::loadKickPreset (broken, error).has_value());
    CHECK (error.isNotEmpty());

    // Kein XML.
    const auto garbage = temp.file ("garbage.bfkapreset");
    garbage.replaceWithText ("das ist kein xml");
    CHECK_FALSE (PresetIO::loadKickPreset (garbage, error).has_value());

    // Fehlende Datei.
    CHECK_FALSE (PresetIO::loadKickPreset (temp.file ("fehlt.bfkapreset"), error).has_value());
}

TEST_CASE ("Preset: Modul-Preset Roundtrip", "[preset]")
{
    TempDirectory temp;

    auto module = makeModule ("dynEq");
    setAttribute (module, "sidechain", 1.0);
    setAttribute (module, "b1_gain", 5.0);
    addBand (module, 1, 60.0, 5.0);

    PresetInfo info;
    info.name = "Bass Shelf";

    juce::String error;
    const auto file = temp.file ("BassShelf.bfkapreset");

    REQUIRE (PresetIO::saveModulePreset (file, module, info, error));
    INFO (error);

    auto loaded = PresetIO::loadModulePreset (file, error);
    REQUIRE (loaded.has_value());
    INFO (error);

    CHECK (loaded->info.isModule);
    CHECK (loaded->info.moduleType == "dynEq");
    CHECK (loaded->module.type == "dynEq");
    CHECK_THAT ((double) loaded->module.data.getProperty ("sidechain"), WithinAbs (1.0, 1.0e-9));
    CHECK_THAT ((double) loaded->module.data.getProperty ("b1_gain"), WithinAbs (5.0, 1.0e-9));

    const auto band = loaded->module.data.getChild (0);
    REQUIRE (band.isValid());
    CHECK (band.hasType (juce::Identifier ("Band")));

    // Verwechslung wird erkannt: Modul-Preset ist kein Kick-Preset.
    CHECK_FALSE (PresetIO::loadKickPreset (file, error).has_value());

    // Und umgekehrt.
    const auto kickFile = temp.file ("Kick.bfkapreset");
    REQUIRE (PresetIO::saveKickPreset (kickFile, KickState{}, makeInfo(), error));
    CHECK_FALSE (PresetIO::loadModulePreset (kickFile, error).has_value());
}

// =========================================================================
// Prozessor: Preset laden/speichern
// =========================================================================
TEST_CASE ("Preset: Prozessor laedt ein Preset mit Kette", "[preset]")
{
    JuceGuiScope guiScope;
    TempDirectory temp;

    juce::String error;

    {
        PluginProcessor source;
        source.prepareToPlay (48000.0, 128);

        KickState state;
        state.setStyleId ("zaag");
        state.setBpm (160.0);
        state.setParam ("l0_vol", 0.4f);
        state.fxChain().modules.push_back (makeModule ("ott"));

        auto info = makeInfo();
        info.name = "Zaag Kick";

        REQUIRE (PresetIO::saveKickPreset (temp.file ("Zaag"), state, info, error));
    }

    PluginProcessor target;
    target.prepareToPlay (48000.0, 128);

    REQUIRE (target.loadPreset (temp.file ("Zaag.bfkapreset"), error));
    INFO (error);

    const auto captured = target.captureState();
    CHECK (captured.getStyleId() == "zaag");
    CHECK_THAT (captured.getBpm(), WithinAbs (160.0, 1.0e-9));
    CHECK_THAT (captured.getParam ("l0_vol"), WithinAbs (0.4f, 1.0e-6f));

    REQUIRE (captured.fxChain().modules.size() == 1);
    CHECK (captured.fxChain().modules[0].type == "ott");
    CHECK (target.getEngineModuleCount() == 1);

    // Kopfdaten lesbar, ohne den Zustand zu uebernehmen.
    auto info = PluginProcessor::readPresetInfo (temp.file ("Zaag.bfkapreset"), error);
    REQUIRE (info.has_value());
    CHECK (info->name == "Zaag Kick");
}

TEST_CASE ("Preset: Modul-Preset im Prozessor behaelt den Kettenplatz", "[preset]")
{
    JuceGuiScope guiScope;
    TempDirectory temp;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    FxChain chain;

    auto eq = makeModule ("eq");
    eq.mix     = 0.5f;
    eq.outGain = 1.25f;
    chain.modules.push_back (eq);

    auto finalizer = makeModule ("final");
    chain.modules.push_back (finalizer);

    proc.setFxChain (chain);

    // Modul 0 als Modul-Preset mit anderem Typ ist unzulaessig (Kette wuerde
    // sonst ihre Struktur tauschen) — geprueft wird hier nur der normale Weg.
    juce::String error;

    PresetInfo info;
    info.name = "EQ breit";

    REQUIRE (proc.saveModulePreset (0, temp.file ("EQbreit"), info, error));
    INFO (error);

    // Plaztausch: eigenes Preset in Platz 1 laden -> Typ wird zu "eq",
    // MIX/OUT des Platzes bleiben.
    REQUIRE (proc.loadModulePreset (1, temp.file ("EQbreit.bfkapreset"), error));
    INFO (error);

    const auto& modules = proc.getFxChain().modules;
    REQUIRE (modules.size() == 2);
    CHECK (modules[1].type == "eq");
    CHECK_THAT (modules[1].mix, WithinAbs (1.0f, 1.0e-6f));
    CHECK_THAT (modules[1].outGain, WithinAbs (1.0f, 1.0e-6f));
    CHECK (modules[0].type == "eq");

    // Ungueltiger Platz meldet einen Fehler.
    CHECK_FALSE (proc.loadModulePreset (7, temp.file ("EQbreit.bfkapreset"), error));
}

// =========================================================================
// A/B-Vergleich (B-94)
// =========================================================================
TEST_CASE ("A/B: zwei Zustaende merken und zurueckholen", "[preset]")
{
    JuceGuiScope guiScope;

    PluginProcessor proc;
    proc.prepareToPlay (48000.0, 128);

    CHECK_FALSE (proc.hasComparisonState (0));
    CHECK_FALSE (proc.hasComparisonState (1));
    CHECK_FALSE (proc.restoreComparisonState (0));
    CHECK (proc.activeComparisonSlot() == -1);

    // A: ohne Kette, l0_vol = 0.25
    proc.parameters().getParameter ("l0_vol")->setValueNotifyingHost (0.25f);
    proc.storeComparisonState (0);

    // B: mit Kette, l0_vol = 0.5
    FxChain chain;
    chain.modules.push_back (makeModule ("ott"));
    proc.setFxChain (chain);
    proc.parameters().getParameter ("l0_vol")->setValueNotifyingHost (0.5f);
    proc.storeComparisonState (1);

    CHECK (proc.hasComparisonState (0));
    CHECK (proc.hasComparisonState (1));
    CHECK (proc.activeComparisonSlot() == 1);

    // A zurueckholen: Parameter und Kette.
    REQUIRE (proc.restoreComparisonState (0));
    CHECK (proc.activeComparisonSlot() == 0);
    CHECK_THAT (proc.captureState().getParam ("l0_vol"), WithinAbs (0.25f, 1.0e-6f));
    CHECK (proc.getFxChain().modules.empty());
    CHECK (proc.getEngineModuleCount() == 0);

    // B wieder.
    REQUIRE (proc.restoreComparisonState (1));
    CHECK_THAT (proc.captureState().getParam ("l0_vol"), WithinAbs (0.5f, 1.0e-6f));
    CHECK (proc.getFxChain().modules.size() == 1);

    // Ungueltige Indizes werden ignoriert.
    proc.storeComparisonState (5);
    CHECK (proc.activeComparisonSlot() == 1);
    CHECK_FALSE (proc.restoreComparisonState (-1));
}

// =========================================================================
// Ketten-Header: Sperre und Werkskette (B-94)
// =========================================================================
TEST_CASE ("Kette: Sperre verhindert Struktur-Aenderungen", "[preset][chain]")
{
    FxChainModel model;

    REQUIRE (model.addModule (FxType::equalizer) == 0);
    REQUIRE (model.addModule (FxType::finalizer) == 1);

    model.setLocked (true);
    CHECK (model.isLocked());

    CHECK (model.addModule (FxType::multiband) == -1);
    CHECK_FALSE (model.removeModule (0));
    CHECK_FALSE (model.moveModule (0, 1));
    CHECK (model.duplicateModule (0) == -1);

    model.clear();
    CHECK (model.size() == 2);

    // Bypass und Regler bleiben erlaubt.
    model.setChainBypass (true);
    CHECK (model.isChainBypassed());

    model.setLocked (false);
    REQUIRE (model.addModule (FxType::multiband) == 2);
    CHECK (model.size() == 3);
}

TEST_CASE ("Kette: Werkskette ist definiert und serialisierbar", "[preset][chain]")
{
    const auto chain = FxChainModel::makeDefaultChain();

    REQUIRE (chain.modules.size() == 2);
    CHECK (chain.modules[0].type == "filt");
    CHECK (chain.modules[1].type == "final");
    CHECK_FALSE (chain.bypassAll);

    for (const auto& module : chain.modules)
    {
        CHECK (module.enabled);
        CHECK_THAT (module.mix, WithinAbs (1.0f, 1.0e-6f));
        CHECK_THAT (module.outGain, WithinAbs (1.0f, 1.0e-6f));
        // Kein `data`: die Module starten mit ihren eigenen Defaults.
        CHECK_FALSE (module.data.isValid());
    }
}
