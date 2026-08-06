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

