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

// The online game room in the Modern menu style (MENU_THEME_MODERN): a top
// bar with the room's name and the host's Start button, a Match rules card
// with the per-player table under it, a Players card, and the chat as a card
// along the bottom (NetPanelChatDockRender). Drawing only: every row keeps its
// GameRoomRow index, so Up/Down/Left/Right/ENTER and the room's state are the
// classic room's, and every row is registered as a tap row the same way
// (stepped values split at their middle into LEFT/RIGHT, as menulist does).
//
// Input parity (CLAUDE.md): every row joins the room's Up/Down walk and is a
// tap row; the focused row is drawn with the ice edge and a focused stepper
// shows its < > arrows; the chat card's hint names ESC, F1 and A, the keys
// with no row of their own.
//
// The Set Teams page (TeamsPanelRenderModern, at the end) is here too, to
// share the fonts; it publishes the same tap rects as the classic page in
// mainmenu_teampanel.cpp, which keeps all of its input handling.

namespace {
using namespace modernui;

constexpr SDL_Rect kTopBar = {12, 10, 616, 42};
constexpr SDL_Rect kStartBtn = {506, 16, 110, 30};
constexpr SDL_Rect kHelpBtn = {396, 19, 52, 24};
constexpr SDL_Rect kTeamsBtn = {532, 68, 86, 22};
constexpr int kCardTop = 60, kCardH = 300;

const char* const kVictoriesLabels[] = {"None", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                                        "10", "11", "12", "15", "20", "30", "50", "100"};
// Seats with no team set show a candy bubble instead of a team colour.
constexpr int kSeatBalls[5] = {7, 3, 4, 5, 6};
}  // namespace

// Fonts are opened once; every line on screen gets its own TTFText from a
// pool, taken in the same order each frame, so a line that does not change
// keeps its texture.
struct MainMenu::RoomModernText {
    TTF_Font *title = nullptr, *value = nullptr, *small = nullptr;
    TTF_Font *body = nullptr, *chat = nullptr, *chatBold = nullptr, *tiny = nullptr;
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
    TTF_Font* fonts[] = {r->title, r->value, r->small, r->body, r->chat, r->chatBold, r->tiny};
    delete r;  // the TTFTexts only borrow the fonts, so they go first
    for (TTF_Font* f : fonts) if (f) TTF_CloseFont(f);
}

