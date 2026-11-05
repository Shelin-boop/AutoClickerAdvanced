---
title: "Lastenheft XClicker Advanced"
subtitle: "Version 1.1, freigegeben"
date: "2026-11-03"
lang: de-DE
---

# 1 Dokumentinformation

| Feld | Inhalt |
|---|---|
| Dokument | Lastenheft XClicker Advanced |
| Version | 1.1 |
| Stand | 2026-11-03 |
| Status | Freigegeben durch den Auftraggeber am 2026-11-03; Version 1.1 auf Anweisung des Auftraggebers vom 2026-11-03 (Abschnitt 11) |
| Basis | xclicker v1.5.1 (robiot/xclicker, GPL-3.0), Quellcode aus dem Projektverzeichnis |
| Kennzeichnung | [verifiziert] = am Quellcode oder an einer Quelle geprüft; [Annahme] = Erfahrungswissen, nicht geprüft; [Vorschlag] = Vorgabe des Auftragnehmers, mit der Freigabe bestätigt; A1 bis D20 = Antworten des Auftraggebers vom 2026-11-03 |

# 2 Ziel und Abgrenzung

XClicker Advanced ist ein Autoclicker für Linux auf Basis von xclicker. Er arbeitet bis zu sieben Zeilen ab, jede mit eigenem Intervall: Klick an der echten Mausposition, Klicks an drei festen Positionen und drei bildbasierte Reaktionen. Es läuft immer nur ein Klick zur selben Zeit. Eine Reaktion klickt auf ein vom Anwender festgelegtes, kreisrundes Muster, sobald es in einem festgelegten Bildschirmbereich neu erscheint.

Nicht Bestandteil der ersten Ausbaustufe: mehrere Muster, Klickversatz zum Mustermittelpunkt, Toleranz gegen Größen- oder Farbänderung, Mehrmonitorbetrieb, Sprachumschaltung (die Oberfläche ist englisch). Die Architektur wird so angelegt, dass diese Punkte später ergänzt werden können.

# 3 Einsatzumgebung

| Nr. | Punkt | Stand |
|---|---|---|
| U-1 | Sitzung | Wayland (Angabe Auftraggeber). Distribution und Desktop-Version werden in Phase 0 automatisch erfasst. |
| U-2 | Zielanwendung | Spiel im Vollbild. Der Clicker wird über die Super-Taste in den Vordergrund geholt und dort eingestellt. [verifiziert, 2026-11-03] Das Spielfenster ist ein XWayland-Fenster (`xprop` liefert WM_CLASS „[Name des Spiels].x86_64“). [Annahme] Die Endung „.x86_64“ deutet auf eine native Linux-Version hin. |
| U-3 | Monitor | Ein Monitor (Hauptmonitor), Auflösung wird automatisch erkannt. |
| U-4 | Architektur | x86_64 |
| U-5 | Ressourcen | Geringer zusätzlicher Verbrauch gewünscht; gröbere Bildanalyse ist zulässig. |

## 3.1 Beobachtungen aus den Fotos vom 2026-11-03

- Das Zielmuster ist die silberne Münze (rot markiert). Sie verschwindet nach dem Klick.
- Ähnliche, etwas kleinere Münzen in den Zähleranzeigen (gelb markiert) bleiben stehen, verschieben sich aber leicht hin und her. Sie dürfen keine Reaktion auslösen.
- Die Fotos sind mit einer Kamera vom Monitor abgenommen. Pixelmaße lassen sich daraus nicht ableiten; maßgeblich ist das spätere Bildschirmfoto im Programm.

# 4 Technisches Risiko: Wayland

## 4.1 Sachstand

- [verifiziert] xclicker nutzt ausschließlich X11-Funktionen: Zeiger versetzen mit `XWarpPointer`, Klicken mit XTest, Hotkeys mit XInput2.
- [verifiziert] Im Quellcode klickt xclicker bei aktivierter Custom Location nur einmal, nach dem Versetzen des Zeigers.
- [verifiziert] GNOME 50 hat die X11-Sitzung entfernt. KDE Plasma 6.8 bietet nur noch Wayland an (laut Bericht vom 2026-06-05 geplant für etwa 2026-10-14; das tatsächliche Erscheinen ist nicht geprüft).

## 4.2 Hypothese zum beobachteten Doppelklick [Annahme]

