# Round and Match State Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix BUG-018, BUG-019, BUG-021, BUG-022, BUG-023, BUG-024, and BUG-025 with deterministic round/match resolution, a reachable local victories limit, parity-correct chain targeting, and collision-safe high-delta movement.

**Architecture:** Keep player lifecycle mutation separate from one idempotent multiplayer outcome resolver. Route loss sweeps, clear wins, departures, and remote finish notifications through that resolver; retain focused board and movement helpers for chain topology and bounded collision sampling. Register a production-object CTest executable so every defect is reproduced before its fix and stays covered afterward.

**Tech Stack:** C++17, SDL3/SDL3_image/SDL3_mixer/SDL3_ttf, CMake/CTest, AddressSanitizer, UndefinedBehaviorSanitizer.

## Global Constraints

- Do not change the line-based network protocol or require synchronized client upgrades.
- Preserve `LEFT` players across consecutive rounds.
- Preserve the existing meaning of `victoriesLimit == 0` as unlimited.
- Preserve the configured game speed and logical 640x480 geometry.
- Keep changes limited to this defect cluster and its regression coverage; do not fold unrelated gameplay or menu cleanup into the work.
- Every production behavior change must first have a focused test that is observed failing for the intended reason.

---

## File map

- Create `tests/bubblegame_rules_test.cpp`: permanent production-object regression harness for BUG-018/019/021/022/023/024/025.
- Create `src/localmultiplayer_settings.h`: local multiplayer option value table and settings-builder interface.
- Create `src/localmultiplayer_settings.cpp`: settings-builder implementation used by the active menu path.
- Modify `CMakeLists.txt`: name the reusable game-source list and register `bubblegame-rules-test`.
- Modify `src/bubblegame.h`: add continuation state, outcome state, resolver interfaces, and the test-access friend seam.
- Modify `src/bubblegame.cpp`: reset the added per-round outcome state.
- Modify `src/bubblegame_state.cpp`: centralize outcome accounting and batch danger losses.
- Modify `src/bubblegame_render.cpp`: call the shared danger sweep and use bounded aim-guide sampling.
- Modify `src/bubblegame_net.cpp`: route `F` and `l` through the shared resolver.
- Modify `src/bubblegame_board.cpp`: correct chain parity and restore cross-chain validation.
- Modify `src/bubblegame_internal.h`: parameterize movement scale and expose the bounded substep calculation.
- Modify `src/bubblegame_shooter.cpp`: evaluate ceiling/collision after every bounded movement substep.
- Modify `src/mainmenu.h`: remove dead 2P state and add the active local victories selection.
- Modify `src/mainmenu.cpp`: propagate negotiated departure behavior and use the local settings builder.
- Modify `src/mainmenu_input.cpp`: remove dead 2P input and add local victories navigation.
- Modify `src/mainmenu_panels.cpp`: remove dead 2P rendering and render the local victories row.
- Modify `docs/audit/REMEDIATION_STATUS.md`: mark the seven defects fixed and move the next-order pointer.
- Modify `docs/MANUAL_TEST_CHECKLIST.md`: record visual/multi-client checks not executed automatically.
- Modify `CHANGELOG.md`: document user-visible behavior corrections.

---

### Task 1: Production gameplay harness and centralized round outcomes (BUG-018, BUG-019, BUG-024)

**Files:**
- Create: `tests/bubblegame_rules_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/bubblegame.h:441-610`
- Modify: `src/bubblegame.cpp:337-415,1040-1070`
- Modify: `src/bubblegame_board.cpp` (mechanical SFX seam only in this task)
- Modify: `src/bubblegame_state.cpp:438-770`
- Modify: `src/bubblegame_render.cpp:995-1040`
- Modify: `src/bubblegame_net.cpp:402-478`
- Modify: `src/bubblegame_shooter.cpp` (mechanical SFX seam only in this task)

**Interfaces:**
- Produces: `enum class RoundWinCause { Elimination, Clear, Departure, Remote }`.
- Produces: `void ApplyPlayerLoss(BubbleArray&)` for lifecycle/animation/kill side effects only.
- Produces: `void ResolveDangerZoneLosses()` to batch every danger loss before resolution.
- Produces: `void ResolveRoundOutcome(int assertedWinnerIdx = -1, RoundWinCause cause = RoundWinCause::Elimination, bool sendNetworkFinish = false)`.
- Produces: `void CommitRoundWin(int winnerIdx, RoundWinCause cause, bool sendNetworkFinish)` and `void FinishRoundAsDraw()`.
- Produces: `int roundWinnerIdx`, reset to `-1` in both `NewGame` and `ReloadGame`.
- Produces: null-safe `void PlaySFX(const char*)`, allowing a production-object
  rules test without constructing settings/audio singletons or touching user preferences.
- Consumes later: departure handling in Task 2 calls `ResolveRoundOutcome`; local victories in Task 3 rely on its canonical `winCount` enforcement.

- [ ] **Step 1: Factor the reusable production source list and add the test seam**

In `CMakeLists.txt`, move every current executable source except `src/main.cpp` and `${RES_FILES}` into `FROZEN_BUBBLE_CORE_SOURCES`, then build the app from that list:

```cmake
set(FROZEN_BUBBLE_CORE_SOURCES
    src/frozenbubble.cpp
    src/menubutton.cpp
    src/mainmenu.cpp
    src/mainmenu_input.cpp
    src/mainmenu_netpanel.cpp
    src/mainmenu_panels.cpp
    src/mainmenu_server.cpp
    src/gamesettings.cpp
    src/audiomixer.cpp
    src/shaderstuff.cpp
    src/bubblegame.cpp
    src/bubblegame_board.cpp
    src/bubblegame_input.cpp
    src/bubblegame_level.cpp
    src/bubblegame_net.cpp
    src/bubblegame_render.cpp
    src/bubblegame_shooter.cpp
    src/bubblegame_state.cpp
    src/netview.cpp
    src/netteams.cpp
    src/roundstats_color.cpp
    src/transitionmanager.cpp
    src/ttftext.cpp
    src/highscoremanager.cpp
    ${NETWORK_CLIENT_SRC}
    src/logger.cpp
    src/platform.cpp
)

add_executable(frozen-bubble-sdl3
    src/main.cpp
    ${FROZEN_BUBBLE_CORE_SOURCES}
    ${RES_FILES}
)
```

