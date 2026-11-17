# Equalizer (`eq`) — Spezifikation

**Ticket:** B-61 · **Modul:** `FxType::equalizer`, Referenz-String `"eq"`
**Code:** `src/fx/modules/EqualizerModule.h/.cpp` · **Tests:** `tests/EqualizerModuleTests.cpp`
**Vertrag:** `src/fx/FxModule.h`, `docs/specs/fx-chain-engine.md`, `docs/01-BESTANDSAUFNAHME.md` 5.10/6.2
**Stand:** 2026-11-17 · Zielplattform MSVC 19.5x, C++20, JUCE 8

---

## 1. Zweck und Abgrenzung

Parametrischer Equalizer auf der Summe. Bis zu **12 Bänder** kommen als Struktur-Zustand
(`<Band>`-Kinder), dazu drei **klassische Regler** (LOW/MID/HIGH) mit eigenen Frequenzen
und ein **MIDDYN**-Anteil.

Nicht Teil des Moduls (macht die Kette, siehe `fx-chain-engine.md` 3.1):
`MIX`, `OUT GAIN`, Bypass, Latenzausgleich. Das Modul liefert immer sein zu 100 % nasses
Signal und ist latenzfrei (Minimum-Phase, `latencySamples() == 0`).

Der Modulzustand besteht aus zwei Teilen:

| Teil | Übergabe | Inhalt |
|---|---|---|
| Skalar-Parameter | `setParam()` / `getParam()` (8 Stück, Anzeige-Einheiten) | `lowgain … middyn` |
| Struktur | `setState()` / `getState()` (`juce::ValueTree`) | bis 12 `<Band>` |

---

## 2. Parameter

`params()` liefert genau diese acht Einträge in dieser Reihenfolge; der Index ist die
Nummer für `setParam()`/`getParam()` und die spätere Zuordnung zu `fx<N>_p<M>`.

| # | `id` | Name | min | max | Default | Einheit |
|---|---|---|---|---|---|---|
| 0 | `lowgain`  | Low Gain     | −18 | 18    | 0    | dB |
| 1 | `lowfreq`  | Low Freq     | 16  | 200   | 90   | Hz |
| 2 | `midgain`  | Mid Gain     | −18 | 18    | 0    | dB |
| 3 | `midfreq`  | Mid Freq     | 20  | 5000  | 900  | Hz |
| 4 | `midq`     | Mid Q        | 0,1 | 10    | 0,9  | — |
| 5 | `highgain` | High Gain    | −18 | 18    | 0    | dB |
| 6 | `highfreq` | High Freq    | 800 | 20000 | 8000 | Hz |
| 7 | `middyn`   | Mid Dynamic  | 0   | 100   | 0    | % |

Die Werte sind Anzeige-Einheiten (dB/Hz/%), nicht normalisiert. `setParam()` klemmt auf
den deklarierten Bereich (doppelte Absicherung zur Kette), `getParam()` liefert den letzten
gesetzten Wert.

---

## 3. Kaskade und Bandbelegung

Die Kaskade hat **feste 15 Plätze**; verarbeitet werden die ersten `chainBandCount`:

```
Stufe 0  LOW    BELL      f = lowfreq,  g = lowgain,  Q = 0,7 (fest), dyn = 0
Stufe 1  MID    BELL      f = midfreq,  g = midgain,  Q = midq,      dyn = middyn
Stufe 2  HIGH   H-SHF     f = highfreq, g = highgain, Q = 0,7071 (fest), dyn = 0
Stufe 3… Stufe 3+n-1: die n Nutzerbänder aus setState() in Dokumentreihenfolge
```

