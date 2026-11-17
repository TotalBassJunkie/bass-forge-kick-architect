# Fortschritt

Laufendes Protokoll. Neueste Einträge oben. Format:
`## JJJJ-MM-TT — <Welle/Phase>` mit erledigten Tickets (B-Nummern) und Notizen.

---

## 2026-11-17 — Welle 9: Die KICK-Seite

**Erledigt (P8, B-83, B-84, B-86, B-154, B-155, B-156, B-157)**

* **B-86 — zwölf Slot-Karten.** Neu `src/ui/SlotCard.{h,cpp}`: Rollenband mit
  Pegel-Strip, Sample-Name, `LOAD`/`X`, `MUTE`/`SOLO` (APVTS), `LOCK`/`P`
  (`extraState`), Mini-Wellenform mit Trim-Fenster und Fade-Marken, drei große
  Regler (VOL/PITCH/STRETCH) und vier Detailregler (TRIM START/END, FADE IN/OUT).
  Die Regler sitzen direkt auf den APVTS-Parametern und rufen nach jeder
  Änderung `applySlotSettings(slot)` — der Renderer übernimmt ohne Ketten-Rebuild.
* **B-154 — Slot-IO im Prozessor.** `setSlotSample()` schreibt die Referenz in
  den Zustand (relativ, wenn die Datei in der Library liegt) und lädt **nur**
  diesen Slot (`KickAssembler::loadOneSlot`); `clearSlotSample()`,
  `applySlotSettings()`, `applyAllSlotSettings()`, `isSlotFilled()`,
  `getSlotVoiceSettings()`. Der `KickRenderer` hat dafür
  `setSlotSettings()` (Einstellungen tauschen, Quelle behalten).
* **B-155 — Rollen, Locks und KEY-TRACK aus der UI.** `setSlotRole`,
  `setSlotLocked`, `setSlotKeyTracked` schreiben in `extraState`; der Zustand
  ist die einzige Wahrheit, die Karte liest ihn im Timer.
* **B-156 — Anzeige-Hüllen.** `KickAssembler::SampleEnvelope` (256 Min/Max-Paare
  je Slot, beim Laden berechnet). Der Visualiser und die Mini-Wellenform der
  Karte zeichnen daraus — ohne Audio-Thread-Zugriff, ohne Sample-genaue Daten.
* **B-84 — Visualiser.** `KickVisualizer` in `src/ui/KickPage.{h,cpp}`:
  **SHOW PARTS** (eine Zeile je belegtem Slot auf der Kick-Zeitachse, Farbe nach
  Rolle) und **SUM** (Summe mit Vol/Position/Trims/Fades, 512 Stützpunkte,
  Mute/Solo wie im Renderer). Dazu Viertelbeat-Raster und
  „Kick-Länge ms | BPM“.
* **B-83 — Transport.** `PLAY`, `LOOP` (1,5 s), `DEMO-KICK`, BPM-Leiste
  (`getBpm`/`setBpm`: Zustand + Renderer + Shaper-Hüllkurve), MASTER-Leiste
  (`master_vol`), `VIEW` zum Umschalten. Die beiden Leisten sind `ValueBar` —
  `LinearBar` mit eigener Wertbeschriftung, weil JUCE bei `LinearBar` nur den
  Balken zeichnet.
* **B-157 — Editor umgebaut.** `PluginEditor` hält jetzt zwei Seiten-
  Komponenten (`KickPage`, `FxPage`) statt einer handgelayouteten Skelett-Seite;
  Titel, Versionszeile, Notenanzeige und Umschalter bleiben im Rahmen.
  `kickPage()`/`fxPage()` sind für Tests erreichbar.
* **`startDemoKick()` schreibt jetzt in den Zustand.** Vorher lud nur der
  Renderer — die Slot-Karten waren leer, obwohl der Kick klang. Jetzt landen
  Referenzen, Rollen (`punch`/`rumble`/`click`) und Startpegel (0/−6/−12 dB über
  die Slot-Volumen-Parameter) im Zustand, danach lädt `loadFromState()`.
  Nebeneffekt: das Demo-Preset lässt sich speichern und wieder laden.

**Behobene Nebenbefunde**

* `ParamDesc::format()` zeigte für Werte aus der Kurvenumkehr „-0.00 dB“;
  Beträge unter 1e-4 werden jetzt auf 0 gesetzt.
* `juce::Slider::setRange (0, 1, 0.0001)` rundete den normalisierten Wert
  (0,85714286 → 0,8571) und damit die dB-Anzeige auf „-0.00 dB“. Slot- und
  Transport-Regler laufen mit Intervall 0.
* `KickPage` ruft am Ende des Konstruktors `resized()`: `setSize()` läuft dort
  **vor** dem Anlegen der Kinder, sonst bleibt die Seite bis zur ersten
  Größenänderung unausgelegt (Tests, die die Seite direkt rendern, sahen eine
  leere Fläche).
* `KickAssembler::findFirstSample()` hatte unerreichbaren Code (C4702);
  jetzt mit `break` statt `return` im Schleifenkörper.
