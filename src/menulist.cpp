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

#include "menulist.h"
#include "sdl3_compat.h"

#include <algorithm>

namespace menulist {

namespace {

void DrawPanel(SDL_Renderer* rend, const SDL_Rect& rect, SDL_Color fill, SDL_Color outline) {
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, fill.r, fill.g, fill.b, fill.a);
    SDL_FRect fr = ToFRect(rect);
    SDL_RenderFillRect(rend, &fr);
    SDL_SetRenderDrawColor(rend, outline.r, outline.g, outline.b, outline.a);
    SDL_RenderRect(rend, &fr);
}

void DrawSelection(SDL_Renderer* rend, const SDL_Rect& rect) {
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, kSelFill.r, kSelFill.g, kSelFill.b, kSelFill.a);
    SDL_FRect fr = ToFRect(rect);
    SDL_RenderFillRect(rend, &fr);
    SDL_SetRenderDrawColor(rend, kSelEdge.r, kSelEdge.g, kSelEdge.b, kSelEdge.a);
    SDL_RenderRect(rend, &fr);
}

// left=true draws with its left edge at x; left=false draws with its right
// edge at x (used for the right-aligned value column).
int DrawText(SDL_Renderer* rend, TTFText& text, const std::string& s, int size, int style,
             SDL_Color fg, int x, int y, bool alignLeft) {
    if (s.empty()) return 0;
    text.UpdateStyle(size, style);
    text.UpdateColor(fg, kTextShadow);
    text.UpdateText(rend, s.c_str(), 0);
    int w = text.Coords()->w;
    text.UpdatePosition({alignLeft ? x : x - w, y});
    SDL_FRect fr = ToFRect(*text.Coords());
    SDL_RenderTexture(rend, text.Texture(), nullptr, &fr);
    return w;
}

// Shrinks s (assumed ASCII -- pop_back() one char at a time is not
// UTF-8-safe, but every label this widget draws is an English string or a
// hostname/nickname) until "<s>..." fits within maxW, so a long server name
// can never run into the right-aligned value column next to it. Re-renders
// the candidate a few times in the (rare) case a label actually overflows --
// not a hot path, this runs once per visible overflowing row per frame.
std::string TruncateToWidth(SDL_Renderer* rend, TTFText& text, const std::string& s,
                             int maxW, int size, int style) {
    if (maxW <= 0) return "";
    text.UpdateStyle(size, style);
    text.UpdateText(rend, s.c_str(), 0);
    if (text.Coords()->w <= maxW) return s;
    std::string truncated = s;
    while (!truncated.empty()) {
        truncated.pop_back();
        std::string candidate = truncated + "...";
        text.UpdateText(rend, candidate.c_str(), 0);
        if (text.Coords()->w <= maxW) return candidate;
    }
    return "...";
}

} // namespace

void DrawHeaderBar(SDL_Renderer* rend, TTFText& text, const SDL_Rect& bar,
                    const char* title, const char* action, bool actionSelected,
                    int actionIndex, const TapRowFn& addTapRow, int fillAlpha) {
    SDL_Color fill = kHeaderFill;
    if (fillAlpha >= 0) fill.a = (Uint8)fillAlpha;
    DrawPanel(rend, bar, fill, kEdge);

    // Action's width measured first (not yet drawn) so a long title -- a
    // room's own name, chosen by whoever created it, not by this game --
    // gets truncated to whatever room is actually left of it instead of
    // running underneath it (found live: "android_us's GAME ROOM | HOST |
    // 2 players" overlapped "Start game!" outright).
    int actionLeft = bar.x + bar.w - 8;
    if (action && action[0] && actionIndex >= 0) {
        text.UpdateStyle(18, TTF_STYLE_BOLD);
        text.UpdateText(rend, action, 0);
        actionLeft -= text.Coords()->w + 6;
    }
    int maxTitleW = actionLeft - (bar.x + 10) - 10;
    std::string titleStr = TruncateToWidth(rend, text, title, maxTitleW, 20, TTF_STYLE_BOLD);
    DrawText(rend, text, titleStr, 20, TTF_STYLE_BOLD, kGold, bar.x + 10, bar.y + 4, true);

    if (action && action[0] && actionIndex >= 0) {
        SDL_Color color = actionSelected ? kGold : kText;
        text.UpdateStyle(18, TTF_STYLE_BOLD);
        text.UpdateColor(color, kTextShadow);
        text.UpdateText(rend, action, 0);
        int tw = text.Coords()->w;
        int sx = bar.x + bar.w - 8 - tw;
        SDL_Rect actRect = {sx - 6, bar.y + 3, tw + 12, bar.h - 6};
        // Selection band first, so the already-rendered text draws on top.
        if (actionSelected) DrawSelection(rend, actRect);
        text.UpdatePosition({sx, bar.y + 5});
        { SDL_FRect fr = ToFRect(*text.Coords()); SDL_RenderTexture(rend, text.Texture(), nullptr, &fr); }
        addTapRow(actionIndex, actRect, -1, false, 0);
    }
}

