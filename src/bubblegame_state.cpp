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

void BubbleGame::SendMalusToOpponent(int malusCount) {
    if (!currentSettings.networkGame) return;

    // >5-player royale mini-board blink: 2 full cycles of the render's 20-frame
    // blink period (bubblegame_render.cpp) at 60fps.
    const int kAttackFlashDurationFrames = 40;

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient || !netClient->IsConnected() || netClient->GetState() != IN_GAME) {
        return;
    }

    // Original logic at frozen-bubble line 1204-1227
    // Two modes:
    // 1. Split malus to ALL living opponents (default)
    // 2. Send ALL malus to ONE specific target (single player targetting mode)

    // Count living opponents (exclude local player at array 0, and teammates in team mode)
    std::vector<int> livingOpponents;
    int localTeam = currentSettings.playerTeams[0];
    for (int i = 1; i < currentSettings.playerCount; i++) {
        if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) {
            if (!currentSettings.teamMode || currentSettings.playerTeams[i] != localTeam)
                livingOpponents.push_back(i);
        }
    }

    if (livingOpponents.empty()) {
        SDL_Log("No living opponents to send malus to");
        return;
    }

    // Battle royale (>5 players alive): malus always goes to ONE opponent (Tetris-99
    // model). Splitting is disabled -- ceil(count/N) to every living opponent would
    // inject up to 19x the malus. Manual target if selected, else random. This is
    // independent of the singlePlayerTargetting toggle (a host setting that only
    // controls the <=5-alive split-vs-single UI/behavior below) since a >5-alive round
    // must never reach the N-way split loop regardless of that flag. Re-evaluated live
    // every call (not a static room-size check) so the game correctly falls back to
    // split/manual-target rules once the field thins to <=5 survivors.
    {
        int livingPlayers = 0;
        for (int i = 0; i < currentSettings.playerCount; i++) {
            if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) livingPlayers++;
        }
        if (currentSettings.networkGame && livingPlayers > 5) {
            bool randomPick = (sendMalusToOne == -1);
            if (!randomPick &&
                !(sendMalusToOne >= 1 && sendMalusToOne < currentSettings.playerCount &&
                  bubbleArrays[sendMalusToOne].playerState == BubbleArray::PlayerState::ALIVE)) {
                // Manual target is stale (died or otherwise invalid) -- re-roll like the random case.
                randomPick = true;
            }
            int target = randomPick ? livingOpponents[rand() % livingOpponents.size()]
                                     : sendMalusToOne;

            std::string targetNick = bubbleArrays[target].playerNickname;
            int resolvedTarget = target;

            // Fallback to lobbyPlayerId if nickname is empty
            if (targetNick.empty()) {
                int lobbyId = bubbleArrays[target].lobbyPlayerId;
                if (lobbyId >= 0 && netClient) {
                    targetNick = netClient->GetPlayerNickname(lobbyId);
                    SDL_Log("Using fallback nickname '%s' from lobbyPlayerId %d for target array %d",
                           targetNick.c_str(), lobbyId, target);
                }
            }

            // Final fallback: generate a nickname
            if (targetNick.empty()) {
                int lobbyId = bubbleArrays[target].lobbyPlayerId;
                char fallbackNick[32];
                if (lobbyId >= 0) {
                    snprintf(fallbackNick, sizeof(fallbackNick), "player%d", lobbyId);
                } else {
                    snprintf(fallbackNick, sizeof(fallbackNick), "player%d", target);
                }
                targetNick = fallbackNick;
            }

            // If even the chosen target somehow resolved to an empty nick (shouldn't
            // happen given the fallback cascade above), try the other living opponents.
            if (targetNick.empty()) {
                for (int opp : livingOpponents) {
                    if (opp == target) continue;
                    std::string nick = bubbleArrays[opp].playerNickname;
                    if (nick.empty() && bubbleArrays[opp].lobbyPlayerId >= 0) {
                        nick = netClient->GetPlayerNickname(bubbleArrays[opp].lobbyPlayerId);
                    }
                    if (!nick.empty()) { targetNick = nick; resolvedTarget = opp; break; }
                }
            }

            if (!targetNick.empty()) {
                char malusMsg[128];
                snprintf(malusMsg, sizeof(malusMsg), "g%s:%d", targetNick.c_str(), malusCount);
                netClient->SendGameData(malusMsg);
                bubbleArrays[0].rSent += malusCount;  // Stats: malus sent (focus-fire)
                bubbleArrays[resolvedTarget].rRecv += malusCount;  // Stats: locally credit target's Def
                bubbleArrays[resolvedTarget].attackFlashFramesLeft = kAttackFlashDurationFrames;
                // Kill attribution: the sender never sees its own 'g' echoed back, so
                // credit this locally too, same as the Def stat above.
                bubbleArrays[resolvedTarget].lastAttackerIdx = 0;
            }
            if (randomPick) sendMalusToOne = -1;  // re-roll on the next attack
            return;
        }
    }

    // Single player targeting mode: send all malus to ONE opponent only when the player has
    // actively selected a target (original lines 1217-1227). With no target selected
    // (sendMalusToOne == -1), fall through to splitting among all living opponents — matching
    // the original (line 1204: "if (!sendmalustoone) { split }"). In team mode we always split.
    if (currentSettings.singlePlayerTargetting && !currentSettings.teamMode && sendMalusToOne != -1) {
        if (sendMalusToOne < currentSettings.playerCount &&
            bubbleArrays[sendMalusToOne].playerState == BubbleArray::PlayerState::ALIVE) {
            std::string targetNick = bubbleArrays[sendMalusToOne].playerNickname;

            // Fallback to lobbyPlayerId if nickname is empty
            if (targetNick.empty()) {
                int lobbyId = bubbleArrays[sendMalusToOne].lobbyPlayerId;
                if (lobbyId >= 0 && netClient) {
                    targetNick = netClient->GetPlayerNickname(lobbyId);
                    SDL_Log("Using fallback nickname '%s' from lobbyPlayerId %d for target array %d",
                           targetNick.c_str(), lobbyId, sendMalusToOne);
                }
            }

            // Final fallback: generate a nickname
            if (targetNick.empty()) {
                int lobbyId = bubbleArrays[sendMalusToOne].lobbyPlayerId;
                char fallbackNick[32];
                if (lobbyId >= 0) {
                    snprintf(fallbackNick, sizeof(fallbackNick), "player%d", lobbyId);
                } else {
                    snprintf(fallbackNick, sizeof(fallbackNick), "player%d", sendMalusToOne);
                }
                targetNick = fallbackNick;
            }

            if (!targetNick.empty()) {
                char malusMsg[128];
                snprintf(malusMsg, sizeof(malusMsg), "g%s:%d", targetNick.c_str(), malusCount);
                SDL_Log("Targeting: Sending all %d malus to %s (array %d)",
                        malusCount, targetNick.c_str(), sendMalusToOne);
                netClient->SendGameData(malusMsg);
                bubbleArrays[0].rSent += malusCount;  // Stats: malus sent (focus-fire)
                bubbleArrays[sendMalusToOne].attackFlashFramesLeft = kAttackFlashDurationFrames;
                bubbleArrays[sendMalusToOne].lastAttackerIdx = 0;
                return;
            }
        }
        // Target died/invalid - fall through to split mode
        sendMalusToOne = -1;
    }

    // Divide malus equally among living opponents (original line 1207)
    // Use ceiling division: int($numb/(@living-1) + 0.99)
    int malusPerOpponent = (malusCount + livingOpponents.size() - 1) / livingOpponents.size();

    SDL_Log("Sending malus: %d total split to %zu opponents = %d per opponent",
            malusCount, livingOpponents.size(), malusPerOpponent);

    // Send 'g' message to each living opponent (original line 1208-1215)
    // Format: g{opponentNick}:{count}
    // Use lobbyPlayerId for more reliable targeting when nicknames might be empty/duplicate
    for (int opponentIdx : livingOpponents) {
        std::string targetNick = bubbleArrays[opponentIdx].playerNickname;

        // Fallback to lobbyPlayerId if nickname is empty
        if (targetNick.empty()) {
            int lobbyId = bubbleArrays[opponentIdx].lobbyPlayerId;
            if (lobbyId >= 0 && netClient) {
                targetNick = netClient->GetPlayerNickname(lobbyId);
                SDL_Log("Using fallback nickname '%s' from lobbyPlayerId %d for array %d",
                       targetNick.c_str(), lobbyId, opponentIdx);
            }
        }

        // Final fallback: use "player{lobbyId}" format
        if (targetNick.empty()) {
            int lobbyId = bubbleArrays[opponentIdx].lobbyPlayerId;
            if (lobbyId >= 0) {
                char fallbackNick[32];
                snprintf(fallbackNick, sizeof(fallbackNick), "player%d", lobbyId);
                targetNick = fallbackNick;
            } else {
                char fallbackNick[32];
                snprintf(fallbackNick, sizeof(fallbackNick), "player%d", opponentIdx);
                targetNick = fallbackNick;
            }
            SDL_Log("Using generated nickname '%s' for array %d (no lobbyPlayerId)",
                   targetNick.c_str(), opponentIdx);
        }

        char malusMsg[128];
        snprintf(malusMsg, sizeof(malusMsg), "g%s:%d", targetNick.c_str(), malusPerOpponent);
        SDL_Log("  -> Sending %d malus to %s (array %d, lobbyId=%d)",
                malusPerOpponent, targetNick.c_str(), opponentIdx,
                bubbleArrays[opponentIdx].lobbyPlayerId);
        netClient->SendGameData(malusMsg);
        bubbleArrays[0].rSent += malusPerOpponent;  // Stats: malus sent (split among opponents)
        // Split hits every living opponent at once, so all of their boards blink together.
        bubbleArrays[opponentIdx].attackFlashFramesLeft = kAttackFlashDurationFrames;
        bubbleArrays[opponentIdx].lastAttackerIdx = 0;
    }
}

