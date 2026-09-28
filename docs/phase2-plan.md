# Phase 2: Zeichnen mit Faktor s (Umbauplan)

Stand: 28.09.2026 · Basis: `develop` (nach Phase 1, PR #2)

## Ziel

- Die Spiellogik bleibt in **640x480**: Positionen, Klickflächen, Mauskoordinaten, Layout, Spielstände, Netzwerk.
- Gezeichnet wird intern mit **Faktor s**. Eingestellt wird er in `AT.json` als `OptionRenderScale`:
  - Standard ist 1, das Spiel verhält sich dann bitgenau wie bisher.
  - Ziel ist 4, das ergibt 2560x1920.
- Bilder ohne HD-Datei werden beim Laden per Nearest-Neighbor auf s-fache Größe gebracht. HD-Dateien dürfen genau s-mal so groß sein wie das Original.

## Kernidee: logische Größe, physische Pixel

Die ~350 Stellen mit `640`, `480`, `440` usw. arbeiten alle in **logischen** Koordinaten. Dasselbe gilt für `SBBM::Size`, `GetXSize()`, `CRect`-Klickflächen und `gMousePosition`. Deshalb fassen wir diese Stellen nicht an. Die Skalierung steckt allein im Bitmap-Kern in `src/SBLib`:

```
SB_CBitmapCore
  Size          logisch (wie bisher, z. B. 640x480)   <- liest die Spiellogik
  Scale         1 oder s                               <- neu, je Bitmap
  lpDDSurface   physisch = Size * Scale                <- SDL-Surface
```

- Alle Methoden von `SB_CBitmapCore` nehmen weiter logische Koordinaten und rechnen intern mit `Scale`. Das betrifft `Blit`, `BlitFast`, `BlitChar`, `Clear`, `SetPixel`, `GetPixel`, `Line`, `SetClipRect` und `GetClipRect`.
- **Jede Bitmap hat ihren eigenen Faktor.** Beim Blit zwischen Bitmaps mit gleichem Faktor läuft alles wie bisher über `SDL_BlitSurface`. Sind die Faktoren verschieden, nimmt der Blit `SDL_BlitScaled`, das per Nearest-Neighbor skaliert und den Colorkey beachtet. So lassen sich Bitmaps einzeln umstellen, und das Spiel bleibt nach jedem Schritt spielbar.
- Das Fenster und die Maus sind schon unabhängig von der Auflösung:
  - `SB_CPrimaryBitmap::Present()` skaliert die Primärtextur auf `TargetSize`.
  - `GameFrame::TranslatePointToGameSpace()` rechnet Fensterkoordinaten in 640x480 um.
  - Nur die Primärtextur wird s-mal größer, am Fenster ändert sich nichts.

### Übergangslösung für direkten Pixelzugriff

Einige Stellen schreiben über `SB_CBitmapKey` (`Bitmap` + `lPitch`) direkt in den Speicher. Dieser Code rechnet mit 640 Pixeln pro Zeile.

| Datei | Zweck | Umgang |
|---|---|---|
| `ColorFx.cpp` (10 Funktionen) | Überblendungen, Schatten (`BlitAlpha`), Text-Hervorhebung, Glühen, Transparenz | nativ auf physische Pixel umstellen |
| `HLine.cpp` (`BlitAt`, `BlitLargeAt`) | Personen und Clans aus `.pol`-Pools (8-Bit-Zeilen) | nativ, jeder Pixel wird ein s×s-Block |
| `GameFrame.cpp`, `StdRaum.cpp` | `memcpy(..., 640 * 2)` beim Überblenden und bei Screenshots | Zeilenlänge aus der Surface nehmen |
| `SmackPrs.cpp`, `CVideo.cpp`, `DeltaVid.cpp` | Video-Frames | Video-Bitmaps bleiben bei Faktor 1 und werden hochskaliert geblittet |
| `Planer.cpp` (Globus), `Insel.cpp`, `Kiosk.cpp`, `AtNet.cpp`, `Init.cpp` | eigene Rasterer und Datentabellen | zuerst in eine Bitmap mit Faktor 1 zeichnen, später bei Bedarf nativ |

Damit der Umbau schrittweise gehen kann, bekommt `SB_CBitmapKey` einen **Kompatibilitätsmodus**. Er greift, wenn die Bitmap einen Faktor über 1 hat und der aufrufende Code noch nicht umgestellt ist:

1. Der Konstruktor rechnet die Bitmap auf 1x herunter und nimmt dafür jeden s-ten Pixel.
2. Der alte Code arbeitet auf diesem 1x-Puffer.
3. Der Destruktor skaliert das Ergebnis per Nearest-Neighbor zurück.

Solange der Inhalt einer Bitmap nur aus hochskalierten 1x-Bildern besteht, ist das **verlustfrei**. Mit HD-Inhalt ginge dabei Detail verloren. Deshalb schreibt jeder Aufruf im Kompatibilitätsmodus eine Log-Zeile mit Aufrufer und Bitmap-Größe. Diese Log-Zeilen sind die Liste der noch offenen Umbauten.

Umgestellter Code bekommt `SB_CBitmapKey(core, SB_NATIVE)` und liest `Key.Scale`.

## Einzelfragen

**Mauskoordinaten und Klickflächen:** keine Änderung. Die Maus kommt schon als 640x480-Koordinate an, und alle Klick-Rechtecke sind logisch.
- Pixelgenaue Treffertests laufen über `GetPixel(x, y)`, und das rechnet künftig selbst mit x·s und y·s.
- Datenbitmaps, die per Key als Tabelle gelesen werden, bleiben bei Faktor 1. Dazu gehören `ShadeBm` in `Init.cpp` und die Globus-Textur in `Planer.cpp`. Sie bekommen beim Laden das neue Flag `CREATE_NOSCALE`.

**Schrift (`.mcf`):**
- `SB_CFont` zeichnet über `BlitChar`. Das wird ein skalierter Blit, die Schrift ist bei s=4 also zunächst blockig.
- Die Laufweiten (`GetWidth`) bleiben logisch, Umbrüche und Layout ändern sich nicht.
- Später gibt es zwei Möglichkeiten:
  - Glyphenblätter der `.mcf`-Schriften als PNG exportieren, hochskalieren und als `hd/fonts/<name>.png` laden.
  - Mit SDL_ttf, das schon gelinkt ist, eine passende TTF-Schrift in s-facher Größe rendern.
- Die Glyphenbreiten müssen dabei gleich bleiben.

**Transparenz und Colorkey:**
- Der Colorkey ist schwarz (0 in RGB565). Nearest-Neighbor erhält ihn exakt.
- HD-Dateien aus einem KI-Upscaler haben weiche Kanten, und im deckenden Bereich können rein schwarze Pixel entstehen, die zu Löchern würden. Deshalb nimmt die Engine beim Laden einer HD-Datei **die Maske des Originals** und skaliert sie per Nearest-Neighbor hoch:
  - Wo das Original transparent ist, wird der HD-Pixel zum Colorkey.
  - Wo das Original deckend ist und der HD-Pixel dem Colorkey entspricht, wird er auf `0x0001` gesetzt, also fast schwarz.
- So bleiben die Umrisse identisch und es gibt weder Säume noch Löcher.

**Schatten:**
- Personenschatten sind GLI-Bitmaps (`clan.Shadow`), die `ColorFX.BlitAlpha` auf die Primär-Bitmap abdunkelt.
- Nach der Umstellung arbeitet `BlitAlpha` in physischen Pixeln. Hat die Quelle den Faktor 1, holt es sich eine zwischengespeicherte hochskalierte Kopie.
- `OptionSchatten` bleibt unverändert.

**Smacker/FLC-Videos:**
- Sie werden weiter in eine 8-Bit-Bitmap in Originalgröße dekodiert (Faktor 1) und hochskaliert geblittet.
- HD-Videos gehören nicht zu Phase 2.

**Mauszeiger:** `SB_CCursor::Render` rechnet mit der Zeiger-Bitmap. Das Ziel-Rechteck wird mit `TargetSize/640` skaliert, der Zeiger kann dann auch eine HD-Bitmap sein.

**Personen (`.pol`-Pools):**
- Das ist ein eigenes Format neben GLI, gezeichnet über `CHLObj::BlitAt`.
- Nach der Umstellung schreibt `BlitAt` jeden Pixel als s×s-Block.
- HD-Personen wären ein eigenes Projekt: `.pol`-Exporter, Upscale, eigenes Ladeformat. Das kommt nach Phase 2.

## Aufwand an Speicher und Rechenzeit bei s=4

Faktor 4 bedeutet **16-mal so viele Pixel**. Die folgenden Zahlen sind Abschätzungen und werden in Schritt 3 und 4 gemessen.

**Speicher:**

| Posten | 1x | s=4 |
|---|---|---|
| Primärpuffer 640x480 RGB565 | 0,6 MB | 9,8 MB (dazu gleich groß als GPU-Textur) |
| Raum-Offscreen 640x440 | 0,56 MB | 9 MB |
| Geladene GLI-Grafiken (Grundstock + aktueller Raum, geschätzt) | 20–60 MB | 320–960 MB |

- Nötig ist ein 64-Bit-Build. Der Windows-Build per CMake/VS2022 ist vermutlich x64, das prüfe ich in Schritt 0.
- Die tatsächliche Summe ermittelt ab Schritt 4 ein Zähler in `SB_CBitmapMain`, der die Bytes aller Surfaces ins Log schreibt.
- Wird es zu knapp, gibt es s=2 (1280x960) mit 4-fachem statt 16-fachem Aufwand, oder HD nur für Hintergründe.

**Rechenzeit:**
- Heute sind es grob 5–10 volle Bildschirmflächen pro Frame, davon das meiste Blits mit Colorkey.
- Bei s=4 sind das etwa 50 Mio. Pixel pro Frame. Reine Kopien schaffen etwa 10 ms, Blits mit Colorkey eher 50–100 ms. Das ergäbe **~10–20 fps statt 60**.
- Der Upload der Primärtextur (9,8 MB pro Frame) ist dagegen unkritisch.
- Gegenmittel, in dieser Reihenfolge:
  1. RLE-Colorkey-Blits für alle geladenen Bitmaps. `SDL_SetSurfaceRLE` gibt es schon für GLI.
  2. Nur geänderte Bereiche neu zeichnen, wo der Raum-Code das hergibt.
  3. Den Primärpuffer in Streifen aufteilen und mit mehreren Threads blitten.
  4. Langfristig statische Hintergründe als GPU-Texturen über den vorhandenen `lpTexture`/`CREATE_VIDMEM`-Weg.
- Gemessen wird mit den vorhandenen `Bench`-Zählern (`Bench.BlitTime`, `ClearTime`).

**Ladezeit:**
- Nearest-Neighbor beim Laden ist vernachlässigbar.
- Ein PNG in 2560x1920 zu dekodieren dauert etwa 50–100 ms. Ein Raumwechsel mit Hintergrund und Sprites kostet also 0,2–1 s.
- Falls das stört, kommt später ein Cache mit RGB565-Rohdaten dazu.

## Schrittfolge

Nach jedem Schritt ist das Spiel spielbar. Mit s=1 ist es bitgenau unverändert, das prüft ein Screenshot-Vergleich. Jeder Schritt ist ein eigener PR.

| # | Schritt | Sichtbar bei s=4 |
|---|---|---|
| 0 | `OptionRenderScale` in `Sim.Options` und `AT.json` (1–4, Standard 1, wirkt beim Neustart); globaler Wert in SBLib (`SB_SetRenderScale`), bevor die erste Bitmap entsteht; 64-Bit-Build prüfen | nichts |
| 1 | `SB_CBitmapCore` rechnet mit `Scale` (alle Methoden, Clip-Rechteck logisch/physisch); Blit zwischen verschiedenen Faktoren per `SDL_BlitScaled`; Testprogramm wie in Phase 1 für s=1/2/4 (Blit, Clip, Colorkey, `GetPixel`) | nichts, alle Bitmaps haben noch Faktor 1 |
| 2 | Kompatibilitätsmodus für `SB_CBitmapKey` samt Log-Zeilen; `CREATE_NOSCALE` für Datenbitmaps | nichts |
| 3 | Primär-Bitmap, Raum-Offscreen und Überblend-Puffer (`gBlendBm`, `OnscreenBitmap`) mit Faktor s anlegen; die `memcpy(640*2)`-Stellen umstellen; Mauszeiger-Ziel skalieren | gleiches Bild, intern 2560x1920; **erste Messung von fps und Speicher** |
| 4 | GLI-Bitmaps beim Laden auf Faktor s hochskalieren (`SB_CBitmapMain::CreateBitmap`); Zähler für den Speicher | gleiches Bild, Speicher ↑ |
| 5 | HD-Dateien in s-facher Größe laden: `GfxLib` hält Original und HD-Surface getrennt, `CreateBitmap` nimmt die HD-Surface, wenn sie genau s-mal so groß ist (sonst die Nearest-Neighbor-Kopie; 1x-HD-Dateien aus Phase 1 gelten weiter); Maskenabgleich wie oben; außerdem die Aufrufe umstellen, die im Büro laufen: `ColorFX.BlitTrans` (Buero.cpp), `BlitWhiteTrans`, `ApplyOn2` (GameFrame), `HighlightText`, `BlitGlow` (StdRaum) | **Meilenstein 1: Büro-Hintergrund in 2560x1920.** Abnahme: Im Büro erscheint keine Log-Zeile aus dem Kompatibilitätsmodus |
| 6 | Personen (`HLine BlitAt`) und Schatten (`BlitAlpha`) nativ | Personen sauber blockig, HD-Hintergrund bleibt erhalten |
| 7 | Übrige `ColorFx`-Funktionen und restliche Key-Stellen, Raum für Raum anhand der Log-Zeilen | alle Räume ohne Detailverlust |
| 8 | Leistung: RLE überall, Neuzeichnen nur geänderter Bereiche, Threads, bei Bedarf GPU-Hintergründe | Ziel 60 fps bei s=4 |
| 9 | Schrift in HD (Glyphenblätter oder TTF) | scharfe Schrift |

Meilenstein 1 braucht die Schritte 0 bis 5.

Wichtig für den Ablauf:
- Schritt 3 ist der erste mit Risiko. Deshalb messen wir dort sofort, wie viel Rechenzeit und Speicher s=4 kostet.
- Bricht die Leistung ein, ziehen wir Schritt 8 vor, bevor HD-Inhalte kommen.

## Folgen für die Werkzeuge

- `tools/gli_export.py` bleibt, wie er ist, und exportiert 1x.
- Der Upscaler (Phase 3) erzeugt s-fache PNGs mit denselben Pfaden: `hd/<ordner>/<datei>/<chunk>.png`.
- Die Engine erkennt am Seitenverhältnis, ob eine Datei 1x oder s× ist. Andere Größen ignoriert sie mit Log-Zeile.
- Weder Spieldateien noch HD-Dateien kommen ins Git. Asset-Guard und `.gitignore` bleiben unverändert.

## Offene Punkte und Risiken

- **Speicher bei s=4:** Das ist nur geschätzt. Die Messung in Schritt 4 entscheidet, ob s=4 für alles geht oder nur für Hintergründe.
- **Leistung der Colorkey-Blits:** Siehe Schritt 8. Notfalls dient s=2 als Zwischenziel.
- **Text-Hervorhebung:** `ColorFx::HighlightText` und `RemapColor` färben Schriftpixel anhand der exakten Farbe um. Mit HD-Schrift aus weichgezeichneten Glyphen reicht der Vergleich auf exakt eine Farbe nicht mehr. Das lösen wir in Schritt 9.
- **Pixelgenaue Treffertests auf HD-Bildern:** Sie müssen auf der Originalmaske laufen und nicht auf dem HD-Bild. Das ist durch den Maskenabgleich in Schritt 5 gesichert.
