# Task 3 Report: Pre-main IDBFS hydration and serialized WASM flushes (BUG-048)

## Status

Implemented and verified Task 3 only. No Task 4 C++ save call sites, CI, docs,
plan, or SDD ledger files were changed.

Commit: `5de4e1e7` (`fix(wasm): hydrate and flush IDBFS persistence (BUG-048)`).

## Implementation

- Added a UMD persistence controller in `web/persistence.js` with the exact
  `createFrozenBubblePersistence(Module, FS, IDBFS, runtime)` factory.
- Mounted IDBFS at exactly `/libsdl/frozen-bubble`, held the exact
  `frozen-bubble-idbfs` run dependency through populate, and exposed the
  required browser APIs and hydration-before-runtime ordering witness.
- Serialized dirty flush requests so `FS.syncfs(false, ...)` calls cannot
  overlap, coalesced writes during a flush into one follow-up, and made a later
  request retryable after a flush error.
- Added six deterministic `node:test` state-machine tests with a callback-driven
  fake filesystem. The tests assert that populate uses only `true` and flushes
  use only `false`.
- Linked `persistence.js` and IDBFS into Emscripten and exported `FS` alongside
  the existing runtime methods.
- Made `tools/serve-wasm.py --port 0` print the actual OS-assigned port.
- Added a dependency-free Node/CDP browser driver. It creates its own Chrome
  profile, writes literal UTF-8 fixtures through `Module.FS`, awaits the public
  flush/idle APIs, reloads the same tab and origin, and compares exact bytes
  after hydration. All direct child processes are terminated in `finally`, and
  only the driver-created profile is removed.

## Strict TDD evidence

### RED

Command:

```text
node --test tests/wasm_persistence_test.cjs
```

Result: exit 1, before `web/persistence.js` was created.

```text
Error: Cannot find module '../web/persistence.js'
Require stack:
- .../tests/wasm_persistence_test.cjs
...
pass 0
fail 1
```

This was the expected module-missing failure specified by the brief.

### GREEN

Command:

```text
node --test tests/wasm_persistence_test.cjs
```

Fresh result after implementation and self-review:

```text
pass 6
fail 0
cancelled 0
skipped 0
todo 0
```

All required named cases passed:

1. hydrate holds the run dependency until populate succeeds
2. hydrate error releases startup and reports failure
3. flush requested before hydrate waits for populate
4. rapid flush requests never overlap syncfs
5. dirty write during flush schedules exactly one follow-up
6. flush error settles waiters and later requests can retry

## Emscripten SDK patch, configure, and build

Installed toolchain: Homebrew Emscripten `6.0.4-git`.

The documented SDL3 port procedure was applied to the resolved Homebrew SDK
root at `/opt/homebrew/Cellar/emscripten/6.0.4/libexec`:

- copied `tools/ports/sdl3_mixer.py` and `tools/ports/sdl3_image.py` into the
  SDK `tools/ports` directory;
- registered `SDL3_IMAGE_FORMATS` and `SDL3_MIXER_FORMATS` in `settings.js` and
  `settings.py`;
- suppressed the SDL3 experimental warning in the SDK port file.

The Codex login environment put Xcode Python 3.9 before Homebrew, while this
Emscripten release requires Python 3.10+. Prepending Homebrew Python 3.14 fixed
the launcher without changing repository code:

```text
export PATH=/opt/homebrew/opt/python@3.14/bin:$PATH
```

Configure command and result:

```text
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
```

```text
-- Configuring done
-- Generating done
-- Build files have been written to: .../build-wasm
```

Build command and result:

```text
cmake --build build-wasm --parallel
```

```text
[100%] Linking CXX executable frozen-bubble-sdl3.html
[100%] Built target frozen-bubble-sdl3
```

The generated link command contains `-lidbfs.js`, the exact `--pre-js` input,
and `FS` in `EXPORTED_RUNTIME_METHODS`. Compilation emitted existing C++ warning
classes (unused values and Emscripten macro extension warnings) but exited 0.

## Real browser reload evidence

Command:

```text
node tools/test-wasm-persistence.mjs build-wasm
```

Fresh result:

```text
PASS: IDBFS hydrated before runtime and persisted all three fixtures across reload
```

The driver used Google Chrome at the known macOS path, a dynamic same-origin
server port, a fresh temporary profile, and the page DevTools WebSocket. It
confirmed `Module.persistentStorageReadyBeforeRuntime === true` before and after
reload and exact bytes for `settings.ini`, `highscores`, and
`highlevelshistory`.

## Additional verification

```text
node --check tools/test-wasm-persistence.mjs                         PASS
python3 -m py_compile tools/serve-wasm.py                            PASS
ctest --test-dir build --output-on-failure                           0 failures
PATH=/opt/homebrew/opt/python@3.14/bin:$PATH cmake --build build-wasm --parallel
                                                                    PASS
git diff --check                                                     PASS
```

Native CTest ran all 11 registered entries: nine passed and the two documented
sanitizer-only server tests were skipped.

## Files changed

- `CMakeLists.txt`
- `web/persistence.js`
- `tests/wasm_persistence_test.cjs`
- `tools/test-wasm-persistence.mjs`
- `tools/serve-wasm.py`

## Self-review

- Re-read the Task 3 brief and checked every required public name, literal mount
  path, run-dependency name, fixture, sync mode, and file boundary.
