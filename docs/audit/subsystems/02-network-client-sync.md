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

### Execution contexts and callback ownership

| Context or callback | Data touched | Thread and ownership conclusion |
|---|---|---|
| Native `Instance` methods, `Update`, and shared parsers | All instance state; socket reads and writes; lobby and game queues | Called from the game/menu loop. No second instance reader or writer was found, so mutation is serialized on the native main thread. |
| Native public-server worker | Static `FetchPublicServers`/`MeasureLatency` results only | The worker does not obtain or mutate the `NetworkClient` singleton. It publishes its result through the menu's separately synchronized state. |
| WASM `Connect` | Allocates `WebSocketHandle`, creates its Emscripten socket, stores the same handle in `websocketSocket` and `s_handle`, and registers four callbacks | The socket attribute requests `createOnMainThread=true`; setup and later callbacks are therefore serialized with the browser main loop in the reviewed build. |
| `onWebSocketOpen` | Borrows callback `userData` as `WebSocketHandle*`, follows `client`, and calls `SetConnected` | Browser main thread. It does not retain event data. The handle is owned by `NetworkClient::Disconnect`, not by the callback. |
| `onWebSocketMessage` | Borrows `e->data`/`e->numBytes`; calls `QueueGameMessage` in game state or `ParseMessage` otherwise | Browser main thread. Payloads are copied into parser/queue storage during the callback; the event buffer is not retained. A WebSocket event is treated as a self-contained parsing unit. |
| `onWebSocketClose` / `onWebSocketError` | Borrow the same handle and call `Disconnect` through its `client` alias | Browser main thread. `Disconnect` closes/deletes the socket and deletes that very callback-data handle. Callback deregistration/lifetime ordering was not dynamically verified and remains a Task 10 platform-runtime limit. |
| Destructor and static `Dispose` | Destructor calls `Disconnect`; `Dispose` deletes the singleton and nulls `ptrInstance` | No ordinary call to `Dispose` was found, so process lifetime is the normal owner. Standard members are destroyed only when the singleton is actually deleted; leaked `currentGame` allocations are not recovered after their pointer is cleared. |

### Member-by-member lifecycle matrix

The following matrix names every `NetworkClient` data member in
`src/networkclient.h`, plus the WASM callback-handle fields and file-static
handle that participate in its lifetime. “Main” means the native game/menu
thread or the serialized browser main thread described above.

