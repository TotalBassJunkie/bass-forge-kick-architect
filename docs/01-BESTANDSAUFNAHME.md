# 01 — Bestandsaufnahme: The Teknovault Kick Builder 1.1.2

**Projekt:** Bass Forge Kick Architect (Arbeitstitel-Nachbau)
**Stand dieser Aufnahme:** 2026-11-17
**Quelle:** lokal installierte Kaufversion, Plugin-Version 1.1.2, Content-Revision „R33" (28.09.2026)

> Dieses Dokument ist die sachliche Ist-Aufnahme. Es beschreibt **was existiert**, nicht
> was wir bauen wollen (das ist `02-MASTERPLAN.md`). Alle Pfade wurden auf diesem
> Rechner verifiziert.

---

## 1. Auftrag (aus der Ausgangs-Notiz)

1. Plugin funktional **1:1** nachbauen — zuerst nur die vorhandene Funktion, keine Extras.
2. Später eine zweite Phase mit eigenen Zusatzfunktionen.
3. Nachbau in **kleinen Schritten**, arbeitsteilig über **viele parallele Subagenten**,
   bevorzugt mit kostenlosen bzw. sehr günstigen, aber code-starken Modellen.
4. **Keine Logos** des Original-Plugins übernehmen → eigenes Branding.
5. Fortschritt liegt dauerhaft in einem **öffentlichen GitHub-Repository**, damit andere
   das Ergebnis herunterladen können.

---

## 2. Ausgangsmaterial auf der Platte

### 2.1 Programme

| Artefakt | Pfad | Größe | SHA-256 |
|---|---|---|---|
| Standalone-App | `C:\Program Files\Teknovault\The Teknovault Kick Builder\The Teknovault Kick Builder.exe` | 14.887.720 B | `5018322d92c7b834ffe5bdbe6a0cc30d436055abf6b3c574f3d47fb3277027cf` |
| VST3-Binary | `C:\Program Files\Common Files\VST3\The Teknovault Kick Builder.vst3\Contents\x86_64-win\The Teknovault Kick Builder.vst3` | 15.195.424 B | `ddbce8274cbc070249efa828ecb28bebc8abcf5f76fab316b5e0072170abcf2e` |
| VST3-Modulinfo | `...\The Teknovault Kick Builder.vst3\Contents\Resources\moduleinfo.json` | 1.512 B | – |
| Handbuch | `C:\Program Files\Teknovault\The Teknovault Kick Builder\The Teknovault Kick Builder Manual.pdf` | 7.500.118 B, 37 Seiten | – |

**Befund 1 — Es gibt keine VST2-Version.** Die Ausgangs-Notiz nennt zusätzlich eine
`.dll`-VST2-Version. Auf diesem Rechner existiert sie nicht: weder unter
`Common Files\VST3`, `Common Files\VST2`, `Program Files\VstPlugins`, noch in der
Registry (`HKLM\SOFTWARE\Teknovault\ITKB` kennt nur `ContentPath`, `Version`,
`ManualPath`). Der VST2-Download wurde offenbar nie installiert. Für den Nachbau
bedeutet das: **Ziele sind VST3 + Standalone** (wie das Original sie tatsächlich
ausliefert); VST2 ist optional und ohnehin von Steinberg abgekündigt.

**Befund 2 — Content- und User-Pfade** (aus `Where everything is.txt` + Registry):

| Zweck | Pfad |
|---|---|
| Sample-Library + Factory-Presets | `C:\Users\Public\Documents\Teknovault\The Teknovault Kick Builder` |
| Eigene Presets/Sounds/Libraries/IRs | `C:\Users\tbj\Documents\Teknovault\The Teknovault Kick Builder` |
| Settings + Caches | `C:\Users\tbj\AppData\Roaming\The Teknovault Kick Builder` |
| Lizenz | `C:\Users\tbj\AppData\Roaming\Teknovault\Licenses` |
| Exporte (Ziel) | `C:\Users\tbj\Documents\Teknovault Kick Builder\Exports` |

### 2.2 Sample-Library (Content) — Zahlengrundlage

| Posten | Wert |
|---|---|
| WAV-Dateien gesamt | **2.443** |
| WAV-Volumen gesamt | **391,1 MB** |
| Impulsantworten (`Impulses\**`) | **75** WAV |
| Factory-Kick-Presets | **44** `.itkbpreset` |
| FX-Chain-Presets | **76** `.xml` |
| Analyse-Archiv | `Analysis\analysis.pack` = **729.795.954 B**, 2.405 Einträge |
| Analyse-Index | `Analysis\index.txt`, 2.443 Zeilen (1:1 zu den WAVs) |

Die Ordnerstruktur ist die eigentliche „Datenbank" des Plugins und deckt sich mit den
Pools in `genres.json`:

