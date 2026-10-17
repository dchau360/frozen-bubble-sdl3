/*
 * Frozen-Bubble SDL3 C++ Port
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

// The Report / Block page: reachable from the keyboard (B), from a tap on the
// chat dock's button and from a tap on another player's chat line, and each
// of its two actions works by key and by tap, sharing the /block list.
//
// Set FB_DUMP_DIR to a directory to also get the page as a PNG.

#include "gamesettings.h"
#include "mainmenu.h"
#include "mainmenu_internal.h"
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
    static void EnterLobby(MainMenu& m, int row) {
        m.showingNetPanel = true;
        m.networkInLobby = true;
        m.networkInputMode = 0;
        m.selectedActionIndex = row;
    }
    static void DrawLobby(MainMenu& m) {
        m.chatLineTaps.clear();
        m.playersOpenBtnRect = SDL_Rect{};
        m.NetPanelLobbyActionsRender();
        m.NetPanelChatDockRender();
    }
    static void DrawPage(MainMenu& m) { m.PlayersPanelRender(); }
    static bool Open(const MainMenu& m) { return m.showingPlayersPanel; }
    static bool Tap(MainMenu& m, float x, float y) { return m.HandlePanelTap(x, y); }
    static void Key(MainMenu& m, SDL_Keycode key) {
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = key;
        e.key.down = true;
        m.HandleInput(&e);
    }
    static const std::vector<std::string>& List(const MainMenu& m) { return m.playersList; }
    static int Cursor(const MainMenu& m) { return m.playersCursor; }
    static SDL_Rect OpenBtn(const MainMenu& m) { return m.playersOpenBtnRect; }
    static SDL_Rect LineRect(const MainMenu& m, const std::string& nick) {
        for (const auto& t : m.chatLineTaps) if (t.nick == nick) return t.rect;
        return SDL_Rect{};
    }
    static SDL_Rect ButtonRect(const MainMenu& m, int row, int button) {
        for (const auto& t : m.playersTaps)
            if (t.row == row && t.button == button) return t.rect;
        return SDL_Rect{};
    }
    static SDL_Rect DoneRect(const MainMenu& m) { return m.playersDoneRect; }
};

struct NetworkClientTestAccess {
    static void SetPlayerNick(NetworkClient& nc, const std::string& nick) { nc.playerNick = nick; }
    static void PushChat(NetworkClient& nc, const std::string& nick, const std::string& msg) {
        nc.chatMessages.push_back({nick, msg, 0});
    }
    static void SetOnline(NetworkClient& nc, std::vector<NetworkPlayer> online) {
        nc.openPlayers = std::move(online);
    }
    static const std::vector<std::string>& Reports(const NetworkClient& nc) {
        return nc.testReportLog;
    }
};

static void Centre(const SDL_Rect& r, float& x, float& y) {
    x = r.x + r.w / 2.0f;
    y = r.y + r.h / 2.0f;
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("players-panel-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    // Never the developer's real settings.ini (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-players-panel-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();

    NetworkClient* nc = NetworkClient::Instance();
    NetworkClientTestAccess::SetPlayerNick(*nc, "Hc");
    NetworkClientTestAccess::PushChat(*nc, "Server", "*** Game created.");
    NetworkClientTestAccess::PushChat(*nc, "Mika", "hi all");
    NetworkClientTestAccess::PushChat(*nc, "Hc", "hello");
    NetworkClientTestAccess::PushChat(*nc, "snowfox", "glhf");
    NetworkClientTestAccess::SetOnline(*nc, {{"Hc", "", false}, {"quietone", "", false}});

    auto menu = MainMenuTestAccess::Create(renderer);
    MainMenuTestAccess::EnterLobby(*menu, 1);   // not the chat row
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    MainMenuTestAccess::DrawLobby(*menu);

    // The dock publishes its button and one tap line per OTHER player's message.
    CHECK(MainMenuTestAccess::OpenBtn(*menu).w > 0);
    CHECK(MainMenuTestAccess::LineRect(*menu, "Mika").w > 0);
    CHECK(MainMenuTestAccess::LineRect(*menu, "snowfox").w > 0);
    CHECK(MainMenuTestAccess::LineRect(*menu, "Hc").w == 0);       // yourself
    CHECK(MainMenuTestAccess::LineRect(*menu, "Server").w == 0);

    // Keyboard: B opens it. Players are newest speaker first, then the
    // lobby's list; never yourself or the server.
    MainMenuTestAccess::Key(*menu, SDLK_B);
    CHECK(MainMenuTestAccess::Open(*menu));
    const auto& list = MainMenuTestAccess::List(*menu);
    CHECK(list.size() == 3);
    if (list.size() == 3) {
        CHECK(list[0] == "snowfox");
        CHECK(list[1] == "Mika");
        CHECK(list[2] == "quietone");
    }
    MainMenuTestAccess::DrawPage(*menu);
    if (const char* dump = SDL_getenv("FB_DUMP_DIR")) {
        if (SDL_Surface* f = SDL_RenderReadPixels(renderer, nullptr)) {
            IMG_SavePNG(f, (std::string(dump) + "/players-panel.png").c_str());
            SDL_DestroySurface(f);
        }
    }

    // Keys: DOWN to Mika, RIGHT to Block, ENTER blocks; ENTER again unblocks.
    MainMenuTestAccess::Key(*menu, SDLK_DOWN);
    MainMenuTestAccess::Key(*menu, SDLK_RIGHT);
    MainMenuTestAccess::Key(*menu, SDLK_RETURN);
    CHECK(settings->IsPlayerBlocked("Mika"));
    MainMenuTestAccess::Key(*menu, SDLK_RETURN);
    CHECK(!settings->IsPlayerBlocked("Mika"));
    MainMenuTestAccess::Key(*menu, SDLK_RETURN);
    CHECK(settings->IsPlayerBlocked("Mika"));

    // LEFT back to Report: sends one report, and a second ENTER does not
    // send another for the same player.
    MainMenuTestAccess::Key(*menu, SDLK_LEFT);
    MainMenuTestAccess::Key(*menu, SDLK_RETURN);
    MainMenuTestAccess::Key(*menu, SDLK_RETURN);
    CHECK(NetworkClientTestAccess::Reports(*nc).size() == 1);

    // Modal: a letter does nothing behind it; ESC closes.
    MainMenuTestAccess::Key(*menu, SDLK_T);
    CHECK(MainMenuTestAccess::Open(*menu));
    MainMenuTestAccess::Key(*menu, SDLK_ESCAPE);
    CHECK(!MainMenuTestAccess::Open(*menu));

    // Taps: the dock button opens it; a button tap acts directly.
    MainMenuTestAccess::DrawLobby(*menu);
    float x, y;
    Centre(MainMenuTestAccess::OpenBtn(*menu), x, y);
    CHECK(MainMenuTestAccess::Tap(*menu, x, y));
    CHECK(MainMenuTestAccess::Open(*menu));
    MainMenuTestAccess::DrawPage(*menu);
    // Row 2 is "quietone": tap Report, then Block.
    Centre(MainMenuTestAccess::ButtonRect(*menu, 2, 0), x, y);
    CHECK(MainMenuTestAccess::Tap(*menu, x, y));
    CHECK(NetworkClientTestAccess::Reports(*nc).size() == 2);
    Centre(MainMenuTestAccess::ButtonRect(*menu, 2, 1), x, y);
    CHECK(MainMenuTestAccess::Tap(*menu, x, y));
    CHECK(settings->IsPlayerBlocked("quietone"));
    // Done closes it.
    Centre(MainMenuTestAccess::DoneRect(*menu), x, y);
    CHECK(MainMenuTestAccess::Tap(*menu, x, y));
    CHECK(!MainMenuTestAccess::Open(*menu));

    // A tap on a chat line opens it with the cursor on that player. (A blocked
    // player's lines are hidden, so Mika comes back first.)
    settings->ToggleBlockedPlayer("Mika");
    CHECK(!settings->IsPlayerBlocked("Mika"));
    // The page's own status lines have scrolled the chat; Mika speaks again.
    NetworkClientTestAccess::PushChat(*nc, "Mika", "gg");
    MainMenuTestAccess::DrawLobby(*menu);
    Centre(MainMenuTestAccess::LineRect(*menu, "Mika"), x, y);
    CHECK(MainMenuTestAccess::Tap(*menu, x, y));
    CHECK(MainMenuTestAccess::Open(*menu));
    CHECK(MainMenuTestAccess::List(*menu).size() == 3);
    CHECK(MainMenuTestAccess::List(*menu)[MainMenuTestAccess::Cursor(*menu)] == "Mika");

    // The B key is just a letter while the chat row is focused.
    MainMenuTestAccess::Key(*menu, SDLK_ESCAPE);
    MainMenuTestAccess::EnterLobby(*menu, 0);
    MainMenuTestAccess::Key(*menu, SDLK_B);
    CHECK(!MainMenuTestAccess::Open(*menu));

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    std::filesystem::remove_all(dir);
    if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    std::puts("players-panel-test: ok");
    return 0;
}
