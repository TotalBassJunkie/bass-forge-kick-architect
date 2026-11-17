#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_data_structures/juce_data_structures.h>

#include <memory>
#include <utility>

#include "core/KickState.h"
#include "fx/FxChainProcessor.h"
#include "fx/FxFactory.h"
#include "fx/FxModule.h"
#include "fx/PassThroughModule.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

    void fillBuffer (juce::AudioBuffer<float>& buffer, float value)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int n = 0; n < buffer.getNumSamples(); ++n)
                data[n] = value;
        }
    }

    void putImpulse (juce::AudioBuffer<float>& buffer)
    {
        buffer.clear();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, 0, 1.0f);
    }

    /** Ein Kettenplatz mit Typ, MIX, OUT GAIN und Schaltzustand. */
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

    /** Setzt ein Modulattribut (Anzeige-Einheit) im Zustands-ValueTree. */
    void setAttribute (FxModuleState& state, const juce::String& id, double value)
    {
        if (! state.data.isValid())
            state.data = juce::ValueTree (juce::Identifier ("Module"));

        state.data.setProperty (juce::Identifier (id), juce::var (value), nullptr);
    }

    /** Tauscht fuer die Dauer eines Tests den Creator eines Typs aus. */
    struct ScopedTypeOverride
    {
        ScopedTypeOverride (FxType type, FxFactory::Creator creator)
            : overridden (type)
        {
            FxFactory::get().registerType (overridden, std::move (creator));
        }

        ~ScopedTypeOverride()
        {
            const auto type = overridden;

            FxFactory::get().registerType (type,
                                           [type] { return std::make_unique<PassThroughModule> (type); });
        }

        FxType overridden;
    };

    const std::vector<FxType>& allFxTypes()
    {
        static const std::vector<FxType> types
        {
            FxType::filters, FxType::equalizer, FxType::midSideEq, FxType::distortion,
            FxType::kickShaper, FxType::transientShaper, FxType::convolver,
            FxType::finalizer, FxType::multiband
        };

        return types;
    }
} // namespace

// ---------------------------------------------------------------------------
// Typ-Kennungen
// ---------------------------------------------------------------------------
TEST_CASE ("Modultyp-Kennungen werden korrekt abgebildet", "[fx]")
{
    REQUIRE (fxTypeFromString ("filt")   == FxType::filters);
    REQUIRE (fxTypeFromString ("eq")     == FxType::equalizer);
    REQUIRE (fxTypeFromString ("mseq")   == FxType::midSideEq);
    REQUIRE (fxTypeFromString ("dist")   == FxType::distortion);
    REQUIRE (fxTypeFromString ("shaper") == FxType::kickShaper);
    REQUIRE (fxTypeFromString ("trans")  == FxType::transientShaper);
    REQUIRE (fxTypeFromString ("reverb") == FxType::convolver);
    REQUIRE (fxTypeFromString ("final")  == FxType::finalizer);
    REQUIRE (fxTypeFromString ("ott")    == FxType::multiband);

    // Gross-/Kleinschreibung und Leerzeichen sind tolerant.
    REQUIRE (fxTypeFromString ("  DIST ") == FxType::distortion);

    // Unbekannt -> nullopt.
    REQUIRE_FALSE (fxTypeFromString ("quatsch").has_value());
    REQUIRE_FALSE (fxTypeFromString ("").has_value());

    for (const auto type : allFxTypes())
    {
        const auto text = toString (type);
        REQUIRE (text.isNotEmpty());
        REQUIRE (fxTypeFromString (text) == type);
    }
}

TEST_CASE ("Registry kennt alle neun Typen und erzeugt Module", "[fx]")
{
    const auto& factory = FxFactory::get();
    const auto types = factory.availableTypes();

    REQUIRE (types.size() == kNumFxTypes);

    for (const auto type : allFxTypes())
    {
        const auto text = toString (type);
        REQUIRE (types.contains (text));
        REQUIRE (factory.isRegistered (type));

        const auto module = factory.create (text);
        REQUIRE (module != nullptr);
        REQUIRE (module->type() == type);
        REQUIRE_FALSE (module->params().empty());
        REQUIRE (module->displayName().isNotEmpty());
    }

    // Unbekannte Kennung liefert keine Instanz.
    REQUIRE (factory.create ("quatsch") == nullptr);
}

