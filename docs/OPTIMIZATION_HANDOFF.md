# Code optimization progress and handoff

Last updated: 2026-09-07 (follow-up review and next improvement backlog)

## Purpose and user preferences

The user requested a code efficiency review with less unnecessary complexity
and verbose commentary, then approved an initial text-cache/logging patch and
asked to commit, merge, and tag it. They requested this persistent progress
record so another AI session can take over when usage runs out. In session 2
they asked to commit the handoff doc, then implement recommended-work item 1
(render-call-site caching), followed by a version bump/push/tag to `v2.4.77`.
In this third session they asked "anything left to optimize?" and then, on
being shown the remaining 4-item list, said "work on them all in order" — all
four are done (see below), and were pushed to `origin/main` after an explicit
confirmation. With no optimization backlog left, the user then reported a
real gameplay bug encountered during their own use (not part of this effort's
scope) — see "Session 3, continued" below — which is now fixed for both room
sizes: the <=5-cap case shipped as `v2.4.78`, and the >5-cap follow-up (the
user explicitly asked "fix the >5-cap room case too"). Those historical
checkpoints are superseded by the current checkpoint below.

On 2026-09-07 the user asked for further improvements after the original list
was completed, asked to add the findings to this document, and then authorized
implementation. Items B (stats-panel portion), C, D, and F have now been
implemented and verified as recorded below.

Update this document after meaningful implementation or verification milestones
and before handing off. Record actual results, outstanding work, and blockers.
Do not infer benchmark gains from passing tests alone — measure when claiming
a speedup.

## Current checkpoint

- Repository: `/Users/dchau/gr/frozen-bubble-sdl3`
- Branch: `main`; the latest source change is `00faeaf4`
  (`perf: bound network connect/startup waits instead of blocking
  indefinitely`), following `b8409d30` (settings-grid label caching) and the
  `d79bf01b`/`1e2ed8e5` batch below. This handoff update is committed
  immediately after the source checkpoint.
