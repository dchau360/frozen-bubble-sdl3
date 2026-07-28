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
| IMP-001 | suspected | Medium benefit / Low effort / Low risk | High | Task 2; resolve in Task 3 | Make the six no-argument server functions use strict C prototypes in declarations and definitions | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| IMP-002 | suspected | Medium benefit / Low effort / Low risk | Medium | Task 2; resolve in Task 3 | Make seven server length/index comparisons type-consistent and verify their boundary behavior | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| IMP-003 | suspected | Low benefit / Low effort / Low risk | High | Task 2; resolve in Task 3 | Mark or remove six intentionally unused GLib callback/signal parameters | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| IMP-004 | suspected | Low benefit / Low effort / Low risk | High | Task 2; resolve in Task 3 | Remove or restore intended use of two dead server locals (`was_playing`, `today`) | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| BUG-001 | suspected | Medium | High | Task 2; resolve in Task 7 | `TextureEx` dereferences failed surface loads before checking and leaks six temporary rectangles during menu setup | [Render baseline notebook](subsystems/05-render-audio.md#candidates) |
| BUG-002 | suspected | Medium | High | Task 2; resolve in Task 3 | The server SIGTERM handler reaches logging, allocation, DNS, socket, and exit routines that are not async-signal-safe | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| SEC-001 | suspected | High | High | Task 2; resolve in Task 3 | The daemon ignores `setgid`/`setuid` failures and can continue with unintended privileges | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| SEC-002 | suspected | High | Medium | Task 2; resolve in Task 3 | The server trusts an unchecked, overflowing HTTP `Content-Length` when sizing and indexing its master-server response buffer | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| SEC-003 | suspected | High | High | Task 2; resolve in Task 4 | Peer-controlled bubble coordinates and numeric fields can reach fixed board indexing without range validation | [Network baseline notebook](subsystems/02-network-client-sync.md#candidates) |
| REL-001 | suspected | Low | High | Task 2; resolve in Tasks 3 and 9 | Server OOM diagnostics pass `size_t` to the signed `%zd` conversion | [Server baseline notebook](subsystems/01-server-protocol.md#candidates) |
| REL-002 | suspected | Medium | High | Task 2; resolve in Tasks 3 and 9 | The server-list regression daemonizes onto a fixed port, can false-pass against an unrelated listener, and leaves the actual server running | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#candidates) |
| IMP-005 | suspected | High benefit / Medium effort / Medium risk | Medium | Task 2; resolve in Tasks 4-7 | Default-initialize state-bearing C++ members or prove construction-before-use invariants | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-006 | suspected | Medium benefit / High effort / Medium risk | Medium | Task 2; resolve in Tasks 5 and 7 | Make gameplay/render numeric conversions and integer-division intent explicit | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-007 | suspected | Medium benefit / Low effort / Low risk | High | Task 2; resolve in Task 7 | Replace `TTFText`'s silent no-op copy assignment with explicit ownership semantics | [Render baseline notebook](subsystems/05-render-audio.md#candidates) |
| IMP-008 | suspected | Low benefit / High effort / Low risk | High | Task 2; resolve by assigned subsystem | Apply constness, API, cast, shadowing, and portability cleanup selectively | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-009 | suspected | Medium benefit / Medium effort / Low risk | High | Task 2; resolve by assigned subsystem | Simplify redundant branches, dead stores, and missing-default control flow after semantic review | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |
| IMP-010 | suspected | Medium benefit / Medium effort / Medium risk | High | Task 2; resolve in Tasks 3 and 7 | Handle raw allocation and asset-load failure consistently instead of dereferencing or continuing | [Build/tooling baseline notebook](subsystems/07-build-release-tooling.md#analyzer-triage) |

No defect is confirmed by Task 2 alone. Analyzer-originated candidates remain
`suspected` until their assigned subsystem gate reproduces and semantically
challenges them; raw diagnostics were not bulk-promoted.
