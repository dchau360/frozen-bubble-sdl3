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

#include "logger.h"
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#endif

FILE* Logger::logFile = nullptr;
bool Logger::initialized = false;

const char* Logger::GetCategoryName(int category) {
    switch (category) {
        case SDL_LOG_CATEGORY_APPLICATION: return "APP";
        case SDL_LOG_CATEGORY_ERROR: return "ERROR";
        case SDL_LOG_CATEGORY_ASSERT: return "ASSERT";
        case SDL_LOG_CATEGORY_SYSTEM: return "SYSTEM";
        case SDL_LOG_CATEGORY_AUDIO: return "AUDIO";
        case SDL_LOG_CATEGORY_VIDEO: return "VIDEO";
        case SDL_LOG_CATEGORY_RENDER: return "RENDER";
        case SDL_LOG_CATEGORY_INPUT: return "INPUT";
        case SDL_LOG_CATEGORY_TEST: return "TEST";
        default: return "CUSTOM";
    }
}

const char* Logger::GetPriorityName(SDL_LogPriority priority) {
    switch (priority) {
        case SDL_LOG_PRIORITY_VERBOSE: return "VERBOSE";
        case SDL_LOG_PRIORITY_DEBUG: return "DEBUG";
        case SDL_LOG_PRIORITY_INFO: return "INFO";
        case SDL_LOG_PRIORITY_WARN: return "WARN";
        case SDL_LOG_PRIORITY_ERROR: return "ERROR";
        case SDL_LOG_PRIORITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

void Logger::LogOutputCallback(void* /* userdata */, int category, SDL_LogPriority priority, const char* message) {
    if (!logFile) return;

    // Get current timestamp
    time_t rawtime;
    struct tm* timeinfo;
    char timestamp[64];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Write to file with timestamp, category, priority, and message
    fprintf(logFile, "[%s] [%s] [%s] %s\n",
            timestamp,
            GetCategoryName(category),
            GetPriorityName(priority),
            message);

    // Flush immediately to ensure logs are written even if program crashes
    fflush(logFile);

    // Also output to console (stderr for SDL_Log)
    fprintf(stderr, "[%s] [%s] %s\n", GetCategoryName(category), GetPriorityName(priority), message);
}

bool Logger::Initialize(const char* logFilePath) {
    if (initialized) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Logger already initialized");
        return true;
    }

    // Default log file path if none provided
    const char* defaultPath = "frozen-bubble.log";
    const char* path = logFilePath ? logFilePath : defaultPath;

    // Open log file in append mode
    logFile = fopen(path, "a");
    if (!logFile) {
        fprintf(stderr, "Failed to open log file: %s\n", path);
        return false;
    }

    // Write session header
    time_t rawtime;
    struct tm* timeinfo;
    char timestamp[64];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    fprintf(logFile, "\n");
    fprintf(logFile, "========================================\n");
    fprintf(logFile, "Frozen Bubble Session Started: %s\n", timestamp);
    fprintf(logFile, "========================================\n");
    fflush(logFile);

    // Set SDL log output function to use our callback
    SDL_SetLogOutputFunction(LogOutputCallback, nullptr);

    // Set log priority to show all messages
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    initialized = true;
    SDL_Log("Logger initialized successfully. Logging to: %s", path);

    return true;
}

void Logger::Shutdown() {
    if (!initialized) return;

    SDL_Log("Logger shutting down");

    if (logFile) {
        // Write session footer
        time_t rawtime;
        struct tm* timeinfo;
        char timestamp[64];

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

        fprintf(logFile, "========================================\n");
        fprintf(logFile, "Frozen Bubble Session Ended: %s\n", timestamp);
        fprintf(logFile, "========================================\n\n");
        fflush(logFile);

        fclose(logFile);
        logFile = nullptr;
    }

    // Reset SDL log output to default
    SDL_SetLogOutputFunction(nullptr, nullptr);

    initialized = false;
}
