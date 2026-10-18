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
#include <fstream>
#include <mutex>
#include <thread>
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
CSmackerPerson::~CSmackerPerson() = default;

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
    HdBitmapClip = ActiveClip; // H25c: welches Bild steht jetzt in Bitmap
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
// H25d: Laden im Hintergrund. Beim ersten Zeichnen einer Figur werden alle HD-Bilder ihrer Clips in eine Warteschlange
// gestellt; Hintergrund-Threads lesen die PNG, pruefen die Groesse, berechnen die Maske fuer die Pruefung gegen das Bild
// im Spiel und bereiten die Farben vor (SB_PrepareHdSpriteSurface). Der Hauptthread legt nur noch die Texturen an, in
// kleinen Portionen je Frame. Bis ein Bild bereit ist, bleibt es 1x. Gleiche Dateien teilen sich eine Textur.
//--------------------------------------------------------------------------------------------
static const long long HdVideoMaxBytes = 512LL * 1024 * 1024;  // Obergrenze je Figur; darueber bleiben weitere Bilder 1x
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
    SDL_Surface *Argb{nullptr}; // vorbereitet, bis zum Hochladen
    long long Bytes{0};
    std::vector<bool> Mask; // deckend je 1x-Pixel in Rect (Alpha in der Mitte des k x k-Blocks)
    SLONG K{0};
    std::shared_ptr<HdJob> DupOf; // gleiche Datei (und Groesse) schon in diesem Satz
    std::string Error;
    double DecodeMs{0};
    ~HdJob() {
        if (Argb != nullptr) {
            gHdVideoQueuedBytes -= Bytes;
            SDL_FreeSurface(Argb);
        }
    }
};

// Alle Bilder einer Figur
struct HdJobSet {
    std::atomic<bool> Alive{true}; // false: Figur weg oder Obergrenze erreicht, nichts mehr laden
    std::mutex M;
    std::map<Uint64, std::weak_ptr<HdJob>> ByKey;
};

class HdVideoLoader {
  public:
    static HdVideoLoader &Get() {
        static HdVideoLoader loader;
        return loader;
    }
    void Submit(const std::vector<std::shared_ptr<HdJob>> &jobs) {
        std::lock_guard<std::mutex> l(M);
        if (Threads.empty()) {
            IMG_Init(IMG_INIT_PNG); // einmal im Hauptthread, bevor die Threads PNGs dekodieren
            const unsigned n = std::max(1U, std::min(3U, std::thread::hardware_concurrency() / 2));
            for (unsigned i = 0; i < n; i++) {
                Threads.emplace_back([this] { Run(); });
            }
            AT_Log_I("HD", "HD-Video: %u Hintergrund-Thread(s) zum Laden", n);
        }
        Queue.insert(Queue.end(), jobs.begin(), jobs.end());
        Cv.notify_all();
    }
    void Wake() { Cv.notify_all(); }
    ~HdVideoLoader() {
        {
            std::lock_guard<std::mutex> l(M);
            Stop = true;
            Queue.clear();
        }
        Cv.notify_all();
        for (auto &t : Threads) {
            t.join();
        }
    }

  private:
    void Run() {
        for (;;) {
            std::shared_ptr<HdJob> job;
            {
                std::unique_lock<std::mutex> l(M);
                for (;;) {
                    if (Stop) {
                        return;
                    }
                    Queue.erase(std::remove_if(Queue.begin(), Queue.end(), [](const std::shared_ptr<HdJob> &j) { return !j->Set->Alive; }), Queue.end());
                    auto it = std::find_if(Queue.begin(), Queue.end(), [](const std::shared_ptr<HdJob> &j) { return j->Wanted.load(); });
                    if (it == Queue.end() && !Queue.empty() && gHdVideoQueuedBytes < HdVideoQueuedMax) {
                        it = Queue.begin();
                    }
                    if (it != Queue.end()) {
                        job = *it;
                        Queue.erase(it);
                        break;
                    }
                    Cv.wait_for(l, std::chrono::milliseconds(50)); // Speicher wird beim Hochladen frei, ohne Signal
                }
            }
            Load(job);
            job->Done = true;
        }
    }

