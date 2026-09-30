# Spieldateien, HD-Grafiken und KI-Modelle

Dieses Repository enthaelt nur Quellcode und Werkzeuge. Folgendes gehoert **nie** ins Git:

- Originaldateien von Airline Tycoon Deluxe (`.gli`, `.glj`, `.smk`, `.lbm`, `.mcf`, `.res`, `.raw`, Spieldaten-CSV usw.).
  Sie sind urheberrechtlich geschuetzt (BFG / Spellbound).
- Daraus erzeugte Grafiken, z. B. per KI hochskalierte HD-Bilder (Ordner `hd/`, `upscaled/`, `export/`).
  Sie bleiben abgeleitete Werke der Originalgrafiken.
- KI-Modelle wie `4x-UltraSharp.pth` (Lizenz CC BY-NC-SA 4.0, nur nicht-kommerziell).

Jede Person erzeugt die HD-Grafiken lokal aus ihrer eigenen, gekauften Spielkopie.

## Schutzmechanismen

1. `.gitignore` schliesst Spieldateien, Asset-Ordner und Modelle aus.
2. Lokaler Hook blockiert Commits: `git config core.hooksPath tools/hooks`
3. GitHub-Workflow `asset guard` prueft jeden Push und Pull Request.

Pruefung von Hand: `python3 tools/asset_guard.py`

## HD-Grafiken lokal erzeugen und verwenden

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

Ohne `hd/`-Ordner verhaelt sich das Spiel unveraendert.