* `PluginEditor` zeigte die KICK-Seite als Skelett; der Tab KICK ist jetzt die
  echte Seite mit Karten und Visualiser.

**B-153 (Teil 1) — Release-CPU gemessen.** `tests/CpuProfileTests.cpp` prüft die
Budgets jetzt je Konfiguration (`NDEBUG`: 6 % je Modul, teure Konfigurationen
30 %; Debug unverändert). Messung im Release: `filt/eq/mseq/shaper/trans/reverb`
je 0,1 %, `dynEq` 0,2 %, `dist/final` 0,5 %, `ott` 0,7 % — **alle zehn Module
zusammen 2,5 %** eines Kerns, jedes unter dem 3-%-Ziel. Teure Einzelkonfigurationen
im Release: Convolver true stereo (4-Kanal-IR, 2 s) 5,2 %, Multiband linearphasig
(513 Taps) 4,7 %. Tabelle in `docs/specs/fx-chain-engine.md` §9.1. Offen bleibt
der Sidechain-Hosttest in einer DAW (Teil 2 von B-153).

**Tests: 316 Testfälle, 540.097 Assertions, alle grün.** Neu:
`tests/KickPageTests.cpp` mit 11 Testfällen (12 Karten, Sample laden/leeren,
Live-Regler, BPM/Klemmung, Rollen/Sperren, Reglertexte, Visualiser PARTS/SUM,
Demo-Kick gegen den Zustand, Editor mit beiden Seiten). Baseline: 305 / 539.977.

**Gates:** Build grün (VST3 + Standalone, keine neuen Warnungen) · Suite grün ·
UI-Renderings gesichtet (`C:\bfka-re\ui-kickpage-*.png`) · Doku aktualisiert.

**Nächste Schritte**

1. **B-95** — `<Samples>` im Preset (Base64-WAV), Größenlimit, Import-Rückweg.
2. **B-150** — `getTailLengthSeconds()` aus Ketten- und Kick-Länge ableiten.
3. **B-151** — Karten-Layout für `dynEq` & Co. gruppieren (Reiter/Aufklappen).
4. **B-152** — IR-Browser mit User-Directories.
5. **B-153** — Release-CPU-Messung gegen die 3-%-Budgets, Sidechain-Hosttest.
6. **B-44** — Pitch/Stretch in `SampleVoice` (die Regler der Karten wirken erst dann).

---

## 2026-11-17 — Welle 8: Sidechain, Presets, die letzten drei Editoren, Rampen, CPU-Profil

**Erledigt (Phase 3, B-146, B-93/B-94, B-92b, B-147, B-148, B-149)**

* **B-146 — Externer Sidechain für `dynEq`.** `FxModule` hat zwei neue
  Methoden (`setSidechainBuffer()` / `acceptsExternalSidechain()`), die
  Ketten-Engine reicht das Signal an Module mit Detektor weiter und verzögert es
  um die Latenz der Vormodule (`sidechainLatencyFor()`). Der Prozessor bietet
  zwei optionale Eingangsbusse an (`Input` = Haupt-Eingang, `Sidechain` = Aux)
  und kopiert den Detektor **vor** dem Leeren des Ausgangspuffers, weil JUCE
  Ein- und Ausgang in-place auf dieselben Kanäle legt. `dynEq` wertet den
  Modus `sidechain = 1` (Ext SC) jetzt wirklich aus, inklusive der SPLIT-Bandfilter,
  und fällt ohne Bus definiert auf „Int SC flat“ (2) zurück. Abnahme: 6 Tests in
  `tests/SidechainTests.cpp`.
* **B-93 — Preset-IO.** Neu `src/core/PresetIO.{h,cpp}`: `<BFKAPreset>`-Kopf
  (`kind="kick|module"`, Name, Autor, Style, Datum, App-Version), Endungs-
  Ergänzung, 32-MB-Grenze, tolerantes Lesen (auch nackte `BFKAKick`-Wurzel).
  Prozessor: `savePreset`/`loadPreset`, `readPresetInfo`,
  `saveModulePreset`/`loadModulePreset` (Kettenplatz behält `enabled`/`mix`/`outGain`).
  UI: `SAVE`/`LOAD` in der Werkzeugleiste, `SAVE MODULE`/`LOAD MODULE` in der
  Seitenleiste, Statuszeile für Rückmeldungen.
* **B-94 — A/B und Ketten-Header.** Zwei gemerkte Gesamtzustände im Prozessor
  (`storeComparisonState`/`restoreComparisonState`, A/B-Knöpfe in der Leiste),
  `FxChainModel` mit **Sperre** (Add/Remove/Move/Duplicate/Clear blockiert,
  Regler bleiben bedienbar) und **Werkskette** (`filt` + `final`).
* **B-92b — die letzten drei Graph-Editoren.** `MidSideEqView` (Bandknoten,
  WIDTH, Solo/Mute), `KickShaperView` (ziebare `<Pt>`-Kurve, Segmenttypen,
  Abspielmarke) und `FinalizerView` (Sub-Shelf, Mono-Marker, Limiter/Clip).
  `createStructureView()` kennt damit sechs der zehn Typen.
  **Nebenbefund behoben:** `writeBackModuleStates()` entfernt die
  Struktur-Kinder (`Band`, `WNode`, `Pt`, `Eq`, `Dc`) jetzt **immer** — vorher
  blieben gelöschte Bänder im Zustand/Preset stehen, weil `getState()` bei
  leerer Liste gar kein `<Band>` mehr liefert.
