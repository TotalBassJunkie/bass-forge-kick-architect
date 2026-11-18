#include <catch2/catch_test_macros.hpp>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "core/PresetIO.h"
#include "core/KickState.h"
#include "engine/KickAssembler.h"
#include "engine/KickRenderer.h"
#include "fx/FxChainProcessor.h"

#include <memory>

using namespace bfka;

// B-133: Crash-Haertung. Jeder Fall muss ohne Absturz und ohne unbehandelte
// Ausnahme durchlaufen; die erwarteten Ergebnisse stehen im Test selbst.
namespace
{
    /** Fuer Tests, die GUI-Objekte anlegen (hier derzeit keiner). */
    struct JuceGuiScope
    {
        juce::ScopedJuceInitialiser_GUI init;
    };

    /** Frisches Arbeitsverzeichnis je Test (wird am Ende geloescht). */
    struct TempDir
    {
        juce::File directory;

        TempDir()
        {
            directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("bfka-robust-tests")
                            .getChildFile (juce::String (juce::Random::getSystemRandom().nextInt64()));

            directory.deleteRecursively();
            directory.createDirectory();
        }

        ~TempDir()
        {
            if (directory.isDirectory())
                directory.deleteRecursively();
        }

        juce::File file (const juce::String& name) const
        {
            return directory.getChildFile (name);
        }
    };
} // namespace

// =========================================================================
// Preset-Dateien
// =========================================================================
TEST_CASE ("Haertung: Preset-Datei mit 0 Bytes", "[robust][preset]")
{
    TempDir temp;

    const auto file = temp.file ("leer.bfkapreset");
    REQUIRE (file.create());

    REQUIRE (file.existsAsFile());
    REQUIRE (file.getSize() == 0);

    juce::String error;
    auto data = PresetIO::loadKickPreset (file, error);

    REQUIRE_FALSE (data.has_value());
    REQUIRE (error.isNotEmpty());
}

TEST_CASE ("Haertung: Preset-Datei mit Muell", "[robust][preset]")
{
    TempDir temp;

    const auto file = temp.file ("muell.bfkapreset");
    file.replaceWithText ("<notxml");

    juce::String error;
    REQUIRE_FALSE (PresetIO::loadKickPreset (file, error).has_value());
    CHECK (error.isNotEmpty());
}

TEST_CASE ("Haertung: Preset ohne Endung", "[robust][preset]")
{
    REQUIRE_FALSE (PresetIO::hasPresetExtension (juce::File ("C:/x/foo.txt")));
    REQUIRE (PresetIO::hasPresetExtension (juce::File ("C:/x/foo.bfkapreset")));
}

// =========================================================================
// Engine: fehlende und kaputte Samples
// =========================================================================
TEST_CASE ("Haertung: KickAssembler mit fehlendem Content-Ordner", "[robust][engine]")
{
    KickState state;

    SampleRef ref;
    ref.relPath  = "Main - Punches/fehlt.wav";
    ref.fileName = "fehlt.wav";
    state.setSample (0, ref);

    KickRenderer renderer;
    renderer.prepare (48000.0, 128);

    KickAssembler assembler;
    juce::StringArray warnings;

    // Der Content-Ordner existiert nicht: Laden darf nur warnen, nicht werfen.
    REQUIRE_NOTHROW (assembler.loadFromState (state, juce::File ("C:/gibt/es/nicht"), renderer, warnings));

    REQUIRE (warnings.size() >= 1);
    REQUIRE_FALSE (renderer.isSlotFilled (0));
}

TEST_CASE ("Haertung: kaputte WAV-Datei wird abgelehnt", "[robust][engine]")
{
    TempDir temp;

    // 100 Bytes Muell mit WAV-Endung.
    juce::String garbage;
    for (int i = 0; i < 20; ++i)
        garbage += "MUELL";

    const auto file = temp.file ("kaputt.wav");
    file.replaceWithText (garbage);

    REQUIRE (file.existsAsFile());
    REQUIRE (file.getSize() == 100);

    juce::AudioFormatManager mgr;
    mgr.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> probe (mgr.createReaderFor (file));
    REQUIRE (probe == nullptr);

    KickState state;

    SampleRef ref;
    ref.absPath  = file.getFullPathName();
    ref.fileName = file.getFileName();
    state.setSample (0, ref);

    KickRenderer renderer;
    renderer.prepare (48000.0, 128);

    KickAssembler assembler;
    juce::StringArray warnings;

    // Oeffentlicher Weg: loadOneSlot() meldet "Nicht lesbar" als Warnung.
    bool loaded = true;
    REQUIRE_NOTHROW (loaded = assembler.loadOneSlot (0, state, temp.directory, renderer, warnings));

    CHECK_FALSE (loaded);
    CHECK_FALSE (renderer.isSlotFilled (0));
    CHECK (warnings.size() >= 1);
}

// =========================================================================
// Engine/FX: Aufrufe ohne Inhalt bzw. ohne prepare
// =========================================================================
TEST_CASE ("Haertung: leerer KickState rendert Stille", "[robust][engine]")
{
    KickState state;
    REQUIRE_FALSE (state.getSample (0).has_value()); // Ausgangslage: kein Sample

    KickRenderer renderer;
    renderer.prepare (48000.0, 128);
    renderer.noteOn();

    juce::AudioBuffer<float> out (2, 512);
    out.clear();

    // Block groesser als prepare(): renderNextBlock() klemmt intern.
    REQUIRE_NOTHROW (renderer.renderNextBlock (out, 0, 512));
    REQUIRE (out.getMagnitude (0, 512) == 0.0f);
}

TEST_CASE ("Haertung: Renderer ohne prepare ist harmlos", "[robust][engine]")
{
    KickRenderer renderer;

    juce::AudioBuffer<float> out (2, 128);
    out.clear();

    REQUIRE_NOTHROW (renderer.renderNextBlock (out, 0, 128));
    REQUIRE (out.getMagnitude (0, 128) == 0.0f);
}

TEST_CASE ("Haertung: FX-Kette ohne prepare ist harmlos", "[robust][fx]")
{
    FxChainProcessor fx;

    juce::AudioBuffer<float> out (2, 128);
    out.clear();

    REQUIRE_NOTHROW (fx.process (out));
    REQUIRE (out.getMagnitude (0, 128) == 0.0f);
}
