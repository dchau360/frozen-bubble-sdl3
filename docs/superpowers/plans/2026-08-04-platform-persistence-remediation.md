# Android Asset and WASM Persistence Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix BUG-046 by making Android asset deployment complete and
retryable, and fix BUG-048 by persisting browser settings and highscores
through a pre-main IDBFS hydrate plus serialized flushes.

**Architecture:** Android rebuilds only its managed `files/share/` tree when a
schema/version marker mismatches, propagates every deployment failure, and
starts SDL only after the complete marker commits. WASM mounts IDBFS at
`/libsdl/frozen-bubble` during `preRun`, blocks `main()` until populate
finishes, and routes closed settings/highscore files through one
serialized/coalescing flush owner.

**Tech Stack:** Java 8 / Android Gradle Plugin 8.2 / JUnit 4, C++17 / SDL3 /
CMake / CTest, Emscripten / IDBFS, JavaScript / Node 22 / Chrome DevTools
Protocol, Python 3.

## Global Constraints

- Preserve Android's synchronous extraction before `super.onCreate()` and the
  exact Java-selected path passed through JNI.
- Never delete or replace all of `getFilesDir()`; only the canonical managed
  `share/` subtree and its asset marker are mutable here.
- Preserve Android API 21 compatibility and Java 8 source compatibility.
- The committed Android marker format is exactly
  `schema-2:<versionCode>`.
- Preserve the browser's existing same-origin nickname behavior.
- The IDBFS mount path is exactly `/libsdl/frozen-bubble`.
- Never start WASM `main()` before the initial persistent populate attempt has
  completed, successfully or with an explicitly handled error.
- Never overlap IDBFS `syncfs` operations or depend on `beforeunload` for
  durability.
- Keep native settings/highscore behavior compatible; the platform flush is a
  native no-op.
- Every production change follows strict TDD: add a behavior regression,
  observe the expected RED failure, implement the minimum change, and observe
  GREEN before refactoring.
- Do not claim live Android verification without a device/emulator run. The
  device install-over-install procedure remains unchecked in this task.
- Do not expand IMP-020 into its Windows, macOS, Linux, signing, dependency,
  logging, or full packaged-smoke matrix.
- Do not fix unrelated platform findings, release a new version, tag, deploy,
  or push as part of implementation.

---

## File map

- `android/app/src/main/java/org/frozenbubble/AssetDeployment.java` — pure
  Java managed-tree deployment algorithm and asset-source interface.
- `android/app/src/main/java/org/frozenbubble/AssetExtractor.java` — Android
  `Context`/`AssetManager` adapter and public extraction entry.
- `android/app/src/main/java/org/frozenbubble/FrozenBubbleActivity.java` —
  fail-closed startup boundary before SDL.
- `android/app/src/test/java/org/frozenbubble/AssetDeploymentTest.java` — JVM
  transactional deployment regressions.
- `android/app/build.gradle` — JUnit dependency.
- `tools/verify_android_assets.py` and
  `tests/android_asset_parity_test.py` — APK/source path-and-hash parity gate.
- `web/persistence.js` — production IDBFS hydrate and serialized flush owner.
- `tests/wasm_persistence_test.cjs` — deterministic JavaScript state-machine
  regressions.
- `tools/test-wasm-persistence.mjs` — real same-origin Chrome reload test.
- `tools/serve-wasm.py` — dynamic-port output used by the browser driver.
- `src/platform.h` / `src/platform.cpp` — cross-platform persistent-storage
  flush boundary.
- `src/gamesettings.cpp` / `src/highscoremanager.cpp` — eager persistence at
  existing file ownership boundaries.
- `tests/persistence_save_test.cpp` — production-object native save regressions.
- `CMakeLists.txt` — IDBFS/pre-JS flags and native persistence test target.
- `.github/workflows/build.yml` — Android and WASM platform gates.
- `CHANGELOG.md`, `README.md`, `web/README.md`,
  `docs/audit/REMEDIATION_STATUS.md`, and
  `docs/MANUAL_TEST_CHECKLIST.md` — user and audit evidence.

---

### Task 1: Transactional Android managed-asset deployment (BUG-046)

**Files:**
- Create:
  `android/app/src/main/java/org/frozenbubble/AssetDeployment.java`
- Create:
  `android/app/src/test/java/org/frozenbubble/AssetDeploymentTest.java`
- Modify:
  `android/app/src/main/java/org/frozenbubble/AssetExtractor.java`