// ---------------------------------------------------------------------------
// Verstaerkung, Bypass, MIX
// ---------------------------------------------------------------------------
TEST_CASE ("Drei Module mit OUT GAIN 0.5 ergeben 0.125", "[fx]")
{
    FxChain chain;

    for (int i = 0; i < 3; ++i)
        chain.modules.push_back (makeModule ("ott", 1.0f, 0.5f));

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    REQUIRE (chainProcessor.isPrepared());
    REQUIRE (chainProcessor.numModules() == 3);
    REQUIRE (chainProcessor.modules().size() == 3);
    REQUIRE (chainProcessor.modules()[0] != nullptr);
    REQUIRE (chainProcessor.totalLatencySamples() == 0);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillBuffer (buffer, 1.0f);

    chainProcessor.process (buffer);

    REQUIRE_THAT (buffer.getSample (0, 0),  WithinAbs (0.125f, 1.0e-6f));
    REQUIRE_THAT (buffer.getSample (1, 64), WithinAbs (0.125f, 1.0e-6f));
    REQUIRE_THAT (buffer.getSample (0, kBlockSize - 1), WithinAbs (0.125f, 1.0e-6f));
}

TEST_CASE ("setChain vor prepare funktioniert ebenfalls", "[fx]")
{
    FxChain chain;
    chain.modules.push_back (makeModule ("ott", 1.0f, 0.5f));

    FxChainProcessor chainProcessor;
    chainProcessor.setChain (chain);          // noch ohne prepare()
    REQUIRE_FALSE (chainProcessor.isPrepared());

    // Ohne prepare() wird nichts verarbeitet.
    juce::AudioBuffer<float> unprepared (2, kBlockSize);
    fillBuffer (unprepared, 1.0f);
    chainProcessor.process (unprepared);
    REQUIRE_THAT (unprepared.getSample (0, 0), WithinAbs (1.0f, 1.0e-6f));

    chainProcessor.prepare (kSampleRate, kBlockSize);
    REQUIRE (chainProcessor.isPrepared());
    REQUIRE (chainProcessor.numModules() == 1);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillBuffer (buffer, 1.0f);
    chainProcessor.process (buffer);
    REQUIRE_THAT (buffer.getSample (0, 0), WithinAbs (0.5f, 1.0e-6f));
}

TEST_CASE ("Abschaltetes Modul wird uebersprungen", "[fx]")
{
    FxChain chain;
    chain.modules.push_back (makeModule ("ott", 1.0f, 1.0f,  true));
    chain.modules.push_back (makeModule ("ott", 1.0f, 0.25f, false));   // aus
    chain.modules.push_back (makeModule ("ott", 1.0f, 0.5f,  true));

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillBuffer (buffer, 1.0f);
    chainProcessor.process (buffer);

    // Modul 2 traegt weder Bearbeitung noch OUT GAIN bei -> 1.0 * 1.0 * 0.5.
    REQUIRE_THAT (buffer.getSample (0, 0), WithinAbs (0.5f, 1.0e-6f));
}

TEST_CASE ("Ketten-Bypass rechnet kein Modul", "[fx]")
{
    FxChain chain;
    chain.modules.push_back (makeModule ("ott", 1.0f, 0.25f));
    chain.bypassAll = true;

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    REQUIRE (chainProcessor.isBypassAll());

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillBuffer (buffer, 1.0f);
    chainProcessor.process (buffer);

    REQUIRE_THAT (buffer.getSample (0, 0), WithinAbs (1.0f, 1.0e-6f));

    // Live umschaltbar.
    chainProcessor.setBypassAll (false);
    chainProcessor.process (buffer);
    REQUIRE_THAT (buffer.getSample (0, 0), WithinAbs (0.25f, 1.0e-6f));
}

TEST_CASE ("MIX blendet trocken und nass", "[fx]")
{
    constexpr float input = 0.25f;

    auto state = makeModule ("ott", 1.0f, 1.0f);
    setAttribute (state, "amount", 200.0);      // nass = 2x Eingang

    FxChain chain;
    chain.modules.push_back (state);

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    const auto render = [&chainProcessor, input] (float mix)
    {
        chainProcessor.setModuleMix (0, mix);

        juce::AudioBuffer<float> buffer (2, kBlockSize);
        fillBuffer (buffer, input);
        chainProcessor.process (buffer);
        return buffer.getSample (0, 0);
    };

    const auto wet = input * 2.0f;

    // mix = 0 -> reines Trockensignal.
    REQUIRE_THAT (render (0.0f), WithinAbs (input, 1.0e-6f));
    // mix = 1 -> reines Nassignal.
    REQUIRE_THAT (render (1.0f), WithinAbs (wet, 1.0e-6f));
    // mix = 0.5 -> lineare Blende.
    REQUIRE_THAT (render (0.5f), WithinAbs (input * 0.5f + wet * 0.5f, 1.0e-6f));

    // Ueber den Kettenzustand derselbe Effekt.
    chainProcessor.setChain (chain);
    juce::AudioBuffer<float> dryBuffer (2, kBlockSize);
    fillBuffer (dryBuffer, input);
    chainProcessor.setModuleMix (0, 0.0f);
    chainProcessor.process (dryBuffer);
    REQUIRE_THAT (dryBuffer.getSample (0, 0), WithinAbs (input, 1.0e-6f));
}

