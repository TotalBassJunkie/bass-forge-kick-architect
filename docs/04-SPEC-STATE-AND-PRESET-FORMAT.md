# 04 — Spezifikation: Zustand, Parameter und Preset-Formate

Diese Datei ist **verbindlich** für die Implementierung in `src/core`. Sie legt fest,
wie Bass Forge Kick Architect seinen Zustand hält, speichert und aus dem
Referenzformat importiert.

---

## 1. Grundregel: alles normalisiert

**Jeder kontinuierliche Parameter wird als `float` im Bereich 0…1 gespeichert.**

* Der Wert im Zustand, im Preset und im Host-Automatisierungs-Lane ist immer 0…1.
* Die Umrechnung in Anzeigewerte (dB, Halbtöne, Hz, x-Faktor) passiert ausschließlich
  in `bfka::ParamDesc` (`src/core/ParameterRegistry.h`).
* Grund: Das Referenzformat speichert ebenfalls normalisiert. Nur so sind importierte
  Presets 1:1 abbildbar, und Host-Automation verhält sich unabhängig von der Einheit.

Ausnahmen (diskrete Zustände) werden ebenfalls als 0/1 bzw. als Ganzzahl-skalierter
Wert geführt: `lN_mute`, `lN_solo` (0/1), `midi_mode` (0…3), `root_note` (0…127).

---

## 2. Parameter-Registry

`ParameterRegistry` (Singleton) ist die **einzige** Quelle der Wahrheit für IDs,
Bereiche, Defaults und Kurven. Reihenfolge der Einträge = Reihenfolge in
`KickState::values()` und damit in der Serialisierung.

| Block | Anzahl | IDs |
|---|---|---|
| Slots 0…11 | 12 × 19 = 228 | `l<N>_<suffix>` |
| Globals | 12 | `sat_amount`, `reverb_size`, `reverb_damp`, `reverb_mix`, `hollow_amt`, `max_amount`, `master_vol`, `midi_mode`, `root_note`, `gspare1`, `gspare2`, `gspare3` |
| FX-Proxy | 8 × 14 = 112 | `fx<N>_p<M>` |
| **Summe** | **352** | |

### 2.1 Slot-Suffixe (in dieser Reihenfolge)

| # | Suffix | Anzeige | Kurve | Arbeitsbereich |
|---|---|---|---|---|
| 0 | `vol` | Volume | dB | −60 … +10 dB |
| 1 | `pitch` | Pitch | Halbtöne | −24 … +24 st |
| 2 | `mute` | Mute | boolean | 0/1 |
| 3 | `solo` | Solo | boolean | 0/1 |
| 4 | `pos` | Position | Prozent | 0 … 100 % |
| 5 | `trimstart` | Trim Start | Prozent | 0 … 100 % |
| 6 | `trimend` | Trim End | Prozent | 0 … 100 % |
| 7 | `fadein` | Fade In | Prozent | 0 … 100 % |
| 8 | `fadeout` | Fade Out | Prozent | 0 … 100 % |
| 9 | `stretch` | Stretch | logarithmisch | 0,25× … 8× |
| 10 | `filter` | Filter | logarithmisch | 20 … 20 000 Hz |
| 11 | `filterq` | Filter Q | linear | 0,1 … 10 |
| 12 | `dlytime` | Delay Time | Prozent | 0 … 100 % |
| 13 | `dlymix` | Delay Mix | Prozent | 0 … 100 % |
| 14 | `spare1` | (Reserve) | Prozent | 0 … 100 % |
| 15 | `spare2` | (Reserve) | Prozent | 0 … 100 % |
| 16 | `ficurve` | Fade-In-Kurve | Prozent | 0 … 100 % |
| 17 | `fishape` | Fade-In-Form | linear | −1 … +1 |
| 18 | `foshape` | Fade-Out-Form | linear | −1 … +1 |

> **Arbeitsannahmen.** Die Bereiche und Kurven sind aus dem Referenzformat abgeleitet
> und in `docs/specs/parameter-normalisation.md` zu verifizieren (Ticket B-11/F1).
> `spare1/2` und `gspare1..3` werden gespeichert, aber nicht angezeigt (Preset-Treue).

### 2.2 FX-Proxy-Parameter

`fx<N>_p<M>` sind die 14 host-sichtbaren Stellvertreter des N-ten Moduls der Kette.
Arbeitshypothese (Zuordnung in `docs/specs/fx-param-bridge.md` zu belegen):

* `p0 … p11` = die ersten zwölf Modulparameter in fester Reihenfolge
* `p12` = `mix`
* `p13` = Reserve

---

## 3. Unser Zustandsbaum

`KickState::toValueTree()` erzeugt:

```xml
<BFKAKick schemaVersion="1" styleId="rawstyle" presetName="Mein Kick"
          bpm="150.0" stretchMode="0" bpmMode="0">
  <Slots>
    <Slot index="0" role="1" locked="0" keyTracked="0"
          relPath="Main - Punches/Rawstyle/Punch Heavy Raw 13.wav"
          absPath="" fileName="Punch Heavy Raw 13.wav" roleId="punch"/>
    …
  </Slots>
  <Params>
    <P id="l0_vol" v="0.699840486"/>
    …
  </Params>
  <FxChain bypassAll="0">
    <Module type="eq" on="1" mix="1.0" outGain="1.0">
      <Band kind="1" f="71.64" q="0.676" g="15.70" dyn="0.0" ch="0"/>
      …
    </Module>
    …
  </FxChain>
</BFKAKick>
```

Regeln:

* `schemaVersion` beginnt bei **1** und wird bei jeder strukturellen Änderung erhöht;
  `KickState::fromValueTree` muss alle älteren Versionen weiter lesen können.
