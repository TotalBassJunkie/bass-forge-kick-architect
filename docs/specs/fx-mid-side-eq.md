# FX-Modul `mseq` — Mid/Side-EQ (Ticket B-62)

**Typ:** `FxType::midSideEq`, XML-Kennung `mseq`, Anzeigename „Mid/Side EQ"
**Code:** `src/fx/modules/MidSideEqModule.h`, `src/fx/modules/MidSideEqModule.cpp`
**Tests:** `tests/MidSideEqModuleTests.cpp` (12 Testfälle)
**Vertrag:** `src/fx/FxModule.h`, `docs/specs/fx-chain-engine.md` (7.1), `docs/specs/fx-equalizer.md`
**Stand:** 2026-11-17 · Zielplattform MSVC 19.5x, C++20, JUCE 8

> **Status:** Dieses Ticket legt nur Code, Tests und diese Spec an und **baut nicht**.
> Alle Zahlen in Abschnitt 10 sind rechnerisch aus den Formeln abgeleitet (plus die
> Schwellen, die die Tests prüfen); sie sind beim ersten Build zu bestätigen.

---

## 1. Zweck und Abgrenzung

Der Mid/Side-EQ teilt die Stereosumme in **Mitte** (`M`) und **Seite** (`S`) und
entzerrt beide Anteile getrennt: zwei feste Shelves je Seite plus bis zu 12 freie
Bänder, die per `ch` einer Seite zugeordnet werden. Dazu kommen **Width** (Seitenanteil
vor der Rückmatrix) und **Solo/Mute je Seite** — das klassische Werkzeug, um ein
Low-End zu monoisieren (`ssolo`/`smute` beim Bass) oder die Mitte freizustellen.

Nicht Teil des Moduls (macht die Kette, siehe `fx-chain-engine.md` 3.1):
`on` (Bypass), `MIX`, `OUT GAIN`, Latenzausgleich. Das Modul liefert immer sein zu
100 % nasses Signal und ist latenzfrei (`latencySamples() == 0`, Minimum-Phase).

Der Modulzustand besteht aus zwei Teilen:

| Teil | Übergabe | Inhalt |
|---|---|---|
| Skalar-Parameter | `setParam()` / `getParam()` (11 Stück, Anzeige-Einheiten) | `midlow … smute` |
| Struktur | `setState()` / `getState()` (`juce::ValueTree`) | bis 12 `<Band>` mit `ch` |

---

## 2. Parameter

`params()` liefert genau diese elf Einträge in dieser Reihenfolge; der Index ist die
Nummer für `setParam()`/`getParam()` und die Zuordnung der Bridge (`fx-param-bridge.md`,
`mseq` → `p0 … p10`).

| # | `id` | Name | min | max | Default | Einheit | Bedeutung |
|---|---|---|---|---|---|---|---|
| 0 | `midlow`   | Mid Low      | −18 | 18  | 0   | dB | Gain der Low-Shelf der Mid-Seite (90 Hz) |
| 1 | `midhigh`  | Mid High     | −18 | 18  | 0   | dB | Gain der High-Shelf der Mid-Seite (8 kHz) |
| 2 | `sidelow`  | Side Low     | −18 | 18  | 0   | dB | Gain der Low-Shelf der Side-Seite (120 Hz) |
| 3 | `sidehigh` | Side High    | −18 | 18  | 0   | dB | Gain der High-Shelf der Side-Seite (6 kHz) |
| 4 | `width`    | Width        | 0   | 200 | 100 | %  | skaliert `S` vor der Rückmatrix |
| 5 | `middyn`   | Mid Dynamic  | 0   | 100 | 0   | %  | dynamische Gegenregelung der Mid-Bänder |
| 6 | `sidedyn`  | Side Dynamic | 0   | 100 | 0   | %  | dynamische Gegenregelung der Side-Bänder |
| 7 | `msolo`    | Mid Solo     | 0   | 1   | 0   | —  | nur Mid hörbar (`S` auf 0) |
| 8 | `ssolo`    | Side Solo    | 0   | 1   | 0   | —  | nur Side hörbar (`M` auf 0) |
| 9 | `mmute`    | Mid Mute     | 0   | 1   | 0   | —  | Mid stumm |
| 10 | `smute`   | Side Mute    | 0   | 1   | 0   | —  | Side stumm |

