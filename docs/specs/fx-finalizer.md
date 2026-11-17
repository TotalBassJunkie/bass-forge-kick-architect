# FX-Modul `final` — Finalizer (Ticket B-67)

**Typ:** `FxType::finalizer`, XML-Kennung `final`, Anzeigename „Finalizer"
**Code:** `src/fx/modules/FinalizerModule.h`, `src/fx/modules/FinalizerModule.cpp`
**Tests:** `tests/FinalizerModuleTests.cpp` (10 Testfälle)
**Stand:** 2026-11-17 — Implementierung fertig; Build/Testlauf macht der Lead
(MSVC `/std:c++20 /W4 /permissive-`). Die Zahlen in Abschnitt 8 stammen aus einer
numerischen Nachbildung der Implementierung (Python, 48 kHz, Block 128), nicht aus
dem kompilierten Modul — siehe O-F10.

---

## 1. Zweck und Abgrenzung

Der Finalizer ist die **letzte Bearbeitungsstufe** im Rack, direkt vor der
Ketten-Karte (MIX / OUT GAIN). Er bündelt vier Aufgaben, die beim Kick-Bau am
Ende anfallen:

| Stufe | Aufgabe |
|---|---|
| SUB-BOOST | Tiefton anheben (Low-Shelf), ohne den Rest zu verfärben |
| MONO BASS | Tiefton unterhalb einer Crossover-Frequenz auf Mono legen (Druck, Monokompatibilität) |
| UPWARD | leise Anteile sanft nach oben ziehen (Dichte, ohne Spitzen zu heben) |
| TRUE-PEAK-CLIP | Spitzen mit Lookahead und True-Peak-Erkennung auf 0 dBFS begrenzen bzw. clippen |

**Nicht** im Modul: `MIX` und `OUT GAIN`. Beide gehören laut Kettenvertrag
(`docs/specs/fx-chain-engine.md`) zur Kette; das Modul liefert ausschließlich sein
zu 100 % nasses Signal. Es hat **keinen** Struktur-Zustand (`getState()`/
`setState()` bleiben bei den BasDefaults aus `FxModule.h`) — alle acht Parameter
sind Skalare.

**Latenz:** `ceil(0.0015 · fs)` Samples, bei 48 kHz **72**, bei 96 kHz **144**,
bei 44,1 kHz **67**. Sie bleibt auch bei `clip = 0` und `clipOn = 0` erhalten
(der Lookahead-Puffer läuft immer mit).

---

## 2. Parameter

`params()` liefert genau diese acht Einträge in dieser Reihenfolge; der Index ist
die Nummer für `setParam()`/`getParam()` und die Zuordnung zu `fx<N>_p<M>`.

| # | `id` | Name | min | max | Default | Einheit | Bedeutung |
|---|---|---|---|---|---|---|---|
| 0 | `boost` | Sub Boost | 0 | 12 | **0** | dB | Anhebung des Low-Shelfs |
| 1 | `freq` | Sub Freq | 20 | 200 | **50** | Hz | Knie-Frequenz des Low-Shelfs |
| 2 | `clip` | Clip | 0 | 100 | **0** | % | Nassanteil des True-Peak-Clippers (0 % = kein Eingriff) |
| 3 | `upward` | Upward | 0 | 100 | **0** | % | Stärke der Aufwärtskompression |
| 4 | `monoF` | Mono Freq | 60 | 500 | **120** | Hz | Crossover des Mono-Basses |
| 5 | `shelfOn` | Shelf On | 0 | 1 | **1** | – | SUB-BOOST ein/aus |
| 6 | `monoOn` | Mono On | 0 | 1 | **1** | – | MONO BASS ein/aus |
| 7 | `clipOn` | Clip On | 0 | 1 | **1** | – | TRUE-PEAK-CLIP ein/aus |

`setParam()` klemmt auf `[minValue, maxValue]` und verwirft NaN/Inf (kein
Vergiften der Kette). `getParam()` liefert den zuletzt gesetzten Anzeige-Wert,
bei ungültigem Index `0.0f`.

