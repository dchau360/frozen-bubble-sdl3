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

#include "mainmenu.h"
#include "netteams.h"
#include "audiomixer.h"
#include "frozenbubble.h"
#include "menulist.h"
#include "transitionmanager.h"
#include "networkclient.h"
#include "platform.h"
#include "bubblegame.h"

#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <errno.h>
#include <thread>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include "socket_compat.h"
#ifndef _WIN32
#ifndef __WASM_PORT__
#include <netdb.h>
#endif
#endif
#ifdef __WASM_PORT__
#include <emscripten.h>
#include <stdlib.h>
#endif

#include <algorithm>
#include "mainmenu_internal.h"
#include "localmultiplayer_settings.h"
#include "netbot.h"

// Indices of the chat messages that should actually be shown, newest last.
//
// Two things are hidden: "!team:" control traffic, which is protocol chatter
// the players were never meant to read, and anyone this device has blocked.
// Server notices are never suppressed -- a blocked nick must not be able to
// hide "*** so-and-so joined" or anything else the server itself says.
//
// This is collected *before* callers take the last N, rather than skipping
// while drawing: a hidden message would otherwise eat its line slot and leave
// the view half empty, so blocking one noisy player would shrink everyone
// else's visible history too.
//
// Shared because both chat views need the same answer. They previously each
// had their own copy and had already drifted -- only one of them filtered
// "!team:", so the same message list rendered differently depending on which
// screen happened to be up.
static std::vector<size_t> VisibleChatIndices(const std::vector<ChatMessage>& msgs) {
    const GameSettings* gs = GameSettings::Instance();
    std::vector<size_t> visible;
    visible.reserve(msgs.size());
    for (size_t i = 0; i < msgs.size(); i++) {
        if (msgs[i].message.size() > 6 && msgs[i].message.compare(0, 6, "!team:") == 0) continue;
        if (msgs[i].nick != "Server" && gs->IsPlayerBlocked(msgs[i].nick)) continue;
        visible.push_back(i);
    }
    return visible;
}

// Bots are ordinary members of the room -- each one has its own connection
// and the server cannot tell it from a person. Only this client knows which
// ones they are, and only this client simulates them.
void MainMenu::SyncLobbyBots() {
    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* room = netClient ? netClient->GetCurrentGame() : nullptr;
    if (!netClient || !room) {
        DropLobbyBots();
        return;
    }

    while ((int)lobbyBots.size() > netRoomBotCount) {
        if (lobbyBots.back()) lobbyBots.back()->Leave();
        lobbyBots.pop_back();
    }

    while ((int)lobbyBots.size() < netRoomBotCount) {
        // The server truncates nicknames to ten characters and the roster is
        // matched by nickname, so the name has to stay distinct within that.
        char botNick[16];
        snprintf(botNick, sizeof(botNick), "bot%d-%03d",
                 (int)lobbyBots.size() + 1, (int)(SDL_GetTicks() % 1000));
        auto bot = std::make_unique<NetBotConnection>();
        if (!bot->JoinRoom(netClient->GetHost(), netClient->GetPort(),
                           room->creator, botNick)) {
            netClient->AddStatusMessage("Could not add a bot");
            netRoomBotCount = (int)lobbyBots.size();
            return;
        }
        lobbyBots.push_back(std::move(bot));
    }

    // The leader blocks waiting for every other player to acknowledge the
    // game start, and a bot acknowledges on its own socket. Without this it
    // would be waiting behind the very loop that has to service it.
    if (!lobbyBots.empty()) {
        netClient->SetLeaderWaitTick([this]() { PumpLobbyBots(); });
    }
}

void MainMenu::PumpLobbyBots() {
    for (auto& bot : lobbyBots) {
        if (bot) bot->Update();
    }
    // A bot's connection can go without us asking -- the host can kick it
    // like any other room member, the server drops it if the room does, or
    // (only knowable once its BOT command has actually been answered, which
    // is why this is checked here rather than where JoinRoom fires it) the
    // server's own -b bot cap turned it away. Forget it here so the count on
    // screen still means what it says, and so asking for the same number
    // again actually reconnects one.
    int rejectedCount = 0;
    const size_t before = lobbyBots.size();
    lobbyBots.erase(std::remove_if(lobbyBots.begin(), lobbyBots.end(),
                                   [&rejectedCount](const std::unique_ptr<NetBotConnection>& b) {
                                       if (!b || !b->IsConnected()) {
                                           if (b && b->WasRejectedByServer()) ++rejectedCount;
                                           return true;
                                       }
                                       return false;
                                   }),
                    lobbyBots.end());
    if (lobbyBots.size() != before) netRoomBotCount = (int)lobbyBots.size();
    if (rejectedCount > 0) {
        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient) {
            char msg[96];
            if (rejectedCount == 1) {
                snprintf(msg, sizeof(msg), "Server bot limit reached -- one bot could not join");
            } else {
                snprintf(msg, sizeof(msg), "Server bot limit reached -- %d bots could not join",
                         rejectedCount);
            }
            netClient->AddStatusMessage(msg);
        }
    }
}

void MainMenu::DropLobbyBots() {
    for (auto& bot : lobbyBots) {
        if (bot) bot->Leave();
    }
    lobbyBots.clear();
    netRoomBotCount = 0;
    NetworkClient* netClient = NetworkClient::Instance();
    if (netClient) netClient->SetLeaderWaitTick(nullptr);
}

void MainMenu::NetPanelRender() {
    if (!showingNetPanel) return;

    NetworkClient* netClient = NetworkClient::Instance();

    // Auto-complete lobby entry once async WebSocket connection opens (WASM only).
    // On the first Enter press the WebSocket is CONNECTING so SendNick fails; we set
    // pendingLobbyConnect and come back here each frame until state becomes CONNECTED.
    if (pendingLobbyConnect && netClient->GetState() == CONNECTED) {
        SDL_Log("pendingLobbyConnect: WebSocket now CONNECTED, completing lobby entry");
        pendingLobbyConnect = false;
        char nickname[32];
        if (networkPreNick[0] != '\0') {
            snprintf(nickname, sizeof(nickname), "%s", networkPreNick);
        } else {
            const char* envUser = getenv("USER");
            if (envUser && envUser[0] != '\0') snprintf(nickname, sizeof(nickname), "%s", envUser);
#ifdef __ANDROID__
            else snprintf(nickname, sizeof(nickname), "android_user");
#else
            else snprintf(nickname, sizeof(nickname), "unnamed");
#endif
        }
        if (netClient->SendNick(nickname)) {
#ifdef __WASM_PORT__
            EM_ASM({ localStorage.setItem('fb_nickname', UTF8ToString($0)); }, nickname);
#else
            GameSettings* gsn = GameSettings::Instance();
            snprintf(gsn->savedNickname, sizeof(gsn->savedNickname), "%s", nickname);
            gsn->SaveKeys();
#endif
            std::string geoLoc = NetworkClient::DetectGeoLocation();
            float gLat = 0.0f, gLon = 0.0f;
            if (sscanf(geoLoc.c_str(), "%f:%f", &gLat, &gLon) == 2) {
                myGeoLat = gLat; myGeoLon = gLon; myGeoLocSet = true;
            }
            if (netClient->SendGeoLoc(geoLoc.c_str())) {
                networkInLobby = true;
                networkInputMode = 0;
                networkGameStarting = false;
                wasmSyncWaitStart = 0;
                RefreshFollowRegistration();
                netClient->RequestList();
                lastListRequest = SDL_GetTicks();
#ifdef __ANDROID__
                SDL_SendAndroidMessage(0x8001, 0);
#endif
            }
        }
    }

    // Update network client
    if (netClient->IsConnected()) {
        // Before netClient->Update(), so a bot that has already been told the
        // game is starting has answered by the time this client acts on the
        // same news.
        PumpLobbyBots();
        netClient->Update();

        // Apply any options broadcast by the host (joiners receive SETOPTIONS push)
        {
            bool cr, cl, st; int vl; int pc[5]; bool nc[5]; bool ag[5]; bool me; bool cm; bool dm; bool tm; int pt[5]; int rcvTc;
            if (netClient->GetAndClearPendingOptions(cr, cl, st, vl, pc, nc, ag, me, cm, dm, tm, pt, rcvTc)) {
                chainReactionEnabled = cr;
                (void)cl;  // "continue when players leave" is always on now
                singlePlayerTargetting = st;
                // Map vl to victoriesLimitIndex
                static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                victoriesLimitIndex = 5; // default
                for (int i = 0; i < 18; i++) { if (vLimits[i] == vl) { victoriesLimitIndex = i; break; } }
                for (int i = 0; i < 5; i++) { playerColorCounts[i] = pc[i]; playerNoCompress[i] = nc[i]; playerAimGuide[i] = ag[i]; }
                netRoomMouseEnabled = me;
                netClearMode = cm;
                netDisableMalus = dm;
                netTeamMode = tm;
                if (rcvTc >= 2 && rcvTc <= 5) netTeamCount = rcvTc;
                for (int i = 0; i < 5; i++) netPlayerTeams[i] = pt[i];
                SDL_Log("Applied host options: cr=%d cl=%d st=%d vl=%d colors=%d,%d,%d,%d,%d mouse=%d cm=%d dm=%d tm=%d",
                    cr,cl,st,vl,pc[0],pc[1],pc[2],pc[3],pc[4],me,cm,dm,tm);
            }
        }

        // Check if game is ready to start (state transitioned to IN_GAME)
        if (!networkGameStarting && netClient->GetState() == IN_GAME) {
#ifdef __WASM_PORT__
            // WASM joiner: WaitForBubble spins without yielding (no Asyncify),
            // so WebSocket callbacks never fire during SyncNetworkLevel.
            // Wait here (across animation frames) until all 40 sync messages
            // (38 bubbles + N + T) are queued, then SetupNewGame will find
            // them already in the queue and WaitForBubble returns immediately.
            if (!netClient->IsLeader()) {
                if (wasmSyncWaitStart == 0) wasmSyncWaitStart = SDL_GetTicks();
                size_t qSize = netClient->MessageQueueSize();
                bool timedOut = (SDL_GetTicks() - wasmSyncWaitStart > 5000);
                SDL_Log("WASM joiner: waiting for sync msgs, queue=%d, waited=%dms",
                        (int)qSize, (int)(SDL_GetTicks() - wasmSyncWaitStart));
                if (qSize < 40 && !timedOut) {
                    return;  // Come back next frame
                }
                SDL_Log("WASM joiner: proceeding with queue=%d timedOut=%d", (int)qSize, timedOut);
                wasmSyncWaitStart = 0;
            }
#endif
            SDL_Log("Game starting - transitioning to network game");
            networkGameStarting = true;

            // OK_GAME_START is now sent automatically by NetworkClient when receiving GAME_CAN_START

            // Copy network lobby settings to game settings
            chainReaction = chainReactionEnabled;
            SDL_Log("Starting game with chainReaction=%s", chainReaction ? "true" : "false");

            // Start network multiplayer game (mode 4 = network multiplayer)
            SetupNewGame(4); // Use network multiplayer mode
            networkInLobby = false;
            showingNetPanel = false;
            SDL_Log("Set showingNetPanel=false, networkInLobby=false before return");

            // Return immediately to avoid rendering lobby UI after game has started
            return;
        }
    }

    // Additional safety: Don't render lobby UI if game is in progress
    if (netClient && netClient->GetState() == IN_GAME) {
        showingNetPanel = false;
        return;
    }

    // If in lobby, use world map background; otherwise use void panel for connection screens

    if (networkInLobby && netGameBackground && networkInputMode == 0) {
        NetPanelWorldMapRender();
        NetPanelLobbyActionsRender();
        NetPanelChatDockRender();
        return;
    }

    // Composing a message: same world map behind, but the chat dock grown over
    // the room's settings so the whole readable log is on screen. The action
    // list is left out rather than drawn behind -- none of it is reachable
    // while typing, and on a phone the keyboard covers the lower half anyway.
    if (networkInLobby && netGameBackground && networkInputMode == 4) {
        NetPanelWorldMapRender();
        NetPanelChatDockRender(true);
        return;
    }

    NetPanelConnectionScreensRender();
}

