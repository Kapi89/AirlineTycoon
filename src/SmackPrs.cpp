//============================================================================================
// SmackPrs.cpp : Wrapper-Klasse für den Smacker für 16Bit Farben bei 8-Bit Videos:
//============================================================================================
#include "SmackPrs.h"

#include "global.h"
#include "helper.h"
#include "Proto.h"

#include <smacker.h>

#include <cmath>
#include <SDL_image.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <list>
#include <fstream>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

#define AT_Error(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "SMACK", __VA_ARGS__)
#define AT_Warn(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "SMACK", __VA_ARGS__)
#define AT_Info(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "SMACK", __VA_ARGS__)
#define AT_Log(...) AT_Log_I("SMACK", __VA_ARGS__)

//--------------------------------------------------------------------------------------------
// Berechnet eine Remapper-Tabelle anhand einer 256-Farben Palette
//--------------------------------------------------------------------------------------------
void CalculatePalettemapper(const UBYTE *pPalette, SDL_Palette *pPaletteMapper) {
    if (pPalette == nullptr) {
        return;
    }

    SDL_Color colors[256];

    for (SLONG c = 0; c < 256; c++) {
        if (pPalette[c * 3] + pPalette[c * 3 + 1] + pPalette[c * 3 + 2] == 0) {
            colors[c] = SDL_Color{4, 4, 4, 0xFF};
        } else {
            colors[c] = SDL_Color{pPalette[c * 3], pPalette[c * 3 + 1], pPalette[c * 3 + 2], 0xFF};
        }
    }

    colors[0] = SDL_Color{0, 0, 0, 255};
    SDL_SetPaletteColors(pPaletteMapper, colors, 0, 256);
}

void CalculatePalettemapperFlc(const flic::Colormap &colormap, SDL_Palette *pPaletteMapper) {
    SDL_Color colors[256];

    for (SLONG c = 0; c < 256; c++) {
        if (colormap[c].r + colormap[c].g + colormap[c].b == 0) {
            colors[c] = SDL_Color{4, 4, 4, 0xFF};
        } else {
            colors[c] = SDL_Color{colormap[c].r, colormap[c].g, colormap[c].b, 0xFF};
        }
    }

    colors[0] = SDL_Color{0, 0, 0, 255};
    SDL_SetPaletteColors(pPaletteMapper, colors, 0, 256);
}

//--------------------------------------------------------------------------------------------
// Wrapper around FLIC library, containing file handles and all data structures
//--------------------------------------------------------------------------------------------
FlcWrapper::FlcWrapper(fs::path fileInput) : Path{fileInput} {
    if (!fs::exists(Path)) {
        AT_Error("FlcWrapper: File %s does not exist!", Path.c_str());
        return;
    }
    init_decoder();
}

FlcWrapper::~FlcWrapper() {
    Decoder = {nullptr};
    FileInterface = {nullptr};
    fclose(f);
    f = nullptr;
}

void FlcWrapper::info_all(unsigned long &FrameNum, unsigned long &Frames) {
    FrameNum = Decoder.getCurrentFrameCount();
    Frames = Header.frames;
}

void FlcWrapper::info_video(unsigned long &Width, unsigned long &Height, unsigned long &Speed) {
    Width = Header.width;
    Height = Header.height;
    Speed = Header.speed;
}

char FlcWrapper::first() {
    memset(Buffer.data(), 0x0, Buffer.size() * sizeof(uint8_t));
    init_decoder();
    return render();
}

char FlcWrapper::next() { return render(); }

const unsigned char *FlcWrapper::get_video() { return Buffer.data(); }

const flic::Colormap &FlcWrapper::get_palette() { return Frame.colormap; }

long FlcWrapper::current_index() { return long(Decoder.getCurrentFrameCount()) - 1; }

void FlcWrapper::init_decoder() {
    if (f) {
        fclose(f);
    }

    f = std::fopen(Path.string().c_str(), "rb");
    if (!f) {
        AT_Error("FlcWrapper: Failed to open %s: errno %d!", Path.c_str(), errno);
        return;
    }

    FileInterface = {f};
    if (!FileInterface.ok()) {
        AT_Error("FlcWrapper: Failed to create file interface for %s!", Path.c_str());
        FileInterface = {nullptr};
        fclose(f);
        f = nullptr;
        return;
    }

    // FileInterface.seek(0);

    Decoder = {&FileInterface};
    if (!Decoder.readHeader(Header)) {
        AT_Error("FlcWrapper: Failed to read header of %s!", Path.c_str());
        Decoder = {nullptr};
        return;
    }

    Buffer.resize(Header.width * Header.height);
    Frame.rowstride = Header.width;
}

char FlcWrapper::render() {
    Frame.pixels = Buffer.data();
    if (!Decoder.readFrame(Frame)) {
        AT_Error("FlcWrapper: Failed to decode frame!");
    }

    int cur = Decoder.getCurrentFrameCount();
    if (cur >= 0 && cur < Header.frames - 1) {
        // AT_Log("%s: SMK_MORE %lu/%lu", Path.c_str(), cur, Header.frames);
        return SMK_MORE;
    }
    if (cur == Header.frames - 1) {
        // AT_Log("%s: SMK_LAST %lu/%lu", Path.c_str(), cur, Header.frames);
        return SMK_LAST;
    }
    if (cur == Header.frames) {
        // AT_Log("%s: SMK_DONE %lu/%lu", Path.c_str(), cur, Header.frames);
        return SMK_DONE;
    }
    AT_Error("%s: SMK_ERROR %lu/%lu", Path.c_str(), cur, Header.frames);
    return SMK_ERROR;
}

//--------------------------------------------------------------------------------------------
// CSmack16::
//--------------------------------------------------------------------------------------------
// Destruktor:
//--------------------------------------------------------------------------------------------
CSmack16::~CSmack16() {
    if (PaletteMapper != nullptr) {
        SDL_FreePalette(PaletteMapper);
    }
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;
}

//--------------------------------------------------------------------------------------------
// Öffnet ein Smacker-Filmchen:
//--------------------------------------------------------------------------------------------
void CSmack16::Open(const CString &Filename) {
    fs::path path{Filename.c_str()};
    fs::path rootPath{SmackerPath.c_str()};
    auto pathToSmk = FullFilesystemPath(path, rootPath);
    auto pathToFlc = FullFilesystemPath(path.replace_extension("flc"), rootPath);

    if (fs::exists(pathToSmk)) {
        pSmack = smk_open_file(FullFilename(Filename, SmackerPath), SMK_MODE_MEMORY);
        smk_enable_video(pSmack, 1U);
        smk_info_video(pSmack, &Width, &Height, nullptr);
        FrameNext = 0;
        State = smk_first(pSmack);
        PaletteMapper = SDL_AllocPalette(256);
        CalculatePalettemapper(smk_get_palette(pSmack), PaletteMapper);
    } else if (fs::exists(pathToFlc)) {
        // AT_Info("Using FLC fallback: %s", pathToFlc.c_str());
        pFlc = new FlcWrapper(pathToFlc);
        pFlc->info_video(Width, Height, Speed);
        FrameNext = 0;
        State = pFlc->first();
        PaletteMapper = SDL_AllocPalette(256);
        CalculatePalettemapperFlc(pFlc->get_palette(), PaletteMapper);
    }
}

//--------------------------------------------------------------------------------------------
// Nächster Frame:
//--------------------------------------------------------------------------------------------
BOOL CSmack16::NextSmk(SBBM *pTargetBm) {
    if (AtGetTime() >= FrameNext && State == SMK_MORE) {
        // Take the next frame:
        State = smk_next(pSmack);

        DOUBLE usf = NAN;
        smk_info_all(pSmack, nullptr, nullptr, &usf);
        FrameNext = AtGetTime() + (usf / 1000.0);

        if (pTargetBm != nullptr) {
            if (SLONG(Width) != pTargetBm->Size.x || SLONG(Height) != pTargetBm->Size.y) {
                pTargetBm->ReSize(XY(Width, Height), CREATE_INDEXED);
            }
            SDL_SetSurfacePalette(pTargetBm->pBitmap->GetSurface(), PaletteMapper);
            SB_CBitmapKey key(*pTargetBm->pBitmap);
            memcpy(key.Bitmap, smk_get_video(pSmack), key.lPitch * Height);
        }
    }

    return static_cast<BOOL>(State == SMK_MORE);
}

BOOL CSmack16::NextFlc(SBBM *pTargetBm) {
    if (AtGetTime() >= FrameNext && State == SMK_MORE) {
        // Take the next frame:
        State = pFlc->next();

        FrameNext = AtGetTime() + Speed;

        if (pTargetBm != nullptr) {
            if (SLONG(Width) != pTargetBm->Size.x || SLONG(Height) != pTargetBm->Size.y) {
                pTargetBm->ReSize(XY(Width, Height), CREATE_INDEXED);
            }
            SDL_SetSurfacePalette(pTargetBm->pBitmap->GetSurface(), PaletteMapper);
            SB_CBitmapKey key(*pTargetBm->pBitmap);
            memcpy(key.Bitmap, pFlc->get_video(), key.lPitch * Height);
        }
    }

    return static_cast<BOOL>(State == SMK_MORE);
}

