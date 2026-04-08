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

#ifndef TRANSITIONMANAGER_H
#define TRANSITIONMANAGER_H

#include <SDL3/SDL.h>
#include <iostream>
#include <string>

#include "gamesettings.h"

#define WINDOW_W 640
#define WINDOW_H 480
#define SURF_FORMAT SDL_PIXELFORMAT_ARGB8888

class TransitionManager final
{
public:
    void DoSnipIn(SDL_Renderer *rend);
    void TakeSnipOut(SDL_Renderer *rend);

    TransitionManager(const TransitionManager& obj) = delete;
    void Dispose();
    static TransitionManager* Instance();
private:
    TransitionManager();
    ~TransitionManager();
    static TransitionManager* ptrInstance;
    GameSettings* gameSettings;

    SDL_Surface *snapIn = nullptr, *snapOut = nullptr;
    SDL_Texture *transitionTexture = nullptr;
    bool stopRendering = true;
};

#endif //TRANSITIONMANAGER_H