Werte sind **Anzeige-Einheiten** (dB / % / Schalter), nicht normalisiert. `setParam()`
klemmt auf `[minValue, maxValue]`, verwirft NaN/Inf und ist gegen ungültige Indizes
unempfindlich; `getParam()` liefert den letzten gesetzten Wert (0.0f bei ungültigem
Index).

Das Referenz-XML (`fx-chain-engine.md` 7.2) beobachtet `width = 100`, alle Gains und
`middyn`/`sidedyn`/`msolo`/`ssolo`/`mmute` = 0 sowie `smute` 0/1 — die Bereiche oben
überdecken das vollständig, ein Preset-Import klemmt also nicht.

---

## 3. Signalfluss

```
M[n] = (L[n] + R[n]) / 2
S[n] = (L[n] - R[n]) / 2

M'   = Mid-Kette(M)
S'   = Side-Kette(S)

midKeep  = (1 - mmute) · (1 - ssolo)        0…1
sideKeep = (1 - smute) · (1 - msolo)        0…1
w        = width / 100                      0…2

L'[n] = midKeep · M' + w · sideKeep · S'
R'[n] = midKeep · M' - w · sideKeep · S'
```

`width = 100 %` ⇒ `w = 1` (neutral). Für ein reines Links-Signal (`R = 0`) ist
`M = S = L/2`, und `M + S = L` ist in Gleitkomma **exakt** (Halbieren und Addieren
zweier gleicher Werte) — der Neutraltest in Abschnitt 11 nutzt das.

Die Matrizen laufen **nur**, wenn das Modul nicht im Durchreiche-Zustand ist
(Abschnitt 5.3).

---

## 4. Kaskade

Jede Seite hat ihre eigene Biquad-Kaskade mit **14 Plätzen**
(`maxStagesPerSide = 2 Shelves + 12 Bänder`):

```
Platz 0   L-SHF   Mid: 90 Hz,  Q = 0,7071, g = midlow
                  Side: 120 Hz, Q = 0,7071, g = sidelow
Platz 1   H-SHF   Mid: 8 kHz,  Q = 0,7071, g = midhigh
                  Side: 6 kHz,  Q = 0,7071, g = sidehigh
Platz 2…  die Bänder aus setState() mit ch == 0 (Mid) bzw. ch == 1 (Side)
          in Dokumentreihenfolge (max. 12 je Seite)
```

Ein `<Band ch="0">` landet also **nur** in der Mid-Kette, ein `<Band ch="1">` **nur**
in der Side-Kette. Die Reihenfolge innerhalb der Kette (Shelves vor den Bändern) folgt
dem Tickettext; da alle Stufen lineare, zeitinvariante Filter sind, ändert die
Reihenfolge den Frequenzgang nicht — nur das Einschwingen nach einem Strukturwechsel.

### 4.1 Bandtypen und Formeln

Die Koeffizienten kommen aus `bfka::dsp::designBiquad()` (`src/fx/dsp/Biquad.h`,
RBJ-Cookbook, `double`, nach `a0` normalisiert) — es gibt **keine** zweite
Biquad-Implementierung. `kind` benutzt dieselben Zahlen wie der Equalizer:

| `kind` | Typ | `dsp::BiquadKind` |
|---|---|---|
| 0 | BELL (peaking) | `peaking` |
| 1 | L-SHF (low shelf) | `lowShelf` |
| 2 | H-SHF (high shelf) | `highShelf` |
| 3 | L-CUT (= Hochpass) | `highPass` |
| 4 | H-CUT (= Tiefpass) | `lowPass` |
| 5 | NOTCH | `notch` |

Ausführung: **Transposed Direct Form II** mit `double`-Koeffizienten und
`double`-Zuständen (`dsp::Biquad`).

### 4.2 Bandtypen und Verstärkung

* **BELL/L-SHF/H-SHF** haben eine Bandverstärkung `g`; ein Band mit
  `|g| ≤ 1·10⁻⁶ dB` ist mathematisch die Identität und wird **übersprungen**
  (Stufe inaktiv, Zustände gelöscht). Das ist die Anforderung aus dem Ticket und
  zugleich die Voraussetzung für den bitidentischen Neutralzustand.
