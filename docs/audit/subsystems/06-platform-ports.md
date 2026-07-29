# 06 — Platform Ports Audit Notebook

## Scope

Task 8: native, WASM, and Android integration, platform guards, filesystem/logging
behavior, and vendored boundaries.

Files reviewed in full:
`src/main.cpp`, `src/platform.cpp`, `src/platform.h`, `src/logger.cpp`,
`src/logger.h`; the platform slices of `src/frozenbubble.cpp`/`.h` and
`src/mainmenu_server.cpp`; `CMakeLists.txt` (platform branches),
`CMakeListsEmscripten.txt`, `cmake/Emscripten.cmake`, `android/app/CMakeLists.txt`;
`web/index.html`, `web/shell.html`, `web/README.md`, `WASM_PORT.md`,
`android/SETUP.md`, `netlify.toml` (COOP/COEP header boundary); the Android
project — `AndroidManifest.xml`, `build.gradle`, `android/build.gradle`,
`settings.gradle`, `gradle.properties`, `gradle/wrapper/*`, `gradlew`,
`gradlew.bat`, `res/values/strings.xml`, and all four
`android/app/src/main/java/org/frozenbubble/*.java`; and the Android and WASM
jobs of `.github/workflows/build.yml` as CI evidence for platforms this host
cannot execute.

Guard branches were enumerated across all of `src/**`.

Reviewed at the boundary only: the 11 vendored `org/libsdl/app/*.java` files, the
four SDL gitlinks, the 97 tracked `android/app/jni/include/SDL2/*` entries, the
four duplicated `android/app/jni/iniparser/*` files, and the two binary Android
resources plus `gradle-wrapper.jar`.

Owned elsewhere and consumed, not re-derived: the socket compatibility layer
(`src/socket_compat.h`, Task 4), the settings/highscore persistence semantics
(Task 6), and the render/audio WASM early-returns (Task 7).

## Trust boundaries and invariants

- **Entry point.** `src/main.cpp` defines `SDL_MAIN_HANDLED` on every platform
  except Android (`main.cpp:20-22`), includes `<SDL3/SDL_main.h>`, then calls
  `InitDataDir()` before `FrozenBubble::Instance()->RunForEver()`. The
  data-directory must therefore be resolved before any singleton exists; every
  asset path in the tree flows through `ASSET()` and hence through `g_dataDir`.
- **Exactly-one-implementation invariant.** `InitDataDir()` has three mutually
  exclusive definitions selected by `#ifdef __ANDROID__` / `#elif
  defined(__WASM_PORT__)` / `#else` (`platform.cpp:31`, `:87`, `:95`), and the
  desktop definition has three further mutually exclusive interior branches
  (`_WIN32 || __MINGW32__`, `__APPLE__`, `__linux__`) all falling through to
  `g_dataDir = DATA_DIR` (`platform.cpp:138`). No platform can select two.
- **`ASSET()` uniformity.** All three arms of `ASSET()` (`platform.h:34-45`)
  return `g_dataDir + relpath`; the `#ifdef` structure is decorative. The
  invariant is therefore: every platform difference lives in `InitDataDir()`
  alone.
- **Android asset invariant.** C++ `fopen`/`SDL_IOStream` cannot read APK
  assets, so `AssetExtractor.extractAll()` must copy every asset to
  `getFilesDir()/share` *before* `super.onCreate()` starts SDL, and
  `InitDataDir()` must read back the exact path Java used
  (`FrozenBubbleActivity.sExtractedDataDir` via JNI). A partial extraction
  breaks the invariant silently, because Android is the one platform where
  `VerifyAssetDirectory()` is compiled out (`frozenbubble.cpp:102`).
- **WASM filesystem invariant.** Assets are read-only MEMFS mounted at `/share`
  by `--preload-file`; preferences are written to whatever `SDL_GetPrefPath`
  returns. SDL3's Emscripten backend returns `/libsdl/<org>/<app>/` created with
  plain `mkdir` (`android/app/jni/SDL3/src/filesystem/emscripten/SDL_sysfilesystem.c:40-79`),
  i.e. MEMFS. Persistence therefore requires an explicitly linked IDBFS mount
  plus `FS.syncfs`, or an `EM_ASM` `localStorage` path.
- **Ownership of the shutdown path.** `~FrozenBubble()` is the only caller of
  `Logger::Shutdown()`, `HighscoreManager::Dispose()`, `AudioMixer::Dispose()`
  and `GameSettings::Dispose()` (`frozenbubble.cpp:196-216`). On native it runs
  via the explicit `this->~FrozenBubble()` at `frozenbubble.cpp:246`; on WASM
  `RunForEver` returns immediately after `emscripten_set_main_loop`
  (`frozenbubble.cpp:238-239`), so no shutdown path ever executes in the
  browser.
- **Server-hosting boundary.** `MainMenu::StartLocalServer` and
  `StopLocalServer` are compiled to a diagnostic stub on
  `__ANDROID__ || __WASM_PORT__ || _WIN32` (`mainmenu_server.cpp:80`, `:173`),
  and `portInUse` returns `false` unconditionally on `_WIN32`
  (`mainmenu_server.cpp:52-53`). `fork`/`exec`/`kill`/`system` therefore exist
  only on macOS and Linux. The root `CMakeLists.txt:38` additionally excludes
  the `server/` subdirectory on `WIN32 OR MINGW OR EMSCRIPTEN`, so those
  platforms cannot build the binary the client would exec.

## Static review

### Step 1 — platform behavior matrix

