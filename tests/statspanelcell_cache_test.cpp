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
#include <cstring>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct BubbleGameTestAccess {
    static TTFText& statsPanelCell(BubbleGame& game, std::vector<TTFText>& pool,
                                   size_t idx, int fontSize = 14) {
        return game.StatsPanelCell(pool, idx, fontSize);
    }
    static std::vector<TTFText>& statsPool(BubbleGame& game) { return game.statsCellPool; }
    static std::vector<TTFText>& royalePool(BubbleGame& game) { return game.royaleHudCellPool; }
    static std::vector<TTFText>& malusPool(BubbleGame& game) { return game.malusAlertPool; }
    static void renderRoundStats(BubbleGame& game, SDL_Renderer* r) { game.RenderRoundStats(r); }
    static void renderRoyaleHud(BubbleGame& game, SDL_Renderer* r) { game.RenderRoyaleHud(r); }
    static void renderMalusAlerts(BubbleGame& game, SDL_Renderer* r) { game.RenderMalusAlerts(r); }
    static void ageMalusAlerts(BubbleGame& game) { game.AgeMalusAlerts(); }
    static void updateRoundStatsHitRects(BubbleGame& game) { game.UpdateRoundStatsHitRects(); }
    static SDL_Rect& statsChatBtn(BubbleGame& game) { return game.statsChatBtn; }
    static SDL_Rect& statsTournamentBtn(BubbleGame& game) { return game.statsTournamentBtn; }
    static bool& tournamentRound(BubbleGame& game) { return game.tournamentRound; }
    static SetupSettings& settings(BubbleGame& game) { return game.currentSettings; }
    static BubbleArray& player(BubbleGame& game, int idx) { return game.bubbleArrays[idx]; }
    static int& roundsPlayed(BubbleGame& game) { return game.roundsPlayed; }
    static bool& netViewAuto(BubbleGame& game) { return game.netViewAuto; }
    static void updatePlayerNames(BubbleGame& game) { game.UpdatePlayerNameWinText(); }
    static TTFText& playerName(BubbleGame& game, int idx) { return game.playerNameWinText[idx]; }
    static TTFText& targetingLabel(BubbleGame& game, int idx) { return game.targetingText[idx]; }
    // R1d-iv (first sub-slice): score/pop-count HUD recompute-vs-blit split.
    static void updateScoreText(BubbleGame& game, BubbleArray& p, int slot) { game.UpdateScoreText(p, slot); }
    static void drawScoreText(BubbleGame& game, int slot) { game.DrawScoreText(slot); }
    static TTFText& scoreLabel(BubbleGame& game, int slot) { return game.scoreText[slot]; }
    static void updatePoppedText(BubbleGame& game, BubbleArray& p, int idx) { game.UpdatePoppedText(p, idx); }
    static void drawPoppedText(BubbleGame& game, int idx) { game.DrawPoppedText(idx); }
    static TTFText& poppedLabel(BubbleGame& game, int idx) { return game.poppedText[idx]; }
    static TTFText& modeTimerLabel(BubbleGame& game) { return game.modeTimerText; }
};

struct TTFTextTestAccess {
    static TTF_Font* font(TTFText& text) { return text.textFont; }
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

