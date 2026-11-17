# FX-Modul `ott` — Multiband (Xfer-OTT-Integration)

**Ticket:** B-140 (Phase 2, Integration externer FX-Plugin-Funktionen)
**Code:** `src/fx/modules/MultibandModule.h/.cpp`, Bausteine in `src/fx/dsp/Biquad.h`
**Tests:** `tests/MultibandModuleTests.cpp` (9 Testfälle) + Kettentest in `tests/FxChainTests.cpp`
**Stand:** 2026-11-17

---

## 1. Zweck

Der Rack-Platz `ott` (Referenz-Typ `"ott"`, Anzeigename **Multiband**) ist die
funktionale Integration des **Xfer Records OTT 1.37** („Multiband Up/Down
Compressor") in den Kick Architect. Die Attribute, die das Referenzformat für
`ott` bereits kennt (`depth`, `time`, `x1`, `x2`, `gl`, `gm`, `gh`, `linear`),
bleiben in derselben Reihenfolge an den Bridge-Positionen `p0…p7`; die
OTT-spezifischen Funktionen sind zusätzliche Attribute danach.

**Quellen (nur lesend analysiert, nichts davon im Repo):**

* Installer `Install_Xfer_OTT_137.exe` (NSIS 3.06.1), entpackt nach `C:\bfka-re\ott`.
* Parameterinventar aus den Bezeichner-Strings der VST3-Binary
  `C:\bfka-re\ott\C_\Program Files\Common Files\VST3\OTT.vst3`
  (`In Gain`, `Depth`, `Time`, `Out Gain`, `Clean XOV`, `Thresh L/M/H`,
  `Gain L/M/H`, `Byp Upwd L/M/H`, `Byp Dnwd L/M/H`, `Upwd Strgth`, `Dnwd Strgth`).
* Bediensemantik (Depth = Nass-/Trocken-Blende, Time = gegenläufig
  Attack/Release) aus der öffentlichen Produktdokumentation/Beschreibung.
* Eine Standalone-Version gibt es für OTT nicht.

---

## 2. Parameter

Reihenfolge = `params()` = Attribute in XML-Reihenfolge. `p` = Bridge-Position
(`p0…p11` = Modulattribute, `p12` = MIX, `p13` = OUTGAIN/2).

| # | `id` | Anzeige | Bereich | Default | Einheit | p |
|---|---|---|---|---|---|---|
| 0 | `depth` | Depth | 0…100 | 50 | % | p0 |
| 1 | `time` | Time | 0…100 | 50 | % | p1 |
| 2 | `x1` | Low/Mid Crossover | 20…1000 | 120 | Hz | p2 |
| 3 | `x2` | Mid/High Crossover | 500…12000 | 2500 | Hz | p3 |
| 4 | `gl` | Low Gain | −12…+12 | 0 | dB | p4 |
| 5 | `gm` | Mid Gain | −12…+12 | 0 | dB | p5 |
| 6 | `gh` | High Gain | −12…+12 | 0 | dB | p6 |
| 7 | `linear` | Linear Phase | 0/1 | 0 | – | p7 |
| 8 | `inGain` | Input Gain | −24…+24 | 0 | dB | p8 |
| 9 | `cleanXov` | Clean Crossover | 0/1 | 0 | – | p9 |
| 10 | `thL` | Threshold Low | −60…0 | −30 | dB | p10 |
| 11 | `thM` | Threshold Mid | −60…0 | −30 | dB | p11 |
| 12 | `thH` | Threshold High | −60…0 | −30 | dB | – |
| 13–15 | `buL`, `buM`, `buH` | Bypass Upward Low/Mid/High | 0/1 | 0 | – | – |
| 16–18 | `bdL`, `bdM`, `bdH` | Bypass Downward Low/Mid/High | 0/1 | 0 | – | – |
| 19 | `upwdStr` | Upward Strength | 0…100 | 50 | % | – |
| 20 | `dnwdStr` | Downward Strength | 0…100 | 50 | % | – |

**OUT GAIN der OTT-Karte ist der `outGain` der Kettenkarte** (wie im
Referenzformat); das Modul liefert ausschließlich 100 % nasses Signal.

Werte jenseits `p11` sind **zustandsgebunden** (Snapshot/`FxModuleState::data`),
nicht über die 14 Host-Proxy-Parameter automatisierbar. Das ist konsistent mit
der Regel „Bänder/Kurven stehen nur im Snapshot" (fx-param-bridge.md §7).

---

## 3. Signalfluss

```
In Gain
  └─ 3-Band-Split (Low | Mid | High)
       ├─ Band-Input-Gain (gl/gm/gh)
       ├─ Downward-Kompressor (3:1, Schwelle thL/M/H)
       └─ Upward-Kompressor (2:1 aufwärts, Schwelle thL/M/H)
  └─ Summe der Bänder → Ketten-MIX / OUT GAIN
```

### 3.1 Crossover

* **Minimum-Phase (Default):** Linkwitz-Riley 4. Ordnung, je Flanke zwei
  kaskadierte Butterworth-2-Stufen (`Q = 0.7071`).
  `cleanXov = 1` schaltet auf **LR 8. Ordnung** (vier Stufen) für eine
  steilere Trennung.
* **`linear = 1`:** linearphasiger FIR-Split, fensterter Sinc mit Hann-Fenster.
  Die Fensterlänge beträgt ≈ 10,7 ms, also **513 Taps bei 48 kHz** (511…2049
  Taps, mit der Samplerate skaliert), DC-normiert. Die Bänder rekonstruieren
  das Eingangssignal **exakt** (`Low + Mid + High == Eingang`, um die
  Gruppenlaufzeit verzögert).

### 3.2 Dynamik

Je Band ein peak-basierter, stereo-gekoppelter Hüllkurvenfolger:

* **Downward:** `(Pegel − Schwelle) · 2/3` dB Absenkung, maximal −48 dB.
* **Upward:** `(Schwelle − Pegel) · 1/2` dB Anhebung, maximal +24 dB.
* **Depth** blendet beide Regelwege (0 % = neutral, 100 % = voll),
  **upwdStr**/**dnwdStr** skalieren Aufwärts-/Abwärtsanteil,
  **bu*/bd*** schalten die Richtung je Band ab.
* **Time** steuert Attack (0,05…20 ms) und Release (500…20 ms) **gegenläufig**
  (wie beim Original: Time klein = schnell/langer Release, Time groß =
  langsam/kurzer Release).

### 3.3 Latenz

| Modus | Latenz |
|---|---|
| Minimum-Phase | 0 Samples |
| Linearphase (513 Taps bei 48 kHz) | **(Taps−1)/2 = 256 Samples** (bei 48 kHz) |

Die Latenz wird über `latencySamples()` gemeldet; ein Umschalten von
`linear` ändert sie. Der Host/PDC-Ausgleich muss nach einem erneuten
`prepare()`/`setChain()` neu gelesen werden.

### 3.4 Bedien-/Diagnose-API

* `bandGainDb (int band)` liefert den mittleren Regelweg des letzten Blocks in
  dB (negativ = Absenkung) für die spätere Kartenanzeige.
* `lowCrossoverHz()` / `highCrossoverHz()` liefern die geglätteten
  Crossover-Frequenzen.

---

## 4. Bewusste Abweichungen

| # | Original/Referenz | Nachbau | Grund |
|---|---|---|---|
| M1 | `x1`-Bereich aus Referenzdaten nur 39…423 Hz ableitbar | **20…1000 Hz** | größerer Bereich klemmt keine Referenzwerte, macht den Regler aber musikalisch nutzbar (Bridge-Normierung weicht daher von der Referenzformel ab) |
| M2 | OTT hat kein `linear` (Referenzattribut) | beibehalten, FIR-Split | Preset-Kompatibilität des Referenzformats |
| M3 | „Out Gain" ist ein OTT-Regler | Ketten-OUT-GAIN | so definiert das Referenzformat den Modulausgang |
| M4 | „Clean XOV" ist beim Original ein einzelner Schalter | als LR4 → LR8 umgesetzt | keine belegte Klangreferenz; steilere Trennung ist die naheliegende Bedeutung |
| M5 | OTT zeigt Drag-Balken je Band | Parameter + Diagnose-API, UI folgt in P9 | UI ist noch nicht gebaut |
| M6 | bis zu 19 Host-Parameter | 12 über die Bridge, Rest im Zustand | Formatgrenze 8×14 Proxy-Parameter |

---

## 5. Getestete Eigenschaften

| Testfall | Prüft |
|---|---|
| Parameterliste | 21 IDs in Referenz-/OTT-Reihenfolge, Bereiche, Defaults, p0/p7-Zuordnung |
| Klemmen | Wertebereiche, ungültige Indizes |
| Linearphase-Rekonstruktion | Ausgabe == Eingang (um 256 Samples verzögert), Fehler < 5·10⁻⁴ |
| Linearphase-Impuls | Peak exakt bei 256, Amplitude 1 |
| Downward | 100-Hz-Sinuston wird um > 3 dB abgesenkt, Band-Regelweg < −2 dB |
| Upward | leiser Ton wird um > 3 dB angehoben |
| Richtungs-Bypass | kein Regelweg im Tiefband bei `buL = bdL = 1` |
| Band-Gain | −12 dB im Tiefband wirkt (60 Hz, x1 = 200 Hz) |
| Input-Gain | −6 dB exakt |
| Kette | OTT → Convolver → Dynamic EQ läuft stabil, latenzfrei |
