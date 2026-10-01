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
    CHECK(b.Parse("1790380800 bob#7f3a=101/5000000/0,al#c21e=40/300000/0,cy#0000=40/300000/0 "
                  "al#c21e=40/300000/0 2,40,300000,0,1,40,300000,0"));
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

    // A most-points board: points ride along, and a points tie at a
    // different level is not a shared rank.
    CHECK(b.Parse("1790380800 al#c21e=50/999999/25000,bob#7f3a=12/300000/25000 - "
                  "1,50,999999,25000,0,0,0,0"));
    CHECK(b.alltime[0].points == 25000 && b.alltime[1].level == 12);
    CHECK(WorldBoard::Rank(b.alltime, 1) == 2);
    CHECK(b.myAlltime.points == 25000 && b.myWeek.rank == 0);

    CHECK(b.Parse("1790380800 - - -"));
    CHECK(b.alltime.empty() && b.week.empty() && !b.hasMine);

    // A server that also sends each run's day (two trailing fields, for the
    // web page) still parses: the game reads the first four only.
    CHECK(b.Parse("1790380800 al#c21e=40/300000/0/DE al#c21e=40/300000/0/DE - 20727 20727"));
    CHECK(b.alltime.size() == 1 && b.alltime[0].country == "DE" && b.week.size() == 1);

    // ...and the two shot lists after the days fill each entry's shots, in
    // list order; a "-" or a list that doesn't line up leaves them at 0
    // without costing the board.
    CHECK(b.Parse("1 a#0001=40/300000/0,b#0002=20/100000/0 b#0002=20/100000/0 - 1,2 2 412,0 77"));
    CHECK(b.alltime.size() == 2 && b.alltime[0].shots == 412 && b.alltime[1].shots == 0);
    CHECK(b.week.size() == 1 && b.week[0].shots == 77);
    CHECK(b.Parse("1 a#0001=40/300000/0,b#0002=20/100000/0 b#0002=20/100000/0 - 1,2 2 412 x"));
    CHECK(b.alltime[0].shots == 0 && b.week[0].shots == 0);
    CHECK(b.Parse("1 a#0001=40/300000/0 - - 1 - 412 -"));
    CHECK(b.alltime[0].shots == 412 && b.week.empty());
    CHECK(WorldBoard::ShotsLabel(0).empty() && WorldBoard::ShotsLabel(1) == "1 shot" &&
          WorldBoard::ShotsLabel(412) == "412 shots");

    // Refused, and the board is left as it was.
    b.Parse("1790380800 x#0000=5/1000/0 - -");
    for (const char* bad : {"", "notanumber - - -", "1 x=5 - -", "1 x=0/5/0 - -", "1 x=5/1000 - -",
                            "1 - - 1,2,3", "1 - - 1,2,3,4,5,6", "1 - -"})
        CHECK(!b.Parse(bad));
    CHECK(b.alltime.size() == 1 && b.alltime[0].name == "x#0000");

    // An optional fourth field is the player's country; anything that
    // isn't two capitals is dropped, not refused.
    CHECK(b.Parse("0 a#0001=40/300000/0/FR,b#0002=20/100000/0,c#0003=10/1/0/xx,d#0004=9/1/0/ABC - -"));
    CHECK(b.alltime.size() == 4);
    CHECK(b.alltime[0].country == "FR" && b.alltime[0].level == 40);
    CHECK(b.alltime[1].country.empty() && b.alltime[2].country.empty() && b.alltime[3].country.empty());

    // Both inputs at once: merged in board order, each tagged with its track.
    {
        WorldBoard kb, ms;
        CHECK(kb.Parse("0 a#0001=40/300000/0,b#0002=20/100000/0 - -"));
        CHECK(ms.Parse("0 c#0003=40/200000/0,a#0001=30/100000/0 - -"));
        const auto lv = WorldBoard::Merge(kb.alltime, ms.alltime, false);
        CHECK(lv.size() == 4);
        CHECK(lv[0].name == "c#0003" && lv[0].track == 1);  // same level, faster
        CHECK(lv[1].name == "a#0001" && lv[1].track == 0);
        CHECK(lv[2].name == "a#0001" && lv[2].track == 1);  // one account, both inputs
        CHECK(lv[3].name == "b#0002" && lv[3].track == 0);

        CHECK(kb.Parse("0 a#0001=10/300000/9000,b#0002=50/100000/5000 - -"));
        CHECK(ms.Parse("0 c#0003=60/100000/9000 - -"));
        const auto pt = WorldBoard::Merge(kb.alltime, ms.alltime, true);
        CHECK(pt.size() == 3);
        CHECK(pt[0].name == "c#0003" && pt[1].name == "a#0001");  // points tie: higher level first
        CHECK(pt[2].name == "b#0002");
        CHECK(WorldBoard::Merge({}, {}, true).empty());
    }

    CHECK(WorldBoard::PointsLabel(48210) == "48,210 pts");
    CHECK(WorldBoard::PointsLabel(1234567) == "1,234,567 pts");
    CHECK(WorldBoard::PointsLabel(999) == "999 pts");
    CHECK(WorldBoard::LevelLabel(101) == "won!");
    CHECK(WorldBoard::LevelLabel(57) == "level 57");
    CHECK(WorldBoard::TimeLabel(725000) == "12'05\"");
    CHECK(WorldBoard::TimeLabel(3725000) == "1:02'05\"");

    if (failures) return 1;
    std::printf("worldboard parse: all checks passed\n");
    return 0;
}
