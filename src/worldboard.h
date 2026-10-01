#ifndef WORLDBOARD_H
#define WORLDBOARD_H

#include <string>
#include <vector>

// One world board for classic single-player runs (furthest level or most
// points, in one input track), as fb-server's HISCORES command sends it
// (protocol 1.7, see server/hiscores.h):
//
//   <week_start> <alltime> <week> <me>
//
// each list "nick#tag=level/time_ms/points,..." or "-", me
// "arank,alevel,atime,apoints,wrank,wlevel,wtime,wpoints" or "-". Level 101
// means the whole set was cleared; points is 0 on a furthest-level board.
// Kept free of SDL and networking so it can be tested alone.
struct WorldBoard {
    struct Entry {
        std::string name;  // "nick#tag"
        int level = 0;
        int timeMs = 0;
        int points = 0;
        int track = 0;  // 0 keyboard/gamepad, 1 mouse/touch; set by Merge()
    };
    struct Mine {
        int rank = 0, level = 0, timeMs = 0, points = 0;  // all zero: no run in that scope
    };

    long long weekStart = 0;
    std::vector<Entry> alltime, week;
    bool hasMine = false;
    Mine myAlltime, myWeek;

    // False (and *this unchanged) if the payload isn't a HISCORES reply.
    bool Parse(const std::string& payload);

    // Competition rank of list[i] ("1, 2, 2, 4"): equal runs share a rank.
    static int Rank(const std::vector<Entry>& list, size_t i);

    // One list out of a keyboard/gamepad list and a mouse/touch list, for
    // showing both inputs at once: each entry tagged with its track, ordered
    // the way fb-server orders that kind of board (furthest level, then lower
    // time; or most points, then higher level, then lower time). A stable
    // merge, so an exact tie keeps keyboard/gamepad first.
    static std::vector<Entry> Merge(const std::vector<Entry>& keyboard,
                                    const std::vector<Entry>& mouse, bool points);

    // "12,345 pts".
    static std::string PointsLabel(int points);
    // "won!" for a cleared set, else "level N".
    static std::string LevelLabel(int level);
    // m'ss" like the local tables, h:mm'ss" past an hour.
    static std::string TimeLabel(int timeMs);
};

#endif
