# Mining-Report: Referenz-Presets (Rohauswertung)

Automatisch erzeugt von [`tools/mine_presets.py`](../../tools/mine_presets.py). Stand: 2026-11-17.

| Datenquelle (nur lesend) | Anzahl |
|---|---|
| Kick-Presets `.itkbpreset` | 44 |
| FX-Chain-Presets `.xml` | 76 |
| Presets mit eingebettetem FX-Snapshot | 43 |

> Alle Werte sind aus den Klartext-XML-Dateien abgeleitet. Es werden **keine** Presetnamen, Sample-Pfade oder Handbuchtexte wiedergegeben; die Presets sind als `preset_01…preset_44` anonymisiert. Rohdaten: `_private/mining/report.json`.

---

## 1. Slot-Belegung

| Rollenname | Vorkommen |
|---|---|
| `PUNCH` | 44 |
| `CLICK` | 33 |
| `RUMBLE` | 26 |
| `TOP` | 18 |
| `CRUNCH` | 18 |
| `CLICK 2` | 6 |
| `SCHRANZ ENHANCER` | 6 |
| `SCHRANZ ENHANCER 2` | 6 |
| `PUNCH ENHANCER` | 4 |

| Slot-Index | Vorkommen |
|---|---|
| 0 | 44 |
| 1 | 10 |
| 3 | 33 |
| 4 | 44 |
| 6 | 6 |
| 7 | 6 |
| 8 | 18 |

Haeufigste Rollenkombinationen:

* `CLICK,PUNCH,RUMBLE,TOP` — 9 Presets
* `CLICK,CRUNCH,PUNCH` — 9 Presets
* `PUNCH,RUMBLE` — 7 Presets
* `CLICK,PUNCH,RUMBLE,SCHRANZ ENHANCER,SCHRANZ ENHANCER 2,TOP` — 6 Presets
* `CLICK,CLICK 2,CRUNCH,PUNCH` — 4 Presets
* `CLICK,CRUNCH,PUNCH,TOP` — 2 Presets
* `CRUNCH,PUNCH,PUNCH ENHANCER` — 2 Presets
* `CLICK,CLICK 2,PUNCH,RUMBLE` — 2 Presets
* `PUNCH,PUNCH ENHANCER,RUMBLE` — 1 Presets
* `CLICK,PUNCH,PUNCH ENHANCER,RUMBLE,TOP` — 1 Presets

## 2. Slot-Parameter `lN_*`

Rollup ueber alle 12 Slots (Wertebereich und Werksdefault):

| Parameterart | IDs | n | min | max | Default(s) | distinct (max) |
|---|---|---|---|---|---|---|
| `lN_dlymix` | 12 | 528 | 0.000000 | 0.000000 | 0.000000000 | 1 |
| `lN_dlytime` | 12 | 528 | 0.300000 | 0.300000 | 0.300000012 | 1 |
| `lN_fadein` | 12 | 528 | 0.000000 | 1.000000 | 0.000000000 | 5 |
| `lN_fadeout` | 12 | 528 | 0.000000 | 0.158200 | 0.000000000 | 7 |
| `lN_ficurve` | 12 | 528 | 0.000000 | 0.500000 | 0.500000000 | 3 |
| `lN_filter` | 12 | 528 | 1.000000 | 1.000000 | 1.000000000 | 1 |
| `lN_filterq` | 12 | 528 | 0.102564 | 0.102564 | 0.102564096 | 1 |
| `lN_fishape` | 12 | 528 | 0.000000 | 0.000000 | 0.000000000 | 1 |
| `lN_fistart` | 12 | 12 | 0.000000 | 0.000000 | 0.000000000 | 1 |
| `lN_foshape` | 12 | 528 | 0.000000 | 0.000000 | 0.000000000 | 1 |
| `lN_mute` | 12 | 528 | 0.000000 | 0.000000 | 0.000000000 | 1 |
| `lN_pitch` | 12 | 528 | 0.477958 | 1.000000 | 0.500000060 | 6 |
| `lN_pos` | 12 | 528 | 0.407500 | 0.544000 | 0.500000000 | 11 |
| `lN_solo` | 12 | 528 | 0.000000 | 0.000000 | 0.000000000 | 1 |
| `lN_spare1` | 12 | 528 | 0.000000 | 0.710000 | 0.500000000 | 4 |
| `lN_spare2` | 12 | 528 | 0.500000 | 0.500000 | 0.500000000 | 1 |
| `lN_stretch` | 12 | 528 | 0.176683 | 0.408213 | 0.374989420 | 2 |
| `lN_trimend` | 12 | 528 | 0.196200 | 1.000000 | 1.000000000 | 5 |
| `lN_trimstart` | 12 | 528 | 0.000000 | 0.001500 | 0.000000000 | 2 |
| `lN_vol` | 12 | 528 | 0.000000 | 0.804556 | 0.699840486 | 11 |

## 3. Globale Parameter

| ID | n | min | max | Default | Anteil Default | distinct | Bruch-Kandidat |
|---|---|---|---|---|---|---|---|
| `sat_amount` | 44 | 0.000000 | 0.000000 | 0.000000000 | 100.0 % | 1 | – |
| `reverb_size` | 44 | 0.500000 | 0.500000 | 0.500000000 | 100.0 % | 1 | 1/2 |
| `reverb_damp` | 44 | 0.500000 | 0.500000 | 0.500000000 | 100.0 % | 1 | 1/2 |
| `reverb_mix` | 44 | 0.000000 | 0.000000 | 0.000000000 | 100.0 % | 1 | – |
| `hollow_amt` | 44 | 0.000000 | 0.000000 | 0.000000000 | 100.0 % | 1 | – |
| `max_amount` | 44 | 0.000000 | 0.000000 | 0.000000000 | 100.0 % | 1 | – |
| `master_vol` | 44 | 0.833333 | 1.000000 | 0.833333313 | 97.7 % | 2 | 5/6 |
| `midi_mode` | 44 | 0.000000 | 0.000000 | 0.000000000 | 100.0 % | 1 | – |
| `root_note` | 44 | 0.283465 | 0.283465 | 0.283464581 | 100.0 % | 1 | 36/127 |
| `gspare1` | 44 | 0.000000 | 0.000000 | 0.000000000 | 100.0 % | 1 | – |
| `gspare2` | 44 | 0.500000 | 0.500000 | 0.500000000 | 100.0 % | 1 | 1/2 |
| `gspare3` | 44 | 0.000000 | 0.500000 | 0.500000000 | 97.7 % | 2 | 1/2 |