Inside the existing native `BUILD_TESTING` block, register a second executable from the same production sources:

```cmake
add_executable(bubblegame-rules-test
    tests/bubblegame_rules_test.cpp
    ${FROZEN_BUBBLE_CORE_SOURCES}
)
set_property(TARGET bubblegame-rules-test PROPERTY CXX_STANDARD 17)
set_property(TARGET bubblegame-rules-test PROPERTY CXX_EXTENSIONS OFF)
target_include_directories(bubblegame-rules-test PRIVATE src)
target_compile_definitions(bubblegame-rules-test PRIVATE
    FROZEN_BUBBLE_TEST_ACCESS
    DATA_DIR="${ASSET_PATH}"
    APP_VERSION="v${PROJECT_VERSION}"
)
target_link_libraries(bubblegame-rules-test PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_mixer::SDL3_mixer
    SDL3_ttf::SDL3_ttf
    iniparser-static
)
if(WIN32 OR MINGW)
    target_link_libraries(bubblegame-rules-test PRIVATE ws2_32)
endif()
add_test(NAME bubblegame-rules-test COMMAND bubblegame-rules-test)
```

In `BubbleGame`'s private section, add only the friend declaration; the accessor itself remains test-only:

```cpp
#ifdef FROZEN_BUBBLE_TEST_ACCESS
    friend struct BubbleGameTestAccess;
#endif
```

Add this ordinary null-safe wrapper in the same private section:

```cpp
void PlaySFX(const char* id) {
    if (audMixer != nullptr) audMixer->PlaySFX(id);
}
```

Mechanically replace all 19 `audMixer->PlaySFX(...)` calls in
`src/bubblegame*.cpp` with `PlaySFX(...)`. Production behavior is unchanged
after `NewGame` assigns `audMixer`; the test can leave it null. Verify the seam
before writing behavioral tests:

```bash
rg -n 'audMixer->PlaySFX' src/bubblegame*.cpp
```

Expected: no matches. Do not construct `AudioMixer` or call
`GameSettings::ReadSettings` in this test: either path would couple CTest to
the user's real preferences directory.

- [ ] **Step 2: Write failing clear-mode, simultaneous-loss, and remote-clear tests**

Start `tests/bubblegame_rules_test.cpp` with real production objects and literal board fixtures:

```cpp
#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "platform.h"

#include <cstdio>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct BubbleGameTestAccess {
    static SetupSettings& settings(BubbleGame& game) { return game.currentSettings; }
    static BubbleArray& player(BubbleGame& game, int idx) { return game.bubbleArrays[idx]; }
    static bool finished(const BubbleGame& game) { return game.gameFinish; }
    static bool lost(const BubbleGame& game) { return game.gameLost; }
    static bool cleared(const BubbleGame& game) { return game.wonByClearing; }
    static bool matchOver(const BubbleGame& game) { return game.gameMatchOver; }
    static int winsP1(const BubbleGame& game) { return game.winsP1; }
    static int winsP2(const BubbleGame& game) { return game.winsP2; }
    static void check(BubbleGame& game, int idx) { game.CheckGameState(game.bubbleArrays[idx]); }

    static void reset(BubbleGame& game, int players, bool network, bool clearMode) {
        singleBubbles.clear();
        malusBubbles.clear();
        game.currentSettings = {};
        game.currentSettings.playerCount = players;
        game.currentSettings.networkGame = network;
        game.currentSettings.clearMode = clearMode;
        game.gameFinish = game.gameLost = game.gameMatchOver = false;
        game.wonByClearing = false;
        game.connectedPlayerCount = players;
        game.winsP1 = game.winsP2 = 0;
        for (int i = 0; i < players; ++i) {
            BubbleArray& p = game.bubbleArrays[i];
            p.playerAssigned = i;
            p.playerState = BubbleArray::PlayerState::ALIVE;
            p.winCount = 0;
            p.mpWinner = false;
            p.lastAttackerIdx = -1;
            p.compressionDisabled = true;
            p.dangerZone = 12;
            for (int row = 0; row < 13; ++row)
                p.bubbleMap[row].assign((row % 2 == 0) ? 8 : 7, Bubble{});
        }
    }
};

static void PutDangerBubble(BubbleArray& player, int col = 0) {
    player.bubbleMap[12][col].bubbleId = 0;
}

int main() {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow(
        "bubblegame-rules-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window != nullptr) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    {
        BubbleGame game(renderer);

        BubbleGameTestAccess::reset(game, 2, false, false);
        BubbleGameTestAccess::check(game, 0);
        CHECK(!BubbleGameTestAccess::finished(game));

        BubbleGameTestAccess::reset(game, 2, false, true);
        BubbleGameTestAccess::check(game, 0);
        CHECK(BubbleGameTestAccess::finished(game));
        CHECK(BubbleGameTestAccess::cleared(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);

        BubbleGameTestAccess::reset(game, 2, false, false);
        PutDangerBubble(BubbleGameTestAccess::player(game, 0));
        PutDangerBubble(BubbleGameTestAccess::player(game, 1));
        BubbleGameTestAccess::check(game, 0);
        CHECK(BubbleGameTestAccess::finished(game));
        CHECK(BubbleGameTestAccess::lost(game));
        CHECK(BubbleGameTestAccess::player(game, 0).winCount == 0);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 0);
        CHECK(BubbleGameTestAccess::winsP1(game) == 0);
        CHECK(BubbleGameTestAccess::winsP2(game) == 0);

        BubbleGameTestAccess::reset(game, 2, true, true);
        BubbleGameTestAccess::check(game, 1);
        CHECK(BubbleGameTestAccess::player(game, 1).winCount == 1);
        CHECK(BubbleGameTestAccess::winsP2(game) == 1);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (failures == 0) std::printf("bubblegame rules tests passed\n");
    return failures == 0 ? 0 : 1;
}
```

