#!/usr/bin/env python3
"""Erzeugt kuenstliche Testdaten im Format der Spieldaten (keine Spieldaten!) fuer den End-to-End-Test von HD_generate.bat.

Aufruf:
  python3 tools/hd_testdaten.py ZIELORDNER

Legt in ZIELORDNER an (alles aus Mustern gerechnet, nichts aus dem Spiel):
  gli/glstd.gli      UI-Grafik mit Colorkey
  gli/glbrick.gli    zwei Hallen-Bausteine
  gli/testa1.gli     ein "Stadtfoto" (Regel *1.gli)
  room/kiosk.gli     ein Raum-Hintergrund
  misc/*.mcf         Schriften fuer schrift_standard, schrift_projektion und schrift_bikubisch
  video/dukau.flc    kurze Animationen (Clips des Duty-free-Raums), fuer AT --hd-export;
  video/dublase.flc  dublase und durede fuer den Test, dass ein Fehler je Clip den Export nicht beendet
  video/durede.flc   (Umgebungsvariable AT_HD_EXPORT_TEST, siehe src/HdVideoExport.cpp)
  video/so_gasmask.smk  Smacker-Datei ohne Huffman-Baeume, Hoehe kein Vielfaches von 4: brachte libsmacker zum
                     Absturz (H29) und muss jetzt durchlaufen
Die Dateien sind nur fuer den Test und duerfen nicht ins Git (der Asset-Guard verhindert das ohnehin).
Keine Abhaengigkeiten ausser der Python-Standardbibliothek.
"""
import os
import random
import struct
import sys

RNG = random.Random(1)


