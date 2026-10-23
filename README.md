# AirlineTycoon

This repository aims to complete the partial source code that is provided as a free bonus in the GOG release
of Airline Tycoon Deluxe.

To run it, you'll need the game assets from the Deluxe edition of the game.
Therefore, you need an existing installation of the game. You can purchase the game from [GOG.com](https://www.gog.com/en/game/airline_tycoon_deluxe) or from [Steam](https://store.steampowered.com/app/331920/Airline_Tycoon_Deluxe/).

Deutsche Anleitung zu den HD-Grafiken: [README.de.md](README.de.md)

## Quick start (Windows)

1. Own and install Airline Tycoon Deluxe ([GOG](https://www.gog.com/en/game/airline_tycoon_deluxe) or
   [Steam](https://store.steampowered.com/app/331920/Airline_Tycoon_Deluxe/)).
2. Download `AT-2.0.4-windows.zip` from the [latest release](https://github.com/Kapi89/AirlineTycoon/releases/latest).
3. Unzip and copy all files into your game folder (e.g. `C:\Program Files (x86)\GOG Galaxy\Games\Airline Tycoon Deluxe`)
   – overwrite existing files.
4. Optional, for HD (4x graphics): double-click `HD_generate.bat` and confirm all questions with `Y`. It installs
   everything it needs and builds the HD files from your own game copy. Without an NVIDIA graphics card this takes
   about 12 hours; you can stop it any time with Ctrl+C and double-click again to continue. If it reports that it
   cannot write into the game folder (possible below `C:\Program Files`), right-click it and choose
   "Run as administrator".
5. Start `AT.exe` – the game now runs in widescreen, and in HD if you did step 4.

The HD files are for private use only and must not be shared. The two AI models used to create them have their own
licenses (one of them, 4x-UltraSharp, is non-commercial only). The manual way for advanced users is described in
[HD graphics](#hd-graphics) below.

## Quick start (macOS)

1. Own and install the Mac version of Airline Tycoon Deluxe ([GOG](https://www.gog.com/en/game/airline_tycoon_deluxe)).
2. Install [Homebrew](https://brew.sh) if you do not have it yet, then in the Terminal:
   `brew install sdl2-compat sdl2_image sdl2_mixer sdl2_ttf`
3. Download `AT-<version>-macos.tar` from the [latest release](https://github.com/Kapi89/AirlineTycoon/releases/latest)
   and double-click it to unpack it.
4. In the Finder, right-click `Airline Tycoon Deluxe.app` -> "Show Package Contents" and open the folder that contains
   `gli`, `room` and `misc`. Copy all unpacked files there and replace existing files.
5. Optional, for HD (4x graphics): double-click `HD_generate.command` in that folder and confirm all questions with
   `Y`. It installs what it needs (Python via Homebrew) and builds the HD files from your own game copy; on a Mac with
   Apple silicon it uses the graphics chip. You can stop it any time with Ctrl+C and double-click again to continue.
   Macs with Intel processors are not supported for HD.
6. Start `AT` from that folder (in the Terminal: `./AT`). If macOS refuses to open `AT` or `HD_generate.command`
   because it was downloaded from the internet, right-click it and choose "Open" once.

HD and widescreen are not yet confirmed in the game on the Mac; feedback is welcome as an issue.

## Quick start (Linux)

1. Own and install Airline Tycoon Deluxe (the game data from the GOG or Steam version: the folder with `gli`, `room`
   and `misc`).
2. Install SDL2, e.g. on Debian/Ubuntu:
   `sudo apt install libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0`
3. Download `AT-<version>-linux.tar` from the [latest release](https://github.com/Kapi89/AirlineTycoon/releases/latest)
   and unpack it into the game folder: `tar -xf AT-<version>-linux.tar -C "/path/to/the/game folder"`.
4. Optional, for HD (4x graphics): in a terminal in the game folder run `./HD_generate.sh` and confirm all questions
   with `Y`. It installs what it needs (Python via your package manager, after asking) and builds the HD files from
   your own game copy, on an NVIDIA graphics card if there is one. You can stop it any time with Ctrl+C and run it
   again to continue.
5. Start `./AT` in the game folder.

## Major Additions

- Many new computer players 
    - you can choose from many different play styles
    - from an easier laid-back competitor to brutal rival
    - from "fair play" to cruel nemesis who will target you specifically
- Massive multiplayer overhaul to reduce desyncs and improve stability
- Native Linux support
- Dedicated server browser and NAT-punchthrough multiplayer (open source server at: https://github.com/WizzardMaker/ATDMasterServer)
- New computer player that plays well without cheating
- Widescreen and optional 4x HD graphics generated from your own copy of the game
- Support for macOS, including native Apple Silicon builds

## License

The code in the repository is licensed under the terms included in the GOG release. As such the code can only be used for non-commercial purposes and remains property of BFG.

It is therefore *not* open-source in the free software sense, for more information refer to the License.txt.

## Installation

1. Buy and install a version of the game (for example on [GOG.com](https://www.gog.com/en/game/airline_tycoon_deluxe) or on [Steam](https://store.steampowered.com/app/331920/Airline_Tycoon_Deluxe/))
2. Download the latest release of this mod for your platform (Windows, macOS or Linux)
3. Unpack the archive into the folder where the old game executable is (for example At.exe on Windows)
4. Start the game using the new game executable (AT.exe on Windows, AT on macOS and Linux)
5. Enjoy!

## HD graphics

Since version 2.0.0 the game can draw all graphics at 4x resolution. The HD graphics are **not** part of the
download: everyone creates them from their own copy of the game. On Windows a double-click on `HD_generate.bat` does
everything. Without HD graphics the game runs as before.

### Licenses: please read first

- **Own the game.** The generator only works with your own installation of Airline Tycoon Deluxe (GOG or Steam).
  The original graphics belong to BFG / Spellbound.
- **Private use only.** The generated images are derived from the original graphics. Do not share, upload or sell
  them.
- **Models:** `4x-UltraSharp` is licensed under CC BY-NC-SA 4.0 and may be used **non-commercially only**.
  `RealESRGAN_x4plus` is licensed under BSD-3-Clause. Neither is in the repository or in the download.
  `HD_generate.bat` shows the licenses and only downloads the models after you confirm with `Y`; each file is checked
  against its SHA-256 checksum to make sure it is the author's original file. `tools/hd_generate.py` itself never
  downloads anything.

### Recommended: one double-click (Windows)

1. **Install the game and the latest release** (2.0.4 or later) as described in [Installation](#installation):
   unpack the release archive into the game folder (the one with `gli`, `room` and `misc`). It then contains
   `AT.exe`, `HD_generate.bat` and the folder `hd_tools`.
2. **Double-click `HD_generate.bat`.** If Windows asks ("Windows protected your PC"), click "More info" and "Run
   anyway". The script does the rest and shows "Step x of 8"; confirm questions with `Y`:
   - checks the game folder, `AT.exe` and the game data,
   - looks for Python 3.10 to 3.13; if it is missing, it offers to install it with `winget` (for your user only) or
     tells you what to do,
   - sets up its own Python environment in `hd_tools\venv`, with CUDA if an NVIDIA graphics card is found
     (`nvidia-smi`),
   - shows the licenses of the two models and downloads them into the folder `models` after you type `Y`,
   - exports the animated people in the rooms (`AT.exe --hd-export alle`, no window). If `AT.exe` stops during the
     export, the script starts it again and skips the clip concerned; if the export does not work at all, it computes
     everything else and lists at the end what stays in normal resolution,
   - computes all HD graphics into `hd` (on the graphics card if possible) and shows the estimated remaining time,
   - sets `"OptionRenderScale": 4` in `AT.json` and asks whether to delete the temporary files (`hd_assets`) and the
     Python environment.
3. **Start the game.** Done.

Duration: without an NVIDIA graphics card about 12 hours (measured: 11:46 h for 14,343 images on a fresh GOG copy;
the animations are the largest part, more processor cores help), much faster with an NVIDIA graphics card. **Stop**
at any time with Ctrl+C or by closing the window; the next double-click continues where it stopped. Disk space:
about 15 GB during the run, afterwards mainly `hd` remains. If something goes wrong, please send the file
`hd_generate.log` from the game folder.

If the game is installed below `C:\Program Files`, not every user may write there: right-click `HD_generate.bat` and
choose "Run as administrator".

### Linux and macOS: HD_generate.sh

`HD_generate.sh` lies next to `AT` in the Linux and macOS release (on the Mac also `HD_generate.command` for a
double-click in the Finder). It does the same 8 steps as the Windows script, with the same log file
`hd_generate.log`:

- Python 3.10 to 3.14 with the module `venv`: if it is missing, the script offers to install it with the package
  manager (`apt`, `dnf`, `pacman`, `zypper`; asks for your password via `sudo`) or on the Mac with Homebrew, otherwise
  it tells you what to install.
- `torch`: on Linux with an NVIDIA card the CUDA build that fits the driver, otherwise the CPU build; on a Mac with
  Apple silicon the normal build, which computes on the Apple graphics chip (MPS). Macs with Intel processors are not
  supported by current `torch` versions (see the manual way below).
- The animations are exported with `./AT --hd-export alle`; no display is needed.
- Options: `--yes` (answer everything with yes), `--only schriften,stadtfotos`, `--cpu`, `--keep` (delete nothing at
  the end), `--game FOLDER`.

On the Mac, `AT`, `HD_generate.sh` and the folder `hd_tools` must be in the data folder inside the `.app` bundle (see
[macOS](#macos)). If macOS refuses to open `HD_generate.command` or `AT` because they were downloaded from the internet,
right-click and choose "Open" once; the script also offers to remove this block for `AT`.

### For advanced users: manual way (Windows, Linux, macOS)

1. **Install the game and the latest release** as described in [Installation](#installation). The new `AT.exe` (or
   `AT`) is then in the game folder.
2. **Install Python** (3.10 to 3.13): on Windows from [python.org](https://www.python.org/downloads/) (tick "Add
   python.exe to PATH" in the installer), on Linux with the package manager, on macOS e.g. with Homebrew.
3. **Get the generator:** it is in the folder `hd_tools` of the release, or download this repository as ZIP (green
   button "Code" -> "Download ZIP") and use the folder `tools/`. The commands below use `tools/`.
4. **Install the Python packages** (in a console in the unpacked folder):

   ```sh
   pip install -r tools/hdgen/requirements.txt
   ```

   With an NVIDIA graphics card, first install `torch` with CUDA as described on
   [pytorch.org](https://pytorch.org/get-started/locally/); everything is much faster then. On a Mac with Apple
   silicon (M1 and later) the normal `torch` already supports the graphics chip (MPS).
5. **Get the models** and put them into a folder `models/` next to `AT.exe`:
   - `4x-UltraSharp.pth` from [OpenModelDB](https://openmodeldb.info/models/4x-UltraSharp) (Kim2091,
     CC BY-NC-SA 4.0, non-commercial only; SHA-256 `a5812231fc936b42af08a5edba784195495d303d5b3248c24489ef0c4021fe01`):
     rooms, user interface, hall bricks and fonts
   - `RealESRGAN_x4plus.pth` from the [Real-ESRGAN release v0.1.0](https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.1.0)
     (BSD-3-Clause; SHA-256 `4fa0d38905f75ac06eb49a7951b426670021be3018265fd191d2125df9d682f1`): city photos, people
     in the hall and animated people in rooms

   If the generator does not find a model, it asks for the path (or use `--ultrasharp FILE`, `--x4plus FILE`,
   `--modelle FOLDER`).
6. **Export the animated people in rooms** (sales people, advisers etc. are videos; the game exports them itself to
   `hd_assets/original/video` and then quits):

   ```sh
   AT.exe --hd-export alle
   ```

   Single rooms instead of `alle`, e.g. `AT.exe --hd-export duty,bank`. Alternatively set
   `"OptionHdVideoExport": "alle"` in `AT.json` and start the game once.
7. **Run the generator** with the game folder as target:

   ```sh
   python tools/hd_generate.py "C:/GOG Games/Airline Tycoon Deluxe" --alles --gpu --english
   ```

   It exports the graphics, upscales them 4x and puts them into the folder `hd/` next to `AT.exe`. Temporary files
   are in `hd_assets/` next to it (can be deleted afterwards).
   - Without a graphics card the whole package takes many hours; stop with Ctrl+C at any time, the next start
     continues. Identical images are computed only once.
   - Without `--alles` the generator computes everything except the animations. Only parts: `--nur schriften`
     (fonts), `--nur stadtfotos` (city photos), `--nur ui`, `--nur raeume` (rooms), `--nur bausteine` (hall bricks),
     `--nur figuren` (people in the hall), `--nur animationen --raum duty` (rooms see
     [ASSETS.md](ASSETS.md#animierte-personen-in-raeumen-export-und-zaehlung-h25a)), also combined, e.g.
     `--nur schriften,stadtfotos`. Fonts and city photos are quick and show right away whether everything works.
   - Hall bricks get their edges from the model (smoother edges, default since 2.0.4); `--bausteine-lanczos` computes
     them as before.
   - `--gpu` computes on the graphics card: CUDA (NVIDIA, Windows/Linux) or MPS (Mac with Apple silicon), otherwise
     on the CPU. `--liste` only shows what is to do. `--log FILE` also writes all output to a file, `--english`
     prints the messages in English.
   - The result is reproducible: the same game copy, models and CPU give the same images.
8. **Switch it on in the game:** set `"OptionRenderScale": 4` in the file `AT.json` next to `AT.exe` (the file is
   created when the game starts for the first time).

### Options in AT.json

`AT.json` is next to `AT.exe` (or `AT`) and is created when the game starts for the first time.

| Option | Value | Meaning |
|---|---|---|
| `OptionRenderScale` | `4` | Use the HD graphics. Must be `4`, because the package is 4x (default `1` = off) |
| `OptionWidescreen` | `1` / `0` | Widescreen on (default) or off (original 4:3 picture) |
| `OptionWidescreenRoomBorder` | `1` / `0` | Borders next to rooms blurred (default) or black |
| `OptionHdVideo` | `1` / `0` | Animated people in rooms in HD if `hd/video` exists (default), or always 1x |
| `OptionHdVideoMaxMB` | `1024` | Graphics memory per animated person; if it does not fit, the whole person is shown in 2x |
| `OptionHdVideoCacheMB` | `1536` | Graphics memory for animated people of rooms you left; HD at once when you come back (`0` = off). Lower it with little graphics memory (integrated graphics) |

Only edit `AT.json` while the game is closed: the game rewrites the file when it quits.

**Tool and diagnostic options** (normally not needed; details in [ASSETS.md](ASSETS.md)):

| Option | Default | Purpose |
|---|---|---|
| `OptionHdVideoExport` | `""` | Tool: export the room animations for the generator at start (`"alle"` or rooms), resets itself afterwards; easier: `AT.exe --hd-export alle` |
| `OptionHdOverlayFilter` | `0` | Fine tuning: filter of 1x remains over HD backgrounds (`0` sharp, `1` soft) |
| `OptionHdMissingLog` | `false` | Diagnostics: log every graphic without HD image once |
| `OptionHdFrameStats` | `0` | Diagnostics: frame times and mouse cursor position in the log every 5 s |
| `OptionCursorExitAnim` | `1` | Diagnostics: `0` = exit arrow does not move (to narrow down stutter) |
| `OptionHdDebugMask` | `false` | Diagnostics: mask images to `hd_debug/` every 5 s |
| `OptionHdPolCheck` | `0` | Diagnostics: at start, check the exported hall figures against the engine |

### macOS

> **HD and widescreen are untested on the Mac** until someone confirms them in the game. Both are only tested in the
> real game on Windows. Feedback is welcome as an issue.

1. **Python:** either with [Homebrew](https://brew.sh) (`brew install python`) or with the installer from
   [python.org](https://www.python.org/downloads/macos/). Best install the packages into their own environment
   (required with Homebrew Python, which otherwise refuses `pip install`):

   ```sh
   python3 -m venv ~/at-hd
   source ~/at-hd/bin/activate
   pip install -r tools/hdgen/requirements.txt
   ```

   Current `torch` versions no longer exist for Macs with Intel processors; there it only works with an older Python
   (up to 3.12) and `torch` 2.2, without graphics card (untested).
2. **Game data of the GOG Mac version:** the game is an `.app` bundle, e.g. `/Applications/Airline Tycoon
   Deluxe.app`. The game data are inside it in a folder with `gli/`, `room/` and `misc/` (right-click the app ->
   "Show Package Contents"; the exact path depends on the version). The generator finds this folder itself:

   ```sh
   python3 tools/hd_generate.py "/Applications/Airline Tycoon Deluxe.app" --gpu --english
   ```

3. **Where does `hd/` belong?** The Mac build is a single program `AT` (no `.app`). The game looks for its data,
   `AT.json` and `hd/` in the folder that contains `AT`. So `AT` belongs into the data folder (the one with `gli/`),
   and that is exactly where the generator puts `hd/`. If there is no `AT` yet, it names the folder.
4. **Graphics on the Mac:** the game draws via SDL, on the Mac with **Metal** by default. For comparison it can be
   started with OpenGL: `SDL_RENDER_DRIVER=opengl ./AT`. On Retina displays the game currently draws in point
   resolution (e.g. 1440x900 instead of 2880x1800); HD is sharper than 1x there, but does not use the full Retina
   resolution yet.

### Linux

Easiest is `./HD_generate.sh` (see above). By hand: `./AT --hd-export alle`, then
`python3 tools/hd_generate.py <game folder> --alles --gpu` (CUDA with an NVIDIA card).

Technical details (methods, manifest, file names) are in [ASSETS.md](ASSETS.md).

## Building

This is for the experts who want to contribute to development.

Check out the repository as usual. Before building, remember to clone the submodules:
```
git submodule update --init
```

You can see the instructions to build and run the project in [BUILDING.md](BUILDING.md).

## All changes

### Version 2.0.5
* Merged the upstream version 1.9.2 by mertenpopp (computer player, multiplayer and many fixes). From its release
  notes:

  The new bots got a lot tougher! Update and see if you can still keep up.

  The computer opponent has been rebuilt from the ground up. In our test games it now earns seven times as much in the first 60 days as it did in 1.9.0 and ends with a fleet of around 70+ planes instead of 10+.

  What you'll notice:

  * It expands fast. It opens routes early, borrows to grow and buys several planes a day once the money flows.
  * It runs a tight airline. Smarter flight planning, better ticket prices and advertising that is bought when it pays off.
  * It plans ahead. It hires crews before it needs them, so it doesn't stall when its fleet explodes late in the game.
  * It plays dirty when allowed. On the harder levels it sabotages and it will take you over if you leave the door open.
  * It still plays fair. No cheating: Same rules, acts only on same information available to you.

  **If 1.9.0's bot felt like a sparring partner, this one is a rival. Good luck.**

* One-click HD on Linux and macOS: `HD_generate.sh` next to `AT` (on the Mac also `HD_generate.command` for a
  double-click), the same steps and log as `HD_generate.bat` on Windows. Tested in CI on Ubuntu and macOS 15.

### Version 2.0.4
* One-click HD on Windows: `HD_generate.bat` (next to `AT.exe`) checks the game folder, installs Python with `winget`
  if needed (after asking), sets up its own Python environment (with CUDA on NVIDIA cards), downloads the models after
  you confirm their licenses (checked by SHA-256), exports the room animations, computes all HD graphics and switches
  the game to HD. It can be stopped with Ctrl+C and resumes on the next double-click; log in `hd_generate.log`. All
  messages are in English.
* New start parameter `AT.exe --hd-export [alle|rooms]`: exports the room animations for the generator without a
  window and quits. An error in one clip no longer ends the export; the next start continues and skips that clip.
* Hall bricks: edges computed by the model (smoother) are now the default; `--bausteine-lanczos` restores the old way.
* Fixed crashes in the Smacker video decoder (libsmacker): empty Huffman trees and frame sizes that are not a multiple
  of 4 (heap overflow). This also affected playing such videos in the game.
* Generator: `--alles` (everything incl. animations), `--log FILE`, `--english`.
* Build: the release archives contain the generator in `hd_tools/`; the macOS build runs on `macos-15` (the
  `macos-latest` runners were often unavailable) and is retried automatically if no runner was found. Preview builds
  are published as pre-releases, so "Latest release" always points to the real release.

### HD graphics and widescreen (2.0.0 - 2.0.4)
* Optional 4x HD graphics, generated locally from your own copy of the game (see [HD graphics](#hd-graphics); nothing
  copyrighted is distributed): rooms, UI, fonts, city photos, airport hall bricks, people in the hall, thought bubbles
  and, since 2.0.3, the animated people in rooms; since 2.0.4 with one double-click on Windows
* Widescreen: the airport hall and rooms with a status bar use the full 16:10 / 16:9 width; soft borders next to rooms
* 2.0.3: animated people in rooms in HD (loaded in the background, cached across visits, whole figure in 2x if it does
  not fit the memory limit), fixed mouse cursor jumps and shifted click areas in widescreen rooms

### General
* Now runnable on Linux (including support for FLC animations found in some versions)
* Can run on MacOS as well
* Game can be configured for windowed / borderless / fullscreen display
* Language can be selected in the options menu (Misc page)

### New computer player

When starting a new game, you can pick a play style for each computer-controlled airline. I tried to make each one feel different to play against:

* **Classic**: The original computer player you know from the base game. It is cheating (see the table below), but it is not a particularly clever manager. Pick it for the nostalgia.
* **Laid-Back**: The nicest of the new competitors. It plays by the rules and builds a proper airline, but it strolls through the airport instead of running, is not picky about which jobs it takes, charges modest ticket prices and does not sabotage or take over competitors. A good first opponent if you want to learn the game or just have a relaxed round.
* **Freight Baron**: A bit tougher competitor with a passion for cargo. It heads for the freight office first and fills its fleet with freight jobs. A fair opponent with a noticeably different rhythm than the others. Also does not sabotage nor take over competitors.
* **Route Tycoon**: A pure profit machine, fair but relentless. It never uses sabotage. Instead it starts building a huge route network almost immediately, trades kerosene cleverly and keeps an eye on the stock market, where it will happily buy up a struggling rival. Keep hold of your own shares if you want to see the end of the game.
* **Challenger**: The best strategist. Picks only lucrative jobs to then bootstrap a route network, pushes ticket prices as far as the passengers tolerate and stocks up on cheap kerosene. Compared to the *Route Tycoon*, it expands its route network slower and will always fully operate the most profitable routes. It never plays dirty, so expect a tough but honest race for the top spot.
* **Saboteur**: Runs its airline just as well as the *Challenger*, but is also a regular customer of the saboteur. Expect trouble with your aeroplanes and your office whenever you pull ahead, so a visit to the security office might be money well spent. It will also employ stock warfare in an attempt to take you over.
* **Nemesis**: Everything the *Saboteur* does, and it is personal. The Nemesis singles out the human player with extra dirty tricks that no other level uses. Pick it if you want an opponent who is out to ruin your day, not just to win.

All new computer players have one thing in common:

**They always respect the rules of the game**

| Rules | Old computer player | New computer players |
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
* About twice as many advisors and flight attendants are up for hire (#26).
* Slightly increase competence of randomly generated employees
* Generate more and also randomized advisors
* Advisors are now drawn per type with weights depending on usefulness
* Regenerate unemployed employees if not hired within 7 days (prevents buildup of low-skill candidates in long games)
* List automatically sorted by skill
* Update worker happiness based on salary
    * Chance to increase/decrease happiness each day based on how much salary is higher/lower than original salary
* The 10% change when increasing/decreasing salary now always refers to the original salary
* Regularly increase worker happiness if company image is great

### Planes
* Reduce (~ half) cost of plane security upgrades
* Game now supports modded plane types with build years after 1998 (#13).
    * Plane build years now count from 2026 instead of 2002
    * Old saves are compatible, plane build years are shifted on load
* First-class seating changes now take effect when the plane is on the ground.
* Plane maintenance costs reset daily. Old value showed total cost per plane which makes it impossible to compare repair costs unless planes where bought on the exact same day.

### Kerosene
* Adjust impact of bad kerosene:
    * Now depends on ratio of bad kerosene in tank (quadratic function now instead of yes/no)
    * Amount of plane damage due to bad kerosene increased
    * Reasoning: Before, it was very easy to save enormous amounts of money by buying 'bad' kerosene. Now, it is still possible to save money, but you will need to carefully consider how much 'bad' kerosene you put in your tanks (between 10% and 20% can work).
* Kerosene advisor gives hints on how to save money in new kerosene advisor report
* ArabAir offers much larger kerosene tanks
* Do not remember selected kerosene quality for auto purchase (was an undocumented and convoluted 'feature')

### Music
* New sound options (OGG/MIDI)
* Patched stuttering glitch when switching music on Windows 11
* All 16 original MIDI tracks are now played; before it was 8 (#37).
* Add your own music: Game will play all OGG and all MIDI files found in the game directory

### Multiplayer
* Massive sync overhaul covering staff, salaries, strikes, items, kerosene, flight plans, gate/city bids, plane problems, the game clock and bankruptcy.
* Hardening against malformed messages, a use-after-free fix and several crash fixes.
* The game calendar uses UTC in network games.
* The game version is now checked when rejoining a saved game.
    * Preview builds can no longer join games running the release build, because peers now also compare a network protocol version.
* The ENET connection type was removed because it didn't work.

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
* Addon mission 05: Formula for luxury score changed
* Addon mission 09: Uhrig flights are now all in the region of the home airport
* Addon mission 09: Fixed counting of Uhrig flights
    * Note that computer players always have and still are cheating in this mission
* First class mission 07: Only need to have two repaired planes, not all of them in case more than two were bought
* Evolution mission 02: Only need to have five planes with full safety upgrades, not all of them in case more than five were bought
* Fixed crash when quickly clicking through dialog options
* Stocks are traded in chunks of 2000 with the price updated after each chunk (exploit fix)
* Food costs now include first-class passengers
* Dragging a flight in the flight planner could make it start before its plane arrived (#44).
* Fixed bug where computer player buys or sells more stock than available
* Fix bug in fare price for first-class passengers not being taken over when flight plan is automatically extended
* Show correct price for auctions (gates and branch offices). Real price is three times monthly rent, previously, only one time the monthly rent was displayed
* Fixed many random crashes

### Default computer player
* Max. range of a plane now also has to be considered by the computer player
* Max. number of seats now also has to be considered by the computer player
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

### Misc
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
* Safety upgrades now protect against the intended problem
    * Spring: Cockpit upgrade protect against "Migrating birds, due to the cockpit"
    * Summer: Safety upgrade protect against "Missing life vests"
    * Fall: Engine upgrade protect against "Engine damage, due to foliage"
    * Winter: Tire upgrade protect against "Landing gear frozen solid"
* High/low route fares for first class now also affect image
* Changed presets for route fares:
    * Old dumping price did nothing: Earned the same image bonus as the next preset but at only half the income
    * Presets are now somewhat price-optimal: They sit right below the next threshold where the customers would become less satisfied with you
    * You can now easily charge much more than before (which is possible if you have enough image)
    * Includes the prices that the new computer players charge
    * First-class presets are 3x economy prices. Reason: Since one FC passenger takes the place of two economy passengers, at only 2x the price you would gain nothing
* Aligned how last-minute flight plan changes affect first-class route passengers

## Credits and Contributors

- [WizzardMaker](https://github.com/WizzardMaker) - Main contributor and maintainer
- [CrossVR](https://github.com/CrossVR) - Original contributor
- [mertenpopp](https://github.com/mertenpopp) - Contributor
- [wackfx](https://github.com/wackfx) - Contributor
- [CrazyOrange](https://github.com/CrazyOrange) - Contributor
- [Heftie](https://github.com/Heftie) - Contributor

