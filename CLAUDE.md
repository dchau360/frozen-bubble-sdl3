# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

**Native build (macOS/Linux):**
```bash
cmake -B build -G Ninja
cmake --build build --parallel
./build/frozen-bubble-sdl3
```

**Debug build:**
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

**Tests:**
```bash
ctest --test-dir build --output-on-failure
```
Two server tests exercise memory-safety fixes and need a sanitizer build; on an
ordinary build they report as skipped rather than passing without running. To
actually run them:
```bash
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan --parallel
ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --test-dir build-asan --output-on-failure
```
`fast_unwind_on_malloc=0` matters here: the default frame-pointer-based
unwinder breaks the moment a leak's call stack passes through SDL3/FreeType/
HarfBuzz (built without `-fsanitize`, so ASan's malloc interceptor still
catches their allocations via its process-global hook, but can't identify
frames inside them without this) — reports come back with unresolvable
`<unknown module>` frames instead of real call sites. The slow unwinder walks
DWARF CFI (`.eh_frame`, present in every one of these libs since C++
exceptions need it) instead, and can identify the module even when it can't
name the function.

**Server (Linux/macOS only, built alongside game automatically):**
```bash
./start-server.sh          # default port 1511
./start-server.sh -p 1234  # custom port
./start-server.sh -d       # debug output
```

**WASM build** — requires Emscripten SDK + manual port-file patching (see README.md "Building WASM locally"). After patching:
```bash
mkdir build-wasm && cd build-wasm
emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)
```

Serve WASM with COOP/COEP headers (required for audio — without them the page
loads but is silent):
```bash
python3 tools/serve-wasm.py   # finds build-wasm/ or dist-wasm/, prints the URL
```

**Android:** `cd android && ./gradlew assembleRelease` — requires SDL3 git submodules initialized (`git submodule update --init --recursive android/app/jni/SDL3*`).

## Architecture

### Top-level structure

Entry point is `main()` in `src/main.cpp` which calls `FrozenBubble::Instance()->RunForEver()`. Everything flows through the `FrozenBubble` singleton.

**`FrozenBubble`** (`src/frozenbubble.h/.cpp`) — top-level game loop and state machine:
- States: `TitleScreen`, `MainGame`, `LevelEditor`, `Netplay`, `Highscores`
- Owns `MainMenu` and `BubbleGame` instances
- Manages `deltaScale` (frame-rate normalization: 1.0 at 60 fps, multiplied into all per-frame movement)
- WASM uses `RunOneFrame()` via Emscripten's main loop; native uses `RunForEver()` with SDL event pump

**`BubbleGame`** (`src/bubblegame.h/.cpp`) — all gameplay logic:
- Owns `BubbleArray bubbleArrays[5]` — one per player (1–5 players)
- `NewGame(SetupSettings)` initializes the round; `SetupSettings` carries all per-game config (chain reactions, network game flag, player count, color counts, aim guide flags, etc.)
- Drives bubble launching, collision, chain reactions, malus, win/loss detection, and network message processing each frame via `ProcessNetworkMessages()`

**`BubbleArray`** (struct in `bubblegame.h`) — per-player state:
- `std::array<std::vector<Bubble>, 13> bubbleMap` — 13 rows, each a vector of bubbles (the original Perl used a flat list; C++ uses a 2D structure but each row is still a vector to handle multiple bubbles per cell after collision)
- Contains `Penguin`, `Shooter`, input flags, malus queue, nextColors queue, network action flags (`mpFirePending`, `mpStickPending`), and player lifecycle state (`PlayerState::ALIVE/LOST/LEFT`)

**`MainMenu`** (`src/mainmenu.h/.cpp`) — all pre-game screens including network lobby, game room, server list, key bindings panel, and settings.

**`NetworkClient`** (singleton, `src/networkclient.h`) — protocol client with dual implementations:
- Native (`networkclient.cpp`): raw TCP socket, non-blocking `recv()` polled each frame
- WASM (`networkclient_wasm.cpp`): Emscripten WebSocket with async callbacks; requires `websockify` proxy to bridge WebSocket→TCP to `fb-server`
- Both share the same `messageQueue`/`syncQueue` deque interface; `ProcessNetworkMessages()` in `BubbleGame` drives per-frame processing
- `syncQueue` stores bubble-sync messages (`b|`, `N`, `T`) separately so `SyncNetworkLevel()` can retrieve them even when they arrive before the sync call (round 2+ race fix)

### Singleton architecture

All major subsystems are singletons accessed via `::Instance()`: `FrozenBubble`, `GameSettings`, `NetworkClient`, `AudioMixer`, `TransitionManager`, `HighscoreManager`. There is no dependency injection — subsystems call each other's `Instance()` directly. `GameSettings` stores INI-parsed settings (key bindings, audio/gfx, speed multiplier, nickname) to `SDL_GetPrefPath()` via the bundled `third_party/iniparser`.

### Rendering

The game renders to a fixed **640×480 logical canvas** (`WINDOW_W`/`WINDOW_H` in `transitionmanager.h`); SDL3 scales this to the actual window. All stored rects are `SDL_Rect` (int), converted with `ToFRect()` (`src/sdl3_compat.h`) when calling SDL3 render functions that require `SDL_FRect`.

