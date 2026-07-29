# 07 — Build, Release, and Tooling Audit Notebook

## Scope

Task 9: builds, tests, packaging, CI, deployment, tools, dependencies, and
operational documentation.

Files reviewed in this gate (read with the agent's file reader unless a shell
command is recorded in the canonical ledger):

- Build definitions: `CMakeLists.txt`, `server/CMakeLists.txt`,
  `CMakeListsEmscripten.txt` (cross-check only; owned by Task 8),
  `android/app/CMakeLists.txt` (cross-check only; owned by Task 8),
  `cmake/Findiniparser.cmake`, `cmake/cmake_uninstall.cmake.in`,
  `cmake/Emscripten.cmake` (cross-check only; owned by Task 8).
- CI/release: `.github/workflows/build.yml` (the only file in
  `.github/workflows/`).
- Android build metadata: `android/build.gradle`, `android/settings.gradle`,
  `android/gradle.properties`, `android/gradle/wrapper/gradle-wrapper.properties`,
  `android/app/build.gradle` (all cross-checks; owned by Task 8).
- Deployment: `docker/Dockerfile`, `docker/docker-compose.yml`,
  `docker/nginx.conf`, `docker/setup.sh`, `docker/ssl/.gitignore`,
  `docker/ssl/fullchain.pem.example`, `docker/ssl/privkey.pem.example`,
  `netlify.toml`, `start-server.sh`.
- Packaging environments: `default.nix`, `shell.nix`, `flake.nix`, `flake.lock`.
- Emscripten port files: `tools/ports/sdl3_image.py`, `tools/ports/sdl3_mixer.py`.
- Tests and tooling (semantics owned by Tasks 3-5; re-examined here only for
  registration, execution, and CI wiring): `tests/net_bots_test.py`,
  `tests/server_list_cap_test.py`, `tests/netview_test.cpp`,
  `tests/netteams_test.cpp`, `tests/roundstats_color_test.cpp`,
  `tools/net_bots.py`, `tools/server_tests/test_room_caps.py`.
- Operational documentation: `README.md`, `SetupServer.md`, plus cross-checks of
  `CLAUDE.md`, `CHANGELOG.md`, `web/README.md`, `WASM_PORT.md`,
  `android/SETUP.md` (the last two owned by Task 8).
- Vendored dependency boundary: `third_party/iniparser/` (four files),
  `COPYING`.

## Trust boundaries and invariants

Boundaries crossed by this subsystem:

- **Tag → artifact.** A `v*.*.*` tag is the only release trigger. Everything a
  user installs is produced from that tag by `.github/workflows/build.yml`.
- **Third-party build inputs → shipped binary.** GitHub Actions, Homebrew,
  MSYS2/pacman, Chocolatey, Emscripten's port registry, `services.gradle.org`,
  `linuxdeploy` `continuous`, and four SDL repositories all inject code into
  artifacts.
- **Repository secret → third-party action.** `secrets.BUTLER_CREDENTIALS` is
  handed to an action resolved from a mutable branch.
- **Vendored source → shipped binary.** `third_party/iniparser` and
  `android/app/jni/iniparser` are statically linked into every artifact.
- **Documentation → operator action.** `README.md`, `SetupServer.md`,
  `docker/setup.sh` and `start-server.sh` are executed by humans against real
  hosts and real TLS material.
- **Test suite → regression protection.** CTest registrations are the only
  automated defence for the invariants earlier gates established.

Invariants this gate tested:

| # | Invariant | Result |
|---|---|---|
| I1 | Every build definition compiles the same effective source set | Holds for native/Android; fails for `CMakeListsEmscripten.txt` (REL-006, Task 8) |
| I2 | Compile definitions, C++ standard, and warning flags agree across build definitions | Fails — warning flags diverge (IMP-022) |
| I3 | One version value flows from the tag into every artifact | Fails — five distinct hard-coded version strings; `versionCode` is a literal (REL-004 extended) |
| I4 | Build inputs are pinned so a tag rebuilds identically | Fails — 0 of 27 action references are commit-pinned, 5 are branch-pinned, Emscripten is `latest` (REL-011) |
| I5 | A step that cannot produce a correct artifact fails the job | Fails — the Windows DLL loop swallows every error (REL-013) |
| I6 | Release artifacts are signed with a stable identity | Fails — per-run Android keystore, ad-hoc unsigned macOS/Windows (REL-007; macOS/Windows documented as known TODOs) |
| I7 | Documented commands work as written | Fails — `SetupServer.md` renewal paths, `README.md` server-list format, `CLAUDE.md` CI section (REL-009, REL-010) |
| I8 | The TLS workflow preserves the operator's real certificate | Fails — reproduced (REL-010) |
| I9 | Registered tests run on every change | Fails — 0 test invocations in CI (IMP-016) |
| I10 | Vendored dependencies carry provenance, version, and licence | Fails — none present (REL-014) |
| I11 | Configuration files parse | Holds — YAML, Compose, CMake, Gradle, and Python all parse (Dynamic evidence) |

## Static review

### Build definition parity (Step 1)

Five build definitions produce four shipped binaries. The effective source sets
were re-derived by extracting `src/*.cpp` tokens from each file and comparing
them as sets (canonical ledger, `2026-07-29T08:0x`), reproducing Task 8's
measurement exactly.

| Aspect | Native (root `CMakeLists.txt`) | Windows (same file, `WIN32 OR MINGW`) | WASM (same file, `EMSCRIPTEN`) | Android (`android/app/CMakeLists.txt`) | Server (`server/CMakeLists.txt`) |
|---|---|---|---|---|---|
| Target / output | `frozen-bubble-sdl3` | `frozen-bubble-sdl3.exe` | `frozen-bubble-sdl3.html` (`SUFFIX ".html"`, `:97`) | `main` → `libmain.so` | `fb-server` |
| `src/*.cpp` count | 28 (27 explicit + `networkclient.cpp`) | 28 + `share/icons/fb.rc` | 29 (adds `networkclient_wasm.cpp`) | 28, set-equal to native | n/a |
| Other sources | — | `RES_FILES` (`:49`) | — | — | 7 `.c`, set-equal to the 7 files on disk |
| C++ standard | `CXX_STANDARD 17`, `CXX_EXTENSIONS OFF` (`:93-94`) | same | same | `CMAKE_CXX_STANDARD 17`, extensions OFF (`:4-5`) plus Gradle `cppFlags "-std=c++17"` | C, no standard set |
| Warning flags | `-Wall -Wextra -pedantic -Wno-pointer-arith` (`:14`) | same | same | **none** | `-Wall -Wextra -pedantic` (`:12`) **plus** the parent directory's four options, because `add_subdirectory(server)` (`:39`) follows `add_compile_options` (`:14`) |
| Compile definitions | `DATA_DIR="${ASSET_PATH}"` (`:140`) | same | `__WASM_PORT__ DATA_DIR="/share"` (`:138`) | `__ANDROID__ DATA_DIR=""` (`:98-101`) — the root file's `__ANDROID_PORT__` branch (`:135`) is never evaluated by the shipping Android build (IMP-015) | `VERSION="2.2.1"` (`:28`) |
| Linked libraries | SDL3, SDL3_image, SDL3_mixer, SDL3_ttf, `iniparser-static` | + `ws2_32` (`:156`) | none found/linked; Emscripten port flags only (`:99-124`) | SDL3 four targets, `iniparser`, `android`, `log`, `GLESv1_CM`, `GLESv2`, `EGL`, `atomic` | `PkgConfig::GLIB2` (+ a dead `ws2_32`/`-Wno-pedantic` block at `:38-42`, unreachable because the root excludes `server/` on `WIN32 OR MINGW OR EMSCRIPTEN` — REL-003) |
| Assets | installed to `share/frozen-bubble`; `DATA_DIR` points at the **source** tree (REL-008) | `cp -r share pkg/` in CI | `--preload-file …/share@/share` (`:121`) | Gradle `assets.srcDirs = ['../../share']` | n/a |
| Tests | 5 CTest registrations (`:160-197`) | same | excluded (`NOT EMSCRIPTEN`) | excluded (`NOT ANDROID`) | contributes `server-list-cap-test` when the `fb-server` target exists |
| Version | `DATA_DIR` only; no version macro | NSIS `VIProductVersion` from the tag; **no `VERSIONINFO` in `share/icons/fb.rc`**, so the `.exe` itself carries no version resource | none | `versionCode 10` / `versionName "2.4.27"` (literals) | `VERSION="2.2.1"` |

Reconciled differences:

- *Intentional:* the WASM extra translation unit, the Windows resource file and
  `ws2_32`, Android's `SHARED` library and Android-only link libraries,
  Android's `DATA_DIR=""` (unreachable — Task 8 proved the `__ANDROID__` arm of
  `InitDataDir` returns first), and the exclusion of tests and `server/` from
  the Android and Emscripten builds.
- *Candidate, promoted:* the Android build applies **no** warning options, so
  the one toolchain most likely to surface portability warnings (NDK Clang, three
  ABIs, 32-bit `armeabi-v7a`) compiles the shared 28-file source set with
  warnings off. The Docker server build also loses `-Wno-pointer-arith`, because
  it configures `server/` as a standalone project. → **IMP-022**.
- *Candidate, promoted:* three build definitions declare version metadata and no
  two agree, and none is derived from the tag. → **REL-004 extension**.
- *Already owned:* `CMakeListsEmscripten.txt`'s 15-of-28 source list and its
  SDL2 port selection are REL-006 (Task 8). This gate confirms the same numbers
  and adds the Nix slice below.

`default.nix` is a fourth stale build definition of the same class as
`CMakeListsEmscripten.txt`. It cannot produce a binary for three independent
reasons: `buildInputs` names `SDL2`, `SDL2.dev`, `SDL2_ttf`, `SDL2_image`,
`SDL2_mixer` (`:28-43`) while `CMakeLists.txt:18-21` calls
`find_package(SDL3 REQUIRED)` and friends; the install phase copies
`build/frozen-bubble-sdl2` (`:57`) which no target emits; and it copies
`../share` (`:56`) from the unpacked source root, whose parent is the Nix build
scratch directory. `flake.nix:3` describes the project as "SDL2 C++ Port",
`default.nix:21-22` sets `pname = "frozen-bubble-sdl2"` and `version = "0.1.0"`,
`default.nix:62` points `homepage` at the upstream `Erizur/frozen-bubble-sdl2`
repository, and `.gitignore:8` still ignores a `/frozen-bubble-sdl2` binary.
→ **REL-006 extension**. Note that Task 8 cited `default.nix:50`'s
`-DASSET_PATH="$out/share"` as a working REL-008 mitigation; the flag is
correct in isolation, but the derivation containing it cannot complete, so it
mitigates nothing in practice. That correction is applied to REL-008's registry
entry.

