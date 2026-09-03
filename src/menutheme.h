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

#ifndef MENUTHEME_H
#define MENUTHEME_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

/*
 * Title-screen menu themes, cycled by the MENU STYLE row.
 *
 * Classic is the original artwork: each row's label is baked into
 * share/gfx/menu/txt_<name>_{off,over}.png, so it needs no font at all. Every
 * other theme draws its label at runtime with SDL_ttf, which is what makes
 * five themes affordable -- baking them would have meant 5 x 8 x 2 = 80 PNGs.
 *
 * Clear is deliberately not a hand-coded imitation of the Classic plate: it
 * blits the same artwork Classic uses, from the label-free plate that
 * tools/make-menustyle-plate.py cloned out of it, and puts clean type on top.
 * Only Slate, Ice and Pop paint their own plate.
 */
enum MenuThemeId {
    MENU_THEME_CLASSIC = 0,
    MENU_THEME_CLEAR,
    MENU_THEME_SLATE,
    MENU_THEME_ICE,
    MENU_THEME_POP,
    MENU_THEME_COUNT
};

// Short name shown on the MENU STYLE row itself ("CLASSIC", "ICE", ...).
const char *MenuThemeName(int theme);

struct MenuThemeStyle {
    const char *fontPath;     // nullptr for Classic's baked rows
    int   fontSize;
    int   labelX;             // label inset from the plate's left edge
    int   outline;            // >0: ring the glyphs this many px in shadow color
    bool  usesPlateArt;       // true: blit the Classic plate; false: paint one
    SDL_Color textIdle, textActive;
    SDL_Color shadowIdle, shadowActive;
    // Painted-plate colors (ignored when usesPlateArt).
    SDL_Color plateTopIdle, plateBotIdle, plateBorderIdle;
    SDL_Color plateTopActive, plateBotActive, plateBorderActive;
    SDL_Color topHighlight;   // 1px line inside the top edge; a=0 disables
    SDL_Color railActive;     // left accent bar on the selected row; a=0 disables
};

const MenuThemeStyle &MenuStyleFor(int theme);

// Paints (or blits) one row's plate. Never called for Classic's seven baked
// rows -- MenuButton blits those directly -- but is called for its MENU STYLE
// row, which has no baked art because its label names the current theme.
void MenuThemeDrawPlate(const SDL_Renderer *rend, const SDL_Rect &rect, int theme, bool active);

// Renders one label into a new texture the caller owns. Returns nullptr if the
// theme has no font (Classic's baked rows) or the font failed to open.
// Callers cache the result: this is a full TTF render plus a texture upload,
// far too costly to repeat every frame.
SDL_Texture *MenuThemeRenderLabel(const SDL_Renderer *rend, int theme,
                                  const char *text, bool active, SDL_Point *outSize);

// Frees the shared fonts and plate textures. Safe to call more than once.
void MenuThemeShutdown();

#endif // MENUTHEME_H
