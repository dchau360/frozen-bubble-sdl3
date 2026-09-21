// R3/R5a/R5b: local rounds recorded live and replayed offline.
//
// The live path is a real BubbleGame driven through AdvanceSimulation() with
// scripted input and deliberately varied deltaScale. The replay path is a
// fresh BubbleGame restored from the captured RoundStartRecord and driven only
// by AdvancePlaybackStep() + each seat's captured lastControls. After every
// step both runs compute CaptureCanonicalStateHash() over the same serialized
// state and the test asserts the sequences match exactly -- "compare at every
// step", not just at the end.
//
// R3 covered one solo Classic round (win/loss/hurry/wall-hits/0.5x-2x/disk).
// R5a widened the same harness to the step-driven local rules: 2-5 seats, bots
// (captured controls, never re-running the AI), chain reactions, attack modes,
// teams, Race mode, predefined levels (randomLevels=false) and a mid-match
// ReloadGame transition.
// R5b adds the recorded-game-clock seam: training (mpTraining) and Timed mode,
// whose end conditions are driven by a synthetic per-step gameClockMs so they
// expire in a handful of steps rather than real time.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "bubblegame_replay.h"
#include "frozenbubble.h"
#include "gamesettings.h"
#include "localmultiplayer_settings.h"
#include "networkclient.h"
#include "platform.h"
#include "replay_format.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
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
    static BubbleArray& player(BubbleGame& game, int idx) { return game.bubbleArrays[idx]; }
    static void seedRng(BubbleGame& game, uint32_t seed) {
        game.rng.Seed(seed);
        game.rngExplicitlySeeded = true;
    }
    static void seedBots(BubbleGame& game, int players, unsigned seed) {
        for (int p = 0; p < players; ++p) {
            game.bubbleArrays[p].botRng = seed + static_cast<unsigned>(p) * 7919u;
            game.bubbleArrays[p].botTargetAngle = -1.0f;
            game.bubbleArrays[p].botThinkFrames = 0;
        }
    }
    static unsigned botRng(BubbleGame& game, int idx) { return game.bubbleArrays[idx].botRng; }
    static void setSessionMode(BubbleGame& game, BubbleGame::SessionMode mode) {
        game.sessionMode = mode;
    }
    static BubbleGame::SessionMode sessionMode(BubbleGame& game) { return game.sessionMode; }
    static bool pendingHighscore(BubbleGame& game) { return game.pendingHighscore; }
    static bool gameFinish(BubbleGame& game) { return game.gameFinish; }
    static bool gameWon(BubbleGame& game) { return game.gameWon; }
    static bool gameLost(BubbleGame& game) { return game.gameLost; }
    static int roundWinnerIdx(BubbleGame& game) { return game.roundWinnerIdx; }
    static void reloadGame(BubbleGame& game, int level) { game.ReloadGame(level); }
    // R5b: drive one step with a synthetic round-relative clock instead of the
    // production AdvanceSimulation()'s live SDL_GetTicks(). This is exactly
    // what the playback side already does via the public AdvancePlaybackStep
    // overload; using the shared private body keeps the live capture as close
    // to production as possible.
    static void advanceSimulationAtScale(BubbleGame& game, float deltaScale, Uint32 gameClockMs) {
        game.AdvanceSimulationAtScale(deltaScale, gameClockMs);
    }
    static int mpTrainScore(BubbleGame& game) { return game.mpTrainScore; }
    static bool modeTimerExpired(BubbleGame& game) { return game.modeTimerExpired; }
    // R6c: how many seats are still connected. Decremented by
    // HandlePlayerDeparture() when an 'l' arrives; the departure scenario
    // checks it round-trips through restore+replay (it is not part of the
    // canonical hash).
    static int connectedPlayerCount(BubbleGame& game) { return game.connectedPlayerCount; }
    // R6b: playback never calls AdoptBots()/SeatBots(), so a restored network
    // round must have no bot connections (seated or pending) at all.
    static bool botConnectionsEmpty(BubbleGame& game) {
        return game.botConnections.empty() && game.pendingBots.empty();
    }
};

// R6a: minimal hooks onto the real NetworkClient singleton so a headless test
// can stand up a fake "already in game" room with no socket. Every send
// already no-ops once sockfd is unset (always true headlessly), so setting
// these fields plus QueueGameMessage() (already public) is enough to drive the
// live ProcessNetworkMessages() dispatch exactly as a real connection would.
struct NetworkClientTestAccess {
    static void SetPlayerNick(NetworkClient& nc, const std::string& nick) { nc.playerNick = nick; }
    static void SetCurrentGame(NetworkClient& nc, GameRoom* game) { nc.currentGame = game; }
    static void SetState(NetworkClient& nc, ConnectionState state) { nc.state = state; }
    static void SetMyPlayerId(NetworkClient& nc, int id) { nc.myPlayerId = (unsigned char)id; }
    static void SetPlayerIdToNick(NetworkClient& nc, const std::map<int, std::string>& m) {
        nc.playerIdToNick = m;
    }
};

// ---------------------------------------------------------------------------
// Scripted input
// ---------------------------------------------------------------------------

struct StepScript {
    float deltaScale = 1.0f;
    bool aim = false;       // set a mouse aim angle this step (seat 0 only)
    float angle = -1.0f;
    bool fire = false;      // request a mouse/touch fire this step
    // R5b: when true the live driver calls AdvanceSimulationAtScale() with
    // this synthetic round-relative gameClockMs instead of AdvanceSimulation(),
    // so training's 120s timer / Timed's countdown can be jumped past without
    // 120000 real steps. Every other script leaves this false and keeps
    // driving the production AdvanceSimulation() live path.
    bool syntheticClock = false;
    Uint32 gameClockMs = 0;
    // R6a: wire messages ("GAMEMSG:{senderId}:{data}") placed on
    // NetworkClient's queue immediately before this step's AdvanceSimulation(),
    // so the live run drains them through the real ProcessNetworkMessages()
    // dispatch. Empty for every local script.
    std::vector<std::string> inboundWire;
};

struct CapturedRecording {
    RoundStartRecord start;
    int playerCount = 1;
    std::vector<StepRecord> steps;                 // playerCount records per simStep
    std::vector<uint64_t> liveHashes;              // one per simStep
    std::vector<std::vector<SDL_Point>> startPositions;  // per seat, row-major
    RoundEndRecord end;
    bool livePendingHighscore = false;
    int liveNetworkSends = 0;
    bool liveGameWon = false;
    bool liveGameLost = false;
    bool liveSawChain = false;
    int liveMpTrainScore = 0;
    bool liveModeTimerExpired = false;
    std::vector<int> liveRFired;
    std::vector<int> liveRPopped;
    std::vector<int> liveRSent;
    std::vector<int> liveRRecv;
    std::vector<int> liveRBlk;
    // R6c: index in liveHashes of the first step on which this client's own
    // gameFinish became true (-1 if it never did). The result-tail scenario
    // uses this to prove CaptureLiveSteps() really ran past that point.
    int liveFirstFinishStep = -1;
    // R6c: connected seats at the end of capture, so a departure scenario can
    // compare it against the replay's value.
    int liveConnectedPlayerCount = 0;
    // R6c: per-seat lifecycle (PlayerState) at the end of capture, so a
    // departure scenario can assert the departed seat really read LEFT without
    // relying on the hash alone.
    std::vector<int> livePlayerStates;
    // R6b: per-seat starting cell contents (row-major bubbleId) at capture
    // time, so a network scenario can prove two seats were built from one
    // SyncNetworkLevel() layout rather than each from its own path.
    std::vector<std::vector<int>> startBubbleIds;
    // R6a: live non-empty bubble count per seat at round end, so a network
    // scenario can assert a remote 's' actually placed a bubble on the remote
    // board (rather than only that the hashes round-tripped).
    std::vector<int> liveSeatBubbleCounts;
};

static SetupSettings MakeSoloClassicSetup(bool randomLevels = true, int startLevel = 1) {
    SetupSettings s;
    s.playerCount = 1;
    s.networkGame = false;
    s.randomLevels = randomLevels;
    s.startLevel = startLevel;
    s.gameMode = GameMode::Classic;
    s.chainReaction = false;
    return s;
}

static void ClearBoard(BubbleArray& p) {
    for (auto& row : p.bubbleMap) {
        for (Bubble& b : row) {
            b.bubbleId = -1;
            b.playerBubble = false;
            b.frozen = false;
            b.shining = false;
        }
    }
}

static void PutBubble(BubbleArray& p, int row, int col, int color) {
    if (row < 0 || row >= (int)p.bubbleMap.size()) return;
    if (col < 0 || col >= (int)p.bubbleMap[row].size()) return;
    p.bubbleMap[row][col].bubbleId = color;
    p.bubbleMap[row][col].playerBubble = true;
    p.bubbleMap[row][col].frozen = false;
}

static std::vector<SDL_Point> CollectPositions(BubbleGame& game, int seat) {
    std::vector<SDL_Point> out;
    BubbleArray& p = BubbleGameTestAccess::player(game, seat);
    for (const auto& row : p.bubbleMap)
        for (const Bubble& b : row)
            out.push_back(b.pos);
    return out;
}

// R6b: row-major bubbleId contents of one seat's board (including -1 empties),
// used to compare two seats' starting layouts by content rather than position.
static std::vector<int> CollectBubbleIds(BubbleGame& game, int seat) {
    std::vector<int> out;
    BubbleArray& p = BubbleGameTestAccess::player(game, seat);
    for (const auto& row : p.bubbleMap)
        for (const Bubble& b : row)
            out.push_back(b.bubbleId);
    return out;
}

// A tiny board whose last shot clears it: two same-color bubbles side by side
// in the top row, with the launcher holding a third of the same color. The
// fired bubble lands as the third member of the group, CheckPossibleDestroy
// pops all three, and CheckGameState sees allClear() -> gameWon.
static void SetupWinBoard(BubbleGame& game) {
    BubbleArray& p = BubbleGameTestAccess::player(game, 0);
    ClearBoard(p);
    PutBubble(p, 0, 3, 5);
    PutBubble(p, 0, 4, 5);
    p.curLaunch = 5;
    p.nextBubble = 5;
    p.nextColors.assign(8, 5);
}

// A board already past the danger line. The first shot (a steep wall-bounce
// that ends on the ceiling) forces CheckGameState, which sees bubbleOnDanger()
// and takes the ApplyPlayerLoss path.
static void SetupLossBoard(BubbleGame& game) {
    BubbleArray& p = BubbleGameTestAccess::player(game, 0);
    ClearBoard(p);
    PutBubble(p, 12, 3, 1);   // dangerZone == 12 for solo
    p.curLaunch = 2;
    p.nextBubble = 2;
    p.nextColors.assign(8, 2);
}

