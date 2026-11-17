# Spec: Datenmodell und Regel-Engine des Style-Randomizers

**Tickets:** B-70 (Parser) · B-71 (Regel-Semantik)
**Stand:** 2026-11-17
**Code:** [`src/library/StyleModel.h`](../../src/library/StyleModel.h),
[`src/library/StyleModel.cpp`](../../src/library/StyleModel.cpp),
[`src/library/RandomizerEngine.h`](../../src/library/RandomizerEngine.h),
[`src/library/RandomizerEngine.cpp`](../../src/library/RandomizerEngine.cpp)
**Tests:** [`tests/StyleModelTests.cpp`](../../tests/StyleModelTests.cpp),
[`tests/RandomizerEngineTests.cpp`](../../tests/RandomizerEngineTests.cpp)

> Dieses Dokument legt fest, **welche Bedeutung** den Feldern aus `genres.json`
> in unserem Nachbau zugewiesen wird — mit Begründung, Konfidenz und offenen
> Fragen. Es ist die verbindliche Grundlage für B-72…B-77 (Ausschlussregel,
> `duplicateGain`/`fileWeights`, No-Repeat, Pflicht-/Optional-Sichtbarkeit,
> statistischer Abgleich).

---

## 1. Datenquelle und Ist-Stand

Quelle (nur lesend, **nicht** ins Repo übernommen):
`C:\Users\Public\Documents\Teknovault\The Teknovault Kick Builder\genres.json`,
`schemaVersion 3`, Revision **R33 (28.09.2026)**, 23.293 B.

| Posten | Ist-Wert in der Datei | Anmerkung |
|---|---|---|
| `schemaVersion` | 3 | wird gelesen, aber nicht ausgewertet |
| `roleRegistry` | 8 Rollen | punch, punch_enhancer, click, rumble, crunch, rumble_enhancer, schranz_enhancer, top |
| `pools[]` | **29** | Auftrag nennt 25 — R33 hat vier ZAAG-/NEWSTYLE-Pools ergänzt (PUN-ZAG, CRN-ZAG, CLK-ZAG, TOP-NEW) |
| `styles[]` | **9** | Auftrag nennt 10 — der zehnte Sidebar-Eintrag ist **FREE BUILD** und hat keinen `id`-Eintrag in der Datei |
| `rules[]` (global) | 1 | `type: "exclude"`, `a: melodic`, `b: atonal`, `scope: kick` |
| `kitPattern`, `categoriesRequired` | vorhanden | für den Randomizer irrelevant, wird ignoriert |
| `comment`-Felder | an Pool, Style und Wurzel | **werden nicht ins Modell übernommen** (reine Redaktionsnotizen) |

Rollenverteilung der 29 Pools: punch 8, crunch 5, rumble 5, punch_enhancer 5,
click 2, top 2, rumble_enhancer 1, schranz_enhancer 1.
Tonalität: neutral 16, atonal 8, melodic 5 (melodisch sind genau die fünf
Crunch-Pools).

---

## 2. JSON-Struktur, die das Modell kennt

| Ebene | Feld | Typ | Modell-Feld | Verwendet von |
|---|---|---|---|---|
| Wurzel | `schemaVersion` | int | — | ignoriert |
| Wurzel | `roleRegistry` | Objekt (Rolle → Info) | `StyleModel::roleRegistry()` (nur die Schlüssel) | UI/Diagnose |
| Wurzel | `rules[]` | Array | `StyleModel::exclusions()` | Engine (Ausschlussregel) |
| Wurzel | `kitPattern`, `categoriesRequired`, `comment` | — | — | ignoriert |
| Pool | `id` | String | `PoolDef::id` | Schlüssel für alles Weitere |
| Pool | `folder` | String | `PoolDef::folder` | SampleLibrary (B-20/B-21) |
| Pool | `role` | String | `PoolDef::role` | Engine |
| Pool | `tonality` | `neutral`/`melodic`/`atonal` | `PoolDef::tonality` | Engine (Ausschluss) |
| Pool | `defaultGain` | Zahl (0…1, **linear**) | `PoolDef::defaultGain` | Engine (Startpegel) |
| Pool | `duplicateGain` | Zahl (0…1, **linear**) | `PoolDef::duplicateGain` | Engine (2. Belegung) |
| Pool | `fileWeights` | Objekt Dateiname → Zahl | `PoolDef::fileWeights` | Engine (Sample-Wahl) |
| Pool | `uiOrder` | int | `PoolDef::uiOrder` | UI (Sortierung) |
| Pool | `comment` | String | — | absichtlich verworfen |
| Style | `id` | String | `StyleDef::id` | `RollRequest::styleId` |
| Style | `display` | String | `StyleDef::display` | UI |
| Style | `short` | String | `StyleDef::shortName` | UI |
| Style | `accent` | `#RRGGBB` | `StyleDef::accent` (`juce::Colour`) | UI |
| Style | `pools` | Objekt poolId → int | `StyleDef::poolWeights` | Engine (Pool-Gewicht) |
| Style | `layerRules` | Objekt Rolle → `{min,max}` | `StyleDef::layerRules` | Engine (harte Grenzen) |
| Style | `rules[]` | Array | `StyleDef::rules` | Engine (Kern der Auswahl) |
| Style | `addable[]` | Array poolId | `StyleDef::addable` | UI (`+ ADD PART`), **nicht** der Roller |
| Style | `defaultPreset` | String | `StyleDef::defaultPreset` | Preset-Auswahl |
| Style | `comment` | String | — | absichtlich verworfen |
| Regel | `code` | String | `StyleRule::code` | Engine (siehe §3.3) |
| Regel | `pools[]` | Array poolId | `StyleRule::pools` | Engine |
| Regel | `db` | `[min,max]` (dB) | `StyleRule::db` | Engine (Pegel 1. Part) |
| Regel | `dbPair` | `[min,max]` (dB) | `StyleRule::dbPair` | Engine (Pegel 2. Part) |
| Regel | `shares[]` | `[{pools[], pct}]` | `StyleRule::shares` | Engine (Verteilung) |

