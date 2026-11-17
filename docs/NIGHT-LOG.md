# Nacht-Lauf 2026-11-17 — OpenCode-Protokoll

Neueste Zeile unten. Format: `HH:MM <Welleneintrag>`.

```
21:10 Uebergabe laut Plan. Branch main, letzter Commit 3967487 (docs: Nachplan).
21:12 Ausgangs-Gate geprueft: Debug-Build "no work to do", Debug-Suite gruen
      (317 Testfaelle / 540115 Assertions), Release-Build + Release-Suite gruen.
      CI-Lauf 44167242991 (docs: Nachplan) noch in Arbeit, Vorgaenger gruen.
      Ausgangslage ist damit gruen — kein Abbruch nach Plan Abschnitt 11.2.
21:14 Modell-Routing festgelegt: Builder = opencode/deepseek-v4.1-flash-free
      (kostenlos, DeepSeek-Familie), Pruefer = opencode/glm-5.3-flash-free
      (andere Familie, kostenlos). Tier B bei DSP: merge-gateway/qwen/qwen3.8-flash
      nur wenn Tier A zweimal scheitert.
21:16 W1 gestartet (4 Tasks parallel, Tier A).
22:40 W1 Gate gruen | Debug+Release gebaut, 333 Testfaelle / 540185 Assertions
      (vorher 317 / 540115, also +16 Faelle) | B-150, B-151, B-95, B-152 fertig.
      Abweichung dokumentiert: Modulkarten-420-px gilt nur ohne Struktur-Editor
      (dynEq = 588 px, arithmetisch unvermeidbar); Test prueft die Grenze an
      "dist" plus Seitenanzahl an dynEq.
```