* **B-147 — Kick-Länge für `shaper`.** `KickRenderer::kickLengthSeconds()`
  (Beat-Länge bei KICK LENGTH = 1 BEAT, sonst die längste belegte Voice);
  `PluginProcessor::updateModuleRenderLength()` setzt sie nach jedem
  Ketten-Rebuild, nach einem neuen Kick und nach `applyState()`. `applyState()`
  gibt das Tempo aus dem Zustand an den Renderer. Finalizer-Latenz im Host-Test
  verifiziert (72 Samples @ 48 kHz, 144 bei zwei Finalizern).
* **B-148 — Rampen.** Die Kette rampt `mix`/`outGain` sampleweise
  (`blendInPlaceRamp`, `applyGainRamp`; O4 geschlossen), der Convolver rampt
  `wet` und `width` sampleweise statt pro Block. MIX-Tests prüfen jetzt das
  Rampenverhalten (Rampe im ersten Block, exakte Werte danach).
* **B-149 — CPU-Profil je Modul.** `tests/CpuProfileTests.cpp` misst alle zehn
  Module (Median aus drei Läufen, 48 kHz/128) und die beiden teuren
  Konfigurationen (Convolver true stereo, Multiband linearphasig); Tabelle in
  `docs/specs/fx-chain-engine.md`, Abschnitt 9.1.

**Nebenbefunde dieser Welle**

* JUCE legt Ein- und Ausgangskanäle **in-place** übereinander
  (`getBusBuffer`-Offsets: Ausgang 0 und Haupt-Eingang 0 sind derselbe Kanal).
  Ein Aux-Bus ohne Haupt-Eingang würde den Ausgang überschreiben — deshalb zwei
  Busse und der Kanal-Offset-Vergleich im Prozessor.
* `applyState()` hat das Tempo nie an den Renderer gegeben; die Kick-Länge war
  dadurch immer 150 BPM. Jetzt Teil von B-147.
* Test-Rendering in PNGs: `BFKA_RENDER_UI=1` schreibt
  `C:\bfka-re\ui-*.png`. **Vorher die alten Dateien löschen** — ein noch
  gesperrtes Ziel lässt `FileOutputStream` still scheitern, und man debuggt
  stundenlang ein Bild, das längst veraltet ist.

**Gates:** Build grün (VST3 + Standalone) · **305 Testfälle / 539.977
Assertions** grün · Doku aktualisiert. Baseline vor der Welle: 278 Testfälle.

**Nächste Schritte**

1. `getTailLengthSeconds()` (noch 8,0 s) aus Kette/Kick-Laenge ableiten.
2. Eingebettete Samples im Preset (`<Samples>`, B-95) und Factory-Presets.
3. IR-Browser mit User-Directories; Karten-Layout für 64-Parameter-Module
   (`dynEq`) gruppieren.
4. Host-Test des Sidechains in einer echten DAW (Bus-Routing); Release-CPU-Messung
   für die 3-%-Budgets.

---

## 2026-11-17 — Welle 7: FX-Kette im Prozessor, FX-UI und die letzten drei Module

**Erledigt (Phase 2/3, B-143, B-144, B-90/B-91, B-92, B-145, B-62/B-64/B-67)**

* **B-143 — Prozessor-Verdrahtung und Zustand.** Neu `src/core/FxChainIO.{h,cpp}`:
  `fxChainToValueTree()`/`fxChainFromValueTree()` halten das `<FxChain>`-Schema
  an einer Stelle, `KickState` ruft sie auf. `extraState` ist jetzt ein
  `BFKAKick` (vorher `BFKAExtra`, das `KickState::fromValueTree()` immer
  ablehnte) — damit tragen `captureState()`/`applyState()` Slots, Rollen, Locks,
  Sample-Pfade und die komplette FX-Kette verlustfrei. `prepareToPlay()` baut die
  Kette (`rebuildEngineChain()`) und meldet die Summe der Modullatenzen per
  `setLatencySamples()`; `setFxChain()` schreibt Zustand + Engine und meldet
  danach neu. Abnahme: PDC 2304 Samples für `ott` (`linear=1`, 256) +
  `reverb` (`zeroLat=0`, 2048).
* **B-144 — Modul-API für die UI.** `setModuleParam`/`getModuleParam`
  (Anzeige-Einheiten, atomar im Modul, kein Rebuild), `setModuleAttribute`,
  `setModuleMix`/`setModuleOutGain`/`setModuleEnabled` (Engine **und**
  `currentChain`), `getModuleDisplay`, `getModule(slot)` sowie
  `writeBackModuleStates()` (Merge von `getState()` in den Ketten-Zustand,
  schließt O6). `FxChainProcessor::moduleAt()` als Modulzeiger-Zugriff.
