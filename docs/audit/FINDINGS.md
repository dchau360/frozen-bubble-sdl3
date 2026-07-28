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
| IMP-001 | confirmed | Medium benefit / Low effort / Low risk | High | Task 3A | Make the six no-argument server functions use strict C prototypes in declarations and definitions | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| IMP-002 | confirmed | Medium benefit / Low effort / Low risk | High | Task 3A | Make seven server length/index comparisons type-consistent; their reviewed bounds do not establish a defect | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| IMP-003 | confirmed | Low benefit / Low effort / Low risk | High | Task 3A | Mark or remove six intentionally unused GLib callback/signal parameters | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| IMP-004 | confirmed | Medium benefit / Low effort / Low risk | High | Task 3A | Remove dead `today`; restore intended state use for `was_playing` while fixing BUG-005 | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| BUG-001 | suspected | Medium | High | Task 2; resolve in Task 7 | `TextureEx` dereferences failed surface loads before checking and leaks six temporary rectangles during menu setup | [Render baseline notebook](subsystems/05-render-audio.md#candidates) |
| BUG-002 | confirmed | Medium | High | Task 3A; stress in Task 3B | The installed SIGTERM handler calls logging, allocation, DNS/HTTP, socket, and exit routines outside the POSIX async-signal-safe set | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| SEC-001 | confirmed | High | High | Task 3A; fault-inject in Task 3B | The daemon ignores `setgid`/`setuid` failures and can continue with unintended privileges | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| SEC-002 | confirmed | High | High | Task 3A; adversarial response in Task 3B | Untrusted plaintext master-response `Content-Length` reaches signed overflow and allocation sizes, allowing startup service termination; exact analyzer OOB wording is not relied upon | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| SEC-003 | suspected | High | High | Task 2; resolve in Task 4 | Peer-controlled bubble coordinates and numeric fields can reach fixed board indexing without range validation | [Network baseline notebook](subsystems/02-network-client-sync.md#candidates) |
| REL-001 | confirmed | Low | High | Task 3A | Server OOM diagnostics pass `size_t` to the signed `%zd` conversion | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| REL-002 | confirmed | Medium | High | Task 3A; owned reruns in Task 3B/9 | Both room-cap harnesses daemonize onto fixed ports, can test unrelated listeners, and tear down only the launcher; the tools harness also binds UDP 1511 | [Harness boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| IMP-005 | suspected | High benefit / Medium effort / Medium risk | Medium | Task 2; resolve in Tasks 4-7 | Default-initialize state-bearing C++ members or prove construction-before-use invariants | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-006 | suspected | Medium benefit / High effort / Medium risk | Medium | Task 2; resolve in Tasks 5 and 7 | Make gameplay/render numeric conversions and integer-division intent explicit | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-007 | suspected | Medium benefit / Low effort / Low risk | High | Task 2; resolve in Task 7 | Replace `TTFText`'s silent no-op copy assignment with explicit ownership semantics | [Render baseline notebook](subsystems/05-render-audio.md#candidates) |
| IMP-008 | suspected | Low benefit / High effort / Low risk | High | Task 2; resolve by assigned subsystem | Apply constness, API, cast, shadowing, and portability cleanup selectively | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-009 | suspected | Medium benefit / Medium effort / Low risk | High | Task 2; resolve by assigned subsystem | Simplify redundant branches, dead stores, and missing-default control flow after semantic review | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-010 | investigating | Medium benefit / Medium effort / Medium risk | High | Server side proven in Task 3A; finish in Task 7 | Handle raw allocation and asset-load failure consistently instead of dereferencing or continuing | [Server ownership review](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) |
| BUG-003 | confirmed | High | High | Task 3A; survival scenarios in Task 3B | Playing-room kick and post-start whole-room closure can leave live fds in priority mode with no game; their next binary line reaches fatal `exit(EXIT_FAILURE)` | [Lifecycle proof](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| BUG-004 | confirmed | Medium | High | Task 3A; load-check in Task 3B | The documented/configurable upload admission limit is inert because `amount_transmitted` is initialized/read/reset but never incremented | [Static review](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| BUG-005 | confirmed | Medium | High | Task 3A; stats scenario in Task 3B | A sole survivor receives a win when the playing room reaches one player and another win when that final player later leaves | [Lifecycle proof](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| BUG-006 | confirmed | Medium | High | Task 3A; fragmentation matrix in Task 3B | WebSocket upgrade assumes one `recv()` contains the full HTTP header; a legal fragmented header is consumed and then misclassified as plain TCP | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| BUG-007 | suspected | High | Medium | Task 3B | Direct blocking/single-send output has no queue or full-write handling and may stall the event loop or corrupt a short WebSocket frame | [Task 3B candidate](subsystems/01-server-protocol.md#candidates) |
| BUG-008 | confirmed | Low | High | Task 3A; allocation-count scenario in Task 3B | Creator-led whole-room closure frees the game but leaks every surviving seat's independently owned room nickname | [Ownership proof](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) |
| BUG-009 | confirmed | Low | High | Task 3A; protocol matrix in Task 3B | Empty nickname validation succeeds, allowing `CREATE ` to allocate an empty room that known LIST parsers cannot enumerate | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| SEC-004 | confirmed | High | High | Task 3A; identity scenario in Task 3B/4 | The server does not bind chat or binary sender identity to the connection's assigned room seat, allowing player/leader impersonation and hostile downstream fields | [Authorization review](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| SEC-005 | confirmed | High | High | Task 3A; UDP sanitizer scenario in Task 3B | A 128-byte LAN probe overwrites every zero in `msg[128]`, after which `strstr()` can read beyond the stack buffer | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| SEC-006 | confirmed | Medium | High | Task 3A; numeric sanitizer scenario in Task 3B | Arbitrarily long peer digit runs execute unchecked signed overflow in `charstar_to_int()` before protocol/cap validation | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| REL-003 | suspected | Medium | Medium | Task 3B/9 | Windows `SOCKET` is narrowed to `int`, rejected above 255, and used as a dense array index; Winsock `fd_set` capacity also conflicts with the documented 255-user cap | [Platform boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| REL-004 | confirmed | Low | High | Task 3A; packaging ownership in Task 9 | Server CMake defines version 2.2.1, runtime logging hard-codes 2.4.9, and the audited production tag is v2.4.27 | [Build boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |

Task 2 alone confirmed no defect. Task 3A promoted only findings with a complete
source-level causal proof and retained timing, backpressure, and platform
questions for Task 3B. Raw analyzer diagnostics were not bulk-promoted.
