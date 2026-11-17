# FX-Ketten-Engine — Spezifikation und Vertrag

**Ticket:** B-69 (Ketten-Engine), Grundlage für B-60…B-68 (die neun Referenzmodule)
sowie für die Phase-2-Module `ott`, `reverb` und `dynEq` (B-140…B-142)
**Code:** `src/fx/FxModule.h`, `src/fx/FxFactory.h/.cpp`, `src/fx/PassThroughModule.h`,
`src/fx/FxChainProcessor.h/.cpp`
**Tests:** `tests/FxChainTests.cpp` (12 Testfälle, 651 Assertions)
**Stand:** 2026-11-17 — Übersetzungseinheiten mit MSVC 19.51 (`/std:c++20 /W4`)
warnungsfrei übersetzt, Testsuite grün.

---

## 1. Zweck und Aufteilung der Verantwortlichkeiten

Das FX-Rack ist eine **geordnete Kette** von Modulen. Die Summe der Slots läuft von oben
nach unten durch die Kette; jedes Modul bearbeitet sie in-place und gibt sie weiter.

Die Verantwortung ist strikt getrennt:

| Ebene | Verantwortung | Datei |
|---|---|---|
| **Modul** (`FxModule`) | DSP des Modultyps. Bekommt das Signal, wie es am Modul ankommt, und schreibt **sein zu 100 % nasses** Signal zurück. | je Modul (B-60…B-68) |
| **Registry** (`FxFactory`) | Typ-Kennung (`"eq"`, `"dist"`, …) → Erzeugerfunktion. | `FxFactory.h/.cpp` |
| **Kette** (`FxChainProcessor`) | Reihenfolge, Bypass, **MIX**, **OUT GAIN**, Latenzausgleich, Display-Daten, Parameterverteilung. | `FxChainProcessor.h/.cpp` |

**Wichtig:** MIX und OUT GAIN gehören **nicht** ins Modul. Ein Modul kennt weder den
Trockenpfad noch die Pegelstufe der Karte. Damit ist jede MIX-Semantik zentral
änderbar, und ein Modul kann niemals versehentlich doppelt blenden.

### 1.1 Signalkette je Block

```
Eingang (Summe)
  └─ für jedes Modul in Kettenreihenfolge:
       pre-Display   ← Kopie des Eingangs in das Modul
       Sidechain     ← optional: Kopie des zweiten Eingangs, verzögert um ΣL der Vormodule
       trocken       ← Kopie (nur wenn nötig)
       Modul.process()          → nass (mit Modullatenz L)
       out = trocken·(1−mix) + nass·mix
       out *= outGain
       post-Display  ← Kopie des Ausgangs
Ausgang
```

### 1.2 Externer Sidechain (B-146)

Detektor-Module (bisher `dynEq`) können statt ihres Eingangs ein zweites Signal
auswerten. Der Vertrag besteht aus zwei Methoden in `FxModule`:

```cpp
virtual void setSidechainBuffer (const juce::AudioBuffer<float>* sidechain) noexcept {}
virtual bool acceptsExternalSidechain() const noexcept { return false; }
```

* Die Engine setzt den Zeiger **vor** jedem `process()` und nimmt ihn danach
  wieder weg; ein Modul darf ihn nicht speichern.
* Die Verzögerung ist die Summe der Latenzen der **Vormodule**
  (`sidechainLatencyFor(slot)`), damit Detektor und Nutzsignal fluchten.
* `FxChainProcessor::process (buffer, sidechain = nullptr)` — ohne Argument
  bleibt alles wie bisher (interne Detektoren).
* Das Plugin bietet dafür zwei optionale Busse (`Input` = Haupt-Eingang,
  `Sidechain` = Aux) und kopiert den Detektor vor dem Leeren des
  Ausgangspuffers (In-Place-Mapping von JUCE).

---

## 2. Schnittstelle `FxModule`

```cpp
class FxModule
{
public:
    virtual ~FxModule() = default;

    virtual FxType type() const noexcept = 0;
    virtual const std::vector<FxParamDesc>& params() const noexcept = 0;
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void setParam (int index, float value) = 0;   // Anzeige-Einheiten
    virtual float getParam (int index) const = 0;
    virtual void process (juce::AudioBuffer<float>& buffer) = 0;   // in-place
    virtual int latencySamples() const noexcept { return 0; }
    virtual juce::String displayName() const = 0;
};
```

### 2.1 Parameter

