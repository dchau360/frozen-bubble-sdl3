#include <SDL3/SDL.h>

#include "logger.h"

#include <cstdio>

int main() {
    const char* path = "/tmp/frozen-bubble-logger-priority-test.log";
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "FROZEN_BUBBLE_DEBUG", "0", true);
    std::remove(path);
    if (!Logger::Initialize(path)) return 1;

    const SDL_LogPriority priority = SDL_GetLogPriority(SDL_LOG_CATEGORY_APPLICATION);
    Logger::Shutdown();
    std::remove(path);

    if (priority != SDL_LOG_PRIORITY_INFO) {
        std::fprintf(stderr, "expected INFO log priority, got %d\n", (int)priority);
        return 1;
    }

    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "FROZEN_BUBBLE_DEBUG", "1", true);
    if (!Logger::Initialize(path)) return 1;
    const SDL_LogPriority debugPriority = SDL_GetLogPriority(SDL_LOG_CATEGORY_APPLICATION);
    Logger::Shutdown();
    std::remove(path);
    if (debugPriority != SDL_LOG_PRIORITY_DEBUG) {
        std::fprintf(stderr, "expected DEBUG log priority when enabled, got %d\n", (int)debugPriority);
        return 1;
    }
    return 0;
}
