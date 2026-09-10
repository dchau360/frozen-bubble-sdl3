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

#include "networkclient.h"
#include "netteams.h"   // kNoTeam
#include "platform.h"
#include <algorithm>
#include <cstring>
#include <errno.h>
#if !defined(_WIN32) && !defined(__WASM_PORT__)
#include <netdb.h>
#endif
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <thread>   // detached resolver worker in Connect()
#ifdef __ANDROID__
#include <jni.h>
#include <SDL3/SDL_system.h>
#endif

NetworkClient* NetworkClient::ptrInstance = nullptr;

namespace {
bool IsResponseForCommand(const std::string& response, const char* command) {
    const size_t commandStart = response.find(' ');
    if (commandStart == std::string::npos) return false;
    const size_t nameStart = commandStart + 1;
    const size_t nameEnd = response.find(':', nameStart);
    return nameEnd != std::string::npos &&
           response.compare(nameStart, nameEnd - nameStart, command) == 0;
}
}  // namespace

#ifndef __WASM_PORT__
namespace {
// Write the whole buffer, or report failure. On Windows the socket is
// non-blocking (see Connect), so send() may accept only part of a message.
// This protocol is newline-framed, so a half-written line does not merely go
// missing -- it leaves the server parsing the next one at the wrong offset.
// Returns len, or -1.
ssize_t SendAll(int fd, const char* data, size_t len) {
    size_t offset = 0;
    int stalls = 0;
    while (offset < len) {
        ssize_t n = send(fd, data + offset, len - offset, MSG_NOSIGNAL);
        if (n > 0) {
            offset += (size_t)n;
            stalls = 0;
            continue;
        }
#ifdef _WIN32
        const bool retryable = (n < 0 && SOCK_ERRNO == WSAEWOULDBLOCK);
#else
        const bool retryable = (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR));
#endif
        // Give a briefly-full send buffer time to drain, but never spin
        // indefinitely on a peer that has stopped reading.
        if (!retryable || ++stalls > 500)
            return -1;
        SDL_Delay(1);
    }
    return (ssize_t)len;
}
}  // namespace

NetworkClient::NetworkClient()
    : sockfd(-1), state(DISCONNECTED), currentGame(nullptr), recvBufferLen(0), myPlayerId(0) {
    SDL_Log("NetworkClient constructor called");
}

NetworkClient::~NetworkClient() {
    Disconnect();
}
#endif // __WASM_PORT__

NetworkClient* NetworkClient::Instance(const char* host, int port) {
    if (ptrInstance == nullptr) {
        ptrInstance = new NetworkClient();
    }

    // If host and port are provided and we're not connected, connect
    if (host != nullptr && port > 0 && ptrInstance->state == DISCONNECTED) {
        ptrInstance->Connect(host, port);
    }

    return ptrInstance;
}

void NetworkClient::Dispose() {
    if (ptrInstance != nullptr) {
        delete ptrInstance;
        ptrInstance = nullptr;
    }
}

#ifndef __WASM_PORT__
// Kickoff only. Name lookup, the TCP connect and the SERVER_READY handshake
// all used to run to completion right here, which meant a single ENTER on
// "connect" could freeze the render loop for up to 8 seconds plus an
// unbounded DNS lookup -- no repaint, no cancel, and on macOS the OS painting
// the window as "not responding". PumpConnect() now advances the phases from
// the main loop instead (async networking handoff, stage 2a-2c).
//
// The return value means "the attempt started", NOT "we are connected" --
// callers decide what happened by looking at IsConnected() / IsConnecting()
// on this and later frames. That is the contract WASM's Connect() has always
// had, so both platforms now behave the same way here.
bool NetworkClient::Connect(const char* host, int port) {
    if (state != DISCONNECTED) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Already connected or connecting");
        return false;
    }

    connectedHost = host ? host : "";
    connectedPort = port;
    readyBanner.clear();

    socket_init();

    // Name lookup goes to a detached worker: getaddrinfo() has no portable
    // async form and no timeout of its own, so on a slow or unreachable DNS
    // server it blocks for as long as the resolver library feels like it.
    //
    // The worker writes into a shared_ptr it co-owns rather than into this
    // object. If the connection is cancelled -- or the client destroyed --
    // while the lookup is still running, the worker finishes writing into
    // memory that is still perfectly valid and simply no longer anybody's
    // business; the block is freed when the last of the two owners drops it.
    // Passing `this` instead would be a use-after-free waiting for a slow DNS
    // server to trigger it.
    auto resolve = std::make_shared<PendingResolve>();
    pendingResolve = resolve;
    const std::string hostCopy = connectedHost;
    const int portCopy = port;
    std::thread([resolve, hostCopy, portCopy]() {
        struct addrinfo hints;
        struct addrinfo* res = nullptr;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char portStr[8];
        snprintf(portStr, sizeof(portStr), "%d", portCopy);

        bool ok = false;
        if (getaddrinfo(hostCopy.c_str(), portStr, &hints, &res) == 0 && res) {
            memcpy(&resolve->addr, res->ai_addr, sizeof(resolve->addr));
            ok = true;
        }
        if (res) freeaddrinfo(res);

        resolve->ok.store(ok);
        // Written last, and read first on the other side: everything above
        // happens-before the main thread's read of `addr`.
        resolve->done.store(true);
    }).detach();

    state = RESOLVING;
    connectPhaseDeadline = SDL_GetTicks() + kResolveTimeoutMs;
    SDL_Log("Connecting to %s:%d (resolving)", connectedHost.c_str(), port);
    return true;
}

// Everything a failed connect has to undo, in one place. Each phase used to
// repeat this inline and it was already inconsistent between them.
void NetworkClient::FailConnect(const char* reason) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Connect to %s:%d failed: %s",
                 connectedHost.c_str(), connectedPort, reason);
    if (sockfd >= 0) {
        SOCKET_CLOSE(sockfd);
        sockfd = -1;
    }
    // Dropping our reference does not disturb a resolver worker still running;
    // it just stops us caring about the answer.
    pendingResolve.reset();
    connectPhaseDeadline = 0;
    readyBanner.clear();
    state = DISCONNECTED;
}

// One frame's worth of progress. Returns immediately unless a connection is
// actually being established, so it is safe to call unconditionally.
void NetworkClient::PumpConnect() {
    if (!IsConnecting()) return;

    if (connectPhaseDeadline != 0 && SDL_GetTicks() > connectPhaseDeadline) {
        // Phrased by phase: "timed out" during AWAITING_READY means the server
        // accepted us and then said nothing, which is a very different
        // diagnosis from a TCP connect that never completed.
        FailConnect(state == RESOLVING     ? "name lookup timed out"
                    : state == CONNECTING  ? "connect timed out"
                                           : "no SERVER_READY from server");
        return;
    }

    if (state == RESOLVING) {
        if (!pendingResolve || !pendingResolve->done.load()) return;  // still looking
        if (!pendingResolve->ok.load()) {
            FailConnect("could not resolve host");
            return;
        }

        struct sockaddr_in serverAddr = pendingResolve->addr;
        pendingResolve.reset();

        sockfd = (int)socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == (int)INVALID_SOCKET) {
            sockfd = -1;
            FailConnect("could not create socket");
            return;
        }

        // Bursty traffic (level sync) arrives faster than a frame can drain it.
        int rcvbuf = 256 * 1024;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, SETSOCKOPT_OPTVAL(rcvbuf), sizeof(rcvbuf)) < 0)
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to set SO_RCVBUF: %d", SOCK_ERRNO);

        // Non-blocking before connect(), for two reasons. One: a blocking
        // connect() to an unreachable host waits on the OS's own timeout,
        // which is not this codebase's to choose (tens of seconds, sometimes
        // minutes). Two: SendAll's retry loop already expects EWOULDBLOCK when
        // the send buffer fills and a peer has stopped reading; on a blocking
        // socket that never happens and send() simply parks in the kernel with
        // no bound, making that retry branch dead code.
#ifdef _WIN32
        {
            u_long nonblocking = 1;
            if (ioctlsocket(sockfd, FIONBIO, &nonblocking) != 0)
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Could not set the connection non-blocking: %d", SOCK_ERRNO);
        }
#else
        {
            int flags = fcntl(sockfd, F_GETFL, 0);
            if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Could not set the connection non-blocking: %d", SOCK_ERRNO);
        }
#endif

        const int result = connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
#ifdef _WIN32
        const bool pending = (result < 0 && SOCK_ERRNO == WSAEWOULDBLOCK);
#else
        const bool pending = (result < 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK));
#endif
        if (result < 0 && !pending) {
            FailConnect("connect refused");
            return;
        }

        if (result == 0) {
            // Completed immediately -- normal on loopback.
            state = AWAITING_READY;
            connectPhaseDeadline = SDL_GetTicks() + kServerReadyTimeoutMs;
            return;
        }

        state = CONNECTING;
        connectPhaseDeadline = SDL_GetTicks() + kConnectTimeoutMs;
        return;
    }

    if (state == CONNECTING) {
        // Poll, never wait: a zero timeout makes this a question about right
        // now rather than a place the frame can get stuck.
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(sockfd, &wfds);
        struct timeval noWait{0, 0};
        if (select(sockfd + 1, nullptr, &wfds, nullptr, &noWait) <= 0) return;  // still in flight

        // Writability only says the attempt finished, not that it succeeded --
        // a refused connection also completes and also becomes writable, so
        // SO_ERROR is what actually distinguishes them. Treating writability
        // alone as success is what used to list dead servers as online.
        int soErr = 0;
        socklen_t soErrLen = sizeof(soErr);
#ifdef _WIN32
        char* soErrPtr = reinterpret_cast<char*>(&soErr);
#else
        void* soErrPtr = &soErr;
#endif
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, soErrPtr, &soErrLen) != 0 || soErr != 0) {
            FailConnect("connection refused or reset");
            return;
        }

        state = AWAITING_READY;
        connectPhaseDeadline = SDL_GetTicks() + kServerReadyTimeoutMs;
        return;
    }

    // AWAITING_READY: drain whatever has arrived and look for the banner.
    char buffer[BUFFER_SIZE];
    for (;;) {
        const ssize_t received = recv(sockfd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        if (received == 0) {
            FailConnect("server closed the connection during handshake");
            return;
        }
        if (received < 0) {
            if (SOCK_WOULD_BLOCK(SOCK_ERRNO)) break;  // nothing more this frame
            FailConnect("receive error during handshake");
            return;
        }

        // Accumulated across reads, and across frames. The banner is one short
        // line, so on loopback it arrives whole and any per-read parsing looks
        // correct -- but a real network, a proxy or a WebSocket bridge can
        // split it anywhere, and then a per-read parser sees neither half
        // contain SERVER_READY and declares a working server unreachable.
        // Covered by tests/fake_server.h's Banner::Split, which splits
        // mid-token precisely because a token-boundary split would not catch
        // it.
        readyBanner.append(buffer, (size_t)received);
        if (readyBanner.find("SERVER_READY") != std::string::npos) break;

        // A peer that streams bytes forever without greeting must not grow
        // this without bound. Keep a trailing window so a token straddling the
        // boundary still matches.
        if (readyBanner.size() > 8192) readyBanner.erase(0, readyBanner.size() - 1024);
    }

    if (readyBanner.find("SERVER_READY") == std::string::npos) return;  // keep waiting

    // Whatever followed the banner belongs to the ordinary message stream, not
    // to the handshake: the server pipelines its first push messages right
    // behind SERVER_READY, and this reads by byte count rather than by line.
    // Hand it to the buffered reader instead of dropping it.
    const size_t bannerEnd = readyBanner.find('\n');
    if (bannerEnd != std::string::npos && bannerEnd + 1 < readyBanner.size()) {
        const std::string leftover = readyBanner.substr(bannerEnd + 1);
        if (leftover.size() < RECV_BUFFER_SIZE) {
            memcpy(recvBuffer, leftover.data(), leftover.size());
            recvBufferLen = (int)leftover.size();
        }
    }
    readyBanner.clear();
    connectPhaseDeadline = 0;
    state = CONNECTED;
    SDL_Log("Connected to server %s:%d", connectedHost.c_str(), connectedPort);
}
#endif // __WASM_PORT__ (Connect)

