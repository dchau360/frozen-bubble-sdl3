# 01 — Server and Protocol Audit Notebook

## Scope

Task 3: C server, TCP/WebSocket framing, untrusted protocol input, room
lifecycle, and associated tests/integration boundaries. The static phase read
every project-owned path under `server/`, plus `tests/server_list_cap_test.py`
and `tools/server_tests/test_room_caps.py`. The closing phase reconciled every
server-local candidate from those complete code paths. Runtime and adversarial
validation was not performed by user direction, so this gate is closed on
static evidence with that omission retained as a final-audit limitation.

## Trust boundaries and invariants

- A successfully accepted peer is admitted only when its integer fd is at most
  255 and the configured connection/one-game LAN limits allow it
  (`net.c:574-589`). All fd-indexed server arrays have 256 elements. Accepted
  fds are the only values stored in `conns`, `conns_prio`, `open_players`, and
  `game.players_conn[]`; this proves the POSIX array indices for admitted peers.
  It does not prove the Windows boundary, where `SOCKET` is pointer-sized and
  is incorrectly narrowed to `int` and treated as a dense fd (REL-003).
- Each admitted fd owns two 16,384-byte heap buffers: decoded/incomplete line
  bytes in `incoming_data_buffers[fd]` and raw incomplete WebSocket frame bytes
  in `ws_raw_frame_buf[fd]`. `conn_terminated()` owns their release and resets
  the WebSocket flag/raw pointer/raw length. The decoded pointer/count are not
  nulled, but the fd is removed from the live lists before reuse, and acceptance
  replaces both values before re-enlisting the fd.
- A TCP/game-protocol line has at most 16,383 received bytes including LF.
  A no-LF buffer that reaches 16,383 is rejected. The stack receive buffer has
  one additional byte for the C terminator. Embedded NUL before LF is rejected
  because `strchr()` cannot reach the LF.
- For a WebSocket peer, decoded line bytes and raw partial-frame bytes are kept
  separately. On each pass, `offset + ws_prefix` is no greater than the prior
  16,383-byte raw/decoded total; the remaining `recv()` capacity is therefore
  non-negative. A 16-bit WebSocket payload larger than this storage can never
  complete: after the retained partial fills the capacity, the next zero-length
  receive is treated as peer shutdown. No buffer overflow follows from that
  path, although the frame-size contract is implicit and poorly diagnosed.
- A room owns one heap `struct game`, from one through twenty fd values, one
  independently allocated nickname per seated player, one printable seat id
  per player, and a monotonically advanced next-id cursor. The room cap is
  initialized to 5 or accepted only in `[2,20]`; `add_player()` checks
  `players_number < max_players` before every fixed-array write. The printable
  id window `['A','z']` has 58 values, and the live-id scan makes collision or
  framing-byte reuse impossible with at most twenty seats.
- `nick[fd]` (connection/chat identity) and `game.players_nick[]` (room identity)
  are distinct allocations, not aliases. A normal part frees the removed room
  nickname; disconnect separately frees the connection nickname, geolocation,
  and IP. The distinction is not enforced as an identity invariant: an
  in-game `NICK` can change only `nick[fd]`, and binary relay trusts the first
  peer-supplied byte rather than the seat id assigned to that fd (SEC-004).
- The server has no output queue. Lobby replies/pushes and all WebSocket writes
  are single blocking `send()` calls; native in-game relays are single
  nonblocking sends and disconnect a destination on any short result. WebSocket
  relay maps every nonnegative `ws_send()` result back to the requested payload
  length, hiding a short frame. These complete paths confirm BUG-007 without a
  runtime backpressure claim.

## Static review

### Acceptance, input retention, dispatch, and teardown