- Modify:
  `android/app/src/main/java/org/frozenbubble/FrozenBubbleActivity.java`
- Modify: `android/app/build.gradle`

**Interfaces:**
- Produces:

```java
final class AssetDeployment {
    static final String MARKER_PREFIX = "schema-2:";

    interface AssetSource {
        String[] list(String path) throws IOException;
        InputStream open(String path) throws IOException;
    }

    static File deploy(AssetSource source, File filesDir, long versionCode)
            throws IOException;
}
```

- `AssetDeployment.deploy(...)` returns exactly `new File(filesDir, "share")`
  after a complete deployment or throws `IOException` before a current marker
  exists.
- `AssetExtractor.extractAll(Context)` remains the public Android entry, now
  declares/propagates `IOException`, and adapts `AssetManager.list/open` to
  `AssetSource`.
- `FrozenBubbleActivity.sExtractedDataDir` is assigned only after successful
  deployment; `super.onCreate()` is not called on failure.

- [ ] **Step 1: Add the failing JVM behavior tests**

Add JUnit 4 to `android/app/build.gradle`:

```gradle
dependencies {
    testImplementation 'junit:junit:4.13.2'
    implementation 'com.google.android.gms:play-services-ads:23.3.0'
    implementation 'com.android.billingclient:billing:7.1.1'
}
```

Create `AssetDeploymentTest` with a `FakeAssetSource` backed by literal byte
arrays and temporary directories. Each test names the production break it
catches and derives expected bytes directly from literals. Required cases:

```java
@Test public void freshDeployWritesAllAssetsAndSchemaMarker()
@Test public void legacyMarkerForcesCompleteRebuild()
@Test public void rebuildReplacesChangedAndTruncatedFiles()
@Test public void rebuildRemovesDeletedAssets()
@Test public void rebuildHandlesFileDirectoryShapeChanges()
@Test public void copyFailureDoesNotCommitCurrentMarker()
@Test public void retryAfterFailureBuildsACompleteTree()
@Test public void rebuildPreservesPreferenceSiblings()
```

The shape-change test performs two independent deployments: source v1 has a
file at `gfx/swap`, source v2 has `gfx/swap/child.png`; then reverse the shape
using another path. The sibling test places literal `settings.ini`,
`highscores`, and `highlevelshistory` files directly under `filesDir` and
asserts their bytes are unchanged after deployment.

- [ ] **Step 2: Run the focused tests and verify RED**

Run:

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
```

Expected: compilation fails because `AssetDeployment` and its `AssetSource`
contract do not exist. This is the intended RED; Gradle/SDK setup failures do
not count.

- [ ] **Step 3: Implement the minimum pure-Java deployment algorithm**

In `AssetDeployment.deploy(...)`:

```java
File managedRoot = new File(filesDir, "share");
File marker = new File(filesDir, ".assets_version");
File markerTemp = new File(filesDir, ".assets_version.tmp");
String wanted = MARKER_PREFIX + versionCode;
String installed = marker.isFile() ? readUtf8(marker).trim() : "";

if (wanted.equals(installed) && managedRoot.isDirectory()) {
    return managedRoot;
}

