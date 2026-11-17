# KICK-Seite (P8)

Stand: 2026-11-17 · Welle 9 · Tickets **B-83** (Transport), **B-84** (Visualiser),
**B-86** (Slot-Karten), **B-154** (Slot-IO/Live-Regler), **B-155** (Rollen im
Zustand), **B-156** (Anzeige-Hüllen), **B-157** (Editor-Umbau).

Die KICK-Seite ist die Hauptseite des Plugins: Transport, Visualiser, zwölf
Slot-Karten, Statuszeile. Sie ist **keine zweite Wahrheit** — alle Werte kommen
aus `PluginProcessor` (`APVTS` + `extraState`), die Seite liest und schreibt nur.

```
+--------------------------------------------------------------+
| PLAY  LOOP  DEMO-KICK              BPM [150 BPM]  MASTER [-]  |  Transport (30 px)
+--------------------------------------------------------------+
| Visualiser: SHOW PARTS | SUM                                   |  150 px
+--------------------------------------------------------------+
| Viewport mit 12 Slot-Karten (3 Spalten x 4 Zeilen, scrollbar)  |  Rest
+--------------------------------------------------------------+
| Statuszeile: "3/12 Slots belegt | 150,0 BPM | Kick 400 ms ..." |  20 px
+--------------------------------------------------------------+
```

## 1. Dateien

| Datei | Inhalt |
|---|---|
| `src/ui/KickPage.{h,cpp}` | Seite, Transport, `KickVisualizer`, `ValueBar` |
| `src/ui/SlotCard.{h,cpp}` | Slot-Karte (Rollenband, Regler, Flags, Mini-Wellenform) |
| `src/ui/UiTheme.h` | Farben/Maße (unverändert genutzt) |
| `src/plugin/PluginEditor.{h,cpp}` | Rahmen, Seitenumschalter, hält beide Seiten |
| `tests/KickPageTests.cpp` | 11 Testfälle (Logik + Render) |

## 2. Transport

| Element | Wirkung | Weg in den Zustand |
|---|---|---|
| **PLAY** | `PluginProcessor::triggerKick()` | — (nur Renderer) |
| **LOOP** | Kick alle 1,5 s über den Seitentimer | `KickPage::loopEnabled` (nur UI) |
| **DEMO-KICK** | `startDemoKick()`: Punch/Rumble/Click aus der Library | schreibt Referenzen + Rollen + Startpegel in `extraState` |
| **BPM** (`ValueBar`) | `setBpm()`: Zustand, Renderer, Shaper-Hüllkurve | `extraState` (`bpm`) |
| **MASTER** (`ValueBar`) | APVTS `master_vol` | APVTS |
| **VIEW** | schaltet PARTS ⇄ SUM | nur Anzeige |

`ValueBar` ist ein `juce::Slider` in `LinearBar`-Optik mit eigener
Wertbeschriftung: `LinearBar` zeichnet bei JUCE nur den Balken, den Text
normalerweise eine Textbox. In einer 30 px hohen Leiste ist dafür kein Platz.

BPM wird auf 20…960 geklemmt (`KickRenderer::kMinBpm/kMaxBpm`). **FOLLOW DAW**
gibt es noch nicht (Ticket B-83b, offen) — das Tempo steht fest im Zustand.

## 3. Visualiser

Zwei Sichten, umschaltbar über `VIEW`:

* **SHOW PARTS** — eine Zeile je belegtem Slot auf der **Kick-Zeitachse**:
  `kickTime = position + (sampleTime - trimStart)`. Farbe nach Rolle
  (`Role` → `colourForRole()`), Slot-Nummer links.
* **SUM** — Summe aller hörbaren Slots (Mute/Solo wie im Renderer) mit
  Lautstärke, Position, Trims und Fades; 512 Stützpunkte, auf 0…1 normiert.

