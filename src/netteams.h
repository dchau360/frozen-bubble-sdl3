// src/netteams.h
#ifndef NETTEAMS_H
#define NETTEAMS_H

// Pure team-assignment logic (no SDL), unit-tested by tests/netteams_test.cpp.

// Round-robin auto-balance: slot i (0-based, in LIST player order) maps to
// team (i % teamCount) + 1. teamCount is assumed clamped to [1,5] by the
// caller; if <= 0 it is treated as 1.
int AutoBalanceTeam(int slot, int teamCount);

// Move one choice left or right through no-team and teams 1..teamCount.
// `direction` is interpreted by sign so keyboard, gamepad, and name taps can
// share the same wraparound rule.
int StepTeamChoice(int currentTeam, int teamCount, int direction);

// A player on no team at all: their own faction, one of possibly many.
//
// Teams used to exist only inside a dedicated "Teams" game mode, so every
// player in a team game was on one of teams 1..5 and a team number was never
// absent. Teams are a per-player setting in every mode now, which means "not
// on a team" has to be representable -- and it is not the same as "on a team
// nobody else is on", because two unaffiliated players are opponents, not a
// pair of one-player teams that happen to look alike.
inline constexpr int kNoTeam = 0;

// True when two team numbers make their holders teammates: the same *real*
// team. Two unaffiliated players are never teammates -- each is their own
// faction, so they attack each other and win separately.
//
// This is the single rule the whole game asks. It is one function because
// spelling it inline was what let the old code read "same team number" as
// "allies": with kNoTeam a shared value, every free agent in the room would
// stop being able to attack every other one, and they would all win together.
bool AreTeammates(int teamA, int teamB);

// How many distinct sides are still standing among `teams`.
//
// Each real team counts once however many players hold it; each unaffiliated
// player counts as a side of their own. A round ends when this reaches 1.
//
// With everyone on kNoTeam this is exactly the player count, which is what
// makes team logic safe to run unconditionally: a game where nobody picked a
// team behaves precisely as a free-for-all did before teams existed.
int CountFactions(const int* teams, int count);

#endif // NETTEAMS_H
