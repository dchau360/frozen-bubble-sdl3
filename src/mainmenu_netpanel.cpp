/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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
#include "audiomixer.h"
#include "frozenbubble.h"
#include "transitionmanager.h"
#include "networkclient.h"
#include "platform.h"

#include <SDL3_image/SDL_image.h>
#include <cstring>
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
        netClient->Update();

        // Apply any options broadcast by the host (joiners receive SETOPTIONS push)
        {
            bool cr, cl, st; int vl; int pc[5]; bool nc[5]; bool ag[5]; bool me;
            if (netClient->GetAndClearPendingOptions(cr, cl, st, vl, pc, nc, ag, me)) {
                chainReactionEnabled = cr;
                continueWhenPlayersLeave = cl;
                singlePlayerTargetting = st;
                // Map vl to victoriesLimitIndex
                static const int vLimits[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,15,20,30,50,100};
                victoriesLimitIndex = 5; // default
                for (int i = 0; i < 18; i++) { if (vLimits[i] == vl) { victoriesLimitIndex = i; break; } }
                for (int i = 0; i < 5; i++) { playerColorCounts[i] = pc[i]; playerNoCompress[i] = nc[i]; playerAimGuide[i] = ag[i]; }
                netRoomMouseEnabled = me;
                SDL_Log("Applied host options: cr=%d cl=%d st=%d vl=%d colors=%d,%d,%d,%d,%d mouse=%d",
                    cr,cl,st,vl,pc[0],pc[1],pc[2],pc[3],pc[4],me);
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
        NetPanelChatStatusRender();
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

        // Render action list at top left (like original)
        const int actionStartY = 30;
        const int actionStartX = 78;
        const int lineHeight = 16;

        // Action menu: Different options depending on whether in a game
        std::vector<std::string> actions;
        GameRoom* currentGame = netClient->GetCurrentGame();

        if (currentGame) {
            // In a game room - show game options
            actions.push_back("Chat");  // index 0

            // Global settings (indices 1-4) - same for host and joiner
            char crText[64], continueText[64], targetText[64], victoriesText[64];
            snprintf(crText, sizeof(crText), "Chain-reaction: %s", chainReactionEnabled ? "enabled" : "disabled");
            snprintf(continueText, sizeof(continueText), "Continue when players leave: %s", continueWhenPlayersLeave ? "enabled" : "disabled");
            snprintf(targetText, sizeof(targetText), "Single player targetting: %s", singlePlayerTargetting ? "enabled" : "disabled");
            const char* victoriesLimits[] = {"none (unlimited)", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "15", "20", "30", "50", "100"};
            snprintf(victoriesText, sizeof(victoriesText), "Victories limit: %s", victoriesLimits[victoriesLimitIndex]);
            actions.push_back(crText);       // index 1
            actions.push_back(continueText); // index 2
            actions.push_back(targetText);   // index 3
            actions.push_back(victoriesText);// index 4

            // Mouse/touch aim (index 5) — per-session local setting, defaults OFF
            {
                char mouseText[64];
                snprintf(mouseText, sizeof(mouseText), "Mouse/Touch aim: %s", netRoomMouseEnabled ? "ON" : "OFF");
                actions.push_back(mouseText); // index 5
            }

            // Per-player grid rows (indices 6-8) — label only; values rendered as grid cells below
            actions.push_back("Max colors:"); // index 6
            actions.push_back("Rows:");      // index 7
            actions.push_back("Aim:");       // index 8

            // Start game (index 9) — host only when >1 player
            if (currentGame->creator == netClient->GetPlayerNick() && currentGame->players.size() > 1) {
                actions.push_back("Start game!"); // index 9
            }
            // No "Part game" menu item - use ESC key to leave like original
        } else {
            // In lobby - show create/join options
            actions.push_back("Chat");
            actions.push_back("Create new game");

            std::vector<GameRoom> games = netClient->GetGameList();
            for (const auto& game : games) {
                std::string playerList;
                for (size_t i = 0; i < game.players.size(); i++) {
                    if (i > 0) playerList += ", ";
                    playerList += game.players[i].nick;
                }
                actions.push_back("Join " + game.creator + "'s game: " + playerList);
            }
        }

        // Grid offset: player header + grid rows are shifted down one line to make room for nick header
        const int gridStart = 6;       // First grid row index
        const int gridYOffset = lineHeight * 2; // Gap after Victories limit + space for nick header

        // Render actions with highlight
        for (size_t i = 0; i < actions.size() && i < 15; i++) {
            int renderY = actionStartY + (int)(i * lineHeight) + ((int)i >= gridStart ? gridYOffset : 0);

            if (i == (size_t)selectedActionIndex && highlightServer) {
                SDL_Rect highlightRect = {actionStartX - 4, renderY - 1, 200, lineHeight};
                { SDL_FRect fr = ToFRect(highlightRect); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &fr); };
            }

            // For grid rows (5-7 in a game room), skip the label text here — rendered as table below
            if (currentGame && (int)i >= gridStart && (int)i <= gridStart + 2) {
                continue;
            }

            char actionText[128];
            snprintf(actionText, sizeof(actionText), "%s", actions[i].c_str());
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), actionText, 0);
            panelText.UpdatePosition({actionStartX, renderY});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
        }

        // Render the per-player settings grid (only in a game room)
        if (currentGame) {
            int numPlayers = (int)currentGame->players.size();
            if (numPlayers < 1) numPlayers = 1;
            if (numPlayers > 5) numPlayers = 5;
            bool isHost = currentGame->creator == netClient->GetPlayerNick();

            // Column layout
            const int labelW = 110;  // Width of row label ("Max colors:", "Row collapse:", "Aim guide:")
            const int colW   = 36;   // Width of each player column

            // Header row sits exactly one lineHeight above the first data row
            int firstDataRowY = actionStartY + gridStart * lineHeight + gridYOffset;
            int headerY = firstDataRowY - lineHeight;

            // Draw grid lines
            {
                SDL_Renderer* rend = const_cast<SDL_Renderer*>(renderer);
                SDL_SetRenderDrawColor(rend, 180, 180, 200, 220);

                int totalCols = numPlayers + 1; // ALL + P1..PN
                int gridLeft  = actionStartX - 2;
                int gridRight = actionStartX + labelW + totalCols * colW;
                int gridTop   = headerY - 1;
                int gridBot   = firstDataRowY + 3 * lineHeight; // 3 data rows

                // Outer border
                SDL_Rect border = {gridLeft, gridTop, gridRight - gridLeft, gridBot - gridTop};
                { SDL_FRect fr = ToFRect(border); SDL_RenderRect(rend, &fr); }

                // Horizontal line after header row
                SDL_RenderLine(rend, (float)gridLeft, (float)(firstDataRowY - 1), (float)gridRight, (float)(firstDataRowY - 1));
                // Horizontal lines between data rows (after Colors, after Row collapse)
                for (int r = 1; r <= 2; r++) {
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

            // Grid rows: Colors (5), Rows (6), Aim (7)
            const char* rowLabels[] = {"Max colors:", "Row collapse:", "Aim guide:"};
            for (int row = 0; row < 3; row++) {
                int rowIdx = gridStart + row;
                int rowY   = actionStartY + rowIdx * lineHeight + gridYOffset;

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
                    } else {
                        bool same = true;
                        for (int i = 1; i < numPlayers; i++) if (playerAimGuide[i] != playerAimGuide[0]) { same = false; break; }
                        if (same) snprintf(cellText, sizeof(cellText), "%s", playerAimGuide[0] ? "on" : "off");
                        else snprintf(cellText, sizeof(cellText), "-");
                    }
                    renderCentered(cellText, actionStartX + labelW, rowY);
                }

                // Per-player cells (P1..PN at col 1..N)
                for (int pi = 0; pi < numPlayers; pi++) {
                    int cellX = actionStartX + labelW + (pi + 1) * colW; // +1 to skip ALL column
                    bool isFocusedCell = (selectedActionIndex == rowIdx && currentPlayerCol == pi + 1);

                    // Cell highlight for focused cell (host only)
                    if (isFocusedCell && isHost && highlightServer) {
                        SDL_Rect cellHl = {cellX - 2, rowY - 1, colW - 2, lineHeight};
                        { SDL_FRect fr = ToFRect(cellHl); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), highlightServer, nullptr, &fr); };
                    }

                    // Cell value text
                    char cellText[8];
                    if (row == 0) {
                        snprintf(cellText, sizeof(cellText), "%d", playerColorCounts[pi]);
                    } else if (row == 1) {
                        snprintf(cellText, sizeof(cellText), "%s", playerNoCompress[pi] ? "off" : "on");
                    } else {
                        snprintf(cellText, sizeof(cellText), "%s", playerAimGuide[pi] ? "on" : "off");
                    }
                    renderCentered(cellText, cellX, rowY);
                }
            }
        }
}

