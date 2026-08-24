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

void BubbleGame::SendNetworkBubbleShot(BubbleArray &bArray) {
    if (!currentSettings.networkGame) return;

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient->IsConnected() || netClient->GetState() != IN_GAME) {
        SDL_Log("SendNetworkBubbleShot: Not connected or not in game (connected=%d, state=%d)",
                netClient->IsConnected(), netClient->GetState());
        return;
    }

    // Send shot in original protocol format: f{angle}:{nextcolor}
    // The color sent is the player's NEW next bubble (what will come after current)
    // This matches original frozen-bubble line 2163: gsend(sprintf("f%.3f:$pdata{$::p}{nextcolor}", $angle{$::p}))
    for (const SingleBubble &sBubble : singleBubbles) {
        if (sBubble.launching && sBubble.assignedArray == bArray.playerAssigned) {
            char shotData[128];
            snprintf(shotData, sizeof(shotData), "f%.3f:%d",
                sBubble.direction,
                bArray.nextBubble);  // Send the NEW next bubble color, not the launched bubble's color
            SDL_Log("Sending shot: angle=%.3f, nextBubble=%d (launched=%d)",
                    sBubble.direction, bArray.nextBubble, sBubble.bubbleId);
            netClient->SendGameData(shotData);
            break;
        }
    }
}

