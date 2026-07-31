# Remediation status

Tracks which findings from [FINDINGS.md](FINDINGS.md) are fixed and which remain.
Generated from the ledger cross-referenced against commit messages since
`v2.4.27`, so a finding counts as fixed only if a commit cites its ID.

**As of `bfed3c98`.** Last release: `v2.4.32` (all five platforms green).
Commits after that tag are on `main` and unreleased.

## Position

| | Total | Fixed | Open |
|---|---|---|---|
| High | 15 | 15 | 0 |
| Medium | 45 | 21 | 24 |
| Low | 13 | 9 | 4 |
| **Defects** | **73** | **45** | **28** |
| Improvements | 24 | 0 | 24 |

All High-severity findings are fixed. One caveat: **BUG-007 is only half done** —
the WebSocket framing side is fixed, the output-queue/deadline side is not, and
it needs a design rather than a patch.

## Open defects

**Medium (24)** — BUG-001, 002, 004, 006, 011, 013, 014, 015, 017, 018, 019,
021, 022, 023, 024, 025, 027, 037, 040, 041, 042, 045, 046, 048

**Low (4)** — BUG-010, 038, 039, 047

## Suggested next order

### 1. Linux sanitizer CI job — do this first

Not a finding; a prerequisite. **Apple's ASan has no leak detection**, so
BUG-001, BUG-041 and BUG-042 cannot be verified on macOS at all. Any leak fix
written before this lands ships unproven.

`server/` already configures standalone without SDL3
(`cmake -S server -B build-asan -DCMAKE_C_FLAGS=-fsanitize=address,undefined`),
which makes a server-side sanitizer job cheap. The client side needs SDL3 built,
which is the expensive part — the Linux job already does it.

The audit makes the same argument in §9 under IMP-016, and notes IMP-021 must
run on Linux for exactly this reason.

### 2. Lifetime/ownership cluster — BUG-001, 041, 042, 045

One root cause: `TextureEx`/`TTFText` ownership and unchecked asset loads.
BUG-045 is the tractable entry point (`TTFText`'s copy constructor copies
nothing and its copy assignment is a no-op, so `HighscoreData` stored by value
loses its texture). BUG-041 and BUG-042 are the leaks and depend on step 1.

This is refcount and lifetime surgery across the render path — worth its own
session with full context.

### 3. Round and match state — BUG-018, 019, 021, 022, 023, 024, 025

Self-contained but only observable by playing: win conditions, draw resolution,
victories limits, chain-target parity. Expect to verify by actually running
matches rather than by harness.

### 4. Protocol and lobby — BUG-002, 004, 006, 011, 013, 014, 015, 017, 037, 040

Server-side, and the protocol harness works — see the client shape in
`tools/server_tests/test_room_caps.py`. Two gotchas that cost time:
the server pushes `SERVER_READY` on connect (drain it before reading a reply),
and it has a multi-second grace period before acting on a dropped socket, so use
an explicit `PART` when testing departure paths.

### 5. Platform — BUG-046, 048

Android asset extraction and WASM persistence (settings and highscores are
written to MEMFS and lost on reload). Both need their platform to verify.

### 6. Lows, then the 24 improvements

## Things to know before continuing

- **Verification gaps are tracked** in
  [../MANUAL_TEST_CHECKLIST.md](../MANUAL_TEST_CHECKLIST.md). Fixes whose code
  path was never executed are listed there rather than being implied to be
  verified.
- **BUG-036 changed the controller scancode stride** from 20 to 26, which
  invalidates saved bindings for players 2–5. Flagged for the maintainer; not
  yet confirmed as acceptable.
- **CI now runs `ctest`**, which it never did before. Compilation used to be the
  entire gate, so every regression test sat unrun.
- **Don't trust the ledger's own prose blindly.** Several findings were written
  before later corrections; some describe files that have since been deleted
  (REL-006) and at least one described a state that no longer existed. Verify
  each against current source before fixing.
