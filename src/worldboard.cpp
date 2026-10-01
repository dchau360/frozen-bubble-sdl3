#include "worldboard.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace {

bool ParseList(const std::string& field, std::vector<WorldBoard::Entry>& out) {
    out.clear();
    if (field == "-") return true;
    std::stringstream ss(field);
    std::string item;
    while (std::getline(ss, item, ',')) {
        const size_t eq = item.rfind('=');
        if (eq == std::string::npos || eq == 0) return false;
        WorldBoard::Entry e;
        e.name = item.substr(0, eq);
        char cc[4] = "";
        const int n = std::sscanf(item.c_str() + eq + 1, "%d/%d/%d/%3s", &e.level, &e.timeMs,
                                  &e.points, cc);
        if (n < 3) return false;
        // Anything but two capitals is dropped rather than refused: a later
        // server's extra field mustn't cost the whole board.
        if (n == 4 && cc[0] >= 'A' && cc[0] <= 'Z' && cc[1] >= 'A' && cc[1] <= 'Z' && !cc[2])
            e.country = cc;
        if (e.level <= 0) return false;
        out.push_back(e);
    }
    return true;
}

// A side list ("N,N,..." or "-") index-aligned with `list`: fills each
// entry's shots. One that doesn't line up is ignored rather than refused,
// like a later server's extra field: the board itself is still good.
void ParseShots(const std::string& field, std::vector<WorldBoard::Entry>& list) {
    if (field == "-") return;
    std::vector<int> v;
    std::stringstream ss(field);
    std::string item;
    while (std::getline(ss, item, ',')) {
        char* end = nullptr;
        const long n = std::strtol(item.c_str(), &end, 10);
        if (item.empty() || !end || *end || n < 0) return;
        v.push_back((int)n);
    }
    if (v.size() != list.size()) return;
    for (size_t i = 0; i < v.size(); ++i) list[i].shots = v[i];
}

}  // namespace

bool WorldBoard::Parse(const std::string& payload) {
    std::stringstream ss(payload);
    std::string ws, alltimeField, weekField, meField;
    if (!(ss >> ws >> alltimeField >> weekField >> meField)) return false;
    char* end = nullptr;
    const long long start = std::strtoll(ws.c_str(), &end, 10);
    if (!end || *end) return false;

    WorldBoard b;
    b.weekStart = start;
    if (!ParseList(alltimeField, b.alltime) || !ParseList(weekField, b.week)) return false;
    if (meField != "-") {
        int v[8];
        if (std::sscanf(meField.c_str(), "%d,%d,%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3],
                        &v[4], &v[5], &v[6], &v[7]) != 8)
            return false;
        b.hasMine = true;
        b.myAlltime = {v[0], v[1], v[2], v[3]};
        b.myWeek = {v[4], v[5], v[6], v[7]};
    }
    // Then the two day lists (the web page's), then the two shot lists.
    std::string allDays, weekDays, allShots, weekShots;
    if (ss >> allDays >> weekDays >> allShots >> weekShots) {
        ParseShots(allShots, b.alltime);
        ParseShots(weekShots, b.week);
    }
    *this = b;
    return true;
}

int WorldBoard::Rank(const std::vector<Entry>& list, size_t i) {
    while (i > 0 && list[i - 1].level == list[i].level && list[i - 1].timeMs == list[i].timeMs &&
           list[i - 1].points == list[i].points)
        --i;
    return (int)i + 1;
}

std::vector<WorldBoard::Entry> WorldBoard::Merge(const std::vector<Entry>& keyboard,
                                                const std::vector<Entry>& mouse, bool points) {
    auto better = [points](const Entry& a, const Entry& b) {
        if (points && a.points != b.points) return a.points > b.points;
        if (a.level != b.level) return a.level > b.level;
        return a.timeMs < b.timeMs;
    };
    std::vector<Entry> out;
    out.reserve(keyboard.size() + mouse.size());
    for (Entry e : keyboard) { e.track = 0; out.push_back(e); }
    for (Entry e : mouse) { e.track = 1; out.push_back(e); }
    std::stable_sort(out.begin(), out.end(), better);
    return out;
}

std::string WorldBoard::PointsLabel(int points) {
    std::string digits = std::to_string(points < 0 ? 0 : points);
    for (int i = (int)digits.size() - 3; i > 0; i -= 3) digits.insert((size_t)i, ",");
    return digits + " pts";
}

std::string WorldBoard::ShotsLabel(int shots) {
    if (shots <= 0) return "";
    return std::to_string(shots) + (shots == 1 ? " shot" : " shots");
}

std::string WorldBoard::LevelLabel(int level) {
    return level > 100 ? "won!" : "level " + std::to_string(level);
}

std::string WorldBoard::TimeLabel(int timeMs) {
    const int total = timeMs / 1000;
    char buf[24];
    if (total >= 3600)
        std::snprintf(buf, sizeof(buf), "%d:%02d'%02d\"", total / 3600, total / 60 % 60, total % 60);
    else
        std::snprintf(buf, sizeof(buf), "%d'%02d\"", total / 60, total % 60);
    return buf;
}