* **L-CUT/H-CUT/NOTCH** sind nie „flach"; `g` und `dyn` werden bei ihnen ignoriert
  (sie haben keine Bandverstärkung).

---

## 5. Zustandsformat (`getState()` / `setState()`)

`getState()` liefert einen `juce::ValueTree` vom Typ `"Module"` mit einem
`"Band"`-Kind je Band — exakt das Referenzformat:

```xml
<Module>
  <Band kind="1" f="71.64"  q="0.676" g="15.70" dyn="0.0"  ch="0"/>
  <Band kind="0" f="440.0"  q="1.2"   g="-3.5"  dyn="50.0" ch="1"/>
  <Band kind="4" f="9000.0" q="0.7"   g="0.0"   dyn="0.0"  ch="1"/>
</Module>
```

| Attribut | Bedeutung | Klemmung beim Lesen | Default bei fehlendem Attribut |
|---|---|---|---|
| `kind` | 0…5 (siehe 4.1) | 0 … 5 | 0 (BELL) |
| `f` | Frequenz in Hz | 10 … 20000 | 1000 |
| `q` | Güte | 0,1 … 10 | 0,9 |
| `g` | Verstärkung in dB | −32 … +32 | 0 |
| `dyn` | Band-Dynamik in % | 0 … 100 | 0 |
| `ch` | 0 = Mid, 1 = Side | 0 … 1 | 0 (Mid) |

Regeln (identisch zum Equalizer):

* Es werden **nur** `"Band"`-Kinder gelesen (Typvergleich ohne Groß-/Kleinschreibung);
  fremde Kinder (`<WNode>`, `<Pt>`, …) werden ignoriert.
* Zahlen dürfen als Zahl **oder als String** im Baum stehen (ValueTrees aus XML tragen
  Zahlen oft als String); beides wird gelesen.
* Unbekannte Attribute werden nicht gelesen und beim Zurückschreiben nicht erhalten
  (der Zustand ist ein Modell, kein Rohdokument).
* Mehr als 12 `"Band"`-Kinder: die **ersten 12** werden übernommen, der Rest verworfen.
* `getState()` schreibt die Werte unverändert (keine Rundung) — der Round-Trip ist
  verlustfrei (`double`-Werte, Toleranz der Tests 10⁻⁶).
* Die Shelves und alle Skalare erscheinen **nicht** als `<Band>`; `getState()` liefert
  bei leerer Bandliste null Kinder.

### 5.1 Übergabe an den Audio-Thread

Wie beim Equalizer: ein **lock-freier Snapshot**. `setState()` schreibt jedes Feld in
ein eigenes `std::atomic`-Feld (`sharedBands[12]`), dann die Anzahl und zuletzt
`sharedVersion.fetch_add(1, release)`. Der Audio-Thread liest die Version (`acquire`),
die Felder und die Version erneut; hat sich die Version dazwischen geändert, wird der
Snapshot verworfen und im nächsten Block erneut gelesen. `prepare()`/`reset()`
übernehmen ihn sofort (`force`).

### 5.2 Neutralzustand (bitidentisches Durchreichen)

Sind **keine** Stufen aktiv, steht `width` exakt auf 100 % und sind alle vier
Solo/Mute-Schalter 0, kehrt `process()` **vor** der Matrix zurück: der Puffer wird
nicht angefasst, es gibt also weder Filter- noch Matrix-Rundung. Das ist der
Bypass-Nulltest der Kette (−120 dBFS).

### 5.3 Solo/Mute-Matrix

```
midKeep  = (1 − mmute) · (1 − ssolo)
sideKeep = (1 − smute) · (1 − msolo)
```

| Einstellung | `midKeep` | `sideKeep` | Ergebnis bei `L = 1`, `R = 0` (`M = S = 0,5`) |
|---|---|---|---|
| Default | 1 | 1 | `L = 1`, `R = 0` |
| `msolo` | 1 | 0 | `L = 0,5`, `R = 0,5` (Monosumme) |
| `ssolo` | 0 | 1 | `L = +0,5`, `R = −0,5` (nur Seite) |
| `mmute` | 0 | 1 | `L = +0,5`, `R = −0,5` |
| `smute` | 1 | 0 | `L = 0,5`, `R = 0,5` (Mono) |
| `msolo` + `mmute` | 0 | 0 | `L = R = 0` |
| `ssolo` + `smute` | 0 | 0 | `L = R = 0` |

