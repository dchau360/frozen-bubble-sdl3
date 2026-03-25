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

#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

#include <SDL2/SDL.h>
#include <iniparser.h>
#include <iostream>
#include <mutex>
#include <string>

struct PlayerKeys {
    SDL_Scancode left, right, fire, center;
};

class GameSettings final
{
public:
    void InitPrefPath();
    void ReadSettings();
    void SaveSettings();
    void SetValue(const char *option, const char *value);
    void GetValue();

    const char *prefPath = nullptr; // Initialized lazily via InitPrefPath() after SDL is ready
    int gfxLevel() { return gfxQuality; }
    SDL_Point curResolution() { return {windowWidth, windowHeight}; }
    bool fullscreenMode() { return useFullscreen; }
    bool linearScaling;
    bool canPlayMusic() { return playMusic; }
    bool canPlaySFX() { return playSfx; }
    bool useClassicAudio() { return classicSound; }
    bool colorBlind() { return colorblindBubbles; }

    PlayerKeys player1Keys, player2Keys, player3Keys, player4Keys, player5Keys;
    void LoadDefaultKeys();
    void SaveKeys();

    GameSettings(const GameSettings& obj) = delete;
    void Dispose();
    static GameSettings* Instance(){
        return ptrInstance;
    };
private:
    void CreateDefaultSettings();
    dictionary *optDict;

    int gfxQuality, windowWidth, windowHeight;
    bool useFullscreen, colorblindBubbles, playMusic, playSfx, classicSound;

    GameSettings(){};
    ~GameSettings();
    static std::mutex mtx;
    static GameSettings* ptrInstance;
};

#endif //GAMESETTINGS_H