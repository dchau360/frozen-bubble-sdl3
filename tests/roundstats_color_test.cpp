#include "roundstats_color.h"
#include "netteams.h"

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
    ok &= Expect(RoundStatsRowColorKind(1, true), RoundStatsColorKind::TEAM,
                 "winning team player used winner highlight instead of team color");

    // A free agent has no team color, so winners retain the existing winner
    // highlight and everyone else uses the ordinary row color.
    ok &= Expect(RoundStatsRowColorKind(kNoTeam, true), RoundStatsColorKind::WINNER,
                 "free-agent winner lost winner highlight");
    ok &= Expect(RoundStatsRowColorKind(kNoTeam, false), RoundStatsColorKind::NORMAL,
                 "ordinary free agent did not use normal color");
    return ok ? 0 : 1;
}
