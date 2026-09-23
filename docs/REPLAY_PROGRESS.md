# Replay project — progress and session handoff

Last updated: 2026-09-21.

## Current checkpoint

- User request: review Pengupop's replay/playback approach, propose how to add it
  to Frozen Bubble, and keep a plan/progress log for separate sessions.
- Confirmed follow-up: **client-side only; every completed round auto-saves
  into a rolling Replays library (no Save button; keep count defaults to 5 and
  is user-adjustable), then local playback and `.fbr` export**. No replay
  upload, no video export, no server changes. Online matches remain in scope
  as client-recorded rounds.
- **R0 complete: research and documentation. R1's four seams implemented and
  test-passing; R1 itself not yet declared closed** -- all four seams
  (GameplayRng R1a, StepContext R1b, PlayerControls R1c, and the
  AdvanceSimulation()/Draw() split R1d, including its four sub-slices
  R1d-i..iv) are implemented and test-passing, but the live multiplayer visual
  checks noted below remain outstanding, and declaring R1 closed is a call for
  whoever is coordinating this project.
- **R2 complete: versioned snapshot/record codec and validation**
  (`src/replay_format.h/.cpp`, `tests/replay_format_test.cpp`) -- round-trip,
  deterministic hash, version rejection, exhaustive truncation sweep,
  oversized/corrupted-length rejection, malformed-magic/bad-record-type
  rejection and pinned byte fixtures all implemented and passing, including
  under ASan/UBSan. Codec-only: no real `BubbleGame`/`BubbleArray` state
  captured, nothing in the engine calls it yet.
- **R3 complete: one solo Classic round recorded and replayed offline**
  (`src/bubblegame_replay.h/.cpp`, `tests/bubblegame_replay_test.cpp`, plus
  two small production seams: an `UpdatePenguin()` Playback branch and an
  `AdvancePlaybackStep(float)` split from `AdvanceSimulation()`). A live
  scripted round's real controls/steps/state are captured into R2's records
  and a fresh game instance replays them, with the canonical state hash
  compared at every step. Covers win by clearing, loss via the danger zone
  after a wall/ceiling hit, hurry auto-fire, and 0.5x/2x scheduling; the disk
  writer/player round-trip and Playback-mode effect suppression are asserted.
- **R5a complete: step-driven local rules** (`src/bubblegame_replay.cpp`
  widened to capture/restore/hash 1-5 seats, board blob v2 storing each row's
  actual horizontal offset and the board's bubble size, a small game-rules
  blob in `levelLayout`, and a bot fire-angle capture in
  `bubblegame_shooter.cpp`). `tests/bubblegame_replay_test.cpp` extends R3's
  harness to 3-player local + bots (proving playback never re-runs
  `DriveBot()`), chain reactions, AttackMode::On/Canceling, teams, Race, a
  predefined (`randomLevels=false`) level, and a round captured after a live
  `ReloadGame()`. Network, UI/library and seek remain out of scope. See the
  dated 2026-09-20 R5a entry below.
- **R5b complete: recorded-game-clock seam + training and Timed mode**
  (`stepGameClockMs` sampled once per `AdvanceSimulation()` step and replayed
  from `StepRecord::gameClockMs`; `bubblegame_render.cpp`/`bubblegame_state.cpp`
  converted; `RestoreRoundStart` now accepts Timed/`mpTraining`).
  `bubblegame_replay_test.cpp` adds a training round and a local 3-seat Timed
  round, and the old Timed/training rejection cases are flipped to acceptance.
  Round-1-only for training/Timed; no network Timed; `durationMs` still 0.
  **R5 is now fully complete (R5a + R5b).**
