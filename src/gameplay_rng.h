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

#ifndef GAMEPLAY_RNG_H
#define GAMEPLAY_RNG_H

#include <cstdint>

// A gameplay-only random stream, isolated from libc's global rand()/srand().
// Levels, next-bubble colors, malus placement and free-fall velocities all
// need to be replay-capturable; cosmetic code (menu candy, highscore
// pictures, transition snow) shares the global rand() stream and can reseed
// or draw from it at any time, so gameplay must not depend on that same
// sequence. Uses the same LCG family already proven by the per-bot RNG in
// bubbleai.cpp's ChooseShot(), for consistency with the one isolated-stream
// pattern this codebase already has.
class GameplayRng {
public:
    void Seed(uint32_t seed) { state = seed; }
    uint32_t State() const { return state; }

    uint32_t Next() {
        state = state * 1103515245u + 12345u;
        return (state >> 16) & 0x7fffu;
    }

    // Mirrors the semantics of the now-removed gameplay ranrange() (formerly
    // in bubblegame_internal.h) so call sites swapped over with no change to
    // the resulting distributions.
    int Range(int a, int b) { return a + static_cast<int>(Next() % static_cast<uint32_t>((b - a) + 1)); }
    float Range(float b) { return Next() / (32768.0f / b); }

private:
    uint32_t state = 0;
};

#endif // GAMEPLAY_RNG_H
