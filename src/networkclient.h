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
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>

#ifndef __WASM_PORT__
// Raw sockets not available in WebAssembly
#include "socket_compat.h"
#endif
#include "attackmode.h"  // AttackMode is used below regardless of platform
#include "gamemode.h"    // GameMode likewise -- see its header comment on the cycle

#define PROTO_MAJOR 1
#define PROTO_MINOR 3
#define BUFFER_SIZE 4096

// The server silently truncates every nickname to 10 characters (the NICK and
// CREATE handlers in server/game.c both do `args[10] = '\0'`). The client has
// to clamp to the same length before it stores or sends one, because it later
// matches the server's authoritative roster against its own record of who is
// in the room *by nickname*. Keeping the untruncated name locally makes that
// comparison fail against the truncated name the server echoes back, so the
// local player gets added to the room a second time as a phantom -- inflating
// the player count, drawing a duplicate board, and deadlocking the
// end-of-round handshake, which waits for a ready signal the phantom can
// never send.
#define MAX_NICK_LENGTH 10

// The accumulation buffer has to be able to hold one whole server line. The
// server formats each line into a 16384-byte buffer and can emit up to
// sizeof(buf)-1 of it (server/net.c send_line), and the LIST reply is built in a
// 16384-byte list_games_str, so a busy lobby legitimately produces lines far
// past BUFFER_SIZE. Sized above that ceiling with room for a partial line still
// waiting in front of the next one.
#define RECV_BUFFER_SIZE 32768

// Deadlines for the three phases of establishing a connection (async
// networking handoff, stage 2a-2c). Each is judged from when its own phase
// began, so a slow lookup does not eat the connect's budget and neither eats
// the handshake's. These bound how long a connection attempt can sit in
// progress -- they no longer bound how long a frame can take, which is now
// unrelated to them.
static const Uint64 kResolveTimeoutMs = 8000;      // DNS can legitimately be slow
static const Uint64 kConnectTimeoutMs = 5000;      // TCP handshake
static const Uint64 kServerReadyTimeoutMs = 3000;  // server's greeting
// How long the leader waits for every joiner to acknowledge the game start
// before starting anyway. Starting without a straggler costs that player a
// desynced board; never starting strands everybody in the lobby.
static const Uint64 kGameStartTimeoutMs = 5000;

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

// Same trust-boundary fold, for the places where "on no team at all" is a
// legal answer rather than a value to be corrected: a team number is now
// optional (see kNoTeam in netteams.h), and a player who has not joined one
// must survive the trip through OPTIONS as 0 rather than being rounded up
// into team 1.
//
// Deliberately not merged into ClampTeamNumber. That one exists so a team
// number is always safe to use as a kTeamColors index, and every caller of it
// relies on getting back something in [1, kMaxTeams]; this one hands back a
// value that must be checked against kNoTeam before it indexes anything.
inline constexpr int ClampTeamOrNone(int team) {
    if (team <= 0) return 0;              // kNoTeam
    if (team > kMaxTeams) return kMaxTeams;
    return team;
}

// The leader sends a joiner exactly this many messages to describe the level:
// 38 bubbles, plus 'N' (next bubble) and 'T' (to-be bubble).
static const size_t kLevelSyncMessageCount = 40;

// Whether a joiner should keep waiting for the leader's level sync to arrive.
//
// Deliberately a pure function, and deliberately compiled on every platform
// even though only the WASM joiner path calls it. The rule it encodes was
// wrong for two releases and nobody noticed, because it lived inline inside an
// `#ifdef __WASM_PORT__` block where no test could reach it: the gate counted
// only the main message queue, but ProcessNetworkMessages() moves 'b|'/'N'/'T'
// out of that queue and into the sync queue as it drains. In round 1 nothing
// was draining yet, so the count reached 40 and the wait ended properly. From
// round 2 on, the game loop was already draining, the count could never reach
// 40, and every single round after the first sat out the full timeout before
// starting. Counting both queues is the fix; being callable from a native test
// is what stops it silently rotting again.
inline bool ShouldKeepWaitingForLevelSync(size_t queuedMessages,
                                          size_t queuedSyncMessages,
                                          Uint64 waitedMs,
                                          Uint64 timeoutMs) {
    if (waitedMs > timeoutMs) return false;  // give up rather than strand the player
    return (queuedMessages + queuedSyncMessages) < kLevelSyncMessageCount;
}

