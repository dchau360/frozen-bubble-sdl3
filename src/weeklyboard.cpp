#include "weeklyboard.h"

#include <cstdlib>
#include <sstream>

namespace {

bool ParseInt(const std::string& s, int64_t& out) {
    if (s.empty() || s.size() > 18) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    out = std::strtoll(s.c_str(), nullptr, 10);
    return true;
}

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

// "nick=12,nick2=9" or "-" (empty).
bool ParseList(const std::string& field, std::vector<std::pair<std::string, int>>& out) {
    out.clear();
    if (field == "-") return true;
    for (const std::string& item : Split(field, ',')) {
        const size_t eq = item.find('=');
        int64_t n = 0;
        if (eq == std::string::npos || eq == 0 || !ParseInt(item.substr(eq + 1), n)) return false;
        out.emplace_back(item.substr(0, eq), static_cast<int>(n));
    }
    return true;
}

}  // namespace

bool WeeklyBoard::Parse(const std::string& payload) {
    std::istringstream in(payload);
    std::string week, w, l, p, me, lobby = "-";
    if (!(in >> week >> w >> l >> p >> me)) return false;
    in >> lobby;  // optional sixth field; stays "-" if a server omits it

    WeeklyBoard b;
    if (!ParseInt(week, b.weekStart)) return false;
    if (!ParseList(w, b.wins) || !ParseList(l, b.losses) || !ParseList(p, b.popped)) return false;
    if (me != "-") {
        const auto parts = Split(me, ',');
        int64_t v[6];
        if (parts.size() != 6) return false;
        for (int i = 0; i < 6; ++i)
            if (!ParseInt(parts[i], v[i])) return false;
        b.hasMine = true;
        b.myWins = static_cast<int>(v[0]);
        b.myLosses = static_cast<int>(v[1]);
        b.myPopped = static_cast<int>(v[2]);
        b.myWinsRank = static_cast<int>(v[3]);
        b.myLossesRank = static_cast<int>(v[4]);
        b.myPoppedRank = static_cast<int>(v[5]);
    }
    std::vector<std::pair<std::string, int>> ranks;
    if (!ParseList(lobby, ranks)) return false;
    for (const auto& r : ranks) b.lobbyWinsRank[r.first] = r.second;
    *this = b;
    return true;
}

int WeeklyBoard::Rank(const std::vector<std::pair<std::string, int>>& list, size_t i) {
    // The list is sorted highest-first, so everyone with a strictly higher
    // count comes before i; stop at the first entry that ties it.
    size_t first = i;
    while (first > 0 && list[first - 1].second == list[i].second) --first;
    return static_cast<int>(first) + 1;
}
