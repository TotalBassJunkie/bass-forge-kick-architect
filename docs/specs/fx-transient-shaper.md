# FX-Modul `trans` — Transient Shaper (Ticket B-65)

**Typ:** `FxType::transientShaper`, XML-Kennung `trans`, Anzeigename „Transient Shaper"
**Code:** `src/fx/modules/TransientShaperModule.h`, `src/fx/modules/TransientShaperModule.cpp`
**Tests:** `tests/TransientShaperModuleTests.cpp` (10 Testfälle, 80 769 Assertions)
**Stand:** 2026-11-17 — mit MSVC 19.51 (`/std:c++20 /W4 /permissive-`) warnungsfrei
übersetzt, Testsuite grün.

---

## 1. Zweck und Abgrenzung

Der Transient Shaper formt die **Einschwingphase** eines Kicks, ohne den Dauerton
anzutasten:

| Regler | Handbuch | Wirkung |
|---|---|---|
| `ATTACK` | „wie scharf der Einschlag ist" | hebt den Einschlag an (mehr Snap) |
| `DUCK` | „der Einbruch nach dem Einschlag" | senkt den Pegel nach dem Einschlag (zähmt Dröhnen) |
| `SPEED` | „wie schnell er sich erholt" | Erholzeit des schnellen Hüllkurvenfolgers |
| `AMOUNT` | Grundpegel/Wirkungsgrad der Stufe | skaliert die Gesamtwirkung, 0 = Durchreichen |

**Nicht** im Modul: `MIX` und `OUT GAIN`. Beide gehören laut Kettenvertrag
(`docs/specs/fx-chain-engine.md`, Abschnitt 3) zur Kette. Das Modul liefert
ausschließlich sein zu 100 % nasses Signal.

**Latenz: 0 Samples.** Das Modul ist Minimum-Phase und hat keinen Lookahead
(Konsequenz siehe Abschnitt 12, O-T2).

---

## 2. Parameter

`params()` liefert genau diese vier Einträge in dieser Reihenfolge; der Index ist die
Nummer für `setParam()`/`getParam()` und die spätere Zuordnung zu `fx<N>_p<M>`:

| # | `id` | Name | min | max | Default | Einheit | Bedeutung |
|---|---|---|---|---|---|---|---|
| 0 | `amount` | Amount | 0 | 100 | **100** | % | Wirkungsgrad der Stufe; 0 = bitgenaues Durchreichen |
| 1 | `attack` | Attack | 0 | 100 | **0** | % | Anhebung des Einschlags |
| 2 | `duck` | Duck | 0 | 100 | **0** | % | Absenkung nach dem Einschlag |
| 3 | `speed` | Speed | 16 | 200 | **48** | ms | Release-Zeit des schnellen Followers |

Die Bereiche überdecken alle aus den Referenzdaten beobachteten Werte
(`amount` 0…100, `attack` 0…100, `duck` 0…63, `speed` 47…180; siehe
`docs/specs/fx-chain-engine.md`, 7.2) — ein Preset-Import klemmt also nicht.
`setParam()` klemmt zusätzlich selbst auf `[minValue, maxValue]` und verwirft
NaN/Inf.

---

## 3. Signalfluss

```
det[n]      = max ( |L[n]|, |R[n]| )                 gemeinsame Erkennung
fast[n]     = Follower (Attack 0,5 ms, Release = SPEED)
slow[n]     = Follower (Attack 30 ms,  Release 300 ms)
diff[n]     = (fast − slow) / (fast + slow)          −1…+1, 0 bei Stille
attackShare = max (0, diff)                          0…1  „Einschlag"
sustainShare= (1 − diff) / 2                         0…1  „Dauer"
effect      = attack/100 · kA · attackShare
            − duck/100   · kD · sustainShare
gain[n]     = clamp (1 + amount/100 · effect, 0, 4)
out[ch][n]  = softLimit ( in[ch][n] · gain[n] )
```

Alle Kanäle bekommen **dieselbe** Verstärkung (Abschnitt 8).

---

## 4. Hüllkurvenfolger

Beide Follower sind One-Pole-Glieder (exponentielle Glättung) mit getrennten
Attack-/Release-Zeitkonstanten:

```
a_attack  = 1 − exp ( −1 / (τ_attack  · fs) )
a_release = 1 − exp ( −1 / (τ_release · fs) )
env      += ( det − env ) · ( det > env ? a_attack : a_release )
```