| Stage | Static path and state transition | Ownership / guard |
|---|---|---|
| Startup | `main()` initializes Winsock/stats, parses and binds in `create_server()`, optionally daemonizes, then enters `connections_manager()`. | `atexit(stats_cleanup)` owns stats teardown. The listener is process-owned; `close_server()` closes only TCP/UDP listeners. |
| Accept | `select()` readiness -> `accept()` -> fd/cap/LAN rejection -> blacklist/rate checks -> keepalive -> per-fd initialization -> WebSocket sniff -> greeting -> normal list -> `open_players`. | `fd > 255` is closed before any fd-array access except the guarded full-message send. Admission then allocates two 16 KiB buffers. |
| TCP retention | Prior decoded partial line is copied to a 16 KiB stack buffer, followed by nonblocking `recv()`. One non-priority line is dispatched at a time; remaining complete/partial bytes return to the per-fd buffer and schedule another pass. | `len <= 16383`; exact no-LF saturation closes. LF-terminated data receives `buf[len] = '\0'`. |
| WebSocket retention | Raw retained bytes are appended after decoded bytes; only the raw suffix is decoded in place. Complete payload is appended to decoded bytes; a raw partial suffix is copied back to the separate WebSocket buffer. | 7-bit/16-bit lengths become `int`; 64-bit length and close frames are fatal. Full-frame check precedes mask and payload access. |
| Command dispatch | Non-priority LF lines enter `process_msg()`. Priority lines normally relay through `process_msg_prio()`; lines beginning `FB/` re-enter text-command dispatch so `PART` and other lobby commands remain reachable in-game. | Protocol parsing is permissive (`charstar_to_int`, fixed offsets) and numeric overflow is unguarded (SEC-006). Empty nicknames pass validation (BUG-009). |
| Room entry | `CREATE` allocates a room and removes creator from `open_players`; `JOIN` locates only an open room, validates the cap/protocol, appends a seat, and removes the joiner from `open_players`. | At most 16 open rooms; cap at most 20; duplicate room nicknames are checked across every seated player. |
| Start / priority | Creator-only `START` emits the seat map and marks `PLAYING`; each `OK_GAME_START` marks that fd started and moves it from `conns` to `conns_prio`. | `players_started[]` is initialized before first use. A repeated `START` resets it while fds remain priority; repeated acknowledgements call `add_prio()` on already-priority fds and remove them from the replacement polling list (BUG-011). Post-start `CLOSE` is the BUG-003 closure/orphan chain. |
| Normal part | Player is shifted out, room nickname freed, leave relayed, fd appended to `open_players`; the `PART` dispatcher then calls `remove_prio(fd)`. | Correct for the requesting fd. Kicked peers and peers released by whole-room closure do not pass through this `remove_prio()` call (BUG-003). |
| Disconnect | `conn_terminated()` closes the fd, resets/frees per-fd network and identity state, removes it from the iteration copy, calls room part then open-player removal, and flags list recalculation. | The `new_conns` copy prevents recursive double teardown during failed-send cascades. OS process exit owns any remaining process allocations. |

### Network-derived length and index trace