## 4. FX-Parameter `fxN_pM`

| Index | IDs | n | min | max | Default(s) | distinct (max) |
|---|---|---|---|---|---|---|
| `pp0` | 8 | 352 | 0.000000 | 1.000000 | 0.000000, 0.500000, 1.000000 | 16 |
| `pp1` | 8 | 352 | 0.000000 | 1.000000 | 0.000000, 0.083300, 0.500000 | 12 |
| `pp2` | 8 | 352 | 0.000000 | 0.887100 | 0.000000, 0.200000, 0.500000 | 12 |
| `pp3` | 8 | 352 | 0.000000 | 1.000000 | 0.000000, 0.477100, 0.500000 | 9 |
| `pp4` | 8 | 352 | 0.000000 | 0.817300 | 0.000000, 0.162200 | 4 |
| `pp5` | 8 | 352 | 0.000000 | 1.000000 | 0.000000, 0.500000 | 4 |
| `pp6` | 8 | 352 | 0.000000 | 1.000000 | 0.000000, 0.709400 | 4 |
| `pp7` | 8 | 352 | 0.000000 | 1.000000 | 0.000000, 0.500000 | 3 |
| `pp8` | 8 | 352 | 0.000000 | 0.000000 | 0.000000 | 1 |
| `pp9` | 8 | 352 | 0.000000 | 0.000000 | 0.000000 | 1 |
| `pp10` | 8 | 352 | 0.000000 | 0.000000 | 0.000000 | 1 |
| `pp11` | 8 | 352 | 0.000000 | 0.000000 | 0.000000 | 1 |
| `pp12` | 8 | 352 | 0.257000 | 1.000000 | 1.000000 | 8 |
| `pp13` | 8 | 352 | 0.196000 | 0.654300 | 0.500000 | 3 |

### 4.1 Muster leerer FX-Slots

Leere Slots (kein Modul an dieser Kettenposition) tragen ein festes Muster:

* `p = [0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 1.000, 0.500]` — 151×

## 5. Modulattribute je `type=`

Datenbasis: 76 Ketten-Presets + 43 Preset-Snapshots.

### `dist` — 185 Vorkommen

Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):

`drive, mode, folds, agc, eqmode, flip`

XML-Reihenfolge == Parameterschema: **nein** (1 Varianten beobachtet)

| # | Attribut | n | min | max | distinct | konstant? |
|---|---|---|---|---|---|---|
| 0 | `drive` | 185 | 0.0000 | 100.0000 | 55 | nein |
| 1 | `mode` | 185 | 1.0000 | 12.0000 | 4 | nein |
| 2 | `folds` | 185 | 2.0000 | 2.4003 | 2 | nein |
| 3 | `agc` | 185 | 1.0000 | 1.0000 | 1 | ja |
| 4 | `eqmode` | 185 | 0.0000 | 0.0000 | 1 | ja |
| 5 | `flip` | 185 | 0.0000 | 0.0000 | 1 | ja |
| – | `mix` (Meta) | 185 | 0.3860 | 1.0000 | 7 | nein |
| – | `outGain` (Meta) | 185 | 0.3920 | 2.0000 | 6 | nein |

Kind-Elemente: `<WNode>` (98×)

| Kind-Attribut | n | min | max | distinct |
|---|---|---|---|---|
| `WNode.seg` | 47 | 1.0000 | 1.0000 | 1 |
| `WNode.ten` | 98 | -0.8473 | 0.9500 | 15 |
| `WNode.x` | 98 | -1.0000 | 1.0000 | 10 |
| `WNode.y` | 98 | -1.0000 | 1.0000 | 9 |

### `eq` — 138 Vorkommen

Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):

`lowgain, lowfreq, midgain, midfreq, midq, highgain, highfreq, middyn`

XML-Reihenfolge == Parameterschema: **nein** (1 Varianten beobachtet)

| # | Attribut | n | min | max | distinct | konstant? |
|---|---|---|---|---|---|---|
| 0 | `lowgain` | 138 | 0.0000 | 0.0000 | 1 | ja |
| 1 | `lowfreq` | 138 | 90.0000 | 90.0000 | 1 | ja |
| 2 | `midgain` | 138 | 0.0000 | 0.0000 | 1 | ja |
| 3 | `midfreq` | 138 | 900.0000 | 900.0000 | 1 | ja |
| 4 | `midq` | 138 | 0.9000 | 0.9000 | 1 | ja |
| 5 | `highgain` | 138 | 0.0000 | 0.0000 | 1 | ja |
| 6 | `highfreq` | 138 | 8000.0000 | 8000.0000 | 1 | ja |
| 7 | `middyn` | 138 | 0.0000 | 0.0000 | 1 | ja |
| – | `mix` (Meta) | 138 | 1.0000 | 1.0000 | 1 | ja |
| – | `outGain` (Meta) | 138 | 0.7264 | 1.2164 | 4 | nein |

Kind-Elemente: `<Band>` (340×)

| Kind-Attribut | n | min | max | distinct |
|---|---|---|---|---|
| `Band.ch` | 340 | 0.0000 | 0.0000 | 1 |
| `Band.dyn` | 340 | 0.0000 | 0.0000 | 1 |
| `Band.f` | 340 | 38.4766 | 14215.9092 | 140 |
| `Band.g` | 340 | -32.0000 | 29.8158 | 139 |
| `Band.kind` | 340 | 0.0000 | 4.0000 | 4 |
| `Band.q` | 340 | 0.3000 | 2.2115 | 25 |

### `final` — 49 Vorkommen

Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):

`boost, freq, clip, upward, monoF, shelfOn, monoOn, clipOn`

XML-Reihenfolge == Parameterschema: **nein** (1 Varianten beobachtet)

