# 08 — Dynamic Integration Audit Notebook

## Scope

Task 10: cross-subsystem runtime matrix, player/mode/lifecycle boundaries,
hostile transport cases, and cleanup proof.

This is the first gate in the audit that runs **real client code against a real
server over a real socket**. Tasks 3 and 4 were static by user direction and
recorded "no socket traffic was generated" as their standing limitation; Task 5
exercised gameplay against production objects in-process; Tasks 6-8 exercised
settings, render and platform behaviour in isolation. Task 10 joins them.

What ran, in three layers:

- **Server layer.** The unchanged `server/fb-server` binary from the Task 2
  ASan+UBSan tree (`build-audit-sanitize/server/fb-server`, `-O1 -g
  -fsanitize=address,undefined`), one fresh instance per matrix row, each on its
  own dedicated TCP port and its own working directory.
- **Transport/lobby layer.** A purpose-built audit harness,
  `/tmp/fb-sdl3-audit/task10/fbharness.py` + `scenario.py`, which speaks the
  lobby protocol directly (`NICK`, `CREATE`, `JOIN`, `SETOPTIONS`, `START`,
  `OK_GAME_START`, `LIST`, `PART`, `TALK`) and then the in-game binary relay
  (`<seat-id><payload>\n`) carrying the `f`/`s`/`g`/`m`/`M`/`F`/`n`/`l`/`o`/`r`/
  `S`/`t` opcodes. Task 4 established that `tools/net_bots.py` can only *join* a
  human-created room and cannot create, start, or assert a round; the new
  harness exists precisely to close that gap. It is audit tooling under
  `/tmp/fb-sdl3-audit/`; nothing under `src/`, `server/`, `tests/` or `tools/`
  changed.
- **Client layer.** Two client-side runners: the shipped
  `build-audit-sanitize/frozen-bubble-sdl3` binary run headlessly against an
  isolated preferences root, and
  `/tmp/fb-sdl3-audit/task10/task10_netclient_harness.cpp`, which links the
  **unchanged production `src/networkclient.cpp` object file** and drives it
  against a live sanitized server.

Files whose runtime behaviour this gate newly covers: `server/game.c`,
`server/net.c`, `server/stats.c`, `server/tools.c`, `server/log.c`,
`server/fb-server.c`, `src/networkclient.cpp`, `src/gamesettings.cpp` (pref-path
isolation only), and the whole-program `frozen-bubble-sdl3` startup/shutdown
path. Semantics of those files remain owned by Tasks 3-8; this gate contributes
runtime evidence and two new defects.

**Security-testing boundary.** The user explicitly restricted security-specific
runtime testing for this audit. Brief Step 5 ("Execute hostile transport cases
during gameplay" — fragmentation/coalescing, mid-message disconnect, invalid
player IDs, duplicated ready/game-over/stats messages, sync reordering, and
flooding) was therefore **not executed**. Each of its six scenarios is recorded
as an explicit limitation below, consistent with how Task 3's runtime security
matrix and Task 6's security runtime rows were already handled. Everything that
did run is well-formed protocol traffic produced by cooperating clients.
BUG-049 was found by ordinary simultaneous disconnects during scenario teardown,
not by any hostile campaign, and it was characterised only by varying the player
count — no offensive testing was performed on it.

## Trust boundaries and invariants

Boundaries crossed at runtime by this gate:

- **Peer socket → server room state.** Every `CREATE`/`JOIN`/`START`/`PART` and
  every TCP close mutates `struct game` and the `open_players` list.
- **Peer socket → other peers' game state.** After `OK_GAME_START` the server is
  a byte relay: one client's frame becomes every other client's input.
- **Server-synthesised message → peer.** `GAME_CAN_START` (the seat map) and the
  `<id>l\n` leave frame are manufactured by the server, not by any client.
- **Server disk.** `joiners.log` is written relative to the server's current
  working directory, so `cd`-ing into a scratch directory before launch
  isolates it. The stats file is **not** on this boundary the same way:
  `stats_init()` (`server/stats.c:82-91`) derives `stats_file_path` from
  `getenv("HOME")` unconditionally, independent of the working directory, so
  isolating cwd does not isolate stats persistence — see Limitations for the
  consequence this had on this gate's own runs.
- **Client → user preferences.** `SDL_GetPrefPath("", "frozen-bubble")` is the
  only path the client writes settings, highscores and level history to.

Invariants this gate tested at runtime:

| # | Invariant | Result |
|---|---|---|
| J1 | A room admits exactly `max_players` seats and rejects the next joiner | Holds — 20 seats admitted, 21st and 22nd rejected `GAME_FULL` (c10) |
| J2 | Every seated player receives the same seat map, with distinct ids | Holds — 11 room journals, every peer's map identical, ids contiguous `A`..`T` (65..84) at 20 players |
| J3 | The relay never echoes a sender its own frame | Holds — **0** self-echoes across **4,990** relayed frames in 11 room journals |
| J4 | Every received frame's sender id is a seat in the room | Holds — **0** unknown-sender frames across the same 4,990 |
| J5 | A departure produces exactly one `l` frame per surviving peer, carrying the departed seat's id | Holds — 5/5 at 6 players (c08), 4/4 at 5 players (c09) |
| J6 | Three consecutive rounds complete with per-round `S` stats and `n` readiness | Holds — 3 stats and 3 ready frames per peer (c05) |
| J7 | `START` with one seat is refused | Holds — `ALONE_IN_THE_DARK` (c12) |
| J8 | The creator leaving **before** start returns joiners to the lobby | Holds — both joiners notified and re-listed as open players (c14) |
| J9 | A room survives an ordinary simultaneous disconnect of its seats | **Fails — heap-use-after-free, server aborts (BUG-049)** |
| J10 | `LIST`'s `free:` counter agrees with the open-player list it accompanies | **Fails — `free:3` beside a one-entry list (BUG-050)** |
| J11 | An out-of-range room cap is rejected or clamped, not silently replaced | **Fails — `CREATE x 21` answers `OK` and yields a 5-seat room (IMP-024)** |
| J12 | A client command that the server rejects is reported as failed | **Fails — `StartGame()` returns true on `ALONE_IN_THE_DARK` (BUG-015, runtime-reproduced)** |
| J13 | Client writes stay inside the configured preferences root | Holds — resolved path and all three written files inside the temp root; the real user preferences' mtime is unchanged |
| J14 | The client starts, runs its loop and shuts down without a sanitizer diagnostic | Holds — 12 s headless run, 0 diagnostics (c18) |
| J15 | The server exits on `SIGTERM` without a sanitizer diagnostic | Holds — exit status 0, 0 diagnostics (c22) |

