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

// CONTROLS & SETTINGS in the Modern menu style (mainmenu_settingsmodern.cpp):
// the focused row carries the ice edge, the player tabs, Game speed's split
// value, the switches, a key binding and the two-press Reset all settings are
// all tap targets that behave as their keys do, and the classic panel still
// draws when another style is picked.
//
// Set FB_DUMP_DIR to a directory to also get the screens as PNGs.

#include "gamesettings.h"
#include "mainmenu.h"
#include "mainmenu_internal.h"
#include "menutheme.h"
#include "platform.h"
#include <SDL3_image/SDL_image.h>

#include <chrono>
#include <cmath>
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
    static void Open(MainMenu& m, int row) {
        m.showingKeysPanel = true;
        m.keyConfigPlayer = 1;
        m.keyConfigIndex = row;
    }
    static int Selected(const MainMenu& m) { return m.keyConfigIndex; }
    static int Player(const MainMenu& m) { return m.keyConfigPlayer; }
    static bool Awaiting(const MainMenu& m) { return m.awaitKp; }
    static bool Armed(const MainMenu& m) { return m.resetAllArmed; }
    static bool HowTo(const MainMenu& m) { return m.showingHowTo; }
    static bool IsOpen(const MainMenu& m) { return m.showingKeysPanel; }
    static void Render(MainMenu& m) { m.KeysPanelRender(); }
    static void Key(MainMenu& m, SDL_Keycode key, SDL_Scancode sc = SDL_SCANCODE_UNKNOWN) {
        SDL_Event e{};
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = key;
        e.key.scancode = sc;
        e.key.down = true;
        m.HandleInput(&e);
    }
    // Renders first so the tap rows are this frame's, then taps; a second
    // tap pushes the key it stands for, delivered as the game would.
    static void Tap(MainMenu& m, float x, float y) {
        m.KeysPanelRender();
        m.HandlePanelTap(x, y);
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_EVENT_KEY_DOWN) m.HandleInput(&e);
    }
};

