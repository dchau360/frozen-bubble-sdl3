#include "mainmenu.h"
#include "mainmenu_internal.h"
#include "bubblegame.h"
#include "localmultiplayer_settings.h"
#include "modernui.h"
#include "netbot.h"
#include "networkclient.h"
#include "platform.h"
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
    std::deque<TTFText> pool, chatPool;
    size_t used = 0, chatUsed = 0;
    SDL_Texture* ball[5] = {};

    TTFText& Take(TTF_Font* f) {
        if (used == pool.size()) pool.emplace_back();
        TTFText& t = pool[used++];
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
        DrawTextLine(rend, T.TakeChat(T.tiny), "ESC leave  \xC2\xB7  F1 help  \xC2\xB7  A teams", kLabel,
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
