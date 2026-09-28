# Phase 2: HD-Darstellung (Umbauplan)

Stand: 28.09.2026 · Plan nach der Prototyp-Messung umgestellt auf **GPU-Hintergründe** · Basis: `develop` (Schritt 0 gemergt)

## Ergebnis des Prototyps (Zweig `phase2-prototype`, wird nicht gemergt)

Messung: Release-Build unter Windows auf einem Laptop, Median je 2-s-Zeile. Das Spiel läuft fest im **20-fps-Takt**, das Budget je Frame ist also **50 ms** und nicht 16 ms.

| | Büro | Flughafenhalle |
|---|---|---|
| s=1, Arbeit | 7 ms | 8 ms |
| s=4, Arbeit | 30 ms | 136 ms (max 191), 7 fps |
| s=4, davon 1x-Kopien (Kompatibilitätsmodus) | 13 ms | 99 ms |
| s=4, **netto** | **16 ms** (max 21) | **41 ms** (max 51) |

- **Bewertung:** Das Büro passt ins Budget. Die Halle liegt netto schon am Limit, bevor irgendetwas nativ umgebaut ist. Den ganzen Bildschirm per CPU in s× zu zeichnen reicht deshalb nicht.
- **Bug im Prototyp:** Bei s=4 fehlte im Büro die Person am Schreibtisch. Ursache war nicht die 1x-Kopie: `SDL_BlitScaled` lehnt 8-Bit-Quellen mit Palette und Colorkey ab ("Blit combination not supported"), und die Person ist ein Smacker-Frame in 8 Bit. Im Prototyp ist das behoben: Der Ausschnitt wird selbst nach RGB565 gewandelt, der Schlüssel-Index wird Colorkey, deckendes Schwarz wird `0x0001`.
- **Folge für die Büro-Messung:** Dort fehlten diese Video-Blits. Sie sind klein (ein Video-Ausschnitt), am Ergebnis ändert das nichts.

## Neues Ziel und Kernidee

- Die Spiellogik bleibt in **640x480**, wie bisher.
- Das Zeichnen per CPU bleibt **komplett in 1x**: Primärpuffer 640x480 RGB565, alle Blits, ColorFx, HLine und Videos genau wie heute. Das kostet laut Messung 7–8 ms.
- **HD entsteht erst auf der GPU beim Anzeigen (`Present`)**, mit mehreren Ebenen:

```
Fenster (z. B. 2560x1920)
  1. HD-Hintergrund als Textur (s-fach), per GPU auf das Zielrechteck skaliert
  2. darüber das 1x-Bild als Overlay-Textur (Nearest-Neighbor),
     ABER durchsichtig überall dort, wo es noch genau dem 1x-Hintergrund entspricht
```

**Die Differenzmaske.** Zum HD-Hintergrund gibt es sein 1x-Original, also genau die Pixel, die die Engine selbst als Hintergrund gezeichnet hat.
- **Unverändert:** Stimmt ein Pixel des fertigen 1x-Frames mit dem 1x-Original an dieser Stelle überein, liegt dort nichts darüber. Das Overlay ist dort durchsichtig, man sieht den HD-Hintergrund.
- **Verändert:** Überall sonst zeigt das Overlay das 1x-Bild. Das betrifft Personen, Sprites, Schrift und Effekte.
- **Aufwand:** Die Maske entsteht auf der CPU in einem Durchlauf über 640x480, also 0,3 Mio. Pixel und etwa 1 ms. Hochgeladen wird eine Textur mit 640x480 RGBA (1,2 MB).

**Vorteile:**
- Kein Umbau von ColorFx, HLine, `memcpy`-Überblendungen, Videos oder Schrift. Die Spiellogik und alle 640er-Stellen bleiben unberührt.
- Die Rechenzeit der CPU steigt nur um Maske und Upload (geschätzt 1–3 ms). Das Vergrößern übernimmt die GPU.
- Korrekt ohne Sonderfälle: Alles, was die Engine über den Hintergrund zeichnet, erscheint so, wie es gezeichnet wurde.

**Grenzen (bewusst in Kauf genommen, später verbessert):**
- **Effekte, die den Hintergrund verändern, erscheinen in diesem Bereich in 1x**, weil sich dort das 1x-Bild vom Original unterscheidet. Beispiele: Schatten, Glas der Bürotür, dunkles Büro, Überblendungen beim Raumwechsel. Das ist sichtbar treppig, aber richtig. Verbesserung in Schritt H6.
- **Personen und Sprites bleiben 1x.** HD-Sprites kommen erst über eine Zeichenliste (H5 / Phase 4).

