# FX-Modul `dynEq` — Dynamic EQ (TDR-Nova-Integration)

**Ticket:** B-142 (Phase 2, Integration externer FX-Plugin-Funktionen)
**Code:** `src/fx/modules/DynamicEqModule.h/.cpp`
**Tests:** `tests/DynamicEqModuleTests.cpp` (11 Testfälle) + Kettentest in `tests/FxChainTests.cpp`
**Stand:** 2026-11-17

---

## 1. Zweck

`dynEq` (Anzeigename **Dynamic EQ**) ist ein **eigenständiger neuer Modultyp**
(Phase-2-Erweiterung, Referenz-Typ-String `"dynEq"`) und integriert die
Funktionen des **TDR Nova** (Tokyo Dawn Labs) in den Kick Architect. Das
bestehende Referenz-Modul `eq` bleibt unverändert, damit die 1:1-Parität der
44 Werk-Presets nicht gefährdet wird.

**Quellen (nur lesend analysiert, nichts davon im Repo):**

* Installer `TDR Nova - setup.exe` (Inno Setup), silente Installation nach
  `C:\Program Files\Common Files\VST3\TDR Nova.vst3` (7,4 MB).
* Bezeichner-/Enum-Inventar der VST3-Binary: `bandActive_N`, `bandType_N`,
  `bandFreq_N`, `bandGain_N`, `bandQ_N`, `bandDynActive_N`, `bandDynThreshold_N`,
  `bandDynRatio_N`, `bandDynAttack_N`, `bandDynRelease_N`, `bandDynSplit_N`,
  `hpActive_master`, `hpFreq_master`, `hpType_master` (6/12/24/72 dB/Okt),
  `lp*`, `bandGain_wide`, `bandDyn*_wide`, `bypass_master`, `delta_master`,
  `gain_master`, `dryMix_master`, `eqAutoGainParam`,
  `channelsParam` (Mono/Stereo/Sum/Diff/Left/Right/Dual Mono),
  `qualityParam` (Eco/Precise/Precise−), `sidechainModeParam`
  (Int SC / Ext SC / Int SC flat), Analyzer-Parameter.
* Offizielles Handbuch (docs.tokyodawn.net/nova-manual): paralleler Aufbau,
  W-Band, SPLIT/STICKY, Dry Mix, Output Gain, EQ Auto Gain, Prozessziel.
* Eine Standalone-Version gibt es für TDR Nova nicht.

---

## 2. Parameter (63)

### 2.1 Global (Index 0…18)

| # | `id` | Anzeige | Bereich | Default | Einheit | p |
|---|---|---|---|---|---|---|
| 0 | `outGain` | Output Gain | −24…+24 | 0 | dB | p0 |
| 1 | `dryMix` | Dry Mix | 0…100 | 0 | % | p1 |
| 2 | `hpActive` | Highpass Active | 0/1 | 0 | – | p2 |
| 3 | `hpFreq` | Highpass Freq | 10…20000 | 20 | Hz | p3 |
| 4 | `hpType` | Highpass Slope | 0…3 | 1 | – | p4 |
| 5 | `lpActive` | Lowpass Active | 0/1 | 0 | – | p5 |
| 6 | `lpFreq` | Lowpass Freq | 100…40000 | 20000 | Hz | p6 |
| 7 | `lpType` | Lowpass Slope | 0…3 | 1 | – | p7 |
| 8 | `wideGain` | W-Band Gain | −12…+12 | 0 | dB | p8 |
| 9 | `wideDyn` | W-Band Dyn | 0/1/2 | 0 | – | p9 |
| 10 | `wideThresh` | W-Band Threshold | −60…0 | −20 | dB | p10 |
| 11 | `wideRatio` | W-Band Ratio | 1…20 | 2 | – | p11 |
| 12 | `wideAttack` | W-Band Attack | 0,1…100 | 10 | ms | – |
| 13 | `wideRelease` | W-Band Release | 1…1000 | 100 | ms | – |
| 14 | `autoGain` | EQ Auto Gain | 0/1 | 1 | – | – |
| 15 | `delta` | Delta | 0/1 | 0 | – | – |
| 16 | `channels` | Channels | 0…6 | 1 | – | – |
| 17 | `quality` | Quality | 0…2 | 1 | – | – |
| 18 | `sidechain` | Sidechain | 0/1 | 0 | – | – |

* `hpType`/`lpType`: **0 = 6 dB/Okt** (einpolig), **1 = 12**, **2 = 24**,
  **3 = 72 dB/Okt** (6 Butterworth-Biquads).
* `channels`: 0 Mono, 1 Stereo, 2 Sum, 3 Diff, 4 Left, 5 Right, 6 Dual Mono.
* `quality`: 0 Eco, 1 Precise, 2 Precise− (wirkt nur auf die
  Analyse-/Detektorauflösung; siehe Abweichung N5).
* `sidechain`: 0 = intern (Detektor hochpassgefiltert 60 Hz), 1 = intern flach.

### 2.2 Bänder (Index 19…62, 4 × 11)

Pro Band `b1`…`b4`:

| Suffix | Anzeige | Bereich | Default |
|---|---|---|---|
| `_active` | Active | 0/1/2 (Off/On/Sticky) | 0 |
| `_type` | Type | 0/1/2 (Low-Shelf/Bell/High-Shelf) | 1 (Bell) |
| `_freq` | Frequency | 10…20000 Hz | 100 / 800 / 4000 / 12000 Hz |
| `_gain` | Gain | −18…+18 dB | 0 |
| `_q` | Q | 0,1…12 | 0,9 |
| `_dyn` | Dyn | 0/1/2 (Off/On/Sticky) | 0 |
| `_split` | Split | 0/1 | 0 |
| `_thresh` | Threshold | −60…0 dB | −20 |
| `_ratio` | Ratio | 1…20 | 2 |
| `_attack` | Attack | 0,1…100 ms | 10 |
| `_release` | Release | 1…1000 ms | 100 |

