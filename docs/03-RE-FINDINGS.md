# 03 — Reverse-Engineering-Befunde (Analyse-Logbuch)

Dieses Dokument hält fest, **was wir über das Verhalten des Referenz-Plugins gelernt
haben** und **wie** wir es gelernt haben. Es enthält bewusst **keine** Auszüge,
Dump-Dateien oder kopierten Inhalte aus der kommerziellen Software — nur
Verhaltensbeschreibungen und daraus abgeleitete Anforderungen.

> Analysiert wurde eine legal erworbene, lokal installierte Lizenz. Zweck: die
> Funktionen verstehen, um sie unabhängig neu zu implementieren. Es wird kein Code
> übernommen.

---

## 1. Methodik

| Methode | Zweck | Datengrundlage |
|---|---|---|
| **Statische Inventur** | Was gehört zum Produkt, wo liegt es? | Verzeichnisbaum, Registry, Content-Ordner |
| **Format-Analyse offener Daten** | Zustand, Parameter, Regeln lesen | XML-Presets, FX-Chain-XML, `genres.json`, `prefs.json` |
| **Handbuch-Auswertung** | Bedienlogik und Signalfluss absichern | 37-seitiges PDF, Textebene extrahiert |
| **Binär-Statik** | Framework, Abhängigkeiten, Modulstruktur, Algorithmus-Familie | PE-Imports, Klassen-/Symbolnamen (RTTI), Zeichenketten |
| **Messung am Original** | Klangverhalten, Pegel, Kennlinien | Render-Exporte (Drag Master), Spektral-/Hüllkurvenvergleich |
| **Empirik** | Randomizer-Verteilungen, Grenzfälle | Serien von Rolls, Zustandsvergleiche |

Alle Aussagen unten sind mit der Methode markiert:
`[D]` = aus offenen Daten, `[H]` = aus dem Handbuch, `[B]` = aus der Binär-Statik,
`[M]` = gemessen, `[?]` = Hypothese, noch zu verifizieren.

---

## 2. Produkt- und Technikbefunde

