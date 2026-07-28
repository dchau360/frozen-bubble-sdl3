# Complete SDL3 Repository Audit Execution Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a complete, evidence-backed, read-only defect and improvement report for the maintained Frozen Bubble SDL3 repository, with durable artifacts that any capable coding agent can resume.

**Architecture:** Use a subsystem-gated audit with risk-first investigation inside each gate. Keep one canonical status file, one explicit coverage inventory, and one evidence notebook per gate; promote only causally proven or reproduced findings into the final report. Production source remains unchanged until the report is complete and remediation is separately approved.

**Tech Stack:** C++17, C11, SDL3, GLib, raw TCP/WebSocket protocol, Python 3 harnesses, CMake/Ninja/CTest, Emscripten, Gradle/Android NDK, GitHub Actions, Docker Compose, Clang sanitizers, cppcheck, and clang-tidy.

## Global Constraints

- Audit production baseline commit `09d6c7bfcd864a0ad3951b87d16a88dc770392a3` (`v2.4.27`).
- Review all maintained project code and integration surfaces defined in `docs/superpowers/specs/2026-07-28-complete-repository-audit-design.md`.
- Treat the original Perl implementation as behavioral reference only.
- Exclude vendored SDL/iniparser internals while reviewing every project-facing integration boundary.
- Do not fix, refactor, or reformat production source during the audit.
- Tracked changes are limited to the approved `AGENTS.md` symlink and `docs/audit/` evidence/report files.
- Temporary harnesses, corpora, logs, and builds live under `/tmp/fb-sdl3-audit/` or ignored build directories.
- Record every command, exit status, material output, and limitation in the relevant notebook and canonical status file.
- Do not classify a suspicion as a defect without reproduction or a complete code-supported causal argument.
- Update `docs/audit/SDL3_REVIEW_STATUS.md` after every meaningful investigation and before ending any session.
- Record the agent/model when known, but never require a specific vendor or model.
- Every subsystem gate closes with no unresolved candidate, unexplained failure, missing file disposition, or undocumented limitation.
- Commit audit documentation after each task; do not combine production fixes with audit evidence.

---

### Task 1: Bootstrap the resumable audit workspace

**Files:**
- Replace: `AGENTS.md` with relative symlink to `CLAUDE.md`
- Create: `docs/audit/SDL3_REVIEW_STATUS.md`
- Create: `docs/audit/FILE_COVERAGE.md`
- Create: `docs/audit/FINDINGS.md`
- Create: `docs/audit/subsystems/01-server-protocol.md`
- Create: `docs/audit/subsystems/02-network-client-sync.md`
- Create: `docs/audit/subsystems/03-gameplay.md`
- Create: `docs/audit/subsystems/04-lobby-settings-input.md`
- Create: `docs/audit/subsystems/05-render-audio.md`
- Create: `docs/audit/subsystems/06-platform-ports.md`
- Create: `docs/audit/subsystems/07-build-release-tooling.md`
- Create: `docs/audit/subsystems/08-dynamic-integration.md`
- Create: `docs/audit/subsystems/09-final-challenge.md`

**Interfaces:**
- Consumes: approved design and production baseline commit
- Produces: canonical handoff, coverage ledger, stable finding registry, and notebook structure used by every later task

- [ ] **Step 1: Verify the production baseline and branch-only changes**

Run:

```bash
git status --short --branch
git rev-parse main
git diff --name-status main...HEAD
git show --stat --oneline 09d6c7bfcd864a0ad3951b87d16a88dc770392a3
```

Expected: `main` resolves to the production baseline; branch-only changes are audit design/plan documents.

- [ ] **Step 2: Make `CLAUDE.md` the single repository instruction source**

Run:

```bash
unlink AGENTS.md
ln -s CLAUDE.md AGENTS.md
test "$(readlink AGENTS.md)" = "CLAUDE.md"
```

Expected: `AGENTS.md` is a relative symlink whose target is `CLAUDE.md`.

- [ ] **Step 3: Create the canonical status file**

Write `docs/audit/SDL3_REVIEW_STATUS.md` with these populated sections:

```markdown
# SDL3 Complete Review Status

## Audit baseline
- Production commit: 09d6c7bfcd864a0ad3951b87d16a88dc770392a3
- Tag: v2.4.27
- Audit mode: report-first, production source read-only

## Session environment
Record agent/model when known, OS/architecture, compiler, CMake, Ninja,
Python, Java, Gradle, Android SDK/NDK, Emscripten, cppcheck, and clang-tidy.

## Current state
- Phase: Bootstrap
- Active gate: Task 1
- Exact next action: Generate and classify the tracked-file inventory

## Gate checklist
List Tasks 1-13 with statuses: pending, active, complete, or blocked.

## Active candidates
State `None` until a candidate is opened.

## Confirmed findings
State `None` until an ID is registered in `docs/audit/FINDINGS.md`.

## Commands and evidence
Append timestamped command, exit status, concise result, and notebook/artifact link.

## Limitations
Record unavailable platforms, tools, credentials, or interactive scenarios.

## Processes and cleanup
Record temporary ports, PIDs/sessions, log paths, and whether cleanup completed.
```

- [ ] **Step 4: Generate the in-scope tracked-file inventory**

