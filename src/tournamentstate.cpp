#include "tournamentstate.h"
#include <algorithm>
#include <charconv>
#include <set>
#include <sstream>

namespace {
std::vector<std::string> Split(const std::string& text, char delimiter) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const auto end = text.find(delimiter, start);
        out.push_back(text.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) return out;
        start = end + 1;
    }
}
bool Number(const std::string& s, int& value, int max = 2147483647) {
    if (s.empty()) return false;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc() && result.ptr == s.data() + s.size() && value >= 0 && value <= max;
}
bool Nick(const std::string& s) {
    return !s.empty() && s.size() <= 10 &&
        std::all_of(s.begin(), s.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        });
}
bool OneOf(const std::string& s, std::initializer_list<const char*> values) {
    return std::any_of(values.begin(), values.end(), [&](const char* v) { return s == v; });
}
bool TournamentStatus(const std::string& s) {
    return OneOf(s, {"registration", "running", "complete", "cancelled"});
}
bool Snapshot(const std::vector<std::string>& words, TournamentSnapshot& s) {
    if (words.size() != 9 || !Number(words[0], s.id) || !s.id ||
        !Number(words[1], s.revision) || !s.revision || !TournamentStatus(words[2]) ||
        !Number(words[3], s.owner) || !Number(words[4], s.self) ||
        !Number(words[5], s.stage, 4) || !Number(words[6], s.champion)) return false;
    s.state = words[2];
    std::set<int> ids;
    std::set<std::string> names;
    if (words[7] != "-") {
        const auto entries = Split(words[7], ';');
        if (entries.size() > 16) return false;
        for (const auto& entry : entries) {
            const auto f = Split(entry, ',');
            TournamentEntrant e;
            int ready;
            if (f.size() != 4 || !Number(f[0], e.id) || !e.id || !Nick(f[1]) ||
                !OneOf(f[2], {"entered", "alive", "eliminated", "withdrawn", "champion"}) ||
                !Number(f[3], ready, 1) || !ids.insert(e.id).second || !names.insert(f[1]).second) return false;
            e.nick = f[1]; e.state = f[2]; e.ready = ready != 0;
            s.entrants.push_back(e);
        }
    }
    for (int id : {s.self, s.owner, s.champion}) if (id && !ids.count(id)) return false;
    std::set<int> matchIds;
    if (words[8] != "-") {
        const auto entries = Split(words[8], ';');
        if (entries.size() > 15) return false;
        for (const auto& entry : entries) {
            const auto f = Split(entry, ',');
            TournamentMatch m;
            int ra, rb;
            if (f.size() != 11 || !Number(f[0], m.id, 15) || !m.id ||
                !Number(f[1], m.stage, 3) || !Number(f[2], m.a) || !Number(f[3], m.b) ||
                !Number(f[4], m.winsA, 2) || !Number(f[5], m.winsB, 2) ||
                !OneOf(f[6], {"waiting", "ready", "countdown", "playing", "reporting", "disputed", "complete", "bye", "forfeit"}) ||
                !Number(f[7], m.round) || !Number(f[8], ra, 1) || !Number(f[9], rb, 1) ||
                !Number(f[10], m.remaining, 3600) || !matchIds.insert(m.id).second ||
                (m.a && !ids.count(m.a)) || (m.b && !ids.count(m.b)) ||
                (m.a && m.a == m.b)) return false;
            m.state = f[6]; m.readyA = ra != 0; m.readyB = rb != 0;
            s.matches.push_back(m);
        }
    }
    return true;
}
}

bool TournamentMatch::Terminal() const {
    return state == "complete" || state == "bye" || state == "forfeit";
}
const TournamentEntrant* TournamentSnapshot::Entrant(int pid) const {
    for (const auto& e : entrants) if (e.id == pid) return &e;
    return nullptr;
}
const TournamentMatch* TournamentSnapshot::MyMatch() const {
    if (!self) return nullptr;
    for (const auto& m : matches)
        if ((m.a == self || m.b == self) && !m.Terminal()) return &m;
    return nullptr;
}
std::string TournamentSnapshot::Name(int pid) const {
    const auto* e = Entrant(pid);
    return e ? e->nick : "—";
}
const TournamentSnapshot* TournamentState::Find(int id) const {
    const auto it = snapshots.find(id);
    return it == snapshots.end() ? nullptr : &it->second;
}
int TournamentState::ActiveId() const {
    for (const auto& entry : snapshots) {
        const auto& s = entry.second;
        const auto* me = s.Entrant(s.self);
        if (me && (me->state == "entered" || me->state == "alive") &&
            (s.state == "registration" || s.state == "running")) return s.id;
    }
    return 0;
}
void TournamentState::Reset() { *this = TournamentState{}; }

