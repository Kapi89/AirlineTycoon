# Spieldateien, HD-Grafiken und KI-Modelle

Dieses Repository enthaelt nur Quellcode und Werkzeuge. Folgendes gehoert **nie** ins Git:

- Originaldateien von Airline Tycoon Deluxe (`.gli`, `.glj`, `.smk`, `.lbm`, `.mcf`, `.res`, `.raw`, Spieldaten-CSV usw.).
  Sie sind urheberrechtlich geschuetzt (BFG / Spellbound).
- Daraus erzeugte Grafiken, z. B. per KI hochskalierte HD-Bilder (Ordner `hd/`, `hd_assets/`, `upscaled/`, `export/`).
  Sie bleiben abgeleitete Werke der Originalgrafiken.
- KI-Modelle wie `4x-UltraSharp.pth` (Lizenz CC BY-NC-SA 4.0, nur nicht-kommerziell), Ordner `models/`.

Jede Person erzeugt die HD-Grafiken lokal aus ihrer eigenen, gekauften Spielkopie.

## Schutzmechanismen

1. `.gitignore` schliesst Spieldateien, Asset-Ordner (`hd/`, `hd_assets/`, ...) und Modelle (`models/`, `*.pth`, ...) aus.
2. Lokaler Hook blockiert Commits: `git config core.hooksPath tools/hooks`
3. GitHub-Workflow `asset guard` prueft jeden Push und Pull Request.

Pruefung von Hand: `python3 tools/asset_guard.py`

## HD-Paket mit dem Generator erzeugen (ab 2.0.0)

