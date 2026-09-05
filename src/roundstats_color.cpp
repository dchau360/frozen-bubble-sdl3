#include "roundstats_color.h"

#include "netteams.h"   // kNoTeam

RoundStatsColorKind RoundStatsRowColorKind(int team, bool winner) {
    if (team != kNoTeam) return RoundStatsColorKind::TEAM;
    if (winner) return RoundStatsColorKind::WINNER;
    return RoundStatsColorKind::NORMAL;
}