Damit sind die klassischen Regler „Teil derselben Kette wie die 12 Bänder" (Handbuch) und
belegen die Bänder 0…2. Die Nutzerbänder kommen **zusätzlich** dazu (Handbuch: „Klick in den
Graphen fügt hinzu"), die Gesamtzahl ist 3 + 12 = 15 Biquads.

**Flache Bänder werden übersprungen.** Ein BELL/L-SHF/H-SHF mit |g| ≤ 10⁻⁴ dB ist
mathematisch exakt die Identität (`b == a` bei A = 1) und wird aus der Kaskade genommen;
seine Zustände werden gelöscht. Konsequenz: im Werkszustand (alle drei Regler 0 dB, keine
Nutzerbänder) ist die Kaskade leer und `process()` **berührt den Buffer nicht** —
bitidentisches Durchreichen. Das ist die geprüfte Grenzfall-Eigenschaft „0 Bänder =
Durchreichen".

L-CUT/H-CUT/NOTCH sind nie „flach"; `g` wird bei ihnen ignoriert (sie haben keine
Bandverstärkung, siehe 4).

---

## 4. Bandtypen und Formeln (RBJ Cookbook)

Alle Koeffizienten in `double`, nach `a0` normalisiert. Bezeichner:

```
w0    = 2π·f/Fs
cosw  = cos(w0),  sinw = sin(w0)
alpha = sinw / (2·Q)              Q ∈ [0,1 … 10]
A     = 10^(g/40)                 g ∈ [−32 … +32] dB
```

| `kind` | Typ | Formeln |
|---|---|---|
| 0 | BELL (peaking) | `b0 = 1 + alpha·A`, `b1 = −2·cosw`, `b2 = 1 − alpha·A`, `a0 = 1 + alpha/A`, `a1 = −2·cosw`, `a2 = 1 − alpha/A` |
| 1 | L-SHF (low shelf) | `b0 = A·((A+1) − (A−1)·cosw + 2√A·alpha)`, `b1 = 2A·((A−1) − (A+1)·cosw)`, `b2 = A·((A+1) − (A−1)·cosw − 2√A·alpha)`, `a0 = (A+1) + (A−1)·cosw + 2√A·alpha`, `a1 = −2·((A−1) + (A+1)·cosw)`, `a2 = (A+1) + (A−1)·cosw − 2√A·alpha` |
| 2 | H-SHF (high shelf) | `b0 = A·((A+1) + (A−1)·cosw + 2√A·alpha)`, `b1 = −2A·((A−1) + (A+1)·cosw)`, `b2 = A·((A+1) + (A−1)·cosw − 2√A·alpha)`, `a0 = (A+1) − (A−1)·cosw + 2√A·alpha`, `a1 = 2·((A−1) − (A+1)·cosw)`, `a2 = (A+1) − (A−1)·cosw − 2√A·alpha` |
| 3 | L-CUT (= Hochpass) | `b0 = (1+cosw)/2`, `b1 = −(1+cosw)`, `b2 = (1+cosw)/2`, `a0 = 1+alpha`, `a1 = −2·cosw`, `a2 = 1−alpha` |
| 4 | H-CUT (= Tiefpass) | `b0 = (1−cosw)/2`, `b1 = 1−cosw`, `b2 = (1−cosw)/2`, `a0 = 1+alpha`, `a1 = −2·cosw`, `a2 = 1−alpha` |
| 5 | NOTCH | `b0 = 1`, `b1 = −2·cosw`, `b2 = 1`, `a0 = 1+alpha`, `a1 = −2·cosw`, `a2 = 1−alpha` |

Eigenschaften, die für die Prüfungen benutzt werden:

* **BELL** hat bei `f` exakt `10^(g/20)` (für g = +12 dB also +12,0 dB) und außerhalb
  seiner Bandbreite (`f/Q`) praktisch 0 dB.
* **Shelf** erreicht den vollen dB-Wert erst weit außerhalb des Knies; am Knie selbst liegt
  er bei etwa der Hälfte. Deshalb messen die Tests den H-SHF bei **10 kHz** und einer
  Eckfrequenz von 1 kHz (rechnerisch +12,05 dB bzw. −0,07 dB bei 100 Hz).
* **L-CUT** @ 500 Hz: bei 100 Hz ca. −28 dB, bei 5 kHz ca. 0 dB.
* **NOTCH** hat bei `f` eine echte Nullstelle; die Restdurchlässigkeit ist nur
  Rechenrauschen.

