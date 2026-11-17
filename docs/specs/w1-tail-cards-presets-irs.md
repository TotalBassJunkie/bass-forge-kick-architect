# Welle 1 — Tail-Länge, Karten-Seiten, Preset-Einbettung, IR-Browser

Stand 2026-11-17 (Nacht-Lauf). Beschreibt vier Bausteine, die zusammen mit den
Tickets B-150, B-151, B-95 und B-152 eingeführt wurden.

## 1 · Tail-Länge (B-150)

Der Host fragt `AudioProcessor::getTailLengthSeconds()` und schneidet die Spur
sonst nach dem letzten Event ab. Der Wert ist jetzt berechnet:

```
tail = renderer.kickLengthSeconds() + fxChain.tailSeconds()     // Sekunden
tail = clamp (tail, 0.05, 60.0)
```

* `FxModule::tailSeconds()` ist virtuell mit Standard `0.0`. Nur Module, die
  einen echten Ausklang haben, überschreiben sie.
* `ConvolverModule::tailSeconds()` = `currentIrLength() / currentIrSampleRate()`
  (aufbereitete IR; ohne IR `0`).
* `FxChainProcessor::tailSeconds()` summiert alle Module. Die Kette ist seriell,
  deshalb ist die Summe die richtige Näherung. `nullptr`-Plätze (unbekannter
  Modultyp) zählen als 0.
* Die Methode läuft ohne Allokation und darf aus dem Audio-Thread kommen.

## 2 · Karten-Seiten (B-151)

Module mit vielen Parametern (`dynEq`) sprengten die Karte. Jetzt:

| Größe | Wert |
|---|---|
| `ModuleCard::paramsPerPage` | 12 |
| Spalten im Raster (`kColumns`) | 6 |
| Reglerreihen je Seite | höchstens 2 |

* `pageCount()` = `max (1, ceil (numParams / 12))`.
* `setPage (p)` klemmt auf `[0, pageCount()-1]`, setzt die Sichtbarkeit
  (`applyPageVisibility()`), aktualisiert `SEITE n/m` und legt neu aus.
* `PREV`/`NEXT` sind nur sichtbar, wenn es mehr als eine Seite gibt.
* `preferredHeight()` zählt nur die Regler der aktuellen Seite und addiert die
  Seitenzeile (18 + 4 px), wenn sie sichtbar ist.

**Bekannte Grenze:** Die Vorgabe „`preferredHeight() <= 420`" ist mit
Struktur-Editor nicht erreichbar. Ein `dynEq` bringt 216 px Struktur-Editor mit;
Kopf + MIX + Display + zwei Reglerreihen ergeben 588 px. Der Test prüft die
420-px-Grenze deshalb an einem Modul **ohne** Struktur-Editor (`dist`) und
zusätzlich die Seitenanzahl für `dynEq`. Wer die Grenze hart will, muss den
Struktur-Editor einklappbar machen — das ist eine eigene Entscheidung
(Geschmacksfrage, Abschnitt 8 des Plans).

## 3 · Samples im Preset (B-95)

```cpp
struct PresetWriteOptions
{
    bool        embedSamples     = false;
    juce::int64 maxEmbeddedBytes = 32 * 1024 * 1024;
};
```

Format unter der Wurzel `<BFKAPreset>`:

```xml
<Samples>
  <Sample slot="0" fileName="kick.wav" format="wav" bits="16" data="UklGR..."/>
</Samples>
```

* Das Base64 steht als Property `data`, nicht als Element-Text — ValueTree
  behandelt Properties verlustfrei und ohne Whitespace-Probleme.
* Quelle ist `absPath`, ersatzweise `relPath` (nur wenn er als Pfad auflösbar
  ist). Nicht lesbare Dateien werden übersprungen, kein Fehler.
* Summe der eingebetteten Bytes über dem Limit → `false` plus Fehlertext, und
  es wird **keine** Datei geschrieben (kein Teilpreset).
* Laden: fehlt die Datei am erwarteten Pfad, wird das WAV nach
  `Dokumente/BassForge/KickArchitect/SampleCache/` geschrieben und die
  Slot-Referenz auf `absPath` umgebogen. Fehler beim Entpacken lassen das Laden
  nicht scheitern.

## 4 · IR-Browser (B-152)

```cpp
struct ImpulseInfo { juce::String name; juce::File file; double seconds; bool factory; };

class ImpulseLibrary
{
public:
    static juce::File defaultDirectory();          // Dokumente/.../IRs
    void setUserDirectory (const juce::File&);
    void setFactoryDirectory (const juce::File&);
    int  scan();                                   // Anzahl der Einträge
    void refresh();
    const std::vector<ImpulseInfo>& entries() const noexcept;
    static bool readLengthSeconds (const juce::File&, double& secondsOut);
};
```

* Endungen: `wav`, `aif`, `aiff`, `flac`; rekursiv, alphabetisch,
  User-Einträge vor Factory-Einträgen, Pfad-Dedupe.
* `readLengthSeconds()` liest nur den Header (`AudioFormatReader`), nicht den
  ganzen Puffer.
* Fehlende Ordner liefern 0 Einträge; kein Log, kein Absturz.
* `ConvolverView` zeigt die Liste unter der IR-Zeile (70 px hoch,
  `preferredHeight()` 320). Doppelklick ruft `setImpulsePath()` und danach
  `writeBackModuleStates()` — derselbe Weg wie der Datei-Dialog.