Grundlage sind die **Anzeige-Hüllen** aus `KickAssembler`
(`SampleEnvelope`: 256 Min/Max-Paare je Slot, beim Laden berechnet). Damit
zeichnet die Seite ohne Audio-Thread-Zugriff und ohne Sample-genaue Daten.

Beide Sichten teilen sich ein Viertelbeat-Raster (`beatLength * 0.25`) und die
Beschriftung `Kick-Länge in ms | BPM`. Die SUM-Kurve ist eine **Näherung aus den
Hüllen**, kein gerenderter Kick — sie zeigt Pegelverläufe, nicht das Ergebnis
der FX-Kette.

## 4. Slot-Karte

| Zeile | Inhalt |
|---|---|
| Rollenband | Rolle (`roleToString`, Farbe je Rolle), Dauer des Samples, Pegel-Strip (VOL in dB, −60…+10) |
| Namenszeile | Sample-Dateiname bzw. „leer – LOAD druecken“, `LOAD`, `X` |
| Flags | `MUTE`/`SOLO` (APVTS), `LOCK`/`P` (`extraState`) |
| Wellenform | Hülle, Trim-Fenster abgedunkelt, Fade-Marken grün |
| grosse Regler | `VOL`, `PITCH`, `STRETCH` |
| Detailregler | `TRIM START`, `TRIM END`, `FADE IN`, `FADE OUT` |

Die Regler liegen **direkt auf den APVTS-Parametern** (`lN_vol`, `lN_pitch`, …)
und sind normalisiert (0…1); der Anzeigetext kommt aus `ParamDesc::format()`
(dB, Halbtöne, %, „x“). Nach jeder Änderung ruft die Karte
`PluginProcessor::applySlotSettings(slot)` — der Renderer übernimmt Vol,
Position, Trims, Fades und Mute/Solo **ohne Rebuild** der Kette.

**Wichtig (Lehre aus dieser Runde):** `juce::Slider::setRange()` mit Intervall
rundet den normalisierten Wert. Mit `0.0001` wurde aus 0,85714286 → 0,8571 und
die dB-Anzeige zeigte „−0.00 dB“ statt „0.00 dB“. Die Slot- und
Transport-Regler laufen deshalb mit **Intervall 0** (`setRange (0.0, 1.0, 0.0)`).

**PITCH und STRETCH sind noch ohne DSP-Wirkung.** Die Werte werden gespeichert
und angezeigt; `SampleVoice` rechnet weiterhin mit `pitchRatio = stretchRatio = 1`
(Ticket **B-44**, offen). Die Karte täuscht nichts vor: der Status sagt nichts
über Klang, die Spez `voice-and-render.md` §9 beschreibt den Weg.

## 5. Prozessor-Schnittstelle

| Methode | Wirkung |
|---|---|
| `setSlotSample (slot, file, error)` | Referenz in `extraState` (relativ, wenn in der Library), `KickAssembler::loadOneSlot()` lädt **nur** diesen Slot, Hülle wird berechnet |
| `clearSlotSample (slot)` | Referenz löschen, Puffer + Hülle + Renderer-Slot leeren |
| `applySlotSettings (slot)` | `VoiceSettings` aus dem Zustand → `KickRenderer::setSlotSettings()` |
| `applyAllSlotSettings()` | dito für alle zwölf Slots (nach Presetwechsel) |
| `isSlotFilled`, `getNumFilledSlots` | Anzeige |
| `getSlotEnvelope (slot)` | Anzeige-Hülle |
| `getSlotVoiceSettings (slot)` | Diagnose/Test: was der Renderer wirklich spielt |
| `setSlotRole / setSlotLocked / setSlotKeyTracked` | Rollen/Locks/KEY-TRACK in `extraState` |
| `getBpm / setBpm` | Tempo in Zustand, Renderer, Shaper-Hüllkurve |
| `getKickLengthSeconds / getBeatLengthSeconds` | Anzeige/Visualiser |

