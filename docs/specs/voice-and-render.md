# Sample-Voice und Kick-Renderer — Spezifikation und Vertrag

**Tickets:** B-30 (Sample-Voice), B-31 (Fade-Kurven), B-33 (Summenbus/Auto-Gain-Stufe),
B-34 (Auto-Gain), B-35 (Kick-Länge) — Phase 3 „Engine-Core"
**Code:** `src/engine/SampleVoice.h/.cpp`, `src/engine/KickRenderer.h/.cpp`
**Tests:** `tests/VoiceAndRenderTests.cpp` (Catch2 v3, Tag `[engine]`)
**Stand:** 2026-11-17 — mit MSVC 19.51 (`/std:c++20 /W4`) warnungsfrei übersetzt;
Testsuite grün: **29 Testfälle, 153.780 Assertions**. Pitch und Stretch sind **nicht**
Teil dieser Stufe (Ticket B-44); die Voice spielt mit Pitch 1,0 und Stretch 1,0, also
Originalwiedergabe.

---

## 1. Zweck und Aufteilung der Verantwortlichkeiten

Ein Kick besteht aus bis zu **12 Sample-Slots**. Jeder Slot wird von einer eigenen
`SampleVoice` gelesen (Position, Trim, Fades, Pegel); der `KickRenderer` summiert die
Voices, schneidet die Summe auf die Beat-Länge und regelt den Pegel.

| Ebene | Verantwortung | Datei |
|---|---|---|
| **Voice** (`SampleVoice`) | Einen getrimmten Ausschnitt eines Samples lesen, um `positionSeconds` verschieben, Fades anwenden, additiv auf einen Zielpuffer schreiben. Hält **keinen** eigenen Puffer. | `SampleVoice.h/.cpp` |
| **Renderer** (`KickRenderer`) | 12 Voices halten, Solo/Mute auflösen, Summe bilden, Kick-Länge anwenden, Auto-Gain messen und regeln. Hält den Summenpuffer (`scratch`). | `KickRenderer.h/.cpp` |

**Wichtig:** Die Voice kennt weder Beat-Länge noch Auto-Gain; der Renderer kennt kein
Sample-Fenster. Beide sind dadurch einzeln prüfbar (die Tests nutzen das aus).

### 1.1 Signalkette je Block

```
renderNextBlock (target, startSample, numSamples)
  │
  ├─ 1. samples = min (numSamples, Blockplatz, bei 1 BEAT: Rest bis zum Beat)
  ├─ 2. scratch leeren
  ├─ 3. für jede aktive Voice: voice.renderNextBlock (scratch, 0, samples)   [additiv]
  ├─ 4. Body-RMS im Fenster 20…120 ms messen   (VOR der Verstärkungsstufe!)
  ├─ 5. Hülle = Beat-Fade × Auto-Gain  → auf scratch multiplizieren
  └─ 6. scratch additiv in target[startSample …] addieren
```

