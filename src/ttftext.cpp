/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ttftext.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#ifdef FROZEN_BUBBLE_TEST_ACCESS
size_t TTFText::testTextureCreationCount = 0;
#endif

namespace {

float g_textScale = 1.f;

// Canvas pixels to texture pixels at `scale`; anything non-zero stays at
// least one pixel.
int Px(int canvasPx, float scale) {
    return canvasPx ? std::max(1, (int)std::lround(canvasPx * scale)) : 0;
}

int ToCanvas(int px, float scale) { return (int)std::lround(px / scale); }

// Sets a font to `scale` times its size for one render and puts it back
// after. Fonts are shared (between TTFTexts, and with the menu theme), so the
// change must not outlive the render.
class ScaledFont {
public:
    ScaledFont(TTF_Font *f, float scale) : font(f), base(TTF_GetFontSize(f)) {
        scaled = scale != 1.f && base > 0.f && TTF_SetFontSize(font, base * scale);
    }
    ~ScaledFont() { if (scaled) TTF_SetFontSize(font, base); }
    ScaledFont(const ScaledFont&) = delete;
    ScaledFont& operator=(const ScaledFont&) = delete;
private:
    TTF_Font *font;
    float base;
    bool scaled = false;
};

// Where to stamp the ring's copies of the text. At 1x it is the original
// eight neighbours; a scaled ring is several pixels wide, where eight copies
// leave notches, so it fills a disc instead.
std::vector<SDL_Point> RingOffsets(int d, bool scaled) {
    if (!scaled)
        return {{-d,-d},{0,-d},{d,-d},{-d,0},{d,0},{-d,d},{0,d},{d,d}};
    std::vector<SDL_Point> pts;
    for (int r = d; r > 0; r -= 2) {
        const int n = std::max(8, (int)std::ceil(3.14159265f * r));  // ~2px apart
        for (int i = 0; i < n; ++i) {
            const float a = 2 * 3.14159265f * i / n;
            pts.push_back({(int)std::lround(std::cos(a) * r), (int)std::lround(std::sin(a) * r)});
        }
    }
    return pts;
}

}  // namespace

void SetTextRenderScale(float scale) { g_textScale = scale > 1.f ? scale : 1.f; }
float TextRenderScale() { return g_textScale; }

SDL_Texture *RenderRingedText(const SDL_Renderer *rend, TTF_Font *font,
                               const char *text, SDL_Color fg, SDL_Color ring,
                               int ringPx, SDL_Point *outSize)
{
    if (outSize) *outSize = SDL_Point{0, 0};
    if (!font || !text || !*text) return nullptr;

    const float scale = g_textScale;
    ScaledFont scaledFont(font, scale);
    SDL_Surface *front = TTF_RenderText_Blended(font, text, 0, fg);
    if (!front) return nullptr;

    // Pad by however far the ring or shadow reaches, so neither is clipped.
    const int pad = ringPx > 0 ? Px(ringPx, scale) : (ring.a ? Px(1, scale) : 0);
    SDL_Surface *canvas = SDL_CreateSurface(front->w + pad * 2, front->h + pad * 2,
                                            SDL_PIXELFORMAT_ARGB8888);
    if (!canvas) { SDL_DestroySurface(front); return nullptr; }

    if (ring.a) {
        SDL_Surface *shadow = TTF_RenderText_Blended(font, text, 0, ring);
        if (shadow) {
            SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_BLEND);
            if (ringPx > 0) {
                for (const SDL_Point &o : RingOffsets(pad, scale != 1.f)) {
                    SDL_Rect dst = {pad + o.x, pad + o.y, shadow->w, shadow->h};
                    SDL_BlitSurface(shadow, nullptr, canvas, &dst);
                }
            } else {
                SDL_Rect dst = {pad + pad, pad + pad, shadow->w, shadow->h};
                SDL_BlitSurface(shadow, nullptr, canvas, &dst);
            }
            SDL_DestroySurface(shadow);
        }
    }

    SDL_SetSurfaceBlendMode(front, SDL_BLENDMODE_BLEND);
    { SDL_Rect dst = {pad, pad, front->w, front->h}; SDL_BlitSurface(front, nullptr, canvas, &dst); }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(const_cast<SDL_Renderer *>(rend), canvas);
    if (outSize) *outSize = SDL_Point{ToCanvas(canvas->w, scale), ToCanvas(canvas->h, scale)};

    SDL_DestroySurface(front);
    SDL_DestroySurface(canvas);
    return tex;
}

TTFText::~TTFText(){
    if (ownsFont && textFont) TTF_CloseFont(textFont);
    if (outTexture) SDL_DestroyTexture(outTexture);
}

TTFText::TTFText(TTFText&& other) noexcept
    : curText(std::move(other.curText)),
      curWrapLength(other.curWrapLength),
      textureRenderer(other.textureRenderer),
      textureDirty(other.textureDirty),
      textureScale(other.textureScale),
      coords(other.coords),
      forecolor(other.forecolor),
      backcolor(other.backcolor),
      useRing(other.useRing),
      ringColor(other.ringColor),
      ringPx(other.ringPx),
      textFont(other.textFont),
      ownsFont(other.ownsFont),
      outTexture(other.outTexture)
{
    other.textFont = nullptr;
    other.ownsFont = false;
    other.outTexture = nullptr;
    other.textureRenderer = nullptr;
    other.textureDirty = true;
}

