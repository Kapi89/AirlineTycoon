# AirlineTycoon

This repository aims to complete the partial source code that is provided as a free bonus in the GOG release
of Airline Tycoon Deluxe.

To run it, you'll need the game assets from the Deluxe edition of the game.
Therefore, you need an existing installation of the game. You can purchase the game from [GOG.com](https://www.gog.com/en/game/airline_tycoon_deluxe) or from [Steam](https://store.steampowered.com/app/331920/Airline_Tycoon_Deluxe/).

## Major Additions

- Native Linux support
- Dedicated server browser and NAT-punchthrough multiplayer (open source server at: https://github.com/WizzardMaker/ATDMasterServer)
- New computer player that plays well without cheating

## License

The code in the repository is licensed under the terms included in the GOG release. As such the code can
only be used for non-commercial purposes and remains property of BFG.

It is therefore *not* open-source in the free software sense, for more information refer to the License.txt.

## Installation

1. Buy and install a version of the game (for example on [GOG.com](https://www.gog.com/en/game/airline_tycoon_deluxe) or on [Steam](https://store.steampowered.com/app/331920/Airline_Tycoon_Deluxe/))
2. Download the latest release of this mod for your platform (Windows, macOS or Linux)
3. Unpack the archive into the folder where the old game executable is (for example At.exe on Windows)
4. Start the game using the new game executable (AT.exe on Windows, AT on macOS and Linux)
5. Enjoy!

## HD-Grafiken (ab Version 2.0.0, Anleitung auf Deutsch)

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

1. **Spiel und die neueste Version installieren** (ab Version 2.0.4) wie unter [Installation](#installation)
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

Dauer: mit NVIDIA-Grafikkarte je nach Karte etwa eine Stunde, nur mit dem Prozessor ein bis zwei Tage (die
Animationen sind der groesste Teil; mehr Prozessorkerne helfen). **Abbrechen** geht jederzeit mit Strg+C oder durch Schliessen des Fensters; der naechste Doppelklick setzt an
der Stelle fort. Platzbedarf: etwa 15 GB waehrend des Laufs, danach bleibt im Wesentlichen `hd`. Bei Problemen die
Datei `hd_generate.log` im Spielordner mitschicken.

Liegt das Spiel unter `C:\Program Files`, darf dort nicht jeder schreiben: dann `HD_generate.bat` per Rechtsklick
"Als Administrator ausfuehren".

#### In English (short)

Windows: unpack the release (2.0.4 or later) into your game folder (the one with `gli`, `room`, `misc`) and
double-click `HD_generate.bat`. It checks the game folder, installs Python 3.12 with `winget` if needed (after asking),
sets up its own Python environment in `hd_tools\venv` (with CUDA on NVIDIA cards), shows the model licenses and
downloads the two models after you type `Y` (4x-UltraSharp is CC BY-NC-SA 4.0, non-commercial only), exports the
room animations with `AT.exe --hd-export alle` (a clip that fails is skipped and reported), generates all HD graphics
into `hd`, sets `"OptionRenderScale": 4` in `AT.json` and offers to delete the temporary files. Press Ctrl+C to stop;
the next double-click resumes. All messages are in English; send `hd_generate.log` if something goes wrong. The HD
graphics are for private use only.

### Fuer Fortgeschrittene: von Hand (Windows, Linux, macOS)

1. **Spiel und die neueste Version installieren** wie unter [Installation](#installation) beschrieben. Die neue
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

## Building

Before building, remember to clone the submodules:
```
git submodule update --init
```

You can see the instructions to build and run the project in [BUILDING.md](BUILDING.md).

## Changes

### HD graphics and widescreen (2.0.0 - 2.0.4)
* 2.0.4: `HD_generate.bat` generates all HD graphics with one double-click on Windows (Python, models after license
  confirmation, room animations via the new start parameter `AT.exe --hd-export alle`); model edges for hall bricks are
  now the default; fixed crashes in the Smacker video decoder (empty Huffman trees, frame sizes not a multiple of 4)
* Optional 4x HD graphics, generated locally from your own copy of the game with `tools/hd_generate.py` (see the German
  guide above; nothing copyrighted is distributed): rooms, UI, fonts, city photos, airport hall bricks, people in the
  hall, thought bubbles and, since 2.0.3, the animated people in rooms
* Widescreen: the airport hall and rooms with a status bar use the full 16:10 / 16:9 width; soft borders next to rooms
* 2.0.3: animated people in rooms in HD (loaded in the background, cached across visits, whole figure in 2x if it does
  not fit the memory limit), fixed mouse cursor jumps and shifted click areas in widescreen rooms

### General
* Now runnable on Linux (including support for FLC animations found in some versions)
* Game can be configured for windowed / borderless / fullscreen display

### New computer player
* Respects the rules of the game

| Rules | Existing computer player | New computer player |
| -------- | ------- | ------- |
| Plane constraints | ❌ Ignores maximum range | ✅ Respects maximum range |
| | ❌ Ignores number of seats | ✅ Respects number of seats |
| | ✅ Respects freight capacity | ✅ Respects freight capacity |
| Charter flights | ❌ Ignores date constraints | ✅ Respects date constraints |
| | ❌ Does not pay fine | ✅ Pays fine if not executed correctly |
| Hidden bonus | ❌ Up to 10% higher premium for charter flights | ✅ No hidden bonus |
| | ❌ Up to 25% bonus when competing for routes | ✅ No hidden bonus |
| | ❌ Small bonus to image every day | ✅ No hidden bonus |
| Employees | ❌ Completely fakes airline employees | ✅ Needs to hire employees |
| Laptop virus | ❌ Removed next day | ✅ Has to act himself |
| Strike | ❌ Will end randomly | ✅ Has to act himself |
| Illness | ❌ No action taken | ✅ Uses medicine |
| Missions | ❌ Uses various cheats | ✅ No cheating |
| Info | ❌ Accesses game state directly | ✅ Only acts on information that would be available to human player |

* Improved core aspects of the computer player

| Features | Existing computer player | New computer player |
| -------- | ------- | ------- |
| Flight planning | ❌ Simple greedy heuristic | ✅ 'Simulated annealing' heuristic minimizing empty flights |
| Plane repairs | ❌ Usually high repair costs | ✅ Low repair cost by putting planes with repair deficit temporarily out of service |
| Delays | ❌ Does not fix schedule if flight was bumped to next day | ✅ Tries to reorganize to avoid delays |
| Route ticket pricing | ❌ Only based on competitor prices | ✅ Sophisticated strategy to improve route utilization and optimize income |
| Kerosene | ❌ Does not buy kerosene at ArabAir | ✅ Buys tanks and kerosene to save money when price is low |
| Bankruptcy avoidance | ❌ Frequently goes bankrupt, especially in last mission | ✅ Able to forecast high future expenses, acts accordingly |
| Missions | ❌ Only small strategy adaptations | ✅ Intricate strategies for several missions |
| Overtake | ❌ Will not overtake competitors | ✅ Overtakes competitors if given the chance |
| Items | ❌ Only uses pliers | ✅ Uses more items |
| Sabotage | ❌ No strategy, sabotages very often and mostly randomly | ✅ Uses sabotage only in specific missions for particular goals |
| Planes | Buys only used planes | Buys only new planes |

### Statistics screen
* Showing far more categories where money was spent
   * For example income from freight jobs, total tons transported, money spent on planes, sabotage or stocks and money gained from interest, credit or stocks
* Accurate summation of money spent
   * Fixed many bugs where especially money earned / spent by competitor would not show up in balance
* Whether or not values are shown depends on skill of financial advisor (for your airline) or skill of spy (regarding competitors)
* Unlimited statistics: Store statistics data for each day since beginning of the game
* Fix rendering of graph when zooming out
* Fix display of mission target

### Information menu
* Much more information on balance sheet depending on skill of your financial advisor
* New financial summary for quick assessment of the financial health of your airline (e.g. operating profit)
* Multiple balance sheets (for previous day / week / overall)
* More information from spy (e.g. weekly balance and financial summary for each competitor)
* Detailed information from kerosene advisor (quality / value of kerosene, money saved by using tanks)

### Option menu
* Game Speed is adjustable in options menu. Available values: 1, 5, 10, 15, 20, 25, 30 (default value). The hosts game speed is synced to clients in a network game.

### Keyboard navigation
* Allow Enter/Backspace in calculator
* Enable keyboard navigation in Laptop / Globe (arrow keys)
* Enable keyboard navigation in HR folder
    * Flip pages using arrow keys
    * Jump 10/100 pages in HR files using Shift/Ctrl
    * Change salary using +/-
    * Hire/fire using Enter/Backspace
* Enable keyboard navigation in plane prop menu (arrow keys, jump using Shift/Ctrl)
* Adjust route ticket prices in larger steps (using Shift/Ctrl)
* Arrow key navigation for many different menus

### Employees
* More pilots/attendants available for hire
* Slightly increase competence of randomly generated employees
* Generate randomized advisors as well
* Regenerate unemployed employees if not hired within 7 days (prevents buildup of low-skill candidates in long games)
* List automatically sorted by skill
* Update worker happiness based on salary
    * Chance to increase/decrease happiness each day based on how much salary is higher/lower than original salary
* The 10% change when increasing/decreasing salary now always refers to the original salary
* Regularly increase worker happiness if company image is great

### Kerosene
* Adjust impact of bad kerosene:
    * Now depends on ratio of bad kerosene in tank (quadratic function now instead of yes/no)
    * Amount of plane damage due to bad kerosene increased
    * Reasoning: Before, it was very easy to save enormous amounts of money by buying 'bad' kerosene. Now, it is still possible to save money, but you will need to carefully consider how much 'bad' kerosene you put in your tanks (between 10% and 20% can work).
* Kerosene advisor gives hints on how to save money in new kerosene advisor report
* ArabAir offers much larger kerosene tanks
* Do not remember selected kerosene quality for auto purchase (was an undocumented and convoluted 'feature')

### Bug fixes
* Fixed frozen windows on laptop
* Integer overflow fixed when emitting lots of stock (resulted in loosing money when emitting)
* Fixed formula for credit limit
* Stock trading: Show correct new account balance in form (including fee)
* Fix saving/reloading of plane equipment configuration
* Fix bug in gate planning ('no gate' warning despite plenty of free gates available)
* Fix distant rendering of sticky notes in the boss office
* Use correct security measure to protect against route stealing
* Fix calculation of passenger happiness
    * Set passenger rating based on quality + small randomized delta (old code could yield just 'okay' even with high quality)
    * Passengers will tolerate high prices if quality is good
* Fix sabotage that puts leaflets into opponent's plane
    * Now passenger happiness is booked to the statistic of the sabotaging player
* 'Plane crash' sabotage now also affects stock price of victim
* Fix calculation of plane repair cost
    * All cost will show up in plane's balance
    * All cost will show up in plane repair cost total
    * Correctly calculate plane's balance over past 7 days
* Consider also number of first class passengers for statistics
* Do not show route utilization for defeated players
* New sound options (OGG/MIDI) + patched stuttering glitch when switching music on Windows 11
* Patched Space station mission prices and texts in stats
* Patched various text scrambling on UI
* Bug fixed in calculation in maximum amount of stock that can be emitted
    * Bug limited max amount of stock to around 2.1 million
    * Integer overflow is fixed now, but the originally intended limit of 250 million was changed to 2.5 million
* Fixed game shifting flights on its own even if they are already locked
    * Could previously cause double-booking of flights (income and cost booked twice)
* Fixed crash in plane designer when attaching engines to left side of tail
* Timeout for sabotage: Jobs canceled if it was not possible to execute job for some time
    * Can happen if selected plane is not used anymore by owner
    * Without this fix in a situation like this the player would never be able to use sabotage again
* Fixed bug where player can 'survive' being overtaken by skipping dialog at the right moment
* Classic mission 04 now uses correct route utilization
    * Previously, even though boss said that routes must be 20% utilized, game would check for 20% plane utilization
* Addon mission 09: Fixed counting of Uhrig flights
    * Note that computer players always have and still are cheating in this mission
* First class mission 07: Only need to have two repaired planes, not all of them in case more than two were bought
* Evolution mission 02: Only need to have five planes with full safety upgrades, not all of them in case more than five were bought
* Fixed many random crashes

### Default computer player
* Uses now same credit limit
* Uses now same rules for trading stock
    * Trading fee (100 + 10% of volume) now also for computer players (fee existed only for player)
    * Do not execute trades in steps of 1000 (this previously made stock prices worse for the human player only)
    * Align function to re-calculate stock price after trade
* Uses now same rules for emitting stock
* Remove sabotage advantages
    * Computer now has to pay for sabotage as well
    * Consider all security measures (e.g. plane crash not possible anymore if plane is protected)
    * Align calculation of ArabAir trust for player and computer
* Remove strange reduction of flight cost in calculation of image change (was a disadvantage for computer player)
* Computer player pays real price for plane upgrades
* Fixed bug that prevented computer players from using routes in most games
    * Computer players will switch to routes in most games eventually
    * Computer players however will use a small cheat that regularly improves their image
* Fixed bug where computer player buys or sells more stock than available

### Misc
* Reduce (~ half) cost of plane security upgrades
* Spy reports enemy activity based on skill
* ArabAir opens one hour earlier
* Calculate route utilization as average of previous 7 days
* Adding NOTFAIR cheat to make competitors much richer
* Adding ODDONEOUT cheat to improve image of competitors
* Adding AUTORUN cheat for ultra-fast forward
* Use player colors when showing routes on laptop
* Buy kerosene by clicking price chart
* Change tooltip of savegames (number of days played)
* Implement actual random generator using Mersenne twister
* Company value includes value of kerosene stored in tanks and tanks themselves
* Company value includes value of plane upgrades
* Company value includes value of airline image (money required to reach current image)
* Strikes will start after 9 am now to give player chance to react
* Make planes in main menu comically long
* Decryption of data files with the run argument "/savedata"
* Option "OptionTicketPriceIncrement" to increase ticket price increment per click
* Director's board now allow for more post-it
* Added options "OptionRentOffice*" to customize the branch number available / day.
* Director's board post-it system improved and allow for more cities (up to 7)

## Credits and Contributors

- [WizzardMaker](https://github.com/WizzardMaker) - Main contributor and maintainer
- [CrossVR](https://github.com/CrossVR) - Original contributor
- [mertenpopp](https://github.com/mertenpopp) - Contributor
- [wackfx](https://github.com/wackfx) - Contributor
- [CrazyOrange](https://github.com/CrazyOrange) - Contributor
- [Heftie](https://github.com/Heftie) - Contributor

