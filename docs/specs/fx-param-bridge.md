# FX-Parameter-Bridge: `fx<N>_p<M>` ↔ Modulattribute (F2)

**Zweck.** Ein Kick-Preset speichert die FX-Kette **zweifach**:

* als **generische, normierte Parameter** `fx0_p0 … fx7_p13` (8 Slots × 14 Werte, 0…1),
* als **eingebetteten Snapshot** `<FxChain>` in **realen Einheiten** (Hz, dB, %, …).

Diese Doppelung ist der Schlüssel: für jedes Preset liegen beide Darstellungen
**derselben** Module vor. Damit lässt sich die Abbildung nicht nur vermuten, sondern
numerisch bestimmen.

**Ergebnis in einem Satz:** `p[i]` ist das *i*-te modulspezifische Attribut in
XML-Reihenfolge, `p12` ist **immer** `MIX`, `p13` ist **immer** `OUTGAIN`
(`outGain/2`) — **mit einer einzigen, belegten Ausnahme** beim Modultyp `shaper`.

**Datenbasis (nur lesend):** 44 Kick-Presets (davon 43 mit eingebettetem
Snapshot = **201 Module mit beiden Darstellungen**) und 76 FX-Chain-Presets
(**438 Module**, nur reale Einheiten) — zusammen **639 Modulinstanzen**.
Werkzeug: `tools/mine_presets.py`; Rohdaten: `docs/specs/mining-report.md` §5–§7
und `_private/mining/report.json`.

**Legende:** Sicher = ≥ 4 verschiedene Stützwerte, Fit exakt · Plausibel = 2–3
Stützwerte oder nur ein Default-Punkt · Unbekannt = Daten reichen nicht.

---

## 1. Datenlage und ein quantitativer Befund vorab

Die 76 Chains und 43 Snapshots enthalten **438 bzw. 201** Modulinstanzen (639
gesamt), aber die Referenz selbst variiert nur wenige Regler. Das begrenzt die
Auswertbarkeit:

| Modultyp | Vorkommen (gesamt) | davon in Presets | davon Attribute konstant | tatsächlich variabel |
|---|---|---|---|---|
| `dist` | 185 | 59 | 5 von 10 | `mix`, `outGain`, `drive`, `mode`, `folds` |
| `eq` | 138 | 41 | **11 von 12** | nur `outGain` |
| `mseq` | 94 | 34 | **13 von 15** | nur `outGain`, `smute` |
| `reverb` | 81 | 29 | 4 von 9 | `mix`, `wet`, `stereo`, `tone`, `speed` |
| `trans` | 59 | 20 | 4 von 8 | `outGain`, `attack`, `duck`, `speed` |
| `final` | 49 | 12 | 6 von 12 | `outGain`, `boost`, `freq`, `clip`, `monoF`, `clipOn` |
| `ott` | 27 | 5 | 6 von 12 | `outGain`, `depth`, `time`, `x1`, `gl`, `gm` |
| `shaper` | 6 | 1 | 7 von 9 | `shape`, `amount` |

Die Spalte „in Presets" ist entscheidend: nur diese Module liefern **beide**
Darstellungen und damit eine prüfbare Zuordnung.

`eq` und `mseq` werden in **keinem** der 44 Presets klanglich eingestellt (nur über
ihre `<Band>`-Kinder). Für ihre Skalare gibt es daher nur **einen** Stützpunkt.

---

## 2. Quantisierung: die `fx`-Werte haben 4 Nachkommastellen

Alle **3 116** belegten `fx`-Werte der 44 Presets liegen auf einem Raster von
**0.0001**:

| Raster | max. Abweichung |
|---|---|
| **1 × 10⁻⁴** | **4,5 × 10⁻⁸ absolut** (= 0,00045 Rasterschritte) |
| 1 × 10⁻³ | 4,5 × 10⁻⁵ absolut |
| 1 × 10⁻⁵ | 4,5 × 10⁻³ absolut (4500 Rasterschritte) |

Gegenprobe an einem Einzelwert: `drive = 18.83049964904785` → `drive/100 =
0.188304996…` → geschrieben wird `0.188299999`, also **0.1883**. Die Werte sind
gerundete 4-Stellen-Dezimalzahlen (in float32 abgelegt und mit 9 Stellen
ausgegeben).

**Zwei Konsequenzen:**

1. **Für den Nachbau:** `fx`-Werte mit 4 Nachkommastellen schreiben. Wer 9 Stellen
   schreibt, erzeugt Presets, die das Original beim Einlesen trotzdem akzeptiert —
   aber einen Roundtrip-Vergleich (B-17) nicht besteht.
2. **Für die Analyse:** Ein 1:1-Fit kann nur bis auf ±0.00005 genau sein. Die
   Toleranz in `tools/mine_presets.py` steht deshalb auf `1.2e-4`; ein strengerer
   Wert erzeugt falsche Negative (dieser Fehler ist beim Bau des Werkzeugs
   aufgetreten und dokumentiert).

---

