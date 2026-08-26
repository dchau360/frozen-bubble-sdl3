#include <SDL3_image/SDL_image.h>

#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "localmultiplayer_settings.h"
#include "platform.h"

#include <cstdio>
#include <set>
#include <utility>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct BubbleGameTestAccess {
    static SetupSettings& settings(BubbleGame& game) { return game.currentSettings; }
    static BubbleArray& player(BubbleGame& game, int idx) { return game.bubbleArrays[idx]; }
    static bool finished(const BubbleGame& game) { return game.gameFinish; }
    static bool lost(const BubbleGame& game) { return game.gameLost; }
    static bool cleared(const BubbleGame& game) { return game.wonByClearing; }
    static bool matchOver(const BubbleGame& game) { return game.gameMatchOver; }
    static int winsP1(const BubbleGame& game) { return game.winsP1; }
    static int winsP2(const BubbleGame& game) { return game.winsP2; }
    static bool waiting(const BubbleGame& game) { return game.waitingForOpponentNewGame; }
    static void beginWaiting(BubbleGame& game) {
        game.waitingForOpponentNewGame = true;
        game.opponentReadyForNewGame = false;
        game.opponentsReadyCount = 0;
    }
    static void check(BubbleGame& game, int idx) { game.CheckGameState(game.bubbleArrays[idx]); }
    static void announce(BubbleGame& game, int winner, bool clear) {
        game.ResolveRoundOutcome(
            winner,
            clear ? BubbleGame::RoundWinCause::Clear : BubbleGame::RoundWinCause::Remote,
            false);
    }
    static void depart(BubbleGame& game, int idx) { game.HandlePlayerDeparture(idx); }
    static bool& chatting(BubbleGame& game) { return game.chattingMode; }
    static void assignChains(BubbleGame& game, int idx) {
        game.AssignChainReactions(game.bubbleArrays[idx]);
    }
    static int checkAir(BubbleGame& game, int idx) {
        return game.CheckAirBubbles(game.bubbleArrays[idx]);
    }
    static void runMalus(BubbleGame& game, int idx, int frame) {
        game.frameCount = frame;
        game.ProcessMalusQueue(game.bubbleArrays[idx], frame);
    }
    static void updateAtScale(BubbleGame& game, float scale) {
        game.UpdateSingleBubblesAtScale(scale);
    }
    static void capturePostRoundTransition(BubbleGame& game) {
        game.testCapturePostRoundTransition = true;
        game.testQuitToTitleRequested = false;
        game.testReloadLevel = -1;
    }
    static bool quitRequested(const BubbleGame& game) {
        return game.testQuitToTitleRequested;
    }
    static int reloadLevel(const BubbleGame& game) {
        return game.testReloadLevel;
    }
    static void setLevel(BubbleGame& game, int level) { game.curLevel = level; }
    static void setDone(BubbleGame& game, int idx, bool done) {
        game.bubbleArrays[idx].mpDone = done;
    }
    static void pressContinue(BubbleGame& game) {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = SDLK_RETURN;
        game.HandleInput(&event);
    }
    static void finishAsDraw(BubbleGame& game) { game.FinishRoundAsDraw(); }
    static void setAnimationsDone(BubbleGame& game) { game.gameMpDone = true; }
    static void useResultPanel(BubbleGame& game, int idx, SDL_Texture* texture) {
        game.multiStatePanels[idx] = texture;
        game.panelRct = {0, 0, 1, 1};
    }
    static SDL_Texture* resultPanel(const BubbleGame& game, int idx) {
        return game.multiStatePanels[idx];
    }
    static void renderResultPanel(BubbleGame& game, SDL_Renderer* renderer) {
        game.RenderMultiplayerResultPanel(renderer);
    }

    static void reset(BubbleGame& game, int players, bool network, bool clearMode) {
        singleBubbles.clear();
        malusBubbles.clear();
        game.currentSettings = {};
        game.currentSettings.playerCount = players;
        game.currentSettings.networkGame = network;
        game.currentSettings.clearMode = clearMode;
        game.gameFinish = game.gameLost = game.gameMatchOver = false;
        game.gameMpDone = false;
        game.wonByClearing = false;
        game.roundWinnerIdx = -1;
        game.connectedPlayerCount = players;
        game.curLevel = 1;
        game.winsP1 = game.winsP2 = 0;
        game.testCapturePostRoundTransition = false;
        game.testQuitToTitleRequested = false;
        game.testReloadLevel = -1;
        for (int i = 0; i < players; ++i) {
            BubbleArray& p = game.bubbleArrays[i];
            p.playerAssigned = i;
            p.playerState = BubbleArray::PlayerState::ALIVE;
            p.winCount = 0;
            p.mpWinner = false;
            p.lastAttackerIdx = -1;
            p.compressionDisabled = true;
            p.dangerZone = 12;
            for (int row = 0; row < 13; ++row)
                p.bubbleMap[row].assign((row % 2 == 0) ? 8 : 7, Bubble{});
        }
    }
};