void MainMenu::NetPanelWorldMapRender() {
    NetworkClient* netClient = NetworkClient::Instance();

    // Reset text color to white — non-lobby screens (connecting, server list) may have
    // left panelText set to yellow, which persists across frames.
    panelText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

        // Request LIST periodically (every 2 seconds)
        Uint32 now = SDL_GetTicks();
        if (now - lastListRequest > 500) {
            netClient->RequestList();
            lastListRequest = now;
        }

        // Render world map background
        SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), netGameBackground, nullptr, nullptr);

        // Render geolocation spots on world map (original: print_spot / save_back_spot)
        // Coordinate formula matches original Perl get_spot_location() at line 4084
        auto geoToScreen = [](float lat, float lon) -> SDL_Point {
            const float x0 = 309.0f, y0 = 231.0f;
            const float lonFactor = 1.424f, latFactor = -145.0f;
            float x = x0 + lon * lonFactor;
            float y = y0 + (float)(std::asinh(std::tan((double)lat * 1.4 * 3.14159265358979323846 / 360.0)) * latFactor);
            return {(int)x, (int)y};
        };
        auto renderSpot = [&](SDL_Texture* tex, int x, int y, const char* nick) {
            if (!tex) return;
            int w = 0, h = 0;
            { float fw, fh; SDL_GetTextureSize(tex, &fw, &fh); w = (int)fw; h = (int)fh; }
            SDL_Rect r = {x - w / 2, y - h / 2, w, h};
            { SDL_FRect fr = ToFRect(r); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), tex, nullptr, &fr); }
            if (nick && nick[0]) {
                networkText.UpdateText(const_cast<SDL_Renderer*>(renderer), nick, 0);
                networkText.UpdatePosition({x - networkText.Coords()->w / 2, y + h / 2 + 1});
                { SDL_FRect fr = ToFRect(*networkText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), networkText.Texture(), nullptr, &fr); };
            }
        };

        // Draw spots for players in games (rendered first, underneath free players)
        {
            std::vector<GameRoom> games = netClient->GetGameList();
            for (const auto& game : games) {
                for (const auto& p : game.players) {
                    if (p.nick == netClient->GetPlayerNick()) continue;
                    float lat = 0.0f, lon = 0.0f;
                    if (sscanf(p.geoloc.c_str(), "%f:%f", &lat, &lon) != 2) continue;
                    SDL_Point sp = geoToScreen(lat, lon);
                    GameRoom* myGame = netClient->GetCurrentGame();
                    bool inMyGame = myGame && (game.creator == myGame->creator);
                    renderSpot(inMyGame ? netSpotInGame : netSpotPlaying, sp.x, sp.y, p.nick.c_str());
                }
            }
        }

        // Draw spots for free (open) players
        {
            std::vector<NetworkPlayer> openPlayers = netClient->GetOpenPlayers();
            for (const auto& p : openPlayers) {
                if (p.nick == netClient->GetPlayerNick()) continue;
                float lat = 0.0f, lon = 0.0f;
                if (sscanf(p.geoloc.c_str(), "%f:%f", &lat, &lon) != 2) continue;
                SDL_Point sp = geoToScreen(lat, lon);
                renderSpot(netSpotFree, sp.x, sp.y, p.nick.c_str());
            }
        }

        // Draw own animated self spot (top layer)
        if (myGeoLocSet) {
            SDL_Point myPos = geoToScreen(myGeoLat, myGeoLon);
            SDL_Texture* selfTex = netSpotSelf[netSpotSelfFrame];
            renderSpot(selfTex, myPos.x, myPos.y, nullptr);  // No label for self
            if (++netSpotSelfFrameTimer >= 4) {  // ~4 frames at 60fps ≈ 60ms
                netSpotSelfFrameTimer = 0;
                if (++netSpotSelfFrame >= 13) netSpotSelfFrame = 0;
            }
        }
}