// Set single player malus targeting (original: sub set_sendmalustoone at line 1330)
// opponentIdx: 1-4 = target that opponent's bubbleArrays slot, -1 = clear (split to all)
void BubbleGame::SetSendMalusToOne(int opponentIdx) {
    sendMalusToOne = opponentIdx;
    playerTargeting[0] = opponentIdx;

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient || !netClient->IsConnected()) return;

    if (opponentIdx == -1) {
        // Clear targeting - broadcast to all so they remove the "attacking me" indicator
        netClient->SendGameData("A");
        SDL_Log("Cleared malus target (sending to all)");
    } else if (opponentIdx < currentSettings.playerCount &&
               bubbleArrays[opponentIdx].playerState == BubbleArray::PlayerState::ALIVE) {
        const std::string& nick = bubbleArrays[opponentIdx].playerNickname;
        if (!nick.empty()) {
            char aMsg[128];
            snprintf(aMsg, sizeof(aMsg), "A%s", nick.c_str());
            netClient->SendGameData(aMsg);
            SDL_Log("Set malus target to %s (array %d)", nick.c_str(), opponentIdx);
        }
    }
    ReRankNetView();  // the target is rank 1, so auto mode pulls it on screen
}

void BubbleGame::ProcessMalusQueue(BubbleArray &bArray, int currentFrame) {
    if (!currentSettings.networkGame && !currentSettings.mpTraining && currentSettings.playerCount < 2) return;
    if (bArray.malusQueue.empty()) return;

    const int MALUS_FREEZE_FRAMES = 20;  // Wait 20 frames after receiving malus (original line 2219)
    const int MAX_MALUS_BUBBLES = 7;     // Max 7 malus bubbles falling at once

    // Count currently falling malus bubbles for this player
    int fallingMalusCount = 0;
    for (const auto &mb : malusBubbles) {
        if (mb.assignedArray == bArray.playerAssigned && !mb.shouldClear) {
            fallingMalusCount++;
        }
    }

    // Check if we can process malus from queue (original line 2227)
    if (bArray.malusQueue.empty() ||
        currentFrame <= bArray.malusQueue[0] + MALUS_FREEZE_FRAMES ||
        fallingMalusCount >= MAX_MALUS_BUBBLES) {
        return;
    }

    // Calculate top_of_cx: highest bubble in each column (original line 2221-2226)
    int top_of_cx[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (size_t row = 0; row < bArray.bubbleMap.size(); row++) {
        for (size_t col = 0; col < bArray.bubbleMap[row].size(); col++) {
            if (bArray.bubbleMap[row][col].bubbleId != -1) {
                if ((int)row > top_of_cx[col]) {
                    top_of_cx[col] = row;
                }
            }
        }
    }

    // Generate ALL malus bubbles at once (original while loop at line 2227)
    std::vector<MalusBubble> newMalusBubbles;
    while (!bArray.malusQueue.empty() &&
           currentFrame > bArray.malusQueue[0] + MALUS_FREEZE_FRAMES &&
           fallingMalusCount < MAX_MALUS_BUBBLES) {

        // Remove one malus from queue
        bArray.malusQueue.erase(bArray.malusQueue.begin());

        // Generate random bubble color (original: int(rand(@bubbles_images)) = 0..7)
        int bubbleId = ranrange(0, bArray.numColors - 1);

        // Choose column (original line 2231-2240): int(rand(7)) = 0..6
        int cx = ranrange(0, 6);

        // If column is full (stickY would exceed row 12), try adjacent columns
        // (original's noinstantdeath logic avoids placing on full columns)
        int attempts = 0;
        while (top_of_cx[cx] >= 12 && attempts < 7) {
            cx = (cx + 1) % 7;
            attempts++;
        }
        if (top_of_cx[cx] >= 12) {
            // All columns full - skip this malus
            fallingMalusCount--;
            continue;
        }

        // Calculate where bubble will stick (original line 2241-2243)
        int cy = 12;  // Always starts at row 12
        int stickY = top_of_cx[cx] + 1;  // Stick one row below highest in column

        // Update top_of_cx for next bubble in this column
        top_of_cx[cx] = stickY;

        // Calculate screen position (original calc_real_pos at line 2244)
        // Mini players use half bubble size
        bool isMini = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
        int bubbleSize = isMini ? 16 : 32;
        int rowSize = bubbleSize * 7 / 8;  // 14 for mini, 28 for full
        int smallerSep = (cy % 2 == 0) ? 0 : bubbleSize / 2;
        float startX = (smallerSep + bubbleSize * cx) + bArray.bubbleOffset.x;
        float startY = (rowSize * cy) + bArray.bubbleOffset.y;

        MalusBubble malus = {
            bArray.playerAssigned,
            bubbleId,
            cx, cy,
            stickY,
            startX, startY,
            {(int)startX, (int)startY},
            false,
            false
        };

        newMalusBubbles.push_back(malus);
        fallingMalusCount++;
    }

    if (newMalusBubbles.empty()) return;

    // Sort by cx (original line 2252)
    std::sort(newMalusBubbles.begin(), newMalusBubbles.end(),
              [](const MalusBubble &a, const MalusBubble &b) { return a.cx < b.cx; });

    // Shift Y positions with spacing (original line 2253-2254)
    int shifting = 0;
    for (auto &malus : newMalusBubbles) {
        shifting += 7;
        int randomShift = ranrange(0, 20);
        malus.posY += shifting + randomShift;
        malus.pos.y = (int)malus.posY;
    }

    // Add all to global list
    for (const auto &malus : newMalusBubbles) {
        malusBubbles.push_back(malus);
    }

    // Send ALL 'm' messages (original line 2255-2256)
    // Only local player (array 0) sends messages
    if (bArray.playerAssigned == 0) {
        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected()) {
            for (const auto &malus : newMalusBubbles) {
                char mMsg[64];
                snprintf(mMsg, sizeof(mMsg), "m%d:%d:%d:%d",
                         malus.bubbleId, malus.cx, malus.cy, malus.stickY);
                SDL_Log("Sending malus bubble: color=%d cx=%d cy=%d stickY=%d",
                        malus.bubbleId, malus.cx, malus.cy, malus.stickY);
                netClient->SendGameData(mMsg);
            }
        }
    }

    PlaySFX("malus");
}

