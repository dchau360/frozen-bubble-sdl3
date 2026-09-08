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

#include "mainmenu.h"
#include "audiomixer.h"
#include "frozenbubble.h"
#include "transitionmanager.h"
#include "networkclient.h"
#include "platform.h"

#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <cmath>
#include <errno.h>
#include <thread>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif
#include "socket_compat.h"
#ifndef _WIN32
#ifndef __WASM_PORT__
#include <netdb.h>
#endif
#endif
#ifdef __WASM_PORT__
#include <emscripten.h>
#include <stdlib.h>
#endif

#include "mainmenu_internal.h"

bool portInUse(int port) {
#ifdef _WIN32
    return false; // Server auto-start not supported on Windows
#else
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    connect(s, (struct sockaddr*)&addr, sizeof(addr));
    fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
    struct timeval tv{0, 200000}; // 200ms
    bool inUse = false;
    if (select(s + 1, nullptr, &wfds, nullptr, &tv) > 0) {
        int err = 0;
        socklen_t errLen = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &errLen);
        inUse = (err == 0); // ECONNREFUSED also triggers select; only success has err==0
    }
    SOCKET_CLOSE(s);
    return inUse;
#endif
}


void MainMenu::StartLocalServer() {
#if defined(__ANDROID__) || defined(__WASM_PORT__) || defined(_WIN32) || defined(__IOS_PORT__)
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot host server on this platform");
    connectErrorMsg = "Server hosting not available on this platform";
    return;
#else
    if (serverHosting) {
        SDL_Log("Server already running");
        return;
    }

    // If the port is already in use, refuse rather than clearing it.
    //
    // This used to run `pkill -x fb-server`, which kills *every* fb-server
    // process the user owns — not just an orphan of this client. A developer
    // running a second server for another project, or an intentionally hosted
    // game on a different port, was terminated without warning or confirmation
    // (audit finding BUG-033). The port being busy does not identify whose
    // process holds it, and this code cannot tell an orphan of its own from
    // someone else's live server.
    //
    // Report it instead and let the user decide. The message names the port so
    // they can find the holder themselves (lsof -iTCP:<port>).
    if (portInUse(networkPort)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Port %d is already in use by another process; not starting a local server. "
                     "Stop whatever is holding it, or choose a different port.", networkPort);
        connectErrorMsg = "Port " + std::to_string(networkPort) + " is already in use";
        return;
    }

    SDL_Log("Starting local server on port %d...", networkPort);

    // Fork process to run server
    pid_t pid = fork();

    if (pid < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to fork server process: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        // Child process - run the server
        char portStr[16];
        snprintf(portStr, sizeof(portStr), "%d", networkPort);

        // Try to find server binary
        const char* serverPaths[] = {
            "server/fb-server",           // From build directory
            "./server/fb-server",
            "../server/fb-server",
            "../../server/fb-server",
            "./build/server/fb-server",
            "/usr/local/bin/fb-server",
            NULL
        };

        // Try each path
        for (int i = 0; serverPaths[i] != NULL; i++) {
            // Check if file exists before trying to exec
            if (access(serverPaths[i], X_OK) == 0) {
                execl(serverPaths[i], "fb-server", "-q", "-d", "-z", "-l", "-p", portStr, (char*)NULL);
            }
        }

        // If we get here, exec failed - write to stderr so parent can see
        fprintf(stderr, "ERROR: Failed to start server - binary not found or not executable\n");
        fprintf(stderr, "Searched paths:\n");
        for (int i = 0; serverPaths[i] != NULL; i++) {
            fprintf(stderr, "  %s: %s\n", serverPaths[i], access(serverPaths[i], X_OK) == 0 ? "found" : "not found");
        }
        exit(1);
    }

    // Parent process
    serverPid = pid;
    serverHosting = true;

    // Set host to localhost since we're hosting
    strcpy(networkHost, "127.0.0.1");

    // Poll for the server actually accepting connections, rather than
    // sleeping a fixed second regardless of how quickly (or slowly) it
    // starts -- fork()+exec()+bind()+listen() is normally done in a handful
    // of milliseconds, so this returns almost immediately in the common
    // case instead of stalling the render loop for the old fixed delay.
    // portInUse() is the same connect-probe already used above to detect a
    // busy port; here it doubles as a readiness check. Bounded to 2 seconds
    // (double the old fixed delay) so a genuinely slow start still gets a
    // fair chance rather than being declared dead early.
    SDL_Log("Waiting for server to initialize...");
    bool serverReady = false;
    {
        Uint64 startTime = SDL_GetTicks();
        const Uint64 timeout = 2000;
        while (SDL_GetTicks() - startTime < timeout) {
            int status;
            if (waitpid(serverPid, &status, WNOHANG) != 0) {
                // Child exited immediately - server failed to start
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Server failed to start (child process exited)");
                serverPid = -1;
                serverHosting = false;
                return;
            }
            if (portInUse(networkPort)) {
                serverReady = true;
                break;
            }
            SDL_Delay(20);
        }
    }
    if (!serverReady) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                     "Server did not confirm readiness within 2s; proceeding anyway (PID %d)",
                     serverPid);
    }

    SDL_Log("Server started with PID %d on port %d", serverPid, networkPort);
