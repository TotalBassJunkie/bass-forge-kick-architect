# Spec — Sample-Library-Index und Analyse-Grundlagen

**Zugehörige Tickets:** B-20 (Content-Pfad), B-21 (WAV-Scanner), B-22 (BPM-/Tonart-Parser),
Vorarbeit für B-23 … B-27
**Code:** `src/library/SampleLibrary.h` / `src/library/SampleLibrary.cpp`
**Tests:** `tests/SampleLibraryFileNameTests.cpp`
**Stand:** 2026-11-17

---

## 1. Zweck und Abgrenzung

Der Index ist die **lesende Bestandsaufnahme** der installierten Content-Library. Er
beantwortet für jede WAV-Datei: *Wo liegt sie, zu welchem Pool gehört sie, welche Rolle
hat dieser Pool, welches Tempo und welchen Grundton kodiert der Dateiname, und wie lang
ist die Datei?*

Nicht Teil dieses Tickets:

* keine Analyse des Audiosignals (Onset, Key-Detection, Ähnlichkeit) → B-23/B-24/B-25,
* kein Cache-Schreiben (Format ist hier nur *geplant*) → B-26,
* kein Hintergrund-Thread/Fortschrittsanzeige → B-27,
* kein Einlesen der Original-Analysedateien (`Analysis/analysis.pack`, `.edm`).

Die Library wird **nur gelesen**. Es wird nichts in den Content-Root geschrieben —
insbesondere nicht in den Teknovault-Ordner unter `C:\Users\Public\Documents\...`.

---

## 2. Ordner → Pool → Rolle

Quelle der Wahrheit ist `genres.json` des Referenzprodukts (schemaVersion 3,
Content-Revision R33); die Ordnernamen wurden gegen die Platte verifiziert.
Die Tabelle steht als `SampleLibrary::poolDefinitions()` im Code.

| Ordner (relativ zum Root) | Pool-ID | Rolle | Tonality |
|---|---|---|---|
| `Main - Punches/Clean EDM` | PUN-EDM | `punch` | neutral |
| `Main - Punches/Industrial` | PUN-IND | `punch` | neutral |
| `Main - Punches/Industrial (Extended)` | PUN-IXT | `punch` | atonal |
| `Main - Punches/Newstyle` | PUN-NEW | `punch` | neutral |
| `Main - Punches/Rawstyle` | PUN-RAW | `punch` | neutral |
| `Main - Punches/Techno` | PUN-TEC | `punch` | neutral |
| `Main - Punches/Techno (Heavy)` | PUN-TCH | `punch` | neutral |
| `Main - Punches/Zaag` | PUN-ZAG | `punch` | neutral |
| `Main - Crunches/Industrial Heavy` | RMB-CIH | **`rumble`** | atonal |
| `Main - Crunches/Newstyle & Midintro (Melodic)` | CRN-NEW | `crunch` | melodisch |
| `Main - Crunches/Psy Style (Melodic)` | CRN-PSY | `crunch` | melodisch |
| `Main - Crunches/Rawstyle (Melodic)` | CRN-RAW | `crunch` | melodisch |
| `Main - Crunches/Reverse Bass (Melodic)` | CRN-REV | `crunch` | melodisch |
| `Main - Crunches/Zaag (Melodic)` | CRN-ZAG | `crunch` | melodisch |
| `Main - Rumbles/Rumble (Rawstyle)` | RMB-RAW | `rumble` | atonal |
| `Main - Rumbles/Rumbles (Bigroom)` | RMB-BIG | `rumble` | atonal |
| `Main - Rumbles/Rumbles (Techno)` | RMB-TEC | `rumble` | atonal |
| `Main - Rumbles/Schranz` | RMB-SCZ | `rumble` | atonal |
| `Transients (Clicks)` | CLK-STD | `click` | neutral |
| `Transients (Clicks) [Zaag]` | CLK-ZAG | `click` | neutral |
| `Transients (Percs)` | PRC-GAB | `punchEnhancer` | neutral |
| `Transients (Punch Top)` | PTP-TEC | `punchEnhancer` | neutral |
| `Transients (Newstyle Tops)` | TOP-NEW | `top` | neutral |
| `Enhancers (Rumbles)` | ENR-TEC | `rumbleEnhancer` | atonal |
| `Enhancers (Schranz)` | ENS-SCZ | `schranzEnhancer` | atonal |
| `Enhancers (Tops)` | ENT-TOP | `top` | neutral |
| `Enhancers (Punch Reverbs)/Reverbs (…)<br>` | PRV-IND/NEW/RAW | `punchEnhancer` | neutral |
| `Impulses/<Kategorie>` | – | `none` | – |

