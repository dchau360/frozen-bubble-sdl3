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

#ifndef MAINMENU_H
#define MAINMENU_H

#include <SDL2/SDL.h>
#include <vector>

#include "menubutton.h"
#include "networkclient.h"
#include "shaderstuff.h"
#include "ttftext.h"

#pragma region "banner_defines"
#define BANNER_START 1000
#define BANNER_SPACING 80
#define BANNER_MINX 304
#define BANNER_MAXX 596
#define BANNER_Y 243
#define BANNER_SLOWDOWN 1
#pragma endregion

#define BLINK_FRAMES 5
#define BLINK_SLOWDOWN 30

#define SP_OPT 5

class MainMenu final
{
public:
    MainMenu(const SDL_Renderer *renderer);
    MainMenu(const MainMenu&) = delete;
    ~MainMenu();
    void Render(void);
    void RefreshCandy();
    void HandleInput(SDL_Event *e);
    void SetupNewGame(int mode);
    void ShowPanel(int which);
    void ReturnToMenu();
    void ReturnToNetLobby();  // Return to network lobby after quitting a network game
private:
    const SDL_Renderer *renderer;
    std::vector<MenuButton> buttons;
    SDL_Texture *background;

    //candy
    SDL_Rect fb_logo_rect, candy_fb_rect;
    SDL_Texture *fbLogo;
    TextureEx candyOrig, candyModif, logoMask;
    int candyIndex = 0;
    int candyMethod = -1;
    bool candyInit = false;
    void InitCandy();
    
    //banner
    SDL_Rect banner_rect;
    SDL_Texture *bannerArtwork, *bannerCPU, *bannerLevel, *bannerSound;
    SDL_Texture *blinkGreenL, *blinkGreenR, *blinkPurpleL, *blinkPurpleR;
    int bannerFU = BANNER_SLOWDOWN;
    int bannerFormulas[4];
    int bannerMax = 0, bannerCurpos = 0;

    //blink
    SDL_Rect blink_green_left, blink_green_right, blink_purple_left, blink_purple_right;
    int blinkGreen = 0, blinkPurple = 0, waitGreen = 0, waitPurple = 0;

    //rest
    uint8_t active_button_index;

    void press();
    void up();
    void down();

    void BlinkRender();
    void BannerRender();
    void CandyRender();

    TTFText panelText;

    //singleplayer panel
    SDL_Texture *singlePanelBG;
    SDL_Texture *singleButtonAct, *singleButtonIdle;
    SDL_Surface *activeSPButtons[SP_OPT];
    SDL_Surface *overlookSfc = nullptr;
    SDL_Texture *idleSPButtons[SP_OPT];
    int activeSPIdx = 0, overlookIndex = 0;
    bool showingSPPanel = false, showing2PPanel = false;
    const struct spPanelEntry {
        std::string option;
        int pivot;
    } spOptions[SP_OPT] = {{"play_all_levels", 90}, {"pick_start_level", 135}, {"play_random_levels", 82}, {"multiplayer_training", 105}, {"local_multiplayer", 100}};
    
    SDL_Rect voidPanelRct = {(640/2) - (341/2), (480/2) - (280/2), 341, 280};
    void SPPanelRender();
    void TPPanelRender();

    //Options panel render
    bool showingOptPanel = false, awaitKp = false, runDelay = false;
    int delayTime;
    SDL_Keycode lastOptInput = SDLK_UNKNOWN;
    SDL_Texture *voidPanelBG;
    void OptPanelRender();

    // Pick start level panel
    bool showingLevelPanel = false;
    std::string levelInput;
    int pickedStartLevel = 1;
    void LevelPanelRender();

    //Keys panel render
    bool showingKeysPanel = false;
    int keyConfigPlayer = 1; // 1 or 2
    int keyConfigIndex = 0; // 0=left, 1=right, 2=fire, 3=center
    void KeysPanelRender();

    // LAN server discovery
    bool isLANGame = false;
    std::vector<ServerInfo> discoveredServers;
    int selectedServerIndex = 0;
    int lanMenuIndex = 0; // 0 = "Host a server", 1+ = discovered servers

    // Net game public server list (mode 10)
    std::vector<ServerInfo> publicServers;
    int netMenuIndex = 0; // 0 = "Manual entry", 1+ = public servers

    //Network panel render
    bool showingNetPanel = false;
    bool showingNetSetupPanel = false; // For chain reaction prompt before network lobby
    bool networkInLobby = false;
    bool networkGameStarting = false; // Track if game start has been initiated
    bool serverHosting = false;
    int serverPid = -1;
    char networkHost[256] = "127.0.0.1";
    int networkPort = 1511;
    int networkInputMode = 0; // 0 = lobby main, 1 = port input, 2 = host/join choice, 3 = join game, 4 = chat input, 5 = username input, 6 = create confirm, 11 = pre-lobby nickname
    std::string connectErrorMsg;
    char networkJoinCreator[32] = "";
    char networkChatInput[256] = "";
    char networkUsername[32] = "";
    char networkPreNick[32] = "";   // Nickname set before connecting (on server selection screen)
    int networkPreNickReturnMode = 7; // Mode to return to after editing pre-lobby nickname
    int selectedGameIndex = -1; // For game list selection
    int selectedActionIndex = 0; // Currently selected action in lobby (0=chat, 1=create, 2+=games)
    Uint32 lastListRequest = 0; // Timestamp of last LIST request
    TTFText networkText;

    // Game settings (when hosting)
    bool chainReactionEnabled = true;
    bool continueWhenPlayersLeave = true;
    bool singlePlayerTargetting = true;
    int victoriesLimitIndex = 5; // 0=none, 1=1, 2=2, 3=3, 4=4, 5=5, etc.
    int playerColorCounts[5] = {7, 7, 7, 7, 7};  // Per-player color count (5-8)

    // World map lobby graphics
    SDL_Texture *netGameBackground = nullptr;
    SDL_Texture *netSpotFree = nullptr;
    SDL_Texture *netSpotInGame = nullptr;
    SDL_Texture *netSpotPlaying = nullptr;
    SDL_Texture *netSpotSelf[13]; // Animated self spot
    SDL_Texture *highlightServer = nullptr;

    void NetPanelRender();
    void NetSetupPanelRender(); // Chain reaction prompt for network games
    void StartLocalServer();
    void StopLocalServer();

    //game setup defines
    bool chainReaction = false;
    int selectedMode;

    // 2-player game setup menu
    int twoPlayerMenuIndex = 0; // Currently selected menu item in 2P setup
    bool twoPlayerCR = true; // Chain reaction for 2P game
    int twoPlayerVictoriesIndex = 5; // Victories limit for 2P game (same as network)

    // Local multiplayer setup panel
    bool showingLocalMPPanel = false;
    int localMPMenuIndex = 0;   // 0=player count, 1=chain reaction, 2=start
    int localMPPlayerCount = 2; // 2-5 players
    bool localMPCR = true;      // Chain reaction enabled
    void LocalMPPanelRender();
};

#endif // MAINMENU_H
