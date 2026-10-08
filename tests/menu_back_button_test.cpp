/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

// The Back button every menu screen shows at the top left
// (menulist::kBackBtn): drawn on every screen past the title menu, absent on
// the title menu itself, and a tap on it does exactly what ESC does there --
// the same screen state afterwards, whichever screen it was.
//
// Set FB_DUMP_DIR to a directory to also get each screen as a PNG.

#include "gamesettings.h"
#include "mainmenu.h"
#include "menulist.h"
#include "networkclient.h"
#include "platform.h"
#include <SDL3_image/SDL_image.h>

#include <chrono>
#include <cstdio>
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
    static void Render(MainMenu& m) { m.Render(); }
    static bool Tap(MainMenu& m, float x, float y) { return m.HandlePanelTap(x, y); }
    static void Key(MainMenu& m, SDL_Keycode key) {
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = key;
        e.key.down = true;
        m.HandleInput(&e);
    }
    // Which screen is up, as one string, so "after the tap" and "after ESC"
    // can be compared whole.
    static std::string Screen(const MainMenu& m) {
        char s[256];
        std::snprintf(s, sizeof(s),
            "sp%d howto%d keys%d replays%d localmp%d opt%d kp%d level%d netsetup%d "
            "net%d lobby%d mode%d teams%d help%d account%d weekly%d tournament%d",
            m.showingSPPanel, m.showingHowTo, m.showingKeysPanel, m.showingReplaysPanel,
            m.showingLocalMPPanel, m.showingOptPanel, m.awaitKp, m.showingLevelPanel,
            m.showingNetSetupPanel, m.showingNetPanel, m.networkInLobby, m.networkInputMode,
            m.showingTeamsPanel, m.showingHelpPanel, m.showingAccount, m.showingWeekly, m.showingTournament);
        return s;
    }
    static void OpenSP(MainMenu& m) { m.ShowPanel(0); }
    static void OpenHowTo(MainMenu& m) { m.ShowPanel(0); m.OpenHowTo(); }
    static void OpenLocalMP(MainMenu& m) { m.ShowPanel(2); }
    static void OpenRandomPrompt(MainMenu& m) { m.ShowPanel(1); }
    static void OpenKeys(MainMenu& m) { m.showingKeysPanel = true; }
    static void OpenReplays(MainMenu& m) { m.showingKeysPanel = true; m.OpenReplaysPanel(); }
    static void OpenAccount(MainMenu& m) { m.ShowPanel(0); m.OpenAccountPanel(); }
    static void ShowNetList(MainMenu& m) {
        m.showingNetPanel = true;
        m.networkInLobby = false;
        m.networkInputMode = 10;
        m.publicServers = {{"fb.servequake.com", 1511, "servequake", 42}};
        m.netMenuIndex = 0;
    }
    static void EnterLobby(MainMenu& m) {
        m.showingNetPanel = true;
        m.networkInLobby = true;
        m.networkInputMode = 0;
        m.selectedActionIndex = 1;
    }
    static void OpenWeekly(MainMenu& m) { EnterLobby(m); m.OpenWeekly(); }
    static void OpenTournament(MainMenu& m) { EnterLobby(m); m.OpenTournament(); }
    static void OpenHelp(MainMenu& m) { EnterLobby(m); m.showingHelpPanel = true; }
    static void OpenTeams(MainMenu& m) { EnterLobby(m); m.OpenTeamsPanel(1); }
    static bool Showing(const MainMenu& m) { return m.BackButtonShowing(); }
};

struct NetworkClientTestAccess {
    static void SetPlayerNick(NetworkClient& nc, const std::string& nick) { nc.playerNick = nick; }
    static void SetCurrentGame(NetworkClient& nc, GameRoom* game) { nc.currentGame = game; }
};

static SDL_Renderer* g_renderer = nullptr;

