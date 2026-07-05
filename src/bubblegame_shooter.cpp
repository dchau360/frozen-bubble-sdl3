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

void BubbleGame::LaunchBubble(BubbleArray &bArray) {
    audMixer->PlaySFX("launch");
    SDL_Log("Launching bubble at angle: %.4f radians (%.2f degrees from center), lowGfx=%d, cos=%.4f",
            bArray.shooterSprite.angle,
            (bArray.shooterSprite.angle - PI/2.0f) * 180.0f / PI,
            lowGfx,
            cosf(bArray.shooterSprite.angle));
    // Use curLaunchRct for launch position (set per-player in NewGame/ReloadGame)
    float startX = (float)bArray.curLaunchRct.x;
    float startY = (float)bArray.curLaunchRct.y;
    bool isMiniLaunch = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
    singleBubbles.push_back({bArray.playerAssigned, bArray.curLaunch, startX, startY, startX, startY, {(int)startX, (int)startY}, {}, bArray.shooterSprite.angle, false, true, bArray.leftLimit, bArray.rightLimit, bArray.topLimit, lowGfx, isMiniLaunch ? 16 : 32});
    PickNextBubble(bArray);
    FrozenBubble::Instance()->totalBubbles++;
    // Stats: count shots for locally-owned arrays only (remote arrays sync via 'S').
    if (!currentSettings.networkGame || bArray.playerAssigned == 0) bArray.rFired++;
    bArray.hurryTimer = 0;
    bArray.chainLevel = 0; // Reset chain level for new shot

    // Send shot to network if this is a network game AND this is the local player
    // Don't send if this is a remote player's shot (we're just replicating their fire from mpFirePending)
    if (currentSettings.networkGame && bArray.playerAssigned == 0) {
        SendNetworkBubbleShot(bArray);
    }
}