#ifndef __WASM_PORT__
void NetworkClient::Disconnect() {
    SDL_Log("!!! DISCONNECT CALLED - State was: %d, sockfd: %d", state, sockfd);
    if (sockfd >= 0) {
        SOCKET_CLOSE(sockfd);
        sockfd = -1;
    }
    state = DISCONNECTED;
    delete currentGame;
    currentGame = nullptr;
    gameList.clear();
    messageQueue.clear();
    syncQueue.clear();
    // Without this the next connection inherits whatever partial line was in
    // flight when this one dropped, so it starts parsing at the wrong offset --
    // and a buffer left full stayed full, carrying the deaf state across the
    // reconnect that was supposed to clear it.
    recvBufferLen = 0;
    // A NICK/CREATE/JOIN in flight when the connection drops must not stay
    // "pending" into the next connection -- the wire protocol carries no
    // request id, so a stale pending flag would misattribute the new
    // connection's first unrelated OK as this one's answer. Native didn't
    // need this before the async networking handoff, stage 1b (these three
    // flags were WASM-only and its own Disconnect() already clears
    // pendingCreate/pendingJoin, though it was missing pendingNick too --
    // fixed alongside this).
    pendingNick = false;
    pendingCreate = false;
    pendingJoin = false;
    // Abandon any connection attempt still in flight (async networking
    // handoff, stage 2a-2c). Dropping our reference to the resolve slot does
    // not disturb a worker still running in it -- the worker co-owns the
    // block, so it writes its answer into memory that stays valid and simply
    // stops being anybody's business. This is also the ESC-cancel path.
    pendingResolve.reset();
    connectPhaseDeadline = 0;
    readyBanner.clear();
    // Likewise abandon a game start still being polled for (stage 3a) -- with
    // the socket gone there is nobody left to answer, and leaving the flag set
    // would have the next connection's first frames poll on its behalf.
    pendingGameStart = false;
    gameStartDeadline = 0;
    gameStartNextPollMs = 0;
    SDL_Log("Disconnected from server");
}

bool NetworkClient::SendCommand(const char* command) {
    if (state == DISCONNECTED || sockfd < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Not connected to server");
        return false;
    }

    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "FB/%d.%d %s\n", PROTO_MAJOR, PROTO_MINOR, command);

    ssize_t sent = SendAll(sockfd, buffer, strlen(buffer));
    if (sent < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send command: %s", strerror(errno));
        Disconnect();
        return false;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Sent: %s", command);

    // Fire-and-forget as of the async networking handoff, stage 1c. This used
    // to block here for up to 100ms on a select()+recv(), feeding whatever
    // came back through strtok() directly -- which, unlike
    // ProcessIncomingData()'s buffered reader, has no memory across calls: a
    // line split across two recv()s (or a partial line left over after the
    // last complete one) was silently mishandled, violating stream semantics
    // (audit BUG-017's inbound half). The reply now arrives through the same
    // path every other inbound byte does -- ProcessIncomingData(), driven by
    // MainMenu::PumpNetworkFrame()'s unconditional per-frame Update() call
    // (stage 1a) -- typically within the same frame on a fast localhost link,
    // and within one round trip otherwise. This is only safe now that stage
    // 1b moved every pending-flag write to before its SendCommand() call --
    // callers used to rely on this inline read to populate lastErrorResponse
    // synchronously before they returned.
    return true;
}
#endif // __WASM_PORT__ (Disconnect, SendCommand)

bool NetworkClient::SendNick(const char* nickname) {
    // Fire-and-forget on both platforms (async networking handoff, stage 1b).
    // This used to be a blocking 20-retry loop on native (up to ~3s: each
    // iteration sent NICK, then slept 50ms on top of SendCommand's own 100ms
    // inline read, waiting to see whether lastErrorResponse came back
    // NICK_IN_USE) while WASM fired once and never retried a collision at
    // all. Both are replaced by the same async pattern CreateGame/JoinGame
    // already used on WASM: send once, record pending state, and let
    // HandleServerResponse()'s OK/NICK_IN_USE handling (below) retry with a
    // numeric suffix or confirm the final nick once it sticks.
    //
    // Clamp to what the server will actually keep, before storing or
    // sending: the roster it echoes back is truncated, and an untruncated
    // local copy fails to match it (see MAX_NICK_LENGTH in networkclient.h).
    std::string originalNick = std::string(nickname).substr(0, MAX_NICK_LENGTH);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "NICK %s", originalNick.c_str());

    // Pending state is set BEFORE SendCommand(), not after. This ordering
    // was load-bearing under stage 1b: native's SendCommand() used to do its
    // own inline 100ms select()+recv() and could synchronously drive the
    // server's reply through HandleServerResponse() before this function
    // returned -- reliably so on a fast localhost round trip. Setting the
    // flag only after SendCommand() returned meant a NICK_IN_USE arriving
    // inside that inline read would find no pending flag set, get silently
    // dropped, and leave pendingNick stuck true forever with no retry ever
    // sent. Caught by a real end-to-end test against a live server
    // (menu_touch_gesture_test.cpp) racing a second connection for the same
    // nick -- a synthetic/mocked test could not have surfaced this, since it
    // depended on a genuinely synchronous response through SendCommand().
    // Stage 1c has since deleted that inline read (SendCommand() is now
    // strictly fire-and-forget, replies arrive only via the next Update()),
    // so this exact race can no longer happen -- kept set-before-send anyway
    // as the safer default rather than re-introducing an ordering dependency
    // for no benefit.
    pendingNick = true;
    pendingNickOrig = originalNick;
    pendingNickTry = originalNick;
    pendingNickSuffix = 2;
    // Set optimistically rather than waiting for confirmation (matching
    // WASM's pre-existing SendNick behavior) -- CreateGame/JoinGame and a lot
    // of render code read playerNick, and some of those can run before the
    // OK for this NICK has come back (e.g. a room screen shown right after
    // sending NICK, or a chat command that fires SendNick then immediately
    // reads GetPlayerNick() for its confirmation message). A NICK_IN_USE
    // retry below corrects this to the final suffixed nick if needed.
    playerNick = originalNick;
    myNickname = originalNick;

    if (!SendCommand(cmd)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send NICK command");
        pendingNick = false;
        return false;
    }
    return true;
}

bool NetworkClient::SendGeoLoc(const char* location) {
    playerGeoloc = location;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "GEOLOC %s", location);
    return SendCommand(cmd);
}

bool NetworkClient::CreateGame(int maxPlayers) {
    // CREATE requires a game name argument (uses player's nickname).
    // Async on both platforms (async networking handoff, stage 1b) -- this
    // used to be a blocking 20-retry loop on native (up to ~3s) that set up
    // currentGame/state synchronously on "success", while WASM fired once and
    // deferred that setup to HandleServerResponse()'s pendingCreate handling
    // below. Native now takes the same path: state/currentGame are set up
    // only once the server's real OK arrives. Do NOT optimistically set
    // state=IN_LOBBY here -- that caused a "phantom game" bug where the
    // client got stuck in a create-game view after server rejection.
    std::string originalNick = playerNick.substr(0, MAX_NICK_LENGTH);
    SDL_Log("CreateGame: sending CREATE %s %d", originalNick.c_str(), maxPlayers);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "CREATE %s %d", originalNick.c_str(), maxPlayers);

    // Pending state set BEFORE SendCommand() -- see SendNick()'s comment on
    // this same ordering for why (a stage 1b race, closed by stage 1c
    // deleting SendCommand()'s inline read; kept as the safer default).
    pendingCreate = true;
    pendingCreateOrigNick = originalNick;
    pendingCreateNick = originalNick;
    pendingCreateSuffix = 2;
    pendingCreateMaxPlayers = maxPlayers;

    if (!SendCommand(cmd)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CreateGame: SendCommand failed");
        pendingCreate = false;
        return false;
    }
    return true;  // state/currentGame set later when server sends OK
}

bool NetworkClient::JoinGame(const char* creator) {
    // JOIN requires creator_nick and player_nick. Async on both platforms
    // (same stage 1b note as CreateGame above) -- native's blocking 20-retry
    // loop and its synchronous currentGame setup on "success" are gone;
    // HandleServerResponse()'s pendingJoin handling below does both once the
    // server actually answers.
    std::string originalNick = playerNick;
    SDL_Log("JoinGame: sending JOIN %s %s", creator, originalNick.c_str());
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "JOIN %s %s", creator, originalNick.c_str());

    // Pending state set BEFORE SendCommand() -- see SendNick()'s comment on
    // this same ordering for why.
    pendingJoin = true;
    pendingJoinCreator = std::string(creator);
    pendingJoinOrigNick = originalNick;
    pendingJoinNick = originalNick;
    pendingJoinSuffix = 2;

    if (!SendCommand(cmd)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "JoinGame: SendCommand failed");
        pendingJoin = false;
        return false;
    }
    return true;  // state/currentGame set later when server sends OK
}

