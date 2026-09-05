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

// PlayerSlotPosition (bubblegame_render.cpp) picks the screen position for a
// player's name/win-count label. It used to be a switch(playerCount) inline
// in UpdatePlayerNameWinText with three near-identical case bodies (3/4/5
// players) plus a royale default; it's now one 5-slot table plus an
// index-selection rule. This is a pure function of three ints -- no
// renderer, no BubbleGame instance -- so it's tested directly rather than
// through the friend-accessor pattern the render-side tests use.
//
// Pins: the 3/4/5-player fixed layouts key strictly off array index (not
// parkedSlot); royale (any other playerCount) keys off parkedSlot for every
// player but player 0, who always gets the fixed center slot; and an
// out-of-range parkedSlot in royale falls back to slot 1's position, not
// slot 0's -- the original code's own fallback, easy to get backwards in a
// refactor since slot 0 is also "the fallback-ish" center slot.

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

static bool SamePoint(SDL_Point a, SDL_Point b) { return a.x == b.x && a.y == b.y; }

int main() {
    const SDL_Point kCenter = {320, 12};
    const SDL_Point kTopLeft = {83, 2};
    const SDL_Point kTopRight = {553, 2};
    const SDL_Point kBottomLeft = {83, 465};
    const SDL_Point kBottomRight = {553, 465};

    // 3-player: array order is display order (center, top-left, top-right).
    CHECK(SamePoint(PlayerSlotPosition(3, 0, /*parkedSlot=*/-1), kCenter));
    CHECK(SamePoint(PlayerSlotPosition(3, 1, /*parkedSlot=*/-1), kTopLeft));
    CHECK(SamePoint(PlayerSlotPosition(3, 2, /*parkedSlot=*/-1), kTopRight));

    // 4-player: adds bottom-left.
    CHECK(SamePoint(PlayerSlotPosition(4, 3, /*parkedSlot=*/-1), kBottomLeft));

    // 5-player: adds bottom-right. parkedSlot is ignored for 3-5 players --
    // passing a value that would mean something different in royale must not
    // perturb the fixed-layout result.
    CHECK(SamePoint(PlayerSlotPosition(5, 4, /*parkedSlot=*/2), kBottomRight));

    // Royale (>5 players): player 0 always gets the fixed center slot,
    // regardless of its parkedSlot value.
    CHECK(SamePoint(PlayerSlotPosition(6, 0, /*parkedSlot=*/3), kCenter));

    // Royale: every other player keys off parkedSlot (0-3), not array index.
    CHECK(SamePoint(PlayerSlotPosition(8, 5, /*parkedSlot=*/0), kTopLeft));
    CHECK(SamePoint(PlayerSlotPosition(8, 5, /*parkedSlot=*/1), kTopRight));
    CHECK(SamePoint(PlayerSlotPosition(8, 5, /*parkedSlot=*/2), kBottomLeft));
    CHECK(SamePoint(PlayerSlotPosition(8, 5, /*parkedSlot=*/3), kBottomRight));

    // Royale: an out-of-range parkedSlot (defensive -- shouldn't happen in
    // practice) falls back to the top-left slot, matching the original
    // switch's own local fallback.
    CHECK(SamePoint(PlayerSlotPosition(6, 1, /*parkedSlot=*/-1), kTopLeft));
    CHECK(SamePoint(PlayerSlotPosition(6, 1, /*parkedSlot=*/4), kTopLeft));

    if (failures == 0) {
        std::printf("player-slot-position-test: all checks passed\n");
    } else {
        std::fprintf(stderr, "player-slot-position-test: %d check(s) failed\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
