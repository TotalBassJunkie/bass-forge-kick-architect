# Time-Stretching — WSOLA und Resampling (Spezifikation)

**Ticket:** B-40 (WSOLA-Kern), B-42 (`KEEP PITCH` vs. `REPITCH`), Grundlage für B-41
(Parameterstudie), B-43 (BPM-Stretch), B-44 (Pitch-Shift), B-45 (Render-Cache), B-46
(Qualitätsmetriken), B-47 (Grenzfälle)
**Code:** `src/engine/WsolaStretcher.h/.cpp`, `src/engine/Resampler.h` (header-only)
**Tests:** `tests/WsolaTests.cpp` (23 Testfälle)
**Stand:** 2026-11-17 — Quelltext nach Review fertig, **noch nicht übersetzt** (kein
Compiler in dieser Session); die Kennzahlen in Abschnitt 9 sind Sollwerte für B-46.

> **Lizenzhinweis.** Das Original nutzt intern Rubber Band und eine eigene
> WSOLA-Engine. Wir implementieren **ausschließlich** die eigene Engine
> (Verhelst/Roelands, 1993) und verwenden **keine** Fremdbibliothek. Es wurde kein
> Code, keine Formelsammlung und kein Parameterdatensatz aus dem Original übernommen;
> die Parameter sind aus der Aufgabenstellung und der DSP-Literatur abgeleitet.

---

## 1. Zweck und Abgrenzung

Ein Techno-Kick wird in zwei **voneinander unabhängigen** Achsen verändert
(Bestandsaufnahme 5.13, Masterplan Punkt 1):

| Achse | Wirkung | Verfahren | Faktor |
|---|---|---|---|
| **Länge** (`STRETCH`) | Länge ändern, Tonhöhe halten | WSOLA | 0,25 … 8,0 |
| **Tonhöhe** (`PITCH`) | Tonhöhe ändern, Länge exakt halten | Resampling + WSOLA-Kompensation (B-44) | −12 … +12 HT |
| **`REPITCH`-Modus** | beides gemeinsam | Resampling | 0,25 … 8,0 |

`WsolaStretcher` deckt die erste und die dritte Zeile ab. `PITCH` (zweite Zeile) ist
ebenfalls hier verankert: die Pitch-Stufe von B-44 setzt auf `Resampler` auf und
kompensiert die dabei entstehende Längenänderung mit WSOLA.

`process()` ist ein **Offline-Renderer-Baustein** (wie `CompositeRenderer`): es läuft
nie im Audio-Callback, darf allokieren und darf die Zielpuffergröße bestimmen.

---

## 2. Signalführung

```
Slot-Sample (Quellrate)
   │
   ├── STRETCH-Modus KEEP PITCH ────────────────────────────────────────────┐
   │      WsolaStretcher::process (…, keepPitch = true)                    │
   │      Grain-Suche → Hann-Fenster → Overlap-Add → Energienormalisierung │
   │                                                                       │
   ├── STRETCH-Modus REPITCH ───────────────────────────────────────────────┤
   │      WsolaStretcher::process (…, keepPitch = false)                    │
   │      Anti-Alias-Vorfilter (nur beim Verkürzen) → Interpolation         │
   │                                                                        │
   └── PITCH (B-44) ────────────────────────────────────────────────────────┤
          Resampler (Ratio 2^(HT/12)) → WSOLA mit 1/ratio zur Längenneutralität
                                                                            ▼
                                                              Ausgabe in Zielrate
```

---

## 3. Algorithmus (WSOLA), Schritt für Schritt

WSOLA synthetisiert das Signal aus **Körnern (Grains)** fester Länge. Jedes Korn wird
an einer *festen* Ausgabeposition platziert, aber an einer *gesuchten* Quellposition
gelesen. Die Suche wählt unter mehreren Kandidaten denjenigen, dessen Überlappungszone
am besten zum vorherigen Korn passt. Dadurch bleiben benachbarte Körner phasenkohärent,
und es entstehen keine Klicks oder Auslöschungen.

### 3.1 Arbeitsgrößen

| Symbol | Bedeutung | Herleitung im Code |
|---|---|---|
| `F` | Framegröße (Analysefenster) | `frame = min(settings.frameSize, Eingabelänge)` |
| `O` | Überlappung | `overlap = clamp(settings.overlapSize, 1, F/2)` |
| `H` | Synthese-Hop | `H = F − O` |
| `R` | Suchbereich | `radius = clamp(settings.searchRadius, 0, F)` |
| `f` | Streckfaktor (Ausgabe/Eingabe) | geklemmt auf 0,25 … 8,0 |

### 3.2 Hann-Fenster

Periodisches Hann-Fenster der Länge `F` (Nenner `F`, nicht `F−1`, damit sich
Nachbarfenster bei `H = F/2` exakt zu 1 addieren):