| # | Attribut | n | min | max | distinct | konstant? |
|---|---|---|---|---|---|---|
| 0 | `boost` | 49 | 0.3549 | 12.0000 | 15 | nein |
| 1 | `freq` | 49 | 35.7513 | 95.6949 | 8 | nein |
| 2 | `clip` | 49 | 0.0000 | 37.2286 | 9 | nein |
| 3 | `upward` | 49 | 20.0000 | 20.0000 | 1 | ja |
| 4 | `monoF` | 49 | 60.0000 | 181.0499 | 7 | nein |
| 5 | `shelfOn` | 49 | 1.0000 | 1.0000 | 1 | ja |
| 6 | `monoOn` | 49 | 1.0000 | 1.0000 | 1 | ja |
| 7 | `clipOn` | 49 | 0.0000 | 1.0000 | 2 | nein |
| – | `mix` (Meta) | 49 | 1.0000 | 1.0000 | 1 | ja |
| – | `outGain` (Meta) | 49 | 0.8928 | 1.0000 | 2 | nein |

### `mseq` — 94 Vorkommen

Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):

`midlow, midhigh, sidelow, sidehigh, width, middyn, sidedyn, msolo, ssolo, mmute, smute`

XML-Reihenfolge == Parameterschema: **nein** (1 Varianten beobachtet)

| # | Attribut | n | min | max | distinct | konstant? |
|---|---|---|---|---|---|---|
| 0 | `midlow` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 1 | `midhigh` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 2 | `sidelow` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 3 | `sidehigh` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 4 | `width` | 94 | 100.0000 | 100.0000 | 1 | ja |
| 5 | `middyn` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 6 | `sidedyn` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 7 | `msolo` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 8 | `ssolo` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 9 | `mmute` | 94 | 0.0000 | 0.0000 | 1 | ja |
| 10 | `smute` | 94 | 0.0000 | 1.0000 | 2 | nein |
| – | `mix` (Meta) | 94 | 1.0000 | 1.0000 | 1 | ja |
| – | `outGain` (Meta) | 94 | 0.6899 | 1.3085 | 3 | nein |

Kind-Elemente: `<Band>` (289×)

| Kind-Attribut | n | min | max | distinct |
|---|---|---|---|---|
| `Band.ch` | 289 | 0.0000 | 1.0000 | 2 |
| `Band.dyn` | 289 | 0.0000 | 0.0000 | 1 |
| `Band.f` | 289 | 52.0850 | 14744.5195 | 108 |
| `Band.g` | 289 | -21.2670 | 28.0887 | 82 |
| `Band.kind` | 289 | 0.0000 | 4.0000 | 5 |
| `Band.q` | 289 | 0.3000 | 4.2610 | 25 |

### `ott` — 27 Vorkommen

Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):

`depth, time, x1, x2, gl, gm, gh, linear`

XML-Reihenfolge == Parameterschema: **nein** (1 Varianten beobachtet)

| # | Attribut | n | min | max | distinct | konstant? |
|---|---|---|---|---|---|---|
| 0 | `depth` | 27 | 8.5369 | 41.0047 | 8 | nein |
| 1 | `time` | 27 | 100.0000 | 116.7027 | 2 | nein |
| 2 | `x1` | 27 | 120.0000 | 323.2963 | 2 | nein |
| 3 | `x2` | 27 | 2500.0000 | 2500.0000 | 1 | ja |
| 4 | `gl` | 27 | 0.0000 | 7.6150 | 3 | nein |
| 5 | `gm` | 27 | -2.7931 | 7.5580 | 8 | nein |
| 6 | `gh` | 27 | 0.0000 | 0.0000 | 1 | ja |
| 7 | `linear` | 27 | 0.0000 | 0.0000 | 1 | ja |
| – | `mix` (Meta) | 27 | 1.0000 | 1.0000 | 1 | ja |
| – | `outGain` (Meta) | 27 | 1.0000 | 2.0000 | 3 | nein |

### `reverb` — 81 Vorkommen

Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):

`wet, stereo, tone, speed`

XML-Reihenfolge == Parameterschema: **nein** (1 Varianten beobachtet)

| # | Attribut | n | min | max | distinct | konstant? |
|---|---|---|---|---|---|---|
| 0 | `wet` | 81 | 35.0000 | 100.0000 | 9 | nein |
| 1 | `stereo` | 81 | 0.0000 | 100.0000 | 15 | nein |
| 2 | `tone` | 81 | 3.0848 | 100.0000 | 23 | nein |
| 3 | `speed` | 81 | 0.0000 | 11.7782 | 2 | nein |
| – | `mix` (Meta) | 81 | 0.2570 | 0.5200 | 18 | nein |
| – | `outGain` (Meta) | 81 | 1.0000 | 1.0000 | 1 | ja |

Kind-Elemente: `<Pt>` (205×)

| Kind-Attribut | n | min | max | distinct |
|---|---|---|---|---|
| `Pt.t` | 205 | -1.0000 | 1.0000 | 29 |
| `Pt.x` | 205 | 0.0000 | 1.0000 | 16 |
| `Pt.y` | 205 | 0.0000 | 1.0000 | 9 |

### `shaper` — 6 Vorkommen

Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):

`sPre, sPost, sLine, shape, amount`

XML-Reihenfolge == Parameterschema: **nein** (1 Varianten beobachtet)

| # | Attribut | n | min | max | distinct | konstant? |
|---|---|---|---|---|---|---|
| 0 | `sPre` | 6 | 1.0000 | 1.0000 | 1 | ja |
| 1 | `sPost` | 6 | 1.0000 | 1.0000 | 1 | ja |
| 2 | `sLine` | 6 | 1.0000 | 1.0000 | 1 | ja |
| 3 | `shape` | 6 | 0.0000 | 2.0000 | 2 | nein |
| 4 | `amount` | 6 | 0.0000 | 100.0000 | 5 | nein |
| – | `mix` (Meta) | 6 | 1.0000 | 1.0000 | 1 | ja |
| – | `outGain` (Meta) | 6 | 1.0000 | 1.0000 | 1 | ja |

