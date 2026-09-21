# Frozen Bubble recording and playback plan

Proposed 2026-09-20 against `61b00290ef264dbcca5135e9a50e4227d939cbad`.
**Research and planning only; no replay implementation exists yet.**
See [source findings](REPLAY_RESEARCH.md) and [current progress](REPLAY_PROGRESS.md).

## Recommended approach

**Confirmed product scope, 2026-09-20:** every completed round is
**automatically saved as a replay** — there is no Save button or other user
action to save one. The device keeps a **rolling library of the most recently
completed rounds, 5 by default**; when a round finishes and the library is
already full, the oldest saved replay is evicted to make room. **The keep
count is a user setting**, adjustable up or down from the Replays submenu and
persisted like other preferences. The Replays library lets the player pick any
entry to **Play** or **Export** (to a `.fbr` file), and **Delete** to free a
slot early. This feature is entirely client-side. There is no replay upload,
account, hosting service, server recorder or server/protocol change. Online
matches are still recordable locally; “client-side” describes where the
recording lives, not a restriction to offline matches.

Build a client-side, offline replay system using **a round-start snapshot,
recorded simulation steps, normalized controls, and state checks**. Use one solo
Classic round as a technical proof, then qualify local and online multiplayer
before calling the round-stats feature complete. The user-facing milestone is
**round completes → auto-saved into Replays (rolling, default last 5) →
Play / Export**,
with Import replay to open exported files. A solo-only prototype does not meet
that milestone.

Pengupop's timestamped events are useful inspiration, but its packaged source
does not include a playback viewer. Frozen Bubble also has variable physics
steps and frame-based rules. Preserve that behavior initially by recording the
step schedule instead of coupling replay work to a live fixed-step conversion.

| Option | Tradeoff | Decision |
| --- | --- | --- |
| Video recording | Faithful pixels but large files; cannot inspect game state or cheaply seek by shots. | Outside this game-state replay project. |
| Seed + raw keys/shots | Small, but loses random choices, timing, mouse/controller behavior and network authority. | Insufficient for this code. |
| Snapshot every rendered frame | Can show a historical view without re-simulation, but needs a comprehensive visual schema and more storage. | Possible later fallback; not the first implementation. |
| Fixed-step simulation + events | Compact and easier to reason about long-term; changes live timing and frame-counted mechanics. | Separate future decision after timing/playfeel validation. |
| Starting state + recorded steps and controls | Preserves the existing step history, supports checks and later checkpoints; modest per-step overhead. | First implementation. |

The first version promises reproducibility only for a validated rules/build and
platform profile. Identical RNG and step inputs do not prove identical floating-
point collision results across compilers or CPUs. Fail clearly on unsupported
versions or desync; do not silently play a different outcome.

## Boundaries to introduce

Suggested names below are proposals, not existing APIs.

1. `StepContext`: monotonic replay step number, exact recorded `deltaScale`
   float bits, and round-relative game time in integer milliseconds. Sample
   the game clock once per step and use it for simulation deadlines. Keep wall
   time for UI scheduling, network transport and file timestamps separately.
2. `PlayerControls`: resolved keyboard/gamepad/mouse/touch intent for each owned
   seat, including held controls, aim target, fire edge and input-origin tag.
   Capture after device translation, before shared fire/hurry/release gating.
   Playback injects this structure at the same phase and never polls devices
   for gameplay. Viewer controls remain active.
3. `AdvanceSimulation(context, controls, inboundEvents)`: extracted from the
   current update sites with their order preserved. `Render()` presents state;
   extra draws must not advance gameplay, consume gameplay RNG, or send traffic.
4. `GameplayRng`: explicit fixed-width state, specified algorithm and bounded-
   integer mapping, independent of menus/transitions. Include its state in the
   snapshot. Keep chain/fall and malus trajectories in the gameplay stream until
   proven irrelevant to future state. Capture bot controls rather than running
   AI during playback; bot decision randomness must remain separate.
5. `SessionMode::Live / Playback` and an explicit effects boundary: gameplay
   may change in-memory scores during playback, but cannot save highscores,
   campaign progress or settings, post telemetry/tournament results, send chat
   or network messages, host/join servers, or start recording itself.
   Note: this boundary was implemented under the 2026-09-20 R1d-iv follow-up
   session even though it carries its own boundary number here; see the dated
   "R1d-iv complete + AdvanceSimulation() boundary + SessionMode effects
   gating" entry in [REPLAY_PROGRESS.md](REPLAY_PROGRESS.md).

Avoid a broad singleton rewrite. Introduce these seams where the current game
already steps, polls input, selects random values and performs external effects.
`UpdatePenguin()` currently also draws hurry warnings; that work must move to
presentation. Audit result/freeze animations too: some advance completion flags.

Preserve the actual simulation phases, including network events before counter
advancement and projectile updates before shooter updates. Give ordered events
a `(step, phase, sequence)` key if more than one injection phase is needed.
Do not sort same-step events by player or type.

## Recording contract

The round must be captured automatically into temporary client-side storage
from its beginning; finalizing at round end cannot reconstruct history from the
final stats. **Capture and saving are both automatic.** Maintain an active-round
recording; when the round settles (see the network result-tail rule below for
online games), finalize it and write it straight into the rolling library
— no separate “candidate awaiting Save” state, since there is no user
action to wait for. The write still must survive a peer starting the next round
or the user leaving the stats UI before the write completes: finalization owns
the recording until its write (and any resulting eviction of the oldest entry)
finishes. Never store references to `bubbleArrays` that `ReloadGame()` will reset.

Start recording **after a round is fully initialized and synchronized, before
its first simulation update**. Recording initially starts only at that boundary;
joining a recording mid-round requires a complete checkpoint and is deferred.
`ReloadGame()` creates another round boundary, not a continuation with stale
queues or a reused timer epoch.

Use an explicit, versioned codec, not raw dumps of C++ objects. A reasonable
first file design is a small binary envelope with typed, length-delimited
records, fixed integer widths/byte order, and a debug text-dump utility. Final
byte layout belongs to R2 after R1 inventories state.

| Record | Required content |
| --- | --- |
| Header | Magic; format version; simulation/rules version; source/build fingerprint (APP_VERSION alone is insufficient); platform/float profile; supported feature flags. |
| Round start | Round ID; effective settings; roster/stable seat IDs and ownership; actual starting boards and current/next color queues; level identity/hash and embedded playable layout; gameplay RNG state; geometry and initial timers/counters; starting scores/wins. |
| Step | Exact `deltaScale`; game clock; normalized controls, preferably changes plus run lengths; ordered inbound gameplay events when network support is enabled. |
| Assertions | Accepted shot/color/placement diagnostics and canonical state hashes after meaningful changes, plus periodic checks. These verify results; they are not applied again as commands. |
| Round end | Outcome, final stats/hash, duration, complete/aborted status. |
| Checkpoint, later | Complete restorable simulation state plus the next event cursor and clock/RNG state. |

Use a new replay step counter: existing `frameCount` advances only in certain
modes and is itself state to preserve. Never reconstruct step scale by rounding
milliseconds or use the viewer's speed setting to recompute it.

Snapshot inventory must cover every value that can affect a later step:
board row lengths/order, cells and positions, parity/offset/compressor state,
shooter/current/next colors and queues, hurry/fire-release flags, pending input,
player lifecycle, scores/statistics, targeting, malus queues and their timestamps,
mode timers and result gates, gameplay RNG, plus active projectiles, falling
chains and malus in flight for checkpoints. Preserve vector order; existing
boards can contain multiple bubbles associated with a cell. Exclude textures,
fonts, device/socket handles and pointers; rebuild those from presentation data.

Capture effective values after settings resolution: `NewGame()` currently
overrides some supplied values from `GameSettings`. Playback must not substitute
the viewer's current mouse, speed or difficulty preferences for the recording.
Presentation preferences may vary only after tests show they cannot change
simulation. Board geometry presently affects physics; retain recorded geometry
until logical board coordinates are decoupled from display layout.

Canonical hashes serialize fields explicitly in stable order, including
simulation float bits and in-flight state. Do not hash object padding, addresses
or draw-only caches. Report the first divergent step/field category and stop.
Checksums detect corruption/desync; they do not make a player-supplied recording
trusted evidence for competitive scores.

## Playback behavior

Restore the initial state through a playback initialization path that bypasses
live level synchronization and setup side effects. Consume recorded steps in
order. A playback scheduler controls how many steps to run before drawing:

- Pause runs zero steps; single-step runs exactly one.
- 0.5× / 1× / 2× / 4× change consumption rate, never the recorded physics scale.
- Restart restores the starting snapshot, resets cursors and transient effects.
- Recorded pauses stop game-time advancement. The initial viewer may skip idle
  pause duration; preserve optional pause metadata if real-time viewing is added.
- Seeking later restores a checkpoint and fast-forwards, with transient audio
  suppressed. A hash alone is not enough to restore a checkpoint.

