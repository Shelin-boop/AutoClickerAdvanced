# Phase 2: Stand (2026-11-03)

## Version 0.3.0 (2026-11-03 16:45)

Auftrag: Phase 2, Reaktionen R1 bis R3 (Lastenheft v1.1, F-20 bis F-28, F-30, F-33).

### Bedienung

1. Spiel in den Vordergrund, F9 drücken (oder „Screenshot“ im Clicker, dann innerhalb des Countdowns von 5 s zum Spiel wechseln). Aufgenommen wird das Fenster unter dem Mauszeiger (XShm), weil das Root-Fenster unter Xwayland schwarz ist.
2. Editor: Werkzeug „Pattern“ (Quadrat 8 px bis 200 px), „Frame 1“ bis „Frame 3“, „Exclusion zone“ (höchstens 8 je Rahmen), „Delete“; Zoom „Fit“ und „100 %“; Vorschau des Musters mit verglichenem Kreis.
3. „Test detection“ zeigt alle Treffer auf dem Foto mit Ähnlichkeitswert (grün = würde reagieren, rot = darunter).
4. „Save“ speichert Rahmen, Sperrbereiche und, nur wenn geändert, `pattern.png` im Konfigurationsverzeichnis.
5. Reaction 1 bis 3 einschalten, Intervall je Zeile (Vorgabe 200 ms), Maustaste, Klickart, Haltezeit wie bei den Positionen; Start mit F8.

### Verfahren

- Vergleich: normierte Kreuzkorrelation über eine Kreismaske, zuerst grob auf verkleinertem Bild (Faktor 1, 2 oder 4, Vorgabe 2), dann um jeden Kandidaten fein und zuletzt auf voller Auflösung. Ein identisches Muster erreicht dadurch 1,00, der Klickpunkt liegt auf dem Mittelpunkt.
- Verglichen wird nur der innere Teil des Kreises (Vorgabe 80 % des Radius), damit der Hintergrund am Rand den Wert nicht drückt (vorher 0,41 bis 0,51 auf anderem Hintergrund, jetzt 1,00).
- Zusätzlich Farbprüfung über den Farbton unabhängig von der Helligkeit (Toleranz 25); verhindert Treffer auf gleich geformten, andersfarbigen Objekten.
- Ablauf je Prüfung: Ausschnitt des Rahmens aufnehmen, Treffer suchen, Treffer in Sperrbereichen verwerfen. Bei der ersten Prüfung nach dem Start werden alle sichtbaren Treffer als bekannt geführt, außer im Umkreis max(Sperrradius, Mustergröße/4) um die Aufnahmestelle. Bekannte Treffer werden innerhalb des Sperrradius nachgeführt und nach 2 s ohne Sichtung vergessen. Nach einem Klick gilt die Stelle für die Sperrzeit (Vorgabe 1000 ms) als gesperrt. Geklickt wird der stärkste neue Treffer.
- Klick wie ein Positionsklick (XSendEvent, Zeiger bleibt, Pointer-Grab, Vorrang für eigene Klicks).

### Neue Einstellungen (Reiter „Pattern reactions“)

| Einstellung | Vorgabe | Bereich |
|---|---|---|
| Similarity threshold | 0,85 | 0,50 bis 0,99 |
| Search on reduced image | 2 | 1, 2, 4 |
| Compared part of the circle | 80 % | 50 % bis 100 % |
| Color tolerance | 25 | 0 bis 100 (0 = nur gleiche Farbe) |
| Lock radius | 30 px | 0 px bis 200 px |
| Lock time after a click | 1000 ms | 0 ms bis 10 000 ms |
| Ignore patterns visible at the start | an | |
| Screenshot key | F9 | frei wählbar |
| Screenshot countdown | 5 s | |

„Compared part of the circle“ und „Color tolerance“ sind Ergänzungen gegenüber dem Lastenheft (Ausgleich für wechselnden Hintergrund und Helligkeit, F-21/C11). Der Reiterumbau betrifft nur die Darstellung; die Einstellungen aus 0.2.x bleiben erhalten.

### Tests (virtueller X-Server, Testspiel mit dem 4K-Bildschirmfoto des Anwenders als Hintergrund)

