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

#include "mainmenu.h"
#include "audiomixer.h"
#include "frozenbubble.h"
#include "transitionmanager.h"
#include "networkclient.h"
#include "platform.h"

#include <SDL2/SDL_image.h>
#include <cstring>
#include <cmath>
#include <errno.h>
#include <thread>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include "socket_compat.h"
#ifndef _WIN32
#ifndef __WASM_PORT__
#include <netdb.h>
#endif
#endif

// Virtual scancodes for controller buttons: CTRL_SC_BASE + controllerSlot*20 + SDL_GameControllerButton
static SDL_Scancode ControllerButtonScancode(int controllerSlot, SDL_GameControllerButton btn) {
    return (SDL_Scancode)(CTRL_SC_BASE + controllerSlot * 20 + (int)btn);
}
static std::string ControllerScancodeName(SDL_Scancode sc) {
    if (!IsVirtualScancode(sc)) return SDL_GetScancodeName(sc);
    int rel = sc - CTRL_SC_BASE;
    int slot = rel / 20;
    int btn  = rel % 20;
    const char* btnNames[] = {"A","B","X","Y","Back","Guide","Start",
                               "LS","RS","LB","RB","DUp","DDown","DLeft","DRight"};
    char buf[32];
    snprintf(buf, sizeof(buf), "Ctrl%d:%s", slot + 1, btn < 15 ? btnNames[btn] : "?");
    return buf;
}

// Returns true if something is already listening on localhost:port
static bool portInUse(int port) {
#ifdef _WIN32
    return false; // Server auto-start not supported on Windows
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    connect(s, (struct sockaddr*)&addr, sizeof(addr));
    fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
    struct timeval tv{0, 200000}; // 200ms
    bool inUse = false;
    if (select(s + 1, nullptr, &wfds, nullptr, &tv) > 0) {
        int err = 0;
        socklen_t errLen = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &errLen);
        inUse = (err == 0); // ECONNREFUSED also triggers select; only success has err==0
    }
    SOCKET_CLOSE(s);
    return inUse;
#endif
}

inline int ranrange(int a, int b) { return a + rand() % ((b - a ) + 1); }

struct ButtonId {
    std::string buttonName;
    std::string iconName;
    int iconFrames;
};

SDL_Point GetSize(SDL_Texture *texture){
    SDL_Point size;
    SDL_QueryTexture(texture, NULL, NULL, &size.x, &size.y);
    return size;
}

MainMenu::MainMenu(const SDL_Renderer *renderer)
    : renderer(renderer), active_button_index(0)
{
    const ButtonId texts[] = {
        {"1pgame", "1pgame", 30}, 
        {"2pgame", "p1p2", 30}, 
        {"langame", "langame", 70}, 
        {"netgame", "netgame", 89}, 
        {"editor", "editor", 67}, 
        {"graphics", "graphics", 30}, 
        {"keys", "keys", 80}, 
        {"highscores", "highscore", 89}
    };
    uint32_t y_start = 14;
    for(size_t i = 0; i < std::size(texts); i++) {
        buttons.push_back(MenuButton(89, y_start, texts[i].buttonName, renderer, texts[i].iconName, texts[i].iconFrames));
        y_start += 56u;
    }

    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);

    background = IMG_LoadTexture(rend, ASSET("/gfx/menu/back_start.png").c_str());
    fbLogo = IMG_LoadTexture(rend, ASSET("/gfx/menu/fblogo.png").c_str());
    fb_logo_rect.x = 400;
    fb_logo_rect.y = 15;
    fb_logo_rect.w = 190;
    fb_logo_rect.h = 119;
    candy_fb_rect = SDL_Rect(fb_logo_rect);

    bannerArtwork = IMG_LoadTexture(rend, ASSET("/gfx/menu/banner_artwork.png").c_str());
    bannerCPU = IMG_LoadTexture(rend, ASSET("/gfx/menu/banner_cpucontrol.png").c_str());
    bannerSound = IMG_LoadTexture(rend, ASSET("/gfx/menu/banner_soundtrack.png").c_str());
    bannerLevel = IMG_LoadTexture(rend, ASSET("/gfx/menu/banner_leveleditor.png").c_str());

    bannerFormulas[0] = BANNER_START;
    bannerFormulas[1] = BANNER_START + GetSize(bannerArtwork).x + BANNER_SPACING;
    bannerFormulas[2] = BANNER_START + GetSize(bannerArtwork).x + BANNER_SPACING
                        + GetSize(bannerSound).x + BANNER_SPACING;
    bannerFormulas[3] = BANNER_START + GetSize(bannerArtwork).x + BANNER_SPACING
                        + GetSize(bannerSound).x + BANNER_SPACING
                        + GetSize(bannerCPU).x + BANNER_SPACING;

    bannerMax = bannerFormulas[3] - (640 - (BANNER_MAXX - BANNER_MINX)) + BANNER_SPACING;
    banner_rect = {BANNER_MINX, BANNER_Y, (BANNER_MAXX - BANNER_MINX), 30};

    blinkGreenL = IMG_LoadTexture(rend, ASSET("/gfx/menu/backgrnd-closedeye-left-green.png").c_str());
    blinkGreenR = IMG_LoadTexture(rend, ASSET("/gfx/menu/backgrnd-closedeye-right-green.png").c_str());
    blink_green_left = {411, 385, GetSize(blinkGreenL).x, GetSize(blinkGreenL).y};
    blink_green_right = {434, 378, GetSize(blinkGreenR).x, GetSize(blinkGreenR).y};

    blinkPurpleL = IMG_LoadTexture(rend, ASSET("/gfx/menu/backgrnd-closedeye-left-purple.png").c_str());
    blinkPurpleR = IMG_LoadTexture(rend, ASSET("/gfx/menu/backgrnd-closedeye-right-purple.png").c_str());
    blink_purple_left = {522, 356, GetSize(blinkPurpleL).x, GetSize(blinkPurpleL).y};
    blink_purple_right = {535, 356, GetSize(blinkPurpleR).x, GetSize(blinkPurpleR).y};

    for (int i = 0; i < SP_OPT; i++) {
        std::string idlePath = ASSET("/gfx/menu/txt_") + spOptions[i].option + "_outlined_text.png";
        std::string activePath = ASSET("/gfx/menu/txt_") + spOptions[i].option + "_text.png";
        idleSPButtons[i] = IMG_LoadTexture(rend, idlePath.c_str());
        activeSPButtons[i] = IMG_Load(activePath.c_str());
    }
    singlePanelBG = IMG_LoadTexture(rend, ASSET("/gfx/menu/1p_panel.png").c_str());
    singleButtonAct = IMG_LoadTexture(rend, ASSET("/gfx/menu/txt_menu_1p_over.png").c_str());
    singleButtonIdle = IMG_LoadTexture(rend, ASSET("/gfx/menu/txt_menu_1p_off.png").c_str());

    voidPanelBG = IMG_LoadTexture(rend, ASSET("/gfx/menu/void_panel.png").c_str());

    InitCandy();

    buttons[active_button_index].Activate();

    panelText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 15);
    panelText.UpdateAlignment(TTF_WRAPPED_ALIGN_CENTER);
    panelText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    networkText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    networkText.UpdateAlignment(TTF_WRAPPED_ALIGN_LEFT);
    networkText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    // Load network lobby graphics
    netGameBackground = IMG_LoadTexture(rend, ASSET("/gfx/back_netgame.png").c_str());
    netSpotFree = IMG_LoadTexture(rend, ASSET("/gfx/netspot.png").c_str());
    netSpotInGame = IMG_LoadTexture(rend, ASSET("/gfx/netspot-insamegame.png").c_str());
    netSpotPlaying = IMG_LoadTexture(rend, ASSET("/gfx/netspot-playing.png").c_str());
    highlightServer = IMG_LoadTexture(rend, ASSET("/gfx/menu/highlight-server.png").c_str());

    // Load animated self spot (13 frames: 1-9, A-D)
    const char* selfSpotFrames[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D"};
    for (int i = 0; i < 13; i++) {
        char rel[64];
        snprintf(rel, sizeof(rel), "/gfx/netspot-self-%s.png", selfSpotFrames[i]);
        netSpotSelf[i] = IMG_LoadTexture(rend, ASSET(rel).c_str());
    }
}

MainMenu::~MainMenu() {
#ifndef __WASM_PORT__
    if (serverFetchThread.joinable()) serverFetchThread.join();
#endif
    SDL_DestroyTexture(background);
    SDL_DestroyTexture(fbLogo);
    buttons.clear();
}

void MainMenu::InitCandy() {
    candyOrig.LoadTextureData(const_cast<SDL_Renderer*>(renderer), ASSET("/gfx/menu/fblogo.png").c_str());
    if(candyMethod == -1) candyMethod = ranrange(0, 8);
    else {
        int a = ranrange(0, 8);
        while (a == candyMethod) a = ranrange(0, 8);
        candyMethod = a;
    }

    SDL_Rect tmpRct;
    if (candyMethod == 3) { // stretch
        candy_fb_rect.x -= (int)(fb_logo_rect.w * 0.05);
        candy_fb_rect.y -= (int)(fb_logo_rect.h * 0.05);
        tmpRct = {(int)(fb_logo_rect.w * 0.05), (int)(fb_logo_rect.h * 0.05), (int)(fb_logo_rect.w * 1.1), (int)(fb_logo_rect.h * 1.1)};
        candyModif.LoadEmptyAndApply(&tmpRct, const_cast<SDL_Renderer*>(renderer), ASSET("/gfx/menu/fblogo.png").c_str());
        SDL_FreeSurface(candyOrig.sfc);
        candyOrig.LoadFromSurface(candyModif.sfc, const_cast<SDL_Renderer*>(renderer));
        candy_fb_rect.w = candyOrig.sfc->w;
        candy_fb_rect.h = candyOrig.sfc->h;
    }
    else if (candyMethod == 4) { // tilt
        candy_fb_rect.x -= (int)(fb_logo_rect.w * 0.05);
        candy_fb_rect.y -= (int)(fb_logo_rect.h * 0.025);
        tmpRct = {(int)(fb_logo_rect.w * 0.05), (int)(fb_logo_rect.h * 0.025), (int)(fb_logo_rect.w * 1.1), (int)(fb_logo_rect.h * 1.05)};
        candyModif.LoadEmptyAndApply(&tmpRct, const_cast<SDL_Renderer*>(renderer), ASSET("/gfx/menu/fblogo.png").c_str());
        SDL_FreeSurface(candyOrig.sfc);
        candyOrig.LoadFromSurface(candyModif.sfc, const_cast<SDL_Renderer*>(renderer));
        candy_fb_rect.w = candyOrig.sfc->w;
        candy_fb_rect.h = candyOrig.sfc->h;
    }
    else if (candyMethod == 5) {
        candyModif.LoadTextureData(const_cast<SDL_Renderer*>(renderer), ASSET("/gfx/menu/fblogo.png").c_str());
        logoMask.LoadTextureData(const_cast<SDL_Renderer*>(renderer), ASSET("/gfx/menu/fblogo-mask.png").c_str()); // points
    }
    else if (candyMethod == 8) { //snow
        candy_fb_rect.x -= (int)(fb_logo_rect.w * 0.05);
        tmpRct = {(int)(fb_logo_rect.w * 0.05), candy_fb_rect.y, (int)(fb_logo_rect.w * 1.1), fb_logo_rect.h + candy_fb_rect.y};
        candyModif.LoadEmptyAndApply(&tmpRct, const_cast<SDL_Renderer*>(renderer), ASSET("/gfx/menu/fblogo.png").c_str());
        SDL_FreeSurface(candyOrig.sfc);
        candyOrig.LoadFromSurface(candyModif.sfc, const_cast<SDL_Renderer*>(renderer));
        candy_fb_rect.y = 0;
        candy_fb_rect.w = candyOrig.sfc->w;
        candy_fb_rect.h = candyOrig.sfc->h;
    }
    else candyModif.LoadTextureData(const_cast<SDL_Renderer*>(renderer), ASSET("/gfx/menu/fblogo.png").c_str());

    candyInit = true;
}

void MainMenu::RefreshCandy(){
    candy_fb_rect = SDL_Rect(fb_logo_rect);
    InitCandy();
}