void MainMenu::NetPanelLobbyActionsRender() {
    NetworkClient* netClient = NetworkClient::Instance();
    SDL_Renderer* roomRenderer = const_cast<SDL_Renderer*>(renderer);

    // The lobby and the game room are one screen with two selection axes: the
    // action/room list, and the player column for the per-player grid rows.
    BeginPanelTapRows(&selectedActionIndex, &currentPlayerCol);

    // Card/panel drawing primitives shared by every box in this revamped
    // layout (header bar, match-rules panel, room cards, player sidebar,
    // online sidebar). Byte-identical copies live in NetPanelChatDockRender()
    // since C++ lambdas aren't shared across functions without extra
    // plumbing.
    auto drawPanel = [&](const SDL_Rect& rect, SDL_Color fill, SDL_Color outline) {
        SDL_SetRenderDrawBlendMode(roomRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(roomRenderer, fill.r, fill.g, fill.b, fill.a);
        SDL_FRect fr = ToFRect(rect);
        SDL_RenderFillRect(roomRenderer, &fr);
        SDL_SetRenderDrawColor(roomRenderer, outline.r, outline.g, outline.b, outline.a);
        SDL_RenderRect(roomRenderer, &fr);
    };
    auto drawLabel = [&](const char* text, int x, int y, SDL_Color color) {
        panelText.UpdateColor(color, {20, 12, 32, 255});
        panelText.UpdateText(roomRenderer, text, 0);
        panelText.UpdatePosition({x, y});
        SDL_FRect fr = ToFRect(*panelText.Coords());
        SDL_RenderTexture(roomRenderer, panelText.Texture(), nullptr, &fr);
    };
    auto drawSelection = [&](const SDL_Rect& rect) {
        SDL_SetRenderDrawColor(roomRenderer, 255, 196, 64, 72);
        SDL_FRect fr = ToFRect(rect);
        SDL_RenderFillRect(roomRenderer, &fr);
        SDL_SetRenderDrawColor(roomRenderer, 255, 218, 92, 240);
        SDL_RenderRect(roomRenderer, &fr);
    };

    const SDL_Color panelEdge = {255, 190, 46, 225};
    const SDL_Color textMain  = {248, 250, 239, 255};
    const SDL_Color textMuted = {174, 211, 202, 255};
    const SDL_Color textGold  = {255, 218, 92, 255};
    // Team colors — shared with the in-gameplay team indicators so a team
    // reads the same color everywhere.
    const SDL_Color *teamColors = kTeamColors;

        // Render action list at top left (like original)
        const int actionStartX = 24;
        const int lineHeight = 16;

        // Action menu: Different options depending on whether in a game
        std::vector<std::string> actions;
        GameRoom* currentGame = netClient->GetCurrentGame();

        if (currentGame) {
            // In a game room - show game options
            actions.push_back("Chat");  // index 0

            // Mode and Malus (indices 1-2) — surfaced first since they define the
            // match type; newly created rooms default focus to "Game mode".
            char modeText[64], malusText[64];
            const char* mode = netTeamMode ? "Teams" : (netClearMode ? "Clear" : "Classic");
            snprintf(modeText, sizeof(modeText), "Game mode: %s", mode);
            snprintf(malusText, sizeof(malusText), "Attack bubbles: %s", netDisableMalus ? "OFF" : "ON");
            actions.push_back(modeText);  // index 1
            actions.push_back(malusText); // index 2

            // Global settings - same for host and joiner. "Continue when
            // players leave" used to sit between chain-reaction and targetting;
            // it is now always on and has no row.
            char crText[64], targetText[64], victoriesText[64];
            snprintf(crText, sizeof(crText), "Chain-reaction: %s", chainReactionEnabled ? "enabled" : "disabled");
            snprintf(targetText, sizeof(targetText), "Single player targetting: %s", singlePlayerTargetting ? "enabled" : "disabled");
            const char* victoriesLimits[] = {"none (unlimited)", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "15", "20", "30", "50", "100"};
            snprintf(victoriesText, sizeof(victoriesText), "Victories limit: %s", victoriesLimits[victoriesLimitIndex]);
            actions.push_back(crText);        // kRoomChain
            actions.push_back(targetText);    // kRoomTarget
            actions.push_back(victoriesText); // kRoomVictories

            // Mouse/touch aim (kRoomMouse) — per-session local setting, defaults OFF
            {
                char mouseText[64];
                snprintf(mouseText, sizeof(mouseText), "Mouse/Touch aim: %s", netRoomMouseEnabled ? "ON" : "OFF");
                actions.push_back(mouseText); // kRoomMouse
            }

            // Per-player grid rows — label only; values rendered as grid cells below
            actions.push_back("Max colors:"); // kRoomMaxColors
            actions.push_back("Rows:");       // kRoomRows
            actions.push_back("Aim:");        // kRoomAim
            actions.push_back("Team:");       // kRoomTeam

            // Bots (kRoomBots, kRoomBotSkill) — host only. A bot joins the
            // room as a real player: everyone sees it in the roster and it
            // counts against the room's cap. Only the host simulates it.
            const bool isRoomHost = currentGame->creator == netClient->GetPlayerNick();
            if (isRoomHost) {
                // Both rows are drawn under the roster rather than here, but
                // they still need slots in this list: it is what Up/Down
                // navigates, and the indices below depend on their being here.
                actions.push_back("Bots");      // kRoomBots
                actions.push_back("Bot skill"); // kRoomBotSkill
            }

            // Start game (kRoomStart) — host only when >1 player
            if (isRoomHost && currentGame->players.size() > 1) {
                actions.push_back("Start game!"); // kRoomStart
            }
            // No "Part game" menu item - use ESC key to leave like original

            // >5-cap Team Mode: team count is fixed at 5 (kTeamColors' full
            // range) rather than host-adjustable -- an earlier "Teams: N"
            // row here was removed after live playtesting found it visually
            // overlapped the per-player grid's "Team:" row (both landed at
            // y~302-306, since the settings column has no vertical room left
            // above the persistent chat dock at y=334) and its 2-5 range was
            // confusing (defaulted to 2, so red/blue was all that appeared
            // until the row was found and adjusted). Per-player team
            // assignment (host: any player; joiner: self only) happens
            // directly in the player-columns roster via the [A] hotkey.
        } else {
            // In lobby - show create/join options
            actions.push_back("Chat");
            actions.push_back("Create new game");
            actions.push_back("Follow this server");  // index kLobbyFollow; rendered in the header below, not here

            std::vector<GameRoom> games = netClient->GetGameList();
            for (const auto& game : games) {
                int n = (int)game.players.size();
                char roomLbl[160];
                // Card-style summary: host, count/cap, and status badge. The cap
                // comes from LIST's "]:N" suffix (5 when an old server omits it).
                snprintf(roomLbl, sizeof(roomLbl), "%s's room  (%d/%d)%s",
                         game.creator.c_str(), n, game.maxPlayers,
                         game.started ? "  [in game]" : "");
                actions.push_back(roomLbl);
            }
        }

        const int gridStart = kRoomGridFirst;  // First grid row index

        bool hasStartRow = currentGame && currentGame->creator == netClient->GetPlayerNick()
                            && currentGame->players.size() > 1;

        // menulist's tap-row signature, for the two menulist:: calls below --
        // this function still drives its own selection through
        // selectedActionIndex/currentPlayerCol via BeginPanelTapRows above,
        // menulist just needs a matching callback shape to register into it.
        auto menulistTap = [&](int index, const SDL_Rect& rect, int subIndex,
                                bool splitAdjust, SDL_Keycode key) {
            AddPanelTapRow(index, rect, subIndex, splitAdjust, key);
        };

        if (currentGame) {
            char title[160];
            bool isHost = currentGame->creator == netClient->GetPlayerNick();
            snprintf(title, sizeof(title), "%.24s's GAME ROOM   |   %s   |   %d players",
                     currentGame->creator.c_str(), isHost ? "HOST" : "GUEST",
                     (int)currentGame->players.size());
            menulist::DrawHeaderBar(roomRenderer, panelText, menulist::kHeaderBar, title,
                hasStartRow ? "Start game!" : nullptr, selectedActionIndex == kRoomStart,
                hasStartRow ? kRoomStart : -1, menulistTap, menulist::kMapFillAlpha);
        } else {
            // Header bar establishes location and role at a glance. Lower
            // alpha than the other hand-rolled panels in this branch --
            // see kMapFillAlpha -- so the world map behind it actually
            // shows through instead of being fully hidden.
            drawPanel({10, 8, 620, 28}, {38, 20, 57, menulist::kMapFillAlpha}, panelEdge);
            char title[160];
            snprintf(title, sizeof(title), "ONLINE LOBBY   |   %s", netClient->GetPlayerNick().c_str());
            drawLabel(title, 20, 14, textGold);

            // Follow-this-server toggle, right-aligned in the header bar --
            // same treatment as "Start game!" in the room header above, and
            // reachable however this server was connected to (list star, LAN
            // discovery, manual entry), since all of them land here once
            // connected. netClient->GetNotifySupport() answers a one-time
            // capability probe (ProbeNotifySupportIfNeeded()) so a server
            // that has never heard of the follow protocol says so instead of
            // silently doing nothing when toggled.
            {
                netClient->ProbeNotifySupportIfNeeded();
                NotifySupport support = netClient->GetNotifySupport();
                bool followed = GameSettings::Instance()->IsServerFollowed(
                    netClient->GetHost(), netClient->GetPort());

                const char* followText;
                bool interactive;
                if (support == NotifySupport::Unsupported) {
                    followText = "Follow: not supported by server";
                    interactive = false;
                } else if (support == NotifySupport::Unknown) {
                    followText = "Follow: checking...";
                    interactive = false;
                } else if (followed) {
                    followText = "★ Following this server";
                    interactive = true;
                } else {
                    followText = "☆ Follow this server (F)";
                    interactive = true;
                }

                bool followSel = interactive && (selectedActionIndex == kLobbyFollow);
                SDL_Color followColor = followSel ? textGold : (interactive ? textMain : textMuted);
                panelText.UpdateColor(followColor, {20, 12, 32, 255});
                panelText.UpdateText(roomRenderer, followText, 0);
                int tw = panelText.Coords()->w;
                int sx = 622 - tw;
                if (followSel) drawSelection({sx - 6, 10, tw + 12, 24});
                drawLabel(followText, sx, 14, followColor);
                if (interactive) {
                    AddPanelTapRow(kLobbyFollow, {sx - 6, 10, tw + 12, 24}, -1, false, SDLK_F);
                }
            }

        }

        // Lobby room browser, as a menulist::List -- same widget, same
        // kListDocked geometry, same navy/gold row style as the game room's
        // own Match rules list below, instead of the hand-rolled teal cards
        // this replaced (found live: the lobby and the room it leads into
        // read as two different screens). Scrolling is List::End's own, so
        // the old fixed "5 rooms at a time" window and its firstVisibleRoom
        // math are gone too -- List already does that for any row count.
        if (!currentGame) {
            menulist::List lobbyList(menulist::kListDocked, selectedActionIndex, 32,
                                      menulist::kMapFillAlpha);
            lobbyList.Header("Game rooms");
            // Not splitAdjust: this row's tap has always meant "create" (the
            // synthetic Return HandlePanelTap sends for a non-splitAdjust
            // row) -- room-size is keyboard Left/Right only, same as before
            // this conversion (see the `!currentGame && selectedActionIndex
            // == 1` branch below). A real splitAdjust row here would eat
            // every tap as a Left/Right step instead, breaking the only way
            // touch players have to actually create a room. The "< N
            // players >" bracket format is kept by building the value
            // string ourselves rather than letting splitAdjust add it.
            char createValue[32];
            snprintf(createValue, sizeof(createValue), "<  %d players  >", kRoomSizes[netRoomSizeChoice]);
            lobbyList.Row(1, "Create Game Room", createValue, true, false);
            for (size_t i = (size_t)(kLobbyFollow + 1); i < actions.size() && i < 18; i++) {
                lobbyList.Row((int)i, actions[i], "");
            }
            lobbyList.End(roomRenderer, panelText, nullptr, menulistTap);
        }

        // Game room: match rules + controls as menulist rows -- 28px pitch
        // rather than the shared 32px default, so the per-player grid below
        // still fits above the persistent chat dock at y=334 without either
        // of them scrolling. Still a real jump from the 15px this replaced,
        // and the grid's own bordered header row is enough of a section
        // break on its own, so there's no separate "Player setup" label
        // spending more of that budget than the grid itself needs.
        int gridHeaderY = 0;
        if (currentGame) {
            // A >5-cap room's roster sidebar (below) is the wider 2-column
            // layout starting at x=354, not the fat single-column one at
            // x=450 -- kListDocked's width alone would run 60px into it
            // (found live, on device: a 20-player room's "Classic" value
            // was rendering clipped exactly at that seam). The per-player
            // grid a few lines down already accounts for this same seam
            // (its own width tops out at 348), this just matches it.
            const bool bigRoomLayout = currentGame->maxPlayers > 5;
            SDL_Rect roomListRect = menulist::kListDocked;
            if (bigRoomLayout) roomListRect.w = 336;
            menulist::List roomList(roomListRect, selectedActionIndex, 28,
                                     menulist::kMapFillAlpha);
            roomList.Header("Match rules");
            const char* mode = netTeamMode ? "Teams" : (netClearMode ? "Clear" : "Classic");
            roomList.Row(kRoomMode, "Game mode", mode);
            roomList.Row(kRoomMalus, "Attack bubbles", netDisableMalus ? "OFF" : "ON", !netDisableMalus);
            roomList.Row(kRoomChain, "Chain reaction", chainReactionEnabled ? "ON" : "OFF", chainReactionEnabled);
            roomList.Row(kRoomTarget, "Solo targetting", singlePlayerTargetting ? "ON" : "OFF", singlePlayerTargetting);
            const char* victoriesLimits[] = {"none (unlimited)", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "15", "20", "30", "50", "100"};
            roomList.Row(kRoomVictories, "Victories limit", victoriesLimits[victoriesLimitIndex]);
            roomList.Row(kRoomMouse, "Mouse / touch aim", netRoomMouseEnabled ? "ON" : "OFF", netRoomMouseEnabled);
            int nextY = roomList.End(roomRenderer, panelText, nullptr, menulistTap);

            // List::End leaves panelText sized/styled for its own last draw
            // (20px bold); the grid below never sets its own style, it just
            // relies on whatever panelText was last left at, so this is
            // where that gets reset to a size the grid was actually
            // designed around instead of silently inheriting the list's.
            panelText.UpdateStyle(16, TTF_STYLE_NORMAL);
            gridHeaderY = nextY + 6;
        }

        // Render the per-player settings grid (only in a game room)
        if (currentGame) {
            int numPlayers = (int)currentGame->players.size();
            if (numPlayers < 1) numPlayers = 1;
            if (numPlayers > 5) numPlayers = 5;
            bool isHost = currentGame->creator == netClient->GetPlayerNick();
            std::string myNickForGrid = netClient->GetPlayerNick();
            int myJoinerSlot = -1;
            for (int i = 0; i < (int)currentGame->players.size(); i++) {
                if (currentGame->players[i].nick == myNickForGrid) { myJoinerSlot = i; break; }
            }

            // Column layout
            const int labelW = 110;  // Width of row label ("Max colors:", "Row collapse:", "Aim guide:", "Team:")
            const int colW   = 36;   // Width of each player column
            auto drawTeamSwatch = [&](int colLeft, int rowTop, int teamVal) {
                int t = teamVal;
                if (t < 1 || t > 5) t = 1;
                SDL_Color c = teamColors[t - 1];
                SDL_SetRenderDrawColor(const_cast<SDL_Renderer*>(renderer), c.r, c.g, c.b, 140);
                SDL_Rect swatch = {colLeft + 3, rowTop - 1, colW - 6, lineHeight};
                SDL_FRect fr = ToFRect(swatch);
                SDL_RenderFillRect(const_cast<SDL_Renderer*>(renderer), &fr);
            };

            // gridHeaderY comes from where the menulist rows above actually
            // ended this frame (roomList.End()'s return value, plus the
            // "Player setup" section header drawn under it) rather than a
            // hardcoded constant -- the row count above it (and so its
            // height) can change with room state (Team mode adds no row
            // today, but the point of deriving this is that it wouldn't
            // silently misalign the grid if that changes).
            int headerY = gridHeaderY;
            int firstDataRowY = headerY + lineHeight;

            // Draw grid lines
            {
                SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
                SDL_SetRenderDrawColor(rend, 180, 180, 200, 220);

                int totalCols = numPlayers + 1; // ALL + P1..PN
                int gridLeft  = actionStartX - 2;
                int gridRight = actionStartX + labelW + totalCols * colW;
                int gridTop   = headerY - 1;
                int gridBot   = firstDataRowY + 4 * lineHeight; // 4 data rows

                // Outer border
                SDL_Rect border = {gridLeft, gridTop, gridRight - gridLeft, gridBot - gridTop};
                { SDL_FRect fr = ToFRect(border); SDL_RenderRect(rend, &fr); }

                // Horizontal line after header row
                SDL_RenderLine(rend, (float)gridLeft, (float)(firstDataRowY - 1), (float)gridRight, (float)(firstDataRowY - 1));
                // Horizontal lines between data rows (after Colors, after Row collapse, after Aim)
                for (int r = 1; r <= 3; r++) {
                    int y = firstDataRowY + r * lineHeight;
                    SDL_RenderLine(rend, (float)gridLeft, (float)y, (float)gridRight, (float)y);
                }

                // Vertical line between label and ALL column
                int xLabel = actionStartX + labelW;
                SDL_RenderLine(rend, (float)xLabel, (float)gridTop, (float)xLabel, (float)gridBot);

                // Vertical lines between each column (ALL|P1, P1|P2, ...)
                for (int c = 1; c <= totalCols - 1; c++) {
                    int x = actionStartX + labelW + c * colW;
                    SDL_RenderLine(rend, (float)x, (float)gridTop, (float)x, (float)gridBot);
                }
            }
            // Helper: render text centered within a column cell
            auto renderCentered = [&](const char* txt, int colLeft, int y) {
                SDL_Renderer* rend2 = const_cast<SDL_Renderer*>(renderer);
                panelText.UpdateText(rend2, txt, 0);
                int tw = 0;
                if (panelText.Texture()) { float ftw; SDL_GetTextureSize(panelText.Texture(), &ftw, nullptr); tw = (int)ftw; }
                int cx = colLeft + colW / 2 - tw / 2;
                panelText.UpdatePosition({cx, y});
                { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend2, panelText.Texture(), nullptr, &fr); };
            };

            // ALL header
            renderCentered("ALL", actionStartX + labelW, headerY);
            // P1..PN headers
            for (int pi = 0; pi < numPlayers; pi++) {
                char pnum[4];
                snprintf(pnum, sizeof(pnum), "P%d", pi + 1);
                renderCentered(pnum, actionStartX + labelW + (pi + 1) * colW, headerY);
            }

            // Grid rows: Colors (8), Rows (9), Aim (10), Team (11)
            const char* rowLabels[] = {"Max colors:", "Row collapse:", "Aim guide:", "Team:"};
            for (int row = 0; row < 4; row++) {
                int rowIdx = gridStart + row;
                int rowY   = firstDataRowY + row * lineHeight;

                // Highlight full row if selected
                if (selectedActionIndex == rowIdx && highlightServer) {
                    SDL_Rect hlRect = {actionStartX - 4, rowY - 1, 200, lineHeight};
                    { SDL_FRect fr = ToFRect(hlRect); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &fr); };
                }

                // Row label
                panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), rowLabels[row], 0);
                panelText.UpdatePosition({actionStartX, rowY});
                { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };

                // ALL cell (col 0) — show value if all players match, else "-"
                {
                    int cellX = actionStartX + labelW;
                    bool isFocusedAll = (selectedActionIndex == rowIdx && currentPlayerCol == 0);
                    // Column 0 is the "apply to everyone" cell.
                    AddPanelTapRow(rowIdx, {cellX - 2, rowY - 1, colW - 2, lineHeight}, 0);
                    if (isFocusedAll && isHost && highlightServer) {
                        SDL_Rect cellHl = {cellX - 2, rowY - 1, colW - 2, lineHeight};
                        { SDL_FRect fr = ToFRect(cellHl); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &fr); };
                    }
                    char cellText[8];
                    if (row == 0) {
                        bool same = true;
                        for (int i = 1; i < numPlayers; i++) if (playerColorCounts[i] != playerColorCounts[0]) { same = false; break; }
                        if (same) snprintf(cellText, sizeof(cellText), "%d", playerColorCounts[0]);
                        else snprintf(cellText, sizeof(cellText), "-");
                    } else if (row == 1) {
                        bool same = true;
                        for (int i = 1; i < numPlayers; i++) if (playerNoCompress[i] != playerNoCompress[0]) { same = false; break; }
                        if (same) snprintf(cellText, sizeof(cellText), "%s", playerNoCompress[0] ? "off" : "on");
                        else snprintf(cellText, sizeof(cellText), "-");
                    } else if (row == 2) {
                        bool same = true;
                        for (int i = 1; i < numPlayers; i++) if (playerAimGuide[i] != playerAimGuide[0]) { same = false; break; }
                        if (same) snprintf(cellText, sizeof(cellText), "%s", playerAimGuide[0] ? "on" : "off");
                        else snprintf(cellText, sizeof(cellText), "-");
                    } else {
                        bool same = true;
                        for (int i = 1; i < numPlayers; i++) if (netPlayerTeams[i] != netPlayerTeams[0]) { same = false; break; }
                        if (same) {
                            drawTeamSwatch(actionStartX + labelW, rowY, netPlayerTeams[0]);
                            snprintf(cellText, sizeof(cellText), "%d", netPlayerTeams[0]);
                        } else {
                            snprintf(cellText, sizeof(cellText), "-");
                        }
                    }
                    renderCentered(cellText, actionStartX + labelW, rowY);
                }

                // Per-player cells (P1..PN at col 1..N)
                for (int pi = 0; pi < numPlayers; pi++) {
                    int cellX = actionStartX + labelW + (pi + 1) * colW; // +1 to skip ALL column
                    bool isFocusedCell = (selectedActionIndex == rowIdx && currentPlayerCol == pi + 1);
                    AddPanelTapRow(rowIdx, {cellX - 2, rowY - 1, colW - 2, lineHeight}, pi + 1);

                    // Cell highlight: host all rows; joiner only their own column on Teams row
                    bool canHighlight = isHost || (row == 3 && pi == myJoinerSlot);
                    if (isFocusedCell && canHighlight && highlightServer) {
                        SDL_Rect cellHl = {cellX - 2, rowY - 1, colW - 2, lineHeight};
                        { SDL_FRect fr = ToFRect(cellHl); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &fr); };
                    }

                    // Cell value text
                    char cellText[8];
                    if (row == 0) {
                        snprintf(cellText, sizeof(cellText), "%d", playerColorCounts[pi]);
                    } else if (row == 1) {
                        snprintf(cellText, sizeof(cellText), "%s", playerNoCompress[pi] ? "off" : "on");
                    } else if (row == 2) {
                        snprintf(cellText, sizeof(cellText), "%s", playerAimGuide[pi] ? "on" : "off");
                    } else {
                        drawTeamSwatch(cellX, rowY, netPlayerTeams[pi]);
                        snprintf(cellText, sizeof(cellText), "%d", netPlayerTeams[pi]);
                    }
                    renderCentered(cellText, cellX, rowY);
                }
            }
        }

        // Player sidebar (game room) / online-player sidebar (lobby) — the
        // two are mutually exclusive, matching whether currentGame is set.
        if (currentGame) {
            // <=5-cap "fat-row" panel: always exactly 5 rows, empty slots
            // show "Waiting for player...". Team chips reuse kTeamColors so a
            // team reads the same color as the settings grid's own swatches.
            // >5-cap rooms get a compact 2-column roster instead (Team Mode is
            // capped at 5 players, so no team chips there).
            const bool bigRoom = currentGame->maxPlayers > 5;
            // Compact 2-column roster for >5-cap rooms occupies the whole gap
            // right of the settings grid (grid never passes x=350 at its
            // 5-column clamp); <=5-cap rooms keep the classic fat panel.
            const int panelX = bigRoom ? 354 : 450;
            const int panelY = 42;
            const int panelW = bigRoom ? 276 : 180;
            drawPanel({panelX, panelY, panelW, 286}, {18, 55, 65, menulist::kMapFillAlpha}, panelEdge);

            char hdr[32];
            if (bigRoom)
                snprintf(hdr, sizeof(hdr), "Players  %d/%d",
                         (int)currentGame->players.size(), currentGame->maxPlayers);
            else
                snprintf(hdr, sizeof(hdr), "Players  %d", (int)currentGame->players.size());
            drawLabel(hdr, panelX + 10, panelY + 8, textGold);

            if (bigRoom) {
                // Two columns of slim rows; slots split evenly (cap 20 -> 10+10,
                // cap 10 -> 5+5). No team chips: Team Mode is capped at 5 players.
                const int cap = currentGame->maxPlayers;
                const int rowsPerCol = (cap + 1) / 2;
                // 19px pitch: 10 rows span y +30..+217, clear of the legend at
                // +224 and the ESC line at +238.
                const int rowH2 = 19;
                const int colW2 = (panelW - 14) / 2;  // 131 at panelW=276
                for (int pi = 0; pi < cap; pi++) {
                    int col = pi / rowsPerCol;
                    int row = pi % rowsPerCol;
                    int rowX = panelX + 7 + col * (colW2 + 3);
                    int rowY = panelY + 30 + row * rowH2;
                    SDL_Rect rowBox = {rowX, rowY, colW2 - 3, rowH2 - 3};
                    SDL_SetRenderDrawColor(roomRenderer, 10, 38, 48, 185);
                    { SDL_FRect fr = ToFRect(rowBox); SDL_RenderFillRect(roomRenderer, &fr); }

                    if (pi < (int)currentGame->players.size()) {
                        const NetworkPlayer& pl = currentGame->players[pi];
                        bool host = (pl.nick == currentGame->creator);
                        bool self = (pl.nick == netClient->GetPlayerNick());
                        int ov = netTeamOverrides.count(pl.nick) ? netTeamOverrides[pl.nick] : 0;
                        int team = EffectiveTeam(pi, netTeamCount, ov);
                        if (netTeamMode && team >= 1 && team <= 5) {
                            SDL_Color chip = kTeamColors[team - 1];
                            SDL_SetRenderDrawColor(roomRenderer, chip.r, chip.g, chip.b, chip.a);
                            SDL_FRect chipRect = {(float)(rowX + 2), (float)(rowY + 3), 8.0f, 8.0f};
                            SDL_RenderFillRect(roomRenderer, &chipRect);
                        }
                        char rowTxt[64];
                        snprintf(rowTxt, sizeof(rowTxt), "%2d %.9s%s%s", pi + 1,
                                 pl.nick.c_str(), host ? " H" : "", self ? " *" : "");
                        drawLabel(rowTxt, rowX + (netTeamMode ? 14 : 4), rowY + 2, self ? textGold : textMain);
                    } else {
                        char rowTxt[24];
                        snprintf(rowTxt, sizeof(rowTxt), "%2d -", pi + 1);
                        drawLabel(rowTxt, rowX + 4, rowY + 2, textMuted);
                    }

                    if (netRosterEditMode && pi == netRosterCursor) {
                        SDL_SetRenderDrawColor(roomRenderer, 255, 255, 120, 255);
                        SDL_FRect hl = {(float)rowX, (float)rowY, (float)(colW2 - 3), (float)(rowH2 - 3)};
                        SDL_RenderRect(roomRenderer, &hl);
                    }
                }
                // Legend for the compact markers ("H" host, "*" you). The
                // [A] hint is appended here rather than on its own line --
                // there's no vertical room left above the chat dock (y=334).
                // While actually in roster-edit mode, swap the legend for the
                // in-mode key hint -- otherwise the Left/Right cycle keys and
                // the exit key are never shown anywhere on screen (found
                // live: user could enter the mode via [A] but had no way to
                // discover what to press next).
                if (netRosterEditMode) {
                    bool selfHost = currentGame->creator == netClient->GetPlayerNick();
                    drawLabel(selfHost ? "Up/Down move   Left/Right team   Enter/Esc done"
                                       : "Left/Right change your team   Enter/Esc done",
                              panelX + 12, panelY + 224, textMuted);
                } else {
                    drawLabel(netTeamMode ? "H host   * you   [A] assign teams" : "H host   * you",
                              panelX + 12, panelY + 224, textMuted);
                }
            } else {
                const int rowH = 38;
                for (int pi = 0; pi < 5; pi++) {
                    int rowY = panelY + 30 + pi * rowH;
                    SDL_Rect rowBox = {panelX + 7, rowY, panelW - 14, rowH - 5};
                    SDL_SetRenderDrawColor(roomRenderer, 10, 38, 48, 185);
                    { SDL_FRect fr = ToFRect(rowBox); SDL_RenderFillRect(roomRenderer, &fr); }

                    char slot[8];
                    snprintf(slot, sizeof(slot), "P%d", pi + 1);
                    drawLabel(slot, panelX + 12, rowY + 8, textMuted);
                    if (pi < (int)currentGame->players.size()) {
                        const NetworkPlayer& pl = currentGame->players[pi];
                        bool host = (pl.nick == currentGame->creator);
                        bool self = (pl.nick == netClient->GetPlayerNick());
                        int team = netPlayerTeams[pi];
                        if (team < 1 || team > 5) team = 1;
                        SDL_Color chip = teamColors[team - 1];
                        SDL_SetRenderDrawColor(roomRenderer, chip.r, chip.g, chip.b, chip.a);
                        SDL_FRect chipRect = {(float)(panelX + 40), (float)(rowY + 8), 12.0f, 12.0f};
                        SDL_RenderFillRect(roomRenderer, &chipRect);

                        char row[96];
                        snprintf(row, sizeof(row), "%.12s%s%s", pl.nick.c_str(),
                                 host ? "  HOST" : "", self ? "  YOU" : "");
                        drawLabel(row, panelX + 58, rowY + 6, self ? textGold : textMain);
                        char teamText[24];
                        snprintf(teamText, sizeof(teamText), "Team %d", team);
                        drawLabel(teamText, panelX + 58, rowY + 20, textMuted);
                    } else {
                        drawLabel("Waiting for player...", panelX + 40, rowY + 8, textMuted);
                    }
                }
            }
            drawLabel("ESC  Leave room", panelX + 12, panelY + 238, textMuted);

            // Bots, below the roster they will appear in. Host only: a bot is
            // an ordinary room member to everyone else. The band under the
            // ESC line is the only clear space left on this screen -- the
            // roster ends at panelY+257 and the panel at panelY+286.
            if (currentGame->creator == netClient->GetPlayerNick()) {
                const int botsY = panelY + 252;
                const int skillY = panelY + 270;
                const int rowW = panelW - 18;
                const int maxBots = MaxRoomBots((int)currentGame->players.size(),
                                                currentGame->maxPlayers, netRoomBotCount);
                char botsText[64];
                if (maxBots <= 0 && netRoomBotCount == 0) {
                    snprintf(botsText, sizeof(botsText), "Bots: 0  (room full)");
                } else {
                    snprintf(botsText, sizeof(botsText), "Bots: < %d >  of %d",
                             netRoomBotCount, maxBots);
                }
                char skillText[64];
                snprintf(skillText, sizeof(skillText), "Skill: < %s >",
                         LocalMPBotSkillName(netRoomBotSkill));

                SDL_Rect botsRect = {panelX + 8, botsY - 3, rowW, 18};
                if (selectedActionIndex == kRoomBots) drawSelection(botsRect);
                AddPanelTapRow(kRoomBots, botsRect);
                drawLabel(botsText, panelX + 12, botsY,
                          selectedActionIndex == kRoomBots ? textGold : textMain);

                SDL_Rect skillRect = {panelX + 8, skillY - 3, rowW, 18};
                if (selectedActionIndex == kRoomBotSkill) drawSelection(skillRect);
                AddPanelTapRow(kRoomBotSkill, skillRect);
                drawLabel(skillText, panelX + 12, skillY,
                          selectedActionIndex == kRoomBotSkill ? textGold : textMuted);
            }
        } else {
            // Lobby online-player sidebar: same DrawSidebarHeader frame as
            // every other sidebar (LocalMP's Players, the room's own Players
            // list below) instead of a hand-rolled panel a few pixels off
            // from kSidebarDocked -- green status dot + nickname per free
            // player, excluding self, capped by however many rows actually
            // fit above the chat dock.
            int sy = menulist::DrawSidebarHeader(roomRenderer, panelText, menulist::kSidebarDocked,
                                                  "Online", menulist::kMapFillAlpha);
            const SDL_Rect& sb = menulist::kSidebarDocked;
            std::vector<NetworkPlayer> openPlayers = netClient->GetOpenPlayers();
            int shown = 0;
            for (const NetworkPlayer& player : openPlayers) {
                if (player.nick == netClient->GetPlayerNick()) continue;
                if (sy + shown * 20 + 14 > sb.y + sb.h) break;
                SDL_SetRenderDrawColor(roomRenderer, 104, 220, 151, 255);
                SDL_FRect dot = {(float)(sb.x + 10), (float)(sy + shown * 20 + 4), 7.0f, 7.0f};
                SDL_RenderFillRect(roomRenderer, &dot);
                char shortNick[24];
                snprintf(shortNick, sizeof(shortNick), "%.18s", player.nick.c_str());
                drawLabel(shortNick, sb.x + 26, sy + shown * 20, textMain);
                shown++;
            }
            if (shown == 0) drawLabel("No free players", sb.x + 10, sy, textMuted);
        }
}

