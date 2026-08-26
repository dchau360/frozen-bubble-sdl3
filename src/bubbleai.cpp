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
// CheckAirBubbles applies after a pop.
int CountDetached(const Grid &grid) {
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
    for (size_t row = 1; row < grid.size(); ++row)
        for (size_t col = 0; col < grid[row].size(); ++col)
            if (grid[row][col] != -1 &&
                !connected.count({static_cast<int>(row), static_cast<int>(col)}))
                ++detached;
    return detached;
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
    if (!InBounds(grid, row, col) || grid[row][col] != -1) return 0;
    grid[row][col] = colour;

    const auto group = SameColourGroup(grid, row, col);
    if (group.size() < 3) return 0;
    for (auto [r, c] : group) grid[r][c] = -1;
    return static_cast<int>(group.size()) + CountDetached(grid);
}

Shot ChooseShot(BubbleArray &board, int colour, bool isMini, Skill skill,
                unsigned *rng) {
    auto next_random = [rng]() {
        *rng = *rng * 1103515245u + 12345u;
        return (*rng >> 16) & 0x7fff;
    };

    std::vector<Shot> ranked;
    for (int i = 0; i < kCandidates; ++i) {
        const float angle = kMinAngle +
            (kMaxAngle - kMinAngle) * static_cast<float>(i) /
            static_cast<float>(kCandidates - 1);
        int row = -1, colIdx = -1;
        if (!PredictLanding(board, angle, colour, isMini, &row, &colIdx)) continue;

        const int popped = ScoreLanding(board, row, colIdx, colour);
        // Popping dominates. Failing that, land as high as possible: depth is
        // what ends the round, so a bubble parked near the ceiling costs less
        // than one parked at the bottom.
        const int score = popped * 1000 - row * 10;
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
            // Half the time it simply does not spot the match, which is what
            // a weak player actually looks like -- not a wobbly aim.
            if (distinct.size() > 1 && next_random() % 2 == 0)
                pick = next_random() % distinct.size();
            break;
    }
    return distinct[pick];
}

}  // namespace BubbleAI
