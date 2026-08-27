// The AI is unit-tested on its own; this runs it through the actual game.
// A bot drives the same shooter flags a keyboard does, so the only honest
// check is to let a real BubbleGame run for a while and see whether the bot
// slots behave like someone is playing them.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "localmultiplayer_settings.h"
#include "platform.h"

#include <cstdio>
#include <cstdlib>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct BubbleGameTestAccess {
    static BubbleArray& player(BubbleGame& game, int idx) {
        return game.bubbleArrays[idx];
    }
    static void step(BubbleGame& game, float scale) {
        game.UpdateSingleBubblesAtScale(scale);
    }
    // NewGame seeds each bot from the clock so a real game never repeats
    // itself. A test needs the opposite.
    static void seedBots(BubbleGame& game, int players, unsigned seed) {
        for (int p = 0; p < players; ++p) {
            game.bubbleArrays[p].botRng = seed + static_cast<unsigned>(p) * 7919u;
            game.bubbleArrays[p].botTargetAngle = -1.0f;
            game.bubbleArrays[p].botThinkFrames = 0;
        }
    }
};

// One frame of the parts of the loop a bot's turn depends on.
static void RunFrames(BubbleGame& game, int players, int frames) {
    for (int frame = 0; frame < frames; ++frame) {
        for (int p = 0; p < players; ++p)
            game.UpdatePenguin(BubbleGameTestAccess::player(game, p));
        BubbleGameTestAccess::step(game, 1.0f);
    }
}

static int Occupied(const BubbleArray& board) {
    int count = 0;
    for (const auto& row : board.bubbleMap)
        for (const Bubble& bubble : row)
            if (bubble.bubbleId != -1) ++count;
    return count;
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_AUDIODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("bot-play-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // A four-player game where player 1 is a person who never touches the
    // controls and 2-4 are bots.
    //
    // Run over several seeds rather than one. Both sources of randomness have
    // to be pinned for that to mean anything: the level comes from the game's
    // own rand(), and NewGame seeds each bot from the wall clock so a real
    // game never repeats itself. Leaving either loose made this block a
    // different experiment on every run -- which is exactly how it eventually
    // failed in CI while passing locally.
    for (unsigned seed = 1; seed <= 5; ++seed) {
        LocalMultiplayerOptions options;
        options.playerCount = 4;
        options.botCount = 3;
        options.botSkill = 2;   // hard, so the result is not luck

        std::srand(seed * 22699u);
        BubbleGame game(renderer);
        game.NewGame(BuildLocalMultiplayerSettings(options));
        BubbleGameTestAccess::seedBots(game, 4, seed * 55291u);

        CHECK(!BubbleGameTestAccess::player(game, 0).isBot);
        for (int p = 1; p < 4; ++p)
            CHECK(BubbleGameTestAccess::player(game, p).isBot);

        RunFrames(game, 4, 4000);

        const BubbleArray& human = BubbleGameTestAccess::player(game, 0);
        for (int p = 1; p < 4; ++p) {
            const BubbleArray& bot = BubbleGameTestAccess::player(game, p);
            // It takes its turns rather than sitting on the trigger. The
            // untouched human slot only fires when the hurry timer forces it,
            // so a bot must be well clear of that rate.
            CHECK(bot.rFired > human.rFired);
            CHECK(bot.rFired >= 10);
            // And it aims: shots land where they clear bubbles. An unaimed
            // shot pops occasionally by luck, so this is about every bot
            // doing it, not about any single pop.
            CHECK(bot.rPopped > 0);
            // Clearing bubbles keeps the board off the floor.
            CHECK(Occupied(bot) < 13 * 8);
            if (bot.rPopped == 0 || Occupied(bot) >= 13 * 8)
                std::fprintf(stderr, "  (seed %u, bot %d: fired %d popped %d occupied %d)\n",
                             seed, p, bot.rFired, bot.rPopped, Occupied(bot));
        }
    }

    // A hard bot should outplay an easy one. One round is noisy -- a lucky
    // random angle pops as well as a chosen one -- so this compares totals
    // across several seeded rounds rather than a single game.
    {
        auto poppedOverRounds = [&](int skill) {
            int total = 0;
            for (unsigned seed = 1; seed <= 6; ++seed) {
                LocalMultiplayerOptions options;
                options.playerCount = 2;
                options.botCount = 1;
                options.botSkill = skill;
                // The board itself comes from the game's rand(); pin it so
                // the two skills are compared on identical levels.
                std::srand(seed * 7717u);
                BubbleGame game(renderer);
                game.NewGame(BuildLocalMultiplayerSettings(options));
                BubbleGameTestAccess::seedBots(game, 2, seed * 104729u);
                RunFrames(game, 2, 3000);
                total += BubbleGameTestAccess::player(game, 1).rPopped;
            }
            return total;
        };
        const int hard = poppedOverRounds(2);
        const int easy = poppedOverRounds(0);
        CHECK(hard > easy);
        if (hard <= easy)
            std::fprintf(stderr, "  (hard popped %d, easy popped %d)\n", hard, easy);
    }

    // Zero bots must leave every slot to its player, untouched.
    {
        LocalMultiplayerOptions options;
        options.playerCount = 3;
        options.botCount = 0;

        std::srand(4242);
        BubbleGame game(renderer);
        game.NewGame(BuildLocalMultiplayerSettings(options));
        for (int p = 0; p < 3; ++p)
            CHECK(!BubbleGameTestAccess::player(game, p).isBot);

        RunFrames(game, 3, 4000);
        // Nobody is at the controls. The hurry timer still force-fires an
        // idle board eventually -- and a forced shot lands somewhere, so it
        // can pop by accident -- but nothing is taking turns at a bot's rate.
        for (int p = 0; p < 3; ++p)
            CHECK(BubbleGameTestAccess::player(game, p).rFired < 10);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (failures == 0) std::printf("bot play tests passed\n");
    return failures == 0 ? 0 : 1;
}
