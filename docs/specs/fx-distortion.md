# Distortion-Modul (`dist`) — Spezifikation und Implementierung

**Ticket:** B-63 (Distortion) · **Typ:** `FxType::distortion` · **XML-Kennung:** `type="dist"`
**Code:** `src/fx/modules/DistortionModule.h`, `src/fx/modules/DistortionModule.cpp`
**Tests:** `tests/DistortionModuleTests.cpp` (29 Testfälle, 14 494 Assertions)
**Stand:** 2026-11-17 — mit MSVC 19.51 (`/std:c++20 /W4`) warnungsfrei übersetzt,
Testsuite grün (Ausführung gegen die echten JUCE-8.0.7-Objekte des Projekts).

---

## 1. Geltungsbereich

Das Modul ist eine Baugruppe des FX-Racks nach `docs/specs/fx-chain-engine.md`:

* Es bearbeitet die Summe **in-place** und liefert sein zu **100 % nasses** Signal.
  **MIX und OUT GAIN macht die Kette**, nicht das Modul.
* Parameter sind **Anzeige-Einheiten** (`drive` in Prozent, `mode` als Index, …).
* `prepare()`/`reset()`/`setParam()`/`setState()` laufen auf dem Message-Thread,
  `process()` auf dem Audio-Thread — ohne Allokation, ohne Logging, ohne Sperre.

---

## 2. Signalkette des Moduls

```
Eingang (Summe)
  │
  ├─ EQ-Chip PRE   (nur eqmode == 1: Tilt, linear, 0 dB Latenz)
  │
  ├─ 2×-Oversampling (immer aktiv, Latenz bleibt dadurch konstant)
  │    ├─ shape(x)                     reine Transferfunktion (Modus)
  │    ├─ DRIVE-Blende                 y = x + d·(shape(x) − x),  d = drive/100
  │    ├─ AGC                          Hüllkurven-Folger, max. ±12 dB
  │    └─ Ceiling                      hart auf [−1, +1]
  │
  ├─ 2×-Downsampling
  ├─ EQ-Chip POST  (nur eqmode == 2)
  └─ Ceiling       hart auf [−1, +1]  (0 dBFS, letzte Instanz)
Ausgang
```

**Sonderfall `drive == 0`:** Die Kurve wird gar nicht angewendet, deshalb läuft der
Eingang dann nicht durch das Oversampling, sondern über eine reine
Verzögerungsleitung um `latencySamples()`. Dieser Pfad ist **samplegenau**
(bitidentisch verzögert), also wirklich „clean“ — der Nulltest der Kette bleibt
erfüllbar. Die Leitung wird auch im Nasspfad ständig mitgeschrieben, damit ein
Umschalten 0 %↔>0 % keinen Sprung erzeugt.

---

## 3. Parameter

`params()` liefert genau diese sechs Einträge in dieser Reihenfolge (Index =
Nummer für `setParam()`/`getParam()`):

| # | `id` | Name | min | max | Default | Einheit | Bedeutung |
|---|---|---|---|---|---|---|---|
| 0 | `drive` | Drive | 0 | 100 | 25 | % | Stärke, mit der die Kurve angewendet wird (0 % = clean, 100 % = exakt die Kurve) |
| 1 | `mode` | Mode | 0 | 9 | 1 | – | Modus-Index, siehe Abschnitt 4 |
| 2 | `folds` | Folds | 2 | 8 | 2 | – | Anzahl der Faltungen (nur `mode = 3`) |
| 3 | `agc` | AGC | 0 | 1 | 1 | – | 0 = keine Pegelregelung, 1 = Regelung an |
| 4 | `eqmode` | EQ | 0 | 2 | 0 | – | 0 = OFF, 1 = PRE, 2 = POST (Abschnitt 8) |
| 5 | `flip` | Flip | 0 | 1 | 0 | – | wählt die Halbwelle (nur `mode = 6`) |

Werte werden doppelt geklemmt (Kette **und** Modul), `setParam()` mit ungültigem
Index ist ein No-op, `getParam()` liefert dann `0.0f`.