### `trans` — 59 Vorkommen

Parameterschema (Attributreihenfolge, ohne `type`/`on`/`mix`/`outGain`/`ir`):

`amount, attack, duck, speed`

XML-Reihenfolge == Parameterschema: **nein** (1 Varianten beobachtet)

| # | Attribut | n | min | max | distinct | konstant? |
|---|---|---|---|---|---|---|
| 0 | `amount` | 59 | 100.0000 | 100.0000 | 1 | ja |
| 1 | `attack` | 59 | 0.0000 | 100.0000 | 22 | nein |
| 2 | `duck` | 59 | 0.0000 | 63.0169 | 12 | nein |
| 3 | `speed` | 59 | 47.1600 | 179.5281 | 19 | nein |
| – | `mix` (Meta) | 59 | 1.0000 | 1.0000 | 1 | ja |
| – | `outGain` (Meta) | 59 | 0.9459 | 1.2807 | 3 | nein |

## 6. Kreuztabelle: Preset → Kette → `fxN_pM` (Kern von F2)

### 6.1 Kettenformen

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

Modulanzahl je Preset: 0 Module: 1×, 2 Module: 2×, 3 Module: 10×, 4 Module: 12×, 5 Module: 2×, 6 Module: 11×, 7 Module: 5×, 8 Module: 1×

### 6.2 Modultyp je Kettenposition

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

### 6.3 `p12` (MIX) je Preset

| Preset | Kette | p12 (MIX) je Slot |
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

## 7. Abbildung Modulattribut → `fxN_pM`

**Quantisierung.** Die `fxN_pM`-Werte liegen auf einem festen Raster (bester Schritt **0.000100**, max. Abweichung 4.500e-08). Der Fit kann daher nur bis auf eine halbe Stufe genau sein; die Toleranz ist auf 1.2e-04 gesetzt.

**Verfahren.** Fuer jedes Attribut eines Modultyps werden alle Beobachtungen aus den 43 Preset-Snapshots gesammelt (Attributwert in realer Einheit ↔ `fxN_pM` an der Position des Attributs im Parameterschema). Angepasst wird eine Gerade `p = a·x + b` per Kleinstquadrat.

### 7.1 Bestaetigte Zuordnungen

| Modultyp | Attribut | p | n | Stuetzwerte | a | b | Bereich bei p=0…1 | Konfidenz |
|---|---|---|---|---|---|---|---|---|
| `dist` | `drive` | 0 | 59 | 34 | 0.010000261 | -0.000008511 | 0.0000 … 99.9974 | hoch |
| `dist` | `mode` | 1 | 59 | 4 | 0.083336699 | -0.000035664 | 0.0000 … 11.9995 | hoch |
| `dist` | `mix` | 12 | 59 | 4 | 1.000000000 | 3.106e-10 | 0.0000 … 1.0000 | hoch |
| `dist` | `outGain` | 13 | 59 | 3 | 0.500001936 | -0.000001689 | 0.0000 … 2.0000 | mittel |
| `final` | `boost` | 0 | 12 | 7 | 0.083332492 | 0.000034718 | 0.0000 … 12.0001 | hoch |
| `final` | `freq` | 1 | 12 | 4 | 0.005555249 | -0.111112221 | 20.0013 … 200.0112 | hoch |
| `final` | `clip` | 2 | 12 | 4 | 0.009999564 | -0.000010829 | 0.0000 … 100.0044 | hoch |
| `final` | `monoF` | 4 | 12 | 4 | 0.002272633 | -0.136362591 | 60.0020 … 500.0203 | hoch |
| `mseq` | `outGain` | 13 | 34 | 2 | 0.500138329 | -0.000138329 | 0.0000 … 1.9994 | niedrig (2 Stuetzwert) |
| `ott` | `depth` | 0 | 5 | 3 | 0.009998811 | 0.000036326 | 0.0000 … 100.0119 | mittel |
| `ott` | `gl` | 4 | 5 | 3 | 0.041667990 | 0.500002214 | -11.9997 … 11.9996 | mittel |
| `ott` | `gm` | 5 | 5 | 3 | 0.041663777 | 0.500009075 | -12.0011 … 12.0006 | mittel |
| `ott` | `outGain` | 13 | 5 | 2 | 0.499928254 | 0.000071746 | 0.0000 … 2.0003 | niedrig (2 Stuetzwert) |
| `reverb` | `wet` | 0 | 29 | 4 | 0.009999951 | 0.000006065 | 0.0000 … 100.0005 | hoch |
| `reverb` | `stereo` | 1 | 29 | 6 | 0.010000005 | -0.000006472 | 0.0000 … 100.0000 | hoch |
| `reverb` | `tone` | 2 | 29 | 13 | 0.010000413 | -0.000026901 | 0.0000 … 99.9959 | hoch |
| `reverb` | `mix` | 12 | 29 | 12 | 0.999999937 | 2.301e-08 | 0.0000 … 1.0000 | hoch |
| `trans` | `attack` | 1 | 20 | 11 | 0.009999889 | 0.000010225 | 0.0000 … 100.0011 | hoch |
| `trans` | `duck` | 2 | 20 | 8 | 0.010000443 | -0.000004622 | 0.0000 … 99.9956 | hoch |
| `trans` | `speed` | 3 | 20 | 9 | 0.005434795 | -0.086954466 | 15.9996 … 199.9992 | hoch |
| `trans` | `outGain` | 13 | 20 | 2 | 0.500796368 | -0.000796368 | 0.0000 … 1.9968 | niedrig (2 Stuetzwert) |

### 7.2 Umkehrfunktionen (fuer die Implementierung)