## Static review

Task 10 is a dynamic gate; the static work here is only the reading needed to
build correct harnesses and to turn each runtime observation into a causal
argument. No file's semantics are re-adjudicated — Tasks 3-8 own those.

**Wire shapes confirmed against source before any process was launched.**
Requests are `FB/<major>.<minor> <COMMAND>[ args]\n`; responses are
`FB/1.3 <COMMAND>: <payload>\n` (`server/net.c:126-131`), asynchronous pushes use
the literal command `PUSH`. `send_ok` sends the payload `OK`
(`server/net.c:70,182-185`). The room's identity is its creator's nickname —
`CREATE` stores its argument as `players_nick[0]` (`server/game.c:207-219`) and
`src/networkclient.cpp:324` sends `CREATE <playerNick> <maxPlayers>` — so a
harness that used a separate room name produced a creator whose in-room identity
differed from its `NICK`; the harness was corrected to match the shipped client.

**Seat identity.** `next_seat_id` (`server/game.c:232-249`) allocates within
`['A','z']`, skipping ids still held, which structurally excludes the framing
bytes `0`, `\n`, `\r` and `,` and guarantees a free id because the window holds
58 values against `MAX_PLAYERS_PER_GAME` 20.

**The 20-player boundary is intentional on both sides.** `server/game.c:46`
defines `MAX_PLAYERS_PER_GAME 20`; `src/bubblegame.h:251` defines
`inline constexpr int MAX_NET_PLAYERS = 20`; the shipped room-size selector
offers exactly three values, `static const int kRoomSizes[3] = {5, 10, 20}`
(`src/mainmenu_internal.h:35`). So 20 is the valid boundary and 21 is
boundary+1, and the boundary+1 *cap argument* is unreachable from the shipped
client — which is why the silent fallback below is an improvement (IMP-024) and
not a user-facing defect.

**The re-entrancy that BUG-049 exploits is acknowledged in the source but not
guarded.** `server/net.c:226` carries the comment "this is where the recursive
call can come from (process_msg_prio with a failed send)". The cycle is:
`player_part_game_` → `process_msg_prio_` (`game.c:1044`) → on a failed `send`,
`conn_to_terminate` (`game.c:977`) → `g_list_foreach` (`game.c:982`) →
`conn_terminated` (`net.c:226`) → `player_part_game_` again. The nested call can
reach `games = g_list_remove(games, g); free(g);` at `game.c:1033-1034`, and the
outer frame then resumes at `game.c:1051` with `if (g->players_number == 1)`.
Nothing between `:1044` and `:1051` re-validates `g`. The comment shows the
recursion was known; the missing piece is that the outer frame's `g` can be
stale when control returns.

**Consequence beyond the read.** If the freed slot happens to hold `1`, the very
next statements are `char* winner_nick = g->players_nick[0];` and
`stats_record_win(winner_nick)` (`game.c:1052-1053`), and `stats_record_win`
performs `g_hash_table_lookup`, `g_strdup` and a file write on that pointer
(`server/stats.c:157-177`) — a second use-after-free, this time on a `char*`
reaching hashing and disk I/O. That is why the Release-build outcome (below) is
worse than the ASan one, not better.

**`free:` is computed from the wrong population.**
`calculate_list_games` (`server/game.c:191-205`) emits
`conns_nb() - players_in_game - 1`, and `players_in_game` is accumulated only in
`list_games_aux`'s **non-OPEN** branch (`game.c:170-183`). Players seated in a
room that has not started are therefore counted as free. `games_open` is
computed (`game.c:158`) and never emitted. The `-1` ("don't count myself") is
coherent only when the requester is itself free.

**No shipped client parses `free:`.** `NetworkClient::ParseListResponse`
(`src/networkclient.cpp:1307-1360`) splits the open-player prefix on commas and
then scans bracketed rooms; it never reads `free:`, `games:` or `playing:`. That
bounds BUG-050's impact to third-party/legacy clients and operator diagnostics.

**`StartGame` never inspects the reply.** `src/networkclient.cpp:495-504`
returns `true` on send success and logs "waiting for GAME_CAN_START…". The
server's ordinary rejections for that command (`ALONE_IN_THE_DARK`,
`NOT_CREATOR`, `NOT_IN_GAME`) are never correlated with the pending operation.
`PartGame` (`:506-513`) is the same shape, setting `state = IN_LOBBY` optimistically.
This is BUG-015's registered mechanism; this gate supplies its first runtime
reproduction.

## Dynamic evidence

### Environment and isolation

