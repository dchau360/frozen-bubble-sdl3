# 02 — Network Client and Synchronization Audit Notebook

## Scope

Task 4 covers the native TCP and browser WebSocket `NetworkClient`
implementations, their shared lobby/game protocol parser, identity mapping,
message and synchronization queues, round transition traffic, and the existing
headless bot harness. The full-review set was `src/networkclient.cpp`,
`src/networkclient_wasm.cpp`, `src/networkclient.h`, `src/socket_compat.h`,
`src/bubblegame_net.cpp`, `tools/net_bots.py`, and
`tests/net_bots_test.py`. Focused boundary review included
`src/bubblegame.cpp`, `src/bubblegame_state.cpp`,
`src/bubblegame_render.cpp`, `src/bubblegame_shooter.cpp`,
`src/bubblegame_level.cpp`, `src/bubblegame.h`,
`src/mainmenu_netpanel.cpp`, `server/game.c`, and the original Perl client.

Production source and configuration were read-only. Security-specific runtime
traffic was omitted by user direction; all security conclusions below are
source-supported causal proofs, not observed exploit results.

## Trust boundaries and invariants

### Transport, thread, and ownership map

| State or resource | Producer/mutator | Consumer | Ownership and serialization invariant |
|---|---|---|---|
| Native `sockfd`, `state`, `recvBuffer`, and lobby models | `Connect`, `SendCommand`, per-frame `Update`, response handlers, `Disconnect` | Main menu and game loop | Main-thread singleton use; no concurrent socket reader is present. The public-server worker uses only static discovery/latency helpers and returns through separately synchronized menu state. |
| WASM WebSocket handle and callbacks | Browser event callbacks registered with `createOnMainThread=true`; shared protocol handlers | Browser main loop and per-frame game code | Callback and frame work are serialized on the browser main thread, so the reviewed queue access does not require a mutex. The raw callback `userData` handle is deleted by `Disconnect`, including from close/error callbacks; callback-teardown ordering was not runtime-verified. |
| `messageQueue` | Native stream parser or WASM message callback | `ProcessNetworkMessages` and synchronous wait helpers | FIFO order is assumed. The queue is unbounded. During play, `ProcessNetworkMessages` drains it every rendered frame. |
| `syncQueue` | `ProcessNetworkMessages` routes `b`, `N`, and `T` | `WaitForBubble`, `WaitForNextBubble`, `WaitForTobeBubble` | FIFO order and exactly 38 board messages followed by `N` and `T` are assumed. There is no round number, sender validation, deduplication, or upper bound. |
| Player-ID map and `myPlayerId` | `GAME_CAN_START` lobby push | binary game sender prefix, board-array routing, display names | The client assumes the server-provided mapping is authoritative. Task 3 proved the server does not bind a relayed first-byte sender ID to the connection (SEC-004). |
| `currentGame` | `new GameRoom` in create/join/response paths | menu, leader test, options, state transitions | Raw heap pointer with no deleting owner. Several exit paths assign null directly, causing BUG-013. |
| Round-ready/stat flags | `n`, `F`, and `S` handlers; local round state | render-loop transition and post-round table | Normal clients send once over reliable ordered transport. Receivers do not deduplicate by sender; the invariant is conventional, not enforced. |

### Protocol and synchronization invariants

- Lobby traffic is newline-delimited `FB/1.3` text. In-game traffic is
  `{sender-id byte}{payload}\n`. For a priority fd, the server explicitly diverts
  an `FB/`-prefixed line back to its text parser; other lines reach the game
  relay. That diversion makes mid-game `PART` valid and dismisses BUG-012.
- `GAME_CAN_START` maps the local nickname to `myPlayerId`; remote IDs are
  assigned to board arrays. This mapping is structurally bounded by the room
  player count, but its identity authenticity depends on SEC-004.
- A leader sends the 38 initial board cells in deterministic row/column order,
  then one `N` and one `T`. A joiner consumes the same count/order. TCP and each
  WebSocket message path preserve ordinary order, but the client neither checks
  that sync messages came from the leader nor labels them with a round.
- During a round, `f` records the peer shot, `s` supplies the authoritative
  stick cell/next colors, `g` adds malus, `m`/`M` animate and place malus
  bubbles, `F` ends the round, `S` contributes its stats row, `n` marks the next
  round ready, `l` marks a departure, and `A`/`r` update targeting.
- `ReloadGame` resets per-round game flags and then invokes level sync. It
  relies on all preceding `b`/`N`/`T` messages having been routed and on no
  stale synchronization messages surviving a connection lifecycle.

### Length and queue proof