void MainMenu::HandleInput(SDL_Event *e){
    // Map gamepad/D-pad to keyboard-equivalent actions for TV remotes
    if (e->type == SDL_CONTROLLERBUTTONDOWN) {
        SDL_KeyboardEvent fake{};
        fake.type = SDL_KEYDOWN;
        fake.state = SDL_PRESSED;
        switch (e->cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    fake.keysym.sym = SDLK_UP; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  fake.keysym.sym = SDLK_DOWN; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  fake.keysym.sym = SDLK_LEFT; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: fake.keysym.sym = SDLK_RIGHT; break;
            case SDL_CONTROLLER_BUTTON_A:          fake.keysym.sym = SDLK_RETURN; break;
            case SDL_CONTROLLER_BUTTON_B:          fake.keysym.sym = SDLK_ESCAPE; break;
            case SDL_CONTROLLER_BUTTON_START:      fake.keysym.sym = SDLK_RETURN; break;
            case SDL_CONTROLLER_BUTTON_X:          fake.keysym.sym = SDLK_t; break; // X=Chat
            case SDL_CONTROLLER_BUTTON_Y:          fake.keysym.sym = SDLK_r; break; // Y=Remove Ads (Android opts panel)
            default: return;
        }
        SDL_Event fakeEvent;
        fakeEvent.type = SDL_KEYDOWN;
        fakeEvent.key = fake;
        HandleInput(&fakeEvent);
        return;
    }

    switch(e->type) {
        case SDL_TEXTINPUT:
            // Handle virtual keyboard character input for host field (mode 8, only when actively editing)
            if (showingNetPanel && !networkInLobby && networkInputMode == 8 && networkFieldEditing) {
                size_t len = strlen(networkHost);
                for (const char* p = e->text.text; *p; p++) {
                    char c = *p;
                    if (c == '\b') {
                        // Android virtual keyboard sends backspace as '\b' in TEXTINPUT
                        if (len > 0) { networkHost[--len] = '\0'; }
                    } else if (len < 255 && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == ':')) {
                        networkHost[len++] = c;
                        networkHost[len] = '\0';
                    }
                }
            }
            // Handle virtual keyboard character input for nickname field (mode 11)
            if (networkInputMode == 11) {
                size_t len = strlen(networkPreNick);
                for (const char* p = e->text.text; *p; p++) {
                    char c = *p;
                    if (c == '\b') {
                        if (len > 0) { networkPreNick[--len] = '\0'; }
                    } else if (len < 15) {
                        networkPreNick[len++] = c;
                        networkPreNick[len] = '\0';
                    }
                }
            }
            // Handle virtual keyboard character input for chat (mode 4)
            if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                size_t len = strlen(networkChatInput);
                for (const char* p = e->text.text; *p; p++) {
                    char c = *p;
                    if (c == '\b') {
                        if (len > 0) { networkChatInput[--len] = '\0'; }
                    } else if (len < 255) {
                        networkChatInput[len++] = c;
                        networkChatInput[len] = '\0';
                    }
                }
            }
            // Handle virtual keyboard character input for username (mode 5)
            if (showingNetPanel && networkInLobby && networkInputMode == 5) {
                size_t len = strlen(networkUsername);
                for (const char* p = e->text.text; *p; p++) {
                    char c = *p;
                    if (c == '\b') {
                        if (len > 0) { networkUsername[--len] = '\0'; }
                    } else if (len < 31) {
                        networkUsername[len++] = c;
                        networkUsername[len] = '\0';
                    }
                }
            }
            break;
        case SDL_KEYDOWN:
            // Handle backspace/delete in text fields before the repeat filter,
            // because Android's IME may send backspace with repeat=1 on a single press.
            if (e->key.keysym.sym == SDLK_BACKSPACE || e->key.keysym.sym == SDLK_DELETE) {
                if (showingNetPanel && !networkInLobby && networkInputMode == 8 && networkFieldEditing) {
                    size_t len = strlen(networkHost);
                    if (len > 0) networkHost[len - 1] = '\0';
                    break;
                }
                if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                    size_t len = strlen(networkPreNick);
                    if (len > 0) networkPreNick[len - 1] = '\0';
                    break;
                }
            }
            if(e->key.repeat) break;

            // Handle network panel text input
            if (showingNetPanel && networkInLobby && networkInputMode == 3) {
                // Join creator name input
                if ((e->key.keysym.sym >= SDLK_a && e->key.keysym.sym <= SDLK_z) ||
                    (e->key.keysym.sym >= SDLK_0 && e->key.keysym.sym <= SDLK_9)) {
                    size_t len = strlen(networkJoinCreator);
                    if (len < 31) {
                        if (e->key.keysym.sym >= SDLK_a && e->key.keysym.sym <= SDLK_z) {
                            networkJoinCreator[len] = 'a' + (e->key.keysym.sym - SDLK_a);
                        } else {
                            networkJoinCreator[len] = '0' + (e->key.keysym.sym - SDLK_0);
                        }
                        networkJoinCreator[len + 1] = '\0';
                    }
                    break;
                } else if (e->key.keysym.sym == SDLK_BACKSPACE) {
                    size_t len = strlen(networkJoinCreator);
                    if (len > 0) networkJoinCreator[len - 1] = '\0';
                    break;
                }
            } else if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                // Chat input - characters handled by SDL_TEXTINPUT
                if (e->key.keysym.sym == SDLK_BACKSPACE) {
                    size_t len = strlen(networkChatInput);
                    if (len > 0) networkChatInput[len - 1] = '\0';
                    break;
                }
            } else if (showingNetPanel && networkInLobby && networkInputMode == 5) {
                // Username input - characters handled by SDL_TEXTINPUT
                if (e->key.keysym.sym == SDLK_BACKSPACE) {
                    size_t len = strlen(networkUsername);
                    if (len > 0) networkUsername[len - 1] = '\0';
                    break;
                }
            } else if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                // Pre-lobby nickname input - characters handled by SDL_TEXTINPUT
                if (e->key.keysym.sym == SDLK_BACKSPACE) {
                    size_t len = strlen(networkPreNick);
                    if (len > 0) networkPreNick[len - 1] = '\0';
                    break;
                }
            }

            // Handle backspace in level panel
            if (showingLevelPanel && !runDelay) {
                if (e->key.keysym.sym == SDLK_BACKSPACE) {
                    if (!levelInput.empty()) levelInput.pop_back();
                    break;
                }
            }

            // Handle backspace in chat input when Chat is selected in lobby
            if (showingNetPanel && networkInLobby && networkInputMode == 0 && selectedActionIndex == 0) {
                if (e->key.keysym.sym == SDLK_BACKSPACE) {
                    size_t len = strlen(networkChatInput);
                    if (len > 0) networkChatInput[len - 1] = '\0';
                    break;
                }
            }

            // Digit input for level selection panel
            if (showingLevelPanel && !runDelay) {
                if (e->key.keysym.sym >= SDLK_0 && e->key.keysym.sym <= SDLK_9) {
                    if (levelInput.size() < 3) {
                        levelInput += (char)('0' + (e->key.keysym.sym - SDLK_0));
                    }
                    break;
                }
            }

            if (showingNetPanel && !networkInLobby && (networkInputMode == 8 || networkInputMode == 9)) {
                // Manual entry form: host (index 0), port (index 1), connect (index 2)
                if (networkFieldEditing) {
                    // Keyboard is open — handle field-specific input
                    if (networkManualFieldIndex == 0) {
                        // Host field editing
                        connectErrorMsg.clear();
                        size_t len = strlen(networkHost);
                        if (e->key.keysym.sym == SDLK_BACKSPACE || e->key.keysym.sym == SDLK_DELETE) {
                            if (len > 0) networkHost[len - 1] = '\0';
                            break;
                        }
                    } else if (networkManualFieldIndex == 1) {
                        // Port field editing
                        if (e->key.keysym.sym >= SDLK_0 && e->key.keysym.sym <= SDLK_9) {
                            if (networkPort < 6553) networkPort = networkPort * 10 + (e->key.keysym.sym - SDLK_0);
                            break;
                        } else if (e->key.keysym.sym == SDLK_BACKSPACE) {
                            networkPort /= 10;
                            break;
                        }
                    }
                } else {
                    // Not editing: UP/DOWN cycles through host, port, connect
                    if (e->key.keysym.sym == SDLK_DOWN) {
                        networkManualFieldIndex = (networkManualFieldIndex + 1) % 3;
                        networkInputMode = (networkManualFieldIndex == 1) ? 9 : 8;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.keysym.sym == SDLK_UP) {
                        networkManualFieldIndex = (networkManualFieldIndex + 2) % 3;
                        networkInputMode = (networkManualFieldIndex == 1) ? 9 : 8;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    }
                }
            }

#ifdef __ANDROID__
            // On Android: pressing 'R' in any options panel triggers "Remove Ads" IAP
            if (awaitKp && (showingOptPanel || showingNetSetupPanel) &&
                e->key.keysym.sym == SDLK_r) {
                SDL_AndroidSendMessage(0x8002, 0); // launch Remove Ads purchase flow
                break;
            }