- Local `main` was 4 commits ahead of `origin/main` at the start of this
  session (`1e2ed8e5`..`21397a04`, the prior session's B/C/D/F work); pushed
  at the start of this session, confirmed by `git push` reporting a
  fast-forward. Re-verify with `git fetch && git log --oneline main..origin/main`
  before trusting this further into a new session.
- CMake/Android version: `2.4.91`; Android versionCode: `74`. Unchanged this
  session -- no release tag was cut for this batch of doc/perf/test-only work.
- Latest local version tag: `v2.4.91`. HEAD includes subsequent CI/tooling commits.
- Source/test changes in `1e2ed8e5`: `src/bubblegame.h`,
  `src/bubblegame_render.cpp`, `src/bubblegame_shooter.cpp`,
  `src/frozenbubble.h`, `src/frozenbubble.cpp`,
  `src/networkclient_wasm.cpp`, `src/ttftext.h`,
  `tests/controller_input_test.cpp`, and
  `tests/statspanelcell_cache_test.cpp`.
- Current native, sanitizer, and WASM verification is recorded under the new
  backlog items. No release tag was created for this batch.

## Completed work

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

## Verification completed (session 3)

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
Recorded here anyway since it's the most recent work on this codebase and the
next session needs accurate state regardless of which effort it belongs to.

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

**Historical release status:** this commit was local-only at the end of
session 3. It is now included in HEAD and the recorded `origin/main`.

## Remaining improvements

The original five-item list is complete. The following is a new backlog,
based on source inspection at `b1c217e0`, not measured new speedup claims.
As of 2026-09-08: C, D, and F are complete; B's stats-panel and one confirmed
menu hot path are complete; G was measured and found not justified; A's
larger async rearchitecture has since landed across four stages in its own
handoff doc, with a short residual list there (see A's own status line for
the pointer); E remains pending (see each item's own status line for current
detail).

### A. Keep networking and server startup responsive (highest user impact)

Status: **the two bounded fixes below landed and were verified 2026-09-07/08
(`00faeaf4`); the larger main-loop-driven async rearchitecture they deferred
has since landed too, across four stages spun out into its own handoff doc
-- see "What landed" at the end of this section.**

- Evidence: `NetworkClient::Connect` in `src/networkclient.cpp` uses blocking
  DNS/connect and a handshake wait. The leader-start path polls inside
  `HandleServerResponse`. `SendAll` retries with sleeps on Windows; POSIX
  sockets remain blocking for sends. `MainMenu::StartServer` in
  `src/mainmenu_server.cpp` sleeps unconditionally for one second.
- Change: advance connection/handshake/startup states from the main loop, move
  blocking name resolution off that loop, and queue partial writes on
  non-blocking sockets. Replace the fixed startup sleep with readiness checks.
- Preserve protocol ordering, hosted-bot servicing, cancellation, deadlines,
  and complete lines across partial sends. Reuse the platform's existing async
  paths where practical rather than creating a second protocol implementation.
- Verify: delayed/failed connection, fragmented replies, slow or non-reading
  peer, local-server launch failure, and cancellation. Demonstrate that input
  and rendering continue during waits; measure worst frame stalls.
- Scope this as a separate implementation batch: it changes more state than
  the smaller fixes below.

**What landed (`00faeaf4`):**

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
- **Deliberately out of scope for this slice, since landed as its own
  multi-session effort:** the main-loop-driven async state machine ("advance
  connection/handshake/startup states from the main loop... demonstrate that
  input and rendering continue during waits") was tracked separately as
  [`docs/ASYNC_NETWORKING_HANDOFF.md`](ASYNC_NETWORKING_HANDOFF.md), which now
  has **all four of its stages landed** (async command path, async connect +
  minimal connecting UI, async game start + level-sync stall mitigation, and
  a matching non-blocking output queue on the server side, closing audit
  `BUG-007`). `Connect()` is no longer a single synchronous call from
  `MainMenu`'s point of view -- the render loop keeps pumping through
  connect, lobby, game-start, and level-sync waits on both native and WASM,
  and this was confirmed live via a real two-browser WASM game against an
  ASan/UBSan server, not just by inspection.
  That doc's own "What's left" section lists what's still open: stage 3b's
  fallback path for `WaitForBubble`/`SyncNetworkLevel` is mitigated rather
  than truly rewritten (only the residual gate-timeout case can still block,
  and this session's playtest didn't hit it), one pre-existing WASM bug
  (heuristic CREATE confirmation) it never touched, an unrelated pre-existing
  server `select()` fairness bug found and deliberately left out of scope
  (flagged as its own background task), and no release has been tagged for
  any of it yet.

### B. Share fonts across cached labels

Status: **gameplay label sharing implemented and verified** on 2026-09-07.
Stats-panel sharing is in `1e2ed8e5`; targeting/name sharing is in `d79bf01b`.
Menu font sharing remains pending measurement.

- Evidence: `BubbleGame::StatsPanelCell` in `src/bubblegame_render.cpp` calls
  `LoadFont(path, size)` for every new cell. Targeting and name labels also
  load repeated font sizes in `src/bubblegame.cpp`.
- Change: share font ownership by the properties actually needed by each
  group while keeping independent per-label textures. Prefer explicit local
  ownership over an unbounded global cache. Ensure labels die before fonts
  and textures die before their renderer.
- Prerequisite: `TTFText::UpdateText` currently uses setter-driven dirty state;
  it does not detect changes made through another borrower or directly to a
  font. Investigate `TTF_GetFontGeneration` plus any layout properties it does
  not cover, or keep shared fonts immutable. Test both borrowers explicitly.
- Verify: count font opens/retained font instances on large stats panels;
  compare first-open time and memory; test style/alignment changes and teardown.
  Texture reuse must remain intact.
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
- Remaining work: measure opens and retained memory before extending this to
  menus. Do not combine borrowers that later change font size, style, or
  alignment unless `TTFText` tracks external font generations or those
  mutations are eliminated.
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
  counted benchmark). Every other `panelText` call site in
  `mainmenu_netpanel.cpp`, `mainmenu_teampanel.cpp`, and `menulist.cpp`
  remains unconverted -- most are one-off titles/headers/footers rendered
  once or twice per frame, not repeating per-row loops, so they were not
  "confirmed hot paths" by the standard this item sets. Re-scope and repeat
  this same pattern if profiling later shows one of them matters.

### C. Preserve frame timing precision in long sessions

Status: **implemented and verified** on 2026-09-07 in `1e2ed8e5`. A
one-day-elapsed regression first failed because 60 float
deadline increments did not total one second. It passed after changing the
frame deadline and interval to double precision and widening the frame/FPS
tick counters to `Uint64`.

- Evidence: `FrozenBubble::frameDeadline` is a `float`, and `RunOneFrame`
  converts `SDL_GetTicks()` to float before comparing deadlines. As absolute
  elapsed time grows, float spacing rounds away fractional frame intervals.
- Change: use double precision or integer high-resolution deadlines; review
  the related 32-bit tick fields when touching this code.
- Verify: synthetic elapsed times of hours/days, overrun resynchronization,
  suspend/resume, and stable 60 FPS pacing without catch-up bursts. Preserve
  the browser's requestAnimationFrame path and speed-multiplier semantics.
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
- Change: classify remaining frequent traces consistently across native/WASM;
  retain useful lifecycle messages, warnings, and errors. Preserve
  `FROZEN_BUBBLE_DEBUG=1` for diagnosis.
- Verify: compare emitted message counts for the same workload in default and
  debug modes, and check that actual failures remain visible. Do not blindly
  downgrade all logs or claim buffered file output eliminates console I/O.
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
- Remaining measurement: record a comparable emitted-message count for a
  repeatable gameplay/network workload. The logger still mirrors emitted
  messages to stderr; routine DEBUG messages no longer reach the callback in
  default mode.
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
- Change: first measure an idle populated room/team picker, then retain
  per-label textures for confirmed hot paths. Investigate text measurement
  that rasterizes candidate strings before replacing it with size queries.
- Verify: texture-creation counts after warm-up, changes in names/options,
  scrolling/truncation, hover/focus colors, and font-size changes. Observe the
  same layout and all keyboard/gamepad/touch/mouse paths; caching must not
  break tap registration or visible focus.
- **Overlaps item B.** The one repeating-list hot path identified by
  inspection -- `NetPanelLobbyActionsRender`'s game-room settings grid -- was
  implemented under item B's write-up (`b8409d30`) rather than duplicated
  here, since it is the same `panelText`/alternating-strings defeat B already
  describes. `mainmenu_teampanel.cpp` and `menulist.cpp` were inspected but
  not converted: their `panelText` calls are almost all one-off titles,
  headers, and footers rendered once or twice per frame (not a per-row loop
  churning against itself), so they did not clear the "confirmed hot path"
  bar this item sets. If a future session wants to pursue this further,
  measure texture-creation counts on an idle populated team picker first, the
  same way this session measured the settings grid's cell count, before
  converting anything.

### F. Correct player-name team color updates (small correctness fix)

Status: **implemented and verified** on 2026-09-07 in `1e2ed8e5`. The
regression first failed for both no-team -> team and team ->
no-team transitions, then passed after selecting the current color before
updating the text and resetting no-team labels to white.

- Evidence: `BubbleGame::UpdatePlayerNameWinText` calls `UpdateText` before
  `UpdateColor`, so the new color is applied to the texture on a later call.
  It also sets a color only for a real team, with no default-color reset when
  the player returns to `kNoTeam`. This can retain the previous team tint.
- Change: choose the current team/default color before updating the text on
  every call. Keep unchanged setters cheap.
- Verify: render a label through team A -> team B -> no team, asserting the
  correct color in the first frame of each change and texture retention on
  subsequent unchanged frames. Include reuse across rounds/game setup.
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
- Measure first on sparse, dense, mini-board, and multi-bot workloads. The
  existing score cache already improved one benchmark substantially.
- If justified, reuse bounded traversal storage and evaluate collision
  candidate pruning. Preserve grid bounds, hit ordering, landing cells,
  chosen angles, tie handling, and RNG consumption with differential tests.
- Keep a reproducible benchmark in the repository rather than relying on the
  old throwaway benchmark or comparing score sums alone.
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

### Measurement baseline for the next batch

Record test platform/build configuration, warm-up, scenario, and iteration
count. Track texture/font creation counts, CPU frame times (including slow
frames), and memory where relevant. Use representative menus, 2/5/20-player
games, stats screens, and bot loads. A percentage improvement in one scenario
does not establish a global FPS or battery-life improvement.

Suggested execution: address the small team-color fix and timing precision
early; treat networking as the highest-impact larger batch. Combine shared
fonts/cache correctness with measured menu caching if that scope stays clear.
Only pursue further bot changes if profiling warrants them.

## Suggested next session

1. Read current repository instructions and inspect git state. Use the current
   checkpoint, not release/push directions embedded in historical sections.
2. Continue from the new backlog. As of `00faeaf4`: C, D, F, and G are
   complete/closed (G deliberately not implemented -- see its status line).
   B and E have their one confirmed hot path each (or, for E, B's hot path
   doubling as E's) done, with the rest of `panelText`'s menu call sites left
   unconverted since they didn't clear the "confirmed hot path" bar. A's two
   bounded, low-risk fixes (`Connect()`'s connect-timeout bound + POSIX
   non-blocking send, `StartLocalServer`'s poll-instead-of-sleep) landed
   first, and its larger main-loop-driven async rearchitecture -- deferred as
   its own batch at the time -- has since landed too, across all four stages
   of `docs/ASYNC_NETWORKING_HANDOFF.md`. That doc's own "What's left"
   section is now the place to look for A's remaining scope (a mitigated but
   not fully rewritten level-sync fallback path, one untouched pre-existing
   WASM bug, an out-of-scope server fairness bug flagged separately, and no
   release tagged yet), not this file.
3. For the selected item, record a baseline or failing regression first, then
   implement and verify in proportion to the change. Preserve input parity
   required by the updated repository instructions.
4. Update this document after each meaningful checkpoint: pending/in progress/
   verified, changed files, reproducible measurements, tests, and limitations.
   Re-check commit/tag/push state before recording it. Do not re-run completed
   historical work merely because it appears earlier in this file.
5. No release tag was cut for this session's work (`21397a04` through
   `00faeaf4`, plus this doc update) -- it's all perf/test/doc changes, no
   version bump. Bump and tag only if the user asks, per the standing "always
   bump before tagging" rule.
