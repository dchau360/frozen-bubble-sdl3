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

#ifndef TTFTEXT_H
#define TTFTEXT_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <string>
#include "sdl3_compat.h"

#define WINDOW_W 640
#define WINDOW_H 480
#define SURF_FORMAT SDL_PIXELFORMAT_ARGB8888

class TTFText final
{
public:
    void LoadFont(const char *path, int size);
    void LoadFont(TTF_Font *fnt);

    void UpdateText(const SDL_Renderer *rend, const char *txt, int wrapLength);
    void UpdateAlignment(int align);
    void UpdateColor(SDL_Color fg, SDL_Color bg);
    void UpdateStyle(int size, int style);
    void UpdateStyle(int style);
    void UpdatePosition(SDL_Point xy);

    const char *Text() { return curText.c_str(); };
    SDL_Rect *Coords() { return &coords; };
    SDL_Texture *Texture() { return outTexture; };

    TTFText() = default;
    ~TTFText();

    // Owns a font (maybe) and a rendered texture, so a copy would either
    // double-free or need to duplicate the GPU texture with no renderer
    // guaranteed to be on hand at copy time. The previous copy constructor
    // silently reset the copy to empty and the copy assignment silently did
    // nothing -- neither freed anything, but a HighscoreData stored by value
    // in a std::vector lost its just-rendered text on every push_back
    // (BUG-045). Nothing needs an actual copy: HighscoreData only needs to
    // relocate this into the vector, so move is enough.
    TTFText(const TTFText&) = delete;
    TTFText& operator=(const TTFText&) = delete;
    TTFText(TTFText&& other) noexcept;
    TTFText& operator=(TTFText&& other) noexcept;
private:
    std::string curText;

    SDL_Rect coords{};
    SDL_Color forecolor{}, backcolor{};

    TTF_Font *textFont = nullptr;
    bool ownsFont = false;
    SDL_Texture *outTexture = nullptr;
};

// Renders `text` as a new texture, ringed with an outline `ringPx` pixels
// thick (eight offset copies in `ring`, cheaper and more portable than
// TTF_SetFontOutline's second font handle) or, when ringPx is 0 and `ring`
// has alpha, a plain 1px drop-shadow instead. Extracted out of menutheme.cpp
// (whose MenuThemeRenderLabel is now a thin wrapper over this) so a screen
// that isn't part of MENU STYLE -- the single-player sub-menu, whose old
// baked labels this fixed -- can render legible text over a busy background
// without pulling in the theme system to do it. Caller owns the returned
// texture (null on failure) and should cache it: a full TTF render plus a
// texture upload is too costly to repeat every frame.
SDL_Texture *RenderRingedText(const SDL_Renderer *rend, TTF_Font *font,
                               const char *text, SDL_Color fg, SDL_Color ring,
                               int ringPx, SDL_Point *outSize);

#endif //TTFTEXT_H