```
Main - Punches/            Clean EDM · Industrial · Industrial (Extended) · Newstyle
                           Rawstyle · Techno · Techno (Heavy) · Zaag
Main - Crunches/           Industrial Heavy · Newstyle & Midintro (Melodic) · Psy Style (Melodic)
                           Rawstyle (Melodic) · Reverse Bass (Melodic) · Zaag (Melodic)
Main - Rumbles/            Rumble (Rawstyle) · Rumbles (Bigroom) · Rumbles (Techno) · Schranz
Transients (Clicks)/       Transient Click N 150.wav
Transients (Clicks) [Zaag]/73 Dateien
Transients (Percs)/        Transient Perc N 150.wav
Transients (Punch Top)/    Punch Top N 150.wav
Transients (Newstyle Tops)/
Enhancers (Rumbles)/       Enhancer Low Mid N 150.wav
Enhancers (Schranz)/       Enhancer Schranz N 150.wav
Enhancers (Tops)/          Enhancer Top N 150.wav
Enhancers (Punch Reverbs)/ Reverbs (Industrial) · (Newstyle) · (Rawstyle)
Impulses/                  Clicks & Snaps · Reverbs · Rumble Tails · Sub Drops
                           Textures · Tonal Bodies
```

**Befund 3 — Tempo steckt im Dateinamen.** Samples heißen
`Punch Zaag 12 A 150.wav`, `Crunch Industrial 101 160.wav`, `Reverb Industrial 1 150.wav`.
Damit sind zwei Dinge schon im Namen kodiert, die das Plugin nutzt:

* **BPM** (letzte Zahl, z. B. `150`, `155`, `160`) → Grundlage für `BPM STRETCH / SYNC BPM`,
  Fallback laut Handbuch: 150.
* **Tonart** bei melodischen Pools (`... 12 A 150.wav`, `... 11 G# 150.wav`) → der
  Crunch/Rumble trägt den Grundton im Namen; die Analyse bestätigt ihn.

### 2.2.1 Audioformate der Library (Header-Scan aller 2.443 Dateien)

Die Library ist **nicht** einheitlich — das ist für den WAV-Import wichtig:

| Format | Anzahl |
|---|---|
| PCM 24 bit, 2 Kanäle, 44,1 kHz | 1.320 |
| IEEE-Float 32 bit, 2 Kanäle, 44,1 kHz | 747 |
| PCM 16 bit, 2 Kanäle, 44,1 kHz | 336 |
| PCM 24 bit, **1 Kanal**, 44,1 kHz | 32 |
| IEEE-Float 32 bit, 2 Kanäle, **48 kHz** | 8 |

Verteilung nach Gruppe (Auszug): Punch-Ordner mischen 24-bit-PCM / 32-bit-Float /
16-bit-PCM (425 / 280 / 105), Rumbles sind überwiegend 24-bit-PCM (202) mit 52 Float-Dateien,
die Punch-Reverbs und Schranz-Enhancer sind durchgehend 24-bit-PCM.
`Impulses/` enthält alle drei PCM-Varianten plus 8 Float-Dateien mit 48 kHz, davon 32 mono.

**Konsequenz:** Der Reader muss 16/24-bit-PCM und 32-bit-Float, mono und stereo sowie
44,1 und 48 kHz verarbeiten und auf die Projekt-Rate resampeln. JUCEs `AudioFormatManager`
deckt das ab; die Analyse- und Stretch-Stufen dürfen keine feste Rate annehmen.

Beispielwerte einer Rumble-Datei: 2 Kanäle, 44,1 kHz, 24 bit, 105.840 Samples = 2,4 s,
635.120 B — das entspricht exakt 2,4 s × 44.100 Hz.



* `prefs.json` (1002 B): flache Key/Value-Liste der Einstellungen, u. a.
  `theme`, `customBase`, `customAccent` (z. B. `4287226368` = ARGB), `visStyle`, `lookDemon`,
  `specTilt` (4.5), `specRange` (84), `autoGain`, `slots12`, `textBright`, `playOnRoll`,
  `lcdScreen`, `vizAnim` (3), `lightFx`, `gradientOn`, `gradientTo`, `vizChannel`,
  `updates`, `updateLatest` (JSON-Blob), `chaosMask`, `alignNotices2`.
  → **Vollständige Settings-Oberfläche ist hier ablesbar.**
