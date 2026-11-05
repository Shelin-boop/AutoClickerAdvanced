# Phase 0: Befunde (Stand 2026-11-03)

Quelle: xca-probe 0.1.0, Lauf 2026-11-03 14:25 auf dem Zielsystem. Antworten des Anwenders ab Schritt 4 sind laut Anwender unsicher; maßgeblich sind dort die protokollierten Ereignisse.

## Zielsystem [verifiziert, Bericht]

- Ubuntu 26.04.1 LTS, Kernel 7.0, glibc 2.43
- KDE Plasma 6.6.6, KWin 6.6.6, Wayland-Sitzung, Xwayland 24.1.10
- Ein Monitor DP-1, 3840 × 2160 px, Skalierung 2 (logisch 1920 × 1080)
- kwinrc [Xwayland] Scale=2: X11-Programme sehen die volle Auflösung 3840 × 2160 px
- Portale: RemoteDesktop v2 (Zeiger, Tastatur, Touch), ScreenCast v5, GlobalShortcuts v2
- /dev/uinput beschreibbar

## Spiel [verifiziert, Bericht]

- „[Name des Spiels].x86_64“, native Linux-Version (nicht Steam/Proton), XWayland-Fenster, Vollbild 3840 × 2160, keine Unterfenster

## Ergebnisse

| Prüfpunkt | Ergebnis | Bewertung |
|---|---|---|
| Zeigerposition über dem Spiel (XQueryPointer) | 161 Änderungen in 200 Proben | geeignet für Schritt M und Bewegungserkennung |
| Bildaufnahme Spielfenster XGetImage | 3840 × 2160 in 109 ms, Bildinhalt vorhanden | geeignet für das Muster-Foto |
| Bildaufnahme Spielfenster XShm | 400 × 400: Mittel 10 ms; voll: Mittel 37 ms | geeignet für Reaktionen |
| Bildaufnahme Root-Fenster | XGetImage Fehler 8, XShm nur schwarz | nicht verwendbar; immer das Spielfenster aufnehmen |
| F8 über XInput2 | kommt an, Spiel und Terminal im Vordergrund | Hotkey wie xclicker möglich |
| XSendEvent ans Testfenster | Klick an Zielposition, Zeiger unverändert | Weg A technisch möglich |
| XSendEvent ans Spiel (Probe 0.1, Runde 1) | Anwender: Spiel reagierte, an der Mausposition nichts | bestätigt durch Probe 0.2 |
| XSendEvent ans Spiel (Probe 0.2, 4 Runden, 2026-11-03 14:53) | Spiel reagierte in jeder Runde, an der Mausposition nichts, Zeiger unverändert | [verifiziert] geeignet für alle Klicks |
| Overlay (X11, override-redirect, 32 bit, durchklickbar) | Banner und Ringe über dem Vollbildspiel sichtbar | [verifiziert] geeignet für Rahmen-Anzeige (F-30) |
| XTest-Klick (mit/ohne XWarpPointer) | im Testfenster nicht angekommen | unzuverlässig, nicht verwenden |
| XTest-Bewegung absolut | Klick kam an der echten Mausposition an; Zeiger landete am unteren Bildrand | [Annahme] Koordinaten werden ohne Faktor 2 an KWin weitergereicht; nicht verwenden |

## Entscheidung (2026-11-03)

Weg A: alle Klicks (M, P1 bis P3, R1 bis R3) per XSendEvent direkt an das Fenster unter der Zielposition, ohne Zeigerbewegung. Bildaufnahme per XShm direkt vom Spielfenster. Anzeigen (Rahmen, Positionsmarken, Hinweise) als durchklickbare X11-Overlay-Fenster. Hotkeys per XInput2 (Raw-Events).