**Parser-Verhalten:** unbekannte Felder werden ignoriert, fehlende Felder mit
Defaults gefüllt, es wird nie eine Exception geworfen. Strukturelle Fehler
(kein JSON-Objekt, `pools`/`styles` fehlt oder ist kein Array) liefern `false`
plus deutsche Meldung, und das zuvor geladene Modell bleibt unverändert
(„nie halb geladen“). Weiche Auffälligkeiten (unbekannte Rolle, doppelte
`id`, Regel ohne `pools`) landen in `StyleModel::warnings()`.

**Bewusste Mapping-Entscheidungen**

* `punch_enhancer` → `Role::punchEnhancer`, `rumble_enhancer` → `Role::rumbleEnhancer`,
  `schranz_enhancer` → `Role::schranzEnhancer` — über das bestehende
  `roleFromString()` aus `core/KickState.h`, damit es nur **eine** Rollen-Tabelle gibt.
* `defaultGain`/`duplicateGain` sind **linear** (0.7 ⇒ −3.1 dB), nicht dB.
  Belegt durch `TOP-NEW` in NEWSTYLE: die Regel setzt `db: [-3,-3]`, und der
  Pool-Startpegel 0.7 der Punch-Enhancer entspricht ebenfalls ≈ −3 dB.
* Unbekannte `role` ⇒ `Role::none` (Pool bleibt im Modell, wird aber von keiner
  Regel erreicht); unbekannte `tonality` ⇒ `neutral`.
* `defaultGain <= 0` oder nicht-finit ⇒ 1.0 (kein stiller Stummschalter).
* Fehlt `max` in `layerRules`, gilt `max = min`.
* `uiOrder` fehlt ⇒ 0. R33 setzt nur bei den vier neuen Pools 1 bzw. 2; die UI
  sortiert deshalb sinnvollerweise `uiOrder == 0` **nach** die explizit
  nummerierten Pools (nicht in dieser Schicht gelöst).

---

## 3. Gewählte Semantik der Regel-Felder

Konfidenz-Skala: **hoch** = durch Daten und Handbuch gedeckt,
**mittel** = durch Datenlage erzwungen, aber nicht bewiesen,
**niedrig** = Arbeitsannahme, muss messtechnisch geprüft werden.

| Feld | Semantik | Konfidenz |
|---|---|---|
| `layerRules.role.min` | Pflichtanzahl dieser Rolle; `> 0` ⇒ mindestens so viele Parts | hoch |
| `layerRules.role.max` | harte Obergrenze dieser Rolle | hoch |
| `pools` (Style) | Gewicht der Pool-Wahl innerhalb einer Regel; `0` sperrt | hoch |
| `rules[].code` | Tier-Folge `[T]` oder `[T, T2]` (siehe §3.3) | mittel |
| `rules[].db` | Pegelbereich (dB, uniform) für den **ersten** Part der Regel | mittel |
| `rules[].dbPair` | Pegelbereich (dB) für den **zweiten** Part der Regel | mittel |
| `rules[].shares` | Prozentverteilung innerhalb der Regel, ersetzt die `pools`-Gewichte | mittel |
| `defaultGain` | Startpegel eines Parts, wenn die Regel keinen `db`-Bereich hat | mittel |
| `duplicateGain` | Pegelfaktor für jede **weitere** Belegung desselben Pools im Kick | mittel |
| `fileWeights` | relatives Gewicht einzelner Dateien, Präfix-Match | mittel |
| globale `rules[]` | Ausschlussregel „melodic × atonal“ pro Kick | hoch |

### 3.1 `layerRules` — harte Grenzen

`min`/`max` sind **die** verbindlichen Grenzen des Rolls. Alles andere
(`code`-Tiers, `shares`, `db`) ist Vorschlag; `layerRules` ist Wahrheit.
Daraus folgt:

1. Nach dem Regel-Lauf wird jedes `min` aufgefüllt (Parts werden nachgelegt,
   notfalls aus einem anderen Pool derselben Rolle) und jedes `max` gekappt
   (entfernt werden zuerst die Parts mit dem höchsten Tier, also die
   entbehrlichsten).
2. Rollen **ohne** `layerRules`-Eintrag sind unbegrenzt (`maxFor()` liefert
   dann −1). Das ist wichtig für den Ausschluss-Ersatz (§3.6) und für
   `addable`-Pools.
3. `optional` im Ergebnis bedeutet: „dieser Part wird von keinem
   Rollen-Minimum gefordert“. Markiert werden die ersten `min` Parts je Rolle
   in der Reihenfolge aufsteigender Tier-Nummer.
   **Achtung:** Ein Part aus einer `code "1"`-Regel in einer Rolle **ohne**
   `layerRules`-Eintrag (z. B. `TOP-NEW` in NEWSTYLE) wird dadurch als
   `optional` gemeldet, obwohl der Roll ihn immer erzeugt. Das Flag sagt also
   nichts über die Roll-Wahrscheinlichkeit aus, sondern nur über die
   Pflicht-Abdeckung der Rolle (offene Frage 13).

### 3.2 `pools` (Style-Gewichte)

Ganzzahl-Gewicht je Pool. Verwendung: Pool-Wahl **innerhalb einer Regel**.
Summe ≠ 100 ist erlaubt (nur relative Größen), z. B. `CLK-STD: 2` gegen
`PUN-EDM: 1`.

* Fehlt ein Pool in der Map, gilt 1 (neutral) — damit sind Regel-Pools, die
  nicht in `pools` stehen, nicht automatisch unerreichbar.
* Eine explizite `0` sperrt den Pool (in den Daten kommt 0 nicht vor).
* In FREE BUILD werden die Style-Gewichte nicht benutzt (es gibt keinen
  Style); dort sind alle Pools einer Rolle gleichberechtigt.

### 3.3 `code` — die Kernentscheidung (B-71)

**Lesart:** `code` ist eine **Tier-Folge** für die Parts *dieser* Regel:

```
code "T"    -> ein Part mit Tier T
code "T-T2" -> Part 1 mit Tier T, Part 2 mit Tier T2
```