> **Abweichung zum Referenzbestand:** Die Referenzdaten enthalten `mode`-Werte
> 1…12 (`docs/specs/mining-report.md`), das Handbuch dokumentiert zehn Modi. Dieses
> Ticket legt den Bereich verbindlich auf **0…9** fest; 10…12 bleiben offen (O2).

---

## 4. Die zehn Modi — reine Transferfunktionen

Alle Modi sind **gedächtnislos** und als `float shape(float x)` öffentlich und
direkt testbar. `shape()` enthält **kein** DRIVE, **keine** AGC, **keinen** EQ und
**kein** Oversampling: es ist die Kurve selbst (also der Zustand bei `drive = 100`).
Jede Formel ist so gebaut, dass `|shape(x)| ≤ 1` und `shape(0) = 0` gilt.

| # | Name | Formel (x = Eingang) | Eigenschaften |
|---|---|---|---|
| 0 | **HARD CLIP** | `clamp(x, −1, 1)` | aggressive Kante; exakt ±1 ab |x| ≥ 1; monoton |
| 1 | **SOFT CLIP** | `tanh(1,5 · x)` | weiche Sättigung, streng monoton, \|y\| < 1 (rundes Ceiling) |
| 2 | **SATURATE** | `t = tanh(3·x)`, `y = t · (1 − 0,25·t²)` | kubischer Term hebt die 3. Harmonische („dick“), \|y\| ≤ 0,75, monoton |
| 3 | **WAVEFOLD** | `sin(π/2 · folds · x)` | Wellenfalter, `folds` = Faltungen; \|y\| ≤ 1 für beliebig große x |
| 4 | **FUZZ** | `\|x\| < 0,02 → 0`, sonst `clamp(5·x, ±1)`, negativ zusätzlich `× 0,85` | Gate erzeugt die zerrissene Textur, Asymmetrie = gerade Harmonische |
| 5 | **RECTIFY** | `\|tanh(1,5 · x)\|` | Vollwellen-Gleichrichtung, verdoppelt die Grundfrequenz, Ausgang einseitig (DC) |
| 6 | **HALF WAVE** | `t = tanh(1,5·x)`; `flip = 0 → max(0, t)`, `flip = 1 → min(0, t)` | formt eine Halbwelle, `flip` wählt welche |
| 7 | **BITCRUSH** | `clamp(round(8·x) / 8, ±1)` | Amplituden-Quantisierung in 12,5 %-Stufen (8 Stufen/Einheit) |
| 8 | **WAVESHAPER** | Tabelle der freien Kurve (Abschnitt 9) | eine Linie für beide Hälften |
| 9 | **WAVESHAPER ASYM** | Tabelle, `x < 0` → negative Hälfte, `x ≥ 0` → positive Hälfte | Mittelpunkt (x = 0) trennt die Hälften, je eigene Kurve |

**DRIVE-Blende** (in `process()`, nicht in `shape()`):

```
d = drive / 100
y = x + d · (shape(x) − x)
```

Damit gilt: `drive = 0` → `y = x` (clean, siehe Abschnitt 2),
`drive = 100` → `y = shape(x)`.

**Ceiling** wird anschließend und noch einmal ganz am Ende angewendet:
`y = clamp(y, −1, +1)` — hart, ohne True-Peak, ohne Lookahead. Bei sehr lauten
Eingängen gewinnt das Ceiling also immer (auch gegen eine AGC-Anhebung).

---

## 5. AGC (Pegelmanagement)

Ziel: der **Ausgangs-RMS folgt dem Eingangs-RMS**, DRIVE ändert den Charakter, nicht
die Lautheit.

* Zwei Hüllkurven-Folger auf der **Leistung** (`|v|²`), einer für den Eingang `x`,
  einer für den Kurvenausgang `y`; Angriff/Release getrennt:
  `attack ≈ 5 ms`, `release ≈ 120 ms`, gerechnet auf der **überabgetasteten** Rate.
* Stellgröße: `target = sqrt(envIn / envOut)` — also das RMS-Verhältnis.
* **Begrenzung auf ±12 dB:** `target = clamp(target, 10^(−12/20), 10^(+12/20))`
  = `[0,2512 … 3,9811]`.