```
w[n] = 0,5 − 0,5 · cos( 2π · n / F )        n = 0 … F−1
```

Eigenschaften: `w[0] = 0`, `w[F/2] = 1`, symmetrisch. Das Fenster wird **nur auf der
Analyseseite** angewandt; die Synthese normiert stattdessen durch die Summe der
tatsächlich aufgetragenen Gewichte (Abschnitt 3.5). Damit ist die Verstärkung
unabhängig von `O` exakt 1 — auch bei 25 % Überlappung, wo die Fenstersumme sonst
zwischen 1 und ~1,5 schwanken würde.

### 3.3 Zeitabbildung

Die Ausgabe besteht aus `K = ceil(outLen / H)` Körnern. Korn `k` wird an der
Ausgabeposition

```
outPos(k) = k · H
```

platziert. Die **nominale** Quellposition folgt der linearen Abbildung

```
p_nom(k) = round( k · H / f )
```

`p_nom` wird bewusst **aus `k` berechnet** und nicht aufsummiert: kein
Rundungsdrift über tausende Körner. `p(0) = 0` — der Anschlag bleibt an Position 0.

### 3.4 Referenzfenster (Template)

Korn `k−1` belegt im Ausgang `[outPos(k−1), outPos(k−1)+F)`. Seine Überlappungszone
(der letzte `O`-Block) stammt aus dem Quellbereich

```
T_k = mono[ p(k−1) + H … p(k−1) + H + O )
```

Genau dieser Abschnitt ist die **Referenz**: Wenn das neue Korn in seiner
Überlappungszone *dasselbe* Signal liefert, ist die Kreuzblende transparent —
beide Körner schreiben identisches Material, es gibt keine Auslöschung und keine
Kammfiltereffekte. Gesucht wird also eine Quellposition `p`, für die
`mono[p … p+O)` möglichst gut zu `T_k` passt.

### 3.5 Ähnlichkeitssuche (normierte Kreuzkorrelation)

Für jeden Kandidaten-Offset `d ∈ [−R, +R]` mit Kandidatenposition `c = p_nom(k) + d`:

```
                     Σ_{n<O}  T[n] · mono[c+n]
ncc(d) = ─────────────────────────────────────────────────────
          sqrt( Σ_{n<O} T[n]² · Σ_{n<O} mono[c+n]² )
```

`ncc ∈ [−1, +1]`; `+1` = identische Wellenform, `0` = unkorreliert (z. B. Stille),
`−1` = Gegenphase. Die Normierung sorgt dafür, dass **laute Abschnitte nicht
bevorzugt** werden (reine Kreuzkorrelation würde immer die energiereichste Stelle
wählen, also z. B. den Kick-Anschlag mehrfach kopieren).

Die Energien `Σ mono[c+n]²` kommen aus Präfixsummen `S[m] = Σ_{i<m} mono[i]²`:

```
Σ_{n<O} mono[c+n]² = S[c+O] − S[c]           (O(1) je Kandidat)
```

**Laufzeitbindung (`tolerance`).** Freie Suche wandert mit der Zeit von der linearen
Zeitabbildung weg — hörbar als „Schmieren" und für `ALIGN` (B-50) fatal, weil der
Anschlag dann nicht mehr an der erwarteten Stelle liegt. Deshalb wird jeder Offset
bestraft:

```
score(d) = ncc(d) − τ · (d / R)²              τ = settings.tolerance ∈ [0, 1]
```

`τ = 0` → freie Suche (maximale Ähnlichkeit, maximale Zeitverzerrung).
`τ = 1` → ein Offset muss fast eine perfekte Korrelation liefern, um sich gegen die
Nominalposition durchzusetzen (praktisch zeitstarr). Vorgabe `τ = 0,2`: ein Sprung
über den halben Suchbereich kostet 0,05 Punkte — das ist etwa der Unterschied
zwischen „perfekt phasenrichtig" und „eine Periode daneben".

Bewusst **keine harte Schranke**, sondern eine Bestrafung: eine harte Schranke
erzwingt bei Überschreitung einen Sprung, der als Klick hörbar ist.

**Zweistufige Suche.** Bei `R = 512` sind das 1025 Kandidaten; jede Bewertung kostet
`O` Multiplikationen. Deshalb:

1. **Grob** auf dem Raster `G = max(1, ⌊R/32⌋)` (bei 512 → 16): Start bei `d = 0`,
   dann symmetrisch `±G, ±2G, …` → 65 Bewertungen.
2. **Fein** sampleweise im Intervall `(d* − G, d* + G)` → ~30 Bewertungen.

