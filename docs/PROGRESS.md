# Fortschritt

Laufendes Protokoll. Neueste Einträge oben. Format:
`## JJJJ-MM-TT — <Welle/Phase>` mit erledigten Tickets (B-Nummern) und Notizen.

---

## 2026-11-17 — Welle 6: FX-Integration von OTT, TDR Nova und Convology XT

**Erledigt (Phase 2, B-140…B-142)**

* **Multiband (Xfer OTT 1.37)** — `src/fx/modules/MultibandModule.*`:
  3-Band-Up/Down-Kompressor mit Depth, Time (gegenläufig Attack/Release),
  In/Gain, Clean-Crossover (LR4/LR8), Thresholds je Band, Band-Input-Gains,
  Bypass Aufwärts/Abwärts je Band sowie linearphasigem FIR-Split (513 Taps,
  256 Samples Latenz). Parameterinventar aus der VST3-Binary
  (`In Gain`, `Clean XOV`, `Thresh L/M/H`, `Byp Upwd/Dnwd L/M/H`,
  `Upwd/Dnwd Strgth`) — die Referenz-Attribute bleiben auf `p0…p7`.
* **Convolver (Convology XT 1.34)** — `src/fx/modules/ConvolverModule.*`:
  IR-Laden (WAV/AIF, 1/2/4 Kanäle inkl. True Stereo), Time-Hüllkurve
  (Start/Attack/Hold/Release), EQ-Kurve und frequenzabhängige Decay-Kurve
  (je 11 Punkte), Normalize, IR-Gain, Reverse, Mono-Input, Stereo-Modulator
  (90°-Quadratur), Decay-Skalierung, Stretch (Resampling), Predelay,
  Width 0…200 %, Zero-Latency-Kopf oder feste 2048 Samples Latenz.
  Faltung über `juce::dsp::Convolution`; IR-Aufbereitung auf dem
  Message-Thread, deterministisch per `prepare()` in die Engine übernommen.
* **Dynamic EQ (TDR Nova)** — `src/fx/modules/DynamicEqModule.*`, **neuer
  Modultyp `dynEq`**: 4 Bänder (Low-Shelf/Bell/High-Shelf) mit je
  Threshold/Ratio/Attack/Release, SPLIT/STICKY, HP/LP mit 6/12/24/72 dB/Okt,
  W-Band mit eigener Dynamik, Dry Mix, Output Gain, EQ Auto Gain, Delta,
  sieben Prozessziele (Mono/Stereo/Sum/Diff/Left/Right/Dual Mono).
  Der bestehende Referenz-`eq` bleibt unverändert (Parität der 44 Presets).
* **`FxChainProcessor::setChain()`** übergibt jetzt den Struktur-Zustand über
  `module->setState (state.data)` — damit erreichen EQ-Bänder, Kurven und
  IR-Pfade die Module (schließt O5 aus der Ketten-Engine-Spec).
* **Doku:** `docs/specs/fx-multiband.md`, `fx-convolver.md`,
  `fx-dynamic-eq.md`; Ketten-Engine-Spec auf zehn Typen aktualisiert.
* **Tests: 212 Testfälle, 491.404 Assertions, alle grün.** Neu: 36 Tests für
  die drei Module (Impuls-/Sinus-/Hüllkurvenmessungen, True-Stereo-Routing,
  Zustands-Roundtrip, Kettenintegration OTT → Convolver → Dynamic EQ).
  Baseline vor der Änderung: 180 Testfälle / 480.817 Assertions.

**RE-Provenienz (nur lesend, nichts im Repo)**