void BubbleGame::SubmitScore(BubbleArray &bArray) {
    SDL_Log("Level %d completed with score: %d", curLevel, bArray.score);
    if (currentSettings.networkGame || currentSettings.playerCount > 1) return;  // Only track 1P levelset scores

    SDL_Log("SubmitScore: getting elapsed time");
    float elapsedSeconds = (SDL_GetTicks() - FrozenBubble::Instance()->startTime) / 1000.0f;
    SDL_Log("SubmitScore: elapsedSeconds=%.1f, getting hm instance", elapsedSeconds);
    HighscoreManager* hm = HighscoreManager::Instance();
    SDL_Log("SubmitScore: calling AppendToLevels, savedLevelGrid rows: %zu", savedLevelGrid.size());
    for (size_t i = 0; i < savedLevelGrid.size(); i++)
        SDL_Log("  row %zu: %zu cells", i, savedLevelGrid[i].size());

    // Store current level grid for highscore display
    hm->AppendToLevels(savedLevelGrid, curLevel);
    SDL_Log("SubmitScore: AppendToLevels done");

    // Check if this qualifies as a top-10 score and save it
    if (hm->CheckAndAddScore(curLevel, elapsedSeconds)) {
        pendingHighscore = true;
        SDL_Log("New high score! Level %d in %.1fs", curLevel, elapsedSeconds);
    }
    SDL_Log("SubmitScore: done");
}

