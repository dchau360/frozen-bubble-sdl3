// R4a: production replay capture, end to end.
//
// The replay engine (R3/R5/R6) was only ever driven from tests; R4a wires it
// into BubbleGame::NewGame()/ReloadGame()/AdvanceSimulationAtScale() through
// the ReplayRecorder singleton. These tests drive real rounds with the
// recorder's sink installed and assert what actually reaches the sink:
//
//  1. a completed local round seals exactly one recording whose bytes decode
//     and replay hash-equal at every step;
//  2. ReloadGame() seals round 1 and opens round 2, carrying wins/RNG forward;
//  3. keep count 0 records nothing at all;
//  4. a Playback instance records nothing;
//  5. a network round's late 'S' result tail lands inside the sealed recording,
//     and the timeout path seals with missing stats explicitly marked;
//  6. a transient-budget overrun seals the recording marked incomplete and the
//     round keeps playing.
//
// The sink is the only observation point this package needs, so no file I/O is
// exercised here -- that is R4b's job.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "bubblegame_replay.h"
#include "gamesettings.h"
#include "networkclient.h"
#include "platform.h"
#include "replay_format.h"
#include "replay_recorder.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
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

struct BubbleGameTestAccess {
    static BubbleArray &player(BubbleGame &game, int idx) { return game.bubbleArrays[idx]; }
    static void seedRng(BubbleGame &game, uint32_t seed) {
        game.rng.Seed(seed);
        game.rngExplicitlySeeded = true;
    }
    static void advance(BubbleGame &game, float deltaScale, Uint32 gameClockMs) {
        game.AdvanceSimulationAtScale(deltaScale, gameClockMs);
    }
    static void reload(BubbleGame &game, int level) { game.ReloadGame(level); }
    static bool gameFinish(const BubbleGame &game) { return game.gameFinish; }
    static int simStep(const BubbleGame &game) { return game.simStep; }
    static void setSessionMode(BubbleGame &game, BubbleGame::SessionMode mode) { game.sessionMode = mode; }
    static BubbleGame::SessionMode sessionMode(const BubbleGame &game) { return game.sessionMode; }
};

struct NetworkClientTestAccess {
    static void SetPlayerNick(NetworkClient &nc, const std::string &nick) { nc.playerNick = nick; }
    static void SetCurrentGame(NetworkClient &nc, GameRoom *game) { nc.currentGame = game; }
    static void SetState(NetworkClient &nc, ConnectionState state) { nc.state = state; }
    static void SetMyPlayerId(NetworkClient &nc, int id) { nc.myPlayerId = (unsigned char)id; }
    static void SetPlayerIdToNick(NetworkClient &nc, const std::map<int, std::string> &m) {
        nc.playerIdToNick = m;
    }
};

// ---------------------------------------------------------------------------
// Sink and recorder control
// ---------------------------------------------------------------------------

// File scope so the std::function the singleton holds always points at a
// living vector; ResetRecorder() re-points it at a cleared one per scenario.
static std::vector<std::vector<uint8_t>> g_recordings;

static void ResetRecorder(int keepCount = 5, size_t budget = 64u * 1024u * 1024u) {
    g_recordings.clear();
    ReplayRecorder::Instance()->Dispose();
    ReplayRecorder *rec = ReplayRecorder::Instance();
    rec->SetSink([](std::vector<uint8_t> bytes) { g_recordings.push_back(std::move(bytes)); });
    rec->SetKeepCount(keepCount);
    rec->SetTransientBudgetBytes(budget);
}

// ---------------------------------------------------------------------------
// Setups
// ---------------------------------------------------------------------------

static SetupSettings MakeTrainingSetup() {
    SetupSettings s;
    s.playerCount = 1;
    s.networkGame = false;
    s.randomLevels = false;
    s.startLevel = 1;
    s.gameMode = GameMode::Classic;
    s.mpTraining = true;
    return s;
}