| Path | Bound or framing behavior | Disposition |
|---|---|---|
| Native lobby input | `recvBuffer[4096]` retains incomplete normal lines in `ProcessIncomingData`; a chunk that would fill the buffer is silently skipped. `SendCommand` instead performs one 100 ms read and passes every `strtok` fragment to the parser, even without a terminating newline. The initial greeting also parses each `recv` independently. | Ordinary server messages fit the buffer, but response state/correlation establishes BUG-015 and split greeting tokens establish BUG-017. Overflow/flood behavior remains part of the untrusted-input limitation. |
| Native game input | A retained 4096-byte stream buffer extracts the binary ID and newline-delimited payload; `gameMsg` and formatted queue entries use the same fixed bound. | Normal emitted payloads fit. Sender identity and numeric payloads are not authenticated/validated (SEC-003/SEC-004). |
| WASM input | Each WebSocket event is parsed independently and copied into 4096-byte local buffers. A final event fragment without newline is treated as a complete line rather than retained. | The audited server emits a complete logical buffer per WebSocket frame in normal operation, so no ordinary defect was promoted. Fragmentation/proxy behavior remains unverified. |
| Outbound game data | `snprintf` return values are used as send lengths without a truncation rejection. All repository call sites supply bounded protocol strings far below 4096 bytes. | No reachable ordinary defect in the scoped call graph; retain as hardening work rather than a finding. |
| Queue growth | Both deques have no message/byte cap. The game drains the main queue per frame, while sync entries wait for level consumption. | A peer/server can grow work and memory, but no security load test was permitted. This is recorded with the peer-input boundary rather than claimed as a measured denial of service. |

## Static review

### Connection and room lifecycle

`PartGame()` sends lobby-text `FB/1.3 PART\n` even from `IN_GAME`. A focused
server recheck disproved the initial concern about this transition:
`server/net.c` recognizes `FB/` on a priority fd and calls `process_msg`, whose
`PART` branch removes the player and priority membership before returning `OK`.
The original Perl client reconnects instead, but that implementation difference
does not establish a defect. BUG-012 is dismissed and its ID is reserved.

Both disconnect implementations discard a live `currentGame` pointer without
deleting it. Native disconnect additionally preserves `recvBufferLen`, IDs,
maps, pending state, and options; WASM disconnect omits `syncQueue.clear()` and
also preserves IDs/maps/options. A later connection can therefore leak every
prior room and, depending on the transport, parse an old partial native line or
consume old browser sync entries in the next game. This proves BUG-013. The
WASM practice of deleting the WebSocket handle from its own close/error callback
was not promoted without browser callback-lifetime evidence.

### Lobby response and reachability handling

Native `SendCommand` performs one `send`, waits at most 100 ms, performs at most
one receive, and parses a non-newline-terminated tail as if complete. `SendNick`,
`CreateGame`, and `JoinGame` then delay 50 ms without pumping `Update`; an absent
immediate error is interpreted as success. `HandleServerResponse` recognizes
only `NICK_IN_USE`, `NO_SUCH_GAME`, and `ALREADY_IN_GAME`; native CREATE ignores
even recognized errors other than `NICK_IN_USE`, while JOIN cannot reject
unrecognized `GAME_FULL` or `INVALID_NICK`. Thus complete immediate errors and
late/partial errors can all manufacture native success.

WASM pending state avoids the 100 ms wait but not the response-model defect.
Unrecognized `GAME_FULL`, `INVALID_NICK`, and `ALREADY_MAX_OPEN_GAMES` leave the
operation pending, and any response containing `OK` other than `PART` is accepted
as the pending CREATE/JOIN response without command correlation. BUG-015 covers
both transports' incomplete error and response correlation.

### Native TCP stream semantics

The connection greeting uses a fresh local buffer for every `recv` and searches
each fragment independently for `SERVER_READY`. TCP may split that token at any
byte, in which case neither fragment matches and the client times out. Outbound,
`SendCommand` treats every nonnegative `send` as complete; `SendGameData` logs a
short send but still returns success. A later write can then concatenate with an
incomplete protocol line. These ordinary stream-assumption failures are BUG-017.

`MeasureLatency` makes a nonblocking connect and treats writable `select` as
success without checking `SO_ERROR`; failed connects are also writable. The
server list can consequently show a refused endpoint as reachable (BUG-016).
The primary native `Connect` remains a blocking OS connect with no client
deadline, an additional responsiveness limitation folded into the same
connection-management remediation.

On Windows, `MSG_DONTWAIT` is defined as zero while the primary `Connect` never
performs the promised post-handshake `ioctlsocket(FIONBIO)`. The first ordinary
per-frame `recv` can therefore block the game loop. The client also narrows
Winsock's pointer-sized `SOCKET` into `int` in the primary and latency paths,
ignores `socket_init()` failure, and never balances startup with cleanup. These
facts extend REL-003 from the server to the native Windows client and raise its
impact to a shipped-platform failure.

### Peer messages and round flow