Schritt-fuer-Schritt-Anleitung fuer Spieler und Lizenzhinweise: siehe [README.md](README.md#hd-grafiken-ab-version-200-anleitung-auf-deutsch).
Kurz:

```sh
pip install -r tools/hdgen/requirements.txt
python3 tools/hd_generate.py /pfad/zum/spiel                 # alles
python3 tools/hd_generate.py /pfad/zum/spiel --nur schriften   # oder stadtfotos, ui, raeume, bausteine
```

Lizenz in einem Satz: Spiel selbst kaufen, das erzeugte Paket nur privat nutzen und nicht weitergeben,
`4x-UltraSharp` nur nicht-kommerziell (CC BY-NC-SA 4.0).

### Ablauf

1. **Export** aus der Spielkopie mit `tools/gli_export.py` und `tools/mcf_export.py` nach `hd_assets/original/`
   (nur die Archive, die fuer `--nur` gebraucht werden).
2. **Verfahren** je PNG aus `tools/hdgen/manifest.json`: zuerst der Eintrag zum Chunk (`<ordner>/<datei>/<CHUNK>`),
   dann die erste passende Regel (Muster wie `gli/*1.gli/*`), sonst `auto`. Schriften stehen unter `schriften`;
   eine Schrift ohne Eintrag bekommt `schrift_standard` (Hinweis im Lauf).
3. **Hochrechnen** (4x) mit den Verfahren aus `tools/hdgen/hd_methods.py`. Das ist die Referenz-Implementierung, mit
   der das abgenommene Paket erzeugt wurde, nur um die Wahl CPU/CUDA ergaenzt.
4. **Ablegen** als `hd/<ordner>/<datei>/<CHUNK>.png` bzw. `hd/<ordner>/<schrift>.mcf.png` neben der `AT.exe`
   (gesucht im Spielordner und eine Ebene tiefer; sonst der Spielordner, oder `--ausgabe`).

Gleiche Eingaben werden nur einmal gerechnet: Schluessel ist die MD5 der Eingabe-PNG plus Verfahren, Modell und
Schrift-Parameter, gemerkt in `hd_assets/hd_cache.jsonl`. Vorhandene Ausgaben werden uebersprungen; jede PNG wird
erst unter einem Zwischennamen geschrieben und dann umbenannt, ein Abbruch hinterlaesst also keine halben Dateien.

### Verfahren

| Verfahren | Wofuer | Ergebnis |
|---|---|---|
| `deckend_rgb` | Raum-Hintergruende | Modell auf das ganze Bild, RGB |
| `deckend_rgba` | deckend gezeichnete Sprites | Modell, Alpha 255 |
| `colorkey_rgba` | Sprites mit Colorkey, UI, Kiosk, Hallen-Bausteine | Schwarz = durchsichtig; Loecher aus Nachbarn auffuellen, Modell; Alpha = Lanczos-Maske, geschaerft |
| `foto_rgb` | Stadtfotos `gli/*1.gli`, `gli/*1.glj` | Modell `x4plus`, RGB, reines Schwarz -> (1,1,1) |
| `auto` | alles ohne Eintrag | `colorkey_rgba`, wenn das Bild reines Schwarz enthaelt, sonst `deckend_rgba` |
| `schrift_standard` | norm_*, small_*, stat_*, status, cond_bl, dlgfont3 | Form per Modell auf der Binaermaske, Farbe per Modell (> 3 Farben) oder bikubisch |
| `schrift_projektion` | inkfont, dlgfont1, dlgfont2 | Tintenanteil per Projektion Tinte/Hintergrund, per Modell vergroessert |
| `schrift_bikubisch` | bank_bl, bank_ro | wie Projektion, aber bikubisch (kein Modell) |

Bilder kleiner als 4 px werden pixelgenau vergroessert. Kategorien fuer `--nur`: `schriften` (alle `.mcf`),
`stadtfotos` (Verfahren `foto_rgb`), `bausteine` (`gli/glbrick*`), `raeume` (alles unter `room/`), `ui` (der Rest).

### Modelle

| Kuerzel | Datei | Quelle | Lizenz | Fuer |
|---|---|---|---|---|
| `ultrasharp` | `4x-UltraSharp.pth` | [OpenModelDB](https://openmodeldb.info/models/4x-UltraSharp) (Kim2091) | CC BY-NC-SA 4.0, nur nicht-kommerziell | alles ausser Stadtfotos |
| `x4plus` | `RealESRGAN_x4plus.pth` | [Real-ESRGAN v0.1.0](https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.1.0) | BSD-3-Clause | Stadtfotos |

Gesucht wird in `--modelle`, dann in `models/` im aktuellen Verzeichnis, im Repository, neben der `AT.exe` und im
Spielordner. Fehlt ein Modell, fragt der Generator nach dem Pfad bzw. nennt Quelle und Lizenz und bricht ab; er
laedt nichts herunter. Zu jedem Modell steht die SHA-256 im Lauf, damit sich Ergebnisse vergleichen lassen.
Geladen wird mit `spandrel`, gerechnet mit `torch`, kachelweise (128 px, Rand 8 px).

### Reproduzierbarkeit

Feste Seeds, deterministische torch-Algorithmen und eine feste Thread-Zahl (`--threads`, Standard 4 wie beim
Referenzpaket). Auf der CPU ergeben gleiche Eingaben und Modelle bitgleiche PNGs. Mit `--gpu` (CUDA) sind die
Ergebnisse gleichwertig, aber nicht bitgleich zur CPU.

### macOS (Stand H18, ungetestet im Spiel)

- **Generator:** laeuft unter macOS wie unter Windows/Linux (nur `os.path`, keine festen Trenner). `--gpu` waehlt CUDA,
  sonst MPS (Apple-Chip, mit `PYTORCH_ENABLE_MPS_FALLBACK=1`), sonst CPU. Gesucht wird der Datenordner (mit `gli/`) bis
  8 Ebenen tief, also auch im `.app`-Paket; `hd/` kommt dorthin, weil das Spiel `hd/` neben `AT` sucht
  (`SDL_GetBasePath`).
- **Renderer:** Die Engine waehlt keinen Treiber (`SDL_CreateRenderer(-1, ...)`); SDL nimmt auf dem Mac **Metal**
  (SDL2 und SDL3), unter Windows Direct3D. Der Metal-Treiber von SDL kann alles, was die HD-Ebene braucht:
  Zwischenziele (`SDL_TEXTUREACCESS_TARGET`, ARGB8888 nativ), die eigenen Blend-Modi der Sprechblasen
  (ZERO/SRC_ALPHA/ONE/ONE_MINUS_SRC_ALPHA mit ADD), Farbmodulation (Raumraender), lineare Filterung,
  `SDL_RenderReadPixels` (F11) und Texturen bis 16384 px. RGB565 (1x-Bild) ist auf Metal nicht nativ und wird von
  SDL umgewandelt, wie schon vor HD.
- **sdl2-compat:** Der Mac-Build nutzt per Homebrew `sdl2-compat`, also SDL3 unter der SDL2-Schnittstelle. Alle
  HD-Harness-Tests (Sprechblasen, BlitTrans, Leuchtrand, Breitbild, Raumraender, Statuszeile) laufen damit unter
  OpenGL und Software gleich wie mit SDL2, bis auf Rundung (hoechstens 2 von 255 Farbstufen). Dabei gefunden und
  behoben: `SDL_ConvertSurface` auf einer RLE-kodierten Spielgrafik laesst in `sdl2-compat` einen veralteten
  Pixelzeiger zurueck, der naechste Zugriff fuehrte zum Absturz (Sprechblasen, Logos in Figuren). Der HD-Code kopiert
  Grafiken jetzt ueber Sperre und Zeilenkopie (`HdCopySurface`). Metal selbst konnte nicht getestet werden.
- **Retina:** Das Fenster wird ohne `SDL_WINDOW_ALLOW_HIGHDPI` erzeugt; das Spiel zeichnet daher in Punkt-Aufloesung,
  macOS skaliert hoch. HD bringt trotzdem mehr Schaerfe, aber nicht die volle Retina-Aufloesung.

## HD-Grafiken von Hand erzeugen und verwenden

1. Bilder aus der eigenen Spielinstallation exportieren (nur Python-Standardbibliothek noetig):

   ```sh
   python3 tools/gli_export.py /pfad/zum/spiel -o hd_assets/original
   ```

   Ergebnis: `hd_assets/original/<ordner>/<datei>/<chunkname>.png`, z. B. `gli/glstd.gli/MENU.png`.
   `<ordner>` und `<datei>` sind kleingeschrieben; Sonderzeichen im Chunknamen werden zu `_`.
2. PNGs bearbeiten bzw. hochskalieren (ausserhalb des Repositorys oder in einem ignorierten Ordner).
3. Ergebnis als `hd/<ordner>/<datei>/<chunkname>.png` neben die `AT.exe` legen.

Beim Laden einer `.gli`/`.glj`-Datei sucht die Engine `hd/<ordner>/<datei>/` im Programmordner.
Gibt es dort eine PNG zum Chunk, haengt es von ihrer Groesse ab, was passiert:

- **Genau s-fache Originalgroesse** bei `OptionRenderScale` = s (2 bis 4, z. B. 4x): Das Original bleibt
  die 1x-Grafik des Spiels, das PNG wird als HD-Textur auf der GPU gezeichnet (Phase 2).
  Beispiele bei s = 4: Raum-Hintergrund und Raum-Sprites `hd/room/kiosk.gli/SLEEPER.png`,
  Hallen-Bausteine `hd/gli/glbrick<n>.gli/<CHUNK>.png`.
  - Mit Alphakanal (RGBA) gilt dieser als Maske (weiche Kanten nach Wunsch). Die Farbe unter
    durchsichtigen Stellen ist egal; die Engine fuellt sie fuer die Filterung aus den Nachbarn auf.
  - Ohne Alphakanal (RGB) wird die Maske aus dem Original berechnet: Pixel mit Farbe 0 (Schwarz)
    sind durchsichtig, die Kante wird weich auf die s-fache Groesse gerechnet.
  - Wird eine Grafik im Spiel nachtraeglich bemalt (z. B. Text), erkennt die Engine das und zeigt sie in 1x.
- **Genau Originalgroesse:** ersetzt das Originalbild direkt (auch ohne GPU-Ebene). 16-, 24- und 32-Bit;
  die Farben werden auf das Format des Originals reduziert (meist RGB565). Ein Alphakanal wird hier
  ignoriert; transparente Stellen muessen Schwarz bleiben (Colorkey).
- Andere Groessen werden ignoriert (Meldung im Log).

### Schriften (HD)

Die Schriften (`.mcf`) sind Glyphenblaetter: alle Zeichen untereinander, jedes gleich gross.

```
python3 tools/mcf_export.py /pfad/zum/spiel -o hd_assets/original
```

Ergebnis: `hd_assets/original/<ordner>/<datei>.png`, z. B. `misc/norm_bl.mcf.png` (RGBA, durchsichtig wie im
Spiel). Die HD-Fassung liegt als `hd/<ordner>/<datei>.png` neben der `AT.exe`, z. B. `hd/misc/norm_bl.mcf.png`,
in **genau s-facher Groesse** des Blatts und im selben Layout (Zeichen untereinander). Mit Alphakanal gilt dieser,
sonst wird die Maske aus dem Original berechnet. Jede Schriftdatei braucht ihre eigene PNG (z. B. `norm_bl`,
`norm_rt`, `norm_wh` unterscheiden sich nur in der Farbe).

### Stadtfotos (HD)

Die Fotos im Globus/Filofax bzw. Laptop liegen als `hd/gli/<stadt>1.gli/<STADT>n.png` (bzw. `.glj`), 4-fach,
RGB ohne Alphakanal. Das Spiel ersetzt im Foto Schwarz durch (fast) Schwarz, damit es deckend gezeichnet wird;
die HD-Fassung wird dann ebenfalls deckend verwendet (reines Schwarz in der PNG am besten als (1,1,1)). Im Log
steht je Foto einmal `Stadtfoto <name>: in HD` bzw. `in 1x`.

### Diagnose (HD-Ebene)

- **F11** (bei `OptionRenderScale` > 1): Der naechste Frame wird nach `hd_debug/` gespeichert:
  `hd_f11_<n>_frame.png` (1x-Frame), `_ref.png` (nachgespielte Referenz), `_mask.png` (magenta = HD sichtbar,
  sonst der 1x-Pixel, der im Overlay deckend liegt), `_hd.png` (1x-Basis + HD-Ebene ohne Overlay) und
  `_screen.png` (so wie angezeigt, ohne Mauszeiger). Im Log steht eine Zeile `HD-Debug F11 #<n>` mit
  Frame-Nummer, Anteil durchsichtig und Herkunft der Liste.
- Alle 5 Sekunden steht im Log eine Zeile `HD-Ausfaelle`: Presents ohne HD-Ebene bzw. mit mehr als 50 % 1x,
  Frames mit mehr als 50 % 1x, Frames, die mit der letzten Liste ergaenzt wurden, und wie viele Eintraege je
  Frame aus dem letzten Frame uebernommen wurden (Stellen, die das Spiel nicht neu gezeichnet hat).

### Breitbild (ab H13)

- `OptionWidescreen` in der `AT.json`: `1` = an (Standard ab H16), `0` = aus (Bild wie bisher). Vorhandene Eintraege
  bleiben unveraendert. Die Leinwand richtet sich
  nach dem Seitenverhaeltnis des Fensters: 16:10 (z. B. 2880x1800) -> 768x480, 16:9 -> 854x480, hoechstens 854.
- Ab H14 nutzt die Flughafenhalle die ganze Leinwand; alle anderen Bildschirme bleiben 640 breit und stehen mittig
  mit schwarzen Raendern. Menues und Dialoge liegen in der Halle im mittleren 640er-Ausschnitt.
- Ab H17 gilt das Breitbild auch in Raeumen mit Statuszeile: der Raum steht mittig (640x440), die Statuszeile reicht
  wie in der Halle ueber die ganze Breite. Bildschirme ohne Statuszeile (Menues, Optionen, Vollbild-Raeume) bleiben
  640 breit und mittig.
  Zeichnet ein Raum selbst in die Statuszeile (z. B. der Exit-Knopf der Statistik ueber dem linken Block), geht das ueber
  `CStdRaum::BlitIntoStatusBand` an die Position der breiten Zeile, mit derselben Umrechnung wie Maus und Tooltips (H17b).
- `OptionWidescreenRoomBorder`: Raender neben dem Raum bzw. neben 640 breiten Bildschirmen: `1` = weichgezeichnet und
  abgedunkelt aus dem Bild selbst, auf der GPU (Standard), `0` = schwarz. Ohne GPU-Zwischenziel bleiben sie schwarz.
- Ab H15 reicht die Statuszeile in der Halle ueber die ganze Breite: linker Block und Inventar am linken, die rechte
  Endkappe am rechten Bildrand, dazwischen weitere Rohrsegmente aus der Original-Grafik. Berater und Handy sitzen am
  rechten Bildrand. Eine eigene Fuellungs-Grafik wird nicht mehr gebraucht.
- Breitbild-Test (mit `OptionHdDebugMask` = 1): Die Halle wird vor dem Zeichnen mit Magenta gefuellt; alle 5 Sekunden
  steht im Log `Breitbild-Test: N Pixel der Halle nicht gezeichnet (Bild-x a..b, Ausschnitt x, x mod 320)` bzw.
  `Halle vollstaendig gezeichnet`.
- Im Log stehen `Breitbild: Fenster WxH, Leinwand Wx480` und bei jedem Wechsel `Bildbreite alt -> neu`;
  die F11-Zeile nennt die Bildgroesse.

### Welche HD-Grafiken fehlen noch?

In `AT.json` `"OptionHdMissingLog": true` setzen. Dann steht fuer jeden Chunk und jede Schrift, die im Spiel
ohne HD-PNG gezeichnet werden, einmal eine Zeile im Log, z. B.

```
HD fehlt (Raum kiosk.gli): hd/room/kiosk.gli/ZEITUNG.png  120x80 -> 480x320
```

Der Raum ist der, in dem die Grafik zuerst auftauchte. Grafiken, die das Spiel nachtraeglich bemalt, stehen mit
dem Hinweis "HD-PNG vorhanden, aber die Grafik wird im Spiel bemalt" in der Liste; sie bleiben 1x.

Ohne `hd/`-Ordner verhaelt sich das Spiel unveraendert.
