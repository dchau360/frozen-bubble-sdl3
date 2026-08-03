#include <SDL3_image/SDL_image.h>

#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "localmultiplayer_settings.h"
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
    static void assignChains(BubbleGame& game, int idx) {
        game.AssignChainReactions(game.bubbleArrays[idx]);
    }

    static void reset(BubbleGame& game, int players, bool network, bool clearMode) {
        singleBubbles.clear();
        malusBubbles.clear();
        game.currentSettings = {};
        game.currentSettings.playerCount = players;
        game.currentSettings.networkGame = network;
        game.currentSettings.clearMode = clearMode;
        game.gameFinish = game.gameLost = game.gameMatchOver = false;
        game.wonByClearing = false;
        game.roundWinnerIdx = -1;
        game.connectedPlayerCount = players;
        game.winsP1 = game.winsP2 = 0;
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

        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleGameTestAccess::check(game, 0);
        CHECK(!BubbleGameTestAccess::finished(game));

        BubbleGameTestAccess::reset(game, 2, false, true);
        BubbleGameTestAccess::check(game, 0);
        CHECK(BubbleGameTestAccess::finished(game));
        CHECK(BubbleGameTestAccess::cleared(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);

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
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (failures == 0) std::printf("bubblegame rules tests passed\n");
    return failures == 0 ? 0 : 1;
}
