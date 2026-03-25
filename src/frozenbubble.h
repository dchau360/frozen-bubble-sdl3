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

#ifndef FROZENBUBBLE_H
#define FROZENBUBBLE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>
#include <string>
#include <iostream>

#include "gamesettings.h"
#include "audiomixer.h"

#include "highscoremanager.h"

#include "mainmenu.h"
#include "bubblegame.h"

#include "ttftext.h"

enum GameState {
    TitleScreen = 0,
    MainGame = 1,
    LevelEditor = 2,
    Netplay = 3,
    Highscores = 4
};

class FrozenBubble
{
public:
    int totalBubbles = 0;
    Uint32 startTime = 0;
    void CallMenuReturn() { mainMenu->ReturnToMenu(); };
    void CallNetLobbyReturn() { mainMenu->ReturnToNetLobby(); };
    void CallGameQuit() { IsGameQuit = true; };
    void CallGamePause() { IsGamePause = !IsGamePause; };

    uint8_t RunForEver();
    GameState currentState = TitleScreen;

    FrozenBubble(const FrozenBubble& obj) = delete;
    static FrozenBubble* Instance();
    BubbleGame* bubbleGame() { return mainGame; };
private:
    int addictedTime = 0;
    bool IsGameQuit = false, IsGamePause = false;
    SDL_Window *window;
    SDL_Renderer *renderer;

    GameSettings *gameOptions;
    AudioMixer *audMixer;

    MainMenu *mainMenu;
    BubbleGame *mainGame;

    HighscoreManager *hiscoreManager;

    TTFText menuText;

    void HandleInput(SDL_Event *e);

    static FrozenBubble* ptrInstance;
    FrozenBubble();
    ~FrozenBubble();
};

#endif // FROZENBUBBLE_H
