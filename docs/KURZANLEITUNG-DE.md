# EPS-16 Plus Emulator – Kurzanleitung

Diese Anleitung erklärt die Installation und die Funktionen des Emulators.
Die Bedienung des Ensoniq EPS-16 Plus selbst – Instrumente laden, sampeln,
editieren und speichern – ist im originalen EPS-16-Plus-Handbuch beschrieben.

## Was ist dieser Emulator?

Der Emulator ist keine Sample-Library und keine vereinfachte Nachbildung der
EPS-Menüs. Er bildet die Gerätehardware nach und führt das originale
Ensoniq-ROM, das originale Betriebssystem und die originale
Keyboard-/Panel-Firmware aus.

Man kann ihn sich als virtuelles EPS-Mainboard vorstellen: Mausklicks werden
wie Taster am echten Gerät an den Keyboard-/Panel-Controller übergeben. Das
originale Betriebssystem entscheidet anschließend, was passiert. Dadurch
bleiben auch Arbeitsweise, Anzeigen und viele Eigenheiten der Hardware
erhalten.

Enthalten sind unter anderem:

- originale EPS-Menüführung mit 22-stelligem VFD und Statusanzeigen;
- acht Instrument-/Track-Taster, Data Entry und Pfeiltasten;
- ES5505-Klangerzeugung und ES5510-Effekte;
- Stereo-Sampling-Eingang mit LINE- und MIC-Signalweg;
- MIDI-Noten, Anschlagstärke, Pitch Wheel, Mod Wheel und Tastendruck;
- unabhängige Plug-in-Instanzen;
- vollständiger Projekt-/Preset-Restore einschließlich Instrumenten,
  Sample-RAM und eingelegter Diskette;
- EPS-Disketten als IMG und HFE.

Der Emulator wird als Apple-Silicon-VST3 für macOS geliefert. Es gibt in
diesem Paket keine Audio-Unit-Version.

## Voraussetzungen

- Apple-Silicon-Mac (arm64), macOS 11 oder neuer;
- VST3-fähige DAW, zum Beispiel Ableton Live;
- eigene, rechtmäßig erworbene EPS-Dateien:
  - kombinierter 128-KiB-Haupt-ROM;
  - 32-KiB-KPC-2.33-ROM;
  - EPS-16-Plus-OS-Diskette als 819.200-Byte-IMG oder HFE v1.

ROMs und Betriebssystem sind urheberrechtlich geschützt und deshalb nicht im
Paket enthalten.

## Installation

1. DAW beenden und das ZIP-Archiv entpacken.
2. `EPS-16 Plus Prototype.vst3` nach
   `~/Library/Audio/Plug-Ins/VST3/` kopieren.
3. Den Ordner `EPS_files` direkt daneben kopieren. Existiert er bereits,
   seinen Inhalt zusammenführen und eigene Disketten nicht überschreiben.
4. Die eigenen Dateien in `EPS_files` ablegen. Bevorzugte Namen:

       eps16plus-rom.bin
       eps16plus-kpc.bin
       EPS130OS.img

   Statt `EPS130OS.img` wird auch `EPS130OS.hfe` erkannt. Der originale
   KPC-Dateiname `Ensoniq EPS KPC2 v2.33 27c256.BIN` wird ebenfalls erkannt.
5. DAW starten und die VST3-Plug-ins neu einlesen lassen.
6. Den Emulator als Instrument laden. Nach dem Booten erscheint normalerweise
   `NO INSTRUMENTS`.

Werden ROM oder OS-Diskette erst bei geöffnetem Plug-in ergänzt, das Plug-in
einmal aus dem Projekt entfernen und erneut einsetzen.

Die fertige Ordnerstruktur sieht typischerweise so aus:

    ~/Library/Audio/Plug-Ins/VST3/
      EPS-16 Plus Prototype.vst3
      EPS_files/
        eps16plus-rom.bin
        eps16plus-kpc.bin
        EPS130OS.img

## Audio und MIDI

Das Plug-in ist ein Instrument mit Stereoausgang und einem zusätzlichen
Stereo-`Sampling Input`. MIDI auf der Instrumentenspur spielt die virtuelle
61-Tasten-Tastatur des EPS. Verwendet werden MIDI-Noten 36 bis 96; außerhalb
dieses Hardwareumfangs liegende Noten werden ignoriert.

Pitch Wheel und Mod Wheel steuern die entsprechenden originalen analogen
Controllerwege. Poly Aftertouch wird als Druck der jeweiligen EPS-Taste
übertragen. Der echte EPS besitzt jedoch kein MPE: Member-Channel-Pressure und
polyphones Pitch-Bending pro Note werden deshalb nicht auf globale oder
falsche Tastaturfunktionen umgedeutet. Noten und Note Off bleiben spielbar.

Für Sampling wird Audio in der DAW auf den zusätzlichen `Sampling Input`
geroutet. In Ableton Live geschieht das über den Eingangs-/Sidechain-Wähler des
Plug-ins. MIC/LINE, Filter, Aufnahme und Zuweisung bedient weiterhin das
originale EPS-OS nach dem Ensoniq-Handbuch.

## Die vier Disketten-Taster

Die kleinen Taster `OS`, `NEW`, `LOAD` und `SAVE` rechts oben sind
Medienfunktionen des Emulators. Sie sind nicht mit dem großen EPS-Modus-Taster
`LOAD` zu verwechseln. Jede Plug-in-Instanz besitzt ihr eigenes virtuelles
Laufwerk. Der Name der eingelegten Diskette steht unter den vier Tastern.

