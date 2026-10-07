#include "mainmenu.h"
#include "mainmenu_internal.h"
#include "menulist.h"
#include "netteams.h"
#include "bubblegame.h"
#include "localmultiplayer_settings.h"
#include "modernui.h"
#include "netbot.h"
#include "networkclient.h"
#include "platform.h"
#include "playerbadge.h"
#include "sdl3_compat.h"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

// The online screens in the Modern menu style (MENU_THEME_MODERN): the game
// room -- a top bar with the room's name and the host's Start button, a Match
// rules card (rules, the per-player table, then the host's Bots and Skill), a
// Players card, and the chat as a card along the bottom
// (NetPanelChatDockRender) -- and, sharing its fonts, the lobby, Set Teams and
// the two server lists. Drawing only: every row keeps the index the classic
// screen gives it, so Up/Down/Left/Right/ENTER and all state are the classic
// screen's, and every row is registered as a tap row the same way (stepped
// values split at their middle into LEFT/RIGHT, as menulist does).
//
// Sized for a phone, where the 640x480 canvas is drawn about 0.8x: nothing
// under 13px, rows 30-50px tall. A list taller than its card scrolls inside
// it: the view moves only as far as keeps the selected row in view, with a
// strip of the next row showing below it (ScrollToShow), so Up/Down, a
// swipe or the mouse wheel (both arrive as Up/Down, see NetListsShowing())
// scroll it, and a tap on a half-shown row selects it and scrolls it in. A
// row outside its card's view registers no tap target.
//
// Input parity (CLAUDE.md): every row joins the screen's Up/Down walk and is a
// tap row; the focused row is drawn with the ice edge and a focused stepper
// shows its < > arrows; the footer hint names the keys with no row of their
// own (ESC, F1, A).

namespace {
using namespace modernui;

constexpr SDL_Rect kTopBar = {8, 8, 624, 46};
constexpr int kTopCy = 31;
constexpr SDL_Rect kStartBtn = {516, 15, 108, 32};
constexpr int kCardTop = 60, kCardH = 238;     // down to the chat card
constexpr int kChatTop = 304, kChatH = 150;    // four lines and the input
constexpr int kHintCy = 467;                   // the footer hint, under the cards

const char* const kVictoriesLabels[] = {"None", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                                        "10", "11", "12", "15", "20", "30", "50", "100"};
// Seats with no team set show a candy bubble instead of a team colour.
constexpr int kSeatBalls[5] = {7, 3, 4, 5, 6};

// The scroll that keeps [top, bottom) -- content coordinates -- inside a view
// viewH tall, moving `scroll` no further than it must; `peek` of the row
// beyond stays visible so the list reads as going on. Clamped to the content.
int ScrollToShow(int scroll, int top, int bottom, int viewH, int contentH, int peek) {
    const int maxScroll = std::max(0, contentH - viewH);
    if (top - peek < scroll) scroll = top - peek;
    if (bottom + peek > scroll + viewH) scroll = bottom + peek - viewH;
    return std::clamp(scroll, 0, maxScroll);
}

// The part of `r` inside `view` (empty when none), so a row scrolled out of
// its card never takes a tap meant for something drawn over that spot.
SDL_Rect Visible(const SDL_Rect& r, const SDL_Rect& view) {
    const int x0 = std::max(r.x, view.x), y0 = std::max(r.y, view.y);
    const int x1 = std::min(r.x + r.w, view.x + view.w), y1 = std::min(r.y + r.h, view.y + view.h);
    if (x1 <= x0 || y1 <= y0) return {0, 0, 0, 0};
    return {x0, y0, x1 - x0, y1 - y0};
}

// A scroll bar down the right edge of a view, drawn only when it scrolls.
void DrawScrollBar(SDL_Renderer* rend, int x, const SDL_Rect& view, int scroll, int contentH) {
    if (contentH <= view.h) return;
    const float trackH = (float)view.h;
    const float thumbH = std::max(18.f, trackH * view.h / contentH);
    const float thumbY = view.y + (trackH - thumbH) * scroll / std::max(1, contentH - view.h);
    FillRoundRect(rend, {(float)x, (float)view.y, 5, trackH}, 2.5f, kRowIdle);
    FillRoundRect(rend, {(float)x, thumbY, 5, thumbH}, 2.5f, kIce);
}

// `s` cut with "..." to fit `maxW` canvas pixels in `font`.
std::string Fit(TTF_Font* font, const std::string& s, int maxW) {
    int w = 0;
    if (!font || maxW <= 0) return s;
    TTF_GetStringSize(font, s.c_str(), 0, &w, nullptr);
    if (w <= maxW) return s;
    std::string cut = s;
    while (!cut.empty()) {
        cut.pop_back();
        TTF_GetStringSize(font, (cut + "...").c_str(), 0, &w, nullptr);
        if (w <= maxW) break;
    }
    return cut + "...";
}

int TextW(TTF_Font* font, const char* s) {
    int w = 0;
    if (font) TTF_GetStringSize(font, s, 0, &w, nullptr);
    return w;
}

// modernui's chip and switch, a size up for these screens.
int Chip(SDL_Renderer* rend, TTFText& t, const char* s, int x, int cy, SDL_Color c, bool filled = false) {
    t.UpdateColor(filled ? kInk : c, kNoShadow);
    t.UpdateText(rend, s, 0);
    const int w = t.Coords()->w + 12;
    FillRoundRect(rend, {(float)x, (float)cy - 10, (float)w, 20}, 7, filled ? c : SDL_Color{c.r, c.g, c.b, 46});
    DrawTextLine(rend, t, s, filled ? kInk : c, kNoShadow, x + 6, cy);
    return w;
}
int ChipW(TTF_Font* font, const char* s) { return TextW(font, s) + 12; }

void Switch(SDL_Renderer* rend, int x, int cy, bool on) {
    FillRoundRect(rend, {(float)x, (float)cy - 11, 40, 22}, 11,
                  on ? SDL_Color{95, 224, 160, 255} : SDL_Color{58, 75, 108, 255});
    FillRoundRect(rend, {(float)(on ? x + 21 : x + 3), (float)cy - 8, 16, 16}, 8, kValue);
}
constexpr int kSwitchW = 40;
}  // namespace

// Fonts are opened once; every line on screen gets its own TTFText from a
// pool, taken in the same order each frame, so a line that does not change
// keeps its texture.
struct MainMenu::RoomModernText {
    TTF_Font *title = nullptr, *value = nullptr, *small = nullptr;
    TTF_Font *body = nullptr, *chat = nullptr, *chatBold = nullptr, *tiny = nullptr, *hint = nullptr;
    std::deque<TTFText> pool, chatPool, teamPool;
    size_t used = 0, chatUsed = 0, teamUsed = 0;
    SDL_Texture* ball[5] = {};

    TTFText& Take(TTF_Font* f) {
        if (used == pool.size()) pool.emplace_back();
        TTFText& t = pool[used++];
        t.LoadFont(f);
        return t;
    }
    TTFText& TakeTeam(TTF_Font* f) {
        if (teamUsed == teamPool.size()) teamPool.emplace_back();
        TTFText& t = teamPool[teamUsed++];
        t.LoadFont(f);
        return t;
    }
    TTFText& TakeChat(TTF_Font* f) {
        if (chatUsed == chatPool.size()) chatPool.emplace_back();
        TTFText& t = chatPool[chatUsed++];
        t.LoadFont(f);
        return t;
    }
};

void MainMenu::FreeRoomModern() {
    if (!roomModern) return;
    for (SDL_Texture* t : roomModern->ball) SDL_DestroyTexture(t);
    RoomModernText* r = roomModern;
    roomModern = nullptr;
    TTF_Font* fonts[] = {r->title, r->value, r->small, r->body, r->chat, r->chatBold, r->tiny, r->hint};
    delete r;  // the TTFTexts only borrow the fonts, so they go first
    for (TTF_Font* f : fonts) if (f) TTF_CloseFont(f);
}

