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

#include "netbot.h"

// SDL lives here, not just in the native half: the shared Update() and
// HandleLine() below are compiled on WASM too, and they read SDL_GetTicks()
// for the keepalive and log through SDL_Log/SDL_LogWarn.
#include <SDL3/SDL.h>

// Parsing is wire-format handling, not socket handling, so it is built on
// every platform including WASM -- and tested there too.
GameCanStartRoster ParseGameCanStart(const std::string& payload,
                                     const std::string& myNick) {
    GameCanStartRoster out;
    size_t i = 0;
    while (i < payload.size()) {
        const int playerId = static_cast<unsigned char>(payload[i++]);
        std::string entry;
        while (i < payload.size() && payload[i] != ',') entry.push_back(payload[i++]);
        if (i < payload.size() && payload[i] == ',') ++i;
        if (entry.empty()) continue;
        out.players[playerId] = entry;
        if (entry == myNick) out.myPlayerId = playerId;
    }
    return out;
}

std::map<int, int> AssignRemoteSeats(const std::vector<int>& roomPlayerIds,
                                     int myPlayerId,
                                     int playerCount) {
    std::map<int, int> seats;
    int slot = 1;
    for (int id : roomPlayerIds) {
        if (id == myPlayerId) continue;
        if (slot >= playerCount) break;
        seats[slot++] = id;
    }
    return seats;
}

int MaxRoomBots(int roomPlayers, int maxPlayers, int currentBots) {
    if (currentBots < 0) currentBots = 0;
    const int free = maxPlayers - roomPlayers;
    int ceiling = currentBots + (free > 0 ? free : 0);
    if (ceiling > kMaxRoomBots) ceiling = kMaxRoomBots;
    if (ceiling < 0) ceiling = 0;
    return ceiling;
}

bool IsConnectionLevelOpcode(char opcode) {
    return opcode == 'n' || opcode == 'l';
}

bool IsKickedMePush(const std::string& line) {
    static const char kKicked[] = "PUSH: KICKED";
    const size_t at = line.find(kKicked);
    if (at == std::string::npos) return false;
    // Anything but trailing whitespace after it means this is the
    // "KICKED: <nick>" notice about somebody else.
    return line.find_first_not_of(" \r\t", at + sizeof(kKicked) - 1) == std::string::npos;
}

bool IsBotLimitReachedReply(const std::string& line) {
    return line.find("BOT_LIMIT_REACHED") != std::string::npos;
}

namespace {

// The push that announces a game and carries the id-to-nickname roster.
// Hoisted out of the native block because the platform-shared HandleLine()
// below matches on it on every build.
constexpr char kStartPush[] = "PUSH: GAME_CAN_START: ";

}  // namespace

// The destructor is shared too: on every platform the only teardown the
// connection needs is whatever Leave() decides is right for the transport.
NetBotConnection::~NetBotConnection() { Leave(); }

void NetBotConnection::HandleLine(const std::string& line) {
    // Before the game starts everything is a lobby line. The one that matters
    // carries the id-to-nickname map, including this bot's own id, which every
    // in-game message it sends has to be prefixed with.
    const size_t at = line.find(kStartPush);
    if (at != std::string::npos) {
        const GameCanStartRoster parsed =
            ParseGameCanStart(line.substr(at + sizeof(kStartPush) - 1), nick);
        roster = parsed.players;
        myPlayerId = parsed.myPlayerId;
        SDL_Log("netbot %s: game starting, id=%d, %zu players",
                nick.c_str(), myPlayerId, roster.size());
        // Acknowledging is what puts this connection into the server's prio
        // mode (game.c ok_start_game -> add_prio). Without it the server
        // keeps reading this socket as lobby text: the bot's shots would
        // never be relayed, and the leader's own start would stall waiting
        // for an acknowledgement that never came.
        SendLine("OK_GAME_START");
        return;
    }

    // The server's kick only puts a player back in the lobby -- the socket
    // stays open. For a person that is the right thing; a bot nobody is
    // hosting any more has nothing to sit in the lobby for, and would show up
    // in the server's player list forever.
    if (IsKickedMePush(line)) {
        SDL_Log("netbot %s: kicked from the room, disconnecting", nick.c_str());
        Leave();
        return;
    }

    if (IsBotLimitReachedReply(line)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "netbot %s: server bot limit reached, disconnecting", nick.c_str());
        rejectedByServer = true;
        Leave();
        return;
    }

    // Once OK_GAME_START is acknowledged (above) the server switches this
    // connection into binary prio mode (game.c ok_start_game -> add_prio),
    // and every line from here on is a raw {id byte}{payload} game frame --
    // exactly the same signal NetworkClient::ProcessIncomingData gates on
    // (state == IN_GAME) rather than sniffing the byte. There is no byte
    // value that reliably marks a game frame on its own: real player ids
    // are 'A'-'z' (game.c next_seat_id), ordinary printable ASCII, not some
    // low control range -- a prior version of this check assumed otherwise
    // and so never actually matched a real game message, silently leaving
    // gameMessages empty for the whole match.
    if (myPlayerId != 0) {
        gameMessages.emplace_back(static_cast<unsigned char>(line[0]), line.substr(1));
    }
}

