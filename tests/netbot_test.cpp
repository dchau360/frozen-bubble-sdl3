// The bot's wire format. A mis-read here does not fail loudly -- it yields a
// plausible-looking but wrong player id, and the server then attributes the
// bot's moves to somebody else -- so the binary roster format and the
// lobby/in-game line split are pinned here.

#include "netbot.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

// <idByte><nick>,<idByte><nick>,...
static std::string Entry(int id, const std::string& nick) {
    return std::string(1, static_cast<char>(id)) + nick;
}

int main() {
    // A normal five-player roster, and we are one of them.
    {
        const std::string payload =
            Entry(3, "alice") + "," + Entry(7, "bot01") + "," +
            Entry(9, "bot02") + "," + Entry(11, "carol");
        const GameCanStartRoster roster = ParseGameCanStart(payload, "bot01");
        CHECK(roster.players.size() == 4);
        CHECK(roster.players.at(3) == "alice");
        CHECK(roster.players.at(7) == "bot01");
        CHECK(roster.players.at(9) == "bot02");
        CHECK(roster.players.at(11) == "carol");
        CHECK(roster.myPlayerId == 7);
    }

    // A nickname we do not hold leaves the id at zero rather than guessing,
    // which is what stops a bot sending as somebody else.
    {
        const std::string payload = Entry(3, "alice") + "," + Entry(4, "bob");
        const GameCanStartRoster roster = ParseGameCanStart(payload, "nobody");
        CHECK(roster.players.size() == 2);
        CHECK(roster.myPlayerId == 0);
    }

    // Ids are raw bytes, so one above 127 must not come back sign-extended.
    {
        const std::string payload = Entry(200, "highid");
        const GameCanStartRoster roster = ParseGameCanStart(payload, "highid");
        CHECK(roster.myPlayerId == 200);
        CHECK(roster.players.count(200) == 1);
    }

    // A trailing separator, and an empty entry, must not invent a player.
    {
        const std::string payload = Entry(5, "solo") + ",";
        const GameCanStartRoster roster = ParseGameCanStart(payload, "solo");
        CHECK(roster.players.size() == 1);
        CHECK(roster.myPlayerId == 5);
    }

    // Empty payload: no players, no id, no crash.
    {
        const GameCanStartRoster roster = ParseGameCanStart("", "bot01");
        CHECK(roster.players.empty());
        CHECK(roster.myPlayerId == 0);
    }

    // A nickname that is a prefix of ours is not ours. The server truncates
    // to ten characters, so near-misses are the normal case, not an edge one.
    {
        const std::string payload = Entry(2, "android_us") + "," + Entry(6, "android");
        const GameCanStartRoster roster = ParseGameCanStart(payload, "android_us");
        CHECK(roster.myPlayerId == 2);
    }

    // Seat assignment. A host's bots are room players like any other, so
    // they are seated by the same rule on every client -- what makes a board
    // a bot's is the host recognising the id afterwards, not the seating.
    {
        // Us (66), our two bots (67, 68) and two humans (70, 72).
        const std::vector<int> room = {66, 67, 68, 70, 72};
        const std::map<int, int> seats = AssignRemoteSeats(room, 66, 5);
        CHECK(seats.size() == 4);
        CHECK(seats.at(1) == 67);
        CHECK(seats.at(2) == 68);
        CHECK(seats.at(3) == 70);
        CHECK(seats.at(4) == 72);
        // Board 0 is ours and is never handed out.
        CHECK(seats.count(0) == 0);
        for (const auto& seat : seats) CHECK(seat.second != 66);
    }

    // We are not first in the room's order: the boards still fill from 1 with
    // no gap where we were skipped.
    {
        const std::vector<int> room = {4, 5, 6};
        const std::map<int, int> seats = AssignRemoteSeats(room, 5, 3);
        CHECK(seats.size() == 2);
        CHECK(seats.at(1) == 4);
        CHECK(seats.at(2) == 6);
    }

    // More players in the room than boards in the game: the surplus is dropped
    // rather than written past the last board.
    {
        const std::vector<int> room = {1, 2, 3, 4, 5};
        const std::map<int, int> seats = AssignRemoteSeats(room, 1, 3);
        CHECK(seats.size() == 2);
        CHECK(seats.count(3) == 0);
    }

    // A one-board game seats nobody.
    {
        const std::map<int, int> seats = AssignRemoteSeats({7, 8}, 7, 1);
        CHECK(seats.empty());
    }

    // What a client may ignore from a seat it simulates itself.
    {
        // Board state: already simulated locally, so replaying it would run
        // the same move twice.
        CHECK(!IsConnectionLevelOpcode('f'));   // fire
        CHECK(!IsConnectionLevelOpcode('s'));   // stick
        CHECK(!IsConnectionLevelOpcode('g'));   // attack
        CHECK(!IsConnectionLevelOpcode('M'));   // malus stick
        CHECK(!IsConnectionLevelOpcode('F'));   // round win
        CHECK(!IsConnectionLevelOpcode('S'));   // round stats
        // Connection state: counted per connection, so a hosted bot's own
        // must be processed like any other player's. Ignoring 'n' here
        // stalls every round after the first.
        CHECK(IsConnectionLevelOpcode('n'));    // ready for next round
        CHECK(IsConnectionLevelOpcode('l'));    // seat left
    }

    // Being kicked, versus being told someone else was.
    {
        CHECK(IsKickedMePush("FB/1.3 PUSH: KICKED"));
        CHECK(IsKickedMePush("FB/1.3 PUSH: KICKED\r"));
        // The notice about another player must not make this bot quit --
        // otherwise kicking one bot would empty the room of all of them.
        CHECK(!IsKickedMePush("FB/1.3 PUSH: KICKED: bot1-998"));
        CHECK(!IsKickedMePush("FB/1.3 PUSH: JOINED: someone"));
        CHECK(!IsKickedMePush(""));
    }

    // The server's answer to our own BOT command, when its -b cap is
    // already full. Wire format matches net.c's send_line ("FB/M.m
    // <command>: <reply>"), so the command name (BOT) is part of the line.
    {
        CHECK(IsBotLimitReachedReply("FB/1.3 BOT: BOT_LIMIT_REACHED"));
        CHECK(!IsBotLimitReachedReply("FB/1.3 BOT: OK"));
        CHECK(!IsBotLimitReachedReply("FB/1.3 CREATE: GAME_FULL"));
        CHECK(!IsBotLimitReachedReply(""));
    }

    // The server's answer to our own JOIN command, when the room did not
    // exist, was full, or the nickname collided. Same wire format as the
    // BOT-cap reply above, but anchored on "JOIN: " so an unrelated command
    // that happens to mention one of these words does not false-positive.
    {
        std::string reason;
        CHECK(IsJoinRejectedReply("FB/1.3 JOIN: NO_SUCH_GAME", &reason));
        CHECK(reason == "NO_SUCH_GAME");
        CHECK(IsJoinRejectedReply("FB/1.3 JOIN: GAME_FULL", &reason));
        CHECK(reason == "GAME_FULL");
        CHECK(IsJoinRejectedReply("FB/1.3 JOIN: NICK_IN_USE", &reason));
        CHECK(reason == "NICK_IN_USE");
        CHECK(IsJoinRejectedReply("FB/1.3 JOIN: ALREADY_IN_GAME", &reason));
        CHECK(reason == "ALREADY_IN_GAME");
        CHECK(IsJoinRejectedReply("FB/1.3 JOIN: INVALID_NICK", &reason));
        CHECK(reason == "INVALID_NICK");
        // A successful JOIN, and an unrelated command that happens to
        // mention GAME_FULL, must not be mistaken for a rejection of ours.
        CHECK(!IsJoinRejectedReply("FB/1.3 JOIN: OK", nullptr));
        CHECK(!IsJoinRejectedReply("FB/1.3 CREATE: GAME_FULL", nullptr));
        CHECK(!IsJoinRejectedReply("", nullptr));
        // A null reason pointer must not crash a caller that only wants
        // the bool (Update()'s HandleLine passes one, but a future caller
        // that only checks the return value should not have to).
        CHECK(IsJoinRejectedReply("FB/1.3 JOIN: GAME_FULL", nullptr));
    }

    // How many bots a host may ask for. Bots take real seats, so the ceiling
    // is what the room has left plus the ones already connected.
    {
        // A 5-cap room with the host alone in it: four seats free.
        CHECK(MaxRoomBots(1, 5, 0) == 4);
        // Two humans in a 5-cap room: three free, so three bots.
        CHECK(MaxRoomBots(2, 5, 0) == 3);
        // Two of those seats are already our bots, so the answer is the same
        // three -- asking for what is already connected must stay legal, or
        // the count could never be reduced.
        CHECK(MaxRoomBots(4, 5, 2) == 3);
        // A full room with no bots of ours: none.
        CHECK(MaxRoomBots(5, 5, 0) == 0);
        // A full room that is full *of* our bots: they may stay, or go.
        CHECK(MaxRoomBots(5, 5, 3) == 3);
        // A big room is still capped at four bots, not at its own size.
        CHECK(MaxRoomBots(1, 20, 0) == kMaxRoomBots);
        // Over-full (a race with a joiner) must not go negative.
        CHECK(MaxRoomBots(7, 5, 0) == 0);
    }

    // A bot's skill is a plain stored value, set once by the lobby when it
    // is added and read back by SeatBots (bubblegame_net.cpp) when the round
    // starts -- no socket involved, so it is safe to construct one here
    // without ever calling JoinRoom.
    {
        NetBotConnection bot;
        // Matches LocalMPBotSkillName's Med tier: the only sane default for
        // a bot nobody ever assigned a skill to.
        CHECK(bot.Skill() == 1);
        bot.SetSkill(0);
        CHECK(bot.Skill() == 0);
        bot.SetSkill(2);
        CHECK(bot.Skill() == 2);
    }

    if (failures == 0) std::printf("netbot tests passed\n");
    return failures == 0 ? 0 : 1;
}