und ein Tier ist eine **Wahrscheinlichkeit, dass der Part entsteht**:

| Tier | Bedeutung (Text) | Wahrscheinlichkeit |
|---|---|---|
| 1 | „immer“ — feste Belegung aus einem Pool | 1.00 |
| 2 | „häufig, aber optional“ | **0.70** (Annahme) |
| 3 | „immer — genau einer aus der Pool-Gruppe“ | 1.00 |
| 4 | „selten/optional“ | **0.40** (Annahme) |

Die zweite Ziffer ist **keine Obergrenze**, sondern die Wahrscheinlichkeit für
den Partner-Part. Obergrenzen kommen ausschließlich aus `layerRules` und
`maxSlots`.

**Begründung (warum nicht „a bis b Teile“):** Nur diese Lesart erklärt den
kompletten Datenbestand widerspruchsfrei:

* Jede `"3"`-Regel hat ≥ 2 Pools (Gruppenwahl), jede `"1"`-Regel genau einen.
  Beide sind „immer“; sie unterscheiden sich nur im Charakter der Wahl.
* `"1-1"` bei SCHRANZ auf `ENS-SCZ` ergibt zwei Parts — genau das fordert
  `layerRules.schranz_enhancer: min 2 / max 2`. Mit „1 bis 1 Teile“ wäre das
  Minimum dieser Rolle **unerfüllbar**.
* `"1-2"` auf `CLK-STD` ergibt 1 oder 2 Clicks — `click: min 1 / max 2`.
* `"3-4"` (RAWSTYLE, Punch-Reverbs) ergibt 1 oder 2 Enhancer —
  `punch_enhancer: min 1 / max 2`.
* `"4-4"` (INDUSTRIAL, Punch-Reverbs) ergibt 0…2 Enhancer —
  `punch_enhancer: min 0 / max 2`.
* Tier 2 und 4 kommen im Datenbestand **ausschließlich** bei Rollen mit
  `min 0` vor. Die konkreten Wahrscheinlichkeiten beeinflussen also nie ein
  Minimum, nur die Wahrscheinlichkeit eines optionalen Parts. Genau deshalb
  sind 0.70/0.40 als Arbeitsannahme vertretbar — B-77 muss sie vermessen.

**Vorkommen im Datenbestand (48 Regeln in 9 Styles):**

| code | Anzahl | Rollen, in denen er vorkommt |
|---|---|---|
| `1` | 14 | rumble, rumble_enhancer, top, crunch, punch, click |
| `2` | 12 | punch_enhancer (Percs/Punch-Top/Reverbs), rumble, rumble_enhancer |
| `3` | 9 | punch, rumble, crunch/rumble-Gemischtgruppe |
| `4` | 3 | punch_enhancer (Punch-Reverbs, leise) |
| `1-1` | 1 | schranz_enhancer (Paar) |
| `1-2` | 7 | click (1–2 Clicks) |
| `3-4` | 1 | punch_enhancer (1–2 Reverbs) |
| `4-4` | 1 | punch_enhancer (0–2 Reverbs) |

Implementiert in `RandomizerEngine::parseRuleCode()` (Toleranz: fehlender oder
unlesbarer `code` ⇒ wie `"1"`, Warnung; Ziffern werden auf 1…4 geklemmt;
mehr als zwei Teile ⇒ Rest ignoriert mit Warnung).

### 3.4 `db` / `dbPair` — Pegel in dB

* `db` gilt für den **ersten** Part der Regel, `dbPair` für den **zweiten**.
* Ist nur `db` gesetzt (`"1-2"` bei CLK-STD hat z. B. nur `dbPair`), gilt für
  den Partner derselben Regel `db`, sonst der Pool-`defaultGain`.
* Der Wert wird **uniform** im Bereich gezogen (Annahme; das Original könnte
  auch einen festen Wert nehmen — bei `[min,max]` mit min == max ist beides
  identisch, und das ist in den Daten häufig: `[-5,-5]`, `[-16,-16]`, `[-3,-3]`).
* Ohne jeden Bereich gilt `defaultGain` des Pools als Startpegel.
* Bereiche sind im Ergebnis **dB** (`RolledPart::gainDb`); intern rechnet die
  Engine über `gainToDb()`/`dbToGain()`.
* Plausibilisierung: die Kombination `db: [-5,-5]` + `dbPair: [-14,-7]` bei
  INDUSTRIAL heißt „ein Punch-Reverb laut, ein zweiter deutlich leiser“;
  `db: [-35,-27]` bei REVERSE BASS/PSY heißt „Reverb nur als Hauch“.

### 3.5 `shares` — Prozentverteilung innerhalb einer Regel

Beispiel NEWSTYLE: `shares: [{PUN-NEW: 60}, {PUN-EDM, PUN-TEC: 40}]`.
Beispiel RAWSTYLE: `[{CRN-RAW: 45}, {RMB-RAW, RMB-BIG, RMB-TEC: 35}, {CRN-ZAG: 20}]`.

* `shares` **ersetzt** die `pools`-Gewichte dieser Regel.
* Der Gruppen-Prozentsatz wird gleichmäßig auf die Pools der Gruppe verteilt
  (in den Daten haben die Gruppenmitglieder identische Style-Gewichte, daher
  ist das gleichwertig zu einer gewichteten Aufteilung).
* Ein Pool der Regel, der in keiner Gruppe steht, ist innerhalb dieser Regel
  **nicht** wählbar (Gewicht 0).
* Kontrolle: 45 + 35 + 20 = 100; 60 + 40 = 100.

### 3.6 Globale Ausschlussregel „melodic × atonal“

`rules[{type:"exclude", a:"melodic", b:"atonal", scope:"kick"}]` bedeutet:
**ein Kick enthält nie gleichzeitig einen melodischen und einen atonalen Part.**
Ist im Modell keine Regel definiert (z. B. bei einer unvollständigen Datei),
gilt genau diese Grundregel weiter (`fallbackExclusions()`).

Auflösung nach der Auswahl (`RandomizerEngine::resolveExclusions()`):