void BubbleGame::UpdatePenguin(BubbleArray &bArray) {
    if (gameFinish) return;

    // In network games, only process keyboard input for local player (array 0)
    // Remote player's actions (array 1) come from network messages (mpFirePending flag)
    // But we still need to process the fire logic for all players (original: iter_players at line 2105)
    // Original checks mp_fire for ALL players, not just local (line 2141)
    // In local multiplayer, ALL players are local (each uses their own controller)
    bool isLocalPlayer = (bArray.playerAssigned == 0) || !currentSettings.networkGame;

    // Process keyboard input only for local players (original: is_local_player($::p))
    if (isLocalPlayer) {
        // Use configured keys from settings
        // Don't accept input if player has lost or game is finished (except local player 0 in finished state)
        // Original: checks if $pdata{state} eq 'game'
        bool acceptInput = (bArray.playerState == BubbleArray::PlayerState::ALIVE)
            && !(chattingMode && currentSettings.networkGame && bArray.playerAssigned == 0);
        if (!acceptInput && bArray.playerAssigned == 0 && gameFinish) {
            // Allow local player to continue for a bit during finish sequence
            acceptInput = false;
        }

        GameSettings* gs = GameSettings::Instance();
        PlayerKeys* allPlayerKeys[5] = {
            &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
            &gs->player4Keys, &gs->player5Keys
        };
        int pIdx = (bArray.playerAssigned >= 0 && bArray.playerAssigned < 5) ? bArray.playerAssigned : 0;
        PlayerKeys& keys = *allPlayerKeys[pIdx];

        if (acceptInput) {
            if (currentSettings.localMultiplayer && bArray.playerAssigned >= 0 && bArray.playerAssigned < 5) {
                // Local multiplayer: keyboard first (always works, matches 1P path),
                // then OR with controller DPAD for real gamepads.
                // Note: on Android TV, opening SDL_Gamepad can suppress d-pad
                // keyboard events, so keyboard must be read unconditionally first.
                int idx = bArray.playerAssigned;
                // IsKeyPressed() handles real keyboard and virtual controller scancodes.
                // Also OR with controllerInputs[] which frozenbubble.cpp writes directly
                // (SDL_GetKeyboardState ignores synthetic SDL_PushEvent KEYDOWN events).
                bArray.shooterLeft   = IsKeyPressed(keys.left)   || controllerInputs[idx].left;
                bArray.shooterRight  = IsKeyPressed(keys.right)  || controllerInputs[idx].right;
                bArray.shooterCenter = IsKeyPressed(keys.center) || controllerInputs[idx].center;
                bArray.shooterAction = IsKeyPressed(keys.fire)   || controllerInputs[idx].fire;
                if (bArray.suppressFireUntilRelease) {
                    if (!IsKeyPressed(keys.fire) && !controllerInputs[idx].fire) bArray.suppressFireUntilRelease = false;
                    else bArray.shooterAction = false;
                }
                if (idx < numControllersOpen && controllers[idx]) {
                    SDL_Gamepad* ctrl = controllers[idx];
                    bArray.shooterLeft   = bArray.shooterLeft   || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_DPAD_LEFT)  != 0;
                    bArray.shooterRight  = bArray.shooterRight  || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) != 0;
                    bArray.shooterCenter = bArray.shooterCenter || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_DPAD_UP)    != 0;
                    bArray.shooterAction = bArray.shooterAction || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_SOUTH)      != 0
                                        || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_DPAD_UP)                            != 0;
                }
            } else if (bArray.playerAssigned == 0) {
                bArray.shooterAction = IsKeyPressed(keys.fire);
                if (bArray.suppressFireUntilRelease) {
                    if (!IsKeyPressed(keys.fire)) bArray.suppressFireUntilRelease = false;
                    else bArray.shooterAction = false;
                }
                bArray.shooterLeft   = IsKeyPressed(keys.left);
                bArray.shooterRight  = IsKeyPressed(keys.right);
                bArray.shooterCenter = IsKeyPressed(keys.center);
            }
            else if (bArray.playerAssigned == 1) {
                bArray.shooterAction = IsKeyPressed(keys.fire);
                if (bArray.suppressFireUntilRelease) {
                    if (!IsKeyPressed(keys.fire)) bArray.suppressFireUntilRelease = false;
                    else bArray.shooterAction = false;
                }
                bArray.shooterLeft   = IsKeyPressed(keys.left);
                bArray.shooterRight  = IsKeyPressed(keys.right);
                bArray.shooterCenter = IsKeyPressed(keys.center);
            }
        } else {
            // Player is dead or game finished - clear all shooter flags
            bArray.shooterAction = false;
            bArray.shooterLeft = false;
            bArray.shooterRight = false;
            bArray.shooterCenter = false;
        }
    }

    // Hurry timer and warnings only for local, still-alive players (remote players have their
    // own timers; a dead player's board is frozen and must not accumulate hurry time or force-fire
    // — original gates the whole per-player update on state == 'ingame', bin/frozen-bubble ~2106).
    if (isLocalPlayer && bArray.playerState == BubbleArray::PlayerState::ALIVE) {
        // Only the classic numbered single-player campaign gets the longer hurry timer
        // (original ~line 3330-3332); random 1P levels and mp_train use the shorter default,
        // same as multiplayer (original ~line 3300-3302).
        bool isClassicCampaign = currentSettings.playerCount < 2 && !currentSettings.randomLevels
            && !currentSettings.mpTraining;
        if (isClassicCampaign) {
            if (bArray.hurryTimer >= TIME_HURRY_WARN) {
                if (bArray.warnTimer <= HURRY_WARN_FC / 2){
                    if(bArray.warnTimer == 0) audMixer->PlaySFX("hurry");
                    { SDL_FRect fr = ToFRect(bArray.hurryRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), bArray.hurryTexture, nullptr, &fr); }
                }
                bArray.warnTimer++;
                if (bArray.warnTimer > HURRY_WARN_FC) {
                    bArray.warnTimer = 0;
                }
                if (bArray.hurryTimer >= TIME_HURRY_MAX) {
                    bArray.shooterAction = true;
                }
            }
        }
        else {
            if (bArray.hurryTimer >= TIME_HURRY_WARN_MP) {
                if (bArray.warnTimer <= HURRY_WARN_MP_FC / 2){
                    if(bArray.warnTimer == 0) audMixer->PlaySFX("hurry");
                    { SDL_FRect fr = ToFRect(bArray.hurryRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), bArray.hurryTexture, nullptr, &fr); }
                }
                bArray.warnTimer++;
                if (bArray.warnTimer > HURRY_WARN_MP_FC) {
                    bArray.warnTimer = 0;
                }
                if (bArray.hurryTimer >= TIME_HURRY_MAX_MP) {
                    bArray.shooterAction = true;
                }
            }
        }
        bArray.hurryTimer++;
    }

    float &angle = bArray.shooterSprite.angle;
    Penguin &penguin = bArray.penguinSprite;

    // Mouse/touch aim: snap angle directly, before the fire check so the correct angle is used
    if (bArray.mouseTargetAngle >= 0.f) {
        angle = bArray.mouseTargetAngle;
        if (angle < 0.1f) angle = 0.1f;
        if (angle > (float)PI - 0.1f) angle = (float)PI - 0.1f;
    }
    // Mouse/touch fire: inject as shooterAction before the fire check
    if (bArray.mouseFirePending) {
        bArray.shooterAction = true;
        bArray.mouseFirePending = false;
    }

    // Check if we should fire: either local player action or remote player mp_fire flag (original line 2141)
    // Block local player from firing while malus bubbles are falling (original line 2146: !@{$malus_bubble{$::p}})
    bool localMalusInFlight = false;
    if (currentSettings.networkGame && bArray.playerAssigned == 0) {
        for (const auto& mb : malusBubbles)
            if (mb.assignedArray == 0 && !mb.shouldClear) { localMalusInFlight = true; break; }
    }

    // For remote players (mp_fire), fire immediately regardless of newShoot state
    // For local players, only fire if newShoot is true (no bubble currently in flight)
    if(bArray.mpFirePending || (!localMalusInFlight && bArray.shooterAction == true && bArray.newShoot == true)) {
        penguin.sleeping = 0;
        if(penguin.curAnimation != 1) penguin.PlayAnimation(1);

        // For remote players with mp_fire, use the angle from the network message
        if (bArray.mpFirePending) {
            angle = bArray.pendingAngle;
            SDL_Log("Launching remote player %d bubble with angle %.3f from network", bArray.playerAssigned, angle);
        }

        LaunchBubble(bArray);
        bArray.shooterAction = false;
        bArray.mpFirePending = false;  // Clear mp_fire flag (original line 2165)
        bArray.newShoot = false;
        // Require the fire key/button to be released before the next shot can fire.
        // newShoot flips back to true as soon as the launched bubble sticks, which can
        // happen almost instantly for short shots (e.g. sticking near the bottom rows).
        // Without this, a player still physically holding the fire key down would see
        // shooterAction == true && newShoot == true again next frame and fire an
        // unintended second shot with no new keypress at all.
        bArray.suppressFireUntilRelease = true;
        return;
    }

    if (angle < 0.1) angle = 0.1;
    if (angle > PI-0.1) angle = PI-0.1;

    if (bArray.shooterLeft || bArray.shooterRight || bArray.shooterCenter) {
        // Keyboard/controller aim takes over from mouse: clear the latched mouse angle so
        // it stops overriding `angle` every frame. Mouse aim re-activates on the next
        // mouse-move event (HandleMouseAim). Without this, once the mouse set mouseTargetAngle
        // the keyboard could never move the aim again until the game was reloaded.
        bArray.mouseTargetAngle = -1.f;
        float ds = FrozenBubble::Instance()->deltaScale;
        float launchStep = (float)LAUNCHER_SPEED * ds;
        if (bArray.shooterLeft) {
            angle += launchStep;  // Move LEFT = increase angle (toward π)
            if(penguin.curAnimation != 1 && (penguin.curAnimation > 7 || penguin.curAnimation < 2)) penguin.PlayAnimation(2);
        }
        else if (bArray.shooterRight) {
            angle -= launchStep;  // Move RIGHT = decrease angle (toward 0)
            if(penguin.curAnimation != 1 && (penguin.curAnimation > 7 || penguin.curAnimation < 2)) penguin.PlayAnimation(5);
        }
        else if (bArray.shooterCenter) {
            if (angle >= PI/2.0f - launchStep && angle <= PI/2.0f + launchStep) angle = PI/2.0f;
            else angle += (angle < PI/2.0f) ? launchStep : -launchStep;
        }

        penguin.sleeping = 0;
    }
    else if(!bArray.shooterLeft && !bArray.shooterRight && !bArray.shooterCenter) {
        penguin.sleeping++;
        if (penguin.sleeping > TIMEOUT_PENGUIN_SLEEP && (penguin.curAnimation > 9 || penguin.curAnimation < 8)) penguin.PlayAnimation(8);
    }

    if (!bArray.shooterLeft && penguin.curAnimation == 3) penguin.PlayAnimation(4);
    if (!bArray.shooterRight && penguin.curAnimation == 6) penguin.PlayAnimation(7);
}