MainMenu::RoomModernText& MainMenu::RoomModern() {
    if (!roomModern) {
        roomModern = new RoomModernText;
        RoomModernText& r = *roomModern;
        const std::string display = ASSET("/gfx/Baloo2-ExtraBold.ttf");
        const std::string body = ASSET("/gfx/DroidSans.ttf");
        r.title = TTF_OpenFont(display.c_str(), 24);
        r.value = TTF_OpenFont(display.c_str(), 18);
        r.small = TTF_OpenFont(display.c_str(), 15);
        r.body = TTF_OpenFont(body.c_str(), 17);
        r.chat = TTF_OpenFont(body.c_str(), 15);
        r.chatBold = TTF_OpenFont(body.c_str(), 15);
        r.tiny = TTF_OpenFont(body.c_str(), 13);
        r.hint = TTF_OpenFont(body.c_str(), 13);
        if (r.chatBold) TTF_SetFontStyle(r.chatBold, TTF_STYLE_BOLD);
        if (r.tiny) TTF_SetFontStyle(r.tiny, TTF_STYLE_BOLD);
        SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
        for (int i = 0; i < 5; i++)
            r.ball[i] = IMG_LoadTexture(rend, ASSET(("/gfx/balls/modern/bubble-" +
                std::to_string(kSeatBalls[i]) + ".png").c_str()).c_str());
    }
    return *roomModern;
}