// Ordered so that everything before CONNECTED is "still being established".
// DISCONNECTED stays 0; RESOLVING and AWAITING_READY were inserted by the
// async networking handoff (stage 2a) and nothing persists or transmits these
// values, so renumbering the later ones is safe.
enum ConnectionState {
    DISCONNECTED,
    RESOLVING,       // name lookup in flight on a worker thread (native)
    CONNECTING,      // TCP connect in flight, or WebSocket not yet open (WASM)
    AWAITING_READY,  // connected, waiting for the server's SERVER_READY banner
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

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    // Connection management
    bool Connect(const char* host, int port);
    void Disconnect();
    // True once the lobby handshake (SERVER_READY) has completed and the
    // connection is actually ready to carry commands -- CONNECTED, IN_LOBBY,
    // or IN_GAME. Deliberately false for DISCONNECTED *and* for CONNECTING:
    // every caller of this (grep confirms all ~18 of them) uses it to gate
    // sending a command, reading player/session state, or deciding whether
    // to request a fresh list -- none of them mean "in any state other than
    // fully idle." `state != DISCONNECTED` happened to be equivalent to that
    // only because Connect() resolved, connected, and handshook fully
    // synchronously, so CONNECTING was never actually observable outside of
    // it -- it was set and then immediately overwritten before Connect()
    // returned. Async networking handoff stage 2d: flagged as the highest-
    // risk single change in the whole handoff (every caller had to be
    // audited), landed on its own commit *before* any of stage 2's other
    // async-connect work, specifically so this semantic fix has its own
    // clean bisection point if something built on top of it goes wrong.
    bool IsConnected() { return state == CONNECTED || state == IN_LOBBY || state == IN_GAME; }
    // True while a connection is being established and has neither succeeded
    // nor failed yet. IsConnected() and IsConnecting() are both false when
    // DISCONNECTED, so a caller that starts a connection has three outcomes to
    // distinguish, not two -- "ready", "still trying", and "gave up".
    //
    // Getting that wrong is not hypothetical: when IsConnected() was narrowed
    // (stage 2d) the connect UI still had only a two-way branch, and WASM --
    // whose Connect() has always returned true with state CONNECTING, leaving
    // the WebSocket to open later -- fell straight into the "failed" arm and
    // stopped being able to reach a lobby at all. The third arm is what that
    // path actually needed.
    bool IsConnecting() {
        return state == RESOLVING || state == CONNECTING || state == AWAITING_READY;
    }
    ConnectionState GetState() { return state; }

    // Where we are connected (or were last asked to connect). Used to tell
    // whether a server picked out of a list is the one this connection is
    // actually talking to.
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

    // Report a player for abuse. The server appends it to a file for its
    // operator to review; nothing is enforced automatically (a nick is not an
    // identity here, so auto-acting on reports would be trivially abusable).
    // Blocking, which is local and immediate, is the other half of this --
    // see GameSettings::ToggleBlockedPlayer.
    bool SendReport(const char* nick, const char* reason);
    // Remove a player from the room. Server-side this is creator-only
    // (game.c) -- a non-host's KICK comes back as an error, so the check
    // does not rest on the client asking nicely.
    bool KickPlayer(const char* nick);

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
#ifdef __WASM_PORT__
    // Entry point for the WASM WebSocket onmessage callback. A WebSocket
    // message boundary is not a protocol-message boundary -- the websockify
    // TCP<->WebSocket bridge can split one logical line across two frames, or
    // coalesce several into one -- so this buffers a trailing partial line
    // across calls exactly like the native recv()/recvBuffer path does; see
    // ProcessIncomingData in networkclient.cpp.
    void HandleWebSocketMessage(const char* data, int numBytes);
#endif
    size_t MessageQueueSize() const { return messageQueue.size(); }