| Member | Creator/default; owner and aliases | Writers and context | Readers | Replacement, reset, reconnect, and teardown |
|---|---|---|---|---|
| static `ptrInstance` | Static null pointer; owning singleton pointer returned by `Instance()` | `Instance()` allocates; `Dispose()` deletes/nulls, on caller thread | Every `Instance()` call | Not reset by connection teardown. `Dispose()` is the only singleton teardown; no ordinary caller was found. |
| native `sockfd` | Constructor sets `-1`; `NetworkClient` owns the OS descriptor | `Connect` replaces it; `Disconnect` closes and writes `-1`; main | `Connect`, `SendCommand`, `SendGameData`, `Update`, `Disconnect` | A failed connect closes before returning. Disconnect/destructor close only the current descriptor. Reconnect starts from `-1`. |
| WASM `websocketSocket` | Constructor sets null; owns a heap `WebSocketHandle`; aliased by every callback's `userData` and by `s_handle` | `Connect` assigns; `Disconnect` deletes the handle and nulls it; browser main | send methods and `Disconnect` | Replaced only after a disconnected-state connect. Disconnect/destructor close/delete the socket then delete the handle; callback-lifetime ordering is unverified. |
| `WebSocketHandle::socket` | Set from `emscripten_websocket_new`; handle owns the browser socket until disconnect | Created in `Connect`; not independently replaced | send methods and `Disconnect` through the handle | Closed and deleted by `Disconnect` before its containing handle is deleted. |
| `WebSocketHandle::client` | Set to `this` in `Connect`; non-owning back-reference | Written once per new handle | All four callbacks | Dies with the handle. Close/error callbacks follow it immediately before the handle may be deleted by `Disconnect`. |
| file-static WASM `s_handle` | Static null; non-owning alias of `websocketSocket` | `Connect` assigns; `Disconnect` nulls; browser main | No reader in the reviewed source | Carries no independent lifetime. Its being unused is compiler-diagnosed; teardown nulls it. |
| `state` | Constructor sets `DISCONNECTED` | Connect paths set `CONNECTING`/`CONNECTED`; create/join responses set `IN_LOBBY`; `GAME_CAN_START` sets `IN_GAME`; part/room-close set `IN_LOBBY`; disconnect sets `DISCONNECTED`; main/callback | Connection predicates, sends, update/parsers, menu and game | Reset on both disconnects. Other session fields are not uniformly reset with it. |
| `playerNick` | Default-empty owned string | `SendNick`, create/join requests and their async success/retry paths; main | reconnect/create/join retry and identity access | Replaced by later identity operations; retained by both disconnects and destroyed with singleton. |
| `playerGeoloc` | Default-empty owned string | `SendGeoLoc`; main | later identity send/access | Replaced by later call; retained across disconnect/reconnect; destroyed with singleton. |
| `messageQueue` | Default-empty owned deque | Native stream parser and WASM callback push; `PutBackMessage` pushes front; consumers pop; main | queue APIs, lobby/game processing and wait helpers | Both disconnects clear it; destruction frees it. It is unbounded while connected. |
| `syncQueue` | Default-empty owned deque | `PushSyncMessage` pushes; `GetNextSyncMessage` and the three sync wait methods pop; main | `HasSyncMessage`, `GetNextSyncMessage`, and sync wait methods | Native disconnect clears it; WASM disconnect does **not**, so reconnect can observe stale entries (BUG-013). Destruction frees it. |
| `gameList` | Default-empty owned vector of value `GameRoom`s | `ParseList`, `ClearGameList`, and room/list parsing; main | copy-return accessor, menu, and current-room synchronization | `ParseList`/clear replace contents; both disconnects clear it; destruction recursively frees value members. |
| `openPlayers` | Default-empty owned vector of value `NetworkPlayer`s | `ParseList` and `ClearGameList`; main | copy-return accessor/menu | Rebuilt/cleared by list paths, but neither disconnect explicitly clears it; next list refresh replaces it. Destruction frees it. |
| `chatMessages` | Default-empty owned vector of value `ChatMessage`s | status, room-close, and talk handlers append and trim to 50; main | copy-return accessor/menu | Never connection-reset, intentionally/persistently retaining prior-session chat/status until singleton destruction. |
| `currentGame` | Constructor null; raw owning-looking pointer returned as a non-owning alias by `GetCurrentGame` | Create/join success allocates or overwrites; list/push handlers mutate the pointee; part, room-close, and disconnect assign null; main | menu, leader/options logic, state transitions | No `delete currentGame` exists. Nulling loses allocations; a later create/join allocates anew, while an extant pointer may be overwritten by value. Destructor cannot free already-lost rooms (BUG-013). |
| native `recvBuffer[4096]` | Object storage; bytes initially indeterminate and owned by the singleton | `ProcessIncomingData` appends, terminates, and `memmove`s retained bytes; main | That parser, always bounded by `recvBufferLen` | Neither disconnect clears bytes. They are harmless only while length is zero; stale length on reconnect makes stale bytes live (BUG-013). Destroyed with object. |
| native `recvBufferLen` | Constructor sets zero | `ProcessIncomingData` increments, resets, and reduces it; main | capacity/framing checks in that parser | Native disconnect does not reset it, so a partial old line survives reconnect (BUG-013). Object destruction ends it. |
| `myPlayerId` | Constructor sets zero | `GAME_CAN_START` assigns when nickname matches; main | `SendGameData`, game routing and accessors | Not reset by disconnect/part/room-close. A later successful `GAME_CAN_START` may replace it, but absent/malformed mapping can retain the old ID. |
| `myNickname` | Default-empty owned string | Native `SendNick`; create/join success paths; main | `GAME_CAN_START` local-ID matching and accessor | Replaced by identity success; retained across disconnect/reconnect; destroyed with singleton. |
| `lastErrorResponse` | Default-empty owned string | Identity/create/join entry points and an `OK` clear it; selected response errors assign it; main | native synchronous success/error decisions and accessor | Only three named error strings are recorded. Disconnect does not clear it; later operations usually clear before send. |
| `playerIdToNick` | Default-empty owned map | `GAME_CAN_START` clears then repopulates; main | display-name/routing accessors | Not reset by disconnect/part/room-close; the next `GAME_CAN_START` normally replaces it. Destruction frees it. |
| `pendingOptions` | In-class `false` | `OPTIONS:` parser sets true; `GetAndClearPendingOptions` sets false; main | pending-options consumer | Neither disconnect resets it. An old unconsumed options notification can cross a connection boundary. |
| `rcvChainReaction` | In-class `true` | `OPTIONS:` parser overwrites; main | `GetAndClearPendingOptions`/menu | Retained across disconnect; next options push replaces it; destroyed with object. |
| `rcvContinueLeave` | In-class `true` | Same options parser; main | Same options consumer | Retained across disconnect; replaced on next options push. |
| `rcvSingleTarget` | In-class `true` | Same options parser; main | Same options consumer | Retained across disconnect; replaced on next options push. |
| `rcvVictoriesLimit` | In-class `5` | Same options parser via `stoi`; main | Same options consumer | Retained across disconnect; replaced on next options push. |
| `rcvPlayerColors[5]` | In-class all `7`; owned array | Same options parser via `stoi`; main | Same options consumer | Retained across disconnect; each element replaced on next complete options push. |
| `rcvNoCompress[5]` | In-class all `false`; owned array | Same options parser; main | Same options consumer | Retained across disconnect; each element replaced on next options push. |
| `rcvAimGuide[5]` | In-class all `false`; owned array | Same options parser; main | Same options consumer | Retained across disconnect; each element replaced on next options push. |
| `rcvMouseEnabled` | In-class `false` | Same options parser; main | Same options consumer | Retained across disconnect; replaced on next options push. |
| `rcvClearMode` | In-class `false` | Same options parser; main | Same options consumer | Retained across disconnect; replaced on next options push. |
| `rcvDisableMalus` | In-class `false` | Same options parser; main | Same options consumer | Retained across disconnect; replaced on next options push. |
| `rcvTeamMode` | In-class `false` | Same options parser; main | Same options consumer | Retained across disconnect; replaced on next options push. |
| `rcvPlayerTeams[5]` | In-class `{1,2,3,4,5}`; owned array | Same options parser via `stoi`; main | Same options consumer | Retained across disconnect; each element replaced on next options push. |
| `rcvTeamCount` | In-class `2` | Same options parser via clamped `stoi`; main | Same options consumer | Retained across disconnect; replaced on next options push. |
| `pendingCreate` | In-class `false` | WASM create sets true; `OK`, retry exhaustion, or disconnect sets false; browser main | response handler | Native build never activates it. WASM disconnect resets only this gate; associated payload state remains but is gated. Unrecognized errors can leave it true (BUG-015). |
| `pendingCreateOrigNick` | Default-empty owned string | WASM create records it; browser main | retry/success response logic | Replaced by next create; not cleared on completion/disconnect, but gated by `pendingCreate`; destroyed with singleton. |
| `pendingCreateNick` | Default-empty owned string | WASM create/retry updates it; browser main | retry/success response logic | Same retained-but-gated lifecycle. |
| `pendingCreateSuffix` | In-class `2` | WASM create resets and retry increments; browser main | retry response logic | Not reset by disconnect/completion; next create resets it to `2`. |
| `pendingCreateMaxPlayers` | In-class `5` | WASM create stores requested maximum; browser main | retry command construction and room creation | Retained after completion/disconnect; next create replaces it. |
| `pendingJoin` | In-class `false` | WASM join sets true; `OK`, `NO_SUCH_GAME`, retry exhaustion, or disconnect sets false; browser main | response handler | Native build never activates it. Payloads remain gated. Other unrecognized errors can leave it true (BUG-015). |
| `pendingJoinCreator` | Default-empty owned string | WASM join records creator; browser main | retry command and room construction | Replaced by next join; retained but gated after completion/disconnect. |
| `pendingJoinOrigNick` | Default-empty owned string | WASM join records it; browser main | retry/success response logic | Replaced by next join; retained but gated after completion/disconnect. |
| `pendingJoinNick` | Default-empty owned string | WASM join/retry updates it; browser main | retry/success response logic | Replaced by next join; retained but gated after completion/disconnect. |
| `pendingJoinSuffix` | In-class `2` | WASM join resets and retry increments; browser main | retry response logic | Not reset by disconnect/completion; next join resets it to `2`. |

