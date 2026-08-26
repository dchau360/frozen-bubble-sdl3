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

#ifndef NETBOT_H
#define NETBOT_H

#include <deque>
#include <map>
#include <string>

// The id-to-nickname map the server sends when a game starts, as
// <idByte><nick>,<idByte><nick>,... Split out from the connection so the
// parsing -- a binary format with an embedded separator, where a mis-read
// yields a plausible-looking but wrong player id -- can be tested without a
// socket. `myNick` selects which entry is ours; 0 means we are not in it.
struct GameCanStartRoster {
    std::map<int, std::string> players;
    int myPlayerId = 0;
};
GameCanStartRoster ParseGameCanStart(const std::string& payload,
                                     const std::string& myNick);

// True for a line that carries an in-game payload rather than a lobby reply.
// Lobby lines are text beginning "FB/"; in-game ones are prefixed with the
// sender's player id, a low byte no printable line starts with.
bool IsGameMessageLine(const std::string& line);

// One bot's connection to the game server.
//
// The server tags every in-game message with the connection it arrived on, so
// a bot cannot be spoken for over the host's socket -- it needs its own. This
// is deliberately not NetworkClient: a bot needs the lobby handshake, the
// player-id mapping and a raw game-message pipe, and none of the lobby UI
// state, server browsing, follow registration or chat history that class
// carries. Keeping it separate also keeps NetworkClient a singleton, which
// the rest of the code assumes.
//
// The bot's board is simulated by the host's own BubbleGame, in a normal
// player slot; this class is only the wire.
class NetBotConnection {
public:
    NetBotConnection() = default;
    ~NetBotConnection();

    NetBotConnection(const NetBotConnection&) = delete;
    NetBotConnection& operator=(const NetBotConnection&) = delete;

    // Connect and take a seat in `roomCreator`'s room. Blocking, but only for
    // as long as a TCP connect to a server the host is already talking to
    // takes. Returns false and leaves the object closed on any failure.
    bool JoinRoom(const std::string& host, int port,
                  const std::string& roomCreator, const std::string& nick);

    // Pump the socket. Lobby lines are consumed here; in-game payloads are
    // queued for TakeGameMessage.
    void Update();

    bool IsConnected() const { return sockfd >= 0; }
    // Non-zero once the server has announced the game and told us who we are.
    int PlayerId() const { return myPlayerId; }
    bool GameStarted() const { return myPlayerId != 0; }
    const std::string& Nick() const { return nick; }
    const std::map<int, std::string>& Roster() const { return roster; }

    // Send one in-game payload ("f1.234:5", "s3:7:2:...", "g4", "n", ...)
    // as this bot. No-op before the game has started.
    bool SendGamePayload(const std::string& payload);

    // Oldest queued incoming game message, or false when there are none.
    // `senderId` is the player it came from.
    bool TakeGameMessage(int* senderId, std::string* payload);

    // PART the room and close. Safe to call twice.
    void Leave();

private:
    bool SendLine(const std::string& command);
    // Reads whatever is available without blocking and splits it into lines.
    void Drain();
    void HandleLine(const std::string& line);

    int sockfd = -1;
    int myPlayerId = 0;
    std::string nick;
    std::string incoming;                       // partial line carry-over
    std::map<int, std::string> roster;          // player id -> nick
    std::deque<std::pair<int, std::string>> gameMessages;
};

#endif  // NETBOT_H
