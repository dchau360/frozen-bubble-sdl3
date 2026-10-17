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

// The Report / Block page for the online lobby and room.
//
// Reporting and blocking used to exist only as typed chat commands (/report,
// /block, /unblock), which a touch player never finds and a pad player cannot
// type. User-generated chat is expected to come with a way to flag and hide
// another player that anyone can reach, so this page puts both one press away
// from the chat: every other player this client knows about (whoever spoke
// recently, the room's roster, the lobby's player list, and anyone already
// blocked, so they can be unblocked) gets a row with a Report and a Block
// button. The typed commands still work and share the same code paths and
// the same block list.
//
// Opened by B (lobby or room, not while typing), the REPORT / BLOCK button in
// the chat dock's header, or a tap on someone's line in the chat, which
// starts the cursor on that player.

#include "mainmenu_internal.h"
#include "menulist.h"
#include "audiomixer.h"
#include "networkclient.h"
#include "gamesettings.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {
constexpr int kPlayersListMax = 60;
constexpr const char* kReportReason = "reported from the player menu";
}

std::vector<std::string> MainMenu::PlayersPanelCandidates() const {
    NetworkClient* netClient = NetworkClient::Instance();
    const GameSettings* gs = GameSettings::Instance();
    const std::string me = netClient->GetPlayerNick();
    std::vector<std::string> out;
    auto add = [&](const std::string& raw) {
        const std::string nick = GameSettings::NormalizeNick(raw);
        if (nick.empty() || nick == "Server") return;
        if (GameSettings::NicksEqual(nick, me)) return;
        for (const std::string& have : out)
            if (GameSettings::NicksEqual(have, nick)) return;
        if ((int)out.size() < kPlayersListMax) out.push_back(nick);
    };

    // Newest speaker first: the player somebody wants to report is almost
    // always one who just said something.
    const std::vector<ChatMessage> chat = netClient->GetChatMessages();
    for (size_t i = chat.size(); i-- > 0;) {
        if (chat[i].nick == "Server" || chat[i].message.compare(0, 3, "***") == 0) continue;
        if (chat[i].message.compare(0, 6, "!team:") == 0 ||
            chat[i].message.compare(0, 9, "!teamset:") == 0) continue;
        add(chat[i].nick);
    }
    if (GameRoom* room = netClient->GetCurrentGame())
        for (const NetworkPlayer& p : room->players) add(p.nick);
    for (const NetworkPlayer& p : netClient->GetOpenPlayers()) add(p.nick);
    for (const std::string& blocked : gs->blockedPlayers) add(blocked);
    return out;
}

void MainMenu::OpenPlayersPanel(const std::string& nick) {
    playersList = PlayersPanelCandidates();
    playersCursor = 0;
    playersButton = 0;
    playersNotice.clear();
    if (!nick.empty()) {
        for (size_t i = 0; i < playersList.size(); i++) {
            if (GameSettings::NicksEqual(playersList[i], nick)) {
                playersCursor = (int)i;
                break;
            }
        }
    }
    showingPlayersPanel = true;
    PlayMenuSFX("menu_selected");
}

void MainMenu::PlayersPanelAct(const std::string& nick, int button) {
    NetworkClient* netClient = NetworkClient::Instance();
    GameSettings* gs = GameSettings::Instance();
    char msg[160];
    if (button == 0) {
        bool already = false;
        for (const std::string& r : playersReported)
            if (GameSettings::NicksEqual(r, nick)) already = true;
        if (already) {
            snprintf(msg, sizeof(msg), "You already reported %s", nick.c_str());
        } else if (netClient->SendReport(nick.c_str(), kReportReason)) {
            playersReported.push_back(nick);
            snprintf(msg, sizeof(msg),
                     "Reported %s to the server operator. Block them to hide their chat.",
                     nick.c_str());
        } else {
            snprintf(msg, sizeof(msg), "Could not send the report");
        }
    } else if (gs->IsPlayerBlocked(nick)) {
        gs->ToggleBlockedPlayer(nick);   // known present: removes
        gs->SaveKeys();                  // see /block: only SaveKeys() writes the list
        snprintf(msg, sizeof(msg), "Unblocked %s", nick.c_str());
    } else if (gs->ToggleBlockedPlayer(nick)) {
        gs->SaveKeys();
        snprintf(msg, sizeof(msg), "Blocked %s -- their chat is now hidden", nick.c_str());
    } else {
        snprintf(msg, sizeof(msg), "Block list is full (%d) -- unblock someone first",
                 GameSettings::kMaxBlockedPlayers);
    }
    playersNotice = msg;
    netClient->AddStatusMessage(msg);
    PlayMenuSFX("menu_selected");
}

