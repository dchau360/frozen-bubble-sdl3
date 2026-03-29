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

#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <queue>
#include <map>

#ifndef __WASM_PORT__
// Raw sockets not available in WebAssembly
#include "socket_compat.h"
#endif

#define PROTO_MAJOR 1
#define PROTO_MINOR 2
#define BUFFER_SIZE 4096

enum ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    IN_LOBBY,
    IN_GAME
};

struct NetworkPlayer {
    std::string nick;
    std::string geoloc;
    bool ready;
};

struct GameRoom {
    std::string creator;
    std::vector<NetworkPlayer> players;
    bool started;
};

struct ChatMessage {
    std::string nick;
    std::string message;
    Uint32 timestamp;
};

struct ServerInfo {
    std::string host;
    int port;
    std::string name;     // Display name (empty = use host:port)
    int latencyMs = -1;   // Round-trip TCP connect time in ms; -1 = unreachable/unknown
};

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    // Connection management
    bool Connect(const char* host, int port);
    void Disconnect();
    bool IsConnected() { return state != DISCONNECTED; }
    ConnectionState GetState() { return state; }

    // Protocol commands
    bool SendNick(const char* nickname);
    bool SendGeoLoc(const char* location);
    bool CreateGame();
    bool JoinGame(const char* creator);
    bool StartGame();
    bool PartGame();
    bool SendTalk(const char* message);
    bool SendGameData(const char* data);
    bool RequestList();
    bool SendCommand(const char* command);

    // Message processing
    void Update(); // Call this each frame
    bool HasMessage();
    std::string GetNextMessage();
    void PutBackMessage(const std::string& msg);  // Put message back at front of queue

    // Game state
    std::vector<GameRoom> GetGameList() { return gameList; }
    void ClearGameList() { gameList.clear(); openPlayers.clear(); }
    std::vector<NetworkPlayer> GetOpenPlayers() { return openPlayers; }
    std::vector<ChatMessage> GetChatMessages() { return chatMessages; }
    GameRoom* GetCurrentGame() { return currentGame; }
    std::string GetPlayerNick() { return playerNick; }
    bool IsLeader();  // Are we the game creator?
    unsigned char GetMyPlayerId() { return myPlayerId; }

    // Level synchronization for multiplayer
    bool SendBubble(int cx, int cy, int bubbleId);  // Leader sends bubble position
    bool SendNextBubble(int bubbleId);   // Leader sends next bubble
    bool SendTobeBubble(int bubbleId);   // Leader sends tobe bubble
    bool WaitForBubble(int& cx, int& cy, int& bubbleId);  // Joiner waits for bubble
    bool WaitForNextBubble(int& bubbleId);  // Joiner waits for next bubble
    bool WaitForTobeBubble(int& bubbleId);  // Joiner waits for tobe bubble

    // Add a local status message (for commands like /nick, /help)
    void AddStatusMessage(const std::string& message);

    // Parse and enqueue a raw protocol message line (used by WASM WebSocket callback)
    void ParseMessage(const char* message);
    // Enqueue an already-formatted GAMEMSG (used by WASM prio message path)
    void QueueGameMessage(const std::string& msg) { messageQueue.push_back(msg); }
    size_t MessageQueueSize() const { return messageQueue.size(); }
    // Called by WASM open callback to transition state to CONNECTED
    void SetConnected() { state = CONNECTED; }
    // True while waiting for async CREATE OK/rejection from server (WASM only)
    bool IsPendingCreate() const { return pendingCreate; }
    // True while waiting for async JOIN OK/rejection from server (WASM only)
    bool IsPendingJoin() const { return pendingJoin; }

    // Send game options to other players (host only)
    bool SendOptions(bool chainReaction, bool continueWhenLeave, bool singleTarget, int victoriesLimit, const int playerColors[5], const bool noCompress[5], const bool aimGuide[5]);

    // Received options from host (updated when SETOPTIONS push arrives)
    bool pendingOptions = false;
    bool rcvChainReaction = true;
    bool rcvContinueLeave = true;
    bool rcvSingleTarget = true;
    int rcvVictoriesLimit = 5;
    int rcvPlayerColors[5] = {7, 7, 7, 7, 7};
    bool rcvNoCompress[5] = {false, false, false, false, false};
    bool rcvAimGuide[5] = {false, false, false, false, false};
    // Returns true (and clears flag) if new options arrived since last call
    bool GetAndClearPendingOptions(bool& cr, bool& cl, bool& st, int& vl, int pc[5], bool nc[5], bool ag[5]) {
        if (!pendingOptions) return false;
        pendingOptions = false;
        cr = rcvChainReaction; cl = rcvContinueLeave; st = rcvSingleTarget; vl = rcvVictoriesLimit;
        for (int i = 0; i < 5; i++) { pc[i] = rcvPlayerColors[i]; nc[i] = rcvNoCompress[i]; ag[i] = rcvAimGuide[i]; }
        return true;
    }

    // Get nickname for a player ID (for multiplayer display)
    std::string GetPlayerNickname(int playerId) const {
        auto it = playerIdToNick.find(playerId);
        return (it != playerIdToNick.end()) ? it->second : "";
    }

    // Get all player ID->nick mappings (populated from GAME_CAN_START)
    const std::map<int, std::string>& GetPlayerIdToNick() const { return playerIdToNick; }

    static NetworkClient* Instance(const char* host = nullptr, int port = 0);
    static void Dispose();
    static std::vector<ServerInfo> DiscoverLANServers();
    static std::vector<ServerInfo> FetchPublicServers();
    static std::string DetectGeoLocation();  // Detect player's lat/lon via IP; returns "lat:lon" or "zz"
    // Returns TCP connect latency in ms, or -1 if unreachable within timeoutMs
    static int MeasureLatency(const char* host, int port, int timeoutMs = 2000);
    static bool IsReachable(const char* host, int port, int timeoutMs = 2000);

private:
#ifndef __WASM_PORT__
    int sockfd;  // TCP socket (native builds)
#else
    void* websocketSocket;  // WebSocket handle (WebAssembly builds) - using void* to avoid emscripten header dependency
#endif
    ConnectionState state;
    std::string playerNick;
    std::string playerGeoloc;

    std::deque<std::string> messageQueue;
    std::vector<GameRoom> gameList;
    std::vector<NetworkPlayer> openPlayers;
    std::vector<ChatMessage> chatMessages;
    GameRoom* currentGame;

#ifndef __WASM_PORT__
    char recvBuffer[BUFFER_SIZE];
    int recvBufferLen;
#endif

    unsigned char myPlayerId;  // Player ID assigned by server for game messages
    std::string myNickname;    // Our nickname for ID mapping
    std::string lastErrorResponse;  // Last error message from server
    std::map<int, std::string> playerIdToNick;  // Map of player ID to nickname

    bool ProcessIncomingData();  // Returns true if data was read, false if EWOULDBLOCK
    void HandleServerResponse(const std::string& response);
    void ParseListResponse(const char* listData);
    void HandlePushMessage(const std::string& pushMsg);

    // WASM async CREATE state
    bool pendingCreate = false;
    std::string pendingCreateOrigNick;
    std::string pendingCreateNick;
    int pendingCreateSuffix = 2;

    // WASM async JOIN state
    bool pendingJoin = false;
    std::string pendingJoinCreator;
    std::string pendingJoinOrigNick;
    std::string pendingJoinNick;
    int pendingJoinSuffix = 2;

    static NetworkClient* ptrInstance;
};

#endif // NETWORKCLIENT_H
