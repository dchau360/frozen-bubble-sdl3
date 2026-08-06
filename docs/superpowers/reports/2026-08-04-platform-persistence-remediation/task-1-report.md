# Task 1 Report — Transactional Android Managed-Asset Deployment (BUG-046)

## Status

`DONE`

Committed implementation:

```text
867b5f12 fix(android): deploy packaged assets transactionally (BUG-046)
```

## Implementation

- Added the pure-Java, package-private `AssetDeployment` core.  It owns only
  `<filesDir>/share` and the two asset-marker files.  A matching
  `schema-2:<versionCode>` marker plus a directory permits a fast return;
  otherwise the existing marker and stale managed tree are removed before a
  fresh extraction begins.
- Extraction recursively traverses the supplied `AssetSource`, copies every
  leaf through a same-directory `.tmp` file, and renames it only after both
  streams have closed.  The marker is likewise written via
  `.assets_version.tmp` and renamed only after the complete tree is present.
  All core I/O failures remain `IOException`s; none are converted to success.
- Replaced `AssetExtractor` with the Android adapter.  It gets the package
  version with the required API 21--27 `versionCode` fallback and adapts
  `AssetManager.list/open` to `AssetDeployment.AssetSource`.
- Made activity startup fail closed: `sExtractedDataDir` is populated only
  after a successful deploy; failure clears it, logs and shows the prescribed
  message, finishes the activity, and returns before `super.onCreate()`.
- Added JUnit 4 and eight JVM regressions using a literal-byte fake asset
  source plus real temporary filesystem directories.

## Files Changed

- `android/app/build.gradle`
- `android/app/src/main/java/org/frozenbubble/AssetDeployment.java` (new)
- `android/app/src/main/java/org/frozenbubble/AssetExtractor.java`
- `android/app/src/main/java/org/frozenbubble/FrozenBubbleActivity.java`
- `android/app/src/test/java/org/frozenbubble/AssetDeploymentTest.java` (new)

## Strict TDD Evidence

### RED

Command run before production deployment code existed:

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
```

Result: exit 1, as expected. `:app:compileDebugUnitTestJavaWithJavac` reported
18 errors beginning with `package AssetDeployment does not exist` and
`cannot find symbol AssetDeployment`. This was the intended missing-contract
failure, not an SDK or Gradle setup failure.

### GREEN

Command run after implementation (and rerun after final cleanup):

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
```

Result: exit 0, `BUILD SUCCESSFUL`. The generated JUnit XML records:

```text
tests="8" skipped="0" failures="0" errors="0"
```

Covered regressions:

1. fresh deployment writes every asset and a schema marker;
2. legacy marker forces a complete rebuild;
3. changed and truncated files are replaced;
4. removed APK assets disappear from the managed tree;
5. file-to-directory and directory-to-file shape changes work;
6. a copying failure cannot commit a current marker;
7. retrying after failure builds a complete tree; and
8. preference siblings directly below `filesDir` are preserved.

## Verification

- `git diff --check` before commit: exit 0.
- `git diff --cached --check` before commit: exit 0.
- Final focused JVM test command above: exit 0 (8/8).
- `git show --check --stat --oneline 867b5f12`: exit 0, no whitespace errors.
- Attempted required APK verification after initializing the Android SDL
  submodules:

  ```bash
  cd android
  ./gradlew :app:assembleDebug --no-daemon
  ```

  It compiled the Java changes and the SDL/native dependencies but exited 1
  while linking the pre-existing `libmain.so` target. The linker reported
  undefined `BuildLocalMultiplayerOptions`, `BuildLocalMultiplayerSettings`,
  and `ApplyLocalMultiplayerVictoriesInput`, referenced by `mainmenu.cpp` and
  `mainmenu_input.cpp`. `android/app/CMakeLists.txt` does not include the
  source defining those symbols. This is outside Task 1's allowed files and
  unrelated to the Java deployment changes.

## Self-Review

