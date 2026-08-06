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

