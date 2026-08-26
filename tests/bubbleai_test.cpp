// The bot aims by flying a probe bubble through the real launch physics, so
// these tests are mostly about whether the prediction agrees with what the
// game would actually do, and whether the scoring prefers the shot a person
// would take.

#include <SDL3/SDL.h>

#include "bubbleai.h"
#include "bubblegame.h"
#include "bubblegame_internal.h"

#include <cstdio>
#include <set>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

// The centre board of a 3+ player game (bubblegame.cpp, case 5).
static void ShapeCentreBoard(BubbleArray &board) {
    board.playerAssigned = 0;
    board.bubbleOffset = {190, 44};
    board.leftLimit = 190;
    board.rightLimit = 446;
    board.topLimit = 44;
    board.curLaunchRct = {302, 390, 32, 32};
    for (int row = 0; row < 13; ++row) {
        const int width = (row % 2 == 0) ? 8 : 7;
        board.bubbleMap[row].assign(width, Bubble{});
        for (int col = 0; col < width; ++col) {
            board.bubbleMap[row][col].bubbleId = -1;
            board.bubbleMap[row][col].pos = {
                board.bubbleOffset.x + 32 * col + (row % 2 ? 16 : 0),
                board.bubbleOffset.y + 28 * row,
            };
        }
    }
}

static void FillRows(BubbleArray &board, int through, int colour) {
    for (int row = 0; row <= through; ++row)
        for (int col = 0; col < (int)board.bubbleMap[row].size(); ++col)
            board.bubbleMap[row][col].bubbleId = colour;
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);

    // --- ScoreLanding ------------------------------------------------------
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 2, 0);

        // Three of a colour only counts once the landing completes the run,
        // so a lone neighbour of the right colour is worth nothing.
        board.bubbleMap[2][3].bubbleId = 5;
        board.bubbleMap[3][3].bubbleId = -1;
        CHECK(BubbleAI::ScoreLanding(board, 3, 3, 5) == 0);

        // A pair plus the landing is a pop of three.
        board.bubbleMap[2][4].bubbleId = 5;
        CHECK(BubbleAI::ScoreLanding(board, 3, 3, 5) >= 3);

        // The wrong colour into the same cell still pops nothing.
        CHECK(BubbleAI::ScoreLanding(board, 3, 3, 1) == 0);

        // An occupied cell is not a landing at all.
        CHECK(BubbleAI::ScoreLanding(board, 2, 4, 5) == 0);
    }

    // Bubbles cut loose by the pop count toward the shot, which is what makes
    // a ceiling-row break worth far more than the three bubbles it removes.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        // A colour-7 bar across row 0 holding a single bubble beneath it.
        board.bubbleMap[0][0].bubbleId = 7;
        board.bubbleMap[0][1].bubbleId = 7;
        board.bubbleMap[1][0].bubbleId = 3;
        board.bubbleMap[2][0].bubbleId = 3;

        const int score = BubbleAI::ScoreLanding(board, 0, 2, 7);
        // Three sevens go, and both threes lose their grip on the ceiling.
        CHECK(score == 5);
    }

    // --- PredictLanding ----------------------------------------------------
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 3, 0);

        // Straight up into a flat wall of bubbles: the probe has to stop under
        // it rather than passing through or reaching the ceiling.
        int row = -1, col = -1;
        CHECK(BubbleAI::PredictLanding(board, (float)PI / 2.0f, 1, false, &row, &col));
        CHECK(row == 4);
        CHECK(board.bubbleMap[row][col].bubbleId == -1);
    }

    {
        BubbleArray board;
        ShapeCentreBoard(board);
        // Empty board: the probe should run all the way to the ceiling row.
        int row = -1, col = -1;
        CHECK(BubbleAI::PredictLanding(board, (float)PI / 2.0f, 1, false, &row, &col));
        CHECK(row == 0);
    }

    // --- ChooseShot --------------------------------------------------------
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 3, 0);
        // Give row 3 a reachable pair of colour 6 with a gap under it.
        board.bubbleMap[3][3].bubbleId = 6;
        board.bubbleMap[3][4].bubbleId = 6;

        unsigned rng = 12345;
        const BubbleAI::Shot shot =
            BubbleAI::ChooseShot(board, 6, false, BubbleAI::Skill::Hard, &rng);
        CHECK(shot.valid);
        // It found the pop rather than parking the bubble somewhere.
        CHECK(shot.score > 0);
        CHECK(BubbleAI::ScoreLanding(board, shot.row, shot.col, 6) >= 3);
        CHECK(shot.angle >= 0.1f && shot.angle <= (float)PI - 0.1f);

        // Firing that angle for real must land where the bot was told it would.
        int row = -1, col = -1;
        CHECK(BubbleAI::PredictLanding(board, shot.angle, 6, false, &row, &col));
        CHECK(row == shot.row && col == shot.col);
    }

    // With no pop available the bot still commits to a shot, and prefers to
    // park it high: depth is what ends the round.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 3, 0);

        unsigned rng = 999;
        const BubbleAI::Shot shot =
            BubbleAI::ChooseShot(board, 4, false, BubbleAI::Skill::Hard, &rng);
        CHECK(shot.valid);
        CHECK(shot.score <= 0);   // nothing to pop
        CHECK(shot.row == 4);     // as high as a shot can reach here
    }

    // Skill is which ranked shot it settles for. An easy bot must actually
    // pass up the best one sometimes, and a hard bot never must.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 3, 0);
        board.bubbleMap[3][3].bubbleId = 6;
        board.bubbleMap[3][4].bubbleId = 6;

        unsigned hardRng = 7;
        int hardBest = 0;
        for (int i = 0; i < 24; ++i) {
            const auto shot = BubbleAI::ChooseShot(board, 6, false,
                                                   BubbleAI::Skill::Hard, &hardRng);
            if (shot.score > 0) ++hardBest;
        }
        CHECK(hardBest == 24);

        unsigned easyRng = 7;
        int easyBest = 0;
        for (int i = 0; i < 24; ++i) {
            const auto shot = BubbleAI::ChooseShot(board, 6, false,
                                                   BubbleAI::Skill::Easy, &easyRng);
            if (shot.score > 0) ++easyBest;
        }
        CHECK(easyBest < 24);
    }

    // A mini side board uses half-size bubbles and its own geometry; the same
    // prediction has to hold there, since that is where most bots will play.
    {
        BubbleArray board;
        board.playerAssigned = 1;
        board.bubbleOffset = {20, 19};
        board.leftLimit = 20;
        board.rightLimit = 148;
        board.topLimit = 19;
        board.curLaunchRct = {78, 192, 16, 16};
        for (int row = 0; row < 13; ++row) {
            const int width = (row % 2 == 0) ? 8 : 7;
            board.bubbleMap[row].assign(width, Bubble{});
            for (int col = 0; col < width; ++col) {
                board.bubbleMap[row][col].bubbleId = -1;
                board.bubbleMap[row][col].pos = {
                    board.bubbleOffset.x + 16 * col + (row % 2 ? 8 : 0),
                    board.bubbleOffset.y + 14 * row,
                };
            }
        }
        FillRows(board, 3, 0);
        board.bubbleMap[3][3].bubbleId = 6;
        board.bubbleMap[3][4].bubbleId = 6;

        unsigned rng = 31;
        const auto shot = BubbleAI::ChooseShot(board, 6, true, BubbleAI::Skill::Hard, &rng);
        CHECK(shot.valid);
        CHECK(shot.score > 0);
    }

    SDL_Quit();
    if (failures == 0) std::printf("bubbleai tests passed\n");
    return failures == 0 ? 0 : 1;
}