bool NetworkClient::StartGame() {
    // Send START command to server
    // Server will respond with GAME_CAN_START push message when ready
    // Don't change state here - wait for GAME_CAN_START
    if (SendCommand("START")) {
        SDL_Log("Sent START command to server, waiting for GAME_CAN_START...");
        return true;
    }
    return false;
}

bool NetworkClient::PartGame() {
    if (SendCommand("PART")) {
        state = IN_LOBBY;  // Still registered with nick on server, back to lobby
        delete currentGame;
        currentGame = nullptr;
        return true;
    }
    return false;
}

bool NetworkClient::SendTalk(const char* message) {
#ifdef FROZEN_BUBBLE_TEST_ACCESS
    ++testTalkSendCount;
#endif
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TALK %s", message);
    return SendCommand(cmd);
}

bool NetworkClient::SendReport(const char* nick, const char* reason) {
    if (!nick || !*nick || !reason || !*reason) return false;
    // The server splits on the first space to separate nick from reason, so a
    // nick containing one would silently report a truncated name with the
    // remainder folded into the reason text. Nicks can't contain spaces
    // anyway (the server's own NICK handling truncates at one), so this only
    // rejects input that was already malformed.
    if (strchr(nick, ' ') != nullptr) return false;
    // The protocol is line-oriented and SendCommand appends the terminator
    // itself, so any control character the caller smuggles in splits one
    // REPORT into several lines -- the server would execute the tail as its
    // own command. The reason text comes straight from the chat box, which
    // accepts clipboard paste, so this is reachable without a modified client.
    for (const char* p = nick; *p; ++p)
        if ((unsigned char)*p < 0x20 || *p == 0x7f) return false;
    for (const char* p = reason; *p; ++p)
        if ((unsigned char)*p < 0x20 || *p == 0x7f) return false;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "REPORT %s %s", nick, reason);
    return SendCommand(cmd);
}

bool NetworkClient::KickPlayer(const char* nick) {
    if (!nick || !*nick) return false;
    // Same line-protocol hygiene as SendReport: the nickname reaches here
    // from a chat box that accepts paste, and SendCommand appends the
    // terminator itself, so an embedded control character would split one
    // KICK into two lines and have the server run the tail as a command.
    // A space would truncate the name server-side (KICK splits on the first
    // one), and nicknames cannot contain one anyway.
    if (strchr(nick, ' ') != nullptr) return false;
    for (const char* p = nick; *p; ++p)
        if ((unsigned char)*p < 0x20 || *p == 0x7f) return false;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "KICK %s", nick);
    return SendCommand(cmd);
}

bool NetworkClient::SendOptions(bool chainReaction, bool continueWhenLeave, bool singleTarget, int victoriesLimit, const int playerColors[5], const bool noCompress[5], const bool aimGuide[5], bool mouseEnabled, GameMode gameMode, int raceTarget, int timedSeconds, AttackMode attackMode, const int playerTeams[5], int teamCount) {
    // Send game options using SETOPTIONS command (original line 4468-4474)
    // Format: SETOPTIONS CHAINREACTION:0/1,...,NUMCOLORS_P1:N,...,NUMCOLORS_P5:N
    char cmd[768];
    snprintf(cmd, sizeof(cmd),
             "SETOPTIONS CHAINREACTION:%d,CONTINUEGAMEWHENPLAYERSLEAVE:%d,SINGLEPLAYERTARGETTING:%d,VICTORIESLIMIT:%d"
             ",NUMCOLORS_P1:%d,NUMCOLORS_P2:%d,NUMCOLORS_P3:%d,NUMCOLORS_P4:%d,NUMCOLORS_P5:%d"
             ",NOCOMPRESS_P1:%d,NOCOMPRESS_P2:%d,NOCOMPRESS_P3:%d,NOCOMPRESS_P4:%d,NOCOMPRESS_P5:%d"
             ",AIMGUIDE_P1:%d,AIMGUIDE_P2:%d,AIMGUIDE_P3:%d,AIMGUIDE_P4:%d,AIMGUIDE_P5:%d"
             // DISABLEMALUS keeps its original 0/1 meaning so a client built
             // before canceling existed still reads "on" vs "off" correctly;
             // MALUSCANCEL rides alongside it and is simply not found by that
             // client's parser, which falls back to its default of 0. The
             // degradation is that such a client attacks without cancelling
             // -- a rule difference, not a desync.
             // TEAMMODE no longer goes out: teams are a per-player setting in
             // every mode now (mainmenu_teampanel.cpp), not a whole-room flag,
             // so there is nothing left for it to carry. Only this client's
             // own code ever read it (the server relays SETOPTIONS as opaque
             // text), so dropping it is not a wire break -- an older build
             // reading this room's OPTIONS simply finds no TEAMMODE key and
             // falls back to its own default of "off", the same as it does
             // today for any key it predates.
             // CLEARMODE keeps its original 0/1 meaning -- set only for
             // GameMode::Clear -- so a client built before Race and Timed
             // existed still reads a Clear room as Clear, and reads a Race or
             // Timed room as Classic rather than as something it cannot name.
             // GAMEMODE rides alongside it with the full four-way value and is
             // simply not found by that client's parser. Same shape, and the
             // same kind of degradation, as MALUSCANCEL above.
             ",MOUSEENABLED:%d,CLEARMODE:%d,GAMEMODE:%d,RACETARGET:%d,TIMELIMIT:%d"
             ",DISABLEMALUS:%d,MALUSCANCEL:%d"
             ",TEAMCOUNT:%d,PLAYERTEAM_P1:%d,PLAYERTEAM_P2:%d,PLAYERTEAM_P3:%d,PLAYERTEAM_P4:%d,PLAYERTEAM_P5:%d",
             chainReaction ? 1 : 0,
             continueWhenLeave ? 1 : 0,
             singleTarget ? 1 : 0,
             victoriesLimit,
             playerColors[0], playerColors[1], playerColors[2], playerColors[3], playerColors[4],
             noCompress[0] ? 1 : 0, noCompress[1] ? 1 : 0, noCompress[2] ? 1 : 0, noCompress[3] ? 1 : 0, noCompress[4] ? 1 : 0,
             aimGuide[0] ? 1 : 0, aimGuide[1] ? 1 : 0, aimGuide[2] ? 1 : 0, aimGuide[3] ? 1 : 0, aimGuide[4] ? 1 : 0,
             mouseEnabled ? 1 : 0, gameMode == GameMode::Clear ? 1 : 0,
             (int)gameMode, raceTarget, timedSeconds,
             attackMode == AttackMode::Off ? 1 : 0,
             attackMode == AttackMode::Canceling ? 1 : 0,
             teamCount, playerTeams[0], playerTeams[1], playerTeams[2], playerTeams[3], playerTeams[4]);
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Sending game options: %s", cmd);
    return SendCommand(cmd);
}

#ifndef __WASM_PORT__
bool NetworkClient::SendGameData(const char* data) {
    // Game messages use binary protocol: {myid byte}{data}\n
    // NOT the FB/1.2 prefix format!
    if (state != IN_GAME || sockfd < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Not in game or not connected");
        return false;
    }

    if (myPlayerId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Player ID not set - cannot send game data");
        return false;
    }

    char buffer[BUFFER_SIZE];
    buffer[0] = (char)myPlayerId;  // First byte is player ID (binary)

    // Format the message part (text + newline)
    int msgLen = snprintf(buffer + 1, sizeof(buffer) - 1, "%s\n", data);
    if (msgLen < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to format game data");
        return false;
    }

    // Total length = 1 byte (player ID) + message length
    size_t len = 1 + msgLen;

    // Don't log ping messages to avoid spam (sent every second)
    bool isPing = (strcmp(data, "p") == 0);

    // Log the exact bytes being sent for debugging
    if (!isPing) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, ">>> Sending game data: [ID=%d] %s (total %zu bytes: 1 byte ID + %d bytes msg)",
                (int)myPlayerId, data, len, msgLen);
    }

    ssize_t sent = SendAll(sockfd, buffer, len);
    if (sent < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send game data: %s", strerror(errno));
        Disconnect();
        return false;
    }

    if (sent != (ssize_t)len) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Partial send: %zd of %zu bytes", sent, len);
    }

    if (!isPing) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, ">>> Successfully sent game data: [ID=%d] %s", (int)myPlayerId, data);
    }
    return true;
}
#endif // __WASM_PORT__ (SendGameData)

bool NetworkClient::RequestList() {
    return SendCommand("LIST");
}

bool NetworkClient::IsLeader() {
    // We're the leader if we created the game (we are the creator)
    if (!currentGame) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "IsLeader: No current game");
        return false;
    }
    bool isLeader = (currentGame->creator == playerNick);
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "IsLeader: creator='%s', playerNick='%s', result=%s",
            currentGame->creator.c_str(), playerNick.c_str(), isLeader ? "true" : "false");
    return isLeader;
}

bool NetworkClient::SendBubble(int cx, int cy, int bubbleId) {
    // Leader sends bubble position: b|cx|cy{bubbleId}
    char msg[64];
    snprintf(msg, sizeof(msg), "b|%d|%d%d", cx, cy, bubbleId);
    return SendGameData(msg);
}

bool NetworkClient::SendNextBubble(int bubbleId) {
    // Leader sends next bubble: N{bubbleId}
    char msg[32];
    snprintf(msg, sizeof(msg), "N%d", bubbleId);
    return SendGameData(msg);
}

bool NetworkClient::SendTobeBubble(int bubbleId) {
    // Leader sends tobe bubble: T{bubbleId}
    char msg[32];
    snprintf(msg, sizeof(msg), "T%d", bubbleId);
    return SendGameData(msg);
}