static SetupSettings MakeNetworkSetup() {
    SetupSettings s;
    s.playerCount = 2;
    s.networkGame = true;
    // randomLevels=false keeps NewGame() off SyncNetworkLevel(), so no socket
    // is needed; the shipped level fills seat 0 and seat 1 stays empty, which
    // is a perfectly valid record for these scenarios.
    s.randomLevels = false;
    s.startLevel = 1;
    s.gameMode = GameMode::Classic;
    s.chainReaction = false;
    return s;
}

static void SetupFakeNetworkClient() {
    NetworkClient *nc = NetworkClient::Instance();
    NetworkClientTestAccess::SetState(*nc, IN_GAME);
    NetworkClientTestAccess::SetMyPlayerId(*nc, 0);
    NetworkClientTestAccess::SetPlayerNick(*nc, "local_nick");
    NetworkClientTestAccess::SetPlayerIdToNick(
        *nc, std::map<int, std::string>{{0, "local_nick"}, {1, "remote_nick"}});
    NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    while (nc->HasMessage()) nc->GetNextMessage();
}

static void ResetFakeNetworkClient() {
    NetworkClient *nc = NetworkClient::Instance();
    NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    NetworkClientTestAccess::SetState(*nc, DISCONNECTED);
    while (nc->HasMessage()) nc->GetNextMessage();
}

// ---------------------------------------------------------------------------
// Read a recording back and replay it (the EncodeRecording/DecodeRecording/
// replay-and-compare shape from tests/bubblegame_replay_test.cpp, minus the
// encode side -- the recorder is the producer here).
// ---------------------------------------------------------------------------

struct DecodedRecording {
    ReplayHeader header;
    RoundStartRecord start;
    std::vector<StepRecord> steps;
    std::vector<uint64_t> hashes;
    RoundEndRecord end;
    bool sawRoundEnd = false;
};

static bool DecodeRecording(const std::vector<uint8_t> &bytes, DecodedRecording &out) {
    ReplayReader reader(bytes);
    if (reader.ReadHeader(out.header) != DecodeResult::Ok) return false;
    if (reader.ReadRoundStart(out.start) != DecodeResult::Ok) return false;

    while (true) {
        RecordType type;
        const DecodeResult peek = reader.PeekRecordType(type);
        if (peek == DecodeResult::Truncated) break;  // clean end of stream
        if (peek != DecodeResult::Ok) return false;

        if (type == RecordType::Step) {
            StepRecord s;
            if (reader.ReadStep(s) != DecodeResult::Ok) return false;
            out.steps.push_back(s);
        } else if (type == RecordType::Assertion) {
            AssertionRecord a;
            if (reader.ReadAssertion(a) != DecodeResult::Ok) return false;
            out.hashes.push_back(a.canonicalStateHash);
        } else if (type == RecordType::RoundEnd) {
            if (reader.ReadRoundEnd(out.end) != DecodeResult::Ok) return false;
            out.sawRoundEnd = true;
        } else {
            return false;
        }
    }
    return true;
}

struct ReplayResult {
    std::vector<uint64_t> hashes;
    std::vector<int> rFired;
    bool gameFinish = false;
};

// Restores the recording and drives it exactly as the game's playback path
// does, returning the per-step canonical hashes and the remote seats' final
// round-stat counters (so a late 'S' can be observed after replay).
static ReplayResult ReplayRecording(SDL_Renderer *renderer, const DecodedRecording &rec) {
    singleBubbles.clear();
    malusBubbles.clear();

    BubbleGame game(renderer);
    RestoreRoundStart(game, rec.start);
    CHECK(BubbleGameTestAccess::sessionMode(game) == BubbleGame::SessionMode::Playback);

    const int playerCount = static_cast<int>(rec.start.playerCount);
    ReplayResult result;
    size_t i = 0;
    while (i < rec.steps.size()) {
        const int step = rec.steps[i].simStep;
        const float deltaScale = rec.steps[i].deltaScale;
        const Uint32 gameClockMs = rec.steps[i].gameClockMs;
        const std::vector<uint8_t> *inbound = nullptr;
        while (i < rec.steps.size() && rec.steps[i].simStep == step) {
            const StepRecord &s = rec.steps[i];
            if (s.seatId < static_cast<uint32_t>(playerCount)) {
                PlayerControls c;
                c.left = s.left != 0;
                c.right = s.right != 0;
                c.center = s.center != 0;
                c.fire = s.fire != 0;
                c.firedByMouse = s.firedByMouse != 0;
                c.mouseAngle = s.mouseAngle;
                BubbleGameTestAccess::player(game, static_cast<int>(s.seatId)).lastControls = c;
            }
            if (s.seatId == 0) inbound = &s.inboundEvents;
            ++i;
        }
        static const std::vector<uint8_t> kNoInbound;
        SetPlaybackInboundEvents(game, inbound ? *inbound : kNoInbound);
        game.AdvancePlaybackStep(deltaScale, gameClockMs);
        result.hashes.push_back(CaptureCanonicalStateHash(game));
    }
    result.gameFinish = BubbleGameTestAccess::gameFinish(game);
    for (int seat = 0; seat < playerCount; ++seat)
        result.rFired.push_back(BubbleGameTestAccess::player(game, seat).rFired);
    return result;
}