## 3. Die Grundstruktur der Bridge

```
p[0 … k-1]  =  die k modulspezifischen Attribute in XML-Reihenfolge
p[12]       =  MIX        (linear 0…1)
p[13]       =  OUTGAIN    (linear, Bereich 0…2 → p = gain / 2)
p[k … 11]   =  0.0        (unbenutzte Reserve)
```

**Nicht** abgebildet werden `type` (Kettenmetadaten), `on` (in keinem `p`-Slot
nachweisbar) und `ir` (Dateipfad, Zeichenkette).

Reihenfolge der `<Module>`-Elemente **ist** die Signalkette; Modul an Position *i*
gehört zu FX-Slot *i* (0-basiert). Belege:

* Leere FX-Slots tragen in allen 43 Snapshots **genau ein** Muster:
  `p0…p11 = 0.0`, `p12 = 1.0`, `p13 = 0.5` — **151 von 151** Leer-Slots, keine
  Ausnahme.
* Die Zahl der `<Module>`-Elemente entspricht in allen 43 Snapshots der Zahl der
  belegten FX-Slots; die Modulanzahl je Preset reicht von 0 bis 8
  (Verteilung: 0 Module ×1, 2 ×2, 3 ×10, 4 ×12, 5 ×2, 6 ×11, 7 ×5, 8 ×1).

---

## 4. Beweisführung: `p12 = MIX` und `p13 = OUTGAIN`

`mix` und `outGain` sind die einzigen Attribute, die **in jedem** Modultyp an
derselben Stelle stehen — sie sind damit typunabhängig prüfbar.

### 4.1 `p12 = MIX` — Identität, exakt

| Modultyp | n | verschiedene `mix`-Werte | Fit `p12 = a·mix + b` | max. Fehler |
|---|---|---|---|---|
| `dist` | 59 | 4 | a = 1.000000, b = 3.1 × 10⁻¹⁰ | < 10⁻⁷ |
| `reverb` | 29 | 12 | a = 1.000000, b = 2.3 × 10⁻⁸ | < 10⁻⁷ |

`p12` ist der `mix` **unverändert** (nicht skaliert, nicht in dB). Zusätzlich gilt
für alle Typen, deren `mix` konstant 1.0 ist (`eq`, `mseq`, `final`, `ott`,
`shaper`, `trans`): `p12 = 1.0`. Das ist konsistent, aber nicht unterscheidend.

**Damit ist die Hypothese aus `03-RE-FINDINGS.md` §3.2 (`p12 = MIX`) bestätigt** —
und zwar an 88 Beobachtungen mit 16 verschiedenen Werten.

### 4.2 `p13 = OUTGAIN`, Bereich 0…2, exakt

| Modultyp | n | verschiedene `outGain`-Werte | Fit `p13 = a·outGain + b` | max. Fehler |
|---|---|---|---|---|
| `dist` | 59 | 3 | a = 0.500002, b = −1.7 × 10⁻⁶ | 2 × 10⁻⁵ |
| `trans` | 20 | 2 | a = 0.500796, b = −8.0 × 10⁻⁴ | 1 × 10⁻⁴ |
| `mseq` | 34 | 2 | a = 0.500138, b = −1.4 × 10⁻⁴ | 1 × 10⁻⁴ |
| `ott` | 5 | 2 | a = 0.499928, b = 7.2 × 10⁻⁵ | 1 × 10⁻⁴ |

Alle vier unabhängigen Typen ergeben **a ≈ 0.5, b ≈ 0**, also

```
p13            = outGain / 2.0            // outGain ∈ [0, 2]
outGain        = 2.0 * p13
```

Belegte Werte: `outGain = 0.392` → `p13 = 0.196`; `0.9908` → `0.4954`;
`1.0` → `0.5`; `1.2492` → `0.6246`; `1.3085` → `0.6543`; `2.0` → `1.0`.
`outGain` ist eine **lineare Verstärkung** (Faktor), keine dB-Angabe — sonst wäre
die Abbildung nicht linear über 0.392…2.0.

### 4.3 `p[i]` = i-tes Attribut in XML-Reihenfolge

Für jedes Attribut wurde geprüft, ob eine Gerade `p = a·x + b` **alle**
Beobachtungen des jeweiligen Typs auf ≤ 1.2 × 10⁻⁴ reproduziert. **21 Zuordnungen**
sind exakt; die dabei herausfallenden Ränder sind durchweg runde Zahlen:

| Modultyp | Attribut | p | n | Stützwerte | a | b | Bereich | Konfidenz |
|---|---|---|---|---|---|---|---|---|
| `dist` | `drive` | 0 | 59 | 34 | 0.010000 | ≈0 | **0…100** | sicher |
| `dist` | `mode` | 1 | 59 | 4 | 0.083337 | ≈0 | **0…12** | sicher |
| `dist` | `mix` | 12 | 59 | 4 | 1.000000 | 0 | 0…1 | sicher |
| `dist` | `outGain` | 13 | 59 | 3 | 0.500002 | ≈0 | 0…2 | plausibel |
| `reverb` | `wet` | 0 | 29 | 4 | 0.0099999 | ≈0 | **0…100** | sicher |
| `reverb` | `stereo` | 1 | 29 | 6 | 0.010000 | ≈0 | **0…100** | sicher |
| `reverb` | `tone` | 2 | 29 | 13 | 0.010000 | ≈0 | **0…100** | sicher |
| `reverb` | `mix` | 12 | 29 | 12 | 1.000000 | 0 | 0…1 | sicher |
| `trans` | `attack` | 1 | 20 | 11 | 0.0099999 | ≈0 | **0…100** | sicher |
| `trans` | `duck` | 2 | 20 | 8 | 0.010000 | ≈0 | **0…100** | sicher |
| `trans` | `speed` | 3 | 20 | 9 | 0.0054348 | −0.08695 | **16…200** | sicher |
| `trans` | `outGain` | 13 | 20 | 2 | 0.500796 | ≈0 | 0…2 | plausibel |
| `final` | `boost` | 0 | 12 | 7 | 0.083333 | ≈0 | **0…12** | sicher |
| `final` | `freq` | 1 | 12 | 4 | 0.0055553 | −0.11111 | **20…200** | sicher |
| `final` | `clip` | 2 | 12 | 4 | 0.010000 | ≈0 | **0…100** | sicher |
| `final` | `monoF` | 4 | 12 | 4 | 0.0022726 | −0.13636 | **60…500** | sicher |
| `ott` | `depth` | 0 | 5 | 3 | 0.0099988 | ≈0 | **0…100** | plausibel |
| `ott` | `gl` | 4 | 5 | 3 | 0.041668 | 0.50000 | **−12…+12** | plausibel |
| `ott` | `gm` | 5 | 5 | 3 | 0.041664 | 0.50001 | **−12…+12** | plausibel |
| `mseq` | `outGain` | 13 | 34 | 2 | 0.500138 | ≈0 | 0…2 | plausibel |
| `ott` | `outGain` | 13 | 5 | 2 | 0.499928 | ≈0 | 0…2 | plausibel |

**Alle Bereiche sind glatte Zahlen** — 0…100, 0…12, 16…200, 20…200, 60…500,
−12…+12, 0…2. Das ist der stärkste Hinweis darauf, dass die Positionshypothese
nicht nur zufällig passt: eine falsche Zuordnung würde beliebige Ränder erzeugen.

Die **Umkehrfunktionen** stehen in `mining-report.md` §7.2; für die Implementierung
gilt durchgängig

```
p_raw = (x - lo) / (hi - lo)        // x = Anzeigewert, [lo, hi] = Bereich oben
p     = round(p_raw, 4)             // 4 Nachkommastellen (§2)
x     = lo + p * (hi - lo)
```

**Roundtrip-Gegenprobe.** Rechnet man den gespeicherten `p`-Wert mit diesen
Umkehrfunktionen zurück in die reale Einheit, weicht das Ergebnis in **allen**
bestätigten Fällen um höchstens **7 × 10⁻³ Anzeigeeinheiten** vom Originalwert ab
(`dist` 4/4, `final` 4/4, `trans` 4/4, `reverb` 4/4, `ott` 4/4, `mseq` 1/1
Attribute verifiziert). Die verbleibende Abweichung ist genau die
4-Stellen-Quantisierung aus §2, hochgerechnet über die Spanne — nicht ein
Modellfehler.

---

## 5. Unabhängige Gegenprobe

Weil die Positionshypothese die Reihenfolge schon *annimmt*, wurde sie ein zweites
Mal **ohne** diese Annahme geprüft: für jedes Attribut wird gesucht, welcher
`p`-Index es unter `p = x/k` (k ∈ {1, 2, 10, 12, 100}) oder `p = (x+A)/2A`
erklärt. `p12`/`p13` sind dabei ausgenommen (dort sind Zufallstreffer systematisch).

Ergebnis für alle 20 Attribute, die überhaupt eine eindeutige Antwort zulassen:
**19 von 20 bestätigen den Positionsindex, 1 weicht ab.**

| Modultyp | Attribut | Positionsindex | unabhängig gefunden | überein? |
|---|---|---|---|---|
| `dist` | `drive` | 0 | p0 `x/100` | ja |
| `dist` | `mode` | 1 | p1 `x/12` | ja |
| `dist` | `agc` | 3 | p3 `x/1` | ja |
| `dist` | `folds` | 2 | p2 `x/10` | ja |
| `reverb` | `wet` | 0 | p0 `x/100` | ja |
| `reverb` | `stereo` | 1 | p1 `x/100` | ja |
| `reverb` | `tone` | 2 | p2 `x/100` | ja |
| `trans` | `amount` | 0 | p0 `x/100` | ja |
| `trans` | `attack` | 1 | p1 `x/100` | ja |
| `trans` | `duck` | 2 | p2 `x/100` | ja |
| `final` | `boost` | 0 | p0 `x/12` | ja |
| `final` | `clip` | 2 | p2 `x/100` | ja |
| `final` | `upward` | 3 | p3 `x/100` | ja |
| `final` | `shelfOn` / `monoOn` / `clipOn` | 5 / 6 / 7 | p5/p6/p7 `x/1` | ja (Boolesche, nicht trennbar) |
| `ott` | `depth` | 0 | p0 `x/100` | ja |
| `ott` | `gl` | 4 | p4 `(x+12)/24` | ja |
| `ott` | `gm` | 5 | p5 `(x+12)/24` | ja |
| `shaper` | `amount` | 4 | **p1** `x/100` | **nein** |