// Leave the random board in place and park the hurry timer one frame below its
// max, so the next step is the one ResolvePlayerControls() force-fires on.
static void SetupHurryBoard(BubbleGame& game) {
    BubbleArray& p = BubbleGameTestAccess::player(game, 0);
    p.hurryTimer = TIME_HURRY_MAX_MP - 1;  // randomLevels uses the MP timers
    p.warnTimer = 0;
}

// A crafted seat-0 board for the malus/race/team/chain tests. The whole top
// row is the launcher's colour, so a straight-up shot is guaranteed to land
// next to a same-colour group and pop it (the whole row goes). With
// withChainPair, a separate colour-1 group sits in the top row with a
// detached colour-1 bubble below it: the pop cuts that bubble loose and
// AssignChainReactions() flies it up into the group.
static void SetupAttackBoard(BubbleGame& game, bool withChainPair) {
    BubbleArray& p = BubbleGameTestAccess::player(game, 0);
    ClearBoard(p);
    for (int c = 0; c < (int)p.bubbleMap[0].size(); ++c)
        PutBubble(p, 0, c, 5);
    if (withChainPair) {
        PutBubble(p, 0, 0, 1);
        PutBubble(p, 0, 1, 1);
        PutBubble(p, 2, 0, 1);  // detached, same colour as the (0,0)/(0,1) group
    }
    p.curLaunch = 5;
    p.nextBubble = 5;
    p.nextColors.assign(8, 5);
}

// Training: a color-5 group (four bubbles, so the pop earns malus under the
// destroyed+falling-2 formula) that the straight-up shot joins, plus a
// ceiling-attached color-1 bubble so the board is NOT cleared. A clear would
// end the round via the solo win path, before training's two-minute clock
// could; this board leaves the round running to its recorded timer expiry and
// makes mpTrainScore nonzero.
static void SetupTrainingBoard(BubbleGame& game) {
    BubbleArray& p = BubbleGameTestAccess::player(game, 0);
    ClearBoard(p);
    PutBubble(p, 0, 0, 1);
    PutBubble(p, 0, 3, 5);
    PutBubble(p, 0, 4, 5);
    PutBubble(p, 0, 5, 5);
    PutBubble(p, 0, 6, 5);
    p.curLaunch = 5;
    p.nextBubble = 5;
    p.nextColors.assign(8, 5);
}

