/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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

TTFText::TTFText(){

}

TTFText::~TTFText(){
    if (ownsFont) TTF_CloseFont(textFont);
    SDL_DestroyTexture(outTexture);
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
    curText = const_cast<char *>(txt);
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