| # | Befund | Quelle |
|---|---|---|
| R-01 | Ausgeliefert werden **Standalone + VST3**; eine VST2-`.dll` existiert im Installationsstand nicht. | `[D]` Dateisystem, Registry |
| R-02 | Framework ist **JUCE 8.0.7**, VST3-SDK **3.7.12**. | `[B]` |
| R-03 | Rendering über **Direct2D**, Audio über **WASAPI/DSound**, MIDI über **WinMM**. | `[B]` Imports |
| R-04 | Standalone und Plugin teilen sich **einen** Prozessor-Code (kein zweiter DSP-Pfad). | `[B]` Klassenstruktur |
| R-05 | Timestretching basiert auf **WSOLA** (Waveform-Similarity Overlap-Add) im
STFT-Kontext, ergänzt um eine separate Pitch-Stufe. | `[B]` Engine-Klassen |
| R-06 | Eine externe Stretch-Bibliothek ist zusätzlich eingebunden (lizenzpflichtig) —
für uns **nicht** relevant, wir bleiben bei der eigenen Variante. | `[B]` |
| R-07 | Der Zustand ist vollständig als **XML** offengelegt (Preset + FX-Kette) — damit
ist die Parameteroberfläche exakt dokumentierbar. | `[D]` |
| R-08 | Die **Randomizer-Regeln** liegen als versionierte JSON-Datei im Content-Ordner
(25 Pools, 10 Styles, Ausschlussregeln, Gewichte, Pegelbereiche). | `[D]` |
| R-09 | Die Sample-Analyse (2.405 Einträge) liegt in einem **eigenen Containerformat**
(„KBAP", Version 1, 2.405 Einträge: Name, Offset, zwei Größen). Nutzlast pro Eintrag
ist eine `*.edm`-Datei. | `[D]` Container-Header |
| R-10 | `index.txt` verknüpft WAV → Analyse-Datei und trägt zusätzlich eine Ganzzahl und
einen Float (vermutlich Kennung + Konfidenz). | `[D]` |
| R-11 | Tempo **und** Tonart stecken bereits in den Sample-Dateinamen
(`… 150.wav`, `… 12 A 150.wav`). Das Plugin nutzt genau diese Information. | `[D]`+`[H]` |
| R-12 | Das Session-/Preset-Format kann **Audiodaten einbetten** (Session-State ≈ 1,1 MB
bei wenigen Sekunden Kick ⇒ Samples liegen komprimiert mit drin). | `[D]` |
| R-13 | Ohne Lizenzaktivierung ist das Plugin **stumm und speichert nicht** — reine
Lizenz-Gate-Logik, kein Demo-Klanglimit. | `[H]` |

**Konsequenz für uns:** R-05/R-06 heißt, wir bauen die Stretch-Stufe von Grund auf
(WSOLA + Resampling) und verzichten auf die lizenzpflichtige Fremdbibliothek — das ist
gleichzeitig der einzige Bereich, in dem wir bewusst *nicht* bit-identisch klingen
können und wollen.

---

## 3. Abgeleitete Spezifikationen

### 3.1 Parameter-Normalisierung (offener Punkt F1)

Presets speichern **normalisierte 0…1-Werte** mit 9 Nachkommastellen. Beispiel:
`master_vol = 0.833333313`, `l0_stretch = 0.374989420`.

Aus `0.833333313` folgt als wahrscheinlichste Abbildung ein Verhältnis `5/6`
bzw. eine Kurve mit Skalen-Endwert; aus `0.374989420` folgt, dass **1.0× Stretch nicht
0,5** ist. Beide Kurven müssen bestimmt werden, sonst klingen importierte Presets anders.

**Vorgehen:** am Original für jeden Parameter mehrere UI-Werte einstellen, Preset
speichern, normalisierten Wert ablesen → Stützstellen → Kurve fitten.
Ergebnis wandert nach `docs/specs/parameter-registry.md`.

### 3.2 FX-Modul-Parameterbrücke (offener Punkt F2)

Der Zustand enthält **8 Slots × 14 generische Parameter** (`fx0_p0 … fx7_p13`),
während die Module selbst deutlich mehr und unterschiedlich benannte Parameter haben
(siehe `05-SPEC-FX-MODULES.md`).

**Beobachtung:** `fx2_p12` entspricht in geprüften Presets dem `mix` des dritten
Moduls (`0.319` ↔ `mix="0.3189999759197235"`). **Hypothese:** `p12` = MIX,
`p0…p11` = die ersten zwölf Modulparameter in fester Reihenfolge, `p13` = Reserve.

**Vorgehen:** je Modultyp ein Preset mit allen Parametern auf bekannte Extremwerte
setzen, speichern, `fxN_p*` ablesen → Zuordnungstabelle. Ergebnis nach
`docs/specs/fx-param-bridge.md`.

### 3.3 Randomizer-Semantik (offener Punkt F9)

`genres.json` beschreibt je Style:

* `pools` – welche Sample-Ordner mit welchem **Gewicht** gezogen werden,
* `layerRules` – min/max Anzahl je Rolle (punch, crunch, rumble, click, top,
  punch_enhancer, rumble_enhancer, schranz_enhancer),
* `rules` – je Regel `code`, `pools`, optional `db`/`dbPair` (Pegelbereiche) und
  `shares` (prozentuale Aufteilung zwischen Pool-Gruppen),
* `addable` – zusätzlich manuell hinzufügbare Pools.

Beobachtete `code`-Werte: `1`, `2`, `3`, `4`, `1-1`, `1-2`, `3-4`, `4-4`.
**Arbeitshypothese:** `code` kodiert „Anzahl bzw. Wahrscheinlichkeit" (z. B. `3` =
„genau einer aus dieser Gruppe, gewichtet", `2` = „optional, wahrscheinlich",
`a-b` = „a bis b Teile"), `db`/`dbPair` den Startpegel optionaler Parts.

**Vorgehen:** im Original pro Style N = 1000 Rolls ausführen, Rollen-Inventar
automatisch protokollieren (Slot-Belegung, Rollen, Pegel) → Verteilungen →
Regel-Engine dagegen fitten. Ergebnis nach `docs/specs/randomizer-rules.md`.

### 3.4 Auto-Gain (offener Punkt F4)

`[H]` sagt: Auto-Gain pegelt den **Body** des Kicks auf ~0 dBFS, nicht die
Transientenspitze; die Spitze darf in den Ceiling laufen („dieser Clip ist der Sound").

**Vorgehen:** identische Slot-Belegung mit Auto-Gain AN/AUS exportieren, Pegel- und
Hüllkurvenvergleich → Fensterlänge, Detektor (RMS/Peak-Fenster), Zielwert und
Ceiling-Verhalten bestimmen.

### 3.5 Kennlinien der Distortion (offener Punkt F5)

Zehn Modi (Hard Clip, Soft Clip, Saturate, Wavefold, Fuzz, Rectify, Half Wave,
Bitcrush, Waveshaper, Waveshaper Asym) mit gemeinsamem 0-dBFS-Ceiling und einem
PRE/POST/OFF-EQ-Chip.

**Vorgehen:** Gleichspannungs-/Rampen-Sweep und Sinus-Sweep durch das Modul schicken,
Ein-/Ausgangspaar aufnehmen → statische Kennlinie rekonstruieren (bei den
gedächtnislosen Modi exakt möglich; bei Fuzz/Half-Wave zusätzlich Hüllkurvenverhalten).
Ergebnis nach `docs/specs/distortion-curves.md`.

### 3.6 Filter (offener Punkt F6)

LP/HP mit 6/12/18/24/30 dB/Okt und „resonantem Knie" bedeutet eine Kaskade von
Biquads mit gemeinsamer Resonanz-Anhebung. NOTCH „immer bis zum Boden" mit
skalenabhängiger Breite ist ein Hinweis auf eine andere Topologie als BELL/PEAK.
PHASER mit 1–6 Notches und COMB mit Polarität sind separate Pfade.

**Vorgehen:** Frequenzgang-Messung (Sinus-Sweep, MLS) je Slope-Stufe und je RESO-Wert,
Pol-/Nullstellen-Fit → Topologie bestimmen.

---

## 4. Bewusst *nicht* weiter verfolgte Pfade

| Pfad | Warum nicht |
|---|---|
| Dekompilierung des gesamten DSP-Codes | Für einen funktionalen Nachbau nicht nötig und rechtlich heikel; Messung + Spezifikation reicht |
| Übernahme der `.edm`-Werksanalyse | Format proprietär, eigene Analyse ist gleichwertig und lizenzrein |
| Nachbau des Lizenzsystems | kein Interesse an Kopierschutz |
| Nachbau der Update-Infrastruktur | eigene Lösung über GitHub Releases |
| Übernahme von Marken/Logos/Schriftzügen | ausdrücklich ausgeschlossen |

---

## 5. Offene Punkte (Stand jetzt)

| ID | Frage | Status |
|---|---|---|
| F1 | Normalisierungskurven aller Parameter | **offen** — Messreihe geplant |
| F2 | `fxN_pM` → Modulparameter | **offen** — Zuordnungstabelle geplant |
| F3 | Pitch-Skala (Halbtonbereich, Rasterung) | **offen** |
| F4 | Auto-Gain-Fenster + Kick-Länge-Fade | **offen** |
| F5 | Distortion-Kennlinien | **offen** |
| F6 | Filter-Topologie/Slopes | **offen** |
| F7 | OTT-/Multiband-Zeitkonstanten | **offen** |
| F8 | ALIGN-/PHASE-ALIGN-Algorithmus | **offen** |
| F9 | `code`/`db`/`shares`-Semantik im Randomizer | **offen** — Messreihe geplant |
| F10 | `.edm`-Format | zurückgestellt (nicht nötig) |
| F11 | Key-Detection-Schwelle | **offen** |
| F12 | WSOLA-Fenster/Hop/Suchbereich | **offen** — Parameterstudie geplant |

Jeder Punkt wird geschlossen, sobald eine Spec-Datei unter `docs/specs/` vorliegt, die
im Parity-Test (B-130) reproduzierbare Werte liefert.