static void CheckHashesEqual(const char *label, const std::vector<uint64_t> &expected,
                             const std::vector<uint64_t> &actual) {
    if (expected.size() != actual.size()) {
        std::fprintf(stderr, "CHECK failed: %s: hash count mismatch (%zu vs %zu)\n",
                     label, expected.size(), actual.size());
        ++failures;
        return;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] != actual[i]) {
            std::fprintf(stderr, "CHECK failed: %s: first divergent step %zu\n", label, i);
            ++failures;
            return;
        }
    }
}

// Create a fresh Live BubbleGame (NewGame runs the recorder's round-start
// hook). Returned by unique_ptr because BubbleGame is non-copyable.
static std::unique_ptr<BubbleGame> NewLiveGame(SDL_Renderer *renderer, uint32_t seed,
                                               const SetupSettings &setup) {
    auto game = std::make_unique<BubbleGame>(renderer);
    BubbleGameTestAccess::seedRng(*game, seed);
    BubbleGameTestAccess::setSessionMode(*game, BubbleGame::SessionMode::Live);
    game->NewGame(setup);
    return game;
}

// Drive a local training round to its recorded two-minute clock expiry with a
// handful of synthetic steps, returning the canonical hash after each. The
// board is whatever NewGame() generated, so it matches the recorder's
// round-start snapshot without any test-side board editing.
static std::vector<uint64_t> DriveTrainingCollectingHashes(BubbleGame &game) {
    std::vector<uint64_t> hashes;
    const Uint32 clocks[3] = {1000u, 1001u, 1000u + 120000u + 10u};
    for (Uint32 clock : clocks) {
        BubbleGameTestAccess::advance(game, 1.0f, clock);
        hashes.push_back(CaptureCanonicalStateHash(game));
    }
    return hashes;
}

static void DriveTrainingToFinish(BubbleGame &game) {
    DriveTrainingCollectingHashes(game);
}

