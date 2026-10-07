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

// Drawing for the modern HUD (BubbleGame::UsesModernHud()): the 1-player
// score panel and the level-cleared / lost / paused card. Pure drawing --
// nothing here reads or changes game state, so replays are untouched by it.

#ifndef MODERNUI_H
#define MODERNUI_H

#include <SDL3/SDL.h>
#include <string>

#include "ttftext.h"

namespace modernui {

// The palette. Every colour the modern screens draw comes from this block, so
// a brand's look is decided here: Frozen Bubble's navy and ice, or Boba
// Buster's chocolate and caramel (FB_BRAND_BOBA, see brand.h). kIce is the
// accent (edges, focus, primary buttons), kInk the text drawn on it, and the
// rest are the dark fills from lightest to darkest.
#if defined(FB_BRAND_BOBA)
constexpr SDL_Color kCardFill = {43, 23, 13, 248};      // chocolate
constexpr SDL_Color kCardRaised = {66, 37, 21, 245};    // a selected menu card
constexpr SDL_Color kWellRim = {84, 48, 28, 200};       // pocket well, inner ring
constexpr SDL_Color kWellOuter = {36, 18, 9, 225};
constexpr SDL_Color kRing = {30, 14, 6, 230};           // outline on score popups
constexpr SDL_Color kWellDeep = {22, 10, 4, 210};
constexpr SDL_Color kDim = {18, 8, 3, 255};             // over the board behind a card
constexpr SDL_Color kEdge = {233, 168, 104, 140};
constexpr SDL_Color kIce = {233, 168, 104, 255};        // caramel
constexpr SDL_Color kLabel = {214, 176, 140, 255};
constexpr SDL_Color kInk = {58, 28, 12, 255};
constexpr SDL_Color kFrost = {255, 243, 226, 255};      // cream
constexpr SDL_Color kSwitchOff = {96, 66, 46, 255};
#else
constexpr SDL_Color kCardFill = {12, 24, 48, 248};
constexpr SDL_Color kCardRaised = {18, 36, 70, 245};
constexpr SDL_Color kWellRim = {22, 44, 78, 200};
constexpr SDL_Color kWellOuter = {10, 22, 44, 225};
constexpr SDL_Color kRing = {6, 20, 40, 230};
constexpr SDL_Color kWellDeep = {6, 14, 30, 210};
constexpr SDL_Color kDim = {3, 8, 20, 255};
constexpr SDL_Color kEdge = {127, 214, 255, 140};
constexpr SDL_Color kIce = {127, 214, 255, 255};
constexpr SDL_Color kLabel = {142, 163, 198, 255};
constexpr SDL_Color kInk = {6, 34, 58, 255};
constexpr SDL_Color kFrost = {219, 232, 251, 255};
constexpr SDL_Color kSwitchOff = {58, 75, 108, 255};
#endif
constexpr SDL_Color kValue = {255, 255, 255, 255};
constexpr SDL_Color kLose = {255, 122, 138, 255};
constexpr SDL_Color kGold = {255, 216, 74, 255};
constexpr SDL_Color kNoShadow = {0, 0, 0, 0};

// `c` with alpha `a`, for the palette's colours at other strengths.
constexpr SDL_Color Alpha(SDL_Color c, Uint8 a) { return {c.r, c.g, c.b, a}; }

void FillRoundRect(SDL_Renderer* rend, SDL_FRect r, float radius, SDL_Color c);
void StrokeRoundRect(SDL_Renderer* rend, SDL_FRect r, float radius, float width, SDL_Color c);

// Menu pieces shared by the Modern menu style's screens.
constexpr SDL_Color kTextShadow = {0, 0, 0, 115};
constexpr SDL_Color kRowFocus = Alpha(kIce, 33);
constexpr SDL_Color kRowIdle = Alpha(kIce, 10);
constexpr SDL_Color kLine = Alpha(kIce, 41);

// Draws `t` showing `s` with its middle at cy: align 0 starts at x, 1 is
// centred on x, 2 ends at x. Returns the width drawn.
int DrawTextLine(SDL_Renderer* rend, TTFText& t, const char* s, SDL_Color fg, SDL_Color shadow,
                 int x, int cy, int align = 0);
// An on/off switch, 30x16, left edge x, middle cy.
void DrawSwitch(SDL_Renderer* rend, int x, int cy, bool on);
// A small rounded label (HOST, YOU) starting at x, middle cy; `filled`
// draws it solid in `c` with dark text. Returns its width.
int DrawChip(SDL_Renderer* rend, TTFText& t, const char* s, int x, int cy, SDL_Color c, bool filled = false);
// A rounded button: `primary` is solid ice, otherwise a faint outline;
// `focused` adds the white ring keyboard focus is shown with.
void DrawPill(SDL_Renderer* rend, TTFText& t, const char* s, SDL_Rect r, bool primary, bool focused);
// The Swap pocket: a round well around the bubble rect `r`, holding `bubble`
// (null draws it empty), with a "SWAP" tag across its rim drawn with `tag`
// (any small bold font, dark text). `lit` brightens the rim and tag, for a
// held bubble. The board (BubbleGame::DrawPocket) and the How to play page
// both draw it here.
void DrawPocketWell(SDL_Renderer* rend, TTFText& tag, SDL_FRect r, SDL_Texture* bubble, bool lit,
                    bool tagged = true);
// The card every Modern menu screen is built from.
void DrawCard(SDL_Renderer* rend, SDL_Rect r, float radius = 14);

// 3'07" -- the same form the High Scores screen uses.
std::string FormatTime(Uint64 ms);
// 12,345.
std::string FormatNumber(int n);

// The text objects the theme draws with, loaded once. Each line on screen
// has its own object so a value that changes (the clock, a counting score)
// re-renders only itself.
struct Fonts {
    TTFText hudLabel[4], hudValue[4];
    TTFText title, statLabel[3], statValue[3], button[2], note, popup, dropped;
    void Load();
};

// The score panel down the left edge: level, score, shots, time.
void DrawHud(SDL_Renderer* rend, Fonts& f, const std::string& level, int score, int shots, Uint64 timeMs);

struct Stat { std::string label, value; };

// One card for level cleared, lost and paused. `appear` runs 0..1 as it
// comes in (it rises and fades in). Buttons: 0, 1 or 2; `focus` is the one
// drawn highlighted, and their rects come back in btnOut for tap hit-tests.
// `art`, when given, is drawn under the title (the pause penguin).
struct Card {
    std::string title;
    SDL_Color titleColor = kIce;
    Stat stats[3];
    int statCount = 0;
    std::string buttons[2];
    int buttonCount = 0;
    int focus = 0;
    std::string note;
    SDL_Texture* art = nullptr;
};
void DrawCard(SDL_Renderer* rend, Fonts& f, const Card& card, float appear, SDL_Rect btnOut[2]);

// The on-screen pause button (BubbleGame::ShowsPauseButton): two bars, or a
// play triangle while the game is paused.
void DrawPauseButton(SDL_Renderer* rend, SDL_Rect r, bool paused);

// A floating score: "+N" over the pop (small) or "N DROPPED!" (big),
// `t` running 0..1 over its life.
void DrawPopup(SDL_Renderer* rend, TTFText& text, const std::string& label, int x, int y, float t, bool big);

}  // namespace modernui

#endif
