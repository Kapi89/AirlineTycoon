#include "defines.h"
#include "helper.h"
#include "Proto.h"
#include "sbl.h"

#include <SDL_image.h>
#include <SDL_surface.h>
#include <SDL_timer.h>

#include <algorithm>
#include <string>

#define AT_Log(...) AT_Log_I("Rendering", __VA_ARGS__)

static_assert(sizeof(void *) == 8, "Airline Tycoon muss als 64-Bit-Programm gebaut werden");

static SLONG gRenderScale = 1;
static XY gLogicalSize{640, 480};

void SB_SetRenderScale(SLONG scale) {
    if (scale < 1 || scale > 4) {
        AT_Log("Render-Faktor %d ungueltig, verwende 1", scale);
        scale = 1;
    }
    gRenderScale = scale;
    AT_Log("Render-Faktor s=%d", gRenderScale);
}

SLONG SB_GetRenderScale() { return gRenderScale; }

void SB_SetLogicalSize(XY size) { gLogicalSize = size; }

XY SB_GetLogicalSize() { return gLogicalSize; }

Uint16 get_pixel16(SDL_Surface *surface, SLONG x, SLONG y);

void put_pixel16(SDL_Surface *surface, SLONG x, SLONG y, Uint16 pixel);
// Primaerpuffer mit GPU-Renderer, an den HD-Blits gemeldet werden (Phase 2, H4)
static SB_CPrimaryBitmap *gHdPrimary = nullptr;

void SB_ForgetHdSurface(const SDL_Surface *hd) {
    if (gHdPrimary != nullptr) {
        gHdPrimary->ForgetHdSurface(hd);
    }
}

void SB_RecordHdShade(SB_CBitmapCore *target, SB_CBitmapCore *shade, XY pos, SB_HdShadeReplay replay, const void *ctx) {
    if (gHdPrimary != nullptr && target == gHdPrimary && shade != nullptr && SB_GetRenderScale() > 1) {
        gHdPrimary->RecordHdShade(shade, pos, replay, ctx);
    }
}

void SB_CBitmapCore::RecordHd(SB_CBitmapCore *target, const SDL_Rect &srcRect, SLONG x, SLONG y, bool colorKey) {
    if (HdTexture != nullptr && gHdPrimary != nullptr && target == gHdPrimary) {
        gHdPrimary->RecordHdBlit(this, srcRect, x, y, colorKey);
    }
}

SB_CBitmapMain::SB_CBitmapMain(SDL_Renderer *render) : Renderer(render) {}

SB_CBitmapMain::~SB_CBitmapMain() {
    for (auto &Bitmap : Bitmaps) {
        Bitmap.second.Release();
    }
}

ULONG SB_CBitmapMain::CreateBitmap(SB_CBitmapCore **out, GfxLib *lib, __int64 name, ULONG flags) {
    auto id = UniqueId++;
    auto res = Bitmaps.emplace(std::make_pair(id, id));
    if (!res.second) {
        assert(false);
        return 1;
    }
    auto it = res.first;
    SB_CBitmapCore *core = &(it->second);
    core->IncRef();
    SDL_Surface *surface = lib->GetSurface(name);
    if (surface != nullptr) {
        core->lpDD = Renderer;
        core->lpDDSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGB565, 0);
        if ((flags & CREATE_USECOLORKEY) != 0U) {
            core->SetColorKey(0);
        }
        if (gHdPrimary != nullptr && SB_GetRenderScale() > 1) {
            SDL_Surface *hd = lib->GetHdSurface(name);
            if (hd != nullptr) {
                core->HdTexture = gHdPrimary->GetHdTextureFor(hd, core->lpDDSurface, (flags & CREATE_USECOLORKEY) != 0U);
            }
        }

        core->lpTexture = (Renderer != nullptr) && ((flags & CREATE_VIDMEM) != 0U) ? SDL_CreateTextureFromSurface(Renderer, core->lpDDSurface) : nullptr;
        core->Size.x = core->lpDDSurface->w;
        core->Size.y = core->lpDDSurface->h;
        core->InitClipRect();

        SDL_SetSurfaceRLE(core->lpDDSurface, SDL_TRUE);
    } else {
        core->lpDD = Renderer;
        core->lpDDSurface = nullptr;
        core->lpTexture = nullptr;
        core->Size.x = 0;
        core->Size.y = 0;
    }
    *out = core;
    return 0;
}

ULONG SB_CBitmapMain::CreateBitmap(SB_CBitmapCore **out, SLONG w, SLONG h, ULONG /*unused*/, ULONG flags, ULONG /*unused*/) {
    auto id = UniqueId++;
    auto res = Bitmaps.emplace(std::make_pair(id, id));
    if (!res.second) {
        assert(false);
        return 1;
    }
    auto it = res.first;
    SB_CBitmapCore *core = &(it->second);
    core->IncRef();
    core->lpDD = Renderer;

    int depth, format;
    if ((flags & CREATE_INDEXED) != 0U) {
        depth = 8;
        format = SDL_PIXELFORMAT_INDEX8;
    } else if ((flags & CREATE_USEALPHA) != 0U) {
        depth = 32;
        format = SDL_PIXELFORMAT_RGBA8888;
    } else {
        depth = 16;
        format = SDL_PIXELFORMAT_RGB565;
    }

    core->lpDDSurface = SDL_CreateRGBSurfaceWithFormat(0, w, h, depth, format);

    if ((flags & CREATE_USECOLORKEY) != 0U) {
        core->SetColorKey(0);
    }

    if (Renderer != nullptr && (flags & CREATE_VIDMEM) != 0U) {
        core->lpTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);

        if ((flags & (CREATE_USEALPHA | CREATE_USECOLORKEY)) != 0U) {
            SDL_SetTextureBlendMode(core->lpTexture, SDL_BLENDMODE_BLEND);
        }
    } else {
        core->lpTexture = nullptr;
    }

    core->Size.x = w;
    core->Size.y = h;
    core->InitClipRect();
    // SDL_SetSurfaceRLE(core->lpDDSurface, SDL_TRUE);
    *out = core;
    return 0;
}

ULONG SB_CBitmapMain::ReleaseBitmap(SB_CBitmapCore *core) {
    auto it = Bitmaps.find(core->getId());
    if (it == Bitmaps.end()) {
        assert(false);
        return 1;
    }
    assert(core == &(it->second));
    if (core->DecRef()) {
        core->Release();
        Bitmaps.erase(it);
    }
    return 0;
}

void SB_CBitmapCore::SetColorKey(ULONG key) { SDL_SetColorKey(lpDDSurface, SDL_TRUE, key); }