// Count living players (original: sub living_players() at line 600)
// Original checks: !$pdata{$::p_}{left} && $pdata{$::p_}{state} eq 'ingame'
int BubbleGame::CountLivingPlayers() {
    int livingCount = 0;
    SDL_Log("CountLivingPlayers: Checking %d players", currentSettings.playerCount);
    for (int i = 0; i < currentSettings.playerCount; i++) {
        bool isAlive = (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE);
        SDL_Log("  Player %d: state=%d (0=ALIVE,1=LOST,2=LEFT), isAlive=%d, lobbyId=%d",
                i, (int)bubbleArrays[i].playerState, isAlive, bubbleArrays[i].lobbyPlayerId);
        if (isAlive) {
            livingCount++;
        }
    }
    SDL_Log("  Total living: %d", livingCount);
    return livingCount;
}

int BubbleGame::CountLivingTeams() {
    std::set<int> aliveTeams;
    for (int i = 0; i < currentSettings.playerCount; i++) {
        if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE)
            aliveTeams.insert(currentSettings.playerTeams[i]);
    }
    return (int)aliveTeams.size();
}

bool BubbleGame::HasDepartedPlayers() const {
    for (int i = 0; i < currentSettings.playerCount; ++i) {
        if (bubbleArrays[i].playerState == BubbleArray::PlayerState::LEFT)
            return true;
    }
    return false;
}

