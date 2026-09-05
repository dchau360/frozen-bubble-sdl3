#include "netteams.h"

#include <set>

bool AreTeammates(int teamA, int teamB) {
    if (teamA == kNoTeam || teamB == kNoTeam) return false;
    return teamA == teamB;
}

int CountFactions(const int* teams, int count) {
    if (!teams || count <= 0) return 0;
    std::set<int> realTeams;
    int loners = 0;
    for (int i = 0; i < count; i++) {
        if (teams[i] == kNoTeam) ++loners;   // each its own side
        else realTeams.insert(teams[i]);
    }
    return (int)realTeams.size() + loners;
}

int AutoBalanceTeam(int slot, int teamCount) {
    if (teamCount < 1) teamCount = 1;
    if (slot < 0) slot = 0;
    return (slot % teamCount) + 1;
}

int StepTeamChoice(int currentTeam, int teamCount, int direction) {
    if (teamCount < 1) teamCount = 1;
    if (currentTeam < kNoTeam || currentTeam > teamCount)
        currentTeam = kNoTeam;
    if (direction < 0)
        return currentTeam == kNoTeam ? teamCount : currentTeam - 1;
    return currentTeam == teamCount ? kNoTeam : currentTeam + 1;
}