#endif

            if (awaitKp && (showingOptPanel || showingNetSetupPanel) && e->key.keysym.sym != SDLK_ESCAPE) {
                AudioMixer::Instance()->PlaySFX("typewriter");
                lastOptInput = e->key.keysym.sym;
                awaitKp = false;
                break;
            }

            // Keys configuration input handling
            if (showingKeysPanel) {
                if (awaitKp && e->key.keysym.sym != SDLK_ESCAPE) {
                    // Set the key for the current player/index
                    GameSettings* gs = GameSettings::Instance();
                    PlayerKeys* allKeys[5] = {
                        &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
                        &gs->player4Keys, &gs->player5Keys
                    };
                    PlayerKeys& keys = *allKeys[keyConfigPlayer - 1];
                    switch (keyConfigIndex) {
                        case 0: keys.left   = e->key.keysym.scancode; break;
                        case 1: keys.right  = e->key.keysym.scancode; break;
                        case 2: keys.fire   = e->key.keysym.scancode; break;
                        case 3: keys.center = e->key.keysym.scancode; break;
                    }
                    awaitKp = false;
                    AudioMixer::Instance()->PlaySFX("typewriter");
                    break;
                } else if (!awaitKp) {
                    // UP/DOWN: navigate keys within current player
                    if (e->key.keysym.sym == SDLK_UP) {
                        keyConfigIndex = (keyConfigIndex == 0) ? 4 : keyConfigIndex - 1;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.keysym.sym == SDLK_DOWN) {
                        keyConfigIndex = (keyConfigIndex == 4) ? 0 : keyConfigIndex + 1;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.keysym.sym == SDLK_LEFT) {
                        // Previous player
                        keyConfigPlayer = (keyConfigPlayer == 1) ? 4 : keyConfigPlayer - 1;
                        keyConfigIndex = 0;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.keysym.sym == SDLK_RIGHT) {
                        // Next player
                        keyConfigPlayer = (keyConfigPlayer == 4) ? 1 : keyConfigPlayer + 1;
                        keyConfigIndex = 0;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                        break;
                    } else if (e->key.keysym.sym == SDLK_RETURN) {
                        if (keyConfigIndex == 4) {
                            // Reset current player to default controller bindings
                            GameSettings* gs = GameSettings::Instance();
                            PlayerKeys* allKeys[5] = {
                                &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
                                &gs->player4Keys, &gs->player5Keys
                            };
                            int slot = keyConfigPlayer - 1;
                            PlayerKeys& keys = *allKeys[slot];
                            keys.left   = (SDL_Scancode)(CTRL_SC_BASE + slot * 20 + SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                            keys.right  = (SDL_Scancode)(CTRL_SC_BASE + slot * 20 + SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                            keys.fire   = (SDL_Scancode)(CTRL_SC_BASE + slot * 20 + SDL_CONTROLLER_BUTTON_A);
                            keys.center = (SDL_Scancode)(CTRL_SC_BASE + slot * 20 + SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                            gs->SaveKeys();
                            AudioMixer::Instance()->PlaySFX("typewriter");
                        } else {
                            // Wait for key press
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            awaitKp = true;
                        }
                        break;
                    }
                }
            }

            // Handle text input for chat when Chat is selected in lobby
            if (showingNetPanel && networkInLobby && networkInputMode == 0 && selectedActionIndex == 0) {
                // Handle printable characters for chat input
                SDL_Keycode key = e->key.keysym.sym;
                size_t len = strlen(networkChatInput);

                if ((key >= SDLK_a && key <= SDLK_z) || (key >= SDLK_0 && key <= SDLK_9) ||
                    key == SDLK_SPACE || key == SDLK_EXCLAIM || key == SDLK_QUESTION ||
                    key == SDLK_COMMA || key == SDLK_PERIOD || key == SDLK_QUOTE ||
                    key == SDLK_MINUS || key == SDLK_UNDERSCORE || key == SDLK_SLASH) {

                    if (len < sizeof(networkChatInput) - 1) {
                        char ch = (char)key;
                        // Convert to uppercase if shift is held
                        if (SDL_GetModState() & KMOD_SHIFT) {
                            if (ch >= 'a' && ch <= 'z') {
                                ch = ch - 'a' + 'A';
                            }
                        }
                        networkChatInput[len] = ch;
                        networkChatInput[len + 1] = '\0';
                    }
                    break; // Don't process this key further
                }
            }

            // Handle local multiplayer setup panel navigation
            if (showingLocalMPPanel && !runDelay) {
                // 0=Players, 1=CR, 2=Row collapse, 3..3+N-1=Aim guide per player, 3+N..3+2N-1=Colors per player, 3+2N=Start
                int localMaxIdx = 3 + 2 * localMPPlayerCount;
                if (e->key.keysym.sym == SDLK_UP) {
                    localMPMenuIndex--;
                    if (localMPMenuIndex < 0) localMPMenuIndex = localMaxIdx;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                } else if (e->key.keysym.sym == SDLK_DOWN) {
                    localMPMenuIndex++;
                    if (localMPMenuIndex > localMaxIdx) localMPMenuIndex = 0;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                } else if (e->key.keysym.sym == SDLK_LEFT || e->key.keysym.sym == SDLK_RIGHT) {
                    if (localMPMenuIndex == 0) {
                        if (e->key.keysym.sym == SDLK_LEFT) {
                            localMPPlayerCount--;
                            if (localMPPlayerCount < 2) localMPPlayerCount = 4;
                        } else {
                            localMPPlayerCount++;
                            if (localMPPlayerCount > 4) localMPPlayerCount = 2;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 1) {
                        localMPCR = !localMPCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 2) {
                        localMPNoCompress = !localMPNoCompress;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= 3 && localMPMenuIndex < 3 + localMPPlayerCount) {
                        int pi = localMPMenuIndex - 3;
                        localMPAimGuide[pi] = !localMPAimGuide[pi];
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= 3 + localMPPlayerCount && localMPMenuIndex < 3 + 2 * localMPPlayerCount) {
                        int pi = localMPMenuIndex - 3 - localMPPlayerCount;
                        if (e->key.keysym.sym == SDLK_LEFT) {
                            playerColorCounts[pi]--;
                            if (playerColorCounts[pi] < 5) playerColorCounts[pi] = 8;
                        } else {
                            playerColorCounts[pi]++;
                            if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    break;
                } else if (e->key.keysym.sym == SDLK_RETURN) {
                    if (localMPMenuIndex == 0) {
                        localMPPlayerCount++;
                        if (localMPPlayerCount > 4) localMPPlayerCount = 2;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 1) {
                        localMPCR = !localMPCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == 2) {
                        localMPNoCompress = !localMPNoCompress;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= 3 && localMPMenuIndex < 3 + localMPPlayerCount) {
                        int pi = localMPMenuIndex - 3;
                        localMPAimGuide[pi] = !localMPAimGuide[pi];
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex >= 3 + localMPPlayerCount && localMPMenuIndex < 3 + 2 * localMPPlayerCount) {
                        int pi = localMPMenuIndex - 3 - localMPPlayerCount;
                        playerColorCounts[pi]++;
                        if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (localMPMenuIndex == localMaxIdx) {
                        // Start game!
                        chainReaction = localMPCR;
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        delayTime = 60;
                        runDelay = true;
                    }
                    break;
                } else if (e->key.keysym.sym == SDLK_ESCAPE) {
                    showingLocalMPPanel = false;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                }
            }

            // Handle 2-player game setup menu navigation
            if (showing2PPanel && !awaitKp) {
                // 0=CR, 1=Victories, 2=Colors P1, 3=Colors P2, 4=Start
                if (e->key.keysym.sym == SDLK_UP) {
                    twoPlayerMenuIndex--;
                    if (twoPlayerMenuIndex < 0) twoPlayerMenuIndex = 4;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                } else if (e->key.keysym.sym == SDLK_DOWN) {
                    twoPlayerMenuIndex++;
                    if (twoPlayerMenuIndex > 4) twoPlayerMenuIndex = 0;
                    AudioMixer::Instance()->PlaySFX("menu_change");
                    break;
                } else if (e->key.keysym.sym == SDLK_LEFT || e->key.keysym.sym == SDLK_RIGHT) {
                    if (twoPlayerMenuIndex == 0) {
                        twoPlayerCR = !twoPlayerCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 1) {
                        if (e->key.keysym.sym == SDLK_LEFT) {
                            twoPlayerVictoriesIndex--;
                            if (twoPlayerVictoriesIndex < 0) twoPlayerVictoriesIndex = 17;
                        } else {
                            twoPlayerVictoriesIndex++;
                            if (twoPlayerVictoriesIndex > 17) twoPlayerVictoriesIndex = 0;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 2 || twoPlayerMenuIndex == 3) {
                        int pi = twoPlayerMenuIndex - 2;
                        if (e->key.keysym.sym == SDLK_LEFT) {
                            playerColorCounts[pi]--;
                            if (playerColorCounts[pi] < 5) playerColorCounts[pi] = 8;
                        } else {
                            playerColorCounts[pi]++;
                            if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    break;
                } else if (e->key.keysym.sym == SDLK_RETURN) {
                    if (twoPlayerMenuIndex == 0) {
                        twoPlayerCR = !twoPlayerCR;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 1) {
                        twoPlayerVictoriesIndex++;
                        if (twoPlayerVictoriesIndex > 17) twoPlayerVictoriesIndex = 0;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 2 || twoPlayerMenuIndex == 3) {
                        int pi = twoPlayerMenuIndex - 2;
                        playerColorCounts[pi]++;
                        if (playerColorCounts[pi] > 8) playerColorCounts[pi] = 5;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    } else if (twoPlayerMenuIndex == 4) {
                        // Start game!
                        chainReaction = twoPlayerCR;
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        delayTime = 60;
                        runDelay = true;
                    }
                    break;
                }
            }

            switch(e->key.keysym.sym) {
                case SDLK_UP:
                    // LAN menu navigation (0 = Set Name, 1 = Host, 2+ = servers)
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        if (lanMenuIndex > 0) { lanMenuIndex--; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        break;
                    }
                    // Net game menu navigation (0 = Manual entry, 1..n = public servers, n+1 = Set Name)
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        if (netMenuIndex > 0) { netMenuIndex--; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        break;
                    }
                    // Handle action menu selection in network lobby
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();

                            // Joiners in game room cannot scroll - stay on chat
                            if (currentGame && currentGame->creator != netClient->GetPlayerNick()) {
                                // Non-host in game - don't allow scrolling
                                break;
                            }

                            int maxActions;
                            if (currentGame && currentGame->creator == netClient->GetPlayerNick()) {
                                // Host: Chat + 4 global + 3 grid rows + optional Start
                                maxActions = 8 + ((int)currentGame->players.size() > 1 ? 1 : 0);
                            } else if (currentGame) {
                                // Non-host in game
                                maxActions = 1; // Just Chat (use ESC to leave)
                            } else {
                                // In lobby
                                std::vector<GameRoom> games = netClient->GetGameList();
                                maxActions = 2 + games.size(); // Chat + Create + Join games
                            }
                            selectedActionIndex--;
                            if (selectedActionIndex < 0) selectedActionIndex = maxActions - 1;
                            AudioMixer::Instance()->PlaySFX("menu_change");
                        }
                    } else {
                        up();
                    }
                    break;
                case SDLK_DOWN:
                    // LAN menu navigation
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        int lanMenuMax = 2 + (int)discoveredServers.size(); // 0=Host, 1..n=servers, n+1=SetName
                        if (lanMenuIndex < lanMenuMax - 1) { lanMenuIndex++; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        break;
                    }
                    // Net game menu navigation
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        int netMenuMax = 2 + (int)publicServers.size(); // 0=Manual, 1..n=servers, n+1=SetName
                        if (netMenuIndex < netMenuMax - 1) { netMenuIndex++; AudioMixer::Instance()->PlaySFX("menu_change"); }
                        break;
                    }
                    // Handle action menu selection in network lobby
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();

                            // Joiners in game room cannot scroll - stay on chat
                            if (currentGame && currentGame->creator != netClient->GetPlayerNick()) {
                                // Non-host in game - don't allow scrolling
                                break;
                            }

                            int maxActions;
                            if (currentGame && currentGame->creator == netClient->GetPlayerNick()) {
                                // Host: Chat + 4 global + 3 grid rows + optional Start
                                maxActions = 8 + ((int)currentGame->players.size() > 1 ? 1 : 0);
                            } else if (currentGame) {
                                // Non-host in game
                                maxActions = 1; // Just Chat (use ESC to leave)
                            } else {
                                // In lobby
                                std::vector<GameRoom> games = netClient->GetGameList();
                                maxActions = 2 + games.size(); // Chat + Create + Join games
                            }
                            selectedActionIndex++;
                            if (selectedActionIndex >= maxActions) selectedActionIndex = 0;
                            AudioMixer::Instance()->PlaySFX("menu_change");
                        }
                    } else {
                        down();
                    }
                    break;
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    // Handle LEFT/RIGHT for changing settings values (when not Chat)
                    if (showingNetPanel && networkInLobby && networkInputMode == 0 && selectedActionIndex != 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        GameRoom* currentGame = netClient->GetCurrentGame();
                        if (currentGame && currentGame->creator == netClient->GetPlayerNick()) {
                            // Only host can change settings
                            bool settingChanged = false;
                            if (selectedActionIndex == 1) {
                                chainReactionEnabled = !chainReactionEnabled;
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == 2) {
                                continueWhenPlayersLeave = !continueWhenPlayersLeave;
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == 3) {
                                singlePlayerTargetting = !singlePlayerTargetting;
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex == 4) {
                                // LEFT/RIGHT cycle victories limit
                                if (e->key.keysym.sym == SDLK_LEFT) {
                                    victoriesLimitIndex--;
                                    if (victoriesLimitIndex < 0) victoriesLimitIndex = 17;
                                } else {
                                    victoriesLimitIndex++;
                                    if (victoriesLimitIndex > 17) victoriesLimitIndex = 0;
                                }
                                AudioMixer::Instance()->PlaySFX("menu_change");
                                settingChanged = true;
                            } else if (selectedActionIndex >= 5 && selectedActionIndex <= 7) {
                                // Grid rows 5/6/7: Left/Right navigates player columns
                                // col 0 = ALL, col 1..N = P1..PN
                                int numPlayers = (int)currentGame->players.size();
                                if (numPlayers < 1) numPlayers = 1;
                                if (numPlayers > 5) numPlayers = 5;
                                int totalCols = numPlayers + 1; // +1 for ALL
                                if (e->key.keysym.sym == SDLK_LEFT) {
                                    currentPlayerCol--;
                                    if (currentPlayerCol < 0) currentPlayerCol = totalCols - 1;
                                } else {
                                    currentPlayerCol++;
                                    if (currentPlayerCol >= totalCols) currentPlayerCol = 0;
                                }
                                AudioMixer::Instance()->PlaySFX("menu_change");
                            }
                            if (settingChanged) {
                                static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                                netClient->SendOptions(chainReactionEnabled, continueWhenPlayersLeave,
                                    singlePlayerTargetting, vLimits[victoriesLimitIndex], playerColorCounts, playerNoCompress, playerAimGuide);
                            }
                        }
                    }
                    break;
                case SDLK_RETURN:
                    if (showingLevelPanel && !runDelay) {
                        // Confirm level selection
                        if (!levelInput.empty()) {
                            int lv = std::stoi(levelInput);
                            if (lv < 1) lv = 1;
                            if (lv > 100) lv = 100;
                            pickedStartLevel = lv;
                        }
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        delayTime = 60;
                        runDelay = true;
                        break;
                    }
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        // Execute selected action in main lobby or game room
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            GameRoom* currentGame = netClient->GetCurrentGame();

                            if (selectedActionIndex == 0) {
                                // Chat - handle commands or send message
                                if (strlen(networkChatInput) > 0) {
                                    // Check if it's a command (starts with /)
                                    if (networkChatInput[0] == '/') {
                                        // Handle chat commands
                                        if (strncmp(networkChatInput, "/nick ", 6) == 0) {
                                            // Change nickname
                                            const char* newNick = networkChatInput + 6;
                                            if (strlen(newNick) > 0 && netClient->SendNick(newNick)) {
                                                char msg[128];
                                                snprintf(msg, sizeof(msg), "You are now known as %s", newNick);
                                                netClient->AddStatusMessage(msg);
                                            } else {
                                                netClient->AddStatusMessage("Erroneous nickname");
                                            }
                                        } else if (strcmp(networkChatInput, "/help") == 0) {
                                            // Show help
                                            netClient->AddStatusMessage("Available commands: /nick <new_nick>, /help");
                                        } else {
                                            // Unknown command
                                            netClient->AddStatusMessage("Unknown command. Type /help for help.");
                                        }
                                    } else {
                                        // Regular chat message - send it
                                        netClient->SendTalk(networkChatInput);
                                    }
                                    networkChatInput[0] = '\0';
                                    AudioMixer::Instance()->PlaySFX("menu_selected");
                                }
                            } else if (currentGame) {
                                // In game room - handle game settings and Start/Part actions
                                bool isHost = currentGame->creator == netClient->GetPlayerNick();

                                if (selectedActionIndex == 0) {
                                    // Chat - same for both host and joiner
                                    if (strlen(networkChatInput) > 0) {
                                        if (networkChatInput[0] == '/') {
                                            // Handle chat commands
                                            if (strncmp(networkChatInput, "/nick ", 6) == 0) {
                                                const char* newNick = networkChatInput + 6;
                                                if (strlen(newNick) > 0 && netClient->SendNick(newNick)) {
                                                    char msg[128];
                                                    snprintf(msg, sizeof(msg), "You are now known as %s", newNick);
                                                    netClient->AddStatusMessage(msg);
                                                } else {
                                                    netClient->AddStatusMessage("Erroneous nickname");
                                                }
                                            } else if (strcmp(networkChatInput, "/help") == 0) {
                                                netClient->AddStatusMessage("Available commands: /nick <new_nick>, /help");
                                            } else {
                                                netClient->AddStatusMessage("Unknown command. Type /help for help.");
                                            }
                                        } else {
                                            // Regular chat message
                                            netClient->SendTalk(networkChatInput);
                                        }
                                        networkChatInput[0] = '\0';
                                        AudioMixer::Instance()->PlaySFX("menu_selected");
                                    }
                                } else if (isHost) {
                                    // Host actions: 0=Chat, 1=CR, 2=Continue, 3=Target, 4=Victories, 5..5+N-1=Colors, last=Start
                                    int numPlayers = currentGame ? (int)currentGame->players.size() : 1;
                                    if (numPlayers < 1) numPlayers = 1;
                                    if (numPlayers > 5) numPlayers = 5;
                                    bool settingChanged = false;
                                    if (selectedActionIndex == 1) {
                                        // Toggle chain reaction
                                        chainReactionEnabled = !chainReactionEnabled;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 2) {
                                        // Toggle continue when players leave
                                        continueWhenPlayersLeave = !continueWhenPlayersLeave;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 3) {
                                        // Toggle single player targetting
                                        singlePlayerTargetting = !singlePlayerTargetting;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 4) {
                                        // Cycle victories limit
                                        victoriesLimitIndex++;
                                        if (victoriesLimitIndex > 17) victoriesLimitIndex = 0; // 18 values total
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 5) {
                                        // Cycle per-player color count; col 0 = ALL
                                        int np = (int)currentGame->players.size();
                                        if (np < 1) np = 1; if (np > 5) np = 5;
                                        int lo = (currentPlayerCol == 0) ? 0 : currentPlayerCol - 1;
                                        int hi = (currentPlayerCol == 0) ? np : currentPlayerCol;
                                        for (int i = lo; i < hi; i++) {
                                            playerColorCounts[i]++;
                                            if (playerColorCounts[i] > 8) playerColorCounts[i] = 5;
                                        }
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 6) {
                                        // Toggle per-player compression; col 0 = ALL (set all to majority opposite)
                                        int np = (int)currentGame->players.size();
                                        if (np < 1) np = 1; if (np > 5) np = 5;
                                        int lo = (currentPlayerCol == 0) ? 0 : currentPlayerCol - 1;
                                        int hi = (currentPlayerCol == 0) ? np : currentPlayerCol;
                                        bool allOn = true;
                                        for (int i = lo; i < hi; i++) if (playerNoCompress[i]) allOn = false;
                                        for (int i = lo; i < hi; i++) playerNoCompress[i] = allOn;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 7) {
                                        // Toggle per-player aim guide; col 0 = ALL (set all to majority opposite)
                                        int np = (int)currentGame->players.size();
                                        if (np < 1) np = 1; if (np > 5) np = 5;
                                        int lo = (currentPlayerCol == 0) ? 0 : currentPlayerCol - 1;
                                        int hi = (currentPlayerCol == 0) ? np : currentPlayerCol;
                                        bool allOn = true;
                                        for (int i = lo; i < hi; i++) if (!playerAimGuide[i]) allOn = false;
                                        for (int i = lo; i < hi; i++) playerAimGuide[i] = !allOn;
                                        AudioMixer::Instance()->PlaySFX("menu_change");
                                        settingChanged = true;
                                    } else if (selectedActionIndex == 8 && currentGame && currentGame->players.size() > 1) {
                                        // Start game (index 8, fixed regardless of player count)
                                        netClient->StartGame();
                                        netClient->AddStatusMessage("Starting game...");
                                        AudioMixer::Instance()->PlaySFX("menu_selected");
                                    }
                                    if (settingChanged) {
                                        static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                                        netClient->SendOptions(chainReactionEnabled, continueWhenPlayersLeave,
                                            singlePlayerTargetting, vLimits[victoriesLimitIndex], playerColorCounts, playerNoCompress, playerAimGuide);
                                    }
                                }
                                // Non-host has no actions other than Chat (use ESC to leave)
                            } else {
                                // In lobby - handle Create/Join actions
                                if (selectedActionIndex == 1) {
                                    // Create new game - do it immediately like original
                                    netClient->CreateGame();
                                    netClient->AddStatusMessage("Game created - now you need to wait for players to join");
                                    AudioMixer::Instance()->PlaySFX("menu_selected");
                                } else {
                                    // Join game (selectedActionIndex >= 2)
                                    SDL_Log("Join game action: selectedActionIndex=%d", selectedActionIndex);
                                    std::vector<GameRoom> games = netClient->GetGameList();
                                    int gameIndex = selectedActionIndex - 2;
                                    SDL_Log("Join game: gameIndex=%d, games.size()=%d", gameIndex, (int)games.size());
                                    if (gameIndex >= 0 && gameIndex < (int)games.size()) {
                                        SDL_Log("Attempting to join game created by: %s", games[gameIndex].creator.c_str());
                                        if (netClient->JoinGame(games[gameIndex].creator.c_str())) {
                                            SDL_Log("JoinGame returned true - successfully joined!");
                                            netClient->AddStatusMessage("Joined game");
                                            AudioMixer::Instance()->PlaySFX("menu_selected");
                                            // Joiner defaults to chat (index 0) and can't scroll through settings
                                            selectedActionIndex = 0;
                                        } else {
                                            SDL_Log("JoinGame returned false - join failed!");
                                        }
                                    } else {
                                        SDL_Log("Join game: gameIndex out of bounds!");
                                    }
                                }
                            }
                        }
                        break;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 6) {
                        // Create game confirmed
                        NetworkClient* netClient = NetworkClient::Instance();
                        // Accept CONNECTED or IN_LOBBY — after returning from a game state is IN_LOBBY
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            if (netClient->CreateGame()) {
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                            }
                        }
                        networkInputMode = 0; // Back to lobby
                        SDL_StopTextInput();
                        break;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 5) {
                        // Change username
                        if (strlen(networkUsername) > 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->SendNick(networkUsername)) {
                                SDL_Log("Username changed to: %s", networkUsername);
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                            }
                            networkUsername[0] = '\0';
                        }
                        networkInputMode = 0; // Back to lobby
                        SDL_StopTextInput();
                        break;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 4) {
                        // Send chat message
                        if (strlen(networkChatInput) > 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            netClient->SendTalk(networkChatInput);
                            networkChatInput[0] = '\0';
                        }
                        networkInputMode = 0; // Back to lobby
                        SDL_StopTextInput();
                        break;
                    } else if (showingNetPanel && networkInLobby && networkInputMode == 3) {
                        // Join game with entered creator name
                        if (strlen(networkJoinCreator) > 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->JoinGame(networkJoinCreator)) {
                                SDL_Log("Joined game created by %s", networkJoinCreator);
                            }
                            networkInputMode = 0; // Back to lobby
                        }
                        break;
                    } else if (showingNetPanel && !networkInLobby && networkInputMode == 11) {
                        // Confirm pre-lobby nickname
                        networkInputMode = networkPreNickReturnMode;
                        SDL_StopTextInput();
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                        break;
                    } else if (showingNetPanel && !networkInLobby &&
                               (networkInputMode == 8 || networkInputMode == 9)) {
                        if (networkFieldEditing) {
                            // ENTER while keyboard open → close keyboard
                            networkFieldEditing = false;
                            SDL_StopTextInput();
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            break;
                        } else if (networkManualFieldIndex == 2) {
                            // ENTER on Connect button → connect
                            goto DO_CONNECT;
                        } else {
                            // ENTER on host/port field → open keyboard
                            networkFieldEditing = true;
                            SDL_StopTextInput();
                            SDL_StartTextInput();
                            { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputRect(&r); }
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            break;
                        }
                    } else if (showingNetPanel && !networkInLobby &&
                               (networkInputMode == 7 || networkInputMode == 10)) {
                        DO_CONNECT:
                        // Connect to server
                        const char* host = networkHost;
                        int port = networkPort;
                        if (networkInputMode == 7) {
                            if (lanMenuIndex == 0) {
                                // "Host a server" selected
                                if (!serverHosting) {
                                    StartLocalServer();
                                    SDL_Delay(500);
                                }
                                connectErrorMsg.clear();
                                discoveredServers = NetworkClient::DiscoverLANServers();
                                for (auto& s : discoveredServers)
                                    s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
                                lanMenuIndex = 0;
                                break;
                            }
                            int serverIdx = lanMenuIndex - 1;
                            if (serverIdx >= (int)discoveredServers.size()) {
                                // "Set Name" selected (last item)
                                networkPreNickReturnMode = 7;
                                networkInputMode = 11;
                                SDL_StopTextInput();
                                SDL_StartTextInput();
                                { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputRect(&r); }
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                break;
                            }
                            host = discoveredServers[serverIdx].host.c_str();
                            port = discoveredServers[serverIdx].port;
                        }
                        if (networkInputMode == 10) {
                            if (netMenuIndex == 0) {
                                // "Manual entry" selected — show host/port form without keyboard.
                                // Keyboard opens only when user presses SELECT on host or port field.
                                networkInputMode = 8;
                                networkFieldEditing = false;
                                networkManualFieldIndex = 0;
                                SDL_StopTextInput();
                                break;
                            }
                            int serverIdx = netMenuIndex - 1;
                            if (serverIdx >= (int)publicServers.size()) {
                                // "Set Name" selected (last item)
                                networkPreNickReturnMode = 10;
                                networkInputMode = 11;
                                SDL_StopTextInput();
                                SDL_StartTextInput();
                                { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputRect(&r); }
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                                break;
                            }
                            host = publicServers[serverIdx].host.c_str();
                            port = publicServers[serverIdx].port;
                        }
                        NetworkClient* netClient = NetworkClient::Instance(host, port);
                        if (netClient->IsConnected()) {
                            AudioMixer::Instance()->PlaySFX("menu_selected");
                            char nickname[32];
                            if (networkPreNick[0] != '\0') {
                                snprintf(nickname, sizeof(nickname), "%s", networkPreNick);
                            } else {
                                const char* envUser = getenv("USER");
                                if (envUser && envUser[0] != '\0') snprintf(nickname, sizeof(nickname), "%s", envUser);
                                else snprintf(nickname, sizeof(nickname), "unnamed");
                            }
                            if (netClient->SendNick(nickname)) {
                                SDL_Delay(100);
                                std::string geoLoc = NetworkClient::DetectGeoLocation();
                                // Parse for own spot rendering
                                float gLat = 0.0f, gLon = 0.0f;
                                if (sscanf(geoLoc.c_str(), "%f:%f", &gLat, &gLon) == 2) {
                                    myGeoLat = gLat; myGeoLon = gLon; myGeoLocSet = true;
                                }
                                if (netClient->SendGeoLoc(geoLoc.c_str())) {
                                    networkInLobby = true;
                                    networkInputMode = 0;  // Switch to lobby mode so C/J/T/U keys work
                                    networkGameStarting = false;
                                    wasmSyncWaitStart = 0;
                                    netClient->RequestList();  // Immediate list on lobby entry
                                    lastListRequest = SDL_GetTicks();
#ifdef __ANDROID__
                                    SDL_AndroidSendMessage(0x8001, 0); // show lobby ad
#endif
                                }
                            } else {
                                // SendNick failed — WebSocket is still connecting (WASM async).
                                // Store the nickname and complete lobby entry in NetPanelRender()
                                // once the WebSocket open callback fires and state becomes CONNECTED.
                                SDL_Log("SendNick failed (state=%d), setting pendingLobbyConnect for async completion", netClient->GetState());
                                snprintf(networkPreNick, sizeof(networkPreNick), "%s", nickname);
                                pendingLobbyConnect = true;
                                connectErrorMsg.clear();
                            }
                        } else {
                            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to connect to %s:%d", host, port);
                            char errBuf[320];
                            snprintf(errBuf, sizeof(errBuf), "Failed to connect to %s:%d", host, port);
                            connectErrorMsg = errBuf;
                        }
                    } else {
                        press();
                    }
                    break;
                case SDLK_c:
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        // Accept CONNECTED or IN_LOBBY — after returning from a game state is IN_LOBBY
                        if (netClient->GetState() == CONNECTED || netClient->GetState() == IN_LOBBY) {
                            if (!netClient->GetCurrentGame()) {
                                // Open create game confirmation screen
                                networkInputMode = 6;
                                AudioMixer::Instance()->PlaySFX("menu_selected");
                            }
                        }
                    }
                    break;
                case SDLK_r:
                    if (showingNetPanel && !networkInLobby && networkInputMode == 7) {
                        connectErrorMsg.clear();
                        discoveredServers = NetworkClient::DiscoverLANServers();
                        for (auto& s : discoveredServers)
                            s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
                        lanMenuIndex = 0;
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    if (showingNetPanel && !networkInLobby && networkInputMode == 10) {
                        connectErrorMsg.clear();
                        netMenuIndex = 0;
                        publicServers = NetworkClient::FetchPublicServers();
                        for (auto& s : publicServers)
                            s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
                        AudioMixer::Instance()->PlaySFX("menu_change");
                    }
                    break;
                case SDLK_j:
                    if (showingNetPanel && networkInLobby) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == CONNECTED) {
                            std::vector<GameRoom> games = netClient->GetGameList();
                            // If a game is selected, join it directly
                            if (selectedGameIndex >= 0 && selectedGameIndex < (int)games.size()) {
                                if (netClient->JoinGame(games[selectedGameIndex].creator.c_str())) {
                                    SDL_Log("Joining game created by %s", games[selectedGameIndex].creator.c_str());
                                    AudioMixer::Instance()->PlaySFX("menu_selected");
                                }
                            } else {
                                // No game selected, enter manual join mode
                                networkInputMode = 3;
                                networkJoinCreator[0] = '\0';
                                SDL_Log("Enter creator name to join their game");
                            }
                        }
                    }
                    break;
                case SDLK_t:
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        // Enter chat mode
                        networkInputMode = 4;
                        networkChatInput[0] = '\0';
                        SDL_StopTextInput();
                        SDL_StartTextInput();
                        { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputRect(&r); }
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                    }
                    break;
                case SDLK_u:
                    if (showingNetPanel && networkInLobby && networkInputMode == 0) {
                        // Enter username change mode
                        networkInputMode = 5;
                        networkUsername[0] = '\0';
                        SDL_StopTextInput();
                        SDL_StartTextInput();
                        { SDL_Rect r = {160, 152, 320, 20}; SDL_SetTextInputRect(&r); }
                        AudioMixer::Instance()->PlaySFX("menu_selected");
                    }
                    break;
                case SDLK_s:
                    if (showingNetPanel && networkInLobby) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        GameRoom* currentGame = netClient->GetCurrentGame();
                        // Only allow host to start game with 'S' key
                        if (currentGame && netClient->GetState() == IN_LOBBY) {
                            bool isHost = currentGame->creator == netClient->GetPlayerNick();
                            if (isHost && currentGame->players.size() > 1) {
                                netClient->StartGame();
                                netClient->AddStatusMessage("Starting game...");
                            }
                        }
                    }
                    break;
                case SDLK_p:
                    if (showingNetPanel && networkInLobby) {
                        NetworkClient* netClient = NetworkClient::Instance();
                        if (netClient->GetState() == IN_LOBBY) {
                            netClient->PartGame();
                            netClient->RequestList();  // Immediate list after parting
                            lastListRequest = SDL_GetTicks();
                        }
                    }
                    break;
                case SDLK_n:
                    if(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL] == SDL_PRESSED) RefreshCandy();
                    break;
                case SDLK_AC_BACK:
                case SDLK_ESCAPE:
                    if (showingSPPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingSPPanel = false;
                        break;
                    }
                    if (showingNetPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        if (networkInputMode == 11) {
                            // Cancel pre-lobby nickname editing
                            networkInputMode = networkPreNickReturnMode;
                            SDL_StopTextInput();
                            break;
                        } else if (networkInputMode == 8 || networkInputMode == 9) {
                            if (networkFieldEditing) {
                                // ESC while keyboard is open: close keyboard, stay on form
                                networkFieldEditing = false;
                                SDL_StopTextInput();
                            } else {
                                // ESC with no keyboard: back to public server list
                                networkInputMode = 10;
                            }
                            break;
                        } else if (networkInputMode == 10) {
                            // Close net panel
                            showingNetPanel = false;
                            break;
                        } else if (networkInputMode == 6) {
                            // Cancel create game confirmation
                            networkInputMode = 0;
                            break;
                        } else if (networkInputMode == 5) {
                            // Cancel username input
                            networkInputMode = 0;
                            networkUsername[0] = '\0';
                            SDL_StopTextInput();
                            break;
                        } else if (networkInputMode == 4) {
                            // Cancel chat input
                            networkInputMode = 0;
                            networkChatInput[0] = '\0';
                            break;
                        } else if (networkInputMode == 3) {
                            // Cancel join input
                            networkInputMode = 0;
                            networkJoinCreator[0] = '\0';
                            break;
                        } else if (networkInputMode == 0 && networkInLobby) {
                            // In lobby - check if in a game
                            NetworkClient* netClient = NetworkClient::Instance();
                            GameRoom* currentGame = netClient->GetCurrentGame();
                            if (currentGame) {
                                // Leave the game (like original)
                                netClient->PartGame();
                                netClient->RequestList();  // Immediate list after parting
                                lastListRequest = SDL_GetTicks();
                                netClient->AddStatusMessage("*** Leaving game...");
                            } else {
                                // Not in a game - disconnect from server
                                showingNetPanel = false;
                                networkInLobby = false;
                                if (netClient->IsConnected()) {
                                    netClient->Disconnect();
                                }
                            }
                        } else {
                            showingNetPanel = false;
                            networkInLobby = false;
                            pendingLobbyConnect = false;
                            if (NetworkClient::Instance()->IsConnected()) {
                                NetworkClient::Instance()->Disconnect();
                            }
                            if (serverHosting) {
                                StopLocalServer();
                            }
                        }
                        break;
                    }
                    if (showingLevelPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingLevelPanel = false;
                        SDL_StopTextInput();
                        runDelay = false;
                        break;
                    }
                    if (showingKeysPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingKeysPanel = false;
                        GameSettings::Instance()->SaveKeys();
                        break;
                    }
                    if (showing2PPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showing2PPanel = false;
                        break;
                    }
                    if (showingLocalMPPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingLocalMPPanel = false;
                        break;
                    }
                    if (showingOptPanel || showingNetSetupPanel) {
                        AudioMixer::Instance()->PlaySFX("cancel");
                        showingOptPanel = showingNetSetupPanel = false;
                        awaitKp = false;
                        break;
                    }
                    // On Android TV, pressing Back at the root menu should not
                    // quit the game — the Fire TV launcher injects KEYCODE_BACK
                    // during the launch transition, which would immediately kill
                    // the app. Users navigate away via the Home button instead.
