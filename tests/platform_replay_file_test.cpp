// R4e: the platform replay export/import seam.
//
// R4d wired the Replays page's Export/Import rows to a pair of bool-returning
// stubs that always failed. R4e replaced them with an asynchronous
// Begin()/Poll() state machine: a native SDL file dialog on desktop, a
// Blob-download/file-input bridge on WASM. A real OS dialog cannot run under
// ctest, so the state machine is exercised through the test-only
// TestSimulate*DialogResult() hooks and the byte-level helpers the desktop
// callbacks use (WriteReplayBytesToPath/ReadReplayBytesFromPath) are tested
// directly.
//
// The WASM JS bridge (EM_JS + the EMSCRIPTEN_KEEPALIVE callbacks) has no
// automated coverage here: it can only run in a browser, not under ctest.

#include "platform.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

// Each run gets its own scratch directory under the system temp dir, never the
// user's real preference directory -- the same pattern the other replay tests
// use.
static std::filesystem::path createTemporaryDirectory(const char *tag) {
    const auto seed = std::chrono::high_resolution_clock::now()
                          .time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path();
    for (int suffix = 0; suffix < 100; ++suffix) {
        const std::filesystem::path candidate =
            root / (std::string("frozen-bubble-platform-replay-test-") + tag + "-" +
                    std::to_string(seed) + "-" + std::to_string(suffix));
        if (std::filesystem::create_directory(candidate)) return candidate;
    }
    return {};
}

static std::vector<uint8_t> knownBytes(uint32_t seed, size_t count) {
    std::vector<uint8_t> bytes(count);
    for (size_t i = 0; i < count; ++i) {
        bytes[i] = static_cast<uint8_t>((seed * 31u + i * 7u) & 0xFFu);
    }
    return bytes;
}

// ---------------------------------------------------------------------------
// 1. Write/Read round-trip and failure cases
// ---------------------------------------------------------------------------

static void testByteRoundTrip(const std::filesystem::path &scratch) {
    const std::filesystem::path path = scratch / "round-trip.fbr";

    std::vector<uint8_t> payload;
    payload.reserve(256);
    for (int i = 0; i < 256; ++i) payload.push_back(static_cast<uint8_t>(i));

    CHECK(WriteReplayBytesToPath(path.string(), payload));

    std::vector<uint8_t> readBack;
    CHECK(ReadReplayBytesFromPath(path.string(), readBack, 4096));
    CHECK(readBack == payload);

    // Writing again truncates rather than appending.
    const std::vector<uint8_t> shorter = {9, 8, 7};
    CHECK(WriteReplayBytesToPath(path.string(), shorter));
    std::vector<uint8_t> readBack2;
    CHECK(ReadReplayBytesFromPath(path.string(), readBack2, 4096));
    CHECK(readBack2 == shorter);

    // Zero-length content is a valid round-trip too.
    CHECK(WriteReplayBytesToPath(path.string(), {}));
    std::vector<uint8_t> readBack3;
    CHECK(ReadReplayBytesFromPath(path.string(), readBack3, 4096));
    CHECK(readBack3.empty());
}

static void testReadFailures(const std::filesystem::path &scratch) {
    // A path whose parent directory does not exist must fail cleanly.
    const std::filesystem::path missing =
        scratch / "no-such-dir" / "nothing.fbr";
    std::vector<uint8_t> out;
    CHECK(!ReadReplayBytesFromPath(missing.string(), out, 4096));
    CHECK(out.empty());

    // An existing file that is larger than the supplied cap is rejected
    // without the whole oversized content being loaded. A small cap passed
    // explicitly avoids needing a real 16 MiB file on disk.
    const std::filesystem::path big = scratch / "too-big.fbr";
    CHECK(WriteReplayBytesToPath(big.string(), knownBytes(1, 64)));
    std::vector<uint8_t> capped;
    CHECK(!ReadReplayBytesFromPath(big.string(), capped, 16));
    CHECK(capped.empty());

    // The same file reads fine at a cap that fits it.
    std::vector<uint8_t> fits;
    CHECK(ReadReplayBytesFromPath(big.string(), fits, 64));
    CHECK(fits.size() == 64);

    // A write into a nonexistent directory fails rather than creating anything.
    const std::filesystem::path badWrite =
        scratch / "still-no-dir" / "out.fbr";
    CHECK(!WriteReplayBytesToPath(badWrite.string(), {1, 2, 3}));
}

