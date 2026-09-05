// Regression test for the per-cell text cache backing the post-round stats
// table, royale HUD, and malus-alert toasts (bubblegame_render.cpp). Those
// panels render a variable number of text cells per frame through
// BubbleGame::StatsPanelCell(), each addressed by call order into a growable
// pool -- replacing an earlier design where every cell in the panel shared
// one TTFText object. Sharing meant each cell's different text invalidated
// the previous cell's texture every single call, even when that cell's own
// text hadn't changed since last frame; giving each cell its own pool slot
// fixes that "alternating cells" churn.
//
// A StatsPanelCell() reference is only valid until the next call that grows
// the pool (std::vector::resize can reallocate), matching how every real
// call site uses it: fetch, then immediately UpdateText/UpdatePosition/render
// within that one statement, never held across another cell's call. This
// test re-fetches a cell's reference before each check for the same reason,
// rather than reusing a reference obtained before a pool-growing call.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "bubblegame.h"
#include "platform.h"

#include <cstdio>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct BubbleGameTestAccess {
    static TTFText& statsPanelCell(BubbleGame& game, std::vector<TTFText>& pool, size_t idx) {
        return game.StatsPanelCell(pool, idx);
    }
    static std::vector<TTFText>& statsPool(BubbleGame& game) { return game.statsCellPool; }
};

static bool HasMarker(TTFText& cell, const char* marker) {
    return SDL_GetBooleanProperty(SDL_GetTextureProperties(cell.Texture()), marker, false);
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();

    SDL_Window* window = SDL_CreateWindow("statspanelcell-cache-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    {
        BubbleGame game(renderer);
        std::vector<TTFText>& pool = BubbleGameTestAccess::statsPool(game);
        const char* marker = "statspanelcell-cache-test.marker";
        const SDL_Color white = {255, 255, 255, 255};
        const SDL_Color transparentBg = {0, 0, 0, 0};

        auto cell = [&](size_t idx, const char* txt) -> TTFText& {
            TTFText& t = BubbleGameTestAccess::statsPanelCell(game, pool, idx);
            t.UpdateColor(white, transparentBg);
            t.UpdateText(renderer, txt, 0);
            return t;
        };

        // Frame 1: render two cells with distinct text, in call order, exactly
        // as RenderRoundStats's `cell()` lambda would.
        CHECK(cell(0, "Player 1").Texture() != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(cell(0, "Player 1").Texture()), marker, true);
        CHECK(cell(1, "Player 2").Texture() != nullptr);

        // The bug this pool fixes: rendering a second cell with different text
        // must not disturb the first cell's texture. It would have, had both
        // cells shared one TTFText object.
        CHECK(HasMarker(cell(0, "Player 1"), marker));

        // Frame 2: same two cells, unchanged text -- both keep their cached
        // textures (the marker survives UpdateText on each).
        SDL_SetBooleanProperty(SDL_GetTextureProperties(cell(1, "Player 2").Texture()), marker, true);
        CHECK(HasMarker(cell(0, "Player 1"), marker));
        CHECK(HasMarker(cell(1, "Player 2"), marker));

        // Frame 3: only cell 1's text changes (e.g. a score ticked up) -- cell
        // 0, whose own text is unchanged, must still keep its cached texture.
        SDL_SetBooleanProperty(SDL_GetTextureProperties(cell(0, "Player 1").Texture()), marker, true);
        CHECK(!HasMarker(cell(1, "Player 2: 5"), marker));
        CHECK(HasMarker(cell(0, "Player 1"), marker));

        // Pool growth: a new index beyond the current size lazily adds and
        // fonts a new slot, rather than aliasing an existing one.
        CHECK(pool.size() == 2);
        CHECK(cell(2, "Player 3").Texture() != nullptr);
        CHECK(pool.size() == 3);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