### Value-model field lifecycle

| Value field | Initialization and writers | Readers, aliases, replacement, and teardown |
|---|---|---|
| `NetworkPlayer::nick` | Default-empty string; list/game parsers and room construction assign it | Read by menu, mapping, and room logic. Objects are stored by value; vector replacement/destruction owns cleanup. |
| `NetworkPlayer::geoloc` | Default-empty string; player/list parsing assigns it when supplied | Read by menu/rendering. Same value ownership; no independent alias retained. |
| `NetworkPlayer::ready` | No in-class initializer; every reviewed construction assigns it before insertion/use, and ready messages mutate room entries | Read by lobby/game readiness UI. Value copies are replaced/destroyed with their containing vectors. |
| `GameRoom::creator` | Default-empty string; list/create/join paths assign it | Read by menu and leader checks. `gameList` owns value instances; `currentGame` is the separate leaking heap instance described above. |
| `GameRoom::players` | Default-empty owned vector; list and room events populate/mutate it | Read by lobby and game setup. Recursively freed for value rooms; freed only if the raw `currentGame` allocation remains reachable and is deleted, which never occurs. |
| `GameRoom::started` | No in-class initializer; every reviewed construction/parser path assigns it before use | Read by room/list UI and transitions. Same enclosing-room lifetime. |
| `GameRoom::maxPlayers` | In-class `5`; create/list/parser paths overwrite it | Read by create/retry and room UI. Same enclosing-room lifetime. |
| `ChatMessage::nick` | Default-empty string; every append path supplies it | Read through copied chat history; value vector owns replacement/destruction. |
| `ChatMessage::message` | Default-empty string; every append path supplies it | Read through copied chat history; value vector owns replacement/destruction. |
| `ChatMessage::timestamp` | No in-class initializer; every reviewed construction assigns it before insertion | Read by chat display; value vector owns lifetime. |
| `ServerInfo::host` | Default-empty string; every discovery construction fills it | Used by discovery/menu/latency logic. Results are local/value vectors, not singleton instance state. |
| `ServerInfo::name` | Default-empty string; every discovery construction fills or deliberately leaves it empty for host/port fallback | Used by server-list display. Results are local/value vectors, not singleton instance state. |
| `ServerInfo::port` | No in-class initializer; every reviewed construction assigns it before use | Used by connection/latency calls; value lifetime only. |
| `ServerInfo::latencyMs` | In-class `-1`; discovery/measurement replaces it | Read by server-list UI; value lifetime only. |