The production change each assertion catches is respectively: removing the `clearMode` gate, resolving inside the loss loop, or restoring the remote-board special case that skips accounting.

- [ ] **Step 3: Run the new target and verify RED**

Run:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target bubblegame-rules-test --parallel
ctest --test-dir build -R '^bubblegame-rules-test$' --output-on-failure
```

Expected: one run reports all three intended behavioral failures: Classic mode finishes on an empty board, simultaneous final loss retains credited counters, and a remote clear does not increment the remote player's canonical win count.

- [ ] **Step 4: Implement the shared resolver and batched sweep**

Add these private declarations and state to `BubbleGame`:

```cpp
enum class RoundWinCause { Elimination, Clear, Departure, Remote };

int roundWinnerIdx = -1;

void ApplyPlayerLoss(BubbleArray& player);
void ResolveDangerZoneLosses();
void ResolveRoundOutcome(
    int assertedWinnerIdx = -1,
    RoundWinCause cause = RoundWinCause::Elimination,
    bool sendNetworkFinish = false);
void CommitRoundWin(int winnerIdx, RoundWinCause cause, bool sendNetworkFinish);
void FinishRoundAsDraw();
bool HasDepartedPlayers() const;
int CountConnectedPlayers() const;
int CountConnectedTeams() const;
```

Implement the decision logic in `bubblegame_state.cpp` with this exact ordering:

```cpp
void BubbleGame::ResolveRoundOutcome(int assertedWinnerIdx,
                                     RoundWinCause cause,
                                     bool sendNetworkFinish) {
    if (gameFinish) {
        // A later clear observation may enrich the presentation metadata, but
        // no later observation may replace or re-credit a committed outcome.
        if (assertedWinnerIdx >= 0)
            CommitRoundWin(assertedWinnerIdx, cause, false);
        return;
    }

    if (assertedWinnerIdx >= 0) {
        CommitRoundWin(assertedWinnerIdx, cause, sendNetworkFinish);
        return;
    }

    const int living = CountLivingPlayers();
    if (living == 0) {
        FinishRoundAsDraw();
        return;
    }

    const bool onePlayer = living == 1;
    const bool oneTeam = currentSettings.teamMode && CountLivingTeams() == 1;
    if (!onePlayer && !oneTeam) return;

    for (int i = 0; i < currentSettings.playerCount; ++i) {
        if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) {
            CommitRoundWin(i, cause, sendNetworkFinish);
            return;
        }
    }
}
```

`CommitRoundWin` must first reconcile duplicate observations, then perform side effects once:

```cpp
if (gameFinish) {
    if (roundWinnerIdx == winnerIdx && cause == RoundWinCause::Clear)
        wonByClearing = true;
    return;
}