Unter Wayland dürfen Programme den echten Mauszeiger nicht versetzen. `XWarpPointer` wirkt deshalb nur innerhalb von XWayland, der X11-Kompatibilitätsschicht. Der simulierte Klick kann dann an zwei Stellen wirksam werden: intern in XWayland an der versetzten Position und über den Compositor an der echten Zeigerposition. Das deckt sich mit der Beobachtung des Auftraggebers (Zeiger bleibt stehen, Klick an beiden Stellen). Ohne Test auf dem Zielsystem ist das nicht bestätigt.

## 4.3 Lösungswege

| Weg | Klick an fester Position | Bildaufnahme | Zeiger | Bewertung |
|---|---|---|---|---|
| A: X11 über XWayland (Vorgabe Auftraggeber) | Ereignis direkt an das Spielfenster senden, ohne Zeiger zu bewegen | Spielfenster über X11 auslesen | bleibt stehen | geringster Aufwand; funktioniert nur, wenn das Spiel ein XWayland-Fenster ist und synthetische Ereignisse annimmt [Annahme] |
| B: Desktop-Portale (RemoteDesktop und ScreenCast) | Zeiger kurz auf die Position setzen, klicken, zurücksetzen | Bildschirmfreigabe über PipeWire | springt kurz sichtbar | funktioniert auch mit nativen Wayland-Fenstern; einmaliger Freigabedialog des Desktops [Annahme] |

Das Spiel ist ein XWayland-Fenster (U-2); die Grundvoraussetzung für Weg A ist damit erfüllt. Ob das Spiel direkt gesendete Ereignisse annimmt und ob die Bildaufnahme über X11 funktioniert, klärt der Machbarkeitstest (Phase 0, Abschnitt 8). Weg A wird bevorzugt, sofern er die Anforderung F-13 erfüllt.

# 5 Anforderungen

## 5.1 Ablaufsteuerung

| ID | Anforderung | Abnahmekriterium |
|---|---|---|
| F-01 | Jede Zeile (M Mausposition, P1, P2, P3, R1, R2, R3) hat ein eigenes Intervall und klickt in diesem Intervall. Nach dem Start ist jede Zeile einmal sofort fällig. Sind mehrere Zeilen gleichzeitig fällig, werden sie nacheinander ausgeführt: zuerst die am längsten fällige, bei Gleichstand die obere. | Zählung je Zeile im Status entspricht den Intervallen. |
| F-02 | Jede Zeile ist einzeln ein- und ausschaltbar. Ausgeschaltete Zeilen beeinflussen die anderen nicht. | Funktionstest. |
| F-03 | Das Intervall wird je Zeile in ms eingegeben, Bereich 50 ms bis 60 000 000 ms; mit Safe Mode mindestens 100 ms. Die gemeinsame Schrittdauer (h, min, s, ms) entfällt. Dauert ein Klick länger als das Intervall, ist die Zeile sofort wieder fällig. | Eingaben außerhalb des Bereichs werden auf die Grenze gesetzt. |
| F-04 | Eine Reaktion prüft ihren Rahmen in ihrem Intervall; ohne Treffer belegt sie nur die Analysezeit. | Messung im Protokoll. |
| F-05 | Reaktionen klicken nur, wenn ihr Intervall fällig ist. | Kein Reaktionsklick außerhalb des eigenen Intervalls. |
| F-06 | Start und Stopp per Knopf und per Hotkey (Vorgabe F8). Hotkey-Modus „Normal“ und „Halten“ wie im Original. Optional Stopp nach einer Anzahl Klicks (alle Zeilen zusammen, wie „Repeat“ im Original) und Zufallsabweichung der Intervalle. | Funktionstest. |

## 5.2 Klickschritte M und P1 bis P3

