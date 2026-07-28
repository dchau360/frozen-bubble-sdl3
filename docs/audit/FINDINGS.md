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
| SEC-003 | suspected | High | High | Task 2; resolve in Task 4 | Peer-controlled bubble coordinates and numeric fields can reach fixed board indexing without range validation | [Network baseline notebook](subsystems/02-network-client-sync.md#candidates) |
| REL-001 | confirmed | Low | High | Task 3 | Server OOM diagnostics pass `size_t` to the signed `%zd` conversion | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| REL-002 | confirmed | Medium | High | Task 3; remediation in Task 9 | Both room-cap harnesses daemonize onto fixed ports, can test unrelated listeners, and tear down only the launcher; the tools harness also binds UDP 1511 | [Harness boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| IMP-005 | suspected | High benefit / Medium effort / Medium risk | Medium | Task 2; resolve in Tasks 4-7 | Default-initialize state-bearing C++ members or prove construction-before-use invariants | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-006 | suspected | Medium benefit / High effort / Medium risk | Medium | Task 2; resolve in Tasks 5 and 7 | Make gameplay/render numeric conversions and integer-division intent explicit | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-007 | suspected | Medium benefit / Low effort / Low risk | High | Task 2; resolve in Task 7 | Replace `TTFText`'s silent no-op copy assignment with explicit ownership semantics | [Render baseline notebook](subsystems/05-render-audio.md#candidates) |
| IMP-008 | suspected | Low benefit / High effort / Low risk | High | Task 2; resolve by assigned subsystem | Apply constness, API, cast, shadowing, and portability cleanup selectively | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-009 | suspected | Medium benefit / Medium effort / Low risk | High | Task 2; resolve by assigned subsystem | Simplify redundant branches, dead stores, and missing-default control flow after semantic review | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
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
| REL-003 | confirmed | Medium | High | Task 3; remediation/build boundary in Task 9 | The Windows-built server narrows `SOCKET` to `int`, dense-indexes fixed arrays, and retains default Winsock `fd_set` capacity despite the documented 255-user model | [Final candidate disposition](subsystems/01-server-protocol.md#candidates) |
| REL-004 | confirmed | Low | High | Task 3; packaging ownership in Task 9 | Server CMake defines version 2.2.1, runtime logging hard-codes 2.4.9, and the audited production tag is v2.4.27 | [Build boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| BUG-010 | confirmed | Low | High | Task 3 | A same-name claimant can terminate a still-connected open peer once its cached whole-second receive-activity delta reaches two | [Final candidate disposition](subsystems/01-server-protocol.md#candidates) |
| BUG-011 | confirmed | Medium | High | Task 3 | Repeated `START` resets readiness while peers remain priority; renewed acknowledgement removes already-priority fds from the replacement polling list | [Final candidate disposition](subsystems/01-server-protocol.md#candidates) |
| IMP-011 | confirmed | Medium benefit / Medium effort / Low risk | High | Task 3 | Replace the synchronous 50 ms accept-time WebSocket sniff with event-loop upgrade state | [Final candidate disposition](subsystems/01-server-protocol.md#candidates) |

Task 2 alone confirmed no defect. Task 3 promoted only findings with complete
source-level causal proof. Runtime/security reproduction was not performed by
user direction, so these are code-supported inferences rather than observed
runtime facts. Raw analyzer diagnostics were not bulk-promoted.