gameFinish = true;
roundWinnerIdx = winnerIdx;
wonByClearing = cause == RoundWinCause::Clear;
```

Build the credited/presented winner list deterministically: it contains only
`winnerIdx` outside Team Mode, and every `ALIVE` player on
`currentSettings.playerTeams[winnerIdx]` in Team Mode. Mark every entry
`mpWinner` and start animation 10. Unless Task 2's abandoned-round condition
suppresses credit, increment each entry's canonical `winCount` exactly once,
increment only the representative's legacy `winsP1`/`winsP2` bucket, call
`Update2PText()` and `UpdatePlayerNameWinText()`, and set `gameMatchOver` when
any credited entry reaches a positive `victoriesLimit`. Preserve the existing
panel rectangle and clear/elimination SFX choices. Send the existing
`F{nickname}` payload only when `sendNetworkFinish` is true and the winner has
not left. `FinishRoundAsDraw` sets `gameFinish=true`, `gameLost=true`,
`roundWinnerIdx=-1`, and does not touch counters.

Move lifecycle mutation, lose SFX, kill credit, target reset, and view ranking
from `HandlePlayerLoss` into `ApplyPlayerLoss`. Implement
`ResolveDangerZoneLosses` in two phases: loop across the active prefix and apply
every currently alive danger loss, then call `ResolveRoundOutcome` once if the
loop changed any player. Replace both danger sweeps (`CheckGameState` and
`bubblegame_render.cpp`) with this helper. Preserve the single-player loss
branch separately. The batched multiplayer call must retain elimination
broadcasting:

```cpp
if (changed) {
    ResolveRoundOutcome(
        -1, RoundWinCause::Elimination, currentSettings.networkGame);
}
```

Gate the multiplayer clear branch in `CheckGameState`:

```cpp
if (bArray.allClear() &&
    (currentSettings.playerCount < 2 || currentSettings.clearMode)) {
```

For a multiplayer clear, call:

```cpp
ResolveRoundOutcome(
    bArray.playerAssigned,
    RoundWinCause::Clear,
    currentSettings.networkGame && bArray.playerAssigned == 0);
```

Replace the body of the network `F` accounting guard with:

```cpp
RoundWinCause cause =
    currentSettings.clearMode && bubbleArrays[winnerPlayer].allClear()
        ? RoundWinCause::Clear
        : RoundWinCause::Remote;
ResolveRoundOutcome(winnerPlayer, cause, false);
```

Reset `roundWinnerIdx` beside `wonByClearing` in both `NewGame` and `ReloadGame`.
Also add `game.roundWinnerIdx = -1;` to the test accessor's `reset` method now
that the production member exists.

- [ ] **Step 5: Add idempotency and team preservation assertions**

Extend the test accessor with a direct announced-winner call:

```cpp
static void announce(BubbleGame& game, int winner, bool clear) {
    game.ResolveRoundOutcome(
        winner,
        clear ? BubbleGame::RoundWinCause::Clear : BubbleGame::RoundWinCause::Remote,
        false);
}
```

Add the four order/idempotency fixtures:

```cpp
BubbleGameTestAccess::reset(game, 2, true, true);
BubbleGameTestAccess::announce(game, 1, false);
BubbleGameTestAccess::check(game, 1);
CHECK(BubbleGameTestAccess::player(game, 1).winCount == 1);
CHECK(BubbleGameTestAccess::cleared(game));

BubbleGameTestAccess::reset(game, 2, true, true);
BubbleGameTestAccess::check(game, 1);
BubbleGameTestAccess::announce(game, 1, false);
CHECK(BubbleGameTestAccess::player(game, 1).winCount == 1);

BubbleGameTestAccess::reset(game, 2, true, false);
BubbleGameTestAccess::announce(game, 1, false);
BubbleGameTestAccess::announce(game, 1, false);
CHECK(BubbleGameTestAccess::player(game, 1).winCount == 1);

BubbleGameTestAccess::reset(game, 4, false, false);
SetupSettings& teamSettings = BubbleGameTestAccess::settings(game);
teamSettings.teamMode = true;
teamSettings.playerTeams[0] = teamSettings.playerTeams[2] = 1;
teamSettings.playerTeams[1] = teamSettings.playerTeams[3] = 2;
BubbleGameTestAccess::announce(game, 0, false);
CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
CHECK(BubbleGameTestAccess::player(game, 2).winCount == 1);
CHECK(BubbleGameTestAccess::player(game, 1).winCount == 0);
CHECK(BubbleGameTestAccess::player(game, 3).winCount == 0);
```

- [ ] **Step 6: Verify GREEN and commit**

Run:

```bash
cmake --build build --target bubblegame-rules-test frozen-bubble-sdl3 --parallel
ctest --test-dir build -R '^bubblegame-rules-test$' --output-on-failure
ctest --test-dir build --output-on-failure
```

Expected: the new target passes and the existing suite remains green, with the two sanitizer-only server tests skipped in the ordinary build.

Commit:

```bash
git add CMakeLists.txt tests/bubblegame_rules_test.cpp src/bubblegame.h src/bubblegame.cpp src/bubblegame_board.cpp src/bubblegame_state.cpp src/bubblegame_render.cpp src/bubblegame_net.cpp src/bubblegame_shooter.cpp
git commit -m "fix: centralize multiplayer round outcomes (BUG-018,019,024)"
```

---

### Task 2: Departure continuation, teams, and match limits (BUG-021)

**Files:**
- Modify: `tests/bubblegame_rules_test.cpp`
- Modify: `src/bubblegame.h:267-290,580-615`
- Modify: `src/bubblegame_state.cpp:438-650`
- Modify: `src/bubblegame_net.cpp:530-600`
- Modify: `src/mainmenu.cpp:558-592`

**Interfaces:**
- Produces: `SetupSettings::continueWhenPlayersLeave`, default `true`.
- Produces: `void HandlePlayerDeparture(int playerIdx)`.
- Consumes: Task 1's `ResolveRoundOutcome(..., RoundWinCause::Departure, ...)`, `CountConnectedPlayers`, and `CountConnectedTeams`.

- [ ] **Step 1: Write failing departure tests**

Add this accessor and cases to the gameplay test:

```cpp
static void depart(BubbleGame& game, int idx) { game.HandlePlayerDeparture(idx); }
```

Test these literal scenarios independently after `reset`:

```cpp
// Continue enabled: the sole survivor receives one win, but a 2P match cannot restart alone.
BubbleGameTestAccess::reset(game, 2, true, false);
BubbleGameTestAccess::settings(game).continueWhenPlayersLeave = true;
BubbleGameTestAccess::depart(game, 1);
CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
CHECK(BubbleGameTestAccess::matchOver(game));

// Continue disabled: show the survivor, do not credit the abandoned round.
BubbleGameTestAccess::reset(game, 2, true, false);
BubbleGameTestAccess::settings(game).continueWhenPlayersLeave = false;
BubbleGameTestAccess::depart(game, 1);
CHECK(BubbleGameTestAccess::player(game, 0).mpWinner);
CHECK(BubbleGameTestAccess::player(game, 0).winCount == 0);
CHECK(BubbleGameTestAccess::matchOver(game));

// Four players on two teams: departure leaving one team resolves both teammates as winners.
BubbleGameTestAccess::reset(game, 4, true, false);
SetupSettings& teamSettings = BubbleGameTestAccess::settings(game);
teamSettings.continueWhenPlayersLeave = true;
teamSettings.teamMode = true;
teamSettings.playerTeams[0] = teamSettings.playerTeams[2] = 1;
teamSettings.playerTeams[1] = teamSettings.playerTeams[3] = 2;
BubbleGameTestAccess::depart(game, 1);
BubbleGameTestAccess::depart(game, 3);
CHECK(BubbleGameTestAccess::player(game, 0).winCount == 1);
CHECK(BubbleGameTestAccess::player(game, 2).winCount == 1);

// A credited departure win reaches the configured limit.
BubbleGameTestAccess::reset(game, 2, true, false);
SetupSettings& limitSettings = BubbleGameTestAccess::settings(game);
limitSettings.continueWhenPlayersLeave = true;
limitSettings.victoriesLimit = 2;
BubbleGameTestAccess::player(game, 0).winCount = 1;
BubbleGameTestAccess::depart(game, 1);
CHECK(BubbleGameTestAccess::player(game, 0).winCount == 2);
CHECK(BubbleGameTestAccess::matchOver(game));
```

- [ ] **Step 2: Run the focused test and verify RED**

Run the Task 1 focused command. Expected: compilation fails because `continueWhenPlayersLeave` and `HandlePlayerDeparture` do not exist. This is the intended missing-contract failure.

- [ ] **Step 3: Propagate and enforce departure state**

Append this field at the end of `SetupSettings` so existing positional aggregate
initializers retain their current meaning:

```cpp
bool continueWhenPlayersLeave = true;
```

In network `SetupNewGame(4)`, copy the negotiated menu field:

```cpp
ns.continueWhenPlayersLeave = continueWhenPlayersLeave;
```

Implement `CountConnectedPlayers` as the number of active-prefix arrays not in `LEFT`, and `CountConnectedTeams` as distinct configured teams among those non-`LEFT` arrays. Implement `HasDepartedPlayers` as any active-prefix `LEFT` state.

Implement `HandlePlayerDeparture` to validate the index, ignore duplicate departures, mark `LEFT`, play the lose animation, clear target/attacker references, decrement `connectedPlayerCount` once, rerank the view, then call:

```cpp
ResolveRoundOutcome(-1, RoundWinCause::Departure, false);
```

In `CommitRoundWin`, compute:

```cpp
const bool abandonedRound =
    !currentSettings.continueWhenPlayersLeave && HasDepartedPlayers();
const bool insufficientOpponents = currentSettings.teamMode
    ? CountConnectedTeams() < 2
    : CountConnectedPlayers() < 2;
```

When `abandonedRound` is true, set winner flags/presentation but do not increment `winCount`, `winsP1`, or `winsP2`. Set `gameMatchOver` when `abandonedRound`, `insufficientOpponents`, or a credited winner reaches the positive victories limit.

Replace the network `l` handler's duplicated winner body with `HandlePlayerDeparture(playerIdx)`; retain only unknown-player logging and the reduced ready-threshold check around it.

- [ ] **Step 4: Verify and commit**

Run the focused test and full CTest suite, then commit:

```bash
git add tests/bubblegame_rules_test.cpp src/bubblegame.h src/bubblegame_state.cpp src/bubblegame_net.cpp src/mainmenu.cpp
git commit -m "fix: honor departure match rules (BUG-021)"
```

---

### Task 3: Reachable local victories-limit option (BUG-023)

**Files:**
- Create: `src/localmultiplayer_settings.h`
- Create: `src/localmultiplayer_settings.cpp`
- Modify: `tests/bubblegame_rules_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/mainmenu.h:55-145,250-280`
- Modify: `src/mainmenu.cpp:295-375,517-640`
- Modify: `src/mainmenu_input.cpp:130-150,626-815,1610-1640`
- Modify: `src/mainmenu_panels.cpp:65-90,240-390`

**Interfaces:**
- Produces: `inline constexpr std::array<int, 18> kVictoriesLimits`.
- Produces: `struct LocalMultiplayerOptions` and `SetupSettings BuildLocalMultiplayerSettings(const LocalMultiplayerOptions&)`.
- Produces: `int localMPVictoriesIndex = 5` in `MainMenu`.
- Removes: every legacy `showing2PPanel`, `TPPanelRender`, `TwoPPanelKey`, `twoPlayer*`, and `SetupNewGame(2)` symbol.

- [ ] **Step 1: Write the failing local-settings propagation test**

Include the new header in the test and add:

```cpp
LocalMultiplayerOptions options;
options.playerCount = 4;
options.chainReaction = true;
options.victoriesIndex = 15;
options.clearMode = false;
options.disableMalus = true;
options.teamMode = true;
options.colors = {5, 6, 7, 8, 8};
options.aimGuide = {true, false, true, false, false};

SetupSettings built = BuildLocalMultiplayerSettings(options);
CHECK(built.localMultiplayer);
CHECK(built.playerCount == 4);
CHECK(built.victoriesLimit == 30);
CHECK(built.playerTeams[0] == 1 && built.playerTeams[1] == 2);
CHECK(built.playerColors[2] == 7);
CHECK(built.aimGuide[2]);
```

Add local two-player and four-player resolver cases proving enforcement uses the
built setting:

```cpp
LocalMultiplayerOptions limited2P;
limited2P.playerCount = 2;
limited2P.victoriesIndex = 2;  // first to 2
BubbleGameTestAccess::reset(game, 2, false, false);
BubbleGameTestAccess::settings(game) =
    BuildLocalMultiplayerSettings(limited2P);
BubbleGameTestAccess::player(game, 0).winCount = 1;
BubbleGameTestAccess::announce(game, 0, false);
CHECK(BubbleGameTestAccess::player(game, 0).winCount == 2);
CHECK(BubbleGameTestAccess::matchOver(game));

LocalMultiplayerOptions limited4P;
limited4P.playerCount = 4;
limited4P.victoriesIndex = 1;  // first to 1
BubbleGameTestAccess::reset(game, 4, false, false);
BubbleGameTestAccess::settings(game) =
    BuildLocalMultiplayerSettings(limited4P);
BubbleGameTestAccess::announce(game, 3, false);
CHECK(BubbleGameTestAccess::player(game, 3).winCount == 1);
CHECK(BubbleGameTestAccess::matchOver(game));

LocalMultiplayerOptions unlimited;
unlimited.playerCount = 2;
unlimited.victoriesIndex = 0;
BubbleGameTestAccess::reset(game, 2, false, false);
BubbleGameTestAccess::settings(game) =
    BuildLocalMultiplayerSettings(unlimited);
BubbleGameTestAccess::player(game, 0).winCount = 99;
BubbleGameTestAccess::announce(game, 0, false);
CHECK(BubbleGameTestAccess::player(game, 0).winCount == 100);
CHECK(!BubbleGameTestAccess::matchOver(game));
```

- [ ] **Step 2: Run and verify RED**

Run the focused build. Expected: compilation fails because `localmultiplayer_settings.h` and its builder do not exist.

- [ ] **Step 3: Implement the focused builder**

Create the header:

```cpp
#ifndef LOCALMULTIPLAYER_SETTINGS_H
#define LOCALMULTIPLAYER_SETTINGS_H

#include "bubblegame.h"
#include <array>

inline constexpr std::array<int, 18> kVictoriesLimits = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 20, 30, 50, 100
};

