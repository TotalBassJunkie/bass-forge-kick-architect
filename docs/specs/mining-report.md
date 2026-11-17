# Mining-Report: Referenz-Presets (Rohauswertung)

**Erzeugt von:** `tools/mine_presets.py` (Stand 2026-11-17)
**Datenbasis (nur lesend erfasst):**

| Quelle | Anzahl |
|---|---|
| Kick-Presets `.itkbpreset` | 44 |
| FX-Chain-Presets `.xml` | 76 |
| Presets mit eingebettetem FX-Snapshot | 43 |

Alle Werte sind aus den Klartext-XML-Dateien abgeleitet. Es werden **keine** Presetnamen, Sample-Pfade oder Handbuchtexte wiedergegeben.

---

## 1. Kennzahlen der Kick-Presets

| Kennzahl | Wert |
|---|---|
| Parameter-IDs insgesamt | 364 |
| davon `lN_*` (Slot) | 240 |
| davon global | 12 |
| davon `fxN_pM` | 112 |
| Presets | 44 |

Slot-Belegung: siehe JSON (`slots_per_preset`). Kettenformen: Abschnitt 4.

### 1.1 Rollup: Slots ueber alle N

Fuer jede Slot-Parameterart der beobachtete Wertebereich ueber alle 12 Slots und alle 44 Presets:

| Parameterart | n | min | max | Median-Bereich |
|---|---|---|---|---|
| `dlymix` | 528 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `dlytime` | 528 | 0.300000 | 0.300000 | 0.300000 … 0.300000 |
| `fadein` | 528 | 0.000000 | 1.000000 | 0.000000 … 0.000000 |
| `fadeout` | 528 | 0.000000 | 0.158200 | 0.000000 … 0.000000 |
| `ficurve` | 528 | 0.000000 | 0.500000 | 0.500000 … 0.500000 |
| `filter` | 528 | 1.000000 | 1.000000 | 1.000000 … 1.000000 |
| `filterq` | 528 | 0.102564 | 0.102564 | 0.102564 … 0.102564 |
| `fishape` | 528 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `foshape` | 528 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `mute` | 528 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `pitch` | 528 | 0.477958 | 1.000000 | 0.500000 … 0.500000 |
| `pos` | 528 | 0.407500 | 0.544000 | 0.500000 … 0.500000 |
| `solo` | 528 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `spare1` | 528 | 0.000000 | 0.710000 | 0.500000 … 0.500000 |
| `spare2` | 528 | 0.500000 | 0.500000 | 0.500000 … 0.500000 |
| `stretch` | 528 | 0.176683 | 0.408213 | 0.374989 … 0.374989 |
| `trimend` | 528 | 0.196200 | 1.000000 | 1.000000 … 1.000000 |
| `trimstart` | 528 | 0.000000 | 0.001500 | 0.000000 … 0.000000 |
| `vol` | 528 | 0.000000 | 0.804556 | 0.667929 … 0.699840 |

### 1.2 Globale Parameter

