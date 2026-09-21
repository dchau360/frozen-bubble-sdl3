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
#include "networkclient.h"

#include <fstream>
#include <sstream>
#include <set>
#include <queue>
#include <map>

#include <cmath>
#include <algorithm>
#include "bubblegame_internal.h"
#include "localmultiplayer_settings.h"
#include "roundstats_color.h"
#include "playerbadge.h"
#include "replay_recorder.h"

// Which platform badge belongs to a player array, if any.
//
// Everyone's tag -- including this client's own -- is looked up by nick in what
// the lobby's LIST responses reported, so every board on every machine agrees
// on what it is showing. PlatformTag() is only the fallback for an array this
// client simulates, which covers the two cases where no LIST entry can exist:
// a local (non-network) game, and a bot this client is hosting, which really is
// playing from this machine.
static char PlatformTagFor(const BubbleArray &arr, bool owned) {
    if (!arr.playerNickname.empty()) {
        if (char tag = NetworkClient::Instance()->GetPlatformForNick(arr.playerNickname))
            return tag;
    }
    return owned ? PlatformTag() : 0;
}


// Positions are centered above/below each player's grid. One 5-slot table
// covers 3/4/5-player fixed layouts and >5-player royale alike: slot 0 is
// the fixed center board; slots 1-4 are the four mini-board corners, in the
// same order the original per-count switch spelled out separately (top-left,
// top-right, bottom-left, bottom-right) -- each count only ever used a
// prefix of these five slots, in this same order.
SDL_Point PlayerSlotPosition(int playerCount, int playerIdx, int parkedSlot) {
    static const SDL_Point kPlayerSlotPos[5] = {
        {320, 12}, {83, 2}, {553, 2}, {83, 465}, {553, 465},
    };

    int slotIdx;
    if (playerCount >= 3 && playerCount <= 5) {
        // 3/4/5-player fixed layouts: array order is display order.
        slotIdx = playerIdx;
    } else {
        // >5-player royale (and the original switch's default for any other
        // count): player 0 is the fixed center board; everyone else keys off
        // their parked slot (0-3), not raw array index, since royale
        // reassigns slots as players die/leave. An out-of-range parked slot
        // (defensive) falls back to slot 1, matching the original local
        // fallback exactly.
        if (playerIdx == 0) {
            slotIdx = 0;
        } else {
            slotIdx = (parkedSlot >= 0 && parkedSlot < 4) ? parkedSlot + 1 : 1;
        }
    }
    return kPlayerSlotPos[slotIdx];
}

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

        // Pick the color before rendering so a team change is visible this
        // frame. Reset no-team players to white instead of retaining a tint
        // from their previous team.
        SDL_Color nameColor = {255, 255, 255, 255};
        if (currentSettings.playerTeams[i] != kNoTeam) {
            nameColor = kTeamColors[currentSettings.playerTeams[i] - 1];
        }
        playerNameWinText[i].UpdateColor(nameColor, {0, 0, 0, 255});
        playerNameWinText[i].UpdateText(renderer, nameWinStr, 0);

        // Use fixed positions based on player layout (matching original FB2)
        const SDL_Point slotPos = PlayerSlotPosition(currentSettings.playerCount, i, bArray.parkedSlot);

        playerNameWinText[i].UpdatePosition({slotPos.x - (playerNameWinText[i].Coords()->w / 2), slotPos.y});
    }
}


void BubbleGame::UpdatePoppedText(BubbleArray &bArray, int idx) {
    // Live popped-bubble count, in every multiplayer mode -- Classic and Clear
    // included, where it is just a running total. In Race it also carries the
    // target, since a bare number there tells you nothing about how close the
    // round is to ending.
    if (currentSettings.playerCount < 2) return;
    if (idx < 0 || idx >= MAX_NET_PLAYERS) return;

    // Mini boards (the 3-5 player corner layouts, and royale's parked slots)
    // are only 128px wide with just ~40px of board free to the right of the
    // next-bubble slot, so they drop the "Pop" label and show the bare
    // number -- the same useMini boundary the render loop already uses for
    // bubble textures and stick animation.
    const bool useMini = (currentSettings.playerCount >= 3 && idx >= 1);

    char buf[32];
    if (currentSettings.gameMode == GameMode::Race) {
        if (useMini) snprintf(buf, sizeof(buf), "%d/%d", bArray.rPopped, currentSettings.raceTarget);
        else snprintf(buf, sizeof(buf), "Pop %d/%d", bArray.rPopped, currentSettings.raceTarget);
    } else {
        if (useMini) snprintf(buf, sizeof(buf), "%d", bArray.rPopped);
        else snprintf(buf, sizeof(buf), "Pop %d", bArray.rPopped);
    }

    // Highlight whoever is winning on pops, but only in the two modes where
    // that is what decides the round. In Classic and Clear the count is
    // information, not a standing, and colouring a "leader" there would imply
    // a race that isn't being run.
    SDL_Color colour = {120, 255, 140, 255}; // Bright green: reads clearly against any board
    if (GameModeCountsPops(currentSettings.gameMode)) {
        const int leader = LeadingPopper();
        // A tie has no leader (LeadingPopper returns -1) and nobody is
        // highlighted, which is the honest picture: level is level.
        if (leader == idx && bArray.rPopped > 0)
            colour = {255, 225, 60, 255}; // Bright gold: this player is currently leading
    }
    poppedText[idx].UpdateColor(colour, {0, 0, 0, 255});
    poppedText[idx].UpdateText(renderer, buf, 0);

    // Anchored to the right of this board's own "current bubble" slot -- the
    // one actually loaded in the cannon and about to fire -- rather than the
    // "next bubble" preview underneath it, so it sits higher up in the same
    // row as the shot the player is lining up right now instead of next to
    // the queued-up color. Vertically centered against that slot; every
    // layout's launch rect is the same 32px square, so this lines up whether
    // it's the 2-player full board or a 3-5 player mini one.
    const SDL_Rect &cl = bArray.curLaunchRct;
    const int gap = 6;
    SDL_Point at = {cl.x + cl.w + gap, cl.y + cl.h / 2 - poppedText[idx].Coords()->h / 2};
    poppedText[idx].UpdatePosition(at);

    // Timed mode: the shared countdown stacks directly under the pop line, at
    // the same anchor, so a player's whole HUD -- their count and the clock
    // both -- sits next to their own shooter instead of splitting the clock
    // off to a separate spot on screen that's easy to lose track of mid-shot.
    // modeTimerText is one shared object (the number is identical for every
    // board); UpdateText only re-renders its texture on the first of these
    // per-frame calls and just re-blits it at each board's own position for
    // the rest, so drawing it once per player costs nothing extra.
    if (currentSettings.gameMode == GameMode::Timed) {
        const int remaining = TimedSecondsRemaining();
        char tbuf[16];
        if (modeTimerExpired && !gameFinish) {
            // The gap between our own buzzer and the winner being announced.
            // Said out loud rather than left on a frozen "0:00", because the
            // board stops accepting input here (see ModeAwaitingVerdict) and
            // a player whose shots stopped working deserves to know why.
            snprintf(tbuf, sizeof(tbuf), "TIME UP");
        } else {
            snprintf(tbuf, sizeof(tbuf), "%d:%02d", remaining / 60, remaining % 60);
        }
        // Red for the last five seconds, so the finish is visible without reading.
        const SDL_Color tColour = (!modeTimerExpired && remaining <= 5)
            ? SDL_Color{255, 70, 70, 255} : SDL_Color{255, 170, 40, 255};
        modeTimerText.UpdateColor(tColour, {0, 0, 0, 255});
        modeTimerText.UpdateText(renderer, tbuf, 0);
        modeTimerText.UpdatePosition({at.x, at.y + poppedText[idx].Coords()->h});
    }
}

// Pure draw companion to UpdatePoppedText(): blits the already-computed
// pop-count line and, in Timed mode, the shared countdown under it. Kept
// separate (R1d-iv, first sub-slice) so a redundant/extra render, or a future
// replay draw, re-blits the current cached textures without recomputing the
// strings; both positions were already fixed by the recompute call. The
// playerCount/idx guards mirror UpdatePoppedText's own early returns so the
// draw can never blit past a recompute that bailed out.
void BubbleGame::DrawPoppedText(int idx) {
    if (currentSettings.playerCount < 2) return;
    if (idx < 0 || idx >= MAX_NET_PLAYERS) return;

    { SDL_FRect fr = ToFRect(*poppedText[idx].Coords());
      SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), poppedText[idx].Texture(), nullptr, &fr); }

    // Same guard as the recompute branch, so the timer is only ever drawn on a
    // frame its text/position was actually refreshed.
    if (currentSettings.gameMode == GameMode::Timed) {
        { SDL_FRect fr = ToFRect(*modeTimerText.Coords());
          SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), modeTimerText.Texture(), nullptr, &fr); }
    }
}

void BubbleGame::MeasureLiveBadges(const BubbleArray &bArray, size_t &poolIdx) {
    liveBadgeCellCount = 0;
    if (!currentSettings.networkGame) return;
    SDL_Renderer *rend = const_cast<SDL_Renderer *>(renderer);
    const char platformTag = PlatformTagFor(bArray, OwnsArray(bArray));
    for (int pass = 0; pass < 2; ++pass) {
        PlayerBadge b;
        const bool ok = pass == 0 ? GetPlatformBadge(platformTag, b)
                                  : GetInputBadge(bArray.roundInput, b);
        if (!ok) continue;
        // Label first, chip second, label's texture last: UpdateText is what
        // gives the cell its width, and the chip has to be sized from that
        // width but painted underneath the glyphs. Only the measure half
        // runs here; DrawLiveBadges() sizes/paints the chips from the
        // precomputed widths below.
        const size_t cellIdx = poolIdx++;
        TTFText &t = StatsPanelCell(badgeCellPool, cellIdx, 11);
        t.UpdateColor(b.text, {0, 0, 0, 0});
        t.UpdateText(rend, b.label, 0);
        const int w = PlayerBadgeChipWidth(t.Coords()->w);
        liveBadgeCells[liveBadgeCellCount++] = {b, w, cellIdx};
    }
}