Ausführung: **Transposed Direct Form II** mit `double`-Zuständen je Band **und Kanal**:

```
y  = b0·x + z1
z1 = b1·x − a1·y + z2
z2 = b2·x − a2·y
```

### 4.1 `ch` (Kanal)

`ch` wird gelesen, geklemmt (0/1) und beim Speichern unverändert zurückgeschrieben. Im
Equalizer auf der Stereo-Summe hat es **keine DSP-Wirkung** — die Semantik
`0 = MID, 1 = SIDE` gilt erst im Mid/Side-Modul (`mseq`), das dieselbe Bandstruktur nutzt.
Der Wert geht beim Round-Trip also nicht verloren.

---

## 5. Dynamische Gegenregelung (`dyn`, `middyn`)

Nur für **BELL/L-SHF/H-SHF** wirksam (nur dort gibt es eine „Bandverstärkung"; bei
L-CUT/H-CUT/NOTCH wird `dyn` ignoriert — siehe O3).

1. **Hüllkurve des Band-Ausgangs** (mono-verknüpft über die Kanäle, Maximum der
   Beträge). Der Bandausgang ist der Ausgang des *statischen* Biquads mit dem
   Basis-Gain — die Regelung wirkt also **vorwärts** (feed-forward) und kann nicht
   rückgekoppelt schwingen.

   ```
   attack   = 1 − exp(−1/(10 ms · Fs))
   release  = 1 − exp(−1/(200 ms · Fs))
   env     += (|y|max − env) · (|y|max > env ? attack : release)
   ```

2. **Pegelfaktor** gegen die Schwelle −6 dBFS (linear 0,5):

   ```
   level = clamp((env − 0,5) / 0,5, 0 … 1)      // 0 unter der Schwelle, 1 ab +6 dB darüber
   amount = level · (dyn / 100)
   ```

3. **Gegenregelung als Kreuzblende** zwischen Bandausgang `y` und Band-Eingang `x`:

   ```
   k    = 1 − amount·(1 − k_min)
   out  = x + k·(y − x)
   ```

   `k = 1` → volles Band, `k = 0` → Band aus. Die Blende ist **pro Sample stetig**
   (kein Zipper, kein Block-Sprung) und erhält die Spektralform, weil sie die
   Band-Abweichung von der Flachheit skaliert.

4. **`k_min`** wird so gelöst, dass die Gegenregelung bei Vollausschlag genau
   `min(6 dB, |g|)` wegnimmt. Am Bandzentrum gilt `|H| = G` und
   `out = x·(1 + k·(G−1))`; daraus

   ```
   G      = 10^(g/20)
   G_ziel = 10^((g − sign(g)·min(6, |g|))/20)
   k_min  = clamp((G_ziel − 1) / (G − 1), 0 … 1)
   ```

   Beispiele: g = +12 dB → `k_min = 0,331` (Band fällt bei Vollausschlag von +12 auf
   +6 dB); g = +3 dB → `k_min = 0` (Band geht ganz auf 0 dB, mehr als 6 dB ist nicht
   nötig); g = −12 dB → `k_min = 0,666` (Absenkung wird auf −6 dB zurückgenommen).
   Bei g = 0 dB bleibt das Band flach und wird übersprungen.

5. `MIDDYN` ist `dyn` des klassischen MID-Bands; LOW/HIGH haben `dyn = 0`.

---

## 6. Glättung, Koeffizienten, Stabilität

* **Glättung:** alle acht Parameter und alle Bandwerte (`f`, `q`, `g`, `dyn`) laufen mit
  einer Exponentialglättung von **20 ms** (Anforderung: ≥ 5 ms) auf ihren Zielwert und
  springen exakt auf das Ziel, sobald der Rest unter 10⁻⁶ liegt. Die Glättung läuft
  **pro Block** (`1 − exp(−blockLen/(tau·Fs))`), nicht pro Sample.
  Ein Typwechsel (`kind`) ist keine Zieh-Bewegung: dort werden Zielwerte sofort
  übernommen und der Filterzustand gelöscht (Dokumentierte Klickmöglichkeit beim
  Typwechsel, siehe O5).