#ifndef __ANDROID__
                    FrozenBubble::Instance()->CallGameQuit();
#endif
                    break;
                case SDLK_F11: // mute / unpause audio
                    if(AudioMixer::Instance()->IsHalted() == true) {
                        AudioMixer::Instance()->MuteAll(true);
                        AudioMixer::Instance()->PlayMusic("intro");
                    }
                    else AudioMixer::Instance()->MuteAll();
                    break;
            }
            break;
    }
}

void MainMenu::Render(void) {
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), background, nullptr, nullptr);

    for (MenuButton &button : buttons) {
        button.Render(renderer);
    }
    BannerRender();
    BlinkRender();
    CandyRender();
    SPPanelRender();
    TPPanelRender();
    LocalMPPanelRender();
    OptPanelRender();
    LevelPanelRender();
    KeysPanelRender();
    NetSetupPanelRender();
    NetPanelRender();
}

void MainMenu::BannerRender() {
    bannerCurpos = bannerCurpos != 0 ? bannerCurpos : 670;
    for(size_t i = 0; i < std::size(bannerFormulas); i++) {
        int posX = bannerFormulas[i] - bannerCurpos;
        SDL_Texture *image = i == 0 ? bannerArtwork : (i == 1 ? bannerSound : (i == 2 ? bannerCPU : bannerLevel));
        SDL_Point size = GetSize(image);
        if (posX > bannerMax / 2) posX = bannerFormulas[i] - (bannerCurpos + bannerMax);

        if (posX < BANNER_MAXX && posX + size.x >= 0) {
            SDL_Rect iRect = {-posX, 0, std::min(size.x + posX, BANNER_MAXX - BANNER_MINX), size.y};
            SDL_Rect dRect = {iRect.x < 0 ? BANNER_MAXX - (-posX > -iRect.w ? -posX + iRect.w : 0): BANNER_MINX, BANNER_Y, 
                              iRect.x < 0 ? iRect.w - (-posX > -iRect.w ? posX : 0): iRect.w, size.y};
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), image, &iRect, &dRect);
        }
    }

    if(GameSettings::Instance()->gfxLevel() > 2) return;
    if(bannerFU == 0) {
        bannerCurpos++;
        bannerFU = BANNER_SLOWDOWN;
    }
    else bannerFU--;
    if(bannerCurpos >= bannerMax) bannerCurpos = 1;
}

void MainMenu::BlinkRender() {
    if(GameSettings::Instance()->gfxLevel() > 2) return;

    if (waitGreen <= 0) {
        if(blinkGreen > 0) {
            blinkGreen--;
            if(!blinkGreen) {
                waitGreen = BLINK_FRAMES;
                if(ranrange(0, 3) <= 1) blinkGreen = -(5 * BLINK_SLOWDOWN); 
            }
        }
        else if(blinkGreen < 0) {
            blinkGreen++;
            if(!blinkGreen) {
                waitGreen = BLINK_FRAMES;
                blinkGreen = 3 * BLINK_SLOWDOWN; 
            }
        }
        else {
            if(ranrange(0, 200) <= 1) {
                waitGreen = BLINK_FRAMES;
                blinkGreen = 3 * BLINK_SLOWDOWN;
            }
        }
    }
    else {
        waitGreen--;
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), blinkGreenL, NULL, &blink_green_left);
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), blinkGreenR, NULL, &blink_green_right);
    }
    
    if(waitPurple <= 0) {
        if(blinkPurple > 0) {
            blinkPurple--;
            if(!blinkPurple) {
                waitPurple = BLINK_FRAMES;
                if(ranrange(0, 3) <= 1) blinkPurple = -(5 * BLINK_SLOWDOWN); 
            }
        }
        else if(blinkPurple < 0) {
            blinkPurple++;
            if(!blinkPurple) {
                waitPurple = BLINK_FRAMES;
                blinkPurple = 3 * BLINK_SLOWDOWN; 
            }
        }
        else {
            if(ranrange(0, 200) <= 1) {
                waitPurple = BLINK_FRAMES;
                blinkPurple = 3 * BLINK_SLOWDOWN;
            }
        }
    }
    else {
        waitPurple--;
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), blinkPurpleL, NULL, &blink_purple_left);
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), blinkPurpleR, NULL, &blink_purple_right);
    }

}

