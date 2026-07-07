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

#include "frozenbubble.h"
#include "bubblegame.h"
#include "audiomixer.h"
#include "highscoremanager.h"
#include "transitionmanager.h"
#include "gamesettings.h"
#include "platform.h"
#include "networkclient.h"

#include <fstream>
#include <sstream>
#include <set>
#include <queue>
#include <map>

#include <cmath>
#include <algorithm>
#include "bubblegame_internal.h"

void BubbleGame::Update2PText() {
    char plyp[16];
    snprintf(plyp, sizeof(plyp), "%i", winsP1);
    winsP1Text.UpdateText(renderer, plyp, 0);
    winsP1Text.UpdatePosition({(SCREEN_CENTER_X + 160), 12});

    snprintf(plyp, sizeof(plyp), "%i", winsP2);
    winsP2Text.UpdateText(renderer, plyp, 0);
    winsP2Text.UpdatePosition({(SCREEN_CENTER_X - 170), 12});
}


void BubbleGame::UpdatePlayerNameWinText() {
    // Update "PlayerName: WinCount" for each player in 3-5 player mode
    // Based on original Frozen Bubble 2 multiplayer layout (see CLAUDE.md and fb2-3-to-5-player.jpg)

    for (int i = 0; i < currentSettings.playerCount; i++) {
        BubbleArray &bArray = bubbleArrays[i];

        // Format: "PlayerName: WinCount"
        char nameWinStr[128];
        if (!bArray.playerNickname.empty()) {
            snprintf(nameWinStr, sizeof(nameWinStr), "%s: %d",
                     bArray.playerNickname.c_str(), bArray.winCount);
        } else {
            // Fallback: Try to get nickname from NetworkClient if we have lobbyPlayerId
            NetworkClient* netClient = NetworkClient::Instance();
            if (netClient && bArray.lobbyPlayerId >= 0) {
                std::string nick = netClient->GetPlayerNickname(bArray.lobbyPlayerId);
                if (!nick.empty()) {
                    bArray.playerNickname = nick;  // Cache it
                    snprintf(nameWinStr, sizeof(nameWinStr), "%s: %d", nick.c_str(), bArray.winCount);
                } else {
                    snprintf(nameWinStr, sizeof(nameWinStr), "Player %d: %d", i + 1, bArray.winCount);
                }
            } else {
                snprintf(nameWinStr, sizeof(nameWinStr), "Player %d: %d", i + 1, bArray.winCount);
            }
        }

        playerNameWinText[i].UpdateText(renderer, nameWinStr, 0);

        // Use fixed positions based on player layout (matching original FB2)
        // Positions are centered above/below each player's grid
        int textX, textY;

        switch (currentSettings.playerCount) {
            case 3:
                // 3 players: center (p1), top-left (rp1), top-right (rp2)
                if (i == 0) {
                    textX = 320; textY = 12;  // Center player at top
                } else if (i == 1) {
                    textX = 83; textY = 2;  // Top-left mini
                } else {
                    textX = 553; textY = 2;  // Top-right mini
                }
                break;
            case 4:
                // 4 players: center (p1), top-left (rp1), top-right (rp2), bottom-left (rp3)
                if (i == 0) {
                    textX = 320; textY = 12;
                } else if (i == 1) {
                    textX = 83; textY = 2;
                } else if (i == 2) {
                    textX = 553; textY = 2;
                } else {
                    textX = 83; textY = 465;  // Bottom-left mini
                }
                break;
            case 5:
                // 5 players: center (p1), all 4 corners
                if (i == 0) {
                    textX = 320; textY = 12;
                } else if (i == 1) {
                    textX = 83; textY = 2;
                } else if (i == 2) {
                    textX = 553; textY = 2;
                } else if (i == 3) {
                    textX = 83; textY = 465;
                } else {
                    textX = 553; textY = 465;  // Bottom-right mini
                }
                break;
            default:
                // Shouldn't reach here, but fallback
                textX = 320; textY = 12;
                break;
        }

        playerNameWinText[i].UpdatePosition({textX - (playerNameWinText[i].Coords()->w / 2), textY});
    }
}


void BubbleGame::UpdateScoreText(BubbleArray &bArray) {
    char scoreStr[64];
    // For 2-player network games, show only player nickname (no score) in wooden banners
    // For 3+ player games, show "Nickname: Score"
    // For single player, show "Score: X"
    if (currentSettings.networkGame && !bArray.playerNickname.empty()) {
        if (currentSettings.playerCount == 2) {
            // 2-player: show just nickname (scores shown separately)
            snprintf(scoreStr, sizeof(scoreStr), "%s", bArray.playerNickname.c_str());
        } else {
            // 3+ players: show nickname with score
            snprintf(scoreStr, sizeof(scoreStr), "%s: %d", bArray.playerNickname.c_str(), bArray.score);
        }
    } else {
        snprintf(scoreStr, sizeof(scoreStr), "Score: %d", bArray.score);
    }

    // Use shared scoreText object but update and render for each player
    // In multiplayer, this gets called once per player in the render loop
    // Each call updates the text and renders immediately at the player's score position
    scoreText.UpdateText(renderer, scoreStr, 0);
    scoreText.UpdatePosition(bArray.scorePos);

    // Render immediately (original: print_scores renders each player's score in the loop)
    { SDL_FRect fr = ToFRect(*scoreText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), scoreText.Texture(), nullptr, &fr); }
}


SDL_Texture** BubbleGame::GetBubbleTextures(bool mini) {
    GameSettings *settings = GameSettings::Instance();
    if (mini) {
        if (settings->colorBlind()) {
            return imgMiniColorblindBubbles;
        }
        return imgMiniBubbles;
    } else {
        if (settings->colorBlind()) {
            return imgColorblindBubbles;
        }
        return imgBubbles;
    }
}


