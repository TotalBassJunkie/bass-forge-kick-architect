# Drittanbieter-Lizenzen

Die folgenden Komponenten werden verwendet bzw. zur Bauzeit herangezogen. Die Liste
wird mit jeder neuen Abhängigkeit fortgeschrieben.

## JUCE 8 (`JUCE`)

* **Zweck:** Plugin-Rahmen (VST3 + Standalone), Audio-Engine-Grundlagen, GUI, Datei-IO.
* **Version:** gepinnt auf `8.0.7` (identisch zur Referenz-Implementierung, damit sich
  Host-Verhalten und Audio-Callback-Semantik gleich anfühlen).
* **Lizenz:** JUCE wird von Raw Material Software / JUCE Ltd. unter mehreren Modellen
  angeboten (AGPLv3, GPLv3, kommerzielle bzw. „Personal"-Lizenz je nach
  Umsatzschwelle). **Diese Frage ist vor einem öffentlichen Release abschließend zu
  klären** und hier mit dem gewählten Modell zu dokumentieren.
* **Was das für dieses Repo heißt:** Der Quellcode hier steht unter GPLv3. Wer das
  Ergebnis kommerziell vertreiben will, muss die JUCE-Lizenzierung eigenständig
  klären (JUCE-Personal-Lizenz oder kommerzielle Lizenz).

## VST3 SDK

* **Zweck:** Plugin-Schnittstelle (über JUCE eingebunden).
* **Lizenz:** Dual — GPLv3 oder Steinberg VST3-Lizenz. Bei Auslieferung als
  Closed-Source-Build ist die Steinberg-Lizenz zu beachten (Namensnennung,
  Lizenztext im Installer).

## Catch2 (`Catch2`)

* **Zweck:** Unit-Tests.
* **Lizenz:** Boost Software License 1.0 (permissiv, keine Auflagen für Binaries).

## CMake / Ninja / MSVC

* **Zweck:** Build-Werkzeuge, keine Laufzeitabhängigkeit, keine Weitergabe.

---

## Bewusst nicht verwendet

| Komponente | Grund |
|---|---|
| **Rubber Band** | GPLv2 / kommerziell — für eine lizenzreine Auslieferung nicht geeignet; wird durch die eigene WSOLA-Engine ersetzt. |
| **SoundTouch / Elastique** | gleiche Begründung (Lizenz bzw. Kosten). |
| **Fremde Samples oder Presets** | werden nicht weitergegeben; das Plugin liest ausschließlich die Library des Nutzers. |
| **Fremde Marken, Logos, Schriftzüge** | werden nicht übernommen. |

---

## Laufzeit-Daten des Nutzers

Bass Forge Kick Architect speichert Einstellungen, Presets, Caches und Exporte in
Benutzerverzeichnissen. Es werden keine Nutzungsdaten an Server übertragen
(kein Telemetrie-, Aktivierungs- oder Update-Zwang). Diese Aussage ist bei jeder
Änderung des Netzwerkverhaltens hier zu aktualisieren.
