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
        if (eq == std::string::npos || eq == 0) return false;
        WorldBoard::Entry e;
        e.name = item.substr(0, eq);
        if (std::sscanf(item.c_str() + eq + 1, "%d/%d/%d", &e.level, &e.timeMs, &e.points) != 3)
            return false;
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
        int v[8];
        if (std::sscanf(meField.c_str(), "%d,%d,%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3],
                        &v[4], &v[5], &v[6], &v[7]) != 8)
            return false;
        b.hasMine = true;
        b.myAlltime = {v[0], v[1], v[2], v[3]};
        b.myWeek = {v[4], v[5], v[6], v[7]};
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

std::string WorldBoard::PointsLabel(int points) {
    std::string digits = std::to_string(points < 0 ? 0 : points);
    for (int i = (int)digits.size() - 3; i > 0; i -= 3) digits.insert((size_t)i, ",");
    return digits + " pts";
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