// only called for a new game.
void BubbleGame::ChooseFirstBubble(BubbleArray *bArray) {
    // Original lines 3431-3456: next_num and tobe_num picked once from player 0's colors,
    // then ALL players get the same values.
    std::vector<int> p0Bubbles = bArray[0].remainingBubbles();
    int firstColor = p0Bubbles[ranrange(1, p0Bubbles.size()) - 1];
    int nextColor  = p0Bubbles[ranrange(1, p0Bubbles.size()) - 1];
    for (int i = 0; i < currentSettings.playerCount; i++) {
        bArray[i].curLaunch  = firstColor;
        bArray[i].nextBubble = nextColor;
    }
}

void BubbleGame::PickNextBubble(BubbleArray &bArray) {
    bArray.curLaunch = bArray.nextBubble;
    std::vector<int> currentBubbles = bArray.remainingBubbles();
    bArray.nextBubble = currentBubbles[ranrange(1, currentBubbles.size()) - 1];
    // Rotate nextColors queue: remove first (just used), append new random
    // Matches Perl: nextcolors is updated after each shot so all clients can compute future root rows
    if (!bArray.nextColors.empty()) bArray.nextColors.erase(bArray.nextColors.begin());
    bArray.nextColors.push_back(ranrange(0, bArray.numColors - 1));
}

