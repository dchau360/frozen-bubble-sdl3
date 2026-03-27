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

#include "frozenbubble.h"
#include "logger.h"
#include "platform.h"
#include <sys/stat.h>

FrozenBubble *FrozenBubble::ptrInstance = NULL;

// Helper function to verify asset directory exists
bool VerifyAssetDirectory(const char* dataDir) {
    struct stat st;
    if (stat(dataDir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Asset directory not found: %s\n"
            "Please ensure the game is installed correctly and DATA_DIR is set properly.",
            dataDir);
        return false;
    }
    return true;
}

const char *formatTime(int time){
    int h = int(time/3600.0);
    int m = int((time-h*3600)/60.0);
    int s = int((time-h*3600)-(m*60));

    static char fm[128];
    size_t offset = 0;
    fm[0] = '\0';
    if (h > 0) {
        offset += snprintf(fm + offset, 128 - offset, "%dh ", h);
    }
    if (m > 0) {
        if (h > 0) {
            offset += snprintf(fm + offset, 128 - offset, "%02dm ", m);
        } else {
            offset += snprintf(fm + offset, 128 - offset, "%dm ", m);
        }
    }
    if (s > 0) {
        if (m > 0) {
            snprintf(fm + offset, 128 - offset, "%02ds", s);
        } else {
            snprintf(fm + offset, 128 - offset, "%ds", s);
        }
    }
    return fm;
}

FrozenBubble *FrozenBubble::Instance()
{
    if(ptrInstance == NULL)
        ptrInstance = new FrozenBubble();
    return ptrInstance;
}

FrozenBubble::FrozenBubble() {
    // Initialize logger first so all subsequent logs are captured
    // Determine log file name based on existing logs (creator, joiner1-4)
    // Supports up to 5 players total
    const char* logFilename = nullptr;
    struct stat st;

    if (stat("frozen-bubble-creator.log", &st) != 0) {
        // creator.log doesn't exist - this is the creator
        logFilename = "frozen-bubble-creator.log";
    } else if (stat("frozen-bubble-joiner1.log", &st) != 0) {
        // creator.log exists but joiner1.log doesn't - this is joiner1
        logFilename = "frozen-bubble-joiner1.log";
    } else if (stat("frozen-bubble-joiner2.log", &st) != 0) {
        // joiner1.log exists but joiner2.log doesn't - this is joiner2
        logFilename = "frozen-bubble-joiner2.log";
    } else if (stat("frozen-bubble-joiner3.log", &st) != 0) {
        // joiner2.log exists but joiner3.log doesn't - this is joiner3
        logFilename = "frozen-bubble-joiner3.log";
    } else {
        // All other logs exist - this is joiner4 (5th player)
        logFilename = "frozen-bubble-joiner4.log";
    }

    Logger::Initialize(logFilename);

    // Verify asset directory exists before proceeding
#ifndef __ANDROID__
    if (!VerifyAssetDirectory(g_dataDir.c_str())) {
        std::string msg = "Could not find game assets at: " + g_dataDir + "\n\nPlease ensure the game is installed correctly.";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Asset Directory Missing",
            msg.c_str(),
            NULL);
        IsGameQuit = true;
        return;
    }
#endif

    // Init video only — audio requires a user gesture in browsers and is
    // initialized later by AudioMixer. SDL_INIT_AUDIO here would fail on
    // Emscripten and cause the constructor to bail before creating the window.
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init(VIDEO) failed: %s", SDL_GetError());
        IsGameQuit = true;
        return;
    }

    gameOptions = GameSettings::Instance();
    gameOptions->ReadSettings();

    SDL_Point resolution = gameOptions->curResolution();
#ifdef __WASM_PORT__
    // Fullscreen via SDL window flags causes a black canvas in browsers.
    // The CSS shell scales the 640x480 canvas to fit the viewport instead.
    Uint32 fullscreen = 0;
#else
    Uint32 fullscreen = gameOptions->fullscreenMode() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
#endif

    window = SDL_CreateWindow("Frozen-Bubble: SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, resolution.x, resolution.y, fullscreen);
    if(gameOptions->linearScaling) SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

    if(!window) {
        IsGameQuit = true;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s", SDL_GetError());
    }

    SDL_Surface *icon = SDL_LoadBMP(ASSET("/gfx/pinguins/window_icon_penguin.bmp").c_str());
    if (icon) {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load window icon: %s", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, 640, 480);

    if(!renderer) {
        IsGameQuit = true;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create renderer: %s", SDL_GetError());
    }

    if( TTF_Init() == -1 )
    {
        IsGameQuit = true;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL_ttf: %s", SDL_GetError());
    }

    audMixer = AudioMixer::Instance();
    hiscoreManager = HighscoreManager::Instance(renderer);

    init_effects((char*)g_dataDir.c_str());
    mainMenu = new MainMenu(renderer);
    mainGame = new BubbleGame(renderer);

    // Initialize game controller support.
    // SDL_INIT_JOYSTICK is needed explicitly on Emscripten (browser Gamepad API)
    // before SDL_INIT_GAMECONTROLLER can detect devices.
    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *gc = SDL_GameControllerOpen(i);
            if (gc) {
                SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
                ControllerState cs;
                cs.id = id;
                controllers.push_back(cs);
                SDL_Log("Opened controller %d: %s", (int)controllers.size(), SDL_GameControllerName(gc));
            }
        }
    }
}

