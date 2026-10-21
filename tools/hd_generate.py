#!/usr/bin/env python3
"""Erzeugt das HD-Paket (4x) aus der eigenen Spielkopie von Airline Tycoon Deluxe.

Aufruf:
  python3 tools/hd_generate.py SPIELORDNER [Optionen]
  python3 tools/hd_generate.py "C:/GOG Games/Airline Tycoon Deluxe" --nur schriften
  python3 tools/hd_generate.py "/Applications/Airline Tycoon Deluxe.app" --gpu
  python3 tools/hd_generate.py SPIELORDNER --nur animationen --raum duty   # Animationen nur auf Wunsch, raumweise
  python3 tools/hd_generate.py SPIELORDNER --alles --gpu --log hd_generate.log  # alles inkl. Animationen (HD_generate.bat)

Der Datenordner (mit gli/) wird im angegebenen Ordner gesucht, auch tief in einem Mac-.app-Paket. Dort sucht das
Spiel auch hd/, dorthin gehoert also auch die neue AT.exe bzw. AT. --gpu nutzt CUDA (NVIDIA) oder MPS (Apple-Chip).

Ablauf:
  1. Export: Grafiken (.gli/.glj), Schriften (.mcf) und Figuren der Halle (.pol) der Spielkopie als PNG
     (tools/gli_export.py, tools/mcf_export.py, tools/pol_export.py) nach ARBEIT/original.
  2. Verfahren je Bild aus tools/hdgen/manifest.json: zuerst Eintrag, dann Regel, sonst "auto".
  3. Hochrechnen (4x) mit den Verfahren aus tools/hdgen/hd_methods.py.
  4. Ablegen als hd/<ordner>/<datei>/<CHUNK>.png bzw. hd/<ordner>/<schrift>.mcf.png neben der AT.exe
     (Figuren: hd/gli/<pool>.pol/<ID>.png; Animationen aus dem Engine-Export hd_assets/original/video:
     hd/video/<clip>.smk/<NNNN>.png, zugeschnitten, dazu crop.csv).

Gleiche Eingaben (MD5 der PNG plus Verfahren und Modell) werden nur einmal gerechnet; vorhandene
Ausgaben werden uebersprungen. Ein abgebrochener Lauf (Strg+C) wird beim naechsten Start fortgesetzt.

Modelle laedt jede Person selbst herunter (siehe ASSETS.md); der Generator laedt nichts aus dem Netz.
Abhaengigkeiten (tools/hdgen/requirements.txt): Export und --liste brauchen nur die Standardbibliothek,
das Hochrechnen numpy und pillow, alle Verfahren mit Modell zusaetzlich torch und spandrel.

Das Ergebnis (hd/, hd_assets/) und die Modelle (models/) duerfen nie ins Git.
"""
import argparse
import fnmatch
import hashlib
import json
import os
import random
import shutil
import struct
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
sys.path.insert(0, os.path.join(TOOLS, "hdgen"))

import gli_export  # noqa: E402
import mcf_export  # noqa: E402
import pol_export  # noqa: E402

MANIFEST = os.path.join(TOOLS, "hdgen", "manifest.json")
# Stand je Verfahren: wird erhoeht, wenn sich das Ergebnis eines Verfahrens aendert. Ausgaben, die mit einem aelteren
# Stand gerechnet wurden, erkennt der Cache und rechnet sie neu (H18b: Stadtfotos am Stueck statt in Kacheln).
VERFAHREN_STAND = {"foto_rgb": 2}
KATEGORIEN = ("schriften", "stadtfotos", "ui", "raeume", "bausteine", "figuren", "animationen")  # auch Reihenfolge der Bearbeitung
NUR_AUF_WUNSCH = {"animationen"}  # lange Rechenzeit: nur mit --nur animationen
VIDEO_RAND = 2  # Zuschnitt der Animationen: Bounding-Box des Clips plus so viele Pixel (1x)

MODELLE = {
    "ultrasharp": {
        "dateien": ("4x-UltraSharp.pth", "4x-UltraSharp.safetensors"),
        "name": "4x-UltraSharp (Kim2091)",
        "quelle": "https://openmodeldb.info/models/4x-UltraSharp",
        "lizenz": "CC BY-NC-SA 4.0 - nur nicht-kommerziell, Weitergabe nur unter gleicher Lizenz",
        "wofuer": "alles ausser Stadtfotos (Raeume, UI, Bausteine, Schriften)",
    },
    "x4plus": {
        "dateien": ("RealESRGAN_x4plus.pth",),
        "name": "RealESRGAN_x4plus (xinntao/Real-ESRGAN, Release v0.1.0)",
        "quelle": "https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.1.0",
        "lizenz": "BSD-3-Clause",
        "wofuer": "Stadtfotos (Globus/Filofax, Laptop) und Figuren der Halle",
    },
}


EN = [False]  # --english (H29b): Meldungen auf Englisch, z. B. fuer HD_generate.bat


def T(de, en):
    """Meldung in der gewaehlten Sprache."""
    return en if EN[0] else de


# ---------------------------------------------------------------- Manifest und Regeln

