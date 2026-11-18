# Quickstart

Kurzweg von einem leeren Rechner bis zum ersten Kick. Ausfuehrlicher
Hintergrund steht in [`README.md`](../README.md), die Testzahlen und der
Fortschritt in [`PROGRESS.md`](PROGRESS.md), die Release-Historie in
[`RELEASE-NOTES.md`](RELEASE-NOTES.md).

---

## 1. Voraussetzungen

| Punkt | Anforderung |
|---|---|
| System | Windows 10 oder Windows 11, 64 Bit (x86_64) |
| Compiler | Visual Studio Build Tools mit Workload **"Desktop development with C++"** (MSVC v143 oder neuer) |
| CMake | kommt mit den Build Tools (erwartet wird >= 3.22) |
| Ninja | kommt mit den Build Tools |
| Git | beliebig, nur fuer den Quellbaum |

Es sind **keine weiteren Downloads** noetig. JUCE 8.0.7 und Catch2 3.7.1 holt
CMake beim ersten Konfigurieren selbst per `FetchContent`; dafuer muss die
erste Konfiguration Netzwerkzugriff haben.

`tools/dev-env.ps1` sucht Visual Studio ueber `vswhere`, setzt `cl`, `cmake` und
`ninja` in die aktuelle Sitzung und legt `BFKA_BUILD_ROOT` an.

---

## 2. Bauen (Schritt fuer Schritt)

PowerShell oeffnen (kein Admin noetig), dann:

```powershell
cd "C:\Users\tbj\Dropbox\.BFR Plugin Projects\Kick Builder Plugin Bass Forge"

# 1) Umgebung: MSVC, CMake, Ninja, BFKA_BUILD_ROOT
. .\tools\dev-env.ps1

# 2) Konfigurieren (Ninja, Tests an)
.\tools\configure.ps1 -Config Release

# 3) Bauen (VST3 + Standalone + Tests)
.\tools\build.ps1 -Config Release
```

Am Ende listet `build.ps1` die Artefakte auf. Sie liegen unter

```
C:\bfka-build\Release\BassForgeKickArchitect_artefacts\Release\
    Standalone\Bass Forge Kick Architect.exe
    VST3\Bass Forge Kick Architect.vst3
```

**Wichtig: der Build laeuft nur unter `C:\bfka-build`.** Das Projekt liegt in
einem Dropbox-Ordner; liegt das Build-Verzeichnis mit darin, sperrt der Sync
regelmaessig Dateien mitten im Linken und der Build bricht mit `LNK1104` oder
`LNK1201` ab. `dev-env.ps1` setzt `BFKA_BUILD_ROOT` deshalb auf `C:\bfka-build`;
ein anderer Pfad geht ueber dieselbe Umgebungsvariable, muss dann aber auch im
Inno-Setup-Skript (`installer/BassForgeKickArchitect.iss`, `BuildRoot`) stimmen.

`configure.ps1` nur nach Aenderungen an `CMakeLists.txt` erneut ausfuehren;
sonst genuegt `build.ps1`. Das Flag `-Clean` loescht das Build-Verzeichnis
vorher komplett.

---

## 3. Tests ausfuehren

```powershell
. .\tools\dev-env.ps1
.\tools\test.ps1 -Config Debug
```

`test.ps1` baut das Ziel `BFKA_Tests` und startet es. Das Binary liegt unter

```
C:\bfka-build\Debug\tests\BFKA_Tests_artefacts\Debug\BFKA_Tests.exe
```

(Release: `C:\bfka-build\Release\tests\BFKA_Tests_artefacts\Release\BFKA_Tests.exe`).
Einzelne Gruppen laufen so - in PowerShell die eckigen Klammern quoten, sonst
deutet die Shell sie als Array-Index:

```powershell
C:\bfka-build\Debug\tests\BFKA_Tests_artefacts\Debug\BFKA_Tests.exe '[ui]'
```