void MainMenu::NetPanelChatDockRender(bool expanded) {
    NetworkClient* netClient = NetworkClient::Instance();
    SDL_Renderer* roomRenderer = const_cast<SDL_Renderer*>(renderer);
    GameRoom* currentGame = netClient->GetCurrentGame();

    // Same drawing primitives as NetPanelLobbyActionsRender() — duplicated
    // here since C++ lambdas aren't shared across functions without extra
    // plumbing.
    auto drawPanel = [&](const SDL_Rect& rect, SDL_Color fill, SDL_Color outline) {
        SDL_SetRenderDrawBlendMode(roomRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(roomRenderer, fill.r, fill.g, fill.b, fill.a);
        SDL_FRect fr = ToFRect(rect);
        SDL_RenderFillRect(roomRenderer, &fr);
        SDL_SetRenderDrawColor(roomRenderer, outline.r, outline.g, outline.b, outline.a);
        SDL_RenderRect(roomRenderer, &fr);
    };
    auto drawLabel = [&](const char* text, int x, int y, SDL_Color color) {
        panelText.UpdateColor(color, {20, 12, 32, 255});
        panelText.UpdateText(roomRenderer, text, 0);
        panelText.UpdatePosition({x, y});
        SDL_FRect fr = ToFRect(*panelText.Coords());
        SDL_RenderTexture(roomRenderer, panelText.Texture(), nullptr, &fr);
    };
    auto drawSelection = [&](const SDL_Rect& rect) {
        SDL_SetRenderDrawColor(roomRenderer, 255, 196, 64, 72);
        SDL_FRect fr = ToFRect(rect);
        SDL_RenderFillRect(roomRenderer, &fr);
        SDL_SetRenderDrawColor(roomRenderer, 255, 218, 92, 240);
        SDL_RenderRect(roomRenderer, &fr);
    };

    const SDL_Color panelEdge = {255, 190, 46, 225};
    const SDL_Color textMain  = {248, 250, 239, 255};
    const SDL_Color textMuted = {174, 211, 202, 255};
    const SDL_Color textGold  = {255, 218, 92, 255};

    // Persistent chat dock: its own background panel, always-visible message
    // area, with the input row's focus box only shown while Chat is selected.
    // Grown upward while composing; the input row stays put at the bottom so
    // the caret does not move under the finger that just opened the keyboard.
    const int dockTop = expanded ? 60 : 334;
    const int dockHeight = expanded ? 412 : 138;
    drawPanel({10, dockTop, 620, dockHeight}, {29, 13, 43, 238}, panelEdge);
    drawLabel(expanded ? "CHAT  --  ENTER sends, ESC cancels" : "CHAT",
              20, dockTop + 6, textGold);
    // Action index 0 is Chat, whose row lives here rather than in the action
    // list (that loop skips i == 0). Registered after the list's rows, which is
    // safe: NetPanelLobbyActionsRender ran first and only it calls Begin.
    if (!expanded) AddPanelTapRow(0, {18, 438, 604, 26});
    const bool inputFocused = expanded || selectedActionIndex == 0;
    if (inputFocused) drawSelection({18, 438, 604, 26});
    char chatText[128];
    size_t inputLength = strlen(networkChatInput);
    const char* visibleInput = networkChatInput;
    bool clippedInput = inputLength > 70;
    if (clippedInput) visibleInput += inputLength - 70;
    snprintf(chatText, sizeof(chatText), "> %s%s%s", clippedInput ? "..." : "",
             visibleInput, inputFocused ? "_" : "");
    drawLabel(chatText, 26, 444, textMain);

    // Display chat messages in the dock's message area.
    const int chatStatusX = 22;
    const int chatStatusY = 426;
    const int chatLineHeight = 16;
    // Bottom line stays at chatStatusY either way, so the log grows upward into
    // the space the expanded dock just claimed. The cap keeps the topmost line
    // clear of the dock's own header.
    const int maxChatLines = expanded ? ((chatStatusY - (dockTop + 24)) / chatLineHeight) + 1 : 5;
    std::vector<ChatMessage> chatMsgs = netClient->GetChatMessages();

    // >5-cap rooms: every client (not just the host) applies !team:<nick>:<n>
    // TALK broadcasts directly to its own nick->override map. SETOPTIONS
    // can't carry teams for P6-20, so the nick-keyed channel is the sync
    // mechanism there. (<=5-cap rooms keep the host-intercept path below.)
    if (currentGame && currentGame->maxPlayers > 5) {
        std::vector<ChatMessage> allMsgs = netClient->GetChatMessages();
        if (allMsgs.size() < teamOverrideChatCount) teamOverrideChatCount = 0;
        for (size_t mi = teamOverrideChatCount; mi < allMsgs.size(); mi++) {
            const std::string& msg = allMsgs[mi].message;
            if (msg.size() > 6 && msg.substr(0, 6) == "!team:") {
                size_t sep = msg.find(':', 6);
                if (sep == std::string::npos) continue;
                std::string senderNick = msg.substr(6, sep - 6);
                int newTeam = std::atoi(msg.c_str() + sep + 1);
                if (!senderNick.empty() && newTeam >= 1 && newTeam <= 5)
                    netTeamOverrides[senderNick] = newTeam;
            }
        }
        teamOverrideChatCount = allMsgs.size();
    }

    // Host: intercept !team:N commands sent by joiners and re-broadcast SETOPTIONS.
    // Visual chrome around this dock changed; this logic itself is untouched.
    if (currentGame && currentGame->creator == netClient->GetPlayerNick()) {
        if (chatMsgs.size() < lastProcessedChatCount) lastProcessedChatCount = 0;
        for (size_t mi = lastProcessedChatCount; mi < chatMsgs.size(); mi++) {
            const std::string& msg = chatMsgs[mi].message;
            if (msg.size() > 6 && msg.substr(0, 6) == "!team:") {
                // Format: !team:<nick>:<team>
                size_t sep = msg.find(':', 6);
                if (sep == std::string::npos) continue;
                std::string senderNick = msg.substr(6, sep - 6);
                int newTeam = std::atoi(msg.c_str() + sep + 1);
                if (newTeam >= 1 && newTeam <= 5 && !senderNick.empty()) {
                    for (int i = 0; i < (int)currentGame->players.size(); i++) {
                        if (currentGame->players[i].nick == senderNick) {
                            netPlayerTeams[i] = newTeam;
                            static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                            netClient->SendOptions(chainReactionEnabled, /*continueWhenLeave=*/true,
                                singlePlayerTargetting, vLimits[victoriesLimitIndex], playerColorCounts,
                                playerNoCompress, playerAimGuide, netRoomMouseEnabled, netClearMode,
                                netDisableMalus, netTeamMode, netPlayerTeams, netTeamCount);
                            break;
                        }
                    }
                }
            }
        }
        lastProcessedChatCount = chatMsgs.size();
    }

    std::vector<size_t> visible = VisibleChatIndices(chatMsgs);

    // Display the most recent messages from bottom up.
    int chatLine = 0;
    size_t startIdx = visible.size() > (size_t)maxChatLines
                      ? visible.size() - (size_t)maxChatLines : 0;
    for (size_t vi = startIdx; vi < visible.size() && chatLine < maxChatLines; vi++) {
        const ChatMessage& cm = chatMsgs[visible[vi]];
        char chatLineText[256];
        // Server messages start with ***, regular messages show <nick>
        if (cm.nick == "Server" || cm.message.find("***") == 0) {
            snprintf(chatLineText, sizeof(chatLineText), "*** %.72s", cm.message.c_str());
        } else {
            snprintf(chatLineText, sizeof(chatLineText), "<%.16s> %.56s",
                cm.nick.c_str(), cm.message.c_str());
        }

        int yPos = chatStatusY - (maxChatLines - 1 - chatLine) * chatLineHeight;
        drawLabel(chatLineText, chatStatusX, yPos,
                  cm.nick == "Server" ? textMuted : textMain);
        chatLine++;
    }
}

void MainMenu::ToggleFollowServer(const ServerInfo& server) {
    GameSettings* gs = GameSettings::Instance();
    NetworkClient* netClient = NetworkClient::Instance();

    const bool wasFollowed = gs->IsServerFollowed(server.host, server.port);
    const bool nowFollowed =
        gs->ToggleServerFollowed(server.host, server.port, server.name);

    if (!wasFollowed && !nowFollowed) {
        // Refused: the list is full. Say so rather than letting the star
        // silently fail to light up.
        connectErrorMsg = "Already following " +
                          std::to_string(GameSettings::kMaxFollowedServers) +
                          " servers -- unfollow one first";
        PlayMenuSFX("menu_change");
        return;
    }

    gs->SaveKeys();

    // Tell the server, but only if this is the server we are actually talking
    // to -- there is no way to reach any other one from here, and the
    // registration is re-sent on connect anyway.
    const char* platform = PushPlatformName();
    const std::string token = PushDeviceToken();
    if (platform != nullptr && !token.empty() && netClient->IsConnected() &&
        netClient->GetHost() == server.host && netClient->GetPort() == server.port) {
        if (nowFollowed) netClient->SendNotifyReg(platform, token.c_str());
        else             netClient->SendNotifyUnreg(token.c_str());
    }

    PlayMenuSFX("menu_selected");
}

void MainMenu::ToggleFollowCurrentServer() {
    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient->IsConnected()) return;

    switch (netClient->GetNotifySupport()) {
        case NotifySupport::Unsupported:
            connectErrorMsg = "This server doesn't support follow notifications";
            PlayMenuSFX("menu_change");
            return;
        case NotifySupport::Unknown:
            // Still probing -- act on nothing rather than guess.
            PlayMenuSFX("menu_change");
            return;
        case NotifySupport::Supported:
            break;
    }

    ServerInfo server;
    server.host = netClient->GetHost();
    server.port = netClient->GetPort();
    server.name = netClient->GetHost();  // no separate display name once connected
    ToggleFollowServer(server);
}

