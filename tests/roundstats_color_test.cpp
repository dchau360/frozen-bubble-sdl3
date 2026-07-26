#include "roundstats_color.h"

#include <iostream>

static bool Expect(RoundStatsColorKind actual, RoundStatsColorKind expected, const char *message) {
    if (actual == expected) return true;
    std::cerr << message << '\n';
    return false;
}

int main() {
    bool ok = true;
    // A winning Team 1 player must remain red (team palette index 0), not
    // switch to the generic green winner highlight.
    ok &= Expect(RoundStatsRowColorKind(true, 1, true), RoundStatsColorKind::TEAM,
                 "winning team player used winner highlight instead of team color");

    // Outside Team Mode, winners retain the existing winner highlight.
    ok &= Expect(RoundStatsRowColorKind(false, 1, true), RoundStatsColorKind::WINNER,
                 "non-team winner lost winner highlight");
    ok &= Expect(RoundStatsRowColorKind(false, 1, false), RoundStatsColorKind::NORMAL,
                 "ordinary non-team player did not use normal color");
    return ok ? 0 : 1;
}