void MainMenu::NetPanelRoomRenderModern() {
    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* game = netClient->GetCurrentGame();
    if (!game) return;
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    RoomModernText& T = RoomModern();
    T.used = 0;

    const std::string me = netClient->GetPlayerNick();
    const bool isHost = game->creator == me;
    const int seated = (int)game->players.size();
    const bool bigRoom = game->maxPlayers > 5;
    const bool hasStart = isHost && seated > 1;

    // The world map stays visible behind, dimmed, as the in-game cards do.
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 5, 10, 22, 158);
    SDL_RenderFillRect(rend, nullptr);

    auto focusRow = [&](const SDL_Rect& r, float radius) {
        const SDL_FRect f = ToFRect(r);
        FillRoundRect(rend, f, radius, kRowFocus);
        StrokeRoundRect(rend, f, radius, 2.f, kIce);
    };
    // A stepped value right-aligned at xr, with < > when focused. Returns the
    // left edge of the value block, where its tap split starts.
    auto stepper = [&](const char* v, int xr, int cy, bool focused) {
        const int right = focused ? xr - 14 : xr;
        const int w = DrawTextLine(rend, T.Take(T.value), v, focused ? kValue : kFrost,
                                   kTextShadow, right, cy, 2);
        if (focused) {
            DrawTextLine(rend, T.Take(T.value), "\xE2\x80\xB9", kIce, kNoShadow, right - w - 10, cy - 1, 1);
            DrawTextLine(rend, T.Take(T.value), "\xE2\x80\xBA", kIce, kNoShadow, xr - 3, cy - 1, 1);
        }
        return right - w - 20;
    };
    // A stepped row's tap targets: the label and the value's left half step
    // back, the value's right half steps forward (menulist::List::End).
    // Clipped to `view` so a half-scrolled row only takes taps where it shows.
    auto splitTap = [&](int index, const SDL_Rect& r, int valueLeft, const SDL_Rect& view) {
        const int split = (valueLeft + r.x + r.w) / 2;
        AddPanelTapRow(index, Visible({r.x, r.y, split - r.x, r.h}, view), -1, false, SDLK_LEFT);
        AddPanelTapRow(index, Visible({split, r.y, r.x + r.w - split, r.h}, view), -1, false, SDLK_RIGHT);
    };

    // ---- Top bar: the room, who you are, HELP and the host's Start ----------
    DrawCard(rend, kTopBar);
    char title[48];
    snprintf(title, sizeof(title), "%.16s's room", game->creator.c_str());
    int x = 24 + DrawTextLine(rend, T.Take(T.title), title, kValue, kTextShadow, 24, kTopCy) + 12;
    x += Chip(rend, T.Take(T.tiny), isHost ? "HOST" : "GUEST", x, kTopCy, kGold, isHost) + 6;
    char count[32];
    snprintf(count, sizeof(count), "%d / %d", seated, game->maxPlayers);
    Chip(rend, T.Take(T.tiny), count, x, kTopCy, kLabel);

    // HELP for everyone (a joiner plays by these rules too); F1 is the key.
    int helpRight = kStartBtn.x - 10;
    if (hasStart) {
        DrawPill(rend, T.Take(T.value), "START", kStartBtn, true, selectedActionIndex == kRoomStart);
        AddPanelTapRow(kRoomStart, kStartBtn);
    } else {
        const char* wait = isHost ? "Waiting for players" : "Host starts the game";
        DrawTextLine(rend, T.Take(T.chat), wait, kLabel, kNoShadow, 616, kTopCy, 2);
        helpRight = 616 - TextW(T.chat, wait) - 14;
    }
    const SDL_Rect helpBtn = {helpRight - 78, 15, 78, 32};
    DrawPill(rend, T.Take(T.small), "HELP", helpBtn, false, selectedActionIndex == kRoomHelpTapIndex);
    AddPanelTapRow(kRoomHelpTapIndex, helpBtn, -1, false, SDLK_F1);

    // ---- Match rules: the rules, the per-player table, Bots and Skill -------
    // One list in the Up/Down order, scrolled inside the card.
    const SDL_Rect rules = bigRoom ? SDL_Rect{8, kCardTop, 300, kCardH} : SDL_Rect{8, kCardTop, 370, kCardH};
    DrawCard(rend, rules);
    DrawTextLine(rend, T.Take(T.tiny), "MATCH RULES", kLabel, kNoShadow, rules.x + 18, kCardTop + 17);
    const SDL_Rect view = {rules.x + 8, kCardTop + 32, rules.w - 24, rules.h - 38};

    char modeValue[64];
    ModeValueLabel(modeValue, sizeof(modeValue), netGameMode, netRaceTargetIndex, netTimedSecondsIndex);
    struct Rule { int idx; const char* label; const char* value; int on; };  // on: -1 stepper
    const Rule ruleRows[] = {
        {kRoomMode, "Game mode", GameModeName(netGameMode), -1},
        {kRoomModeValue, netGameMode == GameMode::Race ? "First to pop" : "Round length", modeValue, -1},
        {kRoomMalus, "Attack bubbles", AttackModeName(netAttackMode), -1},
        {kRoomChain, "Chain reaction", nullptr, chainReactionEnabled ? 1 : 0},
        {kRoomTarget, "Solo targeting", nullptr, singlePlayerTargetting ? 1 : 0},
        {kRoomVictories, "Victories limit", kVictoriesLabels[victoriesLimitIndex], -1},
        {kRoomMouse, "Mouse / touch aim", nullptr, netRoomMouseEnabled ? 1 : 0},
    };
    constexpr int kRuleH = 32, kRulePitch = 36, kGridHeadH = 30, kGridH = 30, kGridPitch = 33;
    // Content layout first (y from the top of the list), so the scroll can be
    // settled before anything is drawn.
    int contentH = 0, selTop = -1, selBottom = -1;
    auto place = [&](int idx, int h, int pitch) {
        const int top = contentH;
        if (idx == selectedActionIndex) { selTop = top; selBottom = top + h; }
        contentH += pitch;
        return top;
    };
    std::vector<std::pair<const Rule*, int>> ruleTops;
    for (const Rule& rule : ruleRows)
        if (!RoomRowHidden(rule.idx)) ruleTops.push_back({&rule, place(rule.idx, kRuleH, kRulePitch)});
    const int gridHeadTop = contentH + 4;
    contentH += 4 + kGridHeadH;
    int gridTops[4];
    for (int row = 0; row < 4; row++) gridTops[row] = place(kRoomGridFirst + row, kGridH, kGridPitch);
    int botsTop = -1, skillTop = -1;
    if (isHost) {
        contentH += 10;
        botsTop = place(kRoomBots, kRuleH, kRulePitch);
        skillTop = place(kRoomBotSkill, kRuleH, kRulePitch);
    }
    contentH -= 4;  // no gap under the last row
    if (selTop >= 0) roomRulesScroll = ScrollToShow(roomRulesScroll, selTop, selBottom, view.h, contentH, 22);
    else roomRulesScroll = std::clamp(roomRulesScroll, 0, std::max(0, contentH - view.h));
    const int y0 = view.y - roomRulesScroll;
    const int rowX = view.x, rowW = view.w;

    SDL_SetRenderClipRect(rend, &view);
    for (const auto& [rule, top] : ruleTops) {
        const SDL_Rect r = {rowX, y0 + top, rowW, kRuleH};
        const bool f = selectedActionIndex == rule->idx;
        if (f) focusRow(r, 9);
        else FillRoundRect(rend, ToFRect(r), 9, kRowIdle);
        const int cy = r.y + r.h / 2;
        DrawTextLine(rend, T.Take(T.body), rule->label, f ? kValue : kFrost, kNoShadow, r.x + 12, cy);
        if (rule->on >= 0) {
            Switch(rend, r.x + r.w - 12 - kSwitchW, cy, rule->on == 1);
            AddPanelTapRow(rule->idx, Visible(r, view));
        } else {
            splitTap(rule->idx, r, stepper(rule->value, r.x + r.w - 12, cy, f), view);
        }
    }

    // The per-player table: ALL plus one column per seat, up to five, as in
    // the classic room. ALL shows a value only when every seat has it.
    {
        const int cols = std::max(1, std::min(5, seated));
        const int labelW = bigRoom ? 108 : 134;
        const int colW = (rowW - labelW) / (cols + 1);
        const int headCy = y0 + gridHeadTop + kGridHeadH / 2 + 2;
        SDL_SetRenderDrawColor(rend, kLine.r, kLine.g, kLine.b, kLine.a);
        SDL_FRect line = {(float)rowX, (float)(y0 + gridHeadTop), (float)rowW, 1};
        SDL_RenderFillRect(rend, &line);
        DrawTextLine(rend, T.Take(T.tiny), bigRoom ? "SEATS 1-5" : "EACH PLAYER", kLabel, kNoShadow,
                     rowX + 12, headCy);
        auto colX = [&](int c) { return rowX + labelW + c * colW; };  // c = 0 is ALL
        for (int c = 0; c <= cols; c++) {
            char head[4];
            snprintf(head, sizeof(head), c ? "P%d" : "ALL", c);
            DrawTextLine(rend, T.Take(T.tiny), head, kLabel, kNoShadow, colX(c) + colW / 2, headCy, 1);
        }
        int myCol = -1;
        for (int i = 0; i < seated && i < 5; i++) if (game->players[i].nick == me) myCol = i + 1;

        const char* labels[4] = {"Max colors", "Row collapse", "Aim guide", "Team"};
        auto value = [&](int row, int p, char* out) {
            if (row == 0) snprintf(out, 8, "%d", playerColorCounts[p]);
            else if (row == 1 && playerNoCompress[p]) snprintf(out, 8, "off");
            else if (row == 1) snprintf(out, 8, "%d", playerNewRowShots[p]);
            else if (row == 2) snprintf(out, 8, "%s", playerAimGuide[p] ? "on" : "off");
            else if (netPlayerTeams[p] == kNoTeam) snprintf(out, 8, "-");
            else snprintf(out, 8, "%d", netPlayerTeams[p]);
        };
        for (int row = 0; row < 4; row++) {
            const int idx = kRoomGridFirst + row;
            const SDL_Rect band = {rowX, y0 + gridTops[row], rowW, kGridH};
            const int cy = band.y + band.h / 2;
            const bool rowFocus = selectedActionIndex == idx;
            FillRoundRect(rend, ToFRect(band), 8, rowFocus ? kRowFocus : kRowIdle);
            DrawTextLine(rend, T.Take(T.chat), labels[row], rowFocus ? kValue : kFrost, kNoShadow, band.x + 12, cy);
            for (int c = 0; c <= cols; c++) {
                char text[8];
                int team = kNoTeam;
                if (c == 0) {
                    bool same = true;
                    char first[8], other[8];
                    value(row, 0, first);
                    for (int p = 1; p < cols; p++) {
                        value(row, p, other);
                        if (strcmp(first, other) != 0) { same = false; break; }
                    }
                    snprintf(text, sizeof(text), "%s", same ? first : "-");
                    if (row == 3 && same) team = netPlayerTeams[0];
                } else {
                    value(row, c - 1, text);
                    if (row == 3) team = netPlayerTeams[c - 1];
                }
                const SDL_Rect cell = {colX(c) + 2, band.y + 2, colW - 4, band.h - 4};
                AddPanelTapRow(idx, Visible(cell, view), c);
                if (team != kNoTeam) {
                    const SDL_Color tc = kTeamColors[ClampTeamNumber(team) - 1];
                    FillRoundRect(rend, ToFRect(cell), 6, {tc.r, tc.g, tc.b, 150});
                }
                // Focus on a cell the player may change: the host any cell,
                // a guest only their own team.
                const bool canChange = c == 0 ? isHost : (isHost || (row == 3 && c == myCol));
                const bool cellFocus = rowFocus && currentPlayerCol == c && canChange;
                if (cellFocus) StrokeRoundRect(rend, ToFRect(cell), 6, 2.f, kIce);
                const bool muted = !strcmp(text, "off") || !strcmp(text, "-");
                DrawTextLine(rend, T.Take(T.small), text, muted ? kLabel : kValue, kNoShadow,
                             cell.x + cell.w / 2, cy, 1);
            }
        }
    }

    // Bots: the host's to add (a bot is an ordinary player to everyone else).
    if (isHost) {
        const int maxBots = MaxRoomBots(seated, game->maxPlayers, netRoomBotCount);
        char bots[32];
        if (maxBots <= 0 && netRoomBotCount == 0) snprintf(bots, sizeof(bots), "0 (full)");
        else snprintf(bots, sizeof(bots), "%d of %d", netRoomBotCount, maxBots);
        SDL_SetRenderDrawColor(rend, kLine.r, kLine.g, kLine.b, kLine.a);
        SDL_FRect divider = {(float)rowX, (float)(y0 + botsTop - 6), (float)rowW, 1};
        SDL_RenderFillRect(rend, &divider);
        const struct { int idx; int top; const char* label; const char* value; } side[2] = {
            {kRoomBots, botsTop, "Bots", bots},
            {kRoomBotSkill, skillTop, "Bot skill", LocalMPBotSkillName(netRoomBotSkill)},
        };
        for (const auto& s : side) {
            const SDL_Rect r = {rowX, y0 + s.top, rowW, kRuleH};
            const bool f = selectedActionIndex == s.idx;
            if (f) focusRow(r, 9);
            else FillRoundRect(rend, ToFRect(r), 9, kRowIdle);
            const int cy = r.y + r.h / 2;
            DrawTextLine(rend, T.Take(T.body), s.label, f ? kValue : kFrost, kNoShadow, r.x + 12, cy);
            splitTap(s.idx, r, stepper(s.value, r.x + r.w - 12, cy, f), view);
        }
    }
    SDL_SetRenderClipRect(rend, nullptr);
    DrawScrollBar(rend, rules.x + rules.w - 12, view, roomRulesScroll, contentH);

    // ---- Players ----------------------------------------------------------
    const SDL_Rect players = bigRoom ? SDL_Rect{314, kCardTop, 318, kCardH} : SDL_Rect{384, kCardTop, 248, kCardH};
    DrawCard(rend, players);
    DrawTextLine(rend, T.Take(T.tiny), "PLAYERS", kLabel, kNoShadow, players.x + 18, kCardTop + 17);
    // The page every seat's team is set on; A is its key from anywhere here.
    const SDL_Rect teamsBtn = {players.x + players.w - 112, kCardTop + 5, 104, 26};
    DrawPill(rend, T.Take(T.small), "Set Teams", teamsBtn, false, selectedActionIndex == kRoomSetTeamsTapIndex);
    AddPanelTapRow(kRoomSetTeamsTapIndex, teamsBtn, -1, false, SDLK_A);
    const int seatsTop = kCardTop + 38, seatsBottom = kCardTop + kCardH - 8;

    if (bigRoom) {
        // Two columns of slim rows; a tap on one opens Set Teams on that seat
        // (the host any seat, a guest their own), as in the classic room.
        const int cap = game->maxPlayers;
        const int perCol = (cap + 1) / 2;
        const int pitch = std::min(40, (seatsBottom - seatsTop) / std::max(1, perCol));
        const int colW = (players.w - 22) / 2;
        TTF_Font* nameFont = pitch < 30 ? T.small : T.value;
        for (int pi = 0; pi < cap; pi++) {
            const SDL_Rect r = {players.x + 8 + (pi / perCol) * (colW + 6), seatsTop + (pi % perCol) * pitch,
                                colW, pitch - 2};
            const int cy = r.y + r.h / 2;
            FillRoundRect(rend, ToFRect(r), 6, kRowIdle);
            char num[4];
            snprintf(num, sizeof(num), "%d", pi + 1);
            DrawTextLine(rend, T.Take(T.tiny), num, kLabel, kNoShadow, r.x + 20, cy, 2);
            if (pi >= seated) continue;
            const NetworkPlayer& pl = game->players[pi];
            const bool self = pl.nick == me;
            if (isHost || self) AddPanelTapRow(kRoomRosterTapBase + pi, r);
            auto it = netTeamOverrides.find(pl.nick);
            const int team = it == netTeamOverrides.end() ? kNoTeam : ClampTeamOrNone(it->second);
            if (team != kNoTeam)
                FillRoundRect(rend, {(float)r.x + 26, (float)cy - 5, 10, 10}, 5, kTeamColors[team - 1]);
            const bool host = pl.nick == game->creator;
            const int nameMax = r.w - 44 - (host ? ChipW(T.tiny, "H") + 4 : 0);
            const std::string nick = Fit(nameFont, pl.nick, nameMax);
            const int w = DrawTextLine(rend, T.Take(nameFont), nick.c_str(), self ? kIce : kValue, kNoShadow,
                                       r.x + 40, cy);
            if (host) Chip(rend, T.Take(T.tiny), "H", r.x + 44 + w, cy, kGold, true);
        }
    } else {
        const int seats = std::max(1, std::min(5, game->maxPlayers));
        for (int pi = 0; pi < seats; pi++) {
            const SDL_Rect r = {players.x + 8, seatsTop + pi * 40, players.w - 16, 36};
            const int cy = r.y + r.h / 2;
            char slot[4];
            snprintf(slot, sizeof(slot), "P%d", pi + 1);
            if (pi >= seated) {
                StrokeRoundRect(rend, ToFRect(r), 9, 1, kLine);
                StrokeRoundRect(rend, {(float)r.x + 10, (float)cy - 8, 16, 16}, 8, 2.f, {58, 75, 108, 255});
                DrawTextLine(rend, T.Take(T.chat), "Waiting for player...", kLabel, kNoShadow, r.x + 34, cy);
                continue;
            }
            FillRoundRect(rend, ToFRect(r), 9, kRowIdle);
            DrawTextLine(rend, T.Take(T.tiny), slot, kLabel, kNoShadow, r.x + r.w - 10, cy, 2);
            const NetworkPlayer& pl = game->players[pi];
            const bool self = pl.nick == me;
            const int team = ClampTeamOrNone(netPlayerTeams[pi]);
            if (team != kNoTeam) {
                FillRoundRect(rend, {(float)r.x + 9, (float)cy - 9, 18, 18}, 9, kTeamColors[team - 1]);
            } else if (T.ball[pi]) {
                SDL_FRect b = {(float)r.x + 8, (float)cy - 10, 20, 20};
                SDL_RenderTexture(rend, T.ball[pi], nullptr, &b);
            }
            const bool host = pl.nick == game->creator;
            const int chips = (host ? ChipW(T.tiny, "HOST") + 4 : 0) + (self ? ChipW(T.tiny, "YOU") + 4 : 0);
            const std::string nick = Fit(T.value, pl.nick, r.w - 34 - 30 - chips - 6);
            int cx = r.x + 34 + DrawTextLine(rend, T.Take(T.value), nick.c_str(), kValue, kNoShadow, r.x + 34, cy) + 6;
            if (host) cx += Chip(rend, T.Take(T.tiny), "HOST", cx, cy, kGold, true) + 4;
            if (self) Chip(rend, T.Take(T.tiny), "YOU", cx, cy, kIce, true);
        }
    }

}