deleteIfPresent(marker);
deleteIfPresent(markerTemp);
deleteRecursively(managedRoot);
if (!managedRoot.mkdirs() && !managedRoot.isDirectory()) {
    throw new IOException("Could not create managed asset root: " + managedRoot);
}
extractEntry(source, "", managedRoot);
writeAtomically(markerTemp, marker, wanted.getBytes(StandardCharsets.UTF_8));
return managedRoot;
```

`extractEntry` recurses from the APK asset root. A leaf is copied to
`<destination>.tmp`, both streams are closed with try-with-resources, then the
temporary file is renamed into place. `deleteRecursively` rejects any target
other than the `managedRoot` supplied internally and throws on every failed
delete. Removing the old marker before touching `share/` is load-bearing: a
failed rebuild must never leave a current marker beside a partial directory.
Marker replacement and file replacement use same-directory renames so API 21
and Java 8 remain sufficient; do not introduce `java.nio.file.Files` APIs that
require newer Android behavior.

No catch inside the deployment core may convert an exception into success.

- [ ] **Step 4: Adapt Android startup and fail closed**

Make `AssetExtractor` a thin adapter:

```java
public static String extractAll(Context context) throws IOException {
    PackageInfo packageInfo;
    try {
        packageInfo = context.getPackageManager()
                .getPackageInfo(context.getPackageName(), 0);
    } catch (PackageManager.NameNotFoundException e) {
        throw new IOException("Could not determine package version", e);
    }
    long versionCode = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
            ? packageInfo.getLongVersionCode()
            : packageInfo.versionCode;
    AssetManager manager = context.getAssets();
    AssetDeployment.AssetSource source = new AssetDeployment.AssetSource() {
        public String[] list(String path) throws IOException {
            return manager.list(path);
        }
        public InputStream open(String path) throws IOException {
            return manager.open(path);
        }
    };
    return AssetDeployment.deploy(source, context.getFilesDir(), versionCode)
            .getAbsolutePath();
}
```

Import `Build`, `PackageInfo`, and `PackageManager`. The guarded legacy
`versionCode` branch is required for API 21--27; do not call
`getLongVersionCode()` unconditionally.

At the start of `FrozenBubbleActivity.onCreate()`:

```java
try {
    sExtractedDataDir = AssetExtractor.extractAll(this);
} catch (Exception e) {
    sExtractedDataDir = "";
    Log.e("FBubble.Assets", "Asset deployment failed; SDL will not start", e);
    Toast.makeText(this,
            "Game assets could not be prepared. Restart or reinstall the app.",
            Toast.LENGTH_LONG).show();
    finish();
    return;
}
super.onCreate(savedInstanceState);
```

Import only `Log` and `Toast`; preserve the existing ordering and JNI field.

- [ ] **Step 5: Run focused and Android build verification**

Run:

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
./gradlew :app:assembleDebug --no-daemon
```

Expected: all eight JVM cases pass and the Android debug APK assembles.

- [ ] **Step 6: Commit Task 1**

```bash
git add android/app/build.gradle \
  android/app/src/main/java/org/frozenbubble/AssetDeployment.java \
  android/app/src/main/java/org/frozenbubble/AssetExtractor.java \
  android/app/src/main/java/org/frozenbubble/FrozenBubbleActivity.java \
  android/app/src/test/java/org/frozenbubble/AssetDeploymentTest.java
git commit -m "fix(android): deploy packaged assets transactionally (BUG-046)"
```

---

### Task 2: Verify APK asset parity

**Files:**
- Create: `tools/verify_android_assets.py`
- Create: `tests/android_asset_parity_test.py`

**Interfaces:**
- Produces CLI:

```text
python3 tools/verify_android_assets.py --apk <apk> --source <share-directory>
```

- Exit `0` only when the APK's non-directory `assets/**` entries, after
  removing the `assets/` prefix, exactly match source relative paths and
  SHA-256 values. Exit non-zero with separate missing, unexpected, and
  mismatched sections otherwise.

- [ ] **Step 1: Write the failing parity-script tests**

Create synthetic ZIP/APK files with `zipfile.ZipFile` and literal contents.
Cover:

```python
def test_matching_paths_and_hashes_pass()
def test_missing_packaged_asset_fails()
def test_unexpected_packaged_asset_fails()
def test_changed_packaged_asset_fails()
```

Invoke the real CLI through `subprocess.run`; assert exit codes and the exact
relative path in each diagnostic rather than importing and duplicating its
comparison logic.

- [ ] **Step 2: Run the parity tests and verify RED**

Run:

```bash
python3 tests/android_asset_parity_test.py
```

Expected: failure because `tools/verify_android_assets.py` does not exist.

- [ ] **Step 3: Implement the path-and-hash comparison**

Use these data shapes:

```python
def source_hashes(root: pathlib.Path) -> dict[str, str]:
    return {
        path.relative_to(root).as_posix(): sha256(path.read_bytes()).hexdigest()
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }

def apk_hashes(apk: pathlib.Path) -> dict[str, str]:
    with zipfile.ZipFile(apk) as archive:
        return {
            info.filename.removeprefix("assets/"):
                sha256(archive.read(info)).hexdigest()
            for info in archive.infolist()
            if info.filename.startswith("assets/") and not info.is_dir()
        }
```

Compare key sets first, then hashes for the intersection. Print sorted paths so
CI output is deterministic. Do not ignore source files silently; Gradle's
configured source directory is the contract being checked.

- [ ] **Step 4: Run focused tests and the real debug APK parity check**

Run:

```bash
python3 tests/android_asset_parity_test.py
python3 tools/verify_android_assets.py \
  --apk android/app/build/outputs/apk/debug/app-debug.apk \
  --source share
```

Expected: four tests pass and the built APK reports matching asset count and
hashes.

- [ ] **Step 5: Commit Task 2**

