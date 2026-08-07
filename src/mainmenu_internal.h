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

#ifndef MAINMENU_INTERNAL_H
#define MAINMENU_INTERNAL_H

// Internal shared declarations for the mainmenu_*.cpp translation units.
// Not part of the public MainMenu interface (see mainmenu.h).

#include "mainmenu.h"

#include <cstdlib>

inline int ranrange(int a, int b) { return a + rand() % ((b - a ) + 1); }

// Room-size choices for the lobby's "Create Game Room" selector (netRoomSizeChoice
// indexes this). `static` gives each including translation unit its own copy,
// avoiding an ODR violation across mainmenu_netpanel.cpp/mainmenu_input.cpp.
static const int kRoomSizes[3] = {5, 10, 20};

// Row indices for the key-config panel (keyConfigIndex). The rows are drawn in
// mainmenu_panels.cpp and acted on in mainmenu_input.cpp; naming them keeps the
// two files from drifting, and keeps the wrap-around arithmetic from being three
// separately-maintained literals.
enum KeyConfigRow {
    kKeyRowLeft      = 0,
    kKeyRowRight     = 1,
    kKeyRowFire      = 2,
    kKeyRowCenter    = 3,
    kKeyRowResetCtrl = 4,
    kKeyRowSpeed     = 5,
    kKeyRowSound     = 6,
    kKeyRowMouse     = 7,
#ifndef __WASM_PORT__
    // The browser build has no fullscreen row: an SDL fullscreen flag blacks out
    // the canvas there, so the CSS shell does the scaling instead.
    kKeyRowFullscreen = 8,
    kKeyRowResetAll   = 9,
#else
    kKeyRowResetAll   = 8,
#endif
    kKeyRowLast = kKeyRowResetAll
};

// Texture dimensions helper (mainmenu.cpp).
SDL_Point GetSize(SDL_Texture *texture);

// Returns true if something is already listening on localhost:port
// (mainmenu_server.cpp).
bool portInUse(int port);

#endif // MAINMENU_INTERNAL_H