Die Kombination aus Faktor je Bitmap, Bildschirmpuffern in s× und nativem Umbau aller Pixelzugriffe entfällt für Phase 2. Der Prototyp hat gezeigt, dass sie für die Halle zu teuer ist.

## Einzelfragen

**Mauskoordinaten und Klickflächen:** keine Änderung. Alles bleibt logisch in 640x480, der Primärpuffer auch.

**Schrift, Videos, HLine-Personen, ColorFx:** keine Änderung, sie laufen in 1x wie heute und erscheinen über das Overlay.

**Transparenz und Colorkey:**
- Der HD-Hintergrund braucht keinen Colorkey, er ist vollflächig.
- Durchsichtig ist nur das Overlay, und das bestimmt die Differenzmaske, nicht der Colorkey.
- Die Maske vergleicht exakte RGB565-Werte. Das ist sicher, weil der 1x-Hintergrund aus derselben Quelle stammt wie das, was die Engine zeichnet.

**Skalierung auf der GPU:**
- HD-Textur: linear, weil das Fenster meist nicht genau 2560x1920 ist.
- Overlay-Textur: `SDL_SetTextureScaleMode(…, SDL_ScaleModeNearest)`, damit 1x-Inhalte scharf und blockig bleiben. Das braucht SDL ≥ 2.0.12, das eingebundene SDL2 prüfe ich in H1.
- Der vorhandene Hinweis `SDL_HINT_RENDER_SCALE_QUALITY=2` gilt dann nur noch als Standard.

**Wann ist ein Raum "HD-fähig"?** Wenn es zum Hintergrund-Chunk (`PicBitmap`, geladen in `CStdRaum` aus `pRoomLib`) eine HD-Datei in genau s-facher Größe gibt. Ohne HD-Datei läuft `Present` wie heute, bitgenau.

**Speicher:**
- Je HD-Hintergrund eine Textur mit 2560x1760 bzw. 2560x1920 (~9–20 MB im Grafikspeicher, je nach Format).
- Im Hauptspeicher nur während des Ladens. Das 1x-Original gibt es ohnehin.
- Bildschirmpuffer in s× entfallen.

**Rechenzeit (Erwartung):** CPU wie heute plus 1–3 ms. Die GPU zeichnet zwei Rechtecke je Frame, das ist für jede Grafikkarte trivial. Gemessen wird mit denselben `Proto`-Zeilen, dann als Dauer-Log im Debug-Build.

**Flughafenhalle:**
- Die Halle hat keinen einzelnen Hintergrund. Sie besteht aus Bausteinen (`Bricks`, Parallax-Ebenen und Boden), die direkt in die Primär-Bitmap gezeichnet werden, verzahnt mit Personen.
- Für sie braucht es eine **Zeichenliste**: Jeder Blit eines Bausteins mit HD-Datei wird zusätzlich mitgeschrieben (Chunk, logische Position, Clip, Reihenfolge).
- Aus der Liste entsteht auf der CPU eine 1x-Referenz "nur Bausteine" für die Differenzmaske, auf der GPU werden die HD-Texturen an denselben Stellen gezeichnet.
- Das ist deutlich mehr Arbeit als ein einzelner Hintergrund und kommt deshalb nach Meilenstein 1 (Schritt H4).

## Schrittfolge

Nach jedem Schritt ist das Spiel spielbar. Ohne HD-Dateien oder mit s=1 ist das Bild bitgenau wie heute. Jeder Schritt ist ein eigener PR.