TTFText& TTFText::operator=(TTFText&& other) noexcept {
    if (this == &other) return *this;

    if (ownsFont && textFont) TTF_CloseFont(textFont);
    if (outTexture) SDL_DestroyTexture(outTexture);

    curText = std::move(other.curText);
    coords = other.coords;
    forecolor = other.forecolor;
    backcolor = other.backcolor;
    useRing = other.useRing;
    ringColor = other.ringColor;
    ringPx = other.ringPx;
    curWrapLength = other.curWrapLength;
    textureRenderer = other.textureRenderer;
    textureDirty = other.textureDirty;
    textureScale = other.textureScale;
    textFont = other.textFont;
    ownsFont = other.ownsFont;
    outTexture = other.outTexture;

    other.textFont = nullptr;
    other.ownsFont = false;
    other.outTexture = nullptr;
    other.textureRenderer = nullptr;
    other.textureDirty = true;
    return *this;
}

void TTFText::InvalidateTexture() {
    textureDirty = true;
    textureRenderer = nullptr;
}

void TTFText::LoadFont(const char *path, int size) {
    if (ownsFont && textFont) {
        TTF_CloseFont(textFont);
    }
    textFont = TTF_OpenFont(path, (float)size);
    ownsFont = true;
    InvalidateTexture();
}
void TTFText::LoadFont(TTF_Font *fnt) {
    if (!ownsFont && textFont == fnt) return;
    if (ownsFont && textFont) {
        TTF_CloseFont(textFont);
    }
    textFont = fnt;
    ownsFont = false;  // External font — caller owns its lifetime
    InvalidateTexture();
}

void TTFText::UpdateText(const SDL_Renderer *rend, const char *txt, int wrapLength) {
    if (!textFont || !txt) {
        if (outTexture != nullptr) { SDL_DestroyTexture(outTexture); outTexture = nullptr; }
        curText.clear();
        InvalidateTexture();
        return;
    }
    const float scale = g_textScale;
    if (!textureDirty && outTexture != nullptr && textureRenderer == rend &&
        curWrapLength == wrapLength && curText == txt && textureScale == scale) {
        return;
    }
    if (outTexture != nullptr) { SDL_DestroyTexture(outTexture); outTexture = nullptr; }
    curText = txt;
    curWrapLength = wrapLength;
    textureDirty = true;

    if (useRing) {
        SDL_Point sz{};
        outTexture = RenderRingedText(rend, textFont, txt, forecolor, ringColor, ringPx, &sz);
        coords.w = sz.x;
        coords.h = sz.y;
        if (outTexture != nullptr) {
#ifdef FROZEN_BUBBLE_TEST_ACCESS
            ++testTextureCreationCount;
#endif
            textureRenderer = rend;
            textureDirty = false;
            textureScale = scale;
        }
        return;
    }

    ScaledFont scaledFont(textFont, scale);
    const int wrap = wrapLength > 0 ? Px(wrapLength, scale) : wrapLength;
    SDL_Surface *front = TTF_RenderText_Blended_Wrapped(textFont, txt, 0, forecolor, wrap);
    if (!front) return;
    SDL_Surface *back = TTF_RenderText_Blended_Wrapped(textFont, txt, 0, backcolor, wrap);
    if (!back) { SDL_DestroySurface(front); return; }
    // The shadow sits one canvas pixel down-right of the text.
    const int shadow = Px(1, scale);
    SDL_Rect end = {-shadow, -shadow, front->w, front->h};
    SDL_BlitSurface(front, nullptr, back, &end);
    outTexture = SDL_CreateTextureFromSurface(const_cast<SDL_Renderer *>(rend), back);
    coords.w = ToCanvas(back->w, scale);
    coords.h = ToCanvas(back->h, scale);
    if (outTexture != nullptr) {
#ifdef FROZEN_BUBBLE_TEST_ACCESS
        ++testTextureCreationCount;
#endif
        textureRenderer = rend;
        textureDirty = false;
        textureScale = scale;
    }
    SDL_DestroySurface(front);
    SDL_DestroySurface(back);
}

void TTFText::UpdateAlignment(int align) {
    if (textFont && TTF_GetFontWrapAlignment(textFont) != (TTF_HorizontalAlignment)align) {
        TTF_SetFontWrapAlignment(textFont, (TTF_HorizontalAlignment)align);
        InvalidateTexture();
    }
}

void TTFText::UpdateColor(SDL_Color fg, SDL_Color bg) {
    if (forecolor.r != fg.r || forecolor.g != fg.g || forecolor.b != fg.b || forecolor.a != fg.a ||
        backcolor.r != bg.r || backcolor.g != bg.g || backcolor.b != bg.b || backcolor.a != bg.a) {
        InvalidateTexture();
    }
    forecolor = fg;
    backcolor = bg;
}

void TTFText::UpdateRing(SDL_Color ring, int px) {
    if (!useRing || ringColor.r != ring.r || ringColor.g != ring.g ||
        ringColor.b != ring.b || ringColor.a != ring.a || ringPx != px) {
        InvalidateTexture();
    }
    useRing = true;
    ringColor = ring;
    ringPx = px;
}

void TTFText::UpdateStyle(int size, int style) {
    if (textFont && (TTF_GetFontSize(textFont) != (float)size ||
                     TTF_GetFontStyle(textFont) != static_cast<TTF_FontStyleFlags>(style))) {
        TTF_SetFontSize(textFont, (float)size);
        TTF_SetFontStyle(textFont, style);
        InvalidateTexture();
    }
}

void TTFText::UpdateStyle(int style) {
    if (textFont && TTF_GetFontStyle(textFont) != static_cast<TTF_FontStyleFlags>(style)) {
        TTF_SetFontStyle(textFont, style);
        InvalidateTexture();
    }
}

void TTFText::UpdatePosition(SDL_Point xy) {
    coords.x = xy.x;
    coords.y = xy.y;
}