* `last_session.state` (1.109.112 B): der wiederhergestellte Letztzustand — die Größe
  zeigt, dass **Audiodaten (mit eingebettete Samples) im Session-State liegen**
  (vgl. Handbuch: „ein gespeichertes Projekt und ein User-Preset tragen die Audio­daten
  selbst").
* `EngineDCache/`, `FxChainPresets/`: Caches bzw. User-Chain-Presets.
* `Licenses/act_*.lic` (641 B), `online.json`, `state.json` — Lizenzsystem (`licensekit`).

---

## 3. Technischer Steckbrief des Originals

| Merkmal | Wert | Herkunft |
|---|---|---|
| Framework | **JUCE v8.0.7** | String `JUCE v8.0.7`, `juce_version_8_0_7` |
| VST3-SDK | **VST 3.7.12** | `moduleinfo.json` |
| Compiler | MSVC (x64), C++20 (`_Ref_count_obj2`, `Concurrency`-RTTI) | RTTI-Namen |
| Klassen-IDs | `ABCDEF019182FAEB544B4E5649746B62` (Processor) / `…1234ABCD…` (EditController) / `…C0DEF00D…` (Compatibility) | `moduleinfo.json` |
| Kategorien | Instrument / Synth | `moduleinfo.json` |
| Grafik | Direct2D/Direct3D-11-Renderer (JUCE `Direct2DHwndContext`, D2D/DXGI/D3D11-Imports) | PE-Imports + RTTI |
| Audio-Imports | WASAPI, DSOUND, WINMM (MIDI), ole32, COMCTL32 | PE-Imports |
| Netz | WININET + WS2_32 (Update-Check, Online-Aktivierung) | PE-Imports |
| Crash-Handling | `dbghelp` (`SymFromAddr`) | PE-Imports |
| Timestretch | **eigene WSOLA-Engine** (`itkb::engined::WsolaPrepared`, `WsolaNPrepared`, `Stft`, `Sig`, `Model`) **+ Rubber Band** (String `Rubber Band`) | RTTI + Strings |
| Pitch-Engine | `itkb::PitchEngineD` mit `FactoryPack` (Analyse-Paket) | RTTI |
| Lizenz | `licensekit` (`OnlineActivation`, `Guard`, `SignatureThread`, `DemoBadge`) | RTTI |

**Befund 4 — Die App ist keine „vorgeschobene" GUI.** Der Audio-DSP, die Analyse,
das Stretching und der Renderer stecken alle in derselben Binary; Standalone und VST3
teilen sich den Code (nur `ITKBProcessor` als gemeinsame Basis). Es gibt also **keine
versteckte zweite Engine**, die man separat analysieren müsste.

---

## 4. Rekonstruierte Architektur (aus RTTI-Klassennamen)

Die Klassennamen der Binary ergeben eine vollständige Landkarte. Gruppiert:

### 4.1 Plugin-Rumpf
| Klasse | Rolle |
|---|---|
| `ITKBProcessor` | `juce::AudioProcessor`; Host des gesamten Zustands |
| `ITKBEditor` | `juce::AudioProcessorEditor`; hält alle Seiten |
| `itkb::SnapshotSource`, `ProcessorSnapshotSource` | Zustands-Snapshots für Undo/Anzeige |
| `itkb::FxSnapUpdater`, `FxSnapUpdater` | FX-Ketten-Snapshots |
| `itkb::AlignmentResult`, `AlignResult` | Ergebnis von ALIGN/PHASE ALIGN |
| `itkb::LevelCurve`, `itkb::PoolDef`, `itkb::CompositeRenderer` | Pegelkurven, Pool-Definitionen, Offline-Renderer |
| `hostlanes::LaneFloat/Int/Bool/Choice/LaneLink` | generisches Parameter-Lane-System (Automatisierung/VST-Parameter-Brücke) |
| Timer-Klassen: `KeyTimer`, `KeyVoteSoon`, `GateRefresh`, `PreRollTimer`, `HostLaneTimer`, `CardTicker`, `GuardTimer`, `DiagTimer`, `AudioPrepThread`, `WarmThread`, `BadAudioRecover` | Nebenläufigkeit, Key-Detection-Nachlauf, Host-Sync, Warm-Up |
| `LibInstaller` | Installation/Update der Content-Library |

### 4.2 DSP-Engine (`itkb::engined`)
| Klasse | Rolle |
|---|---|
| `Stft`, `Sig`, `Model` | STFT-Analyse, Signal-Container, Datenmodell |
| `WsolaPrepared`, `WsolaNPrepared` | vorbereitete WSOLA-Grain-Suche (normal / N-Kanal) |
| `Cancelled`, `RenderCancelled`, `NumpyError` | Abbruch-/Fehlerpfade im Offline-Renderer |
| `PitchEngineD::FactoryPack` | tonale Analyse „ab Werk" (vorinstallierte Analyse) |

### 4.3 FX
`itkb::FxModule` (Basis), `IrStretchPing` (IR-Stretch-Nachricht), `FxProxyParam`
(Generik-Parameter je Modul) — die konkreten Module sind **nicht** als eigene C++-Klassen
sichtbar, sondern laufen über einen Modultyp-Schalter (String-Feld `type=`) und je 14
generische Parameter `p0..p13`.

### 4.4 UI
`TeknovaultLookAndFeel`, `KickViewer`, `LayerPanel`, `AlignPanel`, `FxPage`, `RackContent`,
`FxModuleCard`, `FxKnob`, `FXSection`, `FXTabPanel`, `TransportBar`, `PresetBar`,
`PresetPill`, `PresetManagerOverlay`, `SavePresetOverlay`, `SettingsOverlay`,
`AddPartOverlay`, `AddStyleOverlay`, `AddLayerTile`, `ExpansionInstallOverlay`,
`OpsLibraryPanel`, `ChaosMenu`, `ChaosChips`, `SideStyleTab`, `GenreTabBar`, `StatusBar`,
`LogoLink`, `SwatchButton`, `SkinTile`, `PartChip`, `PanelSwitch`, `DragChip`,
`LabeledKnob`, `RCSlider`, `EqNodeEditor`, `ReverbEnvEditor`, `FreqBandSlider`,
`ColourWheel`, `ButtonNudger`, `ExpandStrip`, `VizCardPanel`, `VizKey`, `HoverTag`,
`ItkbDialog`, `FlatTextButton`, `TextAnalysis`.

### 4.5 Lizenz
`licensekit::{OnlineActivation, Guard, NetWorker, LicensePanel, DemoBadge, DemoNotice,
SignatureThread, KeyFieldFilter, LicenseHeaderDress, LicenseButtonLook}`.

> **Für den Nachbau:** Lizenz/Aktivierung wird **nicht** nachgebaut (siehe Masterplan,
> Abschnitt „bewusste Abweichungen"). Die Struktur ist aber wichtig zu kennen, weil
> das Original ohne Aktivierung stumm schaltet und nicht speichern lässt — beim Testen
> des Originals ist das relevant.

---

## 5. Funktionslandkarte (aus Handbuch + Preset-Inventar verifiziert)

### 5.1 Grundprinzip
```
Samples (je Slot: Pitch, Stretch, Trim, Fades) → ALIGN → Summe
   → FX-Rack (frei bestückbare, sortierbare Modulkette) → Output
```
**Ein** Kick pro Instanz, aus bis zu **12 Slots** (Standard 6, erweiterbar).
Kein Sequencer — der Kick ist ein One-Shot, per MIDI-Note spielbar (KEY TRACK).

### 5.2 Zwei Seiten
* **KICK**: Style-Sidebar, Transport, Visualiser, Slot-Karten.
* **FX**: vertikales Rack, Modulkarten, Ketten-Sidebar.

### 5.3 Header
`KICK / FX`-Umschalter · Preset-Pille (mit Pfeilen + Stern für ungespeicherte Änderung) ·
Save (Overwrite / Save as New) · ACTIVATE (Lizenz) · Undo/Redo · Library · Gear (Settings).

### 5.4 Sidebar
RANDOMIZE · CHAOS (4 Toggles: Pitch ±3 HT, Stretch, FX Chains, FX Modules) ·
FREE BUILD · BOTH LIBRARIES (nur mit OPS-Expansion) · TKNVLT/OPS-Umschalter ·
Style-Liste · `+ ADD STYLE`.

**Die 10 Factory-Styles** (aus `genres.json` belegt):

| id | Anzeige | Akzent | Tempo | Init-Preset |
|---|---|---|---|---|
| `rumble_kicks` | RUMBLE | `#DE2A2A` | 150 | Rumble Magic |
| `reverse_bass` | REVERSE BASS | `#7A3DD6` | 150 | Reverse Boss |
| `industrial` | INDUSTRIAL | `#6E7480` | 160 | Wire Cutter |
| `psy` | PSY | `#7CC41E` | 150 | PsyMeister |
| `schranz` | SCHRANZ | `#E05A1A` | 160 | Schranz Explosion |
| `rumble_kicks_clean` | RUMBLE CLEAN | `#D64848` | 150 | Drop Puncher |
| `midintro_newstyle` | NEWSTYLE | `#3D6BD6` | 150 | THE NEW WAY |
| `rawstyle` | RAWSTYLE | `#C81E5A` | 150 | Crunch Boost |
| `zaag` | ZAAG | `#1FB5A0` | 150 | Zaag Meister |
| – | FREE BUILD | – | – | – |

### 5.5 Transport-Zeile
Play/Loop/Stop · BPM (FOLLOW DAW oder MANUAL) · KEY-Chip (Auto / fester Grundton / `X`)
· KEY TRACK · KEEP FADES · ALIGN-Chip · COPY TO FREE BUILD · EMPTY ·
OUT-Meter (= Master-Fader, Doppelklick = Reset).

### 5.6 Visualiser
VIEW: `SHOW PARTS` / `FOCUS` / `ONE PART` × `SUM` / `POST` / `CLIP` × `STEREO FULL` /
`MID` / `SIDE`. FREQ: BAND (zwei ziehbare Punkte) oder QUICK (`ALL`, `SUB`, `BODY`, `NO SUB`).
Interaktiv: bewegen, trimmen, Fades (Tabs, Anker, Curve-Diamonds, Modus per Rechtsklick:
Linear/Curve/S-Curve/Fast/Slow), Doppelklick = Originalposition, Höhen-Strip.
Grid mit Beat-Unterteilungen + „1 BEAT = ms @ BPM".

### 5.7 Slot-Karte (je Part)
Rollenband (PUNCH / CRUNCH / RUMBLE / TOP / TRANSIENT; Perc/Punch-Top/„Punch Reverb"
lesen als PUNCH, Rumble-Enhancer als RUMBLE, Schranz-Enhancer als TAIL+; eigene Datei
als DRAG & DROP) · Namenszeile mit Pfeilen + Datei-Liste + `Browse file…` +
`Reset to kick default` · VOL (0 dB … +10 dB) · PITCH (Semitöne, längenneutral) ·
STRETCH (0.25×–8×) · RANDOMIZE · Padlock (Lock) · Power/Mute · Solo · `P` (Key-Track-
Teilnahme) · `X` (entfernen) · Pegel-Strip. Rechtsklick auf jeden Regler: Reset /
Wert eintippen / Copy & Paste.

### 5.8 ALIGN & PHASE ALIGN
`ALIGN` schiebt Rumbles/Crunches lückenlos an den Punch; `ALIGN BY`: MOVE CRUNCH |
STRETCH PUNCH (nur KEEP PITCH); `HOW TIGHT`: TIGHT | NORMAL | GENTLE;
`STRETCH CRUNCH` füllt Restlücken; `PHASE ALIGN` schiebt/dreht Parts für maximalen
Low-End; `PARTS THAT MAY MOVE` (Chips, „MOVED BY HAND" = Handplatzierung gewinnt);
`STAYS PUT` (Anker-Part, wenn kein Library-Punch); `LAST PASS`-Report.
Nach PHASE ALIGN invertierte Parts zeigen ein Symbol zum Zurückdrehen.
Settings: `ALIGN BLEND`, `RESULT LINE`.

### 5.9 Library-Overlay
Tabs `PRESETS` · `SOUNDS` · `FX CHAINS` · `MY SOUNDS`; Suche; Vorschau-Panel
(„woraus besteht der Preset"); Favoriten-Stern; Rechtsklick-Menü (Load, Show in Explorer,
Duplicate, Copy to User, Rename, Trash); `SOUNDS` = Dateibaum mit Zählern und
„ähnliche Sounds"-Reihe; `FX CHAINS` mit Ordnern und `+ ADD FOLDER`; `MY SOUNDS` =
eigene Libraries (verlinkt, nicht kopiert, 4 Ebenen tief, Hintergrund-Analyse).

### 5.10 FX-Rack
9 Modultypen:

| Typ (`type=`) | Name | Parameter (aus XML) |
|---|---|---|
| `eq` | Equalizer | `lowgain lowfreq midgain midfreq midq highgain highfreq middyn` + bis 12 `<Band kind f q g dyn ch>` |
| `mseq` | Mid/Side EQ | `midlow midhigh sidelow sidehigh width middyn sidedyn msolo ssolo mmute smute` + `<Band … ch>` |
| `dist` | Distortion | `drive mode folds agc eqmode flip` + `<WNode x y ten seg>`-Kurve |
| `shaper` | Kick Shaper | `sPre sPost sLine shape amount` + `<Pt x y t>`-Hüllkurve |
| `trans` | Transient Shaper | `amount attack duck speed` |
| `reverb` | Convolver | `ir wet stereo tone speed` + `<Pt x y t>`-Hüllkurve |
| `final` | Finalizer | `boost freq clip upward monoF shelfOn monoOn clipOn` |
| `ott` | Multiband | `depth time x1 x2 gl gm gh linear` |
| `filt`* | Filters | (*nicht in Default-XML; Typ laut Handbuch vorhanden: LP/HP 6–30 dB/Okt, Band, Notch, Phaser 1–6, Comb) |
| – | – | je Modul zusätzlich `on`, `mix`, `outGain` |

Handbuch-Details, die aus den XML-Dateien **nicht** hervorgehen und separat zu klären sind:
* **Distortion-Modi:** HARD CLIP, SOFT CLIP, SATURATE, WAVEFOLD (FOLDS), FUZZ, RECTIFY,
  HALF WAVE (FLIP), BITCRUSH, WAVESHAPER, WAVESHAPER ASYM — plus `EQ`-Chip OFF/PRE/POST.
* **Waveshaper-Segmentmodi:** Linear, Curve, Half Sine, Sine, Double Wave, Square,
  Steps/Quantize, Blocks.
* **Filter:** LP/HP 6·12·18·24·30 dB/Okt mit resonanter Knie, BAND 12/24 dB, NOTCH,
  PHASER (1–6 Notches, Polarität ±), COMB (Polarität ±).
* **Convolver:** Zero-Latency, MIX equal-power (Default 50 %), WET, STEREO, TONE (Tilt),
  SPEED (IR-Stretch), ziehbare Hüllkurve.
* **Finalizer:** SUB (BOOST unter FREQ), MONO BASS (unter BELOW), CLIP = True-Peak-Ceiling,
  drei eigene Power-Schalter.
* **Multiband (OTT):** 3-Band Up-/Downward-Kompressor, DEPTH, TIME, HIGH/MID/LOW-Gain,
  je Band UP/DOWN, `LINEAR PHASE`-Split, ziehbare Crossover.

### 5.11 Presets / Export
Preset speichert **alles**: Samples (inkl. Audio bei User-Presets/Projekten), Slot-Edits,
Locks, ALIGN-Settings, KEY TRACK, View-Modi, Stretch-/Tempo-Modi, manueller Key, FX-Rack.
**DRAG MASTER** (fertiger Kick inkl. FX als WAV) und **DRAG STEMS** (jeder Part einzeln,
durch die volle Ausgangskette), Sample-Rate = Projekt, Länge nach `KICK LENGTH`,
Dateinamen `TKNVLT - KBVST - name (date) time`, Ablage in `…\Exports`.

### 5.12 Settings (8 Sektionen)
`GENERAL` (PLUGIN SIZE, UNDO HISTORY, ON LAUNCH, TOOLTIPS, SLOT GRID) ·
`SOUND` (KICK LENGTH, BPM STRETCH, STRETCH MODE, PLAY ON ROLL, AUTO GAIN) ·
`TEMPO & KEY` (TEMPO, KEY DETECTION, KEY TRACK OCTAVE) ·
`ALIGN & PHASE` (ALIGN BLEND, RESULT LINE) ·
`LOOK` (COLOUR THEME, CUSTOM COLOUR, GRID RULER, GRID DIVISIONS, TEXT BRIGHT) ·
`VISUALISER` (WAVEFORM SKIN, LCD SCREEN, ANIMATION, EFFECTS FULL/LIGHT) ·
`SPECTRUM` (SLOPE, RANGE, DETAIL) · dazu `Read Manual`, `License`.

### 5.13 Key-Track
Bibliotheks-Samples tragen ihre Tonart aus der Werksanalyse (Crunch/Rumble = tonal,
`P` automatisch an; Punch = `P` aus). Eigene Samples werden beim Import analysiert;
ohne klare Note → `X`. Zwei unterschiedliche Tonarten sind erlaubt; der KEY-Chip zeigt
dann z. B. `A#+G#` in Amber. `KEY TRACK OCTAVE` (Default C4) legt fest, wo der Kick
untransponiert klingt; Parts werden in **eine** Oktave zusammengezogen.
**PITCH und STRETCH sind getrennt**: PITCH ändert nur die Tonhöhe (Länge bleibt),
STRETCH nur die Länge. `STRETCH MODE`: KEEP PITCH | REPITCH.
`BPM STRETCH`: SYNC BPM (aus dem Dateinamen, Fallback 150) | NATIVE.

---

## 6. Datenformate (im Detail)

### 6.1 Kick-Preset `.itkbpreset` — XML, `schemaVersion="2"`

```xml
<ITKBPreset schemaVersion="2" name="The Nasty" author="Teknovault"
            genreId="rawstyle" appVersion="4.0.3">
  <Slot index="0" role="PUNCH">
    <SampleRef relPath="Main - Punches/Rawstyle/Punch Heavy Raw 13.wav"
               absPath="" fileName="Punch Heavy Raw 13.wav" roleId="punch"/>
  </Slot>
  <Slot index="4" role="RUMBLE"> … </Slot>
  <Params>
    <P id="l0_vol" v="0.699840486"/>   …
    <P id="master_vol" v="0.833333313"/>
    <P id="fx0_p0" v="0.500000000"/>  …
  </Params>
  <FxChain><fxSnap><FxChain bypassAll="0"> … Module … </FxChain></fxSnap></FxChain>
</ITKBPreset>
```
Beobachtungen:
* Nur **belegte** Slots erscheinen als `<Slot>`; `index` 0…11.
* `role` ist der Anzeigename des Rollenbands, `roleId` der Pool-Rollen-Key.
* Werte sind auf **9 Nachkommastellen normalisiert** (0…1), nicht in Prozent/dB.
  Beispiel: `master_vol 0.8333 = −1,58 dB`? → tatsächlich 20·log10(0.8333) = −1,58 dB
  bzw. Skalenabbildung; muss je Parameter verifiziert werden.
* `l<N>_stretch = 0.374989420` bei sehr vielen Presets → das ist **nicht** der
  Stretch-Faktor 1.0, sondern ein normalisierter Wert auf einer 0.25…8-Kurve.
  → **Wichtig für die 1:1-Nachbildung: Normalisierungskurven müssen bestimmt werden.**

**Vollständige Parameterliste eines Presets** (aus `The Nasty.itkbpreset`):

*Pro Slot N = 0…11:* `lN_vol`, `lN_pitch`, `lN_mute`, `lN_solo`, `lN_pos`,
`lN_trimstart`, `lN_trimend`, `lN_fadein`, `lN_fadeout`, `lN_stretch`, `lN_filter`,
`lN_filterq`, `lN_dlytime`, `lN_dlymix`, `lN_spare1`, `lN_spare2`,
`lN_ficurve`, `lN_fishape`, `lN_foshape`.

*Global:* `sat_amount`, `reverb_size`, `reverb_damp`, `reverb_mix`, `hollow_amt`,
`max_amount`, `master_vol`, `midi_mode`, `root_note`, `gspare1`, `gspare2`, `gspare3`.

*FX (8 Slots × 14):* `fx0_p0` … `fx7_p13`. `p12` korreliert in den untersuchten Presets
mit dem `mix` des Moduls (z. B. Convolver `mix="0.3189999759197235"` ↔ `fx2_p12=0.319`).

`spare*`/`gspare*` sind unbenutzte Reserve-Parameter → beim Nachbau als Reserve
mitnehmen (Preset-Kompatibilität), aber nicht in der UI zeigen.

### 6.2 FX-Chain-Preset `.xml` — `<FxChainPreset><FxChain>`

```xml
<FxChainPreset>
  <FxChain bypassAll="0">
    <Module type="eq"    on="1" mix="1.0" outGain="1.0" lowgain="0.0" lowfreq="90.0" …>
      <Band kind="1" f="71.64" q="0.676" g="15.70" dyn="0.0" ch="0"/>
    </Module>
    <Module type="mseq"  … > <Band …/></Module>
    <Module type="dist"  … > <WNode x="-1" y="-1" ten="0.95" seg="1"/></Module>
    <Module type="shaper"… > <Pt x="0" y="1" t="0"/></Module>
    <Module type="trans" … />
    <Module type="reverb" … ir="Reverbs/Plate Dark.wav"><Pt …/></Module>
    <Module type="final" … />
    <Module type="ott"   … />
  </FxChain>
</FxChainPreset>
```
Die Reihenfolge der `<Module>`-Elemente **ist** die Signalkette. `Band kind`: 0=BELL,
1=L-SHF(o. L-CUT), 2=H-SHF(o. H-CUT) — genaue Zuordnung noch zu verifizieren
(Hypothese: identisch zu den Equalizer-Bandtypen des Handbuchs).
Default-Kette `Default INIT.xml` = eq, mseq, dist, shaper, trans, reverb, final, ott.

### 6.3 `genres.json` — Style-/Pool-/Regel-Definition, `schemaVersion 3`
Struktur: `roleRegistry` (8 Rollen) · `kitPattern` · `categoriesRequired` ·
`rules` (globale Ausschlussregel „melodic × atonal") · `pools[]` (25 Pools: `id`,
`folder`, `role`, `tonality`, `defaultGain`, `duplicateGain`, `fileWeights`, `uiOrder`) ·
`styles[]` (`pools` mit Gewichten, `layerRules` min/max je Rolle, `rules` mit
`code`/`pools`/`db`/`dbPair`/`shares`, `addable`, `defaultPreset`).

Regel-Semantik (Hypothese, zu verifizieren): `code` steuert, **ob/wie oft** eine Regel
greift (`1` = immer, `2` = optional/wahrscheinlich, `3` = „genau einer aus dieser
Gruppe", `4` = optional mit anderer Wahrscheinlichkeit, `a-b` = „a bis b Teile"),
`db`/`dbPair` die Pegelbereiche (dB) für optionale Parts, `shares` die prozentuale
Verteilung zwischen Pool-Gruppen.

### 6.4 `Analysis\analysis.pack` — eigenes Containerformat „KBAP"
```
Offset  Bytes  Inhalt
0       4      "KBAP"
4       4      Version = 1 (uint32 LE)
8       4      Eintragszahl = 2405 (uint32 LE)
…       …      2405 × Eintrag:
                uint16 len, char name[len]  ("<16 hex>.edm")
                uint64 offset
                uint32 size_a
                uint32 size_b
```
`index.txt` verknüpft WAV → `.edm` und trägt zwei weitere Zahlen, z. B.
`Reverb Industrial 1 150.wav  211800  4d057b931a2d8d7e.edm  33  0.103633784`
(Größe, Analyse-Datei, eine Ganzzahl — vermutlich Key-/Marker-Kennung, ein Float —
vermutlich Konfidenz). Die `.edm`-Nutzlast ist noch nicht dekodiert (Kandidat: zlib).

> **Konsequenz für den Nachbau:** Wir müssen `.edm` **nicht** lesen können. Wir bauen
> eine eigene, gleichwertige Analyse (Onset/Key/Tonhöhe) und cachen sie im selben
> Prinzip lokal. `.edm`-Import bleibt optionales Extra (Nice-to-have für
> Preset-Portierung). **Wichtig:** Ohne Analyse-Datenbank müssen wir die Library beim
> ersten Start einmal analysieren — das ist bei 2.443 WAVs / 391 MB gut machbar.

### 6.5 `prefs.json`
Flache JSON-Map. Vollständige Settings-Persistenz; dient als Referenz für die
Settings-Defaults. Enthält einen `updateLatest`-JSON-String (Update-Kanal).

---

## 7. Verifizierte Details, die beim Nachbau oft übersehen werden

1. **Pitch ist längenneutral, Stretch ist tonhöhenneutral** — zwei getrennte Pfade
   (PITCH = Resampling auf WSOLA-Ausgabe bzw. Pitch-Shift; STRETCH = WSOLA).
2. **Auto Gain pegelt auf den „Body" des Kicks, nicht auf die Transientenspitze** →
   Transienten dürfen bewusst über 0 dBFS in den Ceiling laufen; „dieser Clip ist der
   Sound". Das ist eine klangprägende Designentscheidung, keine Nachlässigkeit.
3. **Level wird nur beim Rollen gesetzt** — Browsen mit den Pfeilen und das
   Karten-RANDOMIZE lassen `VOL` unangetastet; manuell hinzugefügte Parts kommen mit 0 dB.
4. **RANDOMIZE wiederholt keinen Sample, bis der Ordner durch ist, und nie innerhalb
   von 5 Rolls** — ein No-Repeat-Fenster pro Pool.
5. **Optional-Parts vs. Pflicht-Parts** — Pflichtteile haben kein `X`, optionale ein `X`.
6. **Style-Wechsel ist zustandsbehaftet**: erster Besuch lädt den Style-Init-Kick,
   Rückkehr stellt Samples + Regler + FX-Rack + Presetnamen wieder her (auch nach
   Projekt-Reload).
7. **Slot-Anzahl 6 → 12** schaltet sich automatisch frei, wenn 6 voll sind.
8. **KICK LENGTH = 1 BEAT** schneidet mit weichem Fade am Beat-Ende; `FULL SAMPLE`
   lässt lange Parts ausklingen.
9. **Factory-Presets sind schreibgeschützt**; `Default INIT` der FX-Kette ebenso.
10. **Drag & Drop akzeptiert wav/aif/flac/ogg/mp3** und analysiert die Tonart aus der
    Audiodatei selbst.
11. **Undo umfasst den ganzen Kick-Zustand** (nicht nur einen Regler).
12. **JUCE 8.0.7 + VST3 3.7.12** — Ziel-SDK für den Nachbau, damit sich Host-Verhalten
    (Automation, State, Resize) gleich anfühlt.

---

## 8. Offene Punkte / noch zu klären (Aufgaben für die RE-Phase)

| # | Frage | Warum wichtig | Methode |
|---|---|---|---|
| F1 | Normalisierungskurven aller `lN_*`-Parameter (bes. `stretch`, `pitch`, `vol`, `filter`) | Preset-Kompatibilität + identisches Regelverhalten | RE `ITKBProcessor`-Parameterbereich + Kurzvergleich Original-UI ↔ Preset-Wert |
| F2 | Abbildung `fx<N>_p<M>` → Modulparameter | Preset-Kompatibilität, Automation | RE + Preset-Kreuzvergleich |
| F3 | `pitch`-Semantik: −12…+12 HT? Skala? | Klanggleichheit | Original-UI + Preset-Werte |
| F4 | `KICK LENGTH`-Fade-Kurve, `AUTO GAIN`-Fenster („Body") | Klanggleichheit | Messung am Original (Audio-Capture) |
| F5 | Exakte Transferfunktionen der 10 Distortion-Modi | Klanggleichheit | RE `dist`-Modul + Messung |
| F6 | Filter-Topologie & Slope-Stufen | Klanggleichheit | RE + Messung |
| F7 | OTT-/Multiband-Parameter (Attack/Release-Mapping, `depth`/`time`) | Klanggleichheit | RE + Messung |
| F8 | ALIGN/PHASE-ALIGN-Algorithmus im Detail | Kernfeature | RE `AlignmentResult`-Pfad |
| F9 | `code`-Semantik im Randomizer + `db`/`dbPair`/`shares` | 1:1-Rolls | empirisch (viele Rolls im Original) + RE |
| F10 | `.edm`-Format (nur optional) | Preset-Portierung | Container + zlib-Test |
| F11 | Key-Detection-Güte/Schwelle (`KEY DETECTION`) | eigener Detektor gleichwertig | Messung |
| F12 | WSOLA-Fenster/Hop/Suchbereich des Originals | Stretch-Qualität | RE `WsolaPrepared` + Messung |

---

## 9. Rechtliche und pragmatische Leitplanken

* **Kein Code, keine Assets, keine Logos** aus der Original-Binary übernehmen.
  Analysiert wird, um **Verhalten** zu verstehen; neu implementiert wird selbst.
* **Sample-Library nicht mitliefern.** Die 2.443 WAVs (391 MB) sind Teknovault-Content.
  Unser Plugin **liest** die lokal installierte Library des Nutzers (Content-Pfad
  konfigurierbar), liefert sie aber nicht aus. Für eigene Test-Libraries erzeugen wir
  synthetische Kicks (Skript) + Nutzer-eigene Dateien.
* **Namen:** Produkt heißt **Bass Forge Kick Architect**, Plugin-Code `bfka`,
  Preset-Endung `.bfkapreset`, eigene Farbwelt/Typografie, kein Teknovault-Schriftzug,
  kein `TKNVLT`-Kürzel. Genre-Bezeichnungen (RUMBLE, RAWSTYLE, ZAAG …) sind
  Gattungsbegriffe und bleiben als Style-Namen erhalten — sie sind Teil der 1:1-Funktion.
* **Lizenzfrage JUCE:** Original nutzt JUCE 8.0.7. Wir nutzen JUCE 8 mit
  **JUCE-Personal-Lizenz** (kostenlos unter Umsatzschwelle) bzw. später kommerzieller
  Lizenz — vor öffentlichem Release final zu entscheiden und im README zu dokumentieren.
* **Rubber Band** (GPL/kostenpflichtig) wird **nicht** verwendet; wir implementieren
  Stretching selbst (WSOLA/Phase-Vocoder), lizenzrein.
* **Kein Nachbau des Lizenzsystems.** Der Nachbau ist frei nutzbar; die `licensekit`-
  Funktionalität entfällt (bewusste Abweichung, im Masterplan dokumentiert).

---

## 10. Zusammenfassung in drei Sätzen

Das Original ist ein JUCE-8.0.7-VST3/Standalone-Plugin, das aus bis zu zwölf
Sample-Slots einen Techno-Kick baut (Pitch/Stretch/Trim/Fades je Slot), die Teile
per ALIGN/PHASE ALIGN zusammenschiebt und die Summe durch ein frei bestückbares
FX-Rack aus neun Modultypen schickt.

Der komplette Zustand ist als XML (`.itkbpreset`, `FxChainPreset`) offengelegt, die
Randomizer-Regeln liegen als `genres.json` offen, die Sample-Library samt
Werksanalyse (2.443 WAVs, `.edm`-Cache) ist auf der Platte — damit ist die
**Funktions- und Parameterseite vollständig dokumentierbar**, ohne die Binary
vollständig dekompilieren zu müssen.

Unbekannt und nur per RE/Messung zu klären sind die **DSP-Innenleben** (WSOLA-Parameter,
Distortion-Kurven, Filter-Topologie, OTT-Verhalten, Auto-Gain-Fenster, ALIGN-Algorithmus)
und die **Normalisierungskurven** der Parameter — das ist der Kern der RE-Phase.