Run:

```bash
git ls-tree -r --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 > /tmp/fb-sdl3-audit-tracked-files.txt
rg '^(src|server|tests|tools|android|web|cmake|docker|\.github)/|^(CMakeLists\.txt|CMakeListsEmscripten\.txt|README\.md|SetupServer\.md|WASM_PORT\.md|start-server\.sh|netlify\.toml|shell\.nix|default\.nix|flake\.nix|flake\.lock)$' /tmp/fb-sdl3-audit-tracked-files.txt
```

Populate `docs/audit/FILE_COVERAGE.md` with one row per result and columns:
`Path | Gate | Disposition | Evidence | Notes`. Mark SDL gitlinks,
`android/app/jni/include/SDL2/**`, `android/app/src/main/java/org/libsdl/**`,
and duplicated iniparser sources as vendored/boundary-review entries rather than
silently omitting them.

- [ ] **Step 5: Create the finding registry and notebook headers**

In `docs/audit/FINDINGS.md`, define the stable ID classes `BUG`, `SEC`, `REL`,
and `IMP`, the design severity scale, and a registry table with columns:
`ID | State | Severity/Priority | Confidence | Gate | Summary | Evidence`.

Give every subsystem notebook these sections:
`Scope`, `Trust boundaries and invariants`, `Static review`, `Dynamic evidence`,
`Candidates`, `Confirmed findings`, `Dismissed candidates`, `Coverage`,
`Limitations`, and `Gate conclusion`.

- [ ] **Step 6: Validate bootstrap consistency**

Run:

```bash
test -L AGENTS.md
test -f docs/audit/SDL3_REVIEW_STATUS.md
test -f docs/audit/FILE_COVERAGE.md
test -f docs/audit/FINDINGS.md
test "$(find docs/audit/subsystems -type f -name '*.md' | wc -l | tr -d ' ')" = "9"
git diff --check
```

Expected: all checks exit zero; status points to Task 2 as the exact next action.

- [ ] **Step 7: Commit the audit bootstrap**

```bash
git add AGENTS.md docs/audit
git commit -m "docs: bootstrap resumable SDL3 audit"
```

---

### Task 2: Establish reproducible build, test, sanitizer, and analysis baselines

**Files:**
- Modify: `docs/audit/SDL3_REVIEW_STATUS.md`
- Modify: `docs/audit/FINDINGS.md`
- Modify: `docs/audit/FILE_COVERAGE.md`
- Modify: the subsystem notebook implicated by any baseline failure

**Interfaces:**
- Consumes: Task 1 ledgers and production baseline
- Produces: trusted toolchain inventory, baseline results, sanitizer builds, and candidate IDs for every unexplained failure

- [ ] **Step 1: Record exact local tool versions**

Install missing analysis tools, then run and record results:

```bash
command -v cppcheck || brew install cppcheck
test -x "$(brew --prefix llvm 2>/dev/null)/bin/clang-tidy" || brew install llvm
command -v emcc || brew install emscripten
uname -a
sw_vers
cmake --version
ninja --version
clang --version
python3 --version
java -version
./android/gradlew --version
emcc --version
cppcheck --version
"$(brew --prefix llvm)/bin/clang-tidy" --version
```

Record each initially absent tool, its install result, and its final version.

- [ ] **Step 2: Run a clean Release build and existing tests**

Run:

```bash
cmake -S . -B build-audit-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-audit-release --parallel
ctest --test-dir build-audit-release --output-on-failure
```

Expected baseline: game and server build; all five registered tests pass. Open a
candidate for every warning or failure that is not an environment problem.

- [ ] **Step 3: Run a warnings-strict Debug build**

Run:

```bash
cmake -S . -B build-audit-werror -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-Werror' \
  -DCMAKE_CXX_FLAGS='-Werror'
cmake --build build-audit-werror --parallel
ctest --test-dir build-audit-werror --output-on-failure
```

Record each warning promoted to error. Classify it as defect candidate,
improvement candidate, third-party/environment noise, or dismissed with evidence.

- [ ] **Step 4: Build and test with ASan and UBSan**

Run:

```bash
cmake -S . -B build-audit-sanitize -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' \
  -DCMAKE_CXX_FLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-audit-sanitize --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-audit-sanitize --output-on-failure
```

If Apple ASan cannot report leaks, record that limitation and retain address and
undefined-behavior coverage.

- [ ] **Step 5: Run broad static analyzers**

Run:

```bash
cmake -S . -B build-audit-compile-db -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cppcheck --project=build-audit-compile-db/compile_commands.json \
  --enable=warning,style,performance,portability \
  --inline-suppr --error-exitcode=0 \
  2> /tmp/fb-sdl3-audit/cppcheck.txt
"$(brew --prefix llvm)/bin/run-clang-tidy" -p build-audit-compile-db \
  > /tmp/fb-sdl3-audit/clang-tidy.txt 2>&1
```

Triage every diagnostic affecting project-owned code; never bulk-promote analyzer
output into findings.

- [ ] **Step 6: Update ledgers and close the baseline gate**

Record tool versions, build/test results, artifact paths, candidates, dismissals,
and limitations. Mark build/test/config files reviewed only where the evidence
actually covers them. Set the exact next action to Task 3.

