# Code optimization and async networking — progress and handoff

Last updated: 2026-09-08. **Merged** from two documents that grew alongside
each other: this file (the general code-optimization backlog, items A-G) and
`ASYNC_NETWORKING_HANDOFF.md` (item A's own four-stage deep dive, spun out
because it outgrew the rest of the backlog combined). Both efforts have
landed and shipped as `v2.4.92`, so they're folded into one file now, with
all the completed narrative moved down to **Archive: completed work** so
this file stays skimmable. Start at "Active work" below for current state;
go to the archive only when you need the reasoning/verification detail
behind something already done.

## Purpose and user preferences

The user requested a code efficiency review with less unnecessary complexity
and verbose commentary, then approved an initial text-cache/logging patch and
asked to commit, merge, and tag it. They requested this persistent progress
record so another AI session can take over when usage runs out. Across
several sessions they worked through an original five-item list, then a
follow-up backlog (items A-G), authorizing implementation each time
("work on them all in order", "fix the >5-cap room case too", etc.). Item A
("keep networking and server startup responsive") turned out to need a much
larger rearchitecture than the others, so it was tracked in its own document
for a multi-session, four-stage effort; that document is now merged in here
under the archive, and its own "what's left" residuals are the first thing
in the active list below. Most recently the user said "ok lets tag up",
authorizing a release without waiting for the real-device/browser-pairing
test that had been recommended as a precondition — `v2.4.92` shipped on that
explicit go-ahead.

Update this document after meaningful implementation or verification milestones
and before handing off. Record actual results, outstanding work, and blockers.
Do not infer benchmark gains from passing tests alone — measure when claiming
a speedup.

## Current checkpoint

- Repository: `/Users/dchau/gr/frozen-bubble-sdl3`
- Branch: `main`; latest source change `742d3f90` (`chore: bump version to
  2.4.93`). Tag `v2.4.93` cut and pushed.
- `main` and `origin/main` are in sync as of this checkpoint.
  Re-verify with `git fetch && git log --oneline main..origin/main` before
  trusting this further into a new session.
- CMake/Android version: `2.4.93`; Android versionCode: `76`.
- **`v2.4.92` never shipped.** Its tag build's Linux ASan/UBSan job failed,
  which hard-blocks `Create Release`/`Deploy WASM to Itch.io` by design (see
  CLAUDE.md's CI/release section) — no GitHub Release, no itch.io deploy
  happened for it. Root cause: two pre-existing bugs the sanitizer job
  caught for the first time, neither reproducible locally on macOS (leak
  detection is unsupported there at all, and `rand()`/`srand()` sequences
  are libc-specific, so the same seed behaves differently on Linux glibc):
  - `NetworkClient::currentGame` (a raw `GameRoom*`) was allocated with
    `new` on CREATE/JOIN confirmation but never `delete`d at any of its four
    reset sites (`Disconnect`, `PartGame`, `ROOM_CLOSED`, kicked) — every
    room departure leaked one `GameRoom`. Caught by `netconnect-test`'s
    reconnect-and-recreate scenario (added for stage 3a), apparently the
    first test to run a real CREATE-confirm → `Disconnect` → CREATE-confirm
    cycle inside one ASan-instrumented process.
  - `BubbleGame::PickNextBubble`/`ChooseFirstBubble` indexed
    `remainingBubbles()` via `ranrange(1, size) - 1` with no empty-vector
    guard, so a board that goes empty for an instant (a bot's queued shot
    landing the same frame the board clears, before the round-end check
    notices) hit a division by zero in `ranrange`. `bubblegame_board.cpp`'s
    own `nextBubble` re-validation already guards the identical pattern with
    `!remaining.empty()`; applied the same guard here.
  - Both fixed in `6706d6d5`, verified with a plain push to `main` first
    (CI run `34242064542`, all six build/test jobs green including Linux
    ASan/UBSan) before re-tagging as `v2.4.93` rather than force-moving the
    dead `v2.4.92` tag. **`v2.4.93` shipped successfully** — its tag-build
    run (`34243015633`) finished all six build/test jobs green plus
    `Deploy WASM to Itch.io` and `Create Release`, both confirmed ✓. This is
    the release that's actually live, not `v2.4.92`.
- Status of every backlog item: **A** (async networking) — all four stages
  landed, tagged `v2.4.93` (see above); three small residual items open,
  listed below. **B** (font sharing) — gameplay + one confirmed menu hot
  path done, rest of menu caching pending. **C, D, F** — complete. **E** —
  overlaps B, same pending scope. **G** — measured, found not justified,
  deliberately not implemented (closed, no further action).

## Active work — what's left

### Async networking (item A) residuals

All four stages of the async rearchitecture landed (see archive for full
detail). What's left, in priority order:

1. **Stage 3b's internals are still synchronous.** `WaitForBubble`/
   `WaitForNextBubble`/`WaitForTobeBubble`/`SyncNetworkLevel` were never
   turned into a resumable state machine — 3b closed the stall for the
   common case by gating entry until every sync message is already queued,
   so the loop finds its data immediately and returns without truly
   waiting. The residual case (the gate's own 5s timeout expires with
   messages still missing) still falls through to the old blocking loop,
   per-message, same as before this effort. Doing the real rewrite means
   turning ~150 lines of bubble-position math (mini-player offsets,
   per-player grid replication, launcher/next-bubble assignment) into
   resumable state with no existing automated coverage of that math to
   rewrite against — the risk/effort didn't clear the bar in this batch. A
   live two-browser WASM game exercised the mitigated path end-to-end (see
   archive) with no stalls observed, so this is optional hardening rather
   than a known live bug — pick it up only if a real multi-round game is
   ever seen to hit the residual case.
2. **WASM bug (CREATE confirmation heuristic) is still open.** Any
   non-`PART` bare `OK` still confirms a pending CREATE; unlike the other
   pre-existing WASM bugs this effort found, it was never touched, because
   nothing in the four stages' scope routed through that code path. Small,
   self-contained fix if picked up later: give CREATE the same
   request-scoping the NICK retry path got in stage 1b.
3. **A real pre-existing server bug, found while writing stage 4's test,
   was deliberately left unfixed as out-of-scope**: a `select()`-loop
   fairness gap in `connections_manager()` where one connection's sustained
   flood can make a *different* connection's inbound data invisible to
   `FD_ISSET` for the flood's whole duration. Confirmed pre-existing
   (reproduces against a clean worktree from before any stage-4 change) and
   confirmed unrelated to stage 4's `write_set` addition or to memory
   corruption (clean under ASan with `write_set` both present and
   disabled). Flagged as its own background task (`task_3c17853a`) rather
   than expanded into this effort's scope — the user has since started that
   task in a separate session; check its outcome before re-investigating.
4. **Follow-up, not a defect**: a real device/browser pairing test (not
   just two tabs on one machine) is still worth doing, since the release
   (`v2.4.93`, confirmed shipped — `v2.4.92` never actually did, see
   "Current checkpoint" above) went out without one — see the playtest
   recipe in `docs/MANUAL_TEST_CHECKLIST.md`'s "Two-browser WASM network
   playtest" section, which already flags this and the ESC/tap
   cancel-mid-connect check as uncovered by the one playtest run so far.

### B/E — menu-screen label caching (pending)

Gameplay-panel caching (item B) and its one confirmed menu hot path (the
game-room settings grid) are done — see archive. What's still open, shared
between items B and E:

- `mainmenu_teampanel.cpp` and `menulist.cpp`'s `panelText` call sites were
  inspected but not converted: almost all are one-off titles, headers, and
  footers rendered once or twice per frame (not a per-row loop churning
  against itself), so they didn't clear the "confirmed hot path" bar these
  items set.
- If picked up: measure texture-creation counts on an idle populated team
  picker first, the same way the settings grid's cell count was measured,
  before converting anything — don't share a `TTFText` across borrowers
  that later change font size, style, or alignment unless it tracks
  external font generations, or those mutations are eliminated first (see
  item B's prerequisite note in the archive).

## Suggested next session

1. Read current repository instructions and inspect git state.
2. Pick from "Active work" above — the async residuals are individually
   small and self-contained; the menu-caching item needs a measurement pass
   first, same discipline as every prior item here.
3. Record a baseline or failing regression first, then implement and verify
   in proportion to the change. Preserve input parity required by the
   repository's own instructions (CLAUDE.md's keyboard/gamepad + touch/mouse
   parity checklist).
4. Update this document after each meaningful checkpoint. Add new completed
   work to the archive section below rather than leaving it in "Active
   work" — keep the active section reflecting only what's actually still
   open.
5. Bump and tag a release only if the user asks, per the standing "always
   bump before tagging" rule (`CMakeLists.txt`, `android/app/build.gradle`
   with a strictly-increasing `versionCode`, `.github/workflows/build.yml`'s
   two fallbacks, `default.nix`, and `CHANGELOG.md`, all together).

---

# Archive: completed work

Everything below this line shipped. Kept for the reasoning, verification
detail, and measurements behind each change — skip it unless you need that.

## Code optimization — sessions 1-3

### Session 1: initial text cache + logging (commit `3f96b74a`, tag `v2.4.76`)

See the git log / tag for full detail; summary:
- `src/ttftext.h`/`.cpp`: a `TTFText` reuses its texture when the same
  renderer/text/wrap-width is set again; setters invalidate correctly;
  move-safe; failed creation stays retryable. Limitation flagged at the time:
  an object shared across multiple different strings per frame still churns,
  since the cache only remembers the object's *last* string.
- `src/logger.cpp` and call sites: default INFO priority,
  `FROZEN_BUBBLE_DEBUG=1` for DEBUG traces, buffered routine file output.
- Tests: `tests/ttftext_cache_test.cpp`, `tests/logger_priority_test.cpp`.
- Version bumped to 2.4.76 (CMake, Android, workflow fallbacks, CHANGELOG).

### Session 2: render-call-site caching (commit `d6c6aaa2`) + release `v2.4.77`

Recommended-work item 1 from the session-1 handoff: the exact "alternating
cells" churn the session-1 limitation predicted, found and fixed at the
actual render call sites in `src/bubblegame_render.cpp`.

