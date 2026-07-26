#pragma once

enum class RoundStatsColorKind {
    NORMAL,
    WINNER,
    TEAM,
};

RoundStatsColorKind RoundStatsRowColorKind(bool teamMode, int team, bool winner);