// The chat card: drawn by NetPanelChatDockRender in the lobby and a room in
// the Modern style, which keeps its own message handling and calls these for
// drawing. Four lines at all times (the user's pick over a one-line strip
// that opens on a tap); grown upward while composing. The screen's footer
// hint is drawn here because it changes while composing.
void MainMenu::NetChatDockModern(bool expanded) {
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    RoomModernText& T = RoomModern();
    T.chatUsed = 0;
    const SDL_Rect card = expanded ? SDL_Rect{8, 58, 624, kChatTop + kChatH - 58} : SDL_Rect{8, kChatTop, 624, kChatH};
    DrawCard(rend, card);
    if (expanded) DrawTextLine(rend, T.TakeChat(T.tiny), "CHAT", kLabel, kNoShadow, 26, card.y + 17);

    // Index 0, Chat, is the input line itself.
    const SDL_Rect input = {16, kChatTop + kChatH - 48, 608, 36};
    if (!expanded) AddPanelTapRow(kRoomChat, input);
    const bool focused = expanded || selectedActionIndex == kRoomChat;
    FillRoundRect(rend, ToFRect(input), 18, {5, 10, 22, 180});
    StrokeRoundRect(rend, ToFRect(input), 18, focused ? 2.f : 1.f, focused ? kIce : Alpha(kIce, 72));
    const int cy = input.y + input.h / 2;
    const size_t len = strlen(networkChatInput);
    if (len == 0) {
        DrawTextLine(rend, T.TakeChat(T.chat), focused ? "Type a message, ENTER to send" : "Say something...",
                     kLabel, kNoShadow, input.x + 16, cy);
    } else {
        // The end of a long message, so the caret stays in view.
        const char* shown = networkChatInput + (len > 64 ? len - 64 : 0);
        char line[96];
        snprintf(line, sizeof(line), "%s%s%s", len > 64 ? "..." : "", shown, focused ? "_" : "");
        DrawTextLine(rend, T.TakeChat(T.chat), line, kValue, kNoShadow, input.x + 16, cy);
    }
    const char* hint = expanded ? "ENTER sends  \xC2\xB7  ESC cancels"
        : NetworkClient::Instance()->GetCurrentGame()
            ? "UP/DOWN scrolls  \xC2\xB7  ESC leave  \xC2\xB7  F1 help  \xC2\xB7  A teams"
            : "UP/DOWN scrolls  \xC2\xB7  ENTER create / join  \xC2\xB7  ESC leave server";
    DrawTextLine(rend, T.TakeChat(T.hint), hint, kLabel, kNoShadow, 320, kHintCy, 1);
}

// One message, its top at y (NetPanelChatDockRender spaces them 22 apart).
void MainMenu::NetChatLineModern(const ChatMessage& cm, int y) {
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    RoomModernText& T = RoomModern();
    const int cy = y + 11, x = 26, maxW = 596;
    if (cm.nick == "Server" || cm.message.find("***") == 0) {
        // Grey already marks a server line, so its "*** " goes.
        const char* msg = cm.message.c_str();
        if (cm.message.rfind("*** ", 0) == 0) msg += 4;
        DrawTextLine(rend, T.TakeChat(T.chat), Fit(T.chat, msg, maxW).c_str(), kLabel, kNoShadow, x, cy);
        return;
    }
    char nick[24];
    snprintf(nick, sizeof(nick), "%.16s:", cm.nick.c_str());
    const int w = DrawTextLine(rend, T.TakeChat(T.chatBold), nick, kIce, kNoShadow, x, cy);
    DrawTextLine(rend, T.TakeChat(T.chat), Fit(T.chat, cm.message, maxW - w - 6).c_str(), kFrost, kNoShadow,
                 x + w + 6, cy);
}

