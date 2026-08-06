# Feature parity with the original

What this port implements from the original Frozen Bubble 2 Perl source, and
what it adds beyond it.

## Ported from the original

| Feature | Status |
|---|---|
| 100 single-player levels | ✅ |
| Chain reaction system (cascading pops) | ✅ |
| Malus (attack bubble) system | ✅ |
| 2–5 player network multiplayer layouts | ✅ |
| Network protocol (fb-server + client messages) | ✅ |
| LAN auto-discovery (UDP broadcast) | ✅ |
| Public server list (desktop) | ✅ |
| In-game chat | ✅ |
| Victories limit | ✅ |
| Per-player color count (5–8 colors) | ✅ |
| Row compression toggle per player | ✅ |
| Single-player targeting (malus focus) | ✅ |
| Continue when players leave | ✅ |
| Multiplayer training mode | ✅ |
| Single-player malus targeting logic | ✅ |

## Added beyond the original

| Feature | Status |
|---|---|
| macOS, Windows, Android TV, WebAssembly | ✅ (original was Linux-only) |
| Aim guide (trajectory preview) | ✅ |
| Clear Mode (win by clearing your board) | ✅ |
| Team Mode (2–5 teams) | ✅ |
| Malus attacks toggle (disable entirely) | ✅ |
| Network multiplayer beyond 5 players (up to 20) | ✅ |
| Battle royale opponent auto-view, kill tracking, spectate mode | ✅ |
| Geolocation dots on world map lobby | ✅ |
| Browser saves that survive a reload (IDBFS) | ✅ |

## Local multiplayer

Local (same-machine) multiplayer supports **2 to 4 players** on keyboard and
controllers — `LocalMultiplayerSettings` clamps the player count to that range.
Network rooms are the path to 5 and above.

| Feature | Status |
|---|---|
| Local multiplayer, 2 players | ✅ |
| Local multiplayer, 3–4 players | ✅ |
| Local multiplayer, 5 players | ❌ not supported — use a network room |
