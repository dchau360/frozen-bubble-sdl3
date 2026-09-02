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

#include "frozenbubble.h"
#include "bubblegame.h"
#include "audiomixer.h"
#include "highscoremanager.h"
#include "transitionmanager.h"
#include "gamesettings.h"
#include "platform.h"

#include <fstream>
#include <sstream>
#include <set>
#include <queue>
#include <map>

#include <cmath>
#include <algorithm>
#include "bubblegame_internal.h"

void GetGroupedCount(BubbleArray &bArray, std::vector<Bubble*> *bubbleCount, int row, int col, int *curStack) {
    int targetId = (*bubbleCount)[0]->bubbleId;
    int oddswap = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;

    for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
        int nr = row + dr;
        int nc = col + dc;
        if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
        if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
        Bubble *nb = &bArray.bubbleMap[nr][nc];
        if (nb->bubbleId != targetId) continue;
        if (std::count(bubbleCount->begin(), bubbleCount->end(), nb) > 0) continue;

        bubbleCount->push_back(nb);
        (*curStack)++;
        GetGroupedCount(bArray, bubbleCount, nr, nc, curStack);
    }
}

static std::set<std::pair<int, int>> CollectColorGroup(
    const BubbleArray &bArray, int destRow, int destCol, int bubbleId, int oddswap) {
    std::set<std::pair<int, int>> group;
    std::queue<std::pair<int, int>> queue;

    for (auto [dr, dc] : GridNeighborOffsets(destRow, oddswap)) {
        int row = destRow + dr;
        int col = destCol + dc;
        if (row < 0 || row >= (int)bArray.bubbleMap.size()) continue;
        if (col < 0 || col >= (int)bArray.bubbleMap[row].size()) continue;
        if (bArray.bubbleMap[row][col].bubbleId != bubbleId) continue;
        if (group.insert({row, col}).second) queue.push({row, col});
    }

    while (!queue.empty()) {
        auto [row, col] = queue.front();
        queue.pop();

        for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
            int nextRow = row + dr;
            int nextCol = col + dc;
            if (nextRow < 0 || nextRow >= (int)bArray.bubbleMap.size()) continue;
            if (nextCol < 0 || nextCol >= (int)bArray.bubbleMap[nextRow].size()) continue;
            if (bArray.bubbleMap[nextRow][nextCol].bubbleId != bubbleId) continue;
            if (group.insert({nextRow, nextCol}).second)
                queue.push({nextRow, nextCol});
        }
    }

    return group;
}

static std::set<std::pair<int, int>> CollectRootReachable(
    const BubbleArray &bArray,
    const std::set<std::pair<int, int>> &excluded,
    int oddswap) {
    std::set<std::pair<int, int>> reachable;
    std::queue<std::pair<int, int>> queue;

    for (int col = 0; col < (int)bArray.bubbleMap[0].size(); ++col) {
        std::pair<int, int> position{0, col};
        if (bArray.bubbleMap[0][col].bubbleId == -1 || excluded.count(position) > 0)
            continue;
        reachable.insert(position);
        queue.push(position);
    }

    while (!queue.empty()) {
        auto [row, col] = queue.front();
        queue.pop();

        for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
            int nextRow = row + dr;
            int nextCol = col + dc;
            std::pair<int, int> next{nextRow, nextCol};
            if (nextRow < 0 || nextRow >= (int)bArray.bubbleMap.size()) continue;
            if (nextCol < 0 || nextCol >= (int)bArray.bubbleMap[nextRow].size()) continue;
            if (bArray.bubbleMap[nextRow][nextCol].bubbleId == -1) continue;
            if (excluded.count(next) > 0) continue;
            if (reachable.insert(next).second) queue.push(next);
        }
    }

    return reachable;
}