| # | Schritt | Sichtbar |
|---|---|---|
| 0 | ✅ `OptionRenderScale`, `SB_GetLogicalSize()`, 64-Bit-Pflicht (PR #6) | – |
| P | ✅ Prototyp CPU s× gemessen → GPU-Weg | – |
| H1 | **HD-Hintergrund-Ebene in `SB_CPrimaryBitmap`:** `SetHdBackground(texture, logicalRect, SDL_Surface *ref1x)` / `ClearHdBackground()`; `Present()` zeichnet bei gesetzter Ebene zuerst die HD-Textur, dann das Overlay (RGBA aus Differenzmaske, Nearest-Neighbor); ohne Ebene wie heute. SDL-Version für `SDL_SetTextureScaleMode` prüfen. Test: Testprogramm mit synthetischem Hintergrund, Maske bitgenau | nichts (noch niemand setzt die Ebene) |
| H2 | **Räume mit Einzelhintergrund:** `CStdRaum` lädt zum Hintergrund-Chunk die HD-Datei (s-fach, aus `hd/<ordner>/<datei>/<chunk>.png`) als Textur und setzt die Ebene mit Position im Primärpuffer (Hintergrund → `RoomBm` → Primärpuffer); beim Verlassen des Raums wieder entfernen. `GfxLib` liefert dafür die HD-Surface getrennt vom 1x-Original (die 1x-Ersetzung aus Phase 1 bleibt für 1x-HD-Dateien) | **Meilenstein 1: Büro-Hintergrund in HD (2560x1920 bei s=4)** |
| H3 | Messung und Feinschliff: fps im Büro mit HD; Ränder am Übergang HD/Overlay prüfen; Fall "Hintergrund verschoben" (Handy-Dialog, Scrollen) testen | Messwerte |
| H4 | **Halle:** Zeichenliste für Bausteine mit HD-Datei; 1x-Referenz nur aus Bausteinen; GPU zeichnet HD-Bausteine in Listenreihenfolge unter das Overlay | Halle mit HD-Bausteinen |
| H5 | HD-Sprites über dieselbe Zeichenliste (Berater, Gegenstände); Maske je Sprite aus dem Original, dabei weich hochskaliert mit Schwelle (siehe unten) | HD-Sprites |
| H6 | Effekte über HD: Abdunkeln durch Schatten und dunkles Büro als halbtransparentes Schwarz im Overlay statt 1x-Pixel (Verhältnis Frame/Original je Pixel) | Schatten ohne Treppen |
| H7 | Schrift in HD (Glyphenblätter oder TTF) als GPU-Ebene | scharfe Schrift |

Meilenstein 1 braucht die Schritte H1 und H2.

## Folgen für die Werkzeuge

- `tools/gli_export.py` bleibt, wie er ist, und exportiert 1x.
- Der Upscaler (Phase 3) erzeugt s-fache PNGs mit denselben Pfaden: `hd/<ordner>/<datei>/<chunk>.png`.
- Die Engine akzeptiert **1x** (Ersetzung wie in Phase 1) oder **genau s×** (GPU-Ebene). Andere Größen ignoriert sie mit Log-Zeile.
- Weder Spieldateien noch HD-Dateien kommen ins Git. Asset-Guard und `.gitignore` bleiben unverändert.

## Vorgemerkt für spätere Phasen

- **Phase 4 (HD-Sprites):** Die Maske des Originals **weich hochskalieren und mit einer Schwelle versehen** statt Nearest-Neighbor, sonst werden die Ränder treppig.
- **Phase 6 (16:9):**
  - `SB_LogicalSize` bleibt die Stellschraube für die logische Breite.
  - Die HD-Ebene rechnet nur mit logischen Rechtecken und `TargetSize`, also ohne feste 640.
  - Primärpuffer, Maske und Overlay nehmen ihre Größe aus `SB_GetLogicalSize()`.
- Der **CPU-Weg mit s×-Puffern** aus dem Prototyp bleibt als Notlösung dokumentiert (Zweig `phase2-prototype`), wird aber nicht weiterverfolgt.

## Offene Punkte und Risiken

- **Überblendungen und dunkles Büro:** Dabei ändert sich fast der ganze Frame, dort ist dann vorübergehend alles 1x. Beim Raumwechsel ist das kurz; das dunkle Büro verbessert H6.
- **Exakte Übereinstimmung:** Wenn die Engine den Hintergrund nicht 1:1 blittet (z. B. verschoben oder beschnitten), muss die Referenz dieselbe Geometrie haben. In H2 prüft deshalb ein Log-Zähler den Anteil durchsichtiger Pixel je Frame; im leeren Büro soll er nahe 100 % liegen.
- **Übergang HD/1x:** Um Sprites herum grenzt HD-Hintergrund direkt an 1x-Pixel. Wie störend das ist, prüfen wir in H3. Abhilfe bringen die HD-Sprites in H5.
- **Ältere Grafiktreiber:** Ohne Hardware-Renderer (Software-Fallback in `Present`) bleibt es bei 1x, die HD-Ebene wird dann übersprungen.