    static void Load(const std::shared_ptr<HdJob> &job) {
        const double t0 = HdNowMs();
        std::ifstream in(job->File, std::ios::binary);
        std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (data.empty()) {
            job->Error = "nicht lesbar";
            return;
        }
        Uint64 key = 1469598103934665603ULL; // FNV-1a ueber die Datei, dazu die Groesse des Bereichs
        for (char c : data) {
            key = (key ^ Uint8(c)) * 1099511628211ULL;
        }
        key ^= (Uint64(job->Rect.w) << 40) ^ (Uint64(job->Rect.h) << 20);
        {
            std::lock_guard<std::mutex> l(job->Set->M);
            auto &slot = job->Set->ByKey[key];
            if (auto first = slot.lock()) {
                job->DupOf = first;
                return;
            }
            slot = job;
        }
        SDL_Surface *png = IMG_Load_RW(SDL_RWFromConstMem(data.data(), int(data.size())), 1);
        SDL_Surface *rgba = png != nullptr ? SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_RGBA32, 0) : nullptr;
        SDL_FreeSurface(png);
        if (rgba == nullptr) {
            job->Error = "nicht lesbar";
            return;
        }
        const SDL_Rect &r = job->Rect;
        const int k = r.w > 0 ? rgba->w / r.w : 0;
        if (k < 2 || rgba->w != r.w * k || rgba->h != r.h * k) {
            char msg[128];
            snprintf(msg, sizeof(msg), "hat %dx%d Pixel, erwartet ein Vielfaches von %dx%d", rgba->w, rgba->h, r.w, r.h);
            job->Error = msg;
            SDL_FreeSurface(rgba);
            return;
        }
        job->K = k;
        job->Mask.resize(size_t(r.w) * r.h);
        SDL_LockSurface(rgba);
        for (int y = 0; y < r.h; y++) {
            const auto *hdRow = static_cast<const Uint8 *>(rgba->pixels) + (y * k + k / 2) * rgba->pitch;
            for (int x = 0; x < r.w; x++) {
                job->Mask[size_t(y) * r.w + x] = hdRow[(x * k + k / 2) * 4 + 3] >= 128;
            }
        }
        SDL_UnlockSurface(rgba);
        SDL_Surface *argb = SB_PrepareHdSpriteSurface(rgba);
        SDL_FreeSurface(rgba);
        if (argb == nullptr) {
            job->Error = "nicht umwandelbar";
            return;
        }
        job->Bytes = 4LL * argb->w * argb->h;
        gHdVideoQueuedBytes += job->Bytes;
        job->Argb = argb;
        job->DecodeMs = HdNowMs() - t0;
    }

    std::mutex M;
    std::condition_variable Cv;
    std::deque<std::shared_ptr<HdJob>> Queue;
    std::vector<std::thread> Threads;
    bool Stop{false};
};