// R5a/R5b: shape a full 13-row board for a network seat with valid cell
// positions. NewGame(randomLevels=false) only fills seat 0 via LoadLevel(), so
// a remote seat would otherwise be empty; the recorded board blob stores each
// row's actual horizontal offset, and cell positions must be consistent with
// it for capture/restore to round-trip. Cell contents are cleared (bubbleId
// -1); PutBubble overlays whatever bubbles a scenario needs.
// bubbleSize defaults to 32 (a full-size board, correct for seat 0 and for a
// 2-seat room's seat 1); a battle-royale room (playerCount > 5) uses 16-px
// mini boards for every seat past 0 -- matching EncodeBoardBlob()'s isMini
// rule (playerCount >= 3 && playerAssigned >= 1) and ApplyMiniSlotGeometry()'s
// 128-px-wide slots (8 columns of 16). rowSize = bubbleSize * 7 / 8 scales for
// either value.
static void ShapeNetworkBoard(BubbleGame& game, int seat, int bubbleSize = 32) {
    BubbleArray& p = BubbleGameTestAccess::player(game, seat);
    const int rowSize = bubbleSize * 7 / 8;
    for (int row = 0; row < (int)p.bubbleMap.size(); ++row) {
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

static SetupSettings MakeNetworkSetup() {
    SetupSettings s;
    s.playerCount = 2;
    s.networkGame = true;
    // Avoid NewGame()'s SyncNetworkLevel() path: playback has no server to
    // sync with, and RestoreRoundStart() forces this false for a network
    // record anyway. The recorded board blobs are the source of truth.
    s.randomLevels = false;
    s.startLevel = 1;
    s.gameMode = GameMode::Classic;
    s.chainReaction = false;
    return s;
}

static std::vector<StepScript> FireThenCoast(float fireAngle) {
    std::vector<StepScript> script;
    StepScript first;
    first.deltaScale = 1.0f;
    first.aim = true;
    first.angle = fireAngle;
    first.fire = true;
    script.push_back(first);
    // 0.5x / 2x / 1x scheduled by the recorded step's own deltaScale value.
    for (int i = 1; i < 400; ++i) {
        StepScript s;
        const int m = i % 4;
        s.deltaScale = (m == 0) ? 0.5f : (m == 2 ? 2.0f : 1.0f);
        script.push_back(s);
    }
    return script;
}

static std::vector<StepScript> FireThenIdle(float fireAngle, int idleSteps) {
    std::vector<StepScript> script;
    StepScript first;
    first.deltaScale = 1.0f;
    first.aim = true;
    first.angle = fireAngle;
    first.fire = true;
    script.push_back(first);
    for (int i = 1; i < idleSteps; ++i) {
        StepScript s;
        const int m = i % 4;
        s.deltaScale = (m == 0) ? 0.5f : (m == 2 ? 2.0f : 1.0f);
        script.push_back(s);
    }
    return script;
}

static std::vector<StepScript> IdleFor(int steps) {
    std::vector<StepScript> script;
    for (int i = 0; i < steps; ++i) {
        StepScript s;
        const int m = i % 4;
        s.deltaScale = (m == 0) ? 0.5f : (m == 2 ? 2.0f : 1.0f);
        script.push_back(s);
    }
    return script;
}

// R5b: a script whose live run is driven by AdvanceSimulationAtScale() with a
// synthetic round-relative clock, so training's two-minute timer / Timed's
// countdown can expire in a handful of steps. `stepMs` is added per step; the
// caller mutates the returned steps to set aim/fire and may append a step with
// a large jump to cross the threshold.
static std::vector<StepScript> SyntheticClockSteps(Uint32 startClockMs, Uint32 stepMs, int count) {
    std::vector<StepScript> script;
    for (int i = 0; i < count; ++i) {
        StepScript s;
        s.syntheticClock = true;
        s.gameClockMs = startClockMs + static_cast<Uint32>(i) * stepMs;
        const int m = i % 4;
        s.deltaScale = (m == 0) ? 0.5f : (m == 2 ? 2.0f : 1.0f);
        script.push_back(s);
    }
    return script;
}

// ---------------------------------------------------------------------------
// Live capture / replay drivers
// ---------------------------------------------------------------------------

// Drives an already-initialized game through `script`, capturing every seat's
// controls and the canonical hash once per step. Used by both the fresh-NewGame
// scenarios and the ReloadGame continuity check.
//
// R6c: `stopAtFinish` defaults to true, preserving the historical behavior of
// stopping the moment this client's own gameFinish becomes true. The
// result-tail scenario passes false so it can keep driving a small scripted
// tail of post-finish steps (where a late remote 'S'/'P' still arrives); every
// existing caller keeps the default and captures exactly what it did before.
static void CaptureLiveSteps(BubbleGame& game, int playerCount,
                             const std::vector<StepScript>& script,
                             CapturedRecording& rec,
                             bool stopAtFinish = true) {
    rec.playerCount = playerCount;
    rec.start = CaptureRoundStart(game);
    rec.startPositions.clear();
    rec.startBubbleIds.clear();
    for (int seat = 0; seat < playerCount; ++seat) {
        rec.startPositions.push_back(CollectPositions(game, seat));
        rec.startBubbleIds.push_back(CollectBubbleIds(game, seat));
    }

    for (const StepScript& s : script) {
        FrozenBubble::Instance()->deltaScale = s.deltaScale;
        BubbleArray& p0 = BubbleGameTestAccess::player(game, 0);
        p0.mouseTargetAngle = s.aim ? s.angle : -1.0f;
        p0.mouseFirePending = s.fire;
        // R6a: inject this step's simulated remote payloads through the real
        // client queue, so ProcessNetworkMessages() handles them live.
        if (!s.inboundWire.empty()) {
            NetworkClient* nc = NetworkClient::Instance();
            for (const std::string& wire : s.inboundWire) nc->QueueGameMessage(wire);
        }
        if (s.syntheticClock)
            BubbleGameTestAccess::advanceSimulationAtScale(game, s.deltaScale, s.gameClockMs);
        else
            game.AdvanceSimulation();
        for (int seat = 0; seat < playerCount; ++seat)
            rec.steps.push_back(CaptureStep(game, seat));
        rec.liveHashes.push_back(CaptureCanonicalStateHash(game));
        for (const SingleBubble& sb : singleBubbles)
            if (sb.chainExists) rec.liveSawChain = true;
        if (BubbleGameTestAccess::gameFinish(game)) {
            if (rec.liveFirstFinishStep < 0)
                rec.liveFirstFinishStep = static_cast<int>(rec.liveHashes.size()) - 1;
            if (stopAtFinish) break;
        }
    }

    rec.end = CaptureRoundEnd(game);
    rec.liveConnectedPlayerCount = BubbleGameTestAccess::connectedPlayerCount(game);
    rec.livePendingHighscore = BubbleGameTestAccess::pendingHighscore(game);
    rec.liveGameWon = BubbleGameTestAccess::gameWon(game);
    rec.liveGameLost = BubbleGameTestAccess::gameLost(game);
    rec.liveMpTrainScore = BubbleGameTestAccess::mpTrainScore(game);
    rec.liveModeTimerExpired = BubbleGameTestAccess::modeTimerExpired(game);
    for (int seat = 0; seat < playerCount; ++seat) {
        const BubbleArray& p = BubbleGameTestAccess::player(game, seat);
        rec.liveRFired.push_back(p.rFired);
        rec.liveRPopped.push_back(p.rPopped);
        rec.liveRSent.push_back(p.rSent);
        rec.liveRRecv.push_back(p.rRecv);
        rec.liveRBlk.push_back(p.rBlk);
        rec.livePlayerStates.push_back(static_cast<int>(p.playerState));
        int bubbleCount = 0;
        for (const auto& row : p.bubbleMap)
            for (const Bubble& b : row)
                if (b.bubbleId != -1) ++bubbleCount;
        rec.liveSeatBubbleCounts.push_back(bubbleCount);
    }
}

static CapturedRecording RunLiveRound(SDL_Renderer* renderer,
                                      const SetupSettings& setup,
                                      uint32_t seed,
                                      const std::function<void(BubbleGame&)>& initBoard,
                                      const std::vector<StepScript>& script) {
    singleBubbles.clear();
    malusBubbles.clear();

    BubbleGame game(renderer);
    BubbleGameTestAccess::seedRng(game, seed);
    game.SetSessionMode(BubbleGame::SessionMode::Live);
    game.NewGame(setup);
    // Pinning every bot's stream keeps the live run itself reproducible; the
    // replay must not touch it again (asserted in RunReplay).
    BubbleGameTestAccess::seedBots(game, setup.playerCount, seed * 55291u);
    if (initBoard) initBoard(game);

    NetworkClient* nc = NetworkClient::Instance();
    nc->testGameDataSendCount = 0;

    CapturedRecording rec;
    CaptureLiveSteps(game, setup.playerCount, script, rec);
    rec.liveNetworkSends = nc->testGameDataSendCount;
    return rec;
}

// ---------------------------------------------------------------------------
// R6a: 2-peer network round helpers
// ---------------------------------------------------------------------------

// Stand up a fake "already in game" NetworkClient (no socket) and clear any
// stale queued traffic, so the live ProcessNetworkMessages() dispatch runs
// against it. myPlayerId 0 is the local seat; ids 1..seatCount-1 are the
// remote peers. `seatCount` defaults to 2, reproducing the original
// {{0,"local_nick"},{1,"remote_nick"}} map exactly for every existing call;
// a battle-royale scenario passes its real player count so NewGame()'s
// AssignRemoteSeats() seats ids 1..N-1 on boards 1..N-1 (it walks the map in
// ascending id order). Seats past 1 get distinct generated nicks because
// malus/'F' target resolution matches on nickname.
// `room` is optional: pass a GameRoom whose creator equals the fake nick to
// make NetworkClient::IsLeader() true (R6b's leader-side scenarios). It must
// outlive the call. The default (nullptr) keeps IsLeader() false.
static void SetupFakeNetworkClient(GameRoom* room = nullptr, int seatCount = 2) {
    NetworkClient* nc = NetworkClient::Instance();
    NetworkClientTestAccess::SetState(*nc, IN_GAME);
    NetworkClientTestAccess::SetMyPlayerId(*nc, 0);
    NetworkClientTestAccess::SetPlayerNick(*nc, "local_nick");
    std::map<int, std::string> idToNick = {{0, "local_nick"}, {1, "remote_nick"}};
    for (int id = 2; id < seatCount; ++id)
        idToNick[id] = "remote" + std::to_string(id);
    NetworkClientTestAccess::SetPlayerIdToNick(*nc, idToNick);
    NetworkClientTestAccess::SetCurrentGame(*nc, room);
    while (nc->HasMessage()) nc->GetNextMessage();
}

static void ResetFakeNetworkClient() {
    NetworkClient* nc = NetworkClient::Instance();
    NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    NetworkClientTestAccess::SetState(*nc, DISCONNECTED);
    while (nc->HasMessage()) nc->GetNextMessage();
}

// Like RunLiveRound, but with the fake network client installed before
// NewGame() (so NewGame's network branch seats the remote peer) and torn down
// afterwards. Every send still no-ops on the missing socket. `room` is the
// optional leader room described on SetupFakeNetworkClient. `stopAtFinish`
// defaults true; see CaptureLiveSteps for the R6c result-tail use. Bot streams
// are seeded exactly as RunLiveRound does, so a hosted-bot scenario's live
// decisions are reproducible (and still never re-run during playback).
static CapturedRecording RunLiveNetworkRound(SDL_Renderer* renderer,
                                             const SetupSettings& setup,
                                             uint32_t seed,
                                             const std::function<void(BubbleGame&)>& initBoard,
                                             const std::vector<StepScript>& script,
                                             GameRoom* room = nullptr,
                                             bool stopAtFinish = true) {
    singleBubbles.clear();
    malusBubbles.clear();

    SetupFakeNetworkClient(room, setup.playerCount);
    NetworkClient* nc = NetworkClient::Instance();
    nc->testGameDataSendCount = 0;

    BubbleGame game(renderer);
    BubbleGameTestAccess::seedRng(game, seed);
    game.SetSessionMode(BubbleGame::SessionMode::Live);
    game.NewGame(setup);
    BubbleGameTestAccess::seedBots(game, setup.playerCount, seed * 55291u);
    if (initBoard) initBoard(game);

    CapturedRecording rec;
    CaptureLiveSteps(game, setup.playerCount, script, rec, stopAtFinish);
    rec.liveNetworkSends = nc->testGameDataSendCount;
    ResetFakeNetworkClient();
    return rec;
}

struct ReplayResult {
    std::vector<uint64_t> hashes;
    std::vector<std::vector<SDL_Point>> startPositions;
    std::vector<unsigned> botRngBefore;
    std::vector<unsigned> botRngAfter;
    bool pendingHighscore = false;
    int networkSends = 0;
    bool gameFinish = false;
    int outcome = kReplayOutcomeIncomplete;
    // R6b: no NetBotConnection/botConnections exist on the playback instance.
    bool botConnectionsEmpty = false;
    // R6c: per-seat stats/lifecycle after the whole replay, so a scenario can
    // assert a late 'S' or an 'l' departure landed identically, independent of
    // (but consistent with) the canonical hash comparison.
    std::vector<int> rFired;
    std::vector<int> rPopped;
    std::vector<int> playerStates;
    int connectedPlayerCount = 0;
};

static ReplayResult RunReplay(SDL_Renderer* renderer, const CapturedRecording& rec) {
    singleBubbles.clear();
    malusBubbles.clear();

    BubbleGame game(renderer);
    RestoreRoundStart(game, rec.start);
    CHECK(BubbleGameTestAccess::sessionMode(game) == BubbleGame::SessionMode::Playback);

    NetworkClient* nc = NetworkClient::Instance();
    nc->testGameDataSendCount = 0;

    // Poison the live wall clock: AdvancePlaybackStep() must use the recorded
    // values, never this. If it read the singleton, every hash would diverge.
    FrozenBubble::Instance()->deltaScale = 3.0f;

    ReplayResult result;
    for (int seat = 0; seat < rec.playerCount; ++seat)
        result.startPositions.push_back(CollectPositions(game, seat));
    for (int seat = 0; seat < rec.playerCount; ++seat)
        result.botRngBefore.push_back(BubbleGameTestAccess::botRng(game, seat));

    // One StepRecord per seat per step, in (simStep, seat) order: apply every
    // seat's captured controls, then run exactly one playback step.
    size_t i = 0;
    while (i < rec.steps.size()) {
        const int step = rec.steps[i].simStep;
        const float deltaScale = rec.steps[i].deltaScale;
        // Same recorded clock for every seat in this step (CaptureStep reads
        // the one stepGameClockMs the step set), so take it once from the
        // group's first record.
        const Uint32 gameClockMs = rec.steps[i].gameClockMs;
        // R6a: the step's inbound network payloads ride on seat 0's record
        // (see CaptureStep). Decode and queue them for this playback step
        // before applying controls + stepping, matching where
        // ProcessNetworkMessages() runs on the live path.
        const std::vector<uint8_t>* inbound = nullptr;
        while (i < rec.steps.size() && rec.steps[i].simStep == step) {
            const StepRecord& s = rec.steps[i];
            if (s.seatId < (uint32_t)rec.playerCount) {
                PlayerControls c;
                c.left = s.left != 0;
                c.right = s.right != 0;
                c.center = s.center != 0;
                c.fire = s.fire != 0;
                c.firedByMouse = s.firedByMouse != 0;
                c.mouseAngle = s.mouseAngle;
                BubbleGameTestAccess::player(game, (int)s.seatId).lastControls = c;
            }
            if (s.seatId == 0) inbound = &s.inboundEvents;
            ++i;
        }
        static const std::vector<uint8_t> kNoInbound;
        SetPlaybackInboundEvents(game, inbound ? *inbound : kNoInbound);
        game.AdvancePlaybackStep(deltaScale, gameClockMs);
        result.hashes.push_back(CaptureCanonicalStateHash(game));
    }

    for (int seat = 0; seat < rec.playerCount; ++seat)
        result.botRngAfter.push_back(BubbleGameTestAccess::botRng(game, seat));

    result.pendingHighscore = BubbleGameTestAccess::pendingHighscore(game);
    result.networkSends = nc->testGameDataSendCount;
    result.gameFinish = BubbleGameTestAccess::gameFinish(game);
    result.outcome = static_cast<int>(CaptureRoundEnd(game).outcome);
    result.botConnectionsEmpty = BubbleGameTestAccess::botConnectionsEmpty(game);
    result.connectedPlayerCount = BubbleGameTestAccess::connectedPlayerCount(game);
    for (int seat = 0; seat < rec.playerCount; ++seat) {
        const BubbleArray& p = BubbleGameTestAccess::player(game, seat);
        result.rFired.push_back(p.rFired);
        result.rPopped.push_back(p.rPopped);
        result.playerStates.push_back(static_cast<int>(p.playerState));
    }
    return result;
}

static void CheckHashSequence(const char* label,
                              const std::vector<uint64_t>& expected,
                              const std::vector<uint64_t>& actual) {
    if (expected.size() != actual.size()) {
        std::fprintf(stderr, "CHECK failed: %s: size mismatch (expected %zu, got %zu)\n",
                     label, expected.size(), actual.size());
        ++failures;
        return;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] != actual[i]) {
            std::fprintf(stderr, "CHECK failed: %s: first divergent step %zu (expected %llu, got %llu)\n",
                         label, i,
                         static_cast<unsigned long long>(expected[i]),
                         static_cast<unsigned long long>(actual[i]));
            ++failures;
            return;
        }
    }
}

