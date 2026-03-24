# Frozen-Bubble: SDL2
<p align="center">
  <img src="https://github.com/user-attachments/assets/c68db5c9-7e72-4d19-8e98-c598a3f5e54e">
</p>

A C++ / SDL2 port of the classic [Frozen Bubble 2](http://www.frozen-bubble.org/), reimplementing its gameplay, network multiplayer, and chain reaction system. Targets desktop (Linux/macOS/Windows), Android TV, and WebAssembly.

---

## Game Modes

### Single Player
Classic Frozen Bubble gameplay — 100 levels, scoring, chain reactions.

### 2-Player Local
Two players on the same keyboard:
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
| Colors per player | Number of bubble colors per player's board (5–8, default 7). The optional 8th color is orange. |
| Rows collapse per player | Disable rows dropping down for a specific player (rows never advance) |
| Aim guide per player | Show a trajectory preview for a specific player's shots |

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

See [`android/SETUP.md`](android/SETUP.md) for build instructions.

Controller mapping:

| Button | Action |
|---|---|
| D-pad Left/Right | Aim |
| A | Fire / Select |
| B | Back |
| Start | Pause |

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
- **Network lobby visuals** — Lobby room list and player list rendering has layout gaps (incomplete polish).
- **WebAssembly: public server list** — Fetching the public server list is not implemented in the Wasm build (requires `emscripten_fetch`).

---

## TODO

- [ ] Implement single-player malus targeting to match original Perl behavior
- [ ] Polish network lobby room/player list layout
- [ ] WebAssembly: implement public server fetch via `emscripten_fetch`
- [ ] Sign macOS `.app` bundle for Gatekeeper compatibility
- [ ] Sign Windows installer for SmartScreen compatibility

---

## Credits

Original Frozen Bubble by [Guillaume Cottenceau et al.](http://www.frozen-bubble.org/) — GPL licensed.
This port is independently developed and not affiliated with the original project.