### Teardown and reconnect path matrix

| Path | Explicit reset/release | State intentionally or accidentally retained |
|---|---|---|
| Native `Disconnect` | Closes `sockfd`; sets `DISCONNECTED`; nulls `currentGame`; clears `gameList`, `messageQueue`, and `syncQueue` | Leaks the room; retains `recvBufferLen`/bytes, `openPlayers`, chat, identity/ID/map/error, option notification/values, and async payload fields. Pending booleans are not explicitly reset. |
| WASM `Disconnect` | Closes/deletes browser socket, deletes/nulls handle aliases; sets `DISCONNECTED`; nulls `currentGame`; clears create/join pending booleans, `gameList`, and `messageQueue` | Leaks the room; retains `syncQueue`, `openPlayers`, chat, identity/ID/map/error, options, and pending-operation payload/suffix fields. |
| Successful `PartGame` | Sets `IN_LOBBY` and nulls `currentGame` | Leaks the room and retains queues, lists, chat, identity/maps/options, native partial input, and async payloads. |
| `ROOM_CLOSED` push | Adds status, sets `IN_LOBBY`, nulls `currentGame` | Same room leak and broad session retention; chat additionally records the closure. |
| Destructor / `Dispose` | Destructor invokes platform `Disconnect`; standard strings/containers then destruct; `Dispose` nulls `ptrInstance` | A `currentGame` lost on any earlier nulling path remains leaked. Ordinary process flow never calls `Dispose`. |
| Reconnect | Reuses the singleton and replaces the transport/state through `Connect` | No full session initializer runs. Native partial input and WASM sync data can become active in the next session; retained identities/maps/options can remain observable until later protocol replacement (BUG-013). |

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
- The `p` receive arm is a defensive no-op keepalive handler; normal server
  priority relaying suppresses that short ping form, so the audit does not claim
  a routine peer-visible `p` exchange. The `t` arm displays capped in-game chat
  and plays its sound, but neither the retained bot tests nor a live smoke
  exercised it. Lobby options do not use an in-game opcode: the host sends
  `SETOPTIONS`, the server pushes `OPTIONS:`, the shared lobby parser fills the
  `rcv*` fields and `pendingOptions`, and the menu consumes them through
  `GetAndClearPendingOptions`.
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