void MainMenu::TeamsPanelRenderModern() {
    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* room = netClient->GetCurrentGame();
    if (!room) return;
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    RoomModernText& T = RoomModern();
    T.teamUsed = 0;

    const bool isHost = room->creator == netClient->GetPlayerNick();
    const int mySlot = MyRoomSlot();
    const int playerCount = (int)room->players.size();
    teamSwatchTaps.clear();
    teamPlayerNameTaps.clear();
    teamAutoBalanceTaps.clear();
    teamsCursorPlayer = std::clamp(teamsCursorPlayer, 0, std::max(0, playerCount - 1));

    // A fresh backdrop: this page covers the room completely.
    if (netGameBackground) SDL_RenderTexture(rend, netGameBackground, nullptr, nullptr);
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 5, 10, 22, 168);
    SDL_RenderFillRect(rend, nullptr);

    // Top bar: the page, the room, and Done.
    DrawCard(rend, kTopBar);
    const int w = DrawTextLine(rend, T.TakeTeam(T.title), "SET TEAMS", kValue, kTextShadow, 24, kTopCy);
    char sub[64];
    snprintf(sub, sizeof(sub), "%d players  \xC2\xB7  %.16s's room", playerCount, room->creator.c_str());
    DrawTextLine(rend, T.TakeTeam(T.chat), sub, kLabel, kNoShadow, 24 + w + 14, kTopCy);
    teamsDoneRect = {520, 15, 104, 32};
    DrawPill(rend, T.TakeTeam(T.value), "DONE", teamsDoneRect, true, false);

    // Auto: the host's one-tap splits, also a keyboard row (teamsAutoFocus).
    int bodyTop = kCardTop;
    if (isHost) {
        DrawCard(rend, {8, kCardTop, 624, 46});
        const int cy = kCardTop + 23;
        DrawTextLine(rend, T.TakeTeam(T.tiny), "AUTO", kLabel, kNoShadow, 24, cy);
        const char* labels[kMaxTeams] = {"NONE", "2", "3", "4", "5"};
        int x = 74;
        for (int b = 0; b < kMaxTeams; b++) {
            const SDL_Rect r = {x, cy - 15, b == 0 ? 66 : 40, 30};
            const bool f = teamsAutoFocus == b;
            FillRoundRect(rend, ToFRect(r), 15, f ? kIce : Alpha(kIce, 26));
            if (!f) StrokeRoundRect(rend, ToFRect(r), 15, 1.5f, Alpha(kIce, 110));
            DrawTextLine(rend, T.TakeTeam(T.small), labels[b], f ? kInk : kFrost, kNoShadow,
                         r.x + r.w / 2, cy, 1);
            teamAutoBalanceTaps.push_back({r, b == 0 ? kNoTeam : b + 1});
            x += r.w + 8;
        }
        DrawTextLine(rend, T.TakeTeam(T.chat), "splits everyone evenly", kLabel, kNoShadow, x + 8, cy);
        bodyTop = kCardTop + 52;
    }

    const SDL_Rect body = {8, bodyTop, 624, 452 - bodyTop};
    DrawCard(rend, body);

    // Column heads: the teams in their own colours.
    const int teamX0 = 352, teamPitch = 44, teamW = 38;
    const int headCy = body.y + 17;
    DrawTextLine(rend, T.TakeTeam(T.tiny), "PLAYER", kLabel, kNoShadow, 24, headCy);
    for (int t = kNoTeam; t <= kMaxTeams; t++) {
        DrawTextLine(rend, T.TakeTeam(T.tiny), t == kNoTeam ? "NONE" : std::to_string(t).c_str(),
                     t == kNoTeam ? kLabel : kTeamColors[t - 1], kNoShadow,
                     teamX0 + t * teamPitch + teamW / 2, headCy, 1);
    }

    // Every player, scrolled inside the card so the cursor's row stays in view.
    constexpr int kRowH = 40, kRowPitch = 44;
    const SDL_Rect view = {16, body.y + 32, 600, body.h - 38};
    const int contentH = playerCount * kRowPitch - 4;
    const int curTop = teamsCursorPlayer * kRowPitch;
    teamsScroll = ScrollToShow(teamsScroll, curTop, curTop + kRowH, view.h, contentH, 24);
    SDL_SetRenderClipRect(rend, &view);
    for (int slot = 0; slot < playerCount; slot++) {
        const SDL_Rect r = {view.x, view.y + slot * kRowPitch - teamsScroll, view.w - 10, kRowH};
        if (r.y + r.h <= view.y || r.y >= view.y + view.h) continue;
        const NetworkPlayer& player = room->players[slot];
        const int cy = r.y + r.h / 2;
        const bool self = slot == mySlot;
        // The host moves anyone, everyone else only themselves.
        const bool editable = isHost || self;
        const bool focused = slot == teamsCursorPlayer && teamsAutoFocus < 0;
        if (focused) {
            FillRoundRect(rend, ToFRect(r), 10, kRowFocus);
            StrokeRoundRect(rend, ToFRect(r), 10, 2.f, kIce);
        } else {
            FillRoundRect(rend, ToFRect(r), 10, kRowIdle);
        }
        char num[4];
        snprintf(num, sizeof(num), "%d", slot + 1);
        DrawTextLine(rend, T.TakeTeam(T.tiny), num, kLabel, kNoShadow, r.x + 26, cy, 2);
        const int current = TeamOfSlot(slot);
        if (current != kNoTeam)
            FillRoundRect(rend, {(float)r.x + 36, (float)cy - 8, 16, 16}, 8, kTeamColors[current - 1]);
        else
            StrokeRoundRect(rend, {(float)r.x + 36, (float)cy - 8, 16, 16}, 8, 2.f, {58, 75, 108, 255});
        const bool host = player.nick == room->creator;
        const int chips = (host ? ChipW(T.tiny, "HOST") + 4 : 0) + (self ? ChipW(T.tiny, "YOU") + 4 : 0);
        const std::string nick = Fit(T.value, player.nick, teamX0 - 12 - (r.x + 62) - chips - 8);
        int cx = r.x + 62 + DrawTextLine(rend, T.TakeTeam(T.value), nick.c_str(), self ? kIce : kValue,
                                          kNoShadow, r.x + 62, cy) + 8;
        if (host) cx += Chip(rend, T.TakeTeam(T.tiny), "HOST", cx, cy, kGold, true) + 4;
        if (self) Chip(rend, T.TakeTeam(T.tiny), "YOU", cx, cy, kIce, true);
        if (editable) {
            const SDL_Rect nameTap = Visible({r.x, r.y, teamX0 - 8 - r.x, r.h}, view);
            if (nameTap.w > 0) teamPlayerNameTaps.push_back({nameTap, slot});
        }

        for (int t = kNoTeam; t <= kMaxTeams; t++) {
            const SDL_Rect b = {teamX0 + t * teamPitch, r.y + 5, teamW, r.h - 10};
            const SDL_Color col = t == kNoTeam ? kLabel : kTeamColors[t - 1];
            const bool on = t == current;
            const SDL_FRect fb = ToFRect(b);
            FillRoundRect(rend, fb, 8, on ? col : Alpha(kIce, 13));
            if (on) StrokeRoundRect(rend, fb, 8, 2, kValue);
            else StrokeRoundRect(rend, fb, 8, 1.5f, {col.r, col.g, col.b, (Uint8)(editable ? 140 : 50)});
            DrawTextLine(rend, T.TakeTeam(T.small), t == kNoTeam ? "-" : std::to_string(t).c_str(),
                         on ? kInk : (editable ? kFrost : kLabel), kNoShadow, b.x + b.w / 2, cy, 1);
            const SDL_Rect tap = Visible(b, view);
            if (editable && tap.w > 0) teamSwatchTaps.push_back({tap, slot, t});
        }
    }
    SDL_SetRenderClipRect(rend, nullptr);
    DrawScrollBar(rend, view.x + view.w - 4, view, teamsScroll, contentH);

    DrawTextLine(rend, T.TakeTeam(T.hint),
        isHost ? "UP/DOWN player or Auto  \xC2\xB7  LEFT/RIGHT or tap a team  \xC2\xB7  ENTER applies Auto  \xC2\xB7  ESC closes"
               : "LEFT/RIGHT or tap a team  \xC2\xB7  ESC / Done closes",
        kLabel, kNoShadow, 320, kHintCy, 1);
}