Das eigene Solo wirkt also über die **andere** Seite: `msolo` setzt `S` auf 0 (nicht
`M` auf 1), damit „nur Mid" wirklich die Monosumme ist. Solo **und** Mute derselben
Seite ergeben deshalb 0 auf beiden Seiten (Anforderung aus dem Ticket).

Die Schalter werden wie alle Parameter mit 20 ms geglättet und **rampen** daher über
~20 ms (De-Zipper, siehe O-M4); `prepare()`/`reset()` setzen sie sofort auf den
Zielwert, sodass es nach dem Vorbereiten keine Einlauf-Rampe gibt.

---

## 6. Dynamische Gegenregelung (`middyn`, `sidedyn`, `dyn`)

Wirksam für **BELL/L-SHF/H-SHF** (nur dort gibt es eine Bandverstärkung). Der
wirksame Prozentsatz je Band ist

```
dynPercent = clamp ( max (Seiten-Regler, Band-`dyn`), 0 … 100 )
dynAmount  = dynPercent / 100
```

Seiten-Regler ist `middyn` für die Mid-Kette und `sidedyn` für die Side-Kette; das
Band-Attribut `dyn` wirkt als bandindividueller Mindestwert (Abschnitt 12, O-M3).

1. **Hüllkurve des Ausgangs der jeweiligen Seite** (ein Follower je Seite):

   ```
   attack  = 1 − exp(−1/(10 ms · Fs))
   release = 1 − exp(−1/(200 ms · Fs))

   env += (|out| − env) · (|out| > env ? attack : release)
   ```

   Der Follower (`dsp::EnvelopeFollower`) läuft auf dem Ausgang der Seite **nach**
   der Kaskade; der Pegelfaktor des laufenden Samples stammt aus dem Stand des
   vorigen Samples (ein Sample Verzögerung). Damit gibt es keinen rückgekoppelten
   Kreis ohne Verzögerung und keine Aufschaukelung.

2. **Pegelfaktor** gegen die Schwelle −6 dBFS (linear 0,5):

   ```
   level = clamp ((env − 0,5) / 0,5, 0 … 1)
   ```

3. **Gegenregelung als Kreuzblende** zwischen Band-Eingang `x` und Band-Ausgang `y`,
   pro Sample stetig (kein Zipper, kein Blocksprung):

   ```
   k    = 1 − level · dynAmount · (1 − fadeMin)
   out  = x + k·(y − x)                (k auf 0…1 geklemmt)
   ```

   `fadeMin` ist der kleinstmögliche Faktor und wird wie im Equalizer aus dem
   Band-Gain gelöst, damit die Gegenregelung bei Vollausschlag genau
   `min (6 dB, |g|)` wegnimmt — also höchstens bis auf 0 dB und höchstens 6 dB:

   ```
   G       = 10^(g/20)
   G_ziel  = 10^((g − sign(g)·min(6, |g|))/20)
   fadeMin = clamp ((G_ziel − 1) / (G − 1), 0 … 1)
   ```

   Beispiele: `g = +12 dB` → `fadeMin = 0,331` (Band fällt auf +6 dB),
   `g = +6 dB` → `fadeMin = 0` (Band ganz auf 0 dB),
   `g = −12 dB` → `fadeMin = 0,666` (Absenkung wird auf −6 dB zurückgenommen).

4. Bei `dynPercent = 0` ist `dynAmount = 0`, die Stufe ist dynamisch inaktiv, und der
   Follower wird nicht nachgeführt (keine Rechenzeit, kein Zustand).

---

## 7. Glättung, Koeffizienten, Stabilität

* **Glättung:** alle elf Parameter sowie `f`, `q`, `g` und `dyn` der Bänder laufen mit
  einer Exponentialglättung von **20 ms** auf ihren Zielwert und springen exakt auf
  das Ziel, sobald der Rest unter 10⁻⁶ liegt. Die Glättung läuft **pro Block**
  (`1 − exp(−blockLen/(tau·Fs))`), nicht pro Sample.