* `params()` liefert die Tabelle in Anzeige-Reihenfolge. Der **Index in dieser Tabelle
  ist die Nummer für `setParam()`/`getParam()`** und ist gleichzeitig die spätere
  Zuordnung zu den Host-Proxy-Parametern `fx<N>_p<M>` (Abschnitt 7.1, offener Punkt F2).
* `FxParamDesc::id` ist **exakt der Attributname des Referenz-XML** (`"drive"`,
  `"mode"`, `"folds"`, …). Nur so kann die Kette Modulzustände ohne Sonderwissen
  übertragen.
* Werte in `setParam()`/`getParam()` sind **Anzeige-Einheiten** (Prozent, dB, Hz,
  Modus-Index) — nicht auf 0…1 normalisiert. Die Normalisierung passiert außerhalb
  (`bfka::ParamDesc`, Host-Automation).
* Die Kette klemmt Werte auf `[minValue, maxValue]`, bevor sie `setParam()` ruft;
  Module sollten zusätzlich selbst klemmen (doppelte Absicherung).

### 2.2 Threading-Vertrag

| Methode | Thread | Darf allokieren | Darf loggen |
|---|---|---|---|
| `prepare()`, `reset()` | Message-Thread | ja | ja |
| `setParam()` | Message-Thread | nein | nein |
| `process()` | Audio-Thread | **nein** | **nein** |
| `latencySamples()` | Message-/Audio-Thread (nur lesend) | nein | nein |

`prepare()` darf mehrfach gerufen werden und muss `reset()` einschließen.
Parameter, die im laufenden Betrieb vom Audio-Thread gelesen werden, müssen atomar
oder doppelt gepuffert sein (Beispiel: `PassThroughModule` hält seinen Gain in einem
`std::atomic<float>`).

---

## 3. MIX-Semantik (Entscheidung D1)

### 3.1 Festlegung

Je Modul, nach dem Modul-DSP, vor OUT GAIN:

```
nass    = Ausgang des Moduls
trocken = Eingang des Moduls, um die Modullatenz L verzögert
out     = trocken · (1 − mix) + nass · mix
out    *= outGain
```

* **lineare Überblendung** (kein Equal-Power). `mix = 0` ist bitidentisch zum Eingang,
  `mix = 1` bitidentisch zum Modulausgang, `mix = 0.5` ist das arithmetische Mittel.
  Das ist die einzige Blende, die bei „nur trocken“ und „nur nass“ exakt die Endpunkte
  trifft und damit den Bypass-Nulltest (−120 dBFS) erfüllbar macht.
* **Der Trockenpfad wird um die Modullatenz verzögert**, damit trocken und nass
  phasengleich sind (Abschnitt 4).
* **`mix ≤ 0` und `enabled == false` werden gleich behandelt: das Modul wird
  vollständig übersprungen** — kein DSP, **kein OUT GAIN**, aber die Latenz bleibt
  erhalten.

### 3.2 Begründung

1. *Gleiches Verhalten für beide Abschaltgründe.* Wäre OUT GAIN bei `mix = 0` aktiv,
   würde ein Modul bei MIX 0 den Pegel ändern, bei Power-off aber nicht. Die Karte
   hätte damit zwei verschiedene „aus“-Zustände. Beides als „übersprungen“
   (Spec-Vorgabe: „bei `enabled == false` oder `mix == 0` überspringen“) ist
   konsistent und im Nulltest prüfbar.
2. *Kein Equal-Power in der Kette.* Equal-Power ist eine **klangliche** Entscheidung,
   die zum Modul gehört (der Convolver hat laut Handbuch eine eigene
   Equal-Power-Blende zwischen IR-Wet und Direktsignal). Die Kette bleibt neutral und
   damit für alle neun Module gleich.
3. *Kein Allokieren, keine Verzweigung pro Sample.* Die Blende läuft als
   Multiplikationsschleife über den Block.

### 3.3 Umsetzung

* Kopiert wird nur, wenn es nötig ist: `needDry = (mix < 1) || (latency > 0)`.
  Bei `mix == 1` und `latency == 0` läuft das Modul direkt auf dem Buffer.
* Live-Änderungen von `mix`/`outGain`/`enabled` sind **atomar** und ohne Sperre
  (`setModuleMix()`, `setModuleOutGain()`, `setModuleEnabled()`); sie werden pro Block
  einmal gelesen. Es gibt **noch keine Glättung** der Sprünge (siehe Abschnitt 9, O4).