std::vector<TournamentAction> TournamentActions(const TournamentSnapshot* s, bool enteredElsewhere) {
    std::vector<TournamentAction> actions;
    if (!s) return actions;
    const std::string id = std::to_string(s->id);
    const auto* me = s->Entrant(s->self);
    if (s->state == "registration") {
        if (!me && !enteredElsewhere && s->entrants.size() < 16) actions.push_back({"Join tournament", "JOIN " + id});
        if (me && !me->ready) actions.push_back({"Ready", "READY " + id + " 0 0"});
        if (me && s->self == s->owner) actions.push_back({"Start tournament", "START " + id});
    }
    if (s->state == "running" && me && me->state == "alive") {
        const auto* m = s->MyMatch();
        if (m && m->state == "ready" && !(m->a == s->self ? m->readyA : m->readyB))
            actions.push_back({"Ready", "READY " + id + " " + std::to_string(m->id) + " " + std::to_string(m->round)});
    }
    if (s->matches.size() > 7) actions.push_back({"Next section", "SECTION"});
    if (me && (me->state == "entered" || me->state == "alive") && (s->state == "registration" || s->state == "running"))
        actions.push_back({"Withdraw", "WITHDRAW " + id});
    actions.push_back({"Back", "BACK"});
    return actions;
}

std::string TournamentReportCommand(const TournamentAssignment& a,
                                    const std::string& winnerNick) {
    if (!a.tournament || !a.match || !a.round || a.returned) return {};
    int winner = 0;
    if (winnerNick == a.nickA) winner = a.a;
    else if (winnerNick == a.nickB) winner = a.b;
    else if (!winnerNick.empty()) return {};
    return "REPORT " + std::to_string(a.tournament) + " " +
           std::to_string(a.match) + " " + std::to_string(a.round) + " " +
           std::to_string(winner);
}

bool TournamentState::Apply(const std::string& push) {
    if (push.size() > 8192) return false;
    const auto colon = push.find(": ");
    if (colon == std::string::npos) return false;
    const auto kind = push.substr(0, colon), body = push.substr(colon + 2);
    if (kind == "TOUR_CAPS") { supported = body == "1"; return supported; }
    if (kind == "TOUR_LIST") {
        std::vector<TournamentListing> updated;
        std::set<int> ids;
        if (body != "-") {
            const auto entries = Split(body, ';');
            if (entries.size() > 8) return false;
            for (const auto& entry : entries) {
                const auto f = Split(entry, ',');
                TournamentListing item;
                if (f.size() != 4 || !Number(f[0], item.id) || !item.id ||
                    !TournamentStatus(f[1]) || !Number(f[2], item.count, 16) ||
                    (f[3] != "-" && !Nick(f[3])) || !ids.insert(item.id).second) return false;
                item.state = f[1]; item.owner = f[3]; updated.push_back(item);
            }
        }
        list = std::move(updated);
        return true;
    }
    std::istringstream stream(body);
    std::vector<std::string> words;
    std::string word;
    while (stream >> word) words.push_back(word);
    if (kind == "TOUR_STATE") {
        TournamentSnapshot s;
        if (!Snapshot(words, s)) return false;
        const auto old = snapshots.find(s.id);
        if (old != snapshots.end() && old->second.revision >= s.revision) return true;
        if (old == snapshots.end() && snapshots.size() >= 8) return false;
        snapshots[s.id] = std::move(s);
        return true;
    }
    if (kind == "TOUR_ASSIGN") {
        TournamentAssignment a;
        if (words.size() != 7 || !Number(words[0], a.tournament) || !a.tournament ||
            !Number(words[1], a.match, 15) || !a.match || !Number(words[2], a.round) || !a.round ||
            !Number(words[3], a.a) || !a.a || !Nick(words[4]) ||
            !Number(words[5], a.b) || !a.b || !Nick(words[6]) || a.a == a.b || words[4] == words[6]) return false;
        a.nickA = words[4]; a.nickB = words[6];
        assignment = a;
        return true;
    }
    if (kind == "TOUR_RETURN") {
        int tid, mid, round;
        if (words.size() != 3 || !Number(words[0], tid) || !Number(words[1], mid) || !Number(words[2], round)) return false;
        if (assignment.tournament == tid && assignment.match == mid && assignment.round == round)
            assignment.returned = true;
        return true;
    }
    return false;
}