int BubbleGame::DrawLiveBadges(int x, int y) {
    if (liveBadgeCellCount == 0) return 0;
    SDL_Renderer *rend = const_cast<SDL_Renderer *>(renderer);
    const int startX = x;
    for (int i = 0; i < liveBadgeCellCount; i++) {
        const LiveBadgeCell &cell = liveBadgeCells[i];
        DrawPlayerBadgeChip(rend, {x, y, cell.chipWidth, 14}, cell.badge);
        TTFText &t = badgeCellPool[cell.poolIdx];
        t.UpdatePosition({x + kPlayerBadgePadX, y + 1});
        if (t.Texture()) {
            SDL_FRect fr = ToFRect(*t.Coords());
            SDL_RenderTexture(rend, t.Texture(), nullptr, &fr);
        }
        x += cell.chipWidth + 3;
    }
    return x - startX;
}

void BubbleGame::UpdateScoreText(BubbleArray &bArray, int slot) {
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

    // One scoreText slot per player (single-player uses slot 0; the 2P branch
    // passes each player's own index) so each player's line keeps its own
    // cache instead of alternating a shared object between different strings.
    // In multiplayer, this gets called once per player in the render loop;
    // the blit itself is a separate DrawScoreText() call at that player's
    // score position (R1d-iv split).
    scoreText[slot].UpdateText(renderer, scoreStr, 0);
    scoreText[slot].UpdatePosition(bArray.scorePos);

    // Badges trail the name on the same line; two pool slots per player,
    // indexed off the slot so the two banners never share a cached texture
    // and re-render each other every frame. Measure here so DrawScoreText()
    // stays a pure blit (R1d-iv).
    size_t badgeIdx = (size_t)slot * 2;
    MeasureLiveBadges(bArray, badgeIdx);
}

// Pure draw companion to UpdateScoreText(): blits the already-computed score
// line and the live badges that trail it (the badges were measured by
// UpdateScoreText -> MeasureLiveBadges). Kept separate (R1d-iv) so a
// redundant/extra render, or a future replay draw, re-blits the current
// cached textures without recomputing anything.
void BubbleGame::DrawScoreText(int slot) {
    { SDL_FRect fr = ToFRect(*scoreText[slot].Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), scoreText[slot].Texture(), nullptr, &fr); }

    // Badges trail the name on the same line here -- unlike the 3-5 player
    // boards, a 2-player banner is left-anchored with room to its right.
    const SDL_Rect *nameRect = scoreText[slot].Coords();
    DrawLiveBadges(nameRect->x + nameRect->w + 6, nameRect->y + 2);
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


static void DrawAimGuide(SDL_Renderer* rend, const BubbleArray& bArray, bool isMini, float deltaScale) {
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
    //
    // Smoothed rather than the raw per-frame value: unlike the real bubble, this
    // whole preview path is re-simulated from scratch every frame, so the exact
    // 16ms/17ms alternation SDL_GetTicks() produces at a steady 60fps (see
    // SmoothTowards) would otherwise visibly shimmer the dotted line every frame
    // even while the aim itself is perfectly still (found live: "the autoaim
    // display is gittery"). One shared smoothed value per real frame, not per
    // board: multiple players' own aim guides all read the same instantaneous
    // deltaScale in a single frame, and re-smoothing an unchanged raw value would
    // just re-apply the same delta harmlessly, but skipping repeats keeps the
    // smoothing's own time constant meaning "per frame" regardless of player count.
    static float smoothedDs = 1.0f;
    static float lastRawDs = -1.0f;
    const float rawDs = deltaScale;
    if (rawDs != lastRawDs) {
        lastRawDs = rawDs;
        smoothedDs = SmoothTowards(smoothedDs, rawDs);
    }
    const float ds = smoothedDs;
    const int substeps = LaunchSubstepCount(BUBBLE_SIZE, ds);
    const float subscale = ds / static_cast<float>(substeps);
    float dx = speed * cosf(angle) * subscale;
    float dy = speed * sinf(angle) * subscale;

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);

    for (int step = 0; step < 400 * substeps; step++) {
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
        if (step % (8 * substeps) == 0) {
            int alpha = 200 - step / (2 * substeps);
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


// Returns the pool's cell at `idx`, growing the pool and attaching each newly
// added slot to one immutable font shared by cells of the same size. Backs the
// post-round stats table, royale
// HUD, and malus-alert toasts: each renders a per-frame count of text cells
// that varies with player count/teams/stacked alerts, and giving every cell
// its own persistent slot (addressed by call order) means a cell whose text
// hasn't changed since last frame keeps its cached texture, instead of one
// shared TTFText invalidating on every other cell's different text.
//
// The returned reference is only valid until the next call that grows this
// same pool (pool.resize() below can reallocate). Every call site fetches,
// updates, and renders one cell within a single statement/expression and
// never holds the reference across another cell's call, so this is safe in
// practice -- but don't stash the result in a local that outlives a sibling
// StatsPanelCell() call on the same pool.
TTFText &BubbleGame::StatsPanelCell(std::vector<TTFText> &pool, size_t idx, int fontSize) {
    if (idx >= pool.size()) {
        std::unique_ptr<TTF_Font, FontCloser> *fontOwner = nullptr;
        if (fontSize == 14) fontOwner = &statsPanelFont14;
        else if (fontSize == 16) fontOwner = &statsPanelFont16;

        if (fontOwner && !*fontOwner) {
            fontOwner->reset(TTF_OpenFont(ASSET("/gfx/DroidSans.ttf").c_str(),
                                          static_cast<float>(fontSize)));
        }

        size_t oldSize = pool.size();
        pool.resize(idx + 1);
        for (size_t j = oldSize; j <= idx; j++) {
            if (fontOwner) pool[j].LoadFont(fontOwner->get());
            else pool[j].LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), fontSize);
        }
    }
    return pool[idx];
}

// Pure draw: the "hurry up" warning texture. ResolvePlayerControls()
// (bubblegame_shooter.cpp) decides visibility and plays the warning sound as
// part of its own gameplay-timer bookkeeping; this function only blits the
// texture when bArray.hurryWarnVisible is currently true, and touches no
// gameplay state. Safe to call zero or many times for the same simulation
// step. See docs/REPLAY_PLAN.md's AdvanceSimulation boundary and
// docs/REPLAY_PROGRESS.md's R1d entry.
void BubbleGame::DrawHurryWarning(SDL_Renderer *rend, BubbleArray &bArray) {
    if (!bArray.hurryWarnVisible) return;
    SDL_FRect fr = ToFRect(bArray.hurryRct);
    SDL_RenderTexture(rend, bArray.hurryTexture, nullptr, &fr);
}

void BubbleGame::RenderMalusAlerts(SDL_Renderer *rend) {
    UpdateMalusAlerts();
    DrawMalusAlerts(rend);
}

// Recompute half of the malus-alert toasts (the half that was still fused
// after R1d-ii split the aging out): formats each toast's text, positions it
// and records its alpha into malusAlertDrawOps so DrawMalusAlerts() is a pure
// blit. See docs/REPLAY_PROGRESS.md's R1d-iv entry.
void BubbleGame::UpdateMalusAlerts() {
    malusAlertDrawOps.clear();
    size_t alertIdx = 0;
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
            // Recomputed only when this board is on the current view page;
            // the alert's own framesLeft countdown still ages regardless of
            // paging (see AgeMalusAlerts()).
            if (!p.boardVisible) { line++; continue; }
            char buf[96];
            if (a.blocked)
                snprintf(buf, sizeof(buf), "Blocked  -%d", a.count);
            else
                snprintf(buf, sizeof(buf), "%s  +%d", a.fromNick.c_str(), a.count);
            const size_t cellIdx = alertIdx++;
            TTFText &alertText = StatsPanelCell(malusAlertPool, cellIdx, 16);
            alertText.UpdateColor({255, 140, 40, 255}, {0, 0, 0, 0});  // Orange "incoming malus" toast
            alertText.UpdateText(renderer, buf, 0);
            int tw = alertText.Coords()->w;
            int x = ax;
            if (x + tw > 636) x = 636 - tw;  // keep on-screen
            if (x < 4) x = 4;
            int y = ay - line * lineH;
            if (y < 2) y = 2;
            alertText.UpdatePosition({x, y});
            const Uint8 alpha = a.framesLeft >= 40 ? 255 : (Uint8)(a.framesLeft * 255 / 40);  // fade out
            malusAlertDrawOps.push_back({cellIdx, *alertText.Coords(), alpha});
            line++;
        }
    }
}

// Pure draw half: blits the toasts UpdateMalusAlerts() just recomputed.
void BubbleGame::DrawMalusAlerts(SDL_Renderer *rend) {
    for (const MalusAlertDrawOp &op : malusAlertDrawOps) {
        if (op.poolIdx >= malusAlertPool.size()) continue;
        SDL_Texture *tex = malusAlertPool[op.poolIdx].Texture();
        if (!tex) continue;
        SDL_SetTextureAlphaMod(tex, op.alpha);
        SDL_FRect fr = ToFRect(op.rect);
        SDL_RenderTexture(rend, tex, nullptr, &fr);
        SDL_SetTextureAlphaMod(tex, 255);
    }
}