| ID | Anforderung | Abnahmekriterium |
|---|---|---|
| F-10 | Je Schritt einstellbar: Maustaste (links, Mitte, rechts), Klickart (einfach, doppelt), Haltezeit (konstant oder zufällig, optional). Die Klickart „Dauerhalten“ entfällt, da sie in einer Schrittfolge keinen Sinn ergibt. [Vorschlag] | Funktionstest je Schritt. |
| F-11 | Schritt M klickt an der echten Zeigerposition. | Funktionstest. |
| F-12 | Zeile M wartet, solange der Anwender die Maus bewegt, und klickt nach einer Ruhezeit (Vorgaben siehe Abschnitt 6). Optional warten auch P1 bis P3 (Vorgabe an). | Bei Mausbewegung kein Klick in Zeile M. |
| F-13 | P1 bis P3 klicken ausschließlich an ihrer festgelegten Position. An der echten Zeigerposition erfolgt kein Klick. | Test mit Klick-Testfenster: nur die Position P wird registriert. |
| F-14 | Der Zeiger bleibt dort, wo der Anwender ihn hält. Ist technisch ein kurzes Versetzen und Zurücksetzen nötig (Weg B), ist das zulässig. | Zeiger steht nach jedem Schritt wieder an der Position des Anwenders. |
| F-15 | Die Positionen werden wie im Original per „Get“ (Klick auf den Bildschirm) oder durch Eingabe festgelegt. | Funktionstest. |
| F-16 | Der Abstand zwischen den Klicks eines Doppelklicks ist einstellbar. Zwischen Bewegen, Drücken, Loslassen und Zurückbewegen sowie zwischen zwei Klicks liegt eine einstellbare Wartezeit, damit ein Spiel jedes Ereignis in einem eigenen Bild sieht. | Funktionstest. |

## 5.3 Reaktionen R1 bis R3

| ID | Anforderung | Abnahmekriterium |
|---|---|---|
| F-20 | Drei rechteckige Rahmen, je bis zur Größe des Monitors. Überschneiden sich Rahmen, erscheint eine Fehlermeldung, und die betroffene Reaktion lässt sich nicht aktivieren. | Überschneidung wird abgewiesen. |
| F-21 | Ein gemeinsames Muster für alle Rahmen. Es ist kreisrund, gleich groß und gleichfarbig, nicht verdeckt; Größe bis 200 × 200 px. Mehrere Muster sind als Erweiterung vorgesehen. | Muster bis 200 px wird angenommen, größere Auswahl abgewiesen. |
| F-22 | Muster und Rahmen werden auf einem eingefrorenen Bildschirmfoto ausgewählt. Der Anwender löst das Foto aus: per Hotkey (Vorgabe F9) oder per Knopf mit Countdown, damit vorher zum Spiel gewechselt werden kann. Der Anwender zieht ein Quadrat um das Muster; ausgewertet wird der einbeschriebene Kreis. | Funktionstest. |
| F-23 | Erkennung über Bildvergleich mit einstellbarer Ähnlichkeitsschwelle. Zur Ressourcenschonung wird mit verkleinertem Bild gearbeitet (Faktor einstellbar). | Testbilder mit und ohne Muster. |
| F-24 | Klickziel ist der Mittelpunkt des erkannten Musters. Ein Versatz ist als Erweiterung vorgesehen. | Abweichung höchstens Verkleinerungsfaktor in px. |
| F-25 | Je Rahmen und Durchlauf wird höchstens ein Treffer geklickt (der mit der höchsten Ähnlichkeit); weitere werden ignoriert. | Funktionstest. |
| F-26 | Sperrbereiche: Je Rahmen können Rechtecke festgelegt werden, in denen keine Reaktion erfolgt (zum Beispiel über den Zähleranzeigen). | Muster im Sperrbereich löst nicht aus. |
| F-27 | Bestand beim Start: Muster, die beim Start bereits sichtbar sind, gelten als bekannt und lösen nicht aus. Bekannte Muster werden innerhalb des Sperrradius nachverfolgt, sodass kleines Hin- und Herwackeln keine Reaktion auslöst. Ausnahme: Ein Treffer innerhalb des Sperrradius um die Stelle, an der das Muster aufgenommen wurde, wird nie als Bestand geführt, da das Zielmuster dort wiederkehren kann. Schaltbar. | Wackelnde Zähler-Münze löst nicht aus; Muster an der Aufnahmestelle löst aus. |
| F-28 | Nach einem Reaktionsklick gilt für diese Stelle eine Sperrzeit. Ist das Muster danach noch sichtbar, wird es wie ein neues behandelt. | Kein zweiter Klick innerhalb der Sperrzeit. |

## 5.4 Bedienung und Anzeige

