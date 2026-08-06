# Remediation status

Tracks which findings from [FINDINGS.md](FINDINGS.md) are fixed and which remain.
Generated from the ledger cross-referenced against commit messages since
`v2.4.27`, so a finding counts as fixed only if a commit cites its ID and the
citing commit actually resolves it (not just references it as rationale).

**As of the platform persistence remediation through `8cf21f8a`.** Last
release: `v2.4.33` (all five platforms green, itch.io deploys succeeded) — the
persistence work is on `main` and unreleased.

## Position

| | Total | Fixed | Open |
|---|---|---|---|
| High | 15 | 15 | 0 |
| Medium | 45 | 34 | 11 |
| Low | 13 | 9 | 4 |
| **Defects** | **73** | **58** | **15** |
| Improvements | 24 | 15 done, 3 partial, 1 moot | 5 fully open |

The three partial improvements are IMP-015, IMP-018, and IMP-020. Together with
the five fully open improvements (IMP-006, IMP-016, IMP-017, IMP-019, and
IMP-021), eight improvements still have work remaining. The mutually
exclusive arithmetic is 15 done + 3 partial + 1 moot + 5 fully open = 24.

All High-severity findings are fixed. Two caveats:

- **BUG-007 is only half done** — the WebSocket framing side is fixed, the
  output-queue/deadline side is not, and it needs a design rather than a patch.
