/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
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

// The 1-player sub-menu's five rows used to get their text from hand-lettered
// PNGs (txt_<option>_{text,outlined_text}.png) in a heavily stylized carved
// wood font -- illegible at the panel's own 37px row height, and
// txt_local_multiplayer_*.png turned out to be a byte-for-byte copy of
// txt_multiplayer_training_*.png, so two of the five rows read "MULTIPLAYER
// TRAINING". SPPanelRender now renders all five at runtime with
// RenderRingedText (ttftext.h) instead.
//
// This pins both: EnsureSPLabels() produces a real, correctly-sized texture
// for every row (not just "doesn't crash" -- a failed TTF_OpenFont or
// TTF_RenderText_Blended call fails silently into a null texture, which
// SPPanelRender already tolerates by skipping the draw, so a blank row would
// not otherwise be caught here); and rows 3 and 4 (Multiplayer training,
// Local multiplayer) render to different widths, which is what the original
// duplicate-asset bug looked like at the pixel level -- both baked PNGs were
// 193x38, because they were the same file.

#include <SDL3_image/SDL_image.h>

#include "mainmenu.h"
#include "mainmenu_internal.h"
#include "platform.h"

#include <cstdio>
#include <memory>

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
    static void Ensure(MainMenu& menu) { menu.EnsureSPLabels(); }
    static SDL_Texture* Idle(const MainMenu& menu, int i) { return menu.spLabelIdle[i]; }
    static SDL_Texture* Active(const MainMenu& menu, int i) { return menu.spLabelActive[i]; }
    static SDL_Point Size(const MainMenu& menu, int i) { return menu.spLabelSize[i]; }
};

int main() {
    SDL_SetEnvironmentVariable(
        SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow(
        "sp-panel-labels-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window != nullptr) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    auto menu = MainMenuTestAccess::Create(renderer);
    MainMenuTestAccess::Ensure(*menu);

    for (int i = 0; i < SP_OPT; i++) {
        CHECK(MainMenuTestAccess::Idle(*menu, i) != nullptr);
        CHECK(MainMenuTestAccess::Active(*menu, i) != nullptr);
        SDL_Point size = MainMenuTestAccess::Size(*menu, i);
        CHECK(size.x > 0);
        CHECK(size.y > 0);
    }

    // Row 3 (Multiplayer training) and row 4 (Local multiplayer): the two
    // that shared one baked asset before this fix. Different wording at the
    // same font/size has to measure differently -- a regression that
    // reintroduces the old duplicate (whether by asset or by copy-pasted
    // text) reproduces the original bug's own symptom here.
    SDL_Point trainingSize = MainMenuTestAccess::Size(*menu, 3);
    SDL_Point localSize = MainMenuTestAccess::Size(*menu, 4);
    CHECK(trainingSize.x != localSize.x);

    // Calling it again must not re-render (the cache's whole point -- a full
    // TTF render plus texture upload per row, five rows, every frame the
    // panel is open would not be free) or leak by clobbering the cached
    // pointers with a fresh set. Same pointers back is the proof.
    SDL_Texture* firstIdle0 = MainMenuTestAccess::Idle(*menu, 0);
    MainMenuTestAccess::Ensure(*menu);
    CHECK(MainMenuTestAccess::Idle(*menu, 0) == firstIdle0);

    menu.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    if (failures == 0) {
        std::printf("sp-panel-labels-test: all checks passed\n");
    } else {
        std::fprintf(stderr, "sp-panel-labels-test: %d check(s) failed\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