void MainMenu::RefreshFollowRegistration() {
    const char* platform = PushPlatformName();
    if (platform == nullptr) return;                 // no push story on this build

    const std::string token = PushDeviceToken();
    if (token.empty()) return;                       // nothing to register yet

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient->IsConnected()) return;

    if (!GameSettings::Instance()->IsServerFollowed(netClient->GetHost(),
                                                    netClient->GetPort()))
        return;

    netClient->SendNotifyReg(platform, token.c_str());
}

void MainMenu::NetPanelConnectionScreensRender() {
    NetworkClient* netClient = NetworkClient::Instance();

    // The two server-list screens are full-screen menulist panels, drawn
    // before the small wood popup below is ever touched -- everything past
    // this point still uses that popup, so the two are kept firmly separate.
    if (!networkInLobby && networkInputMode == 7) {
        ServerListPanelRender(/*isLAN=*/true);
        return;
    }
    if (!networkInLobby && networkInputMode == 10) {
        ServerListPanelRender(/*isLAN=*/false);
        return;
    }

    // For non-lobby screens, use void panel
    { SDL_FRect fr = ToFRect(voidPanelRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &fr); };

    char netText[512];

    if (!networkInLobby && networkInputMode == 11) {
        // Pre-lobby nickname input screen
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color black  = {0, 0, 0, 255};
        SDL_Color yellow = {255, 220, 50, 255};

        auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
            panelText.UpdateColor(fg, black);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
            y += panelText.Coords()->h;
        };

        int y = (480/2) - 120;
        char lineBuf[128];

        renderLine("Set Your Name\n", white, y);
        snprintf(lineBuf, sizeof(lineBuf), "[ %s_ ]", networkPreNick);
        renderLine(lineBuf, yellow, y);
        renderLine("\nPress ENTER to confirm\nPress ESC to cancel", white, y);
        return;
    }

    if (!networkInLobby) {
        // Connection screen — render in segments so active field can be colored
        // Host / Port / Connect are one selection axis, same as the lists above.
        BeginPanelTapRows(&networkManualFieldIndex);
        const char* titleStr = serverHosting ? "Hosting Server" : "Join Server";
        bool hostActive = networkManualFieldIndex == 0;
        bool portActive = networkManualFieldIndex == 1;
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color black  = {0, 0, 0, 255};
        SDL_Color yellow = {255, 220, 50, 255};

        auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
            panelText.UpdateColor(fg, black);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
            y += panelText.Coords()->h;
        };

        int y = (480/2) - 120;
        char lineBuf[280];

        snprintf(lineBuf, sizeof(lineBuf), "%s\n\n", titleStr);
        renderLine(lineBuf, white, y);

        bool connectActive = (!networkFieldEditing && networkManualFieldIndex == 2);

        auto renderMenuRow = [&](int index, const char* txt, SDL_Color fg, int& y) {
            int top = y;
            renderLine(txt, fg, y);
            AddPanelTapRow(index, { voidPanelRct.x, top, voidPanelRct.w, y - top });
        };

        snprintf(lineBuf, sizeof(lineBuf), hostActive && networkFieldEditing ? "Host: [ %s_ ]" : "Host:   %s  ", networkHost);
        renderMenuRow(0, lineBuf, hostActive ? yellow : white, y);

        snprintf(lineBuf, sizeof(lineBuf), portActive && networkFieldEditing ? "Port: [ %d_ ]" : "Port:   %d  ", networkPort);
        renderMenuRow(1, lineBuf, portActive ? yellow : white, y);

        snprintf(lineBuf, sizeof(lineBuf), connectActive ? "[ Connect ]" : "  Connect  ");
        renderMenuRow(2, lineBuf, connectActive ? yellow : white, y);

        if (networkFieldEditing) {
            snprintf(lineBuf, sizeof(lineBuf), "\n%sENTER to confirm  ESC to cancel",
                serverHosting ? "Server running...\n\n" : "\n");
        } else {
            snprintf(lineBuf, sizeof(lineBuf), "\n%sUP/DOWN to navigate  ENTER to select\nESC to go back",
                serverHosting ? "Server running...\n\n" : "\n");
        }
        renderLine(lineBuf, white, y);

        if (!connectErrorMsg.empty()) {
            SDL_Color red = {255, 80, 80, 255};
            snprintf(lineBuf, sizeof(lineBuf), "\n%s", connectErrorMsg.c_str());
            renderLine(lineBuf, red, y);
        }

        return;
    } else if (networkInputMode == 3) {
        // Join game input screen
        snprintf(netText, sizeof(netText),
            "Join Game\n\n"
            "Enter the creator's nickname:\n\n"
            "%s_\n\n"
            "Press ENTER to join\n"
            "Press ESC to cancel",
            networkJoinCreator);
    } else if (networkInputMode == 4) {
        // Chat input screen
        snprintf(netText, sizeof(netText),
            "Send Chat Message\n\n"
            "%s_\n\n"
            "Press ENTER to send\n"
            "Press ESC to cancel",
            networkChatInput);
    } else if (networkInputMode == 5) {
        // Username input screen
        snprintf(netText, sizeof(netText),
            "Change Username\n\n\n"
            "Enter your nickname:\n\n"
            "%s_\n\n"
            "Current: %s\n\n"
            "Press ENTER to change\n"
            "Press ESC to cancel",
            networkUsername,
            netClient->GetPlayerNick().c_str());
    } else if (networkInputMode == 6) {
        // Create game confirmation screen
        snprintf(netText, sizeof(netText),
            "Create New Game\n\n\n"
            "Create a new game room?\n\n"
            "Game Name: %s's game\n"
            "Max Players: 5\n\n\n"
            "Press ENTER to create\n"
            "Press ESC to cancel",
            netClient->GetPlayerNick().c_str());
    } else {
        // Main lobby screen with game list
        const char* stateStr = "Disconnected";
        if (netClient->IsConnected()) {
            // Request LIST periodically (every 2 seconds)
            Uint32 now = SDL_GetTicks();
            Uint32 timeSinceLastRequest = now - lastListRequest;
            if (timeSinceLastRequest > 500) {
                netClient->RequestList();
                lastListRequest = now;
            }

            switch (netClient->GetState()) {
                case CONNECTED: stateStr = "Lobby"; break;
                case IN_LOBBY: stateStr = "In Game"; break;
                case IN_GAME: stateStr = "Playing"; break;
                default: stateStr = "Connected"; break;
            }
        } else {
        }

        // Build lobby display
        char lobbyText[2048];
        int offset = 0;

        // Title and status
        offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
            "=== Frozen Bubble Network Lobby ===\n\n"
            "Player: %s  |  Status: %s%s\n\n",
            netClient->GetPlayerNick().c_str(),
            stateStr,
            serverHosting ? "  |  Hosting" : "");

        // Get game list for later use
        std::vector<GameRoom> games = netClient->GetGameList();
        std::vector<NetworkPlayer> openPlayers = netClient->GetOpenPlayers();

        // Show current game if in one
        GameRoom* currentGame = netClient->GetCurrentGame();
        if (currentGame) {
            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "Current Game: %s's game\n",
                currentGame->creator.c_str());

            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "Players: ");
            for (size_t i = 0; i < currentGame->players.size(); i++) {
                offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                    "%s%s",
                    currentGame->players[i].nick.c_str(),
                    i < currentGame->players.size() - 1 ? ", " : "");
            }
            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset, "\n\n");
        } else {
            // Show available games
            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "Available Games (%d):\n", (int)games.size());

            if (games.empty()) {
                offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                    "  (No games available)\n");
            } else {
                for (size_t i = 0; i < games.size() && i < 5; i++) {
                    // Build player names string
                    std::string playerNames;
                    for (size_t j = 0; j < games[i].players.size(); j++) {
                        if (j > 0) playerNames += ", ";
                        playerNames += games[i].players[j].nick;
                    }
                    offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                        "  %s [%s]: %s\n",
                        i == (size_t)selectedGameIndex ? ">" : " ",
                        games[i].creator.c_str(),
                        playerNames.c_str());
                }
            }

            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "\nIn Lobby (%d):\n", (int)openPlayers.size());

            if (openPlayers.empty()) {
                offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                    "  (No players)\n");
            } else {
                for (size_t i = 0; i < openPlayers.size() && i < 8; i++) {
                    offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                        "  %s\n", openPlayers[i].nick.c_str());
                }
            }
        }

        // Recent chat messages
        std::vector<ChatMessage> chatMsgs = netClient->GetChatMessages();
        offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset, "\nChat:\n");

        {
            std::vector<size_t> visible = VisibleChatIndices(chatMsgs);
            size_t chatStart = visible.size() > 3 ? visible.size() - 3 : 0;
            for (size_t vi = chatStart; vi < visible.size(); vi++) {
                offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                    "<%s> %s\n",
                    chatMsgs[visible[vi]].nick.c_str(),
                    chatMsgs[visible[vi]].message.c_str());
            }
        }

        offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
            "\n%sC=Create J=Join%s T=Chat%s",
            !games.empty() && !currentGame ? "UP/DOWN=Select  " : "",
            currentGame ? " S=Start P=Part" : "",
            currentGame ? "" : "\nESC=Disconnect  /nick to rename");

        strncpy(netText, lobbyText, sizeof(netText) - 1);
        netText[sizeof(netText) - 1] = '\0';
    }

    panelText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
    panelText.UpdateText(const_cast<SDL_Renderer *>(renderer), netText, 0);
    panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), (480/2) - 120});
    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
}

