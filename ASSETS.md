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
Gibt es dort eine PNG zum Chunk, wird sie statt des Originalbildes geladen. Einschraenkungen vorerst:

- Das PNG muss **genau die Originalgroesse** haben, sonst wird es ignoriert (Meldung im Log).
- 16-, 24- und 32-Bit-Bilder; die Farben werden auf das Format des Originals reduziert (meist RGB565).
  Ein Alphakanal wird ignoriert; transparente Stellen muessen die Farbe des Originals behalten (Colorkey).

Ohne `hd/`-Ordner verhaelt sich das Spiel unveraendert.
