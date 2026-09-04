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

#ifndef SENDGAMESTATS_H
#define SENDGAMESTATS_H

#include <string>

// Opt-in upload of a single classic-solo-mode result to the community
// leaderboard at petitain.be. Gated end-to-end on
// GameSettings::uploadHighscoreStatsEnabled() (off by default) -- callers
// still check that flag themselves before calling this, so it stays true
// that nothing here ever runs without the player explicitly turning the
// setting on through the confirmation popup in mainmenu_panels.cpp.
//
// Fire-and-forget: returns immediately, does the actual HTTP POST on a
// detached background thread, and never reports success/failure back to the
// caller. See sendGameStats.cpp's header comment for the exact wire format
// and why this is desktop-native only.
void sendGameStats(int score, int level, int playTimeSeconds, const std::string& name);

#endif // SENDGAMESTATS_H
