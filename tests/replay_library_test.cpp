// R4b: the on-disk rolling replay library and the Replay:KeepCount setting.
//
// R4a's ReplayRecorder captures rounds and hands finished .fbr bytes to a sink;
// this package gives that sink a real destination. These tests exercise the
// library directly (Write/List/ReadBytes/Delete/eviction) and the GameSettings
// setting that bounds it, plus the two failure postures that matter: a corrupt
// file in the directory must not break the listing, and a replays/ directory
// that cannot be created must turn every write into a clean no-op.
//
// Fixtures are built with ReplayWriter exactly as tests/bubblegame_replay_test.cpp
// builds its disk recordings -- never a hand-rolled byte layout.

#include <SDL3/SDL.h>

#include "gamesettings.h"
#include "platform.h"
#include "replay_format.h"
#include "replay_library.h"

#include <chrono>
#include <cstdint>
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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Same scratch-pref-dir pattern as tests/persistence_save_test.cpp: each run
// gets its own directory under the system temp dir, never the user's real
// preference directory.
static std::filesystem::path createTemporaryPreferenceDirectory(const char *tag) {
    const auto seed = std::chrono::high_resolution_clock::now()
                          .time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path();
    for (int suffix = 0; suffix < 100; ++suffix) {
        const std::filesystem::path candidate =
            root / (std::string("frozen-bubble-replay-library-test-") + tag + "-" +
                    std::to_string(seed) + "-" + std::to_string(suffix));
        if (std::filesystem::create_directory(candidate)) return candidate;
    }
    return {};
}

// iniparser_dump_ini lowercases keys, pads them, and quotes values, so the
// persisted line reads: keepcount = "9"
static bool iniHasKeyValue(const std::filesystem::path &path,
                           const std::string &key, const std::string &value) {
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

static bool fileContains(const std::filesystem::path &path,
                         const std::string &expected) {
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return input.good() && contents.str().find(expected) != std::string::npos;
}

static int countFbrFiles(const std::filesystem::path &dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec) return 0;
    int count = 0;
    for (const std::filesystem::directory_entry &de :
         std::filesystem::directory_iterator(dir, ec)) {
        std::error_code itemEc;
        if (de.is_regular_file(itemEc) && !itemEc &&
            de.path().extension() == ".fbr") {
            ++count;
        }
    }
    return count;
}

static bool hasStrayTempFile(const std::filesystem::path &dir) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec) return false;
    for (const std::filesystem::directory_entry &de :
         std::filesystem::directory_iterator(dir, ec)) {
        if (de.path().filename().string().find(".tmp") != std::string::npos) {
            return true;
        }
    }
    return false;
}

// A real .fbr stream: header, round start, a few steps, round end. Every field
// under test (mode/players/duration/outcome/complete) is set by the caller so a
// listing that hardcodes or happens to read the wrong record is caught.
static std::vector<uint8_t> makeRecording(uint8_t playerCount, uint8_t gameMode,
                                          uint32_t durationMs, uint8_t outcome,
                                          bool complete, const std::string &fingerprint,
                                          int stepCount = 3,
                                          uint32_t platformFloatProfile = 0) {
    ReplayHeader header;
    header.buildFingerprint.assign(fingerprint.begin(), fingerprint.end());
    header.platformFloatProfile = platformFloatProfile;

    RoundStartRecord start;
    start.playerCount = playerCount;
    start.gameMode = gameMode;

    RoundEndRecord end;
    end.outcome = outcome;
    end.durationMs = durationMs;
    end.complete = complete ? 1 : 0;

    ReplayWriter writer;
    writer.WriteHeader(header);
    writer.WriteRoundStart(start);
    for (int i = 0; i < stepCount; ++i) {
        StepRecord step;
        step.simStep = i;
        writer.WriteStep(step);
    }
    writer.WriteRoundEnd(end);
    return writer.Bytes();
}

static void clearLibrary(ReplayLibrary *library) {
    for (const ReplayLibrary::ReplayEntry &entry : library->List()) {
        library->Delete(entry.filename);
    }
}

