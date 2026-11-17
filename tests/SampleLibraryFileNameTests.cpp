#include <catch2/catch_test_macros.hpp>

#include "core/KickState.h"
#include "library/SampleLibrary.h"

using namespace bfka;

namespace
{
    // Feste Oktave des Index: C=0, C#=1, D=2, D#=3, E=4, F=5,
    // F#=6, G=7, G#=8, A=9, A#=10, B=11 - keine Oktavlage, nur die Tonklasse.
    constexpr int kCSharp = 1;
    constexpr int kE      = 4;
    constexpr int kFSharp = 6;
    constexpr int kGSharp = 8;
    constexpr int kA      = 9;
    constexpr int kASharp = 10;
    constexpr int kB      = 11;
}

// ---------------------------------------------------------------------------
// Tempo
// ---------------------------------------------------------------------------

TEST_CASE ("Tempo steht in der letzten Zahl vor der Endung", "[library][filenames]")
{
    REQUIRE (SampleLibrary::kDefaultBpm == 150.0);

    REQUIRE (SampleLibrary::parseBpmFromFileName ("Punch Zaag 12 A 150.wav")            == 150.0);
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Reverb Industrial 1 155.wav")         == 155.0);
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Crunch Industrial 101 160.wav")       == 160.0);
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Transient Click 1 150.wav")           == 150.0);
}

TEST_CASE ("Reine Indexnamen liefern das Fallback-Tempo 150", "[library][filenames]")
{
    // Diese Namen tragen nur eine laufende Nummer, kein Tempo.
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Punch Top 3.wav")                     == 150.0);
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Enhancer Newstyle Top 10 E.wav")      == 150.0);
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Punch Clean 100.wav")                 == 150.0);

    // Fallstrick aus der echten Library: die Indexnummer liegt im Tempo-Zahlenraum.
    // "Punch Techno 160.wav" ist Index 160 im Techno-Pool, kein 160-BPM-Sample.
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Punch Techno 160.wav")                == 150.0);
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Punch Techno 155.wav")                == 150.0);
}

TEST_CASE ("Unplausible Tempi werden verworfen", "[library][filenames]")
{
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Punch Top 1 12.wav")                  == 150.0);
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Punch Top 1 400.wav")                 == 150.0);
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Ohne Zahlen.wav")                     == 150.0);
}

// ---------------------------------------------------------------------------
// Grundton
// ---------------------------------------------------------------------------

TEST_CASE ("Grundton aus dem Dateinamen", "[library][filenames]")
{
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Crunch Zaag 10 G# 150.wav")           == kGSharp);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Punch Zaag 12 A 150.wav")             == kA);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Enhancer Newstyle Top 10 E.wav")      == kE);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Crunch Rawstyle 11 B 150.wav")        == kB);
}

TEST_CASE ("Namen ohne Noten-Token liefern -1", "[library][filenames]")
{
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Transient Click 1 150.wav")           == -1);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Reverb Industrial 1 150.wav")         == -1);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Enhancer Low Mid 7 150.wav")          == -1);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Punch Clean 100.wav")                 == -1);

    // Melodischer Pool, aber der Grundton fehlt im Namen (3 Dateien in der Library):
    // den muss die Analyse (B-24) liefern.
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Crunch Rawstyle 33 150.wav")          == -1);
}

TEST_CASE ("Vorzeichen # und b", "[library][filenames]")
{
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Crunch Psy 1 C# 150.wav")             == kCSharp);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Punch Zaag 13 A# 150.wav")            == kASharp);
    // b = erniedrigt: Db ist Cis, Bb ist Ais.
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Crunch Psy 1 Db 150.wav")             == kCSharp);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Crunch Psy 1 Bb 150.wav")             == kASharp);
}

// ---------------------------------------------------------------------------
// Gross-/Kleinschreibung und Namensvarianten
// ---------------------------------------------------------------------------

TEST_CASE ("Gross-/Kleinschreibung ist egal", "[library][filenames]")
{
    REQUIRE (SampleLibrary::parseBpmFromFileName ("punch zaag 12 a 150.wav")             == 150.0);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("punch zaag 12 a 150.wav")             == kA);

    REQUIRE (SampleLibrary::parseBpmFromFileName ("CRUNCH ZAAG 10 g# 155.WAV")           == 155.0);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("CRUNCH ZAAG 10 g# 155.WAV")           == kGSharp);
}

TEST_CASE ("Name ohne Endung und ohne Ordner funktioniert", "[library][filenames]")
{
    REQUIRE (SampleLibrary::parseBpmFromFileName ("Punch Zaag 12 A 150")                 == 150.0);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("Punch Zaag 12 A 150")                 == kA);
}