Der Renderer ist damit **additiv**: bereits vorhandener Pufferinhalt bleibt erhalten
(Test „Renderer addiert auf einen bereits gefüllten Zielpuffer"). „Stille liefern" heißt
deshalb: nichts beitragen — ein vorher geleerter Puffer bleibt still.

---

## 2. Schnittstellen

### 2.1 `VoiceSettings` (aus dem Kick-Zustand gefüllt)

| Feld | Quelle im Zustand | Bedeutung |
|---|---|---|
| `positionSeconds` | `lN_pos` | Startversatz der Wiedergabe (Abschnitt 3) |
| `trimStartSeconds` | `lN_trimstart` | Beginn des gelesenen Fensters |
| `trimEndSeconds` | `lN_trimend` | Ende des Fensters; `≤ Start` ⇒ bis Sample-Ende |
| `fadeIn.lengthSeconds` / `.shape` / `.shapeAmount` | `lN_fadein` / `lN_ficurve` / `lN_fishape` | Einblenden |
| `fadeOut.lengthSeconds` / `.shape` / `.shapeAmount` | `lN_fadeout` / `lN_ficurve` / `lN_foshape` | Ausblenden |
| `gainLinear` | `lN_vol` (dB → linear) | Slot-Pegel |
| `mute`, `solo` | `lN_mute`, `lN_solo` | Schaltzustände |

Die Umrechnung Prozent → Sekunden macht der Aufrufer (UI/Parameter-Bridge), weil nur
er die getrimmte Slot-Länge kennt: `lengthSeconds = prozent * getrimmteDauer`.
`shapeAmount` ist der Biegeparameter `-1…+1` aus `fishape`/`foshape`.

> **Hinweis zu `ficurve`:** Der Parameter `lN_ficurve` (0…100 %) ist im Referenzformat
> ein Prozentwert; in dieser Stufe wird die Kurvenform über `FadeShape` gewählt. Die
> Abbildung `ficurve` → `FadeShape` (5 Stufen) ist offener Punkt O1.

### 2.2 `SampleVoice`

```cpp
class SampleVoice
{
public:
    static constexpr int kDeclickSamples = 32;
    static constexpr int kMaxChannels    = 2;

    static float shapeGain (FadeShape shape, float t, float amount) noexcept;

    void setSource (const juce::AudioBuffer<float>* sample, double sourceSampleRate);
    void prepare (double outputSampleRate, int maxBlockSize);
    void setSettings (const VoiceSettings&);
    void reset();
    void startAt (juce::int64 outputSamplePosition);
    bool isActive() const noexcept;
    void renderNextBlock (juce::AudioBuffer<float>& target, int startSample, int numSamples);

    double lengthSeconds() const noexcept;      // getrimmte Länge in Ausgabezeit
    juce::int64 lengthSamples() const noexcept; // getrimmte Länge in Ausgabe-Samples
    double readStep() const noexcept;           // Quell-Samples je Ausgabe-Sample
    bool   hasPlayableWindow() const noexcept;
};
```

Der Quellbuffer wird **nicht kopiert und nicht besessen**. Er muss die Voice überleben
und darf während der Wiedergabe nicht verändert oder neu allokiert werden
(Library-Puffer sind nach dem Laden stabil).

### 2.3 `KickRenderer`

```cpp
class KickRenderer
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setSlot (int slotIndex, const juce::AudioBuffer<float>* sample,
                  double sourceSampleRate, const VoiceSettings& settings);
    void setSlotSettings (int slotIndex, const VoiceSettings& settings);  // B-154: nur Einstellungen
    void clearSlot (int slotIndex);
    void setRenderSettings (const RenderSettings&);   // siehe Hinweis unten
    void noteOn();
    void renderNextBlock (juce::AudioBuffer<float>& target, int startSample, int numSamples);
    double beatLengthSeconds() const noexcept;        // 60 / bpm

    // Diagnose/Test: isPlaying(), numActiveVoices(), autoGainLinear(),
    // autoGainTargetLinear(), hasBodyMeasurement(), bodyRmsDb(), beatLengthSamples()
};
```

> **Abweichung vom Ticket-Entwurf:** `setRenderSettings()` / `renderSettings()` fehlten in
> der Aufgabenstellung, sind aber zwingend nötig — `RenderSettings` (BPM, KICK LENGTH,
> Auto-Gain) muss irgendwie in den Renderer kommen. Ebenfalls ergänzt: die reinen
> Lesezugriffe für Tests und Visualiser. Die im Ticket genannten Signaturen sind
> unverändert.

---

## 3. Zeitbezüge: Was ist relativ zu was?

Das ist die Stelle, an der Nachbauten am häufigsten danebenliegen. Verbindlich gilt:

| Größe | Bezugspunkt | Einheit |
|---|---|---|
| Trigger (`noteOn`) | Ausgabe-Sample 0 des Kicks | — |
| `positionSeconds` | Trigger → **Start der Voice** | Sekunden (Ausgabezeit) |
| `trimStartSeconds` / `trimEndSeconds` | **Sample-Anfang** der Quelle | Sekunden (Quellzeit) |
| `fadeIn.lengthSeconds` | **getrimmte Slot-Länge**, vom Voice-Start an | Sekunden |
| `fadeOut.lengthSeconds` | **getrimmte Slot-Länge**, vom Voice-Ende rückwärts | Sekunden |
| `beatLengthSeconds` | Trigger (`noteOn`) | Sekunden |

```
Trigger                      Voice-Start                     Voice-Ende
  │                              │                                │
  │◄──── positionSeconds ───────►│                                │
  │                              │◄──── getrimmte Länge L ───────►│
  Quelle:  ├── trimStart ────────┴──── trimEnd ──────────────────┤
  │                              │                                │
  Ausgabe:                       │◄── fadeIn ──┤   Vollpegel     ├─ fadeOut ─►│
                                 │                                │
```

### 3.1 Entscheidung: `positionSeconds` ist eine Verschiebung auf der Kick-Zeitachse

`lN_pos` verschiebt die **Voice** nach hinten (späterer Einsatz), nicht das Lese-Fenster
innerhalb des Samples. Begründung:

1. Das Referenzprodukt platziert Parts im Visualiser auf einer Beat-Zeitleiste; das
   Handbuch nennt für ALIGN ausdrücklich „MOVED BY HAND — Handplatzierung gewinnt",
   also gibt es eine manuelle Platzierung je Part. Das ist `lN_pos`.
2. Trim (`trimstart`/`trimend`) ist bereits der Bezug *innerhalb* des Samples. Eine
   zweite Read-Offset-Größe wäre redundant.
3. Nur so bleibt `pos` unabhängig von der Sample-Länge — im Referenzformat ist `pos`
   ein Prozentwert ohne Bezug zur Dateilänge.

Negative Werte werden als 0 behandelt (kein Vorlauf): Vorlauf entsteht in Phase 5
(ALIGN) durch Verschieben der Fenster, nicht durch negative Positionen im Einzel-Slot.

> **Offener Punkt O2:** Die Skala von `lN_pos` (Prozent → Sekunden) und die Frage, ob
> der Bezug die Beat-Länge, eine feste Zeitbasis oder die Kick-Gesamtdauer ist, muss am
> Original gemessen werden (F4). Der Renderer nimmt Sekunden entgegen und ist damit
> skalierungsneutral.

### 3.2 Sample-Rate-Wandlung

`step = (sourceRate / outputRate) · pitchRatio / stretchRatio`, gelesen wird mit
**linearer Interpolation** an der gebrochenen Position:

```
readPos(i) = trimStart·sourceRate + i · step
index      = floor (readPos)          (auf [0, n-1] geklemmt)
frac       = readPos - index          (auf [0,1] geklemmt)
x          = s[index] + (s[index+1] - s[index]) · frac
```

**Warum lineare Interpolation und nicht nur Indexschritt?** Ein reiner Indexschritt
(`index = round(i·step)`) erzeugt bei 44,1 → 48 kHz ein periodisches Jitter-Muster von
±0,5 Samples; das ist bei Transienten als „schmutziges" Höhenband hörbar. Die lineare
Interpolation kostet zwei Multiplikationen je Sample und Kanal, allokiert nicht und
dämpft lediglich die obersten Höhen (bei 44,1 → 48 kHz typisch < 1 dB unter 16 kHz) —
für One-Shot-Kicks (Transient + Körper, wenig Brillanzanteil) ein guter Kompromiss.
Ein sinc-/Polyphase-Resampler ist die dokumentierte Ausbaustufe (offener Punkt O3),
sinnvoll erst zusammen mit B-44, weil dort ohnehin ein vorbereiteter Puffer entsteht.

**Längenberechnung bewusst getrennt:** Die Länge wird **nicht** als
`Quell-Samples / step` gerechnet, sondern direkt aus dem Zeitbezug

```
Dauer   = Fensterlänge · stretchRatio / pitchRatio          (in Sekunden)
Samples = ceil (Dauer · outputRate)
```

Grund: `Quell-Samples / ((srcRate/outRate)·…)` kürzt `sourceRate` nur *fast* heraus und
kann je nach Rundung um ein Sample springen (bei 44,1 kHz → 48 kHz real beobachtbar).
Mit der Zeitformel ist „1 s bleibt 1 s" exakt: 1 s → 48 000 Samples.

---

## 4. Fade-Kurven (`SampleVoice::shapeGain`)

Öffentlich und damit direkt testbar. Eingabe ist der normierte Fortschritt `s ∈ [0,1]`
innerhalb der Fade-Strecke, Ausgabe der Verstärkungsfaktor `g ∈ [0,1]`, monoton steigend,
`g(0) = 0`, `g(1) = 1`. `amount ∈ [-1,+1]` ist der Biegeparameter `fishape`/`foshape`.

| `FadeShape` | Formel | `g(0,5)` bei `amount = 0` | Charakter |
|---|---|---|---|
| `linear` | `g = s` | 0,500 | Gerade; `amount` ohne Wirkung |
| `curve` | `g = s^p` mit `p = 4^amount` | 0,500 | `amount > 0` konvex (hält zurück), `amount < 0` konkav |
| `sCurve` | `u = s^p`, `g = u²(3 − 2u)` | 0,500 | Smoothstep, punktsymmetrisch: `g(s)+g(1−s)=1` |
| `fast` | `g = (1 − e^(−k s)) / (1 − e^(−k))`, `k = 4·2^amount` | 0,881 | schneller Anstieg, exponentiell abfallende Steigung |
| `slow` | `g = ln(1 + c s) / ln(1 + c)`, `c = 4·4^amount` | 0,683 | logarithmisch, sanfter als `fast` |

Eigenschaften, die der Test erzwingt:

* Endpunkte exakt 0 und 1 (bei jeder Kurve, jedem `amount`).
* Monotonie über 512 Stützstellen und fünf `amount`-Werte.
* `sCurve` bei `amount = 0`: `g(0,5) ≈ 0,5` und `g(0,25)+g(0,75) = 1`.
* `curve` mit `amount > 0` liegt an 0,5 **unter** der Geraden (konvex), mit
  `amount < 0` darüber (konkav).
* `fast(0,5) > slow(0,5) > 0,5` — die drei „früh ansteigenden" Kurven sind geordnet.

**Fade-Out wird über den verbleibenden Anteil ausgewertet**, nicht über die verstrichene
Zeit: bei `p = (L − t) / L_out` gilt `g = shapeGain (…, p, …)`. Damit ist die Kurve beim
Ausblenden dieselbe „von 0 nach 1"-Form, nur rückwärts gelesen — `fast` blendet also
spät und steil aus, `curve` mit positivem `amount` früh und flach.

---

## 5. Mono-/Stereo-Verhalten

| Quelle | Ziel | Ergebnis |
|---|---|---|
| Mono | Stereo | derselbe Wert auf **beiden** Kanälen |
| Stereo | Stereo | L → L, R → R (keine Mischung) |
| Stereo | Mono | Mittelwert `0,5·(L+R)` |
| Mono | Mono | direkt |
| n (>2) | 2 | Kanäle jenseits der Quellkanalzahl erhalten den **letzten** Quellkanal |

Der Renderer arbeitet intern immer stereo (`kChannels = 2`). Ein Mono-Ziel bekommt beim
Zusammenmischen `0,5·(L+R)`; bei einem Mono-Kick (beide Kanäle gleich) ändert das den
Pegel nicht.

---

## 6. Declick

Beim Start und am Ende wird **mindestens** eine lineare Rampe über
`kDeclickSamples = 32` Samples erzwungen (32 Samples = 0,67 ms bei 48 kHz), auch wenn
`fadeIn`/`fadeOut` 0 sind:

```
attackRamp (i)  = min (1, (i + 1) / 32)              i = Sample seit Voice-Start
releaseRamp(i)  = min (1, (L − 1 − i) / 32)          L = Länge in Ausgabe-Samples
```

Die Rampe wirkt als **Obergrenze** (`min`) auf den Fade-Faktor, nicht als Untergrenze:

```
gainIn   = min (fadeInFaktor,  attackRamp)
gainOut  = min (fadeOutFaktor, releaseRamp)
gain     = gainLinear · gainIn · gainOut
```

Damit startet jede Voice hörbar bei 0 (erstes Sample = `1/32 · gainLinear`), das letzte
Sample ist **exakt 0**, und ein bereits laufender, längerer Fade wird nicht verändert
(kein doppeltes Blenden, keine Knickstelle). Der Test prüft Rampe, Monotonie und den
exakten Endwert 0.

---

## 7. Kick-Länge (KICK LENGTH)

`beatLengthSeconds() = 60 / bpm`, mit `bpm` geklemmt auf `[20, 960]` (schützt vor
Division durch 0 und vor Unsinnswerten; NaN fällt auf 150 zurück).

| Einstellung | Verhalten |
|---|---|
| `cutToOneBeat = true` („1 BEAT") | Ab `beatSamples = round(60/bpm · rate)` trägt der Renderer nichts mehr bei. Die letzten `tailFadeSamples` davor werden mit einer **raised cosine** `g = 0,5·(1 + cos(π·u))` von 1 auf 0 geblendet; das letzte Sample vor dem Beat ist exakt 0. Danach ist der Kick beendet (`isPlaying() == false`). |
| `cutToOneBeat = false` („FULL SAMPLE") | Keine Begrenzung; die Voices laufen bis zu ihrem Sample-Ende. Der Kick endet, wenn keine Voice mehr aktiv ist. |

Warum raised cosine und nicht linear? Ein linearer Auslauf hat am Fade-Beginn eine
Knickstelle (hörbar als leises „Klacken" bei sehr kurzem `tailFadeSeconds`); die
Cosinus-Blende startet und endet mit Steigung 0. `tailFadeSeconds = 0,01` (10 ms) ist
der Default aus der Aufgabenstellung; ein Wert ≥ Beat-Länge blendet den ganzen Kick aus
und wird auf die Beat-Länge geklemmt.

**Blockgrenzen:** Liegt das Beat-Ende innerhalb eines Blocks, wird der Block an dieser
Stelle abgeschnitten und der Kick beendet. Der Rest des Puffers bleibt unberührt
(additive Semantik), also still, wenn der Puffer vorher geleert wurde.

---

## 8. Auto-Gain

### 8.1 Definition

Nach dem Trigger wird der Pegel des **Bodys** gemessen:

```
Fenster:  [20 ms, 120 ms) nach dem Trigger           (kBodyWindowStart/EndSeconds)
Messgröße: RMS über beide Kanäle im Fenster
           meanSquare = Σ x² / (Samples · Kanäle)
           bodyDb     = 10 · log10 (meanSquare)      (== 20 · log10 (RMS))
Korrektur:  correctionDb = clamp (autoGainTargetDb − bodyDb, ±24 dB)
Ziel:       autoGainTarget = 10^(correctionDb / 20)
```

Das Fenster beginnt bewusst **nach** der Transientenspitze und endet nach 120 ms: das
Referenzprodukt pegelt auf den Körper des Kicks, nicht auf die Spitze („Level management
reads the body of the kick, not the transient spike"). Transienten dürfen dadurch über
0 dBFS laufen — das ist eine Klangentscheidung, keine Nachlässigkeit
(Bestandsaufnahme 7, Punkt 2).

### 8.2 Messpunkt: vor der Verstärkungsstufe

Gemessen wird die **trockene Summe** (Schritt 4 der Signalkette, vor Schritt 5). Die
Regelung ist damit **offen**, nicht rückgekoppelt: die Korrektur wird aus dem
unverstärkten Signal berechnet und danach angewendet. Folge:

* Kein Aufschaukeln, keine Iteration, kein „Pumpen" über mehrere Kicks.
* Der **erste** Kick nach einer Belegungsänderung läuft noch mit der alten Verstärkung;
  ab dem zweiten stimmt der Pegel. Das ist die dokumentierte Vereinfachung dieser
  ersten Fassung.

### 8.3 Glättung

Jede Änderung wird über eine lineare Rampe von mindestens **20 ms** gefahren
(`kAutoGainRampSeconds`), sample-genau im Renderpfad. Ein Sprung tritt nur bei
`reset()` auf (dort wird die Rampe sofort auf das Ziel gesetzt).

### 8.4 Grenzen und Grenzfälle

| Fall | Verhalten |
|---|---|
| Body-RMS = 0 (Stille) | Keine Korrektur, keine Division durch 0; `hasBodyMeasurement()` bleibt `false`. |
| Kick kürzer als 120 ms | Gemessen wird über das vorhandene Fenster (Teilfenster). |
| Korrektur > +24 dB (sehr leises Sample) | Auf +24 dB geklemmt (Faktor 15,849). |
| Korrektur < −24 dB (sehr lautes Sample) | Auf −24 dB geklemmt (Faktor 0,063). |
| `autoGain = false` | Keine Messung, keine Korrektur; die Verstärkung rampt auf 1,0. |
| Belegung geändert (`setSlot`/`clearSlot`) | Messwert wird ungültig; die Verstärkung **bleibt stehen** (kein Pegelsprung), der nächste Kick misst neu. |
| `reset()` | Rampe wird abgeschlossen, Messung verworfen; die Verstärkung bleibt erhalten (Stop/Start erzeugt keinen Pegelsprung). |
| `settings.autoGainTargetDb` = NaN | Messung wird übersprungen (defensiv). |

**Bewusste Grenzen dieser Fassung:**

1. Der Zielpegel wird auf den **RMS des Bodys** bezogen, nicht auf einen True-Peak oder
   ein Loudness-Maß. Ein Kick mit sehr impulsivem Körper kann nach der Regelung im
   Spitzenpegel deutlich über 0 dBFS liegen — genau wie beim Referenzprodukt.
2. Das Fenster ist **fest** (20…120 ms) und nicht tempoabhängig. Bei sehr kurzen Beats
   (< 140 ms, also > 428 BPM) liegt das Fenster teilweise hinter dem Beat-Ende.
3. Die Regelung ist offen (siehe 8.2): der erste Kick nach einer Änderung ist nicht
   kalibriert.
4. Es gibt keine Hysterese und keine Totzone; die Rampe verhindert nur Sprünge.

> **Offener Punkt O4:** Fensterlage und -breite sind eine Arbeitsannahme (F4 der
> Bestandsaufnahme). Messung am Original über Drag-Master-Export nötig.

---

## 9. Erweiterungspunkte für Pitch und Stretch (Ticket B-44)

Die Voice ist so gebaut, dass B-44 **nur** `SampleVoice` erweitert, nicht den Renderer.
Alle Stellen sind im Code mit `Erweiterungspunkt B-44` markiert:

```cpp
// SampleVoice.h (privat)
double pitchRatio   = 1.0;   // 2^(Halbtöne/12)
double stretchRatio = 1.0;   // Zeitfaktor: >1 = länger
```

Zwei abgeleitete Größen hängen ausschließlich daran:

```
Leseschritt   step   = (sourceRate / outputRate) · pitchRatio / stretchRatio
Dauer         Dauer  = Fensterlänge · stretchRatio / pitchRatio
Länge         L      = ceil (Dauer · outputRate)
```

In P3 sind beide Faktoren 1,0 — es gibt **keinen** Schalter und **keinen** toten Code
dafür, nur die Formel, die mit 1,0 exakt die Originalwiedergabe ergibt.

**Wichtig für B-44:** Dieses Modell kann nur „Repitch" (Länge und Tonhöhe gekoppelt),
weil es ein reiner Resampler ist. Für `KEEP PITCH` (längenneutral) muss B-44 einen
vorbereiteten WSOLA-Puffer als **Quelle** einsetzen (`setSource` mit dem gestreckten
Puffer, `sourceRate = outputRate`, `stretchRatio = 1`) — dann bleibt diese Klasse
unverändert, und nur die Vorbereitung (B-45, asynchroner Renderer + Cache) kommt hinzu.
Der `KickRenderer` muss dafür nichts wissen.

---

## 10. Threading und Allokationsfreiheit

| Methode | Thread | Allokiert? |
|---|---|---|
| `SampleVoice::setSource/prepare/setSettings/reset/startAt` | Message-Thread | nein (nur Arithmetik) |
| `SampleVoice::renderNextBlock` | Audio-Thread | **nein** — kein Puffer, kein Log, keine Sperre |
| `KickRenderer::prepare` | Message-Thread | ja (`scratch.setSize`), einmalig |
| `KickRenderer::setSlot/clearSlot/setRenderSettings/noteOn/reset` | Message-Thread | nein |
| `KickRenderer::renderNextBlock` | Audio-Thread | **nein** |

**Zusicherung (nicht durch Catch2 prüfbar):** Im Renderpfad steht kein Allokator, kein
Log-Aufruf (`bfka::log`) und kein Mutex. `SampleVoice` hält gar keinen eigenen Puffer —
sie liest direkt aus dem Quellbuffer und schreibt additiv in den Zielbuffer.
`KickRenderer` legt seinen Summenpuffer ausschließlich in `prepare()` an und arbeitet im
Renderpfad nur mit Zeigern auf diesem Puffer. Ein Zähler-/Hook-Test ist mit Catch2 nicht
möglich (kein malloc-Hook); die Prüfung erfolgt deshalb im Code-Review.

**Nicht parallel erlaubt:** `setSlot`/`clearSlot`/`prepare`/`noteOn` dürfen **nicht**
gleichzeitig mit `renderNextBlock` laufen. Wie der Plugin-Prozessor das absichert
(Note-Grenze, Lock-freier Tausch, doppelter Puffer), ist Sache der Integration
(offener Punkt O5).

---

## 11. Solo-/Mute-Auflösung

* `mute` schlägt alles: ein stummer Slot klingt nicht, auch wenn er Solo hat.
* Ist **irgendein belegter** Slot auf Solo, klingen nur Solo-Slots.
* Beide Flags werden **beim `noteOn()`** ausgewertet. `setSlotSettings()` (B-154)
  kann sie waehrend eines laufenden Kicks umschalten; das wirkt auf den
  **naechsten** Kick. Eine laufende Voice wird nur dann angehalten, wenn sie
  durch die neuen Einstellungen stumm oder unspielbar wird
  (`SampleVoice::setSettings`).
* Ein **leerer** Slot mit Solo schaltet den Kick **nicht** stumm (er kann nichts
  beitragen, also soll er auch nichts unterdrücken).
* `mute` wird in der Voice selbst behandelt (die Voice wird gar nicht gestartet), Solo im
  Renderer (dafür braucht es die Summe aller Slots).

---

## 12. Offene Punkte

| # | Frage | Warum wichtig | Methode |
|---|---|---|---|
| O1 | Abbildung `lN_ficurve` (0…100 %) → `FadeShape` + `shapeAmount` | Preset-Treue der Fades | Original-UI ↔ Preset-Werte vergleichen (F1) |
| O2 | Skala von `lN_pos` (Prozent → Sekunden) und Bezug (Beat, Kick-Länge, feste Zeit) | Position klingt sonst anders | Messung am Original (F4) |
| O3 | Reicht lineare Interpolation, oder braucht es einen Polyphase-Resampler? | Höhenqualität bei 44,1 → 48 kHz | Nulltest gegen Drag-Master-Export |
| O4 | Lage/Breite des Auto-Gain-Fensters (20…120 ms) und Bezugsgröße (RMS vs. Peak) | Pegelgleichheit | Messung am Original (F4) |
| O5 | Wie wird `setSlot` im laufenden Betrieb gegen den Audio-Thread getauscht? | Datenrennen | Integration mit `PluginProcessor` |
| O6 | Verhalten von `lN_pos` bei negativen Werten in der UI (Vorlauf) | ALIGN-Interaktion | Phase 5 (B-50…) |
| O7 | Exakte Fade-Out-Kurvenzuordnung des Referenzprodukts (welche Kurve gehört zu `foshape`?) | Klanggleichheit | Messung der Hüllkurve am Original |

---

## 13. Testabdeckung (`tests/VoiceAndRenderTests.cpp`)

| Bereich | Testfälle |
|---|---|
| Fade-Kurven | Endpunkte linear, Monotonie aller Kurven × 5 `amount`-Werte, `sCurve`-Symmetrie, `curve` konvex/konkav, Ordnung `fast > slow > linear` |
| Voice | Gleichspannung = 1,0 (inkl. Declick 1/32 … 1,0 … 0), additive Addition, Declick-Rampe, Trim-Fenster, `trimEnd = 0`, Position/Verschiebung, Fade-In 50 %, Fade-Out mit Kurvenvergleich, Mono/Stereo-Matrix, 44,1 → 48 kHz, Mute |
| Renderer | `beatLengthSeconds` (150/120/160 BPM, Klemmung), 1 BEAT schneidet bei 0,4 s, Schnitt bei ungerader Blockgröße (7 Samples), FULL SAMPLE klingt weiter, Summe zweier Slots, additive Addition, Solo/Mute (4 Fälle), `clearSlot`, ungültige Indizes, Aufruf ohne `prepare`/`noteOn`, `reset`, zweiter `noteOn` |
| Auto-Gain | Messwert −6,02 dBFS → Faktor 2,0; zweiter Kick mit Body 0 dBFS; abgeschaltete Regelung; Klemmung auf +24 dB |