int DrawSidebarHeader(SDL_Renderer* rend, TTFText& text, const SDL_Rect& sidebar,
                       const char* title, int fillAlpha) {
    SDL_Color fill = kSidebarFill;
    if (fillAlpha >= 0) fill.a = (Uint8)fillAlpha;
    DrawPanel(rend, sidebar, fill, kEdge);
    DrawText(rend, text, title, 14, TTF_STYLE_BOLD, kGold, sidebar.x + 12, sidebar.y + 10, true);
    int ruleY = sidebar.y + 32;
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, kEdge.r, kEdge.g, kEdge.b, 128);
    SDL_RenderLine(rend, (float)(sidebar.x + 12), (float)ruleY,
                   (float)(sidebar.x + sidebar.w - 12), (float)ruleY);
    return ruleY + 10;
}

void DrawFooterHint(SDL_Renderer* rend, TTFText& text, const char* hint) {
    text.UpdateStyle(14, TTF_STYLE_NORMAL);
    text.UpdateColor(kMuted, kTextShadow);
    text.UpdateText(rend, hint, 0);
    int w = text.Coords()->w;
    text.UpdatePosition({(640 - w) / 2, kFooterY});
    SDL_FRect fr = ToFRect(*text.Coords());
    SDL_RenderTexture(rend, text.Texture(), nullptr, &fr);
}

void DrawWorldMapBackdrop(SDL_Renderer* rend, SDL_Texture* bg) {
    if (!bg) return;
    SDL_RenderTexture(rend, bg, nullptr, nullptr);
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, kHeaderFill.r, kHeaderFill.g, kHeaderFill.b, kHeaderFill.a);
    SDL_FRect footerBar = {0, (float)(kFooterY - 6), 640, 26};
    SDL_RenderFillRect(rend, &footerBar);
}

void DrawSectionHeader(SDL_Renderer* rend, TTFText& text, int x, int y, int w,
                        const char* title) {
    DrawText(rend, text, title, 14, TTF_STYLE_BOLD, kGold, x, y, true);
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, kEdge.r, kEdge.g, kEdge.b, 110);
    SDL_RenderLine(rend, (float)x, (float)(y + 22), (float)(x + w), (float)(y + 22));
}

List::List(const SDL_Rect& viewport, int selectedIndex, int rowH, int fillAlpha)
    : viewport_(viewport), selectedIndex_(selectedIndex), rowH_(rowH), fillAlpha_(fillAlpha) {}

void List::Header(const std::string& title) {
    rows_.push_back({-1, title, "", "", kGold, false, true, false, kGold, 0, 0, 0, "", -1});
}

void List::Row(int index, const std::string& label, const std::string& value,
               bool emphasize, bool splitAdjust, SDL_Keycode labelActivateKey,
               SDL_Keycode suffixKey, const std::string& suffixText, int suffixIndex) {
    rows_.push_back({index, label, value, "", emphasize ? kGold : kMuted,
                      splitAdjust, false, false, kGold, 0, labelActivateKey,
                      suffixKey, suffixText, suffixIndex});
}

void List::RowColored(int index, const std::string& label, const std::string& value,
                       SDL_Color valueColor, bool splitAdjust) {
    rows_.push_back({index, label, value, "", valueColor, splitAdjust, false, false, kGold, 0, 0, 0, "", -1});
}

void List::RowWithPrefix(int index, const std::string& prefixGlyph, SDL_Color prefixColor,
                          const std::string& label, const std::string& value,
                          bool emphasize, SDL_Keycode prefixKey) {
    rows_.push_back({index, label, value, prefixGlyph, emphasize ? kGold : kMuted,
                      false, false, true, prefixColor, prefixKey, 0, 0, "", -1});
}

