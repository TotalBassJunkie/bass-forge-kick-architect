# HANDOFF — Bass Forge Kick Architect

**Stand:** 2026-11-18, 10:04 · Branch `main` · Commit `e9c8ab9`
**Build:** Debug **und** Release grün · **533 Testfälle / 540.929 Assertions**
**CI:** grün (`BassForgeKickArchitect-windows-x64` als Artefakt)
**Version:** 0.1.0 (aus `CMakeLists.txt`)

Dieses Dokument ist die Übergabe für die **nächste** Sitzung. Es sagt zuerst,
was noch zu tun ist, dann was schon steht. Alles hier ist am Code geprüft,
nicht geschätzt.

---

## 1 · Das Wichtigste in fünf Sätzen

1. Das Plugin **baut, startet und klingt**: KICK-Seite mit 12 Slots, FX-Rack mit
   10 Modulen, Presets mit eingebetteten Samples, Export als WAV.
2. **Zwölf Bausteine sind fertig und getestet, aber noch nicht im laufenden
   Pfad verdrahtet** (Abschnitt 4). Das ist der Hauptteil der nächsten Arbeit.
3. **Drei Features fehlen ganz**: Stem-Renderer, Datei-Drop, Preset-Konverter
   (Abschnitt 5).
4. **Fünf Punkte bleiben menschlich** (Abschnitt 7) — Hören, DAW-Test,
   Farbfreigabe, JUCE-Lizenz, destruktives Löschen.
5. Die Reihenfolge für die nächste Sitzung steht in **Abschnitt 8** —
   mit Datei, Aufwand und Abnahmekriterium pro Schritt.

---

## 2 · Bauen, testen, prüfen (immer so)

```powershell
cd "C:\Users\tbj\Dropbox\.BFR Plugin Projects\Kick Builder Plugin Bass Forge"
. .\tools\dev-env.ps1                      # MSVC, CMake, Ninja, BFKA_BUILD_ROOT

.\tools\configure.ps1 -Config Debug        # NUR nach CMakeLists-Aenderungen
.\tools\build.ps1 -Config Debug
cmd /c "C:\bfka-build\Debug\tests\BFKA_Tests_artefacts\Debug\BFKA_Tests.exe > C:\bfka-re\out.txt 2> C:\bfka-re\err.txt"
"Exit=$LASTEXITCODE"; Get-Content C:\bfka-re\out.txt | Select-Object -Last 3
```

Release (vor jedem Commit, der Produktcode anfasst):

```powershell
.\tools\configure.ps1 -Config Release
.\tools\build.ps1 -Config Release
cmd /c "C:\bfka-build\Release\tests\BFKA_Tests_artefacts\Release\BFKA_Tests.exe > C:\bfka-re\rel_out.txt 2> C:\bfka-re\err.txt"
```