| Aspect | macOS (native) | Linux (native) | Windows (MinGW) | WASM | Android |
|---|---|---|---|---|---|
| Entry point | `main()`, `SDL_MAIN_HANDLED` | same | same | same | `main()` **without** `SDL_MAIN_HANDLED` (`main.cpp:20-22`); SDL supplies the entry via `SDLActivity` |
| Data directory | `.app` bundle → `SDL_GetBasePath()+"share"` when the base path ends in `Resources/`; otherwise compile-time `DATA_DIR` (`platform.cpp:109-121`, `:138`) | `readlink("/proc/self/exe")`, strip at the last `/bin/`, append `/share/frozen-bubble`; otherwise `DATA_DIR` (`platform.cpp:122-136`) | `GetModuleFileNameA` directory + `\share`; otherwise `DATA_DIR` (`platform.cpp:98-108`) | fixed `"/share"` (`platform.cpp:91-93`) | JNI read of `FrozenBubbleActivity.sExtractedDataDir`, else `SDL_GetAndroidInternalStoragePath()+"/share"`, else the hard-coded `/data/data/org.frozenbubble/files/share` (`platform.cpp:54-85`) |
| `DATA_DIR` definition | `ASSET_PATH`, default `${CMAKE_SOURCE_DIR}/share` (`CMakeLists.txt:127-140`) | same | same | `"/share"` (`CMakeLists.txt:138`) | `""` (`android/app/CMakeLists.txt:100`), unused because the `__ANDROID__` arm never reaches line 138 |
| Asset verification | `VerifyAssetDirectory()` stat + message box, then `IsGameQuit = true` (`frozenbubble.cpp:102-112`) | same | same | same | **compiled out** (`#ifndef __ANDROID__`) |
| Preference directory | `SDL_GetPrefPath("", "frozen-bubble")` → `~/Library/Application Support/frozen-bubble/` (honours `CFFIXED_USER_HOME`, not `HOME`) | `$XDG_DATA_HOME`/`~/.local/share/frozen-bubble/` | `%APPDATA%\frozen-bubble\` | `/libsdl/frozen-bubble/` in **volatile MEMFS** | app-private `files/` path |
| Log file | `fopen` of a **CWD-relative** name (`frozenbubble.cpp:82-99`, `logger.cpp:96`) | same | same (`logger.cpp:24-27` maps `mkdir`, never used) | same, in MEMFS | same; process CWD is `/` |
| Networking | POSIX TCP + UDP LAN discovery | same | Winsock via `socket_compat.h`; nonblocking never enabled (REL-003) | Emscripten WebSocket (`networkclient_wasm.cpp`); TCP/UDP/latency/reachability compiled to stubs | POSIX TCP; public list fetched through `FrozenBubbleActivity.fetchUrl` over JNI (`networkclient.cpp:1610-1640`) |
| Main loop | `while(!IsGameQuit) RunOneFrame()` + manual `SDL_Delay` cap (`frozenbubble.cpp:241`, `:316-322`) | same | same | `emscripten_set_main_loop(wasm_one_frame, 0, 0)`, returns immediately (`frozenbubble.cpp:235-239`) | native loop |
| Frame scaling | `deltaScale` from the persisted `speedMultiplier`, clamped `[0.5, 3·mult]` (`frozenbubble.cpp:264-272`); default 3.0 (`gamesettings.h:89`) | same | same | fixed `elapsed/16.67·3.0` clamped `[0.1, 6.0]` (`frozenbubble.cpp:256-263`) | native branch; default multiplier **1.25** (`gamesettings.h:87`) |
| Input | keyboard + gamepad + `SDL_EVENT_FINGER_*` | same | same | `SDL_TOUCH_MOUSEID` filter disabled and `FINGER_*` handlers compiled out (`frozenbubble.cpp:583-642`); browser `prompt()` for text on touch (`platform.cpp:156-164`, `bubblegame_input.cpp:76`) | `InitControllers()` skipped for local multiplayer so the TV remote keeps generating key events (`bubblegame.cpp:403-405`); `WINDOW_CLOSE_REQUESTED` does **not** quit (`frozenbubble.cpp:478-483`) |
| Audio | full `MIX_*` lifecycle | same | same | music stop is immediate; the 500 ms fade + 600 ms `SDL_Delay` are skipped (`audiomixer.cpp:122-128`) | native path |
| Transitions | full | full | full | `DoSnipIn`/`TakeSnipOut` return immediately (`transitionmanager.cpp:50-52`, `:64-66`) | full |
| Fullscreen | `SDL_WINDOW_FULLSCREEN` from settings | same | same | forced windowed; CSS shell scales the canvas (`frozenbubble.cpp:127-133`) | native path |
| Server hosting | `fork`/`execl` search of six paths + `system("pkill -x fb-server")` | same | stub (`_WIN32`) | stub | stub |
| Shutdown | explicit `this->~FrozenBubble()` then return (`frozenbubble.cpp:246`) | same | same | **never runs** | native path, then `FrozenBubbleActivity.onDestroy` calls `Process.killProcess` (`FrozenBubbleActivity.java:65`) |

### Step 2 — guard inventory and source-list parity

Verbatim guard-token occurrences under `src/` (counted with `grep -rho '\b<tok>\b' src/`):
`__WASM_PORT__` 91, `__ANDROID__` 26, `_WIN32` 22, `__linux__` 2, `__MINGW32__` 2,
`__ANDROID_PORT__` 1, `__APPLE__` 1, `__EMSCRIPTEN__` 0, bare `WIN32` 0.

Notes on the inventory:

- `__ANDROID_PORT__` has exactly one consumer, `gamesettings.h:86`
  (`#if defined(__ANDROID__) || defined(__ANDROID_PORT__)`), and exactly one
  definition site, `CMakeLists.txt:135` — the root `if(ANDROID)` branch. The
  Android product is built by `android/app/CMakeLists.txt`, which defines
  `__ANDROID__` explicitly (`:99`) and never includes the root file. The macro
  is therefore inert in the shipping Android build; the `__ANDROID__` half of
  the same condition is what selects the 1.25 speed default. Not a defect —
  the disjunction is correct on both build paths — but it is dead
  configuration (recorded under IMP-015).
- Every Android-only SDL entry point is guarded: `SDL_SendAndroidMessage`
  at `mainmenu.cpp:669`, `mainmenu_netpanel.cpp:99`, `mainmenu_input.cpp:484`
  and `:1529`, and `SDL_GetAndroidJNIEnv`/`SDL_GetAndroidActivity`/
  `SDL_GetAndroidInternalStoragePath` at `platform.cpp:58-79` and
  `networkclient.cpp:1614-1615` — all inside `#ifdef __ANDROID__`. No
  Android-only symbol is reachable from a desktop or WASM translation unit.
- The desktop `InitDataDir` interior branches use `_WIN32 || __MINGW32__`,
  `__APPLE__` and `__linux__`. A non-Linux, non-Apple, non-Windows Unix (BSD,
  Solaris) selects none of them and falls to `DATA_DIR` — correct by design,
  not an omission.
- `shaderstuff.h:24-26` defines `bzero` as a macro on `_WIN32` and
  `shaderstuff.h:28` includes `<iconv.h>` unconditionally. The only iconv use in
  the tree is inside the block comment at `shaderstuff.cpp:1332-1360`; `grep -rn
  'catch *(' src/` and `grep -rn iconv src/` confirm zero live references
  outside that comment. Both the include and the macro are dead portability
  hazards (IMP-015).

**Source-list parity** (derived by extracting the `src/*.cpp` entries from each
`add_executable`/`add_library` block):

| Build file | Sources | Relationship |
|---|---|---|
| `CMakeLists.txt` | 27 explicit + `${NETWORK_CLIENT_SRC}` → **28** native, **29** Emscripten | reference set |
| `android/app/CMakeLists.txt` | **28** | set-equal to the native effective set; zero additions, zero omissions |
| `CMakeListsEmscripten.txt` | **15** | omits **14** of the 28 and adds `networkclient_wasm.cpp` |

The 14 files `CMakeListsEmscripten.txt` omits are `bubblegame_board.cpp`,
`bubblegame_input.cpp`, `bubblegame_level.cpp`, `bubblegame_net.cpp`,
`bubblegame_render.cpp`, `bubblegame_shooter.cpp`, `bubblegame_state.cpp`,
`mainmenu_input.cpp`, `mainmenu_netpanel.cpp`, `mainmenu_panels.cpp`,
`mainmenu_server.cpp`, `netteams.cpp`, `netview.cpp`, `roundstats_color.cpp`.
Those files carry the definitions of the `BubbleGame` and `MainMenu` methods that
the included `bubblegame.cpp`/`mainmenu.cpp` call, so the target cannot link.
The same file also selects SDL **2** ports (`-s USE_SDL=2`,
`SDL2_IMAGE_FORMATS`, `SDL2_MIXER_FORMATS`, `USE_SDL_TTF=2`, lines 15-18) while
every source includes `<SDL3/…>`. Registered as REL-006.

`cmake/Emscripten.cmake` is redundant but **not** broken; see Dynamic evidence.

**Duplicate-definition parity on the Emscripten build.** The root file compiles
both `networkclient.cpp` and `networkclient_wasm.cpp` (`CMakeLists.txt:54-59`),
so the two must partition the `NetworkClient` methods exactly. Every shared
method in `networkclient.cpp` is wrapped in `#ifndef __WASM_PORT__` blocks at
lines 36, 67, 184, 545, 804, 1484 and 1554 (with matching per-method
`#ifdef __WASM_PORT__`/`#else` splits at `:253`, `:316`, `:400`). Verified
empirically in Dynamic evidence.

### Step 3 — filesystem and logging failure paths

- **Installed desktop layout.** `install(TARGETS …)` puts the binary in
  `bin` and `install(DIRECTORY share/ …)` puts the assets in
  `${CMAKE_INSTALL_PREFIX}/share/frozen-bubble` (`CMakeLists.txt:209-220`).
  Lines 223-226 compute `INSTALLED_ASSET_PATH` and log it — and then **never
  use it**: `DATA_DIR` is defined from `ASSET_PATH`, whose default is
  `${CMAKE_SOURCE_DIR}/share` (`:127-128`, `:140`). On Linux the
  `/proc/self/exe` + `/bin/` heuristic recovers the installed prefix; on macOS
  only the `.app` bundle case is handled, so a `make install` binary run from
  `<prefix>/bin` resolves to the build machine's source tree. Registered as
  REL-008, reproduced at runtime below.
