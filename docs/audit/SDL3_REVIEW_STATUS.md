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
| Agent | Codex subagents `task_1_implementer` (bootstrap), `task_2_implementer` (baselines), `task_3a_static` (server static review), `task_3c_synthesis` (static Task 3 closure), `task_4_implementer` (client/synchronization review), and `task_5_implementer` (gameplay review), plus the Task 6 (lobby/settings/input), Task 7 (render/audio), Task 8 (platform ports), and Task 9 (build/release/tooling) implementer agents |
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
| Emscripten | 6.0.4-git (Homebrew; initially absent, installed successfully in Task 2). Task 8 clone-copied its `libexec` tree to `/tmp/fb-sdl3-audit/task8/emsdk/` and patched only the copy with the repo's SDL3_image/SDL3_mixer port files; the Homebrew installation is unmodified. The launcher honours `EMSDK_PYTHON`, which this host needs because Xcode's Python 3.9.6 precedes Homebrew's on `PATH` |
| cppcheck | 2.21.0 (Homebrew; initially absent, installed successfully in Task 2) |
| clang-tidy | Homebrew LLVM 22.1.8, optimized build (initially absent, installed successfully in Task 2; invoked by absolute keg-only path) |

## Current state

- Phase: Phase 2 — subsystem review
- Active gate: Task 10 (pending)
- Exact next action: Begin Task 10, Step 1: define the recorded matrix before launching processes.

## Gate checklist

| Task | Gate | Status |
|---|---|---|
| Task 1 | Bootstrap resumable audit workspace | complete |
| Task 2 | Reproducible build, test, sanitizer, and analysis baselines | complete |
| Task 3 | C server and untrusted TCP/WebSocket protocol | complete (static evidence; runtime/security matrix omitted by user direction) |
| Task 4 | Native/WASM clients and multiplayer synchronization | complete (static plus existing bot unit checks; security traffic and two-round smoke omitted) |
| Task 5 | Gameplay rules, board algorithms, and round state | complete (Fix Round 1 added a core invariant ledger and exact maximum-delta production-object evidence) |
| Task 6 | Lobby, settings, persistence, and input | complete (static review plus isolated-preferences runtime matrix; security runtime and live-server lobby transitions omitted) |
| Task 7 | Rendering, transitions, fonts, and audio lifecycle | complete (Fix Round 1 corrected BUG-001's leak quantity, BUG-041's trigger set, the pixel-format claim, and IMP-013's attribution; reopened IMP-005's render slice into BUG-043 and registered BUG-044, both reproduced against production objects. Fix Round 2 corrected the `TTFText` instance count Fix Round 1 had misread from a comment — 38 fixed members, not 23 — plus the `MainMenu` texture and `cell()` churn counts, completed the `idleSPButtons` dismissal with a full consequence trace that disproves the proposed indeterminate-rect mechanism, and registered BUG-045, reproduced against the production `ttftext.cpp` object; dummy-driver-only rendering and full-client navigation omissions recorded) |
| Task 8 | Native, WASM, and Android platform integration | complete (Android release APK built locally with zero tracked-file drift; WASM linked in full against a disposable port-patched Emscripten copy; packaged-path, logger, and preference behavior reproduced against unchanged production objects. Browser runtime, Android device runtime, Linux/Windows execution, whole-program packaged-layout startup, and dynamic-library independence recorded as unavailable, not passed. Fix Round 1 applied seven accepted review findings: BUG-048's occurrence counts and characterization corrected, BUG-046 extended with the version-bump and stale-asset-on-update consequences, REL-008 reassessed High → Medium with two mitigating facts, the coverage bootstrap count corrected 20 → 21, two brief Step 6 substitutions added to Limitations, the `web/index.html` script-tag wording corrected, and a full count sweep that found two wrong quantities of twenty re-derived. Fix Round 2 applied one accepted Important and six accepted Minor findings: the REL-008 "highest-impact" superlative re-anchored to name what actually reaches shipped artifacts, the Fix Round 1 opening summary's review-coverage and attribution wording corrected, the count-sweep enumeration reconciled to its stated eighteen, eleven bundled-command ledger rows split to one command per row with two non-integer exit cells resolved, the gate-conclusion completion claim qualified against its recorded substitutions, the `syncfs` "own body" wording corrected to "own definition", and the WASM nickname read corrected from `EM_ASM` to `EM_ASM_PTR`) |
| Task 9 | Build, tests, packaging, CI, deployment, tooling, and operations | complete (all five build definitions compared and reconciled; workflow, Compose, CMake, Gradle and Python configurations parsed locally; every confirmed defect from Tasks 3-9 mapped to a test, a dynamic case, or a registered gap; all 21 remaining pending coverage rows dispositioned. Fourteen IDs registered — REL-009..014 and IMP-016..023 — four entries extended (REL-004 severity Low → Medium, REL-006, REL-007, REL-008), IMP-008 closed, four candidates dismissed with counter-evidence including one disproved by running the command it doubted. The gate also corrected an inherited premise: `CLAUDE.md`'s claim that four of five platform build jobs are disabled with `if: false` is false at the pinned baseline — 0 of 11 jobs carry it — so Task 8's CI-reachability clause is corrected here and in the registry. No workflow was executed, no container started, and no external network operation performed; those are recorded as limitations, not passes. Fix Round 1 applied two accepted Important and two accepted Minor findings: REL-013's DLL count corrected 20 → 21 with the `tr`/undercount root cause recorded, REL-007's citation split across the two workflow steps its four password instances actually span, FILE_COVERAGE.md's inventory rule corrected to stop claiming a second `pending` location that does not exist, the "ten headings, each once" phrasing restated to name the subsystem notebooks it actually governs, and a re-run count sweep that found only the DLL count reproduced differently. Fix Round 2 applied two accepted Important and one accepted Minor finding: a Fix Round 1 ledger row that bundled sixteen independent commands under one non-representative exit was removed and split into sixteen individually re-run rows with honest exits, that same row's mislabeled "fourteen" quantity count corrected to sixteen commands (seventeen quantities), and the two identical "Task 9 Fix Round 1" headings disambiguated into "Findings" and "Ledger" with both misdirected cross-links retargeted to the ledger) |
| Task 10 | Cross-subsystem dynamic integration matrix | pending |
| Task 11 | Complete file coverage and prioritized improvements | pending |
| Task 12 | Independent final challenge | pending |
| Task 13 | Complete repository review report | pending |

## Active candidates

- No Task 7 candidate remains open. BUG-001 is confirmed (both `TextureEx`
  null-deref orderings reproduced under UBSan against production code plus the
  rect/surface leak family — 2 owner-less rects per `InitCandy`, since the
  three `LoadEmptyAndApply` sites are an `if`/`else if` chain); IMP-007 and
  IMP-010 are confirmed improvements with their cross-owner dispositions
  finished; the IMP-006 render slice is closed without promotion. Fix Round 1
  **reopened** the IMP-005 render slice, promoted its one reachable instance to
  BUG-043, and re-closed it; IMP-005 stands as a confirmed improvement.
- No Task 8 candidate remains open either. Every candidate this gate raised
  reached a terminal state: nine were promoted to new IDs, three were recorded
  by extending REL-003, REL-004 and BUG-033, and six were dismissed with
  counter-evidence that traces the consequence rather than noting the absence of
  a failure.
- No Task 9 candidate remains open. Fourteen were promoted to new IDs
  (REL-009..014, IMP-016..023), four were recorded by extending REL-004, REL-006,
  REL-007 and REL-008, one — REL-002's remediation — was discharged as a design
  rather than a code change, and four were dismissed with counter-evidence that
  traces each consequence.
- **IMP-008 is closed** and confirmed as an improvement with no defect promoted
  from any of its families. Task 8 closed its platform slice without promotion
  (the platform sources declare no uninitialized state-bearing member, perform no
  narrowing conversion, and their only cleanup surface is the dead code captured
  as IMP-015). Task 9 closed the last slice: neither cppcheck nor clang-tidy
  analyses CMake, YAML, shell, Python, Nix, TOML or Markdown, so **0** diagnostics
  of any IMP-008 family have a path among Task 9's 21 files, and the twelve
  non-`SEC-003` `bugprone-unchecked-string-to-number-conversion` sites all lie in
  files that closed gates already dispositioned. The boundary Task 9 did own —
  the vendored dependency Task 2 deferred — produced IMP-023 and REL-014
  instead.
- Task 7 added BUG-041 (confirmed, runtime-reproduced transition texture
  leak), BUG-042 (confirmed, static-proven per-match penguin/hurry texture
  reload leak), and IMP-013 (confirmed improvement: off-by-one clamp bounds —
  `get_pixel`'s own at `shaderstuff.cpp:49` and the `set_pixel` call sites at
  `:488`/`:1155`; `set_pixel` itself has no clamp — ASan-demonstrated,
  unreachable with shipped assets). Fix Round 1 added BUG-043 (confirmed,
  runtime-reproduced: `BubbleGame::targetingText` never receives a font, so the
  multiplayer targeting indicator can never render) and BUG-044 (confirmed,
  runtime-reproduced: unchecked `activeSPButtons` loads dereferenced at
  `mainmenu_panels.cpp:198` and `:213`), and extended IMP-012 with the unused
  `FrozenBubble::menuText` member. Fix Round 2 added BUG-045 (confirmed,
  runtime-reproduced: `TTFText`'s reset-to-empty copy constructor and no-op
  copy assignment blank every highscore row stored in `levelsetScores`, so
  `highscoremanager.cpp:339` renders a null texture through a zero rect) — the
  promoted instance of IMP-007's copy semantics, which stands as a confirmed
  improvement. At the close of Task 7 the registry held 69 unique IDs.
- Task 8 added BUG-046 (a failed or partial Android asset extraction is cached
  permanently by an unconditionally written version marker), BUG-047 (the log
  path is CWD-relative and its `creator`/`joiner1`-`joiner4` names count
  launches, not players — six consecutive launches produced five files and then
  reused `joiner4`; the read-only-CWD failure return is discarded by the
  caller), BUG-048 (WASM settings and highscores are written to volatile MEMFS —
  the linked artifact contains zero `IDBFS` references, four `syncfs`
  occurrences that all lie inside `FS.syncfs`'s own definition, and four
  `fb_nickname` `localStorage` occurrences, one `getItem` and three `setItem`),
  REL-005 (97 tracked dangling absolute symlinks
  under `android/app/jni/include/SDL2/`), REL-006 (stale platform build files
  and self-contradicting port documentation), REL-007 (unsigned local release
  APK, a per-build throwaway CI keystore with a repo-visible password, and an
  unpinned Gradle distribution), REL-008 (an installed macOS build resolves
  assets to the build machine's source tree), IMP-014 (2,666,728 bytes of
  duplicate and unreferenced native libraries in the release APK), and IMP-015
  (the dead platform layer). It extended REL-003, REL-004 and BUG-033 rather
  than duplicating them. At the close of Task 8 the registry held 78 unique IDs.
- Task 9 added REL-009 (maintained documentation contradicts the shipped system:
  `CLAUDE.md`'s CI section against a workflow with 0 of 11 jobs disabled, and
  `README.md`'s public-server-list format against `curlFetch`'s parser),
  REL-010 (`docker/setup.sh`'s `openssl rsa -check` gate rejects certbot's
  default ECDSA key — reproduced, exit 1 on two OpenSSL implementations — and
  the script then overwrites the operator's real certificate and key, while
  `SetupServer.md`'s renewal block copies to a path that does not exist from its
  own working directory), REL-011 (0 of 27 `uses:` commit-pinned, 5 on `@master`
  and each carrying the itch.io deploy secret, `latest` Emscripten, and an SDL3
  version matrix in which only Linux, Windows and Android agree), REL-012 (the
  macOS DMG is single-architecture, with no architecture in its name and no
  universal-binary setting anywhere), REL-013 (a 21-DLL copy loop that cannot
  fail and an NDK cache path that interpolates an undefined `env` value to
  empty), REL-014 (the vendored iniparser ships in every artifact with no
  licence, version, or provenance), and IMP-016 through IMP-023 (run the
  registered checks in CI; settings/persistence, gameplay-rules,
  protocol-parser, packaged-artifact and resource-lifetime suites; unify warning
  flags; constrain the iniparser dependency boundary). It extended REL-004
  (severity Low → Medium), REL-006, REL-007 and REL-008 rather than duplicating
  them, and closed IMP-008. The registry now holds **92 unique IDs**:
  BUG-001..048, SEC-001..007, REL-001..014, IMP-001..023.

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
  linked production objects, ~40-50 MB per game start and per round reload,
  the only two `DoSnipIn` producers; menu return is not a trigger) and BUG-042
  (every `NewGame` reloads 394 penguin textures per player plus its 17
  `hurryTexture` load sites, with no destroy site). BUG-001's two null-deref
  orderings were reproduced under UBSan at `shaderstuff.h:55` and `:67`; only
  the `:67` ordering is reachable from a missing asset alone, since production
  callers of `LoadFromSurface` pass `candyModif.sfc`. Fix Round 1 added
  BUG-043 (the multiplayer targeting indicator can never render —
  `targetingText` never receives a font) and BUG-044 (unchecked
  `activeSPButtons` `IMG_Load` results dereferenced in `SPPanelRender`), both
  reproduced against unchanged production objects, and extended IMP-012 with
  the dead `FrozenBubble::menuText` member. Sanitized production-object stress
  of transitions, audio lifecycle, and text lifecycle produced no other
  diagnostic. See the [render/audio notebook](subsystems/05-render-audio.md).
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
- Task 8 confirmed BUG-046, BUG-047, BUG-048, REL-005 through REL-008, IMP-014
  and IMP-015, and closed the platform side of REL-003, REL-004 and BUG-033 by
  extension. REL-008 has the most severe consequence *when reached* — on macOS
  every layout except a `.app` bundle falls through to the compile-time
  `DATA_DIR`, which is the build machine's source `share/`, so a `make install`
  binary run anywhere else reaches Task 6's reproduced BUG-034 through the
  failing `VerifyAssetDirectory` — but Fix Round 1 established that **no
  shipped artifact takes that path**: all three desktop release layouts hit a
  branch that resolves correctly, and REL-008 was reassessed from High to
  Medium accordingly. The findings that do reach shipped artifacts are
  BUG-046, whose `AssetExtractor.java:106` skip fires on the normal
  error-free Android upgrade path of a shipped APK (freezing a partial
  extraction permanently and never refreshing a changed asset), and BUG-048,
  which fires on every page load of the WASM build. **Task 9 corrected the CI
  clause that sentence originally carried.** It read "per `CLAUDE.md` the only
  platform CI currently ships, since the Linux, macOS, Windows and Android build
  jobs are disabled with `if: false`" — measured at the pinned baseline, **0** of
  **11** jobs carry `if: false`, the `release` job `needs` all five build jobs
  and attaches **5** files, and `CHANGELOG.md`'s `v2.4.27` entry records the
  restoration explicitly. BUG-048 still fires on every WASM page load, but WASM
  is one of five platforms CI builds and releases, so BUG-046, REL-007 and
  REL-012 reach shipped Android and macOS artifacts as well. The documentation
  drift itself is registered as REL-009. Guard selection itself is sound:
  every Android-only SDL entry point is guarded, the Android source list is
  set-equal to the native one at 28 files, and a full WASM link plus an
  `llvm-nm --extern-only` comparison proved the two network-client translation
  units share no `NetworkClient` definition. See the
  [platform notebook](subsystems/06-platform-ports.md).
- Task 9 confirmed REL-009 through REL-014 and IMP-016 through IMP-023, closed
  IMP-008 as a confirmed improvement, and resolved the packaging side of
  REL-004, REL-006, REL-007 and REL-008 by extension. The findings that reach a
  shipped artifact are REL-011 (the WASM build ships SDL3 3.4.2 and SDL3_image
  **3.2.4** where Linux, Windows and Android all ship 3.4.4/3.4.2, and macOS
  ships whatever Homebrew has), REL-012 (the DMG is single-architecture and
  unlabelled), REL-013's DLL loop (a missing load-time DLL still produces a
  green job, a signed-off installer, a GitHub release asset and an itch.io
  push), and REL-004's literal `versionCode 10`, which together with REL-007's
  per-run keystore gives two independent reasons a shipped APK cannot upgrade
  another. REL-010 is the operational one: reproduced on two OpenSSL
  implementations, `docker/setup.sh` discards a certbot ECDSA certificate and
  key and serves a self-signed pair instead. REL-009 is the one whose cost this
  audit itself paid — stale CI documentation led Task 8 to under-weight four
  shipped platforms. Every confirmed defect from Tasks 3-9 (68 IDs) is mapped in
  the notebook to an existing test, an audit dynamic case, or a registered gap;
  the dominant conclusion of that mapping is that **no CI job runs any test at
  all** (0 invocations across 11 jobs, 5 tests registered), which IMP-016
  addresses. See the
  [build/release/tooling notebook](subsystems/07-build-release-tooling.md).

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
  byte-identical after). Runs covered: image-format/pitch verification of
  eight shipped files — the seven that reach a pixel routine are 4 bpp
  tight-pitch ABGR8888, while `back_one_player.png` is RGB24 at 3 bpp and is
  blit-only (`highscoremanager.cpp:291`) — an ASan demonstration of the `get_pixel` clamp
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

### Task 7 Fix Round 1

- Independent review of commit `d9597304` raised five substantive and four
  minor findings. All nine were re-verified against production source at the
  cited lines and **accepted; none were disputed**.