- [ ] **Step 7: Commit baseline evidence**

```bash
git add docs/audit
git commit -m "docs: record SDL3 audit baselines"
```

---

### Task 3: Audit the C server and untrusted TCP/WebSocket protocol boundary

**Files:**
- Review: `server/fb-server.c`, `server/game.c`, `server/game.h`
- Review: `server/net.c`, `server/net.h`, `server/ws.c`, `server/ws.h`
- Review: `server/tools.c`, `server/tools.h`, `server/stats.c`, `server/stats.h`
- Review: `server/log.c`, `server/log.h`, `server/win32_compat.h`
- Review boundary: `server/CMakeLists.txt`, `server/init/**`, `server/README`
- Review tests: `tests/server_list_cap_test.py`, `tools/server_tests/test_room_caps.py`
- Modify: `docs/audit/subsystems/01-server-protocol.md`
- Modify: `docs/audit/FINDINGS.md`, `docs/audit/FILE_COVERAGE.md`, `docs/audit/SDL3_REVIEW_STATUS.md`

**Interfaces:**
- Consumes: sanitized server binary and baseline diagnostics
- Produces: server/protocol trust model, confirmed/dismissed server findings, and protocol cases consumed by Tasks 4 and 10

- [ ] **Step 1: Map protocol entry points and ownership**

Trace socket accept/read/write, fd-indexed storage, line extraction, WebSocket
decode/encode, command dispatch, game/player allocation, and disconnect cleanup.
Record all buffer sizes, length types, allocation owners, and state transitions.

- [ ] **Step 2: Review memory and index safety**

Prove or challenge bounds for fd values, player/game counts, nickname and message
lengths, partial frames, coalesced commands, retained input, output queues, and
room arrays. Review integer conversions and every `memcpy`, allocation, and
reallocation reachable from network input.

- [ ] **Step 3: Review protocol authorization and lifecycle invariants**

Check nickname uniqueness, creator authority, `CREATE/JOIN/START/PART`, room caps,
player IDs, `GAMEMSG` sender identity, option propagation, room closure, and
cleanup after abnormal disconnect. Compare intended behavior with
`lib/Games/FrozenBubble/Net.pm` only when intent is ambiguous.

- [ ] **Step 4: Exercise sanitized server edge cases**

Run the sanitized `fb-server` on a dedicated recorded port and use temporary
Python clients under `/tmp/fb-sdl3-audit/` to send:

- Empty lines and unknown commands
- Lines at, below, and above every discovered buffer boundary
- Fragmented commands one byte at a time
- Multiple commands in one write
- Invalid UTF-8 and embedded NUL bytes
- WebSocket frames with 7/16/64-bit lengths, masking variations, fragmentation,
  ping/pong/close, invalid opcodes, and truncated payloads
- Rapid connect/disconnect and creator/member disconnect during every room state

Record bytes sent, server response, process status, sanitizer output, and cleanup.

- [ ] **Step 5: Run existing server regression suites**

```bash
python3 tests/server_list_cap_test.py build-audit-sanitize/server/fb-server
python3 tools/server_tests/test_room_caps.py
```

The second script starts and stops its hardcoded `build/server/fb-server` on port
15113. Treat it as a Release regression run; the temporary protocol harness in
Step 4 provides equivalent sanitized-server coverage.

- [ ] **Step 6: Resolve all server candidates and close coverage**

For each candidate, reproduce under sanitizers or write the complete causal path;
otherwise dismiss it with evidence. Update every server file disposition and set
Task 4 as the exact next action.

- [ ] **Step 7: Commit server audit evidence**

```bash
git add docs/audit
git commit -m "docs: complete server protocol audit gate"
```

---

### Task 4: Audit native/WASM clients and multiplayer synchronization

**Files:**
- Review: `src/networkclient.cpp`, `src/networkclient_wasm.cpp`, `src/networkclient.h`
- Review: `src/socket_compat.h`, `src/bubblegame_net.cpp`
- Cross-review: `src/bubblegame.cpp`, `src/bubblegame_state.cpp`, `src/mainmenu_netpanel.cpp`
- Review tests/tools: `tools/net_bots.py`, `tests/net_bots_test.py`
- Modify: `docs/audit/subsystems/02-network-client-sync.md`
- Modify: shared audit ledgers/status

**Interfaces:**
- Consumes: Task 3 protocol model and server cases
- Produces: client queue/lifecycle model, native-vs-WASM parity assessment, and multiplayer synchronization findings consumed by Tasks 5 and 10

- [ ] **Step 1: Map connection and message lifecycles**

Trace connect, nonblocking receive, buffering, line parsing, push/command routing,
`messageQueue`, `syncQueue`, WebSocket callbacks, disconnect, reconnect, and object
lifetime. Record which thread/callback may mutate each field.

- [ ] **Step 2: Prove receive-buffer and protocol parsing bounds**

Review every length conversion, retained partial line, game-message player ID,
copy operation, queue growth path, and malformed response path. Compare native and
WASM limits and behavior for the same byte sequences.

- [ ] **Step 3: Review multiplayer identity and authority**