- **Relative log path.** `FrozenBubble::FrozenBubble` picks one of five
  hard-coded relative names by `stat`-ing them in the process CWD
  (`frozenbubble.cpp:82-97`) and passes it to `Logger::Initialize`
  (`:99`), which `fopen`s it in append mode (`logger.cpp:96`). The return value
  is discarded at the call site. The names are `creator`, `joiner1` … `joiner4`,
  documented as identifying concurrent players, but the test is existence, not
  liveness — so they in fact enumerate *launches in that directory*. Registered
  as BUG-047, reproduced below.
- **Logger initialization failure.** On `fopen` failure `Logger::Initialize`
  prints to stderr and returns `false` *before* `SDL_SetLogOutputFunction`
  (`logger.cpp:96-100`), leaving SDL's default handler installed. The game
  continues without file diagnostics; nothing surfaces the failure in the UI.
  This is the realistic packaged case (macOS `.app` launched from Finder has
  CWD `/`; Android's process CWD is `/`).
- **Shutdown ordering.** `~FrozenBubble` destroys the renderer and window first,
  then disposes the highscore manager, mixer and settings, then `TTF_Quit`,
  `SDL_Quit`, and finally `Logger::Shutdown` (`frozenbubble.cpp:196-216`).
  Task 7 closed the texture-lifetime question for this order. The platform
  observation this gate adds is that the whole sequence is unreachable on WASM.
- **Android extraction.** `AssetExtractor.extractAll` returns early when
  `.assets_version` matches `getLongVersionCode()` (`AssetExtractor.java:53-61`);
  otherwise it extracts and then writes the marker unconditionally (`:63-74`).
  `extractDir` and `extractFile` swallow every `Exception` into a `Log.e`
  (`:98-100`, `:115-117`), and `extractFile` skips any destination that already
  exists with non-zero length (`:106`). Registered as BUG-046.
- **WASM persistence.** `GameSettings::InitPrefPath` calls
  `SDL_GetPrefPath("", "frozen-bubble")` (`gamesettings.cpp:32`) and
  `HighscoreManager` derives `highlevelshistory`/`highscores` from the same
  `prefPath` (`highscoremanager.cpp:225-226`, `:259-260`). On Emscripten that
  path is MEMFS. The only persistent store in the tree is the `fb_nickname`
  `localStorage` entry written by the four `EM_ASM` sites
  (`mainmenu.cpp:163`, `:264`, `mainmenu_netpanel.cpp:80`,
  `mainmenu_input.cpp:1507`), which is exactly why the `Keys:Nickname`
  INI read/write is `#ifndef __WASM_PORT__` (`gamesettings.cpp:194-197`,
  `:250-252`). Everything else — key bindings, sound flags, graphics level,
  speed multiplier, mouse mode, and both highscore files — is written to
  volatile memory. Registered as BUG-048.
- **Unicode and read-only paths.** `g_dataDir` is a byte string concatenated
  with byte-string relative paths and handed to `fopen`/`IMG_Load`; no
  narrowing, no `wchar_t`, and no locale conversion occurs on any platform, so
  UTF-8 install paths pass through unchanged on macOS/Linux/Android/WASM. On
  Windows the `GetModuleFileNameA` ANSI call is the exception: a path outside
  the active code page is transliterated or replaced by that API before the
  string reaches the game. Read-only asset locations are fine (assets are only
  read); read-only *preference* locations are Task 6's BUG-026, and read-only
  *CWD* is the logger case above.

### Step 4 — Android project review

- `android/build.gradle` pins AGP 8.2.0; `gradle-wrapper.properties` pins
  Gradle 8.2 (`gradle-8.2-all.zip`) with **no** `distributionSha256Sum`.
  `android/gradlew` is tracked mode `100755`, so a fresh clone can execute it.
- `AndroidManifest.xml` still carries `package="org.frozenbubble"` while
  `build.gradle:6` sets `namespace 'org.frozenbubble'`. AGP 8.2 warns and
  ignores the attribute (observed verbatim in the build log); the values agree,
  so there is no behavioral consequence.
- `compileSdk`/`targetSdk` 34, `minSdk` 21, `versionCode` 10,
  `versionName "2.4.27"`, `ndkVersion "25.2.9519653"`,
  `abiFilters 'arm64-v8a', 'armeabi-v7a', 'x86_64'`.
- The `release` build type declares `minifyEnabled false` and a proguard file
  but **no `signingConfig`**, so a local `assembleRelease` emits
  `app-release-unsigned.apk`. CI supplies signing through
  `-Pandroid.injected.signing.*` after generating a fresh keystore in the
  workspace on every run (`.github/workflows/build.yml:389-413`). Registered
  with the wrapper-integrity gap as REL-007.
- The manifest ships Google's public **test** AdMob application ID
  (`ca-app-pub-3940256099942544~3347511713`, flagged by its own comment as
  "swap for production") and `AdsManager.java:41-42` uses the matching public
  test interstitial unit. `MobileAdsInitProvider` is disabled in the manifest
  and `AdsManager.init()` has zero callers anywhere in the tree — only its own
  javadoc names it. The unreachable initializer is recorded under IMP-015; the
  downstream question of whether `InterstitialAd.load` still self-initializes is
  **not** claimed, because that depends on Play Services SDK behavior this gate
  did not execute.
- `FrozenBubbleActivity.onCreate` extracts assets before `super.onCreate`, which
  satisfies the ordering invariant. `onDestroy` calls
  `Process.killProcess(myPid())` after `super.onDestroy()`; the comment gives
  the reason (SDL cannot re-init in one process). Highscores and settings are
  written eagerly at their own call sites (`highscoremanager.cpp:391`, `:411`;
  `gamesettings.cpp:279`, `:304`), so the kill does not by itself lose stored
  state.
- `AssetExtractor.readBytes` ignores the `FileInputStream.read` return value
  (`:123`); for the short version-marker file this is harmless in practice, and
  it is not promoted.
- `android/SETUP.md` documents downloading prebuilt **SDL2** libraries into an
  `android-libs/` directory and copying SDL2's Java glue, none of which exists
  or is referenced; the real build consumes the four SDL3 submodules through
  `android/app/CMakeLists.txt:54-57`. Folded into REL-006.

### Step 5 — WASM project review

- The supported path is the root `CMakeLists.txt` `if(EMSCRIPTEN)` block
  (`:96-125`, `:137-138`), which is exactly what CI runs
  (`emcmake cmake .. -DCMAKE_BUILD_TYPE=Release`, workflow line 468-470) after
  copying `tools/ports/sdl3_image.py` and `tools/ports/sdl3_mixer.py` into the
  emsdk and adding two settings entries.
- `-sDISABLE_EXCEPTION_CATCHING=0` appears only in `target_link_options`, not in
  `target_compile_options`. `grep -rn 'catch *(' src/` returns zero live
  handlers in the whole tree, so the asymmetry has no behavioral consequence
  here; recorded as an observation, not promoted.
- `web/shell.html` is the `--shell-file`; `web/index.html` is a separate,
  unreferenced page titled "Frozen Bubble SDL2" whose only script tag loads
  `frozen-bubble-sdl2.js`, a file no build produces. CI overwrites
  `dist-wasm/index.html` with the generated shell (workflow line 475). Folded
  into REL-006.
- `web/README.md:17` states the WASM build needs no websockify proxy;
  `WASM_PORT.md:96-104` instructs the reader to install and run websockify.
  `WASM_PORT.md:7-11` states the build "currently uses SDL2 Emscripten ports,
  not SDL3" while all three build files and CI select `USE_SDL=3`;
  `WASM_PORT.md:150-159` reproduces an `ASSET()` implementation that does not
  match `platform.h:34-45`; and `WASM_PORT.md:132-134` claims `__WASM_PORT__` is
  "set automatically when using the Emscripten toolchain" — `cmake/Emscripten.cmake`
  never defines it. All folded into REL-006.
- `netlify.toml` and `web/README.md` both apply COOP/COEP. No build file
  requests pthreads or a shared memory (`grep -rn 'pthread\|PTHREAD' CMakeLists.txt
  cmake/ CMakeListsEmscripten.txt` finds only the unrelated MinGW DLL name in
  the workflow), so SharedArrayBuffer is not a prerequisite of this build. The
  headers are inert rather than wrong, and are not promoted.

### Step 6 — packaged-path expectations

- Working-directory independence holds on every desktop platform because each
  branch derives from the executable path or a compile-time constant, never from
  the CWD. The one CWD-dependent artifact is the log file (BUG-047).
- Dynamic-library layout is out of `InitDataDir`'s scope: the macOS build links
  Homebrew SDL3 by rpath; the Windows CI job copies the MinGW DLLs beside the
  exe; Android bundles `lib/<abi>/*.so` in the APK; WASM has no dynamic
  libraries. Only the Android layout was verified locally.
- Linux and Windows execution were not available on this host; their
  conclusions are code traces plus the CI job definitions.

## Dynamic evidence

All commands, exits, and material output are in the canonical status ledger.
Every run used dummy SDL drivers, an isolated `CFFIXED_USER_HOME`, and read-only
repository sources. The user's three real preference files were hashed before
the gate and verified byte-identical afterwards.

### Android build (Step 4)

`cd android && ./gradlew clean assembleRelease --no-daemon` exited **0** in
1 m 30 s from a fresh shell using the persisted `android/local.properties`
(`sdk.dir=/opt/homebrew/share/android-commandlinetools`), NDK 25.2.9519653, and
Homebrew OpenJDK 17.0.19.

- Warnings: the SDK-XML version notice ("understands SDK XML versions up to 3
  but … version 4 was encountered"), the AGP 8 manifest-`package` notice quoted
  in Step 4 above, two `-Wignored-pragmas` `#pragma STDC FENV_ACCESS` warnings
  from `SDL3/src/audio/SDL_audiotypecvt.c:541` and `:820`, and
  `compileReleaseJavaWithJavac`'s "Some input files use or override a deprecated
  API". No warning originated in project C++ sources.
- Output: `android/app/build/outputs/apk/release/app-release-unsigned.apk`,
  37,290,226 bytes; `output-metadata.json` reports `versionCode 10`,
  `versionName "2.4.27"`, `applicationId org.frozenbubble`.
- Three ABI directories present — `arm64-v8a`, `armeabi-v7a`, `x86_64` — each
  carrying 13 shared objects.
- Signing state: unsigned. The APK contains no `META-INF/*.RSA`/`*.SF` v1
  signature block; the file name itself is `app-release-unsigned.apk`.
- **Working-tree effect: none.** `git status --short` printed nothing before and
  after, `git diff --stat HEAD` was empty, and a SHA-256 manifest of the 33
  regular tracked files under `android/` (the other 101 tracked paths are the 97
  dangling symlinks and 4 gitlinks) compared byte-identical. **No restoration was
  required and none was performed.** `android/app/build/`, `android/app/.cxx/`,
  `android/.gradle/` and `android/local.properties` are already covered by
  `.gitignore:20-23`.

Native-library analysis of the produced APK (NDK `llvm-readelf -d` plus
`strings`):

- `libSDL3_image.so` has no libpng `DT_NEEDED` entry but contains the literal
  `libpng16.so`, i.e. it `dlopen`s it; likewise `libSDL3_mixer.so` contains
  `libvorbisfile.so`.
- `libpng.so` and `libpng16.so` are **byte-identical** (`cmp` reports no
  difference) and both carry `SONAME libpng16.so`, so `libpng.so` can never be
  the file that is opened.
- No object in the APK lists `libvorbisenc.so` as `DT_NEEDED`, and no `dlopen`
  string names it.
- Redundant payload: libpng duplicates 288,944 + 212,104 + 318,856 =
  **819,904** bytes, `libvorbisenc.so` 651,600 + 528,136 + 667,088 =
  **1,846,824** bytes; **2,666,728** bytes uncompressed in total across the
  three ABIs. Registered as IMP-014.

### WASM build (Step 5)

A **disposable** Emscripten installation was used: the Homebrew
`emscripten 6.0.4-git` `libexec` tree was clone-copied to
`/tmp/fb-sdl3-audit/task8/emsdk/libexec` and the CI port setup was replayed
against the copy only — `tools/ports/sdl3_mixer.py` and
`tools/ports/sdl3_image.py` copied in, `SDL3_IMAGE_FORMATS`/`SDL3_MIXER_FORMATS`
added at `src/settings.js:1654-1655` and `tools/settings.py:53`, and the
experimental diagnostic commented at `tools/ports/sdl3.py:29`. The copy's own
`.emscripten` resolves `CACHE` into the copy, so **the Homebrew installation was
not modified**; only its read-only `llvm`/`binaryen` directories were reused.

- `emcmake cmake .. -DCMAKE_BUILD_TYPE=Release` in `build-audit-wasm` exited
  **0**.
- `emmake make -j8` exited **0** in 44 s and produced all four artifacts:
  `frozen-bubble-sdl3.wasm` (3,132,667 bytes), `.js` (484,910),
  `.data` (23,647,363), `.html` (2,854). This is a **full link**, not the
  translation-unit-only result Task 4 could reach.
- 29 objects were compiled — the 27 shared sources plus both
  `networkclient.cpp` and `networkclient_wasm.cpp`, confirming the Emscripten
  `NETWORK_CLIENT_SRC` branch.
- 16 compiler warnings in 5 families: `-Wdollar-in-identifier-extension` 7,
  `-Wvariadic-macro-arguments-omitted` 4, `-Wunused-variable` 2,
  `-Wunused-private-field` 2, `-Wunused-but-set-global` 1. No error and no
  undefined symbol.
- **Duplicate-symbol proof.** `llvm-nm --defined-only --extern-only` reports 62
  external definitions in `networkclient.cpp.o` and 30 in
  `networkclient_wasm.cpp.o`; their intersection is 8 symbols, all weak C++
  template/inline instantiations (`std::__throw_length_error`,
  `std::allocator<GameRoom>::destroy`, `__throw_bad_array_new_length`, …).
  **Zero `NetworkClient::` methods appear in both objects**, and the link
  succeeded. The guard partition of Step 2 is exact.
- **Asset packaging proof.** The generated loader lists **3,352** preloaded
  files, all under `/share/`, exactly matching the 3,352 files present under
  `share/` on disk; `remote_package_size` is 23,647,363. `g_dataDir = "/share"`
  therefore addresses the packaged tree correctly, and `ASSET("/gfx/…")`
  resolves to a real entry.
- The custom shell is applied (`fitCanvas`/`preventNavKeys` from
  `web/shell.html` are present in the generated HTML) and the module reports
  `ENVIRONMENT_IS_WEB=true`, `ENVIRONMENT_IS_NODE=false`.
- **Persistence proof.** The linked JavaScript contains **zero** occurrences of
  `IDBFS`; its single `syncfs` occurrence is the `FS.syncfs` API definition, not
  a call; its single `localStorage` occurrence is inside `ASM_CONSTS` — the
  `fb_nickname` read from `mainmenu.cpp:163`. The `.wasm` binary contains the
  `/libsdl/` and `frozen-bubble` pref-path literals. BUG-048 confirmed.
- **`cmake/Emscripten.cmake` re-test.** The command `WASM_PORT.md:65`
  documents (`emcmake cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Emscripten.cmake ..`)
  was run in a throwaway directory and **succeeded** (exit 0): CMake's
  `CMAKE_SYSTEM_NAME Emscripten` still sets `EMSCRIPTEN`, so `__WASM_PORT__`,
  `DATA_DIR="/share"`, the `.html` suffix and the SDL3 port flags are all
  applied and the `server/` subdirectory is still skipped. The suspicion that
  this toolchain breaks the configure is therefore **disproved**. What it does
  produce is a link line carrying `TOTAL_MEMORY=268435456` four times alongside
  the supported path's single `INITIAL_MEMORY=16777216` — two spellings of the
  same Emscripten setting with conflicting values, resolved only by argument
  order. Recorded as an ordering-fragility observation under REL-006's
  redundant-alternative-build-path evidence, not as a separate defect.

### Native packaged-path harness (Step 6)

A test-only translation unit was linked against the **unchanged**
warnings-strict production objects `platform.cpp.o` and `logger.cpp.o` from
`build-audit-werror`, compiled `-Wall -Wextra -pedantic -Werror` with no
diagnostic, and calls the real `InitDataDir()` and `Logger::Initialize()`. It
constructs no `FrozenBubble` and opens no real preference file.

| Layout | `SDL_GetBasePath()` | resulting `g_dataDir` | directory exists |
|---|---|---|---|
| A: harness in `/tmp/…`, CWD = repo root | `/tmp/fb-sdl3-audit/task8/` | `/Users/dchau/gr/frozen-bubble-sdl3/share` | yes |
| B: same binary, CWD = `/` | `/tmp/fb-sdl3-audit/task8/` | `/Users/dchau/gr/frozen-bubble-sdl3/share` | yes |
| C: staged install `…/stage/usr/local/bin/`, CWD = `/private/tmp` | `…/stage/usr/local/bin/` | `/Users/dchau/gr/frozen-bubble-sdl3/share` | yes |
| D: `FakeBundle.app/Contents/MacOS/` with `Contents/Resources/share` | `…/FakeBundle.app/Contents/Resources/` | `…/FakeBundle.app/Contents/Resources/share` | yes |
| E: bundle **without** `Resources/share` | `…/FakeBundle2.app/Contents/Resources/` | `…/FakeBundle2.app/Contents/Resources/share` | **no** |

A vs B proves working-directory independence. D proves the `.app` branch. **C is
REL-008**: the staged-install binary ignored `…/stage/usr/local/share/frozen-bubble`
— which is exactly where `install()` places the assets — and resolved to the
build machine's source tree, because `strings` on `build-audit-release/frozen-bubble-sdl3`
shows the single baked literal `/Users/dchau/gr/frozen-bubble-sdl3/share`. E is
the input state that makes `VerifyAssetDirectory` fail, which is the entry to
Task 6's reproduced BUG-034 crash.

Logger runs against the production `logger.cpp` object:

- Six consecutive launches in one writable directory chose
  `frozen-bubble-creator.log`, `…joiner1.log`, `…joiner2.log`, `…joiner3.log`,
  `…joiner4.log`, and then `…joiner4.log` again — five files created, and every
  launch from the sixth onward appending to the same file. BUG-047's naming half
  reproduced.
- One launch with the CWD set to a mode-555 directory printed
  `Failed to open log file: frozen-bubble-creator.log`, returned
  `logger_initialize=0 initialized=0`, created no file, and the subsequent
  `SDL_Log` call was still emitted — by SDL's *default* handler, because
  `SDL_SetLogOutputFunction` is never reached on the failure path. BUG-047's
  ignored-failure half reproduced.
- The preference probe asserted `ISOLATION=OK` with
  `prefpath=/tmp/fb-sdl3-audit/task8/home8/Library/Application Support/frozen-bubble/`
  before doing anything else, and `shasum -a 256 -c` afterwards reported `OK`
  for all three real preference files.

### Vendored boundary verification (Step 7)

- All **11** files under `android/app/src/main/java/org/libsdl/app/` are
  byte-identical (`cmp`) to the same-named files in the pinned SDL3
  `release-3.4.4` submodule's `android-project` glue, and the set is complete —
  no SDL file is missing locally and none is project-modified.
- All **4** files under `android/app/jni/iniparser/` are byte-identical
  (`cmp`) to `third_party/iniparser/`. They are compiled by two different
  targets (`iniparser` in the Android build, `iniparser-static` in the root
  build), so the copies can drift silently; the Task 2 `sprintf` deprecation
  limitation applies to both.
- The **4** gitlinks resolve to `SDL3` `release-3.4.4`, `SDL3_image`
  `release-3.4.2`, `SDL3_mixer` `release-3.2.0`, `SDL3_ttf` `release-3.2.2`, and
  `git submodule status --recursive` reports **0** uninitialized entries, so the
  Android build was run against fully materialized sources. The desktop build on
  this host links Homebrew SDL 3.4.10 (Task 7's measurement), i.e. the Android
  product is pinned one patch family behind the desktop product — an
  observation for Task 9's dependency review, not a defect.
- The **97** tracked entries under `android/app/jni/include/SDL2/` are all git
  mode `120000` symlinks, all dangling, pointing into four directories beneath
  `/Users/dericchau/ai/fb2-port/frozen-bubble-sdl2/android/app/jni/`. No build
  file, source file, or CI step references `jni/include`. Registered as REL-005.

## Candidates

Every candidate raised in this gate reached a terminal state; none remains open.

| Candidate | Outcome |
|---|---|
| `InitDataDir` ignores the installed asset prefix on macOS | confirmed — REL-008 (reproduced, layout C) |
| CWD-relative log path with launch-count naming and ignored init failure | confirmed — BUG-047 (both halves reproduced) |
| Android extraction caches a partial/failed copy permanently | confirmed — BUG-046 (complete code-supported chain) |
| WASM preferences and highscores never persist | confirmed — BUG-048 (proved in the linked artifact) |
| 97 dangling absolute symlinks tracked under `jni/include/SDL2` | confirmed — REL-005 |
| `CMakeListsEmscripten.txt` cannot link; WASM/Android docs contradict the build | confirmed — REL-006 |
| Release APK signing and Gradle wrapper integrity | confirmed — REL-007 |
| Duplicate/unreferenced native libraries in the APK | confirmed as an improvement — IMP-014 |
| Dead platform-layer code (`ASSET_FILES`/`extractAssets`, logger `mkdir` macro, `<iconv.h>` + `bzero`, `AdsManager.init`, `__ANDROID_PORT__`) | confirmed as an improvement — IMP-015 |
| `APP_VERSION "v2.4.26"` vs tag/APK 2.4.27 | confirmed — recorded by extending REL-004, no new ID |
| `system("pkill -x fb-server")` platform availability and portability | confirmed — recorded by extending BUG-033, no new ID |
| Windows/MinGW platform-guard side of the socket layer | confirmed — recorded by extending REL-003, no new ID |
| `cmake/Emscripten.cmake` breaks the documented configure | **dismissed** — see Dismissed candidates |
| `-sDISABLE_EXCEPTION_CATCHING=0` is link-only | **dismissed** — see Dismissed candidates |
| COOP/COEP claimed "required for audio (SharedArrayBuffer)" | **dismissed** — see Dismissed candidates |
| Android `package=` attribute breaks AGP 8 builds | **dismissed** — see Dismissed candidates |
| `android/gradlew` not executable in a fresh clone | **dismissed** — see Dismissed candidates |
| `<iconv.h>` unavailable on the Android NDK | **dismissed** — see Dismissed candidates |

## Confirmed findings

### BUG-046 — a failed or partial Android asset extraction is cached forever

`AssetExtractor.extractAll` (`AssetExtractor.java:39-78`) writes the
`.assets_version` marker unconditionally after `extractDir` returns (`:68-74`),
and `extractDir`/`extractFile` convert every `Exception` into a `Log.e` and
return normally (`:98-100`, `:115-117`). `extractFile` additionally skips any
destination that already exists with non-zero length (`:106`). So if a copy is
interrupted — device full, process killed mid-write — the destination is left
truncated but non-empty, the marker is still written, and every subsequent
launch takes the early return at `:53-61` and never re-extracts. The truncated
file survives until the app version changes or app data is cleared.

The consequence is unbounded on Android specifically, because Android is the one
platform where `VerifyAssetDirectory` is compiled out (`frozenbubble.cpp:102`):
nothing checks the tree before use. A truncated PNG makes `IMG_Load` return
null, which is the same unchecked-load class already confirmed as BUG-001 and
BUG-044. Severity Medium. Not reproduced on a device — no Android hardware or
emulator was driven — so this is a complete code-supported causal argument, not
an observed runtime fact.

### BUG-047 — CWD-relative log naming counts launches, and its failure is ignored

`FrozenBubble::FrozenBubble` selects one of five relative log names by `stat`-ing
them in the process CWD (`frozenbubble.cpp:82-97`) and discards
`Logger::Initialize`'s return value (`:99`); `Logger::Initialize` `fopen`s the
relative path (`logger.cpp:96`) and, on failure, returns `false` **before**
installing the SDL log callback (`:97-100`).

Two reproduced consequences:

1. The names do not identify concurrent players. Six consecutive single-process
   launches in one directory produced `creator`, `joiner1`, `joiner2`,
   `joiner3`, `joiner4`, then `joiner4` again — the test is file existence, not
   process liveness. A solo player accumulates five stray log files in whatever
   directory they launched from, and from the sixth launch on every session
   appends to `frozen-bubble-joiner4.log` while a genuine second instance is
   misnamed.
2. In a read-only CWD `Logger::Initialize` returned `0`, created nothing, and
   the caller continued without noticing. That is the packaged case: a macOS
   `.app` launched from Finder and an Android process both have CWD `/`. The
   diagnostics that Task 6's BUG-026/BUG-032/BUG-034 investigations depend on
   are silently absent exactly when a user would need them.

Severity Low (diagnostics and stray files; no crash, no data loss). Both halves
reproduced against the production `logger.cpp` object.

### BUG-048 — WASM settings and highscores are written to volatile memory

`GameSettings::InitPrefPath` uses `SDL_GetPrefPath("", "frozen-bubble")`
(`gamesettings.cpp:32`) and `HighscoreManager` builds `highlevelshistory` and
`highscores` from the same `prefPath` (`highscoremanager.cpp:225-226`,
`:259-260`). SDL3's Emscripten implementation returns `/libsdl/<org>/<app>/`
created with plain `mkdir`
(`android/app/jni/SDL3/src/filesystem/emscripten/SDL_sysfilesystem.c:40-79`),
which is MEMFS. The linked WASM artifact contains **zero** `IDBFS` references,
its only `syncfs` occurrence is the unused API definition, and its only
`localStorage` occurrence is the `fb_nickname` `EM_ASM` read. Nothing mounts a
persistent filesystem and nothing calls `FS.syncfs`.

Therefore, on every page load the browser build starts from defaults: key
bindings, sound and graphics settings, the speed multiplier, the mouse/touch aim
flag, and both highscore files are gone. Only the network nickname survives,
through the four `localStorage` `fb_nickname` sites. `WASM_PORT.md:209` lists
High Scores as working. Severity Medium.

The finding compounds with the shutdown gap: on WASM `RunForEver` returns
immediately after `emscripten_set_main_loop` (`frozenbubble.cpp:238-239`), so
`~FrozenBubble` — and with it `GameSettings::Dispose`'s `SaveSettings` and
`HighscoreManager::Dispose`'s `SaveNewHighscores` — never executes at all.

### REL-005 — 97 dangling absolute symlinks are tracked under `android/app/jni/include/SDL2/`

All 97 tracked entries there are git mode `120000` symlinks whose targets are
absolute paths under `/Users/dericchau/ai/fb2-port/frozen-bubble-sdl2/android/app/jni/`
(four distinct directories: `SDL2/include`, `SDL2_image/include`,
`SDL2_mixer/include`, `SDL2_ttf`). Every one dangles in this worktree, and they
dangle in every clone, because the target path is one developer's private
machine layout. They are also functionally dead: no build file, source file, or
CI step references `jni/include`, and the Android build consumes SDL3 headers
from the four submodules instead.

Consequences that follow from the tracked mode alone: `git archive` and any
release tarball carry 97 broken links; a Windows checkout without symlink
support materializes 97 plain text files containing the private path; and any
tool that follows links (packagers, IDE indexers, `find -L`, backup) errors on
each. They also disclose an unrelated absolute filesystem path in the published
repository. Severity Medium; remediation belongs with Task 9's packaging review.

### REL-006 — stale platform build files and self-contradicting port documentation

Four artifacts describe or define a build that does not exist:

1. `CMakeListsEmscripten.txt` lists **15** sources where the effective set is
   **28**, omitting the 14 named in Step 2 — every `bubblegame_*.cpp` except
   `bubblegame.cpp`, every `mainmenu_*.cpp` except `mainmenu.cpp`, plus
   `netview.cpp`, `netteams.cpp` and `roundstats_color.cpp`. The included
   translation units call methods defined only in the omitted ones, so the
   target cannot link. It also selects SDL **2** ports (`:15-18`) while every
   source includes `<SDL3/…>`, and names its target `frozen-bubble-sdl2`.
2. `WASM_PORT.md` states the build uses SDL2 Emscripten ports (`:7-11`),
   documents an `ASSET()` body that does not match `platform.h:34-45`
   (`:150-159`), claims `__WASM_PORT__` is set automatically by the Emscripten
   toolchain when only the two `CMakeLists` files define it (`:132-134`), and
   requires websockify (`:96-104`) where `web/README.md:17` says none is needed.
3. `android/SETUP.md` instructs the reader to download prebuilt SDL**2**
   libraries into a nonexistent `android-libs/` tree and to copy SDL2's Java
   glue; the real build uses the four SDL3 submodules
   (`android/app/CMakeLists.txt:54-57`). Following it cannot produce a build.
4. `web/index.html` is an unreferenced page titled "Frozen Bubble SDL2" whose
   only script tag loads `frozen-bubble-sdl2.js`, which no build emits; CI
   overwrites `dist-wasm/index.html` with the generated shell instead.

Corroborating stale state: `.gitignore:25-26` still ignores
`android/app/jni/SDL2_mixer/external/{ogg,vorbis}/`, directories the tree no
longer contains. `cmake/Emscripten.cmake` is redundant rather than broken (see
Dismissed candidates) but is the mechanism by which `WASM_PORT.md`'s command
produces a link line with two conflicting spellings of the initial-memory
setting.

Severity Medium — a contributor following the checked-in instructions cannot
build either port. Remediation belongs with Task 9.

### REL-007 — release APK signing and build-input integrity

Three linked gaps, established from the build artifact and the workflow text:

- A local `./gradlew assembleRelease` produces `app-release-unsigned.apk`
  because the `release` build type declares no `signingConfig`
  (`android/app/build.gradle:43-47`). Verified: the artifact has that name and
  contains no v1 signature block.
- CI generates a **new** keystore on every run with
  `keytool -genkeypair … -storepass frozenbubble -keypass frozenbubble`
  (`.github/workflows/build.yml:389-399`) and injects it through
  `-Pandroid.injected.signing.*`. `keytool -genkeypair` derives a fresh RSA
  keypair each invocation, so consecutive release builds are signed by different
  certificates. Android refuses to install an update whose signing certificate
  differs from the installed one, so no shipped APK can ever be upgraded in
  place — each release is effectively a new application requiring uninstall.
  The password is a literal in the public workflow.
- `android/gradle/wrapper/gradle-wrapper.properties` pins
  `gradle-8.2-all.zip` with no `distributionSha256Sum`, so the wrapper
  downloads and executes the distribution with no integrity pin. `gradlew` is
  correctly tracked mode `100755`.

Severity Medium. This gate confirms the state; CI and release remediation belong
to Task 9. No credential, keystore, or signing operation was created or
exercised here.

### REL-008 — an installed macOS build resolves assets to the build machine's source tree

`CMakeLists.txt:223-226` computes `INSTALLED_ASSET_PATH` from
`CMAKE_INSTALL_PREFIX` and logs it, but nothing consumes it: `DATA_DIR` is
defined from `ASSET_PATH`, whose default is `${CMAKE_SOURCE_DIR}/share`
(`:127-128`, `:140`). The desktop `InitDataDir` recovers an installed prefix on
Linux (`/proc/self/exe`, strip at the last `/bin/`) and on Windows
(`GetModuleFileNameA` directory), and on macOS only for `.app` bundles whose
base path ends in `Resources/` (`platform.cpp:109-121`). Every other macOS
layout falls through to the baked `DATA_DIR`.

Reproduced: `strings` on `build-audit-release/frozen-bubble-sdl3` shows the
single baked literal `/Users/dchau/gr/frozen-bubble-sdl3/share`, and the harness
run from a staged `…/usr/local/bin/` resolved `g_dataDir` to that same source
path while the staged `…/usr/local/share/frozen-bubble` was ignored. On the
build machine the game still starts, which is what makes the defect easy to
miss; on any other machine `VerifyAssetDirectory` fails, the constructor sets
`IsGameQuit = true` and returns early — the exact entry to Task 6's reproduced
BUG-034 indeterminate-member dereference.

Two smaller members of the same family, traced but not separately reproduced:
the Linux heuristic also falls through for prefixes with no `/bin/` component
(for example a Debian `/usr/games/` install), and the Windows branch falls
through whenever `GetModuleFileNameA` fails. Severity High for the macOS
`make install` path; Medium overall. Remediation belongs with Task 9.

### IMP-014 — the release APK ships a duplicate and an unused native library

`libpng.so` is byte-identical to `libpng16.so` and carries the same
`SONAME libpng16.so`, so the copy can never be the file `SDL3_image` `dlopen`s;
`libvorbisenc.so` is named by no `DT_NEEDED` entry and by no `dlopen` string in
the APK. Across the three ABIs that is 819,904 + 1,846,824 = **2,666,728** bytes
uncompressed of redundant payload in a 37,290,226-byte APK. Excluding both from
the packaged native libraries is a low-risk, low-effort size reduction.
Priority: Medium benefit / Low effort / Low risk.

### IMP-015 — remove the dead platform layer

Five independently verified pieces of dead platform code:

- `platform.cpp:38-52` — the `ASSET_FILES[]` array (a single `nullptr`) and the
  `extractAssets()` placeholder, compiled only on Android and called from
  nowhere; the real work is `AssetExtractor.java`.
- `logger.cpp:24-27` — the `_WIN32` `#include <direct.h>` and
  `#define mkdir(dir, mode) _mkdir(dir)`; `logger.cpp` never calls `mkdir`, and
  the macro leaks into anything that includes the header after it.
- `shaderstuff.h:24-26` and `:28` — the `_WIN32` `bzero` macro and the
  unconditional `#include <iconv.h>`, both existing solely for
  `shaderstuff.cpp:1332-1360`, which is entirely inside a block comment. The
  include is a gratuitous portability requirement on every target.
- `AdsManager.init()` (`AdsManager.java:48-61`) has zero callers; only its own
  javadoc mentions it. Same unreachable-code class as IMP-012, which this
  extends rather than duplicates.
- `__ANDROID_PORT__` — one definition (`CMakeLists.txt:135`, in a branch the
  shipping Android build never evaluates) and one consumer
  (`gamesettings.h:86`, whose `__ANDROID__` disjunct already selects correctly).

Priority: Medium benefit / Low effort / Low risk.

### Extensions to existing findings — no new IDs allocated

- **REL-004** (version drift) now also covers the client: `platform.h:23`
  defines `APP_VERSION "v2.4.26"`, used once at `mainmenu_panels.cpp:471` to
  label the key-configuration panel, while the audited tag is `v2.4.27` and the
  APK this gate built reports `versionName 2.4.27`. Three different version
  strings now ship in one release.
- **BUG-033** (`system("pkill -x fb-server")`) gains its platform dimension:
  the call is reachable only on macOS and Linux, because `StartLocalServer` is a
  diagnostic stub on `__ANDROID__ || __WASM_PORT__ || _WIN32`
  (`mainmenu_server.cpp:80-83`). On the two platforms where it does run,
  `system()`'s return value is discarded, so a missing or failing `pkill` is
  indistinguishable from a successful one; `pkill` is not a POSIX-mandated
  utility, so a minimal container image can lack it entirely; and the process
  filter is by name only, with no ownership or parentage test. Nothing here
  changes the severity — it explains the blast radius. Deliberately not
  reproduced, as in Task 6, because running it kills processes on this host.
- **REL-003** (Windows socket narrowing and blocking receive) gains its
  platform-guard side: `CMakeLists.txt:38` excludes the `server/` subdirectory
  on `WIN32 OR MINGW OR EMSCRIPTEN`, so this project's own build can never
  produce the Windows server that `server/win32_compat.h` exists to support;
  the Windows *client* is built and links `ws2_32` (`:155-157`). The client-side
  `MSG_DONTWAIT` no-op and missing `FIONBIO` that Task 4 confirmed are therefore
  the only Windows socket surface this build actually ships.
- **IMP-005 / IMP-006 / IMP-008 / IMP-009** — the Task 8 file slices are closed
  without promotion. `platform.cpp`, `platform.h`, `main.cpp`, `logger.cpp` and
  `logger.h` declare no state-bearing member without an initializer
  (`g_dataDir` is a `std::string`; `Logger`'s two statics are explicitly
  initialized at `logger.cpp:29-30`), perform no narrowing numeric conversion,
  and contain no const/API cleanup beyond the dead code already captured in
  IMP-015.

## Dismissed candidates

Each dismissal traces the consequence, not merely the absence of a failure.

- **`cmake/Emscripten.cmake` breaks the documented configure.** The suspicion
  was that setting `CMAKE_TOOLCHAIN_FILE` to the project's own file would leave
  `EMSCRIPTEN` unset, making `if(EMSCRIPTEN)` false so that
  `find_package(SDL3 REQUIRED)` would fail and `add_subdirectory(server)` would
  run. Directly tested: the documented command exited 0, no `server/` directory
  was generated, `__WASM_PORT__` and `DATA_DIR="/share"` appear in
  `flags.make`, the target suffix is `.html`, and no `SDL3_DIR` cache entry was
  created — so `EMSCRIPTEN` **is** set by `CMAKE_SYSTEM_NAME Emscripten`, and
  every consequence in the proposed chain fails to occur. The file is redundant
  with the root `CMakeLists.txt`, and the one real artifact of using it is a
  link line carrying `TOTAL_MEMORY=268435456` four times beside
  `INITIAL_MEMORY=16777216`; both name the same setting, so the outcome is
  decided by argument order rather than by a conflict the linker rejects. That
  fragility is recorded as REL-006 evidence; the "breaks the build" claim is
  withdrawn.
- **`-sDISABLE_EXCEPTION_CATCHING=0` is link-only, so `try`/`catch` is
  miscompiled.** `grep -rn 'catch *(' src/` finds zero live handlers in the
  entire tree (Task 6's BUG-032 exists precisely because `stoi`/`stof` are
  called *without* handlers). With no handler to miscompile, an uncaught throw
  aborts on Emscripten exactly as it terminates natively, so the flag placement
  changes no observable behavior for this program. Recorded as an observation.
- **COOP/COEP is claimed "required for audio (SharedArrayBuffer)".** No build
  file requests `-pthread`, `USE_PTHREADS`, `AUDIO_WORKLET`, or shared memory,
  so SharedArrayBuffer is not a prerequisite of this artifact. The consequence
  of the surplus headers is that cross-origin subresources would need CORP —
  and this build has none, since every asset is bundled in the `.data` file
  loaded from the same origin. The documentation rationale is inaccurate; the
  configuration is harmless. Not promoted.
- **`package="org.frozenbubble"` in the manifest breaks AGP 8.** Observed
  verbatim in the build log: AGP 8.2 emits "Setting the namespace via the
  package attribute in the source AndroidManifest.xml is no longer supported,
  and the value is ignored" and continues. The ignored value is identical to the
  `namespace` in `build.gradle`, so the merged manifest, `applicationId`, and
  generated `R` package are unaffected — `output-metadata.json` confirms
  `applicationId org.frozenbubble`. A warning to clean up, not a defect.
- **`android/gradlew` is not executable in a fresh clone.** The CI `chmod +x
  gradlew` step suggested a missing mode bit. `git ls-files -s` shows
  `100755 … android/gradlew`, so git restores the executable bit on checkout and
  the CI step is redundant rather than compensatory. No consequence exists.
- **`<iconv.h>` is unavailable on the Android NDK.** The header is present in
  the NDK 25.2.9519653 sysroot; its functions are declared
  `__INTRODUCED_IN(28)`, which restricts *use*, not inclusion, and the only use
  in the tree is inside a block comment. The Android build compiled
  `shaderstuff.cpp` for all three ABIs with no diagnostic, confirming the header
  resolves at `minSdk 21`. The include remains dead weight (IMP-015) but breaks
  nothing on any built platform.

## Coverage

Task 8 assigned a final disposition to every file the brief names, and to every
vendored path the coverage ledger had left in a bootstrap state. See
[FILE_COVERAGE.md](../FILE_COVERAGE.md) for the per-row record.

Reviewed in full and closed: `src/main.cpp`, `src/platform.cpp`,
`src/platform.h`, `src/logger.cpp`, `src/logger.h`; the platform slices of
`src/frozenbubble.cpp`/`.h` (which completes those two rows — Tasks 6, 7 and 8
together); `CMakeListsEmscripten.txt`, `cmake/Emscripten.cmake`,
`android/app/CMakeLists.txt`, `android/app/build.gradle`,
`android/build.gradle`, `android/settings.gradle`, `android/gradle.properties`,
`android/gradle/wrapper/gradle-wrapper.properties`, `android/gradlew`,
`android/gradlew.bat`, `android/app/src/main/AndroidManifest.xml`,
`android/app/src/main/res/values/strings.xml`, the four
`org/frozenbubble/*.java` files, `android/SETUP.md`, `WASM_PORT.md`,
`web/index.html`, `web/shell.html`, `web/README.md`.

Boundary-reviewed and closed: the 97 `android/app/jni/include/SDL2/*` symlinks
(link mode, target, dangling state, and zero references established — REL-005),
the 11 `org/libsdl/app/*.java` files (byte-identical to SDL3 3.4.4), the 4 SDL
gitlinks (versions and full recursive initialization), the 4 duplicated
`android/app/jni/iniparser/*` files (byte-identical to `third_party/`), and the
three binary assets `gradle-wrapper.jar`, `tv_banner.png`, `ic_launcher.png`
(validated through the successful build that consumed them, plus the wrapper's
missing checksum pin under REL-007).

Read as evidence but owned elsewhere: `.github/workflows/build.yml` (Task 9;
used here only as the definition of the Android/WASM CI paths),
`src/socket_compat.h` and `src/networkclient*.cpp` (Task 4),
`src/gamesettings.*` and `src/highscoremanager.*` (Task 6),
`src/transitionmanager.cpp`, `src/audiomixer.cpp` and `src/shaderstuff.h`
(Task 7), `src/mainmenu*.cpp` (Task 6), `netlify.toml` and `tools/ports/*.py`
(Task 9).

No file in Task 8's scope is left pending.

## Limitations

- **No browser runtime was exercised.** The audit's scope restriction forbids
  starting network listeners, and an Emscripten `--preload-file` bundle cannot
  be loaded over `file://`. The WASM result is therefore a **successful full
  build and artifact analysis only**; the browser runtime status is
  **unavailable, not passed**. No page was rendered, no frame was drawn, and no
  console output was collected.
- **The WebSocket-proxy connection in brief Step 5 was not performed** and is
  out of scope by direction. No websockify, no `fb-server`, no listener and no
  client connection of any kind was created in this gate.
- **No Android device or emulator was driven.** The APK was built and its
  contents analyzed statically; it was never installed or launched. BUG-046,
  the extraction ordering, the `Process.killProcess` teardown, the TV remote
  input path, and the AdMob/Billing flows are all code-supported inferences.
- **Linux and Windows were not executed.** Their `InitDataDir` branches,
  `GetModuleFileNameA` failure handling, the `/usr/games`-style prefix gap, and
  the packaged DLL layout are source traces plus CI job definitions; the
  packaged-path harness ran only on macOS arm64.
- **REL-007 was confirmed from the artifact and the workflow text only.** No
  keystore was generated, no APK was signed, no Gradle distribution was
  re-downloaded, and no release credential was touched.
- **REL-008's non-macOS members are unreproduced.** Only the macOS
  staged-install case was run; the Linux `/usr/games` and Windows
  `GetModuleFileNameA`-failure variants are traced statically.
- **`AdsManager`'s ad-serving consequence is deliberately unclaimed.** That
  `AdsManager.init()` is unreachable is proven by reference count; whether
  `InterstitialAd.load` self-initializes despite the disabled
  `MobileAdsInitProvider` depends on Play Services behavior that was not
  executed, so no ad-delivery failure is asserted.
- **Per the user's scope restriction, no security-specific runtime test was
  run.** The dangling-symlink path disclosure (REL-005), the repo-visible
  keystore password and unpinned Gradle distribution (REL-007), and the test
  AdMob identifiers are documented statically. The omitted checks are
  limitations, not passes.
- **No sanitizer was used in this gate.** The packaged-path harness linked the
  warnings-strict (`build-audit-werror`) production objects, not the
  ASan+UBSan ones, because every finding here is a path-resolution or
  file-lifecycle question rather than a memory-safety one. No leak or
  memory-safety claim is made anywhere in this notebook; Apple ASan's inability
  to detect leaks on this host therefore does not bear on any Task 8
  conclusion.

## Gate conclusion

**Complete.** All eight brief steps executed. The Android release build ran
locally to success with zero tracked-file drift and required no restoration; the
WASM build linked completely against a disposable, port-patched Emscripten copy
that left the system installation untouched; the packaged-path, logger, and
preference behaviors were reproduced against unchanged production objects.

Nine new IDs were registered — BUG-046, BUG-047, BUG-048, REL-005, REL-006,
REL-007, REL-008, IMP-014, IMP-015 — and REL-003, REL-004 and BUG-033 were
extended rather than duplicated. Six candidates were dismissed with recorded
counter-evidence that traces the consequence, including one, `cmake/Emscripten.cmake`,
whose proposed failure chain was directly disproved by running the command.

No candidate remains open, no scoped or vendored file is undispositioned, and
every omitted platform or scenario is recorded above as a limitation rather than
as a pass.

Next gate: Task 9 (build, tests, packaging, CI, deployment, tooling, and
operations).