- **R6a complete: steady-state 2-peer network message replay.** The parsed
  opcode switch moved out of `ProcessNetworkMessages()` into
  `ApplyInboundGameMessage()`; applied inbound payloads are captured per step
  in `StepRecord::inboundEvents` (seat 0's record); the round-start codec now
  carries real `seatOwned`/`seatIds` and a v3 board blob with per-seat
  nicknames; `RestoreRoundStart` accepts exactly a 2-seat network round
  (seat 0 local, seat 1 remote, no bots) and never runs `SyncNetworkLevel`;
  playback drains the recorded payloads instead of pumping the socket.
  `tests/bubblegame_replay_test.cpp` covers remote fire+stick (plus disk
  round-trip), malus both directions and remote-announced `F`, and rejects
  3-seat/owned-remote/bot-hosted network records. **R6 as a whole is NOT
  complete**: R6b (round-start sync, hosted bots, round 2+) and R6c (battle
  royale, delayed events, departures) remain. See the dated 2026-09-20 R6a
  entry below.
- **R6b complete: round-start sync, hosted bots and round 2+ continuity.**
  `RestoreRoundStart()` now accepts network records of 2-5 seats with a general
  per-seat ownership/isBot rule (seat 0 owned non-bot; every other seat owned
  iff a hosted bot). `SyncNetworkLevel()`'s layout and round 2+ continuity need
  no new mechanism -- the round start is still a full board snapshot taken after
  `NewGame()`/`ReloadGame()` resolves. A recorded `wasNetworkLeader` bit (rules
  blob) fixes `UpdateTimedRound()`'s Playback leader/joiner asymmetry for
  network Timed. `tests/bubblegame_replay_test.cpp` covers SyncNetworkLevel
  capture, round 2+ via `ReloadGame()`, a hosted bot, and the network Timed
  leader regression. See the dated 2026-09-20 R6b entry below.
- **R6c complete: battle royale, late result-tail events and mid-round
  departures.** The network seat bound was widened to `MAX_NET_PLAYERS` (20) in
  both `RestoreRoundStart()`'s range check and `SeatCount()` -- the latter
  capped every game at 5, silently truncating wide network captures and hashes.
  `tests/bubblegame_replay_test.cpp` adds a 7-seat battle-royale scenario
  exercising the `>5-alive` random malus branch and hosted bots with per-seat
  hash equality, a late remote `'S'` after `gameFinish` (the harness gained a
  defaulted `stopAtFinish` knob), and a mid-round `'l'` departure. **R6 as a
  whole is now complete (R6a + R6b + R6c).** See the dated 2026-09-20 R6c entry
  below.
- **R4 complete (R4a-R4e), all uncommitted.** R4a (production `ReplayRecorder`
  capture wiring), R4b (on-disk rolling `ReplayLibrary`), R4c (`ReplayPlayer`
  playback engine), R4d (the Replays page UI -- entry list, playback screen,
  keep-count, full keyboard/gamepad/tap input parity), R4e (desktop
  `SDL_ShowSaveFileDialog`/`SDL_ShowOpenFileDialog` and WASM Blob-download/
  file-input export/import, filling in R4d's stub seam). Every package is
  independently verified at the code-review and automated-test level
  (full `ctest` + ASan/UBSan). **Outstanding across all of R4: no manual
  click-through has been done on a real display** -- desktop native (blocked
  this session by the bare-Mach-O/LaunchServices limitation) or WASM in a
  real browser (blocked this session by a local Emscripten/upstream-`emsdk`
  toolchain mismatch, see the R4e entry below). R4d's own manual pass, once
  it did happen, found a real bug (see the 2026-09-21 bug-fix entry below) --
  so this gap is a real one, not a formality. Two unrelated bugs surfaced and
  fixed mid-session (see their own dated entries below): the replay-playback
  "no bubbles" state-machine hijack, and a pre-existing Windows client
  server-list bug (shipped as `v2.4.106`).
- **R7 complete (Android export/import bridge + platform-compatibility gate),
  uncommitted.** iOS dropped from scope per owner decision (immature, no CI,
  and a separate, already-tracked App Store licensing blocker makes
  distribution-facing iOS work low-value right now --
  [[ios-appstore-license-blocker]]). Two independent pieces: (1)
  `ReplayHeader::platformFloatProfile` -- previously an inert always-`0`
  field -- now has real semantics via `ComputeCurrentPlatformFloatProfile()`/
  `IsReplayPlatformCompatible()` (`src/replay_format.h/.cpp`), wired into
  capture (`src/replay_recorder.cpp`) and consumption
  (`ReplayLibrary::ReplayEntry::platformIncompatible`,
  `src/mainmenu_replays.cpp`'s `[INCOMPATIBLE]` badge/disabled-Play/
  defense-in-depth playback refusal/distinct import-mismatch message); (2)
  Android's `platform.cpp` stub branch split off from iOS's, reusing R4e's
  exact desktop `SDL_ShowSaveFileDialog`/`SDL_ShowOpenFileDialog` pattern
  with `SDL_SaveFile()`/`SDL_LoadFile()` byte I/O (transparently handles the
  `content://` URIs SDL3's vendored Android Storage Access Framework bridge
  returns; confirmed no manifest or Java/Kotlin changes were needed). iOS's
  branch is confirmed byte-for-byte unchanged. Independently verified
  2026-09-21: full clean rebuild, full `ctest` (100%, 45 total/43 run/2
  environmental skips -- same as every prior package), the 9
  replay/platform-related tests clean under ASan/UBSan, and a genuinely
  clean (source-touched, all 3 ABIs recompiled) `cd android && ./gradlew
  assembleRelease` -- BUILD SUCCESSFUL, no warnings on the changed file.
  **Outstanding: Android device/emulator click-through** -- no such tool is
  available in this session (unlike the iOS Simulator tool this environment
  does have), so this is a hard boundary carried forward alongside R4d's
  desktop native and R4e's WASM click-through gaps, not a shortcut. See the
  dated 2026-09-21 R7 entry below.
- **R8 complete (replay seek / shot navigation), uncommitted.** The plan's
  own row calls checkpoints "optional" and R8 defers them entirely: since
  `ReplayPlayer::Load()` already decodes a whole recording into memory
  (`groups_`), seeking is implemented as "rebuild the owned `BubbleGame`
  from scratch (the same pattern `Restart()` already used) and replay every
  step from 0 up to the target through the existing `ConsumeOneGroup()`
  forward-playback path" (`ReplayPlayer::SeekToStepIndex()`,
  `src/replay_player.h/.cpp`) -- which makes a seeked state byte-identical
  to sequential playback at that step by construction, and reuses existing
  desync detection for free. "Shot navigation"
  (`SeekToNextShot()`/`SeekToPreviousShot()`) is defined as jumping between
  step groups where some seat's `fire` control rises from false to true,
  precomputed once at `Load()` time (`shotIndices_`) since the codec has no
  dedicated shot event. A real gap was found and fixed while designing this:
  ordinary forward playback already plays production sound effects
  (`PlaySFX` calls in the shooter/board/state code are not gated on
  `SessionMode`), so a naive rebuild-and-replay seek would have re-fired
  every discarded intermediate step's sound; `AudioMixer::MuteAll()`/
  `IsHalted()` (already used identically in `bubblegame_input.cpp`'s mute
  hotkey) mutes the catch-up replay and restores the prior mute state after
  -- **note `MuteAll(bool enable = false)`'s argument is inverted from its
  name: `enable=false`/default mutes, `enable=true`/unmutes**, confirmed
  directly against `audiomixer.cpp` before use, since a naive reading could
  easily get this backwards. Two new UI controls on the playback screen
  ("< Shot" / "Shot >", flanking the existing 4-button control bar,
  `SDLK_COMMA`/`SDLK_PERIOD` plus matching tap targets,
  `src/mainmenu_replays.cpp`/`src/mainmenu.h`) expose shot navigation; no
  drag-able arbitrary-position scrub bar was built (deliberately deferred,
  see the dated entry below). A small `FROZEN_BUBBLE_TEST_ACCESS`-gated
  `AudioMixer::TestSfxPlayCount()`/`TestResetSfxPlayCount()` counter
  (`src/audiomixer.h/.cpp`) was added so the mute-during-seek behavior has
  real automated coverage rather than being eyeballed. Independently
  verified 2026-09-21: every changed file read in full and checked against
  the plan (including a subtlety the implementer correctly caught and I
  independently re-confirmed -- the `MuteAll` argument inversion above), a
  genuinely clean from-scratch rebuild, full `ctest` (100%, 45 total/43
  run/2 environmental skips -- unchanged from before R8), and the 9
  replay/platform tests clean under ASan/UBSan including the four new
  `replay-player-test` scenarios (forward seek, backward seek, shot
  navigation, seek-mute/restore with an unmuted playback control case
  proving the zero-count assertion is real). **With R8 done, the entire
  replay feature (R1 through R8) is code-complete and independently
  verified at the code-review/automated-test level.** See the dated
  2026-09-21 R8 entry below.
- **Next: nothing further planned in this project's own package list.**
  Outstanding before this feature could ship: the three manual
  click-through gaps carried forward across the whole feature (desktop
  native, WASM in-browser, Android device/emulator -- none of R4d's, R4e's
  or R7's own respectively have been exercised on a real display/device in
  this session), manual click-through of R8's two new playback buttons
  specifically, and a commit/release decision -- none of R1-R8 has been
  committed; the working tree still carries all of it as uncommitted edits
  per the standing instruction to only commit on explicit request.
- R1's four seams and the `SessionMode` effects boundary are foundations; R2
  is an isolated codec; R3 is the first package that actually captures real
  game state into that codec and replays it. R4 is the first package a
  player can actually reach and use end to end (modulo the manual
  verification gap above). R7 closed the last engineering gap (mobile
  export/import + a real platform-compatibility gate); R8 added seek/shot
  navigation on top of a fully reliable sequential-playback foundation, per
  the plan's own stated precondition for it.
- Baseline: branch `main`, source commit `e18e00e79e07c8e3a74160258ed15adf370fb094`
  (after the Windows curl fix / `v2.4.106` release), CMake version `2.4.106`.
  Working tree carries all of R1-R8 uncommitted on top of that.
- Read [REPLAY_RESEARCH.md](REPLAY_RESEARCH.md) for evidence and
  [REPLAY_PLAN.md](REPLAY_PLAN.md) for design, packages and acceptance criteria.

## Findings to preserve across sessions

1. The AUR repo contains packaging. Its checksum-verified Pengupop 2.2.6 source
   contains multiplayer event uploads, **not a playback implementation**. A
   historical external viewer/server remains unverified. No implementation work
   needs to wait for finding it.
2. Proposed design: explicit starting state + recorded variable steps + normalized
   input + isolated gameplay RNG + state checks. A global fixed-step conversion
   is not part of the first replay milestone.
3. Frozen Bubble advances gameplay in `Render()`. Splitting simulation from
   drawing, preserving order, and supplying clocks/input/RNG are prerequisites.
4. Network replays need local controls AND applied remote events, stable seat
   identity, bot ownership, exact remote placement and round-sync state. Current
   capacity is 20 network players, despite older five-player architecture notes.
5. Playback must not persist or publish game results. Do not treat setting
   `networkGame = false` as sufficient: that changes gameplay and can enable
   solo highscore paths.
6. Revised user-facing milestone: round completes → auto-saved into Replays
   (rolling, default last 5) → Play / Export, plus Import replay and the
   keep-count setting in the same submenu. Solo R3 is only a
   technical proof; qualify local/online multiplayer R5/R6 before completing
   R4. Revised order: R1 → R2 → R3 → R5/R6 → R4 → R7 → R8 (IDs retained for
   existing references).
7. Capture automatically from round start into temporary storage, then finalize
   and write automatically when the round settles — no Save button, no
   user-initiated request. Keep the finalize/write path independent of game
   state: peers can advance rounds before the write completes, and the write
   must survive that reset. The library holds at most the configured keep
   count (default 5); a write past it evicts the oldest.

## Progress log

### 2026-09-20 — R0: source review and plan

Completed:

- Cloned the AUR packaging repository and pinned its revision; fetched its
  source archive and confirmed SHA-256 against the package metadata.
- Traced event definitions, emission, buffering, master-server submission,
  result flushing, seeded initialization, shot color selection, hostile bubble
  generation and fixed-tick updates. Inspected the live movement-packet path.
- Searched the supplied sources/build manifest for a playback reader/viewer;
  none found. Web searches did not establish a separate playback source.
- Reviewed Frozen Bubble's current render/update loop, normalized movement,
  input, randomness, bots, local/network authority, round lifecycle, persistence
  helpers and existing test seams. Cross-checked shot/stick/malus rules against
  the bundled original Perl source.
- Wrote the research record, staged implementation plan and this handoff log.

Validation:

- Archive checksum matched; source symbols and paths inspected directly.
- Documentation links and whitespace checked before delivery.
- No builds, gameplay tests or runtime playback performed. This is a
  documentation-only change; reproducibility is a proposed acceptance criterion,
  not a verified capability.

Open context:

- The user recalls that replays may have been on a website, but does not
  remember which one. This is consistent with the client upload path; the
  service and its implementation remain unverified. A later URL/reference may
  identify playback code outside the reviewed archive.

### 2026-09-20 — Scope refinement: round-stats save, local playback/export

Completed planning work:

- Incorporated the user's explicit client-only scope and stats-screen entry
  point. No replay server, upload or protocol work is planned.
- Inspected `RenderRoundStats`, `HandleFinishedTap`, result-key/gamepad handling,
  peer-driven next-round transitions, and existing platform bridges.
- Specified automatic temporary capture vs explicit durable Save, detached
  completed-round ownership, bounded late-stats finalization, retry/status UI,
  and protection against save/continue/chat actions falling through each other.
- Specified Replays library/player and `.fbr` file export/import paths for
  desktop, browser and mobile, with links to platform API documentation.
- Moved local/network replay qualification ahead of the first completed product
  milestone. Desktop/browser file export is part of R4 rather than postponed
  to general platform qualification.

Validation: source inspection and documentation checks only. No implementation
or runtime tests. The three replay documents remain uncommitted.

Pending preference at the time: asked whether export means a replay file,
video, or both. **Answered 2026-09-20 — `.fbr` file only, no video.** See the
entry below.

### 2026-09-20 — Scope revision: auto-save last 5, Save button removed

Completed planning work:

- User directive: auto-save the last 5 completed rounds as `.fbr` replays with
  no explicit Save action; the Replays library is where the player later
  chooses to Play or Export any of them.
- Asked and confirmed this **replaces** the explicit Save-replay design from
  the prior entry outright, rather than coexisting with a pin/favorite
  mechanism that would have kept most of the save-flow UI.
- Updated `REPLAY_PLAN.md`: recommended approach, recording contract, network
  result-tail sealing, the former "Round-stats save flow" section (renamed
  "Round-stats behavior"), replay library, local storage and export, the R4
  work-package row, and "Decisions still open".
- Net effect on R4: **smaller**. The round-stats screen needs no new
  interactive element, so the `HandleFinishedTap()`/`HandleInput()` result-action
  work, focus/tap-target parity, duplicate-press guarding and Saving…/retry
  states all drop out of scope; at most a non-interactive "Saved to Replays"
  status line remains. Storage engineering does **not** get simpler: eviction
  on the 6th write is new, and the old "save must survive a reset" requirement
  now applies to every round unconditionally instead of only on a button press.
- Resolved the open question on saved-library quota (fixed at 5, oldest
  evicted). Replaced it with a narrower open question: whether that cap should
  later be user-configurable.

Decided details worth preserving: imported `.fbr` files count toward the same
cap and can evict an auto-saved round; Delete frees a slot early
without triggering eviction; exporting copies bytes out and does **not** exempt
an entry from later eviction, so export is the only way to keep a round beyond
the rolling window.

Validation: documentation edits only. No implementation, build or runtime
tests. The three replay documents remain uncommitted.

### 2026-09-20 — Keep count becomes a setting; export scope closed at `.fbr`

User decisions, all folded into `REPLAY_PLAN.md`:

- The rolling cap of 5 is a **default, not a fixed limit** — the player can
  raise or lower it. This closes the open question the previous entry raised.
- **Export is `.fbr` only.** The long-open video question is answered no; the
  plan now states plainly that no MP4/WebM encoding stage is in scope, rather
  than leaving it listed as pending.
- The **Replays submenu owns all three** of playback, export and the keep-count
  setting, so the setting lives with the feature rather than in the general
  settings panel.

Design details decided while writing this up, each flagged in the plan as a
proposal open to reversal:

- Keep count persists in `GameSettings` (the existing INI-backed preferences),
  not inside replay files.
- Proposed range 1–20 with **0 meaning "do not record"**, since "less"
  reasonably extends to none and a player who wants no capture cost needs an
  off switch. At 0, capture is skipped at round start; existing entries are
  kept, because a setting should govern future writes and not act as a
  bulk-delete.
- The **upper bound is deliberately left unfixed** pending measured `.fbr`
  sizes from R2/R3, consistent with this plan's refusal to assert unmeasured
  numbers.
- **Lowering the count deletes files**, so it evicts down immediately behind
  the existing confirm dialog, naming the exact number to be deleted, with ESC
  cancelling. The alternative — defer eviction until the next write — was
  rejected because the library would visibly hold more entries than the
  setting claims. Raising never deletes and never restores evicted rounds.

Input-parity note carried into R4: the previous entry recorded that the
parity checklist drops out of the **round-stats screen**. It returns in full
for the **Replays submenu**, which is now several interactive rows plus a
confirm dialog — keyboard/gamepad navigation, `BeginPanelTapRows`/
`AddPanelTapRow` tap targets, visible focus on the keep-count control and on
both confirm buttons, and a footer hint. `CLAUDE.md` documents this exact bug
class shipping more than once, so it is called out explicitly in the plan.

Net R4 scope: larger than the previous entry left it. The save-flow UI stayed
deleted, but a settings row, its persistence, its bounds, and destructive-
lowering semantics with confirmation are new work, and `gamesettings.*` joined
R4's file list.

Validation: documentation edits only. No implementation, build or runtime
tests. The three replay documents remain uncommitted.

### 2026-09-20 — R1 slice 1: GameplayRng (first implementation session)

Starting source revision: `main` at `61b00290ef264dbcca5135e9a50e4227d939cbad`
(unchanged since R0), working tree otherwise clean apart from the three
uncommitted replay docs. Changes below are **uncommitted**.

**R1 turned out to be four independent seams, not one.** A single
implementation request ("let's start implementing this") led to three
parallel deep-dive explorations across `bubblegame_render.cpp` (1588 lines),
`bubblegame_state.cpp`, `bubblegame_shooter.cpp`, `bubblegame_board.cpp`,
`bubblegame_level.cpp`, `bubblegame.h/.cpp`, `bubblegame_internal.h`,
`frozenbubble.cpp` and `bubblegame_input.cpp` (~6,600 lines total): the full
`Render()` state-mutation call graph, every RNG/clock call site, and every
input-polling/external-side-effect call site. Splitting R1 into ordered
slices (rather than attempting all of it at once, which the plan already
warned against) followed directly from what that inventory found. This
session implemented only the first slice; the inventory itself is recorded
here so it does not need to be re-derived.

**Inventory findings, condensed** (see `git log`/this session's transcript
for full file:line detail if ever needed again — the essentials are captured
below):

- **RNG.** Every gameplay-affecting `rand()`/`ranrange()` call (levels,
  next-bubble colors, malus color/column/jitter, mp_train malus injection,
  `SingleBubble::GenerateFreeFall()`) shared libc's one global `rand()`
  stream with cosmetic code (menu candy, highscore-entry picture, blink
  icons, transition snow) — meaning nothing gameplay drew was capturable in
  isolation, and a cosmetic draw between two gameplay draws could shift the
  sequence. All such call sites turned out to already be `BubbleGame::`
  methods except `SingleBubble::GenerateFreeFall()` (no back-reference to
  `BubbleGame`) and a `DoFalling()` free function discovered only once
  actually wiring this up (see "Surprises" below). `botRng` (per-bot,
  already isolated, wall-clock-seeded) was confirmed *not* to need touching:
  the plan captures bot decisions as replay controls rather than
  re-deriving them from RNG state, so its determinism isn't a
  replay-correctness requirement.
- **Clock.** `frameCount` only advances in network/local-multiplayer/
  mp_train modes, never in solo campaign — confirmed, not usable as a
  universal step counter. `deltaScale` is read from the `FrozenBubble`
  singleton inside `UpdateSingleBubbles`, `DriveBot`, `UpdatePenguin` and
  `DrawAimGuide` rather than passed in. Both are the concrete targets for a
  future `StepContext` slice, not touched this session.
- **Draw/mutate crossovers** (target of a future `AdvanceSimulation()`
  slice, not touched this session): `UpdatePenguin()` draws the hurry
  texture mid-function; the stick-animation block, `RenderMalusAlerts()`,
  the chat overlay and the attack-flash blink each draw *and* age/mutate
  state in the same call; every `Update*Text` helper recomputes *and* blits
  in one call (a systemic naming trap, not an isolated case);
  `RenderRoundStats()` mutates hit-test rects used later by
  `HandleFinishedTap()`. Confirmed pure mutators despite "Do…Animation"
  naming: `DoFrozenAnimation`, `DoWinAnimation`, `DoPrelightAnimation`,
  `ExpandNewLane`, `CheckPossibleDestroy`, `CheckAirBubbles`.
- **External side effects** (target of a future `SessionMode::Live/Playback`
  slice, not touched this session): `SubmitScore()` (highscores to disk),
  `ReportTournamentResult()`, `FinalizeRoundStats()` (network `'S'`),
  `SendLobbyMatchSummary()`, opt-in stats upload (`sendGameStats.cpp`),
  in-round chat send, malus/shot network broadcasts, and — found
  unexpectedly — a live settings write reachable from mid-round input (the
  colorblind toggle writes `settings.ini` immediately on keypress,
  `bubblegame_input.cpp:271-277`). `HandleFinishedTap()` was confirmed to be
  a hard no-op for non-network games, exactly as the plan already claimed.

**Implemented this session: `GameplayRng`**, an isolated gameplay-only RNG
stream, following the plan's own scoped-down slice 1.

- New `src/gameplay_rng.h`: header-only `GameplayRng` class using the same
  LCG family already proven by `botRng`/`BubbleAI::ChooseShot`'s
  `next_random` lambda (`state = state*1103515245u + 12345u`). `Seed()`,
  `State()`, `Next()`, and `Range(int,int)`/`Range(float)` mirroring the old
  `ranrange()`'s exact semantics.
- `BubbleGame` gained a `GameplayRng rng` member (`src/bubblegame.h`,
  alongside `frameCount`) and a `bool rngExplicitlySeeded` flag (see
  "Surprises" below). Seeded in `NewGame()` from the same wall-clock-derived
  approach `botRng` already uses; **not** reseeded by `ReloadGame()`, so it
  carries forward round to round within a match.
- Migrated every class-A call site to `rng.Range(...)`:
  `bubblegame_level.cpp` (`RandomLevel`, `SyncNetworkLevel`),
  `bubblegame_shooter.cpp` (`ChooseFirstBubble`, `PickNextBubble`),
  `bubblegame_board.cpp` (`CheckPossibleDestroy`'s re-pick, `ExpandNewLane`,
  and the newly-found `DoFalling()`), `bubblegame_state.cpp`
  (`SendMalusToOpponent`'s target pick, `ProcessMalusQueue`),
  `bubblegame_render.cpp` (mp_train malus auto-injection).
  `SingleBubble::GenerateFreeFall()` took `GameplayRng&` as a new parameter
  (3 call sites updated); `DoFalling()` likewise (1 call site). Left every
  class-B (cosmetic) site — `highscoremanager.cpp`, `mainmenu.cpp`,
  `mainmenu_panels.cpp`, `shaderstuff.cpp` — on libc `rand()`/its own
  `mainmenu_internal.h` `ranrange()`, untouched.
- The now-dead `ranrange()` pair in `bubblegame_internal.h` (zero remaining
  call sites after the migration) was deleted rather than left as dead code;
  `mainmenu_internal.h`'s separate copy, still used by cosmetic code, was
  left alone.
- `tests/bot_play_test.cpp` — the *only* test pinning gameplay determinism
  via `std::srand()` (confirmed by grepping all of `tests/*.cpp`) — now
  seeds `game.rng` explicitly instead, via a new
  `BubbleGameTestAccess::seedRng()` helper.
- New `tests/gameplay_rng_test.cpp` (registered in root `CMakeLists.txt` as
  a small standalone executable, no SDL dependency): same-seed determinism,
  different-seed divergence, `Range()` bounds including `a==b`, and —  the
  actual property this type exists for — that interleaving unrelated
  `rand()`/`srand()` calls between draws does not perturb the sequence.

**Surprises found only while wiring this up, beyond what the inventory
predicted:**

1. `DoFalling()` (`bubblegame_board.cpp:579`) is a **free function**, not a
   `BubbleGame::` method, despite sitting textually between two
   `BubbleGame::` methods and reading the `singleBubbles` global — an
   `awk`-based scan for the enclosing function name silently attributed its
   call site to the *previous* `BubbleGame::` method until the compiler
   caught it as "undeclared identifier `rng`". Fixed by adding
   `GameplayRng &rng` as an explicit parameter, matching
   `SingleBubble::GenerateFreeFall()`'s treatment. Lesson for future slices
   digging through this file: do not trust a quick scan for the enclosing
   function; grep for the literal `BubbleGame::`/free-function signature
   immediately above each call site.
2. `NewGame()` calls level generation (which consumes `rng`) **before it
   returns**, so a caller wanting a deterministic level — i.e. every future
   test, and later the replay recorder itself — must seed `rng` *before*
   calling `NewGame()`, not after. But `NewGame()` also unconditionally
   reseeds `rng` from the wall clock as its first act, which would silently
   discard a pre-set test seed. This doesn't affect `botRng` (bot decisions
   aren't consumed until frames run *after* `NewGame()` returns), so the
   asymmetry wasn't visible until actually updating `bot_play_test.cpp`.
   Fixed with a `rngExplicitlySeeded` flag: `NewGame()` only reseeds from the
   wall clock when nobody has already seeded `rng` for this `BubbleGame`
   instance. This is the one deviation from the original plan text ("seed it
   in `NewGame()` ... with the same wall-clock-derived approach `botRng`
   already uses") — the wall-clock seeding itself is unchanged, but it's now
   conditional rather than unconditional.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel      # clean build, no new warnings from this change
ctest --test-dir build --output-on-failure
```

38 tests total, 36 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` — pre-existing environmental skips, unrelated to this
change; see `CLAUDE.md`'s sanitizer-build note for the other kind of
intentional skip). `gameplay-rng-test` (new) and `bot-play-test` (migrated
off `std::srand()`) both pass; `bot-play-test` in particular runs several
thousand simulated frames of real gameplay — level generation, shots,
pops, chain reactions, malus — through every migrated call site.

**Manual/platform checks: completed.** Per this plan's own acceptance line
("live feel manually checked"), the user played two rounds of a real network
game against the production server (fb.servequake.com), using two locally
built `./build/frozen-bubble-sdl3` instances. This exercises the one
network-only migrated call site local/solo play never touches —
`SyncNetworkLevel()` (`bubblegame_level.cpp:225,300-301,328`, the
leader-to-joiner bubble-position sync) — as well as `RandomLevel()`,
next-bubble picks, and malus, across two consecutive rounds (so the
non-reseeded-between-rounds behavior in `NewGame()` was also implicitly
covered). Reported result: "game looked fine." (First attempt at this check,
earlier in the session, was denied screen-access by the user; computer-use
automation to drive the build directly also turned out to be unreliable in
that sandboxed session — a bare Mach-O launched from Terminal doesn't
register with macOS LaunchServices, so the bundle-scoped tools couldn't see
it, and full-screen capture couldn't locate its window either. Playing it
directly, as done here, is simpler and should be the default for this check
going forward rather than attempting computer-use automation.)

**Known failures/blockers:** none found. Build is clean, full test suite
passes, manual visual/network check passed.

**Design changes and reasons:** `rngExplicitlySeeded` (see "Surprises" #2).
Everything else matches the approved plan as written.

### 2026-09-20 — R1 slice 2: StepContext (explicit deltaScale + simStep)

**Starting source revision:** uncommitted, directly on top of the R1 slice 1
(`GameplayRng`) session above, same branch (`main`).

**Re-inventory before implementing.** The original R1 three-agent inventory
(slice-1 session, above) predates slice 1's own edits, so I re-grepped/re-read
the actual current source rather than trusting the old note. The four
`deltaScale` singleton-read call sites the plan named were confirmed
essentially as described:

| Site | What it does |
|---|---|
| `bubblegame_shooter.cpp:121` (`DriveBot`) | bot's turn-rate step |
| `bubblegame_shooter.cpp:370` (`UpdatePenguin`) | keyboard/gamepad turn-rate step; also calls `DriveBot` |
| `bubblegame_shooter.cpp:577-579` (`UpdateSingleBubbles(int id)`) | vestigial wrapper around `UpdateSingleBubblesAtScale(float)`, which already took `deltaScale` explicitly; the `id` param was dead (comment at its call site already said so) |
| `bubblegame_render.cpp:344` (`DrawAimGuide`, static free fn) | dotted aim-trajectory preview; own `static` smoothing state (`smoothedDs`/`lastRawDs`), untouched by this change |

All four are reached only from `BubbleGame::Render()` (confirmed no other
callers of `DriveBot`; `UpdatePenguin` called at `bubblegame_render.cpp:1054,
1161` plus one test call site; the old `UpdateSingleBubbles(0)` at
`bubblegame_render.cpp:1037, 1093`; `DrawAimGuide` at `:1063, 1175`).

**Surprise found only while re-reading `frameCount`.** The plan's one-line
note ("`frameCount` ... does not advance in solo campaign") undersold what
re-reading its actual consumers showed:
- It resets to `0` in **both** `NewGame()` (`bubblegame.cpp:1059`) and
  `ReloadGame()` (`bubblegame.cpp:1404`) — round-relative by design, not a
  match-long counter.
- Its two increment sites in `Render()` are themselves mode-gated:
  unconditional inside the `networkGame` branch (`:875`), separately gated on
  `!gameFinish` for local-multiplayer(≥2)/mp_train (`:886-888`). Solo never
  reaches either.
- Its only consumer, `ProcessMalusQueue()` (`bubblegame_state.cpp:302`),
  no-ops unless `networkGame || mpTraining || playerCount >= 2` — i.e.
  `frameCount` only ever matters in exactly the modes where it already
  increments. It gates a fixed 20-frame malus-freeze window, synced the same
  way on every client in a network game.

Read literally, "introduce a real monotonic step counter to replace
`frameCount`" would mean changing malus-fall timing cadence in every live
multiplayer/network mode — a correctness risk to tested, network-synced
behavior, not a plumbing change. **Decision: did not touch `frameCount` or
any of its three consumers** (the two `malusQueue.push_back(frameCount)`
sites, `ProcessMalusQueue`'s comparison, the blink-period modulo at
`bubblegame_render.cpp:1287`). Added a new, separate, genuinely monotonic
`int simStep` member instead — incremented unconditionally once per
`Render()` call (every mode, including solo, even while `gameFinish`), reset
only in `NewGame()` (not `ReloadGame()`), mirroring how `rng` is deliberately
left alone across `ReloadGame()` in slice 1. `simStep` has no consumer yet
this slice; it exists for the future replay recorder, which is the actual
thing that needs a uniform per-frame index across every mode.

**Implementation:**
- `src/bubblegame.h`: added `int simStep = 0;` next to `rng`, with a comment
  explaining the `frameCount` decision above. Changed declarations to
  `DriveBot(BubbleArray &bArray, float deltaScale)` and
  `UpdatePenguin(BubbleArray &bArray, float deltaScale)`. Removed the
  `UpdateSingleBubbles(int id)` declaration.
- `src/bubblegame_shooter.cpp`: `DriveBot` and `UpdatePenguin` both take
  `deltaScale` as a parameter now instead of reading the singleton;
  `UpdatePenguin`'s internal call to `DriveBot` passes it through. Deleted
  the `UpdateSingleBubbles(int id)` wrapper entirely.
- `src/bubblegame_render.cpp`: `Render()` now reads
  `const float deltaScale = FrozenBubble::Instance()->deltaScale;` once at
  the top (the one place a live wall-clock read is legitimate) and
  increments `simStep` unconditionally in the same spot, before any mode
  branching. Both former `UpdateSingleBubbles(0)` call sites now call
  `UpdateSingleBubblesAtScale(deltaScale)` directly. Both `UpdatePenguin`
  call sites and both `DrawAimGuide` call sites now pass `deltaScale`
  explicitly. `DrawAimGuide` itself takes `deltaScale` as a new parameter
  instead of reading the singleton at its old line 344; its `static`
  smoothing variables are untouched, just fed the parameter.
- `tests/bot_play_test.cpp`: updated the one direct `UpdatePenguin` call
  (inside `RunFrames`) to pass `1.0f`, matching the scale already used for
  `BubbleGameTestAccess::step(game, 1.0f)` in the same loop. No other test
  called `UpdatePenguin`, `DriveBot`, `DrawAimGuide`, or the old
  `UpdateSingleBubbles` wrapper directly (confirmed via grep).

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel      # clean build, no new warnings from this change
ctest --test-dir build --output-on-failure
```

38 tests total, 36 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` — same pre-existing environmental skips as slice 1,
unrelated to this change). No new test file this slice: this is a mechanical
plumbing change with no new distinct code path (same values reach the same
call sites, just via a parameter instead of a global read), and `simStep`
itself has no consumer yet to assert behavior against — `bot-play-test`
exercises `UpdatePenguin`/`DriveBot`/`UpdateSingleBubblesAtScale` every
simulated frame already, which validates the threading compiles and behaves
identically.

**Manual/platform checks: completed.** User played two rounds of a network
game against fb.servequake.com with a bot — a stricter check than the
solo-round check the plan asked for (this slice touches no network-synced
state, so solo would have sufficed, but the network path exercises the same
`deltaScale`-threaded code plus everything slice 1 already covers).
Reported result: "seemed normal from what i could tell" — aim/turn
responsiveness and bot movement unaffected.

**Known failures/blockers:** none found. Build is clean, full test suite
passes, manual check passed.

**Design changes and reasons:** `simStep` added as new, separate state
rather than reinterpreting `frameCount` (see "Surprise" above for the full
reasoning) — this is the one deviation from the plan's literal "replace
frameCount" phrasing.

### 2026-09-20 — R1 slice 3: PlayerControls (capture resolved input, prove replay)

**Starting source revision:** uncommitted, directly on top of the R1 slice 2
(`StepContext`) session above, same branch (`main`).

**Re-read of `UpdatePenguin()` before implementing** (it's the function
slice 2 just touched, so re-checked for drift rather than trusting the old
inventory). Confirmed three phases back to back in one function
(`bubblegame_shooter.cpp:135-393`): (1) input resolution — keyboard/gamepad
reads or `DriveBot()` into `bArray.shooterLeft/Right/Center/Action`, then the
hurry timer, which can force `shooterAction = true` and has its own
draw/SFX side effects (untouched — R1d's job); (2) mouse/touch injection —
snaps `shooterSprite.angle` straight to `mouseTargetAngle` and turns a
pending click into `shooterAction = true`, capturing `firedByMouse` before
the flag is cleared; (3) the fire/hurry/release gate and turn math.

**The one thing that couldn't just be copied: the angle snap.** Phase 3
never read `mouseTargetAngle` — by the time it ran, phase 2 had already
mutated `shooterSprite.angle` directly. A replay that only re-runs "phase 3
driven by a captured struct" would silently produce a keyboard-turn
trajectory instead of the recorded mouse-aimed shot for any mouse/touch shot
if the snap weren't moved. Fixed by moving the snap out of resolution and
into application, gated on `controls.mouseAngle >= 0.f`, driven by the
captured value instead of a live mouse read.

**Implementation:**
- New `src/player_controls.h`: `PlayerControls{ left, right, center, fire,
  firedByMouse, mouseAngle }`, pure data, no logic.
- `src/bubblegame.h`: `#include "player_controls.h"`; `BubbleArray` gained
  `PlayerControls lastControls` (inert this slice, the anchor a future
  recorder hooks into — same pattern as `simStep` being inert until slice 2
  landed). `BubbleGame` gained `ResolvePlayerControls`/`ApplyPlayerControls`
  declarations alongside `UpdatePenguin`/`DriveBot`.
- `src/bubblegame_shooter.cpp`: split the old `UpdatePenguin` into
  `ResolvePlayerControls()` (old phases 1-2, unchanged except the angle snap
  moved out; ends by returning a `PlayerControls` built from `bArray`),
  `ApplyPlayerControls()` (old phase 3, now writes
  `shooterLeft/Right/Center/Action`/`mouseTargetAngle` from the passed
  struct at the top instead of trusting live `bArray` state, then does the
  relocated angle snap, then the unchanged fire/hurry/release gate and turn
  math with `firedByMouse` read from `controls`), and a thin `UpdatePenguin`
  orchestrator that calls both and stores the result in
  `bArray.lastControls`. `DriveBot` itself is untouched, still called from
  inside `ResolvePlayerControls`.

**New test: proves replay independence, not just determinism.**
`tests/bot_play_test.cpp` builds two fully independent `BubbleGame`
instances ("live" and "replay") seeded identically. Live runs a mixed
sequence (keyboard-turn frames via `controllerInputs[0]`, a mouse-aim frame,
a mouse-fired shot) by calling `ResolvePlayerControls` +
`ApplyPlayerControls` each frame, same as `UpdatePenguin` does internally,
capturing the returned `PlayerControls` and the resulting angle/`newShoot`
each frame. Replay then calls **only** `ApplyPlayerControls`, fed the
captured structs in order, **never calling `ResolvePlayerControls`** — and
critically, replay's own live input sources (`controllerInputs[0]`,
`mouseTargetAngle`) are deliberately set to the *opposite* of what produced
the original controls before the loop starts. This is what makes the test
prove `ApplyPlayerControls` doesn't secretly still read live state: if it
did, the two runs would diverge despite identical captured controls. Angle
and fire (`newShoot`) outcomes match frame-for-frame.

**Finding: `suppressFireUntilRelease` diverges between live and replay, and
that's fine.** The first version of this test also asserted
`suppressFireUntilRelease` equality and failed on it for every
pre-fire frame. Root cause: `ResetRoundInputState()`
(`bubblegame.cpp:322-339`, called at round start) deliberately sets
`suppressFireUntilRelease = true` for every player ("block fire key for one
frame after round transition," `bubblegame.h`'s declaration comment) — a
real live-game protection against an already-held fire button instantly
firing into a new round. The *clearing* of that flag lives entirely inside
`ResolvePlayerControls`'s live-device poll (`if (suppressFireUntilRelease &&
!IsKeyPressed(fire) && !controllerInputs[idx].fire) suppressFireUntilRelease
= false;`) and `DriveBot`'s own unconditional one-frame clear for bots —
both Resolve-phase, both requiring a live/AI decision that pure
`ApplyPlayerControls` playback never runs. So a replay-only run gets stuck
with the round-start value.

Checked whether this actually matters: grepped the whole tree for
`suppressFireUntilRelease` (`bubblegame.h`, `bubblegame.cpp`,
`bubblegame_shooter.cpp` only). **`ApplyPlayerControls` never reads the flag
for any decision** — its own fire check is `mpFirePending ||
(!localMalusInFlight && shooterAction && newShoot)`, no
`suppressFireUntilRelease` reference. The flag is pure Resolve/DriveBot-side
bookkeeping about live-input release detection; it has zero effect on what
replay actually produces. Removed the equality assertion (with a comment
explaining why, not a silent deletion) rather than trying to make
`ApplyPlayerControls` reproduce an internal bit it was never designed to
consume. The angle/`newShoot` checks — the actual "fire/aim outcome" the
acceptance criterion asks about — are what stayed, and they pass.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel      # clean build, no new warnings from this change
./build/bot-play-test                # standalone: all scenarios incl. new replay-equality block pass
ctest --test-dir build --output-on-failure
```

`bot-play-test` (existing scenarios plus the new replay-equality block) runs
clean standalone. Full suite: 38 tests total, 36 passed, 2 skipped
(`server-massleave-test`, `server-udp-probe-test` — same pre-existing
environmental skips as slices 1 and 2, unrelated to this change).

**Manual/platform check: completed.** User played a solo round on the
freshly built `./build/frozen-bubble-sdl3` binary (launched directly, not
the stale `/Applications/FrozenBubble.app` copy — that one predates today's
R1a-d changes and was killed first). Covered both a keyboard-turn shot and a
mouse-aimed shot. Reported result: "I was able to shoot and aim with
keyboard and mouse." (Automation note: computer-use tooling in this session
could not reach the real display — its screenshots stayed on a stale,
disconnected virtual desktop while a direct `screencapture` confirmed the
real game window was live and correctly loaded — so this check was done by
the user directly playing, consistent with the automation note under the
R1a entry above.)

**Known failures/blockers:** none. Build clean, full test suite passes,
manual check passed. **R1c is fully complete.**

**Design changes and reasons:** angle-snap relocation from Resolve to Apply
(required for replay correctness, not optional — see above).
`suppressFireUntilRelease` scoped out of the replay-equality contract with a
documented reason (it's write-only from Apply's perspective, read only by
Resolve/DriveBot, which replay never calls) rather than extending
`PlayerControls` to also capture raw "is the button still physically held"
state that nothing downstream needs.

### 2026-09-20 — R1d sub-slice 1: DrawHurryWarning (hurry-warning draw/mutate split)

**Starting source revision:** uncommitted, directly on top of the R1 slice 3
(`PlayerControls`) session above, same branch (`main`).

**Re-inventory before implementing.** R1d's own inventory this session found
that `bubblegame_render.cpp`'s `Render()` (~1600 lines) has several
independent draw/mutate crossovers beyond the one this session fixes:

- `RenderMalusAlerts()` ages/prunes `p.malusAlerts` inside the same loop that
draws them; the in-game chat overlay in `Render()` ages/prunes
`inGameChatMessages` right after drawing it (gated only on `networkGame`);
and the per-board attack-flash blink decrements
`curArray.attackFlashFramesLeft` inside the same
`if (useMini && curArray.boardVisible && attackFlashFramesLeft > 0)` block
that draws the blinking border (so a paged-out board's timer never ages).
All three fuse a countdown/toast draw with the age/prune step.
- `RenderRoundStats()` computes its `statsChatBtn`/`statsTournamentBtn`
hit-test rects inside the draw function; `HandleFinishedTap()` reads them
later, coupling tap correctness to whether the draw ran that frame.
- The systemic `Update*Text` recompute-and-blit pattern (`Update2PText`,
`UpdatePlayerNameWinText`, `UpdatePoppedText`, `UpdateScoreText`,
`DrawLiveBadges`, `RenderRoyaleHud`'s `cell` lambda, and the many inline
"build string, `UpdateText`, immediately `SDL_RenderTexture`" blocks —
mpTrainText, comboText, finalScoreText, targetingText,
chatInputText/chatLineText, modeTimerText, the disconnect "left" overlays,
etc.). These touch no core gameplay rules/RNG/network state, but bundling
texture regeneration with the blit is still not a pure draw. Largest item by
call-site count.

Because of this, **R1d itself is being split further — R1d-i through R1d-iv
— the same way R1 split into R1a-d; this session implements only R1d-i.** The
other three are described in `docs/REPLAY_PLAN.md`'s work-package table and
are not touched this session.

**Implementation (R1d-i):**
- `src/bubblegame.h`: `BubbleArray` gained `bool hurryWarnVisible = false;`
next to `hurryTexture`; `BubbleGame` gained the
`void DrawHurryWarning(SDL_Renderer *rend, BubbleArray &bArray);`
declaration next to `RenderMalusAlerts`.
- `src/bubblegame_shooter.cpp`: inside `ResolvePlayerControls()`, the two
inline hurry-warning draws (`SDL_RenderTexture(... bArray.hurryTexture ...)`)
were removed. The block now sets `bArray.hurryWarnVisible = (warnTimer <=
HURRY_WARN_FC / 2)` (resp. `HURRY_WARN_MP_FC / 2`) instead, so the mutator
still decides the cadence and still owns the `warnTimer`/`hurryTimer`
bookkeeping. `ResolvePlayerControls()` no longer touches `SDL_Renderer`.
- The flag is reset to `false` unconditionally at the top of
`ResolvePlayerControls()`, right after `bool isLocalPlayer = OwnsArray(bArray);`.
This is the one design decision worth recording: the old draw call simply
never ran for a non-local, dead, or `ModeAwaitingVerdict()` player, so those
frames left the previous frame's drawing alone; the new flag must default to
false every call so a player who stops qualifying (dies, round ends, page
changes) does not keep blitting a stale `true`. The whole hurry block remains
gated on `isLocalPlayer && playerState == ALIVE && !ModeAwaitingVerdict()`.
- `src/bubblegame_render.cpp`: new pure `DrawHurryWarning()` blits the texture
only when `hurryWarnVisible` is true and touches no gameplay state; added
above `RenderMalusAlerts()`. Called from `Render()` immediately after each of
the two `UpdatePenguin(curArray, deltaScale);` call sites (solo branch and the
multiplayer loop branch), preserving the exact old draw order (the old inline
draw happened inside `ResolvePlayerControls`, which `UpdatePenguin` runs before
`ApplyPlayerControls`). The multiplayer call is placed unconditionally
*outside/before* the following `if (curArray.boardVisible)` block, matching the
old code, which never gated the hurry draw on `boardVisible`.
- `PlaySFX("hurry")` **deliberately stays in the mutator**. The texture blit
is idempotent and can safely be repeated by a redundant render call, but the
sound is a one-shot event that must fire exactly once per `warnTimer` cycle
(guarded by `warnTimer == 0`); a replay/extra render path must not replay it.
This is why the draw moved and the SFX did not.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel      # clean build, no new warnings from this change
ctest --test-dir build --output-on-failure
```

Full suite: 38 tests total, 36 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` — the same pre-existing environmental skips as
slices 1-3, unrelated to this change). No test file changed; `grep -rn
"hurryWarnVisible\|DrawHurryWarning" tests/` had no pre-existing references,
and this change alters no value the existing `ResolvePlayerControls`/
`ApplyPlayerControls` tests assert on, only where the SDL draw call happens.

**Manual/platform checks: completed for the solo/classic path.** Same
session as the R1c manual check above, same freshly built binary. User
report: "I was able to see the hurry flash once before it auto shot" — the
warning blinked and the round auto-fired at `TIME_HURRY_MAX`, matching pre-
refactor behavior. **Not yet covered: an MP/mp_train round**, where the
identical `hurryWarnVisible`/`PlaySFX` block also changed
(`HURRY_WARN_MP_FC`/`TIME_HURRY_WARN_MP`/`TIME_HURRY_MAX_MP`) — still
outstanding, not skipped; low risk since it's the same code pattern already
confirmed in solo, but not yet exercised.

**Known failures/blockers:** none. Build is clean, full test suite passes,
solo manual check passed. **R1d-i is complete for solo/classic; MP/mp_train
coverage remains outstanding.**

**Design changes and reasons:** the `hurryWarnVisible` default-false-every-call
placement (so a stale `true` cannot linger after a player stops qualifying) and
leaving `PlaySFX("hurry")` in the mutator (a repeated render must not replay a
one-shot sound, unlike a redundant identical texture blit). Both explained in
full above.

**Next concrete step:** R1d-ii (the countdown/toast draw-then-age triplet).
The MP/mp_train hurry-warning check can ride along with whichever future
session next exercises local/network multiplayer (R1d-ii touches
`RenderMalusAlerts()`, which is also multiplayer-only, so that session is a
natural place to cover both at once).

### 2026-09-20 — R1d-ii: countdown/toast draw-then-age triplet

**Starting source revision:** uncommitted, directly on top of the R1d
sub-slice 1 (`DrawHurryWarning`) session above, same branch (`main`).

**Re-inventory before implementing.** Re-read `src/bubblegame_render.cpp`
directly rather than trusting the plan's line numbers. All three crossovers
matched the plan's description exactly, with no extra coupling found:

- `RenderMalusAlerts()` drew each player's `malusAlerts` toasts in a loop,
  then ran a second loop over the same players decrementing `framesLeft` and
  erasing expired entries, all inside the one function.
- The in-game chat overlay in `Render()` drew `inGameChatMessages` inside
  `if (currentSettings.networkGame) { if (!inGameChatMessages.empty() ||
  chattingMode) { ...draw... } ...age/prune... }` -- the age/prune step sits
  after the inner draw's closing brace but is still gated only on the outer
  `networkGame` check, so it always runs regardless of whether the inner draw
  block executed that frame (i.e. even when there was nothing to show and
  `chattingMode` was false).
- The per-board attack-flash blink decremented
  `curArray.attackFlashFramesLeft` as the last statement inside
  `if (useMini && curArray.boardVisible && curArray.attackFlashFramesLeft >
  0) { ...draw blinking border... curArray.attackFlashFramesLeft--; }` in the
  multiplayer per-player loop in `Render()`.

Unlike R1d-i's `DrawHurryWarning`, none of the three needed a new boolean
latch (`hurryWarnVisible`'s pattern): each already carries its own
persistent per-item countdown state (`MalusAlert::framesLeft`,
`InGameChatMsg::framesLeft`, `BubbleArray::attackFlashFramesLeft`) that a
pure draw can read directly. The split only needed pulling the *aging loop*
itself into a separate function, not adding new state.

**Implementation:**
- `src/bubblegame.h`: added four new private method declarations next to the
  existing `RenderMalusAlerts`/`DrawHurryWarning` declarations --
  `AgeMalusAlerts()`, `AgeChatMessages()`, `DrawAttackFlash(SDL_Renderer*,
  BubbleArray&, bool useMini)`, `AgeAttackFlash(BubbleArray&, bool useMini)`.
- `src/bubblegame_render.cpp`:
  - `RenderMalusAlerts()` is now a pure draw only (its trailing age/prune
    loop was removed); the new `AgeMalusAlerts()` ages and prunes every
    player's `malusAlerts`. The `Render()` call site
    (`if (!gameFinish) RenderMalusAlerts(rend);`) became
    `if (!gameFinish) { RenderMalusAlerts(rend); AgeMalusAlerts(); }` --
    draw-before-age order preserved exactly, so the frame an alert's alpha
    fades to its last visible value is still the same frame that then
    erases it, matching the original fused behavior.
  - The chat overlay's trailing age/prune loop in `Render()` became a new
    `AgeChatMessages()` method (defined just above `Render()`); the call
    site is now a single `AgeChatMessages();` statement in the same place,
    still inside `if (currentSettings.networkGame)` and still unconditional
    on whether the draw block above it ran that frame -- gated only on
    `networkGame`, exactly as before.
  - The attack-flash block became `DrawAttackFlash(SDL_Renderer*,
    BubbleArray&, bool useMini)` (pure draw) and `AgeAttackFlash(BubbleArray&,
    bool useMini)` (mutator), both reusing the exact same three-way gate
    (`useMini && bArray.boardVisible && attackFlashFramesLeft > 0`) the
    original fused block used. The paged-out-board quirk this row already
    documented -- a board's flash timer does not age while paged out /
    `boardVisible` is false -- is therefore preserved exactly, not fixed, per
    the plan's explicit instruction. Call site replaced with
    `DrawAttackFlash(rend, curArray, useMini); AgeAttackFlash(curArray,
    useMini);` in the same order (draw then age) as the original.
- `tests/statspanelcell_cache_test.cpp`: the existing malus-alert end-to-end
  test called `RenderMalusAlerts()` twice and asserted
  `p0.malusAlerts.size() == 1` with a comment claiming "framesLeft aged by 1,
  not yet pruned" -- true before this split (aging was fused into the draw),
  trivially true but no longer meaningfully checking anything after it (the
  size stays 1 forever now since nothing ages it without a call to
  `AgeMalusAlerts()`). Strengthened rather than left stale: added a
  `BubbleGameTestAccess::ageMalusAlerts()` accessor and two explicit
  `framesLeft` assertions -- two `RenderMalusAlerts()` calls leave
  `framesLeft` at its original 50 (proving the pure draw does not age),
  then one `AgeMalusAlerts()` call brings it to 49 (proving the mutator is
  the one that actually does). This is the test-side version of the plan's
  completion evidence ("calling the new draw function twice in a row must
  not double-age anything").
- No other test file references any of `RenderMalusAlerts`,
  `AgeMalusAlerts`, `AgeChatMessages`, `DrawAttackFlash`, or
  `AgeAttackFlash` (confirmed by grep across `tests/`).
  `tests/bubblegame_rules_test.cpp`'s existing attack-flash regression test
  drives the real, public `game.Render()` end-to-end and asserts
  `attackFlashFramesLeft` drops by exactly 1 after one call -- unaffected by
  this split since `Render()` still calls the new draw-then-age pair in the
  same order every frame it did before, and this test already passed
  unmodified.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel      # clean build, no new warnings from this change
ctest --test-dir build --output-on-failure
```

Full suite: 38 tests total, 36 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` -- the same pre-existing environmental skips as every
prior R1 session, unrelated to this change).

**Manual/platform checks: not performed this session -- no screen access.**
Malus alerts and the in-game chat overlay are multiplayer-only visuals (they
never appear in a solo round), and the attack-flash blink likewise only fires
from `SendMalusToOpponent`, a multiplayer-only path -- so exercising all
three needs a network or local-multiplayer game with at least one opponent
sending malus/chat, not a solo round. This is explicitly outstanding, the
same as the MP/mp_train hurry-warning check R1d-i deferred; a future session
driving live local/network multiplayer is a natural place to cover both at
once.

**Known failures/blockers:** none. Build is clean, full test suite passes,
the strengthened malus-alert test passes and now actually exercises the
draw/age split instead of trivially passing either way.

**Design changes and reasons:** none of the three items needed a new
boolean-latch flag the way `DrawHurryWarning`/`hurryWarnVisible` did in
R1d-i, because each already had its own per-item countdown state a pure draw
could read directly -- explained in full above. The paged-out-board
attack-flash quirk was intentionally preserved, not fixed, per the plan's own
instruction on this row.

**Next concrete step:** R1d-iii (`RenderRoundStats()`'s `statsChatBtn`/
`statsTournamentBtn` hit-test rects). The live multiplayer manual check for
R1d-i's MP/mp_train hurry warning and this session's malus/chat/attack-flash
trio can all ride along with whichever future session next exercises a real
local/network multiplayer game.

### 2026-09-20 — R1d-iii: RenderRoundStats() hit-test rects

**Starting source revision:** uncommitted, directly on top of the R1d-ii
(countdown/toast draw-then-age triplet) session above, same branch (`main`).

**Re-inventory before implementing.** Re-read `RenderRoundStats()`
(`src/bubblegame_render.cpp`), `HandleFinishedTap()`
(`src/bubblegame_input.cpp:39-54`), and the two `statsChatBtn`/
`statsTournamentBtn` declarations in `src/bubblegame.h` rather than trusting
the plan's line numbers; all three matched the plan's description exactly.
Confirmed the key claim the plan made about this row: every input feeding
`boxH`/`statsChatBtn`/`statsTournamentBtn` geometry is pure state --
`currentSettings.playerCount`, `currentSettings.playerTeams[]`,
`currentSettings.networkGame`, and `IsTournamentRound()` (a trivial
`return tournamentRound;` accessor in `bubblegame_state.cpp`). No texture
size, font metric, or other draw-time-only value feeds in, so the split is
safe. (The draw-only concerns that do read texture metrics -- the platform/
input badges' `PlayerBadgeChipWidth(t.Coords()->w)` and the cell `TTFText`
pool -- are all separate from the button geometry and untouched here.)

**Implemented:**
- New file-scope `RoundStatsLayout` + `ComputeRoundStatsLayout(const
  SetupSettings&, bool tournament)` in an anonymous namespace in
  `src/bubblegame_render.cpp`, holding `boxW/boxX/boxY/boxH`, `rowH/headH`,
  `tournament`, `discordAlertsApply`, and the sorted distinct `teams` vector.
  This is now the single source of truth for the panel's geometry; both call
  sites below go through it, so the draw and the tap targets cannot drift.
- `src/bubblegame.h`: declared `void UpdateRoundStatsHitRects();` next to
  `RenderRoundStats()`.
- `src/bubblegame_render.cpp`: `RenderRoundStats()` now gets its
  `boxW/boxX/boxY/boxH`/`rowH/headH`/`teams`/`tournament`/
  `discordAlertsApply` from the shared helper and reads the already-current
  `statsChatBtn`/`statsTournamentBtn` members purely for drawing -- it no
  longer assigns them inline. New `UpdateRoundStatsHitRects()` computes them
  from the same helper. Both preserve the existing quirks exactly: the
  `playerCount < 2` early return leaves the rects untouched (not zeroed), and
  `!networkGame` zeroes both.
- `Render()`'s call site became
  `if (gameFinish) { UpdateRoundStatsHitRects(); RenderRoundStats(rend); }`,
  so the hit rects are computed as a separate statement from the draw (the
  R1d-ii draw-then-mutate call-site shape) and stay current even if the draw
  path never runs. `HandleFinishedTap()` is unchanged -- it keeps reading the
  same two members.

**Design choice (step 2's fork):** went with the "both functions call only
the shared geometry helper and `RenderRoundStats()` reads the already-current
member rects" option rather than having `RenderRoundStats()` call
`UpdateRoundStatsHitRects()` itself. This makes the draw a genuine pure
consumer of hit-test state -- it never writes it -- matching the R1d-ii
pure-draw/separate-mutator split most closely, and the `Render()` call site
owns the single write per frame. The only cost is that a direct
`RenderRoundStats()` call without a preceding `UpdateRoundStatsHitRects()`
draws the buttons at their zeroed rect (invisible); there is no production
caller like that (the one call site always computes first), and the new test
asserts the separation property directly rather than papering over it.

**Tests:** `tests/statspanelcell_cache_test.cpp` gained a
`BubbleGameTestAccess` accessor set (`updateRoundStatsHitRects`,
`statsChatBtn`, `statsTournamentBtn`, plus `tournamentRound` for the private
flag) and four new blocks:
- network, 2 players, non-tournament: call **only**
  `UpdateRoundStatsHitRects()` and assert the hand-computed rect
  `{48, 118, 88, 24}` and a zeroed BRACKET; a separate "draw-only" instance
  that calls just `RenderRoundStats()` is asserted to leave both rects
  zeroed, proving the draw no longer computes them.
- network, 2 players, tournament: asserts `{48, 102, 88, 24}` and BRACKET
  `{144, 102, 112, 24}`.
- non-network: asserts both rects are zeroed (poisoned first, to prove the
  zeroing overwrites rather than merely leaving defaults).
- `playerCount == 1`: asserts poisoned rects are left untouched, pinning the
  preserved early-return quirk.
- Confirmed via `grep -rn "statsChatBtn\|statsTournamentBtn\|
  HandleFinishedTap" tests/` that no other test referenced any of these; the
  existing `RenderRoundStats`-driving blocks are non-network and never
  asserted on the rects, and needed no change (they still pass unmodified).

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel      # clean build, no new warnings from this change
./build/statspanelcell-cache-test   # exit 0
ctest --test-dir build --output-on-failure
```

Full suite: 38 tests total, 36 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` -- the same pre-existing environmental skips as every
prior R1 session, unrelated to this change). The new coverage lives inside
the existing `statspanelcell-cache-test` executable, so the total count stays
38 rather than growing; its four new blocks run and pass.

**Manual/platform checks: not performed this session -- no screen access.**
The round-stats panel is multiplayer-only twice over: `RenderRoundStats()`
returns immediately when `currentSettings.playerCount < 2`, and the rects
`HandleFinishedTap()` consumes are only ever nonzero for a `networkGame` with
>= 2 players (the tap handler itself also requires `networkGame &&
gameFinish`). So a solo round cannot exercise this screen at all; it needs a
live local/network multiplayer round. This is the same outstanding check
class R1d-i (MP/mp_train hurry warning) and R1d-ii (malus alerts/chat/
attack-flash) are already waiting on, and can ride along with whichever
future session next plays a real multiplayer game. Structurally the change
is a pure refactor of already-pure-state geometry, so the risk of a visual
regression is low.

**Known failures/blockers:** none. Build is clean, full test suite passes,
the new test proves the standalone hit-rect computation matches the geometry
the draw path uses and that the draw no longer writes it.

**Design changes and reasons:** the step-2 fork choice above (draw reads,
separate method writes, shared helper for geometry) and the single-write
call-site placement. No deviation from the plan otherwise.

**Next concrete step:** R1d-iv (the systemic `Update*Text`
recompute-and-blit pattern). Its own row already warns it is the largest item
by call-site count and probably needs a further split by screen area (HUD
score/pop/timer vs. round-stats table vs. chat) rather than one pass over
`bubblegame_render.cpp`. The live multiplayer manual check for R1d-i's
MP/mp_train hurry warning, R1d-ii's malus/chat/attack-flash trio, and this
session's round-stats tap targets can all ride along with whichever future
session next exercises a real local/network multiplayer game.

### 2026-09-20 — R1d-iv (partial, first sub-slice): HUD score/pop/timer text split

**Starting source revision:** uncommitted, directly on top of the R1d-iii
(round-stats hit-test rects) session above, same branch (`main`), HEAD still
`61b00290ef264dbcca5135e9a50e4227d939cbad`. Changes below are **uncommitted**.

**Re-inventory before implementing.** Grepped and re-read
`src/bubblegame_render.cpp` against the task's six claimed findings rather than
trusting the prior note; all six held, with one important narrowing and one
plan-text inaccuracy (below). This session deliberately implements **only**
the `UpdateScoreText` + `UpdatePoppedText`/embedded-`modeTimerText` slice --
the "HUD score/pop/timer" screen area the R1d-iv row itself suggested
splitting by. It explicitly does **not** finish R1d-iv.

Full inventory of `Update*Text`-then-blit sites, current line numbers,
whether this session touched them, and their shape:

| Site (current line) | Shape | This session |
|---|---|---|
| `Update2PText()` (`:90`; `UpdateText` at `:93,:97`) | Recompute only; blit is a separate statement in `Render()` at `:1542-1543`, gated on `playerCount == 2`. Called only from state-mutation sites (`bubblegame_state.cpp:693`, `bubblegame_level.cpp:186,334`, `bubblegame.cpp:1107`), never from the per-frame draw path. | **Already split; no change** |
| `UpdatePlayerNameWinText()` (`:102`; `UpdateText` at `:138`) | Recompute only; per-player blits are separate statements in `Render()` at `:1552-1553` plus a `DrawLiveBadges()` call at `:1559`. Called once per frame in `Render()` inside the `playerCount >= 3` branch. | **Already split; no change** (its `DrawLiveBadges` call is genuinely fused, see below) |
| `UpdatePoppedText()` (`:148`; `UpdateText` at `:185` and `modeTimerText` at `:223`) | **Genuinely fused**: pop-count format+`UpdateColor/Text/Position` then immediate blit, then inline Timed-mode `modeTimerText` format+`UpdateColor/Text/Position` (positioned from `poppedText[idx].Coords()->h`) then immediate blit. Called once per visible player: `if (curArray.boardVisible) UpdatePoppedText(curArray, i);` (`:1364`). | **Split this session** |
| `UpdateScoreText()` (`:278`; `UpdateText` at `:301`) | **Genuinely fused**: score/nickname format+`UpdateText/Position`, then immediate blit, then `DrawLiveBadges()` at `:321` for the player's platform/input badges. Called from two `Render()` sites: solo/2P-vs-training `UpdateScoreText(curArray, 0);` (`:1237`) and per-player in the multiplayer loop (`:1357`). | **Split this session** |
| `DrawLiveBadges()` (`:250`; `UpdateText` at `:265`) | **Genuinely fused and structurally different**: loops up to 2 badges, and each iteration draws a chip sized from that badge's *own* freshly recomputed `Coords()->w`, then advances its returned x-cursor by that width so the next badge positions correctly. Clean separation needs a real two-pass restructuring (measure all, then draw all). | **Out of scope this session (deferred to R1d-iv-b)** |
| `RenderMalusAlerts()` alert text (`:511`) | Fused recompute+blit per alert. (Its *aging* was already split into `AgeMalusAlerts()` in R1d-ii; the text regeneration is a separate, still-unaddressed instance of the R1d-iv pattern.) | **Out of scope (deferred)** |
| `RenderRoyaleHud()` `cell` lambda (`:602`) | Fused: `UpdateColor/Text/Position` then `SDL_RenderTexture` in the same lambda. | **Out of scope (deferred)** |
| `RenderRoundStats()` `cell` lambda (`:738`) and `badges` lambda (`:770`) | Both fused recompute+blit; `badges` additionally sizes a chip from the just-measured width (same shape as `DrawLiveBadges`). Not named in the plan row, but the same pattern. | **Out of scope (deferred)** |
| `finalScoreText` (`:1177`, `:1189`) | Inline build-string+`UpdateText`+immediate blit in the solo win/lose branches. | **Out of scope (deferred)** |
| `mpTrainText` (`:1249`) | Inline build-string+`UpdateText`+immediate blit. | **Out of scope (deferred)** |
| `comboText` (`:1257`) | **Not** a recompute+blit site: the `UpdateText` is set once in the mutator (`bubblegame_board.cpp:428`); the render site is a pure blit followed by `comboDisplayTimer--` -- i.e. a *draw-then-age* crossover of the R1d-ii kind, not an `Update*Text` fusion. Worth a future mutator/draw split. | **Out of scope (deferred)** |
| disconnect "left" overlay (`:1399`) | **Plan text inaccuracy:** this is a pure sprite (`leftRp*Mini`/`leftRp1`) blit, no `TTFText` and no `UpdateText` at all. There is no "left-overlay text" to split. | **N/A; no such site** |
| `targetingText` (`:1455`) | Inline build-string+`UpdateText/Position`+immediate blit. | **Out of scope (deferred)** |
| `clearWinText` (`:1609`) | Inline build-string+`UpdateText/Position`+blit (plus a plate fill behind it). | **Out of scope (deferred)** |
| `chatInputText` (`:1663`) / `chatLineText` (`:1681`) | Inline build-string+`UpdateText/Position`+blit in the chat overlay. | **Out of scope (deferred)** |

**Key inventory correction (plan undersold):** `Update2PText()` and
`UpdatePlayerNameWinText()` already satisfied the recompute-vs-blit split
before this session -- they only `UpdateText`/`UpdatePosition`, and their
blits are already separate statements in `Render()` (with
`UpdatePlayerNameWinText()`'s call site additionally invoking the still-fused
`DrawLiveBadges()`). A literal reading of the R1d-iv row's name list would
have had a session spend time "splitting" two functions that were already
split. Recording it here so no future session re-does it. This narrows
R1d-iv's remaining scope to: `DrawLiveBadges`, `RenderRoyaleHud`, the two
`RenderRoundStats` lambdas, `RenderMalusAlerts`'s text, and the inline
`Render()` blocks (plus `comboText`'s draw-then-age, which is really R1d-ii's
pattern).

**Implementation (this session's slice):**
- `src/bubblegame_render.cpp`: `UpdatePoppedText()` is now a pure recompute --
  it formats the pop-count string/colour, calls
  `poppedText[idx].UpdateColor/UpdateText/UpdatePosition`, and, in Timed mode,
  computes and `UpdateColor/Text/Position`s `modeTimerText` (still positioned
  from the just-computed `poppedText[idx].Coords()->h`). No blits. New
  `DrawPoppedText(int idx)` blits `poppedText[idx]` and, under the identical
  `if (currentSettings.gameMode == GameMode::Timed)` guard, `modeTimerText`.
- `UpdateScoreText()` is now a pure recompute (format + `UpdateText` +
  `UpdatePosition` only). New `DrawScoreText(BubbleArray&, int slot)` blits
  `scoreText[slot]` and then calls `DrawLiveBadges()` with the exact same
  x/y math and `badgeIdx = slot * 2` threading the old fused function used.
- `src/bubblegame.h`: declared `DrawScoreText` beside `UpdateScoreText` and
  `DrawPoppedText` beside `UpdatePoppedText`.
- `Render()` call sites preserve exact order/conditions:
  `UpdateScoreText(curArray, 0); DrawScoreText(curArray, 0);` (solo branch),
  `UpdateScoreText(curArray, i); DrawScoreText(curArray, i);` (inside the
  existing `if (boardVisible && playerCount < 3)`), and
  `UpdatePoppedText(curArray, i); DrawPoppedText(i);` (inside the existing
  `if (boardVisible)`).

**Design choices and reasons:**
- `DrawPoppedText` takes only `int idx` (no `bArray`, and no renderer
  parameter -- it uses the member `renderer`, matching the old fused code),
  because after the recompute both the textures and positions are already
  current. `DrawScoreText` takes `BubbleArray&` only because the untouched
  `DrawLiveBadges` needs it.
- `DrawPoppedText` repeats `UpdatePoppedText`'s two early returns
  (`playerCount < 2`, `idx` range). The sole production call site is already
  inside `playerCount >= 2`/`boardVisible`, so these never fire there, but
  without them a direct draw call could blit a stale texture after a
  mutator that bailed out -- the guard preserves exact behavior and is
  covered by the new test.
- `DrawLiveBadges` is left exactly as-is (per the task's point 5) and merely
  invoked from the new draw function instead of the old fused one; its
  two-pass restructuring is the next sub-slice's job.
- This is a structural split only: no string, position, colour, gating or
  font changed.

**Tests:** `tests/statspanelcell_cache_test.cpp` gained accessors
(`updateScoreText`/`drawScoreText`/`scoreLabel`,
`updatePoppedText`/`drawPoppedText`/`poppedLabel`/`modeTimerLabel`) and two
new end-to-end blocks using the file's existing
`SDL_SetBooleanProperty`/`HasMarker` texture-identity technique:
- Score: recompute twice with unchanged state leaves the marker; a draw-only
  call (no preceding recompute) leaves the marker and the coords intact,
  proving the draw neither recomputes nor moves anything; a score change then
  regenerates on the next recompute.
- Popped/Timed: recompute leaves both `poppedText[0]` and `modeTimerText`
  textures marked; a second recompute and a draw-only call both preserve the
  markers; `playerCount = 1` makes the draw a no-op (guard preserved); a
  changed pop count regenerates.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel      # clean build, no new warnings from this change
./build/statspanelcell-cache-test   # exit 0
ctest --test-dir build --output-on-failure
```

Full suite: 38 tests total, 36 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` -- the same pre-existing environmental skips as every
prior R1 session, unrelated to this change). The new coverage lives inside
the existing `statspanelcell-cache-test`, so the total stays 38.

**Manual/platform checks: not performed this session -- no screen access.**
`UpdateScoreText`/`DrawScoreText` drive the score line in **solo** play too
(the `playerCount == 1` branch calls them), so a solo round is enough to
visually confirm the score HUD. `UpdatePoppedText`/`DrawPoppedText` and the
Timed-mode countdown are **multiplayer-only** (`UpdatePoppedText` returns at
`playerCount < 2`, and the call site lives in the `else`/multiplayer branch),
so those need a local-multiplayer or network round with at least two players
-- a Timed-mode round specifically to exercise the embedded `modeTimerText`
branch. This is the same outstanding manual-check class R1d-i (MP/mp_train
hurry warning), R1d-ii (malus/chat/attack-flash) and R1d-iii (round-stats tap
targets) are waiting on, and can ride along with whichever future session next
plays a real multiplayer game. Structurally this is a pure refactor of text
the draw path already computed identically, so visual-regression risk is low.

**Fraction of R1d-iv covered:** small -- two of the six functions the plan
names plus the `modeTimerText` block embedded in one of them, i.e. the "HUD
score/pop/timer" screen area. `DrawLiveBadges`, `RenderRoyaleHud`, both
`RenderRoundStats` lambdas, `RenderMalusAlerts`'s text, and every inline
`Render()` block listed above remain unsplit. R1d-iv is **not** complete and
should not be marked so.

**Known failures/blockers:** none. Build is clean, full test suite passes, and
the new tests prove the recompute/draw separation directly.

**Next concrete step:** R1d-iv-b -- `DrawLiveBadges`'s two-pass restructuring
(measure the platform and input badge labels and their chip widths first, then
draw all of them using the precomputed widths, so the recompute and the blit
become separable). That is the highest-value next unit because
`UpdatePlayerNameWinText()`'s and `UpdateScoreText()`'s call sites both depend
on it, and it is the one remaining site whose fused shape is structurally
non-trivial rather than mechanical. After it, the mechanical long tail
(`RenderRoyaleHud`/`RenderRoundStats` cell lambdas, `RenderMalusAlerts` alert
text, and the inline `Render()` blocks) can be grouped by screen area into
further sub-slices; `comboText`'s draw-then-age should be folded into an
R1d-ii-style follow-up rather than this one.

### 2026-09-20 — R1d-iv complete + AdvanceSimulation() boundary + SessionMode effects gating

**Starting source revision:** uncommitted, directly on top of the R1d-iv first
sub-slice session above, same branch (`main`), HEAD still
`61b00290ef264dbcca5135e9a50e4227d939cbad`. Changes below are **uncommitted**.

**What this session finished.** The remaining R1d-iv text/cache recompute-vs-blit
sites, the overall `AdvanceSimulation()`/`Draw()` behavioral boundary, and the
`SessionMode::Live/Playback` effects gate. Every seam and boundary named in
R1's plan row (`GameplayRng`, `StepContext`, `PlayerControls`, the
`AdvanceSimulation()`/`Draw()` split, and `SessionMode::Live/Playback`) is now
implemented and passing its tests. Whether that means R1 as a whole is ready to
close out is a judgment call for whoever is coordinating this project to make,
not a determination this session makes for them -- especially since the live
multiplayer visual checks listed below (and carried over from R1d-i/ii/iii)
have still never actually been run.

**R1d-iv restructure (recompute separated from blit, layout/caching/order preserved):**

- `DrawLiveBadges` is now a two-pass split: `MeasureLiveBadges(bArray, poolIdx)`
  recomputes each badge's label texture and records its chip width + badge cell
  index into a new `liveBadgeCells` cache; the pure `DrawLiveBadges(x, y)` reads
  that cache and only draws. `UpdateScoreText` now calls `MeasureLiveBadges`;
  `DrawScoreText` calls `DrawLiveBadges`. The 3-5-player name loop measures then
  blits per visible player, same pool-index order as before.
- `RenderMalusAlerts`/`RenderRoyaleHud`/`RenderRoundStats` are each now a thin
  combined wrapper (kept for back-compat with the existing
  `statspanelcell-cache-test`) over new `Update*` (recompute) + `Draw*` (pure
  blit) pairs:
  - `UpdateMalusAlerts` formats/positions each toast and records
    `{poolIdx, rect, alpha}` in `malusAlertDrawOps`; `DrawMalusAlerts` blits the
    recorded ops. (Aging was already split into `AgeMalusAlerts` in R1d-ii.)
  - `UpdateRoyaleHud` recomputes the alive/page/spectating cells; `DrawRoyaleHud`
    blits `royaleHudCellCount` cells.
  - `UpdateRoundStats` recomputes every stats-table cell and badge and records
    the exact draw order into `roundStatsOps` (Text/Chip/ChatBtn/BracketBtn ops);
    `DrawRoundStats` draws the panel background then replays those ops. This is
    the R1d-iii-style shared-geometry idea applied to the whole panel: the two
    functions cannot drift because the draw is a replay of the recompute's op
    list.
- Inline `Render()` text blocks (mpTrainText, finalScoreText, targetingText,
  chatInputText/chatLineText, clearWinText) are now recompute-then-blit as two
  separate statements in `Draw()`, not a fused expression.
- `comboText`'s draw-then-age crossover got a `comboTextVisible` latch:
  `AdvanceSimulation()` records the pre-decrement visibility and ages the timer
  once per step; `Draw()` only blits the latch. The final visible frame is
  preserved exactly (same pattern as `hurryWarnVisible` in R1d-i).

**AdvanceSimulation()/Draw() boundary.** `Render()` was split into

- `bool AdvanceSimulation()`: every gameplay/timer/animation/input/RNG/network/
  text-aging mutator, in the same relative order the old fused `Render()` ran it
  (network pump -> frameCount -> race/timed -> mp_train -> pause resume ->
  FinalizeRoundStats -> per-mode physics/animation/penguin -> transient aging).
  Returns false for the two old early-return paths (tournament return and
  round-sync wait), which must present only the background.
- `void Draw()`: background + every blit and text recompute. No gameplay state,
  no RNG, no network, no aging.
- `void Render()` = `if (AdvanceSimulation()) Draw(); else blit background;`.

`deltaScale` is still read once from the live singleton, now in
`AdvanceSimulation()`, and stored in a new `stepDeltaScale` member that `Draw()`'s
aim-guide preview reads (so a future playback path can substitute the recorded
value without touching Draw).

**Deliberate presentation nuance (age-before-draw for three transients).** The
clean `AdvanceSimulation(); Draw();` split moves `AgeMalusAlerts`,
`AgeChatMessages` and `AgeAttackFlash` (plus `comboDisplayTimer`'s decrement)
into the advance phase, so those three toasts/blinks now age one frame before
they are drawn rather than drawing then aging in the same statement. This means a
fading toast/chat line/blink disappears ~16ms earlier than the R1d-ii entry's
literal "draw then age" order; the in-memory timers and gameplay are unaffected.
The R1d-ii function-level split is still intact (`Draw*` never ages). Recorded
here rather than silently: the alternative -- keeping those three ages inside
`Draw()` -- would have made an extra `Draw()` advance text-aging state, exactly
what the R1 boundary forbids.

**SessionMode::Live/Playback effects boundary.** New
`BubbleGame::SessionMode { Live, Playback }`, default Live, with
`GetSessionMode/SetSessionMode/EffectsEnabled`. Playback keeps every in-memory
score/stat/win (so a recorded result still displays) but suppresses:
`SubmitScore` highscore/campaign writes, mp_train's `CheckAndAddScore`,
`ReportTournamentResult`, `FinalizeRoundStats`'s 'S' broadcast (via
`SendGameDataFor`), `SendLobbyMatchSummary`, the opt-in `sendGameStats`
telemetry, in-game chat send, malus 'm'/'A' sends, the per-frame ping, and the
mid-round colorblind `settings.ini` write. `SendGameDataFor` is the central gate
(Playback returns false before touching NetworkClient); the few direct
`netClient->SendGameData` sites were gated individually. `QuitToTitle`'s PART +
lobby summary are also gated, so a Playback instance never speaks to a server.
Live behavior is unchanged.

**Tests.** New `tests/replay_session_test.cpp` (registered as
`replay-session-test` in `CMakeLists.txt`):
- Repeated `Draw()` (3x) leaves simStep, hurryTimer, gameplay RNG state, a
  falling bubble's posY and comboDisplayTimer untouched; two `AdvanceSimulation()`
  calls advance simStep and hurryTimer by exactly one per call.
- `comboText` latch: draw does not age; advance ages once and keeps the final
  frame visible.
- `SendGameDataFor` in Live reaches `NetworkClient::SendGameData` (new
  `testGameDataSendCount` counter under `FROZEN_BUBBLE_TEST_ACCESS`), Playback
  stops before it; `FinalizeRoundStats` in Playback still rolls r* into m* but
  sends nothing; `SubmitScore` in Playback keeps the score and leaves
  `pendingHighscore` false.
Existing `statspanelcell-cache-test` kept passing unchanged (the `Render*`
wrappers preserve its call sites), and its `drawScoreText` accessor was updated
for the new `DrawScoreText(int slot)` signature.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel      # clean, no new warnings
./build/replay-session-test         # exit 0
./build/statspanelcell-cache-test   # exit 0
./build/bubblegame-rules-test       # exit 0
./build/bot-play-test               # exit 0
./build/gameplay-rng-test           # exit 0
ctest --test-dir build --output-on-failure
```

39 tests total, 37 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` -- the same pre-existing environmental skips as every
prior R1 session). `git diff --check` clean.

**Manual/platform checks: not performed this session -- no screen access.** The
same outstanding multiplayer visual checks remain from R1d-i/ii/iii (MP/mp_train
hurry warning, malus alerts/chat overlay/attack-flash, round-stats CHAT/BRACKET
tap targets), now joined by the split's visual output. A future session that
plays a real local/network multiplayer round can cover all of them at once.

**Known failures/blockers:** none. Build clean, full suite passes.

**Design changes and reasons:** `DrawLiveBadges`/`RenderRoundStats` etc. use a
measure-then-replay draw-op cache so the recompute and the blit cannot drift;
text recompute stays in the presentation phase as a separate statement from the
blit (it is not gameplay state, per the R1d acceptance list); the three transient
ages moved to `AdvanceSimulation()` with the one-frame nuance above;
`testGameDataSendCount` added to NetworkClient so the suppression test is
non-vacuous. No plan-level correction required -- these are all consequences of
the already-planned boundary, not a change to it.

**Next package:** R2 (versioned snapshot/record codec and validation).

### 2026-09-20 — R2: versioned snapshot/record codec and validation

**Starting source revision:** uncommitted, directly on top of the R1d-iv
complete + `AdvanceSimulation()`/`SessionMode` session above, same branch
(`main`), HEAD still `61b00290ef264dbcca5135e9a50e4227d939cbad`. Changes below
are **uncommitted**.

**Re-inventory before implementing.** Re-read `docs/REPLAY_PLAN.md`'s
Recording-contract table and R2 row, `docs/REPLAY_RESEARCH.md`'s rationale
for a versioned explicit codec, `src/gameplay_rng.h`/`src/player_controls.h`
as the standalone-header pattern, `tests/gameplay_rng_test.cpp` as the
lightweight-test pattern, and `cmake/CoreSources.cmake` plus the
`gameplay-rng-test` block in `CMakeLists.txt` as the registration templates,
rather than trusting the task prompt's own file/line references. Confirmed
directly: no existing hash utility anywhere under `src/`
(`grep -rli "fnv\|hash" src/` found nothing); `MAX_NET_PLAYERS = 20` at
`src/bubblegame.h:275`; the `gameplay-rng-test` block at
`CMakeLists.txt:326-329` is exactly `add_executable` +
`target_include_directories(... PRIVATE src)` +
`target_compile_features(... PRIVATE cxx_std_17)` + `add_test`, with no link
libraries at all (it has none of its own dependencies) -- the same shape this
session's `replay-format-test` needed, plus one extra source file
(`src/replay_format.cpp`) on the `add_executable` line since the codec isn't
header-only the way `GameplayRng` is.

**No further split needed.** The package inventoried at exactly the size the
plan's R2 row described -- one header, one implementation file, one test file,
two build-list registrations -- with no surprise breadth the way R1 and R1d
each turned out to need. Implemented as one slice.

**Implemented:**

- **`src/replay_format.h`** (new). No SDL/game includes, same standalone
  pattern as `src/gameplay_rng.h`/`src/player_controls.h`. Declares:
  `kReplayMagic` (`"FBR1"`), `kCurrentFormatVersion`/`kCurrentSimRulesVersion`
  (both `1`, independent fields per the plan's explicit "container format and
  gameplay rules can version independently"), `kMaxPayloadLength` (1 MiB),
  `kMaxPlayers = 20` (a local copy of `MAX_NET_PLAYERS` with a comment
  pointing at `src/bubblegame.h:275` -- the same no-engine-dependency
  tradeoff `gameplay_rng.h` already accepted for its own constants),
  `enum class RecordType : uint8_t { RoundStart=1, Step=2, Assertion=3,
  RoundEnd=4 }` (a `// Checkpoint = 5` comment reserves the value, matching
  the plan's explicit deferral of checkpoints to R8 -- not implemented),
  `enum class DecodeResult { Ok, Truncated, BadMagic, UnsupportedVersion,
  RecordTooLarge, BadRecordType }`, the five structs
  (`ReplayHeader`, `RoundStartRecord`, `StepRecord`, `AssertionRecord`,
  `RoundEndRecord`, all primitives/fixed arrays/`std::vector<uint8_t>`, no
  pointers), and the `ReplayWriter`/`ReplayReader` class declarations.
- **`src/replay_format.cpp`** (new). Little-endian
  WriteU8/16/32/64/Float/Bytes-equivalent free helpers (`AppendU8` etc. on
  the writer side; the task's named set plus `AppendU64`/`AppendI32` since a
  64-bit hash/level-hash field and several signed fields needed them) and
  matching bounds-checked ReadU8/16/32/64/Float/Bytes free helpers on the
  reader side, each checking remaining-buffer length against `size` before
  touching `data[pos]` and never advancing `pos` past `size`. Record framing
  is `[uint8_t recordType][uint32_t payloadLength][payloadLength bytes]`;
  `ReadRecordFrame()` checks `payloadLength` against `kMaxPayloadLength` and
  then against the actual remaining buffer length, both before any field of
  the payload is touched. `ReadBytes()` (used for every nested
  length-prefixed blob -- build fingerprint, level layout, each per-player
  starting-board blob, inbound-events blob) applies the identical
  cap-then-remaining check, since a nested field can lie about its length
  exactly as a top-level record can. `CanonicalHashFnv1a64()` is a
  self-contained FNV-1a 64-bit hash (offset basis
  `14695981039346656037`, prime `1099511628211`) over an explicitly-passed
  byte span -- callers are expected to hash bytes they already serialized in
  stable field order, never a struct's raw memory. Encode/decode functions
  for all five types follow the header struct's declared field order exactly.
- **`tests/replay_format_test.cpp`** (new, `replay-format-test` target).
  Deliberately SDL-free -- no `SDL_Init`/window/renderer, confirmed by grep
  after writing it. Covers: round-trip equality (hand-constructed values,
  field-by-field comparison functions written in the test file itself rather
  than `operator==` on the structs, so a future field addition forces this
  file to be touched deliberately) for all five structs individually, plus a
  composed header+RoundStart+Step+Assertion+RoundEnd stream decoded
  sequentially via `PeekRecordType()`; five pinned literal byte arrays
  (`kHeaderBytes`, `kRoundStartBytes`, `kStepBytes`, `kAssertionBytes`,
  `kRoundEndBytes`), generated once from a standalone throwaway program (see
  "Byte-fixture generation" below) and compared against a fresh encode
  byte-for-byte -- not regenerated from the struct at test time, so a
  field-order/width/padding regression that still happens to round-trip
  correctly is still caught; deterministic-hash checks (same bytes twice,
  two independently-built-but-equal records, a top-level field flip, and a
  flip inside a nested per-player `startingBoards[]` blob, each confirmed to
  either match or diverge as expected); version rejection
  (`formatVersion = kCurrentFormatVersion + 1` -> `UnsupportedVersion`,
  plus a control case proving the current version still decodes); an
  exhaustive truncation sweep -- every single byte-length prefix of an
  encoded header+RoundStart stream from 0 up to (not including) the full
  stream, asserting each is reported `Truncated`/`BadMagic` at the header
  stage or `Truncated` at the record stage, never `Ok` -- plus the four named
  offsets (mid-header, right after the record's type byte, mid-length-field,
  mid-payload) as explicit, separately-labeled cases; an oversized-length
  case (`kMaxPayloadLength + 1` -> `RecordTooLarge`, checked before the
  remaining-length comparison) and an at-the-cap-but-past-the-buffer case
  (`kMaxPayloadLength` exactly, but the buffer doesn't actually hold that
  many bytes -> `Truncated`), proving the two checks are independently
  enforced, not that one subsumes the other; and malformed magic
  (`BadMagic`) plus bad record type bytes, including `0`, the
  reserved-but-unimplemented `5` (`Checkpoint`), and `0xFF`, all
  `BadRecordType`, plus a case where the type byte names a *different valid*
  record (a real `Step` record read via `ReadRoundStart()`) -- also
  `BadRecordType`, proving a wrong-but-valid type isn't silently
  reinterpreted.
- **`cmake/CoreSources.cmake`**: added `${FB_SRC}/replay_format.cpp` to
  `FROZEN_BUBBLE_CORE_SOURCES`, next to `highscoremanager.cpp` just above the
  network-client entry.
- **`CMakeLists.txt`**: registered `replay-format-test` immediately after the
  `gameplay-rng-test` block, same lightweight pattern (`add_executable`,
  `target_include_directories(... PRIVATE src)`,
  `target_compile_features(... PRIVATE cxx_std_17)`, `add_test`), with
  `src/replay_format.cpp` added directly to the `add_executable` line instead
  of linking `frozen-bubble-core-test` -- the codec has zero engine
  dependency, so pulling in the ~35-file core library for it would be pure
  waste (and would also pull in `FROZEN_BUBBLE_TEST_ACCESS`/SDL, which this
  test deliberately does not need).

**Byte-fixture generation (recorded so a future session doesn't need to
re-derive the method).** Wrote a throwaway `gen_fixtures.cpp` in the session
scratchpad directory that includes `replay_format.h`, builds each record with
the exact same hand-chosen values `MakeTestRoundStart()`/etc. use in the test
file, runs it through `ReplayWriter`, and prints the resulting bytes as a C++
`std::vector<uint8_t>` literal. Compiled and ran it standalone
(`clang++ -std=c++17 -I src gen_fixtures.cpp src/replay_format.cpp`), then
pasted its output directly into `tests/replay_format_test.cpp` as the five
`kXxxBytes` constants. This satisfies "do NOT regenerate the expected bytes
from the struct at test time" literally: the literals were generated once,
outside the test binary, and the test only ever compares a fresh encode
against the already-pinned literal.

**Design decisions and reasons:**

- **`kMaxPlayers` duplicated rather than including `bubblegame.h`.** Exactly
  the tradeoff the task asked for and `gameplay_rng.h` already established:
  a codec with zero engine dependency is worth a one-line duplicated
  constant plus a comment, not worth coupling `replay_format.h` to the whole
  game header.
- **Variable-shape fields (`levelLayout`, `startingBoards[i]`,
  `inboundEvents`) are opaque `std::vector<uint8_t>` blobs, not a real
  board/level schema.** R2's job is the generic envelope; R3 is what
  actually knows `BubbleArray`'s shape. Making these fields typed now would
  mean guessing R3's design before R3 has inventoried it -- the same mistake
  the plan already warns against for over-eager slicing.
- **`gameMode`/`outcome` are opaque small-enum bytes (`uint8_t`), not typed
  enums referencing game code.** Same reasoning: this package must not
  `#include` any gameplay enum, so the byte is carried as-is and R3 defines
  the mapping.
- **Field-by-field comparison functions instead of `operator==`.** Keeps
  `replay_format.h` free of comparison-operator boilerplate that only the
  test needs, and makes a future field addition to any of the five structs a
  forced, visible touch to the test file rather than a silent
  always-passing comparison.
- **`ReadRecordFrame()` returns `BadRecordType` both for an unrecognized type
  byte and for a recognized-but-wrong-for-this-call type byte** (e.g. calling
  `ReadRoundStart()` on a stream whose next record is actually a `Step`).
  Not two different error codes, since both cases mean "the caller cannot
  proceed treating this as the type it asked for" and `DecodeResult` has no
  narrower code for "right shape, wrong type" versus "not a type at all" --
  a caller that needs to tell them apart should use `PeekRecordType()` first,
  which does report the real type when it's valid.
- **`AppendU64`/`ReadU64` and `AppendI32`/`ReadI32` added beyond the task's
  named WriteU8/16/32/Float/Bytes set.** `levelHash`, `canonicalStateHash`
  and `finalStateHash` are `uint64_t`; `boardGeometryId`, `initialSimStep`,
  the score/wins arrays, etc. are signed 32-bit. Both compose from the same
  little-endian-byte-at-a-time pattern as the named helpers rather than
  introducing a different technique.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel        # clean build, no new warnings
./build/replay-format-test            # "replay format tests passed", exit 0
ctest --test-dir build --output-on-failure
```

40 tests total, 38 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` -- the same pre-existing environmental skips as every
prior session). `replay-format-test` is the one new test versus the 39-test
baseline the R1d-iv-complete session left; it and every previously-passing
test still pass.

**Sanitizer build and result:**

```
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan --target replay-format-test --parallel
ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --test-dir build-asan --output-on-failure -R replay-format-test
```

`replay-format-test` passes clean under `-fsanitize=address,undefined` --
`1/1 Test #4: replay-format-test ... Passed`. This is the actual proof the
truncation/oversized-length/malformed-input paths do not read out of bounds,
not just that they return the right `DecodeResult` by luck (the exhaustive
per-offset truncation sweep in the test makes this a meaningfully broad
check, not just the four named cases).

**macOS leak-check limitation (reported per `CLAUDE.md`'s own instruction to
report this accurately).** The task's literal suggested command,
`ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0`, makes **every** ASan
binary on this session's platform (macOS/arm64, Darwin 25.6.0) abort
immediately with `AddressSanitizer: detect_leaks is not supported on this
platform` (exit 134) before running a single test -- confirmed not specific
to this new code by reproducing the identical abort against the pre-existing
`gameplay-rng-test` binary built the same way. AddressSanitizer's leak
detector is a Linux-only feature; it was never available on macOS. The
verification run above therefore omits `detect_leaks=1` (there is nothing to
disable that was ever enabled on this platform) and is otherwise the exact
command `CLAUDE.md` specifies. This is an environment limitation, not a gap
in this session's testing -- the same class of caveat `CLAUDE.md` already
flags for the two server tests' `SKIP_RETURN_CODE 77` behavior, just on the
leak-checking axis instead of the sanitizer-availability axis. A Linux CI
sanitizer job (`.github/workflows/build.yml`'s "Linux ASan/UBSan sanitizer
job") would exercise the leak-detection half of this that macOS cannot.

**Manual/platform checks: not applicable.** This package adds no UI, no
gameplay wiring and nothing reachable from a running game (`replay_format.h`
is not `#include`d anywhere outside its own `.cpp` and test file, confirmed
by grep) -- there is nothing to play-test. The R1 live-multiplayer visual
checks carried over from prior sessions remain exactly as outstanding as the
R1d-iv-complete entry above left them; this session did not touch any file
those checks depend on.

**Known failures/blockers:** none. Ordinary build and full suite pass;
sanitizer build of the new test passes clean (leak detection specifically
unavailable on this platform, not a failure -- see above).

**Scope discipline confirmed by grep before finishing:** no edit touched
`bubblegame.h`/`bubblegame*.cpp`, no existing test file, no replay UI, no
disk-writing file I/O, and no `Checkpoint` record implementation. This
package captures zero real `BubbleGame`/`BubbleArray` state -- every test
value is hand-constructed, never read from a live game instance -- so it
does not by itself demonstrate that recording or replaying an actual round
works. That is R3's entire job, not started here.

**Next concrete step:** R3 -- one solo Classic round recorded and replayed
offline (proposed `bubblegame_replay.cpp`, an in-memory integration fixture
plus a disk writer/player). This is the package that first wires real
`BubbleGame`/`BubbleArray` state into `RoundStartRecord`/`StepRecord`/
`AssertionRecord`/`RoundEndRecord`, using R1's seams (`simStep`,
`stepDeltaScale`, `GameplayRng::State()`, `BubbleArray::lastControls`,
`SessionMode`) as the capture hooks and R2's codec to serialize them.

### 2026-09-20 — R3: one solo Classic round recorded and replayed offline

**Starting source revision:** uncommitted, directly on top of the R2
(versioned codec) session above, same branch (`main`), HEAD still
`61b00290ef264dbcca5135e9a50e4227d939cbad`. Changes below are **uncommitted**.

**Re-inventory before implementing.** Grepped and re-read the real source
rather than trusting the task prompt's file/line references. The prompt's six
findings all held, with two corrections that shaped the implementation:

- `GameMode`/`BubbleArray`/`SetupSettings` matched the described shape; board
  rows are fixed-length `std::vector<Bubble>` cells addressed by column (not
  "multiple bubbles per cell" as the prompt hedged), and `bubbleMap` never
grows beyond `RandomLevel`/`LoadLevel`'s 13 rows. Capture stores per-row cell
  count + `bubbleId`/`playerBubble`/`frozen` in vector order.
- **`singleBubbles`/`malusBubbles` are process-global** (`bubblegame_internal.h`),
  not per-`BubbleGame` members. Live and replay instances therefore cannot run
  side by side without clobbering each other's projectiles. The test runs them
  **sequentially** (capture the whole live round, then replay), which also
  matches the plan's own "independently start ... then replay" wording.
- **`ResolvePlayerControls()` advances derive-only input state** (hurryTimer,
  warnTimer, and it clears mouseFirePending/suppressFireUntilRelease) that
  `ApplyPlayerControls()` never consumes for physics. Skipping Resolve on
  playback (the intended R1c design) means those values are not reproduced,
  which is fine for physics but rules them out of the canonical hash. This was
  pinned by an actual test failure -- see "Surprise" below.
- **`AdvanceSimulation()` factored cleanly** the way the prompt's finding 2
  anticipated: the whole body after the singleton read became a private
  `AdvanceSimulationAtScale(float)`, called by both `AdvanceSimulation()` and
  the new `AdvancePlaybackStep(float)`. Live behavior/call sequence unchanged.

No further split was needed: the package inventoried at one reviewable slice.

**Implemented:**

- **`src/bubblegame_replay.h/.cpp`** (new). Free functions
  `CaptureRoundStart` / `RestoreRoundStart` / `CaptureStep` / `CaptureRoundEnd`
  / `CaptureCanonicalStateHash`, plus a local, bounds-checked board-blob
  encoder/decoder and little-endian append helpers mirroring
  `replay_format.cpp`'s style. Private access is via a
  `friend struct BubbleGameReplayAccess` declared in `bubblegame.h` and defined
  in the `.cpp` (the module ships in the game too, so the test-only
  `BubbleGameTestAccess` cannot be used).
- **Board blob = `RoundStartRecord::startingBoards[0]`**, chosen over
  `levelLayout` because it is semantically the per-player board. Format:
  `version u8`, `flags u8` (chainReaction/randomLevels), `numColors u8`,
  `numRows u8`, geometry (`offsetX/offsetY/left/right/topLimit/
  turnsToCompress/dangerZone/numSeparators` as `i32`), then per row a
  `u16` cell count and per cell `i32 bubbleId + u8 flags`, then a `u16`
  `nextColors` count and `u8` colors. Positions are recomputed on restore via
  `smallerSep + 32*col` / `28*row` from the recorded offset, never stored.
  `levelLayout` stays empty; `levelHash` = FNV-1a of the board blob. Named
  record fields carry `curLaunch`/`nextBubble`/`gameplayRngState`/
  `initialSimStep`/`initialStepDeltaScale`/`startingScore[0]`/`startingWins[0]`.
  Roster/team/seat-ownership stay zeroed (solo only).
- **`RestoreRoundStart` calls `NewGame()` with the recorded effective
  settings, then overwrites the generated board/queues/RNG/counters.** That is
  the playback initialization bypass; `NewGame` supplies geometry/textures and
  clears the shared projectile globals, the overwrite is the actual record
  restore. No `SyncNetworkLevel()` runs (`networkGame=false`). Ends with
  `SetSessionMode(Playback)`. Rejects anything but `playerCount==1 &&
  networkGame==0`; a malformed board blob is logged and leaves the NewGame
  state rather than reading out of bounds.
- **Canonical hash takes `const BubbleGame&`** (not `const BubbleArray&` as the
  prompt proposed) because the gameplay RNG stream and the global projectiles
  are game-scope state. It serializes board order/contents, nextColors,
  curLaunch/nextBubble, `shooterSprite.angle`, score, chainLevel,
  lifecycle/mp flags, geometry/prelight timers, stats, RNG state and every
  in-flight projectile -- and **excludes** `hurryTimer`/`warnTimer`/
  `suppressFireUntilRelease`/`mouseTargetAngle`/`mouseFirePending` (derive-only
  input state playback never advances).
- **`bubblegame_shooter.cpp` `UpdatePenguin()`**: two-line additive branch --
  `if (sessionMode == Playback) { ApplyPlayerControls(bArray,
  bArray.lastControls, deltaScale); return; }` before the unchanged live path.
- **`bubblegame_render.cpp`/`bubblegame.h`**: `AdvanceSimulationAtScale(float)`
  extracted; `AdvanceSimulation()` stays exactly `singleton read ->
  AdvanceSimulationAtScale`; new public `AdvancePlaybackStep(float)`.
- **`tests/bubblegame_replay_test.cpp`** (new `bubblegame-replay-test` target,
  registered like `bot-play-test` since it needs full headless SDL and a real
  `BubbleGame`+renderer). Three scripted rounds (distinct seeds): win by
  clearing a 2-bubble cluster, loss via a row-12 danger bubble after a steep
  wall-bounce shot, and hurry auto-fire with the hurry timer parked one frame
  below `TIME_HURRY_MAX_MP`. Every round alternates `stepDeltaScale` through
  0.5x/1x/2x. For each: capture the live round (state-hash per step), replay
  **in memory** from the `RoundStartRecord` and assert the hash sequence
  matches at every step, then `ReplayWriter`->file->`ReplayReader`->decode and
  run a **third disk replay** comparing to both the live hashes and the
  per-step `AssertionRecord` hashes. The replay poisons
  `FrozenBubble::Instance()->deltaScale = 3.0f` to prove it uses only the
  recorded values. A final check feeds `RestoreRoundStart` a truncated board
  blob and asserts the decoder rejects it without touching the instance.
- **`cmake/CoreSources.cmake`**: added `bubblegame_replay.cpp`.
- **`CMakeLists.txt`**: registered `bubblegame-replay-test` after
  `replay-session-test`.

**Surprise found only while running the test (the R1c contract's blind spot):**
the loss round diverged at step 1 while win/hurry passed. Binary-searching the
hash field list isolated it to `mouseTargetAngle`. Root cause: the loss shot's
landing bubble immediately collides with the row-12 bubble on the very next
step, so `UpdateSingleBubblesAtScale` sets `gameFinish` **before**
`UpdatePenguin()` runs; `UpdatePenguin()` then returns early in *both* runs, so
`ApplyPlayerControls` never touches `mouseTargetAngle`. In live, that field is
whatever the live input last set (the test's `-1.0`); in replay, it is the
previous step's applied `0.3`; and the recorded `lastControls.mouseAngle` is
stale (`0.3`) because Resolve was skipped too. It is uncaptured input state,
not simulation state, so `mouseTargetAngle`/`mouseFirePending` were removed
from the canonical hash (alongside the already-excluded timers), with the
reason documented on `CaptureCanonicalStateHash`. The resolved aim/fire still
lives in `lastControls`/`StepRecord`; nothing about replay correctness changed.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel          # clean, no new warnings
./build/bubblegame-replay-test          # "bubblegame replay tests passed", exit 0
ctest --test-dir build --output-on-failure
```

41 tests total, 39 passed, 2 skipped (`server-massleave-test`,
`server-udp-probe-test` -- the same pre-existing environmental skips as every
prior session). `bubblegame-replay-test` is the one new test versus the
40-test baseline. `replay-format-test`, `replay-session-test`, `bot-play-test`,
`bubblegame-rules-test` and `statspanelcell-cache-test` all still pass.

**Sanitizer build and result:**

```
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan --target bubblegame-replay-test --parallel
ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --test-dir build-asan --output-on-failure -R bubblegame-replay-test
```

`bubblegame-replay-test` passes clean under ASan+UBSan (`1/1 ... Passed`).
`replay-session-test`, `bot-play-test` and `bubblegame-rules-test` were also
rebuilt and pass clean under the same sanitizer configuration. As recorded in
the R2 entry, `ASAN_OPTIONS=detect_leaks=1` aborts every ASan binary on this
macOS/arm64 platform (`detect_leaks is not supported on this platform`), so
leak detection was omitted rather than treated as a failure -- a platform
limitation, not a gap specific to this code.

**Manual/platform checks: not applicable.** This package adds no UI and no
user-reachable path -- nothing includes `bubblegame_replay.h` outside its own
`.cpp` and the new test, and no game flow calls `AdvancePlaybackStep` or the
capture functions yet (R4 wires the library/UI). The R1 live-multiplayer
visual checks carried over from prior sessions remain outstanding; this
session touched `bubblegame_render.cpp`/`bubblegame_shooter.cpp` only by
additive branches whose live paths are unchanged.

**Known failures/blockers:** none. Build clean, full suite passes, sanitizer
clean.

**Scope discipline confirmed before finishing:** no auto-save/library, no
Replays menu/UI, no export/import, no network recording/playback, no bots, no
other game modes, no checkpoints/seek. `replay_format.h/.cpp` were not
modified -- the board/nextColors/settings blob is encoded wholly within the
existing `startingBoards[]` opaque field. No existing test assertion was
weakened.

**Next concrete step:** R4 (auto-save into the rolling Replays library +
Replays submenu + player + export/import) or R5 (other local rules coverage),
per the plan's own revised order `R1 -> R2 -> R3 -> R5/R6 -> R4 -> R7 -> R8`.
**R5 is the more natural next step**: R4's milestone explicitly includes local
and online round-stats saving, which depends on R5/R6 broadening rule
coverage (and on R6 for the network side), so building the library/UI first
would have it stand on a recorder that only handles one solo Classic shape.
See "Next session" below.

### 2026-09-20 — R5a: step-driven local rules (N seats, bots, chain, attack modes, Race, predefined levels, round 2+)

**Starting source revision:** uncommitted, directly on top of the R3 session above,
same branch (`main`), HEAD still `61b00290ef264dbcca5135e9a50e4227d939cbad`.
Changes below are **uncommitted**.

**R5 was split into R5a and R5b by this session's own inventory** (the same
precedent R1/R1d/R3 followed). Most of R5's list is purely step-driven and sits
directly on R3's `AdvancePlaybackStep`/`simStep`/`stepDeltaScale` architecture
with no new seam. Two items are not: **training mode** (`mpTraining`, gated on
`SDL_GetTicks()` via `mpTrainStartTime` in `bubblegame_render.cpp`) and **Timed
mode** (`GameMode::Timed`, gated on `SDL_GetTicks()` via
`modeTimerStart`/`modeTimerDeadline` in `bubblegame_state.cpp`). Both need a real
recorded-game-clock seam (finally using `StepRecord::gameClockMs`, which R3 left
at 0) analogous to R1b's `deltaScale` seam. That is **R5b**. This session (R5a)
implements only the step-driven subset and explicitly rejects the two clock-
driven modes.

**Implemented:**

- **Board-blob format v2** (`src/bubblegame_replay.cpp`, internal blob; the R2
  envelope in `src/replay_format.h/.cpp` is untouched). v1 re-derived each row's
  horizontal offset as `(cellCount % 2 == 0) ? 0 : 16`. That is correct for
  `RandomLevel()` and `LoadLevel()`'s main per-level loop, but **wrong for
  `LoadLevel()`'s filler rows 10-12** (the branch taken when row 9 has an even
  cell count): there the offset follows the *row index* parity, inverted from
  the cell-count formula. Shipped levels all have an odd 10th row, so the branch
  is unreachable with the stock levelset, but a custom level reaches it. v2 now
  stores **each row's actual `smallerSep`** (column 0's x minus `bubbleOffset.x`)
  and the **per-board `bubbleSize`** (16/32), so restore reproduces every board
  `RandomLevel()` or `LoadLevel()` can produce. `kBoardBlobVersion` went 1 → 2.
- **Widened capture/restore/hash to N seats** (1-5; R3 only ever touched seat
  0). `CaptureRoundStart` now fills `seatIds`/`seatOwned`/`seatTeam`,
  `startingBoards[i]`, `currentColor[i]`/`nextColor[i]`,
  `startingScore[i]`/`startingWins[i]` for every seat and hashes all seat blobs
  plus a small game-rules blob into `levelHash`. `RestoreRoundStart` decodes
  every seat's blob up front (one bad blob rejects the whole record), rebuilds
  each board with the stored offset/size, and restores `numColors`,
  geometry/limits, `nextColors`, `malusQueue`, score/wins, `compressionDisabled`,
  `aimGuideEnabled` and `isBot` per seat. `CaptureCanonicalStateHash` now hashes
  **every live seat's board/state** (plus `pos`, see below) instead of only seat
  0, so a divergence in any seat is caught at the step it happens.
- **A small game-rules blob stored in `RoundStartRecord::levelLayout`.** R2's
  envelope has no dedicated field for `attackMode`, `raceTarget`,
  `timedSeconds` or `mpTraining` -- a genuine schema gap -- so they ride in the
  unused opaque `levelLayout` vector rather than forcing a codec change (the
  prompt's preference). It also carries `localMultiplayer`/`mouseEnabled`.
- **Per-step capture is one `StepRecord` per seat per step.** R2's `StepRecord`
  carries a single `seatId`, so the live driver captures one record per seat
after each `AdvanceSimulation()`, and the replay driver groups same-`simStep`
records, places each seat's `lastControls`, then runs exactly one
`AdvancePlaybackStep()`.
- **Bot fire-angle capture** (`src/bubblegame_shooter.cpp`,
  `ResolvePlayerControls`). `DriveBot()` snaps `shooterSprite.angle` directly
  onto `botTargetAngle` on its firing frame, which left/right/fire cannot
  reproduce one-step-short, so the resolved angle is now carried in the
  `PlayerControls::mouseAngle` field `ApplyPlayerControls()` already snaps to
  (bots never use the mouse; `firedByMouse` stays false). Without this the bot
  scenario diverged at its first snapped shot.
- **Widened rejection gate.** `RestoreRoundStart` accepts `playerCount` 1-5,
  `networkGame == 0`, `gameMode` Classic or Race, and a rules blob that does not
  claim `mpTraining`; everything else is logged and the instance is left exactly
  as it was (R3's malformed-input posture, no half-restore). Timed, Clear,
  network, >5 seats, training and a truncated board are all asserted rejected.
- **`RoundEndRecord::outcome`** now reports a committed multiplayer win
  (`roundWinnerIdx >= 0`, e.g. Race target or Classical elimination) as
  `kReplayOutcomeWin` rather than `kReplayOutcomeDraw`; solo win/loss are
  unchanged.

**Tests (`tests/bubblegame_replay_test.cpp`, R3's target, extended in place):**
R3's solo win/loss/hurry/disk assertions are kept; the harness was generalised
to N seats (positions captured/compared, per-seat stats, bot-RNG tracking,
`(simStep, seat)` grouping for replay) and new scenarios added, each asserting a
divergent-free per-step canonical-hash sequence plus a restored-start hash and
per-seat position equality: 3-player local with two bots (also proves the
playback instance's `botRng` never advances -- `DriveBot()` is not re-invoked --
and does a disk encode/decode/replay round-trip); chain reactions (asserts a
chain really was assigned); `AttackMode::On` malus send/receive; `AttackMode::Canceling`
(mal push fully absorbed -- `rBlk > 0`, `rSent == 0`); teams (seat 0's teammate
spared); Race mode reproducing the same winner/outcome; a crafted predefined
level whose 10th row is even, exercising the `LoadLevel` filler-row fix; and a
round captured after a live `ReloadGame()` (accumulated `winCount == 1` plus
carried-forward RNG).

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel
./build/bubblegame-replay-test          # "bubblegame replay tests passed", exit 0
ctest --test-dir build --output-on-failure
```

Ordinary suite: **41 tests, 39 passed, 2 skipped** (`server-massleave-test`,
`server-udp-probe-test` -- the same pre-existing environmental skips as every
prior session). The replay test now covers internal record counts that grew with
N seats.

**Sanitizer build and result:**

```
cmake --build build-asan --target bubblegame-replay-test --parallel
ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --test-dir build-asan --output-on-failure -R bubblegame-replay-test
```

`bubblegame-replay-test` passes clean under ASan+UBSan (`1/1 ... Passed`), which
covers the new board-blob v2 decode/restore paths (row offsets, bubble size,
malus-queue and rules blobs). As recorded in the R2/R3 entries,
`ASAN_OPTIONS=detect_leaks=1` aborts every ASan binary on this macOS/arm64
platform, so leak detection was omitted, not treated as a failure.

**Manual/platform checks: not applicable.** This package adds no UI and nothing
user-reachable: no file includes `bubblegame_replay.h` outside its own `.cpp`
and the test, and no live flow calls the capture functions or
`AdvancePlaybackStep`. The R1 live-multiplayer visual checks carried over from
prior sessions remain outstanding and were not touched here; the only
production edit (`ResolvePlayerControls`' bot-angle capture) is a no-op for live
play (it re-snaps the angle to the value `DriveBot` just set).

**Design changes and reasons:**

- **R5 split into R5a/R5b** (see above) rather than forcing the clock work into
  this session. R5's plan row now states R5a is implemented and R5b is not.
- **Board blob stores `smallerSep` per row + `bubbleSize` per board** instead of
  re-deriving either, because the derivation is exactly what was wrong for
  `LoadLevel`'s filler rows (and because mini seats need the 16px pitch).
- **`pos` was added to the canonical hash.** `SingleBubble::IsCollision` reads
  the grid bubble's `pos`, so it is simulation-relevant, not a draw-only cache
  (`EncodeBoardBlob`'s old comment claimed otherwise). Hashing it is what makes
  the `LoadLevel` position bug observable directly at the restored start rather
  than only after a divergence propagates into board contents.
- **Game-wide settings in `levelLayout`** rather than a codec change: R2's
  generic envelope has no `attackMode`/`raceTarget`/`timedSeconds`/`mpTraining`
  field. This is a recorded schema gap; a future codec revision could promote
  them to named fields.
- **One `StepRecord` per seat per step** (R2's shape) instead of packing a
  multi-seat control record, documented on `CaptureStep`.
- **`rBlk`/`rKills`/`lastAttackerIdx` added to the per-seat canonical hash** so
  canceling and kill attribution are covered by the per-step comparison.
- **Bot angle captured through `mouseAngle`** (see above): the minimal change
  that avoids extending `PlayerControls`/`StepRecord` while reproducing
  `DriveBot`'s direct snap exactly.

**Known failures/blockers:** none. Build clean, full suite passes, sanitizer
clean.

**Next concrete step:** **R5b** -- training mode (`mpTraining`) and Timed mode
(`GameMode::Timed`) only. Both gate their end condition on `SDL_GetTicks()`
directly and R5a deliberately does not capture a game clock. R5b must build the
recorded-game-clock seam: sample a round-relative monotonic clock once per
`AdvanceSimulation()` step into the existing `StepContext`/
`StepRecord::gameClockMs` (R3/R5a leave it 0), have `mpTrainStartTime` and
`modeTimerStart`/`modeTimerDeadline` read that recorded value instead of
`SDL_GetTicks()` (R1b's exact pattern: thread an explicit parameter and read the
wall clock in exactly one place), and restore `mpTrainStartTime`/mode-timer state
in `RoundStartRecord`. Then widen `RestoreRoundStart`'s gate to accept Timed and
mpTraining and add matching live→replay fixtures. The `mpTraining` rules flag and
`timedSeconds` are already captured in the `levelLayout` blob by R5a, and
`RestoreRoundStart` already rejects them, so R5b mainly adds the clock and the
gate change. See [REPLAY_PLAN.md](REPLAY_PLAN.md)'s R5 row.

### 2026-09-20 — R5b: recorded-game-clock seam + training and Timed mode

**Starting source revision:** uncommitted, directly on top of the R5a session
above, same branch (`main`), HEAD still
`61b00290ef264dbcca5135e9a50e4227d939cbad`. Changes below are **uncommitted**.

**Re-inventory before implementing.** Re-read the actual source instead of the
task prompt's line numbers. All of the prompt's claimed locations held:
`AdvanceSimulation()` / `AdvancePlaybackStep(float)` /
`AdvanceSimulationAtScale(float)` in `bubblegame_render.cpp`, the mp_train
elapsed/done block and the HUD-only training countdown in the same file, the
pause-correction block, `TimedSecondsRemaining()` and `UpdateTimedRound()` in
`bubblegame_state.cpp`, the `DecodedRules`/`EncodeRulesBlob`/`DecodeRulesBlob`
mpTraining flag and timedSeconds, and the current `CaptureStep` /
`CaptureCanonicalStateHash` shapes in `bubblegame_replay.cpp`. Confirmed by
reading `NewGame()`/`ReloadGame()`/`ResetModeState()` directly: `NewGame()`
resets `mpTrainStartTime`/`mpTrainScore`/`mpTrainDone` and calls
`ResetModeState()` (which zeroes `modeTimerStart`/`modeTimerExpired`/
`modeTimerDeadline`), while `ReloadGame()` calls `ResetModeState()` but does
**not** reset `mpTrainStartTime`. That asymmetry is exactly the round-1-only
limit documented below, not something this slice tries to paper over.

**R5b was not further split:** the package inventoried at one reviewable slice
(a new `BubbleGame` clock member, five converted call-site groups, the gate
widening, the hash additions and the two new tests).

**Implemented — the clock seam.**

- `src/bubblegame.h`: new `Uint32 stepGameClockMs = 0;` next to
  `stepDeltaScale`, set once per step and read by every converted site.
  `AdvancePlaybackStep` widened to `(float recordedDeltaScale, Uint32
  recordedGameClockMs)`; the private shared body widened to
  `AdvanceSimulationAtScale(float deltaScale, Uint32 gameClockMs)`.
- `src/bubblegame_render.cpp`: `AdvanceSimulation()` still reads the live
  wall-clock singleton for `deltaScale`, now also reads `SDL_GetTicks()` once
  into `gameClockMs`, and passes both down; `AdvanceSimulationAtScale()` sets
  `stepDeltaScale = deltaScale; stepGameClockMs = gameClockMs;` at the top.
  `AdvancePlaybackStep()` threads the recorded clock straight through. No
  production caller other than the test existed for `AdvancePlaybackStep`
  (confirmed by grep).

**Converted call sites (exactly the ~7 the task named; live value-preserving
because `stepGameClockMs` is the same step's `SDL_GetTicks()`):**

1. `AdvanceSimulation()`'s live read → now the one `SDL_GetTicks()` per step.
2. `bubblegame_render.cpp` mp_train: `if (mpTrainStartTime == 0)
   mpTrainStartTime = stepGameClockMs;` and `elapsed = stepGameClockMs -
   mpTrainStartTime`.
3. `bubblegame_render.cpp` pause correction: `pausedFor = stepGameClockMs -
   timePaused`. (`FrozenBubble::Instance()->startTime` and the two
   `modeTimerStart`/`modeTimerDeadline` shifts it feeds are unchanged apart
   from now receiving this `pausedFor`; `startTime` remains its own live-only
   correction.)
4. `bubblegame_render.cpp` training HUD countdown: `elapsed = stepGameClockMs
   - mpTrainStartTime`.
5. `bubblegame_state.cpp` `TimedSecondsRemaining()` (kept `const`): `elapsed =
   stepGameClockMs - modeTimerStart`.
6. `bubblegame_state.cpp` `UpdateTimedRound()`: `if (modeTimerStart == 0)
   modeTimerStart = stepGameClockMs;`.
7. `bubblegame_state.cpp` `UpdateTimedRound()`: `const Uint32 now =
   stepGameClockMs;`.

`now` feeds the network straggler-wait too (`modeTimerDeadline = now +
 kReportWaitMs`, `kVerdictGraceMs`), so strictly it is not local-only; there is
no separate network-only `SDL_GetTicks()` call to leave alone, and reading the
same step clock there is value-identical in live play. Network Timed is still
rejected by `RestoreRoundStart` (R6 owns it), so this cannot affect any replay.
All other wall-clock uses in these two files were left exactly as they were:
the network round-sync wait, `RenderPaused()`'s `timePaused = SDL_GetTicks()`,
the opt-in-upload `playTimeSeconds`, and `gameStartTime`.

**Round-start restore: widened gate only, no new capture field.**
`RestoreRoundStart()` now accepts `GameMode::Timed` alongside Classic/Race
(Clear stays rejected), drops the `if (rules.mpTraining) reject` branch, and
sets `setup.mpTraining = rules.mpTraining` instead of hardcoding false. No
`RoundStartRecord` field was added: a round 1 starts fresh through `NewGame()`,
which zeroes `mpTrainStartTime`/`modeTimerStart`, and the first replay step's
recorded `gameClockMs` self-initializes them exactly as the live run's first
step did. The now-stale `kRulesFlagMpTraining` comment ("never restored as
true") was rewritten.

**Hash additions.** `CaptureCanonicalStateHash()` now appends, after
`stepDeltaScale`: `stepGameClockMs` (u32) and `mpTrainScore` (i32), and sets
two new bits in the existing `gameFlags` byte — `0x20` for `mpTrainDone` and
`0x40` for `modeTimerExpired` (0x01/0x02/0x04/0x08/0x10 were already used).
`modeTimerStart`/`modeTimerDeadline` are deliberately not hashed separately —
they are implied by `stepGameClockMs` plus the flags. Four get-only accessors
were added to `BubbleGameReplayAccess` (`stepGameClockMs`, `mpTrainScore`,
`mpTrainDone`, `modeTimerExpired`); no setter was needed because
`AdvanceSimulationAtScale()` writes `stepGameClockMs` as a member.
`CaptureStep()` now records the real `stepGameClockMs` into
`s.gameClockMs` (R3/R5a hardcoded 0).

**Tests (`tests/bubblegame_replay_test.cpp`):**

- New `StepScript::syntheticClock`/`gameClockMs` plus a `SyntheticClockSteps()`
  helper and a `BubbleGameTestAccess::advanceSimulationAtScale()` accessor, so
  the live capture can drive `AdvanceSimulationAtScale()` with a synthetic
  clock while `AdvanceSimulation()`'s production signature/behavior stays
  untouched. Existing scripts leave `syntheticClock=false` and still go through
  the real `AdvanceSimulation()`.
- `RunReplay()` passes the recorded `StepRecord::gameClockMs` (same for every
  seat in a step) into `AdvancePlaybackStep(deltaScale, gameClockMs)`.
- A **training round** (`mpTraining=true`, 1 seat, `randomLevels=false`) with a
  crafted board (a color-5 group that scores malus under the
  `destroyed+falling-2` formula, plus a ceiling-attached color-1 bubble so the
  board is never cleared): 200 synthetic 1ms steps, then a jump to
  `1000 + 120000 + 10` that crosses `TRAIN_DURATION`. Asserts `gameWon`,
  outcome win, `mpTrainScore > 0`, the live highscore (`pendingHighscore`)
  and its playback suppression, and per-step hash equality via `RunAndCompare`.
- A **local 3-seat Timed round** (`timedSeconds=2`, no bots): seat 0's crafted
  top row pops (`rPopped 8/0/0`), the clock jumps past the limit, and
  `modeTimerExpired` → `LeadingPopper()` → `ResolveRoundOutcome(0, Timeout)`.
  Asserts `winningSeatId == 0`, `modeTimerExpired`, and per-step hash equality.
- The old "unsupported records" block's `GameMode::Timed` and mpTraining cases
  were flipped from `rejected` to **accepted** (`RestoreRoundStart` reaches
  `SessionMode::Playback`); the `GameMode::Clear` case and the 6-seat/network
  cases remain rejected. No existing assertion was weakened or removed.
- `tests/bubblegame_rules_test.cpp`: its `startTimedClock()` helper now also
  pins `game.stepGameClockMs = SDL_GetTicks()`, because its tests drive
  `UpdateTimedRound()` directly without a full step; every assertion in that
  file is unchanged and still passes.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel
./build/bubblegame-replay-test    # includes [mp-training] steps=201 score=2 fired=1 popped=5
                                  #          [timed] steps=201 popped=8/0/0 winner=0
ctest --test-dir build --output-on-failure
```

Ordinary suite: **41 tests, 39 passed, 2 skipped** (`server-massleave-test`,
`server-udp-probe-test` -- the same pre-existing environmental skips as every
prior session). `bubblegame-rules-test` failed once at first because it drives
`UpdateTimedRound()` directly off `SDL_GetTicks()`; the `startTimedClock()`
helper adaptation above fixed it without changing any assertion.

**Sanitizer build and result:**

```
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan --target bubblegame-replay-test --parallel
ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 ./build-asan/bubblegame-replay-test
```

`bubblegame-replay-test` passes clean under ASan+UBSan
(`bubblegame replay tests passed`, exit 0), as does the adapted
`bubblegame-rules-test`. As recorded in every prior entry,
`ASAN_OPTIONS=detect_leaks=1` aborts every ASan binary on this macOS/arm64
platform, so leak detection was omitted, not treated as a failure.

**Manual/platform checks: not applicable.** This package adds no UI and nothing
user-reachable: nothing includes `bubblegame_replay.h` outside its own `.cpp`
and the test, no live flow calls the capture functions or
`AdvancePlaybackStep`, and the only production edits sample/thread a clock value
the live path already read. The R1 live-multiplayer visual checks carried over
from prior sessions remain outstanding and were not touched here.

**Design changes and reasons:**

- **No `RoundStartRecord` field for the clock.** A fresh round 1 self-initializes
  both timers on its first step, exactly as the live path always did, so a
  start value would be redundant. This is why the seam is one member plus a
  parameter, not a schema change.
- **Round-1-only for training/Timed.** `mpTrainStartTime` is not reset by
  `ReloadGame()`, so a training round captured after a live reload would
  restore with a stale training clock; `modeTimerStart` is reset by
  `ReloadGame()`, so Timed has the same round-1 caveat for its clock. This is
  stated plainly as a limit, not silently generalized past what the tests
  cover. Adding `ReloadGame()` continuity for training/Timed is explicitly
  out of scope (R5a's continuity coverage was Race).
- **`stepGameClockMs` hashed rather than `modeTimerStart`/`modeTimerDeadline`.**
  The latter are absolute values implied by the former plus the mode flags.
- **`bubblegame_rules_test.cpp`'s helper adapted, not its assertions.** The
  clock seam moved the "now" source out of `SDL_GetTicks()` and into the step;
  a test that calls `UpdateTimedRound()` directly must supply that step value.
  No assertion changed.

**Known failures/blockers:** none. Build clean, full suite passes, sanitizer
clean for the touched tests.

**Next concrete step:** **R6** (network recording/playback). R5 is now fully
complete (both halves); the plan's revised order is
`R1 -> R2 -> R3 -> R5/R6 -> R4 -> R7 -> R8`, and R4's completion evidence
requires online rounds to auto-save, so R6 is the remaining prerequisite before
R4. See the plan's R6 row.

### 2026-09-20 — R6a: steady-state 2-peer network message replay

**Starting source revision and branch:** uncommitted, directly on top of the
R5b session above, same branch (`main`), HEAD still
`61b00290ef264dbcca5135e9a50e4227d939cbad`. Changes below are **uncommitted**.

**R6 was split into R6a/R6b/R6c by this session's own inventory**, the same
precedent R1/R1d/R5 followed. R6a is steady-state in-round message replay for a
2-peer network round: the parsed-opcode dispatch made shared/replayable, the
applied inbound payloads recorded per step, real seat ownership/ids/nicknames
restored, and playback made to re-apply those payloads with no socket. R6b
(round-start `SyncNetworkLevel`/`b`/`N`/`T` sync, hosted bots, round 2+
continuity) and R6c (battle royale, delayed events, mid-round departures) are
**not** touched and stay rejected.

**Implemented — the extraction.**

- `BubbleGame::ApplyInboundGameMessage(int senderId, const std::string
  &gameData)` (declared in `src/bubblegame.h`, defined in
  `src/bubblegame_net.cpp`) now owns the entire opcode switch that was inline in
  `ProcessNetworkMessages()`: `f`, `p`, `P`, `n`, `s`, `g`, `m`, `M`, `F`, `S`,
  `i`, `t`, `l`, `A`, and `default`. This was a mechanical move, not a rewrite:
  every `sscanf` now reads `gameData.c_str() + 1`, byte-identical to the old
  `char gameData[512]` reading `gameData + 1`, and the case bodies are
  unchanged. The extraction was done with a brace-matched script (not
  hand-retyped), which keeps the "move code, don't rewrite it" property easy to
  audit.
- The `b`/`N`/`T` case did **not** move. Those three are level-sync forwarding
  (`netClient->PushSyncMessage(msg)`) rather than gameplay mutation, and
  round-start sync is explicitly R6b's scope. They stay in
  `ProcessNetworkMessages()` immediately after the unchanged
  `OwnsSenderId()`/`IsConnectionLevelOpcode()` ownership filter (the task's
  preferred option (a)), and are therefore neither captured nor replayed.
- `ProcessNetworkMessages()` now pumps `Update()`/`PumpBotConnections()`, runs
  the same filter, and for each accepted message either forwards `b`/`N`/`T` or
  records and applies it.
- Behavior on the live path is unchanged: the full existing suite (including
  the network-driving `bubblegame_rules_test.cpp` cases) passes without any
  assertion change.

**Implemented — capture.**

- New `BubbleGame::InboundGameEvent { int senderId; std::string gameData; }` and
  a per-step `std::vector<InboundGameEvent> stepInboundEvents`. The live network
  branch clears it at the start of each step; `ProcessNetworkMessages()` appends
  exactly the payloads that passed the filter and calls the handler, so nothing
  that was dropped as an echo is ever recorded and replay never needs to
  reproduce `OwnsSenderId()`/`GetMyPlayerId()`.
- `CaptureStep()` writes that list into **seat 0's** `StepRecord::inboundEvents`
  only (the list is a property of the step; R2's `StepRecord` already carries a
  single `seatId`, and duplicating it across every seat's record for the same
  step would just bloat the file). Layout: `u16` event count, then per event an
  `i32` sender id and a `u16`-length-prefixed `gameData`. `replay_format.h/.cpp`
  were not touched -- the field is already framed as an opaque `vector<uint8_t>,
  the same pattern R5a used for the rules blob.

**Implemented — restore.**

- `CaptureRoundStart()` now writes `r.networkGame` from the live settings,
  `r.seatOwned[i] = game.OwnsArray(bubbleArrays[i])` (previously hardcoded `1`),
  and `r.seatIds[i] = bubbleArrays[i].lobbyPlayerId` (falling back to the array
  index for a local seat, whose lobby id is `-1`).
- The board blob is now **v3** (`kBoardBlobVersion` 2 -> 3): it appends a
  `u16`-length-prefixed nickname after the malus queue, so the restored game
  can resolve a remote `'g'` destination nick and an `'F'` winner nick to the
  right board. `DecodeBoardBlob()` bounds-checks and caps the length (256).
- `RestoreRoundStart()` accepts exactly `playerCount == 2` with
  `networkGame == 1`, `seatOwned[0] == 1`, `seatOwned[1] == 0`, and neither
  board a bot; 3-5-seat, owned-remote-seat and hosted-bot network records are
  rejected, as is anything outside Classic/Race/Timed. The local 1-5-seat gate
  is unchanged. Restore writes `p.lobbyPlayerId = record.seatIds[i]` and
  `p.playerNickname = board.nickname`, **for a network record only** (follow-up
  correction): both writes are now guarded by `networkRecord`, so a restored
  local round leaves them exactly as `NewGame()` produced them, matching
  R5a/R5b's original local-round behavior byte-for-byte.
- **Deliberate deviation: a network record forces `setup.randomLevels = false`
  before `NewGame()`.** `NewGame()` calls `SyncNetworkLevel()` unconditionally
  when `randomLevels && networkGame` and `QuitToTitle()`s on failure; playback
  has no server, so that path would break restoration. The recorded board
  blobs (decoded and overwritten immediately afterward, exactly as every local
  round already does) are the source of truth, so which path `NewGame()` took
  to make its throwaway initial board does not matter. The recorded
  `randomLevels` flag is still preserved in the board blob for informational
  fidelity. No other `currentSettings.networkGame` branch in `NewGame()` touches
  `NetworkClient` except the `IsConnected()`-gated seat-assignment block, which
  is safe with a disconnected client (and simply skipped).

**Implemented — playback.**

- The network branch of `AdvanceSimulationAtScale()` is now session-split. In
  `SessionMode::Playback` it drains `stepInboundEvents` through
  `ApplyInboundGameMessage()` in capture order, then clears, and skips
  `ProcessNetworkMessages()`, the bot pump, the tournament-return check, the
  per-second ping and the round-sync wait entirely. The pure `frameCount++`
  still runs, so malus timing state is identical to the live path.
- New `SetPlaybackInboundEvents(BubbleGame&, const std::vector<uint8_t>&)` in
  `src/bubblegame_replay.{h,cpp}` decodes the blob with the existing
  `BlobReader` and explicit caps (`kMaxInboundEvents = 4096`,
  `kMaxInboundEventLen = 4096`); a malformed blob logs and queues nothing.
- Two production effects guards were added so "zero sends / no `NetworkClient`"
  holds for a replayed network round:
  - `SendNetworkBubbleShot()` now returns early when `!EffectsEnabled()`, so a
    replayed local shot never constructs/touches `NetworkClient` (the old
    `IsConnected()` check was the first thing it did).
  - `SendMalusToOpponent()`'s `IsConnected()` guard now only applies when
    `EffectsEnabled()` is true. Playback must still run the *local* bookkeeping
    (`rSent`, `lastAttackerIdx`, the attack-flash timer) for a pop that earns
    malus -- only the actual send needs suppressing, and `SendGameDataFor()`
    already does that. This was found by the test: requiring `IsConnected()`
    here silently dropped the local side of every attack during replay, making
    `rSent` diverge at the attack step. Live behavior is unchanged.

**Tests (`tests/bubblegame_replay_test.cpp`, R3/R5's target, extended in
place).** Added the `NetworkClientTestAccess` fake-room helper (copied from
`bubblegame_rules_test.cpp`), an `inboundWire` field on `StepScript` so a live
step can queue `GAMEMSG:{senderId}:{data}` strings on the real
`NetworkClient::Instance()` before `AdvanceSimulation()`,
`RunLiveNetworkRound()` (installs the fake client before `NewGame()` so its
network branch seats the remote peer, resets it to `DISCONNECTED` afterward),
a `ShapeNetworkBoard()` helper (builds valid full-size positions for seat 1,
which `LoadLevel()` does not fill), and `RunReplay()` now decodes seat 0's
`inboundEvents` and calls `SetPlaybackInboundEvents()` before each
`AdvancePlaybackStep()`. Four new blocks:

- **remote fire+stick** -- seat 0 fires; the remote `f` at step 5 and `s` at
  step 10 drive the real dispatch. Asserts the round start carries
  `networkGame=1`, `seatOwned={1,0}`, `seatIds={0,1}`; that the remote `s`
  really placed a bubble on seat 1's board; per-step hash equality for the
  in-memory replay; and the same equality after an `EncodeRecording` ->
  `DecodeRecording` disk round-trip (proving the new blob survives the codec).
- **malus both directions** -- seat 0's crafted top row pops via the real
  `SendMalusToOpponent()` path, asserting `rSent[0] > 0`; an injected remote
  `glocal_nick:2` then asserts `rRecv[0] > 0`; per-step hash equality.
- **remote-announced finish** -- injected `Fremote_nick` sets
  `complete=1`, `outcome=Win`, `winningSeatId=1` (`liveGameWon == false`), with
  per-step hash equality and a playback-side finish/outcome check.
- **rejections** -- 3-seat network, owned-remote-seat and hosted-bot network
  records are each asserted to leave the target instance in `Live` mode
  (never half-restored).

No existing assertion was weakened or removed; the three earlier local R3/R5a/
R5b scenarios and the whole existing suite still pass unmodified.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel          # clean, no new warnings
./build/bubblegame-replay-test          # includes [net-fire-stick] seat1Bubbles=1
                                        #          [net-malus] sent0=6 recv0=2 recv1=0
                                        #          [net-finish] winner=1 steps=11
ctest --test-dir build --output-on-failure
```

Ordinary suite: **41 tests, 39 passed, 2 skipped**
(`server-massleave-test`, `server-udp-probe-test` -- the same pre-existing
environmental skips as every prior session). The three network scenarios and
the rejection block run inside the existing `bubblegame-replay-test` executable,
so the total count stays 41.

**Sanitizer build and result:**

```
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan --target bubblegame-replay-test --parallel
ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 ./build-asan/bubblegame-replay-test
```

`bubblegame-replay-test` passes clean under ASan+UBSan (`bubblegame replay
tests passed`, exit 0), covering the new inbound-blob decode and the network
board-blob v3 nickname paths. `bubblegame-rules-test` and `replay-session-test`
were also rebuilt and pass clean under the same configuration. As recorded in
every prior entry, `ASAN_OPTIONS=detect_leaks=1` aborts every ASan binary on this
macOS/arm64 platform, so leak detection was omitted, not treated as a failure.

**Manual/platform checks: not applicable.** This package adds no UI and nothing
user-reachable: nothing includes `bubblegame_replay.h` outside its own `.cpp`
and the test, and no live flow calls the capture functions or
`AdvancePlaybackStep`. The R1 live-multiplayer visual checks carried over from
prior sessions remain outstanding and were not touched here. The two production
edits (`SendNetworkBubbleShot`/`SendMalusToOpponent` guards) are no-ops in Live
mode, which the existing network tests confirm.

**Design changes and reasons:**

- **`b`/`N`/`T` kept out of `ApplyInboundGameMessage()`** (task's option (a)):
  level-sync forwarding is not a gameplay mutation and is R6b's scope, so it
  stays in the outer loop rather than becoming a no-op case inside the replay
  handler.
- **Inbound payloads ride on seat 0's `StepRecord` only**: one list per step,
  not per seat, documented on `CaptureStep`.
- **Board blob v3 nickname** rather than a new `RoundStartRecord` field: same
  "opaque blob, not a codec change" precedent R5a used for the rules blob.
- **`setup.randomLevels = false` for network restore** (see above): the one
  deliberate deviation from recorded `randomLevels`, to keep `SyncNetworkLevel()`
  out of playback.
- **`SendMalusToOpponent()` keeps local bookkeeping in Playback** instead of
  returning early: the send is already suppressed centrally; returning early
  was what desynced `rSent`.

**Known failures/blockers:** none. Build clean, full suite passes, sanitizer
clean for the touched tests.

**Next concrete step:** **R6b** -- round-start `SyncNetworkLevel()` plus the
`b`/`N`/`T` bubble-sync messages resolved into the round-start snapshot (so a
network replay never waits on a socket), hosted bots (`NetBotConnection`/
`botConnections`), and round 2+ network continuity (including the network Timed
leader verdict). R6c (battle royale, delayed events, departures) follows. See
[REPLAY_PLAN.md](REPLAY_PLAN.md)'s R6 row and completion note.

### 2026-09-20 — R6b: round-start sync, hosted bots, round 2+ continuity

**Date / package:** 2026-09-20 — R6b (network round-start sync, hosted bots and
round 2+ continuity; network Timed leader verdict).

**Starting source revision and branch:** uncommitted, directly on top of the R6a
session above, same branch (`main`), HEAD still
`61b00290ef264dbcca5135e9a50e4227d939cbad`. Everything below is **uncommitted**.

**Changes.**

- `src/bubblegame_replay.cpp` — the network gate in `RestoreRoundStart()` was
  widened from "exactly 2 seats, seat 0 owned, seat 1 remote, no bots" to a
  general per-seat rule applied *after* the existing board-decode loop: a
  network record needs `playerCount` in 2-5; seat 0 must be owned
  (`seatOwned[0] == 1`) and its board must not be a bot; for every other seat
  `i`, `seatOwned[i] != 0` must equal `boards[i].isBot`. The coarse
  `playerCount` range check still runs before any board is touched, so a record
  failing it never decodes. `CaptureRoundStart()` now records the live
  `NetworkClient::Instance()->IsLeader()` result (false for a local record) as a
  new rules-blob bit, and `RestoreRoundStart()` writes it back with a new
  `BubbleGameReplayAccess::setWasNetworkLeader()` after `NewGame()` runs. Also
  added the `wasNetworkLeader` accessor pair and the
  `kRulesFlagWasNetworkLeader = 0x08` flag (no version bump).
- `src/bubblegame_replay.h` — updated the file-level scope comment and the
  `RestoreRoundStart()` doc comment to describe the R6b rule (2-5 seats, hosted
  bots, round 2+, recorded leader flag) instead of R6a's 2-peer/no-bot text.
- `src/bubblegame.h` — one new private `bool wasNetworkLeader = false;` member in
  the Race/Timed state block.
- `src/bubblegame_state.cpp` — `UpdateTimedRound()`'s leader read is now
  `EffectsEnabled() ? (netClient && netClient->IsLeader()) : wasNetworkLeader`, so
  Live is byte-for-byte unchanged and Playback reads the recorded flag instead of
  the (unconfigured) live singleton.
- `tests/bubblegame_replay_test.cpp` — one new scenario per R6b item
  (`net-sync-leader`, `net-round2`, `net-hosted-bot`, `net-timed-leader`), a
  per-seat starting-`bubbleId` capture (`startBubbleIds`) to prove two seats
  came from one `SyncNetworkLevel()` layout, a `botConnectionsEmpty`
  `BubbleGameTestAccess` hook surfaced on `ReplayResult`, an optional
  `GameRoom*` parameter on `SetupFakeNetworkClient()`/`RunLiveNetworkRound()`
  (existing call sites pass nothing and are unchanged), and `RunLiveNetworkRound()`
  now seeds bot streams like `RunLiveRound()` and uses `setup.playerCount`
  instead of a hardcoded 2. The old rejection block's three cases were
  re-framed against the new rule.

**Completed acceptance criteria.**

- A `SyncNetworkLevel()` round-1 capture as the leader replays with per-step
  hash equality (`net-sync-leader`); both seats carry the same non-empty cell
  contents, proving the board came from the sync rather than `LoadLevel()`.
- Round 2+ network continuity (`net-round2`): round 1 runs live uncaptured to a
  win, `ReloadGame()` starts round 2, round 2 is captured with
  `startingWins[0] == 1` and `initialSimStep > 0`, and replays with per-step
  hash equality.
- A hosted-bot network seat (`net-hosted-bot`, 2 seats: 0 local, 1 bot): the
  bot really fired (`fired1=2`), `RunAndCompare` passes, and the playback
  instance holds no `botConnections`/`pendingBots` and makes zero network sends.
- Network Timed leader regression (`net-timed-leader`): captured live as the
  leader, and the replay actually reaches `gameFinish` with the same outcome —
  without the fix the replay would stall in the longer non-leader wait branch
  and `replay.gameFinish` would stay false.
- The three old malformed-ownership rejections still reject (3 seats now at the
  missing seat-2 board decode; owned-remote and unowned-bot mismatches at the
  new rule) and leave the instance in `SessionMode::Live`.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel          # clean, no warnings
./build/bubblegame-replay-test          # includes:
                                        #   [net-sync-leader] cells=98 nonEmpty=38 steps=30
                                        #   [net-round2] startingWins=1 initialSimStep=68 steps=150
                                        #   [net-hosted-bot] fired0=0 fired1=2 seat1Bubbles=1 steps=400
                                        #   [net-timed-leader] steps=202 popped=9/0 outcome=1
                                        # bubblegame replay tests passed
ctest --test-dir build --output-on-failure
```

Ordinary suite: **41 tests, 39 passed, 2 skipped**
(`server-massleave-test`, `server-udp-probe-test` — the same pre-existing
environmental skips as every prior session). The four new scenarios and the
re-framed rejection block run inside the existing `bubblegame-replay-test`
executable, so the ctest count stays 41.

Sanitizer build for the touched restore/decode paths:

```
cmake --build build-asan --target bubblegame-replay-test --parallel
ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 ./build-asan/bubblegame-replay-test
```

`bubblegame-replay-test` passes clean under ASan+UBSan (exit 0,
`bubblegame replay tests passed`, no sanitizer diagnostics). As recorded in
every prior entry, `ASAN_OPTIONS=detect_leaks=1` aborts every ASan binary on this
macOS/arm64 platform, so leak detection was omitted, not treated as a failure.

**Manual/platform checks: not applicable.** This package adds no UI and nothing
user-reachable: nothing outside `bubblegame_replay.cpp` includes the replay
header, and no live flow calls capture/restore. The R1 live-multiplayer visual
checks carried over from prior sessions remain outstanding and were not touched
here.

**Design changes and reasons:**

- **Leader flag in the rules blob, not the board blob.** "Was I the network
  leader at round start" is a round-level fact, and `RoundStartRecord::levelLayout`
  is already genuinely single-per-round while the board blob is duplicated per
  seat. Added as `kRulesFlagWasNetworkLeader = 0x08`; no `kBoardBlobVersion` or
  `kRulesBlobVersion` bump, since it is a new bit in an existing flags byte
  (the same way the existing rules bits were introduced).
- **Gate applied after the decode loop.** The per-seat rule needs each decoded
  `boards[i].isBot`, so the decode loop stays first and the rule follows it;
  the coarse `playerCount` range check stays before the loop, preserving the
  "failing record never touches a board" posture.
- **No new sync mechanism.** The research backing this package held: because
  `CaptureRoundStart()` runs after `NewGame()`/`ReloadGame()` resolves, the
  round-start board blob already carries `SyncNetworkLevel()`'s layout and round
  2+'s board, so `b`/`N`/`T` need no capture and no socket is needed during
  playback. No `bubblegame_level.cpp`/`bubblegame_net.cpp` change was made.

**Known failures/blockers:** none. Build clean, full suite passes, sanitizer
clean for the touched test.

**Next concrete step:** **R6c** — >5-player hidden boards and netview paging,
delayed/out-of-order `S`/`P`/`F`/`l` events, and mid-round departures. See
[REPLAY_PLAN.md](REPLAY_PLAN.md)'s R6/R6c rows.

### 2026-09-20 — R6c: battle royale, late result tail, mid-round departures

**Date / package:** 2026-09-20 — R6c (battle royale >5-player hidden boards,
delayed/late `S`/`P`/`F` result-tail events, and mid-round departures ``l``).
The final sub-package of R6; R6 as a whole is complete with it.

**Starting source revision and branch:** uncommitted, directly on top of the
R6b session above, same branch (`main`), HEAD still
`61b00290ef264dbcca5135e9a50e4227d939cbad`. Everything below is **uncommitted**.

**Changes.**

- `src/bubblegame_replay.cpp` — the seat bound was widened in **two** places,
  not one. (1) The coarse range check in `RestoreRoundStart()` now uses a new
  `kMaxNetworkSeats = kMaxPlayers` (20) for a network record while the local
  branch keeps `kMaxLocalSeats` (5) unchanged. (2) `SeatCount()` — the helper
  that decides how many seats `CaptureRoundStart()` captures,
  `CaptureRoundEnd()` reports and `CaptureCanonicalStateHash()` hashes — now
  caps at `kMaxNetworkSeats` when `currentSettings.networkGame` is set and at
  `kMaxLocalSeats` otherwise. The second fix is the important one: without it a
  7-seat network round would still have captured/hashed only seats 0-4 and
  `rec.start.playerCount` would have read 5, so widening only the gate would
  have produced a broken widening. No other production code changed.
- `src/bubblegame_replay.h` — file-level scope comment and the
  `RestoreRoundStart()` doc comment updated from R6b's "2-5 seats" to R6c's
  "2-20 seats" (battle royale). No signature/behavior change beyond the .cpp.
- `tests/bubblegame_replay_test.cpp` — three new scenarios and the test-only
  helpers they need (see below).
- `docs/REPLAY_PLAN.md`, this file — R6/R6c rows updated and the R6c completion
  note / this entry appended only after the ordinary suite passed.

**Completed acceptance criteria.**

- A 7-seat battle-royale round (`net-royale-malus`) captures and replays with
  per-step hash equality across **all seven** seats. `rec.start.playerCount ==
  7` is the direct assertion that `SeatCount()` was widened; seats 1 and 2 are
  hosted bots (`seatOwned == 1`), seats 3-6 genuine remote peers
  (`seatOwned == 0`). Seat 0's crafted top row pops while 7 seats are alive, so
  `SendMalusToOpponent()` takes its `>5-alive` random single-target branch
  (`sent0=6`, `remoteRecv=6`, and exactly one of the six opponents credited --
  `remoteSeats=1`), and both hosted bots fire (`botsFired=2/2`).
  Because that branch draws its target from the shared gameplay RNG, identical
  hash sequences require the same target to have been picked on replay.
- A late result-tail event after this client's own `gameFinish`
  (`net-late-stats`) is captured and replayed: remote `'F'` at step 10 finishes
  the round, a remote `'S'` at step 20 arrives after it, and the harness keeps
  all 40 steps (`finishStep=10 totalSteps=40`); the late `'S'` sets hashed
  `rFired=3`/`rPopped=2` on seat 1 in both live and replay.
- A mid-round departure (`net-royale-departure`): remote seat 6's `'l'` at
  step 5 leaves `connectedPlayerCount == 6` and seat 6 in state `LEFT` in both
  live and replay, and (one departure from seven) the round does not end.
- A 21-seat network record is rejected by the coarse range check; the existing
  6-seat local rejection is unchanged and still passes.

**Commands run and actual results:**

```
cmake -B build -G Ninja
cmake --build build --parallel          # clean, no warnings
./build/bubblegame-replay-test
  [net-royale-malus] seats=7 sent0=6 remoteRecv=6 remoteSeats=1 botsFired=2/2 steps=400
  [net-late-stats] finishStep=10 totalSteps=40 seat1Fired=3 seat1Popped=2
  [net-royale-departure] seats=7 connected=6 seat6State=2 steps=60
  bubblegame replay tests passed
ctest --test-dir build --output-on-failure
```

Ordinary suite: **100% tests passed out of 41** — 39 passed, 2 skipped
(`server-massleave-test`, `server-udp-probe-test`, the same pre-existing
environmental skips as every prior session). The three new scenarios and the
over-bound rejection run inside the existing `bubblegame-replay-test`
executable, so the ctest count stays 41.

Sanitizer build for the touched capture/hash/restore paths:

```
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan --target bubblegame-replay-test --parallel
ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 ./build-asan/bubblegame-replay-test
```

`bubblegame-replay-test` passes clean under ASan+UBSan (exit 0,
`bubblegame replay tests passed`, no sanitizer diagnostics). As recorded in
every prior entry, `ASAN_OPTIONS=detect_leaks=1` aborts every ASan binary on this
macOS/arm64 platform, so leak detection was omitted, not treated as a failure.

**Manual/platform checks: not applicable.** This package adds no UI and nothing
user-reachable: nothing outside `bubblegame_replay.cpp` includes the replay
header, and no live flow calls capture/restore. The R1 live-multiplayer visual
checks carried over from prior sessions remain outstanding and were not touched
here.

**Design changes and reasons:**

- **SeatCount() was the real R6c bug, not just the range check.** The range
  check rejects unsupported foreign records; `SeatCount()` determines what a
  supported record even contains. Leaving it at 5 would have made a 7-seat
  capture look successful while silently dropping seats 5-6, so it is split on
  `networkGame` exactly like the gate. Local-game behavior is byte-for-byte
  unchanged.
- **No `bubblegame_net.cpp` change, and no result-tail production change.**
  Investigation confirmed the dispatch already handles `'l'`/`'S'`/`'P'`/`'F'`
  correctly after `gameFinish` (`'S'`/`'P'` unconditional; `'F'` gated on
  `!gameFinish`, a deterministic no-op), and `HandlePlayerDeparture()`/
  `ResolveRoundOutcome()`/`CommitRoundWin()` read only already-hashed board
  state and never `IsLeader()` or the wall clock. The only gap was the test
  harness, so the fix (`stopAtFinish`) is test-only.
- **`netViewPage`/`boardVisible` left untouched** (no capture, hash or restore):
  view state re-derived deterministically from hashed state, never read by
  simulation or the hash. See the plan's R6c note for the one keyboard-handler
  caveat.
- **Result-tail test shape.** A remote `'F'` (not a local clear) finishes the
  network Classic round, because `CheckGameState()`'s local `allClear()` win
  branch is gated to `playerCount < 2 || gameMode == Clear` and Clear is
  rejected by the replay gate; the subject under test is the late `'S'`, which
  the `'F'` simply gets out of the way first.
- **Test-only helper compatibility.** `CaptureLiveSteps()`/`RunLiveNetworkRound()`
  gained a defaulted `stopAtFinish = true`, `ShapeNetworkBoard()` a defaulted
  `bubbleSize = 32`, and `SetupFakeNetworkClient()` a defaulted `seatCount = 2`
  that rebuilds the original two-entry nick map exactly. Defaults keep every
  existing call site's captured steps and assertions byte-for-byte unchanged.

**Known failures/blockers:** none. Build clean, full suite passes, sanitizer
clean for the touched test. One pre-existing scope gap is flagged (not a
regression, not worked around): `sendMalusToOne` (manual single-target
selection) is neither captured nor restored, so a replay of a round where the
player manually picked a malus target in a `>5-alive` room could diverge. No
tested scenario selects a manual target. This predates R6c and is outside its
stated scope; it is recorded in the plan's R6c scope-limits paragraph.

**Next concrete step:** **R4** — auto-save on round finalize into a rolling
library, detached capture lifetime and eviction, the Replays submenu
(play/export/keep-count), the player, and desktop/WASM replay-file
export/import. This is the first user-facing milestone and is now unblocked:
R6 (network capture/replay) is complete. R7 (mobile bridges/platform
qualification) and R8 (checkpoints/seek) follow. See
[REPLAY_PLAN.md](REPLAY_PLAN.md)'s package order (R1 → R2 → R3 → R5/R6 → R4 →
R7 → R8).

### 2026-09-21 — R4a: production capture wiring (ReplayRecorder)

**Date / package:** 2026-09-21, R4a — the first slice of R4.

**Starting source revision and branch:** `main` at `61b00290`, working tree
carrying the uncommitted R1–R6 replay work.

**Changes:** all uncommitted. New `src/replay_recorder.{h,cpp}` (added to
`cmake/CoreSources.cmake`) and `tests/replay_recorder_test.cpp` (registered as
`replay-recorder-test` in `CMakeLists.txt`). Three one-line hooks:
`bubblegame.cpp` at the end of `NewGame()` and of `ReloadGame()`, and
`bubblegame_render.cpp` at `AdvanceSimulationAtScale()`'s single `return true`.
All three are `if (ReplayRecorder *r = ReplayRecorder::Existing())`, so a build
where nothing created the singleton pays nothing. `bubblegame.h` gains one
accessor, `AppliedInboundEvents()`.

**R4 was split before dispatch**, following the R1/R5/R6 precedent:
**R4a** recorder + capture wiring (this entry) → **R4b** on-disk rolling
library, eviction and the `Replay:KeepCount` setting → **R4c** playback viewer
→ **R4d** the Replays page UI and its input parity → **R4e** desktop and WASM
export/import. Only R4a is done.

**Completed acceptance criteria.** Capture now starts automatically at every
round boundary and seals automatically when the round settles, with no user
action and no file I/O. A local round seals on the step `gameFinish` becomes
true, after that step's records. A network round keeps a **bounded result tail**
open and seals on the first of: every non-owned seat's `S` observed, the next
round boundary, or 2000 ms measured in the *recorded* game clock rather than
wall time (so a replay reproduces the seal deterministically). Missing stats set
`RoundEndRecord::complete = 0` — never a fabricated stats row. A round that
never reached `gameFinish` also gets `outcome = kReplayOutcomeIncomplete`.
Keep count 0 declines before `CaptureRoundStart()`, so it costs nothing rather
than recording-then-discarding; `SessionMode::Playback` records nothing.

**Commands run and actual results.** Clean rebuild (`rm -rf build`, Ninja) with
zero warnings; `ctest --test-dir build --output-on-failure` → **42 tests, 40
passed, 2 skipped** (`server-massleave-test`, `server-udp-probe-test` — the two
pre-existing environmental skips), 333 s. Sanitizer build with
`ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1`,
`ctest -R replay` → 4/4 passed including the new test, 99 s.

**Design changes and reasons.**

- **A singleton, not a `BubbleGame` member.** The plan's "detached capture
  lifetime" rule is that a *finished* recording outlives the round state it came
  from — `QuitToTitle()`, a tournament return or app exit must not take it
  along. Matches `AudioMixer`/`HighscoreManager`/`TransitionManager`.
- **The hooks call `Existing()`, never `Instance()`**, so they cannot
  accidentally bring a recorder into being. Nothing in production installs a
  sink yet, which means the wiring ships inert until R4b — deliberate, but it
  also means **the production path gets no runtime exercise until R4b**.
- **`setSessionMode(Playback)` moved ahead of `NewGame()`** in
  `RestoreRoundStart()` (`bubblegame_replay.cpp`), so the round-start hook sees
  Playback already set and declines. Audited as inert for restore: `NewGame()`
  contains no `EffectsEnabled()`/`sessionMode` reference, and none of the five
  gated functions (`QuitToTitle`, `SetSendMalusToOne`, `ProcessMalusQueue`,
  `SubmitScore`, `CheckGameState`) is reachable from it during a restore —
  `SyncNetworkLevel()` is already excluded because a network record restores
  with `randomLevels = false`. It also makes the restore's own `NewGame()` run
  with effects suppressed, which is what it should always have done.

**Known limitation, recorded rather than worked around.**
`AdvanceSimulationAtScale()` has two `return false` paths — the tournament
return and the joiner's round-sync wait — that never reach the step hook, so
they contribute no `StepRecord` even though `simStep` already advanced and real
mutation already happened on both (`QuitToTitle()` on the first,
`ProcessNetworkMessages()` on the second). Both only run once their round is
already over, so the only recording they can affect is a network result tail
still waiting for a late `S`: that tail loses whatever arrived during the gap
and seals as incomplete at the next round boundary. **The sealed recording stays
self-consistent** — its step list is a prefix and its round-end snapshot matches
its own last captured step — so playback never diverges; it is only ever less
complete than the live round was. Fixing it properly means splitting those
early returns into "mutated, then bailed", which belongs with R8's
seek/checkpoint work. The hook's comment in `bubblegame_render.cpp` states this
in full.

**Also worth carrying into R4b/R4c.** The 64 MiB transient budget is an
explicit placeholder — it covers only the `ReplayWriter` buffer and awaits
measured `.fbr` sizes, which R4b produces. And a joiner's round-sync wait
advances `simStep` without emitting a record, so the eventual playback driver
must take `simStep` from the recorded `StepRecord` rather than assuming
contiguity.

**Verification performed independently** of the implementing agent's report:
every new and changed file read in full, hook placement confirmed by grep,
`seatIds`-vs-`senderId` identity confirmed at
`bubblegame_replay.cpp`'s `seatIds[i] = lobbyPlayerId >= 0 ? lobbyPlayerId : i`,
the `setSessionMode` move audited against all 16 `EffectsEnabled()` call sites,
plus the clean rebuild, full suite and sanitizer runs above. One comment at the
step hook was corrected: it had claimed the two early-return paths "did not
complete a step's mutation", which is false for both.

**Known failures/blockers:** none.

**Next concrete step:** **R4b** — the on-disk rolling library: a `replays/`
child of the pref dir, a binary atomic-write helper (`ReplaceFileAtomically()`
already exists and is the right finalize step; the staging half in
`highscoremanager.cpp` is TU-local, text-mode and `std::string`-payload, so it
cannot be reused), listing metadata, eviction to the keep count, and the
`Replay:KeepCount` setting in `GameSettings`. Wiring it to `ReplayRecorder`'s
sink is a one-line change by construction.

### 2026-09-21 — R4b: on-disk rolling replay library (ReplayLibrary)

**Date / package:** 2026-09-21, R4b — the second slice of R4.

**Starting source revision and branch:** `main` at `61b00290`, working tree
carrying the uncommitted R1–R6 + R4a replay work.

**Changes:** all uncommitted. New `src/replay_library.{h,cpp}` (added to
`cmake/CoreSources.cmake`) and `tests/replay_library_test.cpp` (registered as
`replay-library-test`). `src/gamesettings.h`/`.cpp` gain the
`Replay:KeepCount` setting (range 0–20, default 5) plus a header declaration
for the pre-existing `EnsureDirectoryExists()`. `src/frozenbubble.cpp` gains
the one-line sink wiring `ReplayRecorder`'s R4a comments already anticipated.

**Completed acceptance criteria.** `ReplayLibrary::Write()` is now
`ReplayRecorder`'s real sink: every sealed recording lands atomically in
`<prefPath>replays/` under a collision-free internal name
(`replay-<13-digit-ms>-<4-digit-counter>.fbr`), then eviction runs down to the
keep count, oldest-mtime-first, **only after** the new file is durably
renamed in — a crash mid-eviction can never lose the round that just
finished. `List()` decodes only header + round-start + round-end (never step
data) for cheap metadata; a corrupt or truncated file is skipped with a
logged warning rather than breaking the listing or crashing. `Delete()` and
`ReadBytes()` both reject any filename that could resolve outside
`replays/` (absolute paths, `..`, either separator). Keep count 0 makes
`Write()` a no-op independent of whatever calls it — the library does not
trust `ReplayRecorder` to be its only caller. `Replay:KeepCount` persists
across restart, clamps out-of-range values, and has a dedicated
`SetReplayKeepCount()` setter (no UI calls it yet — R4d's job).

**Commands run and actual results** (my own, independent of the implementing
agent's self-report). Clean rebuild (`rm -rf build`, Ninja): zero warnings.
`ctest --test-dir build --output-on-failure` → **43 tests, 41 passed, 2
skipped** (the same pre-existing `server-massleave-test`/
`server-udp-probe-test` environmental skips), 329 s. Sanitizer build
(`build-asan` reconfigured — its cmake cache predated `replay_library.cpp`'s
addition to `CoreSources.cmake`) with
`ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1`,
`ctest -R replay` → **5/5 passed** including the new test, 100 s.

**Design deviations from the dispatched spec, all traced and confirmed
correct rather than taken on trust:**

- **`Replay:KeepCount` is parsed by hand (`std::strtol` with full validation
  — no trailing garbage, no overflow, in-range) instead of
  `iniparser_getint()`.** That helper's `strtol` returns 0 for a non-numeric
  value, and 0 is a meaningful setting ("do not record") here — a typo in a
  hand-edited ini file would otherwise silently turn capture off instead of
  falling back to the default. Confirmed the validation is airtight: `end !=
  str && *end == '\0' && errno == 0 && 0 <= parsed <= 20`.
- **The sink-wiring line in `frozenbubble.cpp` is compiled out under
  `#ifndef FROZEN_BUBBLE_TEST_ACCESS`.** Every test target links
  `frozen-bubble-core-test`, which defines that macro publicly
  (`CMakeLists.txt`); the production `frozen-bubble-sdl3` target does not.
  Confirmed this guard is load-bearing, not defensive posturing:
  `BubbleGame::NewGame()` really does call `FrozenBubble::Instance()`
  (`bubblegame.cpp`: `FrozenBubble::Instance()->startTime = ...`), so a
  `replay-recorder-test` scenario that constructs its first `BubbleGame`
  after installing its own test sink would otherwise have that sink silently
  replaced by the disk-writing one the instant `FrozenBubble`'s constructor
  ran for the first time in-process — breaking every R4a test without any
  message pointing at the cause.
- **The binary atomic-write helper stayed local to `replay_library.cpp`**
  rather than moving to `platform.*`. Reasonable for now (single caller);
  flagged as the point to promote it once a second binary writer exists
  (R4e's export path is the likely second caller).

**Verification performed independently:** `git status --short` confirmed the
changed/added file set was exactly R4b's scope, nothing else. Read
`replay_library.h`/`.cpp` and the new test file in full. Traced
`DecodeMetadata()`'s hand-rolled record-framing walk against
`replay_format.h`'s actual `RecordType` enum and framing layout
(`[u8 type][u32 len][payload]`) and confirmed `ReplayReader::Remaining()`
makes `bytes.size() - reader.Remaining()` land exactly on the byte offset
after `ReadHeader()`+`ReadRoundStart()`'s two record reads — not a fragile
assumption. Confirmed `IsKnownRecordType()` mirrors exactly the four record
types the codec currently writes (`RoundStart`/`Step`/`Assertion`/`RoundEnd`;
`Checkpoint = 5` stays reserved for R8 and unwritten). Confirmed the
`FROZEN_BUBBLE_TEST_ACCESS` compile-definition split by reading
`CMakeLists.txt` directly rather than trusting the comment. Confirmed the
eviction-after-durable-write ordering and the path-containment checks by
reading the code, not only by the tests passing.

**Known failures/blockers:** none.

**Next concrete step:** **R4c** — the playback viewer: restore a library
entry via `RestoreRoundStart()`/`AdvancePlaybackStep()` (already built by
R3/R5/R6) and drive it as a session with pause/resume, 0.5×/1×/2×/4× speed,
restart and exit. Entered and tested programmatically, no UI yet — R4d wires
the menu row that reaches it.

### 2026-09-21 — R4c: playback viewer (ReplayPlayer)

**Date / package:** 2026-09-21, R4c — the third slice of R4.

**Starting source revision and branch:** `main` at `61b00290`, working tree
carrying the uncommitted R1–R6 + R4a/R4b replay work.

**Changes:** all uncommitted. New `src/replay_player.{h,cpp}` (added to
`cmake/CoreSources.cmake`) and `tests/replay_player_test.cpp` (registered as
`replay-player-test` in `CMakeLists.txt`). One new production bridge in
`src/bubblegame_replay.{h,cpp}`: `SetPlaybackControls(BubbleGame&, int seat,
const PlayerControls&)`, the control-side sibling of
`SetPlaybackInboundEvents()`, reached through the existing
`BubbleGameReplayAccess` friend so `BubbleGame`'s own public surface is
unchanged. `src/replay_player.h` now includes `player_controls.h` to declare
that bridge. No existing function in `bubblegame_replay.cpp` changed;
`replay_format.{h,cpp}` untouched.

**Completed acceptance criteria.** `ReplayPlayer::Load()` decodes a full `.fbr`
stream (header, round start, every Step/Assertion/RoundEnd, with a peek
returning `Truncated` as clean EOF), groups StepRecords by `simStep`, constructs
a fresh owned `BubbleGame` on the supplied renderer and restores it via
`RestoreRoundStart()`; a decode error or a restore rejection leaves the player
cleanly unloaded, and `AdvanceFrame()`/`Draw()` are then no-ops. `AdvanceFrame()`
uses a fractional accumulator so the four fixed speeds change only how many
recorded steps run per call, never the recorded physics scale. Each consumed
group sets every seat's `lastControls` (and seat 0's inbound events), steps once,
and verifies `CaptureCanonicalStateHash()` against the recorded assertion; the
first mismatch sets desynced and stops consumption for good. Pause/resume,
0.5×/1×/2×/4× snap, and `Restart()` (fresh rebuild) are implemented. `Draw()`
blits the game plus a non-interactive TTFText HUD (`REPLAY`, mm:ss / mm:ss,
speed, `PAUSED`) using the same recompute-on-change/draw-every-frame idiom as the
score HUD. `IsFinished`/`IsDesynced`/`IsRecordingComplete`/`ElapsedMs`/`TotalMs`
accessors are present.

**Commands run and actual results.** Clean Ninja configure/build with no
warnings. `ctest --test-dir build --output-on-failure` → **44 tests, 42 passed,
2 skipped** (the same pre-existing `server-massleave-test`/
`server-udp-probe-test` environmental skips), 359 s; the new
`replay-player-test` passes in 28.5 s. Sanitizer build (`build-asan`,
`-fsanitize=address,undefined`,
`ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1`) →
`replay-player-test` passes clean (leak detection omitted as unsupported on this
macOS/arm64 session, per R2's recorded platform limitation).

**Design deviations from the dispatched spec, each deliberate and explained:**

- **`TotalMs()` falls back to the recording's full span when the sealed
  round's `durationMs` is 0, not only when no round-end record is present.**
  The spec's wording assumed a populated `durationMs`, but R4a's
  `CaptureRoundEnd()` still writes 0 (recorded in the R5b entry). Using it
  literally would make every real recording report a 0:00 total; treating 0 as
  "not populated" and falling back to `lastClock - firstClock` is the only
  reading that yields a usable duration. The fallback is the recording's span
  (not the raw last clock) so it equals `ElapsedMs()` at the end regardless of
  where the clock started.
- **`Load()` rejects a `StepRecord` whose `seatId` is >= the round's
  `playerCount`.** Not in the spec, but `Load()` decodes untrusted bytes and
  `SetPlaybackControls` indexes `bubbleArrays`; validating at load is cheap and
  makes a hostile/tampered seat id fail safely at load rather than
  mid-playback. (`SetPlaybackControls` also range-guards the index itself.)
- **`Restart()` keeps the current speed** (only the cursor/accumulator/pause/
  desync reset), matching the spec's explicit list.

**Also worth recording for R4d/R4e.** `ReplayPlayer` is deliberately not a
singleton and owns exactly one recording; a later package owns its lifetime and
the menu row that reaches it. There is no seeking/step/checkpoint, no
export/import and no input handling — `Draw()`'s HUD has no tap/keyboard path by
design, so it needs no input-parity checklist entry. The tests use a small
`ReplayPlayerTestAccess` friend (compiled under `FROZEN_BUBBLE_TEST_ACCESS`,
like `BubbleGameTestAccess`/`TTFTextTestAccess`) only to recompute per-step
hashes and assert the cursor; the driving loop itself is `ReplayPlayer`'s own
`AdvanceFrame()`.

**Known failures/blockers:** none.

**Independently verified 2026-09-21 (not just the dispatched agent's own
report above):** read `src/replay_player.h`/`.cpp` in full and traced
`Load`/`AdvanceFrame`/`ConsumeOneGroup`/`SetSpeed`/`Restart`/`Draw`/`DrawHud`/
`ElapsedMs`/`TotalMs` line by line; read `tests/replay_player_test.cpp` in
full and confirmed all 7 scenarios from the dispatched plan are genuinely
present and substantive, including exact `AdvanceFrame()` call-count
assertions for speed determinism (8/4/16/2 at 1x/2x/0.5x/4x, clean since the
fixture's 8-step recording divides evenly by every tested speed); confirmed
the new `SetPlaybackControls()` bridge in `bubblegame_replay.{h,cpp}` is the
only change to that file (bounds-guarded against `MAX_NET_PLAYERS`, same
`BubbleGameReplayAccess` friend every other restore function already uses)
and that `CMakeLists.txt`/`cmake/CoreSources.cmake` register
`replay-player-test` in the exact same shape as the other three replay test
targets. Ran my own clean rebuild (`rm -rf build && cmake -B build -G Ninja
&& cmake --build build --parallel`) and full `ctest --test-dir build
--output-on-failure`: **44 tests, 42 passed, 2 skipped** (same
`server-massleave-test`/`server-udp-probe-test` environmental skips),
matching the agent's report exactly. Ran my own ASan/UBSan pass
(`ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1
ctest --test-dir build-asan --output-on-failure -R "replay"`, no
`detect_leaks=1` per CLAUDE.md's macOS/arm64 note): **6/6 replay tests pass
clean** (`replay-format-test`, `replay-session-test`,
`bubblegame-replay-test`, `replay-recorder-test`, `replay-library-test`,
`replay-player-test`).

**Next concrete step:** **R4d** — the Replays page reached from the CONTROLS &
SETTINGS panel (the title screen's 8 slots are full), with keyboard/gamepad/tap
navigation, visible focus, a footer hint and the confirm dialog for lowering the
keep count; it will call `ReplayPlayer::Load/AdvanceFrame/SetPaused/SetSpeed/
Restart/Draw`.

### 2026-09-21 — R4d: Replays page UI (entry list, playback, keep-count, input parity)

**Date / package:** 2026-09-21, R4d — the fourth slice of R4, the first
package with a real screen a player can reach.

**Starting source revision and branch:** `main` at `61b00290`, working tree
carrying the uncommitted R1-R6 + R4a-c replay work, all uncommitted.

**Changes:** New `src/mainmenu_replays.cpp` (917 lines) — the Replays list
page, the playback viewer, and both confirm dialogs, in their own
translation unit (registered in `cmake/CoreSources.cmake`). Modified, all
within stated scope: `src/mainmenu_internal.h` (`kKeyRowReplays` inserted
directly above `kKeyRowResetAll` in all three platform branches, correctly
renumbered, `kKeyRowLast` untouched); `src/mainmenu.h` (new page state,
`std::unique_ptr<ReplayPlayer> replayPlayer`, method declarations);
`src/mainmenu_input.cpp` (`ReplayPlaybackKey`/`ReplaysPanelKey` checked first
in `HandleInput`; `HandleReplayPlaybackTap`/`HandleReplaysPanelTap` checked
first in `HandlePanelTap`; `KeysPanelKey`'s ENTER dispatch opens the page);
`src/mainmenu_panels.cpp` (full-bleed early-return branch in `Render()` for
playback; `ReplaysPanelRender()` called right after `KeysPanelRender()` so
it owns the shared tap-row state last; new "Replays" row in
`KeysPanelRender()`); `src/platform.h`/`.cpp` (additive, non-`#ifdef`-gated
`PlatformExportReplayFile`/`PlatformImportReplayFile` seam stubs returning
`false`); `src/replay_library.h`/`.cpp` (additive `EvictionCountFor(int)` and
`ApplyKeepCount(int)`, thin wrappers around the pre-existing private
`EvictTo()` -- confirmed `EvictTo()`'s own body and `Write()`'s
eviction-after-write call site are byte-for-byte unchanged);
`tests/replay_library_test.cpp` (new scenario 8 only; the seven pre-existing
scenarios are unmodified). `replay_format.*`/`replay_recorder.*`/
`replay_player.*` untouched.

**Design decisions made and recorded, per the plan's own request:**

- **No new `FrozenBubble::GameState`.** Playback lives entirely inside
  `MainMenu`: `MainMenu::Render()` takes an early full-bleed branch
  (`if (playingReplay && replayPlayer) { ...; replayPlayer->AdvanceFrame();
  ReplayPlaybackRender(); return; }`) that draws nothing else, mirroring live
  gameplay's own "nothing behind it is drawn" shape.
- **Row + sub-action selection reuses `BeginPanelTapRows(selection,
  subSelection)`** exactly as the plan proposed: `replaysSelection` is the
  focused library entry (or one of three trailing library-level rows --
  Export/Import/keep-count, computed as `size()`, `size()+1`, `size()+2` so
  they can never drift from the renderer), `replaysActionIndex` is the
  focused action (0 = Play, 1 = Delete) within an entry row, drawn gold when
  focused.
- **Export/Import are pinned library-level rows, not per-entry actions** --
  resolving the plan's own noted inconsistency (per-row "Play/Export/Delete"
  vs. a library-level Import). Per-row actions are Play/Delete only; Export
  acts on whichever entry is currently selected.
- **Manual per-row Delete gets its own confirm dialog** (not just the
  lowering-keep-count one), reusing the shared `confirmDialogFocusNo` flag --
  exactly the plan's explicit instruction to add this beyond what the
  library's own "Delete just frees a slot" language implied.
- **Keep-count lowering confirm triggers on explicit Apply** (ENTER, or a
  drawn "Apply" suffix button next to the `< N >` stepper), not on the row
  losing focus -- necessary because a `splitAdjust` row's tap halves only
  ever synthesize LEFT/RIGHT, never a commit. Browsing the stepper up and
  back down before Apply deletes nothing (`replaysKeepCountPending` is
  separate from the persisted setting until commit).
- **Playback keys:** SPACE pause/resume, LEFT/RIGHT cycles 0.5/1/2/4x, R
  restart, ESC exit -- each with a drawn tap-button equivalent
  (Pause/Speed/Restart/Exit) and a footer hint. A finished or desynced
  recording does not auto-exit (an explicit way back is required per
  CLAUDE.md's spirit), but ENTER and tap-anywhere both additionally work
  once it has -- a stray ENTER/tap mid-playback does nothing, checked via
  `IsFinished() || IsDesynced()`.
- **`HasAnyPanelOpen()` left unchanged.** The dispatched plan's own claim
  that this function excludes `showingTeamsPanel`/`showingHelpPanel` was
  stale against current source -- both are in fact already included, and so
  is `showingKeysPanel`, which stays `true` underneath the Replays page and
  playback the whole time they're open. The quit-confirmation gate this
  function drives is therefore already correct with no change needed; this
  is a correction to the plan text, not a deviation in the implementation.

**Keep-count-0 inconsistency, resolved 2026-09-21.** The project owner
confirmed the plan's original intent: lowering the keep count to 0 must only
stop future recording, never delete existing replays. Fixed directly (not
redispatched, since the fix is small and localized):
`ReplayLibrary::EvictionCountFor(keep)`/`ApplyKeepCount(keep)`
(`src/replay_library.h`/`.cpp`) now treat `keep <= 0` as "evict nothing"
rather than "evict everything" -- `EvictTo()` itself (the eviction ordering
both call into) is untouched. `tests/replay_library_test.cpp`'s scenario 8
updated: `EvictionCountFor(0)`/`EvictionCountFor(-3)` now assert `0` (were
`5`, i.e. the whole library), plus two new assertions that
`ApplyKeepCount(0)`/`ApplyKeepCount(-1)` leave all 5 existing files in place.
`src/mainmenu_replays.cpp`'s keep-count sidebar copy corrected from "0 stops
future recording **and deletes every saved replay**" to "...; replays
already saved are kept" -- the confirm-dialog code itself needed no change,
since `CommitReplaysKeepCount()` only shows the delete-confirm when
`EvictionCountFor(target) > 0`, which is now never true for a 0 target, so
lowering to 0 applies immediately with no confirm and no deletion. Verified:
clean rebuild of `frozen-bubble-sdl3` + `replay-library-test`, that binary
run standalone with the corrected scenario 8 passing, then a full
`ctest --test-dir build --output-on-failure` -- **44 tests, 42 passed, 2
skipped**, the same environmental skips as every prior entry.

**Commands run and actual results (independently, not just the dispatched
agent's report):** `git status`/`git diff` confirmed the changed-file set
above and that no commits exist (`git log` still `61b00290`). Read
`src/mainmenu_replays.cpp` in full (917 lines) and every diff hunk in
`mainmenu.h`/`mainmenu_input.cpp`/`mainmenu_panels.cpp`/
`mainmenu_internal.h`/`platform.h`/`.cpp`/`replay_library.h`/`.cpp`, and the
new test scenario 8; traced the tap-dispatch fallthrough
(`HandleReplaysPanelTap` intentionally returns `false` to let the shared
generic `panelTapRows` loop finish the select-then-activate dance for rows
it doesn't own directly) and confirmed it is sound. Clean rebuild
(`rm -rf build && cmake -B build -G Ninja && cmake --build build
--parallel`): zero warnings/errors from any R4d file (one pre-existing,
unrelated warning in `tests/touch_letterbox_test.cpp`). Full
`ctest --test-dir build --output-on-failure`: **44 tests, 42 passed, 2
skipped** (`server-massleave-test`/`server-udp-probe-test`, the same
environmental skips as every prior entry). ASan/UBSan
(`ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1
ctest --test-dir build-asan --output-on-failure -R "replay"`): **6/6 replay
tests pass clean**.

**Manual/visual checks performed and still outstanding:** **not performed in
this session.** The plan explicitly requires a keyboard-only and tap-only
click-through of open/list/Play/Delete-confirm/keep-count-confirm, unlike
R4a-c which needed none. This session has no way to drive the real SDL
window: the native macOS build is a bare executable (confirmed via
`CMakeLists.txt` -- `MACOSX_BUNDLE TRUE` is set only under `if(IOS)`), and a
bare Mach-O launched from a terminal does not register with macOS
LaunchServices, so bundle-scoped computer-use automation cannot attach to
it -- the same limitation already recorded against this exact binary in the
R1a entry above ("a bare Mach-O launched from Terminal doesn't register with
macOS LaunchServices... Playing it directly... should be the default for
this check going forward"). **This needs either the project owner playing
the freshly built `./build/frozen-bubble-sdl3` directly (keyboard-only:
CONTROLS & SETTINGS -> Replays -> navigate/Play/Delete/keep-count; then the
same flows by mouse click, which stands in for touch) or a future session
with real display/computer-use access.** Code correctness, build and
automated-test verification are complete; UI/UX correctness as actually
experienced is not yet confirmed.

**Design changes and reasons:** covered inline above (Export/Import as
pinned rows, keep-count Apply-triggered confirm, manual-delete confirm,
`HasAnyPanelOpen()` correction). No change to `replay_format.*`/
`replay_recorder.*`/`replay_player.*`'s tested behavior.

**Known failures/blockers:** the keep-count-0 inconsistency above is fixed
and verified. Only the outstanding manual verification pass remains (needs
the owner or a future session with display access, not a bug in the code as
verified).

**Next concrete step:** get the manual verification pass done (by the
project owner or a future session with display access) before starting
**R4e** (desktop `SDL_ShowSaveFileDialog`/`SDL_ShowOpenFileDialog` and WASM
Blob-download/file-input export/import bridges, filling in the
`PlatformExportReplayFile`/`PlatformImportReplayFile` seam this package
added) -- R4e builds directly on top of this package's UI, so confirming the
UI itself works first avoids compounding an unverified layer.

### 2026-09-21 — Bug fix: replay playback showed no bubbles (found via the
owner's own manual test pass)

The owner did the manual click-through R4d's own entry above called
outstanding, and it surfaced a real bug on the very first try: opening a
replay of a local (non-network) 2-player round from the Replays page showed
an empty board -- no bubbles at all. Root cause was **not** in
`bubblegame_replay.cpp`'s capture/restore engine, despite that being the
obvious first suspect: every existing `playerCount == 2` scenario across
`tests/bubblegame_replay_test.cpp`, `replay_recorder_test.cpp`,
`replay_player_test.cpp` and `replay_session_test.cpp` happened to use a
*network* setup, so a genuinely local 2-player round had never been driven
through capture -> restore end to end by any test in this project. A
throwaway diagnostic (a real local 2P round captured live, then restored via
`RestoreRoundStart()` into a fresh `BubbleGame`, inspecting `bubbleMap`
directly) proved the engine was fine: 37 of 97 slots non-empty on both
seats, `boardVisible` true, identical live/replay hash sequences.

The actual cause was in `BubbleGame::NewGame()` (`src/bubblegame.cpp`,
~line 1116): it unconditionally executed
`FrozenBubble::Instance()->currentState = MainGame;` plus an
`SDL_PumpEvents()`/`SDL_FlushEvents()` input-queue flush, regardless of which
`BubbleGame` instance `NewGame()` ran on. `RestoreRoundStart()` (called by
`ReplayPlayer::Load()`/`Restart()`) calls `NewGame()` on its own throwaway
`BubbleGame` -- never `FrozenBubble::mainGame`, the real singleton the
top-level render/input dispatch (`frozenbubble.cpp`'s
`if (currentState == TitleScreen) mainMenu->Render(); else if
(currentState == MainGame) mainGame->Render();`) actually looks at. Opening
*any* replay therefore flipped the app's top-level state machine to
`MainGame` the instant `RestoreRoundStart()` ran, so the very next frame
rendered the real, untouched, empty `mainGame` singleton instead of
`MainMenu`'s `ReplayPlaybackRender()` overlay -- "no bubbles" was that
singleton's never-started board, not the correctly-restored replay board,
and (though the owner didn't report this symptom) the replay's own control
bar/HUD would have vanished the same frame for the same reason, since
`MainMenu::Render()` no longer runs once `currentState != TitleScreen`.

**Fix:** gated the `currentState` assignment and the input-flush block on
`sessionMode == SessionMode::Live` (`src/bubblegame.cpp`).
`RestoreRoundStart()` already sets `sessionMode` to `SessionMode::Playback`
*before* calling `NewGame()` (it has to, for other reasons -- see R4a's
Playback-gate note), so this is exactly the same signal
`EffectsEnabled()`/the network-send guards already use elsewhere in this
codebase to keep a playback instance's side effects off the live app. A live
`NewGame()` (the real "Start" flow) still behaves exactly as before.

**Test coverage added:** a new scenario in
`tests/bubblegame_replay_test.cpp` (previously the file's only
`playerCount == 2` local, non-network scenario at all) captures a real local
2P round live, then separately: (a) asserts both seats' starting boards have
`nonEmpty > 0` bubbles, (b) restores into a fresh instance with
`FrozenBubble::Instance()->currentState` forced to `TitleScreen` first (the
state the app is actually in while the Replays page is open) and asserts it
is *still* `TitleScreen` after `RestoreRoundStart()` -- the regression this
bug was -- and (c) still runs the existing `RunAndCompare()` hash/position
comparison every other scenario in this file runs. All three would have
failed before the fix (well, (a)/(c) would have passed -- the engine was
always fine -- but (b) is the one that pins the actual bug).

**Verification:** clean rebuild (`cmake --build build --parallel`), full
`ctest --test-dir build` (44/44, 42 passed + 2 environmental skips, matching
every prior run), and the new scenario specifically re-run standalone
(`./build/bubblegame-replay-test`) to confirm it exercises the fix rather
than passing by coincidence.

**Not yet done:** the owner has not yet re-tested the actual UI (Replays
page -> Play) after this fix landed -- this fix is verified at the engine
level (the exact mechanism the bug worked through, `RestoreRoundStart()` ->
`NewGame()` -> `FrozenBubble::currentState`), but the full manual
click-through R4d's entry above asked for is still outstanding, now on top
of this fix rather than instead of it.

### 2026-09-21 — Bug fix: Windows client can't see the public server list

Unrelated report from the same session, surfaced mid-investigation of the
replay bug above: the Windows client never lists `fb.servequake.com` (or any
other public server). Cause: `NetworkClient::FetchPublicServers()`,
`DetectGeoLocation()` and `DetectCountry()` (`src/networkclient.cpp`) all
fetch their URLs by shelling out to `curl` via `popen()`, using a command
string built for a POSIX shell -- single-quoted URL, `2>/dev/null` for
stderr. `popen()` on Windows always runs its command through `cmd.exe`,
never a POSIX shell: `cmd.exe` quotes with `"`, not `'`, and has no
`/dev/null`. So on Windows either curl received the URL wrapped in literal
stray `'` characters, or the whole command line failed outright at the
redirection before curl ever ran (`/dev/null` doesn't resolve as a path on
Windows) -- either way, zero servers came back, silently, with no error
surfaced anywhere a player would see it. This is a genuine desktop-Windows
regression, not new: this `popen("curl ...")` pattern predates this
session's replay work and was never platform-gated for the three desktop
targets (Linux/macOS have a real POSIX shell under `popen()`; Windows never
did).

**Fix:** extracted a `buildCurlCommand()` helper (`src/networkclient.cpp`,
just above `curlFetch()`) that builds the command string per platform --
`_WIN32` gets double-quoted URL + `2>NUL`, everything else keeps the
original single-quote + `/dev/null` form unchanged. All four call sites
(`curlFetch()`, `DetectGeoLocation()`, `DetectCountry()` x2) now go through
it instead of duplicating the `snprintf` inline, so there is exactly one
place this can drift again. No behavior change on Linux/macOS (same command
string, byte for byte).

**Verification:** clean rebuild succeeds (this machine is macOS, so the
`_WIN32` branch itself is compile-checked by inspection and by matching the
existing `#ifdef _WIN32` conventions already used elsewhere in this same
file, not by an actual Windows build in this session); full `ctest` suite
still 44/44. **Not yet verified on an actual Windows machine or CI's Windows
build** -- that verification is outstanding, same caveat as the replay UI
click-through above.

**Shipped 2026-09-21:** merged via PR #124, CI-green on all platforms
(Windows build compiles clean), released as `v2.4.106`. Runtime confirmation
on an actual Windows client fetching from `fb.servequake.com` is still
outstanding -- CI proves the branch compiles, not that a real Windows client
now sees the server list.

### 2026-09-21 — R4e: desktop and WASM replay export/import (real file dialogs)

**Date / package:** 2026-09-21, R4e -- the fifth and final slice of R4,
filling in the `PlatformExportReplayFile`/`PlatformImportReplayFile` seam
R4d shipped as an unconditional-`false` stub.

**Starting source revision and branch:** `main` at `e18e00e7` (after the
Windows curl fix and its unrelated CI flaky-test-exclusion follow-up were
merged/released as `v2.4.106`), working tree carrying the uncommitted R1-R6
+ R4a-d replay work, still all uncommitted.

**The seam had to change shape, not just get a body.** `SDL_ShowSaveFileDialog`/
`SDL_ShowOpenFileDialog` are asynchronous and callback-based (confirmed
directly against the vendored `SDL_dialog.h`), so R4d's synchronous
bool-returning stub could not wrap them. R4e replaced it with a two-phase
`Begin()`/`Poll()` pair per operation plus a shared `PlatformFileOpStatus`
enum (`Idle`/`Pending`/`Succeeded`/`Cancelled`/`Failed`), declared
un-`#ifdef`-gated in `src/platform.h` with per-platform bodies in
`src/platform.cpp`.

**One design deviation from the dispatched plan, found and fixed before any
code was written:** the plan's desktop design read the dialog's parent
window from `FrozenBubble::Instance()->window`, but `CMakeLists.txt`'s
`ttftext-cache-test` target compiles `src/platform.cpp` standalone --
linking only SDL3/SDL3_image/SDL3_ttf, not the core library `FrozenBubble`
lives in -- so that call would have broken that test target's link. Fixed
by threading an `SDL_Renderer *renderer` parameter through
`PlatformExportReplayFileBegin`/`PlatformImportReplayFileBegin` instead,
resolved to a window via `SDL_GetRenderWindow()` falling back to
`SDL_GetKeyboardFocus()` -- the same pattern this file's own
`SetTextInputAreaLogical()` already uses for the identical problem.
Independently confirmed: `ttftext-cache-test` passed in the full suite run
below.

**Changes:** `src/platform.h` -- replaced the R4d stub declarations with the
`Begin()`/`Poll()` pair and `PlatformFileOpStatus`; added
`FROZEN_BUBBLE_TEST_ACCESS`-gated test hooks (`WriteReplayBytesToPath`,
`ReadReplayBytesFromPath`, `TestSimulateExportDialogResult`,
`TestSimulateImportDialogResult`, `TestResetPlatformReplayFileState`,
`testReplayFileOpsHeadless`), following the existing
`testForceDiscordInviteOff` free-function test-seam convention.
`src/platform.cpp` -- desktop branch (`SDL_ShowSaveFileDialog`/
`SDL_ShowOpenFileDialog`, a mutex-guarded `PendingFileOp` struct per
operation, a `static` callback since SDL documents it may run off the
calling thread); WASM branch (`EM_JS` Blob-download for export, a hidden
`<input type=file>` + `FileReader` bridge for import via
`OnReplayImportAlloc`/`OnReplayImportFileReady`/`OnReplayImportCancelled`
`EMSCRIPTEN_KEEPALIVE` exports, with both the `input`'s `cancel` event and a
window-`focus`+300ms-timeout fallback covering picker dismissal since
`cancel` isn't universally supported); Android/iOS branch unchanged from
R4d's stub, byte-identical behavior. `src/mainmenu.h` -- added
`replayExportPending`/`replayImportPending` flags and a
`PollReplayFileOps()` declaration. `src/mainmenu_replays.cpp` --
`ExportSelectedReplay()`/`ImportReplay()` now call `Begin()` and set a
pending flag instead of branching synchronously;
`PollReplayFileOps()` (called first thing in `ReplaysPanelRender()`, before
anything reads `replaysStatus`) resolves a terminal `Poll()` result, writes
a successful import to `ReplayLibrary`, refreshes the entry list, and clears
the pending flag; `ActivateReplayRow()` refuses to re-trigger Export/Import
while one is already pending (belt-and-suspenders alongside `Begin()`'s own
in-flight refusal). `CMakeLists.txt` -- registered `platform-replay-file-test`
following the exact `replay-library-test`/`replay-player-test` pattern. New
`tests/platform_replay_file_test.cpp`. No change to `replay_library.*`/
`replay_player.*`/`replay_recorder.*`/`replay_format.*`.

**Findings recorded during dispatch/verification:**

- No WASM `ASYNCIFY`/pthreads flags exist anywhere in `CMakeLists.txt` or
  `.github/workflows/build.yml` -- confirmed by grep -- so the WASM build is
  genuinely single-threaded and the mutex there is defensive uniformity with
  desktop, not a strict requirement.
- WASM import allocates via an exported `OnReplayImportAlloc()` rather than
  calling `Module._malloc` directly from JS, because `_malloc`/`_free` are
  not in this project's `EXPORTED_FUNCTIONS` list and adding them was out of
  this package's scope.
- The 16 MiB import size cap (`kPlatformReplayImportMaxBytes`/
  `kReplayImportMaxBytes`) is deliberately far above `replay_format.h`'s
  1 MiB `kMaxPayloadLength`, which bounds a single *record*, not a whole
  `.fbr` file -- a real multi-step recording legitimately exceeds 1 MiB, so
  reusing that constant here would have rejected valid imports.

**Commands run and actual results (independently, not just the dispatched
agent's report):** `git status`/`git diff` confirmed the changed-file set
above (only `tests/platform_replay_file_test.cpp` is new; everything else
already had uncommitted R4d-era edits, consistent with an additive R4e
diff). Read `src/platform.h`/`.cpp` in full for the changed sections,
`src/mainmenu.h`/`mainmenu_replays.cpp`'s new/changed functions, and the
entire new test file (round-trip, oversized/missing-file rejection, full
export/import state-machine coverage including re-entry refusal and
terminal-result-consumed-exactly-once, and a genuine cross-thread completion
exercise firing the simulated dialog callback from a spawned `std::thread`
while the main thread polls). Clean native rebuild + full
`ctest --test-dir build --output-on-failure`: **100% tests passed, 45
total (43 run, 2 skipped)** -- the same `server-massleave-test`/
`server-udp-probe-test` environmental skips as every prior entry;
`platform-replay-file-test` and `ttftext-cache-test` (the target the
renderer-parameter fix specifically protects) both passed. ASan/UBSan
(`ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1
ctest --test-dir build-asan --output-on-failure -R "replay|platform"`):
**9/9 pass, no sanitizer findings**, including the cross-thread test.

**WASM build/click-through: attempted, not completed.** Unlike every prior
R4 package, this is the one platform this session could plausibly verify
visually via the built-in browser tooling rather than deferring to the
project owner -- so real effort went into it. Got substantially further
than the dispatched agent (which had no working Emscripten toolchain at
all): fixed this machine's Homebrew Emscripten config (`BINARYEN_ROOT`/
`LLVM_ROOT` were unset), applied this project's `tools/ports/sdl3_image.py`/
`sdl3_mixer.py` patches into the local Emscripten install exactly as CI
does (`web/README.md`'s documented prerequisite), and got a full SDL3/
FreeType/HarfBuzz port rebuild to complete -- but the final compile failed
on an internal Emscripten API mismatch (`SDL3_IMAGE_FORMATS` read while in
"limited settings mode") between Homebrew's packaged Emscripten 6.0.9 and
the port files, which are written against upstream `emsdk` (what CI actually
uses via `version: 'latest'` in `.github/workflows/build.yml`). No upstream
`emsdk` checkout exists on this machine, and bootstrapping one is a real
toolchain install, not a quick fix. **This is a local-environment gap, not
an R4e defect** -- the C++ side (`EM_JS`/`EMSCRIPTEN_KEEPALIVE` bridge) was
read in full and follows this file's existing idioms correctly, and CI's
own WASM job (using real `emsdk`, not Homebrew) is what actually gates
whether this compiles for real. Project owner confirmed accepting this
level of verification and moving on rather than continuing to chase the
local toolchain.

**Manual/visual checks performed and still outstanding:** desktop native
click-through (open Replays, Export to a chosen path, Import it back) --
**not performed**, same bare-Mach-O/LaunchServices limitation recorded
against every prior package's native check. WASM click-through -- attempted
this session (see above), not completed; next opportunity is either a real
`emsdk` toolchain locally, or this package's first real CI WASM build once
committed.

**Design changes and reasons:** the `SDL_Renderer*` parameter threaded
through both `Begin()` functions (see deviation above) is the only design
change from the dispatched plan; everything else matches.

**Known failures/blockers:** none in the implementation itself. Both
click-through verifications (desktop and WASM) remain outstanding, same
category of gap as R4d's own unresolved manual check until the owner's pass
found the real state-hijack bug -- worth keeping in mind that R4d's own
"code review says it's fine" confidence was wrong on the first real click,
so this package's correctness should be treated as provisional until either
verification actually happens.

**Next concrete step:** R7 (mobile Android/iOS document-picker export/import
bridges, per-platform qualification) is next in the plan's own order. R4 as
a whole -- R4a through R4e -- is now fully implemented and independently
verified at the code/automated-test level; only manual click-through
(desktop and WASM) remains open across the whole package, unchanged in kind
from the gap already flagged after R4d.

### 2026-09-21 — R7: Android replay export/import + platform-compatibility gate

**Scope decisions made with the project owner before dispatch:** iOS is
dropped from this package entirely -- it's a real, locally-buildable target
(Simulator-verified, per earlier sessions) but has zero CI coverage, is
unsigned, has never run on physical hardware, and separately this app
cannot ship on the App Store at all right now
([[ios-appstore-license-blocker]], a licensing blocker unrelated to
engineering) -- no point building distribution-facing functionality for it
today. The platform-compatibility gate (`ReplayHeader::platformFloatProfile`,
previously reserved but always written `0` and never checked) is added now
rather than deferred, using a conservative default: unknown/mismatched
profiles are refused, not silently played; real empirical cross-hardware
fixture testing is out of reach for this session and stays a manual
follow-up, the same posture already accepted for R4d's and R4e's own
outstanding click-throughs.

**Research finding that shrank the Android piece below what the plan
originally assumed:** the vendored `android/app/jni/SDL3` already ships a
complete Storage Access Framework document-picker bridge
(`SDL_androiddialog.c` -> `SDL_android.c`'s `Android_JNI_OpenFileDialog` ->
`SDLActivity.java`'s `showFileDialog()`/`onActivityResult()`), wired through
the exact same `SDL_ShowSaveFileDialog`/`SDL_ShowOpenFileDialog` API R4e
already used for desktop. No new Java/Kotlin code and no new JNI was
needed -- confirmed directly against that vendored source and against
`AndroidManifest.xml` (SAF's `ACTION_OPEN_DOCUMENT`/`ACTION_CREATE_DOCUMENT`
need no declared permission at `targetSdk 36`), not assumed from the plan's
own research. The real gap was narrower: the picker hands back `content://`
URIs, which R4e's desktop byte-I/O helpers
(`WriteReplayBytesToPath`/`ReadReplayBytesFromPath`, `<fstream>`-based)
cannot open.

**Implementation (`src/platform.cpp` only, for the Android piece):** the old
combined `#if defined(__ANDROID__) || defined(__ANDROID_PORT__) ||
defined(__IOS_PORT__)` stub branch split into its own real
`#if defined(__ANDROID__) || defined(__ANDROID_PORT__)` branch, mirroring
desktop's `PendingFileOp`/`ExportDialogCallback`/`ImportDialogCallback`/
`SDL_Show*Dialog` shape exactly, with byte I/O done via `SDL_SaveFile()`/
`SDL_LoadFile()` (thin wrappers over `SDL_IOFromFile()`, whose Android
backend resolves a `content://` URI through
`Android_JNI_OpenFileDescriptor()` -- confirmed by reading the vendored
`SDL_iostream.c` before picking this over the raw `SDL_IOFromFile()` +
`SDL_WriteIO()`/`SDL_ReadIO()` pair, since the one-call form has identical
behavior and is simpler). The Android branch deliberately omits desktop's
`FROZEN_BUBBLE_TEST_ACCESS` headless-test bypass and hooks, since Android
never builds with that macro (it's defined only by the root `CMakeLists.txt`
test target; Android builds go through the separate
`android/app/CMakeLists.txt`). iOS's branch (`#elif defined(__IOS_PORT__)`)
was independently confirmed byte-for-byte unchanged from before this
package (read in full: still the same unconditional `false`/`Idle` stub).
One small doc-comment fix alongside the main change: a stale comment above
the `#if` chain that said "Android/iOS have no bridge yet" was updated to
describe Android's new real bridge while keeping iOS's description accurate
-- confirmed via diff that only that comment block changed.

**Implementation (compatibility gate, `src/replay_format.h/.cpp`,
`src/replay_recorder.cpp`, `src/replay_library.h/.cpp`,
`src/mainmenu_replays.cpp`):**
`ComputeCurrentPlatformFloatProfile()` resolves via preprocessor checks
(`__EMSCRIPTEN__` first, since Emscripten also defines `__linux__`/
`__unix__`; then `__ANDROID__`+arch, `__APPLE__`+arch, `_WIN32`+arch,
`__linux__`+arch) to one of 8 fixed, stable-on-disk profile IDs (Linux
x86_64=1 .. WASM=8), falling back to `0` ("no gate", by design -- see the
header's own comment on why an unrecognized platform's real risk being
uncaught is preferable to a false-positive refusal) for anything else.
`IsReplayPlatformCompatible()` treats `0` and an exact match as compatible.
Wired into capture right next to the existing `buildFingerprint` line in
`replay_recorder.cpp`'s `OnRoundStart`. Wired into consumption via a new
`ReplayLibrary::ReplayEntry::platformIncompatible` bool (set during
`ReadEntryMetadata()`'s existing per-file header decode, mirroring the
`imported` field already there) and `mainmenu_replays.cpp`'s
`[INCOMPATIBLE]` row badge, a disabled/unreachable Play action for such a
row (keyboard/gamepad focus is force-clamped off it in
`ClampReplaysSelection()`; `ActivateReplayRow()` also refuses to invoke
playback on one as a second, defense-in-depth check; `BeginReplayPlayback()`
itself refuses a third time with its own status message, so a flagged entry
truly cannot be played through any path), and a distinct
"Imported, but this replay was recorded on a different platform..." status
line in `PollReplayFileOps()`'s import-success branch -- which still always
writes the file (an explicit player Import action is never silently
dropped), peeking the header before `ReplayLibrary::Write()` consumes the
bytes. A flagged file is never skipped by `List()` the way a genuinely
corrupt one is -- it lists, badged, so the player can see and delete it.

**Tests (all additive, no existing assertion touched):**
`tests/replay_format_test.cpp`'s new `TestPlatformFloatProfile()` (stable
across repeated calls, nonzero on this build, self-compatible, legacy-`0`
compatible, a different nonzero profile incompatible);
`tests/replay_recorder_test.cpp`'s new scenario 7 (a real captured round's
decoded header has `platformFloatProfile == ComputeCurrentPlatformFloatProfile()`,
not `0`); `tests/replay_library_test.cpp`'s new scenario 9 (one fixture
written with the real current profile, one with a different nonzero
profile; `List()` flags exactly the mismatched one, both still appear in
the list, newest-first). Android's `SDL_SaveFile()`/`SDL_LoadFile()`-based
byte I/O gets no `ctest` coverage, same limitation R4e's WASM bridge already
hit -- verified only via a clean `gradlew assembleRelease` plus code review,
per the plan's own stated boundary (no Android emulator/device driving tool
in this session).

**Independent verification performed this session (not just the agent's own
self-report):** read every changed/new file in full
(`src/replay_format.h/.cpp`, `src/replay_recorder.cpp`,
`src/replay_library.h/.cpp`, `src/mainmenu_replays.cpp`, `src/platform.cpp`'s
Android branch and the iOS branch for byte-for-byte-unchanged confirmation,
and the three additive test files) and confirmed every function signature,
wiring site, and test assertion actually matches what was claimed. `git
status --short` confirmed no files outside the intended set were touched, no
new files, nothing committed/pushed. A genuinely clean, from-scratch native
rebuild (`rm -rf build && cmake -B build -G Ninja && cmake --build build
--parallel`) succeeded. Full `ctest --test-dir build` --
**100% passed, 45 total (43 run, 2 environmental skips -- the same
pre-existing `server-massleave-test`/`server-udp-probe-test` skips as every
prior package, nothing new)**. A fresh ASan/UBSan build
(`cmake --build build-asan --parallel`) succeeded clean, and
`ASAN_OPTIONS=fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1
ctest --test-dir build-asan -R "replay|platform"` -- **9/9 passed**
(`replay-format-test`, `replay-session-test`, `bubblegame-replay-test`,
`replay-recorder-test`, `replay-library-test`, `replay-player-test`,
`platform-replay-file-test`, `list-platform-parse-test`,
`server-platform-input-test`). Android verification was made genuinely
independent rather than trusting the agent's own build: `src/platform.cpp`
was `touch`ed to force a real recompile, then `cd android && ./gradlew
assembleRelease` was re-run -- confirmed via `.o` file timestamps that all
three ABIs (arm64-v8a, armeabi-v7a, x86_64) actually recompiled
`platform.cpp` fresh, with **BUILD SUCCESSFUL** and no warnings or
diagnostics on the changed file.

**Known gaps, same posture as every prior package:** Android
device/emulator click-through (export a real replay via the SAF picker,
import it back) is an explicit manual follow-up -- no Android
emulator/device driving tool is available in this session (unlike the iOS
Simulator tool this environment does have), so this is a hard boundary, not
a shortcut, exactly as the dispatch plan called it out in advance. This
joins R4d's outstanding desktop native click-through and R4e's outstanding
WASM click-through as the three manual verification gaps carried forward
across the whole replay feature.

**Next concrete step:** R8 (optional checkpoints/seek), the last package in
the plan's own order. With R7 done, the entire replay feature
(R1 through R7) is code-complete and independently verified at the
code-review/automated-test level; nothing has been committed yet, and the
three manual click-through gaps (desktop native, WASM browser, Android
device) remain open regardless of what R8 adds.

### 2026-09-21 — R8: replay seek / shot navigation

**Scope decision made before dispatch:** the plan's own row calls
checkpoints "optional." Re-inventorying `ReplayPlayer` (`src/replay_player.h/.cpp`)
showed real on-disk `Checkpoint` records (`RecordType::Checkpoint = 5`,
reserved in `replay_format.h` but never implemented) are not needed for
correct seeking: `Load()` already decodes an entire recording into memory
(`groups_`, one entry per recorded step), so a seek to any index can be
"rebuild the owned `BubbleGame` from scratch (`Restart()`'s existing
pattern) and replay every step from 0 up to the target via the same
`ConsumeOneGroup()` forward playback already uses" -- which makes a seeked
state byte-identical to sequential playback at that step by construction
(the acceptance criterion), with zero new hashing/comparison logic, and
inherits desync detection for free (a mismatch mid-seek stops the catch-up
early, same as it stops `AdvanceFrame()` today). Checkpoints stay deferred
as an unimplemented performance optimization for recordings much longer
than R4a's transient capture budget currently allows, which is not a
documented problem today. A full drag-able arbitrary-position scrub bar was
also deliberately not built -- only two shot-navigation buttons/keys are
wired to UI in this package; `SeekToStepIndex()` itself is a general
primitive a future package could expose more UI for.

**"Shot" definition:** since the codec carries no dedicated shot event, a
step group counts as a shot when some seat's `StepRecord::fire` transitions
from false (or unseen) to true relative to that seat's immediately
preceding recorded state. Computed once at the end of a successful `Load()`
into `shotIndices_` (`std::vector<size_t>`), tracking every seat's fire
state every group (not breaking early out of the inner loop once one seat's
edge is found, which would have left later seats' state stale for the next
group's comparison -- called out explicitly in the dispatch prompt as a bug
to avoid, and confirmed absent in the delivered code).

**A real gap found and fixed while designing this (not discovered mid-implementation):**
ordinary forward playback (`AdvanceFrame()` -> `ConsumeOneGroup()` ->
`game_->AdvancePlaybackStep()`) re-executes the same production shooter/
board/state simulation code a live round does, including its unconditional
`PlaySFX(...)` calls (`bubblegame_shooter.cpp`, `bubblegame_board.cpp`,
`bubblegame_state.cpp` -- none gated on `SessionMode`). This is correct
during ordinary 1x-4x playback (a replay viewer should hear the pops), but
a naive rebuild-and-replay-to-target seek would, without suppression,
re-fire every discarded intermediate step's sound too -- exactly what the
plan row's "does not repeat sound/result effects" rules out.
`AudioMixer::MuteAll(bool enable = false)`/`IsHalted() const` already
existed and are already used together in this identical save/restore shape
in `bubblegame_input.cpp`'s mute hotkey handler (confirmed by reading it
before reuse) -- no `AudioMixer` API change was needed for the muting
mechanism itself. **One inversion worth recording plainly since it is easy
to get backwards:** `MuteAll`'s bool argument is the opposite of what its
name suggests -- `enable = false` (the default) mutes, `enable = true`
unmutes. The implementer caught this independently (the dispatch prompt's
own pseudocode had it backwards) and I independently re-confirmed it
directly against `audiomixer.cpp` during verification rather than taking
that catch on faith.

**Implementation (`src/replay_player.h/.cpp`):**
`SeekToStepIndex(size_t targetIndex)` -- clamps the target, saves
`AudioMixer::Instance()->IsHalted()`, mutes if not already muted, rebuilds
`game_` exactly as `Restart()` does (bailing out, with the mute state
restored, if the rebuilt instance is not `SessionMode::Playback`), loops
`ConsumeOneGroup()` up to the target or until desync, sets `paused_ = true`
(a seek always lands paused -- jumping to a moment is for looking at it;
resuming is a conscious next action), then restores the mute state.
`SeekToNextShot()`/`SeekToPreviousShot()` scan `shotIndices_` relative to
`cursor_` and call `SeekToStepIndex()` on the nearest match in that
direction; a genuine no-op (state completely unchanged) when there is no
match, deliberately not "jump to the end/start" as a fallback. `HasShots()`
exposes whether `shotIndices_` is non-empty. `Clear()` resets
`shotIndices_` alongside the other members it already resets, so a
failed/replaced `Load()` cannot leak a stale shot list.

**Implementation (UI, `src/mainmenu.h`/`src/mainmenu_replays.cpp`):** two
new rects (`replayPrevShotRect`, `replayNextShotRect`) flank the existing
4-button playback control bar with narrower buttons ("< Shot" / "Shot >"),
recentered so the full 6-button, 592px-wide row still fits the 640px
canvas. `SDLK_COMMA`/`SDLK_PERIOD` drive `SeekToPreviousShot()`/
`SeekToNextShot()` in `ReplayPlaybackKey()`, each with a matching tap block
in `HandleReplayPlaybackTap()` and an updated footer hint -- full
keyboard-and-tap input parity from the first commit of this UI, per
CLAUDE.md's standing checklist for a new interactive element. Gamepad
reachability of the two new keys was checked against the existing five
playback keys and found consistent, not regressed: only LEFT/RIGHT and ESC
were already gamepad-reachable before this package (SPACE and R were not),
so COMMA/PERIOD not being gamepad-reachable either is an existing gap this
package inherits rather than introduces; making any of the six
gamepad-reachable would touch files outside R8's scope
(`gamesettings.h`/`frozenbubble.cpp`) and was correctly left alone.

**Implementation (test hook, `src/audiomixer.h/.cpp`):** a small
`FROZEN_BUBBLE_TEST_ACCESS`-gated `TestSfxPlayCount()`/
`TestResetSfxPlayCount()` counter, incremented in `PlaySFX()` at the one
point past every early-return guard (`mixerEnabled`/`canPlaySFX()`/
`haltedMixer`) where a sound would actually become audible -- confirmed by
reading `PlaySFX()`'s complete body directly rather than assuming its
structure. This exists so the mute-during-seek behavior has real automated
coverage instead of being eyeballed, matching this project's established
"scope what's testable narrowly rather than skipping" posture (R4e's own
WASM-bridge test-coverage gap took the same stance in the other direction,
flagging what could not be tested rather than silently skipping it).

**Tests (`tests/replay_player_test.cpp`, additive scenarios 8-11; the 7
existing scenarios untouched):** scenario 8 (forward seek lands at the
exact target cursor, undesynced, paused, and its hash matches the live
capture's recorded assertion for that step); scenario 9 (seek backward
after driving forward past the target reproduces both the live capture's
hash and a second, freshly-loaded player driven straight to that index --
proving the rebuild path is not reusing stale forward-run state); scenario
10 (a fixture with fire rising at exactly steps 2/5/9 walks
`SeekToNextShot()`/`SeekToPreviousShot()` through that exact sequence,
becoming a true no-op past either end, plus a zero-shot fixture asserting
`!HasShots()` and complete no-ops both directions); scenario 11 (a forward
seek across a span with a real stick/launch event asserts
`AudioMixer::TestSfxPlayCount() == 0` during the muted catch-up and
`!IsHalted()` after, with a control case driving the identical span via
ordinary unmuted `AdvanceFrame()` asserting the counter *is* nonzero, so
the zero result above is a real assertion and not a fixture that never
plays a sound regardless of muting). All fixtures are genuine live-captured
`BubbleGame` rounds (via the file's existing `BuildSoloRecording`/new
`BuildShotPatternRecording` helpers), not hand-built byte streams, matching
this test file's established fixture convention.

**Independent verification performed this session:** every changed/new
file (`src/replay_player.h/.cpp`, `src/mainmenu.h`, `src/mainmenu_replays.cpp`,
`src/audiomixer.h/.cpp`, `tests/replay_player_test.cpp`) read in full and
checked line-by-line against the approved plan, including personally
re-deriving and re-confirming the `MuteAll` argument-inversion catch
against `audiomixer.cpp` rather than trusting the implementer's report of
it, confirming the shot-detection loop does not have the early-break bug
the dispatch prompt warned against, confirming the new 6-button row's pixel
math actually fits the 640px canvas (592px, centered), and confirming the
new test scenarios build genuine fixtures rather than asserting against
fabricated data. `git status --short` confirmed only the five intended
files (plus the untracked R7 files they extend) show as changed, no new
files, nothing committed/pushed. A genuinely clean, from-scratch native
rebuild (`rm -rf build && cmake -B build -G Ninja && cmake --build build
--parallel`) succeeded. Full `ctest --test-dir build` -- **100% passed, 45
total (43 run, 2 environmental skips -- the same pre-existing
`server-massleave-test`/`server-udp-probe-test` skips as every prior
package, unchanged from before R8)**. A fresh ASan/UBSan build succeeded
clean, and `ASAN_OPTIONS=fast_unwind_on_malloc=0
UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-asan -R
"replay|platform"` -- **9/9 passed**, including `replay-player-test` (50s,
up from R7's ~31s, consistent with the four new scenarios' extra
rebuild-and-replay work).

**Known gaps, same posture as every prior package:** manual click-through
of the two new playback buttons/keys (keyboard, tap, and gamepad if
reachable) is an outstanding follow-up -- no way to drive a real SDL window
in this session, the same hard boundary every prior package's own UI
verification has hit.

**Next concrete step:** none remain in this project's own package list. All
of R1 through R8 are code-complete and independently verified at the
code-review/automated-test level. What remains before this feature could
ship is exclusively verification and process, not engineering: the three
manual click-through gaps carried across the whole feature (desktop native,
WASM in-browser, Android device/emulator), manual click-through of R8's own
two new buttons, and a commit/release decision -- nothing in R1-R8 has been
committed, per the standing instruction to only commit on explicit request.

## Next session

**R1's four seams are implemented and test-passing, but R1 itself should not be
declared closed yet.** All four seams -- `GameplayRng` (R1a), `StepContext`
(R1b), `PlayerControls` (R1c), and the `AdvanceSimulation()`/`Draw()` split
(R1d, sub-slices R1d-i..iv) -- are implemented and test-passing, and the
`SessionMode::Live/Playback` effects boundary is in place. Closing out R1 itself
should wait for whoever is coordinating this project to confirm, particularly
because the accumulated live-multiplayer manual checks (MP/mp_train hurry
warning, malus alerts/chat overlay/attack-flash, round-stats CHAT/BRACKET tap
targets, and R1d-iv's split/HUD/text/`AdvanceSimulation()` output) are still all
outstanding with no screen access used to date -- cover all of them in whichever
future session next plays a real local/network multiplayer round.

**R2 is complete** (versioned snapshot/record codec and validation --
`src/replay_format.h/.cpp`, `tests/replay_format_test.cpp`; see the dated
2026-09-20 entry above for the full byte-layout and design record). It is
codec-only: no real `BubbleGame`/`BubbleArray` state was captured, and
nothing in the engine calls it yet.

**R3 is complete** -- one solo Classic round recorded live and replayed
offline (`src/bubblegame_replay.h/.cpp`, `tests/bubblegame_replay_test.cpp`,
plus the `UpdatePenguin()` Playback branch and the `AdvancePlaybackStep`
split). Real `BubbleGame`/`BubbleArray` state now flows through R2's records,
and the in-memory + decode-from-disk replays match the live run's canonical
state hash at every step across win, loss, hurry auto-fire and varied 0.5x/2x
schedules, with Playback-mode effect suppression asserted. See the dated
2026-09-20 R3 entry above. Solo Classic only -- no multiplayer, bots, other
modes, network, library/UI or seek.

**R5 is complete (R5a and R5b).** R5 was split by R5a's own inventory into the
step-driven local rules (R5a) and the two wall-clock-driven modes (R5b). R5a --
random/predefined levels, chain reactions, 1-5 local players, bots, teams,
AttackMode::On/Canceling, Race, and a round captured after a live `ReloadGame()`
-- is implemented and verified. R5b -- the recorded-game-clock seam plus
training (`mpTraining`) and local Timed mode -- is implemented and verified
(see the dated 2026-09-20 R5a and R5b entries above). Training/Timed coverage is
**round 1 of a match only**; network Timed is accepted as of R6b (see below);
Clear is still rejected; `RoundEndRecord::durationMs` is still not populated.

**R6a is complete** -- steady-state 2-peer network message replay. The parsed
opcode switch moved out of `ProcessNetworkMessages()` into a shared
`ApplyInboundGameMessage()`; the applied inbound payloads are captured per step
in `StepRecord::inboundEvents` (seat 0's record); the round-start codec now
carries real `seatOwned`/`seatIds` and a v3 board blob with per-seat nicknames;
`RestoreRoundStart()` accepts exactly a 2-seat network round (seat 0 local, seat
1 remote, no bots) and never runs `SyncNetworkLevel()`; and playback drains the
recorded payloads with no socket. `lobbyPlayerId`/`playerNickname` restoration
is scoped to network records only, leaving local-round restore exactly as
R5a/R5b left it. `tests/bubblegame_replay_test.cpp` covers
remote fire+stick (plus disk round-trip), malus both directions and a
remote-announced `F`, and rejects 3-seat/owned-remote/bot-hosted network
records. See the dated 2026-09-20 R6a entry above.

**R6b is complete** -- round-start sync, hosted bots and round 2+ continuity.
`RestoreRoundStart()` now accepts 2-5-seat network records with a general
per-seat ownership/isBot rule (seat 0 owned non-bot; every other seat owned iff
a hosted bot). No `bubblegame_level.cpp`/`bubblegame_net.cpp` change was needed:
the round start is a full board snapshot taken after `NewGame()`/`ReloadGame()`
resolves, so `SyncNetworkLevel()`'s generated layout and round 2+ continuity are
captured for free and replayed with no socket. A recorded `wasNetworkLeader` bit
(rules blob, new `0x08` flag) fixes `UpdateTimedRound()`'s Playback
leader/joiner asymmetry for network Timed. `tests/bubblegame_replay_test.cpp`
covers SyncNetworkLevel capture as leader, round 2+ via `ReloadGame()`, a
hosted bot, and the network Timed leader regression. See the dated 2026-09-20
R6b entry above.

**R4 is in progress, split five ways.** `R4a` (production capture wiring, the
`ReplayRecorder` singleton), `R4b` (the on-disk `ReplayLibrary`, eviction,
`Replay:KeepCount`) and `R4c` (the `ReplayPlayer` playback viewer) are complete
and verified -- see the dated 2026-09-21 entries above. Remaining: **R4d** the
Replays page UI and its input parity; **R4e** desktop and WASM export/import.
R6 -- network recording/playback -- is complete (R6a + R6b + R6c), so the
prerequisite R4's completion evidence called for (online rounds auto-saved with
no user action) is satisfiable. R4 is the first user-facing milestone; R7
(mobile export/import bridges and platform qualification) and R8
(checkpoints/seek) follow. See [REPLAY_PLAN.md](REPLAY_PLAN.md)'s R4 row.

**One R4 scoping correction, decided with the project owner 2026-09-20.** The
plan's "Add a **Replays** main-menu row" is not physically available:
`MainMenu`'s constructor builds exactly 8 buttons at `y = 14` stepping 56px,
and the 8th (MENU STYLE, y=406) is already the last step that fits inside the
480px canvas. A 9th row would need every row retightened plus new
`txt_replays_{off,over}.png` art on the Android extractor and WASM preload
lists. **Replays is reached from a new row on the existing CONTROLS & SETTINGS
panel** (`KeyConfigRow`, `mainmenu_internal.h`) instead, which opens the
full-screen Replays page. R4d owns that row.

Suggested prompt to paste into a new session (R4):

> Implement **R4** from docs/REPLAY_PLAN.md (auto-save on round finalize into a
> rolling library with detached capture lifetime and eviction; the Replays
> submenu with play/export and a keep-count setting; the replay player; and
> desktop/WASM replay-file export/import). First read docs/REPLAY_PROGRESS.md in
> full, especially the dated 2026-09-20 R6c entry and the R6a/R6b/R6c completion
> notes in the plan, then inspect the current checkout rather than trusting line
> numbers. R3 built `src/bubblegame_replay.{h,cpp}` (live capture -> in-memory
> record -> play back a fresh `BubbleGame` with per-step hash equality); R5-R6
> widened it to local multiplayer/bots/modes and to network rounds of 2-20
> seats, including late result-tail events and mid-round departures. R4 is UI
> and lifecycle, not new simulation: capture must start automatically at round
> start and finalize/write automatically when the round settles (no Save
> button), independent of game state, surviving a peer-triggered next round;
> the keep count defaults to 5 and persists across restart (0 disables);
> lowering it prompts and deletes exactly the named count, raising it deletes
> nothing. Every Replays-menu row/toggle/confirm must work by keyboard, gamepad
> and tap with visible focus, per the repo's input-parity rules. Run the full
> suite plus the sanitizer build for the new paths; record exact changes/results
> in docs/REPLAY_PROGRESS.md and update the plan's R4 row only when actually
> done.

For subsequent sessions, replace R6 with the next incomplete package and
retain the requirement to update this log. A package can span sessions; record
partial completion explicitly rather than skipping to the next package.

## 2026-09-23: replay-desync bug fix -- danger-blink counters not reset on ReloadGame()

A user reported "the last 2 rounds" of a 9-round network match desyncing on
itch.io/iOS. Diagnosed with the same throwaway-diagnostic method the earlier
`platformFloatProfile` fix used (build `tests/replay_diagnose.cpp`, decode the
user's `.fbr`, step it under `ReplayPlayer` and report the first divergent
group). The header confirmed the file was captured under this build's own
platform bucket, so the divergence was a genuine logic bug rather than
cross-platform float drift.

The tool was extended to re-capture a `RoundStartRecord` from the just-restored
game and diff it byte-for-byte against the original -- `EncodeBoardBlob()`
matched exactly (board/queues/geometry all round-trip correctly), which ruled
out the whole board-reconstruction path and narrowed the search to fields the
canonical hash covers but the blob does not.

Root cause: `DoPrelightAnimation()` (`src/bubblegame_board.cpp`) advances
`framePrelight`/`alertColumn` every simulated frame whenever
`turnsToCompress <= 2`, independent of player input, and both fields are part
of `AppendBoardState()`'s canonical hash. Neither `NewGame()` nor
`ReloadGame()` reset them (unlike their four sibling timing fields --
`explodeWait`/`frozenWait`/`prelightTime`/`waitPrelight` -- which `ReloadGame()`
already resets at every round transition). Live play carries a round's
mid-cycle values into the next round via `ReloadGame()`; `RestoreRoundStart()`
always rebuilds through `NewGame()` on a *freshly constructed* `BubbleGame`,
whose default member initializers silently put both fields back at their
round-1 values. The two paths only disagree when a round actually reached
`turnsToCompress <= 2` before ending -- consistent with only 2 of 9 rounds
desyncing rather than all of them.

Fix: `ReloadGame()` now resets `framePrelight`/`alertColumn` alongside their
four siblings (`src/bubblegame.cpp`). Regression test added to
`tests/bubblegame_replay_test.cpp` ("reloadgame-prelight-carryover"): forces
both counters away from their defaults before a mid-match `ReloadGame()`
transition and asserts (a) they read back reset and (b) the existing live-vs-
restored per-step hash comparison harness still matches. Verified the test
fails at step 0 without the fix (reproducing the exact "first frame, no shot
fired" desync signature) and passes with it.

The diagnostic tool and its CMake targets (native and a WASM/Node variant
added to test a WASM-captured file under its own `platformFloatProfile`
bucket) were throwaway, per the same pattern as the earlier fix, and have been
deleted.

## Handoff entry template

```text
Date / package:
Starting source revision and branch:
Changes and current commit(s), or explicitly uncommitted:
Completed acceptance criteria:
Commands run and actual results (including skips):
Manual/platform checks performed and still outstanding:
Design changes and reasons:
Known failures/blockers:
Next concrete step:
```

Keep this checkpoint current and append milestone entries below the initial
review. Keep durable design decisions in the plan; avoid copying a second plan
into each log entry.