```bash
git add tools/verify_android_assets.py tests/android_asset_parity_test.py
git commit -m "test(android): verify packaged asset parity"
```

---

### Task 3: Pre-main IDBFS hydration and serialized WASM flushes (BUG-048)

**Files:**
- Create: `web/persistence.js`
- Create: `tests/wasm_persistence_test.cjs`
- Create: `tools/test-wasm-persistence.mjs`
- Modify: `tools/serve-wasm.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces CommonJS-testable factory:

```javascript
createFrozenBubblePersistence(Module, FS, IDBFS, runtime)
```

where `runtime` supplies `addRunDependency(name)` and
`removeRunDependency(name)`.
- Produces browser APIs:

```javascript
Module.persistentStorageReady          // Promise<{ok: boolean, error?: Error}>
Module.requestPersistentStorageFlush() // Promise<void>
Module.whenPersistentStorageIdle()     // Promise<void>
Module.FS                              // exported by Emscripten for browser test
Module.persistentStorageReadyBeforeRuntime // boolean ordering witness
```

- The mount path is exactly `/libsdl/frozen-bubble`; the run-dependency name is
  exactly `frozen-bubble-idbfs`.

- [ ] **Step 1: Write deterministic state-machine tests before production JS**

In `tests/wasm_persistence_test.cjs`, use `node:test`, `node:assert/strict`, a
fake `FS` that records `mkdirTree`, `mount`, and queued `syncfs` callbacks, and
literal dependency events. Cover:

```javascript
test('hydrate holds the run dependency until populate succeeds', async () => {})
test('hydrate error releases startup and reports failure', async () => {})
test('flush requested before hydrate waits for populate', async () => {})
test('rapid flush requests never overlap syncfs', async () => {})
test('dirty write during flush schedules exactly one follow-up', async () => {})
test('flush error settles waiters and later requests can retry', async () => {})
```

Assert the exact boolean passed to `syncfs`: `true` only for populate and
`false` only for flush.

- [ ] **Step 2: Run JavaScript tests and verify RED**

Run:

```bash
node --test tests/wasm_persistence_test.cjs
```

Expected: module-load failure because `web/persistence.js` does not exist.

- [ ] **Step 3: Implement the persistence controller minimally**

Use a UMD-style production file so Node can import the factory while Emscripten
can register `Module.preRun`:

```javascript
(function(root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory;
    return;
  }
  root.createFrozenBubblePersistence = factory;
  Module.persistentStoragePopulateCompleted = false;
  Module.frozenBubbleRuntimeInitialized = false;
  Module.persistentStorageReadyBeforeRuntime = false;

  const previousRuntimeInitialized = Module.onRuntimeInitialized;
  Module.onRuntimeInitialized = function() {
    Module.persistentStorageReadyBeforeRuntime =
        Module.persistentStoragePopulateCompleted;
    Module.frozenBubbleRuntimeInitialized = true;
    if (previousRuntimeInitialized) {
      previousRuntimeInitialized.call(Module);
    }
  };

  Module.preRun = Module.preRun || [];
  Module.preRun.push(function() {
    const controller = factory(Module, FS, IDBFS, {
      addRunDependency: function(name) { addRunDependency(name); },
      removeRunDependency: function(name) { removeRunDependency(name); }
    });
    Module.FS = FS;
    Module.requestPersistentStorageFlush = controller.requestFlush;
    Module.whenPersistentStorageIdle = controller.whenIdle;
    Module.persistentStorageReady = controller.hydrate();
  });
})(typeof globalThis !== 'undefined' ? globalThis : this,
function createFrozenBubblePersistence(Module, FS, IDBFS, runtime) {
  const mountPath = '/libsdl/frozen-bubble';
  const dependency = 'frozen-bubble-idbfs';
  let hydrated = false;
  let inFlight = false;
  let dirty = false;
  let idleWaiters = [];
  let hydratePromise;

  function settleWaiters(error) {
    const waiters = idleWaiters;
    idleWaiters = [];
    for (const waiter of waiters) {
      if (error) waiter.reject(error);
      else waiter.resolve();
    }
  }

  function startFlushIfNeeded() {
    if (!hydrated || inFlight || !dirty) return;
    dirty = false;
    inFlight = true;

    const complete = function(error) {
      inFlight = false;
      if (error) {
        dirty = false;
        if (Module.printErr) {
          Module.printErr('Persistent storage flush failed: ' + error);
        }
        settleWaiters(error);
      } else if (dirty) {
        startFlushIfNeeded();
      } else {
        settleWaiters();
      }
    };

    try {
      FS.syncfs(false, complete);
    } catch (error) {
      complete(error);
    }
  }

  function whenIdle() {
    if (hydrated && !inFlight && !dirty) return Promise.resolve();
    return new Promise(function(resolve, reject) {
      idleWaiters.push({resolve, reject});
    });
  }

  function requestFlush() {
    dirty = true;
    const completion = whenIdle();
    startFlushIfNeeded();
    return completion;
  }

  function hydrate() {
    if (hydratePromise) return hydratePromise;
    runtime.addRunDependency(dependency);
    hydratePromise = new Promise(function(resolve) {
      let completed = false;
      const complete = function(error) {
        if (completed) return;
        completed = true;
        hydrated = true;
        Module.persistentStoragePopulateCompleted = true;
        if (error && Module.printErr) {
          Module.printErr('Persistent storage hydrate failed: ' + error);
        }
        resolve(error ? {ok: false, error} : {ok: true});
        runtime.removeRunDependency(dependency);
        if (error) {
          dirty = false;
          settleWaiters(error);
        } else if (dirty) {
          startFlushIfNeeded();
        } else {
          settleWaiters();
        }
      };

      try {
        FS.mkdirTree(mountPath);
        FS.mount(IDBFS, {}, mountPath);
        FS.syncfs(true, complete);
      } catch (error) {
        complete(error);
      }
    });
    return hydratePromise;
  }

  return {hydrate, requestFlush, whenIdle};
});
```

On a flush error, clear the current dirty state, reject the current idle
waiters, set `inFlight = false`, and allow a later request to run. Hydration
returns `{ok:false,error}` rather than leaving an unhandled rejected promise,
logs through `Module.printErr`, and always removes the run dependency.

The browser path appends one `Module.preRun` callback, constructs the controller
with the real `FS`, `IDBFS`, `addRunDependency`, and `removeRunDependency`,
publishes the browser APIs, and starts hydration. The preserved runtime callback
captures `persistentStoragePopulateCompleted` at the moment runtime initializes;
the browser test asserts that captured ordering witness is `true`.

- [ ] **Step 4: Add Emscripten link inputs**

In the existing `EMSCRIPTEN` link options add:

```cmake
"SHELL:--pre-js ${CMAKE_SOURCE_DIR}/web/persistence.js"
-lidbfs.js
"SHELL:-sEXPORTED_RUNTIME_METHODS=['cwrap','ccall','setValue','getValue','UTF8ToString','stringToUTF8','FS']"
```

Replace the existing exported-runtime-methods line rather than adding a second
conflicting setting.

- [ ] **Step 5: Make the local server report its assigned dynamic port**

After creating `TCPServer`, derive:

```python
actual_port = httpd.server_address[1]
url = f"http://localhost:{actual_port}/{landing or ''}"
```

This makes `--port 0` usable by the browser test without racing for a hardcoded
port.

- [ ] **Step 6: Write the real headless-Chrome reload test**

`tools/test-wasm-persistence.mjs` must use only Node built-ins and the Node 22
global `WebSocket`. It will:

1. locate Chrome from `CHROME_BIN` or the known macOS/Linux paths;
2. spawn `tools/serve-wasm.py <build-dir> --port 0` and parse its printed URL;
3. spawn Chrome with `--headless=new`, `--no-sandbox`,
   `--remote-debugging-port=0`, and a new temporary `--user-data-dir`;
4. connect to the page DevTools WebSocket, enable `Page` and `Runtime`, and
   navigate to the server URL;
5. wait until `Module.persistentStorageReady` and runtime initialization are
   complete;
6. write exact literal UTF-8 fixtures to the three files through `Module.FS`;
7. await `Module.requestPersistentStorageFlush()` and
   `Module.whenPersistentStorageIdle()`;
8. reload the same tab and origin; and
9. assert the hydration-before-runtime boolean and exact bytes of all files.

The literal fixtures are:

```text
settings.ini: [GFX]\nQuality = 2\n[Keys]\nSpeedMultiplier = 4.25\n
highscores:    17,Browser Test,12.5,3\n
highlevelshistory: 0   1   2   3   4   5   6   7\n (repeated as ten literal rows), then \n
```

Store the expected history as one explicit ten-row string in the test rather
than deriving expected data with the production serializer. This keeps the
fixture valid for `LoadHighscoreLevels()` when the game reloads it.

All child processes must be terminated in `finally`, and only the test's own
temporary browser profile may be deleted.

- [ ] **Step 7: Verify GREEN at unit and browser levels**

First run:

```bash
node --test tests/wasm_persistence_test.cjs
```

Then configure/build WASM using the repository's documented Emscripten port
patch procedure, followed by:

```bash
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --parallel
node tools/test-wasm-persistence.mjs build-wasm
```

Expected: six JavaScript tests pass; the WASM build links IDBFS; the browser
test persists all three fixtures across a reload at one origin and confirms
hydrate-before-runtime ordering.

- [ ] **Step 8: Commit Task 3**

```bash
git add CMakeLists.txt web/persistence.js tests/wasm_persistence_test.cjs \
  tools/test-wasm-persistence.mjs tools/serve-wasm.py