| Input / derived value | Destination(s) | Guard or open candidate |
|---|---|---|
| Accepted fd | Every `[256]` network/identity array; game fd slots; `fd_set` | POSIX admission closes values above 255. Windows `SOCKET` narrowing/dense indexing remains REL-003. |
| TCP `recv()` result (`ssize_t`) | `buf + offset + ws_prefix`, line terminator, retained decoded buffer | Capacity is `16383-offset-ws_prefix`; retained-total invariant bounds it. `-1/EAGAIN`, EOF, exact no-LF saturation, and embedded NUL all have close/retain paths. |
| WebSocket header bytes | `plen`, `hdr_len`, mask pointer, payload pointer, in-place `memmove()` | Minimum-header and full-frame checks precede reads; `plen <= 65535`, but practical retained frame limit is 16383. RSV/FIN/opcode/masking rules are not fully validated. |
| HTTP upgrade bytes | `req[4096]`, key length, SHA-1 input, response | Receive is bounded and key is limited to 64, but one `recv()` is incorrectly assumed to contain the full header; missing key after a partial read consumes bytes and downgrades to TCP (BUG-006). The synchronous 50 ms accept-time sniff is a confirmed scalability improvement (IMP-011), not a separately proven availability defect. |
| UDP datagram length | `msg[128]`, then `strstr(msg, ...)` | Buffer is pre-zeroed, but a 128-byte datagram overwrites every terminator; subsequent unbounded C-string search can read past the stack array (SEC-005). |
| Protocol major/minor and `CREATE` cap digit runs | Signed `int` accumulator in `charstar_to_int()` | No digit-count/range/overflow guard; arbitrary peer digit runs execute signed overflow before later equality/range checks (SEC-006). |
| Nickname | `nick[fd]`, game nickname allocations, list/map/chat formatting | First 10 bytes are retained and character-checked, but empty is accepted. Connection identity and room identity can diverge; room arrays remain bounded. |
| Geolocation | `geoloc[fd]`, LIST strings | Truncated to 13 bytes but otherwise unvalidated; fixed `strconcat()` destinations prevent overwrite. Delimiter injection remains a protocol-hardening improvement. |
| Options/TALK | 8,192-byte `vasprintf_()` scratch, 1,000-byte talk scratch, 16,384-byte send scratch | Formatting truncates rather than overwrites. Long options are silently truncated to 8,191 bytes before push. TALK is capped by `snprintf()`. |
| Room cap/player count | Three fixed 20-element arrays plus nickname array | Default 5; parsed cap accepted only `[2,20]`; add checks current count before index/write. Legacy clients are rejected from caps above 5. |
| Seat id | `players_id[20]`, first byte of map/leave/self-sync | Allocator wraps in a 58-byte printable window and scans live seats. Incoming relay does not validate/rewrite the claimed first byte (SEC-004). |
| HTTP `Content-Length` and body counts | `size`, `bufsize`, `dlsize`, allocation/reallocation sizes and terminators | Decimal parse and `size + 1` use signed `int` without overflow checks. Malformed values can invoke UB or convert negative sizes to huge `size_t`, and `malloc_()` terminates the process. The no-length growth path caps near 1 GiB. SEC-002 is confirmed for overflow/service termination; the analyzer's exact OOB-write wording is not independently adopted. |
| Outbound formatted length | 16,384-byte line buffer, 16,400-byte WS frame, socket send length | `snprintf()` is capped; normal positive protocol lengths fit the WS frame. There is no retry/queue for a legal short write; blocking calls can wait without a server-side deadline, and WebSocket relay treats a positive short frame as complete (BUG-007). |

### Authorization and room lifecycle