Trace lobby IDs to `bubbleArrays`, leader-only level generation, `b|/N/T` sync,
fire/stick/malus/game-over/ready/player-left/options/targeting/stats opcodes, and
round reset. Check stale, duplicate, reordered, early, and post-disconnect messages.

- [ ] **Step 4: Exercise protocol parity against the sanitized server**

Use temporary proxy/scripts to fragment and coalesce server responses, inject
unknown or malformed pushes/opcodes, close mid-message, reconnect, and deliver
sync messages before the consuming state. Run native client under ASan/UBSan and
record whether WASM behavior can be tested locally or only reasoned/build-checked.

- [ ] **Step 5: Run bot unit tests and multi-round smoke sessions**

```bash
python3 tests/net_bots_test.py
ctest --test-dir build-audit-sanitize -R net-bots-test --output-on-failure
```

Run at least one sanitized server plus one native client and bots through two
round transitions, recording logs and process cleanup.

- [ ] **Step 6: Resolve candidates, update file coverage, and close the gate**

Cross-link protocol findings to Task 3 rather than duplicating IDs. Set Task 5 as
the exact next action.

- [ ] **Step 7: Commit network-client audit evidence**

```bash
git add docs/audit
git commit -m "docs: complete network synchronization audit gate"
```

---

### Task 5: Audit gameplay rules, board algorithms, and round state

**Files:**
- Review: `src/bubblegame.cpp`, `src/bubblegame.h`, `src/bubblegame_internal.h`
- Review: `src/bubblegame_board.cpp`, `src/bubblegame_shooter.cpp`, `src/bubblegame_level.cpp`
- Review: `src/bubblegame_state.cpp`, `src/bubblegame_input.cpp`, `src/bubblegame_render.cpp`
- Cross-review: `src/bubblegame_net.cpp`
- Review helpers/tests: `src/netview.*`, `src/netteams.*`, `src/roundstats_color.*`, matching tests
- Reference: relevant mechanics in `bin/frozen-bubble` and `lib/Games/FrozenBubble/Net.pm`
- Modify: `docs/audit/subsystems/03-gameplay.md`
- Modify: shared audit ledgers/status

**Interfaces:**
- Consumes: Task 4 player identity/message semantics
- Produces: gameplay invariants, behavioral parity evidence, boundary cases, and dynamic scenarios consumed by Task 10

- [ ] **Step 1: Document core state invariants**

Record valid ranges and ownership for player counts, `bubbleArrays`, 13-row maps,
cell multiplicity, colors, angles, positions, `PlayerState`, malus queues,
`nextColors`, shooter state, targeting, team IDs, rounds, and victories.

- [ ] **Step 2: Review placement and collision algorithms**

Trace launch movement with `deltaScale`, wall rebounds, collision detection,
nearest-cell selection, row geometry, group/fallen-bubble discovery, chain
reactions, compressor rows, and loss-line detection. Challenge empty/full board,
edge column, duplicate cell, high delta, and malformed network-placement cases.

- [ ] **Step 3: Review scoring, malus, targeting, and win/loss transitions**

Compare formulas and ordering with the Perl reference. Check overflow, negative
values, team aggregation, spectators, players leaving, simultaneous losses,
clear-mode completion, stats reset, and round/match termination.

- [ ] **Step 4: Review reload and resource/state reset paths**

Trace `NewGame`, level loading/random generation, synchronized levels, reload,
next round, quit to title, and destruction. Identify stale references, counters,
textures, queues, or input flags that survive incorrectly.

- [ ] **Step 5: Run helper tests in Release, Debug, and sanitizer builds**

```bash
ctest --test-dir build-audit-release \
  -R 'netview|netteams|roundstats' --output-on-failure
ctest --test-dir build-audit-werror \
  -R 'netview|netteams|roundstats' --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-audit-sanitize \
  -R 'netview|netteams|roundstats' --output-on-failure
```

- [ ] **Step 6: Run deterministic boundary scenarios**

Use temporary harnesses or controlled bot messages to exercise player counts
1, 2, 5, 6, and 20; all valid team counts; colors at configured minimum/maximum;
simultaneous game-over; clear win; and at least three consecutive rounds.
Preserve seeds/messages/logs for every reproduced issue.

- [ ] **Step 7: Resolve candidates and close gameplay coverage**

Update every gameplay/helper/test file disposition, record Perl comparisons, and
set Task 6 as the exact next action.

- [ ] **Step 8: Commit gameplay audit evidence**

```bash
git add docs/audit
git commit -m "docs: complete gameplay audit gate"
```

---

### Task 6: Audit lobby, settings, persistence, and input

**Files:**
- Review: `src/mainmenu.cpp`, `src/mainmenu.h`, `src/mainmenu_internal.h`
- Review: `src/mainmenu_input.cpp`, `src/mainmenu_netpanel.cpp`, `src/mainmenu_panels.cpp`, `src/mainmenu_server.cpp`
- Review: `src/gamesettings.cpp`, `src/gamesettings.h`
- Review: `src/highscoremanager.cpp`, `src/highscoremanager.h`
- Review: `src/menubutton.cpp`, `src/menubutton.h`
- Cross-review: input/event paths in `src/frozenbubble.cpp`, `src/frozenbubble.h`
- Modify: `docs/audit/subsystems/04-lobby-settings-input.md`
- Modify: shared audit ledgers/status

