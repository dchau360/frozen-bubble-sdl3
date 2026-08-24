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

#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <queue>
#include <map>

#ifndef __WASM_PORT__
// Raw sockets not available in WebAssembly
#include "socket_compat.h"
#endif

#define PROTO_MAJOR 1
#define PROTO_MINOR 3
#define BUFFER_SIZE 4096

// The accumulation buffer has to be able to hold one whole server line. The
// server formats each line into a 16384-byte buffer and can emit up to
// sizeof(buf)-1 of it (server/net.c send_line), and the LIST reply is built in a
// 16384-byte list_games_str, so a busy lobby legitimately produces lines far
// past BUFFER_SIZE. Sized above that ceiling with room for a partial line still
// waiting in front of the next one.
#define RECV_BUFFER_SIZE 32768

// Highest team number a player may be assigned. Team numbers are one-based and
// are used to index kTeamColors (bubblegame.h), which static_asserts that it
// holds exactly this many entries. Peer-supplied team values are clamped to
// [1, kMaxTeams] when OPTIONS is parsed.
inline constexpr int kMaxTeams = 5;

// Fold an untrusted team number into the range kTeamColors can be indexed with.
// OPTIONS arrives from another client, so PLAYERTEAM_Pn is arbitrary until this
// runs; 0 would index kTeamColors[-1] and anything above kMaxTeams would run off
// the end.
inline constexpr int ClampTeamNumber(int team) {
    if (team < 1) return 1;
    if (team > kMaxTeams) return kMaxTeams;
    return team;
}

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
    int maxPlayers = 5;  // room cap from LIST's "]:N" suffix; 5 when absent (old server)
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

