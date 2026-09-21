// Replay-foundation boundary tests (R1): AdvanceSimulation()/Draw() must be
// separable, and SessionMode::Playback must suppress external effects while
// keeping the in-memory result state.
//
// These are deliberately behavioural, not mirror-of-implementation: they
// drive the real BubbleGame (NewGame, AdvanceSimulation, Draw, SubmitScore,
// FinalizeRoundStats, SendGameDataFor) and observe gameplay state, RNG state
// and the NetworkClient test counters.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "networkclient.h"
#include "platform.h"

#include <cstdio>
#include <cmath>

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
    static void seedRng(BubbleGame& game, unsigned seed) {
        game.rng.Seed(seed);
        game.rngExplicitlySeeded = true;
    }
    static GameplayRng& rng(BubbleGame& game) { return game.rng; }
    static void setSessionMode(BubbleGame& game, BubbleGame::SessionMode mode) {
        game.sessionMode = mode;
    }
    static BubbleGame::SessionMode sessionMode(BubbleGame& game) { return game.sessionMode; }
    static int& simStep(BubbleGame& game) { return game.simStep; }
    static int& frameCount(BubbleGame& game) { return game.frameCount; }
    static int& comboDisplayTimer(BubbleGame& game) { return game.comboDisplayTimer; }
    static bool comboVisible(BubbleGame& game) { return game.comboTextVisible; }
    static void advance(BubbleGame& game) { game.AdvanceSimulation(); }
    static void draw(BubbleGame& game) { game.Draw(); }
    static void finalizeStats(BubbleGame& game) { game.FinalizeRoundStats(); }
    static void submitScore(BubbleGame& game, int idx) { game.SubmitScore(game.bubbleArrays[idx]); }
    static bool sendGameData(BubbleGame& game, const char* payload) {
        return game.SendGameDataFor(game.bubbleArrays[0], payload);
    }
    static bool pendingHighscore(BubbleGame& game) { return game.pendingHighscore; }
    static bool& finished(BubbleGame& game) { return game.gameFinish; }
    static bool& won(BubbleGame& game) { return game.gameWon; }
};

struct NetworkClientTestAccess {
    static void SetState(NetworkClient& nc, ConnectionState state) { nc.state = state; }
    static void SetMyPlayerId(NetworkClient& nc, int id) { nc.myPlayerId = (unsigned char)id; }
};