static void CheckPositions(const char* label, const CapturedRecording& rec,
                           const ReplayResult& replay) {
    if (replay.startPositions.size() != rec.startPositions.size()) {
        std::fprintf(stderr, "CHECK failed: %s: seat count mismatch\n", label);
        ++failures;
        return;
    }
    for (size_t seat = 0; seat < rec.startPositions.size(); ++seat) {
        const auto& want = rec.startPositions[seat];
        const auto& got = replay.startPositions[seat];
        if (want.size() != got.size()) {
            std::fprintf(stderr, "CHECK failed: %s: seat %zu bubble count mismatch (%zu vs %zu)\n",
                         label, seat, want.size(), got.size());
            ++failures;
            return;
        }
        for (size_t k = 0; k < want.size(); ++k) {
            if (want[k].x != got[k].x || want[k].y != got[k].y) {
                std::fprintf(stderr,
                             "CHECK failed: %s: seat %zu bubble %zu position (%d,%d) != (%d,%d)\n",
                             label, seat, k, want[k].x, want[k].y, got[k].x, got[k].y);
                ++failures;
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Disk writer/player round-trip
// ---------------------------------------------------------------------------

static std::vector<uint8_t> EncodeRecording(const CapturedRecording& rec) {
    ReplayWriter writer;
    ReplayHeader header;
    const char fingerprint[] = "r5a-local-replay";
    header.buildFingerprint.assign(fingerprint, fingerprint + std::strlen(fingerprint));
    writer.WriteHeader(header);
    writer.WriteRoundStart(rec.start);
    for (size_t i = 0; i < rec.liveHashes.size(); ++i) {
        AssertionRecord assertion;
        assertion.simStep = rec.steps[i * rec.playerCount].simStep;
        assertion.seatId = 0;
        assertion.acceptedShotColor = 0;
        assertion.acceptedColumn = -1;
        assertion.acceptedRow = -1;
        assertion.canonicalStateHash = rec.liveHashes[i];
        writer.WriteAssertion(assertion);
        for (int seat = 0; seat < rec.playerCount; ++seat)
            writer.WriteStep(rec.steps[i * rec.playerCount + seat]);
    }
    writer.WriteRoundEnd(rec.end);
    return writer.Bytes();
}

static bool DecodeRecording(const std::vector<uint8_t>& bytes, CapturedRecording& out,
                            int playerCount) {
    ReplayReader reader(bytes);
    ReplayHeader header;
    if (reader.ReadHeader(header) != DecodeResult::Ok) return false;
    if (reader.ReadRoundStart(out.start) != DecodeResult::Ok) return false;
    out.playerCount = playerCount;

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
            out.liveHashes.push_back(a.canonicalStateHash);
        } else if (type == RecordType::RoundEnd) {
            if (reader.ReadRoundEnd(out.end) != DecodeResult::Ok) return false;
        } else {
            return false;
        }
    }
    return true;
}

static bool WriteFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

static bool ReadFile(const std::filesystem::path& path, std::vector<uint8_t>& bytes) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const std::streamsize size = in.tellg();
    if (size < 0) return false;
    in.seekg(0);
    bytes.resize(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), size);
    return static_cast<bool>(in) || in.eof();
}

// Full live->replay comparison plus the independence guarantees every R5a
// scenario relies on: identical positions at the restored start, identical
// state at every step, and no bot RNG advanced on the playback side.
static void RunAndCompare(SDL_Renderer* renderer, const char* label,
                          const CapturedRecording& rec) {
    ReplayResult replay = RunReplay(renderer, rec);
    CheckPositions(label, rec, replay);
    CheckHashSequence(label, rec.liveHashes, replay.hashes);
    // Playback must never reach the network send path. Checked for every
    // scenario (local sends no-op via `!networkGame`; R6a network rounds via
    // the SessionMode::Playback gate). R6a's two effect guards
    // (SendNetworkBubbleShot, SendMalusToOpponent) exist so this stays true.
    CHECK(replay.networkSends == 0);
    for (size_t seat = 0; seat < replay.botRngBefore.size(); ++seat) {
        if (replay.botRngBefore[seat] != replay.botRngAfter[seat]) {
            std::fprintf(stderr,
                         "CHECK failed: %s: bot RNG advanced on playback at seat %zu (DriveBot re-ran)\n",
                         label, seat);
            ++failures;
        }
    }
}