**Interfaces:**
- Consumes: Tasks 4-5 option, team, player-count, and input expectations
- Produces: UI-state, persistence, and controller/key invariants plus malformed-setting scenarios for Task 10

- [ ] **Step 1: Map menu and room state transitions**

Trace title, panels, server list, lobby, room creation/join, game start, return,
settings, key bindings, and level editor transitions. Check selected indices,
list mutation, focus, stale network state, and all player-count-dependent layouts.

- [ ] **Step 2: Review option serialization and validation**

Trace every local option through room UI, protocol serialization, remote parsing,
`SetupSettings`, and gameplay use. Check values outside UI ranges, player slots
6-20, team overrides, clear/team incompatibilities, and host/joiner differences.

- [ ] **Step 3: Review settings/highscore persistence**

Check pref paths, default creation, parse failures, missing keys, malformed types,
out-of-range numeric values, long nickname/host/path values, partial writes,
unwritable directories, and preservation of user-selected values across upgrades.

- [ ] **Step 4: Review keyboard/controller/mouse bounds**

Trace SDL events to keyboard state, virtual scancodes, controller slots, hotplug,
axis/button mappings, rebinding, simultaneous inputs, and teardown. Prove every
array index and player/controller mapping for zero through five local players.

- [ ] **Step 5: Run temporary-home persistence cases**

Run the sanitized client with isolated temporary preference directories covering:
first run, empty file, malformed INI, missing section/key, huge/negative numeric
values, long strings, read-only file, and unwritable parent. Record exit behavior,
logs, resulting files, and sanitizer output without touching the user's real prefs.

- [ ] **Step 6: Resolve candidates and close the gate**

Update file coverage and cross-link any gameplay/network consequence to the
originating finding. Set Task 7 as the exact next action.

- [ ] **Step 7: Commit lobby/settings/input audit evidence**

```bash
git add docs/audit
git commit -m "docs: complete lobby settings input audit gate"
```

---

### Task 7: Audit rendering, transitions, fonts, and audio lifecycle

**Files:**
- Review: `src/shaderstuff.cpp`, `src/shaderstuff.h`
- Review: `src/transitionmanager.cpp`, `src/transitionmanager.h`
- Review: `src/ttftext.cpp`, `src/ttftext.h`
- Review: `src/audiomixer.cpp`, `src/audiomixer.h`
- Review render lifecycle: `src/frozenbubble.cpp`, `src/bubblegame_render.cpp`, `src/mainmenu_panels.cpp`
- Review compatibility: `src/sdl3_compat.h`
- Modify: `docs/audit/subsystems/05-render-audio.md`
- Modify: shared audit ledgers/status

**Interfaces:**
- Consumes: gameplay/menu render-state expectations
- Produces: resource ownership map, pixel/surface safety assessment, and repeated-lifecycle scenarios for Task 10

- [ ] **Step 1: Build an SDL resource ownership table**

For every project-owned window, renderer, texture, surface, font, audio stream,
track, allocation, and precomputed effect buffer, record creator, owner, aliases,
replacement behavior, and destruction path.

- [ ] **Step 2: Review surface/pixel operations**

For each `memcpy` and pixel loop, verify format, bytes-per-pixel, pitch, width,
height, offsets, effect dimensions, lock requirements, and source/destination
compatibility. Challenge non-default pixel formats and failed allocations/loads.

- [ ] **Step 3: Review transition and text lifecycle**

Trace snapshot capture, effect setup/teardown, repeated transitions, resize/logical
scaling, text refresh, font replacement, empty/long strings, and renderer reset.

- [ ] **Step 4: Review audio initialization and failure handling**

Trace mixer init/open, music/SFX load, track creation, playback, pause/resume,
volume, replacement, shutdown, and missing-device/file behavior. Verify callback
or stream lifetime cannot outlive owned data.

- [ ] **Step 5: Run sanitizer-backed lifecycle stress**

Repeatedly enter/leave menus and gameplay, cycle every transition, reload rounds,
toggle fullscreen/resize where supported, recreate text, play overlapping SFX,
pause/resume, and exit. Record ASan/UBSan output and macOS leak-detection limits.

- [ ] **Step 6: Resolve candidates and close render/audio coverage**

Mark each file disposition, retain dismissed analyzer reports, and set Task 8 as
the exact next action.

- [ ] **Step 7: Commit render/audio audit evidence**

```bash
git add docs/audit
git commit -m "docs: complete rendering audio audit gate"
```

---

### Task 8: Audit native, WASM, and Android platform integration

**Files:**
- Review: `src/main.cpp`, `src/platform.cpp`, `src/platform.h`, `src/logger.cpp`, `src/logger.h`
- Review platform guards: all `__WASM_PORT__`, `__ANDROID_PORT__`, `__ANDROID__`, `WIN32`, and `MINGW` branches in `src/**`
- Review: `web/index.html`, `web/shell.html`, `web/README.md`, `WASM_PORT.md`, `cmake/Emscripten.cmake`, `CMakeListsEmscripten.txt`
- Review Android project code: `android/app/src/main/java/org/frozenbubble/**`, manifest, Gradle/CMake files, `android/SETUP.md`
- Review boundary only: SDL Android Java, SDL gitlinks, legacy SDL2 headers, duplicated iniparser
- Modify: `docs/audit/subsystems/06-platform-ports.md`
- Modify: shared audit ledgers/status

