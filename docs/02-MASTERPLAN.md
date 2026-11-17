# 02 — Masterplan: Bass Forge Kick Architect

**Ziel:** The Teknovault Kick Builder 1.1.2 funktional 1:1 nachbauen — gleiche
Bedienlogik, gleiches Klangverhalten, gleiche Datenmodelle — unter eigenem Branding,
lizenzrein, ohne Original-Assets.

**Produkt:** Bass Forge Kick Architect · Code `BFKA` · Preset `.bfkapreset` ·
Bundle-ID `com.bassforge.kickarchitect` · VST3-UID eigen.
**Formate:** VST3 + Standalone (Windows zuerst, macOS später über CI).
**Stack:** C++20 · JUCE 8 · CMake · Catch2-Tests · GitHub Actions.

---

## 0. Arbeitsweise

### 0.1 Die drei Regeln

1. **Verträge zuerst, dann Parallelität.** Parallele Subagenten können nur
   widerspruchsfrei arbeiten, wenn Header/Interfaces vorher feststehen. Deshalb
   schreibt der Lead-Agent jede Phase zuerst als `src/**/*.h`-Vertrag, dann
   implementieren Subagenten die `.cpp`-Dateien gegen diesen Vertrag.
2. **Jede Phase endet mit einem grünen Build + Test.** Kein Subagent darf den Build
   kaputt zurücklassen; die Integration prüft der Lead.
3. **Parity ist messbar.** Wo Klangverhalten gefordert ist, wird gegen das Original
   gemessen (Spektren, Hüllkurven, Nulltests), nicht „nach Gefühl" abgenommen.

### 0.2 Subagenten-Routing (Kosten)

| Aufgabe | Modell | Begründung |
|---|---|---|
| Mechanik: Boilerplate, CMake, Serialisierung, Testgerüste, Übersetzungen | `desktop-gpu/qwen2.5-coder-14b` (lokal, kostenlos) | hochgradig schematisch, lokal schnell |
| Standard-Code: UI-Widgets, Datei-IO, einfache DSP-Bausteine | `merge-gateway/qwen/qwen3.8-flash` oder `zai/glm-5.3-flash` (günstig) | gute Codequalität, niedrige Kosten |
| Anspruchsvoll: DSP-Kern, WSOLA, Filter, Oszillator-freie Kettenglieder, Align | `deepseek-official/deepseek-flash` | stärkstes verfügbares Flash-Modell |
| Architektur, Verträge, Integration, Review | Lead-Agent | Konsistenz |

Faustregel: **mindestens 60 % der Aufgaben auf dem lokalen Qwen-Modell** (kostenlos),
Rest auf Flash-Modellen. Jede Subagent-Aufgabe bekommt: Ziel, Vertragsdatei,
Akzeptanzkriterien, Testpflicht, „nicht anfassen"-Liste.

### 0.3 Definition of Done (jede Aufgabe)

* Code kompiliert mit `-W4`-Sauberkeit (keine neuen Warnungen).
* Mindestens ein Test in `tests/` deckt die neue Logik ab.
* Öffentliche Symbole dokumentiert (Doxygen-Kurzform).
* Kein `TODO` ohne Ticket-ID.
* Commit-Nachricht: `feat(<bereich>): <was> [B-xx]`.

---

## 1. Phasenübersicht

```
P0  Fundament ............ Repo, Build, Plugin-Skeleton, CI, State-Schema
P1  Datenmodell & IO ..... Zustand, Parameter-Registry, Preset/Chain-IO, Alt-Import
P2  Library & Analyse .... Content-Index, WAV-Scan, BPM/Key/Onset-Analyse, Cache
P3  Engine-Core .......... Sample-Voice, Trim/Fade/Position, Summe, Auto-Gain, Länge
P4  Stretch & Pitch ...... WSOLA, Repitch, BPM-Sync, Pitch-Shift, Offline-Renderer
P5  Align & Phase ........ Gap-Erkennung, Verschieben, Strecken, Polarität
P6  FX-Module ............ 9 Module, Kurven-Editoren, Proxy-Parameter
P7  Randomizer ........... Styles, Pools, Regeln, No-Repeat, Chaos
P8  UI KICK-Seite ........ Look&Feel, Header, Sidebar, Transport, Visualiser, Karten
P9  UI FX-Seite .......... Rack, Modulkarten, Graphen (EQ/Curve/OTT/Crossover)
P10 Overlays ............. Library, Settings, Presets, Add Part, Add Style
P11 Export & DnD ......... Drag Master/Stems, Datei-Drop, WAV-Writer
P12 Key & MIDI ........... Key-Detection, Key-Track, MPE/Note-Handling
P13 Parity & Politur ..... Messvergleich, Presets-Portierung, Crash-Härtung
P14 Extras (Phase 2) ..... eigene Features (später, separater Plan)
```