// Build the space-separated nextcolors string for 's' messages (Perl: "@{$pdata{$::p}{nextcolors}}")
static std::string BuildNextColorsStr(const BubbleArray &bArray) {
    std::string s;
    for (int c : bArray.nextColors) {
        if (!s.empty()) s += ' ';
        s += std::to_string(c);
    }
    return s;
}


static bool IsGridAdjacent(int r1, int c1, int r2, int c2, int oddswap = 0) {
    for (auto [dr, dc] : GridNeighborOffsets(r1, oddswap))
        if (r1 + dr == r2 && c1 + dc == c2) return true;
    return false;
}

// anchorRow/anchorCol: grid position of the hit bubble (-1 if none, e.g. ceiling hit).
// When provided, the result is guaranteed to be a free cell adjacent to the anchor.
void GetClosestFreeCell(SingleBubble &sBubble, BubbleArray &bArray, int *row, int *col,
                        int anchorRow = -1, int anchorCol = -1, bool isMini = false) {
    // Original: get_array_closest_pos() at frozen-bubble lines 636-641
    // Uses MIDPOINT between old and new position (line 2208-2209)

    // Mini players use half bubble size for spacing (matches RandomLevel,
    // GetClosestFreeCell, and AssignChainReactions' isMini handling).
    // Original: local $BUBBLE_SIZE = $BUBBLE_SIZE / 2; local $ROW_SIZE = $ROW_SIZE / 2;
    const int BUBBLE_SIZE = isMini ? 16 : 32;  // Original: line 91
    const int ROW_SIZE = BUBBLE_SIZE * 7 / 8;  // Original: BUBBLE_SIZE * 7/8

    // oddswap: 0 if row 0 has 8 cells (standard), 1 if row 0 has 7 cells (flipped)
    // Matches Perl's $pdata{$player}{oddswap} set by RandomLevel's r value
    int oddswap = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;

    float midX = (sBubble.oldPosX + sBubble.posX) / 2.0f;
    float midY = (sBubble.oldPosY + sBubble.posY) / 2.0f;

    // Formula: int(($y-top_limit+ROW_SIZE/2) / ROW_SIZE)
    // Use bubbleOffset (not leftLimit/topLimit) as the base — bubbleOffset is what the grid
    // cells are actually positioned from. leftLimit/topLimit can differ by a few pixels.
    int cy = (int)((midY - bArray.bubbleOffset.y + ROW_SIZE/2.0f) / ROW_SIZE);

    // Clamp row first so we can read the actual row size
    if (cy < 0) cy = 0;
    if (cy > 12) cy = 12;

    // Row offset: 16px if row has 7 bubbles (odd-shifted), 0 if 8 bubbles.
    // Derived from actual row size so it works for both r=0 and r=1 RandomLevel starts.
    int oddRowOffset = (bArray.bubbleMap[cy].size() == 7) ? BUBBLE_SIZE / 2 : 0;
    int cx = (int)((midX - bArray.bubbleOffset.x + BUBBLE_SIZE/2.0f - oddRowOffset) / BUBBLE_SIZE);

    // Clamp column using actual row size (not assumed parity) to avoid out-of-bounds
    if (cx < 0) cx = 0;
    int maxCol = (int)bArray.bubbleMap[cy].size() - 1;
    if (cx > maxCol) cx = maxCol;

    // If we have an anchor (hit bubble's grid pos), ensure the result is adjacent to it.
    // This prevents placement at a disconnected cell that CheckAirBubbles would immediately remove.
    if (anchorRow >= 0 && anchorCol >= 0) {
        bool calcFree = (bArray.bubbleMap[cy][cx].bubbleId == -1);
        bool calcAdj  = IsGridAdjacent(anchorRow, anchorCol, cy, cx, oddswap);

        if (!calcFree || !calcAdj) {
            // Find the free neighbor of the anchor whose pixel center is closest to midpoint
            float bestDist = 1e30f;
            int bestRow = cy, bestCol = cx;
            bool foundAdj = false;

            for (auto [dr, dc] : GridNeighborOffsets(anchorRow, oddswap)) {
                int nr = anchorRow + dr;
                int nc = anchorCol + dc;
                if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                if (bArray.bubbleMap[nr][nc].bubbleId != -1) continue; // occupied

                int cellRowOffset = (bArray.bubbleMap[nr].size() == 7) ? BUBBLE_SIZE / 2 : 0;
                float cellX = (float)(bArray.bubbleOffset.x + nc * BUBBLE_SIZE + cellRowOffset);
                float cellY = (float)(bArray.bubbleOffset.y + nr * ROW_SIZE);
                float dist  = (cellX - midX)*(cellX - midX) + (cellY - midY)*(cellY - midY);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestRow  = nr;
                    bestCol  = nc;
                    foundAdj = true;
                }
            }

            if (foundAdj) {
                if (!calcAdj)
                    SDL_Log("GetClosestFreeCell: midpoint gave [%d][%d] not adj to anchor [%d][%d]; using [%d][%d]",
                            cy, cx, anchorRow, anchorCol, bestRow, bestCol);
                cy = bestRow;
                cx = bestCol;
            } else {
                // All anchor neighbors occupied — fall back to original BFS from anchor
                SDL_Log("WARNING: all anchor [%d][%d] neighbors occupied, BFS fallback", anchorRow, anchorCol);
                std::queue<std::pair<int,int>> q;
                std::set<std::pair<int,int>> vis;
                q.push({anchorRow, anchorCol});
                vis.insert({anchorRow, anchorCol});
                bool found = false;
                while (!q.empty() && !found) {
                    auto [r, c] = q.front(); q.pop();
                    for (auto [dr, dc] : GridNeighborOffsets(r, oddswap)) {
                        int nr = r + dr, nc = c + dc;
                        if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                        if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                        if (vis.count({nr, nc})) continue;
                        vis.insert({nr, nc});
                        if (bArray.bubbleMap[nr][nc].bubbleId == -1) {
                            cy = nr; cx = nc; found = true; break;
                        }
                        q.push({nr, nc});
                    }
                }
            }
        }
    } else if (bArray.bubbleMap[cy][cx].bubbleId != -1) {
        // Ceiling hit (no anchor) — BFS from calculated position
        SDL_Log("WARNING: Calculated position [%d][%d] is occupied, BFS fallback", cy, cx);
        std::queue<std::pair<int,int>> q;
        std::set<std::pair<int,int>> vis;
        q.push({cy, cx}); vis.insert({cy, cx});
        bool found = false;
        while (!q.empty() && !found) {
            auto [r, c] = q.front(); q.pop();
            for (auto [dr, dc] : GridNeighborOffsets(r, oddswap)) {
                int nr = r + dr, nc = c + dc;
                if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                if (vis.count({nr, nc})) continue;
                vis.insert({nr, nc});
                if (bArray.bubbleMap[nr][nc].bubbleId == -1) {
                    cy = nr; cx = nc; found = true; break;
                }
                q.push({nr, nc});
            }
        }
    }

    *row = cy;
    *col = cx;
}