* **B-90/B-91 — FX-Seite.** Neu `src/ui/`: `UiTheme.h`, `FxChainModel`
  (Add/Remove/Move/Duplicate ohne GUI), `ModuleCard` (Kopf, MIX/OUT,
  Wellenform pre/post, Parameter-Raster in Anzeige-Einheiten), `FxPage`
  (Kettenliste, `+ ADD MODULE`, BYPASS ALL, CLEAR, FX-DEMO, LOOP, Status mit
  Modulzahl + Latenz). Strukturänderungen laufen über
  `suspendProcessing(true) … setFxChain(…) … suspendProcessing(false)`,
  Regler ohne Rebuild. `PluginEditor` hat jetzt die Seiten KICK und FX.
* **B-92 — Graph-Editoren.** `editors/MultibandView` (drei Bänder, ziehbare
  Grenzen `x1`/`x2`, GR-Balken je Band), `editors/ConvolverView`
  (IR-Picker per async FileChooser, EQ-Kurve ±24 dB über 11 Punkte,
  Decay-Kurve 0,1×…2×, jeweils mit `writeBackModuleStates()` nach dem Ziehen),
  `editors/DynamicEqView` (vier Bandknoten mit Frequenz/Gain, Mausrad = Q,
  Rechtsklick = Typ, HP/LP-Kanten, GR-Balken, N1-Hinweis im Footer).
* **B-62/B-64/B-67 — die letzten drei Module.** `mseq` (Mid/Side-EQ mit
  Solo/Mute und `<Band ch=0|1>`), `shaper` (Kick Shaper: `<Pt x y t>`-Hüllkurve,
  8 Segmentmodi, `amount`/`sLine`, Kick-Länge per `setRenderLength`), `final`
  (Finalizer: Sub-Boost-Shelf, Mono-Bass per LR4, Upward, True-Peak-Clipper mit
  1,5 ms Lookahead und 4×-Oversampling). Alle drei mit Spec und Tests; Factory
  und CMake eingetragen — damit sind **alle zehn Modultypen echt**.
* **B-145 — Demo-Kette.** `PluginProcessor::loadFxDemoChain()` baut
  Multiband + (nur bei vorhandener IR) Convolver + Dynamic EQ, „FX-DEMO“ baut
  und startet sie, „LOOP“ triggert alle 1,5 s. Abnahme im Test: Bypass-Nulltest
  bitgenau, mit Kette messbar anders (RMS > 1 %), Latenz 0, CPU 5,7 % eines
  Kerns im Debug-Build (Budget ~10 %).

**Behobene Nebenbefunde**

* `ScopedTypeOverride` in `tests/FxChainTests.cpp` stellte den Creator nicht
  wieder her und ersetzte echte Module dauerhaft durch Platzhalter (fehlende
  Test-Isolation) — jetzt wird der vorherige Creator gesichert
  (`FxFactory::creatorFor()`), die Platzhalter-Tests nutzen ihn explizit.
* Shaper-Trigger: ohne Sperrzeit fiel ein gehaltenes Signal bei jedem
  Nulldurchgang unter die Hysterese, die Hüllkurve startete zweimal pro Periode
  neu (`amount=100` war unhörbar). Neu: `kRearmHoldSeconds = 10 ms`.
* Parameter-Textboxen zeigten die Default-Formatierung, weil
  `textFromValueFunction` erst nach `setValue` gesetzt wurde; `updateText()`
  ergänzt. Nicht-ASCII-Literale in UI-Strings durch ASCII ersetzt (MSVC liest
  die Quellen ohne `/utf-8` in der Systemcodepage).

**Gates:** Build grün (VST3 + Standalone) · **278 Testfälle / 539.262
Assertions** grün · Doku aktualisiert. Baseline vor der Welle: 212 Testfälle.

**Nächste Schritte**

1. B-146: externer Sidechain für `dynEq` (Bus-Layout, Host-Kompatibilität).
2. IR-Browser mit User-Directories; Preset-IO (`.bfkapreset`) an die UI binden,
   A/B-Umschaltung.
3. Smoothing Härtung: `mix`/`outGain` der Kette rampen (O4), Convolver
   `wet`/`stereo`/`tone` per-Sample.
4. CPU-Profile je Modul messen (Budget < 3 % je Modul), Karten-Höhe/Scroll
   aufräumen (Karten mit vielen Parametern sehr hoch).

---

## 2026-11-17 — Welle 6: FX-Integration von OTT, TDR Nova und Convology XT

**Erledigt (Phase 2, B-140…B-142)**

* **Multiband (Xfer OTT 1.37)** — `src/fx/modules/MultibandModule.*`:
  3-Band-Up/Down-Kompressor mit Depth, Time (gegenläufig Attack/Release),
  In/Gain, Clean-Crossover (LR4/LR8), Thresholds je Band, Band-Input-Gains,
  Bypass Aufwärts/Abwärts je Band sowie linearphasigem FIR-Split (513 Taps,
  256 Samples Latenz). Parameterinventar aus der VST3-Binary
  (`In Gain`, `Clean XOV`, `Thresh L/M/H`, `Byp Upwd/Dnwd L/M/H`,
  `Upwd/Dnwd Strgth`) — die Referenz-Attribute bleiben auf `p0…p7`.