MainMenu::RoomModernText& MainMenu::RoomModern() {
    if (!roomModern) {
        roomModern = new RoomModernText;
        RoomModernText& r = *roomModern;
        const std::string display = ASSET("/gfx/Baloo2-ExtraBold.ttf");
        const std::string body = ASSET("/gfx/DroidSans.ttf");
        r.title = TTF_OpenFont(display.c_str(), 20);
        r.value = TTF_OpenFont(display.c_str(), 14);
        r.small = TTF_OpenFont(display.c_str(), 12);
        r.body = TTF_OpenFont(body.c_str(), 13);
        r.chat = TTF_OpenFont(body.c_str(), 12);
        r.chatBold = TTF_OpenFont(body.c_str(), 12);
        r.tiny = TTF_OpenFont(body.c_str(), 10);
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
        StrokeRoundRect(rend, f, radius, 1.5f, kIce);
    };
    // A stepped value right-aligned at xr, with < > when focused. Returns the
    // left edge of the value block, where its tap split starts.
    auto stepper = [&](const char* v, int xr, int cy, bool focused) {
        const int right = focused ? xr - 12 : xr;
        const int w = DrawTextLine(rend, T.Take(T.value), v, focused ? kValue : kFrost,
                                   kTextShadow, right, cy, 2);
        if (focused) {
            DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xB9", kIce, kNoShadow, right - w - 10, cy - 1, 1);
            DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xBA", kIce, kNoShadow, xr - 2, cy - 1, 1);
        }
        return right - w - 18;
    };
    // A stepped row's tap targets: the label and the value's left half step
    // back, the value's right half steps forward (menulist::List::End).
    auto splitTap = [&](int index, const SDL_Rect& r, int valueLeft) {
        const int split = (valueLeft + r.x + r.w) / 2;
        AddPanelTapRow(index, {r.x, r.y, split - r.x, r.h}, -1, false, SDLK_LEFT);
        AddPanelTapRow(index, {split, r.y, r.x + r.w - split, r.h}, -1, false, SDLK_RIGHT);
    };

    // ---- Top bar: the room, who you are, the host's Start -----------------
    DrawCard(rend, kTopBar);
    char title[48];
    snprintf(title, sizeof(title), "%.16s's room", game->creator.c_str());
    int x = 28 + DrawTextLine(rend, T.Take(T.title), title, kValue, kTextShadow, 28, 32) + 12;
    x += DrawChip(rend, T.Take(T.tiny), isHost ? "HOST" : "GUEST", x, 31, isHost ? kGold : kLabel) + 6;
    char count[32];
    snprintf(count, sizeof(count), "%d / %d PLAYERS", seated, game->maxPlayers);
    DrawChip(rend, T.Take(T.tiny), count, x, 31, kLabel);

    // HELP for everyone (a joiner plays by these rules too); F1 is the key.
    DrawPill(rend, T.Take(T.small), "HELP", kHelpBtn, false, selectedActionIndex == kRoomHelpTapIndex);
    AddPanelTapRow(kRoomHelpTapIndex, kHelpBtn, -1, false, SDLK_F1);
    if (hasStart) {
        DrawPill(rend, T.Take(T.value), "START GAME", kStartBtn, true, selectedActionIndex == kRoomStart);
        AddPanelTapRow(kRoomStart, kStartBtn);
    } else {
        DrawTextLine(rend, T.Take(T.chat), isHost ? "Waiting for players" : "The host starts the game",
                     kLabel, kNoShadow, kStartBtn.x + kStartBtn.w, 31, 2);
    }

    // ---- Match rules, then the per-player table ----------------------------
    const SDL_Rect rules = bigRoom ? SDL_Rect{12, kCardTop, 288, kCardH} : SDL_Rect{12, kCardTop, 386, kCardH};
    DrawCard(rend, rules);
    DrawTextLine(rend, T.Take(T.tiny), "MATCH RULES", kLabel, kNoShadow, rules.x + 16, kCardTop + 14);

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
    const int rowX = rules.x + 10, rowW = rules.w - 20;
    int ry = kCardTop + 24;
    for (const Rule& rule : ruleRows) {
        if (RoomRowHidden(rule.idx)) continue;
        const SDL_Rect r = {rowX, ry, rowW, 20};
        const bool f = selectedActionIndex == rule.idx;
        if (f) focusRow(r, 7);
        const int cy = r.y + r.h / 2;
        DrawTextLine(rend, T.Take(T.body), rule.label, f ? kValue : kFrost, kNoShadow, r.x + 8, cy);
        if (rule.on >= 0) {
            DrawSwitch(rend, r.x + r.w - 40, cy, rule.on == 1);
            AddPanelTapRow(rule.idx, r);
        } else {
            splitTap(rule.idx, r, stepper(rule.value, r.x + r.w - 10, cy, f));
        }
        ry += 22;
    }

    // The per-player table: ALL plus one column per seat, up to five, as in
    // the classic room. ALL shows a value only when every seat has it.
    {
        const int cols = std::max(1, std::min(5, seated));
        const int labelW = bigRoom ? 92 : 122;
        const int colW = (rowW - labelW) / (cols + 1);
        const int tableTop = kCardTop + 186;
        SDL_SetRenderDrawColor(rend, kLine.r, kLine.g, kLine.b, kLine.a);
        SDL_FRect rule = {(float)rowX, (float)tableTop - 6, (float)rowW, 1};
        SDL_RenderFillRect(rend, &rule);
        DrawTextLine(rend, T.Take(T.tiny), bigRoom ? "FIRST 5 SEATS" : "EACH PLAYER", kLabel, kNoShadow,
                     rowX + 8, tableTop + 8);
        auto colX = [&](int c) { return rowX + labelW + c * colW; };  // c = 0 is ALL
        for (int c = 0; c <= cols; c++) {
            char head[4];
            snprintf(head, sizeof(head), c ? "P%d" : "ALL", c);
            DrawTextLine(rend, T.Take(T.tiny), head, kLabel, kNoShadow, colX(c) + colW / 2, tableTop + 8, 1);
        }
        int myCol = -1;
        for (int i = 0; i < seated && i < 5; i++) if (game->players[i].nick == me) myCol = i + 1;

        const char* labels[4] = {"Max colors", "Row collapse", "Aim guide", "Team"};
        auto value = [&](int row, int p, char* out) {
            if (row == 0) snprintf(out, 8, "%d", playerColorCounts[p]);
            else if (row == 1) snprintf(out, 8, "%s", playerNoCompress[p] ? "off" : "on");
            else if (row == 2) snprintf(out, 8, "%s", playerAimGuide[p] ? "on" : "off");
            else if (netPlayerTeams[p] == kNoTeam) snprintf(out, 8, "-");
            else snprintf(out, 8, "%d", netPlayerTeams[p]);
        };
        for (int row = 0; row < 4; row++) {
            const int idx = kRoomGridFirst + row;
            const SDL_Rect band = {rowX, tableTop + 18 + row * 19, rowW, 18};
            const int cy = band.y + band.h / 2;
            const bool rowFocus = selectedActionIndex == idx;
            if (rowFocus) FillRoundRect(rend, ToFRect(band), 6, kRowFocus);
            DrawTextLine(rend, T.Take(T.chat), labels[row], rowFocus ? kValue : kFrost, kNoShadow, band.x + 8, cy);
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
                const SDL_Rect cell = {colX(c) + 2, band.y + 1, colW - 4, band.h - 2};
                AddPanelTapRow(idx, cell, c);
                if (team != kNoTeam) {
                    const SDL_Color tc = kTeamColors[ClampTeamNumber(team) - 1];
                    FillRoundRect(rend, ToFRect(cell), 5, {tc.r, tc.g, tc.b, 150});
                }
                // Focus on a cell the player may change: the host any cell,
                // a guest only their own team.
                const bool canChange = c == 0 ? isHost : (isHost || (row == 3 && c == myCol));
                const bool cellFocus = rowFocus && currentPlayerCol == c && canChange;
                if (cellFocus) StrokeRoundRect(rend, ToFRect(cell), 5, 1.5f, kIce);
                const bool muted = !strcmp(text, "off") || !strcmp(text, "-");
                DrawTextLine(rend, T.Take(T.small), text, muted ? kLabel : kValue, kNoShadow,
                             cell.x + cell.w / 2, cy, 1);
            }
        }
    }

    // ---- Players ----------------------------------------------------------
    const SDL_Rect players = bigRoom ? SDL_Rect{306, kCardTop, 322, kCardH} : SDL_Rect{406, kCardTop, 222, kCardH};
    DrawCard(rend, players);
    DrawTextLine(rend, T.Take(T.tiny), "PLAYERS", kLabel, kNoShadow, players.x + 16, kCardTop + 14);
    // The page every seat's team is set on; A is its key from anywhere here.
    DrawPill(rend, T.Take(T.small), "Set Teams", kTeamsBtn, false, selectedActionIndex == kRoomSetTeamsTapIndex);
    AddPanelTapRow(kRoomSetTeamsTapIndex, kTeamsBtn, -1, false, SDLK_A);

    if (bigRoom) {
        // Two columns of slim rows; a tap on one opens Set Teams on that seat
        // (the host any seat, a guest their own), as in the classic room.
        const int cap = game->maxPlayers;
        const int perCol = (cap + 1) / 2;
        const int pitch = std::min(22, 216 / std::max(1, perCol));
        for (int pi = 0; pi < cap; pi++) {
            const SDL_Rect r = {players.x + 8 + (pi / perCol) * 156, kCardTop + 36 + (pi % perCol) * pitch,
                                150, pitch - 3};
            const int cy = r.y + r.h / 2;
            FillRoundRect(rend, ToFRect(r), 6, kRowIdle);
            char num[4];
            snprintf(num, sizeof(num), "%d", pi + 1);
            DrawTextLine(rend, T.Take(T.tiny), num, kLabel, kNoShadow, r.x + 16, cy, 2);
            if (pi >= seated) continue;
            const NetworkPlayer& pl = game->players[pi];
            const bool self = pl.nick == me;
            if (isHost || self) AddPanelTapRow(kRoomRosterTapBase + pi, r);
            auto it = netTeamOverrides.find(pl.nick);
            const int team = it == netTeamOverrides.end() ? kNoTeam : ClampTeamOrNone(it->second);
            if (team != kNoTeam)
                FillRoundRect(rend, {(float)r.x + 21, (float)cy - 4.5f, 9, 9}, 4.5f, kTeamColors[team - 1]);
            char nick[16];
            snprintf(nick, sizeof(nick), "%.10s", pl.nick.c_str());
            const int w = DrawTextLine(rend, T.Take(T.small), nick, self ? kIce : kValue, kNoShadow, r.x + 36, cy);
            if (pl.nick == game->creator) DrawChip(rend, T.Take(T.tiny), "H", r.x + 42 + w, cy, kGold);
        }
    } else {
        const int seats = std::max(1, std::min(5, game->maxPlayers));
        for (int pi = 0; pi < seats; pi++) {
            const SDL_Rect r = {players.x + 8, kCardTop + 34 + pi * 30, players.w - 16, 26};
            const int cy = r.y + r.h / 2;
            FillRoundRect(rend, ToFRect(r), 8, kRowIdle);
            char slot[4];
            snprintf(slot, sizeof(slot), "P%d", pi + 1);
            DrawTextLine(rend, T.Take(T.tiny), slot, kLabel, kNoShadow, r.x + r.w - 8, cy, 2);
            if (pi >= seated) {
                StrokeRoundRect(rend, {(float)r.x + 7, (float)cy - 7, 14, 14}, 7, 1.5f, {58, 75, 108, 255});
                DrawTextLine(rend, T.Take(T.chat), "Waiting for player...", kLabel, kNoShadow, r.x + 28, cy);
                continue;
            }
            const NetworkPlayer& pl = game->players[pi];
            const bool self = pl.nick == me;
            const int team = ClampTeamOrNone(netPlayerTeams[pi]);
            if (team != kNoTeam) {
                FillRoundRect(rend, {(float)r.x + 7, (float)cy - 7, 14, 14}, 7, kTeamColors[team - 1]);
            } else if (T.ball[pi]) {
                SDL_FRect b = {(float)r.x + 6, (float)cy - 8, 16, 16};
                SDL_RenderTexture(rend, T.ball[pi], nullptr, &b);
            }
            char nick[16];
            snprintf(nick, sizeof(nick), "%.10s", pl.nick.c_str());
            int cx = r.x + 28 + DrawTextLine(rend, T.Take(T.value), nick, kValue, kNoShadow, r.x + 28, cy) + 6;
            if (pl.nick == game->creator) cx += DrawChip(rend, T.Take(T.tiny), "HOST", cx, cy, kGold) + 4;
            if (self) DrawChip(rend, T.Take(T.tiny), "YOU", cx, cy, kIce, true);
        }
    }

    // Bots: the host's to add (a bot is an ordinary player to everyone else).
    if (isHost) {
        const int maxBots = MaxRoomBots(seated, game->maxPlayers, netRoomBotCount);
        char bots[32];
        if (maxBots <= 0 && netRoomBotCount == 0) snprintf(bots, sizeof(bots), "0 (full)");
        else snprintf(bots, sizeof(bots), "%d of %d", netRoomBotCount, maxBots);
        SDL_Rect botsRow, skillRow;
        if (bigRoom) {
            botsRow = {players.x + 8, kCardTop + kCardH - 32, 150, 22};
            skillRow = {players.x + 164, kCardTop + kCardH - 32, 150, 22};
        } else {
            botsRow = {players.x + 8, kCardTop + kCardH - 58, players.w - 16, 22};
            skillRow = {players.x + 8, kCardTop + kCardH - 32, players.w - 16, 22};
        }
        SDL_SetRenderDrawColor(rend, kLine.r, kLine.g, kLine.b, kLine.a);
        SDL_FRect divider = {(float)players.x + 14, (float)botsRow.y - 7, (float)players.w - 28, 1};
        SDL_RenderFillRect(rend, &divider);
        const struct { int idx; SDL_Rect r; const char* label; const char* value; } side[2] = {
            {kRoomBots, botsRow, "Bots", bots},
            {kRoomBotSkill, skillRow, "Skill", LocalMPBotSkillName(netRoomBotSkill)},
        };
        for (const auto& s : side) {
            const bool f = selectedActionIndex == s.idx;
            if (f) focusRow(s.r, 7);
            const int cy = s.r.y + s.r.h / 2;
            DrawTextLine(rend, T.Take(T.body), s.label, f ? kValue : kFrost, kNoShadow, s.r.x + 8, cy);
            splitTap(s.idx, s.r, stepper(s.value, s.r.x + s.r.w - 10, cy, f));
        }
    }
}