void BubbleGame::UpdateSingleBubbles(int /*id*/) {
    // Original: iter_players processes ALL players in one loop (line 2105)
    // We process ALL bubbles from ALL players together, like the original

    // Track positions already reserved by chain reactions to prevent conflicts
    std::set<std::pair<int, int>> occupiedPositions;
    for (const auto &sb : singleBubbles) {
        if (sb.chainExists && sb.chainRow != -1 && sb.chainCol != -1) {
            occupiedPositions.insert({sb.chainRow, sb.chainCol});
        }
    }

    for (size_t i = 0; i < singleBubbles.size(); i++) {
        SingleBubble &sBubble = singleBubbles[i];

        if (sBubble.shouldClear) continue;
        // Process ALL players' bubbles (original processes all in iter_players loop)
        // Don't skip based on assignedArray - process all of them!

        BubbleArray *bArray = &bubbleArrays[sBubble.assignedArray];
        bool isMini = (currentSettings.playerCount >= 3 && bArray->playerAssigned >= 1);

        // For chain reaction bubbles, update position FIRST to set chainReachedDest flag
        if (sBubble.chainExists && !sBubble.chainReachedDest) {
            sBubble.UpdatePosition();
        } else if (!sBubble.chainExists) {
            // For non-chain bubbles, update as normal
            sBubble.UpdatePosition();
        }

        // NOW check if chain reaction completed (after UpdatePosition set the flag)
        if (sBubble.chainReachedDest && sBubble.chainRow != -1 && sBubble.chainCol != -1) {
            SDL_Log("Chain reaction completed! Placing bubble %d at grid[%d][%d]",
                    sBubble.bubbleId, sBubble.chainRow, sBubble.chainCol);
            bArray->PlacePlayerBubble(sBubble.bubbleId, sBubble.chainRow, sBubble.chainCol);
            bArray->newShoot = true;
            audMixer->PlaySFX("stick");
            bArray->stickAnimActive = true; bArray->stickAnimFrame = 0; bArray->stickAnimSlowdown = 0;
            bArray->stickAnimPos = {(int)sBubble.pos.x, (int)sBubble.pos.y};
            sBubble.shouldClear = true; // Now clear it
            CheckPossibleDestroy(*bArray);
            // Chain reaction bubble landed — check if more chains are still in flight
            // before releasing malus (original line 2218: no chain reactions in flight)
            {
                int arr = bArray->playerAssigned;
                bool chainInFlight = false;
                for (const auto& sb : singleBubbles)
                    if (!sb.shouldClear && sb.assignedArray == arr && sb.chainExists && !sb.chainReachedDest) { chainInFlight = true; break; }
                if (!chainInFlight && (currentSettings.networkGame ? arr == 0 : (currentSettings.playerCount >= 2 || currentSettings.mpTraining)))
                    ProcessMalusQueue(bubbleArrays[arr], frameCount);
            }
            // Chain-reaction landings don't count toward the compressor/new-root counter
            // (only real fired shots do) — see CheckGameState's countForRoot parameter.
            CheckGameState(*bArray, false);
            continue;
        }

        // Handle launching bubble collisions
        if (sBubble.launching) {
            // Check if remote player has mpStickPending flag (original line 2190)
            BubbleArray* launchArray = &bubbleArrays[sBubble.assignedArray];
            if (launchArray->mpStickPending) {
                // Stick bubble at exact position from 's' message (original line 2192)
                SDL_Log("Processing mp_stick for player %d: cx=%d cy=%d col=%d",
                        sBubble.assignedArray, launchArray->stickCx, launchArray->stickCy, launchArray->stickCol);

                launchArray->PlacePlayerBubble(launchArray->stickCol, launchArray->stickCy, launchArray->stickCx);
                launchArray->newShoot = true;
                launchArray->mpStickPending = false;  // Clear flag (original line 2191)

                sBubble.shouldClear = true;
                audMixer->PlaySFX("stick");
                launchArray->stickAnimActive = true; launchArray->stickAnimFrame = 0; launchArray->stickAnimSlowdown = 0;
                launchArray->stickAnimPos = {(int)sBubble.pos.x, (int)sBubble.pos.y};
                CheckPossibleDestroy(*launchArray);
                CheckGameState(*launchArray);
                continue;  // Skip normal collision detection
            }

            // In network games, skip collision detection for remote players' bubbles
            // They will be placed via 's' message to ensure sync (handled above)
            if (currentSettings.networkGame && sBubble.assignedArray != 0) {
                // This is remote player's bubble - wait for 's' message
                continue;
            }

            // Original line 2195: ceiling check FIRST, independent of existing bubbles
            if (sBubble.pos.y <= bArray->topLimit) {
                int row, col;
                GetClosestFreeCell(sBubble, *bArray, &row, &col, -1, -1, isMini);
                SDL_Log("Ceiling hit: placing at row=%d col=%d pos=(%.1f,%.1f)", row, col, (float)sBubble.pos.x, (float)sBubble.pos.y);
                if (currentSettings.networkGame && sBubble.assignedArray == 0) {
                    NetworkClient* netClient = NetworkClient::Instance();
                    if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                        char stickData[128];
                        snprintf(stickData, sizeof(stickData), "s%d:%d:%d:%s", col, row, sBubble.bubbleId,
                                 BuildNextColorsStr(*bArray).c_str());
                        netClient->SendGameData(stickData);
                    }
                }
                bArray->PlacePlayerBubble(sBubble.bubbleId, row, col);
                bArray->newShoot = true;
                audMixer->PlaySFX("stick");
                bArray->stickAnimActive = true; bArray->stickAnimFrame = 0; bArray->stickAnimSlowdown = 0;
                bArray->stickAnimPos = {(int)sBubble.pos.x, (int)sBubble.pos.y};
                sBubble.shouldClear = true;
                CheckPossibleDestroy(*bArray);
                // Original: the ceiling-stick branch (~2195-2204) has no malus generation at all —
                // only the collision-stick and chain-landing branches release queued malus.
                CheckGameState(*bArray);
                goto STOP_ITER;
            }

            for (int hitRow = 0; hitRow < (int)bArray->bubbleMap.size(); hitRow++) {
                for (int hitCol = 0; hitCol < (int)bArray->bubbleMap[hitRow].size(); hitCol++) {
                    Bubble &bubble = bArray->bubbleMap[hitRow][hitCol];
                    if (sBubble.IsCollision(&bubble)) {
                        int row, col;
                        GetClosestFreeCell(sBubble, *bArray, &row, &col, hitRow, hitCol, isMini);
                        SDL_Log("Bubble stuck at row=%d, col=%d, position=(%.1f, %.1f)",
                                row, col, (float)sBubble.pos.x, (float)sBubble.pos.y);

                        // In network game, send stick position to opponent
                        if (currentSettings.networkGame && sBubble.assignedArray == 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                                // Send: s{cx}:{cy}:{bubbleColor}:{nextcolors...}
                                // Perl format (frozen-bubble line 2199): "s$cx:$cy:$col:@{nextcolors}"
                                char stickData[256];
                                snprintf(stickData, sizeof(stickData), "s%d:%d:%d:%s",
                                    col, row, sBubble.bubbleId, BuildNextColorsStr(*bArray).c_str());
                                SDL_Log("Sending stick: col=%d row=%d color=%d nextColors=%s",
                                        col, row, sBubble.bubbleId, BuildNextColorsStr(*bArray).c_str());
                                netClient->SendGameData(stickData);
                            }
                        }

                        bArray->PlacePlayerBubble(sBubble.bubbleId, row, col);
                        bArray->newShoot = true;
                        audMixer->PlaySFX("stick");
                        bArray->stickAnimActive = true; bArray->stickAnimFrame = 0; bArray->stickAnimSlowdown = 0;
                        bArray->stickAnimPos = {(int)sBubble.pos.x, (int)sBubble.pos.y};
                        sBubble.shouldClear = true;
                        CheckPossibleDestroy(*bArray);
                        // Release queued malus at stick time (original line 2218-2250)
                        // Use bArray->playerAssigned (not sBubble.assignedArray) — see note above.
                        {
                            int arr = bArray->playerAssigned;
                            bool chainInFlight = false;
                            for (const auto& sb : singleBubbles)
                                if (sb.assignedArray == arr && sb.chainExists && !sb.chainReachedDest) { chainInFlight = true; break; }
                            if (!chainInFlight && (currentSettings.networkGame ? arr == 0 : (currentSettings.playerCount >= 2 || currentSettings.mpTraining)))
                                ProcessMalusQueue(bubbleArrays[arr], frameCount);
                        }
                        CheckGameState(*bArray);
                        goto STOP_ITER;
                    }
                };
            }
        }
        // Handle falling bubbles without chain reactions
        // Chain reaction targets are now assigned ONCE in AssignChainReactions() (called from CheckPossibleDestroy)
        // This matches original frozen-bubble line 814-865 which processes @falling bubbles once after creating them
        // Falling bubbles just fall normally here - no per-frame chain reaction checks
        else if (sBubble.falling && !sBubble.chainExists && !sBubble.exploding) {
            // Just let the bubble continue falling
            // Chain reactions are handled by AssignChainReactions() when bubbles are first created
        }

        STOP_ITER:
        continue;
    }
    singleBubbles.erase(std::remove_if(singleBubbles.begin(), singleBubbles.end(), [](const SingleBubble &s){ return s.shouldClear; }), singleBubbles.end());

    // Update malus bubbles (they rise upward to stick position)
    // Original port used 2.0 px/frame; increased 25% to 2.5 for better feel.
    // Multiplied by deltaScale so speed is frame-rate-independent.
    const float MALUS_SPEED = 2.5f * FrozenBubble::Instance()->deltaScale;
    for (auto &malus : malusBubbles) {
        if (malus.shouldClear) continue;

        // Get the BubbleArray for this malus bubble
        BubbleArray* malusArray = &bubbleArrays[malus.assignedArray];

        // Move upward toward stick position
        // Mini players use half bubble size
        bool isMini = (currentSettings.playerCount >= 3 && malusArray->playerAssigned >= 1);
        int bubbleSize = isMini ? 16 : 32;
        int rowSize = bubbleSize * 7 / 8;  // 14 for mini, 28 for full
        float targetY = (rowSize * malus.stickY) + malusArray->bubbleOffset.y;

        if (malus.posY > targetY) {
            malus.posY -= MALUS_SPEED;
            malus.pos.y = (int)malus.posY;
        } else {
            // Reached stick position - place bubble in grid
            malus.shouldStick = true;
        }

        // Stick the malus bubble to the grid
        if (malus.shouldStick) {
            // malus.stickY was computed once, back in ProcessMalusQueue, from the
            // column heights at generation time. Many frames pass while the bubble
            // rises into place, and the board can change in that column in the
            // meantime (a chain reaction, a new shot landing, a compression/new-root
            // shift). Sticking at a stale row would silently overwrite whatever
            // bubble is actually there now, or land in a gap with nothing next to
            // it. Recompute the landing row fresh from the board's current state,
            // mirroring the same top-of-column scan ProcessMalusQueue does, before
            // placing the bubble.
            int topOfCol = 0;
            for (size_t row = 0; row < malusArray->bubbleMap.size(); row++) {
                if (malusArray->bubbleMap[row][malus.cx].bubbleId != -1) {
                    if ((int)row > topOfCol) topOfCol = (int)row;
                }
            }
            malus.stickY = std::min(topOfCol + 1, 12);

            SDL_Log("Malus bubble sticking at cx=%d stickY=%d", malus.cx, malus.stickY);
            malusArray->PlacePlayerBubble(malus.bubbleId, malus.stickY, malus.cx);
            // Malus landing must NOT be a match activator.
            // Original uses real_stick_bubble() (no match check) for malus sticking.
            // Reset playerBubble so CheckPossibleDestroy below doesn't treat it as an
            // activator — otherwise malus landing next to same-colored bubbles would
            // generate and send malus back to opponents.
            malusArray->bubbleMap[malus.stickY][malus.cx].playerBubble = false;
            malusArray->newShoot = true;

            // Send 'M' message to sync sticking (original line 1456-1466)
            if (currentSettings.networkGame && malusArray->playerAssigned == 0) {
                NetworkClient* netClient = NetworkClient::Instance();
                if (netClient && netClient->IsConnected()) {
                    char MMsg[64];
                    snprintf(MMsg, sizeof(MMsg), "M%d:%d", malus.cx, malus.stickY);
                    SDL_Log("Sending malus stick: cx=%d stickY=%d", malus.cx, malus.stickY);
                    netClient->SendGameData(MMsg);
                }
            }

            malus.shouldClear = true;
            CheckPossibleDestroy(*malusArray);
            // Original uses real_stick_bubble() for malus sticking (line 2505/2514),
            // which does NOT increment the newroot counter. Do NOT call CheckGameState here.
        }
    }

    // Clean up malus bubbles
    malusBubbles.erase(std::remove_if(malusBubbles.begin(), malusBubbles.end(),
                                      [](const MalusBubble &m){ return m.shouldClear; }),
                       malusBubbles.end());
}