static void PutDangerBubble(BubbleArray& player, int col = 0) {
    player.bubbleMap[12][col].bubbleId = 0;
}

static void ShapeBoard(BubbleArray& board, bool flipped) {
    for (int row = 0; row < 13; ++row) {
        const bool eightCells = ((row + (flipped ? 1 : 0)) % 2) == 0;
        board.bubbleMap[row].assign(eightCells ? 8 : 7, Bubble{});
    }
}

static SingleBubble FallingBubble(int array, int color, int y) {
    SingleBubble bubble{};
    bubble.assignedArray = array;
    bubble.bubbleId = color;
    bubble.posY = static_cast<float>(y);
    bubble.pos.y = y;
    bubble.falling = true;
    return bubble;
}

static SingleBubble VerticalLaunch(int array, int color, int x, int y,
                                   int size, int left, int right, int top) {
    SingleBubble bubble{};
    bubble.assignedArray = array;
    bubble.bubbleId = color;
    bubble.posX = bubble.oldPosX = static_cast<float>(x);
    bubble.posY = bubble.oldPosY = static_cast<float>(y);
    bubble.pos = bubble.oldpos = {x, y};
    bubble.direction = PI / 2.0f;
    bubble.launching = true;
    bubble.leftLimit = left;
    bubble.rightLimit = right;
    bubble.topLimit = top;
    bubble.bubbleSize = size;
    return bubble;
}

static std::pair<int, int> FindUniqueBubbleByColor(const BubbleArray& board,
                                                   int color) {
    for (int row = 0; row < 13; ++row)
        for (int col = 0;
             col < static_cast<int>(board.bubbleMap[row].size()); ++col)
            if (board.bubbleMap[row][col].bubbleId == color) return {row, col};
    return {-1, -1};
}

static int CountBubblesForColor(const BubbleArray& board, int color) {
    int count = 0;
    for (const auto& row : board.bubbleMap)
        for (const Bubble& bubble : row)
            if (bubble.bubbleId == color) ++count;
    return count;
}

static bool IsExpectedNeighbor(std::pair<int, int> cell) {
    static const std::set<std::pair<int, int>> expected = {
        {1, 2}, {1, 3}, {2, 2}, {2, 4}, {3, 2}, {3, 3}
    };
    return expected.count(cell) == 1;
}

static int CountChainsForColor(int color) {
    int count = 0;
    for (const SingleBubble& bubble : singleBubbles)
        if (bubble.bubbleId == color && bubble.chainExists) ++count;
    return count;
}