`env > det` bedeutet „Signal fällt" → Release; `env < det` bedeutet „Signal steigt"
→ Attack. Der Zustand ist ein einzelner `float` je Follower.

### 4.1 Konstanten

| Konstante | Wert | Rolle |
|---|---|---|
| `kFastAttackMs` | 0,5 ms | Attack des schnellen Followers |
| `kSlowAttackMs` | 30 ms | Attack des langsamen Followers |
| `kSlowReleaseMs` | 300 ms | Release des langsamen Followers |
| `kSpeedMinMs` / `kSpeedMaxMs` | 16 / 200 ms | Bereich von `speed`; **Release des schnellen Followers** |

Damit ist der schnelle Follower immer schneller als der langsame (Attack 0,5 < 30 ms,
Release 16…200 < 300 ms) — die Differenz ist also immer ein „Einschlag-Signal" und
kein Artefakt der Zeitkonstanten.

### 4.2 Koeffizienten bei 48 kHz (gemessen)

| Zeitkonstante | Koeffizient `1 − exp(−1/(τ·fs))` |
|---|---|
| 0,5 ms (fast attack) | 0,040811 |
| 8 ms (Parameterglättung) | 0,002601 |
| 16 ms (speed min) | 0,001301 |
| 30 ms (slow attack) | 0,000694 |
| 48 ms (speed default) | 0,000434 |
| 200 ms (speed max) | 0,000104 |
| 300 ms (slow release) | 0,000069 |

---

## 5. Verstärkungsgesetz

### 5.1 Transienten-Anteil

```
diff = (fast − slow) / (fast + slow)          für fast + slow > 10⁻¹²
diff = 0                                      sonst (Stille)
```

Die Division durch die Summe normiert auf **−1…+1** und macht das Maß
pegelunabhängig: ein leiser Kick wird genauso geformt wie ein lauter. Zugleich ist
`diff` bei Stille exakt 0 (kein 0/0, kein Rauschen im Nichts).

* `diff ≈ +1` — reiner Einschlag (schneller Follower weit über dem langsamen)
* `diff ≈ 0` — Dauer-Signal (beide Follower gleich) **und** Stille
* `diff < 0` — Nachschwingen/Abklingen (der langsame Follower hängt nach)

### 5.2 Anteile und Verstärkung

```
attackShare  = max (0, diff)      // 0…1
sustainShare = (1 − diff) / 2     // 0…1

effect = attack/100 · kA · attackShare − duck/100 · kD · sustainShare
gain   = clamp (1 + amount/100 · effect, 0, 4)
```

| Konstante | Wert | Wirkung |
|---|---|---|
| `kAttackGain` (kA) | 1,0 | bei `attack = 100` und reinem Einschlag: `gain = 2,0` (**+6 dB**) |
| `kDuckGain` (kD) | 0,75 | bei `duck = 100` und reinem Dauer-Anteil: `gain = 0,25` (**−12 dB**) |

Eigenschaften des Gesetzes:

1. **`amount = 0` → `gain = 1` exakt** (und zusätzlich der Sonderpfad aus 10.1).
2. **`amount` skaliert die Wirkung, nicht den Pegel**: bei Default
   (`attack = duck = 0`) ist `effect = 0`, also `gain = 1,0` — das Modul ist dann ein
   bitgenaues Durchreichen (außer für Signale > 0 dBFS, siehe 7).
3. **Dauer-Signale werden nicht verbogen**: bei einem eingeschwungenen Sinus ist
   `fast ≈ slow`, also `attackShare ≈ 0`; ATTACK hat dort keine Wirkung
   (Messung: +0,41 dB, Abschnitt 11).
4. **DUCK wirkt auf den Dauer-Anteil**: `sustainShare` ist im abklingenden Bereich nach
   dem Einschlag nahe 1 (der langsame Follower hängt nach) und bei Stille 0. Ein
   Dauer-Sinus landet bei `sustainShare ≈ 0,5` — DUCK senkt ihn deshalb gleichmäßig,
   aber nicht voll (Messung: −3,83 dB bei `duck = 100`).
5. **Grenzen**: `gain ∈ [0, 4]`; der Maximalwert wird konstruktiv nie erreicht
   (`1 + 1·1,0·1 = 2,0`), die Klemmung ist reine Absicherung.

### 5.3 Verhältnis zur Vertragsformel (bewusste Präzisierung)

Der Tickettext nennt

```
gain = 1 + attack/100 · kA · diff − duck/100 · kD · (1 − diff)
```

Diese Form ist mit `diff ∈ [−1, +1]` umgesetzt als