---

## 6. Der `shaper`-Sonderfall (belegte Abweichung)

`shaper` kommt in genau **einem** der 44 Presets vor. Die Beobachtung:

| Attribut | realer Wert | normiert an der Positionsstelle | gefunden |
|---|---|---|---|
| `sPre` | 1.0 | `p0 = 0.2857` | – |
| `sPost` | 1.0 | `p1 = 0.3125` | – |
| `sLine` | 1.0 | `p2 = 0.0` | – |
| `shape` | 2.0 | `p3 = 0.0` | – |
| `amount` | **31.2542** | `p4 = 0.0` | **`p1 = 0.3125 = 31.2542 / 100`** |

Vollständiger `p`-Vektor:
`[0.2857, 0.3125, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, 0.5]`.

**Auswertung:**

* `amount` wird bei `p1` abgebildet (`amount/100`), **nicht** bei `p4`. Die
  XML-Reihenfolge ist für den `shaper` also **nicht** die Bridge-Reihenfolge.
* `p2`, `p3`, `p4` sind **0.0**, obwohl `sPost = sLine = 1.0` und `shape = 2.0`
  nicht 0 sind. Diese Attribute werden offenbar **gar nicht** über `fx`-Parameter
  geführt (sie stehen nur im Snapshot).
* Damit bleiben für `shaper` genau **zwei** Bridge-Parameter: `p0` (Wert 0.2857)
  und `p1` (`amount/100`). Für `p0` gibt es zwei gleich gute Lesarten:

| Kandidat | Rechnung | Plausibilität |
|---|---|---|
| `p0 = sPre` mit Bereich **[0, 3.5]** | 1.0 / 3.5 = 0.285714 | gerundet auf 4 Stellen = 0.2857 ✔ |
| `p0 = shape / 7` | 2.0 / 7 = 0.285714 | ✔, würde aber `shape` bei p0 statt p3 verorten |

Beide sind mit **einem** Stützpunkt nicht trennbar. Da `shape` gleichzeitig an
`p3 = 0.0` steht, ist „`p0 = sPre`, `shape` nicht gebrückt" die konsistentere
Lesart — sie erklärt auch, warum `p2`/`p3`/`p4` leer sind.

**Konsequenz:** Beim Nachbau den `shaper` mit der Reihenfolge
`p0 = sPre, p1 = amount` implementieren und `sPost`, `sLine`, `shape` **aus dem
Snapshot** lesen, nicht aus den `fx`-Parametern. Messung zur Absicherung: in §7.

---

## 7. Was die `fx`-Parameter **nicht** enthalten (wichtig!)

Die Listen-Daten der Module — EQ-Bänder, Wellenform-Kurven, Hüllkurven — tauchen in
`fx0_p0 … fx7_p13` **nicht** auf:

| Modul | Kind-Elemente | Anzahl beobachtet | Attribute (reale Einheiten) |
|---|---|---|---|
| `eq` | `<Band>` | 340 | `kind`, `f`, `q`, `g`, `dyn`, `ch` |
| `mseq` | `<Band>` | 289 | `kind`, `f`, `q`, `g`, `dyn`, `ch` |
| `dist` | `<WNode>` | 98 | `x`, `y`, `ten`, `seg` |
| `reverb` | `<Pt>` | 205 | `x`, `y`, `t` |
| `shaper` | `<Pt>` | 0 beobachtet | `x`, `y`, `t` (laut Format, hier nicht genutzt) |

Begründung: `p8 … p11` sind in **allen** 44 Presets konstant `0.0`, bei `eq`/`mseq`
endet die Belegung nachweislich bei `p7` bzw. `p10` — für eine 12-Band-Liste ist
dort kein Platz. Beispiel `eq.Band`: Frequenzen 38.5…14 216 Hz, Gains −32…+29.8 dB,
Q 0.3…2.21 — reale Werte, keine 0…1-Normierung.

**Das ist die wichtigste Implementierungsanweisung dieses Dokuments:**

> Bänder, Kurven und Hüllkurven werden **ausschließlich** aus dem eingebetteten
> `<FxChain>`-Snapshot gelesen und geschrieben. `fx<N>_p<M>` transportiert nur
> **Skalare**. Wer die Bänder aus den `fx`-Parametern rekonstruieren will, verliert
> sie vollständig.

---