SEC-003 is confirmed statically. `s` accepts unbounded `cx`, `cy`, and color,
stores them as pending stick state, and the shooter later calls
`PlacePlayerBubble`. `m` stores unbounded cell fields and a matching `M` calls
the same placement function. `PlacePlayerBubble` directly indexes
`bubbleMap[row][col]`. Other peer fields include unbounded `g` loop counts and
stats arithmetic, unchecked colors/angles and sync values, and `OPTIONS` values
parsed by uncaught `std::stoi`. The server transparently relays game payloads;
SEC-004 additionally allows sender/leader impersonation. A connected peer can
therefore drive out-of-bounds board access or client termination. No hostile
message was sent as part of this audit.

For WASM round 2+, `Render` first drains `messageQueue`, routing all `b`/`N`/`T`
entries into `syncQueue`. It then waits for `MessageQueueSize() >= 40`; that size
remains zero for the very sync traffic being awaited. Every joining browser
client therefore reaches the five-second timeout before invoking a level reload
whose sync queue was already ready. This proves BUG-014.

The ordinary `f`/`s`, malus, finish, stats, ready, leave, and target paths are
internally consistent when every sender follows the protocol once and transport
order is preserved. Receiver-side ready/stats deduplication and leader-only sync
validation are absent; under hostile/repeated traffic those gaps join the
SEC-003/SEC-004 boundary rather than establishing a separate ordinary-flow bug.

### Bot harness fidelity

The bot retains partial lobby and game lines, uses `sendall`, parses the binary
ID mapping, emits syntactically valid `f`/`s`, propagates a thread failure, and
closes only its own sockets. Its six unit tests cover helpers and failure
propagation. It does not create or start a room, generate/validate `b`/`N`/`T`,
assert board state, validate finish/stats traffic, impose a whole-run timeout,
or prove two completed rounds. It requires an interactive human-created room.
Its comment that the C++ receiver guards repeated `n` messages is also stronger
than the implementation: the receiver increments a count without sender
deduplication, while normal senders merely send once.

## Dynamic evidence

| Command | Result | Meaning |
|---|---|---|
| `python3 tests/net_bots_test.py` | Exit 0; 6 tests passed | Protocol helper and failure-propagation unit coverage passes. No socket/server integration occurs. |
| `ctest --test-dir build-audit-sanitize -R net-bots-test --output-on-failure` | Exit 0; 1/1 test passed in 0.30 s | The retained sanitizer-build registration passes; it runs the same Python suite, not a sanitized C++ client. |
| `emcmake cmake -S ... -B /tmp/fb-sdl3-audit/task4-wasm-build -G Ninja -DCMAKE_BUILD_TYPE=Release` | Exit 0 | The browser target configures in an isolated temporary build. |
| `cmake --build /tmp/fb-sdl3-audit/task4-wasm-build --parallel` | Exit 1 | Compilation stops at the repository-documented unpatched SDK boundary: `SDL3_image/SDL_image.h` and `SDL3_mixer/SDL_mixer.h` are absent. No audited client compile/runtime inference is made from this failure. |
| direct `em++ -D__WASM_PORT__ -sUSE_SDL=3 -c` of `src/networkclient.cpp` | Exit 0; two unused-variable warnings | The shared client translation unit compiles for WASM independently of the missing image/mixer ports. |
| direct `em++ -D__WASM_PORT__ -sUSE_SDL=3 -c` of `src/networkclient_wasm.cpp` | Exit 0; three warnings | The WebSocket transport translation unit compiles for WASM; warnings are two C++17 variadic-macro extension diagnostics and unused `s_handle`, not compile failures. |
| read-only listener inventory before the smoke decision | Exit 0 | Pre-existing `fb-server` listeners on 15511, 15512, 15113, and 15998 were observed and left untouched. |

No two-round smoke was run. The available bot cannot create/start a room or
assert either sync round and requires interactive graphical leadership. Starting
a server and traffic would therefore not establish the requested result, and
the existing foreign listeners also preclude reuse of fixed-port harnesses under
the exact-child/dynamic-port cleanup rules.

## Candidates

