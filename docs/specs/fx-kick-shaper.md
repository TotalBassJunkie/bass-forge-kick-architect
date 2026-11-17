# FX-Modul `shaper` — Kick Shaper (Ticket B-64)

**Typ:** `FxType::kickShaper`, XML-Kennung `shaper`, Anzeigename „Kick Shaper"
**Code:** `src/fx/modules/KickShaperModule.h`, `src/fx/modules/KickShaperModule.cpp`
**Tests:** `tests/KickShaperModuleTests.cpp` (13 Testfälle)
**Stand:** 2026-11-17 — vom Modul-Autor geschrieben; **Bauen und Ausführen macht der
Lead**. Alle im Text genannten Zahlen sind aus den hier definierten Formeln
gerechnet, nicht am laufenden Binär gemessen.

---

## 1. Zweck und Abgrenzung

Der Kick Shaper formt den **Amplitudenverlauf eines Kicks über eine Beat-Länge**.
Kern ist eine Hüllkurve aus Stützpunkten (`<Pt>`): jeder Punkt trägt eine Position
entlang der Kick-Länge und einen Ziel-Amplitudenfaktor. Bei jedem Trigger wird die
Hüllkurve von Punkt 0 an in Echtzeit abgefahren und multipliziert das Signal.

| Teil | Wirkung |
|---|---|
| `sPre` | linearer Eingangsgain vor der Umformung (0…3,5) |
| `sPost` | linearer Ausgangsgain nach der Umformung (0…3,5) |
| `sLine` | „lineare Sicherung": 1 = Umformung hart aus, 0 = Umformung an |
| `shape` | Segmentmodus der Interpolation zwischen zwei `<Pt>`-Punkten (0…7) |
| `amount` | blendet die Umformung ein (0 % = bitgenaues Durchreichen) |

**Nicht** im Modul: `MIX` und `OUT GAIN`. Beide gehören laut Kettenvertrag
(`fx-chain-engine.md`, Abschnitt 3) zur Kette. Das Modul liefert ausschließlich
sein zu 100 % nasses Signal. Ebenfalls **nicht** im Modul: ein Limiter/Ceiling —
`sPost` darf bis +10,9 dB anheben; begrenzt wird erst im Finalizer der Kette.

**Latenz: 0 Samples.** Das Modul ist ein reiner Verstärker mit Hüllkurve: keine
Verzögerung, kein Lookahead, keine Filter.

---

## 2. Parameter

`params()` liefert genau diese fünf Einträge in dieser Reihenfolge; der Index ist
die Nummer für `setParam()`/`getParam()` und die spätere Zuordnung zu
`fx<N>_p<M>`:

| # | `id` | Name | min | max | Default | Einheit | Bedeutung |
|---|---|---|---|---|---|---|---|
| 0 | `sPre` | Pre Gain | 0 | 3,5 | **1** | x | linearer Eingangsgain |
| 1 | `sPost` | Post Gain | 0 | 3,5 | **1** | x | linearer Ausgangsgain |
| 2 | `sLine` | Linear | 0 | 1 | **1** | – | ≥ 0,5 = lineare Sicherung (Bypass der Umformung) |
| 3 | `shape` | Shape | 0 | 7 | **0** | – | Segmentmodus, Abschnitt 5 |
| 4 | `amount` | Amount | 0 | 100 | **100** | % | blendet die Umformung ein |

`setParam()` klemmt auf `[minValue, maxValue]` und verwirft NaN/Inf; ein
ungültiger Index ist ein No-op, `getParam()` liefert dort `0.0f`. Die Bereiche
entsprechen genau dem Ticket; die Referenzdaten zeigen `sPre = sPost = 1`,
`sLine = 1`, `shape` 0…2, `amount` 0…100 (`fx-param-bridge.md`, §6 und §8).

---

## 3. Signalfluss und DSP

```
Eingang (Summe)
  │
  ├─ sPre        x = in · sPre                       (linear, 0…3,5)
  │
  ├─ Umformung   shaped = x · env(phase)             env = Hüllkurvenwert 0…1
  │              y = x + a · (shaped − x)            a = amount/100 (geglättet)
  │
  └─ sPost       out = y · sPost                     (linear, 0…3,5)
Ausgang
```

* `env` ist ein **reiner Amplitudenfaktor** (0…1), kein dB-Wert. `env = 1` heißt
  „unverändert".
* `a = amount/100` wird mit 20 ms geglättet (Abschnitt 9); `sPre` und `sPost`
  ebenfalls.