git commit -m "fix(wasm): hydrate and flush IDBFS persistence (BUG-048)"
```

---

### Task 4: Persist every settings and highscore mutation eagerly

**Files:**
- Modify: `src/platform.h`
- Modify: `src/platform.cpp`
- Modify: `src/gamesettings.cpp`
- Modify: `src/highscoremanager.cpp`
- Create: `tests/persistence_save_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
void RequestPersistentStorageFlush();
```

- The function invokes `Module.requestPersistentStorageFlush()` under
  `__WASM_PORT__` and does nothing on native platforms.
- `GameSettings::SaveSettings()` calls it only after `fclose`.
- `HighscoreManager::SaveNewHighscores()` calls it only after both output
  streams close.

- [ ] **Step 1: Add a production-object persistence CTest**

Create a temporary preference directory ending in `/`, assign its stable
string storage to the public `GameSettings::prefPath`, and call
`ReadSettings()`. With a real SDL dummy renderer and repository assets, cover:

```cpp
// SetValue must make the new dictionary value visible in settings.ini now.
settings->SetValue("GFX:ShowFPS", "");
CHECK(fileContains(settingsPath, "ShowFPS = true"));

// A completed level grid must reach highlevelshistory immediately.
manager->AppendToLevels(literalGrid, 17);
CHECK(fileContains(historyPath, "1   2   3"));

