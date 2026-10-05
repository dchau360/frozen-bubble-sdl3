# Compared with the original

What this port adds over the original Frozen Bubble 2, what it plays
differently, what it fixes in the original's own code, and what it reproduces
unchanged. The original here is the Perl game in [`bin/frozen-bubble`](../bin/frozen-bubble)
and [`lib/Games/FrozenBubble/`](../lib/Games/FrozenBubble/), and the C server
in [`server/`](../server/).

## The count

**71 differences from the original**, numbered 1–71 below:

| Section | Numbers | How many |
|---|---|---|
| [Features added](#features-added) | 1–58 | 58 |
| [Plays differently](#plays-differently) | 59–63 | 5 |
| [Fixes to the original server](#fixes-to-the-original-server) | 64–71 | 8 |

Numbers run on from one table to the next, so the last row of the last table
is the total. When a difference is added, give it the next free number in its
section, renumber the rows after it, and update the table above.

## What counts as a difference

Three things qualify:

- **Features this port adds.** Gameplay, modes, screens, online services and
  platforms the original never had.
- **Things it plays differently.** Rules and behaviour the original had,
  deliberately changed here.
- **Fixes to the original's code.** The server in [`server/`](../server/) is
  the original `fb-server`, vendored — the C sources still carry
  `Copyright (c) 2004-2012 Guillaume Cottenceau`. Defects found and fixed in
  those files are defects the original still has.

**What is deliberately excluded:** bugs introduced by this C++ rewrite and
then fixed. There are plenty, and they're all in the
[changelog](../CHANGELOG.md) — but the original never had them, so listing
them here as improvements over it would be dishonest. A fix only appears
below if it lands in original code. Small conveniences (a remembered
nickname, a reset-to-defaults button) are left out too, so that the count
measures real differences rather than every changelog line.

---

## Features added

### Game modes and rules

| # | Feature | Added |
|---|---|---|
| 1 | **Clear Mode** — first to clear their board wins the round; last survivor also wins. Defaults to row compression off and malus disabled, both overridable | v2.4.26 |
| 2 | **Race and Timed modes** — Race is first to pop a target number of bubbles (50 by default); Timed is most bubbles popped before the clock runs out (30 seconds by default). Both show a live popped-count HUD | v2.4.95 |
| 3 | **Teams** — a per-player setting available in every mode, not a mode of its own: any player picks a team (1–5) or stays a free agent from the room's Set Teams page, and malus only lands on living opponents outside your team. Hosts get Auto 2/3/4/5 buttons to round-robin every seat across that many teams | v2.4.26 |
| 4 | **Attack bubbles: on, off or Blockable** — off turns malus off entirely, independently of the mode (v2.4.26); Blockable makes the malus you earn pay down what is queued against you first and send only the surplus (v2.4.63) | v2.4.26 |
| 5 | **Local multiplayer above two players** — up to five share one device, on the same centre-plus-four-corners layout a five-player network game uses. The original's local game was exactly two players (`is_2p_game()` in `bin/frozen-bubble` is `@PLAYERS == 2 && !is_mp_game()`); anything more had to be a network game, even sitting at the same machine | v2.4.41 |
| 6 | **Bots**, in local multiplayer and in network rooms, at three skill levels. They aim by flying a probe bubble through the game's own launch physics rather than modelling the board separately, and drive the same shooter controls a player does. A network-room bot joins as an ordinary member with its own connection, so it counts against the room's cap and every other client sees it as a player. A browser bot uses its own WebSocket to the same server rather than a raw socket, so the feature works on every platform. A server operator can cap how many bots may be registered at once, server-wide (`fb-server -b`; see [SetupServer.md](../SetupServer.md#optional--limit-concurrent-bots)) | v2.4.41 |
| 7 | **Online tournaments** — single-elimination brackets for 4–16 human entrants, byes up to the next bracket size, best of three per match, one ruleset for the whole bracket, live bracket screen | v2.4.103 |

### Multiplayer beyond five players

The original capped network games at five players. This port raises that to
twenty, which needed new UI to be playable at all:

| # | Feature | Added |
|---|---|---|
| 8 | **Rooms of 5, 10 or 20 players** | v2.4.26 |
| 9 | **Auto-ranked opponent view** — four boards on screen, held on whoever matters most: targeting you, then attacking, then in danger, then anyone alive. **Tab** pages manually | v2.4.26 |
| 10 | **Slot-relative targeting** — keys **1–4** target whoever occupies that view slot, **0** returns to random | v2.4.26 |
| 11 | **Attack flash** — a board that has actually been hit flashes a border | v2.4.26 |
| 12 | **Kill tracking** — whoever last attacked a player is credited when they're eliminated (**KO** column) | v2.4.26 |
| 13 | **Spectate mode** — after elimination the same keys pin an opponent's board instead of picking a target | v2.4.26 |

### Single player

| # | Feature | Added |
|---|---|---|
| 14 | **Points in a classic game** — the original's 1-player screen shows only "Level N" (its points tally belongs to Multiplayer training). Here every pop and drop scores, the score carries across the levels of one life, and the High Scores screen has a most-points table beside furthest level | — |
| 15 | **Time bonus for a clear**, Bust-a-Move style — 5,000 points for a level cleared in 5 seconds or less, 84 less per second after, nothing from 65 seconds; the level-cleared panel shows the level's time and its bonus | v2.4.134 |
| 16 | **"+N" where a shot lands** — the points each scoring shot earned, floating up from where it hit | v2.4.132 |
| 17 | **Shot count** — the run's shots under the score, kept with each high-score record and on the online Level board | v2.4.128 |
| 18 | **Run time on screen** — the whole run's clock, the one the high-score tables use: TIME in the modern theme's panel (v2.4.133), "Time: 3'07"" under the shots in the original theme (next release) | v2.4.133 |
| 19 | **1-player aim guide** — the bounce line for your shot; a run played with it on is kept out of every high-score table, and START offers to turn it off first | v2.4.132 |
| 20 | **Skip shot** (1-player) — a second fire key (or a right click, or a touch on the strip around the launcher) that shoots the next bubble at once and keeps the loaded one, Bust-a-Move's swap and shot in one press; a run that uses it is kept out of every high-score table | — |
| 21 | **Name prompt** — START asks for a name when online highscores are on and none is set, so runs don't reach the board as `unnamed` | v2.4.124 |

### Look and feel

| # | Feature | Added |
|---|---|---|
| 22 | **Modern in-game theme** (default; **Original** brings back the classic screen) — in a 1-player game, a score panel with level, score counting up, shots and time; cards for level cleared, game over and pause; the next level dropping in row by row; and "N DROPPED!" when a shot cuts bubbles loose | v2.4.133 |
| 23 | **Candy bubbles** — a glossy redraw of the eight colours and their colour-blind versions, as their own setting (**Bubbles: Candy / Classic**), so either set goes with either theme, in every mode | v2.4.133 |
| 24 | **Menu styles** — six looks for the title screen: Classic (the original artwork), Clear, Slate, Ice, Pop and Modern, the in-game theme's dark cards with a candy bubble on the selected row and the default, with every existing player moved to it once; in Modern the 1-player menu is a card too, with a line saying what each mode does and switches for its settings, and the online lobby, the game room, its Set Teams page and the settings panel are drawn as cards (next release) | v2.4.65 |
| 25 | **On-screen pause button** in 1-player games, for touch screens; **P** pauses too, since most keyboards have no Pause key (next release). The original paused on the Pause key only | v2.4.133 |
| 26 | **Sharp text** — on a big monitor, a TV, a Retina Mac or a phone, words are drawn at the screen's own resolution instead of being stretched up from the 640×480 game screen, so scores, cards, popups and menus stay crisp; the board keeps its pixel art. GRAPHICS set to Low brings back the stretched text. Not in the browser build | next release |

### Feedback and information

| # | Feature | Added |
|---|---|---|
| 27 | **Post-round stats table** — bubbles fired and popped, malus sent and received, per player. Network clients broadcast their own numbers so everyone sees exact figures for everyone | v2.4.24 |
| 28 | **Lobby match summary** — the host posts final standings to lobby chat when a match ends | v2.4.24 |
| 29 | **Incoming-malus indicator** — a fading toast naming who attacked you and how many bubbles they sent; repeated hits aggregate | v2.4.24 |
| 30 | **Aim guide** — per-player trajectory preview | v2.3.0 |
| 31 | **Performance overlay** (**F3**) — frame rate, frame-time range, and effective game speed against the configured speed | v2.4.30 |
| 32 | **Lobby refresh** — persistent chat dock, scrollable room cards with player count and cap, online-player sidebar | v2.4.26 |
| 33 | **Platform, input and country badges** — beside each name in a network game: the player's system, and each round whether they shoot with keyboard, mouse, touch or gamepad | v2.4.105 |

### Online services

| # | Feature | Added |
|---|---|---|
| 34 | **Weekly rankings** — each server ranks players by round wins, round losses and bubbles popped for the week (reset Monday 00:00 UTC, bots not counted), shown on a lobby screen, as `#N` badges in the online-player sidebar, and on the web at [/weekly/](https://fb.servequake.com/weekly/) | v2.4.116 |
| 35 | **Online highscores** — one online board for classic single-player runs: furthest level and most points in one life, keyboard vs mouse/touch, all-time and this week, in the game and on the web at [/scores/](https://fb.servequake.com/scores/) | v2.4.120 |
| 36 | **Anonymous player accounts** — a recovery code on the device, signed in by challenge-response, so rankings and highscores belong to a player rather than a name. The code can be copied to another device, replaced, or the account deleted from the game | v2.4.118 |
| 37 | **Discord alerts** (server operator's choice) — a post when a player joins, and per round the result, its length, a win-count chart and bubbles popped, threaded per room | v2.4.101 |
| 38 | **Follow a server** — star a server in the LAN or Net list and be notified when someone joins it | v2.4.37 |
| 39 | **Join our Discord** — a row on the server list and in the lobby that opens the community Discord | — |
| 40 | **Website** — a landing page, privacy policy, the online highscores and the weekly rankings, served by the same nginx as the game's WebSocket proxy and on GitHub Pages | v2.4.116 |

### Players and moderation

| # | Feature | Added |
|---|---|---|
| 41 | **Block and report** — `/block` hides a player's chat (saved per device), `/report` sends a note to the server's operator | v2.4.40 |
| 42 | **Kick** — the room's host can `/kick p2` or `/kick <nick>` | v2.4.41 |
| 43 | **Remembered room settings** — a host's last room setup carries over to the next room, across restarts | v2.4.64 |

### Controls and settings

| # | Feature | Added |
|---|---|---|
| 44 | **Mouse and touch aiming**, host-controlled per room and synced to all players; on by default where there is no keyboard | — |
| 45 | **Controller support** with per-player rebinding of any button, and one-click restore of defaults | v2.3.1 |
| 46 | **Game speed setting**, 1.0–5.0×, saved per device | v2.4.12 |
| 47 | **Frame-rate-independent movement**, so the game runs at the same speed regardless of display refresh | v2.4.9 |
| 48 | **Touch gestures** — tap-to-select in list panels, swipe left to go back or to leave a round | v2.4.35 |
| 49 | **Sound toggle**, **fullscreen toggle**, **saved nickname** | v2.4.15, v2.4.24, v2.4.16 |

### Platforms

The original was Linux-only.

| # | Platform | Notes |
|---|---|---|
| 50 | **macOS** (Apple Silicon) | Native build |
| 51 | **Windows** | Native build, installer |
| 52 | **Linux AppImage** | Single-file build that runs on most distributions |
| 53 | **Android** (phones, tablets, TV) | One APK; controller-first on TV, rotates freely on a phone or tablet; on Google Play |
| 54 | **Android ads, removable** | The Play build shows ads, removable by a yearly subscription or a one-time unlock (v2.4.40) |
| 55 | **Browser** (WebAssembly) | Runs on desktop and mobile, including iPhone. Browser clients reach the same server as native ones over WebSocket, so they play together |
| 56 | **Browser saves** | Settings, key bindings, level history and high scores persist across a reload via IndexedDB (v2.4.34) |
| 57 | **iOS** (experimental) | Builds and runs, unsigned; not distributed — see [IOS.md](IOS.md) (v2.4.35) |
| 58 | **Server in Docker** | `fb-server` plus an nginx TLS/WebSocket front end in one compose stack — see [SetupServer.md](../SetupServer.md) |

---

## Plays differently

Rules and behaviour the original had, changed here on purpose. Changes that
come with a feature counted above (bigger rooms, the time bonus, the new
look) aren't counted again.

| # | Difference | Original | Here |
|---|---|---|---|
| 59 | **Losing a classic level** | The level restarts straight away | Asked first: **Continue** retries that level with the score back to 0 while the run's clock and shots keep counting, or **Start over** goes back to level 1 (v2.4.120) |
| 60 | **Highscores** | One table: furthest level, then time | Furthest level and most points in one life, kept separately for keyboard/gamepad and mouse/touch (a level where both were used counts as mouse/touch, v2.4.130), each record with its shot count |
| 61 | **Replays** | Recorded on Print Screen (or every game with `--auto-record`) to a file, played back with `--replay` from the command line | Every finished round recorded automatically to a rolling library on the device and played back from the Replays page (v2.4.108) |
| 62 | **Continue when players leave** | A room setting | Always on (v2.4.36) |
| 63 | **Title screen** | The original menu artwork | The Slate menu style by default; **Classic** keeps the original (v2.4.67) |

---

## Fixes to the original server

All of these are in original `fb-server` code and are still present upstream.

| # | Fixed | What was wrong | Release |
|---|---|---|---|
| 64 | **Crash on simultaneous disconnects** | Tearing down a room recursively freed the game while an outer frame was still using it (`game.c`). On a normal build this corrupted whichever branch it read next and could write a bogus win to the stats file; under a sanitizer it aborted the whole process, taking every unrelated room down with it | v2.4.28 |
| 65 | **Player impersonation** | The server relayed each in-game message with the sender byte exactly as the client wrote it, so any client could claim to be any other player in its room — or the room leader. Every relayed message is now stamped with the seat the server assigned | v2.4.29 |
| 66 | **One stray message could kill the server** | A connection left in a room that had closed or kicked it could terminate the entire server process with its next in-game message. Only that connection closes now | v2.4.29 |
| 67 | **Malformed LAN discovery packet** | A full-length discovery datagram made the server read past the end of its receive buffer | v2.4.28 |
| 68 | **Silent privilege-drop failure** | Started with `-u`, a failed switch to the requested user was ignored and the daemon carried on with full privileges, keeping its supplementary groups. It now refuses to start | v2.4.29 |
| 69 | **Unchecked master-server reply** | A hostile or broken master-server response could steer the server's own buffer arithmetic; the length is range-checked before use | v2.4.29 |
| 70 | **Busy discovery port aborted startup** | If anything else held the LAN discovery port the server refused to start at all. It now serves games normally and reports only that broadcast discovery is unavailable | v2.4.31 |
| 71 | **Lobby free-player count** | `LIST` reported a `free:` count that contradicted the open-player list in the same message, counting players seated in not-yet-started rooms as free | — |

---

## Reproduced from the original

Not differences, so not numbered.

| Feature | Status |
|---|---|
| 100 single-player levels | ✅ |
| Level editor | ✅ |
| Chain reaction system (cascading pops) | ✅ |
| Malus (attack bubble) system | ✅ |
| 2–5 player network multiplayer layouts | ✅ |
| Network protocol (`fb-server` + client messages) — this port and the original can share a server | ✅ |
| LAN auto-discovery (UDP broadcast) | ✅ |
| Public server list | ✅ |
| Geolocation dots on the world-map lobby | ✅ |
| In-game chat | ✅ |
| Victories limit | ✅ |
| Per-player color count (5–8 colors) | ✅ |
| Row compression toggle per player | ✅ |
| Single-player targeting (malus focus) | ✅ |
| Multiplayer training mode, and its two-minute score | ✅ |
| Hurry warning and forced shot | ✅ |
| Colour-blind bubbles | ✅ |

---

## Known issues

| | |
|---|---|
| **Local multiplayer with 3 or more players** | Experimental — less play-tested than two-player and network play. Two causes of bubbles appearing detached on the smaller side boards are fixed (a malus parking in an emptied column, and a chain-reaction arc using the centre board's threshold — see [CHANGELOG.md](../CHANGELOG.md)), but the mode has not had a full pass since |
| **Intermittent Android cold-start ANR** | Seen twice during manual device testing (2026-09-05, real tablet, debug build v2.4.80) — "Input dispatching timed out ... waited 10003ms for FocusEvent", both times right after the device woke from doze immediately before a fresh install + launch. Not reproduced in 5 further attempts on the same device, including deliberately recreating those exact conditions (sleep 60s to settle into doze, wake, reinstall, launch). No thread dump was captured during an actual stall, so the root cause is unconfirmed — `NetworkClient::DetectGeoLocation()` (`src/networkclient.cpp`) does a synchronous network call with no overall timeout guard, up to ~16s worst case, which is a real code smell worth fixing on its own merits, but it only fires once per session on connecting to a server, not at cold launch, so it doesn't obviously line up with this |
| **Online highscores aren't verified** | An account proves who sent a run, not that it was played. Two cheap plausibility checks and a ban list stand in until replay-based verification exists |

---

## Not supported

| | |
|---|---|
| **Local multiplayer above 5 players** | `LocalMultiplayerSettings` clamps the count to 2–5, the last hand-authored board layout. Use a network room above that |
| **Intel macOS** | Releases are Apple Silicon only; Intel Macs can build from source or play in the browser |
| **iOS download** | The build is unsigned, so it is not distributed; play in the browser instead |
| **Hosting a LAN game on Android, iOS, Windows or the browser** | Hosting starts `fb-server` as a separate process, which those platforms can't do. Joining works everywhere |
