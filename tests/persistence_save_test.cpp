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

// An empty level grid serializes to a line of spaces with no digits in it,
// which is how a phantom map entry shows up in the saved history.
static bool hasBlankPaddedLine(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() &&
            line.find_first_not_of(" \t\r") == std::string::npos) {
            return true;
        }
    }
    return false;
}

static int countCsvRows(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string line;
    int rows = 0;
    while (std::getline(input, line)) {
        if (!line.empty()) ++rows;
    }
    return rows;
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

    // The history must hold the one level that was appended and nothing else.
    // Indexing the level map by position used to default-insert an empty grid
    // for every absent id, so appending level 17 alone wrote 18 blank grids
    // alongside it -- each one a line of spaces.
    CHECK(!hasBlankPaddedLine(historyPath));

    CHECK(manager->CheckAndAddScore(17, 12.5f));
    CHECK(csvHasLevelAndTime(scorePath, 17, 12.5f));

    // Each save now stages a .tmp file beside the real one. Leaving those
    // behind would litter the preferences directory on every toggle and score,
    // so every completed save must have consumed its staging file.
    CHECK(!std::filesystem::exists(settingsPath.string() + ".tmp"));
    CHECK(!std::filesystem::exists(historyPath.string() + ".tmp"));
    CHECK(!std::filesystem::exists(scorePath.string() + ".tmp"));

    // A save replaces the table rather than appending to it, so a second score
    // must not leave the first one duplicated.
    CHECK(manager->CheckAndAddScore(18, 9.0f));
    CHECK(csvHasLevelAndTime(scorePath, 18, 9.0f));
    CHECK(countCsvRows(scorePath) == 2);

    // The swap itself must replace the destination outright, not merge into it.
    const std::filesystem::path replaceTarget = prefDir / "replace-target";
    const std::filesystem::path replaceSource = prefDir / "replace-target.tmp";
    { std::ofstream(replaceTarget) << "stale contents that must not survive\n"; }
    { std::ofstream(replaceSource) << "fresh\n"; }
    CHECK(ReplaceFileAtomically(replaceSource.string(), replaceTarget.string()));
    CHECK(fileContains(replaceTarget, "fresh"));
    CHECK(!fileContains(replaceTarget, "stale"));
    CHECK(!std::filesystem::exists(replaceSource));

    // A failed swap must leave the destination untouched rather than destroying
    // it, and must not strand the staging file.
    const std::filesystem::path missingSource = prefDir / "absent.tmp";
    CHECK(!ReplaceFileAtomically(missingSource.string(), replaceTarget.string()));
    CHECK(fileContains(replaceTarget, "fresh"));

    // A save must rewrite only the table that changed. Planting a sentinel in
    // the history file makes that observable: adding a score touches the score
    // table alone, so a save that still rewrites both would erase the sentinel.
    { std::ofstream(historyPath) << "SENTINEL-HISTORY-UNTOUCHED\n"; }
    CHECK(manager->CheckAndAddScore(19, 7.5f));
    CHECK(csvHasLevelAndTime(scorePath, 19, 7.5f));
    CHECK(fileContains(historyPath, "SENTINEL-HISTORY-UNTOUCHED"));

    // The mirror of the same rule: appending a level must not rewrite scores.
    { std::ofstream(scorePath) << "SENTINEL-SCORES-UNTOUCHED\n"; }
    manager->AppendToLevels(literalGrid, 21);
    CHECK(fileContains(historyPath, "1   2   3"));
    CHECK(fileContains(scorePath, "SENTINEL-SCORES-UNTOUCHED"));

    // Followed servers -- the list of servers allowed to send this device a
    // "someone joined" notification. This has to survive a restart or the
    // player's follows silently vanish, so assert the round trip through the
    // file rather than just the in-memory vector.
    CHECK(settings->followedServers.empty());
    CHECK(!settings->IsServerFollowed("fb.example.org", 1511));

    CHECK(settings->ToggleServerFollowed("fb.example.org", 1511, "Example"));
    CHECK(settings->IsServerFollowed("fb.example.org", 1511));
    // Same host on a different port is a different server.
    CHECK(!settings->IsServerFollowed("fb.example.org", 1512));

    CHECK(settings->ToggleServerFollowed("192.168.1.50", 1511, ""));
    settings->SaveKeys();
    CHECK(iniHasKeyValue(settingsPath, "followedcount", "2"));
    CHECK(iniHasKeyValue(settingsPath, "followed0host", "fb.example.org"));
    CHECK(iniHasKeyValue(settingsPath, "followed0port", "1511"));
    CHECK(iniHasKeyValue(settingsPath, "followed1host", "192.168.1.50"));

    settings->followedServers.clear();
    settings->LoadFollowedServers();
    CHECK(settings->followedServers.size() == 2);
    CHECK(settings->IsServerFollowed("fb.example.org", 1511));
    CHECK(settings->IsServerFollowed("192.168.1.50", 1511));
    CHECK(settings->followedServers[0].label == "Example");

    // Toggling an existing entry removes it, and the removal must reach the
    // file too -- an unfollow that only cleared memory would come back on the
    // next launch and keep notifying.
    CHECK(!settings->ToggleServerFollowed("fb.example.org", 1511, "Example"));
    CHECK(!settings->IsServerFollowed("fb.example.org", 1511));
    settings->SaveKeys();
    settings->followedServers.clear();
    settings->LoadFollowedServers();
    CHECK(settings->followedServers.size() == 1);
    CHECK(!settings->IsServerFollowed("fb.example.org", 1511));
    CHECK(settings->IsServerFollowed("192.168.1.50", 1511));

    // The list is bounded. Past the cap a follow is refused outright rather
    // than evicting somebody else's entry or growing without limit.
    for (int i = 0; i < GameSettings::kMaxFollowedServers + 3; i++) {
        settings->ToggleServerFollowed("host" + std::to_string(i) + ".test",
                                       1511, "");
    }
    CHECK((int)settings->followedServers.size() == GameSettings::kMaxFollowedServers);

    // Garbage in the file is dropped rather than loaded as an entry that can
    // never match a real server.
    settings->followedServers.clear();
    settings->SaveKeys();
    settings->SetValue("Servers:FollowedCount", "2");
    settings->SetValue("Servers:Followed0Host", "");
    settings->SetValue("Servers:Followed0Port", "1511");
    settings->SetValue("Servers:Followed1Host", "ok.example.org");
    settings->SetValue("Servers:Followed1Port", "0");
    settings->LoadFollowedServers();
    CHECK(settings->followedServers.empty());

    settings->followedServers.clear();
    settings->SaveKeys();

    // Reset-to-defaults must reach the file, not just the in-memory members. A
    // reset that only reassigned the members would be undone by the next
    // SaveKeys() writing the stale values straight back out, so assert against
    // settings.ini as well as the object.
    settings->speedMultiplier = 4.5f;
    settings->mouseEnabled = !GameSettings::DefaultMouseEnabled();
    settings->player1Keys.fire = (SDL_Scancode)99;
    settings->SaveKeys();
    CHECK(iniHasKeyValue(settingsPath, "p1fire", "99"));

    settings->ResetToDefaults();
    CHECK(settings->speedMultiplier == GameSettings::DEFAULT_SPEED_MULTIPLIER);
    CHECK(settings->mouseEnabled == GameSettings::DefaultMouseEnabled());
    CHECK(settings->player1Keys.fire == SDL_SCANCODE_UP);
    CHECK(iniHasKeyValue(settingsPath, "p1fire", "82"));
    // ShowFPS was turned on at the top of this test; a reset has to clear
    // everything, not only the keys the reset row sits next to.
    CHECK(iniHasKeyValue(settingsPath, "showfps", "false"));

    // The value written to the file on a reset must agree with the fallback the
    // getter would use. These are two separate code paths, and when they
    // disagree the written value silently wins on every first run.
    CHECK(iniHasKeyValue(settingsPath, "mouseenabled",
                         GameSettings::DefaultMouseEnabled() ? "true" : "false"));

    // And with nothing changed since the last save, shutdown must write nothing
    // at all rather than rewriting both tables on the way out.
    { std::ofstream(historyPath) << "SENTINEL-DISPOSE-HISTORY\n"; }
    { std::ofstream(scorePath) << "SENTINEL-DISPOSE-SCORES\n"; }
    manager->Dispose();
    CHECK(fileContains(historyPath, "SENTINEL-DISPOSE-HISTORY"));
    CHECK(fileContains(scorePath, "SENTINEL-DISPOSE-SCORES"));
    settings->Dispose();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    std::filesystem::remove_all(prefDir);

    return failures == 0 ? 0 : 1;
}