---

## 4. Latenz-Behandlung

### 4.1 Grundsätze

1. **Die Latenz gehört zum Modul.** Ein Modul verantwortet seine eigene Verzögerung
   (FIR-Linearpfad, Lookahead, Faltungs-Partition) und meldet sie über
   `latencySamples()`. Die Kette verzögert das Modul **nicht** selbst.
2. **Die Kette summiert und meldet.** `totalLatencySamples()` ist die Summe aller
   Modullatenzen der aktuell gebauten Kette (auch der abgeschalteten). Der Prozessor
   gibt sie in `prepareToPlay()` über `setLatencySamples()` an den Host (PDC); der Host
   kompensiert sie gegenüber anderen Spuren.
3. **Der Trockenpfad fluchtet.** Für die MIX-Blende wird das trockene Signal über eine
   pro Modul reservierte Ringlinie um genau `latencySamples()` verzögert. Sonst
   entstünde bei `mix < 1` ein Kammfilter.
4. **Abschalten erzeugt keinen Sprung.** Ein übersprungenes Modul schiebt das Signal
   weiterhin um seine Latenz nach hinten. `totalLatencySamples()` bleibt gleich; der
   Host muss seine Kompensation nicht nachziehen, und ein A/B-Vergleich „Modul an/aus“
   bleibt zeitlich deckungsgleich.
5. **Latenz wird nach `prepare()` gelesen.** Module dürfen ihre Latenz von der
   Sample-Rate abhängig machen (z. B. Crossover-FIR, Lookahead in ms). Die Kette fragt
   `latencySamples()` erst nach `module->prepare()` ab und cached den Wert.

### 4.2 Erwartete Latenzen der Module

| Modul | Erwartung |
|---|---|
| `filt`, `eq`, `mseq`, `trans`, `dist`, `shaper`, `ott` (Minimum-Phase) | 0 |
| `ott` mit **LINEAR PHASE**-Split (FIR) | Latenz = (Taps−1)/2, muss gemeldet werden |
| `reverb` | 0 bei Zero-Latency-Kopf (erste Partition direkt), sonst = Partitionslänge |
| `final` | **Lookahead** für True-Peak-CLIP (siehe unten) |

### 4.3 Lookahead beim Finalizer (True-Peak)

Das 0-dBFS-Ceiling des Finalizers ist ein **True-Peak**-Ceiling: damit ein
Inter-Sample-Peak nicht durchrutscht, muss der Begrenzer die Spitze **vor** dem
Eingriff sehen.

* Empfehlung: **Lookahead 1,5 ms** (bei 48 kHz = 72 Samples), Überabtastung ×4 für die
  Spitzenerkennung. Das ergibt ein sicheres Ceiling ohne hörbare Pump-Artefakte.
* Damit meldet `final` genau diesen Wert als `latencySamples()`. Konsequenz für den
  Host: der komplette Kick verschiebt sich um die Latenz — bei einem One-Shot
  unkritisch, aber der Wert muss korrekt gemeldet sein, sonst verschiebt sich der Kick
  gegenüber dem Raster.
* **Alternative für „latency-free“-Betrieb:** ein zweiter Betriebsmodus ohne Lookahead
  (reines Clip, kein True-Peak) — dann 0 Samples, aber kein garantierter True-Peak.
  Auswahl als Modulparameter, Entscheidung offen (Abschnitt 9, O2).
* **Wichtig:** Der Lookahead-Bedarf **verschiebt das Display**. `ModuleDisplay::pre`
  zeigt das Signal am Eingang, `post` das verzögerte Ausgangssignal. Die Oberfläche
  muss die Latenz beim Vergleich zweier Displays berücksichtigen (oder die Kette
  liefert später ein latenzkompensiertes `pre`).

### 4.4 Host-Anbindung

* `prepareToPlay()`: `fx.prepare (sampleRate, samplesPerBlock)`, danach
  `setLatencySamples (fx.totalLatencySamples())`.
* Nach jedem `setChain()` (Modul hinzufügen/entfernen/umsortieren) muss der Prozessor
  `setLatencySamples()` erneut aufrufen — Werte können sich ändern.

---

## 5. Display-Daten

```cpp
struct ModuleDisplay
{
    bool valid;
    juce::AudioBuffer<float> pre;    // Signal AM Modul (Eingang)
    juce::AudioBuffer<float> post;   // Signal NACH MIX und OUT GAIN
};
const ModuleDisplay& displayFor (int moduleIndex) const;
```