// ---------------------------------------------------------------------------

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    const std::filesystem::path prefDir = createTemporaryPreferenceDirectory("main");
    if (prefDir.empty()) {
        std::fprintf(stderr, "temporary preference directory setup failed\n");
        SDL_Quit();
        return 1;
    }
    const std::filesystem::path settingsPath = prefDir / "settings.ini";
    const std::filesystem::path replaysDir = prefDir / "replays";
    std::string prefPathStorage = prefDir.string() + "/";

    GameSettings *settings = GameSettings::Instance();
    settings->prefPath = prefPathStorage.c_str();
    settings->ReadSettings();

    ReplayLibrary *library = ReplayLibrary::Instance();

    // --- 1. GameSettings: default, round-trip, clamping, persistence --------
    {
        // A fresh settings file carries the default.
        CHECK(settings->replayKeepCount() == 5);

        // Dedicated setter persists immediately and a reload of the same path
        // reads it back.
        settings->SetReplayKeepCount(9);
        CHECK(settings->replayKeepCount() == 9);
        CHECK(iniHasKeyValue(settingsPath, "keepcount", "9"));
        settings->ReadSettings();
        CHECK(settings->replayKeepCount() == 9);

        // The setter clamps instead of storing an out-of-range value.
        settings->SetReplayKeepCount(999);
        CHECK(settings->replayKeepCount() == GameSettings::kReplayKeepCountMax);
        CHECK(iniHasKeyValue(settingsPath, "keepcount", "20"));
        settings->SetReplayKeepCount(-4);
        CHECK(settings->replayKeepCount() == 0);
        CHECK(iniHasKeyValue(settingsPath, "keepcount", "0"));

        // An out-of-range or non-numeric file value falls back to the safe
        // default; SetValue's generic string fall-through is used here exactly
        // because it writes the raw text into the file without touching the
        // in-memory member, which is the state a real hand-edited file is in.
        settings->SetValue("Replay:KeepCount", "-1");
        settings->ReadSettings();
        CHECK(settings->replayKeepCount() == 5);
        settings->SetValue("Replay:KeepCount", "21");
        settings->ReadSettings();
        CHECK(settings->replayKeepCount() == 5);
        settings->SetValue("Replay:KeepCount", "abc");
        settings->ReadSettings();
        CHECK(settings->replayKeepCount() == 5);

        // A fresh GameSettings instance reading the same path sees the
        // persisted value -- the "survives restart" requirement.
        settings->SetReplayKeepCount(7);
        settings->Dispose();
        GameSettings *reloaded = GameSettings::Instance();
        reloaded->prefPath = prefPathStorage.c_str();
        reloaded->ReadSettings();
        CHECK(reloaded->replayKeepCount() == 7);
        settings = reloaded;
    }

    // --- 2. Write/ReadBytes round-trip and no stranded .tmp -----------------
    {
        clearLibrary(library);
        settings->SetReplayKeepCount(5);

        const std::vector<uint8_t> bytes =
            makeRecording(1, 0, 1234, 1, true, "round-trip");
        CHECK(library->Write(bytes));
        CHECK(countFbrFiles(replaysDir) == 1);

        const std::vector<ReplayLibrary::ReplayEntry> entries = library->List();
        CHECK(entries.size() == 1);
        if (entries.size() == 1) {
            CHECK(entries[0].playerCount == 1);
            CHECK(entries[0].gameMode == 0);
            CHECK(entries[0].durationMs == 1234);
            CHECK(entries[0].outcome == 1);
            CHECK(entries[0].complete);
            CHECK(!entries[0].imported);

            std::vector<uint8_t> readBack;
            CHECK(library->ReadBytes(entries[0].filename, readBack));
            CHECK(readBack == bytes);
        }

        // Every staged write must have consumed its .tmp file.
        CHECK(!hasStrayTempFile(prefDir));
        CHECK(!hasStrayTempFile(replaysDir));
    }

    // --- 3. Eviction to the keep count, newest-first metadata ---------------
    {
        clearLibrary(library);
        settings->SetReplayKeepCount(3);

        for (int i = 0; i < 5; ++i) {
            const std::vector<uint8_t> bytes = makeRecording(
                static_cast<uint8_t>(i + 1),
                static_cast<uint8_t>(i + 1),
                1000u + static_cast<uint32_t>(i),
                static_cast<uint8_t>((i % 3) + 1),
                true, "evict-" + std::to_string(i), 4);
            CHECK(library->Write(bytes));
        }

        // Exactly the three newest survive on disk...
        CHECK(countFbrFiles(replaysDir) == 3);
        const std::vector<ReplayLibrary::ReplayEntry> entries = library->List();
        CHECK(entries.size() == 3);
        if (entries.size() == 3) {
            // ...listed newest-first, with metadata taken from each recording's
            // own start/end records (indices 4, 3, 2).
            CHECK(entries[0].durationMs == 1004);
            CHECK(entries[1].durationMs == 1003);
            CHECK(entries[2].durationMs == 1002);

            CHECK(entries[0].playerCount == 5);
            CHECK(entries[0].gameMode == 5);
            CHECK(entries[0].outcome == static_cast<uint8_t>((4 % 3) + 1));

            CHECK(entries[1].playerCount == 4);
            CHECK(entries[1].gameMode == 4);
            CHECK(entries[1].durationMs == 1003);

            CHECK(entries[2].playerCount == 3);
            CHECK(entries[2].gameMode == 3);
            CHECK(entries[2].outcome == static_cast<uint8_t>((2 % 3) + 1));
        }
    }

    // --- 4. Keep count 0 refuses to write, regardless of ReplayRecorder -----
    {
        clearLibrary(library);
        settings->SetReplayKeepCount(0);

        const std::vector<uint8_t> bytes = makeRecording(1, 0, 5000, 1, true, "disabled");
        CHECK(library->Write(bytes) == false);
        CHECK(countFbrFiles(replaysDir) == 0);
        CHECK(library->List().empty());

        settings->SetReplayKeepCount(5);
    }

    // --- 5. Delete one entry; path traversal touches nothing outside --------
    {
        clearLibrary(library);
        settings->SetReplayKeepCount(5);

        CHECK(library->Write(makeRecording(1, 1, 2001, 1, true, "del-a", 3)));
        CHECK(library->Write(makeRecording(2, 2, 2002, 2, true, "del-b", 3)));
        CHECK(library->Write(makeRecording(3, 3, 2003, 3, true, "del-c", 3)));

        const std::vector<ReplayLibrary::ReplayEntry> before = library->List();
        CHECK(before.size() == 3);
        if (before.size() == 3) {
            const std::string middle = before[1].filename;
            std::vector<uint8_t> middleBytes;
            CHECK(library->ReadBytes(middle, middleBytes));

            CHECK(library->Delete(middle));
            CHECK(library->Delete(middle) == false);   // already gone

            const std::vector<ReplayLibrary::ReplayEntry> after = library->List();
            CHECK(after.size() == 2);
            for (const ReplayLibrary::ReplayEntry &entry : after) {
                std::vector<uint8_t> bytes;
                CHECK(library->ReadBytes(entry.filename, bytes));
                CHECK(!bytes.empty());
            }
            std::vector<uint8_t> gone;
            CHECK(library->ReadBytes(middle, gone) == false);
        }

        // A sentinel outside replays/ must survive every escape attempt.
        const std::filesystem::path sentinel = prefDir / "traversal-sentinel.txt";
        {
            std::ofstream out(sentinel);
            out << "do not touch\n";
        }
        CHECK(library->Delete("../traversal-sentinel.txt") == false);
        CHECK(library->Delete("..\\traversal-sentinel.txt") == false);
        CHECK(library->Delete(sentinel.string()) == false);      // absolute path
        CHECK(library->Delete("sub/evil.fbr") == false);

        std::vector<uint8_t> out;
        CHECK(library->ReadBytes("../traversal-sentinel.txt", out) == false);
        CHECK(library->ReadBytes(sentinel.string(), out) == false);

        CHECK(std::filesystem::exists(sentinel));
        CHECK(fileContains(sentinel, "do not touch"));
        std::filesystem::remove(sentinel);
    }

    // --- 6. A corrupt file is skipped, not fatal ---------------------------
    {
        clearLibrary(library);
        settings->SetReplayKeepCount(5);

        CHECK(library->Write(makeRecording(1, 1, 3001, 1, true, "valid-a", 4)));
        CHECK(library->Write(makeRecording(2, 2, 3002, 2, false, "valid-b", 4)));

        // Hand-written garbage: not a replay at all.
        {
            std::ofstream bad(replaysDir / "garbage.fbr", std::ios::binary | std::ios::trunc);
            bad << "this is not a replay file";
        }
        // A real recording cut short mid-round-end.
        {
            std::vector<uint8_t> full = makeRecording(3, 3, 3003, 3, true, "trunc", 10);
            full.resize(full.size() - 20);
            std::ofstream out(replaysDir / "truncated.fbr",
                              std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char *>(full.data()),
                      static_cast<std::streamsize>(full.size()));
        }

        const std::vector<ReplayLibrary::ReplayEntry> entries = library->List();
        CHECK(entries.size() == 2);
        if (entries.size() == 2) {
            CHECK(entries[0].durationMs == 3002);
            CHECK(entries[1].durationMs == 3001);
            CHECK(entries[0].complete == false);   // valid-b, complete=0
            CHECK(entries[1].complete);            // valid-a, complete=1
            CHECK(entries[0].playerCount == 2);
            CHECK(entries[1].playerCount == 1);
        }

        std::filesystem::remove(replaysDir / "garbage.fbr");
        std::filesystem::remove(replaysDir / "truncated.fbr");
        clearLibrary(library);
    }

    // --- 7. An unusable replays/ directory fails safely ---------------------
    {
        const std::filesystem::path brokenRoot =
            createTemporaryPreferenceDirectory("broken");
        if (!brokenRoot.empty()) {
            // A regular file where the replays/ directory needs to be.
            {
                std::ofstream inTheWay(brokenRoot / "replays");
                inTheWay << "in the way\n";
            }
            const std::string brokenPrefStorage = brokenRoot.string() + "/";
            settings->prefPath = brokenPrefStorage.c_str();

            const std::vector<uint8_t> bytes =
                makeRecording(1, 0, 4001, 1, true, "broken");
            CHECK(library->Write(bytes) == false);
            CHECK(library->List().empty());

            std::vector<uint8_t> out;
            CHECK(library->ReadBytes("whatever.fbr", out) == false);
            CHECK(library->Delete("whatever.fbr") == false);

            // Nothing outside was created or replaced.
            CHECK(std::filesystem::is_regular_file(brokenRoot / "replays"));

            settings->prefPath = prefPathStorage.c_str();
            std::filesystem::remove_all(brokenRoot);
        }
    }

    // --- 8. R4d: keep-count preview and apply-immediately eviction ---------
    {
        clearLibrary(library);
        // The setting is deliberately not the value under test: EvictionCountFor
        // and ApplyKeepCount take the candidate count as an argument, so the
        // Replays page can preview a value it has not committed yet.
        settings->SetReplayKeepCount(5);

        for (int i = 0; i < 5; ++i) {
            CHECK(library->Write(makeRecording(
                static_cast<uint8_t>(i + 1), static_cast<uint8_t>(i + 1),
                5000u + static_cast<uint32_t>(i), 1, true,
                "keep-" + std::to_string(i), 3)));
        }
        const std::vector<ReplayLibrary::ReplayEntry> before = library->List();
        CHECK(before.size() == 5);

        // Preview: how many an eviction to a candidate count would remove.
        CHECK(library->EvictionCountFor(5) == 0);
        CHECK(library->EvictionCountFor(4) == 1);
        CHECK(library->EvictionCountFor(2) == 3);
        // 0 (and negative) means "stop recording new rounds," never "delete
        // what's already saved" -- REPLAY_PLAN.md is explicit that a settings
        // change must never act as a bulk delete. The preview must say so.
        CHECK(library->EvictionCountFor(0) == 0);
        CHECK(library->EvictionCountFor(-3) == 0);

        // Applying a count at or above the current size is a no-op.
        library->ApplyKeepCount(5);
        CHECK(countFbrFiles(replaysDir) == 5);
        library->ApplyKeepCount(9);
        CHECK(countFbrFiles(replaysDir) == 5);

        // Applying 0 (or negative) is also a no-op on existing entries, for
        // the same reason the preview reports 0 for it above.
        library->ApplyKeepCount(0);
        CHECK(countFbrFiles(replaysDir) == 5);
        library->ApplyKeepCount(-1);
        CHECK(countFbrFiles(replaysDir) == 5);

        // Applying a lower count deletes exactly the oldest entries: the
        // survivors are the first `keep` entries in List()'s newest-first
        // order (the exact reverse of the oldest-first eviction order).
        const int wouldDelete = library->EvictionCountFor(2);
        CHECK(wouldDelete == 3);
        library->ApplyKeepCount(2);
        CHECK(countFbrFiles(replaysDir) == 2);
        const std::vector<ReplayLibrary::ReplayEntry> after = library->List();
        CHECK(after.size() == 2);
        if (after.size() == 2 && before.size() == 5) {
            CHECK(after[0].filename == before[0].filename);
            CHECK(after[1].filename == before[1].filename);
            CHECK(after[0].durationMs == 5004);
            CHECK(after[1].durationMs == 5003);
        }
        CHECK(library->EvictionCountFor(2) == 0);   // nothing left to remove

        // Write()'s own eviction-after-write still behaves with the new
        // methods compiled in: the new file lands first, then the oldest are
        // removed down to the configured count.
        clearLibrary(library);
        settings->SetReplayKeepCount(3);
        for (int i = 0; i < 4; ++i) {
            CHECK(library->Write(makeRecording(1, 0, 6000u + static_cast<uint32_t>(i),
                                               1, true,
                                               "rewrite-" + std::to_string(i), 3)));
        }
        CHECK(countFbrFiles(replaysDir) == 3);
        const std::vector<ReplayLibrary::ReplayEntry> rewritten = library->List();
        CHECK(rewritten.size() == 3);
        if (rewritten.size() == 3) {
            CHECK(rewritten[0].durationMs == 6003);
            CHECK(rewritten[2].durationMs == 6001);
        }
        clearLibrary(library);
    }

    // --- 9. R7: platform/float profile compatibility flag ------------------
    {
        clearLibrary(library);
        settings->SetReplayKeepCount(5);

        const uint32_t currentProfile = ComputeCurrentPlatformFloatProfile();
        // A known bucket that is definitely not this build's, without hardcoding
        // which platform this test runs on.
        const uint32_t otherProfile = (currentProfile == 1) ? 2u : 1u;
        CHECK(currentProfile != 0);
        CHECK(otherProfile != currentProfile);
        CHECK(IsReplayPlatformCompatible(currentProfile));
        CHECK(!IsReplayPlatformCompatible(otherProfile));

        CHECK(library->Write(makeRecording(1, 0, 7001, 1, true, "platform-match",
                                           3, currentProfile)));
        CHECK(library->Write(makeRecording(1, 0, 7002, 1, true, "platform-mismatch",
                                           3, otherProfile)));

        // Both are listed: an incompatible entry is badged, never skipped the
        // way a genuinely corrupt file is.
        const std::vector<ReplayLibrary::ReplayEntry> entries = library->List();
        CHECK(entries.size() == 2);
        if (entries.size() == 2) {
            // Newest-first: mismatch (7002) before match (7001).
            CHECK(entries[0].platformIncompatible == true);
            CHECK(entries[1].platformIncompatible == false);
        }
        clearLibrary(library);
    }

    library->Dispose();
    settings->Dispose();
    SDL_Quit();
    std::filesystem::remove_all(prefDir);

    if (failures == 0) std::printf("replay library tests passed\n");
    return failures == 0 ? 0 : 1;
}