Aktueller Stand (aus `PROGRESS.md`): 504 Testfaelle, 540.847 Assertions,
Debug gruen; Release mit 439 Testfaellen gruen gemessen.

---

## 4. Erste Schritte im Plugin

Starte `Bass Forge Kick Architect.exe` aus dem Standalone-Ordner (oder lade das
VST3 im Host; es ist ein Instrument mit MIDI-Eingang).

1. **DEMO-KICK** in der Transportzeile druecken. Das Plugin sucht Punch,
   Rumble und Click in der Library und legt sie auf Slot 1-3, inklusive Rollen
   und Startpegeln. Ohne Library meldet die Statuszeile
   "Keine Sample-Library gefunden".
2. **Slot laden.** Auf einer Karte `LOAD` druecken und eine WAV/AIFF/FLAC/OGG/
   MP3-Datei waehlen. `X` leert den Slot, `MUTE`/`SOLO` wirken sofort,
   `LOCK` sperrt den Slot gegen Randomizer/Stylewechsel, `P` schaltet
   KEY TRACK fuer den Slot ein.
3. **PITCH und STRETCH** am grossen Regler ziehen. PITCH ist
   laengenneutral (KEEP PITCH ueber vorgerenderten Puffer) oder als REPITCH
   gekoppelt; STRETCH aendert die Laenge ohne Tonhoehe. Die Karten-Regler
   schreiben direkt in die Host-Parameter, ein Ketten-Rebuild ist nicht noetig.
4. **FX-Kette.** Auf die Seite `FX` wechseln, mit `+ ADD MODULE` Module
   anhaengen (zehn Typen), mit den Pfeilen umsortieren, `BYPASS ALL`,
   `CLEAR` und `DEFAULT` liegen in der Kopfzeile. `FX-DEMO` baut eine
   Beispielkette (Multiband, bei vorhandener IR Convolver, Dynamic EQ) und
   spielt sie einmal an. Graph-Editoren oeffnen sich in der Karte.
5. **Preset speichern.** `SAVE` in der FX-Kopfzeile schreibt ein
   `.bfkapreset` in den Nutzer-Presetordner; `LOAD` liest es zurueck
   (Slots, Parameter, FX-Kette). `SAVE MODULE` / `LOAD MODULE` in der
   Seitenleiste sichern einen einzelnen Kettenplatz. A/B ueber die Knoepfe
   `A` und `B` (Klick ruft ab, Shift-Klick merkt).
   Samples werden beim Speichern ueber den Dialog derzeit **nicht** eingebettet
   - die Option existiert im Code (`PresetWriteOptions::embedSamples`), haengt
   aber noch nicht an einem Schalter der UI.
6. **DRAG** in der Transportzeile rendert den Kick offline inklusive FX-Kette
   und legt die WAV-Datei im `Exports`-Ordner ab (Statuszeile zeigt den Pfad).
   Derzeit wird nur die Summe (Master) geschrieben, keine Einzel-Stems.
7. **Overlays.** Die Kopfknoepfe `LIBRARY`, `GEAR` und `PRESETS` oeffnen
   Ueberlagerungen ueber der Seite: Library mit vier Tabs (PRESETS, SOUNDS,
   FX CHAINS, MY SOUNDS; aktuell ist nur PRESETS gefuellt), Einstellungen mit
   acht Sektionen aus `prefs.json`, Preset-Manager zum Browsen und Laden.
   `Escape` schliesst sie.

Hinweis zu `ROLL`: der Knopf in der linken Style-Sidebar zaehlt mit und spielt
den Kick an, wendet aber noch keine Style-Regeln an (die Randomizer-Regeln sind
nicht an die Library gebunden). Styles und CHAOS-Schalter sind sichtbar, aber
noch nicht wirksam.

---

## 5. Wo die Dateien liegen

Alles Nutzerseitige liegt unterhalb der Dokumente, die Einstellungen in AppData.