struct LocalMultiplayerOptions {
    int playerCount = 2;
    bool chainReaction = true;
    bool noCompression = false;
    bool clearMode = false;
    bool disableMalus = false;
    bool teamMode = false;
    int victoriesIndex = 5;
    std::array<int, 5> colors = {8, 8, 8, 8, 8};
    std::array<bool, 5> aimGuide = {};
};

SetupSettings BuildLocalMultiplayerSettings(const LocalMultiplayerOptions& options);

#endif
```

Implement `src/localmultiplayer_settings.cpp` as the UI-independent conversion
point and add it to `FROZEN_BUBBLE_CORE_SOURCES`:

```cpp
#include "localmultiplayer_settings.h"

#include <algorithm>

SetupSettings BuildLocalMultiplayerSettings(
    const LocalMultiplayerOptions& options) {
    SetupSettings settings;
    settings.playerCount = std::clamp(options.playerCount, 2, 4);
    settings.chainReaction = options.chainReaction;
    settings.randomLevels = true;
    settings.localMultiplayer = true;
    settings.clearMode = options.clearMode;
    settings.disableMalus = options.disableMalus;
    settings.teamMode = options.teamMode;
    settings.teamCount = 2;

    const int victoriesIndex = std::clamp(
        options.victoriesIndex, 0,
        static_cast<int>(kVictoriesLimits.size()) - 1);
    settings.victoriesLimit = kVictoriesLimits[victoriesIndex];

    for (int i = 0; i < 5; ++i) {
        settings.playerTeams[i] = options.teamMode ? (i % 2) + 1 : i + 1;
        settings.playerColors[i] = options.colors[i];
        settings.disableCompression[i] = options.noCompression;
        settings.aimGuide[i] = options.aimGuide[i];
    }
    return settings;
}
```

- [ ] **Step 4: Make the active panel the sole local setup path**

Add a `Victories limit` row at local menu index 6. Shift aim-guide rows to `7..7+N-1`, color rows to `7+N..7+2N-1`, and Start to `7+2N`. Left decrements `localMPVictoriesIndex` with wrap from 0 to 17; Right/Enter increments with wrap from 17 to 0. Render the selected label using `kVictoriesLimits`, spelling index 0 as `none (unlimited)`.

Build `LocalMultiplayerOptions` from the active panel fields in `SetupNewGame(7)` and pass `BuildLocalMultiplayerSettings(options)` to `NewGame`.

Delete the complete legacy two-player render/input/setup path and remove `showing2PPanel` from panel-open guards and escape handling. Verify absence with:

```bash
rg -n 'showing2PPanel|TPPanelRender|TwoPPanelKey|twoPlayer(MenuIndex|CR|VictoriesIndex)|SetupNewGame\(2\)' src
```

Expected: no matches.

- [ ] **Step 5: Verify and commit**

Run the focused test, full build, and CTest suite. Commit:

```bash
git add CMakeLists.txt tests/bubblegame_rules_test.cpp src/localmultiplayer_settings.h src/localmultiplayer_settings.cpp src/mainmenu.h src/mainmenu.cpp src/mainmenu_input.cpp src/mainmenu_panels.cpp
git commit -m "fix: expose local victories limit (BUG-023)"
```

---

### Task 4: Flipped-grid chain reservation and cross-chain validation (BUG-022)

**Files:**
- Modify: `tests/bubblegame_rules_test.cpp`
- Modify: `src/bubblegame_board.cpp:57-295`

**Interfaces:**
- Consumes: existing `GridNeighborOffsets(int row, int oddswap)`.
- Produces file-local helpers `CollectColorGroup`, `CollectRootReachable`, and `ValidateAssignedChains`, all using `std::set<std::pair<int,int>>` board positions.

- [ ] **Step 1: Write two failing chain fixtures**

Add `assignChains` inside `BubbleGameTestAccess` (it needs the friend seam),
then add the remaining helpers at file scope:

```cpp
static void assignChains(BubbleGame& game, int idx) {
    game.AssignChainReactions(game.bubbleArrays[idx]);
}
```

```cpp
static void ShapeBoard(BubbleArray& board, bool flipped) {
    for (int row = 0; row < 13; ++row) {
        const bool eightCells = ((row + (flipped ? 1 : 0)) % 2) == 0;
        board.bubbleMap[row].assign(eightCells ? 8 : 7, Bubble{});
    }
}

