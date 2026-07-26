#undef NDEBUG
#include "netteams.h"
#include <cassert>
#include <cstdio>

int main() {
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

    // No override (0) -> auto-balance default.
    assert(EffectiveTeam(1, 4, 0) == 2);
    assert(EffectiveTeam(5, 3, 0) == AutoBalanceTeam(5, 3));

    // Override (>0) wins over the default...
    assert(EffectiveTeam(0, 4, 3) == 3);
    // ...but is clamped into [1, teamCount] so a stale high override can't
    // strand a player on a nonexistent team.
    assert(EffectiveTeam(0, 3, 5) == 3);   // 5 clamped down to 3
    assert(EffectiveTeam(0, 4, 99) == 4);
    // A negative/zero override means "no override" -> default.
    assert(EffectiveTeam(2, 4, 0) == 3);

    std::printf("netteams tests passed\n");
    return 0;
}
