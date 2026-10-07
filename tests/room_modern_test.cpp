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
// targets, and in a 20-seat room a tap on a seat opens Set Teams on it; the
// rules list and the lobby's rooms scroll to the selected row; the NET GAME
// list's rows are tap targets. Also that the classic room still draws when
// another style is picked.
//
// Set FB_DUMP_DIR to a directory to also get the rooms as PNGs.

#include "test_palette.h"
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
#include <vector>

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
    static void RenderTeams(MainMenu& m) { m.TeamsPanelRender(); }
    static int TeamOf(const MainMenu& m, int slot) { return m.TeamOfSlot(slot); }
    static int AutoFocus(const MainMenu& m) { return m.teamsAutoFocus; }
    static int TeamsCursor(const MainMenu& m) { return m.teamsCursorPlayer; }
    static int RoomSize(const MainMenu& m) { return m.netRoomSizeChoice; }
    static int WeeklyIndex(const MainMenu& m, size_t rooms) { return m.LobbyWeeklyIndex(rooms); }
    static int DiscordIndex(const MainMenu& m, size_t rooms) { return m.LobbyDiscordIndex(rooms); }
    static void Key(MainMenu& m, SDL_Keycode key) {
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = key;
        e.key.down = true;
        m.HandleInput(&e);
    }
    static void ShowNetList(MainMenu& m, std::vector<ServerInfo> servers) {
        m.showingNetPanel = true;
        m.networkInLobby = false;
        m.networkInputMode = 10;
        m.publicServers = std::move(servers);
        m.netMenuIndex = 0;
    }
    static void RenderNetList(MainMenu& m) { m.NetPanelConnectionScreensRender(); }
    static int NetIndex(const MainMenu& m) { return m.netMenuIndex; }
    static int SetNameIndex(const MainMenu& m) { return m.ServerListSetNameIndex(false); }
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
    static void SetLobby(NetworkClient& nc, std::vector<GameRoom> games, std::vector<NetworkPlayer> online) {
        nc.gameList = std::move(games);
        nc.openPlayers = std::move(online);
        nc.connectedHost = "fb.servequake.com";
    }
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
        if (IsAccent(r, g, b)) return true;
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

        // Rules start at y 92, 36 apart, from x 16; Classic hides the mode's
        // number row, so Chain reaction is the third row (y 164..196).
        SDL_Surface* frame = Draw(renderer, *menu, "room-modern");
        CHECK(frame != nullptr);
        if (frame) {
            CHECK(IceAt(frame, 15, 18, 180));     // Chain reaction, focused
            CHECK(!IceAt(frame, 15, 18, 144));    // Attack bubbles, not
            SDL_DestroySurface(frame);
        }

        // Game mode: a tap selects it, a second on the value's right half
        // steps it forward.
        const GameMode before = MainMenuTestAccess::Mode(*menu);
        MainMenuTestAccess::Tap(*menu, 345, 108);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomMode);
        CHECK(MainMenuTestAccess::Mode(*menu) == before);
        MainMenuTestAccess::Tap(*menu, 345, 108);
        CHECK(MainMenuTestAccess::Mode(*menu) != before);
        SDL_DestroySurface(Draw(renderer, *menu, "room-modern-mode"));

        // Start, HELP, Set Teams and the chat line each select on a tap.
        MainMenuTestAccess::Tap(*menu, 570, 31);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomStart);
        MainMenuTestAccess::Tap(*menu, 467, 31);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomHelpTapIndex);
        SDL_DestroySurface(Draw(renderer, *menu, "room-modern"));   // tap rows follow the frame
        MainMenuTestAccess::Tap(*menu, 572, 78);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomSetTeamsTapIndex);
        MainMenuTestAccess::Tap(*menu, 572, 78);
        CHECK(MainMenuTestAccess::TeamsOpen(*menu));
        MainMenuTestAccess::CloseTeams(*menu);
        SDL_DestroySurface(Draw(renderer, *menu, "room-modern"));
        MainMenuTestAccess::Tap(*menu, 200, 424);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomChat);
        SDL_DestroySurface(Draw(renderer, *menu, "room-modern-chat"));
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
        // Bots sits under the per-player table, past the bottom of the rules
        // card, so the list scrolls to it: y 238..270 (scrolled 246).
        SDL_Surface* frame = Draw(renderer, *menu, "room20-modern");
        CHECK(frame != nullptr);
        if (frame) {
            CHECK(IceAt(frame, 15, 18, 254));
            SDL_DestroySurface(frame);
        }
        // Seat 2 is the second row of the left column (y 117..134).
        MainMenuTestAccess::Tap(*menu, 380, 125);
        CHECK(MainMenuTestAccess::Selected(*menu) == kRoomRosterTapBase + 1);
        MainMenuTestAccess::Tap(*menu, 380, 125);
        CHECK(MainMenuTestAccess::TeamsOpen(*menu));
        CHECK(MainMenuTestAccess::TeamsCursor(*menu) == 1);

        // Set Teams. A tap on a team button sets it at once: seat 2's row is
        // the second (y 188..228), team 3's button is x 484..522.
        MainMenuTestAccess::RenderTeams(*menu);
        SDL_Surface* teams = Draw(renderer, *menu, "teams-modern-under");
        if (teams) SDL_DestroySurface(teams);
        MainMenuTestAccess::RenderTeams(*menu);
        MainMenuTestAccess::Tap(*menu, 503, 208);
        CHECK(MainMenuTestAccess::TeamOf(*menu, 1) == 3);

        // The Auto buttons from the keyboard: UP past the first player onto
        // Auto 2, RIGHT to Auto 3, ENTER splits everyone three ways.
        MainMenuTestAccess::Key(*menu, SDLK_UP);
        CHECK(MainMenuTestAccess::AutoFocus(*menu) == -1);
        MainMenuTestAccess::Key(*menu, SDLK_UP);
        CHECK(MainMenuTestAccess::AutoFocus(*menu) == 1);
        MainMenuTestAccess::Key(*menu, SDLK_RIGHT);
        CHECK(MainMenuTestAccess::AutoFocus(*menu) == 2);
        MainMenuTestAccess::Key(*menu, SDLK_RETURN);
        CHECK(MainMenuTestAccess::TeamsOpen(*menu));   // ENTER on Auto applies, not closes
        CHECK(MainMenuTestAccess::TeamOf(*menu, 0) == 1);
        CHECK(MainMenuTestAccess::TeamOf(*menu, 1) == 2);
        CHECK(MainMenuTestAccess::TeamOf(*menu, 2) == 3);
        CHECK(MainMenuTestAccess::TeamOf(*menu, 3) == 1);
        {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            MainMenuTestAccess::RenderTeams(*menu);
            SDL_Surface* frame2 = SDL_RenderReadPixels(renderer, nullptr);
            if (frame2) {
                // The focused Auto 3 button (x 196..236, y 68..98) is solid
                // ice; Auto 2 beside it (148..188) is not.
                CHECK(IceAt(frame2, 200, 202, 83));
                CHECK(!IceAt(frame2, 152, 154, 83));
                if (const char* dump = SDL_getenv("FB_DUMP_DIR"))
                    IMG_SavePNG(frame2, (std::string(dump) + "/teams-modern.png").c_str());
                SDL_DestroySurface(frame2);
            }
        }
        // DOWN goes back to the first player; Done closes.
        MainMenuTestAccess::Key(*menu, SDLK_DOWN);
        CHECK(MainMenuTestAccess::AutoFocus(*menu) == -1);
        CHECK(MainMenuTestAccess::TeamsCursor(*menu) == 0);
        MainMenuTestAccess::RenderTeams(*menu);
        MainMenuTestAccess::Tap(*menu, 572, 31);
        CHECK(!MainMenuTestAccess::TeamsOpen(*menu));
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // The lobby: Create game room's size steps from its value, a tap on a
    // room or a pinned row selects it, and many rooms scroll.
    {
        auto room = [](const char* creator, std::vector<const char*> nicks, bool started, int cap) {
            GameRoom g;
            g.creator = creator;
            g.started = started;
            g.maxPlayers = cap;
            for (const char* n : nicks) g.players.push_back({n, "", false});
            return g;
        };
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
        NetworkClientTestAccess::SetLobby(*nc,
            {room("Mika", {"Mika", "snowfox"}, false, 5), room("penguin", {"penguin", "a", "b", "c", "d"}, true, 5)},
            {{"Hc", "", false}, {"Lumi", "", false}, {"Otso", "", false}});
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::EnterRoom(*menu, 1);   // Create game room

        // Create game room is y 92..134, the rooms from y 138, 52 apart.
        SDL_Surface* frame = Draw(renderer, *menu, "lobby-modern");
        CHECK(frame != nullptr);
        if (frame) {
            CHECK(IceAt(frame, 15, 18, 113));      // Create game room, focused
            CHECK(!IceAt(frame, 15, 18, 162));     // the first room, not
            SDL_DestroySurface(frame);
        }

        // Already selected, so a tap on the value's left half steps the size
        // down (20 -> 10) instead of creating the room.
        CHECK(MainMenuTestAccess::RoomSize(*menu) == 2);
        MainMenuTestAccess::Tap(*menu, 300, 113);
        CHECK(MainMenuTestAccess::RoomSize(*menu) == 1);
        CHECK(MainMenuTestAccess::Selected(*menu) == 1);

        // The second room (y 190..238), then the pinned rows of the Online
        // card: 36 apart, the last ending 8 above the card's foot (y 298).
        MainMenuTestAccess::Tap(*menu, 100, 214);
        CHECK(MainMenuTestAccess::Selected(*menu) == 3);
        SDL_DestroySurface(Draw(renderer, *menu, "lobby-modern-room"));
        const int pinned = MainMenuTestAccess::DiscordIndex(*menu, 2) >= 0 ? 2 : 1;
        const int pinTop = 298 - 8 - pinned * 36 + 4;
        MainMenuTestAccess::Tap(*menu, 500, pinTop + 16);
        CHECK(MainMenuTestAccess::Selected(*menu) == MainMenuTestAccess::WeeklyIndex(*menu, 2));
        if (MainMenuTestAccess::DiscordIndex(*menu, 2) >= 0) {
            MainMenuTestAccess::Tap(*menu, 500, pinTop + 36 + 16);
            CHECK(MainMenuTestAccess::Selected(*menu) == MainMenuTestAccess::DiscordIndex(*menu, 2));
        }
        // The chat line is index 0 here too.
        MainMenuTestAccess::Tap(*menu, 300, 424);
        CHECK(MainMenuTestAccess::Selected(*menu) == 0);

        // Twelve rooms: the last one selected scrolls into view.
        std::vector<GameRoom> many;
        for (int i = 0; i < 12; i++) many.push_back(room(("host" + std::to_string(i)).c_str(), {"x"}, i % 3 == 0, 5));
        NetworkClientTestAccess::SetLobby(*nc, many, {{"Hc", "", false}});
        MainMenuTestAccess::EnterRoom(*menu, 2 + 11);
        frame = Draw(renderer, *menu, "lobby-modern-scroll");
        CHECK(frame != nullptr);
        if (frame) {
            // The list scrolls to its end (466 of 620): host11 is the last
            // row in the card (y 244..292), the first rooms are off the top.
            CHECK(IceAt(frame, 15, 18, 268));
            SDL_DestroySurface(frame);
        }
        NetworkClientTestAccess::SetLobby(*nc, {}, {});
    }

    // The NET GAME list: Enter an address first (y 92..132), then each
    // server two lines tall, 54 apart from y 136; Name heads the You card.
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::ShowNetList(*menu, {{"fb.servequake.com", 1511, "servequake", 42},
                                                {"fb2.example.org", 1511, "eu-frozen", 88},
                                                {"10.0.0.4", 1511, "", -1}});
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        MainMenuTestAccess::RenderNetList(*menu);
        SDL_Surface* frame = SDL_RenderReadPixels(renderer, nullptr);
        CHECK(frame != nullptr);
        if (frame) {
            CHECK(IceAt(frame, 15, 18, 112));     // Enter an address, focused
            CHECK(!IceAt(frame, 15, 18, 161));    // the first server, not
            if (const char* dump = SDL_getenv("FB_DUMP_DIR"))
                IMG_SavePNG(frame, (std::string(dump) + "/netlist-modern.png").c_str());
            SDL_DestroySurface(frame);
        }
        MainMenuTestAccess::Tap(*menu, 100, 215);
        CHECK(MainMenuTestAccess::NetIndex(*menu) == 2);
        MainMenuTestAccess::Tap(*menu, 500, 117);
        CHECK(MainMenuTestAccess::NetIndex(*menu) == MainMenuTestAccess::SetNameIndex(*menu));
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