**Problem confirmed by inspection:** several render sites reused *one* shared
`TTFText` member across a loop that rendered several different strings per
frame (once per player, once per stats-table cell, once per chat line, etc.).
Each iteration's `UpdateText()` call sees text different from the *previous
iteration's* call, so the cache invalidates and recreates a texture every
single call, every single frame — regardless of whether that particular
cell's own text had changed since the last frame.

**Fix, by call site:**
- `UpdateScoreText` — was a single shared `scoreText`, alternating between
  two players' score strings every frame in the 2-player branch (always a
  guaranteed miss). Changed to `TTFText scoreText[2]`; function now takes an
  explicit `slot` parameter; single-player call site passes `0`, the 2-player
  loop passes its own player index `i`.
- Targeting indicator — was a single shared `targetingText`, alternating
  across up to `MAX_NET_PLAYERS` targeting strings in royale mode. Changed to
  `TTFText targetingText[MAX_NET_PLAYERS]`, indexed by the render loop's own
  player index.
- In-game chat lines — was a single shared `chatLineText`, alternating across
  up to 3 displayed lines. Changed to `TTFText chatLineText[kMaxChatLines]`
  (`kMaxChatLines = 3`, a new named constant `Render()`'s local `maxShow` is
  now defined in terms of, so the two can't drift apart), indexed by display
  slot.
- `RenderRoundStats`, `RenderRoyaleHud`, `RenderMalusAlerts` — these render a
  *variable*, frame-dependent number of cells. Got a new helper instead:
  `BubbleGame::StatsPanelCell(pool, idx, fontSize = 14)` — returns pool[idx],
  lazily growing the pool (`std::vector<TTFText>`) and loading a new slot's
  font the first time an index is requested. Each panel got its own pool
  (`statsCellPool`, `royaleHudCellPool`, `malusAlertPool`). The old single
  `statsText` and `malusAlertText` members were removed entirely.
  **Caveat documented in the helper itself:** the returned reference is only
  valid until the *next* call that grows the same pool (`vector::resize` can
  reallocate) — every real call site fetches-updates-renders within one
  statement and never holds the reference across a sibling cell's call.
- `UpdatePlayerNameWinText` was inspected and found already correct at the
  time (one persistent `TTFText` per player) — no caching change needed
  there. Its position-selection logic was left alone in session 2 and
  addressed later, in session 3's item 4 below.

**New test:** `tests/statspanelcell_cache_test.cpp`, proving sibling-cell
independence, frame-to-frame cache retention, and correct pool growth.

**Verification:** full release build clean; `ctest` 23 passed / 2 skipped
(the usual sanitizer-only skips); focused ASan/UBSan pass of
`ttftext-cache-test`, `logger-priority-test`, `statspanelcell-cache-test` —
all clean (`detect_leaks=0`, this macOS ASan runtime doesn't support leak
detection).

**Release:** version bumped to 2.4.77 across `CMakeLists.txt`,
`android/app/build.gradle` (versionCode 59→60), `.github/workflows/build.yml`
(both off-tag fallbacks), `default.nix` (was already stale at 2.4.75, fixed
as a bonus, though nothing ships from it), and `CHANGELOG.md`. Pushed to
`origin/main`, tag `v2.4.77` cut and pushed (confirmed with the user before
pushing, since a tag push triggers the release/itch.io CI workflow). An
earlier local-only `v2.4.76`-on-new-commit tag from a mid-session mistake was
deliberately left unpushed and untouched per explicit user instruction
("leave it, just push v2.4.77").

### Session 3: items 2-5 from the session-2 remaining-work list

All four remaining items were completed as separate commits on `main` and
are included in the current checkpoint.

**Item 2 — broaden cache correctness coverage (`da37f18d`).**
Extended `tests/ttftext_cache_test.cpp` to cover what it didn't before: an
isolated color-invalidation case (wrap length held constant, unlike the
original which changed wrap and color together), `UpdateStyle` (both
overloads) invalidation + no-op-on-repeat, `UpdateAlignment`
invalidation + no-op-on-repeat, a renderer-change case, a
retry-after-failure case (no font loaded → null texture, then a successful
retry), and a move-construction/move-assignment case (BUG-045 history: copy
is deleted, move must work; checks the marker survives the move and the
moved-from object's `Texture()` is null).
Extended `tests/statspanelcell_cache_test.cpp` with three new end-to-end
blocks driving `RenderRoundStats` (3-player), `RenderRoyaleHud` (6-player,
one spectating), and `RenderMalusAlerts` (one aging alert) through the real
`BubbleGame` friend-accessor path — not just the `StatsPanelCell` helper in
isolation — proving cache retention across an unrelated cell's update, a
`netViewAuto` toggle, and a frame that only ages an alert's countdown.

**Item 3 — compile the shared test core once (`05b4aedd`).**
Eight test targets (`bubblegame-rules-test`, `bubbleai-test`,
`bot-play-test`, `persistence-save-test`, `localmultiplayer-settings-test`,
`sp-panel-labels-test`, `statspanelcell-cache-test`,
`menu-touch-gesture-test`) each listed `FROZEN_BUBBLE_CORE_SOURCES` (~35
files) directly in their own `add_executable()`, repeating the same
`FROZEN_BUBBLE_TEST_ACCESS`/`DATA_DIR`/`APP_VERSION` compile definitions and
SDL3/iniparser/ws2_32 link libraries eight times over — a full rebuild
compiled the same ~35 files 8 separate times (9 counting the production
`frozen-bubble-sdl3` target). Replaced with one `frozen-bubble-core-test`
static library (`PUBLIC` include dirs/compile definitions/link libraries),
so the 8 targets now just list their own test `.cpp` and link the library.
Production `frozen-bubble-sdl3` stays untouched and separately compiled — it
must not get `FROZEN_BUBBLE_TEST_ACCESS`, which opens `BubbleGame`'s private
members to test-only friend structs.
Verified with a clean reconfigure + rebuild: `bubblegame_render.cpp` (a
representative core file) now compiles twice (once for `frozen-bubble-sdl3`,
once for `frozen-bubble-core-test`) instead of nine times.

**Item 4 — avoid duplicate bot scoring (`3d36ff80`).**
`BubbleAI::ChooseShot` predicts 61 candidate angles per shot; several
neighboring angles routinely funnel into the same landing cell (the
existing "distinct landings" collapse right after the loop depends on
exactly that), yet each candidate was rescored from scratch — fresh grid
copy, flood-fill pop/detach simulation, and (Normal/Hard skill) a
full-board lookahead scan — even when it landed somewhere already scored
this decision. Added a `std::map<std::pair<int,int>, int> scoreCache`,
scoped to one `ChooseShot` call, so a landing cell already scored this
decision is looked up instead of recomputed. Confirmed by code-reading that
`PredictLanding` never mutates the board (traced into
`GetClosestFreeCell`), the key precondition for this cache being sound.
Preserves angle selection, tie behavior, and RNG consumption exactly.
**Measured** (not inferred from passing tests) with a throwaway benchmark
linking the built `frozen-bubble-core-test` library: a busy, varied-color
board, 3000 `ChooseShot` calls at `Skill::Hard`, same RNG seed/iteration
count before vs. after: 2.4446 ms/call → 0.7640 ms/call, checksum of summed
scores identical (145200) both times — confirming identical output, not
just faster wall time. The ~3.2x speedup is specific to this
cache-favorable scenario (many angles funneling into a few open cells), not
a general frame-time claim.

**Item 5 — targeted readability cleanup (`459e3d7a`).**
Found and fixed one genuine duplicate `#include "frozenbubble.h"` in
`src/bubblegame_render.cpp` (was present twice, back to back). Audited every
"line N" / "original: ..." comment across all files touched this session —
all are legitimate cross-references to the immutable original Perl source
(`bin/frozen-bubble`), per CLAUDE.md's own instruction to preserve these;
none were stale or removed. Replaced `UpdatePlayerNameWinText`'s
`switch(playerCount) { case 3/4/5/default }` position logic — three
near-identical case bodies plus a royale default re-deriving a subset of the
same coordinates separately — with a free function `PlayerSlotPosition
(playerCount, playerIdx, parkedSlot)` built on one 5-slot table plus one
index-selection rule, declared in `bubblegame_internal.h` alongside the
existing `StatsPlayerName` (same sharing pattern). Carefully preserved one
subtle behavior: royale's out-of-range-parkedSlot fallback resolves to table
slot 1 (`{83,2}`), matching the original's own local fallback exactly (easy
to flip to slot 0, the *other* "fallback-feeling" center slot, in a
refactor). New test `tests/player_slot_position_test.cpp` — a pure function
of three ints, so tested directly rather than through a friend accessor —
covers 3/4/5-player fixed layouts, all four royale corner positions, and the
fallback case.

**Verification completed (session 3):**
- Full release build (`cmake -B build -G Ninja && cmake --build build
  --parallel`): all targets built clean after each item, including the two
  new test targets (`statspanelcell-cache-test` extensions and the new
  `player-slot-position-test`).
- Full suite: `ctest --test-dir build --output-on-failure` → **24 passed, 0
  failed, 2 skipped** (up from session 2's 23; the 2 skips are still the
  expected sanitizer-only `server-massleave-test` / `server-udp-probe-test`).
- Focused ASan/UBSan passes after each item (`detect_leaks=0` — this macOS
  ASan runtime doesn't support leak detection —
  `fast_unwind_on_malloc=0`): `ttftext-cache-test`,
  `statspanelcell-cache-test`, `bubbleai-test`, `bot-play-test`, and the new
  `player-slot-position-test` all clean.
- Real (not inferred) performance measurement for item 4's bot-scoring cache,
  described above.
- **Not done:** no interactive/visual verification in the actual running
  game, no Android/Windows/WASM/iOS build. Correctness rests on the unit
  tests + full suite + ASan passes, not on having watched a live game.

### Session 3, continued: Auto-balance flood-disconnect bugfix (`v2.4.78`)

Not part of the original optimization scope — the user hit this during
ordinary use right after items 1-4 shipped ("i noticed that when i cycle thru
the auto 2,3,4,5 teams with 4 bots ... i get kicked out of the game room").
Recorded here anyway since it's real work on this codebase and later sessions
need accurate state regardless of which effort it belongs to.

**Root cause:** the server (`server/game.c`, `amount_talk_flood`/`talk()`)
disconnects any client that sends 15 TALK (chat) messages inside one minute.
The full-screen team picker's "Auto 2/3/4/5" buttons
(`src/mainmenu_teampanel.cpp`) applied every occupied seat in a loop, and
`ApplyTeamChoice` sent one `"!team:<nick>:<n>"` TALK message per seat
*unconditionally* — including the host's own change in a <=5-cap room, where
`SyncRoomOptions()` already broadcasts every player's team via SETOPTIONS'
`PLAYERTEAM_Pn` fields, making that TALK pure redundant chatter. A full
5-player room (host + 4 bots, the reported case) sent 5 TALKs per tap; 3 taps
(5x3=15) hit the flood limit exactly and disconnected the host mid-cycle.

**Fix (commit `ff93a0a1`):** the host's own <=5-cap-room path no longer sends
that TALK at all (SyncRoomOptions already covers it); a non-host's own row
and any >5-cap-room change (which has no per-slot team field in SETOPTIONS)
are unchanged. Added an `announce` parameter to `ApplyTeamChoice` so
Auto-balance can defer the host's broadcast to one call after the whole
batch instead of one per seat, and skips seats whose team doesn't change on
a re-tap.

**Test:** added a test-only `SendTalk` call counter to `NetworkClient`
(guarded by `FROZEN_BUBBLE_TEST_ACCESS`) and a new block in
`tests/menu_touch_gesture_test.cpp` that reproduces the exact report (host +
4 bots) and asserts zero TALK sends across an Auto 2→3→4→5 cycle. Verified
this actually catches the bug by stashing the fix and re-running it first
(failed as expected), then restored and re-verified.

**Verification:** full release build + `ctest` (24 passed, 2 skipped as
expected); ASan/UBSan pass of `menu-touch-gesture-test` clean.

**Release:** version bumped to 2.4.78 (same four files as every release, plus
`CHANGELOG.md`), pushed to `origin/main`, tag `v2.4.78` cut and pushed.

### Session 3, continued again: the >5-cap room case too (commit `c5665562`)

The user asked to fix the >5-cap room gap flagged above right after
`v2.4.78` shipped — a >5-cap room has no per-slot team field in SETOPTIONS,
so its Auto-balance TALK-per-seat traffic is not redundant the way the
<=5-cap case was, and couldn't just be dropped. Left unbatched, a *single*
Auto tap in a room with 15+ players (all real seats today — `MAX_NET_PLAYERS`
is 20) would by itself reach the server's 15-TALK/minute flood-kick limit,
worse than the original bug (which took three taps in a 5-player room).

**Fix:** added `MainMenu::ApplyTeamChoicesBatch(changes)` — applies every
`{slot, team}` pair locally (same per-slot logic `ApplyTeamChoice` already
had), then sends ONE combined wire update for the whole batch: a single
`SyncRoomOptions()` for a <=5-cap room, or one
`"!teamset:nick1=t1,nick2=t2,..."` TALK for a >5-cap room (parsed in
`NetPanelChatDockRender`'s existing ">5-cap: every client applies" block).
Nicknames are server-validated to <=10 chars of `[A-Za-z0-9_-]`
(`server/game.c`: `is_nick_ok`), so `,`/`=` can never collide with one, and
a full 20-seat batch comes to well under 300 bytes — nowhere near the
server's 1000-byte TALK relay buffer. The Auto-balance tap handler now
collects its changed seats and calls this once per tap instead of looping
`ApplyTeamChoice`; `ApplyTeamChoice` itself lost the `announce` parameter
the previous fix added, now superseded by this dedicated batch function.

**Test:** extended the existing 6-player >5-cap test to assert exactly one
TALK per Auto tap (was only checking correctness before); added a 16-seat
room (host + 15 bots) case asserting the same across four consecutive taps;
added a receiving-side case (new `NetworkClientTestAccess::PushChatMessage`
+ `MainMenuTestAccess::RenderChatDock`) that pushes a synthetic
`"!teamset:"` message and confirms it's actually parsed and applied,
including that a later batch overwrites rather than merges with an earlier
one. Verified all of it by stashing this fix (keeping the `v2.4.78` fix in
place) and re-running — 10 checks failed across every new/extended
assertion — then restored and re-verified everything passes.

**Verification:** full release build + `ctest` (24 passed, 2 skipped as
expected); ASan/UBSan pass of `menu-touch-gesture-test` clean.

## Items B-G: bounded fixes and measurements (session 4, 2026-09-07/08)

A new backlog was drawn up by source inspection at `b1c217e0` (not measured
new speedup claims), then worked through in priority order.

### A. Bounded connect/startup fixes (`00faeaf4`)

Item A's original scope (evidence: `NetworkClient::Connect` used blocking
DNS/connect and a handshake wait; the leader-start path polled inside
`HandleServerResponse`; `SendAll` retried with sleeps on Windows but POSIX
sockets stayed blocking for sends; `MainMenu::StartServer` slept
unconditionally for one second) was split into two bounded, low-risk fixes
landed first, with the larger main-loop-driven async rearchitecture
deliberately scoped out as its own batch (that batch is the "Async
networking rearchitecture" section below).

**What landed:**
- `NetworkClient::Connect`'s initial `connect()` is now non-blocking +
  `select()`-bounded to 5s on both platforms (previously a plain blocking
  call, bounded only by the OS's own TCP connect timeout -- commonly tens of
  seconds, sometimes minutes, and not this codebase's choice). Uses the same
  non-blocking-connect-then-`SO_ERROR` idiom already shipping in this file's
  `MeasureLatency`/`IsReachable`, not a new pattern.
- The same change puts the POSIX socket in non-blocking mode from before
  `connect()` (previously only Windows did this, and only after a successful
  blocking connect). This closes the second gap the evidence names directly:
  `SendAll`'s bounded retry-on-`EWOULDBLOCK`/`EAGAIN` loop already existed but
  was dead code on POSIX, since a blocking socket's `send()` never returns
  those -- it just blocks in the kernel with no bound at all when the peer
  stops reading. POSIX now gets the same non-blocking socket Windows already
  had, so that retry path is reachable on both platforms.
- `MainMenu::StartLocalServer`'s fixed 1-second `SDL_Delay` is now a
  20ms-interval poll (reusing the existing `portInUse()` connect-probe as a
  readiness check), bounded to 2s, that also still catches an immediate
  child-process exit. The common case (fork+exec+bind+listen, normally
  single-digit to low-double-digit ms) now returns almost immediately.
- Test: `tests/menu_touch_gesture_test.cpp` gained a real end-to-end block --
  fork+exec the actual `fb-server` binary this build produced via
  `StartLocalServer`, assert it returns well under the old fixed delay and
  the port is genuinely accepting connections, then a real
  `NetworkClient::Connect` against that real server (success, bounded time,
  correct state transition), then a real refused-connection case (server
  stopped, same port, fails fast). Verified the `StartLocalServer` timing
  assertion catches the regression: reverting only that change failed it;
  restored and re-verified. The `Connect()` success/refusal assertions pass
  under both the old and new `networkclient.cpp` -- a localhost connect or
  refusal is fast either way, so this coverage guards the refactor (a broken
  non-blocking-connect rewrite would fail it) rather than demonstrating the
  original unbounded-hang bug, which would need an actually
  unreachable/filtered host and was judged unsafe to depend on in an
  automated test (real-network behavior varies by CI sandbox). The 5-second
  connect bound and the `SendAll` fix are verified by code inspection against
  an already-proven idiom, not by a test that reproduces a multi-minute hang.
- Full native build + `ctest`: 27 runnable tests passed, 2 sanitizer-only
  skips as expected. ASan/UBSan focused pass of `menu-touch-gesture-test`
  clean. WASM Release build compiled clean (`Connect()` is
  `#ifndef __WASM_PORT__`, so unaffected there).

The larger deferred scope is the whole "Async networking rearchitecture"
section below — it has since landed in full, tagged `v2.4.92`.

### B. Share fonts across cached labels

Status: **gameplay label sharing implemented and verified** on 2026-09-07.
Stats-panel sharing is in `1e2ed8e5`; targeting/name sharing is in `d79bf01b`.
Menu font sharing has one confirmed hot path done; the rest is the "B/E"
active item above.

- Evidence: `BubbleGame::StatsPanelCell` in `src/bubblegame_render.cpp` calls
  `LoadFont(path, size)` for every new cell. Targeting and name labels also
  load repeated font sizes in `src/bubblegame.cpp`.
- Prerequisite noted at the time: `TTFText::UpdateText` uses setter-driven
  dirty state; it does not detect changes made through another borrower or
  directly to a font. Either investigate `TTF_GetFontGeneration` plus any
  layout properties it doesn't cover, or keep shared fonts immutable (the
  approach actually taken). Test both borrowers explicitly.
- Changed files for the completed portion: `src/bubblegame.h`,
  `src/bubblegame_render.cpp`, `src/ttftext.h`, and
  `tests/statspanelcell_cache_test.cpp`. `BubbleGame` now owns one immutable
  14 px font for stats/royale cells and one immutable 16 px font for malus
  alerts. Pool entries borrow those fonts while retaining independent textures.
- The new pointer-identity regression failed for both font sizes before the
  change and passed afterward. Existing end-to-end texture-cache coverage also
  stayed green: `statspanelcell-cache-test` passed 1/1.
- Full native build and suite passed after this change: 27 runnable tests
  passed, with the 2 sanitizer-only server tests skipped. The five relevant
  tests passed under ASan/UBSan with `ASAN_OPTIONS=detect_leaks=0` on macOS.
- Follow-up change: all `MAX_NET_PLAYERS` targeting labels now borrow one
  immutable 12 px font, and the remote-player name labels borrow one immutable
  centered 16 px font. Player zero retains its distinct 22 px font. The new
  pointer-identity assertions failed for both groups before the change and the
  focused test then passed 1/1.
- After the follow-up, the full native build and suite passed again: 27
  runnable tests passed and the 2 sanitizer-only server tests skipped. The
  focused test passed under ASan/UBSan with leak detection disabled on macOS,
  and the WASM Release build compiled successfully.
- **Menu portion, first confirmed hot path implemented and verified
  2026-09-07 (`b8409d30`).** Scoped to `mainmenu_netpanel.cpp`'s game-room
  settings grid (`NetPanelLobbyActionsRender`'s "ALL / P1..PN" header + 4
  data rows) -- up to 34 short text cells rendered every frame the grid is
  open, all through one shared `panelText`, so every cell's different text
  invalidated the previous cell's just-cached texture. Confirmed this is a
  real immutable-font-safe target first: the comment already in that
  function documents the grid is deliberately reset to one fixed 16px
  Normal style right before this section and never changes it per cell, the
  same precondition item B's own prerequisite note asks for.
  `MainMenu::NetGridCell(idx)` mirrors `StatsPanelCell` exactly (growable
  pool, one shared immutable font, resolved by call order) rather than
  reusing `BubbleGame`'s pool -- `MainMenu` and `BubbleGame` are separate
  classes with independent lifetimes.
- Changed files: `src/mainmenu.h`, `src/mainmenu_netpanel.cpp`,
  `tests/menu_touch_gesture_test.cpp`.
- New regression: a 3-player room asserts the pool's expected cell count
  (24), that an unrelated cell's cached texture survives a second identical
  render, and that changing one player's color count doesn't disturb the
  "ALL" header or "Max colors:" row label's textures (sibling-cell
  independence, the same property `statspanelcell_cache_test` checks for
  the gameplay pools). Verified it catches the regression: reverting only
  the render-site change (pool/member still declared) failed all 8
  assertions; restored and re-verified passing.
- Full native build + `ctest`: 27 runnable tests passed, 2 sanitizer-only
  skips as expected. ASan/UBSan focused pass of `menu-touch-gesture-test`
  clean (`detect_leaks=0`, `fast_unwind_on_malloc=0`). WASM Release build
  compiled clean.
- Not measured: texture-creation counts before/after on a live idle room
  (the fix's correctness rests on the pointer-identity regression, not a
  counted benchmark).

### C. Preserve frame timing precision in long sessions

Status: **implemented and verified** on 2026-09-07 in `1e2ed8e5`. A
one-day-elapsed regression first failed because 60 float
deadline increments did not total one second. It passed after changing the
frame deadline and interval to double precision and widening the frame/FPS
tick counters to `Uint64`.

- Evidence: `FrozenBubble::frameDeadline` is a `float`, and `RunOneFrame`
  converts `SDL_GetTicks()` to float before comparing deadlines. As absolute
  elapsed time grows, float spacing rounds away fractional frame intervals.
- Changed files: `src/frozenbubble.h`, `src/frozenbubble.cpp`, and
  `tests/controller_input_test.cpp`.
- Focused verification:
  `cmake --build build --target controller-input-test --parallel &&`
  `ctest --test-dir build -R '^controller-input-test$' --output-on-failure`
  passed 1/1 after the production change.
- Batch verification: the full native build succeeded and all 27 runnable
  tests passed (29 registered; the 2 sanitizer-only server tests skipped).
  The ASan/UBSan build succeeded, and the focused controller/timing and player
  label tests passed 2/2 with `ASAN_OPTIONS=detect_leaks=0` on macOS.

### D. Finish moving routine traces to DEBUG

Status: **implemented and verified for the identified hot paths** on
2026-09-07 in `1e2ed8e5`.

- Evidence: `src/bubblegame_shooter.cpp` still logs routine launch, placement,
  and malus events with `SDL_Log`. `src/networkclient_wasm.cpp` logs sent
  commands/game data at INFO. `Logger::LogOutputCallback` still writes every
  emitted message to stderr, despite buffering the file stream.
- Changed files: `src/bubblegame_shooter.cpp` and
  `src/networkclient_wasm.cpp`. Routine launch, placement, chain, malus, and
  successful-send traces now use DEBUG. Occupied-cell fallbacks now use real
  WARN priority. WebSocket lifecycle messages and all warning/error paths stay
  visible at the default INFO threshold.
- Focused native verification: the game target built, `git diff --check`
  passed, and `bubblegame-rules-test`, `bot-play-test`, and
  `logger-priority-test` passed 3/3. The existing priority test confirms INFO
  by default and DEBUG under `FROZEN_BUBBLE_DEBUG=1`.
- The existing `build-wasm` Release configuration compiled successfully after
  the transport change. Its warnings were pre-existing Emscripten/unused-value
  warnings; no new compile error occurred.
- **Measurement completed 2026-09-07.** Throwaway benchmark (not committed;
  same shape as item 4's bot-scoring benchmark, linking the built
  `frozen-bubble-core-test` library) drove the identical bot_play_test.cpp
  workload -- 5 seeds x 4000 frames x 4 players (3 bots, hard skill) -- once
  with `SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO)` (the production default)
  and once at `SDL_LOG_PRIORITY_DEBUG` (`FROZEN_BUBBLE_DEBUG=1`), counting
  messages actually delivered to the SDL log callback (a warm-up game ran
  first, unmeasured, so `FrozenBubble::Instance()`'s lazy construction and its
  own `Logger::Initialize` call happened before the counting callback was
  installed and could not clobber it mid-measurement). Result: **117 messages
  at the INFO threshold vs. 2503 at DEBUG** -- DEBUG mode emits 2386 more
  messages for the same workload, a 21.4x increase. Confirms the item's
  premise concretely: the routine per-shot/per-stick/per-malus traces D moved
  to DEBUG are the overwhelming majority of what a normal game session would
  otherwise log, and default-mode players no longer pay that I/O cost.

### E. Extend per-label caching to menu screens

- Evidence: `src/mainmenu_netpanel.cpp` and `src/mainmenu_teampanel.cpp` render
  multiple labels through a shared `panelText`; `src/menulist.cpp` also reuses
  text objects for labels/measurement. The game-panel work did not cover these
  menu paths. Alternating strings defeat a last-string-only cache.
- **Overlaps item B.** The one repeating-list hot path identified by
  inspection -- `NetPanelLobbyActionsRender`'s game-room settings grid -- was
  implemented under item B's write-up (`b8409d30`) rather than duplicated
  here, since it is the same `panelText`/alternating-strings defeat B already
  describes. `mainmenu_teampanel.cpp` and `menulist.cpp` were inspected but
  not converted — see the "B/E" active item above for what's left and how to
  approach it.

### F. Correct player-name team color updates (small correctness fix)

Status: **implemented and verified** on 2026-09-07 in `1e2ed8e5`. The
regression first failed for both no-team -> team and team ->
no-team transitions, then passed after selecting the current color before
updating the text and resetting no-team labels to white.

- Evidence: `BubbleGame::UpdatePlayerNameWinText` calls `UpdateText` before
  `UpdateColor`, so the new color is applied to the texture on a later call.
  It also sets a color only for a real team, with no default-color reset when
  the player returns to `kNoTeam`. This can retain the previous team tint.
- Changed files: `src/bubblegame_render.cpp` and
  `tests/statspanelcell_cache_test.cpp`.
- Focused verification:
  `cmake --build build --target statspanelcell-cache-test --parallel &&`
  `ctest --test-dir build -R '^statspanelcell-cache-test$' --output-on-failure`
  passed 1/1 after the production change.
- Batch verification is recorded under item C above.

### G. Profile remaining bot allocation and collision costs (conditional)

Status: **measured 2026-09-07; not justified, not implemented.**

- Evidence: `Neighbours`, `SameColourGroup`, and `SweepDetached` in
  `src/bubbleai.cpp` still allocate vectors, sets, and queues. `PredictLanding`
  scans board cells at every simulated substep for all candidate angles.
- **Measurement:** throwaway benchmark (not committed, same
  `frozen-bubble-core-test`-linked shape as items 4 and D), timing
  `BubbleAI::ChooseShot` at Hard skill (the expensive path -- lookahead calls
  `BestFollowUpScore`, which is where `Neighbours`/`SameColourGroup`/
  `SweepDetached` allocation actually concentrates) over 1000 calls each on
  three varied-colour board densities chosen to defeat item 4's per-decision
  landing-cell cache more than its own cache-favorable benchmark did (a
  varied-colour fill collapses fewer candidate angles onto the same landing
  cell than a solid-colour fill): sparse (2 rows filled) 0.885 ms/call, busy
  (8 rows) 0.659 ms/call, nearly full (11 rows) 0.393 ms/call.
- **Conclusion:** all three stay under 1 ms/call, and `ChooseShot` runs once
  per bot shot decision (on the order of once every 1-2 seconds of bot play),
  not once per frame -- sub-millisecond one-off cost against that cadence is
  not a frame-time or responsiveness problem regardless of how many
  allocations happen inside it. Item 4's cache already did the work that
  mattered here. Per this item's own "if justified" gate, no further bot
  allocation change is warranted; not implemented.

---

## Async networking rearchitecture (item A's larger scope) — stages 1-4

Spun out of item A above, which landed two bounded fixes (`00faeaf4`) and
explicitly deferred the larger rearchitecture to its own batch. This was
that batch: a multi-session, four-stage effort, each stage independently
shippable and taggable. All four stages landed and shipped as `v2.4.92`.

### Purpose

Item A's deferred scope, verbatim from the original:

> advance connection/handshake/startup states from the main loop, move blocking
> name resolution off that loop, and queue partial writes on non-blocking
> sockets … demonstrate that input and rendering continue during waits; measure
> worst frame stalls.

Preserve: protocol ordering, hosted-bot servicing, cancellation, deadlines, and
complete lines across partial sends. Reuse the platform's existing async paths
rather than creating a second protocol implementation.

### The problem, measured

Every one of these blocked the render loop on native. Worst cases read from the
source as of `3f96b74a`:

| Site | Worst case | Status |
|---|---|---|
| `SendCommand`'s unconditional 100 ms `select()` | 100 ms **per command** | fixed (1c) |
| Lobby idle (`RequestList` every 500 ms) | **~20 % of wall-clock blocked while idle** | fixed (1c — it was this `select()`) |
| `DetectGeoLocation` (2 × `popen(curl)`) | 16 s | fixed (1d) |
| `R` refresh of public server list (unthreaded) | 16 s + 2 s × N | fixed (1d) |
| LAN discovery + latency probes | ~1 s + 2 s × N | fixed (1d — was never threaded anywhere) |
| `SendNick` / `CreateGame` / `JoinGame` retry loops | 3 s each | fixed (1b) |
| `NetBotConnection::JoinRoom`'s blocking connect | OS timeout × N bots | fixed (1e) |
| `Connect` (resolve + connect + drain `SERVER_READY`) | 8 s + unbounded DNS | fixed (2a–2c) |
| Round 2+ level sync always burning its timeout | 5 s **every round after the first** | fixed (3c) |
| Leader `GAME_CAN_START` poll | **15 s** (its own comment says 5 s — wrong) | fixed (3a) |
| `WaitForBubble*` family | 5 s each | mitigated (3b) — gated so the common case doesn't reach this; the loop itself is unchanged |
| `SyncNetworkLevel` (40 waits) | **~200 s** | mitigated (3b) — same; worst case only if the gate's own timeout is hit |

**A single ENTER on "connect" could freeze the UI for ~30 seconds.** No spinner,
no cancel, no repaint — on macOS the OS painting the window as "not responding".
Measured after stages 1–2, the same connect costs a worst single frame of
**2 ms** (see the stage 2 verification section below).

### The approach

**The async implementation already exists.** The Emscripten build cannot block,
so it already has frame-driven equivalents for nearly all of this:
`pendingCreate`/`pendingJoin`/`pendingLobbyConnect`, `wasmSyncWaitStart`,
`wasmBotWaitStart`, and completion handling in `HandleServerResponse`.

So this was mostly a **deletion** job: remove the native-only blocking branches
behind `#ifdef __WASM_PORT__`, make the async path unconditional, and give native
a per-frame transport pump. Platform divergence shrinks, and the WASM-only bugs
listed at the bottom get fixed once, for both platforms.

Scope: client **and** server (stage 4 closes audit `BUG-007`, the same design
defect on the other side of the wire). Connect UI is deliberately minimal —
"Connecting…" plus cancel, not a progress design.

### Stage 1 — Unify on the async command path, kill background stalls

Status: **1a–1f all landed** — stage 1 complete.

Highest value per unit of risk. Removes the always-on lobby tax and the 16 s
geolocation stall.

- **1a. Per-frame pump hook — landed.** `MainMenu::PumpNetworkFrame()`, called
  from `FrozenBubble::RunOneFrame` between the event loop and the render
  dispatch. This point is deliberate: the old pump lived inside
  `NetPanelRender` gated on `showingNetPanel && IsConnected()`, so nothing
  advanced while a connection was still being established — the very
  situation that needs advancing. `NetworkClient::Existing()` (new) avoids
  constructing a client in single-player. The duplicate pump that used to
  live inside `NetPanelRender`'s `IsConnected()` block was removed (it's now
  called once per frame regardless of which screen is up, instead of twice
  per frame while the net panel happened to be showing).
- **1b. `SendNick`/`CreateGame`/`JoinGame` async on native — landed.** The
  `#ifdef __WASM_PORT__` forks and native's 20-iteration `SDL_Delay(50)` retry
  loops (up to 3 s each) are gone; all three now go through the pending-flag
  path unconditionally, on both platforms. `NICK` gained a pending path it
  never had before (`pendingNick`/`IsPendingNick()`) — WASM's old `SendNick`
  set the nick optimistically and never retried a collision at all (see the
  WASM bug list at the bottom); now a `NICK_IN_USE` retries with a numeric
  suffix up to 20 times on both platforms, exactly like `CREATE`/`JOIN`
  already did on WASM.
  Went further than originally scoped, found by reading `server/game.c`'s
  actual error strings rather than trusting the existing `#ifdef` branches
  were complete:
  - **`INVALID_NICK` and `GAME_FULL` were unhandled on every platform**,
    silently falling through `HandleServerResponse`'s if/else chain with no
    error surfaced at all. Both now fail their pending op cleanly (`INVALID_NICK`
    is not retriable — confirmed against `is_nick_ok()` at `server/game.c:712`).
  - **Ordering bug, found by a real end-to-end test, not by inspection**: all
    three send functions originally set their pending flag *after* calling
    `SendCommand()`. Native's `SendCommand()` at the time did its own inline
    blocking 100 ms `select()`+`recv()` (since deleted by 1c) that could
    synchronously drive a fast localhost reply all the way through
    `HandleServerResponse()` *before the caller returns*. With the flag set
    only afterward, a same-frame `NICK_IN_USE` would find no pending flag set,
    get silently dropped, and leave the flag stuck true forever with no retry
    ever sent. Fixed by setting pending state *before* `SendCommand()` in all
    three functions, with the flag cleared again on the `SendCommand()`
    failure path. A mocked/synthetic test could not have caught this — it
    depends on a genuinely synchronous round trip through the real socket
    code, which is why the regression test below opens a second raw TCP
    connection against a live `fb-server` rather than mocking server state.
  - **`Disconnect()` now clears all three pending flags** on both platforms —
    native previously cleared none of them (they were WASM-only fields before
    this stage), and WASM's own `Disconnect()` was missing the newly-added
    `pendingNick`. Without this, a disconnect mid-NICK/CREATE/JOIN would leave
    a stale flag that could misattribute the next connection's first
    unrelated `OK` (the wire protocol carries no request id).
  - **`MainMenu::PollGeoLocFetch()` now holds `GEOLOC` back while
    `IsPendingNick()`** — same "no request id" hazard: if the background
    geoloc fetch finishes while a NICK retry is still in flight, sending
    `GEOLOC` immediately risks its `OK` being consumed as the NICK
    confirmation instead (since `HandleServerResponse` checks `pendingNick`
    first), leaving the real NICK `OK` to fall through unowned. The result
    stays queued in `geoLocToSend` and sends a frame or two later once NICK
    settles — never dropped.
  - New end-to-end regression test in `menu-touch-gesture-test`: opens a raw
    POSIX socket to a real `fb-server`, claims a nickname on it, then drives
    the real `NetworkClient` singleton's `SendNick()` for the same name and
    polls until `IsPendingNick()` clears, asserting the retry lands on the
    `2`-suffixed name. Deliberately does not assert on timing immediately
    after `SendNick()` returns — the whole retry can resolve synchronously
    inside that call on a fast localhost round trip, so only the eventual
    outcome is checked.
- **1c. `SendCommand` fire-and-forget — landed.** Deleted the inline
  `select()`+`recv()`+`strtok` block entirely (native `SendCommand()` now just
  sends and returns `true`/`false`). Safe now that 1b moved every
  pending-flag write to *before* its `SendCommand()` call — the inline read
  is what used to make that ordering load-bearing, and 1b's own comments in
  `SendNick`/`CreateGame` were updated to say so rather than still describing
  the now-deleted read. Replies now arrive only through
  `ProcessIncomingData()`'s buffered reader, driven every frame by
  `MainMenu::PumpNetworkFrame()` (1a) — typically the same frame on a fast
  localhost link, one round trip otherwise. This also fixes the inbound half
  of audit `BUG-017`: the deleted block ran `strtok` directly on each raw
  `recv()` chunk with no memory across calls, so a line split across two
  reads (or a partial line left over from the last complete one) was
  mishandled — `ProcessIncomingData()`'s `recvBuffer`/`recvBufferLen` state
  doesn't have that problem. `lastErrorResponse` (set throughout
  `HandleServerResponse`, never read by any caller — confirmed by grep) was
  left as-is; it was already dead code before this change, not made dead by
  it.