int List::End(SDL_Renderer* rend, TTFText& text, SDL_Texture* panelBG,
              const TapRowFn& addTapRow) {
    // Background first, at the full viewport, unclipped, so its border comes
    // out crisp -- rows are clipped to the viewport below, the panel itself
    // is not.
    if (panelBG) {
        SDL_FRect fr = ToFRect(viewport_);
        SDL_RenderTexture(rend, panelBG, nullptr, &fr);
    } else {
        SDL_Color fill = kListFill;
        if (fillAlpha_ >= 0) fill.a = (Uint8)fillAlpha_;
        DrawPanel(rend, viewport_, fill, kEdge);
    }

    const int total = (int)rows_.size();
    const int visibleRows = std::max(1, viewport_.h / rowH_);

    // Find this row list's copy of the current selection, if it has one --
    // a header-bar action (Start game!, Follow this server) can be the
    // selected index while never appearing as a row here at all.
    int selPos = -1;
    for (int i = 0; i < total; i++) {
        if (rows_[i].index == selectedIndex_) { selPos = i; break; }
    }

    // Pins the selected row one slot from the top rather than remembering a
    // scroll offset across frames: stateless, so there is nothing to get out
    // of sync with the row list a screen rebuilds fresh every frame, and it
    // still clamps to the ends so the list never scrolls past its own content.
    int maxScroll = std::max(0, total - visibleRows);
    int scrollTop = 0;
    if (selPos >= 0) scrollTop = std::clamp(selPos - 1, 0, maxScroll);

    SDL_Rect clip = viewport_;
    SDL_SetRenderClipRect(rend, &clip);

    const int lastVisible = std::min(total, scrollTop + visibleRows + 1);
    for (int i = scrollTop; i < lastVisible; i++) {
        const Entry& row = rows_[i];
        int y = viewport_.y + (i - scrollTop) * rowH_;
        SDL_Rect slot = {viewport_.x, y, viewport_.w, rowH_};

        if (row.header) {
            DrawText(rend, text, row.label, 14, TTF_STYLE_BOLD, kGold,
                     viewport_.x + 14, y + rowH_ - 24, true);
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(rend, kEdge.r, kEdge.g, kEdge.b, 110);
            SDL_RenderLine(rend, (float)(viewport_.x + 14), (float)(y + rowH_ - 4),
                           (float)(viewport_.x + viewport_.w - 14), (float)(y + rowH_ - 4));
            continue;
        }

        bool selected = (row.index == selectedIndex_);
        if (selected) {
            DrawSelection(rend, {viewport_.x + 6, y + 2, viewport_.w - 12, rowH_ - 4});
        }

        int textX = viewport_.x + 14;
        if (row.hasPrefix) {
            int pw = DrawText(rend, text, row.prefix, 20, TTF_STYLE_NORMAL, row.prefixColor,
                               textX, y + (rowH_ - 24) / 2, true);
            // Fixed prefix column (rather than pw itself) keeps every row's
            // label starting at the same x regardless of glyph width.
            textX += std::max(pw, 24) + 8;
            // Registered before the row's own full-width band below, so it
            // wins the hit test (first match wins -- see PanelTapRow).
            addTapRow(row.index, {viewport_.x + 4, y, 32, rowH_}, -1, false, row.prefixKey);
        }

        // Right-edge button (the HELP box), measured and reserved before the
        // value is laid out so the value's right-alignment stops short of it
        // rather than being drawn underneath. Its tap zone goes in here, ahead
        // of every zone this row registers below, so it wins the hit test.
        int rightEdge = viewport_.x + viewport_.w - 14;
        if (row.suffixKey != 0) {
            const int sw = 46, sh = 18;
            // Clear of the scrollbar this list draws at viewport_.w - 10 when
            // its content overflows -- at w - 8 the box's right border sat
            // under the scrollbar, so a tap on what looked like the scrollbar
            // opened the guide instead.
            const int sx = viewport_.x + viewport_.w - 16 - sw;
            const int sy = y + (rowH_ - sh) / 2;
            const SDL_Rect box = {sx, sy, sw, sh};
            const int suffixSel = row.suffixIndex >= 0 ? row.suffixIndex : row.index;
            const bool on = (selectedIndex_ == suffixSel);
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(rend, kSelFill.r, kSelFill.g, kSelFill.b, on ? 130 : 55);
            { SDL_FRect fr = ToFRect(box); SDL_RenderFillRect(rend, &fr); }
            SDL_SetRenderDrawColor(rend, kSelEdge.r, kSelEdge.g, kSelEdge.b, on ? 240 : 150);
            { SDL_FRect fr = ToFRect(box); SDL_RenderRect(rend, &fr); }
            DrawText(rend, text, row.suffixText, 14, TTF_STYLE_BOLD,
                     on ? kGold : kText, sx + 6, sy + 2, true);
            addTapRow(suffixSel, box, -1, false, row.suffixKey);
            rightEdge = sx - 8;
        }

        // Value measured (not yet drawn) before the label, so a long label
        // -- a server's name, chosen by whoever runs that server, not by
        // this game -- gets truncated to whatever room is actually left
        // instead of running into the value's screen space and overlapping
        // it, like the LAN/Net server rows used to before this widget
        // existed to catch it in one place.
        std::string val;
        int valueW = 0;
        if (!row.value.empty()) {
            val = row.splitAdjust ? ("<  " + row.value + "  >") : row.value;
            text.UpdateStyle(20, TTF_STYLE_BOLD);
            text.UpdateText(rend, val.c_str(), 0);
            valueW = text.Coords()->w;
        }
        int maxLabelW = rightEdge - textX - (valueW > 0 ? valueW + 12 : 0);
        std::string label = TruncateToWidth(rend, text, row.label, maxLabelW, 20, TTF_STYLE_NORMAL);

        DrawText(rend, text, label, 20, TTF_STYLE_NORMAL,
                 kText, textX, y + (rowH_ - 24) / 2, true);

        if (!val.empty()) {
            DrawText(rend, text, val, 20, TTF_STYLE_BOLD, row.valueColor,
                     rightEdge, y + (rowH_ - 24) / 2, false);
        }

        // A stepped row's "<  value  >" is drawn right-aligned, so on a row
        // with a long label and a short value (Game speed, "<  3.0  >"
        // against a wide row) the whole bracketed block -- including the
        // "<" a player actually taps to decrease -- sits well into what a
        // 50/50 split of the row would call its right half. Splitting the
        // row itself, not the text, is what let tapping the visible "<"
        // silently increase instead of decrease: the one thing that looks
        // tappable for "down" was on the wrong side of the boundary.
        //
        // Two adjacent tap rows sharing this row's index, split at the
        // bracketed block's own midpoint, fix that without changing
        // anything else -- HandlePanelTap's "not yet selected" branch still
        // just highlights the row regardless of which half was hit, exactly
        // as a single wide row did, and activateKey now says which key a
        // second tap sends outright instead of HandlePanelTap re-deriving a
        // midpoint of its own that this same mismatch would reintroduce.
        if (row.splitAdjust && valueW > 0) {
            const int valueBlockRight = rightEdge;
            const int valueBlockLeft = valueBlockRight - valueW;
            const int splitX = valueBlockRight - valueW / 2;
            // labelActivateKey != 0: this row's label is its own action
            // (e.g. "Create Game Room"), not just more of the value being
            // stepped, so the L/R split is confined to the value block
            // itself and the label gets its own zone -- otherwise the
            // value's own left half would run all the way back to the
            // row's start, over the label, same as it does for a plain
            // stepped row (Game speed, ...) where that's correct because
            // there the whole row IS the value.
            int leftEdge = slot.x;
            if (row.labelActivateKey != 0) {
                const SDL_Rect labelZone = {slot.x, slot.y, valueBlockLeft - slot.x, slot.h};
                addTapRow(row.index, labelZone, -1, false, row.labelActivateKey);
                leftEdge = valueBlockLeft;
            }
            const SDL_Rect leftHalf  = {leftEdge, slot.y, splitX - leftEdge, slot.h};
            const int rowRight = row.suffixKey != 0 ? rightEdge + 8 : slot.x + slot.w;
            const SDL_Rect rightHalf = {splitX, slot.y, rowRight - splitX, slot.h};
            addTapRow(row.index, leftHalf, -1, false, SDLK_LEFT);
            addTapRow(row.index, rightHalf, -1, false, SDLK_RIGHT);
        } else {
            addTapRow(row.index, slot, -1, row.splitAdjust, 0);
        }
    }

    SDL_SetRenderClipRect(rend, nullptr);

    // Scrollbar: only when content overflows the viewport, matching every
    // other "nothing to scroll" screen simply not drawing one.
    if (total > visibleRows) {
        int trackX = viewport_.x + viewport_.w - 10;
        int trackY = viewport_.y + 6;
        int trackH = viewport_.h - 12;
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 255, 255, 255, 40);
        SDL_FRect trackRect = {(float)trackX, (float)trackY, 4.0f, (float)trackH};
        SDL_RenderFillRect(rend, &trackRect);

        int thumbH = std::max(16, trackH * visibleRows / total);
        int thumbY = trackY + (maxScroll > 0 ? (trackH - thumbH) * scrollTop / maxScroll : 0);
        SDL_SetRenderDrawColor(rend, kEdge.r, kEdge.g, kEdge.b, 210);
        SDL_FRect thumbRect = {(float)trackX, (float)thumbY, 4.0f, (float)thumbH};
        SDL_RenderFillRect(rend, &thumbRect);
    }

    int drawnRows = lastVisible - scrollTop;
    return viewport_.y + drawnRows * rowH_;
}

} // namespace menulist
