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

#pragma once

#include <SDL3/SDL.h>
#include <stdio.h>
#include <time.h>

class Logger {
private:
    static FILE* logFile;
    static bool initialized;

    static void LogOutputCallback(void* userdata, int category, SDL_LogPriority priority, const char* message);
    static const char* GetCategoryName(int category);
    static const char* GetPriorityName(SDL_LogPriority priority);

public:
    // Initialize the logger (call this once at startup)
    static bool Initialize(const char* logFilePath = nullptr);

    // Shutdown the logger (call this at program exit)
    static void Shutdown();

    // Check if logger is initialized
    static bool IsInitialized() { return initialized; }
};
