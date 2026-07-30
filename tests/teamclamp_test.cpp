// Team numbers arrive from an untrusted peer's OPTIONS message (PLAYERTEAM_Pn)
// and are used one-based to index the 5-element kTeamColors array in
// src/bubblegame.h; before the clamp, PLAYERTEAM_P1=0 read kTeamColors[-1] and
// large values read past the end.

#undef NDEBUG
#include "networkclient.h"
#include <cassert>
#include <climits>
#include <cstdio>

static_assert(kMaxTeams == 5,
    "kMaxTeams must be 5 to match kTeamColors");
static_assert(ClampTeamNumber(0) == 1,
    "team 0 must fold to 1 to avoid kTeamColors[-1]");
static_assert(ClampTeamNumber(1) == 1,
    "team 1 must pass through unchanged");
static_assert(ClampTeamNumber(5) == 5,
    "team 5 must pass through unchanged");
static_assert(ClampTeamNumber(6) == 5,
    "team 6 must fold to kMaxTeams to avoid out-of-bounds");

int main() {
    // passthrough
    assert(ClampTeamNumber(1) == 1);
    assert(ClampTeamNumber(2) == 2);
    assert(ClampTeamNumber(3) == 3);
    assert(ClampTeamNumber(4) == 4);
    assert(ClampTeamNumber(5) == 5);

    // underflow folds to 1
    assert(ClampTeamNumber(0) == 1);
    assert(ClampTeamNumber(-1) == 1);
    assert(ClampTeamNumber(INT_MIN) == 1);

    // overflow folds to kMaxTeams
    assert(ClampTeamNumber(6) == 5);
    assert(ClampTeamNumber(999) == 5);
    assert(ClampTeamNumber(INT_MAX) == 5);

    // This is the actual safety property: result is always a legal one-based
    // index into the 5-element kTeamColors array.
    for (int i = -1000; i <= 1000; ++i) {
        int r = ClampTeamNumber(i);
        assert(r >= 1);
        assert(r <= kMaxTeams);
    }
    assert(ClampTeamNumber(INT_MIN) >= 1);
    assert(ClampTeamNumber(INT_MIN) <= kMaxTeams);
    assert(ClampTeamNumber(INT_MAX) >= 1);
    assert(ClampTeamNumber(INT_MAX) <= kMaxTeams);

    std::printf("teamclamp tests passed\n");
    return 0;
}
