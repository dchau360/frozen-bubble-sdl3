/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "bubbleai.h"
#include "bubblegame_internal.h"

#include <array>
#include <queue>
#include <set>
#include <utility>
#include <vector>

namespace BubbleAI {
namespace {

// The shooter cannot be aimed flat; BubbleGame::UpdateShooter clamps here.
constexpr float kMinAngle = 0.1f;
constexpr float kMaxAngle = static_cast<float>(PI) - 0.1f;
// Candidate angles per shot. 61 puts them about 3 degrees apart, which is
// finer than the grid can distinguish at the top of the board and cheap
// enough to run for several bots on a phone.
constexpr int kCandidates = 61;
// A shot that never lands has to be abandoned eventually. A bubble crossing
// the tallest board takes well under this many substeps even bouncing.
constexpr int kMaxSubsteps = 4000;

// Shot scoring, ported from the zepr/fbjs clone's cpu.js:
// https://github.com/zepr/fbjs/blob/master/src/main/resources/static/game/scripts/cpu.js
// (GPL-3.0; see README.md's Credits section) -- popped=4, detached=6,
// cluster=4*groupSize, plus a hand-tuned 1..5 positional heatmap -- scaled
// up by 3 to stay in integers. The old score was
// `popped * 1000 - row * 10 + followUp * weight`, which had two defects:
// nothing rewarded a shot that lands next to its own colour without popping,
// so when no candidate popped every candidate tied at -row*10 and
// std::sort's instability picked an arbitrary one; and a pop, however bad,
// always dwarfed every other consideration. Their `4*groupSize` counts the
// placed bubble itself, so the decision-relevant delta between an isolated
// landing and a cluster join is 4 -- one popped bubble -- and their whole
// positional spread is likewise worth about one popped bubble.
constexpr int kPopped = 12;         // per bubble in the popped group
constexpr int kDetached = 18;        // per bubble cut loose; a cut is worth more than a match
constexpr int kCluster = 12;        // per same-colour neighbour joined when the group is < 3
constexpr int kRowPenalty = 1;      // per row down; the whole board's spread is ~one popped bubble
constexpr int kCeilingPenalty = 6;  // a row-0 bubble is a permanent anchor and can never fall in a cut
constexpr int kEdgeBonus = 4;       // keep the centre lane open
constexpr int kDangerPenalty = 30;  // rows >= 11 are close to ending the round
constexpr int kDangerRow = 11;

using Grid = std::array<std::vector<int>, 13>;

Grid ColourGrid(const BubbleArray &board) {
    Grid grid;
    for (size_t row = 0; row < board.bubbleMap.size(); ++row) {
        grid[row].reserve(board.bubbleMap[row].size());
        for (const Bubble &bubble : board.bubbleMap[row])
            grid[row].push_back(bubble.bubbleId);
    }
    return grid;
}

bool InBounds(const Grid &grid, int row, int col) {
    return row >= 0 && row < static_cast<int>(grid.size()) &&
           col >= 0 && col < static_cast<int>(grid[row].size());
}

// Hex neighbours, using the same oddswap convention as the rest of the game.
std::vector<std::pair<int, int>> Neighbours(const Grid &grid, int row, int col) {
    const int oddswap = grid[0].size() == 8 ? 0 : 1;
    std::vector<std::pair<int, int>> out;
    for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
        if (InBounds(grid, row + dr, col + dc)) out.emplace_back(row + dr, col + dc);
    }
    return out;
}

// Connected run of one colour containing (row, col).
std::vector<std::pair<int, int>> SameColourGroup(const Grid &grid, int row, int col) {
    const int colour = grid[row][col];
    std::vector<std::pair<int, int>> group;
    std::set<std::pair<int, int>> seen{{row, col}};
    std::queue<std::pair<int, int>> queue;
    queue.push({row, col});
    while (!queue.empty()) {
        auto cell = queue.front();
        queue.pop();
        group.push_back(cell);
        for (auto n : Neighbours(grid, cell.first, cell.second)) {
            if (seen.count(n) || grid[n.first][n.second] != colour) continue;
            seen.insert(n);
            queue.push(n);
        }
    }
    return group;
}

// Bubbles no longer reachable from the ceiling row -- the same rule
// CheckAirBubbles applies after a pop. Clears them out of the grid in place
// (they would actually fall) and returns how many there were.
int SweepDetached(Grid &grid) {
    std::set<std::pair<int, int>> connected;
    std::queue<std::pair<int, int>> queue;
    for (size_t col = 0; col < grid[0].size(); ++col) {
        if (grid[0][col] != -1) {
            queue.push({0, static_cast<int>(col)});
            connected.insert({0, static_cast<int>(col)});
        }
    }
    while (!queue.empty()) {
        auto [row, col] = queue.front();
        queue.pop();
        for (auto n : Neighbours(grid, row, col)) {
            if (connected.count(n) || grid[n.first][n.second] == -1) continue;
            connected.insert(n);
            queue.push(n);
        }
    }
    int detached = 0;
    for (size_t row = 1; row < grid.size(); ++row) {
        for (size_t col = 0; col < grid[row].size(); ++col) {
            const std::pair<int, int> cell{static_cast<int>(row), static_cast<int>(col)};
            if (grid[row][col] != -1 && !connected.count(cell)) {
                grid[row][col] = -1;
                ++detached;
            }
        }
    }
    return detached;
}

// Places `colour` at (row, col) and, if that completes a group of three or
// more, pops it and sweeps whatever it detaches -- mutating `grid` to the
// board that shot would actually leave behind. Returns the same score
// ScoreLanding does (the popped group plus everything it detaches), so
// callers that only want the total need not change. Split out from
// ScoreLanding so a caller that needs the resulting grid (the lookahead
// below) does not have to re-simulate the shot to get it. The optional
// out-params expose the pieces separately so ChooseShot can weight a popped
// bubble differently from a detached one; every one is null-checked because
// they all default to nullptr.
int PlaceAndScore(Grid &grid, int row, int col, int colour,
                  int *poppedOut = nullptr, int *detachedOut = nullptr,
                  int *groupSizeOut = nullptr) {
    if (!InBounds(grid, row, col) || grid[row][col] != -1) {
        if (poppedOut) *poppedOut = 0;
        if (detachedOut) *detachedOut = 0;
        if (groupSizeOut) *groupSizeOut = 0;
        return 0;
    }
    grid[row][col] = colour;

    const auto group = SameColourGroup(grid, row, col);
    if (group.size() < 3) {
        // Too small to pop: the shot only matters for the cluster it joined.
        // groupSizeOut is the connected same-colour group containing the
        // landing cell after the placement, so a caller can count how many
        // of its own colour it has landed beside.
        if (poppedOut) *poppedOut = 0;
        if (detachedOut) *detachedOut = 0;
        if (groupSizeOut) *groupSizeOut = static_cast<int>(group.size());
        return 0;
    }
    for (auto [r, c] : group) grid[r][c] = -1;
    const int detached = SweepDetached(grid);
    if (poppedOut) *poppedOut = static_cast<int>(group.size());
    if (detachedOut) *detachedOut = detached;
    if (groupSizeOut) *groupSizeOut = static_cast<int>(group.size());
    return static_cast<int>(group.size()) + detached;
}

// The ranking ChooseShot applies to a single landing, minus the lookahead
// term. It runs the placement itself so the caller gets the resulting grid
// back for free -- the lookahead below needs the exact board the shot would
// leave, and simulating the shot twice per candidate would double the cost.
// Split out of ChooseShot so the header can expose it to tests, which must
// bind to the real weights instead of restating them.
int LandingScore(Grid &grid, int row, int col, int colour) {
    int popped = 0, detached = 0, groupSize = 0;
    PlaceAndScore(grid, row, col, colour, &popped, &detached, &groupSize);
    // Weight the pieces separately: a pop is good, but so is parking next
    // to the bot's own colour when nothing pops, and a deep pop is worse
    // than building a cluster high up. Rows keep their width after
    // PlaceAndScore mutates the grid, so the edge test can still read the
    // last column here.
    const int lastColumnOfThatRow = static_cast<int>(grid[row].size()) - 1;
    int score = kPopped * popped + kDetached * detached;
    if (popped == 0 && detached == 0)
        score += kCluster * (groupSize - 1);
    score -= kRowPenalty * row;
    if (row == 0) score -= kCeilingPenalty;
    if (col == 0 || col == lastColumnOfThatRow) score += kEdgeBonus;
    if (row >= kDangerRow) score -= kDangerPenalty;
    return score;
}

// The best score achievable next turn with `nextColour` -- the bubble the
// game has already told the bot is coming, the same preview a person reads
// off the launcher -- played against the board a candidate shot would leave.
// Only cells bordering an existing bubble of that colour are worth trying:
// nothing else can complete a group of three, so nothing else can score.
int BestFollowUpScore(const Grid &grid, int nextColour) {
    if (nextColour < 0) return 0;
    int best = 0;
    for (size_t row = 0; row < grid.size(); ++row) {
        for (size_t col = 0; col < grid[row].size(); ++col) {
            if (grid[row][col] != -1) continue;
            bool reachable = false;
            for (auto n : Neighbours(grid, static_cast<int>(row), static_cast<int>(col))) {
                if (grid[n.first][n.second] == nextColour) { reachable = true; break; }
            }
            if (!reachable) continue;
            Grid copy = grid;
            const int score = PlaceAndScore(copy, static_cast<int>(row),
                                            static_cast<int>(col), nextColour);
            if (score > best) best = score;
        }
    }
    return best;
}

}  // namespace