Zwei Punkte, die man leicht falsch abbildet:

1. **`Main - Crunches/Industrial Heavy` ist eine RUMBLE**, keine Crunch — der Ordner
   liegt nur unter „Crunches". Die Regel „Ordner beginnt mit `Main - Crunches` ⇒ crunch"
   wäre hier falsch.
2. **`Enhancers (Tops)` ist `top`**, nicht `punchEnhancer`. Umgekehrt sind
   `Transients (Percs)` und `Transients (Punch Top)` **`punchEnhancer`**, obwohl sie im
   Rollenband der UI als PUNCH gelesen werden (Bestandsaufnahme 5.7).

`Impulses/<Kategorie>` (Clicks & Snaps, Reverbs, Rumble Tails, Sub Drops, Textures,
Tonal Bodies) sind Faltungs-IRs für den Convolver — indexiert, aber **kein Kick-Pool**.
Pfade in FX-Ketten sind relativ zu `Impulses` (`ir="Reverbs/Plate Dark.wav"`).

### 2.1 Präfix-Matching

* Die Tabelle wird über den **relativ zum Root** liegenden Ordnerpfad ausgewertet
  (Trenner `/`, `\` wird vorher normalisiert).
* Vergleich ist **case-insensitiv**.
* Es gewinnt der **längste** passende Präfix, und der Präfix muss auf einer
  **Segmentgrenze** enden: `Main - Punches` passt auf `Main - Punches/Zaag`, aber
  `Main - Crunches/Industrial` **nicht** auf `Main - Crunches/Industrial Heavy`.
* Drei Auffangzeilen (`Main - Punches`, `Main - Crunches`, `Main - Rumbles`) fangen
  neue bzw. expansionsbedingte Unterordner auf, ohne dass eine Regel gepflegt werden muss.
* Unbekannte Ordner ⇒ `Role::none` + Warnung (siehe §9). Die Ordnerliste ist gedeckelt,
  damit ein falsch gesetzter Root nicht 200 Warnungen erzeugt.

---

## 3. Tempo-Regel (BPM aus dem Dateinamen)

**Grundsatz:** Das Tempo ist die **letzte Zahl** im Dateinamen — aber nur, wenn der Name
überhaupt einem Tempo-Träger folgt.

### 3.1 Regel

1. Ordneranteil und Endung entfernen. Die Endung fällt nur weg, wenn der Teil nach dem
   letzten Punkt **keine Ziffer** enthält (sonst würde `Punch 150.bpm` die 150 verlieren).
2. Alle zusammenhängenden Zahlengruppen sammeln.
3. **Weniger als zwei Zahlengruppen ⇒ kein Tempo kodiert ⇒ Fallback 150.**
4. Sonst: letzte Zahl, falls sie im plausiblen Fenster **60 … 300** liegt, sonst Fallback 150.

Schritt 3 ist der entscheidende Punkt: In der echten Library heißen Dateien entweder
`<Pool> <Index> <BPM>` (z. B. `Punch Top 1 150.wav`) oder — bei Pools ohne Tempoangabe —
nur `<Pool> <Index>` (z. B. `Punch Clean 100.wav`). Bei reinen Indexnamen ist die einzige
Zahl eine laufende Nummer, kein Tempo.

### 3.2 Beispiele (alle getestet)

| Dateiname | Zahlen | Ergebnis |
|---|---|---|
| `Punch Zaag 12 A 150.wav` | 12, 150 | **150** |
| `Reverb Industrial 1 155.wav` | 1, 155 | **155** |
| `Crunch Industrial 101 160.wav` | 101, 160 | **160** |
| `Transient Click 1 150.wav` | 1, 150 | **150** |
| `Punch Top 3.wav` | 3 | **150** (Fallback) |
| `Enhancer Newstyle Top 10 E.wav` | 10 | **150** (Fallback) |
| `Punch Clean 100.wav` | 100 | **150** (Fallback) |

### 3.3 Verifizierter Fallstrick

Die naive Deutung „letzte Zahl ist das Tempo" liefert in der Referenz-Library
**253 falsche Tempi** (z. B. `Punch Clean 100.wav` → 100 BPM). Drei Dateien sind
besonders heimtückisch, weil Index und typisches Tempo zusammenfallen:

* `Main - Punches/Clean EDM/Punch Clean 155.wav`
* `Main - Punches/Techno/Punch Techno 155.wav`
* `Main - Punches/Techno/Punch Techno 160.wav`

Mit obiger Regel bleibt die Verteilung über alle **2.443** Dateien sauber:
**150 → 2.246**, **155 → 60**, **160 → 137**, sonst nichts.

> **Annahme, offen (O-1):** Für die drei genannten Dateien und für alle reinen
> Indexnamen setzen wir 150 an, weil der Name kein Tempo trägt (Handbuch-Fallback).
> Ob das Sample tatsächlich mit 155/160 BPM aufgenommen wurde, kann nur eine
> Tempomessung oder die Werksanalyse (`.edm`) sagen.

---

## 4. Tonart-Regel (Grundton aus dem Dateinamen)

### 4.1 Regel

1. Wie in §3.1: Ordneranteil und Endung entfernen.
2. In Wörter (Whitespace) zerlegen, Satzzeichen an den Token-Rändern abschneiden.
3. **Von hinten nach vorne** den ersten Token suchen, der ein Noten-Token ist
   (`[A-Ga-g]` mit optionalem `#`, `♯`, `b`, `♭`).
4. Kein Treffer ⇒ `keyNote = -1` (unbekannt; die Analyse in B-24 muss liefern).

Der Grund für die Rückwärtssuche: Die Tonart steht **hinter** dem Index und **vor** dem
Tempo (`Punch Zaag 12 A 150`), kann aber auch am Ende stehen, wenn kein Tempo kodiert ist
(`Enhancer Newstyle Top 10 E`).

### 4.2 Feste Oktave

Der Index speichert **nur die Tonklasse**, keine Oktavlage:

```
C = 0, C# = 1, D = 2, D# = 3, E = 4, F = 5,
F# = 6, G = 7, G# = 8, A = 9, A# = 10, B = 11
```

`-1` bedeutet „unbekannt". Ein Kreuz erhöht um 1, ein `b` senkt um 1 (modulo 12), also
`Db = C# = 1` und `Bb = A# = 10`. `Cb = 11` und `B# = 0` sind ebenfalls definiert.

Die Anzeige-/Spieloktave kommt erst beim Rendern dazu:
`SampleLibrary::withOctave (keyNote, octave) = keyNote + 12 * octave` (auf MIDI 0…127
begrenzt). `KEY TRACK OCTAVE` (Default C4 im Referenzprodukt) speist diesen Offset.

> **Annahme, offen (O-2):** Ob das Referenzprodukt C4 als MIDI-Note 60 oder als 48
> behandelt, ist nicht verifiziert. Der Index ist davon unabhängig, weil er nur die
> Tonklasse 0…11 hält; die Abbildung passiert ausschließlich in `withOctave()`.

### 4.3 Beispiele (alle getestet)

| Dateiname | Ergebnis |
|---|---|
| `Crunch Zaag 10 G# 150.wav` | **8** (G♯) |
| `Punch Zaag 12 A 150.wav` | **9** (A) |
| `Enhancer Newstyle Top 10 E.wav` | **4** (E) |
| `Crunch Rawstyle 11 B 150.wav` | **11** (B) |
| `Crunch Psy 1 Db 150.wav` | **1** (C♯) |
| `Transient Click 1 150.wav` | **-1** |
| `Crunch Rawstyle 33 150.wav` | **-1** (melodischer Pool, aber ohne Noten-Token) |

### 4.4 Verteilung in der Referenz-Library

363 von 2.443 Dateien (14,9 %) tragen ein Noten-Token:

| Note | Anzahl | | Note | Anzahl |
|---|---|---|---|---|
| C | 3 | | G | 47 |
| C# | 2 | | G# | 33 |
| D | 39 | | A | 40 |
| D# | 19 | | A# | 10 |
| E | 51 | | B | 2 |
| F | 73 | | | |
| F# | 44 | | | |

Beobachtungen:

* Die fünf melodischen Crunch-Pools tragen in **306 von 309** Dateien einen Grundton;
  die drei Ausnahmen (`Crunch Rawstyle 33/34/35 150.wav`) brauchen die Analyse.
* Auch Namen außerhalb der melodischen Pools tragen Töne:
  `Main - Punches/Newstyle` (22), `Main - Punches/Zaag` (15),
  `Transients (Newstyle Tops)` (20). Der Parser liefert sie mit; ob die Rolle sie nutzt,
  entscheidet die Style-Logik.
* In der Referenz-Library kommen **nur Kreuze** vor (kein `Db`, `Eb` …). Die
  b-Schreibweise ist trotzdem implementiert, weil Nutzer-Libraries sie verwenden.

---

## 5. Was der Index speichert — und was nicht

`SampleInfo` (unveränderlich nach dem Laden):

| Feld | Inhalt | Quelle |
|---|---|---|
| `file` | absoluter Dateiverweis | Dateisystem |
| `relPath` | Pfad relativ zum Root, Trenner `/` | Dateisystem |
| `fileName` | reiner Dateiname mit Endung | Dateisystem |
| `poolFolder` | Ordner relativ zum Root, Trenner `/` | Dateisystem |
| `role` | Rolle des Pools | Ordner-Tabelle (§2) |
| `bpm` | Tempo | Dateiname (§3), Fallback 150 |
| `keyNote` | Tonklasse 0…11 oder −1 | Dateiname (§4) |
| `lengthSeconds` | Länge in s | WAV-Header |
| `numChannels` | Kanäle | WAV-Header |
| `sampleRate` | Abtastrate in Hz | WAV-Header |

**Nicht** im Index:

| Nicht gespeichert | Warum / wo stattdessen |
|---|---|
| Audiodaten | liest erst die Engine (B-30) |
| Wellenform-Peaks | Visualiser (P7), eigener Cache |
| Onset/Trim-Vorschläge | B-23 |
| Key aus dem Signal (Konfidenz) | B-24 — der Dateiname ist nur die Erwartung |
| Ähnlichkeitsvektoren (MFCC/Chroma) | B-25 |
| Lautheit/Peak | Auto-Gain (B-34) misst beim Laden |
| Verzeichnis-Metadaten | Der Cache (§7) merkt sie sich für die Invalidierung |

`lengthSeconds`, `numChannels`, `sampleRate` kommen aus `AudioFormatManager::createReaderFor()`,
also aus dem Dateikopf — **es wird nichts dekodiert**. Ist der Kopf nicht lesbar, bleiben
die drei Felder 0 und die Datei landet in den Warnungen; sie wird **nicht** verworfen
(der Nutzer soll sehen, dass etwas mit der Datei nicht stimmt).

---

## 6. Content-Root-Auflösung

`defaultContentRoot()` nimmt den ersten **existierenden** Kandidaten:

| # | Kandidat | Zugriff |
|---|---|---|
| 1 | `%PUBLIC%\Documents\BassForge\KickArchitect` | lesen + schreiben (eigener Content) |
| 2 | `%PUBLIC%\Documents\Teknovault\The Teknovault Kick Builder` | **nur lesen** |
| 3 | `%USERPROFILE%\Documents\BassForge\KickArchitect` | lesen + schreiben |

Ermittelt wird das über `juce::File::getSpecialLocation()` (`commonDocumentsDirectory`,
`userDocumentsDirectory`) statt über Umgebungsvariablen — das ist unter Windows dieselbe
Auflösung, aber ohne Shell-Abhängigkeit. Existiert keiner der drei, kommt Kandidat 1
zurück (der Pfad, den der Nutzer anlegen soll). `contentRootCandidates()` liefert die
Liste für Einstellungs-UI und Diagnose.

Der gespeicherte Root ist der Root des letzten **erfolgreichen** Scans (`contentRoot()`).

---

## 7. Geplantes Cache-Format (Ticket B-26)

Der Index selbst wird nicht persistiert (er ist in Sekunden neu gebaut), wohl aber die
**Analyse** je Datei. Der Index-Cache ist die Vorstufe: er spart beim Start das Öffnen
von 2.443 Dateiköpfen.

### 7.1 Ablage

```
%APPDATA%\BassForge\KickArchitect\Cache\
    library-<sha1 des Root-Pfades>.bfli     Index-Cache
    analysis-<sha1 des Root-Pfades>.bfan    Analyse-Cache (B-26)
```

Pro Root eine Datei, damit zwei Libraries sich nicht gegenseitig invalidieren.
**Nie** im Content-Root — Kandidat 2 ist schreibgeschützt und fremder Content.

### 7.2 Binärlayout `BFLI` (Vorschlag, Version 1)

```
Header
  char[4]  "BFLI"
  uint32   version      = 1        (bei jeder Layout-Aenderung +1; aeltere Versionen
                                    werden verworfen, nicht migriert)
  uint32   entryCount
  uint64   rootPathHash            (FNV-1a ueber den normalisierten Root-Pfad)

Eintrag (entryCount mal, nach relPath sortiert)
  uint32   relPathLen, danach UTF-8-Bytes (ohne Nullterminator)
  int64    fileSize
  int64    lastModifiedMs           (File::getLastModificationTime().toMilliseconds())
  int32    role                     (bfka::Role als int)
  int32    keyNote                  (-1 = unbekannt)
  float    bpm
  int32    numChannels
  double   sampleRate
  double   lengthSeconds
```

Little Endian, keine Zeiger, keine Strings ausser `relPath`.
Angehängt wird ein `uint32 checksum` (CRC-32 über alles davor) — ein abgeschnittener
Cache soll als „unbrauchbar" erkannt werden, nicht als Wahrheit gelten.

### 7.3 Invalidierung

* **Datei-Ebene:** Ein Eintrag gilt nur, wenn `fileSize` **und** `lastModifiedMs`
  übereinstimmen. Sonst wird die Datei neu gelesen (Kopf öffnen, Name parsen).
  Das ist die Regel, die im Ticket gefordert ist: Größe + Datum.
* **Datei verschwunden:** Eintrag fällt raus (kein Fehler, keine Warnung — Bibliotheken
  werden umsortiert).
* **Neue Datei:** wird ergänzt.
* **Root hat sich geändert:** andere Cache-Datei (Hash im Namen), also automatisch kalt.
* **Parser-Version geändert:** `version` erhöhen ⇒ kompletter Neuaufbau. Wichtig, weil
  sich die Namensregeln (§3/§4) noch ändern können.
* **Kein mtime-Vertrauen:** Datum kann durch Kopieren/Entpacken verloren gehen. Deshalb
  wird der Cache nur als *Beschleuniger* benutzt; ein Cache-Treffer wird nicht als
  Beweis für den Dateiinhalt gewertet (die Engine liest die Datei ohnehin selbst).

### 7.4 Schreiben

Atomar: in eine `TemporaryFile` im Cache-Ordner schreiben, `flush`, dann
`File::replaceWith()` / `moveFileTo()`. Bei Abbruch bleibt die alte Datei heil.
Ein fehlgeschlagener Cache-Schreibvorgang ist **kein** Fehler des Scans (nur Log).

---

## 8. Nebenläufigkeit und Lebensdauer

* Der Index ist nach `loadFromRoot()` unveränderlich; Leser nehmen ein
  `std::shared_lock` auf ein `std::shared_mutex`, der Neuaufbau ein `std::unique_lock`.
* Der Aufbau läuft **transaktional**: gesammelt wird in einen lokalen Vektor, getauscht
  wird erst am Ende. Bei Abbruch (`shouldAbort()` ⇒ true) oder unlesbarem Root bleibt der
  alte Index stehen — die UI sieht nie einen halben Index.
* `samples()` liefert eine Referenz auf den Vektor. Sie bleibt gültig, bis ein neuer Scan
  **erfolgreich** war; `byPool()`/`byRole()` liefern Zeiger in denselben Speicher.
  Wer über einen Reload hinweg Zeiger hält, muss neu abfragen (dokumentiert im Header).
* Abgebrochen wird zwischen zwei Dateien — ein einzelner Dateikopf blockiert also nie
  länger als einen Öffnungsvorgang.
* Die Scan-Reihenfolge des Dateisystems ist nicht reproduzierbar; der Index wird nach
  `relPath` (case-insensitiv) sortiert, damit Tests und Anzeige stabil sind.

---

## 9. Warnungen (`LibraryScanResult::warnings`)

Je Kategorie höchstens 50 Einträge, danach eine Summenzeile — eine kaputte Library soll
die Meldungsliste nicht unbrauchbar machen.

| Meldung | Bedeutung |
|---|---|
| `Content-Root ist kein lesbarer Ordner: …` | Root fehlt/falsch ⇒ `loadFromRoot()` liefert false |
| `Scan abgebrochen; der vorhandene Index bleibt unveraendert.` | `shouldAbort` hat zugeschlagen |
| `Ordner ohne Pool-Zuordnung (Rolle none): …` | Ordner fehlt in der Tabelle (§2) |
| `WAV-Header nicht lesbar (Laenge/Rate unbekannt): …` | Datei bleibt im Index, Felder = 0 |
| `N weitere Warnungen unterdrueckt.` | Deckelung |
| `Keine WAV-Dateien unter … gefunden.` | falscher Root oder leere Library |

Der Scan gilt als **erfolgreich** (`return true`), solange der Root lesbar war und nicht
abgebrochen wurde — Warnungen sind kein Fehler.

---

## 10. Gemessene Ist-Werte (Referenz-Library, 2026-11-17)

| Größe | Wert |
|---|---|
| WAV-Dateien | 2.443 |
| Ordner (relativ, ohne Root) | 35 |
| Dateien mit Tempo 150 | 2.246 |
| Dateien mit Tempo 155 | 60 |
| Dateien mit Tempo 160 | 137 |
| Dateien mit Noten-Token | 363 (14,9 %) |
| melodische Crunch-Pools ohne Noten-Token | 3 (`Crunch Rawstyle 33/34/35`) |
| Impulsantworten (`Impulses/**`) | 75 |
| Ordner ohne Pool-Zuordnung | **0** |
| Kalter Scan (Debug-Build, sequenziell) | **1,43 s** für alle 2.443 Dateien |
| WAV-Header nicht lesbar | **0** |

Der komplette Scan inklusive Öffnen aller 2.443 Dateiköpfe dauert gemessen **1,43 s**
(Debug-Build, ein Thread, ohne Cache). Das Abnahmekriterium „< 60 s kalt" aus dem
Masterplan ist damit schon ohne Parallelisierung erfüllt (vgl. O-5).

Rollenverteilung über die 2.443 Dateien (Summe = 2.443):

| Rolle | Dateien |
|---|---|
| `punch` | 810 |
| `rumble` | 547 (davon 293 aus `Main - Crunches/Industrial Heavy`) |
| `crunch` | 309 |
| `punchEnhancer` | 219 (40 Percs + 60 Punch Top + 119 Punch Reverbs) |
| `click` | 216 |
| `schranzEnhancer` | 100 |
| `top` | 87 (47 Newstyle Tops + 40 Enhancer Tops) |
| `rumbleEnhancer` | 80 |
| `none` | 75 (nur `Impulses/**`) |

### 10.1 Verifikation

Der Code wurde mit dem Projekt-Toolchain (MSVC 14.51, `/std:c++20 /W4 /permissive-`)
gegen die echten JUCE-8.0.7-Header übersetzt — `SampleLibrary.cpp`, der Header als
eigene Übersetzungseinheit und die Catch2-Tests, alle **ohne Warnung**.

Zusätzlich wurde ein Wegwerf-Prüfprogramm gegen die echte Library gefahren (identische
Zusicherungen wie der Testkatalog plus Scan-Prüfungen): **64 Prüfungen, 0 Fehler**.
Gemessene Ergebnisse des Laufs:

```
Scan: ok=1  Dateien=2443  Warnungen=0  Dauer=1.43 s
BPM: 150=2246 155=60 160=137 andere=0 | mit Tonart=363 | ohne Header=0
byPool("Main - Punches/Zaag") = 73
byRole(click) = 216
erster Eintrag: Enhancers (Punch Reverbs)/Reverbs (Industrial)/Reverb Industrial 1 150.wav
                | Rolle=6 (punchEnhancer) | 150.00 BPM | Key=-1 | 0.800 s | 2 ch | 44100 Hz
```

Damit sind BPM-Verteilung, Rollen-Zuordnung, Sortierung, Header-Auswertung und die
Warnungsfreiheit gegen den echten Content belegt — nicht nur gegen die Regeln.

Namensmuster, die der Parser abdecken muss:

```
Punch Top 1 150.wav                 Punch Clean 100.wav            (nur Index)
Crunch Industrial 100 160.wav       Punch Heavy Raw 13.wav         (nur Index)
Crunch Zaag 10 G# 150.wav           Enhancer Newstyle Top 10 E.wav (Tonart, kein Tempo)
Crunch Reverse Bass 01 G# 150.wav   Transient Click Zaag 1.wav     (nur Index)
Reverb Industrial Raw 8 150.wav     Impulses/Reverbs/Plate Dark.wav
```

---

## 11. Integration (CMake, für den Lead)

```cmake
set(BFKA_LIBRARY_SOURCES
    src/library/SampleLibrary.cpp
    )

set(BFKA_HEADERS
    ...
    src/library/SampleLibrary.h
    )
```

Tests (`tests/CMakeLists.txt`):

```cmake
target_sources(BFKA_Tests PRIVATE
    ...
    SampleLibraryFileNameTests.cpp
    ../src/library/SampleLibrary.cpp
    )
```

`juce::AudioFormatManager` steckt in `juce_audio_formats`, das über `juce_audio_utils`
bereits in beiden Targets hängt — keine zusätzliche `target_link_libraries`-Zeile nötig.

---

## 12. Offene Punkte

| # | Punkt | Warum wichtig | Vorgehen |
|---|---|---|---|
| O-1 | Indexnummern im Tempo-Zahlenraum (`Punch Techno 160.wav`) | Tempo-Fallback vs. echtes Tempo | Tempomessung oder `.edm`-Werte prüfen; ggf. Pool-Whitelist „trägt Tempo" einführen |
| O-1b | Namen mit **einer einzigen** Zahl sind grundsätzlich mehrdeutig: `Punch Clean 155` (Index) vs. `Rumble 155` (Tempo) | Nutzer-Libraries benennen oft nur `… 150` | bewusst Fallback 150; eine Pool-Whitelist (Spalte „trägt Tempo" in `PoolDefinition`) löst es, sobald Bedarf besteht |
| O-2 | Oktavkonvention C4 = 60 vs. 48 | KEY TRACK OCTAVE klingt sonst eine Oktave falsch | Original mit bekanntem Preset vergleichen (P8) |
| O-3 | Tonart nur aus dem Dateinamen | Dateien ohne Token (3 Stück) und Nutzer-Samples brauchen die Analyse | B-24, Abgleich ≥ 95 % |
| O-4 | Kein Tempofeld für Impulses/User-Samples | Convolver-IRs haben kein Tempo | BPM dort ignorieren, `role == none` |
| O-5 | Paralleles Scannen / Fortschritt | gemessen 1,43 s kalt — Budget (< 60 s) ist erfüllt; Fortschritt braucht nur eine UI-Anzeige bei sehr langsamen Datenträgern | optional: B-27 (Thread + `shouldAbort` ist schon da) |
| O-6 | Cache noch nicht implementiert | Startzeit | B-26, Format in §7 |
| O-7 | Expansion-/OPS-Library | `BOTH LIBRARIES` im Referenzprodukt | Tabelle um Präfixe erweitern, sobald Content bekannt |
| O-8 | Melodisch-Flag nur heuristisch gepflegt | Randomizer-Regel „melodisch × atonal" (B-9) | `PoolDefinition::melodic` ist gesetzt; verbindliche Registry kommt mit dem StyleModel |
| O-9 | Repo-Tests laufen ohne Dateisystem | CI bleibt unabhängig von der installierten Library | Der Testkatalog prüft Parser + Tabelle; der Lauf gegen die echte Library wurde einmalig manuell verifiziert (§10.1). Optionaler Integrationstest mit synthetischer Library: B-28 |
| O-10 | Symlinks werden nicht verfolgt | `RangedDirectoryIterator` folgt standardmäßig nicht | bewusst so lassen (Endlosschleifen), bei Bedarf Opt-in |
