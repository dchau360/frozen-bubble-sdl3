# Remediation status

Tracks which findings from [FINDINGS.md](FINDINGS.md) are fixed and which remain.
Generated from the ledger cross-referenced against commit messages since
`v2.4.27`, so a finding counts as fixed only if a commit cites its ID and the
citing commit actually resolves it (not just references it as rationale).

**As of the round and match state remediation through `a0f9b4b5`.** Last
release: `v2.4.33` (all five platforms green, itch.io deploys succeeded).

## Position

| | Total | Fixed | Open |
|---|---|---|---|
| High | 15 | 15 | 0 |
| Medium | 45 | 32 | 13 |
| Low | 13 | 9 | 4 |
| **Defects** | **73** | **56** | **17** |
| Improvements | 24 | 17 done/partial, 1 moot | 6 |

The improvement tally retains partially completed IMP-015 in the
done/partial column. The six fully open improvements are IMP-006, IMP-016,
IMP-017, IMP-019, IMP-020, and IMP-021.

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
  CTest fixture; the rendered draw presentation remains unchecked.
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
  setup path plus controlled regression mutations; keyboard/controller visual
  navigation remains unchecked.
- **BUG-024** — remote clear wins now commit once regardless of whether the
  win announcement or replicated stick resolution is processed first;
  `f5ec0509`. Verified by both message-order CTest fixtures; no two-client
  visual session was run.
- **BUG-025** — maximum-delta local full-size and mini shots now substep their
  collision checks instead of tunneling through occupied bubbles; `f2f7692f`,
  with strengthened contact-local fixtures in `a0f9b4b5`. Verified by both
  production-object fixtures and a one-step mutation; bank and vertical shots
  remain unchecked in live gameplay.
- **IMP-018** — complete: the permanent production-object gameplay harness is
  registered as `bubblegame-rules-test` in CTest and covers the round/match,
  chain-topology, and maximum-delta regressions above.
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

## Open defects

**Medium (13)** — BUG-002, 004, 006, 011, 013, 014, 015, 017, 027, 037, 040,
046, 048

**Low (4)** — BUG-010, 038, 039, 047

## Open and partial improvements

- **IMP-006** — bounded numeric conversions/integer-division intent (High
  effort; no proven defect, pure code-quality).
- **IMP-015 (partial)** — see caveat above.
- **IMP-016, IMP-017, IMP-019..021** — the remaining test-infrastructure items:
  protocol/parser unit tests, packaged-artifact smoke tests, and the Linux
  leak-regression job. IMP-018's CTest gameplay harness is now complete.
- **IMP-008** — effectively closed, not "done": Task 9 found zero actionable
  diagnostics in-scope and promoted nothing. No code change needed.

## Suggested next order

### 1. Protocol and lobby — BUG-002, 004, 006, 011, 013, 014, 015, 017, 037, 040

Server-side, and the protocol harness works — see the client shape in
`tools/server_tests/test_room_caps.py`. Two gotchas that cost time:
the server pushes `SERVER_READY` on connect (drain it before reading a reply),
and it has a multi-second grace period before acting on a dropped socket, so use
an explicit `PART` when testing departure paths.

### 2. Platform — BUG-046, 048

Android asset extraction and WASM persistence (settings and highscores are
written to MEMFS and lost on reload). Both need their platform to verify.

### 3. Lows, then the remaining improvements (IMP-006, IMP-015 leftovers,
IMP-016, IMP-017, IMP-019..021)

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
  Compilation used to be the entire gate.
- **Don't trust the ledger's own prose blindly.** Several findings were written
  before later corrections; some describe files that have since been deleted
  (REL-006) and at least one described a state that no longer existed (this
  refresh found a second instance: IMP-015's `bzero` claim was already
  slightly wrong in the original finding — `blacken_()`, not just the
  commented-out `utf8key_`, is the other user). Verify each against current
  source before fixing, not just against this document.