static void ValidateAssignedChains(BubbleArray &bArray, int oddswap) {
    std::vector<SingleBubble*> assignedChains;
    for (SingleBubble &chain : singleBubbles) {
        if (chain.falling && chain.chainExists &&
            chain.assignedArray == bArray.playerAssigned &&
            chain.chainRow != -1 && chain.chainCol != -1) {
            assignedChains.push_back(&chain);
        }
    }

    std::stable_sort(
        assignedChains.begin(), assignedChains.end(),
        [](const SingleBubble *left, const SingleBubble *right) {
            return left->pos.y > right->pos.y;
        });

    for (SingleBubble *chain : assignedChains) {
        if (!chain->chainExists) continue;

        std::set<std::pair<int, int>> otherGroups;
        for (const SingleBubble *other : assignedChains) {
            if (other == chain || !other->chainExists) continue;
            const auto group = CollectColorGroup(
                bArray, other->chainRow, other->chainCol, other->bubbleId, oddswap);
            otherGroups.insert(group.begin(), group.end());
        }

        const auto targetGroup = CollectColorGroup(
            bArray, chain->chainRow, chain->chainCol, chain->bubbleId, oddswap);
        const auto rootReachable = CollectRootReachable(bArray, otherGroups, oddswap);
        int reachableTargetCount = 0;
        for (const auto &position : targetGroup) {
            if (rootReachable.count(position) > 0) ++reachableTargetCount;
        }

        if (reachableTargetCount < 2) {
            chain->chainExists = false;
            chain->chainRow = -1;
            chain->chainCol = -1;
            chain->chainDest = {};
        }
    }
}

// The screen Y a chain-reaction bubble must fall past before it swings back up
// toward its target. The original picks this per player rather than globally
// (bin/frozen-bubble line 2525): 380 for the centre board and for both boards
// of a two-player game, 185 for the two top mini boards, 415 for the two
// bottom ones. Hard-coding 380 everywhere sent a top mini board's chain bubble
// roughly two hundred pixels below its own board -- down across the centre
// board and into the bottom slots -- before it turned around, and it hung
// visibly at the apex of the swing back up.
static int ChainArcThreshold(const BubbleArray &bArray, int playerCount) {
    if (playerCount < 3 || bArray.playerAssigned == 0) return 380;
    // Arrays 1..4 fill the mini slots in order for games up to five players;
    // above that the board is parked in one of the same four slots instead
    // (see the royale branch of the name-plate switch in bubblegame_render).
    const int slot = bArray.parkedSlot >= 0 ? bArray.parkedSlot
                                            : bArray.playerAssigned - 1;
    return slot <= 1 ? 185 : 415;
}

