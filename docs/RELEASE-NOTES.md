# Release-Notes - Bass Forge Kick Architect

<!-- B-134 -->

Stand: 2026-11-18. Grundlage sind `docs/PROGRESS.md` und `docs/specs/`.
Es sind hier nur Dinge beschrieben, die im Repo tatsaechlich umgesetzt und
getestet sind. Was offen ist, steht unter "Bekannte Grenzen" und
"Was noch menschlich geprueft werden muss".

---

## 0.1.0

### Was das Plugin ist

Bass Forge Kick Architect ist ein Kick-Builder fuer Techno und verwandte
Genres. Aus bis zu zwoelf geladenen Sample-Slots (Punch, Rumble, Click, Tops,
Crunch) wird ein Kick zusammengesetzt, per Pitch/Stretch und ALIGN in Form
gebracht und anschliessend durch ein frei bestueckbares FX-Rack geschickt.

Erhaeltlich als **VST3-Instrument** und als **Standalone-Anwendung** fuer
Windows x64.

### Funktionsumfang

* **Zwoelf Slots.** Jeder Slot hat Volume, Pitch, Stretch, Trim Start/End,
  Fade In/Out, Mute, Solo, Lock, Rolle und KEY-TRACK. Die Karten zeigen eine
  Mini-Wellenform mit Trim-Fenster und Fade-Marken.
* **KICK-Seite.** Visualiser mit SHOW PARTS (eine Zeile je belegtem Slot) und
  SUM, Viertelbeat-Raster, Anzeige "Kick-Laenge ms | BPM", Transport mit
  PLAY/LOOP, DEMO-KICK und BPM-Leiste.
* **FX-Rack mit zehn Modulen.** Filter (`filt`), Equalizer (`eq`),
  Distortion (`dist`), Transient Shaper (`trans`), Multiband-Kompressor
  (`ott`), Convolver (`reverb`), Dynamic EQ (`dynEq`), Mid/Side-EQ (`mseq`),
  Kick Shaper (`shaper`), Finalizer (`final`). Kette frei sortierbar,
  Module duplizierbar, MIX/OUT/ENABLE je Modul, BYPASS ALL, Latenzausgleich
  im Host.
* **Graph-Editoren** fuer Multiband, Convolver, Dynamic EQ, Mid/Side-EQ,
  Kick Shaper und Finalizer.
* **Pitch und Stretch.** REPITCH (Resampling) und KEEP PITCH (laengenneutral
  per Pitch-Shifter), Stretch mit eigener WSOLA-Implementierung, Vorab-Render-
  Cache fuer haeufig benutzte Einstellungen.
* **ALIGN.** Gap-Analyse zwischen Punch und Tails, MOVE CRUNCH mit
  TIGHT/NORMAL/GENTLE, PHASE ALIGN (Korrelation im Low-End, Nudge und
  Polaritaets-Flip), Report-Objekte fuer LAST PASS / RESULT LINE.
* **Styles, Randomizer, CHAOS.** Style-Sidebar mit Factory-Styles und eigenen
  Styles (`.bfkastyle`), FREE BUILD, ROLL mit deterministischem Seed,
  CHAOS-Schalter (deterministisch), gewichtete Pool-Wahl und die
  Ausschlussregel "melodisch x atonal".
* **Library-Analyse.** Library-Index aus Ordnern und Dateinamen, Onset-
  Erkennung, Tonart-Erkennung (Chroma + Krumhansl-Template), binaerer
  Analyse-Cache, Hintergrund-Job mit Abbruch.
* **Export.** Offline-Render des Kicks inklusive FX-Kette, WAV-Schreiben
  (16/24-bit), Namensbildung mit Kollisionsschutz.
* **Presets.** Kick- und Modul-Presets (`.bfkapreset`) mit Name, Autor, Style,
  Datum und App-Version. Samples lassen sich als 16-bit-WAV einbetten
  (Base64, Grenze 32 MB); fehlende Samples werden beim Laden in den
  Sample-Cache entpackt.