- Confirmed the controller source matches the supplied minimal state machine
  and has no Task 4 C++ flush call sites.
- Confirmed the browser test uses only Node built-ins plus the Node global
  `WebSocket`, waits for a real reload load event, and drains child output pipes
  to avoid subprocess backpressure.
- Confirmed the expected history is an explicit ten-row literal and byte
  comparisons are independent of production serialization.
- Confirmed cleanup targets only the `mkdtemp` profile owned by this test.
- Confirmed the worktree diff contains only the five requested Task 3 files;
  the report itself is intentionally ignored by `.superpowers/sdd/.gitignore`.

## Concerns

- The local Homebrew Emscripten SDK patch is an external prerequisite and may
  need to be reapplied after a Homebrew Emscripten upgrade.
- The current Codex login environment requires the Homebrew Python `PATH`
  prefix for Emscripten 6.0.4. This is environment-specific and not a repository
  or public-contract change.
- No Emscripten or Chrome behavior contradicted the Task 3 public contract.

## Fix Round 1: child-process launch errors and cleanup

Commit: `03fdccdb` (`fix(wasm): handle persistence test launch failures`).

### Finding addressed

The browser driver spawned the Python server and Chrome without awaiting their
`ChildProcess` launch outcomes. `waitForMatch()` observed only pipe events, and
`stopChild()` waited only for `exit`, so a launch error could escape the normal
promise/finally flow or leave cleanup waiting on an event that is not guaranteed
after spawn failure.

### Focused TDD regression

Added `tests/wasm_persistence_driver_test.cjs`. It starts the driver with:

- an isolated `TMPDIR`;
- `PATH` containing no `python3`;
- an executable `CHROME_BIN` placeholder so discovery succeeds and the Python
  launch is the controlled failure.

The test requires a nonzero driver exit, the exact `spawn python3 ENOENT` error
to reach the driver's awaited `main().catch(...)` path, no unhandled-error
diagnostic, and no leftover profile directory.

#### RED

Command:

```text
node --test tests/wasm_persistence_driver_test.cjs
```

Exact result before the fix:

```text
✖ launch failure is handled and removes the temporary browser profile (51.933417ms)
ℹ tests 1
ℹ suites 0
ℹ pass 0
ℹ fail 1
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 95.0485

✖ failing tests:

test at tests/wasm_persistence_driver_test.cjs:24:1
✖ launch failure is handled and removes the temporary browser profile (51.933417ms)
  AssertionError [ERR_ASSERTION]: The input did not match the regular expression /spawn python3 ENOENT/. Input:

  'Error: Process output ended before WASM server URL. Output:\n' +
    '\n' +
    '    at Socket.onEnd (.../tools/test-wasm-persistence.mjs:83:19)\n' +
    '    at Socket.emit (node:events:521:24)\n' +
    '    at endReadableNT (node:internal/streams/readable:1753:12)\n' +
    '    at process.processTicksAndRejections (node:internal/process/task_queues:90:21)\n'
```

On the installed Node 26 runtime, the failed child's pipe closed before an
unhandled-error crash surfaced, so the pre-fix driver reported the unrelated
pipe-end error instead of the launch error. The controlled RED still proves the
underlying defect: the child launch error was not routed into the awaited
control flow. The same regression also checks the cleanup guarantee directly.

#### GREEN

Command:

```text
node --test tests/wasm_persistence_driver_test.cjs
```

Exact fresh result:

```text
✔ launch failure is handled and removes the temporary browser profile (43.116834ms)
ℹ tests 1
ℹ suites 0
ℹ pass 1
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 74.729083
```

### Implementation

- Added `waitForSpawn(child)`, which subscribes to the child's `spawn` and
  `error` events and is awaited immediately after both server and Chrome
  `spawn()` calls. Pre-spawn errors now reject inside the existing `try`, so the
  existing `finally` always owns cleanup.
- Updated `stopChild()` to return immediately for an absent PID and to settle
  cleanup on `error` or `close`, retaining the five-second SIGKILL fallback.
- Did not change the deferred WebSocket event-waiter behavior.

### Required regression verification

Controller suite:

```text
$ node --test tests/wasm_persistence_test.cjs
✔ hydrate holds the run dependency until populate succeeds
✔ hydrate error releases startup and reports failure
✔ flush requested before hydrate waits for populate
✔ rapid flush requests never overlap syncfs
✔ dirty write during flush schedules exactly one follow-up
✔ flush error settles waiters and later requests can retry
ℹ pass 6
ℹ fail 0
```

Real browser reload:

```text
$ node tools/test-wasm-persistence.mjs build-wasm
PASS: IDBFS hydrated before runtime and persisted all three fixtures across reload
```

Additional checks:

```text
node --check tools/test-wasm-persistence.mjs   PASS
git diff --check                              PASS
```

### Fix Round 1 self-review and concerns

- Both spawn sites await the same launch guard.
- The child is assigned before awaiting launch, so `finally` can always inspect
  it.
- Spawn-error children with no PID cannot deadlock `stopChild()`.
- Normal children settle cleanup on `close`; cleanup-time child errors also
  settle rather than becoming unhandled.
- The regression deletes only its own isolated temporary root in its own
  `finally`.
- No new product/runtime dependencies were added.
- Concern: Node event ordering differs by runtime; the regression therefore
  asserts the public error-routing and cleanup outcomes rather than depending
  on an implementation-specific unhandled-error stack format.
