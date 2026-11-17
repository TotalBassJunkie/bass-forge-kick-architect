# FX-Modul `reverb` — Convolver (Convology-XT-Integration)

**Ticket:** B-141 (Phase 2, Integration externer FX-Plugin-Funktionen)
**Code:** `src/fx/modules/ConvolverModule.h/.cpp`
**Tests:** `tests/ConvolverModuleTests.cpp` (10 Testfälle) + Kettentest in `tests/FxChainTests.cpp`
**Stand:** 2026-11-17

---

## 1. Zweck

Der Rack-Platz `reverb` (Referenz-Typ `"reverb"`, Anzeigename **Convolver**) ist
die funktionale Integration von **Convology XT 1.34** (Impulse Record / Wave
Arts) in den Kick Architect. Die Referenz-Attribute `wet`, `stereo`, `tone`,
`speed` (und der IR-Pfad `ir`) bleiben an ihren Positionen; alle
Convology-Funktionen sind zusätzliche Attribute bzw. Zustands-Kinder.

**Quellen (nur lesend analysiert, nichts davon im Repo):**

* Installer `ConvologyXT-134.exe` (NSIS 3.05), entpackt nach `C:\bfka-re\convology`
  (VST3, AAX, VST2, Factory Library, **Handbuch-PDF**).
* Handbuch `Convology XT Manual.pdf` (v1.34, 5. Aug. 2025): Signalfluss,
  Time/Freq/IR-Settings/Modulation/Scale/Master, True-Stereo.
* Bezeichner der VST3-Binary (`ConvologyXT_64.vst3`) als Gegenprobe.
* Eine Standalone-Version gibt es für Convology XT nicht.

---

## 2. Parameter

| # | `id` | Anzeige | Bereich | Default | Einheit | p |
|---|---|---|---|---|---|---|
| 0 | `wet` | Wet | 0…100 | 100 | % | p0 |
| 1 | `stereo` | Width | 0…200 | 100 | % | p1 |
| 2 | `tone` | Tone | 0…100 | 50 | % | p2 |
| 3 | `speed` | Stretch | −12…+12 | 0 | Halbton | p3 |
| 4 | `predelay` | Predelay | 0…500 | 0 | ms | p4 |
| 5 | `decay` | Decay | 0,1…2,0 | 1,0 | × | p5 |
| 6 | `modDepth` | Mod Depth | 0…100 | 0 | % | p6 |
| 7 | `modRate` | Mod Rate | 0,05…10 | 0,5 | Hz | p7 |
| 8 | `mod90` | Mod 90 | 0/1 | 0 | – | p8 |
| 9 | `reverse` | Reverse | 0/1 | 0 | – | p9 |
| 10 | `monoIn` | Mono Input | 0/1 | 0 | – | p10 |
| 11 | `normalize` | Normalize | 0/1 | 1 | – | p11 |
| 12 | `irGain` | IR Gain | −24…+24 | 0 | dB | – |
| 13 | `trueStereo` | True Stereo | 0/1 | 1 | – | – |
| 14 | `zeroLat` | Zero Latency | 0/1 | 1 | – | – |
| 15 | `envStart` | Start | 0…1000 | 0 | ms | – |
| 16 | `envAttack` | Attack | 0…1000 | 0 | ms | – |
| 17 | `envHold` | Hold | 0…2000 | 0 | ms | – |
| 18 | `envRelease` | Release | 0…5000 | 0 | ms | – |

**Zustands-Kinder** (nur im Modulzustand, nicht in den Proxy-Parametern):

```xml
<Module type="reverb" ir="C:\IRs\Plate.wav" …>
  <Eq>  <Pt y=" 0.0"/> … 11 Punkte (dB, −24…+24) </Eq>
  <Dc>  <Pt y=" 1.0"/> … 11 Punkte (Faktor 0,1…2) </Dc>
</Module>
```

Die 11 Punkte liegen logarithmisch zwischen 20 Hz und 20 kHz. Der IR-Pfad ist
ein String (`ir`) und damit – wie im Original – nicht automatisierbar.

**MIX und OUT GAIN sind der Ketten-MIX und der Ketten-OUT-GAIN** (die Karte
bildet damit Convology „Mix" und „Output" ab); `wet` ist der IR-Pegel.

---

## 3. Signalfluss

```
IR-Datei (WAV/AIF, 1/2/4 Kanäle)
  └─ Aufbereitung (Message-Thread):
       Reverse → Stretch (Resampling) → Decay-Skalierung → Decay-Kurve
       → Time-Hüllkurve (Start/Attack/Hold/Release) → Normalize
  └─ Faltung (Audio-Thread): partitionierte FFT-Faltung (Zero-Latency-Kopf)
  └─ Predelay → Modulation → Tone (Tilt) + EQ-Kurve → Width → Wet-Pegel
```

### 3.1 Faltung

* Ohne geladene IR ist das Modul **neutral**: der Eingang wird unverändert
  durchgereicht (kein Pegelverlust, keine Faltung).
* `juce::dsp::Convolution` mit **Zero-Latency-Kopf** (non-uniform, Kopflänge
  1024). `zeroLat = 0` verwendet die feste Latenz 2048 Samples (CPU-sparsamer).
* **Stereo:** zwei IR-Kanäle (L→L, R→R).
* **Mono:** ein IR-Kanal, auf beide Seiten kopiert.
* **True Stereo (4 Kanäle, L→L, L→R, R→L, R→R):** zwei parallele Stereo-
  Faltungen. Kanal A bekommt `(L, L)` mit IR `(L→L, L→R)`, Kanal B bekommt
  `(R, R)` mit IR `(R→L, R→R)`; die Ausgänge werden summiert.
