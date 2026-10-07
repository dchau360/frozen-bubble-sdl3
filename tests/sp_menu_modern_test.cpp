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

// The 1-player menu in the Modern menu style (mainmenu_spmodern.cpp): the
// focused row carries the ice edge and moves with UP/DOWN, a tap on a row
// selects it and a second tap activates it, and LEFT/RIGHT flip a settings
// row while leaving the game modes and the account row alone.
//
// Set FB_DUMP_DIR to a directory to also get the menu as PNGs.

#include "test_palette.h"
#include "gamesettings.h"
#include "mainmenu.h"
#include "menutheme.h"
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
        auto menu = std::unique_ptr<MainMenu>(new MainMenu(renderer, MainMenu::HeadlessTestTag{}));
        menu->testLocalGameStart = [](const SetupSettings&) {};
        return menu;
    }
    static void OpenSP(MainMenu& m) { m.showingSPPanel = true; m.activeSPIdx = 0; }
    static int SPIdx(const MainMenu& m) { return m.activeSPIdx; }
    static bool Account(const MainMenu& m) { return m.showingAccount; }
    static bool HowTo(const MainMenu& m) { return m.showingHowTo; }
    static bool SPOpen(const MainMenu& m) { return m.showingSPPanel; }
    static void RenderMenu(MainMenu& m) { m.Render(); }
    // A second tap pushes the key it stands for; deliver it as the game would.
    static bool Tap(MainMenu& m, float x, float y) {
        const bool hit = m.HandlePanelTap(x, y);
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_EVENT_KEY_DOWN) m.HandleInput(&e);
        return hit;
    }
};

static void Key(MainMenu& menu, SDL_Keycode key) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = key;
    e.key.down = true;
    menu.HandleInput(&e);
}

// Draws the menu and says whether the left edge of the row at y (its
// middle) is the focused row's ice blue.
static bool EdgeIsIce(SDL_Renderer* rend, MainMenu& menu, int y, const char* dumpName) {
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
    SDL_RenderClear(rend);
    MainMenuTestAccess::RenderMenu(menu);
    SDL_Surface* frame = SDL_RenderReadPixels(rend, nullptr);
    if (!frame) return false;
    // The big rows' 2px edge covers x 167-168, the slim rows' 1.5px one 166-167.
    bool ice = false;
    for (int x = 166; x <= 168; ++x) {
        Uint8 r = 0, g = 0, b = 0, a = 0;
        SDL_ReadSurfacePixel(frame, x, y, &r, &g, &b, &a);
        ice = ice || IsAccent(r, g, b);
    }
    if (const char* dump = SDL_getenv("FB_DUMP_DIR"))
        IMG_SavePNG(frame, (std::string(dump) + "/" + dumpName + ".png").c_str());
    SDL_DestroySurface(frame);
    return ice;
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("sp-menu-modern-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    // Never the developer's real settings.ini (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-sp-menu-modern-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    CHECK(settings->menuTheme() == MENU_THEME_MODERN);

    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::OpenSP(*menu);

        // Row 0 (y 52..92) focused, row 1 (97..137) not; DOWN swaps them.
        CHECK(EdgeIsIce(renderer, *menu, 72, "sp-modern"));
        CHECK(!EdgeIsIce(renderer, *menu, 117, "sp-modern"));
        Key(*menu, SDLK_DOWN);
        CHECK(MainMenuTestAccess::SPIdx(*menu) == 1);
        CHECK(EdgeIsIce(renderer, *menu, 117, "sp-modern-row1"));

        // LEFT/RIGHT do nothing on a game mode.
        Key(*menu, SDLK_LEFT);
        CHECK(MainMenuTestAccess::SPIdx(*menu) == 1);

        // A tap on the Aim guide row (y 330..358) selects it; a second flips it.
        const bool aim = settings->spAimGuideEnabled();
        CHECK(MainMenuTestAccess::Tap(*menu, 300, 340));
        CHECK(MainMenuTestAccess::SPIdx(*menu) == kSPRowAimGuide);
        CHECK(settings->spAimGuideEnabled() == aim);
        CHECK(EdgeIsIce(renderer, *menu, 340, "sp-modern-aim"));
        MainMenuTestAccess::Tap(*menu, 300, 340);
        CHECK(settings->spAimGuideEnabled() != aim);

        // LEFT and RIGHT each flip it.
        Key(*menu, SDLK_LEFT);
        CHECK(settings->spAimGuideEnabled() == aim);
        Key(*menu, SDLK_RIGHT);
        CHECK(settings->spAimGuideEnabled() != aim);
        Key(*menu, SDLK_RIGHT);
        CHECK(settings->spAimGuideEnabled() == aim);

        // The Bubbles row (y 360..388): a two-value stepper, focused with
        // its arrows.
        Key(*menu, SDLK_DOWN);
        CHECK(MainMenuTestAccess::SPIdx(*menu) == kSPRowBubbles);
        const bool candy = settings->modernBubbles();
        Key(*menu, SDLK_RIGHT);
        CHECK(settings->modernBubbles() != candy);
        Key(*menu, SDLK_LEFT);
        CHECK(settings->modernBubbles() == candy);
        CHECK(EdgeIsIce(renderer, *menu, 374, "sp-modern-bubbles"));

        // The account row opens only on ENTER, not on LEFT/RIGHT.
        Key(*menu, SDLK_DOWN);
        CHECK(MainMenuTestAccess::SPIdx(*menu) == kSPRowAccount);
        Key(*menu, SDLK_RIGHT);
        CHECK(!MainMenuTestAccess::Account(*menu));
        Key(*menu, SDLK_RETURN);
        CHECK(MainMenuTestAccess::Account(*menu));
    }

    // How to play, row 4 (y 232..272), where Local multiplayer was: a tap
    // selects it and a second opens the page; its Done button closes it,
    // back to the menu. The keyboard reaches it too, and ESC closes it.
    {
        auto menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::OpenSP(*menu);
        MainMenuTestAccess::RenderMenu(*menu);
        MainMenuTestAccess::Tap(*menu, 300, 252);
        CHECK(MainMenuTestAccess::SPIdx(*menu) == kSPRowHowTo);
        CHECK(!MainMenuTestAccess::HowTo(*menu));
        MainMenuTestAccess::Tap(*menu, 300, 252);
        CHECK(MainMenuTestAccess::HowTo(*menu));
        EdgeIsIce(renderer, *menu, 0, "howto");
        Key(*menu, SDLK_DOWN);   // nothing behind the page moves
        CHECK(MainMenuTestAccess::SPIdx(*menu) == kSPRowHowTo);
        MainMenuTestAccess::Tap(*menu, 558, 414);   // Done
        CHECK(!MainMenuTestAccess::HowTo(*menu));
        CHECK(MainMenuTestAccess::SPOpen(*menu));

        Key(*menu, SDLK_RETURN);
        CHECK(MainMenuTestAccess::HowTo(*menu));
        Key(*menu, SDLK_ESCAPE);
        CHECK(!MainMenuTestAccess::HowTo(*menu));
        CHECK(MainMenuTestAccess::SPOpen(*menu));
    }

    MenuThemeShutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    std::filesystem::remove_all(dir);
    TTF_Quit();
    SDL_Quit();
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
