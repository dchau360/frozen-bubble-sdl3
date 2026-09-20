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

#ifndef PLAYERBADGE_H
#define PLAYERBADGE_H

#include <SDL3/SDL.h>

// The two little tags the game shows beside a network player's name: which OS
// they are playing on, and which device they are shooting with this round.
//
// Both arrive as a single char over the wire -- the platform from the server's
// LIST line (the PLATFORM command, see NetworkPlayer::platform) and the input
// device from the 'i' opcode (BubbleArray::roundInput) -- and both are turned
// into pixels only here, so the lobby, the in-game boards and the post-round
// table cannot drift apart on what a tag means or what colour it is.
//
// Deliberately a coloured chip with a two- or three-letter label rather than a
// drawn logo. Three reasons, in order: the recognisable OS marks are
// trademarks, and a game that ships an Apple or Android silhouette in its UI is
// making a claim it has no licence for; nothing recognisable survives being
// drawn at ~9px on this project's 640x480 logical canvas, where a badge has to
// sit beside a name without crowding the board; and a label needs no asset at
// all, so it costs nothing on the Android asset extractor or the WASM preload
// list and cannot go missing on one platform. The colour is what makes it read
// as a badge at a glance; the letters are what make it unambiguous.
struct PlayerBadge {
    const char* label;   // "WIN", "KB", ... -- never null when Get* returned true
    SDL_Color fill;      // chip background
    SDL_Color text;      // label colour, chosen for contrast against fill
};

// Fills `out` for a tag this build knows and returns true; returns false for 0
// (the player never reported one) or any unrecognised char, which every caller
// treats as "draw nothing". A tag from a client newer than this build lands in
// that second case on purpose: no badge is honest, a guessed badge is not.
bool GetPlatformBadge(char tag, PlayerBadge& out);
bool GetInputBadge(char tag, PlayerBadge& out);

// Where the label goes inside the chip: callers draw it at chip.x +
// kPlayerBadgePadX, and PlayerBadgeChipWidth() reserves the same amount on the
// other side. Shared rather than repeated so a chip can never be sized with
// one padding and filled with another.
constexpr int kPlayerBadgePadX = 4;

// Pixel width the chip must be to hold a label already measured at
// `labelWidth` px. Takes the measured width rather than deriving one from the
// label's length: the three call sites draw at three different font sizes (the
// stats table at 14, the live boards at 11, the lobby sidebar at whatever the
// menu's own label object is set to), and a single per-character estimate that
// fits the smallest of them lets the label hang out of the chip at the largest
// -- which is exactly what the first cut of this did. Every caller already
// owns a text object that knows its own width, so measuring costs nothing.
int PlayerBadgeChipWidth(int labelWidth);

// Draws the chip background for `badge` at `rect`. Callers draw the label
// themselves with whatever text object they already own (the in-game boards
// pool TTFText, the menus use their own label helper), so this deliberately
// does not render text -- it only guarantees every badge in the game has the
// same shape and the same colour for the same tag.
void DrawPlayerBadgeChip(SDL_Renderer* renderer, const SDL_Rect& rect,
                         const PlayerBadge& badge);

#endif // PLAYERBADGE_H