static SingleBubble FallingBubble(int array, int color, int y) {
    SingleBubble bubble{};
    bubble.assignedArray = array;
    bubble.bubbleId = color;
    bubble.posY = static_cast<float>(y);
    bubble.pos.y = y;
    bubble.falling = true;
    return bubble;
}

static int CountChainsForColor(int color) {
    int count = 0;
    for (const SingleBubble& bubble : singleBubbles)
        if (bubble.bubbleId == color && bubble.chainExists) ++count;
    return count;
}
```

For the parity case, use this literal arrangement. The current hard-coded
standard-parity reservation fails to include `(1,1)` in the group reserved
from `(0,0)`, and therefore assigns both falling bubbles:

```cpp
BubbleGameTestAccess::reset(game, 2, false, false);
BubbleArray& flipped = BubbleGameTestAccess::player(game, 0);
ShapeBoard(flipped, true);
flipped.bubbleMap[0][0].bubbleId = 2;
flipped.bubbleMap[1][1].bubbleId = 2;
singleBubbles = {
    FallingBubble(0, 2, 200),
    FallingBubble(0, 2, 180),
};
BubbleGameTestAccess::assignChains(game, 0);
CHECK(CountChainsForColor(2) == 1);
```

For cross-chain validity, use a standard board with root color 0 at
`(0,0)`/`(0,1)` and dependent color 1 at `(1,1)`/`(1,2)`. Removing the root
chain's target group detaches the dependent group, so the latter assignment
must be cancelled:

```cpp
BubbleGameTestAccess::reset(game, 2, false, false);
BubbleArray& standard = BubbleGameTestAccess::player(game, 0);
ShapeBoard(standard, false);
standard.bubbleMap[0][0].bubbleId = 0;
standard.bubbleMap[0][1].bubbleId = 0;
standard.bubbleMap[1][1].bubbleId = 1;
standard.bubbleMap[1][2].bubbleId = 1;
singleBubbles = {
    FallingBubble(0, 0, 200),
    FallingBubble(0, 1, 180),
};
BubbleGameTestAccess::assignChains(game, 0);
CHECK(CountChainsForColor(0) == 1);
CHECK(CountChainsForColor(1) == 0);
```

- [ ] **Step 2: Run and verify RED**

Run the focused test. Expected: flipped parity reports two chains, and the dependent color-1 chain remains assigned.

- [ ] **Step 3: Correct parity and restore the validity pass**

Compute `const int oddswap = bArray.bubbleMap[0].size() == 8 ? 0 : 1;` once at function entry and replace every local parity calculation and hard-coded offset vector with `GridNeighborOffsets(row, oddswap)`.

`CollectColorGroup` starts from every same-color occupied neighbor of a virtual destination and BFSes same-color board cells. `CollectRootReachable` starts from occupied top-row cells not in an excluded set and BFSes every occupied neighbor not excluded. `ValidateAssignedChains` iterates assigned falling bubbles from largest original `pos.y` to smallest, unions every other assignment's target color group into `otherGroups`, intersects the current target group with `CollectRootReachable(board, otherGroups)`, and cancels the assignment when the intersection contains fewer than two board bubbles:

```cpp
chain.chainExists = false;
chain.chainRow = -1;
chain.chainCol = -1;
chain.chainDest = {};
```

Call `ValidateAssignedChains` once after provisional assignment completes.

- [ ] **Step 4: Verify and commit**

Run the focused and full suites. Commit:

```bash
git add tests/bubblegame_rules_test.cpp src/bubblegame_board.cpp
git commit -m "fix: restore chain target topology rules (BUG-022)"
```

---

### Task 5: Collision-safe maximum-delta movement (BUG-025)

**Files:**
- Modify: `tests/bubblegame_rules_test.cpp`
- Modify: `src/bubblegame.h:580-590`
- Modify: `src/bubblegame_internal.h:35-160`
- Modify: `src/bubblegame_shooter.cpp:442-610`
- Modify: `src/bubblegame_render.cpp:190-270`

**Interfaces:**
- Produces: `int LaunchSubstepCount(int bubbleSize, float deltaScale)`.
- Changes: `SingleBubble::UpdatePosition()` to `SingleBubble::UpdatePosition(float deltaScale)`.
- Produces: `void UpdateSingleBubblesAtScale(float deltaScale)`; existing `UpdateSingleBubbles(int)` delegates using `FrozenBubble::Instance()->deltaScale`.

- [ ] **Step 1: Write the failing maximum-delta production test**

Add `updateAtScale` inside `BubbleGameTestAccess`, and add the remaining helpers
at file scope. Add `<set>` and `<utility>` to the test includes when these
helpers are introduced:

```cpp
static void updateAtScale(BubbleGame& game, float scale) {
    game.UpdateSingleBubblesAtScale(scale);
}
```

```cpp
static SingleBubble VerticalLaunch(int array, int color, int x, int y,
                                   int size, int left, int right, int top) {
    SingleBubble bubble{};
    bubble.assignedArray = array;
    bubble.bubbleId = color;
    bubble.posX = bubble.oldPosX = static_cast<float>(x);
    bubble.posY = bubble.oldPosY = static_cast<float>(y);
    bubble.pos = bubble.oldpos = {x, y};
    bubble.direction = PI / 2.0f;
    bubble.launching = true;
    bubble.leftLimit = left;
    bubble.rightLimit = right;
    bubble.topLimit = top;
    bubble.bubbleSize = size;
    return bubble;
}