bool NetworkClient::WaitForBubble(int& cx, int& cy, int& bubbleId) {
    // Joiner waits for bubble message: b|cx|cy{bubbleId}
    // Checks syncQueue first (pre-routed by ProcessNetworkMessages), then main queue.
    Uint64 timeout = 5000;  // 5 second timeout
    Uint64 startTime = SDL_GetTicks();
    std::vector<std::string> deferredMessages; // Collect non-matching messages

    auto tryParse = [&](const std::string& msg) -> bool {
        if (msg.find("GAMEMSG:") != 0) return false;
        int senderId; char gameData[512];
        if (sscanf(msg.c_str(), "GAMEMSG:%d:%511[^\n]", &senderId, gameData) != 2) return false;
        if (gameData[0] != 'b' || gameData[1] != '|') {
            deferredMessages.push_back(msg);
            return false;
        }
        char* data = gameData + 2;
        char cyBubble[16];
        if (sscanf(data, "%d|%15s", &cx, cyBubble) == 2 && strlen(cyBubble) >= 2) {
            cy = cyBubble[0] - '0';
            bubbleId = atoi(cyBubble + 1);
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "WaitForBubble: Received bubble: cx=%d cy=%d id=%d", cx, cy, bubbleId);
            return true;
        }
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WaitForBubble: Failed to parse bubble data: %s", data);
        return false;
    };

    while (SDL_GetTicks() - startTime < timeout) {
        Update();  // Process incoming data

        // Drain syncQueue first (messages pre-buffered by ProcessNetworkMessages)
        while (HasSyncMessage()) {
            std::string msg = GetNextSyncMessage();
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "WaitForBubble: Got sync-queued message: %s", msg.c_str());
            if (tryParse(msg)) {
                for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it)
                    PutBackMessage(*it);
                return true;
            }
        }

        // Also drain main message queue
        while (HasMessage()) {
            std::string msg = GetNextMessage();
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "WaitForBubble: Got message: %s", msg.c_str());
            if (tryParse(msg)) {
                for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it)
                    PutBackMessage(*it);
                return true;
            }
        }

        SDL_Delay(10);
    }

    // Timeout - put back deferred messages
    for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it) {
        PutBackMessage(*it);
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Timeout waiting for bubble message");
    return false;
}

bool NetworkClient::WaitForNextBubble(int& bubbleId) {
    // Joiner waits for next bubble: N{bubbleId}
    Uint64 timeout = 5000;
    Uint64 startTime = SDL_GetTicks();
    std::vector<std::string> deferredMessages;

    auto tryParse = [&](const std::string& msg) -> bool {
        if (msg.find("GAMEMSG:") != 0) return false;
        int senderId; char gameData[512];
        if (sscanf(msg.c_str(), "GAMEMSG:%d:%511[^\n]", &senderId, gameData) != 2) return false;
        if (gameData[0] != 'N') { deferredMessages.push_back(msg); return false; }
        if (sscanf(gameData + 1, "%d", &bubbleId) == 1) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "WaitForNextBubble: Received next bubble: id=%d", bubbleId);
            return true;
        }
        return false;
    };

    while (SDL_GetTicks() - startTime < timeout) {
        Update();

        while (HasSyncMessage()) {
            std::string msg = GetNextSyncMessage();
            if (tryParse(msg)) {
                for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it)
                    PutBackMessage(*it);
                return true;
            }
        }
        while (HasMessage()) {
            std::string msg = GetNextMessage();
            if (tryParse(msg)) {
                for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it)
                    PutBackMessage(*it);
                return true;
            }
        }
        SDL_Delay(10);
    }

    for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it)
        PutBackMessage(*it);
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Timeout waiting for next bubble");
    return false;
}

bool NetworkClient::WaitForTobeBubble(int& bubbleId) {
    // Joiner waits for tobe bubble: T{bubbleId}
    Uint64 timeout = 5000;
    Uint64 startTime = SDL_GetTicks();
    std::vector<std::string> deferredMessages;

    auto tryParse = [&](const std::string& msg) -> bool {
        if (msg.find("GAMEMSG:") != 0) return false;
        int senderId; char gameData[512];
        if (sscanf(msg.c_str(), "GAMEMSG:%d:%511[^\n]", &senderId, gameData) != 2) return false;
        if (gameData[0] != 'T') { deferredMessages.push_back(msg); return false; }
        if (sscanf(gameData + 1, "%d", &bubbleId) == 1) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "WaitForTobeBubble: Received tobe bubble: id=%d", bubbleId);
            return true;
        }
        return false;
    };

    while (SDL_GetTicks() - startTime < timeout) {
        Update();

        while (HasSyncMessage()) {
            std::string msg = GetNextSyncMessage();
            if (tryParse(msg)) {
                for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it)
                    PutBackMessage(*it);
                return true;
            }
        }
        while (HasMessage()) {
            std::string msg = GetNextMessage();
            if (tryParse(msg)) {
                for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it)
                    PutBackMessage(*it);
                return true;
            }
        }
        SDL_Delay(10);
    }

    for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it)
        PutBackMessage(*it);
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Timeout waiting for tobe bubble");
    return false;
}

void NetworkClient::AddStatusMessage(const std::string& message) {
    ChatMessage statusMsg;
    statusMsg.nick = "Server";
    statusMsg.message = message;
    statusMsg.timestamp = SDL_GetTicks();
    chatMessages.push_back(statusMsg);

    // Keep only last 50 messages
    if (chatMessages.size() > 50) {
        chatMessages.erase(chatMessages.begin());
    }
}

#ifndef __WASM_PORT__
void NetworkClient::Update() {
    // Advance a connection that is still being established. This has to come
    // before the guard below, not after: while RESOLVING there is no socket
    // yet (sockfd is -1), so the guard would return before the state machine
    // ever ran and the connection would never progress past its first phase.
    PumpConnect();

    if (state == DISCONNECTED || sockfd < 0) return;
    // Nothing below applies until the handshake is done -- and reading from
    // the socket during AWAITING_READY would steal the banner bytes out from
    // under PumpConnect().
    if (IsConnecting()) return;

    // Read all available data to prevent socket buffer from filling up
    // The server will disconnect us if it can't send (buffer full)
    // Keep reading until EWOULDBLOCK (no more data available)
    int readsThisFrame = 0;
    while (readsThisFrame < 100) {  // Safety limit
        if (!ProcessIncomingData()) {
            break;  // No more data available
        }
        readsThisFrame++;
    }
    if (readsThisFrame > 10) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Read %d packets in one frame; network buffer was filling up", readsThisFrame);
    }

    // After the reads, so a LEADER_CHECK_GAME_START answer that arrived this
    // frame is acted on before we decide whether to send another poll.
    PumpGameStart();
}

bool NetworkClient::ProcessIncomingData() {
    char tempBuffer[BUFFER_SIZE];
    ssize_t received = recv(sockfd, tempBuffer, sizeof(tempBuffer) - 1, MSG_DONTWAIT);

    if (received < 0) {
        int sockerr = SOCK_ERRNO;
        if (!SOCK_WOULD_BLOCK(sockerr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Receive error: %d", sockerr);
            Disconnect();
        }
        return false;  // No data available
    }

    if (received == 0) {
        SDL_Log("Server closed connection");
        Disconnect();
        return false;
    }

    // Append to buffer (don't null-terminate yet for binary data)
    if (recvBufferLen + received >= RECV_BUFFER_SIZE) {
        // Every statement that shrinks recvBufferLen lives inside the append
        // branch below, so without this the full state was absorbing: once the
        // buffer filled, the connection was deaf for the rest of its life and
        // every later read was silently discarded -- the lobby stopped updating
        // and, in game, peer frames vanished while boards quietly diverged.
        // A line this long with no newline in it is not something this protocol
        // produces, so treat it the way the server treats the same condition and
        // drop the connection rather than pretending to be connected.
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Receive buffer overflowed with no complete line (%d + %d >= %d); disconnecting",
                     recvBufferLen, (int)received, RECV_BUFFER_SIZE);
        Disconnect();
        return false;
    }
    {
        memcpy(recvBuffer + recvBufferLen, tempBuffer, received);
        recvBufferLen += received;

        if (state == IN_GAME) {
            // In-game: binary protocol {id byte}{msg}\n
            int processed = 0;
            while (processed < recvBufferLen) {
                // Look for newline
                int msgEnd = -1;
                for (int i = processed; i < recvBufferLen; i++) {
                    if (recvBuffer[i] == '\n') {
                        msgEnd = i;
                        break;
                    }
                }

                if (msgEnd == -1) {
                    // No complete message yet
                    break;
                }

                // Extract message: first byte is sender ID, rest is message
                if (msgEnd > processed) {
                    unsigned char senderId = (unsigned char)recvBuffer[processed];
                    int msgStart = processed + 1;
                    int msgLen = msgEnd - msgStart;

                    // recvBuffer is larger than gameMsg, so this bound is what
                    // keeps a long line from running off the end of a stack
                    // buffer. In-game frames are a few bytes; anything near this
                    // is not one, so skip it rather than truncate it into
                    // something that parses as a different message.
                    if (msgLen >= BUFFER_SIZE) {
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                    "Dropping oversized in-game message from player %d (%d bytes)",
                                    (int)senderId, msgLen);
                    } else if (msgLen > 0) {
                        char gameMsg[BUFFER_SIZE];
                        memcpy(gameMsg, recvBuffer + msgStart, msgLen);
                        gameMsg[msgLen] = '\0';

                        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                                     "Game message from player %d: %s", (int)senderId, gameMsg);

                        // Add to message queue for game to process
                        char fullMsg[BUFFER_SIZE];
                        snprintf(fullMsg, sizeof(fullMsg), "GAMEMSG:%d:%s", (int)senderId, gameMsg);
                        messageQueue.push_back(std::string(fullMsg));
                    }
                }

                processed = msgEnd + 1;
            }

            // Move remaining data to start of buffer
            if (processed > 0) {
                int remaining = recvBufferLen - processed;
                if (remaining > 0) {
                    memmove(recvBuffer, recvBuffer + processed, remaining);
                }
                recvBufferLen = remaining;
            }
        } else {
            // Lobby: text protocol FB/1.2 format
            recvBuffer[recvBufferLen] = '\0';

            // Process complete lines
            char* lineStart = recvBuffer;
            char* lineEnd;

            while ((lineEnd = strchr(lineStart, '\n')) != nullptr) {
                *lineEnd = '\0';
                ParseMessage(lineStart);
                lineStart = lineEnd + 1;
            }

            // Move remaining data to start of buffer
            int remaining = recvBuffer + recvBufferLen - lineStart;
            if (remaining > 0) {
                memmove(recvBuffer, lineStart, remaining);
                recvBufferLen = remaining;
            } else {
                recvBufferLen = 0;
            }
        }
    }
    return true;  // Successfully read and processed data
}
#endif // __WASM_PORT__ (Update, ProcessIncomingData)

