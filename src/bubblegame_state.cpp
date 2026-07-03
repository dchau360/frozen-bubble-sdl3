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

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient || !netClient->IsConnected() || netClient->GetState() != IN_GAME) {
        return;
    }

    // Original logic at frozen-bubble line 1204-1227
    // Two modes:
    // 1. Split malus to ALL living opponents (default)
    // 2. Send ALL malus to ONE specific target (single player targetting mode)

    // Count living opponents (exclude local player at array 0)
    std::vector<int> livingOpponents;
    for (int i = 1; i < currentSettings.playerCount; i++) {
        if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) {
            livingOpponents.push_back(i);
        }
    }

    if (livingOpponents.empty()) {
        SDL_Log("No living opponents to send malus to");
        return;
    }

    // Single player targeting mode: send all malus to ONE opponent only when the player has
    // actively selected a target (original lines 1217-1227). With no target selected
    // (sendMalusToOne == -1), fall through to splitting among all living opponents — matching
    // the original (line 1204: "if (!sendmalustoone) { split }").
    if (currentSettings.singlePlayerTargetting && sendMalusToOne != -1) {
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

    audMixer->PlaySFX("malus");
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

// Handle player loss and check win conditions (original: sub lose() at line 1906-1968)
void BubbleGame::HandlePlayerLoss(BubbleArray &bArray) {
    SDL_Log("HandlePlayerLoss: player %d lost", bArray.playerAssigned);

    // Mark player as lost (original line 1926: $pdata{$player}{state} = 'lost')
    bArray.playerState = BubbleArray::PlayerState::LOST;

    // Play lose sound
    audMixer->PlaySFX("lose");

    if (currentSettings.networkGame && currentSettings.playerCount >= 2) {
        // NOTE: Don't send death notification - in the original, each client independently
        // detects deaths by checking synchronized bubble positions via 's' messages.
        // The 'l' message means "left" (disconnected), not "lost" (died).
        // Original: frozen-bubble lines 1925-1960
        // Multiplayer network game
        int livingCount = CountLivingPlayers();
        SDL_Log("Living players: %d", livingCount);

        if (livingCount == 1) {
            // Find the winner (the last living player)
            int winnerIdx = -1;
            for (int i = 0; i < currentSettings.playerCount; i++) {
                if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) {
                    winnerIdx = i;
                    break;
                }
            }

            if (winnerIdx >= 0) {
                // We have a winner! Game ends immediately (original lines 1933-1944)
                SDL_Log("Winner found: player %d", winnerIdx);
                bubbleArrays[winnerIdx].mpWinner = true;
                bubbleArrays[winnerIdx].penguinSprite.PlayAnimation(10);

                // Guard: only process once per round.
                // Multiple clients independently detect the same winner and each send 'F'.
                // The server relays each 'F' to the *other* players (not back to sender).
                // So if we get here first (before receiving any 'F'), we count locally and
                // the incoming 'F' from the other client is blocked by the !gameFinish guard
                // in the 'F' handler. If 'F' somehow arrived first, gameFinish is already
                // set and we skip the increment here.
                if (!gameFinish) {
                    gameFinish = true;

                    if (winnerIdx == 0) {
                        winsP1++;
                    } else {
                        winsP2++;
                    }
                    bubbleArrays[winnerIdx].winCount++;
                    Update2PText();

                    // Check victories limit
                    if (currentSettings.victoriesLimit > 0 &&
                        bubbleArrays[winnerIdx].winCount >= currentSettings.victoriesLimit) {
                        gameMatchOver = true;
                        SDL_Log("Match over! Player %d reached %d victories",
                                winnerIdx, currentSettings.victoriesLimit);
                    }

                    // Send 'F' to notify other clients (original line 1943)
                    NetworkClient* netClient = NetworkClient::Instance();
                    if (netClient && netClient->IsConnected() && netClient->GetState() == IN_GAME && bubbleArrays[winnerIdx].playerState != BubbleArray::PlayerState::LEFT) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "F%s", bubbleArrays[winnerIdx].playerNickname.c_str());
                        netClient->SendGameData(msg);
                        SDL_Log("Sent win notification: %s", bubbleArrays[winnerIdx].playerNickname.c_str());
                    }
                }
            }
        } else if (livingCount == 0) {
            // All players dead - draw game (no winner)
            // Original handles this in the 'finished' state processing (line 2336, 2353)
            SDL_Log("Draw game - all players are dead!");
            gameFinish = true;
            gameLost = true;  // Mark as lost (no winner)

            // Don't send F message since there's no winner
            // Each client will independently detect all players are dead
        } else if (livingCount > 1) {
            // More players still alive, game continues (original lines 1946-1950)
            SDL_Log("Game continues, %d players still alive", livingCount);
        }
    } else if (currentSettings.playerCount == 2 && !currentSettings.networkGame) {
        // Local 2-player game
        int winnerIdx = bArray.playerAssigned == 0 ? 1 : 0;
        gameFinish = true;
        bubbleArrays[winnerIdx].mpWinner = true;
        bubbleArrays[winnerIdx].penguinSprite.PlayAnimation(10);

        if (winnerIdx == 0) winsP1++;
        else winsP2++;
        Update2PText();
    } else if (currentSettings.playerCount == 1) {
        // Single player: original line 1964 — $pdata{state} = "lost $player"
        gameFinish = true;
        gameLost = true;
    }
}

