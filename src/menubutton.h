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

#ifndef MENUBUTTON_H
#define MENUBUTTON_H

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <utility>
#include <string>
#include <vector>

class MenuButton final
{
public:
    MenuButton(uint32_t x, uint32_t y, const std::string &name, const SDL_Renderer *renderer, const std::string icontag, const int sheetlen);
    MenuButton(const MenuButton&) = delete;
    MenuButton & operator= ( const MenuButton & ) = delete;
    MenuButton(MenuButton&& src) noexcept;
    MenuButton & operator= ( MenuButton && ) = delete;
    ~MenuButton();
    void Render(const SDL_Renderer *renderer);
    void Pressed(void *parent);
    void Activate();
    void Deactivate();
private:
    // Rebuilds labelIdle/labelActive when the theme or the label text has
    // changed since they were last rendered. Both are full TTF renders plus a
    // texture upload, so they are cached rather than redone each frame; the
    // MENU STYLE row is the one whose text moves, and only when pressed.
    void EnsureLabels(const SDL_Renderer *renderer);
    // What this row reads on screen. Baked into the artwork for Classic's
    // seven original rows; drawn from this string for every other theme, and
    // always for MENU STYLE, whose text names the theme in effect.
    std::string LabelText() const;
    void ReleaseLabels();

    bool isActive;
    int sheetLen;
    int curFrame;
    int fixedFrame;
    std::string buttonName;
    std::vector<SDL_Texture*> icons;
    SDL_Rect icon_rect;
    SDL_Texture *backgroundActive;
    SDL_Texture *background;
    SDL_Rect rect;

    SDL_Texture *labelIdle = nullptr;
    SDL_Texture *labelActive = nullptr;
    SDL_Point labelIdleSize{}, labelActiveSize{};
    int cachedTheme = -1;
    std::string cachedLabel;
};

#endif // MENUBUTTON_H
