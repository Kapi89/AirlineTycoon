# Phase 2: Zeichnen mit Faktor s (Umbauplan)

Stand: 28.09.2026, Review eingearbeitet · Basis: `develop` (nach Phase 1, PR #2 und #5)

## Ziel

- Die Spiellogik bleibt in **640x480**: Positionen, Klickflächen, Mauskoordinaten, Layout, Spielstände, Netzwerk.
- Gezeichnet wird intern mit **Faktor s**. Eingestellt wird er in `AT.json` als `OptionRenderScale`:
  - Standard ist 1, das Spiel verhält sich dann bitgenau wie bisher.
  - Ziel ist 4, das ergibt 2560x1920.
- Bilder ohne HD-Datei behalten ihre Originalgröße (Faktor 1) und werden erst beim Zeichnen per Nearest-Neighbor vergrößert.
- HD-Dateien sind genau s-mal so groß wie das Original und liegen auch so im Speicher.

**Voraussetzung:** ein 64-Bit-Build. Schritt 0 schreibt ihn in CMake fest.

## Kernidee: logische Größe, physische Pixel

Die ~350 Stellen mit `640`, `480`, `440` usw. arbeiten alle in **logischen** Koordinaten. Dasselbe gilt für `SBBM::Size`, `GetXSize()`, `CRect`-Klickflächen und `gMousePosition`. Deshalb fassen wir diese Stellen nicht an. Die Skalierung steckt allein im Bitmap-Kern in `src/SBLib`:

```
SB_CBitmapCore
  Size          logisch (wie bisher, z. B. 640x480)   <- liest die Spiellogik
  Scale         1 oder s                               <- neu, je Bitmap
  lpDDSurface   physisch = Size * Scale                <- SDL-Surface
```

- Alle Methoden von `SB_CBitmapCore` nehmen weiter logische Koordinaten und rechnen intern mit `Scale`. Das betrifft `Blit`, `BlitFast`, `BlitChar`, `Clear`, `SetPixel`, `GetPixel`, `Line`, `SetClipRect` und `GetClipRect`.
- **Jede Bitmap hat ihren eigenen Faktor.**
  - **Faktor s** haben nur die Bildschirmpuffer (Primär-Bitmap, Raum-Offscreen, Überblend-Puffer) und Bitmaps aus HD-Dateien.
  - **Faktor 1** behalten alle übrigen Bitmaps: GLI-Grafiken ohne HD-Datei, Schriften, Videos und Datenbitmaps.
- Beim Blit zwischen Bitmaps mit gleichem Faktor läuft alles wie bisher über `SDL_BlitSurface`. Sind die Faktoren verschieden, nimmt der Blit `SDL_BlitScaled` (Nearest-Neighbor, beachtet den Colorkey).
  - So wächst der Speicher nur für HD-Inhalte.
  - Dafür wird fast jeder Blit auf den Bildschirm ein Blit mit Vergrößerung. Wie viel das kostet, misst der Prototyp (Schritt P).
- **Keine neue feste 640 einbauen.** Größe der Puffer, `Present()` und die Umrechnung der Maus lesen eine logische Bildschirmgröße `SB_LogicalSize` (heute 640x480) statt Konstanten. Phase 6 braucht für 16:9 eine logische Breite über 640 (z. B. 854x480). Die vorhandenen 640er-Konstanten in der Spiellogik bleiben vorerst, neue kommen aber nicht dazu.
- Das Fenster ist schon unabhängig von der Auflösung:
  - `SB_CPrimaryBitmap::Present()` skaliert die Primärtextur auf `TargetSize`.
  - `GameFrame::TranslatePointToGameSpace()` rechnet Fensterkoordinaten in logische Koordinaten um.
  - Beide lesen künftig `SB_LogicalSize` statt 640/480.

### Direkter Pixelzugriff (`SB_CBitmapKey`)

Einige Stellen schreiben über `SB_CBitmapKey` (`Bitmap` + `lPitch`) direkt in den Speicher. Dieser Code rechnet mit 640 Pixeln pro Zeile.

| Datei | Zweck | Ziel-Bitmap | Umgang |
|---|---|---|---|
| `ColorFx.cpp` (10 Funktionen) | Überblendungen, Schatten (`BlitAlpha`), Text-Hervorhebung, Glühen, Transparenz | Primär und Raum | **nativ in Schritt 3** |
| `HLine.cpp` (`BlitAt`, `BlitLargeAt`) | Personen und Clans aus `.pol`-Pools | Primär und Raum | **nativ in Schritt 3**, jeder Pixel wird ein s×s-Block |
| `GameFrame.cpp`, `StdRaum.cpp` | `memcpy(..., 640 * 2)` beim Überblenden und bei Screenshots | Primär → Blend-Puffer | **nativ in Schritt 3**, Zeilenlänge aus der Surface |
| `Insel.cpp` (`WaterBlur`) | Wasserspiegelung | Raum | **nativ in Schritt 3** (oder erst in eine Faktor-1-Bitmap zeichnen und dann vergrößert blitten) |
| `SmackPrs.cpp`, `CVideo.cpp`, `DeltaVid.cpp` | Video-Frames | eigene Video-Bitmap | bleibt bei Faktor 1, keine Änderung |
| `Planer.cpp` (Globus), `Kiosk.cpp`, `AtNet.cpp`, `Init.cpp` | eigene Rasterer und Datentabellen | eigene Bitmaps | bleiben bei Faktor 1; zeichnet eine davon in einen Bildschirmpuffer, wird sie nativ umgestellt |

**Keine Übergangslösung für die Bildschirmpuffer.** Auf Primär-Bitmap, Raum-Offscreen und Überblend-Puffer gibt es keinen Kompatibilitätsmodus. Alle Key-Zugriffe darauf werden vor oder mit Schritt 3 nativ umgestellt, damit HD-Inhalt dort nie verloren geht.

Der Kompatibilitätsmodus für `SB_CBitmapKey` bleibt nur als Absicherung für übersehene Stellen:
1. Er wirkt nur auf das **betroffene Teilrechteck**. Der Konstruktor bekommt dafür ein `CRect`.
2. Ein Key auf eine Bitmap mit Faktor über 1 ohne `SB_NATIVE` schreibt eine Log-Zeile mit Aufrufer und Rechteck.
3. Im Debug-Build löst er zusätzlich ein Assert aus.

## Einzelfragen

**Mauskoordinaten und Klickflächen:** keine Änderung. Die Maus kommt schon als logische Koordinate an, alle Klick-Rechtecke sind logisch.
- Pixelgenaue Treffertests laufen über `GetPixel(x, y)`, das künftig selbst mit x·s und y·s rechnet.
- Bei HD-Bitmaps prüft `GetPixel` gegen die Maske des Originals.
- Datenbitmaps (`ShadeBm` in `Init.cpp`, Globus-Textur in `Planer.cpp`) haben ohnehin Faktor 1.

**Schrift (`.mcf`):**
- Schriften bleiben Faktor-1-Bitmaps. `BlitChar` wird ein Blit mit Vergrößerung, die Schrift ist bei s=4 also blockig.
- Die Laufweiten (`GetWidth`) bleiben logisch, Umbrüche und Layout ändern sich nicht.
- HD-Schrift (Glyphenblätter oder TTF über SDL_ttf) kommt im letzten Schritt.

**Transparenz und Colorkey:**
- Der Colorkey ist schwarz (0 in RGB565). Nearest-Neighbor erhält ihn exakt.
- HD-Dateien aus einem KI-Upscaler können weiche Kanten und rein schwarze Pixel im deckenden Bereich haben. Deshalb nimmt die Engine beim Laden einer HD-Datei **die Maske des Originals**, per Nearest-Neighbor hochskaliert:
  - Wo das Original transparent ist, wird der HD-Pixel zum Colorkey.
  - Wo das Original deckend ist und der HD-Pixel dem Colorkey entspricht, wird er auf `0x0001` gesetzt.
- **Für Phase 4 vorgemerkt:** Bei Sprites gibt Nearest-Neighbor treppige Ränder. Besser ist, die Maske des Originals **weich** hochzuskalieren (bilinear oder bikubisch) und danach eine Schwelle anzuwenden, etwa bei 50 %. Die Kanten folgen dann der Form statt dem Pixelraster. Vorgesehen ist dafür die Funktion `HdMask()`, die in Phase 2 nur Nearest-Neighbor kann.

**Schatten:**
- Personenschatten sind GLI-Bitmaps mit Faktor 1 (`clan.Shadow`), die `ColorFX.BlitAlpha` auf die Primär-Bitmap abdunkelt.
- Nach der Umstellung in Schritt 3 liest `BlitAlpha` die Quelle mit Faktor 1 und schreibt jeden Pixel als s×s-Block.

**Smacker/FLC-Videos:** Sie werden weiter in eine 8-Bit-Bitmap in Originalgröße dekodiert (Faktor 1) und vergrößert geblittet.

**Mauszeiger:** `SB_CCursor::Render` bekommt sein Ziel-Rechteck mit `TargetSize / SB_LogicalSize` skaliert.

**Personen (`.pol`-Pools):** `CHLObj::BlitAt` schreibt nach Schritt 3 jeden Pixel als s×s-Block. HD-Personen kommen nach Phase 2.

## Aufwand an Speicher und Rechenzeit bei s=4

Faktor 4 bedeutet **16-mal so viele Pixel**. Die Zahlen sind Abschätzungen, gemessen wird im Prototyp und in Schritt 3.

**Speicher:** Weil Bitmaps ohne HD-Datei bei Faktor 1 bleiben, wachsen nur die Bildschirmpuffer und die HD-Inhalte.

| Posten | 1x | s=4 |
|---|---|---|
| Primärpuffer 640x480 RGB565 | 0,6 MB | 9,8 MB (dazu gleich groß als GPU-Textur) |
| Raum-Offscreen 640x440 | 0,56 MB | 9 MB |
| Überblend-Puffer (`gBlendBm`, `OnscreenBitmap`) | je 0,6 MB | je 9,8 MB |
| GLI-Grafiken ohne HD-Datei | 20–60 MB | unverändert 20–60 MB |
| HD-Hintergrund, je Raum | – | ~9 MB |

- Mit HD-Hintergründen und einigen HD-Sprites im aktuellen Raum liegt der Bedarf grob bei **100–200 MB**.
- Wenn später alle Sprites HD werden, steigt er Richtung 320–960 MB. Das ist ein Grund für den 64-Bit-Build.

**Rechenzeit:**
- Heute sind es grob 5–10 volle Bildschirmflächen pro Frame, davon das meiste Blits mit Colorkey.
- Bei s=4 sind das etwa 50 Mio. Zielpixel pro Frame, fast alle über `SDL_BlitScaled`. Das ist langsamer als ein normaler Blit, und RLE hilft dort nicht.
- Grob geschätzt ergibt das **~5–20 fps**. Der Prototyp misst das.
- Der Upload der Primärtextur (9,8 MB pro Frame) ist dagegen unkritisch.

**Ladezeit:** Ein PNG in 2560x1920 zu dekodieren dauert etwa 50–100 ms. Später kommt eventuell ein Cache mit RGB565-Rohdaten dazu.

## Schrittfolge

Nach jedem Schritt ist das Spiel spielbar. Mit s=1 ist es bitgenau unverändert, das prüft ein Screenshot-Vergleich. Jeder Schritt ist ein eigener PR, außer dem Prototyp.

| # | Schritt | Sichtbar bei s=4 |
|---|---|---|
| 0 | `OptionRenderScale` in `Sim.Options` und `AT.json` (1–4, Standard 1, wirkt beim Neustart); globaler Wert in SBLib (`SB_SetRenderScale` / `SB_GetRenderScale`), gesetzt vor der ersten Bitmap; `SB_LogicalSize` (640x480) als einzige Quelle für die logische Bildschirmgröße; **64-Bit-Build in CMake festschreiben** (Abbruch bei 32 Bit) | nichts |
| P | **Wegwerf-Prototyp** (eigener Zweig, wird nicht gemergt): Primär-Bitmap mit Faktor 4, alles per `SDL_BlitScaled` hineinzeichnen, direkter Pixelzugriff notdürftig oder abgeschaltet; fps im **Büro** und in der **Flughafenhalle** messen (Bench-Zähler und Frame-Zeit) | Messwerte |
| – | **Entscheidung** anhand der Messung: **CPU-Weg** (weiter wie unten, Leistung per RLE/Threads) oder **GPU-Hintergründe** (statische Hintergründe als Texturen, Bildschirmpuffer bleiben 1x oder werden zu Render-Targets). Die Schritte ab 1 werden danach angepasst | – |
| 1 | `SB_CBitmapCore` rechnet mit `Scale` (alle Methoden, Clip-Rechteck logisch/physisch); Blit zwischen verschiedenen Faktoren per `SDL_BlitScaled`; Testprogramm wie in Phase 1 für s=1/2/4 (Blit, Clip, Colorkey, `GetPixel`) | nichts, alle Bitmaps haben noch Faktor 1 |
| 2 | `SB_CBitmapKey` mit `SB_NATIVE`, `Scale` und dem Kompatibilitätsmodus für Teilrechtecke (Log-Zeile, Assert im Debug-Build) | nichts |
| 3 | Primär-Bitmap, Raum-Offscreen und Überblend-Puffer mit Faktor s. **Gleichzeitig nativ:** alle `ColorFx`-Funktionen, `HLine::BlitAt`/`BlitLargeAt`, die `memcpy`-Überblendungen in `GameFrame`/`StdRaum`, `WaterBlur`; Mauszeiger-Ziel skalieren | gleiches Bild, intern 2560x1920; keine Log-Zeile des Kompatibilitätsmodus auf den Bildschirmpuffern |
| 4 | HD-Dateien in s-facher Größe laden: `GfxLib` hält Original und HD-Surface getrennt, `CreateBitmap` gibt bei passender HD-Datei eine Bitmap mit Faktor s zurück, sonst wie bisher Faktor 1; 1x-HD-Dateien aus Phase 1 gelten weiter; Maskenabgleich (`HdMask()`, zunächst Nearest-Neighbor) | **Meilenstein 1: Büro-Hintergrund in 2560x1920** |
| 5 | Restliche Key-Stellen anhand der Log-Zeilen; Zähler für Surface-Speicher im Log | alle Räume ohne Detailverlust |
| 6 | Leistung, je nach Entscheidung aus P: RLE, Neuzeichnen nur geänderter Bereiche, Threads oder GPU-Hintergründe | Ziel 60 fps bei s=4 |
| 7 | Schrift in HD (Glyphenblätter oder TTF) | scharfe Schrift |

Meilenstein 1 braucht die Schritte 0, P, 1 bis 4.

## Folgen für die Werkzeuge

- `tools/gli_export.py` bleibt, wie er ist, und exportiert 1x.
- Der Upscaler (Phase 3) erzeugt s-fache PNGs mit denselben Pfaden: `hd/<ordner>/<datei>/<chunk>.png`.
- Die Engine akzeptiert genau 1x oder genau s×. Andere Größen ignoriert sie mit Log-Zeile.
- Weder Spieldateien noch HD-Dateien kommen ins Git. Asset-Guard und `.gitignore` bleiben unverändert.

## Vorgemerkt für spätere Phasen

- **Phase 4:** die Maske für HD-Sprites weich hochskalieren und mit Schwelle versehen statt Nearest-Neighbor (siehe Transparenz).
- **Phase 6 (16:9):**
  - Die logische Breite wird größer als 640, `SB_LogicalSize` wird dafür die Stellschraube.
  - Bis dahin kommen keine neuen festen 640/480 in `Present`, Maus oder Puffergrößen.
  - Die vorhandenen 640er in der Spiellogik müssen dann nach und nach an `SB_LogicalSize` hängen.

## Offene Punkte und Risiken

- **Leistung der vergrößernden Blits:** Die Entscheidung fällt nach dem Prototyp. Notfalls dient s=2 als Zwischenziel.
- **Text-Hervorhebung:** `ColorFx::HighlightText` und `RemapColor` färben Schriftpixel anhand der exakten Farbe um. Das bleibt korrekt, solange die Schrift in Faktor 1 bleibt; mit HD-Schrift (Schritt 7) muss es neu gelöst werden.
- **Pixelgenaue Treffertests auf HD-Bildern:** Sie laufen gegen die Originalmaske (siehe Mauskoordinaten und Klickflächen).
