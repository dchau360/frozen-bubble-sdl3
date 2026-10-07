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

// The online screens -- the NET GAME list, the lobby, a game room with its
// chat, and Set Teams -- draw the classic way in every menu style: the Modern
// style (the default) used to give them their own cards and was reverted
// (user decision), so each is drawn under Modern and under Classic here and
// the two frames must match pixel for pixel. Also that Set Teams' Auto
// buttons stay a keyboard row.
//
// Set FB_DUMP_DIR to a directory to also get the screens as PNGs.

#include "gamesettings.h"
#include "mainmenu.h"
#include "mainmenu_internal.h"
#include "menutheme.h"
#include "networkclient.h"
#include "platform.h"
#include <SDL3_image/SDL_image.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
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
    static void OpenTeams(MainMenu& m, int slot) { m.OpenTeamsPanel(slot); }
    static bool TeamsOpen(const MainMenu& m) { return m.showingTeamsPanel; }
    static void RenderTeams(MainMenu& m) { m.TeamsPanelRender(); }
    static int TeamOf(const MainMenu& m, int slot) { return m.TeamOfSlot(slot); }
    static int AutoFocus(const MainMenu& m) { return m.teamsAutoFocus; }
    static int TeamsCursor(const MainMenu& m) { return m.teamsCursorPlayer; }
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
    static void Render(MainMenu& m) {
        m.NetPanelLobbyActionsRender();
        m.NetPanelChatDockRender();
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

static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_background = nullptr;
static GameSettings* g_settings = nullptr;

// Draws one screen with a fresh menu in the given style and returns the
// frame (caller frees). `setup` puts the menu on the screen, `draw` draws it.
static SDL_Surface* Frame(int theme, const char* dumpName,
                          const std::function<void(MainMenu&)>& setup,
                          const std::function<void(MainMenu&)>& draw) {
    g_settings->SetValue("Menu:Theme", "");   // Classic: every step wraps there eventually
    while (g_settings->menuTheme() != theme) g_settings->SetValue("Menu:Theme", "");
    auto menu = MainMenuTestAccess::Create(g_renderer);
    setup(*menu);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    if (g_background) SDL_RenderTexture(g_renderer, g_background, nullptr, nullptr);
    draw(*menu);
    SDL_Surface* frame = SDL_RenderReadPixels(g_renderer, nullptr);
    if (frame) {
        if (const char* dump = SDL_getenv("FB_DUMP_DIR"))
            IMG_SavePNG(frame, (std::string(dump) + "/" + dumpName + "-" +
                                std::to_string(theme) + ".png").c_str());
    }
    return frame;
}

static bool SameFrame(SDL_Surface* a, SDL_Surface* b) {
    if (!a || !b || a->w != b->w || a->h != b->h || a->format != b->format) return false;
    for (int y = 0; y < a->h; ++y)
        if (std::memcmp((const Uint8*)a->pixels + y * a->pitch,
                        (const Uint8*)b->pixels + y * b->pitch, (size_t)a->w * 4) != 0)
            return false;
    return true;
}

// The screen must come out the same under Modern as under Classic.
static void ExpectClassic(const char* name,
                          const std::function<void(MainMenu&)>& setup,
                          const std::function<void(MainMenu&)>& draw) {
    SDL_Surface* modern = Frame(MENU_THEME_MODERN, name, setup, draw);
    SDL_Surface* classic = Frame(0, name, setup, draw);
    CHECK(modern != nullptr && classic != nullptr);
    if (!SameFrame(modern, classic)) {
        std::fprintf(stderr, "%s: Modern differs from Classic\n", name);
        ++failures;
    }
    SDL_DestroySurface(modern);
    SDL_DestroySurface(classic);
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("net-menus-classic-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }
    g_renderer = renderer;
    g_background = IMG_LoadTexture(renderer, ASSET("/gfx/back_netgame.png").c_str());

    // Never the developer's real settings.ini (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-net-menus-classic-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    CHECK(settings->menuTheme() == MENU_THEME_MODERN);
    g_settings = settings;

    NetworkClient* nc = NetworkClient::Instance();
    NetworkClientTestAccess::SetPlayerNick(*nc, "Hc");
    NetworkClientTestAccess::PushChat(*nc, "Server", "*** Game created. Waiting for players to join.");
    NetworkClientTestAccess::PushChat(*nc, "Mika", "hi all");
    NetworkClientTestAccess::PushChat(*nc, "snowfox", "glhf");

    const auto render = [](MainMenu& m) { MainMenuTestAccess::Render(m); };

    // The NET GAME list.
    ExpectClassic("netlist",
        [](MainMenu& m) {
            MainMenuTestAccess::ShowNetList(m, {{"fb.servequake.com", 1511, "servequake", 42},
                                                {"fb2.example.org", 1511, "eu-frozen", 88},
                                                {"10.0.0.4", 1511, "", -1}});
        },
        [](MainMenu& m) { MainMenuTestAccess::RenderNetList(m); });

    // The lobby with two rooms and its chat.
    {
        auto makeRoom = [](const char* creator, std::vector<const char*> nicks, bool started) {
            GameRoom g;
            g.creator = creator;
            g.maxPlayers = 5;
            g.started = started;
            for (const char* n : nicks) g.players.push_back({n, "", false});
            return g;
        };
        NetworkClientTestAccess::SetLobby(*nc,
            {makeRoom("Mika", {"Mika", "Ola"}, false), makeRoom("snowfox", {"snowfox"}, true)},
            {{"Hc", "", false}, {"bob", "", false}});
        ExpectClassic("lobby", [](MainMenu& m) { MainMenuTestAccess::EnterRoom(m, 1); }, render);
        NetworkClientTestAccess::SetLobby(*nc, {}, {});
    }

    // A five-seat room, three seated, you hosting.
    {
        GameRoom room;
        room.creator = "Hc";
        room.maxPlayers = 5;
        for (const char* nick : {"Hc", "Mika", "snowfox"}) room.players.push_back({nick, "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);
        ExpectClassic("room", [](MainMenu& m) { MainMenuTestAccess::EnterRoom(m, kRoomChain); }, render);
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // A twenty-seat room, and its Set Teams page.
    {
        GameRoom room;
        room.creator = "Hc";
        room.maxPlayers = 20;
        const char* nicks[] = {"Hc", "Mika", "snowfox", "Pingu42", "Ola", "dev_k", "Rita", "bob",
                               "bot1", "bot2", "bot3", "bot4"};
        for (const char* nick : nicks) room.players.push_back({nick, "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);
        ExpectClassic("room20", [](MainMenu& m) { MainMenuTestAccess::EnterRoom(m, kRoomBots); }, render);
        const auto openTeams = [](MainMenu& m) {
            MainMenuTestAccess::EnterRoom(m, kRoomBots);
            MainMenuTestAccess::OpenTeams(m, 1);
        };
        ExpectClassic("teams", openTeams, [](MainMenu& m) { MainMenuTestAccess::RenderTeams(m); });

        // The Auto buttons from the keyboard: UP past the first player onto
        // Auto 2, RIGHT to Auto 3, ENTER splits everyone three ways, DOWN
        // goes back to the first player.
        auto menu = MainMenuTestAccess::Create(renderer);
        openTeams(*menu);
        MainMenuTestAccess::RenderTeams(*menu);
        CHECK(MainMenuTestAccess::TeamsOpen(*menu));
        CHECK(MainMenuTestAccess::TeamsCursor(*menu) == 1);
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
        MainMenuTestAccess::Key(*menu, SDLK_DOWN);
        CHECK(MainMenuTestAccess::AutoFocus(*menu) == -1);
        CHECK(MainMenuTestAccess::TeamsCursor(*menu) == 0);
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
