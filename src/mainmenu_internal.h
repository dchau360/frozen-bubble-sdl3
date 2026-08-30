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
# ifdef __ANDROID__
    // Ads and in-app purchases are Android-only, so these three rows exist
    // nowhere else. They replaced an [R]-to-buy hint that was unreachable in
    // practice: nothing maps a controller or a touch to R, so no phone,
    // tablet, or TV box could ever trigger it.
    kKeyRowRemoveAdsMonth   = 9,
    kKeyRowRemoveAdsYear    = 10,
    kKeyRowRemoveAdsForever = 11,
    kKeyRowResetAll   = 12,
# else
    kKeyRowResetAll   = 9,
# endif
#else
    kKeyRowResetAll   = 8,
#endif
    kKeyRowLast = kKeyRowResetAll
};

// Fake row indices for the sidebar's P1..P4 player-switch rows (see
// KeysPanelRender). Kept well clear of KeyConfigRow's real range so a tap
// landing on one never gets mistaken for a real settings row -- the
// select-then-activate dance still applies (see HandlePanelTap), it just
// activates via SDLK_1..4 (mainmenu_input.cpp) instead of Left/Right/Return.
static const int kKeyPlayerTapBase = 900;

// Row indices for the game room's action list (selectedActionIndex). The list
// is built positionally in mainmenu_netpanel.cpp and acted on by index in
// mainmenu_input.cpp, so every row that is added or removed renumbers the ones
// below it -- previously in about forty separate literals across the two files,
// where a missed one silently toggles the neighbouring setting instead.
//
// Only meaningful while a game room is open. The plain lobby uses the same
// variable for a different list (0 = chat, 1 = create, 2+ = one per room).
enum GameRoomRow {
    kRoomChat       = 0,
    kRoomMode       = 1,
    kRoomMalus      = 2,
    kRoomChain      = 3,
    kRoomTarget     = 4,
    kRoomVictories  = 5,
    kRoomMouse      = 6,
    // Per-player grid: label on the left, one cell per player across.
    kRoomMaxColors  = 7,
    kRoomRows       = 8,
    kRoomAim        = 9,
    kRoomTeam       = 10,
    // Bots are the host's to add, and "Start game!" is host-only too, so a
    // joiner's list simply ends at kRoomTeam and never reaches either.
    kRoomBots       = 11,   // host only
    kRoomBotSkill   = 12,   // host only (shown even at zero bots, so the
                            // rows below it never renumber)
    kRoomStart      = 13,   // host only, and only with more than one player

    kRoomGridFirst  = kRoomMaxColors,
    kRoomGridLast   = kRoomAim,   // Team is edited through the roster, not here
};

// The plain lobby's "Follow this server" toggle (0 = chat, 1 = create,
// 2 = follow, 3+ = one per room). Rendered in the header bar rather than as
// a list row -- the same treatment kRoomStart gets for "Start game!" -- but
// it still occupies a real slot in the action list so keyboard/gamepad
// Up/Down can reach it, not just touch. Reachable regardless of how this
// server was connected to (list star, LAN discovery, or manual entry), since
// all of those funnel through this one lobby screen once connected.
static const int kLobbyFollow = 2;

// Texture dimensions helper (mainmenu.cpp).
SDL_Point GetSize(SDL_Texture *texture);

// Returns true if something is already listening on localhost:port
// (mainmenu_server.cpp).
bool portInUse(int port);

#endif // MAINMENU_INTERNAL_H