// A falling bubble the single-player physics loop will advance, without a
// board or a shooter to build it up for us. genSpeed starts at zero, so it
// needs a couple of steps before its screen Y actually moves; the Draw-purity
// check only needs the value to stay identical across draws, which it does.
static SingleBubble MakeFallingBubble(GameplayRng& rng) {
    SingleBubble bubble{};
    bubble.assignedArray = 0;
    bubble.bubbleId = 0;
    bubble.posX = bubble.oldPosX = 320.0f;
    bubble.posY = bubble.oldPosY = 100.0f;
    bubble.pos = bubble.oldpos = {320, 100};
    bubble.bubbleSize = 32;
    bubble.leftLimit = 192;
    bubble.rightLimit = 448;
    bubble.topLimit = 51;
    bubble.GenerateFreeFall(rng);
    return bubble;
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_AUDIODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("replay-session-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // --- Draw() is pure; AdvanceSimulation() advances exactly one step ------
    {
        BubbleGame game(renderer);
        BubbleGameTestAccess::seedRng(game, 1234u);

        SetupSettings setup;
        setup.playerCount = 1;
        setup.networkGame = false;
        setup.randomLevels = true;
        game.NewGame(setup);

        singleBubbles.clear();
        singleBubbles.push_back(MakeFallingBubble(BubbleGameTestAccess::rng(game)));

        const int sim0 = BubbleGameTestAccess::simStep(game);
        const int hurry0 = BubbleGameTestAccess::player(game, 0).hurryTimer;
        const uint32_t rng0 = BubbleGameTestAccess::rng(game).State();
        const float fall0 = singleBubbles[0].posY;
        const int combo0 = BubbleGameTestAccess::comboDisplayTimer(game);

        // Repeated draws (no simulation step) must not mutate gameplay state,
        // consume RNG, or advance timers/projectiles.
        BubbleGameTestAccess::draw(game);
        BubbleGameTestAccess::draw(game);
        BubbleGameTestAccess::draw(game);
        CHECK(BubbleGameTestAccess::simStep(game) == sim0);
        CHECK(BubbleGameTestAccess::player(game, 0).hurryTimer == hurry0);
        CHECK(BubbleGameTestAccess::rng(game).State() == rng0);
        CHECK(singleBubbles.size() == 1);
        CHECK(std::fabs(singleBubbles[0].posY - fall0) < 0.0001f);
        CHECK(BubbleGameTestAccess::comboDisplayTimer(game) == combo0);

        // One simulation step advances the step counter and per-frame timers
        // exactly once, and no more.
        BubbleGameTestAccess::advance(game);
        CHECK(BubbleGameTestAccess::simStep(game) == sim0 + 1);
        CHECK(BubbleGameTestAccess::player(game, 0).hurryTimer == hurry0 + 1);
        BubbleGameTestAccess::advance(game);
        CHECK(BubbleGameTestAccess::simStep(game) == sim0 + 2);
        CHECK(BubbleGameTestAccess::player(game, 0).hurryTimer == hurry0 + 2);
    }

    // --- comboText draw-then-age is a draw/mutator crossover -----------------
    // The blit lives in Draw(); the per-step decrement lives in
    // AdvanceSimulation(). A redundant draw must not age the timer, and the
    // latch must still make the final frame visible (the draw-then-age order
    // is preserved through comboTextVisible).
    {
        BubbleGame game(renderer);
        BubbleGameTestAccess::seedRng(game, 99u);

        SetupSettings setup;
        setup.playerCount = 1;
        setup.networkGame = false;
        setup.randomLevels = true;
        game.NewGame(setup);

        BubbleGameTestAccess::comboDisplayTimer(game) = 2;
        BubbleGameTestAccess::draw(game);
        CHECK(BubbleGameTestAccess::comboDisplayTimer(game) == 2);  // draw does not age
        CHECK(BubbleGameTestAccess::comboVisible(game) == false);   // latch not yet set

        BubbleGameTestAccess::advance(game);
        CHECK(BubbleGameTestAccess::comboDisplayTimer(game) == 1);
        CHECK(BubbleGameTestAccess::comboVisible(game) == true);
        BubbleGameTestAccess::advance(game);
        CHECK(BubbleGameTestAccess::comboDisplayTimer(game) == 0);
        CHECK(BubbleGameTestAccess::comboVisible(game) == true);   // last visible frame
        BubbleGameTestAccess::advance(game);
        CHECK(BubbleGameTestAccess::comboDisplayTimer(game) == 0);
        CHECK(BubbleGameTestAccess::comboVisible(game) == false);
    }

    // --- SessionMode: Playback suppresses gameplay network sends -----------
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 2;
        settings.networkGame = true;
        BubbleGameTestAccess::player(game, 0).playerAssigned = 0;
        BubbleGameTestAccess::player(game, 1).playerAssigned = 1;

        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetState(*nc, IN_GAME);
        NetworkClientTestAccess::SetMyPlayerId(*nc, 7);

        // Live reaches the socket path (the native SendGameData increments the
        // test counter even though the headless socket is closed).
        nc->testGameDataSendCount = 0;
        BubbleGameTestAccess::setSessionMode(game, BubbleGame::SessionMode::Live);
        CHECK(BubbleGameTestAccess::sendGameData(game, "f0.5:1") == false);  // no real socket
        CHECK(nc->testGameDataSendCount == 1);                               // but the send path ran

        // Playback stops at BubbleGame::SendGameDataFor before NetworkClient.
        nc->testGameDataSendCount = 0;
        BubbleGameTestAccess::setSessionMode(game, BubbleGame::SessionMode::Playback);
        CHECK(BubbleGameTestAccess::sendGameData(game, "f0.5:1") == false);
        CHECK(nc->testGameDataSendCount == 0);

        // FinalizeRoundStats in Playback keeps the in-memory totals it needs to
        // display the recorded result but sends nothing.
        BubbleArray& p0 = BubbleGameTestAccess::player(game, 0);
        p0.rFired = 4; p0.rPopped = 3; p0.rSent = 2; p0.rRecv = 1; p0.rKills = 0; p0.rBlk = 0;
        nc->testGameDataSendCount = 0;
        BubbleGameTestAccess::finalizeStats(game);
        CHECK(p0.mFired == 4 && p0.mPopped == 3 && p0.mSent == 2 && p0.mRecv == 1);
        CHECK(nc->testGameDataSendCount == 0);

        NetworkClientTestAccess::SetState(*nc, DISCONNECTED);
    }

    // --- SessionMode: Playback suppresses highscore persistence -------------
    // SubmitScore must keep the score in memory but never touch the highscore
    // manager (pendingHighscore stays false, no AppendToLevels/CheckAndAddScore
    // side effect to observe from a headless test).
    {
        BubbleGame game(renderer);
        SetupSettings& settings = BubbleGameTestAccess::settings(game);
        settings.playerCount = 1;
        settings.networkGame = false;
        settings.randomLevels = true;
        BubbleGameTestAccess::player(game, 0).score = 1000;
        BubbleGameTestAccess::setSessionMode(game, BubbleGame::SessionMode::Playback);

        BubbleGameTestAccess::submitScore(game, 0);
        CHECK(BubbleGameTestAccess::player(game, 0).score == 1000);  // in-memory retained
        CHECK(BubbleGameTestAccess::pendingHighscore(game) == false);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (failures == 0) std::printf("replay session tests passed\n");
    return failures == 0 ? 0 : 1;
}
