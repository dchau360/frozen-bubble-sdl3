# Frozen-Bubble: SDL2
<p align="center">
  <img src="https://github.com/user-attachments/assets/c68db5c9-7e72-4d19-8e98-c598a3f5e54e">
</p>

A C++ / SDL2 port of the classic [Frozen Bubble 2](http://www.frozen-bubble.org/), reimplementing its gameplay, network multiplayer, and chain reaction system. The original was Linux-only; this port targets **Linux, macOS, Windows, Android TV, and WebAssembly**.

> **Note:** The original game was written in Perl. This is a full rewrite in C++. While core gameplay and network protocol are faithfully reproduced, there may be inconsistencies and bugs compared to the original — particularly in edge-case game mechanics. Bug reports are welcome via [GitHub Issues](https://github.com/dchau360/frozen-bubble-sdl2/issues).

**Play in browser:** [dchau360.itch.io/frozenbubble2](https://dchau360.itch.io/frozenbubble2)

---

## Game Modes

### Single Player
Classic Frozen Bubble gameplay — 100 levels, scoring, chain reactions.

### 2-Player Local
Two players on the same keyboard or controllers:
- Player 1: Arrow keys + Up to fire
- Player 2: C/X/V + D to fire

### Network Multiplayer (2–5 players)
Play over LAN or internet using the included server. Supports chain reactions, malus (attack bubbles), win tracking, and 2–5 player layouts.

The lobby displays a world map with colored dots showing where each connected player is located. Your location is detected automatically at connect time and shown as an animated pulsing dot.

The host can configure the following settings in the game room — all joined players see updates in real time:

| Setting | Description |
|---|---|
| Chain reaction | Toggle cascading chain reactions on/off |
| Continue when players leave | Keep the game going if a player disconnects |
| Single player targeting | All malus attacks target one player instead of spreading |
| Victories limit | Number of round wins needed to win the match (or unlimited) |
| Max colors per player | Maximum number of bubble colors per player's board (5–8, default 7). The optional 8th color is orange. |
| Rows collapse per player | Whether rows drop down periodically for a specific player (on by default; set to off to disable) |
| Aim guide per player | Show a trajectory preview for a specific player's shots |

The last three settings are shown as a compact grid (P1–P5 columns) that the host navigates with arrow keys and Enter.

---

## Building from Source

### 1. Clone the repo

```bash
git clone https://github.com/dchau360/frozen-bubble-sdl2.git
cd frozen-bubble-sdl2
```

### 2. Install dependencies

**macOS (Homebrew):**
```bash
brew install sdl2 sdl2_image sdl2_mixer sdl2_ttf cmake ninja
```

**Ubuntu / Debian:**
```bash
sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev cmake ninja-build
```

**Windows (MSYS2 MinGW64):**
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image \
          mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-SDL2_ttf
```

> `iniparser` is bundled — no separate install needed.

### 3. Build

```bash
cmake -B build -G Ninja
cmake --build build --parallel
```

### 4. Run

```bash
./build/frozen-bubble-sdl2
```

The server binary (`fb-server`) is built automatically on Linux and macOS alongside the game.

---

## Running a Server

```bash
./start-server.sh          # Start on default port 1511
./start-server.sh -p 1234  # Custom port
./start-server.sh -d       # Debug output
```

`start-server.sh` enables both TCP (direct connections) and UDP broadcast (LAN auto-discovery) by default.

---

## Network Play Quick Start

**Host:**
1. Start the server: `./start-server.sh`
2. Launch the game → **LAN Game** (auto-discovers) or **Net Game** (enter IP)
3. Create a game room and wait for players

**Join:**
1. Launch the game → **LAN Game** to auto-discover on your network
2. Or **Net Game** → enter the host's IP

Any player pressing Enter after a round ends automatically starts the next round for everyone.

---

## Public Server List

Community servers are listed in the [frozen-bubble-servers](https://github.com/dchau360/frozen-bubble-servers) repo — same format as the original `frozen-bubble.org` server list:

```
# host port
myserver.example.com 1511
```

Submit a PR to [frozen-bubble-servers](https://github.com/dchau360/frozen-bubble-servers) to add your server. The game fetches this list automatically on startup.

---

## Android TV

### Installing via Downloader app

The easiest way to sideload on Fire TV:

1. Install the **Downloader** app from the Amazon Appstore
2. Open Downloader and enter code **5985774** (or URL: http://aftv.news/5985774)
3. Follow the prompts to install

See [`android/SETUP.md`](android/SETUP.md) for build-from-source instructions.

Controller mapping:

| Button | Action |
|---|---|
| D-pad Left/Right | Aim |
| A / D-pad Up | Fire / Select |
| B | Back / Exit game |
| Start | Pause |

**Key bindings** — go to Settings → Keys to bind any controller button or key to each action per player. Navigate with UP/DOWN, press ENTER to assign, then press the desired button. Select **Reset ctrl defaults** to restore D-pad + A button defaults for that player's controller slot.

### Entering text (IP address, nickname, etc.)

When a text input field is active, the on-screen keyboard appears. If the **Delete/Clear** key doesn't work immediately:

1. Press any letter key first (e.g. **A**)
2. The field is now active — press **Delete** to erase the letter, then continue deleting the existing text
3. Type the new value and press **Enter** to confirm

---

## macOS: "damaged and can't be opened"

The app is ad-hoc signed but not notarized. If macOS blocks it, strip the quarantine flag in Terminal:

```bash
xattr -cr /Applications/FrozenBubble.app
```

Or right-click the app → **Open** → **Open** to bypass Gatekeeper once.

---

## Known Issues

- **Single-player malus targeting** — Attack bubbles in single-player mode use a placeholder targeting strategy; not yet matched to original game behavior.

---

## TODO

- [ ] Implement single-player malus targeting to match original Perl behavior
- [ ] Sign macOS `.app` bundle for Gatekeeper compatibility
- [ ] Sign Windows installer for SmartScreen compatibility

---

## New in v2.4.8

- **WebAssembly: swap creators fixed** — after a game ends, the server now correctly moves the player's connection back to normal lobby mode (previously left in in-game priority mode), preventing the 5-second in-game timeout and stale duplicate entries when starting a new game with swapped roles
- **WebAssembly: round 2+ sync fixed** — browser client waits for all level-sync messages to arrive before starting each subsequent round (same mechanism as the round 1 fix in v2.4.7); prevents the ~30-second disconnect at the start of round 2
- **Server: version logged at startup** — fb-server now prints its version and protocol to the log on startup, making it easier to confirm which binary is running

---

## New in v2.4.7

- **WebAssembly: game start fixed** — browser client now waits for all 40 level-sync messages to queue up before entering the game loop, preventing the immediate disconnect when a native client hosts and the web client joins

---

## New in v2.4.6

- **Menu animation fix** — graphics quality icon no longer attempts to load missing frames at startup (off-by-one in frame count guard)
- **macOS startup log fix** — suppressed spurious `[ERROR] [DEBUG] Parameter 'texture' is invalid` messages from SDL Metal renderer initialization

---

## New in v2.4.5

- **WebAssembly: join game fixed** — browser client now correctly joins multiplayer game rooms; macOS host can see the web player join
- **WebAssembly: join retry** — JOIN command automatically retries with a name suffix if the nickname is already in use (mirrors the existing CREATE retry behaviour)
- **Game room chat text color** — fixed chat messages appearing yellow on the web client

---

## New in v2.4.4

- **WebAssembly: public server list** — itch.io browser version now fetches and displays the public server list on the Net Game screen
- **Net Game loads instantly** — server list fetch and latency probing moved to a background thread (desktop); browser opens the screen immediately
- **WebAssembly: game creation fixed** — CREATE command now waits for server confirmation before entering the game room; automatically retries with a name suffix if the game name is already taken
- **Max colors** — "Colors" option renamed to "Max colors" in all game setup panels (2P, local multiplayer, LAN, net game)
- **Pause animation fix** — pause penguin animation now loads the correct frames (was off-by-one)
- **Stick effect asset fix** — missing `stick_effect_7-mini.png` added; array bounds corrected

---

## New in v2.3.1

- **Xbox controller support** — fully working in 1P and 2P local modes; bind any button in Settings → Keys
- **Reset controller defaults** — one-click reset to D-pad + A button layout per player in key bindings
- **Bubble centering fix** — all players now land at the same column when shooting straight up
- **Exit button** — replaced High Scores menu button with an Exit App button
- **Net game manual entry** — added visible Connect button; navigate with UP/DOWN, ENTER to select
- **Net game lobby text color** — fixed text appearing all red after a failed connection attempt
- **False local server in Net Game list** — fixed spurious "Local Server" entry appearing when no server is running
- **Net game keyboard** — keyboard no longer auto-opens when entering the manual IP/port entry screen; press ENTER on a field to open it
- **Android TV delete key** — improved backspace handling for text fields on Android 11+

---

## New in v2.3.0

- **Per-player lobby settings grid** — Max colors, Rows collapse, and Aim guide shown as a P1–P5 column grid; host navigates with arrow keys and Enter
- **Aim guide** — trajectory preview toggle per player
- **Row compression toggle** per player — disable rows collapsing for specific players
- **Local multiplayer** — 2 players on controllers (3–5 player local is WIP)

---

## Implemented from Original Perl Version

Features ported from the original Frozen Bubble 2 Perl source:

| Feature | Status |
|---|---|
| 100 single-player levels | ✅ |
| Chain reaction system (cascading pops) | ✅ |
| Malus (attack bubble) system | ✅ |
| 2–5 player network multiplayer layouts | ✅ |
| Network protocol (fb-server + client messages) | ✅ |
| LAN auto-discovery (UDP broadcast) | ✅ |
| Public server list (desktop + WebAssembly) | ✅ |
| In-game chat | ✅ |
| Victories limit | ✅ |
| Per-player color count (5–8 colors) | ✅ |
| Row compression toggle per player | ✅ |
| Single-player targeting (malus focus) | ✅ |
| Continue when players leave | ✅ |
| Multiplayer training mode | ✅ |
| Geolocation dots on world map lobby | ✅ |
| Aim guide (trajectory preview) | ✅ (added beyond original) |
| Local multiplayer (2 players, controllers) | ✅ |
| Local multiplayer (3–5 players, controllers) | ⏳ (WIP) |
| Single-player malus targeting logic | ⏳ (placeholder) |
| macOS, Windows, Android TV, WebAssembly | ✅ (original was Linux-only) |

---

## Credits

Original Frozen Bubble by [Guillaume Cottenceau et al.](http://www.frozen-bubble.org/) — GPL licensed.
This port is independently developed and not affiliated with the original project.
