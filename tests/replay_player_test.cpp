// R4c: the replay playback viewer, end to end.
//
// R3/R5/R6 proved the capture/restore engine and R4a/R4b made recordings
// auto-save into an on-disk library, but nothing could play one back outside
// test code. ReplayPlayer (src/replay_player.{h,cpp}) is that viewer: it decodes
// a .fbr byte stream, restores the round into an owned BubbleGame, and consumes
// the recorded steps at 0.5x/1x/2x/4x while checking the recorded per-step
// canonical hash.
//
// Fixtures are real recordings: a real BubbleGame is driven with real
// controls/synthetic clocks through AdvanceSimulationAtScale(), its steps are
// captured with the production CaptureStep()/CaptureCanonicalStateHash() API,
// and the same EncodeRecording() shape tests/bubblegame_replay_test.cpp uses
// writes them out. Only the corruption/bad-input scenarios build bytes by hand
// on purpose.
//
// The internal-state assertions use a test-only friend struct, exactly as the
// pre-R4c test drivers did -- but the driving loop itself is no longer test
// code: it is ReplayPlayer's own AdvanceFrame().

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "audiomixer.h"
#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "bubblegame_replay.h"
#include "gamesettings.h"
#include "networkclient.h"
#include "platform.h"
#include "replay_format.h"
#include "replay_player.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
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
    static const BubbleArray &playerConst(const BubbleGame &game, int idx) {
        return game.bubbleArrays[idx];
    }
    static void seedRng(BubbleGame &game, uint32_t seed) {
        game.rng.Seed(seed);
        game.rngExplicitlySeeded = true;
    }
    static void advanceSimulationAtScale(BubbleGame &game, float deltaScale, Uint32 gameClockMs) {
        game.AdvanceSimulationAtScale(deltaScale, gameClockMs);
    }
};

// R4c only: the state ReplayPlayer keeps private, for assertions that the
// class's own public API does not expose (cursor position, group count, the
// owned game to recompute hashes).
#ifdef FROZEN_BUBBLE_TEST_ACCESS
struct ReplayPlayerTestAccess {
    static const BubbleGame *game(const ReplayPlayer &player) { return player.game_.get(); }
    static size_t cursor(const ReplayPlayer &player) { return player.cursor_; }
    static size_t groupCount(const ReplayPlayer &player) { return player.groups_.size(); }
};
#endif

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
// Recording fixtures (real captures)
// ---------------------------------------------------------------------------

struct StepSpec {
    float deltaScale = 1.0f;
    uint32_t clockMs = 0;
    bool aim = false;
    float angle = -1.0f;
    bool fire = false;
    std::vector<std::string> inboundWire;
};

struct Captured {
    RoundStartRecord start;
    int playerCount = 1;
    std::vector<StepRecord> steps;  // playerCount per group, group-major
    std::vector<uint64_t> hashes;   // one per group
    RoundEndRecord end;
};

static SetupSettings MakeSoloSetup() {
    SetupSettings s;
    s.playerCount = 1;
    s.networkGame = false;
    s.randomLevels = true;
    s.startLevel = 1;
    s.gameMode = GameMode::Classic;
    s.chainReaction = false;
    return s;
}

