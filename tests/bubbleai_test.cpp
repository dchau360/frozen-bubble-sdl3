// The bot aims by flying a probe bubble through the real launch physics, so
// these tests are mostly about whether the prediction agrees with what the
// game would actually do, and whether the scoring prefers the shot a person
// would take.

#include <SDL3/SDL.h>

#include "bubbleai.h"
#include "bubblegame.h"
#include "bubblegame_internal.h"

#include <cstdio>

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

// Hex adjacency for a board, using the same oddswap convention as the game.
static bool TestAdjacent(const BubbleArray &board, int row, int col,
                         int otherRow, int otherCol) {
    const int oddswap = board.bubbleMap[0].size() == 8 ? 0 : 1;
    for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
        if (row + dr == otherRow && col + dc == otherCol) return true;
    }
    return false;
}

// Same candidate-angle sweep ChooseShot runs, so a case can prove a given
// cell is actually reachable by some angle.
static bool ReachableAt(BubbleArray &board, int colour, bool isMini,
                        int row, int col) {
    for (int i = 0; i < 61; ++i) {
        const float angle = 0.1f + (float(PI) - 0.2f) * (float)i / 60.0f;
        int r = -1, c = -1;
        if (BubbleAI::PredictLanding(board, angle, colour, isMini, &r, &c) &&
            r == row && c == col)
            return true;
    }
    return false;
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
        // Nothing on the board is this colour, so the lookahead below finds
        // no reachable follow-up and cannot add to the score -- leaving
        // "nothing to pop" an honest reading of shot.score. (The default,
        // 0, would land on the very wall this test builds and defeat that.)
        board.nextBubble = 9;

        unsigned rng = 999;
        const BubbleAI::Shot shot =
            BubbleAI::ChooseShot(board, 4, false, BubbleAI::Skill::Hard, &rng);
        CHECK(shot.valid);
        CHECK(shot.score <= 0);   // nothing to pop
        CHECK(shot.row == 4);     // as high as a shot can reach here
    }

    // A bot planning ahead credits a shot for the combo the bubble already
    // queued behind it can now make -- the same preview a person reads off
    // the launcher, not information a player would not have.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 3, 0);
        board.bubbleMap[3][3].bubbleId = 6;
        board.bubbleMap[3][4].bubbleId = 6;

        // No lookahead credit: nothing of this colour survives on the board.
        board.nextBubble = 9;
        unsigned rngA = 7;
        const auto plain = BubbleAI::ChooseShot(board, 6, false, BubbleAI::Skill::Hard, &rngA);
        CHECK(plain.valid && plain.score > 0);

        // Real credit: colour 0 still walls most of the board after this
        // pop, so a bubble of that colour would have plenty to land next to.
        board.nextBubble = 0;
        unsigned rngB = 7;
        const auto planned = BubbleAI::ChooseShot(board, 6, false, BubbleAI::Skill::Hard, &rngB);
        CHECK(planned.valid);
        // Same physical shot either way: the pop's base score dwarfs the
        // lookahead credit, which only tips the total up.
        CHECK(planned.row == plain.row && planned.col == plain.col);
        // But it is credited more for the combo the known next bubble sets up.
        CHECK(planned.score > plain.score);
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
        // The regression this pins: Easy used to connect about half the
        // time (miss 50%, and even a miss drew uniformly from every ranked
        // shot including the near-best ones) -- reported live as still too
        // strong a floor. It now connects closer to a fifth of the time,
        // and a miss is restricted to the worse half of the ranked list;
        // this measured 6/24 against the fixed seed above, so the bound
        // leaves real margin without letting the old ~12/24 behaviour back in.
        CHECK(easyBest <= 10);
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

    // When nothing can pop, the cluster term is what picks the shot: the bot
    // parks beside its own colour rather than anywhere else, because landing
    // next to a matching bubble is the only way a dead shot can set up a later
    // pop. With a single reachable colour-6 bubble the best any angle can do
    // is a pair, so the chosen cell must touch that bubble.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 2, 0);
        board.bubbleMap[3][3].bubbleId = 6;
        // No colour 9 exists anywhere, so the lookahead cannot add anything
        // and the cluster term above decides the shot on its own.
        board.nextBubble = 9;

        unsigned rng = 4242;
        const auto shot = BubbleAI::ChooseShot(board, 6, false,
                                               BubbleAI::Skill::Hard, &rng);
        CHECK(shot.valid);
        // The premise: this shot cannot complete a group of three.
        CHECK(BubbleAI::ScoreLanding(board, shot.row, shot.col, 6) == 0);

        bool besideOwnColour = false;
        for (int r = 0; r < 13; ++r)
            for (int c = 0; c < (int)board.bubbleMap[r].size(); ++c)
                if (board.bubbleMap[r][c].bubbleId == 6 &&
                    TestAdjacent(board, shot.row, shot.col, r, c))
                    besideOwnColour = true;
        CHECK(besideOwnColour);
    }

    // A smaller pop that cuts a stem loose is worth more than a bigger pop
    // that leaves the board hanging from the ceiling. The two landings sit on
    // one board; the colour-2 tail only falls when the colour-7 stem above it
    // goes, so it counts against that landing and no other.
    //
    // The margins are deliberate. The landings trade two popped bubbles
    // against two detached ones, and sit in the same row and both on an edge,
    // so every other term cancels and the assertion holds if and only if
    // kDetached > kPopped -- exactly the claim the weights are meant to
    // encode. An earlier version traded one popped bubble against four
    // detached and so still passed with kDetached as low as 5, which is
    // *below* kPopped and inverts the whole point of the term.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        // Left side: a two-bubble colour-7 stem at the ceiling with a
        // colour-2 tail hanging off it. Landing at (1,0) pops the stem of
        // three and drops the tail of two.
        board.bubbleMap[0][0].bubbleId = 7;
        board.bubbleMap[0][1].bubbleId = 7;
        board.bubbleMap[1][1].bubbleId = 2;
        board.bubbleMap[2][1].bubbleId = 2;
        // Right side: a five-bubble colour-7 run ending in the landing (1,6),
        // which pops five and leaves nothing dangling.
        board.bubbleMap[0][5].bubbleId = 7;
        board.bubbleMap[0][6].bubbleId = 7;
        board.bubbleMap[0][7].bubbleId = 7;
        board.bubbleMap[1][5].bubbleId = 7;

        const int pop5 = BubbleAI::ScoreShot(board, 1, 6, 7);
        const int pop3Cut2 = BubbleAI::ScoreShot(board, 1, 0, 7);
        CHECK(pop3Cut2 > pop5);
    }

    // A bubble parked on the ceiling row is a permanent anchor that can never
    // be shaken loose by a cut, so the bot mildly avoids it: the same shot one
    // row lower scores better even though neither pops anything.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        // Both landings are isolated (no neighbour of the launch colour) and
        // mid-row, so only the row differs.
        CHECK(BubbleAI::ScoreShot(board, 1, 3, 5) > BubbleAI::ScoreShot(board, 0, 3, 5));
    }

    // The centre lane is where a deep column is most likely to reach the
    // floor, so the bot prefers the outside of a row when all else is equal.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        // Same row, same isolated landing; only the column differs.
        CHECK(BubbleAI::ScoreShot(board, 1, 0, 5) > BubbleAI::ScoreShot(board, 1, 3, 5));
    }

    // The headline change: a deep pop near the floor is worth less than
    // building a cluster high up, because depth is what actually ends a
    // round. The bot declines the three-pop at the danger line in favour of a
    // pair beside its own colour near the ceiling. nextBubble is a colour that
    // exists nowhere, so the lookahead cannot muddy the comparison.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 2, 0);
        board.bubbleMap[3][3].bubbleId = 6;   // high cluster target
        board.bubbleMap[12][5].bubbleId = 6;  // deep pair: landing at (11,4)
        board.bubbleMap[12][6].bubbleId = 6;  // pops three, near the floor
        board.nextBubble = 9;

        // Both the high pair and the deep pop are genuinely on offer; without
        // this the case would be vacuous.
        CHECK(ReachableAt(board, 6, false, 3, 2));
        CHECK(ReachableAt(board, 6, false, 11, 4));

        unsigned rng = 4242;
        const auto shot = BubbleAI::ChooseShot(board, 6, false,
                                               BubbleAI::Skill::Hard, &rng);
        CHECK(shot.valid);
        // It chose the high cluster, not the deep pop.
        CHECK(shot.row == 3);
        CHECK(BubbleAI::ScoreLanding(board, shot.row, shot.col, 6) == 0);
    }

    // The counterweight to the cluster preference: when the board is already
    // deep, a real pop is worth far more than another cluster, and the bot
    // takes it. A bot that only built clusters while the rows descended would
    // lose here; the pop's own weight keeps it honest.
    {
        BubbleArray board;
        ShapeCentreBoard(board);
        FillRows(board, 8, 0);
        board.bubbleMap[9][3].bubbleId = 6;  // mid-board pair: landing at
        board.bubbleMap[9][4].bubbleId = 6;  // (9,2) or (9,5) pops three
        board.nextBubble = 9;

        unsigned rng = 4242;
        const auto shot = BubbleAI::ChooseShot(board, 6, false,
                                               BubbleAI::Skill::Hard, &rng);
        CHECK(shot.valid);
        CHECK(BubbleAI::ScoreLanding(board, shot.row, shot.col, 6) >= 3);
    }

    SDL_Quit();
    if (failures == 0) std::printf("bubbleai tests passed\n");
    return failures == 0 ? 0 : 1;
}