Every server instance ran from `build-audit-sanitize/server/fb-server` with
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1` and
`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`, in flags `-d -q -z -o DEBUG`
(foreground, no master-server registration, no language, debug logging) and
**never** `-l`/`-L`, so no UDP listener was ever created. `detect_leaks=0` is
mandatory on Apple's ASan, so **no leak claim is made from any of these runs**;
only UB and memory-error checking was active.

Each row bound its own dedicated TCP port from a range reserved for this task
and confirmed free before use: **24** ports, `25610`-`25623`, `25626`-`25631`
and `25640`-`25643`. Port 1511 was never touched. Four unrelated `fb-server`
processes belonging to the user's own environment (ports 15511, 15512, 15113,
15998) were enumerated **before** the first launch and preserved verbatim; the
cleanup proof below is a diff against that recorded baseline.

Client-side isolation was proven before any client process ran: with
`CFFIXED_USER_HOME` and `HOME` both set to a fresh temp directory, a two-line
probe reported
`PREFPATH=/tmp/fb-sdl3-audit/task10/prefs-isolated/Library/Application Support/frozen-bubble/`
— inside the temp root. The subsequent client run wrote exactly three files
(`settings.ini`, `highscores`, `highlevelshistory`) under its own temp root, and
the real `~/Library/Application Support/frozen-bubble` files still carry their
2026-07-28 08:59:49 mtimes.

### The recorded matrix (Step 1, executed before any process was launched)

41 rows: 31 executed, 10 recorded as limitations. Row 38 of the original 38-row
matrix bundled four manual visual/audio observations under one "not performed"
result; it is split below into rows 38-41, one per observation, so every row's
Result is directly countable (Fix Round 1 correction — see
[Gate conclusion](#gate-conclusion)). "San." is the sanitizer state
of the row's server (and, where a client ran, its client). Full per-row journals
are JSON-lines under `/tmp/fb-sdl3-audit/task10/logs/`.

| # | Row | Setup | Expected observable | Log evidence | San. | Manual visual/audio needed | Result |
|---|---|---|---|---|---|---|---|
| 1 | Native single-player smoke | Shipped client, dummy video/audio, software renderer, isolated `CFFIXED_USER_HOME`, 12 s, then `SIGTERM` | Starts, enters `RunForEver`, saves settings inside the temp root, exits | `c18-client-smoke.log` | 0 diagnostics | No (startup/shutdown only) | **Pass** |
| 2 | Client pref-path isolation | `SDL_GetPrefPath("", "frozen-bubble")` under the temp `CFFIXED_USER_HOME` | Resolved path inside the temp root | recorded in this notebook | n/a | No | **Pass** |
| 3 | 2-player room, normal, round 1 | `CREATE <nick> 5` + 1 joiner, 3 fire/stick pairs, stats, win, ready | 2 seats `A`,`B`; identical maps; no self-echo | `c01-room2.server.log`, `room2.jsonl` | 0 | No | **Pass** |
| 4 | 5-player room, normal | as above, 5 seats | 5 seats `A`..`E`; 4 `S` and 4 `n` per peer | `c02-room5.server.log`, `room5.jsonl` | **1 (BUG-049)** | No | **Pass on gameplay; server aborts at teardown** |
| 5 | 6-player room, normal | 6 seats (first battle-royale count) | 6 seats `A`..`F`; 41 frames per joiner | `c03-room6.server.log`, `room6.jsonl` | **1 (BUG-049)** | No | **Pass on gameplay; server aborts at teardown** |
| 6 | 10-player room, normal | 10 seats (middle `kRoomSizes` value) | 10 seats; 73 frames per joiner | `c15-room10.server.log`, `room10.jsonl` | **1 (BUG-049)** | No | **Pass on gameplay; server aborts at teardown** |
| 7 | 20-player room, normal | 20 seats (`MAX_NET_PLAYERS`) | 20 distinct seats `A`..`T`; 3,059 relayed frames; all maps identical | `c04-room20.server.log`, `room20.jsonl` | **1 (BUG-049)** | No | **Pass on gameplay; server aborts at teardown** |
| 8 | Admission cap at the boundary | `CREATE cap20 20`, then 21 join attempts | 19 admitted (20 seats total), 2 rejected `GAME_FULL` | `c10-cap20.server.log` | 0 | No | **Pass** |
| 9 | Admission cap boundary+1 | `CREATE cap21 21`, then 6 join attempts | Reject or clamp to 20 | `c11-cap21.server.log` | 0 | No | **Fail — silent fallback to 5 (IMP-024)** |
| 10 | Rounds 1-3 | 2 seats, three `play → S → F → n` cycles | 3 `S` and 3 `n` frames per peer; both alive after round 3 | `c05-rounds3.server.log`, `room2r3.jsonl` | 0 | No | **Pass** |
| 11 | Team mode | 5 seats, `SETOPTIONS TEAMPLAY:1,PLAYERTEAM_P1:1,PLAYERTEAM_P2:2,MODE:team` | Options echoed to all 5; round completes | `c06-team5.server.log`, `room5t.jsonl` | **1 (BUG-049)** | Yes for the on-screen team banner (not performed) | **Pass on wire; render unobserved** |
| 12 | Clear mode | 5 seats, `SETOPTIONS TEAMPLAY:0,CLEARMODE:1,MODE:clear` | Options echoed to all 5; round completes | `c07-clear5.server.log`, `room5c.jsonl` | **1 (BUG-049)** | Yes for the clear-win banner and its sound (not performed) | **Pass on wire; render/audio unobserved** |
| 13 | Clear-mode and simultaneous-loss rules | Task 5 production-object harness re-run under ASan+UBSan | Reproduces BUG-018 and BUG-019 unchanged | harness stdout | 0 | No | **Pass (defects reproduced)** |
| 14 | Spectator ranking and >5-player paging | `netview-test` under ASan+UBSan | Passes | `ctest` output | 0 | Yes for on-screen pinning (not performed) | **Pass on logic; render unobserved** |
| 15 | Team assignment mapping | `netteams-test` under ASan+UBSan | Passes | `ctest` output | 0 | No | **Pass** |
| 16 | Round-stats colour selection | `roundstats-color-test` under ASan+UBSan | Passes | `ctest` output | 0 | No | **Pass** |
| 17 | Member departure mid-round | 6 seats; the last joiner closes its socket after round 1's shots | Exactly one `l` frame per survivor, carrying the departed seat | `c08-departmember.server.log`, `room6d.jsonl` | **1 (BUG-049)** | No | **Pass — 5 survivors, 5 `l` frames** |
| 18 | Creator departure mid-round | 5 seats; the creator closes its socket after round 1's shots | Room continues; 4 survivors each receive one `l` | `c09-departcreator.server.log`, `room5d.jsonl` | **1 (BUG-049)** | No | **Pass — room is not closed while PLAYING** |
| 19 | Creator departure before start | Creator + 2 joiners, creator closes while the room is OPEN | Joiners notified and returned to the lobby | `c14-creatorlobby.server.log` | 0 | No | **Pass — both notified, both re-listed as open** |
| 20 | `START` with one seat | Creator alone sends `START` | `ALONE_IN_THE_DARK` | `c12-lonely.server.log` | 0 | No | **Pass** |
| 21 | `PART` then rejoin (return to lobby) | Joiner `PART`s an OPEN room and rejoins | Creator sees `PARTED`, rejoin answers `OK` | `c13-relobby.server.log` | 0 | No | **Pass** |
| 22 | Reconnect attempt mid-game | A seat drops during a round, reconnects with the same nick, tries to rejoin | `NICK: OK`; rejoin refused because the room is PLAYING | `c20-reconnect.server.log` | 0 | No | **Pass — `NO_SUCH_GAME` by design** |
| 23 | Simultaneous disconnect, 2 seats | 2-seat playing room, both sockets closed at once | Server survives | `c16-massleave2.server.log` | 0 | No | **Pass** |
| 24 | Simultaneous disconnect, 3 seats | as above, 3 seats | Server survives | `c16-massleave3.server.log` | **1 (BUG-049)** | No | **Fail — heap-use-after-free abort** |
| 25 | Simultaneous disconnect, 4 seats | as above, 4 seats | Server survives | `c16-massleave4.server.log` | **1 (BUG-049)** | No | **Fail** |
| 26 | Simultaneous disconnect, 5 seats | as above, 5 seats | Server survives | `c16-massleave5.server.log` | **1 (BUG-049)** | No | **Fail** |
| 27 | Same case on the Release server | 3 seats, `build-audit-release/server/fb-server` | Distinguish a sanitizer artefact from a real defect | `c17-release3.server.log` | n/a (uninstrumented) | No | **Fail differently — no abort, the freed read is silent** |
| 28 | Production `NetworkClient` against a live server | `task10_netclient_harness` linking the unchanged `networkclient.cpp` object | Connect, `NICK`, `CREATE 20`, `LIST` parse, `START`, `PART`, `LIST`, disconnect, refused-connect probe | `c19-netclient.client.log`, `c19-netclient.server.log` | 0 server, 0 client | No | **Mixed — see row 29** |
| 29 | Client reports a rejected `START` | part of row 28 | `StartGame()` returns false when the server answers `ALONE_IN_THE_DARK` | `c19-netclient.client.log` (`start_returned=1`) | 0 | No | **Fail — returns true (BUG-015 runtime reproduction)** |
| 30 | `LIST` `free:` versus its own open-player list | 3 seats in an OPEN room + 1 idle player issuing `LIST` | `free:0` beside a one-entry list | `c21-freecount.server.log` | 0 | No | **Fail — `free:3` (BUG-050)** |
| 31 | Clean server shutdown | 2-seat round, then `SIGTERM` | Exits, no diagnostic | `c22-shutdown.server.log` | 0 | No | **Pass — exit status 0** |
| 32 | Fragmented / coalesced protocol frames | — | — | — | — | — | **Not performed — user-restricted security runtime testing** |
| 33 | Disconnect in the middle of a frame | — | — | — | — | — | **Not performed — user-restricted security runtime testing** |
| 34 | Frames claiming a player id that is not the sender's seat | — | — | — | — | — | **Not performed — user-restricted security runtime testing** |
| 35 | Duplicated `n` / `F` / `S` frames | — | — | — | — | — | **Not performed — user-restricted security runtime testing** |
| 36 | Reordered `b`/`N`/`T` sync frames | — | — | — | — | — | **Not performed — user-restricted security runtime testing** |
| 37 | Bounded local flooding | — | — | — | — | — | **Not performed — user-restricted security runtime testing** |
| 38 | Manual observation — clear-win banner and its sound | Clear mode reaches a win with a human watching and listening | — | — | — | Yes | **Not performed — no display, dummy drivers, no input-injection path** |
| 39 | Manual observation — spectator pinning on screen | A spectator's own board stays pinned/highlighted among the mini-boards | — | — | — | Yes | **Not performed — no display, dummy drivers, no input-injection path** |
| 40 | Manual observation — >5-player mini-board paging | The mini-board display pages correctly beyond 5 players | — | — | — | Yes | **Not performed — no display, dummy drivers, no input-injection path** |
| 41 | Manual observation — malus/attack visuals | Malus/attack animations render as expected | — | — | — | Yes | **Not performed — no display, dummy drivers, no input-injection path** |

### Aggregate relay measurements

Re-derived from the 11 saved room journals by a script that reconstructs each
peer's seat map and received frames:

| Journal | Seats | All maps identical | Self-echoes | Unknown senders | Frames delivered |
|---|---:|---|---:|---:|---:|
| `room2.jsonl` | 2 | yes | 0 | 0 | 17 |
| `room2r3.jsonl` | 2 | yes | 0 | 0 | 51 |
| `room2sd.jsonl` | 2 | yes | 0 | 0 | 17 |
| `room5.jsonl` | 5 | yes | 0 | 0 | 164 |
| `room5c.jsonl` | 5 | yes | 0 | 0 | 164 |
| `room5d.jsonl` | 5 | yes | 0 | 0 | 151 |
| `room5t.jsonl` | 5 | yes | 0 | 0 | 164 |
| `room6.jsonl` | 6 | yes | 0 | 0 | 245 |
| `room6d.jsonl` | 6 | yes | 0 | 0 | 229 |
| `room10.jsonl` | 10 | yes | 0 | 0 | 729 |
| `room20.jsonl` | 20 | yes | 0 | 0 | 3,059 |
| **Total** | — | **11 of 11** | **0** | **0** | **4,990** |

### BUG-049 reproduction record

24 server log files were produced (23 from the sanitizer build, 1 from the
Release build). **11** of them contain an AddressSanitizer report, and all
**11** carry the identical summary line:

```text
SUMMARY: AddressSanitizer: heap-use-after-free game.c:1051 in player_part_game_
```

The 11 are `c02-room5`, `c03-room6`, `c04-room20`, `c06-team5`, `c07-clear5`,
`c08-departmember`, `c09-departcreator`, `c15-room10`, `c16-massleave3`,
`c16-massleave4` and `c16-massleave5`. The reporting frame is a 4-byte read at
offset 4 of the 424-byte `struct game` allocated by `create_game` at
`game.c:209`; the freeing frame is `game.c:1034`, three `player_part_game_`
levels deeper in the same stack. The minimum reproducing room size is **3**
seats: the 2-seat case (`c16-massleave2`) never recurses deeply enough and
passed.

On the uninstrumented Release server the same input does **not** abort
(`c17-release3`): the log shows the outer frame continuing past the free
(`stats_record_loss` at `game.c:1048`) with no second win recorded, i.e. the
freed slot happened not to read as `1` in that run. That is the dangerous
outcome — the defect is silent in the shipped configuration, and whether a bogus
win and a dangling-pointer `stats_record_win` follow depends on freed-heap
contents.

### Client-layer runtime record

`task10_netclient_harness` (production `networkclient.cpp` object, unchanged)
against a live sanitized server on port 25640, both processes clean under
ASan+UBSan:

```text
connect=1 state=2
nick=1 create=1 leader=1 state=3
listed_games=1
  room creator='hL' players=1 maxPlayers=20 started=0
