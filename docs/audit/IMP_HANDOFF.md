# Improvement-item handoff (IMP-001 .. IMP-024)

Written for a fresh session (any model) picking up the 24 `IMP-###` improvement
items from [FINDINGS.md](FINDINGS.md). All 24 are `confirmed` and, as of
commit `9f3fdc1f`, **0 are fixed**. This doc exists so that work can proceed
without re-reading the whole audit trail.

## What's already done (don't redo)

Three commits landed just before this handoff, on `main`:

- `dde25951` — CI: added a Linux ASan/UBSan job
  (`test-linux-sanitizer` in `.github/workflows/build.yml`) that builds the
  full client+server and runs `ctest`, gating `release`. This is what makes
  BUG-041/042-class leak fixes provable at all (Apple's ASan has no leak
  detection).
- `aca2b403` — fixed BUG-045: `TTFText` is now move-only with real ownership
  transfer instead of a silent no-op copy. This is **half of IMP-007**
  (the copy-semantics half). The other half of IMP-007 — the shared
  `Dispose()`/explicit-destructor singleton pattern that leaves `ptrInstance`
  dangling in `AudioMixer`, `TransitionManager`, `HighscoreManager`,
  `GameSettings` — is still open and is in Batch B below.
- `9f3fdc1f` — fixed BUG-001: `TextureEx` (`src/shaderstuff.h`) now checks
  inputs before dereferencing, destroys the previous surface before
  reassigning, and has a real destructor + deleted copy ops. This is the
  confirmed-defect instance of **IMP-010**; the rest of IMP-010 (consistent
  asset-load-failure handling elsewhere) is still open and is in Batch B.

Read those three commits' full messages (`git show <hash>`) before touching
adjacent code — they explain *why*, not just *what*.

## Explicitly out of scope for this pass

- **IMP-016 through IMP-021** — these are "design and write a new test
  suite / CI job from scratch" tasks (protocol fuzz harness, gameplay-rules
  regression suite, packaged-artifact smoke tests, a dedicated
  transition/texture-lifecycle leak driver, etc.), not mechanical fixes.
  They need judgment about test design and this repo's existing test
  conventions (see `tests/*_test.py`, `tests/*_test.cpp`, and how
  `SKIP_RETURN_CODE 77` is used for sanitizer-only tests in the top-level
  `CMakeLists.txt`). Handle these as their own focused sessions, not folded
  into this batch pass.
- **IMP-008** — needs no action. The audit's own Task 9 closure concluded
  **0** diagnostics of any IMP-008 family have a live path in the current
  tree; everything it could have produced was already redirected into
  IMP-023 and REL-014. Do not reopen it.

That leaves **18 items** across 6 themed batches below.

## Working method

1. Do **one batch at a time**, in order (A through F is a reasonable
   default, but they're independent of each other — pick any order if that
   helps).
