# 01 — Server and Protocol Audit Notebook

## Scope

Task 3: C server, TCP/WebSocket framing, untrusted protocol input, room lifecycle, and associated tests/integration boundaries.

## Trust boundaries and invariants

Pending Task 3 investigation.

## Static review

Not started.

## Dynamic evidence

Task 2 baseline evidence only; Task 3 semantic and adversarial investigation has
not started.

- `cmake -S . -B build-audit-release -G Ninja -DCMAKE_BUILD_TYPE=Release`
  configured successfully with AppleClang 21.0.0.
- `cmake --build build-audit-release --parallel` built the game, server, and
  test targets successfully but emitted 51 warnings from 27 unique
  project-owned server locations. Header diagnostics repeat in each translation
  unit; the unique locations are enumerated under the candidates below.
- `ctest --test-dir build-audit-release --output-on-failure` reported all five
  registered tests passing in 1.85 seconds. REL-002 later proved that the
  server-list test connected to a pre-existing listener rather than establishing
  ownership of the requested built server, so only the other four results are
  accepted directly from this CTest run.
- The warnings-strict Debug configure succeeded, but its parallel build exited
  1 after AppleClang promoted IMP-001 through IMP-004 diagnostics to errors.
  The failing jobs exposed 18 unique locations before Ninja stopped; the
  successful Release build above remains the complete 27-location inventory.
- The required strict-tree `ctest` invocation then exited 8. `net-bots-test`
  passed six assertions. CTest labeled the server-list Python command Passed,
  but unittest explicitly reported `OK (skipped=1)` because the failed build had
  not linked its server binary. Three C++ test executables were not run. The
  skipped/missing executables are direct consequences of the classified build
  failure, not additional pass evidence or independent defects.
- A fresh verification reproduced REL-002 when parallel Release/sanitizer CTest
  runs shared fixed port 15512: sanitizer CTest received `CREATE: NICK_IN_USE`.
  An already-running server from another checkout had owned that port since
  before Task 2. Directly launching the audit binary on 15512 exited 1 with
  `Address already in use`, while the test's independent port probe accepted
  the foreign listener.
- A preliminary alternate-port/foreground replay passed, but independent review
  correctly rejected its binary-ownership claim because it still hardcoded port
  25512 and checked closure only after execution.
- The accepted replay did not edit tracked code: it executed the test source in
  memory, dynamically asked loopback for an ephemeral port, added foreground
  `-d`, and asserted `self.server.poll() is None` after readiness. The unchanged
  assertions passed against Release on port 63305 (0.607 seconds) and ASan+UBSan
  on port 63316 (1.000 second). No matching foreground child remained after
  either run. Thus server behavior has an owned-binary baseline, while harness
  reliability remains a candidate.

## Candidates

- **IMP-001 — strict no-argument C prototypes (suspected improvement):**
  `server/net.h:47,52-54`, `server/tools.h:40-41`,
  `server/net.c:476,715,816,1330`, and `server/tools.c:180,214` account for 12
  unique `-Wstrict-prototypes` locations covering six functions. The header
  locations repeat across translation units. `connections_manager` already has
  a `(void)` definition, so this is classified as a warning-cleanliness and
  future-compiler-portability improvement, not a demonstrated runtime defect.
- **IMP-002 — signed/unsigned comparisons (suspected improvement):**
  `server/tools.c:119`, `server/game.c:331,617`, and
  `server/net.c:134,461,918,1054` are the seven unique `-Wsign-compare`
  locations. Most are bounded string lengths or syscall result comparisons;
  `strconcat` also deserves explicit zero-size/boundary review in Task 3.
  No failing boundary behavior is established by this baseline alone.
- **IMP-003 — unused callback/signal parameters (suspected improvement):**
  `server/tools.c:173`, `server/game.c:139,152,924`, and
  `server/net.c:772,996` are six unique `-Wunused-parameter` locations whose
  signatures are constrained by GLib callbacks or the signal-handler API.
  Classified as intentional warning noise requiring explicit annotation or
  removal, not third-party/environment noise.
- **IMP-004 — dead locals (suspected improvement):** `server/game.c:1008`
  (`was_playing`) and `server/stats.c:103` (`today`) are two
  `-Wunused-variable` locations. Nearby logic independently reads the same
  state, so the baseline supports dead-code cleanup or restoration-of-intent
  review, not a confirmed behavior defect.
- **SEC-001 — unchecked privilege drop (suspected High):** clang-tidy reports
  ignored `setgid` and `setuid` results at `server/tools.c:283-284`. If either
  call fails after a root launch with `-u`, the daemon continues without
  verifying the intended identity. Task 3 must reproduce failure paths and
  confirm platform impact.
- **SEC-002 — master-response size trust (suspected High):**
  `server/net.c:1243-1286` parses attacker-influenced HTTP `Content-Length`
  through an overflow-prone `int` accumulator, uses `size + 1` as `bufsize`,
  and writes through offsets derived from received byte counts. Clang's taint
  analyzer reports three potential out-of-bounds writes at lines 1262, 1271,
  and 1280. Task 3 owns an adversarial response harness and confirmation.
- **BUG-002 — async-unsafe termination handler (suspected Medium):** SIGTERM is
  installed at `server/net.c:1100`; `sigterm_catcher` calls logging,
  `close_server`, `unregister_server`, and `exit`. Clang-tidy traces 16
  non-async-signal-safe operations through that call graph, including
  allocation, DNS, networking, and libc state. Task 3 must test termination
  under active server work.
- **REL-001 — OOM format mismatch (suspected Low):** `server/tools.c:81,91`
  use signed `%zd` for `size_t` arguments. Cppcheck identifies both exact
  varargs mismatches; use `%zu` or a correctly typed cast after Task 3 review.
- **REL-002 — fixed-port daemonized regression harness (suspected Medium):**
  `tests/server_list_cap_test.py` launches `fb-server` without `-d`, so POSIX
  `daemonize()` forks and the Python `Popen` object no longer owns the listening
  child. Teardown kills/waits only for the launcher. Setup also never checks
  `self.server.poll()` and accepts any responder on hardcoded port 15512. This
  can leak a server, false-pass against stale/foreign code, prevent sanitizer
  ownership, and collide under concurrent CTest. Task 3/9 must make the server
  foreground and allocate/verify an isolated port.

## Confirmed findings

None.

## Dismissed candidates

- Clang's 30 `DeprecatedOrUnsafeBufferHandling` warnings largely recommend
  Annex K `_s` replacements for ordinary `fprintf`/bounded `sscanf` calls.
  They do not identify a concrete unbounded destination on this platform and
  are dismissed as an over-broad macOS checker family; actual bounds reports
  are retained separately as SEC-002 and SEC-003.
- Two `suspicious-string-compare` reports are intentional C truth-value tests:
  nonzero `strcmp`/`strncmp` means mismatch. Fourteen assignment-in-condition
  reports are likewise explicit parse/lookup idioms. Neither family supplies
  defect evidence without a failing path.

## Coverage

Pending; see [FILE_COVERAGE.md](../FILE_COVERAGE.md).

## Limitations

- The Release baseline executes unit/integration tests but does not exercise
  interactive server operation, hostile protocol input, or every warning site's
  boundary behavior. Those remain Task 3 work.
- The original CTest server-list rows are retained as observed results, but
  REL-002 prevents treating them as evidence about the requested binary. The
  dynamic-port, foreground, live-child-verified reruns supply the accepted Task
  2 behavior baseline.

## Gate conclusion

Open — Task 3 has not started.