// Groesse eines Clips (1x) aus dem Dateikopf, fuer HD-Bilder ohne crop.csv
bool HdClipSize(const CString &filename, SLONG &w, SLONG &h) {
    fs::path path{filename.c_str()};
    fs::path rootPath{SmackerPath.c_str()};
    const auto smk = FullFilesystemPath(path, rootPath);
    const auto flc = FullFilesystemPath(path.replace_extension("flc"), rootPath);
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
        std::string Dir;
        SDL_Rect Crop{}; // Bereich im 1x-Bild
        bool Cropped{false};
        std::map<SLONG, Frame> Frames;
    };
    std::map<std::string, Clip> Clips;
    std::vector<Frame *> Pending; // noch ohne Textur
    std::map<const HdJob *, Frame *> ByJob;
    std::shared_ptr<HdJobSet> Set = std::make_shared<HdJobSet>();
    std::string Names;
    long long Bytes{0};
    bool Full{false};
    SLONG Files{0}, Textures{0}, Shared{0}, Failed{0};
    double Start{0}, Ready{0}; // Ready: Zeitpunkt, ab dem alle Bilder bereit waren (0 = noch nicht)
    bool AllReady{false};
    double UploadSum{0}, UploadMax{0}, DecodeSum{0}, DecodeMax{0}, MainMax{0}, BeginMs{0};

    ~HdVideoState() {
        Set->Alive = false;
        if (Files > 0) {
            AT_Log_I("HD",
                     "HD-Video %s: %d Bilder, %d Texturen (%d geteilt, %d 1x), %.0f MB; Laden im Hintergrund Summe %.0f ms, max %.1f ms; "
                     "Hochladen im Hauptthread Summe %.0f ms, je Schritt max %.1f ms; Hauptthread je Zeichnen max %.1f ms (Start %.1f ms); %s",
                     Names.c_str(), Files, Textures, Shared, Failed, double(Bytes) / (1024 * 1024), DecodeSum, DecodeMax, UploadSum, UploadMax, MainMax,
                     BeginMs, AllReady ? "vollstaendig geladen" : "beim Verlassen noch nicht vollstaendig geladen");
        }
    }

    void Fail(Frame &f, const char *why) {
        f.State = -1;
        f.Tex.reset();
        Failed++;
        if (why != nullptr && f.Job) {
            AT_Log_I("HD", "%s %s, bleibt 1x", f.Job->File.string().c_str(), why);
        }
    }

    // Fertig vorbereitete Bilder hochladen, gezeigte zuerst, im Zeitbudget
    void Pump() {
        if (Pending.empty()) {
            return;
        }
        const double now = HdNowMs();
        if (now - gHdUploadWindow > 16.0) {
            gHdUploadWindow = now;
            gHdUploadSpent = 0;
        }
        for (int pass = 0; pass < 2; pass++) {
            for (auto &fp : Pending) {
                if (fp == nullptr) {
                    continue;
                }
                Frame &f = *fp;
                HdJob &j = *f.Job;
                if (!j.Done || (pass == 0 && !j.Wanted)) {
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
                    Fail(f, Full ? nullptr : (j.Error.empty() ? "nicht geladen" : j.Error.c_str()));
                    fp = nullptr;
                    continue;
                }
                if (!f.Tex && Bytes + j.Bytes > HdVideoMaxBytes) {
                    if (!Full) {
                        Full = true;
                        Set->Alive = false;
                        AT_Log_I("HD", "HD-Video %s: Obergrenze %lld MB fuer diese Figur erreicht, weitere Bilder bleiben 1x", Names.c_str(),
                                 HdVideoMaxBytes / (1024 * 1024));
                    }
                    Fail(f, nullptr);
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
        if (Full) {
            for (auto *fp : Pending) {
                const HdJob &j = *fp->Job;
                auto it = j.Done && j.DupOf ? ByJob.find(j.DupOf.get()) : ByJob.end();
                if (it != ByJob.end() && it->second->State > 0 && it->second->Tex) {
                    fp->Tex = it->second->Tex; // gleiches Bild, Textur schon da
                    fp->Scale = it->second->Scale;
                    fp->State = 1;
                    Shared++;
                } else {
                    Fail(*fp, nullptr); // wird nicht mehr geladen
                }
            }
            Pending.clear();
        }
        if (Pending.empty() && !AllReady && Files > 0) {
            AllReady = true;
            Ready = HdNowMs();
            AT_Log_I("HD", "HD-Video %s: %d Bilder nach %.1f s bereit (%d Texturen, %.0f MB)", Names.c_str(), Files, (Ready - Start) / 1000.0, Textures,
                     double(Bytes) / (1024 * 1024));
        }
    }

    // Alle HD-Bilder aller Clips der Figur in die Warteschlange (Name des Clips, Dateiname)
    void Begin(const std::vector<std::pair<std::string, CString>> &clips) {
        HdVideoState &hd = *this;
        hd.Start = HdNowMs();
        std::vector<std::shared_ptr<HdJob>> jobs;
        for (const auto &clip : clips) {
            const std::string &name = clip.first;
            if (name.empty() || hd.Clips.count(name) != 0) {
                continue;
            }
            auto &c = hd.Clips[name];
            const fs::path dir = fs::path((LPCTSTR)AppPath) / "hd" / "video" / name;
            std::error_code ec;
            if (!fs::is_directory(dir, ec)) {
                continue;
            }
            FILE *f = fopen((dir / "crop.csv").string().c_str(), "r");
            if (f != nullptr) {
                int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                if (fscanf(f, "%d;%d;%d;%d", &x0, &y0, &x1, &y1) == 4 && x1 > x0 && y1 > y0 && x0 >= 0 && y0 >= 0) {
                    c.Crop = SDL_Rect{x0, y0, x1 - x0, y1 - y0};
                    c.Cropped = true;
                }
                fclose(f);
            }
            SLONG w = 0, h = 0;
            if (!c.Cropped) {
                if (!HdClipSize(clip.second, w, h)) {
                    AT_Log_I("HD", "HD-Bilder aus %s: Groesse des Clips unbekannt, bleibt 1x", dir.string().c_str());
                    continue;
                }
                c.Crop = SDL_Rect{0, 0, w, h};
            }
            c.Dir = dir.string();
            AT_Log_I("HD", "HD-Bilder aus %s%s", c.Dir.c_str(), c.Cropped ? " (zugeschnitten)" : "");
            hd.Names += (hd.Names.empty() ? "" : ", ") + name.substr(0, name.rfind('.'));
            for (const auto &e : fs::directory_iterator(dir, ec)) {
                const std::string file = e.path().filename().string();
                if (file.size() != 8 || file.compare(4, 4, ".png") != 0 ||
                    !std::all_of(file.begin(), file.begin() + 4, [](char ch) { return ch >= '0' && ch <= '9'; })) {
                    continue;
                }
                auto &fr = c.Frames[SLONG(std::stol(file.substr(0, 4)))];
                fr.Job = std::make_shared<HdJob>();
                fr.Job->File = e.path();
                fr.Job->Rect = c.Crop;
                fr.Job->Set = hd.Set;
                hd.ByJob[fr.Job.get()] = &fr;
                hd.Files++;
            }
            for (auto &fr : c.Frames) {
                jobs.push_back(fr.second.Job); // in Abspielreihenfolge laden
            }
        }
        for (auto &kv : hd.Clips) {
            for (auto &fr : kv.second.Frames) {
                hd.Pending.push_back(&fr.second);
            }
        }
        if (!jobs.empty()) {
            AT_Log_I("HD", "HD-Video %s: %d Bilder werden im Hintergrund geladen", hd.Names.c_str(), hd.Files);
            HdVideoLoader::Get().Submit(jobs);
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

bool CSmackerPerson::GetHdFrame(SDL_Texture *&tex, SLONG &scale, SDL_Rect &crop) {
    if (Sim.Options.OptionHdVideo == 0 || HdBitmapClip < 0 || HdBitmapClip >= Clips.AnzEntries() || HdBitmapFrame < 0) {
        return false;
    }
    const double t0 = HdNowMs();
    if (!Hd) {
        Hd = std::make_shared<HdVideoState>();
        std::vector<std::pair<std::string, CString>> clips;
        for (SLONG i = 0; i < Clips.AnzEntries(); i++) {
            if (Clips[i].Filename.GetLength() > 0) {
                clips.emplace_back(HdClipName(Clips[i].Filename), Clips[i].Filename);
            }
        }
        Hd->Begin(clips);
        Hd->BeginMs = HdNowMs() - t0;
    }
    HdVideoState &hd = *Hd;
    HdVideoState::Frame *f = nullptr;
    const HdVideoState::Clip *c = nullptr;
    auto ci = hd.Clips.find(HdClipName(Clips[HdBitmapClip].Filename));
    if (ci != hd.Clips.end() && !ci->second.Dir.empty()) {
        c = &ci->second;
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