bool PredictLanding(BubbleArray &board, float angle, int colour, bool isMini,
                    int *row, int *col) {
    const int bubbleSize = isMini ? 16 : 32;
    const float startX = static_cast<float>(board.curLaunchRct.x);
    const float startY = static_cast<float>(board.curLaunchRct.y);

    SingleBubble probe{board.playerAssigned, colour, startX, startY, startX, startY,
                       {static_cast<int>(startX), static_cast<int>(startY)}, {}, angle,
                       false, true, board.leftLimit, board.rightLimit, board.topLimit,
                       false, bubbleSize};
    probe.simulated = true;

    const int substeps = LaunchSubstepCount(bubbleSize, 1.0f);
    const float subscale = 1.0f / static_cast<float>(substeps);

    for (int step = 0; step < kMaxSubsteps; ++step) {
        probe.UpdatePosition(subscale);
        if (probe.pos.y <= board.topLimit) {
            GetClosestFreeCell(probe, board, row, col, -1, -1, isMini);
            return true;
        }
        for (int hitRow = 0; hitRow < static_cast<int>(board.bubbleMap.size()); ++hitRow) {
            for (int hitCol = 0; hitCol < static_cast<int>(board.bubbleMap[hitRow].size()); ++hitCol) {
                if (probe.IsCollision(&board.bubbleMap[hitRow][hitCol])) {
                    GetClosestFreeCell(probe, board, row, col, hitRow, hitCol, isMini);
                    return true;
                }
            }
        }
    }
    return false;
}