static void DrawAimGuide(SDL_Renderer* rend, const BubbleArray& bArray, bool isMini) {
    // Mini players use half bubble size for spacing (matches RandomLevel,
    // GetClosestFreeCell, and AssignChainReactions' isMini handling).
    const int BUBBLE_SIZE = isMini ? 16 : 32;
    const int ROW_SIZE = BUBBLE_SIZE * 7 / 8;
    const float speed = (float)(BUBBLE_SPEED);  // 5 pixels/step

    float px = (float)bArray.curLaunchRct.x;
    float py = (float)bArray.curLaunchRct.y;
    float angle = bArray.shooterSprite.angle;
    // Scale the simulated step by deltaScale, exactly as SingleBubble::UpdatePosition()
    // does for the real bubble. deltaScale folds in the frame-rate normalization and
    // the user's speed-multiplier setting, so without it the guide's per-step distance
    // only matches the real shot's per-frame movement by coincidence at one particular
    // speed setting on a perfectly steady frame - at any other speed, or under frame
    // pacing variance, the guide's wall-bounce and grid-collision checks run at a
    // different granularity than actual gameplay and can predict the wrong landing cell.
    const float ds = FrozenBubble::Instance()->deltaScale;
    float dx = speed * cosf(angle) * ds;
    float dy = speed * sinf(angle) * ds;

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);

    for (int step = 0; step < 400; step++) {
        px += dx;
        py -= dy;  // Negative = move up

        if (px < bArray.leftLimit) {
            px = 2.0f * bArray.leftLimit - px;
            dx = -dx;
        }
        if (px > bArray.rightLimit - BUBBLE_SIZE) {
            px = 2.0f * (bArray.rightLimit - BUBBLE_SIZE) - px;
            dx = -dx;
        }

        if (py <= (float)bArray.topLimit) break;

        // Check grid collision every step now that the step size itself matches the
        // real per-frame movement (ds above already accounts for that granularity).
        {
            int cy = (int)((py - bArray.bubbleOffset.y + ROW_SIZE / 2.0f) / ROW_SIZE);
            if (cy >= 0 && cy < 13) {
                int oddRowOffset = ((int)bArray.bubbleMap[cy].size() == 7) ? BUBBLE_SIZE / 2 : 0;
                int cx = (int)((px - bArray.bubbleOffset.x + BUBBLE_SIZE / 2.0f - oddRowOffset) / BUBBLE_SIZE);
                if (cx >= 0 && cx < (int)bArray.bubbleMap[cy].size() &&
                    bArray.bubbleMap[cy][cx].bubbleId != -1) {
                    break;
                }
            }
        }

        // Draw a two-tone marker every 8 steps.  The dark rim and warm center
        // stay readable over both light and dark skin backgrounds.
        if (step % 8 == 0) {
            int alpha = 200 - step / 2;
            if (alpha < 30) alpha = 30;
            SDL_Rect rim = {(int)px + BUBBLE_SIZE / 2 - 4,
                            (int)py + BUBBLE_SIZE / 2 - 4, 8, 8};
            SDL_SetRenderDrawColor(rend, 24, 20, 38, (Uint8)alpha);
            { SDL_FRect fr = ToFRect(rim); SDL_RenderFillRect(rend, &fr); }

            SDL_Rect center = {rim.x + 2, rim.y + 2, 4, 4};
            SDL_SetRenderDrawColor(rend, 255, 211, 66, (Uint8)alpha);
            { SDL_FRect fr = ToFRect(center); SDL_RenderFillRect(rend, &fr); }
        }
    }

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
}


void BubbleGame::RenderMalusAlerts(SDL_Renderer *rend) {
    for (int i = 0; i < currentSettings.playerCount; i++) {
        BubbleArray &p = bubbleArrays[i];
        if (p.malusAlerts.empty()) continue;

        // Anchor just above the player's shooter; stack multiple alerts upward.
        const int ax = p.shooterSprite.rect.x;
        const int ay = p.shooterSprite.rect.y - 20;
        const int lineH = 18;

        int line = 0;
        for (auto &a : p.malusAlerts) {
            if (a.framesLeft <= 0) continue;
            char buf[96];
            snprintf(buf, sizeof(buf), "%s  +%d", a.fromNick.c_str(), a.count);
            malusAlertText.UpdateText(rend, buf, 0);
            int tw = malusAlertText.Coords()->w;
            int x = ax;
            if (x + tw > 636) x = 636 - tw;  // keep on-screen
            if (x < 4) x = 4;
            int y = ay - line * lineH;
            if (y < 2) y = 2;
            malusAlertText.UpdatePosition({x, y});
            SDL_Texture *tex = malusAlertText.Texture();
            if (tex) {
                Uint8 alpha = a.framesLeft >= 40 ? 255 : (Uint8)(a.framesLeft * 255 / 40);  // fade out
                SDL_SetTextureAlphaMod(tex, alpha);
                SDL_FRect fr = ToFRect(*malusAlertText.Coords());
                SDL_RenderTexture(rend, tex, nullptr, &fr);
                SDL_SetTextureAlphaMod(tex, 255);
            }
            line++;
        }

        // Age and prune expired alerts.
        for (auto &a : p.malusAlerts) a.framesLeft--;
        p.malusAlerts.erase(
            std::remove_if(p.malusAlerts.begin(), p.malusAlerts.end(),
                           [](const BubbleArray::MalusAlert &a) { return a.framesLeft <= 0; }),
            p.malusAlerts.end());
    }
}

// Resolve a display name for a player array (local player gets its lobby nick or "You").

std::string StatsPlayerName(const BubbleArray &arr, int idx, bool networkGame) {
    if (idx == 0) {
        if (networkGame) {
            std::string nick = NetworkClient::Instance()->GetPlayerNick();
            if (!nick.empty()) return nick;
        }
        return "You";
    }
    if (!arr.playerNickname.empty()) return arr.playerNickname;
    char buf[16];
    snprintf(buf, sizeof(buf), "Player %d", idx + 1);
    return buf;
}


