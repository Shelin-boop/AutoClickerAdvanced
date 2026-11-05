# Phase 1: Stand (2026-11-03)

## Version 0.2.2 (2026-11-03 16:25)

Rückmeldung Anwender: bei schnellen Positionsklicks sind eigene Mausklicks kaum möglich (0.2.1 verwarf die echten Zeigerereignisse während jedes Positionsklicks).

- Pointer-Grab jetzt synchron: der X-Server stellt die echten Ereignisse während des Positionsklicks zurück und liefert sie danach normal aus (verzögert um höchstens etwa 130 ms, nicht verloren).
- Neue Option „All clicks wait while you click“, Vorgabe an, Nachlauf 500 ms (0 ms bis 5000 ms): solange eine echte Maustaste gedrückt ist und danach, klickt keine Zeile.
- Simulation (P1 alle 150 ms, Anwender klickt 10-mal und bewegt die Maus): 0.2.1: 0 von 10 eigenen Klicks im Spiel; 0.2.2 nur Zurückstellen: 6 von 10, 3 vermischt; 0.2.2 mit Vorrang: 10 von 10, 0 vermischt. Bewegungstest und Mischtest unverändert ohne Verluste.
- [Annahme] Unter Xwayland noch nicht auf dem Zielsystem geprüft.

## Version 0.2.1 (2026-11-03 16:12)

Rückmeldung Anwender zu 0.2.0: nur P1 aktiv, Warten bei Mausbewegung aus. Während die Maus bewegt wird, klickt es nicht mehr an P1, sondern an der Maus.

- Ursache: Das Spiel hat einen Zeiger. Die echten Bewegungsereignisse erreichen das Spiel während des Positionsklicks; das Spiel wertet Drücken und Loslassen an der echten Mausposition aus. Simulation 0.2.0: 2 von 14 am Ziel, 7 an der Maus, 5 verloren.
- Korrektur: Während eines Positionsklicks (etwa 130 ms) aktiver Pointer-Grab auf dem Spielfenster mit leerer Ereignismaske; die echten Zeigerereignisse werden verworfen, XSendEvent ist davon nicht betroffen. Da der Zeiger bereits im Grab-Fenster ist, entstehen keine Enter/Leave-Ereignisse. Option „Hold the mouse off the game during a position click“, Vorgabe an. In dieser Zeit sieht das Spiel eigene Bewegungen und Klicks des Anwenders nicht.
- Simulation 0.2.1: 14 von 14 am Ziel bei bewegter Maus, bei 16 ms und 33 ms Bildzeit; Mischtest unverändert ohne Verluste; Ziehen mit gedrückter Taste ohne Hänger.
- [Annahme] Unter Xwayland noch nicht auf dem Zielsystem geprüft; schlägt der Grab fehl, meldet der Status das.

## Version 0.2.0 (2026-11-03 15:57)

Anweisung Auftraggeber: eigenes Intervall je Zeile (nur ms, 50 bis 60 000 000), nacheinander ausgeführt; gemeinsame Schrittdauer entfernt. Rückfrage geklärt: „eigenes Intervall je Zeile“, gleichzeitig fällige Zeilen nacheinander. Lastenheft auf v1.1 angepasst.

- Ablauf: Nach dem Start ist jede Zeile einmal fällig; es klickt die am längsten fällige, bei Gleichstand die obere. Nächster Termin = geplanter Termin + Intervall (Rate bleibt erhalten), bei Rückstand um ein ganzes Intervall ab jetzt.
- Zwischen zwei Klicks mindestens die Wartezeit (sonst sah das Spiel in der Simulation das Loslassen eines Mausklicks am Ziel des nächsten Positionsklicks: 17 von 104 verloren).
- Wartezeit-Vorgabe 40 ms (deckt 25 Bilder/s ab); Konfigurationen der Version 0.1.x mit 30 ms werden auf 40 ms gesetzt, die gemeinsame Schrittdauer wird zum Intervall jeder Zeile.
- „Repeat (Zyklen)“ ersetzt durch „Stop after N clicks“ (alle Zeilen, wie im Original).
- Simulation (bildweise auswertendes Testprogramm, etwa 10 s, M 100 ms, P1 500 ms, P2 1000 ms, P3 2000 ms): P1 19, P2 10, P3 5 Klicks am Ziel, M 40 Klicks an der Maus, 0 verloren; bei 16 ms und bei 33 ms Bildzeit gleich.

