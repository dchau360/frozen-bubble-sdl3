# Frozen-Bubble: SDL3
<p align="center">
  <img src="https://github.com/user-attachments/assets/c68db5c9-7e72-4d19-8e98-c598a3f5e54e">
</p>

A C++ / SDL3 port of the classic [Frozen Bubble 2](http://www.frozen-bubble.org/), reimplementing its gameplay, network multiplayer, and chain reaction system. The original was Linux-only; this port runs on **Linux, macOS, Windows, Android TV, and in the browser**.

The original was written in Perl; this is a full rewrite in C++. Core gameplay and the network protocol are faithfully reproduced, but edge-case mechanics may still differ — bug reports are welcome via [GitHub Issues](https://github.com/dchau360/frozen-bubble-sdl3/issues).

---

## Download

Latest builds are on the [releases page](https://github.com/dchau360/frozen-bubble-sdl3/releases/latest), and on [itch.io](https://dchau360.itch.io/frozenbubble2).

| Platform | Download | Notes |
|---|---|---|
| **Linux** | `frozen-bubble-linux-x86_64.AppImage` | `chmod +x` and run |
| **macOS** | `frozen-bubble-macos-arm64.dmg` | **Apple Silicon only** — see [macOS notes](#macos-notes) |
| **Windows** | `frozen-bubble-windows-setup.exe` | Unsigned; SmartScreen will warn |
| **Android TV** | `frozen-bubble-android-tv.apk` | Or sideload — see [Android TV](#android-tv) |
| **Browser** | [Play on itch.io](https://dchau360.itch.io/frozenbubble2) | Works on desktop and mobile, including iPhone |

**iOS** has no download: the build exists but is experimental and unsigned, so it
must be re-signed before a device will install it. Build it yourself with
`tools/build-ios.sh` — see [docs/IOS.md](docs/IOS.md). To just play on an iPhone,
use the browser build above.

Building from source: [docs/BUILDING.md](docs/BUILDING.md).

---

## Game Modes

**Single player** — 100 levels, scoring, chain reactions.

**Local multiplayer (2–4 players)** — same keyboard or controllers. Player 1 uses the arrow keys with Up to fire; player 2 uses C/X/V with D to fire.

**Network multiplayer (2–20 players)** — LAN or internet, using the included server. Rooms hold 5, 10, or 20 players.

All three game modes are available in both local and network play, chosen by the host:

- **Classic** — standard chain-reaction gameplay; last player or team standing wins.
- **Clear Mode** — first player to clear their entire board wins the round (the last survivor also wins). Malus and row compression are off by default here, though the host can override both.
- **Team Mode** — players split into teams; malus only lands on living opponents outside your team.

The host configures malus, chain reactions, victories limit, per-player colours and aim guides, mouse/touch aim and more from the game room — all joined players see changes live. Rooms above 5 players get battle-royale UI: four opponent boards on screen at a time, ranked by who's most relevant to you, with **Tab** to page manually, keys **1–4** to target a visible opponent, and a spectate mode after you're knocked out.

After each round a per-player stats table shows bubbles fired and popped, malus sent and received, and kills. In-game chat works during play (**Enter** or **T**, gamepad **X**) and between rounds.

---

## Controls

| Action | Keyboard | Mouse | Touch |
|---|---|---|---|
| Aim | Left / Right arrow | Move mouse | Tap left / right half, or drag |
| Fire | Up arrow or Space | Left click | Tap centre, or tap target |
| Back / pause | Escape | Right click | Swipe left |

In menus on touch devices: tap to select, swipe up/down to scroll, swipe left to go back.

Mouse and touch aiming are enabled per-room by the host, and change the in-game gestures above: with them off, tapping a screen half aims that way; with them on, you drag to aim and tap to fire at a point.

> **Fairness:** free-angle mouse/touch aim is easier than keyboard left/right aiming. In network games the host should set this consistently so everyone plays with the same scheme.

**Controllers** (Android TV and desktop): D-pad left/right aims, **A** or D-pad up fires and selects, **B** goes back, **Start** pauses. Rebind anything under Settings → Keys, per player — navigate with up/down, press Enter, then press the button you want. **Reset ctrl defaults** restores that player's defaults.

---

## Playing in the browser

The browser build runs anywhere with WebAssembly support. Settings, key bindings, level history and high scores are saved in the browser and survive a reload, scoped to that browser profile and the exact origin — they don't follow you to another browser, device, host or port. In a private window, or where site storage is blocked, the game falls back to session-only defaults and logs a diagnostic to the console. Details in [web/README.md](web/README.md#saved-data).

---

## Network play

**Run a server:**

```bash
./start-server.sh          # default port 1511
./start-server.sh -p 1234  # custom port
./start-server.sh -d       # debug output
```

This enables both TCP (direct connections) and UDP broadcast (LAN auto-discovery).

**Host a game:** start the server, then launch the game and pick **LAN Game** (auto-discovers) or **Net Game** (enter an IP). Create a room and wait for players.

**Join:** **LAN Game** to auto-discover on your network, or **Net Game** and enter the host's IP. Any player pressing Enter after a round ends starts the next round for everyone.

**Public servers** are listed in the [frozen-bubble-servers](https://github.com/dchau360/frozen-bubble-servers) repo, in the same format as the original `frozen-bubble.org` list:

```
# host port
myserver.example.com 1511
```

The game fetches this list automatically on startup. Submit a PR there to add yours.

---

## Android TV

Sideload with the **Downloader** app from the Amazon Appstore: enter code **1308098** (or the URL `http://aftv.news/1308098`) and follow the prompts.

**Entering text** (IP address, nickname): when a field is active the on-screen keyboard appears. If **Delete/Clear** doesn't respond straight away, press any letter key first — the field is then active, and Delete will erase both it and the existing text.

---

## macOS notes

Releases are **Apple Silicon (arm64) only** and will not launch on an Intel Mac. Intel users can [build from source](docs/BUILDING.md) or play in the browser. Universal binaries would mean building SDL3 and every dependency for both architectures, since Homebrew ships single-architecture bottles.

The app is ad-hoc signed but not notarized, so macOS may report it as "damaged and can't be opened". Strip the quarantine flag:

```bash
xattr -cr /Applications/FrozenBubble.app
```

Or right-click the app → **Open** → **Open** to bypass Gatekeeper once.

---

## More

- [Changelog](CHANGELOG.md) — release history
- [Building from source](docs/BUILDING.md) — all platforms, including WebAssembly, Android and iOS
- [iOS notes](docs/IOS.md) — experimental unsigned build, and how to sign it
- [Compared with the original](docs/PARITY.md) — features added, fixes to the original's own server code, and what's reproduced unchanged
- [Browser build notes](web/README.md) — saved data, Emscripten port status, serving locally

---

## Credits

Original Frozen Bubble by [Guillaume Cottenceau et al.](http://www.frozen-bubble.org/) — GPL licensed.
This port is independently developed and not affiliated with the original project.

### Third-party components

- [iniparser](https://github.com/ndevilla/iniparser) by Nicolas Devillard — MIT licensed, vendored in `third_party/iniparser/` and compiled into every release artifact. See [LICENSE](third_party/iniparser/LICENSE) and [PROVENANCE.md](third_party/iniparser/PROVENANCE.md).
- SDL3, SDL3_image, SDL3_mixer, SDL3_ttf — zlib licensed, linked at build time.
