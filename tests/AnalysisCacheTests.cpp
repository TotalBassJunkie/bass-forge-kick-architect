/*
 * B-26: Tests fuer den binaeren Analyse-Cache.
 *
 * Geprueft wird der Rundweg (speichern/laden), die Ungueltigkeit bei geaenderter
 * Datei sowie das Verwerfen kaputter Dateien.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <juce_core/juce_core.h>

#include "library/analysis/AnalysisCache.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

TEST_CASE ("Analyse-Cache: speichern und laden", "[library][cache]")
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bfka_analysis_cache_test");
    dir.deleteRecursively();
    dir.createDirectory();
    auto file = dir.getChildFile ("analysis.bin");

    AnalysisCache c (file);
    c.store ("a.wav", 100, 200, { 1, 128.0, 0.01, 0.5, 0.9 });

    REQUIRE (c.save());

    AnalysisCache d (file);
    REQUIRE (d.load());

    auto hit = d.lookup ("a.wav", 100, 200);
    REQUIRE (hit.has_value());
    REQUIRE (hit->keyNote == 1);
    REQUIRE_THAT (hit->bpm, WithinAbs (128.0, 1e-9));
    REQUIRE_THAT (hit->onsetSeconds, WithinAbs (0.01, 1e-9));
    REQUIRE_THAT (hit->lengthSeconds, WithinAbs (0.5, 1e-9));
    REQUIRE_THAT (hit->confidence, WithinAbs (0.9, 1e-9));
}

TEST_CASE ("Analyse-Cache: geaenderte Datei ist ein Miss", "[library][cache]")
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bfka_analysis_cache_test");
    dir.deleteRecursively();
    dir.createDirectory();
    auto file = dir.getChildFile ("analysis.bin");

    AnalysisCache c (file);
    c.store ("a.wav", 100, 200, { 1, 128.0, 0.01, 0.5, 0.9 });
    REQUIRE (c.save());

    AnalysisCache d (file);
    REQUIRE (d.load());

    // Andere Groesse: Eintrag ist veraltet.
    REQUIRE_FALSE (d.lookup ("a.wav", 101, 200).has_value());

    // Andere Aenderungszeit: ebenfalls veraltet.
    REQUIRE_FALSE (d.lookup ("a.wav", 100, 201).has_value());

    // Unveraendert bleibt der Treffer bestehen.
    REQUIRE (d.lookup ("a.wav", 100, 200).has_value());
}

TEST_CASE ("Analyse-Cache: unbekannter Schluessel", "[library][cache]")
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bfka_analysis_cache_test");
    dir.deleteRecursively();
    dir.createDirectory();
    auto file = dir.getChildFile ("analysis.bin");

    AnalysisCache c (file);
    c.store ("a.wav", 100, 200, { 1, 128.0, 0.01, 0.5, 0.9 });
    REQUIRE (c.save());

    AnalysisCache d (file);
    REQUIRE (d.load());

    REQUIRE_FALSE (d.lookup ("b.wav", 100, 200).has_value());
}

TEST_CASE ("Analyse-Cache: kaputte Datei wird verworfen", "[library][cache]")
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bfka_analysis_cache_test");
    dir.deleteRecursively();
    dir.createDirectory();
    auto file = dir.getChildFile ("analysis.bin");

    // Acht Bytes Muell: Kopf ist zu kurz bzw. falsch.
    {
        juce::FileOutputStream out (file);
        REQUIRE (out.openedOk());
        out.writeInt (0x12345678);
        out.writeInt (0x0BADF00D);
    }

    REQUIRE (file.getSize() == 8);

    AnalysisCache d (file);
    REQUIRE_FALSE (d.load());
    REQUIRE (d.size() == 0);
}

TEST_CASE ("Analyse-Cache: clear leert", "[library][cache]")
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bfka_analysis_cache_test");
    dir.deleteRecursively();
    dir.createDirectory();
    auto file = dir.getChildFile ("analysis.bin");

    AnalysisCache d (file);
    d.store ("a.wav", 100, 200, { 1, 128.0, 0.01, 0.5, 0.9 });
    REQUIRE (d.size() == 1);

    d.clear();
    REQUIRE (d.size() == 0);

    REQUIRE (d.save());

    AnalysisCache reloaded (file);
    REQUIRE (reloaded.load());
    REQUIRE (reloaded.size() == 0);
}

TEST_CASE ("Analyse-Cache: mehrere Eintraege ueberleben den Rundweg", "[library][cache]")
{
    juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bfka_analysis_cache_test");
    dir.deleteRecursively();
    dir.createDirectory();
    auto file = dir.getChildFile ("analysis.bin");

    AnalysisCache c (file);
    c.store ("a.wav", 100, 200, { 1, 128.0, 0.01, 0.5, 0.9 });
    c.store ("b.wav", 200, 300, { 5, 140.0, 0.02, 0.25, 0.5 });
    c.store ("c.wav", 300, 400, { 11, 174.0, 0.03, 1.5, 0.1 });
    REQUIRE (c.size() == 3);

    REQUIRE (c.save());

    AnalysisCache d (file);
    REQUIRE (d.load());
    REQUIRE (d.size() == 3);

    auto second = d.lookup ("b.wav", 200, 300);
    REQUIRE (second.has_value());
    REQUIRE (second->keyNote == 5);
    REQUIRE_THAT (second->bpm, WithinAbs (140.0, 1e-9));

    auto third = d.lookup ("c.wav", 300, 400);
    REQUIRE (third.has_value());
    REQUIRE (third->keyNote == 11);
    REQUIRE_THAT (third->lengthSeconds, WithinAbs (1.5, 1e-9));

    // Nicht gespeicherte Schluessel bleiben Miss.
    REQUIRE_FALSE (d.lookup ("d.wav", 400, 500).has_value());
}
