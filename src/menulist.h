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

#ifndef MENULIST_H
#define MENULIST_H

// Shared full-screen settings-list widget, used by every sub-menu panel:
// local multiplayer, change keys, the LAN/Net server lists, and the netplay
// game room. Replaces five independently hand-rolled panel layouts (a wood
// popup covering under half the canvas at one 15px type size, plus the game
// room's own hardcoded settingY[] table) with one shared frame at a type
// scale that actually clears Android's ~14sp legibility floor, and a list
// that scrolls instead of forcing rows to be relocated or deleted for want
// of space. See docs, CHANGELOG vX.X.X.
//
// The frame is four regions, all in the game's fixed 640x480 logical
// canvas: a header bar, a scrolling row list on the left, a fixed context
// sidebar on the right, and a one-line footer hint. Screens that also need
// the persistent chat dock (the game room only) use the "Docked" variants,
// which leave room for it below.

#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

#include "ttftext.h"

namespace menulist {

// ---- shared geometry ---------------------------------------------------
constexpr SDL_Rect kHeaderBar     = {10, 8, 620, 28};
constexpr SDL_Rect kListFull      = {10, 44, 404, 400};
constexpr SDL_Rect kSidebarFull   = {422, 44, 208, 400};
// Leaves room for NetPanelChatDockRender()'s dock, which starts at y=334.
constexpr SDL_Rect kListDocked    = {10, 44, 404, 282};
constexpr SDL_Rect kSidebarDocked = {422, 44, 208, 282};
constexpr int kFooterY = 452;
constexpr int kRowH    = 32;

// ---- shared palette ------------------------------------------------------
// Lifted directly from the game room's own drawPanel/drawLabel/drawSelection,
// which was already the best-looking screen in the game -- the point of this
// widget is that every screen now looks like that one, not a new fourth
// language.
constexpr SDL_Color kText        = {248, 250, 239, 255};
constexpr SDL_Color kMuted       = {174, 211, 202, 255};
constexpr SDL_Color kGold        = {255, 218, 92, 255};
constexpr SDL_Color kBad         = {255, 128, 128, 255};
constexpr SDL_Color kEdge        = {255, 190, 46, 225};
constexpr SDL_Color kSelFill     = {255, 196, 64, 72};
constexpr SDL_Color kSelEdge     = {255, 218, 92, 240};
constexpr SDL_Color kListFill    = {26, 18, 48, 222};
constexpr SDL_Color kSidebarFill = {18, 55, 65, 225};
constexpr SDL_Color kHeaderFill  = {38, 20, 57, 235};
// A lower-opacity variant of the three fills above, for the two screens
// that sit over the world-map backdrop (the joined-server lobby and the
// game room) rather than the busier main-menu artwork every other screen
// uses -- the map is flat and muted enough to show through without hurting
// legibility; the title art and penguin renders behind LocalMP/Keys/the
// server lists are not (found live: dropping every screen's opacity together
// made those three hard to read, so only the map-backed two get it).
constexpr Uint8 kMapFillAlpha = 150;
constexpr SDL_Color kTextShadow  = {20, 12, 32, 255};

// Forwards to MainMenu::AddPanelTapRow's signature exactly, so a caller's
// lambda is just `[&](int i, const SDL_Rect& r, int s, bool sa, SDL_Keycode k)
// { AddPanelTapRow(i, r, s, sa, k); }`.
using TapRowFn = std::function<void(int index, const SDL_Rect& rect,
                                     int subIndex, bool splitAdjust,
                                     SDL_Keycode activateKey)>;

// The {10,8,620,28} header bar every screen shares: a bold title on the
// left and, optionally, one right-aligned action ("Start game!", "Follow
// this server") that becomes its own tap row when interactive. Pass
// actionIndex < 0 when this screen has no header action this frame.
// fillAlpha overrides kHeaderFill's own alpha when >= 0 -- see kMapFillAlpha.
void DrawHeaderBar(SDL_Renderer* rend, TTFText& text, const SDL_Rect& bar,
                    const char* title, const char* action, bool actionSelected,
                    int actionIndex, const TapRowFn& addTapRow, int fillAlpha = -1);

// Draws a sidebar's background plus its own section header and rule, and
// returns the y at which the caller's own content should start. The frame
// is shared; sidebar content (roster rows, warnings, connection details) is
// bespoke per screen and drawn by the caller below that y. fillAlpha
// overrides kSidebarFill's own alpha when >= 0 -- see kMapFillAlpha.
int DrawSidebarHeader(SDL_Renderer* rend, TTFText& text, const SDL_Rect& sidebar,
                       const char* title, int fillAlpha = -1);

// One footer hint line, shared style, at the fixed y every screen uses.
void DrawFooterHint(SDL_Renderer* rend, TTFText& text, const char* hint);

// The world-map backdrop every full-screen panel now shares (the same
// texture the joined-server lobby and room have always drawn over) --
// full-canvas, then its own baked-in "NETWORK PLAY..." watermark (bottom-
// right corner) masked behind a solid bar sized for the footer hint every
// screen draws at kFooterY, so the hint's own right-hand words don't render
// on top of it (found live testing the NET GAME list). bg is the caller's
// own MainMenu::netGameBackground -- menulist doesn't own the texture, so a
// null bg is a no-op rather than a crash.
void DrawWorldMapBackdrop(SDL_Renderer* rend, SDL_Texture* bg);

// A section header (bold gold, uppercase-style, own rule below) outside a
// List -- for a caller that has its own non-row content (the game room's
// per-player grid) to introduce with the same visual language a List's own
// Header() rows use, so the two don't read as two different conventions
// sitting back to back on the same panel.
void DrawSectionHeader(SDL_Renderer* rend, TTFText& text, int x, int y, int w,
                        const char* title);

// The scrolling row list itself. Build it with Header()/Row() calls (order
// is draw order), then End() to draw it: panel background, only the rows
// currently scrolled into view (keeping the selected row visible), the
// scrollbar if content overflows the viewport, and one tap band per visible
// selectable row.
class List {
public:
    // fillAlpha overrides kListFill's own alpha when >= 0 -- see kMapFillAlpha.
    List(const SDL_Rect& viewport, int selectedIndex, int rowH = kRowH, int fillAlpha = -1);