void MainMenu::NetPanelChatStatusRender() {
    NetworkClient* netClient = NetworkClient::Instance();
    GameRoom* currentGame = netClient->GetCurrentGame();
    const int actionStartX = 78;

        // If Chat is selected, show inline text input (like original at y=320)
        const int chatY = 320;
        if (selectedActionIndex == 0) {
            char chatText[512];
            snprintf(chatText, sizeof(chatText), "Say: %s_", networkChatInput);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), chatText, 0);
            panelText.UpdatePosition({actionStartX, chatY});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
        }

        // Display chat messages in status area (like original at y=355-435)
        const int chatStatusX = 10; // Status messages use x=10 in original
        const int chatStatusY = 435; // Bottom of chat area
        const int chatLineHeight = 16;
        const int maxChatLines = 5;
        std::vector<ChatMessage> chatMsgs = netClient->GetChatMessages();

        // Display last 5 messages from bottom up
        int chatLine = 0;
        int startIdx = chatMsgs.size() > maxChatLines ? chatMsgs.size() - maxChatLines : 0;
        for (size_t i = startIdx; i < chatMsgs.size() && chatLine < maxChatLines; i++, chatLine++) {
            char chatLineText[256];
            // Server messages start with ***, regular messages show <nick>
            if (chatMsgs[i].nick == "Server" || chatMsgs[i].message.find("***") == 0) {
                snprintf(chatLineText, sizeof(chatLineText), "*** %s", chatMsgs[i].message.c_str());
            } else {
                snprintf(chatLineText, sizeof(chatLineText), "<%s> %s",
                    chatMsgs[i].nick.c_str(), chatMsgs[i].message.c_str());
            }

            int yPos = chatStatusY - (maxChatLines - 1 - chatLine) * chatLineHeight;
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), chatLineText, 0);
            panelText.UpdatePosition({chatStatusX, yPos});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
        }

        // Show player nickname and count at y=336 like original (below chat input at y=320)
        const int statusY = 336;
        const int statusX = 78;
        char statusText[256];

        // Show own nickname first
        snprintf(statusText, sizeof(statusText), "Player: %s", netClient->GetPlayerNick().c_str());
        panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), statusText, 0);
        panelText.UpdatePosition({statusX, statusY});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };

        // Show available players or game info below
        char playersText[256];
        if (currentGame) {
            // Build comma-separated player list for current game room
            std::string playerNames;
            for (size_t i = 0; i < currentGame->players.size(); i++) {
                if (i > 0) playerNames += ", ";
                playerNames += currentGame->players[i].nick;
            }
            snprintf(playersText, sizeof(playersText), "Players (%d): %s",
                (int)currentGame->players.size(), playerNames.c_str());
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), playersText, 0);
            panelText.UpdatePosition({statusX, statusY + 16});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
        } else {
            // Show lobby player list — header line then names, up to 3 per line
            std::vector<NetworkPlayer> openPlayers = netClient->GetOpenPlayers();
            int total = (int)openPlayers.size();
            snprintf(playersText, sizeof(playersText), "Online (%d):", total);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), playersText, 0);
            panelText.UpdatePosition({statusX, statusY + 16});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };

            // Build name list, up to 9 names (3 lines × 3), then "+N more"
            const int maxShown = 9;
            int lineY = statusY + 32;
            int shown = 0;
            std::string lineBuf;
            for (int pi = 0; pi < total && shown < maxShown; pi++) {
                const std::string& nick = openPlayers[pi].nick;
                if (nick == netClient->GetPlayerNick()) continue; // skip self
                if (!lineBuf.empty()) lineBuf += "  ";
                lineBuf += nick;
                shown++;
                // Flush line every 3 names
                if (shown % 3 == 0 || pi == total - 1) {
                    panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), lineBuf.c_str(), 0);
                    panelText.UpdatePosition({statusX, lineY});
                    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
                    lineY += 16;
                    lineBuf.clear();
                }
            }
            // Flush any remaining partial line
            if (!lineBuf.empty()) {
                panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), lineBuf.c_str(), 0);
                panelText.UpdatePosition({statusX, lineY});
                { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
                lineY += 16;
            }
            // Show overflow count if more players than we displayed
            int remaining = total - shown - 1; // -1 for self
            if (remaining > 0) {
                snprintf(playersText, sizeof(playersText), "+%d more", remaining);
                panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), playersText, 0);
                panelText.UpdatePosition({statusX, lineY});
                { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
            }
        }
}

