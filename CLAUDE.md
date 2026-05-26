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

Serve WASM with COOP/COEP headers (required for audio):
```bash
python3 dist-wasm/serve.py   # or the inline python3 snippet in README
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

### Platform abstraction

`__WASM_PORT__` guards all browser-incompatible code (TCP sockets, server hosting, UDP discovery). `__ANDROID_PORT__`/`__ANDROID__` guards Android asset extraction. The `ASSET(relpath)` macro in `platform.h` prepends `g_dataDir` to asset paths; `g_dataDir` is set at startup by `InitDataDir()`.

Assets live in `share/` (gfx, snd, data, fonts) and are referenced via `ASSET("/gfx/...")`. In WASM they're preloaded at `/share` via Emscripten's `--preload-file`.

### Network protocol

Server is the original `fb-server` (C, in `server/`). Protocol is line-based text over TCP/WebSocket:
- Lobby commands: `NICK`, `LIST`, `CREATE`, `JOIN`, `START`, `PART`, `TALK`
- In-game messages: `GAMEMSG` prefix wrapping single-char opcodes — `f` (fire), `s` (stick/place), `g` (malus attack), `m`/`M` (bubble sync), `F` (game over/win), `n` (ready for next round), `l` (player left), `o` (options), `r` (targeting)

The leader (game creator) is authoritative for level generation and sends bubble positions to joiners via `b|`/`N`/`T` sync messages during `SyncNetworkLevel()`.

### Original Perl source (for verification)

When implementing or debugging game mechanics, compare against the original Perl source at:
- `bin/frozen-bubble` (~2500 lines) — main game loop, collision, chain reactions, malus, win conditions
- `lib/Games/FrozenBubble/Net.pm` — original network protocol

Location: `/Users/dericchau/ai/fb2-port/frozen-bubble-sdl2/`

Key line references: malus formula (line 958), chain reactions (819–841), win sync `F` message (1943), `real_stick_bubble` (731), living players (600).

### CI / release

`.github/workflows/build.yml` builds Linux (AppImage), macOS (DMG), Windows (NSIS installer), Android (APK), and WASM on every push to `main` and on `v*.*.*` tags. Tag pushes also deploy to itch.io via Butler. No automated tests — verification is manual gameplay testing.