// The chat card: drawn by NetPanelChatDockRender in a room in the Modern
// style, which keeps its own message handling and calls these for drawing.
void MainMenu::NetChatDockModern(bool expanded) {
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    RoomModernText& T = RoomModern();
    T.chatUsed = 0;
    const SDL_Rect card = expanded ? SDL_Rect{12, 60, 616, 412} : SDL_Rect{12, 368, 616, 104};
    DrawCard(rend, card);
    DrawTextLine(rend, T.TakeChat(T.tiny), expanded ? "CHAT  \xC2\xB7  ENTER sends, ESC cancels" : "CHAT",
                 kLabel, kNoShadow, 28, card.y + 13);

    // Index 0, Chat, is the input line itself.
    const SDL_Rect input = {24, 436, 592, 28};
    if (!expanded) AddPanelTapRow(kRoomChat, input);
    const bool focused = expanded || selectedActionIndex == kRoomChat;
    FillRoundRect(rend, ToFRect(input), 14, {5, 10, 22, 180});
    StrokeRoundRect(rend, ToFRect(input), 14, focused ? 2.f : 1.f, focused ? kIce : SDL_Color{127, 214, 255, 72});
    const int cy = input.y + input.h / 2;
    const size_t len = strlen(networkChatInput);
    if (len == 0) {
        DrawTextLine(rend, T.TakeChat(T.chat), focused ? "Type a message, ENTER to send" : "Say something...",
                     kLabel, kNoShadow, input.x + 14, cy);
    } else {
        // The end of a long message, so the caret stays in view.
        const char* shown = networkChatInput + (len > 70 ? len - 70 : 0);
        char line[96];
        snprintf(line, sizeof(line), "%s%s%s", len > 70 ? "..." : "", shown, focused ? "_" : "");
        DrawTextLine(rend, T.TakeChat(T.chat), line, kValue, kNoShadow, input.x + 14, cy);
    }
    if (!expanded)
        DrawTextLine(rend, T.TakeChat(T.tiny),
                     NetworkClient::Instance()->GetCurrentGame()
                         ? "ESC leave  \xC2\xB7  F1 help  \xC2\xB7  A teams"
                         : "ENTER create / join  \xC2\xB7  ESC leave server", kLabel,
                     kNoShadow, input.x + input.w - 14, cy, 2);
}

