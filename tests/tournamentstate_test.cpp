#include "tournamentstate.h"
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)

int main() {
    TournamentState state;
    CHECK(state.Apply("TOUR_CAPS: 1"));
    CHECK(state.supported);
    CHECK(state.Apply("TOUR_LIST: 1,registration,8,Ada;2,running,16,Bob"));
    CHECK(state.list.size() == 2);
    CHECK(state.Apply("TOUR_STATE: 1 2 running 1 1 0 0 1,Ada,alive,1;2,Bob,alive,1 1,0,1,2,1,0,ready,2,0,1,60"));
    const auto* snap = state.Find(1);
    CHECK(snap && snap->revision == 2 && snap->self == 1);
    CHECK(snap->MyMatch() && snap->MyMatch()->winsA == 1);
    CHECK(snap->Name(2) == "Bob");
    auto actions = TournamentActions(snap, false);
    CHECK(actions.front().command == "READY 1 1 2");
    CHECK(actions.back().command == "BACK");
    CHECK(TournamentReportCommand(state.assignment, "Ada").empty());
    CHECK(state.ActiveId() == 1);
    CHECK(state.Apply("TOUR_STATE: 1 1 registration 1 1 0 0 1,Ada,entered,0 -"));
    CHECK(state.Find(1)->revision == 2);
    CHECK(!state.Apply("TOUR_STATE: 1 3 running 1 1 0 0 1,Ada,alive,1;1,Bob,alive,1 -"));
    CHECK(!state.Apply("TOUR_STATE: 1 3 running 1 1 0 0 1,Ada,alive,1 1,0,1,99,0,0,ready,1,0,0,60"));
    CHECK(!state.Apply("TOUR_STATE: 1 3 running 1 1 0 0 1,Ada,alive,1 1,0,1,1,0,0,ready,1,0,0,60"));
    CHECK(!state.Apply("TOUR_STATE: 1 3 running 1 1 0 0 1,Ada,alive,1;2,Bob,alive,1 1,0,1,2,3,0,ready,1,0,0,60"));
    CHECK(state.Find(1)->revision == 2); // invalid updates are atomic
    CHECK(!state.Apply("TOUR_STATE: 99999999999999999 3 running 1 1 0 0 - -"));
    CHECK(!state.Apply("TOUR_LIST: 1,registration,100000,Ada"));
    CHECK(state.list.size() == 2);
    CHECK(state.Apply("TOUR_ASSIGN: 1 1 2 1 Ada 2 Bob"));
    CHECK(state.assignment.tournament == 1 && state.assignment.round == 2);
    CHECK(TournamentReportCommand(state.assignment, "Ada") == "REPORT 1 1 2 1");
    CHECK(TournamentReportCommand(state.assignment, "Bob") == "REPORT 1 1 2 2");
    CHECK(TournamentReportCommand(state.assignment, "Nobody").empty());
    CHECK(TournamentReportCommand(state.assignment, "") == "REPORT 1 1 2 0");
    CHECK(!state.Apply("TOUR_ASSIGN: 1 1 2 1 Ada 1 Bob"));
    CHECK(state.Apply("TOUR_RETURN: 1 1 1")); // old return cannot retire newer game
    CHECK(!state.assignment.returned);
    CHECK(state.Apply("TOUR_RETURN: 1 1 2"));
    CHECK(state.assignment.returned);
    // Pre-return bracket navigation / duplicate local outcomes: once a round
    // has returned, a late or repeated local outcome must not be able to
    // build another REPORT against it, whoever it names.
    CHECK(TournamentReportCommand(state.assignment, "Ada").empty());
    CHECK(TournamentReportCommand(state.assignment, "Bob").empty());
    CHECK(TournamentReportCommand(state.assignment, "").empty());
    // The next round's TOUR_ASSIGN replaces the assignment wholesale, so a
    // client that backed out to the bracket before TOUR_RETURN arrived (and
    // is therefore still sitting on the just-returned assignment) picks up
    // the new round cleanly rather than staying latched on `returned`.
    CHECK(state.Apply("TOUR_ASSIGN: 1 1 3 1 Ada 2 Bob"));
    CHECK(state.assignment.round == 3 && !state.assignment.returned);
    CHECK(TournamentReportCommand(state.assignment, "Ada") == "REPORT 1 1 3 1");
    state.Reset();
    CHECK(!state.supported && state.list.empty() && state.ActiveId() == 0);
    std::cout << "Tournament state parsing passed\n";
}