Bei Gleichstand gewinnt der **kleinere Betrag** (Reihenfolge der Bewertung ist
Teil der Determinismus-Zusage). Rechenersparnis ~10× bei praktisch identischem
Ergebnis; die Grobstufe ist fein genug, dass eine Periode typischer Kick-Bässe
(20–80 Hz → 600–2400 Samples bei 48 kHz) nicht übersprungen wird, sobald der
Suchbereich entsprechend gewählt ist (siehe Abschnitt 4, offener Punkt).

### 3.6 Overlap-Add mit Energienormalisierung

Für jedes Korn werden die Samples gewichtet in einen Akkumulator addiert, parallel
läuft die Summe der Gewichte:

```
acc[i]  += w_k(i − outPos(k)) · x[p(k) + i − outPos(k)]      für alle Kanäle
norm[i] += w_k(i − outPos(k))
```

Ausgegeben wird

```
out[i] = acc[i] / norm[i]        für norm[i] > 10⁻¹²
out[i] = 0                       sonst
```

Das ist die **exakte** Umkehrung der Fensterung: Bei `f = 1` ist `acc[i] = norm[i]·x[i]`,
also `out[i] = x[i]` bis auf Gleitkommarundung (Test: maximale Abweichung < 10⁻⁴,
RMS-Abweichung < 1 %). Aus demselben Grund ist die Verstärkung für **jedes** `O`
exakt 1 — kein Pegelsprung beim Umschalten der Überlappung.

Der Nenner ist der einzige Ort, an dem „durch etwas Kleines geteilt" wird. Da
`acc` ausschließlich aus `w·x`-Termen entsteht, ist `acc/norm` für `norm → 0` immer
noch `x` (kein Rauschen wird verstärkt). Unterhalb von 10⁻¹² wird hart auf 0 gesetzt:
**kein NaN, kein Inf**, auch bei digitaler Stille (Test).

### 3.7 Transientenschutz

Das Hann-Fenster startet bei 0 — ein ungefenstertes erstes Korn würde den Anschlag
eines Kicks um einige Samples „ausblenden". Deshalb bleibt der **Kopf des ersten
Korns ungefenstert**:

```
w_0(n) = 1,0           für n < H
w_0(n) = w[n]          sonst
w_k(n) = w[n]          für k ≥ 1
```

Da Korn 1 erst bei `outPos = H` beginnt, überlappt dieser flache Kopf nichts. Die
Normierung ist an dieser Stelle ohnehin 1, der Anschlag kommt also **samplegenau**
heraus (Test: Impuls bei Sample 512 bleibt bei 512, Amplitude unverändert; bei
`f = 2` entsteht **kein** zweiter Impuls).

### 3.8 Ränder und kurze Eingaben

* Die Quelle wird mit **Nullrand** der Breite `R + F + O + 8` beidseitig kopiert.
  Damit darf die Suche über Anfang und Ende hinausschauen, ohne Sonderfälle im
  Kern. Ein Korn, das über das Ende hinausliest, liefert Nullen — das ist ein
  natürliches Ausklingen, kein Fehler.
* `F` wird auf die Eingabelänge begrenzt (`F = min(frameSize, numIn)`), `O` auf
  `F/2`. Für ein 100-Sample-Signal wird also mit `F = 100, O = 50, H = 50` gerechnet.
* Eingaben unter 8 Samples haben kein verwertbares Fenster; sie laufen über den
  `Resampler` (Notpfad). Bei < 8 Samples ist „Tonhöhe" bedeutungslos, der Pfad ist
  deshalb auch für `KEEP PITCH` zulässig.
* Eingaben mit 0 Samples ergeben eine Ausgabe mit 0 Samples.

### 3.9 Länge der Ausgabe

```
outLen = round( eingabeLen · clamp(f, 0,25, 8) )        (mind. 1 Sample)
```

Kaufmännisch gerundet, nicht abgeschnitten: `2,0×` aus 24 000 Samples ergibt exakt
48 000, `0,5×` exakt 12 000. Die Länge ist über `WsolaStretcher::outputLengthFor()`
vorab abfragbar — der Offline-Renderer braucht sie für Puffer- und Beat-Raster
(`KICK LENGTH`, B-35).

---

## 4. Parameter und ihre Wirkung

Vorgaben aus `WsolaSettings` (Angaben in Samples bei Quellrate):