// The online lobby in the Modern style: a top bar, a Game rooms card (Create
// game room with its size, the tournament rows, then every room, scrolled
// inside the card), an Online card (the free players, Weekly rankings and
// Join Discord pinned at its foot), and the chat card. Drawing only, like the
// room: every row keeps the index the classic lobby gives it
// (LobbyTournamentIndex, LobbyRoomListStart, LobbyWeeklyIndex,
// LobbyDiscordIndex), so Up/Down, ENTER and the tap rows cannot tell the two
// apart. Uses the room's fonts.
void MainMenu::NetPanelLobbyRenderModern() {
    NetworkClient* netClient = NetworkClient::Instance();
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    RoomModernText& T = RoomModern();
    T.used = 0;

    const std::string me = netClient->GetPlayerNick();
    const std::vector<GameRoom> games = netClient->GetGameList();
    std::vector<NetworkPlayer> online = netClient->GetOpenPlayers();
    // You first, so the list is never empty while you are connected.
    std::stable_partition(online.begin(), online.end(),
                          [&](const NetworkPlayer& p) { return p.nick == me; });

    // The map and its player spots show through the dimming, as in the room.
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 5, 10, 22, 120);
    SDL_RenderFillRect(rend, nullptr);

    auto focusRow = [&](const SDL_Rect& r, float radius) {
        const SDL_FRect f = ToFRect(r);
        FillRoundRect(rend, f, radius, kRowFocus);
        StrokeRoundRect(rend, f, radius, 2.f, kIce);
    };

    // ---- Top bar --------------------------------------------------------------
    DrawCard(rend, kTopBar);
    int x = 24 + DrawTextLine(rend, T.Take(T.title), "Online lobby", kValue, kTextShadow, 24, kTopCy) + 12;
    const std::string nick = Fit(T.tiny, me, 120);
    x += Chip(rend, T.Take(T.tiny), nick.c_str(), x, kTopCy, kIce, true) + 6;
    char count[32];
    snprintf(count, sizeof(count), "%d IN LOBBY", (int)online.size());
    x += Chip(rend, T.Take(T.tiny), count, x, kTopCy, kLabel);
    const std::string host = Fit(T.chat, netClient->GetHost(), 612 - x - 16);
    DrawTextLine(rend, T.Take(T.chat), host.c_str(), kLabel, kNoShadow, 614, kTopCy, 2);

    // ---- Game rooms -----------------------------------------------------------
    const SDL_Rect rooms = {8, kCardTop, 392, kCardH};
    DrawCard(rend, rooms);
    {
        char head[32];
        snprintf(head, sizeof(head), games.empty() ? "GAME ROOMS" : "GAME ROOMS  \xC2\xB7  %d", (int)games.size());
        DrawTextLine(rend, T.Take(T.tiny), head, kLabel, kNoShadow, rooms.x + 18, kCardTop + 17);
    }
    const int rowX = rooms.x + 8, rowW = rooms.w - 16;
    int y = kCardTop + 32;
    {
        // Create game room: the label creates (ENTER), the size steps with
        // LEFT/RIGHT, so its value block splits into LEFT/RIGHT halves
        // (menulist::List::End's labelActivateKey row).
        const SDL_Rect r = {rowX, y, rowW, 42};
        const bool f = selectedActionIndex == 1;
        if (f) focusRow(r, 11);
        else FillRoundRect(rend, ToFRect(r), 11, kRowIdle);
        const int cy = r.y + r.h / 2;
        if (T.ball[0]) {
            SDL_FRect b = {(float)r.x + 10, (float)cy - 11, 22, 22};
            SDL_RenderTexture(rend, T.ball[0], nullptr, &b);
        }
        DrawTextLine(rend, T.Take(T.value), "Create game room", f ? kValue : kFrost, kTextShadow, r.x + 40, cy);
        char size[24];
        snprintf(size, sizeof(size), "%d players", kRoomSizes[netRoomSizeChoice]);
        const int xr = r.x + r.w - 12;
        const int right = f ? xr - 14 : xr;
        const int w = DrawTextLine(rend, T.Take(T.small), size, f ? kValue : kLabel, kNoShadow, right, cy, 2);
        if (f) {
            DrawTextLine(rend, T.Take(T.value), "\xE2\x80\xB9", kIce, kNoShadow, right - w - 10, cy - 1, 1);
            DrawTextLine(rend, T.Take(T.value), "\xE2\x80\xBA", kIce, kNoShadow, xr - 3, cy - 1, 1);
        }
        const int valueLeft = right - w - 20;
        const int split = (valueLeft + r.x + r.w) / 2;
        AddPanelTapRow(1, {r.x, r.y, valueLeft - r.x, r.h}, -1, false, SDLK_RETURN);
        AddPanelTapRow(1, {valueLeft, r.y, split - valueLeft, r.h}, -1, false, SDLK_LEFT);
        AddPanelTapRow(1, {split, r.y, r.x + r.w - split, r.h}, -1, false, SDLK_RIGHT);
        y += 46;
    }

    // Everything under Create scrolls: the tournament rows, then the rooms
    // (two lines each).
    struct Item { int index, top, h; };
    std::vector<Item> items;
    int contentH = 0;
    const int tourIdx = LobbyTournamentIndex();
    std::vector<TournamentListing> joinable;
    if (tourIdx >= 0) {
        joinable = LobbyJoinableTournaments();
        for (int i = 0; i <= (int)joinable.size(); i++) {
            items.push_back({tourIdx + i, contentH, 36});
            contentH += 40;
        }
    }
    const int roomStart = LobbyRoomListStart();
    for (int i = 0; i < (int)games.size(); i++) {
        items.push_back({roomStart + i, contentH, 48});
        contentH += 52;
    }
    contentH = std::max(0, contentH - 4);
    const SDL_Rect view = {rowX, y, rowW - 10, rooms.y + rooms.h - 6 - y};
    for (const Item& it : items)
        if (it.index == selectedActionIndex)
            lobbyRoomsScroll = ScrollToShow(lobbyRoomsScroll, it.top, it.top + it.h, view.h, contentH, 24);
    lobbyRoomsScroll = std::clamp(lobbyRoomsScroll, 0, std::max(0, contentH - view.h));

    if (games.empty() && tourIdx < 0) {
        DrawTextLine(rend, T.Take(T.chat), "No rooms yet. Create one and", kLabel, kNoShadow, rowX + 12, y + 18);
        DrawTextLine(rend, T.Take(T.chat), "others can join.", kLabel, kNoShadow, rowX + 12, y + 40);
    }
    SDL_SetRenderClipRect(rend, &view);
    for (const Item& it : items) {
        const SDL_Rect r = {view.x, view.y + it.top - lobbyRoomsScroll, view.w, it.h};
        if (r.y + r.h <= view.y || r.y >= view.y + view.h) continue;
        const bool f = selectedActionIndex == it.index;
        if (f) focusRow(r, 11);
        else FillRoundRect(rend, ToFRect(r), 11, kRowIdle);
        AddPanelTapRow(it.index, Visible(r, view));
        if (it.index < roomStart) {
            // A tournament row: Create tournament, then each joinable one.
            const int k = it.index - tourIdx;
            const int cy = r.y + r.h / 2;
            if (k == 0) {
                DrawTextLine(rend, T.Take(T.body), "Create tournament", f ? kValue : kFrost, kNoShadow, r.x + 12, cy);
                continue;
            }
            const auto& t = joinable[k - 1];
            char label[48], value[48];
            snprintf(label, sizeof(label), "%.16s's tournament", t.owner.c_str());
            snprintf(value, sizeof(value), "%d \xC2\xB7 %s", t.count, t.state.c_str());
            DrawTextLine(rend, T.Take(T.body), label, f ? kValue : kFrost, kNoShadow, r.x + 12, cy);
            DrawTextLine(rend, T.Take(T.chat), value, kLabel, kNoShadow, r.x + r.w - 12, cy, 2);
            continue;
        }
        const GameRoom& g = games[it.index - roomStart];
        char seats[16];
        snprintf(seats, sizeof(seats), "%d / %d", (int)g.players.size(), g.maxPlayers);
        const bool full = (int)g.players.size() >= g.maxPlayers;
        // A chip is drawn from its left edge, so measure it first.
        int cx = r.x + r.w - 10 - ChipW(T.tiny, seats);
        Chip(rend, T.Take(T.tiny), seats, cx, r.y + 16, full ? kLose : kIce);
        if (g.started) {
            cx -= ChipW(T.tiny, "PLAYING") + 6;
            Chip(rend, T.Take(T.tiny), "PLAYING", cx, r.y + 16, kGold);
        }
        char title[48];
        snprintf(title, sizeof(title), "%.16s's room", g.creator.c_str());
        const std::string name = Fit(T.value, title, cx - 8 - (r.x + 12));
        DrawTextLine(rend, T.Take(T.value), name.c_str(), f ? kValue : kFrost, kTextShadow, r.x + 12, r.y + 16);
        std::string names;
        for (const NetworkPlayer& p : g.players) {
            if (!names.empty()) names += ", ";
            names += p.nick;
        }
        DrawTextLine(rend, T.Take(T.chat), Fit(T.chat, names, r.w - 24).c_str(), kLabel, kNoShadow,
                     r.x + 12, r.y + 35);
    }
    SDL_SetRenderClipRect(rend, nullptr);
    DrawScrollBar(rend, rooms.x + rooms.w - 12, view, lobbyRoomsScroll, contentH);

    // ---- Online -----------------------------------------------------------------
    const SDL_Rect side = {406, kCardTop, 226, kCardH};
    DrawCard(rend, side);
    {
        char head[32];
        snprintf(head, sizeof(head), "ONLINE  \xC2\xB7  %d", (int)online.size());
        DrawTextLine(rend, T.Take(T.tiny), head, kLabel, kNoShadow, side.x + 18, kCardTop + 17);
    }
    // This week's round-wins rank after each name; a no-op on an old server.
    netClient->MaybeRefreshWeekly();
    const auto& ranks = netClient->weekly.lobbyWinsRank;

    const int discordIdx = LobbyDiscordIndex(games.size());
    const int weeklyIdx = LobbyWeeklyIndex(games.size());
    const int pinned = 1 + (discordIdx >= 0 ? 1 : 0);
    constexpr int kPinH = 32, kPinPitch = 36;
    const int pinTop = side.y + side.h - 8 - pinned * kPinPitch + 4;
    // As many names as fit above the pinned rows; the last line says how
    // many more there are when they do not all fit.
    constexpr int kNamePitch = 27;
    const int nameTop = kCardTop + 32;
    const int fit = std::max(1, (pinTop - 8 - nameTop) / kNamePitch);
    const int shown = (int)online.size() > fit ? fit - 1 : (int)online.size();
    for (int i = 0; i < shown; i++) {
        const NetworkPlayer& p = online[i];
        const bool self = p.nick == me;
        const int cy = nameTop + i * kNamePitch + kNamePitch / 2;
        FillRoundRect(rend, {(float)side.x + 18, (float)cy - 4, 8, 8}, 4, {104, 220, 151, 255});
        // The platform chip on the right, then the rank, then the name in
        // what is left.
        int right = side.x + side.w - 14;
        PlayerBadge badge;
        if (GetPlatformBadge(p.platform, badge)) {
            const int bw = TextW(T.tiny, badge.label);
            const SDL_FRect chip = {(float)right - bw - 12, (float)cy - 10, (float)bw + 12, 20};
            FillRoundRect(rend, chip, 6, badge.fill);
            DrawTextLine(rend, T.Take(T.tiny), badge.label, badge.text, kNoShadow, (int)chip.x + 6, cy);
            right = (int)chip.x - 6;
        }
        auto rank = ranks.find(p.nick);
        if (rank != ranks.end() && rank->second > 0) {
            const std::string text = "#" + std::to_string(rank->second);
            right -= DrawTextLine(rend, T.Take(T.small), text.c_str(), kGold, kNoShadow, right, cy, 2) + 6;
        }
        const int youW = self ? ChipW(T.tiny, "YOU") + 6 : 0;
        const std::string name = Fit(T.small, p.nick, right - (side.x + 32) - youW);
        const int nw = DrawTextLine(rend, T.Take(T.small), name.c_str(), self ? kIce : kValue, kNoShadow,
                                    side.x + 32, cy);
        if (self) Chip(rend, T.Take(T.tiny), "YOU", side.x + 32 + nw + 6, cy, kIce, true);
    }
    if (shown < (int)online.size()) {
        char more[32];
        snprintf(more, sizeof(more), "+ %d more", (int)online.size() - shown);
        DrawTextLine(rend, T.Take(T.chat), more, kLabel, kNoShadow, side.x + 32,
                     nameTop + shown * kNamePitch + kNamePitch / 2);
    } else if (online.size() <= 1) {
        DrawTextLine(rend, T.Take(T.chat), "No one else here yet", kLabel, kNoShadow, side.x + 18,
                     nameTop + shown * kNamePitch + kNamePitch / 2);
    }

    const struct { int idx; const char* label; } pins[2] = {
        {weeklyIdx, "Weekly rankings"},
        {discordIdx, "Join Discord"},
    };
    for (int i = 0; i < pinned; i++) {
        const SDL_Rect r = {side.x + 8, pinTop + i * kPinPitch, side.w - 16, kPinH};
        const bool f = selectedActionIndex == pins[i].idx;
        if (f) focusRow(r, 9);
        else FillRoundRect(rend, ToFRect(r), 9, kRowIdle);
        const int cy = r.y + r.h / 2;
        DrawTextLine(rend, T.Take(T.body), pins[i].label, f ? kValue : kFrost, kNoShadow, r.x + 12, cy);
        DrawTextLine(rend, T.Take(T.value), "\xE2\x80\xBA", kIce, kNoShadow, r.x + r.w - 14, cy - 1, 1);
        AddPanelTapRow(pins[i].idx, r);
    }
}