void MainMenu::PlayersPanelRender() {
    if (!showingPlayersPanel) {
        playersDoneRect = SDL_Rect{};
        playersTaps.clear();
        return;
    }

    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    const GameSettings* gs = GameSettings::Instance();
    menulist::DrawWorldMapBackdrop(rend, netGameBackground);

    playersTaps.clear();
    playersDoneRect = SDL_Rect{};
    menulist::DrawHeaderBar(rend, panelText, menulist::kHeaderBar, "REPORT / BLOCK",
                            "Done", true, 0,
                            [&](int, const SDL_Rect& r, int, bool, SDL_Keycode) {
                                playersDoneRect = r;
                            },
                            menulist::kMapFillAlpha);

    const SDL_Rect body = {10, 44, 620, 400};
    SDL_Color fill = menulist::kListFill;
    fill.a = menulist::kMapFillAlpha;
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, fill.r, fill.g, fill.b, fill.a);
    { SDL_FRect fr = ToFRect(body); SDL_RenderFillRect(rend, &fr); }
    SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g,
                           menulist::kEdge.b, menulist::kEdge.a);
    { SDL_FRect fr = ToFRect(body); SDL_RenderRect(rend, &fr); }

    size_t cellIdx = 0;
    auto drawText = [&](const char* txt, int x, int y, SDL_Color color, int size, int style) {
        TTFText& text = TeamPanelCell(cellIdx++, size, style);
        text.UpdateColor(color, menulist::kTextShadow);
        text.UpdateText(rend, txt, 0);
        text.UpdatePosition({x, y});
        SDL_FRect fr = ToFRect(*text.Coords());
        SDL_RenderTexture(rend, text.Texture(), nullptr, &fr);
    };

    drawText("Report sends the player's name to the server operator. Block hides their chat on this device.",
             body.x + 16, body.y + 8, menulist::kMuted, 12, TTF_STYLE_NORMAL);
    SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g, menulist::kEdge.b, 110);
    SDL_RenderLine(rend, (float)(body.x + 12), (float)(body.y + 28),
                   (float)(body.x + body.w - 12), (float)(body.y + 28));

    const int count = (int)playersList.size();
    playersCursor = std::clamp(playersCursor, 0, std::max(0, count - 1));
    const int rowH = 34;
    const int firstRowY = body.y + 36;
    const int maxRows = (body.h - 80) / rowH;
    const int shown = std::min(count, maxRows);
    const int firstShown = std::clamp(playersCursor - shown + 1, 0, std::max(0, count - shown));
    const int btnW = 100, btnH = 26;
    const int blockX = body.x + body.w - 16 - btnW;
    const int reportX = blockX - 8 - btnW;

    if (count == 0) {
        drawText("No other players to report or block right now.",
                 body.x + 16, firstRowY + 6, menulist::kText, 15, TTF_STYLE_NORMAL);
    }

    for (int i = 0; i < shown; i++) {
        const int idx = firstShown + i;
        const std::string& nick = playersList[idx];
        const int rowY = firstRowY + i * rowH;
        const bool blocked = gs->IsPlayerBlocked(nick);
        bool reported = false;
        for (const std::string& r : playersReported)
            if (GameSettings::NicksEqual(r, nick)) reported = true;

        if (idx == playersCursor) {
            SDL_Rect sel = {body.x + 10, rowY - 3, body.w - 20, rowH - 4};
            SDL_SetRenderDrawColor(rend, menulist::kSelFill.r, menulist::kSelFill.g,
                                   menulist::kSelFill.b, menulist::kSelFill.a);
            { SDL_FRect fr = ToFRect(sel); SDL_RenderFillRect(rend, &fr); }
            SDL_SetRenderDrawColor(rend, menulist::kSelEdge.r, menulist::kSelEdge.g,
                                   menulist::kSelEdge.b, menulist::kSelEdge.a);
            { SDL_FRect fr = ToFRect(sel); SDL_RenderRect(rend, &fr); }
        }

        char label[96];
        snprintf(label, sizeof(label), "%.16s%s", nick.c_str(), blocked ? "   (blocked)" : "");
        drawText(label, body.x + 16, rowY + 4,
                 blocked ? menulist::kMuted : menulist::kText, 15, TTF_STYLE_NORMAL);
        // Tapping the name just moves the cursor there.
        playersTaps.push_back({{body.x + 12, rowY - 3, reportX - body.x - 20, rowH - 4}, idx, -1});

        for (int button = 0; button < 2; button++) {
            const SDL_Rect box = {button == 0 ? reportX : blockX, rowY, btnW, btnH};
            const bool focused = idx == playersCursor && playersButton == button;
            SDL_SetRenderDrawColor(rend, menulist::kButtonFill.r, menulist::kButtonFill.g,
                                   menulist::kButtonFill.b, focused ? 235 : 90);
            { SDL_FRect fr = ToFRect(box); SDL_RenderFillRect(rend, &fr); }
            menulist::SetDrawColor(rend, focused ? menulist::kSelEdge : menulist::kEdge,
                                   focused ? 255 : 170);
            { SDL_FRect fr = ToFRect(box); SDL_RenderRect(rend, &fr); }
            const char* name = button == 0 ? (reported ? "Reported" : "Report")
                                           : (blocked ? "Unblock" : "Block");
            TTFText& text = TeamPanelCell(cellIdx++, 14, TTF_STYLE_BOLD);
            text.UpdateColor(menulist::kText, menulist::kTextShadow);
            text.UpdateText(rend, name, 0);
            text.UpdatePosition({box.x + box.w / 2 - text.Coords()->w / 2,
                                 box.y + box.h / 2 - text.Coords()->h / 2});
            SDL_FRect fr = ToFRect(*text.Coords());
            SDL_RenderTexture(rend, text.Texture(), nullptr, &fr);
            playersTaps.push_back({box, idx, button});
        }
    }

    if (count > shown) {
        char more[48];
        snprintf(more, sizeof(more), "... %d more (UP/DOWN scrolls)", count - shown);
        drawText(more, body.x + 16, firstRowY + shown * rowH + 2, menulist::kMuted, 12,
                 TTF_STYLE_NORMAL);
    }
    if (!playersNotice.empty())
        drawText(playersNotice.c_str(), body.x + 16, body.y + body.h - 24,
                 menulist::kGold, 13, TTF_STYLE_BOLD);

    menulist::DrawFooterHint(rend, panelText,
        "UP/DOWN picks a player    LEFT/RIGHT picks Report or Block    ENTER does it    ESC/Done closes");
}