void MainMenu::CandyRender() {
    if (!candyInit || GameSettings::Instance()->gfxLevel() > 1) {
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), fbLogo, nullptr, &fb_logo_rect);
        return;
    }

    if (candyMethod == 0)       rotate_bilinear_(candyModif.sfc, candyOrig.sfc, SDL_sin(candyIndex/40.0)/10.0);
    else if(candyMethod == 1)   flipflop_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 2)   enlighten_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 3)   stretch_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 4)   tilt_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 5)   points_(candyModif.sfc, candyOrig.sfc, logoMask.sfc);
    else if(candyMethod == 6)   waterize_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 7)   brokentv_(candyModif.sfc, candyOrig.sfc, candyIndex);
    else if(candyMethod == 8)   snow_(candyModif.sfc, candyOrig.sfc);

    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), candyModif.OutputTexture(), nullptr, &candy_fb_rect);
    candyIndex++;
}

void restartOverlook(SDL_Surface *overlookSfc, int &overlookIndex){
    if(GameSettings::Instance()->gfxLevel() > 2) return;
    overlook_init_(overlookSfc);
    overlookIndex = 0;
}

void MainMenu::SPPanelRender() {
    if (!showingSPPanel) return;

    if(overlookSfc == nullptr) {
        overlookSfc = SDL_CreateRGBSurfaceWithFormat(0, activeSPButtons[0]->w, activeSPButtons[0]->h, 32, SURF_FORMAT);
        overlook_init_(overlookSfc);
    }

    // SP panel needs extra height for SP_OPT items: first item at y=191, each 41px apart, 37px tall
    // For 5 items: last item bottom = 191 + (SP_OPT-1)*41 + 37 = 392 -> need panel bottom >= 400
    SDL_Rect spPanelRct = {(640/2) - (341/2), (480/2) - (320/2), 341, 320};
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), singlePanelBG, nullptr, &spPanelRct);
    for (int i = 0; i < SP_OPT; i++){
        int w, h;
        SDL_QueryTexture(idleSPButtons[i], nullptr, nullptr, &w, &h);
        SDL_Rect entryRct = {(640/2)-(298/2), ((480/2)-90)+(41 * (i + 1)), 298, 37};
        SDL_Rect subRct = {(640/2)-(298/2), ((480/2)-90)+(41 * (i + 1)), w, h};
        if(i == activeSPIdx) {
            if (GameSettings::Instance()->gfxLevel() <= 2) {
                overlook_(overlookSfc, activeSPButtons[i], overlookIndex, spOptions[i].pivot);
                SDL_Rect miniRct = {(640/2)-(298/2), ((480/2)-90)+(41 * (i + 1)), overlookSfc->w, overlookSfc->h};
                SDL_Texture *miniOverlook = SDL_CreateTextureFromSurface(const_cast<SDL_Renderer*>(renderer), overlookSfc);
                
                SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), singleButtonAct, nullptr, &entryRct);
                SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), miniOverlook, nullptr, &miniRct);
                SDL_DestroyTexture(miniOverlook);

                overlookIndex++;
                if (overlookIndex >= 70) overlookIndex = 0;
            }
            else SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), singleButtonAct, nullptr, &entryRct);
        }
        else SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), singleButtonIdle, nullptr, &entryRct);
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), idleSPButtons[i], nullptr, &subRct);
    }

    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
}

void MainMenu::TPPanelRender() {
    if (!showing2PPanel) return;

    if (runDelay){
        if (delayTime == 0) SetupNewGame(selectedMode);
        else delayTime--;
    }

    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &voidPanelRct);

    // Menu-based 2P setup like network lobby
    const char* victoriesLimits[] = {"none (unlimited)", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "15", "20", "30", "50", "100"};

    char pnltxt[512];
    snprintf(pnltxt, sizeof(pnltxt),
        "2-player game\n\n"
        "%s Chain-reaction: %s\n"
        "%s Victories limit: %s\n"
        "%s Max colors P1: %d\n"
        "%s Max colors P2: %d\n"
        "%s Start game!\n\n\n"
        "Use UP/DOWN to select\n"
        "LEFT/RIGHT or ENTER to change\n"
        "Press ESC to cancel",
        twoPlayerMenuIndex == 0 ? ">" : " ",
        twoPlayerCR ? "enabled" : "disabled",
        twoPlayerMenuIndex == 1 ? ">" : " ",
        victoriesLimits[twoPlayerVictoriesIndex],
        twoPlayerMenuIndex == 2 ? ">" : " ",
        playerColorCounts[0],
        twoPlayerMenuIndex == 3 ? ">" : " ",
        playerColorCounts[1],
        twoPlayerMenuIndex == 4 ? ">" : " ");

    panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), pnltxt, 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
}

void MainMenu::LocalMPPanelRender() {
    if (!showingLocalMPPanel) return;

    if (runDelay) {
        if (delayTime == 0) SetupNewGame(7);
        else delayTime--;
    }

    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &voidPanelRct);

    int connected = SDL_NumJoysticks();
    char warningText[128] = "";
    if (connected < localMPPlayerCount) {
        snprintf(warningText, sizeof(warningText),
            "WARNING: %d controller(s) connected, need %d\n\n",
            connected, localMPPlayerCount);
    }

    char pnltxt[1024];
    int pos = snprintf(pnltxt, sizeof(pnltxt),
        "Local multiplayer\n\n"
        "%s"
        "%s Players: %d\n"
        "%s Chain-reaction: %s\n"
        "%s Row collapse: %s\n",
        warningText,
        localMPMenuIndex == 0 ? ">" : " ",
        localMPPlayerCount,
        localMPMenuIndex == 1 ? ">" : " ",
        localMPCR ? "enabled" : "disabled",
        localMPMenuIndex == 2 ? ">" : " ",
        localMPNoCompress ? "disabled" : "enabled");
    for (int pi = 0; pi < localMPPlayerCount && pi < 5; pi++) {
        pos += snprintf(pnltxt + pos, sizeof(pnltxt) - pos,
            "%s Aim guide P%d: %s\n",
            localMPMenuIndex == 3 + pi ? ">" : " ",
            pi + 1,
            localMPAimGuide[pi] ? "enabled" : "disabled");
    }
    for (int pi = 0; pi < localMPPlayerCount && pi < 5; pi++) {
        pos += snprintf(pnltxt + pos, sizeof(pnltxt) - pos,
            "%s Max colors P%d: %d\n",
            localMPMenuIndex == 3 + localMPPlayerCount + pi ? ">" : " ",
            pi + 1,
            playerColorCounts[pi]);
    }
    snprintf(pnltxt + pos, sizeof(pnltxt) - pos,
        "%s Start game!\n\n"
        "Each player needs a controller.\n"
        "Use UP/DOWN to select\n"
        "LEFT/RIGHT or ENTER to change\n"
        "Press ESC to cancel",
        localMPMenuIndex == 3 + 2 * localMPPlayerCount ? ">" : " ");

    panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), pnltxt, 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 130});
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
}

void MainMenu::OptPanelRender() {
    if (!showingOptPanel) return;

#ifdef __ANDROID__
    // On Android, show "Remove Ads" option at the bottom of the options panel.
    // Pressing 'R' (mapped from a controller button via fake key event) triggers IAP.
    // This is rendered as informational text; actual purchase is via SDL_AndroidSendMessage.
    (void)0; // placeholder — text injected into snprintf below
#endif

    if(awaitKp == false && lastOptInput != SDLK_UNKNOWN && !runDelay) { // we got our response
        chainReaction = lastOptInput == SDLK_y ? true : false;

        char pnltxt[256];
        snprintf(pnltxt, sizeof(pnltxt), "Random level\n\n\nEnable chain reaction?\n\n\nY or N?:        %s\n\n\n\n\nEnjoy the game!", SDL_GetKeyName(lastOptInput));
        panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), pnltxt, 0);
        panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});

        delayTime = 120;
        runDelay = true;
    }

    if (runDelay){
        if (delayTime == 0) SetupNewGame(selectedMode);
        else delayTime--;
    }

    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &voidPanelRct);
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
}

void MainMenu::LevelPanelRender() {
    if (!showingLevelPanel) return;

    char txt[256];
    int maxLevel = 100;
    snprintf(txt, sizeof(txt),
        "Pick start level\n\n"
        "Enter level (1-%d):\n\n"
        "%s_",
        maxLevel, levelInput.c_str());
    panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 80});

    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &voidPanelRct);
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());

    if (runDelay) {
        if (delayTime == 0) {
            showingLevelPanel = false;
            SDL_StopTextInput();
            SetupNewGame(5);  // mode 5 = pick_start_level
        } else {
            delayTime--;
        }
    }
}

void MainMenu::KeysPanelRender() {
    if (!showingKeysPanel) return;

    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &voidPanelRct);

    char keyText[1024];

    GameSettings* gs = GameSettings::Instance();
    PlayerKeys* allKeys[5] = {
        &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
        &gs->player4Keys, &gs->player5Keys
    };
    PlayerKeys& pk = *allKeys[keyConfigPlayer - 1];

    const char* indicator = awaitKp ? " <--" : " >";

    snprintf(keyText, sizeof(keyText),
        "Key config  Player %d/4\n"
        "LEFT/RIGHT to switch player\n\n"
        "%sturn left?   %s\n"
        "%sturn right?  %s\n"
        "%sfire?        %s\n"
        "%scenter?      %s\n\n"
        "%sReset ctrl defaults\n\n"
        "%s\n"
        "UP/DOWN select, ENTER change\n"
        "ESC when done",
        keyConfigPlayer,
        keyConfigIndex == 0 ? indicator : "  ", ControllerScancodeName(pk.left).c_str(),
        keyConfigIndex == 1 ? indicator : "  ", ControllerScancodeName(pk.right).c_str(),
        keyConfigIndex == 2 ? indicator : "  ", ControllerScancodeName(pk.fire).c_str(),
        keyConfigIndex == 3 ? indicator : "  ", ControllerScancodeName(pk.center).c_str(),
        keyConfigIndex == 4 ? indicator : "  ",
        awaitKp ? "Press button or key..." : "");

    panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), keyText, 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &voidPanelRct);
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
}

void MainMenu::NetSetupPanelRender() {
    if (!showingNetSetupPanel) return;

    if(awaitKp == false && lastOptInput != SDLK_UNKNOWN && !runDelay) { // we got our response
        chainReaction = lastOptInput == SDLK_y ? true : false;

        char pnltxt[256];
        snprintf(pnltxt, sizeof(pnltxt), "Network game\n\n\nEnable chain reaction?\n\n\nY or N?:        %s\n\n\n\n\nConnecting...", SDL_GetKeyName(lastOptInput));
        panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), pnltxt, 0);
        panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});

        delayTime = 60; // Shorter delay for network games
        runDelay = true;
    }

    if (runDelay){
        if (delayTime == 0) {
            // Open the network lobby after chain reaction is set
            showingNetSetupPanel = false;
            showingNetPanel = true;
            networkInLobby = false;
            networkInputMode = 2; // Start with host/join choice
            awaitKp = false;
            runDelay = false;
            lastOptInput = SDLK_UNKNOWN;
        }
        else delayTime--;
    }

    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &voidPanelRct);
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
}

