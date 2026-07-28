# SDL3 Complete Review Status

## Audit baseline

- Production commit: `09d6c7bfcd864a0ad3951b87d16a88dc770392a3`
- Tag: `v2.4.27`
- Audit branch: `codex/sdl3-complete-audit`
- Audit mode: report-first, production source read-only
- Baseline relationship: the audit branch contains only the approved design, plan, instruction symlink, and audit evidence; production source is unchanged.
- Bootstrap start state: clean worktree at `3f57ce9c5a41fccfdd85c9d5fecace5dc85e598c`; `main` resolved to the production commit above.

## Session environment

| Component | Recorded value |
|---|---|
| Agent | Codex subagents `task_1_implementer` (bootstrap), `task_2_implementer` (baselines), `task_3a_static` (server static review), `task_3c_synthesis` (static Task 3 closure), `task_4_implementer` (client/synchronization review), and `task_5_implementer` (gameplay review), plus the Task 6 (lobby/settings/input) and Task 7 (render/audio) implementer agents |
| Model | Unknown; the dispatcher did not expose a model identifier |
| Host | macOS 26.5.2 (build 25F84), Darwin 25.5.0, arm64 |
| Compiler | Apple clang 21.0.0 (`clang-2100.1.1.101`), target `arm64-apple-darwin25.5.0` |
| CMake | 4.3.4 |
| Ninja | 1.13.2 |
| Python | 3.14.6 |
| Java | OpenJDK 17.0.19 (Homebrew) |
| Gradle | 8.2 via `android/gradlew` |
| Android SDK | `/opt/homebrew/share/android-commandlinetools`; sdkmanager 20.0 |
| Android NDK | 25.2.9519653 |
| Emscripten | 6.0.4-git (Homebrew; initially absent, installed successfully in Task 2) |
| cppcheck | 2.21.0 (Homebrew; initially absent, installed successfully in Task 2) |
| clang-tidy | Homebrew LLVM 22.1.8, optimized build (initially absent, installed successfully in Task 2; invoked by absolute keg-only path) |

## Current state

- Phase: Phase 2 — subsystem review
- Active gate: Task 8 (pending)
- Exact next action: Begin Task 8, Step 1: map platform-specific behavior.

## Gate checklist

| Task | Gate | Status |
|---|---|---|
| Task 1 | Bootstrap resumable audit workspace | complete |
| Task 2 | Reproducible build, test, sanitizer, and analysis baselines | complete |
| Task 3 | C server and untrusted TCP/WebSocket protocol | complete (static evidence; runtime/security matrix omitted by user direction) |
| Task 4 | Native/WASM clients and multiplayer synchronization | complete (static plus existing bot unit checks; security traffic and two-round smoke omitted) |
| Task 5 | Gameplay rules, board algorithms, and round state | complete (Fix Round 1 added a core invariant ledger and exact maximum-delta production-object evidence) |
| Task 6 | Lobby, settings, persistence, and input | complete (static review plus isolated-preferences runtime matrix; security runtime and live-server lobby transitions omitted) |
| Task 7 | Rendering, transitions, fonts, and audio lifecycle | complete (static ownership/pixel/lifecycle review plus isolated production-object runtime: BUG-001 and the new BUG-041 reproduced; dummy-driver-only rendering and full-client navigation omissions recorded) |
| Task 8 | Native, WASM, and Android platform integration | pending |
| Task 9 | Build, tests, packaging, CI, deployment, tooling, and operations | pending |
| Task 10 | Cross-subsystem dynamic integration matrix | pending |
| Task 11 | Complete file coverage and prioritized improvements | pending |
| Task 12 | Independent final challenge | pending |
| Task 13 | Complete repository review report | pending |

## Active candidates

- No Task 7 candidate remains open. BUG-001 is confirmed (both `TextureEx`
  null-deref orderings reproduced under UBSan against production code plus the
  rect/surface leak family); IMP-007 and IMP-010 are confirmed improvements
  with their cross-owner dispositions finished; the IMP-005 and IMP-006 render
  slices are closed without promotion.
- IMP-008 — selective API/const/cast/parser cleanup remains open for the later
  assigned subsystems (Tasks 8-9 files); the slices belonging to already-closed
  gates, now including Task 7's render slice, are dispositioned in their
  notebooks.
- Task 7 added BUG-041 (confirmed, runtime-reproduced transition texture
  leak), BUG-042 (confirmed, static-proven per-match penguin/hurry texture
  reload leak), and IMP-013 (confirmed improvement: pixel-helper clamp
  off-by-one, ASan-demonstrated, unreachable with shipped assets).

## Confirmed findings

- Task 3 confirmed BUG-002 through BUG-011; SEC-001, SEC-002, SEC-004 through
  SEC-006; and REL-001 through REL-004. It confirmed IMP-001 through IMP-004
  and IMP-011 as improvements, not defects. BUG-010 resolves nickname eviction;
  BUG-011 resolves repeated `START`; BUG-003 includes post-start `CLOSE`.
- The highest-impact static chains are BUG-003 (priority fd left without a
  game can trigger peer-driven server exit), SEC-004 (unbound player/leader
  identity), and SEC-005 (128-byte UDP probe permits out-of-bounds stack read).
- See [FINDINGS.md](FINDINGS.md) and the
  [server notebook](subsystems/01-server-protocol.md) for severity, proof, and
  exact static causal paths and downstream owners.
- Task 4 confirmed SEC-003 and BUG-013 through BUG-017, dismissed/reserved
  BUG-012, and extended REL-003
  from the server to the native Windows client. The highest-impact client paths
  are unchecked peer coordinates reaching board indexing, incomplete command
  response correlation, native TCP record assumptions, and Windows per-frame
  receive remaining blocking. See the
  [network client notebook](subsystems/02-network-client-sync.md).
- Task 6 confirmed BUG-026 through BUG-040, SEC-007, and IMP-012, and resolved
  the menu/settings half of BUG-021, BUG-023, SEC-004, and IMP-005 by extending
  those entries. The highest-impact paths are BUG-026 (an unwritable or
  unrepairable preferences file spins startup forever), BUG-032 (a corrupt
  highscore file aborts the client during construction), BUG-034 (missing assets
  leave `FrozenBubble`'s members indeterminate and `RunForEver` dereferences
  them), and SEC-007 (an unclamped peer team number reaches five-element
  `kTeamColors` indexing in gameplay). Eight of these were reproduced at runtime
  against unchanged production code inside isolated preference homes. See the
  [lobby/settings/input notebook](subsystems/04-lobby-settings-input.md).
- Task 7 confirmed BUG-001, BUG-041, and BUG-042 plus IMP-007, IMP-010, and
  IMP-013, and closed BUG-034's audio-side lifetime question by cross-link.
  The highest-impact render paths are BUG-041 (every transition animation
  frame leaks a 640×480 texture — reproduced at exactly 1.2 MB/frame through
  linked production objects, ~40-50 MB per game start, round reload, or menu
  return) and BUG-042 (every `NewGame` reloads 394 penguin textures per
  player plus `hurryTexture` with no destroy site). BUG-001's two null-deref
  orderings were reproduced under UBSan at `shaderstuff.h:55` and `:67`.
  Sanitized production-object stress of transitions, audio lifecycle, and
  text lifecycle produced no other diagnostic. See the
  [render/audio notebook](subsystems/05-render-audio.md).
- Task 5 confirmed BUG-018 through BUG-025 and IMP-005, IMP-006, and
  IMP-009. The highest-impact gameplay path is BUG-020: a quit/new-match
  transition can retain in-flight malus assigned to an array whose board was
  cleared, later reaching invalid row-vector indexing. Other confirmed rule
  failures cover unconditional clear wins, simultaneous final losses,
  departures, flipped-grid chain assignment, inert local victory limits, and
  timing-dependent remote clear accounting. BUG-025 proves that the 75 px
  maximum native step can cross an occupied bubble between endpoint-only
  collision samples and attach elsewhere. See the
  [gameplay notebook](subsystems/03-gameplay.md).

## Task 3 closure provenance

- Three whole-task runtime-agent dispatches and one split runtime dispatch were
  rejected by the automated classifier before producing runtime evidence.
- The user then explicitly skipped security work. Task 3 closure therefore used
  the completed static review and read-only source/audit synthesis only.
- No Task 3 runtime server, process, port, harness, client, socket traffic,
  signal scenario, or fault injection was created. Existing foreign listeners
  remained untouched.
- Confirmed Task 3 findings are code-supported inferences, not observed runtime
  facts. The omitted TCP/WebSocket/UDP, backpressure, identity, fault-injection,
  and server-survival checks are a final-audit limitation, not passing evidence.

## Task 4 closure provenance

- Every native/WASM client, shared protocol, compatibility, sync consumer,
  harness, and test file named by the Task 4 brief received a final static
  disposition. Security conclusions remain source proofs because the user
  directed the audit not to send hostile traffic.
- `python3 tests/net_bots_test.py` passed 6/6 and the retained sanitizer CTest
  registration passed 1/1. These are Python unit checks, not socket, browser, or
  C++ sanitizer integration evidence.
- A safe autonomous two-round smoke was unavailable: `tools/net_bots.py` can
  only join a human-created room and cannot create/start it or validate level
  sync/round completion. No server, proxy, client connection, or gameplay
  traffic was started, and existing foreign listeners remained untouched.
- An isolated WASM configure under `/tmp/fb-sdl3-audit/` succeeded. Compilation
  stopped at the documented need to patch Emscripten with SDL3_image and
  SDL3_mixer ports. Direct compilation of both audited client translation units
  then passed with warnings; no link or browser runtime result is claimed.
- Independent read-only review rejected the draft BUG-012 causal chain by
  locating the priority `FB/` diversion, then identified incomplete response
  and TCP-stream candidate coverage. The source recheck dismissed/reserved
  BUG-012, broadened BUG-015, and confirmed BUG-017 before final validation.

## Task 5 closure provenance

- All ten gameplay translation/header files, all six pure helper files, all
  three matching tests, and relevant Perl mechanics received final semantic
  dispositions. Task 4's SEC-003 player/index boundary was consumed without
  generating malformed traffic or recycling its ID.
- The required Release and warnings-strict helper filters passed 3/3. The exact
  leak-enabled sanitizer filter failed because Apple ASan does not support leak
  detection; the accepted leak-disabled ASan+UBSan filter passed 3/3 with no
  diagnostic. Both outcomes remain recorded.
- A fixed-seed pure-helper/oracle harness passed normally and under ASan+UBSan
  for page/team/color/grid boundaries and maximum configured delta. A second
  headless harness linked the unchanged warnings-strict production objects and,
  through a test-TU-only visibility seam, called actual `BubbleGame` count,
  generation, compression, game-state, and loss methods.
- The production-object harness passed normally and under leak-disabled
  ASan+UBSan for player counts 1/2/5/6/20, team counts 1-5, colors 5/8, both
  orientations, three consecutive rounds, Clear Mode, and simultaneous final
  loss. It directly reproduced BUG-018 and BUG-019. No client, listener,
  preference, socket, graphical session, or hostile placement message was
  created. Static causal traces support the remaining findings; omitted runtime
  cases are limitations, not passes.
- Fix Round 1 traced projectile/malus positions and owners, shooter and deferred
  action transitions, target sentinels and update sites, and round/victory
  reset/enforcement state in an explicit invariant ledger. It then extended the
  same production-object harness with the exact native `deltaScale=15` case.
  Linked production `IsCollision`, `GetClosestFreeCell`, and
  `PlacePlayerBubble` calls reproduced BUG-025 normally and under leak-disabled
  ASan+UBSan: both endpoint samples miss the crossed bubble, while the missed
  path later selects the ceiling rather than the adjacent placement. The full
  `UpdatePosition` call remains omitted because it creates the graphical and
  preferences-owning `FrozenBubble` singleton; the production movement formula
  and call order were instead proved statically, so this is not an open
  collision limitation.

## Task 6 closure provenance

- Every file named by the Task 6 brief received a final disposition: the seven
  menu translation units and headers, both settings files, both highscore files,
  both menu-button files, and the input/event/lifecycle paths of
  `frozenbubble.cpp`/`.h`. Rendering and platform aspects of the two
  `frozenbubble` files stay with Tasks 7-8.
- Isolation was proved before any preferences work. macOS `SDL_GetPrefPath`
  ignores `HOME`: the first probe resolved to the user's real preference
  directory, and the harness's own isolation gate aborted with exit 4 before
  opening a file. `CFFIXED_USER_HOME` produced correct isolation, every later
  run asserted `ISOLATION=OK` first, and the user's three real preference files
  were hashed beforehand and verified byte-identical afterwards.
- A test-only translation unit linked the unchanged production `gamesettings`
  object from both the warnings-strict and ASan+UBSan builds and drove the real
  `ReadSettings`/`LoadDefaultKeys`/`SaveKeys` across twelve fixtures: first run,
  empty file, syntax error, missing section/keys, out-of-range numerics,
  non-numeric and NaN numerics, an over-long line, a long nickname, a read-only
  valid file, a read-only malformed file, an unwritable preference directory,
  and an uncreatable preference directory.
- The sanitized production client itself was run five times with dummy video and
  audio drivers, an isolated preference home, and a kill timeout. It reproduced
  the startup hang, both corrupt-highscore aborts, and — through a
  `Contents/Resources/` copy that makes `InitDataDir` derive a missing asset
  directory — the indeterminate-member dereference of BUG-034.
- Analyzer output was re-triaged for the scoped files only: 90 unique cppcheck
  and 79 unique clang-tidy records. Two were promoted on independent evidence
  (BUG-033, and the static-initializer note inside IMP-012); the enum-cast,
  dangling-temporary, and null-`currentGame` families were dismissed with
  recorded counter-evidence.
- No listener, server, socket, browser, peer message, hostile input, or process
  kill was created. The user's real preferences and every foreign process were
  left untouched. Security conclusions, all live-server lobby transitions, and
  the multi-controller hot-plug cases remain source proofs, not observed facts.

## Task 7 closure provenance

- Every file named by the Task 7 brief received a final disposition: the nine
  render/transition/text/audio/compat files in full, the render slices of
  `frozenbubble.cpp`/`.h`, `bubblegame_render.cpp`, and `mainmenu_panels.cpp`
  (platform slices stay with Task 8), and the `server/log.c`/`server/stats.c`
  allocation boundary inherited from Task 3, closed under confirmed IMP-010.
- Step 1's ownership table is the leak instrument because Apple ASan cannot
  detect leaks on this host: every window, renderer, texture family, surface,
  font, effect buffer, mixer, track, and audio object is recorded with
  creator, owner, replacement behavior, and destruction path. The two leak
  defects it exposed are backed by RSS measurement (BUG-041, exactly
  1.2 MB/frame over 100 production `synchro_after` frames, and 21→149 MB
  across five full production `DoSnipIn`/`TakeSnipOut` cycles) and by a
  grep-verified absence of any destroy site (BUG-042).
- A harness linked the unchanged production `shaderstuff`, `transitionmanager`,
  `audiomixer`, `ttftext`, `gamesettings`, and `platform` objects from both
  the warnings-strict and ASan+UBSan trees. Preference isolation was proven
  with `CFFIXED_USER_HOME` before any stateful run (`ISOLATION=OK`; the
  user's three real preference files were hashed before and verified
  byte-identical after). Runs covered: image-format/pitch verification of all
  eight shipped effect inputs, an ASan demonstration of the `get_pixel` clamp
  off-by-one on the production object, both `TextureEx` failure crashes
  (UBSan exits 134), six sanitized full transition animations with no
  diagnostic, three sanitized audio lifecycle cycles ending in a clean
  `Dispose`, and 200 sanitized text lifecycle iterations.
- IMP-013's out-of-bounds arithmetic was proven by sanitizer but dismissed as
  a shipped-asset defect with measured counter-evidence: `fblogo-mask.png`
  has zero white border pixels, so the `points_` walk cannot reach the
  admitted one-past-the-end index; every other caller stays within `w-2`/`h-2`
  guards.
- All rendering used the dummy video driver's software renderer; no Metal/GPU
  session, fullscreen or resize toggle, real audio device, or full-client
  menu-to-game navigation was driven, and BUG-042 is a complete static causal
  proof rather than a runtime reproduction. No listener, server, socket,
  browser, hostile input, or process kill was created; security-specific
  runtime testing remained out of scope by user direction.

## Commands and evidence

