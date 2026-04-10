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

// WebAssembly WebSocket transport layer for NetworkClient.
// Provides WASM-specific overrides for Connect/Disconnect/SendCommand/Update
// and stubs for TCP-only static methods (LAN discovery, latency probing, etc.).
// All shared protocol methods (SendNick, CreateGame, ParseMessage, etc.) live
// in networkclient.cpp and are compiled for both native and WASM targets.

#ifdef __WASM_PORT__

#include "networkclient.h"
#include <SDL3/SDL.h>
#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <cstring>

// ---------------------------------------------------------------------------
// Internal WebSocket state
// ---------------------------------------------------------------------------

struct WebSocketHandle {
    EMSCRIPTEN_WEBSOCKET_T socket;
    NetworkClient* client;
};

static WebSocketHandle* s_handle = nullptr;  // singleton; only one connection at a time

// ---------------------------------------------------------------------------
// WebSocket event callbacks
// ---------------------------------------------------------------------------

static EM_BOOL onWebSocketOpen(int /*eventType*/, const EmscriptenWebSocketOpenEvent* /*e*/, void* userData) {
    SDL_Log("WebSocket connection opened");
    WebSocketHandle* handle = (WebSocketHandle*)userData;
    if (handle && handle->client) {
        // Transition to CONNECTED so SendCommand and protocol methods work
        handle->client->SetConnected();
    }
    return EM_TRUE;
}

static EM_BOOL onWebSocketClose(int /*eventType*/, const EmscriptenWebSocketCloseEvent* e, void* userData) {
    SDL_Log("WebSocket connection closed (code: %d)", e->code);
    WebSocketHandle* handle = (WebSocketHandle*)userData;
    if (handle && handle->client) {
        handle->client->Disconnect();
    }
    return EM_TRUE;
}

static EM_BOOL onWebSocketError(int /*eventType*/, const EmscriptenWebSocketErrorEvent* /*e*/, void* userData) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WebSocket error");
    WebSocketHandle* handle = (WebSocketHandle*)userData;
    if (handle && handle->client) {
        handle->client->Disconnect();
    }
    return EM_TRUE;
}

static EM_BOOL onWebSocketMessage(int /*eventType*/, const EmscriptenWebSocketMessageEvent* e, void* userData) {
    WebSocketHandle* handle = (WebSocketHandle*)userData;
    if (!handle || !handle->client) return EM_TRUE;
    if (!e->data || e->numBytes == 0) return EM_TRUE;

    const char* data = (const char*)e->data;
    int numBytes = (int)e->numBytes;

    // In-game: messages are binary prio format — {senderId_byte}{data}\n
    // Mirror the native TCP ProcessIncomingData logic: parse each newline-delimited
    // message, strip the leading sender-ID byte, and enqueue as GAMEMSG:{id}:{data}.
    if (handle->client->GetState() == IN_GAME) {
        int pos = 0;
        while (pos < numBytes) {
            // Need at least 2 bytes: sender ID + at least one data byte
            if (numBytes - pos < 2) break;
            unsigned char senderId = (unsigned char)data[pos];
            int msgStart = pos + 1;
            const char* nl = (const char*)memchr(data + msgStart, '\n', numBytes - msgStart);
            int msgLen = nl ? (int)(nl - (data + msgStart)) : (numBytes - msgStart);
            if (msgLen > 0) {
                char gameMsg[4096];
                int copyLen = (msgLen < (int)sizeof(gameMsg) - 1) ? msgLen : (int)sizeof(gameMsg) - 1;
                memcpy(gameMsg, data + msgStart, copyLen);
                gameMsg[copyLen] = '\0';
                char fullMsg[4096];
                snprintf(fullMsg, sizeof(fullMsg), "GAMEMSG:%d:%s", (int)senderId, gameMsg);
                handle->client->QueueGameMessage(std::string(fullMsg));
            }
            pos = nl ? (int)(nl - data) + 1 : numBytes;
        }
        return EM_TRUE;
    }

    // Lobby/pre-game: text protocol, newline-delimited
    const char* line = data;
    const char* end = data + numBytes;
    while (line < end) {
        const char* nl = (const char*)memchr(line, '\n', end - line);
        size_t len = nl ? (size_t)(nl - line) : (size_t)(end - line);
        if (len > 0) {
            char msg[4096];
            size_t copyLen = (len < sizeof(msg) - 1) ? len : sizeof(msg) - 1;
            memcpy(msg, line, copyLen);
            msg[copyLen] = '\0';
            if (copyLen > 0 && msg[copyLen - 1] == '\r') msg[copyLen - 1] = '\0';
            handle->client->ParseMessage(msg);
        }
        if (!nl) break;
        line = nl + 1;
    }
    return EM_TRUE;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

NetworkClient::NetworkClient()
    : websocketSocket(nullptr), state(DISCONNECTED), currentGame(nullptr), myPlayerId(0) {
    SDL_Log("NetworkClient (WASM) constructor called");
}

NetworkClient::~NetworkClient() {
    Disconnect();
}

// ---------------------------------------------------------------------------
// Transport: Connect / Disconnect / SendCommand / ProcessIncomingData / Update
// ---------------------------------------------------------------------------

bool NetworkClient::Connect(const char* host, int port) {
    if (state != DISCONNECTED) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Already connected or connecting");
        return false;
    }

    // Use wss:// when the page is served over HTTPS (browsers block mixed content).
    // Use ws:// on plain HTTP (local / itch.io iframe without forcing HTTPS).
    const char* scheme = (EM_ASM_INT({ return location.protocol === 'https:' ? 1 : 0; }))
                         ? "wss://" : "ws://";
    std::string wsUrl = scheme;
    wsUrl += host;
    wsUrl += ":";
    wsUrl += std::to_string(port);

    SDL_Log("Connecting to WebSocket: %s", wsUrl.c_str());

    WebSocketHandle* handle = new WebSocketHandle();
    handle->client = this;

    EmscriptenWebSocketCreateAttributes attrs;
    emscripten_websocket_init_create_attributes(&attrs);
    attrs.url = wsUrl.c_str();
    attrs.protocols = nullptr;
    attrs.createOnMainThread = EM_TRUE;

    EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(&attrs);
    if (ws <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create WebSocket");
        delete handle;
        return false;
    }

    handle->socket = ws;
    websocketSocket = handle;
    s_handle = handle;

    emscripten_websocket_set_onopen_callback(ws, handle, onWebSocketOpen);
    emscripten_websocket_set_onclose_callback(ws, handle, onWebSocketClose);
    emscripten_websocket_set_onerror_callback(ws, handle, onWebSocketError);
    emscripten_websocket_set_onmessage_callback(ws, handle, onWebSocketMessage);

    state = CONNECTING;
    return true;
}