The statically reviewed ordinary `f`/`s`, malus, finish, stats, ready, leave,
target, and `t` display paths are internally consistent when every sender
follows the protocol once and transport order is preserved. Only the bot helper
and failure tests were dynamically exercised; they do not prove each opcode or
the normal lobby-options exchange. Receiver-side ready/stats deduplication and
leader-only sync validation are absent; under hostile/repeated traffic those
gaps join the SEC-003/SEC-004 boundary rather than establishing a separate
ordinary-flow bug.

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
- **A Task 4 fix-round link/schema validator's failing command was not
  preserved verbatim.** The fix-round record captured the failing run's exit
  status (1) and its material output (`AssertionError:
  (PosixPath('docs/audit/FILE_COVERAGE.md'),
  'subsystems/02-network-client-sync.md#transport-thread-and-ownership-map')`)
  after a matrix heading rename left a stale anchor, but not the exact
  invocation text of that first, failing run — only the corrected re-run's
  command was recorded verbatim. The stale anchor was fixed before this gate
  closed, so there is no longer a live target to reproduce the original
  failure against without reintroducing the stale link. Task 11 re-ran today's
  equivalent link/schema check (the same inline Python walk over
  `FINDINGS.md`, `FILE_COVERAGE.md`, `SDL3_REVIEW_STATUS.md`, and this
  notebook) and confirmed it still exits 0 with `notebook_schema_and_links=PASS`,
  so the underlying defect this validator caught has no live recurrence; the
  gap is confined to that one historical command's verbatim text, not to any
  unresolved link.

## Gate conclusion

Task 4 is complete with SEC-003, BUG-013 through BUG-017, and the native-client
extension of REL-003 confirmed; BUG-012 is dismissed and reserved. Every Task
4-local candidate has a final disposition, required tests pass, production
code/configuration remain unchanged,
and the runtime/WASM exclusions above are explicit. Task 5 may consume the
validated network-message and round-state boundary.