ULONG SB_CBitmapCore::Line(SLONG x1, SLONG y1, SLONG x2, SLONG y2, SB_Hardwarecolor hwcolor) {
    if (lpTexture != nullptr) {
        int access = 0;
        SDL_QueryTexture(lpTexture, nullptr, &access, nullptr, nullptr);
        if (access == SDL_TEXTUREACCESS_TARGET) {
            if (SDL_SetRenderTarget(lpDD, lpTexture) < 0) {
                return 1;
            }

            dword key = 0;
            auto color = (dword)hwcolor;
            SDL_GetColorKey(lpDDSurface, &key);
            SDL_SetRenderDrawColor(lpDD, (color & 0xFF0000) >> 16, (color & 0xFF00) >> 8, color & 0xFF,
                                   color == key ? SDL_ALPHA_TRANSPARENT : SDL_ALPHA_OPAQUE);
            SDL_RenderDrawLine(lpDD, x1, y1, x2, y2);
            return 0;
        }
    }

    // Bresenham's Line Algorithm
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
    int dx1 = 0;
    int dy1 = 0;
    int px = 0;
    int py = 0;
    int xe = 0;
    int ye = 0;
    dx = x2 - x1;
    dy = y2 - y1;
    dx1 = fabs(dx);
    dy1 = fabs(dy);
    px = 2 * dy1 - dx1;
    py = 2 * dx1 - dy1;
    if (dy1 <= dx1) {
        if (dx >= 0) {
            x = x1;
            y = y1;
            xe = x2;
        } else {
            x = x2;
            y = y2;
            xe = x1;
        }
        SetPixel(x, y, hwcolor);
        while (x < xe) {
            x = x + 1;
            if (px < 0) {
                px = px + 2 * dy1;
            } else {
                if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) {
                    y = y + 1;
                } else {
                    y = y - 1;
                }
                px = px + 2 * (dy1 - dx1);
            }
            SetPixel(x, y, hwcolor);
        }
    } else {
        if (dy >= 0) {
            x = x1;
            y = y1;
            ye = y2;
        } else {
            x = x2;
            y = y2;
            ye = y1;
        }
        SetPixel(x, y, hwcolor);
        while (y < ye) {
            y = y + 1;
            if (py <= 0) {
                py = py + 2 * dx1;
            } else {
                if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) {
                    x = x + 1;
                } else {
                    x = x - 1;
                }
                py = py + 2 * (dx1 - dy1);
            }
            SetPixel(x, y, hwcolor);
        }
    }
    return 0;
}

void SB_CBitmapCore::SetClipRect(const CRect &rect) {
    SDL_Rect clip = {rect.left, rect.top, rect.Width(), rect.Height()};
    SDL_SetClipRect(lpDDSurface, &clip);
}

SB_Hardwarecolor SB_CBitmapCore::GetHardwarecolor(ULONG color) {
#if 0
    SLONG r = GetHighestSetBit(Format.redMask) - GetHighestSetBit(0xFF0000);
    SLONG g = GetHighestSetBit(Format.greenMask) - GetHighestSetBit(0xFF00);
    SLONG b = GetHighestSetBit(Format.blueMask) - GetHighestSetBit(0xFF);

    SLONG result;
    if (r >= 0)
        result = Format.redMask & ((color & 0xFF0000) << r);
    else
        result = Format.redMask & ((color & 0xFF0000) >> -(char)r);
    if (g >= 0)
        result |= Format.greenMask & ((word)(color & 0xFF00) << g);
    else
        result |= Format.greenMask & ((color & 0xFF00) >> -(char)g);
    if (b >= 0)
        result |= Format.blueMask & ((unsigned char)color << b);
    else
        result |= Format.blueMask & ((dword)(unsigned char)color >> -(char)b);
    return (SB_Hardwarecolor)(result);
#else
    char r = (color & 0xFF0000) >> 16;
    char g = (color & 0xFF00) >> 8;
    char b = (color & 0xFF);
    return SB_Hardwarecolor(SDL_MapRGB(lpDDSurface->format, r, g, b));
#endif
}

SB_Hardwarecolor SB_CBitmapCore::GetHardwarecolor(char r, char g, char b) { return SB_Hardwarecolor(SDL_MapRGB(lpDDSurface->format, r, g, b)); }

ULONG SB_CBitmapCore::Clear(SB_Hardwarecolor hwcolor, const RECT *pRect) {
    auto color = (dword)hwcolor;
    if (SDL_MUSTLOCK(lpDDSurface) && SDL_LockSurface(lpDDSurface) < 0) {
        return 1;
    }
    if (lpTexture != nullptr) {
        if (SDL_SetRenderTarget(lpDD, lpTexture) < 0) {
            return 1;
        }

        dword key = 0;
        SDL_GetColorKey(lpDDSurface, &key);
        SDL_SetRenderDrawColor(lpDD, (color & 0xFF0000) >> 16, (color & 0xFF00) >> 8, color & 0xFF, color == key ? SDL_ALPHA_TRANSPARENT : SDL_ALPHA_OPAQUE);
    }

    if (pRect != nullptr) {
        const CRect &rect = *(const CRect *)pRect;
        SDL_Rect dst = {rect.left, rect.top, rect.Width(), rect.Height()};
        if (lpTexture != nullptr) {
            SDL_RenderFillRect(lpDD, &dst);
        }

        const int result = SDL_FillRect(lpDDSurface, &dst, color);

        if (SDL_MUSTLOCK(lpDDSurface)) {
            SDL_UnlockSurface(lpDDSurface);
        }

        return result;
    }

    if (lpTexture != nullptr) {
        SDL_RenderFillRect(lpDD, nullptr);
    }

    const int result = SDL_FillRect(lpDDSurface, nullptr, color);

    if (SDL_MUSTLOCK(lpDDSurface)) {
        SDL_UnlockSurface(lpDDSurface);
    }
    return result;
}

ULONG SB_CBitmapCore::SetPixel(SLONG x, SLONG y, SB_Hardwarecolor hwcolor) {
    if (SDL_MUSTLOCK(lpDDSurface) && SDL_LockSurface(lpDDSurface) < 0) {
        return 1;
    }
    Uint8 bpp = lpDDSurface->format->BytesPerPixel;
    Uint8 *p = static_cast<Uint8 *>(lpDDSurface->pixels) + y * lpDDSurface->pitch + x * bpp;

    if (lpDDSurface->format->format == SDL_PIXELFORMAT_INDEX8) {
        *static_cast<uint8_t *>(p) = (uint8_t)hwcolor;
    } else if (lpDDSurface->format->format == SDL_PIXELFORMAT_RGBA8888) {
        *reinterpret_cast<uint32_t *>(p) = (uint32_t)hwcolor;
    } else if (lpDDSurface->format->format == SDL_PIXELFORMAT_RGB565) {
        *reinterpret_cast<uint16_t *>(p) = (uint16_t)hwcolor;
    }

    if (SDL_MUSTLOCK(lpDDSurface)) {
        SDL_UnlockSurface(lpDDSurface);
    }
    return 0;
}