// ---------------------------------------------------------------------------
// 2. Export Begin()/Poll() state machine
// ---------------------------------------------------------------------------

static void testExportStateMachine(const std::filesystem::path &scratch) {
    TestResetPlatformReplayFileState();
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Idle);

    const std::vector<uint8_t> payload = knownBytes(7, 300);
    CHECK(PlatformExportReplayFileBegin(nullptr, "chosen.fbr", payload));

    // Re-entering while in flight is refused.
    CHECK(!PlatformExportReplayFileBegin(nullptr, "again.fbr", payload));

    // Not resolved yet.
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Pending);

    // A chosen path: the callback writes the bytes there and marks success.
    const std::filesystem::path outPath = scratch / "export-chosen.fbr";
    const std::string outPathStr = outPath.string();
    const char *chosen[] = {outPathStr.c_str(), nullptr};
    TestSimulateExportDialogResult(chosen);

    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Succeeded);
    // A terminal result is consumed exactly once.
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Idle);

    std::vector<uint8_t> written;
    CHECK(ReadReplayBytesFromPath(outPathStr, written, 4096));
    CHECK(written == payload);

    // Cancel: an empty (null-first) filelist.
    TestResetPlatformReplayFileState();
    CHECK(PlatformExportReplayFileBegin(nullptr, "cancel.fbr", payload));
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Pending);
    const char *cancelled[] = {nullptr};
    TestSimulateExportDialogResult(cancelled);
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Cancelled);
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Idle);

    // Failure: a null filelist.
    TestResetPlatformReplayFileState();
    CHECK(PlatformExportReplayFileBegin(nullptr, "fail.fbr", payload));
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Pending);
    TestSimulateExportDialogResult(nullptr);
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Failed);
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Idle);
}

// ---------------------------------------------------------------------------
// 3. Import Begin()/Poll() state machine
// ---------------------------------------------------------------------------

static void testImportStateMachine(const std::filesystem::path &scratch) {
    const std::vector<uint8_t> payload = knownBytes(11, 512);
    const std::filesystem::path srcPath = scratch / "to-import.fbr";
    CHECK(WriteReplayBytesToPath(srcPath.string(), payload));

    std::vector<uint8_t> pollBytes;
    std::string pollName;

    TestResetPlatformReplayFileState();
    CHECK(PlatformImportReplayFilePoll(pollBytes, pollName) ==
          PlatformFileOpStatus::Idle);

    CHECK(PlatformImportReplayFileBegin(nullptr));
    CHECK(!PlatformImportReplayFileBegin(nullptr));   // refuses re-entry
    CHECK(PlatformImportReplayFilePoll(pollBytes, pollName) ==
          PlatformFileOpStatus::Pending);

    const std::string srcPathStr = srcPath.string();
    const char *chosen[] = {srcPathStr.c_str(), nullptr};
    TestSimulateImportDialogResult(chosen);

    std::vector<uint8_t> outBytes;
    std::string outName;
    CHECK(PlatformImportReplayFilePoll(outBytes, outName) ==
          PlatformFileOpStatus::Succeeded);
    CHECK(outBytes == payload);
    CHECK(outName == "to-import.fbr");

    // Consumed exactly once; the second poll is Idle.
    CHECK(PlatformImportReplayFilePoll(outBytes, outName) ==
          PlatformFileOpStatus::Idle);

    // Cancel.
    TestResetPlatformReplayFileState();
    CHECK(PlatformImportReplayFileBegin(nullptr));
    CHECK(PlatformImportReplayFilePoll(pollBytes, pollName) ==
          PlatformFileOpStatus::Pending);
    const char *cancelled[] = {nullptr};
    TestSimulateImportDialogResult(cancelled);
    CHECK(PlatformImportReplayFilePoll(outBytes, outName) ==
          PlatformFileOpStatus::Cancelled);
    CHECK(PlatformImportReplayFilePoll(outBytes, outName) ==
          PlatformFileOpStatus::Idle);

    // Failure: a null filelist.
    TestResetPlatformReplayFileState();
    CHECK(PlatformImportReplayFileBegin(nullptr));
    CHECK(PlatformImportReplayFilePoll(pollBytes, pollName) ==
          PlatformFileOpStatus::Pending);
    TestSimulateImportDialogResult(nullptr);
    CHECK(PlatformImportReplayFilePoll(outBytes, outName) ==
          PlatformFileOpStatus::Failed);
    CHECK(PlatformImportReplayFilePoll(outBytes, outName) ==
          PlatformFileOpStatus::Idle);

    // A chosen path that doesn't exist reads as a failure, not a crash.
    TestResetPlatformReplayFileState();
    CHECK(PlatformImportReplayFileBegin(nullptr));
    const std::string missing = (scratch / "does-not-exist.fbr").string();
    const char *missingList[] = {missing.c_str(), nullptr};
    TestSimulateImportDialogResult(missingList);
    CHECK(PlatformImportReplayFilePoll(outBytes, outName) ==
          PlatformFileOpStatus::Failed);
    CHECK(PlatformImportReplayFilePoll(outBytes, outName) ==
          PlatformFileOpStatus::Idle);
}

