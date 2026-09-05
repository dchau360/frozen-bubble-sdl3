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

// The full-screen team picker for a Team Mode room.
//
// The room's own team controls are cramped by construction. A <=5-cap room
// gives each player one cell of the settings grid -- 18 logical units tall,
// a couple of millimetres on a phone -- and a >5-cap room has no grid row at
// all (the grid clamps at five columns), only a per-row cycle hidden behind
// the roster. Neither can show what the teams actually are: both cycle
// blindly through the numbers one step at a time, so picking team 4 out of 5
// means tapping the same tiny target three times and reading the result each
// time to know where you are.
//
// A page of its own has room to draw every team as its own button on every
// player's row, which is the whole point: a team is chosen by touching it,
// once, with the current one already visibly marked. That also makes a
// single tap the right interaction here rather than the select-then-activate
// dance the cramped rows need -- that exists because a 18-unit row is easy
// to hit by accident, which a 34x26 button in a full-screen list is not.
//
// Both room sizes share this page. What differs between them is only where a
// change is stored and how it reaches the other clients, which is
// ApplyTeamChoice's business (see its comment); by the time this file draws
// a row it is asking TeamOfSlot and nothing else.

#include "mainmenu_internal.h"
#include "menulist.h"
#include "netteams.h"
#include "audiomixer.h"
#include "networkclient.h"
#include "bubblegame.h"   // kTeamColors

#include <algorithm>
#include <cstdio>
#include <string>

int MainMenu::MyRoomSlot() const {
    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* room = netClient->GetCurrentGame();
    if (!room) return -1;
    const std::string myNick = netClient->GetPlayerNick();
    for (int i = 0; i < (int)room->players.size(); i++)
        if (room->players[i].nick == myNick) return i;
    return -1;
}

int MainMenu::TeamOfSlot(int slot) const {
    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* room = netClient->GetCurrentGame();
    if (!room || slot < 0 || slot >= (int)room->players.size()) return 1;

    if (room->maxPlayers > 5) {
        // Nick-keyed overrides over a round-robin default -- SETOPTIONS has
        // no room to carry teams for P6-20, so a big room has no per-slot
        // array to read.
        auto it = netTeamOverrides.find(room->players[slot].nick);
        const int override = (it != netTeamOverrides.end()) ? it->second : 0;
        return EffectiveTeam(slot, netTeamCount, override);
    }
    return ClampTeamNumber(netPlayerTeams[slot]);
}

void MainMenu::ApplyTeamChoice(int slot, int team) {
    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* room = netClient->GetCurrentGame();
    if (!room || slot < 0 || slot >= (int)room->players.size()) return;
    team = ClampTeamNumber(team);

    const std::string& nick = room->players[slot].nick;
    const bool isHost = room->creator == netClient->GetPlayerNick();

    // Same wire message either way -- "!team:<nick>:<n>", carried as room
    // chat. What changes is who acts on it: in a >5-cap room every client
    // applies it to its own override map, and in a <=5-cap room the host
    // applies it to the per-slot array and re-broadcasts SETOPTIONS. Both
    // receivers live in NetPanelChatDockRender; this only has to send it and
    // apply the same change locally so the page doesn't wait a round trip to
    // redraw.
    if (room->maxPlayers > 5) {
        netTeamOverrides[nick] = team;
    } else {
        netPlayerTeams[slot] = team;
        if (isHost) SyncRoomOptions();
    }

    char talkMsg[64];
    snprintf(talkMsg, sizeof(talkMsg), "!team:%s:%d", nick.c_str(), team);
    netClient->SendTalk(talkMsg);
    AudioMixer::Instance()->PlaySFX("menu_change");
}

void MainMenu::OpenTeamsPanel(int slot) {
    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* room = netClient->GetCurrentGame();
    if (!room) return;

    const int mySlot = MyRoomSlot();
    const int playerCount = (int)room->players.size();
    // A joiner may only change their own row, so opening anywhere else would
    // just be a cursor they cannot use -- park it on their own row whatever
    // was asked for. The host's cursor goes wherever it was pointed.
    const bool isHost = room->creator == netClient->GetPlayerNick();
    int start = (slot >= 0 && slot < playerCount) ? slot : mySlot;
    if (!isHost) start = mySlot;
    if (start < 0) start = 0;

    teamsCursorPlayer = start;
    showingTeamsPanel = true;
    PlayMenuSFX("menu_selected");
}