## 8. Vollständige Attributlisten je Modultyp (aus 201 + 438 Modulinstanzen)

Attributreihenfolge wie in der XML; „→ p" ist die abgeleitete Bridge-Position.
Fettdruck = numerisch bestätigt (siehe §4.3 / §5).

### `eq` — 138 Vorkommen, 8 Bridge-Parameter

| # | Attribut | realer Wertebereich | → p | Status |
|---|---|---|---|---|
| 0 | `lowgain` | 0.0 (konstant) | p0 | unbekannt (0 dB ↔ 0.5) |
| 1 | `lowfreq` | 90.0 (konstant) | p1 | unbekannt (p1 = 0.529) |
| 2 | `midgain` | 0.0 (konstant) | p2 | unbekannt (0 dB ↔ 0.5) |
| 3 | `midfreq` | 900.0 (konstant) | p3 | unbekannt (p3 = 0.4771) |
| 4 | `midq` | 0.9 (konstant) | p4 | unbekannt (p4 = 0.1622) |
| 5 | `highgain` | 0.0 (konstant) | p5 | unbekannt (0 dB ↔ 0.5) |
| 6 | `highfreq` | 8000.0 (konstant) | p6 | unbekannt (p6 = 0.7094) |
| 7 | `middyn` | 0.0 (konstant) | p7 | unbekannt (0 dB ↔ 0.5) |
| – | `mix` | 1.0 | **p12** | plausibel (nicht unterscheidend) |
| – | `outGain` | 0.7264…1.2164 | **p13** | plausibel (p13 = outGain/2) |
| – | `on` | 1.0 | – | nicht gebrückt |
| Kind | `<Band kind f q g dyn ch>` | s. §7 | – | **nicht in `fx`** |

**Wichtigster Teilschluss für `eq`:** die vier Nicht-0.5-Werte sind
`p1 = 0.5290` (lowfreq 90 Hz), `p3 = 0.4771` (midfreq 900 Hz), `p4 = 0.1622`
(midq 0.9), `p6 = 0.7094` (highfreq 8000 Hz). Die drei **Frequenzen** müssten bei
einem gemeinsamen Frequenzbereich monoton mit der Frequenz steigen — sie tun es
nicht (0.5290 > 0.4771 bei 90 Hz < 900 Hz).

| gemeinsamer Log-Bereich | p(90 Hz) | p(900 Hz) | p(8000 Hz) | gemessen |
|---|---|---|---|---|
| 20…20 000 Hz | 0.218 | 0.551 | 0.867 | 0.529 / 0.4771 / 0.7094 |
| 20…2 000 Hz | 0.327 | 0.827 | – | – |
| 10…20 000 Hz | 0.289 | 0.592 | 0.879 | – |
| 20…10 000 Hz | 0.242 | 0.613 | 0.964 | – |

Damit ist **bewiesen**, dass die drei Bänder **unterschiedliche** Frequenzbereiche
haben (oder die Reihenfolge innerhalb von `p1/p3/p6` eine andere ist). Die konkreten
Bereiche sind aus den Daten **nicht** bestimmbar → §9.

Nebenbefund: `0.4771 = ln(9)/ln(100)` ist exakt die Log-Position von `Q = 0.9` im
Bereich [0.1, 10] — sie steht aber an `p3` (midfreq), nicht an `p4` (midq). Das ist
entweder Zufall oder ein Hinweis, dass die beiden Positionen in der Bridge
vertauscht sind. Auch das klärt erst §9.

### `mseq` — 94 Vorkommen, 11 Bridge-Parameter

| # | Attribut | realer Wertebereich | → p | Status |
|---|---|---|---|---|
| 0 | `midlow` | 0.0 (konstant) | p0 | unbekannt (0 dB ↔ 0.5) |
| 1 | `midhigh` | 0.0 (konstant) | p1 | unbekannt (0 dB ↔ 0.5) |
| 2 | `sidelow` | 0.0 (konstant) | p2 | unbekannt (0 dB ↔ 0.5) |
| 3 | `sidehigh` | 0.0 (konstant) | p3 | unbekannt (0 dB ↔ 0.5) |
| 4 | `width` | 100.0 (konstant) | p4 | plausibel: 100 ↔ 0.5 ⇒ Bereich 0…200 |
| 5 | `middyn` | 0.0 (konstant) | p5 | unbekannt (0 dB ↔ 0.5) |
| 6 | `sidedyn` | 0.0 (konstant) | p6 | unbekannt (0 dB ↔ 0.5) |
| 7 | `msolo` | 0.0 (konstant) | p7 | plausibel (Boolean 0 → 0) |
| 8 | `ssolo` | 0.0 (konstant) | p8 | plausibel (Boolean 0 → 0) |
| 9 | `mmute` | 0.0 (konstant) | p9 | plausibel (Boolean 0 → 0) |
| 10 | `smute` | 0.0…1.0 | p10 | plausibel (Boolean 0 → 0) |
| – | `mix` | 1.0 | **p12** | plausibel (nicht unterscheidend) |
| – | `outGain` | 0.6899…1.3085 | **p13** | plausibel (p13 = outGain/2) |
| Kind | `<Band kind f q g dyn ch>` | s. §7 | – | **nicht in `fx`** |