Bound work per UI frame so long catch-up/seek operations remain cancelable. Do
not discard historical steps to make playback catch up. Round clocks, training
timers and score durations use recorded game time; pause corrections must not
be applied twice.

## Network recording extension

Record the **recording client's observed game**, not an invented global server
timeline. Clients see arrivals at different times. Stable replay seat IDs must
be distinct from array index, nickname and connection ID; preserve the mapping
and departure events. Support the current 20-player roster, including hidden
boards and locally owned bot seats.

Extract parsed gameplay application from `ProcessNetworkMessages()` so live
transport and offline records feed the same handler. Capture validated applied
events with sender, phase and order; preserve ownership and echo-filtering
semantics. Record local controls separately, since they are not all echoed back.
Do not blindly feed both outbound actions and their inbound echoes into playback.

Audit all current handlers: `f`, `s`, `g`, `m`, `M`, `F`, `P`, `S`, `i`, `A`,
`l`, `n`, plus `b/N/T` round initialization. The checked-out code uses `A` for
targeting; do not rely only on older protocol summaries. Resolve level-sync
messages into each round's starting snapshot so playback never waits on a real
socket. Capture needed options/roster changes at round boundaries.

Remote `s/m/M` updates and results remain authoritative as they were observed.
Preserve the local/remote distinction: treating all boards as local would
generate extra collision results, attacks and wins. An offline session context
must provide leader/roster facts currently fetched from `NetworkClient`.

Stop local-round capture only after its final state is settled. Network capture
also needs a bounded result tail for late `S/P/F/l` events. Seal when gameplay
has finished and expected final stats arrive, on the next round boundary, or
at a short timeout (proposed: two seconds after gameplay settles); mark missing
stats explicitly. Finalize-and-write waits for sealing before it runs, without
blocking the game loop — there is no user-facing “Saving…” state to show. Always
seal/detach the old round before its state is reset. Disconnect/quit produces an
explicitly incomplete round. Do not send extra messages to ask the server for
replay data.

Default saved content is gameplay and display names. Exclude chat, IP addresses,
geolocation, credentials and unrelated lobby traffic. No server protocol change,
Discord upload or replay hosting service is required for client-side playback.

## Round-stats behavior

1. Saving is automatic and silent by default: finalizing the round writes it
   into the rolling library, including all observed boards, not the
   whole match. No round-stats button, activation, or Saving…/retry state is
   needed — remove that UI surface from scope entirely.
2. An optional, non-interactive status line (e.g. “Saved to Replays”) may be
   drawn on the stats screen once the write completes, purely informational.
   Because it isn't a focusable control, it does not need a tap target, a
   keyboard/gamepad path, or a footer hint under the input-parity rule in
   `CLAUDE.md` — it draws no differently from any other stats text.
3. Continue/Next round/Lobby/Bracket and chat keep working unmodified. The
   round-stats input/tap handling (`HandleFinishedTap()`, `HandleInput()`)
   needs **no new result action** for replay — auto-save has no click to wire
   up. Finalization runs alongside existing stats accumulation, not gated on
   any input path.
4. Viewing and export happen from the main-menu **Replays** library. Do not
   replace an active network room with a replay viewer at the stats screen.
   The library is entered outside a live match, so playback owns an offline
   game instance and cannot freeze the player's ongoing online game.
5. A round that is still finalizing (e.g. an online round's bounded result
   tail, see below) when the next round starts or the app would exit must not
   be dropped: finalization owns and completes the write regardless of what
   the UI does next. Once written, it takes its slot in the rolling library
   immediately — there is no separate “unsaved candidate” state to show.

Concrete integration points verified at the research baseline:

- `RenderRoundStats()` in `src/bubblegame_render.cpp` is multiplayer-only;
  `FinalizeRoundStats()` accumulates stats, and remote `S` may arrive later.
  Trigger finalize-and-save independently of `networkGame` so local
  multiplayer is captured too. Solo result-screen support can reuse the same
  finalize path later.
- `ProcessNetworkMessages()` can auto-answer `n`; `Render()` can then call
  `ReloadGame()` without a local Continue press. Detach and finalize capture
  before that transition and before tournament return/`QuitToTitle()` destroys
  round state — the write must be initiated from the same place regardless of
  whether a human or a peer's `n` triggered the round boundary.
- Any status text added under point 2 must fit the 640×480 canvas alongside
  the existing 20-player/team stats table without displacing it; skip the
  note rather than crowd the table if space is tight.

## Replay library and playback

Add a **Replays** main-menu row. The submenu provides three things: **playback**
of a saved round, **export** of one to a `.fbr` file, and the **keep count**
setting. List auto-saved rounds newest first with date, mode, player count,
duration and outcome; flag incompatible or incomplete files. Each entry has
**Play**, **Export replay** and **Delete**, plus **Import replay** and the keep
count as library-level actions. Deletion uses the existing confirmation/focus
pattern and simply frees that slot early — the next round to finish takes it
without evicting anything else, since eviction only fires when a new entry
would exceed the current keep count. Saving is automatic and exporting is a
separate action: exporting creates a user-selected copy and never removes,
modifies or exempts the original from later eviction.

**Keep count setting.** Default 5, adjustable in the submenu and stored with
the other INI-backed preferences in `GameSettings` so it survives restart, not
in the replay files themselves. Proposed range 1–20, plus **0 meaning "do not
record"** — a player who does not want the storage or capture cost needs a way
off, and "less" reasonably extends to none. At 0, skip capture at round start
rather than recording and discarding, and keep existing entries (the setting
governs future writes; it is not a bulk-delete button). The upper bound is a
placeholder until R2/R3 measure real `.fbr` sizes: pick a max whose worst-case
product with measured per-round size is a defensible disk footprint on mobile
and in browser storage, and state the measured numbers rather than asserting a
limit. The value bounds only the library on disk; transient capture RAM is
per-active-round and does not scale with it.

**Lowering the keep count below the number of stored entries deletes files.**
Evict the oldest down to the new count immediately, behind the existing
confirmation dialog, naming exactly how many replays will be deleted; ESC
cancels and leaves the setting unchanged. Deferring eviction to the next write
instead would leave the library visibly holding more entries than the setting
claims, which is worse than one honest prompt. Raising the count never deletes
anything and never resurrects an already-evicted round.

Every one of these is an interactive element, so the input-parity checklist in
`CLAUDE.md` applies in full here even though it no longer applies to the round-
stats screen: keyboard/gamepad navigation into and out of each row, real tap
targets via `BeginPanelTapRows`/`AddPanelTapRow` (or a dedicated `HandlePanelTap`
hit-test for the confirmation popup), visible focus on the keep-count control
and on both buttons of the confirm dialog, and a `menulist::DrawFooterHint`
spelling out the controls. A keep-count row that only responds to arrow keys,
or a confirm dialog with no tap zones, is exactly the bug class that file
already documents shipping more than once.

The viewer displays REPLAY, elapsed/total time, pause/resume, 0.5×/1×/2×/4×,
restart and exit back to the library. Add single-step if space/input mapping
permits; timeline seeking remains R8. Finish on the recorded stats without live
Next round, chat or result-reporting actions. All controls need keyboard/gamepad
focus, visible selection, direct tap/click targets and a footer hint.

## Local storage and export

Saved files use a proposed `.fbr` (Frozen Bubble Replay) extension and live in a
`replays/` child of the SDL preference directory, capped at the **configured
keep count, default 5**; a write that would exceed it deletes the oldest first.
Treat the extension as a label; validate the file header and contents on import.
Imported files count toward the same cap and can evict an auto-saved round.
Export the same self-contained bytes, including the starting layout/settings,
so the recipient does not need the author's custom level file. Compatible
Frozen Bubble builds play these files.

**Export means `.fbr` only (confirmed 2026-09-20).** A replay file is not a
video and this project is not adding video encoding: no MP4/WebM export, no
local rendering/encoding stage, no capture-to-file path. If that is ever wanted
it is a separate product decision with its own plan, not a variant of this one.

Temporary capture uses bounded RAM, optionally spilling to scratch storage.
Keep in-progress browser captures outside the IDBFS-mounted tree so another
settings flush cannot accidentally persist them. On round finalize, write to a
temporary file beside the final library entry, finalize atomically using the
existing platform helper, evict the oldest entry if this write is the 6th, and
request browser persistence. Distinguish write-in-progress, durable save, and
persistence failure; a queued browser flush alone is not proof that the file
survived a reload. Recording failure must not stop play. On a capture limit,
show Replay unavailable/incomplete rather than silently evicting the starting
state and claiming the round is complete. Define a total transient budget
covering active-round and in-flight finalize/write data, not only per-file caps
— there is no longer a separate “last-round unsaved candidate” allowance, since
finalize-and-write happens unconditionally.

Bound file size, record size/count, duration, roster and board sizes, string
lengths and numeric ranges before allocation/use; reject NaN/Infinity, invalid
indices, decreasing event order, unsupported required records and truncated
records. Define these limits in R2 and test their boundaries.

