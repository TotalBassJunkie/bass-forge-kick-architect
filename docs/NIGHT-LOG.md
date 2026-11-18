# Nacht-Lauf 2026-11-17 — OpenCode-Protokoll

Neueste Zeile unten. Format: `HH:MM <Welleneintrag>`.

```
21:10 Uebergabe laut Plan. Branch main, letzter Commit 3967487 (docs: Nachplan).
21:12 Ausgangs-Gate geprueft: Debug-Build "no work to do", Debug-Suite gruen
      (317 Testfaelle / 540115 Assertions), Release-Build + Release-Suite gruen.
      CI-Lauf 44167242991 (docs: Nachplan) noch in Arbeit, Vorgaenger gruen.
      Ausgangslage ist damit gruen — kein Abbruch nach Plan Abschnitt 11.2.
21:14 Modell-Routing festgelegt: Builder = opencode/deepseek-v4.1-flash-free
      (kostenlos, DeepSeek-Familie), Pruefer = opencode/glm-5.3-flash-free
      (andere Familie, kostenlos). Tier B bei DSP: merge-gateway/qwen/qwen3.8-flash
      nur wenn Tier A zweimal scheitert.
21:16 W1 gestartet (4 Tasks parallel, Tier A).
22:40 W1 Gate gruen | Debug+Release gebaut, 333 Testfaelle / 540185 Assertions
      (vorher 317 / 540115, also +16 Faelle) | B-150, B-151, B-95, B-152 fertig.
      Abweichung dokumentiert: Modulkarten-420-px gilt nur ohne Struktur-Editor
      (dynEq = 588 px, arithmetisch unvermeidbar); Test prueft die Grenze an
      "dist" plus Seitenanzahl an dynEq.
23:40 Modell-Routing korrigiert: opencode/* (Key abgelaufen) und aihubmix/*
      ("endpoint does not exist") sind ausgefallen. Builder laeuft auf
      merge-gateway/deepseek/deepseek-v4.1-flash-1017 (0,15/0,60 USD je M),
      freies Modell nur noch openrouter/cohere/north-mini-code:free.
23:41 Wellen 2-6, 7.1, 7.3, 8.1-8.3, 9.1, 9.4, 9.5, 10.1-10.3, 4.1-4.3, 5.1-5.5
      gruen | 504 Testfaelle / 540847 Assertions (vorher 333 / 540185).
      Neu: PitchShifter, PreRenderCache, QualityMetrics, UndoHistory, ChaosEngine,
      StyleSidebar, StyleIO/UserStyles, OverlayHost, LibraryOverlay,
      SettingsOverlay+Prefs, PresetManagerOverlay, AddPartOverlay/AddStyleOverlay,
      OnsetDetector, KeyDetector, AnalysisCache, AnalysisJob, KeyTracker,
      KeyState/KeyChip, MidiModes, PhaseAlign, AlignEngine, AlignReport,
      OfflineRenderer, ExportNaming, WavWriter, KnobHelpers, UiScaler,
      ParityHarness, RobustnessTests.
      Lead-Korrekturen: KeyDetector-Template-Rotation war invertiert (A->D#,
      G->F), Kartenhoehe um die Seitenzeile, UndoHistory-Namenskollision
      maxSteps, mehrere Catch2/JUCE-API-Abweichungen in Subagent-Tests.
04:40 UI-Integration: Sidebar links in der KICK-Seite, KEY-Chip im Transport,
      DRAG-Export als WAV, OverlayHost mit LIBRARY/GEAR/PRESETS im Editor-Kopf.
      Integrationstests (6) fuer Seiten, Export und Overlays.
      530 Testfaelle / 540904 Assertions, Debug gruen.
04:43 REPORT.md, README.md, QUICKSTART, RELEASE-NOTES, Inno-Skript,
      package-release.ps1 und Handoff geschrieben.
05:00 Undo/Redo verdrahtet (Demo-Kick, Preset-Laden, Strg+Z / Strg+Shift+Z).
      Dabei einen echten Bug gefunden: undo()/redo() sicherten einen
      Default-Zustand statt des aktuellen.
05:10 Debug und Release gruen, 530 Testfaelle / 540913 Assertions.
      CI-Lauf 44233329972 gruen, Artefakt BassForgeKickArchitect-windows-x64.
05:15 Portables Release-Paket gebaut:
      release/BassForgeKickArchitect-0.1.0-portable-win64.zip (4,41 MB)
      plus SHA256SUMS.txt. `release/` ist jetzt in .gitignore.
```