#endif // !_WIN32 && !__ANDROID__ && !__WASM_PORT__
}


void MainMenu::StopLocalServer() {
#if defined(__ANDROID__) || defined(__WASM_PORT__) || defined(_WIN32) || defined(__IOS_PORT__)
    return;
#else
    if (!serverHosting || serverPid <= 0) {
        return;
    }

    SDL_Log("Stopping local server (PID %d)...", serverPid);

    kill(serverPid, SIGTERM);

    int status;
    waitpid(serverPid, &status, WNOHANG);

    serverPid = -1;
    serverHosting = false;

    SDL_Log("Server stopped");
#endif
}

#ifndef __WASM_PORT__
void MainMenu::StartLanFetch() {
    // One fetch at a time -- a second call while one is already running (e.g.
    // rapid-fire R presses) just no-ops rather than queuing another.
    if (lanFetchInProgress.load()) return;
    lanFetchInProgress = true;
    if (lanFetchThread.joinable()) lanFetchThread.join();
    const int probePort = networkPort > 0 ? networkPort : 1511;
    lanFetchThread = std::thread([this, probePort]() {
        std::vector<ServerInfo> fetched = NetworkClient::DiscoverLANServers();
        bool foundLocal = false;
        for (const auto& s : fetched)
            if (s.host == "127.0.0.1" || s.host == "localhost") { foundLocal = true; break; }
        if (!foundLocal && portInUse(probePort)) {
            ServerInfo localServer;
            localServer.host = "127.0.0.1";
            localServer.port = probePort;
            localServer.name = "Local Server";
            localServer.latencyMs = 0;
            fetched.insert(fetched.begin(), localServer);
        }
        for (auto& s : fetched)
            s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
        std::lock_guard<std::mutex> lock(lanFetchMutex);
        lanFetchResult = std::move(fetched);
        lanFetchInProgress = false;
    });
}
#endif

#ifndef __WASM_PORT__
void MainMenu::StartPublicServerFetch() {
    if (serverFetchInProgress.load()) return;
    serverFetchInProgress = true;
    if (serverFetchThread.joinable()) serverFetchThread.join();
    serverFetchThread = std::thread([this]() {
        std::vector<ServerInfo> fetched = NetworkClient::FetchPublicServers();
        bool foundLocal = false;
        for (const auto& s : fetched)
            if (s.host == "127.0.0.1" || s.host == "localhost") { foundLocal = true; break; }
        if (!foundLocal && portInUse(1511)) {
            ServerInfo localServer;
            localServer.host = "127.0.0.1";
            localServer.port = 1511;
            localServer.name = "Local Server";
            localServer.latencyMs = 0;
            fetched.insert(fetched.begin(), localServer);
        }
        for (auto& s : fetched)
            s.latencyMs = NetworkClient::MeasureLatency(s.host.c_str(), s.port);
        std::lock_guard<std::mutex> lock(serverFetchMutex);
        serverFetchResult = std::move(fetched);
        serverFetchInProgress = false;
    });
}
#endif

void MainMenu::StartGeoLocFetch() {
#ifdef __WASM_PORT__
    // DetectGeoLocation() is a fast no-op on WASM ("zz"); no thread needed.
    geoLocRequested = true;
    geoLocFetchResult = NetworkClient::DetectGeoLocation();
    geoLocFetchDone = true;
#else
    if (geoLocRequested) return;  // once per session -- DetectGeoLocation caches internally too
    geoLocRequested = true;
    geoLocFetchInProgress = true;
    geoLocFetchThread = std::thread([this]() {
        std::string result = NetworkClient::DetectGeoLocation();
        std::lock_guard<std::mutex> lock(geoLocFetchMutex);
        geoLocFetchResult = std::move(result);
        geoLocFetchDone = true;
        geoLocFetchInProgress = false;
    });
#endif
}

void MainMenu::PollGeoLocFetch() {
    if (geoLocFetchDone) {
        std::string result;
        {
            std::lock_guard<std::mutex> lock(geoLocFetchMutex);
            if (geoLocFetchDone) {
                result = geoLocFetchResult;
                geoLocFetchDone = false;  // consumed into geoLocToSend below
            }
        }
        if (!result.empty()) {
            float gLat = 0.0f, gLon = 0.0f;
            if (sscanf(result.c_str(), "%f:%f", &gLat, &gLon) == 2) {
                myGeoLat = gLat;
                myGeoLon = gLon;
                myGeoLocSet = true;
            }
            geoLocToSend = std::move(result);
        }
    }
    if (geoLocToSend.empty()) return;
    NetworkClient* netClient = NetworkClient::Existing();
    // May run several frames before a connection exists (fetch started
    // early) or completes (fetch finished before Connect() did) -- keep the
    // result queued rather than dropping it either way.
    if (netClient && netClient->IsConnected()) {
        netClient->SendGeoLoc(geoLocToSend.c_str());
        geoLocToSend.clear();
    }
}
