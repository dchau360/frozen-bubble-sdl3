# Tournament protocol v1

Tournament support is an extension on FB/1.3. All commands are `FB/1.3 TOUR <operation> ...\n`. Replies use the normal `TOUR: OK` or `TOUR: <ERROR>` framing. Successful commands may also push state before replying. `TOUR CAPS` returns `TOUR_CAPS: 1` as a push; old servers reply UNKNOWN_COMMAND and clients leave tournaments hidden.

All IDs are positive decimal integers, except 0 means absent/draw. Entrant IDs start at 1 within each tournament, survive room transitions, and are not file descriptors. Nicknames obey the server's existing nickname restrictions and cannot change during active entry. Times are durations in seconds remaining, calculated on snapshot emission; clients use local monotonic receipt time for display. Maximum 16 entrants, 15 matches, 8 retained tournaments, and one watched tournament per connection. A participant always receives their own tournament state independently of their watched tournament. Completed/cancelled tournaments expire after 1800 seconds.

## Commands

`CAPS`: safe capability probe, allowed before NICK.

`LIST`: push `TOUR_LIST: <items>`; items are semicolon-separated `id,status,count,ownerNick`, or `-` for empty.

`CREATE [options]`: named human lobby client creates and joins a tournament; maximum 16 entrants. Name displayed by owner nickname. No custom title. `options`, if present, is everything after `CREATE ` verbatim -- the same comma-separated `KEY:value` ruleset blob a room's own `SETOPTIONS` command carries (see `src/networkclient.cpp`'s `SendOptions`), chosen by the organizer before creating. Stored on the tournament and applied identically to every match's room as it is created (as an `OPTIONS:` push to both seats, exactly as a live room's `SETOPTIONS` would produce, sent before that match's `TOUR_ASSIGN` recipients see any start signal) so no per-room setting can drift the bracket partway through. Bare `CREATE` (no options) keeps the pre-existing behavior: every match gets the server's zero-initialized game defaults. Not editable after creation -- cancel and recreate to change it before anyone has registered.

`JOIN <tid>`: join registration, human clients only, one active entry per connection, no ordinary room membership.

`WATCH <tid>`: watch and receive a full state snapshot. `WATCH 0` unsubscribes the optional watch. Does not abandon active membership.

`STATE <tid>`: request full snapshot.

`READY <tid> <mid> <round>`: `0 0` was previously also accepted during registration as a per-entrant readiness signal gating `START`; that registration-phase use was dropped 2026-09-11 (organizer request -- the organizer alone decides when to start, not a unanimous ready-up) and the server no longer requires or checks it there. After start, use the current match/round (round starts at 1) as before: readiness is idempotent and never automatic, match readiness expires after 60 seconds, one ready entrant wins by forfeit, neither ready yields no winner, and two ready entrants enter a 5-second countdown -- none of that changed.

`START <tid>`: organizer only, registration only, 4–16 connected entrants (registration-phase readiness is no longer required -- see `READY` above).

`LEAVE <tid>`: registration removes entrant, transfers owner to earliest remaining entrant; after start eliminates entrant and forfeits their current/future match. Disconnected entrants behave the same. Explicit ordinary PART while assigned means withdrawal. Server-directed room retirement is not withdrawal.

`REPORT <tid> <mid> <round> <winnerPid>`: assigned entrants only, active playing/reporting round only; winner must be 0 (draw) or one of the two assigned IDs. First report enters reporting and starts a 15-second deadline. Each entrant's report is immutable. Equal reports commit result once. Conflicts, or missing reports from a still-connected entrant after the deadline, enter disputed; no automatic advancement. A detected disconnect invokes the forfeit policy. Reports from old rounds cannot mutate current scores.

`RESOLVE <tid> <mid> <winnerPid>`: existing admin_authorized connections only, disputed match only. 0 replays the disputed round without awarding a win; a valid assigned winner commits one round win. Result logs must identify the administrative resolution.

`CANCEL <tid>`: organizer during registration; admin_authorized only after start. End all reserved rooms, mark cancelled, retain snapshot.