* Bei den Nutzerbändern ist ein **Strukturwechsel** (Anzahl, `kind` oder `ch` an einer
  Position) keine Ziehbewegung: dort werden die Zielwerte übernommen und der
  Filterzustand gelöscht (Klickmöglichkeit, O-M6).
* **Koeffizienten** werden höchstens **einmal pro Block** gesetzt, und nur, wenn sich
  ein wirksamer Wert (kind/f/q/g/dynAmount) tatsächlich geändert hat.
* **Stabilitätsgrenzen:** `q ∈ [0,1; 10]`, `f ∈ [10 Hz; min(20 kHz, 0,49·Fs)]`,
  `g ∈ [−32; +32] dB` — geklemmt in `setState()` bzw. in `dsp::designBiquad()`.
  Nicht-endliche Koeffizienten schalten die Stufe als Identität ab; kein NaN kann in
  den Puffer gelangen.
* Unbekannte `kind`-Werte (nur durch direkte Manipulation des Baums möglich) machen
  die Stufe inaktiv.
* **Keine Allokation, kein Logging, keine Sperre** in `process()`: alle Puffer sind
  Mitglieder fester Größe (`sharedBands`, `pendingBands`, `sideChains`).
* **Kanäle:** verarbeitet werden Kanal 0/1 als L/R; liegt nur **ein** Kanal vor, gilt
  `M = x`, `S = 0` (Ausgang = Mid-Kette). Kanäle ab 2 bleiben unberührt
  (`maxChannels = 8`).

---

## 8. Parameter- und Zustands-Threading

| Daten | Weg |
|---|---|
| 11 Parameter | `std::atomic<float> paramTargets[11]`, einmal pro Block gelesen, im Audio-Thread geglättet |
| Bandliste | lock-freier Snapshot: atomare Einzelfelder je Band + Versionzähler (`release`/`acquire`, Re-Check nach dem Lesen) |
| Audio-Zustand | nur Audio-Thread (`sideChains` mit Zielwerten, geglätteten Werten, Biquad-Zuständen, Hüllkurven) |
| Diagnose | `activeStageCount()` über `std::atomic<int>` (nur Tests/Anzeige) |

`prepare()`/`reset()` setzen Glättung und Bandwerte auf die aktuellen Ziele und
übernehmen den Snapshot sofort — es gibt weder eine Einlauf-Rampe noch eine
Verzögerung von einem Block.

---

## 9. Latenz

**0 Samples.** Alle Stufen sind Minimum-Phase-Biquads ohne Lookahead; es gibt keine
Verzögerungsleitung. `latencySamples()` liefert 0, die Kette muss nichts ausgleichen.

---

## 10. Rechnerisch erwartete Werte (beim Build zu bestätigen)

| Fall | Erwartung |
|---|---|
| BELL 1 kHz, `g = +6 dB`, `middyn = 0` | +6,02 dB bei 1 kHz |
| Mid-Low-Shelf 90 Hz, `g = +12 dB`, Messung bei 40 Hz | ≈ +11,4 dB (analoges Prototyp-Modell: 3,72 → 11,42 dB) |
| Side-Low-Shelf 120 Hz, `g = +12 dB`, Messung bei 40 Hz | ≈ +11,8 dB (3,89 → 11,81 dB) |
| `width = 200 %`, reines Links-Signal | `L = 1,5·L₀`, `R = −0,5·L₀` |
| `middyn = 100`, Band +6 dB bei 1 kHz, Eingang 0,7 | stationär ≈ +2 dB (Gegenregelung auf 0 dB bei Vollausschlag) |
| Werkszustand, Sinus + Impuls, 80 Blöcke | `max|out − in| = 0` (Puffer wird nicht angefasst) |
| Solo/Mute-Matrix (Tabelle in 5.3) | exakt die dortigen Werte (±10⁻⁶) |

---

## 11. Tests (`tests/MidSideEqModuleTests.cpp`, Catch2 v3)