void NetworkClient::ParseMessage(const char* message) {
    if (strlen(message) == 0) return;

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Received: %s", message);
    // Don't add server protocol messages to queue - they're handled immediately
    // Only GAMEMSG messages from ProcessIncomingData() should be queued
    HandleServerResponse(std::string(message));
}

void NetworkClient::HandleServerResponse(const std::string& response) {
    // Check if it's a PUSH message
    if (response.find("PUSH:") != std::string::npos) {
        size_t pushPos = response.find("PUSH:") + 6; // Skip "PUSH: "
        std::string pushMsg = response.substr(pushPos);
        HandlePushMessage(pushMsg);
        return;
    }

    // Check if it's a LIST response
    if (response.find("LIST:") != std::string::npos) {
        size_t listPos = response.find("LIST:") + 6; // Skip "LIST: "
        const char* listData = response.c_str() + listPos;
        ParseListResponse(listData);
        return;
    }

#ifndef __WASM_PORT__
    // The leader's game-start poll (stage 3a). This has to be matched before
    // the generic "OK" handling below, because that handling attributes a bare
    // OK to whichever of pendingNick/pendingCreate/pendingJoin is set -- and
    // "LEADER_CHECK_GAME_START: OK" contains "OK", so it would otherwise be
    // mistaken for a confirmation of whatever else was last sent.
    //
    // Unlike a bare OK, this one is safe to attribute: the server's send_line()
    // (server/net.c) formats replies as "FB/maj.min <command>: <result>", so a
    // command-specific response echoes the command that caused it.
    if (response.find("LEADER_CHECK_GAME_START") != std::string::npos) {
        if (!pendingGameStart) return;  // stale answer after we already started
        if (response.find("OTHERS_NOT_READY") != std::string::npos) {
            return;  // PumpGameStart() will poll again on its own cadence
        }
        if (response.find("OK") != std::string::npos) {
            SDL_Log("Leader: all players ready!");
            FinishGameStart();
        }
        return;
    }
#endif

    // Handle other responses. The server echoes the command name in every
    // reply (`FB/1.3 CREATE: OK`, for example), so a successful unrelated
    // command must not settle whichever async operation happens to be pending.
    // These pending blocks run on both platforms as of async stage 1b.
    if (response.find("OK") != std::string::npos) {
        SDL_Log("Command successful: %s", response.c_str());
        lastErrorResponse.clear();
        if (pendingNick && IsResponseForCommand(response, "NICK")) {
            SDL_Log("NICK confirmed by server (pendingNick=true): '%s'", pendingNickTry.c_str());
            playerNick = pendingNickTry;
            myNickname = pendingNickTry;
            pendingNick = false;
        } else if (pendingCreate && IsResponseForCommand(response, "CREATE")) {
            SDL_Log("CREATE confirmed by server (pendingCreate=true): game '%s'", pendingCreateNick.c_str());
            state = IN_LOBBY;
            playerNick = pendingCreateNick;
            myNickname = pendingCreateNick;
            if (!currentGame) currentGame = new GameRoom();
            currentGame->creator = pendingCreateNick;
            currentGame->started = false;
            currentGame->maxPlayers = pendingCreateMaxPlayers;
            NetworkPlayer self;
            self.nick = pendingCreateNick;
            self.ready = false;
            currentGame->players.clear();
            currentGame->players.push_back(self);
            pendingCreate = false;
        } else if (pendingJoin && IsResponseForCommand(response, "JOIN")) {
            SDL_Log("JOIN confirmed by server (pendingJoin=true): joined '%s' as '%s'", pendingJoinCreator.c_str(), pendingJoinNick.c_str());
            state = IN_LOBBY;
            playerNick = pendingJoinNick;
            myNickname = pendingJoinNick;
            // Set up currentGame from the cached gameList (populated by LIST responses)
            for (const auto& game : gameList) {
                if (game.creator == pendingJoinCreator) {
                    if (!currentGame) currentGame = new GameRoom();
                    *currentGame = game;
                    break;
                }
            }
            if (!currentGame) {
                // Game not in list yet — create a minimal placeholder
                currentGame = new GameRoom();
                currentGame->creator = pendingJoinCreator;
                currentGame->started = false;
            }
            // Add ourselves — server sends JOINED to existing players only, not to the joiner
            NetworkPlayer self;
            self.nick = pendingJoinNick;
            self.ready = false;
            // Avoid double-adding if already present from gameList sync
            bool alreadyIn = false;
            for (const auto& p : currentGame->players) {
                if (p.nick == pendingJoinNick) { alreadyIn = true; break; }
            }
            if (!alreadyIn) currentGame->players.push_back(self);
            SDL_Log("Joined game '%s', currentGame has %d players", pendingJoinCreator.c_str(), (int)currentGame->players.size());
            pendingJoin = false;
        }
    } else if (response.find("PONG") != std::string::npos) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Ping response");
    } else if (response.find("NICK_IN_USE") != std::string::npos) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "NICK_IN_USE error received");
        lastErrorResponse = "NICK_IN_USE";
        if (pendingNick && pendingNickSuffix <= 20) {
            char suffixBuf[8];
            snprintf(suffixBuf, sizeof(suffixBuf), "%d", pendingNickSuffix);
            std::string retryNick = pendingNickOrig.substr(0, std::min((size_t)9, pendingNickOrig.length())) + suffixBuf;
            pendingNickTry = retryNick;
            pendingNickSuffix++;
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "NICK %s", retryNick.c_str());
            SDL_Log("NICK NICK_IN_USE, retrying with: %s", retryNick.c_str());
            SendCommand(cmd);
            // Optimistic, same as SendNick() itself -- corrected again if this
            // retry also collides.
            playerNick = retryNick;
            myNickname = retryNick;
        } else if (pendingNick) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "NICK failed: all nick variants in use");
            AddStatusMessage("Could not set nickname '" + pendingNickOrig + "': too many players already using it");
            pendingNick = false;
        } else if (pendingCreate && pendingCreateSuffix <= 20) {
            char suffixBuf[8];
            snprintf(suffixBuf, sizeof(suffixBuf), "%d", pendingCreateSuffix);
            std::string retryNick = pendingCreateOrigNick.substr(0, std::min((size_t)9, pendingCreateOrigNick.length())) + suffixBuf;
            pendingCreateNick = retryNick;
            pendingCreateSuffix++;
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "CREATE %s %d", retryNick.c_str(), pendingCreateMaxPlayers);
            SDL_Log("CREATE NICK_IN_USE, retrying with: %s", retryNick.c_str());
            SendCommand(cmd);
        } else if (pendingCreate) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "CREATE failed: all nick variants in use");
            AddStatusMessage("Could not create room: nickname already taken, too many times");
            pendingCreate = false;
        } else if (pendingJoin && pendingJoinSuffix <= 20) {
            char suffixBuf[8];
            snprintf(suffixBuf, sizeof(suffixBuf), "%d", pendingJoinSuffix);
            std::string retryNick = pendingJoinOrigNick.substr(0, std::min((size_t)9, pendingJoinOrigNick.length())) + suffixBuf;
            pendingJoinNick = retryNick;
            pendingJoinSuffix++;
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "JOIN %s %s", pendingJoinCreator.c_str(), retryNick.c_str());
            SDL_Log("JOIN NICK_IN_USE, retrying with: %s", retryNick.c_str());
            SendCommand(cmd);
        } else if (pendingJoin) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "JOIN failed: all nick variants in use");
            AddStatusMessage("Could not join room: nickname already taken, too many times");
            pendingJoin = false;
        }
    } else if (response.find("INVALID_NICK") != std::string::npos) {
        // Not retriable with a numeric suffix (empty, too long, or contains a
        // character the server rejects -- server/game.c is_nick_ok) --
        // previously fell through HandleServerResponse unhandled on every
        // platform, leaving whichever pending flag was set stuck forever.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "INVALID_NICK error received");
        lastErrorResponse = "INVALID_NICK";
        if (pendingNick) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "NICK failed: server rejected '%s' as invalid", pendingNickTry.c_str());
            AddStatusMessage("Nickname '" + pendingNickTry + "' was rejected by the server as invalid");
            pendingNick = false;
        } else if (pendingCreate) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "CREATE failed: server rejected nick as invalid");
            AddStatusMessage("Could not create room: nickname rejected by the server as invalid");
            pendingCreate = false;
        } else if (pendingJoin) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "JOIN failed: server rejected nick as invalid");
            AddStatusMessage("Could not join room: nickname rejected by the server as invalid");
            pendingJoin = false;
        }
    } else if (response.find("GAME_FULL") != std::string::npos) {
        // Same pre-existing gap as INVALID_NICK above, specific to JOIN.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GAME_FULL error received");
        lastErrorResponse = "GAME_FULL";
        if (pendingJoin) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "JOIN failed: GAME_FULL for '%s'", pendingJoinCreator.c_str());
            AddStatusMessage("Could not join " + pendingJoinCreator + "'s room: it's full");
            pendingJoin = false;
        }
    } else if (response.find("NO_SUCH_GAME") != std::string::npos) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "NO_SUCH_GAME error received");
        lastErrorResponse = "NO_SUCH_GAME";
        if (pendingJoin) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "JOIN failed: NO_SUCH_GAME for '%s'", pendingJoinCreator.c_str());
            AddStatusMessage("Could not join " + pendingJoinCreator + "'s room: it no longer exists");
            pendingJoin = false;
        }
    } else if (response.find("ALREADY_IN_GAME") != std::string::npos) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "ALREADY_IN_GAME error received");
        lastErrorResponse = "ALREADY_IN_GAME";
        if (pendingCreate) {
            // Send PART to clear server-side stale game state, then retry
            SDL_Log("CREATE rejected (ALREADY_IN_GAME), sending PART and retrying");
            SendCommand("PART");
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "CREATE %s %d", pendingCreateNick.c_str(), pendingCreateMaxPlayers);
            SendCommand(cmd);
            // pendingCreate stays true, waiting for the new response
        } else if (pendingJoin) {
            // Already in a game — send PART to clear stale state, then retry JOIN
            SDL_Log("JOIN rejected (ALREADY_IN_GAME), sending PART and retrying");
            SendCommand("PART");
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "JOIN %s %s", pendingJoinCreator.c_str(), pendingJoinNick.c_str());
            SendCommand(cmd);
            // pendingJoin stays true, waiting for the new response
        }
    }
}