- Factual corrections to claims this gate had made: BUG-001's leak quantity
  (`mainmenu.cpp:204/214/224/228` is an `if`/`else if` chain, so at most one
  `LoadEmptyAndApply` — 2 leaked rects — runs per `InitCandy`, not three calls
  and six rects); BUG-041's trigger set (only two `DoSnipIn` producers exist —
  `mainmenu.cpp:497` inside `SetupNewGame`, which *is* the game-start trigger
  and had been misattributed to menu return, and `bubblegame.cpp:1012` in
  `ReloadGame`; `QuitToTitle` at `bubblegame.cpp:1363` clears
  `firstRenderDone` with no `DoSnipIn`, so menu return produces no animation);
  the pixel-format claim (this gate's own `formats.log` lists
  `back_one_player.png` as RGB24 bpp=3, so seven of eight, not all eight, are
  4 bpp — the safety argument now rests on that file being blit-only at
  `highscoremanager.cpp:291`); the `hurryTexture` load-site count (17, not 20);
  and IMP-013's attribution (`set_pixel`, `shaderstuff.cpp:41-45`, has no
  clamp — the off-by-one clamps are `get_pixel`'s own and the call sites at
  `:488` and `:1155`, as the notebook's trust-boundary bullet already said).
- Two closures had hidden real defects and were corrected. IMP-005's render
  slice claimed no reachable use-before-initialization; `TTFText::coords`
  (`ttftext.h:57`, uninitialized, default constructor `ttftext.cpp:22-24`
  empty) is reachable through `BubbleGame::targetingText` (`bubblegame.h:535`),
  which has no `LoadFont` call anywhere, so `UpdateText` always returns at
  `ttftext.cpp:48` and `bubblegame_render.cpp:957` renders a null texture
  through an indeterminate rect — registered as **BUG-043**. The
  `mainmenu_panels.cpp` coverage row read "Complete … overlook surface/texture
  per-frame lifecycle correct" while `mainmenu.cpp:124` stores unchecked
  `IMG_Load` results that `mainmenu_panels.cpp:198` and `:213` dereference —
  registered as **BUG-044**, the same missing-asset crash class as BUG-001 and
  cross-linked to it rather than duplicated.
- Completeness items: the Step 1 ownership table's `TTFText` row is now
  exhaustive (`playerNameWinText[MAX_NET_PLAYERS]` itemized and
  `FrozenBubble::menuText` added; the instance total this round stated as 23
  was **wrong and is corrected to 38 fixed members in Fix Round 2 below**);
  `menuText` has zero references outside
  `frozenbubble.h:96` and is recorded as an IMP-012 extension rather than a new
  ID, since IMP-012 already covers unused menu-layer members; and BUG-001 now
  states that `shaderstuff.h:67` is the asset-reachable ordering while `:55`
  additionally requires an `SDL_CreateSurface` failure.
- Two harness subcommands were added to the existing Task 7 harness (`nofont`,
  `overlooknull`), linking the same unchanged production objects. `nofont`
  proved BUG-043 (`coords_w`/`coords_h` still `0xCDCDCDCD`, null texture,
  `SDL_RenderTexture` rejected); `overlooknull` proved BUG-044's `:213` half
  (UBSan null member access at `shaderstuff.cpp:1485`, then SEGV, exit 134).
  BUG-044's `:198` half rests on the static chain — its TU cannot be linked
  without full `MainMenu` construction.

### Task 7 Fix Round 2

- A second independent review of commit `f4d44bda` confirmed all five Fix
  Round 1 corrections but found that Fix Round 1 had **introduced one new false
  claim** and left **one dismissal incomplete**. Both were re-verified against
  production source; one was accepted in full, one was accepted as to the
  incompleteness but its proposed mechanism was disproved.
- **False claim, accepted and corrected.** The Step 1 ownership table wrote
  "`playerNameWinText[MAX_NET_PLAYERS]` = 5 more instances" and totalled 23,
  presenting the table as exhaustive. `src/bubblegame.h:251` declares
  `inline constexpr int MAX_NET_PLAYERS = 20`, so `bubblegame.h:534` declares
  **20** instances; the "3-5 player mode" comment on that line is stale and the
  declaration governs. Re-derived from the declarations, the fixed member total
  is **38**: `BubbleGame` 13 scalars + 20 array = 33, `MainMenu` 2
  (`mainmenu.h:123`, `:198`), `FrozenBubble::menuText` 1 (`frozenbubble.h:96`),
  `HighscoreManager` 2 (`highscoremanager.h:67`). The per-`levelsetScores`
  `layoutText` (`highscoremanager.cpp:35`, vector `:55`) is a **runtime-variable
  set of 0–10**, now stated separately instead of folded into the total.
- **Downstream effect of the corrected count.** For the array itself, none: the
  loader loop at `bubblegame.cpp:151` carries the same `MAX_NET_PLAYERS` bound,
  so all 20 are font-loaded and "exactly two fixed members are never
  font-loaded" (`targetingText`, `menuText`) still holds. For the per-levelset
  set, examining it separately exposed a defect — the rows *are* font-loaded by
  `CreateLevelImages` (`:313-315`), but the state is discarded by `TTFText`'s
  reset-to-empty copy constructor on `push_back`, registered as **BUG-045**
  (the promoted instance of IMP-007's copy semantics, cross-linked to IMP-007
  and to BUG-043's render-a-null-texture consequence rather than duplicating
  either).
- **Incomplete dismissal, corrected; proposed mechanism disproved.** The
  `idleSPButtons` dismissal did rest on an absence of crashes rather than a
  consequence trace, and now carries one. The proposed defect — that
  `SDL_GetTextureSize` leaves `fw`/`fh` uninitialized at
  `mainmenu_panels.cpp:208`, making `:210`'s `subRct` indeterminate like
  BUG-043 — is **wrong**: the function writes `*w = 0` and `*h = 0` *before*
  `CHECK_TEXTURE_MAGIC` returns `false`
  (`android/app/jni/SDL3/src/render/SDL_render.c:1921-1941`, the SDL 3.4.4
  submodule pinned in this tree), and the linked SDL 3.4.10 runtime was probed
  directly with `0xCDCDCDCD`-poisoned floats and read back `0x0p+0`. The
  documented header contract
  (`/opt/homebrew/include/SDL3/SDL_render.h:978-994`) promises only the boolean
  return, so the guarantee is stated as resting on the readable implementation
  of both versions. Consequence chain to its end: `w = h = 0` →
  `subRct = {171, y, 0, 0}` → `SDL_RenderTexture` at `:227` refuses the null
  texture → the idle button's label is silently missing. Not a defect; the
  policy gap stays under IMP-010 and BUG-044's proposed null check covers both
  loads in the `SP_OPT` loop.
- **Quantity sweep.** Every array-size, instance-count, site-count, and
  quantity claim in the four Task 7 documents was re-derived from the
  declarations rather than from comments or unexpanded greps. Two further
  errors were found and corrected: the `MainMenu` texture family ("~44" →
  **37**: 21 `IMG_LoadTexture` statements, of which two are loops —
  `SP_OPT` = 5 (`mainmenu.h:49`) and `netSpotSelf[13]` — giving 19 + 5 + 13)
  and `RenderRoundStats`'s churn ("~30 `cell()` calls" → **24 static call
  sites**, 7 in the per-player loop and 6 in the per-team loop, so at most
  `11 + 7·players + 6·teams` per frame). Re-derived and confirmed correct:
  `SP_OPT` = 5; `activeSPButtons` 4 occurrences; `hurryTexture` 17 load sites /
  18 occurrences in `bubblegame.cpp` / 21 tree-wide; `bubbleArrays` sized
  `MAX_NET_PLAYERS` = 20; 18 `LoadFont` call sites; Penguin frames
  71 + 97 + 68 + 158 = 394 (`bubblegame.h:55-58`); `BUBBLE_STYLES` = 8 so
  "4×8 bubble sets"; `imgBubbleStick[BUBBLE_STICKFC + 1]` = 8 entries with
  `BUBBLE_STICKFC` = 7; `pausePenguin[35]` with the `:1202-1203` wrap to 12 at
  34; 8 `MenuButton`s; the BubbleGame texture breakdown (3 shooters,
  2 compressor, 2 on-top, 4 + 4 attack, 5 left overlays, 2 dots, 2 + 2 panels,
  frozen/prelight ×4); `circle_steps` = `XRES*YRES*sizeof(int)` = 1.2 MB;
  `plasma`/`plasma2`/`plasma3` = 640×480 = 300 KB each; `points`/`flakes`
  `amount = 200`; `precalc_cos`/`precalc_sin` 200 doubles; `bars_effect`
  16 × 40 segments of `(640/16)*bpp`; `fillrect`'s `640/32`/`480/32` rejection;
  3 `LoadEmptyAndApply` sites in an `if`/`else if` chain. Analyzer record
  counts (229 cppcheck, 248 clang-tidy, 86/60/37/20 families) are tool-output
  derived, not declaration derived, and were left as recorded; their artifacts
  remain under `/tmp/fb-sdl3-audit/`.
- One new harness (`/tmp/fb-sdl3-audit/task7/task7_fix2_harness.cpp`,
  subcommands `getsize` and `ttfcopy`) was built warnings-strict against the
  unchanged production `ttftext.cpp.o` from `build-audit-werror`. It
  constructs no preference-owning singleton and opens no preference file, so
  the `CFFIXED_USER_HOME` gate does not apply; it reads only the read-only
  `share/gfx/DroidSans.ttf`.

## Task 8 closure provenance

- Every file named by the Task 8 brief received a final disposition: the five
  platform sources in full, the platform slices of `frozenbubble.cpp`/`.h` and
  `mainmenu_server.cpp` (which completes the two `frozenbubble` rows across
  Tasks 6, 7 and 8), all three WASM build files, all Android project files, and
  every web document. The 116 vendored rows the ledger had left in a bootstrap
  state — 97 SDL2 headers, 11 SDL Android Java files, 4 SDL gitlinks, 4
  duplicated iniparser files — were boundary-reviewed and closed. The ledger
  still holds exactly 237 rows and its path set is byte-identical to the
  pre-edit set, so equality with the pinned-tree filter is preserved by
  construction.
- **The Android build ran locally and succeeded.** `./gradlew clean
  assembleRelease --no-daemon` exited 0 in 1 m 30 s from a fresh shell using the
  persisted `android/local.properties`, NDK 25.2.9519653, and Homebrew OpenJDK
  17.0.19. It produced a 37,290,226-byte `app-release-unsigned.apk` with all
  three ABIs (`arm64-v8a`, `armeabi-v7a`, `x86_64`) and
  `versionCode 10` / `versionName 2.4.27`. The submodules were verified fully
  initialized beforehand (`git submodule status --recursive`, 0 uninitialized),
  so no submodule was created or mutated by this gate.
- **No tracked-file drift, and therefore no restoration.** `git status --short`
  printed nothing both before and after, `git diff --stat HEAD` was empty, and a
  SHA-256 manifest of the 33 regular tracked files under `android/` compared
  byte-identical. No `git clean`, no `git checkout .`, and no `git checkout --`
  of any path was run at any point. The build's own outputs
  (`android/app/build/`, `android/app/.cxx/`, `android/.gradle/`,
  `android/local.properties`) were already covered by `.gitignore:20-23`.
- **The WASM build linked completely, against a disposable Emscripten copy.**
  The Homebrew `emscripten 6.0.4-git` `libexec` tree was clone-copied to
  `/tmp/fb-sdl3-audit/task8/emsdk/libexec` and the CI port setup was replayed
  against the copy alone; the copy's own `.emscripten` resolves `CACHE` into the
  copy, so **the system installation was never modified** — only its read-only
  `llvm`/`binaryen` directories were reused. `emcmake cmake` and `emmake make
  -j8` both exited 0, producing all four artifacts. This supersedes Task 4's
  translation-unit-only result, which had stopped at the unpatched
  SDL3_image/SDL3_mixer port boundary.
- **No browser runtime was exercised**, because loading an Emscripten
  `--preload-file` bundle requires an HTTP origin and the audit's scope
  restriction forbids starting network listeners. WASM runtime status is
  recorded as **unavailable, not passed**; the WebSocket-proxy step of brief
  Step 5 was skipped by direction. No websockify, `fb-server`, listener, socket,
  or client connection was created anywhere in this gate.
- A test-only translation unit linked the unchanged warnings-strict production
  `platform.cpp.o` and `logger.cpp.o` and called the real `InitDataDir()` and
  `Logger::Initialize()` across five packaging layouts and three logger
  scenarios. Preference isolation was proved first (`ISOLATION=OK` under
  `CFFIXED_USER_HOME`), and the user's three real preference files were hashed
  beforehand and verified byte-identical afterwards.
- One dismissal was settled by execution rather than argument: the claim that
  `cmake/Emscripten.cmake` would leave `EMSCRIPTEN` unset and so break
  `WASM_PORT.md`'s documented configure was **disproved** — the command exited
  0 with `__WASM_PORT__`, `DATA_DIR="/share"`, the `.html` suffix and the SDL3
  port flags all applied and `server/` still skipped.
- No Android device or emulator was installed to or launched, no Linux or
  Windows host was executed on, no keystore or signing operation was created,
  no process was killed, and no security-specific runtime test was run.

### Task 8 Fix Round 1

An independent review of commit `6c859034` raised two Important and five Minor
findings, one of which called for a full count sweep. **All seven were
accepted; none was disputed**, and every one was re-verified against the real
files and the retained artifacts before being applied. Of the two quantitative
corrections below, the review itself directly re-derived the `syncfs`/
`localStorage` occurrence counts; the recursive submodule count (37 → 38) was
not one of the review's findings — it was found afterward by the implementer's
own follow-up count sweep (2026-07-29T02:11:10Z, row below), which the review's
fifth Minor finding had called for.

- **BUG-048's occurrence counts were wrong, and the evidence never measured
  them.** The gate stated a "single `syncfs` occurrence" and a "single
  `localStorage` occurrence". The recorded command reported only the *first
  occurrence index* of each token, so uniqueness was never measured — the same
  "count derived from evidence that does not measure the claim" class this gate
  claimed to have eliminated, and self-contradicted by the notebook's own "four
  `EM_ASM` sites" and "four `localStorage` `fb_nickname` sites". Re-measured
  with `grep -o … | wc -l` on the retained artifact: `syncfs` **4**,
  `localStorage` **4**, `IDBFS` **0**. The conclusion survives — all four
  `syncfs` occurrences lie inside `FS.syncfs`'s own definition (the method name,
  its in-flight warning string, and the `mount.type.syncfs` guard-and-dispatch
  pair), so no call site outside the definition exists; all four `localStorage`
  occurrences are `fb_nickname` `ASM_CONSTS` entries. The **characterization**
  is also corrected: the four sites are one `getItem` (`mainmenu.cpp:163`) and
  three `setItem` (`mainmenu.cpp:264`, `mainmenu_netpanel.cpp:80`,
  `mainmenu_input.cpp:1507`) — the three writes are what create the persistence,
  so describing the store as "the `fb_nickname` `EM_ASM` read" was backwards.
- **BUG-046 asserted a repair path its own cited code forecloses, and omitted
  the larger consequence.** The gate said the truncated file "survives until the
  app version changes or app data is cleared". `AssetExtractor.java:106` returns
  early for **any** destination that exists with non-zero length, so a version
  bump re-enters `extractDir`, skips the truncated file again, and then rewrites
  the marker at `:68-74` — **a version change does not repair it**. The same
  line means an app update never refreshes an asset whose content changed while
  keeping its path; only newly added paths are written. That second consequence
  is the larger one in practice (it needs no error at all) and is recorded by
  **extending BUG-046 rather than allocating a new ID**, because the root cause
  is the identical unconditional short-circuit at `:106` and a single fix —
  comparing against the asset, extracting to a temp path and renaming, or
  clearing `destDir` on marker mismatch — resolves both halves.
- **REL-008's severity now reads identically in both documents, and two
  verified mitigating facts were added.** The notebook had said "High for the
  macOS `make install` path; Medium overall" while the registry cell recorded a
  bare `High`. Verified and added: no shipped artifact is affected — the macOS
  release is a hand-built `.app` (`build.yml:155-163` copies `share` into
  `Contents/Resources/`, the harness's layout D, the one case
  `platform.cpp:109-121` handles), the AppImage installs to `AppDir/usr/bin`
  with assets at `usr/share/frozen-bubble` (`:75-81`, recovered by the
  `/proc/self/exe` heuristic), and the Windows package copies `share` beside the
  `.exe` (`:255-260`); and `default.nix:50` already works around the defect with
  `-DASSET_PATH="$out/share"`. **Reassessed to Medium in both documents**: `High`
  is reserved for "a shipped platform rendered unusable" and no shipped platform
  is affected, while a `make install` binary unusable off the build machine is
  squarely the `Medium` definition. The severity of the *consequence* on the
  affected path is unchanged and still stated.
- **The coverage header's bootstrap count was off by one.** It said 20 rows
  remained; 21 carry a pending disposition, the 21st being `CMakeLists.txt`
  ("Baseline exercised; static review pending"), excluded only because the
  header defined bootstrap as two literal strings. The header now counts every
  pending disposition and matches what a reader counting pending rows finds: 21,
  all Task 9. No row remains in the `boundary review pending` state.
