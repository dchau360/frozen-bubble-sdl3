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

    // Lobby replies versus in-game payloads.
    {
        CHECK(!IsGameMessageLine("FB/1.3 OK"));
        CHECK(!IsGameMessageLine("FB/1.3 PUSH: JOINED: bot01"));
        CHECK(!IsGameMessageLine(""));
        // Player ids are small integers, well below any printable byte.
        CHECK(IsGameMessageLine(std::string(1, '\x03') + "f1.234:5"));
        CHECK(IsGameMessageLine(std::string(1, '\x01') + "n"));
        // 0x20 is a space -- the first byte of no line this protocol sends,
        // and the boundary the check is written against.
        CHECK(!IsGameMessageLine(" leading space"));
    }

    // Seat assignment: a four-board game where boards 2 and 3 are our bots.
    {
        // The room lists us (66), two humans (70, 72) and our two bots
        // (67, 68) -- all five ids, in the room's own order.
        const std::vector<int> room = {66, 67, 68, 70, 72};
        const std::map<int, int> botSeats = {{2, 67}, {3, 68}};
        const std::map<int, int> seats = AssignRemoteSeats(room, 66, botSeats, 4);
        // Only board 1 is free; the humans take it in room order.
        CHECK(seats.size() == 1);
        CHECK(seats.at(1) == 70);
        // Neither our own id nor a bot's was handed out.
        for (const auto& seat : seats) {
            CHECK(seat.second != 66);
            CHECK(seat.second != 67 && seat.second != 68);
        }
    }

    // Bots on the low boards must not push a remote player onto a bot board.
    {
        const std::vector<int> room = {10, 11, 12, 13};
        const std::map<int, int> botSeats = {{1, 11}, {2, 12}};
        const std::map<int, int> seats = AssignRemoteSeats(room, 10, botSeats, 4);
        CHECK(seats.size() == 1);
        CHECK(seats.count(1) == 0 && seats.count(2) == 0);
        CHECK(seats.at(3) == 13);
    }

    // No bots: unchanged from the plain case -- boards fill from 1 in room order.
    {
        const std::vector<int> room = {4, 5, 6};
        const std::map<int, int> seats = AssignRemoteSeats(room, 4, {}, 3);
        CHECK(seats.size() == 2);
        CHECK(seats.at(1) == 5);
        CHECK(seats.at(2) == 6);
    }

    // More players in the room than boards in the game: the surplus is dropped
    // rather than written past the last board.
    {
        const std::vector<int> room = {1, 2, 3, 4, 5};
        const std::map<int, int> seats = AssignRemoteSeats(room, 1, {}, 3);
        CHECK(seats.size() == 2);
        CHECK(seats.count(3) == 0);
    }

    // Every board is a bot: nothing left to seat, and no infinite scan.
    {
        const std::map<int, int> botSeats = {{1, 8}, {2, 9}};
        const std::map<int, int> seats = AssignRemoteSeats({7, 8, 9}, 7, botSeats, 3);
        CHECK(seats.empty());
    }

    if (failures == 0) std::printf("netbot tests passed\n");
    return failures == 0 ? 0 : 1;
}
