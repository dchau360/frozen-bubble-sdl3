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
#include <SDL2/SDL.h>
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
    if (!e->isText || !e->data || e->numBytes == 0) return EM_TRUE;

    // Message data is already in e->data as a null-terminated string when isText==true
    const char* data = (const char*)e->data;

    // Parse newline-delimited protocol messages
    const char* line = data;
    const char* end = data + e->numBytes;
    while (line < end) {
        const char* nl = (const char*)memchr(line, '\n', end - line);
        size_t len = nl ? (size_t)(nl - line) : (size_t)(end - line);
        if (len > 0) {
            char msg[4096];
            size_t copyLen = (len < sizeof(msg) - 1) ? len : sizeof(msg) - 1;
            memcpy(msg, line, copyLen);
            msg[copyLen] = '\0';
            // Strip trailing \r
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

    std::string wsUrl = "ws://";
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
    gameList.clear();
    messageQueue.clear();
}

bool NetworkClient::SendCommand(const char* command) {
    WebSocketHandle* handle = (WebSocketHandle*)websocketSocket;
    if (!handle || handle->socket <= 0 || state != CONNECTED) {
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

// ---------------------------------------------------------------------------
// Static stubs — TCP/UDP not available in browser
// ---------------------------------------------------------------------------

std::vector<ServerInfo> NetworkClient::DiscoverLANServers() {
    SDL_Log("LAN discovery not available in WebAssembly");
    return {};
}

std::vector<ServerInfo> NetworkClient::FetchPublicServers() {
    SDL_Log("Public server fetch not yet implemented in WebAssembly (requires emscripten_fetch)");
    return {};
}

std::string NetworkClient::DetectGeoLocation() {
    return "zz";
}

int NetworkClient::MeasureLatency(const char* /*host*/, int /*port*/, int /*timeoutMs*/) {
    return -1;
}

bool NetworkClient::IsReachable(const char* /*host*/, int /*port*/, int /*timeoutMs*/) {
    return false;
}

#endif // __WASM_PORT__