* `y = x + a·(shaped − x)` ist die zahlengleiche Differenzform von
  `out = in·(1−a) + shaped·a`. Sie ist an beiden Enden exakt: bei `a = 0` und bei
  `env = 1` (weil dann `shaped − x = x − x = 0` ist). Das ist die Grundlage der
  Bitgenauigkeit in Abschnitt 8.
* `sLine ≥ 0,5` setzt `a` **hart** auf 0 (Sicherung); `amount` wird dann
  ignoriert, `sPre`/`sPost` wirken weiter.

**Kanalbehandlung:** alle Kanäle bekommen **dieselbe** Hüllkurve und denselben
Trigger (gemeinsamer Detektor = Maximum der Kanalbeträge). Identisches Signal auf
L/R bleibt damit bitgleich; ein Einschlag auf nur einem Kanal formt beide.

---

## 4. Struktur-Zustand: Hüllkurve als `<Pt>`-Kinder

### 4.1 Schema

```xml
<Module type="shaper" on="1" mix="1.0" outGain="1.0"
        sPre="1" sPost="1" sLine="1" shape="0" amount="100">
  <Pt x="0"    y="0.15" t="0"/>
  <Pt x="0.35" y="1"    t="0"/>
  <Pt x="1"    y="0.4"  t="0"/>
</Module>
```

| Attribut | Bedeutung | Bereich (beim Einlesen geklemmt) |
|---|---|---|
| `x` | normierte Position entlang der Kick-Länge | 0…1 |
| `y` | Ziel-Amplitudenfaktor | 0…1 (1 = neutral) |
| `t` | Segmenttyp des Segments **ab diesem Punkt** | 0…3, Belegung Abschnitt 6 |

### 4.2 Default und Grenzen

* **Ohne `<Pt>`-Kinder** gilt die eingebaute Werkskurve aus **zwei Punkten**
  `(0 | 1)` und `(1 | 1)`, beide `t = 0`. Sie ist über die ganze Länge
  **neutral**: `env = 1` exakt (die Interpolation zwischen zwei gleichen Werten
  ergibt unabhängig vom Segmentmodus genau diesen Wert).
* **Höchstens `maxPoints = 32` Punkte.** Weitere Punkte werden verworfen (die
  ersten 32 in Dokumentreihenfolge zählen).
* `getState()` schreibt je Punkt ein `<Pt x y t/>` **in der Reihenfolge der
  gespeicherten Punkte**; die Skalare stehen zusätzlich als Attribute im
  `<Module>` (Anzeige-Einheiten, ganze Zahlen als Integer, krumme auf 6
  Nachkommastellen gerundet).
* `setState()` liest `<Pt>`-Kinder, **ignoriert unbekannte Kinder** und klemmt alle
  Werte. Fehlende `<Pt>`-Kinder setzen die Werkskurve.
* Für die Auswertung wird eine nach `x` **sortierte Kopie** gebaut; Punkte mit
  (nahezu) gleichem `x` (Abstand ≤ 1e-9) fallen zusammen, der zuletzt gelesene
  gewinnt. Der gespeicherte Zustand bleibt unsortiert (verlustfreier Round-Trip
  für bereits sortierte Referenzdaten).

---

## 5. Die acht Segmentmodi (`shape`)

Zwischen zwei benachbarten Punkten `a = (x_a, y_a)` und `b = (x_b, y_b)` gilt mit
`u = (x − x_a) / (x_b − x_a) ∈ [0, 1]`:

```
y(x) = y_a + (y_b − y_a) · f(u)
```

| `shape` | Name | `f(u)` | Eigenschaften |
|---|---|---|---|
| 0 | **Halten** | `0` | Wert des Startpunkts steht bis zum nächsten Punkt |
| 1 | **Lineare Rampe** | `u` | Gerade |
| 2 | **Rampe mit Spiegelung** | `1 − \|2u − 1\|` | Dreieck: Zielwert in der Segmentmitte, Startwert am Ende |
| 3 | **Stufen** | `0` für `u < 1`, `1` bei `u = 1` | Sprung auf den Zielwert am Segmentende |
| 4 | **Exponentielle Annäherung** | `(1 − e^(−k·u)) / (1 − e^(−k))`, `k = 3` | normiert (`f(0) = 0`, `f(1) = 1`), schnell am Anfang, flach am Ende |
| 5 | **Halten + Spiegel** | `0` für `u < 0,5`, sonst `(2u − 1)²` | erste Hälfte auf dem Startwert, zweite Hälfte eine gespiegelte (beschleunigende) Rampe |
| 6 | **Halbwellen-Rampe** | `sin(u·π/2)` | steil am Anfang, flach am Ende |
| 7 | **Invertierte Rampe** | `1 − u` | Wert wandert „vom Ziel zum Start" |

