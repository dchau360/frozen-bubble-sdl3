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
#include <utility>

SDL_Texture *RenderRingedText(const SDL_Renderer *rend, TTF_Font *font,
                               const char *text, SDL_Color fg, SDL_Color ring,
                               int ringPx, SDL_Point *outSize)
{
    if (outSize) *outSize = SDL_Point{0, 0};
    if (!font || !text || !*text) return nullptr;

    SDL_Surface *front = TTF_RenderText_Blended(font, text, 0, fg);
    if (!front) return nullptr;

    // Pad by however far the ring or shadow reaches, so neither is clipped.
    const int pad = ringPx > 0 ? ringPx : (ring.a ? 1 : 0);
    SDL_Surface *canvas = SDL_CreateSurface(front->w + pad * 2, front->h + pad * 2,
                                            SDL_PIXELFORMAT_ARGB8888);
    if (!canvas) { SDL_DestroySurface(front); return nullptr; }

    if (ring.a) {
        SDL_Surface *shadow = TTF_RenderText_Blended(font, text, 0, ring);
        if (shadow) {
            SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_BLEND);
            if (ringPx > 0) {
                const int d = ringPx;
                const int off[8][2] = {{-d,-d},{0,-d},{d,-d},{-d,0},{d,0},{-d,d},{0,d},{d,d}};
                for (const auto &o : off) {
                    SDL_Rect dst = {pad + o[0], pad + o[1], shadow->w, shadow->h};
                    SDL_BlitSurface(shadow, nullptr, canvas, &dst);
                }
            } else {
                SDL_Rect dst = {pad + 1, pad + 1, shadow->w, shadow->h};
                SDL_BlitSurface(shadow, nullptr, canvas, &dst);
            }
            SDL_DestroySurface(shadow);
        }
    }

    SDL_SetSurfaceBlendMode(front, SDL_BLENDMODE_BLEND);
    { SDL_Rect dst = {pad, pad, front->w, front->h}; SDL_BlitSurface(front, nullptr, canvas, &dst); }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(const_cast<SDL_Renderer *>(rend), canvas);
    if (outSize) *outSize = SDL_Point{canvas->w, canvas->h};

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
      coords(other.coords),
      forecolor(other.forecolor),
      backcolor(other.backcolor),
      textFont(other.textFont),
      ownsFont(other.ownsFont),
      outTexture(other.outTexture)
{
    other.textFont = nullptr;
    other.ownsFont = false;
    other.outTexture = nullptr;
}

TTFText& TTFText::operator=(TTFText&& other) noexcept {
    if (this == &other) return *this;

    if (ownsFont && textFont) TTF_CloseFont(textFont);
    if (outTexture) SDL_DestroyTexture(outTexture);

    curText = std::move(other.curText);
    coords = other.coords;
    forecolor = other.forecolor;
    backcolor = other.backcolor;
    textFont = other.textFont;
    ownsFont = other.ownsFont;
    outTexture = other.outTexture;

    other.textFont = nullptr;
    other.ownsFont = false;
    other.outTexture = nullptr;
    return *this;
}

void TTFText::LoadFont(const char *path, int size) {
    if (ownsFont && textFont) {
        TTF_CloseFont(textFont);
    }
    textFont = TTF_OpenFont(path, (float)size);
    ownsFont = true;
}
void TTFText::LoadFont(TTF_Font *fnt) {
    if (ownsFont && textFont) {
        TTF_CloseFont(textFont);
    }
    textFont = fnt;
    ownsFont = false;  // External font — caller owns its lifetime
}

void TTFText::UpdateText(const SDL_Renderer *rend, const char *txt, int wrapLength) {
    if (outTexture != nullptr) { SDL_DestroyTexture(outTexture); outTexture = nullptr; }
    if (!textFont || !txt) return;
    curText = txt;
    SDL_Surface *front = TTF_RenderText_Blended_Wrapped(textFont, txt, 0, forecolor, wrapLength);
    if (!front) return;
    SDL_Surface *back = TTF_RenderText_Blended_Wrapped(textFont, txt, 0, backcolor, wrapLength);
    if (!back) { SDL_DestroySurface(front); return; }
    SDL_Rect end = {-1, -1, front->w, front->h};
    SDL_BlitSurface(front, nullptr, back, &end);
    outTexture = SDL_CreateTextureFromSurface(const_cast<SDL_Renderer *>(rend), back);
    coords.w = back->w;
    coords.h = back->h;
    SDL_DestroySurface(front);
    SDL_DestroySurface(back);
}

void TTFText::UpdateAlignment(int align) {
    if (textFont) TTF_SetFontWrapAlignment(textFont, (TTF_HorizontalAlignment)align);
}

void TTFText::UpdateColor(SDL_Color fg, SDL_Color bg) {
    forecolor = fg;
    backcolor = bg;
}

void TTFText::UpdateStyle(int size, int style) {
    if (textFont) { TTF_SetFontSize(textFont, (float)size); TTF_SetFontStyle(textFont, style); }
}

void TTFText::UpdateStyle(int style) {
    if (textFont) TTF_SetFontStyle(textFont, style);
}

void TTFText::UpdatePosition(SDL_Point xy) {
    coords.x = xy.x;
    coords.y = xy.y;
}