Für `mseq` ist die Struktur trotzdem gut gestützt: der Default-Vektor
`[0.5 ×7, 0, 0, 0, 0, 0]` erklärt sich unter der Positionshypothese **exakt** —
vier 0-dB-Gains und zwei 0-dB-Dynamikwerte liegen bei 0.5, `width = 100` bei
0.5 (halber Bereich von 200), die vier Schalter bei 0.0. Mit 11 Attributen und
12 Slots ist das ein starkes Indiz, auch ohne variierende Werte.

### `dist` — 185 Vorkommen, 6 Bridge-Parameter

| # | Attribut | realer Wertebereich | → p | Status |
|---|---|---|---|---|
| 0 | `drive` | 0.0…100.0 (55 Werte) | p0 | **sicher: p0 = drive/100** |
| 1 | `mode` | 1.0…12.0 (4 Werte) | p1 | **sicher: p1 = mode/12** |
| 2 | `folds` | 2.0…2.4003 | p2 | plausibel (2.0 ↔ 0.2 ⇒ Bereich 0…10) |
| 3 | `agc` | 1.0 (konstant) | p3 | plausibel (Boolean 1 → 1) |
| 4 | `eqmode` | 0.0 (konstant) | p4 | plausibel (Enum 0 → 0) |
| 5 | `flip` | 0.0 (konstant) | p5 | plausibel (Boolean 0 → 0) |
| – | `mix` | 0.386…1.0 (7 Werte) | **p12** | **sicher: p12 = mix** |
| – | `outGain` | 0.392…2.0 (6 Werte) | **p13** | plausibel: p13 = outGain/2 |
| Kind | `<WNode x y ten seg>` | s. §7 | – | **nicht in `fx`** |

Ergänzend: `mode` hat 12 Stufen (1…12) — die Bridge kennt also mehr Modi als die
zehn in der Bestandsaufnahme dokumentierten. `eqmode` als Enum 0/1/2 (OFF/PRE/POST)
bei einem Bereich 0…2 wäre zu `p4 = 0.0` konsistent.

### `shaper` — 6 Vorkommen, 2 Bridge-Parameter (Sonderfall, §6)

| # | Attribut | realer Wertebereich | → p | Status |
|---|---|---|---|---|
| 0 | `sPre` | 1.0 (konstant) | p0 | **unbekannt** (p0 = 0.2857; Kandidat Bereich 0…3.5) |
| 1 | `sPost` | 1.0 (konstant) | – | nicht gebrückt (p2…p4 = 0) |
| 2 | `sLine` | 1.0 (konstant) | – | nicht gebrückt |
| 3 | `shape` | 0.0…2.0 | – | nicht gebrückt |
| 4 | `amount` | 0.0…100.0 | **p1** | **belegt: p1 = amount/100** (nicht p4!) |
| – | `mix` | 1.0 | **p12** | plausibel |
| – | `outGain` | 1.0 | **p13** | plausibel |
| Kind | `<Pt x y t>` | 0 beobachtet | – | **nicht in `fx`** |

### `trans` — 59 Vorkommen, 4 Bridge-Parameter

| # | Attribut | realer Wertebereich | → p | Status |
|---|---|---|---|---|
| 0 | `amount` | 100.0 (konstant) | p0 | plausibel (100 ↔ 1.0 ⇒ Bereich 0…100) |
| 1 | `attack` | 0.0…100.0 (22 Werte) | p1 | **sicher: p1 = attack/100** |
| 2 | `duck` | 0.0…63.02 (12 Werte) | p2 | **sicher: p2 = duck/100** |
| 3 | `speed` | 47.16…179.53 (19 Werte) | p3 | **sicher: p3 = (speed−16)/184** ⇒ Bereich 16…200 |
| – | `mix` | 1.0 | **p12** | plausibel |
| – | `outGain` | 0.9459…1.2807 | **p13** | plausibel |

`speed` ist der einzige Parameter mit **nicht** glatter 0…100-Skala: der Fit liefert
präzise `p = (speed − 16)/184`. Das entspricht einem Bereich **16…200** — ein
plausibler Zeitbereich in Millisekunden. Die Steigung ist mit 9 Stützwerten über
47…180 sehr gut bestimmt.

### `reverb` — 81 Vorkommen, 4 Bridge-Parameter

