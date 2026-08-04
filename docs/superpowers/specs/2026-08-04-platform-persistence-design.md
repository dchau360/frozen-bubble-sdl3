# Android Asset and WASM Persistence Remediation Design

**Date:** 2026-08-04

**Scope:** BUG-046 and BUG-048 from `docs/audit/FINDINGS.md`, plus the
corresponding Android/WASM slice of IMP-020.

## Goal

Make Android's extracted asset tree a complete, current, retryable deployment
of the assets packaged in the installed APK, and make browser settings and
highscores survive a same-origin page reload without racing game startup or
depending on an unreachable WASM shutdown path.

## Current behavior and root causes

### BUG-046 — Android asset extraction

Gradle packages the repository's `share/` contents at the APK asset root.
`FrozenBubbleActivity.onCreate()` synchronously calls
`AssetExtractor.extractAll()` before `SDLActivity.onCreate()` starts SDL, and
native `InitDataDir()` reads the exact returned path through JNI. That ordering
and exact-path contract are correct and must remain unchanged.

The extractor has no transactional completion model:

- `extractFile()` skips every existing destination whose length is non-zero,
  without comparing its bytes, size, version, or timestamp;
- files are written directly to their final destinations;
- recursive directory and file exceptions are logged and swallowed; and
- the current version marker is written even after a partial or failed pass.

Consequently, changed same-path assets are not refreshed on upgrade, a
non-empty truncated file survives the next extraction and then receives a
valid marker, paths removed from the APK remain in the extracted tree, and
file/directory shape changes cannot be reconciled. Clearing app data repairs
the tree, but ordinary upgrades do not.

The extracted `files/share/` subtree contains only managed packaged assets.
Settings and highscore files are siblings under `files/`, so replacing
`files/share/` is safe while replacing all of `files/` would destroy user data.

### BUG-048 — WASM preference persistence

SDL3's Emscripten implementation returns `/libsdl/frozen-bubble/` from
`SDL_GetPrefPath("", "frozen-bubble")`, but that directory lives in volatile
MEMFS. The current WASM link does not include IDBFS, and the generated page has
no persistent mount or `FS.syncfs()` call. `settings.ini`, `highscores`, and
`highlevelshistory` therefore disappear on every page load; only the nickname
survives through its separate `localStorage` implementation.

The timing is load-bearing. `FrozenBubble` reads settings during construction
and constructs `HighscoreManager` immediately afterward. Hydrating persistent
files after `main()` starts would let missing-file repair create defaults and
race or overwrite the stored data.

The WASM main loop registers a browser callback and returns. The native
destructor path, including its final settings and highscore saves, never runs.
Several settings mutations currently update only the in-memory dictionary,
and completed level history is otherwise saved only by later dialog or
destructor paths. Browser correctness therefore requires eager file writes as
well as a persistent filesystem.

## Design decisions

### 1. Android owns one replaceable managed tree

`files/share/` remains the only native asset root and the exact path returned
to C++ through `FrozenBubbleActivity.sExtractedDataDir`.

The marker format changes from a plain version number to:

`schema-2:<versionCode>`

Every legacy marker therefore mismatches once, even if an APK is installed
without changing its version code. A matching marker is accepted only when
`files/share/` is a directory. Same-version corruption after a fully successful
deployment is outside BUG-046's interrupted-extraction/update scope; future
content-integrity checking would require a packaged hash manifest.

On marker mismatch, the extractor will:

1. recursively delete only `files/share/`;
2. recreate the managed root;
3. traverse every packaged asset;
4. write each asset to a temporary sibling, close it successfully, and rename
   it to the final path;
5. propagate any traversal, directory, copy, close, or rename failure;
6. write `.assets_version.tmp` and rename it to `.assets_version` only after
   the complete tree succeeds.

There is no exists/non-empty shortcut during a rebuild. Removing the old tree
first also removes deleted APK paths and reconciles file/directory transitions.
The low-peak-storage tradeoff is intentional: a failed rebuild cannot fall
back to the previous tree, because running a new native binary against an old
asset set is not a sound compatibility mode.

An interrupted or failed pass cannot commit the new marker. The next launch
sees the absent/mismatched marker, deletes the partial tree, and retries from a
known empty state.

### 2. Android extraction fails closed before SDL

The filesystem algorithm moves into a package-private Java component with no
Android dependencies. It consumes a narrow asset-source interface supporting
directory listing and file opening; `AssetExtractor` adapts `AssetManager` and
the Android `Context` to that component.

Failures are no longer swallowed. `FrozenBubbleActivity.onCreate()` catches the
single extraction failure at the platform boundary, emits a useful Android log
entry and user-visible startup error, calls `finish()`, and returns without
calling `super.onCreate()`. SDL and C++ therefore never observe a partially
deployed tree.

The migration and deletion logic must reject targets outside the canonical
`getFilesDir()/share` subtree. No code in this remediation may recursively
delete `getFilesDir()` itself or touch settings/highscore siblings.

