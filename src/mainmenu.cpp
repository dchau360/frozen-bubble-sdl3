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

#include "mainmenu.h"
#include "audiomixer.h"
#include "frozenbubble.h"
#include "transitionmanager.h"
#include "networkclient.h"
#include "platform.h"

#include <SDL3_image/SDL_image.h>
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
#ifdef __WASM_PORT__
#include <emscripten.h>
#include <stdlib.h>
#endif

#include "mainmenu_internal.h"

struct ButtonId {
    std::string buttonName;
    std::string iconName;
    int iconFrames;
};

SDL_Point GetSize(SDL_Texture *texture){
    float fw, fh;
    SDL_GetTextureSize(texture, &fw, &fh);
    return SDL_Point{(int)fw, (int)fh};
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
    panelText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    panelText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    networkText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    networkText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_LEFT);
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

    // Restore last used nickname
#ifdef __WASM_PORT__
    {
        char* wasmNick = (char*)EM_ASM_PTR({
            var n = localStorage.getItem('fb_nickname');
            if (!n || n.length === 0) return 0;
            var len = lengthBytesUTF8(n) + 1;
            var buf = _malloc(len);
            stringToUTF8(n, buf, len);
            return buf;
        });
        if (wasmNick) {
            snprintf(networkPreNick, sizeof(networkPreNick), "%s", wasmNick);
            free(wasmNick);
        }
    }
#else
    {
        const char* saved = GameSettings::Instance()->savedNickname;
        if (saved[0] != '\0') snprintf(networkPreNick, sizeof(networkPreNick), "%s", saved);
    }
#endif
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
        SDL_DestroySurface(candyOrig.sfc);
        candyOrig.LoadFromSurface(candyModif.sfc, const_cast<SDL_Renderer*>(renderer));
        candy_fb_rect.w = candyOrig.sfc->w;
        candy_fb_rect.h = candyOrig.sfc->h;
    }
    else if (candyMethod == 4) { // tilt
        candy_fb_rect.x -= (int)(fb_logo_rect.w * 0.05);
        candy_fb_rect.y -= (int)(fb_logo_rect.h * 0.025);
        tmpRct = {(int)(fb_logo_rect.w * 0.05), (int)(fb_logo_rect.h * 0.025), (int)(fb_logo_rect.w * 1.1), (int)(fb_logo_rect.h * 1.05)};
        candyModif.LoadEmptyAndApply(&tmpRct, const_cast<SDL_Renderer*>(renderer), ASSET("/gfx/menu/fblogo.png").c_str());
        SDL_DestroySurface(candyOrig.sfc);
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
        SDL_DestroySurface(candyOrig.sfc);
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


void MainMenu::SelectAndPressButton(int idx) {
    if (idx < 0 || idx >= (int)buttons.size()) return;
    buttons[active_button_index].Deactivate();
    active_button_index = idx;
    buttons[active_button_index].Activate();
    buttons[active_button_index].Pressed(this);
    AudioMixer::Instance()->PlaySFX("menu_selected");
}


void MainMenu::SavePreNick() {
    // Persist the pre-lobby nickname so it survives even if the user doesn't connect
    if (networkPreNick[0] == '\0') return;
#ifdef __WASM_PORT__
    EM_ASM({ localStorage.setItem('fb_nickname', UTF8ToString($0)); }, networkPreNick);
#else
    GameSettings* gsn = GameSettings::Instance();
    snprintf(gsn->savedNickname, sizeof(gsn->savedNickname), "%s", networkPreNick);
    gsn->SaveKeys();
#endif
}


void restartOverlook(SDL_Surface *overlookSfc, int &overlookIndex){
    if(GameSettings::Instance()->gfxLevel() > 2) return;
    overlook_init_(overlookSfc);
    overlookIndex = 0;
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
            SDL_StartTextInput(SDL_GetKeyboardFocus());
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
            netRoomMouseEnabled = GameSettings::Instance()->mouseEnabled; // load persisted default
            break;
        }
        case 5: { // Net game - fetch public server list + local server
            isLANGame = false;
            netMenuIndex = 0;
            connectErrorMsg.clear();
            showingNetPanel = true;
            networkInLobby = false;
            networkInputMode = 10; // Public server list
            netRoomMouseEnabled = GameSettings::Instance()->mouseEnabled; // load persisted default
            publicServers.clear();
#ifdef __WASM_PORT__
            // WASM: FetchPublicServers() returns instantly (hardcoded list); no thread needed
            publicServers = NetworkClient::FetchPublicServers();
            if (!publicServers.empty()) netMenuIndex = 1; // pre-select first server
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
            // Classic campaign ("Play All Levels") always has chain reaction off,
            // per bin/frozen-bubble ~3327 ($chainreaction = 0 unless level is
            // 'random' or 'mp_train'). `chainReaction` is a scratch member written
            // by other modes' own Y/N prompts (2P setup, Random Levels, MP
            // Training, Network); this path shows no such prompt, so it must not
            // read their leftover value.
            FrozenBubble::Instance()->bubbleGame()->NewGame({false, 1, false});
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
                // Apply per-session mouse setting (off by default in multiplayer)
                GameSettings::Instance()->mouseEnabled = netRoomMouseEnabled;
                FrozenBubble::Instance()->bubbleGame()->NewGame(ns);
            }
            break;
        }
        case 5: // Pick start level
            // Same classic-campaign rule as case 1: a specific numbered level is
            // not "random", so chain reaction must always be off here too, not
            // whatever leftover value another mode's prompt left in `chainReaction`.
            FrozenBubble::Instance()->bubbleGame()->NewGame({false, 1, false, false, false, pickedStartLevel});
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
    SDL_StopTextInput(SDL_GetKeyboardFocus());

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
    SDL_StopTextInput(SDL_GetKeyboardFocus());

    // Clear stale game list immediately so ESC-quitter can't see/join the in-progress game
    // Fresh list arrives shortly from RequestList()
    if (netClient) netClient->ClearGameList();

    // Request a fresh game/player list so lobby shows current state
    if (netClient && netClient->IsConnected())
        netClient->RequestList();
#ifdef __ANDROID__
    SDL_SendAndroidMessage(0x8001, 0); // show lobby ad on return from game
#endif
}