Alle Modi sind deterministisch, gedächtnislos und liefern `f(u) ∈ [0, 1]`; damit
bleibt `y(x)` immer zwischen `y_a` und `y_b`, also in 0…1. Außerhalb des
Punktbereichs wird auf den Randwert geklemmt.

**Bekannte Entartung:** `shape = 0` (Halten) und `shape = 3` (Sprung am
Segmentende) beschreiben bis auf den exakten Randpunkt dieselbe Funktion — der
Sprung auf `y_b` liegt genau dort, wo der nächste Punkt ohnehin mit `y_b`
beginnt. Beide Modi sind trotzdem getrennt implementiert und dokumentiert, weil
das Ticket beide verlangt (offener Punkt O-K2).

---

## 6. Punkt-Segmenttyp `t` (Belegung)

`t` steht am **Startpunkt** eines Segments und gilt für dieses Segment. `t = 0`
ist die Vorgabe und der Grund, warum der globale Regler `shape` bei
Werkshüllkurven überhaupt wirkt.

| `t` | Name | Wirkung auf das Segment |
|---|---|---|
| 0 | **linear / normal** | Segment folgt dem globalen `shape`-Modus (mit `shape = 1` exakt eine lineare Rampe) |
| 1 | **halten** | Wert bleibt auf `y_a`, `shape` wird ignoriert |
| 2 | **Rampe/Spiegel** | wie 0, aber mit gespiegeltem Fortschritt: `f(1 − u)` |
| 3 | **Stufe** | Sprung auf `y_b` am Segmentende, `shape` wird ignoriert |

**Bewusste Abweichung von der Ticket-Formulierung:** Das Ticket nennt `t = 0` als
„linear". Wäre `t = 0` eine *lokale* lineare Rampe, dann wäre der globale Regler
`shape` bei jeder Hüllkurve mit `t = 0`-Punkten (also immer) wirkungslos. Deshalb
ist `t = 0` hier „normal/folgt `shape`" (und `shape = 1` ist die lineare Rampe);
`t = 1…3` sind harte lokale Overrides. Die Belegung der drei Overrides entspricht
dem Ticket (halten / Spiegel / Stufe).

---

## 7. Trigger und Kick-Länge

### 7.1 Flanken-Erkennung

* Detektor: **Eingangssummenpegel** `level = max(|L|, |R|)` — gemessen **vor**
  `sPre`, damit die Schwelle unabhängig vom Eingangsgain greift.
* **Schwelle:** `kTriggerHighLevel = 0,01` (−40 dBFS).
* **Hysterese:** re-armiert wird erst unter `kTriggerLowLevel = 0,0031623`
  (−50 dBFS), also 10 dB unter der Triggerschwelle. Damit löst ein einzelner
  Kick genau einen Trigger aus und nicht mehrere.
* **Sperrzeit `kRearmHoldSeconds = 0,01` s** (10 ms Stille): Erst wenn der Pegel
  so lange unter der Hysterese-Schwelle bleibt, wird neu armiert. Ohne diese
  Sperre fällt ein **gehaltenes** Signal (Sinus, DC-Kick, langer Rumble) bei
  jedem Nulldurchgang unter die Schwelle; die Hüllkurve würde dann zweimal pro
  Periode neu starten und die Umformung wäre praktisch unhörbar
  (Befund beim Bau der Tests, siehe auch Test „amount 100 formt das Signal“).
* Bei jedem Triggerstart wird die Hüllkurven-Position **hart auf 0** gesetzt.
* Liegt kein Trigger an, **parkt** die Uhr bei Position 0 (der Wert des ersten
  Punkts wirkt dann auch auf leise Vorlauf-Samples). Nach dem Ende der Länge
  bleibt die Uhr bei 1 stehen, bis sie re-armiert wird.

### 7.2 Kick-Länge

* Default `kDefaultRenderLengthSeconds = 0,5` s; änderbar über die öffentliche
  Methode `setRenderLength (double seconds)`, geklemmt auf
  0,05 s … 10 s (`kMinRenderLengthSeconds` / `kMaxRenderLengthSeconds`).