---

## 2. Phase 0 — Fundament

**Ziel:** Ein leerer, aber vollständig verdrahteter Plugin-Build, der in einem Host
lädt und eine Test-Sinuston-Ausgabe macht. Ab hier kann jeder Subagent bauen.

| ID | Aufgabe | Modell | Ergebnis |
|---|---|---|---|
| B-00 | Git-Repo, `.gitignore`, README, LICENSE, Issue-Template, Branch-Schutz | Lead | Repo steht |
| B-01 | `CMakeLists.txt` mit `juce_add_plugin`, Targets VST3 + Standalone + Tests | qwen-coder | `cmake --build` grün |
| B-02 | JUCE-Abhängigkeit (FetchContent, feste Version 8.0.7) + Submodule-Policy | qwen-coder | reproduzierbarer Build |
| B-03 | `PluginProcessor`/`PluginEditor`-Skelett, Bus-Layout stereo, 0 dB Pass-through | qwen-coder | lädt im Host |
| B-04 | Logging/Assert-Helper, `bfka::Result`, Fehlercodes | qwen-coder | Basismodul |
| B-05 | GitHub-Actions: Windows-Build + Artefakt, Test-Run, Release-Workflow | Lead | CI grün |
| B-06 | `tests/`-Gerüst (Catch2), `ctest`-Integration | qwen-coder | Tests laufen in CI |
| B-07 | `docs/specs/parameter-registry.md` (Vorlage) | Lead | Spec-Gerüst |

**Abnahme:** CI baut VST3 + Standalone, Artefakt ist herunterladbar, `ctest` läuft mit
mindestens einem Test.

---

## 3. Phase 1 — Datenmodell & IO

**Ziel:** Der komplette Kick-Zustand ist als typisiertes, versioniertes Modell
vorhanden und lässt sich als `.bfkapreset` (unser Format) speichern/laden —
**plus** Import der Original-`.itkbpreset` und Original-`FxChainPreset`-XML.

### 3.1 Vertrag (Lead schreibt zuerst)

```cpp
namespace bfka {

struct SampleRef { juce::String relPath, absPath, fileName, roleId; };

enum class Role { punch, crunch, rumble, click, top,
                  punchEnhancer, rumbleEnhancer, schranzEnhancer };

struct SlotParams {           // exakt die Original-IDs
    float vol{}, pitch{}, pos{}, trimStart{}, trimEnd{}, fadeIn{}, fadeOut{},
          stretch{}, filter{}, filterQ{}, dlyTime{}, dlyMix{},
          fiCurve{}, fiShape{}, foShape{};
    bool  mute{}, solo{};
    float spare1{}, spare2{};
};

struct GlobalParams {
    float satAmount{}, reverbSize{}, reverbDamp{}, reverbMix{}, hollowAmt{},
          maxAmount{}, masterVol{}, rootNote{};
    int   midiMode{};
    float gspare1{}, gspare2{}, gspare3{};
};

struct KickState {            // 12 Slots + Global + FX + Meta
    std::array<std::optional<SampleRef>, 12> slots;
    std::array<SlotParams, 12> slotParams;
    GlobalParams global;
    FxChain fx;
    StyleId style; juce::String presetName; bool dirty{};
    // Locks, Align-Settings, KeyTrack, View-Modi, Stretch-/Tempo-Mode, manueller Key
};

} // namespace bfka
```

| ID | Aufgabe | Modell | Ergebnis |
|---|---|---|---|
| B-10 | `KickState` + `SlotParams` + `GlobalParams` als Value-Tree-Brücke | Lead | Header |
| B-11 | Parameter-Registry: ID → (Bereich, Kurve, Default, Einheit) für **alle** Original-IDs | Lead | `parameter-registry.h/.cpp` |
| B-12 | `.bfkapreset` schreiben/lesen (XML, schemaVersion 1) inkl. eingebetteter Samples | qwen-coder | IO + Tests |
| B-13 | `.itkbpreset` **Import** (schemaVersion 2, Normalisierung wie Original) | deepseek-flash | Roundtrip-Test |
| B-14 | `FxChain`-Modell + `FxChainPreset`-XML lesen/schreiben | qwen-coder | IO + Tests |
| B-15 | Undo/Redo über ganze Zustände (`juce::UndoManager` + Snapshot-Ableitung) | deepseek-flash | Tests |
| B-16 | Zustands-Migration (Schema-Versionen) | qwen-coder | Tests |
| B-17 | A/B-Golden-Tests: 44 Original-Presets importieren → Zustands-Dump vergleichen | deepseek-flash | CI-Gate |

