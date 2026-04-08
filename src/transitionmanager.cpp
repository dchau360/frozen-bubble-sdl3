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

#include "transitionmanager.h"
#include "shaderstuff.h"

TransitionManager *TransitionManager::ptrInstance = NULL;

TransitionManager *TransitionManager::Instance()
{
    if(ptrInstance == NULL)
        ptrInstance = new TransitionManager();
    return ptrInstance;
}

TransitionManager::TransitionManager()
{
    gameSettings = GameSettings::Instance();
    snapIn = SDL_CreateSurface(640, 480, SURF_FORMAT);
    snapOut = SDL_CreateSurface(640, 480, SURF_FORMAT);
}

TransitionManager::~TransitionManager(){
    SDL_DestroySurface(snapIn);
    SDL_DestroySurface(snapOut);
}

void TransitionManager::Dispose(){
    this->~TransitionManager();
}

void TransitionManager::DoSnipIn(SDL_Renderer *rend)
{
#ifdef __WASM_PORT__
    return;  // Blocking animation loop hangs the browser; skip in WASM
#endif
    if (gameSettings->gfxLevel() > 2) return;
    float w = 0, h = 0;
    SDL_GetRenderScale(rend, &w, &h);
    SDL_Rect dstSize = {0, 0, 640, 480};
    SDL_Surface *sfc = SDL_RenderReadPixels(rend, NULL);
    SDL_BlitSurfaceScaled(sfc, NULL, snapIn, &dstSize, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(sfc);
}

void TransitionManager::TakeSnipOut(SDL_Renderer *rend)
{
#ifdef __WASM_PORT__
    return;  // Blocking animation loop hangs the browser; skip in WASM
#endif
    if (gameSettings->gfxLevel() > 2) return;
    float w = 0, h = 0;
    SDL_GetRenderScale(rend, &w, &h);
    SDL_Rect dstSize = {0, 0, 640, 480};
    SDL_Surface *sfc = SDL_RenderReadPixels(rend, NULL);
    SDL_BlitSurfaceScaled(sfc, NULL, snapOut, &dstSize, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(sfc);
    effect(snapIn, snapOut, rend, transitionTexture);
}