* Der Fortschritt pro Sample ist `1 / (length · sampleRate)`; die Position läuft
  von 0 nach 1 und wird dort geklemmt.
* Die Länge wird **einmal pro Block** aus dem atomaren Wert gelesen — eine
  Änderung wirkt also ab dem nächsten Block, ohne Sprung im Sample.

Beispiel (48 kHz, 0,5 s): 24 000 Samples pro Durchlauf, d. h. nach 6 000 Samples
steht die Uhr auf Position 0,25.

---

## 8. Neutralität und Sicherung

| Fall | Verhalten | Begründung |
|---|---|---|
| `amount = 0` | **bitgenau identisch** zum Eingang (bei `sPre = sPost = 1`) | `blend = 0` ⇒ die Umformung wird übersprungen; `·1,0f` ist exakt |
| Werkshüllkurve (kein `<Pt>`) | **bitgenau identisch**, auch bei `amount = 100` | `env = 1` exakt ⇒ `shaped − x = 0` |
| `sLine ≥ 0,5` | **bitgenau identisch** (nur `sPre`/`sPost` wirken) | harte Sicherung, `amount` ignoriert |
| `sPre ≠ 1` oder `sPost ≠ 1` | linearer Gain, sonst neutral | Definition des Parameters |

Der Bypass-Nulltest der Kette (−120 dBFS) ist damit für die drei Fälle oben
erfüllt; ein „heißes" Signal > 0 dBFS läuft bei `amount = 0` ungehindert durch
(im Rack fängt es der Finalizer).

---

## 9. Threading, Latenz, CPU

| Punkt | Wert |
|---|---|
| Latenz | **0 Samples** (`latencySamples() == 0`) |
| `process()` | kein `new`, kein `log`, kein Mutex, kein Sperren, kein `std::vector`-Wachstum |
| Parameterübergabe | `std::atomic<float>`/`<int>`/`<bool>`/`<double>` (relaxed), Lesevorgang pro Block |
| Glättung | `amount`, `sPre`, `sPost` mit 20 ms (One-Pole, `kParamSmoothMs`) |
| Zustand im Audio-Thread | 3 `float` (Glättung) + 1 `double` (Uhr) + 1 `bool` (Trigger) |
| Rechenaufwand je Sample | 1 Pegel-Maximum, 1 Hüllkurven-Auswertung (≤ 32 Punktvergleiche), 3 Glättungen, 2 Multiplikationen |
| Struktur-Zustand | `getState()`/`setState()`/`setEnvelopePoints()`/`setRenderLength()` auf dem Message-Thread; nicht gleichzeitig mit `process()` aufrufen (wie bei allen Modulen des Racks) |

Die Hüllkurven-Auswertung ist bewusst **nicht** vorberechnet (keine Tabelle): die
Punktzahl ist klein (≤ 32) und die Segmentmodi hängen vom Live-Parameter `shape`
ab, der ohne Neu-Backen umschaltbar bleiben soll.

**Diagnose (öffentlich, atomar, nur Tests/Doku):**
`currentEnvelopePosition()`, `currentEnvelopeTarget()`, `currentShapeAmount()`,
`currentPreGain()`, `currentPostGain()`, `isTriggerActive()`,
`preparedSampleRateHz()`, `blockSize()`.
**Analyse-Helfer:** `evaluateEnvelope (double position)` und
`evaluateEnvelopeForShape (int shapeMode, double position)` liefern den
Hüllkurvenwert direkt aus der gespeicherten Punktfolge (gedächtnislos, ohne DSP).

---

## 10. Tests (`tests/KickShaperModuleTests.cpp`, Catch2 v3)

