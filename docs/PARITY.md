# Compared with the original

What this port adds over the original Frozen Bubble 2, what it fixes in the
original's own code, and what it reproduces unchanged.

## What counts as "not in the original"

Two things qualify, and they're different:

- **Features this port adds.** New gameplay, modes and platforms the original
  never had.
- **Fixes to the original's code.** The server in [`server/`](../server/) is the
  original `fb-server`, vendored — the C sources still carry
  `Copyright (c) 2004-2012 Guillaume Cottenceau`. Defects found and fixed in
  those files are defects the original still has.

**What is deliberately excluded:** bugs introduced by this C++ rewrite and then
fixed. There are plenty, and they're all in the [changelog](../CHANGELOG.md) —
but the original never had them, so listing them here as improvements over it
would be dishonest. A fix only appears below if it lands in original code.

---

## Features added

### Game modes

| Feature | Added |
|---|---|
| **Clear Mode** — first to clear their board wins the round; last survivor also wins. Defaults to row compression off and malus disabled, both overridable | v2.4.26 |
| **Team Mode** — 2–5 teams; malus only lands on living opponents outside your team. Per-player grid in rooms of 5 or fewer, dedicated roster above that | v2.4.26 |
| **Malus disable** — turn attack bubbles off entirely, independently of the mode | v2.4.26 |

### Multiplayer beyond five players

The original capped network games at five players. This port raises that to
twenty, which needed new UI to be playable at all:

| Feature | Added |
|---|---|
| **Rooms of 5, 10 or 20 players** | v2.4.26 |
| **Auto-ranked opponent view** — four boards on screen, held on whoever matters most: targeting you, then attacking, then in danger, then anyone alive. **Tab** pages manually | v2.4.26 |
| **Slot-relative targeting** — keys **1–4** target whoever occupies that view slot, **0** returns to random | v2.4.26 |
| **Attack flash** — a board that has actually been hit flashes a border | v2.4.26 |
| **Kill tracking** — whoever last attacked a player is credited when they're eliminated (**KO** column) | v2.4.26 |
| **Spectate mode** — after elimination the same keys pin an opponent's board instead of picking a target | v2.4.26 |

### Feedback and information

| Feature | Added |
|---|---|
| **Post-round stats table** — bubbles fired and popped, malus sent and received, per player. Network clients broadcast their own numbers so everyone sees exact figures for everyone | v2.4.24 |
| **Lobby match summary** — the host posts final standings to lobby chat when a match ends | v2.4.24 |
| **Incoming-malus indicator** — a fading toast naming who attacked you and how many bubbles they sent; repeated hits aggregate | v2.4.24 |
| **Aim guide** — per-player trajectory preview | v2.3.0 |
| **Performance overlay** (**F3**) — frame rate, frame-time range, and effective game speed against the configured speed | v2.4.30 |
| **Lobby refresh** — persistent chat dock, scrollable room cards with player count and cap, online-player sidebar | v2.4.26 |

### Controls and settings

| Feature | Added |
|---|---|
| **Mouse and touch aiming**, host-controlled per room and synced to all players | — |
| **Controller support** with per-player rebinding of any button, and one-click restore of defaults | v2.3.1 |
| **Game speed setting**, 1.0–5.0×, saved per device | v2.4.12 |
| **Frame-rate-independent movement**, so the game runs at the same speed regardless of display refresh | v2.4.9 |
| **Sound toggle**, **fullscreen toggle**, **saved nickname** | v2.4.15, v2.4.24, v2.4.16 |

### Platforms

The original was Linux-only.

| Platform | Notes |
|---|---|
| **macOS** (Apple Silicon), **Windows**, **Linux** | Native builds |
| **Android TV** | Controller-first UI, on-screen keyboard handling |
| **Browser** (WebAssembly) | Runs on desktop and mobile, including iPhone. Browser clients reach the same server as native ones over WebSocket, so they play together |
| **Browser saves** | Settings, key bindings, level history and high scores persist across a reload via IndexedDB (v2.4.34) |

---

## Fixes to the original server

All of these are in original `fb-server` code and are still present upstream.

| Fixed | What was wrong | Release |
|---|---|---|
| **Crash on simultaneous disconnects** | Tearing down a room recursively freed the game while an outer frame was still using it (`game.c`). On a normal build this corrupted whichever branch it read next and could write a bogus win to the stats file; under a sanitizer it aborted the whole process, taking every unrelated room down with it | v2.4.28 |
| **Player impersonation** | The server relayed each in-game message with the sender byte exactly as the client wrote it, so any client could claim to be any other player in its room — or the room leader. Every relayed message is now stamped with the seat the server assigned | v2.4.29 |
| **One stray message could kill the server** | A connection left in a room that had closed or kicked it could terminate the entire server process with its next in-game message. Only that connection closes now | v2.4.29 |
| **Malformed LAN discovery packet** | A full-length discovery datagram made the server read past the end of its receive buffer | v2.4.28 |
| **Silent privilege-drop failure** | Started with `-u`, a failed switch to the requested user was ignored and the daemon carried on with full privileges, keeping its supplementary groups. It now refuses to start | v2.4.29 |
| **Unchecked master-server reply** | A hostile or broken master-server response could steer the server's own buffer arithmetic; the length is range-checked before use | v2.4.29 |
| **Busy discovery port aborted startup** | If anything else held the LAN discovery port the server refused to start at all. It now serves games normally and reports only that broadcast discovery is unavailable | v2.4.31 |
| **Lobby free-player count** | `LIST` reported a `free:` count that contradicted the open-player list in the same message, counting players seated in not-yet-started rooms as free | — |

---

## Reproduced from the original

| Feature | Status |
|---|---|
| 100 single-player levels | ✅ |
| Chain reaction system (cascading pops) | ✅ |
| Malus (attack bubble) system | ✅ |
| 2–5 player network multiplayer layouts | ✅ |
| Network protocol (`fb-server` + client messages) | ✅ |
| LAN auto-discovery (UDP broadcast) | ✅ |
| Public server list | ✅ |
| Geolocation dots on the world-map lobby | ✅ |
| In-game chat | ✅ |
| Victories limit | ✅ |
| Per-player color count (5–8 colors) | ✅ |
| Row compression toggle per player | ✅ |
| Single-player targeting (malus focus) | ✅ |
| Continue when players leave | ✅ |
| Multiplayer training mode | ✅ |

---

## Not supported

| | |
|---|---|
| **Local multiplayer above 4 players** | `LocalMultiplayerSettings` clamps the count to 2–4. Use a network room for 5 and above |
| **Intel macOS** | Releases are Apple Silicon only; Intel Macs can build from source or play in the browser |