Each row records exactly one top-level shell command. A shell loop remains one
syntactic command, but unnamed multi-command “gate” rows are not used. Exit
values and material output are from captured Task 1, fix-round, Task 2, Task 3,
Task 4, Task 5, and Task 6 evidence. Task 6 read its scoped files with the
agent's file reader rather than `nl`/`sed`, so only its shell commands appear
below; the files it read are listed in the
[Task 6 notebook scope](subsystems/04-lobby-settings-input.md#scope).

**Canonical log cutoff:** completed Task 7's ownership/pixel/lifecycle review,
isolated production-object harness runs, and ledger updates. Task 7's final
validation, staging, commit, and post-commit checks belong in its ignored
controller report, preventing a false claim that a commit records itself. The
same exclusion already covers Task 2, the Task 5 fix round, and Task 6.

| Timestamp (UTC) | Command | Exit | Concise result | Evidence |
|---|---|---:|---|---|
| 2026-07-28T02:30:04Z | <code>git status --short --branch</code> | 0 | Clean codex/sdl3-complete-audit worktree | Audit baseline above |
| 2026-07-28T02:30:04Z | <code>git rev-parse main</code> | 0 | 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 | Audit baseline above |
| 2026-07-28T02:30:04Z | <code>git diff --name-status main...HEAD</code> | 0 | Only the approved design and execution plan were added | [Design](../superpowers/specs/2026-07-28-complete-repository-audit-design.md), [plan](../superpowers/plans/2026-07-28-complete-repository-audit.md) |
| 2026-07-28T02:30:04Z | <code>git show --stat --oneline 09d6c7bfcd864a0ad3951b87d16a88dc770392a3</code> | 0 | Resolved baseline; 9 files, 104 insertions, 33 deletions | Audit baseline above |
| 2026-07-28T02:30:04Z | <code>unlink AGENTS.md</code> | 0 | No output; removal succeeded before relative-link creation | Captured symlink sequence |
| 2026-07-28T02:30:04Z | <code>ln -s CLAUDE.md AGENTS.md</code> | 0 | No output; relative symlink created | Captured symlink sequence |
| 2026-07-28T02:30:04Z | <code>readlink AGENTS.md</code> | 0 | CLAUDE.md | Repository root |
| 2026-07-28T02:30:04Z | <code>test "$(readlink AGENTS.md)" = "CLAUDE.md"</code> | 0 | Exact relative target matched | Repository root |
| 2026-07-28T02:30:04Z | <code>printf 'target=%s\n' "$(readlink AGENTS.md)"</code> | 0 | target=CLAUDE.md | Repository root |
| 2026-07-28T02:30:04Z | <code>ls -ld AGENTS.md</code> | 0 | AGENTS.md -> CLAUDE.md | Repository root |
| 2026-07-28T02:34:39Z | <code>test -L AGENTS.md</code> | 0 | AGENTS.md is a symlink | Bootstrap consistency |
| 2026-07-28T02:34:39Z | <code>test -f docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Canonical status exists | This document |
| 2026-07-28T02:34:39Z | <code>test -f docs/audit/FILE_COVERAGE.md</code> | 0 | Coverage ledger exists | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>test -f docs/audit/FINDINGS.md</code> | 0 | Finding registry exists | [FINDINGS.md](FINDINGS.md) |
| 2026-07-28T02:34:39Z | <code>test "$(find docs/audit/subsystems -type f -name '*.md' &#124; wc -l &#124; tr -d ' ')" = "9"</code> | 0 | Nine subsystem notebooks found | [Subsystem notebooks](subsystems/) |
| 2026-07-28T02:34:39Z | <code>git diff --check</code> | 0 | No whitespace errors | Bootstrap consistency |
| 2026-07-28T02:34:39Z | <code>test "$(readlink AGENTS.md)" = "CLAUDE.md"</code> | 0 | Exact relative target matched | Repository root |
| 2026-07-28T02:34:39Z | <code>test "$(awk -F'`' '/^\&#124; `/ {count++} END {print count+0}' docs/audit/FILE_COVERAGE.md)" = "237"</code> | 0 | coverage_rows=237 | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>awk -F'`' '/^\&#124; `/ {print $2}' docs/audit/FILE_COVERAGE.md &gt; /tmp/fb-sdl3-audit-ledger-paths.txt</code> | 0 | Wrote 237 ledger paths; no stdout | Inventory equality |
| 2026-07-28T02:34:39Z | <code>rg '^(src&#124;server&#124;tests&#124;tools&#124;android&#124;web&#124;cmake&#124;docker&#124;\.github)/&#124;^(CMakeLists\.txt&#124;CMakeListsEmscripten\.txt&#124;README\.md&#124;SetupServer\.md&#124;WASM_PORT\.md&#124;start-server\.sh&#124;netlify\.toml&#124;shell\.nix&#124;default\.nix&#124;flake\.nix&#124;flake\.lock)$' /tmp/fb-sdl3-audit-tracked-files.txt &gt; /tmp/fb-sdl3-audit-filtered-paths.txt</code> | 0 | Wrote 237 filtered paths; no stdout | Inventory equality |
| 2026-07-28T02:34:39Z | <code>cmp /tmp/fb-sdl3-audit-filtered-paths.txt /tmp/fb-sdl3-audit-ledger-paths.txt</code> | 0 | No output; path lists are identical | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>for task_notebook in docs/audit/subsystems/*.md; do for task_section in 'Scope' 'Trust boundaries and invariants' 'Static review' 'Dynamic evidence' 'Candidates' 'Confirmed findings' 'Dismissed candidates' 'Coverage' 'Limitations' 'Gate conclusion'; do test "$(rg -c "^## ${task_section}$" "$task_notebook")" = "1"; done; done</code> | 0 | All 9 notebooks have all 10 headings exactly once | [Subsystem notebooks](subsystems/) |
| 2026-07-28T02:34:39Z | <code>task_gitlinks=$(rg -c '^\&#124; `android/app/jni/SDL3(_image&#124;_mixer&#124;_ttf)?` .*Vendored; boundary review pending' docs/audit/FILE_COVERAGE.md)</code> | 0 | Assigned `task_gitlinks=4`; no stdout | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>task_sdl2=$(rg -c '^\&#124; `android/app/jni/include/SDL2/' docs/audit/FILE_COVERAGE.md)</code> | 0 | Assigned `task_sdl2=97`; no stdout | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>task_sdl2_marked=$(rg -c '^\&#124; `android/app/jni/include/SDL2/.*Vendored; boundary review pending' docs/audit/FILE_COVERAGE.md)</code> | 0 | Assigned `task_sdl2_marked=97`; no stdout | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>task_sdl_java=$(rg -c '^\&#124; `android/app/src/main/java/org/libsdl/' docs/audit/FILE_COVERAGE.md)</code> | 0 | Assigned `task_sdl_java=11`; no stdout | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>task_sdl_java_marked=$(rg -c '^\&#124; `android/app/src/main/java/org/libsdl/.*Vendored; boundary review pending' docs/audit/FILE_COVERAGE.md)</code> | 0 | Assigned `task_sdl_java_marked=11`; no stdout | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>task_ini=$(rg -c '^\&#124; `android/app/jni/iniparser/' docs/audit/FILE_COVERAGE.md)</code> | 0 | Assigned `task_ini=4`; no stdout | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>task_ini_marked=$(rg -c '^\&#124; `android/app/jni/iniparser/.*Vendored; boundary review pending' docs/audit/FILE_COVERAGE.md)</code> | 0 | Assigned `task_ini_marked=4`; no stdout | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>test "$task_gitlinks" = 4</code> | 0 | Gitlink boundary count matched 4 | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>test "$task_sdl2" = "$task_sdl2_marked"</code> | 0 | All 97 SDL2 headers boundary-marked | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>test "$task_sdl_java" = "$task_sdl_java_marked"</code> | 0 | All 11 SDL Java files boundary-marked | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>test "$task_ini" = 4</code> | 0 | Four duplicated iniparser files found | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>test "$task_ini" = "$task_ini_marked"</code> | 0 | All four iniparser duplicates boundary-marked | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:34:39Z | <code>rg -n '^\- Exact next action: Begin Task 2, Step 1:' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Exact next action found | Current state above |
| 2026-07-28T02:34:39Z | <code>rg -n '^\&#124; Task 1 .*\&#124; complete \&#124;$' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Task 1 complete row found | Gate checklist above |
| 2026-07-28T02:34:39Z | <code>rg -n '^\&#124; Task 2 .*\&#124; pending \&#124;$' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Task 2 pending row found | Gate checklist above |
| 2026-07-28T02:34:39Z | <code>test -z "$(git diff --name-only -- src server tests tools android web cmake docker .github CMakeLists.txt CMakeListsEmscripten.txt README.md SetupServer.md WASM_PORT.md start-server.sh netlify.toml shell.nix default.nix flake.nix flake.lock)"</code> | 0 | Production source/config diff empty | Audit mode above |
| 2026-07-28T02:36:32Z | <code>git add AGENTS.md docs/audit</code> | 0 | 13 approved paths staged; no stdout | Original staged snapshot |
| 2026-07-28T02:36:32Z | <code>git diff --cached --check</code> | 0 | No output after whitespace correction | Original staged snapshot |
| 2026-07-28T02:36:32Z | <code>test -L AGENTS.md</code> | 0 | Symlink exists | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>test "$(readlink AGENTS.md)" = "CLAUDE.md"</code> | 0 | Working-tree target is CLAUDE.md | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>test "$(git ls-files -s AGENTS.md &#124; awk '{print $1}')" = "120000"</code> | 0 | Staged mode is 120000 | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>test "$(git show :AGENTS.md)" = "CLAUDE.md"</code> | 0 | Staged blob target is CLAUDE.md | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>test -f docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Status exists | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>test -f docs/audit/FILE_COVERAGE.md</code> | 0 | Coverage ledger exists | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>test -f docs/audit/FINDINGS.md</code> | 0 | Finding registry exists | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>test "$(find docs/audit/subsystems -type f -name '*.md' &#124; wc -l &#124; tr -d ' ')" = "9"</code> | 0 | Nine notebooks found | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>test "$(awk -F'`' '/^\&#124; `/ {count++} END {print count+0}' docs/audit/FILE_COVERAGE.md)" = "237"</code> | 0 | 237 coverage rows | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>awk -F'`' '/^\&#124; `/ {print $2}' docs/audit/FILE_COVERAGE.md &gt; /tmp/fb-sdl3-audit-ledger-paths.txt</code> | 0 | Wrote committed-candidate path list; no stdout | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>cmp /tmp/fb-sdl3-audit-filtered-paths.txt /tmp/fb-sdl3-audit-ledger-paths.txt</code> | 0 | No output; inventory paths match | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>for task_notebook in docs/audit/subsystems/*.md; do for task_section in 'Scope' 'Trust boundaries and invariants' 'Static review' 'Dynamic evidence' 'Candidates' 'Confirmed findings' 'Dismissed candidates' 'Coverage' 'Limitations' 'Gate conclusion'; do test "$(rg -c "^## ${task_section}$" "$task_notebook")" = "1"; done; done</code> | 0 | Nine notebook schemas valid | Original final pre-commit check |
| 2026-07-28T02:36:32Z | <code>for task_class in BUG SEC REL IMP; do rg -q "\`${task_class}-###\`" docs/audit/FINDINGS.md; done</code> | 0 | All four stable ID classes found | [FINDINGS.md](FINDINGS.md) |
| 2026-07-28T02:36:32Z | <code>for task_severity in Critical High Medium Low; do rg -q "\*\*${task_severity}:\*\*" docs/audit/FINDINGS.md; done</code> | 0 | All four defect severities found | [FINDINGS.md](FINDINGS.md) |
| 2026-07-28T02:36:32Z | <code>rg -q '^\&#124; ID \&#124; State \&#124; Severity/Priority \&#124; Confidence \&#124; Gate \&#124; Summary \&#124; Evidence \&#124;$' docs/audit/FINDINGS.md</code> | 0 | Registry header found | [FINDINGS.md](FINDINGS.md) |
| 2026-07-28T02:36:32Z | <code>rg -q '^\- Exact next action: Begin Task 2, Step 1:' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Task 2 handoff found | Current state above |
| 2026-07-28T02:36:32Z | <code>test "$(git diff --cached --name-only &#124; wc -l &#124; tr -d ' ')" = "13"</code> | 0 | Exactly 13 staged paths | Original staged snapshot |
| 2026-07-28T02:36:32Z | <code>test -z "$(git diff --cached --name-only &#124; rg -v '^(AGENTS\.md&#124;docs/audit/)')"</code> | 0 | No staged path outside approved scope | Original staged snapshot |
| 2026-07-28T02:36:32Z | <code>git diff --quiet</code> | 0 | No unstaged diff | Original staged snapshot |
| 2026-07-28T02:36:32Z | <code>printf 'FINAL PRECOMMIT PASS: 13 approved staged files; cached diff clean; 237/237 inventory; 9/9 notebooks; Task 2 handoff; no unstaged or production-source changes\n'</code> | 0 | Printed the quoted PASS summary | Original staged snapshot |
| 2026-07-28T02:37:08Z | <code>git commit -m "docs: bootstrap resumable SDL3 audit"</code> | 0 | Created 9f589b1d561436d8da269ad8ea622c639a2f027d; 13 files, 731 insertions, 110 deletions | Original Task 1 commit |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git log -1 --format='commit=%H%nsubject=%s%nparent=%P'</code> | 0 | 9f589b1d; exact subject; parent 3f57ce9c | Original Task 1 commit |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git status --short --branch</code> | 0 | ## codex/sdl3-complete-audit | Original post-commit check |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git status --porcelain</code> | 0 | No output | Original post-commit check |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git show --check --stat --oneline HEAD</code> | 0 | 9f589b1d docs: bootstrap resumable SDL3 audit; no check errors | Original Task 1 commit |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git diff-tree --no-commit-id --name-only -r HEAD &#124; wc -l &#124; tr -d ' '</code> | 0 | 13 | Original committed scope |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git diff-tree --no-commit-id --name-only -r HEAD &#124; rg -v '^(AGENTS\.md&#124;docs/audit/)'</code> | 1 | No output; no out-of-scope path | Expected no-match assertion |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git diff-tree --no-commit-id --name-status -r HEAD</code> | 0 | One type-change plus 12 added audit files | Original committed scope |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git ls-tree HEAD AGENTS.md &#124; awk '{print $1}'</code> | 0 | 120000 | Original committed symlink |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git show HEAD:AGENTS.md</code> | 0 | CLAUDE.md | Original committed symlink |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git show HEAD:docs/audit/FILE_COVERAGE.md &#124; awk -F'`' '/^\&#124; `/ {print $2}' &gt; /tmp/fb-sdl3-audit-committed-ledger-paths.txt</code> | 0 | Wrote 237 committed ledger paths; no stdout | Original committed inventory |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>wc -l &lt; /tmp/fb-sdl3-audit-committed-ledger-paths.txt &#124; tr -d ' '</code> | 0 | 237 | Original committed inventory |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>cmp /tmp/fb-sdl3-audit-filtered-paths.txt /tmp/fb-sdl3-audit-committed-ledger-paths.txt</code> | 0 | No output; exact match | Original committed inventory |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>task_pending=$(git show HEAD:docs/audit/FILE_COVERAGE.md &#124; rg -c '\&#124; Pending review \&#124;')</code> | 0 | Assigned `task_pending=120`; no stdout | Original committed dispositions |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>task_boundary=$(git show HEAD:docs/audit/FILE_COVERAGE.md &#124; rg -c '\&#124; Vendored; boundary review pending \&#124;')</code> | 0 | Assigned `task_boundary=116`; no stdout | Original committed dispositions |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>task_derived=$(git show HEAD:docs/audit/FILE_COVERAGE.md &#124; rg -c '\&#124; Generated/platform-derived validation pending \&#124;')</code> | 0 | Assigned `task_derived=1`; no stdout | Original committed dispositions |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>test "$((task_pending + task_boundary + task_derived))" = 237</code> | 0 | Disposition total matched 237 | Original committed dispositions |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>for task_notebook in $(git ls-tree -r --name-only HEAD docs/audit/subsystems); do for task_section in 'Scope' 'Trust boundaries and invariants' 'Static review' 'Dynamic evidence' 'Candidates' 'Confirmed findings' 'Dismissed candidates' 'Coverage' 'Limitations' 'Gate conclusion'; do test "$(git show "HEAD:${task_notebook}" &#124; rg -c "^## ${task_section}$")" = "1"; done; done</code> | 0 | Nine committed notebooks have ten headings each | Original committed notebooks |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git show HEAD:docs/audit/SDL3_REVIEW_STATUS.md &#124; rg -n '^\- Active gate: Task 2 \(pending\)$'</code> | 0 | Active gate Task 2 found | Original committed handoff |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git show HEAD:docs/audit/SDL3_REVIEW_STATUS.md &#124; rg -n '^\- Exact next action: Begin Task 2, Step 1:'</code> | 0 | Exact next action found | Original committed handoff |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git show HEAD:docs/audit/SDL3_REVIEW_STATUS.md &#124; rg -c '^\&#124; Task ([1-9]&#124;1[0-3]) \&#124;'</code> | 0 | 13 | Original committed handoff |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git diff-tree --no-commit-id --name-only -r HEAD &#124; rg '^(src&#124;server&#124;tests&#124;tools&#124;android&#124;web&#124;cmake&#124;docker&#124;\.github)/&#124;^(CMakeLists\.txt&#124;CMakeListsEmscripten\.txt&#124;README\.md&#124;SetupServer\.md&#124;WASM_PORT\.md&#124;start-server\.sh&#124;netlify\.toml&#124;shell\.nix&#124;default\.nix&#124;flake\.nix&#124;flake\.lock)$'</code> | 1 | No output; no production source/config change | Expected no-match assertion |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>test -f .superpowers/sdd/2026-07-28-complete-repository-audit/task-1-report.md</code> | 0 | Controller report exists | Ignored controller report |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>git check-ignore -q .superpowers/sdd/2026-07-28-complete-repository-audit/task-1-report.md</code> | 0 | Report is ignored | Ignored controller report |
| 2026-07-28 (after 9f589b1d; exact time not captured) | <code>test -z "$(git status --porcelain)"</code> | 0 | Worktree clean | Original final handoff |
| 2026-07-28T02:43:42Z | <code>date -u '+%Y-%m-%dT%H:%M:%SZ'</code> | 0 | 2026-07-28T02:43:42Z | Round 1 timestamp |
| 2026-07-28T02:43:42Z | <code>uname -a</code> | 0 | Darwin 25.5.0, arm64 | Session environment above |
| 2026-07-28T02:43:42Z | <code>sw_vers</code> | 0 | macOS 26.5.2, build 25F84 | Session environment above |
| 2026-07-28T02:43:42Z | <code>c++ --version</code> | 0 | Apple clang 21.0.0 | Session environment above |
| 2026-07-28T02:43:42Z | <code>cmake --version</code> | 0 | CMake 4.3.4 | Session environment above |
| 2026-07-28T02:43:42Z | <code>ninja --version</code> | 0 | 1.13.2 | Session environment above |
| 2026-07-28T02:43:42Z | <code>python3 --version</code> | 0 | Python 3.14.6 | Session environment above |
| 2026-07-28T02:43:42Z | <code>java -version</code> | 0 | OpenJDK 17.0.19 | Session environment above |
| 2026-07-28T02:43:42Z | <code>./android/gradlew --version</code> | 0 | Gradle 8.2 | Session environment above |
| 2026-07-28T02:43:42Z | <code>printf '%s\n' "${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"</code> | 0 | /opt/homebrew/share/android-commandlinetools | Session environment above |
| 2026-07-28T02:43:42Z | <code>find /opt/homebrew/share/android-commandlinetools/ndk -mindepth 1 -maxdepth 1 -type d -print &#124; sort</code> | 0 | NDK 25.2.9519653 | Session environment above |
| 2026-07-28T02:43:42Z | <code>sdkmanager --version</code> | 0 | 20.0 | Session environment above |
| 2026-07-28T02:43:42Z | <code>emcc --version</code> | 127 | zsh: command not found: emcc | Limitations below |
| 2026-07-28T02:43:42Z | <code>cppcheck --version</code> | 127 | zsh: command not found: cppcheck | Limitations below |
| 2026-07-28T02:43:42Z | <code>clang-tidy --version</code> | 127 | zsh: command not found: clang-tidy | Limitations below |
| 2026-07-28T02:43:42Z | <code>git show -s --format=%cI 9f589b1d561436d8da269ad8ea622c639a2f027d</code> | 0 | 2026-07-28T09:37:08+07:00 | Original Task 1 commit |
| 2026-07-28T02:43:42Z | <code>git ls-tree -r --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 &gt; /tmp/fb-sdl3-audit-tracked-files.txt</code> | 0 | Recreated pinned inventory; no stdout | Inventory artifact |
| 2026-07-28T02:43:42Z | <code>rg '^(src&#124;server&#124;tests&#124;tools&#124;android&#124;web&#124;cmake&#124;docker&#124;\.github)/&#124;^(CMakeLists\.txt&#124;CMakeListsEmscripten\.txt&#124;README\.md&#124;SetupServer\.md&#124;WASM_PORT\.md&#124;start-server\.sh&#124;netlify\.toml&#124;shell\.nix&#124;default\.nix&#124;flake\.nix&#124;flake\.lock)$' /tmp/fb-sdl3-audit-tracked-files.txt</code> | 1 | No matches because producer and consumer were mistakenly concurrent | Ordering failure; rerun next |
| 2026-07-28T02:43:42Z | <code>rg '^(src&#124;server&#124;tests&#124;tools&#124;android&#124;web&#124;cmake&#124;docker&#124;\.github)/&#124;^(CMakeLists\.txt&#124;CMakeListsEmscripten\.txt&#124;README\.md&#124;SetupServer\.md&#124;WASM_PORT\.md&#124;start-server\.sh&#124;netlify\.toml&#124;shell\.nix&#124;default\.nix&#124;flake\.nix&#124;flake\.lock)$' /tmp/fb-sdl3-audit-tracked-files.txt</code> | 0 | 237 paths; first workflow, last web/shell.html | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:43:42Z | <code>git ls-tree 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 android/app/jni/SDL3 android/app/jni/SDL3_image android/app/jni/SDL3_mixer android/app/jni/SDL3_ttf</code> | 0 | Four mode-160000 gitlinks | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:43:42Z | <code>cmp -s android/app/jni/iniparser/dictionary.c third_party/iniparser/dictionary.c</code> | 0 | No output; byte-identical | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:43:42Z | <code>cmp -s android/app/jni/iniparser/dictionary.h third_party/iniparser/dictionary.h</code> | 0 | No output; byte-identical | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:43:42Z | <code>cmp -s android/app/jni/iniparser/iniparser.c third_party/iniparser/iniparser.c</code> | 0 | No output; byte-identical | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:43:42Z | <code>cmp -s android/app/jni/iniparser/iniparser.h third_party/iniparser/iniparser.h</code> | 0 | No output; byte-identical | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T02:47:35Z | <code>date -u '+%Y-%m-%dT%H:%M:%SZ'</code> | 0 | 2026-07-28T02:47:35Z | Round 1 validation timestamp |
| 2026-07-28T02:47:35Z | <code>test "$(git diff --name-only &#124; wc -l &#124; tr -d ' ')" = "1"</code> | 0 | One tracked path changed | Round 1 first focused run |
| 2026-07-28T02:47:35Z | <code>test "$(git diff --name-only)" = "docs/audit/SDL3_REVIEW_STATUS.md"</code> | 0 | Only canonical status changed | Round 1 first focused run |
| 2026-07-28T02:47:35Z | <code>git diff --check</code> | 0 | No whitespace errors | Round 1 first focused run |
| 2026-07-28T02:47:35Z | <code>awk '/^\&#124; Timestamp \(UTC\)/ {in_table=1} in_table &amp;&amp; /^$/ {in_table=0} in_table {line=$0; gsub(/\\\&#124;/, "", line); if (split(line, fields, "&#124;") != 7) {print "malformed table row: " NR ":" $0 &gt; "/dev/stderr"; exit 1} rows++} END {if (rows &lt; 50) exit 1; print "command_table_rows=" rows}' docs/audit/SDL3_REVIEW_STATUS.md</code> | 1 | Malformed row 93: raw regex pipes split columns | Round 1 first focused run |
| 2026-07-28T02:47:35Z | <code>test "$(git diff --name-only &#124; wc -l &#124; tr -d ' ')" = "1"</code> | 0 | One tracked path changed | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>test "$(git diff --name-only)" = "docs/audit/SDL3_REVIEW_STATUS.md"</code> | 0 | Only canonical status changed | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>git diff --check</code> | 0 | No whitespace errors | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>awk '/^\&#124; Timestamp \(UTC\)/ {in_table=1} in_table &amp;&amp; /^$/ {in_table=0} in_table {line=$0; gsub(/\\\&#124;/, "", line); if (split(line, fields, "&#124;") != 7) {print "malformed table row: " NR ":" $0 &gt; "/dev/stderr"; exit 1} rows++} END {if (rows &lt; 50) exit 1; print "command_table_rows=" rows}' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | command_table_rows=54 | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>! rg -n '0 except&#124;Host and tool version probes&#124;Task 1 staged consistency checks plus' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | No old aggregate markers found | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>for task_cmd in 'uname -a' 'sw_vers' 'c++ --version' 'cmake --version' 'ninja --version' 'python3 --version' 'java -version' './android/gradlew --version' 'sdkmanager --version'; do rg -Fq "\`$task_cmd\` &#124; 0 &#124;" docs/audit/SDL3_REVIEW_STATUS.md; done</code> | 0 | All available-tool exit-0 rows found | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>for task_cmd in 'emcc --version' 'cppcheck --version' 'clang-tidy --version'; do rg -Fq "\`$task_cmd\` &#124; 127 &#124;" docs/audit/SDL3_REVIEW_STATUS.md; done</code> | 0 | All unavailable-tool exit-127 rows found | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>rg -Fq '`git commit -m "docs: bootstrap resumable SDL3 audit"` &#124; 0 &#124; Created `9f589b1d561436d8da269ad8ea622c639a2f027d`' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Original commit evidence found | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>for task_evidence in 'Committed-path scope gate' 'Committed-symlink gate' 'Committed-inventory equality gate' 'Committed-disposition count gate' 'Committed-notebook schema gate' 'Committed-status handoff gate' 'Committed production-immutability gate'; do rg -Fq "$task_evidence &#124; 0 &#124;" docs/audit/SDL3_REVIEW_STATUS.md; done</code> | 0 | All named post-commit rows found | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>rg -q '^\- Exact next action: Begin Task 2, Step 1:' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Task 2 handoff found | Round 1 corrected run |
| 2026-07-28T02:47:35Z | <code>printf 'FOCUSED STATUS PASS: tracked_scope=1 table_well_formed=yes aggregates_removed=yes precise_tool_exits=yes original_commit=yes postcommit_checks=present handoff=Task2 diff_check=clean\n'</code> | 0 | Printed the quoted PASS summary | Round 1 corrected run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test "$(git diff --name-only &#124; wc -l &#124; tr -d ' ')" = "1"</code> | 0 | One tracked path changed | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test "$(git diff --name-only)" = "docs/audit/SDL3_REVIEW_STATUS.md"</code> | 0 | Only canonical status changed | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git diff --check</code> | 0 | No whitespace errors | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>task_rows=$(awk '/^\&#124; Timestamp \(UTC\)/ {in_table=1} in_table &amp;&amp; /^$/ {in_table=0} in_table {line=$0; gsub(/\\\&#124;/, "", line); if (split(line, fields, "&#124;") != 7) exit 1; rows++} END {print rows}' docs/audit/SDL3_REVIEW_STATUS.md)</code> | 0 | Assigned `task_rows=57`; no stdout | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test "$task_rows" = "57"</code> | 0 | Row-count assertion passed | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>! rg -n '0 except&#124;Host and tool version probes&#124;Task 1 staged consistency checks plus' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | No old aggregate markers found | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq '`git commit -m "docs: bootstrap resumable SDL3 audit"` &#124; 0 &#124; Created `9f589b1d561436d8da269ad8ea622c639a2f027d`' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Original commit row found | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq '`git status --porcelain` &#124; 0 &#124; No output; worktree clean' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Original clean-state row found | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq 'Committed-inventory equality gate &#124; 0 &#124; 237 committed rows; exact path-list match' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Original inventory equality row found | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq 'Fix Round 1 focused status-document gate, first run &#124; 1 &#124;' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Failed focused-run row found | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq 'Fix Round 1 focused status-document gate, corrected rerun &#124; 0 &#124;' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Corrected focused-run row found | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -q '^\- Exact next action: Begin Task 2, Step 1:' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Task 2 handoff found | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>printf 'FINAL FOCUSED STATUS PASS: command_rows=57 tracked_scope=1 table_well_formed=yes precise_exits=yes commit_and_postcommit=yes failed_checks_honest=yes handoff=Task2 diff_check=clean\n'</code> | 0 | Printed the quoted PASS summary | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git diff --stat</code> | 0 | One status file; 57 insertions, 7 deletions | Round 1 final amended-document run |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git add docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | No output | Round 1 fix staging |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git diff --cached --check</code> | 0 | No output | Round 1 fix staging |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git diff --cached --name-only</code> | 0 | docs/audit/SDL3_REVIEW_STATUS.md | Round 1 fix staging |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git diff --name-only</code> | 0 | No output after staging | Round 1 fix staging |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>task_rows=$(awk '/^\&#124; Timestamp \(UTC\)/ {in_table=1} in_table &amp;&amp; /^$/ {in_table=0} in_table {line=$0; gsub(/\\\&#124;/, "", line); if (split(line, fields, "&#124;") != 7) exit 1; rows++} END {print rows}' docs/audit/SDL3_REVIEW_STATUS.md)</code> | 0 | Assigned `task_rows=57`; no stdout | Round 1 fix staging |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test "$task_rows" = "57"</code> | 0 | Row-count assertion passed | Round 1 fix staging |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>printf 'FINAL CACHED FIX PASS: command_rows=57 staged_files=1 table_well_formed=yes exact_exits=yes original_commit=yes postcommit=yes honest_failures=yes chronological_groups=yes handoff=Task2 cached_diff_check=clean unstaged=empty\n'</code> | 0 | Printed the quoted PASS summary | Round 1 fix staging |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git diff --cached --stat</code> | 0 | One status file; 57 insertions, 7 deletions | Round 1 fix staging |
| 2026-07-28 (Round 1 fix commit; exact time not captured) | <code>git commit -m "docs: expand SDL3 audit command evidence"</code> | 0 | Created 820892d72fef1fafca16ac2d5bf31607a88ae5a1; one file, 57 insertions, 7 deletions | Round 1 fix commit |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git log -1 --format='commit=%H%nsubject=%s%nparent=%P'</code> | 0 | 820892d7; exact subject; parent 9f589b1d | Round 1 fix commit |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git status --short --branch</code> | 0 | ## codex/sdl3-complete-audit | Round 1 post-commit |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git status --porcelain</code> | 0 | No output | Round 1 post-commit |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git show --check --stat --oneline HEAD</code> | 0 | 820892d7 docs: expand SDL3 audit command evidence; no check errors | Round 1 fix commit |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git diff-tree --no-commit-id --name-only -r HEAD &#124; wc -l &#124; tr -d ' '</code> | 0 | 1 | Round 1 committed scope |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git diff-tree --no-commit-id --name-only -r HEAD</code> | 0 | docs/audit/SDL3_REVIEW_STATUS.md | Round 1 committed scope |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git show HEAD:docs/audit/SDL3_REVIEW_STATUS.md &gt; /tmp/fb-sdl3-audit-fix-round-1-status.md</code> | 0 | Wrote committed status; no stdout | Round 1 committed document |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>task_rows=$(awk '/^\&#124; Timestamp \(UTC\)/ {in_table=1} in_table &amp;&amp; /^$/ {in_table=0} in_table {line=$0; gsub(/\\\&#124;/, "", line); if (split(line, fields, "&#124;") != 7) exit 1; rows++} END {print rows}' /tmp/fb-sdl3-audit-fix-round-1-status.md)</code> | 0 | Assigned `task_rows=57`; no stdout | Round 1 committed document |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test "$task_rows" = "57"</code> | 0 | Row-count assertion passed | Round 1 committed document |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>! rg -n '0 except&#124;Host and tool version probes&#124;Task 1 staged consistency checks plus' /tmp/fb-sdl3-audit-fix-round-1-status.md</code> | 0 | No old aggregate markers found | Round 1 committed document |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq '`git commit -m "docs: bootstrap resumable SDL3 audit"` &#124; 0 &#124; Created `9f589b1d561436d8da269ad8ea622c639a2f027d`' /tmp/fb-sdl3-audit-fix-round-1-status.md</code> | 0 | Original commit evidence found | Round 1 committed document |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq '`git status --porcelain` &#124; 0 &#124; No output; worktree clean' /tmp/fb-sdl3-audit-fix-round-1-status.md</code> | 0 | Original clean-state evidence found | Round 1 committed document |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq 'Committed-inventory equality gate &#124; 0 &#124; 237 committed rows; exact path-list match' /tmp/fb-sdl3-audit-fix-round-1-status.md</code> | 0 | Original inventory evidence found | Round 1 committed document |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -q '^\- Exact next action: Begin Task 2, Step 1:' /tmp/fb-sdl3-audit-fix-round-1-status.md</code> | 0 | Task 2 handoff retained | Round 1 committed document |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test -f .superpowers/sdd/2026-07-28-complete-repository-audit/task-1-report.md</code> | 0 | Controller report exists | Round 1 report check |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq '820892d72fef1fafca16ac2d5bf31607a88ae5a1' .superpowers/sdd/2026-07-28-complete-repository-audit/task-1-report.md</code> | 0 | Fix commit recorded | Round 1 report check |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>git check-ignore -q .superpowers/sdd/2026-07-28-complete-repository-audit/task-1-report.md</code> | 0 | Controller report ignored | Round 1 report check |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test -z "$(git status --porcelain)"</code> | 0 | Worktree clean | Round 1 final handoff |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test "$(git log -1 --format=%H)" = "820892d72fef1fafca16ac2d5bf31607a88ae5a1"</code> | 0 | HEAD is Round 1 fix commit | Round 1 final handoff |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>test "$(git diff-tree --no-commit-id --name-only -r HEAD)" = "docs/audit/SDL3_REVIEW_STATUS.md"</code> | 0 | Round 1 fix scope is status-only | Round 1 final handoff |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>rg -Fq 'Unavoidable circularity:' .superpowers/sdd/2026-07-28-complete-repository-audit/task-1-report.md</code> | 0 | Cutoff concern recorded | Round 1 final report check |
| 2026-07-28 (after 820892d7; exact time not captured) | <code>printf 'FINAL ROUND 1 PASS: circularity_concern=reported report=ignored worktree=clean head=820892d7\n'</code> | 0 | Printed the quoted PASS summary | Round 1 final handoff |
| 2026-07-28 (Task 2 Step 1; exact time not captured) | <code>mkdir -p /tmp/fb-sdl3-audit</code> | 0 | Created/confirmed the approved analyzer-artifact directory; no output | `/tmp/fb-sdl3-audit` |
| 2026-07-28 (Task 2 Step 1; exact time not captured) | <code>command -v cppcheck &#124;&#124; brew install cppcheck</code> | 0 | Initially absent; installed cppcheck 2.21.0 and tinyxml2 11.0.0 successfully | Session environment above |
| 2026-07-28 (Task 2 Step 1; exact time not captured) | <code>test -x "$(brew --prefix llvm 2&gt;/dev/null)/bin/clang-tidy" &#124;&#124; brew install llvm</code> | 0 | Initially absent; installed keg-only LLVM 22.1.8 and z3 4.16.0 successfully | Session environment above |
| 2026-07-28 (Task 2 Step 1; exact time not captured) | <code>command -v emcc &#124;&#124; brew install emscripten</code> | 0 | Initially absent; installed Emscripten 6.0.4 and 18 dependencies; Homebrew also upgraded five dependencies | Session environment above |
| 2026-07-28T03:25:33Z | <code>uname -a</code> | 0 | Darwin 25.5.0, RELEASE_ARM64_T8142, arm64 | Session environment above |
| 2026-07-28T03:25:33Z | <code>sw_vers</code> | 0 | macOS 26.5.2, build 25F84 | Session environment above |
| 2026-07-28T03:25:33Z | <code>cmake --version</code> | 0 | CMake 4.3.4 | Session environment above |
| 2026-07-28T03:25:33Z | <code>ninja --version</code> | 0 | Ninja 1.13.2 | Session environment above |
| 2026-07-28T03:25:33Z | <code>clang --version</code> | 0 | Apple clang 21.0.0, target arm64-apple-darwin25.5.0 | Session environment above |
| 2026-07-28T03:25:33Z | <code>python3 --version</code> | 0 | Python 3.14.6 | Session environment above |
| 2026-07-28T03:25:33Z | <code>java -version</code> | 0 | OpenJDK 17.0.19 Homebrew runtime | Session environment above |
| 2026-07-28T03:25:33Z | <code>./android/gradlew --version</code> | 0 | Gradle 8.2, JVM 17.0.19, macOS aarch64 | Session environment above |
| 2026-07-28T03:25:33Z | <code>emcc --version</code> | 0 | Emscripten 6.0.4-git; first invocation completed sanity checks | Session environment above |
| 2026-07-28T03:25:33Z | <code>cppcheck --version</code> | 0 | Cppcheck 2.21.0 | Session environment above |
| 2026-07-28T03:25:33Z | <code>"$(brew --prefix llvm)/bin/clang-tidy" --version</code> | 0 | Homebrew LLVM 22.1.8, optimized build | Session environment above |
| 2026-07-28T03:25:33Z | <code>test ! -e build-audit-release &amp;&amp; test ! -e build-audit-werror &amp;&amp; test ! -e build-audit-sanitize &amp;&amp; test ! -e build-audit-compile-db</code> | 0 | All four audit build directories were absent before their first configure | Task 2 clean-build preflight |
| 2026-07-28T03:27:35Z | <code>cmake -S . -B build-audit-release -G Ninja -DCMAKE_BUILD_TYPE=Release</code> | 0 | AppleClang Release configure succeeded; bundled iniparser selected; GLib 2.88.2 and Python 3.14.6 found | `build-audit-release` |
| 2026-07-28T03:27:35Z | <code>cmake --build build-audit-release --parallel</code> | 0 | Built 49 steps including game, fb-server, and test targets; 51 server warning emissions from 27 unique project-owned locations | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| 2026-07-28T03:27:35Z | <code>ctest --test-dir build-audit-release --output-on-failure</code> | 0 | Reported 5/5 passing in 1.85 seconds; REL-002 later invalidated ownership of the fixed-port server-list subprocess, while the other four results remain accepted | [Build/tooling notebook](subsystems/07-build-release-tooling.md#candidates) |
| 2026-07-28T03:28:57Z | <code>cmake -S . -B build-audit-werror -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-Werror' -DCMAKE_CXX_FLAGS='-Werror'</code> | 0 | Warnings-strict Debug configure succeeded | `build-audit-werror` |
| 2026-07-28T03:28:57Z | <code>cmake --build build-audit-werror --parallel</code> | 1 | Expected classified baseline failure: AppleClang promoted server IMP-001 through IMP-004 warnings to errors; Ninja stopped before linking | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| 2026-07-28T03:28:57Z | <code>ctest --test-dir build-audit-werror --output-on-failure</code> | 8 | `net-bots-test` passed six assertions; server-list reported `OK (skipped=1)` because its binary was missing; 3 C++ tests were not run | `build-audit-werror/Testing/Temporary/LastTest.log` |
| 2026-07-28T03:30:17Z | <code>cmake -S . -B build-audit-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' -DCMAKE_CXX_FLAGS='-O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'</code> | 0 | ASan+UBSan Debug configure succeeded | `build-audit-sanitize` |
| 2026-07-28T03:30:17Z | <code>cmake --build build-audit-sanitize --parallel</code> | 0 | Built all 49 steps; repeated server IMP-001 through IMP-004 warnings plus two vendored iniparser `sprintf` deprecation warnings | [Build/tooling notebook](subsystems/07-build-release-tooling.md) |
| 2026-07-28T03:30:17Z | <code>ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-audit-sanitize --output-on-failure</code> | 8 | Apple ASan reported leak detection unsupported and aborted 3 instrumented C++ tests; CTest marked both Python rows Passed, but only net-bots is accepted while server-list retains REL-002 limits | [Build/tooling notebook](subsystems/07-build-release-tooling.md) |
| 2026-07-28T03:30:17Z | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-audit-sanitize --output-on-failure</code> | 0 | Reported 5/5 passing with no sanitizer diagnostic; REL-002 later invalidated server-list process ownership, which the supplemental foreground run restored | [Build/tooling notebook](subsystems/07-build-release-tooling.md#candidates) |
| 2026-07-28 (Task 2 Step 5; exact time not captured) | <code>cmake -S . -B build-audit-compile-db -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON</code> | 0 | Compile-database configure succeeded and generated 43 project/vendored compile commands | `build-audit-compile-db/compile_commands.json` |
| 2026-07-28 (Task 2 Step 5; exact time not captured) | <code>cppcheck --project=build-audit-compile-db/compile_commands.json --enable=warning,style,performance,portability --inline-suppr --error-exitcode=0 2&gt; /tmp/fb-sdl3-audit/cppcheck.txt</code> | 0 | 507 deduplicated diagnostics: 496 project-owned across 34 check IDs and 11 vendored | [Analyzer triage](subsystems/07-build-release-tooling.md#analyzer-triage) |
| 2026-07-28 (Task 2 Step 5; exact time not captured) | <code>"$(brew --prefix llvm)/bin/run-clang-tidy" -p build-audit-compile-db &gt; /tmp/fb-sdl3-audit/clang-tidy.txt 2&gt;&amp;1</code> | 1 | Exact brief command could not find keg-only clang-tidy on PATH; retained as `clang-tidy-initial.txt` | [Build/tooling notebook](subsystems/07-build-release-tooling.md) |
| 2026-07-28 (Task 2 Step 5; exact time not captured) | <code>"$(brew --prefix llvm)/bin/run-clang-tidy" -p build-audit-compile-db -clang-tidy-binary "$(brew --prefix llvm)/bin/clang-tidy" &gt; /tmp/fb-sdl3-audit/clang-tidy.txt 2&gt;&amp;1</code> | 1 | Explicit binary was found, but LLVM 22 enabled no checks without a repository configuration; retained as `clang-tidy-no-checks.txt` | [Build/tooling notebook](subsystems/07-build-release-tooling.md) |
| 2026-07-28 (Task 2 Step 5; exact time not captured) | <code>"$(brew --prefix llvm)/bin/run-clang-tidy" -p build-audit-compile-db -clang-tidy-binary "$(brew --prefix llvm)/bin/clang-tidy" -checks='clang-analyzer-*,bugprone-*,performance-*,portability-*' -header-filter='^(src&#124;server&#124;tests)/' &gt; /tmp/fb-sdl3-audit/clang-tidy.txt 2&gt;&amp;1</code> | 1 | Broad checks reached project code but Homebrew LLVM could not locate the macOS SDK headers; retained as `clang-tidy-no-sysroot.txt` | [Build/tooling notebook](subsystems/07-build-release-tooling.md) |
| 2026-07-28 (Task 2 Step 5; exact time not captured) | <code>xcrun --sdk macosx --show-sdk-path</code> | 0 | Resolved Xcode macOS 26.5 SDK sysroot | `/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk` |
| 2026-07-28 (Task 2 Step 5; exact time not captured) | <code>"$(brew --prefix llvm)/bin/run-clang-tidy" -p build-audit-compile-db -clang-tidy-binary "$(brew --prefix llvm)/bin/clang-tidy" -checks='clang-analyzer-*,bugprone-*,performance-*,portability-*' -header-filter='^/Users/dchau/gr/frozen-bubble-sdl3/(src&#124;server&#124;tests)/' -extra-arg=-isysroot -extra-arg="$(xcrun --sdk macosx --show-sdk-path)" &gt; /tmp/fb-sdl3-audit/clang-tidy.txt 2&gt;&amp;1</code> | 0 | 598 deduplicated diagnostics: 547 project-owned across 38 check IDs and 51 vendored | [Analyzer triage](subsystems/07-build-release-tooling.md#analyzer-triage) |
| 2026-07-28T03:33:11Z | <code>rg -N '^(src&#124;server&#124;tests&#124;third_party)/[^:]+:[0-9]+:[0-9]+: (error&#124;warning&#124;style&#124;performance&#124;portability): .*\[[^][]+\]$' /tmp/fb-sdl3-audit/cppcheck.txt &#124; sort -u &gt; /tmp/fb-sdl3-audit/cppcheck-diagnostics-unique.txt</code> | 0 | `rg` printed `regex parse error` / `error: unclosed character class`; downstream `sort` made the pipeline exit 0 and left an empty/truncated artifact. Non-evidence. | Corrected immediately below |
| 2026-07-28T03:33:11Z | <code>rg -N '^(src&#124;server&#124;tests)/[^:]+:[0-9]+:[0-9]+: (error&#124;warning&#124;style&#124;performance&#124;portability): .*\[[^][]+\]$' /tmp/fb-sdl3-audit/cppcheck.txt &#124; sort -u &gt; /tmp/fb-sdl3-audit/cppcheck-project-unique.txt</code> | 0 | `rg` printed `regex parse error` / `error: unclosed character class`; downstream `sort` made the pipeline exit 0 and left an empty/truncated artifact. Non-evidence. | Corrected immediately below |
| 2026-07-28T03:33:11Z | <code>rg -N '^/Users/dchau/gr/frozen-bubble-sdl3/(src&#124;server&#124;tests&#124;third_party)/[^:]+:[0-9]+:[0-9]+: (warning&#124;error): .*\[[^][]+\]$' /tmp/fb-sdl3-audit/clang-tidy.txt &#124; sed 's#^/Users/dchau/gr/frozen-bubble-sdl3/##' &#124; sort -u &gt; /tmp/fb-sdl3-audit/clang-tidy-diagnostics-unique.txt</code> | 0 | `rg` printed `regex parse error` / `error: unclosed character class`; downstream `sed`/`sort` made the pipeline exit 0 and left an empty/truncated artifact. Non-evidence. | Corrected immediately below |
| 2026-07-28T03:33:11Z | <code>rg -N '^/Users/dchau/gr/frozen-bubble-sdl3/(src&#124;server&#124;tests)/[^:]+:[0-9]+:[0-9]+: (warning&#124;error): .*\[[^][]+\]$' /tmp/fb-sdl3-audit/clang-tidy.txt &#124; sed 's#^/Users/dchau/gr/frozen-bubble-sdl3/##' &#124; sort -u &gt; /tmp/fb-sdl3-audit/clang-tidy-project-unique.txt</code> | 0 | `rg` printed `regex parse error` / `error: unclosed character class`; downstream `sed`/`sort` made the pipeline exit 0 and left an empty/truncated artifact. Non-evidence. | Corrected immediately below |
| 2026-07-28T03:33:25Z | <code>rg -N '^(src&#124;server&#124;tests&#124;third_party)/[^:]+:[0-9]+:[0-9]+: (error&#124;warning&#124;style&#124;performance&#124;portability): .*\[[-A-Za-z0-9_.]+\]$' /tmp/fb-sdl3-audit/cppcheck.txt &#124; sort -u &gt; /tmp/fb-sdl3-audit/cppcheck-diagnostics-unique.txt</code> | 0 | Corrected character class; wrote the deduplicated cppcheck inventory with no stderr/stdout | `/tmp/fb-sdl3-audit/cppcheck-diagnostics-unique.txt` |
| 2026-07-28T03:33:25Z | <code>rg -N '^(src&#124;server&#124;tests)/[^:]+:[0-9]+:[0-9]+: (error&#124;warning&#124;style&#124;performance&#124;portability): .*\[[-A-Za-z0-9_.]+\]$' /tmp/fb-sdl3-audit/cppcheck.txt &#124; sort -u &gt; /tmp/fb-sdl3-audit/cppcheck-project-unique.txt</code> | 0 | Corrected character class; wrote the project-only cppcheck inventory with no stderr/stdout | `/tmp/fb-sdl3-audit/cppcheck-project-unique.txt` |
| 2026-07-28T03:33:25Z | <code>rg -N '^/Users/dchau/gr/frozen-bubble-sdl3/(src&#124;server&#124;tests&#124;third_party)/[^:]+:[0-9]+:[0-9]+: (warning&#124;error): .*\[[-A-Za-z0-9_.]+\]$' /tmp/fb-sdl3-audit/clang-tidy.txt &#124; sed 's#^/Users/dchau/gr/frozen-bubble-sdl3/##' &#124; sort -u &gt; /tmp/fb-sdl3-audit/clang-tidy-diagnostics-unique.txt</code> | 0 | Corrected character class; wrote the deduplicated clang-tidy inventory with no stderr/stdout | `/tmp/fb-sdl3-audit/clang-tidy-diagnostics-unique.txt` |
| 2026-07-28T03:33:25Z | <code>rg -N '^/Users/dchau/gr/frozen-bubble-sdl3/(src&#124;server&#124;tests)/[^:]+:[0-9]+:[0-9]+: (warning&#124;error): .*\[[-A-Za-z0-9_.]+\]$' /tmp/fb-sdl3-audit/clang-tidy.txt &#124; sed 's#^/Users/dchau/gr/frozen-bubble-sdl3/##' &#124; sort -u &gt; /tmp/fb-sdl3-audit/clang-tidy-project-unique.txt</code> | 0 | Corrected character class; wrote the project-only clang-tidy inventory with no stderr/stdout | `/tmp/fb-sdl3-audit/clang-tidy-project-unique.txt` |
| 2026-07-28T03:33:36Z | <code>wc -l /tmp/fb-sdl3-audit/cppcheck-diagnostics-unique.txt /tmp/fb-sdl3-audit/cppcheck-project-unique.txt /tmp/fb-sdl3-audit/clang-tidy-diagnostics-unique.txt /tmp/fb-sdl3-audit/clang-tidy-project-unique.txt</code> | 0 | 507 cppcheck total, 496 cppcheck project, 598 clang-tidy total, 547 clang-tidy project records | [Analyzer triage](subsystems/07-build-release-tooling.md#analyzer-triage) |
| 2026-07-28 (Task 2 Step 5; exact time not captured) | <code>shasum -a 256 /tmp/fb-sdl3-audit/cppcheck.txt /tmp/fb-sdl3-audit/clang-tidy-initial.txt /tmp/fb-sdl3-audit/clang-tidy-no-checks.txt /tmp/fb-sdl3-audit/clang-tidy-no-sysroot.txt /tmp/fb-sdl3-audit/clang-tidy.txt</code> | 0 | Recorded immutable hashes for all five primary analyzer logs | [Build/tooling notebook](subsystems/07-build-release-tooling.md) |
| 2026-07-28T03:44:45Z | <code>cmake --build build-audit-release --parallel</code> | 0 | Final verification replay: `ninja: no work to do` | `build-audit-release` |
| 2026-07-28T03:44:45Z | <code>cmake --build build-audit-sanitize --parallel</code> | 0 | Final verification replay: `ninja: no work to do` | `build-audit-sanitize` |
| 2026-07-28T03:44:45Z | <code>cmake --build build-audit-werror --parallel</code> | 1 | Strict replay again failed only on the classified IMP-001 through IMP-004 warning classes | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| 2026-07-28T03:44:45Z | <code>cppcheck --project=build-audit-compile-db/compile_commands.json --enable=warning,style,performance,portability --inline-suppr --error-exitcode=0 2&gt; /tmp/fb-sdl3-audit/cppcheck-verify.txt</code> | 0 | Replay checked all 43 compile commands; later comparison proved the 2,160-line / 147,118-byte log byte-identical | `/tmp/fb-sdl3-audit/cppcheck-verify.txt` |
| 2026-07-28T03:44:45Z | <code>"/opt/homebrew/opt/llvm/bin/run-clang-tidy" -p build-audit-compile-db -clang-tidy-binary "/opt/homebrew/opt/llvm/bin/clang-tidy" -checks='clang-analyzer-*,bugprone-*,performance-*,portability-*' -header-filter='^(src&#124;server&#124;tests)/' -extra-arg=-isysroot -extra-arg="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk" &gt; /tmp/fb-sdl3-audit/clang-tidy-verify.txt 2&gt;&amp;1</code> | 0 | Relative header filter yielded a 6,885-line partial replay that differed from the primary log; retained as noncanonical, non-evidence | `/tmp/fb-sdl3-audit/clang-tidy-verify.txt` |
| 2026-07-28T03:45:21Z | <code>wc -l -c /tmp/fb-sdl3-audit/cppcheck-verify.txt /tmp/fb-sdl3-audit/clang-tidy-verify.txt</code> | 0 | cppcheck 2,160 lines / 147,118 bytes; partial clang-tidy 6,885 lines / 564,438 bytes | Replay artifact inspection |
| 2026-07-28T03:45:21Z | <code>shasum -a 256 /tmp/fb-sdl3-audit/cppcheck-verify.txt /tmp/fb-sdl3-audit/clang-tidy-verify.txt</code> | 0 | cppcheck `0eb04211...e6eae`; partial clang-tidy `99374465...1fe9` | Replay artifact inspection |
| 2026-07-28T03:45:21Z | <code>cmp -s /tmp/fb-sdl3-audit/cppcheck.txt /tmp/fb-sdl3-audit/cppcheck-verify.txt</code> | 0 | Cppcheck replay log matched the primary log byte-for-byte | Accepted cppcheck replay evidence |
| 2026-07-28T03:45:21Z | <code>printf 'cppcheck_cmp=%s\n' "$?"</code> | 0 | Printed `cppcheck_cmp=0` | Replay artifact inspection |
| 2026-07-28T03:45:21Z | <code>cmp -s /tmp/fb-sdl3-audit/clang-tidy.txt /tmp/fb-sdl3-audit/clang-tidy-verify.txt</code> | 1 | Partial relative-filter replay differed from the primary clang-tidy log; non-evidence | Superseded by deterministic replay |
| 2026-07-28T03:45:21Z | <code>printf 'clang_tidy_cmp=%s\n' "$?"</code> | 0 | Printed `clang_tidy_cmp=1` | Replay artifact inspection |
| 2026-07-28 (Task 2 final verification; exact time not captured) | <code>ctest --test-dir build-audit-release --output-on-failure</code> | 0 | Concurrent verification reported 5/5 passing; it shared REL-002's foreign fixed-port listener | [REL-002](subsystems/07-build-release-tooling.md#candidates) |
| 2026-07-28 (Task 2 final verification; exact time not captured) | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-audit-sanitize --output-on-failure</code> | 8 | Concurrent verification exposed REL-002: server-list received `CREATE: NICK_IN_USE`; the other four tests passed | [REL-002](subsystems/07-build-release-tooling.md#candidates) |
| 2026-07-28 (Task 2 REL-002 investigation; exact time not captured) | <code>lsof -nP -iTCP:15512 -sTCP:LISTEN</code> | 0 | Identified `fb-server` PID 22300 listening on fixed port 15512 | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| 2026-07-28 (Task 2 REL-002 investigation; exact time not captured) | <code>ps -o pid=,ppid=,lstart=,command= -p 22293,22300,74458,76361,92759</code> | 0 | PID 22300 had PPID 1, started 2026-07-26, and ran `/Users/dchau/gr/frozen-bubble-battle/build/server/fb-server -p 15512 -q -z` | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| 2026-07-28 (Task 2 REL-002 investigation; exact time not captured) | <code>build-audit-release/server/fb-server -p 15512 -q -z</code> | 1 | Requested audit server reported `Address already in use`; test setup nevertheless accepted the foreign listener | [REL-002](subsystems/07-build-release-tooling.md#candidates) |
| 2026-07-28T03:47:34Z | <code>python3 -c 'import pathlib,sys; path=pathlib.Path("tests/server_list_cap_test.py"); source=path.read_text().replace("self.port = 15512", "self.port = 25512"); sys.argv=[str(path), "build-audit-release/server/fb-server"]; scope={"__name__":"__main__","__file__":str(path)}; exec(compile(source, str(path), "exec"), scope)'</code> | 5 | Harness mistake: unittest discovered zero tests and printed `Ran 0 tests in 0.000s` / `NO TESTS RAN`. Non-evidence. | Corrected by executing in real `__main__` globals |
| 2026-07-28T03:47:34Z | <code>! nc -z 127.0.0.1 25512</code> | 0 | Port 25512 was closed after the zero-test Release helper attempt | Cleanup evidence only |
| 2026-07-28T03:47:34Z | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 python3 -c 'import pathlib,sys; path=pathlib.Path("tests/server_list_cap_test.py"); source=path.read_text().replace("self.port = 15512", "self.port = 25512"); sys.argv=[str(path), "build-audit-sanitize/server/fb-server"]; scope={"__name__":"__main__","__file__":str(path)}; exec(compile(source, str(path), "exec"), scope)'</code> | 5 | Harness mistake: unittest discovered zero tests and printed `Ran 0 tests in 0.000s` / `NO TESTS RAN`. Non-evidence. | Corrected by executing in real `__main__` globals |
| 2026-07-28T03:47:34Z | <code>! nc -z 127.0.0.1 25512</code> | 0 | Port 25512 was closed after the zero-test sanitizer helper attempt | Cleanup evidence only |
| 2026-07-28 (Task 2 REL-002 reproduction; exact time not captured) | <code>python3 -c 'import pathlib,sys; path=pathlib.Path("tests/server_list_cap_test.py"); source=path.read_text().replace("self.port = 15512", "self.port = 25512"); sys.argv=[str(path), "build-audit-release/server/fb-server"]; globals()["__file__"]=str(path); exec(compile(source, str(path), "exec"), globals())'</code> | 0 | Assertions passed, but omission of `-d` reproduced the daemon leak on temporary port 25512 | [REL-002](subsystems/07-build-release-tooling.md#candidates) |
| 2026-07-28 (Task 2 REL-002 reproduction; exact time not captured) | <code>lsof -nP -iTCP:25512 -sTCP:LISTEN</code> | 0 | Identified leaked listener PID 95766 on temporary port 25512 | Task 2 cleanup evidence |
| 2026-07-28 (Task 2 REL-002 reproduction; exact time not captured) | <code>ps -o pid=,ppid=,lstart=,command= -p 95766</code> | 0 | PID 95766 had PPID 1 and exact command `build-audit-release/server/fb-server -p 25512 -q -z` | Task 2 cleanup evidence |
| 2026-07-28 (Task 2 REL-002 investigation; exact time not captured) | <code>kill -TERM 95766; for task_try in 1 2 3 4 5; do if ! kill -0 95766 2&gt;/dev/null; then exit 0; fi; sleep 1; done; exit 1</code> | 0 | Terminated only the Task 2-owned reproduction daemon on temporary port 25512 | Task 2 cleanup evidence |
| 2026-07-28 (Task 2 REL-002 preliminary verification; exact time not captured) | <code>python3 -c 'import pathlib,sys; path=pathlib.Path("tests/server_list_cap_test.py"); source=path.read_text(); assert source.count("self.port = 15512") == 1; assert source.count("[str(self.server_path), \"-p\"") == 1; source=source.replace("self.port = 15512", "self.port = 25512").replace("[str(self.server_path), \"-p\"", "[str(self.server_path), \"-d\", \"-p\""); sys.argv=[str(path), "build-audit-release/server/fb-server"]; globals()["__file__"]=str(path); exec(compile(source, str(path), "exec"), globals())'</code> | 0 | Assertions passed, but independent review rejected binary-ownership confidence because port 25512 remained hardcoded | [Server baseline notebook](subsystems/01-server-protocol.md) |
| 2026-07-28 (Task 2 REL-002 preliminary verification; exact time not captured) | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 python3 -c 'import pathlib,sys; path=pathlib.Path("tests/server_list_cap_test.py"); source=path.read_text(); assert source.count("self.port = 15512") == 1; assert source.count("[str(self.server_path), \"-p\"") == 1; source=source.replace("self.port = 15512", "self.port = 25512").replace("[str(self.server_path), \"-p\"", "[str(self.server_path), \"-d\", \"-p\""); sys.argv=[str(path), "build-audit-sanitize/server/fb-server"]; globals()["__file__"]=str(path); exec(compile(source, str(path), "exec"), globals())'</code> | 0 | Assertions passed, but independent review rejected binary-ownership confidence because port 25512 remained hardcoded | [Server baseline notebook](subsystems/01-server-protocol.md) |
| 2026-07-28 (Task 2 REL-002 preliminary verification; exact time not captured) | <code>! nc -z 127.0.0.1 25512</code> | 0 | Proved only final port closure; not accepted as child-ownership evidence | Task 2 cleanup evidence |
| 2026-07-28 (Task 2 final verification; exact time not captured) | <code>ctest --test-dir build-audit-release --output-on-failure -E server-list-cap-test</code> | 0 | Four unaffected Release tests passed | `build-audit-release` |
| 2026-07-28 (Task 2 final verification; exact time not captured) | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-audit-sanitize --output-on-failure -E server-list-cap-test</code> | 0 | Four unaffected ASan+UBSan tests passed with no sanitizer diagnostic | `build-audit-sanitize` |
| 2026-07-28T03:49:49Z | <code>/opt/homebrew/opt/llvm/bin/run-clang-tidy -j 1 -p build-audit-compile-db -clang-tidy-binary /opt/homebrew/opt/llvm/bin/clang-tidy -checks='clang-analyzer-*,bugprone-*,performance-*,portability-*' -header-filter='^/Users/dchau/gr/frozen-bubble-sdl3/(src&#124;server&#124;tests&#124;third_party)/' -extra-arg=-isysroot -extra-arg=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk &gt; /tmp/fb-sdl3-audit/clang-tidy-repro.txt 2&gt;&amp;1</code> | 0 | Single-worker replay included vendored headers: 601 unique diagnostics, 547 project-owned. Superseded because its vendored boundary exceeded the primary filter. | `/tmp/fb-sdl3-audit/clang-tidy-repro.txt` |
| 2026-07-28T03:50:38Z | <code>wc -l -c /tmp/fb-sdl3-audit/clang-tidy-repro.txt</code> | 0 | 11,850 lines / 1,070,439 bytes for the expanded-header replay | Superseded replay inspection |
| 2026-07-28T03:50:38Z | <code>shasum -a 256 /tmp/fb-sdl3-audit/clang-tidy-repro.txt</code> | 0 | Expanded-header replay SHA-256 `75037d1605f2c3b73c7cd75455a40b9be0cb7cc8eb2e4444c8243b272cffa36d` | Superseded replay inspection |
| 2026-07-28T03:50:38Z | <code>rg -o '^/Users/dchau/gr/frozen-bubble-sdl3/[^:]+:[0-9]+:[0-9]+: (warning&#124;error): .* \[[-A-Za-z0-9_.]+\]$' /tmp/fb-sdl3-audit/clang-tidy-repro.txt &#124; sed 's#^/Users/dchau/gr/frozen-bubble-sdl3/##' &#124; sort -u &gt; /tmp/fb-sdl3-audit/clang-tidy-repro-unique.txt</code> | 0 | Wrote deduplicated expanded-header replay records; no stdout/stderr | Superseded replay inspection |
| 2026-07-28T03:50:38Z | <code>wc -l /tmp/fb-sdl3-audit/clang-tidy-repro-unique.txt</code> | 0 | 601 unique records | Superseded replay inspection |
| 2026-07-28T03:50:38Z | <code>awk -F: '$1 ~ /^(src&#124;server&#124;tests)\// {n++} END {print "project=" n+0}' /tmp/fb-sdl3-audit/clang-tidy-repro-unique.txt</code> | 0 | Printed `project=547` | Superseded replay inspection |
| 2026-07-28T03:50:38Z | <code>awk -F'[][]' '{print $2}' /tmp/fb-sdl3-audit/clang-tidy-repro-unique.txt &#124; sort &#124; uniq -c &#124; wc -l</code> | 0 | 42 total project/vendored check IDs in the expanded-header replay | Superseded replay inspection |
| 2026-07-28T03:50:59Z | <code>/opt/homebrew/opt/llvm/bin/run-clang-tidy -j 1 -p build-audit-compile-db -clang-tidy-binary /opt/homebrew/opt/llvm/bin/clang-tidy -checks='clang-analyzer-*,bugprone-*,performance-*,portability-*' -header-filter='^/Users/dchau/gr/frozen-bubble-sdl3/(src&#124;server&#124;tests)/' -extra-arg=-isysroot -extra-arg=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX26.5.sdk &gt; /tmp/fb-sdl3-audit/clang-tidy-repro.txt 2&gt;&amp;1</code> | 0 | Canonical single-worker replay completed with the project-only header filter | `/tmp/fb-sdl3-audit/clang-tidy-repro.txt` |
| 2026-07-28T03:51:58Z | <code>wc -l -c /tmp/fb-sdl3-audit/clang-tidy-repro.txt</code> | 0 | 11,670 lines / 1,057,185 bytes | Accepted replay inspection |
| 2026-07-28T03:51:58Z | <code>shasum -a 256 /tmp/fb-sdl3-audit/clang-tidy-repro.txt</code> | 0 | Accepted replay SHA-256 `faf6915e1bb6cfe54176d92550de24f05fae926f8d201603ff7ef64dade29beb` | Accepted replay inspection |
| 2026-07-28T03:51:58Z | <code>rg -o '^/Users/dchau/gr/frozen-bubble-sdl3/[^:]+:[0-9]+:[0-9]+: (warning&#124;error): .* \[[-A-Za-z0-9_.]+\]$' /tmp/fb-sdl3-audit/clang-tidy-repro.txt &#124; sed 's#^/Users/dchau/gr/frozen-bubble-sdl3/##' &#124; sort -u &gt; /tmp/fb-sdl3-audit/clang-tidy-repro-unique.txt</code> | 0 | Wrote deduplicated accepted replay records; no stdout/stderr | Accepted replay inspection |
| 2026-07-28T03:51:58Z | <code>wc -l /tmp/fb-sdl3-audit/clang-tidy-repro-unique.txt</code> | 0 | 598 unique records | Accepted replay inspection |
| 2026-07-28T03:51:58Z | <code>awk -F: '$1 ~ /^(src&#124;server&#124;tests)\// {n++} END {print "project=" n+0}' /tmp/fb-sdl3-audit/clang-tidy-repro-unique.txt</code> | 0 | Printed `project=547` | Accepted replay inspection |
| 2026-07-28T03:51:58Z | <code>cmp -s /tmp/fb-sdl3-audit/clang-tidy-project-unique.txt &lt;(awk -F: '$1 ~ /^(src&#124;server&#124;tests)\//' /tmp/fb-sdl3-audit/clang-tidy-repro-unique.txt)</code> | 0 | All 547 project-owned records matched the primary inventory exactly | Accepted deterministic replay evidence |
| 2026-07-28T03:51:58Z | <code>printf 'project_cmp=%s\n' "$?"</code> | 0 | Printed `project_cmp=0` | Accepted replay inspection |
| 2026-07-28 (Task 2 review fix verification; exact time not captured) | <code>python3 -c 'import pathlib,sys; path=pathlib.Path("tests/server_list_cap_test.py"); source=path.read_text(); old_port="        self.port = 15512"; new_port="        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as port_probe:\n            port_probe.bind((\"127.0.0.1\", 0))\n            self.port = port_probe.getsockname()[1]\n        print(f\"OWNED_TEST_PORT={self.port}\")"; old_cmd="[str(self.server_path), \"-p\""; new_cmd="[str(self.server_path), \"-d\", \"-p\""; old_ready="        self.socks = []"; new_ready="        time.sleep(0.1)\n        if self.server.poll() is not None:\n            self.fail(f\"server launcher exited before ownership check: {self.server.returncode}\")\n        self.socks = []"; assert source.count(old_port) == source.count(old_cmd) == source.count(old_ready) == 1; source=source.replace(old_port,new_port).replace(old_cmd,new_cmd).replace(old_ready,new_ready); sys.argv=[str(path), "build-audit-release/server/fb-server"]; globals()["__file__"]=str(path); exec(compile(source, str(path), "exec"), globals())'</code> | 0 | Dynamically allocated port 63305; exact foreground Release child remained alive after readiness; assertions passed in 0.607 seconds | [Server baseline notebook](subsystems/01-server-protocol.md) |
| 2026-07-28 (Task 2 review fix verification; exact time not captured) | <code>! pgrep -f 'build-audit-release/server/fb-server.*-d'</code> | 0 | No matching Release foreground child remained after teardown | Task 2 cleanup evidence |
| 2026-07-28 (Task 2 review fix verification; exact time not captured) | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 python3 -c 'import pathlib,sys; path=pathlib.Path("tests/server_list_cap_test.py"); source=path.read_text(); old_port="        self.port = 15512"; new_port="        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as port_probe:\n            port_probe.bind((\"127.0.0.1\", 0))\n            self.port = port_probe.getsockname()[1]\n        print(f\"OWNED_TEST_PORT={self.port}\")"; old_cmd="[str(self.server_path), \"-p\""; new_cmd="[str(self.server_path), \"-d\", \"-p\""; old_ready="        self.socks = []"; new_ready="        time.sleep(0.1)\n        if self.server.poll() is not None:\n            self.fail(f\"server launcher exited before ownership check: {self.server.returncode}\")\n        self.socks = []"; assert source.count(old_port) == source.count(old_cmd) == source.count(old_ready) == 1; source=source.replace(old_port,new_port).replace(old_cmd,new_cmd).replace(old_ready,new_ready); sys.argv=[str(path), "build-audit-sanitize/server/fb-server"]; globals()["__file__"]=str(path); exec(compile(source, str(path), "exec"), globals())'</code> | 0 | Dynamically allocated port 63316; exact foreground ASan+UBSan child remained alive after readiness; assertions passed in 1.000 second | [Server baseline notebook](subsystems/01-server-protocol.md) |
| 2026-07-28 (Task 2 review fix verification; exact time not captured) | <code>! pgrep -f 'build-audit-sanitize/server/fb-server.*-d'</code> | 0 | No matching sanitizer foreground child remained after teardown | Task 2 cleanup evidence |
| 2026-07-28T04:05:55Z | <code>cmake --build build-audit-release --parallel</code> | 0 | Post-review final replay: `ninja: no work to do` | `build-audit-release` |
| 2026-07-28T04:05:55Z | <code>cmake --build build-audit-sanitize --parallel</code> | 0 | Post-review final replay: `ninja: no work to do` | `build-audit-sanitize` |
| 2026-07-28T04:06:02Z | <code>ctest --test-dir build-audit-release --output-on-failure -E server-list-cap-test</code> | 0 | Post-review final replay: 4/4 unaffected tests passed in 0.13 seconds | `build-audit-release` |
| 2026-07-28T04:06:02Z | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-audit-sanitize --output-on-failure -E server-list-cap-test</code> | 0 | Post-review final replay: 4/4 unaffected tests passed in 0.37 seconds with no sanitizer diagnostic | `build-audit-sanitize` |
| 2026-07-28 (Task 3A; exact time not captured) | <code>wc -l server/fb-server.c server/game.c server/game.h server/net.c server/net.h server/ws.c server/ws.h server/tools.c server/tools.h server/stats.c server/stats.h server/log.c server/log.h server/win32_compat.h server/CMakeLists.txt server/README tests/server_list_cap_test.py tools/server_tests/test_room_caps.py</code> | 0 | Counted 4,337 lines across every named implementation/header/boundary/test input before review | [Server coverage](subsystems/01-server-protocol.md#coverage) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>find server/init -type f -maxdepth 2 -print</code> | 0 | Enumerated all three init boundary files | [Server coverage](subsystems/01-server-protocol.md#coverage) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>nl -ba server/tools.c</code> | 0 | Reviewed numeric/allocation/list/daemon/privilege paths with stable line references | [Task 2 dispositions](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>nl -ba server/ws.c</code> | 0 | Reviewed handshake, per-fd state, send framing, and in-place decode bounds | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>nl -ba server/stats.c</code> | 0 | Reviewed hash/file ownership, reset, save, and win/loss lifecycle | [Ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>nl -ba server/log.c</code> | 0 | Reviewed formatting allocation, syslog/stderr, and signal-reachable behavior | [Ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>nl -ba server/net.c &#124; sed -n '565,725p'</code> | 0 | Confirmed admission guard, fd initialization, buffer allocation, and normal/priority list transitions | [Lifecycle map](subsystems/01-server-protocol.md#acceptance-input-retention-dispatch-and-teardown) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>nl -ba server/game.c &#124; sed -n '621,845p'</code> | 0 | Confirmed protocol parsing, nickname/room validation, and command authority dispatch | [Authorization review](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>rg -n 'amount_transmitted&#124;players_started&#124;players_id&#124;players_conn&#124;players_nick&#124;games_open&#124;open_players&#124;nick_available&#124;current_command' server/game.c server/net.c</code> | 0 | Proved no transmission-counter increment and enumerated every room/fd identity state access | [BUG-004 and lifecycle evidence](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>git log -S'amount_transmitted +=' --oneline --all -- server/game.c server/net.c</code> | 0 | Located historical counter-removal commit `2d1a4b4d`; current tree has no increment | [BUG-004](subsystems/01-server-protocol.md#confirmed-findings) |
| 2026-07-28 (Task 3A; exact time not captured) | <code>git diff --stat 09d6c7bfcd864a0ad3951b87d16a88dc770392a3..345e58b5c8e92bd39aa6b38e8b31d49fb6f1081c -- server tests/server_list_cap_test.py tools/server_tests/test_room_caps.py</code> | 0 | No output; audited server/test inputs exactly match the production baseline | Audit baseline above |
| 2026-07-28 (Task 3A; exact time not captured) | <code>git status --short --branch</code> | 0 | Began on the requested branch with no tracked or untracked status entries beyond the branch header | Task 3A preflight |
| 2026-07-28 (Task 3 closure; exact time not captured) | <code>sed -n '1,280p' .superpowers/sdd/2026-07-28-complete-repository-audit/task-3c-synthesis-brief.md</code> | 0 | Read the controller brief requiring static-only closure, no security runtime work, final dispositions, Task 4 handoff, and the exact commit subject | Task 3 controller brief |
| 2026-07-28 (Task 3 closure; exact time not captured) | <code>sed -n '1,9999p' .superpowers/sdd/2026-07-28-complete-repository-audit/task-3a-static-report.md</code> | 0 | Read the complete ignored static-phase report and its finding/dependency inventory | Static phase evidence |
| 2026-07-28 (Task 3 closure; exact time not captured) | <code>nl -ba server/game.c &#124; sed -n '324,500p'</code> | 0 | Rechecked `START`, `CLOSE`, readiness, priority promotion, and kick paths supporting BUG-003 and BUG-011 | [Authorization review](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| 2026-07-28 (Task 3 closure; exact time not captured) | <code>nl -ba server/net.c &#124; sed -n '480,670p'</code> | 0 | Rechecked single-thread list replacement, select/accept admission, and fd-indexed initialization supporting BUG-011 and REL-003 | [Lifecycle map](subsystems/01-server-protocol.md#acceptance-input-retention-dispatch-and-teardown) |
| 2026-07-28 (Task 3 closure; exact time not captured) | <code>nl -ba server/ws.c &#124; sed -n '135,255p'</code> | 0 | Rechecked blocking single-send WebSocket output, frame construction, and upgrade handling supporting BUG-006/007 and IMP-011 | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| 2026-07-28 (Task 3 closure; exact time not captured) | <code>sed -n '1,180p' server/win32_compat.h</code> | 0 | Rechecked the Winsock compatibility boundary used by the confirmed REL-003 source/build mismatch | [Platform boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| 2026-07-28 (Task 4; exact time not captured) | <code>git status --short --branch</code> | 0 | Began on `codex/sdl3-complete-audit` at `52424189f9a2f4ee68e9552e8f8f27df4f3a2bc7` with no changed path | Task 4 preflight |
| 2026-07-28 (Task 4; exact time not captured) | <code>nl -ba src/networkclient.cpp</code> | 0 | Reviewed native/shared connection, response, parser, identity, sync, discovery, and latency paths | [Task 4 static review](subsystems/02-network-client-sync.md#static-review) |
| 2026-07-28 (Task 4; exact time not captured) | <code>nl -ba src/networkclient_wasm.cpp</code> | 0 | Reviewed WebSocket callback, framing, async state, lifecycle, and browser stub parity | [Task 4 static review](subsystems/02-network-client-sync.md#static-review) |
| 2026-07-28 (Task 4; exact time not captured) | <code>nl -ba src/networkclient.h</code> | 0 | Reviewed all 244 lines of public/private state, defaults, queue APIs, IDs, options, and raw ownership | [Task 4 trust boundaries](subsystems/02-network-client-sync.md#trust-boundaries-and-invariants) |
| 2026-07-28 (Task 4; exact time not captured) | <code>nl -ba src/socket_compat.h</code> | 0 | Reviewed all 66 lines of Winsock/POSIX type, flag, startup, error, and close compatibility | [Task 4 Windows boundary](subsystems/02-network-client-sync.md#lobby-response-and-reachability-handling) |
| 2026-07-28 (Task 4; exact time not captured) | <code>nl -ba src/bubblegame_net.cpp</code> | 0 | Traced every game opcode, sender/array mapping, peer numeric sink, and sync routing path | [Peer-message review](subsystems/02-network-client-sync.md#peer-messages-and-round-flow) |
| 2026-07-28 (Task 4; exact time not captured) | <code>nl -ba src/bubblegame.cpp; nl -ba src/bubblegame_state.cpp; nl -ba src/bubblegame_render.cpp; nl -ba src/bubblegame_shooter.cpp; nl -ba src/bubblegame_level.cpp; nl -ba src/bubblegame.h; nl -ba src/mainmenu_netpanel.cpp</code> | 0 | Reviewed 6,369 focused consumer/declaration lines for exit, round sync, placement, level order, and lobby-thread boundaries | [Task 4 coverage](subsystems/02-network-client-sync.md#coverage) |
| 2026-07-28 (Task 4; exact time not captured) | <code>nl -ba tools/net_bots.py</code> | 0 | Reviewed all 450 lines of framing, buffering, bot state, threads, failure propagation, and cleanup | [Bot harness fidelity](subsystems/02-network-client-sync.md#bot-harness-fidelity) |
| 2026-07-28 (Task 4; exact time not captured) | <code>nl -ba tests/net_bots_test.py</code> | 0 | Reviewed all six unit cases and established their socket/sync coverage boundary | [Bot harness fidelity](subsystems/02-network-client-sync.md#bot-harness-fidelity) |
| 2026-07-28 (Task 4; exact time not captured) | <code>python3 tests/net_bots_test.py</code> | 0 | Six tests passed in 0.013 seconds | [Dynamic evidence](subsystems/02-network-client-sync.md#dynamic-evidence) |
| 2026-07-28 (Task 4; exact time not captured) | <code>ctest --test-dir build-audit-sanitize -R net-bots-test --output-on-failure</code> | 0 | Registered net-bots-test passed 1/1 in 0.30 seconds | [Dynamic evidence](subsystems/02-network-client-sync.md#dynamic-evidence) |
| 2026-07-28 (Task 4; exact time not captured) | <code>emcmake cmake -S /Users/dchau/gr/frozen-bubble-sdl3 -B /tmp/fb-sdl3-audit/task4-wasm-build -G Ninja -DCMAKE_BUILD_TYPE=Release</code> | 0 | Isolated WASM target configured successfully | [Dynamic evidence](subsystems/02-network-client-sync.md#dynamic-evidence) |
| 2026-07-28 (Task 4; exact time not captured) | <code>cmake --build /tmp/fb-sdl3-audit/task4-wasm-build --parallel</code> | 1 | Build stopped at documented unpatched SDK boundary: SDL3_image and SDL3_mixer headers absent | [Dynamic evidence](subsystems/02-network-client-sync.md#dynamic-evidence) |
| 2026-07-28 (Task 4; exact time not captured) | <code>em++ -std=c++17 -Wall -Wextra -pedantic -D__WASM_PORT__ -Isrc -sUSE_SDL=3 -c src/networkclient.cpp -o /tmp/fb-sdl3-audit/task4-networkclient-wasm.o</code> | 0 | Shared client WASM translation unit compiled with two unused-variable warnings | [Dynamic evidence](subsystems/02-network-client-sync.md#dynamic-evidence) |
| 2026-07-28 (Task 4; exact time not captured) | <code>em++ -std=c++17 -Wall -Wextra -pedantic -D__WASM_PORT__ -Isrc -sUSE_SDL=3 -c src/networkclient_wasm.cpp -o /tmp/fb-sdl3-audit/task4-networkclient-wasm-transport.o</code> | 0 | WebSocket transport WASM translation unit compiled with three nonfatal warnings | [Dynamic evidence](subsystems/02-network-client-sync.md#dynamic-evidence) |
| 2026-07-28 (Task 4; exact time not captured) | <code>lsof -nP -iTCP -sTCP:LISTEN &#124; rg 'fb-server&#124;COMMAND&#124;1511&#124;15511&#124;15512&#124;15113&#124;15998'</code> | 0 | Observed four pre-existing fb-server listeners; no process or socket was touched | [Dynamic evidence](subsystems/02-network-client-sync.md#dynamic-evidence) |
| 2026-07-28 (Task 4; exact time not captured) | <code>git diff 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 -- server docker .github CMakeLists.txt cmake src/main.cpp</code> | 0 | No output; audited production/configuration paths remained unchanged | Audit baseline above |
| 2026-07-28 (Task 4 review correction; exact time not captured) | <code>nl -ba server/net.c &#124; sed -n '345,375p'; nl -ba server/game.c &#124; sed -n '629,910p'</code> | 0 | Verified priority `FB/` diversion and explicit PART/remove-priority branch, dismissing and reserving BUG-012 | [Connection lifecycle](subsystems/02-network-client-sync.md#connection-and-room-lifecycle) |
| 2026-07-28 (Task 4 review correction; exact time not captured) | <code>nl -ba src/networkclient.cpp &#124; sed -n '924,1130p'</code> | 0 | Verified incomplete error recognition and uncorrelated WASM OK handling, broadening BUG-015 | [Response handling](subsystems/02-network-client-sync.md#lobby-response-and-reachability-handling) |
| 2026-07-28 (Task 4 review correction; exact time not captured) | <code>nl -ba bin/frozen-bubble &#124; sed -n '4590,4608p'; nl -ba lib/Games/FrozenBubble/Net.pm &#124; sed -n '350,365p'</code> | 0 | Rechecked the original client's active-game escape and disconnect/reconnect reference behavior used only as BUG-012 comparison evidence | [Connection lifecycle](subsystems/02-network-client-sync.md#connection-and-room-lifecycle) |
| 2026-07-28 (Task 5; exact time not captured) | <code>git status --short &amp;&amp; git diff --check &amp;&amp; git diff --stat &amp;&amp; git diff --name-only 4083c31c51ba4c1dbd2bb98f6ae4e37eeaba67fc</code> | 0 | Preflight found only the four approved audit documents modified and no whitespace error | Task 5 scope preflight |
| 2026-07-28 (Task 5; exact time not captured) | <code>nl -ba src/bubblegame.cpp &#124; sed -n '337,455p;900,1018p;1180,1230p;1330,1385p'; nl -ba src/bubblegame_state.cpp &#124; sed -n '430,790p'; nl -ba src/bubblegame_net.cpp &#124; sed -n '210,590p'</code> | 0 | Traced match construction/reset, clear/loss/team/victory accounting, and ordinary peer finish/departure state | [Task 5 transition review](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| 2026-07-28 (Task 5; exact time not captured) | <code>nl -ba src/bubblegame_board.cpp &#124; sed -n '1,690p'; nl -ba src/bubblegame_shooter.cpp &#124; sed -n '1,705p'; nl -ba src/bubblegame_level.cpp &#124; sed -n '1,315p'</code> | 0 | Traced adjacency, grouping, falling/chains, launch/collision/malus placement, generation, and compression paths | [Task 5 board review](subsystems/03-gameplay.md#placement-collision-grouping-and-compression) |
| 2026-07-28 (Task 5; exact time not captured) | <code>nl -ba src/bubblegame.h; nl -ba src/bubblegame_internal.h; nl -ba src/bubblegame_input.cpp; nl -ba src/bubblegame_render.cpp; nl -ba src/netview.cpp; nl -ba src/netview.h; nl -ba src/netteams.cpp; nl -ba src/netteams.h; nl -ba src/roundstats_color.cpp; nl -ba src/roundstats_color.h; nl -ba tests/netview_test.cpp; nl -ba tests/netteams_test.cpp; nl -ba tests/roundstats_color_test.cpp</code> | 0 | Completed declaration, inline-physics, input, render, pure-helper, and matching-test inspection for every remaining scoped file | [Task 5 coverage](subsystems/03-gameplay.md#coverage) |
| 2026-07-28 (Task 5; exact time not captured) | <code>nl -ba bin/frozen-bubble &#124; sed -n '590,620p;720,910p;940,970p;1900,1990p;2300,2410p'; nl -ba lib/Games/FrozenBubble/Net.pm &#124; sed -n '1,430p'</code> | 0 | Compared living-player, placement, chain-validation, malus, simultaneous-finish, and protocol semantics with the Perl reference | [Task 5 static review](subsystems/03-gameplay.md#static-review) |
| 2026-07-28 (Task 5; exact time not captured) | <code>rg -n 'bubblegame&#124;netview&#124;netteams&#124;roundstats' /tmp/fb-sdl3-audit/cppcheck-project-unique.txt /tmp/fb-sdl3-audit/cppcheck-uninitialized.txt /tmp/fb-sdl3-audit/clang-tidy-project-unique.txt &#124; sed -n '1,260p'</code> | 0 | Revisited the gameplay-owned initialization, numeric, constness, shadowing, and control diagnostics before final IMP-005/006/008/009 disposition | [Task 5 analyzer disposition](subsystems/03-gameplay.md#reload-reset-and-construction) |
| 2026-07-28 (Task 5; exact time not captured) | <code>tmpdir=$(mktemp -d /tmp/fb-sdl3-task5-inventory.XXXXXX) &amp;&amp; git ls-tree -r --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 &#124; rg '^(src&#124;server&#124;tests&#124;tools&#124;android&#124;web&#124;cmake&#124;docker&#124;\.github)/&#124;^(CMakeLists\.txt&#124;CMakeListsEmscripten\.txt&#124;README\.md&#124;SetupServer\.md&#124;WASM_PORT\.md&#124;start-server\.sh&#124;netlify\.toml&#124;shell\.nix&#124;default\.nix&#124;flake\.nix&#124;flake\.lock)$' &#124; sort &gt; "$tmpdir/pinned.txt" &amp;&amp; python3 -c "from pathlib import Path; t=chr(96); print('\\n'.join(sorted(x.split(t)[1] for x in Path('docs/audit/FILE_COVERAGE.md').read_text().splitlines() if x.startswith('&#124; '+t))))" &gt; "$tmpdir/ledger.txt" &amp;&amp; diff -u "$tmpdir/pinned.txt" "$tmpdir/ledger.txt"</code> | 0 | Coverage inventory exactly matched all 237 pinned-tree paths | [Task 5 coverage](subsystems/03-gameplay.md#coverage) |
| 2026-07-28 (Task 5; exact time not captured) | <code>ctest --test-dir build-audit-release -R 'netview&#124;netteams&#124;roundstats' --output-on-failure</code> | 0 | Required Release helper filter passed 3/3 | [Task 5 dynamic evidence](subsystems/03-gameplay.md#dynamic-evidence) |
| 2026-07-28 (Task 5; exact time not captured) | <code>ctest --test-dir build-audit-werror -R 'netview&#124;netteams&#124;roundstats' --output-on-failure</code> | 0 | Required warnings-strict Debug helper filter passed 3/3 | [Task 5 dynamic evidence](subsystems/03-gameplay.md#dynamic-evidence) |
| 2026-07-28 (Task 5; exact time not captured) | <code>ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-audit-sanitize -R 'netview&#124;netteams&#124;roundstats' --output-on-failure</code> | 8 | Required leak-enabled filter aborted 3/3 because Apple ASan reports `detect_leaks is not supported on this platform`; environment limitation, not a test pass | [Task 5 dynamic evidence](subsystems/03-gameplay.md#dynamic-evidence) |
| 2026-07-28 (Task 5; exact time not captured) | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-audit-sanitize -R 'netview&#124;netteams&#124;roundstats' --output-on-failure</code> | 0 | Accepted leak-disabled ASan+UBSan helper filter passed 3/3 with no diagnostic | [Task 5 dynamic evidence](subsystems/03-gameplay.md#dynamic-evidence) |
| 2026-07-28 (Task 5 harness development; exact time not captured) | <code>test -f /tmp/fb-sdl3-audit/task5_boundary_harness.cpp &amp;&amp; /usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task5_boundary_harness.cpp src/netview.cpp src/netteams.cpp src/roundstats_color.cpp -o /tmp/fb-sdl3-audit/task5_boundary_harness &amp;&amp; /tmp/fb-sdl3-audit/task5_boundary_harness &#124; tee /tmp/fb-sdl3-audit/task5_boundary_harness.log</code> | 1 | Initial compile exposed three harness API-name mistakes; non-evidence corrected before execution | Harness development chronology |
| 2026-07-28 (Task 5 harness development; exact time not captured) | <code>/usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task5_boundary_harness.cpp src/netview.cpp src/netteams.cpp src/roundstats_color.cpp -o /tmp/fb-sdl3-audit/task5_boundary_harness &amp;&amp; /tmp/fb-sdl3-audit/task5_boundary_harness &#124; tee /tmp/fb-sdl3-audit/task5_boundary_harness.log</code> | 1 | First executable run caught an error in the clear-winner oracle assertion; non-evidence corrected before acceptance | Harness development chronology |
| 2026-07-28 (Task 5; exact time not captured) | <code>/usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task5_boundary_harness.cpp src/netview.cpp src/netteams.cpp src/roundstats_color.cpp -o /tmp/fb-sdl3-audit/task5_boundary_harness &amp;&amp; /tmp/fb-sdl3-audit/task5_boundary_harness &#124; tee /tmp/fb-sdl3-audit/task5_boundary_harness.log</code> | 0 | Accepted fixed-seed boundary run passed players 1/2/5/6/20, teams 1-5, colors 5/8, both orientations, delta 15, and three-round/state scenarios | `/tmp/fb-sdl3-audit/task5_boundary_harness.cpp` and `.log` |
| 2026-07-28 (Task 5; exact time not captured) | <code>/usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined /tmp/fb-sdl3-audit/task5_boundary_harness.cpp src/netview.cpp src/netteams.cpp src/roundstats_color.cpp -fsanitize=address,undefined -o /tmp/fb-sdl3-audit/task5_boundary_harness_sanitize &amp;&amp; ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task5_boundary_harness_sanitize</code> | 0 | Sanitized fixed-seed boundary run printed the same PASS matrix with no ASan/UBSan diagnostic | [Task 5 dynamic evidence](subsystems/03-gameplay.md#dynamic-evidence) |
| 2026-07-28 (Task 5 production-harness development; exact time not captured) | <code>object_paths=($(find build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src -name '*.o' ! -name 'main.cpp.o' -print)); /usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task5_actual_gameplay_harness.cpp $object_paths build-audit-werror/libiniparser-static.a -Wl,-rpath,/opt/homebrew/lib /opt/homebrew/lib/libSDL3_image.0.4.4.dylib /opt/homebrew/lib/libSDL3_mixer.0.2.4.dylib /opt/homebrew/lib/libSDL3_ttf.0.2.2.dylib /opt/homebrew/lib/libSDL3.0.dylib -o /tmp/fb-sdl3-audit/task5_actual_gameplay_harness</code> | 1 | Initial strict compile rejected the test-only `private` visibility macro under `-Wkeyword-macro`; a scoped diagnostic pragma fixed the harness only | Production-harness development chronology |
| 2026-07-28 (Task 5 production-harness development; exact time not captured) | <code>object_paths=($(find build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src -name '*.o' ! -name 'main.cpp.o' -print)); /usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task5_actual_gameplay_harness.cpp $object_paths build-audit-werror/libiniparser-static.a -Wl,-rpath,/opt/homebrew/lib /opt/homebrew/lib/libSDL3_image.0.4.4.dylib /opt/homebrew/lib/libSDL3_mixer.0.2.4.dylib /opt/homebrew/lib/libSDL3_ttf.0.2.2.dylib /opt/homebrew/lib/libSDL3.0.dylib -o /tmp/fb-sdl3-audit/task5_actual_gameplay_harness &amp;&amp; /tmp/fb-sdl3-audit/task5_actual_gameplay_harness</code> | 1 | First executable run exposed a harness expectation that assumed random levels always start in standard orientation; corrected to validate and flip either generated orientation | Production-harness development chronology |
| 2026-07-28 (Task 5; exact time not captured) | <code>object_paths=($(find build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src -name '*.o' ! -name 'main.cpp.o' -print)); /usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task5_actual_gameplay_harness.cpp $object_paths build-audit-werror/libiniparser-static.a -Wl,-rpath,/opt/homebrew/lib /opt/homebrew/lib/libSDL3_image.0.4.4.dylib /opt/homebrew/lib/libSDL3_mixer.0.2.4.dylib /opt/homebrew/lib/libSDL3_ttf.0.2.2.dylib /opt/homebrew/lib/libSDL3.0.dylib -o /tmp/fb-sdl3-audit/task5_actual_gameplay_harness &amp;&amp; /tmp/fb-sdl3-audit/task5_actual_gameplay_harness</code> | 0 | Unchanged warnings-strict production objects passed the full boundary matrix and directly reproduced BUG-018/019 | [Task 5 dynamic evidence](subsystems/03-gameplay.md#dynamic-evidence) |
| 2026-07-28 (Task 5; exact time not captured) | <code>object_paths=($(find build-audit-sanitize/CMakeFiles/frozen-bubble-sdl3.dir/src -name '*.o' ! -name 'main.cpp.o' -print)); /usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined /tmp/fb-sdl3-audit/task5_actual_gameplay_harness.cpp $object_paths build-audit-sanitize/libiniparser-static.a -fsanitize=address,undefined -Wl,-rpath,/opt/homebrew/lib /opt/homebrew/lib/libSDL3_image.0.4.4.dylib /opt/homebrew/lib/libSDL3_mixer.0.2.4.dylib /opt/homebrew/lib/libSDL3_ttf.0.2.2.dylib /opt/homebrew/lib/libSDL3.0.dylib -o /tmp/fb-sdl3-audit/task5_actual_gameplay_harness_sanitize &amp;&amp; ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task5_actual_gameplay_harness_sanitize</code> | 0 | Sanitized unchanged production objects passed the same matrix with no ASan/UBSan diagnostic | [Task 5 dynamic evidence](subsystems/03-gameplay.md#dynamic-evidence) |
| 2026-07-28T13:12:24Z | <code>object_paths=($(find build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src -name '*.o' ! -name 'main.cpp.o' -print)); /usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task5_actual_gameplay_harness.cpp $object_paths build-audit-werror/libiniparser-static.a -Wl,-rpath,/opt/homebrew/lib /opt/homebrew/lib/libSDL3_image.0.4.4.dylib /opt/homebrew/lib/libSDL3_mixer.0.2.4.dylib /opt/homebrew/lib/libSDL3_ttf.0.2.2.dylib /opt/homebrew/lib/libSDL3.0.dylib -o /tmp/fb-sdl3-audit/task5_actual_gameplay_harness_fix1 &amp;&amp; /tmp/fb-sdl3-audit/task5_actual_gameplay_harness_fix1</code> | 0 | Fix Round 1 strict harness printed `maxDeltaTunnel=BUG-025 collisionPlacement=adjacent actualPlacement=ceiling` using linked production collision/selection/placement code | [Maximum-delta proof](subsystems/03-gameplay.md#maximum-delta-collision-trace) |
| 2026-07-28T13:12:24Z | <code>object_paths=($(find build-audit-sanitize/CMakeFiles/frozen-bubble-sdl3.dir/src -name '*.o' ! -name 'main.cpp.o' -print)); /usr/bin/c++ -I/opt/homebrew/include -I/usr/local/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined /tmp/fb-sdl3-audit/task5_actual_gameplay_harness.cpp $object_paths build-audit-sanitize/libiniparser-static.a -fsanitize=address,undefined -Wl,-rpath,/opt/homebrew/lib /opt/homebrew/lib/libSDL3_image.0.4.4.dylib /opt/homebrew/lib/libSDL3_mixer.0.2.4.dylib /opt/homebrew/lib/libSDL3_ttf.0.2.2.dylib /opt/homebrew/lib/libSDL3.0.dylib -o /tmp/fb-sdl3-audit/task5_actual_gameplay_harness_fix1_sanitize &amp;&amp; ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task5_actual_gameplay_harness_fix1_sanitize</code> | 0 | Fix Round 1 ASan+UBSan harness printed the same BUG-025 outcome with no diagnostic | [Maximum-delta proof](subsystems/03-gameplay.md#maximum-delta-collision-trace) |
| 2026-07-28T13:05Z | <code>wc -l src/mainmenu.cpp src/mainmenu.h src/mainmenu_internal.h src/mainmenu_input.cpp src/mainmenu_netpanel.cpp src/mainmenu_panels.cpp src/mainmenu_server.cpp src/gamesettings.cpp src/gamesettings.h src/highscoremanager.cpp src/highscoremanager.h src/menubutton.cpp src/menubutton.h src/frozenbubble.cpp src/frozenbubble.h</code> | 0 | Counted 6,730 lines across all fifteen scoped files before review | [Task 6 scope](subsystems/04-lobby-settings-input.md#scope) |
| 2026-07-28T13:10Z | <code>grep -n 'errs&#92;&#124;line too long&#92;&#124;syntax' third_party/iniparser/iniparser.c</code> | 0 | Located the parse-failure sites proving `iniparser_load` returns NULL on syntax errors and over-long lines | [BUG-026 basis](subsystems/04-lobby-settings-input.md#settings-and-highscore-persistence-step-3) |
| 2026-07-28T13:12Z | <code>grep -rn 'controllerInputs' src/</code> | 0 | Proved `controllerInputs[5]` is zeroed and read but never written by `HandleControllerEvent` | [IMP-012](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| 2026-07-28T13:12Z | <code>grep -rn 'GAMEPAD_REMOVED&#92;&#124;JOYSTICK_REMOVED&#92;&#124;SDL_CloseGamepad' src/</code> | 0 | Only `bubblegame.cpp` closes gamepads; no removal handler exists in `frozenbubble.cpp` | [BUG-035](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| 2026-07-28T13:13Z | <code>sed -n '160,182p' /opt/homebrew/include/SDL3/SDL_gamepad.h</code> | 0 | SDL3 defines 26 gamepad buttons with `SDL_GAMEPAD_BUTTON_TOUCHPAD = 20` | [BUG-036](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| 2026-07-28T13:14Z | <code>grep -n 'SDL_SCANCODE_COUNT' /opt/homebrew/include/SDL3/SDL_scancode.h</code> | 0 | `SDL_SCANCODE_COUNT = 512`, fixing the bound BUG-028 and BUG-035 exceed | [BUG-028](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T13:16Z | <code>sed -n '1140,1200p' src/networkclient.cpp</code> | 0 | `TEAMCOUNT` is clamped 2-5 while `PLAYERTEAM_Pn` is stored unclamped | [SEC-007](subsystems/04-lobby-settings-input.md#option-serialization-and-validation-step-2) |
| 2026-07-28T13:17Z | <code>grep -n 'SendOptions' -A 30 src/networkclient.cpp</code> | 0 | Wire format and signature stop at `_P5`, so room slots 6-20 are unrepresentable | [BUG-040](subsystems/04-lobby-settings-input.md#option-serialization-and-validation-step-2) |
| 2026-07-28T13:18Z | <code>grep -rn 'showing2PPanel&#92;s*=&#92;&#124;selectedMode = 2&#92;&#124;SetupNewGame(2)' src/</code> | 0 | `showing2PPanel` is assigned false twice and true never; `SetupNewGame(2)` has no caller | [BUG-023 extension](subsystems/04-lobby-settings-input.md#menu-and-room-state-transitions-step-1) |
| 2026-07-28T13:20Z | <code>grep -E '^src/(mainmenu&#124;gamesettings&#124;highscoremanager&#124;menubutton&#124;frozenbubble)' /tmp/fb-sdl3-audit/cppcheck-project-unique.txt &#124; awk -F'[][]' '{print $2}' &#124; sort &#124; uniq -c &#124; sort -rn</code> | 0 | 90 scoped cppcheck records across 21 check IDs, led by 23 redundantAssignment and 21 uninitialized-member records | [Analyzer triage](subsystems/04-lobby-settings-input.md#analyzer-triage) |
| 2026-07-28T13:20Z | <code>grep -E '^src/(mainmenu&#124;gamesettings&#124;highscoremanager&#124;menubutton&#124;frozenbubble)' /tmp/fb-sdl3-audit/clang-tidy-project-unique.txt &#124; awk -F'[][]' '{print $2}' &#124; sort &#124; uniq -c &#124; sort -rn</code> | 0 | 79 scoped clang-tidy records across 16 check IDs, including the single command-processor and throwing-static-initialization hits | [Analyzer triage](subsystems/04-lobby-settings-input.md#analyzer-triage) |
| 2026-07-28T13:35Z | <code>/usr/bin/c++ -I/opt/homebrew/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task6/task6_settings_harness.cpp build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src/gamesettings.cpp.o build-audit-werror/libiniparser-static.a -Wl,-rpath,/opt/homebrew/lib /opt/homebrew/lib/libSDL3.0.dylib -o /tmp/fb-sdl3-audit/task6/task6_settings_harness</code> | 0 | Warnings-strict harness linked the unchanged production settings object with no diagnostic | `/tmp/fb-sdl3-audit/task6/task6_settings_harness.cpp` |
| 2026-07-28T13:38Z | <code>env HOME=/tmp/fb-sdl3-audit/task6/home_probe /tmp/fb-sdl3-audit/task6/task6_settings_harness /tmp/fb-sdl3-audit/task6/home_probe probe</code> | 4 | Isolation gate refused: with `HOME` alone, `SDL_GetPrefPath` still resolved to the user's real directory; no file was opened | [Limitations](subsystems/04-lobby-settings-input.md#limitations) |
| 2026-07-28T13:39Z | <code>env HOME=/tmp/fb-sdl3-audit/task6/home_probe CFFIXED_USER_HOME=/tmp/fb-sdl3-audit/task6/home_probe /tmp/fb-sdl3-audit/task6/task6_settings_harness /tmp/fb-sdl3-audit/task6/home_probe probe</code> | 0 | `ISOLATION=OK`: preference path resolved inside the temporary home | [Dynamic evidence](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T13:40Z | <code>shasum -a 256 "/Users/dchau/Library/Application Support/frozen-bubble/settings.ini" "/Users/dchau/Library/Application Support/frozen-bubble/highscores" "/Users/dchau/Library/Application Support/frozen-bubble/highlevelshistory" &#124; tee /tmp/fb-sdl3-audit/task6/real-prefs-baseline.txt</code> | 0 | Recorded pre-work hashes of the user's three real preference files | `/tmp/fb-sdl3-audit/task6/real-prefs-baseline.txt` |
| 2026-07-28T13:48Z | <code>chmod +x /tmp/fb-sdl3-audit/task6/run_matrix.sh &amp;&amp; bash /tmp/fb-sdl3-audit/task6/run_matrix.sh /tmp/fb-sdl3-audit/task6/task6_settings_harness /tmp/fb-sdl3-audit/task6/scen normal 10 2&gt;&amp;1 &#124; tee /tmp/fb-sdl3-audit/task6/matrix-normal.log &#124; head -200</code> | 0 | First matrix attempt: `head` closed the pipe and SIGPIPE killed the driver after case G. Truncated; non-evidence, superseded by the full rerun | Corrected immediately below |
| 2026-07-28T13:50Z | <code>bash /tmp/fb-sdl3-audit/task6/run_matrix.sh /tmp/fb-sdl3-audit/task6/task6_settings_harness /tmp/fb-sdl3-audit/task6/scen normal 10 &gt; /tmp/fb-sdl3-audit/task6/matrix-normal.log 2&gt;&amp;1</code> | 0 | All twelve fixtures ran with `ISOLATION=OK`; C/G reset the file, E/F kept out-of-range height and NaN speed, H saved silently, I/J/K never returned | [Persistence matrix](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T13:51Z | <code>/usr/bin/c++ -I/opt/homebrew/include -I/Users/dchau/gr/frozen-bubble-sdl3/src -I/Users/dchau/gr/frozen-bubble-sdl3/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined /tmp/fb-sdl3-audit/task6/task6_settings_harness.cpp build-audit-sanitize/CMakeFiles/frozen-bubble-sdl3.dir/src/gamesettings.cpp.o build-audit-sanitize/libiniparser-static.a -fsanitize=address,undefined -Wl,-rpath,/opt/homebrew/lib /opt/homebrew/lib/libSDL3.0.dylib -o /tmp/fb-sdl3-audit/task6/task6_settings_harness_sanitize</code> | 0 | ASan+UBSan harness linked the sanitized production settings object | `/tmp/fb-sdl3-audit/task6/task6_settings_harness_sanitize` |
| 2026-07-28T13:51Z | <code>ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 bash /tmp/fb-sdl3-audit/task6/run_matrix.sh /tmp/fb-sdl3-audit/task6/task6_settings_harness_sanitize /tmp/fb-sdl3-audit/task6/scen-san sanitize 20 &gt; /tmp/fb-sdl3-audit/task6/matrix-sanitize.log 2&gt;&amp;1</code> | 0 | Same twelve outcomes plus UBSan `load of value 99999, which is not a valid value for type 'SDL_Scancode'` (SIGABRT) in case F | [BUG-028](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T13:52Z | <code>HOME=… CFFIXED_USER_HOME=… SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 python3 -c '…subprocess.run(["build-audit-sanitize/frozen-bubble-sdl3"], timeout=12, cwd=clienthome)…'</code> | 0 | Clean isolated home: defaults created, both highscore files reported missing, reached `RunForEver: starting loop`, killed at the 12 s timeout; wrote its log into the working directory | [Full-client runs](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T13:53Z | <code>printf 'notanumber,bob,12.5,3&#92;n' &gt; clienthome2/…/highscores &amp;&amp; HOME=… CFFIXED_USER_HOME=… SDL_VIDEODRIVER=dummy python3 -c '…subprocess.run(["build-audit-sanitize/frozen-bubble-sdl3"], timeout=12…)…'</code> | 0 | Child exit −6: `libc++abi: terminating due to uncaught exception of type std::invalid_argument: stoi: no conversion` | [BUG-032](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T13:54Z | <code>for h in clienthome3 clienthome4; do HOME=… CFFIXED_USER_HOME=… SDL_VIDEODRIVER=dummy python3 -c '…subprocess.run(["build-audit-sanitize/frozen-bubble-sdl3"], timeout=12…)…' $h; done</code> | 0 | Read-only malformed settings hung the shipped binary for the full 12 s with no window; corrupt `highlevelshistory` aborted with exit −6 | [BUG-026 and BUG-032](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T13:55Z | <code>mkdir -p /tmp/fb-sdl3-audit/task6/FakeBundle.app/Contents/Resources &amp;&amp; cp build-audit-sanitize/frozen-bubble-sdl3 … &amp;&amp; HOME=… CFFIXED_USER_HOME=… SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 python3 -c '…subprocess.run([FakeBundle…/frozen-bubble-sdl3], timeout=15…)…'</code> | 0 | Child exit −6 with UBSan `member call on misaligned address 0xbebebebebebebebe for type 'AudioMixer'` at `frozenbubble.cpp:228` from `main.cpp:30` | [BUG-034](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T13:55Z | <code>shasum -a 256 -c /tmp/fb-sdl3-audit/task6/real-prefs-baseline.txt</code> | 0 | All three real preference files reported `OK`; the user's preferences were never modified | [Limitations](subsystems/04-lobby-settings-input.md#limitations) |
| 2026-07-28T13:56Z | <code>cd /tmp/fb-sdl3-audit/task6 &amp;&amp; /usr/bin/c++ … numkeys.cpp … -o numkeys &amp;&amp; SDL_VIDEODRIVER=dummy ./numkeys</code> | 0 | Printed `numkeys=512 SDL_SCANCODE_COUNT=512`, the exact bound BUG-028 and BUG-035 exceed | [BUG-028](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-28T14:05Z | <code>python3 - &lt;&lt;'PY' … rewrite the fifteen Task 6 rows of docs/audit/FILE_COVERAGE.md … PY</code> | 0 | Printed `rewritten rows: 15`; every scoped file now carries a Task 6 disposition | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28T14:06Z | <code>test "$(awk -F'`' '/^&#92;&#124; `/ {count++} END {print count+0}' docs/audit/FILE_COVERAGE.md)" = "237" &amp;&amp; tmpdir=$(mktemp -d …) &amp;&amp; git ls-tree -r --name-only 09d6c7bf… &#124; rg '…' &#124; sort &gt; "$tmpdir/pinned.txt" &amp;&amp; python3 -c '…' &gt; "$tmpdir/ledger.txt" &amp;&amp; diff -u "$tmpdir/pinned.txt" "$tmpdir/ledger.txt"</code> | 0 | `rows=237 OK` and `inventory equality OK`: the ledger still equals the pinned-tree filter exactly | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-28 (Task 7 scope; exact time not captured) | <code>wc -l src/shaderstuff.cpp src/shaderstuff.h src/transitionmanager.cpp src/transitionmanager.h src/ttftext.cpp src/ttftext.h src/audiomixer.cpp src/audiomixer.h src/sdl3_compat.h src/frozenbubble.cpp src/bubblegame_render.cpp src/mainmenu_panels.cpp</code> | 0 | Counted 4,925 lines across the twelve scoped files before review | [Task 7 scope](subsystems/05-render-audio.md#scope) |
| 2026-07-28 (Task 7 Step 2; exact time not captured) | <code>file share/gfx/menu/fblogo.png share/gfx/menu/fblogo-mask.png share/gfx/menu/txt_*_text.png share/gfx/menu/txt_*_outlined_text.png</code> | 0 | Every shipped effect-input PNG is 8-bit/color RGBA | [Task 7 static review](subsystems/05-render-audio.md#static-review) |
| 2026-07-28 (Task 7 Step 1; exact time not captured) | <code>grep -n 'IMG_Load…SDL_DestroyTexture…TTF_OpenFont…MIX_…new SDL_Rect…' src/*.cpp src/*.h</code> | 0 | Enumerated every SDL create/destroy site for the Step 1 ownership table | [Ownership table](subsystems/05-render-audio.md#static-review) |
| 2026-07-28 (Task 7 Step 1; exact time not captured) | <code>grep -n 'hurryTexture&#92;&#124;DestroyTexture' src/bubblegame.cpp</code> | 0 | 20 `hurryTexture` load sites and zero matching destroy sites (BUG-042 basis) | [BUG-042](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Step 1; exact time not captured) | <code>grep -n 'LoadPenguin' src/*.cpp</code> | 0 | Every `NewGame` player-count case calls `LoadPenguin` per player; no destroy path exists for the 394-texture set | [BUG-042](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28T14:27:35Z | <code>grep -rn 'transitionTexture' src/</code> | 0 | Only the null initializer and the by-value `effect()` argument exist; the member is never assigned (BUG-041 basis) | [BUG-041](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28T14:27:35Z | <code>for fn in draw_line_ blacken_ alphaize_ pixelize_ rotate_nearest_ rotate_bicubic_ autopseudocrop store_effect copy_line shrink_; do printf '%s: ' $fn; grep -rl "$fn" src/ --exclude=shaderstuff.cpp --exclude=shaderstuff.h &#124; tr '&#92;n' ' '; echo; done</code> | 0 | Seven effect helpers have no external caller (dead code, IMP-009); `shrink_`'s only caller is `highscoremanager.cpp` | [Dismissed candidates](subsystems/05-render-audio.md#dismissed-candidates) |
| 2026-07-28 (Task 7 triage; exact time not captured) | <code>grep -E '^src/(shaderstuff&#124;transitionmanager&#124;ttftext&#124;audiomixer&#124;sdl3_compat&#124;bubblegame_render&#124;mainmenu_panels&#124;frozenbubble)' /tmp/fb-sdl3-audit/cppcheck-project-unique.txt</code> | 0 | 229 scoped cppcheck records; promoted only the BUG-001/IMP-010/IMP-007/IMP-005 instances | [Analyzer triage](subsystems/05-render-audio.md#static-review) |
| 2026-07-28 (Task 7 triage; exact time not captured) | <code>grep -E '^src/(shaderstuff&#124;transitionmanager&#124;ttftext&#124;audiomixer&#124;sdl3_compat&#124;bubblegame_render&#124;mainmenu_panels&#124;frozenbubble)' /tmp/fb-sdl3-audit/clang-tidy-project-unique.txt &#124; awk -F'[][]' '{print $2}' &#124; sort &#124; uniq -c &#124; sort -rn</code> | 0 | 248 scoped clang-tidy records across 16 check IDs, led by 86 narrowing and 60 implicit-widening | [Analyzer triage](subsystems/05-render-audio.md#static-review) |
| 2026-07-28 (Task 7 harness; exact time not captured) | <code>/usr/bin/c++ -I/opt/homebrew/include -I…/src -I…/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src/shaderstuff.cpp.o …transitionmanager… …audiomixer… …ttftext… …gamesettings… …platform… build-audit-werror/libiniparser-static.a -Wl,-rpath,/opt/homebrew/lib …SDL3/SDL3_image/SDL3_mixer/SDL3_ttf dylibs… -o /tmp/fb-sdl3-audit/task7/task7_harness</code> | 0 | Warnings-strict harness linked six unchanged production objects with no diagnostic | `/tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp` |
| 2026-07-28 (Task 7 harness; exact time not captured) | <code>/usr/bin/c++ … -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined … build-audit-sanitize object set … -o /tmp/fb-sdl3-audit/task7/task7_harness_sanitize</code> | 0 | ASan+UBSan harness linked the sanitized production objects | `/tmp/fb-sdl3-audit/task7/task7_harness_sanitize` |
| 2026-07-28 (Task 7 isolation; exact time not captured) | <code>shasum -a 256 "/Users/dchau/Library/Application Support/frozen-bubble/settings.ini" "…/highscores" "…/highlevelshistory" &#124; tee /tmp/fb-sdl3-audit/task7/real-prefs-baseline.txt</code> | 0 | Recorded pre-work hashes of the user's three real preference files | `/tmp/fb-sdl3-audit/task7/real-prefs-baseline.txt` |
| 2026-07-28 (Task 7 isolation; exact time not captured) | <code>mkdir -p /tmp/fb-sdl3-audit/task7/home7 &amp;&amp; env HOME=/tmp/fb-sdl3-audit/task7/home7 CFFIXED_USER_HOME=/tmp/fb-sdl3-audit/task7/home7 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness probe /tmp/fb-sdl3-audit/task7/home7</code> | 0 | `ISOLATION=OK`: pref path resolved inside the temporary home before any preference-owning singleton existed | [Task 7 dynamic evidence](subsystems/05-render-audio.md#dynamic-evidence) |
| 2026-07-28 (Task 7 Step 2; exact time not captured) | <code>SDL_VIDEODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness formats "$PWD/share"</code> | 0 | All eight effect inputs are 4 bpp tight-pitch; `fblogo-mask.png` has zero white border pixels | `/tmp/fb-sdl3-audit/task7/formats.log` |
| 2026-07-28 (Task 7 Step 2; exact time not captured) | <code>SDL_VIDEODRIVER=dummy ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task7/task7_harness_sanitize oobdemo</code> | 134 | Production `get_pixel` (`shaderstuff.cpp:49`) performed a 4-byte heap-buffer-overflow READ exactly 0 bytes past a tightly-sized surface when passed `x == w` (IMP-013 demonstration) | `/tmp/fb-sdl3-audit/task7/oobdemo.log` |
| 2026-07-28 (Task 7 Step 5; exact time not captured) | <code>SDL_VIDEODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness texleak 100</code> | 0 | 100 production `synchro_after` frames grew RSS linearly 18→138 MB: exactly 1.2 MB (one dropped 640×480 texture) per frame | [BUG-041](subsystems/05-render-audio.md#dynamic-evidence) |
| 2026-07-28T14:24:51Z | <code>env HOME=… CFFIXED_USER_HOME=… SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness transition /tmp/fb-sdl3-audit/task7/home7 "$PWD/share" 5</code> | 0 | `ISOLATION=OK`, default `gfxLevel=1`; five full production `DoSnipIn`/`TakeSnipOut` cycles grew RSS 21→149 MB (one allocator-return dip recorded honestly) | `/tmp/fb-sdl3-audit/task7/transition.log` |
| 2026-07-28T14:25:15Z | <code>env HOME=… CFFIXED_USER_HOME=… SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task7/task7_harness_sanitize audio /tmp/fb-sdl3-audit/task7/home7 "$PWD/share" 3</code> | 0 | Three sanitized audio lifecycle cycles (music replace, six overlapping SFX, missing file, pause/mute, unknown track, `Dispose`) with no diagnostic; missing-file repeats re-log a stale empty error | `/tmp/fb-sdl3-audit/task7/audio-sanitize.log` |
| 2026-07-28 (Task 7 Step 5; exact time not captured) | <code>SDL_VIDEODRIVER=dummy ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task7/task7_harness_sanitize ttftext "$PWD/share" 200</code> | 0 | 200 sanitized text lifecycle iterations (empty/10 KB strings, font replacement, external-font adoption, destruction) with no diagnostic; RSS plateaued after iteration 50 | `/tmp/fb-sdl3-audit/task7/ttftext-sanitize.log` |
| 2026-07-28 (Task 7 Step 5; exact time not captured) | <code>SDL_VIDEODRIVER=dummy ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task7/task7_harness_sanitize texfail "$PWD/share"</code> | 134 | UBSan: `member access within null pointer of type 'SDL_Surface'` at `shaderstuff.h:67` — `LoadEmptyAndApply` missing-asset crash (BUG-001) | `/tmp/fb-sdl3-audit/task7/texfail.log` |
| 2026-07-28 (Task 7 Step 5; exact time not captured) | <code>SDL_VIDEODRIVER=dummy ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task7/task7_harness_sanitize texfail2</code> | 134 | UBSan: null-member access at `shaderstuff.h:55` — `LoadFromSurface(nullptr, …)` pre-check dereference (BUG-001) | `/tmp/fb-sdl3-audit/task7/texfail2.log` |
| 2026-07-28 (Task 7 Step 5; exact time not captured) | <code>env HOME=… CFFIXED_USER_HOME=… SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 /tmp/fb-sdl3-audit/task7/task7_harness_sanitize transition /tmp/fb-sdl3-audit/task7/home7 "$PWD/share" 6</code> | 0 | Six sanitized full production transition animations: no ASan/UBSan diagnostic in any pixel loop; RSS 44→264 MB shows the BUG-041 growth under sanitizer too | `/tmp/fb-sdl3-audit/task7/transition-sanitize.log` |
| 2026-07-28 (Task 7 cleanup; exact time not captured) | <code>shasum -a 256 -c /tmp/fb-sdl3-audit/task7/real-prefs-baseline.txt</code> | 0 | All three real preference files reported `OK`; the user's preferences were never modified | [Task 7 limitations](subsystems/05-render-audio.md#limitations) |
| 2026-07-28T14:27:35Z | <code>git diff --stat 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 -- src server</code> | 0 | No output; production source remains identical to the pinned baseline | Audit baseline above |
| 2026-07-28 (Task 7 ledger; exact time not captured) | <code>test "$(awk -F'&#96;' '/^&#92;&#124; &#96;/ {count++} END {print count+0}' docs/audit/FILE_COVERAGE.md)" = "237" &amp;&amp; tmpdir=$(mktemp -d /tmp/fb-sdl3-task7-inventory.XXXXXX) &amp;&amp; git ls-tree -r --name-only 09d6c7bf… &#124; rg '…' &#124; sort &gt; "$tmpdir/pinned.txt" &amp;&amp; python3 -c '…' &gt; "$tmpdir/ledger.txt" &amp;&amp; diff -u "$tmpdir/pinned.txt" "$tmpdir/ledger.txt"</code> | 0 | `rows=237 OK` and `inventory equality OK` after the Task 7 disposition rewrites | [FILE_COVERAGE.md](FILE_COVERAGE.md) |

## Limitations

- Emscripten, cppcheck, and clang-tidy were absent at bootstrap and installed successfully in Task 2. Homebrew LLVM remains keg-only, so the audit invokes clang-tidy by its absolute `$(brew --prefix llvm)/bin` path.
- The default Release build is successful but not warning-clean: AppleClang emitted 51 server warning instances from 27 unique locations. Task 3A confirmed IMP-001 through IMP-004 as implementation improvements; the audit itself intentionally leaves production code unchanged.
- The strict Debug build cannot complete until IMP-001 through IMP-004 are resolved in a future remediation task. Its subsequent 3/5 not-run CTest result is a downstream missing-executable consequence, not an independent candidate.
- Apple ASan does not support leak detection on this host. The required leak-enabled run is recorded as an environment limitation; the accepted leak-disabled verification passed four unaffected tests plus the isolated foreground server-list assertions with no sanitizer diagnostic.
- The sanitizer build's two `sprintf` deprecation warnings are in bundled `third_party/iniparser`, classified as vendored dependency noise pending Task 9 boundary/version review.
- The exact clang-tidy helper command was not directly usable with keg-only LLVM 22: it needed an explicit binary, explicit check families, and an Xcode SDK sysroot. All failed attempts and the successful reproducible fallback are retained.
- Cppcheck and clang-tidy are broad signal sources, not test or proof substitutes. Task 2 triaged every project-owned diagnostic by counted family, but assigned subsystem gates still own semantic confirmation or dismissal.
- REL-002 prevents the registered server-list CTest result from proving which binary served the request on POSIX. Task 2 therefore records the raw result but accepts only the supplemental dynamic-port, foreground, live-child-verified runs as Release/sanitizer server evidence.
- Only native macOS arm64 build/test/analyzer baselines were run. Linux,
  Windows, and Android-device behavior has not been tested. Task 4's isolated
  browser configure passed, but compilation stopped at the documented unpatched
  SDL3_image/SDL3_mixer SDK boundary. Both audited client translation units did
  compile directly for WASM with warnings; no full link or browser runtime was
  tested.
- No release/deployment credentials, signing credentials, external hosts, or interactive gameplay scenarios were evaluated in Task 2.
- Task 3 executed no socket, server, process, port, harness, DNS, HTTP, signal,
  fault-injection, or other runtime/network scenario. Three whole-task runtime
  dispatches and one split runtime dispatch were rejected by the automated
  classifier before producing evidence; the user then explicitly skipped
  security work. All Task 3 dispositions are source proofs, and the omitted
  security/runtime matrix remains a final-audit limitation.
- Task 4 ran no hostile input, server, socket, proxy, browser, or graphical
  gameplay scenario. The available bot could not autonomously establish and
  assert two rounds, so passing unit checks do not close runtime synchronization
  coverage. Windows conclusions are static portability proofs.
- Task 5 ran no malformed/hostile placement traffic, graphical client, or live
  multi-client gameplay. Its headless production-object harness calls actual
  private gameplay methods but does not render, play audio, drive collision
  animation, invoke full `NewGame`/`ReloadGame` transitions, or exercise real
  network arrival timing. SEC-003 and omitted security runtime remain
  limitations rather than passes.
- The local two-player and disconnect option-propagation origins identified by
  BUG-021/023 crossed into Task 6, which resolved both menu origins and closed
  that handoff.
- macOS `SDL_GetPrefPath` ignores `HOME`, so Task 6 isolation required
  `CFFIXED_USER_HOME`. The first probe therefore resolved to the user's real
  preference directory; the harness gate aborted before opening a file, the
  directory and its three files pre-existed, and all three hashes were verified
  unchanged afterwards.
- Task 6 exercised only the settings subsystem through a linked production
  object. All full-client runs used dummy video/audio drivers and were killed at
  the title screen, so no menu navigation, panel transition, key rebinding,
  controller event, or lobby interaction was driven at runtime; those
  conclusions are source traces.
- Task 6 started no server or connection, so every network lobby transition was
  reviewed statically. BUG-033 was deliberately not reproduced because doing so
  kills processes on this host, and BUG-035/BUG-036 need six to eleven physical
  gamepad hot-plug cycles this environment cannot supply.
- Per the user's scope restriction, Task 6 ran no security-specific runtime
  test. SEC-007 and the SEC-004 lobby-side consumption are code-supported
  inferences; the omitted forged-`OPTIONS` and out-of-range-team checks are a
  final-audit limitation, not a pass. WASM, Windows, and Android menu/input
  paths were likewise reviewed statically only.
- Task 7's rendering ran exclusively on the dummy video driver's software
  renderer: no Metal/GPU renderer, real window, fullscreen toggle, live
  resize, or real audio device was exercised, and no full-client menu-to-game
  navigation was driven. BUG-042 is therefore a complete static causal proof,
  not a runtime reproduction. Because Apple ASan cannot detect leaks, Task 7's
  leak conclusions rest on the exhaustive ownership table, grep-verified
  destroy-site absence, and RSS measurements; `effect()`'s unseeded random
  selection means the six sanitized animations did not provably cover all
  five effect families. WASM/Android render and audio paths remain Task 8
  static scope, and no security-specific runtime test was run in Task 7.

## Processes and cleanup

- REL-002's first reproduction created Task 2-owned daemon PID 95766 on temporary port 25512; it was identified by exact command/start time, terminated, and the port was verified closed. The preliminary hardcoded-port reruns were not accepted as ownership evidence. The accepted dynamic-port foreground reruns asserted the exact child alive and cleaned up normally.
- Pre-existing listeners on ports 15511, 15512, 15113, and 15998 were observed during isolation diagnosis. They predate Task 2 or are outside its assigned ownership and were intentionally left untouched; no Task 2-owned server, client, proxy, port listener, background process, or shell session remains.
- Task 3 started no runtime process, listener, port, harness, client, or traffic,
  so it created no external state requiring cleanup. Existing foreign listeners
  were not queried or disturbed during Task 3 closure and remain untouched.
- Task 4 likewise created no listener, server, proxy, client connection, or
  gameplay traffic. Read-only inventory found the same foreign listeners and
  left them untouched. The failed `/tmp/fb-sdl3-audit/task4-wasm-build` tree is
  regenerable local build evidence and owns no running process.
- Task 5 started no listener, client, graphical session, or background process
  and touched no preferences. Its normal and sanitizer oracle/production-object
  harness binaries, sources, and log under `/tmp/fb-sdl3-audit/` are local
  regenerable evidence; they own no running process or external state.
- Task 6 started no listener, server, client connection, or background process,
  and killed no process. Its five sanitized full-client runs used dummy drivers,
  isolated preference homes, and kill timeouts; all exited or were terminated
  within their timeout, and none remains running. Every scenario home, harness
  binary, source, and log lives under `/tmp/fb-sdl3-audit/task6/`, including one
  copy of the sanitized client inside a `FakeBundle.app` layout; all are local
  regenerable evidence owning no external state. The user's real preference
  files were hashed before and verified byte-identical after the gate.
- Task 7 started no listener, server, client connection, or background
  process, and killed no process. Its harness runs used dummy video/audio
  drivers, an isolated `CFFIXED_USER_HOME` preference home, and read-only
  `share/` assets; the two expected UBSan crash reproductions terminated
  themselves (exit 134) and left nothing running. All Task 7 harness sources,
  binaries, logs, and the isolated home live under
  `/tmp/fb-sdl3-audit/task7/` as local regenerable evidence owning no
  external state. The user's real preference files were hashed before and
  verified byte-identical after the gate.
- Temporary files include the Task 1 inventory files and Task 2 analyzer logs/triage artifacts under `/tmp/fb-sdl3-audit/`; all are local, regenerable evidence and contain no credentials.
- The four generated audit build directories are retained for Tasks 3 and 9
  (especially the sanitized server) and are locally excluded through untracked
  `.git/info/exclude` entries. Remove them after the complete audit; no listener
  or child process remains.