class Plan:
    """Bestimmt Verfahren, Modell und Kategorie je exportierter PNG."""

    def __init__(self, path=MANIFEST, bausteine_modellkante=True):
        self.bausteine_modellkante = bausteine_modellkante
        with open(path, encoding="utf-8") as fh:
            m = json.load(fh)
        if m.get("scale", 4) != 4:
            raise SystemExit("manifest.json: nur scale 4 wird unterstuetzt")
        self.eintraege = {e["pfad"]: e["verfahren"] for e in m["eintraege"]}
        self.regeln = m["regeln"]
        self.schriften = {}
        for gruppe in m["schriften"].values():
            dateien = gruppe["dateien"]
            par = gruppe.get("parameter", {})
            if isinstance(dateien, dict):
                for name, (ink, bg) in dateien.items():
                    self.schriften[name] = (gruppe["verfahren"], tuple(ink), tuple(bg), par)
            else:
                for name in dateien:
                    self.schriften[name] = (gruppe["verfahren"], None, None, par)

    @staticmethod
    def kategorie(schluessel, verfahren):
        if schluessel.startswith("video/"):
            return "animationen"
        if ".pol/" in schluessel:
            return "figuren"
        if verfahren == "foto_rgb":
            return "stadtfotos"
        if schluessel.startswith("gli/glbrick"):
            return "bausteine"
        if schluessel.startswith("room/"):
            return "raeume"
        return "ui"

    def bild(self, schluessel):
        """schluessel: '<ordner>/<datei>/<CHUNK>' -> (verfahren, modell, kategorie)."""
        verfahren = self.eintraege.get(schluessel)
        modell = None
        if verfahren is None:
            for r in self.regeln:
                if fnmatch.fnmatchcase(schluessel, r["muster"]):
                    verfahren, modell = r["verfahren"], r.get("modell")
                    break
            else:
                verfahren = "auto"
        if modell is None:
            modell = "x4plus" if verfahren in ("foto_rgb", "figur_rgba") else "ultrasharp"
        kategorie = self.kategorie(schluessel, verfahren)
        if self.bausteine_modellkante and kategorie == "bausteine" and verfahren == "colorkey_rgba":
            verfahren = "colorkey_rgba_modellkante"  # H24: Kante per Modell statt Lanczos (seit H29 Standard)
        return verfahren, modell, kategorie

    def schrift(self, name):
        """name: z. B. 'norm_bl' -> (verfahren, modell, ink, bg, parameter, bekannt)."""
        if name in self.schriften:
            verfahren, ink, bg, par = self.schriften[name]
            bekannt = True
        else:
            verfahren, ink, bg, par, bekannt = "schrift_standard", None, None, {}, False
        modell = None if verfahren == "schrift_bikubisch" else "ultrasharp"
        return verfahren, modell, ink, bg, par, bekannt

    def archiv_kategorien(self, rel):
        """Kategorien, die in einem Archiv '<ordner>/<datei>' vorkommen koennen (fuer --nur)."""
        kat = {self.bild(rel + "/X")[2]}
        prefix = rel + "/"
        kat.update(self.kategorie(k, v) for k, v in self.eintraege.items() if k.startswith(prefix))
        return kat


# ---------------------------------------------------------------- Pfade

def _ordner_breitensuche(start, tiefe=8):
    """Alle Unterordner bis zur angegebenen Tiefe, flache zuerst; hd/ und hd_assets/ werden uebersprungen."""
    ebene = [start]
    for _ in range(tiefe + 1):
        naechste = []
        for ordner in ebene:
            yield ordner
            try:
                eintraege = sorted(os.listdir(ordner))
            except OSError:
                continue
            naechste += [os.path.join(ordner, d) for d in eintraege
                         if d.lower() not in ("hd", "hd_assets", "models") and os.path.isdir(os.path.join(ordner, d))]
        ebene = naechste


def finde_datenordner(spiel):
    """Ordner mit den Spieldaten (enthaelt gli/ mit .gli-Dateien). Dort sucht das Spiel auch hd/: AT bzw. AT.exe muss
    in diesem Ordner liegen. Beim Mac-Bundle der GOG-Version liegt er tief im .app-Paket."""
    for ordner in _ordner_breitensuche(spiel):
        try:
            gli = [d for d in os.listdir(ordner) if d.lower() == "gli" and os.path.isdir(os.path.join(ordner, d))]
        except OSError:
            continue
        for d in gli:
            try:
                if any(f.lower().endswith(".gli") for f in os.listdir(os.path.join(ordner, d))):
                    return ordner
            except OSError:
                pass
    return None


def hat_at(ordner):
    try:
        return any((f.lower() == "at.exe" or f == "AT") and os.path.isfile(os.path.join(ordner, f)) for f in os.listdir(ordner))
    except OSError:
        return False