| ID | n | min | max | median | distinct | haeufigste Werte |
|---|---|---|---|---|---|---|
| `sat_amount` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | 0.000000 (44×) |
| `reverb_size` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | 0.500000 (44×) |
| `reverb_damp` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | 0.500000 (44×) |
| `reverb_mix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | 0.000000 (44×) |
| `hollow_amt` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | 0.000000 (44×) |
| `max_amount` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | 0.000000 (44×) |
| `master_vol` | 44 | 0.833333 | 1.000000 | 0.833333 | 2 | 0.833333 (43×), 1.000000 (1×) |
| `midi_mode` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | 0.000000 (44×) |
| `root_note` | 44 | 0.283465 | 0.283465 | 0.283465 | 1 | 0.283465 (44×) |
| `gspare1` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | 0.000000 (44×) |
| `gspare2` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | 0.500000 (44×) |
| `gspare3` | 44 | 0.000000 | 0.500000 | 0.500000 | 2 | 0.500000 (43×), 0.000000 (1×) |

## 2. FX-Parameter `fxN_pM` (normalisiert)

### 2.1 Rollup ueber alle Slots

| Index | n | min | max | Median-Bereich |
|---|---|---|---|---|
| `pp0` | 352 | 0.000000 | 1.000000 | 0.000000 … 0.500000 |
| `pp1` | 352 | 0.000000 | 1.000000 | 0.000000 … 0.529000 |
| `pp2` | 352 | 0.000000 | 0.887100 | 0.000000 … 0.500000 |
| `pp3` | 352 | 0.000000 | 1.000000 | 0.000000 … 0.500000 |
| `pp4` | 352 | 0.000000 | 0.817300 | 0.000000 … 0.162200 |
| `pp5` | 352 | 0.000000 | 1.000000 | 0.000000 … 0.500000 |
| `pp6` | 352 | 0.000000 | 1.000000 | 0.000000 … 0.709400 |
| `pp7` | 352 | 0.000000 | 1.000000 | 0.000000 … 0.500000 |
| `pp8` | 352 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `pp9` | 352 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `pp10` | 352 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `pp11` | 352 | 0.000000 | 0.000000 | 0.000000 … 0.000000 |
| `pp12` | 352 | 0.257000 | 1.000000 | 1.000000 … 1.000000 |
| `pp13` | 352 | 0.196000 | 0.654300 | 0.500000 … 0.500000 |

### 2.2 Wert pro Slot-Index (Mittel ueber p0..p10)

Aktive FX-Slots: siehe Abschnitt 4. Leere Slots sind an einem Muster aus `p12=1.0`, `p13=0.5` und sonst 0 erkennbar.

## 3. Modulattribute je `type=` (aus 76 Chains + 44 Snapshots)

### `dist` — 185 Vorkommen

Attributreihenfolge (kanonisch, stabilste Position zuerst):

| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |
|---|---|---|---|---|---|---|---|
| 0 | `on` | 0 | 1.00 | 185 | 1.000000 | 1.000000 | 1 |
| 1 | `mix` | 1 | 1.00 | 185 | 0.386000 | 1.000000 | 7 |
| 2 | `outGain` | 2 | 1.00 | 185 | 0.392002 | 2.000000 | 6 |
| 3 | `drive` | 3 | 1.00 | 185 | 0.000000 | 100.000000 | 55 |
| 4 | `mode` | 4 | 1.00 | 185 | 1.000000 | 12.000000 | 4 |
| 5 | `folds` | 5 | 1.00 | 185 | 2.000000 | 2.400300 | 2 |
| 6 | `agc` | 6 | 1.00 | 185 | 1.000000 | 1.000000 | 1 |
| 7 | `eqmode` | 7 | 1.00 | 185 | 0.000000 | 0.000000 | 1 |
| 8 | `flip` | 8 | 1.00 | 185 | 0.000000 | 0.000000 | 1 |

Kind-Elemente: `<WNode>` (98×)

| Kind-Attribut | n | min | max | distinct |
|---|---|---|---|---|
| `WNode.seg` | 47 | 1.000000 | 1.000000 | 1 |
| `WNode.ten` | 98 | -0.847286 | 0.950000 | 15 |
| `WNode.x` | 98 | -1.000000 | 1.000000 | 10 |
| `WNode.y` | 98 | -1.000000 | 1.000000 | 9 |

### `eq` — 138 Vorkommen

Attributreihenfolge (kanonisch, stabilste Position zuerst):

| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |
|---|---|---|---|---|---|---|---|
| 0 | `on` | 0 | 1.00 | 138 | 1.000000 | 1.000000 | 1 |
| 1 | `mix` | 1 | 1.00 | 138 | 1.000000 | 1.000000 | 1 |
| 2 | `outGain` | 2 | 1.00 | 138 | 0.726430 | 1.216395 | 4 |
| 3 | `lowgain` | 3 | 1.00 | 138 | 0.000000 | 0.000000 | 1 |
| 4 | `lowfreq` | 4 | 1.00 | 138 | 90.000000 | 90.000000 | 1 |
| 5 | `midgain` | 5 | 1.00 | 138 | 0.000000 | 0.000000 | 1 |
| 6 | `midfreq` | 6 | 1.00 | 138 | 900.000000 | 900.000000 | 1 |
| 7 | `midq` | 7 | 1.00 | 138 | 0.900000 | 0.900000 | 1 |
| 8 | `highgain` | 8 | 1.00 | 138 | 0.000000 | 0.000000 | 1 |
| 9 | `highfreq` | 9 | 1.00 | 138 | 8000.000000 | 8000.000000 | 1 |
| 10 | `middyn` | 10 | 1.00 | 138 | 0.000000 | 0.000000 | 1 |

Kind-Elemente: `<Band>` (340×)

| Kind-Attribut | n | min | max | distinct |
|---|---|---|---|---|
| `Band.ch` | 340 | 0.000000 | 0.000000 | 1 |
| `Band.dyn` | 340 | 0.000000 | 0.000000 | 1 |
| `Band.f` | 340 | 38.476601 | 14215.909180 | 140 |
| `Band.g` | 340 | -32.000000 | 29.815826 | 139 |
| `Band.kind` | 340 | 0.000000 | 4.000000 | 4 |
| `Band.q` | 340 | 0.300000 | 2.211514 | 25 |

### `final` — 49 Vorkommen

Attributreihenfolge (kanonisch, stabilste Position zuerst):

| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |
|---|---|---|---|---|---|---|---|
| 0 | `on` | 0 | 1.00 | 49 | 1.000000 | 1.000000 | 1 |
| 1 | `mix` | 1 | 1.00 | 49 | 1.000000 | 1.000000 | 1 |
| 2 | `outGain` | 2 | 1.00 | 49 | 0.892784 | 1.000000 | 2 |
| 3 | `boost` | 3 | 1.00 | 49 | 0.354900 | 12.000000 | 15 |
| 4 | `freq` | 4 | 1.00 | 49 | 35.751301 | 95.694901 | 8 |
| 5 | `clip` | 5 | 1.00 | 49 | 0.000000 | 37.228600 | 9 |
| 6 | `upward` | 6 | 1.00 | 49 | 20.000000 | 20.000000 | 1 |
| 7 | `monoF` | 7 | 1.00 | 49 | 60.000000 | 181.049896 | 7 |
| 8 | `shelfOn` | 8 | 1.00 | 49 | 1.000000 | 1.000000 | 1 |
| 9 | `monoOn` | 9 | 1.00 | 49 | 1.000000 | 1.000000 | 1 |
| 10 | `clipOn` | 10 | 1.00 | 49 | 0.000000 | 1.000000 | 2 |

### `mseq` — 94 Vorkommen

Attributreihenfolge (kanonisch, stabilste Position zuerst):

| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |
|---|---|---|---|---|---|---|---|
| 0 | `on` | 0 | 1.00 | 94 | 1.000000 | 1.000000 | 1 |
| 1 | `mix` | 1 | 1.00 | 94 | 1.000000 | 1.000000 | 1 |
| 2 | `outGain` | 2 | 1.00 | 94 | 0.689931 | 1.308515 | 3 |
| 3 | `midlow` | 3 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 4 | `midhigh` | 4 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 5 | `sidelow` | 5 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 6 | `sidehigh` | 6 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 7 | `width` | 7 | 1.00 | 94 | 100.000000 | 100.000000 | 1 |
| 8 | `middyn` | 8 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 9 | `sidedyn` | 9 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 10 | `msolo` | 10 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 11 | `ssolo` | 11 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 12 | `mmute` | 12 | 1.00 | 94 | 0.000000 | 0.000000 | 1 |
| 13 | `smute` | 13 | 1.00 | 94 | 0.000000 | 1.000000 | 2 |

Kind-Elemente: `<Band>` (289×)

| Kind-Attribut | n | min | max | distinct |
|---|---|---|---|---|
| `Band.ch` | 289 | 0.000000 | 1.000000 | 2 |
| `Band.dyn` | 289 | 0.000000 | 0.000000 | 1 |
| `Band.f` | 289 | 52.084999 | 14744.519531 | 108 |
| `Band.g` | 289 | -21.267023 | 28.088749 | 82 |
| `Band.kind` | 289 | 0.000000 | 4.000000 | 5 |
| `Band.q` | 289 | 0.300000 | 4.260968 | 25 |

### `ott` — 27 Vorkommen

Attributreihenfolge (kanonisch, stabilste Position zuerst):

| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |
|---|---|---|---|---|---|---|---|
| 0 | `on` | 0 | 1.00 | 27 | 1.000000 | 1.000000 | 1 |
| 1 | `mix` | 1 | 1.00 | 27 | 1.000000 | 1.000000 | 1 |
| 2 | `outGain` | 2 | 1.00 | 27 | 1.000000 | 2.000000 | 3 |
| 3 | `depth` | 3 | 1.00 | 27 | 8.536900 | 41.004700 | 8 |
| 4 | `time` | 4 | 1.00 | 27 | 100.000000 | 116.702698 | 2 |
| 5 | `x1` | 5 | 1.00 | 27 | 120.000000 | 323.296326 | 2 |
| 6 | `x2` | 6 | 1.00 | 27 | 2500.000000 | 2500.000000 | 1 |
| 7 | `gl` | 7 | 1.00 | 27 | 0.000000 | 7.615000 | 3 |
| 8 | `gm` | 8 | 1.00 | 27 | -2.793100 | 7.558000 | 8 |
| 9 | `gh` | 9 | 1.00 | 27 | 0.000000 | 0.000000 | 1 |
| 10 | `linear` | 10 | 1.00 | 27 | 0.000000 | 0.000000 | 1 |

### `reverb` — 81 Vorkommen

Attributreihenfolge (kanonisch, stabilste Position zuerst):

| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |
|---|---|---|---|---|---|---|---|
| 0 | `on` | 0 | 1.00 | 81 | 1.000000 | 1.000000 | 1 |
| 1 | `mix` | 1 | 1.00 | 81 | 0.257000 | 0.520000 | 18 |
| 2 | `outGain` | 2 | 1.00 | 81 | 1.000000 | 1.000000 | 1 |
| 3 | `wet` | 3 | 1.00 | 81 | 35.000000 | 100.000000 | 9 |
| 4 | `stereo` | 4 | 1.00 | 81 | 0.000000 | 100.000000 | 15 |
| 5 | `tone` | 5 | 1.00 | 81 | 3.084800 | 100.000000 | 23 |
| 6 | `speed` | 6 | 1.00 | 81 | 0.000000 | 11.778200 | 2 |

Kind-Elemente: `<Pt>` (205×)

| Kind-Attribut | n | min | max | distinct |
|---|---|---|---|---|
| `Pt.t` | 205 | -1.000000 | 1.000000 | 29 |
| `Pt.x` | 205 | 0.000000 | 1.000000 | 16 |
| `Pt.y` | 205 | 0.000000 | 1.000000 | 9 |

### `shaper` — 6 Vorkommen

Attributreihenfolge (kanonisch, stabilste Position zuerst):

| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |
|---|---|---|---|---|---|---|---|
| 0 | `on` | 0 | 1.00 | 6 | 1.000000 | 1.000000 | 1 |
| 1 | `mix` | 1 | 1.00 | 6 | 1.000000 | 1.000000 | 1 |
| 2 | `outGain` | 2 | 1.00 | 6 | 1.000000 | 1.000000 | 1 |
| 3 | `sPre` | 3 | 1.00 | 6 | 1.000000 | 1.000000 | 1 |
| 4 | `sPost` | 4 | 1.00 | 6 | 1.000000 | 1.000000 | 1 |
| 5 | `sLine` | 5 | 1.00 | 6 | 1.000000 | 1.000000 | 1 |
| 6 | `shape` | 6 | 1.00 | 6 | 0.000000 | 2.000000 | 2 |
| 7 | `amount` | 7 | 1.00 | 6 | 0.000000 | 100.000000 | 5 |

### `trans` — 59 Vorkommen

Attributreihenfolge (kanonisch, stabilste Position zuerst):

| # | Attribut | Position | Positionsstabilitaet | n | min | max | distinct |
|---|---|---|---|---|---|---|---|
| 0 | `on` | 0 | 1.00 | 59 | 1.000000 | 1.000000 | 1 |
| 1 | `mix` | 1 | 1.00 | 59 | 1.000000 | 1.000000 | 1 |
| 2 | `outGain` | 2 | 1.00 | 59 | 0.945886 | 1.280745 | 3 |
| 3 | `amount` | 3 | 1.00 | 59 | 100.000000 | 100.000000 | 1 |
| 4 | `attack` | 4 | 1.00 | 59 | 0.000000 | 100.000000 | 22 |
| 5 | `duck` | 5 | 1.00 | 59 | 0.000000 | 63.016899 | 12 |
| 6 | `speed` | 6 | 1.00 | 59 | 47.160000 | 179.528107 | 19 |

## 4. Kreuztabelle: Preset → FX-Kette (Kern von F2)

### 4.1 Verteilung der Kettenformen

| Modulreihenfolge | Presets |
|---|---|
| `eq,reverb,mseq,dist` | 4 |
| `reverb,dist,final` | 3 |
| `eq,trans,reverb,ott,mseq,dist,dist` | 3 |
| `reverb,reverb,eq,mseq,dist,final` | 2 |
| `trans,eq,mseq,dist` | 2 |
| `eq,mseq,trans,dist` | 2 |
| `eq,reverb,mseq,trans,dist,final` | 2 |
| `mseq,trans,dist` | 2 |
| `eq,mseq,dist` | 2 |
| `reverb,reverb,trans,eq,mseq,dist` | 2 |
| `eq,dist` | 2 |
| `eq,dist,reverb,dist,eq,mseq,dist` | 2 |
| `reverb,dist,final,dist` | 1 |
| `eq,dist,eq,trans,mseq,dist,eq,dist` | 1 |
| `dist,final,dist` | 1 |
| `reverb,eq,dist` | 1 |
| `` | 1 |
| `eq,trans,mseq,dist` | 1 |
| `eq,trans,dist` | 1 |
| `eq,trans,reverb,mseq,dist` | 1 |
| `eq,mseq,trans,dist,final` | 1 |
| `eq,mseq,reverb,trans,dist,final` | 1 |
| `shaper,dist,ott,final,mseq,dist` | 1 |
| `ott,dist,eq,mseq,reverb,dist` | 1 |
| `mseq,eq,eq,dist` | 1 |
| `trans,mseq,dist,dist` | 1 |
| `eq,reverb,dist,eq,mseq,dist` | 1 |
| `dist,eq,mseq,reverb,eq,dist` | 1 |

### 4.2 Modultyp je Kettenposition

| Position | Typen (Anzahl) |
|---|---|
| 0 | `eq` (24), `reverb` (9), `trans` (3), `mseq` (3), `dist` (2), `shaper` (1), `ott` (1) |
| 1 | `reverb` (11), `dist` (11), `trans` (8), `mseq` (7), `eq` (5), `final` (1) |
| 2 | `mseq` (10), `dist` (9), `reverb` (7), `eq` (5), `trans` (5), `final` (4), `ott` (1) |
| 3 | `dist` (15), `mseq` (4), `trans` (4), `ott` (3), `eq` (3), `final` (1), `reverb` (1) |
| 4 | `mseq` (8), `dist` (6), `eq` (3), `final` (1), `reverb` (1) |
| 5 | `dist` (10), `final` (5), `mseq` (2) |
| 6 | `dist` (5), `eq` (1) |
| 7 | `dist` (1) |

### 4.3 Details je Preset

Vollstaendig im JSON (`crosstab.rows`). Kompakt hier:

| Preset | Kette | p12 je Slot |
|---|---|---|
| `preset_01` | `eq,reverb,mseq,dist` | 1.000, 0.500, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_02` | `reverb,dist,final,dist` | 0.271, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_03` | `reverb,reverb,eq,mseq,dist,final` | 0.385, 0.460, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_04` | `eq,reverb,mseq,dist` | 1.000, 0.500, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_05` | `eq,dist,eq,trans,mseq,dist,eq,dist` | 1.000, 0.506, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_06` | `reverb,reverb,eq,mseq,dist,final` | 0.385, 0.460, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_07` | `dist,final,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_08` | `eq,reverb,mseq,dist` | 1.000, 0.500, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_09` | `reverb,eq,dist` | 0.487, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_10` | `reverb,dist,final` | 0.500, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_11` | `trans,eq,mseq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_12` | `` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_13` | `reverb,dist,final` | 0.500, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_14` | `eq,trans,mseq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_15` | `eq,trans,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_16` | `eq,trans,reverb,mseq,dist` | 1.000, 1.000, 0.293, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_17` | `eq,trans,reverb,ott,mseq,dist,dist` | 1.000, 1.000, 0.319, 1.000, 1.000, 0.386, 1.000, 1.000 |
| `preset_18` | `eq,mseq,trans,dist,final` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_19` | `eq,mseq,trans,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_20` | `eq,reverb,mseq,trans,dist,final` | 1.000, 0.434, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_21` | `eq,reverb,mseq,trans,dist,final` | 1.000, 0.434, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_22` | `eq,mseq,reverb,trans,dist,final` | 1.000, 1.000, 0.434, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_23` | `eq,mseq,trans,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_24` | `eq,trans,reverb,ott,mseq,dist,dist` | 1.000, 1.000, 0.319, 1.000, 1.000, 0.386, 1.000, 1.000 |
| `preset_25` | `mseq,trans,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_26` | `shaper,dist,ott,final,mseq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_27` | `ott,dist,eq,mseq,reverb,dist` | 1.000, 1.000, 1.000, 1.000, 0.469, 1.000, 1.000, 1.000 |
| `preset_28` | `eq,mseq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_29` | `eq,trans,reverb,ott,mseq,dist,dist` | 1.000, 1.000, 0.319, 1.000, 1.000, 0.386, 1.000, 1.000 |
| `preset_30` | `mseq,trans,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_31` | `reverb,dist,final` | 0.500, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_32` | `mseq,eq,eq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_33` | `trans,eq,mseq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_34` | `reverb,reverb,trans,eq,mseq,dist` | 0.385, 0.257, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_35` | `reverb,reverb,trans,eq,mseq,dist` | 0.385, 0.257, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_36` | `eq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_37` | `trans,mseq,dist,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_38` | `eq,mseq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_39` | `eq,reverb,dist,eq,mseq,dist` | 1.000, 0.294, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_40` | `eq,dist,reverb,dist,eq,mseq,dist` | 1.000, 0.704, 0.294, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_41` | `eq,dist` | 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_42` | `dist,eq,mseq,reverb,eq,dist` | 1.000, 1.000, 1.000, 0.444, 1.000, 1.000, 1.000, 1.000 |
| `preset_43` | `eq,dist,reverb,dist,eq,mseq,dist` | 1.000, 0.704, 0.294, 1.000, 1.000, 1.000, 1.000, 1.000 |
| `preset_44` | `eq,reverb,mseq,dist` | 1.000, 0.500, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 |

## 5. Kandidaten: Modulattribut → `fxN_pM`

Verfahren: fuer jede Kombination (Modultyp, Attribut, p-Index) werden alle Beobachtungen aus den 44 Preset-Snapshots gesammelt. Ein Kandidat gilt als **exakt reproduzierend**, wenn eine lineare **oder** logarithmische Abbildung alle Punkte mit Toleranz 0.0015 trifft.

| Modultyp | Attribut | n | distinct(real) | Kandidaten (p, Trefferquote, Modell) |
|---|---|---|---|---|
| `dist` | `agc` | 59 | 1 | keine |
| `dist` | `drive` | 59 | 34 | p0: 100% lin; p13: 97% lin; p12: 8% lin |
| `dist` | `eqmode` | 59 | 1 | keine |
| `dist` | `flip` | 59 | 1 | keine |
| `dist` | `folds` | 59 | 1 | keine |
| `dist` | `mix` | 59 | 4 | p12: 100% lin; p13: 97% lin; p13: 97% log |
| `dist` | `mode` | 59 | 4 | p1: 100% lin; p1: 97% log; p13: 97% lin |
| `dist` | `on` | 59 | 1 | keine |
| `dist` | `outGain` | 59 | 3 | p13: 100% lin; p13: 98% log; p12: 90% lin |
| `eq` | `highfreq` | 41 | 1 | keine |
| `eq` | `highgain` | 41 | 1 | keine |
| `eq` | `lowfreq` | 41 | 1 | keine |
| `eq` | `lowgain` | 41 | 1 | keine |
| `eq` | `middyn` | 41 | 1 | keine |
| `eq` | `midfreq` | 41 | 1 | keine |
| `eq` | `midgain` | 41 | 1 | keine |
| `eq` | `midq` | 41 | 1 | keine |
| `eq` | `mix` | 41 | 1 | keine |
| `eq` | `on` | 41 | 1 | keine |
| `eq` | `outGain` | 41 | 1 | keine |
| `final` | `boost` | 12 | 7 | p0: 100% lin; p2: 50% log; p2: 33% lin |
| `final` | `clip` | 12 | 4 | p2: 100% lin; p1: 50% lin; p4: 50% lin |
| `final` | `clipOn` | 12 | 1 | keine |
| `final` | `freq` | 12 | 4 | p1: 100% lin; p1: 67% log; p2: 58% lin |
| `final` | `mix` | 12 | 1 | keine |
| `final` | `monoF` | 12 | 4 | p4: 100% lin; p4: 75% log; p2: 75% log |
| `final` | `monoOn` | 12 | 1 | keine |
| `final` | `on` | 12 | 1 | keine |
| `final` | `outGain` | 12 | 1 | keine |
| `final` | `shelfOn` | 12 | 1 | keine |
| `final` | `upward` | 12 | 1 | keine |
| `mseq` | `middyn` | 34 | 1 | keine |
| `mseq` | `midhigh` | 34 | 1 | keine |
| `mseq` | `midlow` | 34 | 1 | keine |
| `mseq` | `mix` | 34 | 1 | keine |
| `mseq` | `mmute` | 34 | 1 | keine |
| `mseq` | `msolo` | 34 | 1 | keine |
| `mseq` | `on` | 34 | 1 | keine |
| `mseq` | `outGain` | 34 | 2 | p13: 100% lin; p13: 100% log |
| `mseq` | `sidedyn` | 34 | 1 | keine |
| `mseq` | `sidehigh` | 34 | 1 | keine |
| `mseq` | `sidelow` | 34 | 1 | keine |
| `mseq` | `smute` | 34 | 1 | keine |
| `mseq` | `ssolo` | 34 | 1 | keine |
| `mseq` | `width` | 34 | 1 | keine |
| `ott` | `depth` | 5 | 3 | p0: 100% lin; p4: 80% log; p13: 80% lin |
| `ott` | `gh` | 5 | 1 | keine |
| `ott` | `gl` | 5 | 3 | p4: 100% lin; p0: 80% lin; p13: 80% lin |
| `ott` | `gm` | 5 | 3 | p5: 100% lin; p13: 80% lin; p0: 80% lin |
| `ott` | `linear` | 5 | 1 | keine |
| `ott` | `mix` | 5 | 1 | keine |
| `ott` | `on` | 5 | 1 | keine |
| `ott` | `outGain` | 5 | 2 | p2: 100% lin; p13: 100% lin; p2: 100% log |
| `ott` | `time` | 5 | 1 | keine |
| `ott` | `x1` | 5 | 2 | p2: 100% lin; p13: 100% lin; p2: 100% log |
| `ott` | `x2` | 5 | 1 | keine |
| `reverb` | `mix` | 29 | 12 | p12: 100% lin; p12: 31% log; p0: 31% lin |
| `reverb` | `on` | 29 | 1 | keine |
| `reverb` | `outGain` | 29 | 1 | keine |
| `reverb` | `speed` | 29 | 1 | keine |
| `reverb` | `stereo` | 29 | 6 | p1: 100% lin; p0: 83% lin; p0: 83% log |
| `reverb` | `tone` | 29 | 13 | p2: 100% lin; p12: 10% lin; p12: 10% log |
| `reverb` | `wet` | 29 | 4 | p0: 100% lin; p0: 90% log; p12: 31% lin |
| `trans` | `amount` | 20 | 1 | keine |
| `trans` | `attack` | 20 | 11 | p1: 100% lin; p13: 95% lin; p2: 20% lin |
| `trans` | `duck` | 20 | 8 | p2: 100% lin; p13: 95% lin; p3: 35% lin |
| `trans` | `mix` | 20 | 1 | keine |
| `trans` | `on` | 20 | 1 | keine |
| `trans` | `outGain` | 20 | 2 | p13: 100% lin; p13: 100% log; p3: 10% lin |
| `trans` | `speed` | 20 | 9 | p3: 100% lin; p13: 95% lin; p13: 95% log |

## 6. Normalisierungskurven: Pruefungen

### 6.1 Verhaeltnis-Hypothesen

**`root_note`**

```json
{
  "param": "root_note",
  "observed_median": 0.283464581,
  "observed_values": [
    {
      "value": 0.283465,
      "count": 44,
      "share": 1.0
    }
  ],
  "hypothesis": "MIDI-Note n / 127 (n=36 -> C1)",
  "predicted_36_127": 0.28346456692913385,
  "delta": 1.4070866138915505e-08,
  "match": true,
  "top_value_note_estimates": [
    {
      "v": 0.283465,
      "n_127": 36.0001,
      "count": 44
    }
  ]
}
```

**`master_vol`**

```json
{
  "param": "master_vol",
  "observed_median": 0.833333313,
  "observed_values": [
    {
      "value": 0.833333,
      "count": 43,
      "share": 0.9773
    },
    {
      "value": 1.0,
      "count": 1,
      "share": 0.0227
    }
  ],
  "ratio_5_6": 0.9999999756,
  "db_20log10": -1.5836251328882063,
  "hypothesis": "linear 0..1.2 (0.8333 = 1/1.2) oder 0..1.2 Bereich",
  "as_gain_1_over": 1.2000000292800006
}
```

**`l0_stretch`**

```json
{
  "param": "l0_stretch",
  "observed_median": 0.37498942,
  "hypotheses": {
    "log_0.25_8": 0.91697,
    "log_0.25_4": 0.707086,
    "lin_0.25_8": 3.156168
  },
  "note": "1.0 als Zielwert pruefen: bei welchem v liegt Faktor 1.0?"
}
```

### 6.2 Wertebereichs-/Symmetriepruefung

| Parameter | n | min | max | median | distinct | voll 0..1 | Median=0.5 | Anteil 0.5 | Symmetrie um 0.5 |
|---|---|---|---|---|---|---|---|---|---|
| `master_vol` | 44 | 0.833333 | 1.000000 | 0.833333 | 2 | nein | nein | 0.0 | 0.8333 |
| `root_note` | 44 | 0.283465 | 0.283465 | 0.283465 | 1 | nein | nein | 0.0 | 0.4331 |
| `sat_amount` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `reverb_size` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `reverb_damp` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `reverb_mix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `hollow_amt` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `max_amount` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `midi_mode` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l0_vol` | 44 | 0.699840 | 0.804556 | 0.699840 | 2 | nein | nein | 0.0 | 0.5044 |
| `l0_pitch` | 44 | 0.477958 | 0.543896 | 0.500000 | 6 | nein | ja | 0.7045 | 0.0219 |
| `l0_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l0_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l0_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l0_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l0_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l0_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l0_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l0_fadeout` | 44 | 0.000000 | 0.158200 | 0.000000 | 7 | nein | nein | 0.0 | 0.8418 |
| `l0_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l0_trimend` | 44 | 0.196200 | 1.000000 | 1.000000 | 5 | nein | nein | 0.0 | 0.1962 |
| `l0_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l0_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l0_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l1_vol` | 44 | 0.495896 | 0.737372 | 0.667929 | 11 | nein | nein | 0.0 | 0.2333 |
| `l1_pitch` | 44 | 0.500000 | 0.957583 | 0.500000 | 3 | nein | ja | 0.9545 | 0.4576 |
| `l1_stretch` | 44 | 0.344057 | 0.374989 | 0.374989 | 2 | nein | nein | 0.0 | 0.2810 |
| `l1_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l1_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l1_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l1_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l1_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l1_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l1_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l1_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l1_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l1_ficurve` | 44 | 0.068000 | 0.500000 | 0.500000 | 2 | nein | ja | 0.9773 | 0.4320 |
| `l1_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l1_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l2_vol` | 44 | 0.626584 | 0.699840 | 0.699840 | 3 | nein | nein | 0.0 | 0.3264 |
| `l2_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l2_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l2_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l2_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l2_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l2_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l2_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l2_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l2_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l2_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l2_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l2_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l2_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l2_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l3_vol` | 44 | 0.683358 | 0.699840 | 0.699840 | 2 | nein | nein | 0.0 | 0.3832 |
| `l3_pitch` | 44 | 0.500000 | 1.000000 | 0.500000 | 3 | nein | ja | 0.8636 | 0.5000 |
| `l3_stretch` | 44 | 0.176683 | 0.374989 | 0.374989 | 2 | nein | nein | 0.0 | 0.4483 |
| `l3_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l3_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l3_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l3_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l3_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l3_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l3_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l3_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l3_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l3_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l3_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l3_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l4_vol` | 44 | 0.552330 | 0.743960 | 0.699840 | 5 | nein | nein | 0.0 | 0.2963 |
| `l4_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l4_stretch` | 44 | 0.374989 | 0.408213 | 0.374989 | 2 | nein | nein | 0.0 | 0.2168 |
| `l4_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l4_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l4_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l4_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l4_pos` | 44 | 0.407500 | 0.544000 | 0.500000 | 11 | nein | ja | 0.7273 | 0.0485 |
| `l4_fadein` | 44 | 0.000000 | 1.000000 | 0.000000 | 5 | ja | nein | 0.0 | 0.0000 |
| `l4_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l4_trimstart` | 44 | 0.000000 | 0.001500 | 0.000000 | 2 | nein | nein | 0.0 | 0.9985 |
| `l4_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l4_ficurve` | 44 | 0.000000 | 0.500000 | 0.500000 | 3 | nein | ja | 0.7045 | 0.5000 |
| `l4_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l4_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l5_vol` | 44 | 0.626584 | 0.699840 | 0.699840 | 2 | nein | nein | 0.0 | 0.3264 |
| `l5_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l5_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l5_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l5_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l5_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l5_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l5_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l5_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l5_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l5_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l5_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l5_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l5_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l5_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l6_vol` | 44 | 0.699840 | 0.699840 | 0.699840 | 1 | nein | nein | 0.0 | 0.3997 |
| `l6_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l6_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l6_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l6_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l6_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l6_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l6_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l6_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l6_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l6_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l6_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l6_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l6_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l6_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l7_vol` | 44 | 0.699840 | 0.699840 | 0.699840 | 1 | nein | nein | 0.0 | 0.3997 |
| `l7_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l7_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l7_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l7_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l7_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l7_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l7_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l7_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l7_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l7_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l7_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l7_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l7_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l7_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l8_vol` | 44 | 0.000000 | 0.699840 | 0.699840 | 2 | nein | nein | 0.0 | 0.3002 |
| `l8_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l8_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l8_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l8_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l8_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l8_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l8_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l8_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l8_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l8_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l8_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l8_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l8_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l8_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l9_vol` | 44 | 0.699840 | 0.699840 | 0.699840 | 1 | nein | nein | 0.0 | 0.3997 |
| `l9_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l9_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l9_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l9_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l9_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l9_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l9_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l9_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l9_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l9_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l9_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l9_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l9_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l9_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l10_vol` | 44 | 0.626584 | 0.699840 | 0.699840 | 2 | nein | nein | 0.0 | 0.3264 |
| `l10_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l10_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l10_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l10_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l10_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l10_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l10_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l10_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l10_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l10_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l10_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l10_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l10_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l10_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l11_vol` | 44 | 0.699840 | 0.699840 | 0.699840 | 1 | nein | nein | 0.0 | 0.3997 |
| `l11_pitch` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l11_stretch` | 44 | 0.374989 | 0.374989 | 0.374989 | 1 | nein | nein | 0.0 | 0.2500 |
| `l11_filter` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l11_filterq` | 44 | 0.102564 | 0.102564 | 0.102564 | 1 | nein | nein | 0.0 | 0.7949 |
| `l11_dlytime` | 44 | 0.300000 | 0.300000 | 0.300000 | 1 | nein | nein | 0.0 | 0.4000 |
| `l11_dlymix` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l11_pos` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l11_fadein` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l11_fadeout` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l11_trimstart` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l11_trimend` | 44 | 1.000000 | 1.000000 | 1.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l11_ficurve` | 44 | 0.500000 | 0.500000 | 0.500000 | 1 | nein | ja | 1.0 | 0.0000 |
| `l11_fishape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |
| `l11_foshape` | 44 | 0.000000 | 0.000000 | 0.000000 | 1 | nein | nein | 0.0 | 1.0000 |

---

## 7. Rohdaten

Vollstaendige Auswertung: `_private/mining/report.json` (nicht Teil des oeffentlichen Repos).