* Fensterbreite `N = min(maxBlockSize, 512)` Samples, in `prepare()` reserviert.
  Der Audio-Thread kopiert darin allokationsfrei die letzten `N` Samples des Blocks
  (linksbündig; bei kürzeren Blöcken ist der Rest genullt).
* Spezifikationsvorgabe „Wellenform wie sie AM Modul ankommt, nicht danach“ ist über
  `pre` erfüllt. `post` kommt zusätzlich, weil die Karte sonst keine Wirkung zeigt.
* `displayFor()` auf einen ungültigen Index oder auf einen Kettenplatz mit
  unbekanntem Modultyp liefert eine statische, ungültige Anzeige (`valid == false`).
* Keine Sperre: der Audio-Thread schreibt, die UI liest. Ein Lesevorgang kann einen
  halb aktualisierten Block sehen — für eine Wellenform unkritisch.

---

## 6. Registry, Kettenaufbau, Parameter

### 6.1 `FxFactory`

```cpp
FxFactory& FxFactory::get();                                    // Singleton
void registerType (FxType, Creator);                            // ersetzt/entfernt
std::unique_ptr<FxModule> create (const juce::String& typeString) const;
juce::StringArray availableTypes() const;                       // "filt","eq",…
bool isRegistered (FxType) const;
```

* Der Konstruktor registriert **alle neun Typen mit `PassThroughModule`**. Ein echtes
  Modul übernimmt seinen Platz später mit einem einzigen Aufruf beim Start:
  ```cpp
  FxFactory::get().registerType (FxType::distortion,
                                 [] { return std::make_unique<DistortionModule>(); });
  ```
* Unbekannte Kennung oder nicht registrierter Typ → `nullptr`. Es wird **nicht** in der
  Registry geloggt; die Meldung macht die Kette (sie kennt Position und Kontext).
* Registrierung ist nicht threadsicher gegen `create()` — sie passiert beim Start.

### 6.2 `setChain()`

1. Für jeden `FxModuleState` der Kette: Instanz über die Registry erzeugen.
2. Parameter aus `FxModuleState::data` übernehmen: **jedes Attribut, dessen Name
   `FxParamDesc::id` entspricht**, in Anzeige-Einheiten, geklemmt auf den deklarierten
   Bereich. Fehlende/krumme Attribute (String, void) werden ignoriert, das Modul behält
   dann seinen Default. Danach erhält das Modul den **Struktur-Zustand** über
   `setState (state.data)` (Bänder, Kurven, IR-Pfad, Envelopen).
3. `enabled`, `mix`, `outGain` in die Laufzeit-Slots übernehmen.
4. Unbekannte Typen: **Platz bleibt erhalten** (`modules()[i] == nullptr`) und
   `bfka::log::w` meldet es. Dadurch stimmen Kartenindex und Kettenposition der UI
   überein, und der Zustand bleibt beim Hin-und-Her-Konvertieren verlustfrei
   (vgl. `04-SPEC-STATE-AND-PRESET-FORMAT.md`, 5.2).
5. Ist die Kette bereits `prepare()`t, werden die neuen Module sofort vorbereitet
   (Reihenfolge: `prepare` → Latenz lesen → Ringlinien/Buffer bauen → `reset`).

`FxChain::bypassAll` wird übernommen (`setBypassAll()` / `isBypassAll()`); im
Bypass rechnet kein Modul, die Latenzen bleiben aber erhalten (gleiche Begründung wie
in 4.1 Punkt 4).

### 6.3 Andocken von Modul-Presets (B-93)

Ein **Modul-Preset** ist genau ein `FxModuleState`:

```xml
<Module type="dist" on="1" mix="1.0" outGain="1.0" drive="45" mode="3" …>
  <WNode …/>
</Module>
```

* **Speichern:** `type` + `enabled`/`mix`/`outGain` + `data` (ValueTree) in eine Datei
  schreiben (`.bfkamodule`, Schema wie `FxChainPreset`).
* **Laden:** Der Zustand ersetzt `FxModuleState` an dieser Kettenposition; es genügt
  `setChain()` erneut zu rufen — die Kette baut die Instanz neu und überträgt die
  Attribute.
* **Pfeile im Kartenkopf** („nächstes/vorheriges Modul-Preset“) sind reine
  Dateiauswahl; kein Sonderpfad in der Engine nötig.