// The LAN and NET GAME server lists in the Modern style: a Servers card (row
// 0's action -- Host a server or enter an address -- then every server, two
// lines each, scrolled inside the card) and a You card (Set name, Account
// code, and on NET the Discord invite), with the connect status at its foot.
// Same indices, same status text and the same cancel tap target as
// ServerListPanelRender, whose fetch polling runs before this is called.
void MainMenu::ServerListPanelRenderModern(bool isLAN) {
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    RoomModernText& T = RoomModern();
    T.used = 0;
    int& menuIndex = isLAN ? lanMenuIndex : netMenuIndex;
    const std::vector<ServerInfo>& servers = isLAN ? discoveredServers : publicServers;

    if (netGameBackground) SDL_RenderTexture(rend, netGameBackground, nullptr, nullptr);
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 5, 10, 22, 150);
    SDL_RenderFillRect(rend, nullptr);
    BeginPanelTapRows(&menuIndex);

    auto focusRow = [&](const SDL_Rect& r, float radius) {
        const SDL_FRect f = ToFRect(r);
        FillRoundRect(rend, f, radius, kRowFocus);
        StrokeRoundRect(rend, f, radius, 2.f, kIce);
    };

    // ---- Top bar --------------------------------------------------------------
    DrawCard(rend, kTopBar);
    int x = 24 + DrawTextLine(rend, T.Take(T.title), isLAN ? "LAN GAME" : "NET GAME", kValue, kTextShadow,
                              24, kTopCy) + 12;
    if (!servers.empty()) {
        char count[32];
        snprintf(count, sizeof(count), "%d SERVER%s", (int)servers.size(), servers.size() == 1 ? "" : "S");
        Chip(rend, T.Take(T.tiny), count, x, kTopCy, kLabel);
    }

    // ---- Servers ----------------------------------------------------------------
    const SDL_Rect card = {8, kCardTop, 400, 392};
    DrawCard(rend, card);
    DrawTextLine(rend, T.Take(T.tiny), isLAN ? "LOCAL NETWORK" : "PUBLIC SERVERS", kLabel, kNoShadow,
                 card.x + 18, kCardTop + 17);
    const SDL_Rect view = {card.x + 8, kCardTop + 32, card.w - 26, card.h - 38};
    constexpr int kFirstH = 40, kRowH = 50, kPitch = 54;
    const bool busy = isLAN ? lanFetchInProgress.load() : serverFetchInProgress.load();
    const int contentH = kFirstH + 4 + (busy || servers.empty() ? 60 : (int)servers.size() * kPitch - 4);
    if (menuIndex == 0) serverListScroll = 0;
    else if (menuIndex >= 1 && menuIndex <= (int)servers.size()) {
        const int top = kFirstH + 4 + (menuIndex - 1) * kPitch;
        serverListScroll = ScrollToShow(serverListScroll, top, top + kRowH, view.h, contentH, 24);
    }
    serverListScroll = std::clamp(serverListScroll, 0, std::max(0, contentH - view.h));
    const int y0 = view.y - serverListScroll;

    SDL_SetRenderClipRect(rend, &view);
    {
        // Row 0: LAN hosts a server here; Net opens the manual-entry form.
        const SDL_Rect r = {view.x, y0, view.w, kFirstH};
        const bool f = menuIndex == 0;
        if (f) focusRow(r, 11);
        else StrokeRoundRect(rend, ToFRect(r), 11, 1.f, kLine);
        const char* label = isLAN ? (serverHosting ? "Server running (rescan)" : "+  Host a server")
                                  : "+  Enter a server address...";
        DrawTextLine(rend, T.Take(T.value), label, kIce, kNoShadow, r.x + 14, r.y + r.h / 2);
        AddPanelTapRow(0, Visible(r, view));
    }
    const int listTop = y0 + kFirstH + 4;
    if (busy || servers.empty()) {
        const char* line1 = busy ? (isLAN ? "Scanning local network..." : "Fetching server list...")
                                 : (isLAN ? "No servers found" : "No public servers listed");
        DrawTextLine(rend, T.Take(T.body), line1, kLabel, kNoShadow, view.x + 14, listTop + 18);
        if (!busy)
            DrawTextLine(rend, T.Take(T.chat), isLAN ? "Start one: fb-server -l" : "Press R to refresh.",
                         kLabel, kNoShadow, view.x + 14, listTop + 42);
    } else {
        for (int i = 0; i < (int)servers.size(); i++) {
            const SDL_Rect r = {view.x, listTop + i * kPitch, view.w, kRowH};
            if (r.y + r.h <= view.y || r.y >= view.y + view.h) continue;
            const ServerInfo& s = servers[i];
            const bool offline = s.latencyMs < 0;
            const bool f = menuIndex == i + 1;
            if (f) focusRow(r, 11);
            else FillRoundRect(rend, ToFRect(r), 11, kRowIdle);
            // Ping as a coloured chip: green when quick, gold when slow.
            char ping[24];
            snprintf(ping, sizeof(ping), offline ? "offline" : "%d ms", s.latencyMs);
            const SDL_Color pc = offline ? kLabel : s.latencyMs < 80 ? SDL_Color{111, 240, 166, 255} : kGold;
            const int pw = ChipW(T.small, ping);
            Chip(rend, T.Take(T.small), ping, r.x + r.w - 12 - pw, r.y + r.h / 2, pc);
            const std::string addr = s.host + ":" + std::to_string(s.port);
            const std::string name = Fit(T.value, s.name.empty() ? addr : s.name, r.w - 40 - pw);
            DrawTextLine(rend, T.Take(T.value), name.c_str(), offline ? kLabel : kValue, kTextShadow, r.x + 14,
                         r.y + 17);
            DrawTextLine(rend, T.Take(T.chat), Fit(T.chat, offline ? addr + "  \xC2\xB7  no answer" : addr,
                                                   r.w - 40 - pw).c_str(),
                         kLabel, kNoShadow, r.x + 14, r.y + 36);
            // Offline servers stay tappable: selecting one is how the player
            // reads its address, and keyboard/gamepad nav can land on them too.
            AddPanelTapRow(i + 1, Visible(r, view));
        }
    }
    SDL_SetRenderClipRect(rend, nullptr);
    DrawScrollBar(rend, card.x + card.w - 13, view, serverListScroll, contentH);

    // ---- You: name, account, Discord, and the connect status ---------------
    const SDL_Rect side = {414, kCardTop, 218, 392};
    DrawCard(rend, side);
    DrawTextLine(rend, T.Take(T.tiny), "YOU", kLabel, kNoShadow, side.x + 18, kCardTop + 17);
    const char* curNick = networkPreNick[0] != '\0' ? networkPreNick