## Version 0.1.1 (2026-11-03 15:45)

Rückmeldung Anwender zu 0.1.0 (500 ms, nur P1 bis P3): Klicks kamen nicht immer korrekt im Spiel an, zusätzlich entstand ein Klick an der manuellen Maus.

- Ursache [Annahme, durch Simulation gestützt]: 0.1.0 sendete Bewegung und Drücken sowie Loslassen und Zurückbewegen jeweils ohne Pause. Ein Spiel, das Eingaben einmal pro Bild auswertet, sieht das Loslassen dann an der echten Mausposition. Simulation mit bildweise auswertendem Testprogramm: 0.1.0 bei 16 ms Bildzeit 1 von 7 Klicks korrekt, 6 an der Maus; bei 33 ms 0 von 7.
- Korrektur: Bewegen, 30 ms, Drücken, 50 ms, Loslassen, 30 ms, Zurückbewegen (Timing aus Probe 0.2, dort 4 von 4 erfolgreich). Wartezeit in den Einstellungen 0 ms bis 200 ms. Simulation 0.1.1: 6 von 6 korrekt bei 16, 25 und 33 ms Bildzeit.
- Bewegt der Anwender die Maus während eines Positionsklicks, kann das Spiel den Klick an der Maus sehen (Simulation: 4 von 7). Neue Option „Also pause positions 1 to 3 while the mouse moves“, Vorgabe an (Simulation: keine Fehlklicks).
- Ein Positionsklick dauert mit den Vorgaben etwa 110 ms; kürzere Schrittdauern werden dadurch für P-Schritte nicht erreicht.

Version 0.1.0 (Grundstand) ausgeliefert als `XClicker_Advanced-0.1.0-x86_64.AppImage` und Quellarchiv `xclicker-advanced-0.1.0-src.tar.gz` (C, GTK3, Makefile, `pkg/build-appimage.sh`).

## Umsetzung gegen das Lastenheft v1.0

| ID | Stand |
|---|---|
| F-01, F-02 | umgesetzt: Folge M, P1, P2, P3; ausgeschaltete Schritte ohne Wartezeit übersprungen (Test virtueller Bildschirm: 3 Schritte à 50 ms = 20 Klicks/s) |
| F-03 | umgesetzt: Safe Mode mindestens 100 ms, sonst 50 ms; Anhebung wird im Status gemeldet |
| F-04, F-05 | Phase 2 (Reaktionen) |
| F-06 | umgesetzt: Knopf und Hotkey, Modus Normal/Hold, Wiederholungen, Zufallsabweichung |
| F-10 | umgesetzt: je Schritt Maustaste, Einfach/Doppel, Haltezeit (Vorgabe 40 ms, zufällig 20 ms bis Wert) |
| F-11, F-12 | umgesetzt: Klick an der Mausposition, Pause bei Bewegung > 30 px, Fortsetzung nach 300 ms Ruhe (Test virtueller Bildschirm bestanden) |
| F-13, F-14 | umgesetzt: XSendEvent an das Fenster unter der Position, Zeiger unverändert |
| F-15 | umgesetzt: Eingabe oder „Get“ mit Banner und Ring, Bestätigung mit dem Start/Stop-Hotkey oder Klick, Esc bricht ab |
| F-16 | umgesetzt: Doppelklick-Abstand 20 ms bis 500 ms, Vorgabe 150 ms |
| F-30 | teilweise: Ringe für P1 bis P3, sichtbar solange das Fenster aktiv ist; Rahmen folgen in Phase 2 |
| F-31, F-32 | umgesetzt: englische GTK3-Oberfläche, Speicherung in `~/.config/xclicker-advanced/xclicker-advanced.conf` |
| F-33 | teilweise: Zyklus, Klickzahl, letzter Schritt, Pause-Hinweis; Reaktionsdaten folgen in Phase 2 |
| F-40 bis F-44 | umgesetzt: AppImage x86_64, Startmenü-Eintrag bei jedem Start (Pfad wird nachgeführt), Schalter in den Einstellungen, Name und Icon, GTK3 vom System |
| F-60 | vorbereitet: LICENSE, NOTICE.md mit Änderungsvermerk, README, Quellarchiv |

## Offen

- Abnahme auf dem Zielsystem (Kubuntu 26.04, Plasma 6.6.6, Wayland) steht aus.
- Phase 2: Reaktionen R1 bis R3.