void BubbleGame::AssignChainReactions(BubbleArray &bArray) {
    // Assign chain reaction targets to falling bubbles ONCE when they're created
    // Original: frozen-bubble line 814-865 in stick_bubble function
    // This creates the cascading chain reaction effect when chain bubbles land and trigger more groups

    SDL_Log("AssignChainReactions: Checking %zu falling bubbles for chain targets", singleBubbles.size());
    const int oddswap = bArray.bubbleMap[0].size() == 8 ? 0 : 1;

    // Track positions already reserved by chain reactions to prevent conflicts
    std::set<std::pair<int, int>> occupiedPositions;

    // Track bubbles that are part of groups targeted by earlier chain reactions
    // Original line 829: prevents chaining to bubbles in chained_bubbles groups
    std::set<std::pair<int, int>> chainedGroupBubbles;

    // Mark all occupied positions in the grid
    for (size_t row = 0; row < bArray.bubbleMap.size(); row++) {
        for (size_t col = 0; col < bArray.bubbleMap[row].size(); col++) {
            if (bArray.bubbleMap[row][col].bubbleId != -1) {
                occupiedPositions.insert({row, col});
            }
        }
    }

    // Also mark positions already reserved by other chain reactions
    for (const auto &sb : singleBubbles) {
        if (sb.chainExists && sb.chainRow != -1 && sb.chainCol != -1) {
            occupiedPositions.insert({sb.chainRow, sb.chainCol});
        }
    }

    // Calculate distance_to_root for all bubbles (Original: frozen-bubble lines 801-810)
    // This is used to prioritize chain reactions for groups closer to the root
    std::map<std::pair<int, int>, int> distanceToRoot;

    // Initialize all bubbles to distance 0
    for (size_t row = 0; row < bArray.bubbleMap.size(); row++) {
        for (size_t col = 0; col < bArray.bubbleMap[row].size(); col++) {
            if (bArray.bubbleMap[row][col].bubbleId != -1) {
                distanceToRoot[{row, col}] = 0;
            }
        }
    }

    // BFS from root bubbles (top row, row 0) to calculate distances
    std::queue<std::pair<int, int>> queue;
    std::set<std::pair<int, int>> visited;

    // Start with bubbles in top row (root_bubbles in original)
    for (size_t col = 0; col < bArray.bubbleMap[0].size(); col++) {
        if (bArray.bubbleMap[0][col].bubbleId != -1) {
            queue.push({0, col});
            visited.insert({0, col});
            distanceToRoot[{0, col}] = 1;  // Root bubbles have distance 1
        }
    }

    // BFS to assign distances
    while (!queue.empty()) {
        auto [row, col] = queue.front();
        queue.pop();
        int currentDistance = distanceToRoot[{row, col}];

        for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
            int nr = row + dr;
            int nc = col + dc;
            if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
            if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
            if (visited.count({nr, nc}) > 0) continue;
            if (bArray.bubbleMap[nr][nc].bubbleId == -1) continue;

            visited.insert({nr, nc});
            distanceToRoot[{nr, nc}] = currentDistance + 1;
            queue.push({nr, nc});
        }
    }

    // Collect all potential chain targets (grouped bubbles) with their distances
    // Original line 818-821: finds grouped_bubbles
    struct ChainTarget {
        int row, col;
        int bubbleId;
        int distance;
    };
    std::vector<ChainTarget> potentialTargets;

    for (size_t row = 0; row < bArray.bubbleMap.size(); row++) {
        for (size_t col = 0; col < bArray.bubbleMap[row].size(); col++) {
            Bubble &gridBubble = bArray.bubbleMap[row][col];
            if (gridBubble.bubbleId == -1) continue;

            // Check if this bubble has a neighbor of the same color (part of a group)
            bool hasNeighborSameColor = false;
            for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
                int nr = row + dr;
                int nc = col + dc;
                if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                if (bArray.bubbleMap[nr][nc].bubbleId == gridBubble.bubbleId) {
                    hasNeighborSameColor = true;
                    break;
                }
            }

            if (hasNeighborSameColor) {
                int dist = distanceToRoot.count({row, col}) > 0 ? distanceToRoot[{row, col}] : 999;
                potentialTargets.push_back({(int)row, (int)col, gridBubble.bubbleId, dist});
            }
        }
    }

    // Sort by distance_to_root (Original line 828)
    // Groups closer to root are processed first
    std::sort(potentialTargets.begin(), potentialTargets.end(),
              [](const ChainTarget &a, const ChainTarget &b) {
                  return a.distance < b.distance;
              });

    SDL_Log("AssignChainReactions: Found %zu potential targets, sorted by distance", potentialTargets.size());

    // Process potential targets in distance order (Original line 828: sort by distance_to_root)
    // This ensures groups closer to the root get priority for chain reactions
    for (const auto &target : potentialTargets) {
        int row = target.row;
        int col = target.col;
        int bubbleId = target.bubbleId;

        // Skip if this bubble is part of a group already targeted by another chain reaction
        // Original line 832-833: prevents chaining to same group twice
        if (chainedGroupBubbles.count({row, col}) > 0) {
            continue;
        }

        SDL_Log("  Examining target at [%d][%d] color=%d distance=%d", row, col, bubbleId, target.distance);

        // Find free adjacent positions for this target
        // Original line 830: next_positions($pos, $player)
        // Find first free position adjacent to this target
        int freeRow = -1, freeCol = -1;
        for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
            int nr = row + dr;
            int nc = col + dc;
            if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
            if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;

            // Original line 834: check if position is free
            if (bArray.bubbleMap[nr][nc].bubbleId == -1 &&
                occupiedPositions.count({nr, nc}) == 0) {
                freeRow = nr;
                freeCol = nc;
                break;  // Use first free position found
            }
        }

        // If no free position, skip this target
        if (freeRow == -1) {
            continue;
        }

        // Find a matching falling bubble that hasn't been assigned yet
        // Original line 836-861: foreach my $falling (@falling)
        for (auto &sBubble : singleBubbles) {
            // Skip if: not falling, already chained, wrong player, wrong color, launching, or exploding
            // CRITICAL: Must ONLY process bubbles that are genuinely falling from disconnection
            if (!sBubble.falling || sBubble.chainExists ||
                sBubble.assignedArray != bArray.playerAssigned ||
                sBubble.bubbleId != bubbleId ||
                sBubble.launching || sBubble.exploding || sBubble.shouldClear) {
                continue;
            }

            // Found a match! Assign chain reaction target
            // Original line 839-842: assigns chaindestx, chaindesty
            SDL_Log("    Chain target found! Bubble color=%d will rise to [%d][%d]",
                    sBubble.bubbleId, freeRow, freeCol);

            sBubble.chainExists = true;
            sBubble.chainRow = freeRow;
            sBubble.chainCol = freeCol;
            // Compute pixel position from grid coords using actual row offset
            // (matches how bubble pos.x is set in LoadLevel/RandomLevel). Mini
            // players use half bubble size, matching RandomLevel/GetClosestFreeCell.
            bool isMini = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
            int chainBubbleSize = isMini ? 16 : 32;
            int chainRowSize = chainBubbleSize * 7 / 8;
            int chainRowOffset = (bArray.bubbleMap[freeRow].size() == 7) ? chainBubbleSize / 2 : 0;
            sBubble.chainDest = {
                bArray.bubbleOffset.x + freeCol * chainBubbleSize + chainRowOffset,
                bArray.bubbleOffset.y + freeRow * chainRowSize
            };
            sBubble.chainArcThreshold =
                ChainArcThreshold(bArray, currentSettings.playerCount);
            occupiedPositions.insert({freeRow, freeCol}); // Reserve this position

            // Mark all bubbles in this group as "chained" to prevent other chains from targeting them
            // Original line 846-858: calculates chained_bubbles group
            // When this chain lands and explodes the group, these bubbles won't exist anymore
            const auto group = CollectColorGroup(
                bArray, freeRow, freeCol, sBubble.bubbleId, oddswap);
            chainedGroupBubbles.insert(group.begin(), group.end());

            SDL_Log("    Marked %zu bubbles in chain target group as unavailable", chainedGroupBubbles.size());

            // Found a chain target, move to next target position
            // Original line 859: last; (exits inner foreach loop)
            break;
        }
    }

    ValidateAssignedChains(bArray, oddswap);
}

