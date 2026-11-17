# Parameter-Normalisation: 0…1-Werte → Anzeigewerte (F1)

**Zweck.** Die Referenz speichert ihren kompletten Kick-Zustand als XML mit
**normalisierten Werten** (0…1, 9 Nachkommastellen). Für einen 1:1-Nachbau muss
jede Normalisierungskurve bekannt sein — sonst klingen importierte Presets anders
und Automationsdaten passen nicht.

**Status:** Dieses Dokument beantwortet F1 **teilweise**. Aus den 44 Werks-Presets
lassen sich mehrere Kurven **beweisen** und mehrere gängige Annahmen **widerlegen**;
der Rest ist mit konkreten Messungen am Original zu schließen (Abschnitt 7).

**Datenbasis (nur lesend):** 44 Kick-Presets, 528 belegte Parameter-Sätze
(44 × 12 Slots) plus 44 globale Sätze. Vollständige Rohauswertung:
`tools/mine_presets.py` → `docs/specs/mining-report.md` und
`_private/mining/report.json`.

**Methodische Grenze.** Ein Preset liefert pro Parameter nur dann Kurveninformation,
wenn der Parameter **variiert**. Genau das ist das Problem: die Werks-Presets sind
überwiegend auf Werksdefaults. Bei den meisten Parametern sehen wir daher nur
**einen** Stützpunkt — und ein Punkt bestimmt keine Kurve. Was ein Punkt aber sehr
wohl kann: eine **falsche** Kurve widerlegen. Davon macht dieses Dokument
ausgiebig Gebrauch.

**Legende:** `[B]` belegt (aus den Daten zwingend) · `[W]` widerlegt ·
`[H]` Hypothese (plausibel, nicht belegt) · `[O]` offen (Daten reichen nicht)

---

## 1. Vollständige Parameterliste (Ist-Stand aus den Daten)

Die Daten enthalten **einen Parameter, der in der bisherigen Bestandsaufnahme fehlt**:

| Befund | Detail |
|---|---|
| Neuer Slot-Parameter | **`lN_fistart`** — 20. Slot-Parameter, in genau **1** von 44 Presets vorhanden (12 Werte, alle `0.0`) |
| Konsequenz | Die Parameter-Registry (B-11) braucht **20** Slot-Parameter, nicht 19. `fistart` gehört zu den Fade-In-Feldern (`ficurve`/`fishape`); es ist wie `spare1/2` als Reserve zu führen, aber **im Zustand zu erhalten**. |

Alle übrigen in `docs/01-BESTANDSAUFNAHME.md` §6.1 genannten IDs sind vorhanden —
nichts fehlt. Die FX-Indizes `fx0_p0 … fx7_p13` sind **vollständig** belegt
(alle 14 Indizes kommen vor).

---

## 2. Beobachtete Wertebereiche (alle 44 Presets)

### 2.1 Slot-Parameter `lN_*` (N = 0…11, je 528 Werte)