1. **Gewinnerseite** wird einmal je Regel bestimmt: die Seite mit mehr
   Pflicht-Parts; bei Gleichstand die Tonalität des **ersten** Parts in
   Roll-Reihenfolge (deterministisch).
2. Jeder Part der **Verliererseite** wird von hinten nach vorne behandelt:
   * **Ersatz** durch einen Pool der Gewinner-Tonalität — Suchreihenfolge:
     gleiche Regel → gleiche Rolle → jede Rolle mit freier `layerRules`-Kapazität.
     Der Pegel des Parts bleibt erhalten.
   * sonst **entfernen**, aber nur wenn die Rolle ihr Minimum danach noch hält.
   * Ein Pflicht-Part ohne Ersatz bleibt stehen.
3. **Präzedenz:** Pflicht (`layerRules.min`) > Ausschlussregel > Optional.
   Kollidieren zwei Pflicht-Parts, gewinnt der Ausschluss **nicht**; die Engine
   protokolliert das in `lastWarnings()`, statt ein Minimum zu brechen.

Warum Ersatz statt „einfach löschen“? Der Datenbestand ist so gebaut, dass die
Regel normalerweise gar nicht greifen muss: RAWSTYLE bietet Crunch *und* Rumble
an, wählt aber per `"3"`-Gruppe genau **einen** davon (45 % / 35 % / 20 %).
Die Regel ist also die Absicherung für den Fall, dass mehrere Quellen
zusammentreffen (eigene Styles, FREE BUILD, `+ ADD PART`).

### 3.7 `duplicateGain` — zweite Belegung desselben Pools

Betroffen: `RMB-SCZ` und `RMB-CIH` (`0.75`).
Regel: der **erste** Part eines Pools nutzt den Pegel aus `db`/`defaultGain`,
jeder **weitere** Part desselben Pools im selben Kick bekommt zusätzlich
`gainToDb(duplicateGain)` (0.75 ⇒ −2.5 dB). Sinn: zwei Schranz-Rumbles
übereinander sollen nicht doppelt so laut summieren.

### 3.8 `fileWeights` — einzelne Dateien abwerten

Beispiel `PUN-RAW`: `{"Punch Heavy Raw": 0.3}` — „kommt 70 % weniger oft“.
Umsetzung: beim Ziehen des konkreten Samples wird jede Datei des Pools mit
Gewicht 1.0 angesetzt, Dateien mit Treffer im `fileWeights`-Eintrag mit ihrem
Gewicht. Treffer-Regeln (in dieser Reihenfolge):

1. exakter Dateiname (`Punch Heavy Raw 13 150.wav`) oder Name ohne Endung,
2. **Präfix**-Treffer (`Punch Heavy Raw …`), case-insensitiv.

Ein Eintrag ist also ein **Filter auf Dateinamen**, keine Sperre: 0.3 heißt
seltener, nicht „nie“. Das deckt sich mit dem Redaktionskommentar („about 17 %
of Rawstyle punches … 17 % is fine“).

### 3.9 `maxSlots` und FREE BUILD

* `RollRequest::maxSlots` wird auf 0…12 geklemmt (`KickState::kNumSlots`).
* Die Obergrenze ist hart, **außer** sie liegt unter der Summe der
  Rollen-Minima: Pflicht schlägt Deckel. Die Summe der Minima liegt in allen
  Factory-Styles zwischen 2 und 6 (Maximum: SCHRANZ mit 6 — passt exakt in die
  6-Slot-Konfiguration des Originals). Entfernt werden zuerst Parts mit hohem
  Tier, danach späte Positionen.
* **FREE BUILD** (`freeBuild == true`, Style-id wird ignoriert) hat keine
  Style-Regeln. Arbeitsannahme (Konfidenz **niedrig**):

  | Rolle | min | max | Tier |
  |---|---|---|---|
  | punch | 1 | 1 | 1 |
  | click | 0 | 2 | 2 |
  | crunch | 0 | 1 | 2 |
  | rumble | 0 | 1 | 2 |
  | punch_enhancer | 0 | 1 | 2 |
  | rumble_enhancer | 0 | 1 | 4 |
  | schranz_enhancer | 0 | 1 | 4 |
  | top | 0 | 1 | 2 |

  Quellen sind alle Pools der jeweiligen Rolle, gleich gewichtet. Die globale
  Ausschlussregel gilt auch hier. FREE BUILD ist damit ein „Grundgerüst“, das
  der Nutzer anschließend per `+ ADD PART` erweitert (B-74/B-76).
* Unbekannte `styleId` (ohne `freeBuild`) fällt auf FREE BUILD zurück und
  erzeugt eine Warnung — nie ein leeres Ergebnis, nie eine Exception.

### 3.10 Determinismus

* Einzige Zufallsquelle ist `std::mt19937_64`, das pro `roll()` mit
  `RollRequest::seed` neu gesetzt wird.
* Es werden **keine** `std::uniform_*_distribution`-Objekte benutzt, weil deren
  Abbildung nicht standardisiert ist. Stattdessen 53-Bit-Mantisse:
  `(rng() >> 11) * 2^-53`. Damit ist ein Roll über Compiler/STL-Versionen
  hinweg reproduzierbar.
* Ausgabereihenfolge ist stabil: punch, punch_enhancer, click, crunch, rumble,
  rumble_enhancer, schranz_enhancer, top (innerhalb einer Rolle Roll-Reihenfolge).
  Die endgültige **Slot-Zuordnung** (0…11) ist nicht Teil dieser Schicht,
  sondern von B-74 zu entscheiden.

---

## 4. Style-Tabelle (alle 9 Styles)

Gewichte in Klammern = `pools`-Gewicht. „Pflicht“ = `layerRules.min > 0`.
Pegel = `db`/`dbPair` der zugehörigen Regel (dB); „—“ = kein Bereich, dann
gilt `defaultGain` des Pools.

**Übersicht (aus `genres.json` berechnet, inkl. `layerRules`-Kappung):**