| Parameter | Vorgabe | Wirkung | zu klein | zu groß |
|---|---|---|---|---|
| `frameSize` `F` | 2048 (≈ 46 ms @ 44,1 kHz) | Länge des Analysefensters; bestimmt die Zeitauflösung | Transienten werden verschmiert, Bässe verlieren Grundton-Kohärenz; Suchkosten sinken | Zeitauflösung leidet, kurze Kicks werden „weichgespült"; Suchkosten steigen |
| `overlapSize` `O` | 512 (25 %) | Länge der Kreuzblende; `H = F−O` | hörbare Klicks/Phasensprünge, besonders im Sub | mehr Rechenzeit, stärkere Verschmierung von Transienten |
| `searchRadius` `R` | 512 | wie weit die Phase maximal korrigiert werden darf | Suche findet die passende Periode nicht → Auslöschungen im Bass | Rechenzeit; ohne `tolerance` driftet die Zeitabbildung |
| `tolerance` `τ` | 0,20 | Gewicht der Laufzeitbindung (Abschnitt 3.5) | Zeitdrift, „Schmieren", Anschlag wandert | Suche bleibt starr → Kammfilter, Doppel-Transienten |

### 4.1 Empfehlungen

* **Sub-Bass / Rumble (Kernfall):** `F = 2048`, `O = 1024` (50 %), `R = 512`. Das
  Kreuzblendenfenster (23 ms @ 44,1 kHz) ist dann länger als eine Periode von
  40 Hz — Voraussetzung dafür, dass die Suche die Phase überhaupt sinnvoll
  einrasten kann.
* **Klick/Punch (kurz, transient):** `F = 1024`, `O = 512`, `R = 256`.
* **Extremfaktoren (`f > 4` oder `f < 0,4`):** `R ≥ 2 · H / f` wählen, sonst kann die
  Suche die Periode nicht erreichen. Das ist ein Kandidat für die Parameterstudie
  B-41 (siehe Abschnitt 11).

### 4.2 Abweichung von „50 % Überlappung" (Entscheidung für den Lead)

Die Aufgabenstellung nennt „Hann-Fenster, **50 %-Überlappung**", die vorgegebene
Struktur `WsolaSettings` setzt aber `frameSize = 2048` **und** `overlapSize = 512` —
das sind 25 %. Beides ist nicht gleichzeitig Vorgabe.

Entscheidung im Code:

* `WsolaSettings` bleibt **exakt wie vorgegeben** (Vorgabe = 25 % Überlappung).
* Der Algorithmus ist generisch: `H = F − O`, jede Überlappung von 1 Sample bis
  `F/2` funktioniert, die Normierung ist in allen Fällen exakt.
* Für die klassische 50-%-Konfiguration gibt es
  `WsolaStretcher::fiftyPercentOverlap()`; die Tests nutzen sie zusätzlich.
* **Empfehlung an den Lead:** für Kick-Bässe 50 % verwenden, also
  `overlapSize = frameSize/2`. Wenn B-41 das bestätigt, sollte der Vorgabewert in
  der Struktur auf 1024 geändert werden — das ist eine Ein-Zeilen-Änderung und
  betrifft nur die Vorgabe, nicht die Rechnung.

---

## 5. Verhalten bei den Extremfaktoren

### 5.1 `0,25×` (Viertel der Länge, 4× so schnell gelesen)

* `H / f = 4 · H`: die Quelle wird viermal schneller durchlaufen als die Ausgabe
  wächst. Aus 2 s Kick werden 0,5 s.
* Körner springen weit; die Suche kann nur innerhalb `±R` korrigieren, das sind bei
  `H = 1536` und `f = 0,25` rund 8 % des Quellvorschubs. Die Korrektur gelingt
  deshalb nur für tiefe Anteile, deren Periode klein gegen `R` ist; höhere Anteile
  bleiben phasenunkorreliert und klingen „hohl".
* Inhalt wird **verworfen** (jedes vierte Korn entfällt). Ein 0,25×-Kick klingt
  kurz und hart — für `ALIGN`/`STRETCH CRUNCH` (B-52) ist das gewollt.
* Der Anschlag bleibt erhalten (Transientenschutz, Abschnitt 3.7).

### 5.2 `8×` (Achtfache Länge)

* `H / f = H / 8`: die Quelle wird achtmal langsamer durchlaufen, jedes Quellstück
  wird also ~8× wiederholt. Bei `H = 1536` sind das 192 Samples Vorschub je Korn.
* Die Suche hat relativ viel Freiheit (`R/hopIn ≈ 2,7`), rastet also gut ein; das
  Ergebnis bleibt phasenkohärent, klingt aber „gehalten" (kein neuer Inhalt).
* Risiko: bei stark moduliertem Material (Rauschen) wird die Wiederholung als
  metallisch hörbar — die 8-fache Länge eines One-Shots wird praktisch immer als
  Effekt gehört. Für den Kick-Bau ist 8× ein Sound-Design-Werkzeug, kein
  Korrekturfaktor.
* Speicher: die Ausgabe ist 8× so lang; bei 2 s Quellmaterial sind das 16 s. Die
  Notbremse `maxOutputLength = 2^26` Samples (≈ 23 min @ 48 kHz) verhindert
  Überläufe.

### 5.3 Außerhalb des Bereichs

