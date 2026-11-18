# Bass Forge Kick Architect

Bass Forge Kick Architect baut aus bis zu zwoelf Sample-Schichten einen fertigen
Kick fuer Techno und Hardstyle: jeder Slot hat Volume, Pitch, Stretch, Trim und
Fades, die Schichten werden zu einer Summe gelegt und laufen danach durch ein
frei bestueckbares FX-Rack mit zehn Modultypen. Ausgeliefert wird das Ganze als
VST3-Instrument und als Standalone-Anwendung fuer Windows x64 (C++20, JUCE 8).

---

## Status

**Version 0.1.0** (aus `CMakeLists.txt`, `project(... VERSION 0.1.0)`).

**Fertig und im Build**

* Build lokal und in CI: VST3 + Standalone + `BFKA_Tests.exe`, MSVC `/W4`.
* Datenmodell: `KickState` mit 12 Slots, Rollen, Locks, KEY-TRACK und FX-Kette;
  `ParameterRegistry` mit **364** Host-Parametern (12 x 20 Slot + 12 Global +
  8 x 14 FX-Proxy).
* KICK-Seite: 12 Slot-Karten (Mini-Wellenform, Trim-Fenster, Fade-Marken),
  Visualiser mit SHOW PARTS / SUM, Transport mit PLAY, LOOP, DEMO-KICK,
  BPM- und MASTER-Leiste, Statuszeile.
* Engine: `SampleVoice` mit Trim, fuenf Fade-Kurven und Declick-Rampe;
  `KickRenderer` (Solo-Logik, Auto-Gain, Kick-Laenge); Pitch/Stretch wirken
  (REPITCH per Resampling, KEEP PITCH ueber vorgerenderten Puffer mit
  `isPitchAccurate()` als Fallback-Melder); eigene WSOLA-Engine,
  `PitchShifter`, `PreRenderCache`.
* FX-Rack: alle **zehn** Modultypen echt (`filt`, `eq`, `dist`, `trans`,
  `ott`, `reverb`, `dynEq`, `mseq`, `shaper`, `final`), freie Reihenfolge,
  MIX/OUT/ENABLE je Modul, Latenzausgleich, externer Sidechain fuer `dynEq`,
  sechs Graph-Editoren, CPU-Profil je Modul.
* Presets: `.bfkapreset` fuer Kick und Modul, A/B-Vergleich, Undo-Historie
  (50 Schnappschuesse, als Baustein), IR-Browser fuer den Convolver.
* Export: Offline-Render inklusive FX-Kette, WAV-Schreiben (24 bit) mit
  Kollisionsschutz.
* Overlays: Library, Settings (`prefs.json`), Preset-Manager und das
  Dialog-System ohne modalen Loop sind an die Kopfknoepfe angeschlossen.
  `AddPartOverlay` und `AddStyleOverlay` sind gebaut, aber noch nicht
  aufgerufen.
* Tests: vollstaendige Suite in Debug und Release gruen (Zahlen unten).

**Offen**

* Die **fuenf menschlichen Punkte** (nicht automatisierbar, siehe
  `docs/PLAN-OPENCODE-NACHT.md`, Abschnitt 8): Sidechain-Hosttest in einer DAW,
  Hoerurteil gegen das Original, Freigabe von Akzentfarben und Typografie,
  JUCE-Lizenzierung vor einem kommerziellen Release, Loeschen/Umbenennen von
  Nutzer-Presets.
* **Stem-Renderer fehlt.** Es gibt nur den Master-Export (`DRAG` rendert die
  Summe und legt sie in `Exports` ab). Ein `StemRenderer` (je Part durch die
  volle Kette) existiert im Quellbaum nicht und ist nicht angebunden.
* **Roll-Regeln sind nicht an die Library gebunden.** `StyleSidebar` zeigt
  Styles und CHAOS-Schalter, `ROLL` zaehlt nur mit und meldet
  "Style-Regeln noch nicht an die Library gebunden"; `StyleModel` und
  `UserStyles` werden der Sidebar nicht uebergeben, `RandomizerEngine` und
  `ChaosEngine` laufen nur in Tests.
* ALIGN/PHASE ALIGN, KEY-TRACK, MIDI-Modi, Undo und `FOLLOW DAW` liegen als
  getestete Bausteine vor, sind aber noch nicht in Prozessor und UI verdrahtet.
* Das Einbetten von Samples ins Preset ist als `PresetWriteOptions` vorhanden
  und getestet, aber im Speichern-Dialog noch nicht als Schalter auswaehlbar.
* Kein VST2, kein AAX, keine macOS-Version; keine mitgelieferten Samples und
  keine Factory-Presets.

---

## Schnellstart

```powershell
cd "C:\Users\tbj\Dropbox\.BFR Plugin Projects\Kick Builder Plugin Bass Forge"
. .\tools\dev-env.ps1
.\tools\configure.ps1 -Config Release
.\tools\build.ps1 -Config Release
.\tools\test.ps1 -Config Release
```