    // Bubble-sync message queue: 'b|', 'N', 'T' messages routed here by ProcessNetworkMessages
    // so WaitForBubble/WaitForNextBubble/WaitForTobeBubble can find them even if they arrived
    // before SyncNetworkLevel was called (race condition fix for round 2+).
    void PushSyncMessage(const std::string& msg) { syncQueue.push_back(msg); }
    bool HasSyncMessage() const { return !syncQueue.empty(); }
    size_t SyncQueueSize() const { return syncQueue.size(); }
    std::string GetNextSyncMessage() {
        if (syncQueue.empty()) return "";
        std::string msg = syncQueue.front();
        syncQueue.pop_front();
        return msg;
    }
    // Called by WASM open callback to transition state to CONNECTED
    void SetConnected() { state = CONNECTED; }
    // True while waiting for async CREATE OK/rejection from server (both
    // platforms as of the async networking handoff, stage 1b -- was WASM only)
    bool IsPendingCreate() const { return pendingCreate; }
    // True while waiting for async JOIN OK/rejection from server (both
    // platforms as of the async networking handoff, stage 1b -- was WASM only)
    bool IsPendingJoin() const { return pendingJoin; }
    // True while waiting for async NICK OK/rejection from server. Used by
    // MainMenu::PollGeoLocFetch() to hold off sending GEOLOC until NICK has
    // settled, preserving command order during nickname suffix retries.
    bool IsPendingNick() const { return pendingNick; }