Messverfahren: Sinus einschwingen lassen, Pegelgewinn als
`20·log10(RMS_out / RMS_in)` im eingeschwungenen Fenster (zweite Hälfte des Signals).
Bei 1 kHz und 0,5 s bzw. 0,6 s Länge ist das Fenster ein ganzzahliges Vielfaches der
Periodendauer (250 bzw. 300 Perioden), bei 40 Hz sind es 12 Perioden.

| Testfall | Prüft | Schwelle |
|---|---|---|
| Parameterliste | `params().size() == 11`, `params()[0].id == "midlow"`, `params()[10].id == "smute"`, alle IDs in Reihenfolge, Bereiche, Defaults, Einheiten, Werkswerte, `type()`, `displayName()`, `latencySamples() == 0` | exakt |
| Klemmen | `setParam(0, 99) → 18`, `setParam(0, −99) → −18`, `setParam(4, −5) → 0`, `setParam(4, 999) → 200`, `setParam(5, 250) → 100`, `setParam(9, 5) → 1`, ungültige Indizes folgenlos | exakt |
| Neutraltest | Werkszustand, Sinus + Impuls, 80 Blöcke à 128 | `max|out − in| < 10⁻⁷`, `activeStageCount() == 0` |
| M/S-Identität | `width = 100`, reines Links-Signal: Ausgang = Eingang; zusätzlich `width = 200`: `L = 1,5·x`, `R = −0,5·x` | 10⁻⁷ bzw. 10⁻⁶ |
| Mid-Solo | `L = 1`, `R = 0`, `msolo = 1` | beide Kanäle 0,5 (±10⁻⁶) |
| Side-Solo | `L = 1`, `R = 0`, `ssolo = 1` | `L = +0,5`, `R = −0,5` (±10⁻⁶) |
| Solo/Mute-Matrix | `mmute`, `smute`, `msolo`+`mmute`, `ssolo`+`smute` | Tabelle 5.3 (±10⁻⁶) |
| Band-seitig | `<Band ch="1" kind="0" f="1000" g="+6">`: Mitten-Signal bleibt unverändert (0 dB), Seiten-Signal +6 dB; umgekehrt für `ch="0"` | 0 ± 0,05 dB, +6 dB ± 1,5 |
| Shelves | `midlow = 12` bzw. `sidelow = 12` bei 40 Hz: Anhebung auf der eigenen Seite, 0 dB auf der anderen | > 7 dB und < 12,5 dB, sonst 0 ± 0,05 dB |
| `middyn` | lautes Mitten-Signal (0,7), Band +6 dB: `middyn = 100` gegen `middyn = 0`; Kontrolle mit `sidedyn = 100` | statisch ≈ +6 dB; dynamisch < statisch − 2 dB, < 3 dB und > −1 dB; Kontrolle = statisch ± 0,1 dB |
| `<Band>`-Roundtrip | `setState` mit drei Bändern (ch 0/1 gemischt) → `getState()` liefert dieselben Werte | 10⁻⁶ |
| 12er-Grenze | 20 Bänder rein → 12 raus, Reihenfolge erhalten | exakt |
| Fremde Kinder/Attribute | `<WNode>` ignoriert, `colour` ignoriert, `g` als String gelesen, `ch` erhalten | exakt |

---

## 12. Abweichungen und offene Punkte