void BubbleGame::CheckPossibleDestroy(BubbleArray &bArray){
    int totalDestroyed = 0;  // Track destroyed bubbles for malus calculation

    for (size_t i = 0; i < bArray.bubbleMap.size(); i++) {
        for (size_t j = 0; j < bArray.bubbleMap[i].size(); j++) {
            if (bArray.bubbleMap[i][j].playerBubble == true) { // activator
                std::vector<Bubble*> bubbleCount;
                int groupedCount = 0;
                bubbleCount.push_back(&bArray.bubbleMap[i][j]);
                GetGroupedCount(bArray, &bubbleCount, i, j, &groupedCount);
                if (groupedCount >= 2) {
                    SDL_Log("Match found: %d bubbles (chainReaction=%d)", groupedCount + 1, currentSettings.chainReaction);
                    PlaySFX("destroy_group");

                    // Calculate score: 10 points per bubble (groupedCount+1 = total including activator), with chain multiplier
                    int baseScore = (groupedCount + 1) * 10;
                    int multiplier = 1 + bArray.chainLevel;
                    bArray.score += baseScore * multiplier;
                    bArray.chainLevel++; // Increment chain level for subsequent groups

                    // Show combo text if chain level > 0
                    if (bArray.chainLevel > 1) {
                        comboDisplayTimer = 60; // Display for 60 frames (1 second at 60fps)
                        char comboStr[32];
                        snprintf(comboStr, sizeof(comboStr), "COMBO x%d!", bArray.chainLevel);
                        comboText.UpdateText(renderer, comboStr, 0);
                        comboText.UpdatePosition({SCREEN_CENTER_X - (comboText.Coords()->w / 2), 200});
                    }

                    totalDestroyed += groupedCount;  // Excludes activator, matching original @will_destroy

                    // Stats: count popped bubbles (group incl. activator) for locally-owned arrays.
                    if (OwnsArray(bArray))
                        bArray.rPopped += groupedCount + 1;

                    bool isMiniPop = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
                    for (Bubble *bubble : bubbleCount) {
                        float startX = (float)bubble->pos.x;
                        float startY = (float)bubble->pos.y;
                        SingleBubble bubs = {bArray.playerAssigned, bArray.curLaunch, startX, startY, startX, startY, bubble->pos, {}, bArray.shooterSprite.angle, false, false, bArray.leftLimit, bArray.rightLimit, bArray.topLimit, lowGfx, isMiniPop ? 16 : 32};
                        bubs.CopyBubbleProperties(bubble);
                        bubs.GenerateFreeFall(true);
                        singleBubbles.push_back(bubs);
                        bubble->bubbleId = -1;
                        bubble->playerBubble = false;
                    }
                } else {
                    // No match — reset activator flag so it isn't re-triggered on future shots
                    bArray.bubbleMap[i][j].playerBubble = false;
                }
                bubbleCount.clear();
                continue;
            }
        }
    }

    // Original Perl: air bubble check and malus only run inside the match block (else of @will_destroy <= 1).
    // If no match occurred (totalDestroyed == 0), skip both — mirroring the original behavior.
    int fallingCount = 0;
    if (totalDestroyed > 0) {
        fallingCount = CheckAirBubbles(bArray);

        // Assign chain reaction targets to newly falling bubbles (original line 814-865)
        // This happens ONCE per stick event, not every frame
        // In network games, only run chain reactions for the LOCAL player (array 0).
        // Chain reactions run only on boards this client simulates. A remote
        // player runs their own and tells us the outcome; running them here
        // too would fight that rather than agree with it.
        bool shouldRunChainReactions = currentSettings.chainReaction && fallingCount > 0 &&
            OwnsArray(bArray);
        if (shouldRunChainReactions) {
            AssignChainReactions(bArray);
        }

        // Re-validate nextBubble after pops: if the queued color was cleared, repick.
        // Original Perl doesn't do this (same bug exists there); we improve on it.
        // Skip for network games — nextBubble is synced via 's' messages from the leader.
        if (!currentSettings.networkGame) {
            std::vector<int> remaining = bArray.remainingBubbles();
            if (!remaining.empty() &&
                std::find(remaining.begin(), remaining.end(), bArray.nextBubble) == remaining.end()) {
                bArray.nextBubble = remaining[ranrange(1, remaining.size()) - 1];
            }
        }
    }

    // Calculate malus: destroyed + falling - 2 (original formula at line 958)
    int malusValue = totalDestroyed + fallingCount - 2;

    // "Attack bubbles: with canceling" -- what you just earned pays down what
    // you still owe before any of it is sent on. Only bubbles still queued can
    // be cancelled: once ProcessMalusQueue has turned one into a falling
    // MalusBubble it is committed and on its way down, so the queue is exactly
    // the "not yet landed" set. The count is clamped by the queue's own size,
    // which is what keeps it from going negative -- cancelling more than you
    // owe empties the queue and sends nothing, it never banks credit.
    //
    // Applied to every board rather than only the ones this client owns, and
    // before the OwnsArray-gated send below: each client runs this same
    // function for each board off the same stick events, so cancelling here
    // keeps every client's view of a remote player's queue in step with the
    // owner's, instead of the owner quietly cancelling and everyone else
    // still drawing malus that is never going to land.
    {
        // Clamped again here, not because MalusCancelled is expected to be
        // wrong, but because this is an erase range: a count past the queue's
        // end is memory corruption rather than a wrong number, and that is too
        // sharp an edge to leave resting on one function's arithmetic.
        int cancelled = MalusCancelled(currentSettings.attackMode, malusValue,
                                       (int)bArray.malusQueue.size());
        cancelled = std::clamp(cancelled, 0, (int)bArray.malusQueue.size());
        if (cancelled > 0) {
            bArray.malusQueue.erase(bArray.malusQueue.begin(),
                                    bArray.malusQueue.begin() + cancelled);
            malusValue -= cancelled;
            // Stat: malus blocked. Counted here on every client's copy of every
            // board, same as the erase above and for the same reason -- but
            // only the owning client's rBlk is ever broadcast (FinalizeRoundStats)
            // or shown as this player's own row, so a non-owner's local count for
            // someone else's board is discarded rather than double-counted; see
            // the 'S' handler in bubblegame_net.cpp, which overwrites it outright.
            bArray.rBlk += cancelled;
            AddMalusAlert(bArray, "", cancelled, /*blocked=*/true);
            SDL_Log("Malus canceling: blocked %d incoming, %d left to send",
                    cancelled, malusValue);
        }
    }

    if (malusValue > 0 && currentSettings.attackMode != AttackMode::Off) {
        if (currentSettings.mpTraining && bArray.playerAssigned == 0) {
            // mp_train: malus converted to score (original malus_change at line 1185)
            mpTrainScore += malusValue;
        } else if (currentSettings.networkGame && OwnsArray(bArray)) {
            // Every board we simulate attacks on its own behalf.
            SDL_Log("Awarding %d malus to opponent (%d destroyed + %d falling - 2)",
                    malusValue, totalDestroyed, fallingCount);
            SendMalusToOpponent(malusValue, bArray);
        } else if (!currentSettings.networkGame && currentSettings.playerCount >= 2) {
            // Local multiplayer: send the full malus to every other living player's queue.
            // Original (line 1191-1216, !is_mp_game() branch) sends the undivided count to
            // each opponent — division by (living - 1) only applies in the true network-game
            // branch, so local multiplayer must not divide either.
            int attackerIdx = bArray.playerAssigned;
            int attackerTeam = currentSettings.playerTeams[attackerIdx];
            for (int i = 0; i < currentSettings.playerCount; i++) {
                if (i != attackerIdx && bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE &&
                    (!currentSettings.teamMode || currentSettings.playerTeams[i] != attackerTeam)) {
                    for (int m = 0; m < malusValue; m++)
                        bubbleArrays[i].malusQueue.push_back(frameCount);
                    bubbleArrays[i].rRecv += malusValue;  // Stats: malus received
                    bArray.rSent += malusValue;           // Stats: malus sent by attacker
                    bubbleArrays[i].lastAttackerIdx = attackerIdx;  // Kill attribution
                    AddMalusAlert(bubbleArrays[i],
                                  bArray.playerNickname.empty()
                                      ? ("Player " + std::to_string(attackerIdx + 1))
                                      : bArray.playerNickname,
                                  malusValue);
                    SDL_Log("Local malus: %d bubbles queued for player %d", malusValue, i);
                }
            }
        }
    }
}