Reviewed the committed diff against every item in `task-1-brief.md`:

- The exact `schema-2:` prefix and public adapter signature are present.
- The marker is removed before `share/` is touched, and is written only after
  extraction completes.
- File and marker replacements are same-directory renames and use no
  `java.nio.file.Files` production APIs; source compatibility remains Java 8.
- The managed-tree deletion entrypoint rejects a non-managed target, while
  all delete/list/rename failures are propagated as `IOException`.
- The legacy Android version-code branch, pre-SDL extraction ordering,
  unchanged JNI field, and failure return before `super.onCreate()` are all
  retained.
- Tests use independent literal byte expectations rather than implementation
  helpers, and the mutation targets called out by the task (stale files,
  truncation, shape changes, failed copy, and sibling preservation) each have
  an observable assertion.
- An independent post-commit review found no critical, important, or minor
  findings. It separately confirmed the API 21--27 guard, marker/failure
  ordering, pre-`super.onCreate()` failure behavior, all eight JVM cases, and
  the unrelated native-link assembly blocker. No source edits were made by the
  reviewer.

## Concerns

1. The original Android assembly blocker is resolved by Fix Round 3; the exact
   `assembleDebug` command now packages the debug APK successfully.
2. Initializing the required Android submodules caused a build tool to delete
   a tracked third-party `zconf.h`; it was restored immediately. The worktree
   was clean after the Task 1 commit (apart from this uncommitted report, which
   is intentionally outside the implementation commit).

## Fix Round 1 — Symlink-Safe Managed-Root Deletion

### Change

Commit `9b6f1cd5 fix(android): contain managed asset deletion (BUG-046)` closes
the containment gap identified in review. `AssetDeployment` now detects a
symbolic link by comparing a file's canonical parent with the parent of its
canonical path. The valid-marker fast path rejects a symlinked `share` root,
and `deleteTree()` checks every root/child before `exists()`, `isDirectory()`,
or `listFiles()`. A link is unlinked with `File.delete()` and is never
traversed; this also handles broken links without treating them as absent.

### Covering Regression

Added `rebuildUnlinksManagedRootSymlinkWithoutDeletingTarget`. It creates a
stale-marker rebuild in which `<filesDir>/share` is a symlink to a separate
directory containing the literal `must-survive.txt` file. The test asserts
that the outside bytes remain `outside-data`, the replacement `share` is no
longer a symlink, and the new managed asset was deployed.

### RED

Before the containment change, the command was:

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
```

It exited 1 with the intended reproduction:

```text
AssetDeploymentTest > rebuildUnlinksManagedRootSymlinkWithoutDeletingTarget FAILED
9 tests completed, 1 failed
```

The failure was the external-sentinel assertion at
`AssetDeploymentTest.java:200`, proving that the old recursive deletion had
followed the `share` link and removed its target's file.

### GREEN

After the minimal symlink detection/unlinking change, the same focused command
exited 0 with `BUILD SUCCESSFUL`. Its JUnit XML recorded:

```text
tests="9" skipped="0" failures="0" errors="0"
```

### Self-Review

- The production fix uses only `File.getCanonicalFile()` and `File.delete()`,
  not `java.nio.file.Files`, preserving the API 21/Java 8 deployment core.
- The link check precedes all directory traversal in `deleteTree`, so both a
  symlinked root and any symlinked child are unlinked rather than followed.
- A matching marker no longer authorizes a symlinked root to be returned as
  the managed data directory.
- `git diff --check`, `git diff --cached --check`, and
  `git show --check 9b6f1cd5` completed without whitespace errors.

## Fix Round 2 — Sibling-Target Symlink Containment

### Change

Commit `d756daaf fix(android): detect sibling asset symlinks (BUG-046)` fixes
the parent-equality bypass in the first containment detector. The detector now
builds the complete expected canonical path from the canonical parent plus the
entry name, then compares it with the entry's complete canonical path. Thus
`<filesDir>/share -> <filesDir>/other` is identified as a link even though
both paths have the same canonical parent. The existing root fast-path and
pre-traversal unlink behavior apply to this case and to symlinked children.

### Covering Regression

Added `rebuildUnlinksSiblingTargetSymlinkWithoutDeletingTarget`. It creates
`<filesDir>/other/must-survive.txt`, makes `<filesDir>/share` a symbolic link
to that sibling, forces a stale-marker rebuild, and asserts the literal
`sibling-data` survives. It also verifies that the deployed root is a real
directory rather than a link and contains the fresh `gfx/blue.png` asset.

### RED

Before the full-path comparison, this focused command ran:

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
```

