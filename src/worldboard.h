#ifndef WORLDBOARD_H
#define WORLDBOARD_H

#include <string>
#include <vector>

// One track's world board for classic single-player runs, as fb-server's
// HISCORES command sends it (protocol 1.7, see server/hiscores.h):
//
//   <week_start> <alltime> <week> <me>
//
// each list "nick#tag=level/time_ms,..." or "-", me
// "arank,alevel,atime,wrank,wlevel,wtime" or "-". Level 101 means the whole
// set was cleared. Kept free of SDL and networking so it can be tested alone.
struct WorldBoard {
    struct Entry {
        std::string name;  // "nick#tag"
        int level = 0;
        int timeMs = 0;
    };
    struct Mine {
        int rank = 0, level = 0, timeMs = 0;  // all zero: no run in that scope
    };

    long long weekStart = 0;
    std::vector<Entry> alltime, week;
    bool hasMine = false;
    Mine myAlltime, myWeek;

    // False (and *this unchanged) if the payload isn't a HISCORES reply.
    bool Parse(const std::string& payload);

    // Competition rank of list[i] ("1, 2, 2, 4"): equal runs share a rank.
    static int Rank(const std::vector<Entry>& list, size_t i);

    // "won!" for a cleared set, else "level N".
    static std::string LevelLabel(int level);
    // m'ss" like the local tables, h:mm'ss" past an hour.
    static std::string TimeLabel(int timeMs);
};

#endif