Der Build laeuft **nur** unter `C:\bfka-build\<Config>` (nicht im
Dropbox-Ordner, sonst sperrt der Sync Dateien beim Linken). Die Standalone-
Anwendung liegt danach unter
`C:\bfka-build\Release\BassForgeKickArchitect_artefacts\Release\Standalone\`.
Schritt fuer Schritt mit Fehlersuche: [`docs/QUICKSTART.md`](docs/QUICKSTART.md).

---

## Projektstruktur

| Ordner | Inhalt |
|---|---|
| `src/core` | `KickState`, `ParameterRegistry`, `PresetIO`, `FxChainIO`, `StyleIO`, `Prefs`, `UndoHistory`, `WavWriter`, `ExportNaming`, `Version`, `Log` |
| `src/engine` | `SampleVoice`, `KickRenderer`, `KickAssembler`, `WsolaStretcher`, `PitchShifter`, `PreRenderCache`, `Resampler`, `AlignEngine`, `PhaseAlign`, `AlignReport`, `KeyTracker`, `KeyState`, `MidiModes`, `OfflineRenderer`, `ParityHarness` |
| `src/fx` | `FxModule`-Schnittstelle, `FxFactory`, `FxChainProcessor`, die zehn Module unter `modules/`, `dsp/Biquad.h` |
| `src/library` | `SampleLibrary` (Index, Ordnertabelle, Content-Root), `StyleModel`, `RandomizerEngine`, `ChaosEngine`, `UserStyles`, `ImpulseLibrary`, Analyse unter `analysis/` (Onset, Key, Cache, Job) |
| `src/ui` | `KickPage`, `SlotCard`, `FxPage`, `ModuleCard`, `StyleSidebar`, `KeyChip`, `FxChainModel`, `UiScaler`, `KnobHelpers`, `UiTheme`, `editors/` (Graph-Editoren), `overlays/` |
| `src/plugin` | `PluginProcessor` (APVTS, Renderer, FX-Kette, Busse, Presets) und `PluginEditor` (Rahmen, Seiten, Overlay-Host) |
| `tests` | Catch2-v3-Suite, ein Testziel `BFKA_Tests` mit 71 Quelldateien (davon 69 Testdateien) |
| `tools` | `dev-env.ps1`, `configure.ps1`, `build.ps1`, `test.ps1`, `package-release.ps1`, Analyse-Skripte (`mine_presets.py`, `strings_extract.py`, ...) |
| `installer` | `BassForgeKickArchitect.iss` (Inno Setup, VST3 + Standalone, portables ZIP ueber `tools/package-release.ps1`) |
| `docs` | Bestandsaufnahme, Masterplan, RE-Befunde, Spezifikationen unter `specs/`, `PROGRESS.md`, `NIGHT-LOG.md`, `RELEASE-NOTES.md`, `LICENSE-THIRD-PARTY.md`, `PLAN-OPENCODE-NACHT.md` |

---

## Tests

```powershell
. .\tools\dev-env.ps1
.\tools\test.ps1 -Config Debug      # baut BFKA_Tests und fuehrt die Suite aus
```

Das Test-Binary liegt unter
`C:\bfka-build\Debug\tests\BFKA_Tests_artefacts\Debug\BFKA_Tests.exe`
(Release entsprechend unter `Release\...`). Einzelne Gruppen in PowerShell
quotieren: `...\BFKA_Tests.exe '[ui]'`.

Stand aus `docs/PROGRESS.md` (Nacht-Lauf, 2026-11-18): **504 Testfaelle**,
**540.847 Assertions**, Debug gruen. Release ist mit **439 Testfaellen** gruen
gemessen; danach kamen nur Testdateien dazu.

---

## Mitwirken und Regeln

Drei Konventionen, die sonst Zeit kosten (vollstaendig in
[`docs/PLAN-OPENCODE-NACHT.md`](docs/PLAN-OPENCODE-NACHT.md), Abschnitt 1):

1. **String-Literale nur ASCII** (`ae/oe/ue/ss`). MSVC liest die Quellen ohne
   `/utf-8` in der Systemcodepage; Umlaute in Strings werden zu Mojibake.
   Kommentare duerfen Umlaute enthalten.
2. **Zeilenenden LF, UTF-8 ohne BOM.** Kein `Set-Content` und kein
   `write_text` ohne `newline="\n"`.
3. **Dateikopf mit Zweck und Ticket-ID** (`B-xx`), oeffentliche Symbole
   dokumentiert, kein `TODO` ohne Ticket-ID.

Weiter gilt: Build nur unter `C:\bfka-build`, keine neuen Abhaengigkeiten
(JUCE 8.0.7 plus Standardbibliothek), kein fremder Content, keine Test-
Loeschung. Neue Quelldateien traegt der Lead in `CMakeLists.txt` bzw.
`tests/CMakeLists.txt` ein.

---

## Lizenz

Der Quellcode steht unter der **GNU General Public License v3.0** (siehe
`LICENSE`). Die Laufzeitumgebung bringt eigene Bedingungen mit:

* **JUCE 8** (gepinnt auf `8.0.7`) wird von JUCE Ltd. unter AGPLv3, GPLv3 oder
  einer kommerziellen/Personal-Lizenz angeboten.
* **VST3 SDK** dual GPLv3 oder Steinberg-VST3-Lizenz; **Catch2** unter Boost
  Software License 1.0.

Details, inklusive der bewusst nicht verwendeten Bibliotheken (Rubber Band,
SoundTouch, Elastique) und des Netzwerkverhaltens:
[`docs/LICENSE-THIRD-PARTY.md`](docs/LICENSE-THIRD-PARTY.md).

**Offene Entscheidung:** Welches JUCE-Modell gilt, ist vor einem kommerziellen
Release zu klaeren und dort einzutragen. Bis dahin ist das Plugin nur unter
GPLv3/AGPLv3-Bedingungen weitergebbar.
