# Airline Tycoon Deluxe: HD-Grafiken (Anleitung auf Deutsch)

English version, installation and everything else: [README.md](README.md). Die Kurzanleitung fuer Windows steht dort
unter "Quick start".

## HD-Grafiken (ab Version 2.0.0)

Ab Version 2.0.0 kann das Spiel alle Grafiken in 4-facher Aufloesung zeichnen. Die HD-Grafiken sind **nicht** im
Download enthalten: Jede Person erzeugt sie aus der eigenen, gekauften Spielkopie. Unter Windows geht das mit einem
Doppelklick auf `HD_generate.bat`. Ohne HD-Grafiken laeuft das Spiel wie bisher.

### Lizenz: bitte zuerst lesen

- **Spiel selbst kaufen.** Der Generator arbeitet nur mit einer eigenen Installation von Airline Tycoon Deluxe
  (GOG oder Steam). Die Originalgrafiken gehoeren BFG / Spellbound.
- **Das HD-Paket nur privat nutzen.** Die erzeugten Bilder sind abgeleitete Werke der Originalgrafiken. Nicht
  weitergeben, nicht hochladen, nicht verkaufen.
- **Modelle:** `4x-UltraSharp` steht unter CC BY-NC-SA 4.0 und darf **nur nicht-kommerziell** genutzt werden.
  `RealESRGAN_x4plus` steht unter BSD-3-Clause. Sie liegen nicht im Repository und nicht im Download.
  `HD_generate.bat` zeigt die Lizenzen an und laedt die Modelle erst, wenn man mit `Y` zustimmt; jede Datei wird an
  ihrer Pruefsumme (SHA-256) als die Originaldatei des Autors erkannt. `tools/hd_generate.py` selbst laedt nichts
  aus dem Netz.

### Empfohlen: mit einem Doppelklick (Windows)