2. **One commit per batch** (or per item within a batch, if that's cleaner
   to review — either is fine, just don't mix batches in one commit).
3. **Verify before moving to the next batch:**
   ```bash
   cmake --build build --parallel
   ctest --test-dir build --output-on-failure
   ```
   If `build/` doesn't exist yet: `cmake -B build -G Ninja && cmake --build build --parallel`.
   For anything touching lifetime/ownership (all of Batch B, most of
   Batch A), also rebuild under the sanitizer and re-run:
   ```bash
   cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
     -DCMAKE_C_FLAGS=-fsanitize=address,undefined \
     -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined
   cmake --build build-asan --parallel
   ctest --test-dir build-asan --output-on-failure
   ```
   (Leak detection itself won't work if you're on macOS — `AddressSanitizer:
   detect_leaks is not supported on this platform` — that's expected and not
   a failure; it's exactly why the Linux CI job in `dde25951` exists. UBSan
   and crash/corruption detection still work fine on macOS.)
4. Match existing code style: no new doc comments explaining *what* code
   does; only comment the non-obvious *why*. Don't refactor beyond each
   item's stated scope.
5. Full findings text for every ID below (with exact file:line references
   and reasoning) is in [FINDINGS.md](FINDINGS.md) — search for the ID.
   Deeper subsystem context is in `subsystems/*.md`, linked from each
   finding's `Evidence` column.

---

## Batch A — server/protocol style (low risk, mechanical)

- **IMP-001** — Give the six no-argument server functions strict C
  prototypes in both declarations and definitions (e.g. `void reread()` →
  `void reread(void)`), across `server/*.c`/`server/*.h`.
- **IMP-002** — Make seven server length/index comparisons type-consistent
  (signed/unsigned mismatches flagged by the analyzer; their reviewed bounds
  don't establish an actual defect, this is pure cleanup).
- **IMP-003** — Mark or remove six intentionally-unused GLib
  callback/signal parameters (e.g. `(void)user_data;` or drop the name).
- **IMP-004** — Remove the dead `today` variable and restore the intended
  use of `was_playing` state in `server/game.c` (BUG-005, which this ties
  to, is already fixed — check `git log --oneline -- server/game.c` for
  that prior fix before touching this so the two don't conflict).
- **IMP-011** — Replace the synchronous 50ms accept-time WebSocket sniff
  with event-loop upgrade state (`server/ws.c`/`server/net.c`). This one is
  more of a real design change than the others in this batch — budget more
  care and testing for it specifically; the server test harness in
  `tools/server_tests/` and `tests/server_*_test.py` should catch a
  regression here.

## Batch B — C++ lifetime/correctness (medium risk — verify under ASan)

- **IMP-005** — Default-initialize state-bearing C++ members across the
  client (the render slice's reachable instance was already promoted to
  BUG-043 and fixed separately; this is the remaining sweep for members
  with no reachable use-before-initialization, i.e. genuinely latent ones).
- **IMP-006** — Make bounded gameplay/render numeric conversions and
  integer-division intent explicit (no proven precision defect exists;
  this is clarity/documentation of intent, e.g. explicit casts with a
  one-line comment on why truncation is fine).
- **IMP-007 (remaining half)** — Fix the shared `Dispose()`/explicit-
  destructor singleton pattern that leaves `ptrInstance` dangling in
  `AudioMixer`, `TransitionManager`, `HighscoreManager`, `GameSettings`.
  Not currently tripped by the call order, so this is preventative — check
  each singleton's `Dispose()`/destructor pair and how `ptrInstance` is
  cleared (or isn't).
- **IMP-009** — Simplify redundant branches, dead stores, unused attachment
  helpers, and missing-default control flow (analyzer-flagged, semantic
  review already done — see `subsystems/03-gameplay.md#reload-reset-and-construction`
  for the specific list).
- **IMP-010 (remaining)** — Make asset-load/allocation-failure handling
  consistent: server unchecked/`exit` mixes, render's unchecked
  `precalc_cos`/`precalc_sin`, `plasma.raw`'s `exit(1)`, warn-and-continue
  texture loads elsewhere in `shaderstuff.cpp`/`mainmenu*.cpp`, silent-null
  text paths, and audio's permanent negative caching — each currently
  follows a different policy; pick one (warn-and-degrade, matching the
  BUG-001 fix's pattern) and apply it consistently. This is the biggest
  item in this batch — consider splitting server vs. render/audio into two
  commits.
- **IMP-013** — Fix the off-by-one clamp bounds guarding the pixel helpers:
  `get_pixel`'s own `CLAMP(x,0,s->w)`/`CLAMP(y,0,s->h)` (`shaderstuff.cpp:49`)
  and the identical caller-side clamps feeding `set_pixel`
  (`shaderstuff.cpp:488`, `:1155`, `:1158`) all admit one-past-the-end
  indices; `set_pixel` itself (`shaderstuff.cpp:41-45`) has no clamp at
  all. Also assert the tight-pitch/32-bpp surface contract and bound
  `myLockSurface`'s retry loop.

## Batch C — dead code removal (needs reachability care)

- **IMP-012** — Remove or wire up the menu's unreachable code: the
  two-player panel, the network setup panel, the `editor` button, the
  `LevelEditor`/`Netplay` states, `selectedGameIndex`, `controllerInputs[5]`,
  the diverted gamepad branch, the throwing static settings initializer,
  and `FrozenBubble::menuText` (`TTFText`, zero references outside
  `frozenbubble.h:96`).
- **IMP-015** — Remove the dead platform layer: `platform.cpp`'s
  `ASSET_FILES[]`/`extractAssets()` placeholder (never called —
  `AssetExtractor.java` does the real work), `logger.cpp`'s unused
  `_WIN32` `mkdir` macro and `<direct.h>` include, `shaderstuff.h`'s
  `bzero` macro and `<iconv.h>` include (exist only for a block-commented
  function), `AdsManager.init()` (zero callers), and the `__ANDROID_PORT__`
  branch that's never evaluated in the shipping build.

  **Before deleting anything in this batch: `grep -rn` for every symbol
  first and confirm zero live callers.** "Unreachable" here is the audit's
  claim from static analysis at the time it was written — re-verify against
  current `main`, since the ledger has already noted some findings describe
  state that's since changed.

## Batch D — Android packaging

- **IMP-014** — Exclude duplicate/unused native libraries from the release
  APK: `libpng.so` is byte-identical to `libpng16.so` and shares its
  `SONAME` (so `SDL3_image` can never `dlopen` the former), and
  `libvorbisenc.so` is named by no `DT_NEEDED` entry or `dlopen` string
  anywhere. Together ~2.6MB uncompressed across the three shipped ABIs.
  Exclude via Gradle packaging options (`android/app/build.gradle`) rather
  than deleting source, since these likely come from the SDL3 dependency
  build. Verify with `cd android && ./gradlew assembleRelease` (needs the
  SDL3 submodules initialized per `CLAUDE.md`).

## Batch E — build hygiene

- **IMP-022** — Unify compiler warning flags across build definitions.
  Top-level `CMakeLists.txt` applies `-Wall -Wextra -pedantic
  -Wno-pointer-arith` to native/Windows/WASM and (by directory scope) to
  `server/`, but `android/app/CMakeLists.txt` applies **none** — so the
  NDK toolchain compiles the same translation units, including 32-bit
  `armeabi-v7a` where narrowing/pointer-width diagnostics matter most, with
  warnings off. `docker/Dockerfile`'s server build also loses
  `-Wno-pointer-arith` since it configures `server/` as a top-level
  project. Lift the flag set into a shared interface target or small
  included `.cmake` module, consumed from all three roots.
- **IMP-023** — Constrain the iniparser dependency boundary.
  `CMakeLists.txt`'s `find_package(iniparser QUIET COMPONENTS static)`
  silently prefers any system iniparser over the bundled copy;
  `cmake/Findiniparser.cmake` declares no version and passes no
  `VERSION_VAR`; the bundled sources carry no version marker. Either pass a
  minimum version and match it against the vendored API level, or drop the
  `find_package` call and always build the bundled sources. Record the
  vendored release alongside the licence text (REL-014 already requires
  the licence file itself — don't duplicate that work, just add the
  version marker).

## Batch F — server CREATE room-cap validation

- **IMP-024** — `server/game.c:762` (line number as of the audit; re-check
  current line) reads `if (mp >= 2 && mp <= MAX_PLAYERS_PER_GAME) max_players
  = mp;` with no `else`, so any out-of-range room-size argument to `CREATE`
  silently leaves the legacy default of 5 in place while the server still
  answers `CREATE: OK` — the requester is never told their argument was
  ignored. Not reachable from the shipped client (it only ever sends 5, 10,
  or 20 per `src/mainmenu_internal.h`'s `kRoomSizes[3]`), but the server is
  a public endpoint other clients can speak to. Reject with a warning
  string, or clamp to `MAX_PLAYERS_PER_GAME` and log it — either is
  reasonable, pick whichever matches how other out-of-range server
  arguments are already handled nearby.

---

## Reference

- Full finding text, confidence, and evidence links: [FINDINGS.md](FINDINGS.md)
- Subsystem deep-dives: `subsystems/01-server-protocol.md` through
  `subsystems/09-final-challenge.md`
- Overall defect (not improvement) status: [REMEDIATION_STATUS.md](REMEDIATION_STATUS.md)
- Manual verification gaps for already-fixed defects: [../MANUAL_TEST_CHECKLIST.md](../MANUAL_TEST_CHECKLIST.md)
- Repo build/test commands, architecture notes, release process: `/CLAUDE.md`