| Operation | Authorized source / rule | Result |
|---|---|---|
| `NICK` | Any connection, including priority mode; collision scan covers only `open_players`. | Once the cached whole-second activity difference reaches two, another peer claiming the same nickname terminates the still-connected open fd; socket liveness is not checked (BUG-010). In-game identity divergence remains SEC-004. |
| `CREATE` / `JOIN` | Any fd not already in a game; name unique across all current room seats. | Creates/joins only open rooms and enforces cap/legacy guard. Established `NICK` need not match requested seat nickname. |
| `CLOSE`, `START`, `KICK`, `SETOPTIONS` | Slot-zero creator only. | Creator authority checks are present. Status-transition checks are incomplete: `CLOSE` and `START` remain accepted after play begins. |
| `LEADER_CHECK_GAME_START` | Any seated player, despite the name. | Read-only readiness response; no mutation. This is an authority-hardening issue, not a confirmed defect by itself. |
| `OK_GAME_START` | The seated fd marks only its own slot. | First start moves it to priority. Repeated creator `START` resets the epoch without demotion; the next acknowledgement calls `add_prio()` again, appends to the old priority list, removes the fd from `new_conns`, and leaves `prio[fd]` set after the old list is replaced (BUG-011). |
| In-game relay | Any priority fd in a room. | Payload is relayed unchanged; the server never binds byte zero to `players_id[find_player_number(g,fd)]`, so peers can impersonate any seat/leader and inject downstream fields (SEC-004). |
| `ADMIN_REREAD` | Peer address string exactly `127.0.0.1`. | Other peers receive `DENIED`; the kernel-supplied IPv4 address is the authorization source. |
| Creator leaves pre-play | Whole room is removed; survivors receive `ROOM_CLOSED` and return to `open_players`. | Correct authority intent matches the C++ client boundary, but survivor room nicknames are lost without free (BUG-008). If creator first changes a playing room to `CLOSED`, survivors are still priority and BUG-003 applies. |
| Player leaves during play | Leave id is relayed; leaver gets a loss; sole survivor gets a win. | Room remains `PLAYING` with one player. When that player later leaves, the empty-room branch awards the same player another win (BUG-005). |
| Kick during play | Creator calls `player_part_game_()` for target. | Target is removed from the room but never from priority mode; its `KICKED` push is itself dropped because pushes to priority fds are suppressed. Its next binary line finds no game and calls `exit(EXIT_FAILURE)` (BUG-003). |

### Allocation owners and destruction paths

- `game` is owned by the `games` list and freed when empty or when the creator
  closes a pre-play room. Each `players_nick[]` entry is independently owned by
  the game seat and normally freed on removal. Whole-room creator departure
  frees only the creator's saved nickname and the structure, leaking every
  surviving seat nickname (BUG-008).
- `nick[fd]`, `geoloc[fd]`, and `IP[fd]` are connection-owned and freed by
  `conn_terminated()`. Room nicknames are copies, so this is not a double-free.
- `games`, `open_players`, `conns`, and `conns_prio` own GLib list nodes only;
  their data are integer fds or separately owned game structures.
- WebSocket state is a static fd-indexed flag plus the two network-owned buffers.
  `ws_reset()` runs on normal connection teardown.
- The stats hash owns a duplicated key and `PlayerStats` structure, but its
  value destructor is only `g_free`: the nested `PlayerStats.nick` allocation
  leaks at cleanup/replacement. This is bounded by distinct recorded names and
  is retained as part of IMP-010 rather than a separate defect.
- `logging_init()` passes an allocated ident string to `openlog()` and loses the
  pointer. Because `openlog()` retention semantics are platform-dependent, the
  safe ownership should be made explicit under IMP-010.

### Build, operations, and harness boundary

- `server/CMakeLists.txt` builds all seven C units with GLib and Windows
  `ws2_32`. Its public executable definition reports `VERSION="2.2.1"`, while
  `net.c` logs `v2.4.9` and the audited production tag is `v2.4.27` (REL-004).
- `server/win32_compat.h` maps socket close/error/poll/syslog and stubs daemon
  user switching. It does not supply a portable socket-handle abstraction;
  the Windows-built server assigns pointer-sized Winsock `SOCKET` results to
  `int`, compares the narrowed value to 255, uses it as an array index, and
  relies on the default Winsock `fd_set` capacity while advertising 255 users.
  That source/build mismatch confirms REL-003; Windows execution was not
  available. Stats persistence also formats `time_t` with `%ld`, which is not
  portable to 64-bit Windows `time_t`.
- The README accurately describes the bridge and 255-player/resource-limit
  intent; the promised built-in upload limiter is contradicted by BUG-004.
  Init artifacts are legacy SysV examples. Their configuration format matches
  `create_server()`, but lifecycle/quoting modernization belongs to Task 9.
- `tests/server_list_cap_test.py` uses fixed TCP port 15512, launches without
  `-d`, accepts any listener, and tears down only the launcher. Its receive
  helper also stops as soon as a response prefix arrives, so payload assertions
  can be transport-fragmentation flaky.