open_players=0
start_returned=1 state_after_start=3
part=1 games_after_part=0 open_after_part=1
disconnected state=0 connected=0
connect_to_closed_port=0 state=0
task10-netclient=DONE
```

Positives established here for the first time at runtime: the greeting handshake
completes; `NICK`/`CREATE`/`PART` correlate correctly; `ParseListResponse`
correctly recovers `maxPlayers=20` from the `[hL]:20` cap suffix and correctly
moves the player between the room roster and the open-player list across
`PART`; `Connect` to a port with no listener returns false (`errno 61`).
The one failure is row 29: the server answered
`FB/1.3 START: ALONE_IN_THE_DARK`, the client logged
"Sent START command to server, waiting for GAME_CAN_START…" and `StartGame()`
returned `1`.

### Cleanup proof (Step 6)

Re-run after the last matrix row, each check a separate command with its own
exit (the exits are in the [status ledger](../SDL3_REVIEW_STATUS.md#task-10-integration-ledger);
`lsof` exits **1** when nothing matches, and those rows record 1, not 0):

- `lsof -nP -iTCP:25610-25650` produced no output and exited **1** — no socket
  of any state, listening or otherwise, exists anywhere in the range this task
  reserved.
- A per-port sweep over the **24** ports actually used reported `FREE` for every
  one; the counting form of the same sweep printed **24**.
- `pgrep -fl fb-server` still lists exactly **four** processes, and `diff`
  against the baseline captured before the first launch produced no output and
  exited **0** — the list is byte-identical. Those four are **not** this task's:
  they belong to the user's own environment on ports 15511, 15512, 15113 and
  15998, three of them from a different repository, and `ps -o etime` reports
  elapsed times of 4 d 00:49, 4 d 00:49, 1 d 15:37 and 3 d 14:51 — every one
  started days before this gate's first launch. Task 10 neither started nor
  stopped any of them.
- `pgrep -fl 'task10_netclient_harness|task10/scenario.py|build-audit-sanitize/frozen-bubble-sdl3|prefprobe'`
  produced no output and exited **1** — no harness, scenario driver, client or
  probe process from this gate survives.
- `kill -0 94457` exited **1**; the one long-lived client process this gate
  started is gone.

Preserved artifacts (every log backing a finding, kept, not deleted):
`/tmp/fb-sdl3-audit/task10/logs/` holds the 24 per-row server logs, the 11
room journals and the remaining scenario journals as JSON lines, the client
smoke log, the `NetworkClient` harness client log, and both `fb-server` process
snapshots. The harness sources are `/tmp/fb-sdl3-audit/task10/fbharness.py`,
`scenario.py`, `run_case.sh` and `task10_netclient_harness.cpp`.
**Correction (Fix Round 1): the original claim here — "Nothing outside
`/tmp/fb-sdl3-audit/` was written except the tracked audit documents" — was
false, and is withdrawn.** `run_case.sh` `cd`s into a scratch directory before
every launch, which isolates `joiners.log` (below), but it never sets `HOME`,
and `server/stats.c:82-91`'s `stats_init()` derives `stats_file_path` from
`getenv("HOME")` unconditionally, independent of the working directory. All
**24** `fb-server` instances this gate started therefore read from and wrote
to the real `/Users/dchau/.fb-server/stats.dat` — the operator's own file, not
a Task 10 artifact. This was confirmed after the fact by an independent
reviewer and re-verified directly against the pinned baseline and the
preserved evidence: `git show 09d6c7bf:server/stats.c` shows the unconditional
`getenv("HOME")` derivation at `:82-91`; `run_case.sh` contains no `HOME=`
assignment anywhere; and the real file's mtime (`2026-07-29T15:48:29Z`) lands
in the same second as the last matrix row's server log
(`c22-shutdown.server.log`) and its content carries nick prefixes
(`ml3_00`, `ml4_02`, `5d_03`, `5c_00`, `5t_00`, `10_08`, `20_00`, `6_04`,
`2_00`, `rc_01`, `2sd_00`, `2r3_00`, `ml2_00`, `ml5_00`, `6d_00`) that only
`scenario.py`'s nick-generation scheme produces. The file was left exactly as
found — not deleted, truncated, or modified by this correction, since it is
the operator's data and is itself the evidence. See Limitations and
**REL-015**. `fb-server`
appends a `joiners.log` to its working directory on every `NICK`, which is why
every instance was started from a directory under `/tmp/fb-sdl3-audit/task10/`:
**25** such files exist there (one per server instance, the 24 matrix rows plus
the initial harness-bring-up instance) and **1** exists in the repository root,
whose mtime is 2026-07-28 11:01 — a day before this gate's first launch, so it
belongs to an earlier task, not this one.

### Cross-gate corroboration

- **BUG-005** (double win accounting) — the second win it describes is emitted
  from `game.c:1051-1054`, which is exactly the statement BUG-049 makes read
  freed memory. In these runs the freed value never read as `1`, so the second
  win was **not** observed; the two defects are entangled and BUG-049 must be
  fixed before BUG-005's behaviour can be measured. The `game.c:1030` "wins
  (last player remaining)" credit to the last leaver **was** observed in
  `c16-massleave3` and `c17-release3`.
- **BUG-021** (departure bypasses configured continuation) — the server side is
  now measured: a creator leaving a **PLAYING** room does not close it
  (`c09-departcreator`, 4 survivors), while a creator leaving an **OPEN** room
  closes it and returns joiners to the lobby (`c14-creatorlobby`). The
  continuation decision is therefore entirely the client's, matching Task 6's
  proof that `CONTINUEGAMEWHENPLAYERSLEAVE` has no `SetupSettings` field.
- **BUG-013** (reconnection contamination) — server-side admission is not the
  contaminating path: a mid-game reconnect is refused `NO_SUCH_GAME` because
  `find_game_by_nick_aux` only matches `GAME_STATUS_OPEN` (`c20-reconnect`).
  BUG-013's residue is client-local, as registered.
- **BUG-040** (per-player options only expressible for slots 1-5) — corroborated
  on the wire: the 10- and 20-seat rooms started and relayed normally with a
  `SETOPTIONS` payload that names only `P1`/`P2`, and the server echoed it
  verbatim to all 10/20 seats. The server neither validates nor extends it.

## Candidates

No Task 10 candidate remains open. Every candidate this gate raised reached a
terminal state:

- Promoted to new IDs: **BUG-049**, **BUG-050**, **IMP-024**. Fix Round 1 adds
  a fourth: **REL-015** — `fb-server` has no way to relocate its stats-file
  path away from `$HOME`, which is what let this gate's own runs read and
  write the operator's real stats file undetected; see Confirmed findings and
  Limitations.
- Recorded by extending an existing entry rather than allocating a new ID:
  **BUG-015** (first runtime reproduction — `StartGame()` returns true on a
  server rejection), **BUG-005** (entanglement with BUG-049 recorded; its second
  win remains unobserved), **BUG-021** and **BUG-013** (server-side halves
  measured and shown not to be the defective side), **BUG-040** (wire
  corroboration at 10 and 20 seats), **REL-002** (a second, independent
  demonstration that fixed-port harnesses are avoidable: every row here bound a
  reserved port checked free beforehand and released it afterwards).
- Dismissed with counter-evidence: three candidates, below.

## Confirmed findings

### New defects

**BUG-049 — High. Recursive room teardown frees the game object while an outer
frame is still using it (heap-use-after-free, `server/game.c:1051`).**

`player_part_game_` reaches `process_msg_prio_(fd, "<id>l\n", 3, g)` at
`game.c:1044` to tell the surviving seats that a player left. If any of those
`send()` calls fails — the ordinary case when two or more clients disconnect at
about the same time, since the second and later sends to a peer that has already
closed return `EPIPE` — `process_msg_prio_` appends that destination to
`conn_to_terminate` (`game.c:977`) and runs `g_list_foreach(…,
conn_to_terminate_helper, …)` (`game.c:982`), which calls `conn_terminated`
(`net.c:226`), which calls `player_part_game_` **again on the same game**. That
nested call can drive `players_number` to zero and execute
`games = g_list_remove(games, g); free(g);` (`game.c:1033-1034`). Control then
returns to the outer frame, which resumes at `game.c:1051` with
`if (g->players_number == 1)` — a read of freed memory. `server/net.c:226`'s own
comment names the recursion ("this is where the recursive call can come from");
what is missing is any re-validation of `g` after `:1044` returns.

*Reproduction.* Deterministic, in **11** of the 24 recorded server runs, all with
the identical ASan summary quoted above, at seat counts 3, 4, 5, 6, 10 and 20,
in normal, team and clear rooms, and both from scenario teardown and from the
dedicated `massleave` case. The minimum reproducing size is **3** seats;
2 seats cannot recurse deeply enough and passed. No malformed, fragmented or
flooded traffic is involved — every peer simply closes its TCP socket, exactly
as a client does when it is quit or loses its network.

*Consequence.* Under ASan the process aborts, taking every other room on the
server with it. Uninstrumented — the shipped configuration — it does **not**
abort (`c17-release3`): the freed 4 bytes are read and used as a branch
condition. If they read as `1`, the next two statements take
`g->players_nick[0]` (also freed) and pass it to `stats_record_win`, which
hashes it, `g_strdup`s it, and writes the stats file
(`server/stats.c:157-177`) — a second use-after-free reaching disk. Either way a
public server is one ordinary multi-client disconnect away from undefined
behaviour, and the room size at which it starts (3) is below the smallest room
the shipped client can create (5).

*Fix shape (not applied — this audit changes no production source).* Either
defer the `conn_to_terminate` drain until after `player_part_game_` finishes,
or have `player_part_game_` re-look-up its game by fd after `:1044` and bail
if it is gone, or set a per-game "being torn down" flag that suppresses nested
teardown. The same guard is what BUG-003's post-start closure path needs.

**BUG-050 — Low. `LIST` reports a `free:` player count that contradicts the
open-player list in the same message.** `calculate_list_games`
(`server/game.c:191-205`) emits `conns_nb() - players_in_game - 1`, and
`players_in_game` is accumulated only for rooms whose status is **not**
`GAME_STATUS_OPEN` (`game.c:170-183`). Every player seated in a room that has
not started yet is therefore counted as free. Reproduced with three players
seated in one open room plus one genuinely idle player issuing the `LIST`:

```text
FB/1.3 LIST: fcIdle, [fcA,fcJ0,fcJ1]:5 free:3 games:0 playing:0 at:
```

The enumerated open-player list has exactly **1** entry; `free:` says **3**;
excluding the requester itself the correct value is **0**. `games_open` is
computed at `game.c:158` and never emitted, so the field that would have made
the message self-consistent is discarded. Severity is Low because
`NetworkClient::ParseListResponse` (`src/networkclient.cpp:1307-1360`) never
reads `free:` — the shipped client builds its lobby entirely from the
enumerated list, which is correct — so the impact is confined to third-party or
legacy clients and to operators reading server output.

### New improvement

**IMP-024 — Medium benefit / Low effort / Low risk. Validate and diagnose the
`CREATE` room-cap argument instead of silently substituting 5.**
`server/game.c:762` reads `if (mp >= 2 && mp <= MAX_PLAYERS_PER_GAME)
max_players = mp;` with no `else`, so any out-of-range value leaves the legacy
default of 5 in place and the server still answers `CREATE: OK`. Reproduced:
`CREATE cap21 21` produced a room advertised as `[cap21,…]:5` that rejected its
fifth joiner with `GAME_FULL`. The requester is told nothing. The shipped client
cannot reach this — `kRoomSizes` offers only `{5, 10, 20}`
(`src/mainmenu_internal.h:35`) — which is exactly why this is an improvement and
not a defect; but the server is a public endpoint that other clients speak to.
*Location* `server/game.c:755-777`. *Change:* reject with a warning string, or
clamp to `MAX_PLAYERS_PER_GAME` and log it, so the created room's cap always
either matches the request or is refused. *Assertion for a future test:*
`CREATE r 21` must not produce a room whose advertised cap is 5 while the reply
is `OK`. *Matrix:* Linux and macOS.

### New reliability/deployment defect (Fix Round 1)

**REL-015 — Medium. `fb-server` derives its stats-file path from `$HOME`
unconditionally, with no flag, no cwd-relative fallback, and no override
other than `HOME` itself, making sandboxed or CI testing of the server
structurally unsafe against the operator's real host state.**
`stats_init()` (`server/stats.c:82-91`) reads `getenv("HOME")` and, if set,
builds `stats_file_path` as `<HOME>/.fb-server/stats.dat`, falling back to
`/var/lib/fb-server/stats.dat` only when `HOME` is unset. Unlike
`joiners.log`, which is opened relative to the process's current working
directory and is therefore isolated by `cd`-ing into a scratch directory
before launch, this path is independent of `chdir()` — no amount of working-
directory isolation touches it. This gate's own `run_case.sh` `cd`s into a
per-case scratch directory before every launch but never sets `HOME`, so all
**24** `fb-server` instances it started inherited the operator's real `$HOME`
and read from, and wrote to, the real `/Users/dchau/.fb-server/stats.dat`.

*Reproduction/verification.* `git show
09d6c7bfcd864a0ad3951b87d16a88dc770392a3:server/stats.c` confirms the
unconditional `getenv("HOME")` derivation at the pinned baseline, unchanged
from what shipped. `grep -n HOME /tmp/fb-sdl3-audit/task10/run_case.sh`
matches nothing. The real file's mtime, `2026-07-29T15:48:29Z`, is the same
second as the last matrix row's server log
(`c22-shutdown.server.log`, `2026-07-29T15:48:29Z`), and its content contains
nick prefixes (`ml3_00`, `ml4_02`, `5d_03`, `5c_00`, `5t_00`, `10_08`,
`20_00`, `6_04`, `2_00`, `rc_01`, `2sd_00`, `2r3_00`, `ml2_00`, `ml5_00`,
`6d_00`) that only this gate's own `scenario.py` nick-generation scheme
produces. No flag, cwd-relative path, or environment variable other than
`HOME` exists anywhere in `server/stats.c` to redirect this file.

*Consequence.* Any sandboxed, containerized, or CI run of `fb-server` that
does not separately override `HOME` will silently read and write the
invoking user's real stats file, with no crash, error, or log message
distinguishing a test run's writes from a real deployment's — exactly what
happened here, undetected until independent review.

*Fix shape (not applied — this audit changes no production source).* Add a
`--stats-file <path>` flag, honor a dedicated environment variable
independent of `HOME`, or default to a cwd-relative path the way
`joiners.log` already behaves.

### Extensions to existing entries

- **BUG-015** gains its first runtime reproduction: against a live server,
  `NetworkClient::StartGame()` returned `true` while the server's reply on the
  wire was `FB/1.3 START: ALONE_IN_THE_DARK`, and the client logged that it was
  "waiting for GAME_CAN_START" — a push that will never arrive. Confidence is
  unchanged (already High); the evidence class moves from static to reproduced.
- **BUG-005** is recorded as entangled with BUG-049: its second win is emitted
  from `game.c:1051-1054`, the exact statements BUG-049 makes read freed memory,
  so BUG-005 cannot be measured until BUG-049 is fixed. Its first win
  (`game.c:1030`, credited to the last player to leave) was observed.
- **BUG-021**, **BUG-013** and **BUG-040** gain the server-side runtime
  measurements described under Cross-gate corroboration; no severity changes.
- **IMP-019** (protocol and parser unit tests) gains three concrete cases this
  gate can now specify from reproduced behaviour: a three-seat playing room
  whose seats all close at once must not re-enter `player_part_game_` on a freed
  game (BUG-049); `LIST`'s `free:` field must equal the length of the
  open-player list it accompanies, minus the requester (BUG-050); and
  `CREATE r 21` must not answer `OK` with a 5-seat room (IMP-024). None of the
  three needs hostile traffic.
- **IMP-018** (promote Task 5's gameplay harness into CTest) is corroborated:
  that harness was re-run unchanged under ASan+UBSan this gate and reproduced
  BUG-018, BUG-019 and BUG-025 exactly as recorded, so its promotion is a
  packaging exercise, not new engineering.

## Dismissed candidates

**Duplicate seat ids in large rooms — dismissed.** The `next_seat_id` window
`['A','z']` holds 58 values against a 20-seat cap, and the allocator rescans for
collisions. Measured: the 20-seat room produced exactly 20 distinct ids, the
contiguous range 65-84 (`A`..`T`), and every peer's decoded map was identical.
No id collided and no id landed on a framing byte.

**Trailing comma in the `LIST` open-player list — dismissed.**
`fcIdle,` and `clJ0,clJ1,` look truncated, but `list_open_nicks_aux`
(`server/game.c:139-151`) appends `,` after **every** entry by design, and
`ParseListResponse` (`src/networkclient.cpp:1326-1346`) iterates on `find(',')`
and skips empty tokens, so the trailing separator is consumed correctly. The
room list, by contrast, uses a between-only separator (`game.c:103-113`); the
two shapes differ deliberately and both parse.

**`games:0` beside an open room — dismissed.** `games:` is fed by
`games_running`, which `list_games_aux` increments only in the non-OPEN branch
(`server/game.c:174`), and the protocol comment at `game.c:185-190` documents
the trailing fields as counts about *running* games. Reporting `games:0` while
one **open** room exists is therefore consistent with the field's definition,
unlike `free:`, whose definition it contradicts. No defect.

**Mid-game reconnect refusal — dismissed.** A player that drops during a round
and reconnects is answered `NO_SUCH_GAME` on `JOIN`. This is
`find_game_by_nick_aux`'s explicit `g->status == GAME_STATUS_OPEN` filter
(`server/game.c:285-293`) doing what it says: a started room is not joinable.
The `NICK` reclaim itself succeeded, so no nickname is stranded. Deliberate
design, not a defect; the inability to rejoin a round in progress is a product
decision recorded here as an observation.

## Coverage

This gate adds runtime coverage to rows already dispositioned by earlier gates;
it introduces no new path and closes no `pending` row, so `FILE_COVERAGE.md`
still has 237 rows and 0 pending dispositions. The rows whose Notes this gate
extends are `server/game.c`, `server/net.c`, `src/networkclient.cpp` and
`tools/net_bots.py`. See [FILE_COVERAGE.md](../FILE_COVERAGE.md).

Runtime coverage delivered, by subsystem:

| Subsystem (owning gate) | What Task 10 exercised at runtime |
|---|---|
| Server protocol (Task 3) | Greeting, `NICK`, `CREATE`, `JOIN`, `SETOPTIONS`, `START`, `OK_GAME_START`, `LIST`, `PART`, binary relay, synthesized `l`, seat allocation, admission caps, room teardown, `SIGTERM` exit — with sanitizers on |
| Network client (Task 4) | The production `networkclient.cpp` object over a real socket: connect/greeting, command correlation, `LIST` parsing including the `:N` cap suffix, leader detection, `PART`, disconnect, refused-connect probe |
| Gameplay (Task 5) | Task 5's production-object harness re-run under ASan+UBSan; three consecutive rounds and per-round stats/readiness exercised on the wire |
| Lobby/settings/input (Task 6) | Preference-path isolation proven before any client ran; room lifecycle transitions (create → join → part → rejoin → close) driven end to end **at the wire-protocol level, by this gate's own harness — not through the `mainmenu_netpanel.cpp` UI Task 6 owns, which Limitation 5 records as never exercised** |
| Render/audio (Task 7) | Only indirectly: the client started, created a software renderer and shut down with no diagnostic. Visual and audio observation was not performed |
| Platform ports (Task 8) | Native macOS only. No WASM, Android or Windows runtime |
| Build/release (Task 9) | The Task 2 sanitizer and Release trees were re-used unchanged and both produced working binaries |

## Limitations

1. **Brief Step 5 was not executed.** All six hostile transport scenarios —
   fragmentation/coalescing, mid-frame disconnect, frames claiming a foreign
   player id, duplicated `n`/`F`/`S` frames, reordered `b`/`N`/`T` sync frames,
   and bounded flooding — are recorded as matrix rows 32-37 with the result
   "not performed", because the user explicitly restricted security-specific
   runtime testing for this audit. This matches Task 3's omitted runtime/security
   matrix and Task 6's omitted security runtime rows. The consequence is that
   SEC-002 through SEC-007 remain without runtime evidence, and BUG-006,
   BUG-007, BUG-014 and BUG-017 remain statically argued.
2. **BUG-049 was characterised only functionally.** Its reproduction varies one
   parameter — the number of seats closing at once — and stops there. No attempt
   was made to steer the freed allocation, to make the freed slot read as `1`,
   or to reach the `stats_record_win` dangling-pointer path. That would be
   exploit development, which is outside this audit's scope. The
   `stats_record_win` consequence is therefore a code-supported argument, not a
   reproduction.
3. **No leak claim.** Apple's ASan rejects `detect_leaks=1`, so every run used
   `detect_leaks=0`. These runs prove the absence of the memory errors ASan and
   UBSan do check for; they say nothing about leaks. Leak reasoning stays static,
   as in Tasks 5-8.
4. **No manual visual or audio observation.** Every client process ran with
   `SDL_VIDEO_DRIVER=dummy`, `SDL_AUDIO_DRIVER=dummy` and the software renderer.
   The clear-win banner and its sound, spectator pinning on screen, the
   >5-player mini-board paging, the malus/attack visuals, and (row 11) the
   team-mode on-screen team banner were not seen or heard by a human. Their
   logic is covered by `netview-test`, `roundstats-color-test` and Task 5's
   harness; their presentation is not. (Task 11: row 11's parenthetical "Yes
   for the on-screen team banner (not performed)" predates this gate's close
   and names a fifth unobserved visual distinct from the clear-win banner in
   this same list; it does not change the 41-row/31-executed/10-not-performed
   counts, since row 11 itself is already counted among the 31 executed rows —
   only its render-observation aside was previously missing from this
   canonical list.)
5. **The shipped client was never driven through its menus into a network
   game.** SDL's dummy video driver accepts no externally injected input and the
   binary takes no arguments (`src/main.cpp:27`), so there is no path to
   navigate the real client from the title screen into a lobby without modifying
   production source. The client layer was therefore covered in two pieces — a
   whole-program startup/shutdown smoke and a production-object network harness
   — which together do not equal one human-driven session. This is the same
   omission Task 7 recorded as "full-client navigation omitted".
6. **Single-player gameplay was not played.** Row 1 is a startup/shutdown smoke
   only; the actual single-player round logic is covered by Task 5's
   production-object harness, re-run here, not by a played game.
7. **Native macOS only.** No Linux, Windows, Android or browser runtime was
   available, so nothing here speaks to WASM's `networkclient_wasm.cpp` (BUG-014
   in particular), to Windows socket typing (REL-003), or to Android.
8. **No WebSocket transport.** Every connection was raw TCP. `server/ws.c`'s
   handshake and framing (BUG-006, SEC-002) were not exercised, because the
   browser client and the nginx/websockify path were both unavailable.
9. **Harness fidelity.** The harness is a faithful *protocol* peer, not a
   faithful *game* peer: its `f`/`s`/`S`/`F`/`n` payloads are well-formed and
   deterministic but do not simulate real board physics, so wire-level and
   lifecycle invariants are what these rows prove — not that the boards on two
   real clients would agree. Level synchronization (`b`/`N`/`T`) was not driven,
   because only the leader generates it and no real leader ran.
10. **The 21-seat case is a server-side probe.** `CREATE r 21` cannot be produced
    by the shipped client, so IMP-024's row measures the server's protocol
    contract, not a reachable user action.
11. **The server's stats file was not isolated, and this was not caught until
    independent review (Fix Round 1).** `server/stats.c:82-91`'s `stats_init()`
    derives `stats_file_path` from `getenv("HOME")` unconditionally, a
    mechanism entirely independent of the server's working directory. This
    gate's isolation strategy — `cd`-ing each `fb-server` instance into its own
    scratch directory — correctly isolated `joiners.log`, which *is*
    cwd-relative, but does nothing for the stats file, and `run_case.sh` never
    set `HOME` for any of its **24** launches. Every one of them therefore read
    from and wrote to the operator's real `/Users/dchau/.fb-server/stats.dat`.
    This is registered as **REL-015**: `fb-server` offers no flag,
    cwd-relative path, or `HOME`-independent environment override to relocate
    this file, which makes sandboxed or CI testing of the server structurally
    unsafe against a real host's state. The real file was left exactly as
    found — not deleted, truncated, or modified — because it is the operator's
    data and is itself the forensic evidence for this limitation.

## Gate conclusion

Complete, with the executed/limitation split stated above: **31** of the 41
recorded matrix rows were executed and **10** were recorded as not performed —
six of them because the user restricted security-specific runtime testing, four
(the split former row 38: clear-win banner and sound, spectator pinning,
>5-player paging, malus/attack visuals) because no display, audio device or
input-injection path existed.

Two new defects and one improvement are registered: **BUG-049** (High), a
deterministically reproduced heap-use-after-free in the server's recursive room
teardown that a public server reaches from an ordinary multi-client disconnect
and that is *silent* in the shipped uninstrumented build; **BUG-050** (Low), a
`LIST` counter that contradicts the list it accompanies; and **IMP-024**, the
missing validation of the `CREATE` room-cap argument. **BUG-015** gains its
first runtime reproduction, and BUG-005, BUG-013, BUG-021 and BUG-040 gain
server-side runtime measurements without severity changes. Three candidates were
dismissed with counter-evidence. Fix Round 1 registered a fourth finding,
**REL-015** (Medium): `fb-server`'s stats-file path is derived from `$HOME`
unconditionally with no isolation mechanism, and this gate's own 24 server
launches never set `HOME`, so every one of them read from and wrote to the
operator's real `~/.fb-server/stats.dat` — see Limitations.

Every dedicated listener, client and harness process this gate started was
stopped and proven gone: all **24** dedicated ports read free afterwards, and
the `fb-server` process list is byte-identical to the baseline captured before
the first launch, so the four unrelated servers belonging to the user's own
environment were neither touched nor counted. The repository worktree is clean
and the production tree is byte-identical to the pinned baseline. The commands,
exits and evidence paths are in the
[status ledger](../SDL3_REVIEW_STATUS.md#task-10-integration-ledger).