**Abnahme:** Alle 44 Werk-Presets importieren fehlerfrei; Parameter-Dump identisch zu
einer Referenzliste; Speichern/Laden ist verlustfrei (Roundtrip-Byte-Vergleich auf
Werteebene).

---

## 4. Phase 2 — Library & Analyse

**Ziel:** Die installierte Sample-Library wird gefunden, indexiert, analysiert und
durchsuchbar gemacht — mit derselben Ordner-/Pool-Logik wie das Original.

| ID | Aufgabe | Modell | Ergebnis |
|---|---|---|---|
| B-20 | Content-Pfad-Auflösung (Registry, Prefs, Fallback), Library-Root-Erkennung | qwen-coder | Tests |
| B-21 | WAV-Scanner → `SampleInfo{ path, role, pool, bpm, key, lengthS, channels, rate }` | qwen-coder | Index |
| B-22 | BPM-Parser aus Dateinamen (Fallback 150) + Tonart-Parser (`A#`, `G#`, …) | qwen-coder | Tests |
| B-23 | Onset/Transienten-Detektor (Envelope-Fluss) für Trim-Vorschläge | deepseek-flash | Analyse |
| B-24 | Key-Detection (chroma/HPCP + Template-Matching, Konfidenz) | deepseek-flash | ≤ 5 % Abweichung zum Dateinamen |
| B-25 | „Ähnliche Sounds"-Ähnlichkeitsmaß (MFCC/Chroma-Distanz) | glm-5.3-flash | Ranking |
| B-26 | Analyse-Cache (binär, versioniert, inkrementell) | qwen-coder | Tests |
| B-27 | Hintergrund-Analyse-Thread mit Fortschritt/Abbruch | deepseek-flash | UI-fähig |
| B-28 | Synthetische Test-Library-Generator (Kicks aus Oszillatoren) | glm-5.3-flash | 100 Testdateien |

**Abnahme:** Library mit 2.443 Dateien wird in < 60 s kalt indexiert (parallel),
Key-Erkennung stimmt bei den melodischen Pools ≥ 95 % mit dem Dateinamen überein.
**Rechtlich:** Eigene Test-Library wird erzeugt; die Teknovault-Library wird nur
lokal gelesen, nie ins Repo committet.

---

## 5. Phase 3 — Engine-Core

**Ziel:** Ein Kick aus mehreren Slots klingt korrekt: Position, Trim, Fades mit Kurven,
Gain, Summe, Auto-Gain, Kick-Länge.

| ID | Aufgabe | Modell | Ergebnis |
|---|---|---|---|
| B-30 | `SampleVoice`: Laden, Position, Trim, Loop-frei, Fade-in/out mit 5 Kurvenformen | deepseek-flash | Tests |
| B-31 | Fade-Kurven: Linear, Curve (`t`-Biegeparameter), S-Curve, Fast, Slow (+ `fishape`/`foshape`) | deepseek-flash | Kurven-Unit-Test |
| B-32 | Per-Slot-Filter (`lN_filter`/`filterq`) + Delay (`dlytime`/`dlymix`) | qwen-coder | Tests |
| B-33 | Summenbus + `master_vol`-Skala | qwen-coder | Tests |
| B-34 | Auto-Gain: Body-Fenster-Detektor + Zielpegel 0 dBFS | deepseek-flash | Messung |
| B-35 | Kick-Länge: `1 BEAT` (weiches Fade) vs. `FULL SAMPLE` | qwen-coder | Tests |
| B-36 | `sat_amount`, `reverb_size/damp/mix`, `hollow_amt`, `max_amount` — Semantik klären, dann bauen | Lead + deepseek | Spec + Code |
| B-37 | Offline-Renderer (`CompositeRenderer`) für Visualiser/Export | deepseek-flash | Tests |
| B-38 | Latenz-/Performance-Budget: < 2 % CPU je Instanz im Ruhezustand | Lead | Messung |

**Abnahme:** Ein 6-Slot-Kick aus einem Original-Preset klingt bei identischen
Sample-Dateien im Nulltest deckungsgleich (Restfehler < −60 dBFS RMS gegen Referenz-
Render aus dem Original; Referenzaufnahme über Drag-Master des Originals).

---

## 6. Phase 4 — Stretch & Pitch

**Ziel:** Der klanglich heikelste Teil. PITCH (längenneutral) und STRETCH
(tonhöhenneutral) müssen getrennt, in Echtzeit-fähiger Latenz und in Werkzeugqualität
funktionieren.