* `attack`-Term identisch (`diff ≥ 0` geclippt, weil negativer „Einschlag" keiner ist),
* `duck`-Term `(1 − diff)/2` statt `(1 − diff)`:
  * der Faktor **½** normiert den Ausdruck auf 0…1 (sonst wäre die Duck-Wirkung
    doppelt so groß wie der eingestellte Prozentwert),
  * die **Stille-Normierung** (`diff = 0` bei `fast + slow = 0`) verhindert, dass DUCK
    im Nichts mit voller Stärke „auf Vorrat" arbeitet — sonst würde jeder leise
    Abschnitt um bis zu −7,5 dB abgesenkt, weil beide Hüllkurven 0 sind.
* `amount` skaliert den gesamten Effekt-Term (Interpretation von „Grundpegel/
  Wirkungsgrad der Stufe", siehe O-T4).

---

## 6. SPEED

`speed` (16…200 ms) ist die **Release-Zeit des schnellen Followers**. Der Koeffizient
wird aus dem geglätteten Wert berechnet, aber nur neu, wenn sich der Wert um mehr als
`kSpeedRecalcMs = 0,05 ms` geändert hat (Hysterese) — dadurch kostet SPEED ein
`std::exp` pro Block statt pro Sample. Zwischen zwei Blöcken ist der Koeffizient
konstant; da er nur die Hüllkurven-Zeitkonstante und nicht den Ausgangspegel direkt
setzt, ist die Treppe (ein Block = 2,7 ms bei 48 kHz/128) unhörbar.

Wirkung: kleines `speed` = schnelle Erholung = kurzer, knackiger Effekt;
großes `speed` = der schnelle Follower bleibt länger „oben", der Effekt hält länger an
(Messung: Abschnitt 11.4).

---

## 7. Weiche Sättigung (Ausgangsbegrenzung)

```
|y| ≤ 1         →  y                                  (bitgenau)
|y| > 1         →  sign(y) · ( 1 + tanh(k·(|y| − 1)) / k ),  k = 4
nicht endlich   →  0
```

Eigenschaften:

* **Knie bei exakt 0 dBFS** (`kSoftKneeLevel = 1,0`), darunter wird nichts angefasst.
* **C¹-stetig**: Wert 1, Steigung 1 auf beiden Seiten des Knies (links exakt 1,
  rechts `sech²(0) = 1`) — kein Knick, kein Klick.
* **Asymptote** `kSoftCeiling = 1 + 1/k = 1,25` (≈ +1,94 dBFS): der Ausgang kann nicht
  über +2 dBFS laufen, egal wie heiß ATTACK boostet.
* **Monoton** und symmetrisch; messbare Werte: 1,0 → 1,000000; 1,25 → 1,190399;
  1,5 → 1,241007; 2,0 → 1,249832; 4,0 → 1,250000.

Bewusste Entscheidung: bei `amount = 0` wird der Puffer **komplett** nicht angefasst
(also auch nicht limitiert), weil der Vertrag dort bitgenaues Durchreichen verlangt.
Der Nulltest der Kette (−120 dBFS) ist damit erfüllt; ein über 0 dBFS heißes Signal
läuft in diesem einen Fall ungehindert durch (im Rack fängt es der Finalizer).

---

## 8. Stereo — gemeinsame Hüllkurve

Der Detektor ist das **Maximum der Kanalbeträge**. Daraus folgt:

* Identisches Signal auf L/R → identische Verstärkung → Ausgang L/R **bitgleich**
  (Messung: `max|L − R| = 0`).
* Ein Einschlag auf nur einem Kanal hebt über die gemeinsame Hüllkurve **beide**
  Kanäle an — die Stereobasis driftet nicht auseinander
  (Messung: linker Dauerton +2,27 dB, obwohl der Burst nur rechts sitzt).
* Gegenphasige Signale (L = −R) löschen die Erkennung nicht aus (kein Summen-/
  Mitten-Detektor, sondern Betrags-Maximum).

---

## 9. Parameterglättung

Alle vier Parameter werden im Audio-Thread mit einem One-Pole-Glied geglättet
(`kParamSmoothMs = 8 ms`, Koeffizient 0,002601 bei 48 kHz; ≈ 63 % nach 8 ms).
Damit sind Regler-Sprünge und Host-Automation de-zippert; die geforderten ≥ 5 ms sind
eingehalten.

* Der Message-Thread schreibt nur die **Ziele** in `std::atomic<float>`
  (`std::memory_order_relaxed`), der Audio-Thread liest sie einmal pro Block und
  glättet sampleweise. Keine Sperre, kein `std::atomic` pro Sample.
* `prepare()`/`reset()` setzen die geglätteten Werte auf die Ziele — es gibt keine
  Einlauf-Rampe, und ein Test mit `amount = 0` ist ab dem ersten Sample bitgenau.

---

## 10. Grenzfälle

| Fall | Verhalten |
|---|---|
| **`amount = 0`** | Sonderpfad: Puffer bleibt unberührt (bitgenau), Hüllkurven werden auf 0 gesetzt, damit ein erneutes Aufdrehen nicht mit alter Energie einrastet. Auch bei `attack = duck = 100` und dreifach heißem Signal ist `max|out − in| = 0` (gemessen). |
| **Stille** | `det = 0`, Hüllkurven 0, `diff = 0`, `gain = 1` → Ausgang 0. Kein Denormal-Absturz (`kDenormalGuard = 1e-15`, Flush auf 0). |
| **DC** | Konstanter Gleichanteil: beide Follower gehen auf denselben Wert, `diff → 0`, ATTACK wirkungslos (Ausgang 0,500318 bei Eingang 0,5), keine Modulation (< 0,03 dB Restwelligkeit). Mit `duck = 100` wird DC gleichmäßig auf 0,312619 (−4,08 dB) abgesenkt — gewollt (DUCK senkt den Dauer-Anteil). |
| **Sehr kurze Blöcke** | `numSamples = 0` → sofortiges `return`; 1 Sample und 7 Samples funktionieren (endlich, begrenzt). |
| **Kein `prepare()`** | Koeffizienten stehen auf „instant", die Glättung ist aus → stabil, endlich, begrenzt (kein Absturz). Die Kette ruft immer `prepare()`. |
| **NaN/Inf im Eingang** | Detektor-Hüllkurven werden über `sanitizeEnvelope()` verworfen (→ 0), der Ausgang über `softLimit()` (→ 0). Das Modul erholt sich selbst, statt dauerhaft zu verstummen. |
| **`setParam()` vor `prepare()`** | erlaubt; die Ziele werden beim nächsten `prepare()`/`reset()` in die Glättung übernommen. |

---

## 11. Messwerte

Alle Werte mit der echten Implementierung gemessen (48 kHz, Block 128, Testsignale wie
in `tests/TransientShaperModuleTests.cpp`).

### 11.1 `amount = 0`

| Signal | `max|out − in|` |
|---|---|
| Kick, `attack = duck = 100`, `speed = 16` | 0,000e+00 |
| dasselbe Signal × 3 (heiß, Spitze 2,1) | 0,000e+00 |

### 11.2 ATTACK (Impuls 0,4, danach Stille)

| Einstellung | Spitzenwert |
|---|---|
| `attack = 0` | 0,400000 |
| `attack = 100` | 0,786619 (**Faktor 1,967**) |

### 11.3 DUCK (Kick: 3 Samples 0,7 + Körper 0,35·e^(−t/120 ms)·sin 55 Hz)

| Messung | `duck = 0` | `duck = 100` | Differenz |
|---|---|---|---|
| Peak (Einschlag) | 0,705038 | 0,695836 | −0,13 dB |
| RMS 80…200 ms | 0,082433 (−21,68 dB) | 0,049235 (−26,15 dB) | **−4,48 dB** |

### 11.4 SPEED

Hüllkurve 100 ms nach einem Einschlag von 0,7 (Diagnose `currentFastEnvelope()`):

| `speed` | `fast` @ 100 ms | `slow` @ 100 ms |
|---|---|---|
| 16 ms | 1,594e-04 | 1,044e-03 |
| 48 ms | 1,026e-02 | 1,044e-03 |
| 200 ms | 4,990e-02 | 1,044e-03 |

Faktor 16 ms ↔ 200 ms: **313×**. Hörbarer Pegelverlauf (Kick, `duck = 100`,
Fenster 150…300 ms):

| `speed` | RMS |
|---|---|
| 16 ms | −33,39 dB |
| 200 ms | −30,61 dB (**+2,78 dB**) |

### 11.5 Dauer-Sinus (`attack = 100`, `duck = 0`, Pegel im Fenster 0,5…1,0 s)

| Frequenz | Eingang | Ausgang | Abweichung |
|---|---|---|---|
| 60 Hz | −9,031 dB | −8,622 dB | **+0,408 dB** |
| 100 Hz | −9,031 dB | −8,612 dB | **+0,419 dB** |
| 1000 Hz | −9,031 dB | −8,608 dB | **+0,423 dB** |

Mit `duck = 100` (100 Hz): −3,83 dB gleichmäßige Absenkung, Streuung der 20-ms-Fenster
über 0,4…0,9 s: **0,006 dB** (kein Pumpen, keine Modulation).
Mit Default-Parametern (`attack = duck = 0`): `max|out − in| = 0` (bitgenau).

### 11.6 Stereo

| Messung | Wert |
|---|---|
| `max|L − R|` bei identischem L/R-Eingang | 0,000e+00 |
| Linkspegel mit Burst nur rechts (100…110 ms) | −25,50 dB |
| Linkspegel ohne Burst | −27,76 dB |
| Differenz (gemeinsame Hüllkurve) | **+2,27 dB** |

### 11.7 Stabilität und Sättigung

| Messung | Wert |
|---|---|
| 100 Blöcke Rauschen (±0,9), alle Parameter max | endlich, Peak **1,248707** (Ceiling 1,25) |
| Hot-Impuls 1,6, `attack = 100` | 1,250000 (begrenzt) |
| Hot-Impuls 1,6, `attack = 0` | 1,245919 (nur Sättigung) |
| Sättigung 1,0 / 1,5 / 2,0 / 4,0 | 1,000000 / 1,241007 / 1,249832 / 1,250000 |

---

## 12. Tests (`tests/TransientShaperModuleTests.cpp`)

| Testfall | Prüft | Messwert / Schwelle |
|---|---|---|
| Parametertabelle und Metadaten | `params().size() == 4`, IDs, Namen, Bereiche, Defaults, Einheiten, `type()`, `displayName()`, `latencySamples() == 0`, Klemmen, ungültige Indizes, NaN-Abweisung | exakt |
| `amount` 0 reicht bitgenau durch | 1× und 3× heißes Signal, 50 Blöcke Dauerbetrieb, danach wieder aktiv | `max|out − in| ≤ 1e-6` (ist 0) |
| ATTACK hebt den Einschlag an | Impuls: Peak `attack = 100` > `attack = 0` | Faktor > 1,2 (ist 1,97) |
| DUCK senkt den Pegel nach dem Einschlag | RMS 80…200 ms, Peak des Einschlags bleibt | < 0,8× (ist 0,60×), Peak ≥ 0,97× |
| Dauer-Sinus wird nicht verbogen | 60/100/1000 Hz bei `attack = 100`, DUCK ohne Pumpen, Default bitgenau | ≤ **3 dB** (ist ≤ 0,42 dB) |
| SPEED ändert die Erholzeit | Hüllkurve nach 100 ms, Pegelverlauf 150…300 ms | 16 ms < 0,2× von 200 ms (ist 0,0032×); Pegel +1 dB (ist +2,78 dB) |
| Stereo-Kohärenz | identisches L/R bitgleich, gemeinsame Hüllkurve über die Kanäle | `==` exakt; +1 dB (ist +2,27 dB) |
| 100 Blöcke Rauschen bleiben endlich | kein NaN/Inf, Grenzen von `gain` und Ausgang | Peak ≤ Ceiling |
| Stille, DC und sehr kurze Blöcke | Stille/Hüllkurven 0, DC konstant, 1-/7-Sample-Block, leerer Block, Aufruf ohne `prepare()` | DC 0,5 ± 0,01 |
| Weiche Sättigung ab 0 dBFS | bitgenau unter 0 dBFS, monoton, Asymptote, Symmetrie, NaN/Inf | exakt |

Verifikation: Die beiden Übersetzungseinheiten wurden mit den Projekt-Flags
(`/std:c++20 /W4 /permissive-`, MSVC 19.51) warnungsfrei übersetzt und über einen
temporären Runner **außerhalb des Repos** (identische Objekte/Bibliotheken des
bestehenden Build-Verzeichnisses) ausgeführt: **10 Testfälle, 80 769 Assertions, alle
grün**. Im Repo selbst wurde dafür nichts geändert.

---

## 13. Threading, Latenz, CPU

| Punkt | Wert |
|---|---|
| Latenz | 0 Samples (kein Lookahead) |
| `process()` | kein `new`, kein `log`, kein Mutex, kein `std::vector`-Zugriff |
| Parameterübergabe | `std::atomic<float>` (relaxed), ein Lesevorgang pro Block |
| Zusätzlicher Zustand | 2 `float` Hüllkurven + 4 `float` Glättung (keine Puffer, keine Allokation) |
| Rechenaufwand | pro Sample: 2 Detektor-Ops, 2 One-Pole, ~8 Mul/Add, 1 `tanh` — nur oberhalb 0 dBFS |
| Ausgabe-Diagnose | `currentGain()`, `currentFastEnvelope()`, `currentSlowEnvelope()` (atomar, nur Tests/Doku) |

---

## 14. Offene Punkte

| # | Punkt | Warum wichtig | Vorschlag |
|---|---|---|---|
| **O-T1** | `kA = 1,0` und `kD = 0,75` sind **nicht** aus den Referenzdaten ableitbar (dort stehen nur Parameterwerte, keine Kennlinien) | Klangtreue zum Original | Hörvergleich mit 10 Referenz-Kicks; ggf. `kA`/`kD` in einem Bereich 0,5…1,5 nachziehen — die Konstanten sind an einer Stelle dokumentiert und leicht änderbar |
| **O-T2** | Kein Lookahead: die Anhebung baut sich in der 0,5-ms-Attack-Zeit des schnellen Followers auf, extrem kurze Einschläge (< 0,5 ms) werden also nicht in voller Höhe angehoben | Snap bei sehr harten Kicks | Falls nötig: Lookahead 1…2 ms (dann `latencySamples() != 0` und Meldung an den Host, siehe Ketten-Spec 4.1) — Entscheidung nach Hörvergleich |
| **O-T3** | DUCK senkt auch Dauer-Signale (`sustainShare ≈ 0,5` → −3,83 dB bei `duck = 100`) | „Dröhnen zähmen" braucht das; „Dauer-Signal nicht verbiegen" nur bei `attack` geprüft | Bestätigen, ob das Original DUCK ebenfalls auf Dauerpegel wirken lässt; sonst `sustainShare` auf `max(0, −diff)` umstellen (eine Zeile) |
| **O-T4** | `amount` als Gesamt-Wirkungsgrad interpretiert (skaliert den Effekt-Term, nicht den Ausgangspegel) | Aus dem Handbuch nicht eindeutig | Am Original verifizieren; alternative Lesart wäre eine Ausgangspegelsenkung |
| **O-T5** | `speed` wirkt nur auf den schnellen Follower; der langsame bleibt bei 300 ms | Falls das Original auch die Slow-Release mitzieht, ändert sich das Nachschwing-Verhalten | Hörvergleich; Stelle ist zentral (`kSlowReleaseMs`) |
| **O-T6** | Sättigung greift bei `amount > 0` auch, wenn `gain = 1` (Signal > 0 dBFS), bei `amount = 0` dagegen gar nicht | Bypass-Nulltest vs. „nie clippen" | So belassen (Bitgenauigkeit hat Vorrang); im Rack begrenzt der Finalizer |
| **O-T7** | CPU-Budget (< 3 % je Modul) hier nicht gemessen — kein Echtzeit-Host in dieser Umgebung | Abnahmekriterium aus dem Masterplan | Im Host mit 10 Instanzen bei 48 kHz/128 nachmessen |
| **O-T8** | Registrierung in der Registry ist Aufgabe des Leads (`FxFactory::registerType`), dieses Ticket ändert `FxFactory.cpp` nicht | Reihenfolge im Start-Up | Siehe Abschnitt 15 |

---

## 15. Einbindung (CMake — Zeilen für den Lead)

Dieses Ticket legt **nur** die vier Dateien an und ändert kein CMake. Einzutragen sind:

`CMakeLists.txt`:

```cmake
set(BFKA_FX_SOURCES
    src/fx/FxFactory.cpp
    src/fx/FxChainProcessor.cpp
    src/fx/modules/TransientShaperModule.cpp   # NEU (B-65)
    )

set(BFKA_HEADERS
    ...
    src/fx/PassThroughModule.h
    src/fx/modules/TransientShaperModule.h     # NEU (B-65)
    ...
    )
```

`tests/CMakeLists.txt`:

```cmake
target_sources(BFKA_Tests PRIVATE
    ...
    FxChainTests.cpp
    TransientShaperModuleTests.cpp             # NEU (B-65)
    ...
    ../src/fx/FxChainProcessor.cpp
    ../src/fx/modules/TransientShaperModule.cpp # NEU (B-65)
    ...
    )
```

Registry (beim Start, z. B. in `PluginProcessor`-Initialisierung):

```cpp
FxFactory::get().registerType (FxType::transientShaper,
                               [] { return std::make_unique<TransientShaperModule>(); });
```
