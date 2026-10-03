// The title screen's Modern menu style (MENU_THEME_MODERN, menutheme.h): a
// fresh install starts on it, every row renders a label in it, and the
// selected row is drawn with the ice-blue edge that marks it, so keyboard
// focus is visible on screen and not only implied by which row ENTER fires.
//
// Set FB_DUMP_DIR to a directory to also get the title screen as a PNG.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "gamesettings.h"
#include "menubutton.h"
#include "menutheme.h"
#include "platform.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
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

static SDL_Color Pixel(SDL_Surface* sfc, int x, int y) {
    SDL_Color c{};
    SDL_ReadSurfacePixel(sfc, x, y, &c.r, &c.g, &c.b, &c.a);
    return c;
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("menu-theme-modern-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    // Never the developer's real settings.ini (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-menu-theme-modern-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    CHECK(settings->menuTheme() == MENU_THEME_MODERN);  // a fresh install's default
    CHECK(std::strcmp(MenuThemeName(MENU_THEME_MODERN), "MODERN") == 0);

    // Every label the title screen shows renders in the Modern font.
    for (const char* text : {"START 1P GAME", "SETTINGS", "STYLE: < MODERN >"}) {
        SDL_Point size{};
        SDL_Texture* label = MenuThemeRenderLabel(renderer, MENU_THEME_MODERN, text, true, &size);
        CHECK(label != nullptr);
        CHECK(size.x > 40 && size.y > 10);
        SDL_DestroyTexture(label);
    }

    // The title screen as MainMenu builds it: the same eight rows at the
    // same places (mainmenu.cpp), the second one selected.
    struct Row { const char* name; const char* icon; int frames; };
    const Row rows[] = {
        {"1pgame", "1pgame", 30}, {"2pgame", "p1p2", 30}, {"langame", "langame", 70},
        {"netgame", "netgame", 89}, {"graphics", "graphics", 30}, {"keys", "keys", 80},
        {"highscores", "highscore", 89}, {"menustyle", "editor", 67},
    };
    std::vector<MenuButton> buttons;
    for (size_t i = 0; i < std::size(rows); ++i)
        buttons.emplace_back(89, 14 + 56 * (uint32_t)i, rows[i].name, renderer, rows[i].icon, rows[i].frames);
    buttons[1].Activate();
    buttons[7].Activate();  // the longest label, "STYLE: < MODERN >", selected too

    SDL_Texture* background = IMG_LoadTexture(renderer, ASSET("/gfx/menu/back_start.png").c_str());
    CHECK(background != nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, background, nullptr, nullptr);
    MenuThemeDrawBackdrop(renderer, MENU_THEME_MODERN);
    for (MenuButton& b : buttons) b.Render(renderer);

    SDL_Surface* frame = SDL_RenderReadPixels(renderer, nullptr);
    CHECK(frame != nullptr);
    if (frame) {
        // The left edge of a card, halfway down: ice blue on the selected
        // row (y = 14 + 56 + 23), only a faint line on the one above it.
        const SDL_Color selected = Pixel(frame, 92, 93);
        const SDL_Color idle = Pixel(frame, 92, 37);
        CHECK(selected.b > 200 && selected.g > 160);
        CHECK(idle.b < selected.b - 40);

        if (const char* dump = SDL_getenv("FB_DUMP_DIR"))
            IMG_SavePNG(frame, (std::string(dump) + "/title-modern.png").c_str());
        SDL_DestroySurface(frame);
    }

    buttons.clear();
    SDL_DestroyTexture(background);
    MenuThemeShutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    std::filesystem::remove_all(dir);
    TTF_Quit();
    SDL_Quit();
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