* Die Korrektur wird mit einer Zeitkonstante von **2 ms** geglättet (kein Zipper),
  Startwert 1,0.
* Stille-Erkennung: liegt `envOut` unter 1e−12, bleibt `target = 1`
  (Stille bleibt Stille, keine Division durch 0).
* `agc == 0`: keine Regelung, gain wird auf 1,0 gehalten (kein Nachklingen).
* Die AGC läuft **pro Kanal** (kein Übersprechen).

Gemessen (Testfall „AGC hält die Lautheit“): Soft Clip, 1 kHz, Amplitude 0,3 →
mit AGC 0,0 dB Abweichung zum Eingangs-RMS (Toleranz im Test 1,5 dB), ohne AGC
+2,9 dB. Gegenprobe auf die Begrenzung: eine Kurve, die um 34 dB absenkt, wird nur
mit 12 dB gegengeregelt (Testfall „AGC korrigiert höchstens ±12 dB“).

---

## 6. Oversampling und Latenz

* `juce::dsp::Oversampling<float>`, **Faktor 2** (`factorLog2 = 1`),
  `filterHalfBandPolyphaseIIR`, `maxQuality = true`,
  `useIntegerLatency = true` — die Latenz ist damit ganzzahlig.
* Immer aktiv (auch bei linearen Einstellungen), damit sich die Latenz nicht mit
  dem DRIVE-Regler ändert: der Trockenpfad der Kette wird **nach `prepare()** genau
  einmal kompensiert.
* `latencySamples()` = `lround(getLatencyInSamples())`.
  **Gemessen mit JUCE 8.0.7: 4 Samples** (unabhängig von der Sample-Rate, weil der
  Filter in Basis-Samples rechnet; bei 48 kHz = 0,083 ms). Zum Vergleich: dieselbe
  Konfiguration mit FIR-Equiripple hätte 49 Samples — daher IIR.
* Die Latenz wird nur von `prepare()` gesetzt und ist danach konstant.
* Der Trockenpfad der Kette wird entsprechend um 4 Samples verzögert; der
  DRIVE-0-Pfad verweigert exakt dieselben 4 Samples (Abschnitt 2), sonst entstünde
  bei Teilmix ein Kammfilter.

---

## 7. Grenzfälle

| Fall | Verhalten | Nachweis |
|---|---|---|
| Stille | bleibt exakt 0 (kein Rauschen, kein DC) | Testfall „Stille bleibt Stille“ |
| DC | bleibt DC (0,4 rein → 0,4 raus, kein Ableiten) | derselbe Testfall |
| `drive = 0` bei `mode = 0` | bitidentisch zum Eingang, nur um die Latenz verzögert (Toleranz 1e-6) | Testfall „DRIVE 0 … bitidentisch“ |
| Eingang ±8 | Ausgang nirgends über 1,0 (+ 1e-6), in allen zehn Modi und mit/ohne AGC | Testfall „Ceiling hält bei ±8“ |
| Mono-Buffer | funktioniert (Modul rechnet 1…2 Kanäle) | Testfall „Mono-Buffer“ |
| Buffer > 2 Kanäle | nur die ersten zwei Kanäle werden bearbeitet (das Rack ist stereo, `FxChainProcessor::kMaxChannels = 2`) | dokumentiert |
| Block größer als `maxBlockSize` | wird intern in `maxBlockSize`-Häppchen zerlegt | Code |

---

## 8. EQ-Chip (OFF / PRE / POST)

Der Chip ist ein **Tilt** mit fester Ecke — ein einfacher Klangregler, wie im Ticket
gefordert (kein eigenes `amount`-Attribut im Referenzformat):

```
lp  += a · (x − lp)          a = 1 − exp(−2π · 700 Hz / fs)      (1-Pol-Tiefpass)
y   = 0,7079 · lp  +  1,4125 · (x − lp)      (−3 dB unter 700 Hz, +3 dB darüber)
Zustand je Kanal und Modus (PRE/POST teilen den Zustand nicht — siehe offener Punkt O6)
```

* `eqmode = 0` → Chip aus, keine Filterung.
* `eqmode = 1` (PRE) → Filter **vor** dem Oversampling: formt, was in die
  Verzerrung geht (mehr Höhen → aggressivere Harmonische).
* `eqmode = 2` (POST) → Filter **nach** dem Downsampling: formt, was herauskommt
  (heller, Sub leiser).

Der 1-Pol-Zweig ist kein echtes Shelf-Paar: die Summe ergibt bei der Ecke eine
leichte Mittenanhebung (~+0,5 dB). Das ist bewusst einfach gehalten und dokumentiert;
gemessen werden bei 60 Hz ca. −3 dB und bei 3 kHz ca. +3 dB.

---

## 9. Waveshaper-Kurve (`<WNode>`)

### 9.1 Format

Die Kurve steht als Knotenliste im `<Module>`-ValueTree — **exakt** das
Referenzformat:

```xml
<Module type="dist" on="1" mix="1.0" outGain="1.0" drive="45" mode="8" …>
  <WNode x="-1" y="-1" ten="0.5" seg="0"/>
  <WNode x="0"  y="0"  ten="0.5" seg="0"/>
  <WNode x="1"  y="1"  ten="0.5" seg="0"/>