- `tools/server_tests/test_room_caps.py` has the same stronger lifecycle flaw:
  fixed TCP port 15113, no foreground `-d`, teardown of only the launcher, and
  unnecessary `-l` binding UDP 1511. Its claim that `finally` cannot leak a
  server is false on POSIX. Both harnesses are covered by REL-002.

### Task 2 candidate dispositions

| ID | Task 3A disposition | Static basis / remaining dependency |
|---|---|---|
| IMP-001 | Confirmed improvement | Six declarations/definitions use non-prototype `()`; runtime behavior is not implicated, but strict C declarations remove call-check ambiguity and the warning family. |
| IMP-002 | Confirmed improvement; defect aspect dismissed | All seven comparisons were traced. Their operands are bounded before memory access; `strconcat()` is called only with nonzero fixed-array sizes. Type cleanup remains worthwhile, but no boundary failure follows from these sites. |
| IMP-003 | Confirmed improvement | All six parameters are callback/signal signature obligations and intentionally unused; annotate/remove names rather than change behavior. |
| IMP-004 | Confirmed improvement | `stats.c:103` is dead. `game.c:1008` is dead but adjacent winner logic is independently defective as BUG-005, so restoration of its intended state guard is preferable to blind removal. |
| BUG-002 | Confirmed Medium | Installed SIGTERM handler directly calls logging, listener close, DNS/HTTP registration removal, allocation, and `exit()`; these are outside the POSIX async-signal-safe set. Runtime signal stress was omitted by user direction; confirmation is the complete static call chain, not observed undefined behavior. |
| SEC-001 | Confirmed High | After a successful lookup, both `setgid()` and `setuid()` return values are discarded and daemon startup continues. Failed uid drop can leave root; failed gid drop can retain unintended groups. Linux fault injection was omitted by user direction. |
| SEC-002 | Confirmed High, narrowed | Untrusted plaintext HTTP `Content-Length` reaches overflow-prone signed parsing, `size + 1`, and allocation. Startup blacklist download runs this in the server process, so malformed size can terminate service. The static review does not adopt the analyzer's three exact OOB-write claims, and hostile response testing was omitted by user direction. |
| REL-001 | Confirmed Low | `%zd` requires the signed counterpart of `size_t`, but `malloc_()`/`realloc_()` pass unsigned `size_t`; the variadic type mismatch is undefined and `%zu` is the correct format. |
| REL-002 | Confirmed Medium | Source proves daemon/fixed-port/foreign-listener ownership failures in both Python harnesses; Task 2 already reproduced the registered test's false ownership and one leaked daemon. Harness repair and durable regression ownership transfer to Task 9. |
| IMP-010 | Confirmed improvement (cross-owner disposition completed in Task 7) | Server-side raw allocation failure is inconsistent: `vasprintf_()` and stats-directory setup use unchecked `strdup()`, while wrapper/GLib policy exits the whole server. Task 7 closed the remaining owner — render's unchecked `precalc_cos`/`precalc_sin`, the `plasma.raw` `exit(1)`, warn-then-crash `TextureEx` (defect side is BUG-001), warn-and-continue texture loads, silent-null text, and audio's permanent negative caching — see [Task 7's ownership table](05-render-audio.md#static-review). IMP-010 is confirmed in `FINDINGS.md`, not open. |

## Dynamic evidence

Task 3 created no runtime evidence. Accepted dynamic evidence remains only the
earlier Task 2 baseline:

- Release built successfully; warnings-strict stopped on IMP-001 through
  IMP-004; the retained ASan+UBSan build completed.
- Four unaffected registered tests passed in final Release and sanitizer runs.
- The registered server-list result is not ownership evidence because of
  REL-002. Task 2's in-memory foreground/dynamic-port replay did assert the
  exact Release and sanitizer children alive and passed unchanged assertions.

Three whole-task runtime-agent dispatches and one split runtime dispatch were
rejected by the automated classifier before producing runtime evidence. The
user then explicitly skipped security work. No Task 3 server process, port,
harness, protocol client, socket traffic, signal scenario, or fault injection
was created, and existing foreign listeners remained untouched. Consequently,
the intended TCP/WebSocket/UDP boundary matrix, server-survival cases,
backpressure measurement, identity capture, stats/allocation checks, fake-master
responses, and privilege/signal fault injection are omitted—not passes.

## Candidates

No server-local candidate remains open:

| Candidate | Final disposition | Static basis / downstream owner |
|---|---|---|
| BUG-007 — blocking/one-shot output | Confirmed High | Blocking sockets have no output deadline or queue, and WebSocket relay converts every nonnegative short frame into apparent full-payload success. Runtime frequency was not measured. |
| REL-003 — Windows socket/index model | Confirmed Medium | The Windows-built target narrows `SOCKET` to `int`, dense-indexes fixed arrays, and retains the default Winsock `fd_set` capacity despite a 255-user default. Task 9 owns remediation/build portability. |
| Nickname-eviction timing | Confirmed as BUG-010 Low | `conn_recently_active()` is only a cached receive-time comparison; once its whole-second delta reaches two, a same-name claimant calls `conn_terminated()` on a still-connected open peer. |
| Repeated `START` | Confirmed as BUG-011 Medium | `real_start_game()` resets `players_started[]` while fds remain priority; renewed `OK_GAME_START` calls `add_prio()` and removes an already-priority fd from the replacement polling list. |
| Post-start `CLOSE` | Confirmed under BUG-003 | `CLOSE` changes a playing room to `CLOSED`; creator `PART` then takes the whole-room branch without demoting survivors, leaving priority fds with no game and a fatal next binary dispatch. |
| Per-accept 50 ms WebSocket sniff | Confirmed as IMP-011 | The synchronous accept path waits up to 50 ms before admitting every native connection. Static evidence supports event-loop integration as an improvement, but no measured service-impact defect. |
| IMP-010 raw allocation policy | Transferred | Server inconsistency is proven; Task 7 owns the cross-owner asset/allocation disposition. |
| SEC-004 downstream parser impact | Transferred | Server identity trust is confirmed; Task 4 owns client-side opcode/index impact without duplicating the ID. |
| REL-002 and REL-004 operations boundaries | Transferred | Server-local causes are confirmed; Task 9 owns harness repair, Windows/build portability, and version/package reconciliation. |

## Confirmed findings

- BUG-002 — async-unsafe SIGTERM handler.
- BUG-003 — room-removal paths orphan priority fds and permit a peer-triggered
  whole-server exit.
- BUG-004 — `-t` transmission limiter is inert because its counter is never
  incremented.
- BUG-005 — persistent stats double-credit the sole survivor.
- BUG-006 — fragmented WebSocket upgrades are consumed then misclassified.
- BUG-007 — blocking single-send output can stall the single event loop, and a
  positive short WebSocket frame is falsely reported as a complete relay.
- BUG-008 — creator-led whole-room closure leaks surviving room nicknames.
- BUG-009 — empty room nicknames pass validation and create an unlistable room.
- BUG-010 — a same-name claimant can evict a still-connected lobby peer once
  its cached receive activity delta reaches two seconds.
- BUG-011 — repeated `START` and acknowledgement can remove already-priority
  fds from the replacement polling list while leaving `prio[fd]` set.
- SEC-001 — daemon continues after unchecked privilege-drop calls.
- SEC-002 — untrusted master response size can overflow and terminate service.
- SEC-004 — chat and binary sender identity are not bound to the connection's
  assigned room seat.
- SEC-005 — a full-size UDP probe can drive `strstr()` beyond `msg[128]`.
- SEC-006 — peer digit runs can overflow `charstar_to_int()` before validation.
- REL-001 — OOM diagnostics have a variadic signedness mismatch.
- REL-002 — server harnesses do not own or clean up the daemon they test.
- REL-003 — the Windows server narrows Winsock handles and cannot safely honor
  its documented/default connection model with the current dense arrays and
  default `fd_set` capacity.
- REL-004 — server build/banner/log versions disagree.
- IMP-011 — move the synchronous 50 ms WebSocket sniff out of the serial accept
  path and recognize complete upgrades through event-loop state.

## Dismissed candidates

- Clang's 30 `DeprecatedOrUnsafeBufferHandling` warnings largely recommend
  Annex K `_s` replacements for ordinary `fprintf`/bounded `sscanf` calls.
  They do not identify a concrete unbounded destination on this platform.
  Actual bounds defects are tracked as SEC-002 and SEC-005.
- Two `suspicious-string-compare` reports are intentional C truth-value tests;
  fourteen assignment-in-condition reports are explicit parse/lookup idioms.
- IMP-002's seven signed/unsigned sites do not independently cross a memory or
  protocol boundary under their callers' proven fixed-size guards. They remain
  a confirmed cleanup improvement, not a defect.
- The separate decoded/WebSocket-raw retention design preserves
  `offset + ws_prefix <= 16383`; no static overflow was found in the partial
  frame merge. Oversized 16-bit frames disconnect rather than overwrite.
- Permissive RSV/FIN/opcode/masking acceptance and geolocation delimiter input
  were dismissed as separate defects: the source establishes noncanonical
  protocol acceptance but no additional memory, authority, or lifecycle failure
  beyond the confirmed findings. Standards hardening remains future work.

## Coverage

- Static-complete server implementation/header review: `fb-server.c`,
  `game.c/.h`, `net.c/.h`, `ws.c/.h`, `tools.c/.h`, `stats.c/.h`, `log.c/.h`,
  and `win32_compat.h`.
- Static-complete project-facing boundary review: `server/AUTHORS`,
  `server/CMakeLists.txt`, `server/README`, `server/init/README`,
  `server/init/fb-server`, `server/init/fb-server.conf`,
  `tests/server_list_cap_test.py`, and
  `tools/server_tests/test_room_caps.py`.
- The detailed per-path disposition is recorded in
  [FILE_COVERAGE.md](../FILE_COVERAGE.md). All 20 server files and both harnesses
  have final Task 3 dispositions. Build, platform, client, and allocation
  boundaries are explicitly transferred to Tasks 9, 4, and 7 respectively.

## Limitations

- No socket, server process, port, harness, DNS, HTTP, signal, daemon, protocol
  client, or other network/runtime action was executed in Task 3.
- POSIX source invariants were proven against the audited tree. Windows socket
  execution, Linux signal/privilege behavior, actual WebSocket/TCP backpressure,
  and hostile boundary traffic were not observed. Security-specific runtime
  checks are omitted from the remaining audit by user direction.
- The Perl client reference was used only to resolve leader/room intent. It
  confirms the creator closes before initial `START`, the leader owns level
  synchronization, and clients interpret the first in-game byte as player
  identity; it does not excuse server-side trust of hostile peers.
- Apple ASan cannot perform leak detection in the retained sanitizer build, so
  no Task 3 leak-runtime claim is made for BUG-008.
- All newly closed dispositions are code-supported inferences. The audit does
  not claim runtime reproduction, sanitizer confirmation, exploitability
  frequency, Windows execution, or server-survival passes for them.

## Gate conclusion

Closed — every server-local candidate has a final confirmed or dismissed
disposition, or an explicit cross-subsystem owner. Static causal evidence—not
Task 3 runtime observation—supports the confirmed findings. The omitted
security/runtime matrix is a final-audit limitation by user direction. Exact
next action: begin Task 4, auditing native/WASM clients and multiplayer
synchronization while consuming SEC-004's server trust boundary and omitting
security-specific runtime checks.