| Testfall | Prüft |
|---|---|
| Parametertabelle und Metadaten | `type()`, `displayName()`, `latencySamples() == 0`, 5 Parameter, IDs/Namen/Bereiche/Defaults/Einheiten exakt, `shape`-Max = 7, keine Ketten-Parameter (`mix`/`outGain`/`on`) |
| setParam klemmt | `setParam(4, 500) → 100`, `setParam(0, −1) → 0`, `setParam(3, 99) → 7`, `setParam(1, 10) → 3,5`, ungültige Indizes, NaN-Abweisung |
| `amount = 0` bitgenau | Impuls und Sinus, `sLine` 0 und 1, 94 Blöcke, heißes Signal (Spitze 2,5): maximale Differenz exakt 0,0f |
| `amount = 100` formt | maximale Differenz > 0,01; Fenster bei Position 0,25 messbar leiser (Faktor ≈ 0,575) |
| `sLine` neutral | `sLine` 1 und 0,5 bitgenau; 0,49 formt wieder; `sPre`/`sPost` = 2 wirken als Faktor 4 |
| `<Pt>`-Roundtrip | 4 Punkte `setState` → `envelopePoints()` → `getState()` (x/y/t, Toleranz 1e-6); leerer Zustand = Werkskurve; Skalare als Attribute |
| 32 Punkte / Klemmen | 40 Punkte → 32; x/y/t geklemmt; unbekannte Kinder ignoriert |
| die acht Segmentmodi | (a) `evaluateEnvelopeForShape` gegen die dokumentierte Formel an zwei Stützstellen je Modus, (b) Referenzwerte (linear 0,4/0,6; halten 0,2; Spiegel 1,0; invertiert 0,8), (c) DC-Eingang: `out[n] = env(n)`, Probe bei Position 0,25 gegen die Formel (Toleranz 1e-5), mindestens zwei Modi unterscheiden sich > 1e-4 |
| Trigger | Block über −40 dBFS startet bei Position 0 (< 0,01); Stille re-armiert (Position exakt 0); Block unter der Schwelle triggert nicht; danach erneut Start bei 0 |
| `setRenderLength` | Default 0,5 s; 1 s + 2 400 Samples → Position 0,05; Klemmen 0,05…10 s; NaN verworfen |
| Default-Hüllkurve neutral | ohne `<Pt>`, `sLine = 0`, `amount = 100`: maximale Differenz exakt 0,0f |
| Stabilität/Grenzfälle | 100 Blöcke Rauschen mit allen Reglern am Anschlag (endlich), Stille exakt 0, DC bleibt DC, Mono-Buffer, 1-/7-Sample-Block, leerer Block, Aufruf ohne `prepare()` |
| Latenz 0 | vor/nach `prepare()`, bei anderen Blockgrößen und 96 kHz |

---

## 11. Abweichungen und offene Punkte

| # | Punkt | Warum wichtig | Vorschlag |
|---|---|---|---|
| **O-K1** | **Keine echten Referenzdaten für die Hüllkurvenform.** In den 639 ausgewerteten Modulinstanzen kommt `shaper` 6× vor, aber mit **0 `<Pt>`-Kindern** (`fx-param-bridge.md`, §7). | Klangtreue | Die Werkskurve ist deshalb bewusst **neutral** (`y = 1`) und die Form entsteht ausschließlich aus Nutzer-Punkten. Sobald Referenz-Snapshots mit `<Pt>` vorliegen: Formen übernehmen, Default ggf. nachziehen (eine Stelle: `makeDefaultEnvelope()`). |
| **O-K2** | `shape = 0` (Halten) und `shape = 3` (Stufe) sind bis auf den Randpunkt dieselbe Funktion (Abschnitt 5). | „8 verschiedene Modi" | So belassen (das Ticket definiert beide so); falls im Hörtest eine echte Treppe gewünscht ist, `shape = 3` auf 4 Stufen (`floor(4u)/4`) umstellen — eine Zeile in `shapeBend()`. |
| **O-K3** | `t = 0` bedeutet hier „folgt `shape`" statt „lineare Rampe" (Abschnitt 6). | Ohne diese Lesart wäre der globale `shape`-Regler bei Default-Hüllkurven wirkungslos. | Alternative: `t = 0` als feste lineare Rampe und `shape` nur für Punkte mit `t` als „global" markiert — würde eine zusätzliche Kodierung im `t`-Attribut brauchen. |
| **O-K4** | Der Trigger-Pegel wird **vor** `sPre` gemessen. | Ein sehr leiser Kick + hoher `sPre` triggert nicht. | Falls das Original nach dem Gain triggert: Messpunkt in `process()` um eine Zeile verschieben (dokumentiert an genau einer Stelle). |
| **O-K5** | `amount` ist im Ticket als „blendet die Umformung ein" beschrieben; die Blendformel ist `y = x + a·(x·env − x)`. | Interpretation | Im Hörtest bestätigen; alternativ wäre eine reine Ausgangspegel-Skalierung denkbar. |
| **O-K6** | Kein Limiter/Ceiling im Modul. | `sPost` bis 3,5 (+10,9 dB) kann über 0 dBFS laufen. | So belassen (Kettenvertrag: MIX/OUT GAIN und Begrenzung gehören zur Kette/Finalizer). |
| **O-K7** | CPU-Budget (< 3 % je Modul) hier nicht gemessen — kein Echtzeit-Host in dieser Umgebung. | Abnahmekriterium aus dem Masterplan | Im Host mit 10 Instanzen bei 48 kHz/128 nachmessen. |
| **O-K8** | Registrierung in der Registry ist Aufgabe des Leads (`FxFactory::registerType`); dieses Ticket ändert `FxFactory.cpp` und `CMakeLists.txt` **nicht**. | Reihenfolge im Start-Up | Siehe Abschnitt 13. |