- **Two brief Step 6 substitutions were added to the Limitations blocks.**
  Dynamic-library independence was never confirmed, and the shipped binary was
  never launched from a staged or bundle layout (a harness over
  `platform.cpp.o`/`logger.cpp.o` was used). Both were transparent where they
  appear in the Static review and Dynamic evidence sections but were missing
  from the Limitations enumerations; both are now recorded there.
- **"Only script tag" corrected.** `web/index.html` has two `<script>` tags: an
  inline `Module` setup block at `:89-135` and the external tag at `:136` that
  loads `frozen-bubble-sdl2.js`. The finding is otherwise unchanged.
- **Count sweep for the Important-1 error class.** Every occurrence, site, and
  instance count in the four Task 8 documents was re-derived with a command that
  actually measures the claim. Twenty quantities were re-derived; **eighteen
  reproduced exactly** — counted as one item each: (1) the nine guard-token
  counts (91/26/22/2/2/1/1/0/0), (2) the source lists (27 explicit + 1 = 28
  native / 29 Emscripten, Android 28 set-equal, Emscripten file 15 omitting
  exactly the 14 named), (3) 7 `iconv` matches, (4) 0 live `catch` handlers,
  (5) 0 `META-INF` v1 signature entries, (6) 3,352 preloaded files all under
  `/share/` against 3,352 on disk, (7) the APK's 37,290,226 bytes and 13 `.so`
  per ABI and 819,904 + 1,846,824 = 2,666,728 redundant bytes, (8) the 62/30/8
  `llvm-nm` intersection with 0 `NetworkClient::` symbols in both, (9) 29
  compiled objects, (10) 16 warnings in 5 families, (11) `TOTAL_MEMORY` ×4 vs
  `INITIAL_MEMORY` ×1 on the generated link line, (12) 97/97 symlinks, (13) 11
  SDL Java files, (14) 4 iniparser files, (15) 4 gitlinks, (16) 134 tracked
  `android/` paths of which 33 are regular files, (17) the FILE_COVERAGE.md
  inventory-equality check as a single re-derivation (116 vendored coverage
  rows, 147 coverage rows updated, 237 total rows, 78 unique IDs), and (18) the
  single baked `DATA_DIR` literal. **Two were wrong**: the
  `syncfs`/`localStorage` counts above, and the recursive submodule count,
  recorded as 37 and re-derived as **38** — the undercount is `plutovg`, which
  appears at two paths (`SDL3_ttf/external/plutovg` and
  `SDL3_ttf/external/plutosvg/plutovg`) at the same commit `3e6f922f`. Still 0
  uninitialized, so no conclusion changes. One further imprecision was tightened:
  the Android-SDL-entry-point row recorded "11 hits" from a `grep -rn` line
  count; those 11 lines are 9 call sites plus 2 comment lines (12 name
  occurrences), and all 9 call sites are inside `#ifdef __ANDROID__` as claimed.

## Task 9 closure provenance

- Every file named by the Task 9 brief received a final disposition, and all
  **21** rows that still carried a pending disposition after Task 8 were
  dispositioned in this gate. The coverage ledger remains **237** rows, exactly
  equal to the pinned-tree filter of `09d6c7bf`, with **0** pending.
- The gate is a static and configuration-level review. **No GitHub Actions
  workflow was executed, triggered, or dispatched; no container was started; no
  release, artifact, itch.io channel, dependency download, or external host was
  touched; no listener, socket, or server process was created.** Every CI
  conclusion is a reading of `.github/workflows/build.yml` against documented
  Actions semantics, and every dependency version was read from a file rather
  than confirmed upstream.
- Four of the brief's five Step 4 validators ran to a clean exit; two of those
  needed a substitution, both recorded with the exact command and its exit.
  `ruby … YAML.load_file(…, aliases: true)` exited **1** because macOS ships
  Ruby 2.6.10, whose Psych predates the `aliases:` keyword; the same call
  without the keyword exited **0**, and the document contains no anchors.
  `./android/gradlew tasks --all --no-daemon` exited **1** because the wrapper
  takes its project directory from the CWD and the repository root has no
  `settings.gradle`; `--project-dir android` exited **0** and listed 329 task
  lines. `docker compose -f docker/docker-compose.yml config` exited **0** as a
  local parse — the daemon was not required, and `docker compose up` was
  deliberately not run. `cmake -S . -B build-audit-config -G Ninja
  -DCMAKE_BUILD_TYPE=Release` exited **0** into a directory added to the
  untracked `.git/info/exclude` *before* creation. `python3 -m py_compile` over
  `tools/net_bots.py`, `tests/*.py` and `tools/server_tests/*.py` exited **0**,
  as did a separate run over the two Emscripten port files.
- Gradle drift control: `git status --short` (0 lines), a SHA-256 manifest of
  all **134** tracked `android/` paths, and the SHA-256 of `git ls-files -s
  android` were captured before the first Gradle invocation and re-derived after
  the last. Both diffs were empty and the index hash was identical
  (`e7f56a3342a1e48c27c88386e7fa8763f509d14eafdc4f5adc83c57f25ed3b74`). **No
  path was restored, because none was modified.**
- One finding was reproduced rather than argued: REL-010's `key_ok` predicate.
  `openssl rsa -in <prime256v1 key> -check -noout` exited **1** under the system
  LibreSSL 3.3 and again under Homebrew OpenSSL 3.6.3. The destructive
  `openssl req -x509` branch that follows it was **not** executed against any
  certificate pair, so the overwrite is a code-supported consequence of a
  reproduced predicate, not an observed deletion.
- One candidate was disproved by running the command it doubted:
  `cmake_uninstall.cmake.in`'s `exec_program` is deprecated, not removed —
  `cmake -P` on a minimal `exec_program` script exited **0** on CMake 4.3.4 with
  only a `CMP0153` developer warning.
- A count sweep re-derived every quantity this gate states with a command that
  measures that claim, not a first-occurrence index or a line count standing in
  for an occurrence count. Twenty-four quantities were re-derived; **twenty-three
  reproduced exactly and one was wrong**: (1) 11 jobs / 0 `if: false` / 5
  `release_files` / 5 `release_needs`, (2) 27 `uses:`, (3) 0 SHA-pinned, (4) 5
  branch-pinned, (5) 5 of those being the butler action, (6) 5
  `BUTLER_CREDENTIALS` references, (7) **wrong** — 20 DLL names in the copy
  loop, corrected to **21** in Fix Round 1 (the sweep command undercounted; see
  below), (8) 3
  `|| true`, (9) 1 `permissions:` block, (10) 0 test invocations, (11) 1
  workflow file, (12) 0 architecture settings in either file, (13) 0
  `versionCode`/`versionName` occurrences in the workflow, (14) 0 `VERSIONINFO`
  in `share/icons/fb.rc`, (15) 4 vendored iniparser files, (16) 1 licence file
  in the repository, (17) 0 tracked `dist-wasm/` paths, (18) the source-set
  parity 28 native / 29 Emscripten / 28 Android set-equal / 15 Emscripten-file
  omitting 14, and 7 server sources equal to the 7 on disk, (19) the WASM port
  versions 3.4.2 / 3.2.2 / 3.2.4 / 3.2.0 read from the emsdk copy that linked
  the artifact, (20) the CI-pinned 3.4.4 / 3.4.2 / 3.2.0 / 3.2.2, (21) 134
  tracked `android/` files, (22) the unchanged android index hash, (23) 237
  coverage rows with 0 pending, and (24) 92 unique, per-class contiguous
  registry IDs.
- Per the user's scope restriction no security-specific runtime test was run.
  The `@master` action pin holding a deploy secret, the repo-visible keystore
  password, the unverified MinGW downloads, the absent commit pins, and the
  mutable base-image tags are documented statically from the workflow text
  alone; the omitted supply-chain and credential-exposure checks are
  limitations, not passes.

### Task 9 Fix Round 1 Findings

An independent review of commit `efc5ba3b` raised two Important and two Minor
findings. **All four were accepted; none was disputed**, and each was
re-verified against the pinned baseline before being applied.