| Modultyp | Attribut | Umkehrfunktion |
|---|---|---|
| `dist` | `drive` | `x = (p - -0.000008511) / 0.010000261` |
| `dist` | `mode` | `x = (p - -0.000035664) / 0.083336699` |
| `dist` | `mix` | `x = (p - 3.106e-10) / 1.000000000` |
| `dist` | `outGain` | `x = (p - -0.000001689) / 0.500001936` |
| `final` | `boost` | `x = (p - 0.000034718) / 0.083332492` |
| `final` | `freq` | `x = (p - -0.111112221) / 0.005555249` |
| `final` | `clip` | `x = (p - -0.000010829) / 0.009999564` |
| `final` | `monoF` | `x = (p - -0.136362591) / 0.002272633` |
| `mseq` | `outGain` | `x = (p - -0.000138329) / 0.500138329` |
| `ott` | `depth` | `x = (p - 0.000036326) / 0.009998811` |
| `ott` | `gl` | `x = (p - 0.500002214) / 0.041667990` |
| `ott` | `gm` | `x = (p - 0.500009075) / 0.041663777` |
| `ott` | `outGain` | `x = (p - 0.000071746) / 0.499928254` |
| `reverb` | `wet` | `x = (p - 0.000006065) / 0.009999951` |
| `reverb` | `stereo` | `x = (p - -0.000006472) / 0.010000005` |
| `reverb` | `tone` | `x = (p - -0.000026901) / 0.010000413` |
| `reverb` | `mix` | `x = (p - 2.301e-08) / 0.999999937` |
| `trans` | `attack` | `x = (p - 0.000010225) / 0.009999889` |
| `trans` | `duck` | `x = (p - -0.000004622) / 0.010000443` |
| `trans` | `speed` | `x = (p - -0.086954466) / 0.005434795` |
| `trans` | `outGain` | `x = (p - -0.000796368) / 0.500796368` |

### 7.3 Einzelpunkt-Befunde (Position plausibel, Kurve unbekannt)

Attribute, die in allen Presets denselben Wert haben, koennen keine Kurve belegen. Liegt ihr `p`-Wert aber nicht bei 0, ist die *Position* im Parameterschema zumindest konsistent:

| Modultyp | Attribut | p | realer Wert | p-Wert |
|---|---|---|---|---|
| `dist` | `folds` | 2 | 2.0000 | 0.2000 |
| `dist` | `agc` | 3 | 1.0000 | 1.0000 |
| `eq` | `lowgain` | 0 | 0.0000 | 0.5000 |
| `eq` | `lowfreq` | 1 | 90.0000 | 0.5290 |
| `eq` | `midgain` | 2 | 0.0000 | 0.5000 |
| `eq` | `midfreq` | 3 | 900.0000 | 0.4771 |
| `eq` | `midq` | 4 | 0.9000 | 0.1622 |
| `eq` | `highgain` | 5 | 0.0000 | 0.5000 |
| `eq` | `highfreq` | 6 | 8000.0000 | 0.7094 |
| `eq` | `middyn` | 7 | 0.0000 | 0.5000 |
| `eq` | `mix` | 12 | 1.0000 | 1.0000 |
| `eq` | `outGain` | 13 | 1.0000 | 0.5000 |
| `final` | `upward` | 3 | 20.0000 | 0.2000 |
| `final` | `shelfOn` | 5 | 1.0000 | 1.0000 |
| `final` | `monoOn` | 6 | 1.0000 | 1.0000 |
| `final` | `clipOn` | 7 | 1.0000 | 1.0000 |
| `final` | `mix` | 12 | 1.0000 | 1.0000 |
| `final` | `outGain` | 13 | 1.0000 | 0.5000 |
| `mseq` | `midlow` | 0 | 0.0000 | 0.5000 |
| `mseq` | `midhigh` | 1 | 0.0000 | 0.5000 |
| `mseq` | `sidelow` | 2 | 0.0000 | 0.5000 |
| `mseq` | `sidehigh` | 3 | 0.0000 | 0.5000 |
| `mseq` | `width` | 4 | 100.0000 | 0.5000 |
| `mseq` | `middyn` | 5 | 0.0000 | 0.5000 |
| `mseq` | `sidedyn` | 6 | 0.0000 | 0.5000 |
| `mseq` | `mix` | 12 | 1.0000 | 1.0000 |
| `ott` | `time` | 1 | 100.0000 | 0.4716 |
| `ott` | `x2` | 3 | 2500.0000 | 0.5067 |
| `ott` | `gh` | 6 | 0.0000 | 0.5000 |
| `ott` | `mix` | 12 | 1.0000 | 1.0000 |
| `reverb` | `speed` | 3 | 0.0000 | 0.5000 |
| `reverb` | `outGain` | 13 | 1.0000 | 0.5000 |
| `shaper` | `sPre` | 0 | 1.0000 | 0.2857 |
| `shaper` | `sPost` | 1 | 1.0000 | 0.3125 |
| `shaper` | `mix` | 12 | 1.0000 | 1.0000 |
| `shaper` | `outGain` | 13 | 1.0000 | 0.5000 |
| `trans` | `amount` | 0 | 100.0000 | 1.0000 |
| `trans` | `mix` | 12 | 1.0000 | 1.0000 |

### 7.4 Werksdefault-Vektor je Modultyp

Der haeufigste `fx`-Vektor eines Typs, gegenueber den Modulattributen an dieser Kettenposition. Alle Zeilen muessen unter **einer** Abbildung gleichzeitig erklaerbar sein — das ist die staerkste Bestaetigung der Parameterreihenfolge.

**`dist`** (Vektor in 4 Presets, 31 Varianten)

| p | Attribut | realer Default | p-Wert |
|---|---|---|---|
| 0 | `drive` | 18.8306 | 0.1883 |
| 1 | `mode` | 1.0000 | 0.0833 |
| 2 | `folds` | 2.0000 | 0.2000 |
| 3 | `agc` | 1.0000 | 1.0000 |
| 4 | `eqmode` | 0.0000 | 0.0000 |
| 5 | `flip` | 0.0000 | 0.0000 |
| 12 | `mix` | 1.0000 | 1.0000 |
| 13 | `outGain` | 1.0000 | 0.5000 |

**`eq`** (Vektor in 41 Presets, 1 Varianten)

