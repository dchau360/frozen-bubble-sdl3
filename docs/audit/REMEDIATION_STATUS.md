# Remediation status

Tracks which findings from [FINDINGS.md](FINDINGS.md) are fixed and which remain.
Generated from the ledger cross-referenced against commit messages since
`v2.4.27`, so a finding counts as fixed only if a commit cites its ID and the
citing commit actually resolves it (not just references it as rationale).

**As of `f616fe98`.** Last release: `v2.4.33` (all five platforms green,
itch.io deploys succeeded).

## Position

| | Total | Fixed | Open |
|---|---|---|---|
| High | 15 | 15 | 0 |
| Medium | 45 | 23 | 22 |
| Low | 13 | 9 | 4 |
| **Defects** | **73** | **47** | **26** |
| Improvements | 24 | 16 done, 1 moot | 7 |

All High-severity findings are fixed. Two caveats:

- **BUG-007 is only half done** — the WebSocket framing side is fixed, the
  output-queue/deadline side is not, and it needs a design rather than a patch.
- **IMP-015 is only partially done** — `AdsManager.init()` (Android, still
  never called), `__ANDROID_PORT__` (`gamesettings.h:106`, still defined), and
  the `bzero` macro / `<iconv.h>` include in `shaderstuff.h` (still present;
  used only by `blacken_()`, which itself has zero callers and is dead code
  the original finding didn't name) are all still there. Confirmed by direct
  grep against current source, not inherited from the original finding text.

## Fixed since the last refresh

- **BUG-045** — `TTFText` copy semantics; `aca2b403`.
- **BUG-001** — `TextureEx` lifetime/leak; `9f3fdc1f`.
- **IMP-001..005, 007, 009..015 (partial), 022, 023, 024** — the 24-item
  improvement pass, delegated in batches and independently verified:
  `95ceee2b`, `53eccc5a`, `4c3fb45c`, `0b3f1bf7`. That work introduced four
  regressions of its own (WebSocket fragmented-handshake handling, a WebSocket
  pipelined-data misroute, a plasma-transition null-deref crash, and a CREATE
  room-cap clamp that logged but didn't clamp) — all four found by direct
  empirical reproduction, not by inspection, and fixed in `f96b316c`.
- **Linux ASan/UBSan CI job** — not a finding fix, but the prerequisite
  IMP-016/IMP-021 named: `dde25951`. This is what makes BUG-041/042 provable
  going forward, since Apple's ASan has no leak detection.

## Open defects

**Medium (22)** — BUG-002, 004, 006, 011, 013, 014, 015, 017, 018, 019, 021,
022, 023, 024, 025, 027, 037, 040, 041, 042, 046, 048

**Low (4)** — BUG-010, 038, 039, 047

## Open improvements

- **IMP-006** — bounded numeric conversions/integer-division intent (High
  effort; no proven defect, pure code-quality).
- **IMP-015 (partial)** — see caveat above.
- **IMP-016..021** — test-infrastructure items (register the harnesses that
  already exist as CTest targets, protocol/parser unit tests, packaged-artifact
  smoke tests, the Linux leak-regression job). Explicitly excluded from the
  IMP delegation on request; not started.
- **IMP-008** — effectively closed, not "done": Task 9 found zero actionable
  diagnostics in-scope and promoted nothing. No code change needed.

## Suggested next order

### 1. Lifetime/ownership cluster — BUG-041, 042

Same root cause as the now-fixed BUG-001/BUG-045 (`TextureEx`/`TTFText`
ownership). The Linux sanitizer CI job now exists specifically to prove these,
so this is the first cluster where a fix can actually be verified rather than
argued from ownership tables.

### 2. Round and match state — BUG-018, 019, 021, 022, 023, 024, 025

Self-contained but only observable by playing: win conditions, draw
resolution, victories limits, chain-target parity. Expect to verify by
actually running matches rather than by harness.

### 3. Protocol and lobby — BUG-002, 004, 006, 011, 013, 014, 015, 017, 037, 040

Server-side, and the protocol harness works — see the client shape in
`tools/server_tests/test_room_caps.py`. Two gotchas that cost time:
the server pushes `SERVER_READY` on connect (drain it before reading a reply),
and it has a multi-second grace period before acting on a dropped socket, so use
an explicit `PART` when testing departure paths.

### 4. Platform — BUG-046, 048

Android asset extraction and WASM persistence (settings and highscores are
written to MEMFS and lost on reload). Both need their platform to verify.

### 5. Lows, then the remaining improvements (IMP-006, 015 leftovers, 016-021)

## Things to know before continuing

- **Verification gaps are tracked** in
  [../MANUAL_TEST_CHECKLIST.md](../MANUAL_TEST_CHECKLIST.md). Fixes whose code
  path was never executed are listed there rather than being implied to be
  verified.
- **BUG-036 changed the controller scancode stride** from 20 to 26, which
  invalidates saved bindings for players 2–5. Flagged for the maintainer; not
  yet confirmed as acceptable. Shipped in v2.4.33's changelog as a known,
  one-time rebind requirement.
- **CI now runs `ctest`** and, since `dde25951`, a full Linux ASan/UBSan build.
  Compilation used to be the entire gate.
- **Don't trust the ledger's own prose blindly.** Several findings were written
  before later corrections; some describe files that have since been deleted
  (REL-006) and at least one described a state that no longer existed (this
  refresh found a second instance: IMP-015's `bzero` claim was already
  slightly wrong in the original finding — `blacken_()`, not just the
  commented-out `utf8key_`, is the other user). Verify each against current
  source before fixing, not just against this document.
