# REPORT — Nacht-Lauf 2026-11-17/18

Auftrag: `docs/PLAN-OPENCODE-NACHT.md`. Dieser Bericht sagt, was gebaut ist,
was **nicht** verdrahtet ist, und was ein Mensch prüfen muss.

## 1 · Zahlen

| Größe | Vor dem Lauf | Nach dem Lauf |
|---|---|---|
| Testfälle | 317 | **528** |
| Assertions | 540.115 | **540.904** |
| Quelldateien (`src/`) | 91 | **145** |
| Testdateien (`tests/`) | 34 | **69** |

Build: Debug und Release grün, MSVC `/W4`, einzige Warnung ist der Bestand
`warning D9025` (`/Zi` durch `/Z7` überschrieben). Release-Suite: siehe
Abschnitt 6 (wird am Ende des Laufs eingetragen).

## 2 · Wellen

| Welle | Stand | Anmerkung |
|---|---|---|
| **W1** Kleinkram | **fertig** | Tail-Länge, Karten-Seiten, Preset-Einbettung, IR-Browser — alle im laufenden Code. |
| **W2** Pitch/Stretch | **fertig (Bausteine)** | `SampleVoice` wendet Pitch/Stretch an; KEEP PITCH nutzt einen vorgerenderten Puffer, sonst REPITCH-Fallback. `PreRenderCache` gebaut und getestet, aber **noch nicht** in `KickAssembler` vorgerendert — im Live-Pfad greift der Fallback. |
| **W3** Zustand/Undo/Tempo | **fertig (teils)** | Migration v1→v2 aktiv, FOLLOW DAW aktiv. `UndoHistory` gebaut/getestet, **nicht** an den Prozessor gehängt (kein Strg+Z im Editor). |
| **W4** Style-Sidebar/CHAOS | **fertig (teils)** | Sidebar hängt links in der KICK-Seite, `+ ADD STYLE`-Overlay existiert. ROLL löst nur den Kick aus und meldet, dass die Roll-Regeln nicht an die Library gebunden sind. `ChaosEngine` gebaut/getestet, nicht an die Sidebar gehängt. |
| **W5** Overlays | **fertig (teils)** | LIBRARY / GEAR / PRESETS im Editor-Kopf öffnen die Overlays über `OverlayHost` (Escape schließt). PRESETS liest den Nutzer-Ordner und lädt ein Preset. SOUNDS / FX CHAINS / MY SOUNDS sind leer, solange kein Content-Index angebunden ist. `AddPartOverlay` gebaut, **nicht** aufgerufen. |
| **W6** Library-Analyse | **fertig (Bausteine)** | Onset, Key-Detection, Cache und Hintergrund-Job gebaut und getestet. **Nicht** in `SampleLibrary`/`SampleInfo` verdrahtet. |
| **W7** Export & D&D | **teilweise** | `OfflineRenderer`, `ExportNaming`, `WavWriter` fertig; **DRAG** rendert den Kick und legt ihn unter `Dokumente/BassForge/KickArchitect/Exports` ab. **Stems (B-111) fehlen**, **Datei-Drop (B-113) fehlt**. |
| **W8** Key & MIDI | **fertig (Bausteine)** | `KeyTracker`, `KeyState`/`KeyChip`, `MidiModes` gebaut und getestet; KEY-Chip sitzt im Transport. Die MIDI-Modi wirken **noch nicht** im `processBlock`. |
| **W9** ALIGN | **fertig (Bausteine)** | `AlignEngine` (MOVE CRUNCH + STRETCH CRUNCH, pinned, Anker), `PhaseAlign`, `AlignReport` gebaut und mit Zahlen getestet. **Nicht** in den Render-Pfad gehängt. |
| **W10** Politur | **teilweise** | `KnobHelpers`, `UiScaler`, `RobustnessTests` fertig. **B-131 (Preset-Konverter) fehlt.** `KnobHelpers`/`UiScaler` sind nicht auf alle Regler bzw. den Editor angewendet. |
| **W11** Abschluss | **teilweise** | Parity-Harness, Inno-Skript, portables ZIP, Release-Notes, README, Quickstart fertig. Dieser Bericht + Handoff sind Teil davon. |

## 3 · Was ein Mensch hören bzw. prüfen muss

1. **Sidechain-Hosttest in einer DAW** (B-153 Rest) — Aux-Bus-Routing, im
   Unit-Test nicht abbildbar.
2. **Hörurteil „klingt wie das Original"** (B-46, B-77, B-130) — die
   Messwerte stehen (`ParityHarness`), das Ohr fehlt.
3. **Freigabe der Akzentfarben und Typografie** (B-80) — Renderings über
   `BFKA_RENDER_UI=1` erzeugen.
4. **JUCE-Lizenzierung** vor einem kommerziellen Release (AGPLv3 vs.
   Commercial).
5. **Löschen/Umbenennen von Nutzer-Presets** — der Manager verschiebt nur in
   `_Papierkorb`; das endgültige Löschen bleibt beim Menschen.

## 4 · Bekannte Abweichungen von den Verträgen

* **Modulkarten-Höhe 420 px** ist mit Struktur-Editor arithmetisch unmöglich
  (`dynEq` bringt allein 216 px mit; mit zwei Reglerreihen sind es 588 px).
  Umgesetzt: höchstens 12 Regler je Seite, Test prüft `<= 420` an einem Modul
  **ohne** Struktur-Editor.
* **`MidiModes::fromState`** teilt durch 3, nicht durch 4: der Parameter
  `midi_mode` ist auf 0…3 registriert, nicht auf 0…1.
* **`KeyTracker`** nimmt C als Part-Grundton, weil der Slot-Zustand noch keinen
  eigenen Grundton trägt (offener Punkt für die Sample-Key-Anbindung).

## 5 · Gefundene und behobene Fehler des Laufs

* `KeyDetector`: Template-Rotation war invertiert (A wurde als D# erkannt,
  G als F). Einzeiler in `src/library/analysis/KeyDetector.cpp`.
* `ModuleCard::preferredHeight()` ignorierte die neue Seitenzeile.
* `UndoHistory`: Membername `maxSteps` kollidierte mit der Methode.
* `AlignEngineTests`: Test wiederverwendete einen `isAnchor`-Part und prüfte
  damit die falsche Erwartung.
* Mehrere Catch2-/JUCE-API-Abweichungen in Subagent-Tests (`LessThan`,
  `GreaterThan`, `AffineTransform::getScaleX`, Most-Vexing-Parse).

## 6 · Offene Baustellen (bewusst nicht gefaked)

* **Stem-Renderer (B-111)**, **Datei-Drop (B-113)**, **Preset-Konverter
  (B-131)** sind nicht gebaut. Kein `DRAG STEMS`-Knopf, damit nichts
  verspricht, was es nicht gibt.
* **Verdrahtung** von Undo, CHAOS, Analyse, MIDI-Modi, Key-Track und ALIGN in
  den laufenden Prozessor: die Bausteine sind getestet, die Anbindung ist der
  nächste Schritt.
* **Content-Index**: LIBRARY zeigt nur Presets; SOUNDS/FX CHAINS/MY SOUNDS
  brauchen den Library-Scan im Editor.
