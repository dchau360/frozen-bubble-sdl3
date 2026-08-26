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

bool IsGameMessageLine(const std::string& line) {
    return !line.empty() && static_cast<unsigned char>(line[0]) < 0x20;
}

#ifndef __WASM_PORT__

#include "socket_compat.h"

#include <SDL3/SDL.h>

#include <cerrno>
#include <cstring>
#if !defined(_WIN32)
#include <netdb.h>   // getaddrinfo; Windows has it in ws2tcpip.h, via socket_compat.h
#endif
#include <string>

namespace {

constexpr int kProtoMajor = 1;
constexpr int kProtoMinor = 3;
constexpr char kStartPush[] = "PUSH: GAME_CAN_START: ";
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

NetBotConnection::~NetBotConnection() { Leave(); }

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
    roster.clear();
    gameMessages.clear();
    incoming.clear();

    // The server truncates nicknames the same way it does for a person, and
    // the roster is matched by nickname, so send what it will echo back.
    if (!SendLine("NICK " + nick) ||
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

    if (IsGameMessageLine(line)) {
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

bool NetBotConnection::TakeGameMessage(int* senderId, std::string* payload) {
    if (gameMessages.empty()) return false;
    if (senderId) *senderId = gameMessages.front().first;
    if (payload) *payload = gameMessages.front().second;
    gameMessages.pop_front();
    return true;
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

// A browser tab cannot open the extra raw sockets a bot needs, and the
// WebSocket path would need one proxied connection per bot. Bots are a
// native/mobile feature; the stubs keep the call sites uniform.
NetBotConnection::~NetBotConnection() {}
bool NetBotConnection::JoinRoom(const std::string&, int, const std::string&,
                                const std::string&) { return false; }
void NetBotConnection::Update() {}
bool NetBotConnection::SendGamePayload(const std::string&) { return false; }
bool NetBotConnection::TakeGameMessage(int*, std::string*) { return false; }
void NetBotConnection::Leave() {}
bool NetBotConnection::SendLine(const std::string&) { return false; }
void NetBotConnection::Drain() {}
void NetBotConnection::HandleLine(const std::string&) {}

#endif  // __WASM_PORT__
