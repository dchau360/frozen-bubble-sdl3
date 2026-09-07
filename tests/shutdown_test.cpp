#include "frozenbubble.h"
#include "platform.h"

#include <cstdio>
#include <cstring>

struct FrozenBubbleTestAccess {
    static FrozenBubble* create() {
        auto* game = new FrozenBubble(FrozenBubble::HeadlessTestTag{});
        // Use the real destructor, including renderer and SDL_ttf shutdown.
        game->headlessTestMode = false;
        return game;
    }
    static bool loadOverlay(FrozenBubble& game, bool render) {
        game.fpsText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 12);
        game.fpsText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        if (!render) return true;
        game.window = SDL_CreateWindow("shutdown-test", 64, 64, SDL_WINDOW_HIDDEN);
        game.renderer = game.window ? SDL_CreateRenderer(game.window, nullptr) : nullptr;
        if (!game.renderer) return false;
        game.fpsText.UpdateText(game.renderer, "60 FPS", 0);
        return game.fpsText.Texture() != nullptr;
    }
    static void destroy(FrozenBubble* game) { delete game; }
};

int main(int argc, char** argv) {
    const bool empty = argc > 1 && std::strcmp(argv[1], "empty") == 0;
    const bool render = argc > 1 && std::strcmp(argv[1], "rendered") == 0;
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    InitDataDir();
    if (!empty && (!SDL_Init(SDL_INIT_VIDEO) || !TTF_Init())) {
        std::fprintf(stderr, "SDL setup failed: %s\n", SDL_GetError());
        return 1;
    }
    // Check the fixture font even when the overlay is hidden, so missing
    // assets cannot turn the crash regression into a silent pass.
    if (!empty) {
        TTF_Font* font = TTF_OpenFont(ASSET("/gfx/DroidSans.ttf").c_str(), 12);
        if (!font) {
            std::fprintf(stderr, "font setup failed: %s\n", SDL_GetError());
            return 1;
        }
        TTF_CloseFont(font);
    }

    auto* game = FrozenBubbleTestAccess::create();
    if (!empty && !FrozenBubbleTestAccess::loadOverlay(*game, render)) {
        std::fprintf(stderr, "overlay setup failed: %s\n", SDL_GetError());
        return 1;
    }

    // Regression: the member font used to close after TTF_Quit, crashing in
    // FreeType. A visible overlay also owns a texture that must be released
    // before SDL_DestroyRenderer. An early startup failure owns neither.
    FrozenBubbleTestAccess::destroy(game);
    if (TTF_WasInit() != 0 || SDL_WasInit(0) != 0) {
        std::fprintf(stderr, "shutdown left SDL subsystems initialized\n");
        return 1;
    }
    return 0;
}