// A qualified score must reach highscores before dialog/destructor cleanup.
CHECK(manager->CheckAndAddScore(17, 12.5f));
CHECK(csvHasLevelAndTime(scorePath, 17, 12.5f));
```

Use literal expectations and real files. Do not assert on a flush mock. Dispose
the singleton objects only after the pre-dispose assertions, then destroy the
renderer/window and remove only the test-created temporary directory.

Register `persistence-save-test` with `tests/persistence_save_test.cpp` plus
`${FROZEN_BUBBLE_CORE_SOURCES}`, the same compile definitions, includes, and
SDL/iniparser libraries as `bubblegame-rules-test`.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target persistence-save-test --parallel
ctest --test-dir build -R '^persistence-save-test$' --output-on-failure
```

Expected: the three pre-dispose persistence assertions fail on current code:
`SetValue`, `AppendToLevels`, and `CheckAndAddScore` update memory but do not
write their files eagerly.

- [ ] **Step 3: Add the cross-platform flush boundary**

Declare `RequestPersistentStorageFlush()` unconditionally in `platform.h`.
Implement it unconditionally after the existing WASM-only block in
`platform.cpp` (the `EM_ASM` body itself remains guarded):

```cpp
void RequestPersistentStorageFlush() {
#ifdef __WASM_PORT__
    EM_ASM({
        if (Module['requestPersistentStorageFlush']) {
            // The JS controller reports the error; consume the rejection here
            // so a C++ fire-and-forget request is never unhandled.
            Module['requestPersistentStorageFlush']().catch(function() {});
        }
    });
#endif
}
```

Native compilation leaves an intentional empty function body.

- [ ] **Step 4: Persist settings after every mutation**

Call `RequestPersistentStorageFlush()` after `fclose(setFile)` in
`SaveSettings()`.

For every recognized `SetValue` branch, call `SaveSettings()` after updating
`optDict` and before returning. The fall-through path also calls
`SaveSettings()` once after `iniparser_set`. Keep `setSoundEnabled()` and
`SaveKeys()` on their existing save path; they inherit the new flush without a
second direct request.

- [ ] **Step 5: Persist highscore/history mutations after closed files**

Call `RequestPersistentStorageFlush()` at the end of
`SaveNewHighscores()`, after both `.close()` calls.

Call `SaveNewHighscores()` at the end of `AppendToLevels()`. In
`CheckAndAddScore()`, call it only after a qualifying entry has been inserted,
sorted, and truncated to ten. The later name-entry save remains and records the
entered name as a second valid update.

