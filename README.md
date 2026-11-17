# Bass Forge Kick Architect

Ein eigenständiger Techno-Kick-Builder für Windows (VST3 + Standalone), gebaut mit
C++20 und JUCE 8.

> **Status: Pre-Alpha.** Dieses Repository startet gerade. Der aktuelle Stand ist die
> vollständige Bestandsaufnahme und der Bauplan; der Code wächst danach in kleinen,
> nachvollziehbaren Schritten. Siehe [Roadmap](#roadmap) und
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
* **Modulares FX-Rack** mit neun Modultypen: Filters, Equalizer, Mid/Side-EQ,
  Distortion, Kick Shaper, Transient Shaper, Convolver, Finalizer, Multiband.
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

Artefakte landen in `build\<Config>\`:
`BassForgeKickArchitect_artefacts\<Config>\VST3\` und `...\Standalone\`.

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
src/fx/         Die neun FX-Module und die Ketten-Engine
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
| **P0** | Fundament: Repo, Build, Plugin-Skelett, CI | in Arbeit |
| **P1** | Datenmodell & IO (Presets, FX-Chains) | geplant |
| **P2** | Library & Sample-Analyse | geplant |
| **P3** | Engine-Core (Voices, Summe, Auto-Gain) | geplant |
| **P4** | Stretch & Pitch (eigene WSOLA-Engine) | geplant |
| **P5** | ALIGN & PHASE ALIGN | geplant |
| **P6** | Neun FX-Module | geplant |
| **P7** | Style-Randomizer | geplant |
| **P8–P10** | KICK-UI, FX-UI, Overlays | geplant |
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