| ID | Anforderung | Abnahmekriterium |
|---|---|---|
| F-30 | Rahmen und Sperrbereiche sind beim Erstellen sichtbar und immer dann, wenn das Clicker-Fenster im Vordergrund ist. Sie sind durchklickbar und werden ausgeblendet, wenn eine andere Anwendung im Vordergrund ist. | Sichtprüfung. |
| F-31 | Oberfläche in GTK3, angelehnt an xclicker, mit einer Schrittliste (Schalter, Maustaste, Klickart, Einstellknopf je Schritt). Oberflächensprache Englisch. | Sichtprüfung. |
| F-32 | Einstellungen werden automatisch gespeichert, das Muster als PNG-Datei im Konfigurationsverzeichnis des Benutzers. | Neustart übernimmt alle Einstellungen. |
| F-33 | Statusanzeige: aktueller Schritt, letzte Reaktion (Uhrzeit hh:mm:ss, Position, Ähnlichkeitswert). Dient der Einstellung der Schwelle. | Sichtprüfung. |

## 5.5 Installation und Paket

| ID | Anforderung | Abnahmekriterium |
|---|---|---|
| F-40 | Auslieferung als AppImage für x86_64. | Start durch Doppelklick nach Setzen des Ausführungsrechts. |
| F-41 | Beim Start trägt sich das Programm automatisch ins Startmenü ein (Benutzerebene, ohne Root-Rechte): Desktop-Datei und Icon. Wurde die AppImage verschoben, wird der Eintrag beim nächsten Start angepasst. | Eintrag mit Icon erscheint im Startmenü. |
| F-42 | In den Einstellungen gibt es einen Knopf „Startmenü-Eintrag entfernen“. | Eintrag und Icon werden entfernt. |
| F-43 | Programmname „XClicker Advanced“, eigenes Icon auf Basis des Originals. [Vorschlag] | Sichtprüfung. |
| F-44 | GTK3 wird wie beim Original vom System verwendet; weitere Bibliotheken (zum Beispiel für die Bilderkennung) sind in der AppImage enthalten. | Start auf dem Zielsystem ohne Nachinstallation. |

## 5.6 Ressourcen

| ID | Anforderung | Abnahmekriterium |
|---|---|---|
| F-50 | Bilder werden nur aufgenommen und ausgewertet, wenn ein Reaktionsschritt an der Reihe ist, und nur im Bereich des Rahmens. | Code-Prüfung. |
| F-51 | Zielwert CPU-Last im Betrieb: unter 5 % eines Kerns bei einem aktiven Rahmen. [Annahme, Zielwert; Messung in Phase 2] | Messung auf dem Zielsystem. |

## 5.7 Weitergabe (vorbereitet)

| ID | Anforderung | Abnahmekriterium |
|---|---|---|
| F-60 | Lizenz GPL-3.0. Die Urheberhinweise des Originals bleiben erhalten; ein Änderungsvermerk mit Datum wird beigefügt. Lizenztexte aller enthaltenen Bibliotheken liegen bei. Zu jeder AppImage gibt es ein Quellcode-Archiv und eine README. Dies ist keine Rechtsberatung. | Vollständigkeitsprüfung der Dateien. |

# 6 Parameter und Vorgabewerte

| Parameter | Vorgabe | Bereich | Herkunft |
|---|---|---|---|
| Intervall je Zeile | M 100 ms, P 1000 ms | 50 ms bis 60 000 000 ms (Safe Mode: ab 100 ms) | Auftraggeber, v1.1 |
| Safe Mode | an | an, aus | Original |
| Zufallsabweichung der Intervalle | aus | ± 0 ms bis ± 60 000 ms | Original |
| Stopp nach Klicks | aus (endlos) | 1 bis 100 000 000 Klicks | Original („Repeat“) |
| Hotkey Start/Stopp | F8 | frei wählbar | Original |
| Hotkey Bildschirmfoto | F9 | frei wählbar | neu [Vorschlag] |
| Countdown Bildschirmfoto | 5 s | 0 s bis 30 s | neu [Vorschlag] |
| Doppelklick-Abstand | 150 ms | 20 ms bis 500 ms | Original (dort fest) |
| Wartezeit zwischen Klick-Ereignissen | 40 ms | 0 ms bis 200 ms | Phase 1, Rückmeldung 0.1.0 |
| Positionen warten bei Mausbewegung | an | an, aus | Phase 1, Rückmeldung 0.1.0 |
| Bewegungsschwelle | 30 px | 1 px bis 100 px | B5, Freigabe |
| Ruhezeit bis Fortsetzung | 300 ms | 50 ms bis 5000 ms | B5 [Vorschlag] |
| Mustergröße | aus der Auswahl | 8 px bis 200 px | C11 |
| Ähnlichkeitsschwelle | 0,85 | 0,50 bis 0,99 | [Vorschlag, in Phase 2 abzustimmen] |
| Verkleinerungsfaktor Analyse | 2 | 1, 2, 4 | A3 |
| Sperrradius (Wackeln) | 30 px | 0 px bis 200 px | C14 [Vorschlag] |
| Bestand beim Start ignorieren | an | an, aus | ursprüngliche Anforderung |
| Sperrzeit nach Reaktion | 1000 ms | 0 ms bis 10 000 ms | [Vorschlag] |
| Sperrbereiche je Rahmen | keine | bis 8 Rechtecke | C14 [Vorschlag] |