- [ ] **Step 6: Verify focused and complete native suites**

Run:

```bash
cmake --build build --parallel
ctest --test-dir build -R '^persistence-save-test$' --output-on-failure
ctest --test-dir build --output-on-failure -j1
```

Expected after Task 4: the new target passes; ordinary CTest has 12 registered
tests, 10 passes, and the two sanitizer-only server tests skip.

- [ ] **Step 7: Rebuild WASM and repeat the browser reload regression**

Run:

```bash
cmake --build build-wasm --parallel
node tools/test-wasm-persistence.mjs build-wasm
```

Expected: reload persistence remains green with the real C++ flush call linked
into the artifact.

- [ ] **Step 8: Commit Task 4**

```bash
git add src/platform.h src/platform.cpp src/gamesettings.cpp \
  src/highscoremanager.cpp tests/persistence_save_test.cpp CMakeLists.txt
git commit -m "fix: flush settings and highscores eagerly (BUG-048)"
```

---

### Task 5: Run platform persistence gates in CI

**Files:**
- Modify: `.github/workflows/build.yml`

**Interfaces:**
- Android job consumes the Gradle test task and
  `tools/verify_android_assets.py` from Tasks 1–2.
- WASM job consumes `tests/wasm_persistence_test.cjs` and
  `tools/test-wasm-persistence.mjs` from Task 3.

- [ ] **Step 1: Prove every command locally before editing CI**

Run the exact commands CI will use:

```bash
cd android
./gradlew :app:testDebugUnitTest --no-daemon
cd ..
node --test tests/wasm_persistence_test.cjs
python3 tools/verify_android_assets.py \
  --apk android/app/build/outputs/apk/debug/app-debug.apk --source share
node tools/test-wasm-persistence.mjs build-wasm
```

Expected: all commands exit `0`. This is the behavior gate for the workflow
change; no test should grep workflow source.

- [ ] **Step 2: Add Android job gates**

After submodule/toolchain setup and before the release build, add:

```yaml
- name: Test Android asset deployment
  working-directory: android
  run: |
    chmod +x gradlew
    ./gradlew :app:testDebugUnitTest --no-daemon
```

After the existing APK rename step, add:

```yaml
- name: Verify packaged Android assets
  run: |
    python3 tools/verify_android_assets.py \
      --apk frozen-bubble-android-tv.apk --source share
```

- [ ] **Step 3: Add WASM job gates**

Add `actions/setup-node@v4` with Node 22, then run the deterministic JS tests
before configure/build:

```yaml
- uses: actions/setup-node@v4
  with:
    node-version: '22'

- name: Test WASM persistence state machine
  run: node --test tests/wasm_persistence_test.cjs
```

After packaging `dist-wasm`, add:

```yaml
- name: Test WASM persistence across reload
  run: node tools/test-wasm-persistence.mjs dist-wasm
```

The driver discovers `google-chrome`, `chromium`, or `chromium-browser` from
`PATH` (while still honoring `CHROME_BIN`) and fails with a clear diagnostic if
the runner supplies none; do not hardcode one executable path in the workflow.

- [ ] **Step 4: Validate workflow syntax and rerun local gates**

Run:

```bash
python3 -c 'import yaml' 2>/dev/null \
  && python3 -c 'import yaml; yaml.safe_load(open(".github/workflows/build.yml"))' \
  || ruby -e 'require "yaml"; YAML.load_file(".github/workflows/build.yml")'
node --test tests/wasm_persistence_test.cjs
python3 tests/android_asset_parity_test.py
```

Expected: workflow parses and both permanent host test groups pass.

- [ ] **Step 5: Commit Task 5**

```bash
git add .github/workflows/build.yml
git commit -m "ci: verify Android and WASM persistence"
```

---

### Task 6: Close BUG-046 and BUG-048 without overstating IMP-020

**Files:**
- Modify: `CHANGELOG.md`
- Modify: `README.md`
- Modify: `web/README.md`
- Modify: `docs/audit/REMEDIATION_STATUS.md`
- Modify: `docs/MANUAL_TEST_CHECKLIST.md`

**Interfaces:**
- Consumes the final Task 1–5 commit SHAs and their reports.
- Produces exact defect arithmetic: 58 fixed / 15 open overall; Medium 34 fixed
  / 11 open.
- Produces exact improvement arithmetic: 15 done + 3 partial + 1 moot + 5
  fully open = 24. IMP-020 moves from fully open to partial; eight
  improvements still have remaining work.