void NetBotConnection::Update() {
    Drain();
    for (;;) {
        const size_t nl = incoming.find('\n');
        if (nl == std::string::npos) break;
        const std::string line = incoming.substr(0, nl);
        incoming.erase(0, nl + 1);
        if (!line.empty()) HandleLine(line);
    }

    // Keepalive. A bot goes quiet the moment its board is out of the round --
    // it has no shots left to send -- and five seconds later the server drops
    // it for inactivity, which reads to everyone else as the bot rage-quitting
    // mid-match. The human client sends the same 'p' once a second for the
    // same reason; the server consumes it and does not relay it.
    if (sockfd >= 0 && myPlayerId != 0) {
        const unsigned now = SDL_GetTicks();
        if (now - lastSendTicks >= 1000) SendGamePayload("p");
    }
}

bool NetBotConnection::TakeGameMessage(int* senderId, std::string* payload) {
    if (gameMessages.empty()) return false;
    if (senderId) *senderId = gameMessages.front().first;
    if (payload) *payload = gameMessages.front().second;
    gameMessages.pop_front();
    return true;
}

#ifndef __WASM_PORT__

#include "socket_compat.h"

#include <cerrno>
#include <cstring>
#if !defined(_WIN32)
#include <netdb.h>   // getaddrinfo; Windows has it in ws2tcpip.h, via socket_compat.h
#endif
#include <string>

namespace {

constexpr int kProtoMajor = 1;
constexpr int kProtoMinor = 3;
// A bot's own messages are short; the level sync it receives is not, so the
// read buffer is sized for a burst rather than a line.
constexpr size_t kReadChunk = 16384;

// A signal can interrupt a socket call at any time; that is not a failure,
// but the plain would-block test does not cover it, so without this an
// unlucky signal would close the bot's connection mid-match.
bool SockInterrupted(int err) {
#ifdef _WIN32
    return err == WSAEINTR;
#else
    return err == EINTR;
#endif
}

bool SendAllBytes(int fd, const char* data, size_t len) {
    size_t offset = 0;
    int stalls = 0;
    while (offset < len) {
        const ssize_t n = send(fd, data + offset, len - offset, MSG_NOSIGNAL);
        if (n > 0) {
            offset += static_cast<size_t>(n);
            stalls = 0;
            continue;
        }
        if (n < 0 && SockInterrupted(SOCK_ERRNO)) continue;
        if (n < 0 && SOCK_WOULD_BLOCK(SOCK_ERRNO)) {
            // The protocol is newline-framed, so a half-written line does not
            // merely go missing: it leaves the server parsing the next one at
            // the wrong offset. Wait briefly rather than dropping the tail.
            if (++stalls > 100) return false;
            SDL_Delay(1);
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

bool NetBotConnection::SendLine(const std::string& command) {
    if (sockfd < 0) return false;
    const std::string line =
        "FB/" + std::to_string(kProtoMajor) + "." + std::to_string(kProtoMinor) +
        " " + command + "\n";
    lastSendTicks = SDL_GetTicks();
    return SendAllBytes(sockfd, line.c_str(), line.size());
}

bool NetBotConnection::JoinRoom(const std::string& host, int port,
                                const std::string& roomCreator,
                                const std::string& botNick) {
    Leave();
    socket_init();

    sockfd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (sockfd == static_cast<int>(INVALID_SOCKET)) {
        sockfd = -1;
        return false;
    }

    struct addrinfo hints;
    struct addrinfo* res = nullptr;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || res == nullptr) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "netbot: cannot resolve %s", host.c_str());
        Leave();
        return false;
    }
    const int rc = connect(sockfd, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen));
    freeaddrinfo(res);
    if (rc < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "netbot: connect failed: %d", SOCK_ERRNO);
        Leave();
        return false;
    }