BOOL CSmack16::Next(SBBM *pTargetBm) {
    if (pSmack != nullptr) {
        return NextSmk(pTargetBm);
    }
    if (pFlc != nullptr) {
        return NextFlc(pTargetBm);
    }
    return false;
}

//--------------------------------------------------------------------------------------------
// Und Feierabend:
//--------------------------------------------------------------------------------------------
void CSmack16::Close() {
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;
}

//--------------------------------------------------------------------------------------------
// CSmackerClip::
//--------------------------------------------------------------------------------------------
// Konstruktor:
//--------------------------------------------------------------------------------------------
CSmackerClip::CSmackerClip() { PaletteMapper = SDL_AllocPalette(256); }

//--------------------------------------------------------------------------------------------
// Destruktor:
//--------------------------------------------------------------------------------------------
CSmackerClip::~CSmackerClip() {
    if (PaletteMapper != nullptr) {
        SDL_FreePalette(PaletteMapper);
    }
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;
}

//--------------------------------------------------------------------------------------------
// Startet die Animation
//--------------------------------------------------------------------------------------------
void CSmackerClip::Start() {
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;

    if (Filename.GetLength() > 0) {
        fs::path path{Filename.c_str()};
        fs::path rootPath{SmackerPath.c_str()};
        auto pathToSmk = FullFilesystemPath(path, rootPath);
        auto pathToFlc = FullFilesystemPath(path.replace_extension("flc"), rootPath);

        if (fs::exists(pathToSmk)) {
            pSmack = smk_open_file(pathToSmk.string().c_str(), SMK_MODE_MEMORY);
            smk_enable_video(pSmack, 1U);
            smk_info_all(pSmack, &FrameNum, &Frames, nullptr);
            smk_info_video(pSmack, &Width, &Height, nullptr);
            smk_first(pSmack);
            CalculatePalettemapper(smk_get_palette(pSmack), PaletteMapper);
        } else if (fs::exists(pathToFlc)) {
            pFlc = new FlcWrapper(pathToFlc);
            pFlc->info_all(FrameNum, Frames);
            pFlc->info_video(Width, Height, Speed);
            pFlc->first();
            CalculatePalettemapperFlc(pFlc->get_palette(), PaletteMapper);
        }
    }

    if (IsFXPlaying == 0) {
        if (SoundFilename.GetLength() > 0) {
            if (strchr(SoundFilename, '|') != nullptr) {
                char *p = const_cast<char *>((LPCTSTR)SoundFilename);

                NumSoundFx = 1;
                while (true) {
                    p = strchr(p + 1, '|');

                    if (p != nullptr) {
                        NumSoundFx++;
                    } else {
                        break;
                    }
                }

                NextSyllable();
            } else {
                NumSoundFx = 1;
                SoundFx.ReInit(SoundFilename, SmackerPath);
            }
        } else {
            NumSoundFx = 0;
        }
    }

    RepeatCount = Repeat.x + rand() % (Repeat.y - Repeat.x + 1);
}