void DoFalling(std::vector<SDL_Point> &map, std::vector<SingleBubble> &bubbles, bool &lowGfx) {
    if (map.size() < 1 || bubbles.size() < 1) return;
    SDL_Log("DoFalling called: %d bubbles to fall, lowGfx=%d", (int)bubbles.size(), lowGfx);
    int maxy = map[map.size() - 1].y;
    int shiftSameLine = 0, line = maxy;
    for (size_t i = map.size(); i > 0; i--) { //original FB does backwards sorting for the formula
        int y = map[i - 1].y;
        shiftSameLine = line != y ? 0 : shiftSameLine;
        line = y;
        // Use falling=true (not explode) so AssignChainReactions can find these bubbles.
        // Only the directly-matched group in CheckPossibleDestroy gets exploding=true.
        // Original: disconnected bubbles go into @falling_bubble (falling), not @exploding_bubble.
        bubbles[i - 1].GenerateFreeFall(false, (maxy - y) * 5 + shiftSameLine);
        singleBubbles.push_back(bubbles[i - 1]);
        shiftSameLine++;
    }
    map.clear();
    bubbles.clear();
}

int BubbleGame::CheckAirBubbles(BubbleArray &bArray) {
    // Original algorithm: frozen-bubble lines 800-812
    // Use BFS from row 0 (ceiling) to mark all connected bubbles
    // Any bubble NOT marked is falling

    std::vector<SDL_Point> fallingLocs;
    std::vector<SingleBubble> singlesFalling;
    int fallingCount = 0;
    bool isMiniAir = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);

    // Mark all bubbles as unvisited (distance = 0 means not connected)
    std::set<std::pair<int, int>> connected;
    std::queue<std::pair<int, int>> queue;

    // Start with root bubbles (row 0) - these are always attached
    for (size_t col = 0; col < bArray.bubbleMap[0].size(); col++) {
        if (bArray.bubbleMap[0][col].bubbleId != -1) {
            queue.push({0, col});
            connected.insert({0, col});
        }
    }

    // BFS to mark all connected bubbles
    while (!queue.empty()) {
        auto [row, col] = queue.front();
        queue.pop();

        // Get neighbor offsets — use oddswap to handle both r=0 and r=1 grid orientations
        int oddswap = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;
        for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
            int nr = row + dr;
            int nc = col + dc;

            // Check bounds
            if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
            if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;

            // Skip if already visited or empty
            if (connected.count({nr, nc}) > 0) continue;
            if (bArray.bubbleMap[nr][nc].bubbleId == -1) continue;

            // Mark as connected and add to queue
            connected.insert({nr, nc});
            queue.push({nr, nc});
        }
    }

    // Now find all bubbles that are NOT connected (these should fall)
    for (size_t i = 1; i < bArray.bubbleMap.size(); i++) {  // Start from row 1 (row 0 always attached)
        for (size_t j = 0; j < bArray.bubbleMap[i].size(); j++) {
            if (bArray.bubbleMap[i][j].bubbleId == -1) continue;

            // If not in connected set, it should fall
            if (connected.count({i, j}) == 0) {
                if (bArray.bubbleMap[i][j].playerBubble)
                    SDL_Log("AIR_BUBBLE: Newly placed playerBubble at row=%zu col=%zu removed (not connected to ceiling)", i, j);
                float startX = (float)bArray.bubbleMap[i][j].pos.x;
                float startY = (float)bArray.bubbleMap[i][j].pos.y;
                SingleBubble bubbly = {bArray.playerAssigned, bArray.curLaunch, startX, startY, startX, startY,
                                       bArray.bubbleMap[i][j].pos, {}, bArray.shooterSprite.angle, false, false,
                                       bArray.leftLimit, bArray.rightLimit, bArray.topLimit, lowGfx, isMiniAir ? 16 : 32};
                bubbly.CopyBubbleProperties(&bArray.bubbleMap[i][j]);
                singlesFalling.push_back(bubbly);
                fallingLocs.push_back({(int)j, (int)i});
                bArray.bubbleMap[i][j].bubbleId = -1;
                bArray.bubbleMap[i][j].playerBubble = false;
                fallingCount++;
            }
        }
    }

    // Award points for falling bubbles: 20 points per bubble, with chain multiplier
    if (fallingCount > 0) {
        int baseScore = fallingCount * 20;
        int multiplier = 1 + bArray.chainLevel;
        bArray.score += baseScore * multiplier;
    }

    if (singlesFalling.size() > 0) {
        SDL_Log("DoFalling: %d bubbles falling after match clear (chainReaction=%d)",
                (int)singlesFalling.size(), currentSettings.chainReaction);
    }
    DoFalling(fallingLocs, singlesFalling, lowGfx);

    return fallingCount;  // Return count for malus calculation
}