* **Default je Typ:** `FxFactory::create(type)` ohne `data` liefert die Modul-Defaults
  der Klasse — das ist der „Reset auf Werkszustand“.

### 6.4 Duplizieren

Ein Modul wird dupliziert, indem der `FxModuleState` **kopiert** und direkt hinter dem
Original in `FxChain::modules` eingefügt wird (`insert (begin + i + 1, copy)`), danach
`setChain()`. Eigene Laufzeitdaten (Filterzustände, Hüllkurven) werden **nicht**
mitkopiert — das Duplikat startet frisch (`reset()`), was dem Verhalten des Originals
entspricht. Für „Copy/Paste“ gilt dasselbe; die Zwischenablage hält nur den Zustand.

---

## 7. Die zehn Modultypen

### 7.1 Kennungen und Enum

| `type=` | `FxType` | Anzeigename | Parameter (XML-Reihenfolge, Anzeige-Einheiten) |
|---|---|---|---|
| `filt` | `filters` | Filters | *nicht im XML belegt*; laut Handbuch: `CUTOFF`, `RESO`, `DRIVE`, `TYPE`, `SLOPE`, `POLARITY` |
| `eq` | `equalizer` | Equalizer | `lowgain`, `lowfreq`, `midgain`, `midfreq`, `midq`, `highgain`, `highfreq`, `middyn` + bis 12 `<Band kind f q g dyn ch>` |
| `mseq` | `midSideEq` | Mid/Side EQ | `midlow`, `midhigh`, `sidelow`, `sidehigh`, `width`, `middyn`, `sidedyn`, `msolo`, `ssolo`, `mmute`, `smute` + `<Band … ch>` |
| `dist` | `distortion` | Distortion | `drive`, `mode`, `folds`, `agc`, `eqmode`, `flip` + `<WNode x y ten seg>`-Kurve |
| `shaper` | `kickShaper` | Kick Shaper | `sPre`, `sPost`, `sLine`, `shape`, `amount` + `<Pt x y t>`-Hüllkurve |
| `trans` | `transientShaper` | Transient Shaper | `amount`, `attack`, `duck`, `speed` |
| `reverb` | `convolver` | Convolver | `wet`, `stereo`, `tone`, `speed` + Phase-2-Attribute (Predelay, Decay, Modulation, Reverse, Mono-In, Normalize, IR-Gain, True-Stereo, Zero-Latency, Time-Hüllkurve) + `<Eq>`/`<Dc>`-Kurven (siehe fx-convolver.md) |
| `final` | `finalizer` | Finalizer | `boost`, `freq`, `clip`, `upward`, `monoF`, `shelfOn`, `monoOn`, `clipOn` |
| `ott` | `multiband` | Multiband | `depth`, `time`, `x1`, `x2`, `gl`, `gm`, `gh`, `linear` + OTT-Attribute (`inGain`, `cleanXov`, `thL/M/H`, `bu*`, `bd*`, `upwdStr`, `dnwdStr`, siehe fx-multiband.md) |
| `dynEq` | `dynamicEq` | Dynamic EQ | 19 globale Attribute + 4 Bänder mit je 11 Attributen (TDR-Nova-Integration, siehe fx-dynamic-eq.md) |

**Jedes Modul hat zusätzlich:** `on` (Bypass), `mix`, `outGain` — diese drei sind
**Kettenzustand** (`FxModuleState`), keine `FxParamDesc`-Einträge.

### 7.2 Beobachtete Wertebereiche (aus 76 Ketten + 44 Snapshots)

Nur Orientierung für die `FxParamDesc`-Bereiche der Module; die verbindlichen Bereiche
setzt das jeweilige Modul-Ticket. `[D]` = aus den Referenzdaten abgeleitet.