void NetworkClient::HandlePushMessage(const std::string& pushMsg) {
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "PUSH message: %s", pushMsg.c_str());

    if (pushMsg.find("SERVER_READY") == 0) {
        // Server ready, extract server name
        SDL_Log("Server ready");
    } else if (pushMsg.find("JOINED:") == 0) {
        // Player joined
        std::string nick = pushMsg.substr(8); // Skip "JOINED: "
        SDL_Log("Player %s joined the game", nick.c_str());

        // Add to current game if we're in one
        if (currentGame) {
            NetworkPlayer player;
            player.nick = nick;
            player.ready = false;
            currentGame->players.push_back(player);
            SDL_Log("Added %s to game, now has %d players", nick.c_str(), (int)currentGame->players.size());
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "JOINED message received but currentGame is NULL!");
        }
    } else if (pushMsg.find("ROOM_CLOSED:") == 0) {
        // Creator/leader left before the round started; server tore down the
        // whole room rather than promoting another connection to lead (see
        // player_part_game_ in server/game.c). Everyone else drops back to
        // the lobby, same as a manual PartGame().
        std::string creatorNick = pushMsg.substr(13); // Skip "ROOM_CLOSED: "
        SDL_Log("Room closed: creator %s left", creatorNick.c_str());

        ChatMessage chat;
        chat.nick = "Server";
        chat.message = "Room closed: " + creatorNick + " (the host) left";
        chat.timestamp = SDL_GetTicks();
        chatMessages.push_back(chat);
        if (chatMessages.size() > 50) {
            chatMessages.erase(chatMessages.begin());
        }

        state = IN_LOBBY;
        delete currentGame;
        currentGame = nullptr;
    } else if (pushMsg.find("PARTED:") == 0) {
        // Player left
        std::string nick = pushMsg.substr(8); // Skip "PARTED: "
        SDL_Log("Player %s left the game", nick.c_str());

        // Remove from current game
        if (currentGame) {
            auto it = std::remove_if(currentGame->players.begin(), currentGame->players.end(),
                [&nick](const NetworkPlayer& p) { return p.nick == nick; });
            currentGame->players.erase(it, currentGame->players.end());
        }
    } else if (pushMsg.find("TALK:") == 0) {
        // Chat message
        std::string message = pushMsg.substr(6); // Skip "TALK: "
        ChatMessage chat;

        // Parse format: "nick: message" or just "message"
        size_t colonPos = message.find(':');
        if (colonPos != std::string::npos && colonPos < 20) {
            chat.nick = message.substr(0, colonPos);
            chat.message = message.substr(colonPos + 2); // Skip ": "
        } else {
            chat.nick = "Server";
            chat.message = message;
        }
        chat.timestamp = SDL_GetTicks();

        chatMessages.push_back(chat);

        // Keep only last 50 messages
        if (chatMessages.size() > 50) {
            chatMessages.erase(chatMessages.begin());
        }

        SDL_Log("Chat: [%s] %s", chat.nick.c_str(), chat.message.c_str());
    } else if (pushMsg.compare(0, 6, "KICKED") == 0 &&
               pushMsg.find_first_not_of(" \r\t", 6) == std::string::npos) {
        // We are the one who was kicked. The server has already removed this
        // connection from the room (game.c kick_player -> player_part_game_),
        // so staying on the room screen would leave us looking at a room we
        // are no longer in, unable to act on anything in it.
        SDL_Log("We were kicked from the room");
        ChatMessage chat;
        chat.nick = "Server";
        chat.message = "The host removed you from the room";
        chat.timestamp = SDL_GetTicks();
        chatMessages.push_back(chat);
        if (chatMessages.size() > 50) chatMessages.erase(chatMessages.begin());
        state = IN_LOBBY;
        delete currentGame;
        currentGame = nullptr;
    } else if (pushMsg.find("KICKED:") == 0) {
        // Someone else was kicked. This arrives instead of PARTED, so the
        // roster has to be maintained here too or they linger in the list.
        std::string nick = pushMsg.substr(8);
        SDL_Log("Kicked: %s", nick.c_str());
        if (currentGame) {
            auto it = std::remove_if(currentGame->players.begin(), currentGame->players.end(),
                [&nick](const NetworkPlayer& p) { return p.nick == nick; });
            currentGame->players.erase(it, currentGame->players.end());
        }
        ChatMessage chat;
        chat.nick = "Server";
        chat.message = nick + " was removed by the host";
        chat.timestamp = SDL_GetTicks();
        chatMessages.push_back(chat);
        if (chatMessages.size() > 50) chatMessages.erase(chatMessages.begin());
    } else if (pushMsg.find("OPTIONS: ") == 0) {
        // Host broadcast updated game settings (server relays as "OPTIONS: ...")
        std::string opts = pushMsg.substr(9); // Skip "OPTIONS: "
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Received SETOPTIONS: %s", opts.c_str());
        // Parse key:value pairs separated by commas
        auto parseVal = [&](const char* key, int def) -> int {
            std::string search = std::string(key) + ":";
            size_t p = opts.find(search);
            if (p == std::string::npos) return def;
            // OPTIONS arrives from another client. std::stoi throws
            // std::invalid_argument on a non-numeric value and std::out_of_range
            // on one too large for int; uncaught, either terminates the process.
            // Fall back to the default so a malformed push cannot kill the game.
            try {
                return std::stoi(opts.substr(p + search.size()));
            } catch (const std::logic_error &) {
                SDL_Log("Ignoring malformed OPTIONS value for %s", key);
                return def;
            }
        };
        rcvChainReaction = parseVal("CHAINREACTION", 1) != 0;
        rcvContinueLeave = parseVal("CONTINUEGAMEWHENPLAYERSLEAVE", 1) != 0;
        rcvSingleTarget  = parseVal("SINGLEPLAYERTARGETTING", 1) != 0;
        rcvVictoriesLimit = parseVal("VICTORIESLIMIT", 5);
        rcvPlayerColors[0] = parseVal("NUMCOLORS_P1", 7);
        rcvPlayerColors[1] = parseVal("NUMCOLORS_P2", 7);
        rcvPlayerColors[2] = parseVal("NUMCOLORS_P3", 7);
        rcvPlayerColors[3] = parseVal("NUMCOLORS_P4", 7);
        rcvPlayerColors[4] = parseVal("NUMCOLORS_P5", 7);
        rcvNoCompress[0] = parseVal("NOCOMPRESS_P1", 0) != 0;
        rcvNoCompress[1] = parseVal("NOCOMPRESS_P2", 0) != 0;
        rcvNoCompress[2] = parseVal("NOCOMPRESS_P3", 0) != 0;
        rcvNoCompress[3] = parseVal("NOCOMPRESS_P4", 0) != 0;
        rcvNoCompress[4] = parseVal("NOCOMPRESS_P5", 0) != 0;
        rcvAimGuide[0] = parseVal("AIMGUIDE_P1", 0) != 0;
        rcvAimGuide[1] = parseVal("AIMGUIDE_P2", 0) != 0;
        rcvAimGuide[2] = parseVal("AIMGUIDE_P3", 0) != 0;
        rcvAimGuide[3] = parseVal("AIMGUIDE_P4", 0) != 0;
        rcvAimGuide[4] = parseVal("AIMGUIDE_P5", 0) != 0;
        rcvMouseEnabled = parseVal("MOUSEENABLED", 0) != 0;
        // GAMEMODE when the room's host is new enough to send one; otherwise
        // fall back to the CLEARMODE bit, which is all an older host says.
        // ClampGameMode is the trust boundary: this value comes straight off
        // another client's push and selects behaviour on this one.
        rcvGameMode = ClampGameMode(
            parseVal("GAMEMODE", parseVal("CLEARMODE", 0) != 0 ? (int)GameMode::Clear
                                                              : (int)GameMode::Classic));
        // Taken at face value inside sane bounds rather than snapped to this
        // build's own menu steps -- see ClampRaceTarget's comment on why the
        // room's number has to survive the trip intact.
        rcvRaceTarget = ClampRaceTarget(parseVal("RACETARGET", kRaceTargetDefault));
        rcvTimedSeconds = ClampTimedSeconds(parseVal("TIMELIMIT", kTimedSecondsDefault));
        // Off wins over canceling if a malformed push somehow sets both:
        // "no attacks at all" is the safer of the two to land on.
        rcvAttackMode = parseVal("DISABLEMALUS", 0) != 0 ? AttackMode::Off
                      : parseVal("MALUSCANCEL", 0) != 0 ? AttackMode::Canceling
                      : AttackMode::On;
        rcvTeamCount = (int)parseVal("TEAMCOUNT", 2);
        if (rcvTeamCount < 2) rcvTeamCount = 2;
        if (rcvTeamCount > 5) rcvTeamCount = 5;
        for (int i = 0; i < 5; i++) {
            char key[32];
            snprintf(key, sizeof(key), "PLAYERTEAM_P%d", i + 1);
            // Clamp here, at the trust boundary: this value comes from another
            // client's OPTIONS push and, when it names a real team, is used
            // one-based to index kTeamColors. ClampTeamOrNone rather than
            // ClampTeamNumber because 0 is now a real answer ("no team") that
            // has to survive the trip -- callers check it against kNoTeam
            // before indexing. A missing key defaults to no team, which is
            // also what an older peer that never sends one should read as.
            rcvPlayerTeams[i] = ClampTeamOrNone((int)parseVal(key, kNoTeam));
        }
        pendingOptions = true;
    } else if (pushMsg.find("GAME_CAN_START:") == 0) {
        // Game is ready to start - server sent player mappings
        // Format: {id byte}{nick},{id byte}{nick},...
        std::string mappings = pushMsg.substr(16); // Skip "GAME_CAN_START: "
        SDL_Log("Game can start! Parsing player mappings...");

        // Parse binary format to find our player ID
        const char* data = mappings.c_str();
        size_t len = mappings.length();
        size_t i = 0;

        // Clear old mappings
        playerIdToNick.clear();

        while (i < len) {
            // First byte is player ID
            unsigned char playerId = (unsigned char)data[i];
            i++;

            // Read nickname until comma or end
            std::string nick;
            while (i < len && data[i] != ',') {
                nick += data[i];
                i++;
            }
            i++; // Skip comma

            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Player mapping: ID=%d nick=%s", (int)playerId, nick.c_str());

            // Store mapping for later use
            playerIdToNick[(int)playerId] = nick;

            // Check if this is us
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Comparing nick='%s' with myNickname='%s'", nick.c_str(), myNickname.c_str());
            if (nick == myNickname) {
                myPlayerId = playerId;
                SDL_Log("Found our player ID: %d (matched nickname '%s')", (int)myPlayerId, nick.c_str());
            }

            // Update currentGame->players with the authoritative player list from server
            // This ensures all players see correct nicknames in the game room
            if (currentGame) {
                // Check if player already exists in the list
                bool found = false;
                for (auto& p : currentGame->players) {
                    if (p.nick == nick) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // Add new player
                    NetworkPlayer newPlayer;
                    newPlayer.nick = nick;
                    newPlayer.ready = false;
                    currentGame->players.push_back(newPlayer);
                    SDL_Log("Added player %s to currentGame from GAME_CAN_START (now %d players)",
                           nick.c_str(), (int)currentGame->players.size());
                }
            }
        }

        // Original Perl: leader must poll LEADER_CHECK_GAME_START until all others send OK_GAME_START,
        // then leader sends OK_GAME_START last. Non-leaders just send OK_GAME_START immediately.
        // This ensures all players are in prio mode before the leader starts sending sync messages.
#ifndef __WASM_PORT__
        if (IsLeader()) {
            // Kick off the poll and get out of this push handler. It used to
            // run right here as a blocking loop -- 50 attempts of up to 200ms
            // select() plus a 100ms sleep, so up to 15 seconds of frozen
            // render loop (its own comment claimed 5s), inside a function
            // reached from the per-frame message pump.
            //
            // PumpGameStart() sends the polls from now on and
            // HandleServerResponse() reads the answers, so the frame keeps
            // turning throughout -- which also means hosted bots keep being
            // serviced by the ordinary per-frame pump (stage 1a) instead of
            // by the leaderWaitTick callback this loop had to call by hand.
            // That callback had no other caller and is now retired.
            SDL_Log("Leader: polling LEADER_CHECK_GAME_START until all joiners are ready...");
            pendingGameStart = true;
            gameStartDeadline = SDL_GetTicks() + kGameStartTimeoutMs;
            gameStartNextPollMs = 0;  // poll on the very next pump
            return;                   // OK_GAME_START waits for FinishGameStart()
        }
#endif // __WASM_PORT__ (LEADER_CHECK_GAME_START TCP poll)

        // Non-leaders acknowledge immediately -- they have nobody to wait for.
        SDL_Log("Sending OK_GAME_START acknowledgement (state is still %d)...", state);
        bool sent = SendCommand("OK_GAME_START");
        SDL_Log("OK_GAME_START sent result: %s", sent ? "SUCCESS" : "FAILED");

        // NOW transition to IN_GAME state after OK response has been handled
        SDL_Log("Setting state to IN_GAME (myPlayerId=%d)", (int)myPlayerId);
        state = IN_GAME;
    }
}