**Interfaces:**
- Consumes: ownership, persistence, protocol, and build expectations from Tasks 3-7
- Produces: platform parity matrix and explicit build/runtime limitations used by Tasks 9-12

- [ ] **Step 1: Map platform-specific behavior**

Create a native/macOS, Linux, Windows, WASM, and Android matrix for entry point,
data directory, preference directory, networking, main loop, input, audio, asset
loading, logging, server availability, and shutdown.

- [ ] **Step 2: Review compile guards and source-list parity**

List every guarded branch and prove that each platform selects exactly one valid
implementation. Compare native `CMakeLists.txt`, Android source list, and WASM
sources for omissions, duplicate symbols, stale files, and definition drift.

- [ ] **Step 3: Review filesystem and logging failure paths**

Check executable/AppImage/app-bundle paths, Android extraction, WASM `/share`,
pref paths, relative/absolute assets, missing directories, Unicode paths, read-only
locations, logger initialization failure, and shutdown ordering.

- [ ] **Step 4: Build Android locally**

Run from a fresh shell so persisted JDK/SDK configuration is tested:

```bash
cd android
./gradlew clean assembleRelease --no-daemon
cd ..
```

Record all warnings, three ABI outputs, signing state, APK path, and any generated
working-tree changes; restore only generated tracked-file changes attributable to
the build and document the restoration.

- [ ] **Step 5: Build WASM when Emscripten is available**

Use the workflow's port-file setup in a disposable Emscripten installation or
document why local setup cannot reproduce it. Build into `build-audit-wasm`,
serve with COOP/COEP headers, load the game, connect through WebSocket proxy, and
record console/runtime errors. Never mark WASM runtime passed from compile alone.

- [ ] **Step 6: Validate native packaged-path behavior**

Run the native binary from the build tree and from a staged install/app-bundle
layout. Confirm asset, preference, log, dynamic-library, and working-directory
independence. Use CI evidence for Linux/Windows where local execution is absent
and record runtime testing as unavailable.

- [ ] **Step 7: Resolve candidates and close platform coverage**

Assign dispositions to vendored files as boundary-reviewed/excluded with reason.
Set Task 9 as the exact next action.

- [ ] **Step 8: Commit platform audit evidence**

```bash
git add docs/audit
git commit -m "docs: complete platform integration audit gate"
```

---

### Task 9: Audit build, tests, packaging, CI, deployment, tooling, and operations

**Files:**
- Review: `CMakeLists.txt`, `server/CMakeLists.txt`, `cmake/**`
- Review: `.github/workflows/build.yml`
- Review: `android/*.gradle`, Gradle wrapper metadata, `android/app/*.gradle`, `android/app/CMakeLists.txt`
- Review: `docker/**`, `SetupServer.md`, `start-server.sh`
- Review: `tools/**`, `tests/**`, `web/**`
- Review: `README.md`, `WASM_PORT.md`, `android/SETUP.md`, `netlify.toml`, Nix files
- Modify: `docs/audit/subsystems/07-build-release-tooling.md`
- Modify: shared audit ledgers/status

**Interfaces:**
- Consumes: all earlier build/platform/test gaps
- Produces: release reliability findings, dependency/CI risk assessment, and prioritized test/tooling improvements

- [ ] **Step 1: Compare all build source lists and definitions**

Build a table for native, Windows, WASM, Android, and server sources, compile
definitions, C++ standard, warnings, linked libraries, assets, tests, versions,
and output names. Reconcile every difference as intentional or candidate finding.

- [ ] **Step 2: Review dependency and action pinning**

Check release URLs/tags, `latest` dependencies, GitHub Actions tags versus commit
pins, Butler action branch pinning, Emscripten patch assumptions, Gradle wrapper,
NDK/SDK versions, submodule depth/recursion, Homebrew fallback versions, and
reproducibility of artifacts.

- [ ] **Step 3: Review release version/signing/artifact flow**

Trace tag to macOS plist, Windows version resource/NSIS, Android version/signing,
WASM package, GitHub release files, and Itch channels. Check unsigned/ad-hoc/test
keys, secret handling, artifact nesting, filename consistency, permissions, and
failure propagation.

- [ ] **Step 4: Validate workflow/config syntax without external writes**

Run local parsers and dry-run validators:

```bash
ruby -e 'require "yaml"; YAML.load_file(".github/workflows/build.yml", aliases: true)'
docker compose -f docker/docker-compose.yml config
cmake -S . -B build-audit-config -G Ninja -DCMAKE_BUILD_TYPE=Release
./android/gradlew tasks --all --no-daemon
python3 -m py_compile tools/net_bots.py tests/*.py tools/server_tests/*.py
```

Record unavailable Docker/Gradle components as limitations, not passes.

- [ ] **Step 5: Assess test coverage against discovered risks**

Map each confirmed/candidate invariant to an existing test, dynamic audit case,
or uncovered gap. Register concrete `IMP` entries for missing regression coverage,
including proposed test location, inputs, assertions, and platform matrix.