| p | Attribut | realer Default | p-Wert |
|---|---|---|---|
| 0 | `lowgain` | 0.0000 | 0.5000 |
| 1 | `lowfreq` | 90.0000 | 0.5290 |
| 2 | `midgain` | 0.0000 | 0.5000 |
| 3 | `midfreq` | 900.0000 | 0.4771 |
| 4 | `midq` | 0.9000 | 0.1622 |
| 5 | `highgain` | 0.0000 | 0.5000 |
| 6 | `highfreq` | 8000.0000 | 0.7094 |
| 7 | `middyn` | 0.0000 | 0.5000 |
| 12 | `mix` | 1.0000 | 1.0000 |
| 13 | `outGain` | 1.0000 | 0.5000 |

**`final`** (Vektor in 3 Presets, 7 Varianten)

| p | Attribut | realer Default | p-Wert |
|---|---|---|---|
| 0 | `boost` | 7.7166 | 0.6431 |
| 1 | `freq` | 95.6949 | 0.4205 |
| 2 | `clip` | 0.0000 | 0.0000 |
| 3 | `upward` | 20.0000 | 0.2000 |
| 4 | `monoF` | 60.0000 | 0.0000 |
| 5 | `shelfOn` | 1.0000 | 1.0000 |
| 6 | `monoOn` | 1.0000 | 1.0000 |
| 7 | `clipOn` | 1.0000 | 1.0000 |
| 12 | `mix` | 1.0000 | 1.0000 |
| 13 | `outGain` | 1.0000 | 0.5000 |

**`mseq`** (Vektor in 33 Presets, 2 Varianten)

| p | Attribut | realer Default | p-Wert |
|---|---|---|---|
| 0 | `midlow` | 0.0000 | 0.5000 |
| 1 | `midhigh` | 0.0000 | 0.5000 |
| 2 | `sidelow` | 0.0000 | 0.5000 |
| 3 | `sidehigh` | 0.0000 | 0.5000 |
| 4 | `width` | 100.0000 | 0.5000 |
| 5 | `middyn` | 0.0000 | 0.5000 |
| 6 | `sidedyn` | 0.0000 | 0.5000 |
| 7 | `msolo` | 0.0000 | 0.0000 |
| 8 | `ssolo` | 0.0000 | 0.0000 |
| 9 | `mmute` | 0.0000 | 0.0000 |
| 10 | `smute` | 0.0000 | 0.0000 |
| 12 | `mix` | 1.0000 | 1.0000 |
| 13 | `outGain` | 1.0000 | 0.5000 |

**`ott`** (Vektor in 3 Presets, 3 Varianten)

| p | Attribut | realer Default | p-Wert |
|---|---|---|---|
| 0 | `depth` | 8.5369 | 0.0854 |
| 1 | `time` | 100.0000 | 0.4716 |
| 2 | `x1` | 120.0000 | 0.4714 |
| 3 | `x2` | 2500.0000 | 0.5067 |
| 4 | `gl` | 0.0000 | 0.5000 |
| 5 | `gm` | 0.0000 | 0.5000 |
| 6 | `gh` | 0.0000 | 0.5000 |
| 7 | `linear` | 0.0000 | 0.0000 |
| 12 | `mix` | 1.0000 | 1.0000 |
| 13 | `outGain` | 1.0000 | 0.5000 |

**`reverb`** (Vektor in 3 Presets, 16 Varianten)

| p | Attribut | realer Default | p-Wert |
|---|---|---|---|
| 0 | `wet` | 100.0000 | 1.0000 |
| 1 | `stereo` | 100.0000 | 1.0000 |
| 2 | `tone` | 74.2373 | 0.7424 |
| 3 | `speed` | 0.0000 | 0.5000 |
| 12 | `mix` | 0.5000 | 0.5000 |
| 13 | `outGain` | 1.0000 | 0.5000 |

**`shaper`** (Vektor in 1 Presets, 1 Varianten)

| p | Attribut | realer Default | p-Wert |
|---|---|---|---|
| 0 | `sPre` | 1.0000 | 0.2857 |
| 1 | `sPost` | 1.0000 | 0.3125 |
| 2 | `sLine` | 1.0000 | 0.0000 |
| 3 | `shape` | 2.0000 | 0.0000 |
| 4 | `amount` | 31.2542 | 0.0000 |
| 12 | `mix` | 1.0000 | 1.0000 |
| 13 | `outGain` | 1.0000 | 0.5000 |

**`trans`** (Vektor in 3 Presets, 11 Varianten)

| p | Attribut | realer Default | p-Wert |
|---|---|---|---|
| 0 | `amount` | 100.0000 | 1.0000 |
| 1 | `attack` | 0.0000 | 0.0000 |
| 2 | `duck` | 36.9296 | 0.3693 |
| 3 | `speed` | 47.6499 | 0.1720 |
| 12 | `mix` | 1.0000 | 1.0000 |
| 13 | `outGain` | 1.0000 | 0.5000 |

### 7.5 Nicht bestimmbare Zuordnungen

| Modultyp | Attribut | p | n | real distinct | Status |
|---|---|---|---|---|---|
| `dist` | `folds` | 2 | 59 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `dist` | `agc` | 3 | 59 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `dist` | `eqmode` | 4 | 59 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `dist` | `flip` | 5 | 59 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `lowgain` | 0 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `lowfreq` | 1 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `midgain` | 2 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `midfreq` | 3 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `midq` | 4 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `highgain` | 5 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `highfreq` | 6 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `middyn` | 7 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `mix` | 12 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `eq` | `outGain` | 13 | 41 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `final` | `upward` | 3 | 12 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `final` | `shelfOn` | 5 | 12 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `final` | `monoOn` | 6 | 12 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `final` | `clipOn` | 7 | 12 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `final` | `mix` | 12 | 12 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `final` | `outGain` | 13 | 12 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `midlow` | 0 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `midhigh` | 1 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `sidelow` | 2 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `sidehigh` | 3 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `width` | 4 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `middyn` | 5 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `sidedyn` | 6 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `msolo` | 7 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `ssolo` | 8 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `mmute` | 9 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `smute` | 10 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `mseq` | `mix` | 12 | 34 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `ott` | `time` | 1 | 5 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `ott` | `x2` | 3 | 5 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `ott` | `gh` | 6 | 5 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `ott` | `linear` | 7 | 5 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `ott` | `mix` | 12 | 5 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `reverb` | `speed` | 3 | 29 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `reverb` | `outGain` | 13 | 29 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `shaper` | `sPre` | 0 | 1 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `shaper` | `sPost` | 1 | 1 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `shaper` | `sLine` | 2 | 1 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `shaper` | `shape` | 3 | 1 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `shaper` | `amount` | 4 | 1 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `shaper` | `mix` | 12 | 1 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `shaper` | `outGain` | 13 | 1 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `trans` | `amount` | 0 | 20 | 1 | Attribut konstant -> Kurve nicht bestimmbar |
| `trans` | `mix` | 12 | 20 | 1 | Attribut konstant -> Kurve nicht bestimmbar |

