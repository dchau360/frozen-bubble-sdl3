// src/netteams.h
#ifndef NETTEAMS_H
#define NETTEAMS_H

// Pure team-assignment logic for >5-player Team Mode (no SDL) -- unit-tested
// by tests/netteams_test.cpp.

// Round-robin auto-balance: slot i (0-based, in LIST player order) maps to
// team (i % teamCount) + 1. teamCount is assumed clamped to [1,5] by the
// caller; if <= 0 it is treated as 1.
int AutoBalanceTeam(int slot, int teamCount);

// Effective team for a slot: the override if one is set (overrideTeam > 0),
// otherwise the auto-balance default. The result is clamped into
// [1, teamCount] so a stale override (e.g. team 5 after the host lowered the
// count to 3) can't strand a player on a team that no longer exists.
int EffectiveTeam(int slot, int teamCount, int overrideTeam);

#endif // NETTEAMS_H