| # | Attribut | realer Wertebereich | → p | Status |
|---|---|---|---|---|
| – | `ir` | Dateipfad (String) | – | **nicht in `fx`** |
| 0 | `wet` | 35.0…100.0 (9 Werte) | p0 | **sicher: p0 = wet/100** |
| 1 | `stereo` | 0.0…100.0 (15 Werte) | p1 | **sicher: p1 = stereo/100** |
| 2 | `tone` | 3.08…100.0 (23 Werte) | p2 | **sicher: p2 = tone/100** |
| 3 | `speed` | 0.0…11.778 | p3 | unbekannt (0.0 ↔ 0.5 ⇒ bipolar, Spanne offen) |
| – | `mix` | 0.257…0.52 (18 Werte) | **p12** | **sicher: p12 = mix** |
| – | `outGain` | 1.0 | **p13** | plausibel |
| Kind | `<Pt x y t>` | x 0…1, y 0…1, t −1…1 | – | **nicht in `fx`** |

`speed` ist bipolar (`0.0` liegt bei `p3 = 0.5`) — plausibel als IR-Stretch in
Halbtönen mit 0 in der Mitte. Die Spanne ist aus einem Punkt nicht bestimmbar.

### `final` — 49 Vorkommen, 8 Bridge-Parameter

| # | Attribut | realer Wertebereich | → p | Status |
|---|---|---|---|---|
| 0 | `boost` | 0.3549…12.0 (15 Werte) | p0 | **sicher: p0 = boost/12** ⇒ Bereich 0…12 |
| 1 | `freq` | 35.75…95.69 (8 Werte) | p1 | **sicher: p1 = (freq−20)/180** ⇒ Bereich 20…200 Hz |
| 2 | `clip` | 0.0…37.23 (9 Werte) | p2 | **sicher: p2 = clip/100** |
| 3 | `upward` | 20.0 (konstant) | p3 | plausibel (20 ↔ 0.2 ⇒ Bereich 0…100) |
| 4 | `monoF` | 60.0…181.05 (7 Werte) | p4 | **sicher: p4 = (monoF−60)/440** ⇒ Bereich 60…500 Hz |
| 5 | `shelfOn` | 1.0 (konstant) | p5 | plausibel (Boolean 1 → 1) |
| 6 | `monoOn` | 1.0 (konstant) | p6 | plausibel (Boolean 1 → 1) |
| 7 | `clipOn` | 0.0…1.0 | p7 | plausibel (Boolean 0/1 → 0/1) |
| – | `mix` | 1.0 | **p12** | plausibel |
| – | `outGain` | 0.8928…1.0 | **p13** | plausibel |

`final` ist der **am besten belegte** Modultyp: vier Attribute mit 4–7 Stützwerten
ergeben durchweg glatte Bereiche (0…12, 20…200, 0…100, 60…500).

### `ott` — 27 Vorkommen, 8 Bridge-Parameter

| # | Attribut | realer Wertebereich | → p | Status |
|---|---|---|---|---|
| 0 | `depth` | 8.54…41.0 (8 Werte) | p0 | plausibel: p0 = depth/100 |
| 1 | `time` | 100.0…116.70 | p1 | unbekannt (100 ↔ 0.4716) |
| 2 | `x1` | 120.0…323.30 | p2 | **mehrdeutig** (s. u.) |
| 3 | `x2` | 2500.0 (konstant) | p3 | unbekannt (2500 ↔ 0.5067) |
| 4 | `gl` | 0.0…7.615 (3 Werte) | p4 | **plausibel: p4 = (gl+12)/24** ⇒ ±12 dB |
| 5 | `gm` | −2.793…7.558 (8 Werte) | p5 | **plausibel: p5 = (gm+12)/24** ⇒ ±12 dB |
| 6 | `gh` | 0.0 (konstant) | p6 | unbekannt (0 dB ↔ 0.5 ⇒ bipolar) |
| 7 | `linear` | 0.0 (konstant) | p7 | plausibel (Boolean 0 → 0) |
| – | `mix` | 1.0 | **p12** | plausibel |
| – | `outGain` | 1.0…2.0 | **p13** | plausibel |

`x1` ist ein Sonderfall: mit nur **zwei** verschiedenen Werten (120 → 0.4714,
323.3 → 0.8871) passen **lineare und logarithmische** Abbildung beide exakt. Die
lineare Deutung ergibt einen negativen unteren Rand (`p=0 → −110 Hz`, physikalisch
unsinnig), die logarithmische einen Bereich **39…423 Hz**. Für einen
3-Band-Crossover sind 39…423 Hz plausibel, aber keinesfalls gesichert. **Mehrdeutig
→ offen.**

---

## 9. Zuordnungstabelle mit Konfidenz (Zusammenfassung)