static std::pair<int, int> FindPlayerBubble(const BubbleArray& board) {
    for (int row = 0; row < 13; ++row)
        for (int col = 0;
             col < static_cast<int>(board.bubbleMap[row].size()); ++col)
            if (board.bubbleMap[row][col].playerBubble) return {row, col};
    return {-1, -1};
}

static bool IsExpectedNeighbor(std::pair<int, int> cell) {
    static const std::set<std::pair<int, int>> expected = {
        {1, 2}, {1, 3}, {2, 2}, {2, 4}, {3, 2}, {3, 3}
    };
    return expected.count(cell) == 1;
}
```

Create the exact audited standard-board case. It needs two maximum-delta
frames to expose the old endpoint-only result: the first tunnels past the
target without sticking, and the second reaches the ceiling and sticks in row
0. The fixed substep path sticks during the first frame.

```cpp
BubbleGameTestAccess::reset(game, 2, false, false);
BubbleArray& full = BubbleGameTestAccess::player(game, 0);
ShapeBoard(full, false);
full.bubbleOffset = {354, 40};
full.leftLimit = 354;
full.rightLimit = 610;
full.topLimit = 40;
full.bubbleMap[2][3].bubbleId = 0;
full.bubbleMap[2][3].pos = {450, 96};
singleBubbles = {VerticalLaunch(0, 1, 450, 136, 32, 354, 610, 40)};
for (int frame = 0; frame < 2 && !singleBubbles.empty(); ++frame)
    BubbleGameTestAccess::updateAtScale(game, 15.0f);
const auto fullLanding = FindPlayerBubble(full);
CHECK(IsExpectedNeighbor(fullLanding));
CHECK(fullLanding.first != 0);
```

Repeat on array 1 with the audited mini-board geometry:

```cpp
BubbleGameTestAccess::reset(game, 3, false, false);
BubbleArray& mini = BubbleGameTestAccess::player(game, 1);
ShapeBoard(mini, false);
mini.bubbleOffset = {20, 19};
mini.leftLimit = 20;
mini.rightLimit = 148;
mini.topLimit = 19;
mini.bubbleMap[2][3].bubbleId = 0;
mini.bubbleMap[2][3].pos = {68, 47};
singleBubbles = {VerticalLaunch(1, 1, 68, 67, 16, 20, 148, 19)};
for (int frame = 0; frame < 2 && !singleBubbles.empty(); ++frame)
    BubbleGameTestAccess::updateAtScale(game, 15.0f);