| Style | Parts im Minimum | Parts im Maximum | Pflichtsumme |
|---|---|---|---|
| `rumble_kicks` | 5 | 8 | 5 |
| `reverse_bass` | 3 | 7 | 3 |
| `industrial` | 2 | 4 | 2 |
| `psy` | 3 | 7 | 3 |
| `schranz` | 6 | **12** | 6 |
| `rumble_kicks_clean` | 3 | 4 | 3 |
| `midintro_newstyle` | 4 | 8 | 4 |
| `rawstyle` | 3 | 6 | 3 |
| `zaag` | 3 | 3 | 3 |

„Minimum“ = alle Tier-2/4-Würfe fallen aus (dann greifen die
`layerRules`-Minima), „Maximum“ = alle Würfe treffen und die `max`-Grenzen sind
ausgeschöpft. Kein Style überschreitet 12 Slots; SCHRANZ erreicht sie exakt.
Alle Pflichtrollen sind durch Regel-Pools gedeckt — der einzige Pool, der in
einem Style deklariert, aber in keiner Regel verwendet wird, ist `CLK-STD` in
RAWSTYLE, und genau dieser steht dort in `addable`.

### 4.1 RUMBLE — `rumble_kicks` · `#DE2A2A` · Preset „Rumble Magic“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-EDM (1), PUN-TEC (1), RMB-TEC (1), ENR-TEC (1), ENT-TOP (1), CLK-STD (2), PRC-GAB (1), PTP-TEC (1) | punch 1–1, rumble 1–1, rumble_enhancer 1–1, top 1–1, click 1–2 | punch_enhancer 0–2 |

| Regel | Pools | Pegel |
|---|---|---|
| `3` | PUN-EDM, PUN-TEC | — |
| `1` | RMB-TEC | — |
| `1` | ENR-TEC | — |
| `1` | ENT-TOP | — |
| `1-2` | CLK-STD | 1. Part —, 2. Part `dbPair [-5,-5]` |
| `2` | PRC-GAB | `db [-30,-23]` |
| `2` | PTP-TEC | `db [-20,-12]` |

### 4.2 REVERSE BASS — `reverse_bass` · `#7A3DD6` · „Reverse Boss“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-EDM (1), PUN-TEC (1), CRN-REV (1), CLK-STD (2), PRC-GAB (1), PTP-TEC (1), PRV-IND (1), PRV-NEW (1), PRV-RAW (1) | punch 1–1, crunch 1–1, click 1–2 | punch_enhancer 0–3 |

| Regel | Pools | Pegel |
|---|---|---|
| `3` | PUN-EDM, PUN-TEC | — |
| `1` | CRN-REV | — |
| `1-2` | CLK-STD | 2. Part `dbPair [-5,-5]` |
| `2` | PRC-GAB | `db [-30,-23]` |
| `2` | PTP-TEC | `db [-20,-12]` |
| `4` | PRV-IND, PRV-NEW, PRV-RAW | `db [-35,-27]` |

### 4.3 INDUSTRIAL — `industrial` · `#6E7480` · „Wire Cutter“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-IND (1), PUN-IXT (1), RMB-CIH (1), RMB-RAW (1), PRV-IND (2), PRV-NEW (2), PRV-RAW (2) | punch 1–1, rumble 1–1 | punch_enhancer 0–2 |

| Regel | Pools | Pegel |
|---|---|---|
| `3` | PUN-IND, PUN-IXT | — |
| `3` | RMB-CIH, RMB-RAW | — |
| `4-4` | PRV-IND, PRV-NEW, PRV-RAW | 1. Part `db [-5,-5]`, 2. Part `dbPair [-14,-7]` |

> Hinweis: `PUN-IXT` ist als **atonal** getaggt (Jim-Regel: „dieser Punch darf
> nie auf einen melodischen Crunch treffen“). Die Ausschlussregel erledigt das
> für INDUSTRIAL ohne Sonderfall, weil der Style keinen melodischen Pool hat.

### 4.4 PSY — `psy` · `#7CC41E` · „PsyMeister“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-EDM (1), PUN-NEW (1), PUN-TEC (1), CRN-PSY (1), CLK-STD (2), PRC-GAB (1), PTP-TEC (1), PRV-IND (1), PRV-NEW (1), PRV-RAW (1) | punch 1–1, crunch 1–1, click 1–2 | punch_enhancer 0–3 |

| Regel | Pools | Pegel |
|---|---|---|
| `3` | PUN-EDM, PUN-NEW, PUN-TEC | — |
| `1` | CRN-PSY | — |
| `1-2` | CLK-STD | 2. Part `dbPair [-5,-5]` |
| `2` | PRC-GAB | `db [-30,-23]` |
| `2` | PTP-TEC | `db [-20,-12]` |
| `4` | PRV-IND, PRV-NEW, PRV-RAW | `db [-35,-27]` |

### 4.5 SCHRANZ — `schranz` · `#E05A1A` · „Schranz Explosion“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-EDM (1), PUN-TEC (1), PUN-TCH (1), RMB-SCZ (2), RMB-TEC (1), ENS-SCZ (2), ENR-TEC (1), ENT-TOP (1), CLK-STD (2), PRC-GAB (1), PTP-TEC (1) | punch 1–1, rumble 1–3, schranz_enhancer **2–2**, top 1–1, click 1–2 | rumble_enhancer 0–1, punch_enhancer 0–2 |

| Regel | Pools | Pegel |
|---|---|---|
| `3` | PUN-EDM, PUN-TEC, PUN-TCH | — |
| `1-2` | RMB-SCZ | 2. Part `dbPair [-15,-5]` |
| `2` | RMB-TEC | — |
| `1-1` | ENS-SCZ | — (liefert per Definition **zwei** Enhancer) |
| `2` | ENR-TEC | — |
| `1` | ENT-TOP | — |
| `1-2` | CLK-STD | 2. Part `dbPair [-5,-5]` |
| `2` | PRC-GAB | `db [-32,-25]` |
| `2` | PTP-TEC | `db [-23,-14]` |

