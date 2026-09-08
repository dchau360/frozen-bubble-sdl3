#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "platform.h"
#include "ttftext.h"

#include <cstdio>
#include <utility>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

static bool HasMarker(SDL_Texture* tex, const char* marker) {
    return tex && SDL_GetBooleanProperty(SDL_GetTextureProperties(tex), marker, false);
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();

    SDL_Window* window = SDL_CreateWindow("ttftext-cache-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    SDL_Window* window2 = SDL_CreateWindow("ttftext-cache-test-2", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer2 = window2 ? SDL_CreateRenderer(window2, nullptr) : nullptr;
    if (!renderer || !renderer2) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    const char* marker = "ttftext-cache-test.marker";

    // Original coverage: identical text, position-only change, changed text,
    // changed wrap length.
    {
        TTFText text;
        text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "unchanged", 0);
        CHECK(text.Texture() != nullptr);

        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);

        text.UpdateText(renderer, "unchanged", 0);
        CHECK(HasMarker(text.Texture(), marker));

        text.UpdatePosition({12, 18});
        text.UpdateText(renderer, "unchanged", 0);
        CHECK(HasMarker(text.Texture(), marker));

        text.UpdateText(renderer, "changed", 0);
        CHECK(!HasMarker(text.Texture(), marker));

        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);
        text.UpdateText(renderer, "changed", 120);
        CHECK(!HasMarker(text.Texture(), marker));
    }

    // Color invalidation, isolated from a wrap-length change: the original
    // color case above changed wrap length (0 -> 120) in the same call that
    // changed color, so it couldn't tell which one actually forced the
    // recreate. Here wrap length stays at 0 throughout.
    {
        TTFText text;
        text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "color-test", 0);
        CHECK(text.Texture() != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);

        // Same color, same wrap: must not invalidate.
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "color-test", 0);
        CHECK(HasMarker(text.Texture(), marker));

        // Different color, same text, same wrap: must invalidate.
        text.UpdateColor({255, 220, 0, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "color-test", 0);
        CHECK(!HasMarker(text.Texture(), marker));
    }

    // Style/size change (UpdateStyle(size, style)) invalidates even when text,
    // wrap, and color are unchanged; repeating the same size/style is a no-op.
    {
        TTFText text;
        text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "style-test", 0);
        CHECK(text.Texture() != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);

        text.UpdateStyle(16, TTF_STYLE_NORMAL);  // same size/style: no-op
        text.UpdateText(renderer, "style-test", 0);
        CHECK(HasMarker(text.Texture(), marker));

        text.UpdateStyle(20, TTF_STYLE_BOLD);  // different: must invalidate
        text.UpdateText(renderer, "style-test", 0);
        CHECK(!HasMarker(text.Texture(), marker));
    }

    // Style-only overload (UpdateStyle(style)) behaves the same way.
    {
        TTFText text;
        text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "style-only-test", 0);
        CHECK(text.Texture() != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);

        text.UpdateStyle(TTF_STYLE_NORMAL);  // matches current style: no-op
        text.UpdateText(renderer, "style-only-test", 0);
        CHECK(HasMarker(text.Texture(), marker));

        text.UpdateStyle(TTF_STYLE_ITALIC);  // different: must invalidate
        text.UpdateText(renderer, "style-only-test", 0);
        CHECK(!HasMarker(text.Texture(), marker));
    }

    // Alignment change invalidates; repeating the same alignment is a no-op.
    {
        TTFText text;
        text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "align-test", 200);
        CHECK(text.Texture() != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);

        text.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
        text.UpdateText(renderer, "align-test", 200);
        CHECK(!HasMarker(text.Texture(), marker));

        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);
        text.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);  // same: no-op
        text.UpdateText(renderer, "align-test", 200);
        CHECK(HasMarker(text.Texture(), marker));
    }

    // Renderer change invalidates even when text/wrap/color are unchanged:
    // a texture belongs to the renderer that created it.
    {
        TTFText text;
        text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "renderer-test", 0);
        CHECK(text.Texture() != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);

        text.UpdateText(renderer2, "renderer-test", 0);
        CHECK(!HasMarker(text.Texture(), marker));
    }

    // Reassigning the same externally-owned font must preserve the cached
    // texture. Callers with per-cell text caches select a fixed shared font
    // on every render; treating the same pointer as a new font would defeat
    // the cache even though no rendering input changed.
    {
        TTF_Font* borrowed = TTF_OpenFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        CHECK(borrowed != nullptr);
        if (borrowed) {
            TTFText text;
            text.LoadFont(borrowed);
            text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
            text.UpdateText(renderer, "borrowed-font-test", 0);
            CHECK(text.Texture() != nullptr);
            SDL_SetBooleanProperty(SDL_GetTextureProperties(text.Texture()), marker, true);

            text.LoadFont(borrowed);
            text.UpdateText(renderer, "borrowed-font-test", 0);
            CHECK(HasMarker(text.Texture(), marker));
        }
        TTF_CloseFont(borrowed);
    }

    // A failed render (no font loaded) leaves no texture and stays retryable:
    // loading a font afterward and asking for the same string again succeeds,
    // rather than the earlier failure being mistaken for "already up to date."
    {
        TTFText text;
        text.UpdateText(renderer, "retry-test", 0);
        CHECK(text.Texture() == nullptr);

        text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        text.UpdateText(renderer, "retry-test", 0);
        CHECK(text.Texture() != nullptr);
    }

    // Move construction/assignment transfer the cached texture rather than
    // recreating it, and leave the moved-from object in a safe, empty state
    // (BUG-045: a copy used to silently drop the just-rendered texture).
    {
        TTFText source;
        source.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        source.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
        source.UpdateText(renderer, "move-test", 0);
        CHECK(source.Texture() != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(source.Texture()), marker, true);

        TTFText moved(std::move(source));
        CHECK(HasMarker(moved.Texture(), marker));
        CHECK(source.Texture() == nullptr);  // moved-from: safe to destroy, nothing to double-free

        // The moved-to object's cache still recognizes unchanged text as unchanged.
        moved.UpdateText(renderer, "move-test", 0);
        CHECK(HasMarker(moved.Texture(), marker));

        TTFText assigned;
        assigned.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
        assigned = std::move(moved);
        CHECK(HasMarker(assigned.Texture(), marker));
        CHECK(moved.Texture() == nullptr);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer2);
    SDL_DestroyWindow(window2);
    TTF_Quit();
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
