# Fortschritt

Laufendes Protokoll. Neueste Einträge oben. Format:
`## JJJJ-MM-TT — <Welle/Phase>` mit erledigten Tickets (B-Nummern) und Notizen.

---

## 2026-11-17 — Welle 3: Engine, Stretching und Randomizer im Build

**Erledigt**

* **Sample-Engine (P3)**: `src/engine/SampleVoice.*` (Position, Trim, fünf Fade-Kurven,
  Declick-Rampe, monotone Ausgabe) und `src/engine/KickRenderer.*` (12 Slots, Solo-Logik,
  `KICK LENGTH` 1 Beat vs. Full Sample, Auto-Gain über das Body-Fenster 20–120 ms).
* **Stretching (P4)**: `src/engine/WsolaStretcher.*` — eigene WSOLA-Implementierung
  (Hann-Fenster, Grain-Suche per normalisierter Kreuzkorrelation, OLA-Normalisierung,
  kanalgekoppelte Suche gegen Phasendrift im Bass) plus `REPITCH`-Pfad und
  `src/engine/Resampler.h`. **Lizenzrein, keine Fremdbibliothek.**
* **Randomizer (P7)**: `src/library/StyleModel.*` (JSON-Parser für `genres.json`:
  25 Pools, 10 Styles, Rollen, Tonarten, Gewichte) und `src/library/RandomizerEngine.*`
  (Pflicht-/Optionalteile, gewichtete Pool-Wahl, `shares`, Pegelbereiche,
  Ausschlussregel „melodic × atonal", deterministisch über Seed).
* **Tests: 112 Testfälle, 384.903 Assertions, alle grün.** Build grün.
* **CI:** erster vollständig grüner Durchlauf auf GitHub Actions (7 min 16 s),
  Artefakt-Upload funktioniert.

**Gates:** Build grün · Tests grün · `origin/main` aktuell · CI success.

**Nächste Schritte**

1. Pitch/Stretch in die Voice einhängen (B-44) — die Schnittstellen stehen.
2. Die neun FX-Module implementieren (größter Parallelisierungssprung).
3. Danach UI-Seiten (P8/P9).

---

## 2026-11-17 — Welle 2: Parameter-Modell korrigiert, Library integriert

**Erledigt**

* **Preset-Datenauswertung abgeschlossen** (44 Kicks, 76 FX-Chains, 639 Modulinstanzen).
  Acht Befunde mit Zahlen, drei davon ändern das Datenmodell:
  * **Parameterzahl 352 → 364.** Es gibt einen bisher unbekannten 20. Slot-Parameter
    `lN_fistart` (Fade-In-Anker), nachweisbar im neuesten Werkpreset.
  * **Stretch-Kurve korrigiert.** Logarithmus ist widerlegt: 1,0× wird als
    `0.374989420` gespeichert, Log ergäbe 0,4. Eine **Potenzkurve mit Exponent 0,42**
    trifft den Messwert exakt. Test dafür ist drin.
  * **FX-Parameterbrücke geklärt:** `p12 = MIX`, `p13 = outGain / 2`,
    `p0…p11` = Modulattribute in XML-Reihenfolge; Bänder/Kurven liegen **nicht** in den
    Proxy-Parametern, sondern nur im `<FxChain>`-Snapshot. Werte sind 4-stellig quantisiert.
  * Ebenfalls belegt: `root_note = MIDI-Note / 127`.
* **Sample-Library-Index integriert** (`src/library/SampleLibrary.*`): Ordner→Rollen
  inklusive der beiden Sonderfälle (`Main - Crunches/Industrial Heavy` ist eine Rumble,
  `Enhancers (Tops)` ist TOP), BPM- und Tonart-Erkennung aus Dateinamen.
  Verifiziert gegen die echte Library: **2.443 Dateien in 1,43 s**, BPM-Verteilung
  2.246× 150 / 60× 155 / 137× 160, 363 Dateien mit Tonart.
* **Tests: 38 Testfälle, 3.571 Assertions, alle grün.** Build grün.

**Gates:** lokaler Build grün · Tests grün · CI beim Push ausgelöst.

**Läuft parallel:** Engine-Core (Sample-Voice, Fades, Summe, Auto-Gain) und
WSOLA-Stretcher; StyleModel/Randomizer.

---

## 2026-11-17 — Welle 1: FX-Gerüst integriert, Repo live

**Erledigt**

* **GitHub-Repo angelegt und gepusht:** <https://github.com/TotalBassJunkie/bass-forge-kick-architect> (public).
  CI-Workflow `Build` läuft bei jedem Push.
* **FX-Ketten-Framework** (B-69) von Grund auf vorhanden und **im Build integriert**:
  `src/fx/FxModule.h` (Schnittstelle, `FxType`, Parameterbeschreibung),
  `src/fx/FxFactory.*` (Registry der neun Modultypen), `src/fx/PassThroughModule.h`
  (Test-Platzhalter), `src/fx/FxChainProcessor.*` (Kette, MIX-Blende, OUT-GAIN,
  Bypass, Latenzausgleich, Display-Puffer je Modul).
* **Tests: 22 Testfälle, 3.407 Assertions, alle grün** — inklusive FX-Kette
  (Reihenfolge über OUT-GAIN, MIX 0/0,5/1, unbekannter Modultyp, Bypass, Latenz-Summe).
* Library-Audioformate vollständig erfasst (siehe Bestandsaufnahme 2.2.1):
  die 2.443 WAVs sind gemischt 16/24-bit-PCM und 32-bit-Float, überwiegend
  44,1 kHz stereo, dazu 32 mono-IRs und 8 Dateien mit 48 kHz.

**Gates:** Build grün · Tests grün · Remote `origin/main` aktuell.

**Nächste Schritte**

1. Wellen 1 einmischen: StyleModel/Randomizer, SampleLibrary (laufen).
2. Danach P3 (Engine-Core) und P4 (WSOLA) als Kernstück.

---

## 2026-11-17 — Welle 0/1: Fundament gebaut

**Erledigt (P0, B-00…B-06)**

* Git-Repo initialisiert (Branch `main`), zwei Commits.
* Toolchain verifiziert: MSVC 19.51 (VS BuildTools 2026), CMake 4.3.1, Ninja 1.13.2.
* `CMakeLists.txt` mit `juce_add_plugin`: Ziele **VST3 + Standalone**, JUCE **8.0.7**
  (FetchContent bzw. lokaler Baum über `BFKA_JUCE_SOURCE_DIR`), Catch2 v3.7.1 für Tests.
* **Build grün**: `Bass Forge Kick Architect.vst3`, `Bass Forge Kick Architect.exe`
  (Standalone), `BFKA_Tests.exe`.
* **Tests grün**: 11 Testfälle, 2.759 Assertions.
* Code-Stand:
  * `src/core/Version` — Produkt-Metadaten (`BFKA`, `.bfkapreset`, Bundle-ID).
  * `src/core/Log` — Logging ohne Audio-Thread-Nutzung.
  * `src/core/ParameterRegistry` — **352 Parameter** (12×19 Slot + 12 Global + 8×14 FX-Proxy)
    mit Kurven, Bereichen, Defaults, Formatierung.
  * `src/core/KickState` — Zustand inkl. Slots/Rollen/Locks/FX-Kette und ValueTree-Serialisierung.
  * `src/plugin/PluginProcessor` + `PluginEditor` — lauffähiges Skelett, 352 Host-Parameter,
    Zustandsspeicherung, minimaler Editor.
* Helfer: `tools/dev-env.ps1` (findet VS/CMake/Ninja selbst), `configure.ps1`, `build.ps1`, `test.ps1`.
* CI: `.github/workflows/build.yml` — Windows-Build, Tests, Artefakt-Upload, Release bei Tag.
* Spezifikation `docs/04-SPEC-STATE-AND-PRESET-FORMAT.md` (Zustandsbaum, Preset-Format,
  Import-Mapping des Referenzformats).

**Gates:** Build grün (Exitcode 0) · `BFKA_Tests` grün · Konfiguration reproduzierbar
über `tools\configure.ps1`.

**Erkenntnisse aus der Datenauswertung (Zwischenstand)**

* `root_note = 0.283465` → das ist **36/127**, also MIDI-Note 36 (C2). Bestätigt die
  Normalisierung `note/127`.
* `master_vol` liegt bei 43 von 44 Presets exakt auf `0.833333` = **5/6**.
* `stretch` liegt bei fast allen Presets auf `0.374989` — also ist 1,0× **nicht** 0,5
  (Arbeitsannahme: logarithmische Abbildung 0,25×…8× → 1,0× entspricht ≈ 0,40).
* `filter` = 1.0 und `filterq` = 0.102564 konstant → Slot-Filter ist in Werk-Presets
  durchgehend offen; `0.102564` ist vermutlich `4/39`.

**Nächste Schritte**

1. GitHub-Repo anlegen und pushen (wartet auf Zugang).
2. Welle 1 einmischen: StyleModel/Randomizer, SampleLibrary, FX-Ketten-Framework.
3. Danach P3 (Engine-Core) und P4 (WSOLA) als Kernstück.

---

## 2026-11-17 — Kickoff: Bestandsaufnahme & Plan

**Erledigt**

* Vollständige Inventur der installierten Referenz (Programme, Content, Bibliothek,
  Presets, Analysedaten) — Hashes, Größen, Pfade, Zahlen.
* Handbuch (37 Seiten) ausgewertet, Signalfluss und Bedienlogik dokumentiert.
* Offene Datenformate gelesen und dokumentiert: Kick-Preset-XML, FX-Chain-XML,
  `genres.json` (25 Pools / 10 Styles), `prefs.json`, Analyse-Container.
* Binär-Statik: Framework (JUCE 8.0.7), VST3-SDK 3.7.12, Renderer, Imports,
  Modul-/Klassenlandkarte, Stretch-Algorithmusfamilie (WSOLA).
* Toolchain geprüft: MSVC 19.51 (VS BuildTools 2026), CMake 4.3.1, Ninja 1.13.2.

**Dokumente**

* [`01-BESTANDSAUFNAHME.md`](01-BESTANDSAUFNAHME.md) — Ist-Zustand, Datenformate,
  Parameterinventar, offene Fragen, Leitplanken.
* [`02-MASTERPLAN.md`](02-MASTERPLAN.md) — Phasen P0–P14, ~85 Tickets (B-00…B-135),
  Subagenten-Routing, Abnahmekriterien, Risiken, Meilensteine.
* [`03-RE-FINDINGS.md`](03-RE-FINDINGS.md) — Methodik und Befundlogbuch mit
  Quellenmarkierung.

**Entscheidungen**

* Produktname: **Bass Forge Kick Architect**, Code `BFKA`.
* Stack: C++20 + JUCE 8 (Version gepinnt), CMake, VST3 + Standalone, Windows zuerst.
* Kein Rubber Band (lizenzpflichtig) — eigene WSOLA-Engine.
* Kein Lizenzsystem, kein fremder Sample-Content im Repo.
* RE-Artefakte (String-/Klassenauszüge, Handbuchtext) bleiben lokal in `_private/`
  und werden **nicht** veröffentlicht.

**Nächste Schritte**

1. Repo-Gerüst + CI (P0, B-00…B-06).
2. Verträge schreiben: `KickState`, `ParameterRegistry`, `FxChain`, `Library`,
   `Engine`, `FxModule`-Interface.
3. Welle 1 parallel: Fundament, Preset-IO, Library-Index, Testdaten, Parametertabelle.

---

<!-- Vorlage für neue Einträge:

## JJJJ-MM-TT — Welle N

**Erledigt:** B-xx, B-yy; je ein Satz Ergebnis + Link auf PR/Commit.

**Gates:** Build grün, `ctest` grün, CI-Artefakt vorhanden.

**Blocker:** …

**Nächste Schritte:** …

-->