#ifndef __WASM_PORT__
void NetworkClient::PumpGameStart() {
    if (!pendingGameStart) return;

    const Uint64 now = SDL_GetTicks();
    if (now > gameStartDeadline) {
        // Start anyway. A joiner that never acknowledges costs itself a
        // desynced board; refusing to start strands everyone else in the
        // lobby with no way forward, which is strictly worse.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Leader: joiners did not all acknowledge in time, starting anyway");
        FinishGameStart();
        return;
    }

    if (now >= gameStartNextPollMs) {
        char buf[BUFFER_SIZE];
        snprintf(buf, sizeof(buf), "FB/%d.%d LEADER_CHECK_GAME_START\n", PROTO_MAJOR, PROTO_MINOR);
        SendAll(sockfd, buf, strlen(buf));
        // Same 100ms cadence the old loop used between attempts.
        gameStartNextPollMs = now + 100;
    }
}

void NetworkClient::FinishGameStart() {
    pendingGameStart = false;
    gameStartDeadline = 0;
    gameStartNextPollMs = 0;

    SDL_Log("Sending OK_GAME_START acknowledgement (state is still %d)...", state);
    const bool sent = SendCommand("OK_GAME_START");
    SDL_Log("OK_GAME_START sent result: %s", sent ? "SUCCESS" : "FAILED");

    SDL_Log("Setting state to IN_GAME (myPlayerId=%d)", (int)myPlayerId);
    state = IN_GAME;
}
#endif

void NetworkClient::ParseListResponse(const char* listData) {
    // Clear current lists
    gameList.clear();
    openPlayers.clear();

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Parsing LIST response");
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Raw data: %s", listData);

    // Format: <open-players>,<space>[<game1>][<game2>]...<space>free:<count> games:<count> playing:<count> at:<geolocs>

    std::string data(listData);
    size_t pos = 0;

    // Parse open players (not in any game)
    size_t spacePos = data.find(' ');
    if (spacePos != std::string::npos) {
        std::string openPlayersStr = data.substr(0, spacePos);

        // Split by comma
        size_t start = 0;
        size_t commaPos;
        while ((commaPos = openPlayersStr.find(',', start)) != std::string::npos) {
            std::string playerStr = openPlayersStr.substr(start, commaPos - start);
            if (!playerStr.empty()) {
                NetworkPlayer player;

                // Check for geoloc (format: NICK:GEOLOC)
                size_t colonPos = playerStr.find(':');
                if (colonPos != std::string::npos) {
                    player.nick = playerStr.substr(0, colonPos);
                    player.geoloc = playerStr.substr(colonPos + 1);
                } else {
                    player.nick = playerStr;
                }
                player.ready = false;
                openPlayers.push_back(player);
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Open player: %s%s%s", player.nick.c_str(),
                       player.geoloc.empty() ? "" : " (",
                       player.geoloc.empty() ? "" : (player.geoloc + ")").c_str());
            }
            start = commaPos + 1;
        }

        data = data.substr(spacePos + 1);
    }

    // Parse games (enclosed in brackets)
    while ((pos = data.find('[')) != std::string::npos) {
        size_t endPos = data.find(']', pos);
        if (endPos == std::string::npos) break;

        std::string gameStr = data.substr(pos + 1, endPos - pos - 1);
        GameRoom game;
        game.started = false;

        // Parse players in game (comma-separated)
        size_t start = 0;
        size_t commaPos;
        bool first = true;
        while ((commaPos = gameStr.find(',', start)) != std::string::npos) {
            std::string playerStr = gameStr.substr(start, commaPos - start);
            if (!playerStr.empty()) {
                NetworkPlayer player;

                // Check for geoloc
                size_t colonPos = playerStr.find(':');
                if (colonPos != std::string::npos) {
                    player.nick = playerStr.substr(0, colonPos);
                    player.geoloc = playerStr.substr(colonPos + 1);
                } else {
                    player.nick = playerStr;
                }
                player.ready = false;

                // First player is the creator
                if (first) {
                    game.creator = player.nick;
                    first = false;
                }

                game.players.push_back(player);
            }
            start = commaPos + 1;
        }

        // Don't forget the last player
        if (start < gameStr.length()) {
            std::string playerStr = gameStr.substr(start);
            if (!playerStr.empty()) {
                NetworkPlayer player;
                size_t colonPos = playerStr.find(':');
                if (colonPos != std::string::npos) {
                    player.nick = playerStr.substr(0, colonPos);
                    player.geoloc = playerStr.substr(colonPos + 1);
                } else {
                    player.nick = playerStr;
                }
                player.ready = false;

                if (first) {
                    game.creator = player.nick;
                }

                game.players.push_back(player);
            }
        }

        // Optional ":<cap>" suffix after the closing bracket (server sends the
        // room's max_players there; absent on old servers -> default 5).
        size_t capEnd = endPos + 1;
        if (capEnd < data.size() && data[capEnd] == ':') {
            int cap = 0;
            size_t d = capEnd + 1;
            while (d < data.size() && data[d] >= '0' && data[d] <= '9') {
                if (cap <= 20) cap = cap * 10 + (data[d] - '0');
                d++;
            }
            if (cap >= 2) game.maxPlayers = std::min(cap, 20);  // clamp to MAX room size
            capEnd = d;
        }

        if (!game.players.empty()) {
            gameList.push_back(game);
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Game: %s (%d/%d players)", game.creator.c_str(),
                    (int)game.players.size(), game.maxPlayers);
        }

        data = data.substr(capEnd);
    }

    SDL_Log("Found %d games and %d open players", (int)gameList.size(), (int)openPlayers.size());

    // Update currentGame from gameList - this is needed for BOTH host and joiners:
    // - Host: doesn't receive JOINED messages (server only sends to other players)
    // - Joiners: only receive JOINED for players who join AFTER them, not players already in game
    // So LIST is the authoritative source for the complete player list
    if (currentGame && !gameList.empty()) {
        for (const auto& game : gameList) {
            if (game.creator == currentGame->creator) {
                // Found our game - sync player list from LIST
                SDL_Log("LIST update: Our game '%s' has %d players", game.creator.c_str(), (int)game.players.size());

                // Rebuild in server-authoritative order so all clients agree on slot indices.
                // The old add-at-end approach left different clients with different orderings
                // whenever players joined faster than the 500ms LIST refresh interval.
                currentGame->players = game.players;
                currentGame->maxPlayers = game.maxPlayers;
                SDL_Log("LIST sync: rebuilt player list (%d players)", (int)currentGame->players.size());

                break;
            }
        }
    }
}

bool NetworkClient::HasMessage() {
    return !messageQueue.empty();
}

std::string NetworkClient::GetNextMessage() {
    if (messageQueue.empty()) return "";
    std::string msg = messageQueue.front();
    messageQueue.pop_front();
    return msg;
}

void NetworkClient::PutBackMessage(const std::string& msg) {
    messageQueue.push_front(msg);
}

// NOTE: DiscoverLANServers, MeasureLatency, IsReachable, DetectGeoLocation,
// FetchPublicServers, and curlFetch are each wrapped with #ifndef __WASM_PORT__
// below. DetectGeoLocation and FetchPublicServers also contain inner
// #ifdef __WASM_PORT__ early-return guards — those are dead code now that the
// whole function is excluded by the outer guard, but they are left in place
// to document the original intent.