void BubbleGame::CheckGameState(BubbleArray &bArray) {
    if (bArray.compressionDisabled) return;
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
                audMixer->PlaySFX("newroot_solo");
            }
            else {
                ExpandNewLane(bArray);
                bArray.turnsToCompress = 12;
                audMixer->PlaySFX("newroot");
            }
        }
    if (bArray.allClear()) {
        // Award bonus for clearing the level!
        if (currentSettings.playerCount < 2) {
            int clearBonus = 1000;
            bArray.score += clearBonus;

            // Submit score when level is cleared
            SubmitScore(bArray);
        }

        gameFinish = true;
        if (currentSettings.playerCount < 2) gameWon = true;
        else {
            audMixer->PlaySFX("lose");
            bArray.mpWinner = true;

            // In network games, only local player (array 0) processes wins
            if (currentSettings.networkGame && bArray.playerAssigned == 0) {
                // Local player cleared all bubbles - we won!
                // Count locally: server does NOT relay our own 'F' back to us.
                winsP1++;
                bArray.winCount++;
                Update2PText();

                if (currentSettings.victoriesLimit > 0 &&
                    bArray.winCount >= currentSettings.victoriesLimit) {
                    gameMatchOver = true;
                    SDL_Log("Match over! Local player reached %d victories",
                            currentSettings.victoriesLimit);
                }

                // Send 'F' to inform opponents (original line 1943)
                NetworkClient* netClient = NetworkClient::Instance();
                if (netClient && netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                    std::string fMsg = "F" + netClient->GetPlayerNick();
                    netClient->SendGameData(fMsg.c_str());
                    SDL_Log("Sent win notification: F%s", netClient->GetPlayerNick().c_str());
                }
            } else if (!currentSettings.networkGame) {
                // Non-network multiplayer (local 2P)
                if (bArray.playerAssigned == 0) winsP1++;
                else winsP2++;
                Update2PText();
            }
        }
        panelRct = {SCREEN_CENTER_X - 173, 480 - 289, 329, 159};
        bArray.penguinSprite.PlayAnimation(10);
    }
    // Check if ANY player hit the danger zone (original: verify_if_end() at line 1970-1975)
    // The original runs this sweep every frame over ALL players regardless of network vs.
    // local play, so any multiplayer game (network or local) must sweep every player here —
    // otherwise a local player pushed into the danger zone purely by incoming malus (rather
    // than their own shot) wouldn't be flagged lost until they next fired themselves.
    if (currentSettings.playerCount >= 2) {
        // Check all players for danger zone (original: iter_players with cy > 11 check)
        for (int i = 0; i < currentSettings.playerCount; i++) {
            BubbleArray &checkArray = bubbleArrays[i];
            if (checkArray.bubbleOnDanger() && checkArray.playerState == BubbleArray::PlayerState::ALIVE) {
                if (!currentSettings.networkGame || i == 0) {
                    // Local player lost (network games only show this HUD cue for the local
                    // player; local multiplayer shows it for whichever board actually lost)
                    panelRct = {SCREEN_CENTER_X - 173, 480 - 248, 345, 124};
                    checkArray.curLaunchRct = {checkArray.curLaunchRct.x - 1, checkArray.curLaunchRct.y - 1, 34, 48};
                }
                checkArray.penguinSprite.PlayAnimation(11);
                HandlePlayerLoss(checkArray);
            }
        }
    } else {
        // Single player - only check the current (only) player
        if (bArray.bubbleOnDanger() && bArray.playerState == BubbleArray::PlayerState::ALIVE) {
            panelRct = {SCREEN_CENTER_X - 173, 480 - 248, 345, 124};
            bArray.curLaunchRct = {bArray.curLaunchRct.x - 1, bArray.curLaunchRct.y - 1, 34, 48};
            bArray.penguinSprite.PlayAnimation(11);
            HandlePlayerLoss(bArray);
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

        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected() && netClient->GetState() == IN_GAME) {
            char statsMsg[64];
            snprintf(statsMsg, sizeof(statsMsg), "S%d:%d:%d:%d",
                     me.rFired, me.rPopped, me.rSent, me.rRecv);
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