static SetupSettings MakeNetworkSetup() {
    SetupSettings s;
    s.playerCount = 2;
    s.networkGame = true;
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

// Give a network seat a full 13-row board of valid cell positions. NewGame()
// with randomLevels=false only fills seat 0 via LoadLevel(), so a remote seat's
// rows would otherwise be empty and the inbound 's' handler would have no cell
// to write a placed bubble into. Copied from
// tests/bubblegame_replay_test.cpp's R6a harness so the fixture is the exact
// proven shape.
static void ShapeNetworkBoard(BubbleGame &game, int seat, int bubbleSize = 32) {
    BubbleArray &p = BubbleGameTestAccess::player(game, seat);
    const int rowSize = bubbleSize * 7 / 8;
    for (int row = 0; row < static_cast<int>(p.bubbleMap.size()); ++row) {
        p.bubbleMap[row].clear();
        const int cells = (row % 2 == 0) ? 8 : 7;
        const int smallerSep = (row % 2 == 0) ? 0 : bubbleSize / 2;
        for (int col = 0; col < cells; ++col) {
            Bubble b;
            b.bubbleId = -1;
            b.pos = {smallerSep + bubbleSize * col + p.bubbleOffset.x,
                     rowSize * row + p.bubbleOffset.y};
            p.bubbleMap[row].push_back(b);
        }
    }
}

// Drive an already-initialized game through `script`, capturing every seat's
// StepRecord and the canonical hash once per step. The exact production capture
// API the recorder uses.
static void CaptureLive(BubbleGame &game, int playerCount,
                        const std::vector<StepSpec> &script, Captured &out) {
    out.playerCount = playerCount;
    out.start = CaptureRoundStart(game);
    for (const StepSpec &s : script) {
        BubbleArray &p0 = BubbleGameTestAccess::player(game, 0);
        p0.mouseTargetAngle = s.aim ? s.angle : -1.0f;
        p0.mouseFirePending = s.fire;
        if (!s.inboundWire.empty()) {
            NetworkClient *nc = NetworkClient::Instance();
            for (const std::string &wire : s.inboundWire) nc->QueueGameMessage(wire);
        }
        BubbleGameTestAccess::advanceSimulationAtScale(game, s.deltaScale, s.clockMs);
        for (int seat = 0; seat < playerCount; ++seat)
            out.steps.push_back(CaptureStep(game, seat));
        out.hashes.push_back(CaptureCanonicalStateHash(game));
    }
    out.end = CaptureRoundEnd(game);
}

// A solo Classic round with `steps` synthetic idle steps, clocks 0..steps-1.
// Synthetic clocks keep the recorded game clock deterministic and make the
// round's full span exactly `steps - 1` ms.
static Captured BuildSoloRecording(SDL_Renderer *renderer, int steps, uint32_t seed) {
    singleBubbles.clear();
    malusBubbles.clear();

    BubbleGame game(renderer);
    BubbleGameTestAccess::seedRng(game, seed);
    game.SetSessionMode(BubbleGame::SessionMode::Live);
    game.NewGame(MakeSoloSetup());

    std::vector<StepSpec> script;
    for (int i = 0; i < steps; ++i) {
        StepSpec s;
        s.deltaScale = 1.0f;
        s.clockMs = static_cast<uint32_t>(i);
        script.push_back(s);
    }

    Captured out;
    CaptureLive(game, 1, script, out);
    return out;
}

// A solo Classic round where `fire` is true on exactly the requested steps and
// false everywhere else, so the recording's fire control is a clean sequence of
// rising edges at those step indices. Aiming straight up on firing steps makes
// each one an actual launch (so the recording also exercises launch/stick SFX),
// but the fire field itself is captured from the resolved controls regardless.
static Captured BuildShotPatternRecording(SDL_Renderer *renderer, int steps,
                                          const std::vector<int> &fireSteps,
                                          uint32_t seed) {
    singleBubbles.clear();
    malusBubbles.clear();

    BubbleGame game(renderer);
    BubbleGameTestAccess::seedRng(game, seed);
    game.SetSessionMode(BubbleGame::SessionMode::Live);
    game.NewGame(MakeSoloSetup());

    std::vector<StepSpec> script;
    for (int i = 0; i < steps; ++i) {
        StepSpec s;
        s.deltaScale = 1.0f;
        s.clockMs = static_cast<uint32_t>(i);
        script.push_back(s);
    }
    for (const int i : fireSteps) {
        if (i < 0 || i >= steps) continue;
        script[i].aim = true;
        script[i].angle = PI / 2.0f;
        script[i].fire = true;
    }

    Captured out;
    CaptureLive(game, 1, script, out);
    return out;
}

static std::vector<uint8_t> EncodeRecording(const Captured &rec) {
    ReplayWriter writer;
    ReplayHeader header;
    const char fingerprint[] = "r4c-replay-player-test";
    header.buildFingerprint.assign(fingerprint, fingerprint + std::strlen(fingerprint));
    writer.WriteHeader(header);
    writer.WriteRoundStart(rec.start);
    for (size_t i = 0; i < rec.hashes.size(); ++i) {
        AssertionRecord assertion;
        assertion.simStep = rec.steps[i * rec.playerCount].simStep;
        assertion.seatId = 0;
        assertion.acceptedShotColor = 0;
        assertion.acceptedColumn = -1;
        assertion.acceptedRow = -1;
        assertion.canonicalStateHash = rec.hashes[i];
        writer.WriteAssertion(assertion);
        for (int seat = 0; seat < rec.playerCount; ++seat)
            writer.WriteStep(rec.steps[i * rec.playerCount + seat]);
    }
    writer.WriteRoundEnd(rec.end);
    return writer.Bytes();
}

// ---------------------------------------------------------------------------
// ReplayPlayer driving helpers
// ---------------------------------------------------------------------------

static std::vector<uint64_t> RunToCompletionCollectingHashes(ReplayPlayer &player, int maxCalls) {
    std::vector<uint64_t> hashes;
    int calls = 0;
    while (!player.IsFinished() && calls < maxCalls) {
        player.AdvanceFrame();
        ++calls;
        hashes.push_back(CaptureCanonicalStateHash(*ReplayPlayerTestAccess::game(player)));
    }
    return hashes;
}

static int CountCallsToFinish(ReplayPlayer &player, int maxCalls) {
    int calls = 0;
    while (!player.IsFinished() && calls < maxCalls) {
        player.AdvanceFrame();
        ++calls;
    }
    return calls;
}

// ---------------------------------------------------------------------------

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_AUDIODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();

    SDL_Window *window = SDL_CreateWindow("replay-player-test", 64, 64, SDL_WINDOW_HIDDEN);
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
        std::filesystem::temp_directory_path() / "frozen-bubble-replay-player-test";
    std::filesystem::remove_all(scratch, ec);
    std::filesystem::create_directories(scratch, ec);
    GameSettings *settings = GameSettings::Instance();
    std::string prefPathStorage = scratch.string() + "/";
    settings->prefPath = prefPathStorage.c_str();
    settings->ReadSettings();

    // --- 1. load a real recording and drive it to completion at 1x ---------
    {
        const int kSteps = 8;
        Captured rec = BuildSoloRecording(renderer, kSteps, 4242u);
        const std::vector<uint8_t> bytes = EncodeRecording(rec);

        ReplayPlayer player;
        CHECK(player.Load(bytes, renderer));
        CHECK(player.IsLoaded());
        CHECK(!player.IsFinished());
        CHECK(!player.IsPaused());
        CHECK(!player.IsDesynced());
        CHECK(!player.IsRecordingComplete());  // idle round never reached gameFinish
        CHECK(player.ElapsedMs() == 0);
        CHECK(player.TotalMs() == static_cast<uint32_t>(kSteps - 1));
        CHECK(ReplayPlayerTestAccess::groupCount(player) == static_cast<size_t>(kSteps));
        player.Draw();  // loaded Draw() must be safe headless

        std::vector<uint64_t> hashes =
            RunToCompletionCollectingHashes(player, 100);
        CHECK(hashes.size() == static_cast<size_t>(kSteps));
        CHECK(player.IsFinished());
        CHECK(!player.IsDesynced());
        CHECK(ReplayPlayerTestAccess::cursor(player) == static_cast<size_t>(kSteps));
        CHECK(player.ElapsedMs() == player.TotalMs());
        CHECK(player.ElapsedMs() == static_cast<uint32_t>(kSteps - 1));
        // Every step consumed exactly once and reproduced the live hash.
        for (size_t i = 0; i < hashes.size(); ++i) CHECK(hashes[i] == rec.hashes[i]);
        player.Draw();  // finished Draw() must still be safe
        std::fprintf(stderr, "  [load-1x] steps=%zu elapsed=%u total=%u\n",
                     hashes.size(), player.ElapsedMs(), player.TotalMs());
    }

    // --- 2. speed determinism: exact AdvanceFrame counts -------------------
    {
        Captured rec = BuildSoloRecording(renderer, 8, 4242u);
        const std::vector<uint8_t> bytes = EncodeRecording(rec);

        auto countAt = [&](float speed) {
            ReplayPlayer player;
            CHECK(player.Load(bytes, renderer));
            player.SetSpeed(speed);
            CHECK(player.Speed() == speed);
            return CountCallsToFinish(player, 1000);
        };
        const int at1x = countAt(1.0f);
        const int at2x = countAt(2.0f);
        const int atHalf = countAt(0.5f);
        const int at4x = countAt(4.0f);
        CHECK(at1x == 8);
        CHECK(at2x == 4);
        CHECK(atHalf == 16);
        CHECK(at4x == 2);
        std::fprintf(stderr, "  [speed] 1x=%d 2x=%d 0.5x=%d 4x=%d\n",
                     at1x, at2x, atHalf, at4x);
    }

    // --- 3. pause freezes progress; unpausing resumes identically ----------
    {
        Captured rec = BuildSoloRecording(renderer, 8, 4242u);
        const std::vector<uint8_t> bytes = EncodeRecording(rec);

        ReplayPlayer reference;
        CHECK(reference.Load(bytes, renderer));
        const std::vector<uint64_t> full =
            RunToCompletionCollectingHashes(reference, 100);
        CHECK(full.size() == 8);

        ReplayPlayer player;
        CHECK(player.Load(bytes, renderer));
        std::vector<uint64_t> partial = RunToCompletionCollectingHashes(player, 3);
        CHECK(partial.size() == 3);
        for (size_t i = 0; i < partial.size(); ++i) CHECK(partial[i] == full[i]);

        player.SetPaused(true);
        CHECK(player.IsPaused());
        const uint32_t elapsedBefore = player.ElapsedMs();
        const size_t cursorBefore = ReplayPlayerTestAccess::cursor(player);
        for (int i = 0; i < 5; ++i) player.AdvanceFrame();  // must be pure no-ops
        CHECK(player.ElapsedMs() == elapsedBefore);
        CHECK(ReplayPlayerTestAccess::cursor(player) == cursorBefore);
        player.Draw();  // paused HUD path

        player.SetPaused(false);
        std::vector<uint64_t> rest;
        while (!player.IsFinished()) {
            player.AdvanceFrame();
            rest.push_back(CaptureCanonicalStateHash(*ReplayPlayerTestAccess::game(player)));
        }
        std::vector<uint64_t> combined = partial;
        combined.insert(combined.end(), rest.begin(), rest.end());
        CHECK(combined.size() == full.size());
        for (size_t i = 0; i < full.size(); ++i) CHECK(combined[i] == full[i]);
        CHECK(!player.IsDesynced());
        std::fprintf(stderr, "  [pause] pausedAt=%u resumed=%zu/%zu\n",
                     elapsedBefore, combined.size(), full.size());
    }

    // --- 4. Restart rebuilds from scratch ---------------------------------
    {
        Captured rec = BuildSoloRecording(renderer, 8, 4242u);
        const std::vector<uint8_t> bytes = EncodeRecording(rec);

        ReplayPlayer fresh;
        CHECK(fresh.Load(bytes, renderer));
        const std::vector<uint64_t> full = RunToCompletionCollectingHashes(fresh, 100);
        CHECK(full.size() == 8);
        const uint32_t total = fresh.TotalMs();

        ReplayPlayer player;
        CHECK(player.Load(bytes, renderer));
        RunToCompletionCollectingHashes(player, 3);
        CHECK(!player.IsFinished());
        player.SetPaused(true);

        player.Restart();
        CHECK(!player.IsPaused());
        CHECK(!player.IsDesynced());
        CHECK(player.ElapsedMs() == 0);
        CHECK(!player.IsFinished());
        CHECK(ReplayPlayerTestAccess::cursor(player) == 0);

        const std::vector<uint64_t> restarted = RunToCompletionCollectingHashes(player, 100);
        CHECK(restarted.size() == full.size());
        for (size_t i = 0; i < full.size(); ++i) CHECK(restarted[i] == full[i]);
        CHECK(player.ElapsedMs() == total);
        CHECK(!player.IsDesynced());
        std::fprintf(stderr, "  [restart] steps=%zu elapsed=%u total=%u\n",
                     restarted.size(), player.ElapsedMs(), player.TotalMs());
    }

    // --- 5. bad input loads fail safe -------------------------------------
    {
        Captured rec = BuildSoloRecording(renderer, 4, 99u);
        const std::vector<uint8_t> good = EncodeRecording(rec);

        // (a) wrong magic.
        {
            std::vector<uint8_t> bad = good;
            bad[0] ^= 0xFF;
            ReplayPlayer player;
            CHECK(!player.Load(bad, renderer));
            CHECK(!player.IsLoaded());
            CHECK(!player.IsFinished());
            CHECK(player.ElapsedMs() == 0 && player.TotalMs() == 0);
            player.AdvanceFrame();
            player.Draw();
            CHECK(player.ElapsedMs() == 0);
            // A failed load must not poison a later valid one.
            CHECK(player.Load(good, renderer));
            CHECK(player.IsLoaded());
        }
        // (b) truncated partway through the round-start record.
        {
            ReplayReader reader(good);
            ReplayHeader header;
            CHECK(reader.ReadHeader(header) == DecodeResult::Ok);
            const size_t headerEnd = good.size() - reader.Remaining();
            std::vector<uint8_t> bad(good.begin(), good.begin() + headerEnd + 8);
            ReplayPlayer player;
            CHECK(!player.Load(bad, renderer));
            CHECK(!player.IsLoaded());
            player.AdvanceFrame();
            player.Draw();
            CHECK(player.ElapsedMs() == 0);
        }
        // (c) a record claiming more than the codec's maximum payload length.
        {
            ReplayWriter writer;
            ReplayHeader header;
            writer.WriteHeader(header);
            std::vector<uint8_t> bad = writer.Bytes();
            bad.push_back(static_cast<uint8_t>(RecordType::RoundStart));
            uint32_t length = kMaxPayloadLength + 1;
            bad.push_back(static_cast<uint8_t>(length & 0xFF));
            bad.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
            bad.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
            bad.push_back(static_cast<uint8_t>((length >> 24) & 0xFF));
            ReplayPlayer player;
            CHECK(!player.Load(bad, renderer));
            CHECK(!player.IsLoaded());
            player.AdvanceFrame();
            player.Draw();
            CHECK(player.ElapsedMs() == 0);
        }
        // (d) a step naming a seat past the round's playerCount.
        {
            Captured tampered = rec;
            tampered.steps[1].seatId = 5;
            ReplayPlayer player;
            CHECK(!player.Load(EncodeRecording(tampered), renderer));
            CHECK(!player.IsLoaded());
        }
        std::fprintf(stderr, "  [bad-input] all four rejected safely\n");
    }

    // --- 6. a tampered control fails visibly at the right step -------------
    {
        Captured rec = BuildSoloRecording(renderer, 8, 4242u);
        // Corrupt group 3's seat-0 controls: request a shot the live run never
        // made. The recorded assertion for that step is left untouched, so
        // playback must diverge exactly there.
        const size_t idx = 3;  // playerCount == 1, so group index == record index
        CHECK(rec.steps[idx].fire == 0);
        rec.steps[idx].fire = 1;
        const std::vector<uint8_t> tampered = EncodeRecording(rec);

        ReplayPlayer player;
        CHECK(player.Load(tampered, renderer));
        CHECK(!player.IsDesynced());
        int calls = 0;
        while (!player.IsFinished() && !player.IsDesynced() && calls < 100) {
            player.AdvanceFrame();
            ++calls;
        }
        CHECK(player.IsDesynced());
        CHECK(!player.IsFinished());
        CHECK(calls == 4);  // groups 0-2 clean, group 3 mismatched and stopped
        CHECK(ReplayPlayerTestAccess::cursor(player) == 4);
        const uint32_t elapsedAtDesync = player.ElapsedMs();
        CHECK(elapsedAtDesync == 3);
        for (int i = 0; i < 5; ++i) player.AdvanceFrame();  // must stay stopped
        CHECK(player.ElapsedMs() == elapsedAtDesync);
        CHECK(ReplayPlayerTestAccess::cursor(player) == 4);
        player.Draw();  // desynced state must still draw without crashing
        std::fprintf(stderr, "  [desync] detected at cursor=%zu elapsed=%u\n",
                     ReplayPlayerTestAccess::cursor(player), elapsedAtDesync);
    }

    // --- 7. a 2-seat network recording, inbound events included ------------
    {
        singleBubbles.clear();
        malusBubbles.clear();
        SetupFakeNetworkClient();

        BubbleGame game(renderer);
        BubbleGameTestAccess::seedRng(game, 7777u);
        game.SetSessionMode(BubbleGame::SessionMode::Live);
        game.NewGame(MakeNetworkSetup());
        ShapeNetworkBoard(game, 0);
        ShapeNetworkBoard(game, 1);

        // Seat 0 fires straight up; at step 5 the remote peer fires and at
        // step 10 the server delivers the remote stick. Both ride the real
        // NetworkClient queue, so the live run drains them through the real
        // ProcessNetworkMessages() dispatch -- and CaptureStep() records them
        // on seat 0's StepRecord for playback to apply at the same step.
        std::vector<StepSpec> script;
        for (int i = 0; i < 40; ++i) {
            StepSpec s;
            s.deltaScale = 1.0f;
            s.clockMs = static_cast<uint32_t>(i);
            script.push_back(s);
        }
        script[0].aim = true;
        script[0].angle = PI / 2.0f;
        script[0].fire = true;
        script[5].inboundWire.push_back("GAMEMSG:1:f1.571:3");
        script[10].inboundWire.push_back("GAMEMSG:1:s2:0:5:1 2 3 4 5 6 7 0");

        Captured rec;
        CaptureLive(game, 2, script, rec);
        ResetFakeNetworkClient();

        const std::vector<uint8_t> bytes = EncodeRecording(rec);
        ReplayPlayer player;
        CHECK(player.Load(bytes, renderer));
        const std::vector<uint64_t> hashes = RunToCompletionCollectingHashes(player, 1000);
        CHECK(player.IsFinished());
        CHECK(!player.IsDesynced());
        CHECK(hashes.size() == rec.hashes.size());
        for (size_t i = 0; i < hashes.size(); ++i) CHECK(hashes[i] == rec.hashes[i]);

        // The remote 's' landed a bubble on seat 1, which only happens if the
        // inbound event was applied on the correct playback step.
        const BubbleGame *playback = ReplayPlayerTestAccess::game(player);
        int remoteBubbles = 0;
        for (const auto &row : BubbleGameTestAccess::playerConst(*playback, 1).bubbleMap)
            for (const Bubble &b : row)
                if (b.bubbleId != -1) ++remoteBubbles;
        CHECK(remoteBubbles >= 1);
        std::fprintf(stderr, "  [network] steps=%zu remoteBubbles=%d\n",
                     hashes.size(), remoteBubbles);
    }

    // --- 8. forward seek correctness ---------------------------------------
    {
        const int kSteps = 8;
        Captured rec = BuildSoloRecording(renderer, kSteps, 4242u);
        const std::vector<uint8_t> bytes = EncodeRecording(rec);

        ReplayPlayer player;
        CHECK(player.Load(bytes, renderer));
        const size_t target = 4;
        player.SeekToStepIndex(target);

        CHECK(ReplayPlayerTestAccess::cursor(player) == target);
        CHECK(!player.IsDesynced());
        CHECK(player.IsPaused());  // a seek always lands paused
        // Consuming groups 0..target-1 must reproduce the recorded assertion
        // hash for step target-1. rec.hashes[i] is the live capture's hash
        // after the step whose record is rec.steps[i] (simStep i+1 here), so
        // the hash for the last consumed step is rec.hashes[target - 1].
        CHECK(CaptureCanonicalStateHash(*ReplayPlayerTestAccess::game(player)) ==
              rec.hashes[target - 1]);
        std::fprintf(stderr, "  [seek-fwd] target=%zu hash-match\n", target);
    }

    // --- 9. backward seek correctness ---------------------------------------
    {
        const int kSteps = 8;
        Captured rec = BuildSoloRecording(renderer, kSteps, 4242u);
        const std::vector<uint8_t> bytes = EncodeRecording(rec);

        ReplayPlayer player;
        CHECK(player.Load(bytes, renderer));
        for (int i = 0; i < 6; ++i) player.AdvanceFrame();
        CHECK(ReplayPlayerTestAccess::cursor(player) == 6);

        const size_t target = 3;
        player.SeekToStepIndex(target);
        CHECK(ReplayPlayerTestAccess::cursor(player) == target);
        CHECK(!player.IsDesynced());
        CHECK(player.IsPaused());

        // The seeked-back state must equal both the live capture's hash for
        // that step and a second, freshly-loaded player driven straight there.
        ReplayPlayer reference;
        CHECK(reference.Load(bytes, renderer));
        for (size_t i = 0; i < target; ++i) reference.AdvanceFrame();
        CHECK(CaptureCanonicalStateHash(*ReplayPlayerTestAccess::game(player)) ==
              CaptureCanonicalStateHash(*ReplayPlayerTestAccess::game(reference)));
        CHECK(CaptureCanonicalStateHash(*ReplayPlayerTestAccess::game(player)) ==
              rec.hashes[target - 1]);
        std::fprintf(stderr, "  [seek-back] target=%zu hash-match\n", target);
    }

    // --- 10. shot navigation -------------------------------------------------
    {
        Captured rec = BuildShotPatternRecording(renderer, 16, {2, 5, 9}, 5555u);
        const std::vector<uint8_t> bytes = EncodeRecording(rec);

        ReplayPlayer player;
        CHECK(player.Load(bytes, renderer));
        CHECK(player.HasShots());

        player.SeekToNextShot();
        CHECK(ReplayPlayerTestAccess::cursor(player) == 2);
        CHECK(player.IsPaused());
        player.SeekToNextShot();
        CHECK(ReplayPlayerTestAccess::cursor(player) == 5);
        player.SeekToNextShot();
        CHECK(ReplayPlayerTestAccess::cursor(player) == 9);
        const size_t atLast = ReplayPlayerTestAccess::cursor(player);
        player.SeekToNextShot();  // past the last shot: complete no-op
        CHECK(ReplayPlayerTestAccess::cursor(player) == atLast);

        player.SeekToPreviousShot();
        CHECK(ReplayPlayerTestAccess::cursor(player) == 5);
        player.SeekToPreviousShot();
        CHECK(ReplayPlayerTestAccess::cursor(player) == 2);
        const size_t atFirst = ReplayPlayerTestAccess::cursor(player);
        player.SeekToPreviousShot();  // before the first shot: complete no-op
        CHECK(ReplayPlayerTestAccess::cursor(player) == atFirst);
        CHECK(!player.IsDesynced());

        // Zero-shot fixture: fire is never true, so shot navigation is inert.
        Captured idle = BuildSoloRecording(renderer, 6, 1234u);
        const std::vector<uint8_t> idleBytes = EncodeRecording(idle);
        ReplayPlayer noShots;
        CHECK(noShots.Load(idleBytes, renderer));
        CHECK(!noShots.HasShots());
        const size_t before = ReplayPlayerTestAccess::cursor(noShots);
        const uint64_t hashBefore =
            CaptureCanonicalStateHash(*ReplayPlayerTestAccess::game(noShots));
        noShots.SeekToNextShot();
        noShots.SeekToPreviousShot();
        CHECK(ReplayPlayerTestAccess::cursor(noShots) == before);
        CHECK(CaptureCanonicalStateHash(*ReplayPlayerTestAccess::game(noShots)) ==
              hashBefore);
        CHECK(!noShots.IsPaused());
        std::fprintf(stderr, "  [shots] next/prev walk and zero-shot no-op\n");
    }

    // --- 11. seek pauses and mutes audio; restores mute state ----------------
    {
        Captured rec = BuildShotPatternRecording(renderer, 40, {2}, 7777u);
        const std::vector<uint8_t> bytes = EncodeRecording(rec);

        AudioMixer *mixer = AudioMixer::Instance();
        mixer->MuteAll(true);  // force a known unmuted starting state
        CHECK(!mixer->IsHalted());

        AudioMixer::TestResetSfxPlayCount();
        ReplayPlayer player;
        CHECK(player.Load(bytes, renderer));
        player.SeekToStepIndex(20);  // span includes the launch and the stick

        CHECK(player.IsPaused());
        CHECK(!player.IsDesynced());
        CHECK(ReplayPlayerTestAccess::cursor(player) == 20);
        CHECK(AudioMixer::TestSfxPlayCount() == 0);  // catch-up ran muted
        CHECK(!mixer->IsHalted());                   // unmuted again after

        // Control: the same span played normally (unmuted) must fire SFX, so
        // the zero above is a real assertion rather than a silent fixture.
        AudioMixer::TestResetSfxPlayCount();
        ReplayPlayer control;
        CHECK(control.Load(bytes, renderer));
        for (size_t i = 0; i < 20; ++i) control.AdvanceFrame();
        CHECK(AudioMixer::TestSfxPlayCount() != 0);
        std::fprintf(stderr, "  [seek-mute] mutedSeek=0 control=%d\n",
                     AudioMixer::TestSfxPlayCount());
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    std::filesystem::remove_all(scratch, ec);

    if (failures == 0) std::printf("replay player tests passed\n");
    return failures == 0 ? 0 : 1;
}