| Modul | Parameter | beobachtet min … max (distinct) |
|---|---|---|
| `dist` | `drive` | 0 … 100 (55) |
| | `mode` | 1 … 12 (4) |
| | `folds` | 2,0 … 2,4 (2) |
| | `agc`, `flip` | 0/1 (je 1) |
| | `eqmode` | 0 … 0 (1) |
| `eq` | `lowgain`/`midgain`/`highgain`/`middyn` | 0 (je 1) |
| | `lowfreq` | 90 (1) |
| | `midfreq` | 900 (1) |
| | `midq` | 0,9 (1) |
| | `highfreq` | 8000 (1) |
| `mseq` | `midlow`/`midhigh`/`sidelow`/`sidehigh`/`middyn`/`sidedyn`/`msolo`/`ssolo`/`mmute` | 0 (je 1) |
| | `width` | 100 (1) |
| | `smute` | 0/1 (2) |
| `shaper` | `sPre`/`sPost`/`sLine` | 1 (je 1) |
| | `shape` | 0 … 2 (2) |
| | `amount` | 0 … 100 (5) |
| `trans` | `amount` | 100 (1) |
| | `attack` | 0 … 100 (22) |
| | `duck` | 0 … 63 (12) |
| | `speed` | 47 … 180 (19) |
| `reverb` | `wet` | 35 … 100 (4) |
| | `stereo` | 0 … 100 (6) |
| | `tone` | 3 … 100 (13) |
| | `speed` | 0 … 12 (2) |
| `final` | `boost` | 0,35 … 12 (15) |
| | `freq` | 36 … 96 (8) |
| | `clip` | 0 … 37 (9) |
| | `upward` | 20 (1) |
| | `monoF` | 60 … 181 (7) |
| | `shelfOn`/`monoOn` | 1 (je 1) |
| | `clipOn` | 0/1 (2) |
| `ott` | `depth` | 8,5 … 41 (8) |
| | `time` | 100 … 117 (2) |
| | `x1` | 120 … 323 (2) |
| | `x2` | 2500 (1) |
| | `gl` | 0 … 7,6 (3) |
| | `gm` | −2,8 … 7,6 (8) |
| | `gh` | 0 (1) |
| | `linear` | 0 (1) |

Die Wertebereiche der Module müssen **so gewählt werden, dass alle beobachteten Werte
hineinpassen** (Preset-Import darf nicht klemmen). Größere zulässige Bereiche sind
unproblematisch; zu enge führen zu hörbaren Preset-Abweichungen.

### 7.3 Platzhalter `PassThroughModule`

`PassThroughModule(FxType, int reportedLatencySamples = 0)`

* klanglich neutral, genau ein Parameter: `amount` 0…400 % (Default 100 %).
  `100 %` = bitidentisches Durchreichen; andere Werte skalieren nur den Pegel
  (`amount/100`). Dadurch lassen sich MIX-Blende und OUT GAIN im Test nachweisen,
  ohne DSP zu benötigen.
* `reportedLatencySamples` meldet eine Latenz, **ohne** zu verzögern — damit ist die
  Latenzlogik der Kette (Ringlinie, Bypass-Erhalt, PDC-Summe) ohne echten DSP prüfbar.

### 7.4 Phase-2-Module (`ott`, `reverb`, `dynEq`)

Drei Module integrieren Funktionen externer Plugins. Sie nutzen dieselbe
`FxModule`-Schnittstelle, haben aber zusätzliche Attribute, die nur im
Snapshot (`FxModuleState::data`) stehen und nicht über die 14 Proxy-Parameter
automatisierbar sind:

| Modul | Typ-String | Spec |
|---|---|---|
| Multiband (Xfer OTT) | `ott` | `docs/specs/fx-multiband.md` |
| Convolver (Convology XT) | `reverb` | `docs/specs/fx-convolver.md` |
| Dynamic EQ (TDR Nova) | `dynEq` | `docs/specs/fx-dynamic-eq.md` |

Regeln:

* Die **Referenz-Attribute des jeweiligen Originaltyps behalten ihre
  Bridge-Positionen** (`p0…p7` bei `ott`, `p0…p3` bei `reverb`); zusätzliche
  Attribute folgen danach. `p12`/`p13` bleiben MIX/OUTGAIN.
* `setChain()` ruft nach dem Übernehmen der Skalar-Attribute jetzt
  `module->setState (state.data)` auf. Damit erreichen Bänder, Kurven,
  IR-Pfade und Envelopen erstmals die Module (schließt O5/O6 für die neuen
  Module und für den Equalizer).
* Der Convolver bearbeitet IR-Änderungen auf dem Message-Thread und
  synchronisiert sie über `prepare()` in die Faltung; der Audio-Thread
  allokiert weiterhin nicht.

---

## 8. Getestete Eigenschaften (`tests/FxChainTests.cpp`)