void MainMenu::NetPanelRender() {
    if (!showingNetPanel) return;

    NetworkClient* netClient = NetworkClient::Instance();

    // Auto-complete lobby entry once async WebSocket connection opens (WASM only).
    // On the first Enter press the WebSocket is CONNECTING so SendNick fails; we set
    // pendingLobbyConnect and come back here each frame until state becomes CONNECTED.
    if (pendingLobbyConnect && netClient->GetState() == CONNECTED) {
        SDL_Log("pendingLobbyConnect: WebSocket now CONNECTED, completing lobby entry");
        pendingLobbyConnect = false;
        char nickname[32];
        if (networkPreNick[0] != '\0') {
            snprintf(nickname, sizeof(nickname), "%s", networkPreNick);
        } else {
            const char* envUser = getenv("USER");
            if (envUser && envUser[0] != '\0') snprintf(nickname, sizeof(nickname), "%s", envUser);
            else snprintf(nickname, sizeof(nickname), "unnamed");
        }
        if (netClient->SendNick(nickname)) {
            std::string geoLoc = NetworkClient::DetectGeoLocation();
            float gLat = 0.0f, gLon = 0.0f;
            if (sscanf(geoLoc.c_str(), "%f:%f", &gLat, &gLon) == 2) {
                myGeoLat = gLat; myGeoLon = gLon; myGeoLocSet = true;
            }
            if (netClient->SendGeoLoc(geoLoc.c_str())) {
                networkInLobby = true;
                networkInputMode = 0;
                networkGameStarting = false;
                wasmSyncWaitStart = 0;
                netClient->RequestList();
                lastListRequest = SDL_GetTicks();
#ifdef __ANDROID__
                SDL_AndroidSendMessage(0x8001, 0);
#endif
            }
        }
    }

    // Update network client
    if (netClient->IsConnected()) {
        netClient->Update();

        // Apply any options broadcast by the host (joiners receive SETOPTIONS push)
        {
            bool cr, cl, st; int vl; int pc[5]; bool nc[5]; bool ag[5];
            if (netClient->GetAndClearPendingOptions(cr, cl, st, vl, pc, nc, ag)) {
                chainReactionEnabled = cr;
                continueWhenPlayersLeave = cl;
                singlePlayerTargetting = st;
                // Map vl to victoriesLimitIndex
                static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                victoriesLimitIndex = 5; // default
                for (int i = 0; i < 18; i++) { if (vLimits[i] == vl) { victoriesLimitIndex = i; break; } }
                for (int i = 0; i < 5; i++) { playerColorCounts[i] = pc[i]; playerNoCompress[i] = nc[i]; playerAimGuide[i] = ag[i]; }
                SDL_Log("Applied host options: cr=%d cl=%d st=%d vl=%d colors=%d,%d,%d,%d,%d",
                    cr,cl,st,vl,pc[0],pc[1],pc[2],pc[3],pc[4]);
            }
        }

        // Check if game is ready to start (state transitioned to IN_GAME)
        if (!networkGameStarting && netClient->GetState() == IN_GAME) {
#ifdef __WASM_PORT__
            // WASM joiner: WaitForBubble spins without yielding (no Asyncify),
            // so WebSocket callbacks never fire during SyncNetworkLevel.
            // Wait here (across animation frames) until all 40 sync messages
            // (38 bubbles + N + T) are queued, then SetupNewGame will find
            // them already in the queue and WaitForBubble returns immediately.
            if (!netClient->IsLeader()) {
                if (wasmSyncWaitStart == 0) wasmSyncWaitStart = SDL_GetTicks();
                size_t qSize = netClient->MessageQueueSize();
                bool timedOut = (SDL_GetTicks() - wasmSyncWaitStart > 5000);
                SDL_Log("WASM joiner: waiting for sync msgs, queue=%d, waited=%dms",
                        (int)qSize, (int)(SDL_GetTicks() - wasmSyncWaitStart));
                if (qSize < 40 && !timedOut) {
                    return;  // Come back next frame
                }
                SDL_Log("WASM joiner: proceeding with queue=%d timedOut=%d", (int)qSize, timedOut);
                wasmSyncWaitStart = 0;
            }
#endif
            SDL_Log("Game starting - transitioning to network game");
            networkGameStarting = true;

            // OK_GAME_START is now sent automatically by NetworkClient when receiving GAME_CAN_START

            // Copy network lobby settings to game settings
            chainReaction = chainReactionEnabled;
            SDL_Log("Starting game with chainReaction=%s", chainReaction ? "true" : "false");

            // Start network multiplayer game (mode 4 = network multiplayer)
            SetupNewGame(4); // Use network multiplayer mode
            networkInLobby = false;
            showingNetPanel = false;
            SDL_Log("Set showingNetPanel=false, networkInLobby=false before return");

            // Return immediately to avoid rendering lobby UI after game has started
            return;
        }
    }

    // Additional safety: Don't render lobby UI if game is in progress
    if (netClient && netClient->GetState() == IN_GAME) {
        showingNetPanel = false;
        return;
    }

    // If in lobby, use world map background; otherwise use void panel for connection screens
    if (networkInLobby && netGameBackground && networkInputMode == 0) {
        // Reset text color to white — non-lobby screens (connecting, server list) may have
        // left panelText set to yellow, which persists across frames.
        panelText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

        // Request LIST periodically (every 2 seconds)
        Uint32 now = SDL_GetTicks();
        if (now - lastListRequest > 500) {
            netClient->RequestList();
            lastListRequest = now;
        }

        // Render world map background
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), netGameBackground, nullptr, nullptr);

        // Render geolocation spots on world map (original: print_spot / save_back_spot)
        // Coordinate formula matches original Perl get_spot_location() at line 4084
        auto geoToScreen = [](float lat, float lon) -> SDL_Point {
            const float x0 = 309.0f, y0 = 231.0f;
            const float lonFactor = 1.424f, latFactor = -145.0f;
            float x = x0 + lon * lonFactor;
            float y = y0 + (float)(std::asinh(std::tan((double)lat * 1.4 * 3.14159265358979323846 / 360.0)) * latFactor);
            return {(int)x, (int)y};
        };
        auto renderSpot = [&](SDL_Texture* tex, int x, int y, const char* nick) {
            if (!tex) return;
            int w = 0, h = 0;
            SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
            SDL_Rect r = {x - w / 2, y - h / 2, w, h};
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), tex, nullptr, &r);
            if (nick && nick[0]) {
                networkText.UpdateText(const_cast<SDL_Renderer*>(renderer), nick, 0);
                networkText.UpdatePosition({x - networkText.Coords()->w / 2, y + h / 2 + 1});
                SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), networkText.Texture(), nullptr, networkText.Coords());
            }
        };

        // Draw spots for players in games (rendered first, underneath free players)
        {
            std::vector<GameRoom> games = netClient->GetGameList();
            for (const auto& game : games) {
                for (const auto& p : game.players) {
                    if (p.nick == netClient->GetPlayerNick()) continue;
                    float lat = 0.0f, lon = 0.0f;
                    if (sscanf(p.geoloc.c_str(), "%f:%f", &lat, &lon) != 2) continue;
                    SDL_Point sp = geoToScreen(lat, lon);
                    GameRoom* myGame = netClient->GetCurrentGame();
                    bool inMyGame = myGame && (game.creator == myGame->creator);
                    renderSpot(inMyGame ? netSpotInGame : netSpotPlaying, sp.x, sp.y, p.nick.c_str());
                }
            }
        }

        // Draw spots for free (open) players
        {
            std::vector<NetworkPlayer> openPlayers = netClient->GetOpenPlayers();
            for (const auto& p : openPlayers) {
                if (p.nick == netClient->GetPlayerNick()) continue;
                float lat = 0.0f, lon = 0.0f;
                if (sscanf(p.geoloc.c_str(), "%f:%f", &lat, &lon) != 2) continue;
                SDL_Point sp = geoToScreen(lat, lon);
                renderSpot(netSpotFree, sp.x, sp.y, p.nick.c_str());
            }
        }

        // Draw own animated self spot (top layer)
        if (myGeoLocSet) {
            SDL_Point myPos = geoToScreen(myGeoLat, myGeoLon);
            SDL_Texture* selfTex = netSpotSelf[netSpotSelfFrame];
            renderSpot(selfTex, myPos.x, myPos.y, nullptr);  // No label for self
            if (++netSpotSelfFrameTimer >= 4) {  // ~4 frames at 60fps ≈ 60ms
                netSpotSelfFrameTimer = 0;
                if (++netSpotSelfFrame >= 13) netSpotSelfFrame = 0;
            }
        }

        // Render action list at top left (like original)
        const int actionStartY = 30;
        const int actionStartX = 78;
        const int lineHeight = 16;

        // Action menu: Different options depending on whether in a game
        std::vector<std::string> actions;
        GameRoom* currentGame = netClient->GetCurrentGame();

        if (currentGame) {
            // In a game room - show game options
            actions.push_back("Chat");  // index 0

            // Global settings (indices 1-4) - same for host and joiner
            char crText[64], continueText[64], targetText[64], victoriesText[64];
            snprintf(crText, sizeof(crText), "Chain-reaction: %s", chainReactionEnabled ? "enabled" : "disabled");
            snprintf(continueText, sizeof(continueText), "Continue when players leave: %s", continueWhenPlayersLeave ? "enabled" : "disabled");
            snprintf(targetText, sizeof(targetText), "Single player targetting: %s", singlePlayerTargetting ? "enabled" : "disabled");
            const char* victoriesLimits[] = {"none (unlimited)", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "15", "20", "30", "50", "100"};
            snprintf(victoriesText, sizeof(victoriesText), "Victories limit: %s", victoriesLimits[victoriesLimitIndex]);
            actions.push_back(crText);       // index 1
            actions.push_back(continueText); // index 2
            actions.push_back(targetText);   // index 3
            actions.push_back(victoriesText);// index 4

            // Per-player grid rows (indices 5-7) — label only; values rendered as grid cells below
            actions.push_back("Max colors:"); // index 5
            actions.push_back("Rows:");      // index 6
            actions.push_back("Aim:");       // index 7

            // Start game (index 8) — host only when >1 player
            if (currentGame->creator == netClient->GetPlayerNick() && currentGame->players.size() > 1) {
                actions.push_back("Start game!"); // index 8
            }
            // No "Part game" menu item - use ESC key to leave like original
        } else {
            // In lobby - show create/join options
            actions.push_back("Chat");
            actions.push_back("Create new game");

            std::vector<GameRoom> games = netClient->GetGameList();
            for (const auto& game : games) {
                std::string playerList;
                for (size_t i = 0; i < game.players.size(); i++) {
                    if (i > 0) playerList += ", ";
                    playerList += game.players[i].nick;
                }
                actions.push_back("Join " + game.creator + "'s game: " + playerList);
            }
        }

        // Grid offset: player header + grid rows are shifted down one line to make room for nick header
        const int gridStart = 5;       // First grid row index
        const int gridYOffset = lineHeight * 2; // Gap after Victories limit + space for nick header

        // Render actions with highlight
        for (size_t i = 0; i < actions.size() && i < 15; i++) {
            int renderY = actionStartY + (int)(i * lineHeight) + ((int)i >= gridStart ? gridYOffset : 0);

            if (i == (size_t)selectedActionIndex && highlightServer) {
                SDL_Rect highlightRect = {actionStartX - 4, renderY - 1, 200, lineHeight};
                SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &highlightRect);
            }

            // For grid rows (5-7 in a game room), skip the label text here — rendered as table below
            if (currentGame && (int)i >= gridStart && (int)i <= gridStart + 2) {
                continue;
            }

            char actionText[128];
            snprintf(actionText, sizeof(actionText), "%s", actions[i].c_str());
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), actionText, 0);
            panelText.UpdatePosition({actionStartX, renderY});
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
        }

        // Render the per-player settings grid (only in a game room)
        if (currentGame) {
            int numPlayers = (int)currentGame->players.size();
            if (numPlayers < 1) numPlayers = 1;
            if (numPlayers > 5) numPlayers = 5;
            bool isHost = currentGame->creator == netClient->GetPlayerNick();

            // Column layout
            const int labelW = 110;  // Width of row label ("Max colors:", "Row collapse:", "Aim guide:")
            const int colW   = 36;   // Width of each player column

            // Header row sits exactly one lineHeight above the first data row
            int firstDataRowY = actionStartY + gridStart * lineHeight + gridYOffset;
            int headerY = firstDataRowY - lineHeight;

            // Draw grid lines
            {
                SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
                SDL_SetRenderDrawColor(rend, 180, 180, 200, 220);

                int totalCols = numPlayers + 1; // ALL + P1..PN
                int gridLeft  = actionStartX - 2;
                int gridRight = actionStartX + labelW + totalCols * colW;
                int gridTop   = headerY - 1;
                int gridBot   = firstDataRowY + 3 * lineHeight; // 3 data rows

                // Outer border
                SDL_Rect border = {gridLeft, gridTop, gridRight - gridLeft, gridBot - gridTop};
                SDL_RenderDrawRect(rend, &border);

                // Horizontal line after header row
                SDL_RenderDrawLine(rend, gridLeft, firstDataRowY - 1, gridRight, firstDataRowY - 1);
                // Horizontal lines between data rows (after Colors, after Row collapse)
                for (int r = 1; r <= 2; r++) {
                    int y = firstDataRowY + r * lineHeight;
                    SDL_RenderDrawLine(rend, gridLeft, y, gridRight, y);
                }

                // Vertical line between label and ALL column
                int xLabel = actionStartX + labelW;
                SDL_RenderDrawLine(rend, xLabel, gridTop, xLabel, gridBot);

                // Vertical lines between each column (ALL|P1, P1|P2, ...)
                for (int c = 1; c <= totalCols - 1; c++) {
                    int x = actionStartX + labelW + c * colW;
                    SDL_RenderDrawLine(rend, x, gridTop, x, gridBot);
                }
            }
            // Helper: render text centered within a column cell
            auto renderCentered = [&](const char* txt, int colLeft, int y) {
                SDL_Renderer* rend2 = const_cast<SDL_Renderer*>(renderer);
                panelText.UpdateText(rend2, txt, 0);
                int tw = 0;
                if (panelText.Texture()) SDL_QueryTexture(panelText.Texture(), nullptr, nullptr, &tw, nullptr);
                int cx = colLeft + colW / 2 - tw / 2;
                panelText.UpdatePosition({cx, y});
                SDL_RenderCopy(rend2, panelText.Texture(), nullptr, panelText.Coords());
            };

            // ALL header
            renderCentered("ALL", actionStartX + labelW, headerY);
            // P1..PN headers
            for (int pi = 0; pi < numPlayers; pi++) {
                char pnum[4];
                snprintf(pnum, sizeof(pnum), "P%d", pi + 1);
                renderCentered(pnum, actionStartX + labelW + (pi + 1) * colW, headerY);
            }

            // Grid rows: Colors (5), Rows (6), Aim (7)
            const char* rowLabels[] = {"Max colors:", "Row collapse:", "Aim guide:"};
            for (int row = 0; row < 3; row++) {
                int rowIdx = gridStart + row;
                int rowY   = actionStartY + rowIdx * lineHeight + gridYOffset;

                // Highlight full row if selected
                if (selectedActionIndex == rowIdx && highlightServer) {
                    SDL_Rect hlRect = {actionStartX - 4, rowY - 1, 200, lineHeight};
                    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &hlRect);
                }

                // Row label
                panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), rowLabels[row], 0);
                panelText.UpdatePosition({actionStartX, rowY});
                SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());

                // ALL cell (col 0) — show value if all players match, else "-"
                {
                    int cellX = actionStartX + labelW;
                    bool isFocusedAll = (selectedActionIndex == rowIdx && currentPlayerCol == 0);
                    if (isFocusedAll && isHost && highlightServer) {
                        SDL_Rect cellHl = {cellX - 2, rowY - 1, colW - 2, lineHeight};
                        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &cellHl);
                    }
                    char cellText[8];
                    if (row == 0) {
                        bool same = true;
                        for (int i = 1; i < numPlayers; i++) if (playerColorCounts[i] != playerColorCounts[0]) { same = false; break; }
                        if (same) snprintf(cellText, sizeof(cellText), "%d", playerColorCounts[0]);
                        else snprintf(cellText, sizeof(cellText), "-");
                    } else if (row == 1) {
                        bool same = true;
                        for (int i = 1; i < numPlayers; i++) if (playerNoCompress[i] != playerNoCompress[0]) { same = false; break; }
                        if (same) snprintf(cellText, sizeof(cellText), "%s", playerNoCompress[0] ? "off" : "on");
                        else snprintf(cellText, sizeof(cellText), "-");
                    } else {
                        bool same = true;
                        for (int i = 1; i < numPlayers; i++) if (playerAimGuide[i] != playerAimGuide[0]) { same = false; break; }
                        if (same) snprintf(cellText, sizeof(cellText), "%s", playerAimGuide[0] ? "on" : "off");
                        else snprintf(cellText, sizeof(cellText), "-");
                    }
                    renderCentered(cellText, actionStartX + labelW, rowY);
                }

                // Per-player cells (P1..PN at col 1..N)
                for (int pi = 0; pi < numPlayers; pi++) {
                    int cellX = actionStartX + labelW + (pi + 1) * colW; // +1 to skip ALL column
                    bool isFocusedCell = (selectedActionIndex == rowIdx && currentPlayerCol == pi + 1);

                    // Cell highlight for focused cell (host only)
                    if (isFocusedCell && isHost && highlightServer) {
                        SDL_Rect cellHl = {cellX - 2, rowY - 1, colW - 2, lineHeight};
                        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &cellHl);
                    }

                    // Cell value text
                    char cellText[8];
                    if (row == 0) {
                        snprintf(cellText, sizeof(cellText), "%d", playerColorCounts[pi]);
                    } else if (row == 1) {
                        snprintf(cellText, sizeof(cellText), "%s", playerNoCompress[pi] ? "off" : "on");
                    } else {
                        snprintf(cellText, sizeof(cellText), "%s", playerAimGuide[pi] ? "on" : "off");
                    }
                    renderCentered(cellText, cellX, rowY);
                }
            }
        }

        // If Chat is selected, show inline text input (like original at y=320)
        const int chatY = 320;
        if (selectedActionIndex == 0) {
            char chatText[512];
            snprintf(chatText, sizeof(chatText), "Say: %s_", networkChatInput);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), chatText, 0);
            panelText.UpdatePosition({actionStartX, chatY});
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
        }

        // Display chat messages in status area (like original at y=355-435)
        const int chatStatusX = 10; // Status messages use x=10 in original
        const int chatStatusY = 435; // Bottom of chat area
        const int chatLineHeight = 16;
        const int maxChatLines = 5;
        std::vector<ChatMessage> chatMsgs = netClient->GetChatMessages();

        // Display last 5 messages from bottom up
        int chatLine = 0;
        int startIdx = chatMsgs.size() > maxChatLines ? chatMsgs.size() - maxChatLines : 0;
        for (size_t i = startIdx; i < chatMsgs.size() && chatLine < maxChatLines; i++, chatLine++) {
            char chatLineText[256];
            // Server messages start with ***, regular messages show <nick>
            if (chatMsgs[i].nick == "Server" || chatMsgs[i].message.find("***") == 0) {
                snprintf(chatLineText, sizeof(chatLineText), "*** %s", chatMsgs[i].message.c_str());
            } else {
                snprintf(chatLineText, sizeof(chatLineText), "<%s> %s",
                    chatMsgs[i].nick.c_str(), chatMsgs[i].message.c_str());
            }

            int yPos = chatStatusY - (maxChatLines - 1 - chatLine) * chatLineHeight;
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), chatLineText, 0);
            panelText.UpdatePosition({chatStatusX, yPos});
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
        }

        // Show player nickname and count at y=336 like original (below chat input at y=320)
        const int statusY = 336;
        const int statusX = 78;
        char statusText[256];

        // Show own nickname first
        snprintf(statusText, sizeof(statusText), "Player: %s", netClient->GetPlayerNick().c_str());
        panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), statusText, 0);
        panelText.UpdatePosition({statusX, statusY});
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());

        // Show available players or game info below
        char playersText[256];
        if (currentGame) {
            // Build comma-separated player list
            std::string playerNames;
            for (size_t i = 0; i < currentGame->players.size(); i++) {
                if (i > 0) playerNames += ", ";
                playerNames += currentGame->players[i].nick;
            }
            snprintf(playersText, sizeof(playersText), "Players (%d): %s",
                (int)currentGame->players.size(), playerNames.c_str());
        } else {
            std::vector<NetworkPlayer> openPlayers = netClient->GetOpenPlayers();
            snprintf(playersText, sizeof(playersText), "Available Players: %d", (int)openPlayers.size());
        }
        panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), playersText, 0);
        panelText.UpdatePosition({statusX, statusY + 16});
        SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());

        return;
    }

    // For non-lobby screens, use void panel
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &voidPanelRct);

    char netText[512];

    if (!networkInLobby && networkInputMode == 7) {
        // LAN server list screen
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color black  = {0, 0, 0, 255};
        SDL_Color yellow = {255, 220, 50, 255};
        SDL_Color red    = {255, 80, 80, 255};

        auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
            panelText.UpdateColor(fg, black);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
            y += panelText.Coords()->h;
        };

        int y = (480/2) - 120;
        char lineBuf[280];

        renderLine("LAN Game\n", white, y);

        // Menu item 0: Host a server
        bool hostSel = (lanMenuIndex == 0);
        snprintf(lineBuf, sizeof(lineBuf),
            hostSel ? "[ %s ]" : "  %s  ",
            serverHosting ? "Server running (rescan)" : "Host a server");
        renderLine(lineBuf, hostSel ? yellow : white, y);

        // Menu items 1+: discovered servers (includes 127.0.0.1 if local server running)
        if (discoveredServers.empty()) {
            renderLine("  (no servers found)", white, y);
            renderLine("  Start server with: ./build/server/fb-server -l", white, y);
        } else {
            for (int i = 0; i < (int)discoveredServers.size(); i++) {
                bool sel = (lanMenuIndex == i + 1);
                const std::string& dname = discoveredServers[i].name.empty()
                    ? discoveredServers[i].host + ":" + std::to_string(discoveredServers[i].port)
                    : discoveredServers[i].name;
                char latBuf[16];
                int lat = discoveredServers[i].latencyMs;
                if (lat < 0) snprintf(latBuf, sizeof(latBuf), "offline");
                else         snprintf(latBuf, sizeof(latBuf), "%dms", lat);
                snprintf(lineBuf, sizeof(lineBuf),
                    sel ? "[ %-28s %7s ]" : "  %-28s %7s  ",
                    dname.c_str(), latBuf);
                renderLine(lineBuf, sel ? yellow : white, y);
            }
        }

        // Last menu item: Set Name
        {
            int lanMenuMax = 2 + (int)discoveredServers.size(); // 0=Host, 1..n=servers, n+1=SetName
            bool sel = (lanMenuIndex == lanMenuMax - 1);
            const char* curNick = networkPreNick[0] != '\0' ? networkPreNick : (getenv("USER") ? getenv("USER") : "unnamed");
            snprintf(lineBuf, sizeof(lineBuf), sel ? "[ Set Name: %-20s ]" : "  Set Name: %-20s  ", curNick);
            renderLine(lineBuf, sel ? yellow : white, y);
        }

        snprintf(lineBuf, sizeof(lineBuf), "\nUP/DOWN  ENTER to select  R to rescan\nESC to cancel");
        renderLine(lineBuf, white, y);

        if (!connectErrorMsg.empty()) {
            snprintf(lineBuf, sizeof(lineBuf), "\n%s", connectErrorMsg.c_str());
            renderLine(lineBuf, red, y);
        }

        return;
    }

    if (!networkInLobby && networkInputMode == 10) {
#ifndef __WASM_PORT__
        // Poll background server fetch result
        if (!serverFetchInProgress.load() && publicServers.empty()) {
            std::lock_guard<std::mutex> lock(serverFetchMutex);
            publicServers = std::move(serverFetchResult);
            serverFetchResult.clear();
        }
#endif

        // Net game public server list screen
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color black  = {0, 0, 0, 255};
        SDL_Color yellow = {255, 220, 50, 255};
        SDL_Color red    = {255, 80, 80, 255};
        SDL_Color grey   = {160, 160, 160, 255};

        auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
            panelText.UpdateColor(fg, black);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
            y += panelText.Coords()->h;
        };

        int y = (480/2) - 120;
        char lineBuf[320];

        renderLine("Net Game\n", white, y);

        // Menu item 0: Manual entry
        bool manualSel = (netMenuIndex == 0);
        renderLine(manualSel ? "[ Manual entry ]" : "  Manual entry  ", manualSel ? yellow : white, y);

        // Menu items 1+: public internet servers only
        // (Local LAN server is accessible via the LAN Game panel instead)
        if (serverFetchInProgress.load()) {
            renderLine("  (fetching server list...)", grey, y);
        } else if (publicServers.empty()) {
            renderLine("  (no public servers listed)", white, y);
        } else {
            for (int i = 0; i < (int)publicServers.size(); i++) {
                bool sel = (netMenuIndex == i + 1);
                bool offline = (publicServers[i].latencyMs < 0);
                const std::string& displayName = publicServers[i].name.empty()
                    ? publicServers[i].host + ":" + std::to_string(publicServers[i].port)
                    : publicServers[i].name;
                char latencyBuf[16];
                if (offline) snprintf(latencyBuf, sizeof(latencyBuf), "offline");
                else         snprintf(latencyBuf, sizeof(latencyBuf), "%dms", publicServers[i].latencyMs);
                snprintf(lineBuf, sizeof(lineBuf),
                    sel ? "[ %-28s %7s ]" : "  %-28s %7s  ",
                    displayName.c_str(), latencyBuf);
                SDL_Color col = offline ? grey : (sel ? yellow : white);
                renderLine(lineBuf, col, y);
            }
        }

        // Last menu item: Set Name
        {
            int netMenuMax = 2 + (int)publicServers.size(); // 0=Manual, 1..n=servers, n+1=SetName
            bool sel = (netMenuIndex == netMenuMax - 1);
            const char* curNick = networkPreNick[0] != '\0' ? networkPreNick : (getenv("USER") ? getenv("USER") : "unnamed");
            snprintf(lineBuf, sizeof(lineBuf), sel ? "[ Set Name: %-20s ]" : "  Set Name: %-20s  ", curNick);
            renderLine(lineBuf, sel ? yellow : white, y);
        }

        snprintf(lineBuf, sizeof(lineBuf), "\nUP/DOWN  ENTER to select  R to refresh\nESC to cancel");
        renderLine(lineBuf, white, y);

        if (pendingLobbyConnect) {
            renderLine("\nConnecting...", yellow, y);
        } else if (!connectErrorMsg.empty()) {
            snprintf(lineBuf, sizeof(lineBuf), "\n%s", connectErrorMsg.c_str());
            renderLine(lineBuf, red, y);
        }

        return;
    }

    if (!networkInLobby && networkInputMode == 11) {
        // Pre-lobby nickname input screen
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color black  = {0, 0, 0, 255};
        SDL_Color yellow = {255, 220, 50, 255};

        auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
            panelText.UpdateColor(fg, black);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
            y += panelText.Coords()->h;
        };

        int y = (480/2) - 120;
        char lineBuf[128];

        renderLine("Set Your Name\n", white, y);
        snprintf(lineBuf, sizeof(lineBuf), "[ %s_ ]", networkPreNick);
        renderLine(lineBuf, yellow, y);
        renderLine("\nPress ENTER to confirm\nPress ESC to cancel", white, y);
        return;
    }

    if (!networkInLobby) {
        // Connection screen — render in segments so active field can be colored
        const char* titleStr = serverHosting ? "Hosting Server" : "Join Server";
        bool hostActive = networkManualFieldIndex == 0;
        bool portActive = networkManualFieldIndex == 1;
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color black  = {0, 0, 0, 255};
        SDL_Color yellow = {255, 220, 50, 255};

        auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
            panelText.UpdateColor(fg, black);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
            SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
            y += panelText.Coords()->h;
        };

        int y = (480/2) - 120;
        char lineBuf[280];

        snprintf(lineBuf, sizeof(lineBuf), "%s\n\n", titleStr);
        renderLine(lineBuf, white, y);

        bool connectActive = (!networkFieldEditing && networkManualFieldIndex == 2);

        snprintf(lineBuf, sizeof(lineBuf), hostActive && networkFieldEditing ? "Host: [ %s_ ]" : "Host:   %s  ", networkHost);
        renderLine(lineBuf, hostActive ? yellow : white, y);

        snprintf(lineBuf, sizeof(lineBuf), portActive && networkFieldEditing ? "Port: [ %d_ ]" : "Port:   %d  ", networkPort);
        renderLine(lineBuf, portActive ? yellow : white, y);

        snprintf(lineBuf, sizeof(lineBuf), connectActive ? "[ Connect ]" : "  Connect  ");
        renderLine(lineBuf, connectActive ? yellow : white, y);

        if (networkFieldEditing) {
            snprintf(lineBuf, sizeof(lineBuf), "\n%sENTER to confirm  ESC to cancel",
                serverHosting ? "Server running...\n\n" : "\n");
        } else {
            snprintf(lineBuf, sizeof(lineBuf), "\n%sUP/DOWN to navigate  ENTER to select\nESC to go back",
                serverHosting ? "Server running...\n\n" : "\n");
        }
        renderLine(lineBuf, white, y);

        if (!connectErrorMsg.empty()) {
            SDL_Color red = {255, 80, 80, 255};
            snprintf(lineBuf, sizeof(lineBuf), "\n%s", connectErrorMsg.c_str());
            renderLine(lineBuf, red, y);
        }

        return;
    } else if (networkInputMode == 3) {
        // Join game input screen
        snprintf(netText, sizeof(netText),
            "Join Game\n\n"
            "Enter the creator's nickname:\n\n"
            "%s_\n\n"
            "Press ENTER to join\n"
            "Press ESC to cancel",
            networkJoinCreator);
    } else if (networkInputMode == 4) {
        // Chat input screen
        snprintf(netText, sizeof(netText),
            "Send Chat Message\n\n"
            "%s_\n\n"
            "Press ENTER to send\n"
            "Press ESC to cancel",
            networkChatInput);
    } else if (networkInputMode == 5) {
        // Username input screen
        snprintf(netText, sizeof(netText),
            "Change Username\n\n\n"
            "Enter your nickname:\n\n"
            "%s_\n\n"
            "Current: %s\n\n"
            "Press ENTER to change\n"
            "Press ESC to cancel",
            networkUsername,
            netClient->GetPlayerNick().c_str());
    } else if (networkInputMode == 6) {
        // Create game confirmation screen
        snprintf(netText, sizeof(netText),
            "Create New Game\n\n\n"
            "Create a new game room?\n\n"
            "Game Name: %s's game\n"
            "Max Players: 5\n\n\n"
            "Press ENTER to create\n"
            "Press ESC to cancel",
            netClient->GetPlayerNick().c_str());
    } else {
        // Main lobby screen with game list
        const char* stateStr = "Disconnected";
        if (netClient->IsConnected()) {
            // Request LIST periodically (every 2 seconds)
            Uint32 now = SDL_GetTicks();
            Uint32 timeSinceLastRequest = now - lastListRequest;
            if (timeSinceLastRequest > 500) {
                netClient->RequestList();
                lastListRequest = now;
            }

            switch (netClient->GetState()) {
                case CONNECTED: stateStr = "Lobby"; break;
                case IN_LOBBY: stateStr = "In Game"; break;
                case IN_GAME: stateStr = "Playing"; break;
                default: stateStr = "Connected"; break;
            }
        } else {
        }

        // Build lobby display
        char lobbyText[2048];
        int offset = 0;

        // Title and status
        offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
            "=== Frozen Bubble Network Lobby ===\n\n"
            "Player: %s  |  Status: %s%s\n\n",
            netClient->GetPlayerNick().c_str(),
            stateStr,
            serverHosting ? "  |  Hosting" : "");

        // Get game list for later use
        std::vector<GameRoom> games = netClient->GetGameList();
        std::vector<NetworkPlayer> openPlayers = netClient->GetOpenPlayers();

        // Show current game if in one
        GameRoom* currentGame = netClient->GetCurrentGame();
        if (currentGame) {
            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "Current Game: %s's game\n",
                currentGame->creator.c_str());

            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "Players: ");
            for (size_t i = 0; i < currentGame->players.size(); i++) {
                offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                    "%s%s",
                    currentGame->players[i].nick.c_str(),
                    i < currentGame->players.size() - 1 ? ", " : "");
            }
            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset, "\n\n");
        } else {
            // Show available games
            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "Available Games (%d):\n", (int)games.size());

            if (games.empty()) {
                offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                    "  (No games available)\n");
            } else {
                for (size_t i = 0; i < games.size() && i < 5; i++) {
                    // Build player names string
                    std::string playerNames;
                    for (size_t j = 0; j < games[i].players.size(); j++) {
                        if (j > 0) playerNames += ", ";
                        playerNames += games[i].players[j].nick;
                    }
                    offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                        "  %s [%s]: %s\n",
                        i == (size_t)selectedGameIndex ? ">" : " ",
                        games[i].creator.c_str(),
                        playerNames.c_str());
                }
            }

            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "\nIn Lobby (%d):\n", (int)openPlayers.size());

            if (openPlayers.empty()) {
                offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                    "  (No players)\n");
            } else {
                for (size_t i = 0; i < openPlayers.size() && i < 8; i++) {
                    offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                        "  %s\n", openPlayers[i].nick.c_str());
                }
            }
        }

        // Recent chat messages
        std::vector<ChatMessage> chatMsgs = netClient->GetChatMessages();
        offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset, "\nChat:\n");

        int chatStart = chatMsgs.size() > 3 ? chatMsgs.size() - 3 : 0;
        for (size_t i = chatStart; i < chatMsgs.size(); i++) {
            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "<%s> %s\n",
                chatMsgs[i].nick.c_str(),
                chatMsgs[i].message.c_str());
        }

        offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
            "\n%sC=Create J=Join%s T=Chat%s",
            !games.empty() && !currentGame ? "UP/DOWN=Select  " : "",
            currentGame ? " S=Start P=Part" : "",
            currentGame ? "" : "\nESC=Disconnect  /nick to rename");

        strncpy(netText, lobbyText, sizeof(netText) - 1);
        netText[sizeof(netText) - 1] = '\0';
    }

    panelText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
    panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), netText, 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
    SDL_RenderCopy(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, panelText.Coords());
}