TEST_CASE ("Pfad vor dem Dateinamen wird ignoriert", "[library][filenames]")
{
    REQUIRE (SampleLibrary::parseBpmFromFileName ("C:\\Library\\Main - Punches\\Zaag\\Punch Zaag 12 A 150.wav")
             == 150.0);
    REQUIRE (SampleLibrary::parseKeyFromFileName ("C:\\Library\\Main - Punches\\Zaag\\Punch Zaag 12 A 150.wav")
             == kA);

    REQUIRE (SampleLibrary::parseKeyFromFileName ("Main - Crunches/Psy Style (Melodic)/Crunch Psy 3 F# 155.wav")
             == kFSharp);
}

// ---------------------------------------------------------------------------
// Ordner -> Rolle
// ---------------------------------------------------------------------------

TEST_CASE ("Ordner werden auf die Rollen abgebildet", "[library][folders]")
{
    REQUIRE (SampleLibrary::roleForFolder ("Main - Punches/Zaag")                  == Role::punch);
    REQUIRE (SampleLibrary::roleForFolder ("Main - Crunches/Zaag (Melodic)")       == Role::crunch);
    REQUIRE (SampleLibrary::roleForFolder ("Main - Rumbles/Schranz")               == Role::rumble);

    // Sonderfall aus genres.json: liegt unter "Crunches", ist aber eine Rumble.
    REQUIRE (SampleLibrary::roleForFolder ("Main - Crunches/Industrial Heavy")     == Role::rumble);

    // Transienten und Enhancer
    REQUIRE (SampleLibrary::roleForFolder ("Transients (Clicks)")                  == Role::click);
    REQUIRE (SampleLibrary::roleForFolder ("Transients (Clicks) [Zaag]")           == Role::click);
    REQUIRE (SampleLibrary::roleForFolder ("Transients (Percs)")                   == Role::punchEnhancer);
    REQUIRE (SampleLibrary::roleForFolder ("Transients (Punch Top)")               == Role::punchEnhancer);
    REQUIRE (SampleLibrary::roleForFolder ("Transients (Newstyle Tops)")           == Role::top);
    REQUIRE (SampleLibrary::roleForFolder ("Enhancers (Rumbles)")                  == Role::rumbleEnhancer);
    REQUIRE (SampleLibrary::roleForFolder ("Enhancers (Schranz)")                  == Role::schranzEnhancer);
    REQUIRE (SampleLibrary::roleForFolder ("Enhancers (Tops)")                     == Role::top);
}

TEST_CASE ("Laengster Praefix gewinnt", "[library][folders]")
{
    // "Techno" darf "Techno (Heavy)" nicht schlucken.
    REQUIRE (SampleLibrary::roleForFolder ("Main - Punches/Techno (Heavy)")        == Role::punch);
    REQUIRE (SampleLibrary::roleForFolder ("Enhancers (Punch Reverbs)/Reverbs (Industrial)")
             == Role::punchEnhancer);
}

TEST_CASE ("Unbekannte Ordner haben die Rolle none", "[library][folders]")
{
    REQUIRE (SampleLibrary::roleForFolder ("Voellig/Unbekannt")                   == Role::none);
    REQUIRE (SampleLibrary::definitionForFolder ("Voellig/Unbekannt")             == nullptr);

    // Impulsantworten sind indexiert, aber kein Kick-Pool: bekannt, Rolle none.
    REQUIRE (SampleLibrary::roleForFolder ("Impulses/Reverbs")                    == Role::none);
    REQUIRE (SampleLibrary::definitionForFolder ("Impulses/Reverbs")              != nullptr);
}

TEST_CASE ("Ordner-Vergleich ist case-insensitiv und toleriert Backslashes", "[library][folders]")
{
    REQUIRE (SampleLibrary::roleForFolder ("main - punches/zaag")                 == Role::punch);
    REQUIRE (SampleLibrary::roleForFolder ("Main - Punches\\Zaag")                == Role::punch);
}

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------

TEST_CASE ("Oktav-Offset wird erst beim Rendern addiert", "[library][key]")
{
    REQUIRE (SampleLibrary::kKeyNoteC == 0);
    REQUIRE (SampleLibrary::withOctave (kA, 0)   == kA);
    REQUIRE (SampleLibrary::withOctave (kA, 4)   == kA + 48);
    REQUIRE (SampleLibrary::withOctave (-1, 4)   == -1);
}

TEST_CASE ("Content-Root-Kandidaten sind definiert", "[library][paths]")
{
    const auto candidates = contentRootCandidates();
    REQUIRE (candidates.size() == 3);
    REQUIRE (candidates.front().getFullPathName().contains ("BassForge"));

    // defaultContentRoot() liefert immer einen Pfad - notfalls den anzulegenden.
    REQUIRE (defaultContentRoot().getFullPathName().isNotEmpty());
}
