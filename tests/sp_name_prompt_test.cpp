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

// START in the 1-player menu asks for a name before a classic run when world
// highscores are on and no name is set (mainmenu_spname.cpp), so runs don't
// go on the public board as "unnamed". Pins: when it opens and when it
// doesn't, that only valid name characters can be typed, Save stores the
// name and starts the game, Skip starts it without one, an empty Save does
// nothing, and it is never asked twice in a session. Also that the menu's
// Account code row opens the account screen and ESC comes back to the menu.

#include "gamesettings.h"
#include "mainmenu.h"
#include "platform.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static int starts = 0;

struct MainMenuTestAccess {
    static std::unique_ptr<MainMenu> Create(const SDL_Renderer* renderer) {
        auto menu = std::unique_ptr<MainMenu>(new MainMenu(renderer, MainMenu::HeadlessTestTag{}));
        menu->testLocalGameStart = [](const SetupSettings&) { ++starts; };
        return menu;
    }
    static void OpenSPOnStart(MainMenu& m) { m.showingSPPanel = true; m.activeSPIdx = 0; }
    static bool Prompt(const MainMenu& m) { return m.spNamePrompt; }
    static std::string Input(const MainMenu& m) { return m.spNameInput; }
    static int Focus(const MainMenu& m) { return m.spNameFocus; }
    static int SPIdx(const MainMenu& m) { return m.activeSPIdx; }
    static bool SPPanel(const MainMenu& m) { return m.showingSPPanel; }
    static bool Account(const MainMenu& m) { return m.showingAccount; }
    static void Render(MainMenu& m) { m.SPPanelRender(); }
};

static void Key(MainMenu& menu, SDL_Keycode key) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = key;
    e.key.down = true;
    menu.HandleInput(&e);
}

static void Type(MainMenu& menu, const char* text) {
    SDL_Event e{};
    e.type = SDL_EVENT_TEXT_INPUT;
    e.text.text = text;
    menu.HandleInput(&e);
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("sp-name-prompt-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }
    // the prefill drops the '!' (MinGW has no setenv under strict C++17)
#ifdef _WIN32
    _putenv_s("USER", "tester!x");
#else
    setenv("USER", "tester!x", 1);
#endif

    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-sp-name-prompt-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    settings->savedNickname[0] = '\0';
    CHECK(settings->worldHighscoresEnabled());

    // No name: START opens the prompt instead of the game, prefilled.
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::OpenSPOnStart(*menu);
        Key(*menu, SDLK_RETURN);
        CHECK(MainMenuTestAccess::Prompt(*menu));
        CHECK(starts == 0);
        CHECK(MainMenuTestAccess::Input(*menu) == "testerx");

        // Only [A-Za-z0-9_-], at most 10.
        for (int i = 0; i < 7; ++i) Key(*menu, SDLK_BACKSPACE);
        CHECK(MainMenuTestAccess::Input(*menu).empty());
        // An empty Save does nothing.
        Key(*menu, SDLK_RETURN);
        CHECK(MainMenuTestAccess::Prompt(*menu));
        CHECK(starts == 0);
        Type(*menu, "Bo b!_-9");
        CHECK(MainMenuTestAccess::Input(*menu) == "Bob_-9");
        Type(*menu, "abcdefgh");
        CHECK(MainMenuTestAccess::Input(*menu) == "Bob_-9abcd");

        // Focus moves both ways and is what ENTER uses; other keys stay in.
        Key(*menu, SDLK_RIGHT);
        CHECK(MainMenuTestAccess::Focus(*menu) == 1);
        Key(*menu, SDLK_LEFT);
        CHECK(MainMenuTestAccess::Focus(*menu) == 0);
        Key(*menu, SDLK_DOWN);
        CHECK(MainMenuTestAccess::Prompt(*menu));

        // Save and play: the name is stored and the game starts.
        Key(*menu, SDLK_RETURN);
        CHECK(!MainMenuTestAccess::Prompt(*menu));
        CHECK(starts == 1);
        CHECK(std::strcmp(settings->savedNickname, "Bob_-9abcd") == 0);

        // A name exists now: START just starts.
        MainMenuTestAccess::OpenSPOnStart(*menu);
        Key(*menu, SDLK_RETURN);
        CHECK(!MainMenuTestAccess::Prompt(*menu));
        CHECK(starts == 2);
    }

    // Skip (ESC, or ENTER on Skip) starts without a name, and the prompt is
    // not shown again that session.
    settings->savedNickname[0] = '\0';
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::OpenSPOnStart(*menu);
        Key(*menu, SDLK_RETURN);
        CHECK(MainMenuTestAccess::Prompt(*menu));
        Key(*menu, SDLK_ESCAPE);
        CHECK(!MainMenuTestAccess::Prompt(*menu));
        CHECK(starts == 3);
        CHECK(settings->savedNickname[0] == '\0');

        MainMenuTestAccess::OpenSPOnStart(*menu);
        Key(*menu, SDLK_RETURN);
        CHECK(!MainMenuTestAccess::Prompt(*menu));
        CHECK(starts == 4);
    }
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::OpenSPOnStart(*menu);
        Key(*menu, SDLK_RETURN);
        CHECK(MainMenuTestAccess::Prompt(*menu));
        Key(*menu, SDLK_TAB);
        CHECK(MainMenuTestAccess::Focus(*menu) == 1);
        Key(*menu, SDLK_RETURN);
        CHECK(!MainMenuTestAccess::Prompt(*menu));
        CHECK(starts == 5);
        CHECK(settings->savedNickname[0] == '\0');
    }

    // World highscores off: never asked, nothing would be sent anyway.
    settings->SetValue("Stats:WorldHighscores", "");
    CHECK(!settings->worldHighscoresEnabled());
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::OpenSPOnStart(*menu);
        Key(*menu, SDLK_RETURN);
        CHECK(!MainMenuTestAccess::Prompt(*menu));
        CHECK(starts == 6);
    }

    // Account code: the last row (UP from the first wraps to it) opens the
    // account screen, which the panel then draws; ESC goes back to the menu.
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::OpenSPOnStart(*menu);
        Key(*menu, SDLK_UP);
        CHECK(MainMenuTestAccess::SPIdx(*menu) == kSPRowAccount);
        Key(*menu, SDLK_RETURN);
        CHECK(MainMenuTestAccess::Account(*menu));
        CHECK(starts == 6);
        MainMenuTestAccess::Render(*menu);
        Key(*menu, SDLK_ESCAPE);
        CHECK(!MainMenuTestAccess::Account(*menu));
        CHECK(MainMenuTestAccess::SPPanel(*menu));
        MainMenuTestAccess::Render(*menu);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    else std::printf("sp-name-prompt-test: all checks passed\n");
    return failures ? 1 : 0;
}