// Draws the screen `setup` opens and checks its Back button, then checks a
// tap on it leaves the menu on the same screen as ESC does.
// `waitsForServer`: leaving a room goes out as PART and the screen changes
// on the server's answer, so there only "same as ESC" can be checked.
static void ExpectBack(const char* name, const std::function<void(MainMenu&)>& setup,
                       bool waitsForServer = false) {
    auto tapped = MainMenuTestAccess::Create(g_renderer);
    setup(*tapped);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    MainMenuTestAccess::Render(*tapped);
    if (!MainMenuTestAccess::Showing(*tapped)) {
        std::fprintf(stderr, "%s: no Back button\n", name);
        ++failures;
    }
    if (SDL_Surface* frame = SDL_RenderReadPixels(g_renderer, nullptr)) {
        if (const char* dump = SDL_getenv("FB_DUMP_DIR"))
            IMG_SavePNG(frame, (std::string(dump) + "/back-" + name + ".png").c_str());
        // The button's outline, in the menus' edge colour, drawn over
        // whatever the screen had there.
        SDL_Surface* rgba = SDL_ConvertSurface(frame, SDL_PIXELFORMAT_RGBA32);
        if (rgba) {
            const Uint8* p = (const Uint8*)rgba->pixels + menulist::kBackBtn.y * rgba->pitch +
                             (menulist::kBackBtn.x + menulist::kBackBtn.w / 2) * 4;
            if (!(p[0] > 200 && p[1] > 140)) {
                std::fprintf(stderr, "%s: Back button outline not drawn (%d,%d,%d)\n",
                             name, p[0], p[1], p[2]);
                ++failures;
            }
            SDL_DestroySurface(rgba);
        }
        SDL_DestroySurface(frame);
    }
    const std::string before = MainMenuTestAccess::Screen(*tapped);
    CHECK(MainMenuTestAccess::Tap(*tapped, menulist::kBackBtn.x + 20.f, menulist::kBackBtn.y + 14.f));

    auto escaped = MainMenuTestAccess::Create(g_renderer);
    setup(*escaped);
    MainMenuTestAccess::Render(*escaped);
    MainMenuTestAccess::Key(*escaped, SDLK_ESCAPE);

    const std::string afterTap = MainMenuTestAccess::Screen(*tapped);
    const std::string afterEsc = MainMenuTestAccess::Screen(*escaped);
    if (afterTap != afterEsc || (afterTap == before && !waitsForServer)) {
        std::fprintf(stderr, "%s: Back went to\n  %s\nESC went to\n  %s\nfrom\n  %s\n",
                     name, afterTap.c_str(), afterEsc.c_str(), before.c_str());
        ++failures;
    }
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("menu-back-button-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }
    g_renderer = renderer;

    // Never the developer's real settings.ini (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-menu-back-button-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();

    NetworkClient* nc = NetworkClient::Instance();
    NetworkClientTestAccess::SetPlayerNick(*nc, "Hc");

    // The title menu itself has nowhere to go back to.
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::Render(*menu);
        CHECK(!MainMenuTestAccess::Showing(*menu));
        CHECK(!MainMenuTestAccess::Tap(*menu, menulist::kBackBtn.x + 20.f, menulist::kBackBtn.y + 14.f));
    }

    ExpectBack("sp", MainMenuTestAccess::OpenSP);
    ExpectBack("howto", MainMenuTestAccess::OpenHowTo);
    ExpectBack("account", MainMenuTestAccess::OpenAccount);
    ExpectBack("localmp", MainMenuTestAccess::OpenLocalMP);
    ExpectBack("random-prompt", MainMenuTestAccess::OpenRandomPrompt);
    ExpectBack("settings", MainMenuTestAccess::OpenKeys);
    ExpectBack("replays", MainMenuTestAccess::OpenReplays);
    ExpectBack("netlist", MainMenuTestAccess::ShowNetList);
    ExpectBack("lobby", MainMenuTestAccess::EnterLobby);
    ExpectBack("help", MainMenuTestAccess::OpenHelp);
    ExpectBack("weekly", MainMenuTestAccess::OpenWeekly);
    ExpectBack("tournament", MainMenuTestAccess::OpenTournament);
    {
        GameRoom room;
        room.creator = "Hc";
        room.maxPlayers = 5;
        for (const char* nick : {"Hc", "Mika", "snowfox"}) room.players.push_back({nick, "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);
        ExpectBack("room", MainMenuTestAccess::EnterLobby, true);
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);
        ExpectBack("teams", MainMenuTestAccess::OpenTeams);
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (failures) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::puts("menu back button: ok");
    return 0;
}