* **Koeffizienten** werden **einmal pro Block** gesetzt, und nur, wenn sich ein
  wirksamer Wert tatsächlich geändert hat (Vergleich gegen die zuletzt angewendeten
  Werte). Nie pro Sample.
* **Stabilitätsgrenzen:** `Q ∈ [0,1; 10]`, `f ∈ [10 Hz; min(20 kHz, 0,49·Fs)]`,
  `g ∈ [−32; +32] dB`. Bei `a0 = 0` oder nicht-endlichen Koeffizienten wird die Stufe als
  Identität geschaltet (`b0 = 1`, Rest 0) und deaktiviert — kein NaN kann in den Buffer
  gelangen.
* **Ungültige `kind`-Werte** (außerhalb 0…5, kann nur durch direkte Manipulation des
  Baums entstehen) machen die Stufe inaktiv.
* **Grenzfälle (geprüft):** 0 Bänder → bitidentisches Durchreichen; 12 Bänder @ +32 dB →
  endlich, keine Aufschaukelung; Frequenzen 0,5 Hz / 96 kHz / 0,1 Hz / 999999 Hz →
  geklemmt, endlich; `q = 0` bzw. `q = 100` → geklemmt.
* **Keine Allokation, kein Logging, keine Sperre** in `process()`; alle Puffer sind
  Mitglieder fester Größe. Maximal 8 Kanäle werden verarbeitet (Kettensignal ist stereo;
  weitere Kanäle bleiben unberührt).

### 6.1 Threading

| Daten | Weg |
|---|---|
| 8 Parameter | `std::atomic<float> paramTargets[8]`, gelesen pro Block |
| Bandliste | lock-freier Snapshot: `std::atomic` Felder je Band + Versionzähler (`release` beim Schreiben, `acquire` beim Lesen, Re-Check nach dem Lesen → verworfen und im nächsten Block wiederholt, wenn der Schreiber dazwischenkam) |
| Audio-Zustand | nur Audio-Thread (`bandCurrent`, `bandStages`) |

`prepare()`/`reset()` übernehmen den Snapshot sofort (`force`), damit ein vor dem
`prepare()` gesetzter Zustand ohne einen Block Verzögerung wirkt.

---

## 7. Zustandsformat (`getState()` / `setState()`)

`getState()` liefert einen `juce::ValueTree` vom Typ `"Module"` mit einem `"Band"`-Kind je
Band — exakt das Referenzformat:

```xml
<Module>
  <Band kind="1" f="71.64"  q="0.676" g="15.70" dyn="0.0"  ch="0"/>
  <Band kind="0" f="440.0"  q="1.2"   g="-3.5"  dyn="50.0" ch="0"/>
  <Band kind="4" f="9000.0" q="0.7"   g="0.0"   dyn="0.0"  ch="1"/>
</Module>
```

| Attribut | Bedeutung | Klemmung beim Lesen |
|---|---|---|
| `kind` | 0 = BELL, 1 = L-SHF, 2 = H-SHF, 3 = L-CUT, 4 = H-CUT, 5 = NOTCH | 0 … 5 |
| `f` | Frequenz in Hz | 10 … 20000 |
| `q` | Güte | 0,1 … 10 |
| `g` | Verstärkung in dB | −32 … +32 |
| `dyn` | dynamische Gegenregelung in % | 0 … 100 |
| `ch` | 0 = beide/MID, 1 = SIDE (nur `mseq` wirksam) | 0 … 1 |

Regeln:

* Es werden **nur** `"Band"`-Kinder gelesen (Typvergleich ohne Groß-/Kleinschreibung);
  fremde Kinder (`<WNode>`, `<Pt>`, …) werden ignoriert.