**Erkenntnis aus dem RE:** Original = eigene **WSOLA**-Engine (`WsolaPrepared`,
`WsolaNPrepared`, `Stft`, `Sig`, `Model`) + Rubber Band. Wir bauen **nur** die eigene
Engine (lizenzrein) und verzichten auf Rubber Band.

| ID | Aufgabe | Modell | Ergebnis |
|---|---|---|---|
| B-40 | WSOLA-Kern: Grain-Fenster, Hop, Cross-Correlation-Suche, Overlap-Add | deepseek-flash | Tests |
| B-41 | Parameterstudie: Fenstergrößen, Suchbereich, Hann/Blackman | Lead | Messreihe |
| B-42 | `KEEP PITCH` (WSOLA) vs. `REPITCH` (Resampling) | deepseek-flash | Tests |
| B-43 | `BPM STRETCH / SYNC BPM`: Faktor = eigenes BPM / Kick-BPM | qwen-coder | Tests |
| B-44 | Pitch-Shift unabhängig von der Länge (Resampling + WSOLA-Kompensation) | deepseek-flash | Tests |
| B-45 | Asynchroner Vorab-Renderer + Cache (Key = Sample + Pitch + Stretch + Mode) | deepseek-flash | Tests |
| B-46 | Qualitätsmetriken: Transientenerhalt, Aliasing, Phasenkohärenz | Lead | Report |
| B-47 | Grenzfälle: 0.25×, 8×, sehr kurze Samples, DC/Stille | qwen-coder | Tests |

**Abnahme:** Stretch 0.5×/2× ohne hörbare Doppel-Transienten; PITCH ändert die Länge
exakt 0 Sample; Renderzeit für einen 1-Beat-Kick < 30 ms (asynchron, nie im Audio-Callback).

---

## 7. Phase 5 — Align & Phase Align

| ID | Aufgabe | Modell | Ergebnis |
|---|---|---|---|
| B-50 | Gap-Analyse: Ende des Ankers vs. Beginn der Tail-Parts | deepseek-flash | Tests |
| B-51 | `MOVE CRUNCH`: Verschieben nach `HOW TIGHT` (TIGHT/NORMAL/GENTLE) | deepseek-flash | Tests |
| B-52 | `STRETCH PUNCH` (nur KEEP PITCH) + `STRETCH CRUNCH` | deepseek-flash | Tests |
| B-53 | `STRETCH FADES` (Fade kürzen, wenn Lücke bleibt) | qwen-coder | Tests |
| B-54 | Hand-Platzierung (`pinned`) respektieren + Chip „MOVED BY HAND" | qwen-coder | Tests |
| B-55 | Anker-Wahl (Library-Punch, sonst energie­stärkster Start) | qwen-coder | Tests |
| B-56 | PHASE ALIGN: Korrelation im Low-End, Nudge + Polaritäts-Flip | deepseek-flash | Tests |
| B-57 | `LAST PASS`-Report-Strings (identische Texte wie Original) | qwen-coder | Stringliste |
| B-58 | `RESULT LINE`-Anzeige + `ALIGN BLEND`-Setting | qwen-coder | Tests |

**Abnahme:** Nach ALIGN ist die Lücke zwischen Punch und Rumble ≤ 1 ms (TIGHT) und
der Low-End-Pegel (20–80 Hz) steigt gegenüber „ohne Align" messbar; identisches
Verhalten wie Original bei denselben 10 Test-Kicks.

---

## 8. Phase 6 — FX-Module

Neun Modultypen, alle mit `on` / `mix` / `outGain`. Jedes Modul ist eine eigene
Subagent-Aufgabe mit eigenem Test (Impulsantwort, Spektrum, THD).

