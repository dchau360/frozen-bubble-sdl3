#ifndef TOURNAMENTSTATE_H
#define TOURNAMENTSTATE_H

#include <map>
#include <string>
#include <vector>

struct TournamentEntrant {
    int id = 0;
    std::string nick, state;
    bool ready = false;
};
struct TournamentMatch {
    int id = 0, stage = 0, a = 0, b = 0, winsA = 0, winsB = 0;
    std::string state;
    int round = 0;
    bool readyA = false, readyB = false;
    int remaining = 0;
    bool Terminal() const;
};
struct TournamentSnapshot {
    int id = 0, revision = 0, owner = 0, self = 0, stage = 0, champion = 0;
    std::string state;
    std::vector<TournamentEntrant> entrants;
    std::vector<TournamentMatch> matches;
    const TournamentEntrant* Entrant(int id) const;
    const TournamentMatch* MyMatch() const;
    std::string Name(int id) const;
};
struct TournamentListing {
    int id = 0, count = 0;
    std::string state, owner;
};
struct TournamentAssignment {
    int tournament = 0, match = 0, round = 0, a = 0, b = 0;
    std::string nickA, nickB;
    bool returned = false;
};
struct TournamentAction { std::string label, command; };
std::vector<TournamentAction> TournamentActions(const TournamentSnapshot* snapshot, bool enteredElsewhere);
std::string TournamentReportCommand(const TournamentAssignment& assignment,
                                    const std::string& winnerNick);

// Transport-independent, bounded snapshot parser shared by native and WASM.
class TournamentState {
public:
    bool Apply(const std::string& push);
    void Reset();
    const TournamentSnapshot* Find(int id) const;
    int ActiveId() const;
    bool supported = false;
    std::vector<TournamentListing> list;
    std::map<int, TournamentSnapshot> snapshots;
    TournamentAssignment assignment;
};

#endif