// Mutator: ages and prunes every player's malusAlerts. Split out of
// RenderMalusAlerts() (R1d-ii) so a redundant/extra render never double-ages
// the toasts; call once per real simulation step, after the draw, so the
// frame that shows framesLeft==1 fading out is the same frame that then
// erases it (matching the original fused behavior exactly).
void BubbleGame::AgeMalusAlerts() {
    for (int i = 0; i < currentSettings.playerCount; i++) {
        BubbleArray &p = bubbleArrays[i];
        for (auto &a : p.malusAlerts) a.framesLeft--;
        p.malusAlerts.erase(
            std::remove_if(p.malusAlerts.begin(), p.malusAlerts.end(),
                           [](const BubbleArray::MalusAlert &a) { return a.framesLeft <= 0; }),
            p.malusAlerts.end());
    }
}

// Pure draw: blinking yellow border on a mini-board that was actually
// attacked. Reads bArray.attackFlashFramesLeft as of the call; aging is a
// separate mutator, AgeAttackFlash() below, so an extra or repeated call to
// this function does not advance the timer.
//
// Gated on useMini (playerCount >= 3, same boundary every other
// opponent-board visual already uses -- bubble textures, chain bubbles, the
// stick-animation sprite) and bArray.boardVisible: attackFlashFramesLeft is
// set by SendMalusToOpponent for every network game regardless of room size
// or paging, but this only ever draws (and AgeAttackFlash below only ever
// ages) when the board is the current useMini/boardVisible target -- a
// paged-out board's timer does not age while its board isn't visible. This
// is an existing quirk, preserved as-is per docs/REPLAY_PROGRESS.md's R1d-ii
// entry, not a behavior change.
void BubbleGame::DrawAttackFlash(SDL_Renderer *rend, BubbleArray &bArray, bool useMini) {
    if (!(useMini && bArray.boardVisible && bArray.attackFlashFramesLeft > 0)) return;
    const int kBlinkPeriodFrames = 20;  // ~0.33s on/off cycle at 60fps
    if ((frameCount % kBlinkPeriodFrames) < kBlinkPeriodFrames / 2) {
        int bx = bArray.leftLimit - 6;
        int by = bArray.topLimit - 6;
        int bw = (bArray.rightLimit - bArray.leftLimit) + 12;
        int bh = (bArray.shooterSprite.rect.y + bArray.shooterSprite.rect.h + 4) - by;
        SDL_SetRenderDrawColor(rend, 255, 255, 100, 255);
        for (int t = 0; t < 2; t++) {
            SDL_FRect fr = ToFRect(SDL_Rect{bx - t, by - t, bw + 2 * t, bh + 2 * t});
            SDL_RenderRect(rend, &fr);
        }
    }
}

// Mutator: ages bArray.attackFlashFramesLeft. Same gating as DrawAttackFlash
// above -- see its comment for the paged-out-board quirk this preserves.
void BubbleGame::AgeAttackFlash(BubbleArray &bArray, bool useMini) {
    if (useMini && bArray.boardVisible && bArray.attackFlashFramesLeft > 0) {
        bArray.attackFlashFramesLeft--;
    }
}

void BubbleGame::RenderRoyaleHud(SDL_Renderer *rend) {
    UpdateRoyaleHud();
    DrawRoyaleHud(rend);
}

// Recompute half of the >5-player royale HUD (alive count + page indicator,
// plus the spectating hints). Fills royaleHudCellPool and records
// royaleHudCellCount so DrawRoyaleHud() is a pure blit.
void BubbleGame::UpdateRoyaleHud() {
    royaleHudCellCount = 0;
    // >5-player royale only (caller already checks playerCount > 5); shown clear of the
    // center board's top (board spans x 190-446, top y=44 -- see NewGame's default: case).
    const int n = currentSettings.playerCount;
    int alive = 0;
    for (int i = 0; i < n; i++) {
        if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) alive++;
    }

    int pageStart = netViewPage * 4 + 1;
    int pageEnd = std::min(pageStart + 3, n - 1);

    auto cell = [&](const char *txt, int x, int y, SDL_Color c) {
        TTFText &t = StatsPanelCell(royaleHudCellPool, royaleHudCellCount++);
        t.UpdateColor(c, {0, 0, 0, 0});
        t.UpdateText(renderer, txt, 0);
        t.UpdatePosition({x, y});
    };

    const SDL_Color hud = {255, 255, 100, 255};

    char aliveBuf[32];
    snprintf(aliveBuf, sizeof(aliveBuf), "%d/%d alive", alive, n);
    cell(aliveBuf, 254, 12, hud);

    char pageBuf[64];
    if (netViewAuto)
        snprintf(pageBuf, sizeof(pageBuf), "opponents: auto  [Tab]");
    else
        snprintf(pageBuf, sizeof(pageBuf), "opponents %d-%d of %d  [Tab]", pageStart, pageEnd, n - 1);
    cell(pageBuf, 254, 28, hud);

    if (bubbleArrays[0].playerState != BubbleArray::PlayerState::ALIVE) {
        const SDL_Color spec = {255, 160, 60, 255};
        cell("SPECTATING", 254, 44, spec);
        cell("[1-4] pin view  [0] auto", 254, 60, spec);
    }
}