`shell.nix` and `flake.nix` are thin wrappers over `default.nix` and inherit
its breakage. `flake.lock` pins `nixpkgs` to rev
`1d3aeb5a193b9ff13f63f4d9cc169fb88129f860` on `nixos-24.11` — a correct,
reproducible pin, and the only fully pinned dependency declaration in the
repository.

`cmake/Findiniparser.cmake` was traced against the caller. `CMakeLists.txt:24`
calls `find_package(iniparser QUIET COMPONENTS static)`; because the call is
neither `REQUIRED` nor component-required, `iniparser_FIND_REQUIRED_static` is
unset, the module's required-vars list reduces to `iniparser_INCLUDE_PATH`
(`:18`, `:34-43`), and `HANDLE_COMPONENTS` does not fail on a missing optional
component. A system installation with a header but no static library therefore
sets `iniparser_FOUND` while creating no `iniparser-static` target — and
`CMakeLists.txt:25`'s `if(NOT iniparser_FOUND OR NOT TARGET iniparser-static)`
correctly catches exactly that case and falls back to the bundled sources. The
guard is sound; no defect. What the module does **not** do is constrain the
version: it declares no `iniparser_VERSION`, passes no `VERSION_VAR` to
`find_package_handle_standard_args` (`:51-55`), and the root call passes no
version argument. A system `iniparser` of any vintage silently displaces the
bundled copy. → **IMP-023**.

`cmake/cmake_uninstall.cmake.in` reads `install_manifest.txt` and removes each
entry through `exec_program`. It honours `DESTDIR` and reports a missing
manifest as a fatal error. See Dismissed candidates for the `exec_program`
removal suspicion, which was disproved by running it.

### Dependency and action pinning (Step 2)

Measured on `.github/workflows/build.yml`: **27** `uses:` references across
**9** distinct actions; **0** are commit-pinned (no `@<40-hex>` reference
exists); **5** are branch-pinned to `@master`. Every one of the five is
`josephbmanley/butler-publish-itchio-action@master`, and every one of the five
receives `secrets.BUTLER_CREDENTIALS` in its `env:` block. The remaining 22 use
mutable major-version tags (`actions/checkout@v4` ×5,
`actions/upload-artifact@v4` ×5, `actions/download-artifact@v4` ×6,
`actions/cache@v4` ×2, `actions/setup-java@v4`, `msys2/setup-msys2@v2`,
`mymindstorm/setup-emsdk@v14`, `softprops/action-gh-release@v2`).

Non-action inputs:

| Input | Pin | Reproducible? |
|---|---|---|
| SDL / SDL_image / SDL_mixer / SDL_ttf (Linux) | `release-3.4.4` / `release-3.4.2` / `release-3.2.0` / `release-3.2.2`, `git clone --depth 1 --branch` (`:42-50`) | Tag-pinned; tags are movable but conventionally stable |
| SDL3 MinGW devel archives (Windows) | same four versions, `curl` from GitHub release URLs (`:239-242`) | Tag-pinned, **no checksum verified** |
| SDL3 family (macOS) | `brew install sdl3 sdl3_image sdl3_ttf …` (`:133`) — **no version** | Not reproducible; whatever Homebrew has that day |
| SDL_mixer fallback (macOS) | `release-3.2.0` (`:137`) | Tag-pinned, used only if `brew install sdl3_mixer` fails |
| SDL3 family (Android) | submodule gitlinks `release-3.4.4` / `-3.4.2` / `-3.2.0` / `-3.2.2` | Commit-pinned — the strongest pin in the project |
| SDL3, SDL3_ttf (WASM) | supplied by whichever Emscripten `setup-emsdk` resolves for `version: 'latest'` | Not reproducible |
| SDL3_image (WASM) | `tools/ports/sdl3_image.py:10` `TAG = 'release-3.2.4'` + SHA-512 | Pinned **and** integrity-checked |
| SDL3_mixer (WASM) | `tools/ports/sdl3_mixer.py:8-10` `VERSION = '3.2.0'` + SHA-512 | Pinned and integrity-checked |
| `linuxdeploy` | `releases/download/continuous/…` (`:102`) | Not reproducible — a rolling build |
| NSIS | `choco install nsis -y` (`:316`) | Not reproducible |
| Gradle distribution | `gradle-8.2-all.zip`, no `distributionSha256Sum` | Version-pinned, unverified — REL-007 |
| NDK | `ndk;25.2.9519653` (`:361`) | Version-pinned |
| Base images | `ubuntu:22.04` ×2 in `docker/Dockerfile`, `nginx:alpine` in compose | Mutable tags, no digests |
| `nixpkgs` | `flake.lock` rev `1d3aeb5a…` | Fully pinned |

The SDL3 version matrix that results was re-derived against the actual port
files the Task 8 WASM link consumed (`/tmp/fb-sdl3-audit/task8/emsdk/libexec/tools/ports/`):

| Platform | SDL3 | SDL3_image | SDL3_mixer | SDL3_ttf |
|---|---|---|---|---|
| Linux CI | 3.4.4 | 3.4.2 | 3.2.0 | 3.2.2 |
| Windows CI | 3.4.4 | 3.4.2 | 3.2.0 | 3.2.2 |
| Android | 3.4.4 | 3.4.2 | 3.2.0 | 3.2.2 |
| macOS CI | Homebrew, unpinned (this host: 3.4.10) | Homebrew | Homebrew | Homebrew |
| WASM | **3.4.2** (emsdk port, unpinned by this repo) | **3.2.4** (repo port file) | 3.2.0 | 3.2.2 |

Three of five platforms agree exactly. macOS floats with Homebrew. WASM is two
patch families behind on SDL3 and **two minor families behind on SDL3_image**
(3.2.4 against 3.4.2 everywhere else) because `tools/ports/sdl3_image.py` pins a
tag the rest of the project abandoned. → **REL-011**.

The Emscripten patch procedure (`:441-464`, mirrored verbatim in
`README.md:204-220`) rewrites three SDK files with `sed` and guards two of the
three with `if ! grep -q`. Every one of the three edits is a silent no-op when
its anchor text is absent: `SDL2_MIXER_FORMATS` must exist in `src/settings.js`,
`'SDL2_MIXER_FORMATS'` must exist in `tools/settings.py`, and
`diagnostics.warning('experimental'` must exist in `tools/ports/sdl3.py`. Since
the SDK version is `latest`, upstream is free to rename any of them. The first
two failures surface as a build error (unknown setting); the third surfaces only
as a warning that the workflow does not treat as fatal. This is a maintenance
hazard rather than a present defect and is recorded under REL-011 with the
`latest` pin that causes it.

Positive findings worth recording: the two repo-supplied port files verify their
downloads with SHA-512 hashes; the workflow triggers on `pull_request`, not
`pull_request_target`, so fork PRs never receive `secrets.BUTLER_CREDENTIALS`;
and the four Android submodules are commit-pinned gitlinks.

### Release version, signing, and artifact flow (Step 3)

Tag → artifact trace:

1. `on.push.tags: ['v*.*.*']` (`:6`). Five build jobs run on **every** push to
   `main` and every PR to `main` as well — none carries a tag condition.
2. `release` (`:491-509`) is gated on `startsWith(github.ref, 'refs/tags/')`,
   `needs` all five build jobs, and attaches exactly 5 files.
3. The five `deploy-itchio-*` jobs each depend on **one** build job, so a
   partial failure still publishes the platforms that succeeded to itch.io while
   the GitHub release is withheld. This is a divergence between the two
   distribution channels; it is recorded as an observation because both
   behaviours are defensible and neither produces a wrong artifact.
4. macOS/Windows derive `${GITHUB_REF_NAME#v}` and fall back to the literal
   `2.4.27` off-tag (`:127-128`, `:232-233`). The macOS `Info.plist` receives it
   as `CFBundleVersion` and `CFBundleShortVersionString`; the NSIS script
   receives it as `VERSION` and builds `VIProductVersion "${VERSION}.0"`.
   `VIProductVersion` requires four numeric fields, so any tag that is not
   exactly `vX.Y.Z` (a pre-release such as `v2.5.0-rc1` still matches the
   `v*.*.*` filter) produces an invalid value and fails `makensis`.
5. Android takes no version from the tag at all: `versionCode 10` and
   `versionName "2.4.27"` are literals in `android/app/build.gradle:14-15` and
   nothing in the workflow overrides them (measured: zero `versionCode` or
   `versionName` occurrences in `build.yml`). Two different tagged releases
   therefore produce APKs with the *same* `versionCode`, which Android treats as
   "not an upgrade" independently of REL-007's certificate rotation.
6. WASM carries no version anywhere in its artifact.

Distinct version strings reachable from build inputs: **five** —
`2.2.1` (`server/CMakeLists.txt:28`, printed by `fb-server`'s version banner at
`server/fb-server.c:43` and sent as the master-server user agent at
`server/net.c:1160`), `2.4.9` (`server/net.c:1098`, the startup log line),
`v2.4.26` (`src/platform.h:23`, rendered on the key-configuration panel at
`src/mainmenu_panels.cpp:471`), `2.4.27` (the APK `versionName` and the
macOS/Windows off-tag fallback), and `0.1.0` (`default.nix:22`). Four of the
five reach a runtime or artifact surface; `0.1.0` reaches only a Nix store path
from a derivation that cannot build. → **REL-004 extension**.

Signing and secrets:

- **Android.** `android/app/build.gradle:43-47`'s `release` block declares no
  `signingConfig`; `:app:signingReport` reports `Variant: release / Config: null
  / Store: null / Alias: null`, which is direct confirmation of Task 8's
  `app-release-unsigned.apk` observation. CI supplies a keystore generated fresh
  in the job with the literal password `frozenbubble` appearing four times in
  the public workflow, split across two consecutive steps: `-storepass` at
  `:396` and `-keypass` at `:397` inside "Generate release keystore" (`:390-398`),
  and `KEYSTORE_PASSWORD` at `:404` and `KEY_PASSWORD` at `:406` inside the
  `env:` block of the next step, "Build release APK" (`:400-413`). → REL-007,
  already registered; this gate adds the `signingReport` evidence and corrects
  its reachability (below).
- **macOS.** Ad-hoc `codesign --sign -` over the Frameworks and then
  `--deep` over the bundle (`:186-190`), no notarization, no Developer ID. This
  is **documented**: `README.md:318-326` tells users how to strip quarantine and
  `README.md:339-340` lists signing macOS and Windows as open TODOs. Recorded as
  a known, disclosed limitation rather than a new finding.
- **Windows.** No signing at all; also a disclosed TODO.
- **Secret handling.** One secret (`BUTLER_CREDENTIALS`, 5 references) and two
  repository variables (`ITCHIO_GAME`, `ITCHIO_USERNAME`, 5 references each).
  The secret is never echoed and is confined to `env:` on the five deploy steps.
  Its exposure is entirely the `@master` pin (REL-011). Exactly one job declares
  `permissions:` (`release`, `contents: write`, `:496-497`); the other ten
  inherit the repository default token scope, which this audit cannot read.

Artifact naming, nesting, and permissions:

- Five `upload-artifact` calls produce artifacts `Linux`, `macOS`, `Windows`,
  `Android`, `WASM`. The `release` job's bare `download-artifact` places each in
  a directory named after the artifact, so the five `files:` paths
  (`Linux/frozen-bubble-linux-x86_64.AppImage` etc.) resolve. The WASM artifact
  uploads two paths (`dist-wasm/` and `frozen-bubble-wasm.zip`), so the
  least-common-ancestor rule keeps `dist-wasm/` as a subdirectory; the release
  reference `WASM/frozen-bubble-wasm.zip` and the itch reference
  `artifact-wasm/dist-wasm` are both consistent with that layout. No nesting
  defect found.
- Only the Linux artifact names its architecture. The macOS job runs on
  `macos-latest` and neither `CMakeLists.txt` nor the workflow sets
  `CMAKE_OSX_ARCHITECTURES` or any universal-binary flag (measured: zero
  occurrences of `CMAKE_OSX_ARCHITECTURES` or `universal` in either file), so
  `frozen-bubble-macos.dmg` contains a **single-architecture** binary and
  single-architecture bundled dylibs, matching whatever architecture the runner
  label resolves to, and is published to the generically named itch channel
  `osx`. Macs of the other architecture cannot run it, and nothing in the
  filename, the release notes, or `README.md` says so. → **REL-012**.

Failure propagation:

- Three `|| true` suppressions exist. Two are benign optional copies of
  `fb-server`. The third (`:261-269`) wraps a loop over **21** named DLLs with
  `cp /mingw64/bin/$dll pkg/ 2>/dev/null || true`, so no missing runtime library
  can fail the job. `SDL3.dll`, `libstdc++-6.dll` and `libwinpthread-1.dll` are
  not optional; if any is absent the NSIS installer is still built, uploaded,
  attached to the release, and pushed to itch, and the failure first appears as a
  loader error on a user's machine. There is no post-package verification step
  anywhere in the job — no dependency walk, no smoke launch. → **REL-013**.
- `Cache NDK` (`:342-346`) sets `path: ${{ env.ANDROID_SDK_ROOT }}/ndk/25.2.9519653`.
  The `env` context is populated only from workflow-, job-, and step-level `env:`
  maps; the workflow's single `env:` block (`:14-15`) defines only
  `FORCE_JAVASCRIPT_ACTIONS_TO_NODE24`, and neither the job nor the step defines
  `ANDROID_SDK_ROOT`. The expression therefore interpolates to the empty string
  and the cache path becomes the absolute `/ndk/25.2.9519653`. The very next
  step (`:358-361`) reads the same variable as `$ANDROID_SDK_ROOT` inside a
  `run:` shell, where the runner image *does* define it — the two adjacent
  spellings of one variable are what make the mistake legible. Consequence: the
  NDK cache never hits, and every Android run re-downloads the NDK. No
  correctness impact. → **REL-013**.

### Test coverage against discovered risks (Step 5)

Registered tests, measured from a fresh configure (`ctest -N` reports
**Total Tests: 5**):

| Test | Kind | Sources | Covers |
|---|---|---|---|
| `netview-test` | C++ | `tests/netview_test.cpp`, `src/netview.cpp` | opponent view ranking and paging |
| `netteams-test` | C++ | `tests/netteams_test.cpp`, `src/netteams.cpp` | team assignment/override, slots 0-19 |
| `roundstats-color-test` | C++ | `tests/roundstats_color_test.cpp`, `src/roundstats_color.cpp` | round-stats colour selection |
| `net-bots-test` | Python | `tests/net_bots_test.py` | bot framing/buffering unit checks |
| `server-list-cap-test` | Python | `tests/server_list_cap_test.py` | room-cap enforcement; **REL-002** invalidates its process ownership |

`tools/server_tests/test_room_caps.py` is a second, unregistered room-cap
harness that also binds a fixed port (REL-002).

**No CI job runs any of them.** Measured: zero occurrences of `ctest`,
`BUILD_TESTING`, `--target test`, `gradlew test`, or `pytest` across
`.github/workflows/build.yml`, which is the only file in `.github/workflows/`.
Compilation success is the entire automated gate for eleven jobs. → **IMP-016**.

Every confirmed defect from Tasks 3-9 mapped to its covering mechanism. The
table has **32** rows and each ID appears in the IDs column of exactly one row;
**68** IDs are mapped (47 confirmed `BUG`, 7 `SEC`, 14 `REL`; `BUG-012` is
dismissed and excluded). Later columns may name an ID again when the evidence
sentence needs it.

| Invariant class | IDs | Existing test | Audit dynamic case | Gap → owner |
|---|---|---|---|---|
| Server room/seat lifecycle, win accounting, admission caps | BUG-003, BUG-004, BUG-005, BUG-008, BUG-009, BUG-010, BUG-011 | `server-list-cap-test` touches the cap for BUG-004 only, and REL-002 makes even that untrustworthy | none — Task 3 was static by user direction | **IMP-019** |
| Server protocol framing and untrusted length/index handling | BUG-002, BUG-006, BUG-007, SEC-002, SEC-005, SEC-006 | none | none | **IMP-019** |
| Server identity binding | SEC-001, SEC-004 | none | none | **IMP-019** (non-hostile shape checks only; hostile traffic stays out of scope) |
| Client connection lifecycle, command correlation, stream semantics | BUG-013, BUG-014, BUG-015, BUG-016, BUG-017 | `net-bots-test` covers framing only | none — no socket traffic was generated | **IMP-019** |
| Peer-supplied coordinates and options reaching indexing | SEC-003, SEC-007 | none | none | **IMP-019** (bounds assertions on the parse boundary, no hostile traffic) |
| Round outcome, departure, victory-limit, clear-mode rules | BUG-018, BUG-019, BUG-020, BUG-021, BUG-022, BUG-023, BUG-024 | `netteams-test`, `netview-test` cover adjacent helpers only | Task 5's production-object harness reproduced BUG-018 and BUG-019 | **IMP-018** (promote the harness into CTest) |
| Collision sampling at maximum delta | BUG-025 | none | Task 5 Fix Round 1 reproduced BUG-025 with linked production objects | **IMP-018** |
| Settings persistence, parsing, clamping, diagnostics | BUG-026, BUG-027, BUG-028, BUG-029, BUG-030, BUG-031 | none | all six reproduced in Task 6's isolated-preference matrix | **IMP-017** |
| Highscore/level-history parsing and construction ordering | BUG-032, BUG-034 | none | both reproduced in Task 6 | **IMP-017** |
| Local-server process control | BUG-033 | none | deliberately not reproduced (kills host processes) | **IMP-017** (inject the process-control call) |
| Controller slot bounds and scancode stride | BUG-035, BUG-036 | `netteams-test` covers slots 0-19 for teams only | not reproducible — needs 6-11 physical hot-plugs | **IMP-017** |
| Lobby room-scoped state reset and join targeting | BUG-037, BUG-038 | none | none — no server was started | **IMP-019** |
| Player-count/UI reachability limits | BUG-039, BUG-040 | none | none | **IMP-017** |
| Texture, surface, and font resource lifetime | BUG-001, BUG-041, BUG-042, BUG-045 | none | BUG-001/041/045 reproduced against production objects; BUG-042 is a static proof | **IMP-021** (needs LeakSanitizer, unavailable on this host) |
| Unchecked asset loads and never-initialized text objects | BUG-043, BUG-044, IMP-013's bounds | none | both reproduced under UBSan/ASan | **IMP-021** |
| Android asset extraction idempotence and refresh | BUG-046 | none | none — no device or emulator | **IMP-020** |
| Log path resolution and initialization failure | BUG-047 | none | reproduced against the production `logger.cpp` object | **IMP-020** |
| WASM preference persistence | BUG-048 | none | artifact analysis only; no browser runtime | **IMP-020** |
| Diagnostic format correctness | REL-001 | none | none | **IMP-019** |
| Test-harness process/port ownership | REL-002 | the affected test *is* `server-list-cap-test` | Task 2's dynamic-port foreground substitution | **IMP-016** (isolate the port and own the child) |
| Windows socket typing and nonblocking mode | REL-003 | none | none — no Windows host | **IMP-020** (a Windows CI job that at least links and smoke-runs) |
| Version metadata coherence | REL-004 | none | re-derived statically this gate | **IMP-016** (a single tag→artifact version assertion) |
| Tracked dangling symlinks | REL-005 | none | `git ls-files -s` measured 97 mode-`120000` entries | **IMP-016** (a repository-hygiene check) |
| Stale build files and contradicting documentation | REL-006 | none | Task 8 ran the documented configure; this gate read the Nix slice | **IMP-016** (build every declared build file, or delete it) |
| Release signing identity | REL-007 | none | `signingReport` this gate; Task 8's unsigned APK | **IMP-020** |
| Installed-layout asset resolution | REL-008 | none | reproduced against the production `platform.cpp` object | **IMP-020** |
| Documentation matching the shipped system | REL-009 | none | re-derived this gate against the workflow, `CHANGELOG.md`, and `curlFetch` | **IMP-016** |
| Operator TLS material preservation | REL-010 | none | reproduced this gate (`openssl rsa -check` on an EC key, exit 1 on two OpenSSL implementations) | **IMP-020** |
| Build-input pinning and cross-platform version agreement | REL-011 | none | port-file tags re-derived from the emsdk copy that produced the artifact | **IMP-016** |
| Artifact architecture coverage | REL-012 | none | static — no macOS CI run available | **IMP-020** |
| CI steps that cannot fail or take effect | REL-013 | none | static | **IMP-016** |
| Vendored dependency provenance and licence | REL-014 | none | measured this gate — one `COPYING`, no iniparser notice | **IMP-016** |