def waehle_geraet(torch, gpu):
    """--gpu: CUDA (Windows/Linux, NVIDIA), sonst MPS (Mac mit Apple-Chip), sonst CPU. Liefert (geraet, meldung)."""
    if not gpu:
        return "cpu", None
    if torch.cuda.is_available():
        return "cuda", T("Rechne auf der Grafikkarte (CUDA): ", "Computing on the graphics card (CUDA): ") + torch.cuda.get_device_name(0)
    mps = getattr(torch.backends, "mps", None)
    if mps is not None and mps.is_available():
        return "mps", T("Rechne auf dem Apple-Grafikchip (MPS)", "Computing on the Apple graphics chip (MPS)")
    return "cpu", T("Hinweis: --gpu angegeben, aber weder CUDA noch MPS ist verfuegbar. Rechne auf der CPU.",
                    "Note: --gpu given, but neither CUDA nor MPS is available. Computing on the CPU.")


def finde_modell(kuerzel, args, spiel, at_ordner):
    info = MODELLE[kuerzel]
    direkt = getattr(args, kuerzel)
    if direkt:
        return direkt if os.path.isfile(direkt) else None
    orte = [args.modelle] if args.modelle else []
    orte += [os.path.join(os.getcwd(), "models"), os.path.join(REPO, "models"),
             os.path.join(at_ordner, "models"), os.path.join(spiel, "models"),
             os.path.join(os.path.abspath(os.path.expanduser(args.spiel)), "models")]
    for ort in orte:
        if not ort or not os.path.isdir(ort):
            continue
        namen = {f.lower(): f for f in os.listdir(ort)}
        for d in info["dateien"]:
            if d.lower() in namen:
                return os.path.join(ort, namen[d.lower()])
    return None


def modell_hinweis(kuerzel):
    info = MODELLE[kuerzel]
    return (f"  {info['name']}\n"
            f"    Datei:  {' oder '.join(info['dateien'])}\n"
            f"    Quelle: {info['quelle']}\n"
            f"    Lizenz: {info['lizenz']}\n"
            f"    Fuer:   {info['wofuer']}")


