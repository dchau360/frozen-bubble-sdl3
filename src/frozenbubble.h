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

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
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

    // Delta-time scale: 1.0 at 60 fps, 2.0 at 30 fps, etc.
    // All per-frame movement amounts are multiplied by this so the game
    // runs at the same real-time speed regardless of frame rate.
    float deltaScale = 1.0f;
    void CallMenuReturn() { mainMenu->ReturnToMenu(); };
    void CallNetLobbyReturn() { mainMenu->ReturnToNetLobby(); };
    void CallGameQuit() { IsGameQuit = true; };
    void CallGamePause() { IsGamePause = !IsGamePause; };

    uint8_t RunForEver();
    void RunOneFrame();
    GameState currentState = TitleScreen;

    FrozenBubble(const FrozenBubble& obj) = delete;
    static FrozenBubble* Instance();
    BubbleGame* bubbleGame() { return mainGame; };
private:
    int addictedTime = 0;
    bool IsGameQuit = false, IsGamePause = false;
    Uint32 lastBackPressTime = 0;

    // Per-controller state: joystick instance ID + axis hold flags
    struct ControllerState {
        SDL_JoystickID id;
        bool axisLeftHeld = false, axisRightHeld = false;
        bool axisUpHeld   = false, axisDownHeld  = false;
    };
    std::vector<ControllerState> controllers;

    void HandleControllerEvent(SDL_Event *e);
    static void PushKey(SDL_Keycode key, bool down);
    static void PushScancode(SDL_Scancode sc, bool down, bool skipEvent = false);
    SDL_Window *window;
    SDL_Renderer *renderer;

    GameSettings *gameOptions;
    AudioMixer *audMixer;

    MainMenu *mainMenu;
    BubbleGame *mainGame;

    HighscoreManager *hiscoreManager;

    TTFText menuText;

    void HandleInput(SDL_Event *e);

    // Frame timing — used by RunOneFrame (persist across calls in WASM)
    unsigned int frameTicks = 0;
    unsigned int frameLastTick = 0;
    float frameTime = 1000.0f / 60.0f;

    static FrozenBubble* ptrInstance;
    FrozenBubble();
    ~FrozenBubble();
};

#endif // FROZENBUBBLE_H
