// WorldBoard::Parse (src/worldboard.cpp): the HISCORES reply from fb-server
// (protocol 1.7, server/hiscores.h) -- both lists, the asker's own line, the
// "-" placeholders, and replies it must refuse without changing the board.
#include <cstdio>
#include <string>

#include "worldboard.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    WorldBoard b;
    CHECK(b.Parse("1790380800 bob#7f3a=101/5000000,al#c21e=40/300000,cy#0000=40/300000 "
                  "al#c21e=40/300000 2,40,300000,1,40,300000"));
    CHECK(b.weekStart == 1790380800LL);
    CHECK(b.alltime.size() == 3);
    CHECK(b.alltime[0].name == "bob#7f3a" && b.alltime[0].level == 101 && b.alltime[0].timeMs == 5000000);
    CHECK(b.week.size() == 1 && b.week[0].name == "al#c21e");
    CHECK(b.hasMine);
    CHECK(b.myAlltime.rank == 2 && b.myAlltime.level == 40 && b.myAlltime.timeMs == 300000);
    CHECK(b.myWeek.rank == 1);
    // Equal runs share a rank.
    CHECK(WorldBoard::Rank(b.alltime, 0) == 1);
    CHECK(WorldBoard::Rank(b.alltime, 1) == 2);
    CHECK(WorldBoard::Rank(b.alltime, 2) == 2);

    CHECK(b.Parse("1790380800 - - -"));
    CHECK(b.alltime.empty() && b.week.empty() && !b.hasMine);

    // Refused, and the board is left as it was.
    b.Parse("1790380800 x#0000=5/1000 - -");
    for (const char* bad : {"", "notanumber - - -", "1 x=5 - -", "1 x=0/5 - -", "1 - - 1,2,3", "1 - -"})
        CHECK(!b.Parse(bad));
    CHECK(b.alltime.size() == 1 && b.alltime[0].name == "x#0000");

    CHECK(WorldBoard::LevelLabel(101) == "won!");
    CHECK(WorldBoard::LevelLabel(57) == "level 57");
    CHECK(WorldBoard::TimeLabel(725000) == "12'05\"");
    CHECK(WorldBoard::TimeLabel(3725000) == "1:02'05\"");

    if (failures) return 1;
    std::printf("worldboard parse: all checks passed\n");
    return 0;
}