void BubbleGame::RenderRoundStats(SDL_Renderer *rend) {
    // Post-round per-player stats table overlay (multiplayer only).
    const int n = currentSettings.playerCount;
    if (n < 2) return;

    const int boxW = 384, boxX = (640 - boxW) / 2, boxY = 6;
    const int rowH = 16, headH = 22;
    const int hintH = currentSettings.networkGame ? rowH : 0;
    const int boxH = headH + rowH * (n + 1) + hintH + 6;

    // Semi-transparent backing panel.
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_Rect bg = {boxX, boxY, boxW, boxH};
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 190);
    { SDL_FRect fr = ToFRect(bg); SDL_RenderFillRect(rend, &fr); }
    SDL_SetRenderDrawColor(rend, 255, 255, 255, 90);
    { SDL_FRect fr = ToFRect(bg); SDL_RenderRect(rend, &fr); }
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);

    // Column x offsets (numbers right-anchored-ish via left placement that fits 14px font).
    const int colName = boxX + 8;
    const int colFired = boxX + 168;
    const int colPopped = boxX + 234;
    const int colSent = boxX + 296;
    const int colRecv = boxX + 348;

    auto cell = [&](const char *txt, int x, int y, SDL_Color c) {
        statsText.UpdateColor(c, {0, 0, 0, 0});
        statsText.UpdateText(rend, txt, 0);
        statsText.UpdatePosition({x, y});
        if (statsText.Texture()) {
            SDL_FRect fr = ToFRect(*statsText.Coords());
            SDL_RenderTexture(rend, statsText.Texture(), nullptr, &fr);
        }
    };

    const SDL_Color hdr = {255, 255, 100, 255};
    const SDL_Color win = {120, 255, 120, 255};
    const SDL_Color normal = {235, 235, 235, 255};

    int y = boxY + 3;
    // roundsPlayed is incremented by FinalizeRoundStats (called earlier this
    // frame), so it already counts the round whose stats are shown here.
    char roundHdr[32];
    snprintf(roundHdr, sizeof(roundHdr), "ROUND %d STATS", roundsPlayed > 0 ? roundsPlayed : 1);
    cell(roundHdr, colName, y, hdr);
    y += headH;
    cell("Player", colName, y, hdr);
    cell("Fire", colFired, y, hdr);
    cell("Pop", colPopped, y, hdr);
    cell("Atk", colSent, y, hdr);
    cell("Def", colRecv, y, hdr);
    y += rowH;

    char buf[32];
    for (int i = 0; i < n; i++) {
        BubbleArray &p = bubbleArrays[i];
        SDL_Color c = p.mpWinner ? win : normal;
        std::string name = StatsPlayerName(p, i, currentSettings.networkGame);
        if (name.size() > 14) name = name.substr(0, 14);
        cell(name.c_str(), colName, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rFired);  cell(buf, colFired, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rPopped); cell(buf, colPopped, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rSent);   cell(buf, colSent, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rRecv);   cell(buf, colRecv, y, c);
        y += rowH;
    }

    if (currentSettings.networkGame) {
        const char *hint = waitingForOpponentNewGame
            ? "T / X: CHAT    WAITING FOR PLAYERS"
            : (gameMatchOver
                ? "T / X: CHAT    ENTER: LOBBY"
                : "T / X: CHAT    ENTER / FIRE: NEXT ROUND");
        cell(hint, colName, y, hdr);

        // Tappable CHAT button (touch devices have no T key). Anchored under
        // the panel's left edge; HandleFinishedTap() hit-tests this rect.
        statsChatBtn = {boxX, boxY + boxH + 4, 88, 24};
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 0, 0, 0, 190);
        { SDL_FRect fr = ToFRect(statsChatBtn); SDL_RenderFillRect(rend, &fr); }
        SDL_SetRenderDrawColor(rend, 255, 255, 100, 200);
        { SDL_FRect fr = ToFRect(statsChatBtn); SDL_RenderRect(rend, &fr); }
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
        cell(chattingMode ? "SEND" : "CHAT", statsChatBtn.x + 24, statsChatBtn.y + 4, hdr);
    } else {
        statsChatBtn = {0, 0, 0, 0};
    }
}