int ScoreLanding(const BubbleArray &board, int row, int col, int colour) {
    Grid grid = ColourGrid(board);
    return PlaceAndScore(grid, row, col, colour);
}

int ScoreShot(const BubbleArray &board, int row, int col, int colour) {
    Grid grid = ColourGrid(board);
    return LandingScore(grid, row, col, colour);
}

Shot ChooseShot(BubbleArray &board, int colour, bool isMini, Skill skill,
                unsigned *rng) {
    auto next_random = [rng]() {
        *rng = *rng * 1103515245u + 12345u;
        return (*rng >> 16) & 0x7fff;
    };

    // How much a candidate's lookahead score counts for. The base score used
    // to run in the thousands (`popped * 1000`), so the old weights were only
    // ever a near-tie breaker; the new weights run in the tens, so these drop
    // by the same factor (~80x) or the lookahead would dominate the shot.
    // They still cannot talk the bot into a worse shot for the sake of a
    // combo. Easy gets none: a bot that does not reliably see the best shot
    // in front of it has no business planning a second one ahead.
    const int lookaheadWeight =
        skill == Skill::Hard ? 6 : skill == Skill::Normal ? 2 : 0;

    std::vector<Shot> ranked;
    for (int i = 0; i < kCandidates; ++i) {
        const float angle = kMinAngle +
            (kMaxAngle - kMinAngle) * static_cast<float>(i) /
            static_cast<float>(kCandidates - 1);
        int row = -1, colIdx = -1;
        if (!PredictLanding(board, angle, colour, isMini, &row, &colIdx)) continue;

        Grid grid = ColourGrid(board);
        int score = LandingScore(grid, row, colIdx, colour);
        // Look one shot further using the bubble already queued behind this
        // one -- the same preview a person reads off the launcher, not
        // information the bot has that a player does not.
        if (lookaheadWeight > 0) {
            score += BestFollowUpScore(grid, board.nextBubble) * lookaheadWeight;
        }
        ranked.push_back({angle, score, row, colIdx, true});
    }
    if (ranked.empty()) return {};

    std::sort(ranked.begin(), ranked.end(),
              [](const Shot &a, const Shot &b) { return a.score > b.score; });

    // Collapse to one entry per landing cell. Several neighbouring angles
    // usually funnel into the same cell, so without this "the second best
    // shot" would just be a fractionally different angle onto the very same
    // outcome, and a weaker bot would play identically to a strong one.
    std::vector<Shot> distinct;
    std::set<std::pair<int, int>> seen;
    for (const Shot &shot : ranked) {
        if (seen.insert({shot.row, shot.col}).second) distinct.push_back(shot);
    }

    size_t pick = 0;
    switch (skill) {
        case Skill::Hard:
            pick = 0;
            break;
        case Skill::Normal:
            // Usually right, occasionally settles for the next best board.
            if (distinct.size() > 1 && next_random() % 4 == 0) pick = 1;
            break;
        case Skill::Easy:
            // Reported live as still too strong: half the time landing the
            // objectively best shot on the board, and the other half
            // picking uniformly from EVERY distinct landing (`distinct` is
            // sorted best-to-worst end to end) still had good odds of
            // something perfectly playable, since most boards have several
            // decent options clustered near the top. A weak player mostly
            // does not find the good options at all, not "a random option
            // among all of them" -- so a miss now restricts the draw to the
            // worse half of the ranked list, and misses happen far more
            // often than they connect.
            if (distinct.size() > 1 && next_random() % 10 >= 2) {
                const size_t worseHalfStart = distinct.size() / 2;
                pick = worseHalfStart +
                       next_random() % (distinct.size() - worseHalfStart);
            }
            break;
    }
    return distinct[pick];
}

}  // namespace BubbleAI