// ---------------------------------------------------------------------------
// Unbekannter Typ
// ---------------------------------------------------------------------------
TEST_CASE ("Unbekannter Modultyp wird uebersprungen", "[fx]")
{
    FxChain chain;
    chain.modules.push_back (makeModule ("ott", 1.0f, 0.5f));
    chain.modules.push_back (makeModule ("quatsch", 1.0f, 0.1f));
    chain.modules.push_back (makeModule ("ott", 1.0f, 0.5f));

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    // Der Platz bleibt erhalten, damit Karten-Indizes stimmen.
    REQUIRE (chainProcessor.numModules() == 3);
    REQUIRE (chainProcessor.modules().size() == 3);
    REQUIRE (chainProcessor.modules()[0] != nullptr);
    REQUIRE (chainProcessor.modules()[1] == nullptr);
    REQUIRE (chainProcessor.modules()[2] != nullptr);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillBuffer (buffer, 1.0f);

    chainProcessor.process (buffer);          // darf nicht abstuerzen

    REQUIRE_THAT (buffer.getSample (0, 0), WithinAbs (0.25f, 1.0e-6f));
    REQUIRE_FALSE (chainProcessor.displayFor (1).valid);
}

// ---------------------------------------------------------------------------
// Parameter aus dem Modulzustand
// ---------------------------------------------------------------------------
TEST_CASE ("Modulattribute aus dem Zustand landen im Modul", "[fx]")
{
    // "ott" ist noch mit dem Platzhalter belegt; hier geht es nur um den Weg
    // Zustand -> Modul, nicht um DSP.
    auto state = makeModule ("ott", 1.0f, 1.0f);
    setAttribute (state, "amount", 50.0);     // 50 % Pegel

    FxChain chain;
    chain.modules.push_back (state);

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    REQUIRE (chainProcessor.modules().size() == 1);
    REQUIRE (chainProcessor.modules()[0] != nullptr);
    REQUIRE (chainProcessor.modules()[0]->type() == FxType::multiband);
    REQUIRE_FALSE (chainProcessor.modules()[0]->params().empty());
    REQUIRE (chainProcessor.modules()[0]->params()[0].id == "amount");
    REQUIRE_THAT (chainProcessor.modules()[0]->getParam (0), WithinAbs (50.0f, 1.0e-4f));

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillBuffer (buffer, 0.5f);
    chainProcessor.process (buffer);

    REQUIRE_THAT (buffer.getSample (0, 0), WithinAbs (0.25f, 1.0e-6f));
}

TEST_CASE ("Modulattribute werden auf den deklarierten Bereich geklemmt", "[fx]")
{
    auto state = makeModule ("ott", 1.0f, 1.0f);
    setAttribute (state, "amount", 900.0);    // ueber dem Maximum (400)

    FxChain chain;
    chain.modules.push_back (state);

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    REQUIRE (chainProcessor.modules().size() == 1);
    REQUIRE (chainProcessor.modules()[0] != nullptr);
    REQUIRE_THAT (chainProcessor.modules()[0]->getParam (0), WithinAbs (400.0f, 1.0e-4f));
}

TEST_CASE ("Echte Module ersetzen die Platzhalter", "[fx]")
{
    // Gebaut und in der Factory registriert sind inzwischen vier Module; die
    // uebrigen fuenf laufen weiter als Platzhalter.
    struct Expectation { const char* typeString; const char* firstParam; };

    for (const auto& expected : { Expectation { "eq",    "lowgain" },
                                  Expectation { "dist",  "drive"   },
                                  Expectation { "trans", "amount"  },
                                  Expectation { "filt",  "type"    } })
    {
        const auto module = FxFactory::get().create (expected.typeString);
        INFO ("Typ " << expected.typeString);
        REQUIRE (module != nullptr);
        REQUIRE_FALSE (module->params().empty());
        REQUIRE (module->params()[0].id == juce::String (expected.firstParam));
    }

    // Noch nicht gebaute Module fallen weiterhin auf den Platzhalter zurueck.
    for (const auto& stillStub : { "mseq", "shaper", "reverb", "final", "ott" })
    {
        const auto module = FxFactory::get().create (stillStub);
        INFO ("Typ " << stillStub);
        REQUIRE (module != nullptr);
        REQUIRE (module->params()[0].id == "amount");
    }
}