static SDL_Texture* g_background = nullptr;

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
    SDL_Window* window = SDL_CreateWindow("settings-modern-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }
    g_background = IMG_LoadTexture(renderer, ASSET("/gfx/back_netgame.png").c_str());

    // Never the developer's real settings.ini (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-settings-modern-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    CHECK(settings->menuTheme() == MENU_THEME_MODERN);

    // The Game card's rows: x from 332, 36 tall, Game speed at y 86, then 40
    // apart (on a build with the Fullscreen row and no store).
    constexpr int kSpeedY = 86, kPitch = 40;
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::Open(*menu, kKeyRowMouse);
        SDL_Surface* frame = Draw(renderer, *menu, "settings-modern");
        CHECK(frame != nullptr);
        if (frame) {
            CHECK(IceAt(frame, 331, 334, kSpeedY + 2 * kPitch + 18));   // Mouse / touch aim, focused
            CHECK(!IceAt(frame, 331, 334, kSpeedY + kPitch + 18));      // Sound, not
            SDL_DestroySurface(frame);
        }

        // Mouse / touch aim: a tap selects it (already), a second flips it.
        const bool mouseBefore = settings->mouseEnabled;
        MainMenuTestAccess::Tap(*menu, 450, kSpeedY + 2 * kPitch + 18);
        CHECK(settings->mouseEnabled != mouseBefore);

        // Game speed: the first tap selects, the second on the value's right
        // half steps it up, on the label steps it down.
        const float speed = settings->speedMultiplier;
        MainMenuTestAccess::Tap(*menu, 600, kSpeedY + 18);
        CHECK(MainMenuTestAccess::Selected(*menu) == kKeyRowSpeed);
        CHECK(settings->speedMultiplier == speed);
        SDL_DestroySurface(Draw(renderer, *menu, "settings-modern-speed"));
        MainMenuTestAccess::Tap(*menu, 600, kSpeedY + 18);
        CHECK(std::fabs(settings->speedMultiplier - (speed + 0.1f)) < 0.01f);
        MainMenuTestAccess::Tap(*menu, 360, kSpeedY + 18);
        CHECK(std::fabs(settings->speedMultiplier - speed) < 0.01f);

        // The player tabs: two taps on Player 3 switch to it (SDLK_3), and
        // RIGHT from a key row moves on to Player 4.
        MainMenuTestAccess::Tap(*menu, 196, 100);
        CHECK(MainMenuTestAccess::Selected(*menu) == kKeyPlayerTapBase + 3);
        MainMenuTestAccess::Tap(*menu, 196, 100);
        CHECK(MainMenuTestAccess::Player(*menu) == 3);
        MainMenuTestAccess::Key(*menu, SDLK_RIGHT);
        CHECK(MainMenuTestAccess::Player(*menu) == 4);
        MainMenuTestAccess::Key(*menu, SDLK_1);
        CHECK(MainMenuTestAccess::Player(*menu) == 1);

        // A binding: two taps on Fire (y 218..256) wait for a key, which then binds it.
        MainMenuTestAccess::Tap(*menu, 100, 237);
        CHECK(MainMenuTestAccess::Selected(*menu) == kKeyRowFire);
        MainMenuTestAccess::Tap(*menu, 100, 237);
        CHECK(MainMenuTestAccess::Awaiting(*menu));
        SDL_DestroySurface(Draw(renderer, *menu, "settings-modern-await"));
        MainMenuTestAccess::Key(*menu, SDLK_SPACE, SDL_SCANCODE_SPACE);
        CHECK(!MainMenuTestAccess::Awaiting(*menu));
        CHECK(settings->player1Keys.fire == SDL_SCANCODE_SPACE);

        // Skip shot, player 1's fifth binding (y 310..348): bound the same way.
        MainMenuTestAccess::Tap(*menu, 100, 329);
        CHECK(MainMenuTestAccess::Selected(*menu) == kKeyRowFireNext);
        SDL_DestroySurface(Draw(renderer, *menu, "settings-modern-skipshot"));
        MainMenuTestAccess::Tap(*menu, 100, 329);
        CHECK(MainMenuTestAccess::Awaiting(*menu));
        MainMenuTestAccess::Key(*menu, SDLK_RCTRL, SDL_SCANCODE_RCTRL);
        CHECK(settings->player1Keys.fireNext == SDL_SCANCODE_RCTRL);

        // It is player 1's alone: UP/DOWN step over it for anyone else, and
        // there is no row to tap where it would be.
        MainMenuTestAccess::Key(*menu, SDLK_2);
        MainMenuTestAccess::Key(*menu, SDLK_UP);
        MainMenuTestAccess::Key(*menu, SDLK_UP);
        MainMenuTestAccess::Key(*menu, SDLK_UP);   // wraps from Turn left to Reset all
        int guard = 0;
        while (MainMenuTestAccess::Selected(*menu) != kKeyRowCenter && guard++ < 20)
            MainMenuTestAccess::Key(*menu, SDLK_DOWN);
        MainMenuTestAccess::Key(*menu, SDLK_DOWN);
        CHECK(MainMenuTestAccess::Selected(*menu) == kKeyRowResetCtrl);
        MainMenuTestAccess::Key(*menu, SDLK_UP);
        CHECK(MainMenuTestAccess::Selected(*menu) == kKeyRowCenter);
        MainMenuTestAccess::Tap(*menu, 100, 329);
        CHECK(MainMenuTestAccess::Selected(*menu) != kKeyRowFireNext);
        MainMenuTestAccess::Key(*menu, SDLK_1);

        // How to play (y 264..300): the second tap opens the page over the
        // panel, ESC closes it back to the panel.
        MainMenuTestAccess::Tap(*menu, 400, 284);
        CHECK(MainMenuTestAccess::Selected(*menu) == kKeyRowHowTo);
        MainMenuTestAccess::Tap(*menu, 400, 284);
        CHECK(MainMenuTestAccess::HowTo(*menu));
        MainMenuTestAccess::Key(*menu, SDLK_ESCAPE);
        CHECK(!MainMenuTestAccess::HowTo(*menu));
        CHECK(MainMenuTestAccess::IsOpen(*menu));

        // Reset all settings: the second tap only arms it.
        MainMenuTestAccess::Tap(*menu, 400, 364);
        CHECK(MainMenuTestAccess::Selected(*menu) == kKeyRowResetAll);
        MainMenuTestAccess::Tap(*menu, 400, 364);
        CHECK(MainMenuTestAccess::Armed(*menu));
        CHECK(settings->player1Keys.fire == SDL_SCANCODE_SPACE);
        SDL_DestroySurface(Draw(renderer, *menu, "settings-modern-reset"));

        // ESC closes.
        MainMenuTestAccess::Key(*menu, SDLK_ESCAPE);
        CHECK(!MainMenuTestAccess::IsOpen(*menu));
    }

    // Another style keeps the classic panel.
    {
        settings->SetValue("Menu:Theme", "");   // wraps to Classic
        CHECK(settings->menuTheme() == 0);
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::Open(*menu, kKeyRowMouse);
        SDL_Surface* frame = Draw(renderer, *menu, "settings-classic");
        CHECK(frame != nullptr);
        if (frame) {
            CHECK(!IceAt(frame, 331, 334, kSpeedY + 2 * kPitch + 18));
            SDL_DestroySurface(frame);
        }
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