// ---------------------------------------------------------------------------

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_AUDIODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();

    SDL_Window* window = SDL_CreateWindow("bubblegame-replay-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Keep highscore writes out of the user's real preference directory -- the
    // live win below legitimately persists (that is the effect Playback must
    // suppress), so this test must not touch the real library.
    std::error_code ec;
    const std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "frozen-bubble-r5a-replay-test";
    std::filesystem::remove_all(scratch, ec);
    std::filesystem::create_directories(scratch, ec);
    std::string prefPathStorage = scratch.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPathStorage.c_str();
    settings->ReadSettings();

    // A custom levelset whose 10th row has 8 cells. Every shipped level has 7,
    // which routes LoadLevel into the filler-row branch that happens to match
    // the general position formula -- the branch that inverts it (and that
    // R5a fixed by storing each row's smallerSep explicitly) is only reachable
    // with a level like this one.
    const std::filesystem::path customLevelPath = scratch / "custom-levels";
    {
        std::ofstream lvl(customLevelPath, std::ios::trunc);
        lvl << "3 3 3 3 3 3 3 3\n";
        lvl << "2 2 2 2 2 2 2\n";
        lvl << "4 4 4 4 4 4 4 4\n";
        lvl << "- - - - - - -\n";
        lvl << "- - - - - - - -\n";
        lvl << "- - - - - - -\n";
        lvl << "- - - - - - - -\n";
        lvl << "- - - - - - -\n";
        lvl << "- - - - - - - -\n";
        lvl << "1 1 1 1 1 1 1 1\n";  // row 9: 8 cells -> LoadLevel's filler if-branch
    }

    const SetupSettings solo = MakeSoloClassicSetup();

    // --- Round 1: win by clearing, with a normal aimed shot + varied scale --
    {
        CapturedRecording rec = RunLiveRound(renderer, solo, 20260920u, SetupWinBoard,
                                             FireThenCoast(PI / 2.0f));
        CHECK(rec.liveGameWon == true);
        CHECK(rec.end.outcome == kReplayOutcomeWin);
        CHECK(rec.liveHashes.size() > 1);
        CHECK(rec.end.complete == 1);
        // The live run really did earn a highscore; Playback must suppress it.
        CHECK(rec.livePendingHighscore == true);

        ReplayResult replay;
        {
            // 1) In-memory replay: compare at every step.
            replay = RunReplay(renderer, rec);
            CHECK(replay.gameFinish == true);
            CHECK(replay.outcome == kReplayOutcomeWin);
            CheckHashSequence("win in-memory", rec.liveHashes, replay.hashes);

            // 3) Effect suppression on the Playback instance.
            CHECK(replay.pendingHighscore == false);
            CHECK(replay.networkSends == 0);
        }

        // 2) Disk round-trip: encode, write, read back, replay the decoded
        //    records, and confirm both the per-step assertion hashes and a
        //    fresh replay reproduce the live sequence.
        const std::vector<uint8_t> bytes = EncodeRecording(rec);
        CHECK(bytes.size() > 0);
        const std::filesystem::path file = scratch / "round-win.fbr";
        CHECK(WriteFile(file, bytes));

        std::vector<uint8_t> readBack;
        CHECK(ReadFile(file, readBack));
        CHECK(readBack == bytes);

        CapturedRecording decoded;
        CHECK(DecodeRecording(readBack, decoded, rec.playerCount));
        CHECK(decoded.steps.size() == rec.steps.size());
        CheckHashSequence("win disk assertions", rec.liveHashes, decoded.liveHashes);

        ReplayResult diskReplay = RunReplay(renderer, decoded);
        CheckHashSequence("win disk replay", rec.liveHashes, diskReplay.hashes);
    }

    // --- Round 2: loss via danger zone after a wall/ceiling hit -------------
    {
        CapturedRecording rec = RunLiveRound(renderer, solo, 78123u, SetupLossBoard,
                                             FireThenCoast(0.3f));
        CHECK(rec.end.outcome == kReplayOutcomeLoss);
        CHECK(rec.liveGameLost == true);
        CHECK(rec.liveHashes.size() > 1);

        ReplayResult replay = RunReplay(renderer, rec);
        CHECK(replay.outcome == kReplayOutcomeLoss);
        CheckHashSequence("loss in-memory", rec.liveHashes, replay.hashes);
    }

    // --- Round 3: hurry-timer auto-fire, no input at all -------------------
    {
        CapturedRecording rec = RunLiveRound(renderer, solo, 55501u, SetupHurryBoard,
                                             IdleFor(240));
        CHECK(rec.liveHashes.size() > 2);
        // The hurry timer force-fires without any input: the resolved control
        // captured on the forced step must be a fire edge.
        bool sawForcedFire = false;
        for (size_t i = 0; i < rec.steps.size(); ++i) {
            if (rec.steps[i].fire != 0) { sawForcedFire = true; break; }
        }
        CHECK(sawForcedFire);
        CHECK(rec.liveRFired[0] >= 1);

        ReplayResult replay = RunReplay(renderer, rec);
        CheckHashSequence("hurry in-memory", rec.liveHashes, replay.hashes);
    }

    // --- R5a: 3 local players with two bots, control replay without AI ------
    {
        LocalMultiplayerOptions options;
        options.playerCount = 3;
        options.botCount = 2;
        options.botSkill = 1;
        options.chainReaction = false;
        options.gameMode = GameMode::Classic;
        options.attackMode = AttackMode::On;
        SetupSettings setup = BuildLocalMultiplayerSettings(options);

        CapturedRecording rec = RunLiveRound(renderer, setup, 31337u, nullptr,
                                             FireThenCoast(PI / 2.0f));
        CHECK(rec.playerCount == 3);
        CHECK(rec.steps.size() == rec.liveHashes.size() * 3);
        // Both bots actually played, so their captured controls are meaningful.
        CHECK(rec.liveRFired[1] > 0);
        CHECK(rec.liveRFired[2] > 0);
        std::fprintf(stderr, "  [local-mp-bots] fired human=%d bot1=%d bot2=%d popped=%d/%d/%d\n",
                     rec.liveRFired[0], rec.liveRFired[1], rec.liveRFired[2],
                     rec.liveRPopped[0], rec.liveRPopped[1], rec.liveRPopped[2]);
        RunAndCompare(renderer, "local-mp-bots", rec);
    }

    // --- R4d bug fix regression: local 2-player (no network) replay must not
    // hijack FrozenBubble's top-level state, and must restore a populated
    // board -----------------------------------------------------------------
    //
    // Bug report (2026-09-21): opening a replay of a local 2-player round from
    // the Replays page showed no bubbles on screen. bubblegame_replay.cpp's
    // capture/restore engine itself was not the cause (every existing
    // playerCount==2 scenario in this suite happened to be a network round,
    // leaving local 2P genuinely untested end to end). The real cause was in
    // NewGame(), called by RestoreRoundStart(): it unconditionally set
    // FrozenBubble::Instance()->currentState = MainGame, regardless of which
    // BubbleGame instance NewGame() ran on. RestoreRoundStart()'s throwaway
    // replay-viewer instance is never FrozenBubble::mainGame, so this flipped
    // the app's top-level state machine to MainGame the instant a replay was
    // opened -- the very next frame rendered the real, empty mainGame
    // singleton instead of MainMenu's replay viewer. Fixed by gating that
    // assignment (and the input-queue flush beside it) on sessionMode ==
    // SessionMode::Live, which RestoreRoundStart() already sets to Playback
    // before calling NewGame(). This scenario pins both halves: the board
    // really does restore with content, and currentState is left alone.
    {
        LocalMultiplayerOptions options;
        options.playerCount = 2;
        options.botCount = 0;
        options.chainReaction = false;
        options.gameMode = GameMode::Classic;
        options.attackMode = AttackMode::On;
        SetupSettings setup = BuildLocalMultiplayerSettings(options);

        CapturedRecording rec = RunLiveRound(renderer, setup, 20260921u, nullptr,
                                             FireThenCoast(PI / 2.0f));
        CHECK(rec.playerCount == 2);
        int liveNonEmpty0 = 0, liveNonEmpty1 = 0;
        for (int id : rec.startBubbleIds[0]) if (id != -1) ++liveNonEmpty0;
        for (int id : rec.startBubbleIds[1]) if (id != -1) ++liveNonEmpty1;
        CHECK(liveNonEmpty0 > 0);
        CHECK(liveNonEmpty1 > 0);

        // Restore directly (bypassing the AdvancePlaybackStep loop) and inspect
        // the board immediately after RestoreRoundStart, exactly what
        // ReplayPlayer::Draw() would show before any AdvanceFrame() call.
        {
            // Force a TitleScreen baseline -- exactly the state the app is in
            // while the Replays menu is open -- so the check below actually
            // proves the fix rather than coincidentally comparing MainGame to
            // MainGame (an earlier live-round test in this same process leaves
            // currentState at MainGame otherwise).
            FrozenBubble::Instance()->currentState = TitleScreen;
            BubbleGame replayGame(renderer);
            RestoreRoundStart(replayGame, rec.start);
            CHECK(BubbleGameTestAccess::sessionMode(replayGame) == BubbleGame::SessionMode::Playback);
            // The regression: RestoreRoundStart() must never touch the app's
            // top-level state machine.
            CHECK(FrozenBubble::Instance()->currentState == TitleScreen);

            for (int seat = 0; seat < 2; ++seat) {
                int nonEmpty = 0;
                for (int id : CollectBubbleIds(replayGame, seat)) if (id != -1) ++nonEmpty;
                CHECK(nonEmpty > 0);
                CHECK(BubbleGameTestAccess::player(replayGame, seat).boardVisible);
            }
        }

        RunAndCompare(renderer, "local-2p-no-network", rec);
    }

    // --- R5a: chain reactions -----------------------------------------------
    {
        LocalMultiplayerOptions options;
        options.playerCount = 3;
        options.botCount = 0;
        options.chainReaction = true;
        options.gameMode = GameMode::Classic;
        SetupSettings setup = BuildLocalMultiplayerSettings(options);

        CapturedRecording rec = RunLiveRound(renderer, setup, 90210u,
                                             [](BubbleGame& g) { SetupAttackBoard(g, true); },
                                             FireThenIdle(PI / 2.0f, 180));
        // A detached bubble fell and a same-color group existed, so the chain
        // assignment path really ran (its RNG draws and target cells are part
        // of the step hash).
        std::fprintf(stderr, "  [chain] sawChain=%d fired=%d popped=%d sent=%d\n",
                     rec.liveSawChain ? 1 : 0, rec.liveRFired[0], rec.liveRPopped[0], rec.liveRSent[0]);
        CHECK(rec.liveSawChain == true);
        RunAndCompare(renderer, "chain-reaction", rec);
    }

    // --- R5a: AttackMode::On sends malus ------------------------------------
    {
        LocalMultiplayerOptions options;
        options.playerCount = 3;
        options.botCount = 0;
        options.chainReaction = false;
        options.gameMode = GameMode::Classic;
        options.attackMode = AttackMode::On;
        SetupSettings setup = BuildLocalMultiplayerSettings(options);

        CapturedRecording rec = RunLiveRound(renderer, setup, 4242u,
                                             [](BubbleGame& g) { SetupAttackBoard(g, false); },
                                             FireThenIdle(PI / 2.0f, 150));
        std::fprintf(stderr, "  [attack-on] fired=%d popped=%d sent=%d recv=%d/%d/%d rBlk=%d\n",
                     rec.liveRFired[0], rec.liveRPopped[0], rec.liveRSent[0],
                     rec.liveRRecv[0], rec.liveRRecv[1], rec.liveRRecv[2], rec.liveRBlk[0]);
        CHECK(rec.liveRSent[0] > 0);
        CHECK(rec.liveRRecv[1] > 0);
        CHECK(rec.liveRRecv[2] > 0);
        RunAndCompare(renderer, "attack-on", rec);
    }

    // --- R5a: AttackMode::Canceling absorbs a queued malus -------------------
    {
        LocalMultiplayerOptions options;
        options.playerCount = 3;
        options.botCount = 0;
        options.chainReaction = false;
        options.gameMode = GameMode::Classic;
        options.attackMode = AttackMode::Canceling;
        SetupSettings setup = BuildLocalMultiplayerSettings(options);

        CapturedRecording rec = RunLiveRound(renderer, setup, 777u,
                                             [](BubbleGame& g) {
                                                 SetupAttackBoard(g, false);
                                                 // Owe far more malus than this pop earns, so the
                                                 // whole earning is cancelled and nothing is sent.
                                                 BubbleArray& p = BubbleGameTestAccess::player(g, 0);
                                                 for (int i = 0; i < 20; ++i) p.malusQueue.push_back(2000 + i);
                                             },
                                             FireThenIdle(PI / 2.0f, 150));
        CHECK(rec.liveRBlk[0] > 0);
        CHECK(rec.liveRSent[0] == 0);
        CHECK(rec.liveRRecv[1] == 0);
        CHECK(rec.liveRRecv[2] == 0);
        RunAndCompare(renderer, "attack-canceling", rec);
    }

    // --- R5a: teams spare teammates -----------------------------------------
    {
        LocalMultiplayerOptions options;
        options.playerCount = 4;
        options.botCount = 0;
        options.chainReaction = false;
        options.gameMode = GameMode::Classic;
        options.attackMode = AttackMode::On;
        options.teamMode = true;   // seats 0/2 team 1, seats 1/3 team 2
        SetupSettings setup = BuildLocalMultiplayerSettings(options);

        CapturedRecording rec = RunLiveRound(renderer, setup, 24680u,
                                             [](BubbleGame& g) { SetupAttackBoard(g, false); },
                                             FireThenIdle(PI / 2.0f, 150));
        // Seat 0's teammate is seat 2: the attack must skip it and land on 1/3.
        CHECK(rec.liveRSent[0] > 0);
        CHECK(rec.liveRRecv[1] > 0);
        CHECK(rec.liveRRecv[2] == 0);
        CHECK(rec.liveRRecv[3] > 0);
        RunAndCompare(renderer, "teams", rec);
    }

    // --- R5a: Race mode's target check reproduces the same outcome ----------
    {
        LocalMultiplayerOptions options;
        options.playerCount = 3;
        options.botCount = 0;
        options.chainReaction = false;
        options.gameMode = GameMode::Race;
        options.raceTargetIndex = 0;  // 10; the crafted pop is enough with falling counted
        SetupSettings setup = BuildLocalMultiplayerSettings(options);
        setup.raceTarget = 1;  // the first popped bubble wins

        CapturedRecording rec = RunLiveRound(renderer, setup, 1u,
                                             [](BubbleGame& g) { SetupAttackBoard(g, false); },
                                             FireThenIdle(PI / 2.0f, 150));
        CHECK(rec.end.complete == 1);
        CHECK(rec.end.outcome == kReplayOutcomeWin);
        CHECK(rec.end.winningSeatId == 0);

        ReplayResult replay = RunReplay(renderer, rec);
        CheckPositions("race", rec, replay);
        CheckHashSequence("race in-memory", rec.liveHashes, replay.hashes);
        CHECK(replay.gameFinish == true);
        CHECK(replay.outcome == kReplayOutcomeWin);
    }

    // --- R5a: randomLevels=false (predefined level) position restore ---------
    {
        SetupSettings setup = MakeSoloClassicSetup(/*randomLevels=*/false, /*startLevel=*/1);
        setup.randomLevels = false;
        // NewGame loads the shipped levelset; swap in the custom one (whose 10th
        // row takes LoadLevel's inverted filler branch) and reload level 1.
        CapturedRecording rec = RunLiveRound(
            renderer, setup, 13579u,
            [&](BubbleGame& g) {
                g.LoadLevelset(customLevelPath.string().c_str());
                g.LoadLevel(1);
                BubbleArray& p = BubbleGameTestAccess::player(g, 0);
                p.curLaunch = 1;
                p.nextBubble = 1;
                p.nextColors.assign(8, 1);
            },
            IdleFor(120));
        CHECK(rec.liveHashes.size() > 1);
        RunAndCompare(renderer, "predefined-level", rec);
    }

    // --- R5a: mid-match ReloadGame transition --------------------------------
    {
        singleBubbles.clear();
        malusBubbles.clear();

        LocalMultiplayerOptions options;
        options.playerCount = 2;
        options.botCount = 0;
        options.chainReaction = false;
        options.gameMode = GameMode::Race;
        options.raceTargetIndex = 0;
        SetupSettings setup = BuildLocalMultiplayerSettings(options);
        setup.raceTarget = 1;  // the first popped bubble wins the round

        BubbleGame live(renderer);
        BubbleGameTestAccess::seedRng(live, 989898u);
        live.SetSessionMode(BubbleGame::SessionMode::Live);
        live.NewGame(setup);
        SetupAttackBoard(live, false);

        // Round 1: seat 0 reaches the race target, accumulating a win.
        for (int i = 0; i < 300 && !BubbleGameTestAccess::gameFinish(live); ++i) {
            FrozenBubble::Instance()->deltaScale = 1.0f;
            for (int s = 0; s < 2; ++s)
                controllerInputs[s] = ControllerInput{};
            BubbleArray& p0 = BubbleGameTestAccess::player(live, 0);
            p0.mouseTargetAngle = (i == 0) ? PI / 2.0f : -1.0f;
            p0.mouseFirePending = (i == 0);
            live.AdvanceSimulation();
        }
        CHECK(BubbleGameTestAccess::gameFinish(live) == true);
        CHECK(BubbleGameTestAccess::player(live, 0).winCount == 1);

        BubbleGameTestAccess::reloadGame(live, 1);  // randomLevels: a fresh board

        CapturedRecording rec;
        CaptureLiveSteps(live, 2, FireThenIdle(PI / 2.0f, 150), rec);
        // The recorded round-2 snapshot carries the win accumulated in round
        // 1 and a nonzero simStep from round 1; the carried-forward RNG is
        // proven by the per-step hash equality below.
        CHECK(rec.start.startingWins[0] == 1);
        CHECK(rec.start.initialSimStep > 0);
        std::fprintf(stderr, "  [reloadgame] startingWins=%d initialSimStep=%d startingScore=%d\n",
                     rec.start.startingWins[0], rec.start.initialSimStep, rec.start.startingScore[0]);
        RunAndCompare(renderer, "reloadgame", rec);
    }

    // --- R5b: training round (mpTraining), recorded clock crosses 120s -------
    {
        // Round-1-only by design: NewGame() resets mpTrainStartTime and this
        // slice does not add ReloadGame() continuity for training. The clock
        // seam self-initializes it on the first step, exactly as the live path
        // used to from SDL_GetTicks().
        SetupSettings setup;
        setup.playerCount = 1;
        setup.networkGame = false;
        setup.randomLevels = false;
        setup.startLevel = 1;
        setup.gameMode = GameMode::Classic;
        setup.mpTraining = true;

        // 200 synthetic 1ms steps let the straight-up shot pop the crafted
        // color-5 group; the final jump crosses the real 120000ms TRAIN_DURATION.
        // The board is deliberately not cleared, so the round ends on the
        // training clock rather than via the solo win path.
        std::vector<StepScript> script = SyntheticClockSteps(1000, 1, 200);
        script[0].aim = true;
        script[0].angle = PI / 2.0f;
        script[0].fire = true;
        StepScript expire;
        expire.syntheticClock = true;
        expire.deltaScale = 1.0f;
        expire.gameClockMs = 1000 + 120000 + 10;
        script.push_back(expire);

        CapturedRecording rec = RunLiveRound(renderer, setup, 424242u,
                                             [](BubbleGame& g) { SetupTrainingBoard(g); },
                                             script);
        CHECK(rec.liveGameWon == true);
        CHECK(rec.end.outcome == kReplayOutcomeWin);
        CHECK(rec.end.complete == 1);
        // The new hash field is exercised with a nonzero value: at least one
        // pop scored malus for the training score. This is what makes the
        // mpTrainScore hash entry meaningful rather than always-zero.
        CHECK(rec.liveMpTrainScore > 0);
        std::fprintf(stderr, "  [mp-training] steps=%zu score=%d fired=%d popped=%d pendingHighscore=%d\n",
                     rec.liveHashes.size(), rec.liveMpTrainScore, rec.liveRFired[0],
                     rec.liveRPopped[0], rec.livePendingHighscore ? 1 : 0);
        // The live run earned a training highscore; Playback must suppress it.
        CHECK(rec.livePendingHighscore == true);

        RunAndCompare(renderer, "mp-training", rec);
        ReplayResult replay = RunReplay(renderer, rec);
        CHECK(replay.gameFinish == true);
        CHECK(replay.outcome == kReplayOutcomeWin);
        CHECK(replay.pendingHighscore == false);
    }

    // --- R5b: local Timed round, clear leader wins when the clock runs out ---
    {
        LocalMultiplayerOptions options;
        options.playerCount = 3;
        options.botCount = 0;
        options.chainReaction = false;
        options.gameMode = GameMode::Timed;
        SetupSettings setup = BuildLocalMultiplayerSettings(options);
        setup.timedSeconds = 2;  // expire shortly after seat 0's shot pops

        // Seat 0's crafted top row pops on the straight-up shot; seats 1/2 get
        // no controls and never pop, so LeadingPopper() has an unambiguous
        // leader. The final jump crosses the 2000ms limit.
        std::vector<StepScript> script = SyntheticClockSteps(1000, 1, 200);
        script[0].aim = true;
        script[0].angle = PI / 2.0f;
        script[0].fire = true;
        StepScript expire;
        expire.syntheticClock = true;
        expire.deltaScale = 1.0f;
        expire.gameClockMs = 1000 + 2000 + 10;
        script.push_back(expire);

        CapturedRecording rec = RunLiveRound(renderer, setup, 5566u,
                                             [](BubbleGame& g) { SetupAttackBoard(g, false); },
                                             script);
        CHECK(rec.playerCount == 3);
        CHECK(rec.end.complete == 1);
        CHECK(rec.end.outcome == kReplayOutcomeWin);
        CHECK(rec.end.winningSeatId == 0);
        CHECK(rec.liveRPopped[0] > rec.liveRPopped[1]);
        CHECK(rec.liveRPopped[0] > rec.liveRPopped[2]);
        CHECK(rec.liveModeTimerExpired == true);
        std::fprintf(stderr, "  [timed] steps=%zu popped=%d/%d/%d winner=%u\n",
                     rec.liveHashes.size(), rec.liveRPopped[0], rec.liveRPopped[1],
                     rec.liveRPopped[2], static_cast<unsigned>(rec.end.winningSeatId));

        RunAndCompare(renderer, "timed", rec);
        ReplayResult replay = RunReplay(renderer, rec);
        CHECK(replay.gameFinish == true);
        CHECK(replay.outcome == kReplayOutcomeWin);
    }

    // --- R6b: network Timed captured as leader replays to the verdict ------
    {
        // Regression for UpdateTimedRound()'s leader/joiner asymmetry. The live
        // capture runs as the leader (the fake room's creator matches our nick),
        // so its recording is the *short* one: the leader resolves once its own
        // modeTimerDeadline (expiry + 1500ms) passes, without waiting for the
        // remote seat's final count. Before the R6b fix, Playback had no
        // NetworkClient room, so IsLeader() always read false and the replay
        // would take the non-leader wait branch (expiry + 1500 + 2000ms grace),
        // running out of recorded steps before UpdateTimedRound() ever resolved
        // -- replay.gameFinish would stay false even though every available
        // step's hash matched. The recorded wasNetworkLeader flag makes the
        // replay resolve at the same short step. The final two jumps cross the
        // 2000ms limit and then the leader's +1500ms report wait.
        SetupSettings setup = MakeNetworkSetup();
        setup.gameMode = GameMode::Timed;
        setup.timedSeconds = 2;
        GameRoom room;
        room.creator = "local_nick";
        room.started = true;

        std::vector<StepScript> script = SyntheticClockSteps(1000, 1, 200);
        script[0].aim = true;
        script[0].angle = PI / 2.0f;
        script[0].fire = true;
        StepScript expire;
        expire.syntheticClock = true;
        expire.deltaScale = 1.0f;
        expire.gameClockMs = 1000 + 2000 + 10;
        script.push_back(expire);
        StepScript pastDeadline;
        pastDeadline.syntheticClock = true;
        pastDeadline.deltaScale = 1.0f;
        pastDeadline.gameClockMs = 1000 + 2000 + 10 + 1500 + 10;
        script.push_back(pastDeadline);

        CapturedRecording rec = RunLiveNetworkRound(
            renderer, setup, 6401u,
            [](BubbleGame& g) {
                ShapeNetworkBoard(g, 0);
                ShapeNetworkBoard(g, 1);
                SetupAttackBoard(g, false);
            },
            script, &room);
        CHECK(rec.end.complete == 1);
        CHECK(rec.liveModeTimerExpired == true);
        CHECK(rec.liveRPopped[0] > 0);
        std::fprintf(stderr, "  [net-timed-leader] steps=%zu popped=%d/%d outcome=%u\n",
                     rec.liveHashes.size(), rec.liveRPopped[0], rec.liveRPopped[1],
                     static_cast<unsigned>(rec.end.outcome));
        RunAndCompare(renderer, "net-timed-leader", rec);
        ReplayResult replay = RunReplay(renderer, rec);
        // The assertion that specifically fails without the fix: the replay must
        // actually reach the verdict within the leader's short recording, not
        // merely match hashes for however many steps happen to exist.
        CHECK(replay.gameFinish == true);
        CHECK(replay.outcome == rec.end.outcome);
    }

    // --- R6a: 2-peer network round -- remote fire + stick ------------------
    {
        SetupSettings setup = MakeNetworkSetup();
        auto init = [](BubbleGame& g) {
            ShapeNetworkBoard(g, 0);
            ShapeNetworkBoard(g, 1);
        };
        // Seat 0 fires straight up; the remote peer fires at step 5 and the
        // server delivers its stick at step 10. Both are injected on the real
        // NetworkClient queue, so the live run drains them through the real
        // ProcessNetworkMessages() dispatch.
        std::vector<StepScript> script = IdleFor(40);
        script[0].aim = true;
        script[0].angle = PI / 2.0f;
        script[0].fire = true;
        script[5].inboundWire.push_back("GAMEMSG:1:f1.571:3");
        script[10].inboundWire.push_back("GAMEMSG:1:s2:0:5:1 2 3 4 5 6 7 0");

        CapturedRecording rec = RunLiveNetworkRound(renderer, setup, 6101u, init, script);
        CHECK(rec.playerCount == 2);
        CHECK(rec.start.networkGame == 1);
        CHECK(rec.start.seatOwned[0] == 1);
        CHECK(rec.start.seatOwned[1] == 0);
        CHECK(rec.start.seatIds[0] == 0);
        CHECK(rec.start.seatIds[1] == 1);
        // The remote 's' really landed a bubble on the remote board.
        CHECK(rec.liveSeatBubbleCounts.size() == 2);
        CHECK(rec.liveSeatBubbleCounts[1] >= 1);
        std::fprintf(stderr, "  [net-fire-stick] seat1Bubbles=%d steps=%zu\n",
                     rec.liveSeatBubbleCounts[1], rec.liveHashes.size());

        RunAndCompare(renderer, "net-fire-stick", rec);

        // The inbound payloads survive the disk codec too.
        const std::vector<uint8_t> bytes = EncodeRecording(rec);
        CapturedRecording decoded;
        CHECK(DecodeRecording(bytes, decoded, 2));
        CHECK(decoded.steps.size() == rec.steps.size());
        ReplayResult disk = RunReplay(renderer, decoded);
        CheckHashSequence("net-fire-stick disk", rec.liveHashes, disk.hashes);
        CHECK(disk.networkSends == 0);
    }

    // --- R6a: 2-peer network round -- malus both directions ----------------
    {
        SetupSettings setup = MakeNetworkSetup();
        auto init = [](BubbleGame& g) {
            ShapeNetworkBoard(g, 0);
            ShapeNetworkBoard(g, 1);
            SetupAttackBoard(g, false);  // seat 0's top row pops on a straight shot
        };
        std::vector<StepScript> script = IdleFor(150);
        script[0].aim = true;
        script[0].angle = PI / 2.0f;
        script[0].fire = true;
        // Remote hits the local seat back after the local pop has earned malus.
        script[60].inboundWire.push_back("GAMEMSG:1:glocal_nick:2");

        CapturedRecording rec = RunLiveNetworkRound(renderer, setup, 6102u, init, script);
        // "Local attacks remote" is a real SendMalusToOpponent() send, not an
        // inbound message: it must show up in the local seat's rSent.
        CHECK(rec.liveRSent[0] > 0);
        // "Remote attacks back" is the injected 'g', applied to the local
        // owned seat whose nickname matches the destination.
        CHECK(rec.liveRRecv[0] > 0);
        std::fprintf(stderr, "  [net-malus] sent0=%d recv0=%d recv1=%d\n",
                     rec.liveRSent[0], rec.liveRRecv[0], rec.liveRRecv[1]);
        RunAndCompare(renderer, "net-malus", rec);
    }

    // --- R6a: 2-peer network round -- remote-announced finish ('F') ---------
    {
        SetupSettings setup = MakeNetworkSetup();
        auto init = [](BubbleGame& g) {
            ShapeNetworkBoard(g, 0);
            ShapeNetworkBoard(g, 1);
        };
        std::vector<StepScript> script = IdleFor(30);
        script[10].inboundWire.push_back("GAMEMSG:1:Fremote_nick");

        CapturedRecording rec = RunLiveNetworkRound(renderer, setup, 6103u, init, script);
        CHECK(rec.end.complete == 1);
        CHECK(rec.end.outcome == kReplayOutcomeWin);
        CHECK(rec.end.winningSeatId == 1);
        CHECK(rec.liveGameWon == false);  // a remote win is not the local solo win flag
        std::fprintf(stderr, "  [net-finish] winner=%u steps=%zu\n",
                     static_cast<unsigned>(rec.end.winningSeatId), rec.liveHashes.size());
        RunAndCompare(renderer, "net-finish", rec);
        ReplayResult replay = RunReplay(renderer, rec);
        CHECK(replay.gameFinish == true);
        CHECK(replay.outcome == kReplayOutcomeWin);
    }

    // --- R6b: malformed network ownership combinations stay rejected ------
    {
        SetupSettings setup = MakeNetworkSetup();
        auto init = [](BubbleGame& g) {
            ShapeNetworkBoard(g, 0);
            ShapeNetworkBoard(g, 1);
        };
        CapturedRecording net = RunLiveNetworkRound(renderer, setup, 6104u, init, IdleFor(2));
        CHECK(net.start.networkGame == 1);
        CHECK(net.start.playerCount == 2);

        auto rejected = [&](const RoundStartRecord& bad, const char* label) {
            BubbleGame playback(renderer);
            RestoreRoundStart(playback, bad);
            CHECK(BubbleGameTestAccess::sessionMode(playback) == BubbleGame::SessionMode::Live);
            if (BubbleGameTestAccess::sessionMode(playback) != BubbleGame::SessionMode::Live)
                std::fprintf(stderr, "  (%s was not rejected)\n", label);
        };
        // The record was captured with playerCount=2, so raising it to 3
        // leaves seat 2's board blob empty. R6b's gate now allows 3 seats, but
        // the decode loop rejects the missing blob before any ownership rule
        // runs -- a different reason than R6a's flat "playerCount != 2".
        { RoundStartRecord bad = net.start; bad.playerCount = 3; rejected(bad, "3-seat network (missing seat-2 board)"); }
        // R6c: the network bound is now 20 (the engine's real battle-royale
        // ceiling), so 21 trips the coarse range check before any board is
        // decoded -- rejected for the range, not a missing blob.
        { RoundStartRecord bad = net.start; bad.playerCount = 21; rejected(bad, "21-seat network (over the 20-seat bound)"); }
        // An owned seat past 0 must also be marked isBot; this claims we
        // simulate a remote human.
        { RoundStartRecord bad = net.start; bad.seatOwned[1] = 1; rejected(bad, "owned non-bot remote seat"); }
        // The inverse: flip seat 1's board-blob kBoardFlagIsBot bit (v3 flags
        // byte at index 1, value 0x04) without setting seatOwned[1], claiming
        // a remote peer is actually our own hosted bot. This is the mismatched
        // counterpart of the passing hosted-bot scenario below (which sets both
        // seatOwned[1]=1 and the isBot bit).
        {
            RoundStartRecord bad = net.start;
            CHECK(bad.startingBoards[1].size() > 1);
            if (bad.startingBoards[1].size() > 1) bad.startingBoards[1][1] |= 0x04;
            rejected(bad, "unowned bot seat");
        }
    }

    // --- R6b: SyncNetworkLevel() round-1 capture, as the leader -------------
    {
        // randomLevels=true makes NewGame() call SyncNetworkLevel(); the fake
        // room's creator matches our nick so IsLeader() is true and the leader
        // branch runs (SendBubble/SendNextBubble/SendTobeBubble, all no-ops with
        // no socket) instead of the joiner's blocking WaitForBubble().
        SetupSettings setup = MakeNetworkSetup();
        setup.randomLevels = true;
        GameRoom room;
        room.creator = "local_nick";
        room.started = true;

        CapturedRecording rec = RunLiveNetworkRound(renderer, setup, 6201u, nullptr,
                                                    FireThenIdle(PI / 2.0f, 30), &room);
        CHECK(rec.start.networkGame == 1);
        CHECK(rec.start.playerCount == 2);
        CHECK(rec.start.seatOwned[0] == 1);
        CHECK(rec.start.seatOwned[1] == 0);
        // SyncNetworkLevel()'s defining behavior is one generated layout applied
        // to every player, so both seats must carry the same non-empty cell
        // contents. LoadLevel() (the path RestoreRoundStart() rebuilds through)
        // would leave seat 1 empty, so this proves the board really came from
        // the sync.
        CHECK(rec.startBubbleIds.size() == 2);
        CHECK(rec.startBubbleIds[0] == rec.startBubbleIds[1]);
        int nonEmpty = 0;
        for (int id : rec.startBubbleIds[0]) if (id != -1) ++nonEmpty;
        CHECK(nonEmpty > 0);
        std::fprintf(stderr, "  [net-sync-leader] cells=%zu nonEmpty=%d steps=%zu\n",
                     rec.startBubbleIds[0].size(), nonEmpty, rec.liveHashes.size());
        RunAndCompare(renderer, "net-sync-leader", rec);
    }

    // --- R6b: network round 2+ continuity -----------------------------------
    {
        // Mirrors the local ReloadGame() continuity test: round 1 runs live
        // uncaptured to completion, then ReloadGame() starts round 2 and only
        // round 2 is captured. randomLevels=false makes both rounds take the
        // deterministic LoadLevel() path, so no socket is needed for round 2.
        singleBubbles.clear();
        malusBubbles.clear();
        SetupFakeNetworkClient();
        NetworkClient* nc = NetworkClient::Instance();
        nc->testGameDataSendCount = 0;

        SetupSettings setup = MakeNetworkSetup();
        setup.gameMode = GameMode::Race;
        setup.raceTarget = 1;  // seat 0's crafted pop wins round 1

        BubbleGame live(renderer);
        BubbleGameTestAccess::seedRng(live, 626262u);
        live.SetSessionMode(BubbleGame::SessionMode::Live);
        live.NewGame(setup);
        ShapeNetworkBoard(live, 0);
        ShapeNetworkBoard(live, 1);
        SetupAttackBoard(live, false);

        for (int i = 0; i < 300 && !BubbleGameTestAccess::gameFinish(live); ++i) {
            FrozenBubble::Instance()->deltaScale = 1.0f;
            BubbleArray& p0 = BubbleGameTestAccess::player(live, 0);
            p0.mouseTargetAngle = (i == 0) ? PI / 2.0f : -1.0f;
            p0.mouseFirePending = (i == 0);
            live.AdvanceSimulation();
        }
        CHECK(BubbleGameTestAccess::gameFinish(live) == true);
        CHECK(BubbleGameTestAccess::player(live, 0).winCount == 1);

        BubbleGameTestAccess::reloadGame(live, 1);
        ShapeNetworkBoard(live, 1);  // RemoveArray/LoadLevel leave seat 1 empty

        CapturedRecording rec;
        CaptureLiveSteps(live, 2, FireThenIdle(PI / 2.0f, 150), rec);
        CHECK(rec.start.networkGame == 1);
        CHECK(rec.start.startingWins[0] == 1);
        CHECK(rec.start.initialSimStep > 0);
        std::fprintf(stderr, "  [net-round2] startingWins=%d initialSimStep=%d steps=%zu\n",
                     rec.start.startingWins[0], rec.start.initialSimStep, rec.liveHashes.size());
        RunAndCompare(renderer, "net-round2", rec);
        ResetFakeNetworkClient();
    }

    // --- R6b: network-hosted bot -------------------------------------------
    {
        // A 2-seat room (seat 0 local, seat 1 a bot this client hosts) is the
        // minimal hosted-bot case; it is also the passing counterpart of the
        // "unowned bot seat" rejection above. NewGame() reads
        // setup.playerIsBot[1] to mark board 1 isBot, which OwnsArray() reports
        // as owned; RestoreRoundStart() accepts the matching (owned + isBot)
        // pair and Playback applies the bot's captured controls without ever
        // re-running DriveBot().
        SetupSettings setup = MakeNetworkSetup();
        setup.playerIsBot[1] = true;
        setup.botSkill = 1;
        auto init = [](BubbleGame& g) {
            ShapeNetworkBoard(g, 0);
            ShapeNetworkBoard(g, 1);
        };
        CapturedRecording rec = RunLiveNetworkRound(renderer, setup, 6301u, init, IdleFor(400));
        CHECK(rec.playerCount == 2);
        CHECK(rec.start.networkGame == 1);
        CHECK(rec.start.seatOwned[0] == 1);
        CHECK(rec.start.seatOwned[1] == 1);
        // The hosted bot really fired (its board is the only one speaking for
        // seat 1), so its captured controls are meaningful rather than empty.
        CHECK(rec.liveRFired[1] > 0);
        std::fprintf(stderr, "  [net-hosted-bot] fired0=%d fired1=%d seat1Bubbles=%d steps=%zu\n",
                     rec.liveRFired[0], rec.liveRFired[1], rec.liveSeatBubbleCounts[1],
                     rec.liveHashes.size());
        RunAndCompare(renderer, "net-hosted-bot", rec);
        ReplayResult replay = RunReplay(renderer, rec);
        // Playback never calls AdoptBots()/SeatBots(), and the restored network
        // instance must therefore hold no bot connections at all.
        CHECK(replay.botConnectionsEmpty == true);
        CHECK(replay.networkSends == 0);
    }

    // --- R6c: 7-seat battle-royale round (hidden mini boards) ---------------
    {
        // 7 seats lands NewGame()/ReloadGame() in the default: >5-player
        // battle-royale case: seat 0 keeps the full-size center board, seats
        // 1..6 get the 16-px mini-slot geometries. Before R6c's SeatCount()
        // split this scenario would silently capture/hash only seats 0-4 and
        // rec.start.playerCount would read 5, so rec.start.playerCount == 7
        // below is the assertion that the capture actually widened.
        //
        // Ownership is mixed on purpose: seat 0 is the local human; seats 1
        // and 2 are bots this client hosts (owned + isBot); seats 3-6 are
        // genuine remote peers (unowned + not bot). Seat 0's crafted top row
        // pops on a straight shot while all 7 seats are alive, so
        // SendMalusToOpponent() takes its >5-player random single-target
        // branch; seats 1/2 fire as hosted bots. The per-step canonical hash
        // covers every seat's board, so a divergence in any of the 7 is
        // caught, and because the random malus target consumes the shared
        // gameplay RNG, identical hash sequences require the same target to
        // have been picked on replay.
        SetupSettings setup = MakeNetworkSetup();
        setup.playerCount = 7;
        setup.playerIsBot[1] = true;
        setup.playerIsBot[2] = true;
        setup.botSkill = 1;
        auto init = [](BubbleGame& g) {
            ShapeNetworkBoard(g, 0, 32);
            for (int seat = 1; seat < 7; ++seat) ShapeNetworkBoard(g, seat, 16);
            SetupAttackBoard(g, false);  // seat 0's top row pops on a straight shot
        };
        std::vector<StepScript> script = IdleFor(400);
        script[0].aim = true;
        script[0].angle = PI / 2.0f;
        script[0].fire = true;

        CapturedRecording rec = RunLiveNetworkRound(renderer, setup, 6401u, init, script);
        CHECK(rec.start.networkGame == 1);
        // The whole point of the SeatCount() widening: all 7 seats captured.
        CHECK(rec.start.playerCount == 7);
        CHECK(rec.playerCount == 7);
        CHECK(rec.start.seatOwned[0] == 1);
        CHECK(rec.start.seatOwned[1] == 1);
        CHECK(rec.start.seatOwned[2] == 1);
        for (int seat = 3; seat < 7; ++seat) CHECK(rec.start.seatOwned[seat] == 0);

        // The >5-player attack really landed somewhere: exactly one living
        // opponent's local rRecv was credited by the random single-target
        // branch (the <=5 split branch would credit all six at once).
        int remoteRecv = 0, remoteRecvSeats = 0;
        for (int seat = 1; seat < 7; ++seat) {
            remoteRecv += rec.liveRRecv[seat];
            if (rec.liveRRecv[seat] > 0) ++remoteRecvSeats;
        }
        CHECK(rec.liveRSent[0] > 0);
        CHECK(remoteRecv > 0);
        CHECK(remoteRecvSeats == 1);
        // At least one hosted bot actually fired.
        CHECK(rec.liveRFired[1] > 0 || rec.liveRFired[2] > 0);
        std::fprintf(stderr,
                     "  [net-royale-malus] seats=%d sent0=%d remoteRecv=%d remoteSeats=%d botsFired=%d/%d steps=%zu\n",
                     (int)rec.start.playerCount, rec.liveRSent[0], remoteRecv, remoteRecvSeats,
                     rec.liveRFired[1], rec.liveRFired[2], rec.liveHashes.size());
        // RunAndCompare's CheckHashSequence compares the canonical hash of
        // every seat (CaptureCanonicalStateHash iterates all 7) at every step.
        RunAndCompare(renderer, "net-royale-malus", rec);
    }

    // --- R6c: late result-tail event after gameFinish is still captured -----
    {
        // The harness's historical CaptureLiveSteps() stopped the moment this
        // client's own gameFinish became true, so no prior scenario ever drove
        // a step past that point -- "late" remote events were untested. This
        // scenario passes stopAtFinish=false and keeps stepping: a remote 'F'
        // at step 10 sets gameFinish (network Classic cannot finish on a local
        // clear, so the finish is remote-announced, exactly as net-finish
        // does), then a remote 'S' stats sync from the still-open seat 1
        // arrives at step 20, after the round is over. The late 'S'
        // observably mutates hashed state (rFired/rPopped/rSent/rRecv/rKills/
        // rBlk are all in AppendBoardState), so matching hash sequences plus
        // the explicit seat-1 stats assertions prove it was applied in playback
        // too.
        SetupSettings setup = MakeNetworkSetup();
        auto init = [](BubbleGame& g) {
            ShapeNetworkBoard(g, 0);
            ShapeNetworkBoard(g, 1);
        };
        std::vector<StepScript> script = IdleFor(40);
        script[10].inboundWire.push_back("GAMEMSG:1:Fremote_nick");
        script[20].inboundWire.push_back("GAMEMSG:1:S3:2:1:0:0:0");

        CapturedRecording rec = RunLiveNetworkRound(renderer, setup, 6403u, init, script,
                                                    nullptr, /*stopAtFinish=*/false);
        // The harness really kept going past the finish step...
        CHECK(rec.liveFirstFinishStep == 10);
        CHECK(rec.liveHashes.size() == script.size());
        CHECK(static_cast<size_t>(rec.liveFirstFinishStep) + 1 < rec.liveHashes.size());
        // ...and the late 'S' really applied to the remote seat.
        CHECK(rec.liveRFired.size() == 2);
        CHECK(rec.liveRFired[1] == 3);
        CHECK(rec.liveRPopped[1] == 2);
        std::fprintf(stderr,
                     "  [net-late-stats] finishStep=%d totalSteps=%zu seat1Fired=%d seat1Popped=%d\n",
                     rec.liveFirstFinishStep, rec.liveHashes.size(),
                     rec.liveRFired[1], rec.liveRPopped[1]);
        RunAndCompare(renderer, "net-late-stats", rec);
        ReplayResult replay = RunReplay(renderer, rec);
        CHECK(replay.gameFinish == true);
        CHECK(replay.rFired.size() == 2);
        CHECK(replay.rFired[1] == 3);
        CHECK(replay.rPopped[1] == 2);
    }

    // --- R6c: mid-round departure ('l') in a 7-seat royal round ------------
    {
        // A departure in a wide battle-royale room: remote seat 6 announces it
        // left mid-round (gameFinish still false). HandlePlayerDeparture() sets
        // LEFT, decrements connectedPlayerCount, re-ranks the view (not hashed,
        // and deterministic) and calls ResolveRoundOutcome() -- which, with 6
        // seats still alive, does NOT end the round. The replay must reproduce
        // the same lifecycle, connected count and hash sequence.
        SetupSettings setup = MakeNetworkSetup();
        setup.playerCount = 7;
        auto init = [](BubbleGame& g) {
            ShapeNetworkBoard(g, 0, 32);
            for (int seat = 1; seat < 7; ++seat) ShapeNetworkBoard(g, seat, 16);
        };
        std::vector<StepScript> script = IdleFor(60);
        // lobbyId 6 == seat 6 (SetupFakeNetworkClient assigns array index i to
        // lobby id i). 'l' is a connection-level opcode but still applied and
        // captured by ProcessNetworkMessages().
        script[5].inboundWire.push_back("GAMEMSG:6:l");

        CapturedRecording rec = RunLiveNetworkRound(renderer, setup, 6404u, init, script);
        CHECK(rec.start.playerCount == 7);
        CHECK(rec.liveConnectedPlayerCount == 6);  // 7 seats - one departure
        CHECK(rec.livePlayerStates.size() == 7);
        CHECK(rec.livePlayerStates[6] == (int)BubbleArray::PlayerState::LEFT);
        CHECK(rec.livePlayerStates[0] == (int)BubbleArray::PlayerState::ALIVE);
        // One departure from 7 does not end the round.
        CHECK(rec.end.complete == 0);
        std::fprintf(stderr,
                     "  [net-royale-departure] seats=%d connected=%d seat6State=%d steps=%zu\n",
                     (int)rec.start.playerCount, rec.liveConnectedPlayerCount,
                     rec.livePlayerStates[6], rec.liveHashes.size());
        RunAndCompare(renderer, "net-royale-departure", rec);
        ReplayResult replay = RunReplay(renderer, rec);
        CHECK(replay.connectedPlayerCount == rec.liveConnectedPlayerCount);
        CHECK(replay.playerStates.size() == 7);
        CHECK(replay.playerStates[6] == (int)BubbleArray::PlayerState::LEFT);
        CHECK(replay.gameFinish == false);
    }

    // --- A malformed board blob is rejected safely, not read out of bounds --
    {
        BubbleGame source(renderer);
        BubbleGameTestAccess::seedRng(source, 7u);
        source.NewGame(solo);
        RoundStartRecord truncated = CaptureRoundStart(source);
        truncated.startingBoards[0].resize(3);  // cut mid-header

        BubbleGame playback(renderer);
        RestoreRoundStart(playback, truncated);  // must return, not crash
        // Rejected before SetSessionMode(): the instance is untouched.
        CHECK(BubbleGameTestAccess::sessionMode(playback) == BubbleGame::SessionMode::Live);
    }

    // --- R5a/R5b: unsupported records rejected; Timed/training accepted -----
    {
        BubbleGame source(renderer);
        BubbleGameTestAccess::seedRng(source, 123u);
        source.NewGame(solo);
        RoundStartRecord good = CaptureRoundStart(source);

        auto rejected = [&](const RoundStartRecord& bad, const char* label) {
            BubbleGame playback(renderer);
            RestoreRoundStart(playback, bad);
            CHECK(BubbleGameTestAccess::sessionMode(playback) == BubbleGame::SessionMode::Live);
            if (BubbleGameTestAccess::sessionMode(playback) != BubbleGame::SessionMode::Live)
                std::fprintf(stderr, "  (%s was not rejected)\n", label);
        };
        { RoundStartRecord bad = good; bad.playerCount = 6; rejected(bad, "6 seats"); }
        { RoundStartRecord bad = good; bad.networkGame = 1; rejected(bad, "network"); }
        { RoundStartRecord bad = good; bad.gameMode = (uint8_t)GameMode::Clear; rejected(bad, "Clear"); }
        // R5b: Timed and training are now accepted (the scenarios above replay
        // them end-to-end), so the gate must reach Playback rather than reject.
        {
            RoundStartRecord timed = good;
            timed.gameMode = (uint8_t)GameMode::Timed;
            BubbleGame playback(renderer);
            RestoreRoundStart(playback, timed);
            CHECK(BubbleGameTestAccess::sessionMode(playback) == BubbleGame::SessionMode::Playback);
        }
        {
            RoundStartRecord training = good;
            CHECK(!training.levelLayout.empty());
            if (!training.levelLayout.empty()) training.levelLayout[1] |= 0x01;  // rules mpTraining flag
            BubbleGame playback(renderer);
            RestoreRoundStart(playback, training);
            CHECK(BubbleGameTestAccess::sessionMode(playback) == BubbleGame::SessionMode::Playback);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    std::filesystem::remove_all(scratch, ec);

    if (failures == 0) std::printf("bubblegame replay tests passed\n");
    return failures == 0 ? 0 : 1;
}