#ifdef _WIN32
    u_long nonblocking = 1;
    ioctlsocket(sockfd, FIONBIO, &nonblocking);
#else
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
#endif

    nick = botNick;
    myPlayerId = 0;
    rejectedByServer = false;
    roster.clear();
    gameMessages.clear();
    incoming.clear();

    // The server truncates nicknames the same way it does for a person, and
    // the roster is matched by nickname, so send what it will echo back.
    //
    // BOT is sent before JOIN and its answer is not waited for here -- these
    // sends are fire-and-forget, like the rest of this function. A server
    // that enforces a bot cap answers asynchronously (HandleLine watches for
    // BOT_LIMIT_REACHED and sets rejectedByServer, which disconnects); an
    // older server that has never heard of BOT just answers UNKNOWN_COMMAND
    // and otherwise ignores it, so this is safe to send unconditionally.
    // Sending it first, ahead of JOIN, means a capped bot never actually
    // takes a room seat before it is turned away.
    if (!SendLine("NICK " + nick) ||
        !SendLine("BOT") ||
        !SendLine("JOIN " + roomCreator + " " + nick)) {
        Leave();
        return false;
    }
    return true;
}

void NetBotConnection::Drain() {
    if (sockfd < 0) return;
    char buffer[kReadChunk];
    for (;;) {
        const ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            incoming.append(buffer, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) {          // server closed
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "netbot %s: server closed the connection", nick.c_str());
            Leave();
            return;
        }
        const int err = SOCK_ERRNO;
        if (SockInterrupted(err)) continue;
        if (SOCK_WOULD_BLOCK(err)) return;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "netbot %s: recv failed (%d), dropping", nick.c_str(), err);
        Leave();
        return;
    }
}

bool NetBotConnection::SendGamePayload(const std::string& payload) {
    if (sockfd < 0 || myPlayerId == 0) return false;
    std::string framed;
    framed.reserve(payload.size() + 2);
    framed.push_back(static_cast<char>(myPlayerId));
    framed.append(payload);
    framed.push_back('\n');
    lastSendTicks = SDL_GetTicks();
    return SendAllBytes(sockfd, framed.c_str(), framed.size());
}

void NetBotConnection::Leave() {
    if (sockfd >= 0) {
        SendLine("PART");
        SOCKET_CLOSE(sockfd);
    }
    sockfd = -1;
    myPlayerId = 0;
    roster.clear();
    gameMessages.clear();
    incoming.clear();
}

#else  // __WASM_PORT__

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

namespace {

constexpr int kProtoMajor = 1;
constexpr int kProtoMinor = 3;

}  // namespace

// A browser tab cannot open the raw TCP socket the native half uses, but it is
// not limited to a single network connection either: a tab may hold several
// WebSockets at once, so a bot is simply a second WebSocket to the same server
// the host is already talking to. The transport is the only part that differs
// -- handshake order, framing, line splitting and the keepalive are all the
// shared code above, and the call sites in MainMenu never notice.
struct BotSocketHandle {
    EMSCRIPTEN_WEBSOCKET_T socket;
    NetBotConnection* owner;

    // The callbacks are static members of this struct rather than free
    // functions so that NetBotConnection's `friend struct BotSocketHandle`
    // declaration covers them: they need to reach private state (nick,
    // roomCreator, incoming, sockfd) that the public interface deliberately
    // does not expose. Being static member functions they are ordinary
    // function pointers, which is all emscripten_websocket_set_*_callback
    // wants.
    static EM_BOOL OnOpen(int eventType, const EmscriptenWebSocketOpenEvent* e,
                          void* userData);
    static EM_BOOL OnMessage(int eventType, const EmscriptenWebSocketMessageEvent* e,
                             void* userData);
    static EM_BOOL OnClose(int eventType, const EmscriptenWebSocketCloseEvent* e,
                           void* userData);
    static EM_BOOL OnError(int eventType, const EmscriptenWebSocketErrorEvent* e,
                           void* userData);
};