void NetworkClient::Disconnect() {
    WebSocketHandle* handle = (WebSocketHandle*)websocketSocket;
    if (handle) {
        if (handle->socket > 0) {
            emscripten_websocket_close(handle->socket, 1000, "");
            emscripten_websocket_delete(handle->socket);
        }
        delete handle;
        websocketSocket = nullptr;
        s_handle = nullptr;
    }
    state = DISCONNECTED;
    currentGame = nullptr;
    pendingCreate = false;
    pendingJoin = false;
    gameList.clear();
    messageQueue.clear();
}

bool NetworkClient::SendCommand(const char* command) {
    WebSocketHandle* handle = (WebSocketHandle*)websocketSocket;
    if (!handle || handle->socket <= 0 || state == DISCONNECTED || state == CONNECTING) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Not connected, cannot send command");
        return false;
    }

    // Wrap in fb-server protocol header and append newline
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "FB/%d.%d %s\n", PROTO_MAJOR, PROTO_MINOR, command);

    EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(handle->socket, buffer);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send WebSocket message: %d", result);
        return false;
    }

    SDL_Log("Sent: %s", command);
    return true;
}

bool NetworkClient::ProcessIncomingData() {
    // WebSocket messages arrive asynchronously via onWebSocketMessage callback
    // and are placed directly into the message queue — nothing to poll here.
    return false;
}

void NetworkClient::Update() {
    // Nothing to do: message delivery is callback-driven in WASM.
}

bool NetworkClient::SendGameData(const char* data) {
    // Prio (in-game) messages use the same binary format as the TCP client:
    //   byte 0  = myPlayerId  (server fd, identifies sender to other clients)
    //   bytes 1+ = {data}\n
    // Sent as a WebSocket binary frame so the server passes it to process_msg_prio
    // (not process_msg, which requires the "FB/1.2 " text-protocol prefix).
    WebSocketHandle* handle = (WebSocketHandle*)websocketSocket;
    if (!handle || handle->socket <= 0) return false;
    if (state != IN_GAME) return false;
    if (myPlayerId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Player ID not set - cannot send game data");
        return false;
    }

    char buffer[BUFFER_SIZE];
    buffer[0] = (char)myPlayerId;
    int msgLen = snprintf(buffer + 1, (int)sizeof(buffer) - 2, "%s\n", data);
    int totalLen = 1 + msgLen;

    bool isPing = (strcmp(data, "p") == 0);
    if (!isPing) {
        SDL_Log(">>> Sending game data: [ID=%d] %s (%d bytes)", (int)myPlayerId, data, totalLen);
    }

    EMSCRIPTEN_RESULT result = emscripten_websocket_send_binary(handle->socket, buffer, totalLen);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send game data: %d", result);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Static stubs — TCP/UDP not available in browser
// ---------------------------------------------------------------------------

std::vector<ServerInfo> NetworkClient::DiscoverLANServers() {
    SDL_Log("LAN discovery not available in WebAssembly");
    return {};
}

std::vector<ServerInfo> NetworkClient::FetchPublicServers() {
    // Return the known public server as default.
    // When served over HTTPS (e.g. itch.io), browsers require WSS on port 443
    // with a reverse proxy forwarding to fb-server on 1511.
    // Over plain HTTP (localhost dev), use ws:// on the native server port.
    bool isHttps = (bool)EM_ASM_INT({ return location.protocol === 'https:' ? 1 : 0; });
    std::vector<ServerInfo> servers;
    ServerInfo s;
    s.host = "fb.servequake.com";
    s.port = isHttps ? 443 : 1511;
    s.name = "fb.servequake.com (browser)";
    s.latencyMs = 0;
    servers.push_back(s);
    return servers;
}

std::string NetworkClient::DetectGeoLocation() {
    return "zz";
}

int NetworkClient::MeasureLatency(const char* /*host*/, int /*port*/, int /*timeoutMs*/) {
    // TCP latency probing is not available in a browser — return 0 so the
    // server list shows servers as available rather than "offline".
    return 0;
}

bool NetworkClient::IsReachable(const char* /*host*/, int /*port*/, int /*timeoutMs*/) {
    return true;
}

#endif // __WASM_PORT__