`f` wird auf `[0,25; 8,0]` geklemmt; `NaN`/`Inf` fallen auf `1,0` zurück.
`outputLengthFor()` und `process()` verwenden **dieselbe** Klemmfunktion, damit die
vorab berechnete Länge und die tatsächliche Ausgabe nie auseinanderlaufen
(Test: `f = 0,01` → 0,25×).

---

## 6. Warum die Grain-Suche kanalgekoppelt ist

Für die Suche wird **ein** Signal verwendet, die Summe aller Kanäle:

```
mono[n] = (1/C) · Σ_c x_c[n]
```

Der gefundene Offset `d` wird auf **alle** Kanäle gemeinsam angewandt; es gibt
keine kanalweise Suche und keine kanalweise Normierung.

Begründung — ein Rechenbeispiel mit einem typischen Kick-Bass:

* Ein 50-Hz-Sinus hat bei 48 kHz eine Periode von 960 Samples.
* Würde man die Kanäle **getrennt** suchen, könnten L und R auf zwei verschiedene
  Phasen einrasten, z. B. Offset `d_L = 0` und `d_R = 480` (halbe Periode).
* Ergebnis: `L` und `R` liegen bei 50 Hz **in Gegenphase**. Für den Hörer bricht der
  Bass zusammen — im Bassbereich ist der Interauralzeitunterschied ohnehin unhörbar,
  der Summenpegel entscheidet. Aus einem Mono-kompatiblen Kick wird ein
  ausgelöschter Kick, und im Club (Mono-Sub) ist er ganz weg.
* Zusätzlich würde eine getrennte Normierung (Abschnitt 3.6) die Pegelverhältnisse
  zwischen L und R verbiegen — die Stereobreite des Kicks würde mit dem Streckfaktor
  wandern.

Konsequenzen dieser Entscheidung:

1. **Stereobreite bleibt erhalten.** Beide Kanäle werden mit denselben Gewichten und
   denselben Positionen verarbeitet; das Verhältnis L:R bleibt pro Sample gleich.
2. **Mono-Quellen** (`C = 1`) sind der Sonderfall `mono = x`.
3. **Identische Kanäle bleiben identisch** (bitgleich), weil auf beiden Kanälen
   dieselben Operationen auf denselben Werten laufen (Test ≤ 10⁻⁹, praktisch 0).
4. **Gegenphasige Kanäle** (`L = −R`) haben `mono = 0`; dann fehlt die
   Korrelationsgrundlage, alle Energien sind ~0 und die Suche fällt auf die
   Nominalposition zurück (`d = 0`). Das ist das richtige Verhalten: bei perfekter
   Gegenphase gibt es kein sinnvolles Ähnlichkeitsmaß. In der Praxis kommt das nur
   bei bewusst invertierten Layern vor (PHASE ALIGN, B-56, kann Polarität flippen).
5. Grenzfall `numChannels > 2`: die Summe mittelt automatisch; für 5.1-Material wäre
   eine gewichtete Summe (LFE ausgenommen) korrekt — offener Punkt, Abschnitt 11.

---

## 7. `KEEP PITCH` vs. `REPITCH`

| | `KEEP PITCH` | `REPITCH` |
|---|---|---|
| Verfahren | WSOLA (Körner + Suche + OLA) | Interpolation (`Resampler`) |
| Länge | `f ×` | `f ×` |
| Tonhöhe | **unverändert** | `1/f ×` |
| Rechenweg | 2 Speicherdurchläufe, Suchkosten | 1 Durchlauf, O(1) je Sample |
| Klang bei `f > 1` | „länger, gleiche Tonhöhe" — Material wird wiederholt | „langsamer, tiefer" — wie halbe Bandgeschwindigkeit |
| Klang bei `f < 1` | „kürzer, gleiche Tonhöhe" — Material wird verworfen | „schneller, höher" |
| Typischer Einsatz | `STRETCH` (Vorgabe), `STRETCH PUNCH` (B-52), BPM-Sync | Sound-Design, „Tape-Stop"-Effekte, `REPITCH`-Modus |

### 7.1 Beispiel `REPITCH`, `f = 2,0`

Sinus 300 Hz, 1 s bei 48 kHz (48 000 Samples):

```
Eingang : 48 000 Samples, 300 Hz
Ausgang : 96 000 Samples, 150 Hz
```

Der Resampler liest die Quelle mit halber Geschwindigkeit (`position = i / 2`), es
entstehen also doppelt so viele Samples mit halbierter Frequenz — eine Oktave
tiefer. Bei einem Kick: Anschlag wird weicher und tiefer, Länge verdoppelt.
(Test: FFT-Peak 150 Hz ±3 %, bestätigt über Nulldurchgänge.)

### 7.2 Beispiel `KEEP PITCH`, `f = 2,0`