### 7.7 Rueckrechnung (Roundtrip)

| Modultyp | Attribute verifiziert | max. Rekonstruktionsfehler |
|---|---|---|
| `dist` | 4 / 4 | 5.36e-03 |
| `eq` | 0 / 0 | 0.00e+00 |
| `final` | 4 / 4 | 6.38e-03 |
| `mseq` | 1 / 1 | 2.22e-16 |
| `ott` | 4 / 4 | 1.98e-03 |
| `reverb` | 4 / 4 | 4.67e-03 |
| `shaper` | 0 / 0 | 0.00e+00 |
| `trans` | 4 / 4 | 6.77e-03 |

### 7.8 Gegenprobe: unabhaengige Suche nach dem richtigen p-Index

Fuer jedes Attribut wird ohne Annahme ueber die Reihenfolge geprueft, welcher `p`-Index es unter `p = x/k` (k ∈ {1, 2, 10, 12, 100}) oder `p = (x+A)/2A` erklaert. Stimmt das Ergebnis mit dem Positionsbefund ueberein, ist die Reihenfolge **doppelt** belegt; weicht es ab, ist sie fuer diesen Typ fraglich.

| Modultyp | Attribut | n | Stuetzwerte | Positionsindex | gefunden | ueberein? |
|---|---|---|---|---|---|---|
| `dist` | `agc` | 59 | 1 | 3 | p3: `x/1` | ja |
| `dist` | `drive` | 59 | 34 | 0 | p0: `x/100` | ja |
| `dist` | `folds` | 59 | 1 | 2 | p2: `x/10`, p3: `x/2` | ja |
| `dist` | `mode` | 59 | 4 | 1 | p1: `x/12` | ja |
| `final` | `boost` | 12 | 7 | 0 | p0: `x/12` | ja |
| `final` | `clip` | 12 | 4 | 2 | p2: `x/100` | ja |
| `final` | `clipOn` | 12 | 1 | 7 | p5: `x/1`, p6: `x/1`, p7: `x/1` | ja |
| `final` | `monoOn` | 12 | 1 | 6 | p5: `x/1`, p6: `x/1`, p7: `x/1` | ja |
| `final` | `shelfOn` | 12 | 1 | 5 | p5: `x/1`, p6: `x/1`, p7: `x/1` | ja |
| `final` | `upward` | 12 | 1 | 3 | p3: `x/100` | ja |
| `ott` | `depth` | 5 | 3 | 0 | p0: `x/100` | ja |
| `ott` | `gl` | 5 | 3 | 4 | p4: `(x+12)/24` | ja |
| `ott` | `gm` | 5 | 3 | 5 | p5: `(x+12)/24` | ja |
| `reverb` | `stereo` | 29 | 6 | 1 | p1: `x/100` | ja |
| `reverb` | `tone` | 29 | 13 | 2 | p2: `x/100` | ja |
| `reverb` | `wet` | 29 | 4 | 0 | p0: `x/100` | ja |
| `shaper` | `amount` | 1 | 1 | 4 | p1: `x/100` | **nein** |
| `trans` | `amount` | 20 | 1 | 0 | p0: `x/100` | ja |
| `trans` | `attack` | 20 | 11 | 1 | p1: `x/100` | ja |
| `trans` | `duck` | 20 | 8 | 2 | p2: `x/100` | ja |

### 7.9 Parameterschema je Modultyp (XML-Reihenfolge)

* `dist`: `p0…p5` = `drive`, `mode`, `folds`, `agc`, `eqmode`, `flip` → `p12` = MIX, `p13` = OUTGAIN
* `eq`: `p0…p7` = `lowgain`, `lowfreq`, `midgain`, `midfreq`, `midq`, `highgain`, `highfreq`, `middyn` → `p12` = MIX, `p13` = OUTGAIN
* `final`: `p0…p7` = `boost`, `freq`, `clip`, `upward`, `monoF`, `shelfOn`, `monoOn`, `clipOn` → `p12` = MIX, `p13` = OUTGAIN
* `mseq`: `p0…p10` = `midlow`, `midhigh`, `sidelow`, `sidehigh`, `width`, `middyn`, `sidedyn`, `msolo`, `ssolo`, `mmute`, `smute` → `p12` = MIX, `p13` = OUTGAIN
* `ott`: `p0…p7` = `depth`, `time`, `x1`, `x2`, `gl`, `gm`, `gh`, `linear` → `p12` = MIX, `p13` = OUTGAIN
* `reverb`: `p0…p3` = `wet`, `stereo`, `tone`, `speed` → `p12` = MIX, `p13` = OUTGAIN
* `shaper`: `p0…p4` = `sPre`, `sPost`, `sLine`, `shape`, `amount` → `p12` = MIX, `p13` = OUTGAIN
* `trans`: `p0…p3` = `amount`, `attack`, `duck`, `speed` → `p12` = MIX, `p13` = OUTGAIN

## 8. Normalisierungskurven (F1)

### 8.1 `stretch`

Default (Werkswert): `0.374989420` in 44 Slots.