> Das ist der einzige Style, in dem `"1-1"` vorkommt. Genau hier zeigt sich,
> warum `code` als Tier-Folge (und nicht als Anzahlbereich) gelesen werden
> muss: `schranz_enhancer` verlangt **zwei** Parts, die Regel liefert sie.

### 4.6 RUMBLE CLEAN — `rumble_kicks_clean` · `#D64848` · „Drop Puncher“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-EDM (1), PUN-TEC (1), RMB-BIG (1), CLK-STD (2) | punch 1–1, rumble 1–1, click 1–2 | — |

| Regel | Pools | Pegel |
|---|---|---|
| `3` | PUN-EDM, PUN-TEC | — |
| `1` | RMB-BIG | — |
| `1-2` | CLK-STD | 2. Part `dbPair [-5,-5]` |

### 4.7 NEWSTYLE — `midintro_newstyle` · `#3D6BD6` · „THE NEW WAY“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-EDM (1), PUN-NEW (1), PUN-TEC (1), CRN-NEW (1), CLK-STD (2), PRC-GAB (1), PTP-TEC (1), PRV-IND (1), PRV-NEW (1), PRV-RAW (1), TOP-NEW (1) | punch 1–1, crunch 1–1, click 1–2 | punch_enhancer 0–3 |

| Regel | Pools | Pegel |
|---|---|---|
| `3` + `shares` | PUN-NEW **60 %**, PUN-EDM + PUN-TEC **40 %** | — |
| `1` | CRN-NEW | — |
| `1-2` | CLK-STD | 2. Part `dbPair [-5,-5]` |
| `2` | PRC-GAB | `db [-32,-25]` |
| `2` | PTP-TEC | `db [-23,-14]` |
| `4` | PRV-IND, PRV-NEW, PRV-RAW | `db [-32,-24]` |
| `1` | TOP-NEW | `db [-3,-3]` (immer, immer −3 dB) |

### 4.8 RAWSTYLE — `rawstyle` · `#C81E5A` · „Crunch Boost“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-RAW (1), CRN-RAW (1), RMB-RAW (1), RMB-BIG (1), RMB-TEC (1), PRV-IND (2), PRV-NEW (2), PRV-RAW (2), CRN-ZAG (1), CLK-ZAG (1), CLK-STD (2) | punch 1–1, punch_enhancer 1–2 | crunch 0–1, rumble 0–1 |

| Regel | Pools | Pegel |
|---|---|---|
| `1` | PUN-RAW | — |
| `3` + `shares` | CRN-RAW **45 %**; RMB-RAW + RMB-BIG + RMB-TEC **35 %**; CRN-ZAG **20 %** | — |
| `3-4` | PRV-IND, PRV-NEW, PRV-RAW | 1. Part `db [-16,-16]`, 2. Part `dbPair [-25,-18]` |
| `1` | CLK-ZAG | — |

`addable: [CLK-STD]` (nur für `+ ADD PART`, nicht für den Roller).

> Das ist der einzige Style, der Crunch **und** Rumble anbietet — die `"3"`-Regel
> sorgt dafür, dass pro Roll genau **einer** der beiden kommt. Damit ist die
> Ausschlussregel im Normalfall nie auslösend, sondern nur Absicherung.

### 4.9 ZAAG — `zaag` · `#1FB5A0` · „Zaag Meister“

| Pools | Pflicht | Optional |
|---|---|---|
| PUN-ZAG (1), CRN-ZAG (1), CLK-ZAG (1) | punch 1–1, crunch 1–1, click 1–1 | — |

| Regel | Pools | Pegel |
|---|---|---|
| `1` | PUN-ZAG | — |
| `1` | CRN-ZAG | — |
| `1` | CLK-ZAG | — |

Komplett deterministisch: jeder Roll enthält genau diese drei Parts.

### 4.10 FREE BUILD — kein `id` in der Datei

Siehe §3.9. Sidebar-Eintrag Nummer 10, kein `defaultPreset`, kein Akzent in
`genres.json` (die UI nutzt hier einen eigenen Neutral-Akzent).

---

## 5. Pool-Tabelle (alle 29)