* IR-Aufbereitung (Resampling, Hüllkurven, Kurven, Normalisierung) läuft auf
  dem Message-Thread. **Beim `prepare()`** wird das Kernel synchron in die
  Engine übernommen (deterministisch, kein Crossfade-Artefakt beim Start).
  **Bei Live-Änderungen** (Regler für Speed/Decay/Reverse/… während der
  Wiedergabe) wird das fertige Kernel über einen atomaren Zeiger an den
  Audio-Thread übergeben und dort über `loadImpulseResponse()` (laut JUCE
  wait-free) eingespielt; JUCE blendet die alte und neue Engine selbst über.
  Der Audio-Thread ruft dabei **kein** `prepare()` auf und allokiert nicht.

### 3.2 IR-Aufbereitung (Message-Thread)

| Funktion | Umsetzung |
|---|---|
| **Stretch** (`speed`) | Resampling der IR um `2^(speed/12)`; ändert Länge **und** Tonhöhe (wie im Original) |
| **Decay** (`decay`) | exponentielle Hüllkurve von 1,0 auf `decay` über die IR-Länge |
| **Decay-Kurve** `<Dc>` | 3-Band-Split (LR4 bei 400 Hz / 4 kHz), je Band eigene exponentielle Dämpfung aus der Kurve (Tiefen 100 Hz, Mitten 1,2 kHz, Höhen 8 kHz) |
| **Time-Hüllkurve** | Start-Crop, Attack-Rampe 0→1, Hold, Release 1→0; Defaults (0/0/0/0) verändern die IR **nicht** |
| **Reverse** | Zeitumkehr je Kanal |
| **Normalize** | Skalierung auf Gesamtenergie 1 |
| **EQ-Kurve** `<Eq>` | 11 Peaking-Biquads (Q = 1,2) auf dem Nasssignal |

### 3.3 Master-Sektion

* **Width:** M/S-Shuffler, `0 %` = Mono, `100 %` = unverändert, `200 %` =
  Außenbasis (out-of-phase).
* **Tone:** Tilt, `50 %` = neutral; Low-Shelf 250 Hz und High-Shelf 3,5 kHz
  mit gegensätzlichem Vorzeichen (max. ±12 dB).
* **Modulation:** Stereo-Modulator mit Depth (0…5 ms) und Rate
  (0,05…10 Hz); `mod90 = 1` schaltet die Kanäle in Quadratur (sin/cos).
* **Predelay:** Ringpuffer 0…500 ms.

### 3.4 Latenz

`latencySamples()` meldet die Latenz der gewählten Engine: 0 Samples im
Zero-Latency-Kopf, 2048 Samples bei `zeroLat = 0`. Die Betriebsart wird beim
nächsten `prepare()` übernommen.

---

## 4. Zustand / Serialisierung

`getState()` liefert `ir` (String) sowie `<Eq>`- und `<Dc>`-Kinder mit je 11
`<Pt y="…">`. `setState()` liest denselben Aufbau; ein geänderter `ir`-Pfad
lädt die Datei (fehlende Datei → Warnung im Log, alter Zustand bleibt).
Statische Bearbeitung der Kurven über `setEqCurve()`/`setDecayCurve()`.

---

## 5. Bewusste Abweichungen

| # | Original | Nachbau | Grund |
|---|---|---|---|
| C1 | 70+ Factory-IRs im Installer | **keine IRs im Repo**; Nutzer lädt eigene WAV/AIF | fremder Content darf nicht weiterverteilt werden (vgl. A4) |
| C2 | Preset-Browser, User Presets, Automation Table, Master Lock | nicht Teil des Moduls (UI folgt) | reine UI-/Host-Verwaltung |
| C3 | True-Stereo-Umschalter je Preset (TS-Button) | Parameter `trueStereo` | im Modul automatisierbar |
| C4 | IR-Stretch „Speed" bipolar unbekannt skaliert | −12…+12 Halbtöne | musikalisch dokumentierte Skala |
| C5 | IR-Bild, Statusleiste (Rate/Länge/Kanäle/TS) | Diagnose-API (`currentIrLength`, `currentIrChannels`, `isTrueStereo`) | UI folgt in P9 |
| C6 | Preferences: Blockgröße/Latenz | `zeroLat`-Schalter | vereinfacht |
| C7 | externe IR-Verzeichnisse | nur Dateipfad | UI folgt |

---

## 6. Getestete Eigenschaften

| Testfall | Prüft |
|---|---|
| Parameterliste | 19 IDs, Referenzattribute p0…p3 |
| Klemmen | Wertebereiche, ungültige Indizes |
| Dirac-IR | Signal wird durchgereicht (Fehler < 2·10⁻³) |
| Predelay | Impuls erscheint nach 100 ms (Samples 4700…4900) |
| Stretch | +12 Halbtöne halbiert die IR-Länge (4800 → ~2400) |
| Decay | Tail/Head-Verhältnis < 0,8 bei `decay = 0,5` |
| Normalize | IR `[1,1]` → Ausgangspegel 0,5 (Energie 2) |
| Width 0 | Mono-Summierung |
| True Stereo | Kreuzantworten L→R/R→L, Abschalten liefert Stille |
| Zustand | `ir`, `<Eq>`, `<Dc>` Roundtrip |
| Kette | läuft stabil mit OTT und Dynamic EQ |