Sinus 220 Hz, 1 s:

```
Eingang : 48 000 Samples, 220 Hz
Ausgang : 96 000 Samples, 220 Hz
```

Der Quellvorschub ist `H/2 = 768` Samples je Korn, die Ausgabe wächst um
`H = 1536`. Material wird also doppelt so oft gelesen wie geschrieben. Die Suche
sorgt dafür, dass benachbarte Körner phasenrichtig überblenden (bei 220 Hz liegt
eine Periodenlänge der Suche gut im Bereich `R = 512`).
(Test: FFT-Peak 220 Hz ±3 %.)

### 7.3 Beispiel BPM-Sync (B-43)

`BPM STRETCH / SYNC BPM`, Faktor = `eigenes BPM / Kick-BPM` (aus dem Dateinamen,
Fallback 150; Bestandsaufnahme Befund 3). Ein 150-BPM-Punch (400 ms) auf ein
160-BPM-Projekt:

```
f = 160 / 150 = 1,0667   →   400 ms · 1,0667 = 426,7 ms
```

`KEEP PITCH` ist hier Pflicht: der Punch soll nur ins Raster passen, nicht tiefer
klingen. `REPITCH` wäre ein 1,07-fach tieferer Punch — bei kleinen Faktoren (unter
±2 %) unhörbar, bei größeren Abweichungen klanglich falsch.

---

## 8. Reproduzierbarkeit und Rechenbudget

**Determinismus.** Es gibt keinen Zufall, keine Zeit, keine Threads und keine
Iteration über Container mit nicht festgelegter Reihenfolge. Gleitkommaoperationen
laufen immer in derselben Reihenfolge, Positionen werden aus `k` berechnet (nicht
aufsummiert), Gleichstände in der Suche werden durch die Bewertungsreihenfolge
aufgelöst. Ergebnis: gleiche Eingabe + gleicher Faktor = **bitgleiche** Ausgabe
(Test). Der `Resampler` startet jeden `process()`-Aufruf mit frischem Filterzustand,
damit auch der REPITCH-Pfad diese Zusage hält.

**Rechenbudget** (grobe Abschätzung, 48 kHz, Stereo, `F = 2048`, `O = 1024`):

| Fall | Körner | Suchbewertungen | MACs | Größenordnung |
|---|---|---|---|---|
| 1 Beat @ 150 BPM (400 ms), `f = 1` | 19 | 0 (Suche aus) | < 0,1 M | < 1 ms |
| 1 Beat, `f = 2` | 38 | 38 × ~95 | ~3,7 M | 1–3 ms |
| 8 s Kick, `f = 8` | 3000 | 3000 × ~95 | ~290 M | 100–300 ms |

