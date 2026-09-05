#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "platform.h"
#include "ttftext.h"

#include <cstdio>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();

    SDL_Window* window = SDL_CreateWindow("ttftext-cache-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    {
        TTFText text;
        text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "unchanged", 0);
        CHECK(text.Texture() != nullptr);

        const char* marker = "ttftext-cache-test.marker";
        SDL_PropertiesID properties = SDL_GetTextureProperties(text.Texture());
        SDL_SetBooleanProperty(properties, marker, true);

        text.UpdateText(renderer, "unchanged", 0);
        CHECK(SDL_GetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, false));

        text.UpdatePosition({12, 18});
        text.UpdateText(renderer, "unchanged", 0);
        CHECK(SDL_GetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, false));

        text.UpdateText(renderer, "changed", 0);
        CHECK(!SDL_GetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, false));

        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);
        text.UpdateText(renderer, "changed", 120);
        CHECK(!SDL_GetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, false));

        text.UpdateColor({255, 220, 0, 255}, {0, 0, 0, 255});
        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);
        text.UpdateText(renderer, "changed", 0);
        CHECK(!SDL_GetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, false));
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