**Schalter-Semantik:** `>= 0,5` = ein. `clip` wirkt nur bei `clipOn >= 0,5`
**und** `clip > 0`; `boost` nur bei `shelfOn >= 0,5`; `monoF` nur bei
`monoOn >= 0,5` (und nur bei Stereo-Eingang).

---

## 3. Signalweg

```
In
 ├─ Subsonik-Hochpass 15 Hz (2. Ordnung)          immer im Zustand, eingeblendet
 ├─ 1. SUB-BOOST   Low-Shelf 2. Ordnung (RBJ) bei `freq`, `boost` dB
 ├─ 2. MONO BASS   LR4-Crossover bei `monoF`
 │                  low  = (lowL + lowR) · 0,5   -> auf beide Kanäle
 │                  high = highL / highR         -> je Kanal
 │                  out  = low + high
 ├─ 3. UPWARD      Aufwärtskompression der Spitzen unter −6 dBFS
 ├─ 4. TRUE-PEAK-CLIP  Lookahead-Limiter, Ceiling 0 dBFS, 4×-Oversampling
 │                  out = trocken · (1 − clip/100) + limitiert · clip/100
 └─ Out (100 % nass)
```

### 3.1 Einblendung statt harter Bypass

Jede Stufe hat eine geglättete Einblendung (`engage`, `shelfMix`, `monoMix`,
`clip/100`, `upward/100`), jeweils One-Pole mit **20 ms**. Damit gilt:

* **Nullzustand bitgenau:** wenn `shelfOn = monoOn = clipOn = 0` und
  `boost = upward = clip = 0`, sind alle Einblendungen exakt 0 und die
  Verarbeitung reduziert sich auf `out = in` (um die Lookahead-Latenz
  verschoben). Der Subsonik-Hochpass ist in diesem Zustand **ausgeblendet**
  (Begründung: O-F1).
* **Kein Klick beim Umschalten:** eine Stufe wird über 20 ms ein-/ausgeblendet;
  die Filterzustände laufen dabei durch (warm), damit kein Sprung entsteht.

### 3.2 SUB-BOOST (1)

Ein RBJ-Low-Shelf 2. Ordnung (`dsp::designBiquad (lowShelf, fs, freq, Q, boost)`)
mit `Q = 1,2` (Begründung: 4.1). Bei `boost = 0` ist `A = 1`, Zähler- und
Nennerkoeffizienten sind dann identisch — die Stufe reicht ohne Pegel- und
Phasenänderung durch. Ist `shelfOn = 0`, wird sie zusätzlich hart ausgeblendet
(gemessen: 0,0 dB Unterschied zwischen `boost = 0` und `boost = 6`).

### 3.3 MONO BASS (2)

Linkwitz-Riley 4. Ordnung = zwei kaskadierte Butterworth-2-Filter je Flanke
(`Q = 0,7071`). Für den Tiefenanteil:

```
lowMono = ( lowL + lowR ) · 0,5
outL    = lowMono + highL
outR    = lowMono + highR
```

Der Höhenanteil bleibt kanalgetrennt; nur das Tiefton wird mono. Bei Mono-Eingang
(1 Kanal) ist die Stufe wirkungslos (nichts zu summieren).

### 3.4 UPWARD (3)

Stereo-gekoppelter Peak-Follower (Attack **5 ms**, Release **100 ms**) auf dem
Maximum der Kanalbeträge, danach:

```
levelDb = 20·log10 (env)
liftDb  = clamp ( −6 dBFS − levelDb , 0 , +12 dB )
gainDb  = upward/100 · liftDb
out     = in · 10^(gainDb/20)
```

`upward = 0` ergibt `gainDb = 0` → `gain = 1,0` exakt (bitgenaues Durchreichen).
Der Follower läuft auf dem **Eingang** der Stufe (feed-forward), damit die
Anhebung nicht rückkoppelt.

### 3.5 TRUE-PEAK-CLIP (4)

Der Lookahead-Puffer (Länge `L`, siehe 5) läuft immer. Die Bearbeitung ist:

```
y[n]       = Signal nach Stufe 3
peak4x[n]  = max (|y[n]|, |CatmullRom (y[n−2..n], t = 0,25 / 0,5 / 0,75)|)
need[n]    = min (1, 1 / peak4x[n])
gWin[n]    = min (need[n−L … n])            Fenster-Minimum (Stereo-gekoppelt)
gRel[n]    = min (1, gRel[n−1] + (1 − gRel[n−1]) · a_50ms)
g[n]       = min (gWin[n], gRel[n])
out[n]     = y[n−L] · (1 − clip/100) + y[n−L] · g[n] · clip/100
```

Das Fenster-Minimum enthält immer den eigenen `need`-Wert des ausgegebenen
Samples, deshalb ist `|out| <= 0 dBFS` **konstruktiv** garantiert (nicht nur
empirisch) — solange `clip = 100 %` ist. Die Release-Rampe (50 ms) verhindert
Pumpen; sie kann den Gain nur anheben, nie über `gWin` hinaus.

---

## 4. Filterdesign

### 4.1 Low-Shelf (SUB-BOOST)

RBJ-Cookbook, Koeffizienten aus `dsp::designBiquad()` (bilineare Transformation,
`alpha = sin(w0)/(2Q)`):

| Größe | Wert |
|---|---|
| `A` | `10^(boost/40)` |
| `w0` | `2π · freq / fs` |
| `Q` (Shelf-Slope) | **1,2** |
| Verstärkung am Knie (`f = freq`) | `A` = halber Boost in dB (RBJ-Eigenschaft) |
| Max. Überhöhung bei `boost = 6` | **+6,9 dB @ 29 Hz** (bei `freq = 50`) |
| Wirkung bei 40 Hz (`freq = 50`, `boost = 6`) | **+5,84 dB** |