void BubbleGame::Render() {
    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);
    SDL_RenderTexture(rend, background, nullptr, nullptr);

    // Process network messages if this is a network game
    if (currentSettings.networkGame) {
        ProcessNetworkMessages();

        // Send ping every second to prevent idle timeout (60 FPS = 60 frames/sec)
        // This matches the original Perl implementation which sends 'p' every second
        networkFrameCounter++;
        if (networkFrameCounter >= 60) {
            networkFrameCounter = 0;
            NetworkClient* netClient = NetworkClient::Instance();
            if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                netClient->SendGameData("p");
            }
        }

        // Check if both players are ready for new game after round ends
        if (waitingForOpponentNewGame && opponentReadyForNewGame) {
#ifdef __WASM_PORT__
            // WASM joiner: WaitForBubble inside ReloadGame->SyncNetworkLevel cannot yield,
            // so queue the 40 sync messages first (same fix as initial game start).
            NetworkClient* netClientRound = NetworkClient::Instance();
            if (netClientRound && !netClientRound->IsLeader()) {
                if (wasmRoundSyncWaitStart == 0) wasmRoundSyncWaitStart = SDL_GetTicks();
                size_t qSize = netClientRound->MessageQueueSize();
                bool timedOut = (SDL_GetTicks() - wasmRoundSyncWaitStart > 5000);
                SDL_Log("WASM round sync wait: queue=%d waited=%dms", (int)qSize, (int)(SDL_GetTicks() - wasmRoundSyncWaitStart));
                if (qSize < 40 && !timedOut) {
                    return;  // come back next frame
                }
                SDL_Log("WASM round sync: proceeding queue=%d timedOut=%d", (int)qSize, timedOut);
                wasmRoundSyncWaitStart = 0;
            }
#endif
            SDL_Log("All players ready - starting new game (detected in render loop)");
            if (chattingMode) FinishInGameChat(false);
            waitingForOpponentNewGame = false;
            opponentReadyForNewGame = false;
            opponentsReadyCount = 0;
            // In network games, check who won by looking at mpWinner flags
            // The player who cleared their board or whose opponent hit danger wins
            bool localPlayerWon = bubbleArrays[0].mpWinner;
            SDL_Log("Starting next round: localPlayerWon=%d", localPlayerWon);
            if (localPlayerWon) {
                ReloadGame(++curLevel);
            } else {
                // Opponent won or we lost - replay same level
                ReloadGame(curLevel);
            }
        }

        // Increment global frame counter (used by malus timing)
        frameCount++;
        // NOTE: ProcessMalusQueue for local player is called inside the stick handler
        // (after bubble sticks), matching original Perl behavior at line 2217-2258.
        // This ensures malus only falls AFTER the local player fires and their bubble sticks.
    }

    // NOTE: Local multiplayer and mp_train malus queues are processed at stick time (inside the
    // stick handlers), matching original Perl behavior where malus only falls after the recipient
    // sticks their bubble. mp_train is folded in here (instead of its own per-frame block) so
    // frameCount still increments once per frame for it without a separate unconditional
    // ProcessMalusQueue call contradicting "malus only falls after the shot sticks".
    if ((!currentSettings.networkGame && currentSettings.playerCount >= 2 && !gameFinish) ||
        (currentSettings.mpTraining && !gameFinish)) {
        frameCount++;
    }

    // Multiplayer training mode: periodically inject random malus, enforce 2-min timer
    if (currentSettings.mpTraining && !gameFinish) {
        if (mpTrainStartTime == 0) mpTrainStartTime = SDL_GetTicks();
        Uint32 elapsed = SDL_GetTicks() - mpTrainStartTime;
        const Uint32 TRAIN_DURATION = 120 * 1000;  // 2 minutes in ms

        if (!mpTrainDone && elapsed >= TRAIN_DURATION) {
            // Time's up — show score and end
            mpTrainDone = true;
            gameFinish = true;
            gameWon = true;
            // Store training score as level=101 (sentinel for mp_train) with time=score
            if (HighscoreManager::Instance()->CheckAndAddScore(mpTrainScore, 0.0f))
                pendingHighscore = true;
        } else if (!mpTrainDone) {
            // Randomly inject malus rows (original: rand($mptrainingdiff*(1000/$TARGET_ANIM_SPEED)) == 0)
            // mptrainingdiff default = 30 seconds between attacks; at 60fps: 30*60=1800 frames avg
            BubbleArray &arr = bubbleArrays[0];
            if (arr.malusQueue.empty()) {
                int roll = rand() % 1800;
                if (roll == 0) {
                    int count = 1 + rand() % 6;
                    for (int i = 0; i < count; i++)
                        arr.malusQueue.push_back(frameCount);
                }
            }
        }
    }

    if(playedPause) {
        audMixer->PauseMusic(true);
        playedPause = false;
        FrozenBubble::Instance()->startTime += SDL_GetTicks() - timePaused;
    }

    // Roll up per-round stats once when a multiplayer round ends (also broadcasts 'S').
    if (gameFinish && !roundStatsFinalized && currentSettings.playerCount >= 2) {
        FinalizeRoundStats();
        roundStatsFinalized = true;
    }

    if(currentSettings.playerCount == 1) {
        BubbleArray &curArray = bubbleArrays[0];

        SDL_Rect rct;
        for (int i = 1; i < 10; i++) {
            rct.x = curArray.rightLimit;
            rct.y = 104 - (7 * i) - i;
            rct.w = rct.h = 7;
            { SDL_FRect fr = ToFRect(rct); SDL_RenderTexture(rend, dotTexture[i == curArray.turnsToCompress ? 1 : 0], nullptr, &fr); }
        }
        for (int i = 0; i < curArray.numSeparators; i++) {
            rct.x = SCREEN_CENTER_X - 95;
            rct.y = (28 * i);
            rct.w = 188;
            rct.h = 28;
            { SDL_FRect fr = ToFRect(rct); SDL_RenderTexture(rend, sepCompressorTexture, nullptr, &fr); }
        }
        { SDL_FRect fr = ToFRect(curArray.compressorRct); SDL_RenderTexture(rend, compressorTexture, nullptr, &fr); }

        if (curArray.turnsToCompress <= 2) {
            DoPrelightAnimation(curArray, curArray.prelightTime);
        }
        SDL_Texture** useBubbles = GetBubbleTextures();
        for (const std::vector<Bubble> &vecBubble : curArray.bubbleMap) for (Bubble bubble : vecBubble) bubble.Render(rend, useBubbles, imgBubblePrelight, imgBubbleFrozen);

        // Stick effect animation (original: $sticking_bubble / sticking_step)
        if (curArray.stickAnimActive) {
            SDL_Rect sr = {curArray.stickAnimPos.x - 16, curArray.stickAnimPos.y - 16, 32, 32};
            { SDL_FRect fr = ToFRect(sr); SDL_RenderTexture(rend, imgBubbleStick[curArray.stickAnimFrame], nullptr, &fr); }
            if (++curArray.stickAnimSlowdown >= 2) {
                curArray.stickAnimSlowdown = 0;
                if (++curArray.stickAnimFrame > BUBBLE_STICKFC) curArray.stickAnimActive = false;
            }
        }

        if(gameFinish) {
            if (!gameWon && !gameLost) DoFrozenAnimation(curArray, curArray.frozenWait);

            if (gameLost) {
                { SDL_FRect fr = ToFRect(panelRct); SDL_RenderTexture(rend, soloStatePanels[0], nullptr, &fr); }
                // Show final score on lose screen
                char finalScore[64];
                snprintf(finalScore, sizeof(finalScore), "Final Score: %d", curArray.score);
                finalScoreText.UpdateText(renderer, finalScore, 0);
                finalScoreText.UpdatePosition({SCREEN_CENTER_X - (finalScoreText.Coords()->w / 2), panelRct.y + panelRct.h - 40});
                { SDL_FRect fr = ToFRect(*finalScoreText.Coords()); SDL_RenderTexture(rend, finalScoreText.Texture(), nullptr, &fr); }
            }
            else if (gameWon) {
                { SDL_FRect fr = ToFRect(panelRct); SDL_RenderTexture(rend, soloStatePanels[1], nullptr, &fr); }
                // Show final score on win screen (training shows mp_train score, normal shows bubble score)
                char finalScore[64];
                if (currentSettings.mpTraining)
                    snprintf(finalScore, sizeof(finalScore), "Training Score: %d", mpTrainScore);
                else
                    snprintf(finalScore, sizeof(finalScore), "Final Score: %d", curArray.score);
                finalScoreText.UpdateText(renderer, finalScore, 0);
                finalScoreText.UpdatePosition({SCREEN_CENTER_X - (finalScoreText.Coords()->w / 2), panelRct.y + panelRct.h - 40});
                { SDL_FRect fr = ToFRect(*finalScoreText.Coords()); SDL_RenderTexture(rend, finalScoreText.Texture(), nullptr, &fr); }
            }
        }

        if(singleBubbles.size() > 0) {
            UpdateSingleBubbles(0);
            for (SingleBubble &bubble : singleBubbles) bubble.Render(rend, useBubbles);
        }

        // Render malus bubbles in mp_training mode
        if (currentSettings.mpTraining) {
            for (MalusBubble &malus : malusBubbles) {
                malus.Render(rend, useBubbles, false);
            }
        }

        { SDL_FRect fr = ToFRect(curArray.curLaunchRct); SDL_RenderTexture(rend, gameFinish && !gameWon ? imgBubbleFrozen : useBubbles[curArray.curLaunch], nullptr, &fr); }
        { SDL_FRect fr = ToFRect(curArray.nextBubbleRct); SDL_RenderTexture(rend, useBubbles[curArray.nextBubble], nullptr, &fr); }
        { SDL_FRect fr = ToFRect(curArray.onTopRct); SDL_RenderTexture(rend, onTopTexture, nullptr, &fr); }
        if (gameFinish && !gameWon) { SDL_FRect fr = ToFRect(curArray.frozenBottomRct); SDL_RenderTexture(rend, imgBubbleFrozen, nullptr, &fr); }

        UpdatePenguin(curArray);
        if(!lowGfx) curArray.penguinSprite.Render();
        curArray.shooterSprite.Render(lowGfx);
        // Redraw the current bubble on top of the shooter/cannon sprite -- the
        // cannon graphic is large enough to cover most of it, otherwise making
        // the loaded bubble's color hard to see while aiming.
        { SDL_FRect fr = ToFRect(curArray.curLaunchRct); SDL_RenderTexture(rend, gameFinish && !gameWon ? imgBubbleFrozen : useBubbles[curArray.curLaunch], nullptr, &fr); }
        if (curArray.aimGuideEnabled && !gameFinish) {
            bool isMini = (currentSettings.playerCount >= 3 && curArray.playerAssigned >= 1);
            DrawAimGuide(rend, curArray, isMini);
        }
        { SDL_FRect fr = ToFRect(*inGameText.Coords()); SDL_RenderTexture(rend, inGameText.Texture(), nullptr, &fr); }

        // Display score (UpdateScoreText now renders immediately)
        UpdateScoreText(curArray);

        // Multiplayer training: show countdown timer and training score
        if (currentSettings.mpTraining && mpTrainStartTime > 0) {
            const Uint32 TRAIN_DURATION = 120 * 1000;
            Uint32 elapsed = SDL_GetTicks() - mpTrainStartTime;
            int remaining = (elapsed < TRAIN_DURATION) ? (int)((TRAIN_DURATION - elapsed) / 1000) : 0;
            int m = remaining / 60;
            int s = remaining % 60;
            char trainBuf[64];
            snprintf(trainBuf, sizeof(trainBuf), "%d'%02d\"  Score: %d", m, s, mpTrainScore);
            mpTrainText.UpdateText(renderer, trainBuf, 0);
            mpTrainText.UpdatePosition({32, 177});
            { SDL_FRect fr = ToFRect(*mpTrainText.Coords()); SDL_RenderTexture(rend, mpTrainText.Texture(), nullptr, &fr); }
        }

        // Display combo text if timer is active
        if (comboDisplayTimer > 0) {
            { SDL_FRect fr = ToFRect(*comboText.Coords()); SDL_RenderTexture(rend, comboText.Texture(), nullptr, &fr); }
            comboDisplayTimer--;
        }
    }
    else { //iterate until all penguins & status are rendered
        // Update ALL players' bubbles ONCE before rendering (original: iter_players at line 2105)
        // This ensures all players are processed in a single unified loop
        UpdateSingleBubbles(0);  // id parameter ignored now - processes all players

        for (int i = 0; i < currentSettings.playerCount; i++) {
            BubbleArray &curArray = bubbleArrays[i];

            SDL_Rect rct;
            for (int i = 1; i < 13; i++) {
                rct.x = curArray.rightLimit;
                rct.y = 104 - (7 * i) - i;
                rct.w = rct.h = 7;
                { SDL_FRect fr = ToFRect(rct); SDL_RenderTexture(rend, dotTexture[i == curArray.turnsToCompress ? 1 : 0], nullptr, &fr); }
            }

            // Use mini textures for remote players (playerAssigned >= 1) in 3-5 player games
            bool useMini = (currentSettings.playerCount >= 3 && curArray.playerAssigned >= 1);
            SDL_Texture** useBubbles = GetBubbleTextures(useMini);
            SDL_Texture* useFrozen = useMini ? imgMiniBubbleFrozen : imgBubbleFrozen;
            SDL_Texture* usePrelight = useMini ? imgMiniBubblePrelight : imgBubblePrelight;

            // Don't render shooter bubbles for LOST players (prevents crashes from invalid bubble indices)
            // In network games, losing players become spectators and shouldn't have active bubbles
            if (curArray.playerState != BubbleArray::PlayerState::LOST) {
                { SDL_FRect fr = ToFRect(curArray.curLaunchRct); SDL_RenderTexture(rend, gameFinish && !curArray.mpWinner ? useFrozen : useBubbles[curArray.curLaunch], nullptr, &fr); }
                { SDL_FRect fr = ToFRect(curArray.nextBubbleRct); SDL_RenderTexture(rend, useBubbles[curArray.nextBubble], nullptr, &fr); }
                { SDL_FRect fr = ToFRect(curArray.onTopRct); SDL_RenderTexture(rend, onTopTexture, nullptr, &fr); }
            }
            if ((gameFinish && !curArray.mpWinner) || curArray.playerState == BubbleArray::PlayerState::LOST) { SDL_FRect fr = ToFRect(curArray.frozenBottomRct); SDL_RenderTexture(rend, useFrozen, nullptr, &fr); }

            if (curArray.turnsToCompress <= 2) {
                DoPrelightAnimation(curArray, curArray.prelightTime);
            }
            for (const std::vector<Bubble> &vecBubble : curArray.bubbleMap) for (Bubble bubble : vecBubble) bubble.Render(rend, useBubbles, usePrelight, useFrozen);

            // Stick effect animation (original: $sticking_bubble / sticking_step)
            if (curArray.stickAnimActive) {
                SDL_Texture* stickTex = useMini ? imgMiniBubbleStick[curArray.stickAnimFrame] : imgBubbleStick[curArray.stickAnimFrame];
                int sz = useMini ? 16 : 32;
                SDL_Rect sr = {curArray.stickAnimPos.x - sz/2, curArray.stickAnimPos.y - sz/2, sz, sz};
                { SDL_FRect fr = ToFRect(sr); SDL_RenderTexture(rend, stickTex, nullptr, &fr); }
                if (++curArray.stickAnimSlowdown >= 2) {
                    curArray.stickAnimSlowdown = 0;
                    if (++curArray.stickAnimFrame > BUBBLE_STICKFC) curArray.stickAnimActive = false;
                }
            }

            if(gameFinish) {
                if (!curArray.mpWinner) DoFrozenAnimation(curArray, curArray.frozenWait);
                else {
                    DoWinAnimation(curArray, curArray.explodeWait);
                    idxMPWinner = i;
                }
            } else if (curArray.playerState == BubbleArray::PlayerState::LOST) {
                // Player died mid-round while others are still playing (3-5 player games):
                // progressively freeze their board so the death is visually indicated,
                // matching the original Perl's update_lost() (frozen-bubble line 2007/2106).
                DoFrozenAnimation(curArray, curArray.frozenWait);
            }

            UpdatePenguin(curArray);
            if(!lowGfx) curArray.penguinSprite.Render();
            curArray.shooterSprite.Render(lowGfx);
            // Redraw the current bubble on top of the shooter/cannon sprite -- the
            // cannon graphic is large enough to cover most of it, otherwise making
            // the loaded bubble's color hard to see while aiming.
            if (curArray.playerState != BubbleArray::PlayerState::LOST) {
                SDL_FRect fr = ToFRect(curArray.curLaunchRct);
                SDL_RenderTexture(rend, gameFinish && !curArray.mpWinner ? useFrozen : useBubbles[curArray.curLaunch], nullptr, &fr);
            }
            if (curArray.aimGuideEnabled && !gameFinish &&
                curArray.playerState == BubbleArray::PlayerState::ALIVE) {
                bool isMini = (currentSettings.playerCount >= 3 && curArray.playerAssigned >= 1);
                DrawAimGuide(rend, curArray, isMini);
            }

            // NOTE: UpdateSingleBubbles is now called ONCE before the loop (line 2416)
            // Don't call it here per-player anymore

            // Display score with nickname for each player (original: print_scores at line 1868)
            // In 3+ player games, skip score text — win counts are shown via UpdatePlayerNameWinText
            // at the same screen positions, so rendering both would cause overlapping text.
            if (currentSettings.playerCount < 3) {
                UpdateScoreText(curArray);
            }

            // Display "left" overlay for players who actually disconnected (original line 1951-1955)
            // NOTE: LOST = died (still in game), LEFT = disconnected. Only show for LEFT.
            if (currentSettings.networkGame && curArray.playerAssigned >= 1 &&
                curArray.playerState == BubbleArray::PlayerState::LEFT) {
                // Determine which texture and position to use based on player and mini graphics
                SDL_Texture* leftTexture = nullptr;
                SDL_Rect leftRect = {0, 0, 0, 0};

                bool isMini = (currentSettings.playerCount >= 3);
                if (isMini) {
                    // Mini left overlays for 3-5 player games
                    if (curArray.playerAssigned == 1) {
                        leftTexture = leftRp1Mini;
                        leftRect = {20, 19, 128, 173};  // rp1 position
                    } else if (curArray.playerAssigned == 2) {
                        leftTexture = leftRp2Mini;
                        leftRect = {492, 19, 128, 173};  // rp2 position
                    } else if (curArray.playerAssigned == 3) {
                        leftTexture = leftRp3Mini;
                        leftRect = {20, 287, 128, 173};  // rp3 position
                    } else if (curArray.playerAssigned == 4) {
                        leftTexture = leftRp4Mini;
                        leftRect = {492, 287, 128, 173};  // rp4 position
                    }
                } else {
                    // Full size left overlay for 2-player game
                    leftTexture = leftRp1;
                    leftRect = {320, 0, 320, 480};  // rp1 position (right side)
                }

                if (leftTexture) {
                    { SDL_FRect fr = ToFRect(leftRect); SDL_RenderTexture(rend, leftTexture, nullptr, &fr); }
                }
            }

            // Render targeting attack indicator on the targeted opponent's board
            // (original: put_image_to_background($imgbin{attack}{...}) in set_sendmalustoone at line 1338)
            // Attack positions from Stuff.pm POS_MP: rp1={25,213}, rp2={496,214}, rp3={24,442}, rp4={496,442}
            if (currentSettings.singlePlayerTargetting && sendMalusToOne == i &&
                curArray.playerAssigned >= 1 && curArray.playerAssigned <= 4) {
                static const SDL_Point attackPos[4] = {{25, 213}, {496, 214}, {24, 442}, {496, 442}};
                int rpIdx = curArray.playerAssigned - 1;
                if (imgAttack[rpIdx]) {
                    SDL_Rect attackRct;
                    { float fw, fh; SDL_GetTextureSize(imgAttack[rpIdx], &fw, &fh); attackRct.w = (int)fw; attackRct.h = (int)fh; }
                    attackRct.x = attackPos[rpIdx].x;
                    attackRct.y = attackPos[rpIdx].y;
                    { SDL_FRect fr = ToFRect(attackRct); SDL_RenderTexture(rend, imgAttack[rpIdx], nullptr, &fr); }
                }
            }

            // Show targeting text: who this player is targeting
            if (currentSettings.singlePlayerTargetting && !gameFinish &&
                playerTargeting[i] >= 0 && playerTargeting[i] < currentSettings.playerCount) {
                const std::string& targetNick = bubbleArrays[playerTargeting[i]].playerNickname;
                if (!targetNick.empty()) {
                    char tgtBuf[64];
                    snprintf(tgtBuf, sizeof(tgtBuf), "> %s", targetNick.c_str());
                    targetingText.UpdateText(rend, tgtBuf, 0);
                    // Position: near each player's shooter area
                    int tx, ty;
                    if (curArray.playerAssigned == 0) {
                        tx = curArray.shooterSprite.rect.x + curArray.shooterSprite.rect.w / 2 - 30;
                        ty = curArray.shooterSprite.rect.y - 20;
                    } else {
                        tx = curArray.shooterSprite.rect.x;
                        ty = curArray.shooterSprite.rect.y + curArray.shooterSprite.rect.h;
                    }
                    targetingText.UpdatePosition({tx, ty});
                    { SDL_FRect fr = ToFRect(*targetingText.Coords()); SDL_RenderTexture(rend, targetingText.Texture(), nullptr, &fr); }
                }
            }
        }

        // Render "attackme" indicators on local player board when opponents are targeting us
        // (original: redraw_attackingme() at line 1345)
        // attackme position from Stuff.pm: p1 attackme={185, 448}, each attacker offset by 24px
        if (currentSettings.singlePlayerTargetting && !attackingMe.empty() && !gameFinish) {
            for (size_t k = 0; k < attackingMe.size(); k++) {
                int attackerArray = attackingMe[k];
                if (attackerArray >= 1 && attackerArray <= 4) {
                    int rpIdx = attackerArray - 1;
                    if (imgAttackMe[rpIdx]) {
                        SDL_Rect amRct;
                        { float fw, fh; SDL_GetTextureSize(imgAttackMe[rpIdx], &fw, &fh); amRct.w = (int)fw; amRct.h = (int)fh; }
                        amRct.x = 185 + ((int)k * 24);
                        amRct.y = 448;
                        { SDL_FRect fr = ToFRect(amRct); SDL_RenderTexture(rend, imgAttackMe[rpIdx], nullptr, &fr); }
                    }
                }
            }
        }

        // Check all players for danger zone every frame in multiplayer (original: verify_if_end() at line 2319)
        // Original checks: if ($pdata{state} eq 'game' && any { $_->{cy} > 11 })
        // Only check while global game state is "game" (not finished/won).
        // This sweep must run for local multiplayer too, not just network games: malus sticks
        // deliberately don't call CheckGameState (to avoid double-counting the compressor/new-root
        // counter), so this every-frame sweep is the only way to catch a local player pushed into
        // the danger zone by incoming malus between their own shots.
        if (!gameFinish && currentSettings.playerCount >= 2) {
            static int checkCounter = 0;
            checkCounter++;
            for (int i = 0; i < currentSettings.playerCount; i++) {
                BubbleArray &checkArray = bubbleArrays[i];
                // Check if player is alive AND has bubbles in danger zone (cy > 11 means row 12+)
                bool inDanger = checkArray.bubbleOnDanger();
                bool isAlive = (checkArray.playerState == BubbleArray::PlayerState::ALIVE);

                // Log every 60 frames (once per second at 60fps) for debugging
                if (checkCounter % 60 == 0 && i < 3) {
                    SDL_Log("Player %d: alive=%d, inDanger=%d, lobbyId=%d",
                            i, isAlive, inDanger, checkArray.lobbyPlayerId);
                }

                if (isAlive && inDanger) {
                    SDL_Log("!!! Player %d hit danger zone!", i);
                    // In network games only the local player's HUD (index 0) reflects here; in
                    // local multiplayer every board is local so each losing player's HUD updates.
                    if (!currentSettings.networkGame || i == 0) {
                        panelRct = {SCREEN_CENTER_X - 173, 480 - 248, 345, 124};
                        checkArray.curLaunchRct = {checkArray.curLaunchRct.x - 1, checkArray.curLaunchRct.y - 1, 34, 48};
                    }
                    checkArray.penguinSprite.PlayAnimation(11);
                    HandlePlayerLoss(checkArray);
                }
            }
        }

        if (gameFinish) {
            if (currentSettings.playerCount == 2) {
                if (gameMpDone) { SDL_FRect fr = ToFRect(panelRct); SDL_RenderTexture(rend, multiStatePanels[idxMPWinner], nullptr, &fr); }
                else {
                    for (int i = 0; i < currentSettings.playerCount; i++) {
                        if (bubbleArrays[i].mpDone == false) {
                            gameMpDone = false;
                            break;
                        } else gameMpDone = true;
                    }
                }
            }
        }

        if(singleBubbles.size() > 0) {
            SDL_Texture** useBubbles = GetBubbleTextures();
            for (SingleBubble &bubble : singleBubbles) bubble.Render(rend, useBubbles);
        }

        // Render malus bubbles (attack bubbles)
        if(malusBubbles.size() > 0) {
            for (MalusBubble &malus : malusBubbles) {
                // Determine if this malus bubble belongs to a mini player
                // In 3+ player games: array 0 (center) is full size, arrays 1+ are mini
                bool useMini = (currentSettings.playerCount >= 3 && malus.assignedArray >= 1);
                SDL_Texture** bubbles = GetBubbleTextures(useMini);
                malus.Render(rend, bubbles, useMini);
            }
        }

        // Render win counters and player names
        if (currentSettings.playerCount == 2) {
            // 2-player mode: show simple win counters
            { SDL_FRect fr = ToFRect(*winsP1Text.Coords()); SDL_RenderTexture(rend, winsP1Text.Texture(), nullptr, &fr); }
            { SDL_FRect fr = ToFRect(*winsP2Text.Coords()); SDL_RenderTexture(rend, winsP2Text.Texture(), nullptr, &fr); }
        } else if (currentSettings.playerCount >= 3) {
            // Update names every frame to pick up nicknames as they become available
            UpdatePlayerNameWinText();

            // 3-5 player mode: show player name and win count
            for (int i = 0; i < currentSettings.playerCount; i++) {
                if (playerNameWinText[i].Texture()) {
                    { SDL_FRect fr = ToFRect(*playerNameWinText[i].Coords()); SDL_RenderTexture(rend, playerNameWinText[i].Texture(), nullptr, &fr); }
                }
            }
        }

        // Incoming-malus toasts ("who hit you and how many"); fade out during play.
        if (!gameFinish) RenderMalusAlerts(rend);

        // Post-round stats table (multiplayer): shown while the round-end screen is up.
        if (gameFinish) RenderRoundStats(rend);
    }

    // In-game chat overlay (network games only)
    if (currentSettings.networkGame) {
        if (!inGameChatMessages.empty() || chattingMode) {
            const int lineH   = 18;
            const int maxShow = 3;
            const int chatX   = 5;
            // Base Y: bottom of screen with room for maxShow lines
            const int baseY   = 480 - maxShow * lineH - 2;

            // Semi-transparent dark background
            int bgY = baseY - 2;
            int bgH = maxShow * lineH + 4;
            if (chattingMode) { bgY -= lineH + 4; bgH += lineH + 4; }
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            SDL_Rect chatBg = {0, bgY, 640, bgH};
            SDL_SetRenderDrawColor(rend, 0, 0, 0, 170);
            { SDL_FRect fr = ToFRect(chatBg); SDL_RenderFillRect(rend, &fr); }
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);

            // Draw input line (yellow) above the message area
            if (chattingMode) {
                char inputLine[512];
                snprintf(inputLine, sizeof(inputLine), "Say: %s_", chatInputBuf);
                chatInputText.UpdateText(rend, inputLine, 630);
                chatInputText.UpdatePosition({chatX, bgY + 2});
                if (chatInputText.Texture())
                    { SDL_FRect fr = ToFRect(*chatInputText.Coords()); SDL_RenderTexture(rend, chatInputText.Texture(), nullptr, &fr); }
            }

            // Draw last maxShow messages (white)
            int count = (int)inGameChatMessages.size();
            int start = count > maxShow ? count - maxShow : 0;
            for (int i = start; i < count; i++) {
                char lineBuf[512];
                snprintf(lineBuf, sizeof(lineBuf), "%s: %s",
                         inGameChatMessages[i].nick.c_str(),
                         inGameChatMessages[i].text.c_str());
                chatLineText.UpdateText(rend, lineBuf, 630);
                chatLineText.UpdatePosition({chatX, baseY + (i - start) * lineH});
                if (chatLineText.Texture())
                    { SDL_FRect fr = ToFRect(*chatLineText.Coords()); SDL_RenderTexture(rend, chatLineText.Texture(), nullptr, &fr); }
            }
        }

        // Age and prune expired messages so the overlay auto-hides instead of
        // lingering until 10 more messages are typed (mirrors RenderMalusAlerts).
        for (auto &msg : inGameChatMessages) msg.framesLeft--;
        inGameChatMessages.erase(
            std::remove_if(inGameChatMessages.begin(), inGameChatMessages.end(),
                           [](const InGameChatMsg &m) { return m.framesLeft <= 0; }),
            inGameChatMessages.end());
    }

    if (!firstRenderDone) {
        TransitionManager::Instance()->TakeSnipOut(rend);
        firstRenderDone = true;
    }
}