void MainMenu::NetChatLineModern(const ChatMessage& cm, int y) {
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    RoomModernText& T = RoomModern();
    const int cy = y + 7;
    if (cm.nick == "Server" || cm.message.find("***") == 0) {
        // Grey already marks a server line, so its "*** " goes.
        const char* msg = cm.message.c_str();
        if (cm.message.rfind("*** ", 0) == 0) msg += 4;
        char text[96];
        snprintf(text, sizeof(text), "%.80s", msg);
        DrawTextLine(rend, T.TakeChat(T.chat), text, kLabel, kNoShadow, 28, cy);
        return;
    }
    char nick[24], text[80];
    snprintf(nick, sizeof(nick), "%.16s:", cm.nick.c_str());
    snprintf(text, sizeof(text), "%.64s", cm.message.c_str());
    const int w = DrawTextLine(rend, T.TakeChat(T.chatBold), nick, kIce, kNoShadow, 28, cy);
    DrawTextLine(rend, T.TakeChat(T.chat), text, kFrost, kNoShadow, 28 + w + 6, cy);
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
    DrawCard(rend, {12, 10, 616, 44});
    const int w = DrawTextLine(rend, T.TakeTeam(T.title), "SET TEAMS", kValue, kTextShadow, 28, 32);
    char sub[64];
    snprintf(sub, sizeof(sub), "%d players  \xC2\xB7  %.16s's room", playerCount, room->creator.c_str());
    DrawTextLine(rend, T.TakeTeam(T.chat), sub, kLabel, kNoShadow, 28 + w + 14, 33);
    teamsDoneRect = {538, 18, 82, 28};
    DrawPill(rend, T.TakeTeam(T.value), "DONE", teamsDoneRect, true, false);

    const SDL_Rect body = {12, 62, 616, 384};
    DrawCard(rend, body);

    // Auto: the host's one-tap splits, also a keyboard row (teamsAutoFocus).
    if (isHost) {
        DrawTextLine(rend, T.TakeTeam(T.tiny), "AUTO", kLabel, kNoShadow, 28, 76);
        const char* labels[kMaxTeams] = {"NONE", "2", "3", "4", "5"};
        int x = 70;
        for (int b = 0; b < kMaxTeams; b++) {
            const SDL_Rect r = {x, 65, b == 0 ? 50 : 30, 22};
            const bool f = teamsAutoFocus == b;
            FillRoundRect(rend, ToFRect(r), 8, f ? kIce : SDL_Color{127, 214, 255, 26});
            if (!f) StrokeRoundRect(rend, ToFRect(r), 8, 1, SDL_Color{127, 214, 255, 72});
            DrawTextLine(rend, T.TakeTeam(T.small), labels[b], f ? kInk : kFrost, kNoShadow,
                         r.x + r.w / 2, r.y + r.h / 2, 1);
            teamAutoBalanceTaps.push_back({r, b == 0 ? kNoTeam : b + 1});
            x += r.w + 6;
        }
        DrawTextLine(rend, T.TakeTeam(T.chat), "splits everyone evenly", kLabel, kNoShadow, x + 6, 76);
    }

    // Column heads: the teams in their own colours.
    const int teamX0 = 390, teamPitch = 38;
    DrawTextLine(rend, T.TakeTeam(T.tiny), "PLAYER", kLabel, kNoShadow, 34, 108);
    for (int t = kNoTeam; t <= kMaxTeams; t++) {
        DrawTextLine(rend, T.TakeTeam(T.tiny), t == kNoTeam ? "NONE" : std::to_string(t).c_str(),
                     t == kNoTeam ? kLabel : kTeamColors[t - 1], kNoShadow,
                     teamX0 + t * teamPitch + 16, 108, 1);
    }
    SDL_SetRenderDrawColor(rend, kLine.r, kLine.g, kLine.b, kLine.a);
    SDL_FRect rule = {22, 118, 596, 1};
    SDL_RenderFillRect(rend, &rule);

    // Nine rows at a time; the window follows the cursor, as on the
    // classic page.
    constexpr int kVisible = 9, kRowPitch = 34, kRowsTop = 126;
    const int shown = std::min(playerCount, kVisible);
    const int first = std::clamp(teamsCursorPlayer - shown + 1, 0, std::max(0, playerCount - shown));
    if (playerCount > shown) {
        char range[48];
        snprintf(range, sizeof(range), "%d-%d of %d", first + 1, first + shown, playerCount);
        DrawTextLine(rend, T.TakeTeam(T.tiny), range, kLabel, kNoShadow, 612, 76, 2);
        const float trackH = kVisible * kRowPitch - 4;
        FillRoundRect(rend, {622, (float)kRowsTop, 4, trackH}, 2, kRowIdle);
        FillRoundRect(rend, {622, kRowsTop + trackH * first / playerCount, 4,
                             trackH * shown / playerCount}, 2, kIce);
    }

    for (int k = 0; k < shown; k++) {
        const int slot = first + k;
        const NetworkPlayer& player = room->players[slot];
        const SDL_Rect r = {22, kRowsTop + k * kRowPitch, 596, 30};
        const int cy = r.y + r.h / 2;
        const bool self = slot == mySlot;
        // The host moves anyone, everyone else only themselves.
        const bool editable = isHost || self;
        const bool focused = slot == teamsCursorPlayer && teamsAutoFocus < 0;
        if (focused) {
            FillRoundRect(rend, ToFRect(r), 8, kRowFocus);
            StrokeRoundRect(rend, ToFRect(r), 8, 1.5f, kIce);
        } else if (k % 2) {
            FillRoundRect(rend, ToFRect(r), 8, {127, 214, 255, 8});
        }
        char num[4];
        snprintf(num, sizeof(num), "%d", slot + 1);
        DrawTextLine(rend, T.TakeTeam(T.tiny), num, kLabel, kNoShadow, r.x + 22, cy, 2);
        const int current = TeamOfSlot(slot);
        if (current != kNoTeam)
            FillRoundRect(rend, {(float)r.x + 32, (float)cy - 6, 12, 12}, 6, kTeamColors[current - 1]);
        else
            StrokeRoundRect(rend, {(float)r.x + 32, (float)cy - 6, 12, 12}, 6, 1.5f, {58, 75, 108, 255});
        char nick[16];
        snprintf(nick, sizeof(nick), "%.12s", player.nick.c_str());
        int cx = r.x + 54 + DrawTextLine(rend, T.TakeTeam(T.value), nick, self ? kIce : kValue,
                                          kNoShadow, r.x + 54, cy) + 8;
        if (player.nick == room->creator) cx += DrawChip(rend, T.TakeTeam(T.tiny), "HOST", cx, cy, kGold) + 4;
        if (self) DrawChip(rend, T.TakeTeam(T.tiny), "YOU", cx, cy, kIce, true);
        if (editable) teamPlayerNameTaps.push_back({{r.x, r.y, teamX0 - 8 - r.x, r.h}, slot});

        for (int t = kNoTeam; t <= kMaxTeams; t++) {
            const SDL_Rect b = {teamX0 + t * teamPitch, r.y + 4, 32, r.h - 8};
            const SDL_Color col = t == kNoTeam ? kLabel : kTeamColors[t - 1];
            const bool on = t == current;
            const SDL_FRect fb = ToFRect(b);
            FillRoundRect(rend, fb, 7, on ? col : SDL_Color{127, 214, 255, 13});
            if (on) StrokeRoundRect(rend, fb, 7, 2, kValue);
            else StrokeRoundRect(rend, fb, 7, 1, {col.r, col.g, col.b, (Uint8)(editable ? 115 : 45)});
            DrawTextLine(rend, T.TakeTeam(T.small), t == kNoTeam ? "-" : std::to_string(t).c_str(),
                         on ? kInk : (editable ? kFrost : kLabel), kNoShadow, b.x + b.w / 2, cy, 1);
            if (editable) teamSwatchTaps.push_back({b, slot, t});
        }
    }

    menulist::DrawFooterHint(rend, panelText,
        isHost ? "UP/DOWN player or Auto    LEFT/RIGHT or tap a team    ENTER applies Auto    ESC closes"
               : "LEFT/RIGHT or tap a team    ESC / Done closes");
}