</Module>
```

| Attribut | Bedeutung | Bereich |
|---|---|---|
| `x` | Stützstelle | −1 … +1 (beim Einlesen geklemmt) |
| `y` | Kurvenwert | −1 … +1 (beim Einlesen geklemmt) |
| `ten` | Biegegrad des Segments **davor** | 0 … 1 (0,5 = keine Biegung) |
| `seg` | Segmentmodus des Segments **davor** | 0 … 7 |

Werkskurve: drei Knoten (−1, −1), (0, 0), (1, 1) mit `seg = 0` (Linear) und
`ten = 0,5` → **exakte Identität**. Ein leerer Zustand (`<Module/>` ohne `WNode`)
setzt die Werkskurve.

### 9.2 Segmentmodi

Zwischen zwei benachbarten Knoten `a` (x_a, y_a) und `b` (x_b, y_b) mit
`u = (x − x_a) / (x_b − x_a) ∈ [0, 1]`:

| `seg` | Name | Interpolation `f(u)` | `ten` wirkt |
|---|---|---|---|
| 0 | Linear | `u` | – |
| 1 | Curve | `u^e` mit `e = 2^((ten − 0,5)·2)` → 0,5 = Gerade, 1 → `u²`, 0 → `√u` | ja (Biegegrad) |
| 2 | Half Sine | `sin(u·π/2)` | – |
| 3 | Sine | `0,5 − 0,5·cos(u·π)` (S-Kurve) | – |
| 4 | Double Wave | `u + 0,25·sin(2π·u)` | – |
| 5 | Square | `0` für `u < ten`, sonst `1` | ja (Tastverhältnis) |
| 6 | Steps/Quantize | `floor(4·u)/4` (Treppen) | – |
| 7 | Blocks | `round(4·u)/4` (Blöcke, halbe Blöcke am Rand) | – |

`y(x) = y_a + (y_b − y_a) · f(u)`.

**Reihenfolge/Entartung:** Knoten werden beim Backen nach `x` sortiert; Knoten mit
(nahezu) gleichem `x` (Abstand < 1e−6) werden verworfen. Außerhalb des
Knotenbereichs wird auf den Randwert geklemmt. Das Ergebnis wird auf ±1 begrenzt.

**`ten` außerhalb 0…1:** Das Referenzformat enthält auch negative Werte
(min. −0,8473, siehe `docs/specs/mining-report.md`). `getState()`/`setState()`
erhalten den Rohwert (verlustfreier Round-Trip); für die Übertragung wird er auf
0…1 geklemmt (offener Punkt O1).

### 9.3 Tabellen

Weil die Interpolation pro Sample zu teuer wäre (Potenz/Suche im 2×-übertasteten
Bereich), wird die Kurve bei jeder Änderung (`setCurveNodes()`/`setState()`,
Konstruktor) in **drei Tabellen mit je 2049 Stützstellen** gebacken und zur Laufzeit
linear interpoliert:

* `symTable`: ein Pfad über [−1, +1] (Modus 8),
* `asymNegTable`: Knoten mit `x ≤ 0`, Pfad über [−1, 0] (Modus 9, negativer Zweig),
* `asymPosTable`: Knoten mit `x ≥ 0`, Pfad über [0, +1] (Modus 9, positiver Zweig).

Damit ist die Transferfunktion im Betrieb rein arithmetisch (eine Multiplikation,
eine Addition, ein Tabellenzugriff), allokationsfrei und ohne Verzweigungslogik.

### 9.4 Zustand (`getState()` / `setState()`)

* `getState()` baut `<Module>` mit den sechs Attributen (`drive`, `mode`, `folds`,
  `agc`, `eqmode`, `flip`, Anzeige-Einheiten) und je Knoten ein `<WNode x y ten seg/>`.
  Ganzzahlige Werte werden als Integer geschrieben (Referenz schreibt `drive="45"`,
  nicht `45.0`), krumme auf 6 Nachkommastellen gerundet (kein Float-Rauschen).
* `setState()` übernimmt Attribute, sofern vorhanden (dieselben IDs wie `params()`,
  geklemmt auf den deklarierten Bereich) und alle `<WNode>`-Kinder in Reihenfolge.
  Unbekannte Kinder werden ignoriert. Kein `WNode` → Werkskurve.
* Beide Methoden laufen auf dem Message-Thread und dürfen allokieren.

---

## 10. Tests (`tests/DistortionModuleTests.cpp`)

29 Testfälle, 14 494 Assertions — alle grün.

| Testfall | prüft |
|---|---|
| Typ und Parameter-Tabelle | `type()`, `displayName()`, IDs/Reihenfolge/Bereiche/Einheiten, Defaults |
| setParam klemmt … | Klemmen auf min/max, ungültige Indizes |
| shape() endlich/nullpunkt/ceiling | alle Modi: endlich, `shape(0) ≈ 0`, `\|shape\| ≤ 1` (Sweep −10…10 und ±1e6) |
| HARD CLIP exakt | `shape(2) = 1`, `shape(−2) = −1`, `shape(±0,5) = ±0,5` |
| HARD/SOFT monoton | Monotonie über −3…3 |
| SOFT CLIP | `shape(0,5) > 0,5`, `shape(1e6) ≈ 1`, Symmetrie |
| SATURATE | bleibt unter 1, aber deutlich über der Geraden |
| WAVEFOLD | `folds = 2`: `shape(1,5)` innerhalb ±1, nicht konstant, `folds` wirkt und wird geklemmt |
| FUZZ | Gate bei 0,005 = 0, asymmetrischer Pegel |
| RECTIFY | `shape(−0,5) = shape(0,5)`, einseitig, im Ceiling |
| HALF WAVE | `flip` wählt die Halbwelle |
| BITCRUSH | 0,125-Raster, Ceiling |
| Werkskurve | Identität (symmetrisch **und** asymmetrisch) |
| Waveshaper-Zustand mit zwei WNodes | `setState()` → `curveNodes()` → `getState()` (x, y, ten, seg, Attribute) |
| getState-Defaults | sechs Attribute, drei Knoten, leerer Zustand = Werkskurve |
| freie Kurve | Rampe y = x/2, asymmetrische Hälften, Klemmen außerhalb |
| Segmentmodi | Linear, Curve (ten = 0/1), Square, Steps, Sine, Double Wave |
| DRIVE 0 bitidentisch | Ausgang == Eingang (− Latenz), Toleranz 1e-6, Kopf still |
| DRIVE 0 laut | dito bei 0,5 Aussteuerung in einem nichtlinearen Modus |
| Ceiling bei ±8 | alle zehn Modi, mit und ohne AGC, beide Kanäle |
| Stille / DC | 0 bleibt 0; 0,4 bleibt 0,4 |
| Kanäle getrennt | rechter Kanal bleibt 0, wenn links Signal liegt |
| Mono | 1-Kanal-Buffer |
| AGC Lautheit | ±1,5 dB zum Eingangs-RMS; ohne AGC deutlich lauter |
| AGC ±12 dB | Kurve mit −34 dB wird nur um 12 dB angehoben |
| Latenz | 0 vor `prepare()`, konstant über `prepare()`/Blockgrößen, plausibel klein |
| unvorbereitetes process() | No-op |
| reset() | reproduzierbarer Zustand |
| EQ-Chip | OFF vs. PRE/POST: 3 kHz lauter, 60 Hz leiser |

---

## 11. Integration

Die Eintragung in Build und Registry hat der Lead bereits vorgenommen (Stand dieser
Spec):

| Datei | Zeile | Eintrag |
|---|---|---|
| `CMakeLists.txt` | 67 | `src/fx/modules/DistortionModule.cpp` (in `BFKA_FX_SOURCES`) |
| `CMakeLists.txt` | 95 | `src/fx/modules/DistortionModule.h` (in `BFKA_HEADERS`) |
| `tests/CMakeLists.txt` | 21 | `DistortionModuleTests.cpp` |
| `tests/CMakeLists.txt` | 31 | `../src/fx/modules/DistortionModule.cpp` |
| `src/fx/FxFactory.cpp` | 4 | `#include "fx/modules/DistortionModule.h"` |
| `src/fx/FxFactory.cpp` | 25 | `registerType (FxType::distortion, [] { return std::make_unique<DistortionModule>(); });` |