bool MainMenu::PlayersPanelKey(SDL_Event* e) {
    if (!showingPlayersPanel) return false;
    const int count = (int)playersList.size();
    switch (e->key.key) {
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
            showingPlayersPanel = false;
            PlayMenuSFX("cancel");
            return true;
        case SDLK_UP:
            if (playersCursor > 0) { playersCursor--; PlayMenuSFX("menu_change"); }
            return true;
        case SDLK_DOWN:
            if (playersCursor < count - 1) { playersCursor++; PlayMenuSFX("menu_change"); }
            return true;
        case SDLK_LEFT:
        case SDLK_RIGHT:
        case SDLK_TAB:
            playersButton = 1 - playersButton;
            PlayMenuSFX("menu_change");
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            if (playersCursor >= 0 && playersCursor < count)
                PlayersPanelAct(playersList[playersCursor], playersButton);
            else
                showingPlayersPanel = false;
            return true;
        default:
            // Modal: nothing behind this page sees a key while it is open.
            return true;
    }
}

bool MainMenu::HandlePlayersPanelTap(float lx, float ly) {
    if (!showingPlayersPanel) return false;
    auto hit = [&](const SDL_Rect& r) {
        return r.w > 0 && r.h > 0 &&
               lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h;
    };
    if (hit(playersDoneRect)) {
        showingPlayersPanel = false;
        PlayMenuSFX("cancel");
        return true;
    }
    for (const PlayersTap& tap : playersTaps) {
        if (!hit(tap.rect)) continue;
        if (tap.row < 0 || tap.row >= (int)playersList.size()) break;
        playersCursor = tap.row;
        if (tap.button >= 0) {
            playersButton = tap.button;
            PlayersPanelAct(playersList[tap.row], tap.button);
        } else {
            PlayMenuSFX("menu_change");
        }
        return true;
    }
    // Modal, so a miss is consumed rather than reaching the room underneath.
    return true;
}