//--------------------------------------------------------------------------------------------
// Beendet sie:
//--------------------------------------------------------------------------------------------
void CSmackerClip::Stop() {
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;
    FrameNext = 0;
    LastFrame = 0;

    // SmackPic.ReSize (0);
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CSmackerClip::ReSize(SLONG ClipId, const CString &Filename, const CString &SoundFilename,

                          XY ScreenOffset, SLONG MoodId,
                          CRepeat Repeat,     // Min..Max
                          CPostWait PostWait, // Min..Max in 20/stel Sekunden
                          BOOL CanCancelClip, // Kann der Clip vorzeitig abgebrochen werden?

                          SLONG *PostVar, // NULL oder Variable, die am Animationsende gesetzt werden soll
                          SLONG PostOperation, SLONG PostValue,

                          const SLONG *DecisionVar, // wenn !=NULL, legt sie fest, welche Folgeanimation gespielt wird...

                          const CString &SuccessorTokens, // z.B. "A2X8"...
                          SLONG SuccessorIds, ...) {
    SLONG c = 0;

    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    if (pFlc != nullptr) {
        delete pFlc;
    }

    CSmackerClip::Filename = Filename;
    CSmackerClip::SoundFilename = SoundFilename;
    CSmackerClip::pSmack = nullptr;
    CSmackerClip::LastFrame = 0;
    CSmackerClip::Width = 0;
    CSmackerClip::Height = 0;
    CSmackerClip::FrameNum = 0;
    CSmackerClip::Frames = 0;
    CSmackerClip::FrameNext = 0;

    // Statische Eigenschaften...
    CSmackerClip::ClipId = ClipId;
    CSmackerClip::ScreenOffset = ScreenOffset;
    CSmackerClip::MoodId = MoodId;
    CSmackerClip::Repeat = Repeat;
    CSmackerClip::PostWait = PostWait;
    CSmackerClip::CanCancelClip = CanCancelClip;

    CSmackerClip::PostVar = PostVar;
    CSmackerClip::PostOperation = PostOperation;
    CSmackerClip::PostValue = PostValue;
    CSmackerClip::DecisionVar = DecisionVar;

    CSmackerClip::SuccessorTokens = SuccessorTokens;
    CSmackerClip::SuccessorIds.ReSize(SuccessorTokens.GetLength() / 2);

    CSmackerClip::SuccessorIds[0] = SuccessorIds;

    // Hilfskonstruktion für beliebige viele Argumente deklarieren:
    {
        va_list Vars;

        // Tabelle initialisieren:
        va_start(Vars, SuccessorIds);

        for (c = 1; c < CSmackerClip::SuccessorIds.AnzEntries(); c++) {
            CSmackerClip::SuccessorIds[c] = va_arg(Vars, SLONG);
        }

        // Daten bereinigen:
        va_end(Vars);
    }

    CSmackerClip::IsFXPlaying = FALSE;

    CSmackerClip::pFlc = nullptr;

    // Dynamische Eigenschaften...
    CSmackerClip::State = SMACKER_CLIP_INACTIVE;
    CSmackerClip::RepeatCount = 0;
    CSmackerClip::WaitCount = 0;
}

//--------------------------------------------------------------------------------------------
// Lässt die Person die nächste Silbe raussuchen:
//--------------------------------------------------------------------------------------------
void CSmackerClip::NextSyllable() {
    if (SoundFilename.GetLength() > 0) {
        char *p = const_cast<char *>((LPCTSTR)SoundFilename);

        if (NumSoundFx < 3) {
            for (SLONG n = rand() % NumSoundFx; n > 0; n--) {
                p = strchr(p, '|') + 1;
            }
        } else {
            for (SLONG n = SoundRandom.Rand(0, NumSoundFx - 1); n > 0; n--) {
                p = strchr(p, '|') + 1;
            }
        }

        char buffer[200];
        strncpy(buffer, p, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
        char *pp = strchr(buffer, '|');

        if (pp != nullptr) {
            *pp = 0;
        }

        SoundFx.ReInit(buffer, SmackerPath);
    }
}

//--------------------------------------------------------------------------------------------
// Lässt die Person die nächste Silbe sprechen:
//--------------------------------------------------------------------------------------------
void CSmackerClip::PlaySyllable() {
    if (SoundFx.pFX != nullptr) {
        SoundFx.Play(0, Sim.Options.OptionEffekte * 100 / 7);
        TickerNext = AtGetTime() + SoundFx.pFX->GetByteLength() * 1000 / 22000;
        IsFXPlaying = TRUE;
    } else {
        TickerNext = 0;
    }
}

//--------------------------------------------------------------------------------------------
// CSmackerPerson::
//--------------------------------------------------------------------------------------------
////Konstruktor:
//--------------------------------------------------------------------------------------------
CSmackerPerson::CSmackerPerson() {
    ActiveClip = -1;
    CurrentMood = -1;
    DesiredMood = -1;
    AcceptedMood1 = -1;
    AcceptedMood2 = -1;
}

//--------------------------------------------------------------------------------------------
// Destruktor:
//--------------------------------------------------------------------------------------------

bool CSmackerPerson::clipExists(SLONG clip) const {
    if (clip < 0 || clip >= Clips.AnzEntries()) {
        return false;
    }
    return (Clips[clip].pSmack != nullptr) || (Clips[clip].pFlc != nullptr);
}

bool CSmackerPerson::isEmptyClip(SLONG clip) const {
    if (clip < 0 || clip >= Clips.AnzEntries()) {
        return false;
    }
    return (Clips[ActiveClip].pSmack == nullptr) && (Clips[ActiveClip].pFlc == nullptr);
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CSmackerPerson::ReSize(SLONG NumberOfClips) { Clips.ReSize(NumberOfClips); }

//--------------------------------------------------------------------------------------------
// Legt fest, wie die Person labert:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::SetSpeakFx(const CString &Filename) { SpeakFx.ReInit(Filename, SmackerPath); }

//--------------------------------------------------------------------------------------------
// Legt die Stimmung (Freizeit, Reden, Zuhören) fest, in der man jemand haben will
//--------------------------------------------------------------------------------------------
void CSmackerPerson::SetDesiredMood(SLONG DesiredMood, SLONG AcceptedMood1, SLONG AcceptedMood2) {
    CSmackerPerson::DesiredMood = DesiredMood;
    CSmackerPerson::AcceptedMood1 = AcceptedMood1;
    CSmackerPerson::AcceptedMood2 = AcceptedMood2;
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
SLONG CSmackerPerson::GetMood() { return (CurrentMood); }

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
SLONG CSmackerPerson::GetDesiredMood() { return (DesiredMood); }

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
SLONG CSmackerPerson::GetClip() { return (ActiveClip); }

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
SLONG CSmackerPerson::GetFrame() const {
    if (clipExists(ActiveClip)) {
        return (Clips[ActiveClip].FrameNum);
    }
    return (0);
}

//--------------------------------------------------------------------------------------------
// Beginnt den nächsten Clip, selbst wenn es verboten ist:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::ForceNextClip() {
    Clips[ActiveClip].Stop();
    Clips[ActiveClip].State = SMACKER_CLIP_INACTIVE;
    NextClip();
}

void CSmackerPerson::CopyFrameSmk() {
    CalculatePalettemapper(smk_get_palette(Clips[ActiveClip].pSmack), Clips[ActiveClip].PaletteMapper);
    Bitmap.ReSize(XY(Clips[ActiveClip].Width, Clips[ActiveClip].Height), CREATE_INDEXED);
    SDL_SetSurfacePalette(Bitmap.pBitmap->GetSurface(), Clips[ActiveClip].PaletteMapper);
    {
        SB_CBitmapKey key(*Bitmap.pBitmap);
        memcpy(key.Bitmap, smk_get_video(Clips[ActiveClip].pSmack), key.lPitch * Clips[ActiveClip].Height);
    }
    BitmapPos = Clips[ActiveClip].ScreenOffset;
    HdBitmapClip = ActiveClip; // H25c: welches Bild steht jetzt in Bitmap
    HdBitmapFrame = SLONG(Clips[ActiveClip].FrameNum);
}

void CSmackerPerson::DecodeFrameSmk() {
    DOUBLE usf = NAN;
    if (Clips[ActiveClip].FrameNum >= Clips[ActiveClip].Frames - 1) {
        smk_first(Clips[ActiveClip].pSmack);
    } else {
        smk_next(Clips[ActiveClip].pSmack);
    }
    smk_info_all(Clips[ActiveClip].pSmack, &Clips[ActiveClip].FrameNum, &Clips[ActiveClip].Frames, &usf);
    Clips[ActiveClip].FrameNext = AtGetTime() + (usf / 1000.0);
}

void CSmackerPerson::CopyFrameFlc() {
    CalculatePalettemapperFlc(Clips[ActiveClip].pFlc->get_palette(), Clips[ActiveClip].PaletteMapper);
    Bitmap.ReSize(XY(Clips[ActiveClip].Width, Clips[ActiveClip].Height), CREATE_INDEXED);
    SDL_SetSurfacePalette(Bitmap.pBitmap->GetSurface(), Clips[ActiveClip].PaletteMapper);
    {
        SB_CBitmapKey key(*Bitmap.pBitmap);
        memcpy(key.Bitmap, Clips[ActiveClip].pFlc->get_video(), key.lPitch * Clips[ActiveClip].Height);
    }
    BitmapPos = Clips[ActiveClip].ScreenOffset;
    HdBitmapClip = ActiveClip;                                      // H25c: welches Bild steht jetzt in Bitmap
    HdBitmapFrame = SLONG(Clips[ActiveClip].pFlc->current_index()); // FrameNum ist bei FLC 1-basiert
}

void CSmackerPerson::DecodeFrameFlc() {
    if (Clips[ActiveClip].FrameNum >= Clips[ActiveClip].Frames - 1) {
        Clips[ActiveClip].pFlc->first();
    } else {
        Clips[ActiveClip].pFlc->next();
    }
    Clips[ActiveClip].pFlc->info_all(Clips[ActiveClip].FrameNum, Clips[ActiveClip].Frames);
    Clips[ActiveClip].FrameNext = AtGetTime() + Clips[ActiveClip].Speed;
}

void CSmackerPerson::CopyFrame() {
    if (ActiveClip < 0 || ActiveClip >= Clips.AnzEntries()) {
        return;
    }
    if (Clips[ActiveClip].pSmack != nullptr) {
        CopyFrameSmk();
    } else if (Clips[ActiveClip].pFlc != nullptr) {
        CopyFrameFlc();
    }
}

void CSmackerPerson::DecodeFrame() {
    if (ActiveClip < 0 || ActiveClip >= Clips.AnzEntries()) {
        return;
    }
    if (Clips[ActiveClip].pSmack != nullptr) {
        DecodeFrameSmk();
    } else if (Clips[ActiveClip].pFlc != nullptr) {
        DecodeFrameFlc();
    }
}

//--------------------------------------------------------------------------------------------
// Aktualisiert eine Animation
//--------------------------------------------------------------------------------------------
void CSmackerPerson::Pump() {
    if (Clips.AnzEntries() == 0) {
        return;
    }

    if (ActiveClip != -1 && (Clips[ActiveClip].IsFXPlaying != 0) && Clips[ActiveClip].NumSoundFx > 1 && AtGetTime() > Clips[ActiveClip].TickerNext) {
        Clips[ActiveClip].NextSyllable();
        Clips[ActiveClip].PlaySyllable();
    }

    // Wenn gerade ein Leerclip abgespielt wird:
    if (isEmptyClip(ActiveClip)) {
        Clips[ActiveClip].Stop();
        Clips[ActiveClip].State = SMACKER_CLIP_INACTIVE;
        NextClip();

        CopyFrame();
        if (Clips[ActiveClip].FrameNum == 0 && (Clips[ActiveClip].IsFXPlaying == 0)) {
            Clips[ActiveClip].PlaySyllable();
        }
        DecodeFrame();

        return;
    }

    if (ActiveClip == -1) {
        ActiveClip = 0;
        if (CurrentMood == -1) {
            CurrentMood = Clips[ActiveClip].MoodId;
        }
        if (DesiredMood == -1) {
            DesiredMood = Clips[ActiveClip].MoodId;
        }

        Clips[ActiveClip].State = SMACKER_CLIP_PLAYING;
        Clips[ActiveClip].Start();

        CopyFrame();
        if (Clips[ActiveClip].FrameNum == 0 && (Clips[ActiveClip].IsFXPlaying == 0)) {
            Clips[ActiveClip].PlaySyllable();
        }
        DecodeFrame();
    }

    if ((Clips[ActiveClip].CanCancelClip != 0) &&
        (CurrentMood != DesiredMood || ((Clips[ActiveClip].DecisionVar != nullptr) && Clips[ActiveClip].DecisionVar[0] != -1))) {
        Clips[ActiveClip].Stop();
        Clips[ActiveClip].State = SMACKER_CLIP_INACTIVE;
        NextClip();

        return;
    }

    if (Clips[ActiveClip].State == SMACKER_CLIP_PLAYING) {
        if (AtGetTime() >= Clips[ActiveClip].FrameNext) {
            CopyFrame();

            // Variablenveränderung, während der Film läuft?
            if ((Clips[ActiveClip].PostVar != nullptr) && ((Clips[ActiveClip].PostOperation & SMACKER_CLIP_FRAME) != 0)) // Variablen-Messageing:
            {
                if (Clips[ActiveClip].FrameNum >= ULONG(Clips[ActiveClip].PostOperation >> 13) &&
                    Clips[ActiveClip].LastFrame < (Clips[ActiveClip].PostOperation >> 13)) {

                    switch (Clips[ActiveClip].PostOperation & 1023) {
                    case SMACKER_CLIP_ADD:
                        Clips[ActiveClip].PostVar[0] += Clips[ActiveClip].PostValue;
                        break;
                    case SMACKER_CLIP_SUB:
                        Clips[ActiveClip].PostVar[0] -= Clips[ActiveClip].PostValue;
                        break;
                    case SMACKER_CLIP_SET:
                        Clips[ActiveClip].PostVar[0] = Clips[ActiveClip].PostValue;
                        break;
                    case SMACKER_CLIP_XOR:
                        Clips[ActiveClip].PostVar[0] ^= Clips[ActiveClip].PostValue;
                        break;
                    default:
                        break;
                    }
                }
            }

            Clips[ActiveClip].LastFrame = Clips[ActiveClip].FrameNum;

            if (Clips[ActiveClip].FrameNum >= Clips[ActiveClip].Frames - 1) {
                Clips[ActiveClip].RepeatCount--;

                if (CurrentMood != DesiredMood) {
                    Clips[ActiveClip].RepeatCount = 0;
                }

                if (Clips[ActiveClip].RepeatCount > 0) {
                    if (CurrentMood != SPM_TALKING && CurrentMood != SPM_ANGRY_TALKING) {
                        Clips[ActiveClip].PlaySyllable();
                    }
                }
            }

            if (Clips[ActiveClip].RepeatCount == 0) {
                // End of Animation...
                if (Clips[ActiveClip].PostWait.y > 0) {
                    Clips[ActiveClip].WaitCount =
                        (Clips[ActiveClip].PostWait.x + rand() % (Clips[ActiveClip].PostWait.y - Clips[ActiveClip].PostWait.x + 1)) * 50 + AtGetTime();
                    Clips[ActiveClip].State = SMACKER_CLIP_WAITING;
                } else {
                    if (CurrentMood != DesiredMood || (CurrentMood != SPM_TALKING && CurrentMood != SPM_ANGRY_TALKING)) {
                        Clips[ActiveClip].SoundFx.Stop();
                        Clips[ActiveClip].IsFXPlaying = FALSE;
                    }

                    Clips[ActiveClip].Stop();
                    Clips[ActiveClip].State = SMACKER_CLIP_INACTIVE;
                    NextClip();
                }
            } else {
                if (Clips[ActiveClip].FrameNum == 0 && (Clips[ActiveClip].IsFXPlaying == 0)) {
                    Clips[ActiveClip].PlaySyllable();
                }

                DecodeFrame();
            }
        }
    } else if (Clips[ActiveClip].State == SMACKER_CLIP_WAITING) {
        if (SLONG(AtGetTime()) > Clips[ActiveClip].WaitCount) {
            Clips[ActiveClip].Stop();
            NextClip();
        }
    }
}

//--------------------------------------------------------------------------------------------
// Sucht den Folgeclip heraus:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::NextClip() {
    SLONG c = 0;
    SLONG PropSum = 0;

    if (Clips.AnzEntries() == 0) {
        return;
    }

    if ((Clips[ActiveClip].PostVar != nullptr) && ((Clips[ActiveClip].PostOperation & SMACKER_CLIP_POST) != 0)) // Variablen-Messageing:
    {
        switch (Clips[ActiveClip].PostOperation & 1023) {
        case SMACKER_CLIP_ADD:
            Clips[ActiveClip].PostVar[0] += Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_SUB:
            Clips[ActiveClip].PostVar[0] -= Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_SET:
            Clips[ActiveClip].PostVar[0] = Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_XOR:
            Clips[ActiveClip].PostVar[0] ^= Clips[ActiveClip].PostValue;
            break;
        default:
            break;
        }
    }

    // Erste Priorität hat immer eine "DecisionVar"
    if ((Clips[ActiveClip].DecisionVar != nullptr) && Clips[ActiveClip].DecisionVar[0] != -1) {
        ActiveClip = Clips[ActiveClip].DecisionVar[0];
        goto found_next_clip;
    } else // Ansonsten entscheiden wir anhand von Zufall und Regeln
    {
        if (CurrentMood == DesiredMood) {
            // 1. Schauen, wie's weitergeht ohen die Stimmung zu ändern:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] != 'E') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] != 'E') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 2. Unmöglich, rettet bei Bugs vor dem Absturz:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
            }

            PropSum = rand() % PropSum;

            // Alternative auswählten:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                if (PropSum < 0) {
                    ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                    goto found_next_clip;
                }
            }
        } else // Die Stimmung wechseln:
        {
            // 1. Probieren, ob es Exits in die richtige Richtung gibt:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] == 'E') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] == 'E') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 2. Probieren, ob es Exits in andere, akzeptierte Richtungen gibt:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if ((Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == CurrentMood ||
                     Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood1 || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood2) &&
                    Clips[ActiveClip].SuccessorTokens[c * 2] == 'E') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if ((Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == CurrentMood ||
                         Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood1 ||
                         Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood2) &&
                        Clips[ActiveClip].SuccessorTokens[c * 2] == 'E') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 3. Probieren, ob es eine direkte Verbindung zur ZielMood gibt:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] != 'S') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] != 'S') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 4. Der Rest:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if ((Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == CurrentMood ||
                     Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood1 || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood2) &&
                    Clips[ActiveClip].SuccessorTokens[c * 2] != 'S') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if ((Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == CurrentMood ||
                         Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood1 ||
                         Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood2) &&
                        Clips[ActiveClip].SuccessorTokens[c * 2] != 'S') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 5. Unmöglich, rettet bei Bugs vor dem Absturz:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
            }

            PropSum = rand() % PropSum;

            // Alternative auswählten:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                if (PropSum < 0) {
                    ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                    goto found_next_clip;
                }
            }
        }
    }
    DebugBreak();