void BubbleGame::DrawRoyaleHud(SDL_Renderer *rend) {
    for (int i = 0; i < royaleHudCellCount && i < (int)royaleHudCellPool.size(); i++) {
        TTFText &t = royaleHudCellPool[i];
        if (!t.Texture()) continue;
        SDL_FRect fr = ToFRect(*t.Coords());
        SDL_RenderTexture(rend, t.Texture(), nullptr, &fr);
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


namespace {

// Single source of truth for the round-stats panel's geometry. Both the draw
// (RenderRoundStats) and the tap-target computation (UpdateRoundStatsHitRects)
// go through this, so the two can never drift. Every input is pure state --
// settings.playerCount/playerTeams/networkGame and the tournament flag -- with
// no texture or font metric feeding into it, which is what makes it safe to
// compute outside an SDL draw call.
struct RoundStatsLayout {
    int boxW = 0, boxX = 0, boxY = 0, boxH = 0;
    int rowH = 0, headH = 0;
    bool tournament = false;
    bool discordAlertsApply = false;
    std::vector<int> teams;  // Distinct team numbers present, ascending
};

RoundStatsLayout ComputeRoundStatsLayout(const SetupSettings &settings, bool tournament) {
    RoundStatsLayout layout;
    const int n = settings.playerCount;
    layout.boxW = 544;
    layout.boxX = (640 - layout.boxW) / 2;
    layout.boxY = 6;
    layout.rowH = 16;
    layout.headH = 22;
    const int hintH = settings.networkGame ? layout.rowH : 0;
    // Discord round-result alerts are sniffed server-side off the 'F' opcode
    // for every non-tournament room (see CLAUDE.md's "Discord round-result
    // alerts" section and server/game.c's `!g->tournament_id` guard) --
    // tournament matches never post, so this note would mislead if shown
    // during one.
    layout.tournament = tournament;
    layout.discordAlertsApply = settings.networkGame && !tournament;
    const int discordHintH = layout.discordAlertsApply ? layout.rowH : 0;

    // Distinct teams present, in ascending team-number order (team subtotal
    // rows below). Real teams only: a "TEAM 0" subtotal row would be a total
    // across players who are not on a side together, which is not a team
    // score.
    for (int i = 0; i < n; i++) {
        int t = settings.playerTeams[i];
        if (t == kNoTeam) continue;
        if (std::find(layout.teams.begin(), layout.teams.end(), t) == layout.teams.end())
            layout.teams.push_back(t);
    }
    std::sort(layout.teams.begin(), layout.teams.end());
    const int teamRows = layout.teams.empty() ? 0 : (int)layout.teams.size() + 1;  // +1 separator/header row

    layout.boxH = layout.headH + layout.rowH * (n + 1 + teamRows) + hintH + discordHintH + 6;
    return layout;
}

}  // namespace


void BubbleGame::RenderRoundStats(SDL_Renderer *rend) {
    UpdateRoundStats();
    DrawRoundStats(rend);
}

void BubbleGame::UpdateRoundStats() {
    // Recompute half of the post-round per-player stats table (multiplayer
    // only). Fills statsCellPool and records the draw order into
    // roundStatsOps so DrawRoundStats() is a pure blit of the same cells.
    roundStatsOps.clear();
    const int n = currentSettings.playerCount;
    if (n < 2) return;

    // Panel geometry comes from the same helper UpdateRoundStatsHitRects()
    // uses to size its tap targets, so the two can never drift.
    const RoundStatsLayout layout = ComputeRoundStatsLayout(currentSettings, IsTournamentRound());
    const int boxW = layout.boxW, boxX = layout.boxX, boxY = layout.boxY, boxH = layout.boxH;
    const int rowH = layout.rowH, headH = layout.headH;
    const std::vector<int> &teams = layout.teams;
    const bool tournament = layout.tournament;
    const bool discordAlertsApply = layout.discordAlertsApply;
    statsPanelBox = {boxX, boxY, boxW, boxH};

    // Column x offsets (numbers right-anchored-ish via left placement that fits 14px font).
    const int colName = boxX + 8;
    const int colWin = boxX + 160;
    const int colFired = boxX + 206;
    const int colPopped = boxX + 264;
    const int colSent = boxX + 320;
    const int colRecv = boxX + 372;
    const int colBlk = boxX + 424;
    const int colKills = boxX + 476;

    size_t cellIdx = 0;
    auto cell = [&](const char *txt, int x, int y, SDL_Color c) {
        const size_t idx = cellIdx++;
        TTFText &t = StatsPanelCell(statsCellPool, idx);
        t.UpdateColor(c, {0, 0, 0, 0});
        t.UpdateText(renderer, txt, 0);
        t.UpdatePosition({x, y});
        roundStatsOps.push_back({RoundStatsOpKind::Text, idx, {}, {}});
    };

    // Platform chip then input chip, left to right from `x`. Either can be
    // absent -- a player on an older client, a server older than protocol 1.4,
    // or someone who never fired this round -- and the second chip simply
    // moves left rather than leaving a gap, so a table where nobody reported
    // anything looks exactly as it did before badges existed. Chip background
    // first, label immediately after, so the label lands on top of its own
    // chip and never under the next row's.
    auto badges = [&](char platformTag, char inputTag, int x, int y) {
        // Network games only, same gate DrawLiveBadges applies on the board
        // itself: in a local game every row is a player sitting at this
        // machine, so a platform column would print the same answer on every
        // line and an input column would mostly say "KB" about all of them.
        if (!currentSettings.networkGame) return;
        for (int pass = 0; pass < 2; ++pass) {
            PlayerBadge b;
            const bool ok = pass == 0 ? GetPlatformBadge(platformTag, b)
                                      : GetInputBadge(inputTag, b);
            if (!ok) continue;
            // Not cell(): the chip has to be sized from the label's measured
            // width and drawn before it, so this drives the same pool by hand
            // in the order chip-then-glyphs. One pool slot per badge either
            // way, so the panel's cell accounting is unchanged.
            const size_t idx = cellIdx++;
            TTFText &t = StatsPanelCell(statsCellPool, idx);
            t.UpdateColor(b.text, {0, 0, 0, 0});
            t.UpdateText(renderer, b.label, 0);
            const int w = PlayerBadgeChipWidth(t.Coords()->w);
            t.UpdatePosition({x + kPlayerBadgePadX, y});
            roundStatsOps.push_back({RoundStatsOpKind::Chip, idx, {x, y + 1, w, rowH - 4}, b});
            x += w + 3;
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
    cell("Win", colWin, y, hdr);
    cell("Fire", colFired, y, hdr);
    cell("Pop", colPopped, y, hdr);
    cell("Sent", colSent, y, hdr);
    cell("Rcv", colRecv, y, hdr);
    cell("Blk", colBlk, y, hdr);
    cell("KO", colKills, y, hdr);
    y += rowH;

    char buf[32];
    for (int i = 0; i < n; i++) {
        BubbleArray &p = bubbleArrays[i];
        SDL_Color c = normal;
        switch (RoundStatsRowColorKind(currentSettings.playerTeams[i], p.mpWinner)) {
        case RoundStatsColorKind::TEAM:
            c = kTeamColors[currentSettings.playerTeams[i] - 1];
            break;
        case RoundStatsColorKind::WINNER:
            c = win;
            break;
        case RoundStatsColorKind::NORMAL:
            break;
        }
        std::string name = StatsPlayerName(p, i, currentSettings.networkGame);
        // Shorter than the 14 this used to allow: the two badges share the
        // name column's 152px, and a full-width name would push them under
        // the Win column rather than truncating itself.
        if (name.size() > 9) name = name.substr(0, 9);
        cell(name.c_str(), colName, y, c);
        badges(PlatformTagFor(p, OwnsArray(p)), p.roundInput, colName + 68, y);
        snprintf(buf, sizeof(buf), "%d", p.winCount); cell(buf, colWin, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rFired);  cell(buf, colFired, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rPopped); cell(buf, colPopped, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rSent);   cell(buf, colSent, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rRecv);   cell(buf, colRecv, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rBlk);    cell(buf, colBlk, y, c);
        snprintf(buf, sizeof(buf), "%d", p.rKills);  cell(buf, colKills, y, c);
        y += rowH;
    }

    if (!teams.empty()) {
        cell("TEAM TOTALS", colName, y, hdr);
        y += rowH;
        for (int t : teams) {
            int tFired = 0, tPopped = 0, tSent = 0, tRecv = 0, tBlk = 0, tKills = 0;
            for (int i = 0; i < n; i++) {
                if (currentSettings.playerTeams[i] != t) continue;
                BubbleArray &p = bubbleArrays[i];
                tFired += p.rFired; tPopped += p.rPopped; tSent += p.rSent; tRecv += p.rRecv;
                tBlk += p.rBlk; tKills += p.rKills;
            }
            SDL_Color c = kTeamColors[t - 1];
            snprintf(buf, sizeof(buf), "TEAM %d", t); cell(buf, colName, y, c);
            snprintf(buf, sizeof(buf), "%d", tFired);  cell(buf, colFired, y, c);
            snprintf(buf, sizeof(buf), "%d", tPopped); cell(buf, colPopped, y, c);
            snprintf(buf, sizeof(buf), "%d", tSent);   cell(buf, colSent, y, c);
            snprintf(buf, sizeof(buf), "%d", tRecv);   cell(buf, colRecv, y, c);
            snprintf(buf, sizeof(buf), "%d", tBlk);    cell(buf, colBlk, y, c);
            snprintf(buf, sizeof(buf), "%d", tKills);  cell(buf, colKills, y, c);
            y += rowH;
        }
    }

    if (currentSettings.networkGame) {
        const char *hint = tournament
            ? "T / X: CHAT    ENTER / FIRE: BRACKET"
            : (waitingForOpponentNewGame
                ? "T / X: CHAT    WAITING FOR PLAYERS"
                : (gameMatchOver
                    ? "T / X: CHAT    ENTER: LOBBY"
                    : "T / X: CHAT    ENTER / FIRE: NEXT ROUND"));
        cell(hint, colName, y, hdr);
        if (discordAlertsApply) {
            cell("STATS POSTED TO DISCORD #now-playing CHANNEL",
                 colName, y + rowH, normal);
        }

        // Tappable CHAT button (touch devices have no T key). The rect itself
        // is computed by UpdateRoundStatsHitRects() (called separately, and
        // independent of whether this draw runs), so read the already-current
        // members here instead of assigning them inline.
        roundStatsOps.push_back({RoundStatsOpKind::ChatBtn, 0, statsChatBtn, {}});
        cell(chattingMode ? "SEND" : "CHAT", statsChatBtn.x + 24, statsChatBtn.y + 4, hdr);
        if (tournament) {
            roundStatsOps.push_back({RoundStatsOpKind::BracketBtn, 0, statsTournamentBtn, {}});
            cell("BRACKET", statsTournamentBtn.x + 20,
                 statsTournamentBtn.y + 4, hdr);
        }
    }
}

void BubbleGame::DrawRoundStats(SDL_Renderer *rend) {
    // Pure draw half of the round-stats table: panel background then the
    // cells/chips/buttons recorded by UpdateRoundStats() in the exact order
    // the old fused function drew them.
    if (currentSettings.playerCount < 2) return;

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 190);
    { SDL_FRect fr = ToFRect(statsPanelBox); SDL_RenderFillRect(rend, &fr); }
    SDL_SetRenderDrawColor(rend, 255, 255, 255, 90);
    { SDL_FRect fr = ToFRect(statsPanelBox); SDL_RenderRect(rend, &fr); }
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);

    for (const RoundStatsOp &op : roundStatsOps) {
        switch (op.kind) {
        case RoundStatsOpKind::Text:
        case RoundStatsOpKind::Chip: {
            if (op.kind == RoundStatsOpKind::Chip)
                DrawPlayerBadgeChip(rend, op.rect, op.badge);
            if (op.poolIdx < statsCellPool.size()) {
                TTFText &t = statsCellPool[op.poolIdx];
                if (t.Texture()) {
                    SDL_FRect fr = ToFRect(*t.Coords());
                    SDL_RenderTexture(rend, t.Texture(), nullptr, &fr);
                }
            }
            break;
        }
        case RoundStatsOpKind::ChatBtn: {
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(rend, 0, 0, 0, 190);
            { SDL_FRect fr = ToFRect(op.rect); SDL_RenderFillRect(rend, &fr); }
            SDL_SetRenderDrawColor(rend, 255, 255, 100, 200);
            { SDL_FRect fr = ToFRect(op.rect); SDL_RenderRect(rend, &fr); }
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
            break;
        }
        case RoundStatsOpKind::BracketBtn: {
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(rend, 255, 218, 92, 200);
            { SDL_FRect fr = ToFRect(op.rect); SDL_RenderRect(rend, &fr); }
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
            break;
        }
        }
    }
}

void BubbleGame::UpdateRoundStatsHitRects() {
    // Same early-return-untouched quirk as RenderRoundStats(): with fewer than
    // two players there is no stats panel, so whatever the rects held from a
    // previous frame is deliberately left alone rather than zeroed.
    if (currentSettings.playerCount < 2) return;

    const RoundStatsLayout layout = ComputeRoundStatsLayout(currentSettings, IsTournamentRound());
    if (currentSettings.networkGame) {
        statsChatBtn = {layout.boxX, layout.boxY + layout.boxH + 4, 88, 24};
        if (layout.tournament) {
            statsTournamentBtn = {statsChatBtn.x + statsChatBtn.w + 8,
                                  statsChatBtn.y, 112, statsChatBtn.h};
        } else {
            statsTournamentBtn = {0, 0, 0, 0};
        }
    } else {
        statsChatBtn = {0, 0, 0, 0};
        statsTournamentBtn = {0, 0, 0, 0};
    }
}

void BubbleGame::RenderMultiplayerResultPanel(SDL_Renderer *rend) {
    // Only two-player layouts have a winner-panel texture. A draw has no
    // roundWinnerIdx and must not fall back to the P1 texture.
    if (gameMpDone && currentSettings.playerCount == 2 &&
        roundWinnerIdx >= 0 && roundWinnerIdx < 2) {
        SDL_FRect fr = ToFRect(panelRct);
        SDL_RenderTexture(rend, multiStatePanels[roundWinnerIdx], nullptr, &fr);
    }
}

void BubbleGame::UpdateMultiplayerCompletionState() {
    if (!gameFinish || currentSettings.playerCount < 2) return;

    // Network rendering historically aggregates the two-player animation
    // pair. Local multiplayer extends the same rule to every reachable local
    // setup so HandleInput can consume the finished round -- bounded by the
    // seat cap rather than a literal, since a count this misses leaves the
    // round finished but impossible to move on from.
    const bool supported = currentSettings.playerCount == 2 ||
        (!currentSettings.networkGame && currentSettings.localMultiplayer &&
         currentSettings.playerCount <= kMaxLocalPlayers);
    if (!supported) return;

    gameMpDone = true;
    for (int i = 0; i < currentSettings.playerCount; ++i) {
        if (!bubbleArrays[i].mpDone) {
            gameMpDone = false;
            break;
        }
    }
}


// Mutator: ages and prunes inGameChatMessages. Split out of Render()'s inline
// chat-overlay block (R1d-ii) so a redundant/extra render never double-ages
// the overlay; call once per real simulation step in network games, after the
// draw, so the frame that shows a message about to expire is the same frame
// that then erases it (matching the original fused behavior exactly).
void BubbleGame::AgeChatMessages() {
    for (auto &msg : inGameChatMessages) msg.framesLeft--;
    inGameChatMessages.erase(
        std::remove_if(inGameChatMessages.begin(), inGameChatMessages.end(),
                       [](const InGameChatMsg &m) { return m.framesLeft <= 0; }),
        inGameChatMessages.end());
}

bool BubbleGame::AdvanceSimulation() {
    // Read once, here, and thread explicitly from here down -- AdvanceSimulation()
    // is the one place reading the live wall-clock singleton is legitimate; the
    // playback path (AdvancePlaybackStep) supplies the recorded values instead.
    // See docs/REPLAY_PLAN.md / docs/REPLAY_PROGRESS.md (StepContext, R5b).
    const float deltaScale = FrozenBubble::Instance()->deltaScale;
    const Uint32 gameClockMs = SDL_GetTicks();
    return AdvanceSimulationAtScale(deltaScale, gameClockMs);
}

bool BubbleGame::AdvancePlaybackStep(float recordedDeltaScale, Uint32 recordedGameClockMs) {
    // Same step body as the live path, but the scale and clock come from the
    // recording rather than the wall clock -- a replay must never re-derive
    // speed or deadlines from the viewer's current preferences or real time.
    // See docs/REPLAY_PROGRESS.md (R3, R5b).
    return AdvanceSimulationAtScale(recordedDeltaScale, recordedGameClockMs);
}

bool BubbleGame::AdvanceSimulationAtScale(float deltaScale, Uint32 gameClockMs) {
    stepDeltaScale = deltaScale;
    stepGameClockMs = gameClockMs;
    // Monotonic step index for the replay recorder -- see simStep's
    // declaration comment in bubblegame.h for why this is separate from
    // frameCount below.
    simStep++;

    // Process network messages if this is a network game
    if (currentSettings.networkGame) {
        if (sessionMode == SessionMode::Playback) {
            // Offline replay: never pump the socket or the bot sockets. The
            // recorded inbound payloads for this step are drained through the
            // exact same handler the live path uses, in capture order, before
            // the step's physics -- matching where ProcessNetworkMessages()
            // runs on the live path. A malformed/empty blob simply applies
            // nothing. See docs/REPLAY_PROGRESS.md (R6a).
            for (const InboundGameEvent &event : stepInboundEvents)
                ApplyInboundGameMessage(event.senderId, event.gameData);
            stepInboundEvents.clear();
        } else {
            stepInboundEvents.clear();
            ProcessNetworkMessages();
            NetworkClient* net = NetworkClient::Existing();
            if (IsTournamentRound() && net &&
                net->tournaments.assignment.tournament &&
                net->tournaments.assignment.returned) {
                QuitToTitle();
                return false;
            }

            // Send ping every second to prevent idle timeout (60 FPS = 60 frames/sec)
            // This matches the original Perl implementation which sends 'p' every second
            networkFrameCounter++;
            if (networkFrameCounter >= 60) {
                networkFrameCounter = 0;
                if (EffectsEnabled()) {
                    NetworkClient* netClient = NetworkClient::Instance();
                    if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                        netClient->SendGameData("p");
                    }
                }
            }

            // Check if both players are ready for new game after round ends
            if (waitingForOpponentNewGame && opponentReadyForNewGame) {
                // Joiner (async networking handoff, stage 3b, extending 3c's fix
                // to this second call site): wait here, across frames, until all
                // 40 of round 2+'s level-sync messages are queued, before
                // handing off to ReloadGame -> SyncNetworkLevel -> WaitForBubble
                // -- same rationale and same trick as the initial game-start
                // gate in mainmenu_netpanel.cpp (see the long comment there).
                //
                // This site's gate was never given stage 3c's fix and carried
                // the same bug on its own: it checked only
                // NetworkClient::MessageQueueSize(), but ProcessNetworkMessages()
                // has already been draining 'b|'/'N'/'T' into syncQueue for the
                // entire match by the time round 2 starts, so the main queue
                // alone could never reach 40 -- every round after the first
                // burned this gate's full 5s timeout before proceeding, on both
                // platforms, independent of the lobby-entry gate's own fix.
                NetworkClient* netClientRound = NetworkClient::Instance();
                if (netClientRound && !netClientRound->IsLeader()) {
                    if (roundSyncWaitStart == 0) roundSyncWaitStart = SDL_GetTicks();
                    const size_t qSize = netClientRound->MessageQueueSize();
                    const size_t sSize = netClientRound->SyncQueueSize();
                    const Uint64 waited = SDL_GetTicks() - roundSyncWaitStart;
                    SDL_Log("Round sync wait: queue=%d sync=%d waited=%dms", (int)qSize, (int)sSize, (int)waited);
                    if (ShouldKeepWaitingForLevelSync(qSize, sSize, waited, 5000)) {
                        return false;  // come back next frame
                    }
                    SDL_Log("Round sync: proceeding queue=%d sync=%d waited=%dms", (int)qSize, (int)sSize, (int)waited);
                    roundSyncWaitStart = 0;
                }
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
                // TEMP-FIX-TEST: bail out of this frame immediately after an
                // in-frame round transition, so this frame produces only a
                // RoundStart record and the new round's first real step (and
                // its StepRecord/Assertion) happens on the NEXT call instead
                // of the same already-incremented simStep.
                return true;
            }
        }  // end live-only network I/O

        // Increment global frame counter (used by malus timing)
        frameCount++;
        // NOTE: ProcessMalusQueue for local player is called inside the stick handler
        // (after bubble sticks), matching original Perl behavior at line 2217-2258.
        // This ensures malus only falls AFTER the local player fires and their bubble sticks.
    } else {
        // A local round never produces inbound network payloads, but a
        // BubbleGame instance is reused across matches and can go network ->
        // local. Clear whatever the last network step left in stepInboundEvents
        // so replay capture does not record a stale payload against a local
        // step. (Its declaration comment already promises a per-step clear;
        // this is that clear for the non-network path.)
        stepInboundEvents.clear();
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

    // Race / Timed (gamemode.h). Both run after the network pump above so the
    // counts they read already include everything that arrived this frame, and
    // before the per-mode simulation below so a round that ends on this frame
    // is treated as finished rather than a frame late.
    //
    // BroadcastPoppedCounts is not gated on the mode: the popped HUD is shown
    // in every multiplayer mode, and it sends nothing on a frame where no
    // count moved.
    if (!gameFinish && currentSettings.playerCount >= 2) {
        BroadcastPoppedCounts();
        UpdateTimedRound();
    }

    // Multiplayer training mode: periodically inject random malus, enforce 2-min timer
    if (currentSettings.mpTraining && !gameFinish) {
        if (mpTrainStartTime == 0) mpTrainStartTime = stepGameClockMs;
        Uint32 elapsed = stepGameClockMs - mpTrainStartTime;
        const Uint32 TRAIN_DURATION = 120 * 1000;  // 2 minutes in ms

        if (!mpTrainDone && elapsed >= TRAIN_DURATION) {
            // Time's up — show score and end
            mpTrainDone = true;
            gameFinish = true;
            gameWon = true;
            // Store training score as level=101 (sentinel for mp_train) with time=score.
            // Same keyboard/gamepad-vs-mouse/touch split and mixed-input
            // disqualification as the classic solo path -- see
            // BubbleGame::ScoringInputMethod and its lock/disqualify site in
            // bubblegame_shooter.cpp, which applies here too since mp_train
            // is single-player and not a network game. Playback suppresses the
            // highscore write but keeps the in-memory score/win state.
            if (EffectsEnabled() && !scoringDisqualified) {
                HighscoreManager::InputMethod method =
                    (scoringInputMethod == ScoringInputMethod::Mouse) ? HighscoreManager::InputMethod::Mouse
                                                                        : HighscoreManager::InputMethod::Keyboard;
                if (HighscoreManager::Instance()->CheckAndAddScore(mpTrainScore, 0.0f, method))
                    pendingHighscore = true;
            }
        } else if (!mpTrainDone) {
            // Randomly inject malus rows (original: rand($mptrainingdiff*(1000/$TARGET_ANIM_SPEED)) == 0)
            // mptrainingdiff default = 30 seconds between attacks; at 60fps: 30*60=1800 frames avg
            BubbleArray &arr = bubbleArrays[0];
            if (arr.malusQueue.empty()) {
                int roll = rng.Range(0, 1799);
                if (roll == 0) {
                    int count = 1 + rng.Range(0, 5);
                    for (int i = 0; i < count; i++)
                        arr.malusQueue.push_back(frameCount);
                }
            }
        }
    }

    if(playedPause) {
        // Not while muted: now that muting pauses the track rather than stopping
        // it, an unguarded resume here would start the music playing again on
        // unpause even though the player had muted it.
        if (!audMixer->IsHalted()) audMixer->ResumeMusic();
        playedPause = false;
        Uint32 pausedFor = stepGameClockMs - timePaused;
        FrozenBubble::Instance()->startTime += pausedFor;
        // The training clock needs the same correction as the highscore timer
        // above, or a paused game burns its two minutes while nothing moves.
        if (mpTrainStartTime > 0) mpTrainStartTime += pausedFor;
        // Same for a Timed round's clock, and for the leader's deadline for
        // hearing everyone's final count -- a pause during that window would
        // otherwise expire it and rank the round on whoever had reported so far.
        if (modeTimerStart > 0) modeTimerStart += pausedFor;
        if (modeTimerDeadline > 0) modeTimerDeadline += pausedFor;
    }

    // Roll up per-round stats once when a multiplayer round ends (also broadcasts 'S').
    if (gameFinish && !roundStatsFinalized && currentSettings.playerCount >= 2) {
        FinalizeRoundStats();
        roundStatsFinalized = true;
    }

    if(currentSettings.playerCount == 1) {
        BubbleArray &curArray = bubbleArrays[0];

        if (curArray.turnsToCompress <= 2) {
            DoPrelightAnimation(curArray, curArray.prelightTime);
        }

        // Stick effect animation (original: $sticking_bubble / sticking_step)
        if (curArray.stickAnimActive) {
            if (++curArray.stickAnimSlowdown >= 2) {
                curArray.stickAnimSlowdown = 0;
                if (++curArray.stickAnimFrame > BUBBLE_STICKFC) curArray.stickAnimActive = false;
            }
        }

        if(gameFinish) {
            if (!gameWon && !gameLost) DoFrozenAnimation(curArray, curArray.frozenWait);
        }

        // Unconditional, matching the >=2-player branch below (see its own
        // comment on this same call): UpdateSingleBubbles also drives
        // malusBubbles toward their stick position (bubblegame_shooter.cpp,
        // UpdateSingleBubblesAtScale), and mp_train can have malus on screen
        // with singleBubbles empty -- between shots, nothing is currently
        // launching. Gating this call on singleBubbles.size() > 0 stalled
        // that malus animation dead in place until the next shot fired a
        // new SingleBubble and this ran again, which is what "malus pauses
        // and only continues once I shoot" was live.
        UpdateSingleBubblesAtScale(deltaScale);

        UpdatePenguin(curArray, deltaScale);

        // Combo text is a draw-then-age crossover: the blit lives in Draw(),
        // so record its pre-decrement visibility here and age it exactly once
        // per simulation step (R1d-iv).
        comboTextVisible = (comboDisplayTimer > 0);
        if (comboDisplayTimer > 0) comboDisplayTimer--;
    }
    else { //iterate until all penguins & status are advanced
        // Update ALL players' bubbles ONCE before the per-player loop (original: iter_players at line 2105)
        // This ensures all players are processed in a single unified loop
        UpdateSingleBubblesAtScale(deltaScale);  // processes all players' bubbles once

        for (int i = 0; i < currentSettings.playerCount; i++) {
            BubbleArray &curArray = bubbleArrays[i];

            // Use mini textures for remote players (playerAssigned >= 1) in 3-5 player games
            bool useMini = (currentSettings.playerCount >= 3 && curArray.playerAssigned >= 1);

            if (curArray.turnsToCompress <= 2) {
                DoPrelightAnimation(curArray, curArray.prelightTime);
            }

            // Stick effect animation (original: $sticking_bubble / sticking_step).
            // The original fused block was gated on boardVisible, so the age
            // keeps that gate -- a paged-out board's stick animation does not
            // advance while it is off-screen.
            if (curArray.boardVisible) {
                if (curArray.stickAnimActive) {
                    if (++curArray.stickAnimSlowdown >= 2) {
                        curArray.stickAnimSlowdown = 0;
                        if (++curArray.stickAnimFrame > BUBBLE_STICKFC) curArray.stickAnimActive = false;
                    }
                }
            }

            if(gameFinish) {
                if (!curArray.mpWinner) DoFrozenAnimation(curArray, curArray.frozenWait);
                else {
                    DoWinAnimation(curArray, curArray.explodeWait);
                }
            } else if (curArray.playerState == BubbleArray::PlayerState::LOST) {
                // Player died mid-round while others are still playing (3-5 player games):
                // progressively freeze their board so the death is visually indicated,
                // matching the original Perl's update_lost() (frozen-bubble line 2007/2106).
                DoFrozenAnimation(curArray, curArray.frozenWait);
            }

            UpdatePenguin(curArray, deltaScale);

            // Blinking yellow border on any mini-board that was actually
            // attacked. Split into a pure draw (DrawAttackFlash) and a
            // mutator (AgeAttackFlash) (R1d-ii); the mutator keeps the exact
            // gating (and paged-out-board quirk) documented on the draw.
            AgeAttackFlash(curArray, useMini);
        }

        // Check all players for danger zone every frame in multiplayer (original: verify_if_end() at line 2319)
        // Original checks: if ($pdata{state} eq 'game' && any { $_->{cy} > 11 })
        // Only check while global game state is "game" (not finished/won).
        // This sweep must run for local multiplayer too, not just network games: malus sticks
        // deliberately don't call CheckGameState (to avoid double-counting the compressor/new-root
        // counter), so this every-frame sweep is the only way to catch a local player pushed into
        // the danger zone by incoming malus between their own shots.
        if (!gameFinish && currentSettings.playerCount >= 2) {
            ResolveDangerZoneLosses();
        }

        if (gameFinish) {
            UpdateMultiplayerCompletionState();
        }

        // Incoming-malus toasts ("who hit you and how many"); fade out during play.
        // Draw lives in Draw(); this is the per-step aging half (R1d-ii).
        if (!gameFinish) {
            AgeMalusAlerts();
        }
    }

    // In-game chat overlay aging, gated only on networkGame, same as the draw
    // (R1d-ii). The draw half lives in Draw().
    if (currentSettings.networkGame) {
        AgeChatMessages();
    }

    // R4a: hand the completed step to the replay recorder. Placed at the single
    // successful-exit point, after every mutator above has run, so a recorded
    // step is exactly the state the next step starts from. No-op unless a
    // recorder exists and is capturing.
    //
    // The two `return false` paths earlier (tournament return, round-sync wait)
    // never reach here, so they contribute no StepRecord even though simStep
    // already advanced and -- on both paths -- real mutation already happened
    // (QuitToTitle() on the first, ProcessNetworkMessages() on the second).
    // That is a deliberate limitation, not an accident: both paths only run
    // once the round they belong to is already over, so the only recording they
    // can affect is a network result tail still waiting for a late 'S'. Such a
    // tail loses whatever arrived during the gap and then seals as incomplete
    // at the next round boundary. The sealed recording stays self-consistent --
    // its step list is a prefix and its round-end snapshot matches its own last
    // captured step -- so playback never diverges; it is only ever less
    // complete than the live round was. Capturing these steps properly would
    // mean splitting the two early returns into "mutated, then bailed", which
    // belongs with the seek/checkpoint work, not here.
    if (ReplayRecorder *recorder = ReplayRecorder::Existing()) recorder->OnStep(*this);

    return true;
}

void BubbleGame::Draw() {
    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);
    SDL_RenderTexture(rend, background, nullptr, nullptr);

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

        SDL_Texture** useBubbles = GetBubbleTextures();
        for (const std::vector<Bubble> &vecBubble : curArray.bubbleMap) for (Bubble bubble : vecBubble) bubble.Render(rend, useBubbles, imgBubblePrelight, imgBubbleFrozen);

        // Stick effect animation (original: $sticking_bubble / sticking_step)
        if (curArray.stickAnimActive) {
            SDL_Rect sr = {curArray.stickAnimPos.x - 16, curArray.stickAnimPos.y - 16, 32, 32};
            { SDL_FRect fr = ToFRect(sr); SDL_RenderTexture(rend, imgBubbleStick[curArray.stickAnimFrame], nullptr, &fr); }
        }

        if(gameFinish) {
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

        if (singleBubbles.size() > 0) {
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

        DrawHurryWarning(rend, curArray);
        if(!lowGfx) curArray.penguinSprite.Render();
        curArray.shooterSprite.Render(lowGfx);
        // Redraw the current bubble on top of the shooter/cannon sprite -- the
        // cannon graphic is large enough to cover most of it, otherwise making
        // the loaded bubble's color hard to see while aiming.
        { SDL_FRect fr = ToFRect(curArray.curLaunchRct); SDL_RenderTexture(rend, gameFinish && !gameWon ? imgBubbleFrozen : useBubbles[curArray.curLaunch], nullptr, &fr); }
        if (curArray.aimGuideEnabled && !gameFinish) {
            bool isMini = (currentSettings.playerCount >= 3 && curArray.playerAssigned >= 1);
            DrawAimGuide(rend, curArray, isMini, stepDeltaScale);
        }
        { SDL_FRect fr = ToFRect(*inGameText.Coords()); SDL_RenderTexture(rend, inGameText.Texture(), nullptr, &fr); }

        // Display score: recompute the string, then blit it (R1d-iv a, split
        // so a redundant/extra render cannot recompute the texture).
        UpdateScoreText(curArray, 0);
        DrawScoreText(0);

        // Multiplayer training: show countdown timer and training score
        if (currentSettings.mpTraining && mpTrainStartTime > 0) {
            const Uint32 TRAIN_DURATION = 120 * 1000;
            Uint32 elapsed = stepGameClockMs - mpTrainStartTime;
            int remaining = (elapsed < TRAIN_DURATION) ? (int)((TRAIN_DURATION - elapsed) / 1000) : 0;
            int m = remaining / 60;
            int s = remaining % 60;
            char trainBuf[64];
            snprintf(trainBuf, sizeof(trainBuf), "%d'%02d\"  Score: %d", m, s, mpTrainScore);
            mpTrainText.UpdateText(renderer, trainBuf, 0);
            mpTrainText.UpdatePosition({32, 177});
            { SDL_FRect fr = ToFRect(*mpTrainText.Coords()); SDL_RenderTexture(rend, mpTrainText.Texture(), nullptr, &fr); }
        }

        // Display combo text while the timer is active (aged by AdvanceSimulation)
        if (comboTextVisible) {
            { SDL_FRect fr = ToFRect(*comboText.Coords()); SDL_RenderTexture(rend, comboText.Texture(), nullptr, &fr); }
        }
    }
    else { //iterate until all penguins & status are rendered
        for (int i = 0; i < currentSettings.playerCount; i++) {
            BubbleArray &curArray = bubbleArrays[i];

            // Use mini textures for remote players (playerAssigned >= 1) in 3-5 player games
            bool useMini = (currentSettings.playerCount >= 3 && curArray.playerAssigned >= 1);
            SDL_Texture** useBubbles = GetBubbleTextures(useMini);
            SDL_Texture* useFrozen = useMini ? imgMiniBubbleFrozen : imgBubbleFrozen;
            SDL_Texture* usePrelight = useMini ? imgMiniBubblePrelight : imgBubblePrelight;

            // >5-player royale: boards paged out of view (BubbleGame::netViewPage) skip all
            // draw-only work below. Simulation (AdvanceSimulation) still runs
            // unconditionally for every player every frame so hidden boards keep
            // playing and can still finish/die/win off-screen.
            // No-op for <=5-player games since ApplyNetViewPage() marks everything visible.
            if (curArray.boardVisible) {
                SDL_Rect rct;
                for (int i = 1; i < 13; i++) {
                    rct.x = curArray.rightLimit;
                    rct.y = 104 - (7 * i) - i;
                    rct.w = rct.h = 7;
                    { SDL_FRect fr = ToFRect(rct); SDL_RenderTexture(rend, dotTexture[i == curArray.turnsToCompress ? 1 : 0], nullptr, &fr); }
                }

                // Don't render shooter bubbles for LOST players (prevents crashes from invalid bubble indices)
                // In network games, losing players become spectators and shouldn't have active bubbles
                if (curArray.playerState != BubbleArray::PlayerState::LOST) {
                    { SDL_FRect fr = ToFRect(curArray.curLaunchRct); SDL_RenderTexture(rend, gameFinish && !curArray.mpWinner ? useFrozen : useBubbles[curArray.curLaunch], nullptr, &fr); }
                    { SDL_FRect fr = ToFRect(curArray.nextBubbleRct); SDL_RenderTexture(rend, useBubbles[curArray.nextBubble], nullptr, &fr); }
                    { SDL_FRect fr = ToFRect(curArray.onTopRct); SDL_RenderTexture(rend, onTopTexture, nullptr, &fr); }
                }
                if ((gameFinish && !curArray.mpWinner) || curArray.playerState == BubbleArray::PlayerState::LOST) { SDL_FRect fr = ToFRect(curArray.frozenBottomRct); SDL_RenderTexture(rend, useFrozen, nullptr, &fr); }
            }

            if (curArray.boardVisible) {
                for (const std::vector<Bubble> &vecBubble : curArray.bubbleMap) for (Bubble bubble : vecBubble) bubble.Render(rend, useBubbles, usePrelight, useFrozen);

                // Stick effect animation (original: $sticking_bubble / sticking_step)
                if (curArray.stickAnimActive) {
                    SDL_Texture* stickTex = useMini ? imgMiniBubbleStick[curArray.stickAnimFrame] : imgBubbleStick[curArray.stickAnimFrame];
                    int sz = useMini ? 16 : 32;
                    SDL_Rect sr = {curArray.stickAnimPos.x - sz/2, curArray.stickAnimPos.y - sz/2, sz, sz};
                    { SDL_FRect fr = ToFRect(sr); SDL_RenderTexture(rend, stickTex, nullptr, &fr); }
                }
            }

            DrawHurryWarning(rend, curArray);
            if (curArray.boardVisible) {
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
                    DrawAimGuide(rend, curArray, isMini, stepDeltaScale);
                }
            }

            // Physics already ran for every board in AdvanceSimulation()
            // (UpdateSingleBubblesAtScale), once before the per-player loop,
            // so nothing here re-advances projectiles.

            // Display score with nickname for each player (original: print_scores at line 1868)
            // In 3+ player games, skip score text — win counts are shown via UpdatePlayerNameWinText
            // at the same screen positions, so rendering both would cause overlapping text.
            if (curArray.boardVisible && currentSettings.playerCount < 3) {
                UpdateScoreText(curArray, i);
                DrawScoreText(i);
            }
            // Drawn for every visible board in every multiplayer mode, on both
            // sides of the <3 split above: the 2-player layout gets it under
            // the score banner, the 3-5 player one under the name caption.
            if (curArray.boardVisible) {
                UpdatePoppedText(curArray, i);
                DrawPoppedText(i);
            }

            // Display "left" overlay for players who actually disconnected (original line 1951-1955)
            // NOTE: LOST = died (still in game), LEFT = disconnected. Only show for LEFT.
            if (curArray.boardVisible && currentSettings.networkGame && curArray.playerAssigned >= 1 &&
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
            // >5-player rooms always use one-target attacks and the slot-relative picker is
            // active regardless of the singlePlayerTargetting toggle (bubblegame_input.cpp),
            // so the indicator must show up in that case too, independent of the toggle.
            if ((currentSettings.singlePlayerTargetting || currentSettings.playerCount > 5) &&
                sendMalusToOne == i && curArray.boardVisible &&
                currentSettings.playerCount <= 5 &&
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
            } else if ((currentSettings.singlePlayerTargetting || currentSettings.playerCount > 5) &&
                       sendMalusToOne == i && curArray.boardVisible &&
                       currentSettings.playerCount > 5 && curArray.playerAssigned >= 1) {
                // >5-player royale: multiple arrays can share the same physical mini-slot
                // across pages, so key the attack icon off parkedSlot instead of array index.
                static const SDL_Point attackPos[4] = {{25, 213}, {496, 214}, {24, 442}, {496, 442}};
                int rpIdx = curArray.parkedSlot;
                if (rpIdx >= 0 && rpIdx < 4 && imgAttack[rpIdx]) {
                    SDL_Rect attackRct;
                    { float fw, fh; SDL_GetTextureSize(imgAttack[rpIdx], &fw, &fh); attackRct.w = (int)fw; attackRct.h = (int)fh; }
                    attackRct.x = attackPos[rpIdx].x;
                    attackRct.y = attackPos[rpIdx].y;
                    { SDL_FRect fr = ToFRect(attackRct); SDL_RenderTexture(rend, imgAttack[rpIdx], nullptr, &fr); }
                }
            }

            // Blinking yellow border on any mini-board that was actually attacked.
            // Pure draw half (R1d-ii); the aging half is in AdvanceSimulation().
            DrawAttackFlash(rend, curArray, useMini);

            // Show targeting text: who this player is targeting
            if (curArray.boardVisible &&
                (currentSettings.singlePlayerTargetting || currentSettings.playerCount > 5) && !gameFinish &&
                playerTargeting[i] >= 0 && playerTargeting[i] < currentSettings.playerCount) {
                const std::string& targetNick = bubbleArrays[playerTargeting[i]].playerNickname;
                if (!targetNick.empty()) {
                    char tgtBuf[64];
                    snprintf(tgtBuf, sizeof(tgtBuf), "> %s", targetNick.c_str());
                    targetingText[i].UpdateText(renderer, tgtBuf, 0);
                    // Position: near each player's shooter area
                    int tx, ty;
                    if (curArray.playerAssigned == 0) {
                        tx = curArray.shooterSprite.rect.x + curArray.shooterSprite.rect.w / 2 - 30;
                        ty = curArray.shooterSprite.rect.y - 20;
                    } else {
                        tx = curArray.shooterSprite.rect.x;
                        ty = curArray.shooterSprite.rect.y + curArray.shooterSprite.rect.h;
                    }
                    targetingText[i].UpdatePosition({tx, ty});
                    { SDL_FRect fr = ToFRect(*targetingText[i].Coords()); SDL_RenderTexture(rend, targetingText[i].Texture(), nullptr, &fr); }
                }
            }
        }

        // Render "attackme" indicators on local player board when opponents are targeting us
        // (original: redraw_attackingme() at line 1345)
        // attackme position from Stuff.pm: p1 attackme={185, 448}, each attacker offset by 24px
        if (currentSettings.singlePlayerTargetting && !attackingMe.empty() && !gameFinish) {
            for (size_t k = 0; k < attackingMe.size(); k++) {
                int attackerArray = attackingMe[k];
                if (attackerArray < 0 || attackerArray >= currentSettings.playerCount) continue;
                int rpIdx;
                if (currentSettings.playerCount <= 5) {
                    if (attackerArray < 1 || attackerArray > 4) continue;
                    rpIdx = attackerArray - 1;
                } else {
                    // >5-player royale: the attacker's board must be on the current view
                    // page, and multiple arrays can share a slot, so key off parkedSlot.
                    const BubbleArray &attacker = bubbleArrays[attackerArray];
                    if (!attacker.boardVisible) continue;
                    rpIdx = attacker.parkedSlot;
                    if (rpIdx < 0 || rpIdx > 3) continue;
                }
                if (imgAttackMe[rpIdx]) {
                    SDL_Rect amRct;
                    { float fw, fh; SDL_GetTextureSize(imgAttackMe[rpIdx], &fw, &fh); amRct.w = (int)fw; amRct.h = (int)fh; }
                    amRct.x = 185 + ((int)k * 24);
                    amRct.y = 448;
                    { SDL_FRect fr = ToFRect(amRct); SDL_RenderTexture(rend, imgAttackMe[rpIdx], nullptr, &fr); }
                }
            }
        }

        if (gameFinish) {
            if (currentSettings.playerCount == 2)
                RenderMultiplayerResultPanel(rend);
        }

        if(singleBubbles.size() > 0) {
            SDL_Texture** useBubbles = GetBubbleTextures();
            for (SingleBubble &bubble : singleBubbles) {
                // >5-player royale: only draw bubbles belonging to a board on the current
                // view page. Physics (UpdatePosition) already ran unconditionally above.
                if (!bubbleArrays[bubble.assignedArray].boardVisible) continue;
                bubble.Render(rend, useBubbles);
            }
        }

        // Render malus bubbles (attack bubbles)
        if(malusBubbles.size() > 0) {
            for (MalusBubble &malus : malusBubbles) {
                if (!bubbleArrays[malus.assignedArray].boardVisible) continue;
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

            // 3-5 player mode: show player name and win count. Measure the
            // badges first, then blit name and badges -- the measure pass
            // allocates the badge cells the pure DrawLiveBadges() reads.
            size_t badgeIdx = 0;
            for (int i = 0; i < currentSettings.playerCount; i++) {
                if (!bubbleArrays[i].boardVisible) continue;
                if (playerNameWinText[i].Texture()) {
                    MeasureLiveBadges(bubbleArrays[i], badgeIdx);
                    { SDL_FRect fr = ToFRect(*playerNameWinText[i].Coords()); SDL_RenderTexture(rend, playerNameWinText[i].Texture(), nullptr, &fr); }
                    // Under the name rather than after it: these boards are
                    // centred on a fixed slot position, so anything appended
                    // horizontally would either overhang the board's edge or
                    // shift the name off its own centre.
                    const SDL_Rect *nameRect = playerNameWinText[i].Coords();
                    DrawLiveBadges(nameRect->x, nameRect->y + nameRect->h + 1);
                }
            }
        }

        // >5-player royale HUD: alive count + page indicator. No-op for <=5 players.
        if (currentSettings.playerCount > 5) {
            UpdateRoyaleHud();
            DrawRoyaleHud(rend);
        }

        // Prominent round-end banner naming the winner, shown for every mode
        // (previously Clear-only). Positioned above panelRct so it never
        // overlaps the 2P win-panel image or the 3-5P name/win-count text.
        if (gameFinish && roundWinnerIdx >= 0) {
            // roundWinnerIdx is the player who actually won the round --
            // CommitRoundWin sets it once, from the asserted winner, and never
            // from a teammate it also credits. A scan for the first mpWinner
            // instead (the previous approach) found whichever teammate
            // happened to sit at the lowest array index, not necessarily the
            // one who won anything, and named only that one player even
            // when the win belonged to their whole team (found live: "clear
            // mode with teams set doesn't attribute win to team").
            int winnerIdx = roundWinnerIdx;

            // The "how" prefix is recomputed fresh from live state every
            // frame rather than read back from a transmitted RoundWinCause:
            // the wire protocol's 'F' message is just "F<nickname>", with no
            // room to carry Race/Timed as a distinct cause without breaking
            // old clients, so a remote peer's 'F' handler can only tag a win
            // Clear or Remote (see bubblegame_net.cpp). Recomputing here
            // instead works identically on every client, self-corrects on
            // any one-frame lag between a win being announced and its final
            // popped-count landing, and stays correct for as long as
            // gameFinish holds the round-end screen up.
            const char *prefix = "";
            if (wonByClearing) {
                prefix = "Board Cleared! ";
            } else if (currentSettings.gameMode == GameMode::Race &&
                       bubbleArrays[winnerIdx].rPopped >= currentSettings.raceTarget) {
                prefix = "First to Pop! ";
            } else if (currentSettings.gameMode == GameMode::Timed && modeTimerExpired) {
                prefix = "Time's Up! ";
            }

            char banner[160];
            const int winningTeam = currentSettings.playerTeams[winnerIdx];
            if (winningTeam != kNoTeam) {
                snprintf(banner, sizeof(banner), "%sTeam %d Wins!", prefix, winningTeam);
            } else {
                snprintf(banner, sizeof(banner), "%s%s Wins!", prefix,
                         StatsPlayerName(bubbleArrays[winnerIdx], winnerIdx, currentSettings.networkGame).c_str());
            }
            clearWinText.UpdateText(renderer, banner, 0);
            clearWinText.UpdatePosition({SCREEN_CENTER_X - (clearWinText.Coords()->w / 2), 165});
            if (clearWinText.Texture()) {
                // Semi-transparent plate behind the banner: the ring outline
                // alone still washed out over a light or similarly-colored
                // patch of board, so give the whole banner its own contrast
                // floor independent of what's underneath.
                SDL_Rect plate = *clearWinText.Coords();
                plate.x -= 16; plate.y -= 8; plate.w += 32; plate.h += 16;
                SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(rend, 0, 0, 0, 150);
                SDL_FRect plateFr = ToFRect(plate);
                SDL_RenderFillRect(rend, &plateFr);

                SDL_FRect fr = ToFRect(*clearWinText.Coords());
                SDL_RenderTexture(rend, clearWinText.Texture(), nullptr, &fr);
            }
        }

        // Incoming-malus toasts ("who hit you and how many"); fade out during play.
        // Recompute + draw live here; the per-step aging half is in
        // AdvanceSimulation().
        if (!gameFinish) {
            UpdateMalusAlerts();
            DrawMalusAlerts(rend);
        }

        // Post-round stats table (multiplayer): shown while the round-end screen is up.
        // The hit-test rects are computed independently of the draw so tap
        // targets stay correct regardless of render cadence (R1d-iii).
        if (gameFinish) {
            UpdateRoundStatsHitRects();
            UpdateRoundStats();
            DrawRoundStats(rend);
        }
    }

    // In-game chat overlay (network games only)
    if (currentSettings.networkGame) {
        if (!inGameChatMessages.empty() || chattingMode) {
            const int lineH   = 18;
            const int maxShow = kMaxChatLines;
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
                chatInputText.UpdateText(renderer, inputLine, 630);
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
                // One slot per displayed line (bounded by maxShow, i.e. kMaxChatLines) so
                // a line whose text is unchanged from last frame keeps its texture,
                // instead of one shared object invalidating on every other line's text.
                int slot = i - start;
                chatLineText[slot].UpdateText(renderer, lineBuf, 630);
                chatLineText[slot].UpdatePosition({chatX, baseY + slot * lineH});
                if (chatLineText[slot].Texture())
                    { SDL_FRect fr = ToFRect(*chatLineText[slot].Coords()); SDL_RenderTexture(rend, chatLineText[slot].Texture(), nullptr, &fr); }
            }
        }
    }

    if (!firstRenderDone) {
        TransitionManager::Instance()->TakeSnipOut(rend);
        firstRenderDone = true;
    }
}

void BubbleGame::Render() {
    if (AdvanceSimulation()) {
        Draw();
    } else {
        // The tournament-return and round-sync-wait paths advance the frame
        // but skip presentation beyond the background, exactly as the old
        // fused Render()'s early returns did (the background blit used to
        // precede both early returns).
        SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);
        SDL_RenderTexture(rend, background, nullptr, nullptr);
    }
}




void BubbleGame::RenderPaused() {
    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);

    if(!playedPause) {
        audMixer->PauseMusic();
        PlaySFX("pause");
        playedPause = true;
        pauseFrame = 0;

        if(prePauseBackground != nullptr) SDL_DestroyTexture(prePauseBackground);

        SDL_Surface *sfc = SDL_RenderReadPixels(rend, NULL);
        prePauseBackground = SDL_CreateTextureFromSurface(rend, sfc);
        SDL_DestroySurface(sfc);
    }

    // Black, so the letterbox bars stay black behind the pause overlay too.
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
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
