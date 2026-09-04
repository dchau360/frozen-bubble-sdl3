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
// Every platform posts through its own transport, the same split the rest
// of the codebase already uses for "make an HTTP request" (see
// networkclient.cpp's curlFetch()/DetectGeoLocation() and platform_ios.mm):
//   - Desktop (Linux/macOS/Windows): shells out to the `curl` binary via
//     popen(), same as curlFetch()/DetectGeoLocation() already do, run on a
//     detached std::thread so the blocking popen() never stalls the game
//     loop that calls this.
//   - Android: JNI call to FrozenBubbleActivity.postJson(), the POST
//     counterpart of the existing fetchUrl() -- also detached, since that
//     call blocks the calling thread the same way fetchUrl() does.
//   - iOS: NSURLSession POST (IosPostJson(), platform_ios.mm) -- already
//     asynchronous on the session's own queue, so no extra thread needed.
//   - WASM: browser fetch() via EM_JS -- already asynchronous in the JS
//     event loop, so no extra thread needed (and none is available: this
//     build has no pthread support).
// All four are fire-and-forget: none of them reports success or failure
// back to the caller.

#include "sendGameStats.h"
#include "gamesettings.h"

#include <fstream>
#include <random>

#if defined(__ANDROID__)
#include <SDL3/SDL.h>
#include <jni.h>
#include <thread>
#elif defined(__WASM_PORT__)
#include <emscripten.h>
#elif defined(__IOS_PORT__)
#include "platform.h"
#else
#include <cstdio>
#include <thread>
#endif

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

#if defined(__ANDROID__)
// Mirrors networkclient.cpp's androidFetchUrl() exactly (same JNI-context
// lookup, same reason: FindClass()-by-name resolves against the wrong
// classloader from a thread the JVM did not create, which this detached
// std::thread is; going through the already-valid Activity jobject instead
// does not have that problem). Blocking -- must not run on the caller's own
// thread, see sendGameStats() below.
void AndroidPostJson(const std::string &url, const std::string &json) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return;

    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetStaticMethodID(cls, "postJson",
                                            "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!mid) {
        SDL_Log("AndroidPostJson: postJson method not found");
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return;
    }

    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jjson = env->NewStringUTF(json.c_str());
    env->CallStaticVoidMethod(cls, mid, jurl, jjson);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(jjson);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}
#elif defined(__WASM_PORT__)
// fetch() is already asynchronous in the browser's own event loop -- this
// returns to C++ immediately, no thread of any kind involved or needed.
EM_JS(void, WasmPostJson, (const char *url, const char *json), {
    try {
        fetch(UTF8ToString(url), {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: UTF8ToString(json),
        }).catch(function(e) {});
    } catch (e) {}
});
#elif !defined(__IOS_PORT__)
// Shells out to curl the same way networkclient.cpp's DetectGeoLocation()/
// DiscoverServers() do. The JSON body travels over curl's stdin (`-d @-`)
// rather than being interpolated into the shell command string: the payload
// carries the player's own nickname, which the player fully controls and can
// fill with shell metacharacters ('; $(...); backticks), and popen() runs
// its argument through /bin/sh. Only the hardcoded URL constant below is
// ever part of the command line itself.
void DesktopPostJson(const std::string &url, const std::string &jsonPayload) {
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
#endif

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

    const std::string url = "https://petitain.be/scoredb.php";

    // Best-effort, fire-and-forget: the caller (CheckGameState(), once per
    // frame in the worst case, right at the moment of game over) must never
    // block on the network. See this file's own header comment for why each
    // platform branch below either detaches its own thread or needs none.
#if defined(__ANDROID__)
    std::thread([url, jsonPayload]() { AndroidPostJson(url, jsonPayload); }).detach();
#elif defined(__WASM_PORT__)
    WasmPostJson(url.c_str(), jsonPayload.c_str());
#elif defined(__IOS_PORT__)
    IosPostJson(url, jsonPayload);
#else
    std::thread(DesktopPostJson, url, jsonPayload).detach();
#endif
}
