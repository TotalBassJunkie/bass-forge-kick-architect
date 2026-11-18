// B-76: Tests fuer eigene Styles (`.bfkastyle`) und die Ordner-Verwaltung.
#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include "core/StyleIO.h"
#include "library/UserStyles.h"

using namespace bfka;

namespace
{
    /** Frisches Arbeitsverzeichnis je Test (wird am Ende geloescht). */
    struct TempDirectory
    {
        juce::File directory;

        TempDirectory()
        {
            directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("bfka-style-tests")
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

    UserStyle makeStyle (const juce::String& id, const juce::String& name)
    {
        UserStyle style;
        style.id   = id;
        style.name = name;
        return style;
    }
} // namespace

TEST_CASE ("Style: Rundweg ueber Datei", "[core][style]")
{
    TempDirectory temp;

    UserStyle s;
    s.id            = "user-test";
    s.name          = "Test";
    s.basedOn       = "rawstyle";
    s.accentColor   = "#ff8800";
    s.bothLibraries = true;
    s.freeBuild     = true;
    s.notes         = "handmade";

    juce::String error;
    const auto file = temp.file ("user-test");

    REQUIRE (StyleIO::save (file, s, error));
    INFO (error);

    // Endung wurde ergaenzt.
    const auto saved = temp.file ("user-test.bfkastyle");
    REQUIRE (saved.existsAsFile());
    CHECK (saved.getFileExtension() == ".bfkastyle");

    auto loaded = StyleIO::load (saved, error);
    REQUIRE (loaded.has_value());
    INFO (error);

    REQUIRE (loaded->id == "user-test");
    REQUIRE (loaded->name == "Test");
    CHECK (loaded->basedOn == "rawstyle");
    CHECK (loaded->accentColor == "#ff8800");
    REQUIRE (loaded->bothLibraries);
    REQUIRE (loaded->freeBuild);
    REQUIRE (loaded->notes == "handmade");
}

TEST_CASE ("Style: ungueltige Datei wird abgelehnt", "[core][style]")
{
    TempDirectory temp;

    juce::String error;
    const auto file = temp.file ("kaputt.bfkastyle");

    REQUIRE (file.replaceWithText ("kaputt"));

    REQUIRE_FALSE (StyleIO::load (file, error).has_value());
    REQUIRE (error.isNotEmpty());

    // Fehlende Datei und falsche Wurzel ebenso.
    CHECK_FALSE (StyleIO::load (temp.file ("fehlt.bfkastyle"), error).has_value());

    const auto wrongRoot = temp.file ("falsch.bfkastyle");
    REQUIRE (wrongRoot.replaceWithText ("<EtwasAnderes id=\"a\" name=\"b\"/>"));

    CHECK_FALSE (StyleIO::load (wrongRoot, error).has_value());
    CHECK (error.isNotEmpty());
}

TEST_CASE ("Style: ungueltiger Style wird nicht gespeichert", "[core][style]")
{
    TempDirectory temp;

    UserStyle leer;
    juce::String error;

    REQUIRE_FALSE (StyleIO::save (temp.file ("leer.bfkastyle"), leer, error));
    CHECK (error.isNotEmpty());
    CHECK_FALSE (temp.file ("leer.bfkastyle").existsAsFile());
}

TEST_CASE ("Styles: Ordner-Verwaltung", "[core][style]")
{
    TempDirectory temp;

    juce::String error;
    UserStyles us (temp.directory);

    REQUIRE (us.scan() == 0);
    CHECK (us.size() == 0);

    UserStyle a;
    a.id   = "a";
    a.name = "Alpha";

    REQUIRE (us.addOrReplace (a, error));
    INFO (error);
    REQUIRE (us.size() == 1);
    REQUIRE (us.find ("a") != nullptr);
    CHECK (us.find ("a")->name == "Alpha");
    CHECK (temp.file ("a.bfkastyle").existsAsFile());

    // Ersetzen haelt die Anzahl.
    a.name = "Alpha 2";
    REQUIRE (us.addOrReplace (a, error));
    REQUIRE (us.size() == 1);
    CHECK (us.find ("a")->name == "Alpha 2");

    REQUIRE (us.remove ("a", error));
    INFO (error);
    REQUIRE (us.size() == 0);
    CHECK_FALSE (temp.file ("a.bfkastyle").existsAsFile());
}

TEST_CASE ("Styles: scan liest vorhandene Dateien", "[core][style]")
{
    TempDirectory temp;

    juce::String error;
    REQUIRE (StyleIO::save (temp.file ("b.bfkastyle"), makeStyle ("b", "Beta"), error));
    REQUIRE (StyleIO::save (temp.file ("a.bfkastyle"), makeStyle ("a", "Alpha"), error));

    // Eine kaputte Datei stoert den Scan nicht.
    REQUIRE (temp.file ("kaputt.bfkastyle").replaceWithText ("kaputt"));

    UserStyles us (temp.directory);
    REQUIRE (us.scan() == 2);

    // Nach Namen sortiert.
    REQUIRE (us.entries().size() == 2);
    CHECK (us.entries()[0].name == "Alpha");
    CHECK (us.entries()[1].name == "Beta");
}

TEST_CASE ("Styles: remove ohne Eintrag meldet Fehler", "[core][style]")
{
    TempDirectory temp;

    juce::String error;
    UserStyles us (temp.directory);

    REQUIRE (us.scan() == 0);
    REQUIRE_FALSE (us.remove ("gibtsnicht", error));
    CHECK (error.isNotEmpty());
}