void BubbleGame::DoFrozenAnimation(BubbleArray &bArray, int &waitTime){
    if (waitTime <= 0) {
        waitTime = FROZEN_FRAMEWAIT;
        for (int i = (int)bArray.bubbleMap.size() - 1; i >= 0; i--) {
            for (int j = (int)bArray.bubbleMap[i].size() - 1; j >= 0; j--) {
                if (bArray.bubbleMap[i][j].frozen != true && bArray.bubbleMap[i][j].bubbleId != -1) {
                    bArray.bubbleMap[i][j].frozen = true;
                    return;
                }
                else continue;
            }
        }
        if (currentSettings.playerCount < 2) {
            PlaySFX("noh");
            gameLost = true;
        }
        else bArray.mpDone = true;
    }
    else waitTime--;
}

void BubbleGame::DoWinAnimation(BubbleArray &bArray, int &waitTime){
    if (waitTime <= 0) {
        waitTime = EXPLODE_FRAMEWAIT;
        bool isMiniWin = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
        for (int i = (int)bArray.bubbleMap.size() - 1; i >= 0; i--) {
            for (int j = (int)bArray.bubbleMap[i].size() - 1; j >= 0; j--) {
                if (bArray.bubbleMap[i][j].bubbleId != -1) {
                    float startX = (float)bArray.bubbleMap[i][j].pos.x;
                    float startY = (float)bArray.bubbleMap[i][j].pos.y;
                    SingleBubble bubbly = {bArray.playerAssigned, bArray.curLaunch, startX, startY, startX, startY, bArray.bubbleMap[i][j].pos, {}, bArray.shooterSprite.angle, false, false, bArray.leftLimit, bArray.rightLimit, bArray.topLimit, lowGfx, isMiniWin ? 16 : 32};
                    bubbly.CopyBubbleProperties(&bArray.bubbleMap[i][j]);
                    bubbly.GenerateFreeFall(true, 0);
                    singleBubbles.push_back(bubbly);
                    bArray.bubbleMap[i][j].bubbleId = -1;
                    bArray.bubbleMap[i][j].playerBubble = false;
                    return;
                }
                else continue;
            }
        }
        bArray.mpDone = true;
    }
    else waitTime--;
}