- **REL-013's DLL count was wrong, and the error is explained rather than
  silently swapped.** The gate stated **20** named DLLs in the Windows
  packaging loop; the true count is **21**. The recorded sweep command, `sed -n
  '261,268p' .github/workflows/build.yml | tr -s ' \\' '\n\n' | grep -c
  '\.dll$'`, undercounts because the loop's last entry on line 267,
  `libpcre2-8-0.dll;`, has its trailing semicolon glued directly to the
  filename with no space or backslash between them for `tr -s ' \\'` to split
  on — the resulting token `libpcre2-8-0.dll;` fails the `\.dll$`-anchored
  `grep`. A regex extraction (`grep -oE '[A-Za-z0-9_.+-]+\.dll'`) over the same
  lines finds all 21. The `:261-269` line range the gate cited for the loop
  (`for dll in \` at `:261` through `done` at `:269`) was already correct and
  is unchanged; only the count was wrong. REL-013's substance — the copy loop
  cannot fail — is unaffected. The **24-quantity count-sweep claim is
  corrected** from "all twenty-four reproduced exactly" to twenty-three
  reproduced exactly and one (the DLL count) wrong, now fixed.
- **REL-007's citation range was incomplete.** The gate cited
  `.github/workflows/build.yml:390-398` for "a literal password appearing four
  times", but the four occurrences are at `:396` and `:397` (inside "Generate
  release keystore", `:390-398`) and `:404` and `:406` (inside the *next*
  step's `env:` block, "Build release APK", `:400-413`) — two of the four sit
  outside the cited range. The aggregate count of four was already correct; the
  citation now names both steps and all four line numbers so each instance is
  verifiable in place.
- **FILE_COVERAGE.md's inventory rule gestured at a location that does not
  exist.** Its closing sentence claimed the word `pending` "now appears in this
  file only in this rule and in prose describing not-yet-merged upstream
  work". A full-file case-insensitive search finds `pending` nowhere else in
  the file — the second location was never real. Corrected to state plainly
  that the rule paragraph is the only place the word appears. The 237-row /
  0-pending count itself did not change.
- **The "ten headings, each once" phrasing invited a wrong reading.** That
  invariant (`Scope`, `Trust boundaries and invariants`, `Static review`,
  `Dynamic evidence`, `Candidates`, `Confirmed findings`, `Dismissed
  candidates`, `Coverage`, `Limitations`, `Gate conclusion`, in order, each
  exactly once) applies to the **nine subsystem notebooks** under
  `docs/audit/subsystems/`, `07-build-release-tooling.md` included — not to
  this file. This file's own `##` heading structure is different by design: it
  recurs a `## Task N closure provenance` heading once per task (Tasks 3
  through 9) plus `Audit baseline`, `Session environment`, `Current state`,
  `Gate checklist`, `Active candidates`, `Confirmed findings`, `Commands and
  evidence`, `Limitations`, and `Processes and cleanup` — a literal `grep -c
  '^## '` count of **16**, not 10, and that is correct for this file; it was
  never subject to the ten-heading invariant.
- **Re-run count sweep for the Important-1 error class.** Every Task 9 count
  whose original command relied on whitespace splitting, `tr`, a line count
  standing in for an occurrence count, or a first-occurrence index was
  re-derived directly against the pinned-baseline workflow file (which carries
  no drift from `09d6c7bf`): `uses:` **27**, commit-pinned **0**, branch-pinned
  **5**, butler `@master` **5**, `BUTLER_CREDENTIALS` **5**, `|| true` **3**,
  `permissions:` **1**, test-invocation tokens **0**, workflow files **1**,
  architecture settings **0** in both files, `versionCode`/`versionName` in the
  workflow **0**, `VERSIONINFO` **0**, vendored iniparser files **4**, licence
  files **1**, tracked `dist-wasm/` paths **0**, tracked `android/` paths
  **134**, and server sources **7** declared / **7** on disk — all
  re-derived unchanged, each independently confirmed with `grep -o` (occurrence
  count) equal to `grep -c` (line count) wherever a line could in principle
  hold more than one match. **Only the DLL count reproduced differently** (21,
  not 20), because it was the only one of these built on a `tr`-based
  whitespace split rather than a direct pattern match. The ruby one-liner
  (`jobs=11 if_false=0 release_files=5 release_needs=5`) was independently
  re-run in Ruby with the same result.

Full findings-registry text for REL-007 and REL-013 is corrected in
[FINDINGS.md](FINDINGS.md) and in the notebook's own
[Fix Round 1 addendum](subsystems/07-build-release-tooling.md#gate-conclusion).

## Commands and evidence

Each row records exactly one top-level shell command. A shell loop remains one
syntactic command, but unnamed multi-command “gate” rows are not used. Exit
values and material output are from captured Task 1, fix-round, and Task 2
through Task 8 evidence. Tasks 6, 7 and 8 read their scoped files with the
agent's file reader rather than `nl`/`sed`, so only their shell commands appear
below; the files they read are listed in the
[Task 6 notebook scope](subsystems/04-lobby-settings-input.md#scope) and the
[Task 8 notebook scope](subsystems/06-platform-ports.md#scope).

**Canonical log cutoff:** completed Task 9's build-definition parity
measurements, Step 4 validators and their two recorded substitutions, the
REL-010 predicate reproduction, the `exec_program` disproof, the Gradle drift
manifests, and the 24-quantity count sweep — plus everything through Task 8's
Fix Round 2. Task 9's final validation, staging, commit, and post-commit checks
belong in its ignored controller report, preventing a false claim that a commit
records itself. The same exclusion already covers Task 2, the Task 5 fix round,
Task 6, Task 7, and Task 8 including both of its fix rounds.

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
| 2026-07-28 (Task 7 Step 1; exact time not captured) | <code>grep -n 'hurryTexture&#92;&#124;DestroyTexture' src/bubblegame.cpp</code> | 0 | 17 `hurryTexture` load sites and zero matching destroy sites (BUG-042 basis); the earlier "20" miscounted — corrected in Fix Round 1, see the recount row below | [BUG-042](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Step 1; exact time not captured) | <code>grep -n 'LoadPenguin' src/*.cpp</code> | 0 | Every `NewGame` player-count case calls `LoadPenguin` per player; no destroy path exists for the 394-texture set | [BUG-042](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28T14:27:35Z | <code>grep -rn 'transitionTexture' src/</code> | 0 | Only the null initializer and the by-value `effect()` argument exist; the member is never assigned (BUG-041 basis) | [BUG-041](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28T14:27:35Z | <code>for fn in draw_line_ blacken_ alphaize_ pixelize_ rotate_nearest_ rotate_bicubic_ autopseudocrop store_effect copy_line shrink_; do printf '%s: ' $fn; grep -rl "$fn" src/ --exclude=shaderstuff.cpp --exclude=shaderstuff.h &#124; tr '&#92;n' ' '; echo; done</code> | 0 | Seven effect helpers have no external caller (dead code, IMP-009); `shrink_`'s only caller is `highscoremanager.cpp` | [Dismissed candidates](subsystems/05-render-audio.md#dismissed-candidates) |
| 2026-07-28 (Task 7 triage; exact time not captured) | <code>grep -E '^src/(shaderstuff&#124;transitionmanager&#124;ttftext&#124;audiomixer&#124;sdl3_compat&#124;bubblegame_render&#124;mainmenu_panels&#124;frozenbubble)' /tmp/fb-sdl3-audit/cppcheck-project-unique.txt</code> | 0 | 229 scoped cppcheck records; promoted only the BUG-001/IMP-010/IMP-007/IMP-005 instances | [Analyzer triage](subsystems/05-render-audio.md#static-review) |
| 2026-07-28 (Task 7 triage; exact time not captured) | <code>grep -E '^src/(shaderstuff&#124;transitionmanager&#124;ttftext&#124;audiomixer&#124;sdl3_compat&#124;bubblegame_render&#124;mainmenu_panels&#124;frozenbubble)' /tmp/fb-sdl3-audit/clang-tidy-project-unique.txt &#124; awk -F'[][]' '{print $2}' &#124; sort &#124; uniq -c &#124; sort -rn</code> | 0 | 248 scoped clang-tidy records across 16 check IDs, led by 86 narrowing and 60 implicit-widening | [Analyzer triage](subsystems/05-render-audio.md#static-review) |
| 2026-07-28 (Task 7 harness; exact time not captured) | <code>/usr/bin/c++ -I/opt/homebrew/include -I…/src -I…/third_party/iniparser -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src/shaderstuff.cpp.o …transitionmanager… …audiomixer… …ttftext… …gamesettings… …platform… build-audit-werror/libiniparser-static.a -Wl,-rpath,/opt/homebrew/lib …SDL3/SDL3_image/SDL3_mixer/SDL3_ttf dylibs… -o /tmp/fb-sdl3-audit/task7/task7_harness</code> | 0 | Warnings-strict harness linked six unchanged production objects with no diagnostic | `/tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp` |
| 2026-07-28 (Task 7 harness; exact time not captured) | <code>/usr/bin/c++ … -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined … build-audit-sanitize object set … -o /tmp/fb-sdl3-audit/task7/task7_harness_sanitize</code> | 0 | ASan+UBSan harness linked the sanitized production objects | `/tmp/fb-sdl3-audit/task7/task7_harness_sanitize` |
| 2026-07-28 (Task 7 isolation; exact time not captured) | <code>shasum -a 256 "/Users/dchau/Library/Application Support/frozen-bubble/settings.ini" "…/highscores" "…/highlevelshistory" &#124; tee /tmp/fb-sdl3-audit/task7/real-prefs-baseline.txt</code> | 0 | Recorded pre-work hashes of the user's three real preference files | `/tmp/fb-sdl3-audit/task7/real-prefs-baseline.txt` |
| 2026-07-28 (Task 7 isolation; exact time not captured) | <code>mkdir -p /tmp/fb-sdl3-audit/task7/home7 &amp;&amp; env HOME=/tmp/fb-sdl3-audit/task7/home7 CFFIXED_USER_HOME=/tmp/fb-sdl3-audit/task7/home7 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness probe /tmp/fb-sdl3-audit/task7/home7</code> | 0 | `ISOLATION=OK`: pref path resolved inside the temporary home before any preference-owning singleton existed | [Task 7 dynamic evidence](subsystems/05-render-audio.md#dynamic-evidence) |
| 2026-07-28 (Task 7 Step 2; exact time not captured) | <code>SDL_VIDEODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness formats "$PWD/share"</code> | 0 | Seven of the eight probed files are 4 bpp tight-pitch ABGR8888 — every input that reaches a pixel routine; the eighth, `back_one_player.png`, is `SDL_PIXELFORMAT_RGB24` bpp=3 and is blit-only. `fblogo-mask.png` has zero white border pixels. (The original row said "all eight", contradicting its own log; corrected in Fix Round 1.) | `/tmp/fb-sdl3-audit/task7/formats.log` |
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
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -n 'LoadEmptyAndApply&#92;&#124;candyMethod ==' src/mainmenu.cpp</code> | 0 | Printed 8 lines: `candyMethod` tests at 196/204/214/224/228 and `LoadEmptyAndApply` calls at 208/218/231. Lines 204/214/224/228 are `if` / `else if` / `else if` / `else if`, so at most one of the three calls executes per `InitCandy` → 2 leaked rects, not 6 (BUG-001 quantity correction) | [BUG-001](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -rn 'DoSnipIn&#92;&#124;TakeSnipOut' src/</code> | 0 | 7 hits. Producers: `mainmenu.cpp:497` (inside `SetupNewGame`) and `bubblegame.cpp:1012` (`ReloadGame`). Consumer: `bubblegame_render.cpp:1173`. Remaining hits are the declarations/definitions in `transitionmanager.{h,cpp}`. Exactly two triggers — menu return is not one (BUG-041 correction) | [BUG-041](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -rn 'firstRenderDone' src/</code> | 0 | 5 hits: set false at `bubblegame.cpp:1013` (after `ReloadGame`'s `DoSnipIn`) and `bubblegame.cpp:1363` (`QuitToTitle`, with **no** `DoSnipIn`); consumed at `bubblegame_render.cpp:1172-1174`; declared `bubblegame.h:479`. Confirms menu return produces no transition animation | [BUG-041](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -rn 'SetupNewGame' src/</code> | 0 | 9 hits; every game-start path (`mainmenu.cpp:285`, `mainmenu_panels.cpp:238/278/407/436`, and the network start at `mainmenu_netpanel.cpp:163`) funnels through `MainMenu::SetupNewGame`, whose first statement is the `DoSnipIn` at line 497 — so line 497 is the game-start trigger, not a menu-return trigger | [BUG-041](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -rn 'targetingText' src/</code> | 0 | Exactly 4 hits: declaration `bubblegame.h:535` and uses `bubblegame_render.cpp:946` (`UpdateText`), `:956` (`UpdatePosition`), `:957` (`Coords()`/`Texture()` render). **No `LoadFont` call** (BUG-043 basis) | [BUG-043](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -rn 'LoadFont(' src/ &#124; grep -v ttftext</code> | 0 | 18 `LoadFont` call sites across `bubblegame.cpp` (13, incl. the `playerNameWinText[i]` loop at :153), `mainmenu.cpp` (2), `highscoremanager.cpp` (3). Matched against the `TTFText` member inventory, exactly two fixed members are never loaded: `targetingText` (BUG-043) and `FrozenBubble::menuText` (IMP-012). **Fix Round 2:** this row originally said "the 23 `TTFText` instances"; the correct fixed-member total is 38, and the two-never-loaded conclusion is unchanged | [Ownership table](subsystems/05-render-audio.md#static-review) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -n 'TTFText' src/bubblegame.h src/mainmenu.h src/highscoremanager.h src/frozenbubble.h</code> | 0 | Member inventory for the Step 1 table: `bubblegame.h:532-545` = 13 scalars + `playerNameWinText[MAX_NET_PLAYERS]`; `mainmenu.h:123,198` = 2; `highscoremanager.h:67` = 2; `frozenbubble.h:96` = `menuText`. **Superseded by Fix Round 2:** this grep does not expand `MAX_NET_PLAYERS`, and the "23 instances total" concluded from it was wrong — `bubblegame.h:251` fixes the bound at 20, giving 38 fixed members plus a runtime-variable per-`levelsetScores` set. See the Fix Round 2 rows below | [Ownership table](subsystems/05-render-audio.md#static-review) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -rn 'menuText' src/</code> | 0 | Exactly one line — `src/frozenbubble.h:96: TTFText menuText;`. Zero references outside the declaration: unused member, recorded as an IMP-012 extension | [IMP-012](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -rn 'activeSPButtons' src/</code> | 0 | Exactly 4 lines: `mainmenu.h:128` (decl), `mainmenu.cpp:124` (`IMG_Load`, result never checked), `mainmenu_panels.cpp:198` (`activeSPButtons[0]->w`), `:213` (passed into `overlook_`). No null check exists anywhere on the path (BUG-044 basis) | [BUG-044](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -rn 'backgroundSfc' src/</code> | 0 | Exactly 4 lines: `highscoremanager.h:63` (decl), `.cpp:189` (`IMG_Load` of `back_one_player.png`), `:290` (a log), `:291` (`SDL_BlitSurface` into the ARGB8888 `bigOne`). The RGB24 surface is only format-converting-blitted, never indexed — the corrected pixel-format safety argument | [Trust boundaries](subsystems/05-render-audio.md#trust-boundaries-and-invariants) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -c 'hurryTexture' src/bubblegame.cpp &amp;&amp; grep -c 'hurryTexture = IMG_LoadTexture' src/bubblegame.cpp</code> | 0 | Printed `18` then `17`: 18 occurrences, one of which is the comment at line 304, leaving **17** load sites (441…849). Corrects the earlier "20 load sites" row | [BUG-042](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>grep -n 'bytes_per_pixel' src/shaderstuff.cpp</code> | 0 | 39 hits. Only `:620`, `:1206`, `:1212`, `:1460`, `:1485`, `:1490` test `!= 4` and `abort()`; the other samplers only reject `== 1`, so 2/3-bpp inputs would silently misindex there. Also fixes the `set_pixel`/`get_pixel` clamp attribution: `set_pixel` (`:41-45`) has no clamp; the off-by-one clamps are `get_pixel`'s (`:49`) and the call sites `:488`/`:1155` | [IMP-013](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28 (Task 7 Fix Round 1 harness; exact time not captured) | <code>/usr/bin/c++ -I/opt/homebrew/include -I"$PWD/src" -I"$PWD/third_party/iniparser" -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src/{shaderstuff,transitionmanager,audiomixer,ttftext,gamesettings,platform}.cpp.o build-audit-werror/libiniparser-static.a -Wl,-rpath,/opt/homebrew/lib -L/opt/homebrew/lib -lSDL3 -lSDL3_image -lSDL3_mixer -lSDL3_ttf -o /tmp/fb-sdl3-audit/task7/task7_harness</code> | 0 | Rebuilt the warnings-strict harness with the two new subcommands (`nofont`, `overlooknull`) against the same unchanged production objects; no diagnostic | `/tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp` |
| 2026-07-28 (Task 7 Fix Round 1 harness; exact time not captured) | <code>/usr/bin/c++ … -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined /tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp build-audit-sanitize/CMakeFiles/frozen-bubble-sdl3.dir/src/{shaderstuff,transitionmanager,audiomixer,ttftext,gamesettings,platform}.cpp.o build-audit-sanitize/libiniparser-static.a … -o /tmp/fb-sdl3-audit/task7/task7_harness_sanitize</code> | 0 | Rebuilt the ASan+UBSan harness against the sanitized production objects | `/tmp/fb-sdl3-audit/task7/task7_harness_sanitize` |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>SDL_VIDEODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness nofont</code> | 0 | Printed `texture=0x0 coords_x=120 coords_y=300 coords_w=-842150451 coords_h=-842150451` then `render_texture_ok=0 err=Parameter 'texture' is invalid`. `-842150451` == `0xCDCDCDCD` poison: production `UpdateText` never wrote `coords.w/h` and never created a texture — BUG-043 reproduced | `/tmp/fb-sdl3-audit/task7/nofont.log` |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>SDL_VIDEODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness_sanitize nofont</code> | 0 | Identical output under ASan+UBSan with no sanitizer diagnostic (the indeterminate read is not MSan-detectable on this host; the poison value carries the proof) | `/tmp/fb-sdl3-audit/task7/nofont-sanitize.log` |
| 2026-07-28 (Task 7 Fix Round 1; exact time not captured) | <code>SDL_VIDEODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_harness_sanitize overlooknull</code> | 134 | Production `overlook_(dest, nullptr, 0, 149)` produced `src/shaderstuff.cpp:1485:41: runtime error: member access within null pointer of type 'SDL_Surface'`, then `AddressSanitizer: SEGV on unknown address 0x000000000004` with frame `#0 overlook_ shaderstuff.cpp:1485` — BUG-044's `mainmenu_panels.cpp:213` half reproduced | `/tmp/fb-sdl3-audit/task7/overlooknull-sanitize.log` |
| 2026-07-28T15:44:15Z | <code>grep -n 'MAX_NET_PLAYERS = ' src/bubblegame.h</code> | 0 | Printed `251:inline constexpr int MAX_NET_PLAYERS = 20;`. The declaration governs; `bubblegame.h:534`'s "3-5 player mode" comment is stale. `playerNameWinText[MAX_NET_PLAYERS]` is therefore **20** instances, not 5, and the exhaustive fixed-member `TTFText` total is **38** (13 + 20 + 2 + 1 + 2), correcting Fix Round 1's 23 | [Ownership table](subsystems/05-render-audio.md#static-review) |
| 2026-07-28T15:44:15Z | <code>grep -c 'IMG_LoadTexture' src/mainmenu.cpp</code> | 0 | Printed `21`. Two of the 21 statements sit in loops (`:123` over `SP_OPT` = 5, `:156` over 13 `netSpotSelf` frames), so the `MainMenu` texture family is 19 + 5 + 13 = **37**, correcting the "~44" in the Step 1 table | [Ownership table](subsystems/05-render-audio.md#static-review) |
| 2026-07-28T15:44:15Z | <code>sed -n '1921,1941p' android/app/jni/SDL3/src/render/SDL_render.c</code> | 0 | `SDL_GetTextureSize` writes `*w = 0` and `*h = 0` **before** `CHECK_TEXTURE_MAGIC(texture, false)`, so a null texture leaves deterministic zeros, not indeterminate values. Counter-evidence disproving the proposed indeterminate-rect defect at `mainmenu_panels.cpp:208-210`; the dismissal stands, now with a full consequence trace | [Dismissed candidates](subsystems/05-render-audio.md#dismissed-candidates) |
| 2026-07-28T15:44:22Z | <code>grep -rn 'CreateLevelImages()' src/</code> | 0 | Exactly two call sites — `highscoremanager.cpp:152` (`AppendToLevels`) and `:230` (constructor) — plus the definition at `:277` and the declaration at `highscoremanager.h:61`. `CheckAndAddScore` calls neither, and `BubbleGame::SubmitScore` calls `AppendToLevels` (`bubblegame_state.cpp:418`) **before** `CheckAndAddScore` (`:422`), so no refresh follows the mutation — BUG-045 basis | [BUG-045](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28T15:44:22Z | <code>grep -n 'RefreshTextStatus&#124;push_back(newEntry)&#124;std::sort' src/highscoremanager.cpp</code> | 0 | `:171` refreshes `newEntry`, `:172` copies it into the vector, `:175` sorts. The copy at `:172` runs `TTFText`'s reset-to-empty copy constructor (`ttftext.h:52`) and the sort's assignments are the no-op at `ttftext.h:53`, so the row stored is textless — BUG-045 basis | [BUG-045](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-28T15:44:22Z | <code>awk '/void BubbleGame::RenderRoundStats/,/^}$/' src/bubblegame_render.cpp &#124; grep -c 'cell('</code> | 0 | Printed `24`: 24 static `cell()` call sites (8 header, 7 in the per-player loop, 6 in the per-team loop, 3 standalone), so per-frame invocations are at most `11 + 7·players + 6·teams` — correcting the "~30 `cell()` calls" churn note | [Step 3](subsystems/05-render-audio.md#static-review) |
| 2026-07-28T15:45:03Z | <code>/usr/bin/c++ -I/opt/homebrew/include -I"$PWD/src" -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror /tmp/fb-sdl3-audit/task7/task7_fix2_harness.cpp build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src/ttftext.cpp.o -Wl,-rpath,/opt/homebrew/lib -L/opt/homebrew/lib -lSDL3 -lSDL3_image -lSDL3_mixer -lSDL3_ttf -o /tmp/fb-sdl3-audit/task7/task7_fix2_harness</code> | 0 | Built the fix-round-2 harness warnings-strict against the unchanged production `ttftext.cpp` object; no diagnostic. It constructs no `GameSettings`/`FrozenBubble` singleton and opens no preference file | `/tmp/fb-sdl3-audit/task7/task7_fix2_harness.cpp` |
| 2026-07-28T15:45:04Z | <code>SDL_VIDEODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_fix2_harness getsize</code> | 0 | Against the linked SDL 3.4.10 runtime: `pre fw=-0x1.9b9b9ap+28 fh=-0x1.9b9b9ap+28` (0xCDCDCDCD poison), then `ret=0 err='Parameter 'texture' is invalid'` and `post fw=0x0p+0 fh=0x0p+0 int_w=0 int_h=0`, `getsize_outputs_written=1`. Confirms the 3.4.4 source reading on the version the native build actually links | `/tmp/fb-sdl3-audit/task7/fix2-getsize.log` |
| 2026-07-28T15:45:04Z | <code>SDL_VIDEODRIVER=dummy /tmp/fb-sdl3-audit/task7/task7_fix2_harness ttfcopy "$PWD/share/gfx/DroidSans.ttf"</code> | 0 | Production `TTFText` loaded and rendered (`src tex=0x9b5048c00 coords={40,50,47,57}`); `push_back` into a `std::vector` of an aggregate mirroring `HighscoreData` yielded `copy tex=0x0 coords={0,0,0,0} level=7 name=audit`, and the production render call returned `render_copy_ok=0 err='Parameter 'texture' is invalid' rect={0,0,0,0}`; copy assignment left the destination null (`assign_dst_tex=0x0 assign_src_tex=0x9b5048f00`). BUG-045 reproduced | `/tmp/fb-sdl3-audit/task7/fix2-ttfcopy.log` |
| 2026-07-29T01:09:00Z | <code>git status --short</code> | 0 | No output; clean worktree at `abdaac56` before any Task 8 work | Task 8 closure provenance above |
| 2026-07-29 (Task 8 Step 2; exact time not captured) | <code>grep -rho "&#92;bTOKEN&#92;b" src/ &#124; wc -l</code> (run once per token) | 0 | Verbatim guard-token counts under `src/`: `__WASM_PORT__` 91, `__ANDROID__` 26, `_WIN32` 22, `__linux__` 2, `__MINGW32__` 2, `__ANDROID_PORT__` 1, `__APPLE__` 1, `__EMSCRIPTEN__` 0, bare `WIN32` 0 | [Guard inventory](subsystems/06-platform-ports.md#static-review) |
| 2026-07-29 (Task 8 Step 2; exact time not captured) | <code>grep -rn 'SDL_GetAndroid&#92;&#124;SDL_SendAndroidMessage&#92;&#124;SDL_IsAndroidTV&#92;&#124;SDL_ShowAndroid' src/</code> | 0 | 11 matching **lines** (12 name occurrences, since the comment at `networkclient.cpp:1612` names two): **9 call sites** plus 2 comment lines (`networkclient.cpp:1612`, `mainmenu_panels.cpp:390`). All 9 call sites (`mainmenu.cpp:669`, `mainmenu_netpanel.cpp:99`, `mainmenu_input.cpp:484`/`:1529`, `platform.cpp:58`/`:59`/`:79`, `networkclient.cpp:1614`/`:1615`) sit inside `#ifdef __ANDROID__`, so no Android-only symbol is reachable from a desktop or WASM TU | [Guard inventory](subsystems/06-platform-ports.md#static-review) |
| 2026-07-29 (Task 8 Step 2; exact time not captured) | <code>python3 -c '…extract src/*.cpp from each add_executable/add_library block…'</code> | 0 | Source-list parity: root `CMakeLists.txt` 27 explicit + `${NETWORK_CLIENT_SRC}` = 28 native / 29 Emscripten; `android/app/CMakeLists.txt` **28, set-equal** to the native effective set (zero additions, zero omissions); `CMakeListsEmscripten.txt` **15**, omitting 14 and adding `networkclient_wasm.cpp` | [Source-list parity](subsystems/06-platform-ports.md#static-review) |
| 2026-07-29 (Task 8 Step 2; exact time not captured) | <code>grep -rn 'catch *(' src/</code> | 1 | No output: **zero** live `catch` handlers in the whole tree. Disproves the proposed `-sDISABLE_EXCEPTION_CATCHING=0`-is-link-only defect — there is no handler to miscompile | [Dismissed candidates](subsystems/06-platform-ports.md#dismissed-candidates) |
| 2026-07-29 (Task 8 Step 3; exact time not captured) | <code>grep -rn 'iconv' src/</code> | 0 | 7 hits: `shaderstuff.h:28`'s include and six inside the block comment at `shaderstuff.cpp:1332-1360`. No live iconv use exists, so the include and the `_WIN32` `bzero` macro are dead portability weight (IMP-015) | [IMP-015](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29 (Task 8 Step 3; exact time not captured) | <code>git ls-files -s android/app/jni/include/ &#124; awk '$1=="120000"' &#124; wc -l</code> | 0 | Printed `97` against 97 total tracked entries there — **every** entry is a symlink. A companion `test -e` loop reported `dangling=97`, and the distinct targets are four directories under `/Users/dericchau/ai/fb2-port/frozen-bubble-sdl2/android/app/jni/` (REL-005 basis) | [REL-005](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29 (Task 8 Step 3; exact time not captured) | <code>grep -rn 'jni/include&#92;&#124;include/SDL2&#92;&#124;SDL2/SDL.h' android/app/CMakeLists.txt android/app/build.gradle CMakeLists.txt src/ .github/workflows/build.yml</code> | 1 | No output: no build file, source file, or CI step references the 97 symlinked headers — they are dead as well as broken | [REL-005](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T01:15:26Z | <code>git ls-files android &#124; while read f; do [ -f "$f" ] &amp;&amp; [ ! -L "$f" ] &amp;&amp; shasum -a 256 "$f"; done &gt; …/android-pre-manifest.txt</code> | 0 | 33 regular tracked files hashed; the other 101 of the 134 tracked `android/` paths are the 97 dangling symlinks and 4 gitlinks | `/tmp/fb-sdl3-audit/task8/android-pre-manifest.txt` |
| 2026-07-29T01:16:21Z | <code>git submodule status --recursive</code> | 0 | 38 entries, **0** uninitialized (no leading `-`) — recorded as 37 at the time and re-derived as **38** in Fix Round 1 below; the undercount is `plutovg`, which appears at two paths (`SDL3_ttf/external/plutovg` and `SDL3_ttf/external/plutosvg/plutovg`) at the same commit `3e6f922f`: `SDL3` release-3.4.4, `SDL3_image` release-3.4.2, `SDL3_mixer` release-3.2.0, `SDL3_ttf` release-3.2.2 plus their nested externals. The Android build could therefore proceed without this gate initializing anything | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T01:16:30Z | <code>./gradlew clean assembleRelease --no-daemon</code> (cwd `android/`) | 0 | `BUILD SUCCESSFUL in 1m 30s`, 52 actionable tasks. Warnings: SDK-XML v4 notice, the AGP 8 manifest-`package` notice, two `-Wignored-pragmas` from `SDL3/src/audio/SDL_audiotypecvt.c:541`/`:820`, and one javac deprecated-API note. No project C++ warning | `/tmp/fb-sdl3-audit/task8/android-build.log` |
| 2026-07-29T01:18:01Z | <code>git status --short</code> | 0 | No output after the build: **zero tracked-file drift**. `git diff --stat HEAD` was likewise empty and the post-build 33-file manifest diffed identical to the pre-build one, so no restoration was needed or performed | `/tmp/fb-sdl3-audit/task8/android-post-manifest.txt` |
| 2026-07-29 (Task 8 Step 4; exact time not captured) | <code>unzip -l android/app/build/outputs/apk/release/app-release-unsigned.apk</code> | 0 | 37,290,226-byte APK named `…-unsigned`; three ABI directories `arm64-v8a`, `armeabi-v7a`, `x86_64`, 13 shared objects each; no `META-INF/*.RSA`/`*.SF` v1 signature block. `output-metadata.json` reports `versionCode 10`, `versionName "2.4.27"`, `applicationId org.frozenbubble` (REL-007 basis) | [REL-007](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29 (Task 8 Step 4; exact time not captured) | <code>llvm-readelf -d lib/arm64-v8a/*.so</code> plus <code>strings -a</code> and <code>cmp</code> on the extracted APK | 0 | `libSDL3_image.so` `dlopen`s `libpng16.so` and `libSDL3_mixer.so` `dlopen`s `libvorbisfile.so`; `libpng.so` is **byte-identical** to `libpng16.so` and shares its `SONAME`, and **no** object names `libvorbisenc.so` in `DT_NEEDED` or any `dlopen` string. Redundant payload 819,904 + 1,846,824 = **2,666,728** bytes across three ABIs (IMP-014) | [IMP-014](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T01:19:18Z | <code>/bin/cp -Rc /opt/homebrew/Cellar/emscripten/6.0.4/libexec /tmp/fb-sdl3-audit/task8/emsdk/libexec</code> | 0 | 1.2 GB APFS clone of the Emscripten tree into the disposable directory; all subsequent port patching touched the copy only, and `em-config CACHE` resolved into the copy, leaving the Homebrew installation unmodified | `/tmp/fb-sdl3-audit/task8/emsdk/` |
| 2026-07-29 (Task 8 Step 5; exact time not captured) | <code>cp tools/ports/sdl3_mixer.py tools/ports/sdl3_image.py …/emsdk/libexec/tools/ports/</code> and the three CI `sed`/`perl` patches | 0 | Replayed the workflow's port setup on the copy: `SDL3_IMAGE_FORMATS`/`SDL3_MIXER_FORMATS` added at `src/settings.js:1654-1655` and `tools/settings.py:53`, and the experimental diagnostic commented at `tools/ports/sdl3.py:29`. Verified by re-grep after each edit | `/tmp/fb-sdl3-audit/task8/emsdk/libexec/tools/ports/` |
| 2026-07-29T01:20:06Z | <code>emcmake cmake .. -DCMAKE_BUILD_TYPE=Release</code> (cwd `build-audit-wasm/`) | 0 | Configured against the patched disposable toolchain; bundled iniparser selected, asset path `/Users/dchau/gr/frozen-bubble-sdl3/share` | `/tmp/fb-sdl3-audit/task8/wasm-configure.log` |
| 2026-07-29T01:20:12Z | <code>emmake make -j8</code> (cwd `build-audit-wasm/`) | 0 | **Full link in 44 s** — `frozen-bubble-sdl3.wasm` 3,132,667 B, `.js` 484,910 B, `.data` 23,647,363 B, `.html` 2,854 B. 29 objects compiled (27 shared plus both network-client TUs). 16 warnings in 5 families (`-Wdollar-in-identifier-extension` 7, `-Wvariadic-macro-arguments-omitted` 4, `-Wunused-variable` 2, `-Wunused-private-field` 2, `-Wunused-but-set-global` 1); no error, no undefined symbol | `/tmp/fb-sdl3-audit/task8/wasm-build.log` |
| 2026-07-29 (Task 8 Step 5; exact time not captured) | <code>llvm-nm --defined-only --extern-only networkclient.cpp.o networkclient_wasm.cpp.o</code> then <code>comm -12</code> | 0 | 62 external definitions vs 30; intersection **8**, all weak C++ template/inline instantiations (`std::__throw_length_error`, `std::allocator&lt;GameRoom&gt;::destroy`, `__throw_bad_array_new_length`, …). **Zero `NetworkClient::` methods in both** — the `#ifndef __WASM_PORT__` partition is exact, corroborated by the successful link | [Source-list parity](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29 (Task 8 Step 5; exact time not captured) | <code>python3 -c '…parse loadPackage(&#123;files:…&#125;) from frozen-bubble-sdl3.js…'</code> | 0 | **3,352** preloaded files, all under `/share/`, `remote_package_size` 23,647,363 — exactly matching the 3,352 files `find share -type f` reports on disk. Confirms `g_dataDir = "/share"` addresses the packaged tree | [WASM evidence](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29 (Task 8 Step 5; exact time not captured) | <code>python3 -c '…report first occurrence of IDBFS/syncfs/localStorage/MEMFS in frozen-bubble-sdl3.js…'</code> | 0 | Reported **first-occurrence indices only**: `IDBFS` **absent** (index -1); `syncfs` first found inside the `FS.syncfs` API definition; `localStorage` first found inside `ASM_CONSTS`, the `fb_nickname` read from `mainmenu.cpp:163`. **This command never measured occurrence counts**, so the "single occurrence" conclusion originally drawn from it was unsupported and is corrected by the Fix Round 1 rows below; the `IDBFS`-absent result stands, because index -1 does prove zero occurrences (BUG-048) | [BUG-048](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T01:25:06Z | <code>emcmake cmake -DCMAKE_TOOLCHAIN_FILE=…/cmake/Emscripten.cmake …</code> (throwaway dir) | 0 | The `WASM_PORT.md:65` command **succeeds**: no `server/` directory generated, no `SDL3_DIR` cache entry, `__WASM_PORT__` and `DATA_DIR=\"/share\"` in `flags.make`, `.html` suffix set — so `CMAKE_SYSTEM_NAME Emscripten` does set `EMSCRIPTEN`. The proposed defect is **disproved**; the real artifact is `TOTAL_MEMORY=268435456` ×4 beside `INITIAL_MEMORY=16777216` ×1 on one link line | [Dismissed candidates](subsystems/06-platform-ports.md#dismissed-candidates) |
| 2026-07-29T01:24:23Z | <code>/usr/bin/c++ -I/opt/homebrew/include -I"$PWD/src" -std=c++17 -arch arm64 -Wall -Wextra -pedantic -Werror -DDATA_DIR='"…/share"' …/task8_platform_harness.cpp build-audit-werror/CMakeFiles/frozen-bubble-sdl3.dir/src/&#123;platform,logger&#125;.cpp.o -L/opt/homebrew/lib -lSDL3 -o …/task8_harness</code> | 0 | Built the packaged-path harness warnings-strict against the **unchanged** production `platform.cpp.o` and `logger.cpp.o`; no diagnostic. It constructs no `FrozenBubble` and opens no real preference file | `/tmp/fb-sdl3-audit/task8/task8_platform_harness.cpp` |
| 2026-07-29 (Task 8 Step 6; exact time not captured) | <code>strings build-audit-release/frozen-bubble-sdl3 &#124; grep -x '/Users/.*/share'</code> | 0 | One literal: `/Users/dchau/gr/frozen-bubble-sdl3/share`. The Release binary bakes the build machine's source tree as `DATA_DIR` (REL-008 basis) | [REL-008](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29 (Task 8 Step 6; exact time not captured) | <code>SDL_VIDEODRIVER=dummy …/task8_harness datadir</code> (five layouts) | 0 | Real `InitDataDir()`: cwd repo-root and cwd `/` both gave `/Users/dchau/gr/frozen-bubble-sdl3/share` (working-directory independence); `FakeBundle.app/Contents/MacOS/` gave `…/Contents/Resources/share`; and the staged `…/stage/usr/local/bin/` binary **also** gave the source tree, ignoring `…/stage/usr/local/share/frozen-bubble` — REL-008 reproduced. A bundle without `Resources/share` reported `datadir_is_directory=0`, the input to BUG-034 | [REL-008](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29 (Task 8 Step 6; exact time not captured) | <code>env HOME=… CFFIXED_USER_HOME=… SDL_VIDEODRIVER=dummy …/task8_harness prefpath /tmp/fb-sdl3-audit/task8/home8</code> | 0 | `ISOLATION=OK`, `prefpath=/tmp/fb-sdl3-audit/task8/home8/Library/Application Support/frozen-bubble/`. Isolation asserted before anything else; macOS honours `CFFIXED_USER_HOME`, not `HOME`, as Task 6 established | [Task 8 limitations](subsystems/06-platform-ports.md#limitations) |
| 2026-07-29 (Task 8 Step 6; exact time not captured) | <code>for i in 1 2 3 4 5 6; do (cd …/logdir &amp;&amp; SDL_VIDEODRIVER=dummy …/task8_harness logger); done</code> | 0 | Six consecutive launches in one writable directory chose `frozen-bubble-creator.log`, `…joiner1`, `…joiner2`, `…joiner3`, `…joiner4`, then `…joiner4` again — five files created and every later launch appending to the same one. The names count launches, not players (BUG-047) | [BUG-047](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29 (Task 8 Step 6; exact time not captured) | <code>(cd …/rodir &amp;&amp; SDL_VIDEODRIVER=dummy …/task8_harness logger)</code> with the directory at mode 555 | 5 | Production `Logger::Initialize` printed `Failed to open log file: frozen-bubble-creator.log`, returned `logger_initialize=0 initialized=0`, and created nothing; the following `SDL_Log` was still emitted, by SDL's **default** handler, because `SDL_SetLogOutputFunction` is never reached on the failure path (BUG-047's ignored-failure half) | [BUG-047](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29 (Task 8 Step 7; exact time not captured) | <code>for f in $(ls android/app/src/main/java/org/libsdl/app/); do cmp -s … ; done</code> | 0 | All **11** vendored SDL Android Java files are byte-identical to the pinned SDL3 `release-3.4.4` submodule's `android-project` glue, and the set is complete — no SDL file missing locally, none project-modified | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29 (Task 8 Step 7; exact time not captured) | <code>for f in dictionary.c dictionary.h iniparser.c iniparser.h; do cmp -s android/app/jni/iniparser/$f third_party/iniparser/$f; done</code> | 0 | All **4** report identical at this gate, not merely at bootstrap. They are compiled by two different targets (`iniparser` vs `iniparser-static`), so the copies can drift silently — Task 9 dependency boundary | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29 (Task 8 Step 7; exact time not captured) | <code>shasum -a 256 -c /tmp/fb-sdl3-audit/task8/real-prefs-baseline.txt</code> | 0 | All three real preference files reported `OK`; the user's preferences were never modified by this gate | [Task 8 limitations](subsystems/06-platform-ports.md#limitations) |
| 2026-07-29 (Task 8 ledger; exact time not captured) | <code>git diff --stat 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 -- src server android web cmake CMakeLists.txt CMakeListsEmscripten.txt</code> | 0 | No output; production source, the Android project, the web shell, and every build file remain identical to the pinned baseline after the Android and WASM builds | Audit baseline above |
| 2026-07-29T02:11:02Z | <code>grep -o localStorage build-audit-wasm/frozen-bubble-sdl3.js &#124; wc -l</code> | 0 | **`localStorage` 4.** Corrects the "single occurrence" claim: the Step 5 evidence had reported only the first-occurrence index | [BUG-048](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:11:02Z | <code>grep -o syncfs build-audit-wasm/frozen-bubble-sdl3.js &#124; wc -l</code> | 0 | **`syncfs` 4.** Corrects the "single occurrence" claim the same way | [BUG-048](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:11:02Z | <code>grep -o IDBFS build-audit-wasm/frozen-bubble-sdl3.js &#124; wc -l</code> | 0 | **`IDBFS` 0.** Confirms the original absence result | [BUG-048](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:11:02Z | <code>grep -o 'localStorage&#92;.[a-zA-Z]*' build-audit-wasm/frozen-bubble-sdl3.js &#124; sort &#124; uniq -c</code> | 0 | `1 localStorage.getItem`, `3 localStorage.setItem`. The `ASM_CONSTS` keys are 626508 (get) and 626690 / 626749 / 626808 (set); all four are `fb_nickname`. **The three writes create the persistence** — the finding's "the `fb_nickname` `EM_ASM` read" characterization is corrected | [BUG-048](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:11:10Z | <code>grep -o 'FS&#92;.syncfs&#92;&#124;mount&#92;.type&#92;.syncfs&#92;&#124;syncfs(' build-audit-wasm/frozen-bubble-sdl3.js &#124; sort &#124; uniq -c</code> | 0 | `1 syncfs(` (the method definition), `1 FS.syncfs` (inside its own in-flight warning string), `2 mount.type.syncfs` (the guard and the dispatch). All four occurrences are inside `FS.syncfs`'s own definition, so **no `FS.syncfs(` invocation exists anywhere** and the API is never entered — BUG-048's conclusion stands on a measurement that supports it | [BUG-048](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:11:10Z | <code>for f in build-audit-wasm/frozen-bubble-sdl3.html web/shell.html; do for t in IDBFS syncfs localStorage indexedDB; do …; done; done</code> | 0 | All eight counts **0**. Neither the generated page nor the custom shell introduces a persistent mount, so the artifact set as a whole has no persistence path beyond `fb_nickname` | [BUG-048](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:10Z | <code>git submodule status --recursive &#124; wc -l</code> | 0 | **38** entries. Corrects the 37 recorded at 01:16:21Z | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:10Z | <code>git submodule status --recursive &#124; grep -c '^-'</code> | 1 | **0** uninitialized (`grep -c` exits 1 because the pattern matches zero lines) | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:10Z | <code>git submodule status --recursive &#124; awk '{print $1}' &#124; sort &#124; uniq -d</code> | 0 | Shows the cause of the 37→38 discrepancy: commit `3e6f922f` appears twice, as `SDL3_ttf/external/plutovg` and `SDL3_ttf/external/plutosvg/plutovg`. No conclusion changes: the build still ran against fully materialized sources | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:12Z | <code>awk -F'&#124;' '/^&#124; `/{if (tolower($4) ~ /pending/) c++} END{print c}' docs/audit/FILE_COVERAGE.md</code> | 0 | **21** rows carry a pending disposition (20 `Pending review` + `CMakeLists.txt`'s `Baseline exercised; static review pending`); the gate column of all 21 is `Task 9`. Corrects the coverage header's 20 | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T02:11:33Z | <code>for t in __WASM_PORT__ __ANDROID__ _WIN32 __linux__ __MINGW32__ __ANDROID_PORT__ __APPLE__ __EMSCRIPTEN__ WIN32; do grep -rho "&#92;b$t&#92;b" src/ &#124; wc -l; done</code> | 0 | Re-derived unchanged: 91, 26, 22, 2, 2, 1, 1, 0, 0. Count sweep — guard inventory verified | [Guard inventory](subsystems/06-platform-ports.md#static-review) |
| 2026-07-29T02:11:33Z | <code>grep -rn iconv src/ &#124; wc -l</code> | 0 | **7** lines. Count sweep — measures the same thing the finding claims | [IMP-015](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:11:33Z | <code>grep -rho iconv src/ &#124; wc -l</code> | 0 | **7** occurrences — coincides with the line count, so the recorded "7 hits" measures what it claims; no correction needed | [IMP-015](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:11:33Z | <code>grep -rn 'SDL_GetAndroid&#92;&#124;SDL_SendAndroidMessage&#92;&#124;SDL_IsAndroidTV&#92;&#124;SDL_ShowAndroid' src/ &#124; wc -l</code> | 0 | **11** matching lines | [Guard inventory](subsystems/06-platform-ports.md#static-review) |
| 2026-07-29T02:11:33Z | <code>grep -rho 'SDL_GetAndroid&#92;&#124;SDL_SendAndroidMessage&#92;&#124;SDL_IsAndroidTV&#92;&#124;SDL_ShowAndroid' src/ &#124; wc -l</code> | 0 | **12** name occurrences: the comment at `networkclient.cpp:1612` names two. Of the 11 lines, **9 are call sites** and 2 are comments (`networkclient.cpp:1612`, `mainmenu_panels.cpp:390`); all 9 call sites sit inside `#ifdef __ANDROID__`. Count sweep — the row's wording is tightened, the conclusion is unchanged | [Guard inventory](subsystems/06-platform-ports.md#static-review) |
| 2026-07-29T02:11:33Z | <code>python3 -c "…count filename: entries and remote_package_size in frozen-bubble-sdl3.js…"</code> | 0 | **3,352** `filename:` entries, all under `/share/`, `remote_package_size` 23,647,363. Count sweep — re-derived unchanged | [WASM evidence](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>find share -type f &#124; wc -l</code> | 0 | **3,352** files on disk, matching the preloaded count | [WASM evidence](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>stat -f%z android/app/build/outputs/apk/release/app-release-unsigned.apk</code> | 0 | **37,290,226** bytes. Count sweep — re-derived unchanged | [IMP-014](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>for a in arm64-v8a armeabi-v7a x86_64; do unzip -l … &#124; grep -c "lib/$a/.*&#92;.so"; done</code> | 0 | **13** `.so` in each of the three ABIs. Count sweep — re-derived unchanged | [IMP-014](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>awk</code> sum of the two redundant library sizes from the `unzip -l` byte column | 0 | libpng 819,904 + libvorbisenc 1,846,824 = **2,666,728**. Count sweep — re-derived unchanged | [IMP-014](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>grep -o TOTAL_MEMORY … /wasm-doccmd/CMakeFiles/frozen-bubble-sdl3.dir/link.txt &#124; wc -l</code> | 0 | **4** occurrences on the generated link line. Count sweep — the ×4 claim is measured, not inferred from the one occurrence in `cmake/Emscripten.cmake:33` | [Dismissed candidates](subsystems/06-platform-ports.md#dismissed-candidates) |
| 2026-07-29T02:11:33Z | <code>grep -o INITIAL_MEMORY … /wasm-doccmd/CMakeFiles/frozen-bubble-sdl3.dir/link.txt &#124; wc -l</code> | 0 | **1** occurrence on the generated link line. Count sweep — the ×1 claim is measured, not inferred from the one occurrence in `CMakeLists.txt:116` | [Dismissed candidates](subsystems/06-platform-ports.md#dismissed-candidates) |
| 2026-07-29T02:11:33Z | <code>find build-audit-wasm -name '*.cpp.o' &#124; wc -l</code> | 0 | **29** objects. Count sweep — re-derived unchanged | [WASM evidence](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>grep -o '&#91;-W&#91;a-z-&#93;*&#93;' wasm-build.log &#124; sort &#124; uniq -c</code> | 0 | **16** warnings in **5** families (7 / 4 / 2 / 2 / 1). Count sweep — re-derived unchanged | [WASM evidence](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>grep -c 'warning:' wasm-build.log</code> | 0 | **16** total warning lines, matching the family sum. Count sweep — re-derived unchanged | [WASM evidence](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>git ls-files -s android/app/jni/include/</code> | 0 | 97 tracked entries, **97** of mode `120000`. Count sweep — re-derived unchanged | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>ls android/app/src/main/java/org/libsdl/app/</code> | 0 | **11** SDL Java files. Count sweep — re-derived unchanged | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>ls android/app/jni/iniparser/</code> | 0 | **4** iniparser files. Count sweep — re-derived unchanged | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>git ls-files -s android &#124; awk '$1=="160000"' &#124; wc -l</code> | 0 | **4** gitlinks. Count sweep — re-derived unchanged | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>git ls-files android &#124; wc -l</code> | 0 | **134** tracked `android/` paths. Count sweep — re-derived unchanged | [Vendored boundary](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>wc -l nc-native-ext.syms</code> | 0 | **62**. Count sweep — re-derived unchanged | [Source-list parity](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>wc -l nc-wasm-ext.syms</code> | 0 | **30**. Count sweep — re-derived unchanged | [Source-list parity](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>wc -l nc-dup-ext.syms</code> | 0 | intersection **8**. Count sweep — re-derived unchanged | [Source-list parity](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>grep -c NetworkClient nc-dup-ext.syms</code> | 1 | **0** `NetworkClient` symbols in the intersection (`grep -c` exits 1 because the pattern matches zero lines). Count sweep — re-derived unchanged | [Source-list parity](subsystems/06-platform-ports.md#dynamic-evidence) |
| 2026-07-29T02:11:33Z | <code>strings build-audit-release/frozen-bubble-sdl3 &#124; grep -c -x '/Users/.*/share'</code> | 0 | **1** — the "single baked literal" claim now rests on a count, not on a one-line grep output. Count sweep — re-derived unchanged | [REL-008](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:11:44Z | <code>python3 -c "…extract src/*.cpp from each add_executable/add_library block and compare as sets…"</code> | 0 | Root `CMakeLists.txt` **27** explicit + `${NETWORK_CLIENT_SRC}` → **28** native / **29** Emscripten; `android/app/CMakeLists.txt` **28**, set difference with the native effective set **empty**; `CMakeListsEmscripten.txt` **15**, omitting exactly the **14** files the notebook names and adding `networkclient_wasm.cpp`. Count sweep — re-derived unchanged | [Source-list parity](subsystems/06-platform-ports.md#static-review) |
| 2026-07-29T02:13:33Z | <code>sed -n '104,108p;115,119p' android/app/src/main/java/org/frozenbubble/AssetExtractor.java</code> | 0 | `extractFile:106` is `if (dest.exists() &amp;&amp; dest.length() &gt; 0) return;` with no content, size, or timestamp comparison, and `extractAll:68-74` rewrites the marker unconditionally afterwards. Confirms both halves of the BUG-046 correction: a version bump re-enters `extractDir`, skips the truncated file again, and re-stamps the marker; and an update refreshes only absent-or-empty paths | [BUG-046](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:13:33Z | <code>sed -n '155,163p;75,81p;255,260p' .github/workflows/build.yml</code> | 0 | macOS: `cp -r share "$APP/Resources/share"` into a hand-built `.app` — the layout `platform.cpp:109-121` handles. Linux: assets at `AppDir/usr/share/frozen-bubble` with the binary in `AppDir/usr/bin`. Windows: `cp -r share pkg/` beside the `.exe` | [REL-008](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:13:33Z | <code>sed -n '49,52p' default.nix</code> | 0 | `default.nix:50` configures `-DASSET_PATH="$out/share"`. **No shipped artifact takes REL-008's fall-through path**; severity reassessed High → Medium | [REL-008](subsystems/06-platform-ports.md#confirmed-findings) |
| 2026-07-29T02:13:33Z | <code>grep -n '&lt;script' web/index.html</code> | 0 | Two tags: an inline block opening at `:89` (closing at `:135`) and the external `:136` `frozen-bubble-sdl2.js`. Corrects "only script tag" to "sole external script tag" in REL-006 | [REL-006](subsystems/06-platform-ports.md#confirmed-findings) |

### Task 8 Fix Round 2

An independent re-review of commit `90c3b8c7` approved the gate's substance —
every corrected and re-derived quantity from Fix Round 1 reproduced exactly and
Fix Round 1 introduced no new measurement error — but raised one Important and
six Minor wording/ledger-hygiene findings. All seven were accepted; none was
disputed. Verified against the real files: the REL-008 "highest-impact"
superlative was stale against its own Fix Round 1 downgrade and is re-anchored
to name the findings that actually reach shipped artifacts (BUG-046, BUG-048);
the Fix Round 1 opening summary overstated review coverage and misattributed
the submodule-count correction to the review rather than the implementer's
follow-up sweep; the count-sweep enumeration named the wrong number of items
against its stated "eighteen" and omitted the "0 v1 signature entries" item;
eleven Fix Round 1 ledger rows bundled 2-5 commands under a single exit cell,
two of them with non-integer "0 / 1" exit values, violating this file's own
one-command-per-row convention; the gate conclusion's "All eight brief steps
executed" did not qualify the recorded Step 5/Step 6 substitutions; the
`syncfs` ledger row said "own body" where the notebook already said the
accurate "own definition"; and the WASM nickname read was called `EM_ASM`
where it is `EM_ASM_PTR` (`mainmenu.cpp:162`). FINDINGS.md's line 98 was
checked against the same EM_ASM claim and found to already read `ASM_CONSTS`
with no `EM_ASM` string present, so no edit was needed there — the review
finding did not reproduce against that file's current content.

| 2026-07-29T07:23:00Z | <code>python3 -c "…extract '## ' headings from docs/audit/subsystems/06-platform-ports.md and compare to the ten required, in order…"</code> | 0 | `headings=10 match=True each_once=True` — Scope, Trust boundaries and invariants, Static review, Dynamic evidence, Candidates, Confirmed findings, Dismissed candidates, Coverage, Limitations, Gate conclusion, each exactly once, in order | [06-platform-ports.md](subsystems/06-platform-ports.md) |
| 2026-07-29T07:23:06Z | <code>test "$(awk -F'&#96;' '/^&#92;&#124; &#96;/ {count++} END {print count+0}' docs/audit/FILE_COVERAGE.md)" = "237" &amp;&amp; python3 -c '…extract path column…' &amp;&amp; diff -u /tmp/fb-sdl3-audit/task1-expected-paths.txt /tmp/fb-sdl3-fixround2-final.txt</code> | 0 | `rows=237 OK inventory-equality OK` — no diff output against the Task 1 cached expected-path set for the pinned tree `09d6c7bf` | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T07:23:11Z | <code>python3 -c "…extract BUG/SEC/REL/IMP IDs from FINDINGS.md and check uniqueness and per-class contiguity from 1…"</code> | 0 | `total=78 unique=78 contiguous_all=True counts={'IMP': 15, 'BUG': 48, 'SEC': 7, 'REL': 8}` — no ID recycled or renumbered | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T07:23:15Z | <code>python3 -c "…split every &#96;&#124; 2026-…&#96; command-table row on '&#124;' and check 7 cells and an integer exit cell…"</code> | 0 | `rows=474 malformed=0 bad_exit=0` — every command-table row in `SDL3_REVIEW_STATUS.md` has exactly 7 pipe-separated cells (no unescaped `&#124;`) and a single-integer exit cell, after this round's row-splitting | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T07:23:19Z | <code>grep -c '^- Exact next action:' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | **1** — this file's line-35 exact-next-action bullet appears exactly once | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T07:23:19Z | <code>grep -c "^&#124; Task 8 &#124;" docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | **1** — the Task 8 gate row appears exactly once and reads `complete (...)` with the Fix Round 2 summary appended | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T07:23:24Z | <code>git diff --stat 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 -- src server android web cmake tools tests third_party CMakeLists.txt CMakeListsEmscripten.txt .github</code> | 0 | No output; production source, the Android project, the web shell, and every build file remain identical to the pinned baseline after Fix Round 2's doc-only edits | Audit baseline above |
| 2026-07-29T07:23:24Z | <code>git diff --check</code> | 0 | No output; no whitespace/conflict-marker defect in the Fix Round 2 diff | Fix Round 2 diff |

### Task 9

| 2026-07-29T07:46:13Z | <code>ruby -e 'require "yaml"; YAML.load_file(".github/workflows/build.yml", aliases: true)'</code> | 1 | The brief's exact command. `unknown keyword: aliases (ArgumentError)` — macOS ships Ruby 2.6.10, whose Psych predates the keyword. A host limitation, **not** a YAML defect | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T07:46:16Z | <code>ruby -e 'require "yaml"; d=YAML.load_file(".github/workflows/build.yml"); puts d["jobs"].keys.length'</code> | 0 | Accepted substitute. **11** jobs parsed; the document contains no YAML anchors, so the omitted keyword was immaterial | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T07:46:20Z | <code>docker compose -f docker/docker-compose.yml config</code> | 0 | Local parse only; the daemon was not required and no container was created. Resolved to 2 services, ports 1511/80/443, 3 read-only bind mounts, `depends_on: service_started`. `docker compose up` was deliberately not run | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T07:46:40Z | <code>printf '/build-audit-config/&#92;n' &gt;&gt; .git/info/exclude</code> | 0 | No output; the build directory was locally excluded **before** it was created, so it can never appear as drift | Processes and cleanup below |
| 2026-07-29T07:46:51Z | <code>cmake -S . -B build-audit-config -G Ninja -DCMAKE_BUILD_TYPE=Release</code> | 0 | `iniparser system package not found, building from bundled source`; `Found glib-2.0, version 2.88.2`; `Asset path is: …/frozen-bubble-sdl3/share`; `Installed assets will be at: /usr/local/share/frozen-bubble` — REL-008's message about a variable nothing consumes | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T07:46:58Z | <code>ctest -N</code> | 0 | `Total Tests: 5` in the fresh configure — the five registrations at `CMakeLists.txt:160-197` | [subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5](subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5) |
| 2026-07-29T07:47:05Z | <code>git ls-files android &#124; while read -r f; do shasum -a 256 "$f"; done &gt; android-manifest-before.txt</code> | 0 | Pre-Gradle drift manifest: **134** tracked `android/` paths hashed; manifest SHA-256 `d1bd0edf…92dd1` | Processes and cleanup below |
| 2026-07-29T07:47:08Z | <code>git ls-files -s android &#124; shasum -a 256</code> | 0 | Pre-Gradle index hash `e7f56a3342a1e48c27c88386e7fa8763f509d14eafdc4f5adc83c57f25ed3b74` | Processes and cleanup below |
| 2026-07-29T07:47:15Z | <code>./android/gradlew tasks --all --no-daemon</code> | 1 | The brief's exact command. `Directory '/Users/dchau/gr/frozen-bubble-sdl3' does not contain a Gradle build` — the wrapper takes its project directory from the CWD. A CWD limitation, **not** a repository defect | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T07:47:22Z | <code>./android/gradlew --project-dir android tasks --all --no-daemon</code> | 0 | Accepted substitute. `BUILD SUCCESSFUL`, 329 task lines including `app:assembleRelease`, `app:bundleRelease`, `app:lintVitalRelease`, `app:testDebugUnitTest`, `app:testReleaseUnitTest` | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T07:47:31Z | <code>./android/gradlew --project-dir android :app:signingReport --no-daemon</code> | 0 | `Variant: release / Config: null / Store: null / Alias: null` — direct confirmation that the release build type declares no `signingConfig` (REL-007) | [subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| 2026-07-29T07:47:45Z | <code>diff -u android-manifest-before.txt android-manifest-after.txt</code> | 0 | No output. Post-Gradle manifest identical across all 134 tracked `android/` paths; the index hash re-derived to the same `e7f56a33…5ed3b74`. **No path was restored, because none was modified** | Processes and cleanup below |
| 2026-07-29T07:47:50Z | <code>python3 -m py_compile tools/net_bots.py tests/*.py tools/server_tests/*.py</code> | 0 | No output; all four Python files compile | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T07:47:52Z | <code>python3 -m py_compile tools/ports/sdl3_image.py tools/ports/sdl3_mixer.py</code> | 0 | No output; both Emscripten port files compile | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T07:48:10Z | <code>openssl ecparam -name prime256v1 -genkey -noout -out ec_privkey.pem</code> | 0 | No output; an ECDSA private key of certbot's default type was generated under `/tmp/fb-sdl3-audit/task9/` | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T07:48:12Z | <code>openssl rsa -in ec_privkey.pem -check -noout</code> | 1 | `expecting an rsa key` (system LibreSSL 3.3). This is `docker/setup.sh:23`'s `key_ok` predicate verbatim — REL-010's regenerate branch is taken | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T07:48:14Z | <code>/opt/homebrew/opt/openssl@3/bin/openssl rsa -in ec_privkey.pem -check -noout</code> | 1 | `Not an RSA key` (OpenSSL 3.6.3). The predicate fails on the implementation the documented Ubuntu target uses too, so REL-010 is not a LibreSSL artefact | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T07:48:30Z | <code>cmake -P exec_program_probe.cmake</code> | 0 | `out=hi rv=0` with only a `CMP0153` developer warning on CMake 4.3.4 — `exec_program` is deprecated, **not** removed, disproving the `cmake_uninstall.cmake.in` candidate | [subsystems/07-build-release-tooling.md#dismissed-candidates](subsystems/07-build-release-tooling.md#dismissed-candidates) |
| 2026-07-29T08:06:20Z | <code>ruby -e '…count jobs, jobs with if:false, release files and release needs…'</code> | 0 | `jobs=11 if_false=0 release_files=5 release_needs=5`. Count sweep — contradicts `CLAUDE.md`'s CI section (REL-009) | [subsystems/07-build-release-tooling.md#operational-documentation-against-actual-commands-step-6](subsystems/07-build-release-tooling.md#operational-documentation-against-actual-commands-step-6) |
| 2026-07-29T08:06:20Z | <code>grep -c 'uses:' .github/workflows/build.yml</code> | 0 | **27** action references. Count sweep — the denominator for the pinning claim | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T08:06:20Z | <code>grep -cE 'uses: [^ ]+@[0-9a-f]{40}' .github/workflows/build.yml</code> | 1 | **0** commit-pinned references (`grep -c` exits 1 because the pattern matches zero lines). Count sweep — measured, not inferred | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T08:06:20Z | <code>grep -cE 'uses: [^ ]+@(master&#124;main)' .github/workflows/build.yml</code> | 0 | **5** branch-pinned references. Count sweep | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T08:06:20Z | <code>grep -c 'josephbmanley/butler-publish-itchio-action@master' .github/workflows/build.yml</code> | 0 | **5** — all five branch-pinned references are the butler action. Count sweep | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T08:06:20Z | <code>grep -c 'secrets.BUTLER_CREDENTIALS' .github/workflows/build.yml</code> | 0 | **5** — every branch-pinned step also receives the itch.io deploy secret. Count sweep | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T08:06:20Z | <code>sed -n '261,268p' .github/workflows/build.yml &#124; tr -s ' &#92;&#92;' '&#92;n&#92;n' &#124; grep -c '&#92;.dll$'</code> | 0 | **Undercounted — corrected in Fix Round 1.** Reported 20 DLL names, but this command splits only on space/backslash, and the loop's final entry, `libpcre2-8-0.dll;`, has its trailing semicolon glued directly to the filename with no space or backslash for `tr` to split on, so the `&#92;.dll$`-anchored `grep` never matched it. The true count is **21**, re-derived by regex extraction in the Fix Round 1 row below; the "corrects an earlier working estimate of 21" note this row originally carried had it backwards — that estimate was correct | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T08:06:20Z | <code>grep -c '&#124;&#124; true' .github/workflows/build.yml</code> | 0 | **3** failure suppressions; two are benign optional `fb-server` copies. Count sweep | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T08:06:20Z | <code>grep -c 'permissions:' .github/workflows/build.yml</code> | 0 | **1** — only `release` declares a token scope; the other 10 jobs inherit the repository default. Count sweep | [subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| 2026-07-29T08:06:20Z | <code>grep -cE 'ctest&#124;BUILD_TESTING&#124;--target test&#124;gradlew test&#124;pytest' .github/workflows/build.yml</code> | 1 | **0** test invocations across all 11 jobs (`grep -c` exits 1 on zero matches). Count sweep — the basis of IMP-016 | [subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5](subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5) |
| 2026-07-29T08:06:20Z | <code>ls .github/workflows &#124; wc -l</code> | 0 | **1** workflow file, so the zero-test-invocation count is repository-wide. Count sweep | [subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5](subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5) |
| 2026-07-29T08:06:20Z | <code>grep -cE 'CMAKE_OSX_ARCHITECTURES&#124;universal&#124;[^-]-arch ' CMakeLists.txt .github/workflows/build.yml</code> | 1 | **0** in each file. Count sweep — the macOS artifact is single-architecture by omission (REL-012) | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T08:06:20Z | <code>grep -cE 'versionCode&#124;versionName' .github/workflows/build.yml</code> | 1 | **0** — no workflow step overrides the literal `versionCode 10`. Count sweep (REL-004) | [subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| 2026-07-29T08:06:20Z | <code>grep -c 'VERSIONINFO' share/icons/fb.rc</code> | 1 | **0** — the Windows resource file holds only an `ICON` statement, so the shipped `.exe` carries no version resource. Count sweep (REL-004) | [subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| 2026-07-29T08:06:20Z | <code>git ls-files third_party/iniparser &#124; wc -l</code> | 0 | **4** tracked files — two `.c`, two `.h`, and no licence, README, or version marker. Count sweep (REL-014) | [subsystems/07-build-release-tooling.md#imp-008-closure](subsystems/07-build-release-tooling.md#imp-008-closure) |
| 2026-07-29T08:06:20Z | <code>git ls-files &#124; grep -icE '^(LICENSE&#124;COPYING&#124;NOTICE)'</code> | 0 | **1** licence file in the whole repository (`COPYING`), which does not mention iniparser. Count sweep (REL-014) | [subsystems/07-build-release-tooling.md#imp-008-closure](subsystems/07-build-release-tooling.md#imp-008-closure) |
| 2026-07-29T08:06:20Z | <code>git ls-files &#124; grep -c '^dist-wasm/'</code> | 1 | **0** tracked paths under `dist-wasm/` — the input to the `netlify.toml` question, which was dismissed rather than promoted. Count sweep | [subsystems/07-build-release-tooling.md#dismissed-candidates](subsystems/07-build-release-tooling.md#dismissed-candidates) |
| 2026-07-29T08:06:34Z | <code>python3 -c "…extract src/*.cpp per build file, compare as sets, and count server sources…"</code> | 0 | `native=28 emscripten=29 android=28 android_eq_native=True emsc_file=15 missing=14`; `server_srcs=7 ondisk=7`. Count sweep — reproduces Task 8's parity numbers exactly | [subsystems/07-build-release-tooling.md#build-definition-parity-step-1](subsystems/07-build-release-tooling.md#build-definition-parity-step-1) |
| 2026-07-29T08:06:34Z | <code>for p in sdl3 sdl3_ttf sdl3_image sdl3_mixer; do grep -m1 -oE 'release-[0-9.]+…' …task8/emsdk/libexec/tools/ports/$p.py; done</code> | 0 | `sdl3=3.4.2 sdl3_ttf=release-3.2.2 sdl3_image=release-3.2.4 sdl3_mixer=3.2.0`, read from the disposable emsdk copy that actually linked the Task 8 WASM artifact. Count sweep (REL-011) | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T08:06:34Z | <code>sed -n '43,46p' .github/workflows/build.yml</code> | 0 | `SDL release-3.4.4`, `SDL_image release-3.4.2`, `SDL_mixer release-3.2.0`, `SDL_ttf release-3.2.2` — the pins the WASM row is skewed against. Count sweep (REL-011) | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T08:06:34Z | <code>git ls-files android &#124; wc -l</code> | 0 | **134** tracked `android/` paths — the manifest denominator. Count sweep — re-derived unchanged from Task 8 | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |

### Task 9 Fix Round 1 Ledger

| 2026-07-29T13:21:12Z | <code>sed -n '261,269p' .github/workflows/build.yml &#124; grep -oE '[A-Za-z0-9_.+-]+&#92;.dll' &#124; wc -l</code> | 0 | **21** — a regex extraction over the loop's own line range finds all 21 DLL names, correcting REL-013's 20 | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T13:21:17Z | <code>sed -n '261p;269p' .github/workflows/build.yml</code> | 0 | `for dll in &#92;` at `:261`, `done` at `:269` — the loop's line range was already correct; only the count inside it was wrong | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T13:21:20Z | <code>grep -nE 'storepass&#124;keypass&#124;KEYSTORE_PASSWORD&#124;KEY_PASSWORD' .github/workflows/build.yml</code> | 0 | Six lines: `:396,397` (`-storepass`/`-keypass` literals in "Generate release keystore"), `:404,406` (`KEYSTORE_PASSWORD`/`KEY_PASSWORD` literals in "Build release APK"'s `env:` block), and `:411,413` (the same two names read back as `$VAR`, not new literal instances) — confirms REL-007's four literal occurrences span two steps, two of them outside the gate's original `:390-398` citation | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T13:21:26Z | <code>grep -in 'pending' docs/audit/FILE_COVERAGE.md</code> | 0 | One match: line 5, the inventory rule paragraph itself. Corrects the rule's own claim that a second, prose location exists — it does not | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T13:21:31Z | <code>python3 -c "…extract '## ' headings from docs/audit/subsystems/07-build-release-tooling.md and compare to the ten required, in order…"</code> | 0 | `headings=10 match=True each_once=True` — the ten-heading invariant holds for this notebook, unaffected by the Fix Round 1 prose appended to its Gate conclusion section | [07-build-release-tooling.md](subsystems/07-build-release-tooling.md#gate-conclusion) |
| 2026-07-29T13:21:35Z | <code>grep -c '^## ' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | **16** — this file's own level-2 heading count, unaffected by this round's `###`-level Fix Round subsection; the ten-heading invariant was never claimed for this file (it applies to the nine subsystem notebooks only) | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T13:22:00Z | <code>python3 -c "…extract BUG/SEC/REL/IMP IDs from FINDINGS.md and check uniqueness and per-class contiguity from 1…"</code> | 0 | `total=92 unique=92 contiguous=True counts={'IMP': 23, 'BUG': 48, 'SEC': 7, 'REL': 14}` — no ID recycled or renumbered by this round's doc-only edits | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T13:21:54Z | <code>python3 -c "…extract path column from FILE_COVERAGE.md, count and dedupe…" &amp;&amp; diff -u /private/tmp/fb-sdl3-audit/task1-expected-paths.txt &lt;(sort /tmp/fb-sdl3-fixround-final-paths.txt)</code> | 0 | `rows=237 unique=237`; empty diff against the Task 1 cached expected-path set for the pinned tree `09d6c7bf` — inventory-equality holds | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T13:22:00Z | <code>awk -F'&#124;' '/^&#124; &#96;/{if (tolower($0) ~ /pending/) c++} END{print c+0}' docs/audit/FILE_COVERAGE.md</code> | 0 | **0** — no coverage row carries a pending disposition | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T13:24:30Z | <code>grep -c '^- Exact next action:' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | **1** — the exact-next-action bullet appears exactly once, unchanged: "Begin Task 10, Step 1: define the recorded matrix before launching processes." | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T13:24:30Z | <code>grep -c '^&#124; Task 9 &#124;' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | **1** — the Task 9 gate row appears exactly once and reads `complete (...)` with the Fix Round 1 summary appended | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T13:24:30Z | <code>git diff --stat 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 -- src server android web cmake tools tests third_party share CMakeLists.txt CMakeListsEmscripten.txt .github docker README.md SetupServer.md CLAUDE.md netlify.toml default.nix shell.nix flake.nix flake.lock start-server.sh</code> | 0 | No output; every production, build, and documentation path outside `docs/audit/` remains byte-identical to the pinned baseline after Fix Round 1's doc-only edits | Audit baseline above |
| 2026-07-29T13:24:30Z | <code>git diff --check &amp;&amp; git diff --cached --check</code> | 0 | No output from either; no whitespace or conflict-marker defect in the Fix Round 1 diff | Fix Round 1 diff |
| 2026-07-29T13:24:53Z | <code>python3 -c "…split every &#96;&#124; 2026-…&#96; command-table row on '&#124;' and check 7 cells and an integer exit cell…"</code> | 0 | `rows=531 malformed=0 bad_exit=0` at the time of this check — every command-table row in `SDL3_REVIEW_STATUS.md` has exactly 7 pipe-separated cells and a single-integer exit cell after this round's additions | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |

### Task 9 Fix Round 2

An independent re-review of commit `2b859b42` verified all four Fix Round 1
corrections reproduced unchanged (the 21-DLL count, REL-007's four password
line numbers, both Minors, 19 of the 21 sweep quantities directly, ID
contiguity, the 237-row coverage inventory, and the `## `/level-2 heading
count), and found that Fix Round 1's own re-run count sweep introduced two
Important defects in a single ledger row, plus one Minor pre-existing since
Fix Round 1 landed. **All three were accepted; none was disputed.**

- **A bundled ledger row recorded a non-representative exit (Important 1,
  fixed).** The former row at `SDL3_REVIEW_STATUS.md:1400` grouped sixteen
  independent top-level `grep`/`ls`/`git ls-files` invocations inside one `{
  ...; }` shell group and recorded a single exit of **0** for the whole
  group — violating this file's own "exactly one top-level shell command per
  row" convention (stated above the ledger) and misrepresenting the result,
  since at least six of the sixteen constituent commands (commit-pinned,
  test-invocation, the two-file architecture grep, versionCode/versionName,
  VERSIONINFO, and dist-wasm) individually exit **1** on zero matches — this
  file's own earlier unbundled rows for those same greps already record exit
  1. This is the same defect class Task 8 Fix Round 2 fixed (its gate row
  records "eleven bundled-command ledger rows split to one command per
  row"). **Fix:** the bundled row is removed and its sixteen constituent
  commands are re-run individually below, each with the exit that command
  actually produces standing alone. Every quantity the removed row reported
  (27, 0, 5, 5, 5, 3, 1, 0, 1, 0/0, 0, 0, 4, 1, 0, 134) remains represented
  in the ledger, unchanged, now with an honest per-command exit.
- **The same row mislabeled its own count (Important 2, fixed).** Its
  Result cell said "Every one of the fourteen simple grep/wc quantities …
  reproduced unchanged" while listing sixteen comma-separated values drawn
  from sixteen distinct commands. **Fourteen was wrong on either count:
  sixteen commands, or seventeen quantities once the two-file architecture
  grep's "0/0" is unpacked into two per-file counts.** A search of
  `FINDINGS.md`, the `07-build-release-tooling.md` notebook, and the
  git-ignored Task 9 report found the wrong count nowhere else — it was
  confined to this one now-removed cell, so no other document needed
  correction.
- **A duplicate heading misdirected two cross-links (Minor 3, fixed).** This
  file had two identical `### Task 9 Fix Round 1` headings — the narrative
  (formerly `:748`) and the ledger (formerly `:1392`). Both
  `FINDINGS.md:258` and `subsystems/07-build-release-tooling.md:1064` link
  to "the status ledger" and both meant the evidence table, but the
  duplicate heading text meant both anchors resolved to the first
  (narrative) occurrence. **Fix:** the two headings are renamed `### Task 9
  Fix Round 1 Findings` and `### Task 9 Fix Round 1 Ledger`, and both
  cross-links are retargeted to `#task-9-fix-round-1-ledger`. No other
  task's Fix Round headings in this file are duplicated — `Task 7 Fix Round
  1`/`Task 7 Fix Round 2` and `Task 8 Fix Round 1`/`Task 8 Fix Round 2` each
  carry a distinct round number and appear once apiece.
- **Bundled-row sweep (also required).** Every other Task 9 ledger row, from
  both the original Task 9 commit and Fix Round 1, was inspected by hand for
  a bundled `{ ...; }` group or a recorded exit its command did not
  actually produce. None was found: the `for`/`while`-loop rows (DLL sed/tr
  extraction, the Android manifest hash loop, the port-version loop) are
  each one syntactic loop per the file's own convention; the `&&`-chained
  rows (the FILE_COVERAGE.md path-count-and-diff check, the `git diff
  --check && git diff --cached --check` row) are single compound AND-lists
  whose recorded exit is the chain's real exit, not a composite standing in
  for unrelated commands. The removed row at the old `:1400` was the only
  defect of this class in Task 9's ledger contribution.

| 2026-07-29T13:44:44Z | <code>grep -c 'uses:' .github/workflows/build.yml</code> | 0 | **27** action references — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's non-representative shared exit (Important 1) | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T13:44:45Z | <code>grep -cE 'uses: [^ ]+@[0-9a-f]{40}' .github/workflows/build.yml</code> | 1 | **0** commit-pinned references (`grep -c` exits 1 because the pattern matches zero lines) — reproduces the count unchanged. Fix Round 2: individually re-run; this is one of the six constituents whose true standalone exit the removed row's shared 0 misrepresented | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T13:44:46Z | <code>grep -cE 'uses: [^ ]+@(master&#124;main)' .github/workflows/build.yml</code> | 0 | **5** branch-pinned references — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T13:44:47Z | <code>grep -c 'josephbmanley/butler-publish-itchio-action@master' .github/workflows/build.yml</code> | 0 | **5** — all five branch-pinned references are the butler action — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T13:44:48Z | <code>grep -c 'secrets.BUTLER_CREDENTIALS' .github/workflows/build.yml</code> | 0 | **5** — every branch-pinned step also receives the itch.io deploy secret — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| 2026-07-29T13:44:49Z | <code>grep -c '&#124;&#124; true' .github/workflows/build.yml</code> | 0 | **3** failure suppressions — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T13:44:50Z | <code>grep -c 'permissions:' .github/workflows/build.yml</code> | 0 | **1** — only `release` declares a token scope — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| 2026-07-29T13:44:51Z | <code>grep -cE 'ctest&#124;BUILD_TESTING&#124;--target test&#124;gradlew test&#124;pytest' .github/workflows/build.yml</code> | 1 | **0** test invocations across all 11 jobs (`grep -c` exits 1 on zero matches) — reproduces the count unchanged. Fix Round 2: individually re-run; another of the six constituents whose true standalone exit the removed row's shared 0 misrepresented | [subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5](subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5) |
| 2026-07-29T13:44:52Z | <code>ls .github/workflows &#124; wc -l</code> | 0 | **1** workflow file — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5](subsystems/07-build-release-tooling.md#test-coverage-against-discovered-risks-step-5) |
| 2026-07-29T13:44:53Z | <code>grep -cE 'CMAKE_OSX_ARCHITECTURES&#124;universal&#124;[^-]-arch ' CMakeLists.txt .github/workflows/build.yml</code> | 1 | **0** in each file (`0/0`) — reproduces the count unchanged. Fix Round 2: individually re-run; another of the six constituents whose true standalone exit the removed row's shared 0 misrepresented | [subsystems/07-build-release-tooling.md#confirmed-findings](subsystems/07-build-release-tooling.md#confirmed-findings) |
| 2026-07-29T13:44:54Z | <code>grep -cE 'versionCode&#124;versionName' .github/workflows/build.yml</code> | 1 | **0** — no workflow step overrides the literal `versionCode 10` — reproduces the count unchanged. Fix Round 2: individually re-run; another of the six constituents whose true standalone exit the removed row's shared 0 misrepresented | [subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| 2026-07-29T13:44:55Z | <code>grep -c 'VERSIONINFO' share/icons/fb.rc</code> | 1 | **0** — the Windows resource file holds only an `ICON` statement — reproduces the count unchanged. Fix Round 2: individually re-run; another of the six constituents whose true standalone exit the removed row's shared 0 misrepresented | [subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| 2026-07-29T13:44:56Z | <code>git ls-files third_party/iniparser &#124; wc -l</code> | 0 | **4** tracked files — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#imp-008-closure](subsystems/07-build-release-tooling.md#imp-008-closure) |
| 2026-07-29T13:44:57Z | <code>git ls-files &#124; grep -icE '^(LICENSE&#124;COPYING&#124;NOTICE)'</code> | 0 | **1** licence file in the whole repository — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#imp-008-closure](subsystems/07-build-release-tooling.md#imp-008-closure) |
| 2026-07-29T13:44:58Z | <code>git ls-files &#124; grep -c '^dist-wasm/'</code> | 1 | **0** tracked paths under `dist-wasm/` — reproduces the count unchanged. Fix Round 2: individually re-run; the last of the six constituents whose true standalone exit the removed row's shared 0 misrepresented | [subsystems/07-build-release-tooling.md#dismissed-candidates](subsystems/07-build-release-tooling.md#dismissed-candidates) |
| 2026-07-29T13:44:59Z | <code>git ls-files android &#124; wc -l</code> | 0 | **134** tracked `android/` paths — reproduces the count unchanged. Fix Round 2: individually re-run to replace the removed bundled row's shared exit (Important 1) | [subsystems/07-build-release-tooling.md#dynamic-evidence](subsystems/07-build-release-tooling.md#dynamic-evidence) |
| 2026-07-29T13:48:12Z | <code>python3 -c "…extract '## ' headings from each of the nine docs/audit/subsystems/*.md notebooks and compare each to the ten required, in order…"</code> | 0 | `headings=10 match=True` for all nine notebooks (`01-server-protocol.md` through `09-final-challenge.md`) — the ten-heading invariant holds repository-wide, unaffected by this round's doc-only edits | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T13:48:16Z | <code>python3 -c "…count &#96;&#124; &#96;&#96;&#96;&#96; rows and case-insensitive 'pending' occurrences in docs/audit/FILE_COVERAGE.md…"</code> | 0 | `rows=237 pending=0` — the 237-row inventory and zero-pending invariant both hold unchanged | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T13:48:20Z | <code>python3 -c "…extract BUG/SEC/REL/IMP IDs from FINDINGS.md and check uniqueness and per-class contiguity from 1…"</code> | 0 | `total=92 unique=92 contiguous=True counts={'IMP': 23, 'BUG': 48, 'SEC': 7, 'REL': 14}` — still 92 unique, contiguous per class; no ID recycled or renumbered by this round's doc-only edits | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T13:48:24Z | <code>python3 -c "…split every &#96;&#124; 2026-…&#96; command-table row on '&#124;' and check 7 cells and an integer exit cell…"</code> | 0 | `rows=535 malformed=0 bad_exit=0` at the time of this check, before this round's own row-splitting additions; every command-table row in `SDL3_REVIEW_STATUS.md` has exactly 7 pipe-separated cells and a single-integer exit cell | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T13:48:28Z | <code>grep -c '^- Exact next action:' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | **1** — the exact-next-action bullet still appears exactly once, unchanged: "Begin Task 10, Step 1: define the recorded matrix before launching processes." | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T13:48:32Z | <code>grep -c '^&#124; Task 9 &#124;' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | **1** — the Task 9 gate row still appears exactly once and reads `complete (...)` with the Fix Round 2 summary appended | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |
| 2026-07-29T13:48:36Z | <code>git diff --stat 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 -- src server android web cmake tools tests third_party share CMakeLists.txt CMakeListsEmscripten.txt .github docker README.md SetupServer.md CLAUDE.md netlify.toml default.nix shell.nix flake.nix flake.lock start-server.sh</code> | 0 | No output; every production, build, and documentation path outside `docs/audit/` remains byte-identical to the pinned baseline after Fix Round 2's doc-only edits | Audit baseline above |
| 2026-07-29T13:48:40Z | <code>git diff --check &amp;&amp; git diff --cached --check</code> | 0 | No output from either; no whitespace or conflict-marker defect in the Fix Round 2 diff | Fix Round 2 diff |
| 2026-07-29T13:48:49Z | <code>python3 -c "…slugify every heading in SDL3_REVIEW_STATUS.md GitHub-style and check for duplicate anchors, specifically task-9-fix-round-1-findings and task-9-fix-round-1-ledger…"</code> | 0 | `duplicate_anchors=[] findings=1 ledger=1 bare_old_anchor=0` — the renamed headings each resolve to a unique anchor and the retired ambiguous `#task-9-fix-round-1` anchor no longer exists; both retargeted cross-links (`FINDINGS.md:258`, `subsystems/07-build-release-tooling.md:1064`) now resolve to `#task-9-fix-round-1-ledger` | [SDL3_REVIEW_STATUS.md](SDL3_REVIEW_STATUS.md) |

## Limitations

- Emscripten, cppcheck, and clang-tidy were absent at bootstrap and installed successfully in Task 2. Homebrew LLVM remains keg-only, so the audit invokes clang-tidy by its absolute `$(brew --prefix llvm)/bin` path.
- The default Release build is successful but not warning-clean: AppleClang emitted 51 server warning instances from 27 unique locations. Task 3A confirmed IMP-001 through IMP-004 as implementation improvements; the audit itself intentionally leaves production code unchanged.
- The strict Debug build cannot complete until IMP-001 through IMP-004 are resolved in a future remediation task. Its subsequent 3/5 not-run CTest result is a downstream missing-executable consequence, not an independent candidate.
- Apple ASan does not support leak detection on this host. The required leak-enabled run is recorded as an environment limitation; the accepted leak-disabled verification passed four unaffected tests plus the isolated foreground server-list assertions with no sanitizer diagnostic.
- The sanitizer build's two `sprintf` deprecation warnings are in bundled `third_party/iniparser`, classified as vendored dependency noise. Task 9 completed the deferred boundary/version review: the vendored copy has no licence, README, or version marker (REL-014) and the build silently prefers an unconstrained system copy over it (IMP-023). Its internals remain outside the project-owned source audit.
- The exact clang-tidy helper command was not directly usable with keg-only LLVM 22: it needed an explicit binary, explicit check families, and an Xcode SDK sysroot. All failed attempts and the successful reproducible fallback are retained.
- Cppcheck and clang-tidy are broad signal sources, not test or proof substitutes. Task 2 triaged every project-owned diagnostic by counted family, but assigned subsystem gates still own semantic confirmation or dismissal.
- REL-002 prevents the registered server-list CTest result from proving which binary served the request on POSIX. Task 2 therefore records the raw result but accepts only the supplemental dynamic-port, foreground, live-child-verified runs as Release/sanitizer server evidence.
- Only native macOS arm64 build/test/analyzer baselines were run. Linux,
  Windows, and Android-device behavior has not been tested. Task 4's isolated
  browser configure passed, but compilation stopped at the documented unpatched
  SDL3_image/SDL3_mixer SDK boundary. Both audited client translation units did
  compile directly for WASM with warnings; no full link or browser runtime was
  tested. **Task 8 superseded the WASM build half of this limitation**: a
  disposable, port-patched Emscripten copy produced a complete link and all four
  artifacts. The browser runtime half stands unchanged.
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
- Task 7 Fix Round 2 reproduced BUG-045 at the `TTFText`/`std::vector`
  boundary, not through `HighscoreData` itself, which is defined inside
  `highscoremanager.cpp` and is unreachable from a test TU; no full-client
  single-player levelset completion was driven, so the step from the
  reproduced copy semantics to the missing on-screen labels rests on the
  static call-order trace. `SDL_GetTextureSize`'s output-write behavior is
  guaranteed by the readable SDL 3.4.4 submodule source and confirmed
  empirically on the linked SDL 3.4.10 runtime; the public header documents
  only the boolean return, so the property is implementation-verified rather
  than contract-guaranteed and could change in a future SDL release.
- Task 8 exercised **no browser runtime**. Loading an Emscripten
  `--preload-file` bundle requires an HTTP origin, and the audit's scope
  restriction forbids starting network listeners, so the WASM result is a
  successful build and artifact analysis only. WASM runtime status is
  **unavailable, not passed** — no page was rendered, no frame drawn, no console
  output collected — and brief Step 5's WebSocket-proxy connection was skipped
  by direction rather than attempted.
- Task 8 installed to and launched **no Android device or emulator**. The APK
  was built and its contents analyzed statically. BUG-046, the extraction
  ordering, the `Process.killProcess` teardown, the TV-remote input path, and
  the AdMob/Billing flows are therefore code-supported inferences, not observed
  facts. Whether `InterstitialAd.load` self-initializes despite the disabled
  `MobileAdsInitProvider` is deliberately unclaimed.
- Task 8 executed on **no Linux or Windows host**. The Linux `/proc/self/exe`
  branch, the Windows `GetModuleFileNameA` branch and its failure fallback, the
  `/usr/games`-style prefix gap, and the packaged DLL layout are source traces
  plus CI job definitions; only the macOS packaged-path cases were run. REL-008's
  non-macOS members are unreproduced for the same reason.
- Task 8 **never launched the shipped binary from a staged or bundle layout**,
  and **did not confirm dynamic-library independence** — both are brief Step 6
  substitutions, recorded in Fix Round 1. What ran instead is a test-only
  harness linking the unchanged production `platform.cpp.o` and `logger.cpp.o`;
  it exercises the real `InitDataDir()` and `Logger::Initialize()` (which is
  what REL-008 and BUG-047 rest on) but constructs no `FrozenBubble`, creates no
  window, loads no asset, and opens no real preference file. No packaged layout
  was checked for library resolution: the macOS host build links Homebrew SDL3
  by rpath, the Windows DLL copy and the AppImage bundling were read from the
  workflow only, and the Android `lib/<abi>/*.so` set was inspected inside the
  APK, never loaded. Whole-program packaged startup and dynamic-library
  independence are **unexercised, not passed**.
- Task 8 created **no keystore, signature, listener, socket, server, proxy,
  client connection, or killed process**, and ran **no sanitizer** — its harness
  linked the warnings-strict production objects because every Task 8 finding is
  a path-resolution or file-lifecycle question, so no leak or memory-safety
  claim is made anywhere in the gate. REL-007 was confirmed from the artifact
  and the workflow text alone. Per the user's scope restriction no
  security-specific runtime test was run: the dangling-symlink path disclosure,
  the repo-visible keystore password, the unpinned Gradle distribution, and the
  test AdMob identifiers are documented statically, and the omitted checks are
  limitations, not passes.

- Task 9 executed, triggered, or dispatched **no GitHub Actions workflow**.
  Every CI conclusion is a reading of `.github/workflows/build.yml` against
  documented Actions semantics. Specifically unexamined: which architecture
  `macos-latest` currently resolves to (REL-012 is therefore stated
  architecture-agnostically), whether `upload-artifact` preserves the AppImage
  executable bit through the itch.io path, which Emscripten release
  `version: 'latest'` resolves to, whether all 21 named MinGW DLLs exist on the
  runner, and the repository's default `GITHUB_TOKEN` scope. These are
  **unexamined, not passed**.
- Task 9 started **no container**. `docker compose config` is a local parse; no
  image was built, no service started, no port bound, and `docker compose up`
  was deliberately not run. `docker/Dockerfile`, `docker/nginx.conf` and the
  compose service wiring are static readings.
- Task 9 performed **no external network operation**: no release, no artifact
  upload, no itch.io channel, no server contact, no dependency download. Every
  version pin was read from a file; upstream tag existence and content were not
  confirmed.
- `python3 -c "import yaml"` is unavailable on this host, so the workflow was
  confirmed by a single YAML implementation (Ruby 2.6 Psych) rather than two.
  Two of the brief's Step 4 commands — the exact `ruby … aliases: true` and
  `./android/gradlew tasks --all --no-daemon` — exited 1 for host and CWD
  reasons; both exact commands and both accepted substitutes are recorded with
  exits, and neither exact failure indicates a repository defect.
- REL-010's destructive `openssl req -x509` branch was **not** executed against
  any certificate pair. Only its `key_ok` gate was reproduced, so the overwrite
  is a code-supported consequence of a reproduced predicate rather than an
  observed deletion. certbot's ECDSA default was taken from its documented
  behaviour since 2.0 and not verified by installing certbot.
- `netlify.toml`'s effective publish directory depends on Netlify site settings
  outside the repository, so that deployment question is **unresolvable from
  repository evidence** and was dismissed rather than promoted — not resolved in
  the project's favour.
- Per the user's scope restriction Task 9 ran **no security-specific runtime
  test**. The `@master` action pin holding a deploy secret, the repo-visible
  keystore password, the unverified MinGW downloads, the absent commit pins, and
  the mutable base-image tags are documented statically from the workflow text;
  the omitted supply-chain and credential-exposure checks are limitations, not
  passes.
- Task 9 read `CLAUDE.md` and `CHANGELOG.md` as evidence for REL-009. Neither is
  part of the 237-row coverage inventory, so REL-009's `CLAUDE.md` half is
  registered without a coverage row of its own.

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
  verified byte-identical after the gate. Fix Round 2 added
  `task7_fix2_harness.cpp`, its binary, and `fix2-getsize.log` /
  `fix2-ttfcopy.log` in the same directory; both runs exited 0 under the dummy
  video driver, started no process, and opened no preference file (the harness
  constructs no preference-owning singleton).
- Task 8 started no listener, server, client connection, proxy, or background
  process, and killed no process. Its two builds are the only lasting local
  state: the Gradle/CMake outputs under `android/app/build/`,
  `android/app/.cxx/` and `android/.gradle/` (already covered by
  `.gitignore:20-23`, and a Gradle daemon was explicitly refused with
  `--no-daemon`), and the new `build-audit-wasm/` tree at the repository root,
  which was added to the untracked `.git/info/exclude` **before** the build so
  it could never appear as drift. The disposable Emscripten copy, its
  port patches, its build cache, the packaged-path harness source/binary, the
  staged install and `.app` layouts, the isolated preference home, the log
  scenario directories, and all logs live under `/tmp/fb-sdl3-audit/task8/` as
  local regenerable evidence owning no external state; the 1.2 GB Emscripten
  clone is the largest and may be deleted freely. The Homebrew Emscripten
  installation was never modified, the four SDL submodules were never
  initialized or updated by this gate, and the user's three real preference
  files were hashed before and verified byte-identical after.
- Task 9 started no listener, server, client connection, proxy, container, or
  background process, and killed no process. It created one lasting local
  artifact inside the repository: the `build-audit-config/` CMake tree, added to
  the untracked `.git/info/exclude` **before** it was created so it could never
  appear as drift. Three read-only Gradle invocations ran with `--no-daemon`;
  a before/after SHA-256 manifest of all 134 tracked `android/` paths and of
  `git ls-files -s android` proved zero drift, so no path needed restoring. The
  disposable EC key, the `exec_program` probe script, the validator logs, and
  the Gradle manifests live under `/tmp/fb-sdl3-audit/task9/` as local
  regenerable evidence owning no external state; the EC key is a throwaway test
  key that protects nothing and was never used for anything. No preference file
  was opened, so none needed hashing. No production or test file was modified.
- Temporary files include the Task 1 inventory files and Task 2 analyzer logs/triage artifacts under `/tmp/fb-sdl3-audit/`; all are local, regenerable evidence and contain no credentials.
- Six generated audit build directories are now retained for Tasks 3 and 10
  (the four from Task 2, especially the sanitized server, plus Task 8's
  `build-audit-wasm/` and Task 9's `build-audit-config/`) and are locally
  excluded through untracked `.git/info/exclude` entries. Remove them after the complete audit; no listener
  or child process remains.