// ---------------------------------------------------------------------------

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_AUDIODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();

    SDL_Window *window = SDL_CreateWindow("replay-recorder-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Keep highscore writes out of the user's real preference directory.
    std::error_code ec;
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "frozen-bubble-replay-recorder-test";
    std::filesystem::remove_all(scratch, ec);
    std::filesystem::create_directories(scratch, ec);
    GameSettings *settings = GameSettings::Instance();
    std::string prefPathStorage = scratch.string() + "/";
    settings->prefPath = prefPathStorage.c_str();
    settings->ReadSettings();

    const SetupSettings training = MakeTrainingSetup();
    const SetupSettings networkSetup = MakeNetworkSetup();

    // --- 1. local round: exactly one recording, decodes and replays hash-equal
    {
        ResetRecorder();
        singleBubbles.clear();
        malusBubbles.clear();

        std::unique_ptr<BubbleGame> game = NewLiveGame(renderer, 20260920u, training);
        CHECK(ReplayRecorder::Instance()->IsRecording());
        DriveTrainingToFinish(*game);
        CHECK(BubbleGameTestAccess::gameFinish(*game));
        CHECK(ReplayRecorder::Instance()->IsRecording() == false);

        CHECK(g_recordings.size() == 1);
        if (g_recordings.size() == 1) {
            DecodedRecording decoded;
            CHECK(DecodeRecording(g_recordings[0], decoded));
            CHECK(decoded.sawRoundEnd);
            CHECK(decoded.end.complete == 1);
            CHECK(decoded.end.outcome == kReplayOutcomeWin);
            CHECK(decoded.steps.size() > 0);
            CHECK(decoded.hashes.size() > 0);

            ReplayResult replay = ReplayRecording(renderer, decoded);
            CHECK(replay.gameFinish);
            CheckHashesEqual("local round", decoded.hashes, replay.hashes);
        }
        std::fprintf(stderr, "  [local] recordings=%zu\n", g_recordings.size());
    }

    // --- 2. ReloadGame() seals round 1 and opens round 2 ---------------------
    {
        ResetRecorder();
        singleBubbles.clear();
        malusBubbles.clear();
        SetupFakeNetworkClient();

        std::unique_ptr<BubbleGame> live = NewLiveGame(renderer, 6202u, networkSetup);

        // Round 1: a remote 'F' ends the round; ReloadGame() is the next round
        // boundary, which is what seals round 1's open result tail.
        NetworkClient::Instance()->QueueGameMessage("GAMEMSG:1:Fremote_nick");
        BubbleGameTestAccess::advance(*live, 1.0f, 1000);
        CHECK(BubbleGameTestAccess::gameFinish(*live));
        BubbleGameTestAccess::reload(*live, 1);

        // Round 2: finish it too (and let its 'S' arrive) so both recordings
        // reach the sink.
        NetworkClient::Instance()->QueueGameMessage("GAMEMSG:1:Fremote_nick");
        BubbleGameTestAccess::advance(*live, 1.0f, 2000);
        NetworkClient::Instance()->QueueGameMessage("GAMEMSG:1:S1:1:0:0:0:0");
        BubbleGameTestAccess::advance(*live, 1.0f, 2001);
        ResetFakeNetworkClient();

        CHECK(g_recordings.size() == 2);
        if (g_recordings.size() == 2) {
            DecodedRecording round1, round2;
            CHECK(DecodeRecording(g_recordings[0], round1));
            CHECK(DecodeRecording(g_recordings[1], round2));
            // Neither recording is truncated: each has at least one step and a
            // round-end record.
            CHECK(round1.steps.size() > 0 && round1.sawRoundEnd);
            CHECK(round2.steps.size() > 0 && round2.sawRoundEnd);
            // Round 2 carries the match forward: the remote seat's round-1 win
            // survives, the monotonic simStep keeps climbing, and the gameplay
            // RNG was not reseeded (ReloadGame leaves it alone by design).
            CHECK(round2.start.startingWins[1] == 1);
            CHECK(round2.start.initialSimStep > round1.start.initialSimStep);
            CHECK(round2.start.gameplayRngState != round1.start.gameplayRngState);
            std::fprintf(stderr,
                         "  [reload] rounds=%zu r1steps=%zu r2steps=%zu r2wins1=%d\n",
                         g_recordings.size(), round1.steps.size(), round2.steps.size(),
                         round2.start.startingWins[1]);
        }
    }

    // --- 3. keep count 0 records nothing; capture is simulation-neutral -----
    {
        ResetRecorder(/*keepCount=*/0);
        singleBubbles.clear();
        malusBubbles.clear();

        std::unique_ptr<BubbleGame> game = NewLiveGame(renderer, 777u, training);
        CHECK(ReplayRecorder::Instance()->IsRecording() == false);
        const std::vector<uint64_t> uncapturedHashes = DriveTrainingCollectingHashes(*game);
        CHECK(BubbleGameTestAccess::gameFinish(*game));
        // No sink call at all -- not a record-then-discard.
        CHECK(g_recordings.empty());

        // The hard constraint: recording must not alter the simulation. Re-run
        // the identical seeded round with capture enabled and require the same
        // canonical hash sequence at every step.
        ResetRecorder(/*keepCount=*/5);
        std::unique_ptr<BubbleGame> captured = NewLiveGame(renderer, 777u, training);
        CHECK(ReplayRecorder::Instance()->IsRecording());
        const std::vector<uint64_t> capturedHashes = DriveTrainingCollectingHashes(*captured);
        CheckHashesEqual("recording on/off", uncapturedHashes, capturedHashes);
        CHECK(g_recordings.size() == 1);
        std::fprintf(stderr, "  [keep0] recordings=0; on/off hashes=%zu match\n",
                     uncapturedHashes.size());
    }

    // --- 4. Playback records nothing ----------------------------------------
    {
        // Reuse scenario 1's recording as the playback source, then assert that
        // restoring and driving it produces no new sink call.
        ResetRecorder();
        singleBubbles.clear();
        malusBubbles.clear();

        std::unique_ptr<BubbleGame> game = NewLiveGame(renderer, 4242u, training);
        DriveTrainingToFinish(*game);
        CHECK(g_recordings.size() == 1);
        DecodedRecording decoded;
        CHECK(DecodeRecording(g_recordings[0], decoded));

        // From here on, drive a Playback instance restored from that recording.
        ResetRecorder();  // fresh sink; g_recordings empties
        ReplayResult replay = ReplayRecording(renderer, decoded);
        CHECK(replay.gameFinish);
        // RestoreRoundStart enters Playback before its NewGame(), so the
        // recorder's round-start hook declines and the whole replay runs with
        // no recording active.
        CHECK(g_recordings.empty());
        std::fprintf(stderr, "  [playback] recordings=%zu\n", g_recordings.size());
    }

    // --- 5a. network result tail: a late 'S' lands inside the recording -----
    {
        ResetRecorder();
        singleBubbles.clear();
        malusBubbles.clear();
        SetupFakeNetworkClient();

        std::unique_ptr<BubbleGame> live = NewLiveGame(renderer, 6403u, networkSetup);
        CHECK(ReplayRecorder::Instance()->IsRecording());

        // Step 0: remote announces the finish -- the local client's gameFinish
        // goes true, which opens the result tail rather than sealing.
        NetworkClient::Instance()->QueueGameMessage("GAMEMSG:1:Fremote_nick");
        BubbleGameTestAccess::advance(*live, 1.0f, 1000);
        CHECK(BubbleGameTestAccess::gameFinish(*live));
        CHECK(ReplayRecorder::Instance()->IsRecording());  // still watching

        // Step 1: the late stats sync arrives and closes the tail.
        NetworkClient::Instance()->QueueGameMessage("GAMEMSG:1:S3:2:1:0:0:0");
        BubbleGameTestAccess::advance(*live, 1.0f, 1001);
        CHECK(ReplayRecorder::Instance()->IsRecording() == false);
        ResetFakeNetworkClient();

        CHECK(g_recordings.size() == 1);
        if (g_recordings.size() == 1) {
            DecodedRecording decoded;
            CHECK(DecodeRecording(g_recordings[0], decoded));
            CHECK(decoded.sawRoundEnd);
            // Stats arrived, so the recording is marked complete.
            CHECK(decoded.end.complete == 1);
            ReplayResult replay = ReplayRecording(renderer, decoded);
            CheckHashesEqual("net late S", decoded.hashes, replay.hashes);
            // The late 'S' really is inside the recording: replaying it applies
            // the remote seat's stats.
            CHECK(replay.rFired.size() == 2);
            CHECK(replay.rFired[1] == 3);
            std::fprintf(stderr, "  [net-late-S] steps=%zu rFired1=%d\n",
                         decoded.steps.size(), replay.rFired.size() > 1 ? replay.rFired[1] : -1);
        }
    }

    // --- 5b. network result tail: timeout seals with missing stats marked ----
    {
        ResetRecorder();
        singleBubbles.clear();
        malusBubbles.clear();
        SetupFakeNetworkClient();

        std::unique_ptr<BubbleGame> live = NewLiveGame(renderer, 6404u, networkSetup);
        NetworkClient::Instance()->QueueGameMessage("GAMEMSG:1:Fremote_nick");
        BubbleGameTestAccess::advance(*live, 1.0f, 1000);   // finish at t=1000
        CHECK(ReplayRecorder::Instance()->IsRecording());

        // No 'S' ever arrives. The tail closes once the recorded clock is two
        // seconds past the finish.
        BubbleGameTestAccess::advance(*live, 1.0f, 1500);
        CHECK(ReplayRecorder::Instance()->IsRecording());
        BubbleGameTestAccess::advance(*live, 1.0f, 3001);   // 2001ms after finish
        CHECK(ReplayRecorder::Instance()->IsRecording() == false);
        ResetFakeNetworkClient();

        CHECK(g_recordings.size() == 1);
        if (g_recordings.size() == 1) {
            DecodedRecording decoded;
            CHECK(DecodeRecording(g_recordings[0], decoded));
            CHECK(decoded.sawRoundEnd);
            // Missing stats are explicit: complete=0, not a fabricated row.
            CHECK(decoded.end.complete == 0);
            std::fprintf(stderr, "  [net-timeout] steps=%zu complete=%d\n",
                         decoded.steps.size(), decoded.end.complete);
        }
    }

    // --- 6. transient budget overrun: incomplete, round plays on ------------
    {
        ResetRecorder(/*keepCount=*/5, /*budget=*/256);
        singleBubbles.clear();
        malusBubbles.clear();

        std::unique_ptr<BubbleGame> game = NewLiveGame(renderer, 5150u, training);
        CHECK(ReplayRecorder::Instance()->IsRecording());

        // The header + round start alone exceed 256 bytes, so the first step
        // trips the budget and seals the partial recording marked incomplete.
        const int stepBefore = BubbleGameTestAccess::simStep(*game);
        BubbleGameTestAccess::advance(*game, 1.0f, 1000);
        CHECK(ReplayRecorder::Instance()->IsRecording() == false);
        CHECK(g_recordings.size() == 1);
        if (g_recordings.size() == 1) {
            DecodedRecording decoded;
            CHECK(DecodeRecording(g_recordings[0], decoded));
            CHECK(decoded.sawRoundEnd);
            CHECK(decoded.end.complete == 0);
            CHECK(decoded.end.outcome == kReplayOutcomeIncomplete);
        }

        // The round keeps simulating normally without a recorder.
        CHECK(BubbleGameTestAccess::simStep(*game) > stepBefore);
        BubbleGameTestAccess::advance(*game, 1.0f, 121010);
        CHECK(BubbleGameTestAccess::gameFinish(*game));
        CHECK(g_recordings.size() == 1);  // still just the one sealed recording
        std::fprintf(stderr, "  [budget] recordings=%zu gameFinish=%d\n",
                     g_recordings.size(),
                     BubbleGameTestAccess::gameFinish(*game) ? 1 : 0);
    }

    // --- 7. header records this build's platform/float profile ----------------
    {
        ResetRecorder();
        singleBubbles.clear();
        malusBubbles.clear();

        std::unique_ptr<BubbleGame> game = NewLiveGame(renderer, 7307u, training);
        DriveTrainingToFinish(*game);
        CHECK(g_recordings.size() == 1);
        if (g_recordings.size() == 1) {
            DecodedRecording decoded;
            CHECK(DecodeRecording(g_recordings[0], decoded));
            CHECK(decoded.header.platformFloatProfile ==
                  ComputeCurrentPlatformFloatProfile());
            // The gate this profile feeds treats a same-build file as playable.
            CHECK(IsReplayPlatformCompatible(decoded.header.platformFloatProfile));
        }
        std::fprintf(stderr, "  [platform-profile] recordings=%zu\n", g_recordings.size());
    }

    ReplayRecorder::Instance()->Dispose();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    std::filesystem::remove_all(scratch, ec);

    if (failures == 0) std::printf("replay recorder tests passed\n");
    return failures == 0 ? 0 : 1;
}