| ID-Schema | min | max | Median | distinct (max) | Werksdefault | Einordnung |
|---|---|---|---|---|---|---|
| `lN_vol` | 0.000000 | 0.804556 | 0.699840 | 11 | **0.699840486** | dB-verdächtig `[O]` |
| `lN_pitch` | 0.477958 | 1.000000 | 0.500000 | 6 | **0.500000060** | bipolar um 0.5 `[B]`, Skala `[O]` |
| `lN_mute` | 0.000000 | 0.000000 | 0.000000 | 1 | 0.0 | boolean `[B]` |
| `lN_solo` | 0.000000 | 0.000000 | 0.000000 | 1 | 0.0 | boolean `[B]` |
| `lN_pos` | 0.407500 | 0.544000 | 0.500000 | 11 | **0.500000000** | Anteil 0…1, Mitte = 0.5 `[H]` |
| `lN_trimstart` | 0.000000 | 0.001500 | 0.000000 | 2 | 0.0 | Anteil der Sample-Länge `[H]` |
| `lN_trimend` | 0.196200 | 1.000000 | 1.000000 | 5 | **1.000000000** | Anteil der Sample-Länge `[H]` |
| `lN_fadein` | 0.000000 | 1.000000 | 0.000000 | 5 | 0.0 | Anteil der Sample-Länge `[H]` |
| `lN_fadeout` | 0.000000 | 0.158200 | 0.000000 | 7 | 0.0 | Anteil der Sample-Länge `[H]` |
| `lN_stretch` | 0.176683 | 0.408213 | 0.374989 | 2 | **0.374989420** | siehe §4 — **nicht** logarithmisch `[W]` |
| `lN_filter` | 1.000000 | 1.000000 | 1.000000 | 1 | 1.0 | offen = Maximum `[H]` |
| `lN_filterq` | 0.102564 | 0.102564 | 0.102564 | 1 | **0.102564096** | Q-Bereich `[0.1, 4.0]` `[H]`, siehe §5 |
| `lN_dlytime` | 0.300000 | 0.300000 | 0.300000 | 1 | **0.300000012** | exakt `float32(0.3)` `[B]` |
| `lN_dlymix` | 0.000000 | 0.000000 | 0.000000 | 1 | 0.0 | linear 0…1 `[H]` |
| `lN_spare1` | 0.000000 | 0.710000 | 0.500000 | 4 | 0.5 | Reserve, **variiert** |
| `lN_spare2` | 0.500000 | 0.500000 | 0.500000 | 1 | 0.5 | Reserve |
| `lN_ficurve` | 0.000000 | 0.500000 | 0.500000 | 3 | **0.500000000** | bipolare Kurvenbiegung `[H]` |
| `lN_fishape` | 0.000000 | 0.000000 | 0.000000 | 1 | 0.0 | Modus-Index 0 `[H]` |
| `lN_foshape` | 0.000000 | 0.000000 | 0.000000 | 1 | 0.0 | Modus-Index 0 `[H]` |
| `lN_fistart` | 0.000000 | 0.000000 | 0.000000 | 1 | 0.0 | neu entdeckt, Reserve |

### 2.2 Globale Parameter (je 44 Werte)

| ID | min | max | distinct | Werksdefault | Anteil Default |
|---|---|---|---|---|---|
| `sat_amount` | 0.000000 | 0.000000 | 1 | 0.0 | 100 % |
| `reverb_size` | 0.500000 | 0.500000 | 1 | **0.5** | 100 % |
| `reverb_damp` | 0.500000 | 0.500000 | 1 | **0.5** | 100 % |
| `reverb_mix` | 0.000000 | 0.000000 | 1 | 0.0 | 100 % |
| `hollow_amt` | 0.000000 | 0.000000 | 1 | 0.0 | 100 % |
| `max_amount` | 0.000000 | 0.000000 | 1 | 0.0 | 100 % |
| `master_vol` | 0.833333 | 1.000000 | **2** | **0.833333313** | 97.7 % |
| `midi_mode` | 0.000000 | 0.000000 | 1 | 0.0 | 100 % |
| `root_note` | 0.283465 | 0.283465 | 1 | **0.283464581** | 100 % |
| `gspare1` | 0.000000 | 0.000000 | 1 | 0.0 | 100 % |
| `gspare2` | 0.500000 | 0.500000 | 1 | 0.5 | 100 % |
| `gspare3` | 0.000000 | 0.500000 | 2 | 0.5 | 97.7 % |

**Wichtige Konsequenz:** 15 von 32 Parameterarten stehen in **allen** 44 Presets auf
demselben Wert. Für diese ist aus Presets **prinzipiell keine** Kurve ableitbar —
hier hilft nur Messung am Original.

---

## 3. Belegte Abbildungen

### 3.1 `root_note` = MIDI-Note / 127  `[B]`

Alle 44 Presets speichern exakt `0.283464581`. Vergleich:

| Kandidat | Wert | Abweichung zum Messwert |
|---|---|---|
| **36 / 127** | 0.283464566929… | 1.4 × 10⁻⁸ |
| `float32(36/127)` | 0.283464580774… | **geschrieben als 0.283464581 — exakt** |
| 36 / 128 | 0.281250000 | 2.2 × 10⁻³ |
| 36 / 100 | 0.360000000 | 7.7 × 10⁻² |

Nur `n/127` passt, und zwar **bis auf die letzte geschriebene Stelle genau** (der
Wert ist die 9-stellige Darstellung des float32 von 36/127). Damit ist:

```
root_note_normalised = midi_note / 127.0          // n = 0…127
midi_note            = round(root_note * 127.0)   // Default: 36
```

Die Vermutung aus `03-RE-FINDINGS.md` §3.1 („0.283464581 könnte 36/127 sein") ist
damit **bestätigt**. Dass *alle* 44 Presets 36 tragen, ist ebenfalls eine Aussage:
der Grundton wird offenbar nicht pro Preset gespeichert, sondern folgt der
Key-Track-Einstellung beim Rollen. ***Nicht* bewiesen** ist, ob für `n` eine
Oktavnormierung (C4-Bezug) nötig ist — dafür fehlt ein Preset mit abweichendem
`root_note`.

### 3.2 `pitch` ist um 0.5 zentriert  `[B]`

Der „unberührte" Wert lautet in 31 von 44 Slot-0-Fällen `0.500000060`. Das ist
**exakt eine float32-ULP oberhalb von 0.5** (2⁻²⁴ = 5.96 × 10⁻⁸). Ein float32-0.5
würde als `0.500000000` geschrieben; der Wert ist also 0.5 plus Rundungsrauschen,
d. h. **die Ruhelage ist 0 Halbtöne bei normiert 0.5**.

```
semitones = (pitch - 0.5) * 2 * REICHWEITE      // REICHWEITE noch offen, s. §6
```

Damit ist die Annahme „pitch symmetrisch um 0.5" **belegt** — die *Reichweite* aber
**nicht**. Gegenprobe: bei ±12 HT müssten alle beobachteten Werte auf ganzen
Halbtönen liegen:

| normiert | Δ zu 0.5 | HT bei ±12 | HT bei ±24 | ganzzahlig? |
|---|---|---|---|---|
| 0.500000060 | +0.000000060 | +0.0000014 | +0.0000029 | ja (Default) |
| 0.504666686 | +0.004666686 | +0.1120 | +0.2240 | nein |
| 0.541666687 | +0.041666687 | **+1.0000** | +2.0000 | ja (bei ±12) |
| 0.543895841 | +0.043895841 | +1.0535 | +2.1070 | nein |
| 0.477958351 | −0.022041649 | −0.5290 | −1.0580 | nein |
| 0.510291696 | +0.010291696 | +0.2470 | +0.4940 | nein |

Nur **einer** von sechs Werten fällt exakt auf ein Halbton-Raster (`0.541666687`
= 13/24 → +1 HT bei ±12). Das ist ein Indiz, kein Beweis. Die Presets enthalten
offensichtlich auch krumme Pitch-Werte, deshalb ist die Skala aus dieser Stichprobe
nicht entscheidbar.

### 3.3 `master_vol` — Default ist exakt 5/6  `[B]`

43 von 44 Presets: `0.833333313`. Das ist die 9-stellige Form von
`float32(5/6) = 0.8333333134651184`. `master_vol` ist also **kein** krummer
Messwert, sondern ein exakter Bruch.

Damit lässt sich die Skala aber **nicht** festlegen (siehe §6.2).

### 3.4 `dlytime` — Default ist exakt float32(0.3)  `[B]`

Alle 528 Werte: `0.300000012` = `float32(0.3)`. Der Delay-Zeit-Parameter hat also
eine **angezeigte** Größe mit Default 0.3 (z. B. 300 ms bei einem Bereich 0…1 s
oder 0…1000 ms) — der normierte Wert ist 1:1 die Anzeigegröße geteilt durch das
Maximum. Konsequenz für die Registry: als Wahrscheinlichkeits-/Zeitanteil mit
Default 0.3 zu führen, Einheit erst nach Messung (§7.5).

### 3.5 Booleans und Anteile  `[B]`/`[H]`

| Parameter | Befund | Einordnung |
|---|---|---|
| `lN_mute`, `lN_solo` | immer 0.0, nie > 0 | boolean, Default `false` `[B]` |
| `midi_mode` | immer 0.0 | Enum/Boolean, Default 0 `[B]` |
| `lN_trimstart` / `lN_trimend` | Default 0.0 / 1.0, dazwischen Werte | **Anteil 0…1** der Sample-Länge `[H]` |
| `lN_fadein` / `lN_fadeout` | Default 0.0, Werte bis 1.0 | Anteil 0…1 `[H]` |
| `lN_pos` | Default 0.5, Streuung 0.4075…0.5440 eng um 0.5 | Anzeigeposition, 0…1 `[H]` |
| `lN_spare1` | Default 0.5, Werte 0.0…0.71 | Reserve, in der Tat ungenutzt |
| `lN_ficurve` | Default 0.5, außerdem 0.0 und 0.24 | **bipolarer** Kurvenbieger, 0.5 = linear `[H]` |
| `lN_fishape`, `lN_foshape` | immer 0.0 | Modus-Index, Default 0 `[H]` |

Für `trim*`/`fade*`/`pos` ist „Anteil 0…1" deutlich plausibler als dB oder
Halbton, weil die Werte exakt 0.0 und exakt 1.0 als Anschläge treffen und die
Visualizer-Anker direkt als 0…1-Positionen gezeichnet werden.

---

## 4. `stretch`: die gängige Annahme ist widerlegt  `[W]`

**Ausgangsvermutung** (aus `01-BESTANDSAUFNAHME.md` §6.1): `0.374989420` sei ein
normierter Wert „auf einer 0.25…8-Kurve", also vermutlich 1.0× logarithmisch
abgebildet.

**Messung:** `0.374989420` steht in **520 von 528** Slots (Default). Eine
logarithmische Abbildung über [0.25, 8] müsste den Faktor 1.0 bei

```
v(1.0) = ln(1.0/0.25) / ln(8/0.25) = ln 4 / ln 32 = 1.386294 / 3.465736 = 0.400000
```

ablegen. Gemessen ist **0.374989** — Abweichung **0.025**. Das ist um fünf
Größenordnungen mehr, als float32-Rundung erklären kann (ULP bei 0.375 ≈ 3 × 10⁻⁸).
Eine mittige Normierung (0.5 = 1.0×) ist mit 0.125 Abweichung noch weiter entfernt.

**Die logarithmische Deutung ist damit widerlegt.** Es bleiben zwei Möglichkeiten:

| Variante | Aussage | Konsequenz für den Nachbau |
|---|---|---|
| **(a)** Kurve ist log über [0.25, 8], aber der Default ist **nicht** 1.0× | Default = `0.916970×` | Presets klingen beim Import zu tief/kurz, wenn man 1.0× annimmt |
| **(b)** Kurve ist **keine** Logarithmus-, sondern eine Potenz-/Skew-Kennlinie; Default = 1.0× | `v = ((f − 0.25) / 7.75)^s` mit **s = 0.420000** (JUCE-Skew-Äquivalent 2.380952) | `f = 0.25 + 7.75 · v^(1/0.42)` |

Variante (b) reproduziert den Messwert **exakt** (der Exponent fällt auf 0.420000
bis auf 10⁻⁶ genau aus). Das ist bemerkenswert, weil s ein freier Parameter ist und
trotzdem auf einen runden Wert fällt — ein Indiz, kein Beweis: mit einem einzigen
Stützpunkt ist *jede* Potenzkurve anpassbar. Variante (a) bleibt zulässig und ist
sogar die konservativere Lesart, denn sie ändert nur die Default-Interpretation.

**Wichtig für die Parity:** In beiden Varianten ist `0.374989420` **nicht** 0.5 und
**nicht** 0.4 — wer „0.5 = keine Streckung" implementiert, liegt falsch. Die
Entscheidung erfordert **eine** Messung: im Original STRETCH auf `1.00×` stellen,
Preset speichern, den Wert ablesen (§7.1).

**Beobachtete Stützstellen** (alle 528 Slots): `0.374989420` (520×),
`0.176683307` (5×), `0.344057173` (2×), `0.408213377` (1×). Unter Variante (b)
entsprechen diese Faktoren `1.0000×`, `0.4613×`, `0.8238×`, `1.0292×`; unter
Variante (a) `0.9170×`, `0.4230×`, `0.7554×`, `0.9438×`. Keine der beiden
Reihen ist offensichtlich „runder" — die Frage bleibt bis zur Messung offen.

---

## 5. `filter` / `filterq`  `[H]`

Beide sind in **allen 528** Slots konstant — es gibt genau einen Stützpunkt.

* `lN_filter = 1.000000000` (Maximum). Ein Slot-Tiefpass, der „offen" ist, hat
  seine Grenzfrequenz am Maximum. Hypothese: normierte Grenzfrequenz, 1.0 = offen
  bzw. 20 kHz. Bereich/Kurve unbekannt `[O]`.
* `lN_filterq = 0.102564096`. Auffällig: `4/39 = 0.1025641026` liegt nur **7 × 10⁻⁹**
  daneben (eine float32-ULP), und `4/39` ist genau `(0.5 − 0.1)/(4.0 − 0.1)`.
  Das entspricht **Q-Bereich [0.1, 4.0] mit Default Q = 0.5** — ein für einen
  Slot-Filter sehr plausibler Wertebereich.

  ```
  filterq_normalised = (Q - 0.1) / 3.9        Q ∈ [0.1, 4.0], Default 0.5
  Q                  = 0.1 + 3.9 * filterq_normalised
  ```

  Der Schluss hängt an der Annahme „Default-Q = 0.5". Er ist damit **plausibel,
  nicht belegt** — aber die Übereinstimmung bis auf eine ULP ist stark. Gegenprobe
  mit anderen Bereichen liefert keine Kandidaten innerhalb 2 × 10⁻⁶ (geprüft:
  [0.1, 10], [0.3, 10], [0.5, 10], [0.1, 5], [0.2, 5], [0.1, 2], [0.5, 4]).

---

## 6. Offene Skalen: Kandidaten mit Zahlen

### 6.1 `lN_vol`  `[O]`

Default `0.699840486` (41 von 44 Slot-0-Werten; über alle Slots 11 verschiedene
Werte, min `0.0`, max `0.804556072`).

Drei unabhängige Lesarten:

| Lesart | Rechnung | Ergebnis |
|---|---|---|
| **Lineare Verstärkung 0…1** | `20·log10(0.699840486)` | **−3.10002 dB** Default; Maximum `0.80456` = −1.89 dB |
| **Lineare Verstärkung, 1.0 beim Default** | `gain = v / 0.699840486` | Max = **1.42890 (+3.10 dB)** |
| **dB-linear über [lo, hi]** | Default-DB = lo + 0.699840486·(hi−lo) | siehe Tabelle |

Die dB-Lesart mit „Default ist ein glatter dB-Wert" (Kandidaten, sortiert nach
Glattheit):