int BubbleGame::CountConnectedPlayers() const {
    int connected = 0;
    for (int i = 0; i < currentSettings.playerCount; ++i) {
        if (bubbleArrays[i].playerState != BubbleArray::PlayerState::LEFT)
            ++connected;
    }
    return connected;
}

int BubbleGame::CountConnectedTeams() const {
    std::set<int> connectedTeams;
    for (int i = 0; i < currentSettings.playerCount; ++i) {
        if (bubbleArrays[i].playerState != BubbleArray::PlayerState::LEFT)
            connectedTeams.insert(currentSettings.playerTeams[i]);
    }
    return static_cast<int>(connectedTeams.size());
}

void BubbleGame::UpdateDepartureMatchTermination() {
    const bool abandonedRound =
        !currentSettings.continueWhenPlayersLeave && HasDepartedPlayers();
    const bool insufficientOpponents = currentSettings.teamMode
        ? CountConnectedTeams() < 2
        : CountConnectedPlayers() < 2;
    if (abandonedRound || insufficientOpponents) {
        gameMatchOver = true;
        waitingForOpponentNewGame = false;
        opponentReadyForNewGame = false;
    }
}

void BubbleGame::HandlePlayerDeparture(int playerIdx) {
    if (playerIdx < 0 || playerIdx >= currentSettings.playerCount) return;

    BubbleArray& player = bubbleArrays[playerIdx];
    if (player.playerState == BubbleArray::PlayerState::LEFT) return;

    SDL_Log("Marking player array %d (lobbyId=%d) as LEFT (disconnected)",
            playerIdx, player.lobbyPlayerId);
    player.playerState = BubbleArray::PlayerState::LEFT;
    player.penguinSprite.PlayAnimation(11);

    if (sendMalusToOne == playerIdx) SetSendMalusToOne(-1);
    attackingMe.erase(std::remove(attackingMe.begin(), attackingMe.end(), playerIdx),
                      attackingMe.end());
    playerTargeting[playerIdx] = -1;

    if (connectedPlayerCount > 0) --connectedPlayerCount;
    SDL_Log("connectedPlayerCount now %d", connectedPlayerCount);
    ReRankNetView();
    ResolveRoundOutcome(-1, RoundWinCause::Departure, false);
}