---

## 12. Parameter-Bridge (`fx<N>_p<M>`)

Maßgeblich ist `docs/specs/fx-param-bridge.md` **§6** (Sonderfall `shaper`). Für
`shaper` sind nur **zwei** Parameter gebrückt:

| Bridge | Attribut | Umrechnung | Beleg |
|---|---|---|---|
| `p0` | `sPre` | `p0 = sPre / 3.5` (Bereich 0…3,5) | §6: `1.0 / 3.5 = 0.285714` → `0.2857` ✔ (Lesart „`p0 = sPre`" ist die konsistentere von zwei gleich guten) |
| `p1` | `amount` | `p1 = amount / 100` | §6: `31.2542 / 100 = 0.3125` exakt ✔ |
| – | `sPost`, `sLine`, `shape` | **nicht** gebrückt | §6: `p2 = p3 = p4 = 0.0`, obwohl `sPost = sLine = 1.0` und `shape = 2.0` — diese Attribute stehen nur im Snapshot |
| `p12` | `mix` | `1.0` (Kette) | §4.1 |
| `p13` | `outGain` | `outGain / 2` (Kette) | §4.2 |

**Konsequenz für Import/Export:** `sPost`, `sLine`, `shape` und die `<Pt>`-Liste
kommen **ausschließlich** aus dem eingebetteten `<FxChain>`-Snapshot; die
`fx`-Proxy-Parameter tragen nur `sPre` und `amount`. Wer die Hüllkurve aus den
`fx`-Parametern rekonstruiert, verliert sie vollständig (§7).

---

## 13. Einbindung (Zeilen für den Lead)

Dieses Ticket legt **nur** die vier Dateien an und ändert kein CMake und keine
Registry. Einzutragen sind:

`CMakeLists.txt` (in `BFKA_FX_SOURCES`, alphabetisch vor `MultibandModule.cpp`):

```cmake
    src/fx/modules/KickShaperModule.cpp        # NEU (B-64)
```

`CMakeLists.txt` (in `BFKA_HEADERS`, nach `src/fx/modules/FiltersModule.h`):

```cmake
    src/fx/modules/KickShaperModule.h          # NEU (B-64)
```

`tests/CMakeLists.txt` (`target_sources(BFKA_Tests PRIVATE …)`):

```cmake
    KickShaperModuleTests.cpp                  # NEU (B-64)
    ../src/fx/modules/KickShaperModule.cpp     # NEU (B-64)
```

`src/fx/FxFactory.cpp`:

```cpp
#include "fx/modules/KickShaperModule.h"
...
registerType (FxType::kickShaper, [] { return std::make_unique<KickShaperModule>(); });
```

Damit ersetzt das Modul die bisherige `PassThroughModule`-Platzhalter-Registrierung
für `FxType::kickShaper`.

---

## Nachtrag 2026-11-17 (B-92b, B-147)

* **UI:** src/ui/editors/KickShaperView.{h,cpp} (B-92b). Zeigt die Hüllkurve aus
  <Pt> als ziehbare Punkte, die tatsächliche Kurve über
  evaluateEnvelope() (129 Stützstellen), Segmenttyp je Punkt (	, Rechtsklick),
  Abspielmarke an currentEnvelopePosition() und die Statuszeile
  LEN <ms> / POS / ENV. Schreiben über setEnvelopePoints() +
  PluginProcessor::writeBackModuleStates().
* **Kick-Länge (B-147):** Der Prozessor setzt die Länge jetzt aus dem Renderer
  (KickRenderer::kickLengthSeconds() = 60/bpm bei KICK LENGTH 1 BEAT, sonst die
  längste belegte Voice) — nach jedem Ketten-Rebuild, nach einem neuen Kick und
  nach pplyState() (dort geht das Tempo aus dem Zustand an den Renderer).
  Abnahme: 	ests/RenderLengthTests.cpp.