| # | Punkt | Warum wichtig | Vorschlag |
|---|---|---|---|
| **O-M1** | **`kind` 1…5 ist für `mseq` nicht belegt.** Die Zahlen sind aus dem Equalizer übernommen (dort ist nur `kind = 0 → BELL` sowie 1/2 als „Low-/High-Band" aus den Referenzdaten belegt, siehe `fx-equalizer.md` O1). | Preset-Kompatibilität: eine falsche Zahl macht aus einem Shelf einen Hochpass. | Am Original verifizieren (Preset mit `kind ≥ 3` laden und Frequenzgang messen). |
| **O-M2** | **Shelf-Eckfrequenzen und Q sind eigene Setzung:** Mid 90 Hz / 8 kHz, Side 120 Hz / 6 kHz, Q = 0,7071. | Klang der vier Regler; die Referenz-Presets variieren die Gains nie. | Hörvergleich/ Messung am Original; die Werte stehen an einer Stelle (`kShelfLowHz`, `kShelfHighHz`, `kShelfQ`). |
| **O-M3** | **Band-`dyn` wird mit dem Seiten-Regler kombiniert** (`max (sideDyn, bandDyn)`), statt wie im Equalizer ausschließlich bandweise zu wirken. | Das Referenzformat hat beide Ebenen; ohne Kombination wäre das Band-Attribut wirkungslos. | Alternative Lesarten: nur Seiten-Regler (dann `band.dyn` ignorieren) oder Summe. Am Original prüfen (Band mit `dyn > 0` bei `middyn = 0`). |
| **O-M4** | **Solo/Mute sind geglättet** (20 ms Rampe statt hartem Umschalten). | De-Zipper gegen Klicks; das Original schaltet vermutlich hart. | So belassen; bei Bedarf die vier Schalter von der Glättung ausnehmen (dann klickt es beim Umschalten). |
| **O-M5** | **Ein gemeinsamer Follower je Seite** statt eines Detektors je Band; die Hüllkurve stammt vom **Seiten-Ausgang** (ein Sample Verzögerung). | Mehrere dynamische Bänder einer Seite reagieren damit gleichsinnig. | Falls das Original bandweise Detektoren nutzt: `StageRuntime` um einen eigenen Follower erweitern (Speicher ist da). |
| **O-M6** | Typ-/Kanalwechsel eines Bands löscht den Filterzustand → möglicher Klick. | Hörbarkeit beim Umschalten im Band-Menü. | Wie im Equalizer (O5): bei Bedarf kurze Gain-Rampe auf 0 dB, Typwechsel, zurück. |
| **O-M7** | Gegenregelungs-Kennlinie (Schwelle −6 dBFS, Attack/Release, max. 6 dB) ist eigene Setzung, wie im Equalizer (O4). | „DYN" klingt nur bei passender Zeit wie das Original. | Am Original messen und hier ersetzen. |
| **O-M8** | CPU-Budget (< 3 % je Modul) ist hier nicht gemessen — dieses Ticket baut nicht. | Abnahmekriterium aus dem Masterplan. | Im Host mit 10 Instanzen bei 48 kHz/128 nachmessen (zwei 14-stufige Kaskaden). |
| **O-M9** | Registrierung in der Registry und Eintrag in CMake sind Aufgabe des Leads; dieses Ticket ändert weder `FxFactory.cpp` noch `CMakeLists.txt`. | Ohne Eintrag ist das Modul nicht erreichbar. | Siehe Abschnitt 13. |

---

## 13. Einbindung (macht der Lead)

```cmake
# CMakeLists.txt — BFKA_FX_SOURCES
    src/fx/modules/MidSideEqModule.cpp

# CMakeLists.txt — BFKA_HEADERS
    src/fx/modules/MidSideEqModule.h

# tests/CMakeLists.txt — target_sources(BFKA_Tests PRIVATE ...)
    MidSideEqModuleTests.cpp
    ../src/fx/modules/MidSideEqModule.cpp
```

Registry (in `FxFactory.cpp`, ersetzt den `PassThroughModule`-Platzhalter für `mseq`):

```cpp
#include "fx/modules/MidSideEqModule.h"

FxFactory::get().registerType (FxType::midSideEq,
                               [] { return std::make_unique<MidSideEqModule>(); });
```

Die `#include`-Pfade sind relativ zu `src` (`target_include_directories(... PRIVATE src)`),
also `"fx/modules/MidSideEqModule.h"` — im Modul selbst wie in den Tests.

---

## Nachtrag 2026-11-17 (B-92b)

UI-Editor src/ui/editors/MidSideEqView.{h,cpp}: Frequenz-/Gain-Plot
(20 Hz…20 kHz log, ±18 dB) mit Knoten je Band (Mid ch = 0 blau, Side ch = 1
gelb), Ziehen ändert /g, Mausrad q, Rechtsklick den kind
(0 Bell … 5 Notch), Doppelklick auf freie Fläche legt ein Band an, Strg-Klick
löscht. Darunter WIDTH-Slider und die vier Schalter MID SOLO, SIDE SOLO,
MID MUTE, SIDE MUTE (Parameter width, msolo, ssolo, mmute, smute).
Geschrieben wird über setState() + writeBackModuleStates(); dyn bleibt
unverändert erhalten.