void MainMenu::press() {
    if (showingOptPanel || showing2PPanel || showingLocalMPPanel || showingKeysPanel || showingNetSetupPanel || showingLevelPanel) return;
    AudioMixer::Instance()->PlaySFX("menu_selected");

    if (showingSPPanel) {
        if (activeSPIdx == 0) SetupNewGame(1);
        else if (activeSPIdx == 1) {
            // Pick start level: open number input panel
            showingLevelPanel = true;
            levelInput.clear();
            runDelay = false;
            SDL_StartTextInput();
        }
        else if (activeSPIdx == 2) ShowPanel(1);
        else if (activeSPIdx == 3) {
            // mp_training: ask chain reaction then start
            showingSPPanel = false;
            showingOptPanel = awaitKp = true;
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer),
                "Multiplayer training\n\n\nEnable chain reaction?\n\n\nY or N?:          \n"
#ifdef __ANDROID__
                "\n\n[R] Remove Ads"
#endif
                , 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
            selectedMode = 6;  // mode 6 = mp_training
        }
        else if (activeSPIdx == 4) {
            // Local multiplayer: open sub-panel to pick player count
            showingSPPanel = false;
            showingLocalMPPanel = true;
            localMPMenuIndex = 0;
            localMPPlayerCount = 2;
            localMPCR = true;
            runDelay = false;
        }
        return;
    }

    buttons[active_button_index].Pressed(this);
}

