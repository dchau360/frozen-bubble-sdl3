/*
 * Frozen-Bubble SDL3 C++ Port
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

// The online game room in the Modern menu style (mainmenu_roommodern.cpp):
// the focused rule row carries the ice edge, a tap on a stepped value's right
// half steps it forward, Start / Set Teams / HELP / the chat line are tap
// targets, and in a 20-seat room a tap on a seat opens Set Teams on it. Also
// that the classic room still draws when another style is picked.
//
// Set FB_DUMP_DIR to a directory to also get the rooms as PNGs.

#include "gamesettings.h"
#include "mainmenu.h"
#include "mainmenu_internal.h"
#include "menutheme.h"
#include "networkclient.h"
#include "platform.h"
#include <SDL3_image/SDL_image.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct MainMenuTestAccess {
    static std::unique_ptr<MainMenu> Create(const SDL_Renderer* renderer) {
        return std::unique_ptr<MainMenu>(new MainMenu(renderer, MainMenu::HeadlessTestTag{}));
    }
    static void EnterRoom(MainMenu& m, int row) {
        m.showingNetPanel = true;
        m.networkInLobby = true;
        m.networkInputMode = 0;
        m.selectedActionIndex = row;
    }
    static int Selected(const MainMenu& m) { return m.selectedActionIndex; }
    static GameMode Mode(const MainMenu& m) { return m.netGameMode; }
    static bool TeamsOpen(const MainMenu& m) { return m.showingTeamsPanel; }
    static void CloseTeams(MainMenu& m) { m.showingTeamsPanel = false; }
    static void Render(MainMenu& m) {
        m.NetPanelLobbyActionsRender();
        m.NetPanelChatDockRender();
    }
    // A second tap pushes the key it stands for; deliver it as the game would.
    static void Tap(MainMenu& m, float x, float y) {
        m.HandlePanelTap(x, y);
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_EVENT_KEY_DOWN) m.HandleInput(&e);
    }
};

struct NetworkClientTestAccess {
    static void SetPlayerNick(NetworkClient& nc, const std::string& nick) { nc.playerNick = nick; }
    static void SetCurrentGame(NetworkClient& nc, GameRoom* game) { nc.currentGame = game; }
    static void PushChat(NetworkClient& nc, const std::string& nick, const std::string& msg) {
        nc.chatMessages.push_back({nick, msg, 0});
    }
};

static SDL_Texture* g_background = nullptr;

// Draws the room over the world map and returns the frame (caller frees).
static SDL_Surface* Draw(SDL_Renderer* rend, MainMenu& menu, const char* dumpName) {
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
    SDL_RenderClear(rend);
    if (g_background) SDL_RenderTexture(rend, g_background, nullptr, nullptr);
    MainMenuTestAccess::Render(menu);
    SDL_Surface* frame = SDL_RenderReadPixels(rend, nullptr);
    if (frame) {
        if (const char* dump = SDL_getenv("FB_DUMP_DIR"))
            IMG_SavePNG(frame, (std::string(dump) + "/" + dumpName + ".png").c_str());
    }
    return frame;
}

static bool IceAt(SDL_Surface* frame, int x0, int x1, int y) {
    for (int x = x0; x <= x1; ++x) {
        Uint8 r = 0, g = 0, b = 0, a = 0;
        SDL_ReadSurfacePixel(frame, x, y, &r, &g, &b, &a);
        if (b > 200 && g > 160 && r > 90) return true;
    }
    return false;
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("room-modern-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }
    g_background = IMG_LoadTexture(renderer, ASSET("/gfx/back_netgame.png").c_str());

    // Never the developer's real settings.ini (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-room-modern-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    CHECK(settings->menuTheme() == MENU_THEME_MODERN);

    NetworkClient* nc = NetworkClient::Instance();
    NetworkClientTestAccess::SetPlayerNick(*nc, "Hc");
    NetworkClientTestAccess::PushChat(*nc, "Server", "*** Game created. Waiting for players to join.");
    NetworkClientTestAccess::PushChat(*nc, "Mika", "hi all");
    NetworkClientTestAccess::PushChat(*nc, "snowfox", "glhf");

    // A five-seat room, three seated, you hosting.
    {
        GameRoom room;
        room.creator = "Hc";
        room.maxPlayers = 5;
        for (const char* nick : {"Hc", "Mika", "snowfox"}) room.players.push_back({nick, "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::EnterRoom(*menu, kRoomChain);

        // Rules start at y 84, 22 apart; Classic hides the mode's number row,
        // so Chain reaction is the third row (y 128..148).
        SDL_Surface* frame = Draw(renderer, *menu, "room-modern");
        CHECK(frame != nullptr);
        if (frame) {
            CHECK(IceAt(frame, 21, 24, 138));     // Chain reaction, focused
            CHECK(!IceAt(frame, 21, 24, 116));    // Attack bubbles, not
            SDL_DestroySurface(frame);
        }

        // Game mode: a tap selects it, a second on the value's right half
        // steps it forward.
        const GameMode before = MainMenuTestAccess::Mode(*menu);
        MainMenuTestAccess::Tap(*menu, 380, 94);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomMode);
        CHECK(MainMenuTestAccess::Mode(*menu) == before);
        MainMenuTestAccess::Tap(*menu, 380, 94);
        CHECK(MainMenuTestAccess::Mode(*menu) != before);
        Draw(renderer, *menu, "room-modern-mode");

        // Start, HELP, Set Teams and the chat line each select on a tap.
        MainMenuTestAccess::Tap(*menu, 560, 31);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomStart);
        MainMenuTestAccess::Tap(*menu, 420, 31);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomHelpTapIndex);
        Draw(renderer, *menu, "room-modern");   // tap rows follow the frame
        MainMenuTestAccess::Tap(*menu, 575, 79);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomSetTeamsTapIndex);
        MainMenuTestAccess::Tap(*menu, 575, 79);
        CHECK(MainMenuTestAccess::TeamsOpen(*menu));
        MainMenuTestAccess::CloseTeams(*menu);
        Draw(renderer, *menu, "room-modern");
        MainMenuTestAccess::Tap(*menu, 200, 450);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomChat);
        Draw(renderer, *menu, "room-modern-chat");
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // A twenty-seat room: two columns of seats; a tap on one opens Set Teams.
    {
        GameRoom room;
        room.creator = "Hc";
        room.maxPlayers = 20;
        const char* nicks[] = {"Hc", "Mika", "snowfox", "Pingu42", "Ola", "dev_k", "Rita", "bob",
                               "bot1", "bot2", "bot3", "bot4"};
        for (const char* nick : nicks) room.players.push_back({nick, "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::EnterRoom(*menu, kRoomBots);
        SDL_Surface* frame = Draw(renderer, *menu, "room20-modern");
        CHECK(frame != nullptr);
        if (frame) SDL_DestroySurface(frame);
        // Seat 2 is the second row of the left column (y 117..135).
        MainMenuTestAccess::Tap(*menu, 380, 126);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomRosterTapBase + 1);
        MainMenuTestAccess::Tap(*menu, 380, 126);
        CHECK(MainMenuTestAccess::TeamsOpen(*menu));
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // Another style keeps the classic room.
    {
        GameRoom room;
        room.creator = "Hc";
        room.maxPlayers = 5;
        for (const char* nick : {"Hc", "Mika"}) room.players.push_back({nick, "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);
        settings->SetValue("Menu:Theme", "");   // wraps to Classic
        CHECK(settings->menuTheme() == 0);
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::EnterRoom(*menu, kRoomChain);
        SDL_Surface* frame = Draw(renderer, *menu, "room-classic");
        CHECK(frame != nullptr);
        if (frame) {
            CHECK(!IceAt(frame, 21, 24, 138));
            SDL_DestroySurface(frame);
        }
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    SDL_DestroyTexture(g_background);
    MenuThemeShutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    std::filesystem::remove_all(dir);
    TTF_Quit();
    SDL_Quit();
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