FrozenBubble::~FrozenBubble() {
    if(renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if(window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    hiscoreManager->Dispose();
    audMixer->Dispose();
    gameOptions->Dispose();

    TTF_Quit();
    IMG_Quit();
    Mix_Quit();
    SDL_Quit();

    // Shutdown logger last to capture all cleanup logs
    Logger::Shutdown();
}

#ifdef __WASM_PORT__
#include <emscripten.h>
static void wasm_one_frame() {
    FrozenBubble* fb = FrozenBubble::Instance();
    if (fb) fb->RunOneFrame();
}
#endif

uint8_t FrozenBubble::RunForEver()
{
    if(currentState == TitleScreen) audMixer->PlayMusic("intro");

    frameLastTick = SDL_GetTicks();
    frameTicks    = frameLastTick;

    SDL_Log("RunForEver: starting loop");

#ifdef __WASM_PORT__
    // Emscripten: hand control back to the browser and call RunOneFrame each tick.
    // fps=0 uses requestAnimationFrame (matches display refresh rate).
    emscripten_set_main_loop(wasm_one_frame, 0, 0);
    return 0;
#else
    while(!IsGameQuit) {
        RunOneFrame();
    }
    if (startTime != 0) addictedTime += SDL_GetTicks() - startTime;
    if(addictedTime != 0) printf("Addicted for %s, %d bubbles were launched.", formatTime(addictedTime/1000), totalBubbles);
    this->~FrozenBubble();
    return 0;
#endif
}

void FrozenBubble::RunOneFrame()
{
    frameLastTick = frameTicks;
    frameTicks    = SDL_GetTicks();
    float elapsed = (float)(frameTicks - frameLastTick);

    // handle input
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT || e.type == SDL_APP_TERMINATING ||
            (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) ||
            (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Quit-triggering event: type=0x%x winev=%d key=%d",
                e.type, e.window.event, e.key.keysym.sym);
        }
        if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP ||
            e.type == SDL_CONTROLLERAXISMOTION  || e.type == SDL_CONTROLLERDEVICEADDED ||
            e.type == SDL_JOYDEVICEADDED) {
            HandleControllerEvent(&e);
            continue;
        }
        HandleInput(&e);
    }

    // render
    if(!IsGamePause) {
        SDL_RenderClear(renderer);
        if (currentState == TitleScreen) mainMenu->Render();
        else if (currentState == MainGame) mainGame->Render();
        else if (currentState == Highscores) {
            if (hiscoreManager->lastState == 1) mainGame->Render();
            hiscoreManager->RenderScoreScreen();
        }
        SDL_RenderPresent(renderer);
    } else {
        if (currentState == MainGame) {
            mainGame->RenderPaused();
            SDL_RenderPresent(renderer);
        }
    }

#ifndef __WASM_PORT__
    // On native, cap frame rate manually. In WASM the browser's
    // requestAnimationFrame already limits to the display refresh rate.
    if (elapsed < frameTime) {
        SDL_Delay((Uint32)(frameTime - elapsed));
    }
#endif
}

void FrozenBubble::PushKey(SDL_Keycode key, bool down) {
    SDL_Event ev{};
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.sym = key;
    ev.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    SDL_PushEvent(&ev);
}

void FrozenBubble::PushScancode(SDL_Scancode sc, bool down, bool skipEvent) {
    if (IsVirtualScancode(sc)) {
        virtualKeyState[sc - CTRL_SC_BASE] = down;
        // In-game callers pass skipEvent=true: virtualKeyState is enough, no SDL event needed.
        // Key-binding menu callers pass skipEvent=false so the menu captures the scancode.
        if (skipEvent) return;
    }
    SDL_Event ev{};
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.scancode = sc;
    ev.key.keysym.sym = SDL_GetKeyFromScancode(sc);
    SDL_PushEvent(&ev);
}