    // A section header: bold gold, uppercase, its own rule below. Not
    // selectable -- does not participate in Up/Down or take a tap row.
    void Header(const std::string& title);

    // label left, value right-aligned. value.empty() draws a label-only row
    // (e.g. "Manual entry...", "Reset ctrl defaults"). `emphasize` colours a
    // non-empty value gold+bold when true (an enabled/set state) or muted
    // grey when false (disabled/off) -- the on/off convention every screen
    // already used informally. splitAdjust wraps the value in "<  >" and
    // marks the row so a second tap on either half steps it, matching the
    // existing PanelTapRow::splitAdjust contract.
    void Row(int index, const std::string& label, const std::string& value = "",
             bool emphasize = true, bool splitAdjust = false);

    // Same as Row(), but the value is drawn in an explicit colour instead of
    // the on/off convention -- for a value whose meaning is a hue, not a
    // state (a team's own colour).
    void RowColored(int index, const std::string& label, const std::string& value,
                     SDL_Color valueColor, bool splitAdjust = false);

    // A row with a second, narrower tap target at its left edge (the LAN/Net
    // server lists' follow star), registered before the row's own full-width
    // band so it wins the hit test. prefixGlyph is drawn in place of the
    // usual bullet position; prefixKey is what a tap on it sends.
    void RowWithPrefix(int index, const std::string& prefixGlyph, SDL_Color prefixColor,
                        const std::string& label, const std::string& value,
                        bool emphasize, SDL_Keycode prefixKey);

    // Draws everything and registers tap rows for what's visible. Returns
    // the logical-canvas y one row past the last row drawn, in case a caller
    // wants to append bespoke content directly below (unused by any current
    // caller, kept because every List is built this way and it costs
    // nothing to expose).
    int End(SDL_Renderer* rend, TTFText& text, SDL_Texture* panelBG,
            const TapRowFn& addTapRow);

private:
    struct Entry {
        int index;              // -1 for header/unselectable
        std::string label, value, prefix;
        SDL_Color valueColor;
        bool splitAdjust;
        bool header;
        bool hasPrefix;
        SDL_Color prefixColor;
        SDL_Keycode prefixKey;
    };
    SDL_Rect viewport_;
    int selectedIndex_;
    int rowH_;
    int fillAlpha_;
    std::vector<Entry> rows_;
};

} // namespace menulist

#endif // MENULIST_H