EM_BOOL BotSocketHandle::OnOpen(int, const EmscriptenWebSocketOpenEvent*, void* userData) {
    BotSocketHandle* handle = static_cast<BotSocketHandle*>(userData);
    if (!handle || !handle->owner) return EM_TRUE;
    NetBotConnection* bot = handle->owner;

    // Native JoinRoom sends these three the moment connect() returns. A
    // WebSocket is not usable until its onopen fires -- after JoinRoom has
    // already returned -- so the same handshake moves here instead.
    //
    // BOT is sent before JOIN and its answer is not waited for: a server that
    // enforces a bot cap answers asynchronously (HandleLine watches for
    // BOT_LIMIT_REACHED and sets rejectedByServer, which disconnects), and an
    // older server that has never heard of BOT just answers UNKNOWN_COMMAND
    // and otherwise ignores it, so this is safe to send unconditionally.
    // Sending it first, ahead of JOIN, means a capped bot never actually
    // takes a room seat before it is turned away.
    if (!bot->SendLine("NICK " + bot->nick) ||
        !bot->SendLine("BOT") ||
        !bot->SendLine("JOIN " + bot->roomCreator + " " + bot->nick)) {
        bot->Leave();
    }
    return EM_TRUE;
}

EM_BOOL BotSocketHandle::OnMessage(int, const EmscriptenWebSocketMessageEvent* e,
                                   void* userData) {
    BotSocketHandle* handle = static_cast<BotSocketHandle*>(userData);
    if (!handle || !handle->owner) return EM_TRUE;
    if (!e->data || e->numBytes == 0) return EM_TRUE;

    // Append the bytes and stop: Update()'s line-splitting loop does the rest.
    // The server newline-terminates both lobby text and in-game binary frames,
    // and HandleLine's myPlayerId != 0 gate already tells a game frame from a
    // lobby line, so there is nothing for a NetworkClient-style IN_GAME-aware
    // parser to add here -- reimplementing one would just be a second,
    // divergent copy of the same framing logic.
    handle->owner->incoming.append(reinterpret_cast<const char*>(e->data), e->numBytes);
    return EM_TRUE;
}

EM_BOOL BotSocketHandle::OnClose(int, const EmscriptenWebSocketCloseEvent*, void* userData) {
    BotSocketHandle* handle = static_cast<BotSocketHandle*>(userData);
    if (!handle || !handle->owner) return EM_TRUE;
    // Mark the connection dead and do nothing else. The BotSocketHandle that
    // carries this very userData is freed by Leave(), so tearing anything
    // down here would race a Leave() that is about to run -- or already has
    // -- and touch freed memory. Clearing sockfd is the whole job: it is what
    // IsConnected() reads, and Leave() still frees the handle when it runs.
    handle->owner->sockfd = -1;
    return EM_TRUE;
}

EM_BOOL BotSocketHandle::OnError(int, const EmscriptenWebSocketErrorEvent*, void* userData) {
    BotSocketHandle* handle = static_cast<BotSocketHandle*>(userData);
    if (!handle || !handle->owner) return EM_TRUE;
    // Same minimal treatment as OnClose. A socket error is followed by a close
    // event, but not always before some other code notices, so the dead flag
    // is set here as well; real teardown is still exclusively Leave()'s job.
    handle->owner->sockfd = -1;
    return EM_TRUE;
}

bool NetBotConnection::SendLine(const std::string& command) {
    if (sockfd < 0) return false;
    const std::string line =
        "FB/" + std::to_string(kProtoMajor) + "." + std::to_string(kProtoMinor) +
        " " + command + "\n";
    lastSendTicks = SDL_GetTicks();
    const EMSCRIPTEN_RESULT result =
        emscripten_websocket_send_utf8_text(sockfd, line.c_str());
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "netbot %s: send failed (%d)",
                    nick.c_str(), static_cast<int>(result));
        return false;
    }
    return true;
}