void FrozenBubble::HandleControllerEvent(SDL_Event *e) {
    // Hot-plug: open newly connected controllers and assign to next player slot.
    // SDL_JOYDEVICEADDED is a fallback for Emscripten/browser where the Gamepad API
    // may fire the joystick event before the controller mapping is resolved.
    if (e->type == SDL_CONTROLLERDEVICEADDED || e->type == SDL_JOYDEVICEADDED) {
        int idx = (e->type == SDL_CONTROLLERDEVICEADDED) ? e->cdevice.which : e->jdevice.which;
        if (!SDL_IsGameController(idx)) return;
        SDL_GameController *gc = SDL_GameControllerOpen(idx);
        if (gc) {
            SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
            // Only add if not already tracked
            bool found = false;
            for (auto& cs : controllers) if (cs.id == id) { found = true; break; }
            if (!found) {
                ControllerState cs;
                cs.id = id;
                controllers.push_back(cs);
                SDL_Log("Controller connected: %s → player %d", SDL_GameControllerName(gc), (int)controllers.size());
            }
        }
        return;
    }

    // Find the ControllerState for this event's joystick ID
    SDL_JoystickID evId = (e->type == SDL_CONTROLLERAXISMOTION) ? e->caxis.which : e->cbutton.which;
    ControllerState *cs = nullptr;
    int playerIdx = 0;
    for (int i = 0; i < (int)controllers.size(); i++) {
        if (controllers[i].id == evId) { cs = &controllers[i]; playerIdx = i; break; }
    }
    if (!cs) return;

    // Get this player's key bindings
    GameSettings* gs = GameSettings::Instance();
    PlayerKeys* allKeys[] = {
        &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
        &gs->player4Keys, &gs->player5Keys
    };
    PlayerKeys* pk = allKeys[playerIdx < 5 ? playerIdx : 0];

    if (e->type == SDL_CONTROLLERBUTTONDOWN || e->type == SDL_CONTROLLERBUTTONUP) {
        bool down = (e->type == SDL_CONTROLLERBUTTONDOWN);

        // If the Keys panel is waiting for a binding, emit a virtual scancode
        if (down && mainMenu->IsAwaitingKeyBind()) {
            SDL_Scancode vsc = (SDL_Scancode)(300 + playerIdx * 20 + e->cbutton.button);
            PushScancode(vsc, true);
            return;
        }

        // Outside of an active game, always use standard nav keys so menus work.
        // In-game: push virtual scancode into virtualKeyState[] so IsKeyPressed() works.
        // controllerInputs[] approach was wrong: playerIdx (controller slot) != playerAssigned.
        bool inGame = (currentState == MainGame);
        if (inGame) {
            int btn = e->cbutton.button;
            if (btn == SDL_CONTROLLER_BUTTON_B)     { PushKey(SDLK_AC_BACK, down); }
            else if (btn == SDL_CONTROLLER_BUTTON_START) { PushKey(SDLK_PAUSE, down); }
            else {
                SDL_Scancode vsc = (SDL_Scancode)(CTRL_SC_BASE + playerIdx * 20 + btn);
                PushScancode(vsc, down, true);
            }
        } else {
            switch (e->cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  PushKey(SDLK_LEFT,   down); break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: PushKey(SDLK_RIGHT,  down); break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:    PushKey(SDLK_UP,     down); break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  PushKey(SDLK_DOWN,   down); break;
                case SDL_CONTROLLER_BUTTON_A:          PushKey(SDLK_RETURN, down); break;
                case SDL_CONTROLLER_BUTTON_B:          PushKey(SDLK_AC_BACK, down); break;
                case SDL_CONTROLLER_BUTTON_START:      PushKey(SDLK_PAUSE,   down); break;
                default: break;
            }
        }
    }

    if (e->type == SDL_CONTROLLERAXISMOTION) {
        const Sint16 DEAD = 8000;
        Sint16 val = e->caxis.value;

        bool inGame = (currentState == MainGame);
        if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
            bool wantLeft  = val < -DEAD;
            bool wantRight = val >  DEAD;
            if (inGame) {
                SDL_Scancode scLeft  = (SDL_Scancode)(CTRL_SC_BASE + playerIdx * 20 + SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                SDL_Scancode scRight = (SDL_Scancode)(CTRL_SC_BASE + playerIdx * 20 + SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                if (wantLeft  != cs->axisLeftHeld)  { PushScancode(scLeft,  wantLeft,  true); cs->axisLeftHeld  = wantLeft;  }
                if (wantRight != cs->axisRightHeld) { PushScancode(scRight, wantRight, true); cs->axisRightHeld = wantRight; }
            } else {
                SDL_Scancode scLeft  = mainMenu->IsAwaitingKeyBind()
                    ? (SDL_Scancode)(300 + playerIdx * 20 + SDL_CONTROLLER_BUTTON_DPAD_LEFT)
                    : SDL_SCANCODE_LEFT;
                SDL_Scancode scRight = mainMenu->IsAwaitingKeyBind()
                    ? (SDL_Scancode)(300 + playerIdx * 20 + SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                    : SDL_SCANCODE_RIGHT;
                if (wantLeft  && !cs->axisLeftHeld)  { PushScancode(scLeft,  true);  cs->axisLeftHeld  = true;  }
                if (!wantLeft &&  cs->axisLeftHeld)  { PushScancode(scLeft,  false); cs->axisLeftHeld  = false; }
                if (wantRight && !cs->axisRightHeld) { PushScancode(scRight, true);  cs->axisRightHeld = true;  }
                if (!wantRight && cs->axisRightHeld) { PushScancode(scRight, false); cs->axisRightHeld = false; }
            }
        }
        if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            bool wantUp   = val < -DEAD;
            bool wantDown = val >  DEAD;
            if (inGame) {
                SDL_Scancode scUp   = (SDL_Scancode)(CTRL_SC_BASE + playerIdx * 20 + SDL_CONTROLLER_BUTTON_DPAD_UP);
                SDL_Scancode scDown = (SDL_Scancode)(CTRL_SC_BASE + playerIdx * 20 + SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                if (wantUp   != cs->axisUpHeld)   { PushScancode(scUp,   wantUp,   true); cs->axisUpHeld   = wantUp;   }
                if (wantDown != cs->axisDownHeld)  { PushScancode(scDown, wantDown, true); cs->axisDownHeld = wantDown; }
            } else {
                SDL_Scancode scUp   = mainMenu->IsAwaitingKeyBind()
                    ? (SDL_Scancode)(300 + playerIdx * 20 + SDL_CONTROLLER_BUTTON_DPAD_UP)
                    : SDL_SCANCODE_UP;
                SDL_Scancode scDown = mainMenu->IsAwaitingKeyBind()
                    ? (SDL_Scancode)(300 + playerIdx * 20 + SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                    : SDL_SCANCODE_DOWN;
                if (wantUp   && !cs->axisUpHeld)   { PushScancode(scUp,   true);  cs->axisUpHeld   = true;  }
                if (!wantUp  &&  cs->axisUpHeld)   { PushScancode(scUp,   false); cs->axisUpHeld   = false; }
                if (wantDown && !cs->axisDownHeld) { PushScancode(scDown, true);  cs->axisDownHeld = true;  }
                if (!wantDown && cs->axisDownHeld) { PushScancode(scDown, false); cs->axisDownHeld = false; }
            }
        }
    }
}

void FrozenBubble::HandleInput(SDL_Event *e) {
    switch(e->type) {
        case SDL_WINDOWEVENT:
            switch (e->window.event) {
                case SDL_WINDOWEVENT_CLOSE:
                {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_WINDOWEVENT_CLOSE received");
#ifndef __ANDROID__
                    // On Android, onDestroy/nativeSendQuit → SDL_QUIT is the correct
                    // exit path. WINDOWEVENT_CLOSE can fire spuriously during surface
                    // lifecycle events and must not quit the game.
                    IsGameQuit = true;
#endif
                    break;
                }
            }
            break;
        case SDL_QUIT:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_QUIT received");
            IsGameQuit = true;
            break;
        case SDL_KEYDOWN:
            if(e->key.repeat) break;
            switch(e->key.keysym.sym) {
                case SDLK_AC_BACK:
                case SDLK_ESCAPE:
                {
                    // Only trigger double-back-to-quit when at the root menu (no panels open)
                    if (currentState == TitleScreen && !mainMenu->HasAnyPanelOpen()) {
                        Uint32 now = SDL_GetTicks();
                        if (now - lastBackPressTime < 2000) {
                            IsGameQuit = true;
                        }
                        lastBackPressTime = now;
                    }
                    // Always let mainMenu/mainGame handle it too (closes panels, cancels, etc.)
                    break;
                }
                case SDLK_F12:
                    gameOptions->SetValue("GFX:Fullscreen", "");
                    SDL_SetWindowFullscreen(window, gameOptions->fullscreenMode() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    break;
                case SDLK_PAUSE:
                    CallGamePause();
                    if (currentState == MainGame) {
                        if (!mainGame->playedPause) mainGame->playedPause = false;
                    }
                    break;
            }
            break;
    }

    if (IsGamePause) return;
    if(currentState == Highscores) {
        hiscoreManager->HandleInput(e);
        return;
    }
    if(currentState == TitleScreen) mainMenu->HandleInput(e);
    if(currentState == MainGame) mainGame->HandleInput(e);
}