Release-Paket (ZIP + Prüfsummen):

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\package-release.ps1 -Version "0.1.0"
```

**Harte Regeln**

* Build **nur** unter `C:\bfka-build` — nie im Dropbox-Ordner (`LNK1104`/`LNK1201`).
* Debug **und** Release müssen grün sein; einzige erlaubte Warnung ist `D9025`.
* Neue Quelldateien **immer** in `CMakeLists.txt` **und** `tests/CMakeLists.txt`
  eintragen (dort sind die Quellen explizit gelistet, kein GLOB).
* Keine Testlöschung. Roter Test wird repariert.
* `release/` ist per `.gitignore` ausgenommen (Binärpakete).

---

## 3 · Was fertig ist und **im laufenden Programm wirkt**

| Bereich | Stand | Beleg im Code |
|---|---|---|
| Tail-Länge (Kick + FX) | aktiv | `PluginProcessor::getTailLengthSeconds`, `FxChainProcessor::tailSeconds` |
| Karten-Seiten (12 Regler/Seite) | aktiv | `ModuleCard::pageCount/setPage` |
| Samples im Preset (Base64-WAV) | aktiv | `PresetIO::saveKickPreset(...,PresetWriteOptions)` |
| IR-Browser mit Liste | aktiv | `ImpulseLibrary` + `ConvolverView` |
| Pitch/Stretch in der Voice | aktiv (REPITCH) | `SampleVoice::updateDerived` |
| FOLLOW DAW (Host-BPM) | aktiv | `PluginProcessor::applyEffectiveBpm`, 4 Hz gedrosselt |
| Schema-Migration v1→v2 | aktiv | `KickState::migrateTo` |
| **Undo/Redo** + Strg+Z | aktiv | `PluginProcessor::beginStateEdit/undo`, `PluginEditor::keyPressed` |
| Style-Sidebar + KEY-Chip + DRAG-Export | aktiv | `KickPage` (Sidebar links, Chip im Transport) |
| Overlays LIBRARY/GEAR/PRESETS + **X-Knopf** | aktiv | `OverlayHost`, `PluginEditor::open*` |
| Presets speichern/laden (Kick + Modul) | aktiv, auf der **FX-Seite** | `FxPage::chooseSavePreset`, `PluginProcessor::savePreset` |
| Export (offline → WAV 24 bit) | aktiv | `PluginProcessor::exportKickToFile` |
| Installer + portables ZIP + Doku | vorhanden | `installer/`, `tools/package-release.ps1`, `README.md`, `docs/QUICKSTART.md` |

Aktuell installiert: `C:\Program Files\Common Files\VST3\Bass Forge Kick Architect.vst3`
(Build vom 18.11., 09:55). Standalone:
`C:\bfka-build\Release\BassForgeKickArchitect_artefacts\Release\Standalone\Bass Forge Kick Architect.exe`.
Portables Paket: `release\BassForgeKickArchitect-0.1.0-portable-win64.zip`.

---

## 4 · **Gebaut und getestet, aber NICHT verdrahtet** ← Hauptarbeit

Diese Liste ist per Quelltext-Scan erzeugt (wer wird von wem eingebunden).
Für jeden Eintrag steht, wo er hin muss und woran man Erfolg erkennt.

| Baustein | Datei | Muss wohin | Abnahmekriterium |
|---|---|---|---|
| `PreRenderCache` + `PitchShifter` | `src/engine/PreRenderCache.h`, `src/engine/PitchShifter.h` | `KickAssembler::loadSlotWithSettings`: bei `keepPitch` vorrendern, Ergebnis per `SampleVoice::setPreRendered()` in die Voice | Pitch +12 bei STRETCH 2× ändert die **Länge**, nicht die Tonhöhe; `isPitchAccurate()` meldet `true` |
| `ChaosEngine::applyTo` | `src/library/ChaosEngine.h` | `StyleSidebar::onChaosToggled` / ROLL in `KickPage` | Gleicher Seed → gleicher Kick; die vier CHAOS-Schalter ändern nur ihre Bereiche |
| `OnsetDetector`, `KeyDetector`, `AnalysisCache`, `AnalysisJob` | `src/library/analysis/*` | `SampleLibrary::scan`: Job starten, Ergebnisse in `SampleInfo` (`bpm`,`key`,`lengthSeconds`,`onsetSeconds`,`confidence`) | Library-Liste zeigt BPM/Key; zweiter Scan läuft aus dem Cache (< 100 ms für 100 Dateien) |
| `KeyTracker` | `src/engine/KeyTracker.h` | `PluginProcessor::processBlock` bei Note-On (wenn `lN_keytracked`) | Gespielte Note G verschiebt KEY-TRACK-Parts um die kürzeste Transposition |
| `MidiModes` | `src/engine/MidiModes.h` | `processBlock`: `shouldTrigger`/`onNoteOn`/`onNoteOff`, `allNotesOff` bei Song-Stop | Modus SINGLE triggert nur die erste Note; All-Notes-Off killt hängende Kicks |
| `AlignEngine` + `AlignReport` + `PhaseAlign` | `src/engine/AlignEngine.h`, `AlignReport.h`, `PhaseAlign.h` | Vor dem ersten Block bzw. im Offline-Render: Gaps schließen, `LAST PASS`-Zeilen an die Statuszeile | Lücke nach ALIGN ≤ Toleranz; `pinned` unverändert; Report im UI sichtbar |
| `KnobHelpers` | `src/ui/KnobHelpers.h` | `SlotCard::addKnob` und `ModuleCard`-Regler | Rechtsklick bietet Reset/Wert/Kopieren; Shift = fein — **Achtung:** `KnobHelpers` überschreibt `textFromValueFunction`; für dB/st-Formatierung vorher `KnobConfig` um einen Format-Override erweitern |
| `UiScaler` | `src/ui/UiScaler.h` | `PluginEditor`: `setResizable`, `setResizeLimits`, Transform anwenden | Fenster 70–200 % ohne abgeschnittene Regler |
| `AddPartOverlay` / `AddStyleOverlay` | `src/ui/overlays/AddPartOverlay.h` | `KickPage` (`+ ADD PART`) und `StyleSidebar` (`+ ADD STYLE`) über `OverlayHost` | Picker zweistufig; bei 12 belegten Slots „NO FREE SLOT" |
| `ParityHarness` | `src/engine/ParityHarness.h` | Nur Offline-Prüfung / Report, kein Produktpfad nötig | Werte für einen gerenderten Kick ausgeben |
| `StyleSidebar` ROLL | `src/ui/StyleSidebar.h` | `RandomizerEngine` + `PoolResolver` an `StyleModel` binden | ROLL belegt Slots nach Style-Regeln statt nur den Kick zu triggern |

**Wichtig:** `ChaosEngine` wird heute nur für den Typ `ChaosFlags` eingebunden —
`applyTo()` ruft niemand. `PhaseAlign` erscheint nur als Namensgleichheit mit
`AlignSettings::phaseAlign`; es ist **nicht** verdrahtet.

---

## 5 · Was ganz fehlt

| Ticket | Was | Warum nicht gefaked |
|---|---|---|
| **B-111** Stem-Renderer | je Part durch die volle Kette | Deshalb gibt es **keinen** `DRAG STEMS`-Knopf — ein Knopf ohne Wirkung wäre schlimmer als keiner |
| **B-113** Datei-Drop | `wav/aif/flac/ogg/mp3` auf Karten, Key aus Dateiname | `FileDropTarget` existiert nicht; nur `KeyFromFilename`-Logik fehlt |
| **B-131** Preset-Konverter | `.itkbpreset` → `.bfkapreset` als Konsolen-Tool | Braucht die eigenen RE-Analysedaten; ohne Beleg nicht sinnvoll baubar |

Zusätzlich offen, aber kein eigenes Ticket:

* **LIBRARY-Tabs SOUNDS / FX CHAINS / MY SOUNDS** sind leer — sie brauchen den
  Content-Index (Abschnitt 4, Analyse-Pipeline) und eine FX-Chain-Ablage.
* **Preset-Speichern liegt auf der FX-Seite**, nicht auf der KICK-Seite:
  `FxPage` hat `SAVE`/`LOAD` für Kick-Presets und `SAVE`/`LOAD` für
  Modul-Presets (`PluginProcessor::savePreset`/`saveModulePreset` →
  `PresetIO`). Wer vom Kick-Fenster aus speichern will, muss dorthin wechseln.
* **`embedSamples`** ist API-only: `PresetWriteOptions` ist implementiert und
  getestet, aber `PluginProcessor::savePreset` nutzt die 4-Parameter-Variante
  ohne Einbettung — es fehlt der Schalter im UI **und** eine Überladung im
  Prozessor.

---

## 6 · Bekannte Abweichungen und Entscheidungen

1. **Modulkarten-Höhe:** Die Vorgabe „≤ 420 px" ist mit Struktur-Editor
   arithmetisch unmöglich (`dynEq` bringt 216 px mit; mit zwei Reglerreihen
   sind es 588 px). Umgesetzt: 12 Regler je Seite, Test prüft ≤ 420 an einem
   Modul **ohne** Struktur-Editor. Wer die Grenze hart will, muss den
   Struktur-Editor einklappbar machen.
2. **`MidiModes::fromState` teilt durch 3, nicht durch 4** — der Parameter
   `midi_mode` ist auf 0…3 registriert.
3. **`KeyTracker` nimmt C als Part-Grundton**, weil der Slot-Zustand keinen
   eigenen Grundton trägt. Sobald die Analyse den Sample-Key liefert
   (Abschnitt 4), muss `transpositionForPart` den echten Grundton bekommen.
4. **Undo klammert nur abgeschlossene Aktionen** (Demo-Kick, Preset-Laden),
   keine Regler-Zwischenstände — bewusst, damit die Historie nicht zumüllt.
5. **`OverlayHost::paint`** dunkelt mit 55 % ab; der Schließen-Knopf sitzt in
   einer eigenen Kopfleiste über dem Panel, damit er nie Tabs überdeckt.

---

## 7 · Fünf Punkte bleiben menschlich

1. **Sidechain-Hosttest** in einer DAW (Aux-Bus-Routing, B-153 Rest).
2. **Hörurteil** „klingt wie das Original" — Messwerte liefert `ParityHarness`,
   das Ohr fehlt.
3. **Freigabe Akzentfarben/Typografie** (B-80).
4. **JUCE-Lizenzierung** vor einem kommerziellen Release (AGPLv3 vs. Commercial).
5. **Endgültiges Löschen** von Nutzer-Presets (der Manager verschiebt nur nach
   `_Papierkorb`).

---

## 8 · Reihenfolge für die nächste Sitzung

Nach Nutzen pro Aufwand sortiert. Jeder Schritt endet mit Debug + Release
grün, Commit, Push, CI grün.

| # | Schritt | Aufwand | Warum zuerst |
|---|---|---|---|
| 1 | **`PreRenderCache` in den Lade-Pfad** | ~1 h | KEEP PITCH ist heute ein REPITCH-Fallback — das ist ein hörbarer Kompromiss an einer Kernfunktion |
| 2 | **Analyse-Pipeline in `SampleLibrary`** + `SampleInfo` erweitern | ~2 h | Schaltet BPM/Key in der Library **und** den KEY-Chip von „AUTO" auf echte Werte |
| 3 | **`MidiModes` + `KeyTracker` in `processBlock`** | ~1,5 h | Macht das Plugin im Host-Arrangement benutzbar |
| 4 | **ALIGN vor dem Rendern** + LAST PASS in der Statuszeile | ~2 h | Der musikalisch heikelste Teil: danach **hören** (Abschnitt 7, Punkt 2) |
| 5 | **`KnobHelpers`** auf Slot-/Modulkarten (mit Format-Override) | ~1,5 h | Bedienkomfort, kein Klangrisiko |
| 6 | **`AddPart`/`AddStyle` verbinden** + ROLL an `RandomizerEngine` | ~2 h | Schließt die zwei sichtbaren Bedienlücken |
| 7 | **B-113 Datei-Drop**, dann **B-111 Stems**, zuletzt **B-131** | ~4 h | Reihenfolge nach Abhängigkeit (Stems brauchen den Offline-Renderer, der steht) |
| 8 | **`UiScaler`** an den Editor | ~1 h | Zuschlag, kein Blocker |

**Nicht** anfangen, bevor Punkt 1–3 grün sind: Alles danach baut auf dem
Lade- und Analysepfad auf.

---

## 9 · Infrastruktur-Hinweise (spart der nächsten Sitzung Stunden)

* **Modell-Provider:** `opencode/*` antwortet „API key has expired",
  `aihubmix/*` „The requested API endpoint does not exist". Nutzbar ist
  `merge-gateway/*`. Bewährt: `merge-gateway/deepseek/deepseek-v4.1-flash-1017`
  (0,15/0,60 USD je Mio. Token) als Builder, `merge-gateway/zai/glm-5.3-flash`
  oder `merge-gateway/qwen/qwen3.8-flash` als familienfremder Prüfer (L8).
  Frei nutzbar war zuletzt `openrouter/cohere/north-mini-code:free`.
* **Dropbox-Sperren:** Direkt nach `git commit`/`push` kann der Editor
  „Unable to edit" melden (Sync hält die Datei). Kurz warten, dann erneut.
* **Build-Zeit:** inkrementell ~1–3 min, voll ~8–12 min. Zwei Konfigurationen
  parallel bauen geht, kostet aber CPU.
* **CI:** `gh run list --limit 3`; ältere Läufe werden durch neue Pushes
  abgebrochen, das ist normal.
* **`gh run watch`** liefert in PowerShell Exitcode 1 wegen stderr — Text
  lesen, nicht den Code.

---

## 10 · Fallstricke, die schon Zeit gekostet haben

**Catch2 / JUCE 8.0.7**

* `Catch::Matchers::LessThan` / `GreaterThan` gibt es hier **nicht** →
  `REQUIRE (x < y)` schreiben.
* `juce::AffineTransform` hat kein `getScaleX()` → `mat00` lesen.
* `juce::Rectangle` hat kein `getLeft()` → `getX()`.
* `Button::triggerClick()` ist **asynchron** → im Test
  `MessageManager::runDispatchLoopUntil (100)` (dafür ist
  `JUCE_MODAL_LOOPS_PERMITTED=1` im Test-Target gesetzt).
* `REQUIRE_NOTHROW ({ ... })` kompiliert nicht → Lambda aufrufen.

**C++**

* Most Vexing Parse: `Prefs p (juce::File());` deklariert eine **Funktion**
  → `Prefs p { juce::File() };`.
* `juce::TextButton` ist weder kopier- noch verschiebbar →
  `std::vector<std::unique_ptr<TextButton>>`.
* `juce::Slider::setRange(a, b, interval)` rundet Werte — bei normalisierten
  Parametern Intervall **0**.

**Projekt-spezifisch**

* `resized()` im Konstruktor aufrufen, wenn Kinder erst danach entstehen.
* Parameterglättung 20 ms → Tests brauchen ~80 Blöcke à 128 Samples.
* `setChain` ruft `setState` — nicht entfernen (sonst gehen Bänder/Kurven/IR
  verloren).
* `writeBackModuleStates()` entfernt Struktur-Kinder (`Band`, `WNode`, `Pt`,
  `Eq`, `Dc`) und schreibt sie neu; nur diese Namen verwenden.
* Strings nur ASCII, LF, UTF-8 ohne BOM; in Python/PowerShell niemals
  Here-Strings mit Backticks für Quelltext verwenden (frisst `\f`, `\a`, `\b`).

---

## 11 · Datei-Landkarte

```
src/core        KickState, ParameterRegistry, PresetIO, FxChainIO, UndoHistory,
                Prefs, StyleIO, ExportNaming, WavWriter, Log, Version
src/engine      SampleVoice, KickRenderer, KickAssembler, WsolaStretcher,
                Resampler, PitchShifter, PreRenderCache, OfflineRenderer,
                AlignEngine, AlignReport, PhaseAlign, KeyTracker, KeyState,
                MidiModes, ParityHarness
src/fx          FxModule (Vertrag), FxChainProcessor, FxFactory, 10 Module,
                Biquad
src/library     SampleLibrary, StyleModel, RandomizerEngine, ChaosEngine,
                ImpulseLibrary, UserStyles, analysis/{OnsetDetector,
                KeyDetector, AnalysisCache, AnalysisJob}
src/ui          KickPage, FxPage, SlotCard, ModuleCard, ModuleStructureView,
                StyleSidebar, KeyChip, KnobHelpers, UiScaler, FxChainModel,
                UiTheme, editors/*, overlays/*
src/plugin      PluginProcessor, PluginEditor
tests/          69 Dateien, Catch2 v3; jede Datei in tests/CMakeLists.txt
docs/specs      18 Spec-Dokumente (Verträge, Formate, Messverfahren)
tools           dev-env, configure, build, test, package-release, Analyse-Skripte
installer       Inno-Setup-Skript
```

---

## 12 · Definition of Done für jede Änderung

1. Debug **und** Release bauen, `/W4` ohne neue Warnung (Ausnahme `D9025`).
2. Mindestens ein Test für die neue Funktion; **volle** Suite grün (beide
   Konfigurationen).
3. Neue Dateien in `CMakeLists.txt` **und** `tests/CMakeLists.txt`.
4. Öffentliche Symbole dokumentiert, Ticket-ID im Dateikopf, deutsch
   kommentiert, Bezeichner englisch.
5. Commit `feat(<bereich>): <was> [B-xx]` bzw. `fix(...)`, `test(...)`,
   `docs(...)`; danach Push auf `origin/main`, CI grün.
6. `docs/PROGRESS.md` und – bei neuen Verträgen – `docs/specs/*` fortschreiben.