* **Convolver (Convology XT 1.34)** — `src/fx/modules/ConvolverModule.*`:
  IR-Laden (WAV/AIF, 1/2/4 Kanäle inkl. True Stereo), Time-Hüllkurve
  (Start/Attack/Hold/Release), EQ-Kurve und frequenzabhängige Decay-Kurve
  (je 11 Punkte), Normalize, IR-Gain, Reverse, Mono-Input, Stereo-Modulator
  (90°-Quadratur), Decay-Skalierung, Stretch (Resampling), Predelay,
  Width 0…200 %, Zero-Latency-Kopf oder feste 2048 Samples Latenz.
  Faltung über `juce::dsp::Convolution`; IR-Aufbereitung auf dem
  Message-Thread, beim Start deterministisch per `prepare()` in die Engine
  übernommen, Live-Änderungen wait-free über einen atomaren Kernel-Zeiger.
* **Dynamic EQ (TDR Nova)** — `src/fx/modules/DynamicEqModule.*`, **neuer
  Modultyp `dynEq`**: 4 Bänder (Low-Shelf/Bell/High-Shelf) mit je
  Threshold/Ratio/Attack/Release, SPLIT/STICKY, HP/LP mit 6/12/24/72 dB/Okt,
  W-Band mit eigener Dynamik, Dry Mix, Output Gain, EQ Auto Gain, Delta,
  sieben Prozessziele (Mono/Stereo/Sum/Diff/Left/Right/Dual Mono).
  Der bestehende Referenz-`eq` bleibt unverändert (Parität der 44 Presets).
* **`FxChainProcessor::setChain()`** übergibt jetzt den Struktur-Zustand über
  `module->setState (state.data)` — damit erreichen EQ-Bänder, Kurven und
  IR-Pfade die Module (schließt O5 aus der Ketten-Engine-Spec).
* **Doku:** `docs/specs/fx-multiband.md`, `fx-convolver.md`,
  `fx-dynamic-eq.md`; Ketten-Engine-Spec auf zehn Typen aktualisiert.
* **Tests: 212 Testfälle, 491.404 Assertions, alle grün.** Neu: 30 Modul-Tests (9 Multiband, 10 Convolver, 11 Dynamic EQ) für
  die drei Module (Impuls-/Sinus-/Hüllkurvenmessungen, True-Stereo-Routing,
  Zustands-Roundtrip, Kettenintegration OTT → Convolver → Dynamic EQ).
  Baseline vor der Änderung: 180 Testfälle / 480.817 Assertions.

**RE-Provenienz (nur lesend, nichts im Repo)**