        // Pool slots keep separate textures but borrow one immutable font.
        // Re-fetch after growth because resize may move the TTFText objects.
        CHECK(TTFTextTestAccess::font(cell(0, "Player 1")) ==
              TTFTextTestAccess::font(cell(2, "Player 3")));
    }

    {
        BubbleGame game(renderer);
        std::vector<TTFText>& pool = BubbleGameTestAccess::malusPool(game);
        BubbleGameTestAccess::statsPanelCell(game, pool, 0, 16);
        BubbleGameTestAccess::statsPanelCell(game, pool, 5, 16);
        // Re-fetch slot zero after the pool grows.
        TTFText& firstAgain = BubbleGameTestAccess::statsPanelCell(game, pool, 0, 16);
        TTFText& last = BubbleGameTestAccess::statsPanelCell(game, pool, 5, 16);
        CHECK(TTFTextTestAccess::font(firstAgain) == TTFTextTestAccess::font(last));
    }

    {
        BubbleGame game(renderer);
        CHECK(TTFTextTestAccess::font(BubbleGameTestAccess::targetingLabel(game, 0)) ==
              TTFTextTestAccess::font(BubbleGameTestAccess::targetingLabel(game,
                                                                           MAX_NET_PLAYERS - 1)));
        // Player zero intentionally uses a larger label. Remote players all
        // use the same immutable centered 16 px font.
        CHECK(TTFTextTestAccess::font(BubbleGameTestAccess::playerName(game, 1)) ==
              TTFTextTestAccess::font(BubbleGameTestAccess::playerName(game,
                                                                       MAX_NET_PLAYERS - 1)));
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

    // RenderRoundStats has no game-mode gate on the team-totals block --
    // Clear Mode gets the same "TEAM TOTALS" rows Classic does whenever
    // someone is on a team. Same 3-player shape as the test above (1 header +
    // 8 column headers + 3*8 player rows = 33), plus one team of 2 adding a
    // "TEAM TOTALS" header cell and one 7-cell team row (name + 6 stat
    // columns) = 33 + 1 + 7 = 41.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 3;
        settings.networkGame = false;
        settings.gameMode = GameMode::Clear;
        settings.playerTeams[0] = settings.playerTeams[1] = 1;
        BubbleGameTestAccess::roundsPlayed(game) = 1;
        for (int i = 0; i < 3; i++) {
            BubbleArray& p = BubbleGameTestAccess::player(game, i);
            p.winCount = i;
            p.rFired = 10 + i;
            p.rPopped = 5 + i;
            p.mpWinner = (i == 0);
        }
        std::vector<TTFText>& pool = BubbleGameTestAccess::statsPool(game);
        BubbleGameTestAccess::renderRoundStats(game, renderer);
        CHECK(pool.size() == 41);
    }

    // A team change must recolor a player label immediately, including the
    // transition back to no team. The texture marker proves the render was
    // replaced on the first update after each change rather than one frame
    // late or left tinted with the previous team's color.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 3;
        BubbleArray& player = BubbleGameTestAccess::player(game, 0);
        player.playerNickname = "Player";
        player.winCount = 2;
        TTFText& label = BubbleGameTestAccess::playerName(game, 0);
        const char* marker = "statspanelcell-cache-test.player-name-color";

        settings.playerTeams[0] = kNoTeam;
        BubbleGameTestAccess::updatePlayerNames(game);
        CHECK(label.Texture() != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(label.Texture()), marker, true);

        settings.playerTeams[0] = 1;
        BubbleGameTestAccess::updatePlayerNames(game);
        CHECK(!HasMarker(label, marker));

        BubbleGameTestAccess::updatePlayerNames(game);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(label.Texture()), marker, true);
        settings.playerTeams[0] = kNoTeam;
        BubbleGameTestAccess::updatePlayerNames(game);
        CHECK(!HasMarker(label, marker));
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
    //
    // RenderMalusAlerts() is a pure draw (R1d-ii): repeated calls must not
    // age or prune anything by themselves. AgeMalusAlerts() is the separate
    // mutator that actually ages/prunes; asserted here too so this test still
    // exercises the aging path it used to cover implicitly before the split.
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
        CHECK(p0.malusAlerts.size() == 1);
        CHECK(p0.malusAlerts[0].framesLeft == 50);  // pure draw: two renders leave it unchanged

        BubbleGameTestAccess::ageMalusAlerts(game);
        CHECK(p0.malusAlerts.size() == 1);
        CHECK(p0.malusAlerts[0].framesLeft == 49);  // AgeMalusAlerts is the real mutator
    }

    // R1d-iii: the round-stats tap-target rects (statsChatBtn/
    // statsTournamentBtn) are computed by UpdateRoundStatsHitRects(), not by
    // the RenderRoundStats() draw. A standalone call must produce the same
    // geometry the draw path would, with RenderRoundStats() never invoked.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 2;
        settings.networkGame = true;

        // Hand-computed from ComputeRoundStatsLayout's geometry: boxX=48,
        // boxY=6, rowH=16, headH=22. No teams assigned, non-tournament, so
        // teamRows=0 and both the network hint row and the Discord hint row
        // are present (16+16).
        //   boxH = 22 + 16*(2+1) + 16 + 16 + 6 = 108
        //   statsChatBtn = {48, 6 + 108 + 4, 88, 24} = {48, 118, 88, 24}
        BubbleGameTestAccess::updateRoundStatsHitRects(game);
        const SDL_Rect& chat = BubbleGameTestAccess::statsChatBtn(game);
        CHECK(chat.x == 48);
        CHECK(chat.y == 118);
        CHECK(chat.w == 88);
        CHECK(chat.h == 24);
        CHECK(BubbleGameTestAccess::statsTournamentBtn(game).w == 0);  // not a tournament round

        // And the draw itself no longer sets hit-test state: a separate
        // network instance that only ever renders leaves the rects zeroed.
        BubbleGame drawOnly(renderer);
        SetupSettings& drawOnlySettings = BubbleGameTestAccess::settings(drawOnly);
        drawOnlySettings.playerCount = 2;
        drawOnlySettings.networkGame = true;
        BubbleGameTestAccess::renderRoundStats(drawOnly, renderer);
        CHECK(BubbleGameTestAccess::statsChatBtn(drawOnly).w == 0);
        CHECK(BubbleGameTestAccess::statsTournamentBtn(drawOnly).w == 0);
    }

    // A tournament round additionally exposes the BRACKET button, and the
    // Discord hint row (which only non-tournament rooms get) is suppressed.
    // tournamentRound is private state with no public setter;
    // BubbleGameTestAccess reaches it directly.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 2;
        settings.networkGame = true;
        BubbleGameTestAccess::tournamentRound(game) = true;

        // boxH = 22 + 48 + 16 + 0 + 6 = 92; chat y = 6 + 92 + 4 = 102;
        // BRACKET x = 48 + 88 + 8 = 144.
        BubbleGameTestAccess::updateRoundStatsHitRects(game);
        const SDL_Rect& chat = BubbleGameTestAccess::statsChatBtn(game);
        CHECK(chat.x == 48);
        CHECK(chat.y == 102);
        CHECK(chat.w == 88);
        CHECK(chat.h == 24);
        const SDL_Rect& bracket = BubbleGameTestAccess::statsTournamentBtn(game);
        CHECK(bracket.x == 144);
        CHECK(bracket.y == 102);
        CHECK(bracket.w == 112);
        CHECK(bracket.h == 24);
    }

    // A local (non-network) game zeroes both rects, so HandleFinishedTap()
    // -- a no-op there anyway -- can never see a stale tap target.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 2;
        settings.networkGame = false;
        BubbleGameTestAccess::statsChatBtn(game) = {7, 7, 7, 7};  // poison
        BubbleGameTestAccess::statsTournamentBtn(game) = {8, 8, 8, 8};
        BubbleGameTestAccess::updateRoundStatsHitRects(game);
        CHECK(BubbleGameTestAccess::statsChatBtn(game).w == 0);
        CHECK(BubbleGameTestAccess::statsTournamentBtn(game).w == 0);
    }

    // The playerCount < 2 early return is preserved exactly: it leaves
    // whatever the rects held untouched rather than zeroing them.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 1;
        settings.networkGame = true;
        BubbleGameTestAccess::tournamentRound(game) = true;
        BubbleGameTestAccess::statsChatBtn(game) = {7, 7, 7, 7};  // poison
        BubbleGameTestAccess::statsTournamentBtn(game) = {8, 8, 8, 8};
        BubbleGameTestAccess::updateRoundStatsHitRects(game);
        CHECK(BubbleGameTestAccess::statsChatBtn(game).x == 7);
        CHECK(BubbleGameTestAccess::statsTournamentBtn(game).x == 8);
    }

    // R1d-iv (first sub-slice): UpdateScoreText recomputes the score string
    // only; DrawScoreText is the pure blit. Proves the split directly -- a
    // repeated recompute with unchanged state keeps the cached texture, and
    // the draw alone never invalidates it.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 1;
        settings.networkGame = false;
        BubbleArray& p = BubbleGameTestAccess::player(game, 0);
        p.score = 42;
        p.scorePos = {10, 20};
        TTFText& score = BubbleGameTestAccess::scoreLabel(game, 0);
        const char* marker = "statspanelcell-cache-test.scoretext";

        BubbleGameTestAccess::updateScoreText(game, p, 0);
        CHECK(score.Texture() != nullptr);
        CHECK(std::strcmp(score.Text(), "Score: 42") == 0);
        CHECK(score.Coords()->x == 10 && score.Coords()->y == 20);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(score.Texture()), marker, true);

        // Recompute twice with the same state: the texture stays cached.
        BubbleGameTestAccess::updateScoreText(game, p, 0);
        CHECK(HasMarker(score, marker));

        // Draw alone (no preceding recompute) must not invalidate or
        // regenerate the cached texture or move it.
        BubbleGameTestAccess::drawScoreText(game, 0);
        CHECK(HasMarker(score, marker));
        CHECK(score.Coords()->x == 10 && score.Coords()->y == 20);

        // A real state change does regenerate on the next recompute.
        p.score = 43;
        BubbleGameTestAccess::updateScoreText(game, p, 0);
        CHECK(!HasMarker(score, marker));
        CHECK(std::strcmp(score.Text(), "Score: 43") == 0);
    }

    // R1d-iv (first sub-slice): UpdatePoppedText recomputes both the pop-count
    // line and, in Timed mode, the shared countdown; DrawPoppedText is the
    // pure blit of both. Same separation property as the score test above.
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 2;
        settings.gameMode = GameMode::Timed;
        BubbleArray& p = BubbleGameTestAccess::player(game, 0);
        p.rPopped = 3;
        TTFText& popped = BubbleGameTestAccess::poppedLabel(game, 0);
        TTFText& timer = BubbleGameTestAccess::modeTimerLabel(game);
        const char* poppedMarker = "statspanelcell-cache-test.poptext";
        const char* timerMarker = "statspanelcell-cache-test.modetimer";

        BubbleGameTestAccess::updatePoppedText(game, p, 0);
        CHECK(popped.Texture() != nullptr);
        CHECK(timer.Texture() != nullptr);
        CHECK(std::strcmp(popped.Text(), "Pop 3") == 0);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(popped.Texture()), poppedMarker, true);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(timer.Texture()), timerMarker, true);

        // Second recompute with unchanged state: neither texture is rebuilt.
        BubbleGameTestAccess::updatePoppedText(game, p, 0);
        CHECK(HasMarker(popped, poppedMarker));
        CHECK(HasMarker(timer, timerMarker));

        // Draw alone (no preceding recompute) blits the cached pair without
        // invalidating either -- including the Timed-mode timer blit.
        BubbleGameTestAccess::drawPoppedText(game, 0);
        CHECK(HasMarker(popped, poppedMarker));
        CHECK(HasMarker(timer, timerMarker));

        // DrawPoppedText keeps UpdatePoppedText's playerCount guard: a solo
        // game's pure draw is a no-op, never a stale blit.
        settings.playerCount = 1;
        BubbleGameTestAccess::drawPoppedText(game, 0);
        CHECK(HasMarker(popped, poppedMarker));
        CHECK(HasMarker(timer, timerMarker));

        // A changed pop count regenerates on the next recompute.
        settings.playerCount = 2;
        p.rPopped = 4;
        BubbleGameTestAccess::updatePoppedText(game, p, 0);
        CHECK(!HasMarker(popped, poppedMarker));
        CHECK(std::strcmp(popped.Text(), "Pop 4") == 0);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