void MainMenu::NetPanelConnectionScreensRender() {
    NetworkClient* netClient = NetworkClient::Instance();

    // For non-lobby screens, use void panel
    { SDL_FRect fr = ToFRect(voidPanelRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), voidPanelBG, nullptr, &fr); };

    char netText[512];

    if (!networkInLobby && networkInputMode == 7) {
        // LAN server list screen
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color black  = {0, 0, 0, 255};
        SDL_Color yellow = {255, 220, 50, 255};
        SDL_Color red    = {255, 80, 80, 255};

        auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
            panelText.UpdateColor(fg, black);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
            y += panelText.Coords()->h;
        };

        int y = (480/2) - 120;
        char lineBuf[280];

        renderLine("LAN Game\n", white, y);

        // Menu item 0: Host a server
        bool hostSel = (lanMenuIndex == 0);
        snprintf(lineBuf, sizeof(lineBuf),
            hostSel ? "[ %s ]" : "  %s  ",
            serverHosting ? "Server running (rescan)" : "Host a server");
        renderLine(lineBuf, hostSel ? yellow : white, y);

        // Menu items 1+: discovered servers (includes 127.0.0.1 if local server running)
        if (discoveredServers.empty()) {
            renderLine("  (no servers found)", white, y);
            renderLine("  Start server with: ./build/server/fb-server -l", white, y);
        } else {
            for (int i = 0; i < (int)discoveredServers.size(); i++) {
                bool sel = (lanMenuIndex == i + 1);
                const std::string& dname = discoveredServers[i].name.empty()
                    ? discoveredServers[i].host + ":" + std::to_string(discoveredServers[i].port)
                    : discoveredServers[i].name;
                char latBuf[16];
                int lat = discoveredServers[i].latencyMs;
                if (lat < 0) snprintf(latBuf, sizeof(latBuf), "offline");
                else         snprintf(latBuf, sizeof(latBuf), "%dms", lat);
                snprintf(lineBuf, sizeof(lineBuf),
                    sel ? "[ %-28s %7s ]" : "  %-28s %7s  ",
                    dname.c_str(), latBuf);
                renderLine(lineBuf, sel ? yellow : white, y);
            }
        }

        // Last menu item: Set Name
        {
            int lanMenuMax = 2 + (int)discoveredServers.size(); // 0=Host, 1..n=servers, n+1=SetName
            bool sel = (lanMenuIndex == lanMenuMax - 1);
            #ifdef __ANDROID__
            const char* curNick = networkPreNick[0] != '\0' ? networkPreNick : (getenv("USER") ? getenv("USER") : "android_user");
#else
            const char* curNick = networkPreNick[0] != '\0' ? networkPreNick : (getenv("USER") ? getenv("USER") : "unnamed");
#endif
            snprintf(lineBuf, sizeof(lineBuf), sel ? "[ Set Name: %-20s ]" : "  Set Name: %-20s  ", curNick);
            renderLine(lineBuf, sel ? yellow : white, y);
        }

        snprintf(lineBuf, sizeof(lineBuf), "\nUP/DOWN  ENTER to select  R to rescan\nESC to cancel");
        renderLine(lineBuf, white, y);

        if (!connectErrorMsg.empty()) {
            snprintf(lineBuf, sizeof(lineBuf), "\n%s", connectErrorMsg.c_str());
            renderLine(lineBuf, red, y);
        }

        return;
    }

    if (!networkInLobby && networkInputMode == 10) {
#ifndef __WASM_PORT__
        // Poll background server fetch result
        if (!serverFetchInProgress.load() && publicServers.empty()) {
            std::lock_guard<std::mutex> lock(serverFetchMutex);
            publicServers = std::move(serverFetchResult);
            serverFetchResult.clear();
        }
#endif

        // Net game public server list screen
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color black  = {0, 0, 0, 255};
        SDL_Color yellow = {255, 220, 50, 255};
        SDL_Color red    = {255, 80, 80, 255};
        SDL_Color grey   = {160, 160, 160, 255};

        auto renderLine = [&](const char* txt, SDL_Color fg, int& y) {
            panelText.UpdateColor(fg, black);
            panelText.UpdateText(const_cast<SDL_Renderer*>(renderer), txt, 0);
            panelText.UpdatePosition({(640/2) - (panelText.Coords()->w / 2), y});
            { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), panelText.Texture(), nullptr, &fr); };
            y += panelText.Coords()->h;
        };

        int y = (480/2) - 120;
        char lineBuf[320];

        renderLine("Net Game\n", white, y);

        // Menu item 0: Manual entry
        bool manualSel = (netMenuIndex == 0);
        renderLine(manualSel ? "[ Manual entry ]" : "  Manual entry  ", manualSel ? yellow : white, y);

        // Menu items 1+: public internet servers only
        // (Local LAN server is accessible via the LAN Game panel instead)
        if (serverFetchInProgress.load()) {
            renderLine("  (fetching server list...)", grey, y);
        } else if (publicServers.empty()) {
            renderLine("  (no public servers listed)", white, y);
        } else {
            for (int i = 0; i < (int)publicServers.size(); i++) {
                bool sel = (netMenuIndex == i + 1);
                bool offline = (publicServers[i].latencyMs < 0);
                const std::string& displayName = publicServers[i].name.empty()
                    ? publicServers[i].host + ":" + std::to_string(publicServers[i].port)
                    : publicServers[i].name;
                char latencyBuf[16];
                if (offline) snprintf(latencyBuf, sizeof(latencyBuf), "offline");
                else         snprintf(latencyBuf, sizeof(latencyBuf), "%dms", publicServers[i].latencyMs);
                snprintf(lineBuf, sizeof(lineBuf),
                    sel ? "[ %-28s %7s ]" : "  %-28s %7s  ",
                    displayName.c_str(), latencyBuf);
                SDL_Color col = offline ? grey : (sel ? yellow : white);
                renderLine(lineBuf, col, y);
            }
        }

        // Last menu item: Set Name
        {
            int netMenuMax = 2 + (int)publicServers.size(); // 0=Manual, 1..n=servers, n+1=SetName
            bool sel = (netMenuIndex == netMenuMax - 1);
            #ifdef __ANDROID__
            const char* curNick = networkPreNick[0] != '\0' ? networkPreNick : (getenv("USER") ? getenv("USER") : "android_user");
#else
            const char* curNick = networkPreNick[0] != '\0' ? networkPreNick : (getenv("USER") ? getenv("USER") : "unnamed");
#endif
            snprintf(lineBuf, sizeof(lineBuf), sel ? "[ Set Name: %-20s ]" : "  Set Name: %-20s  ", curNick);
            renderLine(lineBuf, sel ? yellow : white, y);
        }

        snprintf(lineBuf, sizeof(lineBuf), "\nUP/DOWN  ENTER to select  R to refresh\nESC to cancel");
        renderLine(lineBuf, white, y);

        if (pendingLobbyConnect) {
            renderLine("\nConnecting...", yellow, y);
        } else if (!connectErrorMsg.empty()) {
            snprintf(lineBuf, sizeof(lineBuf), "\n%s", connectErrorMsg.c_str());
            renderLine(lineBuf, red, y);
        }

        return;
    }

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

        snprintf(lineBuf, sizeof(lineBuf), hostActive && networkFieldEditing ? "Host: [ %s_ ]" : "Host:   %s  ", networkHost);
        renderLine(lineBuf, hostActive ? yellow : white, y);

        snprintf(lineBuf, sizeof(lineBuf), portActive && networkFieldEditing ? "Port: [ %d_ ]" : "Port:   %d  ", networkPort);
        renderLine(lineBuf, portActive ? yellow : white, y);

        snprintf(lineBuf, sizeof(lineBuf), connectActive ? "[ Connect ]" : "  Connect  ");
        renderLine(lineBuf, connectActive ? yellow : white, y);

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

        int chatStart = chatMsgs.size() > 3 ? chatMsgs.size() - 3 : 0;
        for (size_t i = chatStart; i < chatMsgs.size(); i++) {
            offset += snprintf(lobbyText + offset, sizeof(lobbyText) - offset,
                "<%s> %s\n",
                chatMsgs[i].nick.c_str(),
                chatMsgs[i].message.c_str());
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