found_next_clip:
    BOOL LastTalking = static_cast<BOOL>(CurrentMood == SPM_TALKING || CurrentMood == SPM_ANGRY_TALKING);
    BOOL Talking = static_cast<BOOL>(Clips[ActiveClip].MoodId == SPM_TALKING || Clips[ActiveClip].MoodId == SPM_ANGRY_TALKING);

    if ((Talking != 0) && (LastTalking == 0) && ((Sim.Options.OptionTalking * Sim.Options.OptionDigiSound) != 0)) {
        SpeakFx.Play(DSBPLAY_NOSTOP | DSBPLAY_LOOPING, Sim.Options.OptionTalking * 100 / 7);
    } else if ((Talking == 0) && (LastTalking != 0)) {
        SpeakFx.Stop();
    }

    CurrentMood = Clips[ActiveClip].MoodId;
    Clips[ActiveClip].State = SMACKER_CLIP_PLAYING;
    Clips[ActiveClip].Start();

    if ((Clips[ActiveClip].PostVar != nullptr) && ((Clips[ActiveClip].PostOperation & SMACKER_CLIP_PRE) != 0)) // Variablen-Messageing:
    {
        switch (Clips[ActiveClip].PostOperation & 1023) {
        case SMACKER_CLIP_ADD:
            Clips[ActiveClip].PostVar[0] += Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_SUB:
            Clips[ActiveClip].PostVar[0] -= Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_SET:
            Clips[ActiveClip].PostVar[0] = Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_XOR:
            Clips[ActiveClip].PostVar[0] ^= Clips[ActiveClip].PostValue;
            break;
        default:
            break;
        }
    }

    if (isEmptyClip(ActiveClip) && Clips[ActiveClip].PostWait.x == 0 && Clips[ActiveClip].PostWait.y == 0) {
        NextClip();
    }
}

//--------------------------------------------------------------------------------------------
// Blittet eine Person in eine Bitmap:
//--------------------------------------------------------------------------------------------
static std::string HdClipName(const CString &filename);
static std::shared_ptr<SDL_Surface> HdFrame565(SDL_Surface *frame, const SDL_Rect &crop);

