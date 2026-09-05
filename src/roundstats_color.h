#pragma once

enum class RoundStatsColorKind {
    NORMAL,
    WINNER,
    TEAM,
};

// How one player's round-stats row should be coloured.
//
// A player on a real team is coloured by that team, so the table reads the
// same way the boards and the lobby's team chips do. A player on no team
// (kNoTeam) has no team colour to take, so they fall through to the winner
// highlight or plain text -- which is what every row did before teams became
// a per-player setting rather than a whole-game mode.
RoundStatsColorKind RoundStatsRowColorKind(int team, bool winner);