- Erkennung auf dem Foto, Münze 110 px: Ähnlichkeit 1,000 bei Faktor 1, 2 und 4; Abweichung des Mittelpunkts 0 px (F-24). Suchzeit für einen Rahmen 1200 × 850 px: 29,7 ms (Faktor 1), 4,8 ms (Faktor 2), 3,7 ms (Faktor 4) auf der Test-CPU [nicht auf dem Zielsystem gemessen].
- Testspiel: Münze erscheint an zufälliger Stelle und verschwindet beim Klick; zusätzlich eine stehende Lockmünze und die Zähler-Münzen.
  - R1 auf der schwarzen Fläche: 17 von 18 Münzen getroffen. Die fehlende war beim Start bereits sichtbar und wurde wie vorgesehen ignoriert (F-27). Mittlere Reaktionszeit 126 ms bei 200 ms Intervall.
  - R1 über der bunten Spalte: 22 von 23.
  - Lockmünze: 0 Klicks (Bestand). Mit Sperrbereich darüber ebenfalls 0. Ohne Bestandsregel und ohne Sperrbereich wird sie etwa einmal je Sperrzeit geklickt (erwartet, F-28).
  - Zähler-Münzen in R2: 0 Fehlklicks.
- Lastanteil: 4,1 % eines Kerns für einen Rahmen 1200 × 850 px bei 200 ms, 6,6 % für 1110 × 1720 px.
- Toleranz der Erkennung: Unschärfe 0,95, Helligkeit 1,00, Größe ±5 % etwa 0,65, ±10 % 0,52, Drehung 10° 0,54. Animiert oder skaliert sich die Münze im Spiel, muss die Schwelle gesenkt werden; der Status zeigt den aktuellen Wert je Rahmen.
- Regression Phase 1 (Mischtest, Bewegungstest, eigene Klicks mit Vorrang): unverändert ohne Verluste.

### Umsetzung gegen das Lastenheft v1.1

| ID | Stand |
|---|---|
| F-04, F-05 | umgesetzt: R1 bis R3 als eigene Zeilen mit Intervall, nacheinander mit den übrigen Zeilen |
| F-20 | umgesetzt: drei Rahmen bis Monitorgröße; Überschneidung wird im Editor abgewiesen, beim Start gemeldet, die Reaktion startet nicht |
| F-21 | umgesetzt: ein Muster, 8 px bis 200 px, Auswahl größer 200 px wird begrenzt |
| F-22 | umgesetzt: F9 oder Knopf mit Countdown, Editor auf dem eingefrorenen Foto |
| F-23 | umgesetzt: Schwelle und Verkleinerungsfaktor einstellbar |
| F-24 | umgesetzt: Mittelpunkt, in der Simulation 0 px Abweichung |
| F-25 | umgesetzt: höchstens ein Klick je Rahmen und Prüfung, stärkster Treffer |
| F-26 | umgesetzt: bis 8 Sperrbereiche je Rahmen |
| F-27 | umgesetzt, schaltbar, mit Ausnahme der Aufnahmestelle |
| F-28 | umgesetzt: Sperrzeit einstellbar |
| F-30 | umgesetzt: Rahmen (R1 grün, R2 gelb, R3 blau) und Sperrbereiche (rot schraffiert) als durchklickbare Überlagerung, sichtbar solange das Clicker-Fenster aktiv ist; [Annahme] über dem Vollbildspiel unter KWin noch nicht geprüft |
| F-32 | umgesetzt: `pattern.png` und Rahmen in der Konfiguration |
| F-33 | umgesetzt: Ähnlichkeit und Prüfdauer je Rahmen, letzte Reaktion mit Uhrzeit, Zeile, Position und Wert |

### Auslieferung

`XClicker_Advanced-0.3.0-x86_64.AppImage` (Inhalt gegen den Build geprüft, benötigt glibc ab 2.34), Quellarchiv `xclicker-advanced-0.3.0-src.tar.gz`.

## Offen

- Abnahme auf dem Zielsystem: Aufnahme des Spielfensters unter Xwayland bei Skalierung 2, Prüfdauer (Probe 0.1: 400 × 400 px im Mittel 10 ms), Sichtbarkeit der Rahmen über dem Spiel, Ähnlichkeitswerte der echten Münze (Animation, Skalierung).
- Quellarchiv in den Projektdateien steht noch auf 0.1.0.
- Erweiterungen vorbehalten: mehrere Muster, Klickversatz (F-21, F-24).