Transition effects (plasma, bars, circles, etc.) live in `src/shaderstuff.cpp` — pixel-manipulation routines ported from the original Perl/C source. `TransitionManager` calls `TakeSnipOut`/`DoSnipIn` to apply them between screens.

### Controller input

Local multiplayer controller input uses a virtual scancode system: physical gamepad buttons are mapped to virtual scancodes starting at `CTRL_SC_BASE` (300), with 20 slots per player. `virtualKeyState[]` and `controllerInputs[5]` globals (in `src/gamesettings.h`) are written by `FrozenBubble::HandleControllerEvent()` and read by `BubbleGame` each frame alongside keyboard state via `IsKeyPressed()`.

### Platform abstraction

`__WASM_PORT__` guards all browser-incompatible code (TCP sockets, server hosting, UDP discovery). `__ANDROID_PORT__`/`__ANDROID__` guards Android asset extraction. The `ASSET(relpath)` macro in `platform.h` prepends `g_dataDir` to asset paths; `g_dataDir` is set at startup by `InitDataDir()`.

Assets live in `share/` (gfx, snd, data, fonts) and are referenced via `ASSET("/gfx/...")`. In WASM they're preloaded at `/share` via Emscripten's `--preload-file`.

### Network protocol

Server is the original `fb-server` (C, in `server/`). Protocol is line-based text over TCP/WebSocket:
- Lobby commands: `NICK`, `LIST`, `CREATE`, `JOIN`, `START`, `PART`, `TALK`, `NOTIFYREG`/`NOTIFYUNREG` (follow-a-server push registration), `REPORT <nick> <reason>` (abuse report, appended to a flat file for the operator; never acted on automatically)
- In-game messages: `GAMEMSG` prefix wrapping single-char opcodes — `f` (fire), `s` (stick/place), `g` (malus attack), `m`/`M` (bubble sync), `F` (game over/win), `n` (ready for next round), `l` (player left), `o` (options), `r` (targeting), `S` (round stats sync: `S{fired}:{popped}:{malusSent}:{malusReceived}`, broadcast once per round so all clients can render the post-round stats table)

The leader (game creator) is authoritative for level generation and sends bubble positions to joiners via `b|`/`N`/`T` sync messages during `SyncNetworkLevel()`.

**Production server deployment:** `docker/docker-compose.yml` runs `fb-server` on TCP 1511 (native clients) plus an nginx container that terminates TLS and proxies WebSocket connections on port 443 (browser/WASM clients). See `SetupServer.md` for SSL certificate setup.

### Original Perl source (for verification)

When implementing or debugging game mechanics, compare against the original Perl source:
- `bin/frozen-bubble` (~2500 lines) — main game loop, collision, chain reactions, malus, win conditions
- `lib/Games/FrozenBubble/Net.pm` — original network protocol

Key line references: malus formula (line 958), chain reactions (819–841), win sync `F` message (1943), `real_stick_bubble` (731), living players (600).

### CI / release

`.github/workflows/build.yml` builds Linux (AppImage), macOS (DMG), Windows (NSIS installer), Android (APK), WASM, and a Linux ASan/UBSan sanitizer job (ships no artifact of its own; exists to catch memory-safety regressions) on every push to `main`, on PRs to `main`, and on `v*.*.*` tags. On tag pushes, `release` packages whichever of the five platform builds actually succeeded that run into a GitHub Release, and `deploy-itchio-html5` publishes the WASM build to itch.io via Butler — itch.io gets only the browser build; downloads for the other four platforms are distributed through the GitHub Release (and Google Play for Android) instead.

A failing ASan/UBSan run hard-blocks both: no platform ships, itch.io included, if it catches a memory-safety bug. A single platform build failing on its own (say, Windows) does *not* block the others any more — `release`'s `if:` opts out of the default needs-failure skip (`!cancelled()`) and just omits that platform's file from the release, so the other four still ship and the failed one stays on whatever version its last successful tag published until it's fixed and re-tagged. Only `release` and `deploy-itchio-html5` are tag-gated (`startsWith(github.ref, 'refs/tags/')`), so pushes and PRs build (and, for six jobs, test) without publishing anything.

Android releases are signed with a persistent key held in repository secrets, and a tagged build fails outright rather than shipping an APK that cannot be upgraded — see `docs/ANDROID_SIGNING.md`.

### Cutting a release

Three files carry the version and must be bumped together:

1. `CMakeLists.txt` — `project(... VERSION x.y.z ...)`. Authoritative: the game's
   `APP_VERSION` (shown in the menu), the server's startup log and its
   master-server user agent all derive from it.
2. `android/app/build.gradle` — `versionName`, and `versionCode` must strictly
   increase or Android refuses the upgrade.
3. `.github/workflows/build.yml` — the two off-tag `version="x.y.z"` fallbacks.

`default.nix` carries a copy too, but nothing ships from it. Then update
`CHANGELOG.md`, commit, and push an annotated `vx.y.z` tag — the tag is what
triggers the release and the itch.io deploys.

Automated tests run under `ctest` (`ctest --test-dir build`). Two of them need a sanitizer build and report themselves as skipped otherwise via `SKIP_RETURN_CODE 77`, rather than passing without running. Gameplay itself is still verified manually.