* Unbekannte Attribute werden nicht gelesen und beim Zurückschreiben **nicht** erhalten
  (der Zustand ist ein Modell, kein Rohdokument). Für `getState()` unbekannte Attribute
  gibt es daher keinen Platz im Format.
* Zahlen dürfen als Zahl **oder als String** im Baum stehen (ValueTrees aus XML tragen
  Zahlen oft als String); beides wird gelesen.
* Mehr als 12 `"Band"`-Kinder: die **ersten 12** werden übernommen, der Rest verworfen.
* `getState()` schreibt die Werte **unverändert** (keine Rundung) — ein Round-Trip ist
  verlustfrei.
* Die klassischen Regler erscheinen **nicht** als `<Band>` (sie sind Attribute des
  `<Module>`, wie im Referenz-XML). `getState()` liefert bei leerer Bandliste also null
  Kinder.

---

## 8. Tests (`tests/EqualizerModuleTests.cpp`)

Messverfahren: Sinus einschwingen lassen (0,3 s), dann RMS des eingeschwungenen Zustands
(0,32 s) gegen den analytischen Eingangs-RMS (Amplitude 0,5) vergleichen. Bei den
Testfrequenzen 100 Hz / 500 Hz / 900 Hz / 1 kHz / 5 kHz / 10 kHz / 16 kHz ist das
Messfenster ein ganzzahliges Vielfaches der Periodendauer.

| Testfall | Prüft |
|---|---|
| Parameterliste | acht IDs in Reihenfolge, Werkswerte, Bereiche, Klemmung, ungültiger Index |
| BELL 1 kHz +12 dB | 1 kHz ≈ +12 dB (±1,5), 100 Hz ≈ 0 dB (±0,5) |
| H-SHF +12 dB | 10 kHz ≈ +12 dB (±1,5), 100 Hz ≈ 0 dB (±0,5) |
| L-CUT 500 Hz | 100 Hz mindestens 15 dB leiser als 5 kHz, 5 kHz ≈ 0 dB |
| NOTCH 1 kHz | mindestens 30 dB unter dem Pegel bei 100 Hz |
| 0 Bänder | Sample-für-Sample-Identität (±10⁻⁶), `getState()` leer |
| 12 Bänder | alle sechs Typen, ±32 dB, `dyn` aktiv, ~1 s Laufzeit: alles endlich, kein Aufschaukeln |
| Frequenzränder | 0,5 Hz / 96 kHz / 0,1 Hz / 999999 Hz: endlich, keine Aufschaukelung |
| `dyn` | +12-dB-Band mit `dyn = 100`: 4…8 dB unter der statischen Verstärkung (max. 6 dB) |
| klassische Regler | LOW → BELL bei `lowfreq`, HIGH → H-SHF, MID → BELL bei `midfreq` |
| Round-Trip | drei Bänder rein, drei mit gleichen Werten raus (`kind`, `f`, `q`, `g`, `dyn`, `ch`) |
| 12er-Grenze | 20 Bänder rein → 12 raus, Reihenfolge erhalten |
| Fremde Attribute | `<WNode>` ignoriert, `colour` ignoriert, `g` als String gelesen |
| Leeres `setState` | löscht die Bänder, Signal ist danach wieder Durchreichen |

---

## 9. Offene Punkte