def bild(w, h, colorkey=False):
    """Zeilen aus (r, g, b): Verlauf mit etwas Rauschen; mit colorkey schwarze Flaechen (durchsichtig im Spiel)."""
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            r = x * 255 // max(w - 1, 1)
            g = y * 255 // max(h - 1, 1)
            b = (x + y) * 7 % 256
            r, g, b = (min(255, max(8, c + RNG.randrange(40))) for c in (r, g, b))
            if colorkey and ((x < w // 3 and y < h // 3) or (x >= w // 2 and y >= h // 2 and y % 2 == 0)):
                r = g = b = 0
            row.append((r, g, b))
        rows.append(row)
    return rows


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def schreibe_gli(pfad, chunks):
    """GLIB-Archiv wie in tools/gli_export.py beschrieben: Kopf, Verzeichnis, je Grafik 76 Byte Bildkopf + RGB565."""
    os.makedirs(os.path.dirname(pfad), exist_ok=True)
    kopf = b"GLIB" + bytes(30) + struct.pack("<II", len(chunks), 42)
    eintrag = 4 + 1 + 8 + 4
    offset = len(kopf) + eintrag * len(chunks)
    verzeichnis, daten = b"", b""
    for name, rows in chunks:
        h, w = len(rows), len(rows[0])
        pix = b"".join(struct.pack("<H", rgb565(*p)) for row in rows for p in row)
        hdr = [0] * 19
        hdr[1], hdr[2], hdr[3], hdr[6] = len(pix), w, h, 16
        hdr[8], hdr[9], hdr[10] = 0xF800, 0x07E0, 0x001F
        verzeichnis += struct.pack("<IB8sI", eintrag, 1, name.encode().ljust(8, b"\0"), offset + len(daten))
        daten += struct.pack("<19I", *hdr) + pix
    with open(pfad, "wb") as fh:
        fh.write(kopf + verzeichnis + daten)


def schreibe_mcf(pfad, w, h, farben):
    """Schrift wie in tools/mcf_export.py beschrieben: 44 Byte Kopf, Farbindizes, Palette (B, G, R, x), Breiten."""
    os.makedirs(os.path.dirname(pfad), exist_ok=True)
    zeichen = 223
    pix = bytearray(w * h * zeichen)
    for i in range(zeichen):
        if i % 17 == 5:
            continue  # leeres Zeichen
        r = random.Random(i % 40)
        for y in range(2, h - 2):
            for x in range(1 + i % 3, w - 2):
                if r.random() >= 0.35:
                    pix[(i * h + y) * w + x] = r.randrange(1, len(farben))
    pal = b"".join(bytes((b, g, r, 0)) for (r, g, b) in farben)
    hdr = struct.pack("<5H2i5H4i", 0, 0, 0, w, h, 0, 0, len(farben), 0, 32, 32 + zeichen - 1, 0, len(pix), len(pal), 0, 0)
    with open(pfad, "wb") as fh:
        fh.write(hdr + bytes(pix) + pal + bytes(512))


def schreibe_flc(pfad, w, h, bilder):
    """FLC (Magic 0xAF12) mit je Bild einem COLOR_256- und einem COPY-Chunk; Index 0 ist im Spiel durchsichtig."""
    os.makedirs(os.path.dirname(pfad), exist_ok=True)
    pal = bytearray()
    for i in range(256):
        pal += bytes((i, 255 - i, (i * 3) % 256)) if i else bytes(3)
    farbe = struct.pack("<H", 1) + bytes((0, 0)) + bytes(pal)
    frames = b""
    for n in range(bilder):
        pix = bytearray(w * h)
        for y in range(h // 4 + n, h - h // 4 + n):
            for x in range(w // 4, w - w // 3):
                if 0 <= y < h:
                    pix[y * w + x] = 1 + (x * 7 + y * 3 + n * 20) % 250
        chunks = [(4, farbe), (16, bytes(pix))]
        body = b"".join(struct.pack("<IH", 6 + len(d), t) + d for t, d in chunks)
        frames += struct.pack("<IHH8x", 16 + len(body), 0xF1FA, len(chunks)) + body
    kopf = struct.pack("<IHHHHHHI", 128 + len(frames), 0xAF12, bilder, w, h, 8, 0, 70).ljust(128, b"\0")
    with open(pfad, "wb") as fh:
        fh.write(kopf + frames)


def schreibe_smk(pfad, w, h, bilder):
    """Minimale Smacker-Datei (SMK2): alle vier Huffman-Baeume leer, keine Palette, je Bild 4 Byte Videodaten."""
    os.makedirs(os.path.dirname(pfad), exist_ok=True)
    baeume = bytes(1)  # vier Bits 0 = vier leere Baeume
    chunk = bytes(4)
    kopf = b"SMK2" + struct.pack("<IIIiI", w, h, bilder, 100, 0)
    kopf += struct.pack("<7I", *([0] * 7)) + struct.pack("<I", len(baeume)) + struct.pack("<4I", 0, 0, 0, 0)
    kopf += struct.pack("<7I", *([0] * 7)) + struct.pack("<I", 0)
    kopf += struct.pack(f"<{bilder}I", *([len(chunk)] * bilder)) + bytes(bilder)
    with open(pfad, "wb") as fh:
        fh.write(kopf + baeume + chunk * bilder)


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    ziel = sys.argv[1]
    schreibe_gli(os.path.join(ziel, "gli", "glstd.gli"), [("MENU", bild(52, 36, True))])
    schreibe_gli(os.path.join(ziel, "gli", "glbrick.gli"), [("BRICK1", bild(48, 32, True)), ("BRICK2", bild(40, 24, True))])
    schreibe_gli(os.path.join(ziel, "gli", "testa1.gli"), [("TESTA1", bild(90, 60))])
    schreibe_gli(os.path.join(ziel, "room", "kiosk.gli"), [("ZEITUNG", bild(64, 48))])
    schreibe_mcf(os.path.join(ziel, "misc", "norm_bl.mcf"), 8, 10,
                 [(0, 0, 0), (20, 20, 160), (60, 60, 200), (120, 120, 240), (200, 200, 255), (90, 30, 30)])
    schreibe_mcf(os.path.join(ziel, "misc", "inkfont.mcf"), 8, 10, [(0, 0, 0), (0, 16, 82), (111, 123, 160), (222, 231, 239)])
    schreibe_mcf(os.path.join(ziel, "misc", "bank_bl.mcf"), 8, 10, [(0, 0, 0), (29, 29, 141), (140, 137, 137), (255, 245, 134)])
    schreibe_flc(os.path.join(ziel, "video", "dukau.flc"), 40, 32, 3)
    schreibe_flc(os.path.join(ziel, "video", "dublase.flc"), 24, 20, 2)
    schreibe_flc(os.path.join(ziel, "video", "durede.flc"), 24, 20, 2)
    schreibe_smk(os.path.join(ziel, "video", "so_gasmask.smk"), 30, 18, 3)
    print(f"Testdaten (kuenstlich, keine Spieldaten) in {ziel}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
