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
//
// A second half below drives RenderRoundStats/RenderRoyaleHud/
// RenderMalusAlerts themselves (not just the StatsPanelCell helper in
// isolation), so a future edit to one of those functions that reintroduces
// call-order/index bugs -- or reverts to a shared object -- gets caught even
// though the helper itself still works correctly.

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
    static std::vector<TTFText>& royalePool(BubbleGame& game) { return game.royaleHudCellPool; }
    static std::vector<TTFText>& malusPool(BubbleGame& game) { return game.malusAlertPool; }
    static void renderRoundStats(BubbleGame& game, SDL_Renderer* r) { game.RenderRoundStats(r); }
    static void renderRoyaleHud(BubbleGame& game, SDL_Renderer* r) { game.RenderRoyaleHud(r); }
    static void renderMalusAlerts(BubbleGame& game, SDL_Renderer* r) { game.RenderMalusAlerts(r); }
    static SetupSettings& settings(BubbleGame& game) { return game.currentSettings; }
    static BubbleArray& player(BubbleGame& game, int idx) { return game.bubbleArrays[idx]; }
    static int& roundsPlayed(BubbleGame& game) { return game.roundsPlayed; }
    static bool& netViewAuto(BubbleGame& game) { return game.netViewAuto; }
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

    // End-to-end: RenderRoundStats itself, across a 3-player non-network game.
    // Cell call order: round header (1), column header (8), then one 8-cell
    // row per player. No teams are assigned, so the team-totals block and
    // (non-network) the chat hint row are both skipped -- a fixed 1+8+3*8=33
    // cells every call, so the pool never grows again after the first render
    // and index-based pool access below stays valid throughout.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 3;
        settings.networkGame = false;
        BubbleGameTestAccess::roundsPlayed(game) = 1;
        for (int i = 0; i < 3; i++) {
            BubbleArray& p = BubbleGameTestAccess::player(game, i);
            p.winCount = i;
            p.rFired = 10 + i;
            p.rPopped = 5 + i;
            p.mpWinner = (i == 0);
        }

        std::vector<TTFText>& pool = BubbleGameTestAccess::statsPool(game);
        const char* marker = "statspanelcell-cache-test.roundstats";
        const size_t headerIdx = 0;         // "ROUND 1 STATS"
        const size_t player0NameIdx = 1 + 8;  // first cell of player 0's row

        BubbleGameTestAccess::renderRoundStats(game, renderer);
        CHECK(pool.size() == 33);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(pool[headerIdx].Texture()), marker, true);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(pool[player0NameIdx].Texture()), marker, true);

        // Re-render with nothing changed: both cells keep their textures.
        BubbleGameTestAccess::renderRoundStats(game, renderer);
        CHECK(HasMarker(pool[headerIdx], marker));
        CHECK(HasMarker(pool[player0NameIdx], marker));

        // Change only player 1's fired count, then re-render. The header and
        // player 0's name cell are each other's siblings in call order but
        // share no text with player 1's row, so both must still be cached --
        // this is the actual regression the earlier shared-statsText design
        // could not have passed.
        BubbleGameTestAccess::player(game, 1).rFired += 1;
        BubbleGameTestAccess::renderRoundStats(game, renderer);
        CHECK(HasMarker(pool[headerIdx], marker));
        CHECK(HasMarker(pool[player0NameIdx], marker));
    }

    // End-to-end: RenderRoyaleHud, >5-player royale with the local player
    // spectating (exercises the "SPECTATING" / pinned-view cells too).
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 6;
        BubbleGameTestAccess::player(game, 0).playerState = BubbleArray::PlayerState::LOST;

        std::vector<TTFText>& pool = BubbleGameTestAccess::royalePool(game);
        const char* marker = "statspanelcell-cache-test.royalehud";
        const size_t aliveIdx = 0;  // "N/6 alive"

        BubbleGameTestAccess::renderRoyaleHud(game, renderer);
        CHECK(pool.size() == 4);  // alive, page, SPECTATING, pin-view hint
        SDL_SetBooleanProperty(SDL_GetTextureProperties(pool[aliveIdx].Texture()), marker, true);

        // Re-render unchanged: the alive-count cell keeps its texture.
        BubbleGameTestAccess::renderRoyaleHud(game, renderer);
        CHECK(HasMarker(pool[aliveIdx], marker));

        // Flip the auto-view setting, which changes only the page cell's text
        // (index 1) -- the alive-count cell (index 0), rendered just before
        // it in call order, must not be disturbed.
        BubbleGameTestAccess::netViewAuto(game) = !BubbleGameTestAccess::netViewAuto(game);
        BubbleGameTestAccess::renderRoyaleHud(game, renderer);
        CHECK(HasMarker(pool[aliveIdx], marker));
    }

    // End-to-end: RenderMalusAlerts. An alert's displayed text depends only
    // on its sender/count/blocked flag, not on framesLeft -- so it should
    // stay cached across frames while it merely ages toward expiry.
    {
        BubbleGame game(renderer);
        BubbleGameTestAccess::settings(game).playerCount = 2;
        BubbleArray& p0 = BubbleGameTestAccess::player(game, 0);
        p0.malusAlerts.push_back({"Rival", 3, 50, false});  // fromNick, count, framesLeft, blocked

        std::vector<TTFText>& pool = BubbleGameTestAccess::malusPool(game);
        const char* marker = "statspanelcell-cache-test.malusalert";

        BubbleGameTestAccess::renderMalusAlerts(game, renderer);
        CHECK(pool.size() == 1);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(pool[0].Texture()), marker, true);

        BubbleGameTestAccess::renderMalusAlerts(game, renderer);
        CHECK(HasMarker(pool[0], marker));
        CHECK(p0.malusAlerts.size() == 1);  // framesLeft aged by 1, not yet pruned
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