// Whether the currently-connected server understands NOTIFYREG/NOTIFYUNREG at
// all -- an older fb-server, or anything else answering on that port, has
// never heard of them and the protocol gives no other way to tell short of
// asking. Reset to Unknown on every new connection.
enum class NotifySupport { Unknown, Supported, Unsupported };

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    // Connection management
    bool Connect(const char* host, int port);
    void Disconnect();
    bool IsConnected() { return state != DISCONNECTED; }
    ConnectionState GetState() { return state; }

    // Where we are connected (or were last asked to connect). Used to tell
    // whether a server picked out of a list is the one this connection is
    // talking to, which decides whether a follow can be registered right now.
    const std::string& GetHost() const { return connectedHost; }
    int GetPort() const { return connectedPort; }

    // Protocol commands
    bool SendNick(const char* nickname);
    bool SendGeoLoc(const char* location);
    bool CreateGame(int maxPlayers = 5);
    bool JoinGame(const char* creator);
    bool StartGame();
    bool PartGame();
    bool SendTalk(const char* message);
    bool SendGameData(const char* data);
    bool RequestList();

    // "Follow this server": hand the server this device's push token so it can
    // notify us when someone joins, including after we disconnect. platform
    // must be "ios" or "android" -- the server rejects anything else, and no
    // other platform has a push story to register for. Safe to re-send on
    // every connect; the server upserts by token without resetting its
    // notification cooldown.
    bool SendNotifyReg(const char* platform, const char* token);
    bool SendNotifyUnreg(const char* token);

    // Report a player for abuse. The server appends it to a file for its
    // operator to review; nothing is enforced automatically (a nick is not an
    // identity here, so auto-acting on reports would be trivially abusable).
    // Blocking, which is local and immediate, is the other half of this --
    // see GameSettings::ToggleBlockedPlayer.
    bool SendReport(const char* nick, const char* reason);

    // Capability probe for the follow feature: sends a side-effect-free
    // NOTIFYUNREG for a token nothing will ever hold, once per connection,
    // and reads the next "OK" (supported) vs "UNKNOWN_COMMAND" (not) off the
    // wire in HandleServerResponse(). No-op once notifySupport is already
    // known, or while a probe is already in flight, so it is safe to call
    // every frame from render code.
    void ProbeNotifySupportIfNeeded();
    NotifySupport GetNotifySupport() const { return notifySupport; }

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

    // Bubble-sync message queue: 'b|', 'N', 'T' messages routed here by ProcessNetworkMessages
    // so WaitForBubble/WaitForNextBubble/WaitForTobeBubble can find them even if they arrived
    // before SyncNetworkLevel was called (race condition fix for round 2+).
    void PushSyncMessage(const std::string& msg) { syncQueue.push_back(msg); }
    bool HasSyncMessage() const { return !syncQueue.empty(); }
    std::string GetNextSyncMessage() {
        if (syncQueue.empty()) return "";
        std::string msg = syncQueue.front();
        syncQueue.pop_front();
        return msg;
    }
    // Called by WASM open callback to transition state to CONNECTED
    void SetConnected() { state = CONNECTED; }
    // True while waiting for async CREATE OK/rejection from server (WASM only)
    bool IsPendingCreate() const { return pendingCreate; }
    // True while waiting for async JOIN OK/rejection from server (WASM only)
    bool IsPendingJoin() const { return pendingJoin; }

    // Send game options to other players (host only)
    bool SendOptions(bool chainReaction, bool continueWhenLeave, bool singleTarget, int victoriesLimit, const int playerColors[5], const bool noCompress[5], const bool aimGuide[5], bool mouseEnabled, bool clearMode, bool disableMalus, bool teamMode, const int playerTeams[5], int teamCount);

    // Received options from host (updated when SETOPTIONS push arrives)
    bool pendingOptions = false;
    bool rcvChainReaction = true;
    bool rcvContinueLeave = true;
    bool rcvSingleTarget = true;
    int rcvVictoriesLimit = 5;
    int rcvPlayerColors[5] = {7, 7, 7, 7, 7};
    bool rcvNoCompress[5] = {false, false, false, false, false};
    bool rcvAimGuide[5] = {false, false, false, false, false};
    bool rcvMouseEnabled = false;
    bool rcvClearMode = false;
    bool rcvDisableMalus = false;
    bool rcvTeamMode = false;
    int rcvPlayerTeams[5] = {1, 2, 3, 4, 5};
    int rcvTeamCount = 2;
    // Returns true (and clears flag) if new options arrived since last call
    bool GetAndClearPendingOptions(bool& cr, bool& cl, bool& st, int& vl, int pc[5], bool nc[5], bool ag[5], bool& me, bool& cm, bool& dm, bool& tm, int pt[5], int& tc) {
        if (!pendingOptions) return false;
        pendingOptions = false;
        cr = rcvChainReaction; cl = rcvContinueLeave; st = rcvSingleTarget; vl = rcvVictoriesLimit;
        for (int i = 0; i < 5; i++) { pc[i] = rcvPlayerColors[i]; nc[i] = rcvNoCompress[i]; ag[i] = rcvAimGuide[i]; }
        me = rcvMouseEnabled;
        cm = rcvClearMode; dm = rcvDisableMalus;
        tm = rcvTeamMode; for (int i = 0; i < 5; i++) pt[i] = rcvPlayerTeams[i];
        tc = rcvTeamCount;
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
    std::string connectedHost;
    int connectedPort = 0;
    std::string playerNick;
    std::string playerGeoloc;

    std::deque<std::string> messageQueue;
    std::deque<std::string> syncQueue;   // Bubble-sync messages ('b|', 'N', 'T') preserved for WaitForBubble
    std::vector<GameRoom> gameList;
    std::vector<NetworkPlayer> openPlayers;
    std::vector<ChatMessage> chatMessages;
    GameRoom* currentGame;

#ifndef __WASM_PORT__
    char recvBuffer[RECV_BUFFER_SIZE];
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

    // Follow-feature capability probe (native and WASM both use this, unlike
    // the WASM-only pending* blocks below).
    NotifySupport notifySupport = NotifySupport::Unknown;
    bool pendingNotifyProbe = false;

    // WASM async CREATE state
    bool pendingCreate = false;
    std::string pendingCreateOrigNick;
    std::string pendingCreateNick;
    [[maybe_unused]] int pendingCreateSuffix = 2; // referenced only in networkclient_wasm.cpp
    [[maybe_unused]] int pendingCreateMaxPlayers = 5;  // room size chosen for the in-flight CREATE, carried across nick-suffix retries

    // WASM async JOIN state
    bool pendingJoin = false;
    std::string pendingJoinCreator;
    std::string pendingJoinOrigNick;
    std::string pendingJoinNick;
    [[maybe_unused]] int pendingJoinSuffix = 2; // referenced only in networkclient_wasm.cpp

    static NetworkClient* ptrInstance;
};

#endif // NETWORKCLIENT_H
