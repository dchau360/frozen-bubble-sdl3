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

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for release history.

---

## Known Issues

- **Single-player malus targeting** — Attack bubbles in single-player mode use a placeholder targeting strategy; not yet matched to original game behavior.

---

## TODO

- [ ] Implement single-player malus targeting to match original Perl behavior
- [ ] Sign macOS `.app` bundle for Gatekeeper compatibility
- [ ] Sign Windows installer for SmartScreen compatibility

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