Damit liegt der Kern innerhalb des Abnahmekriteriums von Phase 4 („1-Beat-Kick
< 30 ms"), **sofern** gerendert wird, wenn der Audio-Callback nicht läuft — was der
asynchrone Vorab-Renderer mit Cache (B-45) ohnehin vorsieht.

**Speicher** je Aufruf (Stereo): Quellkopie `2·(n + 2·(R+F+O+8))` Floats,
Akkumulator `2·outLen` Floats, Normierung `outLen` Floats, Präfixsummen
`(n + 2·pad + 1)` Doubles. Für einen 2-s-Kick bei `f = 8` (16 s Ausgabe) sind das
rund 25 MB — für einen Offline-Renderer unkritisch, im Audio-Thread verboten.

---

## 9. Messmetriken (B-46) — was wir prüfen werden

Die Testsuite prüft Korrektheit (Länge, Tonhöhe, Stille, Determinismus). Die
**Klangqualität** wird getrennt gemessen; Vorschlag für B-46:

| Metrik | Definition | Zielwert | Messmethode |
|---|---|---|---|
| **Transientenerhalt** | Position und Amplitude des globalen Peaks nach dem Stretchen | Position auf ±1 Sample, Amplitude ±0,5 dB | Impuls- und Punch-Sample, Peak-Suche; zusätzlich Hüllkurven-Flanke (10 % → 90 % Anstieg) vor/nach |
| **Doppel-Transienten** | Energie im Fenster [+5 ms, +40 ms] nach dem Anschlag im Verhältnis zum Anschlag | < −20 dB (kein zweiter „Schlag") | Impuls-Test, Fenster-Energie |
| **Spektrale Verzerrung** | Abstand der Langzeit-Spektren (1/3-Oktav, 20 Hz…16 kHz) vor/nach dem Stretchen | < 1,0 dB je Terz, Summe < 2 dB | `juce::dsp::FFT`, Welch-Mittelung |
| **Tonhöhenfehler** | Abweichung des Grundtons (`KEEP PITCH`) | < 0,5 % (≈ 8 Cent) | FFT-Peak + parabolische Interpolation, Nulldurchgangs-Gegenprobe |
| **Phasenkohärenz L/R** | Korrelationskoeffizient L/R nach dem Stretchen gegen den Wert davor | Abweichung < 0,05 | `Σ L·R / sqrt(Σ L²·Σ R²)` |
| **Bass-Erhalt (Mono-Summe)** | Pegel der Mono-Summe bei 40–80 Hz vor/nach | < 1 dB Abweichung | Bandpass + RMS |
| **Aliasing (REPITCH)** | Energie oberhalb `f · fs/2` beim Hochstimmen, relativ zum Nutzsignal | < −60 dB | Spektrum bei `f = 0,25` mit Sinus-Sweep |
| **Klickfreiheit** | max. Sample-Differenz zwischen Nachbarsamples (2. Ableitung) gegen Referenz | keine Ausreißer > 6 σ | Differenz-Signal |
| **Ähnlichkeitsmaß** | `meanSimilarity()` der Engine | > 0,9 bei tonalem Material | Engine-Diagnose |

Die Engine liefert für die letzten beiden Punkte bereits Werte:
`lastSimilarity()` (letzte Suche) und `meanSimilarity()` (Mittel des Aufrufs). Ein
dauerhaft niedriges `meanSimilarity` bei tonalem Material ist ein direktes Indiz für
einen zu kleinen `searchRadius` oder ein zu großes `tolerance`.

---

## 10. Testfälle (`tests/WsolaTests.cpp`)

| Testfall | Prüft |
|---|---|
| Faktor 1,0 unverändert | Länge gleich; RMS-Abweichung < 1 %; max. Sample-Differenz < 10⁻⁴ |
| Faktor 2,0 / 0,5 Länge | Länge samplegenau (48 000 / 12 000), Toleranz 5 % zur Anforderung |
| 50-%-Konfiguration | `fiftyPercentOverlap()` liefert `O = 1024`, Ausgabe plausibel |
| `REPITCH` 2,0 | Länge ×2 **und** dominante Frequenz halbiert (FFT + Nulldurchgänge, ±3 %) |
| `KEEP PITCH` 2,0 | Länge ×2 **und** dominante Frequenz gleich (±3 %) |
| `KEEP PITCH` 0,5 | Tonhöhe bleibt ebenfalls erhalten |
| Stereo identisch | L/R nach dem Stretchen bitgleich (≤ 10⁻⁹), auch im REPITCH-Pfad |
| Stereo 90° | Korrelation L/R bleibt um 0 (±0,1) — keine Phasenzerstörung |
| Stille | Faktor 0,25/0,5/1/2/8 × beide Modi: alle Samples exakt 0, keine NaN/Inf |
| Klemmung | `f = 0,01` → 0,25; `f = 100` → 8,0; Länge plausibel |
| Extremfaktoren | 0,25 und 8: Länge, Endlichkeit, Pegel plausibel |
| Kurze Eingaben | 0, 1, 2, 7, 8, 40 Samples, beide Modi: kein Absturz, definierte Länge |
| Impuls 1,0 | Peak bleibt bei Sample 512 mit Amplitude 1,0 |
| Impuls 2,0 | kein Doppel-Transient (einziger Ausschlag < 0,05) |
| Determinismus | zwei Instanzen und Wiederholung auf derselben Instanz → bitgleich |
| Diagnose | `lastSimilarity`/`meanSimilarity` in [0,1], Mittel > 0,5 bei Sinus |
| Resampler | Identität bei Ratio 1, Längen, linear vs. kubisch, leere Eingabe |

---

## 11. Offene Punkte und mögliche Verbesserungen

1. **Parameterstudie (B-41) fehlt noch.** Die Vorgaben (2048/512/512/0,2) sind
   Startwerte. Zu klären: `O = 1024` (50 %) als Vorgabe? `searchRadius` soll für
   `f < 0,4` mitwachsen (`R ≥ 2·H/f`), sonst bleibt die Phase unkorrigiert.
2. **Transienten-Erkennung.** Ein Onset-Detektor könnte Körner an erkannten
   Transienten **sperren** (kein Suchfenster über den Anschlag, kein Wiederholen).
   Damit ließen sich Doppel-Transienten bei `f > 1` konstruktiv ausschließen statt
   nur statistisch zu vermeiden. Kandidat für eine zweite Stufe.
3. **Phase-Vocoder-Hybrid.** WSOLA ist bei stark tonalem Material mit langen
   Perioden (Sub-Bass unter 40 Hz) und bei `f < 0,5` im Nachteil. Ein Hybrid — bis
   ~200 Hz Phase-Vocoder mit Phasenfortschreibung, darüber WSOLA — ist der
   klassische Weg. Aufwand: eigener STFT-Zweig (`Stft` existiert im Original, bei
   uns noch nicht). Erste Stufe bleibt WSOLA; der Hybrid ist die Rückfalllinie,
   falls die Abnahme „Stretch 0,5×/2× ohne hörbare Doppel-Transienten" nicht
   erreicht wird (Masterplan, Risikotabelle).
4. **Suche im Frequenzbereich.** Die Kreuzkorrelation läuft derzeit im Zeitbereich
   (~95 Bewertungen × `O` MACs). Eine FFT-basierte Suche wäre bei großem `R`
   schneller, lohnt aber erst, wenn der Cache (B-45) die Renderzeit nicht mehr
   dominiert.
5. **Blockweise/Streaming-Betrieb.** `process()` ist offline und puffergenau. Für
   eine Echtzeit-Vorschau (Play on Roll) bräuchte es einen zustandsbehafteten Kern
   mit Ringpuffer, Verwaltung von `previousPos` und Crossfade-Zustand über Blockgrenzen.
   Bewusst **nicht** Teil dieses Tickets.
6. **Kanalgewichtung.** Bei mehr als zwei Kanälen ist die einfache Summe
   (`mono = Σ/C`) nicht optimal: der LFE-Kanal (5.1) würde die Suche dominieren.
   Vorschlag: Gewichte je Kanal, LFE ausnehmen.
7. **Anti-Alias im REPITCH-Pfad** ist ein zweistufiger Biquad (24 dB/Okt) — gut
   genug für Sprache und Kicks, aber kein Polyphasen-FIR. Bei `f ≤ 0,3` bleibt ein
   Restaliasing; messbar in der B-46-Metrik „Aliasing". Verbesserung: halbband-
   basierte Dezimierung oder ein 64-Tap-FIR.
8. **`tolerance` kalibrieren.** Der Wert 0,2 ist eine begründete Wahl, aber nicht
   gemessen. B-41 sollte ihn gegen die Metriken aus Abschnitt 9 variieren
   (0 … 0,5) und den Wert festlegen, bei dem `ALIGN` (B-50) den Anschlag noch
   zuverlässig trifft.
9. **Kubische Interpolation bei `f = 1`.** Catmull-Rom ist bei `t = 0` exakt (Test),
   bei Zwischenwerten leicht wellig im Ultraschall. Für Kicks irrelevant; für
   späteres Hi-Hat-Material ggf. durch einen bandbegrenzten Interpolator ersetzen.

---

## 12. Schnittstelle (Kurzfassung)

```cpp
namespace bfka {

struct WsolaSettings {
    int   frameSize    = 2048;
    int   overlapSize  = 512;    // max. frameSize/2
    int   searchRadius = 512;
    float tolerance    = 0.2f;
};

class WsolaStretcher {
public:
    static constexpr double minFactor = 0.25;
    static constexpr double maxFactor = 8.0;

    void prepare (double sampleRate, int numChannels, const WsolaSettings& = {});
    void reset();

    void process (const juce::AudioBuffer<float>& input,
                  juce::AudioBuffer<float>& output,
                  double factor,
                  bool keepPitch);

    float lastSimilarity() const noexcept;
    float meanSimilarity() const noexcept;

    static double clampFactor (double factor) noexcept;
    static int    outputLengthFor (int inputLength, double factor) noexcept;
    static WsolaSettings fiftyPercentOverlap (int frameSize = 2048,
                                              int searchRadius = 512,
                                              float tolerance = 0.2f) noexcept;
};

class Resampler {                       // header-only, src/engine/Resampler.h
public:
    void reset (int numChannels);
    void process (const juce::AudioBuffer<float>& in,
                  juce::AudioBuffer<float>& out, double ratio);
    void processToLength (const juce::AudioBuffer<float>& in,
                          juce::AudioBuffer<float>& out, int outputLength);
    static int outputLengthFor (int inputLength, double ratio) noexcept;
    void setQuality (ResamplerQuality);          // linear | cubic (Vorgabe: cubic)
    void setAntiAliasEnabled (bool);             // Vorgabe: true
};

}
```

**Konventionen:** `factor` und `ratio` sind immer *Ausgabe/Eingabe* (identisch zu
`SampleVoice::stretchRatio`, „>1 = länger"). `process()` darf allokieren, darf
`output` dimensionieren und läuft nie im Audio-Thread. `in` und `out` dürfen
derselbe Puffer sein (wird intern abgefangen).

**Fehlerverhalten:** keine Exceptions, keine Zusicherungen im Hot-Path. Ungültige
Parameter werden geklemmt, leere Puffer ergeben leere Ausgaben, Stille ergibt exakt
0 (kein NaN/Inf). Logging ausschließlich über `log::d(...)` — eine Zeile je Aufruf,
nichts in der Kornschleife.