void BubbleGame::ApplyPlayerLoss(BubbleArray& player) {
    if (player.playerState != BubbleArray::PlayerState::ALIVE) return;

    SDL_Log("ApplyPlayerLoss: player %d lost", player.playerAssigned);
    player.playerState = BubbleArray::PlayerState::LOST;
    player.penguinSprite.PlayAnimation(11);
    PlaySFX("lose");

    if (player.lastAttackerIdx >= 0 &&
        player.lastAttackerIdx < currentSettings.playerCount) {
        bubbleArrays[player.lastAttackerIdx].rKills++;
    }

    if (currentSettings.networkGame && currentSettings.playerCount > 5 &&
        sendMalusToOne == player.playerAssigned) {
        SetSendMalusToOne(-1);
    }
    ReRankNetView();
}

void BubbleGame::ResolveDangerZoneLosses() {
    bool changed = false;
    for (int i = 0; i < currentSettings.playerCount; ++i) {
        BubbleArray& player = bubbleArrays[i];
        if (!player.bubbleOnDanger() ||
            player.playerState != BubbleArray::PlayerState::ALIVE) {
            continue;
        }

        SDL_Log("Player %d hit danger zone", i);
        if (!currentSettings.networkGame || i == 0) {
            panelRct = {SCREEN_CENTER_X - 173, 480 - 248, 345, 124};
            player.curLaunchRct = {
                player.curLaunchRct.x - 1, player.curLaunchRct.y - 1, 34, 48};
        }
        ApplyPlayerLoss(player);
        changed = true;
    }

    if (changed) {
        ResolveRoundOutcome(
            -1, RoundWinCause::Elimination, currentSettings.networkGame);
    }
}

void BubbleGame::ResolveRoundOutcome(int assertedWinnerIdx,
                                     RoundWinCause cause,
                                     bool sendNetworkFinish) {
    if (gameFinish) {
        // A later clear observation may enrich the presentation metadata, but
        // no later observation may replace or re-credit a committed outcome.
        if (assertedWinnerIdx >= 0)
            CommitRoundWin(assertedWinnerIdx, cause, false);
        UpdateDepartureMatchTermination();
        return;
    }

    if (assertedWinnerIdx >= 0) {
        CommitRoundWin(assertedWinnerIdx, cause, sendNetworkFinish);
        return;
    }

    const int living = CountLivingPlayers();
    if (living == 0) {
        FinishRoundAsDraw();
        return;
    }

    const bool onePlayer = living == 1;
    const bool oneTeam = currentSettings.teamMode && CountLivingTeams() == 1;
    if (!onePlayer && !oneTeam) return;

    for (int i = 0; i < currentSettings.playerCount; ++i) {
        if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) {
            CommitRoundWin(i, cause, sendNetworkFinish);
            return;
        }
    }
}