ULONG SB_CBitmapCore::GetPixel(SLONG x, SLONG y) {
    if (SDL_MUSTLOCK(lpDDSurface) && SDL_LockSurface(lpDDSurface) < 0) {
        return 1;
    }
    Uint8 bpp = lpDDSurface->format->BytesPerPixel;
    Uint8 bits = lpDDSurface->format->BitsPerPixel;
    Uint8 *p = static_cast<Uint8 *>(lpDDSurface->pixels) + y * lpDDSurface->pitch + x * bpp;
    dword result = *reinterpret_cast<Uint32 *>(p);
    if (SDL_MUSTLOCK(lpDDSurface)) {
        SDL_UnlockSurface(lpDDSurface);
    }
    return result & ((1 << bits) - 1);
}

Uint16 get_pixel16(SDL_Surface *surface, SLONG x, SLONG y) {
    // Convert the pixels to 32 bit
    auto *pixels = static_cast<Uint16 *>(surface->pixels);

    // Get the requested pixel
    return pixels[(y * surface->pitch / 2) + x];
}

void put_pixel16(SDL_Surface *surface, SLONG x, SLONG y, Uint16 pixel) {
    // Convert the pixels to 32 bit
    auto *pixels = static_cast<Uint16 *>(surface->pixels);

    // Set the pixel
    pixels[(y * surface->pitch / 2) + x] = pixel;
}

SDL_Surface *SB_CBitmapCore::GetFlippedSurface() {
    if (flippedBufferSurface != nullptr) {
        return flippedBufferSurface;
    }

    flippedBufferSurface =
        SDL_CreateRGBSurfaceWithFormat(lpDDSurface->flags, lpDDSurface->w, lpDDSurface->h, lpDDSurface->format->BitsPerPixel, lpDDSurface->format->format);

    if (SDL_MUSTLOCK(lpDDSurface)) {
        // Lock the surface
        SDL_LockSurface(lpDDSurface);
        SDL_LockSurface(flippedBufferSurface);
    }

    for (SLONG x = 0, rx = lpDDSurface->w - 1; x < lpDDSurface->w; x++, rx--) {
        // Go through rows
        for (SLONG y = 0; y < lpDDSurface->h; y++) {
            Uint16 pixel = get_pixel16(lpDDSurface, x, y);
            put_pixel16(flippedBufferSurface, rx, y, pixel);
        }
    }

    if (SDL_MUSTLOCK(lpDDSurface)) {
        // Lock the surface
        SDL_UnlockSurface(lpDDSurface);
        SDL_UnlockSurface(flippedBufferSurface);
    }

    Uint32 key = 0;
    if (SDL_GetColorKey(lpDDSurface, &key) == 0) {
        SDL_SetColorKey(flippedBufferSurface, 1, key);
    }

    return flippedBufferSurface;
}

ULONG SB_CBitmapCore::Blit(class SB_CBitmapCore *core, SLONG x, SLONG y) {
    if (!lpDDSurface || !core->lpDDSurface) {
        return 0;
    }

    SDL_Rect dst = {x, y, Size.x, Size.y};
    const int rc = SDL_BlitSurface(lpDDSurface, nullptr, core->lpDDSurface, &dst);
    RecordHd(core, SDL_Rect{0, 0, Size.x, Size.y}, x, y, true);
    return rc;
}

ULONG SB_CBitmapCore::Blit(class SB_CBitmapCore *core, SLONG x, SLONG y, const CRect &rect) {
    if (!lpDDSurface || !core->lpDDSurface) {
        return 0;
    }

    SDL_Rect src = {rect.left, rect.top, rect.Width(), rect.Height()};
    SDL_Rect dst = {x, y, rect.Width(), rect.Height()};
    const int rc = SDL_BlitSurface(lpDDSurface, &src, core->lpDDSurface, &dst);
    RecordHd(core, SDL_Rect{rect.left, rect.top, rect.Width(), rect.Height()}, x, y, true);
    return rc;
}

ULONG SB_CBitmapCore::BlitFast(class SB_CBitmapCore *core, SLONG x, SLONG y) {
    if (!lpDDSurface || !core->lpDDSurface) {
        return 0;
    }

    // Ignore source color key
    Uint32 key = 0;
    int result = SDL_GetColorKey(lpDDSurface, &key);
    if (result != -1) {
        SDL_SetColorKey(lpDDSurface, SDL_FALSE, key);
    }

    SDL_Rect dst = {x, y, Size.x, Size.y};
    SDL_BlitSurface(lpDDSurface, nullptr, core->lpDDSurface, &dst);
    RecordHd(core, SDL_Rect{0, 0, Size.x, Size.y}, x, y, false);

    // Restore color key
    if (result != -1) {
        SDL_SetColorKey(lpDDSurface, SDL_TRUE, key);
    }
    return 0;
}

ULONG SB_CBitmapCore::BlitFast(class SB_CBitmapCore *core, SLONG x, SLONG y, const CRect &rect) {
    if (!lpDDSurface) {
        return 0;
    }

    // Ignore source color key
    Uint32 key = 0;
    int result = SDL_GetColorKey(lpDDSurface, &key);
    if (result != -1) {
        SDL_SetColorKey(lpDDSurface, SDL_FALSE, key);
    }

    SDL_Rect src = {rect.left, rect.top, rect.Width(), rect.Height()};
    SDL_Rect dst = {x, y, rect.Width(), rect.Height()};
    SDL_BlitSurface(lpDDSurface, &src, core->lpDDSurface, &dst);
    RecordHd(core, SDL_Rect{rect.left, rect.top, rect.Width(), rect.Height()}, x, y, false);

    // Restore color key
    if (result != -1) {
        SDL_SetColorKey(lpDDSurface, SDL_TRUE, key);
    }
    return 0;
}

ULONG SB_CBitmapCore::BlitChar(SDL_Surface *font, SLONG x, SLONG y, const SDL_Rect &rect) {
    SDL_Rect dst = {x, y, rect.w, rect.h};
    return SDL_BlitSurface(font, &rect, lpDDSurface, &dst);
}

void SB_CBitmapCore::InitClipRect() { SDL_SetClipRect(lpDDSurface, nullptr); }