void CSmackerPerson::BlitAtT(SBBM &RoomBm, XY Offset) {
    if (clipExists(ActiveClip)) {
        RoomBm.BlitFromT(Bitmap, BitmapPos + Offset);

        // H25c: HD-Fassung des Bildes; Referenz ist eine 1x-Kopie des Bildes (bei Abweichung zeigt das Overlay 1x)
        if (SB_GetRenderScale() > 1 && Bitmap.pBitmap != nullptr && RoomBm.pBitmap != nullptr) {
            SDL_Texture *tex = nullptr;
            SLONG scale = 0;
            SDL_Rect crop{};
            if (GetHdFrame(tex, scale, crop)) {
                auto src = HdFrame565(Bitmap.pBitmap->GetSurface(), crop);
                if (src) {
                    const XY p = BitmapPos + Offset;
                    SB_RecordHdSprite(RoomBm.pBitmap, src, tex, p.x + crop.x, p.y + crop.y, scale);
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// H25c: HD-Bilder der animierten Personen (hd/video/<clip>.smk/<NNNN>.png, Nummer wie im Export von H25a)
// H25d: Laden im Hintergrund. Beim ersten Zeichnen einer Figur sucht ein Hintergrund-Thread alle HD-Bilder ihrer Clips
// zusammen (Verzeichnis, crop.csv, Groesse der PNG, gleiche Dateien) und entscheidet, ob die Figur in 4x, 2x oder 1x
// gezeigt wird (OptionHdVideoMaxMB, immer die ganze Figur gleich). Danach lesen die Threads die PNG, pruefen die Groesse,
// berechnen die Maske fuer die Pruefung gegen das Bild im Spiel und bereiten die Farben vor (SB_PrepareHdSpriteSurface).
// Der Hauptthread legt nur noch die Texturen an, in kleinen Portionen je Frame. Bis ein Bild bereit ist, bleibt es 1x.
// Gleiche Dateien teilen sich eine Textur.
//--------------------------------------------------------------------------------------------
static const long long HdVideoQueuedMax = 256LL * 1024 * 1024; // vorbereitet, aber noch nicht hochgeladen (alle Figuren)
static const double HdVideoUploadMs = 3.0;                     // Hochladen je 16 ms hoechstens etwa so lange (alle Figuren)

static double HdNowMs() { return double(SDL_GetPerformanceCounter()) * 1000.0 / double(SDL_GetPerformanceFrequency()); }

static std::string HdClipName(const CString &filename) {
    std::string n = fs::path((LPCTSTR)filename).filename().string();
    for (auto &ch : n) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return n;
}

namespace {
std::atomic<long long> gHdVideoQueuedBytes{0};

struct HdJobSet;

// Ein HD-Bild (eine Datei). Die Ergebnisfelder schreibt ein Hintergrund-Thread, gelesen werden sie erst nach Done.
struct HdJob {
    fs::path File;
    SDL_Rect Rect{}; // Bereich im 1x-Bild (crop.csv oder das ganze Bild)
    std::shared_ptr<HdJobSet> Set;
    std::atomic<bool> Wanted{false}; // wird gerade gezeigt: zuerst laden
    std::atomic<bool> Done{false};
    SLONG ClipIndex{0};         // H27: Clip in HdJobSet::Clips (sichtbarer Clip zuerst)
    SDL_Surface *Argb{nullptr}; // vorbereitet, bis zum Hochladen
    long long Bytes{0};
    std::vector<bool> Mask;       // deckend je 1x-Pixel in Rect (Alpha in der Mitte des k x k-Blocks)
    SLONG K{0};                   // Texturpixel je 1x-Pixel
    std::shared_ptr<HdJob> DupOf; // gleiche Datei (und Groesse) schon in dieser Figur
    std::string Error;
    double DecodeMs{0};
    ~HdJob() {
        if (Argb != nullptr) {
            gHdVideoQueuedBytes -= Bytes;
            SDL_FreeSurface(Argb);
        }
    }
};

// Ein Clip der Figur. Name und Pfade setzt der Hauptthread, den Rest der Scan im Hintergrund.
struct HdScanClip {
    std::string Name;
    fs::path Dir;      // hd/video/<clip>.smk
    fs::path Smk, Flc; // Clip selbst, fuer die Groesse ohne crop.csv
    bool Found{false};
    SDL_Rect Crop{}; // Bereich im 1x-Bild
    bool Cropped{false};
    std::map<SLONG, std::shared_ptr<HdJob>> Frames;
};

// Alle Bilder einer Figur
struct HdJobSet {
    std::atomic<bool> Alive{true};    // false: Figur weg, nichts mehr laden
    std::atomic<bool> Scanned{false}; // Ergebnis des Scans gueltig
    std::atomic<int> HotClip{-1};     // H27: Clip, der gerade gezeigt wird; seine Bilder zuerst laden
    long long CapBytes{0};
    std::vector<HdScanClip> Clips;
    // Ergebnis des Scans
    SLONG Half{1};         // 2: Bilder auf halbe Groesse (2x statt 4x), damit die Figur unter die Obergrenze passt
    bool All1x{false};     // passt auch in 2x nicht: ganze Figur 1x
    long long Estimate{0}; // Grafikspeicher bei voller Groesse (verschiedene Bilder)
    SLONG Files{0}, Unique{0};
    double ScanMs{0};
    std::vector<std::string> Log; // Meldungen fuer das Log (schreibt der Hauptthread)
};

// Groesse eines Clips (1x) aus dem Dateikopf (smk oder flc), fuer HD-Bilder ohne crop.csv
bool HdClipSize(const fs::path &smk, const fs::path &flc, SLONG &w, SLONG &h) {
    unsigned char head[12]{};
    std::error_code ec;
    const bool isSmk = fs::exists(smk, ec);
    std::ifstream in(isSmk ? smk : flc, std::ios::binary);
    if (!in.read(reinterpret_cast<char *>(head), sizeof(head))) {
        return false;
    }
    if (isSmk && head[0] == 'S' && head[1] == 'M' && head[2] == 'K') {
        w = SLONG(head[4] | head[5] << 8 | head[6] << 16 | head[7] << 24);
        h = SLONG(head[8] | head[9] << 8 | head[10] << 16 | head[11] << 24);
    } else if (!isSmk && head[5] == 0xAF && (head[4] == 0x11 || head[4] == 0x12)) {
        w = SLONG(head[8] | head[9] << 8);
        h = SLONG(head[10] | head[11] << 8);
    } else {
        return false;
    }
    return w > 0 && h > 0;
}

// Breite und Hoehe aus dem PNG-Kopf
bool HdPngSize(const fs::path &file, SLONG &w, SLONG &h) {
    unsigned char head[24]{};
    std::ifstream in(file, std::ios::binary);
    if (!in.read(reinterpret_cast<char *>(head), sizeof(head)) || head[1] != 'P' || head[2] != 'N' || head[3] != 'G' || head[12] != 'I') {
        return false;
    }
    w = SLONG(head[16] << 24 | head[17] << 16 | head[18] << 8 | head[19]);
    h = SLONG(head[20] << 24 | head[21] << 16 | head[22] << 8 | head[23]);
    return w > 0 && h > 0;
}

Uint64 HdFileHash(const fs::path &file) {
    std::ifstream in(file, std::ios::binary);
    std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Uint64 key = 1469598103934665603ULL; // FNV-1a
    for (char c : data) {
        key = (key ^ Uint8(c)) * 1099511628211ULL;
    }
    return key;
}

// Halbe Groesse (2x2-Mittel, Farbe nach Alpha gewichtet)
SDL_Surface *HdHalve(SDL_Surface *rgba) {
    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, rgba->w / 2, rgba->h / 2, 32, SDL_PIXELFORMAT_RGBA32);
    if (out == nullptr) {
        return nullptr;
    }
    SDL_LockSurface(rgba);
    SDL_LockSurface(out);
    for (int y = 0; y < out->h; y++) {
        const auto *r0 = static_cast<const Uint8 *>(rgba->pixels) + (2 * y) * rgba->pitch;
        const auto *r1 = r0 + rgba->pitch;
        auto *o = static_cast<Uint8 *>(out->pixels) + y * out->pitch;
        for (int x = 0; x < out->w; x++) {
            const Uint8 *p[4] = {r0 + 8 * x, r0 + 8 * x + 4, r1 + 8 * x, r1 + 8 * x + 4};
            Uint32 a = 0, c[3] = {0, 0, 0};
            for (const Uint8 *q : p) {
                a += q[3];
                for (int i = 0; i < 3; i++) {
                    c[i] += Uint32(q[i]) * q[3];
                }
            }
            for (int i = 0; i < 3; i++) {
                o[4 * x + i] = a != 0 ? Uint8((c[i] + a / 2) / a) : 0;
            }
            o[4 * x + 3] = Uint8((a + 2) / 4);
        }
    }
    SDL_UnlockSurface(out);
    SDL_UnlockSurface(rgba);
    return out;
}

class HdVideoLoader {
  public:
    static HdVideoLoader &Get() {
        static HdVideoLoader loader;
        return loader;
    }
    void Scan(const std::shared_ptr<HdJobSet> &set) {
        std::lock_guard<std::mutex> l(M);
        StartThreads();
        Scans.push_back(set);
        Cv.notify_all();
    }
    void Wake() { Cv.notify_all(); }
    ~HdVideoLoader() {
        {
            std::lock_guard<std::mutex> l(M);
            Stop = true;
            Queue.clear();
            Scans.clear();
        }
        Cv.notify_all();
        for (auto &t : Threads) {
            t.join();
        }
    }

  private:
    void StartThreads() {
        if (Threads.empty()) {
            IMG_Init(IMG_INIT_PNG); // einmal im Hauptthread, bevor die Threads PNGs dekodieren
            // H27: alle Kerne bis auf zwei (Hauptthread, Treiber), hoechstens 6, mit niedriger Prioritaet
            const unsigned hw = std::thread::hardware_concurrency();
            const unsigned n = std::max(1U, std::min(6U, hw > 2 ? hw - 2 : 1U));
            for (unsigned i = 0; i < n; i++) {
                Threads.emplace_back([this] {
                    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
                    Run();
                });
            }
            NThreads = n;
            AT_Log_I("HD", "HD-Video: %u Hintergrund-Thread(s) zum Laden", n);
        }
    }

    void Run() {
        for (;;) {
            std::shared_ptr<HdJobSet> scan;
            std::shared_ptr<HdJob> job;
            {
                std::unique_lock<std::mutex> l(M);
                for (;;) {
                    if (Stop) {
                        return;
                    }
                    if (!Scans.empty()) {
                        scan = Scans.front();
                        Scans.pop_front();
                        break;
                    }
                    Queue.erase(std::remove_if(Queue.begin(), Queue.end(), [](const std::shared_ptr<HdJob> &j) { return !j->Set->Alive; }), Queue.end());
                    auto it = std::find_if(Queue.begin(), Queue.end(), [](const std::shared_ptr<HdJob> &j) { return j->Wanted.load(); });
                    if (it == Queue.end() && !Queue.empty() && gHdVideoQueuedBytes < HdVideoQueuedMax) {
                        // H27: zuerst die Bilder des Clips, der gerade gezeigt wird, dann der Rest in Abspielreihenfolge
                        it = std::find_if(Queue.begin(), Queue.end(), [](const std::shared_ptr<HdJob> &j) { return j->ClipIndex == j->Set->HotClip.load(); });
                        if (it == Queue.end()) {
                            it = Queue.begin();
                        }
                    }
                    if (it != Queue.end()) {
                        job = *it;
                        Queue.erase(it);
                        break;
                    }
                    Cv.wait_for(l, std::chrono::milliseconds(50)); // Speicher wird beim Hochladen frei, ohne Signal
                }
            }
            if (scan) {
                std::vector<std::shared_ptr<HdJob>> jobs;
                if (scan->Alive) {
                    DoScan(*scan, scan, jobs);
                }
                {
                    std::lock_guard<std::mutex> l(M);
                    Queue.insert(Queue.end(), jobs.begin(), jobs.end());
                }
                scan->Scanned = true;
                Cv.notify_all();
                continue;
            }
            Load(*job);
            job->Done = true;
        }
    }

    // Alle HD-Bilder der Figur zusammensuchen, Speicher schaetzen, 4x/2x/1x entscheiden, Auftraege anlegen
    static void DoScan(HdJobSet &set, const std::shared_ptr<HdJobSet> &self, std::vector<std::shared_ptr<HdJob>> &jobs) {
        const double t0 = HdNowMs();
        struct Entry {
            std::shared_ptr<HdJob> Job;
            uintmax_t Size{0};
            SLONG W{0}, H{0};
        };
        std::vector<Entry> all;
        for (auto &c : set.Clips) {
            std::error_code ec;
            if (!fs::is_directory(c.Dir, ec)) {
                continue;
            }
            FILE *f = fopen((c.Dir / "crop.csv").string().c_str(), "r");
            if (f != nullptr) {
                int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                if (fscanf(f, "%d;%d;%d;%d", &x0, &y0, &x1, &y1) == 4 && x1 > x0 && y1 > y0 && x0 >= 0 && y0 >= 0) {
                    c.Crop = SDL_Rect{x0, y0, x1 - x0, y1 - y0};
                    c.Cropped = true;
                }
                fclose(f);
            }
            if (!c.Cropped) {
                SLONG w = 0, h = 0;
                if (!HdClipSize(c.Smk, c.Flc, w, h)) {
                    set.Log.push_back(c.Dir.string() + ": Groesse des Clips unbekannt, bleibt 1x");
                    continue;
                }
                c.Crop = SDL_Rect{0, 0, w, h};
            }
            c.Found = true;
            for (const auto &e : fs::directory_iterator(c.Dir, ec)) {
                const std::string file = e.path().filename().string();
                if (file.size() != 8 || file.compare(4, 4, ".png") != 0 ||
                    !std::all_of(file.begin(), file.begin() + 4, [](char ch) { return ch >= '0' && ch <= '9'; })) {
                    continue;
                }
                auto job = std::make_shared<HdJob>();
                job->File = e.path();
                job->Rect = c.Crop;
                job->Set = self;
                job->ClipIndex = SLONG(&c - set.Clips.data());
                c.Frames[SLONG(std::stol(file.substr(0, 4)))] = job;
            }
            for (auto &fr : c.Frames) { // in Abspielreihenfolge
                Entry en;
                en.Job = fr.second;
                en.Size = fs::file_size(en.Job->File, ec);
                HdPngSize(en.Job->File, en.W, en.H);
                all.push_back(en);
            }
        }
        // Gleiche Dateien: gleiche Dateigroesse und Bildgroesse, dann gleicher Inhalt
        std::map<std::tuple<uintmax_t, SLONG, SLONG, int, int>, std::vector<Entry *>> groups;
        for (auto &en : all) {
            groups[std::make_tuple(en.Size, en.W, en.H, en.Job->Rect.w, en.Job->Rect.h)].push_back(&en);
        }
        std::map<const HdJob *, bool> dup;
        for (auto &g : groups) {
            if (g.second.size() < 2) {
                continue;
            }
            std::map<Uint64, std::shared_ptr<HdJob>> byHash;
            for (Entry *en : g.second) {
                auto &first = byHash[HdFileHash(en->Job->File)];
                if (first) {
                    en->Job->DupOf = first;
                    dup[en->Job.get()] = true;
                } else {
                    first = en->Job;
                }
            }
        }
        bool canHalve = true;
        for (auto &en : all) {
            set.Files++;
            if (dup.count(en.Job.get()) == 0) {
                set.Unique++;
                set.Estimate += 4LL * en.W * en.H;
                canHalve = canHalve && en.Job->Rect.w > 0 && en.W / en.Job->Rect.w >= 4;
            }
        }
        if (set.Estimate > set.CapBytes) {
            if (canHalve && set.Estimate / 4 <= set.CapBytes) {
                set.Half = 2;
            } else {
                set.All1x = true;
            }
        }
        for (auto &en : all) {
            if (set.All1x || en.Job->DupOf) {
                en.Job->Done = true; // gleiche Datei: Textur vom ersten Bild
            } else {
                jobs.push_back(en.Job);
            }
        }
        set.ScanMs = HdNowMs() - t0;
    }

    static void Load(HdJob &job) {
        const double t0 = HdNowMs();
        std::ifstream in(job.File, std::ios::binary);
        std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (data.empty()) {
            job.Error = "nicht lesbar";
            return;
        }
        SDL_Surface *png = IMG_Load_RW(SDL_RWFromConstMem(data.data(), int(data.size())), 1);
        SDL_Surface *rgba = png != nullptr ? SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_RGBA32, 0) : nullptr;
        SDL_FreeSurface(png);
        if (rgba == nullptr) {
            job.Error = "nicht lesbar";
            return;
        }
        const SDL_Rect &r = job.Rect;
        const int k = r.w > 0 ? rgba->w / r.w : 0;
        if (k < 2 || rgba->w != r.w * k || rgba->h != r.h * k) {
            char msg[128];
            snprintf(msg, sizeof(msg), "hat %dx%d Pixel, erwartet ein Vielfaches von %dx%d", rgba->w, rgba->h, r.w, r.h);
            job.Error = msg;
            SDL_FreeSurface(rgba);
            return;
        }
        job.K = k;
        job.Mask.resize(size_t(r.w) * r.h);
        SDL_LockSurface(rgba);
        for (int y = 0; y < r.h; y++) {
            const auto *hdRow = static_cast<const Uint8 *>(rgba->pixels) + (y * k + k / 2) * rgba->pitch;
            for (int x = 0; x < r.w; x++) {
                job.Mask[size_t(y) * r.w + x] = hdRow[(x * k + k / 2) * 4 + 3] >= 128;
            }
        }
        SDL_UnlockSurface(rgba);
        if (job.Set->Half == 2 && k >= 4 && k % 2 == 0) {
            SDL_Surface *half = HdHalve(rgba);
            SDL_FreeSurface(rgba);
            rgba = half;
            job.K = k / 2;
            if (rgba == nullptr) {
                job.Error = "nicht verkleinert";
                return;
            }
        }
        SDL_Surface *argb = SB_PrepareHdSpriteSurface(rgba);
        SDL_FreeSurface(rgba);
        if (argb == nullptr) {
            job.Error = "nicht umwandelbar";
            return;
        }
        job.Bytes = 4LL * argb->w * argb->h;
        gHdVideoQueuedBytes += job.Bytes;
        job.Argb = argb;
        job.DecodeMs = HdNowMs() - t0;
    }

    std::mutex M;
    std::condition_variable Cv;
    std::deque<std::shared_ptr<HdJobSet>> Scans;
    std::deque<std::shared_ptr<HdJob>> Queue;
    std::vector<std::thread> Threads;

  public:
    unsigned NThreads{0};

  private:
    bool Stop{false};
};

// Hochladen: Zeitbudget fuer alle Figuren zusammen
double gHdUploadWindow = 0, gHdUploadSpent = 0, gHdUploadAvg = 0; // Avg: gleitendes Mittel je Schritt
} // namespace

struct CSmackerPerson::HdVideoState {
    struct Frame {
        std::shared_ptr<HdJob> Job;
        std::shared_ptr<SDL_Texture> Tex;
        SLONG Scale{0};
        SLONG State{0}; // 0 laedt, 1 Textur da, 2 gegen das Bild im Spiel geprueft, -1 bleibt 1x
        int Rows{0};    // schon hochgeladene Zeilen (Tex angelegt, State 0)
    };
    struct Clip {
        SDL_Rect Crop{}; // Bereich im 1x-Bild
        bool Cropped{false};
        SLONG Index{0}; // in Set->Clips (H27)
        std::map<SLONG, Frame> Frames;
    };
    std::map<std::string, Clip> Clips; // erst nach dem Scan
    bool Installed{false};
    std::vector<Frame *> Pending; // noch ohne Textur
    std::map<const HdJob *, Frame *> ByJob;
    std::shared_ptr<HdJobSet> Set = std::make_shared<HdJobSet>();
    std::string Names;
    long long Bytes{0};
    SLONG Files{0}, Textures{0}, Shared{0}, Failed{0};
    double Start{0}, Ready{0}; // Ready: Zeitpunkt, ab dem alle Bilder bereit waren
    bool AllReady{false};
    double UploadSum{0}, UploadMax{0}, DecodeSum{0}, DecodeMax{0}, MainMax{0}, BeginMs{0};
    std::string Key;       // H27: Clips und Obergrenze, fuer den Cache ueber Besuche
    bool FromCache{false}; // dieser Besuch kam aus dem Cache

    // H27: alle Figuren, die gerade Bilder laden koennen (Hochladen auch zwischen zwei Frames, CSmackerPerson::HdIdle)
    static std::vector<HdVideoState *> &Live() {
        static std::vector<HdVideoState *> live;
        return live;
    }
    HdVideoState() { Live().push_back(this); }
    ~HdVideoState() {
        Set->Alive = false;
        auto &live = Live();
        live.erase(std::remove(live.begin(), live.end(), this), live.end());
    }
    HdVideoState(const HdVideoState &) = delete;
    HdVideoState &operator=(const HdVideoState &) = delete;

    // Beim Verlassen des Raums (Figur verschwindet)
    void LogLeave() const {
        if (Files > 0) {
            AT_Log_I("HD",
                     "HD-Video %s: %d Bilder, %d Texturen (%d geteilt, %d 1x), %s, %.0f MB%s; Laden im Hintergrund Summe %.0f ms, max %.1f ms; "
                     "Hochladen im Hauptthread Summe %.0f ms, je Schritt max %.1f ms; Hauptthread je Zeichnen max %.1f ms (Start %.1f ms); %s",
                     Names.c_str(), Files, Textures, Shared, Failed, Mode(), double(Bytes) / (1024 * 1024), FromCache ? " aus dem Cache" : "", DecodeSum,
                     DecodeMax, UploadSum, UploadMax, MainMax, BeginMs, AllReady ? "vollstaendig geladen" : "beim Verlassen noch nicht vollstaendig geladen");
        }
    }

    // Erneut betreten, Texturen aus dem Cache: nur die Zahlen dieses Besuchs zuruecksetzen
    void Revisit() {
        FromCache = true;
        Start = HdNowMs();
        UploadSum = UploadMax = DecodeSum = DecodeMax = MainMax = BeginMs = 0;
        AT_Log_I("HD", "HD-Video %s: aus dem Cache, sofort bereit (%d Bilder, %d Texturen, %s, %.0f MB)", Names.c_str(), Files, Textures, Mode(),
                 double(Bytes) / (1024 * 1024));
    }

    const char *Mode() const { return Set->All1x ? "1x" : Set->Half == 2 ? "2x" : "4x"; }

    void Fail(Frame &f, const char *why) {
        f.State = -1;
        f.Tex.reset();
        Failed++;
        if (why != nullptr && f.Job) {
            AT_Log_I("HD", "%s %s, bleibt 1x", f.Job->File.string().c_str(), why);
        }
    }

    // Alle Clips der Figur (Name, Dateiname); den Rest macht der Scan im Hintergrund
    void Begin(const std::vector<std::pair<std::string, CString>> &clips, long long capBytes) {
        Start = HdNowMs();
        Set->CapBytes = capBytes;
        const fs::path root = fs::path((LPCTSTR)AppPath) / "hd" / "video";
        std::error_code ec;
        if (!fs::is_directory(root, ec)) {
            Installed = true; // keine HD-Videos
            return;
        }
        for (const auto &clip : clips) {
            if (std::any_of(Set->Clips.begin(), Set->Clips.end(), [&](const HdScanClip &c) { return c.Name == clip.first; })) {
                continue;
            }
            HdScanClip c;
            c.Name = clip.first;
            c.Dir = root / clip.first;
            fs::path path{clip.second.c_str()};
            fs::path rootPath{SmackerPath.c_str()};
            c.Smk = FullFilesystemPath(path, rootPath); // wie CSmackerClip::Start, aus dem Cache (nicht threadsicher)
            c.Flc = FullFilesystemPath(path.replace_extension("flc"), rootPath);
            Set->Clips.push_back(c);
        }
        HdVideoLoader::Get().Scan(Set);
    }

    // Ergebnis des Scans uebernehmen
    void Install() {
        Installed = true;
        for (const auto &line : Set->Log) {
            AT_Log_I("HD", "%s", line.c_str());
        }
        SLONG nClips = 0, nCropped = 0;
        for (auto &sc : Set->Clips) {
            if (!sc.Found) {
                continue;
            }
            nClips++;
            nCropped += sc.Cropped ? 1 : 0;
            Names += (Names.empty() ? "" : ", ") + sc.Name.substr(0, sc.Name.rfind('.'));
            Clip &c = Clips[sc.Name];
            c.Crop = sc.Crop;
            c.Cropped = sc.Cropped;
            c.Index = SLONG(&sc - Set->Clips.data());
            for (auto &fr : sc.Frames) {
                Frame &f = c.Frames[fr.first];
                f.Job = fr.second;
                ByJob[f.Job.get()] = &f;
                Files++;
                if (Set->All1x) {
                    f.State = -1;
                } else {
                    Pending.push_back(&f);
                }
            }
        }
        if (Files == 0) {
            return;
        }
        AllReady = Set->All1x;
        AT_Log_I("HD",
                 "HD-Video %s: %d Bilder (%d verschieden) aus %d Clips (%d zugeschnitten), Grafikspeicher %.0f MB bei voller Groesse, "
                 "Obergrenze %lld MB: %s; Suche im Hintergrund %.0f ms",
                 Names.c_str(), Files, Set->Unique, nClips, nCropped, double(Set->Estimate) / (1024 * 1024), Set->CapBytes / (1024 * 1024),
                 Set->All1x       ? "ganze Figur 1x"
                 : Set->Half == 2 ? "ganze Figur in 2x"
                                  : "4x",
                 Set->ScanMs);
    }

    // Fertig vorbereitete Bilder hochladen, gezeigte zuerst, im Zeitbudget
    void Pump() {
        if (!Installed) {
            if (!Set->Scanned) {
                return;
            }
            Install();
        }
        if (Pending.empty()) {
            return;
        }
        const double now = HdNowMs();
        if (now - gHdUploadWindow > 16.0) {
            gHdUploadWindow = now;
            gHdUploadSpent = 0;
        }
        // H27: zuerst angefangene Texturen fertig hochladen (nicht viele halb), dann gezeigte Bilder, dann der Rest
        for (int pass = 0; pass < 3; pass++) {
            for (auto &fp : Pending) {
                if (fp == nullptr) {
                    continue;
                }
                Frame &f = *fp;
                HdJob &j = *f.Job;
                if (!j.Done || (pass == 0 && !f.Tex) || (pass == 1 && !j.Wanted)) {
                    continue;
                }
                if (j.DupOf) {
                    auto it = ByJob.find(j.DupOf.get());
                    if (it == ByJob.end() || it->second->State == 0) {
                        continue; // erstes Bild noch nicht da
                    }
                    if (it->second->Tex) {
                        f.Tex = it->second->Tex;
                        f.Scale = it->second->Scale;
                        f.State = 1;
                        Shared++;
                    } else {
                        Fail(f, nullptr);
                    }
                    fp = nullptr;
                    continue;
                }
                if (!j.Error.empty() || j.Argb == nullptr) {
                    Fail(f, j.Error.empty() ? "nicht geladen" : j.Error.c_str());
                    fp = nullptr;
                    continue;
                }
                // In Schritten: Textur anlegen, dann Streifen von etwa 512 KB, solange das Zeitbudget reicht
                bool done = false;
                while (!done && f.State == 0) {
                    if (gHdUploadSpent > 0 && gHdUploadSpent + gHdUploadAvg > HdVideoUploadMs) {
                        break; // im naechsten Frame weiter
                    }
                    const double t0 = HdNowMs();
                    if (!f.Tex) {
                        SDL_Texture *tex = SB_CreateHdSpriteTextureEmpty(j.Argb->w, j.Argb->h);
                        if (tex == nullptr) {
                            Fail(f, "Textur nicht angelegt");
                            break;
                        }
                        f.Tex = std::shared_ptr<SDL_Texture>(tex, SB_ForgetHdTexture); // frei, wenn die Figur (der Raum) verschwindet
                        Bytes += j.Bytes;
                        f.Rows = 0;
                    } else {
                        const int rows = std::min(j.Argb->h - f.Rows, std::max(1, (1 << 19) / j.Argb->pitch));
                        const SDL_Rect stripe{0, f.Rows, j.Argb->w, rows};
                        SDL_UpdateTexture(f.Tex.get(), &stripe, static_cast<const Uint8 *>(j.Argb->pixels) + f.Rows * j.Argb->pitch, j.Argb->pitch);
                        f.Rows += rows;
                        done = f.Rows >= j.Argb->h;
                    }
                    const double ms = HdNowMs() - t0;
                    gHdUploadSpent += ms;
                    gHdUploadAvg = gHdUploadAvg > 0 ? gHdUploadAvg * 0.8 + ms * 0.2 : ms;
                    UploadSum += ms;
                    UploadMax = std::max(UploadMax, ms);
                }
                if (f.State < 0) {
                    fp = nullptr;
                    continue;
                }
                if (!done) {
                    continue;
                }
                SDL_FreeSurface(j.Argb);
                j.Argb = nullptr;
                gHdVideoQueuedBytes -= j.Bytes;
                DecodeSum += j.DecodeMs;
                DecodeMax = std::max(DecodeMax, j.DecodeMs);
                f.Scale = j.K;
                f.State = 1;
                Textures++;
                fp = nullptr;
            }
        }
        Pending.erase(std::remove(Pending.begin(), Pending.end(), nullptr), Pending.end());
        if (Pending.empty() && !AllReady) {
            AllReady = true;
            Ready = HdNowMs();
            AT_Log_I("HD",
                     "HD-Video %s: %d Bilder nach %.1f s bereit (%d Texturen, %s, %.0f MB; Dekodieren Summe %.1f s in %u Threads, Hochladen Summe %.2f s)",
                     Names.c_str(), Files, (Ready - Start) / 1000.0, Textures, Mode(), double(Bytes) / (1024 * 1024), DecodeSum / 1000.0,
                     HdVideoLoader::Get().NThreads, UploadSum / 1000.0);
        }
    }
};

// Das Bild (8 Bit mit Palette, Index 0 durchsichtig) als RGB565 mit Colorkey 0, so wie es in den Raum geblittet wird
// (CalculatePalettemapper macht reines Schwarz zu (4,4,4), 0x0000 kommt also nur als Colorkey vor)
static std::shared_ptr<SDL_Surface> HdFrame565(SDL_Surface *frame, const SDL_Rect &crop) {
    if (frame == nullptr || frame->format->BytesPerPixel != 1 || frame->format->palette == nullptr) {
        return nullptr;
    }
    const SDL_Rect r = crop.w > 0 ? crop : SDL_Rect{0, 0, frame->w, frame->h};
    if (r.x < 0 || r.y < 0 || r.x + r.w > frame->w || r.y + r.h > frame->h) {
        return nullptr;
    }
    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, r.w, r.h, 16, SDL_PIXELFORMAT_RGB565);
    if (out == nullptr) {
        return nullptr;
    }
    Uint16 map[256];
    const SDL_Palette *pal = frame->format->palette;
    for (int i = 0; i < 256; i++) {
        const SDL_Color c = i < pal->ncolors ? pal->colors[i] : SDL_Color{0, 0, 0, 255};
        map[i] = i == 0 ? 0 : Uint16(SDL_MapRGB(out->format, c.r, c.g, c.b));
    }
    SDL_LockSurface(frame);
    SDL_LockSurface(out);
    for (int y = 0; y < r.h; y++) {
        const auto *in = static_cast<const Uint8 *>(frame->pixels) + (r.y + y) * frame->pitch + r.x;
        auto *o = reinterpret_cast<Uint16 *>(static_cast<Uint8 *>(out->pixels) + y * out->pitch);
        for (int x = 0; x < r.w; x++) {
            o[x] = map[in[x]];
        }
    }
    SDL_UnlockSurface(out);
    SDL_UnlockSurface(frame);
    SDL_SetColorKey(out, SDL_TRUE, 0);
    return std::shared_ptr<SDL_Surface>(out, SDL_FreeSurface);
}

//--------------------------------------------------------------------------------------------
// H27: Cache ueber Besuche. Fertig geladene Figuren behalten ihre Texturen nach dem Verlassen des Raums, bis
// OptionHdVideoCacheMB (alle behaltenen Figuren zusammen) erreicht ist; dann fallen die am laengsten nicht gezeigten weg.
//--------------------------------------------------------------------------------------------
static std::list<std::pair<std::string, std::shared_ptr<void>>> &HdCacheList() {
    static std::list<std::pair<std::string, std::shared_ptr<void>>> list; // vorne: zuletzt verlassen
    return list;
}

CSmackerPerson::~CSmackerPerson() {
    if (Hd) {
        Hd->LogLeave();
        HdKeep(Hd);
    }
}

void CSmackerPerson::HdKeep(std::shared_ptr<HdVideoState> &hd) {
    const long long cap = Sim.Options.OptionHdVideoCacheMB * 1024LL * 1024;
    if (hd.use_count() != 1 || !hd->AllReady || hd->Files == 0 || hd->Bytes <= 0 || hd->Bytes > cap) {
        return; // nicht fertig, nichts drin oder zu gross: wird freigegeben
    }
    auto &list = HdCacheList();
    list.emplace_front(hd->Key, hd);
    long long total = 0;
    for (auto it = list.begin(); it != list.end();) {
        const long long bytes = static_cast<HdVideoState *>(it->second.get())->Bytes;
        if (total + bytes > cap) {
            AT_Log_I("HD", "HD-Video %s: aus dem Cache entfernt (%.0f MB)", static_cast<HdVideoState *>(it->second.get())->Names.c_str(),
                     double(bytes) / (1024 * 1024));
            it = list.erase(it);
        } else {
            total += bytes;
            ++it;
        }
    }
    AT_Log_I("HD", "HD-Video %s: im Cache behalten (%.0f MB; Cache %.0f von %lld MB, %d Figuren)", hd->Names.c_str(), double(hd->Bytes) / (1024 * 1024),
             double(total) / (1024 * 1024), cap / (1024 * 1024), SLONG(list.size()));
}

std::shared_ptr<CSmackerPerson::HdVideoState> CSmackerPerson::HdTake(const std::string &key) {
    auto &list = HdCacheList();
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (it->first == key) {
            auto hd = std::static_pointer_cast<HdVideoState>(it->second);
            list.erase(it);
            return hd;
        }
    }
    return nullptr;
}

void CSmackerPerson::HdIdle() {
    if (Sim.Options.OptionHdVideo == 0 || SB_GetRenderScale() <= 1) {
        return;
    }
    const auto live = HdVideoState::Live(); // Kopie: Pump legt keine Figuren an oder weg
    for (HdVideoState *hd : live) {
        if (!hd->AllReady) {
            hd->Pump();
        }
    }
}

bool CSmackerPerson::GetHdFrame(SDL_Texture *&tex, SLONG &scale, SDL_Rect &crop) {
    if (Sim.Options.OptionHdVideo == 0 || HdBitmapClip < 0 || HdBitmapClip >= Clips.AnzEntries() || HdBitmapFrame < 0) {
        return false;
    }
    const double t0 = HdNowMs();
    if (!Hd) {
        std::vector<std::pair<std::string, CString>> clips;
        const long long cap = Sim.Options.OptionHdVideoMaxMB * 1024LL * 1024;
        std::string key = std::to_string(cap);
        for (SLONG i = 0; i < Clips.AnzEntries(); i++) {
            if (Clips[i].Filename.GetLength() > 0) {
                clips.emplace_back(HdClipName(Clips[i].Filename), Clips[i].Filename);
                key += "|" + clips.back().first;
            }
        }
        Hd = HdTake(key); // H27: schon einmal fertig geladen?
        if (Hd) {
            Hd->Revisit();
        } else {
            Hd = std::make_shared<HdVideoState>();
            Hd->Key = key;
            Hd->Begin(clips, cap);
        }
        Hd->BeginMs = HdNowMs() - t0;
    }
    HdVideoState &hd = *Hd;
    HdVideoState::Frame *f = nullptr;
    const HdVideoState::Clip *c = nullptr;
    auto ci = hd.Clips.find(HdClipName(Clips[HdBitmapClip].Filename));
    if (ci != hd.Clips.end()) {
        c = &ci->second;
        hd.Set->HotClip = ci->second.Index; // H27: Bilder dieses Clips zuerst laden
        auto fi = ci->second.Frames.find(HdBitmapFrame);
        if (fi != ci->second.Frames.end()) {
            f = &fi->second;
            if (f->State == 0 && !f->Job->Wanted.exchange(true)) {
                HdVideoLoader::Get().Wake(); // dieses Bild zuerst
            }
        }
    }
    hd.Pump();
    bool ok = false;
    if (f != nullptr && f->State == 1) {
        // Passt die HD-Datei zu diesem Bild? Deckend in 1x <-> Alpha in der Mitte des k x k-Blocks (Rand toleriert)
        const SDL_Rect &r = c->Crop;
        const HdJob &j = f->Job->DupOf ? *f->Job->DupOf : *f->Job;
        SDL_Surface *frame = Bitmap.pBitmap->GetSurface();
        if (frame == nullptr || frame->format->BytesPerPixel != 1 || r.x + r.w > frame->w || r.y + r.h > frame->h ||
            (!c->Cropped && (r.w != frame->w || r.h != frame->h))) {
            hd.Fail(*f, "passt nicht zur Groesse des Bildes im Spiel");
        } else {
            long differ = 0;
            SDL_LockSurface(frame);
            for (int y = 0; y < r.h; y++) {
                const auto *in = static_cast<const Uint8 *>(frame->pixels) + (r.y + y) * frame->pitch + r.x;
                for (int x = 0; x < r.w; x++) {
                    differ += (in[x] != 0) != j.Mask[size_t(y) * r.w + x] ? 1 : 0;
                }
            }
            SDL_UnlockSurface(frame);
            if (differ * 50 > long(r.w) * r.h) {
                char why[96];
                snprintf(why, sizeof(why), "passt nicht zum Bild im Spiel (%ld von %d Pixeln)", differ, r.w * r.h);
                hd.Fail(*f, why);
            } else {
                f->State = 2;
            }
        }
    }
    if (f != nullptr && f->State == 2) {
        tex = f->Tex.get();
        scale = f->Scale;
        crop = c->Cropped ? c->Crop : SDL_Rect{0, 0, 0, 0};
        ok = true;
    }
    hd.MainMax = std::max(hd.MainMax, HdNowMs() - t0);
    return ok;
}

//--------------------------------------------------------------------------------------------
// Blittet eine Person in eine Bitmap:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::BlitAtT(SBBM &RoomBm) { BlitAtT(RoomBm, XY(0, 0)); }