---

## 12. Offene Punkte

| # | Punkt | Warum wichtig | Vorschlag |
|---|---|---|---|
| **O1** | `ten` ist laut Referenzdaten negativ (bis −0,8473), laut Ticket 0…1 | Preset-Klang | B-92: Referenzkurven anhören; ggf. Vorzeichen als Gegenbiegung interpretieren statt zu klemmen |
| **O2** | Referenzdaten enthalten `mode` 1…12, das Ticket zehn Modi 0…9 | Preset-Import, UI | Modi 10…12 am Original verifizieren; Bereich bleibt vorerst 0…9 |
| **O3** | Ketten-Spec (4.2) erwartet für `dist` Latenz **0**, real sind es **4** Samples (2×-Oversampling) | PDC, Kick-Position | Ketten-Spec-Tabelle korrigieren; die Kette kompensiert die 4 Samples korrekt |
| **O4** | RECTIFY / HALF WAVE / FUZZ sind einseitig bzw. asymmetrisch → **DC-Anteil** am Ausgang | Kopfraum, Rumpeln | DC-Blocker (z. B. 5-Hz-Hochpass) im Modul oder im Summenpfad nachrüsten |
| **O5** | Semantik von `seg = 5/6/7` (Square/Steps/Blocks) ist aus den Daten nicht belegt | Klangtreue | Interpretation hier dokumentiert; mit Referenz-Wellenformen abgleichen |
| **O6** | PRE und POST teilen sich denselben 1-Pol-Zustand; nach einem Moduswechsel klingt der Filter kurz ein | Klickfreiheit | getrennte Zustände für PRE/POST, oder Zustand beim Umschalten löschen |
| **O7** | DRIVE ist eine reine Blende **ohne Vorverstärkung** (Ticket-Text); das Original könnte zusätzlich den Eingang anheben | Klangtreue | A/B gegen die Referenz; ggf. `drive` zusätzlich als Pre-Gain (z. B. 0…+18 dB) |
| **O8** | AGC-Ballistik des Originals unbekannt (hier 5 ms/120 ms, RMS-Verhältnis, ±12 dB) | Pumpen | Feintuning mit Hörvergleich |
| **O9** | Gruppenlaufzeit des IIR-Halbbands ist frequenzabhängig; der Trockenpfad ist mit 4 Samples *ganzzahlig* verzögert → bei Teil-MIX der Kette bleibt eine kleine Kammfilter-Neigung nahe Nyquist | Teilmix | Alternativ linearphasiges FIR (49 Samples) oder fraktionaler Ausgleich |
| **O10** | `folds` ist im Referenzbestand praktisch konstant (2,0…2,4) | Aussagekraft | Faltungscharakter anhand von Testsignalen klären |
