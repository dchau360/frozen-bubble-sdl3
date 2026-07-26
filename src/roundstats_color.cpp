#include "roundstats_color.h"

RoundStatsColorKind RoundStatsRowColorKind(bool teamMode, int team, bool winner) {
    (void)team;
    if (teamMode) return RoundStatsColorKind::TEAM;
    if (winner) return RoundStatsColorKind::WINNER;
    return RoundStatsColorKind::NORMAL;
}