The eight registered improvements (**IMP-016** through **IMP-023**) are
specified in Confirmed findings with location, inputs, assertions, and platform
matrix.

### Operational documentation against actual commands (Step 6)

| Document | Command / claim | Verification | Result |
|---|---|---|---|
| `README.md:180-183` | `cmake -B build -G Ninja` then `cmake --build build --parallel` | Configured the identical command shape into an ignored tree; exit 0, 5 tests registered | Correct |
| `README.md:188` | `./build/frozen-bubble-sdl3` | Path matches the target name and default build dir | Correct |
| `README.md:191` | "server binary is built automatically on Linux and macOS" | `CMakeLists.txt:38` excludes `server/` only on `WIN32 OR MINGW OR EMSCRIPTEN`; the configure log shows `fb-server`'s GLib check running | Correct |
| `README.md:204-227` | Emscripten patch + build | Byte-for-byte the workflow's `:441-470` procedure; Task 8 executed an equivalent patched build successfully | Correct, with REL-011's brittleness |
| `README.md:229-241` | COOP/COEP serving snippet | Serves the CWD, which after step 3 is `build-wasm`, and opens `frozen-bubble-sdl3.html`, which the `SUFFIX ".html"` property emits. `open` is macOS-only | Correct on macOS; the launcher is platform-specific |
| `README.md:274-279` | Public-server-list format `myserver.example.com 1511` | The GitHub list is fetched with `originalFormat=false` (`src/networkclient.cpp:1802`), whose branch splits on the **last colon** of the first token; a space-separated entry yields `host="myserver.example.com"`, `port` left at the default 1511, and the remainder (`"1511"`) used as the display **name** | **Wrong** — REL-009 |
| `SetupServer.md:120-125` | Copy certs from repo root into `docker/ssl/` | CWD after `:98` is the repo root | Correct |
| `SetupServer.md:129-134` | `cd docker && ./setup.sh -d` | `setup.sh:16` re-anchors to its own directory and `:49` runs `docker compose up --build -d` | Correct |
| `SetupServer.md:166-176` | Renewal block | `cd docker` first, then `sudo cp … docker/ssl/fullchain.pem` — that path does not exist from inside `docker/`; both copies fail and `./setup.sh -d` restarts on the unchanged (now expired) certificate | **Wrong** — REL-010 |
| `SetupServer.md:145-151` | Native 1511 / browser 443 | Matches `docker-compose.yml:12,22` and `nginx.conf:18,32` | Correct |
| `SetupServer.md:208-212` | `./build/server/fb-server -q -l -z` | Matches the CMake output path and the flags `start-server.sh:68` uses | Correct |
| `start-server.sh` | `-p/-d/-h`, `cd …/build/server` | Argument parsing, error path, and binary path all check out against `README.md:247-251`; `-h` output matches the implemented flags | Correct |
| `docker/setup.sh:22-23` | `cert_ok`/`key_ok` gate | `openssl rsa -in <EC key> -check -noout` exits 1 on LibreSSL 3.3 ("expecting an rsa key") and on OpenSSL 3.6.3 ("Not an RSA key"). certbot has defaulted to ECDSA keys since 2.0, so the documented Step 5/Step 6 flow lands an EC key that this gate rejects, and `:37-40` then overwrites **both** `$CERT` and `$KEY` with a self-signed RSA pair | **Wrong, reproduced** — REL-010 |
| `docker/ssl/.gitignore` | `*.pem` ignores real material, `*.pem.example` stays tracked | Verified: the rule cannot match the two `.example` files | Correct — good practice |
| `CLAUDE.md:88-92` | "Linux, macOS, Windows, and Android build jobs … currently disabled with `if: false` … only the WASM build and deploy run … the `release` job currently needs/packages only the WASM zip" | Measured on the pinned baseline: **0** of **11** jobs carry `if: false`; `release` `needs` all five build jobs and attaches **5** files. `CHANGELOG.md`'s `v2.4.27` entry states the opposite of `CLAUDE.md` in plain words — "All release builds restored — Linux AppImage, macOS DMG, Windows installer, Android APK, and WebAssembly packages are built, attached to tagged GitHub releases, and deployed to their Itch.io channels" | **Wrong** — REL-009, and it invalidates a premise Task 8 relied on (see Confirmed findings) |
| `netlify.toml` | COOP/COEP for `/*` | Header values match `README.md:235-236` exactly | Correct (see Dismissed candidates for the publish-directory question) |
| `docker/Dockerfile:24` | `EXPOSE 1511` | The image's `CMD` passes `-l`, which the server documents as UDP LAN discovery, but neither `EXPOSE` nor `docker-compose.yml:12` maps UDP. LAN discovery cannot reach a containerised server | Recorded as an observation: the deployment is explicitly for a public host reached by name, where UDP broadcast is inapplicable |

### Analyzer triage

Inherited unchanged from Task 2; the anchor is referenced by earlier ledger
rows. Every unique project-owned diagnostic is accounted for below. Counts are
deduplicated by path, line, column, message, and check ID. Candidate IDs group
related evidence; they do not assert that every member of a checker family is a
defect.

#### Cppcheck: 496 project diagnostics

| Count | Check IDs | Disposition |
|---:|---|---|
| 169 | `constParameterPointer`, `constParameterReference`, `constVariable`, `constVariablePointer`, `constVariableReference`, `passedByValue`, `returnByReference`, `useStlAlgorithm`, `variableScope`, `functionStatic`, `noExplicitConstructor` | Selective low-priority API/constness work under IMP-008; no behavior failure shown. |
| 181 | `cstyleCast`, `dangerousTypeCast`, `shadowMember`, `shadowVariable` | Selective cast/shadow cleanup under IMP-008; the analyzer supplies no invalid runtime value. |
| 1 | `suspiciousFloatingPointCast` | Numeric-intent review under IMP-006. |
| 74 | `uninitMemberVar`, `uninitMemberVarNoCtor`, `uninitMemberVarPrivate` | IMP-005. Existing constructors/setup paths and the green sanitizer suite prevent bulk defect promotion; assigned gates must prove construction-before-use. |
| 57 | `knownConditionTrueFalse`, `duplicateCondition`, `identicalConditionAfterEarlyExit`, `redundantAssignment`, `redundantCondition`, `redundantContinue`, `redundantInitialization`, `unreadVariable`, `uselessCallsSubstr` | IMP-004 covers the two server dead locals; IMP-009 covers the remaining simplification candidates. The repeated `currentGame` render blocks are independent, not a logic duplicate. |
| 6 | `danglingTemporaryLifetime`, `noOperatorEq`, `operatorEqVarError` | Two temporary reports dismissed by C++ const-reference lifetime extension; `FrozenBubble` is a non-copied singleton; three `TTFText` member reports become IMP-007. |
| 6 | `nullPointerOutOfMemory`, `nullPointerRedundantCheck` | Direct `TextureEx` null-ordering evidence becomes BUG-001; raw allocation/failure consistency becomes IMP-010. |
| 2 | `invalidPrintfArgType_sint` | REL-001. |

#### clang-tidy: 547 project diagnostics