### 3. WASM mounts and hydrates IDBFS before `main()`

A focused pre-JavaScript module, `web/persistence.js`, owns browser filesystem
persistence. The Emscripten target links `-lidbfs.js` and loads this module with
`--pre-js`.

During `Module.preRun`, the module will:

1. create `/libsdl/frozen-bubble` if needed;
2. mount IDBFS at exactly `/libsdl/frozen-bubble`;
3. add a named Emscripten run dependency;
4. call `FS.syncfs(true, callback)` to populate MEMFS from IndexedDB;
5. mark hydration complete and remove the run dependency in both success and
   error paths.

Removing the dependency on error is mandatory: private browsing, quota
restrictions, or IndexedDB failures may lose persistence, but must not hang the
game indefinitely. Errors are surfaced through `Module.printErr`; the game
continues with defaults after the failed hydrate.

No push to IndexedDB may begin until the initial populate attempt has
finished. This prevents an empty startup filesystem from overwriting stored
data.

### 4. WASM writes use one serialized/coalescing flush owner

`web/persistence.js` exposes one promise-returning flush request and one
promise-returning idle observer. The state machine has these invariants:

- at most one `FS.syncfs(false, ...)` is in flight;
- a request during an in-flight flush sets one dirty-again flag rather than
  starting an overlapping sync;
- completion starts exactly one follow-up flush when dirty-again is set;
- callers waiting for idle settle only after there is neither an in-flight
  flush nor pending dirty work;
- an error is logged and settles waiters without permanently wedging later
  requests.

The C++ platform boundary gains one function that requests this flush under
`__WASM_PORT__` and is a no-op everywhere else. It is called only after the
corresponding `FILE` or `std::ofstream` objects have been closed.

`GameSettings::SaveSettings()` requests a flush after closing `settings.ini`.
Every persistent settings mutation, including graphics quality, fullscreen,
FPS display, and colorblind mode, must reach `SaveSettings()` eagerly instead
of relying on WASM teardown.

`HighscoreManager::SaveNewHighscores()` requests one flush after closing both
`highlevelshistory` and `highscores`. Level-history and qualified-score
mutations must reach that save path eagerly; later name entry may persist a
second update. The flush coalescer absorbs closely spaced saves without
overlapping IDBFS operations.

The nickname's existing `localStorage` behavior is unchanged. Migrating it into
IDBFS is unnecessary for these defects and would risk changing the network
menu's established startup behavior.

## Component and file responsibilities

- `android/app/src/main/java/org/frozenbubble/AssetExtractor.java`: Android
  facade, version lookup, `AssetManager` adapter, and public extraction entry.
- A new package-private Java deployment component beside `AssetExtractor`:
  managed-root rebuild, safe recursive deletion, atomic per-file copy, marker
  commit, and propagated result/error.
- `android/app/src/main/java/org/frozenbubble/FrozenBubbleActivity.java`:
  fail-closed startup boundary before `SDLActivity.onCreate()`.
- `android/app/src/test/java/org/frozenbubble/`: host JVM regression fixtures
  using temporary directories and an in-memory/fake asset source.
- `android/app/build.gradle`: JUnit dependency and unit-test configuration.
- `web/persistence.js`: IDBFS mount/hydration and the complete flush state
  machine; no game-specific settings parsing.
- `src/platform.h` and `src/platform.cpp`: the cross-platform flush request;
  native builds receive a no-op implementation.
- `src/gamesettings.cpp` and `src/highscoremanager.cpp`: eager file saves at
  existing persistence ownership boundaries.
- `CMakeLists.txt`: IDBFS/pre-JS link inputs and test registration/exported
  runtime methods needed by browser verification.
- `tests/` and `tools/`: JavaScript state-machine tests, APK parity check, and
  same-origin headless-browser reload driver.
- `.github/workflows/build.yml`: run Android JVM/parity checks and WASM
  unit/browser persistence checks in their existing platform jobs.

## Test-driven verification

Every production change follows a red-green cycle. Each regression must fail
against current `main` for the named defect rather than for fixture/setup
errors, and every expected value is independently derived.

### Android host JVM tests

The pure Java deployment component will be exercised through real temporary
files. Fixtures cover:

- fresh extraction and committed `schema-2:<versionCode>` marker;
- migration from a legacy plain-number marker;
- replacement of a changed same-path asset;
- repair of a non-empty truncated destination;
- removal of paths absent from the packaged source;
- file-to-directory and directory-to-file transitions;
- an injected copy failure that leaves no current marker;
- successful retry after that failure; and
- byte-for-byte preservation of sibling settings/highscore fixtures outside
  `files/share/`.

The focused command is Gradle's host unit-test task. The platform gate also
assembles the release APK and compares every packaged asset path and hash with
the repository `share/` tree. This proves the extractor's assumed source tree
matches the artifact Gradle actually ships.

### WASM JavaScript tests