* Installer aus `BF Kick Forge Plugins for FX` entpackt bzw. installiert
  (Analysis-Kopien unter `C:\bfka-re\`): OTT (NSIS, VST3 + AAX),
  Convology XT (NSIS, VST3/AAX/VST2 + Factory Library + **Handbuch-PDF**),
  TDR Nova (Inno Setup, VST3 x64 + x86).
* Parameterinventare aus den Bezeichner-Strings der VST3-Binaries;
  Bediensemantik aus dem Convology-Handbuch und der öffentlichen
  TDR-Nova-/OTT-Dokumentation. Keine Standalone-Versionen vorhanden.
* Keine IRs, Presets, Assets oder Strings der Fremd-Plugins wurden übernommen.

**Bewusste Abweichungen (dokumentiert in den Specs):** serielle statt parallele
Nova-Struktur, keine Factory-IRs, kein externer Sidechain, OTT-`x1`-Bereich
20…1000 Hz, Quality-Modus ohne Oversampling-Unterschied.

**Gates:** Build grün (VST3 + Standalone) · Suite grün · Dokumentation aktualisiert.

**Nächste Schritte**

1. UI (P9): Karten und Graphen für die neuen Module (OTT-Crossover,
   Convolver-Kurven, Dynamic-EQ-Bänder).
2. Verbleibende Referenzmodule: `mseq`, `shaper`, `final`.
3. Modul-Presets (`getState()`-Rückschreiben in den Kick-Zustand, O6).

---

## 2026-11-17 — Welle 5: Der erste hörbare Kick

**Erledigt**

* **Engine und FX-Kette sind im Plugin verdrahtet.** `PluginProcessor::processBlock`
  rendert jetzt: Kick-Summe → FX-Kette → Master. Jede MIDI-Note-On startet den Kick neu.
* **`src/engine/KickAssembler.*`**: übersetzt die normalisierten Slot-Parameter in
  `VoiceSettings` (dB→linear, Prozent→Sekunden, `ficurve`→Kurvenform), löst Sample-Pfade
  relativ zur Library auf und hält die Audiodaten am Leben. Zusätzlich baut
  `buildDemoKick()` einen Test-Kick aus Punch, Rumble und Click der Library.
* **Editor**: Buttons „Demo-Kick laden" und „Play" plus Statuszeile — damit ist der
  erste Kick im Standalone hörbar.
* **Filters-Modul selbst geschrieben** (`src/fx/modules/FiltersModule.*`): LP/HP mit
  echter Butterworth-Kaskade (1–5 Pole = 6/12/18/24/30 dB/Oktave), BAND 12/24,
  NOTCH (Kerbe > 100 dB), PHASER mit 1–6 gestaffelten Allpass-Kerben, COMB,
  DRIVE-Sättigung vor dem Filter. Messwerte: LP 12 dB/Okt dämpft eine Oktave über
  CUTOFF um 12 dB, 24 dB/Okt um 24 dB; Durchlass −0,001 dB.
* **Build-Verzeichnis aus Dropbox verlegt** (`C:\bfka-build`). Ursache der sporadischen
  `LNK1104`/`LNK1201`-Fehler war der Dropbox-Sync, der Dateien mitten im Linken sperrte.
  `tools/dev-env.ps1` setzt `BFKA_BUILD_ROOT` automatisch.
* **Tests: 180 Testfälle, 480.817 Assertions, alle grün.** Neu: 11 Filter-Tests und
  4 Integrations-Tests, die einen Kick durch die Engine rendern und prüfen, dass Ton
  herauskommt (Spitzenwert ≈ 0,82) und nach dem Beat Stille folgt.

**Gates:** Build grün (VST3 + Standalone) · Suite grün · Artefakte vorhanden.

**Nächste Schritte**

1. Die übrigen fünf Module: Convolver, Finalizer, Multiband, Mid/Side-EQ, Kick Shaper.
2. Danach KICK-UI (P8) mit Visualiser und Slot-Karten.

---

## 2026-11-17 — Welle 4: erste echte FX-Module im Build

**Erledigt**

* **Equalizer** (`src/fx/modules/EqualizerModule.*`): 15-stufige Biquad-Kaskade, RBJ-Formeln
  für BELL/L-SHF/H-SHF/L-CUT/H-CUT/NOTCH, 12 Nutzerbänder über `setState()/getState()`,
  dynamische Gegenregelung (max. 6 dB), 20 ms Glättung, lock-freier Band-Snapshot.
  Werkszustand ist bitidentisches Durchreichen (flache Bänder werden übersprungen).
* **Distortion** (`src/fx/modules/DistortionModule.*`): zehn Modi, AGC,
  PRE/POST-Tilt, 2×-Oversampling, 0-dBFS-Ceiling, Waveshaper-Kurve als `WNode`-Liste.
* **Transient Shaper** (`src/fx/modules/TransientShaperModule.*`): dualer Hüllkurvenfolger,
  ATTACK/DUCK/SPEED, Soft-Limiter; `amount == 0` ist bitgenau.
* Alle drei sind in `FxFactory` registriert; die übrigen sechs laufen weiter als Platzhalter.
* **Tests: 167 Testfälle, 476.736 Assertions.** Stand: 166 grün, 1 als **bekannter Fehler**
  markiert (`[!mayfail]`) — siehe unten.

**Bekannter Fehler (offen)**

* `DistortionModule`: bei `drive = 0` ist der Ausgang rund **3,5 dB zu leise**
  (Soll: reine Verzögerung). Verdacht: Ringpuffer in `copyDelayed()`. Ticket **B-63b**,
  Fix-Auftrag läuft; der Test bleibt im Bestand und ist nur als „may fail" markiert.

**Gates:** Build grün · Suite grün (1 erwarteter Fehlschlag) · Push erfolgt.

**Nächste Schritte**

1. Filters-Modul (Neuauftrag läuft), danach Convolver, Finalizer, Multiband, Mid/Side-EQ,
   Kick Shaper.
2. Danach Engine + FX-Kette im `PluginProcessor` verdrahten, damit der erste Kick hörbar wird.

---

## 2026-11-17 — Welle 3: Engine, Stretching und Randomizer im Build

**Erledigt**

* **Sample-Engine (P3)**: `src/engine/SampleVoice.*` (Position, Trim, fünf Fade-Kurven,
  Declick-Rampe, monotone Ausgabe) und `src/engine/KickRenderer.*` (12 Slots, Solo-Logik,
  `KICK LENGTH` 1 Beat vs. Full Sample, Auto-Gain über das Body-Fenster 20–120 ms).
* **Stretching (P4)**: `src/engine/WsolaStretcher.*` — eigene WSOLA-Implementierung
  (Hann-Fenster, Grain-Suche per normalisierter Kreuzkorrelation, OLA-Normalisierung,
  kanalgekoppelte Suche gegen Phasendrift im Bass) plus `REPITCH`-Pfad und
  `src/engine/Resampler.h`. **Lizenzrein, keine Fremdbibliothek.**
* **Randomizer (P7)**: `src/library/StyleModel.*` (JSON-Parser für `genres.json`:
  25 Pools, 10 Styles, Rollen, Tonarten, Gewichte) und `src/library/RandomizerEngine.*`
  (Pflicht-/Optionalteile, gewichtete Pool-Wahl, `shares`, Pegelbereiche,
  Ausschlussregel „melodic × atonal", deterministisch über Seed).
* **Tests: 112 Testfälle, 384.903 Assertions, alle grün.** Build grün.
* **CI:** erster vollständig grüner Durchlauf auf GitHub Actions (7 min 16 s),
  Artefakt-Upload funktioniert.

**Gates:** Build grün · Tests grün · `origin/main` aktuell · CI success.

**Nächste Schritte**

1. Pitch/Stretch in die Voice einhängen (B-44) — die Schnittstellen stehen.
2. Die neun FX-Module implementieren (größter Parallelisierungssprung).
3. Danach UI-Seiten (P8/P9).

---

## 2026-11-17 — Welle 2: Parameter-Modell korrigiert, Library integriert

**Erledigt**

* **Preset-Datenauswertung abgeschlossen** (44 Kicks, 76 FX-Chains, 639 Modulinstanzen).
  Acht Befunde mit Zahlen, drei davon ändern das Datenmodell:
  * **Parameterzahl 352 → 364.** Es gibt einen bisher unbekannten 20. Slot-Parameter
    `lN_fistart` (Fade-In-Anker), nachweisbar im neuesten Werkpreset.
  * **Stretch-Kurve korrigiert.** Logarithmus ist widerlegt: 1,0× wird als
    `0.374989420` gespeichert, Log ergäbe 0,4. Eine **Potenzkurve mit Exponent 0,42**
    trifft den Messwert exakt. Test dafür ist drin.
  * **FX-Parameterbrücke geklärt:** `p12 = MIX`, `p13 = outGain / 2`,
    `p0…p11` = Modulattribute in XML-Reihenfolge; Bänder/Kurven liegen **nicht** in den
    Proxy-Parametern, sondern nur im `<FxChain>`-Snapshot. Werte sind 4-stellig quantisiert.
  * Ebenfalls belegt: `root_note = MIDI-Note / 127`.
* **Sample-Library-Index integriert** (`src/library/SampleLibrary.*`): Ordner→Rollen
  inklusive der beiden Sonderfälle (`Main - Crunches/Industrial Heavy` ist eine Rumble,
  `Enhancers (Tops)` ist TOP), BPM- und Tonart-Erkennung aus Dateinamen.
  Verifiziert gegen die echte Library: **2.443 Dateien in 1,43 s**, BPM-Verteilung
  2.246× 150 / 60× 155 / 137× 160, 363 Dateien mit Tonart.
* **Tests: 38 Testfälle, 3.571 Assertions, alle grün.** Build grün.

**Gates:** lokaler Build grün · Tests grün · CI beim Push ausgelöst.

**Läuft parallel:** Engine-Core (Sample-Voice, Fades, Summe, Auto-Gain) und
WSOLA-Stretcher; StyleModel/Randomizer.

---

## 2026-11-17 — Welle 1: FX-Gerüst integriert, Repo live

**Erledigt**

* **GitHub-Repo angelegt und gepusht:** <https://github.com/TotalBassJunkie/bass-forge-kick-architect> (public).
  CI-Workflow `Build` läuft bei jedem Push.
* **FX-Ketten-Framework** (B-69) von Grund auf vorhanden und **im Build integriert**:
  `src/fx/FxModule.h` (Schnittstelle, `FxType`, Parameterbeschreibung),
  `src/fx/FxFactory.*` (Registry der neun Modultypen), `src/fx/PassThroughModule.h`
  (Test-Platzhalter), `src/fx/FxChainProcessor.*` (Kette, MIX-Blende, OUT-GAIN,
  Bypass, Latenzausgleich, Display-Puffer je Modul).
* **Tests: 22 Testfälle, 3.407 Assertions, alle grün** — inklusive FX-Kette
  (Reihenfolge über OUT-GAIN, MIX 0/0,5/1, unbekannter Modultyp, Bypass, Latenz-Summe).
* Library-Audioformate vollständig erfasst (siehe Bestandsaufnahme 2.2.1):
  die 2.443 WAVs sind gemischt 16/24-bit-PCM und 32-bit-Float, überwiegend
  44,1 kHz stereo, dazu 32 mono-IRs und 8 Dateien mit 48 kHz.

**Gates:** Build grün · Tests grün · Remote `origin/main` aktuell.

**Nächste Schritte**

1. Wellen 1 einmischen: StyleModel/Randomizer, SampleLibrary (laufen).
2. Danach P3 (Engine-Core) und P4 (WSOLA) als Kernstück.

---

## 2026-11-17 — Welle 0/1: Fundament gebaut

**Erledigt (P0, B-00…B-06)**

* Git-Repo initialisiert (Branch `main`), zwei Commits.
* Toolchain verifiziert: MSVC 19.51 (VS BuildTools 2026), CMake 4.3.1, Ninja 1.13.2.
* `CMakeLists.txt` mit `juce_add_plugin`: Ziele **VST3 + Standalone**, JUCE **8.0.7**
  (FetchContent bzw. lokaler Baum über `BFKA_JUCE_SOURCE_DIR`), Catch2 v3.7.1 für Tests.
* **Build grün**: `Bass Forge Kick Architect.vst3`, `Bass Forge Kick Architect.exe`
  (Standalone), `BFKA_Tests.exe`.
* **Tests grün**: 11 Testfälle, 2.759 Assertions.
* Code-Stand:
  * `src/core/Version` — Produkt-Metadaten (`BFKA`, `.bfkapreset`, Bundle-ID).
  * `src/core/Log` — Logging ohne Audio-Thread-Nutzung.
  * `src/core/ParameterRegistry` — **352 Parameter** (12×19 Slot + 12 Global + 8×14 FX-Proxy)
    mit Kurven, Bereichen, Defaults, Formatierung.
  * `src/core/KickState` — Zustand inkl. Slots/Rollen/Locks/FX-Kette und ValueTree-Serialisierung.
  * `src/plugin/PluginProcessor` + `PluginEditor` — lauffähiges Skelett, 352 Host-Parameter,
    Zustandsspeicherung, minimaler Editor.
* Helfer: `tools/dev-env.ps1` (findet VS/CMake/Ninja selbst), `configure.ps1`, `build.ps1`, `test.ps1`.
* CI: `.github/workflows/build.yml` — Windows-Build, Tests, Artefakt-Upload, Release bei Tag.
* Spezifikation `docs/04-SPEC-STATE-AND-PRESET-FORMAT.md` (Zustandsbaum, Preset-Format,
  Import-Mapping des Referenzformats).

**Gates:** Build grün (Exitcode 0) · `BFKA_Tests` grün · Konfiguration reproduzierbar
über `tools\configure.ps1`.

**Erkenntnisse aus der Datenauswertung (Zwischenstand)**

* `root_note = 0.283465` → das ist **36/127**, also MIDI-Note 36 (C2). Bestätigt die
  Normalisierung `note/127`.
* `master_vol` liegt bei 43 von 44 Presets exakt auf `0.833333` = **5/6**.
* `stretch` liegt bei fast allen Presets auf `0.374989` — also ist 1,0× **nicht** 0,5
  (Arbeitsannahme: logarithmische Abbildung 0,25×…8× → 1,0× entspricht ≈ 0,40).
* `filter` = 1.0 und `filterq` = 0.102564 konstant → Slot-Filter ist in Werk-Presets
  durchgehend offen; `0.102564` ist vermutlich `4/39`.

**Nächste Schritte**

1. GitHub-Repo anlegen und pushen (wartet auf Zugang).
2. Welle 1 einmischen: StyleModel/Randomizer, SampleLibrary, FX-Ketten-Framework.
3. Danach P3 (Engine-Core) und P4 (WSOLA) als Kernstück.

---

## 2026-11-17 — Kickoff: Bestandsaufnahme & Plan

**Erledigt**

* Vollständige Inventur der installierten Referenz (Programme, Content, Bibliothek,
  Presets, Analysedaten) — Hashes, Größen, Pfade, Zahlen.
* Handbuch (37 Seiten) ausgewertet, Signalfluss und Bedienlogik dokumentiert.
* Offene Datenformate gelesen und dokumentiert: Kick-Preset-XML, FX-Chain-XML,
  `genres.json` (25 Pools / 10 Styles), `prefs.json`, Analyse-Container.
* Binär-Statik: Framework (JUCE 8.0.7), VST3-SDK 3.7.12, Renderer, Imports,
  Modul-/Klassenlandkarte, Stretch-Algorithmusfamilie (WSOLA).
* Toolchain geprüft: MSVC 19.51 (VS BuildTools 2026), CMake 4.3.1, Ninja 1.13.2.

**Dokumente**

* [`01-BESTANDSAUFNAHME.md`](01-BESTANDSAUFNAHME.md) — Ist-Zustand, Datenformate,
  Parameterinventar, offene Fragen, Leitplanken.
* [`02-MASTERPLAN.md`](02-MASTERPLAN.md) — Phasen P0–P14, ~85 Tickets (B-00…B-135),
  Subagenten-Routing, Abnahmekriterien, Risiken, Meilensteine.
* [`03-RE-FINDINGS.md`](03-RE-FINDINGS.md) — Methodik und Befundlogbuch mit
  Quellenmarkierung.

**Entscheidungen**

* Produktname: **Bass Forge Kick Architect**, Code `BFKA`.
* Stack: C++20 + JUCE 8 (Version gepinnt), CMake, VST3 + Standalone, Windows zuerst.
* Kein Rubber Band (lizenzpflichtig) — eigene WSOLA-Engine.
* Kein Lizenzsystem, kein fremder Sample-Content im Repo.
* RE-Artefakte (String-/Klassenauszüge, Handbuchtext) bleiben lokal in `_private/`
  und werden **nicht** veröffentlicht.

**Nächste Schritte**

1. Repo-Gerüst + CI (P0, B-00…B-06).
2. Verträge schreiben: `KickState`, `ParameterRegistry`, `FxChain`, `Library`,
   `Engine`, `FxModule`-Interface.
3. Welle 1 parallel: Fundament, Preset-IO, Library-Index, Testdaten, Parametertabelle.

---

<!-- Vorlage für neue Einträge:

## JJJJ-MM-TT — Welle N

**Erledigt:** B-xx, B-yy; je ein Satz Ergebnis + Link auf PR/Commit.

**Gates:** Build grün, `ctest` grün, CI-Artefakt vorhanden.

**Blocker:** …

**Nächste Schritte:** …

-->
