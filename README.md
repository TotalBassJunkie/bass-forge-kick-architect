# Bass Forge Kick Architect

Ein eigenständiger Techno-Kick-Builder für Windows (VST3 + Standalone), gebaut mit
C++20 und JUCE 8.

> **Status: Alpha-Vorstufe.** Build läuft (VST3 + Standalone), die KICK-Seite mit
> zwölf Slot-Karten und Visualiser ist bedienbar, das FX-Rack mit zehn Modulen steht.
> Noch offen: Pitch/Stretch in der Voice, ALIGN, Style-Sidebar, Overlays und Export.
> Siehe [Roadmap](#roadmap), [`docs/PROGRESS.md`](docs/PROGRESS.md) und
> [`docs/02-MASTERPLAN.md`](docs/02-MASTERPLAN.md).

---

## Worum es geht

Bass Forge Kick Architect baut aus mehreren Sample-Schichten einen fertigen Kick:

```
Samples (je Slot: Pitch, Stretch, Trim, Fades) → ALIGN / PHASE ALIGN
        → Summe → FX-Rack (freie Modulkette) → Output
```

* **Bis zu 12 Slots** (Standard 6), jeder Slot mit Volume, Pitch (längenneutral),
  Stretch (tonhöhenneutral), Trim, Fade-In/Out inkl. Kurvenformen, Filter und Delay.
* **ALIGN & PHASE ALIGN** schieben Punch, Crunch und Rumble lückenlos und phasenrichtig
  zusammen.
* **Style-Randomizer** mit Genre-Regeln, Pflicht- und Optionalteilen, No-Repeat-Logik
  und einem CHAOS-Modus.
* **Modulares FX-Rack** mit zehn Modultypen: Filters, Equalizer, Mid/Side-EQ,
  Distortion, Kick Shaper, Transient Shaper, Convolver, Finalizer, Multiband
  und Dynamic EQ.
* **Phase-2-Integrationen** (eigene Module auf Basis der Referenz-Plätze):
  Multiband nach dem Vorbild des Xfer OTT (Up/Down-Kompression, Crossovers,
  linearer Split, Band-Bypässe), Convolver nach dem Vorbild von Convology XT
  (True-Stereo-IRs, EQ-/Decay-Kurven, Hüllkurve, Modulation, Stretch, Predelay,
  Width) und Dynamic EQ nach dem Vorbild des TDR Nova (4 dynamische Bänder,
  HP/LP, W-Band, Auto-Gain, Delta, sieben Prozessziele). Eigene Implementierung,
  keine Assets oder Presets der Vorbilder.
* **Key-Track**: der Kick folgt gespielten Noten, mit Oktav-Angleich der Schichten.
* **Export per Drag & Drop**: fertiger Kick (Master) oder jede Schicht einzeln (Stems).

Ein ausführliches Handbuch als PDF entsteht mit der ersten Beta.

---

## Was dieses Projekt *nicht* ist

* **Kein Fork und keine Weiterverbreitung des kommerziellen Originals.**
  Es wird kein Code, keine Ressource und kein Logo eines bestehenden Produkts
  übernommen. Analysiert wurde ausschließlich, um **Verhalten** zu verstehen;
  implementiert ist alles neu.
* **Keine Sample-Library im Download.** Das Plugin liest deine eigene Sample-Sammlung
  (bzw. eine vorhandene, lokal installierte Library) und liefert keine fremden
  Audiodateien mit. Ein kleines, selbst erzeugtes Demo-Paket kommt später dazu.
* **Kein Kopierschutz.** Der Bau ist frei nutzbar, ohne Aktivierung.

Mehr dazu in [`docs/01-BESTANDSAUFNAHME.md`](docs/01-BESTANDSAUFNAHME.md), Abschnitt 9.

---

## Bauen

### Voraussetzungen

| Werkzeug | Version | Hinweis |
|---|---|---|
| Visual Studio Build Tools | 2022/2026, MSVC v143+ | „Desktop development with C++" |
| CMake | ≥ 3.22 | bei den Build Tools enthalten |
| Ninja | beliebig | bei den Build Tools enthalten |
| Git | beliebig | |

JUCE 8 wird beim ersten Konfigurieren automatisch geholt (CMake `FetchContent`),
Version fest auf `8.0.7` gepinnt.

### Schnellstart (Windows)

```powershell
# 1) Build-Umgebung einrichten (setzt vcvars64 + Werkzeugpfade)
. .\tools\dev-env.ps1

# 2) Konfigurieren (Debug)
.\tools\configure.ps1 -Config Debug

# 3) Bauen
.\tools\build.ps1 -Config Debug

# 4) Tests
.\tools\test.ps1
```

Das Build-Verzeichnis liegt standardmäßig **außerhalb** des Projektordners
(`C:\bfka-build\<Config>`) — liegen Projekt und Build in einem synchronisierten Ordner
(Dropbox/OneDrive), sperrt der Sync regelmäßig Dateien mitten im Linken und erzeugt
`LNK1104`/`LNK1201`. Über die Umgebungsvariable `BFKA_BUILD_ROOT` lässt sich der Pfad
ändern.

Artefakte landen in `<BuildRoot>\<Config>\BassForgeKickArchitect_artefacts\<Config>\`:
`VST3\` und `Standalone\`.

### Manuell

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Repository-Aufbau

```
docs/           Bestandsaufnahme, Masterplan, Spezifikationen, Fortschritt
src/core/       Zustand, Parameter-Registry, Preset-/Chain-IO
src/engine/     Sample-Voices, Renderer, Stretch/Pitch, ALIGN
src/fx/         Die FX-Module und die Ketten-Engine
src/library/    Content-Index, WAV-Analyse, Key-/BPM-Erkennung
src/ui/         Look & Feel, Seiten, Widgets, Overlays
src/plugin/     Plugin-Prozessor und Editor (JUCE-Einstieg)
tests/          Unit- und Integrationstests
tools/          Build-Helfer und Entwicklerwerkzeuge
resources/      Eigene Icons, Theme-Daten, Demo-Paket
```

---

## Roadmap

| Meilenstein | Inhalt | Status |
|---|---|---|
| **P0** | Fundament: Repo, Build, Plugin-Skelett, CI | fertig |
| **P1** | Datenmodell & IO (Presets, FX-Chains) | fertig (`.bfkapreset`, Modul-Presets, A/B) |
| **P2** | Library & Sample-Analyse | teilweise (Index/Scan; Analyse offen) |
| **P3** | Engine-Core (Voices, Summe, Auto-Gain) | fertig (inkl. Anzeige-Huellen) |
| **P4** | Stretch & Pitch (eigene WSOLA-Engine) | teilweise (WSOLA-Bausteine, noch nicht in der Voice) |
| **P5** | ALIGN & PHASE ALIGN | geplant |
| **P6** | FX-Module (9 Referenztypen + Dynamic EQ) | fertig (10 Typen, Karten + Graph-Editoren) |
| **P7** | Style-Randomizer | teilweise (Regeln/Engine; UI offen) |
| **P8–P10** | KICK-UI, FX-UI, Overlays | KICK-Seite und FX-Rack gebaut; Style-Sidebar und Overlays offen |
| **P11** | Export & Drag and Drop | geplant |
| **P12** | Key-Track & MIDI | geplant |
| **P13** | Parity-Messung, Installer, Release | geplant |

Details, Aufgaben-IDs und Abnahmekriterien: [`docs/02-MASTERPLAN.md`](docs/02-MASTERPLAN.md).
Laufender Fortschritt: [`docs/PROGRESS.md`](docs/PROGRESS.md).

---

## Mitmachen

Issues und Pull Requests sind willkommen, sobald der erste lauffähige Build steht.
Bitte vorher [`docs/02-MASTERPLAN.md`](docs/02-MASTERPLAN.md) lesen — dort steht, welche
Bausteine bewusst wie umgesetzt werden.

---

## Lizenz

Der Quellcode steht unter der **GNU General Public License v3.0** (siehe `LICENSE`),
solange nicht anders angegeben. Teile der Laufzeitumgebung (JUCE) unterliegen eigenen
Lizenzbedingungen; die Details stehen in [`docs/LICENSE-THIRD-PARTY.md`](docs/LICENSE-THIRD-PARTY.md).

Wer das Plugin kommerziell vertreiben will, muss die JUCE-Lizenzierung selbst klären.

---

## English summary

**Bass Forge Kick Architect** is an independent techno kick builder for Windows
(VST3 + Standalone, C++20 + JUCE 8). It layers up to twelve samples — each with
lenght-neutral pitch, pitch-neutral stretch, trim and shaped fades — aligns them
without gaps and in phase, sums them, and runs the result through a freely reorderable
nine-module FX rack. A style-based randomizer keeps every roll inside its genre.

The project is a clean-room functional rebuild: no code, asset or logo of any existing
commercial product is used or redistributed, and no sample content is bundled — the
plugin reads the user's own library. The repo is currently at pre-alpha: the full
inventory and build plan are in [`docs/`](docs/), the code follows in small steps.