| Count | Check IDs | Disposition |
|---:|---|---|
| 27 | `clang-diagnostic-sign-compare`, `clang-diagnostic-strict-prototypes`, `clang-diagnostic-unused-parameter`, `clang-diagnostic-unused-variable` | Exact compiler-location inventory already retained as IMP-001 through IMP-004. |
| 268 | `bugprone-implicit-widening-of-multiplication-result`, `bugprone-integer-division`, `bugprone-misplaced-widening-cast`, `bugprone-narrowing-conversions` | IMP-006; most are legacy render/gameplay arithmetic, so semantic gates must distinguish intended truncation from defects. |
| 121 | `bugprone-assignment-in-if-condition`, `bugprone-branch-clone`, `bugprone-easily-swappable-parameters`, `bugprone-macro-parentheses`, `bugprone-reserved-identifier`, `bugprone-suspicious-string-compare`, `bugprone-switch-missing-default-case`, `bugprone-throwing-static-initialization`, `bugprone-unhandled-self-assignment`, `bugprone-unsafe-functions`, `bugprone-unused-return-value`, `clang-analyzer-optin.performance.Padding`, `clang-analyzer-security.insecureAPI.bzero`, `performance-enum-size`, `performance-move-const-arg`, `performance-unnecessary-value-param`, `portability-avoid-pragma-once` | IMP-007 through IMP-009 where applicable. Truth-valued string compares, bounded nonblocking `connect` calls, and supported `#pragma once` are dismissed; no raw promotion. |
| 27 | `bugprone-unchecked-string-to-number-conversion` | Fifteen peer-facing sync/game sites contribute to SEC-003; twelve server-list, geolocation, stats-file, and UI parsing sites remain validation improvements under IMP-008. |
| 16 | `bugprone-signal-handler` | BUG-002. |
| 5 | `clang-analyzer-core.CallAndMessage` | Four null-current-game paths are infeasible because the only call is guarded by `currentGame && isHost`; SDL fills all 16 alpha values in the fifth path. Dismissed with source evidence. |
| 1 | `clang-analyzer-cplusplus.NewDeleteLeaks` | BUG-001. |
| 10 | `clang-analyzer-deadcode.DeadStores` | IMP-004 and IMP-009. |
| 20 | `clang-analyzer-optin.core.EnumCastOutOfRange` | Dismissed: values 300+ intentionally implement the documented virtual-scancode namespace and are never sent to SDL as physical enum values. |
| 3 | `clang-analyzer-security.ArrayBound` | SEC-002. |
| 30 | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling` | Dismissed over-broad macOS Annex K suggestions; concrete bounds issues are separately retained. |
| 2 | `clang-analyzer-security.insecureAPI.UncheckedReturn` | SEC-001. |
| 13 | `bugprone-random-generator-seed`, `clang-analyzer-security.insecureAPI.rand` | Dismissed as non-cryptographic cosmetic/gameplay randomness; no security decision uses these values. |
| 1 | `clang-analyzer-security.insecureAPI.strcpy` | Dismissed: `strcat` is preceded by an exact combined-length check against the destination array. |
| 3 | `bugprone-command-processor` | Dismissed as injection reports: `system` uses a fixed literal and both `popen` paths receive compile-time URL literals. Process ownership/return handling remains eligible for Task 6/9 review — Task 6 closed it as BUG-033. |

#### IMP-008 closure

IMP-008 was the last analyzer candidate still open, recorded as having "only the
Task 9 files left". That description is now resolved against the evidence. Task
9's file set is CMake, YAML, shell, Python, Nix, TOML, and Markdown; neither
cppcheck nor clang-tidy analyses any of those, and the retained per-file
diagnostic inventory contains **zero** diagnostics whose path is one of Task 9's
21 files. The IMP-008 families' remaining sites all live in files that closed
gates own — the twelve non-`SEC-003`
`bugprone-unchecked-string-to-number-conversion` sites are in
`src/networkclient.cpp` (Task 4), `src/bubblegame_net.cpp` (Task 5),
`src/mainmenu_netpanel.cpp` and `src/mainmenu_input.cpp` (Task 6),
`src/socket_compat.h`, `src/logger.h`, `src/roundstats_color.h`,
`server/stats.c`, `server/ws.h` and `server/win32_compat.h` (Tasks 3-8) — and
each of those notebooks dispositioned its slice.

What Task 9 does own is the boundary Task 2 deferred: the vendored dependency.
`third_party/iniparser` contributed 11 cppcheck and 51 clang-tidy diagnostics
that were excluded from project findings, including the two macOS `sprintf`
deprecation warnings at `iniparser.c:333,874`. Reviewing that boundary produced
two outcomes: the version/preference gap (**IMP-023**) and the provenance/licence
gap (**REL-014**). With those registered, IMP-008 has no open slice and moves to
`confirmed` as an improvement, with no defect promoted from any of its families.

## Dynamic evidence

Task 2 cross-gate baselines (retained unchanged):

- The clean native Release configure/build completed. CTest reported all five
  registered tests passing, but REL-002 later invalidated ownership of the
  server-list test's fixed-port daemon. The other four tests passed again in a
  fresh isolated CTest run; the unchanged server-list assertions passed in a
  supplemental dynamic-port/foreground run that verified the Release child was
  still alive after readiness. The warnings-strict result and server candidates
  are recorded in [01-server-protocol.md](01-server-protocol.md).
- The ASan+UBSan Debug configure/build completed. Apple ASan aborted all three
  instrumented C++ tests when the required `detect_leaks=1` option reported
  `detect_leaks is not supported on this platform`. CTest marked both Python
  rows Passed, but only net-bots is accepted; server-list retains REL-002's
  process-ownership limitation. The retained-coverage rerun with
  `detect_leaks=0`, `halt_on_error=1`, and UBSan stack traces initially reported
  5/5 tests passing, subject to REL-002. Fresh verification passed the four
  unaffected tests plus the isolated foreground server-list assertions against
  the sanitizer binary, with no ASan or UBSan diagnostic.
- The sanitizer build also emitted two macOS `sprintf` deprecation warnings at
  `third_party/iniparser/iniparser.c:333,874`. The configure explicitly selected
  this bundled dependency because no system iniparser was found. They are
  vendored dependency noise, and the boundary review they were deferred to is
  now complete (IMP-023, REL-014).
- The compile-database configure succeeded. Cppcheck 2.21.0 checked 43 compile
  commands and exited 0 as configured, writing 2,160 lines / 147,118 bytes to
  `/tmp/fb-sdl3-audit/cppcheck.txt` (SHA-256
  `0eb042110d1cd1576a14abb4135cb679426136db99fe3bd22368d6f159ee6eae`).
  Deduplication found 507 diagnostics: 496 project-owned across 34 check IDs
  and 11 vendored iniparser diagnostics.
- The brief's exact clang-tidy helper command exited 1 because keg-only
  `clang-tidy` was absent from `PATH`; its 86-byte log is retained as
  `clang-tidy-initial.txt`. An explicit-binary retry exited 1 because LLVM 22
  enables no checks without a repository configuration; its 45-byte log is
  `clang-tidy-no-checks.txt`. A broad-check retry then reached project code but
  exited 1 because Homebrew LLVM lacked the Xcode SDK sysroot; its 448,449-byte
  partial log is `clang-tidy-no-sysroot.txt`.
- The final clang-tidy inventory invocation explicitly supplied the binary,
  check families, project header filter, and Xcode 26.5 SDK sysroot. It exited 0
  and wrote 11,670 lines / 1,057,186 bytes to
  `/tmp/fb-sdl3-audit/clang-tidy.txt` (SHA-256
  `973f9d2e6e8242695be3151d2b7aa031e9d6f3480c0088d6edfd2618f8dfd4bd`).
  Deduplication found 598 diagnostics: 547 project-owned across 38 check IDs
  and 51 vendored iniparser diagnostics.
- A deterministic single-worker replay with an absolute project header filter
  exited 0 and wrote 11,670 lines / 1,057,185 bytes to
  `/tmp/fb-sdl3-audit/clang-tidy-repro.txt` (SHA-256
  `faf6915e1bb6cfe54176d92550de24f05fae926f8d201603ff7ef64dade29beb`).
  Its raw ordering/summary byte differed, but its 598 deduplicated diagnostics
  and all 547 project-owned diagnostic records match the primary inventory
  exactly.

Task 9 validators and measurements (Step 4; full rows in the canonical ledger):

- **Workflow YAML.** The brief's exact command
  `ruby -e 'require "yaml"; YAML.load_file(".github/workflows/build.yml", aliases: true)'`
  exited **1** with `unknown keyword: aliases (ArgumentError)`. The cause is the
  host toolchain, not the file: macOS ships Ruby 2.6.10, whose Psych
  `load_file` predates the `aliases:` keyword. The accepted substitute
  `ruby -e 'require "yaml"; YAML.load_file(…)'` exited **0**; the document
  contains no YAML anchors, so the keyword was immaterial. A second ruby program
  parsed the document and reported `total_jobs=11 jobs_with_if_false=0` and
  `release_files=5`. `python3 -c "import yaml"` was **not** available
  (`ModuleNotFoundError: No module named 'yaml'`), recorded as a limitation on
  cross-parser confirmation.
- **Compose.** `docker compose -f docker/docker-compose.yml config` exited
  **0** and emitted the fully resolved model — two services, three read-only
  bind mounts, ports 1511/80/443, `depends_on: service_started`. The Docker
  daemon was not required and no container was created; this is a local parse
  only, and **no `docker compose up` was run**.
- **CMake.** `cmake -S . -B build-audit-config -G Ninja -DCMAKE_BUILD_TYPE=Release`
  exited **0** into a directory added to the untracked `.git/info/exclude`
  *before* creation, so it can never appear as drift. The log records
  `iniparser system package not found, building from bundled source`,
  `Found glib-2.0, version 2.88.2`, `Asset path is: /Users/dchau/gr/frozen-bubble-sdl3/share`,
  and `Installed assets will be at: /usr/local/share/frozen-bubble` — the last
  being REL-008's misleading message about a variable nothing consumes.
  `ctest -N` in that tree reported `Total Tests: 5`.
- **Gradle.** The brief's exact command `./android/gradlew tasks --all --no-daemon`
  exited **1**: `Directory '/Users/dchau/gr/frozen-bubble-sdl3' does not contain
  a Gradle build.` The wrapper resolves its own installation from the script
  path but takes the project directory from the CWD, and the repository root has
  no `settings.gradle`. The accepted substitute
  `./android/gradlew --project-dir android tasks --all --no-daemon` exited
  **0** and listed 329 task lines including `app:assembleRelease`,
  `app:bundleRelease`, `app:lintVitalRelease`, `app:testDebugUnitTest` and
  `app:testReleaseUnitTest`. A supplementary read-only
  `:app:signingReport` exited 0 and reported `Variant: release / Config: null /
  Store: null / Alias: null`.
  **Drift control:** `git status --short` (0 lines) and a SHA-256 manifest of all
  **134** tracked `android/` paths plus the SHA-256 of `git ls-files -s android`
  were captured before the first Gradle invocation and re-derived after the
  last. Both diffs were empty and the index hash was identical
  (`e7f56a3342a1e48c27c88386e7fa8763f509d14eafdc4f5adc83c57f25ed3b74` before and
  after). No path was restored because none was modified.
- **Python.** `python3 -m py_compile tools/net_bots.py tests/*.py tools/server_tests/*.py`
  exited **0**. The two Emscripten port files were compiled separately and also
  exited **0**.
- **REL-010 reproduction.** `openssl ecparam -name prime256v1 -genkey -noout -out
  ec_privkey.pem` (exit 0) followed by `openssl rsa -in ec_privkey.pem -check
  -noout` exited **1** under the system LibreSSL 3.3 (`expecting an rsa key`)
  and again exited **1** under Homebrew OpenSSL 3.6.3 (`Not an RSA key`). This
  is `docker/setup.sh:23`'s `key_ok` predicate verbatim, so the script's
  regenerate branch is taken for any ECDSA certbot key. No certificate,
  container, listener, or server was created; the probe files live under
  `/tmp/fb-sdl3-audit/task9/`.
- **`exec_program` probe.** `cmake -P` on a two-line script calling
  `exec_program` exited **0** on CMake 4.3.4 with only a `CMP0153` developer
  warning, disproving the suspicion recorded under Dismissed candidates.
- **Counting commands.** Action pinning (27 `uses:`, 0 SHA-pinned, 5 `@master`),
  secret/variable references (5/5/5), `|| true` occurrences (3), DLL names in
  the Windows copy loop (20), test invocations in CI (0), workflow files (1),
  source-set parity (28/28/29/15/7), and the SDL3 version matrix were each
  measured with a command that counts the thing claimed, not a first match. See
  the canonical ledger.

## Candidates

No Task 9 candidate remains open. Every candidate this gate raised reached a
terminal state:

- Promoted to new IDs: REL-009, REL-010, REL-011, REL-012, REL-013, REL-014,
  IMP-016, IMP-017, IMP-018, IMP-019, IMP-020, IMP-021, IMP-022, IMP-023.
- Recorded by extending an existing entry rather than allocating a new ID:
  REL-004 (two further version strings, the static `versionCode`, and the
  missing Windows `VERSIONINFO`), REL-006 (the Nix slice), REL-007 (the
  `signingReport` evidence and the corrected reachability), REL-008 (the
  `default.nix` mitigation qualified).
- Closed without promotion: IMP-008 (see Analyzer triage), REL-002's remediation
  design (recorded under Confirmed findings; the defect itself remains as
  registered by Task 3).
- Dismissed with consequence-tracing counter-evidence: four candidates, below.

## Confirmed findings

### New defects

**REL-009 — Low. Maintained documentation contradicts the shipped system.**
Two independent instances, both measured against the pinned baseline.
*(a)* `CLAUDE.md`'s CI section states that the Linux, macOS, Windows and Android
build jobs and their itch.io deploy jobs "are currently disabled with `if: false`
to cut Actions usage — only the WASM build and deploy run", and that "the
`release` job currently needs/packages only the WASM zip". The workflow contains
**0** jobs with `if: false` out of **11**; `release` `needs` all five build jobs
and attaches **5** files; and `CHANGELOG.md`'s `v2.4.27` entry records the
restoration in the opposite words. The consequence is not cosmetic: a maintainer
or reviewer reading `CLAUDE.md` concludes that four platforms are not being
shipped and will under-weight every defect that reaches them — which is exactly
what happened inside this audit (see the Task 8 correction below).
*(b)* `README.md:274-279` documents the public-server-list submission format as
`myserver.example.com 1511`, but the GitHub list is parsed with
`originalFormat=false` (`src/networkclient.cpp:1802`), whose branch takes the
first whitespace-delimited token, splits it on its **last colon**, and treats the
remainder of the line as a display name. A submission in the documented format
yields `host="myserver.example.com"`, `port` left at the default `1511`, and the
display name `"1511"`; a submission with any other port is silently connected to
1511. The correct format is `host:port  Display Name`, which `SetupServer.md:189-192`
documents correctly — so the two user-facing documents disagree with each other.

**REL-010 — Medium. The documented TLS certificate workflow discards the
operator's real certificate.** Two compounding halves.
*(a)* `docker/setup.sh:22-23` gates on `cert_ok`/`key_ok`, where
`key_ok() { openssl rsa -in "$KEY" -check -noout 2>/dev/null; }`. `openssl rsa`
accepts only RSA keys, and certbot has issued ECDSA keys by default since
version 2.0 — the exact tool `SetupServer.md:108-109` instructs the operator to
run. Reproduced: `openssl rsa -check -noout` on a `prime256v1` key exits **1**
under LibreSSL 3.3 and under OpenSSL 3.6.3. The `else` branch at `:37-40` then
runs `openssl req -x509 -newkey rsa:2048 … -keyout "$KEY" -out "$CERT"`, which
**truncates and overwrites both** the certificate and the private key the
operator just copied in at `SetupServer.md:123-124`. The stack comes up serving
a self-signed `CN=localhost` certificate; every browser/WASM client is rejected,
and the script's own message says so without ever revealing that a valid
certificate was destroyed. The `/etc/letsencrypt` originals survive, so this is
recoverable, which is why it is Medium and not High.
*(b)* `SetupServer.md:166-176`'s renewal block runs `cd docker` and then
`sudo cp /etc/letsencrypt/… docker/ssl/fullchain.pem`. From inside `docker/`
that destination directory does not exist, so both copies fail; the operator
then runs `./setup.sh -d`, which finds the *old* files, and — if they are RSA
and merely expired, since `cert_ok` checks parseability only and never expiry —
accepts them and restarts the stack on an expired certificate.

**REL-011 — Medium. Build inputs are unpinned and the SDL3 versions diverge
across platforms.** Measured: **0** of **27** `uses:` references are pinned to a
commit; **5** are pinned to the mutable branch `@master`, and all five are
`josephbmanley/butler-publish-itchio-action`, each receiving
`secrets.BUTLER_CREDENTIALS`, so a force-push or account takeover on that
repository's default branch executes attacker-chosen code in a job holding the
itch.io deployment credential. Beyond actions: `mymindstorm/setup-emsdk@v14`
requests `version: 'latest'`, so the SDL3 and SDL3_ttf versions inside the WASM
artifact are decided at build time and are not recorded anywhere;
`linuxdeploy-x86_64.AppImage` is fetched from the rolling `continuous` release;
NSIS comes from an unversioned `choco install`; the macOS SDL3 stack comes from
unversioned `brew install`; the four SDL3 MinGW archives are downloaded without
any checksum; and `docker/Dockerfile` and `docker-compose.yml` use mutable
`ubuntu:22.04` / `nginx:alpine` tags. The measurable consequence today is a
version matrix in which only Linux, Windows and Android agree
(SDL3 3.4.4 / image 3.4.2 / mixer 3.2.0 / ttf 3.2.2): the WASM build ships SDL3
**3.4.2** and SDL3_image **3.2.4** — the latter two minor families behind every
other platform, pinned by `tools/ports/sdl3_image.py:10` — and macOS ships
whatever Homebrew currently has (3.4.10 on the audit host). A related
maintenance hazard sits in the same job: all three `sed` edits that install the
port files are silent no-ops if upstream renames their anchor text
(`SDL2_MIXER_FORMATS` in `src/settings.js`, `'SDL2_MIXER_FORMATS'` in
`tools/settings.py`, `diagnostics.warning('experimental'` in
`tools/ports/sdl3.py`), and `version: 'latest'` guarantees that upstream will
change under the workflow eventually.

**REL-012 — Medium. The macOS DMG is single-architecture and says nothing about
it.** Neither `CMakeLists.txt` nor `.github/workflows/build.yml` contains any
occurrence of `CMAKE_OSX_ARCHITECTURES`, `-arch`, or a universal-binary
configuration (measured: zero matches). `build-macos` therefore compiles for the
`macos-latest` runner's own architecture, `dylibbundler` (`:163-168`) copies
Homebrew dylibs of that same architecture into `Contents/Frameworks`, and the
result is published as `frozen-bubble-macos.dmg` and to the itch.io channel
`osx`. Neither name carries an architecture, `README.md:6` advertises plain
"macOS", and no release note qualifies it. Macs of the other architecture
download an app that cannot launch. Contrast the Linux job, which correctly
names `frozen-bubble-linux-x86_64.AppImage`.

**REL-013 — Medium. Two CI steps cannot signal failure or cannot take effect.**
*(a)* The Windows packaging loop (`:261-269`) iterates **21** DLL names and
copies each with `cp /mingw64/bin/$dll pkg/ 2>/dev/null || true`. Genuinely
optional libraries motivated the suppression, but `SDL3.dll`,
`libstdc++-6.dll` and `libwinpthread-1.dll` are load-time requirements of the
executable. If any is missing the loop still succeeds, NSIS still packages
`pkg\*.*`, the installer is still uploaded, attached to the GitHub release and
pushed to itch.io, and the first symptom is a loader error on a user's machine.
The job performs no dependency walk and no smoke launch afterwards.
*(b)* `Cache NDK` (`:342-346`) declares
`path: ${{ env.ANDROID_SDK_ROOT }}/ndk/25.2.9519653`. The `env` context is
populated only from workflow/job/step `env:` maps; the workflow's only `env:`
block defines `FORCE_JAVASCRIPT_ACTIONS_TO_NODE24` alone, so the expression
interpolates to the empty string and the cache path is the absolute
`/ndk/25.2.9519653`. The step immediately after reads the same variable
correctly as `$ANDROID_SDK_ROOT` inside a `run:` shell. The NDK cache therefore
never hits and every Android run re-downloads the NDK; no artifact is affected.

**REL-014 — Medium. The vendored iniparser ships in every artifact with no
licence, version, or provenance.** `third_party/iniparser/` contains exactly four
files — `iniparser.c`, `iniparser.h`, `dictionary.c`, `dictionary.h` — and no
`LICENSE`, `COPYING`, `README`, or version marker. The two headers carry only
`@file`/`@author N. Devillard`/`@brief` comment blocks with no copyright or
licence text. The repository has exactly one licence file, `COPYING`, which does
not mention iniparser. `android/app/jni/iniparser/` is a second copy with the
same gap. Upstream iniparser is distributed under the MIT licence, whose sole
condition is that the copyright notice accompany all copies and substantial
portions — and every artifact this project ships statically links this code
(`CMakeLists.txt:27-33` → `iniparser-static`, `android/app/CMakeLists.txt:60-64`
→ `iniparser`). The same gap prevents any version determination: the API shape
(`const char * iniparser_getstring`, `iniparser_getint64`) places it in the 4.x
family, but nothing records which release, so no upstream fix can be tracked
against it.

### Extensions to existing entries

- **REL-004** (version strings) is extended from three strings to the full
  measured set of **five** — `2.2.1`, `2.4.9`, `v2.4.26`, `2.4.27`, `0.1.0` —
  and gains two packaging consequences this gate owns: `versionCode` is the
  literal `10` in `android/app/build.gradle:14` and nothing in the workflow
  overrides it, so two different tagged releases produce APKs Android refuses to
  treat as upgrades even if they were signed identically; and
  `share/icons/fb.rc` contains only an `ICON` statement and **no** `VERSIONINFO`
  block, so the shipped `frozen-bubble-sdl3.exe` carries no version resource at
  all while the NSIS installer around it does.
- **REL-006** (stale build files and contradicting documentation) is extended
  with the Nix slice: `default.nix` cannot build for three independent reasons
  (SDL2 `buildInputs` against `find_package(SDL3 REQUIRED)`; installing
  `build/frozen-bubble-sdl2`, a name no target emits; copying `../share` from
  outside the unpacked source root), `default.nix:21-22,62` and `flake.nix:3`
  still identify the project as the SDL2 port, and `.gitignore:8` still ignores
  `/frozen-bubble-sdl2`.
- **REL-007** (signing and build-input integrity) gains direct evidence —
  `:app:signingReport` reports `Variant: release / Config: null` — and a
  reachability correction: the Android job is **not** disabled, so the per-run
  keystore reaches every tagged release, and it compounds with REL-004's static
  `versionCode` to give two independent reasons a shipped APK cannot upgrade
  another.
- **REL-008** (installed-layout asset resolution) has its `default.nix:50`
  mitigation qualified: the `-DASSET_PATH="$out/share"` flag is correct in
  isolation, but the derivation containing it cannot complete (REL-006
  extension), so it does not in fact mitigate anything for any user.
- **REL-002** (harness process/port ownership) keeps its Task 3 severity and
  wording. Task 9's remediation ownership is discharged as a concrete design:
  bind the test server to port 0, read the bound port from its stdout or from a
  `--port-file`, run it in the foreground as a direct child, assert the child is
  alive after readiness, and tear it down with `terminate()`/`wait()` in a
  `finally`. Applying the same change to `tools/server_tests/test_room_caps.py`
  removes its UDP 1511 bind as well. The design belongs to **IMP-016**'s CI job,
  which is what makes the fix observable.

### New improvements

**IMP-016 — High benefit / Low effort / Low risk. Run the registered checks in
CI.** No job in `.github/workflows/build.yml` (the only workflow file) invokes
`ctest`, `--target test`, `gradlew test`, or `pytest`; compilation is the entire
gate. *Location:* a new `test` job. *Inputs:* an `ubuntu-22.04` configure with
`-DCMAKE_BUILD_TYPE=Debug`, the existing SDL3 build step, `ctest
--output-on-failure`. *Assertions:* all 5 registered tests pass, after REL-002's
isolation fix lands. *Matrix:* Linux at minimum; macOS second. Add to the same
job the cheap repository-hygiene assertions this audit had to make by hand:
no tracked dangling symlink (REL-005), a single version value across
`server/CMakeLists.txt`, `src/platform.h`, `android/app/build.gradle` and the
tag (REL-004), every declared build file configures (REL-006), a licence file
exists for each vendored dependency (REL-014), and every `uses:` is
commit-pinned (REL-011). Make the `release` job `needs` it.

**IMP-017 — High benefit / Medium effort / Low risk. Settings, persistence, and
input-bounds regression tests.** *Location:* `tests/gamesettings_test.cpp`
linking `src/gamesettings.cpp` (the pattern Task 6's harness already proved).
*Inputs:* an isolated `CFFIXED_USER_HOME`; a read-only preferences directory; an
INI with a syntax error; an INI with an over-long line; `Key=99999`;
`WindowHeight=100000`; `SpeedMultiplier=nan`; a truncated highscore file; a
level-history file with a non-numeric field. *Assertions:* `ReadSettings`
terminates in bounded time (BUG-026); a syntax error preserves the other stored
keys (BUG-027); every scancode written to `PlayerKeys` is `< SDL_SCANCODE_COUNT`
(BUG-028); `WindowHeight` is clamped by the height bound (BUG-029);
`SpeedMultiplier` is finite (BUG-030); a write failure is reported on a category
SDL does not suppress (BUG-031); a corrupt highscore file leaves the object
constructed rather than aborting (BUG-032, BUG-034). Extend with controller-slot
release and the 20-vs-26 stride (BUG-035, BUG-036) by driving
`HandleControllerEvent` with synthesized `SDL_Event`s rather than real hardware,
and with the player-count reachability limits (BUG-039, BUG-040). Inject the
process-control call behind a seam so BUG-033 can be asserted without running
`pkill`. *Matrix:* Linux and macOS.

**IMP-018 — High benefit / Medium effort / Medium risk. Promote Task 5's
production-object gameplay harness into CTest.** The harness already exists and
already reproduced BUG-018, BUG-019 and BUG-025 against unchanged production
objects; it is simply not registered. *Location:*
`tests/bubblegame_rules_test.cpp` plus the existing test-TU visibility seam.
*Inputs:* player counts 1/2/5/6/20; team counts 1-5; colours 5 and 8; both grid
orientations; three consecutive rounds; Clear Mode; a simultaneous final loss; a
quit-then-new-match transition that shrinks the player count; `deltaScale = 15`.
*Assertions:* Clear Mode is honoured on board clear (BUG-018); a simultaneous
final loss resolves as a draw (BUG-019); no malus survives a match transition
into a cleared board (BUG-020); departures respect the configured continuation
and victory limits (BUG-021, BUG-023); chain targeting uses flipped-grid parity
(BUG-022); clear-win accounting is independent of `F`/stick ordering (BUG-024);
a maximum-delta step cannot pass through an occupied cell (BUG-025).
*Matrix:* Linux and macOS, normal and ASan+UBSan.

**IMP-019 — High benefit / High effort / Low risk. Protocol and parser unit
tests for the server and the network client.** *Location:*
`tests/server_parse_test.c` (linking `server/net.c`, `server/game.c`,
`server/tools.c` with a test `main`) and `tests/netclient_parse_test.cpp`.
*Inputs:* only well-formed and boundary-shaped inputs — this audit's scope
excludes hostile traffic — a WebSocket handshake split across two buffers
(BUG-006); a short send return (BUG-007); an empty `CREATE ` nickname (BUG-009);
a room reaching one player and then zero (BUG-005); a `START` repeated while
peers are already priority (BUG-011); an upload that exceeds the configured
admission limit (BUG-004); a room kick and a post-start close (BUG-003); a
`Content-Length` at `INT_MAX` (SEC-002); a 128-byte LAN probe with no NUL
(SEC-005); a 40-digit numeric field (SEC-006); a chat/binary frame whose claimed
sender is not the connection's seat (SEC-004); a `setgid` stub returning failure
(SEC-001); a coordinate outside the board and a `PLAYERTEAM_Pn` outside 1-5
(SEC-003, SEC-007); a lobby response arriving out of order and an ordinary
rejection type (BUG-015); a `connect` that becomes writable but sets `SO_ERROR`
(BUG-016); a greeting split across `recv` calls (BUG-017); a round-2 sync
arriving before `SyncNetworkLevel` (BUG-014); a part/rejoin cycle (BUG-013,
BUG-037); a game-list rebuild between highlight and join (BUG-038); and a
`%zd`-formatted `size_t` (REL-001). *Assertions:* each parse either succeeds
with the correct value or is rejected without indexing outside its buffer, and
no path calls `exit()`. *Matrix:* Linux; macOS for the client half.

**IMP-020 — High benefit / Medium effort / Low risk. Packaged-artifact smoke
tests.** Every finding in this class was established from static or
harness-level evidence because no gate could launch a packaged artifact.
*Location:* a `package-smoke` job per platform, after each build job.
*Inputs:* the produced AppImage, `.app`, installer directory, APK and WASM
bundle; a clean machine with no source tree at the baked `DATA_DIR`.
*Assertions:* the binary starts under a dummy video/audio driver and exits 0
with `--help`-equivalent handling (REL-003, REL-008, BUG-034); `g_dataDir`
resolves inside the package, not into a build path (REL-008); the log file is
created outside the process CWD and initialization failure is reported
(BUG-047); on Android an emulator install-over-install refreshes a changed asset
and repairs a truncated one (BUG-046); on WASM a headless browser reload
preserves settings and highscores (BUG-048); a dependency walk finds no
unresolved import (REL-013); the APK is signed by a stable, checked-in-secret
certificate and its `versionCode` exceeds the previous release's (REL-007,
REL-004); the DMG contains the expected architectures (REL-012); and
`docker/setup.sh` leaves a pre-existing ECDSA key byte-identical (REL-010).
*Matrix:* all five platforms.

**IMP-021 — Medium benefit / Medium effort / Low risk. Resource-lifetime
regression job.** Apple ASan cannot detect leaks, so every leak conclusion in
this audit rests on ownership tables and RSS measurement. *Location:* a
Linux-only CI job building with `-fsanitize=address,undefined` and
`ASAN_OPTIONS=detect_leaks=1`, running the IMP-018 harness plus a transition and
text-lifecycle driver. *Inputs:* a game start, a round reload, ten transition
animations covering all five effect families with a seeded RNG, a levelset
highscore insert, a missing single-player button asset, a missing candy asset.
*Assertions:* no leak is reported across repeated game starts (BUG-041,
BUG-042); a missing asset produces a diagnostic and a clean exit rather than a
null dereference (BUG-001, BUG-044); a stored `TTFText` retains its texture
(BUG-045); the targeting indicator has a font before it is rendered (BUG-043);
and the pixel helpers reject one-past-the-end indices (IMP-013).
*Matrix:* Linux only — the platform where LeakSanitizer works.

**IMP-022 — Medium benefit / Low effort / Low risk. Unify the warning
configuration across build definitions.** `CMakeLists.txt:14` applies
`-Wall -Wextra -pedantic -Wno-pointer-arith` to the native, Windows and WASM
builds and, by directory scope, to `server/` as well;
`android/app/CMakeLists.txt` applies **none**, so the NDK toolchain compiles the
same 28 translation units — including the 32-bit `armeabi-v7a` ABI, where
narrowing and pointer-width diagnostics are most informative — with warnings
off. The standalone `docker/Dockerfile` server build likewise loses
`-Wno-pointer-arith` because it configures `server/` as a top-level project.
Lift the flag set into a shared interface target or a small included `.cmake`
module and consume it from all three roots.

**IMP-023 — Medium benefit / Low effort / Low risk. Constrain the iniparser
dependency boundary.** `CMakeLists.txt:24` silently prefers any system
iniparser over the bundled copy, `cmake/Findiniparser.cmake` sets no
`iniparser_VERSION` and passes no `VERSION_VAR`, and the bundled sources carry
no version marker, so the build's actual parser implementation is a property of
the host rather than the repository. `README.md:176` tells the reader
"`iniparser` is bundled — no separate install needed", which is true only when
no system copy happens to be installed. Either pass a minimum version and match
it against the vendored API level, or drop the `find_package` call and always
build the bundled sources; record the vendored release alongside the licence
text REL-014 requires.

## Dismissed candidates

- **`cmake_uninstall.cmake.in` uses a removed CMake command.** Suspected: the
  file's `exec_program` (`:10`) was deprecated in CMake 3.0, and this host runs
  CMake 4.3.4, so `make uninstall` would abort with "Unknown CMake command". The
  consequence would be that the documented uninstall target fails on every
  modern toolchain. **Disproved by running it:** `cmake -P` on a minimal script
  calling `exec_program` exited **0**, executing the command and returning its
  output and status; CMake 4.3.4 emits only a `CMP0153` developer warning. The
  command is deprecated, not removed, so the uninstall target still works. No
  finding; the modernization to `execute_process` is left as ordinary cleanup
  under IMP-009's simplification class rather than a new ID.
- **`find_package(iniparser QUIET COMPONENTS static)` can leave the build with
  no iniparser target.** Suspected: a system installation providing only a
  header and a shared library would set `iniparser_FOUND` without creating
  `iniparser-static`, and `target_link_libraries(… iniparser-static)`
  (`CMakeLists.txt:153`) would then reference a nonexistent target and fail at
  generate time. **Traced to counter-evidence:** the guard at
  `CMakeLists.txt:25` is `if(NOT iniparser_FOUND OR NOT TARGET
  iniparser-static)`, whose second disjunct is exactly this case, and it builds
  the bundled static library under the same name. The audit host reproduced the
  benign side of the same branch — the configure log records `iniparser system
  package not found, building from bundled source` and generation succeeded. The
  real weakness at this boundary is the absence of any version constraint, which
  is registered as IMP-023.
- **The WASM artifact is nested wrongly inside its release and itch payloads.**
  Suspected: `build-wasm` uploads two paths (`dist-wasm/` and
  `frozen-bubble-wasm.zip`) under one artifact name, so the downstream
  references `WASM/frozen-bubble-wasm.zip` (release) and
  `artifact-wasm/dist-wasm` (itch) could not both be right. **Traced:**
  `upload-artifact` roots a multi-path artifact at the least common ancestor of
  the matched files, which here is the workspace root, so the artifact contains
  `dist-wasm/…` and `frozen-bubble-wasm.zip` side by side. The bare
  `download-artifact` in `release` writes it to `WASM/`, making
  `WASM/frozen-bubble-wasm.zip` correct; the named download in
  `deploy-itchio-html5` writes it to `artifact-wasm/`, making
  `artifact-wasm/dist-wasm` correct. Both references resolve; the two consumers
  deliberately ship different shapes (a zip for the release page, a directory
  for itch's HTML5 player). No defect.
- **`netlify.toml` deploys the source tree instead of the game.** Suspected: the
  file declares COOP/COEP headers but no `[build]` section, and `dist-wasm/` is
  produced only in CI (measured: zero tracked paths under `dist-wasm/`), so a
  Netlify deploy would publish the repository root and serve no playable build.
  **Not promoted:** Netlify resolves the publish directory from site-level
  settings when `netlify.toml` omits it, and those settings live outside the
  repository and outside this audit's read access. The consequence therefore
  cannot be traced to a wrong outcome from repository evidence alone. Recorded
  instead as a Limitation. The headers themselves were verified correct: they
  match `README.md:235-236` exactly and are the two required for
  `SharedArrayBuffer`-backed audio.

## Coverage

All **21** remaining pending rows in
[FILE_COVERAGE.md](../FILE_COVERAGE.md) reached a final disposition in this
gate: `.github/workflows/build.yml`, `CMakeLists.txt`, `README.md`,
`SetupServer.md`, `cmake/Findiniparser.cmake`, `cmake/cmake_uninstall.cmake.in`,
`default.nix`, `docker/Dockerfile`, `docker/docker-compose.yml`,
`docker/nginx.conf`, `docker/setup.sh`, `docker/ssl/.gitignore`,
`docker/ssl/fullchain.pem.example`, `docker/ssl/privkey.pem.example`,
`flake.lock`, `flake.nix`, `netlify.toml`, `shell.nix`, `start-server.sh`,
`tools/ports/sdl3_image.py`, `tools/ports/sdl3_mixer.py`.

`server/CMakeLists.txt`'s Task 9 build-boundary half is closed by the parity
table above. The inventory remains **237** rows, equal to the pinned-tree filter
of `09d6c7bfcd864a0ad3951b87d16a88dc770392a3`, and **0** rows carry a pending
disposition after this gate.

## Limitations

- LeakSanitizer is unavailable in Apple ASan on this macOS arm64 host. Address
  and undefined-behavior instrumentation are retained; leak coverage is absent.
  IMP-021 exists because of this.
- The registered server-list test is not process/port isolated on POSIX. Task 2
  used an in-memory dynamic-port/foreground substitution with a live-child
  ownership assertion for trustworthy binary coverage and did not alter
  production or test code. Task 9 recorded the remediation design but changed no
  test file.
- **No GitHub Actions workflow was executed, triggered, or dispatched.** Every
  conclusion about CI is a reading of `build.yml` against documented Actions
  semantics. Specifically unverified: whether `macos-latest` currently resolves
  to an arm64 or x86_64 runner (REL-012 is stated architecture-agnostically for
  that reason), whether `upload-artifact` preserves the AppImage executable bit
  through the itch.io deploy path, which Emscripten release `version: 'latest'`
  resolves to, whether the 21 named MinGW DLLs are all present on the runner,
  and the repository's default `GITHUB_TOKEN` permission scope. These are
  **unexamined, not passed**.
- **No container was started.** `docker compose config` is a local parse; the
  Docker daemon was not required and no image was built, no service started, no
  port bound. `docker/Dockerfile`, `docker/nginx.conf` and the compose service
  wiring are static readings. `docker compose up` was deliberately not run.
- **No external network operation was performed.** No release was created, no
  artifact uploaded, no itch.io channel touched, no server contacted, no
  dependency downloaded. All version pins were read from files; upstream tag
  existence and content were not confirmed.
- **`python3 -c "import yaml"` is unavailable on this host**, so the workflow was
  confirmed by a single YAML implementation (Ruby 2.6 Psych) rather than two.
  The brief's exact ruby invocation additionally failed on that Ruby's missing
  `aliases:` keyword; the substitute is recorded with its exit status and the
  reason the keyword was immaterial.
- **Two brief Step 4 commands were substituted, not skipped.** The exact
  `ruby … aliases: true` and `./android/gradlew tasks --all --no-daemon`
  invocations both exited 1 for host/CWD reasons; both exact commands and both
  accepted substitutes are recorded with exits. Neither exact command's failure
  indicates a repository defect.
- `docker/setup.sh`'s regenerate branch was proved by reproducing its `key_ok`
  predicate against a real EC key on two OpenSSL implementations. The
  destructive `openssl req -x509` step itself was **not** executed against any
  real or fake certificate pair, so the overwrite is a code-supported
  consequence of a reproduced predicate rather than an observed file deletion.
  certbot's ECDSA default was taken from its documented behaviour since 2.0 and
  not verified by installing certbot.
- Per the user's scope restriction, **no security-specific runtime test was
  run**. The `@master` action pin, the repo-visible keystore password, the
  unchecked MinGW downloads, the absent action commit pins, and the mutable base
  image tags are documented statically from the workflow text. The omitted
  supply-chain and credential-exposure checks are limitations, not passes.
- `netlify.toml`'s effective publish directory depends on Netlify site settings
  that are not in the repository, so the deployment question raised under
  Dismissed candidates is **unresolvable from repository evidence**, not
  resolved in the project's favour.
- Task 9 read `CLAUDE.md` and `CHANGELOG.md` as evidence. Neither is part of the
  237-row coverage inventory (Task 1's filter excluded them), so REL-009's
  `CLAUDE.md` half is recorded as a finding without a coverage row of its own.
- The two bundled iniparser copies were reviewed at the boundary only —
  provenance, version, licence, preference order, and their two macOS `sprintf`
  warnings. Their internals remain excluded from the project-owned source audit.

## Gate conclusion

Closed. All eight brief steps were executed, with the two Step 4 substitutions
recorded above and Step 4's `docker compose config` explicitly limited to a
local parse. Fourteen new IDs were registered — REL-009 through REL-014 and
IMP-016 through IMP-023 — four existing entries (REL-004, REL-006, REL-007,
REL-008) were extended rather than duplicated, REL-002's remediation ownership
was discharged as a design rather than a code change, IMP-008 was closed as a
confirmed improvement with no defect promoted from any of its families, and four
candidates were dismissed with counter-evidence that traces each consequence —
one of them disproved by running the command it doubted. All 21 remaining
pending coverage rows reached a final disposition; the inventory stays at 237
rows with none pending. The most consequential correction this gate makes is not
one of its own findings: `CLAUDE.md`'s claim that four of five platform build
jobs are disabled is false at the pinned baseline, and Task 8's reachability
statements that relied on it are corrected in the registry and the status file.

**Fix Round 1** (independent review of `efc5ba3b`) accepted two Important and
two Minor findings, none disputed. **REL-013's DLL count was wrong**: the
Windows packaging loop copies **21** named DLLs, not 20 — the original sweep
command (`sed -n '261,268p' | tr -s ' \\' '\n\n' | grep -c '\.dll$'`) undercounted
because the loop's final entry, `libpcre2-8-0.dll;`, has its trailing semicolon
glued directly to the filename with no space or backslash for `tr` to split on,
so the `\.dll$`-anchored `grep` never matched it; the `:261-269` line range
itself was already correct. **REL-007's citation range was incomplete**: the
literal password appears four times, but two of the four (`:404`, `:406`) sit
in the *next* step's `env:` block, outside the originally cited `:390-398`
range — the citation now names both steps explicitly. Neither finding's
conclusion or severity changed. A re-run count sweep re-derived every Task 9
quantity that depended on whitespace splitting, `tr`, a line count standing in
for an occurrence count, or a first-occurrence index; only the DLL count
reproduced differently (21, not 20). Full record in the
[status ledger](../SDL3_REVIEW_STATUS.md#task-9-fix-round-1-ledger).