void MainMenu::ServerListPanelRender(bool isLAN) {
    SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
    int& menuIndex = isLAN ? lanMenuIndex : netMenuIndex;
    std::vector<ServerInfo>& servers = isLAN ? discoveredServers : publicServers;

#ifndef __WASM_PORT__
    if (!isLAN) {
        // Poll background server fetch result -- unchanged from before this
        // rewrite, just relocated with the rest of the Net-list logic.
        if (!serverFetchInProgress.load() && publicServers.empty()) {
            std::lock_guard<std::mutex> lock(serverFetchMutex);
            publicServers = std::move(serverFetchResult);
            serverFetchResult.clear();
        }
    }
#endif

    BeginPanelTapRows(&menuIndex);
    auto tap = [&](int index, const SDL_Rect& rect, int subIndex, bool splitAdjust, SDL_Keycode key) {
        AddPanelTapRow(index, rect, subIndex, splitAdjust, key);
    };

    menulist::DrawHeaderBar(rend, panelText, menulist::kHeaderBar,
        isLAN ? "LAN GAME" : "NET GAME", nullptr, false, -1, tap);

    menulist::List list(menulist::kListFull, menuIndex);
    list.Header(isLAN ? "Local network" : "Public servers");

    // Row 0: LAN hosts a server here; Net opens the manual-entry form. Both
    // reuse the same slot so the rows below never renumber between screens.
    if (isLAN) {
        list.Row(0, serverHosting ? "Server running (rescan)" : "Host a server", "");
    } else {
        list.Row(0, "Manual entry...", "");
    }

    if (!isLAN && serverFetchInProgress.load()) {
        list.Row(-1, "Fetching server list...", "");
    } else if (servers.empty()) {
        list.Row(-1, isLAN ? "No servers found" : "No public servers listed", "");
        if (isLAN) list.Row(-1, "Start one: fb-server -l", "");
    } else {
        for (int i = 0; i < (int)servers.size(); i++) {
            const ServerInfo& s = servers[i];
            bool offline = (s.latencyMs < 0);
            const std::string& name = s.name.empty()
                ? s.host + ":" + std::to_string(s.port) : s.name;
            std::string latency = offline ? "offline" : std::to_string(s.latencyMs) + " ms";
            bool followed = GameSettings::Instance()->IsServerFollowed(s.host, s.port);
            // Offline servers stay tappable: selecting one is how the player
            // reads its address, and keyboard/gamepad nav can land on them too.
            list.RowWithPrefix(i + 1, followed ? "★" : "☆",
                followed ? menulist::kGold : menulist::kMuted,
                name, latency, !offline, SDLK_F);
        }
    }

    // Last row: Set Name.
    int lastIdx = 1 + (int)servers.size();
    const char* curNick = networkPreNick[0] != '\0' ? networkPreNick
#ifdef __ANDROID__
        : (getenv("USER") ? getenv("USER") : "android_user");
#else
        : (getenv("USER") ? getenv("USER") : "unnamed");
#endif
    list.Row(lastIdx, "Set name", curNick, true, true);

    list.End(rend, panelText, nullptr, tap);

    // Sidebar: details for whichever server row is currently selected, plus
    // connection status -- everything that used to be squeezed onto extra
    // lines below the list (offline/error text, "Connecting...") now has a
    // fixed home instead of pushing the row list itself around.
    int sy = menulist::DrawSidebarHeader(rend, panelText, menulist::kSidebarFull, "Selected");
    const SDL_Rect& sb = menulist::kSidebarFull;
    auto sidebarLine = [&](const std::string& txt, SDL_Color color, int size = 15) {
        panelText.UpdateStyle(size, TTF_STYLE_NORMAL);
        panelText.UpdateColor(color, menulist::kTextShadow);
        panelText.UpdateText(rend, txt.c_str(), 0);
        panelText.UpdatePosition({sb.x + 12, sy});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
        sy += size + 10;
    };

    if (menuIndex >= 1 && menuIndex <= (int)servers.size()) {
        const ServerInfo& s = servers[menuIndex - 1];
        bool offline = (s.latencyMs < 0);
        bool followed = GameSettings::Instance()->IsServerFollowed(s.host, s.port);
        sidebarLine(s.name.empty() ? s.host : s.name, menulist::kText, 16);
        sidebarLine(s.host + "  :" + std::to_string(s.port), menulist::kMuted);
        sidebarLine(offline ? "offline" : std::to_string(s.latencyMs) + " ms",
                    offline ? menulist::kBad : menulist::kMuted);
        sy += 10;
        sidebarLine(followed ? "★ Following" : "☆ Not followed",
                    followed ? menulist::kGold : menulist::kMuted);
        sidebarLine("notified when someone joins", menulist::kMuted, 13);
    } else if (!isLAN && servers.empty() && !serverFetchInProgress.load()) {
        sidebarLine("No servers to show yet.", menulist::kMuted);
        sidebarLine("Press R to refresh.", menulist::kMuted);
    } else {
        sidebarLine(isLAN ? "Pick a server, or host" : "Pick a server, or enter",
                    menulist::kMuted);
        sidebarLine(isLAN ? "one from this list." : "one manually.", menulist::kMuted);
    }

    sy = sb.y + sb.h - 70;
    if (!isLAN && pendingLobbyConnect) {
        sidebarLine("Connecting...", menulist::kGold);
    } else if (!connectErrorMsg.empty()) {
        sidebarLine(connectErrorMsg, menulist::kBad, 13);
    }

    menulist::DrawFooterHint(rend, panelText, isLAN
        ? "UP/DOWN select    ENTER connect    R rescan    F follow    ESC cancel"
        : "UP/DOWN select    ENTER connect    R refresh    F follow    ESC cancel");
}
