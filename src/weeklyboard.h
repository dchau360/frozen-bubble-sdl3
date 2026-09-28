#ifndef WEEKLYBOARD_H
#define WEEKLYBOARD_H

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

// The online lobby's weekly rankings (protocol 1.5, fb-server's WEEKLY
// command -- see weekly_command in server/game.c). Transport-independent, so
// native and WASM share it and tests/weeklyboard_parse_test.cpp can drive it
// without a socket.
struct WeeklyBoard {
    // Monday 00:00 UTC that started this week, as a Unix time.
    int64_t weekStart = 0;
    // Highest first, as the server sent them. Ties share a rank; see Rank().
    std::vector<std::pair<std::string, int>> wins, losses, popped;
    // The asking player's own line; hasMine is false before their first round.
    bool hasMine = false;
    int myWins = 0, myLosses = 0, myPopped = 0;
    int myWinsRank = 0, myLossesRank = 0, myPoppedRank = 0;  // 0 = unranked
    // Round-wins rank of each player currently in the lobby who has one --
    // the "#2" badge beside names in the lobby's Online sidebar.
    std::map<std::string, int> lobbyWinsRank;

    // Parses the payload after "WEEKLY: ". False (board left untouched) on
    // anything malformed. Fields past the sixth are ignored, so a newer
    // server can add more without breaking this client.
    bool Parse(const std::string& payload);

    // Competition rank ("1, 2, 2, 4") of list[i].
    static int Rank(const std::vector<std::pair<std::string, int>>& list, size_t i);
};

#endif