#ifdef __ANDROID__
        : (getenv("USER") ? getenv("USER") : "android_user");
#else
        : (getenv("USER") ? getenv("USER") : "unnamed");
#endif
    int sy = kCardTop + 32;
    // A two-line row that opens something: what it is, then its value.
    auto openRow = [&](int index, const char* label, const std::string& value) {
        const SDL_Rect r = {side.x + 8, sy, side.w - 16, 50};
        const bool f = menuIndex == index;
        if (f) focusRow(r, 11);
        else FillRoundRect(rend, ToFRect(r), 11, kRowIdle);
        DrawTextLine(rend, T.Take(T.chat), label, kLabel, kNoShadow, r.x + 12, r.y + 15);
        DrawTextLine(rend, T.Take(T.value), Fit(T.value, value, r.w - 44).c_str(), f ? kValue : kFrost,
                     kNoShadow, r.x + 12, r.y + 34);
        DrawTextLine(rend, T.Take(T.value), "\xE2\x80\xBA", kIce, kNoShadow, r.x + r.w - 14, r.y + r.h / 2 - 1, 1);
        AddPanelTapRow(index, r);
        sy += 54;
    };
    openRow(ServerListSetNameIndex(isLAN), "Name", curNick);
    openRow(ServerListAccountIndex(isLAN), "Account code", "view / move");
    const int discordIdx = isLAN ? -1 : ServerListDiscordIndex();
    if (discordIdx >= 0) {
        sy += 8;
        DrawTextLine(rend, T.Take(T.tiny), "COMMUNITY", kLabel, kNoShadow, side.x + 18, sy + 9);
        sy += 22;
        openRow(discordIdx, "Alerts when players join", "Join Discord");
    }

    // Connecting now takes real, visible time on both platforms, so the
    // status shows the client's actual state (see ServerListPanelRender).
    NetworkClient* statusClient = NetworkClient::Existing();
    const bool connecting = pendingLobbyConnect || (statusClient && statusClient->IsConnecting());
    if (connecting) {
        // A tap target for cancelling, not just ESC (CLAUDE.md input parity).
        cancelConnectTapRect = {side.x + 8, side.y + side.h - 66, side.w - 16, 58};
        FillRoundRect(rend, ToFRect(cancelConnectTapRect), 11, {255, 216, 74, 30});
        StrokeRoundRect(rend, ToFRect(cancelConnectTapRect), 11, 1.5f, kGold);
        DrawTextLine(rend, T.Take(T.value), "Connecting...", kGold, kNoShadow, side.x + 22, side.y + side.h - 47);
        DrawTextLine(rend, T.Take(T.chat), "Tap here or ESC to cancel", kLabel, kNoShadow, side.x + 22,
                     side.y + side.h - 24);
    } else {
        cancelConnectTapRect = {0, 0, 0, 0};  // not showing: nothing to hit
        if (!connectErrorMsg.empty()) {
            // Word-wrapped into the card, bottom up, at most three lines.
            std::vector<std::string> lines;
            std::string line, word;
            const int maxW = side.w - 32;
            auto flush = [&]() { if (!line.empty()) lines.push_back(line); line.clear(); };
            for (size_t i = 0; i <= connectErrorMsg.size(); i++) {
                const char c = i < connectErrorMsg.size() ? connectErrorMsg[i] : ' ';
                if (c != ' ') { word += c; continue; }
                if (word.empty()) continue;
                const std::string tryLine = line.empty() ? word : line + " " + word;
                if (TextW(T.chat, tryLine.c_str()) > maxW && !line.empty()) { flush(); line = word; }
                else line = tryLine;
                word.clear();
            }
            flush();
            if (lines.size() > 3) { lines.resize(3); lines[2] = Fit(T.chat, lines[2] + "...", maxW); }
            int ly = side.y + side.h - 18 - ((int)lines.size() - 1) * 20;
            for (const std::string& l : lines) {
                DrawTextLine(rend, T.Take(T.chat), l.c_str(), kLose, kNoShadow, side.x + 16, ly);
                ly += 20;
            }
        }
    }

    DrawTextLine(rend, T.Take(T.hint),
                 connecting ? "Connecting...  \xC2\xB7  ESC cancel"
                 : isLAN ? "UP/DOWN select  \xC2\xB7  ENTER connect  \xC2\xB7  R rescan  \xC2\xB7  ESC back"
                         : "UP/DOWN select  \xC2\xB7  ENTER connect  \xC2\xB7  R refresh  \xC2\xB7  ESC back",
                 kLabel, kNoShadow, 320, kHintCy, 1);
}