void BubbleGame::CommitRoundWin(int winnerIdx,
                                RoundWinCause cause,
                                bool sendNetworkFinish) {
    if (gameFinish) {
        if (roundWinnerIdx == winnerIdx && cause == RoundWinCause::Clear)
            wonByClearing = true;
        return;
    }

    gameFinish = true;
    roundWinnerIdx = winnerIdx;
    wonByClearing = cause == RoundWinCause::Clear;
    panelRct = {SCREEN_CENTER_X - 173, 480 - 289, 329, 159};

    if (cause == RoundWinCause::Clear) {
        PlaySFX("lose");
        PlaySFX("applause");
    }

    std::vector<int> winners;
    if (currentSettings.teamMode) {
        const int winningTeam = currentSettings.playerTeams[winnerIdx];
        for (int i = 0; i < currentSettings.playerCount; ++i) {
            if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE &&
                currentSettings.playerTeams[i] == winningTeam) {
                winners.push_back(i);
            }
        }
    } else {
        winners.push_back(winnerIdx);
    }

    const bool abandonedRound =
        !currentSettings.continueWhenPlayersLeave && HasDepartedPlayers();

    for (int idx : winners) {
        BubbleArray& winner = bubbleArrays[idx];
        winner.mpWinner = true;
        winner.penguinSprite.PlayAnimation(10);
        if (!abandonedRound) winner.winCount++;
    }

    if (!abandonedRound) {
        if (winnerIdx == 0) winsP1++;
        else winsP2++;
    }
    Update2PText();
    UpdatePlayerNameWinText();

    UpdateDepartureMatchTermination();
    if (!abandonedRound && currentSettings.victoriesLimit > 0) {
        for (int idx : winners) {
            if (bubbleArrays[idx].winCount >= currentSettings.victoriesLimit) {
                gameMatchOver = true;
                SDL_Log("Match over! Player %d reached %d victories",
                        idx, currentSettings.victoriesLimit);
                break;
            }
        }
    }

    if (sendNetworkFinish &&
        bubbleArrays[winnerIdx].playerState != BubbleArray::PlayerState::LEFT) {
        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected() && netClient->GetState() == IN_GAME) {
            std::string finishMessage = "F" + bubbleArrays[winnerIdx].playerNickname;
            netClient->SendGameData(finishMessage.c_str());
            SDL_Log("Sent win notification: %s",
                    bubbleArrays[winnerIdx].playerNickname.c_str());
        }
    }
}

void BubbleGame::FinishRoundAsDraw() {
    SDL_Log("Draw game - all players are dead!");
    gameFinish = true;
    gameLost = true;
    roundWinnerIdx = -1;
    UpdateDepartureMatchTermination();
}

void BubbleGame::CheckGameState(BubbleArray &bArray, bool countForRoot) {
    // Only actual fired shots advance the compressor/new-root counter; chain-reaction
    // landings must not (original: count_for_root=0 for chain landings vs =1 for real
    // shots, bin/frozen-bubble ~line 2566 vs the fire block).
    if (countForRoot && !bArray.compressionDisabled) {
        bArray.turnsToCompress--;
        if (bArray.turnsToCompress == 1) bArray.waitPrelight = PRELIGHT_FAST;
        if (bArray.turnsToCompress == 0) {
            ResetPrelight(bArray);
            bArray.waitPrelight = PRELIGHT_SLOW;
            if (currentSettings.playerCount < 2) {
                bArray.turnsToCompress = 9;
                bArray.dangerZone--;
                bArray.numSeparators++;
                bArray.ExpandOffset(0, 28);
                bArray.compressorRct.y += 28;
                PlaySFX("newroot_solo");
            }
            else {
                ExpandNewLane(bArray);
                bArray.turnsToCompress = 12;
                PlaySFX("newroot");
            }
        }
    }
    if (bArray.allClear() &&
        (currentSettings.playerCount < 2 || currentSettings.clearMode)) {
        // Award bonus for clearing the level!
        if (currentSettings.playerCount < 2) {
            int clearBonus = 1000;
            bArray.score += clearBonus;

            // Submit score when level is cleared
            SubmitScore(bArray);
        }

        if (currentSettings.playerCount < 2) {
            gameFinish = true;
            gameWon = true;
            panelRct = {SCREEN_CENTER_X - 173, 480 - 289, 329, 159};
            bArray.penguinSprite.PlayAnimation(10);
        } else {
            ResolveRoundOutcome(
                bArray.playerAssigned,
                RoundWinCause::Clear,
                currentSettings.networkGame && bArray.playerAssigned == 0);
        }
    }
    // Check if ANY player hit the danger zone (original: verify_if_end() at line 1970-1975)
    // The original runs this sweep every frame over ALL players regardless of network vs.
    // local play, so any multiplayer game (network or local) must sweep every player here —
    // otherwise a local player pushed into the danger zone purely by incoming malus (rather
    // than their own shot) wouldn't be flagged lost until they next fired themselves.
    if (currentSettings.playerCount >= 2) {
        ResolveDangerZoneLosses();
    } else {
        // Single player - only check the current (only) player
        if (bArray.bubbleOnDanger() && bArray.playerState == BubbleArray::PlayerState::ALIVE) {
            panelRct = {SCREEN_CENTER_X - 173, 480 - 248, 345, 124};
            bArray.curLaunchRct = {bArray.curLaunchRct.x - 1, bArray.curLaunchRct.y - 1, 34, 48};
            ApplyPlayerLoss(bArray);
            gameFinish = true;
            gameLost = true;
            roundWinnerIdx = -1;
        }
    }
}

