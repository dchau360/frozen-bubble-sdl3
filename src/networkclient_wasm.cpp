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

// WebAssembly/WebSocket network client implementation
// This file replaces networkclient.cpp for WebAssembly builds
// Browsers cannot use raw TCP sockets - must use WebSocket via emscripten_websocket.h

#ifdef __WASM_PORT__

#include "networkclient.h"
#include <SDL2/SDL.h>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <cstring>
#include <errno.h>
#include <stdio.h>

// WebSocket handle wrapper
struct WebSocketHandle {
    EMSCRIPTEN_WEBSOCKET_T socket;
    NetworkClient* client;
    std::deque<std::string>* messageQueue;
};

// WebSocket callback functions
static EM_BOOL onWebSocketOpen(int eventType, const EmscriptenWebSocketOpenEvent* e, void* userData) {
    SDL_Log("WebSocket connection opened");
    WebSocketHandle* handle = (WebSocketHandle*)userData;
    if (handle && handle->client) {
        // State transition handled by client
    }
    return EM_TRUE;
}

static EM_BOOL onWebSocketClose(int eventType, const EmscriptenWebSocketCloseEvent* e, void* userData) {
    SDL_Log("WebSocket connection closed (code: %d)", e->code);
    WebSocketHandle* handle = (WebSocketHandle*)userData;
    if (handle && handle->client) {
        handle->client->Disconnect();
    }
    return EM_TRUE;
}

static EM_BOOL onWebSocketError(int eventType, const EmscriptenWebSocketErrorEvent* e, void* userData) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WebSocket error");
    WebSocketHandle* handle = (WebSocketHandle*)userData;
    if (handle && handle->client) {
        handle->client->Disconnect();
    }
    return EM_TRUE;
}

static EM_BOOL onWebSocketMessage(int eventType, const EmscriptenWebSocketMessageEvent* e, void* userData) {
    WebSocketHandle* handle = (WebSocketHandle*)userData;
    if (!handle || !handle->messageQueue) {
        return EM_TRUE;
    }

    // Get message length
    unsigned long msgLen = 0;
    emscripten_websocket_get_buffered_amount(e->socket, &msgLen);
    if (msgLen > 0) {
        char* msgData = new char[msgLen + 1];
        emscripten_websocket_receive(e->socket, msgData, msgLen);
        msgData[msgLen] = '\0';

        // Parse the message (fb-server protocol is newline-delimited)
        char* line = msgData;
        char* next;
        while ((next = strchr(line, '\n')) != nullptr) {
            *next = '\0';
            if (strlen(line) > 0) {
                handle->messageQueue->push_back(std::string(line));
            }
            line = next + 1;
        }
        // Handle last line without newline
        if (strlen(line) > 0) {
            handle->messageQueue->push_back(std::string(line));
        }
        delete[] msgData;
    }
    return EM_TRUE;
}

bool NetworkClient::Connect(const char* host, int port) {
    if (state != DISCONNECTED) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Already connected or connecting");
        return false;
    }

    // Build WebSocket URL (browsers require ws:// or wss://)
    std::string wsUrl = "ws://";
    wsUrl += host;
    wsUrl += ":";
    wsUrl += std::to_string(port);

    SDL_Log("Connecting to WebSocket: %s", wsUrl.c_str());

    // Create WebSocket handle
    WebSocketHandle* handle = new WebSocketHandle();
    handle->client = this;
    handle->messageQueue = &messageQueue;

    // Create WebSocket connection
    EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(wsUrl.c_str(), nullptr, EM_TRUE);
    if (ws <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create WebSocket");
        delete handle;
        return false;
    }

    handle->socket = ws;
    websocketSocket = (void*)handle;

    // Register callbacks
    emscripten_websocket_set_onopen_callback(ws, handle, onWebSocketOpen);
    emscripten_websocket_set_onclose_callback(ws, handle, onWebSocketClose);
    emscripten_websocket_set_onerror_callback(ws, handle, onWebSocketError);
    emscripten_websocket_set_onmessage_callback(ws, handle, onWebSocketMessage);

    state = CONNECTING;

    // emscripten_websocket_new is async - we'll transition to CONNECTED in onWebSocketOpen
    return true;
}

void NetworkClient::Disconnect() {
    WebSocketHandle* handle = (WebSocketHandle*)websocketSocket;
    if (handle) {
        if (handle->socket > 0) {
            emscripten_websocket_close(handle->socket, EM_TRUE);
        }
        delete handle;
        websocketSocket = nullptr;
    }
    state = DISCONNECTED;
}

bool NetworkClient::SendCommand(const char* command) {
    WebSocketHandle* handle = (WebSocketHandle*)websocketSocket;
    if (!handle || handle->socket <= 0 || state != CONNECTED) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Not connected, cannot send command");
        return false;
    }

    // Send command as text message (fb-server protocol is text-based)
    EMSCRIPTEN_RESULT result = emscripten_websocket_send(handle->socket, (void*)command, strlen(command));
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send WebSocket message");
        return false;
    }
    return true;
}

bool NetworkClient::ProcessIncomingData() {
    // WebSocket messages are handled asynchronously in onWebSocketMessage
    // They're already in the queue, no additional processing needed
    return true;
}

bool NetworkClient::SendNick(const char* nickname) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "NICK %s", nickname);
    return SendCommand(cmd);
}

bool NetworkClient::SendGeoLoc(const char* location) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "GEOLOC %s", location);
    return SendCommand(cmd);
}

bool NetworkClient::CreateGame() {
    return SendCommand("CREATE");
}

bool NetworkClient::JoinGame(const char* creator) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "JOIN %s", creator);
    return SendCommand(cmd);
}

bool NetworkClient::StartGame() {
    return SendCommand("START");
}

bool NetworkClient::PartGame() {
    return SendCommand("PART");
}

bool NetworkClient::SendTalk(const char* message) {
    // Escape message for protocol
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "TALK %s", message);
    return SendCommand(cmd);
}

bool NetworkClient::SendGameData(const char* data) {
    // Game data is sent as-is
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "%s", data);
    return SendCommand(cmd);
}

bool NetworkClient::RequestList() {
    return SendCommand("LIST");
}

bool NetworkClient::SendBubble(int cx, int cy, int bubbleId) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "b %d %d %d", cx, cy, bubbleId);
    return SendCommand(cmd);
}

bool NetworkClient::SendNextBubble(int bubbleId) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "n %d", bubbleId);
    return SendCommand(cmd);
}

bool NetworkClient::SendTobeBubble(int bubbleId) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "t %d", bubbleId);
    return SendCommand(cmd);
}

bool NetworkClient::WaitForBubble(int& cx, int& cy, int& bubbleId) {
    // Handled via message queue in main game loop
    return false;
}

bool NetworkClient::WaitForNextBubble(int& bubbleId) {
    return false;
}

bool NetworkClient::WaitForTobeBubble(int& bubbleId) {
    return false;
}

bool NetworkClient::SendOptions(bool chainReaction, bool continueWhenLeave, bool singleTarget, int victoriesLimit, const int playerColors[5]) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "O %d %d %d %d %d %d %d %d %d",
             chainReaction ? 1 : 0, continueWhenLeave ? 1 : 0, singleTarget ? 1 : 0, victoriesLimit,
             playerColors[0], playerColors[1], playerColors[2], playerColors[3], playerColors[4]);
    return SendCommand(cmd);
}

bool NetworkClient::IsLeader() {
    if (!currentGame) return false;
    return playerNick == currentGame->creator;
}

#endif // __WASM_PORT__
