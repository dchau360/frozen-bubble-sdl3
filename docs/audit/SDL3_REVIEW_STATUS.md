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
| Agent | Codex subagents `task_1_implementer` (bootstrap), `task_2_implementer` (baselines), `task_3a_static` (server static review), `task_3c_synthesis` (static Task 3 closure), `task_4_implementer` (client/synchronization review), and `task_5_implementer` (gameplay review), plus the Task 6 (lobby/settings/input), Task 7 (render/audio), Task 8 (platform ports), Task 9 (build/release/tooling), and Task 10 (cross-subsystem dynamic integration) implementer agents |
| Model | Unknown for Tasks 1-9; the dispatcher did not expose a model identifier. Task 10 ran as **Claude Opus**, exact model id `claude-opus-5` |
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

- Phase: Phase 5 complete — independent final challenge closed
- Active gate: Task 13
- Exact next action: Begin Task 13 — write `docs/audit/SDL3_COMPLETE_REVIEW.md`. Final synthesis is **authorized** by Task 12; see [Task 12 closure provenance](#task-12-closure-provenance) for the corrections the report must carry and the three residual limitations it must reproduce verbatim.

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
| Task 10 | Cross-subsystem dynamic integration matrix | complete (41 matrix rows recorded before any process launched — the original 38-row matrix's bundled row 38 was split in Fix Round 1 into 4 individually countable manual-observation rows; **31 executed, 10 recorded as not performed** — the six hostile-transport rows because the user restricted security-specific runtime testing, four manual visual/audio rows (clear-win banner and sound, spectator pinning, >5-player paging, malus/attack visuals) because no display, audio device or input-injection path existed). First gate to run real client code against a real server over a real socket: 24 `fb-server` instances (23 ASan+UBSan, 1 Release), each on its own dedicated port from a 24-port reserved range, driving rooms at 2/3/4/5/6/10/20 seats plus the 20-seat admission boundary and its boundary+1, normal/team/clear modes, rounds 1-3, member and creator departure both during and before a round, mid-game reconnect, `PART`/rejoin, and `SIGTERM` shutdown. Relay invariants re-derived from 11 saved journals: 4,990 frames delivered, 0 self-echoes, 0 unknown senders, all seat maps identical. Two defects and one improvement registered — **BUG-049** (High: recursive `player_part_game_` teardown frees the game and the outer frame reads it at `server/game.c:1051`; 11 identical ASan reproductions, minimum 3 seats, triggered by ordinary simultaneous disconnects, and **silent** on the uninstrumented Release build), **BUG-050** (Low: `LIST`'s `free:` counter contradicts its own open-player list) and **IMP-024** (unvalidated `CREATE` room cap). BUG-015 gained its first runtime reproduction; BUG-005 is recorded as entangled with BUG-049 and not independently measurable; BUG-013, BUG-021 and BUG-040 gained server-side runtime measurements with no severity change; three candidates dismissed with counter-evidence. No leak claim is made — Apple ASan requires `detect_leaks=0`. All 24 ports read free afterwards and the `fb-server` process list is byte-identical to the pre-launch baseline, so the four unrelated servers belonging to the user's own environment were neither touched nor counted. **Fix Round 1** corrected a false cleanup-proof claim and registered a fourth finding, **REL-015** (Medium): `fb-server`'s stats-file path is derived from `$HOME` unconditionally with no isolation mechanism, and this gate's own 24 launches — which isolated `joiners.log` by working directory but never set `HOME` — read from and wrote to the operator's real `~/.fb-server/stats.dat` for the entire gate, discovered in independent review rather than prevented; the off-by-one `networkclient.cpp:1306` citation was also corrected to `:1307`) |
| Task 11 | Complete file coverage and prioritized improvements | complete (237-row pinned-commit inventory reconciled exactly, diff-empty against a fresh regeneration of Task 1 Step 4's own selection command; **Fix Round 1** re-ran all eleven Step 2 cross-cutting categories as individually commanded, individually exited ledger rows against project-owned `src`/`server` — raw copies (3 hits), allocation/free (11 `malloc`/`calloc`/`realloc`, 44 `free(`), SDL create/destroy pairing (15 `SDL_Create*`, 37 `SDL_Destroy*`), hardcoded versions (reproduces REL-004's five), filesystem/`getenv(HOME)`/`SDL_GetPrefPath`/`SDL_GetBasePath` (5 hits, all already covered), **unchecked indices** (30 hits — one new candidate, **BUG-051**, an unguarded `std::array` write in `LoadLevelset`, unreachable with the shipped 10-line-per-block asset), **signed/unsigned conversions** (36 hits, all bounds-safe), **ignored return values** (19 statement-level hits, all idiomatic or already covered by BUG-033/BUG-047), **global/singleton lifetime** (33 `ptrInstance` hits, exactly matching IMP-007's four-singleton leak pattern plus two confirmed-safe outliers, `NetworkClient` and `FrozenBubble`), and **network lengths/opcodes** (46 hits, all covered by SEC-002/SEC-006/BUG-006 or independently verified safe in `ws_decode_inplace`) — the two previously-unevidenced categories that lacked even a real command now have one each; the **compile-guard sweep was rerun with a token-enumerating pattern and corrected**: seven guard tokens exist in `src` (`__ANDROID__`, `__ANDROID_PORT__`, `__WASM_PORT__`, `_WIN32`, `__APPLE__`, `__MINGW32__`, `__linux__`), not two, and only the first three are the ones CLAUDE.md's Platform abstraction section names — the four OS-detection macros in `platform.cpp` are real and correctly used but undocumented by CLAUDE.md, which is now stated honestly rather than claimed as an exact match; a mis-cited ledger command (`task6_settings_harness probe`, which could not match either probe row's actual text) was replaced with one that matches both; the `getenv("USER")` sweep was widened to the six sites the independent review found unswept, all dispositioned as benign bounded nickname fallbacks; and the "36/15/28" duplicate-platform-source-list counts were given their own reproducible per-file commands and reconfirmed unchanged. One stale "Investigating" candidate row in the server notebook corrected to its already-confirmed disposition, leaving zero open `suspected`/`investigating` states across the 97-ID registry; all 24 `IMP` entries confirmed to share one benefit/effort/risk vocabulary; all five Task 6/Task 4/Task 10 deferred items resolved with re-derived evidence, not re-deferred; notebooks 01-08 reconfirmed at ten headings each with an explicit gate conclusion and no unresolved action) |
| Task 12 | Independent final challenge | complete (independent challenger **Claude Opus**, model id `claude-opus-5`, fresh context. All **72** confirmed defects challenged on reachability, inputs, platform, impact and root cause: **62 upheld** with citations re-derived from the pinned source, **9 revised**, **0 dismissed** — no confirmed defect was a false positive. All **24** improvements challenged: 0 rejected, **2 revised** (IMP-013's clamp-site enumeration completed with `shaderstuff.cpp:1158`; IMP-021's transition driver corrected so its BUG-041 assertion can fail). All **43** explicit dismissal bullets in notebooks 01-08 read and 4 sampled in depth: **0 overturned**; BUG-012 stays dismissed and its ID stays retired. **One new defect: BUG-052 (High)** — `NetworkClient`'s receive-buffer append guard has no `else` and every `recvBufferLen` reduction sits inside its body, so a server line exceeding `BUFFER_SIZE` (4096) against the server's 16383-byte line ceiling puts the connection into a permanently deaf absorbing state on ordinary traffic. It came from an observation notebook 02 conceded at `:153` and set aside without ever opening it as a candidate. **One cross-notebook contradiction resolved** against the pinned source: notebook 04's trust table said "any room member can emit `SETOPTIONS`" where `server/game.c:405` enforces slot-zero creator authority, as notebook 01's own row correctly recorded — SEC-004 and SEC-007 re-scoped. **One Fix-Round correction reversed as itself wrong**: BUG-041's trigger set — `DoSnipIn` animates nothing, the leak is produced by `TakeSnipOut` at `bubblegame_render.cpp:1173`, and `QuitToTitle`'s `firstRenderDone` clear is what arms it, so menu return *is* a trigger. **One finding strengthened from static to observed**: REL-002, via a passive 1 d 21 h old orphan holding UDP 1511 and TCP 15113. **Four previously unstated reachability qualifications** recorded (BUG-002, SEC-001, SEC-002 — whose impact was also extended to a `bufsize == 0` / `recv` length `(size_t)-1` path — and SEC-005, which is *strengthened*, both documented launch paths passing `-l`). Coverage claim re-derived: 237/237 with an empty `diff`, 42 disposition strings summing to 237 with every class sampled (Fix Round 1 correction of a miscounted 41 — see [Task 12 Fix Round 1 Findings](#task-12-fix-round-1-findings)), 0 pending rows, 0 broken links and 0 broken anchors, notebooks 01-08 structurally conformant, registry with 0 duplicate IDs and 0 gaps. Started no process and killed none; security findings challenged **statically only** per the standing user restriction) |
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
  the only two `DoSnipIn` producers; menu return is not a trigger — **Task 12
  correction: this trigger-set claim was itself wrong. `DoSnipIn` animates
  nothing at all; the leak is produced solely by `TakeSnipOut`
  (`bubblegame_render.cpp:1173`), armed by `firstRenderDone == false`, which
  `QuitToTitle` clears — so menu return *is* a trigger. See
  [Task 12 confirmed findings](subsystems/09-final-challenge.md#confirmed-findings).**)
  and BUG-042
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
  ignores `HOME`: the first probe (13:38Z) resolved to the user's real
  preference directory, and the harness's own isolation gate aborted with exit
  4 before opening a file, so the directory and its three files were confirmed
  pre-existing only by listing and timestamps at that point, not by hash.
  `CFFIXED_USER_HOME` produced correct isolation at the next probe (13:39Z,
  `ISOLATION=OK`); a SHA-256 baseline of the three real files was then recorded
  immediately afterward (13:40Z), before the stateful 12-fixture matrix run
  (13:50Z), and every later run asserted `ISOLATION=OK` first. The baseline was
  verified byte-identical at 13:55Z. (Task 11 correction: "hashed beforehand"
  previously implied the hash predated the 13:38Z isolation probe itself; the
  ledger shows the hash was recorded only after that probe succeeded, matching
  the notebook's more precise "verified by listing and timestamps" wording for
  the probe window and "re-verified byte-identical after the gate" for the
  13:40Z→13:55Z window it actually protects.)
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
  `firstRenderDone` with no `DoSnipIn`, so menu return produces no animation —
  **superseded in Task 12: this correction was itself backwards. `DoSnipIn`
  calls no `effect()` and animates nothing regardless of caller; the leak comes
  from `TakeSnipOut` (`bubblegame_render.cpp:1173`), which runs whenever
  `firstRenderDone == false`, and `QuitToTitle` clearing that flag is exactly
  what arms it — so menu return *is* a BUG-041 trigger. See
  [Task 12 confirmed findings](subsystems/09-final-challenge.md#confirmed-findings).**);
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

## Task 10 closure provenance

- **Agent and model.** The Task 10 implementer ran as **Claude Opus** (exact
  model id `claude-opus-5`), dispatched by the audit controller. Earlier gates
  ran under agents whose model the dispatcher did not expose; this is the first
  row in this file that records one.
- **The matrix was written before any process was launched.** All 41 rows —
  setup, expected observable, log evidence, sanitizer state, whether manual
  visual/audio observation is required, and result — are in the
  [Task 10 notebook](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched)
  (Fix Round 1 split the original matrix's bundled row 38 into four
  individually countable manual-observation rows, 38-41, so every row's
  Result is directly countable). **31** were executed and **10** are recorded
  as not performed.
- **Brief Step 5 was not executed, by standing user restriction.** The user
  explicitly restricted security-specific runtime testing for this audit. All
  six hostile transport scenarios — fragmentation/coalescing, mid-frame
  disconnect, frames claiming a foreign player id, duplicated `n`/`F`/`S`
  frames, reordered `b`/`N`/`T` sync frames, and bounded flooding — are matrix
  rows 32-37 with the result "not performed", exactly as Task 3's runtime
  security matrix and Task 6's security runtime rows were handled. They are
  limitations, not passes, and they are not counted as executed work.
- **BUG-049 was not found by hostile testing.** It surfaced during ordinary
  scenario teardown, when the seats of a playing room closed their sockets at
  about the same time. It was characterised by varying exactly one functional
  parameter, the seat count (2 passes, 3/4/5 fail), and by re-running the same
  input against the uninstrumented Release binary to prove it is not a
  sanitizer artefact. No attempt was made to steer the freed allocation or to
  reach the `stats_record_win` dangling-pointer path; that consequence is a
  code-supported argument, not a reproduction, and the notebook says so.
- **Isolation was proven before, not after.** `CFFIXED_USER_HOME` and `HOME`
  were both pointed at a fresh temp directory and `SDL_GetPrefPath("",
  "frozen-bubble")` was resolved and printed *before* any client process ran;
  the path was inside the temp root. The client then wrote exactly three files
  there, and the user's real preference files still carry their 2026-07-28
  08:59:49 mtimes.
- **Dedicated ports only.** **24** ports were used — `25610`-`25623`,
  `25626`-`25631`, `25640`-`25643` — each confirmed free before binding. Port
  1511 was never touched and no UDP listener was ever created (`-l`/`-L` were
  never passed). Four unrelated `fb-server` processes belonging to the user's
  own environment (ports 15511, 15512, 15113, 15998) were enumerated *before*
  the first launch; `diff` against that baseline after the last row produced no
  output, and `ps -o etime` shows all four started 1-4 days earlier.
- **No leak claim.** Apple's ASan rejects `detect_leaks=1`, so every run used
  `detect_leaks=0`. These runs establish the absence of the memory errors ASan
  and UBSan actually check; they say nothing about leaks.
- **Quantities were re-derived, not carried over.** The 4,990 relayed frames,
  0 self-echoes, 0 unknown senders, 11-of-11 identical seat maps, 11-of-24
  sanitizer reports, 24 ports, 237 coverage rows, and the 92→95 finding-ID
  count each come from a command that measures that claim; those commands are
  in the ledger below.
- **The audit tooling is not production source.** `fbharness.py`,
  `scenario.py`, `run_case.sh` and `task10_netclient_harness.cpp` live under
  `/tmp/fb-sdl3-audit/task10/`. Nothing under `src/`, `server/`, `tests/` or
  `tools/` was created, modified, or reformatted; the production tree remains
  byte-identical to `09d6c7bf`.
- **What this gate could not do.** It never drove the shipped client through
  its menus into a network game — SDL's dummy video driver accepts no injected
  input and `src/main.cpp:27` takes no arguments — so the client layer is
  covered in two pieces (a whole-program startup/shutdown smoke and a
  production-object network harness) that together do not equal one
  human-driven session. It exercised macOS only, raw TCP only, and observed no
  pixel or sound. All eleven limitations are enumerated in the notebook.

### Task 10 Fix Round 1

An independent review of commit `f297dfc1` raised two Critical, one Important
and one Minor finding. **All four were accepted; none was disputed.**

- **Critical: the cleanup proof's "nothing outside `/tmp/fb-sdl3-audit/` was
  written except the tracked audit documents" claim was false.**
  `server/stats.c:82-91`'s `stats_init()` derives `stats_file_path` from
  `getenv("HOME")` unconditionally — a mechanism independent of the server's
  working directory, unlike `joiners.log`. `run_case.sh` never set `HOME` for
  any of its 24 launches, so every `fb-server` instance this gate started
  read from and wrote to the real `/Users/dchau/.fb-server/stats.dat`.
  Verified directly against the pinned baseline and the preserved evidence
  (ledger rows below) rather than taken on the reviewer's word. The false
  claim is withdrawn and replaced with an accurate one in the notebook's
  cleanup proof; the real file was left exactly as found. A new Limitation
  (11) and finding (**REL-015**, Medium) are registered — `fb-server` has no
  flag, cwd-relative path, or `HOME`-independent override to relocate this
  file, which makes sandboxed or CI testing of the server structurally unsafe
  against a real host's state.
- **Critical: the "28 executed / 10 not performed" figure did not reconcile
  with its own table.** The 38-row matrix had exactly 31 rows with a concrete
  Pass/Fail/Mixed verdict and 7 rows reading "Not performed" (rows 32-37 plus
  a row 38 that bundled four manual visual/audio observations into one row);
  28/10 only followed by silently reclassifying rows 11, 12 and 14 as
  not-performed for their embedded visual sub-component while their headline
  Result still read a qualified "Pass", which also double-counted against row
  38's own bundled list. Fixed by splitting row 38 into four individually
  countable rows (38-41: clear-win banner and sound, spectator pinning,
  >5-player paging, malus/attack visuals) and re-deriving the count directly:
  **41** total rows, **31** executed (rows 1-31, none reclassified), **10**
  not performed (rows 32-41). The "four manual visual/audio observations"
  list is now identical everywhere — the notebook's matrix, its Limitations
  section 4, `FINDINGS.md`, this file, and the task-10 report all name the
  same four items; "full menu navigation into a network game" is no longer
  listed among them, since it is a distinct, already fully-documented
  omission (Limitation 5, about the absence of an input-injection path, not
  about display/audio observation).
- **Important: the `networkclient.cpp:1306-1360` citation was off by one.**
  `ParseListResponse` begins at `:1307` at the pinned baseline, not `:1306`.
  Corrected everywhere it appeared: the notebook (two places), `FINDINGS.md`'s
  BUG-050 entry, and the task-10 report.
- **Minor: the Coverage table over-credited Task 6.** Its "room lifecycle
  transitions... driven end to end" credit is true only at the protocol
  level, performed by this gate's own harness — not by Task 6's
  `mainmenu_netpanel.cpp` UI, which the notebook's own Limitation 5 already
  says was never exercised. Reworded so the two statements no longer
  contradict each other.

Full findings-registry text for REL-015 is in
[FINDINGS.md](FINDINGS.md); the notebook's own corrections are in its
[Cleanup proof](subsystems/08-dynamic-integration.md#cleanup-proof-step-6),
[Limitations](subsystems/08-dynamic-integration.md#limitations) and
[Confirmed findings](subsystems/08-dynamic-integration.md#new-reliabilitydeployment-defect-fix-round-1)
sections.

## Task 11 closure provenance

- **Agent and model.** The Task 11 implementer ran as **Claude Sonnet 5**
  (model id `claude-sonnet-5`), dispatched by the audit controller.
- **Step 1 — inventory reconciliation.** Task 1 Step 4's exact selection
  command was re-run against the same pinned commit
  `09d6c7bfcd864a0ad3951b87d16a88dc770392a3`: **237** paths, `diff`-identical
  (exit 0, no output) to `FILE_COVERAGE.md`'s 237 rows. No path was added,
  removed, or found missing an evidence/exclusion disposition.
- **Step 2 — cross-cutting searches (as corrected in Fix Round 1; see below).**
  The original gate ran and recorded only six of the brief's eleven named
  categories with a real command; five — unchecked indices, signed/unsigned
  conversions, ignored return values, global/singleton lifetime, and network
  lengths/opcodes — were claimed complete with no supporting command anywhere.
  Fix Round 1 ran all eleven against project-owned `src/`/`server/` (vendored
  SDL/iniparser internals excluded per the audit's standing boundary), each as
  its own ledger row with its own real exit code (full table:
  [Task 11 Fix Round 1 ledger](#task-11-fix-round-1-ledger)). Results: raw string
  copies (`strcpy|strcat|sprintf|gets`) — 3 hits, bounds-safe, one a comment;
  allocation/free (`malloc|calloc|realloc`, `free\(`) — 11 and 44 hits, mapping
  to Task 3's server allocation-owner table and Task 7's render ownership
  table; SDL create/destroy pairing (`SDL_Create[A-Za-z]+\(`,
  `SDL_Destroy[A-Za-z]+\(`) — 15 and 37 call sites, matching Task 7's
  exhaustive ownership table; hardcoded version literals — reproduces exactly
  REL-004's already-catalogued five (`2.2.1`, `2.4.9`, `v2.4.26`, `2.4.27`,
  `0.1.0`) among IP-address and third-party-dependency-pin noise;
  filesystem/`getenv` assumptions (`getenv\("HOME"\)|SDL_GetPrefPath|SDL_GetBasePath`)
  — 5 hits, all already covered by REL-015, REL-008, and Task 6's persistence
  review; **unchecked indices**
  (`\[(team|scancode|senderId|playerId|slot|idx|index|cellIndex|button|sc)\b[^]]*\]`)
  — 30 hits, 29 bounds-safe (loop-bounded, `%`-wrapped, or already-registered
  under BUG-035/BUG-036/SEC-007) and **one new candidate**: `BubbleGame::LoadLevelset`
  (`src/bubblegame_level.cpp:67`) writes `level[idx]` into a fixed
  `std::array<std::vector<int>, 10>` with no `idx < 10` guard, unlike the
  sibling `HighscoreManager::LoadLevelsetHighscores` loader for the same file
  format, which does guard it — registered as **BUG-051** (Low; the shipped
  `data/levels` asset cannot trigger it, confirmed by a line-count showing all
  100 blocks are exactly 10 lines); **signed/unsigned conversions**
  (casts to `unsigned`/`size_t`/`uintN_t`) — 36 hits, all bounds-safe pointer-
  difference or byte-extraction patterns already within IMP-002/REL-001/SEC-002/
  SEC-006/BUG-035/BUG-036's territory; **ignored return values** (statement-level
  `system|fwrite|fread|write|read|remove|rename|fclose|unlink|SDL_RenderCopy|SDL_RenderPresent|chdir|mkdir`)
  — 19 hits, the `system()` and `Logger::Initialize` ones already BUG-033/
  BUG-047, the remaining `fclose`/`mkdir`/`SDL_RenderPresent` ignores are
  idiomatic with no established resource or security consequence beyond what
  REL-015 already characterizes for `ensure_stats_dir`'s `mkdir`; **global/
  singleton lifetime** (`ptrInstance\b`) — 33 hits across the six singletons
  with `ptrInstance` fields, exactly reproducing IMP-007's pattern: `AudioMixer`,
  `TransitionManager`, `HighscoreManager`, and `GameSettings`'s `Dispose()`
  methods free members but never null `ptrInstance` (IMP-007's already-registered
  defect), while `NetworkClient::Dispose()` correctly nulls it after `delete`
  and `FrozenBubble` has no `Dispose()` at all (a true program-lifetime
  singleton) — both confirmed as the non-matching, safe outliers, no new
  candidate; and **network lengths/opcodes**
  (`plen|hdr_len|Content-Length|content_length|opcode|recvBufferLen|msgLen|payload_len|framelen|frame_len`)
  — 46 hits, the `Content-Length` site already SEC-002, the client `msgLen`/
  `recvBufferLen` framing already within BUG-006/SEC-006's territory, and
  `server/ws.c`'s `ws_decode_inplace` frame-length/opcode handling (`plen`,
  `hdr_len`) independently re-read line-by-line and confirmed safe: the
  64-bit-length case is rejected (`plen == 127` returns `-1`), the extended
  16-bit length is read before use, and the frame-completeness check
  (`total - pos < hdr_len + plen`) gates the `memmove` that follows it — no new
  candidate. TODO/FIXME/XXX/HACK markers exist only inside vendored
  `org/libsdl/app/` files, out of scope. **One new candidate (BUG-051) was
  opened and confirmed; the other ten categories closed with no new
  candidate.**
- **Step 3 — candidate registry.** One stale "Investigating" candidate row
  survived in `subsystems/01-server-protocol.md` (IMP-010), left over from
  before Task 7 closed its cross-owner half; corrected to "Confirmed
  improvement (cross-owner disposition completed in Task 7)". After the fix,
  the only remaining occurrences of `suspected`/`investigating` anywhere under
  `docs/audit/` are `FINDINGS.md`'s own rule-definition sentence — the same
  self-referential pattern `FILE_COVERAGE.md` already uses for "pending". The
  96-ID registry (BUG-001..050, SEC-001..007, REL-001..015, IMP-001..024)
  remains unique and contiguous per class; no ID was renumbered or recycled.
- **Step 4 — severity/confidence normalization.** Confidence is uniformly
  `High` across all 96 rows by this audit's established convention; the
  observed-runtime-fact vs. complete-causal-proof vs. weaker-inference
  distinction Step 4 asks for is already carried in each row's Summary text
  (e.g. "reproduced under UBSan" vs. "code-supported inferences" vs.
  "runtime/security reproduction was not performed by user direction") and
  was spot-checked, not rewritten. Severity values are exactly
  `Critical`/`High`/`Medium`/`Low`/`-` (the last only on `dismissed` rows) —
  no stray values found.
- **Step 5 — improvement ranking.** All 24 `IMP` entries already carry a
  `{Low,Medium,High} benefit / {Low,Medium,High} effort / {Low,Medium,High}
  risk` triple in one consistent vocabulary (verified by a parser that checks
  every row against that exact pattern: 24/24 conforming). No entry needed
  re-normalizing and no speculative entry was added to fill an empty
  category; several categories (e.g. a dedicated "developer experience" IMP
  distinct from diagnostics/testing) have no dedicated entry because no
  gate's evidence supported one as a separate recommendation.
- **The five deferred items were each resolved, not re-deferred:**
  1. Task 6's runtime-reproduction count was six in `FINDINGS.md` and the
     notebook's gate conclusion, eight in `SDL3_REVIEW_STATUS.md`. Re-derived
     from the notebook's own persistence-matrix and full-client-run tables:
     the true count is **eight** (BUG-026 through BUG-032, BUG-034).
     `FINDINGS.md` and the notebook were corrected to read eight; this file's
     existing "Eight of these" was already correct and is unchanged.
  2. BUG-035's notebook entry stopped at "the derived code reaches ≥512,
     past the 512-entry array `SDL_GetKeyboardState` returns" without tracing
     how an out-of-range virtual scancode reaches a player's stored binding
     in the first place. Completed with the missing hop: bind-capture
     (`frozenbubble.cpp:384-388`) → `PushScancode`'s raw-event fallback,
     which bypasses the virtual-range guard entirely once the code leaves
     `[300,400)` (`frozenbubble.cpp:334-347`) → `KeysPanelKey`'s unchecked
     store into `PlayerKeys` (`mainmenu_input.cpp:498-513`) → `IsKeyPressed`'s
     unguarded `SDL_GetKeyboardState(NULL)[sc]` index
     (`gamesettings.h:50-54`), verified line-for-line against the pinned
     source.
  3. The Task 6 closure-provenance and processes/cleanup wording said the
     user's three real preference files were "hashed beforehand," which
     overstates protection during the risky 13:38Z isolation probe itself —
     the ledger shows the hash was recorded at 13:40Z, after both the failed
     13:38Z probe and the successful 13:39Z one. Corrected to name what
     actually protected each window: listing/timestamps before and during the
     probe, a hash immediately after it succeeded, and byte-identical
     re-verification after the later stateful matrix run — matching the
     notebook's already-accurate "verified by listing and timestamps"
     wording. Task 7's and Task 8's identically worded sentences were checked
     against their own ledgers and left unchanged: Task 7's hash row
     genuinely precedes its isolation-probe row.
  4. A Task 4 fix-round link/schema validator's failing run was recorded with
     its exit code and output but not its exact command text — only the
     corrected re-run's text survives in `task-4-report.md`. The underlying
     stale anchor was already fixed before this gate, so the original failure
     cannot be reproduced without reintroducing it; documented as a closed
     historical gap in `subsystems/02-network-client-sync.md`'s Limitations,
     alongside today's re-run of the equivalent check (exit 0, still passing).
  5. Notebook 08's Limitations item 4 canonical four-item list of unobserved
     visuals (clear-win banner/sound, spectator pinning, >5-player paging,
     malus/attack visuals) did not name row 11's parenthetical "on-screen team
     banner" gap. Added to the list; confirmed it does not change the
     41/31/10 matrix count, since row 11 is already one of the 31 executed
     rows.
- **No production source was touched.** Every edit in this gate is confined
  to `docs/audit/`; the final drift check below confirms it.

### Task 11 Fix Round 1 Findings

An independent review of commit `03421bb3` raised one Critical, two Important
and two Minor finding; **all five were accepted, none disputed.**

- **Critical — five of Step 2's eleven cross-cutting categories had no
  supporting command anywhere.** Unchecked indices, signed/unsigned
  conversions, ignored return values, global/singleton lifetime, and network
  lengths/opcodes were asserted complete (in the gate-checklist sentence, the
  closure-provenance narrative, and the report) with zero grep, pattern, hit,
  or reconciliation on record. Fixed: all eleven categories, including the six
  that already had real evidence, were re-run as individually commanded,
  individually exited rows in the
  [Task 11 Fix Round 1 Ledger](#task-11-fix-round-1-ledger) below, with the
  exact pattern stated for each so it can be re-run. The unchecked-indices
  sweep surfaced one genuine new candidate — **BUG-051** — registered in
  `FINDINGS.md`, `subsystems/03-gameplay.md`'s Candidates and Confirmed
  findings, and `FILE_COVERAGE.md`'s `bubblegame_level.cpp` row; the other four
  previously-missing categories, and the six that already had partial
  evidence, closed with no new candidate, each with its stated pattern and
  count. No hostile-traffic or runtime security probing was performed for the
  network lengths/opcodes category, per the user's standing restriction; its
  disposition (`ws_decode_inplace`'s frame-length handling) is static
  line-by-line reading only.
- **Important — the compile-guards claim was false.** The gate claimed only
  `__ANDROID__` and `__WASM_PORT__` appear as `#if`/`#ifdef` targets in `src/`,
  contradicted by its own cited evidence: `gamesettings.h:86` reads
  `#if defined(__ANDROID__) || defined(__ANDROID_PORT__)`, and CLAUDE.md's own
  Platform abstraction section names `__ANDROID_PORT__` explicitly. Fixed: a
  token-enumerating sweep
  (`grep -rhoE '#\s*(if|ifdef|ifndef|elif)\b.*' src | grep -oE '__[A-Za-z0-9_]+__|_WIN32|__linux__' | sort -u`)
  finds **seven** guard tokens, not two: `__ANDROID__`, `__ANDROID_PORT__`,
  `__WASM_PORT__`, `_WIN32`, `__APPLE__`, `__MINGW32__`, `__linux__`. Only the
  first three are the ones CLAUDE.md's Platform abstraction section names; the
  other four are standard OS-detection macros used in `platform.cpp` for
  asset/path resolution (`_WIN32`/`__MINGW32__` at `:22,98`, `__linux__` at
  `:24,122`, `__APPLE__` at `:109`) and in `mainmenu_server.cpp`'s local-server
  stubs (`_WIN32`) — real, correctly used, and simply not named by CLAUDE.md's
  Platform abstraction paragraph, which is a narrower documentation gap, not a
  functional defect. The false "matches CLAUDE.md's documented guard set
  exactly" claim is removed everywhere it appeared (the Step 2 narrative above
  and the gate-checklist sentence); the true set and the true (partial)
  overlap with CLAUDE.md are now stated directly.
- **Important — a ledger row's command did not support its own claimed
  result.** The row with command
  `grep -n "task6/real-prefs-baseline\|task6_settings_harness probe"` claimed
  to confirm the Task 6 13:38Z (exit 4) and 13:39Z (`ISOLATION=OK`) probe rows,
  but neither alternation term matches either row's actual command text (both
  contain `task6_settings_harness` followed by a path argument, then a
  trailing ` probe` argument — `task6_settings_harness probe` as a contiguous
  substring never occurs). Fixed: replaced with
  `grep -nE '2026-07-28T13:3[89]Z' docs/audit/SDL3_REVIEW_STATUS.md`, verified
  to match exactly the two probe rows and nothing else (re-run: exit 0, two
  lines, `:1528` and `:1529`, formerly `:1404` and `:1405` before Fix Round
  1's insertions shifted this file). See the
  [Task 11 Fix Round 1 Ledger](#task-11-fix-round-1-ledger) for the re-run.
- **Minor — the filesystem/`getenv` sweep was narrower than its own claimed
  scope.** The Step 2 closure text framed the sweep as covering
  "filesystem/`getenv` assumptions" broadly, but it was scoped only to
  `getenv("HOME")`, `SDL_GetPrefPath`, and `SDL_GetBasePath`, missing six
  `getenv("USER")` sites (`mainmenu_netpanel.cpp:70,1010,1012,1092,1094`,
  `mainmenu_input.cpp:1495`). Fixed: widened with
  `grep -rn 'getenv("USER")' src` (6 hits, exit 0) and inspected each; all six
  follow the identical pattern
  `getenv("USER") ? getenv("USER") : "<fallback>"` feeding a bounded
  `snprintf` into a fixed buffer as a nickname default — benign, no new
  candidate. The Step 2 narrative above now names both `getenv` patterns
  actually searched rather than describing the narrower one broadly.
- **Minor — the "duplicate platform source lists" 36/15/28 counts were
  unreproducible.** No ledger row recorded the command or filtering logic
  behind them. Fixed: three individually commanded, individually exited rows
  in the [ledger](#task-11-fix-round-1-ledger) reproduce all three counts
  unchanged, with the filter stated precisely — comment lines excluded, then
  literal `.cpp` occurrences counted: `grep -vE '^\s*#' CMakeLists.txt | grep -oE '\.cpp' | wc -l`
  → **36** (a naive `grep -oE '\.cpp' CMakeLists.txt | wc -l` over the same
  file returns 37, the extra one being `.cpp` inside line 55's comment);
  `grep -oE '\.cpp' CMakeListsEmscripten.txt | wc -l` → **15** (no comment
  lines contain `.cpp` in this file); `grep -oE '\.cpp' android/app/CMakeLists.txt | wc -l`
  → **28** (same). No number differed from what was previously stated; only
  the reproducing command was missing.

## Task 12 closure provenance

Independent final challenge. Reviewer: **Claude Opus**, exact model id
`claude-opus-5`, dispatched with a fresh context and given only `CLAUDE.md`, the
approved design, the plan, this status file, `FILE_COVERAGE.md`, `FINDINGS.md`,
notebooks 01-08, the pinned production tree, and `/tmp/fb-sdl3-audit/`. Full
record in [subsystems/09-final-challenge.md](subsystems/09-final-challenge.md).

**Scope executed.** All 72 confirmed defects (BUG-001..051 less the dismissed
BUG-012, SEC-001..007, REL-001..015); all 24 improvements; all 43 explicit
dismissal bullets across notebooks 01-08; all 8 cross-subsystem categories the
plan's Step 4 names; and the complete-coverage claim, re-derived rather than
re-read. Every quantity relied on was re-measured with a command that measures
the claim.

**Defect dispositions.** 62 upheld, 9 revised, 0 dismissed. No confirmed defect
was found to be a false positive. The nine revisions:

| ID | Revision |
|---|---|
| BUG-002 | Reachability qualified — `l0()`/`exit()` unsafe in every configuration, but `unregister_server`'s DNS/HTTP/allocation members are `!quiet && !lan_game_mode`-gated and both documented launch paths pass `-q` and `-l` |
| BUG-041 | **Fix Round 1's trigger correction reversed.** `DoSnipIn` (`transitionmanager.cpp:48-60`) calls no `effect()` and animates nothing; the leak is produced by `TakeSnipOut` (`:62-75`) at its sole call site `bubblegame_render.cpp:1173`, armed by `!firstRenderDone`, which is cleared at `bubblegame.cpp:1013`, at `bubblegame.cpp:1363` (**`QuitToTitle`**) and by the `bubblegame.h:479` initializer. Clearing the flag is what schedules the animation, so menu return **is** a trigger. Also `gfxLevel() <= 2`-gated and compiled out on WASM |
| SEC-001 | Reachability qualified — `-u user` required, inside `daemonize()`; no documented launch path passes `-u` and Docker's `-d` skips `daemonize()` entirely |
| SEC-002 | Reachability qualified (all three `http_get` sites `!quiet`-gated; both launch paths pass `-q`) **and impact extended** — a `Content-Length` wrapping to exactly `-1` gives `bufsize == 0` and a `recv` length of `(size_t)-1` at `net.c:1253`, i.e. an attacker-length write past a zero-size allocation, not only the `malloc_` `exit()`. Static argument; no exploit attempt |
| SEC-004 | `OPTIONS:` half corrected — `setoptions` **does** enforce slot-zero creator authority (`server/game.c:405`, `wn_not_creator` at `:415`). The verbatim binary relay half (`game.c:962-979`) is upheld exactly |
| SEC-005 | Upheld and **strengthened** — reachability established for the first time: the UDP listener needs `-l`/`-L`, and both `start-server.sh:68` and `docker/Dockerfile:31` pass `-l`, so the over-read path is live in every documented deployment |
| SEC-007 | Threat model corrected — the sender must be the **room creator**, not "any room member". The defect chain itself was fully re-derived and confirmed |
| REL-002 | Strengthened from static to **observed**, with a named consequence — an orphan holding UDP 1511 denies LAN hosting to every later `fb-server -l` via `create_udp_server`'s `exit(EXIT_FAILURE)` |
| REL-009 | Extended with a third `CLAUDE.md` drift — `:54`'s `bubbleArrays[5]` / "1–5 players" against a 20-element array and room sizes `{5, 10, 20}` |

**New finding.** **BUG-052 (High, confidence High)**: `ProcessIncomingData`'s
append guard `if (recvBufferLen + received < BUFFER_SIZE)`
(`src/networkclient.cpp:843`) has no `else`, and every statement that reduces
`recvBufferLen` (`:894`, `:914`, `:916`) lives inside its body, so once the
4096-byte buffer fills the connection is permanently deaf while the socket stays
open. The server emits single lines up to 16383 bytes (`server/net.c:126-146`;
`list_games_str` is `[16384]`, `server/game.c:134`) and the `LIST` reply grows by
up to 25 bytes per lobby connection (`game.c:139-151`) against a `max_users`
default of 255 (`net.c:82`), so ~165 idle clients suffice with **no** malformed
input. Consequence: the lobby stops updating and every push is dropped; in
`IN_GAME` every relayed peer frame is dropped and boards diverge silently. Not
memory corruption. **Not reproduced** — reproduction needs either ~165
concurrent connections or a deliberately over-long line, the latter barred by the
security-runtime restriction. Origin: an observation notebook 02 conceded at
`:153` ("a chunk that would fill the buffer is silently skipped") and disposed of
with "Ordinary server messages fit the buffer … Overflow/flood behavior remains
part of the untrusted-input limitation" — never opened as a candidate, so the
"every candidate is confirmed or dismissed" completion condition never reached
it. Both halves of that disposition were falsified.

**Improvement dispositions.** 0 rejected. IMP-013 revised (fourth clamp site
`shaderstuff.cpp:1158`). IMP-021 revised (its transition driver must clear
`firstRenderDone` and call `TakeSnipOut`; driving `DoSnipIn` animates nothing, so
as specified its BUG-041 assertion could not fail).

**Cross-subsystem sweeps.** 8 of 8 executed. Length mismatches produced BUG-052.
Lobby-to-game option drift produced no fourth drift beyond BUG-018, BUG-021 and
SEC-007 — `VICTORIESLIMIT` **is** propagated and enforced on the network path.
Player-ID mapping, round resets, renderer/audio teardown, platform source-list
drift, package/runtime path drift, and settings/input effects produced nothing
beyond the existing registry; four invariants were re-derived and hold (seat-id
window, `playerCount`-bounded peer fan-out, `bubbleArrays[20]` capacity,
`IsVirtualScancode`'s `[300,400)` window, single renderer create/destroy pair).

**Coverage-claim audit.** 237/237 inventory reconciliation with an empty `diff`;
42 disposition strings summing to 237 with every class sampled (Fix Round 1
correction of a miscounted 41 — see
[Task 12 Fix Round 1 Findings](#task-12-fix-round-1-findings)); `pending` appears
once, in its own rule paragraph; 0 broken links and 0 broken anchors across all
audit documents; notebooks 01-08 carry exactly ten headings each, once, in order;
registry has 97→**98** IDs with 0 duplicates and 0 gaps and states 97 `confirmed`
+ 1 `dismissed`. Two honest qualifications were added to `FILE_COVERAGE.md`:
only three of the design's five disposition classes are used (blocked *checks*
live in Limitations, not on rows), and six in-scope maintained paths carry no row
because Task 1 Step 4's pattern does not match them — `CLAUDE.md`,
`CHANGELOG.md`, `.gitignore`, `.gitmodules`, `COPYING`, and the four
`third_party/iniparser/*` files, five of which are cited as evidence for
REL-006, REL-009 or REL-014.

**Authorization.** Every issue this gate raised has been applied to the registry,
the affected notebooks, and the coverage ledger, and no challenge issue remains
open. **Task 13 is authorized.** The report must carry BUG-052 at High, the nine
revisions in their corrected form, and the three residual limitations verbatim:
(1) no security-specific runtime testing was performed anywhere in this audit, so
SEC-001..007 remain code-supported inferences and SEC-002's extended
heap-overflow consequence in particular is unresolved rather than resolved;
(2) BUG-052 is unreproduced; (3) the "conceded-and-set-aside inline observation"
class was sampled by a bounded language sweep (6 passages found, 1 concealing a
High defect), not swept exhaustively, so it stands as the residual risk with the
highest demonstrated yield in this audit. The report must not restate the two
propositions this gate dismissed: that a non-creator can push room `OPTIONS:`,
and that `DoSnipIn` produces the transition animation.

### Task 12 Fix Round 1 Findings

An independent review of commit `0c2aa97a` raised three Important and two
Minor findings; **all five were accepted, none disputed.**

- **Important — disposition-census miscount (41 stated, 42 actual).** The
  census in `FILE_COVERAGE.md`, this gate's notebook (twice — the Step 5
  narrative and the Coverage section), and both places in this file (the
  gate-checklist row and this closure-provenance section) all stated **41**
  distinct disposition strings summing to 237. Re-running the same command
  (see the [ledger](#task-12-fix-round-1-ledger) below) finds **42**, still
  summing to **237** — the sum was never wrong. Root cause: the census was
  tabulated *before* this same gate's own edit to the `src/networkclient.cpp`
  row (made to register BUG-052) landed in the same commit; that edit's new
  disposition text, "Complete; native baseline, direct WASM compile, Task 10
  live-socket runtime, and Task 12 length-boundary re-review", is a singleton
  class the pre-edit tabulation never saw. Fixed in all five places, each now
  reading **42**. The "every class sampled" claim is otherwise intact but was
  necessarily one class short of complete for the same reason; the missed
  class — the `src/networkclient.cpp` row itself — was sampled in this fix
  round by re-deriving its BUG-052 citations (`:843`, `:894`, `:914`, `:916`)
  against the pinned source, and they hold exactly as stated.
- **Important — ledger-row undercount in the gitignored `task-12-report.md`
  (20 stated, 36 actual).** Section 7's "Corrections applied" table stated
  "20 ledger rows" were added to this file's
  `### Task 12 independent final challenge` section. A direct count (see the
  ledger below) finds **36** timestamped rows in that section. Section 2's
  table, separately, lists **37** numbered commands — a different, larger
  count than the 36 ledger rows, because it additionally includes the final
  `git add`/`git commit` (item 36) and the post-commit production-drift
  verification (item 37), neither of which could be logged as a timestamped
  ledger row inside the file being committed. Fixed in `task-12-report.md`
  (gitignored, so this correction is not part of this commit) with both
  figures stated and reconciled.
- **Important — the BUG-041 reversal was not propagated into two historical
  narrative sections.** `subsystems/05-render-audio.md:239` (approximate) and
  `FINDINGS.md:90` correctly annotate that Task 12 reversed Task 7 Fix Round
  1's claim that `DoSnipIn` is the only trigger and that menu return is not
  one. This file's "Confirmed findings" summary (the BUG-041 bullet under the
  Task 7 paragraph) and its own "Task 7 Fix Round 1" section still asserted
  the falsified claims verbatim, with no annotation. Fixed: both now carry an
  inline "Task 12 correction"/"superseded in Task 12" parenthetical, following
  the model already used for the Task 6 "hashed beforehand" parenthetical (see
  Task 6 closure provenance) and the REL-013 "Undercounted — corrected in Fix
  Round 1" ledger rows — the original claim is left in place, not rewritten,
  and a pointer to
  [Task 12 confirmed findings](subsystems/09-final-challenge.md#confirmed-findings)
  is added; that anchor was checked to resolve unambiguously (see ledger).
- **Minor — unevidenced "4 sampled in depth" dismissal claim.** The
  notebook's Coverage section and this file's gate-checklist/closure-provenance
  prose stated "4 sampled in depth" without naming them, even though
  Step 5b of the same notebook already names and evidences all four (BUG-012,
  `idleSPButtons` `SDL_GetTextureSize`, `cmake_uninstall.cmake.in`
  `exec_program`, `netlify.toml` publish-directory). Fixed: the notebook's
  Coverage bullet now names the four and points to Step 5b's one-line record
  for each; no new sampling was needed since Step 5b already supplied it.
- **Minor — ambiguous "Next free IDs" sentence.** `FINDINGS.md:150` and the
  notebook's matching "Registry after this gate" bullet read "...IMP-001..024.
  Next free IDs are BUG-053, SEC-008, REL-016, IMP-025," which scans as a
  continuation of the findings list. Fixed in both places with a "Not yet
  allocated — do not count these as findings" label immediately before the
  four IDs.

### Task 12 Fix Round 1 Ledger

| Timestamp | Command | Exit | Result | Evidence |
|---|---|---|---|---|
| 2026-07-30T00:20:05Z | <code>awk -F'&#124;' 'NF&gt;4 &amp;&amp; \$2 ~ /`/ {print \$4}' docs/audit/FILE_COVERAGE.md &#124; sed 's/^ *//; s/ *\$//' &#124; sort &#124; uniq -c &#124; wc -l</code> | 0 | **42** — corrects the "41" stated in `FILE_COVERAGE.md`, this notebook (twice) and this file (twice) | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-30T00:20:22Z | <code>awk -F'&#124;' 'NF&gt;4 &amp;&amp; \$2 ~ /`/ {print \$4}' docs/audit/FILE_COVERAGE.md &#124; sed 's/^ *//; s/ *\$//' &#124; sort &#124; uniq -c &#124; awk '{s+=\$1} END{print s}'</code> | 0 | **237** — unchanged; the sum half of the original claim was never wrong | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-30T00:20:40Z | <code>git log --oneline --all -S 'Task 12 length-boundary re-review' -- docs/audit/FILE_COVERAGE.md</code> | 0 | `0c2aa97a` only — the singleton disposition class causing the 41→42 discrepancy was introduced by this gate's own commit, after its internal census was already written | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-30T00:20:55Z | <code>git show 09d6c7bfcd864a0ad3951b87d16a88dc770392a3:src/networkclient.cpp \| sed -n '843p;894p;914p;916p'</code> | 0 | `if (recvBufferLen + received < BUFFER_SIZE) {` / `recvBufferLen = remaining;` (×2) / `recvBufferLen = 0;` — samples the previously-uncounted class and confirms BUG-052's citations against the pinned source | [FINDINGS.md](FINDINGS.md) |
| 2026-07-30T00:21:15Z | <code>awk '/^### Task 12 independent final challenge/{f=1;next} /^## Limitations/{f=0} f' docs/audit/SDL3_REVIEW_STATUS.md &#124; grep -c '^&#124; 202'</code> | 0 | **36** timestamped ledger rows in this file's `### Task 12 independent final challenge` section — not the 20 `task-12-report.md` §7 stated | This file, `### Task 12 independent final challenge` |
| 2026-07-30T00:21:30Z | <code>awk '/^## 2\. Commands/{f=1;next} /^## 3\./{f=0} f' task-12-report.md &#124; grep -oE '^&#124; [0-9]+[a-z]?' &#124; wc -l</code> | 0 | **38** rows total: numbers 1-37 plus one unnumbered `8b` sub-row — a different count than the 36 status-file ledger rows because it additionally includes the final `git commit` (item 36) and the post-commit drift check (item 37), neither loggable inside the file being committed | `task-12-report.md` §2 (gitignored) |
| 2026-07-30T00:21:45Z | <code>python3 -c "import re,pathlib; t=pathlib.Path('docs/audit/subsystems/09-final-challenge.md').read_text(); print('confirmed-findings' in [re.sub(r'[^a-z0-9- ]','',h.lower()).strip().replace(' ','-') for h in re.findall(r'^##+ (.+)$', t, re.M)])"</code> | 0 | `True` — `subsystems/09-final-challenge.md#confirmed-findings` resolves to exactly one heading (`## Confirmed findings`, appears once), so both new BUG-041 cross-references are unambiguous | [subsystems/09-final-challenge.md](subsystems/09-final-challenge.md#confirmed-findings) |

## Commands and evidence

Each row records exactly one top-level shell command. A shell loop remains one
syntactic command, but unnamed multi-command “gate” rows are not used. Exit
values and material output are from captured Task 1, fix-round, and Task 2
through Task 10 evidence. Tasks 6, 7 and 8 read their scoped files with the
agent's file reader rather than `nl`/`sed`, so only their shell commands appear
below; the files they read are listed in the
[Task 6 notebook scope](subsystems/04-lobby-settings-input.md#scope) and the
[Task 8 notebook scope](subsystems/06-platform-ports.md#scope).

**Canonical log cutoff:** completed Task 9's build-definition parity
measurements, Step 4 validators and their two recorded substitutions, the
REL-010 predicate reproduction, the `exec_program` disproof, the Gradle drift
manifests, and the 24-quantity count sweep — plus everything through Task 8's
Fix Round 2, and Task 10's complete integration matrix, harness build, client
smoke, reproductions, re-derivations and cleanup proof (its own
[ledger section](#task-10-integration-ledger)). Task 9's and Task 10's final
validation, staging, commit, and post-commit checks belong in their ignored
controller reports, preventing a false claim that a commit records itself. The same exclusion already covers Task 2, the Task 5 fix round,
Task 6, Task 7, Task 8 including both of its fix rounds, and Task 10.

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

### Task 10 integration ledger

Each row is exactly one top-level shell command with its own real integer exit.
`lsof` and `pgrep` exit **1** when nothing matches; those rows record 1 rather
than normalizing it to 0. `./run_case.sh` is a single audit script that runs one
matrix row end to end (start a dedicated sanitized server on a reserved port,
run one scenario, stop the server, verify the port released); it is one command
with one exit, and the row's substantive result is the `SCENARIO_EXIT` /
`SERVER_ALIVE_AFTER` / `SANITIZER_DIAGNOSTICS` triple it prints, which the
Result cell quotes. Commands run from `/tmp/fb-sdl3-audit/task10` unless an
absolute path is shown. No hostile, malformed, fragmented, duplicated,
reordered or flooding traffic appears anywhere below: that class of runtime
testing was excluded by user direction.

| Timestamp (UTC) | Command | Exit | Concise result | Evidence |
|---|---|---:|---|---|
| 2026-07-29T15:33:21Z | <code>lsof -nP -iTCP:25610 -sTCP:LISTEN</code> | 0 | Confirmed the first reserved port free before binding, then that the bring-up server was listening on it. Ports 1511 and the user's 15xxx ports were never touched | [subsystems/08-dynamic-integration.md#environment-and-isolation](subsystems/08-dynamic-integration.md#environment-and-isolation) |
| 2026-07-29T15:37:40Z | <code>pgrep -fl fb-server &gt; logs/preexisting-fbserver.txt</code> | 0 | Baseline captured **before** the first Task 10 launch: **4** unrelated `fb-server` processes on ports 15511, 15512, 15113, 15998, three of them from a different repository | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T15:37:59Z | <code>./run_case.sh c01-room2 25610 room2 25610 2 5 1 - normal</code> | 0 | 2-seat room, normal mode: `create_reply=FB/1.3 CREATE: OK`, 0 join failures, 2 distinct seats (`A`,`B`), started=true, `SERVER_ALIVE_AFTER=yes`, `SANITIZER_DIAGNOSTICS=0`, `PORT_RELEASED 25610`. `run_case.sh` is one audit script per matrix row (start a dedicated server, run one scenario, stop it, check the port); its own exit is 0 when the row completed and cleaned up, and the row's findings are the three values it prints | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:38:15Z | <code>./run_case.sh c02-room5 25611 room5 25611 5 5 1 - normal</code> | 0 | 5-seat room: 5 distinct seats `A`..`E`, 4 `S` and 4 `n` frames per peer, started=true — but `SERVER_ALIVE_AFTER=no`, `SANITIZER_DIAGNOSTICS=1`. **First BUG-049 reproduction** | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:38:27Z | <code>./run_case.sh c03-room6 25612 room6 25612 6 6 1 - normal</code> | 0 | 6-seat room (first battle-royale count): 6 distinct seats, 41 frames per joiner, started=true; `SERVER_ALIVE_AFTER=no`, `SANITIZER_DIAGNOSTICS=1` (BUG-049) | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:38:47Z | <code>./run_case.sh c04-room20 25613 room20 25613 20 20 1 - normal</code> | 0 | **20-seat room at `MAX_NET_PLAYERS`**: 20 distinct seats, the contiguous range 65-84 (`A`..`T`), every peer's seat map identical, 153 frames per joiner, started=true; `SERVER_ALIVE_AFTER=no`, `SANITIZER_DIAGNOSTICS=1` (BUG-049) | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:39:17Z | <code>./run_case.sh c05-rounds3 25614 room2r3 25614 2 5 3 - normal</code> | 0 | **Rounds 1-3**: 3 `S` stats and 3 `n` ready frames per peer, both peers alive after round 3; `SERVER_ALIVE_AFTER=yes`, `SANITIZER_DIAGNOSTICS=0` | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:39:30Z | <code>./run_case.sh c06-team5 25615 room5t 25615 5 5 1 - team</code> | 0 | **Team mode**: `SETOPTIONS TEAMPLAY:1,PLAYERTEAM_P1:1,PLAYERTEAM_P2:2,MODE:team` echoed to all 5 seats, round completed; `SANITIZER_DIAGNOSTICS=1` at teardown (BUG-049). The on-screen team banner was not observed | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:39:44Z | <code>./run_case.sh c07-clear5 25616 room5c 25616 5 5 1 - clear</code> | 0 | **Clear mode**: `SETOPTIONS TEAMPLAY:0,CLEARMODE:1,MODE:clear` echoed to all 5 seats, round completed; `SANITIZER_DIAGNOSTICS=1` at teardown (BUG-049). The clear-win banner and its sound were not observed | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:39:52Z | <code>./run_case.sh c08-departmember 25617 room6d 25617 6 6 1 member normal</code> | 0 | **Member departure mid-round** from a 6-seat room: `after_member_depart_alive=5` and `round1_leave_msgs=5` — exactly one `l` frame per survivor, carrying the departed seat's id; `SANITIZER_DIAGNOSTICS=1` at teardown (BUG-049) | [subsystems/08-dynamic-integration.md#cross-gate-corroboration](subsystems/08-dynamic-integration.md#cross-gate-corroboration) |
| 2026-07-29T15:40:08Z | <code>./run_case.sh c09-departcreator 25618 room5d 25618 5 5 1 creator normal</code> | 0 | **Creator departure mid-round** from a 5-seat room: `after_creator_depart_alive=4`, `round1_leave_msgs=4`; the room is **not** closed while PLAYING, so continuation is entirely the client's decision (BUG-021 corroboration); `SANITIZER_DIAGNOSTICS=1` at teardown (BUG-049) | [subsystems/08-dynamic-integration.md#cross-gate-corroboration](subsystems/08-dynamic-integration.md#cross-gate-corroboration) |
| 2026-07-29T15:40:27Z | <code>./run_case.sh c10-cap20 25619 cap20 25619 20 21</code> | 0 | **Admission boundary**: `CREATE cap20 20` then 21 join attempts → **19** admitted (20 seats with the creator), **2** rejected `GAME_FULL`. `SANITIZER_DIAGNOSTICS=0` | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:40:46Z | <code>./run_case.sh c11-cap21 25620 cap21 25620 21 6</code> | 0 | **Boundary+1**: `CREATE cap21 21` answered `OK` but produced a **5**-seat room — `FB/1.3 LIST: … [cap21,cap21j00,cap21j01,cap21j02,cap21j03]:5 …`, 4 joiners admitted, 2 rejected. Silent fallback to the legacy default → **IMP-024** | [subsystems/08-dynamic-integration.md#new-improvement](subsystems/08-dynamic-integration.md#new-improvement) |
| 2026-07-29T15:41:17Z | <code>./run_case.sh c12-lonely 25621 lonely 25621</code> | 0 | `START` with a single seat is refused: `FB/1.3 START: ALONE_IN_THE_DARK`. `SANITIZER_DIAGNOSTICS=0` | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:41:19Z | <code>./run_case.sh c13-relobby 25622 relobby 25622</code> | 0 | `PART` then rejoin an OPEN room: creator saw `PARTED`, the rejoin answered `JOIN: OK`. `SANITIZER_DIAGNOSTICS=0` | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:41:22Z | <code>./run_case.sh c14-creatorlobby 25623 creatorlobby 25623</code> | 0 | **Creator departure before start**: both joiners notified and both re-listed as open players (`FB/1.3 LIST: clJ0,clJ1,  free:1 …`); neither connection was dropped. `SANITIZER_DIAGNOSTICS=0` | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:42:34Z | <code>./run_case.sh c16-massleave2 25626 massleave2 25626 2</code> | 0 | **Minimum-size probe, 2 seats**: all sockets closed at once from a playing room; `SERVER_ALIVE_AFTER=yes`, `SANITIZER_DIAGNOSTICS=0` — 2 seats cannot recurse deeply enough to reach BUG-049 | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:42:38Z | <code>./run_case.sh c16-massleave3 25627 massleave3 25627 3</code> | 0 | **3 seats — the minimum reproducing size**: `SERVER_ALIVE_AFTER=no`, `SANITIZER_DIAGNOSTICS=1`, `heap-use-after-free game.c:1051 in player_part_game_` (BUG-049). Below the smallest room the shipped client can create | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:42:43Z | <code>./run_case.sh c16-massleave4 25628 massleave4 25628 4</code> | 0 | 4 seats: `SERVER_ALIVE_AFTER=no`, `SANITIZER_DIAGNOSTICS=1` (BUG-049) | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:42:48Z | <code>./run_case.sh c16-massleave5 25629 massleave5 25629 5</code> | 0 | 5 seats: `SERVER_ALIVE_AFTER=no`, `SANITIZER_DIAGNOSTICS=1` (BUG-049) | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:43:21Z | <code>python3 /tmp/fb-sdl3-audit/task10/scenario.py massleave3r 25630 3</code> | 0 | The identical 3-seat input against the **uninstrumented Release** `fb-server`: the server **did not abort**. Its log shows the outer frame continuing past the free (`stats_record_loss`, `game.c:1048`) with no second win recorded — the freed 4 bytes happened not to read as `1`. Proves BUG-049 is not a sanitizer artefact and is *silent* in the shipped configuration | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:44:01Z | <code>./run_case.sh c15-room10 25631 room10 25631 10 10 1 - normal</code> | 0 | 10-seat room (the middle `kRoomSizes` value): 10 distinct seats, 73 frames per joiner, started=true; `SANITIZER_DIAGNOSTICS=1` at teardown (BUG-049) | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T15:44:20Z | <code>CFFIXED_USER_HOME=/tmp/fb-sdl3-audit/task10/prefs-isolated HOME=/tmp/fb-sdl3-audit/task10/prefs-isolated ./prefprobe</code> | 0 | **Isolation proven before any client ran**: `PREFPATH=/tmp/fb-sdl3-audit/task10/prefs-isolated/Library/Application Support/frozen-bubble/` — inside the temp root | [subsystems/08-dynamic-integration.md#environment-and-isolation](subsystems/08-dynamic-integration.md#environment-and-isolation) |
| 2026-07-29T15:44:59Z | <code>/Users/dchau/gr/frozen-bubble-sdl3/build-audit-sanitize/frozen-bubble-sdl3</code> | 0 | **Native single-player smoke.** The shipped client under ASan+UBSan, dummy video/audio and the software renderer, with an isolated `CFFIXED_USER_HOME`: reached `RunForEver: starting loop`, ran 12 s, handled `SIGTERM` as `SDL_EVENT_QUIT`, wrote `settings.ini` inside the temp root and logged `Logger shutting down`. **0** sanitizer diagnostics. It was not navigated through its menus — no input-injection path exists | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:45:20Z | <code>stat -f "%Sm %N" "$HOME/Library/Application Support/frozen-bubble"/*</code> | 0 | All three of the user's **real** preference files still carry their `Jul 28 08:59:49 2026` mtimes — untouched by this gate's client runs | [subsystems/08-dynamic-integration.md#environment-and-isolation](subsystems/08-dynamic-integration.md#environment-and-isolation) |
| 2026-07-29T15:45:40Z | <code>ctest -R 'netview&#124;netteams&#124;roundstats' --output-on-failure</code> | 0 | The three registered C++ tests re-run in the ASan+UBSan tree with `detect_leaks=0`: `netview-test`, `netteams-test`, `roundstats-color-test` — **3/3 passed**, no diagnostic. Covers spectator ranking, >5-player paging, team assignment and round-stats colours at the logic level; their on-screen presentation was not observed | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:45:55Z | <code>./task5_actual_gameplay_harness_fix1_sanitize</code> | 0 | Task 5's production-object gameplay harness re-run unchanged under ASan+UBSan: `task5-actual-gameplay=PASS seed=0x5d13 players=1,2,5,6,20 teams=1..5 colors=5,8 grid=standard,flipped rounds=3 classicClear=BUG-018/clear simultaneous=BUG-019 maxDeltaTunnel=BUG-025 …` — BUG-018, BUG-019 and BUG-025 reproduce exactly as recorded, corroborating IMP-018 | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T15:46:10Z | <code>clang++ -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -I…/src task10_netclient_harness.cpp build-audit-sanitize/CMakeFiles/frozen-bubble-sdl3.dir/src/networkclient.cpp.o -lSDL3 -o task10_netclient_harness</code> | 0 | Linked the **unchanged production `networkclient.cpp` sanitizer object** into an audit harness. No production source was created, modified or recompiled from edited sources | [subsystems/08-dynamic-integration.md#client-layer-runtime-record](subsystems/08-dynamic-integration.md#client-layer-runtime-record) |
| 2026-07-29T15:46:24Z | <code>./task10_netclient_harness 25640</code> | 0 | **Production `NetworkClient` against a live sanitized server.** `connect=1`, `nick=1 create=1 leader=1`, `listed_games=1` with `maxPlayers=20` recovered from the `[hL]:20` cap suffix, `part=1 games_after_part=0 open_after_part=1`, `connect_to_closed_port=0`; **0** sanitizer diagnostics on both processes. One failure: `start_returned=1` while the wire carried `FB/1.3 START: ALONE_IN_THE_DARK` — **BUG-015's first runtime reproduction** | [subsystems/08-dynamic-integration.md#client-layer-runtime-record](subsystems/08-dynamic-integration.md#client-layer-runtime-record) |
| 2026-07-29T15:47:10Z | <code>./run_case.sh c20-reconnect 25641 reconnect 25641</code> | 0 | **Mid-game reconnect**: a seat drops during a round, reconnects with the same nick (`FB/1.3 NICK: OK`) and is refused rejoin with `FB/1.3 JOIN: NO_SUCH_GAME`, because `find_game_by_nick_aux` only matches `GAME_STATUS_OPEN`. Deliberate design — dismissed, not a defect. `SANITIZER_DIAGNOSTICS=0` | [subsystems/08-dynamic-integration.md#dismissed-candidates](subsystems/08-dynamic-integration.md#dismissed-candidates) |
| 2026-07-29T15:47:54Z | <code>./run_case.sh c21-freecount 25642 freecount 25642</code> | 0 | **BUG-050 reproduction**: `FB/1.3 LIST: fcIdle, [fcA,fcJ0,fcJ1]:5 free:3 games:0 playing:0 at:` — the enumerated open-player list has **1** entry, `free:` says **3**, the correct value excluding the requester is **0**. `SANITIZER_DIAGNOSTICS=0` | [subsystems/08-dynamic-integration.md#confirmed-findings](subsystems/08-dynamic-integration.md#confirmed-findings) |
| 2026-07-29T15:48:29Z | <code>kill -TERM 94942</code> | 0 | **Clean server shutdown** after a completed 2-seat round: the sanitized server exited on `SIGTERM` with exit status **0** and **0** sanitizer diagnostics, and the port was released | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T18:00:43Z | <code>ls /tmp/fb-sdl3-audit/task10/logs/c*.server.log &#124; wc -l</code> | 0 | **24** per-row server logs — 23 from the ASan+UBSan build, 1 from the Release build. Re-derived, not carried over | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T18:00:52Z | <code>grep -l 'ERROR: AddressSanitizer' /tmp/fb-sdl3-audit/task10/logs/c*.server.log &#124; wc -l</code> | 0 | **11** of the 24 server logs contain an AddressSanitizer report | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T18:01:04Z | <code>grep -h 'SUMMARY: AddressSanitizer' /tmp/fb-sdl3-audit/task10/logs/c*.server.log &#124; sort -u</code> | 0 | Exactly **one** distinct summary line across all 11 reports: `SUMMARY: AddressSanitizer: heap-use-after-free game.c:1051 in player_part_game_` — every reproduction is the same defect | [subsystems/08-dynamic-integration.md#bug-049-reproduction-record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record) |
| 2026-07-29T18:01:15Z | <code>python3 /tmp/fb-sdl3-audit/task10/verify_relay.py</code> | 0 | Relay invariants re-derived from the saved journals: `journals=11 maps_agree=11/11 self_echo_total=0 unknown_sender_total=0 frames_total=4990` | [subsystems/08-dynamic-integration.md#aggregate-relay-measurements](subsystems/08-dynamic-integration.md#aggregate-relay-measurements) |
| 2026-07-29T17:56:56Z | <code>lsof -nP -iTCP:25610-25650</code> | 1 | **Cleanup.** No output; `lsof` exits **1** when nothing matches, so no socket of any state exists anywhere in the reserved range. The exit is recorded as 1, not normalized to 0 | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T17:57:20Z | <code>for p in 25610 … 25643; do lsof -nP -iTCP:$p &gt;/dev/null 2&gt;&amp;1 &amp;&amp; echo BOUND &#124;&#124; echo FREE; done &#124; grep -c '^FREE$'</code> | 0 | **24** — every one of the 24 ports this gate used reads FREE. One syntactic loop, per this file's convention | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T17:57:40Z | <code>pgrep -fl fb-server &gt; logs/final2-fbserver.txt</code> | 0 | Final snapshot: the same **4** processes on ports 15511, 15512, 15113, 15998 | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T17:57:52Z | <code>diff logs/preexisting-fbserver.txt logs/final2-fbserver.txt</code> | 0 | No output — the `fb-server` process list is **byte-identical** to the baseline captured before the first launch. Task 10 neither started nor stopped any of the four | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T17:58:05Z | <code>ps -o pid=,etime=,command= -p 22293 -p 22300 -p 74458 -p 76361</code> | 0 | Elapsed times `04-00:49:44`, `04-00:49:44`, `01-15:37:04`, `03-14:51:07` — all four survivors started 1-4 **days** before this gate's first launch, so none is Task 10's | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T17:58:20Z | <code>pgrep -fl 'task10_netclient_harness&#124;task10/scenario.py&#124;build-audit-sanitize/frozen-bubble-sdl3&#124;prefprobe'</code> | 1 | No output, exit **1** — no harness, scenario driver, client or probe process from this gate survives | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T17:58:32Z | <code>kill -0 94457</code> | 1 | Exit **1** — the one long-lived client process this gate started is gone | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T18:02:10Z | <code>find /tmp/fb-sdl3-audit/task10 -name joiners.log &#124; wc -l</code> | 0 | **25** — one per server instance (24 matrix rows plus the bring-up instance); every server ran from a working directory under `/tmp/fb-sdl3-audit/task10/`, so the side-effect file never landed elsewhere | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T18:02:22Z | <code>find /Users/dchau/gr/frozen-bubble-sdl3 -name joiners.log -exec ls -l {} \;</code> | 0 | **1** in the repository root, mtime `Jul 28 11:01` — a day before this gate's first launch, so it belongs to an earlier task | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T18:03:05Z | <code>python3 -c "…extract BUG/SEC/REL/IMP IDs from FINDINGS.md and check uniqueness and per-class contiguity from 1…"</code> | 0 | `total 95 unique 95 contiguous True counts {'BUG': 50, 'IMP': 24, 'REL': 14, 'SEC': 7}` — 92 before Task 10, **95** after; BUG-049, BUG-050 and IMP-024 are the next free IDs in their classes and no existing ID was renumbered or recycled | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T18:03:18Z | <code>python3 -c "…compare each subsystem notebook's '## ' headings to the ten required, in order…"</code> | 0 | `notebooks 9 mismatches 0` — the ten-heading invariant holds for all nine notebooks including the newly written `08-dynamic-integration.md` | [subsystems/08-dynamic-integration.md](subsystems/08-dynamic-integration.md) |
| 2026-07-29T18:03:30Z | <code>python3 -c "…count &#96;&#124; &#96;&#96;&#96;&#96; rows and case-insensitive 'pending' occurrences in docs/audit/FILE_COVERAGE.md…"</code> | 0 | `rows 237` and **10** `pending` occurrences, all on line 5 — the inventory rule paragraph. Task 10 extended four rows' Notes and introduced no new row and no new pending state | [FILE_COVERAGE.md](FILE_COVERAGE.md) |

### Task 10 Fix Round 1

An independent review of commit `f297dfc1` raised two Critical, one Important
and one Minor finding; all four accepted. Each row below is its own top-level
command, independently re-run against the pinned baseline and the preserved
`/tmp/fb-sdl3-audit/task10/` evidence, with its own real exit code.

| Time (UTC) | Command | Exit | Result | Evidence |
|---|---|---:|---|---|
| 2026-07-29T18:25:03Z | <code>git show 09d6c7bfcd864a0ad3951b87d16a88dc770392a3:server/stats.c &#124; sed -n '82,91p'</code> | 0 | Confirms `stats_init()`'s unconditional `getenv("HOME")` derivation of `stats_file_path` at the pinned baseline, unchanged from what shipped | [subsystems/08-dynamic-integration.md#trust-boundaries-and-invariants](subsystems/08-dynamic-integration.md#trust-boundaries-and-invariants) |
| 2026-07-29T18:25:19Z | <code>grep -c 'HOME=' /tmp/fb-sdl3-audit/task10/run_case.sh</code> | 1 | `0` matches — `run_case.sh` never sets `HOME` for any of its 24 launches (`grep -c` exits 1 on zero matches, recorded as-is, not normalized) | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T18:25:34Z | <code>stat -f "%Sm %z %N" /Users/dchau/.fb-server/stats.dat</code> | 0 | `Jul 29 22:48:29 2026 1677 /Users/dchau/.fb-server/stats.dat` — the real, pre-existing file, confirmed untouched by this correction (read-only verification) | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T18:25:50Z | <code>python3 -c "…compare stats.dat's mtime (UTC) against c22-shutdown.server.log's mtime (UTC)…"</code> | 0 | Both `2026-07-29T15:48:29Z` — the real stats file was modified the same second as the last matrix row's server log, squarely inside the gate's recorded execution window | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T18:26:07Z | <code>grep -oE '(ml3_00&#124;ml4_02&#124;5d_03&#124;5c_00&#124;5t_00&#124;10_08&#124;20_00&#124;6_04&#124;2_00&#124;rc_01&#124;2sd_00&#124;2r3_00&#124;ml2_00&#124;ml5_00&#124;6d_00)' /Users/dchau/.fb-server/stats.dat &#124; sort -u &#124; wc -l</code> | 0 | **15** distinct Task-10-only nick prefixes found in the real file, confirming it was populated by this gate's own scenario runs | [subsystems/08-dynamic-integration.md#cleanup-proof-step-6](subsystems/08-dynamic-integration.md#cleanup-proof-step-6) |
| 2026-07-29T18:26:25Z | <code>python3 -c "…extract BUG/SEC/REL/IMP IDs from FINDINGS.md and check uniqueness and per-class contiguity…"</code> | 0 | `total 96 unique 96 contiguous True counts {'BUG': 50, 'IMP': 24, 'REL': 15, 'SEC': 7}` — 95 before this round, **96** after; REL-015 is the next free ID in its class and no existing ID was renumbered or recycled | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T18:26:40Z | <code>python3 -c "…count rows 1-41 in the notebook's matrix table by leading '&#124; N &#124;' and count how many have Result containing 'Not performed'…"</code> | 0 | `total=41 not_performed=10 executed=31` — measured directly from the corrected table, not hand-counted; matches the count stated in this file's gate row, `FINDINGS.md`, and the notebook's own matrix-summary line and Gate conclusion | [subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched](subsystems/08-dynamic-integration.md#the-recorded-matrix-step-1-executed-before-any-process-was-launched) |
| 2026-07-29T18:26:55Z | <code>git show 09d6c7bfcd864a0ad3951b87d16a88dc770392a3:src/networkclient.cpp &#124; sed -n '1306p;1307p'</code> | 0 | Line 1306 is blank (end of the previous function); line 1307 is `void NetworkClient::ParseListResponse(const char* listData) {` — confirms the corrected citation `:1307-1360` | [subsystems/08-dynamic-integration.md#static-review](subsystems/08-dynamic-integration.md#static-review) |
| 2026-07-29T18:27:10Z | <code>grep -rln "1306-1360" docs/audit .superpowers/sdd/2026-07-28-complete-repository-audit/task-10-report.md</code> | 0 | Matches only inside this ledger's own and the report's own narrative describing the old (wrong) citation string — not a live citation. The notebook and `FINDINGS.md` citations are confirmed all `:1307-1360` by separate inspection | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T18:27:24Z | <code>for h in 'Scope' 'Trust boundaries and invariants' 'Static review' 'Dynamic evidence' 'Candidates' 'Confirmed findings' 'Dismissed candidates' 'Coverage' 'Limitations' 'Gate conclusion'; do grep -c "^## ${h}\$" docs/audit/subsystems/08-dynamic-integration.md; done &#124; sort -u</code> | 0 | Prints `1` once — every one of the ten headings still appears exactly once after all Fix Round 1 edits | [subsystems/08-dynamic-integration.md](subsystems/08-dynamic-integration.md) |

### Task 11

Reconciliation and prioritization gate. Each row is its own top-level command
with its own real exit code, re-run against the pinned baseline and the live
`docs/audit/` tree.

| Time (UTC) | Command | Exit | Result | Evidence |
|---|---|---:|---|---|
| 2026-07-30T00:05:00Z | <code>git ls-tree -r --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 &gt; /tmp/fb-sdl3-audit/task11-pinned-tree.txt</code> | 0 | 3,623 tracked paths at the pinned commit | `/tmp/fb-sdl3-audit/task11-pinned-tree.txt` |
| 2026-07-30T00:05:10Z | <code>rg '^(src&#124;server&#124;tests&#124;tools&#124;android&#124;web&#124;cmake&#124;docker&#124;\.github)/&#124;^(CMakeLists\.txt&#124;CMakeListsEmscripten\.txt&#124;README\.md&#124;SetupServer\.md&#124;WASM_PORT\.md&#124;start-server\.sh&#124;netlify\.toml&#124;shell\.nix&#124;default\.nix&#124;flake\.nix&#124;flake\.lock)$' /tmp/fb-sdl3-audit/task11-pinned-tree.txt &#124; sort &gt; /tmp/fb-sdl3-audit/task11-regenerated-selection.txt</code> | 0 | Task 1 Step 4's exact selection pattern, re-run against the same pinned commit, reproduces **237** paths | `/tmp/fb-sdl3-audit/task11-regenerated-selection.txt` |
| 2026-07-30T00:05:20Z | <code>grep -oE '^\| `[^`]+`' docs/audit/FILE_COVERAGE.md &#124; sed -E 's/^\| `//; s/`$//' &#124; sort &gt; /tmp/fb-sdl3-audit/task11-coverage-paths.txt</code> | 0 | 237 paths extracted from `FILE_COVERAGE.md`'s own rows | `/tmp/fb-sdl3-audit/task11-coverage-paths.txt` |
| 2026-07-30T00:05:30Z | <code>diff /tmp/fb-sdl3-audit/task11-regenerated-selection.txt /tmp/fb-sdl3-audit/task11-coverage-paths.txt</code> | 0 | No output — the regenerated 237-path selection and `FILE_COVERAGE.md`'s 237 rows are exactly the same path set; no path is missing, no path is stale, no exclusion reason is needed | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-30T00:06:00Z | <code>python3 -c "…extract BUG/SEC/REL/IMP IDs from FINDINGS.md and check uniqueness and per-class contiguity…"</code> | 0 | `total 96 unique 96 contiguous True counts {'IMP': 24, 'BUG': 50, 'SEC': 7, 'REL': 15}` — unchanged since Task 10 Fix Round 1; no ID added, renumbered, or recycled by Task 11 | [FINDINGS.md](FINDINGS.md) |
| 2026-07-30T00:06:15Z | <code>grep -rn "suspected&#124;investigating&#124;Investigating" docs/audit/</code> | 0 | Before this gate: one live hit, `subsystems/01-server-protocol.md`'s IMP-010 candidate row, still reading "Investigating across Tasks 3 and 7" though `FINDINGS.md` already shows IMP-010 confirmed with its Task 7 cross-owner disposition complete. Corrected to "Confirmed improvement (cross-owner disposition completed in Task 7)"; re-run after the edit finds only this rule's own self-referential definition in `FINDINGS.md` line 3, matching `FILE_COVERAGE.md`'s established "pending" self-reference convention | [subsystems/01-server-protocol.md#candidates](subsystems/01-server-protocol.md#candidates) |
| 2026-07-30T00:06:30Z | <code>for f in docs/audit/subsystems/0{1,2,3,4,5,6,7,8}-*.md; do for h in 'Scope' 'Trust boundaries and invariants' 'Static review' 'Dynamic evidence' 'Candidates' 'Confirmed findings' 'Dismissed candidates' 'Coverage' 'Limitations' 'Gate conclusion'; do c=$(grep -c "^## ${h}\$" "$f"); if [ "$c" != "1" ]; then echo "MISMATCH $f $h count=$c"; fi; done; done; echo SWEEP_DONE</code> | 0 | `SWEEP_DONE` with no `MISMATCH` line — all eight subsystem notebooks still carry exactly the ten required headings, each exactly once | [subsystems/](subsystems/) |
| 2026-07-30T00:06:45Z | <code>python3 -c "…extract BUG IDs from notebook 04's persistence-matrix table and full-client-run section and union them…"</code> | 0 | `['BUG-026', 'BUG-027', 'BUG-028', 'BUG-029', 'BUG-030', 'BUG-031', 'BUG-032', 'BUG-034'] 8` — the notebook's own evidence tables name exactly eight runtime-reproduced IDs, resolving the "six" vs "eight" inconsistency (deferred item 1) in favor of **eight** | [subsystems/04-lobby-settings-input.md#dynamic-evidence](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| 2026-07-30T00:07:00Z | <code>sed -n '384,388p;334,347p' src/frozenbubble.cpp</code> | 0 | Confirms hops 1-2 of BUG-035's completed trace (deferred item 2): lines 334-347 print `PushScancode`'s raw-event fallback (`if (IsVirtualScancode(sc)) { ... if (skipEvent) return; }` then an unconditional `SDL_PushEvent`), guarded only by `IsVirtualScancode`, not by `skipEvent`; lines 384-388 print the bind-capture site (`if (down && mainMenu->IsAwaitingKeyBind()) { ... PushScancode(vsc, true); return; }`) that emits the derived scancode the fallback consumes | [subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| 2026-07-30T00:07:03Z | <code>sed -n '498,513p' src/mainmenu_input.cpp</code> | 0 | Confirms hop 3: `KeysPanelKey` stores `e->key.scancode` straight into the selected `PlayerKeys` field (`case 0: keys.left = e->key.scancode; break;` and siblings for `right`/`fire`/`center`) with no bounds or scancode-range check | [subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| 2026-07-30T00:07:06Z | <code>sed -n '50,54p' src/gamesettings.h</code> | 0 | Confirms hop 4: `IsKeyPressed` falls to the unguarded `SDL_GetKeyboardState(NULL)[sc]` index whenever `sc` is not a virtual scancode, completing BUG-035's four-hop trace | [subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| 2026-07-30T00:07:15Z | <code>grep -nE '2026-07-28T13:3[89]Z' docs/audit/SDL3_REVIEW_STATUS.md</code> | 0 | Matches exactly the two Task 6 probe rows, identified here by their stable `2026-07-28T13:38Z`/`13:39Z` timestamp content rather than a bare line number since this file's line numbers shift as sections are inserted — currently at `:1528` (13:38Z, exit 4, `HOME` alone still resolved to the user's real directory, no file opened; formerly `:1404` before Fix Round 1's ~200 inserted lines shifted this file) and `:1529` (13:39Z, exit 0, `ISOLATION=OK` with `CFFIXED_USER_HOME` added; formerly `:1405`) — and nothing else. (Fix Round 1: the original command here, `grep -n "task6/real-prefs-baseline\|task6_settings_harness probe"`, did not match either probe row's actual command text — both contain `task6_settings_harness` followed by a path argument, then a separate trailing ` probe` argument, so the substring `task6_settings_harness probe` never occurs; it matched only later hash-related lines and this ledger row's own prose, not the two probe rows it was cited to confirm. This replacement command is the one that actually demonstrates the 13:38Z/13:39Z ordering deferred item 3 relies on.) The hash baseline, currently at `:1530` (13:40Z; formerly `:1406`), postdates both probes, so "hashed beforehand" overstated the probe-window protection; corrected to name the listing/timestamp protection the probe window actually had | [SDL3_REVIEW_STATUS.md#task-6-closure-provenance](SDL3_REVIEW_STATUS.md#task-6-closure-provenance) |
| 2026-07-30T00:07:30Z | <code>python3 - &lt;&lt;'PY'\nfrom pathlib import Path\nimport re\nfiles=[Path('docs/audit/FINDINGS.md'),Path('docs/audit/FILE_COVERAGE.md'),Path('docs/audit/SDL3_REVIEW_STATUS.md'),Path('docs/audit/subsystems/02-network-client-sync.md')]\nfor f in files:\n for target in re.findall(r'\[[^]]+\]\(([^)]+)\)',f.read_text()):\n  if target.startswith(('http:','https:','#')): continue\n  base,_,anchor=target.partition('#'); p=(f.parent/base).resolve(); assert p.exists(),(f,target)\n  if anchor:\n   anchors=[]\n   for h in re.findall(r'^#{1,6} +(.*)$',p.read_text().lower(),re.M):\n    a=re.sub(r'[^a-z0-9 _-]','',h).strip().replace(' ','-'); anchors.append(re.sub(r'-+','-',a))\n   assert anchor.lower() in anchors,(f,target)\nprint('notebook_schema_and_links=PASS')\nPY</code> | 0 | `notebook_schema_and_links=PASS` — Task 4's fix-round link/schema check still passes today against the current anchor set (deferred item 4); the original failing command's exact text was not preserved in `task-4-report.md`, only its output, and there is no longer a stale anchor to reproduce it against, so this is documented as a closed historical gap in [subsystems/02-network-client-sync.md#limitations](subsystems/02-network-client-sync.md#limitations) rather than re-derived verbatim | [subsystems/02-network-client-sync.md#limitations](subsystems/02-network-client-sync.md#limitations) |
| 2026-07-30T00:07:45Z | <code>python3 -c "…count rows 1-41 in notebook 08's matrix table and how many have a verdict cell starting '**Not performed'…"</code> | 0 | `total=41 not_performed=10 executed=31` reconfirmed unchanged; row 11's team-banner aside (deferred item 5) sits inside one of the 31 executed rows, so adding it to Limitations item 4's canonical list does not change this count | [subsystems/08-dynamic-integration.md#limitations](subsystems/08-dynamic-integration.md#limitations) |
| 2026-07-30T00:08:00Z | <code>grep -rnE '\b(strcpy&#124;strcat&#124;sprintf&#124;gets)\s*\(' src server</code> | 0 | Three hits: a comment (`bubblegame_net.cpp:50`), a length-checked `strcat` into `chatInputBuf` (`bubblegame_input.cpp:174`, guarded by `curLen + addLen < sizeof(chatInputBuf) - 1`), and a fixed 9-byte literal `strcpy` into a 256-byte `networkHost` (`mainmenu_server.cpp:150`). Both real call sites are bounds-safe; no new candidate | [subsystems/04-lobby-settings-input.md#static-review](subsystems/04-lobby-settings-input.md#static-review) |
| 2026-07-30T00:08:15Z | <code>grep -rnE 'TODO&#124;FIXME&#124;XXX&#124;HACK' src server tools tests android/app/src/main/java</code> | 0 | Every hit lives under vendored `org/libsdl/app/` (excluded by the audit's vendored-boundary rule) or is a comment describing an AppImage mount-point pattern, not a marker of incomplete project logic; no new candidate | [SDL3_REVIEW_STATUS.md#task-11-closure-provenance](SDL3_REVIEW_STATUS.md#task-11-closure-provenance) |
| 2026-07-30T00:08:30Z | <code>python3 -c "…parse FINDINGS.md's IMP rows and check every Severity/Priority cell matches '(Low&#124;Medium&#124;High) benefit / (Low&#124;Medium&#124;High) effort / (Low&#124;Medium&#124;High) risk'…"</code> | 0 | `total_imp_rows 24 nonconforming []` — all 24 `IMP` entries already share one consistent benefit/effort/risk vocabulary; no duplicate normalization needed | [FINDINGS.md](FINDINGS.md) |
| 2026-07-30T00:08:45Z | <code>git diff --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 HEAD -- src server tests tools android web cmake docker .github CMakeLists.txt CMakeListsEmscripten.txt README.md SetupServer.md WASM_PORT.md start-server.sh netlify.toml default.nix flake.nix shell.nix CLAUDE.md</code> | 0 | No output — zero production drift going into Task 11's doc-only edits | [SDL3_REVIEW_STATUS.md#audit-baseline](SDL3_REVIEW_STATUS.md#audit-baseline) |

### Task 11 Fix Round 1 Ledger

An independent review of commit `03421bb3` raised one Critical, two Important
and two Minor finding; all five accepted (see
[Task 11 Fix Round 1 Findings](#task-11-fix-round-1-findings)). Each row below
is its own top-level command, independently re-run against the pinned
baseline and the live `docs/audit/`/`src`/`server` tree; the corrected probe
row appears in place, above, at its original `T00:07:15Z` timestamp.

| Time (UTC) | Command | Exit | Result | Evidence |
|---|---|---:|---|---|
| 2026-07-30T01:00:00Z | <code>grep -rnE '\[(team&#124;scancode&#124;senderId&#124;playerId&#124;slot&#124;idx&#124;index&#124;cellIndex&#124;button&#124;sc)\b[^]]*\]' src server</code> | 0 | **30** hits (unchecked indices, previously-missing category 1 of 5). 29 are bounds-safe (loop-`.size()`-bounded, `%4`/`%20`-wrapped, or already BUG-035/BUG-036/SEC-007); one is new: `src/bubblegame_level.cpp:67`'s `level[idx] = line` has no `idx < 10` guard against the fixed `std::array<std::vector<int>, 10>`, unlike `src/highscoremanager.cpp:132-134`'s `if (idx < 10) level[idx] = line` for the identical file format | [subsystems/03-gameplay.md#confirmed-findings](subsystems/03-gameplay.md#confirmed-findings) |
| 2026-07-30T01:00:15Z | <code>python3 -c "print(open('share/data/levels').read())" &#124; awk 'BEGIN{c=0;m=0;n=0}/^$/{if(c>0){print c; if(n==0&#124;&#124;c&lt;n)n=c; if(c&gt;m)m=c}; c=0; next}{c++}END{if(c&gt;0){if(n==0&#124;&#124;c&lt;n)n=c; if(c&gt;m)m=c}}' &#124; sort -n &#124; uniq -c</code> | 0 | Confirms BUG-051 is not reachable with the shipped asset: every one of the **100** level blocks in `share/data/levels` has exactly **10** lines (min = max = 10), so `idx` never exceeds 9 with the current file | [subsystems/03-gameplay.md#confirmed-findings](subsystems/03-gameplay.md#confirmed-findings) |
| 2026-07-30T01:00:30Z | <code>grep -rn "SaveLevelset" src</code> | 1 | No output — no Level Editor (or any other) write path exists in the pinned source that could author a level block exceeding 10 lines, confirming BUG-051 has no in-app trigger today either | [subsystems/03-gameplay.md#confirmed-findings](subsystems/03-gameplay.md#confirmed-findings) |
| 2026-07-30T01:01:00Z | <code>grep -rnE '\((unsigned( int&#124; long&#124; char&#124; short)?&#124;size_t&#124;uint8_t&#124;uint16_t&#124;uint32_t&#124;uint64_t)\)' src server</code> | 0 | **36** hits (signed/unsigned conversions, previously-missing category 2 of 5). All are bounds-safe: pointer-difference casts (`ws.c`), fixed-width protocol byte extraction already within SEC-002/SEC-006's territory (`networkclient.cpp:867,1213`, `networkclient_wasm.cpp:93` — each feeds a lookup loop, never a direct index), and `size_t`-vs-loop-index comparisons already IMP-002/REL-001's territory. No new candidate | [subsystems/01-server-protocol.md#task-2-candidate-dispositions](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| 2026-07-30T01:01:30Z | <code>grep -rnE '^\s*(system&#124;fwrite&#124;fread&#124;write&#124;read&#124;remove&#124;rename&#124;fclose&#124;unlink&#124;SDL_RenderCopy&#124;SDL_RenderPresent&#124;chdir&#124;mkdir)\(' src server</code> | 0 | **19** hits (ignored return values, previously-missing category 3 of 5). `mainmenu_server.cpp:93`'s `system(...)` is BUG-033; `server/stats.c:76`'s `mkdir(...)` failure surfaces downstream at the `fopen` REL-015 already characterizes; the remaining `fclose`/`SDL_RenderPresent` statement-level ignores are idiomatic with no established resource or security consequence. No new candidate | [FINDINGS.md](../audit/FINDINGS.md) |
| 2026-07-30T01:02:00Z | <code>grep -rn "ptrInstance\b" src/*.h src/*.cpp</code> | 0 | **33** hits (global/singleton lifetime, previously-missing category 4 of 5), across the six singletons with a `ptrInstance` field. `AudioMixer::Dispose()`, `TransitionManager::Dispose()`, `HighscoreManager::Dispose()`, and `GameSettings::Dispose()` free members but never null `ptrInstance` — exactly IMP-007's already-registered pattern. `NetworkClient::Dispose()` (`networkclient.cpp:60-64`) correctly sets `ptrInstance = nullptr` after `delete`; `FrozenBubble` (`frozenbubble.h/.cpp`) declares `ptrInstance` but has no `Dispose()` at all — a true program-lifetime singleton, not the leaking pattern. Both confirmed as the non-matching, safe outliers. No new candidate | [FINDINGS.md](../audit/FINDINGS.md) |
| 2026-07-30T01:02:30Z | <code>grep -rnE '\b(plen&#124;hdr_len&#124;Content-Length&#124;content_length&#124;opcode&#124;recvBufferLen&#124;msgLen&#124;payload_len&#124;framelen&#124;frame_len)\b' src server</code> | 0 | **46** hits (network lengths/opcodes, previously-missing category 5 of 5). `server/net.c:1133`'s `Content-Length` literal is SEC-002; the client-side `msgLen`/`recvBufferLen` framing sites are within BUG-006/SEC-006's territory; `server/ws.c`'s `ws_decode_inplace` `plen`/`hdr_len` frame-length handling was re-read line-by-line: the 64-bit length case is rejected (`plen == 127` returns `-1`, `:239`), the extended 16-bit length is read from both bytes before use (`:241-244`), and the frame-completeness gate (`total - pos < hdr_len + plen`, `:249`) runs before the `memmove` that follows it (`:259`). No hostile-traffic or runtime probing was performed, per the user's standing restriction; this disposition is static reading only. No new candidate | [subsystems/01-server-protocol.md#network-derived-length-and-index-trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| 2026-07-30T01:03:00Z | <code>grep -rhoE '#\s*(if&#124;ifdef&#124;ifndef&#124;elif)\b.*' src &#124; grep -oE '__[A-Za-z0-9_]+__&#124;_WIN32&#124;__linux__' &#124; sort -u</code> | 0 | **Seven** guard tokens, not two: `__ANDROID__`, `__ANDROID_PORT__`, `__APPLE__`, `__MINGW32__`, `__WASM_PORT__`, `__linux__`, `_WIN32`. Confirms the Important 2 finding: `gamesettings.h:86` (`#if defined(__ANDROID__) \|\| defined(__ANDROID_PORT__)`), `platform.cpp:22,98` (`_WIN32`/`__MINGW32__`), `platform.cpp:24,122` (`__linux__`), `platform.cpp:109` (`__APPLE__`). Only the first three tokens are named by CLAUDE.md's Platform abstraction section; the false "matches exactly" claim is removed | [SDL3_REVIEW_STATUS.md#task-11-fix-round-1-findings](#task-11-fix-round-1-findings) |
| 2026-07-30T01:03:15Z | <code>grep -rn "#\s*(if&#124;ifdef&#124;ifndef&#124;elif).*__ANDROID_PORT__" src</code> | 0 | One site: `src/gamesettings.h:86`, exactly the line the independent review cited | [SDL3_REVIEW_STATUS.md#task-11-fix-round-1-findings](#task-11-fix-round-1-findings) |
| 2026-07-30T01:04:00Z | <code>grep -rn 'getenv("USER")' src</code> | 0 | **6** hits: `mainmenu_netpanel.cpp:70,1010,1012,1092,1094`, `mainmenu_input.cpp:1495` — exactly the six sites the independent review found unswept. Each follows `getenv("USER") ? getenv("USER") : "<fallback>"` feeding a bounded `snprintf` as a nickname default; benign, no new candidate | [SDL3_REVIEW_STATUS.md#task-11-fix-round-1-findings](#task-11-fix-round-1-findings) |
| 2026-07-30T01:04:30Z | <code>grep -vE '^\s*#' CMakeLists.txt &#124; grep -oE '\.cpp' &#124; wc -l</code> | 0 | **36** — reproduces the "36" `CMakeLists.txt` count with its filter stated: exclude comment lines, then count literal `.cpp` occurrences (a naive count with no comment exclusion returns 37, the extra one being line 55's comment mentioning `networkclient.cpp`) | [FINDINGS.md](../audit/FINDINGS.md) |
| 2026-07-30T01:04:45Z | <code>grep -oE '\.cpp' CMakeListsEmscripten.txt &#124; wc -l</code> | 0 | **15** — reproduces the "15" count; no comment line in this file contains `.cpp`, so no exclusion is needed | [FINDINGS.md](../audit/FINDINGS.md) |
| 2026-07-30T01:05:00Z | <code>grep -oE '\.cpp' android/app/CMakeLists.txt &#124; wc -l</code> | 0 | **28** — reproduces the "28" count; same, no comment exclusion needed | [FINDINGS.md](../audit/FINDINGS.md) |
| 2026-07-30T01:05:30Z | <code>grep -rnE '\b(malloc&#124;calloc&#124;realloc)\s*\(' src server &#124; wc -l</code> | 0 | **11** — re-derives the allocation count the original gate's Step 2 narrative stated with no ledger row of its own; unchanged | [subsystems/01-server-protocol.md#task-2-candidate-dispositions](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| 2026-07-30T01:05:45Z | <code>grep -rnE '\bfree\s*\(' src server &#124; wc -l</code> | 0 | **44** — re-derives the paired `free(` count with its own command; unchanged | [subsystems/01-server-protocol.md#task-2-candidate-dispositions](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| 2026-07-30T01:06:00Z | <code>grep -rnE 'SDL_Create[A-Za-z]+\(' src server &#124; wc -l</code> | 0 | **15** — re-derives the SDL create-call-site count with its own command; matches Task 7's ownership table | [subsystems/05-render-audio.md#confirmed-findings](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-30T01:06:15Z | <code>grep -rnE 'SDL_Destroy[A-Za-z]+\(' src server &#124; wc -l</code> | 0 | **37** — re-derives the SDL destroy-call-site count with its own command; matches Task 7's ownership table | [subsystems/05-render-audio.md#confirmed-findings](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-30T01:06:30Z | <code>grep -rnoE '"[^"]*[0-9]+\.[0-9]+\.[0-9]+[^"]*"' src server *.nix CMakeLists.txt CMakeListsEmscripten.txt android/app/build.gradle .github/workflows/build.yml</code> | 0 | Reproduces exactly REL-004's catalogued five version strings (`2.2.1`, embedded `v2.4.9`, `v2.4.26`, `2.4.27`, `0.1.0`) among IP-address literals and third-party dependency version pins (SDL/SDL_image/SDL_mixer/SDL_ttf tags, NDK version, AGP/CMake versions) — no new candidate | [FINDINGS.md](../audit/FINDINGS.md) |
| 2026-07-30T01:06:45Z | <code>grep -rnE 'getenv\("HOME"\)&#124;SDL_GetPrefPath&#124;SDL_GetBasePath' src server &#124; wc -l</code> | 0 | **5** — re-derives the filesystem/`getenv(HOME)` count with its own command: `gamesettings.cpp:32`, `platform.h:29` (comment), `server/stats.c:87` (REL-015), `platform.cpp:110,113` (REL-008); unchanged | [FINDINGS.md](../audit/FINDINGS.md) |
| 2026-07-30T01:07:00Z | <code>python3 -c "…extract BUG/SEC/REL/IMP IDs from FINDINGS.md and check uniqueness and per-class contiguity…"</code> | 0 | `total 97 unique 97 contiguous True counts {'IMP': 24, 'BUG': 51, 'SEC': 7, 'REL': 15}` — **97**, up from 96, after BUG-051; BUG-051 is the next free ID in its class, no existing ID renumbered or recycled | [FINDINGS.md](../audit/FINDINGS.md) |
| 2026-07-30T01:07:15Z | <code>git diff --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 HEAD -- src server tests tools android web cmake docker .github CMakeLists.txt CMakeListsEmscripten.txt README.md SetupServer.md WASM_PORT.md start-server.sh netlify.toml default.nix flake.nix shell.nix CLAUDE.md</code> | 0 | No output — zero production drift after Task 11 Fix Round 1's doc-only edits | [SDL3_REVIEW_STATUS.md#audit-baseline](SDL3_REVIEW_STATUS.md#audit-baseline) |

### Task 12 independent final challenge

Reviewer: **Claude Opus** (`claude-opus-5`). One top-level shell command per row
with that command's own real integer exit code; `grep`/`pgrep`/`lsof` exit 1 on
no match, and those rows record 1 honestly. This gate started **no** process and
killed none — the two `ps`/`lsof` rows observe processes that predate it by days.

| Timestamp | Command | Exit | Result | Evidence |
|---|---|---|---|---|
| 2026-07-29T23:10:04Z | <code>git diff --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 HEAD -- src server tests tools android web cmake docker .github CMakeLists.txt CMakeListsEmscripten.txt README.md SetupServer.md WASM_PORT.md start-server.sh netlify.toml default.nix flake.nix shell.nix CLAUDE.md</code> | 0 | No output — confirmed zero production drift **before** the challenge began, so every citation below is read from the pinned baseline | [Audit baseline](#audit-baseline) |
| 2026-07-29T23:11:12Z | <code>git ls-tree -r --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 &gt; /tmp/fb-sdl3-audit/t12-pinned-tree.txt</code> | 0 | **3623** tracked paths at the pinned commit | `/tmp/fb-sdl3-audit/t12-pinned-tree.txt` |
| 2026-07-29T23:11:30Z | <code>grep -oE '^\&#124; `[^`]+`' docs/audit/FILE_COVERAGE.md &#124; sed 's/^\&#124; //; s/`//g' &#124; sort &gt; /tmp/fb-sdl3-audit/t12-coverage-paths.txt</code> | 0 | **237** paths extracted from `FILE_COVERAGE.md`'s own rows, independently of Task 11's extraction | `/tmp/fb-sdl3-audit/t12-coverage-paths.txt` |
| 2026-07-29T23:11:48Z | <code>rg '^(src&#124;server&#124;tests&#124;tools&#124;android&#124;web&#124;cmake&#124;docker&#124;\.github)/&#124;^(CMakeLists\.txt&#124;CMakeListsEmscripten\.txt&#124;README\.md&#124;SetupServer\.md&#124;WASM_PORT\.md&#124;start-server\.sh&#124;netlify\.toml&#124;shell\.nix&#124;default\.nix&#124;flake\.nix&#124;flake\.lock)$' /tmp/fb-sdl3-audit/t12-pinned-tree.txt &#124; sort &gt; /tmp/fb-sdl3-audit/t12-regenerated.txt</code> | 0 | Task 1 Step 4's own selection command re-run from scratch: **237** paths | `/tmp/fb-sdl3-audit/t12-regenerated.txt` |
| 2026-07-29T23:11:55Z | <code>diff /tmp/fb-sdl3-audit/t12-regenerated.txt /tmp/fb-sdl3-audit/t12-coverage-paths.txt</code> | 0 | **No output.** The 237-row inventory claim is independently reconciled | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T23:12:20Z | <code>comm -23 &lt;(sort /tmp/fb-sdl3-audit/t12-pinned-tree.txt) /tmp/fb-sdl3-audit/t12-regenerated.txt &#124; grep -vE '^(share&#124;bin&#124;lib&#124;po&#124;data&#124;gfx&#124;snd&#124;icons)/'</code> | 0 | Enumerated the maintained tracked paths the selection pattern excludes. Six are inside the design's stated scope and carry no coverage row: `CLAUDE.md`, `CHANGELOG.md`, `.gitignore`, `.gitmodules`, `COPYING`, `third_party/iniparser/*` (4 files). Five are cited as evidence for REL-006/REL-009/REL-014 | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T23:13:40Z | <code>awk -F'&#124;' 'NF&gt;4 &amp;&amp; \$2 ~ /`/ {print \$4}' docs/audit/FILE_COVERAGE.md &#124; sed 's/^ *//; s/ *\$//' &#124; sort &#124; uniq -c &#124; sort -rn</code> | 0 | Disposition census: **41** distinct strings summing to **237**. Largest classes 97 vendored-symlink, 21 `Complete`, 19 vendored, 15 `Reviewed; defect confirmed`, 14 `Reviewed; no defect`. Every class sampled. No row uses the design's *Excluded* or *Blocked* classes | [subsystems/09-final-challenge.md#static-review](subsystems/09-final-challenge.md#static-review) |
| 2026-07-29T23:13:55Z | <code>grep -ci 'pending' docs/audit/FILE_COVERAGE.md</code> | 0 | **1** at the time of measurement — the single occurrence was inside the inventory-rule paragraph that defines the word, so the "0 rows carry a pending disposition" claim holds. The whole-file form is self-referential (this gate's own verification note later added a second prose occurrence), so the check was restated in cell-restricted form; see the next row | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T23:39:20Z | <code>awk -F'&#124;' 'NF&gt;4 &amp;&amp; \$2 ~ /`/ {print \$4}' docs/audit/FILE_COVERAGE.md &#124; grep -ci pending</code> | 1 | **0** (exit 1 = no match) — no disposition cell in the ledger contains the word, in a form that stays true regardless of later prose. `FILE_COVERAGE.md`'s rule paragraph and [subsystems/09-final-challenge.md#static-review](subsystems/09-final-challenge.md#static-review) both now state this form instead of the self-referential whole-file one | [FILE_COVERAGE.md](FILE_COVERAGE.md) |
| 2026-07-29T23:15:10Z | <code>python3 -c "…resolve every Markdown link reference in FILE_COVERAGE.md, FINDINGS.md, SDL3_REVIEW_STATUS.md and subsystems/*.md against the target files' real GitHub-style heading anchors…"</code> | 0 | `total bad: 0` — **0** missing files and **0** missing anchors repository-wide across the audit documents | [subsystems/09-final-challenge.md#static-review](subsystems/09-final-challenge.md#static-review) |
| 2026-07-29T23:16:02Z | <code>for f in 0*.md; do echo "--- \$f"; grep -n '^## ' \$f; done</code> | 0 | Notebooks 01-08 each carry exactly the ten required headings, once each, in the required order; 09 matches after this gate wrote it | [subsystems/09-final-challenge.md](subsystems/09-final-challenge.md) |
| 2026-07-29T23:16:40Z | <code>grep -oE '^\&#124; (BUG&#124;SEC&#124;REL&#124;IMP)-[0-9]{3}' docs/audit/FINDINGS.md &#124; sed 's/^\&#124; //' &#124; sort &gt; /tmp/fb-sdl3-audit/t12-ids.txt</code> | 0 | **97** registry rows extracted (pre-BUG-052 state) | `/tmp/fb-sdl3-audit/t12-ids.txt` |
| 2026-07-29T23:16:50Z | <code>python3 -c "…group /tmp/fb-sdl3-audit/t12-ids.txt by class and report max, count and missing numbers…"</code> | 0 | `BUG max 51 count 51 missing []`, `IMP max 24 count 24 missing []`, `REL max 15 count 15 missing []`, `SEC max 7 count 7 missing []` — **0** duplicates, **0** gaps. Next free IDs confirmed before allocating BUG-052 | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:17:05Z | <code>awk -F'&#124;' 'NF&gt;5 &amp;&amp; \$2 ~ /(BUG&#124;SEC&#124;REL&#124;IMP)-[0-9]/ {gsub(/^ +&#124; +\$/,"",\$3); print \$3}' docs/audit/FINDINGS.md &#124; sort &#124; uniq -c</code> | 0 | `96 confirmed` + `1 dismissed` — **0** rows left in `suspected` or `investigating` | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:20:11Z | <code>git ls-tree -r 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 android/app/jni/include/SDL2/ &#124; grep -c '^120000'</code> | 0 | **97** — REL-005's symlink count reproduced exactly | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:20:25Z | <code>grep -c 'if: false' .github/workflows/build.yml</code> | 1 | **0** matches (exit 1 = no match) — REL-009's "0 of 11 jobs disabled" reproduced | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:20:33Z | <code>grep -cE '^  [a-z][a-zA-Z0-9_-]*:\$' .github/workflows/build.yml</code> | 0 | **13** top-level two-space keys, of which `push:` and `pull_request:` belong to `on:` — i.e. **11** jobs, reproducing REL-009's denominator | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:20:41Z | <code>grep -cE 'uses:' .github/workflows/build.yml</code> | 0 | **27** — REL-011's `uses:` count reproduced | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:20:49Z | <code>grep -c 'uses: josephbmanley' .github/workflows/build.yml</code> | 0 | **5** — REL-011's `@master` count reproduced; all five are the itch.io publish action, at `:524`, `:543`, `:562`, `:581`, `:600` | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:21:02Z | <code>grep -oE '[A-Za-z0-9_.+-]+\.dll' .github/workflows/build.yml &#124; sort -u &#124; wc -l</code> | 0 | **21** — confirms REL-013's Fix Round 1 correction (20 → 21) with an independent extraction | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:21:15Z | <code>grep -cE 'CMAKE_OSX_ARCHITECTURES&#124;universal&#124;-arch ' .github/workflows/build.yml CMakeLists.txt</code> | 1 | `build.yml:0` and `CMakeLists.txt:0` (exit 1 = no match in either) — REL-012's zero-occurrence claim reproduced | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:21:28Z | <code>grep -n 'frozenbubble' .github/workflows/build.yml</code> | 0 | Password literal at `:396`, `:397`, `:404`, `:406` — **four** occurrences spanning the two steps REL-007's corrected citation names; `:394` and `:405` are the key *alias*, correctly excluded from that count | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:21:44Z | <code>git ls-tree -r --name-only 09d6c7bfcd864a0ad3951b87d16a88dc770392a3 third_party/</code> | 0 | Exactly **4** files — `dictionary.c/.h`, `iniparser.c/.h`; no licence, README or version marker. REL-014 reproduced | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:21:52Z | <code>grep -ci iniparser COPYING</code> | 1 | **0** (exit 1 = no match) — the repository's single licence file does not mention iniparser. REL-014 reproduced | [FINDINGS.md](FINDINGS.md) |
| 2026-07-29T23:24:30Z | <code>grep -rn 'currentGame = new\&#124;delete currentGame' src</code> | 0 | **5** `currentGame = new GameRoom()` sites and **0** `delete currentGame` anywhere — BUG-013's room-object leak confirmed independently | [subsystems/02-network-client-sync.md#confirmed-findings](subsystems/02-network-client-sync.md#confirmed-findings) |
| 2026-07-29T23:25:10Z | <code>grep -n 'recvBufferLen' src/networkclient.cpp</code> | 0 | Every mutation site: `:38` (constructor 0), `:843` (guard), `:845` (increase), `:894`/`:914`/`:916` (the only reductions — all inside the guard's body). Establishes BUG-052's absorbing state | [subsystems/09-final-challenge.md#confirmed-findings](subsystems/09-final-challenge.md#confirmed-findings) |
| 2026-07-29T23:25:40Z | <code>grep -n 'list_games_str\[\&#124;list_playing_geolocs_str\[\&#124;list_game_str\[\&#124;mapping_str\[\&#124;can_start_msg\[' server/game.c</code> | 0 | `list_games_str[16384]` (`:134`) against the client's `BUFFER_SIZE` 4096 — the server-side half of BUG-052's length mismatch | [subsystems/09-final-challenge.md#confirmed-findings](subsystems/09-final-challenge.md#confirmed-findings) |
| 2026-07-29T23:26:05Z | <code>grep -rn 'setoptions&#124;players_conn\[0\] == fd' server/game.c</code> | 0 | `setoptions` gates on `if (g->players_conn[0] == fd)` at `:405` and answers `wn_not_creator` at `:415` — **falsifies** notebook 04's "any room member can emit `SETOPTIONS`"; SEC-004 and SEC-007 re-scoped | [subsystems/04-lobby-settings-input.md#trust-boundaries-and-invariants](subsystems/04-lobby-settings-input.md#trust-boundaries-and-invariants) |
| 2026-07-29T23:27:12Z | <code>grep -rn 'DoSnipIn&#124;TakeSnipOut' src</code> | 0 | `DoSnipIn` at `mainmenu.cpp:497` and `bubblegame.cpp:1012` captures only; the single `effect()` producer is `TakeSnipOut` at `bubblegame_render.cpp:1173` — **reverses** BUG-041's Fix Round 1 trigger correction | [subsystems/05-render-audio.md#confirmed-findings](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-29T23:27:25Z | <code>grep -rn 'firstRenderDone' src</code> | 0 | Cleared at `bubblegame.cpp:1013`, `bubblegame.cpp:1363` (`QuitToTitle`), and by the `bubblegame.h:479` initializer; read only at `bubblegame_render.cpp:1172`. Clearing it is what arms the animation, so menu return **is** a BUG-041 trigger | [subsystems/05-render-audio.md#confirmed-findings](subsystems/05-render-audio.md#confirmed-findings) |
| 2026-07-29T23:28:40Z | <code>grep -rn 'clearMode' src/bubblegame.h src/bubblegame\*.cpp</code> | 0 | One hit — the `SetupSettings` declaration at `bubblegame.h:267`. **Zero** readers in any gameplay translation unit, confirming BUG-018's root cause rather than only its symptom | [subsystems/03-gameplay.md#confirmed-findings](subsystems/03-gameplay.md#confirmed-findings) |
| 2026-07-29T23:29:15Z | <code>grep -rn 'GAMEPAD_REMOVED&#124;JOYSTICK_REMOVED&#124;SDL_CloseGamepad&#124;controllers.erase&#124;controllers.clear' src</code> | 0 | One unrelated hit in `bubblegame.cpp:208`; no removal handling for `FrozenBubble::controllers`, so slots are never released — BUG-035 confirmed | [subsystems/04-lobby-settings-input.md#confirmed-findings](subsystems/04-lobby-settings-input.md#confirmed-findings) |
| 2026-07-29T23:30:02Z | <code>grep -rn 'SDL_SetWindowFullscreen&#124;SDL_CreateRenderer&#124;SDL_DestroyRenderer&#124;SDL_CreateWindow' src</code> | 0 | One create/destroy pair (`frozenbubble.cpp:135`, `:152`, `:198`) and two `SDL_SetWindowFullscreen` calls; no renderer-recreation path, so no texture can outlive its renderer across a state change — the brief's "renderer/audio teardown" category yields nothing | [subsystems/09-final-challenge.md#trust-boundaries-and-invariants](subsystems/09-final-challenge.md#trust-boundaries-and-invariants) |
| 2026-07-29T23:31:32Z | <code>lsof -nP -iTCP -sTCP:LISTEN &#124; grep -c fb-server</code> | 0 | **4** listeners — the same four foreign `fb-server` processes Task 10 enumerated and left untouched. This gate started none | [Processes and cleanup](#processes-and-cleanup) |
| 2026-07-29T23:32:05Z | <code>ps -o pid,etime,command -p \$(pgrep -x fb-server &#124; tr '\n' ',' &#124; sed 's/,\$//')</code> | 0 | Elapsed times 4 d 06 h, 4 d 06 h, **1 d 21 h**, 3 d 20 h — all predate this gate. The 1 d 21 h entry is `tools/server_tests/../../build/server/fb-server -p 15113 -q -l -z`, i.e. REL-002's own predicted orphan | [subsystems/09-final-challenge.md#dynamic-evidence](subsystems/09-final-challenge.md#dynamic-evidence) |
| 2026-07-29T23:32:20Z | <code>lsof -nP -p 74458 -a -i</code> | 0 | That orphan holds `UDP *:1511` and `TCP *:15113`. Because `create_udp_server` answers a failed bind with `exit(EXIT_FAILURE)` (`server/net.c:766-769`), it denies LAN hosting to every later `fb-server -l` on this host — REL-002 upgraded from static to **observed**. Not terminated: it is not this gate's process | [FINDINGS.md](FINDINGS.md) |

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

- Task 10 executed **31** of its **41** recorded matrix rows (Fix Round 1 split the original 38-row matrix's bundled row 38 into four individually countable manual-observation rows). The **10** it did not execute are: the six brief Step 5 hostile-transport scenarios (fragmentation/coalescing, mid-frame disconnect, frames claiming a foreign player id, duplicated `n`/`F`/`S` frames, reordered `b`/`N`/`T` sync frames, bounded flooding), **omitted because the user explicitly restricted security-specific runtime testing**, exactly as Task 3's runtime security matrix and Task 6's security runtime rows were omitted; and four manual visual/audio rows (the clear-win banner and its sound, spectator pinning on screen, the >5-player mini-board paging, and the malus/attack visuals), omitted because no display, audio device, or input-injection path existed. These are limitations, not passes. SEC-002 through SEC-007 therefore still have no runtime evidence, and BUG-006, BUG-007, BUG-014 and BUG-017 remain statically argued.
- Task 10's stats-file isolation failed for the entire gate, and this was found in review, not prevented (Fix Round 1). `server/stats.c:82-91` derives `stats_file_path` from `getenv("HOME")` unconditionally, independent of working directory; `run_case.sh` isolated `joiners.log` by `cd`-ing into a scratch directory but never set `HOME`, so all 24 `fb-server` launches read from and wrote to the operator's real `~/.fb-server/stats.dat`. Registered as **REL-015**: the server has no flag, cwd-relative path, or `HOME`-independent override to relocate this file.
- Task 10 never drove the shipped client through its menus. SDL's dummy video driver accepts no externally injected input and `src/main.cpp:27` takes no arguments, so the client layer was covered in two pieces — a whole-program startup/shutdown smoke and a production-object `NetworkClient` harness against a live server — which together do not equal one human-driven session. This is the same omission Task 7 recorded as "full-client navigation omitted". Single-player gameplay was likewise not played; its rules are covered by Task 5's harness, re-run here.
- Task 10 exercised macOS and raw TCP only. No WebSocket transport was driven, so `server/ws.c`'s handshake and framing (BUG-006, SEC-002) went untested, and nothing in that gate speaks to WASM's `networkclient_wasm.cpp` (BUG-014), to Windows socket typing (REL-003), or to Android.
- Task 10's harness is a faithful protocol peer, not a faithful game peer: its `f`/`s`/`S`/`F`/`n` payloads are well-formed and deterministic but do not simulate board physics, so its rows prove wire-level and lifecycle invariants, not that two real clients' boards would agree. Level synchronization (`b`/`N`/`T`) was not driven, because only a real leader generates it.
- Task 10's BUG-049 was characterised only functionally, by varying the seat count. No attempt was made to steer the freed allocation or to reach the `stats_record_win` dangling-pointer path; that consequence is a code-supported argument, not a reproduction. Every Task 10 run used `detect_leaks=0` because Apple ASan rejects `detect_leaks=1`, so **no leak claim is made from any of them**.
- Task 10's `CREATE r 21` row measures the server's protocol contract, not a reachable user action: `src/mainmenu_internal.h:35` offers only `kRoomSizes[3] = {5, 10, 20}`, so the shipped client cannot request an out-of-range cap. That is why the silent fallback is IMP-024 and not a defect.

- **Task 12 challenged every security finding statically only**, per the standing
  user restriction. No exploit attempt, hostile-traffic test, fuzz run, or
  offensive runtime probe was made, so SEC-001 through SEC-007 remain
  code-supported inferences. SEC-002's newly established consequence in
  particular — that a `Content-Length` wrapping to exactly `-1` yields
  `bufsize == 0` and a `recv` length of `(size_t)-1` at `server/net.c:1253`,
  i.e. an attacker-length write past a zero-size allocation — is an **open
  limitation, not a resolution**: settling it would require exactly the class of
  testing that is out of scope.
- **BUG-052 is unreproduced.** Its causal chain is complete and every element is
  cited at the pinned commit, but no run drove a >4095-byte server line into a
  real client. Reproduction needs either ~165 concurrent lobby connections or a
  deliberately over-long line, and the latter is barred by the restriction
  above. It is the one finding registered in this audit with no runtime evidence
  of its own.
- **Task 12 sampled, rather than exhaustively swept, the class of observation
  that a notebook concedes inline and sets aside without opening it as a
  candidate.** A bounded language sweep over notebooks 01-08 found **6** such
  passages; five are sound and one — `02-network-client-sync.md:153` — concealed
  BUG-052, a High-severity defect. Because the audit's completion condition is
  "every *candidate* is confirmed or dismissed", an observation that never
  becomes a candidate is not covered by it. A language sweep cannot be complete
  over ~5,000 lines of notebook prose, so this stands as the residual risk with
  the highest demonstrated yield in this audit.
- **Task 12 re-derived BUG-019 and BUG-025 only to the level of their cited
  mechanism.** Their numeric reproductions — the simultaneous-loss ordering and
  the 75 px tunnelling geometry — were accepted from Task 5's production-object
  harness logs rather than re-run. REL-010 likewise retains its Task 9 premise:
  certbot's ECDSA-by-default behaviour is taken from documentation, and the
  destructive `openssl req -x509` branch remains unexecuted.
- **Task 12 compiled nothing and launched nothing**, so it contributes no
  dynamic, sanitizer or leak evidence of its own. Apple ASan's inability to
  detect leaks is unchanged, so no leak conclusion anywhere in this audit —
  BUG-001, BUG-008, BUG-013, BUG-041, BUG-042 — rests on a sanitizer pass; all
  rest on ownership tables, grep-verified destroy-site absence, and RSS
  measurement.
- **`FILE_COVERAGE.md` uses three of the design's five disposition classes.**
  No row is *Excluded with a recorded reason* or *Blocked with a recorded
  limitation*; blocked **checks** are recorded in this section instead. A reader
  of the ledger alone therefore cannot see which rows depend on an unavailable
  platform.
- **Six in-scope maintained paths carry no coverage row** because Task 1 Step
  4's selection pattern does not match them: `CLAUDE.md`, `CHANGELOG.md`,
  `.gitignore`, `.gitmodules`, `COPYING`, and the four `third_party/iniparser/*`
  files. Five are cited as evidence for REL-006, REL-009 or REL-014. The
  `CLAUDE.md`/`CHANGELOG.md` half was already recorded above by Task 9; the rest
  is recorded here and in `FILE_COVERAGE.md`. Asymmetry noted: the Android
  duplicate `android/app/jni/iniparser/*` has four rows while the primary copy —
  the one statically linked into every desktop, Windows and WASM artifact — has
  none.
- **`SDL_GAMEPAD_BUTTON_COUNT` was read from the host's Homebrew SDL 3.4.10
  header**, not from the pinned 3.4.4 submodule, when Task 12 confirmed
  BUG-036's 20-vs-26 stride. The value is 26 in both families, but the
  measurement is host-derived.

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
  directory and files were confirmed pre-existing by listing and timestamps
  before the 13:38Z→13:39Z isolation probe, hashed at 13:40Z immediately after
  isolation succeeded, and verified byte-identical against that hash at 13:55Z
  after the stateful matrix run.
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
- Task 10 is the first gate that started listeners. It launched **24** `fb-server` instances (23 from the ASan+UBSan tree, 1 from the Release tree), one shipped client, one production-object `NetworkClient` harness, one preference-path probe, and the scenario drivers. Every server bound a **dedicated** TCP port confirmed free beforehand — the 24 ports `25610`-`25623`, `25626`-`25631` and `25640`-`25643`. Port 1511 was never used and `-l`/`-L` were never passed, so no UDP listener was ever created. Every server ran from its own working directory under `/tmp/fb-sdl3-audit/task10/run/`, which is where all 25 `joiners.log` side-effect files landed; the single `joiners.log` in the repository root predates this gate by a day.
- Task 10's cleanup was proven, not assumed. `lsof -nP -iTCP:25610-25650` exited **1** with no output, and a per-port sweep reported all **24** ports FREE. The four `fb-server` processes still running — ports 15511, 15512, 15113, 15998, three of them from a different repository — are the same four that were enumerated **before** the first launch: `diff` of the two snapshots produced no output and exited 0, and `ps -o etime` shows elapsed times of 4 d, 4 d, 1 d and 3 d, all predating this gate. They were neither touched nor counted. `pgrep` for the harness, scenario, client and probe names exited **1**, and `kill -0` on the one long-lived client PID exited **1**. No Task 10-owned server, client, harness, listener, proxy or background process remains.
- Task 10's client processes used dummy video/audio drivers and an isolated `CFFIXED_USER_HOME`, whose resolved `SDL_GetPrefPath` was printed and checked **before** any client ran. The user's real preference files still carry their `Jul 28 08:59:49 2026` mtimes. All Task 10 harness sources, binaries, logs and journals live under `/tmp/fb-sdl3-audit/task10/` as local regenerable evidence owning no external state; every log backing a finding was preserved.
- **Task 12 started no server, listener, client, proxy, container, harness,
  build, or background process, and killed none.** It bound no port, opened no
  preference file, compiled nothing, and ran no sanitizer. Its only writes are
  the tracked documents under `docs/audit/` and four scratch files —
  `t12-pinned-tree.txt`, `t12-regenerated.txt`, `t12-coverage-paths.txt`,
  `t12-ids.txt`, plus a transient `t12-out.txt` — all under
  `/tmp/fb-sdl3-audit/`, all local regenerable evidence owning no external
  state and containing no credentials.
- Task 12's only runtime observation is **passive**. `lsof -nP -iTCP -sTCP:LISTEN
  | grep -c fb-server` reported the same **4** foreign `fb-server` listeners
  Task 10 enumerated, with `ps -o etime` elapsed times of 4 d 06 h, 4 d 06 h,
  1 d 21 h and 3 d 20 h — every one predating this gate. They were read and left
  untouched, exactly as Task 10 left them. One of them, PID 74458
  (`tools/server_tests/../../build/server/fb-server -p 15113 -q -l -z`, 1 d 21 h),
  holds `UDP *:1511` and `TCP *:15113` and is REL-002's own predicted orphan; it
  was **deliberately not terminated** because it is not this gate's process, and
  it is reported for the operator to clear before any LAN-hosting test, since
  `create_udp_server`'s `exit(EXIT_FAILURE)` on a failed bind makes every later
  `fb-server -l` on this host die at startup while it lives.