* Installer aus `BF Kick Forge Plugins for FX` entpackt bzw. installiert
  (Analysis-Kopien unter `C:\bfka-re\`): OTT (NSIS, VST3 + AAX),
  Convology XT (NSIS, VST3/AAX/VST2 + Factory Library + **Handbuch-PDF**),
  TDR Nova (Inno Setup, VST3 x64 + x86).
* Parameterinventare aus den Bezeichner-Strings der VST3-Binaries;
  Bediensemantik aus dem Convology-Handbuch und der öffentlichen
  TDR-Nova-/OTT-Dokumentation. Keine Standalone-Versionen vorhanden.
* Keine IRs, Presets, Assets oder Strings der Fremd-Plugins wurden übernommen.

**Bewusste Abweichungen (dokumentiert in den Specs):** serielle statt parallele
Nova-Struktur, keine Factory-IRs, kein externer Sidechain, OTT-`x1`-Bereich
20…1000 Hz, Quality-Modus ohne Oversampling-Unterschied.

**Gates:** Build grün (VST3 + Standalone) · Suite grün · Dokumentation aktualisiert.

**Nächste Schritte**

1. UI (P9): Karten und Graphen für die neuen Module (OTT-Crossover,
   Convolver-Kurven, Dynamic-EQ-Bänder).
2. Verbleibende Referenzmodule: `mseq`, `shaper`, `final`.
3. Modul-Presets (`getState()`-Rückschreiben in den Kick-Zustand, O6).

---

## 2026-11-17 — Welle 5: Der erste hörbare Kick

**Erledigt**

* **Engine und FX-Kette sind im Plugin verdrahtet.** `PluginProcessor::processBlock`
  rendert jetzt: Kick-Summe → FX-Kette → Master. Jede MIDI-Note-On startet den Kick neu.
* **`src/engine/KickAssembler.*`**: übersetzt die normalisierten Slot-Parameter in
  `VoiceSettings` (dB→linear, Prozent→Sekunden, `ficurve`→Kurvenform), löst Sample-Pfade
  relativ zur Library auf und hält die Audiodaten am Leben. Zusätzlich baut
  `buildDemoKick()` einen Test-Kick aus Punch, Rumble und Click der Library.
* **Editor**: Buttons „Demo-Kick laden" und „Play" plus Statuszeile — damit ist der
  erste Kick im Standalone hörbar.
* **Filters-Modul selbst geschrieben** (`src/fx/modules/FiltersModule.*`): LP/HP mit
  echter Butterworth-Kaskade (1–5 Pole = 6/12/18/24/30 dB/Oktave), BAND 12/24,
  NOTCH (Kerbe > 100 dB), PHASER mit 1–6 gestaffelten Allpass-Kerben, COMB,
  DRIVE-Sättigung vor dem Filter. Messwerte: LP 12 dB/Okt dämpft eine Oktave über
  CUTOFF um 12 dB, 24 dB/Okt um 24 dB; Durchlass −0,001 dB.
* **Build-Verzeichnis aus Dropbox verlegt** (`C:\bfka-build`). Ursache der sporadischen
  `LNK1104`/`LNK1201`-Fehler war der Dropbox-Sync, der Dateien mitten im Linken sperrte.
  `tools/dev-env.ps1` setzt `BFKA_BUILD_ROOT` automatisch.
* **Tests: 180 Testfälle, 480.817 Assertions, alle grün.** Neu: 11 Filter-Tests und
  4 Integrations-Tests, die einen Kick durch die Engine rendern und prüfen, dass Ton
  herauskommt (Spitzenwert ≈ 0,82) und nach dem Beat Stille folgt.

**Gates:** Build grün (VST3 + Standalone) · Suite grün · Artefakte vorhanden.

**Nächste Schritte**

1. Die übrigen fünf Module: Convolver, Finalizer, Multiband, Mid/Side-EQ, Kick Shaper.
2. Danach KICK-UI (P8) mit Visualiser und Slot-Karten.

---

## 2026-11-17 — Welle 4: erste echte FX-Module im Build

**Erledigt**

* **Equalizer** (`src/fx/modules/EqualizerModule.*`): 15-stufige Biquad-Kaskade, RBJ-Formeln
  für BELL/L-SHF/H-SHF/L-CUT/H-CUT/NOTCH, 12 Nutzerbänder über `setState()/getState()`,
  dynamische Gegenregelung (max. 6 dB), 20 ms Glättung, lock-freier Band-Snapshot.
  Werkszustand ist bitidentisches Durchreichen (flache Bänder werden übersprungen).
* **Distortion** (`src/fx/modules/DistortionModule.*`): zehn Modi, AGC,
  PRE/POST-Tilt, 2×-Oversampling, 0-dBFS-Ceiling, Waveshaper-Kurve als `WNode`-Liste.
* **Transient Shaper** (`src/fx/modules/TransientShaperModule.*`): dualer Hüllkurvenfolger,
  ATTACK/DUCK/SPEED, Soft-Limiter; `amount == 0` ist bitgenau.
* Alle drei sind in `FxFactory` registriert; die übrigen sechs laufen weiter als Platzhalter.
* **Tests: 167 Testfälle, 476.736 Assertions.** Stand: 166 grün, 1 als **bekannter Fehler**
  markiert (`[!mayfail]`) — siehe unten.

**Bekannter Fehler (offen)**

* `DistortionModule`: bei `drive = 0` ist der Ausgang rund **3,5 dB zu leise**
  (Soll: reine Verzögerung). Verdacht: Ringpuffer in `copyDelayed()`. Ticket **B-63b**,
  Fix-Auftrag läuft; der Test bleibt im Bestand und ist nur als „may fail" markiert.

**Gates:** Build grün · Suite grün (1 erwarteter Fehlschlag) · Push erfolgt.

**Nächste Schritte**

1. Filters-Modul (Neuauftrag läuft), danach Convolver, Finalizer, Multiband, Mid/Side-EQ,
   Kick Shaper.
2. Danach Engine + FX-Kette im `PluginProcessor` verdrahten, damit der erste Kick hörbar wird.

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