- [ ] **Step 6: Review operational documentation against actual commands**

Execute non-destructive documented commands or validate their paths/options.
Check server ports, TLS/WebSocket proxy, COOP/COEP headers, Android prerequisites,
artifact names, build directories, and release instructions for drift.

- [ ] **Step 7: Resolve candidates and close build/release/tooling coverage**

Set Task 10 as the exact next action.

- [ ] **Step 8: Commit build/release/tooling audit evidence**

```bash
git add docs/audit
git commit -m "docs: complete build release tooling audit gate"
```

---

### Task 10: Execute the cross-subsystem dynamic integration matrix

**Files:**
- Modify: `docs/audit/subsystems/08-dynamic-integration.md`
- Modify: `docs/audit/FINDINGS.md`, `docs/audit/FILE_COVERAGE.md`, `docs/audit/SDL3_REVIEW_STATUS.md`

**Interfaces:**
- Consumes: exact dynamic cases and invariants from Tasks 3-9
- Produces: end-to-end runtime evidence, cross-subsystem findings, and cleanup proof

- [ ] **Step 1: Define the recorded matrix before launching processes**

Include native single-player smoke; network games with 2, 5, 6, and 20 players;
normal, team, and clear modes; spectator entry/pinning; creator/member departure;
rounds 1-3; malformed/fragmented traffic; settings isolation; and clean shutdown.
For every row record setup, expected observable, log evidence, sanitizer state,
manual visual/audio observation required, and result.

- [ ] **Step 2: Launch isolated sanitized services**

Use dedicated ports and `/tmp/fb-sdl3-audit/` logs. Record command, PID/session,
port, binary commit/build, settings directory, and cleanup command before starting
each process. Never reuse unrelated servers or user preference files.

- [ ] **Step 3: Execute player-count boundaries**

Run 2-, 5-, 6-, and 20-player rooms with controlled bots. Verify roster, board
mapping, paging/pinning, input ownership, game start, message identity, targeting,
disconnect behavior, winner determination, round stats, and next-round readiness.

- [ ] **Step 4: Execute mode and lifecycle boundaries**

Run normal, team, clear, and spectator scenarios. Include simultaneous losses,
clear win/banner/audio, creator leaving before/during game, member leaving,
reconnect attempts, return to lobby/title, and three consecutive rounds.

- [ ] **Step 5: Execute hostile transport cases during gameplay**

Fragment/coalesce messages, disconnect mid-message, send invalid player IDs,
duplicate ready/game-over/stats messages, reorder sync messages where the
transport harness permits, and flood within bounded local limits. Record both
client and server behavior and sanitizer reports.

- [ ] **Step 6: Clean up and prove no test process remains**

Record final process/port checks and confirm all dedicated listeners, clients,
bots, proxies, and servers are stopped. Preserve relevant logs and delete only
disposable artifacts whose paths were recorded.

- [ ] **Step 7: Resolve every integration candidate and close the gate**

Cross-link findings to their originating subsystem and update severity based on
runtime impact. Set Task 11 as the exact next action.

- [ ] **Step 8: Commit integration evidence**

```bash
git add docs/audit
git commit -m "docs: complete dynamic integration audit gate"
```

---

### Task 11: Reconcile complete file coverage and prioritize improvements

**Files:**
- Modify: `docs/audit/FILE_COVERAGE.md`
- Modify: `docs/audit/FINDINGS.md`
- Modify: all `docs/audit/subsystems/*.md` as evidence requires
- Modify: `docs/audit/SDL3_REVIEW_STATUS.md`

**Interfaces:**
- Consumes: all subsystem and dynamic evidence
- Produces: zero-gap coverage ledger, resolved candidate registry, and ranked improvement set for independent challenge

- [ ] **Step 1: Reconcile inventory against the pinned commit**

Regenerate the pinned-commit file list and compare it with `FILE_COVERAGE.md`.
Add any missing path, remove no path without an exclusion reason, and verify each
row links to evidence or a boundary/exclusion explanation.

- [ ] **Step 2: Run cross-cutting source searches**

Search project-owned code for unchecked indices, raw copies/formatting, signed/
unsigned conversions, allocation/free and SDL create/destroy pairs, ignored return
values, global/singleton lifetime, compile guards, hardcoded capacities/versions,
network lengths/opcodes, filesystem assumptions, and duplicate platform source
lists. Reconcile each hit with notebook evidence.

- [ ] **Step 3: Resolve the candidate registry**

No entry may remain `suspected` or `investigating`. Confirm with evidence or
dismiss with the exact counter-evidence and link. Check stable IDs are unique and
never recycled.

- [ ] **Step 4: Normalize defect severity and confidence**

Apply the design scale consistently. Separate observed runtime facts, complete
causal proofs, weaker inferences, and platform limitations. Merge duplicate root
causes while retaining every affected path/platform.

- [ ] **Step 5: Rank improvements separately**

For every `IMP` entry record expected benefit, implementation effort (`small`,
`medium`, or `large`), regression risk (`low`, `medium`, or `high`), concrete
files/components, proposed change, and verification method. Cover maintainability,
tests, architecture, diagnostics, performance, developer experience, and release
reliability only where evidence supports the recommendation.

