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

| Site | File | Worst case |
|---|---|---|
| `SendCommand`'s unconditional 100 ms `select()` | `src/networkclient.cpp:333-353` | 100 ms **per command** |
| Lobby idle (`RequestList` every 500 ms) | `src/mainmenu_netpanel.cpp:426`, `:1634` | **~20 % of wall-clock blocked while idle** |
| `DetectGeoLocation` (2 × `popen(curl)`) | `src/networkclient.cpp:2050-2103` | 16 s |
| `Connect` (resolve + connect + drain `SERVER_READY`) | `src/networkclient.cpp:102-292` | 8 s + unbounded DNS |
| `SendNick` / `CreateGame` / `JoinGame` retry loops | `src/networkclient.cpp:385`, `:452`, `:535` | 3 s each |
| Leader `GAME_CAN_START` poll | `src/networkclient.cpp:1556-1593` | **15 s** (its own comment says 5 s — wrong) |
| `WaitForBubble*` family | `src/networkclient.cpp:848-1001` | 5 s each |
| `SyncNetworkLevel` (40 waits) | `src/bubblegame_level.cpp:193-340` | **~200 s** |
| `R` refresh of public server list (unthreaded) | `src/mainmenu_input.cpp:164` | 16 s + 2 s × N |

**A single ENTER on "connect" can freeze the UI for ~30 seconds.** No spinner, no
cancel, no repaint — on macOS the OS paints the window as "not responding".

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

Status: **1a and 1d landed; 1b/1c/1e/most of 1f still open**

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
- **1b. `SendNick`/`CreateGame`/`JoinGame` async on native — not started.**
  Delete the `#ifdef __WASM_PORT__` forks and their 20-iteration
  `SDL_Delay(50)` retry loops; make the pending-flag path unconditional. Add a
  deadline so a silent server fails visibly. NICK gains a pending path too (it
  has none on WASM today).
- **1c. `SendCommand` fire-and-forget — not started.** Delete the
  `select()`+`recv()`+`strtok` block. Only safe *after* 1a and 1b, because the
  retry loops depend on that inline read populating `lastErrorResponse`. Also
  fixes the inbound half of audit `BUG-017`.
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
- **1e. Bound `NetBotConnection::JoinRoom`'s connect — not started**
  (`src/netbot.cpp:299`).
- **1f. Bare delays — partially done.** The two `SDL_Delay` calls in the
  DO_CONNECT nick/geoloc chain (100ms, 500ms-adjacent context) were removed as
  a side effect of 1d's decoupling; `IsPendingCreate()`/`IsPendingJoin()`
  status rows are still not surfaced (depends on 1b).

## Stage 2 — Async connect + minimal connecting UI

Status: **not started**

- **2a.** `ConnectionState` gains `RESOLVING` and `AWAITING_READY`; `CONNECTING`
  becomes a state that actually survives a frame (today it is written at
  `networkclient.cpp:188` and never observed). `Connect()` becomes kickoff-only.
- **2b.** `getaddrinfo` moves to a detached worker writing into a `shared_ptr`
  result slot, so a cancelled op never touches a freed client. There is no
  usable async resolver in the dependency set; a thread is the portable option.
- **2c.** The `SERVER_READY` handshake becomes frame-driven with a deadline,
  replacing the 3 s drain loop.
- **2d.** Fix `IsConnected()` — it returns true while `CONNECTING`, which becomes
  an active landmine once that state persists. Every caller needs auditing.
- **2e. UI.** `MenuReturnKey`'s `DO_CONNECT` block becomes "kick off and return".
  Per CLAUDE.md's input-parity rule the connecting state needs all four: an
  indicator, ESC/B cancel, a **tap target** for cancel, and a
  `menulist::DrawFooterHint`. Note `MenuEscapeKey`'s mode-10 branch neither
  disconnects nor clears `pendingLobbyConnect`, unlike mode 7's broad `else`;
  `connectErrorMsg` is currently never cleared by ESC at all.

## Stage 3 — Async game start and level sync (highest risk)

Status: **not started**

- **3a.** The leader `GAME_CAN_START` poll becomes a frame-driven deadline,
  adopting the `wasmBotWaitStart` pattern. Retires `leaderWaitTick()`, whose
  only purpose is pumping bots from inside the blocking loop — stage 1a's hook
  takes that over. Also removes a re-entrancy hazard: `Update()` inside
  `WaitForBubble` re-enters `HandlePushMessage` and can recurse into this poll.
- **3b.** `WaitForBubble`/`WaitForNextBubble`/`WaitForTobeBubble` and
  `SyncNetworkLevel` become a frame-driven state machine. These waits are *not*
  `#ifdef`-guarded today — they compile into WASM too. `NewGame` and
  `ReloadGame` are both reached from render functions, so the sync must yield.

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

---

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

1. **Round 2+ always burns the full 5 s sync timeout.** The gate at
   `src/mainmenu_netpanel.cpp:319-341` tests `MessageQueueSize()`, which counts
   only `messageQueue` — but `ProcessNetworkMessages` has already moved
   `b|`/`N`/`T` into `syncQueue`. The count can never reach 40. (Stage 3c.)
2. **WASM `Disconnect` never clears `syncQueue`**, leaking stale sync messages
   into the next connection.
3. **CREATE confirmation is heuristic** — any non-`PART` `OK` confirms it, so an
   unrelated `OK` can falsely confirm a pending CREATE.
4. **WASM `SendNick` sets the nick optimistically** and never retries, risking a
   `myPlayerId` mismatch when the server truncates or rejects it. (Stage 1b.)
