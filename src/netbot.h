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
#include <vector>

#ifdef __WASM_PORT__
struct BotSocketHandle;  // defined in netbot.cpp's WASM half; see friend decl below
#endif

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

// Which board each of the room's other players is rendered on.
//
// `roomPlayerIds` is every id the room announced, in its own order. Boards
// fill from index 1 up (index 0 is always the local player), stopping at
// `playerCount`. A host's bots are in that list like anyone else and are
// seated by the same rule -- every client runs this over the same room, so
// they agree on which board is whose, and only the host knows which of them
// it is playing itself.
//
// Split out from the caller because getting it wrong is quiet: a board
// assigned to the wrong id replays another player's shots for a whole round.
std::map<int, int> AssignRemoteSeats(const std::vector<int>& roomPlayerIds,
                                     int myPlayerId,
                                     int playerCount);

// The most bots a host may have in a room right now.
//
// `roomPlayers` is everyone currently in the room, the host's `currentBots`
// among them, and `maxPlayers` is the room's cap. Bots take real seats, so
// the ceiling has to leave room for the people already there -- and asking
// for fewer than are connected is always allowed, since that is how they are
// removed.
inline constexpr int kMaxRoomBots = 4;
int MaxRoomBots(int roomPlayers, int maxPlayers, int currentBots);

// True for the push that tells *this* connection it was kicked.
//
// The server sends a bare "PUSH: KICKED" to the player it removed and
// "PUSH: KICKED: <nick>" to everyone else about that player, so the two
// forms differ only by what follows -- and reading the second as the first
// would have every bot in the room quit whenever any one of them was kicked.
bool IsKickedMePush(const std::string& line);

// True for the server's reply to our own BOT command when the server-wide
// bot cap (fb-server's -b) is already at its limit. The response is a
// lobby-command reply, not a PUSH -- it answers the BOT this connection
// itself sent, so there is no "about someone else" case to rule out the
// way IsKickedMePush has to.
bool IsBotLimitReachedReply(const std::string& line);

// Whether an in-game opcode is about the connection that sent it rather than
// about the contents of a board.
//
// This is the line that decides what a client may ignore when the sender is a
// seat it simulates itself. Board-state messages ('f' fire, 's' stick, 'g'
// attack, 'M' malus stick, 'F' win, 'S' round stats) describe a simulation
// that has already run locally, so replaying them would run it twice.
// Connection-level ones do not: 'n' says one connection is ready for the next
// round and 'l' that one is gone, and both are counted per connection -- a
// hosted bot holds its own, so its 'n' must be counted like any other
// player's, or the round waits forever for a seat that already answered.
bool IsConnectionLevelOpcode(char opcode);

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
    // queued for TakeGameMessage, and a keepalive goes out when the bot has
    // had nothing to say for a second -- the server terminates an in-game
    // connection that is silent for five (server/net.c, "in game gracetime"),
    // which a bot hits as soon as its board is out of the round.
    void Update();

    bool IsConnected() const { return sockfd >= 0; }
    // True once the server has told this connection its own BOT command
    // was refused -- the server-wide bot cap (fb-server's -b) was already
    // at its limit. Leave() disconnects on the same line that sets this,
    // but does not clear it, so a caller finding !IsConnected() can still
    // read why: capped, versus any other disconnect.
    bool WasRejectedByServer() const { return rejectedByServer; }
    // Non-zero once the server has announced the game and told us who we are.
    int PlayerId() const { return myPlayerId; }
    bool GameStarted() const { return myPlayerId != 0; }
    const std::string& Nick() const { return nick; }
    const std::map<int, std::string>& Roster() const { return roster; }

    // The BubbleAI::Skill index this bot plays at, set once by the lobby at
    // the moment it is added (SyncLobbyBots, mainmenu_netpanel.cpp) and
    // carried with the connection from then on -- a later change to the
    // room's Bot skill setting only affects bots added after that change,
    // so a room can mix difficulties ("bot1-high", "bot2-low", ...).
    // SeatBots (bubblegame_net.cpp) reads this per bot instead of one
    // room-wide value.
    void SetSkill(int s) { skill = s; }
    int Skill() const { return skill; }

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
    bool rejectedByServer = false;
    // Last time anything was written, for the keepalive below.
    unsigned lastSendTicks = 0;
    std::string nick;
    // Default matches LocalMPBotSkillName's Med tier -- only reached if a
    // caller seats a bot without ever calling SetSkill.
    int skill = 1;
#ifdef __WASM_PORT__
    // Who created the room this bot is joining. Native consumes it inside
    // JoinRoom and never needs it again; a WebSocket is asynchronous, so the
    // JOIN command cannot go out until the socket's onopen fires -- long
    // after JoinRoom has returned -- and the name has to live here until
    // then. Guarded because native never reads it and would otherwise warn
    // about an unused private field.
    std::string roomCreator;
    // BotSocketHandle* -- the Emscripten socket plus the back-pointer the
    // callbacks use to reach this object. void* keeps the emscripten headers
    // out of this file; only the WASM half of netbot.cpp casts it back.
    void* wsHandle = nullptr;
    // The callbacks are static members of BotSocketHandle and have to reach
    // the private state above (send the join handshake, append to `incoming`,
    // clear `sockfd` on close), so the struct is a friend. It is only defined
    // in the WASM half of netbot.cpp.
    friend struct BotSocketHandle;
#endif
    std::string incoming;                       // partial line carry-over
    std::map<int, std::string> roster;          // player id -> nick
    std::deque<std::pair<int, std::string>> gameMessages;
};

#endif  // NETBOT_H