void BubbleGame::DoPrelightAnimation(BubbleArray &bArray, int &waitTime){
    if (waitTime <= 0) {
        if(bArray.framePrelight <= 0) {
            for (size_t i = 0; i < bArray.bubbleMap.size(); i++) {
                if (lowGfx) continue;
                for (size_t j = 0; j < bArray.bubbleMap[i].size(); j++) {
                    if (j == (size_t)bArray.alertColumn) {
                        bArray.bubbleMap[i][j].shining = true;
                    }
                    else {
                        bArray.bubbleMap[i][j].shining = false;
                        continue;
                    }
                }
            }
            bArray.alertColumn++;
            if (bArray.alertColumn > 8) {
                waitTime = bArray.waitPrelight;
                bArray.alertColumn = 0;
            }
            bArray.framePrelight = PRELIGHT_FRAMEWAIT;
        }
        else bArray.framePrelight--;
    }
    else waitTime--;
}

void ResetPrelight(BubbleArray &bArray) {
    for (size_t i = 0; i < bArray.bubbleMap.size(); i++) {
        for (size_t j = 0; j < bArray.bubbleMap[i].size(); j++) {
            bArray.bubbleMap[i][j].shining = false;
        }
    }
}

void BubbleGame::ExpandNewLane(BubbleArray &bArray) {
    int newSize = bArray.bubbleMap[0].size() == 7 ? 8 : 7;

    // Mini players use half bubble size
    bool isMini = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
    int bubbleSize = isMini ? 16 : 32;
    int rowSize = bubbleSize * 7 / 8;  // 14 for mini, 28 for full
    int shiftY = rowSize;  // Shift amount equals ROW_SIZE

    for (std::size_t i = bArray.bubbleMap.size() - 1; i > 0; --i) {
        bArray.bubbleMap[i] = bArray.bubbleMap[i - 1];
        for (Bubble &bubble : bArray.bubbleMap[i]) {
            bubble.pos.y += shiftY;
        }
    }
    bArray.bubbleMap[0].clear();

    SDL_Point &offset = bArray.bubbleOffset;
    int smallerSep = newSize % 2 == 0 ? 0 : bubbleSize / 2;
    for (int j = 0; j < newSize; j++)
    {
        // Use nextColors queue for Perl-compatible color selection (original: ExpandNewLane uses nextcolors, line 951)
        int colorId;
        if (!bArray.nextColors.empty()) {
            colorId = bArray.nextColors.front();
            bArray.nextColors.erase(bArray.nextColors.begin());
            bArray.nextColors.push_back(ranrange(0, bArray.numColors - 1));  // Replenish queue
        } else {
            colorId = ranrange(0, bArray.numColors - 1);
        }
        bArray.bubbleMap[0].push_back(Bubble{colorId, {(smallerSep + bubbleSize * ((int)j)) + offset.x, offset.y}});
    }
}