| Inhalt | Pfad |
|---|---|
| Kick- und Modul-Presets (`.bfkapreset`) | `Dokumente\BassForge\KickArchitect\Presets` |
| Exporte aus `DRAG` (`.wav`, 24 bit) | `Dokumente\BassForge\KickArchitect\Exports` |
| Impulse-Responses fuer den Convolver (wav/aif/aiff/flac) | `Dokumente\BassForge\KickArchitect\IRs` |
| Eigene Styles (`.bfkastyle`) | `Dokumente\BassForge\KickArchitect\Styles` |
| Aus Presets entpackte Samples | `Dokumente\BassForge\KickArchitect\SampleCache` |
| Einstellungen (`prefs.json`, acht Sektionen) | `%APPDATA%\BassForge\KickArchitect\prefs.json` |

`Dokumente` ist der Windows-Dokumenteordner des angemeldeten Nutzers, also
`C:\Users\<Name>\Documents`. Die Ordner legt das Plugin beim ersten Zugriff an
(Presets), spaetestens beim ersten Schreiben.

Die **Sample-Library** ist etwas anderes: sie wird nicht vom Plugin angelegt,
sondern gesucht, in dieser Reihenfolge (erster vorhandener Ordner gewinnt):

1. `%PUBLIC%\Documents\BassForge\KickArchitect`
2. `%PUBLIC%\Documents\Teknovault\The Teknovault Kick Builder` (nur lesend)
3. `%USERPROFILE%\Documents\BassForge\KickArchitect`

Fehlt der Ordner, kommt Kandidat 1 als Ziel zurueck - ihn anlegen und die eigene
Sammlung im erwarteten Schema (Punch/Rumble/Click/Tops/Crunch) hineinlegen.

Kennzahlen zum Preset-Format: eingebettete Samples werden als 16-bit-WAV
Base64-kodiert unter `<Samples>` abgelegt, Obergrenze 32 MB; wird sie
ueberschritten, entsteht **keine** Teildatei. Beim Laden fehlender Samples
landet die Kopie im `SampleCache`.

---

## 6. Fehlersuche

**Build rot, `LNK1104` / `LNK1201` / "Zugriff verweigert"**
Meist liegt das Build-Verzeichnis im synchronisierten Ordner, oder ein Viren-
scanner haelt die `.obj`/`.exe`. Pruefen, ob `$env:BFKA_BUILD_ROOT` wirklich
`C:\bfka-build` ist (`echo $env:BFKA_BUILD_ROOT`) und die Umgebung neu laden
(`. .\tools\dev-env.ps1`). Danach `.\tools\configure.ps1 -Config Release
-Clean` und neu bauen. Bei "no work to do" ohne Artefakt: das Ziel existiert
nicht in dieser Konfiguration - erst konfigurieren, dann bauen.

**Preset klingt leer, Slots zeigen aber Dateien**
Die Sample-Referenzen im Preset zeigen auf Pfade, die es auf diesem Rechner
nicht gibt (anderer Nutzer, verschobene Library) und es waren keine Samples
eingebettet. Im `SampleCache` nachsehen, ob eine Kopie existiert; sonst die
betroffenen Slots per `LOAD` neu fuellen. Beim Speichern von Presets, die
weitergegeben werden sollen, spaeter die Einbett-Option nutzen (siehe oben)
oder die Samples mitliefern. Zeigt ein frisch geladenes Preset gar nichts,
hilft `DEMO-KICK` als Gegenprobe: der schreibt bewusst in den Zustand und
muss Karten und Klang fuellen.

**Convolver meldet "Keine IRs gefunden"**
Der Ordner `Dokumente\BassForge\KickArchitect\IRs` fehlt oder ist leer, oder
die Dateien haben ein anderes Format als wav/aif/aiff/flac. Ordner anlegen,
Dateien hineinlegen, im Convolver-Modul die Liste mit Doppelklick neu laden.
Der Convolver laesst sich ohne IR trotzdem einsetzen (das Modul bleibt dann
wirkungslos), und `FX-DEMO` laesst ihn in diesem Fall bewusst weg.
