# Pengupop replay research

Reviewed 2026-09-20. This is a source review, not a runtime playback test.
Continue with [the implementation plan](REPLAY_PLAN.md) and
[the progress / session handoff](REPLAY_PROGRESS.md).

## Finding

Pengupop 2.2.6 contains a compact **multiplayer event recorder that uploads to
its master server**. The supplied distribution does **not** contain a replay
file reader, playback mode, replay browser, or the master server implementation.
Consequently, its recording format and simulation can be explained directly;
the behavior of a separate historical playback service cannot be verified from
this package. Do not describe this as a working replay player we can port.

The user recalls that replays may have been on a website but does not remember
which one. That is consistent with the upload path, but does not verify a
particular service or its playback implementation.

The useful design is to save a starting state, timestamped actions, and random
outcomes, then advance the game simulation again. Frozen Bubble can use that
idea, but its current timing and state ownership need explicit handling.

## Reproducible source baseline

| Item | Reviewed revision |
| --- | --- |
| [AUR packaging repository](https://aur.archlinux.org/pengupop.git) | `18fc4246deb000d6d69cb199ce56ad25db51dc34` |
| Package | `pengupop 2.2.6-1`; AUR commit dated 2025-01-31 |
| [Actual source archive](https://mirror.amdmi3.ru/distfiles/pengupop-2.2.6.tar.gz) | Release dated 2010-01-01 in `ChangeLog` |
| Archive SHA-256 | `f6cef1fedb11bafc947f8824533df9ca9dd6aeaa5aa795c61870b986a1fc45ba` |
| Frozen Bubble source | `61b00290ef264dbcca5135e9a50e4227d939cbad`, version `2.4.105` |

The downloaded archive matched the checksum in both `PKGBUILD` and `.SRCINFO`.
The AUR repository is a package recipe, not the game's development repository.
No package scripts or downloaded executables were run.

To reacquire the same sources in a later session:

```sh
review_dir=$(mktemp -d)
git clone https://aur.archlinux.org/pengupop.git "$review_dir/aur"
git -C "$review_dir/aur" checkout 18fc4246deb000d6d69cb199ce56ad25db51dc34
curl -fL https://mirror.amdmi3.ru/distfiles/pengupop-2.2.6.tar.gz \
  -o "$review_dir/pengupop-2.2.6.tar.gz"
shasum -a 256 "$review_dir/pengupop-2.2.6.tar.gz"
# Compare against the hash above before extracting.
tar -xzf "$review_dir/pengupop-2.2.6.tar.gz" -C "$review_dir"
```

All Pengupop line references below refer to that archive, not Frozen Bubble.

## What the recorder actually does

| Code | Verified behavior |
| --- | --- |
| `packet.h`, `struct event`, `EVENT_*` | Each event is four bytes: a 24-bit tick followed by one byte of event data. |
| `main.c:564`, `add_event()` | Writes the tick most-significant byte first; buffers 256 events; flushes a full buffer before adding another event. Returns immediately for `single_player`. |
| `main.c:548`, `submit_events()` | Sends `packet_eventlog` (`0x0a`) with `4 * event_count` payload bytes; clears the count. |
| `main.c:509`, `send_packet()` | Event logs go to the master connection. Movement packets (`>= 0x10`) can use a direct peer connection. |
| `main.c:476`, `log_round()` | Records win/loss once and flushes the remaining events. |
| `main.c:1397`, start-game handler | Decodes a four-byte server seed, initializes both boards, resets tick count, and starts simulation. |
| `main.c:185`, `rng()`; `main.c:1169`, `init_field()` | Custom integer PRNG creates the shared starting board and initial current/next colors. |
| `main.c:1086`, `random_bubble()`; `main.c:1110`, `shoot()` | Later local shot colors use libc `rand()`. The selected next color is recorded explicitly after shooting. |

The event data meanings at the call sites are:

| Value | Meaning |
| --- | --- |
| `0x20` | Begin turning left. |
| `0x10` | Begin turning right. |
| `0x30` | Stop turning; emitted when the active direction key is released. |
| `0x80 \| color` | A shot happened; low bits contain the **new next color**, not the fired color (`main.c:2912`, `2950`). |
| `0x40 \| color` | A hostile bubble was spawned; color is the actual field color (`main.c:3175`). |
| `0x08` | Round start. |
| `0x04` / `0x02` | Win / loss. |

Shot next-colors use 0–7; hostile bubble colors use the field's 1–8 convention.
This is a tagged encoding, not a set of independently combinable flags: for
example, a hostile bubble of color 8 also has the `EVENT_START` bit set.
An inferred decoder would need to recognize the high-bit event classes first.

The event itself contains neither player ID nor starting seed. Connection and
match context supply those elsewhere. A saved standalone replay would therefore
need a wrapper preserving that context. The upload code alone does not establish
that the server persisted it or how it combined both players' logs.

## Timing and inferred playback

Multiplayer uses an 8 ms simulation step (125 steps/second), defined at
`main.c:88`. The main loop at `2598` catches up with repeated `game_tick()` calls;
only the last catch-up step is painted. `game_tick()` increments `tickidx` at
`1083`. SDL input is processed after that catch-up loop, so event ordering at a
tick boundary matters. A start event can occur at tick 1: initialization calls
`game_tick(1)` before recording it. Some early returns in initial connection
setup occur before the start-event call, another reason not to assume a complete
file format from the event declarations.

**Inference, not recovered playback code:** a player could reconstruct the
seeded board, advance that same simulation to each event tick, apply aim changes,
shoot with the recorded next color, spawn the recorded hostile bubble, and show
the recorded result. Array order preserves multiple events at the same tick.
Pause and speed controls could change how quickly ticks are consumed without
changing the physics step.

Limits of copying this approach literally:

- The 24-bit tick wraps after about 37.3 hours at 8 ms per step (calculated from
  `2^24 * 0.008` seconds). There is no replay-file version or seek index here.
- The custom initial-board PRNG does not make all randomness portable. Hostile
  bubble trajectories use `sin(pow(seed++, 4.5))` (`main.c:3165`), and shot
  trajectories use floating-point trigonometry. Cross-platform equality is not
  proven by the event format.
- Full buffers and completed results are submitted; an interrupted connection
  can lose an unsubmitted tail. No local crash-recoverable recording is present.
- The separate campaign simulation in `singleplayer.c` uses 4 ms steps and does
  not implement this multiplayer recorder. Do not claim solo replay support.

## Live opponent updates are a different mechanism

`packet_movement` carries angle, direction, current/next colors, and optionally
the packed board and moving bubbles (`packet.h`, `main.c:1482–1587`, `3194+`).
Those moving bubbles include positions and velocities. This is live state
replication, separate from the compact event upload. The client has no
`packet_eventlog` playback handler in `process_packet()`.

The two designs offer different lessons: event records are compact; authoritative
board/state updates can correct a remote view without requiring identical local
physics. Neither is a complete on-disk replay implementation in this archive.

## Frozen Bubble integration findings

Paths below refer to the reviewed Frozen Bubble revision. Prefer symbol names
when resuming after code changes.

| Area | Evidence and consequence |
| --- | --- |
| Simulation in rendering | `src/bubblegame_render.cpp:801`, `BubbleGame::Render()` pumps networking, advances counters, runs training/timed rules, updates projectiles and shooters, and finalizes stats. Drawing additional frames currently advances the game. |
| Update order | `UpdateSingleBubbles()` precedes `UpdatePenguin()` in both solo and multiplayer branches. Moving all input application to the start of a step could change which shot launches after a landing. Preserve the actual phases. |
| Variable timing | `src/frozenbubble.cpp:437`, `RunOneFrame()` computes clamped `deltaScale`; browser and native speed policies differ. Many other counters still advance once per call. Replaying only elapsed seconds or only shots is insufficient. |
| Wall clocks | `UpdateTimedRound()`, training logic, score time, pause correction and game duration read `SDL_GetTicks()`. Playback needs a supplied game clock. Transport timeouts remain real-time concerns. |
| Randomness | `ranrange()` in `src/bubblegame_internal.h:36` shares libc `rand()` with levels, next colors, malus, menu animations, transitions, and highscore pictures. `shaderstuff.cpp` also calls `srand()`. A seed alone is not a reliable replay contract. |
| Falling bubbles matter | `SingleBubble::GenerateFreeFall()` chooses random velocities; chain landings in `UpdateSingleBubblesAtScale()` mutate the board and release malus. Do not classify all falling effects as cosmetic. |
| Input | `UpdatePenguin()` polls keyboard/controller state, drives bots, applies mouse/touch fire, and force-fires on hurry timeout. Capture a normalized input boundary shared with playback, not SDL key events alone. |
| Bots | Per-bot `botRng` is separate, but seeded from `SDL_GetTicks()` in `NewGame()`. Either capture its state and algorithm or record bot controls and bypass AI on playback. |
| Network authority | `ProcessNetworkMessages()` handles `f/s/g/m/M/F/P/S/i/A/l/n` and sync `b/N/T`. Remote placements are authoritative; local shots and hosted bots are already simulated and some echoes are ignored. A receive-only packet log misses local actions. |
| Round initialization | `NewGame()`, `ReloadGame()`, `LoadLevel()`, `RandomLevel()`, `SyncNetworkLevel()` create state and touch settings/services. Playback should restore explicit state rather than calling live networking setup. |
| Actual capacity | `src/bubblegame.h:272` defines `MAX_NET_PLAYERS = 20`; the array is sized with it. Five is the local-control/display convention, not the network replay roster limit. |
| Side effects | `SubmitScore()`, training completion, `CheckGameState()`, `ReportTournamentResult()`, `FinalizeRoundStats()` and `SendLobbyMatchSummary()` can persist or publish results. Playback must suppress those external effects while retaining in-memory stats. |
| Storage | `src/platform.h` exposes `ReplaceFileAtomically()` and `RequestPersistentStorageFlush()`. Browser persistence mounts `/libsdl/frozen-bubble` in `web/persistence.js`; a replay directory must be inside persistent storage. |
| Tests/build | `BubbleGameTestAccess` in `tests/bubblegame_rules_test.cpp` already exposes useful seams; `cmake/CoreSources.cmake` is shared with Android. |

The original Perl `bin/frozen-bubble`, `update_game()` around lines 2100–2260,
confirms that `f` announces angle/new next color, `s` supplies exact placement
and the color queue, and malus release depends on stick/chain timing. Preserve
these rules when extracting the C++ simulation; do not substitute Pengupop's
mechanics.

No code needs to be copied from Pengupop. Its `main.c` header says GPL version 3
or later; Frozen Bubble's reviewed C++ headers say GPL version 2. This plan uses
independently written code and the architectural observations above.

## Unverified items

- Whether a historical Junoplay website or a different Pengupop distribution
  supplied a replay viewer, and whether its source is still available.
- Whether uploaded event logs were intended for playback, result verification,
  or both. The client does not establish server behavior.
- Runtime replay fidelity: neither a Pengupop replay viewer nor a Frozen Bubble
  recorder/player was built or run during this review.

These gaps do not block an independent Frozen Bubble replay implementation.