const auto miniLanding = FindPlayerBubble(mini);
CHECK(IsExpectedNeighbor(miniLanding));
CHECK(miniLanding.first != 0);
```

- [ ] **Step 2: Run and verify RED**

Run the focused test. Expected: compilation initially fails because `UpdateSingleBubblesAtScale` is missing; after adding only the delegating seam, the current endpoint-only path places the shot in row 0.

- [ ] **Step 3: Parameterize movement and substep local launches**

Implement:

```cpp
inline int LaunchSubstepCount(int bubbleSize, float deltaScale) {
    if (!std::isfinite(deltaScale) || deltaScale <= 0.0f) return 1;
    const float distance = static_cast<float>(BUBBLE_SPEED) * deltaScale;
    const float collisionRadius = static_cast<float>(bubbleSize) * 0.82f;
    return std::max(1, static_cast<int>(std::ceil(distance / collisionRadius)));
}
```

Add explicit `<algorithm>` and `<cmath>` includes beside this helper; do not
rely on transitive SDL/game headers for `std::max`, `std::ceil`, or
`std::isfinite`.

Change every `SingleBubble::UpdatePosition` caller to pass an explicit scale. `UpdateSingleBubbles(int)` becomes a one-line delegate to `UpdateSingleBubblesAtScale`.

For a locally simulated launched bubble, compute `substeps` and `subscale = deltaScale / substeps`. For each substep: update the bubble; process ceiling contact; scan occupied bubbles with `IsCollision`; place/send/stick and stop immediately on the first contact. Preserve the existing remote `mpStickPending` exact-cell path and remote animation behavior; remote boards do not run local collision prediction.

Keep `oldPosX`/`oldPosY` updated inside each substep so `GetClosestFreeCell` sees the contact-local midpoint. Falling, chained, and exploding bubbles still receive one full-scale update per frame.

- [ ] **Step 4: Match aim-guide sampling**

In `DrawAimGuide`, compute the same substep count and use
`deltaScale / substeps` for its per-sample `dx`/`dy`. Change the outer limit
from `400` to `400 * substeps`, the marker condition from `step % 8 == 0` to
`step % (8 * substeps) == 0`, and the fade calculation from `200 - step / 2`
to `200 - step / (2 * substeps)`. This preserves the guide's physical reach,
marker spacing, and fade distance.

- [ ] **Step 5: Verify and commit**

Run focused, full Debug, and sanitizer builds:

```bash
cmake --build build --target bubblegame-rules-test frozen-bubble-sdl3 --parallel
ctest --test-dir build --output-on-failure
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS=-fsanitize=address,undefined \
  -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined
cmake --build build-asan --parallel
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-asan --output-on-failure
```

Expected: all ordinary tests pass; the new gameplay target and both sanitizer-dependent server tests execute successfully in the sanitizer build with no ASan/UBSan diagnostics. On macOS, keep `detect_leaks=0` because Apple ASan does not support LeakSanitizer.

Commit:

```bash
git add tests/bubblegame_rules_test.cpp src/bubblegame.h src/bubblegame_internal.h src/bubblegame_shooter.cpp src/bubblegame_render.cpp
git commit -m "fix: substep launched bubble collisions (BUG-025)"
```

---

### Task 6: Ledger, changelog, and manual verification record

**Files:**
- Modify: `docs/audit/REMEDIATION_STATUS.md`
- Modify: `docs/MANUAL_TEST_CHECKLIST.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: final commit IDs and actual test output from Tasks 1-5.
- Produces: an audit ledger that counts only fixes proven by their citing commits.

- [ ] **Step 1: Update the remediation ledger from verified facts**

After all code commits exist, update the position table from 49 fixed / 24 open to 56 fixed / 17 open, and Medium from 25 fixed / 20 open to 32 fixed / 13 open. Remove BUG-018, BUG-019, BUG-021, BUG-022, BUG-023, BUG-024, and BUG-025 from the open list. Add one fixed-since-refresh entry naming each bug, its commit, and the test/manual evidence actually obtained. Mark IMP-018 complete because its permanent CTest harness now exists. Move “Suggested next order” to Protocol and lobby.

- [ ] **Step 2: Record only the remaining manual gaps**

Add a “Round and match state cluster” section to `docs/MANUAL_TEST_CHECKLIST.md` containing unchecked procedures for:

- active Local Multiplayer victories row navigation with keyboard and controller;
- Classic empty-board continuation and Clear Mode empty-board win presentation/SFX;
- simultaneous local final loss presentation as a draw;
- Team Mode one-team-survivor presentation;
- two native network clients exercising departure continuation on and off;
- two native network clients exercising both visible remote clear message orders;
- maximum-speed bank and vertical shots against occupied bubbles.

Do not describe any of those as executed unless they were actually run and observed.

- [ ] **Step 3: Add user-facing changelog entries**

Under `v2.4.33`, add concise bullets covering corrected Classic/Clear/Team win conditions, simultaneous draws, departure/victories behavior, reachable local victories settings, chain reactions on flipped boards, and high-speed collision tunneling.

- [ ] **Step 4: Run final verification and inspect the diff**

Run:

```bash
git diff --check
cmake --build build --parallel
ctest --test-dir build --output-on-failure
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-asan --output-on-failure
rg -n 'showing2PPanel|TPPanelRender|TwoPPanelKey|twoPlayer(MenuIndex|CR|VictoriesIndex)|SetupNewGame\(2\)' src
git status --short
git diff --stat HEAD~5..HEAD
```

Expected: no whitespace errors; ordinary and sanitizer suites pass; the dead legacy symbols return no matches; status contains only the three intended documentation files before the final commit.

- [ ] **Step 5: Commit documentation**

```bash
git add docs/audit/REMEDIATION_STATUS.md docs/MANUAL_TEST_CHECKLIST.md CHANGELOG.md
git commit -m "docs: close round and match state findings"
```

---

## Final acceptance checklist

- [ ] Classic and Team modes do not award a win solely because one board is empty.
- [ ] Clear Mode awards exactly one win for either local or replicated board clearing.
- [ ] Simultaneous final danger losses commit a draw with no transient score.
- [ ] Departures honor continuation, team survival, connected-opponent, and victories-limit rules.
- [ ] Local 2-4 player setup visibly exposes and propagates the victories limit.
- [ ] Flipped grids reserve one complete chain target group and invalid cross-chains are cancelled.
- [ ] Maximum-delta normal and mini shots cannot pass through occupied bubbles.
- [ ] Existing protocol bytes and consecutive-round `LEFT` preservation remain unchanged.
- [ ] Debug and ASan/UBSan CTest suites pass with no unexpected skips or diagnostics.
- [ ] Ledger counts, manual gaps, and changelog match the evidence actually produced.