* **Undo/Redo** mit Schnappschuessen (50 Schritte), A/B-Vergleich,
  IR-Browser fuer den Convolver.

### Systemvoraussetzungen

* Windows 10 oder Windows 11, 64 Bit (x86_64).
* VST3-Host (getestet ist der Betrieb als Instrument mit MIDI-Eingang) oder
  die mitgelieferte Standalone-Anwendung.
* Fuer den Randomizer und die Library-Ansicht: eine vorhandene
  Sample-Library im erwarteten Ordnerschema. Ohne Library startet das Plugin,
  der Demo-Kick meldet dann eine Warnung.

### Installation

**Variante A - Setup (empfohlen)**

1. `BassForgeKickArchitect-Setup-0.1.0.exe` starten.
2. Der Assistent schlaegt fuer das VST3 den gemeinsamen Ordner
   `C:\Program Files\Common Files\VST3` vor; ein anderer Host-Ordner kann
   eingetragen werden.
3. Optional ein Desktop-Symbol anlegen, am Ende "Starten" anhaken.

Der Deinstaller entfernt Programm und VST3 wieder. Ausser den Eintraegen des
Deinstallers schreibt das Setup nichts in die Registry.

**Variante B - portables ZIP**

1. `BassForgeKickArchitect-0.1.0-portable-win64.zip` entpacken.
2. Den Ordner `VST3\Bass Forge Kick Architect.vst3` nach
   `C:\Program Files\Common Files\VST3` kopieren (oder in den VST3-Ordner des
   Hosts).
3. `Standalone\Bass Forge Kick Architect.exe` kann direkt gestartet werden,
   ohne Installation.

Die Datei `SHA256SUMS.txt` neben dem ZIP enthaelt die Pruefsummen.

### Bekannte Grenzen in 0.1.0

* **Sidechain-Hosttest offen.** Der externe Sidechain des Dynamic EQ ist im
  Unit-Test abgedeckt (Bus-Layout, Latenzausgleich), aber noch nicht in einer
  echten DAW mit Aux-Routing geprueft.
* **Hoerurteil offen.** Die Uebereinstimmung mit dem Referenzklang ist nur
  messtechnisch belegt, nicht durch einen Vergleich mit dem Ohr.
* **Modulkarten-Obergrenze 420 px gilt nur ohne Struktur-Editor.** Karten mit
  Graph-Editor sind bauartbedingt hoeher (z. B. `dynEq` mit zwei Reglerreihen
  588 px).
* Karten mit vielen Parametern werden seitenweise umgeblaettert
  (`PREV`/`NEXT`, hoechstens 12 Regler je Seite).
* Windows x64 ist die einzige gebaute Plattform. Es gibt kein VST2, kein AAX
  und keine macOS-Version.

### Was noch menschlich geprueft werden muss

Diese fuenf Punkte lassen sich nicht automatisieren (siehe
`docs/PLAN-OPENCODE-NACHT.md`, Abschnitt 8):

1. **Sidechain-Hosttest in einer DAW** (B-153 Rest). Braucht Ableton, Reaper
   oder Cubase mit Aux-Bus-Routing; im Unit-Test nicht abbildbar.
2. **Hoerurteil "klingt wie das Original"** (B-46, B-77, B-130). Ohr und
   Referenz-Plugin noetig; der Agent liefert nur Messwerte und Reports.
3. **Freigabe der Akzentfarben und Typografie** (B-80). Geschmacksfrage des
   Auftraggebers; Renderings liegen unter `C:\bfka-re\ui-*.png`.
4. **JUCE-Lizenzierung vor einem kommerziellen Release.** Rechtliche
   Entscheidung, nicht technisch loesbar.
5. **Loeschen oder Umbenennen von Nutzer-Presets.** Destruktiv; bleibt beim
   Bestaetigungsdialog und beim Menschen.