- **IMP-015 is only partially done** — `AdsManager.init()` (Android, still
  never called), `__ANDROID_PORT__` (`gamesettings.h:106`, still defined), and
  the `bzero` macro / `<iconv.h>` include in `shaderstuff.h` (still present;
  used only by `blacken_()`, which itself has zero callers and is dead code
  the original finding didn't name) are all still there. Confirmed by direct
  grep against current source, not inherited from the original finding text.

## Fixed since the last refresh

- **BUG-018** — Classic games no longer end merely because one board is empty,
  while Clear Mode commits one canonical clear win; `f5ec0509`. Verified by
  production-object CTest fixtures; presentation and SFX remain unchecked in
  the manual checklist.
- **BUG-019** — simultaneous final danger-zone losses now resolve as a draw
  without transient winner credit; `f5ec0509`. Verified by a production-object
  CTest fixture and a render-facing regression that proves a draw cannot select
  either two-player winner texture; the live visual presentation remains
  unchecked.
- **BUG-021** — departures now honor continuation, team survival,
  connected-opponent, victories-limit, and terminal ready-wait rules;
  `1db437b4`, with terminal-state follow-up `1050b6c0`. Verified by departure,
  late-departure, draw, team, and restart-wait CTest fixtures; no two-client
  manual session was run.
- **BUG-022** — flipped boards reserve complete chain target groups and cancel
  invalid cross-chains; `171847bf`. Verified by focused parity and cross-chain
  CTest fixtures; no visual chain-reaction match was run.
- **BUG-023** — the active local 2–4 player panel now renders, edits, and
  propagates its victories limit; `ae3d3c20`, with coverage in `edfcf492` and
  `e7f025a2`. Verified through the real headless `MainMenu` render, input, and
  setup path plus the real local post-round Enter consumer: reached finite
  limits return to the menu, while below-limit 3P/4P rounds wait for all
  animations and advance. Keyboard/controller visual navigation remains
  unchecked.
- **BUG-024** — remote clear wins now commit once regardless of whether the
  win announcement or replicated stick resolution is processed first;
  `f5ec0509`. Verified by both message-order CTest fixtures; no two-client
  visual session was run.
- **BUG-025** — maximum-delta local full-size and mini shots now substep their
  collision checks instead of tunneling through occupied bubbles; `f2f7692f`,
  with strengthened contact-local fixtures in `a0f9b4b5`. Verified by both
  production-object fixtures and a one-step mutation; bank and vertical shots
  remain unchecked in live gameplay.
- **IMP-018 (partial)** — the permanent production-object gameplay harness is
  registered as `bubblegame-rules-test` in CTest and covers the focused
  round/match, chain-topology, and maximum-delta regressions above. The
  canonical matrix in FINDINGS.md remains open: it still lacks the complete
  1/2/5/6/20-player, 1–5-team, 5/8-colour, both-orientation, three-round,
  shrink/new-match, stale-malus/BUG-020, normal-plus-sanitized lifecycle gate.
- **BUG-045** — `TTFText` copy semantics; `aca2b403`.
- **BUG-001** — `TextureEx` lifetime/leak; `9f3fdc1f`.
- **IMP-001..005, 007, 009..015 (partial), 022, 023, 024** — the 24-item
  improvement pass, delegated in batches and independently verified:
  `95ceee2b`, `53eccc5a`, `4c3fb45c`, `0b3f1bf7`. That work introduced four
  regressions of its own (WebSocket fragmented-handshake handling, a WebSocket
  pipelined-data misroute, a plasma-transition null-deref crash, and a CREATE
  room-cap clamp that logged but didn't clamp) — all four found by direct
  empirical reproduction, not by inspection, and fixed in `f96b316c`.
- **Linux ASan/UBSan CI job** — not a finding fix, but the prerequisite
  IMP-016/IMP-021 named: `dde25951`. This is what made BUG-041/042 provable.
- **BUG-041** — `synchro_after` and the five `*_effect` functions
  (`shaderstuff.h`/`.cpp`) passed their texture pointer by value all the way
  up from `TransitionManager::transitionTexture`, so no frame's texture was
  ever visible to the next frame or to the caller: every single animation
  frame leaked one 640×480 texture, not just one per transition. Fixed by
  threading `tex` through the whole chain as `SDL_Texture*&`; `TakeSnipOut`
  now destroys the final frame's texture once the animation completes.
  Verified via RSS-growth measurement (macOS has no LeakSanitizer): pre-fix
  leaked ~6.7 MB per transition animation over 60 repeated calls, post-fix
  flat at ~127 KB/animation (measurement noise). Confirmed against the
  pre-fix code via `git stash`, not just against the fixed version alone.
- **BUG-042** — `Penguin::LoadPenguin` reloaded all 394 animation textures
  (`handle`/`wait`/`win`/`lose`) every call with no destroy of the previous
  set, and `bubbleArrays[i].hurryTexture` was reassigned the same way inside
  `NewGame`'s switch statement (up to `MAX_NET_PLAYERS` in the battle-royale
  branch) — both leaked their previous contents on every match start.
  Fixed: `Penguin` gained a destructor (so `BubbleGame`'s own shutdown, which
  never touched `penguinSprite`, doesn't leak the final match's set either)
  and destroy-before-reload guards in `LoadPenguin`; `NewGame` now destroys
  every `hurryTexture` up front, mirroring the existing `background` cleanup
  that was already there. Verified via RSS-growth measurement, isolating
  `Penguin` directly (it needs nothing from `BubbleGame`/`FrozenBubble`):
  pre-fix ~3.8 MB/call, post-fix exactly 0 KB/call over 40 calls. Note: an
  ASan build of the *same* harness showed spurious growth from ASan's
  quarantine (redzone retention) even on the fixed code — a real gotcha,
  resolved by cross-checking against a plain (non-sanitized) build instead.

- **BUG-046** — Android asset extraction left a partially populated tree behind.
  Extraction is now transactional (`867b5f12`): assets are staged and swapped
  into place, an interrupted or failed run is detected on the next launch and
  redone, and the managed tree is rebuilt in full on upgrade. Deletion is
  contained to the managed root (`9b6f1cd5`) and refuses to follow a symlink out
  of it, including one whose target is a sibling of the root (`d756daaf`) —
  found by review, not by the original finding. `c05dcbb5` added
  `tools/verify_android_assets.py`, which hashes every packaged asset against
  `share/`. Evidence: `AssetDeploymentTest.java` (JVM unit tests, covering
  changed/deleted/new assets and truncation repair) and APK hash parity over
  3352 files, both gated in CI since `8cf21f8a`. **Not verified on a device or
  emulator** — no Android runtime was executed during this remediation. The
  install-over-install procedure is recorded, unchecked, in
  [../MANUAL_TEST_CHECKLIST.md](../MANUAL_TEST_CHECKLIST.md).
- **BUG-048** — browser settings and highscores were written to MEMFS and lost
  on reload. They now live on IDBFS, hydrated before `main()` runs and flushed
  after each write, with flushes serialized and coalesced so concurrent
  `syncfs` calls cannot stack (`5de4e1e7`, `03fdccdb`). `27927781` made every
  settings and highscore mutation persist when it happens rather than only at a
  clean shutdown. Three follow-ups came out of reviewing that change: saves are
  written in full and swapped into place so an interrupted save cannot truncate
  the table (`eabe89a1`); a save touches only the table that changed
  (`885f46a4`); and `e1fd6cd8` fixed two pre-existing bugs the eager saving
  exposed — indexing the level map by position default-inserted an empty grid
  per absent id, each insertion extending the loop that fed it, so one completed
  level wrote eighteen blank grids; and both `Dispose()` methods ran the
  destructor by hand without freeing anything, which LeakSanitizer put at 958 KB
  across 20 allocations. Evidence: `tests/wasm_persistence_test.cjs` (controller
  state machine), `tools/test-wasm-persistence.mjs` (headless Chrome, reload
  against the packaged `dist-wasm`), and `tests/persistence_save_test.cpp`
  (real files, no mocks), all gated in CI since `8cf21f8a`. The leak fix is
  confirmed by the Linux ASan job, since Apple ASan has no LeakSanitizer.

## Open defects

**Medium (11)** — BUG-002, 004, 006, 011, 013, 014, 015, 017, 027, 037, 040

**Low (4)** — BUG-010, 038, 039, 047

## Open and partial improvements

- **IMP-006** — bounded numeric conversions/integer-division intent (High
  effort; no proven defect, pure code-quality).
- **IMP-015 (partial)** — see caveat above.
- **IMP-018 (partial)** — the focused CTest harness is permanent, but the
  canonical matrix and lifecycle coverage listed above remain outstanding.
- **IMP-020 (partial)** — two of its packaged-artifact assertions now run in CI:
  the WASM headless-browser reload preserves settings and highscores, and the
  APK's packaged assets match `share/` by hash. Everything else it asks for is
  still open — no packaged artifact is launched anywhere. Specifically
  outstanding: the AppImage, `.app`, and Windows installer starting under dummy
  video/audio drivers on a machine with no source tree at the baked `DATA_DIR`
  (REL-003, REL-008, BUG-034); `g_dataDir` resolving inside the package
  (REL-008); log-file location and initialization-failure reporting (BUG-047);
  the Android emulator install-over-install that BUG-046 names; the dependency
  walk for unresolved imports (REL-013); APK signing identity and `versionCode`
  (REL-007, REL-004); DMG architectures (REL-012); and `docker/setup.sh`
  leaving a pre-existing ECDSA key byte-identical (REL-010).
- **IMP-016, IMP-017, IMP-019, IMP-021** — the remaining fully open
  test-infrastructure items: protocol/parser unit tests and the Linux
  leak-regression job.
- **IMP-008** — effectively closed, not "done": Task 9 found zero actionable
  diagnostics in-scope and promoted nothing. No code change needed.

## Suggested next order

### 1. Protocol and lobby — BUG-002, 004, 006, 011, 013, 014, 015, 017, 037, 040

Server-side, and the protocol harness works — see the client shape in
`tools/server_tests/test_room_caps.py`. Two gotchas that cost time:
the server pushes `SERVER_READY` on connect (drain it before reading a reply),
and it has a multi-second grace period before acting on a dropped socket, so use
an explicit `PART` when testing departure paths.

### 2. Settings and state — BUG-027

The remaining non-protocol Medium. The platform pair that used to sit here
(BUG-046, BUG-048) is fixed.

### 3. Lows, then the remaining improvements (IMP-006, IMP-015 leftovers,
IMP-016, IMP-017, IMP-019, IMP-020 leftovers, IMP-021)