- [ ] **Step 6: Close all ordinary subsystem gates**

Verify notebooks 01-08 have explicit gate conclusions and no unresolved action.
Set Task 12 as the exact next action.

- [ ] **Step 7: Commit reconciled coverage and priorities**

```bash
git add docs/audit
git commit -m "docs: reconcile SDL3 audit coverage"
```

---

### Task 12: Perform an independent final challenge

**Files:**
- Review: all audit artifacts and source at the pinned production commit
- Modify: `docs/audit/subsystems/09-final-challenge.md`
- Modify: any audit evidence corrected by the challenge
- Modify: `docs/audit/SDL3_REVIEW_STATUS.md`

**Interfaces:**
- Consumes: closed Tasks 1-11 and complete candidate/finding ledgers
- Produces: independently challenged findings, corrected severity/coverage, and approval to synthesize the final report

- [ ] **Step 1: Start with a fresh reviewer context**

Give the reviewer only `CLAUDE.md`, the approved design, this plan,
`SDL3_REVIEW_STATUS.md`, `FILE_COVERAGE.md`, `FINDINGS.md`, subsystem notebooks,
and pinned commit. Record agent/model when known. Do not give conclusions that are
not already present in the audit artifacts.

- [ ] **Step 2: Challenge every confirmed defect**

For each `BUG`, `SEC`, and `REL`, attempt to falsify reachability, inputs,
platform applicability, impact, and root cause. Re-run reproductions where local
tools permit. Record `upheld`, `revised`, or `dismissed` with evidence.

- [ ] **Step 3: Challenge improvement value and scope**

Reject generic cleanup, speculative rewrites, recommendations already satisfied,
or changes whose likely regression risk outweighs evidence-backed benefit. Verify
remaining improvements have concrete changes and verification methods.

- [ ] **Step 4: Search for missed cross-subsystem interactions**

Focus on server/client length mismatches, lobby-to-game option drift, player-ID
mapping, round resets, renderer/audio teardown during state changes, platform
source-list drift, package/runtime path drift, and settings/input effects on
gameplay/network state.

- [ ] **Step 5: Audit the coverage claim itself**

Sample every disposition class, verify notebook links, compare inventory counts,
and ensure unavailable platform/runtime checks are limitations rather than passes.

- [ ] **Step 6: Apply challenge corrections and close the gate**

Update ledgers/notebooks with corrections, keep a record of challenged-and-
dismissed findings, and state whether final synthesis is authorized. Set Task 13
as the exact next action only when no challenge issue remains open.

- [ ] **Step 7: Commit final-challenge evidence**

```bash
git add docs/audit
git commit -m "docs: complete independent SDL3 audit challenge"
```

---

### Task 13: Publish the complete repository review report

**Files:**
- Create: `docs/audit/SDL3_COMPLETE_REVIEW.md`
- Modify: `docs/audit/SDL3_REVIEW_STATUS.md`
- Verify: all `docs/audit/**`

**Interfaces:**
- Consumes: independently challenged findings, improvements, coverage, commands, and limitations
- Produces: final report and a terminal resumable status with no remaining audit action

- [ ] **Step 1: Write the executive conclusion**

State audited commit, scope, methods, number of confirmed findings by severity,
release-readiness assessment, highest risks, and material limitations. Do not claim
absence of undiscovered defects.

- [ ] **Step 2: Write confirmed findings in remediation order**

For every upheld `SEC`, `BUG`, and `REL`, include stable ID, severity, confidence,
affected platforms/files/lines, evidence, reproduction or causal path, user impact,
proposed correction, and verification strategy. Link to subsystem evidence.

- [ ] **Step 3: Write the improvement roadmap**

Group upheld `IMP` entries by priority and include benefit, effort, regression
risk, concrete target files/components, proposed change, and verification method.
Keep improvements visibly separate from defects.

- [ ] **Step 4: Write security, platform, test, and coverage appendices**

Summarize protocol trust boundaries, sanitizer/static-analysis results, platform
build/runtime status, exact commands, complete file dispositions, dismissed false
positives, known limitations, and residual risks.

- [ ] **Step 5: Validate report consistency mechanically and manually**

Run:

```bash
rg -n 'TB[D]|TO[D]O|investigating|suspected' docs/audit
git diff --check
```

Expected: no unresolved marker appears in final-state audit artifacts. Verify every
finding ID in the report exists once in `FINDINGS.md`, every evidence link resolves,
severity totals match, and every coverage row has a final disposition.

- [ ] **Step 6: Mark the audit status terminal**

Set phase to `Complete`, link the final report and final-challenge notebook, list
the final audited commit and artifact commit, state `Exact next action: None —
audit complete`, confirm no temporary process/listener remains, and retain all
environment limitations.

- [ ] **Step 7: Commit the complete report**

```bash
git add docs/audit
git commit -m "docs: publish complete SDL3 repository audit"
```

- [ ] **Step 8: Verify branch state and hand off remediation decisions**

Run:

```bash
git status --short --branch
git log --oneline --decorate main..HEAD
```

Expected: clean audit branch containing instruction symlink plus design, plan,
evidence, and final report commits; no production source changes. Present the
report to the user before proposing or starting any remediation branch.