#ifndef __WASM_PORT__
std::vector<ServerInfo> NetworkClient::DiscoverLANServers() {
    std::vector<ServerInfo> servers;

    int udpSock = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock == (int)INVALID_SOCKET) return servers;

    int broadcast = 1;
    setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, SETSOCKOPT_OPTVAL(broadcast), sizeof(broadcast));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(1511);
    dest.sin_addr.s_addr = INADDR_BROADCAST;

    char probe[64];
    snprintf(probe, sizeof(probe), "FB/%d.%d SERVER PROBE", PROTO_MAJOR, PROTO_MINOR);
    sendto(udpSock, probe, strlen(probe), 0, (struct sockaddr*)&dest, sizeof(dest));

    Uint64 startTime = SDL_GetTicks();
    while (SDL_GetTicks() - startTime < 1000) {
        fd_set readfds;
        struct timeval tv = {0, 50000};  // 50ms
        FD_ZERO(&readfds);
        FD_SET(udpSock, &readfds);
        if (select(udpSock + 1, &readfds, nullptr, nullptr, &tv) > 0) {
            char buf[256];
            struct sockaddr_in from;
            socklen_t fromLen = sizeof(from);
            ssize_t n = recvfrom(udpSock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &fromLen);
            if (n > 0) {
                buf[n] = '\0';
                int port = 0;
                if (sscanf(buf, "FB/%*d.%*d SERVER HERE AT PORT %d", &port) == 1) {
                    ServerInfo si;
                    si.host = inet_ntoa(from.sin_addr);
                    si.port = port;
                    bool dup = false;
                    for (const auto& s : servers) {
                        if (s.host == si.host && s.port == si.port) { dup = true; break; }
                    }
                    if (!dup) servers.push_back(si);
                }
            }
        }
    }

    SOCKET_CLOSE(udpSock);

    // UDP broadcast doesn't reach loopback — also probe 127.0.0.1 directly
    // so a locally-hosted server appears in the LAN list
    {
        int port = 1511;
        bool alreadyFound = false;
        for (const auto& s : servers) {
            if (s.host == "127.0.0.1" && s.port == port) { alreadyFound = true; break; }
        }
        if (!alreadyFound && MeasureLatency("127.0.0.1", port, 300) >= 0) {
            ServerInfo si;
            si.host = "127.0.0.1";
            si.port = port;
            servers.insert(servers.begin(), si);  // Put localhost first
        }
    }

    return servers;
}
#endif // __WASM_PORT__ (DiscoverLANServers)

#ifndef __WASM_PORT__
int NetworkClient::MeasureLatency(const char* host, int port, int timeoutMs) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);
    if (getaddrinfo(host, portStr, &hints, &res) != 0 || !res) return -1;

    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) { freeaddrinfo(res); return -1; }

#ifdef _WIN32
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    connect(s, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
    struct timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    bool ok = (select(s + 1, nullptr, &wfds, nullptr, &tv) > 0);

    // select() reporting the socket writable only means the connect attempt
    // finished — it says nothing about whether it succeeded. A refused
    // connection also completes and also becomes writable, so treating
    // writability alone as success listed dead servers as online, with a
    // plausible-looking latency (audit finding BUG-016). The outcome lives in
    // SO_ERROR, which is zero only on an actual connection.
    if (ok) {
        int soErr = 0;
        socklen_t soErrLen = sizeof(soErr);
        // Not SETSOCKOPT_OPTVAL: that yields a const char* for setsockopt, and
        // getsockopt writes through this pointer.
#ifdef _WIN32
        char* soErrPtr = reinterpret_cast<char*>(&soErr);
#else
        void* soErrPtr = &soErr;
#endif
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, soErrPtr, &soErrLen) != 0 || soErr != 0) {
            ok = false;
        }
    }

    SOCKET_CLOSE(s);

    if (!ok) return -1;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
    return (int)ms;
}

bool NetworkClient::IsReachable(const char* host, int port, int timeoutMs) {
    return MeasureLatency(host, port, timeoutMs) >= 0;
}
#endif // __WASM_PORT__ (MeasureLatency, IsReachable)

// Public server list hosted in a dedicated repo — same name and format as original frozen-bubble.org
#define GITHUB_SERVER_LIST_URL \
    "https://raw.githubusercontent.com/dchau360/frozen-bubble-servers/main/serverlist-" PROTO_MAJOR_STR

// Original Frozen Bubble master server list URL (format: "host port" per line)
#define FB_MASTER_SERVER_URL \
    "http://www.frozen-bubble.org/servers/serverlist-" PROTO_MAJOR_STR

#define XSTR(s) STR(s)
#define STR(s) #s
#define PROTO_MAJOR_STR XSTR(PROTO_MAJOR)

#ifndef __WASM_PORT__
#ifdef __ANDROID__
// Fetch a URL via JNI by calling FrozenBubbleActivity.fetchUrl(String) → String.
// SDL3's SDL_GetAndroidJNIEnv() / SDL_GetAndroidActivity() give us the JNI context.
static std::string androidFetchUrl(const char* url) {
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return "";

    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetStaticMethodID(cls, "fetchUrl", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!mid) {
        SDL_Log("androidFetchUrl: fetchUrl method not found");
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return "";
    }

    jstring jurl = env->NewStringUTF(url);
    jstring jresult = (jstring)env->CallStaticObjectMethod(cls, mid, jurl);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);

    if (!jresult) return "";
    const char* chars = env->GetStringUTFChars(jresult, nullptr);
    std::string result(chars ? chars : "");
    env->ReleaseStringUTFChars(jresult, chars);
    env->DeleteLocalRef(jresult);
    return result;
}
#endif

static void curlFetch(const char* url, std::vector<ServerInfo>& out, bool originalFormat) {
#if defined(__ANDROID__) || defined(__IOS_PORT__)
#ifdef __ANDROID__
    std::string body = androidFetchUrl(url);
#else
    std::string body = IosFetchUrl(url, 8);
#endif
    if (body.empty()) return;

    // Parse line by line — same logic as the popen path below
    std::istringstream stream(body);
    std::string lineStr;
    while (std::getline(stream, lineStr)) {
        // Strip trailing \r
        if (!lineStr.empty() && lineStr.back() == '\r') lineStr.pop_back();
        if (lineStr.empty() || lineStr[0] == '#') continue;

        char host[256] = "", name[256] = "";
        int port = 1511;
        if (originalFormat) {
            if (sscanf(lineStr.c_str(), "%255s %d", host, &port) < 2) continue;
        } else {
            char hostport[256] = "";
            sscanf(lineStr.c_str(), "%255s %255[^\n]", hostport, name);
            char* colon = strrchr(hostport, ':');
            if (colon) { *colon = '\0'; strncpy(host, hostport, sizeof(host)-1); port = atoi(colon+1); }
            else strncpy(host, hostport, sizeof(host)-1);
        }
        if (host[0] == '\0') continue;
        bool dup = false;
        for (const auto& s : out) if (s.host == host && s.port == port) { dup = true; break; }
        if (dup) continue;
        ServerInfo si;
        si.host = host; si.port = port;
        si.name = (name[0] != '\0') ? name : (std::string(host) + ":" + std::to_string(port));
        out.push_back(si);
    }
    return;
#endif
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -s --connect-timeout 5 --max-time 8 '%s' 2>/dev/null", url);
    FILE* fp = popen(cmd, "r");
    if (!fp) return;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;

        char host[256] = "";
        int port = 1511;
        char name[256] = "";

        if (originalFormat) {
            // Original format: "host port"
            if (sscanf(line, "%255s %d", host, &port) < 2) continue;
        } else {
            // GitHub format: "host:port Name..."
            char hostport[256] = "";
            sscanf(line, "%255s %255[^\n]", hostport, name);
            char* colon = strrchr(hostport, ':');
            if (colon) {
                *colon = '\0';
                strncpy(host, hostport, sizeof(host) - 1);
                port = atoi(colon + 1);
            } else {
                strncpy(host, hostport, sizeof(host) - 1);
            }
        }
        if (host[0] == '\0') continue;

        // Deduplicate
        bool dup = false;
        for (const auto& s : out)
            if (s.host == host && s.port == port) { dup = true; break; }
        if (dup) continue;

        ServerInfo si;
        si.host = host;
        si.port = port;
        si.name = (name[0] != '\0') ? name : (std::string(host) + ":" + std::to_string(port));
        out.push_back(si);
    }
    pclose(fp);
}

std::string NetworkClient::DetectGeoLocation() {
    // Cache result for the session
    static std::string cached = "";
    static bool tried = false;
    if (tried) return cached;
    tried = true;

#ifdef __WASM_PORT__
    return "zz";
#endif

    // Try ipinfo.io/loc (HTTPS, returns "lat,lon") then fall back to ip-api.com (HTTP)
    const char* urls[] = {
        "https://ipinfo.io/loc",
        "http://ip-api.com/line/?fields=lat,lon"
    };
    std::string body;

    for (const char* url : urls) {
        body.clear();
#if defined(__ANDROID__)
        body = androidFetchUrl(url);
#elif defined(__IOS_PORT__)
        body = IosFetchUrl(url, 8);
#else
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "curl -s --connect-timeout 5 --max-time 8 '%s' 2>/dev/null", url);
        FILE* fp = popen(cmd, "r");
        if (fp) {
            char buf[64];
            while (fgets(buf, sizeof(buf), fp)) body += buf;
            pclose(fp);
        }
#endif
        if (!body.empty()) break;
    }

    // Handles both "lat,lon" (ipinfo) and "lat\nlon" (ip-api)
    float lat = 0.0f, lon = 0.0f;
    if (sscanf(body.c_str(), "%f,%f", &lat, &lon) == 2 ||
        sscanf(body.c_str(), "%f %f", &lat, &lon) == 2) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f:%.1f", lat, lon);
        // Ensure within 13 char server limit
        buf[13] = '\0';
        cached = buf;
        SDL_Log("Detected geolocation: %s", cached.c_str());
        return cached;
    }

    SDL_Log("Geolocation detection failed, using 'zz'");
    cached = "zz";
    return cached;
}

std::vector<ServerInfo> NetworkClient::FetchPublicServers() {
    std::vector<ServerInfo> servers;

#ifdef __WASM_PORT__
    // WASM: return hardcoded known public servers
    {
        ServerInfo si;
        si.host = "fb.servequake.com";
        si.port = 1511;
        si.name = "Frozen Bubble Server";
        si.latencyMs = -1; // latency probe not available in WASM
        servers.push_back(si);
    }
    return servers;
#endif

    SDL_Log("Fetching server lists...");

    // 1. Original Frozen Bubble master server
    curlFetch(FB_MASTER_SERVER_URL, servers, true);
    SDL_Log("After master server: %d servers", (int)servers.size());

    // 2. Community server list (host:port Name format)
    curlFetch(GITHUB_SERVER_LIST_URL, servers, false);
    SDL_Log("After GitHub list: %d servers total", (int)servers.size());

    return servers;
}
#endif // __WASM_PORT__ (androidFetchUrl, curlFetch, DetectGeoLocation, FetchPublicServers)