void BubbleGame::FinalizeRoundStats() {
    // Called once per round when the round ends. Rolls each player's per-round stats
    // into their match totals, and (in network games) broadcasts the local player's
    // round stats so all clients can render an accurate per-player table.
    if (currentSettings.networkGame) {
        BubbleArray &me = bubbleArrays[0];  // local player is authoritative for itself
        me.mFired += me.rFired;
        me.mPopped += me.rPopped;
        me.mSent  += me.rSent;
        me.mRecv  += me.rRecv;
        me.mKills += me.rKills;

        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected() && netClient->GetState() == IN_GAME) {
            char statsMsg[64];
            snprintf(statsMsg, sizeof(statsMsg), "S%d:%d:%d:%d:%d",
                     me.rFired, me.rPopped, me.rSent, me.rRecv, me.rKills);
            netClient->SendGameData(statsMsg);
        }
        // Remote arrays' totals are accumulated as their 'S' messages arrive.
    } else {
        // Local multiplayer: every array is locally tracked.
        for (int i = 0; i < currentSettings.playerCount; i++) {
            bubbleArrays[i].mFired  += bubbleArrays[i].rFired;
            bubbleArrays[i].mPopped += bubbleArrays[i].rPopped;
            bubbleArrays[i].mSent   += bubbleArrays[i].rSent;
            bubbleArrays[i].mRecv   += bubbleArrays[i].rRecv;
            bubbleArrays[i].mKills  += bubbleArrays[i].rKills;
        }
    }
    roundsPlayed++;
}

void BubbleGame::AddMalusAlert(BubbleArray &target, const std::string &fromNick, int count) {
    if (count <= 0) return;
    const int ALERT_FRAMES = 150;  // ~2.5s at 60fps
    std::string nick = fromNick.empty() ? "Someone" : fromNick;
    // Aggregate repeated hits from the same sender into one toast.
    for (auto &a : target.malusAlerts) {
        if (a.fromNick == nick && a.framesLeft > 0) {
            a.count += count;
            a.framesLeft = ALERT_FRAMES;
            return;
        }
    }
    target.malusAlerts.push_back({nick, count, ALERT_FRAMES});
    if (target.malusAlerts.size() > 4) target.malusAlerts.erase(target.malusAlerts.begin());
}

void BubbleGame::SendLobbyMatchSummary() {
    // Leader posts a match summary to the lobby chatroom. Called from QuitToTitle AFTER
    // PartGame() has returned us to IN_LOBBY (so TALK uses the lobby text protocol).
    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient || !netClient->IsConnected()) return;
    if (!netClient->IsLeader()) return;        // only one client posts
    if (roundsPlayed <= 0) return;             // nothing meaningful to report
    if (currentSettings.playerCount < 2) return;

    char line[256];
    snprintf(line, sizeof(line), "--- Match over (%d round%s) ---",
             roundsPlayed, roundsPlayed == 1 ? "" : "s");
    netClient->SendTalk(line);

    for (int i = 0; i < currentSettings.playerCount; i++) {
        BubbleArray &p = bubbleArrays[i];
        std::string name = StatsPlayerName(p, i, true);
        snprintf(line, sizeof(line),
                 "%s: %d win%s | fired %d, popped %d, atk %d, def %d",
                 name.c_str(), p.winCount, p.winCount == 1 ? "" : "s",
                 p.mFired, p.mPopped, p.mSent, p.mRecv);
        netClient->SendTalk(line);
    }
}