### OS – Betriebssystemdiskette wieder einlegen

`OS` legt die konfigurierte `EPS130OS.img` oder `EPS130OS.hfe` erneut ein. Das
ist beispielsweise praktisch, wenn das originale Betriebssystem wieder seine
Systemdiskette verlangt. Der laufende EPS erhält dabei ein echtes emuliertes
Diskettenwechsel-Signal und wird nicht neu gestartet.

### NEW – neue leere EPS-Diskette

`NEW` erzeugt eine frisch formatierte, leere 800-KiB-EPS-Diskette und legt sie
sofort ein. Die Anzeige nennt sie `NEWDISK (UNSAVED)`. Erst `SAVE` schreibt
sie als Datei auf den Mac. `NEW` fragt vor dem Ersetzen der aktuell
eingelegten Diskette nach. Vor jedem Medienwechsel empfiehlt sich trotzdem
`SAVE`, denn `OS` und `LOAD` müssen nicht vor ungespeicherten Änderungen
warnen.

### LOAD – vorhandene Diskette einlegen

`LOAD` öffnet eine vorhandene EPS-Diskettendatei:

- `IMG`: logisches 800-KiB-Diskettenabbild; gut für Emulator, Archiv und
  Datenaustausch mit passenden Werkzeugen;
- `HFE v1`: spurorientiertes Abbild für HFE-kompatible Laufwerksersatzgeräte,
  beispielsweise Gotek-Konfigurationen mit HFE-Unterstützung.

Der Emulator erkennt das Format anhand des Dateiinhalts. Nach dem Einlegen
wird der Diskettenwechsel an das originale OS gemeldet. Die Dateien werden
nicht über ein eigenes Schnelllade-Menü umgangen; geladen wird weiterhin mit
dem EPS-OS.

### SAVE – eingelegte Diskette exportieren

`SAVE` speichert den aktuellen Inhalt der virtuellen Diskette wahlweise als
IMG oder HFE. Dazu gehören auch Sektoränderungen, die das originale EPS-OS
seit dem Einlegen geschrieben hat.

- IMG empfiehlt sich für Backups und weitere Emulatorarbeit.
- HFE empfiehlt sich für ein HFE-kompatibles Laufwerksersatzgerät an echter
  Hardware.

Vor `OS`, `NEW` oder `LOAD` sollte eine veränderte Diskette mit `SAVE`
gesichert werden, wenn sie als eigenständige Datei erhalten bleiben soll.

## DAW-Projekte, Presets und mehrere Instanzen

Beim Speichern des DAW-Projekts wird der vollständige Maschinenzustand der
Instanz gesichert: Betriebssystemzustand, Instrumente, Samples, Effekte,
Controllerzustand und eingelegte Diskette. Beim erneuten Öffnen kann die
Instanz deshalb an derselben Stelle weiterarbeiten.

Das ersetzt nicht `SAVE`, wenn eine Diskettendatei außerhalb des Projekts oder
für echte Hardware benötigt wird. ROM- und OS-Dateien werden ebenfalls nicht
in das DAW-Projekt eingebettet; sie bleiben im externen `EPS_files`-Ordner.

Mehrere Instanzen sind vollständig voneinander getrennt. Jede besitzt eigene
Instrumente, Samples, Effekte und eine eigene eingelegte Diskette.

## Hinweise zur Bedienung

- Die Anzeige und die Bedeutung der Tasten kommen vom originalen EPS-OS.
- Derselbe Taster kann je nach Modus eine andere Funktion besitzen.
- Das Stacken per Doppelklick auf einen Instrument-/Track-Taster gehört zum
  LOAD-Kontext des originalen Betriebssystems.
- Die Computer-Pfeiltasten steuern bei fokussiertem Plug-in die vier
  EPS-Pfeiltasten. Die Maus kann alle Paneltaster und Data Entry bedienen.
- Eine separate Bildschirmtastatur gibt es absichtlich nicht; gespielt wird
  über die MIDI-Spur der DAW.

Für alle musikalischen Funktionen des Samplers gilt das originale
EPS-16-Plus-Handbuch.

## Wenn etwas nicht funktioniert

**Das Plug-in bootet nicht:** Dateinamen, Größen und Position des
`EPS_files`-Ordners prüfen. Danach das Plug-in neu einsetzen.

**macOS blockiert das Plug-in:** Das Prototyp-Paket ist technisch signiert,
aber nicht von Apple notarisiert. In den macOS-Datenschutz-/Sicherheits-
Einstellungen nur dann ausdrücklich erlauben, wenn das Paket aus einer
vertrauenswürdigen Quelle stammt. Gatekeeper nicht systemweit deaktivieren.

**Kein Ton:** Bootvorgang abwarten, Instrument laden oder erzeugen, richtigen
Track auswählen, MIDI-Noten 36–96 verwenden und VOLUME kontrollieren.

**Eine Diskette erscheint leer:** Im originalen EPS-OS den passenden
LOAD-/INSTRUMENT-Kontext wählen. Der Medien-Taster `LOAD` legt nur die
Diskette ein; er lädt nicht automatisch ein Instrument.

**Änderungen fehlen in einer externen Diskettendatei:** Vor dem Wechseln oder
Schließen ausdrücklich `SAVE` verwenden.