| ID | Modul | Kernaufgabe | Modell |
|---|---|---|---|
| B-60 | `filt` Filters | LP/HP 6·12·18·24·30 dB/Okt (Kaskade aus Biquads, resonantes Knie), BAND 12/24, NOTCH, PHASER 1–6 (±), COMB (±), CUTOFF/RESO/DRIVE | deepseek-flash |
| B-61 | `eq` Equalizer | 12 Bänder, Typen BELL/L-SHF/H-SHF/L-CUT/H-CUT/NOTCH, dyn. Bänder, Spektrum-Analyse | deepseek-flash |
| B-62 | `mseq` Mid/Side-EQ | MS-Matrix + je Seite Bänder, `width`, Solo/Mute je Seite | qwen-coder |
| B-63 | `dist` Distortion | 10 Modi + AGC + `EQ`-Chip PRE/POST/OFF + 0-dBFS-Ceiling + WNode-Kurve | deepseek-flash |
| B-64 | `shaper` Kick Shaper | Ein-Beat-Hüllkurve, 8 Segmentmodi, AMOUNT, SAVE/RESET-SHAPES | glm-5.3-flash |
| B-65 | `trans` Transient Shaper | ATTACK/DUCK/SPEED, Envelope-Follower | deepseek-flash |
| B-66 | `reverb` Convolver | Zero-Latency-Faltung (partitioned), IR-Stretch (SPEED), STEREO, TONE, Hüllkurve, Equal-Power-Mix | deepseek-flash |
| B-67 | `final` Finalizer | SUB-Boost (FREQ/BOOST), MONO BASS (Crossover), True-Peak-CLIP | deepseek-flash |
| B-68 | `ott` Multiband | 3-Band Up/Down-Kompressor, LINEARE-PHASE-Split (FIR), Crossovers ziehbar | deepseek-flash |
| B-69 | Ketten-Engine | Reihenfolge, Bypass, Reorder, Per-Modul-Latenzausgleich, Display-Daten | Lead |

**Abnahme je Modul:** Bypass-Nulltest (−120 dBFS), Frequenzgang/THD-Report im PR,
CPU-Budget < 3 % je Modul bei 48 kHz/128 Samples.

---

## 9. Phase 7 — Randomizer

| ID | Aufgabe | Modell | Ergebnis |
|---|---|---|---|
| B-70 | `genres.json`-Parser → `Style/Pool/Rule`-Modell | qwen-coder | Tests |
| B-71 | Regel-Engine: `code`-Semantik, `db`/`dbPair`, `shares`, `layerRules` min/max | Lead + deepseek | Spec + Code |
| B-72 | Ausschlussregel „melodic × atonal" + `duplicateGain`/`fileWeights` | qwen-coder | Tests |
| B-73 | No-Repeat: Ordner-Permutation + 5-Roll-Fenster | qwen-coder | Tests |
| B-74 | Pflicht- vs. Optional-Parts, `X`-Sichtbarkeit, Startpegel (Roll vs. Browsen) | qwen-coder | Tests |
| B-75 | CHAOS: Pitch ±3 HT, Stretch, FX-Chain, FX-Module + eigener Dice | glm-5.3-flash | Tests |
| B-76 | FREE BUILD, BOTH LIBRARIES, `+ ADD STYLE` (eigene Styles) | glm-5.3-flash | Tests |
| B-77 | Statistischer Abgleich mit Original (1000 Rolls je Style, Verteilung) | Lead | Report |

**Abnahme:** Über 10.000 Rolls je Style bleiben Rollen-Minima/-Maxima zu 100 %
eingehalten; Pool-Häufigkeiten weichen < 3 % von der Original-Messung ab.

---

## 10. Phase 8 — UI KICK-Seite

| ID | Aufgabe | Modell | Ergebnis |
|---|---|---|---|
| B-80 | `BfkaLookAndFeel` (eigene Farbwelt, Typografie, Knöpfe, Regler) | glm-5.3-flash | Styleguide |
| B-81 | Header: Seitenschalter, Preset-Pille, Save, Undo/Redo, Library, Gear | qwen-coder | Mockup→Code |
| B-82 | Style-Sidebar: Liste, Akzentfarbe, Tabs TKNVLT/OPS (eigene Namen), ADD STYLE | qwen-coder | Code |
| B-83 | Transport-Zeile: Play/Loop/Stop, BPM-Editor, KEY-Chip, Toggles, OUT-Meter/Fader | deepseek-flash | Code |
| B-84 | Visualiser: Wellenform-Renderer (Parts/Solo/Focus), Spektrum, Clip-Ansicht, Grid | deepseek-flash | Code |
| B-85 | Interaktion im Visualiser: Move, Trim, Fade-Tabs/Anker/Diamanten, Rechtsklick-Menü | deepseek-flash | Code |
| B-86 | Slot-Karten (6 + 12), alle Regler, Lock, Mute/Solo, P, X, Pegel-Strip | qwen-coder | Code |
| B-87 | Regler-Rechtsklick (Reset/Typisieren/Copy&Paste), Shift-Fein, Ctrl-Schritte | qwen-coder | Code |
| B-88 | Undo-Integration, Tooltips, Tastaturkürzel | qwen-coder | Code |
| B-89 | Resize / UIScaler / HiDPI | qwen-coder | Code |