IDs also z. B. `b1_active`, `b1_type`, `b1_freq`, … `b4_release`.
Die Parameter `p0…p11` sind über die 14er-Bridge automatisierbar; alle
weiteren Attribute sind zustandsgebunden (Snapshot).

---

## 3. Signalfluss

```
Eingang
  └─ HP → LP (Eingangsfilter, vor dem Trockenabgriff)
  └─ W-Band: Gain + Kompressor (Threshold/Ratio/Attack/Release)
  └─ Band 1 … Band 4 (serielle Biquad-Kaskade, je Band statisch + dynamisch)
  └─ Output Gain → EQ Auto Gain → Delta/Dry Mix → Ausgang
```

### 3.1 Statische EQ-Kurve

Jedes Band ist ein Biquad (RBJ): Low-Shelf, Bell oder High-Shelf; Frequenz
10 Hz…20 kHz, Q 0,1…12, Gain ±18 dB. Ein Band mit 0 dB Gain ist neutral.

### 3.2 Dynamik je Band

* **Detektor:** Bandpass am Band-Schwerpunkt (Q des Bandes, min. 0,3) und
  Peak-Hüllkurve mit Attack/Release.
* **SPLIT = 1:** eigener Detektor und eigene Ratio/Attack/Release.
* **SPLIT = 0 (verlinkt, Nova-Default):** das Band folgt dem **W-Band-Detektor**
  und übernimmt Ratio/Attack/Release des W-Bands.
* **Regelgesetz:** `Regelweg = max(0, Pegel − Threshold) · (1 − 1/Ratio)`;
  die statische Verstärkung wird um diesen Betrag reduziert. Bei starker
  Dynamik kippt eine Anhebung in eine Absenkung (wie im Original).
* **Sticky** (`_dyn = 2`): unterhalb der Schwelle wird der letzte Regelweg
  gehalten (kein Release).

### 3.3 W-Band

Gain (±12 dB) plus eigener Kompressor mit Threshold/Ratio/Attack/Release;
`wideDyn = 2` hält den Regelweg unterhalb der Schwelle (Sticky). Das W-Band
liegt hier als **serielle Wideband-Stufe vor den Bändern** (Nova: parallel,
siehe N1).

### 3.4 Master-Funktionen

* **Auto Gain:** ermittelt die mittlere statische EQ-Verstärkung über 24
  log-spaced Frequenzen (20 Hz…20 kHz) und kompensiert sie langsam (300 ms).
* **Delta:** Ausgabe = Trocken − Nass (Regelweg-Differenz).
* **Dry Mix:** `0 %` = nass, `100 %` = Bypass.
* **Output Gain:** ±24 dB.
* **Prozessziel:** Mono summiert; Sum/Diff verarbeiten nur Mitten- bzw.
  Seitenanteil; Left/Right nur den jeweiligen Kanal; Dual Mono entkoppelt die
  Detektoren.

---

## 4. Bewusste Abweichungen

| # | Original | Nachbau | Grund |
|---|---|---|---|
| N1 | vollständig **parallele** Bandstruktur + W-Band als Rest des Spektrums | **serielle Biquad-Kaskade** + Wideband-Stufe | gleiche statische Kurve, deutlich einfacher und CPU-freundlicher; Band-Interaktionen weichen geringfügig ab |
| N2 | externer Sidechain (EXT SC) | nicht unterstützt; `sidechain` = intern/intern-flach | der Kick Architect ist ein Instrument ohne Sidechain-Bus |
| N3 | Analyzer (In/Out/SC), FFT-Boden, EQ-Range | nicht im Modul (UI folgt) | reine Anzeige |
| N4 | Preset-Manager, A/B, Undo, Order-by-Freq, Band-Solo | UI/Publisher, nicht DSP | folgt mit der FX-Seite (P9) |
| N5 | Quality Eco/Precise/Precise+ mit 100/200 kHz Bandbreite und Nichtlinearität | Parameter vorhanden; aktuell ohne Oversampling-Unterschied | Bandbreite ist bei 48 kHz irrelevant; dokumentiert |
| N6 | „Split" schaltet den eigenen Prozessor | wie beschrieben; Sticky semantisch vereinfacht | Handbuchbeschreibung deckt das Verhalten ab |
| N7 | HP/LP mit 6/12/24/72 dB/Okt | 6 (einpolig), 12, 24, 72 (6 Stufen) | 1:1 |

---

## 5. Getestete Eigenschaften

| Testfall | Prüft |
|---|---|
| Parameterliste | 63 IDs (19 global + 4×11 Bänder) in stabiler Reihenfolge |
| Klemmen | Wertebereiche, ungültige Indizes |
| Statisches Bell-Band | +6 dB bei 1 kHz (±1 dB), 100 Hz unbeeinflusst |
| Hochpass / Tiefpass | > 12 dB Dämpfung außerhalb, neutral im Durchlass |
| Band-Dynamik | lautes Signal: Regelweg < −5 dB, Ausgang ≈ −6 dB; leises Signal: statische Anhebung bleibt |
| Verlinktes Band | folgt dem W-Band-Detektor |
| Dry Mix 100 % | Bypass |
| Delta | Ausgang = Trocken − Nass (+9,5 dB bei +12 dB Boost) |
| Mono-Ziel | beide Kanäle identisch |
| Auto Gain | kompensiert statische Anhebung um > 2 dB |
| Kette | läuft stabil mit OTT und Convolver |