| Kurve | Faktor beim Default | v fuer Faktor 1.0 | Abweichung zum Default |
|---|---|---|---|
| `log_0.25_8` | 0.916970 | 0.400000 | 0.025011 |
| `lin_0.25_8` | 3.156168 | 0.096774 | -0.278215 |
| `log_0.25_4` | 0.707086 | 0.500000 | 0.125011 |
| `lin_0.25_4` | 1.656210 | 0.200000 | -0.174989 |
| `log_0.5_2` | 0.840884 | 0.500000 | 0.125011 |
| `lin_0.5_2` | 1.062484 | 0.333333 | -0.041656 |
| `log_0.125_8` | 0.594577 | 0.500000 | 0.125011 |
| `lin_0.125_8` | 3.078042 | 0.111111 | -0.263878 |
| `log_0.25_10` | 0.997001 | 0.375804 | 0.000814 |
| `lin_0.25_10` | 3.906147 | 0.076923 | -0.298066 |
| `log_0.25_16` | 1.189155 | 0.333333 | -0.041656 |
| `lin_0.25_16` | 6.156083 | 0.047619 | -0.327370 |
| `log_0.5_8` | 1.414172 | 0.250000 | -0.124989 |
| `lin_0.5_8` | 3.312421 | 0.066667 | -0.308323 |
| `log_0.25_6` | 0.823199 | 0.436209 | 0.061219 |
| `lin_0.25_6` | 2.406189 | 0.130435 | -0.244555 |

Potenz-Kurve `v = ((f-0.25)/7.75)^s` mit Faktor 1.0 beim Default: **s = 0.420000** (JUCE-Skew äquivalent 2.380952).

### 8.2 `pitch`

Default: `0.500000060` — Abstand zu 0.5 beträgt **1.0 float32-ULP**.

| Reichweite | Semitöne der beobachteten Werte | max. Abweichung vom 1-HT-Raster |
|---|---|---|
| pm3 | -0.132, 0.000, 0.028, 0.062, 0.250, 0.263 | 0.2634 |
| pm6 | -0.265, 0.000, 0.056, 0.123, 0.500, 0.527 | 0.5000 |
| pm12 | -0.529, 0.000, 0.112, 0.247, 1.000, 1.054 | 0.4710 |
| pm24 | -1.058, 0.000, 0.224, 0.494, 2.000, 2.107 | 0.4940 |
| pm36 | -1.587, 0.000, 0.336, 0.741, 3.000, 3.160 | 0.4130 |

**Keine** der geprueften Reichweiten bildet alle beobachteten Werte auf ganze Halbtoene ab (kleinste Restabweichung 0.2634 HT bei `pm3`). Entweder ist die Skala nicht halbtonlinear, oder die Presets enthalten bewusst krumme Pitch-Werte.

### 8.3 `root_note`

Default: `0.283464581`, distinct = 1.

| Skala | Wert beim Default | naechste Ganzzahl | Abweichung |
|---|---|---|---|
| `*127` | 36.000002 | 36 | 0.000001787 |
| `*128` | 36.283466 | 36 | 0.283466368 |
| `*12` | 3.401575 | 3 | 0.401574972 |
| `*24` | 6.803150 | 7 | -0.196850056 |
| `*36` | 10.204725 | 10 | 0.204724916 |

Bruch-Kandidat: **36/127** (Fehler 1.41e-08).

### 8.4 `filterq`

Default: `0.102564096` (konstant in allen Slots).

Bruch-Kandidat: 4/39

| Q-Bereich @ Default-Q | vorhergesagt | Abweichung |
|---|---|---|
| `[0.1,4]@Q=0.5` | 0.102564103 | 7.000e-09 |

### 8.5 `vol` und `master_vol`

**`lN_vol`** — Default `0.699840486`

* `20·log10(Default)` = -3.1000 dB
* Lineare Verstaerkung mit 1.0 beim Default ⇒ Maximalwert 1.428897 (3.1000 dB)

| dB-Bereich | Default in dB | Glattheit | bestes Raster | Rasterfehler |
|---|---|---|---|---|
| [0, 10] | 6.998 | 0.0016 | 1.0 dB | 0.046 |
| [-25, 5] | -4.005 | 0.0048 | 1.0 dB | 0.137 |
| [-24, 6] | -3.005 | 0.0048 | 1.0 dB | 0.137 |
| [-30, 10] | -2.006 | 0.0064 | 2.0 dB | 0.091 |
| [-40, 10] | -5.008 | 0.0080 | 0.25 dB | 0.089 |
| [-50, 10] | -8.010 | 0.0096 | 0.25 dB | 0.093 |

**`master_vol`** — Default `0.833333313` (97.7 % aller Presets), distinct = 2

* Bruch-Kandidat: **5/6**
* dB-Skala mit 0 dB beim Default ⇒ Bereich `[lo, lo + |lo|/0.8333]` z. B. für lo = -60, -50, -48, -40, -36, -30, -25, -24, -20 dB:

| lo (dB) | hi (dB) |
|---|---|
| -60 | 12.0000 |
| -50 | 10.0000 |
| -48 | 9.6000 |
| -40 | 8.0000 |
| -36 | 7.2000 |
| -30 | 6.0000 |
| -25 | 5.0000 |
| -24 | 4.8000 |
| -20 | 4.0000 |

* Lineare Verstaerkung mit 1.0 beim Default ⇒ `gain = v / 0.833333313`, Maximalwert 1.200000 (1.58 dB).

> **Wichtig:** 5/6 ist ein einfacher Bruch. Bei der Annahme "Default = 0 dB" gilt `lo = −5·hi`, d. h. **jedes** ganzzahlige `hi` erfuellt die Messung exakt. Die Tabelle unten belegt daher nur, dass eine dB-Deutung mit 0 dB beim Default *moeglich* ist — sie unterscheidet die Kandidaten **nicht**.

| dB-Bereich | Default in dB | Abstand zur Ganzzahl |
|---|---|---|
| [-60, 6] | -5.000 | 0.0000 |
| [-60, 12] | -0.000 | 0.0000 |
| [-50, 10] | -0.000 | 0.0000 |
| [-48, 6] | -3.000 | 0.0000 |
| [-36, 6] | -1.000 | 0.0000 |
| [-30, 6] | -0.000 | 0.0000 |

---

## 9. Rohdaten

Vollstaendige Auswertung inkl. aller Einzelwerte: [`_private/mining/report.json`](../../_private/mining/report.json) (nicht Teil des oeffentlichen Repos).