bool NetBotConnection::JoinRoom(const std::string& host, int port,
                                const std::string& roomCreator,
                                const std::string& botNick) {
    Leave();

    // Stash the names the handshake needs before the connection even opens:
    // on WASM the NICK/BOT/JOIN commands cannot go out until the socket's
    // onopen fires, which is long after this function has returned, so
    // OnOpen reaches back here for both.
    nick = botNick;
    this->roomCreator = roomCreator;
    myPlayerId = 0;
    rejectedByServer = false;
    roster.clear();
    gameMessages.clear();
    incoming.clear();

    // Browsers block mixed content outright, so the scheme has to follow the
    // page's own: wss:// when served over HTTPS, ws:// otherwise. This is the
    // same check NetworkClient::Connect uses, and getting it wrong means the
    // browser refuses the connection before any bytes move.
    const char* scheme = (EM_ASM_INT({ return location.protocol === 'https:' ? 1 : 0; }))
                         ? "wss://" : "ws://";
    std::string wsUrl = scheme;
    wsUrl += host;
    wsUrl += ":";
    wsUrl += std::to_string(port);

    BotSocketHandle* handle = new BotSocketHandle();
    handle->owner = this;

    EmscriptenWebSocketCreateAttributes attrs;
    emscripten_websocket_init_create_attributes(&attrs);
    attrs.url = wsUrl.c_str();
    attrs.protocols = nullptr;
    attrs.createOnMainThread = EM_TRUE;

    EMSCRIPTEN_WEBSOCKET_T ws = emscripten_websocket_new(&attrs);
    if (ws <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "netbot %s: failed to create WebSocket", nick.c_str());
        delete handle;
        // sockfd is still -1 from Leave() above, so IsConnected() stays false.
        return false;
    }

    handle->socket = ws;
    wsHandle = handle;
    // Emscripten's socket type is just an int and valid handles are > 0, so
    // the handle is stored in sockfd exactly as a native fd would be. That
    // keeps the inlined IsConnected() (sockfd >= 0) correct on both platforms
    // with no change to the method; the only oddity is that this "fd" is also
    // the value the websocket API takes back in its calls.
    sockfd = ws;

    emscripten_websocket_set_onopen_callback(ws, handle, BotSocketHandle::OnOpen);
    emscripten_websocket_set_onmessage_callback(ws, handle, BotSocketHandle::OnMessage);
    emscripten_websocket_set_onclose_callback(ws, handle, BotSocketHandle::OnClose);
    emscripten_websocket_set_onerror_callback(ws, handle, BotSocketHandle::OnError);

    return true;
}

void NetBotConnection::Drain() {
    // Nothing to poll: WebSocket bytes arrive through OnMessage, which appends
    // them straight onto `incoming`. Update() still splits that buffer into
    // lines on every platform.
}

bool NetBotConnection::SendGamePayload(const std::string& payload) {
    if (sockfd < 0 || myPlayerId == 0) return false;
    std::string framed;
    framed.reserve(payload.size() + 2);
    framed.push_back(static_cast<char>(myPlayerId));
    framed.append(payload);
    framed.push_back('\n');
    lastSendTicks = SDL_GetTicks();
    // This has to be a binary frame: that is what routes it to the server's
    // process_msg_prio instead of process_msg, which would expect the
    // "FB/1.3 " text-protocol prefix that game messages deliberately do not
    // carry (the same reason NetworkClient::SendGameData sends binary).
    const EMSCRIPTEN_RESULT result =
        emscripten_websocket_send_binary(sockfd, framed.data(),
                                         static_cast<uint32_t>(framed.size()));
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "netbot %s: game send failed (%d)",
                    nick.c_str(), static_cast<int>(result));
        return false;
    }
    return true;
}

void NetBotConnection::Leave() {
    BotSocketHandle* handle = static_cast<BotSocketHandle*>(wsHandle);
    if (handle) {
        // Tell the server we are going while the socket is still open -- a bot
        // that vanishes without PART is left sitting in the lobby forever.
        // sockfd is the test, not handle->socket: if onclose has already run
        // it set sockfd to -1 and there is no peer left to tell.
        if (sockfd >= 0) SendLine("PART");

        // Detach before tearing down so a callback that fires during
        // close/delete finds owner == nullptr and does nothing instead of
        // touching a connection that is being destroyed.
        handle->owner = nullptr;
        if (handle->socket > 0) {
            // Taken from handle->socket, never from sockfd: onclose may have
            // cleared sockfd already, and the socket still has to be closed
            // and deleted either way before the handle itself is freed.
            emscripten_websocket_close(handle->socket, 1000, "");
            emscripten_websocket_delete(handle->socket);
        }
        delete handle;
        wsHandle = nullptr;
    }
    sockfd = -1;
    myPlayerId = 0;
    roster.clear();
    gameMessages.clear();
    incoming.clear();
}

#endif  // __WASM_PORT__
