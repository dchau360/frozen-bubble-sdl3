/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// Opt-in ("Stats:UploadHighscore" setting -- see
// GameSettings::uploadHighscoreStatsEnabled(), off by default) uploader for
// the community leaderboard at petitain.be. Called once from
// BubbleGame::CheckGameState() whenever a classic single-player run ends in
// a loss, and only after the player has explicitly turned the setting on
// through the confirmation popup in mainmenu_panels.cpp's KeysPanelRender.
//
// Wire format -- keep that popup's text in sync with this if it ever
// changes:
//   POST https://petitain.be/scoredb.php
//   Content-Type: application/json
//   {
//     "name": "<nickname, or \"Anonymous\" if none set> (<machine_id>)",
//     "machine_id": "<random id persisted per install>",
//     "score": <int>, "level": <int>, "play_time": <seconds>
//   }
// No other fields, and never the player's OS account name -- see
// GetOrCreateMachineId()'s comment for why an unset nickname falls back to
// "Anonymous" rather than $USER/%USERNAME%.
//
// Desktop native only (Linux/macOS/Windows): shells out to the `curl` binary
// exactly the way DetectGeoLocation()/DiscoverServers() already do in
// networkclient.cpp, rather than linking libcurl. That keeps this feature
// from adding a new build dependency to any platform or test target --
// see CMakeLists.txt/cmake/CoreSources.cmake, where this file is compiled
// only into the main desktop target. Android/iOS/WASM already have their own
// per-platform HTTP path (androidFetchUrl()/IosFetchUrl(), see platform.h),
// but it's GET-only today and none of those three is where this feature was
// asked for, so sendGameStats() simply isn't compiled into those builds.

#include "sendGameStats.h"
#include "gamesettings.h"

#include <cstdio>
#include <fstream>
#include <random>
#include <thread>

namespace {

// Per-install random id, persisted under the same directory GameSettings
// uses for settings.ini (SDL_GetPrefPath -- a real per-user config
// directory), not a relative "machine_id.txt" in the current working
// directory. The relative path silently failed to persist on a read-only
// install (AppImage, /Applications, Program Files -- every write attempt
// fails and a fresh id is "generated" and immediately lost on every single
// launch) and was not per-user on a shared machine either.
std::string GetOrCreateMachineId(const std::string &prefPath) {
    const std::string idFile = prefPath + "machine_id.txt";
    std::string machineId;

    std::ifstream infile(idFile);
    if (infile.is_open()) std::getline(infile, machineId);

    if (machineId.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        machineId = "pc_" + std::to_string(dis(gen));

        std::ofstream outfile(idFile);
        if (outfile.is_open()) outfile << machineId;
    }
    return machineId;
}

// Shells out to curl the same way networkclient.cpp's DetectGeoLocation()/
// DiscoverServers() do. The JSON body travels over curl's stdin (`-d @-`)
// rather than being interpolated into the shell command string: the payload
// carries the player's own nickname, which the player fully controls and can
// fill with shell metacharacters ('; $(...); backticks), and popen() runs
// its argument through /bin/sh. Only the hardcoded URL constant below is
// ever part of the command line itself.
void PostJson(const std::string &url, const std::string &jsonPayload) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST -H 'Content-Type: application/json' "
             "--connect-timeout 2 --max-time 3 -d @- '%s' >/dev/null 2>&1",
             url.c_str());
    FILE *fp = popen(cmd, "w");
    if (!fp) return;
    fwrite(jsonPayload.data(), 1, jsonPayload.size(), fp);
    pclose(fp);
}

} // namespace

void sendGameStats(int score, int level, int playTimeSeconds, const std::string &name) {
    GameSettings *gs = GameSettings::Instance();
    const std::string prefPath = gs->prefPath ? gs->prefPath : "";
    const std::string machineId = GetOrCreateMachineId(prefPath);
    // No OS-username fallback: an empty nickname becomes "Anonymous" rather
    // than sending the player's local account name to a third-party server
    // they never actually typed a name into.
    const std::string displayName = (name.empty() ? std::string("Anonymous") : name) +
                                     " (" + machineId + ")";

    std::string jsonPayload = "{\n";
    jsonPayload += "  \"name\": \"" + displayName + "\",\n";
    jsonPayload += "  \"machine_id\": \"" + machineId + "\",\n";
    jsonPayload += "  \"score\": " + std::to_string(score) + ",\n";
    jsonPayload += "  \"level\": " + std::to_string(level) + ",\n";
    jsonPayload += "  \"play_time\": " + std::to_string(playTimeSeconds) + "\n";
    jsonPayload += "}";

    // Detached, best-effort: the caller (CheckGameState(), once per frame in
    // the worst case, right at the moment of game over) must never block on
    // the network, so the actual POST -- including curl's own timeouts --
    // runs off the game loop entirely. Nothing observes whether it lands.
    std::thread(PostJson, "https://petitain.be/scoredb.php", jsonPayload).detach();
}