| Bereich | Default in dB | Abstand zur Ganzzahl |
|---|---|---|
| **[0, 10]** | **+6.998** | 0.0016 |
| [−25, +5] | −4.005 | 0.0048 |
| [−24, +6] | −3.005 | 0.0048 |
| [−30, +10] | −2.006 | 0.0064 |
| [−40, +10] | −5.008 | 0.0080 |
| [−50, +10] | −8.010 | 0.0096 |

Bemerkenswert: **`20·log10(Default) = −3.10002 dB`** und `dbToGain(−3.1) = 0.6998420`
— der Default liegt also fast exakt auf −3,1 dB. Das ist entweder die lineare
Verstärkung selbst oder ein starker Hinweis auf eine dB-Skala mit −3,1 dB Default.
**Beide Lesarten sind mit den Daten vereinbar**; sie unterscheiden sich klanglich
um bis zu 3 dB und müssen gemessen werden (§7.2).

Der Bereich `[0, 10]` dB ist der einzige Kandidat, der zum Handbuch-Hinweis „VOL
(0 dB … +10 dB)" passt **und** gleichzeitig erklärt, warum der Minimalwert `0.0`
vorkommt (0 % = 0 dB = „kein Boost").

### 6.2 `master_vol`  `[O]` — die Daten unterscheiden nicht

Default = `5/6` exakt, Maximalwert `1.0` (in 1 Preset genutzt).

* **Deutung A (dB-linear, Default = 0 dB):** Aus `lo + (5/6)(hi − lo) = 0` folgt
  `lo = −5·hi`. **Jedes** ganzzahlige `hi` erfüllt die Messung exakt — die Tabelle
  ist daher geometrisch und **kein** Beweis:

  | hi (dB) | lo (dB) | Spanne | Anmerkung |
  |---|---|---|---|
  | **+10** | **−50** | 60 dB | passt zum +10-dB-Motiv des Plugins |
  | +12 | −60 | 72 dB | |
  | +6 | −30 | 36 dB | |
  | +5 | −25 | 30 dB | |
  | +4 | −20 | 24 dB | |

* **Deutung B (lineare Verstärkung):** `gain = v / (5/6)`, d. h. Maximum
  `1.2` = **+1.58 dB**, Default = 0 dB. Ein Master-Fader, der nur +1,58 dB kann,
  wäre ungewöhnlich — Deutung A ist plausibler, aber nicht belegt.

* Reine Zahl: `20·log10(5/6) = −1.5836 dB`. Wer `master_vol` naiv als Amplitude
  liest, bekommt −1,58 dB statt 0 dB.

### 6.3 `lN_vol`-Einordnung in die Lautheitskette

`lN_vol` und `master_vol` haben **verschiedene** Defaults (`0.6998` vs `0.8333`) und
damit verschiedene Kurven. Sie dürfen nicht über dieselbe Formel laufen. Das ist
eine konkrete Anforderung an die Registry (B-11): **zwei getrennte Skalen-Objekte**.

### 6.4 Übrige offene Parameter

| Parameter | Datenlage | Kandidat |
|---|---|---|
| `sat_amount`, `hollow_amt`, `max_amount`, `reverb_mix` | immer 0.0 | Anteil 0…1 mit Default 0 `[H]` |
| `reverb_size`, `reverb_damp` | immer 0.5 | bipolar oder 50 %-Default `[O]` |
| `lN_filter` | immer 1.0 | log-Frequenz, 1.0 = offen `[H]` |
| `lN_fishape`, `lN_foshape` | immer 0.0 | Modus-Index (Enum), Default 0 `[H]` |
| `lN_dlymix` | immer 0.0 | Anteil 0…1 `[H]` |

---

## 7. Offene Messungen — konkretes Vorgehen

Alle Messungen laufen nach demselben Muster: **im Original einen Regler auf einen
bekannten Anzeigewert stellen → Preset als User-Preset speichern → normierten Wert
aus der XML ablesen.** Zwei bis fünf Stützpunkte genügen, weil die Kurvenform dann
durch die Kandidatenliste eindeutig wird.

> Hinweis: Factory-Presets sind schreibgeschützt — es muss ein **eigenes** Preset
> gespeichert werden. User-Presets liegen unter
> `…\Documents\Teknovault\The Teknovault Kick Builder\` und tragen zusätzlich
> Audiodaten; für die Kurvenmessung genügt das Auslesen der `<Params>`.

### 7.1 STRETCH — die wichtigste Messung (hohe Priorität)

Ein Sample in Slot 0 laden, STRETCH auf **0.50× / 1.00× / 2.00× / 4.00× / 8.00×**
stellen, je ein Preset speichern, `l0_stretch` ablesen.

* Liefert `1.00× → 0.374989420`, ist die Potenz-Kurve (b) aus §4 bestätigt
  (s = 0.42) und der Bereich ist [0.25, 8].
* Liefert `1.00× → 0.400000`, ist die Kurve logarithmisch und der Werksdefault
  bedeutet `0.9170×` — dann muss der Default beim Import entsprechend umgerechnet
  werden.
* Die fünf Punkte legen Kurvenform und Bereich gleichzeitig fest.

### 7.2 VOL / MASTER VOL — Pegelgesetz (hohe Priorität)

**a)** Master-Fader auf `0.0 dB` und auf Maximum stellen, je speichern, `master_vol`
ablesen. Damit ist der Bereich aus §6.2 eindeutig (`hi` bekannt).

**b)** Slot-VOL auf `0 dB`, `+3 dB`, `+10 dB` stellen und zusätzlich **den
Ausgangspegel messen** (Drag-Master-Export eines Ein-Slot-Kicks, Peak/RMS). Aus
`normierter Wert ↔ gemessene Verstärkung` folgt, ob die Skala dB-linear oder
lineare Amplitude ist — die reine XML-Ablesung kann das nicht entscheiden, weil
beide Lesarten dieselben Zahlen erzeugen können.

### 7.3 EQ-Skalare — die einzige unbestätigte Position (mittlere Priorität)

Die drei EQ-Frequenzen sind die **einzige** Stelle im FX-Bridge, an der die
Positionshypothese nicht numerisch bestätigt ist (siehe
[`fx-param-bridge.md`](fx-param-bridge.md) §6): ihre normierten Werte
`0.529 / 0.4771 / 0.7094` sind **nicht monoton** in der Frequenz
(90 / 900 / 8000 Hz) — bei einem gemeinsamen Frequenzbereich müssten sie
monoton steigen. Das beweist, dass die drei Bänder **unterschiedliche**
Frequenzbereiche haben.

Messung: LOWFREQ, MIDFREQ, HIGHFREQ und MIDQ jeweils auf Minimum und Maximum
stellen, Preset speichern. Aus je zwei Punkten folgen Bereich und Kurve pro Band.

### 7.4 PITCH — Skala (mittlere Priorität)

PITCH auf `+1`, `+5`, `+12` Halbtöne stellen, `l0_pitch` ablesen. Drei Punkte
entscheiden zwischen ±12, ±24 und einer nichtlinearen Skala.

### 7.5 Delay-Zeit (niedrige Priorität)

`DLYTIME` auf 10 % und 100 % stellen und die angezeigte Zeit (ms) notieren →
Einheit und Maximum. Erwartung: Maximum 1 s bzw. 1000 ms.

### 7.6 Priorisierung

| Rang | Messung | Warum |
|---|---|---|
| 1 | **STRETCH 1.00×** | entscheidet über die Kurvenform; betrifft jeden Slot jedes Presets |
| 2 | **VOL/Master-Pegelgesetz** | betrifft den Pegel jedes Kicks; zwei Lesarten unterscheiden sich um ~3 dB |
| 3 | **EQ-Frequenzen min/max** | einzige numerisch unbestätigte Position im FX-Bridge |
| 4 | PITCH-Skala | betrifft die Tonhöhe jedes gepitchten Parts |
| 5 | Delay, restliche FX-Bereiche | geringer Klangeinfluss bzw. schon rechnerisch eng eingegrenzt |

---

## 8. Anforderungen an die Parameter-Registry (B-11)

Aus diesem Dokument folgen unmittelbar:

1. **20 Slot-Parameter**, nicht 19 — `fistart` ergänzen (§1).
2. `root_note` als `note/127` modellieren, Default 36 (§3.1).
3. `pitch` mit **Mitte 0.5** modellieren, nicht mit 0 = neutral (§3.2).
4. `master_vol` mit Default **5/6** und eigener Skala, getrennt von `lN_vol` (§6.3).
5. `stretch` **nicht** als Logarithmus über [0.25, 8] implementieren (§4) — bis zur
   Messung den Default `0.374989420` als **opake Zahl** durchreichen und die
   Anzeige aus einer austauschbaren Kurvenfunktion ableiten.
6. Alle Werte als **float32** speichern und mit **9 Nachkommastellen** schreiben,
   damit Roundtrips byte-gleich bleiben (`0.500000060` ist eine ULP, keine 0.5).
7. `fx`-Werte mit **4 Nachkommastellen** schreiben (siehe `fx-param-bridge.md` §2).

---

## 9. Zusammenfassung

**Belegt:** `root_note = n/127` (36/127, 44/44 exakt) · `pitch`-Mitte = 0.5
(1 ULP) · `master_vol`-Default = 5/6 exakt · `dlytime`-Default = float32(0.3) ·
`mute`/`solo`/`midi_mode` als Booleans · `trim`/`fade`/`pos` als 0…1-Anteile.

**Widerlegt:** `stretch` ist **keine** Logarithmus-Kurve über [0.25, 8] mit 1.0×
bei 0.5 (Messwert 0.375, erwartet wären 0.4 bzw. 0.5) · `master_vol = −1,58 dB` ist
zwar arithmetisch richtig, aber **kein** Beleg für eine dB-Skala (5/6 ist exakt) ·
`pitch` liegt **nicht** durchgehend auf ganzen Halbtönen (1 von 6 Werten).

**Offen:** die absoluten Bereiche von `vol`, `stretch`, `filter`, `filterq` und die
Restkurven — alle in §7 mit einem Messrezept hinterlegt.