ULONG SB_CBitmapCore::Release() {
    if (gHdPrimary != nullptr && lpDDSurface != nullptr) {
        gHdPrimary->DropHdBlitsFrom(lpDDSurface); // HD-Textur selbst gehoert dem Cache, Schatten-Textur wird freigegeben
    }
    HdTexture = nullptr;
    if (lpDDSurface != nullptr) {
        SDL_FreeSurface(lpDDSurface);
    }
    if (flippedBufferSurface != nullptr) {
        SDL_FreeSurface(flippedBufferSurface);
    }
    if (lpTexture != nullptr) {
        SDL_DestroyTexture(lpTexture);
    }
    return 0;
}

bool SB_CPrimaryBitmap::FastClip(CRect clipRect, POINT *pPoint, RECT *pRect) {
    POINT offset;
    offset.x = 0;
    if (pRect->top <= 0) {
        offset.y = 0;
    } else {
        offset.y = pRect->top;
    }
    if ((offset.x != 0) || (offset.y != 0)) {
        OffsetRect(pRect, -offset.x, -offset.y);
    }
    if (pRect->right + pPoint->x >= clipRect.right) {
        pRect->right = clipRect.right - pPoint->x;
    }
    if (pPoint->x < clipRect.left) {
        pRect->left += clipRect.left - pPoint->x;
        pPoint->x = clipRect.left;
    }
    if (pRect->bottom + pPoint->y > clipRect.bottom) {
        pRect->bottom = clipRect.bottom - pPoint->y;
    }
    if (pPoint->y < clipRect.top) {
        pRect->top += clipRect.top - pPoint->y;
        pPoint->y = clipRect.top;
    }
    if ((offset.x != 0) || (offset.y != 0)) {
        OffsetRect(pRect, offset.x, offset.y);
    }
    return pRect->right - pRect->left > 0 && pRect->bottom - pRect->top > 0;
}