    // Send game options to other players (host only)
    bool SendOptions(bool chainReaction, bool continueWhenLeave, bool singleTarget, int victoriesLimit, const int playerColors[5], const bool noCompress[5], const bool aimGuide[5], bool mouseEnabled, GameMode gameMode, int raceTarget, int timedSeconds, AttackMode attackMode, const int playerTeams[5], int teamCount);

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
    GameMode rcvGameMode = GameMode::Classic;
    // Values, not table indices: a room set to "first to 50" means 50 to every
    // client whatever its own kRaceTargets happens to hold, so the number is
    // what crosses the wire and each client finds its own nearest step.
    int rcvRaceTarget = kRaceTargetDefault;
    int rcvTimedSeconds = kTimedSecondsDefault;
    AttackMode rcvAttackMode = AttackMode::On;
    // No team until an OPTIONS push says otherwise -- the same default a
    // room starts every player on now.
    int rcvPlayerTeams[5] = {0, 0, 0, 0, 0};
    int rcvTeamCount = 2;
    // Returns true (and clears flag) if new options arrived since last call
    bool GetAndClearPendingOptions(bool& cr, bool& cl, bool& st, int& vl, int pc[5], bool nc[5], bool ag[5], bool& me, GameMode& gm, int& rt, int& ts, AttackMode& dm, int pt[5], int& tc) {
        if (!pendingOptions) return false;
        pendingOptions = false;
        cr = rcvChainReaction; cl = rcvContinueLeave; st = rcvSingleTarget; vl = rcvVictoriesLimit;
        for (int i = 0; i < 5; i++) { pc[i] = rcvPlayerColors[i]; nc[i] = rcvNoCompress[i]; ag[i] = rcvAimGuide[i]; }
        me = rcvMouseEnabled;
        gm = rcvGameMode; rt = rcvRaceTarget; ts = rcvTimedSeconds; dm = rcvAttackMode;
        for (int i = 0; i < 5; i++) pt[i] = rcvPlayerTeams[i];
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
    // Like Instance(), but never constructs one. The per-frame pump in
    // FrozenBubble::RunOneFrame() runs on every frame of every mode, including
    // single-player, where Instance() would otherwise allocate a client that
    // nothing ever uses.
    static NetworkClient* Existing() { return ptrInstance; }
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

#ifndef __WASM_PORT__
    // --- Async connect state machine (async networking handoff, stage 2a-2c).
    //
    // Connect() used to run name lookup, TCP connect and the SERVER_READY
    // handshake to completion before returning, blocking the render loop for
    // up to 8s plus an unbounded DNS lookup. It is now kickoff-only, and
    // PumpConnect() -- called every frame from Update() -- advances
    // RESOLVING -> CONNECTING -> AWAITING_READY -> CONNECTED.

    // Name lookup runs on a detached worker. The result lives in a shared_ptr
    // the worker co-owns, so cancelling (or destroying the client) mid-lookup
    // leaves the worker writing to memory that is still valid and simply
    // nobody's business any more -- rather than into a freed NetworkClient.
    struct PendingResolve {
        std::atomic<bool> done{false};
        std::atomic<bool> ok{false};
        // Written by the worker before `done` is set, read by the main thread
        // only after it observes `done`. That release/acquire pairing on the
        // atomic is what publishes it; no mutex needed.
        struct sockaddr_in addr {};
    };
    std::shared_ptr<PendingResolve> pendingResolve;

    // Deadlines, as absolute SDL_GetTicks() values. Zero means "not armed".
    Uint64 connectPhaseDeadline = 0;
    // Accumulates the SERVER_READY banner across reads while AWAITING_READY,
    // for the same reason the synchronous handshake had to: the banner can
    // arrive split across TCP segments.
    std::string readyBanner;

    // Advances the connect state machine by one frame's worth. No-op unless a
    // connection is being established.
    void PumpConnect();
    // Shared teardown for every way a connect can fail, so no path forgets one
    // of the four things that have to be undone.
    void FailConnect(const char* reason);

    // --- Async game start (async networking handoff, stage 3a).
    //
    // The leader must poll LEADER_CHECK_GAME_START until every joiner has
    // acknowledged, and only then send its own OK_GAME_START -- that ordering
    // is what puts everyone in prio mode before the leader starts broadcasting
    // level sync. It used to do that in a blocking loop inside a push-message
    // handler: 50 attempts of up to 200ms select() plus a 100ms sleep, so up
    // to 15 seconds of frozen render loop (its own comment claimed 5s).
    bool pendingGameStart = false;
    Uint64 gameStartDeadline = 0;
    Uint64 gameStartNextPollMs = 0;
    // Drives that poll one frame at a time. No-op unless a start is pending.
    void PumpGameStart();
    // Send OK_GAME_START and enter the game. The single place that finishes a
    // start, whether the server said everyone was ready or the deadline ran
    // out first.
    void FinishGameStart();
#endif
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
#else
    // Carries a trailing partial line across WebSocket onmessage events --
    // see HandleWebSocketMessage's declaration comment above.
    char recvBuffer[RECV_BUFFER_SIZE];
    int recvBufferLen = 0;
#endif

    unsigned char myPlayerId;  // Player ID assigned by server for game messages
    std::string myNickname;    // Our nickname for ID mapping
    std::string lastErrorResponse;  // Last error message from server
    std::map<int, std::string> playerIdToNick;  // Map of player ID to nickname

    bool ProcessIncomingData();  // Returns true if data was read, false if EWOULDBLOCK
    void HandleServerResponse(const std::string& response);
    void ParseListResponse(const char* listData);
    void HandlePushMessage(const std::string& pushMsg);

    // Async CREATE state (async networking handoff, stage 1b -- shared by
    // both platforms; was WASM-only before native's blocking SDL_Delay retry
    // loop was retired in favor of this).
    bool pendingCreate = false;
    std::string pendingCreateOrigNick;
    std::string pendingCreateNick;
    int pendingCreateSuffix = 2;
    int pendingCreateMaxPlayers = 5;  // room size chosen for the in-flight CREATE, carried across nick-suffix retries

    // Async JOIN state (same stage 1b note as CREATE above).
    bool pendingJoin = false;
    std::string pendingJoinCreator;
    std::string pendingJoinOrigNick;
    std::string pendingJoinNick;
    int pendingJoinSuffix = 2;

    // Async NICK state (same stage 1b note as CREATE above -- this one is new
    // rather than promoted from an existing WASM path: WASM's own SendNick
    // used to set playerNick optimistically and never retry NICK_IN_USE at
    // all).
    bool pendingNick = false;
    std::string pendingNickOrig;
    std::string pendingNickTry;
    int pendingNickSuffix = 2;

#ifdef FROZEN_BUBBLE_TEST_ACCESS
public:
    // Counts SendTalk calls regardless of whether the socket accepted them
    // (SendCommand no-ops when disconnected, which every headless test is).
    // Lets a test pin how many TALK messages a user action generates against
    // the server's own flood-kick threshold (server/game.c: 15 TALKs inside
    // one minute terminates the connection) -- see
    // tests/menu_touch_gesture_test.cpp's Auto-balance flood regression.
    int testTalkSendCount = 0;
private:
#endif

    // Test-only access to otherwise-private connection state, so a headless
    // test can stand up a fake "already in a game room" NetworkClient
    // without a real socket -- see NetworkClientTestAccess in
    // tests/menu_touch_gesture_test.cpp.
    friend struct NetworkClientTestAccess;

    static NetworkClient* ptrInstance;
};

#endif // NETWORKCLIENT_H
