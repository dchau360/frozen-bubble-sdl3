#include "netteams.h"

int AutoBalanceTeam(int slot, int teamCount) {
    if (teamCount < 1) teamCount = 1;
    if (slot < 0) slot = 0;
    return (slot % teamCount) + 1;
}

int EffectiveTeam(int slot, int teamCount, int overrideTeam) {
    if (teamCount < 1) teamCount = 1;
    int team = (overrideTeam > 0) ? overrideTeam : AutoBalanceTeam(slot, teamCount);
    if (team < 1) team = 1;
    if (team > teamCount) team = teamCount;
    return team;
}