| # | Punkt | Warum wichtig | Vorschlag |
|---|---|---|---|
| **O1** | **`kind` 3…5 ist nicht belegt.** Aus den Referenzdaten belegt sind nur `kind = 0 → BELL` sowie `1`/`2` als „Low-/High-Band" (`01-BESTANDSAUFNAHME` 6.2: „1 = L-SHF (o. L-CUT), 2 = H-SHF (o. H-CUT)"). Die Zuordnung 3 = L-CUT, 4 = H-CUT, 5 = NOTCH folgt ausschließlich der Handbuch-Reihenfolge der Typen (BELL, L-SHF, H-SHF, L-CUT, H-CUT, NOTCH). | Preset-Kompatibilität: eine falsche Zahl macht aus einem Shelf einen Hochpass. | Am Original verifizieren: Presets mit `kind ≥ 3` suchen und den Frequenzgang messen. **Hypothese B**, die die Formulierung „1 = L-SHF **oder** L-CUT" nahelegt: `kind` 1/2 sind ein Low-/High-Band, das je nach **Vorzeichen von `g`** als Shelf (g > 0) oder Cut (g < 0) wirkt; 3…5 wären dann anders belegt. Test: Preset mit `kind = 1` und `g < 0` laden — klingt es wie eine Absenkung (Hypothese A) oder wie ein Hochpass (Hypothese B)? |
| **O2** | Q der klassischen LOW/HIGH-Bänder ist nirgends belegt (es gibt nur `midq`). | Klang der drei Regler. | Hier 0,7 (LOW) und 0,7071 (HIGH, max. Flankensteilheit ohne Überschwingen) gesetzt. Am Original messen und nachtragen. |
| **O3** | `dyn` bei L-CUT/H-CUT/NOTCH ist hier **wirkungslos**. | Handbuch nennt „dynamische Bandverstärkung" — für Cuts/Notch gibt es keine Verstärkung. Referenz könnte stattdessen die Flankensteilheit/Q oder die Cut-Tiefe regeln. | Am Original prüfen (Band mit `kind = 3` und `dyn > 0`); dann ggf. auf `q`-Modulation erweitern. |
| **O4** | Kennlinie der Gegenregelung (Schwelle −6 dBFS, linearer Pegelfaktor, max. 6 dB) ist eine **eigene Setzung**; das Handbuch nennt keine Zahlen. | „MIDDYN" klingt nur bei passender Schwelle/Zeit wie das Original. | Parameter am Original messen (Attack/Release/Schwelle) und hier ersetzen. |
| **O5** | Typwechsel eines Bands löscht den Filterzustand → möglicher Klick. | Hörbarkeit beim Umschalten im Band-Menü. | Bei Bedarf überblenden (kurze Gain-Rampe auf 0 dB, Typwechsel, zurück). |
| **O6** | Der Graph (Klick fügt Band hinzu, Ziehen verschiebt, Mausrad ändert Q, Doppelklick löscht) ist **UI**, nicht Modul. Das Modul bietet dafür nur `setState()`/`getState()`. | Ohne UI keine Bedienung. | Eigenes UI-Ticket: Graph mit Spectrum-Analyse (B-61 nennt „Spektrum-Analyse"), Band-Knoten und den Typ-Menüs; Zustand über `setState()` schreiben. |
| **O7** | Kein Spektrum-Analysator im Modul (B-61 nennt ihn). | Anzeige. | Wie `ModuleDisplay` in der Kette: Analyse gehört ins UI (FFT auf `pre`/`post`), nicht in den Audio-Pfad des Moduls. |
| **O8** | Max. 8 Kanäle; bei mehr Kanälen bleiben weitere unberührt. | Sicherheitsnetz, Kette ist stereo. | Unkritisch; bei Bedarf `maxChannels` erhöhen (kostet nur Speicher). |

---

## 10. Einbindung (macht der Lead)

```cmake
# CMakeLists.txt — BFKA_FX_SOURCES
    src/fx/modules/EqualizerModule.cpp

# CMakeLists.txt — BFKA_HEADERS
    src/fx/modules/EqualizerModule.h

# tests/CMakeLists.txt — target_sources(BFKA_Tests PRIVATE ...)
    EqualizerModuleTests.cpp
    ../src/fx/modules/EqualizerModule.cpp
```

Registry (in `FxFactory.cpp`, beim Start — ersetzt den Platzhalter):

```cpp
#include "fx/modules/EqualizerModule.h"

FxFactory::get().registerType (FxType::equalizer,
                               [] { return std::make_unique<EqualizerModule>(); });
```

Die `#include`-Pfade sind relativ zu `src` (`target_include_directories(... PRIVATE src)`),
also `"fx/modules/EqualizerModule.h"` — im Modul selbst wie in den Tests.
