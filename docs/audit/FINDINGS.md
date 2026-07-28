# SDL3 Audit Finding Registry

This registry assigns stable, never-recycled IDs. A suspicion is not a confirmed defect: candidates move from `suspected` to `investigating`, then to either `confirmed` or `dismissed`. Dismissed candidates remain documented in the relevant subsystem notebook.

## Stable ID classes

- `BUG-###`: functional or reliability defect.
- `SEC-###`: security or untrusted-input defect.
- `REL-###`: build, packaging, release, deployment, or portability defect.
- `IMP-###`: suggested improvement, kept separate from defects.

## Severity and prioritization

Defect severities follow the approved design:

- **Critical:** remotely exploitable, major security-boundary failure, or widespread unrecoverable corruption.
- **High:** crash, memory corruption, serious multiplayer desynchronization, or a shipped platform rendered unusable.
- **Medium:** incorrect gameplay, broken edge case, practically significant resource leak, or meaningful portability/release defect.
- **Low:** limited incorrect behavior, weak diagnostics, or minor robustness issue.

Improvements are ranked separately by expected benefit, implementation effort, and regression risk. They are not assigned defect severity without defect evidence. The `Severity/Priority` cell for an improvement records that evidence-backed priority profile.

## Registry

| ID | State | Severity/Priority | Confidence | Gate | Summary | Evidence |
|---|---|---|---|---|---|---|
| IMP-001 | confirmed | Medium benefit / Low effort / Low risk | High | Task 3 | Make the six no-argument server functions use strict C prototypes in declarations and definitions | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| IMP-002 | confirmed | Medium benefit / Low effort / Low risk | High | Task 3 | Make seven server length/index comparisons type-consistent; their reviewed bounds do not establish a defect | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| IMP-003 | confirmed | Low benefit / Low effort / Low risk | High | Task 3 | Mark or remove six intentionally unused GLib callback/signal parameters | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| IMP-004 | confirmed | Medium benefit / Low effort / Low risk | High | Task 3 | Remove dead `today`; restore intended state use for `was_playing` while fixing BUG-005 | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| BUG-001 | suspected | Medium | High | Task 2; resolve in Task 7 | `TextureEx` dereferences failed surface loads before checking and leaks six temporary rectangles during menu setup | [Render baseline notebook](subsystems/05-render-audio.md#candidates) |
| BUG-002 | confirmed | Medium | High | Task 3 | The installed SIGTERM handler calls logging, allocation, DNS/HTTP, socket, and exit routines outside the POSIX async-signal-safe set; runtime stress was omitted by user direction | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| SEC-001 | confirmed | High | High | Task 3 | The daemon ignores `setgid`/`setuid` failures and can continue with unintended privileges; fault injection was omitted by user direction | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| SEC-002 | confirmed | High | High | Task 3 | Untrusted plaintext master-response `Content-Length` reaches signed overflow and allocation sizes, allowing startup service termination; exact analyzer OOB wording and runtime reproduction are not claimed | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| SEC-003 | confirmed | High | High | Task 4 | Peer-controlled bubble coordinates and numeric fields reach unchecked board indexing and exception-throwing option parsing | [Peer-message proof](subsystems/02-network-client-sync.md#peer-messages-and-round-flow) |
| REL-001 | confirmed | Low | High | Task 3 | Server OOM diagnostics pass `size_t` to the signed `%zd` conversion | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| REL-002 | confirmed | Medium | High | Task 3; remediation in Task 9 | Both room-cap harnesses daemonize onto fixed ports, can test unrelated listeners, and tear down only the launcher; the tools harness also binds UDP 1511 | [Harness boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| IMP-005 | confirmed | High benefit / Medium effort / Medium risk | High | Tasks 2 and 5; remaining owners Tasks 6-7 | Default-initialize state-bearing C++ members; reviewed network/gameplay construction-before-use paths do not establish a defect | [Gameplay construction proof](subsystems/03-gameplay.md#reload-reset-and-construction) |
| IMP-006 | confirmed | Medium benefit / High effort / Medium risk | High | Tasks 2 and 5; remaining render owner Task 7 | Make bounded gameplay/render numeric conversions and integer-division intent explicit; the Task 5 slice has no proven precision defect | [Gameplay numeric disposition](subsystems/03-gameplay.md#reload-reset-and-construction) |
| IMP-007 | suspected | Medium benefit / Low effort / Low risk | High | Task 2; resolve in Task 7 | Replace `TTFText`'s silent no-op copy assignment with explicit ownership semantics | [Render baseline notebook](subsystems/05-render-audio.md#candidates) |
| IMP-008 | suspected | Low benefit / High effort / Low risk | High | Task 2; resolve by assigned subsystem | Apply constness, API, cast, shadowing, and portability cleanup selectively | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-009 | confirmed | Medium benefit / Medium effort / Low risk | High | Tasks 2 and 5; remaining owners later gates | Simplify redundant branches, dead stores, unused attachment helpers, and missing-default control flow after semantic review | [Gameplay analyzer disposition](subsystems/03-gameplay.md#reload-reset-and-construction) |
| IMP-010 | investigating | Medium benefit / Medium effort / Medium risk | High | Task 7 (server side proven in Task 3) | Handle raw allocation and asset-load failure consistently instead of dereferencing or continuing | [Server ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) |
| BUG-003 | confirmed | High | High | Task 3 | Playing-room kick and post-start whole-room closure can leave live fds in priority mode with no game; their next binary line reaches fatal `exit(EXIT_FAILURE)` | [Lifecycle proof](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| BUG-004 | confirmed | Medium | High | Task 3 | The documented/configurable upload admission limit is inert because `amount_transmitted` is initialized/read/reset but never incremented | [Static review](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| BUG-005 | confirmed | Medium | High | Task 3 | A sole survivor receives a win when the playing room reaches one player and another win when that final player later leaves | [Lifecycle proof](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| BUG-006 | confirmed | Medium | High | Task 3 | WebSocket upgrade assumes one `recv()` contains the full HTTP header; a legal fragmented header is consumed and then misclassified as plain TCP | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| BUG-007 | confirmed | High | High | Task 3 | Direct blocking/single-send output has no queue or deadline, and WebSocket relay reports every nonnegative short frame as a complete payload | [Final candidate disposition](subsystems/01-server-protocol.md#candidates) |
| BUG-008 | confirmed | Low | High | Task 3 | Creator-led whole-room closure frees the game but leaks every surviving seat's independently owned room nickname | [Ownership proof](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) |
| BUG-009 | confirmed | Low | High | Task 3 | Empty nickname validation succeeds, allowing `CREATE ` to allocate an empty room that known LIST parsers cannot enumerate | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| SEC-004 | confirmed | High | High | Task 3; downstream impact in Task 4 | The server does not bind chat or binary sender identity to the connection's assigned room seat, allowing player/leader impersonation and hostile downstream fields | [Authorization review](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| SEC-005 | confirmed | High | High | Task 3 | A 128-byte LAN probe overwrites every zero in `msg[128]`, after which `strstr()` can read beyond the stack buffer | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| SEC-006 | confirmed | Medium | High | Task 3 | Arbitrarily long peer digit runs execute unchecked signed overflow in `charstar_to_int()` before protocol/cap validation | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| REL-003 | confirmed | High | High | Tasks 3-4; remediation/build boundary in Task 9 | Windows server/client code narrows `SOCKET`; the server also exceeds default `fd_set` capacity, while the client never enables nonblocking mode before per-frame receive | [Client extension](subsystems/02-network-client-sync.md#lobby-response-and-reachability-handling) |
| REL-004 | confirmed | Low | High | Task 3; packaging ownership in Task 9 | Server CMake defines version 2.2.1, runtime logging hard-codes 2.4.9, and the audited production tag is v2.4.27 | [Build boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| BUG-010 | confirmed | Low | High | Task 3 | A same-name claimant can terminate a still-connected open peer once its cached whole-second receive-activity delta reaches two | [Final candidate disposition](subsystems/01-server-protocol.md#candidates) |
| BUG-011 | confirmed | Medium | High | Task 3 | Repeated `START` resets readiness while peers remain priority; renewed acknowledgement removes already-priority fds from the replacement polling list | [Final candidate disposition](subsystems/01-server-protocol.md#candidates) |
| IMP-011 | confirmed | Medium benefit / Medium effort / Low risk | High | Task 3 | Replace the synchronous 50 ms accept-time WebSocket sniff with event-loop upgrade state | [Final candidate disposition](subsystems/01-server-protocol.md#candidates) |
| BUG-012 | dismissed | - | High | Task 4 | Priority ingress diverts `FB/` text to the command parser, whose `PART` branch removes the player and priority membership; the ID remains reserved | [Dismissed candidate](subsystems/02-network-client-sync.md#dismissed-candidates) |
| BUG-013 | confirmed | Medium | High | Task 4 | Disconnect/part paths leak `currentGame` and retain native partial input or WASM sync/session state that can contaminate reconnection | [Connection lifecycle](subsystems/02-network-client-sync.md#connection-and-room-lifecycle) |
| BUG-014 | confirmed | Medium | High | Task 4 | WASM round 2+ waits for 40 entries in `messageQueue` after routing all sync traffic into `syncQueue`, forcing the five-second timeout | [Round-flow proof](subsystems/02-network-client-sync.md#peer-messages-and-round-flow) |
| BUG-015 | confirmed | Medium | High | Task 4 | Native and WASM command state machines omit ordinary rejection types and do not reliably correlate success with the pending create/join operation | [Response handling](subsystems/02-network-client-sync.md#lobby-response-and-reachability-handling) |
| BUG-016 | confirmed | Low | High | Task 4 | Native reachability treats writable nonblocking connect as success without checking `SO_ERROR`, so refused endpoints can appear online | [Response handling](subsystems/02-network-client-sync.md#lobby-response-and-reachability-handling) |
| BUG-017 | confirmed | Medium | High | Task 4 | Native TCP setup searches each greeting `recv` independently and outbound paths accept short sends as complete, violating stream semantics | [Stream-semantics proof](subsystems/02-network-client-sync.md#native-tcp-stream-semantics) |
| BUG-018 | confirmed | Medium | High | Task 5 | Multiplayer board clear ignores the Clear Mode flag and ends Classic and Team Mode rounds | [Winner-state proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-019 | confirmed | Medium | High | Task 5 | Sequential final-loss handling can credit a winner or both players before resolving a simultaneous draw | [Simultaneous-loss proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-020 | confirmed | High | High | Task 5 | Distinct-match startup retains malus and transient round state; after player-count shrink a stale malus can index an inactive cleared board | [Reset-path proof](subsystems/03-gameplay.md#reload-reset-and-construction) |
| BUG-021 | confirmed | Medium | High | Tasks 5-6 | Disconnect handling bypasses configured continuation, one-team-survivor, and victories-limit semantics | [Departure-state proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-022 | confirmed | Medium | High | Task 5 | Chain target reservation uses standard parity on flipped grids and omits the reference cross-chain validity pass | [Chain-reaction proof](subsystems/03-gameplay.md#placement-collision-grouping-and-compression) |
| BUG-023 | confirmed | Medium | High | Tasks 5-6 | The local two-player victories limit is displayed but neither propagated into gameplay nor enforced | [Local match proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-024 | confirmed | Medium | High | Task 5 | Remote clear-win accounting depends on whether `F` is queued before deferred replicated-stick resolution | [Clear-order proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |

Task 2 alone confirmed no defect. Tasks 3 through 5 promoted only findings with
complete source-level causal proof. Runtime/security reproduction was not
performed by user direction, so security findings are code-supported inferences
rather than observed runtime facts. Raw analyzer diagnostics were not
bulk-promoted. Task 4's required bot unit checks passed; its available harness
could not autonomously create, start, synchronize, and assert a two-round game.
Task 5's helper tests, pure-helper oracle, and headless production-object
boundary harness passed, but no graphical or multi-client gameplay runtime was
claimed.
