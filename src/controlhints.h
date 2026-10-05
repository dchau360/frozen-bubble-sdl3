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

// What a 1-player game's controls are called on this device, in words: the
// Swap hint at the start of a run (BubbleGame::SkipShotHintLine) and the How
// to play page (mainmenu_howto.cpp) both read them here, so the two always
// name the same keys. Player 1's bindings only.

#ifndef CONTROLHINTS_H
#define CONTROLHINTS_H

#include <SDL3/SDL.h>
#include <string>

namespace controlhints {

// A phone, a tablet, or a browser on one.
bool TouchDevice();
// "Right Shift", or just "X" for a pad button. Empty when unbound.
std::string KeyName(SDL_Scancode sc);
// "Space", "Space or click the board", "tap the board".
std::string FireControl(bool mouseAim);
// "Right Shift", "Right Shift or right click", "tap below the board".
std::string SwapControl(bool mouseAim);

}  // namespace controlhints

#endif