| Pool | Ordner | Rolle | Tonalität | defaultGain | duplicateGain | fileWeights | uiOrder |
|---|---|---|---|---|---|---|---|
| PUN-EDM | Main - Punches/Clean EDM | punch | neutral | 1.0 | 1.0 | — | 0 |
| PUN-IND | Main - Punches/Industrial | punch | neutral | 1.0 | 1.0 | — | 0 |
| PUN-IXT | Main - Punches/Industrial (Extended) | punch | **atonal** | 1.0 | 1.0 | — | 0 |
| PUN-NEW | Main - Punches/Newstyle | punch | neutral | 1.0 | 1.0 | — | 0 |
| PUN-RAW | Main - Punches/Rawstyle | punch | neutral | 1.0 | 1.0 | `Punch Heavy Raw` 0.3 | 0 |
| PUN-TEC | Main - Punches/Techno | punch | neutral | 1.0 | 1.0 | — | 0 |
| PUN-TCH | Main - Punches/Techno (Heavy) | punch | neutral | 1.0 | 1.0 | — | 0 |
| PUN-ZAG | Main - Punches/Zaag | punch | neutral | 1.0 | 1.0 | — | 1 |
| CRN-NEW | Main - Crunches/Newstyle & Midintro (Melodic) | crunch | **melodic** | 1.0 | 1.0 | — | 0 |
| CRN-PSY | Main - Crunches/Psy Style (Melodic) | crunch | **melodic** | 1.0 | 1.0 | — | 0 |
| CRN-RAW | Main - Crunches/Rawstyle (Melodic) | crunch | **melodic** | 1.0 | 1.0 | — | 0 |
| CRN-REV | Main - Crunches/Reverse Bass (Melodic) | crunch | **melodic** | 1.0 | 1.0 | — | 0 |
| CRN-ZAG | Main - Crunches/Zaag (Melodic) | crunch | **melodic** | 1.0 | 1.0 | — | 1 |
| RMB-RAW | Main - Rumbles/Rumble (Rawstyle) | rumble | **atonal** | 1.0 | 1.0 | — | 0 |
| RMB-SCZ | Main - Rumbles/Schranz | rumble | **atonal** | 1.0 | **0.75** | — | 0 |
| RMB-BIG | Main - Rumbles/Rumbles (Bigroom) | rumble | **atonal** | 1.0 | 1.0 | — | 0 |
| RMB-TEC | Main - Rumbles/Rumbles (Techno) | rumble | **atonal** | 1.0 | 1.0 | — | 0 |
| RMB-CIH | Main - Crunches/Industrial Heavy | rumble | **atonal** | 1.0 | **0.75** | — | 0 |
| ENR-TEC | Enhancers (Rumbles) | rumble_enhancer | **atonal** | 1.0 | 1.0 | — | 0 |
| ENS-SCZ | Enhancers (Schranz) | schranz_enhancer | **atonal** | 1.0 | 1.0 | — | 0 |
| ENT-TOP | Enhancers (Tops) | top | neutral | 1.0 | 1.0 | — | 0 |
| TOP-NEW | Transients (Newstyle Tops) | top | neutral | 1.0 | 1.0 | — | 2 |
| CLK-STD | Transients (Clicks) | click | neutral | 1.0 | 1.0 | — | 0 |
| CLK-ZAG | Transients (Clicks) [Zaag] | click | neutral | 1.0 | 1.0 | — | 1 |
| PTP-TEC | Transients (Punch Top) | punch_enhancer | neutral | **0.7** | 1.0 | — | 0 |
| PRC-GAB | Transients (Percs) | punch_enhancer | neutral | **0.7** | 1.0 | — | 0 |
| PRV-IND | Enhancers (Punch Reverbs)/Reverbs (Industrial) | punch_enhancer | neutral | **0.7** | 1.0 | — | 0 |
| PRV-NEW | Enhancers (Punch Reverbs)/Reverbs (Newstyle) | punch_enhancer | neutral | **0.7** | 1.0 | — | 0 |
| PRV-RAW | Enhancers (Punch Reverbs)/Reverbs (Rawstyle) | punch_enhancer | neutral | **0.7** | 1.0 | — | 0 |

---

## 6. Ablauf eines Rolls

```text
roll(RollRequest{styleId, freeBuild, maxSlots, seed})
 1. rng.seed(seed); Modell-Sicht waehlen (Style oder FREE BUILD)
 2. Regeln -> Parts               collectFromStyle()     (code-Tiers, shares, db/dbPair)
 3. layerRules durchsetzen        enforceLayerRules()    (min nachfuellen, max kappen)
 4. Ausschlussregel               resolveExclusions()    (Ersatz, sonst entfernen)
 5. Obergrenze                    applySlotLimit()       (Pflicht schlaegt Deckel)
 6. Samples ziehen                resolveSamples()       (fileWeights, No-Repeat, duplicateGain)
 7. Pflicht markieren + sortieren markMandatory(), sortParts()
 8. -> std::vector<RolledPart>
```

`RolledPart` trägt `role`, `poolId`, `samplePath`, `gainDb`, `optional`.
Die Engine kennt **keine** Dateisystem-Layouts: Pfade kommen aus
`PoolResolver`, No-Repeat-Eintscheidungen aus `NoRepeatTracker` (beide
Schnittstellen sind injiziert und in den Tests durch Attrappen ersetzt).

### 6.1 Empirische Prüfung (2026-11-17)

Der komplette Datenbestand wurde mit dem echten `genres.json` vermessen:
**1.000 Rolls je Style** (9.000 Rolls) plus 300 Rolls mit `maxSlots = 6`,
500 FREE-BUILD-Rolls und die Catch2-Unit-Tests (23 Testfälle, 1.829
Assertions). Ergebnis: **0 Verstöße**.

| Style | Parts min…max | davon Pflicht | beobachtete Pool-Häufigkeiten (je 1.000 Rolls) |
|---|---|---|---|
| rumble_kicks | 5…8 | 5 | RMB-TEC 1000, ENR-TEC 1000, ENT-TOP 1000, CLK-STD 1691, PRC-GAB 715, PTP-TEC 695, PUN-EDM 483 / PUN-TEC 517 |
| reverse_bass | 3…7 | 3 | CRN-REV 1000, CLK-STD 1711, PRC-GAB 704, PTP-TEC 689, PRV-* 409 |
| industrial | 2…4 | 2 | PUN-IND 483 / PUN-IXT 517, RMB-CIH 465 / RMB-RAW 535, PRV-* 582 |
| psy | 3…7 | 3 | CRN-PSY 1000, CLK-STD 1711, PRC-GAB 704, PTP-TEC 689, PRV-* 409, PUN-* 1000 |
| schranz | 6…12 | 6 | ENS-SCZ **2000**, RMB-SCZ 1717, RMB-TEC 713, ENR-TEC 684, ENT-TOP 1000, CLK-STD 1693, PRC-GAB 703, PTP-TEC 722 |
| rumble_kicks_clean | 3…4 | 3 | RMB-BIG 1000, CLK-STD 1711 |
| midintro_newstyle | 4…8 | 3 | CRN-NEW 1000, TOP-NEW 1000, CLK-STD 1711, PRC-GAB 704, PTP-TEC 689, PRV-* 409, **PUN-NEW 593** vs. PUN-EDM 194 + PUN-TEC 213 |
| rawstyle | 4…5 | 2 | PUN-RAW 1000, CLK-ZAG 1000, **CRN-RAW 417**, RMB-* 368, **CRN-ZAG 215**, PRV-* 1411 |
| zaag | 3…3 | 3 | PUN-ZAG/CRN-ZAG/CLK-ZAG je 1000 |

Damit sind die Kernannahmen dieser Spec **messtechnisch bestätigt**:

* `layerRules`-Minima und -Maxima in 9.000 Rolls zu 100 % eingehalten
  (auch die Paar-Regel `"1-1"`: ENS-SCZ kommt exakt 2.000-mal vor).