- **1d. Thread the HTTP/UDP stalls — landed, expanded beyond the original
  scope.** All four blocking discovery/geoloc paths are now backed by a
  background thread, not just the two originally named:
  - `NetworkClient::DetectGeoLocation()` (up to ~16 s, two sequential
    curl/HTTP calls) — `MainMenu::StartGeoLocFetch()`/`PollGeoLocFetch()`
    (`src/mainmenu_server.cpp`). **Decoupled from lobby entry entirely**,
    not just threaded: GEOLOC has no server-side ordering requirement
    relative to NICK (`server/game.c` stores it per-fd unconditionally), so
    the player now enters the lobby immediately after NICK succeeds instead
    of sitting on a frozen "connecting" screen for up to 16 s while geoloc
    resolves in the background. `SendGeoLoc` fires whenever the fetch
    completes, queued in `geoLocToSend` if no connection exists yet.
  - The `SDLK_R` public-server-list refresh (`src/mainmenu_input.cpp`) — was
    the one inconsistent path; the *initial* fetch (`ShowPanel` case 5) was
    already threaded. Both now share `MainMenu::StartPublicServerFetch()`
    (factored out of the duplicated inline lambda).
  - LAN discovery + per-server latency probing (`DiscoverLANServers` + N ×
    `MeasureLatency`, up to ~1s + 2s×N) — **was never threaded anywhere**,
    including the initial "LAN game" panel open (`ShowPanel` case 3), the
    `SDLK_R` refresh, and the "Host a server" rescan
    (`src/mainmenu_input.cpp`'s `DO_CONNECT` block). All three now call the
    new `MainMenu::StartLanFetch()`. `ServerListPanelRender` gained a
    `lanFetchResult` drain mirroring the pre-existing public-server one, plus
    a "Scanning local network..." row.
  - Known limitation carried over from the pre-existing `serverFetchThread`
    pattern, not newly introduced: `MainMenu`'s destructor joins these
    threads, so quitting while a public-server or geoloc fetch is still in
    flight can block shutdown for the same duration the old code blocked the
    render loop. Not fixed here; the real fix is stage 2b's
    detached-thread-plus-`shared_ptr`-result pattern, generalized to these
    fetches too.
  - The "Host a server" flow's `SDL_Delay(500)` was deliberately **kept** —
    it covers the UDP discovery responder's startup, which
    `StartLocalServer()`'s TCP-only readiness poll doesn't observe — but no
    longer stacks with a second *blocking* discovery scan on top of it.
  - Regression test: `menu-touch-gesture-test` gained a block exercising
    `StartLanFetch()`'s async kickoff and completion against a real UDP
    broadcast (network-free, deterministic ~1s window). The public-server and
    geoloc fetches were **not** given automated completion tests — both hit
    real internet endpoints, which would make ctest's runtime and pass/fail
    depend on network reachability (the same reasoning that kept `Connect()`'s
    unreachable-host case out of automated coverage in the item A slice).
  - **Verified** (2026-09-08): full native build clean; `ctest`: 27/27
    runnable tests passed (2 sanitizer-only skips as expected), including the
    new `StartLanFetch` async-kickoff regression block. WASM was not rebuilt
    this session (local Emscripten toolchain issue — `emmake.py` uses
    `match` syntax against what's invoking as Python 2); the `#ifdef
    __WASM_PORT__` branches touched here were reviewed by inspection against
    the pre-existing `serverFetchThread` pattern. These paths are all
    `#ifndef __WASM_PORT__` (WASM has no threads), so there was never a WASM
    code path here to exercise at all — the later live playtest (see below)
    doesn't add coverage for 1d because there's nothing on that platform to
    cover.
- **1e. Bound `NetBotConnection::JoinRoom`'s connect — landed.** Mirrors the
  same non-blocking-connect + bounded-`select()` fix `NetworkClient::Connect()`
  already had from item A (`00faeaf4`): the socket is set non-blocking
  *before* `connect()` rather than after, a pending (`EINPROGRESS`/
  `EWOULDBLOCK`) connect is bounded by a 3s `select()`, and the real outcome
  is read via `SO_ERROR`. Previously a plain blocking `connect()` — called
  synchronously from `MainMenu::SyncLobbyBots()`'s `while` loop, itself
  reached from an input handler (adjusting the bot-count setting), so adding
  several bots at once could freeze the render loop for that many multiples
  of the OS's own TCP connect timeout (commonly tens of seconds each), with
  no way to cancel. A bot always connects to the same host:port the player's
  own client is already talking to, so 3s is generous for the common case;
  the bound exists for when the network degrades or the port stops answering
  between the player's own connect and the bot's. Verified against a real
  server: `net-bots-test` and `server-bot-cap-test` both exercise
  `JoinRoom()` end-to-end and pass.
- **1f. Bare delays and pending-status feedback — done.** The two `SDL_Delay`
  calls in the DO_CONNECT nick/geoloc chain (100ms, 500ms-adjacent context)
  were removed as a side effect of 1d's decoupling. `IsPendingCreate()`/
  `IsPendingJoin()`/`IsPendingNick()` themselves are not surfaced as their own
  UI row (the lobby already shows an optimistic status line the moment
  `CreateGame()`/`JoinGame()`/`SendNick()` is called, e.g. "Game created - now
  you need to wait for players to join", and the room screen appears on its
  own once `currentGame` is populated by the async confirmation) — what *was*
  genuinely missing, and is now fixed, is failure feedback: every terminal
  failure path in `HandleServerResponse` (`NICK_IN_USE`/`INVALID_NICK`/
  `GAME_FULL`/`NO_SUCH_GAME` exhausting retries or hitting a non-retriable
  error) used to clear its pending flag with only an `SDL_LogWarn` — invisible
  to the player, who just saw nothing happen. Each of those paths now also
  calls `AddStatusMessage()`, the same "Server: ..." status-line mechanism
  already used elsewhere in the lobby, so a rejected CREATE/JOIN/NICK now
  visibly says why.

**Verified (1b, 2026-09-08):** full native build clean. Plain `ctest`: 29
tests, 27 run / 100% pass (2 sanitizer-only skips as expected), including
the new NICK_IN_USE end-to-end test. Sanitizer build rebuilt and re-run
without `detect_leaks=1` (macOS/Darwin does not support ASan leak detection
at all — confirmed directly: running a test binary under `detect_leaks=1`
aborts immediately with "AddressSanitizer: detect_leaks is not supported on
this platform", a pre-existing local-environment limitation, not a code
defect; the documented `detect_leaks=1` command in CLAUDE.md targets Linux
CI, where it is supported). Under `UBSAN_OPTIONS=print_stacktrace=1 ctest`,
all 29 tests run and pass, including the two sanitizer-only ones —
confirming the new recursive-call pattern (a reply processed synchronously
inside `SendCommand()`, inside `SendNick()`/`CreateGame()`/`JoinGame()`,
inside `HandleServerResponse()`) is memory- and UB-safe. WASM was again not
rebuilt (same toolchain issue); the `Disconnect()` and
`HandleServerResponse()` changes were reviewed by inspection at the time.
**Update:** the live two-browser WASM playtest (below) has since exercised
this path directly — NICK went through on both real clients — so this
caveat is closed, not just inspected.

**Verified (1c/1e/1f, 2026-09-08):** full native build clean. Plain `ctest`:
29 tests, 27 run / 100% pass — notably the NICK_IN_USE end-to-end test (now
genuinely exercising the multi-frame async path, since `SendCommand()` can
no longer resolve it synchronously) and `net-bots-test`/`server-bot-cap-test`
still pass. Sanitizer build rebuilt and re-run (no `detect_leaks=1` per the
macOS limitation above): all 29 run and pass, including the two
sanitizer-only ones, after each of the three changes in this batch —
rebuilt and re-tested separately after each, not just once at the end. WASM
was not rebuilt (same toolchain issue); 1c is native-only (WASM's own
`SendCommand` was already async), 1e is inside the same `#ifndef
__WASM_PORT__` block the pre-existing blocking connect lived in (WASM's
`JoinRoom` is a separate implementation, not touched), and 1f's
`AddStatusMessage` calls are shared code, reviewed by inspection.

### Stage 2 — Async connect + minimal connecting UI

Status: **2a-2e all landed** — stage 2 complete.

- **2a. Kickoff-only `Connect()` — landed.** `ConnectionState` gained
  `RESOLVING` and `AWAITING_READY`, and `CONNECTING` now genuinely survives
  frames. `Connect()` starts the attempt and returns; `PumpConnect()`, called
  from `Update()` (and so from `MainMenu::PumpNetworkFrame()` every frame),
  advances RESOLVING → CONNECTING → AWAITING_READY → CONNECTED. Each phase has
  its own deadline judged from when that phase began, so a slow lookup no
  longer eats the connect's budget.
  **`Connect()`'s return value changed meaning**: it is now "the attempt
  started", not "we are connected" — which is the contract WASM's `Connect()`
  has always had, so the two platforms finally agree. Only one production
  caller exists (`Instance(host, port)`), which is why the blast radius was
  small; the rest were tests, updated to pump.
- **2b. Threaded name lookup — landed.** `getaddrinfo` runs on a detached
  worker writing into a `shared_ptr` slot the worker co-owns. Cancelling — or
  destroying the client — mid-lookup leaves the worker writing into memory
  that is still valid and simply nobody's business, rather than into a freed
  `NetworkClient`. Passing `this` would have been a use-after-free waiting for
  a slow DNS server to trigger it. `netconnect-test` cancels a connect
  mid-flight specifically so ASan would catch that if it were ever
  reintroduced.
- **2c. Frame-driven `SERVER_READY` handshake — landed.** Replaces the 3 s
  drain loop. Accumulates the banner across reads *and across frames*, for the
  same reason the synchronous version had to (see the split-banner bug below).
  Anything arriving after the banner is handed to the buffered reader instead
  of dropped — the server pipelines its first push messages right behind
  SERVER_READY, and this reads by byte count, not by line.
- **2d. Fix `IsConnected()` — landed, deliberately ahead of 2a/2b/2c.** It was
  `state != DISCONNECTED`, so it returned true while `CONNECTING` — harmless
  only because `Connect()` resolved, connected and handshook synchronously,
  so `CONNECTING` was set and overwritten before `Connect()` ever returned and
  no caller could observe it. It becomes an active landmine the moment 2a
  makes that state persist across frames, so it is fixed *first*, on its own
  commit, while it is still a provable no-op — if something built on top of
  it later goes wrong, this has its own clean bisection point rather than
  being buried inside the state-machine rewrite. Now
  `state == CONNECTED || state == IN_LOBBY || state == IN_GAME`.
  All ~18 callers were audited (grep, every hit read in context): every one
  of them gates sending a command, reading player/session state, or deciding
  whether to request a fresh list — i.e. all of them mean "ready to carry
  commands", none mean "in any state other than fully idle". The
  `pendingLobbyConnect` completion path in `NetPanelRender()` is unaffected
  because it tests `GetState() == CONNECTED` explicitly rather than going
  through `IsConnected()`.
- **2e. Connecting UI — landed.** All four of CLAUDE.md's input-parity
  requirements: a "Connecting..." indicator (now driven by the client's actual
  `IsConnecting()` state, and shown on the LAN list too — it was public-list
  only because on native the connect used to finish inside the keypress and
  there was nothing to show), ESC cancel, a **tap target** for cancel, and a
  footer hint that switches to "Connecting...    ESC cancel" while in flight.
  `MainMenu::CancelPendingConnect()` is the one place that tears an attempt
  down, wired into both ESC paths and the tap. Both of those paths were
  genuinely broken for an async connect: mode 10's ESC neither disconnected
  nor cleared `pendingLobbyConnect`, and the broad `else` covering LAN only
  disconnected `if (IsConnected())` — which is false while connecting, so an
  in-flight attempt would have outlived the screen that started it.
  Tap-cancel has its own regression test (hit, miss, and inert-when-hidden).

**Verified (stage 2, 2026-09-08):** `tests/fake_server.h` and
`netconnect-test` were built **first**, before the state-machine rewrite,
specifically so the rewrite had deterministic coverage of the cases it
exists to handle. That ordering paid for itself immediately: the fixture
failed on its first run against then-shipping code and exposed the
split-banner bug (see 2c), which no test against a real localhost server
could have found.

Frame stalls, measured by `netconnect-test` (one `Update()` per simulated
frame, timing every individual call):

| case | worst single frame | total wait |
|---|---|---|
| baseline (instant banner) | 0 ms | 57 ms |
| banner delayed 600 ms | 1 ms | 772 ms |
| banner split mid-token | 1 ms | 187 ms |
| peer accepts, never greets | 2 ms | 3024 ms |
| refused port | 0 ms | 20 ms |
| blackholed port | 2 ms | 5014 ms |

The right-hand column is what the old code blocked the render loop for, in a
single call. The middle column is what it costs now. The test asserts a 50 ms
budget rather than the 16 ms a 60fps frame allows, because one call
occasionally losing the CPU to the scheduler is not a blocking bug — the
measured values sit two orders of magnitude below either bound.

`ctest`: 30 tests, 28 run / 100% pass. ASan/UBSan build reconfigured, rebuilt
and rerun (no `detect_leaks=1`, unsupported on macOS): all 30 run and pass,
including the fixture's threads and `netconnect-test`'s cancel-mid-resolve
case — the one that would catch the detached resolver touching a freed client.

**A WASM regression was introduced and fixed inside this stage, found by
reading rather than by tests.** Narrowing `IsConnected()` in 2d broke WASM's
lobby entry: WASM's `Connect()` has always returned true with state
`CONNECTING`, so the old `state != DISCONNECTED` sent it into the arm that
sets `pendingLobbyConnect`. With the narrowed predicate it fell into the
"Failed to connect" arm instead and could never reach a lobby. The 2d commit
called itself "a provable no-op", which was true for native and wrong for
WASM. The fix is the three-way branch (`IsConnected()` / `IsConnecting()` /
neither) that stage 2 needed anyway. **Update:** the live two-browser WASM
playtest (below) has since exercised real lobby entry on two real WASM
clients end to end, so this caveat is closed for the happy path — still no
automated regression coverage, but no longer inspection-only.

### Stage 3 — Async game start and level sync (highest risk)

Status: **3a and 3c landed; 3b partially landed** (stall mitigated for both
call sites and confirmed live via a genuine two-browser WASM multi-round
game; a full state-machine rewrite of `WaitForBubble`/`SyncNetworkLevel`
themselves remains open — see "Active work" above)

- **3a. Leader `GAME_CAN_START` poll — landed.** Used to run as a blocking loop
  *inside a push-message handler* (`HandlePushMessage`, reached from the
  per-frame message pump): 50 attempts of up to 200 ms `select()` plus a
  100 ms `SDL_Delay`, so a single slow joiner could freeze the leader's render
  loop for up to 15 s (the loop's own comment claimed 5 s). Now a kickoff
  (`pendingGameStart = true`) plus a per-frame pump (`PumpGameStart()`, called
  from `Update()` after the socket read loop): it re-sends
  `LEADER_CHECK_GAME_START` on the same ~100 ms cadence the old loop used, and
  `HandleServerResponse()` matches `LEADER_CHECK_GAME_START`'s reply *before*
  the generic bare-`OK` handling (the reply's own text contains "OK", which
  would otherwise be misattributed to a pending nick/create/join). On answer
  or on `kGameStartTimeoutMs` (5 s) — whichever comes first —
  `FinishGameStart()` sends `OK_GAME_START` and enters `IN_GAME`; hitting the
  deadline starts anyway rather than stranding every other player in the room
  over one silent joiner.
  This also retires `leaderWaitTick()` — its only purpose was pumping hosted
  bots from inside the blocking loop by hand, a job stage 1a's per-frame hook
  already does unconditionally. `SetLeaderWaitTick()` and the member are gone;
  `MainMenu::AddBots`/`DropLobbyBots` no longer touch it.
  Also removes a re-entrancy hazard the old loop carried: `Update()` inside
  `WaitForBubble` re-entered `HandlePushMessage` and could recurse into this
  poll. That hazard is specific to `WaitForBubble` still being synchronous —
  it returns once 3b's rewrite lands.
  New coverage in `netconnect_test.cpp` drives the real path (`NICK` → `CREATE`
  → `START` → a `GAME_CAN_START` push in the server's own wire format) against
  `fake_server.h`, now extended with a small canned-reply mechanism (`Rule`:
  match a line substring, answer with the next entry in a scripted list). Two
  cases: a joiner that answers "not ready" a few times then "OK" (worst frame
  0 ms, ~370 ms total — the ordinary path a real localhost server answers too
  fast to exercise), and a joiner that never answers at all (worst frame 2 ms,
  ~5 s total, bounded by the deadline rather than the old loop's 15 s, ~267
  frames pumped throughout the wait rather than the loop sleeping through it).
- **3b. Partially landed — the stall is closed for the common case; the
  internals are still synchronous for the fallback case.**
  `WaitForBubble`/`WaitForNextBubble`/`WaitForTobeBubble` and
  `SyncNetworkLevel` were *not* turned into a frame-driven state machine —
  that would mean rewriting ~150 lines of bubble-position math (mini-player
  offsets, per-player grid replication, launcher/next-bubble assignment) into
  something resumable, for logic with no automated coverage of its own and no
  way to verify a rewrite's correctness short of a genuine two-client game.
  Instead this reused the trick already shipped for WASM in 3c: a joiner
  doesn't enter `SetupNewGame`/`ReloadGame` at all until every level-sync
  message for that round is already queued, polled from the per-frame path
  (`ShouldKeepWaitingForLevelSync()`, the same pure rule 3c introduced). Once
  that gate releases, `SyncNetworkLevel`'s `WaitForBubble` calls find every
  message already sitting in the queue and return on their first pass rather
  than genuinely waiting — so the render loop keeps turning during the part
  that used to freeze it, without touching the math that produces the level
  at all.
  This was WASM-only because WASM's `WaitForBubble` spins without yielding
  and WASM has no threads, so a WebSocket callback can only ever fire between
  animation frames — a spin that never returns to the browser's event loop
  can never observe the message it's waiting for, making the gate mandatory
  there for correctness, not just responsiveness. Native's `Update()` inside
  that same spin does real, working `recv()` calls, so the gate was never a
  correctness requirement on native — but the stall was real anyway:
  `SyncNetworkLevel` runs synchronously inside a render function with no
  return to the frame loop until it finishes, so one delayed bubble message
  froze input and rendering for up to 5 s (`WaitForBubble`'s own timeout),
  compounding across up to 40 such waits in a genuinely bad run. The gate is
  now unconditional (`mainmenu_netpanel.cpp`'s initial game-start check, and
  `bubblegame_render.cpp`'s round-2+ `waitingForOpponentNewGame` check) —
  extending already-shipped, already-tested logic to a second platform,
  rather than adding a second implementation of it.
  **Found in the process: the round-2+ gate in `bubblegame_render.cpp` had
  never received 3c's fix at all.** It's a separate call site from the one
  3c touched (that one gates the lobby's one-time transition into the first
  round; this one gates every round after) and still checked only
  `MessageQueueSize()` — so **every round after the first burned this gate's
  full 5 s timeout too, on both platforms, independent of and in addition to
  the lobby-entry gate's own bug.** Fixed the same way, with the same pure
  rule. Also fixed a latent leak this uncovered: neither this gate's
  timestamp (`wasmRoundSyncWaitStart`, now `roundSyncWaitStart` and no longer
  `#ifdef`-guarded) nor the lobby-entry one's had ever been reset on starting
  a fresh match, so a stale value surviving from a quit-mid-wait match could
  make the very next match's first use of the gate measure "waited" against
  the wrong clock and read as already timed out, silently skipping the wait
  it exists to do.
  What remains open — see "Active work" above for the current framing: the
  residual case where the gate's own 5 s timeout is hit with messages still
  missing still falls through to `WaitForBubble`'s old blocking loop,
  per-message, exactly as before and exactly as WASM already accepted as its
  fallback in 3c. No automated regression test was written for the two
  closed call sites either, for the same reason 3b's rewrite itself wasn't
  attempted: nothing here can spin up `BubbleGame`/`MainMenu` headlessly and
  drive a real multi-round match. `bot-play-test`/`netbot-test`/
  `net-bots-test` (a real `fb-server` plus bots) all still pass, but none of
  them exercises this path — a bot is a level-sync *leader*'s local
  bookkeeping problem, not a joiner waiting on one. The genuine two-client
  multi-round game this needed has since been run — see "Verified: live
  two-browser WASM playtest" below. It confirmed the mitigated common path
  (multiple real rounds, no stalls, clean sanitizer log); it did not
  exercise the residual gate-timeout fallback case.
- **3c. Round 2+ level-sync stall — landed.** The joiner's wait gate counted
  only `MessageQueueSize()`, but `ProcessNetworkMessages()` moves `b|`/`N`/`T`
  out of that queue and into `syncQueue` as it drains. In round 1 nothing was
  draining yet, so the count reached 40 and the wait ended properly; from
  round 2 on the game loop was already draining, the count could never reach
  40, and **every round after the first sat out the full 5 s timeout before
  starting**. Now counts both queues.
  The rule was extracted into `ShouldKeepWaitingForLevelSync()` in
  `networkclient.h` — a pure function compiled on every platform — and given a
  native test. That is the actual fix for the class of bug: it was wrong for
  two releases precisely because it lived inline inside an
  `#ifdef __WASM_PORT__` block where no test could reach it. WASM being this
  effort's repeated blind spot is why the rule now lives somewhere testable
  rather than just being corrected in place.
  Also fixes the fact that WASM's `Disconnect()` never cleared `syncQueue`
  (native's always has), so a round's leftover sync messages survived into
  the next connection.

### Stage 4 — Server output queue (audit BUG-007)

Status: **landed**

`server/net.c`'s `send_line()` was a bare blocking `send()` with no queue, and
`connections_manager()`'s `select()` built a read-set only — there was no
`O_NONBLOCK` anywhere in `server/`. One peer that stopped reading (a frozen
client, a bad connection, or a hostile one) could stall the single-threaded
event loop's `send()` call indefinitely, freezing service to every other
connected player for as long as that one peer's TCP receive window stayed
full.

**The fix**: `net_queue_send(fd, data, len)` — a non-blocking send-or-queue
that is now the single choke point every write in `server/` goes through,
directly or via `ws_send()` (which frames a WebSocket message, then queues).
Each fd gets a `GByteArray` output buffer (`outqueue[256]`) plus a timestamp
of when it first went non-empty (`outqueue_since[256]`), capped at
`OUTQUEUE_MAX_BYTES` (256 KiB) and `OUTQUEUE_MAX_AGE_SECONDS` (30s). A `send()`
that can't take the whole payload immediately queues the remainder instead of
blocking; a later call appends to the existing queue rather than re-sending.

The queue is drained from one safe, already-existing call site:
`handle_incoming_data_generic()`'s entry, once per connection per event-loop
pass. Termination on a cap being exceeded happens only from there too — never
from inside `net_queue_send`/`outqueue_flush` themselves, reusing the same
`conn_terminated(fd, ...); return;` idiom used throughout the rest of that
function rather than adding a second, less-audited teardown path.
`connections_manager()`'s `select()` also gained a write-set (any fd with a
non-empty queue) purely to shorten its wakeup latency — the flush attempt
itself is always unconditional and non-blocking regardless of that bit, so a
platform quirk in write-readiness detection can't make it miss a flush.

`ws_send()`'s contract changed to match: it used to block until the whole
frame reached the socket or hard-failed; now it returns success as soon as the
frame is either sent or queued, and only hard-errors immediately. `game.c`'s
`process_msg_prio_()` — the real-time gameplay relay every fire/stick/malus
message goes through — was updated so its plain-TCP branch also routes through
`net_queue_send()`, so WebSocket and native TCP clients get the identical
backpressure/cap policy for the same message types rather than leaving one
transport more lenient than the other.

**Verification**: `tests/server_stall_test.py` (`server-stall-test` in
`ctest`) is a dedicated regression test — a real `fb-server` subprocess, a
flooder and a victim that start a real game together, and an unrelated
"control" connection. The victim stops calling `recv()` entirely (without
closing its socket) right after the game starts; the flooder then sends real
GAMEMSG fire traffic (the same relay path every shot takes) until the queue
comfortably exceeds both victim's kernel receive buffer and the app-level cap.
Throughout, control's `LIST` requests must keep getting answered within 2s —
that promptness is the actual BUG-007 claim. The test then confirms the
server's log shows the victim actually got dropped (the byte/age cap doing its
job), and that the server process is still alive with no ASan/UBSan
diagnostics. Passes cleanly (~6-7s) under both a plain build and
`build-asan`; the full existing suite is 31/31 either way (2 sanitizer-only
skips on a plain build, all 31 running clean under ASan/UBSan).

This test needed two adjustments to test the right thing rather than an
artifact of the test harness itself, both discovered empirically while writing
it:
- The flooder must send in small, paced batches (a few hundred messages,
  <1000 bytes, with a short pause), not one giant blob. A single oversized
  `sendall()` can fill the server's own fixed-size *inbound* framing buffer
  (`INCOMING_DATA_BUFSIZE`, unrelated to BUG-007's output queue) before a
  trailing `\n` lands inside it, which trips a pre-existing "too much data
  without LF" guard and kills the flooder itself. Confirmed this is
  pre-existing, unrelated behavior by reproducing it against the pre-stage-4
  server in a clean worktree — not something this stage introduced or fixes.
- The server's default 5-second in-game "gracetime" (kicks a connection that
  has *sent* nothing in 5s — a liveness check, unrelated to BUG-007's
  *output*-side queue) fires long before the queue's own 30-second age cap
  ever gets a chance to. The test passes `-g` with a generous value so it
  actually exercises the queue's own cap rather than always hitting the
  unrelated gracetime timer first.
- **Also discovered, and deliberately left alone**: while a peer floods
  sustained, back-to-back GAMEMSG traffic, a *different* prio-mode
  connection's own inbound data (confirmed with a lone 3-byte ping, sent
  while flooding was active) can go unnoticed by `select()` for the whole
  duration of the flood, even though the same connection is read cleanly the
  moment the flood pauses. Reproduced identically against the pre-stage-4
  server in a clean worktree, and confirmed it isn't `write_set` (still
  reproduces with `write_set` disabled entirely) or memory corruption (clean
  under ASan). This looks like a real fairness/starvation gap in the
  `prio_processed` → `continue` → fresh-`select()` loop under sustained
  single-peer load, but it's pre-existing, separate from BUG-007's "blocking
  send()" defect, and out of this stage's scope — this is the `task_3c17853a`
  item in "Active work" above.

### Verified: live two-browser WASM playtest (2026-09-08)

The first genuine two-client verification of any of this: two real browser
tabs, each a real WASM build served via `tools/serve-wasm.py` on port 8090,
both connected over real WebSockets to a real `fb-server` built with
`-fsanitize=address,undefined`. Not a harness — actual gameplay, driven by
synthetic DOM input events into the two tabs, through NICK → CREATE/JOIN →
lobby → connecting UI → game start → multiple full rounds, with real
fire/malus GAMEMSG relay between the two clients throughout. Server log
stayed clean of ASan/UBSan diagnostics for the whole session.

This closed out the "needs a real WASM build to confirm" caveats logged
under stages 1, 2, and 3 for every code path actually exercised by two
clients playing a game together: NICK/CREATE/JOIN's async retry path (1b),
`SendCommand` fire-and-forget (1c), the async connect state machine and
connecting UI including `IsConnected()`'s narrowed meaning (2a-2e), the
leader's async `GAME_CAN_START` poll (3a), and — the specific thing this was
run to check — round 2+ level sync no longer stalling (3c) and staying
stalled-free on native/WASM alike under the 3b mitigation.

Still not covered by this playtest, so still open per "Active work" above:
stage 1d's threaded discovery/geoloc paths are native-only (no WASM code
path to verify there), stage 2b's detached DNS resolver is likewise
native-only, and stage 3b's residual (gate-timeout-exceeded) fallback case
was not hit during this session's rounds — the mitigated common path was
validated, not the fallback loop itself. Two browser tabs on one machine
also isn't the same as two genuinely separate devices/networks; a real
device pairing is still worth doing (see "Active work" above), though the
release already shipped without waiting for it.

### Verification infrastructure

**Mock hostile-peer fixture** (`tests/fake_server.h`) — an in-test TCP
listener on an ephemeral port with knobs for: delayed banner, split/fragmented
banner, accepts-but-never-responds, accepts-but-never-reads, never-accepts. This
is what makes item A's verification matrix deterministic rather than
timing-dependent, and it doubles as the regression test for `BUG-017` and part
of `IMP-019`.

- **Frame-stall assertions** (item A's core demand): each op's kickoff returns in
  < 16 ms, and every subsequent pump call returns in < 16 ms while the op is in
  flight, against a never-responding fixture peer.
- **Stage 1**: assert near-zero wall time for a burst of N commands; assert a
  NICK_IN_USE collision still resolves to `nick2` via the async path (the main
  behavioral regression risk of 1b/1c).
- **Stage 2**: connect to a blackholed address; assert the UI still renders and
  ESC still cancels. The genuinely-unreachable-host timeout is environment
  dependent — assert an **upper bound only**, never a specific duration.
- **Stage 3**: `fake_server.h`'s canned-reply mechanism for the leader poll
  (automated); the level-sync mitigation itself was verified via the live
  two-browser WASM playtest instead, since driving `BubbleGame`/`MainMenu`
  headlessly through a real multi-round match isn't feasible with existing
  infra.
- **Stage 4**: `tests/server_stall_test.py` — a harness client that connects
  and stops reading, asserting other clients keep being served.

Existing infra reused: `frozen-bubble-core-test` + `FROZEN_BUBBLE_TEST_ACCESS`,
the `MainMenuTestAccess`/`NetworkClientTestAccess` friend structs in
`tests/menu_touch_gesture_test.cpp`, and the Python harness that spawns a real
`fb-server`. Sanitizer-only tests use `SKIP_RETURN_CODE 77`.

Manual: `docs/MANUAL_TEST_CHECKLIST.md`'s "Two-browser WASM network playtest"
section has the repeatable recipe for the live playtest described above,
including the two gaps it didn't cover (ESC/tap cancel-mid-connect, and
testing across genuinely separate devices/networks).

### Risks

- **`IsConnected()` semantics change** (2d) was the single most likely source of
  subtle breakage; every caller was audited (see 2d above).
- **Hosted bots must keep being serviced during every wait** — a bot silent > 5 s
  is dropped by the server (`src/netbot.cpp:193-197`). Stage 1a's hook had to
  land before stage 3 retired `leaderWaitTick` — it did, in the right order.
- **Did not regress the v2.4.88 WebSocket partial-line fix** when touching the
  buffered reader — confirmed by inspection and by the live playtest's clean
  multi-round session.

### Pre-existing WASM bugs found while planning this

Not regressions from this work — they were latent before it started, and got
fixed as a side effect of the stages that touched the same code.

1. ~~**Round 2+ always burns the full 5 s sync timeout.**~~ **Fixed in stage
   3c** — the gate counts both queues now, and the rule moved to a pure,
   natively-tested function.
2. ~~**WASM `Disconnect` never clears `syncQueue`**, leaking stale sync
   messages into the next connection.~~ **Fixed in stage 3c.**
3. **CREATE confirmation is heuristic** — any non-`PART` `OK` confirms it, so an
   unrelated `OK` can falsely confirm a pending CREATE. Still open — see
   "Active work" above.
4. ~~**WASM `SendNick` sets the nick optimistically** and never retries, risking
   a `myPlayerId` mismatch when the server truncates or rejects it.~~ **Fixed
   in stage 1b** — `SendNick` is now unified across platforms with a real
   `NICK_IN_USE` retry.
