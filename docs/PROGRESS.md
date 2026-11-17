# Fortschritt

Laufendes Protokoll. Neueste Einträge oben. Format:
`## JJJJ-MM-TT — <Welle/Phase>` mit erledigten Tickets (B-Nummern) und Notizen.

---

## 2026-11-17 — Kickoff: Bestandsaufnahme & Plan

**Erledigt**

* Vollständige Inventur der installierten Referenz (Programme, Content, Bibliothek,
  Presets, Analysedaten) — Hashes, Größen, Pfade, Zahlen.
* Handbuch (37 Seiten) ausgewertet, Signalfluss und Bedienlogik dokumentiert.
* Offene Datenformate gelesen und dokumentiert: Kick-Preset-XML, FX-Chain-XML,
  `genres.json` (25 Pools / 10 Styles), `prefs.json`, Analyse-Container.
* Binär-Statik: Framework (JUCE 8.0.7), VST3-SDK 3.7.12, Renderer, Imports,
  Modul-/Klassenlandkarte, Stretch-Algorithmusfamilie (WSOLA).
* Toolchain geprüft: MSVC 19.51 (VS BuildTools 2026), CMake 4.3.1, Ninja 1.13.2.

**Dokumente**

* [`01-BESTANDSAUFNAHME.md`](01-BESTANDSAUFNAHME.md) — Ist-Zustand, Datenformate,
  Parameterinventar, offene Fragen, Leitplanken.
* [`02-MASTERPLAN.md`](02-MASTERPLAN.md) — Phasen P0–P14, ~85 Tickets (B-00…B-135),
  Subagenten-Routing, Abnahmekriterien, Risiken, Meilensteine.
* [`03-RE-FINDINGS.md`](03-RE-FINDINGS.md) — Methodik und Befundlogbuch mit
  Quellenmarkierung.

**Entscheidungen**

* Produktname: **Bass Forge Kick Architect**, Code `BFKA`.
* Stack: C++20 + JUCE 8 (Version gepinnt), CMake, VST3 + Standalone, Windows zuerst.
* Kein Rubber Band (lizenzpflichtig) — eigene WSOLA-Engine.
* Kein Lizenzsystem, kein fremder Sample-Content im Repo.
* RE-Artefakte (String-/Klassenauszüge, Handbuchtext) bleiben lokal in `_private/`
  und werden **nicht** veröffentlicht.

**Nächste Schritte**

1. Repo-Gerüst + CI (P0, B-00…B-06).
2. Verträge schreiben: `KickState`, `ParameterRegistry`, `FxChain`, `Library`,
   `Engine`, `FxModule`-Interface.
3. Welle 1 parallel: Fundament, Preset-IO, Library-Index, Testdaten, Parametertabelle.

---

<!-- Vorlage für neue Einträge:

## JJJJ-MM-TT — Welle N

**Erledigt:** B-xx, B-yy; je ein Satz Ergebnis + Link auf PR/Commit.

**Gates:** Build grün, `ctest` grün, CI-Artefakt vorhanden.

**Blocker:** …

**Nächste Schritte:** …

-->