int main() {
    SDL_SetEnvironmentVariable(
        SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow(
        "bubblegame-rules-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window != nullptr) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    {
        BubbleGame game(renderer);

        LocalMultiplayerOptions options;
        options.playerCount = 4;
        options.chainReaction = true;
        options.victoriesIndex = 15;
        options.clearMode = false;
        options.disableMalus = true;
        options.teamMode = true;
        options.colors = {5, 6, 7, 8, 8};
        options.aimGuide = {true, false, true, false, false};

        SetupSettings built = BuildLocalMultiplayerSettings(options);
        CHECK(built.localMultiplayer);
        CHECK(built.playerCount == 4);
        CHECK(built.victoriesLimit == 30);
        CHECK(built.playerTeams[0] == 1 && built.playerTeams[1] == 2);
        CHECK(built.playerColors[2] == 7);
        CHECK(built.aimGuide[2]);

        LocalMultiplayerOptions limited2P;
        limited2P.playerCount = 2;
        limited2P.victoriesIndex = 2;  // first to 2
        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleGameTestAccess::settings(game) =
            BuildLocalMultiplayerSettings(limited2P);
        BubbleGameTestAccess::player(game, 0).winCount = 1;
        BubbleGameTestAccess::announce(game, 0, false);
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 2);
        CHECK(BubbleGameTestAccess::matchOver(game));

        LocalMultiplayerOptions limited4P;
        limited4P.playerCount = 4;
        limited4P.victoriesIndex = 1;  // first to 1
        BubbleGameTestAccess::reset(game, 4, false, false);
        BubbleGameTestAccess::settings(game) =
            BuildLocalMultiplayerSettings(limited4P);
        BubbleGameTestAccess::announce(game, 3, false);
        CHECK(BubbleGameTestAccess::player(game, 3).winCount == 1);
        CHECK(BubbleGameTestAccess::matchOver(game));

        LocalMultiplayerOptions unlimited;
        unlimited.playerCount = 2;
        unlimited.victoriesIndex = 0;
        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleGameTestAccess::settings(game) =
            BuildLocalMultiplayerSettings(unlimited);
        BubbleGameTestAccess::player(game, 0).winCount = 99;
        BubbleGameTestAccess::announce(game, 0, false);
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 100);
        CHECK(!BubbleGameTestAccess::matchOver(game));

        // The real local post-round Enter consumer must return from a finite
        // match instead of reloading and clearing gameMatchOver.
        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleGameTestAccess::settings(game).localMultiplayer = true;
        BubbleGameTestAccess::settings(game).randomLevels = true;
        BubbleGameTestAccess::settings(game).victoriesLimit = 1;
        BubbleGameTestAccess::announce(game, 0, false);
        BubbleGameTestAccess::setDone(game, 0, true);
        BubbleGameTestAccess::setDone(game, 1, true);
        BubbleGameTestAccess::capturePostRoundTransition(game);
        BubbleGameTestAccess::pressContinue(game);
        CHECK(BubbleGameTestAccess::quitRequested(game));
        CHECK(BubbleGameTestAccess::reloadLevel(game) == -1);

        // Local 3P and 4P rounds advance only after every participant's end
        // animation has completed. This drives the real HandleInput consumer,
        // with only ReloadGame's transition/board generation captured.
        for (int players : {3, 4}) {
            BubbleGameTestAccess::reset(game, players, false, false);
            BubbleGameTestAccess::settings(game).localMultiplayer = true;
            BubbleGameTestAccess::settings(game).randomLevels = true;
            BubbleGameTestAccess::settings(game).victoriesLimit = 2;
            BubbleGameTestAccess::setLevel(game, 6);
            BubbleGameTestAccess::announce(game, players - 1, false);
            CHECK(!BubbleGameTestAccess::matchOver(game));
            for (int i = 0; i < players; ++i) {
                BubbleGameTestAccess::setDone(game, i, i != players - 1);
            }
            BubbleGameTestAccess::capturePostRoundTransition(game);
            BubbleGameTestAccess::pressContinue(game);
            CHECK(BubbleGameTestAccess::reloadLevel(game) == -1);

            BubbleGameTestAccess::setDone(game, players - 1, true);
            BubbleGameTestAccess::pressContinue(game);
            CHECK(BubbleGameTestAccess::reloadLevel(game) == 7);
            CHECK(!BubbleGameTestAccess::quitRequested(game));
        }

        // A simultaneous draw has no winner texture. Exercise the render-facing
        // production method with a literal one-pixel panel and verify that it
        // leaves the render target untouched.
        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleGameTestAccess::finishAsDraw(game);
        BubbleGameTestAccess::setAnimationsDone(game);
        SDL_Texture* oldPanel = BubbleGameTestAccess::resultPanel(game, 0);
        SDL_Texture* redPanel = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET, 1, 1);
        SDL_SetRenderTarget(renderer, redPanel);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, nullptr);
        BubbleGameTestAccess::useResultPanel(game, 0, redPanel);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        BubbleGameTestAccess::renderResultPanel(game, renderer);
        SDL_Rect sampleRect{0, 0, 1, 1};
        SDL_Surface* sample = SDL_RenderReadPixels(renderer, &sampleRect);
        Uint8 red = 255, green = 255, blue = 255, alpha = 255;
        CHECK(sample != nullptr);
        if (sample != nullptr) {
            CHECK(SDL_ReadSurfacePixel(
                sample, 0, 0, &red, &green, &blue, &alpha));
            CHECK(red == 0 && green == 0 && blue == 0);
            SDL_DestroySurface(sample);
        }
        BubbleGameTestAccess::useResultPanel(game, 0, oldPanel);
        SDL_DestroyTexture(redPanel);

        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleGameTestAccess::check(game, 0);
        CHECK(!BubbleGameTestAccess::finished(game));

        BubbleGameTestAccess::reset(game, 2, false, true);
        BubbleGameTestAccess::check(game, 0);
        CHECK(BubbleGameTestAccess::finished(game));
        CHECK(BubbleGameTestAccess::cleared(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);

        // Clearing a single-player level scores it once and only once. Bubbles
        // already in flight keep driving CheckGameState after the win -- a late
        // chain landing on the empty board is dropped as unattached, leaving the
        // board clear again -- and that used to award the 1000 bonus a second
        // time and write a duplicate highscore row.
        //
        // networkGame is set purely so SubmitScore returns before it reaches the
        // highscore manager: this is about the scoring guard, not the save path.
        BubbleGameTestAccess::reset(game, 1, true, false);
        BubbleGameTestAccess::player(game, 0).score = 0;
        BubbleGameTestAccess::check(game, 0);
        CHECK(BubbleGameTestAccess::finished(game));
        CHECK(BubbleGameTestAccess::player(game, 0).score == 1000);
        BubbleGameTestAccess::check(game, 0);
        CHECK(BubbleGameTestAccess::player(game, 0).score == 1000);

        BubbleGameTestAccess::reset(game, 2, false, false);
        PutDangerBubble(BubbleGameTestAccess::player(game, 0));
        PutDangerBubble(BubbleGameTestAccess::player(game, 1));
        BubbleGameTestAccess::check(game, 0);
        CHECK(BubbleGameTestAccess::finished(game));
        CHECK(BubbleGameTestAccess::lost(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 0);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 0);
        CHECK(BubbleGameTestAccess::winsP1(game) == 0);
        CHECK(BubbleGameTestAccess::winsP2(game) == 0);

        BubbleGameTestAccess::reset(game, 2, true, true);
        BubbleGameTestAccess::check(game, 1);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 1);
        CHECK(BubbleGameTestAccess::winsP2(game) == 1);

        BubbleGameTestAccess::reset(game, 2, true, true);
        BubbleGameTestAccess::announce(game, 1, false);
        BubbleGameTestAccess::check(game, 1);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 1);
        CHECK(BubbleGameTestAccess::cleared(game));

        BubbleGameTestAccess::reset(game, 2, true, true);
        BubbleGameTestAccess::check(game, 1);
        BubbleGameTestAccess::announce(game, 1, false);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 1);

        BubbleGameTestAccess::reset(game, 2, true, false);
        BubbleGameTestAccess::announce(game, 1, false);
        BubbleGameTestAccess::announce(game, 1, false);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 1);

        BubbleGameTestAccess::reset(game, 4, false, false);
        SetupSettings& teamSettings = BubbleGameTestAccess::settings(game);
        teamSettings.teamMode = true;
        teamSettings.playerTeams[0] = teamSettings.playerTeams[2] = 1;
        teamSettings.playerTeams[1] = teamSettings.playerTeams[3] = 2;
        BubbleGameTestAccess::announce(game, 0, false);
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
        CHECK(BubbleGameTestAccess::player(game, 2).winCount == 1);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 0);
        CHECK(BubbleGameTestAccess::player(game, 3).winCount == 0);

        // Continue enabled: the sole survivor receives one win, but a 2P match cannot restart alone.
        BubbleGameTestAccess::reset(game, 2, true, false);
        BubbleGameTestAccess::settings(game).continueWhenPlayersLeave = true;
        BubbleGameTestAccess::depart(game, 1);
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
        CHECK(BubbleGameTestAccess::matchOver(game));

        // Continue disabled: show the survivor, do not credit the abandoned round.
        BubbleGameTestAccess::reset(game, 2, true, false);
        BubbleGameTestAccess::settings(game).continueWhenPlayersLeave = false;
        BubbleGameTestAccess::depart(game, 1);
        CHECK(BubbleGameTestAccess::player(game, 0).mpWinner);
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 0);
        CHECK(BubbleGameTestAccess::matchOver(game));

        // Four players on two teams: departure leaving one team resolves both teammates as winners.
        BubbleGameTestAccess::reset(game, 4, true, false);
        SetupSettings& departureTeamSettings = BubbleGameTestAccess::settings(game);
        departureTeamSettings.continueWhenPlayersLeave = true;
        departureTeamSettings.teamMode = true;
        departureTeamSettings.playerTeams[0] = departureTeamSettings.playerTeams[2] = 1;
        departureTeamSettings.playerTeams[1] = departureTeamSettings.playerTeams[3] = 2;
        BubbleGameTestAccess::depart(game, 1);
        BubbleGameTestAccess::depart(game, 3);
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
        CHECK(BubbleGameTestAccess::player(game, 2).winCount == 1);

        // A credited departure win reaches the configured limit.
        BubbleGameTestAccess::reset(game, 2, true, false);
        SetupSettings& limitSettings = BubbleGameTestAccess::settings(game);
        limitSettings.continueWhenPlayersLeave = true;
        limitSettings.victoriesLimit = 2;
        BubbleGameTestAccess::player(game, 0).winCount = 1;
        BubbleGameTestAccess::depart(game, 1);
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 2);
        CHECK(BubbleGameTestAccess::matchOver(game));

        // An abandoned round that ends in a draw still ends the match.
        BubbleGameTestAccess::reset(game, 3, true, false);
        BubbleGameTestAccess::settings(game).continueWhenPlayersLeave = false;
        BubbleGameTestAccess::depart(game, 2);
        PutDangerBubble(BubbleGameTestAccess::player(game, 0));
        PutDangerBubble(BubbleGameTestAccess::player(game, 1));
        BubbleGameTestAccess::check(game, 0);
        CHECK(BubbleGameTestAccess::lost(game));
        CHECK(BubbleGameTestAccess::matchOver(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 0);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 0);

        // A late departure ends an already-finished match when continuation is disabled.
        BubbleGameTestAccess::reset(game, 3, true, false);
        BubbleGameTestAccess::settings(game).continueWhenPlayersLeave = false;
        BubbleGameTestAccess::announce(game, 0, false);
        CHECK(!BubbleGameTestAccess::matchOver(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
        BubbleGameTestAccess::depart(game, 2);
        CHECK(BubbleGameTestAccess::matchOver(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);

        // A late departure cannot restart an already-finished 2P match alone.
        BubbleGameTestAccess::reset(game, 2, true, false);
        BubbleGameTestAccess::settings(game).continueWhenPlayersLeave = true;
        BubbleGameTestAccess::announce(game, 0, false);
        CHECK(!BubbleGameTestAccess::matchOver(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
        BubbleGameTestAccess::beginWaiting(game);
        BubbleGameTestAccess::depart(game, 1);
        CHECK(BubbleGameTestAccess::matchOver(game));
        CHECK(!BubbleGameTestAccess::waiting(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);

        // Late departures cannot restart an already-finished match with one connected team.
        BubbleGameTestAccess::reset(game, 4, true, false);
        SetupSettings& lateTeamSettings = BubbleGameTestAccess::settings(game);
        lateTeamSettings.continueWhenPlayersLeave = true;
        lateTeamSettings.teamMode = true;
        lateTeamSettings.playerTeams[0] = lateTeamSettings.playerTeams[2] = 1;
        lateTeamSettings.playerTeams[1] = lateTeamSettings.playerTeams[3] = 2;
        BubbleGameTestAccess::announce(game, 0, false);
        CHECK(!BubbleGameTestAccess::matchOver(game));
        BubbleGameTestAccess::depart(game, 1);
        CHECK(!BubbleGameTestAccess::matchOver(game));
        BubbleGameTestAccess::depart(game, 3);
        CHECK(BubbleGameTestAccess::matchOver(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
        CHECK(BubbleGameTestAccess::player(game, 2).winCount == 1);

        // A flipped grid reserves the whole adjacent color group for one chain.
        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleArray& flipped = BubbleGameTestAccess::player(game, 0);
        ShapeBoard(flipped, true);
        flipped.bubbleMap[0][0].bubbleId = 2;
        flipped.bubbleMap[1][1].bubbleId = 2;
        singleBubbles = {
            FallingBubble(0, 2, 200),
            FallingBubble(0, 2, 180),
        };
        BubbleGameTestAccess::assignChains(game, 0);
        CHECK(CountChainsForColor(2) == 1);

        // Removing the root chain's target group invalidates its dependent chain.
        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleArray& standard = BubbleGameTestAccess::player(game, 0);
        ShapeBoard(standard, false);
        standard.bubbleMap[0][0].bubbleId = 0;
        standard.bubbleMap[0][1].bubbleId = 0;
        standard.bubbleMap[1][1].bubbleId = 1;
        standard.bubbleMap[1][2].bubbleId = 1;
        singleBubbles = {
            FallingBubble(0, 0, 200),
            FallingBubble(0, 1, 180),
        };
        BubbleGameTestAccess::assignChains(game, 0);
        CHECK(CountChainsForColor(0) == 1);
        CHECK(CountChainsForColor(1) == 0);

        // Maximum frame deltas must not tunnel a full-size launch through a bubble.
        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleArray& full = BubbleGameTestAccess::player(game, 0);
        ShapeBoard(full, false);
        full.bubbleOffset = {354, 40};
        full.leftLimit = 354;
        full.rightLimit = 610;
        full.topLimit = 40;
        full.bubbleMap[2][3].bubbleId = 0;
        full.bubbleMap[2][3].pos = {450, 96};
        singleBubbles = {VerticalLaunch(0, 1, 450, 136, 32, 354, 610, 40)};
        for (int frame = 0; frame < 2 && !singleBubbles.empty(); ++frame)
            BubbleGameTestAccess::updateAtScale(game, 15.0f);
        const auto fullLanding = FindUniqueBubbleByColor(full, 1);
        CHECK(CountBubblesForColor(full, 1) == 1);
        CHECK(IsExpectedNeighbor(fullLanding));
        CHECK(fullLanding.first != 0);
        CHECK(full.bubbleMap[2][3].bubbleId == 0);
        CHECK(full.stickAnimPos.y > full.topLimit);

        // The same maximum delta must remain collision-safe on a mini board.
        BubbleGameTestAccess::reset(game, 3, false, false);
        BubbleArray& mini = BubbleGameTestAccess::player(game, 1);
        ShapeBoard(mini, false);
        mini.bubbleOffset = {20, 19};
        mini.leftLimit = 20;
        mini.rightLimit = 148;
        mini.topLimit = 19;
        mini.bubbleMap[2][3].bubbleId = 0;
        mini.bubbleMap[2][3].pos = {68, 47};
        singleBubbles = {VerticalLaunch(1, 1, 68, 67, 16, 20, 148, 19)};
        for (int frame = 0; frame < 2 && !singleBubbles.empty(); ++frame)
            BubbleGameTestAccess::updateAtScale(game, 15.0f);
        const auto miniLanding = FindUniqueBubbleByColor(mini, 1);
        CHECK(CountBubblesForColor(mini, 1) == 1);
        CHECK(IsExpectedNeighbor(miniLanding));
        CHECK(miniLanding.first != 0);
        CHECK(mini.bubbleMap[2][3].bubbleId == 0);
        CHECK(mini.stickAnimPos.y > mini.topLimit);
    }

    // --- IsTouchBackSwipe -------------------------------------------------
    // Touch is the only input with no other way out of a round, and the cost of
    // a false positive is a quit mid-game, so the gesture is deliberately hard
    // to trigger by accident.
    {
        BubbleGame game(renderer);
        BubbleGameTestAccess::reset(game, 1, false, false);

        SDL_Rect& shooter = BubbleGameTestAccess::player(game, 0).shooterSprite.rect;
        shooter = {300, 400, 40, 48};
        const float barrel = 400.f + 48.f * 0.5f;   // shooter centre
        const float below = barrel + 20.f;          // inside the dead band
        const float above = barrel - 60.f;          // where aiming still acts

        // A long leftward swipe low on the screen: the intended gesture.
        CHECK(game.IsTouchBackSwipe(500.f, below, 300.f, below));

        // The same swipe higher up is someone aiming, and must not quit.
        CHECK(!game.IsTouchBackSwipe(500.f, above, 300.f, above));

        // Direction matters: rightward is not a back gesture.
        CHECK(!game.IsTouchBackSwipe(300.f, below, 500.f, below));

        // Too short to be deliberate.
        CHECK(!game.IsTouchBackSwipe(400.f, below, 330.f, below));

        // Long enough, but diagonal rather than a horizontal swipe.
        CHECK(!game.IsTouchBackSwipe(500.f, below, 380.f, below - 100.f));

        // A plain tap is not a swipe.
        CHECK(!game.IsTouchBackSwipe(400.f, below, 400.f, below));

        // Chatting swallows input; the gesture must not fire behind the prompt.
        BubbleGameTestAccess::chatting(game) = true;
        CHECK(!game.IsTouchBackSwipe(500.f, below, 300.f, below));
        BubbleGameTestAccess::chatting(game) = false;
        CHECK(game.IsTouchBackSwipe(500.f, below, 300.f, below));
    }

    // --- Malus landing rows -----------------------------------------------
    // A malus bubble rises from below and parks one row under whatever it
    // meets. When its column holds nothing at all there is no "whatever" to
    // park under, and parking at row 1 regardless leaves it hanging with an
    // empty ceiling cell above it. Nothing sweeps that up: detached grid
    // bubbles are only removed by CheckAirBubbles, which CheckPossibleDestroy
    // runs only after a pop, and a malus landing alone pops nothing. Boards
    // empty out columns routinely late in a round, and mini boards in 3+
    // player games take malus from every opponent at once, which is where this
    // was visible as bubbles stuck in mid-air.
    {
        BubbleGame game(renderer);
        BubbleGameTestAccess::reset(game, 4, false, false);
        BubbleArray& mini = BubbleGameTestAccess::player(game, 1);
        ShapeBoard(mini, false);
        mini.bubbleOffset = {20, 19};
        mini.leftLimit = 20; mini.rightLimit = 148; mini.topLimit = 19;
        mini.numColors = 4;
        // Real per-cell positions, the way RandomLevel lays a mini board out.
        for (int row = 0; row < 13; ++row) {
            const int size = (int)mini.bubbleMap[row].size();
            const int sep = (size % 2 == 0) ? 0 : 8;
            for (int col = 0; col < size; ++col)
                mini.bubbleMap[row][col].pos = {sep + 16 * col + 20, 14 * row + 19};
        }
        // Cleared down to a single ceiling bubble in column 7, which is what a
        // round looks like just before a win. Malus columns are drawn from
        // 0..6, so whichever one comes up is guaranteed to be empty.
        mini.bubbleMap[0][7].bubbleId = 1;

        mini.malusQueue.push_back(0);
        BubbleGameTestAccess::runMalus(game, 1, 1000);
        CHECK(malusBubbles.size() == 1);
        if (malusBubbles.size() == 1) {
            // An empty column has to aim at the ceiling row itself, not one
            // row under it.
            CHECK(malusBubbles[0].cx >= 0 && malusBubbles[0].cx <= 6);
            CHECK(malusBubbles[0].stickY == 0);

            // The stick handler rescans the column when the bubble arrives,
            // because the board can change while it is still rising. That
            // rescan has to reach the same conclusion.
            malusBubbles[0].cx = 5;
            for (int frame = 0; frame < 4000 && !malusBubbles.empty(); ++frame)
                BubbleGameTestAccess::updateAtScale(game, 1.0f);
            CHECK(malusBubbles.empty());
            CHECK(mini.bubbleMap[0][5].bubbleId != -1);
            CHECK(mini.bubbleMap[1][5].bubbleId == -1);

            // The real proof: the connectivity sweep must not want to drop it.
            singleBubbles.clear();
            CHECK(BubbleGameTestAccess::checkAir(game, 1) == 0);
        }
    }

    // A column that does hold bubbles still takes the malus underneath them,
    // so the empty-column case above cannot have been fixed by always aiming
    // at the ceiling.
    {
        BubbleGame game(renderer);
        BubbleGameTestAccess::reset(game, 4, false, false);
        BubbleArray& mini = BubbleGameTestAccess::player(game, 1);
        ShapeBoard(mini, false);
        mini.bubbleOffset = {20, 19};
        mini.leftLimit = 20; mini.rightLimit = 148; mini.topLimit = 19;
        mini.numColors = 4;
        for (int row = 0; row < 13; ++row) {
            const int size = (int)mini.bubbleMap[row].size();
            const int sep = (size % 2 == 0) ? 0 : 8;
            for (int col = 0; col < size; ++col)
                mini.bubbleMap[row][col].pos = {sep + 16 * col + 20, 14 * row + 19};
        }
        for (int row = 0; row <= 3; ++row)
            for (int col = 0; col < (int)mini.bubbleMap[row].size(); ++col)
                mini.bubbleMap[row][col].bubbleId = 1;

        mini.malusQueue.push_back(0);
        BubbleGameTestAccess::runMalus(game, 1, 1000);
        CHECK(malusBubbles.size() == 1);
        if (malusBubbles.size() == 1) {
            CHECK(malusBubbles[0].stickY == 4);
            malusBubbles[0].cx = 5;
            for (int frame = 0; frame < 4000 && !malusBubbles.empty(); ++frame)
                BubbleGameTestAccess::updateAtScale(game, 1.0f);
            CHECK(malusBubbles.empty());
            CHECK(mini.bubbleMap[4][5].bubbleId != -1);
        }
    }

    // --- Chain-reaction arc thresholds -------------------------------------
    // A chain bubble falls until it passes a screen-space threshold, then
    // swings back up to its target. The threshold belongs to the board's slot
    // (original bin/frozen-bubble line 2525), so a top mini board must turn
    // around while it is still near its own board rather than after crossing
    // the centre board on its way toward the bottom of the screen.
    {
        BubbleGame game(renderer);

        struct Case { int players; int array; int expected; const char* what; };
        const Case cases[] = {
            {2, 0, 380, "two-player left board"},
            {2, 1, 380, "two-player right board"},
            {4, 0, 380, "centre board"},
            {4, 1, 185, "top-left mini"},
            {4, 2, 185, "top-right mini"},
            {5, 3, 415, "bottom-left mini"},
            {5, 4, 415, "bottom-right mini"},
        };

        for (const Case& c : cases) {
            BubbleGameTestAccess::reset(game, c.players, false, false);
            BubbleArray& board = BubbleGameTestAccess::player(game, c.array);
            ShapeBoard(board, false);
            board.bubbleMap[0][0].bubbleId = 2;
            board.bubbleMap[1][0].bubbleId = 2;
            singleBubbles = {FallingBubble(c.array, 2, 100)};
            BubbleGameTestAccess::assignChains(game, c.array);
            CHECK(singleBubbles.size() == 1);
            if (singleBubbles.size() == 1) {
                CHECK(singleBubbles[0].chainExists);
                CHECK(singleBubbles[0].chainArcThreshold == c.expected);
                if (singleBubbles[0].chainArcThreshold != c.expected)
                    std::fprintf(stderr, "  (%s: got %d, wanted %d)\n",
                                 c.what, singleBubbles[0].chainArcThreshold,
                                 c.expected);
            }
        }

        // Above five players a board is parked into one of the same four
        // slots, so the threshold follows the slot rather than the array index.
        BubbleGameTestAccess::reset(game, 8, false, false);
        BubbleArray& parked = BubbleGameTestAccess::player(game, 6);
        ShapeBoard(parked, false);
        parked.parkedSlot = 3;  // bottom-right
        parked.bubbleMap[0][0].bubbleId = 2;
        parked.bubbleMap[1][0].bubbleId = 2;
        singleBubbles = {FallingBubble(6, 2, 100)};
        BubbleGameTestAccess::assignChains(game, 6);
        CHECK(singleBubbles.size() == 1);
        if (singleBubbles.size() == 1)
            CHECK(singleBubbles[0].chainArcThreshold == 415);
    }

    // And the threshold has to actually keep the swing inside the board: a
    // top-left mini board runs from y=19 to about y=201, so its chain bubble
    // must turn around near there. Under the old global 380 it sank to y=424
    // first -- through the centre board and into the bottom-left slot -- which
    // is what a player saw as a bubble adrift in someone else's board.
    {
        BubbleGame game(renderer);
        BubbleGameTestAccess::reset(game, 4, false, false);
        BubbleArray& mini = BubbleGameTestAccess::player(game, 1);
        ShapeBoard(mini, false);
        mini.bubbleOffset = {20, 19};
        mini.leftLimit = 20; mini.rightLimit = 148; mini.topLimit = 19;
        for (int row = 0; row < 13; ++row) {
            const int size = (int)mini.bubbleMap[row].size();
            const int sep = (size % 2 == 0) ? 0 : 8;
            for (int col = 0; col < size; ++col)
                mini.bubbleMap[row][col].pos = {sep + 16 * col + 20, 14 * row + 19};
        }
        mini.bubbleMap[0][0].bubbleId = 2;
        mini.bubbleMap[1][0].bubbleId = 2;

        SingleBubble faller{};
        faller.assignedArray = 1;
        faller.bubbleId = 2;
        faller.posX = 60; faller.posY = 100;
        faller.pos = {60, 100};
        faller.falling = true;
        faller.bubbleSize = 16;
        faller.leftLimit = 20; faller.rightLimit = 148; faller.topLimit = 19;
        singleBubbles = {faller};
        BubbleGameTestAccess::assignChains(game, 1);

        float deepest = 0.0f;
        for (int frame = 0; frame < 600; ++frame) {
            bool inFlight = false;
            for (const SingleBubble& sb : singleBubbles) {
                if (sb.chainExists && !sb.shouldClear && !sb.chainReachedDest) {
                    if (sb.posY > deepest) deepest = sb.posY;
                    inFlight = true;
                }
            }
            if (!inFlight) break;
            BubbleGameTestAccess::updateAtScale(game, 1.0f);
        }
        CHECK(deepest > 0.0f);
        CHECK(deepest < 230.0f);
        if (deepest >= 230.0f)
            std::fprintf(stderr, "  (mini chain bubble sank to y=%.1f)\n", deepest);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (failures == 0) std::printf("bubblegame rules tests passed\n");
    return failures == 0 ? 0 : 1;
}