| Modultyp | sicher | plausibel | unbekannt |
|---|---|---|---|
| `dist` | `drive`→p0, `mode`→p1, `mix`→p12 | `folds`→p2 (2.0↔0.2), `agc`→p3, `eqmode`→p4, `flip`→p5, `outGain`→p13 | – |
| `final` | `boost`→p0, `freq`→p1, `clip`→p2, `monoF`→p4 | `upward`→p3, `shelfOn`→p5, `monoOn`→p6, `clipOn`→p7, `mix`→p12, `outGain`→p13 | – |
| `trans` | `attack`→p1, `duck`→p2, `speed`→p3 | `amount`→p0, `mix`→p12, `outGain`→p13 | – |
| `reverb` | `wet`→p0, `stereo`→p1, `tone`→p2, `mix`→p12 | `outGain`→p13 | `speed`→p3 (Spanne) |
| `mseq` | – | `width`→p4, `msolo`→p7, `ssolo`→p8, `mmute`→p9, `smute`→p10, `mix`→p12, `outGain`→p13 | `midlow/midhigh/sidelow/sidehigh/middyn/sidedyn` (p0…p3, p5, p6) |
| `ott` | – | `depth`→p0, `gl`→p4, `gm`→p5, `linear`→p7, `mix`→p12, `outGain`→p13 | `time`→p1, `x1`→p2 (mehrdeutig), `x2`→p3, `gh`→p6 |
| `eq` | – | `mix`→p12, `outGain`→p13 | alle 8 Skalare p0…p7 |
| `shaper` | `amount`→**p1** | `mix`→p12, `outGain`→p13 | `sPre`→p0; `sPost`/`sLine`/`shape` nicht gebrückt |

**Was „plausibel mit einem Punkt" heißt:** Der Default-Punkt ist *konsistent* mit
der Position (z. B. `0 dB ↔ 0.5`, `Boolean 1 ↔ 1.0`), aber jede andere Zuordnung
mit gleicher Wertelogik wäre es auch. Solche Einträge dürfen nicht als verifiziert
gelten.

---

## 10. Offene Messungen

Alle Messungen nach demselben Muster: **im Original einen Regler verstellen →
FX-Chain-Preset *und* Kick-Preset speichern → beide XML vergleichen.** Aus
`Attribut (real) ↔ fx<N>_p<M> (normiert)` fällt die Formel direkt heraus.

| Priorität | Messung | Schließt |
|---|---|---|
| 1 | Im Kick-Preset `eq`: LOWFREQ, MIDFREQ, HIGHFREQ, MIDQ je auf **Minimum und Maximum** | die einzige unbestätigte Stelle in §4 — und die in §8 gezeigte Nicht-Monotonie |
| 2 | Im Kick-Preset `shaper`: `sPre` auf Min/Max; `amount` auf Min/Max | den Sonderfall §6 (`p0`-Frage, `amount`→p1 bestätigen) |
| 3 | `ott`: `x1`/`x2` auf Min/Max, `time` auf Min/Max, `gh` auf ±Extrem | `ott` ist mit `eq` zusammen der am schlechtesten belegte Typ |
| 4 | `reverb`: `speed` auf Min/Max | Spanne des bipolaren IR-Stretch |
| 5 | `mseq`: einen der vier Gains auf ±Extrem | Bereich der dB-Gains (p0…p3) und damit die Skala für 6 unbekannte Parameter |
| 6 | je Modultyp `mix` einmal auf 0.5 | bestätigt p12 auch für die Typen, bei denen `mix` sonst konstant 1.0 ist |

Die Priorität folgt dem Klangeinfluss: `eq` und `ott` sind die Typen mit den meisten
unbekannten Skalaren; `final`, `trans`, `reverb`, `dist` sind bis auf Kleinigkeiten
geklärt.

---

## 11. Zusammenfassung

**Belegt:**

1. **`p12` = `mix`** (Identität, 88 Beobachtungen, 16 verschiedene Werte, Fehler < 10⁻⁷).
2. **`p13` = `outGain / 2`** (Bereich 0…2, aus vier Modultypen unabhängig).
3. **`p[i]` = *i*-tes modulspezifisches Attribut in XML-Reihenfolge** — 21 exakte
   Fits mit durchweg **glatten** Bereichen (0…100, 0…12, 16…200, 20…200, 60…500,
   ±12 dB, 0…2) und **19 von 20** unabhängigen Gegenproben.
4. **`fx`-Werte sind auf 4 Nachkommastellen quantisiert** (Raster 10⁻⁴, Abweichung
   4,5 × 10⁻⁸).
5. **Bänder, Kurven und Hüllkurven sind *nicht* in den `fx`-Parametern** — sie
   existieren nur im eingebetteten `<FxChain>`-Snapshot in realen Einheiten.
6. **`shaper` weicht ab:** `amount` liegt auf `p1` (nicht `p4`); `sPost`, `sLine`,
   `shape` werden gar nicht gebrückt.

**Offen:** die Skalare von `eq` (8), von `mseq` (6) sowie `ott.time`, `ott.x1`,
`ott.x2`, `ott.gh` und `reverb.speed` — durchweg Parameter, die in den 44
Werks-Presets nie verstellt wurden. Messrezepte in §10.

**Eine unerwartete Konsequenz für den Nachbau:** die Bridge ist **nicht** „14 freie
Parameter je Modul", sondern eine **feste, typabhängige Zuordnung** mit zwei
Reserve-Slots (`p12`/`p13` sind belegt, `p[k…11]` ungenutzt). Für die
Preset-Kompatibilität muss diese Zuordnung **pro Modultyp** implementiert werden —
inklusive der `shaper`-Ausnahme.
