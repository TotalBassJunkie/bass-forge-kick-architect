# Fortschritt

Laufendes Protokoll. Neueste Einträge oben. Format:
`## JJJJ-MM-TT — <Welle/Phase>` mit erledigten Tickets (B-Nummern) und Notizen.

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