| Platform | Proposed export/import path |
| --- | --- |
| macOS / Windows / Linux | Export via SDL's asynchronous Save File dialog; import via Open File. Copy finalized bytes to the chosen destination. Marshal callbacks to the game thread and retain request/filter data until completion. |
| Browser / WASM | Export local bytes as a Blob download on a user gesture; import via a local file input and validate before placing a copy in persistent storage. No HTTP upload. Keep a download link available where browser gesture rules require it. |
| Android | Platform bridge to the Storage Access Framework create/open-document UI; write/read the selected content URI rather than assuming a filesystem path. |
| iOS | Platform bridge to the document picker for export/import; validate lifecycle, destination access and cancellation on device. |

SDL documents Save File as asynchronous, available since 3.2.0, with callbacks
potentially on another thread; verify the project's actual SDL builds before
using it. [SDL Save File dialog](https://wiki.libsdl.org/SDL3/SDL_ShowSaveFileDialog).
Blob URLs support downloads of locally generated bytes and require lifetime
management. [MDN Blob URLs](https://developer.mozilla.org/en-US/docs/Web/URI/Reference/Schemes/blob).
Mobile bridge references:
[Android document storage](https://developer.android.com/training/data-storage/shared/documents-files),
[Apple document picker](https://developer.apple.com/documentation/uikit/uidocumentpickerviewcontroller).

These are implementation paths, not completed platform integrations. Export
cancel/error must leave the library file intact. Report browser “Download
started” rather than claiming the user saved a file the browser cannot confirm.
Use a sanitized suggested name such as `frozen-bubble-2026-09-20-round-03.fbr`.
Import uses a new internal ID; metadata cannot select arbitrary paths. Validate
and copy into controlled storage before listing; avoid duplicate entries for
the same recording. Include round-trip export/import/playback in acceptance.

## Work packages for separate sessions

Each package should finish with a small reviewable change, actual test results
and an updated [progress entry](REPLAY_PROGRESS.md). Preserve the package IDs
from the original plan, but use this revised order: **R1 → R2 → R3 → R5/R6 → R4
→ R7 → R8**. R5 and R6 both depend on R3; they can be implemented in separate
sessions. R4 may be developed earlier behind a feature gate, but its completed
milestone includes local and online round-stats saving and file export/import.
Split a package further if its inventory reveals more
work; do not label partial functionality complete.

| ID | Deliverable and likely files | Completion evidence |
| --- | --- | --- |
| R0 | Research and these documents. | Source archive verified; claims tied to code; playback-source gap recorded. **Complete.** |
| R1 | Simulation/input/clock/effects seams and explicit gameplay RNG. Split into four sub-slices below once its own inventory (2026-09-20) showed it was not one seam — see `docs/REPLAY_PROGRESS.md` for the full inventory and each sub-slice's session log. | Same supplied inputs/context/RNG produce matching state; extra renders do not affect gameplay; existing rules/input/bot tests pass; live feel manually checked. No file format or UI yet. R1 as a whole is complete only once all four sub-slices are. |
| R1a | `GameplayRng`: isolate gameplay randomness from libc's global `rand()`/`srand()`. `src/gameplay_rng.h` (new), `bubblegame.h/.cpp`, `bubblegame_level.cpp`, `bubblegame_shooter.cpp`, `bubblegame_board.cpp`, `bubblegame_state.cpp`, `bubblegame_render.cpp`, `bubblegame_internal.h`. | Same seed → identical sequence; unrelated `rand()`/`srand()` calls interleaved between draws do not perturb it; existing rules/bot tests pass with `rng` seeded explicitly instead of `std::srand()`. **Implemented 2026-09-20; build and full test suite pass; "live feel manually checked" done — user played two network-game rounds against fb.servequake.com — see the dated progress entry.** |
| R1b | `StepContext`: explicit `deltaScale` parameter instead of a global-singleton read (`DriveBot`, `UpdatePenguin`, `UpdateSingleBubblesAtScale` via its `Render()` callers, `DrawAimGuide`); a new monotonic `simStep` counter, additive alongside `frameCount` rather than replacing it (see the dated progress entry for why). `bubblegame.h`, `bubblegame_shooter.cpp`, `bubblegame_render.cpp`. | Same `StepContext` produces matching state regardless of wall-clock jitter; existing tests pass. **Implemented 2026-09-20; build and full test suite pass (38 tests, 36 passed, 2 pre-existing skips); no gameplay behavior change (same values, explicit parameter instead of an implicit global read); "live feel manually checked" done — user played two network-game rounds against fb.servequake.com with a bot — see the dated progress entry.** |
| R1c | `PlayerControls`: capture resolved keyboard/gamepad/mouse/touch/bot intent into a struct at the boundary inside `UpdatePenguin()` (after mouse/touch injection, before the shared fire/hurry/release gate). `src/player_controls.h` (new), `bubblegame.h`, `bubblegame_shooter.cpp`. | Captured controls, replayed, reproduce the same fire/aim outcome as the live input that produced them; bot decisions captured as controls rather than re-derived. **Implemented and complete 2026-09-20; build and full test suite pass; two-instance replay-equality test proves independence from live input, not just determinism; "live feel manually checked" done — user played a solo round with both keyboard and mouse aim/fire — see the dated progress entry for the `suppressFireUntilRelease` finding.** |
| R1d | `AdvanceSimulation()`: split `Render()` and its helpers into a pure mutator followed by pure drawing, untangling the draw/mutate crossovers the R1a inventory found (`UpdatePenguin()`'s mid-function hurry draw, `RenderMalusAlerts()`/chat overlay/attack-flash's draw-then-age pattern, every `Update*Text` helper's recompute-and-blit). `bubblegame_render.cpp` and helpers across `bubblegame_*.cpp`. Likely needs its own further sub-slicing once started. | Extra renders never advance gameplay, consume gameplay RNG, or send traffic; existing rules/input/bot tests pass; live feel manually checked. **Update 2026-09-20: all four sub-slices (R1d-i..iv) are implemented and test-passing, and the `AdvanceSimulation()`/`Draw()` split described here is in place -- `Render()` was split into `bool AdvanceSimulation()` (every gameplay/timer/animation/input/RNG/network/text-aging mutator, in the same relative order the old fused path ran them, returning false for the two present-background-only early returns) and `void Draw()` (background plus every blit and text recompute, with no gameplay state, RNG, network or aging), leaving `Render()` as `if (AdvanceSimulation()) Draw(); else blit background;`. Build and full test suite pass (39 tests, 37 passed, 2 pre-existing skips). The "live feel manually checked" half of this row's acceptance criterion has not actually been performed -- no screen access was used in any R1 session to date, and the accumulated live multiplayer visual checks from R1d-i/ii/iii/iv remain outstanding -- so it is not claimed as satisfied. See the dated progress entries.** |
| R1d-i | `DrawHurryWarning()`: split the hurry-warning texture draw + `PlaySFX("hurry")` trigger out of `ResolvePlayerControls()` (`bubblegame_shooter.cpp`) into a pure draw function called from `Render()` right after each `UpdatePenguin()` call, driven by a new `BubbleArray::hurryWarnVisible` flag the mutator sets. `bubblegame_shooter.cpp`, `bubblegame_render.cpp`, `bubblegame.h`. | Same visible/audio timing as before; `ResolvePlayerControls()` no longer touches `SDL_Renderer`. **Implemented 2026-09-20; build and full test suite pass (38 tests, 36 passed, 2 pre-existing skips); "live feel manually checked" done for solo/classic — user saw the hurry warning flash once before auto-fire; MP/mp_train coverage still outstanding, folded into R1d-ii — see the dated progress entry.** |
| R1d-ii | Countdown/toast draw-then-age triplet -- `RenderMalusAlerts()` (ages/prunes `p.malusAlerts` inside the same loop that draws them), the in-game chat overlay in `Render()` (ages/prunes `inGameChatMessages` right after drawing it, gated only on `networkGame`), and the per-board attack-flash blink (decrements `curArray.attackFlashFramesLeft` inside the same `if (useMini && curArray.boardVisible && attackFlashFramesLeft > 0)` block that draws the blinking border, so a paged-out board's timer never ages). All three: split the age/prune step into a mutator call and leave a pure draw behind. `bubblegame_render.cpp`. | Extra renders do not age timers; visible timing unchanged. **Implemented 2026-09-20; all three items landed in one session -- the triplet was as independent and mechanical as the plan described, so the fallback of landing only a subset was not needed; build and full test suite pass (38 tests, 36 passed, 2 pre-existing skips); the paged-out-board attack-flash quirk was preserved exactly, not fixed, per this row's own description. Manual live-feel check still outstanding -- malus alerts and chat are multiplayer-only, needs a network/local-multiplayer game, not solo -- see the dated progress entry.** |
| R1d-iii | `RenderRoundStats()`'s hit-test rects -- `statsChatBtn`/`statsTournamentBtn` are computed inside the draw function and read later by `HandleFinishedTap()`; decouple their computation from whether the draw actually runs that frame. `bubblegame_render.cpp`. | Tap targets remain correct regardless of draw cadence. **Implemented 2026-09-20; a new anonymous-namespace `ComputeRoundStatsLayout()` is now the single geometry source for both the panel draw and a new `UpdateRoundStatsHitRects()`, which `Render()` calls as a separate statement from `RenderRoundStats()` so the rects stay current independently of the draw; the `playerCount < 2` untouched-rects quirk and the `!networkGame` zeroing are preserved exactly; build and full test suite pass (38 tests, 36 passed, 2 pre-existing skips). Manual live check still outstanding -- the round-stats panel is multiplayer-only (`RenderRoundStats()` returns at `playerCount < 2`, `HandleFinishedTap()` additionally gates on `networkGame && gameFinish`) -- see the dated progress entry.** |
| R1d-iv | The systemic `Update*Text` recompute-and-blit pattern -- `Update2PText`, `UpdatePlayerNameWinText`, `UpdatePoppedText`, `UpdateScoreText`, `DrawLiveBadges`, `RenderRoyaleHud`'s inline `cell` lambda, and the many inline "build string, UpdateText, immediately SDL_RenderTexture" blocks scattered through `Render()` (mpTrainText, comboText, finalScoreText, targetingText, chatInputText/chatLineText, modeTimerText, the disconnect "left" overlays, etc.) all fuse text-texture regeneration with the blit in one call. None of these touch core gameplay rules/RNG/network state -- they recompute presentation strings from state other phases already mutate -- but they are not pure draws either, since regenerating a texture is itself a side effect bundled with the blit. Largest item by call-site count; likely needs its own dedicated session(s) and probably its own further split by screen area (HUD score/pop/timer vs. round-stats table vs. chat) rather than one pass over the whole file. `bubblegame_render.cpp`. | Text regeneration is separated from the blit; extra renders do not advance gameplay. **Implemented 2026-09-20 across two sessions: every site named in this row's description is now split into a recompute/measure phase and a pure-draw phase. First session (the HUD score/pop/timer sub-slice): `UpdateScoreText` and `UpdatePoppedText` (including the embedded Timed-mode `modeTimerText`) became pure recomputes, with new pure `DrawScoreText`/`DrawPoppedText` blits called as separate statements from the same `Render()` call sites; build and full test suite passed (38 tests, 36 passed, 2 pre-existing skips). The follow-up session the same day finished the rest: `DrawLiveBadges` is now a two-pass `MeasureLiveBadges`/`DrawLiveBadges` split (`MeasureLiveBadges` recomputes each badge's label texture and records its chip width + badge cell index in a `liveBadgeCells` cache, and the pure `DrawLiveBadges` only reads and blits that cache); `RenderMalusAlerts`, `RenderRoyaleHud` and `RenderRoundStats` are each now a thin combined wrapper over new `Update*` (recompute) + `Draw*` (pure blit) pairs, with `RenderRoundStats` replaying a recorded `roundStatsOps` draw-op list so recompute and blit cannot drift; the inline `Render()` text blocks (mpTrainText, finalScoreText, targetingText, chatInputText/chatLineText, clearWinText) are recompute-then-blit as two separate statements in `Draw()`; and `comboText`'s draw-then-age crossover gained a `comboTextVisible` latch, aged once in `AdvanceSimulation()` with `Draw()` only blitting the latch. `Update2PText` and `UpdatePlayerNameWinText` were re-inventoried and found already split (they only recompute; their blits were already separate statements in `Render()`), so they needed no change. Build and full test suite pass (39 tests, 37 passed, 2 pre-existing skips). Manual/platform checks: not performed -- no screen access; the score HUD is reachable in solo play, but the popped/Timed countdown, malus alerts, round-stats panel, badges and the split's other output are multiplayer-only, so the live multiplayer visual checks for this row's output are still outstanding -- the same outstanding class R1d-i/ii/iii are waiting on -- see the dated progress entry. This covers R1d-iv's named sites only; it does not by itself establish that R1d or R1 as a whole are done (see the R1d row).** |
| R2 | Versioned snapshot/record codec and validation; `src/replay_format.h/.cpp`, `tests/replay_format_test.cpp`. Sources added via `cmake/CoreSources.cmake`; test registered directly in `CMakeLists.txt` (see completion note). | Round-trip field coverage, deterministic hash, malformed/truncated/oversized/version rejection, portable byte fixtures, sanitizer parser checks. No SDL resources serialized. **Implemented 2026-09-20; codec-only -- see the dated progress entry and the note below. Does not wire any real BubbleGame/BubbleArray state into these records; R3 still has that whole task ahead of it.** |
| R3 | One solo Classic round recorded and replayed offline; proposed `bubblegame_replay.cpp`, in-memory integration fixture plus disk writer/player. | Independently start a live scripted game, capture its actual controls/steps/state, then replay in a fresh game instance and compare at every step. Cover win, loss, hurry auto-fire, wall/ceiling hits, pause/resume and 0.5×/2× scheduling; verify no persistence/network/telemetry effects. **Implemented 2026-09-20 as one slice -- see the completion note below the table and the dated progress entry. `src/bubblegame_replay.h/.cpp` bridge real `BubbleGame`/`BubbleArray` state into R2's records; `tests/bubblegame_replay_test.cpp` proves per-step hash equality for a live scripted round, its in-memory replay, and a decode-from-disk replay across win, loss, hurry auto-fire and varied 0.5×/2× schedules, with Playback-mode effect suppression asserted. Pause/resume is covered by construction: pause never reaches `AdvanceSimulation()`/`Draw()`, so no `StepRecord` exists for a paused span. Solo Classic only; no multiplayer/mode coverage.** |
| R4 | Auto-save on round finalize into a rolling library, detached capture lifetime and eviction, Replays submenu (play / export / keep-count setting), player, and desktop/WASM replay-file export/import. `mainmenu*`, `bubblegame_render.cpp`, `frozenbubble.*`, `gamesettings.*`, `platform.*`, browser bridge. **Split five ways: R4a** production capture wiring (`ReplayRecorder` singleton + the `NewGame()`/`ReloadGame()`/`AdvanceSimulationAtScale()` hooks, no disk, no UI) — **complete 2026-09-21**; **R4b** on-disk rolling library (`ReplayLibrary`), eviction and the `Replay:KeepCount` setting — **complete 2026-09-21**; **R4c** playback viewer (`ReplayPlayer`: load/decode a `.fbr`, drive it at 0.5x/1x/2x/4x via a fractional accumulator, pause/resume, restart, desync detection, non-interactive HUD; programmatic/headless, no menu path yet) — **complete 2026-09-21**; **R4d** the Replays page UI (`mainmenu_replays.cpp`: CONTROLS & SETTINGS row, entry list with Play/Delete per row, pinned Export/Import/keep-count rows, both confirm dialogs, full-bleed playback screen, `platform.h`'s export/import seam stub) — **code-complete and independently verified (build/full suite/ASan) 2026-09-21; manual keyboard+tap click-through still outstanding**, see the dated entry in `REPLAY_PROGRESS.md`; **R4e** desktop (`SDL_ShowSaveFileDialog`/`SDL_ShowOpenFileDialog` behind a `Begin()`/`Poll()` async seam) and WASM (Blob-download export, `<input type=file>`+`FileReader` import bridge) export/import — **code-complete and independently verified (build/full suite/ASan) 2026-09-21; desktop native and WASM-in-browser click-through both still outstanding**, see the dated entry in `REPLAY_PROGRESS.md`. **R4 as a whole (R4a-R4e) is now code-complete**; only the manual click-through gap carries forward. Note the Replays entry point is a row on the CONTROLS & SETTINGS panel, not a main-menu row — the title screen's 8 button slots are physically full (see `REPLAY_PROGRESS.md`). | Local and online rounds auto-saved with no user action; write survives peer-triggered next round and leaving stats; a round completing past the keep count evicts the oldest; keep count persists across restart, 0 disables capture, lowering it prompts and deletes exactly the named count, raising it deletes nothing; submenu rows and the confirm dialog each work by keyboard, gamepad and tap with visible focus; late/missing stats bounded; fresh-process playback; export/import replays identically; cancel/failure leaves originals intact; browser reload persistence checked. **First user-facing milestone.** |
| R5 | Other local rules: random levels, training, chain reactions, 2–5 local players, bots, teams, attack modes, Race/Timed and multiple rounds. | Captured live fixtures cover these rules with matching checkpoints/results; input-origin and bot playback do not re-run live device/AI decisions. Reject features not yet validated. **Update 2026-09-20: split into R5a and R5b after this session's own inventory.** R5a -- the step-driven subset (random levels, predefined levels, chain reactions, 1-5 local players, bots, teams, AttackMode::On/Canceling, Race, and a round captured from a live `ReloadGame()`) -- is implemented: `src/bubblegame_replay.cpp` now captures/restores/hashes every seat, the board blob (v2) stores each row's actual horizontal offset and the board's bubble size (fixing `LoadLevel()`'s inverted filler-row positions), and `tests/bubblegame_replay_test.cpp` extends R3's harness with per-step hash equality for each rule, proving bot playback never re-runs `DriveBot()` (playback-side `botRng` never advances). Training and Timed mode (R5b) are **not** implemented -- both gate their end condition on `SDL_GetTicks()` and R5a records no game clock, so `RestoreRoundStart()` logs and rejects them (as it does any network or >5-seat record). R5 is not complete until R5b lands.** **Update 2026-09-20 (R5b): R5 is now FULLY complete.** R5b added the recorded-game-clock seam -- `stepGameClockMs`, sampled once per `AdvanceSimulation()` step alongside `deltaScale` and replayed from `StepRecord::gameClockMs` by `AdvancePlaybackStep(float, Uint32)` -- and converted the five wall-clock sites training/Timed depend on (training's elapsed/done check and its HUD countdown, the pause-correction `pausedFor`, `TimedSecondsRemaining()`, and `UpdateTimedRound()`'s `modeTimerStart`/`now`) to read it instead of `SDL_GetTicks()`. `RestoreRoundStart()` now accepts `GameMode::Timed` and `mpTraining` and restores the training flag; `CaptureCanonicalStateHash()` also hashes `stepGameClockMs`, `mpTrainScore`, `mpTrainDone` (gameFlags bit 0x20) and `modeTimerExpired` (0x40). `tests/bubblegame_replay_test.cpp` adds a training round (clock jumped past 120s; `mpTrainScore` nonzero; highscore suppression asserted) and a local 3-seat Timed round resolved by `LeadingPopper()`, both with per-step hash equality, and flips the old Timed/training rejection cases to acceptance while keeping Clear rejected. **Scope limits:** training and Timed coverage is **round 1 of a match only** -- `ReloadGame()` does not reset `mpTrainStartTime`, so a training round captured after a live reload is not reproduced, and R5a's `ReloadGame()` continuity remains Race-only; network Timed is still rejected (R6); `RoundEndRecord::durationMs` is still not populated.** |
| R6 | Network recording/playback: parsed event seam, offline session context, ownership, sync snapshots, result tail. `bubblegame_net.cpp`, `bubblegame_level.cpp`, `networkclient.*` only as needed for separation. **Split at R6a's own inventory (2026-09-20) into R6a/R6b/R6c below** so the first network package could land as one reviewable, independently testable slice rather than touching every network-path branch at once. | Fake-server capture with local/remote actions, hosted bots and echoes; delayed events and departures; round 2+; 6/20-player hidden boards; all peers can be offline during playback; zero sends/reports. **Complete as of 2026-09-20: all three sub-slices (R6a, R6b, R6c) have landed and are test-passing.** |
| R6a | Steady-state in-round message replay for a 2-peer network round. Extract the parsed-opcode switch out of `ProcessNetworkMessages()` into a shared `ApplyInboundGameMessage()`; capture the applied inbound payloads per step in `StepRecord::inboundEvents`; widen the round-start codec with real seat ownership/ids and per-seat nicknames; give `RestoreRoundStart()` a 2-seat-network path (board-blob round start, never `SyncNetworkLevel`); skip socket/bot pumping during Playback and drain the recorded payloads instead. `src/bubblegame_net.cpp`, `src/bubblegame_replay.{h,cpp}`, `src/bubblegame.h`, `src/bubblegame_render.cpp`, `tests/bubblegame_replay_test.cpp`. | Remote fire+stick, malus both directions and remote-announced finish captured live and replayed with per-step hash equality; no network send on the playback side; 3-5-seat/bot-hosted network rounds still rejected. **Implemented 2026-09-20 -- see the R6a completion note below.** |
| R6b | Round-start leader/joiner sync and continuity: `SyncNetworkLevel()` plus the `b`/`N`/`T` bubble-sync messages resolved into the round-start snapshot so playback never waits on a socket, hosted bots (`NetBotConnection`/`botConnections`), and round 2+ network continuity (network Timed verdicts included). Expected `src/bubblegame_level.cpp`, `src/bubblegame_net.cpp`, `src/bubblegame_replay.cpp`. | Fake-server round-start sync, hosted bots and round 2+ captured/replayed; all peers offline during playback. **Implemented 2026-09-20 -- see the R6b completion note below. No `bubblegame_level.cpp`/`bubblegame_net.cpp` change was needed: the round start is a full board snapshot taken after `NewGame()` returns, so it captures `SyncNetworkLevel()`'s generated layout for free. The only production changes are the widened per-seat ownership/isBot gate in `RestoreRoundStart()` (`src/bubblegame_replay.cpp`) and a recorded leader flag that `UpdateTimedRound()` (`src/bubblegame_state.cpp`) reads during Playback (`src/bubblegame.h`, `src/bubblegame_replay.{h,cpp}`).** |
| R6c | Battle royale and disruption: >5-player hidden boards and netview paging, delayed events, and mid-round departures (`l`). `src/bubblegame_replay.cpp`, `src/bubblegame_net.cpp`. | 6/20-seat records, delayed `S/P/F/l` result tail, and departure handling captured/replayed. **Implemented 2026-09-20 -- see the R6c completion note below. No `bubblegame_net.cpp` change was needed: the `l`/`S`/`P`/`F` handlers already apply deterministically (`'S'`/`'P'` have no `gameFinish` gate; the `'F'` gate is a deterministic no-op), and `HandlePlayerDeparture()`/`ResolveRoundOutcome()` read only hashed board state. The only production changes are the network-aware bound in `SeatCount()` (which was silently capping every capture and hash at 5 seats) and the widened network range check in `RestoreRoundStart()` (`src/bubblegame_replay.cpp`).** |
| R7 | Mobile export/import bridges and full platform qualification; build profile policy. `platform.*`, Android activity/JNI, iOS document picker, `web/persistence.js`, build configuration as needed. **Scope decided 2026-09-21: Android only, iOS dropped** (immature/no CI/no hardware testing, plus a separate App Store licensing blocker makes distribution-facing iOS work low-value now -- see `REPLAY_PROGRESS.md`). **Code-complete and independently verified (build/full suite/ASan/`gradlew assembleRelease`) 2026-09-21.** Android's document-picker bridge reused SDL3's already-vendored Storage Access Framework support (no new Java/JNI/manifest changes needed) -- only `platform.cpp`'s byte-I/O needed real work (`SDL_SaveFile()`/`SDL_LoadFile()` for transparent `content://` URI support). Also added real semantics to the previously-inert `ReplayHeader::platformFloatProfile` gate (`ComputeCurrentPlatformFloatProfile()`/`IsReplayPlatformCompatible()`), wired into capture and into the Replays page UI (`[INCOMPATIBLE]` badge, disabled Play, distinct import-mismatch message). **Android device/emulator click-through still outstanding** (no such tool available this session), see the dated entry in `REPLAY_PROGRESS.md`. | Platform builds pass; mobile document export/import, cancel and app suspension tested; same fixture tested on macOS/Linux/Windows/WASM/mobile before relaxing compatibility gates. Unsupported cross-platform files get a clear message. |
| R8 | Optional checkpoints and seek/shot navigation after sequential playback is reliable. **Code-complete and independently verified (build/full suite/ASan) 2026-09-21.** On-disk checkpoints deliberately not implemented -- `ReplayPlayer::Load()` already holds a whole recording in memory, so `SeekToStepIndex()`/`SeekToNextShot()`/`SeekToPreviousShot()` (`src/replay_player.h/.cpp`) seek by rebuilding the owned `BubbleGame` and replaying from 0 through the existing forward-playback path, muting `AudioMixer` for the discarded catch-up steps so a seek does not re-fire skipped steps' sounds. Two new playback-screen buttons ("< Shot"/"Shot >", `src/mainmenu_replays.cpp`/`src/mainmenu.h`) expose shot navigation with full keyboard/tap parity; no drag-able scrub bar was built. **With this, R1 through R8 are all code-complete; manual click-through (desktop native, WASM browser, Android device, and R8's own two new buttons) remains the only outstanding work before release**, see the dated entry in `REPLAY_PROGRESS.md`. | Seeked state equals sequential playback at the same step, including in-flight chains/malus; seek stays responsive and does not repeat sound/result effects. |

R1 was the largest risk and did need multiple sessions: its own inventory
(R1a's session, 2026-09-20) confirmed it is four independent seams — RNG
(R1a), clock (R1b), input (R1c), and the `Render()` mutate/draw split (R1d)
— not one, and it is sliced into that order above accordingly. Extract each
seam preserving existing call ordering; add injected input/clock/RNG and
effects gating only once the seam it depends on has landed. Replacing libc
RNG changes future random sequences: preserve allowed ranges/distributions and
record that intentional change instead of promising identical old seeded games
(R1a did this — same ranges, different backing generator). R1d itself split
further into R1d-i..iv once its own inventory was done this session (2026-09-20),
for the same reason R1 split into R1a-d; see `docs/REPLAY_PROGRESS.md` for the
inventory and each sub-slice's status.

**R2 completion note (2026-09-20).** Built exactly the generic envelope and
record schema this row describes, nothing more: `src/replay_format.h` (no
SDL/game includes, same standalone pattern as `src/gameplay_rng.h`) declares
the magic/version/cap constants, `RecordType`, `DecodeResult`, the five
structs (`ReplayHeader`, `RoundStartRecord`, `StepRecord`,
`AssertionRecord`, `RoundEndRecord`) and the `ReplayWriter`/`ReplayReader`
classes; `src/replay_format.cpp` implements them. Byte layout: every
multi-byte field is little-endian, written/read by hand-rolled
WriteU8/16/32/64/Float/Bytes and matching bounds-checked
ReadU8/16/32/64/Float/Bytes helpers (never a memcpy'd struct, never host byte
order); a record's on-disk framing after the header is
`[uint8_t recordType][uint32_t payloadLength][payloadLength bytes]`, and the
reader checks `payloadLength` against both `kMaxPayloadLength` (1 MiB) and
the buffer's actual remaining length before reading or allocating anything
sized from it. `RecordType` reserves value 5 for `Checkpoint` in a comment
only -- not implemented, per this table's own deferral of checkpoints to R8.
`kMaxPlayers = 20` is a local copy of `MAX_NET_PLAYERS`
(`src/bubblegame.h:275`) with a comment pointing at the real definition,
since this header still does not include `bubblegame.h`. A self-contained
FNV-1a 64-bit hash (`CanonicalHashFnv1a64`) is provided for the "canonical
hash" fields; no external dependency, since none existed in `src/` to reuse.
`tests/replay_format_test.cpp` (new `replay-format-test` CMake target,
registered directly rather than via `frozen-bubble-core-test`, mirroring the
lightweight `gameplay-rng-test` pattern) covers every item in this row's
completion-evidence list: round-trip equality for all five structs
individually and composed into one stream; a pinned literal byte sequence
for each of the five, compared byte-for-byte against a fresh encode (not
regenerated from the struct at test time); deterministic-hash checks
(repeated calls, independently-built-but-equal records, and both a top-level
and a deeply-nested field flip both changing the hash); version rejection
(`UnsupportedVersion` distinct from other errors); an exhaustive truncation
sweep at every byte offset of an encoded stream plus the four named
offsets (mid-header, right after a record's type byte, mid-length-field,
mid-payload); an oversized-length case (`RecordTooLarge`) and a
within-cap-but-past-the-buffer case (`Truncated`), proving the cap check and
the remaining-length check are both independently enforced; and malformed
magic / bad record type byte (including the reserved-but-unimplemented value
5), each its own distinct `DecodeResult`. Full ordinary suite: 40 tests, 38
passed, 2 pre-existing skips. Sanitizer build: `replay-format-test` passes
clean under ASan+UBSan (`-fsanitize=address,undefined`); on this macOS/arm64
session, `ASAN_OPTIONS=detect_leaks=1` itself aborts every ASan binary
(`AddressSanitizer: detect_leaks is not supported on this platform`) --
confirmed as a platform-wide limitation, not specific to this test, by
reproducing it against the pre-existing `gameplay-rng-test` binary too; the
actual verification run used `ASAN_OPTIONS=fast_unwind_on_malloc=0` (leak
detection omitted as unsupported here) per this file's own instruction to
report Linux/macOS leak-check differences accurately. See the dated
2026-09-20 R2 progress entry for the full command transcript.

**This package is codec-only.** No real `BubbleGame`/`BubbleArray` state was
captured into any of these records -- every field in `RoundStartRecord`,
`StepRecord`, `AssertionRecord` and `RoundEndRecord` is populated from
hand-constructed test values in `tests/replay_format_test.cpp`, never from a
live game. `bubblegame.h` was not included or touched, and nothing in the
engine calls any of this yet. Recording/replaying an actual round is R3's
entire job, not started here.

**R3 completion note (2026-09-20).** Implemented in one slice: new
`src/bubblegame_replay.h/.cpp` (bridge between real game state and R2's
records), a `SessionMode::Playback` branch in `UpdatePenguin()`, an
`AdvancePlaybackStep(float)` split from `AdvanceSimulation()` sharing one inner
body, and `tests/bubblegame_replay_test.cpp` (`bubblegame-replay-test`, a
full-headless test that drives a real renderer/BubbleGame). Design decisions
actually made, some deviating from this row's own prose:

- **Board blob uses `RoundStartRecord::startingBoards[0]`.** Its internal
  format is a version byte, settings flags (chainReaction/randomLevels),
  `numColors`, row count, geometry (offset/limits/turnsToCompress/dangerZone/
  numSeparators), each row's cell count, and each cell's `bubbleId` +
  `playerBubble`/`frozen` flags in vector order, then the `nextColors` queue.
  Cell positions are recomputed on restore from row/col + `bubbleOffset`
  (`smallerSep + 32*col`, `28*row`), matching `LoadLevel`/`RandomLevel`; `pos`
  is never stored. `levelLayout` stays empty; `levelHash` becomes an FNV-1a
  hash of the board blob. Named fields carry curLaunch/nextBubble/rng/simStep/
  stepDeltaScale/score/wins. Roster/team/seat fields stay zeroed (solo only).
- **`RestoreRoundStart` does call `NewGame()`**, with the recorded effective
  settings, then overwrites the generated board/queues/RNG/counters. That is
  the bypass: `NewGame` supplies geometry/textures and clears the shared
  `singleBubbles`/`malusBubbles` globals; the overwrite is the actual playback
  initialization. No `SyncNetworkLevel()` runs (networkGame=false).
- **AdvanceSimulation()'s split is `AdvanceSimulationAtScale(float)`**, called
  by both `AdvanceSimulation()` (after its singleton read) and the new
  `AdvancePlaybackStep(recordedDeltaScale)`. Live behavior and call sequence
  are unchanged; the singleton is read in exactly one place.
- **`CaptureCanonicalStateHash` takes the whole `BubbleGame`,** not a bare
  `BubbleArray`, because the gameplay RNG stream and the in-flight
  `SingleBubble` projectiles also live at game scope (the latter in a shared
global). It hashes board order/contents, nextColors, curLaunch/nextBubble,
  angle, score, chainLevel, lifecycle/mp flags, geometry and prelight timers,
  stats, plus RNG state and projectiles -- and deliberately excludes
  derive-only input state (`hurryTimer`/`warnTimer`/`suppressFireUntilRelease`/
  `mouseTargetAngle`/`mouseFirePending`), which `ResolvePlayerControls()` owns
  and playback never advances. On the step a stick sets `gameFinish`,
  `UpdatePenguin()` returns before touching those in both runs, so the live
  value is uncaptured input state; hashing them would compare a value the
  record does not carry. The resolved controls -- including the
  hurry-forced fire edge -- are what `lastControls`/`StepRecord` carry.
- **Pause/resume needs no code and gets no record**: pause is a separate
  `RenderPaused()` path that never calls `AdvanceSimulation()`/`Draw()`, so no
  `StepRecord` exists for a paused span and replay reproduces "nothing
  happened" across it. Wall/ceiling hits are covered by the scripted loss
  round (a steep wall bounce that ends on the ceiling) and the win round's
  ceiling-adjacent shot.
- **`gameClockMs` stays 0.** The R1 StepContext seam has `simStep` and
  `stepDeltaScale` only; R3 does not fake a wall-clock game time.

The completion evidence is met for the scenarios actually scripted, and this
note claims no more than that: solo Classic, no multiplayer, no bots, no other
modes, no network, no library/UI. See `docs/REPLAY_PROGRESS.md`'s dated R3
entry for the exact test results.

**R6a completion note (2026-09-20).** R6 was split into R6a/R6b/R6c by this
session's own inventory. R6a -- steady-state in-round message replay for a
**2-peer** network round -- is implemented and test-passing. **R6 as a whole is
not complete**: R6b (round-start `SyncNetworkLevel`/`b`/`N`/`T` sync, hosted
bots, round 2+ continuity) and R6c (battle royale, delayed events, mid-round
departures) remain. What actually landed:

- **`BubbleGame::ApplyInboundGameMessage(int senderId, const std::string
  &gameData)`** now owns the entire parsed-opcode switch that lived inline in
  `ProcessNetworkMessages()` (`f`, `p`, `P`, `n`, `s`, `g`, `m`, `M`, `F`, `S`,
  `i`, `t`, `l`, `A` and the `default` case). The extraction is a move, not a
  rewrite: every `sscanf` reads `gameData.c_str() + 1` exactly as the old
  `char gameData[512]` read `gameData + 1`, and the existing rules/network
  tests pass unmodified. The `b`/`N`/`T` forwarding deliberately did **not**
  move: those are level-sync routing, not gameplay mutation, and R6b owns them;
  they stay in `ProcessNetworkMessages()` immediately after the unchanged
  `OwnsSenderId()`/`IsConnectionLevelOpcode()` filter. `ProcessNetworkMessages()`
  now pumps, filters, records the applied payloads, and calls the handler.
- **Capture.** `ProcessNetworkMessages()` appends every payload that passes the
  ownership filter to a per-step `std::vector<BubbleGame::InboundGameEvent>`.
  `CaptureStep()` writes that step's list into **seat 0's** `StepRecord::inboundEvents`
  only (the step is a property of the step, not a seat, so it is not duplicated
  across the step's records). Encoding is a `u16` count, then per event an
  `i32` sender id and a `u16`-length-prefixed `gameData`. Replay never has to
  reproduce `OwnsSenderId()`/`GetMyPlayerId()`: it re-applies exactly the
  mutations that happened, in order.
- **Restore.** `CaptureRoundStart()` now writes the real `networkGame` flag,
  `seatOwned[i] = OwnsArray(bubbleArrays[i])`, and `seatIds[i] = lobbyPlayerId`
  (array index for a local seat, whose lobby id is `-1`). The board blob was
  bumped to **v3**, appending a length-prefixed per-seat nickname so the
  restored game can resolve a `'g'` destination nick / `'F'` winner nick to a
  board. `RestoreRoundStart()` accepts exactly `playerCount == 2`,
  `networkGame == 1`, seat 0 owned, seat 1 not owned and not a bot (3-5 seats
  and hosted bots stay rejected as R6b/R6c), with the same Classic/Race/Timed
  mode set the local gate already accepted. For a network record it sets
  `setup.networkGame = true` but **forces `setup.randomLevels = false`** before
  `NewGame()` -- a deliberate deviation, because `NewGame()` calls
  `SyncNetworkLevel()` unconditionally when `randomLevels && networkGame` and
  would `QuitToTitle()` with no server; the recorded board blobs (decoded and
  overwritten immediately afterward, exactly as local rounds already do) are
  the source of truth. No `NetworkClient` connection is required or used for
  the board data.
- **Playback.** The network branch of `AdvanceSimulationAtScale()` is now
  session-split: in `SessionMode::Playback` it drains `stepInboundEvents`
  through `ApplyInboundGameMessage()` in capture order and skips
  `ProcessNetworkMessages()`, the bot pump, the ping timer, the tournament
  check and the round-sync wait entirely, while still running the pure
  `frameCount++`. `SetPlaybackInboundEvents()` decodes a `StepRecord::inboundEvents`
  blob with the same bounds-checked reader style as the board blob and capped
  counts/lengths. `SendNetworkBubbleShot()` gained an `EffectsEnabled()` guard
  so a replayed local shot never constructs/touches `NetworkClient`; and
  `SendMalusToOpponent()`'s connection guard now only applies when
  `EffectsEnabled()` is true, so playback keeps the local `rSent` /
  `lastAttackerIdx` / attack-flash bookkeeping (the sends themselves are already
  suppressed by `SendGameDataFor()`). Live behavior is unchanged in both cases.
- **Tests.** `tests/bubblegame_replay_test.cpp` adds a fake-network harness
  (the `NetworkClientTestAccess` pattern from `bubblegame_rules_test.cpp`) and
  three 2-peer scenarios, each captured live through the real
  `ProcessNetworkMessages()` dispatch and compared step-by-step against a
  fresh playback: remote fire+stick (also round-tripped through the disk
  codec), malus in both directions (the local side driven through the real
  `SendMalusToOpponent()` pop path, not an injected echo), and a
  remote-announced `F` finish. 3-seat-network, owned-remote-seat and
  hosted-bot-network records are asserted rejected. Full suite: **41 tests, 39
  passed, 2 pre-existing environmental skips**; the replay test is clean under
  ASan+UBSan.

**Scope limits, stated plainly.** Exactly 2 seats; no hosted bots, no
`SyncNetworkLevel`/`b`/`N`/`T`, no round 2+, no >5-seat boards, no delayed-event
or mid-round-departure handling, and no per-round result-tail sealing. The gate
accepts network Classic/Race/Timed (the same mode set as local) but only
Classic is actually exercised end-to-end; network Timed's leader verdict is
R6b's concern. "All peers offline during playback" is true by construction for
the covered paths, but the board is still a full snapshot, not a sync-message
reconstruction. `RoundEndRecord::durationMs` remains unpopulated.

**R6b completion note (2026-09-20).** R6b -- round-start sync, hosted bots and
round 2+ network continuity, including network Timed's leader verdict -- is
implemented and test-passing. **R6 as a whole is still not complete**: R6c
(battle royale, delayed events, mid-round departures) remains. What landed,
and what did *not* need to:

- **No `bubblegame_level.cpp`/`bubblegame_net.cpp` change was needed.** The
  claim in the plan's R6b row that a joiner playback would otherwise "wait on a
  socket" proved to be already false for the capture design: `CaptureRoundStart()`
  snapshots the board *after* `NewGame()`/`ReloadGame()` returns, regardless of
  whether that board came from `LoadLevel()`, `RandomLevel()`, or
  `SyncNetworkLevel()`. `RestoreRoundStart()` never calls `SyncNetworkLevel()`
  (it forces `setup.randomLevels = false` for a network record), so a captured
  `SyncNetworkLevel()` layout replays from the board blob with no socket at all.
  The `b`/`N`/`T` messages stay exactly where R6a left them (routed to
  `syncQueue` in `ProcessNetworkMessages()`); they are not captured, and they do
  not need to be. The same snapshot-after-resolution property gives round 2+
  continuity for free.
- **Widened gate in `RestoreRoundStart()` (`src/bubblegame_replay.cpp`).** The
  old flat 2-seat/seat-0-owned/seat-1-remote check was replaced with a general
  per-seat rule, applied *after* the existing per-seat board-blob decode loop
  (the rule needs each decoded `isBot`): a network record must have
  `playerCount` in 2-5; seat 0 must be owned and its board must not be a bot;
  for every other seat `i`, `seatOwned[i] != 0` must equal `boards[i].isBot`
  (an owned non-bot seat claims we simulate a remote human; an unowned bot seat
  claims our hosted bot is a stranger -- either rejects the whole record, logged
  and untouched, matching every other rejection in the function). The coarse
  `playerCount` range check still runs before any board is touched, so a record
  failing it never attempts a decode.
- **Hosted bots.** Once the gate accepts `(owned, isBot)` pairs, a network
  round whose non-zero seat is a bot this client hosts works with no new
  mechanism: `NewGame()` already reads `SetupSettings::playerIsBot[i]` into
  `BubbleArray::isBot`, `OwnsArray()` reports it owned, and `UpdatePenguin()`'s
  Playback branch applies the bot's captured `lastControls` without ever
  re-running `DriveBot()`. Playback never calls `AdoptBots()`/`SeatBots()`, so a
  restored network round holds no `botConnections` at all (asserted).
- **Recorded leader flag (network Timed).** `UpdateTimedRound()`
  (`src/bubblegame_state.cpp`) had a real Playback bug: a leader's live capture
  is short (it resolves at `modeTimerDeadline`, expiry + 1500ms), while a
  non-leader waits an extra `kVerdictGraceMs` (2000ms); during Playback
  `NetworkClient::Instance()->IsLeader()` always read false, so a leader-captured
  round took the longer branch and ran out of recorded steps before resolving.
  `CaptureRoundStart()` now records the live leader status, and `UpdateTimedRound()`
  reads it only when `!EffectsEnabled()` (Live still queries the singleton
  byte-for-byte as before). **The leader flag rides the `RoundStartRecord::levelLayout`
  rules blob as a new `kRulesFlagWasNetworkLeader = 0x08` bit, not the per-seat
  board blob**, because "was I the leader" is a round-level fact and the rules
  blob is already single-per-round (the board blob is duplicated across every
  seat). No `kBoardBlobVersion`/`kRulesBlobVersion` bump: this is a new bit in an
  existing flags byte, the same way the existing rules bits were added.
- **Tests.** `tests/bubblegame_replay_test.cpp` adds one scenario per item:
  `net-sync-leader` (round 1 with `randomLevels = true` and a fake leader room,
  proving both seats carry the same non-empty `SyncNetworkLevel()` layout),
  `net-round2` (round 1 live uncaptured, `ReloadGame()`, then round 2 captured;
  asserts `startingWins[0] == 1` and `initialSimStep > 0`), `net-hosted-bot`
  (2-seat room with `playerIsBot[1]`, asserts the bot fired, no playback bot
  connections and zero network sends), and `net-timed-leader` (the regression:
  captured as leader, replay must actually reach `gameFinish` and match the live
  outcome -- a stalled-short replay would otherwise "match" a short hash prefix).
  The old rejection block's three cases were re-framed against the new rule
  (3 seats now reject at the missing seat-2 board decode; the owned-remote and
  unowned-bot mismatches still reject) without weakening what they assert.

**Scope limits, stated plainly (R6b).** Network playback covers 2-5 seats,
Classic/Race/Timed, with or without hosted bots, and round 2+ continuity; only
2-seat and the hosted-bot/timed variants are exercised end-to-end (3-5-seat
network boards are accepted by the gate but not scripted, since the mini-board
geometry they use is already covered by R5a local seats). `b`/`N`/`T` are not
captured as events (the board snapshot makes that unnecessary). No >5-seat
battle royale, no delayed/out-of-order events, no mid-round-departure test, and
no per-round result-tail sealing -- all R6c. `RoundEndRecord::durationMs`
remains unpopulated.

**R6c completion note (2026-09-20).** R6c -- battle royale (>5-player hidden
boards), late result-tail events and mid-round departures -- is implemented and
test-passing. **R6 as a whole is now complete: R6a, R6b and R6c have all
landed.** What landed, and what did *not* need to:

- **The seat bound had to be widened in two places, not one.**
  `bubblegame.h` simulates up to `MAX_NET_PLAYERS` (20) boards in a network
  battle-royale room (`NewGame()`/`ReloadGame()`'s `default:` case builds all
  of them). `RestoreRoundStart()`'s coarse range check was the obvious one, but
  **`SeatCount()` was the silent bug**: it decided how many seats
  `CaptureRoundStart()` captures, `CaptureRoundEnd()` reports and
  `CaptureCanonicalStateHash()` hashes, and it was capped at 5 for *every*
  game. A 7-seat network round would therefore still have captured/hashed only
  seats 0-4 and `rec.start.playerCount` would have read 5. `SeatCount()` now
  caps at `kMaxPlayers` (20) when `currentSettings.networkGame` is set and at
  `kMaxLocalSeats` (5) otherwise; `RestoreRoundStart()`'s network branch uses a
  new `kMaxNetworkSeats = kMaxPlayers` (`2-20`) while the local branch keeps
  `1-5` byte-for-byte. The per-seat ownership/isBot rule already ran up to
  `playerCount` iterations and needed no change. A 21-seat network record is
  still rejected by the coarse check; a 6-seat local record is still rejected.
- **Battle-royale geometry needed no new work.** The round start is still the
  post-`NewGame()` board snapshot (R6b), so a 7-seat record captures seat 0's
  full-size board and seats 1-6's 16-px mini boards (each with its per-seat
  `bubbleOffset`/limits). `RestoreRoundStart()` rebuilds through
  `NewGame(setup)` with `playerCount = 7`, which takes the same battle-royale
  geometry, then overwrites every board from the decoded blobs.
- **`netViewPage`/`boardVisible` were deliberately not captured, hashed or
  restored.** They are view/page state: assigned deterministically from
  already-hashed board state by `ApplyNetViewAuto()`/`ReRankNetView()` (called
  from `HandlePlayerDeparture()`/`ApplyPlayerLoss()`), never read by the
  physics/malus/RNG code, and absent from `CaptureCanonicalStateHash()`, so
  both runs re-derive the same values. (One scope caveat below, about the
  keyboard handler reading `boardVisible`.)
- **No production change was needed for the result tail.** Re-read and
  confirmed: `AdvanceSimulationAtScale()`'s Live network branch calls
  `ProcessNetworkMessages()` unconditionally (no `gameFinish` gate);
  `FinalizeRoundStats()` is called from inside `AdvanceSimulationAtScale()`
  (gated on `gameFinish && !roundStatsFinalized`), not from `Draw()`;
  `ApplyInboundGameMessage()`'s `'S'` and `'P'` cases have no `gameFinish` gate
  and mutate hashed stats whenever they arrive; the `'F'` case gates its
  `ResolveRoundOutcome()` on `!gameFinish`, a deterministic no-op for a late
  `'F'`. The gap was purely in the test harness: `CaptureLiveSteps()` broke its
  loop the moment this client's own `gameFinish` became true, so no test had
  ever stepped past that point. `CaptureLiveSteps()`/`RunLiveNetworkRound()`
  gained a defaulted `stopAtFinish = true`; the new result-tail scenario passes
  `false` and runs a full 40-step script (remote `'F'` at step 10, late remote
  `'S'` at step 20), asserting the harness captured all 40 steps and that the
  late `'S'` (which sets hashed `rFired`/`rPopped`) replays identically.
- **Mid-round departures needed no production change either.** `'l'` looks up
  the seat whose `lobbyPlayerId` matches the sender and calls
  `HandlePlayerDeparture()`, which sets `LEFT`, decrements
  `connectedPlayerCount`, re-ranks the (unhashed) view and calls
  `ResolveRoundOutcome(-1, Departure, false)`. `ResolveRoundOutcome()`/
  `CommitRoundWin()` read only hashed board/settings state
  (`CountLivingPlayers()`/`CountLivingTeams()` via `CountFactions`) and never
  call `IsLeader()` or read the wall clock. `'l'` is a connection-level opcode
  but is still applied and recorded by `ProcessNetworkMessages()`.
- **Tests.** `tests/bubblegame_replay_test.cpp` adds three scenarios:
  `net-royale-malus` (7 seats -- seat 0 local, seats 1/2 hosted bots, seats 3-6
  remote; seat 0's crafted pop takes `SendMalusToOpponent()`'s `>5-alive`
  random single-target branch, at least one hosted bot fires, and
  `rec.start.playerCount == 7` proves all seven seats were captured/hashed),
  `net-late-stats` (the result-tail proof above) and `net-royale-departure` (a
  remote seat's `'l'` at step 5 of a 7-seat round; asserts seat 6 reads `LEFT`
  and `connectedPlayerCount == 6` in both live and replay, and the round does
  not end). Test-only helpers added: the `stopAtFinish` knob, a
  `ShapeNetworkBoard(..., bubbleSize)` parameter (16 for mini seats; default 32
  keeps every existing call site unchanged), `SetupFakeNetworkClient(room,
  seatCount)` (default 2 keeps the old two-entry nick map byte-for-byte; a
  7-entry map seats ids 0-6 on boards 0-6), a
  `BubbleGameTestAccess::connectedPlayerCount()` accessor, and per-seat
  lifecycle/stat fields on `CapturedRecording`/`ReplayResult`. An over-bound
  (21-seat) network record is asserted rejected by the coarse range check, and
  the existing 6-seat local rejection is unchanged.

**Scope limits, stated plainly (R6c), and one flagged gap.** R6 now covers
network playback of 2-20 seats, Classic/Race/Timed, with or without hosted
bots, round 2+ continuity, late result-tail events and mid-round departures,
all proven with per-step hash equality across every seat. What is *not*
covered: `b`/`N`/`T` are still not captured as events (the board snapshot makes
that unnecessary); `RoundEndRecord::durationMs` remains unpopulated; and only
one departure is exercised (a 7-seat room losing one seat, which deliberately
does not end the round) -- an elimination cascade via `l` is not scripted. One
pre-existing capture gap is worth flagging explicitly rather than glossing
over: `boardVisible` *is* read by the keyboard handler in
`bubblegame_input.cpp` to map a slot key to a board and thus choose
`sendMalusToOne`, and manual single-target selection (`sendMalusToOne`) is
itself neither captured in `StepRecord` nor restored by `RestoreRoundStart()`.
A replay of a live round in which the player manually picked a malus target
could therefore diverge. This is not caused by `boardVisible` capture, predates
R6c, and no tested scenario selects a manual target (`sendMalusToOne` stays
`-1`), but it is a real limitation of the "capture controls, replay them" model
for manually-targeted malus and is recorded here.

## Validation and release gates

Use the existing core-test target and `BubbleGameTestAccess` pattern. New replay
tests should compare an independently captured live run with a fresh playback,
not just decode data with the same code that encoded it. Check board contents,
color queues, projectiles/chains, malus timing, scores, winners and RNG state.
Vary drawing cadence and unrelated cosmetic randomness to expose hidden coupling.

For relevant implementation packages:

```sh
cmake -B build -G Ninja
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the repository's ASan/UBSan workflow for the new parser and state restoration
code; see `CLAUDE.md` for the exact sanitizer configuration and the two server
tests that skip on ordinary builds. Linux leak checking and macOS leak-check
limitations must be reported accurately. No tests were run for this planning-
only change because executable code did not change.

Before shipping, extend `docs/MANUAL_TEST_CHECKLIST.md` with live-record/replay,
all input methods, storage failure, pause/speed/exit, compatibility errors and
platform checks. Record measured replay size and capture/seek cost; do not claim
a performance benefit from tests alone. Release/tag work is separate from this
review request.

## Decisions still open

- The keep count's **upper bound** and the exact per-file/transient RAM limits,
  both pending measured `.fbr` sizes from R2/R3. Default 5, user-adjustable,
  0 = do not record, and automatic (not explicit) saving are all confirmed,
  2026-09-20.
- Whether initial sharing across platforms is required. That may favor a more
  authoritative state stream if floating-point re-simulation diverges.
- Whether to invest later in fixed-step live gameplay. Replays do not require
  that change for the recorded-step design.
