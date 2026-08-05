#include "gamesettings.h"
#include "highscoremanager.h"
#include "platform.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

static bool fileContains(const std::filesystem::path& path,
                         const std::string& expected) {
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return input.good() && contents.str().find(expected) != std::string::npos;
}

// iniparser_dump_ini lowercases keys, pads them to a fixed column, and quotes
// values, so the persisted line for GFX:ShowFPS reads:
//     showfps                        = "true"
static bool iniHasKeyValue(const std::filesystem::path& path,
                           const std::string& key, const std::string& value) {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;

        std::string name = line.substr(0, separator);
        name.erase(name.find_last_not_of(" \t") + 1);
        if (name != key) continue;

        std::string stored = line.substr(separator + 1);
        const std::size_t first = stored.find_first_not_of(" \t\"");
        const std::size_t last = stored.find_last_not_of(" \t\"\r");
        if (first == std::string::npos) return value.empty();
        return stored.substr(first, last - first + 1) == value;
    }
    return false;
}

static bool csvHasLevelAndTime(const std::filesystem::path& path,
                               int expectedLevel, float expectedTime) {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        std::stringstream row(line);
        std::string level;
        std::string name;
        std::string time;
        if (std::getline(row, level, ',') &&
            std::getline(row, name, ',') &&
            std::getline(row, time, ',')) {
            try {
                if (std::stoi(level) == expectedLevel &&
                    std::fabs(std::stof(time) - expectedTime) < 0.001f) {
                    return true;
                }
            } catch (const std::exception&) {
                // A malformed row is not the expected persisted score.
            }
        }
    }
    return false;
}

static std::filesystem::path createTemporaryPreferenceDirectory() {
    const auto seed = std::chrono::high_resolution_clock::now()
                          .time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path();
    for (int suffix = 0; suffix < 100; ++suffix) {
        const std::filesystem::path candidate =
            root / ("frozen-bubble-persistence-save-test-" +
                    std::to_string(seed) + "-" + std::to_string(suffix));
        if (std::filesystem::create_directory(candidate)) return candidate;
    }
    return {};
}

int main() {
    SDL_SetEnvironmentVariable(
        SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!TTF_Init()) {
        std::fprintf(stderr, "TTF initialization failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    InitDataDir();
    SDL_Window* window = SDL_CreateWindow(
        "persistence-save-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window != nullptr) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    const std::filesystem::path prefDir = createTemporaryPreferenceDirectory();
    if (prefDir.empty()) {
        std::fprintf(stderr, "temporary preference directory setup failed\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    std::string prefPathStorage = prefDir.string() + "/";
    const std::filesystem::path settingsPath = prefDir / "settings.ini";
    const std::filesystem::path historyPath = prefDir / "highlevelshistory";
    const std::filesystem::path scorePath = prefDir / "highscores";

    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPathStorage.c_str();
    settings->ReadSettings();

    settings->SetValue("GFX:ShowFPS", "");
    CHECK(iniHasKeyValue(settingsPath, "showfps", "true"));

    HighscoreManager* manager = HighscoreManager::Instance(renderer);
    const std::array<std::vector<int>, 10> literalGrid = {{
        {1, 2, 3},
        {4, 5, 6},
        {7, 0, 1},
        {2, 3, 4},
        {5, 6, 7},
        {0, 1, 2},
        {3, 4, 5},
        {6, 7, 0},
        {1, 2, 3},
        {4, 5, 6},
    }};
    manager->AppendToLevels(literalGrid, 17);
    CHECK(fileContains(historyPath, "1   2   3"));

    CHECK(manager->CheckAndAddScore(17, 12.5f));
    CHECK(csvHasLevelAndTime(scorePath, 17, 12.5f));

    manager->Dispose();
    settings->Dispose();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    std::filesystem::remove_all(prefDir);

    return failures == 0 ? 0 : 1;
}