// ---------------------------------------------------------------------------
// Latenz
// ---------------------------------------------------------------------------
TEST_CASE ("Latenz wird summiert und beim Ueberspringen gehalten", "[fx]")
{
    // Module, die ihre Latenz nur melden (kein echter DSP).
    const ScopedTypeOverride overrideDist (FxType::distortion,
                                           [] { return std::make_unique<PassThroughModule> (FxType::distortion, 4); });
    const ScopedTypeOverride overrideOtt (FxType::multiband,
                                          [] { return std::make_unique<PassThroughModule> (FxType::multiband, 8); });

    FxChain chain;
    chain.modules.push_back (makeModule ("dist"));    // 4 Samples
    chain.modules.push_back (makeModule ("ott"));     // 8 Samples

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    REQUIRE (chainProcessor.totalLatencySamples() == 12);

    // Modul 2 abschalten: die gemeldete Latenz bleibt (kein PDC-Sprung).
    chainProcessor.setModuleEnabled (1, false);
    REQUIRE (chainProcessor.totalLatencySamples() == 12);

    // Ausschalten darf nichts zerstoeren: kein Absturz, Ausgang bleibt endlich
    // und frei von NaN/Inf. Die exakte Impulsposition haengt an der
    // Latenzstrategie der Kette und wird hier bewusst nicht festgeschrieben.
    juce::AudioBuffer<float> bypassed (2, kBlockSize);
    putImpulse (bypassed);
    chainProcessor.process (bypassed);

    for (int ch = 0; ch < bypassed.getNumChannels(); ++ch)
        for (int n = 0; n < bypassed.getNumSamples(); ++n)
        {
            const auto value = bypassed.getSample (ch, n);
            INFO ("Kanal " << ch << " Sample " << n);
            REQUIRE (std::isfinite (value));
            REQUIRE (std::abs (value) <= 4.0f);
        }

    // Wieder aktiv: das Modul selbst verzoegert (hier nicht), die Kette nicht.
    chainProcessor.setModuleEnabled (1, true);

    juce::AudioBuffer<float> active (2, kBlockSize);
    putImpulse (active);
    chainProcessor.process (active);

    REQUIRE_THAT (active.getSample (0, 0), WithinAbs (1.0f, 1.0e-6f));
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
TEST_CASE ("Display liefert Ein- und Ausgang je Modul", "[fx]")
{
    FxChain chain;
    chain.modules.push_back (makeModule ("ott", 1.0f, 0.5f));

    FxChainProcessor chainProcessor;
    chainProcessor.prepare (kSampleRate, kBlockSize);
    chainProcessor.setChain (chain);

    REQUIRE_FALSE (chainProcessor.displayFor (0).valid);
    REQUIRE_FALSE (chainProcessor.displayFor (-1).valid);
    REQUIRE_FALSE (chainProcessor.displayFor (99).valid);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    fillBuffer (buffer, 0.5f);
    chainProcessor.process (buffer);

    const auto& display = chainProcessor.displayFor (0);

    REQUIRE (display.valid);
    REQUIRE (display.pre.getNumChannels() == FxChainProcessor::kMaxChannels);
    REQUIRE (display.post.getNumChannels() == FxChainProcessor::kMaxChannels);
    REQUIRE (display.pre.getNumSamples() == kBlockSize);
    REQUIRE (display.post.getNumSamples() == kBlockSize);

    // pre = vor dem Modul, post = nach MIX und OUT GAIN.
    REQUIRE_THAT (display.pre.getSample (0, 0),  WithinAbs (0.5f, 1.0e-6f));
    REQUIRE_THAT (display.post.getSample (0, 0), WithinAbs (0.25f, 1.0e-6f));

    // Fenster ist auf 512 Samples begrenzt.
    chainProcessor.prepare (kSampleRate, 1024);
    REQUIRE (chainProcessor.displayFor (0).pre.getNumSamples() == FxChainProcessor::kMaxDisplaySamples);
    REQUIRE_FALSE (chainProcessor.displayFor (0).valid);
}