| Candidate | Final state | Reason |
|---|---|---|
| SEC-003 | confirmed, High | Direct peer numeric fields reach unchecked board indexing and uncaught conversion; runtime exploit traffic intentionally omitted. |
| BUG-012 | dismissed | Priority ingress diverts `FB/` to the text parser, whose `PART` branch removes both room and priority membership. ID reserved permanently. |
| BUG-013 | confirmed, Medium | Room ownership leaks and transport/session fields survive disconnect, permitting stale parsing/sync across reconnection. |
| BUG-014 | confirmed, Medium | WASM round transition waits on the queue that was just drained instead of `syncQueue`, forcing a five-second timeout. |
| BUG-015 | confirmed, Medium | Native commands accept silence/partial/unrecognized errors; WASM pending operations omit errors and correlate any unrelated non-PART `OK`. |
| BUG-016 | confirmed, Low | Native reachability omits `SO_ERROR` and can report a refused connect as online. |
| BUG-017 | confirmed, Medium | Native greeting and writes assume one TCP call preserves a logical record; split greeting tokens and short writes are mishandled. |
| REL-003 extension | confirmed, High | Native Windows client combines `SOCKET` narrowing with a blocking per-frame receive path. |
| Task 4 slice of IMP-005 | dismissed as a defect | `ready`, `started`, `port`, and native buffer bytes are assigned before each reviewed use; remaining IMP-005 owners are Tasks 5-7. |
| WASM per-event fragment retention | dismissed for ordinary flow | The audited server sends each logical buffer as one WebSocket frame; proxy/browser fragmentation was not dynamically established. |
| Outbound `snprintf` truncation | dismissed for scoped reachability | Every repository call site supplies a bounded payload substantially below the buffer. |

## Confirmed findings

- **SEC-003 (High):** peer numeric data reaches unchecked board indexing and
  exception-throwing parsing. SEC-004 magnifies the identity/leader boundary.
- **BUG-013 (Medium):** connection teardown leaks room objects and retains
  session data capable of contaminating reconnection.
- **BUG-014 (Medium):** browser round 2+ synchronization observes the wrong
  queue and waits for the timeout.
- **BUG-015 (Medium):** native and WASM command state machines omit ordinary
  errors and do not reliably correlate success with the pending operation.
- **BUG-016 (Low):** native reachability can classify failed nonblocking
  connects as successful.
- **BUG-017 (Medium):** the native client mishandles a fragmented greeting token
  and accepts short outbound writes as complete.
- **REL-003 (High):** the existing Windows socket portability finding also
  affects the native client and includes a blocking per-frame receive path.

## Dismissed candidates

- Analyzer-reported uninitialized Task 4 members are assigned on every audited
  construction/use path; raw `recvBuffer` bytes are protected by a zero initial
  length. The Task 4 portion of IMP-005 is therefore closed without a defect.
- BUG-012 is dismissed by the priority-ingress `FB/` diversion and the text
  parser's explicit `PART`/`remove_prio` branch. Its ID will not be reused.
- Valid production outbound payloads cannot reach the fixed-buffer truncation
  path. The API should still reject truncation if it is generalized.
- The WASM parser does not retain a line split across browser message events,
  but the scoped server WebSocket sender preserves one relay buffer per frame.
  No ordinary-flow failure was proven.
- `GAME_CAN_START` mapping loops and LIST room-cap parsing were checked for
  bounded access. No Task 4 defect was found in their ordinary input range.
- Task 4 instances of broad IMP-008/IMP-009 analyzer advice are cleanup
  opportunities, not defects. Their remaining subsystem owners retain the
  registry entries.

## Coverage

All Task 4 implementation, header, compatibility, harness, and test files were
read completely. Focused consumers were traced through board placement, render
round transitions, room exit, lobby presentation, server priority relay, and
the original Perl reconnect behavior. Per-file dispositions are recorded in
[FILE_COVERAGE.md](../FILE_COVERAGE.md). Required bot checks passed; no source or
test file was changed.

## Limitations

- Security-specific runtime traffic, malformed frames, queue flooding, identity
  spoofing, and hostile numeric injection were explicitly omitted. SEC-003 is a
  source-supported conclusion, not runtime reproduction.
- The existing bot harness cannot autonomously establish a room, start play,
  validate level synchronization, or prove two rounds; no qualifying smoke was
  available.
- No native graphical multiplayer session was driven. Dynamic proof is limited
  to the existing six-test bot unit suite.
- The Homebrew Emscripten 6.0.4 installation is not patched with the repository's
  SDL3_image/SDL3_mixer port files. The full isolated target therefore did not
  build, although both audited client translation units compile directly for
  WASM with warnings. No link, browser, WebSocket proxy, callback-lifetime, or
  round-transition runtime claim is made.
- Windows socket behavior was not executed on Windows. REL-003's client extension
  follows directly from the compatibility macro and missing nonblocking call.
- WebSocket close/error callback teardown ordering was not exercised. It is a
  Task 10 browser-runtime boundary, not an unresolved Task 4 candidate.
- Existing foreign server listeners were observed read-only and were not
  signaled, connected to, or otherwise modified.

## Gate conclusion

Task 4 is complete with SEC-003, BUG-013 through BUG-017, and the native-client
extension of REL-003 confirmed; BUG-012 is dismissed and reserved. Every Task
4-local candidate has a final disposition, required tests pass, production
code/configuration remain unchanged,
and the runtime/WASM exclusions above are explicit. Task 5 may consume the
validated network-message and round-state boundary.