// ---------------------------------------------------------------------------
// 4. Cross-thread completion (SDL may invoke its callback off-thread)
// ---------------------------------------------------------------------------

static void testCrossThreadCompletion(const std::filesystem::path &scratch) {
    TestResetPlatformReplayFileState();

    const std::vector<uint8_t> payload = knownBytes(23, 128);
    CHECK(PlatformExportReplayFileBegin(nullptr, "threaded.fbr", payload));

    const std::filesystem::path outPath = scratch / "threaded-out.fbr";
    const std::string outPathStr = outPath.string();

    // Fire the simulated dialog callback from another thread while the main
    // thread polls, the way SDL's own callback may arrive from a backend
    // thread. This is not a TSan run -- just a check that nothing crashes and
    // the terminal result is observed.
    std::thread callbackThread([&outPathStr]() {
        const char *chosen[] = {outPathStr.c_str(), nullptr};
        TestSimulateExportDialogResult(chosen);
    });

    PlatformFileOpStatus status = PlatformFileOpStatus::Idle;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        status = PlatformExportReplayFilePoll();
        if (status == PlatformFileOpStatus::Succeeded ||
            status == PlatformFileOpStatus::Cancelled ||
            status == PlatformFileOpStatus::Failed) {
            break;
        }
        std::this_thread::yield();
    }
    callbackThread.join();

    CHECK(status == PlatformFileOpStatus::Succeeded);
    // And the operation is done, so the next poll is Idle.
    CHECK(PlatformExportReplayFilePoll() == PlatformFileOpStatus::Idle);

    std::vector<uint8_t> written;
    CHECK(ReadReplayBytesFromPath(outPathStr, written, 4096));
    CHECK(written == payload);
}

int main() {
    // Native dialogs cannot run under ctest; this makes the desktop Begin()
    // arm the pending state without opening one, so TestSimulate*DialogResult()
    // can drive the completion path.
    testReplayFileOpsHeadless = true;

    const std::filesystem::path scratch = createTemporaryDirectory("main");
    if (scratch.empty()) {
        std::fprintf(stderr, "could not create a scratch directory\n");
        return 1;
    }

    testByteRoundTrip(scratch);
    testReadFailures(scratch);
    testExportStateMachine(scratch);
    testImportStateMachine(scratch);
    testCrossThreadCompletion(scratch);

    std::filesystem::remove_all(scratch);

    if (failures == 0) std::printf("platform replay file tests passed\n");
    return failures == 0 ? 0 : 1;
}