* Ausschlussregel „melodic × atonal“ nie verletzt; RAWSTYLE wählt wie erwartet
  per `"3"`-Gruppe genau eine Seite (45/35/20 % → 417/368/215).
* `shares` wirken (NEWSTYLE 59 % / 41 % statt 60/40 — Rundung der
  Einzel-Pool-Aufteilung), Tier 2 trifft mit ≈ 70 % (PRC-GAB 71,5 %,
  PTP-TEC 69,5 %), Tier 4 mit ≈ 40 % (PRV-* 41 %, industrial 0,4 + 0,4·0,4).
* Determinismus: gleicher Seed liefert bit-identische Ergebnisse.
* `maxSlots = 6` wird von allen Styles eingehalten; FREE BUILD liefert immer
  einen Punch; unbekannte Style-id fällt auf FREE BUILD zurück und warnt.
* Pegel-Spannen entsprechen den Bereichen: CLK-STD 0/−5 dB, TOP-NEW −3 dB,
  PRC-GAB −32…−23 dB, PTP-TEC −23…−12 dB, PRV* −35…−5 dB,
  RMB-SCZ 0…−17,5 dB (`dbPair` plus `duplicateGain`).


---

## 7. Offene Fragen und Klärungsvorschlag

| # | Frage | Warum offen | Vorschlag |
|---|---|---|---|
| 1 | Sind 0.70 (Tier 2) und 0.40 (Tier 4) die richtigen Wahrscheinlichkeiten? | Nirgends dokumentiert; nur die Datenlage grenzt sie ein (Tier 2/4 nur bei `min 0`) | B-77: 1.000 Rolls je Style im Original, Häufigkeit der optionalen Parts messen (z. B. „wie oft hat ein RUMBLE-Kick einen PTP-TEC?“, „wie oft zwei Clicks?“), dann Tiers kalibrieren |
| 2 | Wird der Partner-Part aus `dbPair` auch dann gezogen, wenn er einen **anderen** Pool trifft? | `dbPair` ist textlich an „Pair“ gebunden, in den Daten aber immer Gruppen-/Einzelpool-Regeln | Annahme: `dbPair` gehört zum **zweiten Part der Regel**, unabhängig vom Pool. Prüfen, ob im Original zwei Clicks immer dieselbe Datei sind oder zwei verschiedene |
| 3 | Ist `db`-Ziehung uniform oder wird der Bereich in Stufen gerundet? | `[min,max]` mit min < max nur bei 8 Regeln; Original könnte runden | B-77: Verteilung der `lN_vol`-Werte der 44 Presets je Style ansehen (Mining-Report, §2) |
| 4 | Semantik von `code` — ist die Tier-Folge-Lesart bewiesen? | Keine Doku im Content; nur Rückschluss aus `layerRules` (siehe §3.3) | **Hochpriorisiert:** ein Original-Roll je Style mit 1.000 Wiederholungen und gelocktem Seed-Vergleich; zusätzlich Gegenprobe „`1-1` = 1 Part“ (würde SCHRANZ sofort brechen) |
| 5 | Was genau macht `duplicateGain`? (linear auf zweite Belegung, oder generell Dämpfung des Pools?) | Nur bei RMB-SCZ/RMB-CIH gesetzt; kein Handbuchtext | Annahme: zweite Belegung. Test mit SCHRANZ-Rolls im Original (2× RMB-SCZ): Pegel des zweiten Parts messen (Slot-`lN_vol`) |
| 6 | `fileWeights`-Schlüssel: Präfix oder exakter Name? | Schlüssel „Punch Heavy Raw“ ohne `.wav`/BPM passt auf viele Dateien | Annahme: Präfix, case-insensitiv. Gegenprobe: 100 Rolls PUN-RAW, Anteil der „Punch Heavy Raw *“-Dateien ≈ 17 % (Redaktionsangabe) |
| 7 | Wie wird die Slot-Position (0…11) vergeben? | Nicht Teil von `genres.json`; Mining-Report zeigt Muster (0 punch, 3 click, 4 rumble, 6/7 schranz, 8 top) | B-74: feste Rollen-Slots mit Fallback; die Engine liefert nur die Reihenfolge |
| 8 | Ist `maxSlots` wirklich weich gegenüber Pflichtteilen? | Original erlaubt 6 oder 12 Slots; die Summe der Minima ist in allen Factory-Styles ≤ 6 (SCHRANZ: 6) | Beibehalten (Pflicht > Deckel). Bei 6 Slots ist der Deckel für alle Factory-Styles erfüllbar; nur eigene Styles mit vielen Pflichtrollen können ihn sprengen → UI sollte warnen |
| 9 | Zählt `addable` für den Roller? | In RAWSTYLE steht nur CLK-STD darin, im Roll nicht verwendet | Annahme: nein, `addable` ist reine UI-Liste für `+ ADD PART` |
| 10 | Anzahl-Diskrepanz 25/29 Pools und 10/9 Styles im Auftrag | Auftragstext stammt aus einer älteren Revision als R33 | Als erledigt betrachten: die Datei ist maßgeblich (29/9 + FREE BUILD); Spec und Modell folgen der Datei |
| 11 | `kitPattern` / `categoriesRequired` | Für den Randomizer nicht gebraucht, aber vorhanden | Ignorieren; falls später „Kit-Erkennung“ kommt (B-21), dort auswerten |
| 12 | Dürfen eigene Styles (B-76) `code`-Tiers frei setzen? | Unsere Engine unterstützt 1…4 und `a-b` | Ja; bei ungültigem `code` fällt die Engine auf „Pflicht“ zurück und warnt |
| 13 | Ist `optional` die richtige Sichtbarkeitsregel für das `X` an der Slot-Karte (B-74)? | `optional` = „nicht durch ein Minimum gefordert“; dadurch gilt `TOP-NEW` in NEWSTYLE als optional, obwohl jede Roll ihn erzeugt | B-74: `optional` nur für die Pflicht-Kennzeichnung nutzen; entfernbar bleibt jede Karte. Alternativ später ein zusätzliches Feld `rolledAsMandatory` (Tier 1/3) ergänzen |