1. **Spiel und die neueste Version installieren** (ab Version 2.0.4) wie unter [Installation](README.md#installation)
   beschrieben: das Release-Archiv in den Spielordner entpacken, in den Ordner mit `gli`, `room` und `misc`. Danach
   liegen dort `AT.exe`, `HD_generate.bat` und der Ordner `hd_tools`.
2. **Doppelklick auf `HD_generate.bat`.** Fragt Windows nach ("Der Computer wurde durch Windows geschuetzt"), auf
   "Weitere Informationen" und "Trotzdem ausfuehren" klicken. Das Skript erledigt alles Weitere und zeigt jeweils
   "Step x of 8". Seine Meldungen und Rueckfragen sind auf Englisch; Zustimmung mit `Y` (auch `J` geht):
   - prueft Spielordner, `AT.exe` und Spieldaten,
   - sucht Python 3.10 bis 3.13; fehlt es, bietet es die Installation mit `winget` an (nur fuer den eigenen
     Benutzer) oder nennt, was zu tun ist,
   - richtet eine eigene Python-Umgebung in `hd_tools\venv` ein; mit NVIDIA-Grafikkarte (`nvidia-smi`) mit CUDA,
   - zeigt die Lizenzen der beiden Modelle und laedt sie nach `Y` in den Ordner `models`,
   - exportiert die animierten Personen der Raeume (`AT.exe --hd-export alle`, ohne Fenster). Bricht `AT.exe` dabei
     ab, startet das Skript es neu und laesst den betroffenen Clip aus; klappt der Export gar nicht, rechnet es alles
     andere und nennt am Ende, was in normaler Aufloesung bleibt,
   - rechnet alle HD-Grafiken nach `hd` (mit Grafikkarte, wenn vorhanden) und zeigt die geschaetzte Restzeit,
   - setzt in `AT.json` `"OptionRenderScale": 4` und fragt, ob die Zwischendateien (`hd_assets`) und die
     Python-Umgebung geloescht werden sollen.
3. **Spiel starten.** Fertig.

Dauer: ohne NVIDIA-Grafikkarte etwa 12 Stunden (gemessen: 11:46 h fuer 14.343 Bilder auf einer frischen GOG-Kopie;
die Animationen sind der groesste Teil, mehr Prozessorkerne helfen), mit NVIDIA-Grafikkarte deutlich kuerzer. **Abbrechen** geht jederzeit mit Strg+C oder durch Schliessen des Fensters; der naechste Doppelklick setzt an
der Stelle fort. Platzbedarf: etwa 15 GB waehrend des Laufs, danach bleibt im Wesentlichen `hd`. Bei Problemen die
Datei `hd_generate.log` im Spielordner mitschicken.

Liegt das Spiel unter `C:\Program Files`, darf dort nicht jeder schreiben: dann `HD_generate.bat` per Rechtsklick
"Als Administrator ausfuehren".

### Fuer Fortgeschrittene: von Hand (Windows, Linux, macOS)

1. **Spiel und die neueste Version installieren** wie unter [Installation](README.md#installation) beschrieben. Die neue
   `AT.exe` (bzw. `AT`) liegt danach im Spielordner.
2. **Python installieren** (3.10 bis 3.13): Windows von [python.org](https://www.python.org/downloads/) (beim
   Installieren "Add python.exe to PATH" ankreuzen), Linux ueber die Paketverwaltung, macOS z. B. mit Homebrew.
3. **Generator holen:** Er liegt im Release im Ordner `hd_tools`, oder dieses Repository als ZIP herunterladen
   (gruener Knopf "Code" -> "Download ZIP") und den Ordner `tools/` nehmen. Die Befehle unten verwenden `tools/`.
4. **Python-Pakete installieren** (in einer Konsole im entpackten Ordner):

   ```sh
   pip install -r tools/hdgen/requirements.txt
   ```

   Mit NVIDIA-Grafikkarte vorher `torch` mit CUDA nach [pytorch.org](https://pytorch.org/get-started/locally/)
   installieren; dann geht alles viel schneller. Auf einem Mac mit Apple-Chip (M1 und neuer) bringt das
   normale `torch` die Grafikkarten-Unterstuetzung (MPS) schon mit.
5. **Modelle besorgen** und in einen Ordner `models/` neben die `AT.exe` legen:
   - `4x-UltraSharp.pth` von [OpenModelDB](https://openmodeldb.info/models/4x-UltraSharp) (Kim2091,
     CC BY-NC-SA 4.0, nur nicht-kommerziell; SHA-256 `a5812231fc936b42af08a5edba784195495d303d5b3248c24489ef0c4021fe01`):
     fuer Raeume, Oberflaeche, Hallen-Bausteine und Schriften
   - `RealESRGAN_x4plus.pth` aus dem [Release v0.1.0 von Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.1.0)
     (BSD-3-Clause; SHA-256 `4fa0d38905f75ac06eb49a7951b426670021be3018265fd191d2125df9d682f1`): fuer die Stadtfotos,
     die Figuren in der Halle und die animierten Personen in Raeumen

   Findet der Generator ein Modell nicht, fragt er nach dem Pfad (oder `--ultrasharp DATEI`, `--x4plus DATEI`,
   `--modelle ORDNER`).
6. **Animierte Personen in Raeumen exportieren** (Verkaeufer, Berater usw. sind Videos; das Spiel exportiert sie selbst
   nach `hd_assets/original/video` und beendet sich danach):

   ```sh
   AT.exe --hd-export alle
   ```

   Einzelne Raeume statt `alle`, z. B. `AT.exe --hd-export duty,bank`. Alternativ in `AT.json`
   `"OptionHdVideoExport": "alle"` setzen und das Spiel einmal starten.
7. **Generator starten** mit dem Spielordner als Ziel:

   ```sh
   python tools/hd_generate.py "C:/GOG Games/Airline Tycoon Deluxe" --alles --gpu
   ```

   Er exportiert die Grafiken, rechnet sie 4-fach hoch und legt sie in den Ordner `hd/` neben die `AT.exe`.
   Zwischendateien liegen in `hd_assets/` daneben (kann danach geloescht werden).
   - Ohne Grafikkarte dauert das ganze Paket viele Stunden; die Animationen allein auf der CPU etwa 17 Minuten je
     Million Pixel, alle Raeume zusammen grob 100 Millionen Pixel. Abbruch mit Strg+C ist jederzeit moeglich; ein
     neuer Start setzt fort. Gleiche Bilder werden nur einmal gerechnet.
   - Ohne `--alles` rechnet der Generator alles ausser den Animationen. Nur Teile: `--nur schriften`, `--nur
     stadtfotos`, `--nur ui`, `--nur raeume`, `--nur bausteine`, `--nur figuren` (Personen in der Halle), `--nur
     animationen --raum duty` (Raeume siehe [ASSETS.md](ASSETS.md#animierte-personen-in-raeumen-export-und-zaehlung-h25a)),
     auch kombiniert, z. B. `--nur schriften,stadtfotos`. Schriften und Stadtfotos sind schnell und zeigen sofort, ob
     alles klappt.
   - Die Hallen-Bausteine bekommen ihre Kante per Modell (glattere Raender, Standard seit 2.0.4);
     `--bausteine-lanczos` rechnet sie wie vorher.
   - `--gpu` rechnet auf der Grafikkarte: CUDA (NVIDIA, Windows/Linux) oder MPS (Mac mit Apple-Chip), sonst auf der
     CPU. `--liste` zeigt nur, was zu tun ist. `--log DATEI` schreibt alle Ausgaben zusaetzlich in eine Datei,
     `--english` gibt die Meldungen auf Englisch aus.
   - Das Ergebnis ist reproduzierbar: gleiche Spielkopie, gleiche Modelle und CPU ergeben dieselben Bilder.
8. **Im Spiel einschalten:** in der Datei `AT.json` neben der `AT.exe` `"OptionRenderScale": 4` eintragen (die Datei
   entsteht beim ersten Start des Spiels).

### Optionen in AT.json

Die Datei `AT.json` liegt neben der `AT.exe` (bzw. `AT`) und entsteht beim ersten Start.

| Option | Wert | Bedeutung |
|---|---|---|
| `OptionRenderScale` | `4` | HD-Grafiken verwenden. Muss `4` sein, weil das Paket 4-fach ist (Standard `1` = aus) |
| `OptionWidescreen` | `1` / `0` | Breitbild an (Standard) bzw. aus (Bild wie im Original 4:3) |
| `OptionWidescreenRoomBorder` | `1` / `0` | Raender neben Raeumen weichgezeichnet (Standard) bzw. schwarz |
| `OptionHdVideo` | `1` / `0` | Animierte Personen in Raeumen in HD, wenn `hd/video` vorhanden (Standard) bzw. immer 1x |
| `OptionHdVideoMaxMB` | `1024` | Grafikspeicher je animierter Person; passt sie nicht hinein, wird sie ganz in 2x gezeigt |
| `OptionHdVideoCacheMB` | `1536` | Grafikspeicher fuer animierte Personen verlassener Raeume; beim erneuten Betreten sofort HD (`0` = aus). Bei wenig Grafikspeicher (integrierte Grafik) kleiner stellen |

`AT.json` nur bei beendetem Spiel bearbeiten: Das Spiel schreibt die Datei beim Beenden neu.

**Werkzeug- und Diagnose-Optionen** (normalerweise nicht noetig; Einzelheiten in [ASSETS.md](ASSETS.md)):

| Option | Standard | Wofuer |
|---|---|---|
| `OptionHdVideoExport` | `""` | Werkzeug: beim Start die Raum-Animationen fuer den Generator exportieren (`"alle"` oder Raeume), setzt sich danach selbst zurueck; einfacher: `AT.exe --hd-export alle` |
| `OptionHdOverlayFilter` | `0` | Feineinstellung: Filter der 1x-Reste ueber HD-Hintergruenden (`0` scharf, `1` weich) |
| `OptionHdMissingLog` | `false` | Diagnose: jede Grafik ohne HD-Bild einmal ins Log |
| `OptionHdFrameStats` | `0` | Diagnose: Frame-Zeiten und Lage des Mauszeigers alle 5 s ins Log |
| `OptionCursorExitAnim` | `1` | Diagnose: `0` = Exit-Pfeil steht still (zum Eingrenzen von Rucklern) |
| `OptionHdDebugMask` | `false` | Diagnose: alle 5 s Masken-Bilder nach `hd_debug/` |
| `OptionHdPolCheck` | `0` | Diagnose: beim Start die exportierten Hallen-Figuren gegen die Engine pruefen |

### macOS

> **HD und Breitbild sind auf dem Mac bisher ungetestet**, bis jemand es im Spiel bestaetigt. Getestet ist beides im
> echten Spiel nur unter Windows. Rueckmeldungen gern als Issue.

1. **Python:** entweder mit [Homebrew](https://brew.sh) (`brew install python`) oder mit dem Installer von
   [python.org](https://www.python.org/downloads/macos/). Die Pakete am besten in eine eigene Umgebung installieren
   (bei Homebrew-Python noetig, weil `pip install` dort sonst abgelehnt wird):

   ```sh
   python3 -m venv ~/at-hd
   source ~/at-hd/bin/activate
   pip install -r tools/hdgen/requirements.txt
   ```

   Auf Macs mit Intel-Prozessor gibt es aktuelle `torch`-Versionen nicht mehr; dort geht es nur mit einer aelteren
   Python-Version (bis 3.12) und `torch` 2.2, ohne Grafikkarte (ungetestet).
2. **Spieldaten aus der GOG-Mac-Version:** Das Spiel ist ein `.app`-Paket, z. B. `/Applications/Airline Tycoon
   Deluxe.app`. Die Spieldaten liegen darin in einem Ordner mit `gli/`, `room/` und `misc/` (Rechtsklick auf die App ->
   "Paketinhalt zeigen"; der genaue Pfad haengt von der Version ab). Der Generator findet diesen Ordner selbst:

   ```sh
   python3 tools/hd_generate.py "/Applications/Airline Tycoon Deluxe.app" --gpu
   ```

3. **Wohin gehoert `hd/`?** Der Mac-Build von Version 2.0.0 ist ein einzelnes Programm `AT` (kein `.app`). Das Spiel
   sucht Spieldaten, `AT.json` und `hd/` im Ordner, in dem `AT` liegt. `AT` gehoert also in den Datenordner (den mit
   `gli/`), und genau dorthin legt der Generator `hd/`. Liegt dort noch kein `AT`, nennt er den Ordner.
4. **Grafik auf dem Mac:** Das Spiel zeichnet ueber SDL, auf dem Mac standardmaessig mit **Metal**. Zum Vergleich laesst
   es sich mit OpenGL starten: `SDL_RENDER_DRIVER=opengl ./AT`. Auf Retina-Bildschirmen zeichnet das Spiel derzeit in der
   Punkt-Aufloesung (z. B. 1440x900 statt 2880x1800); HD ist dort schaerfer als 1x, nutzt die volle Retina-Aufloesung
   aber noch nicht.

Technische Einzelheiten (Verfahren, Manifest, Dateinamen) stehen in [ASSETS.md](ASSETS.md).