void BubbleGame::ProcessNetworkMessages() {
    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient->IsConnected()) {
        static int notConnectedCount = 0;
        if (notConnectedCount++ % 60 == 0) {  // Log once per second at 60fps
            SDL_Log("ProcessNetworkMessages: Not connected (state=%d)", netClient->GetState());
        }
        return;
    }

    // Update network client
    netClient->Update();

    // Process all pending messages
    while (netClient->HasMessage()) {
        std::string msg = netClient->GetNextMessage();

        // Parse network game messages in format GAMEMSG:{senderId}:{data}
        if (msg.find("GAMEMSG:") == 0) {
            int senderId;
            char gameData[512];
            if (sscanf(msg.c_str(), "GAMEMSG:%d:%511[^\n]", &senderId, gameData) == 2) {
                SDL_Log("Processing game message from player %d: %s", senderId, gameData);

                // Ignore our own messages echoed back
                if (senderId == netClient->GetMyPlayerId()) {
                    SDL_Log("Ignoring our own message (ID=%d)", senderId);
                    continue;
                }

                // Parse game data - original protocol (first character is message type)
                char msgType = gameData[0];
                switch (msgType) {
                    case 'f': {
                        // Fire: f{angle}:{nextcolor}
                        // The color in message is opponent's NEW next bubble (after their current shot)
                        // Create their shot with their CURRENT bubble, then update their next
                        // This matches original frozen-bubble line 1404: ($angle{$player}, $pdata{$player}{nextcolor}) = $params
                        float angle;
                        int opponentNewNextColor;
                        if (sscanf(gameData + 1, "%f:%d", &angle, &opponentNewNextColor) == 2) {
                            // Find which player array this sender is using (original: $actions{$player}{mp_fire} = 1)
                            int opponentIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                    opponentIdx = i;
                                    break;
                                }
                            }

                            // If not found, assign to next available remote slot
                            if (opponentIdx == -1) {
                                NetworkClient* netClient = NetworkClient::Instance();
                                for (int i = 1; i < currentSettings.playerCount; i++) {
                                    if (bubbleArrays[i].lobbyPlayerId == -1) {
                                        bubbleArrays[i].lobbyPlayerId = senderId;
                                        bubbleArrays[i].playerNickname = netClient->GetPlayerNickname(senderId);
                                        opponentIdx = i;
                                        SDL_Log("'f' message: Assigned lobbyId %d (nick='%s') to player array %d",
                                                senderId, bubbleArrays[i].playerNickname.c_str(), i);
                                        break;
                                    }
                                }
                            }

                            if (opponentIdx < 0) {
                                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find/assign player for 'f' message from senderId %d", senderId);
                                break;
                            }

                            BubbleArray &opponentArray = bubbleArrays[opponentIdx];

                            // Set flag to fire in the game loop (original line 1403: $actions{$player}{mp_fire} = 1)
                            // Store angle and update nextcolor (original line 1404)
                            opponentArray.mpFirePending = true;
                            opponentArray.pendingAngle = angle;
                            opponentArray.shooterSprite.angle = angle;  // Update shooter angle for visual display
                            opponentArray.nextBubble = opponentNewNextColor;  // Update their next bubble color

                            SDL_Log("Received fire command from player %d (array %d): angle=%.3f, nextColor=%d - will fire in game loop",
                                    senderId, opponentIdx, angle, opponentNewNextColor);
                        } else {
                            SDL_Log("ERROR: Failed to parse fire message: %s", gameData);
                        }
                        break;
                    }
                    case 'p': {
                        // Ping - ignore (keepalive only)
                        break;
                    }
                    case 'n': {
                        // Newgame - an opponent is ready for next round
                        opponentsReadyCount++;
                        SDL_Log("Opponent ready for new game (received 'n'), count=%d/%d",
                                opponentsReadyCount, connectedPlayerCount - 1);
                        if (opponentsReadyCount >= connectedPlayerCount - 1) {
                            opponentReadyForNewGame = true;
                        }

                        // Auto-respond for ALL player counts (original: receiving 'n' triggers mp_newgame->()
                        // which immediately sends our own 'n' without requiring local keypress).
                        // This means only ONE player needs to press a key; everyone else auto-responds.
                        if (!waitingForOpponentNewGame && gameFinish && !gameMatchOver) {
                            SDL_Log("Opponent pressed key first - auto-sending 'n' (%dP game)", currentSettings.playerCount);
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                                netClient->SendGameData("n");
                                waitingForOpponentNewGame = true;
                            }
                        }
                        break;
                    }
                    case 's': {
                        // Stick: s{cx}:{cy}:{bubbleColor}:{nc0} {nc1} ... {nc7}
                        // Perl format: "s$cx:$cy:$col:@{$pdata{$::p}{nextcolors}}" (space-sep 8 colors)
                        // Opponent's bubble has stuck - place it at exact transmitted position
                        // Format matches original: frozen-bubble line 1418-1425
                        int cx, cy, bubbleColor;
                        if (sscanf(gameData + 1, "%d:%d:%d", &cx, &cy, &bubbleColor) == 3) {
                            // Parse nextColors: find the 3rd colon, then read space-separated ints
                            std::vector<int> recvNextColors;
                            const char* p = gameData + 1;
                            int colons = 0;
                            while (*p && colons < 3) { if (*p == ':') colons++; p++; }
                            while (*p) {
                                int c; int consumed = 0;
                                if (sscanf(p, "%d%n", &c, &consumed) == 1) {
                                    recvNextColors.push_back(c);
                                    p += consumed;
                                    while (*p == ' ') p++;
                                } else break;
                            }
                            int nextBubble = recvNextColors.empty() ? 0 : recvNextColors[0];
                            SDL_Log("Received stick: col=%d row=%d color=%d nextColors[%zu] from lobbyId=%d", cx, cy, bubbleColor, recvNextColors.size(), senderId);

                            // Find or assign this remote player's array
                            int opponentIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                    opponentIdx = i;
                                    break;
                                }
                            }

                            // If not found, assign to next available remote slot
                            if (opponentIdx == -1) {
                                NetworkClient* netClient = NetworkClient::Instance();
                                for (int i = 1; i < currentSettings.playerCount; i++) {
                                    if (bubbleArrays[i].lobbyPlayerId == -1) {
                                        bubbleArrays[i].lobbyPlayerId = senderId;
                                        bubbleArrays[i].playerNickname = netClient->GetPlayerNickname(senderId);
                                        opponentIdx = i;
                                        SDL_Log("'s' message: Assigned lobbyId %d (nick='%s') to player array %d",
                                                senderId, bubbleArrays[i].playerNickname.c_str(), i);
                                        break;
                                    }
                                }
                            }

                            if (opponentIdx < 0) {
                                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find/assign player for senderId %d", senderId);
                                break;
                            }

                            BubbleArray &opponentArray = bubbleArrays[opponentIdx];

                            // Set flag for stick to be processed in game loop (original line 1422: $actions{$player}{mp_stick} = 1)
                            // Store stick data (original line 1423)
                            opponentArray.mpStickPending = true;
                            opponentArray.stickCx = cx;
                            opponentArray.stickCy = cy;
                            opponentArray.stickCol = bubbleColor;
                            opponentArray.nextBubble = nextBubble;  // Update their next bubble (front of nextColors)
                            // Sync full nextColors queue (Perl-compatible: used by ExpandNewLane for new root row)
                            if (!recvNextColors.empty()) {
                                opponentArray.nextColors = recvNextColors;
                            }

                            SDL_Log("Set mp_stick flag for player %d (array %d): cx=%d cy=%d col=%d nextBubble=%d nextColors[%zu]",
                                    senderId, opponentIdx, cx, cy, bubbleColor, nextBubble, recvNextColors.size());
                        } else {
                            SDL_Log("ERROR: Failed to parse stick message: %s", gameData);
                        }
                        break;
                    }
                    case 'g': {
                        // Receive malus attack from opponent
                        // Format: g{destPlayerNick}:{count}
                        // Original at line 1425-1432
                        char destNick[64];
                        int malusCount;
                        if (sscanf(gameData + 1, "%63[^:]:%d", destNick, &malusCount) == 2) {
                            // Only process if this message is for us (original line 1428)
                            NetworkClient* netClient = NetworkClient::Instance();
                            std::string myNick = netClient ? netClient->GetPlayerNick() : "";
                            SDL_Log("'g' message: dest='%s' count=%d myNick='%s' senderId=%d myId=%d",
                                    destNick, malusCount, myNick.c_str(), senderId,
                                    netClient ? netClient->GetMyPlayerId() : -1);

                            // Kill attribution: every client sees every 'g' message broadcast
                            // (not just the addressee), so each independently tracks who last
                            // attacked whom from the same messages -- resolve the sender's
                            // array index once here for both branches below.
                            int senderIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) { senderIdx = i; break; }
                            }

                            if (netClient && myNick == destNick) {
                                SDL_Log("  -> YES, this malus is FOR ME! Adding to my queue");


                                // Add to local player's malus queue (opponent attacks us, so array 0)
                                // Store current frame number for each malus
                                for (int i = 0; i < malusCount; i++) {
                                    bubbleArrays[0].malusQueue.push_back(frameCount);
                                }
                                bubbleArrays[0].rRecv += malusCount;  // Stats: malus received
                                if (senderIdx >= 0) bubbleArrays[0].lastAttackerIdx = senderIdx;
                                AddMalusAlert(bubbleArrays[0], netClient->GetPlayerNickname(senderId), malusCount);
                            } else {
                                SDL_Log("  -> NO, not for me (dest='%s' != myNick='%s'), IGNORING",
                                        destNick, myNick.c_str());
                            }
                        } else {
                            SDL_Log("ERROR: Failed to parse malus message: %s", gameData);
                        }
                        break;
                    }
                    case 'm': {
                        // Receive malus bubble from opponent (they generated it, we display it)
                        // Format: m{bubbleId}:{cx}:{cy}:{stick_y}
                        // Original at line 1435-1451
                        // Skip our own 'm' messages echoed back by server (original: only process from others)
                        {
                            NetworkClient* netClientM = NetworkClient::Instance();
                            if (netClientM && (int)netClientM->GetMyPlayerId() == senderId) {
                                SDL_Log("Ignoring own 'm' echo from server");
                                break;
                            }
                        }
                        int bubbleId, cx, cy, stickY;
                        if (sscanf(gameData + 1, "%d:%d:%d:%d", &bubbleId, &cx, &cy, &stickY) == 4) {
                            SDL_Log("Received opponent's malus bubble from senderId=%d: color=%d cx=%d cy=%d stickY=%d",
                                    senderId, bubbleId, cx, cy, stickY);

                            // Find which array this opponent belongs to
                            int opponentIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                    opponentIdx = i;
                                    break;
                                }
                            }

                            if (opponentIdx < 0) {
                                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                           "Received 'm' message from unknown senderId %d, ignoring", senderId);
                                break;
                            }

                            // Ignore malus for a player whose board is already frozen (dead), instead
                            // of wrongly overlaying bubbles onto it (original ~line 1435).
                            if (bubbleArrays[opponentIdx].playerState != BubbleArray::PlayerState::ALIVE) break;

                            BubbleArray &opponentArray = bubbleArrays[opponentIdx];
                            // Mini players use half bubble size
                            bool isMini = (currentSettings.playerCount >= 3 && opponentIdx >= 1);
                            int bubbleSize = isMini ? 16 : 32;
                            int rowSize = bubbleSize * 7 / 8;  // 14 for mini, 28 for full
                            int smallerSep = (cy % 2 == 0) ? 0 : bubbleSize / 2;
                            float startX = (smallerSep + bubbleSize * cx) + opponentArray.bubbleOffset.x;
                            float startY = (rowSize * cy) + opponentArray.bubbleOffset.y;

                            MalusBubble malus = {
                                opponentIdx,  // opponent's array index
                                bubbleId,
                                cx, cy,
                                stickY,
                                startX, startY,
                                {(int)startX, (int)startY},
                                false,
                                false
                            };

                            malusBubbles.push_back(malus);
                        } else {
                            SDL_Log("ERROR: Failed to parse malus bubble message: %s", gameData);
                        }
                        break;
                    }
                    case 'M': {
                        // Opponent's malus bubble stuck
                        // Format: M{cx}:{stick_y}
                        // Original at line 1453-1466
                        int cx, stickY;
                        if (sscanf(gameData + 1, "%d:%d", &cx, &stickY) == 2) {
                            SDL_Log("Opponent's malus bubble stuck from senderId=%d: cx=%d stickY=%d",
                                    senderId, cx, stickY);

                            // Find which array this opponent belongs to
                            int opponentIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                    opponentIdx = i;
                                    break;
                                }
                            }

                            if (opponentIdx < 0) {
                                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                           "Received 'M' message from unknown senderId %d, ignoring", senderId);
                                break;
                            }

                            // Ignore malus for a player whose board is already frozen (dead), instead
                            // of wrongly overlaying bubbles onto it (original ~line 1453).
                            if (bubbleArrays[opponentIdx].playerState != BubbleArray::PlayerState::ALIVE) break;

                            // Find and stick the corresponding malus bubble on opponent's board
                            for (auto &malus : malusBubbles) {
                                if (malus.assignedArray == opponentIdx && malus.cx == cx && malus.stickY == stickY && !malus.shouldClear) {
                                    SDL_Log("Found opponent's malus bubble to stick on array %d", opponentIdx);
                                    BubbleArray &opponentArray = bubbleArrays[opponentIdx];
                                    opponentArray.PlacePlayerBubble(malus.bubbleId, stickY, cx);
                                    opponentArray.newShoot = true;
                                    malus.shouldClear = true;
                                    CheckPossibleDestroy(opponentArray);
                                    // Don't check game state for opponent - they will send 'F' message if they win/lose
                                    break;
                                }
                            }
                        } else {
                            SDL_Log("ERROR: Failed to parse malus stick message: %s", gameData);
                        }
                        break;
                    }
                    case 'F': {
                        // Finish/Win notification from remote player
                        // Perl format: "F{winnerNick}" (no separator) - original line 1467-1470
                        // Also handle legacy C++ format "F:{idx}" for backward compat
                        std::string winnerNick = gameData + 1;  // Everything after 'F'
                        SDL_Log("Received win notification: F'%s'", winnerNick.c_str());

                        int winnerPlayer = -1;

                        // Try legacy format first: "F:{digit}"
                        if (winnerNick.size() >= 2 && winnerNick[0] == ':' && isdigit((unsigned char)winnerNick[1])) {
                            winnerPlayer = winnerNick[1] - '0';
                        } else {
                            // Perl format: match nick to player arrays
                            NetworkClient* netClient = NetworkClient::Instance();
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].playerNickname == winnerNick) {
                                    winnerPlayer = i;
                                    break;
                                }
                            }
                            // If nick matches our own nick, winner is local player (array 0)
                            if (winnerPlayer == -1 && netClient && netClient->GetPlayerNick() == winnerNick) {
                                winnerPlayer = 0;
                            }
                        }

                        if (winnerPlayer >= 0 && winnerPlayer < currentSettings.playerCount) {
                            // Guard: only process the first 'F' per round (multiple clients may send it)
                            if (!gameFinish) {
                                RoundWinCause cause =
                                    currentSettings.clearMode && bubbleArrays[winnerPlayer].allClear()
                                        ? RoundWinCause::Clear
                                        : RoundWinCause::Remote;
                                ResolveRoundOutcome(winnerPlayer, cause, false);
                            }
                        } else {
                            SDL_Log("ERROR: Could not identify winner from F message: '%s'", winnerNick.c_str());
                        }
                        break;
                    }
                    case 'S': {
                        // Round stats sync from a remote player: S{fired}:{popped}:{sent}:{recv}:{kills}
                        // Sent once per round by each client when its round ends. The trailing
                        // :{kills} field is newer than the rest; sscanf fills rf/rp/rs/rr from the
                        // same call even if it stops at 4 (kills stays at its 0 default), so this
                        // stays compatible with any peer still on the old 4-field format.
                        int rf, rp, rs, rr, rk = 0;
                        if (sscanf(gameData + 1, "%d:%d:%d:%d:%d", &rf, &rp, &rs, &rr, &rk) >= 4) {
                            int idx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) { idx = i; break; }
                            }
                            if (idx >= 1) {
                                BubbleArray &pa = bubbleArrays[idx];
                                pa.rFired = rf; pa.rPopped = rp; pa.rSent = rs; pa.rRecv = rr; pa.rKills = rk;
                                pa.mFired += rf; pa.mPopped += rp; pa.mSent += rs; pa.mRecv += rr; pa.mKills += rk;
                                SDL_Log("Round stats from player %d (array %d): F%d P%d A%d D%d K%d",
                                        senderId, idx, rf, rp, rs, rr, rk);
                            } else {
                                SDL_Log("'S' stats from unknown senderId %d, ignoring", senderId);
                            }
                        }
                        break;
                    }
                    case 't': {
                        // In-game chat from remote player
                        InGameChatMsg chatMsg;
                        chatMsg.nick = netClient->GetPlayerNickname(senderId);
                        if (chatMsg.nick.empty()) chatMsg.nick = "Player";
                        // Dropped here rather than filtered at render time, so a
                        // blocked player's message never shows *and* never plays
                        // the chat sound -- an audible ping for a message you
                        // cannot see would be worse than not blocking at all.
                        if (GameSettings::Instance()->IsPlayerBlocked(chatMsg.nick)) break;
                        chatMsg.text = gameData + 1;
                        chatMsg.framesLeft = 300;  // 5 seconds at 60 fps
                        inGameChatMessages.push_back(chatMsg);
                        if (inGameChatMessages.size() > 10)
                            inGameChatMessages.erase(inGameChatMessages.begin());
                        PlaySFX("chatted");
                        break;
                    }
                    case 'l': {
                        // Player-left notification: a remote player disconnected mid-game
                        SDL_Log("Received player-left ('l') from lobby player ID %d", senderId);

                        // Find which player array this senderId corresponds to
                        int playerIdx = -1;
                        for (int i = 0; i < currentSettings.playerCount; i++) {
                            if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                playerIdx = i;
                                break;
                            }
                        }

                        if (playerIdx >= 0) {
                            HandlePlayerDeparture(playerIdx);
                        } else {
                            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                       "Received player-left from unknown player ID %d", senderId);
                        }

                        // If already waiting for new-game sync, check if the reduced threshold is now met
                        // (the disconnected player will never send 'n', so count them as ready)
                        if (waitingForOpponentNewGame && opponentsReadyCount >= connectedPlayerCount - 1) {
                            SDL_Log("All remaining connected opponents ready after disconnect - starting new game");
                            opponentReadyForNewGame = true;
                        }
                        break;
                    }
                    case 'A': {
                        // Opponent changed their targeting (original: command 'A' at line 1477)
                        // Format: A{targetNick} = opponent is targeting that player
                        //         A (empty)     = opponent cleared targeting
                        // Find which array the sender belongs to
                        int senderIdx = -1;
                        for (int i = 0; i < currentSettings.playerCount; i++) {
                            if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                senderIdx = i;
                                break;
                            }
                        }
                        if (senderIdx < 0) break;

                        NetworkClient* netClient = NetworkClient::Instance();
                        std::string myNick = netClient ? netClient->GetPlayerNick() : "";
                        const char* targetNick = gameData + 1;  // Skip 'A' prefix

                        if (strlen(targetNick) == 0 || myNick != targetNick) {
                            // Opponent cleared target or is targeting someone else - remove from attackingMe
                            attackingMe.erase(std::remove(attackingMe.begin(), attackingMe.end(), senderIdx),
                                              attackingMe.end());
                        } else {
                            // Opponent is targeting us (targetNick == myNick)
                            if (std::find(attackingMe.begin(), attackingMe.end(), senderIdx) == attackingMe.end()) {
                                attackingMe.push_back(senderIdx);
                            }
                        }
                        // Track all players' targets (not just who's targeting me)
                        if (strlen(targetNick) == 0) {
                            playerTargeting[senderIdx] = -1;  // cleared
                        } else {
                            // Find which array has targetNick
                            int targetIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].playerNickname == targetNick) { targetIdx = i; break; }
                            }
                            playerTargeting[senderIdx] = targetIdx;
                        }
                        SDL_Log("'A' message: sender=%d targetNick='%s' myNick='%s' attackingMe.size=%zu",
                                senderIdx, targetNick, myNick.c_str(), attackingMe.size());
                        ReRankNetView();  // attacker set/cleared: auto view re-ranks
                        break;
                    }
                    case 'b':
                    case 'N':
                    case 'T':
                        // Bubble sync messages from leader (SyncNetworkLevel).
                        // Route to syncQueue so WaitForBubble/WaitForNextBubble/WaitForTobeBubble
                        // can pick them up even if they arrived before ReloadGame was called.
                        SDL_Log("Routing bubble-sync message '%c' to syncQueue", msgType);
                        netClient->PushSyncMessage(msg);
                        break;
                    default:
                        SDL_Log("Unknown game message type: %c", msgType);
                        break;
                    }
            }
        } else if (msg.find("GAME_START") == 0) {
            SDL_Log("Network game starting!");
        } else if (msg.find("PLAYER_PART") == 0) {
            SDL_Log("Opponent left the game");
        }
    }
}
