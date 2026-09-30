#include "worldboard.h"

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
        const size_t slash = item.rfind('/');
        if (eq == std::string::npos || slash == std::string::npos || slash < eq || eq == 0)
            return false;
        WorldBoard::Entry e;
        e.name = item.substr(0, eq);
        e.level = std::atoi(item.substr(eq + 1, slash - eq - 1).c_str());
        e.timeMs = std::atoi(item.substr(slash + 1).c_str());
        if (e.level <= 0) return false;
        out.push_back(e);
    }
    return true;
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
        int v[6];
        if (std::sscanf(meField.c_str(), "%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
            return false;
        b.hasMine = true;
        b.myAlltime = {v[0], v[1], v[2]};
        b.myWeek = {v[3], v[4], v[5]};
    }
    *this = b;
    return true;
}

int WorldBoard::Rank(const std::vector<Entry>& list, size_t i) {
    while (i > 0 && list[i - 1].level == list[i].level && list[i - 1].timeMs == list[i].timeMs) --i;
    return (int)i + 1;
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