Zero-dependency JavaScript tests run the production `web/persistence.js`
module with a controlled fake FS/IDBFS boundary. They prove:

- the run dependency remains held until populate completes;
- populate failure is logged and still releases startup;
- no flush occurs before hydration completes;
- two rapid requests never overlap `syncfs` calls;
- a request during an active flush produces one follow-up flush; and
- idle waiters settle on success and error without wedging future requests.

### WASM real-browser reload test

The release-like WASM output is served with the repository's COOP/COEP server.
A zero-dependency headless-Chrome driver uses one temporary browser profile and
one stable scheme/host/port for the entire test. It will:

1. clear the origin's previous storage;
2. load the page and wait for hydration/runtime readiness;
3. write valid non-default `settings.ini`, `highscores`, and
   `highlevelshistory` fixtures into the production mount;
4. request the production flush and await persistence idle;
5. reload the same page in the same browser profile and origin;
6. assert hydration completed before runtime initialization; and
7. assert all three files retain their independently specified bytes.

The test verifies the real IDBFS/IndexedDB boundary. Host/native tests cover
the C++ eager-save entry points; the browser test does not substitute a mock
filesystem for reload durability.

### Repository-wide verification

The final gate includes:

- native Debug configure/build and full CTest;
- native ASan+UBSan configure/build and serial full CTest;
- Android host unit tests and release APK assembly/parity;
- WASM release configure/build, JavaScript tests, and headless-browser reload;
- `git diff --check` and a clean worktree.

No Android emulator or device is available on this host. The install-over-
install procedure remains explicitly unchecked: install a prior signed APK,
create preferences and truncate an extracted asset, install a newer same-key
APK with changed/deleted/new assets, relaunch, and verify the asset tree is
current while preferences survive.

## Documentation and ledger updates

After automated verification:

- add the user-visible fixes to `CHANGELOG.md`;
- update WASM persistence/build notes in `README.md` and `web/README.md`;
- close BUG-046 and BUG-048 in `docs/audit/REMEDIATION_STATUS.md` with exact
  commit and automated evidence references;
- change IMP-020 from fully open to partial rather than claiming its broader
  five-platform packaged-artifact matrix is complete;
- update defect/improvement arithmetic exactly; and
- record the unexecuted Android device/emulator procedure in
  `docs/MANUAL_TEST_CHECKLIST.md`.

Closing these two Medium defects changes the defect ledger from 56 fixed / 17
open to 58 fixed / 15 open, and Medium from 32 fixed / 13 open to 34 fixed / 11
open. If no other improvement changes occur, the mutually exclusive
improvement arithmetic becomes 15 done + 3 partial + 1 moot + 5 fully open =
24; the number of improvements with work remaining stays eight.

## Alternatives rejected

### Android versioned staged trees

Extracting into a versioned temporary tree and publishing it after completion
would retain the previous complete tree during upgrades. It was rejected for
this scope because it roughly doubles the 29 MiB/3,352-file storage footprint,
adds migration and cleanup states, and cannot safely run a new native binary
against old assets after a failed update anyway.

### Android hash-manifest reconciliation

A generated path/size/SHA-256 manifest could repair arbitrary same-version
corruption and avoid rewriting unchanged files. It was rejected because it
introduces a second build artifact and significant Gradle/runtime reconciliation
logic beyond BUG-046's update/interrupted-extraction failure. APK parity is
still verified without making a manifest part of the runtime contract.

### WASM IDBFS `autoPersist`

IDBFS auto-persist has less explicit plumbing, but it obscures flush completion
and error attribution and offers no natural deterministic idle gate for the
browser regression. The explicit serialized owner is more observable and
prevents overlapping syncs independently of Emscripten implementation details.

### `localStorage` file blobs

Encoding the three files into `localStorage` would be synchronous, but would
duplicate filesystem semantics, require custom encoding/version/corruption
handling, and create a second persistence system beside SDL's preference path.
IDBFS preserves the existing file ownership model.

## Constraints and non-goals

- Preserve Android's synchronous extraction before `super.onCreate()` and the
  exact Java-selected path passed through JNI.
- Never delete or replace all of `getFilesDir()`; only the canonical managed
  `share/` subtree and its asset marker are mutable here.
- Preserve Android API 21 compatibility and Java 8 source compatibility.
- Preserve the browser's existing same-origin nickname behavior.
- Never start WASM `main()` before the initial persistent populate attempt has
  completed, successfully or with an explicitly handled error.
- Never overlap IDBFS `syncfs` operations or depend on `beforeunload` for
  durability.
- Keep native settings/highscore behavior compatible; the platform flush is a
  native no-op.
- Do not claim live Android verification without a device/emulator run.
- Do not expand IMP-020 into its Windows, macOS, Linux, signing, dependency,
  logging, or full packaged-smoke matrix.
- Do not fix unrelated platform findings, release a new version, tag, or
  deploy artifacts as part of this remediation.
