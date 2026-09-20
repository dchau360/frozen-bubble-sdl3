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

#include "playerbadge.h"
#include "sdl3_compat.h"

namespace {

// Dark text on the light chips, light text on the dark ones -- picked per
// entry rather than computed, since there are nine of them and a luminance
// rule would still need hand-tuning at this size.
constexpr SDL_Color kDark  = {20, 22, 28, 255};
constexpr SDL_Color kLight = {245, 245, 250, 255};

struct BadgeEntry {
    char tag;
    PlayerBadge badge;
};

// Colours are the ones each platform's own community already reads as "that
// one" (Windows blue, Android green, the web's cyan) without reproducing
// anybody's logo -- see the header on why this is a chip and not an icon.
constexpr BadgeEntry kPlatformBadges[] = {
    {'W', {"WIN", {0,   120, 215, 255}, kLight}},
    {'M', {"MAC", {190, 192, 198, 255}, kDark }},
    {'L', {"LIN", {235, 160, 30,  255}, kDark }},
    {'A', {"AND", {60,  170, 90,  255}, kLight}},
    {'I', {"IOS", {120, 125, 135, 255}, kLight}},
    {'B', {"WEB", {40,  180, 200, 255}, kDark }},
};

// One palette for every input badge: unlike the platform, which is a property
// of the player worth telling apart at a glance across a whole lobby, this one
// only ever has to be read next to the name it belongs to, and a second set of
// six competing colours on the same board would be noise.
constexpr SDL_Color kInputFill = {70, 74, 86, 255};

constexpr BadgeEntry kInputBadges[] = {
    {'K', {"KB",  kInputFill, kLight}},
    {'M', {"MSE", kInputFill, kLight}},
    {'T', {"TCH", kInputFill, kLight}},
    {'G', {"PAD", kInputFill, kLight}},
};

template <size_t N>
bool Lookup(const BadgeEntry (&table)[N], char tag, PlayerBadge& out) {
    if (!tag) return false;
    for (const BadgeEntry& e : table) {
        if (e.tag == tag) { out = e.badge; return true; }
    }
    return false;
}

}  // namespace

bool GetPlatformBadge(char tag, PlayerBadge& out) {
    return Lookup(kPlatformBadges, tag, out);
}

bool GetInputBadge(char tag, PlayerBadge& out) {
    return Lookup(kInputBadges, tag, out);
}

int PlayerBadgeChipWidth(int labelWidth) {
    // kPlayerBadgePadX either side of the measured label, plus one column for
    // the chip's own border on each edge -- a label placed flush against that
    // border has its last stem swallowed by it. The floor keeps a chip visible
    // if a text object ever reports a zero width (an unloaded font, a failed
    // texture), so a badge degrades to a blank chip rather than to a
    // zero-width sliver that shifts everything after it left.
    if (labelWidth < 0) labelWidth = 0;
    const int w = labelWidth + 2 * kPlayerBadgePadX;
    return w < 14 ? 14 : w;
}

void DrawPlayerBadgeChip(SDL_Renderer* renderer, const SDL_Rect& rect,
                         const PlayerBadge& badge) {
    if (!renderer) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, badge.fill.r, badge.fill.g, badge.fill.b, 235);
    SDL_FRect fr = ToFRect(rect);
    SDL_RenderFillRect(renderer, &fr);
    // A one-pixel darker edge, so a chip whose fill is close to whatever is
    // behind it (the grey ones, over the game's own grey furniture) still
    // reads as a separate object rather than a smudge on the background.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
    SDL_RenderRect(renderer, &fr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