| Testfall | Prüft |
|---|---|
| Drei Module OUT GAIN 0,5 → 0,125 | Multiplikation der Kettenverstärkung |
| `setChain` vor `prepare` | Reihenfolge-Unabhängigkeit, kein Processing ohne `prepare` |
| Abschaltetes Modul | `enabled == false` überspringt DSP **und** OUT GAIN |
| Ketten-Bypass | `bypassAll` rechnet kein Modul, live umschaltbar |
| MIX | `0` → trocken, `1` → nass, `0,5` → lineare Blende |
| Unbekannter Typ | Platz bleibt (`nullptr`), kein Absturz, Warnung, Display ungültig |
| Modulattribute | `data`-Attribute landen geklemmt im Modul |
| Latenz | Summe (4+8=12), Bypass hält Latenz (Impuls bei 8), Modul selbst verzögert nicht |
| Display | `pre`/`post`-Pegel, Kanäle, Fenster (128 bzw. 512), `valid`-Zustände |
| Typ-Kennungen | Hin-/Rückabbildung aller zehn Typen, unbekannt → `nullopt` |

---

## 9. Offene Punkte

| # | Punkt | Warum wichtig | Vorschlag / Ticket |
|---|---|---|---|
| **O1** | Parameternamen von `filt` sind **nicht** aus dem XML belegt (`CUTOFF`, `RESO`, `DRIVE`, `TYPE`, `SLOPE`, `POLARITY` laut Handbuch) | Preset-Kompatibilität, Proxy-Parameter | B-60: Namen/Reihenfolge am Original verifizieren, dann hier nachtragen |
| **O2** | ~~Lookahead-Länge des Finalizer-Ceilings (Vorschlag 1,5 ms) und ein „latency-free“-Modus~~ **entschieden**: 1,5 ms Lookahead (`ceil (0.0015 · sampleRate)` = 72 @ 48 kHz, 144 @ 96 kHz), gemeldet über `latencySamples()`; der Prozessor summiert sie per `setLatencySamples()` | Kick-Verschiebung im Raster, Klang | erledigt mit B-67 (Phase 3) |
| **O3** | Equal-Power-Blende des Convolvers liegt **im Modul** (`wet` gegen Direktsignal); die Ketten-MIX bleibt linear | Mischverhalten bei 50 % | B-66: Equal-Power im Modul, Kette unverändert lassen |
| **O4** | `mix`/`outGain` springen pro Block (kein Rampen) → Zipper bei Automation | Hörbarkeit bei Automation | Nachrüsten: `applyGainRamp` über den Block (kleine Änderung, additiv) |
| **O5** | ~~`FxModuleState::data` enthält nur Attribute; Kind-Elemente erreichen das Modul nicht~~ **behoben**: `setChain()` ruft `module->setState (state.data)` (Phase 2) | EQ-Bänder, Kurven, IR-Pfade sind Teil der Module | erledigt für alle zehn Typen (`eq`, `mseq`, `shaper`, `ott`, `reverb`, `dynEq` u. a.) |
| **O6** | ~~Zustandsübertragung Modul↔Zustand ist einseitig~~ **behoben**: `PluginProcessor::writeBackModuleStates()` merged `module->getState()` in `currentChain.modules[i].data` (skalare Attribute bleiben, gleichnamige Kinder werden ersetzt, `ir` übernommen) | Nach dem Bearbeiten in der UI muss der Zustand zurückgeschrieben werden | erledigt mit B-144 (Phase 3) |
| **O7** | ~~Kartenindex ↔ Kettenposition bei unbekannten Typen (`nullptr`)~~ **behoben**: die Karte zeigt Titel aus dem Typ-String und den Hinweis „Für diesen Typ gibt es kein Modul“; der Platz bleibt erhalten | UI darf keine Karte für ein unbekanntes Modul zeichnen | erledigt mit B-90/B-91 (Phase 3) |
| **O8** | `fxN_pM` → Modulparameter (F2) | Host-Automation je Modul | Nach Modulimplementierung: `params()`-Reihenfolge als Zuordnung festschreiben, `p12 = mix`, `p13` Reserve (Hypothese aus `03-RE-FINDINGS.md`, 3.2) |
| **O9** | Display zeigt `post` **nach** OUT GAIN, aber **vor** der Latenzkorrektur | Vergleich zweier Displays kann verschoben wirken | Mit B-92 klären, ob die UI latenzkompensiert zeichnet |
| **O10** | Keine Obergrenze für die Zahl der Module in der Kette | 76 Referenzketten haben max. 8 Module | UI-seitig begrenzen (z. B. 8), Engine bleibt offen |