# 7 Vorgesehene Erweiterungen

- Mehrere Muster, je Rahmen eigenes Muster.
- Klickversatz zum Mustermittelpunkt.
- Toleranz gegen Größen-, Farb- und Hintergrundänderung.
- Mehrere Monitore.
- Übersetzbare Oberfläche (zum Beispiel Deutsch).

# 8 Phasenplan

| Phase | Inhalt | Ergebnis |
|---|---|---|
| 0 | Machbarkeitstest mit einem kleinen Diagnoseprogramm auf dem Zielsystem: Ist das Spiel ein XWayland-Fenster? Lässt es sich über X11 auslesen? Welche Klickmethode trifft nur die Position P (Ereignis direkt ans Fenster, XTest mit Versetzen, Portal)? Kommen Hotkeys an, während das Spiel im Vordergrund ist? | Architekturentscheid Weg A oder B |
| 1 | Fork von xclicker: Ablaufsteuerung, Schritte M und P1 bis P3, Einstellungen je Schritt, AppImage, Startmenü-Eintrag | Erste nutzbare Version |
| 2 | Reaktionen: Bildschirmfoto, Auswahl von Muster, Rahmen und Sperrbereichen, Overlay, Erkennung, Ignorierlogik, CPU-Messung | Vollständiger Funktionsumfang |
| 3 | Weitergabe: Lizenzdateien, Änderungsvermerk, Quellcode-Archiv, README | Weitergabefähiges Paket |

Jede Phase wird auf dem Zielsystem des Auftraggebers abgenommen. Im Entwicklungssystem des Auftragnehmers ist nur ein Test mit virtuellem X11-Bildschirm möglich, kein Test unter Wayland und nicht mit dem Spiel.

# 9 Offene Punkte

| Nr. | Frage | Benötigt für |
|---|---|---|
| O-1 | Distribution und Desktop-Version: werden vom Diagnoseprogramm (Phase 0) automatisch erfasst. | Phase 0 |
| O-2 | Spiel ist XWayland-Fenster: erledigt (U-2). Hotkey-Empfang bei Spiel im Vordergrund, Annahme direkt gesendeter Klicks und Bildaufnahme: werden in Phase 0 geprüft. | Phase 0, Wahl Weg A/B |
| O-3 | Oberflächensprache: erledigt, Englisch. | Phase 1 |
| O-4 | Freigabe: erledigt am 2026-11-03. | Beginn Phase 0 |

# 10 Quellen

- xclicker v1.5.1, Quellcode aus dem Projektverzeichnis (geprüft 2026-11-03).
- PC Games Hardware, Bericht vom 2026-06-05: KDE Plasma 6.8 ohne X11-Sitzung. <https://www.pcgameshardware.de/Linux-Software-26761/News/KDE-Plasma-68-drops-X11-Wayland-becomes-mandatory-from-October-2026-1544518/>
- It's FOSS, Bericht vom 2026-03-19: GNOME 50 ohne X11. <https://itsfoss.com/news/gnome-50-release.md>

# 11 Änderungshistorie

| Version | Datum | Änderung |
|---|---|---|
| 0.1 | 2026-11-03 | Entwurf zur Freigabe |
| 1.0 | 2026-11-03 | Freigabe mit Änderungen: F-14 kurzes Versetzen des Zeigers zulässig; F-27 Ausnahme für die Aufnahmestelle des Musters; Oberfläche Englisch; Bewegungsschwelle Vorgabe 30 px, höchstens 100 px; U-2 und O-1 bis O-4 aktualisiert |
| 1.1 | 2026-11-03 | Auf Anweisung des Auftraggebers: eigenes Intervall je Zeile statt gemeinsamer Schrittdauer (F-01 bis F-05); Stopp nach Klicks statt Zyklen (F-06); Wartezeit zwischen Klick-Ereignissen und optionales Warten der Positionen bei Mausbewegung (F-12, F-16, Abschnitt 6) |