def modelle_aufloesen(benoetigt, args, spiel, at_ordner):
    pfade = {}
    for k in sorted(benoetigt):
        p = finde_modell(k, args, spiel, at_ordner)
        while p is None and sys.stdin.isatty() and not args.nicht_fragen:
            print(T(f"\nModell '{k}' nicht gefunden. Bitte selbst herunterladen:\n{modell_hinweis(k)}",
                    f"\nModel '{k}' not found. Please download it yourself:\n{modell_hinweis(k)}"))
            antwort = input(T("Pfad zur Modelldatei (leer = abbrechen): ", "Path to the model file (empty = cancel): ")).strip().strip('"')
            if not antwort:
                break
            p = antwort if os.path.isfile(antwort) else None
            if p is None:
                print(T(f"Datei nicht gefunden: {antwort}", f"File not found: {antwort}"))
        if p is None:
            print(T(f"\nFEHLER: Modell '{k}' fehlt. Bitte selbst herunterladen und in einen Ordner 'models/' legen "
                    f"(neben die AT.exe, in den Spielordner oder ins aktuelle Verzeichnis) oder mit --{k} DATEI angeben:\n"
                    f"{modell_hinweis(k)}\nDer Generator laedt nichts selbst herunter.",
                    f"\nERROR: model '{k}' is missing. Please download it and put it into a folder 'models/' (next to "
                    f"AT.exe, in the game folder or the current folder) or pass --{k} FILE:\n{modell_hinweis(k)}\n"
                    "The generator does not download anything itself."), file=sys.stderr)
            raise SystemExit(2)
        pfade[k] = os.path.abspath(p)
    return pfade


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for block in iter(lambda: fh.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


# ---------------------------------------------------------------- Export

def exportieren(spiel, original, plan, kategorien):
    n_bilder = n_schriften = fehler = 0
    if kategorien - {"schriften", "figuren", "animationen"}:
        archive = list(gli_export.find_archives([spiel], original))
        for i, path in enumerate(archive, 1):
            folder = os.path.basename(os.path.dirname(os.path.abspath(path))).lower()
            rel = folder + "/" + os.path.basename(path).lower()
            if not (plan.archiv_kategorien(rel) & kategorien):
                continue
            try:
                n, _ = gli_export.export(path, original)
                n_bilder += n
            except (ValueError, struct.error) as exc:
                fehler += 1
                print(f"  {path}: {T('FEHLER', 'ERROR')} {exc}", file=sys.stderr)
            fortschritt(T(f"Export Grafiken {i}/{len(archive)}: {rel}", f"Exporting graphics {i}/{len(archive)}: {rel}"))
        fortschritt_ende()
    if "schriften" in kategorien:
        for path in mcf_export.find_fonts([spiel], original):
            folder = os.path.basename(os.path.dirname(os.path.abspath(path))).lower()
            out = os.path.join(original, folder, os.path.basename(path).lower() + ".png")
            if os.path.exists(out):
                continue
            try:
                width, height, chars, rows = mcf_export.read_mcf(path)
            except (ValueError, struct.error) as exc:
                fehler += 1
                print(f"  {path}: {T('FEHLER', 'ERROR')} {exc}", file=sys.stderr)
                continue
            os.makedirs(os.path.dirname(out), exist_ok=True)
            mcf_export.write_png(out, width, height * chars, rows)
            n_schriften += 1
    n_figuren = 0
    if "figuren" in kategorien:
        n_figuren, f = pol_export.export(spiel, original)
        fehler += f
    print(T(f"Export: {n_bilder} Bilder, {n_schriften} Schriften und {n_figuren} Figuren neu exportiert"
            + (f", {fehler} Dateien mit Fehler" if fehler else "") + ".",
            f"Export: {n_bilder} images, {n_schriften} fonts and {n_figuren} figures newly exported"
            + (f", {fehler} files with errors" if fehler else "") + "."))
    return fehler


# ---------------------------------------------------------------- Auftraege

class Auftrag:
    __slots__ = ("quelle", "ziel", "rel", "verfahren", "modell", "kategorie", "ink", "bg", "par")

    def __init__(self, quelle, ziel, rel, verfahren, modell, kategorie, ink=None, bg=None, par=None):
        self.quelle, self.ziel, self.rel = quelle, ziel, rel
        self.verfahren, self.modell, self.kategorie = verfahren, modell, kategorie
        self.ink, self.bg, self.par = ink, bg, par or {}

    def schluessel(self):
        """MD5 der Eingabe plus alles, was das Ergebnis bestimmt."""
        with open(self.quelle, "rb") as fh:
            md5 = hashlib.md5(fh.read()).hexdigest()
        extra = json.dumps([self.ink, self.bg, self.par], sort_keys=True) if self.kategorie in ("schriften", "animationen") else ""
        stand = VERFAHREN_STAND.get(self.verfahren, 1)
        return f"{md5}|{self.verfahren}|{self.modell}|{extra}" + (f"|v{stand}" if stand > 1 else "")


def video_clip(ordner):
    """frames.csv eines Clips aus dem Engine-Export (H25a): (Raeume, Breite, Hoehe, Boxen je Bild, Zuschnitt).
    Zuschnitt: Bounding-Box aller Bilder plus VIDEO_RAND, im Bild; None, wenn alle Bilder leer sind."""
    raeume, w, h, boxen = [], 0, 0, []
    with open(os.path.join(ordner, "frames.csv"), encoding="utf-8") as fh:
        for line in fh:
            t = line.strip().split(";")
            if t[0] == "rooms":
                raeume = [r for r in t[1:] if r]
            elif t[0] == "size":
                w, h = int(t[1]), int(t[2])
            elif t[0] == "frame":
                x0, y0, x1, y1 = (int(v) for v in t[2:6])
                boxen.append(None if x1 < 0 else (x0, y0, x1, y1))
    b = [x for x in boxen if x]
    crop = None
    if b:
        crop = (max(0, min(x[0] for x in b) - VIDEO_RAND), max(0, min(x[1] for x in b) - VIDEO_RAND),
                min(w, max(x[2] for x in b) + VIDEO_RAND), min(h, max(x[3] for x in b) + VIDEO_RAND))
    return raeume, w, h, boxen, crop


def png_groesse(pfad):
    """Breite, Hoehe aus dem PNG-Kopf (ohne Bildbibliothek); None, wenn keine PNG."""
    try:
        with open(pfad, "rb") as fh:
            kopf = fh.read(24)
        if kopf[:8] == b"\x89PNG\r\n\x1a\n" and kopf[12:16] == b"IHDR":
            return struct.unpack(">II", kopf[16:24])
    except OSError:
        pass
    return None


def auftraege_sammeln(original, ziel, plan, kategorien, raeume=None):
    auftraege, unbekannte_schriften = [], []
    clips, leer, andere_raeume = {}, 0, set()
    for root, dirs, files in os.walk(original):
        dirs.sort()
        for f in sorted(files):
            if not f.endswith(".png"):
                continue
            quelle = os.path.join(root, f)
            rel = os.path.relpath(quelle, original).replace(os.sep, "/")
            if rel.startswith("video/"):
                # Animationen (H25b): Bilder aus dem Engine-Export, zugeschnitten auf die Box des Clips
                if "animationen" not in kategorien:
                    continue
                if root not in clips:
                    try:
                        clips[root] = video_clip(root)
                    except (OSError, ValueError, IndexError):
                        clips[root] = None
                info = clips[root]
                if info is None:
                    continue
                c_raeume, _, _, boxen, crop = info
                if raeume and not any(r.startswith(p) for r in c_raeume for p in raeume):
                    andere_raeume.add(root)
                    continue
                n = int(f[:-4]) if f[:-4].isdigit() else -1
                if crop is None or n < 0 or n >= len(boxen) or boxen[n] is None:
                    leer += 1  # leeres Bild: bleibt 1x (zeigt ohnehin nichts)
                    continue
                verfahren, modell, kat = plan.bild(rel[:-len(".png")])
                a = Auftrag(quelle, os.path.join(ziel, *rel.split("/")), rel, verfahren, modell, kat, par={"crop": list(crop)})
            elif f.endswith(".mcf.png"):
                name = f[:-len(".mcf.png")]
                verfahren, modell, ink, bg, par, bekannt = plan.schrift(name)
                if not bekannt:
                    unbekannte_schriften.append(name)
                a = Auftrag(quelle, os.path.join(ziel, *rel.split("/")), rel, verfahren, modell, "schriften", ink, bg, par)
            else:
                verfahren, modell, kat = plan.bild(rel[:-len(".png")])
                a = Auftrag(quelle, os.path.join(ziel, *rel.split("/")), rel, verfahren, modell, kat)
            if a.kategorie in kategorien:
                auftraege.append(a)
    if unbekannte_schriften:
        print(T(f"Hinweis: {len(unbekannte_schriften)} Schrift(en) ohne Eintrag im Manifest, Verfahren schrift_standard: ",
                f"Note: {len(unbekannte_schriften)} font(s) without manifest entry, method schrift_standard: ")
              + ", ".join(unbekannte_schriften))
    if "animationen" not in kategorien and os.path.isdir(os.path.join(original, "video")):
        print(T("Hinweis: Animationen (Video-Export vorhanden) gehoeren nicht zum Standardumfang: "
                "--alles oder --nur animationen [--raum <name>]",
                "Note: animations (video export present) are not part of the default scope: "
                "--alles or --nur animationen [--raum <name>]"))
    if "animationen" in kategorien:
        nclips = len([c for c in clips.values() if c]) - len(andere_raeume)
        if not clips:
            print(T(f"Hinweis: kein Video-Export in {os.path.join(original, 'video')}. Die Animationen exportiert das Spiel "
                    "selbst: AT.exe --hd-export alle (oder Raeume) starten; HD_generate.bat macht das automatisch.",
                    f"Note: no video export in {os.path.join(original, 'video')}. The game exports the animations itself: "
                    "run AT.exe --hd-export alle (or rooms); HD_generate.bat does this automatically."))
        else:
            print(T(f"Animationen: {nclips} Clips" + (f" (Raum {', '.join(raeume)})" if raeume else "")
                    + (f", {leer} leere Bilder bleiben 1x" if leer else ""),
                    f"Animations: {nclips} clips" + (f" (room {', '.join(raeume)})" if raeume else "")
                    + (f", {leer} empty frames stay 1x" if leer else "")))
    auftraege.sort(key=lambda a: (KATEGORIEN.index(a.kategorie), a.rel))
    return auftraege


def crop_csv_schreiben(ziel, auftraege):
    """Animationen: crop.csv je Clip (x0;y0;x1;y1 im 1x-Bild), daran erkennt die Engine die zugeschnittenen Bilder."""
    for ordner, crop in sorted({(os.path.dirname(a.ziel), tuple(a.par["crop"])) for a in auftraege if a.kategorie == "animationen"}):
        pfad = os.path.join(ordner, "crop.csv")
        inhalt = ";".join(str(v) for v in crop) + "\n"
        try:
            with open(pfad, encoding="utf-8") as fh:
                if fh.read() == inhalt:
                    continue
        except OSError:
            pass
        os.makedirs(ordner, exist_ok=True)
        with open(pfad + ".tmp", "w", encoding="utf-8") as fh:
            fh.write(inhalt)
        os.replace(pfad + ".tmp", pfad)


def falsche_groesse(a):
    """Animationen: vorhandene Ausgabe passt nicht zum Zuschnitt (z. B. Vollformat von Hand) -> neu rechnen."""
    if a.kategorie != "animationen":
        return False
    x0, y0, x1, y1 = a.par["crop"]
    return png_groesse(a.ziel) != ((x1 - x0) * 4, (y1 - y0) * 4)


# ---------------------------------------------------------------- Rechnen

def rechnen(a, hm, modelle):
    from PIL import Image
    im = Image.open(a.quelle)
    if a.par.get("crop"):
        im = im.crop(tuple(a.par["crop"]))  # Animationen: Ausschnitt des Clips (crop.csv)
    model = hm.load_model(modelle[a.modell], ARGS.threads) if a.modell else None
    if a.verfahren in hm.BILD_VERFAHREN:
        return hm.BILD_VERFAHREN[a.verfahren](model, im)
    if a.verfahren == "schrift_standard":
        return hm.m_schrift_standard(model, im)
    if a.verfahren == "schrift_standard_v1":
        return hm.m_schrift_standard_v1(model, im)
    if a.verfahren == "schrift_projektion":
        return hm.m_schrift_projektion(model, im, a.ink, a.bg)
    if a.verfahren == "schrift_bikubisch":
        return hm.m_schrift_bikubisch(im, a.ink, a.bg, gain=a.par.get("gain", 2.5), off=a.par.get("off", 0.1))
    raise ValueError(f"unbekanntes Verfahren {a.verfahren}")


def speichern(img, ziel):
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    tmp = ziel + ".tmp"
    img.save(tmp, format="PNG")
    os.replace(tmp, ziel)  # nie eine halbe PNG unter dem endgueltigen Namen


def kopieren(quelle, ziel):
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    tmp = ziel + ".tmp"
    shutil.copyfile(quelle, tmp)
    os.replace(tmp, ziel)


class Cache:
    """MD5-Cache: Schluessel -> bereits geschriebene Ausgabe (relativ zu hd/). Bleibt ueber Laeufe erhalten."""

    def __init__(self, path, ziel):
        self.path, self.ziel, self.map, self.zuletzt = path, ziel, {}, {}
        if os.path.exists(path):
            with open(path, encoding="utf-8") as fh:
                for line in fh:
                    try:
                        k, rel = json.loads(line)
                        self.map[k] = rel
                        self.zuletzt[rel] = k  # mit welchem Schluessel die Ausgabe zuletzt entstand
                    except ValueError:
                        pass  # abgebrochene letzte Zeile
        self.fh = open(path, "a", encoding="utf-8")

    def veraltet(self, rel, k):
        """Ausgabe mit anderem Schluessel entstanden (Verfahren, Modell oder Eingabe geaendert)? Ausgaben ohne Eintrag
        (z. B. von Hand abgelegt) gelten nicht als veraltet und bleiben unangetastet."""
        alt = self.zuletzt.get(rel)
        return alt is not None and alt != k

    def get(self, k):
        rel = self.map.get(k)
        pfad = os.path.join(self.ziel, *rel.split("/")) if rel else None
        if pfad and os.path.exists(pfad):
            return pfad
        return None

    def put(self, k, rel):
        self.map[k] = rel
        self.zuletzt[rel] = k
        self.fh.write(json.dumps([k, rel]) + "\n")
        self.fh.flush()


# ---------------------------------------------------------------- Fortschritt und Log

_TTY = sys.stdout.isatty()
_letzte = [0.0, 0.0]  # letzte Fortschrittszeile auf dem Bildschirm (ohne Konsole) und im Log
_LOG = [None]


class _Tee:
    """--log: alles, was auf dem Bildschirm erscheint, auch an die Logdatei anhaengen."""

    def __init__(self, strom, log):
        self.strom, self.log = strom, log

    def write(self, text):
        self.strom.write(text)
        self.log.write(text)
        return len(text)

    def flush(self):
        self.strom.flush()
        self.log.flush()

    def isatty(self):
        return self.strom.isatty()


def log_starten(pfad):
    log = open(pfad, "a", encoding="utf-8", errors="replace", buffering=1)
    log.write(f"\n===== hd_generate.py {time.strftime('%Y-%m-%d %H:%M:%S')}: {' '.join(sys.argv[1:])}\n")
    _LOG[0] = log
    sys.stdout = _Tee(sys.__stdout__, log)
    sys.stderr = _Tee(sys.__stderr__, log)


def fortschritt(text):
    jetzt = time.time()
    if _LOG[0] and jetzt - _letzte[1] > 60:  # Fortschritt ins Log nur einmal pro Minute
        _letzte[1] = jetzt
        _LOG[0].write(time.strftime("%H:%M:%S ") + text + "\n")
    konsole = sys.__stdout__ if _LOG[0] else sys.stdout
    if _TTY:
        konsole.write("\r" + text[:118].ljust(118))
        konsole.flush()
    elif jetzt - _letzte[0] > 10:
        _letzte[0] = jetzt
        konsole.write(text + "\n")
        konsole.flush()


def fortschritt_ende():
    if _TTY:
        (sys.__stdout__ if _LOG[0] else sys.stdout).write("\n")


def dauer(s):
    s = int(s)
    return f"{s // 3600}:{s // 60 % 60:02d}:{s % 60:02d}"


# ---------------------------------------------------------------- Hauptprogramm

ARGS = None


def main():
    global ARGS
    ap = argparse.ArgumentParser(description="HD-Paket (4x) aus der eigenen Spielkopie erzeugen")
    ap.add_argument("spiel", help="Ordner der Spielinstallation (oder darueber, z. B. das .app-Paket auf dem Mac)")
    ap.add_argument("--ausgabe", help="Zielordner hd/ (Standard: hd/ neben der AT.exe)")
    ap.add_argument("--arbeit", help="Arbeitsordner fuer Export und Cache (Standard: hd_assets/ neben hd/)")
    ap.add_argument("--nur", help="nur diese Kategorien, mit Komma getrennt: " + ", ".join(KATEGORIEN)
                    + " (animationen nur auf Wunsch, nicht im Standardumfang)")
    ap.add_argument("--alles", action="store_true", help="alle Kategorien einschliesslich der Animationen")
    ap.add_argument("--raum", help="Animationen nur fuer diese Raeume (Anfang des Namens genuegt, mit Komma getrennt), "
                                   "z. B. duty oder bank,makler")
    ap.add_argument("--gpu", action="store_true",
                    help="auf der Grafikkarte rechnen: CUDA (NVIDIA) oder MPS (Mac mit Apple-Chip), sonst CPU; nicht bitgleich zur CPU")
    ap.add_argument("--threads", type=int, default=4, help="CPU-Threads fuer torch (Standard 4, wie beim Referenzpaket)")
    ap.add_argument("--modelle", help="Ordner mit den Modelldateien (sonst models/ suchen)")
    ap.add_argument("--ultrasharp", help="Pfad zu 4x-UltraSharp.pth")
    ap.add_argument("--x4plus", help="Pfad zu RealESRGAN_x4plus.pth")
    ap.add_argument("--nicht-fragen", action="store_true", help="bei fehlendem Modell nicht nach dem Pfad fragen")
    ap.add_argument("--liste", action="store_true", help="nur exportieren und zaehlen, nichts rechnen")
    ap.add_argument("--bausteine-modellkante", action="store_true", default=True,
                    help="Hallen-Bausteine mit colorkey_rgba_modellkante, Kante per Modell (Standard seit H29)")
    ap.add_argument("--bausteine-lanczos", dest="bausteine_modellkante", action="store_false",
                    help="Hallen-Bausteine wie vor H29 mit colorkey_rgba (Kante per Lanczos); "
                         "vorhandene Bausteine werden dann neu gerechnet")
    ap.add_argument("--log", help="Ausgaben zusaetzlich an diese Datei anhaengen (Fortschritt einmal pro Minute)")
    ap.add_argument("--english", action="store_true", help="Meldungen auf Englisch (fuer HD_generate.bat)")
    ARGS = args = ap.parse_args()
    EN[0] = args.english
    if args.log:
        log_starten(args.log)

    if args.alles and args.nur:
        raise SystemExit(T("--alles und --nur schliessen sich aus", "--alles and --nur cannot be combined"))
    if args.alles:
        kategorien = set(KATEGORIEN)
    elif args.nur:
        kategorien = {k.strip() for k in args.nur.split(",") if k.strip()}
        falsch = kategorien - set(KATEGORIEN)
        if falsch:
            raise SystemExit(T("--nur: unbekannte Kategorie ", "--nur: unknown category ") + ", ".join(sorted(falsch))
                             + T(" (erlaubt: ", " (allowed: ") + ", ".join(KATEGORIEN) + ")")
    else:
        kategorien = set(KATEGORIEN) - NUR_AUF_WUNSCH
    raeume = [r.strip().lower() for r in args.raum.split(",") if r.strip()] if args.raum else None
    if raeume and "animationen" not in kategorien:
        print(T("Hinweis: --raum gilt nur fuer die Animationen (--nur animationen)", "Note: --raum only applies to animations (--nur animationen)"))

    spiel = os.path.abspath(os.path.expanduser(args.spiel))
    if not os.path.isdir(spiel):
        raise SystemExit(T(f"Spielordner nicht gefunden: {spiel}", f"Game folder not found: {spiel}"))
    if args.gpu:
        os.environ.setdefault("PYTORCH_ENABLE_MPS_FALLBACK", "1")  # vor dem Import von torch: fehlende MPS-Operationen auf der CPU
    daten = finde_datenordner(spiel)
    if daten is None:
        raise SystemExit(T(f"Keine Spieldaten gefunden (Ordner gli/ mit .gli-Dateien) in {spiel}",
                           f"No game data found (folder gli/ with .gli files) in {spiel}"))
    at_ordner = daten
    if not args.ausgabe and not hat_at(daten):
        print(T(f"Hinweis: Im Datenordner liegt noch keine AT.exe bzw. AT. Das Spiel sucht hd/ neben dem Programm; "
                f"die neue AT.exe bzw. AT gehoert deshalb in diesen Ordner: {daten}",
                f"Note: there is no AT.exe or AT in the data folder yet. The game looks for hd/ next to the program, "
                f"so the new AT.exe or AT belongs into this folder: {daten}"))
    spiel = daten  # nur aus dem Datenordner exportieren (das Mac-Bundle enthaelt mehr als die Spieldaten)
    ziel = os.path.abspath(os.path.expanduser(args.ausgabe) if args.ausgabe else os.path.join(at_ordner, "hd"))
    arbeit = os.path.abspath(os.path.expanduser(args.arbeit) if args.arbeit else os.path.join(os.path.dirname(ziel), "hd_assets"))
    original = os.path.join(arbeit, "original")

    umfang = ", ".join(k for k in KATEGORIEN if k in kategorien)
    print(T(f"Spiel:   {spiel}\nAusgabe: {ziel}\nArbeit:  {arbeit}\nUmfang:  {umfang}",
            f"Game:    {spiel}\nOutput:  {ziel}\nWork:    {arbeit}\nScope:   {umfang}"))
    plan = Plan(bausteine_modellkante=args.bausteine_modellkante)
    fehler = exportieren(spiel, original, plan, kategorien)
    auftraege = auftraege_sammeln(original, ziel, plan, kategorien, raeume)
    crop_csv_schreiben(ziel, auftraege)

    zaehler = {}
    for a in auftraege:
        zaehler[(a.kategorie, a.verfahren)] = zaehler.get((a.kategorie, a.verfahren), 0) + 1
    for (kat, verf), n in sorted(zaehler.items(), key=lambda x: (KATEGORIEN.index(x[0][0]), x[0][1])):
        print(f"  {kat:<11} {verf:<25} {n:6d}")
    os.makedirs(arbeit, exist_ok=True)
    cache = Cache(os.path.join(arbeit, "hd_cache.jsonl"), ziel)
    fehlend = [a for a in auftraege if not os.path.exists(a.ziel)]
    veraltet = [a for a in auftraege if os.path.exists(a.ziel) and (cache.veraltet(a.rel, a.schluessel()) or falsche_groesse(a))]
    offen = sorted(fehlend + veraltet, key=lambda a: (KATEGORIEN.index(a.kategorie), a.rel))
    print(T(f"{len(auftraege)} Bilder, davon {len(auftraege) - len(offen)} schon vorhanden, {len(offen)} offen"
            + (f" ({len(veraltet)} neu: Verfahren oder Zuschnitt geaendert)" if veraltet else "") + ".",
            f"{len(auftraege)} images, {len(auftraege) - len(offen)} already done, {len(offen)} to do"
            + (f" ({len(veraltet)} again: method or crop changed)" if veraltet else "") + "."))
    if args.liste or not offen:
        return 1 if fehler else 0

    # Abhaengigkeiten erst jetzt: Export und --liste kommen ohne torch aus
    try:
        import numpy as np
        import PIL  # noqa: F401
    except ImportError as exc:
        raise SystemExit(T(f"Fehlendes Python-Paket ({exc.name}). Bitte: pip install -r tools/hdgen/requirements.txt",
                           f"Missing Python package ({exc.name}). Please run: pip install -r tools/hdgen/requirements.txt"))
    benoetigt = {a.modell for a in offen if a.modell}
    if benoetigt:
        try:
            import torch
            import spandrel  # noqa: F401
        except ImportError as exc:
            raise SystemExit(T(f"Fehlendes Python-Paket ({exc.name}). Bitte: pip install -r tools/hdgen/requirements.txt",
                           f"Missing Python package ({exc.name}). Please run: pip install -r tools/hdgen/requirements.txt"))
    import hd_methods as hm

    modelle = modelle_aufloesen(benoetigt, args, spiel, at_ordner)
    for k, p in sorted(modelle.items()):
        print(f"{T('Modell', 'Model')} {k}: {p}\n  SHA-256 {sha256(p)}")

    # Deterministisch: feste Seeds, deterministische Algorithmen, feste Thread-Zahl
    random.seed(0)
    np.random.seed(0)
    if benoetigt:
        torch.manual_seed(0)
        torch.use_deterministic_algorithms(True, warn_only=True)
        torch.set_num_threads(args.threads)
        hm.DEVICE, meldung = waehle_geraet(torch, args.gpu)
        if hm.DEVICE == "cuda":
            os.environ.setdefault("CUBLAS_WORKSPACE_CONFIG", ":4096:8")
            torch.backends.cudnn.benchmark = False
            torch.backends.cudnn.deterministic = True
        if meldung:
            print(meldung)
        if hm.DEVICE == "cpu":
            print(T(f"Rechne auf der CPU mit {args.threads} Threads. Das ganze Paket dauert so viele Stunden; "
                    "Abbruch mit Strg+C, ein neuer Start setzt fort.",
                    f"Computing on the CPU with {args.threads} threads. The whole package takes many hours; "
                    "stop with Ctrl+C, the next start resumes."))

    start = time.time()
    gerechnet = kopiert = 0
    rechenzeit = 0.0
    try:
        for i, a in enumerate(offen, 1):
            k = a.schluessel()
            vorhanden = cache.get(k)
            if vorhanden and vorhanden != a.ziel:
                kopieren(vorhanden, a.ziel)
                cache.put(k, a.rel)
                kopiert += 1
            else:
                t0 = time.time()
                try:
                    img = rechnen(a, hm, modelle)
                except Exception as exc:  # ein kaputtes Bild soll den Lauf nicht beenden
                    fortschritt_ende()
                    print(f"{T('FEHLER', 'ERROR')} {a.rel} ({a.verfahren}): {exc}", file=sys.stderr)
                    fehler += 1
                    continue
                speichern(img, a.ziel)
                cache.put(k, a.rel)
                gerechnet += 1
                rechenzeit += time.time() - t0
            rest = len(offen) - i
            eta = rechenzeit / gerechnet * rest if gerechnet else 0
            fortschritt(f"[{i}/{len(offen)}] {100 * i // len(offen)}% | "
                        + T(f"gerechnet {gerechnet}, aus Cache {kopiert} | Rest ca. {dauer(eta)}",
                            f"computed {gerechnet}, from cache {kopiert} | remaining approx. {dauer(eta)}")
                        + f" | {a.rel}")
    except KeyboardInterrupt:
        fortschritt_ende()
        print(T(f"Abgebrochen. {gerechnet} gerechnet, {kopiert} aus dem Cache. Ein neuer Start setzt fort.",
                f"Stopped. {gerechnet} computed, {kopiert} from cache. The next start resumes."))
        return 130
    fortschritt_ende()
    print(T(f"Fertig in {dauer(time.time() - start)}: {gerechnet} gerechnet, {kopiert} aus dem Cache kopiert"
            + (f", {fehler} Fehler" if fehler else "") + f".\nHD-Paket: {ziel}",
            f"Done in {dauer(time.time() - start)}: {gerechnet} computed, {kopiert} copied from cache"
            + (f", {fehler} errors" if fehler else "") + f".\nHD package: {ziel}"))
    print(T("Im Spiel: in AT.json \"OptionRenderScale\": 4 setzen (siehe ASSETS.md).",
            "In the game: set \"OptionRenderScale\": 4 in AT.json (see ASSETS.md)."))
    return 1 if fehler else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        fortschritt_ende()
        print(T("Abgebrochen. Ein neuer Start setzt fort.", "Stopped. The next start resumes."))
        sys.exit(130)
