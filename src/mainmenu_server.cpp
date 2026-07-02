/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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
#if defined(__ANDROID__) || defined(__WASM_PORT__) || defined(_WIN32)
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot host server on this platform");
    connectErrorMsg = "Server hosting not available on this platform";
    return;
#else
    if (serverHosting) {
        SDL_Log("Server already running");
        return;
    }

    // If port is already in use by a leftover process, kill it first
    if (portInUse(networkPort)) {
        SDL_Log("Port %d already in use — killing orphaned fb-server...", networkPort);
        system("pkill -x fb-server 2>/dev/null");
        SDL_Delay(300);
        if (portInUse(networkPort)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Port %d still in use after kill attempt", networkPort);
            connectErrorMsg = "Port " + std::to_string(networkPort) + " is already in use";
            return;
        }
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

    // Give server time to start
    SDL_Log("Waiting for server to initialize...");
    SDL_Delay(1000);

    // Check if child process is still running
    int status;
    pid_t result = waitpid(serverPid, &status, WNOHANG);
    if (result != 0) {
        // Child exited immediately - server failed to start
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Server failed to start (child process exited)");
        serverPid = -1;
        serverHosting = false;
        return;
    }

    SDL_Log("Server started with PID %d on port %d", serverPid, networkPort);
#endif // !_WIN32 && !__ANDROID__ && !__WASM_PORT__
}


void MainMenu::StopLocalServer() {
#if defined(__ANDROID__) || defined(__WASM_PORT__) || defined(_WIN32)
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