//--------------------------------------------------------------------------------------------
// HD-Hintergrund-Ebene (Phase 2, H1)
//--------------------------------------------------------------------------------------------
static Uint32 Rgb565ToArgb(Uint16 v) {
    static Uint32 table[65536];
    static bool init = false;
    if (!init) {
        for (Uint32 i = 0; i < 65536; i++) {
            const Uint32 r = ((i >> 11) & 31) * 255 / 31;
            const Uint32 g = ((i >> 5) & 63) * 255 / 63;
            const Uint32 b = (i & 31) * 255 / 31;
            table[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        init = true;
    }
    return table[v];
}

// Weicht jeder Kanal (R5 G6 B5) um hoechstens 1 Stufe ab?
static bool NearlyEqual565(Uint16 a, Uint16 b) {
    const int dr = int(a >> 11) - int(b >> 11);
    const int dg = int((a >> 5) & 63) - int((b >> 5) & 63);
    const int db = int(a & 31) - int(b & 31);
    return dr >= -1 && dr <= 1 && dg >= -1 && dg <= 1 && db >= -1 && db <= 1;
}

SLONG SB_BuildHdOverlay(const SDL_Surface *frame, const SDL_Surface *ref, const SDL_Rect &rect, Uint32 *dst, SLONG dstPitch, SLONG *nearMiss) {
    SLONG transparent = 0;
    SLONG nearCount = 0;
    for (SLONG y = 0; y < frame->h; y++) {
        const auto *f = reinterpret_cast<const Uint16 *>(static_cast<const Uint8 *>(frame->pixels) + y * frame->pitch);
        auto *d = reinterpret_cast<Uint32 *>(reinterpret_cast<Uint8 *>(dst) + y * dstPitch);
        const bool rowInRect = y >= rect.y && y < rect.y + rect.h && y - rect.y < ref->h;
        const Uint16 *r = rowInRect ? reinterpret_cast<const Uint16 *>(static_cast<const Uint8 *>(ref->pixels) + (y - rect.y) * ref->pitch) : nullptr;
        for (SLONG x = 0; x < frame->w; x++) {
            const SLONG rx = x - rect.x;
            const bool inRect = r != nullptr && rx >= 0 && rx < rect.w && rx < ref->w;
            if (inRect && f[x] == r[rx]) {
                d[x] = 0; // durchsichtig: HD-Hintergrund sichtbar
                transparent++;
            } else {
                d[x] = Rgb565ToArgb(f[x]);
                if (inRect && nearMiss != nullptr && NearlyEqual565(f[x], r[rx])) {
                    nearCount++;
                }
            }
        }
    }
    if (nearMiss != nullptr) {
        *nearMiss = nearCount;
    }
    return transparent;
}

SDL_Texture *SB_CPrimaryBitmap::CreateHdTexture(SDL_Surface *surface) {
    if (!CanUseHd() || surface == nullptr) {
        return nullptr;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(lpDD, surface);
    if (tex != nullptr) {
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    } else {
        AT_Log("HD: Textur %dx%d nicht angelegt: %s", surface->w, surface->h, SDL_GetError());
    }
    return tex;
}

void SB_CPrimaryBitmap::SetHdBackground(SDL_Texture *hd, const SDL_Rect &logicalRect, const SDL_Surface *ref1x) {
    if (!CanUseHd() || hd == nullptr || ref1x == nullptr || ref1x->format->format != SDL_PIXELFORMAT_RGB565) {
        return;
    }
    if (hd != HdTexture || HdRef == nullptr || HdRef->w != ref1x->w || HdRef->h != ref1x->h) {
        // Neues Original: Kopie anlegen (die Quelle kann RLE-kodiert sein, daher ueber Lock)
        if (HdRef != nullptr) {
            SDL_FreeSurface(HdRef);
        }
        HdRef = SDL_CreateRGBSurfaceWithFormat(0, ref1x->w, ref1x->h, 16, SDL_PIXELFORMAT_RGB565);
        auto *src = const_cast<SDL_Surface *>(ref1x);
        SDL_LockSurface(src);
        for (SLONG y = 0; y < ref1x->h; y++) {
            memcpy(static_cast<Uint8 *>(HdRef->pixels) + y * HdRef->pitch, static_cast<const Uint8 *>(src->pixels) + y * src->pitch, ref1x->w * 2);
        }
        SDL_UnlockSurface(src);
        AT_Log("HD-Hintergrund aktiv: %dx%d an %d,%d", ref1x->w, ref1x->h, logicalRect.x, logicalRect.y);
    }
    HdTexture = hd;
    HdRect = logicalRect;
    HdFramesSinceSet = 0;
}

void SB_CPrimaryBitmap::ForgetHdTexture(const SDL_Texture *hd) {
    if (hd != nullptr && hd == HdTexture) {
        HdTexture = nullptr;
        HdThisFrame = false;
    }
}

void SB_CPrimaryBitmap::SetOverlayLinear(bool linear) {
    OverlayLinear = linear;
    if (Overlay != nullptr) {
        SDL_SetTextureScaleMode(Overlay, linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    }
    AT_Log("HD-Overlay-Filter: %s", linear ? "linear" : "nearest");
}

void SB_CPrimaryBitmap::SetHdDebugDir(const char *dir) {
    HdDebugDir = dir != nullptr ? dir : "";
    if (!HdDebugDir.empty()) {
        AT_Log("HD-Debug: Masken-PNGs nach %s", HdDebugDir.c_str());
    }
}

//--------------------------------------------------------------------------------------------
// Zeichenliste (H4)
//--------------------------------------------------------------------------------------------
// HD-Textur zu einer HD-Surface (einmal je Surface). Alpha kommt aus dem Colorkey des 1x-Originals
// (Nearest-Neighbor; Phase 4: weich hochskaliert mit Schwelle).
SDL_Texture *SB_CPrimaryBitmap::GetHdTextureFor(SDL_Surface *hd, const SDL_Surface *orig1x, bool colorKey) {
    if (!CanUseHd() || hd == nullptr || orig1x == nullptr || orig1x->format->format != SDL_PIXELFORMAT_RGB565) {
        return nullptr;
    }
    auto it = HdTexCache.find(hd);
    if (it != HdTexCache.end()) {
        return it->second;
    }
    SDL_Surface *argb = SDL_ConvertSurfaceFormat(hd, SDL_PIXELFORMAT_ARGB8888, 0);
    if (argb == nullptr) {
        return nullptr;
    }
    if (colorKey && orig1x->w > 0 && orig1x->h > 0) {
        const SLONG sx = argb->w / orig1x->w;
        const SLONG sy = argb->h / orig1x->h;
        for (SLONG y = 0; y < argb->h; y++) {
            const auto *o = reinterpret_cast<const Uint16 *>(static_cast<const Uint8 *>(orig1x->pixels) + min(y / sy, orig1x->h - 1) * orig1x->pitch);
            auto *d = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(argb->pixels) + y * argb->pitch);
            for (SLONG x = 0; x < argb->w; x++) {
                d[x] = o[min(x / sx, orig1x->w - 1)] == 0 ? 0 : (d[x] | 0xFF000000);
            }
        }
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(lpDD, argb);
    SDL_FreeSurface(argb);
    if (tex == nullptr) {
        AT_Log("HD: Textur %dx%d nicht angelegt: %s", hd->w, hd->h, SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    HdTexCache[hd] = tex;
    return tex;
}

void SB_CPrimaryBitmap::ForgetHdSurface(const SDL_Surface *hd) {
    auto it = HdTexCache.find(hd);
    if (it == HdTexCache.end()) {
        return;
    }
    SDL_Texture *tex = it->second;
    HdTexCache.erase(it);
    // Blits dieses Frames mit der Textur verwerfen
    auto usesTex = [tex](const HdBlit &b) { return b.Tex == tex; };
    HdBlits.erase(std::remove_if(HdBlits.begin(), HdBlits.end(), usesTex), HdBlits.end());
    HdDrawList.erase(std::remove_if(HdDrawList.begin(), HdDrawList.end(), usesTex), HdDrawList.end());
    SDL_DestroyTexture(tex);
}

void SB_CPrimaryBitmap::DropHdBlitsFrom(const SDL_Surface *src) {
    auto fromSrc = [src](const HdBlit &b) { return b.Src == src; };
    HdBlits.erase(std::remove_if(HdBlits.begin(), HdBlits.end(), fromSrc), HdBlits.end());
    auto it = HdShadeCache.find(src);
    if (it != HdShadeCache.end()) {
        // Schatten-Textur gehoert zu dieser Surface: auch aus dem angezeigten Frame nehmen
        HdDrawList.erase(std::remove_if(HdDrawList.begin(), HdDrawList.end(), fromSrc), HdDrawList.end());
        SDL_DestroyTexture(it->second);
        HdShadeCache.erase(it);
    }
}

// Schwarze Textur, Alpha = 1 - Wert/8 (BlitAlpha multipliziert mit Wert/8), linear gefiltert
SDL_Texture *SB_CPrimaryBitmap::GetShadeTexture(SDL_Surface *shade) {
    auto it = HdShadeCache.find(shade);
    if (it != HdShadeCache.end()) {
        return it->second;
    }
    SDL_Surface *argb = SDL_CreateRGBSurfaceWithFormat(0, shade->w, shade->h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (argb == nullptr) {
        return nullptr;
    }
    SDL_LockSurface(shade);
    for (SLONG y = 0; y < shade->h; y++) {
        const auto *s = reinterpret_cast<const Uint16 *>(static_cast<const Uint8 *>(shade->pixels) + y * shade->pitch);
        auto *d = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(argb->pixels) + y * argb->pitch);
        for (SLONG x = 0; x < shade->w; x++) {
            const Uint32 v = min(Uint32(s[x]), Uint32(8));
            d[x] = ((8 - v) * 255 / 8) << 24;
        }
    }
    SDL_UnlockSurface(shade);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(lpDD, argb);
    SDL_FreeSurface(argb);
    if (tex != nullptr) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
        HdShadeCache[shade] = tex;
    }
    return tex;
}

void SB_CPrimaryBitmap::RecordHdShade(SB_CBitmapCore *shade, XY pos, SB_HdShadeReplay replay, const void *ctx) {
    SDL_Surface *surface = shade->GetSurface();
    if (surface == nullptr || replay == nullptr || surface->format->BytesPerPixel != 2 || HdBlits.size() >= 8192) {
        return;
    }
    SDL_Texture *tex = GetShadeTexture(surface);
    if (tex == nullptr) {
        return;
    }
    // BlitAlpha clippt nur an der Puffergroesse (Hoehe hoechstens 440), nicht am Clip-Rechteck
    const SDL_Rect clip{0, 0, lpDDSurface->w, min(lpDDSurface->h, 440)};
    HdBlits.push_back(HdBlit{surface, tex, SDL_Rect{0, 0, surface->w, surface->h}, SDL_Rect{pos.x, pos.y, surface->w, surface->h}, clip, true, replay, ctx, pos});
}

void SB_CPrimaryBitmap::RecordHdBlit(SB_CBitmapCore *src, const SDL_Rect &srcRect, SLONG x, SLONG y, bool colorKey) {
    if (HdBlits.size() >= 8192) {
        return;
    }
    HdBlits.push_back(HdBlit{src->GetSurface(), src->GetHdTexture(), srcRect, SDL_Rect{x, y, srcRect.w, srcRect.h}, lpDDSurface->clip_rect, colorKey, nullptr, nullptr, XY(0, 0)});
}

//--------------------------------------------------------------------------------------------
// Baut Referenz und Overlay fuer diesen Frame. Referenz: invertierter Frame (passt nirgends),
// darauf das 1x-Original des Hintergrunds (H2) und alle HD-Blits (H4) in Zeichenreihenfolge.
// Wo der Frame der Referenz entspricht, ist das Overlay durchsichtig und die GPU-Ebenen sichtbar.
//--------------------------------------------------------------------------------------------
void SB_CPrimaryBitmap::BuildHdOverlay() {
    HdThisFrame = false;
    // Hintergrund gilt nur, solange der Raum ihn beim Zeichnen erneuert
    HdBgThisFrame = HdTexture != nullptr && HdRef != nullptr && HdFramesSinceSet++ <= 2;
    if (!HdBgThisFrame) {
        HdTexture = nullptr;
    }
    if (!HdBgThisFrame && HdBlits.empty()) {
        HdDrawList.clear();
        return;
    }
    if (Overlay == nullptr) {
        Overlay = SDL_CreateTexture(lpDD, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Size.x, Size.y);
        if (Overlay == nullptr) {
            AT_Log("HD: Overlay nicht angelegt: %s", SDL_GetError());
            HdTexture = nullptr;
            HdBlits.clear();
            return;
        }
        SDL_SetTextureBlendMode(Overlay, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(Overlay, OverlayLinear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    }
    if (HdFullRef == nullptr || HdFullRef->w != lpDDSurface->w || HdFullRef->h != lpDDSurface->h) {
        SDL_FreeSurface(HdFullRef);
        HdFullRef = SDL_CreateRGBSurfaceWithFormat(0, lpDDSurface->w, lpDDSurface->h, 16, SDL_PIXELFORMAT_RGB565);
        if (HdFullRef == nullptr) {
            return;
        }
    }

    const Uint64 start = SDL_GetPerformanceCounter();

    // Referenz aufbauen
    for (SLONG y = 0; y < lpDDSurface->h; y++) {
        const auto *f = reinterpret_cast<const Uint16 *>(static_cast<const Uint8 *>(lpDDSurface->pixels) + y * lpDDSurface->pitch);
        auto *r = reinterpret_cast<Uint16 *>(static_cast<Uint8 *>(HdFullRef->pixels) + y * HdFullRef->pitch);
        for (SLONG x = 0; x < lpDDSurface->w; x++) {
            r[x] = Uint16(~f[x]);
        }
    }
    if (HdBgThisFrame) {
        SDL_Rect bgRect{HdRect.x, HdRect.y, min(HdRect.w, HdRef->w), min(HdRect.h, HdRef->h)};
        SDL_Rect dst = bgRect;
        SDL_Rect srcRect{0, 0, bgRect.w, bgRect.h};
        SDL_BlitSurface(HdRef, &srcRect, HdFullRef, &dst); // HdRef hat keinen Colorkey: 1:1 kopieren
    }
    for (const HdBlit &b : HdBlits) {
        if (b.Replay != nullptr) {
            SDL_SetClipRect(HdFullRef, nullptr);
            b.Replay(HdFullRef, b.Src, b.Pos, b.Ctx); // Abdunkeln wie im Frame
            continue;
        }
        SDL_Rect clip = b.Clip;
        SDL_SetClipRect(HdFullRef, &clip);
        SDL_Rect srcRect = b.SrcRect;
        SDL_Rect dst = b.Dst;
        Uint32 key = 0;
        const bool hasKey = SDL_GetColorKey(b.Src, &key) == 0;
        if (hasKey && !b.ColorKey) {
            SDL_SetColorKey(b.Src, SDL_FALSE, key);
        }
        SDL_BlitSurface(b.Src, &srcRect, HdFullRef, &dst);
        if (hasKey && !b.ColorKey) {
            SDL_SetColorKey(b.Src, SDL_TRUE, key);
        }
    }
    SDL_SetClipRect(HdFullRef, nullptr);

    void *pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(Overlay, nullptr, &pixels, &pitch) < 0) {
        HdBlits.clear();
        return;
    }
    SLONG nearMiss = 0;
    const SDL_Rect full{0, 0, lpDDSurface->w, lpDDSurface->h};
    const SLONG transparent = SB_BuildHdOverlay(lpDDSurface, HdFullRef, full, static_cast<Uint32 *>(pixels), pitch, &nearMiss);
    if (HdBgThisFrame) {
        // Anteil durchsichtiger Pixel im Hintergrund-Rechteck (Abnahme H2)
        for (SLONG y = max(0, HdRect.y); y < min(lpDDSurface->h, HdRect.y + HdRect.h); y++) {
            const auto *o = reinterpret_cast<const Uint32 *>(static_cast<const Uint8 *>(pixels) + y * pitch);
            for (SLONG x = max(0, HdRect.x); x < min(lpDDSurface->w, HdRect.x + HdRect.w); x++) {
                HdStatBgTransparent += o[x] == 0 ? 1 : 0;
            }
        }
        HdStatTotal += Uint64(min(HdRect.w, HdRef->w)) * Uint64(min(HdRect.h, HdRef->h));
    }
    SDL_UnlockTexture(Overlay);
    HdThisFrame = true;
    // Die aufgezeichneten Blits gehoeren ab jetzt zum Overlay; jedes Present bis zum naechsten
    // Flip zeichnet genau diese Liste (auch Present ohne neuen Frame aus der Hauptschleife).
    HdDrawList.swap(HdBlits);
    HdBlits.clear();
    CheckHdDip(transparent, full.w * full.h);

    HdStatTransparent += transparent;
    HdStatNearMiss += nearMiss;
    HdStatFrameTotal += Uint64(full.w) * Uint64(full.h);
    for (const HdBlit &b : HdDrawList) {
        (b.Replay != nullptr ? HdStatShades : HdStatBlits)++;
    }
    HdStatTicks += SDL_GetPerformanceCounter() - start;
    HdStatFrames++;
    LogHdStats();
}

// Einbruch-Erkennung (nur mit Debug-Ordner): Faellt der Anteil durchsichtiger Pixel eines Frames
// um mehr als 10 Prozentpunkte unter den gleitenden Mittelwert, werden dieser und die naechsten
// 4 Frames als hd_dip<n>_<k>_{frame,ref,mask}.png gespeichert (hoechstens alle 30 s).
void SB_CPrimaryBitmap::CheckHdDip(SLONG transparent, SLONG total) {
    const double pctNow = total != 0 ? 100.0 * double(transparent) / double(total) : 0.0;
    if (!HdDebugDir.empty()) {
        const Uint64 now = SDL_GetPerformanceCounter();
        const Uint64 freq = SDL_GetPerformanceFrequency();
        if (HdDipFramesLeft == 0 && HdAvgPct >= 0.0 && pctNow < HdAvgPct - 10.0 && (HdLastDip == 0 || now - HdLastDip > 30 * freq)) {
            HdDipFramesLeft = 5;
            HdDipSeries++;
            HdLastDip = now;
            AT_Log("HD-Debug: Einbruch %.1f %% statt ~%.1f %% durchsichtig (%zu HD-Blits), speichere 5 Frames als hd_dip%d_*", pctNow, HdAvgPct,
                   HdDrawList.size(), HdDipSeries);
        }
        if (HdDipFramesLeft > 0) {
            char prefix[32];
            snprintf(prefix, sizeof(prefix), "hd_dip%d_%d_", HdDipSeries, 6 - HdDipFramesLeft);
            DumpHdDebug(prefix);
            AT_Log("HD-Debug: %s %.1f %% durchsichtig, %zu HD-Blits", prefix, pctNow, HdDrawList.size());
            HdDipFramesLeft--;
        }
    }
    HdAvgPct = HdAvgPct < 0.0 ? pctNow : 0.9 * HdAvgPct + 0.1 * pctNow;
}

void SB_CPrimaryBitmap::LogHdStats() {
    const Uint64 now = SDL_GetPerformanceCounter();
    const Uint64 freq = SDL_GetPerformanceFrequency();
    if (HdStatLast == 0) {
        HdStatLast = now;
    }
    if (now - HdStatLast < 5 * freq || HdStatFrames == 0) {
        return;
    }
    auto pct = [](Uint64 a, Uint64 b) { return b != 0 ? 100.0 * double(a) / double(b) : 0.0; };
    if (HdStatTotal != 0) {
        AT_Log("HD-Hintergrund: %.1f %% des Hintergrunds durchsichtig (Bezug: Hintergrund-Rechteck %dx%d)", pct(HdStatBgTransparent, HdStatTotal),
               HdRect.w, HdRect.h);
    }
    AT_Log("HD: %.1f %% des Frames durchsichtig, %.1f %% deckend aber nur knapp abweichend (<=1 Stufe), %.1f HD-Blits/Frame, %.1f Schatten/Frame, "
           "Overlay %.2f ms/Frame (%d Frames, %llu Present davon %llu ohne neuen Frame)",
           pct(HdStatTransparent, HdStatFrameTotal), pct(HdStatNearMiss, HdStatFrameTotal), double(HdStatBlits) / HdStatFrames,
           double(HdStatShades) / HdStatFrames,
           1000.0 * double(HdStatTicks) / double(freq) / HdStatFrames, HdStatFrames, static_cast<unsigned long long>(HdStatPresents),
           static_cast<unsigned long long>(HdStatPresentsOnly));
    if (!HdDebugDir.empty() && HdDipFramesLeft == 0) {
        DumpHdDebug("hd_");
    }
    HdStatTransparent = HdStatNearMiss = HdStatTotal = HdStatTicks = 0;
    HdStatBlits = HdStatShades = HdStatBgTransparent = HdStatFrameTotal = 0;
    HdStatPresents = HdStatPresentsOnly = 0;
    HdStatFrames = 0;
    HdStatLast = now;
}

// Legt hd_frame.png (1x-Frame), hd_ref.png (Referenz) und hd_mask.png ab. In der Maske ist
// durchsichtig = magenta, knapp abweichend = gelb, sonst der Frame-Pixel (= im Overlay deckend).
// Pixel der Referenz, die nicht zu HD gehoeren, sind invertiert (passen nie).
void SB_CPrimaryBitmap::DumpHdDebug(const std::string &prefix) {
    if (HdFullRef == nullptr || lpDDSurface == nullptr) {
        return;
    }
    SDL_Surface *mask = SDL_CreateRGBSurfaceWithFormat(0, lpDDSurface->w, lpDDSurface->h, 16, SDL_PIXELFORMAT_RGB565);
    SDL_Surface *frame = SDL_CreateRGBSurfaceWithFormat(0, lpDDSurface->w, lpDDSurface->h, 16, SDL_PIXELFORMAT_RGB565);
    if (mask == nullptr || frame == nullptr) {
        SDL_FreeSurface(mask);
        SDL_FreeSurface(frame);
        return;
    }
    for (SLONG y = 0; y < lpDDSurface->h; y++) {
        const auto *f = reinterpret_cast<const Uint16 *>(static_cast<const Uint8 *>(lpDDSurface->pixels) + y * lpDDSurface->pitch);
        const auto *r = reinterpret_cast<const Uint16 *>(static_cast<const Uint8 *>(HdFullRef->pixels) + y * HdFullRef->pitch);
        auto *m = reinterpret_cast<Uint16 *>(static_cast<Uint8 *>(mask->pixels) + y * mask->pitch);
        auto *fr = reinterpret_cast<Uint16 *>(static_cast<Uint8 *>(frame->pixels) + y * frame->pitch);
        for (SLONG x = 0; x < lpDDSurface->w; x++) {
            fr[x] = f[x];
            m[x] = f[x] == r[x] ? 0xF81F : (NearlyEqual565(f[x], r[x]) ? 0xFFE0 : f[x]);
        }
    }
    const std::string base = HdDebugDir + "/" + prefix;
    IMG_SavePNG(frame, (base + "frame.png").c_str());
    IMG_SavePNG(HdFullRef, (base + "ref.png").c_str());
    if (IMG_SavePNG(mask, (base + "mask.png").c_str()) == 0) {
        AT_Log("HD-Debug: %sframe.png, %sref.png, %smask.png gespeichert", prefix.c_str(), prefix.c_str(), prefix.c_str());
    } else {
        AT_Log("HD-Debug: Speichern fehlgeschlagen: %s", IMG_GetError());
    }
    SDL_FreeSurface(mask);
    SDL_FreeSurface(frame);
}

SLONG SB_CPrimaryBitmap::Flip() {
    if (lpDD != nullptr) {
        BuildHdOverlay(); // liest den fertigen Frame, bevor die Textur entsperrt wird
        HdFromFlip = true;
        /*
         * None of the SDL renderers actually lock the GPU resource,
         * they all use either staging memory or a staging texture.
         * Thus we can still use the texture while it's locked and
         * we simply cycle through lock/unlock to update the texture.
         */
        SDL_UnlockTexture(lpTexture);
        if (SDL_LockTextureToSurface(lpTexture, nullptr, &lpDDSurface) < 0) {
            return -1;
        }
    } else {
        if (Cursor != nullptr) {
            Cursor->FlipBegin();
        }

        SDL_Rect target = SDL_Rect{TargetOffset.x, TargetOffset.y, TargetSize.x, TargetSize.y};
        if (SDL_BlitScaled(lpDDSurface, nullptr, SDL_GetWindowSurface(Window), &target) < 0) {
            return -2;
        }

        if (Cursor != nullptr) {
            Cursor->FlipEnd();
        }
    }

    return Present();
}

SLONG SB_CPrimaryBitmap::Present() {
    if (lpDD != nullptr) {
        SDL_SetRenderDrawColor(lpDD, 0, 0, 0, 255);
        SDL_RenderClear(lpDD);

        // Set the backbuffer as the render target
        if (SDL_SetRenderTarget(lpDD, nullptr) < 0) {
            return -1;
        }

        const SDL_Rect target = SDL_Rect{TargetOffset.x, TargetOffset.y, TargetSize.x, TargetSize.y};
        if (HdThisFrame) {
            // GPU-Ebenen: ggf. 1x-Frame als Basis, HD-Hintergrund, HD-Blits; darueber der 1x-Frame mit Differenzmaske
            const float sx = float(TargetSize.x) / float(Size.x);
            const float sy = float(TargetSize.y) / float(Size.y);
            const SDL_FRect full{float(TargetOffset.x), float(TargetOffset.y), float(TargetSize.x), float(TargetSize.y)};
            auto toTarget = [&](const SDL_Rect &r) {
                return SDL_FRect{float(TargetOffset.x) + float(r.x) * sx, float(TargetOffset.y) + float(r.y) * sy, float(r.w) * sx, float(r.h) * sy};
            };
            // Basis: 1x-Frame, damit unter weichen HD-Raendern nie Schwarz durchscheint
            SDL_RenderCopyF(lpDD, lpTexture, nullptr, &full);
            if (HdBgThisFrame) {
                const SDL_FRect bg = toTarget(HdRect);
                SDL_RenderCopyF(lpDD, HdTexture, nullptr, &bg);
            }
            const SLONG s = SB_GetRenderScale();
            for (const HdBlit &b : HdDrawList) {
                const SDL_FRect c = toTarget(b.Clip);
                const SDL_Rect clip{SLONG(c.x), SLONG(c.y), SLONG(c.x + c.w + 0.999F) - SLONG(c.x), SLONG(c.y + c.h + 0.999F) - SLONG(c.y)};
                SDL_RenderSetClipRect(lpDD, &clip);
                const SLONG ts = b.Replay != nullptr ? 1 : s; // Schatten-Texturen sind 1x, linear gefiltert
                const SDL_Rect src{b.SrcRect.x * ts, b.SrcRect.y * ts, b.SrcRect.w * ts, b.SrcRect.h * ts};
                const SDL_FRect dst = toTarget(b.Dst);
                SDL_SetTextureBlendMode(b.Tex, b.ColorKey ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
                SDL_RenderCopyF(lpDD, b.Tex, &src, &dst);
            }
            SDL_RenderSetClipRect(lpDD, nullptr);
            if (SDL_RenderCopyF(lpDD, Overlay, nullptr, &full) < 0) {
                return -2;
            }
        } else if (SDL_RenderCopy(lpDD, lpTexture, nullptr, &target) < 0) {
            // Copy our primary texture to the backbuffer
            return -2;
        }

        // Render the cursor onto the backbuffer
        if (Cursor != nullptr) {
            Cursor->Render(lpDD);
        }

        SDL_RenderPresent(lpDD);
        if (HdThisFrame) {
            HdStatPresents++;
            HdStatPresentsOnly += HdFromFlip ? 0 : 1;
        }
        HdFromFlip = false;
    } else {
        if (SDL_UpdateWindowSurface(Window) < 0) {
            return -3;
        }
        SDL_Delay(10); // Ensure we don't run too fast without v-sync
    }
    return 0;
}

void SB_CPrimaryBitmap::SetTarget(XY offset, XY size) {
    this->TargetOffset = offset;
    this->TargetSize = size;
}

SLONG SB_CPrimaryBitmap::Create(SDL_Renderer **out, SDL_Window *Wnd, unsigned short /*flags*/, SLONG w, SLONG h, unsigned char /*unused*/,
                                unsigned short /*unused*/) {
    SDL_ClearError();

    Window = Wnd;
    lpDD = SDL_CreateRenderer(Window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

    if (lpDD != nullptr) {
        AT_Log("Using hardware accelerated presentation");
        lpTexture = SDL_CreateTexture(lpDD, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);

        if (SDL_LockTextureToSurface(lpTexture, nullptr, &lpDDSurface) < 0) {
            AT_Log("Unable to lock backbuffer to surface");
            return -1;
        }
        gHdPrimary = this;
    } else {
        AT_Log("Falling back to software presentation");
        AT_Log("Reason for fallback: %s", SDL_GetError());
        SDL_ClearError();

        lpTexture = nullptr;
        lpDDSurface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 16, SDL_PIXELFORMAT_RGB565);
    }

    Size.x = w;
    Size.y = h;
    TargetSize = XY(w, h);
    Cursor = nullptr;
    InitClipRect();
    *out = lpDD;
    return 0;
}

ULONG SB_CPrimaryBitmap::Release() {
    if (gHdPrimary == this) {
        gHdPrimary = nullptr;
    }
    HdBlits.clear();
    HdDrawList.clear();
    for (auto &t : HdTexCache) {
        SDL_DestroyTexture(t.second);
    }
    HdTexCache.clear();
    for (auto &t : HdShadeCache) {
        SDL_DestroyTexture(t.second);
    }
    HdShadeCache.clear();
    if (HdFullRef != nullptr) {
        SDL_FreeSurface(HdFullRef);
        HdFullRef = nullptr;
    }
    if (HdRef != nullptr) {
        SDL_FreeSurface(HdRef);
        HdRef = nullptr;
    }
    if (lpDD == nullptr) {
        if (lpDDSurface != nullptr) {
            SDL_FreeSurface(lpDDSurface);
            lpDDSurface = nullptr;
        }
        assert(lpTexture == nullptr);
    } else {
        if (Overlay != nullptr) {
            SDL_DestroyTexture(Overlay);
            Overlay = nullptr;
        }
        HdTexture = nullptr;
        if (lpTexture != nullptr) {
            SDL_DestroyTexture(lpTexture);
            lpTexture = nullptr;
        }
        SDL_DestroyRenderer(lpDD);
        lpDD = nullptr;
        lpDDSurface = nullptr;
    }
    return 0;
}

SB_CBitmapKey::SB_CBitmapKey(class SB_CBitmapCore &core) : Surface(core.lpDDSurface) {
    if (Surface == nullptr) {
        return;
    }
    if (SDL_MUSTLOCK(Surface)) {
        SDL_LockSurface(Surface);
    }
    Bitmap = Surface->pixels;
    lPitch = Surface->pitch;
}

SB_CBitmapKey::~SB_CBitmapKey() {
    if (Surface == nullptr) {
        return;
    }
    if (SDL_MUSTLOCK(Surface)) {
        SDL_UnlockSurface(Surface);
    }
}