Server limits are bounded at compile time for v1; expose no unbounded allocations or retained subscriptions. Rate-limit TOUR commands per connection with sufficient room for normal snapshots and readiness; report errors without disconnecting valid ordinary play.

## Full snapshots

Every mutation broadcasts a complete, atomic, one-line snapshot. This is intentionally not a delta protocol: any newer snapshot can replace an older one without gap recovery. Normal send_line limits exceed the bounded snapshot size.

`TOUR_STATE: tid revision status ownerPid selfPid stage championPid entrants matches`

status: `registration`, `running`, `complete`, `cancelled`.

entrants: `pid,nick,state,ready;...` or `-`; state is `entered`, `alive`, `eliminated`, `withdrawn`, `champion`. ready is 0/1 (registration readiness).

matches: `mid,stage,a,b,wa,wb,state,round,readyA,readyB,remaining;...` or `-`.

Match IDs follow tree order: all opening matches, then successive stages; stage is zero-based. a/b are entrant IDs, 0 for unresolved/empty slots. wa/wb are series wins 0–2. state is `waiting`, `ready`, `countdown`, `playing`, `reporting`, `disputed`, `complete`, `bye`, `forfeit`. Terminal winner is the entrant with 2 wins; a bye uses 2–0; a double forfeit has 0–0 and no winner. round increments for every new game attempt, including draws and administrative replays. Do not increment until retirement of the previous round. Remaining is a nonnegative readiness/countdown/reporting deadline in seconds, 0 when none.

Snapshot selfPid is the receiving connection's tournament entrant ID, or 0 for a viewer. Viewing must not accidentally convert membership to observer status. Clients maintain snapshots keyed by tournament ID and ignore older/equal revisions. STATE with unknown/expired ID returns NOT_FOUND; clear that view.

## Gameplay adapter

Use a fresh reserved two-player room for each game round, keeping series state in the tournament coordinator. This reuses the initial game handshake and avoids the legacy round-ready auto-response. Neither room size nor the ordinary five-board arrays represents the tournament roster.

On countdown completion, send to each assigned client, before GAME_CAN_START:

`TOUR_ASSIGN: tid mid round aId aNick bId bNick`

The first entrant is gameplay leader. Client constructs its currentGame with those two names, maxPlayers=2, creator=aNick, sets isLeader by own nickname, clears old gameplay/sync queues, and prepares a fresh game. Server sends normal GAME_CAN_START and runs existing OK_GAME_START/LEADER_CHECK_GAME_START handshake. Reserved rooms do not appear joinable to outsiders. Reuse bubble level generation/sync and ordinary gameplay messages inside this room.

Structurally fixed regardless of `CREATE`'s options: two players, two wins per series (best-of-three across separate rooms, one round per room), teams off, mouse/touch aiming on. Game mode, attack mode, chain reactions, aim guide, and colors instead follow whatever ruleset the organizer chose in `CREATE`'s options blob (server zero-initialized defaults if it carried none), applied identically to every match's room — these are not locked, and a client must not re-force them at game start. Client displays series score from tournament snapshots (a fresh game room has no series history).

Normal F messages can convey gameplay outcome to the peer but never score the tournament or emit legacy round-result announcements from reserved rooms. Each client sends REPORT when it observes the result. Ignore legacy n in reserved rooms. On result commit/dispute/forfeit, retire that game's reserved room without departure-result side effects and send:

`TOUR_RETURN: tid mid round`

Clients clear ordinary room/network gameplay state but keep tournament snapshot and last round stats. Post-round ENTER returns to bracket; Ready there starts only the next expected round. Disputes remain visible until resolution. A server-directed return that arrives before local outcome must stop that round safely and display the tournament status rather than inventing a win.

CONTROL and GAMEPLAY lines can be coalesced or fragmented over TCP or WebSocket. No remaining lines from a read may be dropped after a text command in priority mode. Lifecycle mutations must respect net.c's iteration lists and deferred disconnect handling.

## Compatibility

Do not bump global protocol version. Old clients play normal rooms on new servers unchanged. New clients probe TOUR CAPS on old servers and keep the normal lobby. Reset all tournament capability, snapshots, assignment and readiness state on disconnect/server change. Tournament UI must use shared parsing on native and WASM.
