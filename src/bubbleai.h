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

#ifndef BUBBLEAI_H
#define BUBBLEAI_H

struct BubbleArray;

// Bot aiming.
//
// The bot does not model the board's geometry itself. It flies a probe bubble
// through the real launch physics -- the same SingleBubble::UpdatePosition,
// the same collision test, the same GetClosestFreeCell -- once per candidate
// angle, and keeps the angle whose landing scores best. Anything the physics
// does for free, including wall bounces, the bot therefore gets for free too,
// and the two can never drift apart the way a re-derived model would.
namespace BubbleAI {

// How hard the bot tries. Kept small and explicit rather than a 0-100 dial:
// each step is a different behaviour, not a different number.
enum class Skill {
    Easy,    // often takes a mediocre angle on purpose
    Normal,  // usually best, sometimes second best
    Hard,    // always the best angle it found
};

struct Shot {
    float angle = 0.0f;
    int score = 0;
    int row = -1, col = -1;   // where the probe predicts it lands
    bool valid = false;       // false when no angle lands anywhere
};

// Where a shot fired at `angle` would come to rest. `board` is not modified,
// but is taken by reference because the shared cell-resolution helper needs a
// mutable one. Returns false if the probe never lands (angles pointing into a
// wall forever, or an empty board).
bool PredictLanding(BubbleArray &board, float angle, int colour, bool isMini,
                    int *row, int *col);

// Bubbles removed if `colour` lands at (row, col): the matched group plus
// whatever that detaches from the ceiling. Zero when the group is under three.
int ScoreLanding(const BubbleArray &board, int row, int col, int colour);

// The angle to fire at. `rng` is advanced, so a caller keeps its own stream
// and bots stay independent of the game's own rand() sequence.
Shot ChooseShot(BubbleArray &board, int colour, bool isMini, Skill skill,
                unsigned *rng);

}  // namespace BubbleAI

#endif  // BUBBLEAI_H