## Things to know before continuing

- **Verification gaps are tracked** in
  [../MANUAL_TEST_CHECKLIST.md](../MANUAL_TEST_CHECKLIST.md). Fixes whose code
  path was never executed are listed there rather than being implied to be
  verified.
- **BUG-036 changed the controller scancode stride** from 20 to 26, which
  invalidates saved bindings for players 2–5. Flagged for the maintainer; not
  yet confirmed as acceptable. Shipped in v2.4.33's changelog as a known,
  one-time rebind requirement.
- **CI now runs `ctest`** and, since `dde25951`, a full Linux ASan/UBSan build.
  Compilation used to be the entire gate. Since `8cf21f8a` it also runs the
  Android asset-deployment JVM tests and APK hash parity, and the WASM
  controller tests plus a headless-Chrome reload of the packaged bundle.
- **Apple ASan has no LeakSanitizer**, so a leak fix that passes locally on
  macOS proves nothing about leaks — the Linux ASan job is the gate. `leaks
  --atExit` is the local substitute; it caught the `Dispose()` leaks that a
  clean macOS sanitizer run had missed.
- **Don't trust the ledger's own prose blindly.** Several findings were written
  before later corrections; some describe files that have since been deleted
  (REL-006) and at least one described a state that no longer existed (this
  refresh found a second instance: IMP-015's `bzero` claim was already
  slightly wrong in the original finding — `blacken_()`, not just the
  commented-out `utf8key_`, is the other user). Verify each against current
  source before fixing, not just against this document.
