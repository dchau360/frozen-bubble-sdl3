# Async networking rearchitecture — progress and handoff

Spun out of `docs/OPTIMIZATION_HANDOFF.md` item A ("Keep networking and server
startup responsive"), which landed two bounded fixes in `00faeaf4` and
explicitly deferred the larger rearchitecture to its own batch. This doc is that
batch. It is a multi-session, four-stage effort; each stage is independently
shippable and taggable.

## Purpose

Item A's deferred scope, verbatim from the original:

> advance connection/handshake/startup states from the main loop, move blocking
> name resolution off that loop, and queue partial writes on non-blocking
> sockets … demonstrate that input and rendering continue during waits; measure
> worst frame stalls.

Preserve: protocol ordering, hosted-bot servicing, cancellation, deadlines, and
complete lines across partial sends. Reuse the platform's existing async paths
rather than creating a second protocol implementation.

## The problem, measured

Every one of these blocks the render loop on native. Worst cases read from the
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

## The approach

**The async implementation already exists.** The Emscripten build cannot block,
so it already has frame-driven equivalents for nearly all of this:
`pendingCreate`/`pendingJoin`/`pendingLobbyConnect`, `wasmSyncWaitStart`,
`wasmBotWaitStart`, and completion handling in `HandleServerResponse`.

So this is mostly a **deletion** job: remove the native-only blocking branches
behind `#ifdef __WASM_PORT__`, make the async path unconditional, and give native
a per-frame transport pump. Platform divergence shrinks, and the WASM-only bugs
listed at the bottom get fixed once, for both platforms.

Scope: client **and** server (stage 4 closes audit `BUG-007`, the same design
defect on the other side of the wire). Connect UI is deliberately minimal —
"Connecting…" plus cancel, not a progress design.

---

## Stage 1 — Unify on the async command path, kill background stalls

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
  set the nick optimistically and never retried a collision at all (bug 4
  below); now a `NICK_IN_USE` retries with a numeric suffix up to 20 times on
  both platforms, exactly like `CREATE`/`JOIN` already did on WASM.
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

## Stage 2 — Async connect + minimal connecting UI

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

## Stage 3 — Async game start and level sync (highest risk)

Status: **3a and 3c landed; 3b partially landed (stall mitigated for both**
**call sites; a full state-machine rewrite of `WaitForBubble`/`SyncNetworkLevel`**
**themselves remains open and needs genuine two-client verification)**

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
  it returns once 3b lands.
  New coverage in `netconnect_test.cpp` drives the real path (`NICK` → `CREATE`
  → `START` → a `GAME_CAN_START` push in the server's own wire format) against
  `fake_server.h`, now extended with a small canned-reply mechanism (`Rule`:
  match a line substring, answer with the next entry in a scripted list). Two
  cases: a joiner that answers "not ready" a few times then "OK" (worst frame
  0 ms, ~370 ms total — the ordinary path a real localhost server answers too
  fast to exercise), and a joiner that never answers at all (worst frame 2 ms,
  ~5 s total, bounded by the deadline rather than the old loop's 15 s, ~267
  frames pumped throughout the wait rather than the loop sleeping through it).
- **3b. Partially landed — the stall is closed for the common case; the**
  **internals are still synchronous for the fallback case.**
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
  full 5 s timeout too, on both platforms, independent of and in addition to**
  **the lobby-entry gate's own bug.** Fixed the same way, with the same
  pure rule. Also fixed a latent leak this uncovered: neither this gate's
  timestamp (`wasmRoundSyncWaitStart`, now `roundSyncWaitStart` and no longer
  `#ifdef`-guarded) nor the lobby-entry one's had ever been reset on starting
  a fresh match, so a stale value surviving from a quit-mid-wait match could
  make the very next match's first use of the gate measure "waited" against
  the wrong clock and read as already timed out, silently skipping the wait
  it exists to do.
  What remains open: the residual case where the gate's own 5 s timeout is
  hit with messages still missing. `WaitForBubble`'s loop is still reached in
  that case and can still block per remaining message, exactly as before and
  exactly as WASM already accepted as its fallback in 3c — turning that into
  a real state machine is the rest of 3b, unattempted here given the effort
  budget and the correctness risk of touching that math with no test harness
  that can drive two real network clients through a full match. No automated
  regression test was written for the two closed call sites either, for the
  same reason: nothing here can spin up `BubbleGame`/`MainMenu` headlessly
  and drive a real multi-round match. `bot-play-test`/`netbot-test`/
  `net-bots-test` (a real `fb-server` plus bots) all still pass, but none of
  them exercises this path — a bot is a level-sync *leader*'s local
  bookkeeping problem, not a joiner waiting on one. **Verifying this needs
  a genuine two-client multi-round game** (see the manual-test note below).
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
  Also fixes WASM bug 2 below: WASM's `Disconnect()` never cleared `syncQueue`
  (native's always has), so a round's leftover sync messages survived into the
  next connection.

## Stage 4 — Server output queue (audit BUG-007)

Status: **not started**

`server/net.c`'s `send_line()` (`:157-177`) is a bare blocking `send()` with no
queue, and `connections_manager()` (`:616`) builds a read-set only — there is no
`O_NONBLOCK` anywhere in `server/`. One peer that stops reading stalls every
other player.

Add a per-fd FIFO output buffer; set accepted fds non-blocking; add a write-set
to the `select()`; flush on writable; drop a peer whose queue exceeds a size or
age cap. `send_line` appends rather than sending. FIFO ordering must be
preserved — the protocol is order-dependent.

---

## What's been verified so far (1a + 1d, 2026-09-08)

Full native build (`cmake --build build`) clean across every target,
`ctest --test-dir build`: 27/27 runnable tests passed (2 sanitizer-only skips
as expected), including the new `StartLanFetch` async-kickoff regression block
in `menu-touch-gesture-test`.

WASM was **not** rebuilt this session — the local Emscripten install's Python
tooling fails on an unrelated syntax error (`emmake.py` uses `match` syntax
against what's invoking as Python 2). The `#ifdef __WASM_PORT__` branches
touched here (`StartGeoLocFetch`'s WASM path, the `#ifdef` guards around
`StartLanFetch`/`StartPublicServerFetch`, `ShowPanel` case 3) were reviewed by
inspection against the pre-existing `serverFetchThread` pattern, which uses
the identical conditional-compilation shape and already compiles for WASM. A
real WASM build should still confirm this before the next tag.

## What's been verified so far (1b, 2026-09-08)

Full native build clean. Plain `ctest --test-dir build`: 29 tests, 27 run /
100% pass (2 sanitizer-only skips as expected), including the new NICK_IN_USE
end-to-end test described above. Sanitizer build rebuilt
(`cmake --build build-asan`) and re-run without `detect_leaks=1` — macOS/Darwin
does not support ASan leak detection at all (confirmed directly: running a test
binary under `detect_leaks=1` aborts immediately with "AddressSanitizer:
detect_leaks is not supported on this platform", a pre-existing local-environment
limitation, not a code defect; the documented `detect_leaks=1` command in
CLAUDE.md targets Linux CI, where it is supported). Under
`UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-asan`, all 29 tests
run and pass, including the two sanitizer-only ones — confirming the new
recursive-call pattern (a reply processed synchronously inside `SendCommand()`,
inside `SendNick()`/`CreateGame()`/`JoinGame()`, inside `HandleServerResponse()`)
is memory- and UB-safe.

WASM was again not rebuilt (same local toolchain issue as above); the
`Disconnect()` and `HandleServerResponse()` changes were reviewed by inspection
— `networkclient_wasm.cpp`'s own copies of the touched blocks use the same
`pendingNick`/`pendingCreate`/`pendingJoin` fields already shared via
`networkclient.h`, so no new `#ifdef` branch was introduced for WASM to diverge
on. A real WASM build should still confirm this before the next tag.

## What's been verified so far (1c/1e/1f, 2026-09-08)

Full native build clean. Plain `ctest --test-dir build`: 29 tests, 27 run /
100% pass — notably `menu-touch-gesture-test`'s NICK_IN_USE end-to-end test
(now genuinely exercising the multi-frame async path, since `SendCommand()`
can no longer resolve it synchronously) and `net-bots-test`/
`server-bot-cap-test` (exercise `NetBotConnection::JoinRoom()` end-to-end
against a real server) still pass. Sanitizer build rebuilt and re-run
(`UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-asan`, no
`detect_leaks=1` per the macOS limitation above): all 29 run and pass,
including the two sanitizer-only ones, after each of the three changes in
this batch (1c's `SendCommand` deletion, 1e's `netbot.cpp` connect fix, 1f's
`AddStatusMessage` failure-feedback additions) — rebuilt and re-tested
separately after each, not just once at the end.

WASM was not rebuilt (same local toolchain issue as above). 1c's change is
native-only (`#ifndef __WASM_PORT__`, WASM's own `SendCommand` was already
async). 1e's `netbot.cpp` change is inside the same `#ifndef __WASM_PORT__`
block the pre-existing blocking connect lived in; WASM's `JoinRoom` (a
separate `#else` implementation using `emscripten_websocket_new`) was not
touched. 1f's `AddStatusMessage` calls are inside `HandleServerResponse()`,
shared by both platforms, and reviewed by inspection.

---

## What's been verified so far (stage 2, 2026-09-08)

`tests/fake_server.h` and `netconnect-test` were built **first**, before the
state-machine rewrite, specifically so the rewrite had deterministic coverage
of the cases it exists to handle. That ordering paid for itself immediately:
the fixture failed on its first run against then-shipping code and exposed the
split-banner bug (see stage 2c), which no test against a real localhost server
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
neither) that stage 2 needed anyway. There is still no automated WASM
coverage and the local Emscripten toolchain is broken, so **a real WASM build
must exercise lobby entry before the next tag** — this is the second stage
running on inspection alone for that platform.

## Verification

**Mock hostile-peer fixture** (new, `tests/fake_server.h`) — an in-test TCP
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
- **Stage 3**: two real clients (Python harness against a real `fb-server`)
  through a full 2-round game; assert round 2 sync completes well under 5 s,
  which fails on today's code (see WASM bug 1 below).
- **Stage 4**: harness client that connects and stops reading; assert other
  clients keep being served.

Existing infra to reuse: `frozen-bubble-core-test` + `FROZEN_BUBBLE_TEST_ACCESS`,
the `MainMenuTestAccess`/`NetworkClientTestAccess` friend structs in
`tests/menu_touch_gesture_test.cpp`, and the Python harness that spawns a real
`fb-server`. Sanitizer-only tests use `SKIP_RETURN_CODE 77`.

Manual: extend `docs/MANUAL_TEST_CHECKLIST.md`'s network entries. Stages 3 and 4
need genuine two-client testing.

Per stage, before tagging (CLAUDE.md "Cutting a release"): bump
`CMakeLists.txt`, `android/app/build.gradle` (`versionCode` must strictly
increase), `.github/workflows/build.yml`'s two fallbacks, and `default.nix`;
update `CHANGELOG.md`. CI's Linux ASan/UBSan job hard-blocks the release — the
threading added in 1d/2b is exactly what it exists to catch.

## Risks

- **`IsConnected()` semantics change** (2d) is the single most likely source of
  subtle breakage; audit every caller.
- **Hosted bots must keep being serviced during every wait** — a bot silent > 5 s
  is dropped by the server (`src/netbot.cpp:193-197`). Stage 1a's hook must land
  before stage 3 retires `leaderWaitTick`.
- **Do not regress the v2.4.88 WebSocket partial-line fix** when touching the
  buffered reader.
- Stage 3 touches game start on both platforms and is the natural place to stop
  if the earlier stages already deliver enough.

## Pre-existing WASM bugs found while planning this

Not regressions from this work — they are latent today, and get fixed as a side
effect of the stages that touch the same code.

1. ~~**Round 2+ always burns the full 5 s sync timeout.**~~ **Fixed in stage
   3c** — the gate counts both queues now, and the rule moved to a pure,
   natively-tested function.
2. ~~**WASM `Disconnect` never clears `syncQueue`**, leaking stale sync
   messages into the next connection.~~ **Fixed in stage 3c.**
3. **CREATE confirmation is heuristic** — any non-`PART` `OK` confirms it, so an
   unrelated `OK` can falsely confirm a pending CREATE.
4. ~~**WASM `SendNick` sets the nick optimistically** and never retries, risking
   a `myPlayerId` mismatch when the server truncates or rejects it.~~ **Fixed
   in stage 1b** — `SendNick` is now unified across platforms with a real
   `NICK_IN_USE` retry.