void BubbleGame::RenderPaused() {
    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);

    if(!playedPause) {
        audMixer->PauseMusic();
        audMixer->PlaySFX("pause");
        playedPause = true;
        pauseFrame = 0;

        if(prePauseBackground != nullptr) SDL_DestroyTexture(prePauseBackground);

        SDL_Surface *sfc = SDL_RenderReadPixels(rend, NULL);
        prePauseBackground = SDL_CreateTextureFromSurface(rend, sfc);
        SDL_DestroySurface(sfc);
    }

    SDL_RenderClear(rend);
    SDL_RenderTexture(rend, prePauseBackground, nullptr, nullptr);
    SDL_RenderTexture(rend, pauseBackground, nullptr, nullptr);

    if (nextPauseUpd <= 0){
        pauseFrame++;
        nextPauseUpd = 2;
        if(pauseFrame >= 34) {
            pauseFrame = 12;
        }
    }
    else nextPauseUpd--;

    SDL_Rect pauseRct = {SCREEN_CENTER_X - 95, SCREEN_CENTER_Y - 72, 190, 143};
    { SDL_FRect fr = ToFRect(pauseRct); SDL_RenderTexture(rend, pausePenguin[pauseFrame], nullptr, &fr); }

    timePaused = SDL_GetTicks();
}