**Abnahme:** Screenshot-Vergleich „Layout-Treue" ≥ 90 % (Positionen/Größen der
Hauptelemente), alle Funktionen bedienbar, 60 fps Zeichnen bei 1440p.

---

## 11. Phase 9 — UI FX-Seite

| ID | Aufgabe | Modell |
|---|---|---|
| B-90 | Rack: Ketten-Sidebar, Reorder per Drag, Ein-/Ausklappen, `+ ADD MODULE`-Grid | deepseek-flash |
| B-91 | Modulkarte: Kopf (Up/Down, Icon, Preset-Pfeile, Duplicate, Power, Entfernen), MIX, Meter | qwen-coder |
| B-92 | Graph-Editoren: EQ-Kurve (Bänder), Waveshaper (WNode + 8 Segmentmodi), Kick-Shaper-Kurve, Reverb-Hüllkurve, OTT-Crossover | deepseek-flash |
| B-93 | Modul-Preset-Verwaltung (Save Current, Pfeile, Default) | qwen-coder |
| B-94 | Ketten-Header: Lock, Clear, Default, Auswahl, Save, Bypass | qwen-coder |

---

## 12. Phase 10 — Overlays

| ID | Aufgabe | Modell |
|---|---|---|
| B-100 | Library-Overlay: 4 Tabs, Suche, Vorschau, Favoriten, Kontextmenüs | qwen-coder |
| B-101 | Settings-Overlay: 8 Sektionen, alle Zeilen aus `prefs.json` | qwen-coder |
| B-102 | Preset-Manager (Browsen, Speichern, Umbenennen, Duplizieren, Papierkorb) | qwen-coder |
| B-103 | `+ ADD PART`-Picker (zweistufig, Gruppensymbole, „NO FREE SLOT") | qwen-coder |
| B-104 | `+ ADD STYLE`-Overlay (Name, Ordner-Häkchen ALWAYS/SOMETIMES, Tempo, Farbe) | glm-5.3-flash |
| B-105 | Dialog-/Tooltip-System (Enter/Escape, innerhalb des Fensters) | qwen-coder |

---

## 13. Phase 11 — Export & Drag and Drop

| ID | Aufgabe | Modell |
|---|---|---|
| B-110 | Drag Master: Offline-Render inkl. FX → temporäre WAV → `performExternalDragDropOfFiles` | deepseek-flash |
| B-111 | Drag Stems: je Part durch die volle Kette | deepseek-flash |
| B-112 | Dateinamensschema + `Exports`-Ordner + Kollisionsschutz | qwen-coder |
| B-113 | Datei-Drop auf Karten/ADD-PART (wav/aif/flac/ogg/mp3) + Key-Analyse | qwen-coder |
| B-114 | WAV-Writer in Projekt-Sample-Rate, korrekte Bit-Tiefe | qwen-coder |

---

## 14. Phase 12 — Key & MIDI

| ID | Aufgabe | Modell |
|---|---|---|
| B-120 | `KEY TRACK`: Noten → Transposition, `KEY TRACK OCTAVE`, Oktav-Angleich der Parts | deepseek-flash |
| B-121 | `P`-Flag je Part, Priorität Crunch > Rumble > Rest | qwen-coder |
| B-122 | KEY-Chip-Zustände: Auto / fester Ton / `X` / Amber-Kombination (`A#+G#`) | qwen-coder |
| B-123 | MIDI-Modus (`midi_mode`), Note-On-Retry, All-Notes-Off | qwen-coder |
| B-124 | Key-Detection-Güte-Einstellung (`KEY DETECTION`) | glm-5.3-flash |

---

## 15. Phase 13 — Parity & Politur

| ID | Aufgabe | Modell |
|---|---|---|
| B-130 | Parity-Harness: 44 Werk-Presets, je 3 Kicks, Spektrum-/Hüllkurvenvergleich Original ↔ Nachbau | Lead |
| B-131 | Preset-Portierung: `.itkbpreset` → `.bfkapreset` (Batch-Konverter als Tool) | deepseek-flash |
| B-132 | Performance: Kaltstart < 2 s, Presetwechsel < 150 ms, 12 Slots + 8 Module stabil | Lead |
| B-133 | Crash-Härtung: fehlende Samples, kaputte WAVs, fehlender Content-Ordner | qwen-coder |
| B-134 | Installer (Inno Setup) + portable ZIP + Release-Notes | qwen-coder |
| B-135 | Doku: README, Quickstart, Screenshots, Lizenztexte, Credits | Lead |

---

## 16. Bewusste Abweichungen vom Original

Diese Punkte werden **nicht** 1:1 übernommen — jeweils mit Begründung, damit später
niemand „fehlende Funktion" vermutet:

| # | Original | Nachbau | Grund |
|---|---|---|---|
| A1 | Lizenzsystem `licensekit` mit Online-Aktivierung, stumm ohne Lizenz | keine Aktivierung, voll funktionsfähig | wir bauen kein Kopierschutzsystem nach |
| A2 | Rubber Band als zweiter Stretcher | nur eigene WSOLA-Engine | Rubber Band ist GPL/kostenpflichtig |
| A3 | Teknovault-Logos, `TKNVLT`-Kürzel, Markenfarben | eigenes Branding „Bass Forge" | Vorgabe des Auftraggebers |
| A4 | Sample-Library im Installer (391 MB) | nicht enthalten; Nutzer bindet eigene/installierte Library ein | fremder Content darf nicht weiterverteilt werden |
| A5 | Werksanalyse `.edm` (729 MB) | eigene Analyse beim ersten Start, eigener Cache | Format nicht offengelegt, nicht nötig |
| A6 | OPS-Expansion-Installation (`.kickbuilder`-Paket) | eigenes Expansionsformat (`.bfkapack`) | Originalformat ist proprietär |
| A7 | Auto-Update über teknovault-licensing.com | GitHub-Releases-Update-Check | eigene Infrastruktur |
| A8 | VST2 (laut Notiz vorhanden) | nicht gebaut | im Original-Install gar nicht vorhanden, von Steinberg abgekündigt |
| A9 | `spare1/2`, `gspare1..3` | im Zustand erhalten, nicht in der UI | Preset-Kompatibilität |

---

## 17. Risiko-Register

| Risiko | Wirkung | Gegenmaßnahme |
|---|---|---|
| Stretch-Qualität erreicht Original nicht | Kernfeature wirkt „billig" | frühe Parameterstudie (B-41/46), notfalls zweite Stufe: Phase-Vocoder-Hybrid |
| Auto-Gain-Fenster falsch | Kick klingt anders/leiser | Messung am Original über Drag-Master-Export |
| Normalisierungskurven unbekannt | Presets klingen nach Import anders | B-11 + B-17 Golden-Tests, Werte am Original ablesen |
| Distortion-Kurven unbekannt | Modul klingt anders | Sweep-Messung (Sinus-Sweep durch Modul, Kennlinie rekonstruieren) |
| UI-Umfang größer als gedacht (sehr viele Overlays) | Verzögerung | Overlays erst nach spielbarem Kern (P13) |
| JUCE-Lizenz bei kommerziellem Release | Rechtliches Risiko | vor Release klären, im README dokumentieren |
| Content-Library fehlt beim Nutzer | Plugin „leer" | eigener Demopack aus synthetischen Kicks, klare Erst-Start-Anleitung |
| Parallele Subagenten kollidieren | Merge-Chaos | Verträge-zuerst-Regel, ein Ordner je Aufgabe, Lead integriert |

---

## 18. Meilensteine (Definition von „fertig genug")

| MS | Inhalt | Nutzerwert |
|---|---|---|
| **MS1** | P0–P3: ein Kick aus mehreren Slots klingt, Preset laden/speichern | technisch hörbar |
| **MS2** | P4–P5: Pitch/Stretch/Align funktionieren | „klingt wie ein Kick-Builder" |
| **MS3** | P6: alle 9 FX-Module | „klingt wie das Original" |
| **MS4** | P7 + P8: Randomizer + KICK-UI | **benutzbar** |
| **MS5** | P9–P12: FX-UI, Overlays, Export, Key-Track | **1:1 erreicht** |
| **MS6** | P13: Parity-Messung, Installer, Release | **veröffentlichbar** |
| **MS7** | P14: eigene Zusatzfunktionen | Phase 2 des Auftrags |

---

## 19. Nächste konkrete Schritte (Reihenfolge der Ausführung)

1. **Toolchain prüfen** (MSVC/CMake/Ninja) und Repo anlegen → B-00/B-01.
2. **Verträge schreiben**: `KickState`, `ParameterRegistry`, `FxChain`, `Library`,
   `Engine`, `FxModule`-Interface (Lead, vor jeder Parallelisierung).
3. **Welle 1 (parallel, 5 Subagenten):** B-01/02/03/04/06 (Fundament) ·
   B-12/14 (IO) · B-21/22 (Library-Index) · B-28 (Testdaten) · B-11/17 (Registry/Golden).
4. **Welle 2:** Engine-Core + WSOLA parallel; UI-Gerüst mit Dummy-Daten.
5. **Welle 3:** FX-Module (9 parallel) — der größte Parallelisierungssprung.
6. **Welle 4:** Randomizer + UI-Vervollständigung + Export.
7. **Welle 5:** Parity-Messung, Politur, Release.

Nach jeder Welle: Lead integriert, CI muss grün sein, Fortschritt wird committet und
in `docs/PROGRESS.md` fortgeschrieben.

---

## 20. Phase 2 — FX-Integration externer Plugins (B-140…B-142)

**Ziel:** Die Funktionen von drei Referenz-Plugins in eigene Rack-Module
überführen — ohne Assets, Presets, IRs oder Code der Vorbilder zu übernehmen.
Die Analyse erfolgte an den Installern/VST3-Binaries und den öffentlichen
Handbüchern (Details in `docs/03-RE-FINDINGS.md` §7).

| ID | Modul | Vorbild | Ergebnis | Spec | Status |
|---|---|---|---|---|---|
| **B-140** | `ott` Multiband | Xfer Records OTT 1.37 | 3-Band Up/Down-Kompressor, Depth/Time, Crossovers, Band-Gains, Thresholds, Richtungs-Bypässe, linearphasiger Split | `docs/specs/fx-multiband.md` | fertig |
| **B-141** | `reverb` Convolver | Convology XT 1.34 | True-Stereo-IRs, Time-Hüllkurve, EQ-/Decay-Kurven, Modulation, Stretch, Predelay, Width, Zero-Latency-Kopf | `docs/specs/fx-convolver.md` | fertig |
| **B-142** | `dynEq` Dynamic EQ | TDR Nova | 4 dynamische Bänder, SPLIT/STICKY, HP/LP, W-Band, Auto-Gain, Delta, 7 Prozessziele | `docs/specs/fx-dynamic-eq.md` | fertig |

**Abnahmekriterien je Modul**

1. Build warnungsfrei unter `/W4`, Tests grün.
2. Referenz-Attribute des Originaltyps behalten ihre Bridge-Positionen;
   Zusatzfunktionen liegen im Zustand (Snapshot).
3. Spec mit Parameter-/Funktionstabelle, DSP-Beschreibung und bewussten
   Abweichungen.
4. Bypass/Neutralzustand nachweisbar (Parameter-Defaults).

**Nicht enthalten (bewusst):** Factory-IR-Library, externe Sidechain-Kanäle,
Preset-Browser/Automation-Table der Vorbilder, UI-Nachbau der Vorbilder.
---

## 21. Nachtrag 2026-11-17 — Tickets der Wellen 7 und 8

Nach dem Masterplan sind inzwischen weitere Tickets entstanden und **erledigt**.
Sie sind hier nachgetragen, damit die Nummernfolge vollständig bleibt;
Einzelheiten in docs/PROGRESS.md und im jeweiligen Handoff.

| Ticket | Inhalt | Status |
|---|---|---|
| B-143 | FX-Kette aus dem Zustand bauen, extraState reparieren, PDC melden | erledigt (Welle 7) |
| B-144 | Modul-API für die UI (setModuleParam, writeBackModuleStates, …) | erledigt (Welle 7) |
| B-145 | Demo-Kette, Hörprüfung, Statuszeile | erledigt (Welle 7) |
| B-146 | Externer Sidechain für dynEq (zwei optionale Busse, Detektor-Latenzausgleich) | erledigt (Welle 8) |
| B-93/B-94 | .bfkapreset-IO, Modul-Presets, A/B, Ketten-Header (Lock/Clear/Default) | erledigt (Welle 8) |
| B-92b | Graph-Editoren mseq, shaper, inal | erledigt (Welle 8) |
| B-147 | shaper bekommt die Kick-Länge; Finalizer-Latenz im Host verifiziert | erledigt (Welle 8) |
| B-148 | Rampen für Ketten-mix/outGain (O4) und Convolver-wet/width | erledigt (Welle 8) |
| B-149 | CPU-Profil je Modul, Budgets dokumentiert | erledigt (Welle 8) |
| B-95 | Eingebettete Samples im Preset (<Samples>) | offen |
| B-150 | getTailLengthSeconds() aus Kette/Kick-Länge ableiten | offen |
| B-151 | Karten-Layout für Module mit vielen Parametern (dynEq, 64) gruppieren | offen |
| B-152 | IR-Browser mit User-Directories | offen |
| B-153 | Release-CPU-Messung und Sidechain-Hosttest in einer DAW | offen |

**Getestete Eigenschaften:** siehe docs/specs/fx-chain-engine.md (Abschnitt 8
und 9.1). **375 zusätzliche Assertions** seit Welle 6; aktueller Stand
**305 Testfälle / 539.977 Assertions**.