void MainMenu::TeamsPanelRender() {
    if (!showingTeamsPanel) return;

    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* room = netClient->GetCurrentGame();
    // The room can go away underneath this page (kicked, host left, connection
    // dropped). Close rather than draw a page about nobody.
    if (!room || !netTeamMode) {
        showingTeamsPanel = false;
        teamSwatchTaps.clear();
        teamsDoneRect = SDL_Rect{};
        return;
    }

    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    const bool isHost = room->creator == netClient->GetPlayerNick();
    const int mySlot = MyRoomSlot();
    const int playerCount = (int)room->players.size();

    teamSwatchTaps.clear();
    teamsCursorPlayer = std::clamp(teamsCursorPlayer, 0, std::max(0, playerCount - 1));

    // How many teams this room offers. A >5-cap room fixes it at netTeamCount
    // (see the roster's own comment on why it isn't host-adjustable); a
    // <=5-cap room has always let any of the five be picked.
    const int teamCount = (room->maxPlayers > 5) ? ClampTeamNumber(netTeamCount) : kMaxTeams;

    // The header's right-hand action is the visible way out. Its rect comes
    // back from DrawHeaderBar rather than being computed here, so the button
    // that is drawn and the box that is clickable cannot drift apart -- the
    // action's width depends on the rendered text, which only DrawHeaderBar
    // knows. (actionIndex must be >= 0 or the action is not drawn at all; the
    // index itself is unused here since this page hit-tests its own rects
    // instead of registering panel tap rows.)
    teamsDoneRect = SDL_Rect{};
    menulist::DrawHeaderBar(rend, panelText, menulist::kHeaderBar, "SET TEAMS",
                            "Done", true, 0,
                            [&](int, const SDL_Rect& r, int, bool, SDL_Keycode) {
                                teamsDoneRect = r;
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

    auto drawText = [&](const char* txt, int x, int y, SDL_Color color, int size, int style) {
        panelText.UpdateStyle(size, style);
        panelText.UpdateColor(color, menulist::kTextShadow);
        panelText.UpdateText(rend, txt, 0);
        panelText.UpdatePosition({x, y});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
    };

    // Column header over the swatch block, so the numbers are identified once
    // rather than repeated on every row.
    const int swatchW = 34, swatchH = 26, swatchGap = 6;
    const int swatchBlockW = teamCount * swatchW + (teamCount - 1) * swatchGap;
    const int swatchX0 = body.x + body.w - 16 - swatchBlockW;
    drawText("PLAYER", body.x + 16, body.y + 10, menulist::kMuted, 13, TTF_STYLE_BOLD);
    drawText("TEAM", swatchX0, body.y + 10, menulist::kMuted, 13, TTF_STYLE_BOLD);
    SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g, menulist::kEdge.b, 110);
    SDL_RenderLine(rend, (float)(body.x + 12), (float)(body.y + 28),
                   (float)(body.x + body.w - 12), (float)(body.y + 28));

    // One row per seat that actually holds a player. Empty seats are left out
    // entirely rather than drawn greyed: a team can't be assigned to nobody,
    // and a 20-cap room would otherwise be mostly blank rows.
    const int rowH = 34;
    const int firstRowY = body.y + 36;
    const int maxRows = (body.h - 44) / rowH;
    const int shown = std::min(playerCount, maxRows);
    // Keep the cursor's row on screen when a big room overflows the page.
    const int firstShown = std::clamp(teamsCursorPlayer - shown + 1, 0,
                                      std::max(0, playerCount - shown));

    for (int i = 0; i < shown; i++) {
        const int slot = firstShown + i;
        if (slot >= playerCount) break;
        const NetworkPlayer& player = room->players[slot];
        const int rowY = firstRowY + i * rowH;
        const bool self = (slot == mySlot);
        // Who may move this player: the host moves anyone, everyone else only
        // themselves. Exactly the rule the roster's own edit mode applies --
        // and the one the server would not enforce for us, since a team
        // change travels as ordinary room chat.
        const bool editable = isHost || self;

        if (slot == teamsCursorPlayer) {
            SDL_Rect sel = {body.x + 10, rowY - 3, body.w - 20, rowH - 4};
            SDL_SetRenderDrawColor(rend, menulist::kSelFill.r, menulist::kSelFill.g,
                                   menulist::kSelFill.b, menulist::kSelFill.a);
            { SDL_FRect fr = ToFRect(sel); SDL_RenderFillRect(rend, &fr); }
            SDL_SetRenderDrawColor(rend, menulist::kSelEdge.r, menulist::kSelEdge.g,
                                   menulist::kSelEdge.b, menulist::kSelEdge.a);
            { SDL_FRect fr = ToFRect(sel); SDL_RenderRect(rend, &fr); }
        }

        char label[96];
        snprintf(label, sizeof(label), "%d.  %.12s%s%s", slot + 1, player.nick.c_str(),
                 room->creator == player.nick ? "   (host)" : "",
                 self ? "   (you)" : "");
        drawText(label, body.x + 16, rowY + 4,
                 self ? menulist::kGold : menulist::kText, 15,
                 self ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);

        const int current = TeamOfSlot(slot);
        for (int team = 1; team <= teamCount; team++) {
            SDL_Rect box = {swatchX0 + (team - 1) * (swatchW + swatchGap), rowY - 1,
                            swatchW, swatchH};
            const SDL_Color chip = kTeamColors[team - 1];
            const bool on = (team == current);

            // The team's own colour, full strength for the team this player
            // is on and dimmed for the rest, so the current one reads at a
            // glance across a 20-row list.
            SDL_SetRenderDrawColor(rend, chip.r, chip.g, chip.b, on ? 235 : 60);
            { SDL_FRect fr = ToFRect(box); SDL_RenderFillRect(rend, &fr); }
            if (on) {
                SDL_SetRenderDrawColor(rend, menulist::kSelEdge.r, menulist::kSelEdge.g,
                                       menulist::kSelEdge.b, 255);
            } else {
                SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g,
                                       menulist::kEdge.b, editable ? 160 : 60);
            }
            { SDL_FRect fr = ToFRect(box); SDL_RenderRect(rend, &fr); }

            char num[4];
            snprintf(num, sizeof(num), "%d", team);
            panelText.UpdateStyle(14, on ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
            panelText.UpdateColor(on ? menulist::kTextShadow
                                     : (editable ? menulist::kText : menulist::kMuted),
                                  on ? chip : menulist::kTextShadow);
            panelText.UpdateText(rend, num, 0);
            panelText.UpdatePosition({box.x + box.w/2 - panelText.Coords()->w/2,
                                       box.y + box.h/2 - panelText.Coords()->h/2});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

            // Only rows this client may actually change become tap targets.
            // A joiner tapping someone else's row would otherwise send a
            // "!team:" for them that every other client would apply.
            if (editable) teamSwatchTaps.push_back({box, slot, team});
        }
    }

    if (playerCount > shown) {
        char more[48];
        snprintf(more, sizeof(more), "... %d more", playerCount - shown);
        drawText(more, body.x + 16, firstRowY + shown * rowH + 4, menulist::kMuted, 13,
                 TTF_STYLE_NORMAL);
    }

    // Every way out is named, since this page covers the room completely and
    // a player who cannot find the way back is stuck looking at it: the Done
    // button above (click or tap), ESC/Enter, gamepad B (SDLK_AC_BACK, see
    // TeamsPanelKey), a right-click, or a back-swipe -- the last two both
    // arrive here as ESC.
    menulist::DrawFooterHint(rend, panelText,
        isHost ? "Tap a team    UP/DOWN player    LEFT/RIGHT team    ESC/Done to close"
               : "Tap your team    LEFT/RIGHT team    ESC/Done to close");
}

bool MainMenu::TeamsPanelKey(SDL_Event *e) {
    if (!showingTeamsPanel) return false;

    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* room = netClient->GetCurrentGame();
    if (!room) { showingTeamsPanel = false; return true; }

    const bool isHost = room->creator == netClient->GetPlayerNick();
    const int mySlot = MyRoomSlot();
    const int playerCount = (int)room->players.size();
    const int teamCount = (room->maxPlayers > 5) ? ClampTeamNumber(netTeamCount) : kMaxTeams;

    switch (e->key.key) {
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
        case SDLK_RETURN:
            showingTeamsPanel = false;
            PlayMenuSFX("cancel");
            return true;
        case SDLK_UP:
            // Only the host has anyone else's row to move to; a joiner's
            // cursor stays on their own, matching the roster's edit mode.
            if (isHost && teamsCursorPlayer > 0) {
                teamsCursorPlayer--;
                PlayMenuSFX("menu_change");
            }
            return true;
        case SDLK_DOWN:
            if (isHost && teamsCursorPlayer < playerCount - 1) {
                teamsCursorPlayer++;
                PlayMenuSFX("menu_change");
            }
            return true;
        case SDLK_LEFT:
        case SDLK_RIGHT: {
            const int slot = isHost ? teamsCursorPlayer : mySlot;
            if (slot < 0 || slot >= playerCount) return true;
            int team = TeamOfSlot(slot);
            if (e->key.key == SDLK_LEFT) { team--; if (team < 1) team = teamCount; }
            else                         { team++; if (team > teamCount) team = 1; }
            ApplyTeamChoice(slot, team);
            return true;
        }
        default:
            // Modal: nothing behind this page sees a key while it is open.
            return true;
    }
}

bool MainMenu::HandleTeamsPanelTap(float lx, float ly) {
    if (!showingTeamsPanel) return false;

    auto hit = [&](const SDL_Rect& r) {
        return r.w > 0 && r.h > 0 &&
               lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h;
    };

    if (hit(teamsDoneRect)) {
        showingTeamsPanel = false;
        PlayMenuSFX("cancel");
        return true;
    }

    // One tap sets the team, rather than the select-then-activate dance the
    // room's own cramped rows need -- see this file's header comment. Only
    // swatches this client may change were published, so a hit here is
    // already known to be allowed.
    for (const TeamSwatchTap& swatch : teamSwatchTaps) {
        if (!hit(swatch.rect)) continue;
        if (TeamOfSlot(swatch.slot) != swatch.team)
            ApplyTeamChoice(swatch.slot, swatch.team);
        teamsCursorPlayer = swatch.slot;
        return true;
    }

    // Modal, so a miss is consumed rather than reaching the room underneath.
    return true;
}