`startDemoKick()` schreibt die Demo jetzt **in den Zustand** (Referenzen, Rollen
`punch/rumble/click`, Startpegel 0/−6/−12 dB über die Slot-Volumen-Parameter)
und lädt danach über `loadFromState()`. Vorher lud nur der Renderer — die
Slot-Karten waren leer, obwohl der Kick klang.

## 6. Threading

* Alles auf dem Message-Thread. `setSlotSample`, `clearSlotSample`,
  `applySlotSettings`, `setBpm` allokieren (Datei lesen, Hülle rechnen) und
  dürfen nicht parallel zu `renderNextBlock()` laufen — im Betrieb passiert das
  über den Message-Thread; die Renderer-Methoden selbst sind allokationsfrei.
* `KickRenderer::setSlotSettings()` ändert nur Einstellungen: eine laufende
  Voice rechnet ab dem nächsten Sample mit dem neuen Fenster; `mute`/`solo`
  greifen beim nächsten `noteOn()` (ein One-Shot wird nicht mitten im Klang
  stummgeschaltet).
* Die Seite refresht mit 8 Hz (`setRefreshRateHz`); der Timer liest den Zustand
  einmal und gibt ihn an alle Karten weiter (`KickState`-Kopie).

## 7. Tests (`tests/KickPageTests.cpp`)

| Testfall | Prüft |
|---|---|
| zwölf Slot-Karten | 12 Karten, Zugriffsschutz auf ungültige Indizes, Rendern |
| Sample laden | `setSlotSample` füllt Slot, Hülle, Zustand; Karte zeigt Namen |
| Slot leeren | Zustand, Hülle, Renderer leer; doppeltes Leeren unschädlich |
| Regler live | −6 dB → 0,501 linear; Fade 25 % → 0,0625 s; MUTE im Renderer |
| BPM | Tempo/Beat-/Kick-Länge, Zustand, Klemmung 20…960 |
| Rolle/Sperre/KEY-TRACK | alle drei im Zustand, `roleToString`, ungültige Indizes |
| Reglertexte | „0.00 dB“, „0.00 st“, „1.000 x“ (Rundungsrauschen) |
| Visualiser | PARTS/SUM rendern, MUTE ändert die Summe nicht negativ |
| Demo-Kick | Demo landet im Zustand, Karten stimmen mit Zustand überein (WARN ohne Library) |
| Editor | beide Seiten vorhanden und zeichenbar |

Renderings für die Sichtprüfung: `BFKA_RENDER_UI=1` schreibt
`C:\bfka-re\ui-kickpage-*.png` (`parts`, `sum`, `sum-muted`, `loaded`, `demo`)
und `ui-editor-kick.png` / `ui-editor-fx-page.png`.

**Fallstrick, der diese Runde gekostet hat:** `KickPage` legt die Kinder in
`resized()` aus. `setSize()` im Konstruktor läuft **vor** dem Anlegen der
Kinder, deshalb ruft der Konstruktor `resized()` am Ende selbst auf. Ohne das
sieht ein Test, der die Seite direkt rendert, eine leere Fläche.

## 8. Was hier bewusst **nicht** drin ist

| Fehlt | Ticket |
|---|---|
| Style-Sidebar (Styles, RANDOMIZE/CHAOS, FREE BUILD) | B-82, B-85, offen |
| Visualiser-Interaktion (Move/Trim/Fades per Maus, Rechtsklick-Menü) | B-85, offen |
| Regler-Rechtsklick (Reset/Typisieren/Copy), Shift-Fein, Tastatur | B-87/B-88, offen |
| Resize/UIScaler/HiDPI | B-89, offen |
| Pitch- und Stretch-DSP | B-44, offen |
| ALIGN/PHASE ALIGN, KEY-Chip, KEY TRACK (Auswertung) | P4/P12, offen |
| Drag & Drop von Dateien auf Karten | B-113, offen |

Der Rollenband-Text zeigt die Rolle an, sie ist (noch) nicht per UI änderbar —
sie kommt aus Style/Library (`Role`).