* `role` ist der numerische `bfka::Role`-Wert; zusätzlich wird `roleId` als Text
  mitgeschrieben (menschenlesbar, importfreundlich).
* Fehlende Parameter werden mit ihrem Default aufgefüllt, nicht als Fehler behandelt.
* Unbekannte `<P>`-IDs werden ignoriert (Vorwärtskompatibilität).
* `bpm` ist `double`. `stretchMode`: 0 = keepPitch, 1 = repitch. `bpmMode`: 0 = syncBpm, 1 = native.

---

## 4. Unser Preset-Format `.bfkapreset`

* Wurzel: `<BFKAPreset version="1" name="…" author="…" styleId="…" created="ISO-8601">`
* Inhalt: derselbe Baum wie in Abschnitt 3, zusätzlich:
  * `<Samples>` — optional eingebettete Audiodaten für User-Presets
    (`<Sample slot="0" name="…" format="wav" bytes="…"/>` mit Base64-Payload,
    damit ein Preset unabhängig von der Library klingt — wie beim Referenzprodukt).
* Endung `.bfkapreset`, Wildcard `*.bfkapreset`.
* **Werk-Presets sind schreibgeschützt** (Ordner `Presets/Factory`), User-Presets
  liegen unter `Presets/User`.

### 4.1 Ablage

| Zweck | Pfad (Windows) |
|---|---|
| Content-Root (vom Nutzer wählbar) | `%PUBLIC%\Documents\BassForge\KickArchitect` |
| Werk-Presets | `<Content>/Presets/Factory/<style>/` |
| User-Presets | `%USERPROFILE%\Documents\BassForge\KickArchitect\Presets\` |
| Settings/Prefs | `%APPDATA%\BassForge\KickArchitect\prefs.json` |
| Cache (Analyse, Render) | `%APPDATA%\BassForge\KickArchitect\Cache\` |
| Exporte | `%USERPROFILE%\Documents\BassForge\KickArchitect\Exports\` |

---

## 5. Import des Referenzformats (`.itkbpreset`, schemaVersion 2)

Der Import ist **einmalig und lesend**: bestehende Presets des Referenzprodukts
sollen im Nachbau klingen. Er ist Ticket **B-13**.

### 5.1 Zuordnung Kick-Preset

| Referenz-Element | Ziel |
|---|---|
| `<ITKBPreset name= author= genreId= appVersion=>` | `presetName`, `author` (nur Info), `styleId` (Mapping s. u.), `appVersion` ignorieren |
| `<Slot index role>` + `<SampleRef relPath absPath fileName roleId>` | `<Slot index role locked keyTracked relPath …>`; `locked`/`keyTracked` haben im Referenzformat kein Gegenstück → Default `0` |
| `<Params><P id v>` | direkt in `<Params>` übernehmen, **Wert unverändert** (identische Normalisierung) |
| `<FxChain><fxSnap><FxChain>` | `<FxChain>` mit denselben `<Module>`-Elementen; Attribute 1:1 übernehmen |
| unbekannte `P`-IDs | ignorieren, aber im Import-Report zählen |

`genreId`-Mapping: `rumble_kicks`, `reverse_bass`, `industrial`, `psy`, `schranz`,
`rumble_kicks_clean`, `midintro_newstyle`→`newstyle`, `rawstyle`, `zaag`, sonst `free_build`.

### 5.2 Zuordnung FX-Chain (`FxChainPreset`)

Identisch: `bypassAll`, je Modul `type`, `on`, `mix`, `outGain` sowie alle weiteren
Attribute und Kind-Elemente (`<Band>`, `<WNode>`, `<Pt>`) unverändert übernehmen.
Unbekannte `type`-Werte werden als „nicht unterstütztes Modul" konserviert
(bleiben im Zustand erhalten, werden aber nicht verarbeitet) — so geht beim
Hin-und-Her-Konvertieren nichts verloren.

### 5.3 Qualitätsregeln für den Import

* Kein Werteverlust: unbekannte Attribute werden als Rohdaten mitgeschrieben.
* Import meldet strukturiert: gelesene Slots, unbekannte Parameter, unbekannte Module,
  fehlende Sample-Dateien.
* Fehlende Samples führen **nicht** zum Abbruch: Slot bleibt leer mit Hinweis
  (`fileName` bleibt im Zustand erhalten, damit der Nutzer die Datei nachliefern kann).

---

## 6. Verhältnis Host-Automation ↔ Presets

* Die 352 Parameter werden über `juce::AudioProcessorValueTreeState` als
  `AudioParameterFloat` mit Bereich 0…1 angelegt (IDs = Registry-IDs).
* Anzeige im Host: über `AudioParameterFloat::stringFromValue` möglich; die initiale
  Implementierung liefert neutrale Zahlenwerte, die Einheiten-Anzeige kommt mit P8.
* Ein Preset speichert **nicht** die Host-Parameter, sondern den `KickState`
  (der die Parameter enthält). Der Ladevorgang setzt alle Parameter über
  `setValueNotifyingHost`, damit die Host-Automation sauber nachzieht.

---

## 7. Was der Zustand bewusst *nicht* enthält

| Nicht im Zustand | Warum |
|---|---|
| Sample-Audiodaten (außer bei User-Presets, dort optional eingebettet) | Library-Dateien bleiben Dateien |
| Analyse-Cache (Key, Onset) | gehört in den Cache, ist reproduzierbar |
| UI-Geometrie (Fenstergröße, Scrollpositionen) | gehört in `prefs.json` |
| Undo-Historie | flüchtig |
| Lizenz-/Aktivierungsdaten | gibt es nicht (bewusste Abweichung A1) |
