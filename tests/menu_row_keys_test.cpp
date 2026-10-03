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

// On the title screen, LEFT/RIGHT on the STYLE row step back/forward through
// the menu themes, and on the GRAPHICS row through the three quality levels
// (ENTER and a tap only ever stepped one way). On any other title row, and
// with a panel open, LEFT/RIGHT leave both alone.

#include <SDL3_image/SDL_image.h>

#include "gamesettings.h"
#include "mainmenu.h"
#include "platform.h"

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
        return std::unique_ptr<MainMenu>(
            new MainMenu(renderer, MainMenu::HeadlessTestTag{}));
    }
    // The headless constructor builds no title rows; add the two this needs.
    static void AddRows(MainMenu& menu, const SDL_Renderer* renderer) {
        menu.buttons.push_back(MenuButton(89, 14, "1pgame", renderer, "1pgame", 30));
        menu.buttons.push_back(MenuButton(89, 70, "menustyle", renderer, "editor", 67));
        menu.buttons.push_back(MenuButton(89, 126, "graphics", renderer, "graphics", 30));
        menu.active_button_index = 0;
        menu.buttons[0].Activate();
    }
    static void SelectRow(MainMenu& menu, int i) {
        menu.buttons[menu.active_button_index].Deactivate();
        menu.active_button_index = i;
        menu.buttons[i].Activate();
    }
    static void SetSPPanel(MainMenu& menu, bool open) { menu.showingSPPanel = open; }
};

static void PressKey(MainMenu& menu, SDL_Keycode key) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = key;
    e.key.down = true;
    menu.HandleInput(&e);
}

int main() {
    SDL_SetEnvironmentVariable(
        SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow(
        "menu-row-keys-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    // Settings are written through on every step; keep them out of the real
    // pref dir.
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-menu-row-keys-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    CHECK(settings->menuTheme() == 5);  // Modern, a fresh install's default
    CHECK(settings->gfxLevel() == 1);   // full effects, likewise

    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::AddRows(*menu, renderer);

        // Not on the STYLE row: LEFT/RIGHT do nothing to the theme.
        PressKey(*menu, SDLK_RIGHT);
        PressKey(*menu, SDLK_LEFT);
        CHECK(settings->menuTheme() == 5);
        CHECK(settings->gfxLevel() == 1);

        MainMenuTestAccess::SelectRow(*menu, 1);
        PressKey(*menu, SDLK_RIGHT);
        CHECK(settings->menuTheme() == 0);  // wraps to Classic
        PressKey(*menu, SDLK_RIGHT);
        CHECK(settings->menuTheme() == 1);
        PressKey(*menu, SDLK_LEFT);
        CHECK(settings->menuTheme() == 0);
        PressKey(*menu, SDLK_LEFT);
        CHECK(settings->menuTheme() == 5);  // and back to Modern
        PressKey(*menu, SDLK_LEFT);
        CHECK(settings->menuTheme() == 4);  // then Pop
        PressKey(*menu, SDLK_RIGHT);
        CHECK(settings->menuTheme() == 5);

        CHECK(settings->gfxLevel() == 1);   // STYLE never touches graphics

        // GRAPHICS: RIGHT goes the way ENTER always has (1 -> 3 -> 2 -> 1),
        // LEFT the other way (1 -> 2 -> 3 -> 1).
        MainMenuTestAccess::SelectRow(*menu, 2);
        PressKey(*menu, SDLK_RIGHT);
        CHECK(settings->gfxLevel() == 3);
        PressKey(*menu, SDLK_RIGHT);
        CHECK(settings->gfxLevel() == 2);
        PressKey(*menu, SDLK_RIGHT);
        CHECK(settings->gfxLevel() == 1);
        PressKey(*menu, SDLK_LEFT);
        CHECK(settings->gfxLevel() == 2);
        PressKey(*menu, SDLK_LEFT);
        CHECK(settings->gfxLevel() == 3);
        PressKey(*menu, SDLK_LEFT);
        CHECK(settings->gfxLevel() == 1);
        CHECK(settings->menuTheme() == 5);  // and graphics never the theme

        // A panel on top of the title screen owns LEFT/RIGHT.
        MainMenuTestAccess::SetSPPanel(*menu, true);
        PressKey(*menu, SDLK_RIGHT);
        CHECK(settings->gfxLevel() == 1);
        MainMenuTestAccess::SelectRow(*menu, 1);
        PressKey(*menu, SDLK_RIGHT);
        CHECK(settings->menuTheme() == 5);
        MainMenuTestAccess::SetSPPanel(*menu, false);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    else std::printf("menu-row-keys-test: all checks passed\n");
    return failures ? 1 : 0;
}
