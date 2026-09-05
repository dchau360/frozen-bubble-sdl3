#undef NDEBUG
#include "netteams.h"
#include <cassert>
#include <cstdio>

int main() {
    // Free agents are opponents even though both use the same sentinel. A
    // missing non-zero guard in AreTeammates would merge every unaffiliated
    // player into one accidental team.
    assert(!AreTeammates(kNoTeam, kNoTeam));
    assert(!AreTeammates(kNoTeam, 1));
    assert(!AreTeammates(1, kNoTeam));
    assert(AreTeammates(3, 3));
    assert(!AreTeammates(3, 4));

    // Each free agent is a faction of their own, while repeated real team
    // numbers collapse to one faction.
    const int freeForAll[] = {kNoTeam, kNoTeam, kNoTeam, kNoTeam};
    assert(CountFactions(freeForAll, 4) == 4);
    const int mixed[] = {1, 1, 2, kNoTeam, kNoTeam};
    assert(CountFactions(mixed, 5) == 4);
    assert(CountFactions(nullptr, 5) == 0);
    assert(CountFactions(mixed, 0) == 0);

    // Auto-balance round-robins across teams by slot.
    assert(AutoBalanceTeam(0, 4) == 1);
    assert(AutoBalanceTeam(1, 4) == 2);
    assert(AutoBalanceTeam(3, 4) == 4);
    assert(AutoBalanceTeam(4, 4) == 1);   // wraps
    assert(AutoBalanceTeam(19, 4) == 4);  // 19 % 4 = 3 -> team 4
    assert(AutoBalanceTeam(0, 2) == 1);
    assert(AutoBalanceTeam(1, 2) == 2);
    assert(AutoBalanceTeam(2, 2) == 1);
    // Degenerate teamCount treated as 1.
    assert(AutoBalanceTeam(5, 0) == 1);
    assert(AutoBalanceTeam(5, 1) == 1);

    // Every team-changing input cycles across all actual choices. In
    // particular, either direction can restore the default no-team state.
    assert(StepTeamChoice(kNoTeam, 5, 1) == 1);
    assert(StepTeamChoice(1, 5, 1) == 2);
    assert(StepTeamChoice(5, 5, 1) == kNoTeam);
    assert(StepTeamChoice(kNoTeam, 5, -1) == 5);
    assert(StepTeamChoice(1, 5, -1) == kNoTeam);
    assert(StepTeamChoice(4, 4, 1) == kNoTeam);

    std::printf("netteams tests passed\n");
    return 0;
}