- [ ] **Step 1: Update user-facing release notes and WASM documentation**

Add changelog bullets stating:

- Android upgrades now rebuild the managed packaged-asset tree completely and
  retry after interrupted/failed extraction without deleting preferences.
- Browser settings, keys, level history, and highscores now use IDBFS and
  survive same-origin reloads.

In `README.md` and `web/README.md`, document that browser persistence is scoped
to the browser profile and exact origin, and that private/quota-disabled
storage falls back to session-only defaults with a console diagnostic. Do not
claim cross-origin, cross-device, or private-mode durability.

- [ ] **Step 2: Update the audit ledger exactly**

In `docs/audit/REMEDIATION_STATUS.md`:

- remove BUG-046 and BUG-048 from the Medium open list;
- add fixed entries citing the actual implementation commits and automated
  Android JVM/APK and WASM unit/browser evidence;
- change totals to 58 fixed / 15 open and Medium to 34 fixed / 11 open;
- list IMP-020 among partial improvements, leaving its other packaged-platform
  requirements explicit;
- change improvement arithmetic to
  `15 done + 3 partial + 1 moot + 5 fully open = 24`; and
- make the suggested next order begin with the remaining protocol/lobby and
  settings defect work rather than the now-closed platform pair.

- [ ] **Step 3: Record the unexecuted Android runtime procedure**

Add one unchecked item to `docs/MANUAL_TEST_CHECKLIST.md` that requires two
same-signer APKs with increasing version codes. It must explicitly check a
changed asset, deleted asset, new asset, non-empty truncation repair, exact
`g_dataDir`, and preservation of settings/highscores. State that no emulator
or device run occurred during this remediation.

Record the WASM browser reload as automated evidence, not an unchecked manual
claim, only if Task 3/4 actually ran it successfully.

- [ ] **Step 4: Reconcile documentation and inspect the exact diff**

Run:

```bash
rg -n 'BUG-046|BUG-048|IMP-020|58|15 open|34 fixed|11 open' \
  CHANGELOG.md README.md web/README.md docs/audit/REMEDIATION_STATUS.md \
  docs/MANUAL_TEST_CHECKLIST.md
git diff --check
git diff -- CHANGELOG.md README.md web/README.md \
  docs/audit/REMEDIATION_STATUS.md docs/MANUAL_TEST_CHECKLIST.md
```

Expected: the two defects are fixed, IMP-020 is partial, arithmetic is
mutually exclusive, and the Android device procedure remains unchecked.

- [ ] **Step 5: Run the final verification matrix**

Native Debug:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure -j1
```

Native ASan+UBSan:

```bash
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS=-fsanitize=address,undefined \
  -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure -j1
```

Android:

```bash
cd android
./gradlew :app:testDebugUnitTest :app:assembleRelease --no-daemon
cd ..
android_apk='android/app/build/outputs/apk/release/app-release-unsigned.apk'
test -f "$android_apk" || android_apk='android/app/build/outputs/apk/release/app-release.apk'
python3 tools/verify_android_assets.py --apk "$android_apk" --source share
```

WASM:

```bash
node --test tests/wasm_persistence_test.cjs
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --parallel
node tools/test-wasm-persistence.mjs build-wasm
```

Final hygiene:

```bash
git diff --check
git status --short
```

Expected: ordinary CTest has 12 registered tests with 10 passing and two
expected sanitizer-only skips; sanitizer CTest passes 12/12; Android JVM tests
and release APK parity pass; WASM unit and same-origin reload tests pass; no
sanitizer diagnostics or uncommitted files remain.

- [ ] **Step 6: Commit Task 6**

```bash
git add CHANGELOG.md README.md web/README.md \
  docs/audit/REMEDIATION_STATUS.md docs/MANUAL_TEST_CHECKLIST.md
git commit -m "docs: close Android and WASM persistence findings"
```

---

## Final review and completion gate

After every task has its own spec/quality approval:

1. Generate one whole-branch review package from the branch merge base through
   `HEAD`.
2. Dispatch a fresh architecture-level reviewer over the complete package,
   this plan, task reports, and deferred-minor ledger.
3. If the reviewer finds load-bearing issues, use the single final fix wave and
   one scoped re-review required by subagent-driven development.
4. Rerun the complete Task 6 verification matrix after the final reviewed
   commit.
5. Do not mark the Android device/emulator procedure as executed.
6. Use `superpowers:finishing-a-development-branch` and let the user choose
   local merge, push/PR, or preserving the branch.