void MainMenu::down()
{
    if (showingOptPanel || showing2PPanel || showingLocalMPPanel || showingKeysPanel || showingNetSetupPanel || showingLevelPanel) return;
    AudioMixer::Instance()->PlaySFX("menu_change");

    if (showingSPPanel) {
        if (activeSPIdx == SP_OPT - 1) activeSPIdx = 0;
        else activeSPIdx++;
        restartOverlook(overlookSfc, overlookIndex);
        return;
    }

    buttons[active_button_index].Deactivate();
    if(active_button_index == (buttons.size() - 1)) {
        active_button_index = 0;
    } else {
        active_button_index++;
    }

    buttons[active_button_index].Activate();
}

void MainMenu::up()
{
    if (showingOptPanel || showing2PPanel || showingLocalMPPanel || showingKeysPanel || showingNetSetupPanel || showingLevelPanel) return;
    AudioMixer::Instance()->PlaySFX("menu_change");

    if (showingSPPanel) {
        if (activeSPIdx == 0) activeSPIdx = SP_OPT - 1;
        else activeSPIdx--;
        restartOverlook(overlookSfc, overlookIndex);
        return;
    }

    buttons[active_button_index].Deactivate();

    if(active_button_index == 0) {
        active_button_index = buttons.size() - 1;
    } else {
        active_button_index--;
    }

    buttons[active_button_index].Activate();
}

void MainMenu::ShowPanel(int which) {
    switch (which){
        case 0: // singleplayer menu
            showingSPPanel = true;
            panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), "Start 1-player game menu", 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
            break;
        case 1: // random level
            showingSPPanel = false;
            showingOptPanel = awaitKp = true;
            panelText.UpdateText(const_cast<SDL_Renderer *>(renderer),
                "Random level\n\n\nEnable chain reaction?\n\n\nY or N?:          \n"
#ifdef __ANDROID__
                "\n\n[R] Remove Ads"
#endif
                , 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
            selectedMode = 3;
            break;
        case 2: // local multiplayer (2-4 players same keyboard/controller)
            showingLocalMPPanel = true;
            localMPMenuIndex = 0;
            localMPPlayerCount = 2;
            localMPCR = true;
            runDelay = false;
            selectedMode = 7;
            break;
        case 3: { // LAN game - discover servers via UDP broadcast
            isLANGame = true;
            discoveredServers = NetworkClient::DiscoverLANServers();
            // Also add localhost:1511 if server is running locally
            bool foundLocal = false;
            for (const auto& s : discoveredServers) {
                if (s.host == "127.0.0.1" || s.host == "localhost") {
                    foundLocal = true;
                    break;
                }
            }
            if (!foundLocal) {
                // Check if local server is listening on port 1511
                if (portInUse(1511)) {
                    ServerInfo localServer;
                    localServer.host = "127.0.0.1";
                    localServer.port = 1511;
                    localServer.name = "Local Server";
                    localServer.latencyMs = 0;
                    discoveredServers.push_back(localServer);
                }
            }
            for (auto& s : discoveredServers)
                s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
            selectedServerIndex = 0;
            lanMenuIndex = 0;
            connectErrorMsg.clear();
            showingNetPanel = true;
            networkInLobby = false;
            networkInputMode = 7; // LAN server list
            break;
        }
        case 5: { // Net game - fetch public server list + local server
            isLANGame = false;
            netMenuIndex = 0;
            connectErrorMsg.clear();
            showingNetPanel = true;
            networkInLobby = false;
            networkInputMode = 10; // Public server list
            publicServers.clear();
#ifdef __WASM_PORT__
            // WASM: FetchPublicServers() returns instantly (hardcoded list); no thread needed
            publicServers = NetworkClient::FetchPublicServers();
#else
            // Native: fetch + latency probe can block for seconds — run on background thread
            if (!serverFetchInProgress.load()) {
                serverFetchInProgress = true;
                if (serverFetchThread.joinable()) serverFetchThread.join();
                serverFetchThread = std::thread([this]() {
                    std::vector<ServerInfo> fetched = NetworkClient::FetchPublicServers();
                    // Add local server if running
                    bool foundLocal = false;
                    for (const auto& s : fetched)
                        if (s.host == "127.0.0.1" || s.host == "localhost") { foundLocal = true; break; }
                    if (!foundLocal && portInUse(1511)) {
                        ServerInfo localServer;
                        localServer.host = "127.0.0.1";
                        localServer.port = 1511;
                        localServer.name = "Local Server";
                        localServer.latencyMs = 0;
                        fetched.insert(fetched.begin(), localServer);
                    }
                    for (auto& s : fetched)
                        s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
                    std::lock_guard<std::mutex> lock(serverFetchMutex);
                    serverFetchResult = std::move(fetched);
                    serverFetchInProgress = false;
                });
            }
#endif
            break;
        }
        case 4: // keys configuration
            showingKeysPanel = true;
            keyConfigPlayer = 1;
            keyConfigIndex = 0;
            break;
        case 6:
            HighscoreManager::Instance()->ShowScoreScreen(0);
            break;
        default:
            break;
    }
}

void MainMenu::SetupNewGame(int mode) {
    TransitionManager::Instance()->DoSnipIn(const_cast<SDL_Renderer*>(renderer));
    switch(mode){
        case 1:
            FrozenBubble::Instance()->bubbleGame()->NewGame({chainReaction, 1, false});
            break;
        case 2: {
            SetupSettings ns2p;
            ns2p.chainReaction = chainReaction;
            ns2p.playerCount = 2;
            ns2p.randomLevels = true;
            for (int i = 0; i < 5; i++) ns2p.playerColors[i] = playerColorCounts[i];
            FrozenBubble::Instance()->bubbleGame()->NewGame(ns2p);
            break;
        }
        case 3:
            FrozenBubble::Instance()->bubbleGame()->NewGame({chainReaction, 1, false, true});
            break;
        case 4: { // Network multiplayer (dynamic player count based on game room)
            NetworkClient* netClient = NetworkClient::Instance();
            int playerCount = 2; // Default to 2 players

            // Get actual player count from current game room
            if (netClient && netClient->GetCurrentGame()) {
                playerCount = (int)netClient->GetCurrentGame()->players.size();
                SDL_Log("Starting network game with %d players from game room", playerCount);
                SDL_Log("Players in room:");
                for (size_t i = 0; i < netClient->GetCurrentGame()->players.size(); i++) {
                    SDL_Log("  Player %zu: %s", i, netClient->GetCurrentGame()->players[i].nick.c_str());
                }
            } else {
                SDL_Log("Warning: No current game, defaulting to 2 players");
            }

            SDL_Log("Calling NewGame with playerCount=%d, chainReaction=%s", playerCount, chainReaction ? "true" : "false");
            {
                SetupSettings ns;
                ns.chainReaction = chainReaction;
                ns.playerCount = playerCount;
                ns.networkGame = true;
                ns.randomLevels = true;
                ns.singlePlayerTargetting = singlePlayerTargetting;
                static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                ns.victoriesLimit = vLimits[victoriesLimitIndex];
                for (int i = 0; i < 5; i++) {
                    ns.playerColors[i] = playerColorCounts[i];
                    ns.disableCompression[i] = playerNoCompress[i];
                    ns.aimGuide[i] = playerAimGuide[i];
                }
                FrozenBubble::Instance()->bubbleGame()->NewGame(ns);
            }
            break;
        }
        case 5: // Pick start level
            FrozenBubble::Instance()->bubbleGame()->NewGame({chainReaction, 1, false, false, false, pickedStartLevel});
            break;
        case 6: // Multiplayer training
            FrozenBubble::Instance()->bubbleGame()->NewGame({chainReaction, 1, false, true, false, 1, true});
            break;
        case 7: { // Local multiplayer (controller-based, 2-4 players)
            SetupSettings ns7;
            ns7.chainReaction = localMPCR;
            ns7.playerCount = localMPPlayerCount;
            ns7.randomLevels = true;
            ns7.localMultiplayer = true;
            for (int i = 0; i < 5; i++) {
                ns7.playerColors[i] = playerColorCounts[i];
                ns7.disableCompression[i] = localMPNoCompress;
                ns7.aimGuide[i] = localMPAimGuide[i];
            }
            FrozenBubble::Instance()->bubbleGame()->NewGame(ns7);
            break;
        }
        default:
            break;
    }
}

void MainMenu::ReturnToMenu() {
    SDL_Log("!!! ReturnToMenu() called - currentState changing to TitleScreen");
    AudioMixer::Instance()->PlayMusic("intro");
    FrozenBubble::Instance()->currentState = TitleScreen;
    candyIndex = 0;
    bannerCurpos = 0;
    showingSPPanel = false;
    showing2PPanel = false;
    showingLocalMPPanel = false;
    showingOptPanel = false;
    showingLevelPanel = false;
    showingNetPanel = false;
    networkInLobby = false;
    awaitKp = false;
    selectedMode = 0;
    runDelay = false;
    SDL_StopTextInput();

    // Clean up server if running
    if (serverHosting) {
        StopLocalServer();
    }
}

void MainMenu::ReturnToNetLobby() {
    SDL_Log("ReturnToNetLobby() called - returning to network lobby");

    // Clear the current game room data so it doesn't show stale info
    NetworkClient* netClient = NetworkClient::Instance();
    if (netClient) {
        GameRoom* currentGame = netClient->GetCurrentGame();
        if (currentGame) {
            currentGame->players.clear();
            currentGame->creator.clear();
            currentGame->started = false;
        }
        // If somehow still IN_GAME (BubbleGame normally calls PartGame first), clean up
        if (netClient->GetState() == IN_GAME) {
            netClient->PartGame();
        }
    }

    FrozenBubble::Instance()->currentState = TitleScreen;
    showingNetPanel = true;
    networkInLobby = true;
    networkInputMode = 0;
    networkGameStarting = false;
    wasmSyncWaitStart = 0;
    pendingLobbyConnect = false;
    SDL_StopTextInput();

    // Clear stale game list immediately so ESC-quitter can't see/join the in-progress game
    // Fresh list arrives shortly from RequestList()
    if (netClient) netClient->ClearGameList();

    // Request a fresh game/player list so lobby shows current state
    if (netClient && netClient->IsConnected())
        netClient->RequestList();
#ifdef __ANDROID__
    SDL_AndroidSendMessage(0x8001, 0); // show lobby ad on return from game
#endif
}

void MainMenu::StartLocalServer() {
#if defined(__ANDROID__) || defined(__WASM_PORT__) || defined(_WIN32)
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot host server on this platform");
    connectErrorMsg = "Server hosting not available on this platform";
    return;
#else
    if (serverHosting) {
        SDL_Log("Server already running");
        return;
    }

    // If port is already in use by a leftover process, kill it first
    if (portInUse(networkPort)) {
        SDL_Log("Port %d already in use — killing orphaned fb-server...", networkPort);
        system("pkill -x fb-server 2>/dev/null");
        SDL_Delay(300);
        if (portInUse(networkPort)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Port %d still in use after kill attempt", networkPort);
            connectErrorMsg = "Port " + std::to_string(networkPort) + " is already in use";
            return;
        }
    }

    SDL_Log("Starting local server on port %d...", networkPort);

    // Fork process to run server
    pid_t pid = fork();

    if (pid < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to fork server process: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        // Child process - run the server
        char portStr[16];
        snprintf(portStr, sizeof(portStr), "%d", networkPort);

        // Try to find server binary
        const char* serverPaths[] = {
            "server/fb-server",           // From build directory
            "./server/fb-server",
            "../server/fb-server",
            "../../server/fb-server",
            "./build/server/fb-server",
            "/usr/local/bin/fb-server",
            NULL
        };

        // Try each path
        for (int i = 0; serverPaths[i] != NULL; i++) {
            // Check if file exists before trying to exec
            if (access(serverPaths[i], X_OK) == 0) {
                execl(serverPaths[i], "fb-server", "-q", "-d", "-z", "-l", "-p", portStr, (char*)NULL);
            }
        }

        // If we get here, exec failed - write to stderr so parent can see
        fprintf(stderr, "ERROR: Failed to start server - binary not found or not executable\n");
        fprintf(stderr, "Searched paths:\n");
        for (int i = 0; serverPaths[i] != NULL; i++) {
            fprintf(stderr, "  %s: %s\n", serverPaths[i], access(serverPaths[i], X_OK) == 0 ? "found" : "not found");
        }
        exit(1);
    }

    // Parent process
    serverPid = pid;
    serverHosting = true;

    // Set host to localhost since we're hosting
    strcpy(networkHost, "127.0.0.1");

    // Give server time to start
    SDL_Log("Waiting for server to initialize...");
    SDL_Delay(1000);

    // Check if child process is still running
    int status;
    pid_t result = waitpid(serverPid, &status, WNOHANG);
    if (result != 0) {
        // Child exited immediately - server failed to start
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Server failed to start (child process exited)");
        serverPid = -1;
        serverHosting = false;
        return;
    }

    SDL_Log("Server started with PID %d on port %d", serverPid, networkPort);
#endif // !_WIN32 && !__ANDROID__ && !__WASM_PORT__
}

void MainMenu::StopLocalServer() {
#if defined(__ANDROID__) || defined(__WASM_PORT__) || defined(_WIN32)
    return;
#else
    if (!serverHosting || serverPid <= 0) {
        return;
    }

    SDL_Log("Stopping local server (PID %d)...", serverPid);

    kill(serverPid, SIGTERM);

    int status;
    waitpid(serverPid, &status, WNOHANG);

    serverPid = -1;
    serverHosting = false;

    SDL_Log("Server stopped");
#endif
}