Warum `Q = 1,2`? Mit dem Butterworth-Wert `Q = 0,7071` liefert der Shelf bei
`freq = 50` und 40 Hz nur **+4,22 dB**; der Ticket-Referenzwert („+6 dB ±1,5 dB
bei 40 Hz") wäre damit nur knapp erfüllt. `Q = 1,2` bringt am Knie bereits ~86 %
des Boosts und liegt mit +5,84 dB mittig im Toleranzband. Der Preis ist eine
Überhöhung von +0,9 dB über den Nennboost direkt unter dem Knie (bei 29 Hz statt
50 Hz) — musikalisch unkritisch, weil unterhalb von 30 Hz ohnehin der
Subsonik-Hochpass und der Clipper folgen.

### 4.2 Mono-Bass-Crossover (LR4)

Zwei kaskadierte Butterworth-2-Filter (`Q = 0,7071`) für Tiefpass bzw. Hochpass,
also LR4 mit 24 dB/Okt pro Flanke. Die Summe beider Zweige ist ein Allpass
(flat im Betrag); durch die Mono-Summierung des Tiefenanteils bleibt das
Tiefton phasenrichtig, der Höhenanteil unverändert.

### 4.3 Subsonik-Hochpass

Butterworth-2-Hochpass (`Q = 0,7071`) bei **15 Hz**, fest, gegen DC und Subsonik.
Er wird nur eingeblendet, wenn mindestens eine Stufe arbeitet (O-F1); sein Zustand
läuft immer mit, damit das Einblenden keinen Einschwingvorgang erzeugt.
Bei 40 Hz kostet er 0,08 dB, bei 1 kHz 0,00001 dB (im Signalweg nicht messbar).

---

## 5. Lookahead und True-Peak-Verfahren

### 5.1 Latenz-Formel

```
L = jmax ( 1, (int) std::ceil ( 0.0015 · fs ) )
```

| fs | 44,1 kHz | 48 kHz | 96 kHz | 192 kHz |
|---|---|---|---|---|
| `L` | **67** | **72** | **144** | **288** |

`latencySamples()` liefert `L` nach `prepare()` und **0** davor. Der Wert ist
**unabhängig** von `clip`, `clipOn` und allen anderen Parametern: der
Lookahead-Puffer ist immer in Betrieb, nur die Anwendung des Gains wird
ausgeblendet. Der Ketten-Prozessor (`FxChainProcessor`) summiert die
Modullatenzen und meldet sie per `setLatencySamples()` an den Host.

`0.0015` ist bewusst als **double** deklariert: als `float` wäre
`0.0015f · 48000 = 72,0000006`, und `ceil()` ergäbe 73 statt 72.

### 5.2 Oversampling: 4×, Catmull-Rom (kein FIR-Halbband)

Die Spitzenerkennung braucht Werte **zwischen** den Samples (Inter-Sample-Peaks,
die beim D/A-Wandler bzw. Codec über 0 dBFS landen). Drei Gründe für
Catmull-Rom-Interpolation der Nachbarwerte statt Polyphase-/Zero-Stuffing-FIR:

1. **Keine zusätzliche Latenz.** Ein FIR-Halbbandfilter mit brauchbarer
   Sperrdämpfung braucht Dutzende Taps und damit selbst Latenz; die
   Latenz-Formel aus 5.1 wäre nicht mehr exakt einhaltbar.
2. **Keine Tabellen, keine Allokation, kein Zustand pro Kanal außer zwei
   `float`.** Der Filterkern läuft im Audio-Thread, `process()` darf nicht
   allokieren; ein Tabellenfilter wäre entweder konstant (Tabelle im Speicher)
   oder müsste pro Block neu gebaut werden.
3. **Fehler klein genug.** Für ein bandbegrenztes Signal (< 20 kHz bei 48 kHz)
   liegt der Interpolationsfehler weit unter der Pegelauflösung; die
   True-Peak-Korrektur bewegt sich im Zehntel-dB-Bereich.

Berechnung: zwischen `p1 = y[n−1]` und `p2 = y[n]` mit den Stützstellen
`p0 = y[n−2]` und `p3 = 2·y[n] − y[n−1]` (lineare Extrapolation, weil das Signal
kausal ist und `y[n+1]` fehlt):

```
y(t) = ½ · [ 2·p1 + (−p0 + p2)·t + (2p0 − 5p1 + 4p2 − p3)·t² + (−p0 + 3p1 − 3p2 + p3)·t³ ]
```

ausgewertet bei `t = 0,25 / 0,5 / 0,75`; `|y[n]|` kommt immer mit ins Maximum.
Der Gain wird stereo-gekoppelt aus dem Maximum beider Kanäle gebildet (die
Stereobasis bleibt erhalten).

### 5.3 Warum kein „harter" Clipper

`clip = 100 %` ist ein **True-Peak-Limiter** mit Gain-Reduktion, kein
Wellenform-Clipper: bei 1,5 ms Lookahead und Fenster-Minimum wird die Spitze
vorher abgesenkt statt abgeschnitten. `clip < 100 %` blendet zwischen trockenem
und limitiertem Signal (0 % = kein Eingriff). Konsequenz: bei Teilwerten ist der
Ausgang **nicht** garantiert unter 0 dBFS (O-F6).

---

## 6. Threading

| Punkt | Umsetzung |
|---|---|
| Parameterübergabe | `std::atomic<float> paramTargets[8]`, `memory_order_relaxed`; der Audio-Thread liest sie einmal pro Block |
| Glättung | One-Pole, 20 ms (`1 − exp(−1/(fs·0,020))`), **sampleweise** im Audio-Thread |
| Filterkoeffizienten | höchstens **einmal pro Block** neu (`updateFilterCoefficients()` am Blockanfang), nur bei Änderung > 1e-6 |
| `process()` | kein `new`, kein `log`, kein Mutex, keine `std::vector`-Größenänderung; alle Puffer in `prepare()` allokiert |
| Puffer | `delayLine[2][L]`, `gainRing[L+1]` (`std::vector`, in `prepare()` dimensioniert) |
| Diagnose | `currentUpwardGainDb()`, `currentLimiterGain()` (atomar, nur Tests/Doku) |
| Rechenaufwand | pro Sample: 1 HP + 1 Shelf + 4 Crossover-Biquads + Follower + 4 Interpolationen + **L+1 Vergleiche** für das Fenster-Minimum (73 bei 48 kHz) |

Das Fenster-Minimum wird bewusst als lineare Suche über den Ring implementiert
(kein Monotonic-Deque): 73 `min`-Operationen pro Sample sind bei 48 kHz
~0,1 % eines Kerns und der Code bleibt trivial prüfbar. Bei 192 kHz sind es 289
Vergleiche pro Sample — falls das CPU-Budget dort reißt, ist der Deque die
naheliegende Optimierung (O-F8).

---

## 7. Abweichungen und offene Punkte

| # | Punkt | Entscheidung / Grund | Vorschlag |
|---|---|---|---|
| **O-F1** | Ticket sagt „Hochpass 15 Hz, immer aktiv"; der Nulltest verlangt aber `Ausgang = Eingang` (Toleranz 1e-6). | Ein aktiver 15-Hz-Hochpass verschiebt Phase und Betrag jedes Signals (bei 1 kHz: −0,021 rad, ~1e-2 Abweichung) — beides ist zusammen **nicht** erfüllbar. Der HP läuft im Zustand immer mit, wird aber nur eingeblendet, wenn mindestens eine Stufe arbeitet. | Falls der HP zwingend immer hörbar sein soll: Nulltest auf „HP + Latenz" umdefinieren (Test 4 anpassen) und das Einblenden entfernen. |
| **O-F2** | Ticket: „1-kHz-Signal bleibt unkorreliert" beim Mono-Bass. | Eine **normierte** Korrelation ist hier kein brauchbares Maß: der rechte Kanal enthält nach dem Mono-Bass noch −80 dB des 1-kHz-Tons (Rest des LR4-Tiefpasses) und ist damit trotzdem phasengleich → Korrelation 1,0. Der Test prüft deshalb die Pegeldifferenz (`R < 0,01·L`) **und** `max|L−R| > 0,2`. | Bei Bedarf im Test zusätzlich einen Hochpass auf beide Kanäle legen, dann ist auch die Korrelation ein valides Maß. |
| **O-F3** | Shelf-Güte `Q = 1,2` ist eine Setzung (RBJ kennt nur „Slope"). | Erfüllt den Referenzwert +6 dB bei 40 Hz/`freq = 50`; `Q = 0,7071` ergäbe nur +4,2 dB (knapp unter der Toleranz). | Bei Hörvergleich ggf. auf 1,0…1,4 nachziehen (eine Konstante: `kShelfQ`). |
| **O-F4** | Upward-Gesetz (Schwelle −6 dBFS, Steigung 1, Deckel +12 dB) ist nicht aus Referenzdaten ableitbar. | Einfachstes feed-forward-Gesetz, das „leise anheben, laute nicht anfassen" erfüllt und bei 0 % exakt neutral ist. | Hörvergleich; Schwelle/Steigung/Deckel liegen zentral in `FinalizerModule.h`. |
| **O-F5** | Limiter-Gain ist ein gleitendes Fenster-Minimum; die Attack-Flanke ist damit nicht „weich". | Konstruktive Garantie `|out| <= 0 dBFS`; der Lookahead verhindert, dass die Absenkung als Sprung im Ausgang hörbar wird. | Bei Bedarf eine Attack-Rampe über `L/4` Samples ergänzen (dann Garantie nur noch empirisch). |
| **O-F6** | `clip < 100 %` ist eine Trocken/Nass-Blende; der Ausgang kann weiter über 0 dBFS liegen. | So im Ticket gefordert („Zwischen Trocken und Clipped wird mit `clip/100` geblendet"). | — |
| **O-F7** | Maximal 2 Kanäle. | Das Rack ist stereo; Kanäle > 2 werden nicht bearbeitet. | Bei Bedarf `maxChannels` erhöhen (Puffer werden in `prepare()` dimensioniert). |
| **O-F8** | CPU-Budget (< 3 % je Modul) in dieser Umgebung nicht messbar (kein Echtzeit-Host). | — | Im Host mit 10 Instanzen bei 48 kHz/128 messen; ggf. Fenster-Minimum auf Monotonic-Deque umstellen. |
| **O-F9** | Registrierung in `FxFactory.cpp` und CMake-Einträge sind Lead-Aufgaben; dieses Ticket ändert nur die vier Dateien. | Vorgabe des Tickets. | Siehe Abschnitt 10. |
| **O-F10** | Die Messwerte in Abschnitt 8 stammen aus einer numerischen Nachbildung (Python) der Implementierung, nicht aus dem kompilierten Modul. | Kein Build in dieser Umgebung (Vorgabe). | Nach dem Lead-Build Testsuite laufen lassen; die Schwellen sind mit ≥ 2× Abstand zu den Messwerten gewählt. |

---

## 8. Messwerte (Vorabbestimmung, 48 kHz, Block 128)

| Messung | Einstellung | Wert |
|---|---|---|
| Latenz | 44,1 / 48 / 96 / 192 kHz | 67 / **72** / 144 / 288 Samples |
| Nullzustand | alle Stufen aus | `max|out − in[n−72]|` = **0,0** (bitgenau) |
| Nullzustand, 50 Blöcke | wie oben, Dauerbetrieb | bitgenau, kein Zustandsleck |
| Sub-Boost 40 Hz | `freq = 50`, `boost = 6` vs. `0` | **+5,84 dB** |
| Sub-Boost 5 kHz | `freq = 50`, `boost = 6` vs. `0` | **−0,0004 dB** |
| Sub-Boost ohne Schalter | `shelfOn = 0`, `boost = 6` vs. `0` | **0,0 dB** |
| Mono-Bass 60 Hz (L = Ton, R = Stille) | `monoF = 120` | Korrelation **1,000**, Peak L 0,264 / R **0,235** |
| Mono-Bass 60 Hz, `monoOn = 0` | — | Peak R **0,0** |
| Mono-Bass 1 kHz | `monoF = 120` | Peak R / Peak L = **1,03e-4** (−79,7 dB), `max|L−R|` = 0,50 |
| True-Peak, +6 dB Übersteuerung | `clipOn = 1`, `clip = 100` | Peak **1,000** (1 kHz, 61,7 Hz, 11,025 kHz, 41,3 Hz) |
| True-Peak aus | `clip = 0` | Peak **2,000** |
| Upward −40 dBFS | `upward = 0 → 100` | **+12,0 dB** |
| Upward −3 dBFS | `upward = 100` | **0,0 dB** (über der Schwelle) |
| Parametersprung `boost` 0 → 12 dB | 40-Hz-Ton, Sprung an der Blockgrenze | `max|y[n]−y[n−1]|` = **0,0035** (Amplitude 0,50 → 2,05) |
| 100 Blöcke Rauschen, alles auf Maximum | ±0,9 | endlich, Peak **1,000** |

---

## 9. Tests (`tests/FinalizerModuleTests.cpp`)

| Testfall | Prüft | Schwelle |
|---|---|---|
| Parametertabelle und Metadaten | 8 IDs/Namen in Reihenfolge, Bereiche, Defaults, Einheiten, `type()`, `displayName()`, Latenz 0 vor `prepare()`, kein Struktur-Zustand, ungültige Indizes, NaN/Inf-Abweisung | exakt |
| Parameter werden geklemmt | `boost` 99 → 12, `freq` 1 → 20, `clip` −1 → 0, `upward` 150 → 100, `monoF` 10/900 → 60/500 | exakt |
| Lookahead-Latenz | 0 vor `prepare()`, 72 @48 k, 144 @96 k, 67 @44,1 k, 288 @192 k, unabhängig von `clip`/`clipOn`, `lookaheadSamplesFor (0) = 72`, `(100) = 1` | exakt |
| Neutralzustand | alle Stufen aus: `out = in[n−L]`, erster Puffer leer, 50 Blöcke Dauerbetrieb | `< 1e-6` (ist 0) |
| Sub-Boost | 40 Hz +6 dB ±1,5; 5 kHz ±0,2 dB; `shelfOn = 0` ±0,1 dB | siehe links (ist 5,84 / 0,0004 / 0) |
| Mono-Bass | 60 Hz: Korrelation > 0,99, R > 0,15, `monoOn = 0` → R = 0; 1 kHz: R < 0,01·L, `max|L−R|` > 0,2 | siehe links |
| True-Peak | `clip = 100` und +6 dB Übersteuerung: alle `|x| <= 1` (1 kHz, 61,7 Hz, 11,025 Hz); `clip = 0`: Peak 2,0; `clipOn = 0`: keine Begrenzung | `<= 1 + 1e-6` |
| Upward | −40 dBFS: > 6 dB mehr Pegel (ist 12), ≤ 12,5 dB; −3 dBFS: ±0,5 dB | siehe links |
| Parametersprung | Sprung `boost` 0 → 12 dB an der Blockgrenze, Pegel steigt > 2×, `max|Δ| <= 0,5` | ≤ 0,5 (ist 0,0035) |
| Stabilität/Grenzfälle | 100 Blöcke Rauschen endlich und begrenzt, Stille bleibt still, Mono-Puffer, 1-/7-Sample-Block, leerer Block, Aufruf ohne `prepare()` (Puffer unverändert), `reset()` | siehe links |

---

## 10. Einbindung (Zeilen für den Lead)

Dieses Ticket legt **nur** die vier Dateien an und ändert kein CMake und keine
Factory.

`CMakeLists.txt`:

```cmake
set(BFKA_FX_SOURCES
    ...
    src/fx/modules/FinalizerModule.cpp   # NEU (B-67)
    )

set(BFKA_HEADERS
    ...
    src/fx/modules/FinalizerModule.h     # NEU (B-67)
    )
```

`tests/CMakeLists.txt`:

```cmake
target_sources(BFKA_Tests PRIVATE
    ...
    FinalizerModuleTests.cpp               # NEU (B-67)
    ../src/fx/modules/FinalizerModule.cpp  # NEU (B-67)
    )
```

`src/fx/FxFactory.cpp` (ersetzt den `PassThroughModule`-Platzhalter):

```cpp
#include "fx/modules/FinalizerModule.h"
...
registerType (FxType::finalizer, [] { return std::make_unique<FinalizerModule>(); });
```

Öffentliche Member des Moduls: `FinalizerModule()`, `params()`, `prepare()`,
`reset()`, `setParam()`, `getParam()`, `process()`, `latencySamples()`,
`displayName()`, `type()`, `lookaheadSamplesFor()`, `currentUpwardGainDb()`,
`currentLimiterGain()`, `sampleRate()`, `blockSize()` sowie die Konstanten
`maxChannels`, `oversamplingFactor`, `kSubsonicHz`, `kSubsonicQ`, `kShelfQ`,
`kCrossoverQ`, `kUpwardThresholdDb`, `kUpwardMaxLiftDb`, `kUpwardAttackMs`,
`kUpwardReleaseMs`, `kLookaheadSeconds`, `kCeiling`, `kParamSmoothMs`,
`kLimiterReleaseMs`, `kDenormalGuard`, `kCoeffEpsilon` und das Enum
`ParamIndex` (`pBoost`, `pFreq`, `pClip`, `pUpward`, `pMonoFreq`, `pShelfOn`,
`pMonoOn`, `pClipOn`, `numParams`).

---

## Nachtrag 2026-11-17 (B-92b, B-147)

* **UI:** src/ui/editors/FinalizerView.{h,cpp} — reine Anzeige: Sub-Shelf-Kurve
  (oost/req), Mono-Bass-Marker (monoF), Balken für Aufwärts-Anhebung
  (currentUpwardGainDb()), Limiter-Reduktion (currentLimiterGain()) und
  Clip-Lampe; Statuszeile LAT / CLIP / UP.
* **Latenz (B-147):** PluginProcessor::updateModuleRenderLength() ändert die
  Latenz nicht, aber ein Test prüft die gemeldete Host-Latenz:
  getLatencySamples() == 72 bei 48 kHz, 144 bei zwei Finalizern in Reihe
  (	ests/RenderLengthTests.cpp).