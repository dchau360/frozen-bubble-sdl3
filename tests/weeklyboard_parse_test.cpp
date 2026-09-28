// Client half of the WEEKLY command (protocol 1.5): parsing fb-server's reply
// into the lobby's weekly-rankings screen. The server half is covered against
// the real binary by server-weekly-test.
#include "weeklyboard.h"
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)

int main() {
    WeeklyBoard b;
    CHECK(b.Parse("1790553600 bob=5,alice=5,carol=2 alice=1 alice=50,bob=20 5,1,50,1,1,1"));
    CHECK(b.weekStart == 1790553600);
    CHECK(b.wins.size() == 3 && b.wins[0].first == "bob" && b.wins[2].second == 2);
    CHECK(b.losses.size() == 1);
    CHECK(b.popped[1].first == "bob" && b.popped[1].second == 20);
    CHECK(b.hasMine && b.myWins == 5 && b.myLosses == 1 && b.myPopped == 50);
    CHECK(b.myWinsRank == 1 && b.myPoppedRank == 1);

    // Competition ranking: ties share a rank, the next rank skips.
    CHECK(WeeklyBoard::Rank(b.wins, 0) == 1);
    CHECK(WeeklyBoard::Rank(b.wins, 1) == 1);
    CHECK(WeeklyBoard::Rank(b.wins, 2) == 3);

    // Empty lists and no line of your own.
    WeeklyBoard e;
    CHECK(e.Parse("1790553600 - - - -"));
    CHECK(e.wins.empty() && e.losses.empty() && e.popped.empty() && !e.hasMine);

    // Malformed replies are rejected and leave the previous board intact.
    CHECK(!b.Parse(""));
    CHECK(!b.Parse("1790553600 - - -"));                  // missing field
    CHECK(!b.Parse("1790553600 - - - - extra"));          // extra field
    CHECK(!b.Parse("x - - - -"));                         // bad week
    CHECK(!b.Parse("1790553600 bob - - -"));              // no '='
    CHECK(!b.Parse("1790553600 =3 - - -"));               // no nick
    CHECK(!b.Parse("1790553600 bob=x - - -"));            // bad count
    CHECK(!b.Parse("1790553600 - - - 1,2,3"));            // short own line
    CHECK(b.wins.size() == 3 && b.hasMine);
    std::cout << "weeklyboard-parse-test: ok\n";
    return 0;
}
