# Bass Forge Kick Architect — Nachplan bis zur Fertigstellung (OpenCode-Runbook)

> **Zweck dieser Datei:** Sie ist der *einzige* Auftrag für einen unbeaufsichtigten
> OpenCode-Lauf („über Nacht"). Der Lead-Agent arbeitet die Warteschlange unten
> Welle für Welle ab, delegiert **jede** Aufgabe an einen Sub-Agenten (möglichst
> günstiges/freies Modell), prüft das Gate, committet und läuft weiter.
> Diese Datei ist auch das Fortschritts-Protokoll: Kästchen werden abgehakt,
> Befunde werden eingetragen.

**Stand bei Übergabe:** 2026-11-17, 21:10 · Repo <https://github.com/TotalBassJunkie/bass-forge-kick-architect>
**Branch:** `main` · letzter Commit `836bdd9` · **CI grün** (Release-Build + Tests).
**Tests:** 317 Testfälle / 540.115 Assertions, Debug **und** Release grün.
**Teil-Fortschritt:** ≈ 61 % des Gesamtumfangs (Berechnung in Abschnitt 3).

---

## 0. In 60 Sekunden: was hier passiert

1. Der Lead liest diese Datei vollständig und legt `docs/NIGHT-LOG.md` an.
2. Pro **Welle**: Aufgaben parallel an Sub-Agenten geben (jede Aufgabe nur ihre
   Dateien!), Ergebnisse einsammeln, **Gate** fahren (Build + Tests + Doku),
   committen, pushen, Kästchen hier abhaken.
3. Nach der letzten Welle: Release-Build, volle Suite, `docs/PROGRESS.md`,
   `REPORT.md`, neuer Handoff. Fertig.
4. Bricht etwas dreimal hintereinander beim Gate ab: **Welle zurückrollen**,
   Tickets als `blocked` eintragen, mit der nächsten *unabhängigen* Welle
   weitermachen. Am Ende Bericht schreiben. Nie mit rotem Build weiterarbeiten.

---

## 1. Verfassung — Regeln, die **nie** gebrochen werden

### 1.1 Absolute Verbote

1. **Kein fremder Content.** Keine Samples, IRs, Presets, Logos, Strings,
   Binärdateien aus dem Referenzprodukt (Teknovault Kick Builder) ins Repo.
   Nur lesend analysieren, nie kopieren.
2. **Nicht anfassen:** die installierte Referenz unter
   `C:\Users\Public\Documents\Teknovault\…`, Original-Installer, `C:\bfka-re\`
   (nur lesen), alles außerhalb des Repos und `C:\bfka-build`.
3. **Build nur unter `C:\bfka-build`** (nie im Dropbox-Ordner — sonst
   `LNK1104`/`LNK1201` durch den Sync).
4. **Keine neuen Abhängigkeiten.** Nur JUCE 8.0.7 (bereits via FetchContent) und
   die C++-Standardbibliothek. Kein Netzwerkzugriff im Code, keine Downloads.
5. **Keine Test-Löschung.** Ein roter Test wird repariert, nicht entfernt.
   Ausnahme: ein Test, der nachweislich Falsches prüft — dann Test **und**
   Begründung im Commit.

### 1.2 Quelltext-Konventionen (kosten sonst Stunden)

* **Strings nur ASCII.** MSVC liest Quellen in der Systemcodepage; Umlaute in
  String-Literalen werden Mojibake. Kommentare dürfen Umlaute enthalten —
  sicherer ist `ae/oe/ue`.
* **Zeilenenden LF**, UTF-8 ohne BOM. Kein `Set-Content`, kein
  `write_text` ohne `newline="\n"`; in Python immer
  `open(p,'wb').write(text.encode('utf-8'))`.
* **Datei-Header-Kommentar** mit Zweck und Ticket-ID; öffentliche Symbole
  dokumentieren.
* **Kein `TODO` ohne Ticket-ID.**
* Neue Quelldateien **immer** in `CMakeLists.txt` (`BFKA_UI_SOURCES` /
  `BFKA_FX_SOURCES` / `BFKA_HEADERS`) **und** – bei Tests – in
  `tests/CMakeLists.txt` eintragen. Das macht **nur der Lead**, nie ein
  Sub-Agent.
* Threading-Verträge einhalten: `process()` im Audio-Thread **ohne**
  Allokation, Log, Mutex, Datei-IO.

### 1.3 Arbeitsweise mit Sub-Agenten

* Sub-Agenten **schreiben nur Dateien**. Sie rufen **kein** `git`, **kein**
  `cmake`, **kein** `ninja`, **keine** Tests auf. Build/Test/Commit macht der Lead.
* Ein Sub-Agent bekommt **maximal 3 Dateien** (1–2 neue + optional 1 Anpassung)
  und **eine** Aufgabe. Zu groß = aufteilen.
* **Vertrag zuerst:** Der Lead schreibt im Prompt die exakte API (Signaturen,
  Zustandsformat, Fehlerfälle) und die Testfälle auf. Sub-Agenten erfinden keine
  Architektur. (Erfahrungswert dieses Projekts: mit Vertrag kompilierten drei
  parallel geschriebene UI-Editoren ohne eine einzige Korrektur.)
* **Datei-Eigentum:** Innerhalb einer Welle bearbeitet **jeder** Sub-Agent
  **verschiedene** Dateien. Gemeinsame Dateien (`PluginProcessor.*`,
  `PluginEditor.*`, `KickPage.*`, `CMakeLists.txt`) sind **Lead-Dateien** — nur
  der Lead ändert sie, nach dem Einsammeln aller Tasks.
* **Kontext-Hygiene:** Jeder Prompt nennt die 2–3 Dateien, die der Sub-Agent
  zuerst lesen soll, und sagt „lies nichts anderes".

### 1.4 Modell-Routing (kostenarm)

| Tier | Wofür | Beispiele (falls verfügbar) |
|---|---|---|
| **A** (frei/günstig) | mechanische Dateiarbeit gegen fertigen Vertrag: UI-Views, Parser, IO, Testdateien, Doku | `deepseek-flash`, `qwen-coder`, `glm-5.3-flash` |
| **B** (mittel) | DSP mit vorgegebenen Formeln, Mehrdatei-Integration, Fehlersuche an einem roten Test | nächststärkeres verfügbares Modell |
| **C** (Lead) | Orchestrierung, CMake, git, CI-Diagnose, Architekturentscheidungen, alles mit Klang | das stärkste verfügbare Modell |

Regel: **immer mit Tier A starten.** Erst wenn dieselbe Aufgabe zweimal am Gate
scheitert, einmal mit Tier B wiederholen; danach aufgeben (blocked) und
weiterziehen. Kein Modell-Roulette: maximal **1 Eskalation** pro Aufgabe.

---

## 2. Umgebung, Kommandos, Fallstricke

### 2.1 Kommandos (immer so)

```powershell
cd "C:\Users\tbj\Dropbox\.BFR Plugin Projects\Kick Builder Plugin Bass Forge"
. .\tools\dev-env.ps1                    # MSVC, CMake, Ninja, BFKA_BUILD_ROOT
.\tools\configure.ps1 -Config Debug      # NUR nach CMakeLists-Aenderungen
.\tools\build.ps1 -Config Debug
cmd /c "C:\bfka-build\Debug\tests\BFKA_Tests_artefacts\Debug\BFKA_Tests.exe > C:\bfka-re\out.txt 2> C:\bfka-re\err.txt"
"Exit=$LASTEXITCODE"; Get-Content C:\bfka-re\out.txt | Select-Object -Last 3
```

Release-Prüfung (vor jedem Wellen-Ende, greift CPU-Budgets):

```powershell
.\tools\configure.ps1 -Config Release
.\tools\build.ps1 -Config Release
cmd /c "C:\bfka-build\Release\tests\BFKA_Tests_artefacts\Release\BFKA_Tests.exe > C:\bfka-re\rel_out.txt 2> C:\bfka-re\rel_err.txt"
```

Einzelne Testgruppen: `… BFKA_Tests.exe "[ui]"` — **in PowerShell einfach
quoten** (`'[ui]'`), sonst deutet PowerShell die Klammern als Array-Index.

CI:

```powershell
git push origin main
gh run list --limit 3                    # den LETZTEN Lauf ansehen
gh run watch <id> --exit-status
```

### 2.2 Fallstricke (jeder hat schon Zeit gekostet)

* **`juce::Slider::setRange(a, b, interval)` rundet den Wert.** Bei
  normalisierten Parametern (0…1) deshalb **Intervall 0**.
* **`resized()` im Konstruktor aufrufen**, wenn die Kinder erst danach
  angelegt werden (sonst bleibt die Seite leer).
* **`LinearBar` zeichnet keinen Text** → eigenes `ValueBar`-Muster in
  `src/ui/KickPage.{h,cpp}`.
* **`juce::FileOutputStream` scheitert still**, wenn die Datei gesperrt ist →
  vor Render-Tests `C:\bfka-re\ui-*.png` löschen und Dateizeit prüfen.
* **`std::make_unique<X>(..., *this)` mit privater Listener-Basis** scheitert →
  `static_cast<X::Listener&>(*this)` im Member.
* **`setChain` ruft `setState`** — nicht entfernen (sonst gehen Bänder/Kurven/IR
  verloren).
* **Parameterglättung 20 ms** → Tests müssen ~80 Blöcke à 128 Samples
  einschwingen, bevor sie exakte Werte erwarten.
* **`writeBackModuleStates()`** entfernt Struktur-Kinder (`Band`, `WNode`, `Pt`,
  `Eq`, `Dc`) immer und schreibt sie neu; nur diese Namen verwenden.
* **Presets laden Samples nur über `applyState()` → `loadFromState()`**
  (nicht auf einen zweiten Pfad verlassen).
* **PowerShell + stderr:** `git push`/Tests schreiben Fortschritt auf stderr und
  liefern dadurch Exitcode 1 als „NativeCommandError". Text lesen, nicht nur den Code.
* **Doku-Umlaute:** beim Editieren echte Zeichen schreiben, nicht die Mojibake
  aus der Konsolenausgabe kopieren (sonst „old_string not found").

### 2.3 Definition of Done (jede Aufgabe)

1. Build **Debug** und **Release** mit MSVC `/W4` **ohne neue Warnungen**
   (`warning D9025` ist Bestand und zählt nicht).
2. Mindestens **ein** Test in `tests/` für die neue Funktion; die **volle**
   Suite bleibt grün (Debug **und** Release).
3. Öffentliche Symbole dokumentiert, Ticket-ID im Dateikopf.
4. Doku: passendes `docs/specs/*.md` + Eintrag in `docs/PROGRESS.md`.
5. Commit-Format: `feat(<bereich>): <was> [B-xx]` (bzw. `fix(...)`, `test(...)`,
   `docs(...)`), danach Push auf `origin/main`.
6. Kein fremder Content, keine neuen Abhängigkeiten.

---

## 3. Wo das Projekt steht (Ausgangslage)

| Phase | Inhalt | Stand |
|---|---|---|
| P0 Fundament | Repo, Build, CI, Tests | **fertig** |
| P1 Datenmodell & IO | `KickState`, `ParameterRegistry` (320 IDs), `.bfkapreset`, `.bfkapreset`-Module, A/B | **fertig** |
| P2 Library & Analyse | Index/Scan + Dateinamen-Parser; **Audio-Analyse fehlt** | ~60 % |
| P3 Engine-Core | `SampleVoice`, `KickRenderer` (12 Slots, Solo, Autogain), `KickAssembler` | **fertig** |
| P4 Stretch & Pitch | `WsolaStretcher`, `Resampler` gebaut/getestet, **nicht in der Voice verdrahtet** | ~50 % |
| P5 ALIGN & PHASE ALIGN | nicht begonnen | 0 % |
| P6 FX-Module | 10 Typen, Karten, 6 Graph-Editoren, Sidechain, CPU-Profil | **fertig** |
| P7 Style-Randomizer | `StyleModel` + `RandomizerEngine` (Regeln, No-Repeat) fertig; **UI/CHAOS/FREE BUILD fehlen** | ~60 % |
| P8–P10 UI | KICK-Seite (Karten, Visualiser, Transport) + FX-Rack fertig; **Style-Sidebar, Overlays, Undo, Resize fehlen** | ~60 % |
| P11 Export & D&D | nicht begonnen | 0 % |
| P12 Key & MIDI | nur Note-On-Trigger | ~10 % |
| P13 Parity/Installer | nicht begonnen | 0 % |

**Gewichtet: ≈ 61 %.** „Benutzbar" (Kick bauen + FX-Kette) ist ≈ 75 %.
**Ziellinie dieses Plans:** alle Phasen bis auf die ausdrücklich
menschlichen Punkte (Abschnitt 8) erledigt; Release-Artefakte gebaut; Doku
vollständig.

---

## 4. Warteschlange — Wellen, Aufgaben, Gates

**Spalten:** ID · Ticket · Ziel · Dateien (Eigentum des Sub-Agenten) · Test ·
Tier · Prio (1 = muss, 2 = wichtig, 3 = nice).

**Regeln für jede Welle**

1. Lead liest die Welle, schreibt die Verträge (Abschnitt 5, Vorlage).
2. Sub-Agenten **parallel** starten (so viele wie Aufgaben, maximal 4 —
   mehr parallel bringt bei geteilter CPU nichts und kostet nur Kontingent).
3. Einsammeln, **Lead integriert** gemeinsame Dateien, trägt CMake ein.
4. **Gate** (Abschnitt 6). Grün → commit + push + CI. Rot → reparieren
   (max. 2 Versuche), sonst Welle zurückrollen (`git checkout -- <dateien>`),
   Tickets `blocked`, nächste Welle.
5. Kästchen unten abhaken, eine Zeile in `docs/NIGHT-LOG.md`.

---

### W1 — Kleinkram & Korrektheit (4 parallel, risikolos)

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W1.1 | **B-150** | `getTailLengthSeconds()` = Kick-Länge + Ketten-Tails | `src/plugin/PluginProcessor.{h,cpp}`, `src/fx/FxModule.h`, `src/fx/modules/ConvolverModule.{h,cpp}` | `tests/TailLengthTests.cpp` | A | 1 |
| W1.2 | **B-151** | Modulkarten mit vielen Parametern gruppieren (Seiten/Reiter), Höhe ≤ 420 px | `src/ui/ModuleCard.{h,cpp}` | `tests/ModuleCardLayoutTests.cpp` | A | 1 |
| W1.3 | **B-95** | Samples optional ins Preset einbetten (`<Samples>` Base64-WAV) + Rückweg | `src/core/PresetIO.{h,cpp}` | `tests/PresetEmbedTests.cpp` | B | 1 |
| W1.4 | **B-152** | IR-Browser: User- und Factory-Ordner, Liste, Doppelklick lädt | `src/library/ImpulseLibrary.{h,cpp}` (neu), `src/ui/editors/ConvolverView.{h,cpp}` | `tests/ImpulseLibraryTests.cpp` | A | 2 |

**Vertrags-Skizzen**

* **W1.1** `FxModule` bekommt `virtual double tailSeconds() const { return 0.0; }`.
  `ConvolverModule` überschreibt es als
  `hasImpulseResponse() ? currentIrLength() / currentIrSampleRate() : 0.0`
  (beide Getter existieren schon).
  `PluginProcessor::getTailLengthSeconds()` = `renderer.kickLengthSeconds() +
  fxChain.tailSeconds()`, geklemmt auf `[0.05, 60.0]`. `FxChainProcessor::
  double tailSeconds() const` = Summe der Modul-Tails (Kette ist seriell).
  Tests: 0 Module → Kick-Länge; Convolver mit 2-s-IR → ≥ Kick+2 s; nie < 0,05.
* **W1.2** `ModuleCard` zeigt höchstens **12** Parameter je Seite, Rest über
  `PREV`/`NEXT`-Knöpfe (`pageCount()`, `setPage(int)`, `currentPage()`).
  `preferredHeight()` darf 420 px nicht überschreiten. `dynEq` (64 Parameter)
  muss damit in ≤ 6 Seiten passen. Test: Karte für `dynEq` bauen,
  `preferredHeight() <= 420`, Seitenwechsel zeigt andere Slider-Werte.
* **W1.3** `PresetWriteOptions { bool embedSamples = false; juce::int64
  maxEmbeddedBytes = 32 * 1024 * 1024; }`, überladene
  `PresetIO::saveKickPreset (file, state, info, options, error)`. Jedes Sample
  als `<Sample slot="N" fileName="…" format="wav" bits="16">BASE64</Sample>`
  unter `<Samples>`; Limit überschreiten = Fehlertext, kein Teilpreset.
  Beim Laden: fehlt die Datei am erwarteten Pfad, WAV aus `<Samples>` in
  `Dokumente\BassForge\KickArchitect\SampleCache\` schreiben und Referenz
  darauf setzen. **Test:** Preset mit einem 0,25-s-WAV speichern (eingebettet),
  Originaldatei löschen, Preset laden → Slot gefüllt, `isSlotFilled(0)`.
* **W1.4** `ImpulseLibrary`: `scan()` über
  `Dokumente\BassForge\KickArchitect\IRs` + optionalen Factory-Ordner;
  `const std::vector<ImpulseInfo>& entries()` (`name`, `path`, `seconds`);
  `refresh()`. `ConvolverView` bekommt eine scrollbare Liste unter dem
  IR-Feld; Doppelklick setzt `ir` und lädt. Ohne Ordner: Hinweis, kein Absturz.

---

### W2 — Pitch & Stretch hörbar (Basis-Modell, **streng seriell**)

Grund: Hier hängt alles Weitere ab (Karten-Regler, ALIGN, BPM-SYNC,
Vorab-Renderer). Deshalb **ein** Sub-Agent pro Schritt, Tier B, Lead prüft nach
jedem Schritt.

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W2.1 | **B-42, B-43** | `VoiceSettings` trägt `pitchSemitones`, `stretchFactor`, `StretchMode`; `SampleVoice` wendet sie an (REPITCH = Resampling, KEEP PITCH = WSOLA-Vorrendering, Fallback = REPITCH) | `src/engine/SampleVoice.{h,cpp}`, `src/engine/KickAssembler.cpp` (`voiceSettingsForSlot`), `src/engine/KickRenderer.{h,cpp}` (`setSlot`/`setSlotSettings` reichen durch) | `tests/PitchStretchTests.cpp` | B | 1 |
| W2.2 | **B-44** | Längenneutraler Pitch-Shift: Resampling um `2^(st/12)` + WSOLA-Rückführung auf die Originallänge | `src/engine/PitchShifter.{h,cpp}` (neu) | `tests/PitchShifterTests.cpp` | B | 1 |
| W2.3 | **B-45, B-47** | Vorab-Renderer + Cache (Schlüssel: Sample-Id + Pitch + Stretch + Modus), Grenzfälle 0,25×/8×/sehr kurz/Stille/DC | `src/engine/PreRenderCache.{h,cpp}` (neu) | `tests/PreRenderTests.cpp` | B | 2 |
| W2.4 | **B-46** | Qualitätsmetriken (Transientenerhalt, Aliasing, Phasenkohärenz) als Test-Report | `tests/QualityMetricsTests.cpp` (neu) | dito | A | 3 |

**Verträge W2.1 (Auszug)**

```cpp
struct VoiceSettings {
    // ... bestehende Felder ...
    double pitchSemitones = 0.0;    // -24..+24, laengenneutral bei KEEP PITCH
    double stretchFactor  = 1.0;    // 0.25..8.0 (Zeitfaktor > 1 = laenger)
    StretchMode mode      = StretchMode::keepPitch;  // keepPitch | repitch
};
```
`SampleVoice`: bei `repitch` wirkt `pitchSemitones` **und** `stretchFactor`
gemeinsam auf `stepPerOutputSample` (Länge ändert sich mit);
bei `keepPitch` wird zuerst ein vorgerenderter Puffer übernommen
(`setPreRendered (const juce::AudioBuffer<float>*, double rate)`), sonst
Fallback = REPITCH (und `bool isPitchAccurate() const` meldet `false`, damit die
UI das anzeigen kann).
**Tests:** +12 Halbtöne halbiert die Länge (REPITCH) bzw. hält sie
(KEEP PITCH, gemessen über `lengthSeconds()` mit Toleranz 2 %);
`stretchFactor = 2` verdoppelt die Länge; KEEP PITCH verändert die
Grundfrequenz nicht (Nulldurchgangs-Zählung vor/nach).

**W2.1-Zuordnung (steht schon fest, nicht neu erfinden):**
`KickAssembler::voiceSettingsForSlot()` bildet
`lN_pitch` → `pitchSemitones` (Halbton-Parameter, Bereich −24…+24),
`lN_stretch` → `stretchFactor` (Potenzkurve 0,25…8) und
`KickState::stretchMode()` → `mode`.

**W2.2-Zuordnung:** der Pitch-Shift ist
`Resampler` (Verhältnis `2^(st/12)`) gefolgt von
`WsolaStretcher::process (input, output, 1.0 / ratio)` mit
`WsolaStretcher::outputLengthFor()` als Zielgröße; `clampFactor()` beachten.

**Klangprüfung (Lead, Pflicht):** Nach W2.1 im Standalone `DEMO-KICK` und an
Slot 1 `PITCH`/`STRETCH` ziehen — keine Klicks, keine Ausreißer. Befund in
`docs/NIGHT-LOG.md`.

---

### W3 — Zustand, Undo, Tempo (3 parallel)

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W3.1 | **B-15** | Undo/Redo über ganze Zustände (`juce::UndoManager` + Snapshot-Ableitung), 50 Schritte | `src/core/UndoHistory.{h,cpp}` (neu) | `tests/UndoHistoryTests.cpp` | B | 2 |
| W3.2 | **B-16** | Zustands-Migration über Schema-Versionen (v1 → v2 …), unbekannte Felder erhalten | `src/core/KickState.cpp`, `src/core/PresetIO.cpp` | `tests/StateMigrationTests.cpp` | A | 3 |
| W3.3 | **B-83b** | `FOLLOW DAW`: BPM aus dem Host-Playhead, `BpmMode` wird wirksam, Chip im Transport | `src/plugin/PluginProcessor.{h,cpp}`, `src/engine/KickRenderer.{h,cpp}` | `tests/FollowDawTests.cpp` | B | 2 |

**W3.1-Vertrag:** `UndoHistory { void beginEdit (const juce::String& name);
void endEdit(); bool canUndo() const; bool canRedo() const;
juce::String undoName() const; … bool undo(); bool redo(); void clear(); }`.
Der Prozessor ruft `beginEdit/endEdit` um jede Zustandsänderung
(`applyState`, `setSlotSample`, …) — **kein** Undo für Regler-Zwischenstände
(nur `endEdit` beim Loslassen), damit die Historie nicht zumüllt.
**W3.3-Vertrag:** `PluginProcessor::getPlayHead()` → `AudioPlayHead::PositionInfo`
mit `getBpm()`; im `processBlock` bei `BpmMode::syncBpm` das Host-BPM an
`renderer.setRenderSettings()` und `KickShaper`-Länge weiterreichen (max. 4×
pro Sekunde, nicht jeder Block). Test mit gefaktem PlayHead.

---

### W4 — Style-Sidebar & Randomizer-UI (3 parallel + Lead-Integration)

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W4.1 | **B-82** | Style-Sidebar: Style-Liste (9 Factory + FREE BUILD), Akzentfarbe, `+ ADD STYLE`, Roll-Zähler | `src/ui/StyleSidebar.{h,cpp}` (neu) | `tests/StyleSidebarTests.cpp` | A | 1 |
| W4.2 | **B-75** | CHAOS: Pitch ±3 HT, Stretch, FX-Chain, FX-Module, eigenes Würfeln, deterministisch mit Seed | `src/library/ChaosEngine.{h,cpp}` (neu) | `tests/ChaosEngineTests.cpp` | A | 2 |
| W4.3 | **B-76** | FREE BUILD, BOTH LIBRARIES, eigene Styles als `.bfkastyle` (IO + Verwaltung) | `src/core/StyleIO.{h,cpp}` (neu), `src/library/UserStyles.{h,cpp}` (neu) | `tests/StyleIOTests.cpp` | A | 2 |

**W4.1-Vertrag:** `StyleSidebar : juce::Component` mit
`std::function<void(const juce::String& styleId)> onStyleSelected`,
`onRandomizeRequested`, `onChaosToggled (ChaosFlags)`;
`void setModel (const StyleModel*, const UserStyles*)`,
`void setActiveStyle (const juce::String&)`, `void setAccentFor (juce::Colour)`.
Reihenfolge/ Akzentfarben aus `docs/01-BESTANDSAUFNAHME.md` §5.4.
**W4.2-Vertrag:** `struct ChaosFlags { bool pitch, stretch, fxChain, fxModules; }`,
`class ChaosEngine { ChaosEngine (RandomizerEngine&, std::uint64_t seed);
void setFlags (ChaosFlags); void applyTo (KickState&, ChaosFlags); }` —
deterministisch: gleicher Seed + gleicher Eingangszustand = gleiches Ergebnis.

**Lead-Integration (nach Einsammeln):** Sidebar in `KickPage` links einhängen,
KickPage-Layout anpassen (Sidebar 210 px, Kartenbereich schrumpft, `VIEW`
bleibt erreichbar), `+ ADD STYLE` mit Overlay aus W5.5 verbinden.

---

### W5 — Overlays (5 parallel, je eigene Datei)

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W5.1 | **B-100** | Library-Overlay: 4 Tabs (PRESETS/SOUNDS/FX CHAINS/MY SOUNDS), Suche, Vorschau „woraus besteht der Preset", Favoriten | `src/ui/overlays/LibraryOverlay.{h,cpp}` (neu) | `tests/LibraryOverlayTests.cpp` | A | 1 |
| W5.2 | **B-101** | Settings-Overlay, 8 Sektionen, alle Zeilen aus `prefs.json` | `src/core/Prefs.{h,cpp}` (neu), `src/ui/overlays/SettingsOverlay.{h,cpp}` (neu) | `tests/PrefsTests.cpp` | A | 2 |
| W5.3 | **B-102** | Preset-Manager (browsen, speichern, umbenennen, duplizieren, Papierkorb) | `src/ui/overlays/PresetManagerOverlay.{h,cpp}` (neu) | `tests/PresetManagerTests.cpp` | A | 2 |
| W5.4 | **B-105** | Dialog-/Tooltip-System (Enter/Escape, innerhalb des Fensters, kein Modal-Loop) | `src/ui/overlays/DialogHost.{h,cpp}` (neu) | `tests/DialogHostTests.cpp` | A | 1 |
| W5.5 | **B-103, B-104** | `+ ADD PART`-Picker (zweistufig, Gruppensymbole, „NO FREE SLOT") und `+ ADD STYLE` | `src/ui/overlays/AddPartOverlay.{h,cpp}`, `AddStyleOverlay.{h,cpp}` (neu) | `tests/AddPartTests.cpp` | A | 2 |

**Gemeinsamer Vertrag (vom Lead vorab schreiben):**

```cpp
class OverlayHost {                        // DialogHost.h
public:
    void push (std::unique_ptr<juce::Component> overlay);   // dunkelt ab, faengt Klicks
    void pop();
    bool isOpen() const;
    void setBoundsToParent (juce::Rectangle<int>);
};
```
Jedes Overlay ist ein `juce::Component` mit `paint/resized`, **kein**
`showModalDialog`, **kein** eigener Looper. Alle Overlays werden erst
eingebunden, wenn sie einzeln rendern und ihre Tests grün sind.

**Lead-Integration:** `OverlayHost` in `PluginEditor` (über den Seiten),
Header-Knöpfe LIBRARY / GEAR / PRESET-PILLE anschließen.

---

### W6 — Library-Analyse (4 parallel, eigener Ordner)

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W6.1 | **B-23** | Onset/Transienten-Detektor (Envelope-Fluss) für Trim-Vorschläge | `src/library/analysis/OnsetDetector.{h,cpp}` (neu) | `tests/OnsetDetectorTests.cpp` | A | 2 |
| W6.2 | **B-24** | Key-Detection (Chroma/HPCP + Template-Matching, Konfidenz) | `src/library/analysis/KeyDetector.{h,cpp}` | `tests/KeyDetectorTests.cpp` | B | 2 |
| W6.3 | **B-26** | Analyse-Cache (binär, versioniert, inkrementell) | `src/library/analysis/AnalysisCache.{h,cpp}` | `tests/AnalysisCacheTests.cpp` | A | 2 |
| W6.4 | **B-27** | Hintergrund-Analyse-Thread mit Fortschritt/Abbruch | `src/library/analysis/AnalysisJob.{h,cpp}` | `tests/AnalysisJobTests.cpp` | B | 2 |

Regel: **keine** Analyse im `processBlock`, alles auf dem Message-/Worker-Thread.
Messerwartung (aus dem Masterplan): Key-Detection ≤ 5 % Abweichung zum
Dateinamen-Key auf der Test-Library; Onset-Toleranz ±10 ms.
**Lead-Integration:** `SampleLibrary` ruft Cache/Job an, `SampleInfo` bekommt
`bpm`, `key`, `lengthSeconds`, `onsetSeconds`, `confidence`.

---

### W7 — Export & Drag and Drop (4 parallel + Lead)

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W7.1 | **B-37, B-110** | Offline-Renderer: Kick + FX-Kette offline in einen Puffer (48 kHz/32 bit) | `src/engine/OfflineRenderer.{h,cpp}` (neu) | `tests/OfflineRenderTests.cpp` | B | 1 |
| W7.2 | **B-111** | Stems: je Part durch die volle Kette | `src/engine/StemRenderer.{h,cpp}` (neu) | `tests/StemRenderTests.cpp` | B | 2 |
| W7.3 | **B-112, B-114** | Dateinamenschema + Kollisionsschutz + WAV-Writer (Projekt-Samplerate, Bit-Tiefe) | `src/core/ExportNaming.{h,cpp}`, `src/core/WavWriter.{h,cpp}` (neu) | `tests/ExportIOTests.cpp` | A | 1 |
| W7.4 | **B-113** | Datei-Drop auf Karten (`wav/aif/flac/ogg/mp3`) + Key aus Dateiname | `src/ui/FileDropTarget.{h,cpp}` (neu), `src/library/KeyFromFilename.{h,cpp}` (neu) | `tests/FileDropTests.cpp` | A | 2 |

**Lead-Integration (B-110b):** `KickPage` bekommt `DRAG` (Master) und
`DRAG STEMS`; `startDragging` über `performExternalDragDropOfFiles` mit
offline gerenderter temporärer WAV im `Exports`-Ordner.

---

### W8 — Key & MIDI (3 parallel)

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W8.1 | **B-120** | KEY TRACK: Noten → Transposition, Oktav-Angleich der Parts | `src/engine/KeyTracker.{h,cpp}` (neu) | `tests/KeyTrackTests.cpp` | B | 2 |
| W8.2 | **B-121, B-122** | `P`-Flag-Priorität (Crunch > Rumble > Rest), KEY-Chip-Zustände (Auto/fest/`X`/`A#+G#`) | `src/ui/KeyChip.{h,cpp}` (neu), `src/engine/KeyState.{h,cpp}` (neu) | `tests/KeyChipTests.cpp` | A | 2 |
| W8.3 | **B-123, B-124** | MIDI-Modi (`midi_mode`), Note-On-Retry, All-Notes-Off; KEY-DETECTION-Güte | `src/engine/MidiModes.{h,cpp}` (neu) | `tests/MidiModeTests.cpp` | B | 3 |

---

### W9 — ALIGN & PHASE ALIGN (Basis-Modell, **streng seriell**)

Der musikalisch heikelste Block; jede Stufe einzeln prüfen und hören.

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W9.1 | **B-50, B-51** | Gap-Analyse (Ende Anker ↔ Beginn Tails) und `MOVE CRUNCH` mit TIGHT/NORMAL/GENTLE | `src/engine/AlignEngine.{h,cpp}` (neu) | `tests/AlignEngineTests.cpp` | B | 1 |
| W9.2 | **B-52, B-53** | `STRETCH PUNCH` (nur KEEP PITCH) und `STRETCH CRUNCH`, `STRETCH FADES` | dito | dito | B | 1 |
| W9.3 | **B-54, B-55** | Hand-Platzierung (`pinned`) respektieren, Anker-Wahl (Library-Punch, sonst stärkster Start) | dito | dito | B | 2 |
| W9.4 | **B-56** | PHASE ALIGN: Korrelation im Low-End, Nudge + Polaritäts-Flip | `src/engine/PhaseAlign.{h,cpp}` (neu) | `tests/PhaseAlignTests.cpp` | B | 2 |
| W9.5 | **B-57, B-58** | `LAST PASS`-Report-Objekte + `RESULT LINE`/`ALIGN BLEND`-Settings | `src/engine/AlignReport.{h,cpp}` (neu) | `tests/AlignReportTests.cpp` | A | 3 |

**Vertrag (Lead schreibt ihn vor W9.1 vollständig):**
`struct AlignResult { std::vector<double> offsetsSeconds; std::vector<double>
stretchFactors; std::vector<bool> polarityFlip; juce::StringArray report; };
AlignResult run (const KickState&, const std::vector<PartTiming>&,
const AlignSettings&);`
Tests messen **Zahlen**, keine Strings: Lücke nach ALIGN ≤ 1 ms, Anker
unverändert, `pinned` nie verschoben, Polaritäts-Flip verbessert die
Low-End-Korrelation (vorher/nachher als Messwert).

---

### W10 — Politur & Werkzeuge (4 parallel)

| ID | Ticket | Ziel | Dateien | Test | Tier | Prio |
|---|---|---|---|---|---|---|
| W10.1 | **B-87** | Regler-Rechtsklick (Reset/Typisieren/Copy&Paste), Shift-Fein, Ctrl-Schritte — als ein Helfer für alle Regler | `src/ui/KnobHelpers.{h,cpp}` (neu) | `tests/KnobHelpersTests.cpp` | A | 2 |
| W10.2 | **B-89** | Resize/UIScaler/HiDPI (Fenster 70–200 %, Layout bleibt lesbar) | `src/ui/UiScaler.{h,cpp}` (neu) | `tests/UiScalerTests.cpp` | B | 3 |
| W10.3 | **B-133** | Crash-Härtung: fehlende Samples, kaputte WAVs, fehlender Content-Ordner, 0-Byte-Presets | `tests/RobustnessTests.cpp` + gezielte Fixes | dito | A | 1 |
| W10.4 | **B-131** | `.itkbpreset` → `.bfkapreset`-Konverter als Konsolen-Tool (nur eigene Analyse-Daten) | `tools/preset-convert/main.cpp` (neu), `CMakeLists.txt` (Lead) | `tests/PresetConvertTests.cpp` | A | 3 |

---

### W11 — Abschluss (Lead selbst, keine Sub-Agenten)

| ID | Ticket | Ziel |
|---|---|---|
| W11.1 | **B-130** | Parity-Harness: soweit **ohne** Original-Plugin messbar (Impuls-/Spektrum-Invarianten, Hüllkurvenform, Kick-Länge, Latenz). Der echte „Original ↔ Nachbau"-Vergleich bleibt Mensch (Abschnitt 8) |
| W11.2 | **B-134** | Inno-Setup-Skript + portable ZIP + Release-Notes (Version aus CMake) |
| W11.3 | **B-135** | README/Quickstart/Screenshots (aus `BFKA_RENDER_UI=1`-Renderings)/Lizenztexte |
| W11.4 | – | Release-Build, volle Suite Debug+Release, `docs/PROGRESS.md`, `REPORT.md`, Handoff schreiben |

---

## 5. Vorlage: Sub-Agent-Prompt (kopieren und ausfüllen)

```text
Du bist Implementierungs-Sub-Agent im Projekt "Bass Forge Kick Architect"
(C++20 + JUCE 8.0.7, Windows, MSVC). Du bekommst genau EINE Aufgabe.

REGELN (verbindlich):
- Lies zuerst NUR diese Dateien: <Datei 1>, <Datei 2>.
- Schreibe NUR diese Dateien: <Datei A>, <Datei B>.
- Rufe KEIN git, KEIN cmake, KEIN ninja und KEINE Tests auf. Du baust nicht.
- Keine neuen Abhängigkeiten, kein Netzwerk, keine fremden Assets/Content.
- Strings ausschliesslich ASCII (ae/oe/ue). Zeilenenden LF. UTF-8 ohne BOM.
- Kommentare auf Deutsch, Bezeichner englisch. Dateikopf mit Zweck + Ticket
  "<B-xx>".
- Kein TODO ohne Ticket-ID. Keine Platzhalter: die Datei muss vollstaendig sein.
- Halte dich exakt an die API unten. Erfinde keine Architektur.

PROJEKTKONTEXT (so baust du auf):
<2-5 Saetze: was das Modul ist, wie es aufgerufen wird, Threading>

VERTRAG (exakt diese Signaturen/Formate):
<Header-Block mit Signaturen, Datenformaten, Fehlerfaellen>

TESTS (du schreibst die Datei tests/<X>Tests.cpp, Catch2 v3):
<4-8 konkrete REQUIRE/CHECK-Zeilen in Prosa: welche Eingabe, welches Ergebnis>

AUSGABE:
1. Die vollstaendigen Dateien (keine Diffs, keine Auslassungen).
2. Ein Kurzbericht (max. 10 Zeilen): welche Dateien, welche offenen Punkte,
   welche Annahmen.
```

---

## 6. Das Gate (nach **jeder** Welle, ohne Ausnahme)

```powershell
cd "C:\Users\tbj\Dropbox\.BFR Plugin Projects\Kick Builder Plugin Bass Forge"
. .\tools\dev-env.ps1
.\tools\configure.ps1 -Config Debug
.\tools\build.ps1 -Config Debug
cmd /c "C:\bfka-build\Debug\tests\BFKA_Tests_artefacts\Debug\BFKA_Tests.exe > C:\bfka-re\out.txt 2> C:\bfka-re\err.txt"
"Exit=$LASTEXITCODE"; Get-Content C:\bfka-re\out.txt | Select-Object -Last 3
```

**Gate ist grün, wenn**

1. Debug-Build ohne **neue** Warnungen (`error C…`, `warning C…` außer `D9025`),
2. `All tests passed (… in N test cases)` mit **N ≥ letzter Stand**,
3. Release-Build + Release-Suite ebenfalls grün (mindestens 1× pro Welle),
4. `docs/PROGRESS.md` und die passende `docs/specs/*.md` fortgeschrieben,
5. die neuen Dateien in `CMakeLists.txt`/`tests/CMakeLists.txt` stehen,
6. `git status` sauber committed, gepusht, CI-Lauf **grün**
   (`gh run list --limit 3`, den letzten Lauf ansehen — ältere werden durch neue
   Pushes abgebrochen, das ist normal).

**Bei rotem Gate**

1. Ursache lesen (`Select-String -Path C:\bfka-re\out.txt -Pattern 'FAILED'
   -Context 0,6`).
2. Max. **2** Reparaturversuche (Tier A → Tier B).
3. Sonst: `git checkout -- <dateien der Welle>` (bzw. `git restore .`),
   `git clean -fd` nur für neu angelegte Dateien der Welle, Welle als
   `blocked` markieren, **weiter mit der nächsten unabhängigen Welle**.
4. Nie mit rotem Gate weiterarbeiten; nie einen roten Test löschen.

---

## 7. Wie der Agent berichtet (Pflicht)

* **`docs/NIGHT-LOG.md`** (neu, fortlaufend, neueste Zeile unten):
  ```
  21:15 W1 gestartet (4 Tasks) | Modelle: deepseek-flash x4
  21:52 W1 Gate gruen | 322 Testfaelle | commit 1a2b3c4
  21:53 W2.1 gestartet (Tier B)
  ```
* **Diese Datei**: Kästchen in Abschnitt 10 abhaken, bei blockierten Tickets
  `→ blocked: <Grund>` ergänzen.
* **`REPORT.md`** (am Ende): pro Welle eine Zeile, offene Punkte, Zahlen,
  was ein Mensch hören/prüfen muss.
* **`docs/PROGRESS.md`**: ein Abschnitt „Nacht-Lauf <Datum>" mit den
  wichtigsten Ergebnissen und den neuen Testzahlen.
* **Handoff** (`C:\Users\tbj\Desktop\Bass Forge Kick Architect - HANDOFF
  <Datum>_<HHMM>.md`) nach dem Muster des Vorgängers.

---

## 8. Ausdrücklich **menschlich** — nicht automatisieren

| Punkt | Warum |
|---|---|
| **Sidechain-Hosttest in einer DAW** (B-153 Rest) | Braucht Ableton/Reaper/Cubase mit Aux-Bus-Routing. Im Unit-Test nicht abbildbar. Am Ende als „manuell offen" melden. |
| **Hörurteil „klingt wie das Original"** (B-46, B-77, B-130) | Ohr und Referenz-Plugin. Der Agent liefert nur Messwerte und Reports. |
| **Freigabe der Akzentfarben/Typografie** (B-80) | Geschmacksfrage des Auftraggebers. Agent liefert Renderings (`C:\bfka-re\ui-*.png`). |
| **JUCE-Lizenzierung vor einem kommerziellen Release** | Rechtliche Entscheidung. |
| **Löschen/Umbenennen von Nutzer-Presets** | Destruktiv; bleibt Bestätigungsdialog + Mensch. |

---

## 9. Fertig ist das Projekt, wenn …

1. alle Tickets aus Abschnitt 4 mit Prio 1 und 2 `done` oder `blocked` (mit
   Grund) sind,
2. Debug **und** Release bauen, alle Tests grün (Ziel: **> 400 Testfälle**),
3. CI grün auf `main`, VST3 + Standalone als Artefakt vorhanden,
4. `README.md`, `docs/PROGRESS.md`, `docs/specs/*` den tatsächlichen Stand
   beschreiben (keine „geplant"-Reste für gebaute Dinge),
5. `REPORT.md` + Handoff geschrieben sind,
6. die fünf menschlichen Punkte aus Abschnitt 8 als TODO für den Auftraggeber
   dastehen.

**Nicht** Teil der Ziellinie: Feature-Wünsche über den Masterplan hinaus
(P14 „eigene Zusatzfunktionen").

---

## 10. Fortschritt (der Agent hakt hier ab)

### Wellen

- [ ] W1 Kleinkram & Korrektheit (B-150, B-151, B-95, B-152)
- [ ] W2 Pitch & Stretch (B-42, B-43, B-44, B-45, B-47, B-46)
- [ ] W3 Zustand/Undo/Tempo (B-15, B-16, B-83b)
- [ ] W4 Style-Sidebar & Randomizer (B-82, B-75, B-76)
- [ ] W5 Overlays (B-100, B-101, B-102, B-105, B-103, B-104)
- [ ] W6 Library-Analyse (B-23, B-24, B-26, B-27)
- [ ] W7 Export & D&D (B-37, B-110, B-111, B-112, B-113, B-114)
- [ ] W8 Key & MIDI (B-120, B-121, B-122, B-123, B-124)
- [ ] W9 ALIGN & PHASE ALIGN (B-50…B-58)
- [ ] W10 Politur & Werkzeuge (B-87, B-89, B-133, B-131)
- [ ] W11 Abschluss (B-130, B-134, B-135, Release, Report)

### Tickets en détail

- [ ] B-150 Tail-Länge · - [ ] B-151 Karten-Layout · - [ ] B-95 Samples im Preset · - [ ] B-152 IR-Browser
- [ ] B-42 REPITCH · - [ ] B-43 BPM-SYNC · - [ ] B-44 Pitch längenneutral · - [ ] B-45 Vorab-Renderer · - [ ] B-46 Qualität · - [ ] B-47 Grenzfälle
- [ ] B-15 Undo/Redo · - [ ] B-16 Migration · - [ ] B-83b FOLLOW DAW
- [ ] B-82 Sidebar · - [ ] B-75 CHAOS · - [ ] B-76 FREE BUILD/eigene Styles
- [ ] B-100 Library-Overlay · - [ ] B-101 Settings · - [ ] B-102 Preset-Manager · - [ ] B-105 Dialoge · - [ ] B-103 ADD PART · - [ ] B-104 ADD STYLE
- [ ] B-23 Onset · - [ ] B-24 Key-Detection · - [ ] B-26 Cache · - [ ] B-27 Hintergrund-Job
- [ ] B-37/B-110 Offline-Render · - [ ] B-111 Stems · - [ ] B-112/B-114 Dateien+WAV · - [ ] B-113 Datei-Drop
- [ ] B-120 KEY TRACK · - [ ] B-121 P-Flag · - [ ] B-122 KEY-Chip · - [ ] B-123 MIDI-Modi · - [ ] B-124 Detektionsgüte
- [ ] B-50 Gaps · - [ ] B-51 MOVE CRUNCH · - [ ] B-52 STRETCH · - [ ] B-53 STRETCH FADES · - [ ] B-54 pinned · - [ ] B-55 Anker · - [ ] B-56 PHASE ALIGN · - [ ] B-57 LAST PASS · - [ ] B-58 RESULT LINE
- [ ] B-87 Regler-Komfort · - [ ] B-89 Resize/HiDPI · - [ ] B-133 Crash-Härtung · - [ ] B-131 Preset-Konverter
- [ ] B-130 Parity · - [ ] B-134 Installer · - [ ] B-135 Doku

---

## 11. Startauftrag (das ist der erste Prompt an OpenCode)

```text
Lies die Datei "Bass Forge Kick Architect - OPENCODE-NACHTPLAN.md" vollstaendig.
Du bist der Lead dieses Laufs. Arbeite die Warteschlange (Abschnitt 4) Welle fuer
Welle ab:

1. Lege docs/NIGHT-LOG.md an und trage den Start ein.
2. Pruefe zuerst das Ausgangs-Gate (Abschnitt 6, Debug+Release) und notiere die
   Testzahl. Ist es rot, brich ab und schreibe den Grund in REPORT.md.
3. Fuer jede Welle: Vertraege schreiben, Teilaufgaben PARALLEL an Sub-Agenten
   geben (Abschnitt 5, Tier A, dann B), Ergebnisse einsammeln, gemeinsame
   Dateien (PluginProcessor/PluginEditor/KickPage/CMakeLists) selbst anpassen,
   Gate fahren, committen (Format aus 1.3/DoD), pushen, CI pruefen, in dieser
   Datei abhaken, eine Zeile ins NIGHT-LOG.
4. Bei rotem Gate: maximal zwei Reparaturversuche, sonst Welle zurueckrollen,
   Tickets als blocked markieren, weiter mit der naechsten unabhaengigen Welle.
5. Niemals: git/CMake/Tests an Sub-Agenten delegieren, Tests loeschen, fremden
   Content einbauen, neue Abhaengigkeiten, mit rotem Build weiterarbeiten.
6. Am Ende: Release-Build, volle Suite (Debug+Release), docs/PROGRESS.md,
   REPORT.md, Handoff-Datei auf dem Desktop, Abschnitt 10 vollstaendig abhaken.

Melde mir am Ende in hoechstens 30 Zeilen: was fertig ist, was blocked ist (mit
Grund), die neuen Testzahlen, und die fuenf menschlichen Punkte aus Abschnitt 8.
```

---

## 12. Notfall-Knopf

Wenn der Lauf entgleist (Build dauerhaft rot, Modelle liefern Unbrauchbares,
Kosten laufen weg): **eine** Zeile genügt als Abbruchregel, die der Agent
jederzeit selbst anwenden darf —

> Nach drei fehlgeschlagenen Wellen hintereinander: sofort stoppen, alles
> Unfertige zurückrollen (`git restore .` + `git clean -fd` für neue Dateien),
> `REPORT.md` mit dem letzten grünen Commit-Hash schreiben und aufhören.

Der letzte grüne Stand bleibt damit immer benutzbar.