It exited 1 with:

```text
AssetDeploymentTest > rebuildUnlinksSiblingTargetSymlinkWithoutDeletingTarget FAILED
10 tests completed, 1 failed
```

The failure occurred at the sibling sentinel assertion
(`AssetDeploymentTest.java:216`), demonstrating that the prior detector
followed the sibling target during managed-tree deletion.

### GREEN

After the detector change, the same focused command exited 0 with
`BUILD SUCCESSFUL`. The JUnit XML records:

```text
tests="10" skipped="0" failures="0" errors="0"
```

### Self-Review

- The comparison includes both canonical parent and filename, which detects
  sibling redirects while avoiding false positives from a canonicalized parent
  path.
- It remains pure Java `File` I/O, preserving the API 21/Java 8 constraint and
  the existing no-`java.nio.file.Files` production rule.
- Link detection still occurs before the valid-marker directory return and
  before all recursive `isDirectory()`/`listFiles()` traversal.
- `git diff --check`, `git diff --cached --check`, and
  `git show --check d756daaf` were clean.

## Fix Round 3 — Android Local-Multiplayer Link Parity

### Change

Commit `1d1c7f4e fix(android): link local multiplayer settings` adds the
existing `../../src/localmultiplayer_settings.cpp` to the Android `main`
shared-library source list in `android/app/CMakeLists.txt`. No gameplay source
was changed. The top-level CMake target already includes this source, and it
defines `BuildLocalMultiplayerOptions`, `BuildLocalMultiplayerSettings`, and
`ApplyLocalMultiplayerVictoriesInput`.

### RED

The initial required APK verification was the accepted failing baseline:

```bash
cd android
./gradlew :app:assembleDebug --no-daemon
```

It reached the native `libmain.so` link and exited 1. The linker reported:

```text
undefined symbol: BuildLocalMultiplayerOptions(...)
undefined symbol: BuildLocalMultiplayerSettings(LocalMultiplayerOptions const&)
undefined symbol: ApplyLocalMultiplayerVictoriesInput(...)
```

The Android target listed `mainmenu.cpp` and `mainmenu_input.cpp`, which
reference these functions, but omitted their existing defining source.

### GREEN

After the one-line target-source correction, the same exact command:

```bash
cd android
./gradlew :app:assembleDebug --no-daemon
```

completed all configured ABI builds, packaged
`app/build/outputs/apk/debug/app-debug.apk`, and exited 0 with:

```text
BUILD SUCCESSFUL in 53s
38 actionable tasks: 16 executed, 22 up-to-date
```

The deployment regression suite was also rerun with:

```bash
cd android
./gradlew :app:testDebugUnitTest \
  --tests org.frozenbubble.AssetDeploymentTest --no-daemon
```

It exited 0 with `BUILD SUCCESSFUL`; its JUnit XML reports:

```text
tests="10" skipped="0" failures="0" errors="0"
```

### Self-Review

- The correction adds exactly the source that owns all three unresolved
  symbols, at the same position used by the top-level core-source list.
- It changes neither local-multiplayer behavior nor the asset-deployment
  implementation.
- `git diff --check` and `git diff --cached --check` were clean before the
  commit. A build-generated deletion of third-party `zconf.h` was restored;
  only `android/app/CMakeLists.txt` was committed.