// The online lobby in the Modern style: a top bar, a Game rooms card (Create
// game room with its size, the tournament rows, then every room), an Online
// card (the free players, Weekly rankings and Join Discord pinned at its
// foot), and the chat card. Drawing only, like the room: every row keeps the
// index the classic lobby gives it (LobbyTournamentIndex, LobbyRoomListStart,
// LobbyWeeklyIndex, LobbyDiscordIndex), so Up/Down, ENTER and the tap rows
// cannot tell the two apart. Uses the room's fonts.
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
        StrokeRoundRect(rend, f, radius, 1.5f, kIce);
    };

    // ---- Top bar --------------------------------------------------------------
    DrawCard(rend, kTopBar);
    int x = 28 + DrawTextLine(rend, T.Take(T.title), "Online lobby", kValue, kTextShadow, 28, 32) + 12;
    char nick[24];
    snprintf(nick, sizeof(nick), "%.16s", me.c_str());
    x += DrawChip(rend, T.Take(T.tiny), nick, x, 31, kIce, true) + 6;
    char count[32];
    snprintf(count, sizeof(count), "%d IN LOBBY", (int)online.size());
    DrawChip(rend, T.Take(T.tiny), count, x, 31, kLabel);
    char host[80];
    snprintf(host, sizeof(host), "%.40s", netClient->GetHost().c_str());
    DrawTextLine(rend, T.Take(T.chat), host, kLabel, kNoShadow, 612, 31, 2);

    // ---- Game rooms -----------------------------------------------------------
    const SDL_Rect rooms = {12, kCardTop, 386, kCardH};
    DrawCard(rend, rooms);
    DrawTextLine(rend, T.Take(T.tiny), "GAME ROOMS", kLabel, kNoShadow, rooms.x + 16, kCardTop + 14);
    const int rowX = rooms.x + 10, rowW = rooms.w - 20;
    int y = kCardTop + 26;
    {
        // Create game room: the label creates (ENTER), the size steps with
        // LEFT/RIGHT, so its value block splits into LEFT/RIGHT halves
        // (menulist::List::End's labelActivateKey row).
        const SDL_Rect r = {rowX, y, rowW, 34};
        const bool f = selectedActionIndex == 1;
        if (f) focusRow(r, 9);
        else FillRoundRect(rend, ToFRect(r), 9, kRowIdle);
        const int cy = r.y + r.h / 2;
        if (T.ball[0]) {
            SDL_FRect b = {(float)r.x + 8, (float)cy - 9, 18, 18};
            SDL_RenderTexture(rend, T.ball[0], nullptr, &b);
        }
        DrawTextLine(rend, T.Take(T.value), "Create game room", f ? kValue : kFrost, kTextShadow, r.x + 34, cy);
        char size[24];
        snprintf(size, sizeof(size), "%d players", kRoomSizes[netRoomSizeChoice]);
        const int xr = r.x + r.w - 12;
        const int right = f ? xr - 12 : xr;
        const int w = DrawTextLine(rend, T.Take(T.small), size, f ? kValue : kLabel, kNoShadow, right, cy, 2);
        if (f) {
            DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xB9", kIce, kNoShadow, right - w - 10, cy - 1, 1);
            DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xBA", kIce, kNoShadow, xr - 2, cy - 1, 1);
        }
        const int valueLeft = right - w - 18;
        const int split = (valueLeft + r.x + r.w) / 2;
        AddPanelTapRow(1, {r.x, r.y, valueLeft - r.x, r.h}, -1, false, SDLK_RETURN);
        AddPanelTapRow(1, {valueLeft, r.y, split - valueLeft, r.h}, -1, false, SDLK_LEFT);
        AddPanelTapRow(1, {split, r.y, r.x + r.w - split, r.h}, -1, false, SDLK_RIGHT);
        y += 38;
    }
    // A plain row: label on the left, an optional value on the right.
    auto plainRow = [&](int index, int h, const char* label, const char* value) {
        const SDL_Rect r = {rowX, y, rowW, h};
        const bool f = selectedActionIndex == index;
        if (f) focusRow(r, 8);
        else FillRoundRect(rend, ToFRect(r), 8, kRowIdle);
        const int cy = r.y + r.h / 2;
        DrawTextLine(rend, T.Take(T.body), label, f ? kValue : kFrost, kNoShadow, r.x + 12, cy);
        if (value && value[0]) DrawTextLine(rend, T.Take(T.chat), value, kLabel, kNoShadow, r.x + r.w - 12, cy, 2);
        AddPanelTapRow(index, r);
        y += h + 4;
    };
    const int tourIdx = LobbyTournamentIndex();
    if (tourIdx >= 0) {
        plainRow(tourIdx, 26, "Create tournament", "");
        const auto joinable = LobbyJoinableTournaments();
        for (size_t i = 0; i < joinable.size(); i++) {
            const auto& t = joinable[i];
            char label[48], value[48];
            snprintf(label, sizeof(label), "%.16s's tournament", t.owner.c_str());
            snprintf(value, sizeof(value), "%d \xC2\xB7 %s", t.count, t.state.c_str());
            plainRow(tourIdx + 1 + (int)i, 26, label, value);
        }
    }

    // The rooms, two lines each, scrolled so the selected one stays in view
    // (one slot from the top, clamped at the ends, as menulist::List does).
    const int roomStart = LobbyRoomListStart();
    const int areaTop = y + 2, areaBottom = rooms.y + rooms.h - 10;
    const int pitch = 44;
    const int visible = std::max(1, (areaBottom - areaTop + 4) / pitch);
    const int total = (int)games.size();
    const int sel = selectedActionIndex - roomStart;
    const int maxScroll = std::max(0, total - visible);
    const int scrollTop = (sel >= 0 && sel < total) ? std::clamp(sel - 1, 0, maxScroll) : 0;
    if (total == 0) {
        DrawTextLine(rend, T.Take(T.chat), "No rooms yet. Create one and others can join.", kLabel, kNoShadow,
                     rowX + 12, areaTop + 16);
    }
    for (int i = scrollTop; i < total && i < scrollTop + visible; i++) {
        const GameRoom& g = games[i];
        const SDL_Rect r = {rowX, areaTop + (i - scrollTop) * pitch, rowW - (maxScroll ? 8 : 0), pitch - 4};
        const int index = roomStart + i;
        const bool f = selectedActionIndex == index;
        if (f) focusRow(r, 9);
        else FillRoundRect(rend, ToFRect(r), 9, kRowIdle);
        char title[48];
        snprintf(title, sizeof(title), "%.16s's room", g.creator.c_str());
        DrawTextLine(rend, T.Take(T.value), title, f ? kValue : kFrost, kTextShadow, r.x + 12, r.y + 13);
        std::string names;
        for (const NetworkPlayer& p : g.players) {
            if (!names.empty()) names += ", ";
            names += p.nick;
        }
        if (names.size() > 48) names = names.substr(0, 45) + "...";
        DrawTextLine(rend, T.Take(T.chat), names.c_str(), kLabel, kNoShadow, r.x + 12, r.y + 29);
        char seats[16];
        snprintf(seats, sizeof(seats), "%d / %d", (int)g.players.size(), g.maxPlayers);
        int cx = r.x + r.w - 10;
        const bool full = (int)g.players.size() >= g.maxPlayers;
        // A chip is drawn from its left edge, so measure it first.
        int sw = 0;
        if (T.tiny) TTF_GetStringSize(T.tiny, seats, 0, &sw, nullptr);
        cx -= sw + 12;
        DrawChip(rend, T.Take(T.tiny), seats, cx, r.y + 13, full ? kLose : kIce);
        if (g.started) {
            int pw = 0;
            if (T.tiny) TTF_GetStringSize(T.tiny, "PLAYING", 0, &pw, nullptr);
            DrawChip(rend, T.Take(T.tiny), "PLAYING", cx - pw - 18, r.y + 13, kGold);
        }
        AddPanelTapRow(index, r);
    }
    if (maxScroll > 0) {
        const float trackH = (float)(areaBottom - areaTop);
        const float thumbH = std::max(16.f, trackH * visible / total);
        const float thumbY = areaTop + (trackH - thumbH) * scrollTop / maxScroll;
        FillRoundRect(rend, {(float)rooms.x + rooms.w - 12, (float)areaTop, 4, trackH}, 2, kRowIdle);
        FillRoundRect(rend, {(float)rooms.x + rooms.w - 12, thumbY, 4, thumbH}, 2, kIce);
    }

    // ---- Online -----------------------------------------------------------------
    const SDL_Rect side = {406, kCardTop, 222, kCardH};
    DrawCard(rend, side);
    DrawTextLine(rend, T.Take(T.tiny), "ONLINE", kLabel, kNoShadow, side.x + 16, kCardTop + 14);
    // This week's round-wins rank after each name; a no-op on an old server.
    netClient->MaybeRefreshWeekly();
    const auto& ranks = netClient->weekly.lobbyWinsRank;

    const int discordIdx = LobbyDiscordIndex(games.size());
    const int weeklyIdx = LobbyWeeklyIndex(games.size());
    const int pinned = 1 + (discordIdx >= 0 ? 1 : 0);
    const int pinTop = side.y + side.h - 10 - pinned * 30 + 4;
    int py = kCardTop + 30;
    bool anyOther = false;
    for (const NetworkPlayer& p : online) {
        const bool self = p.nick == me;
        if (!self) anyOther = true;
        if (py + 22 > pinTop - 6) break;
        const int cy = py + 10;
        FillRoundRect(rend, {(float)side.x + 16, (float)cy - 3.5f, 7, 7}, 3.5f, {104, 220, 151, 255});
        char name[16];
        snprintf(name, sizeof(name), "%.10s", p.nick.c_str());
        int cx = side.x + 30 + DrawTextLine(rend, T.Take(T.small), name, self ? kIce : kValue, kNoShadow,
                                            side.x + 30, cy) + 6;
        if (self) cx += DrawChip(rend, T.Take(T.tiny), "YOU", cx, cy, kIce, true) + 4;
        auto rank = ranks.find(p.nick);
        if (rank != ranks.end() && rank->second > 0) {
            const std::string text = "#" + std::to_string(rank->second);
            DrawTextLine(rend, T.Take(T.tiny), text.c_str(), kGold, kNoShadow, cx, cy);
        }
        PlayerBadge badge;
        if (GetPlatformBadge(p.platform, badge)) {
            int bw = 0;
            if (T.tiny) TTF_GetStringSize(T.tiny, badge.label, 0, &bw, nullptr);
            const SDL_FRect chip = {(float)side.x + side.w - 16 - bw - 10, (float)cy - 7, (float)bw + 10, 14};
            FillRoundRect(rend, chip, 4, badge.fill);
            DrawTextLine(rend, T.Take(T.tiny), badge.label, badge.text, kNoShadow, (int)chip.x + 5, cy);
        }
        py += 22;
    }
    if (!anyOther && py + 22 <= pinTop - 6)
        DrawTextLine(rend, T.Take(T.chat), "No one else in the lobby", kLabel, kNoShadow, side.x + 16, py + 10);

    SDL_SetRenderDrawColor(rend, kLine.r, kLine.g, kLine.b, kLine.a);
    SDL_FRect divider = {(float)side.x + 14, (float)pinTop - 5, (float)side.w - 28, 1};
    SDL_RenderFillRect(rend, &divider);
    const struct { int idx; const char* label; } pins[2] = {
        {weeklyIdx, "Weekly rankings"},
        {discordIdx, "Join Discord server"},
    };
    for (int i = 0; i < pinned; i++) {
        const SDL_Rect r = {side.x + 8, pinTop + i * 30, side.w - 16, 26};
        const bool f = selectedActionIndex == pins[i].idx;
        if (f) focusRow(r, 8);
        else FillRoundRect(rend, ToFRect(r), 8, kRowIdle);
        const int cy = r.y + r.h / 2;
        DrawTextLine(rend, T.Take(T.body), pins[i].label, f ? kValue : kFrost, kNoShadow, r.x + 10, cy);
        DrawTextLine(rend, T.Take(T.body), "\xE2\x80\xBA", kIce, kNoShadow, r.x + r.w - 12, cy - 1, 1);
        AddPanelTapRow(pins[i].idx, r);
    }
}
