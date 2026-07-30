# Changelog

## v2.4.30

- **Desktop frame pacing fixed** — the frame limiter was measuring the wrong interval, so instead of holding a steady 60 fps it alternated a full-length pause with almost none, delivering frames in short/long pairs at roughly 107 fps. On a 60 Hz display about half of those were drawn and never shown, and the rest arrived out of step with the refresh, which read as stutter. Frames now arrive evenly.
- **Desktop now runs at exactly browser speed** — a side effect of the pacing bug was that the very short frames hit an internal lower limit, quietly adding about 6% to the game speed on desktop that the browser build never had. Desktop and browser now run identically at the same speed setting.
- **Display sync enabled on desktop** — the game now presents in step with the monitor instead of on its own clock, which removes a dropped or doubled frame every few seconds. Falls back to the frame limiter where the display driver does not support it.
- **Performance overlay** — press **F3** to show frames per second, frame-time range, and effective game speed against the configured speed, in the bottom-right corner. Off by default, remembered between sessions. Useful for comparing desktop against the browser build, and for telling a frame-rate problem apart from a game-speed one — they look the same while playing but have different causes.

## v2.4.29

Completes the high-severity fixes from the repository audit that v2.4.28 started.

- **Players can no longer impersonate each other** — the server relayed each in-game message with the sender byte exactly as the sending client wrote it, so a client could claim to be any other player in its room, or the room leader. The server now stamps every relayed message with the seat it assigned that connection.
- **A stray message no longer takes the server down** — a connection left in a room that had closed or kicked it could, with its next in-game message, terminate the whole server process and every unrelated game running on it. Only that connection is closed now.
- **Server privilege drop fails loudly** — when started with `-u`, a failed switch to the requested user was ignored and the daemon carried on with its original privileges. It now refuses to start, and also drops supplementary groups, which it previously kept.
- **Server rejects an implausible response length** — a hostile or broken master-server reply could steer the server's own buffer arithmetic; the value is now range-checked before use.
- **The game starts even when its settings file cannot be written** — an unwritable preferences folder previously left the game retrying forever before any window appeared, so it looked frozen. It now starts with default settings and says so.
- **A corrupt highscore or level file no longer prevents startup** — the bad entry is skipped and the rest of the file is kept, instead of the game closing during startup.
- **Long lobby listings no longer break the connection** — on a busy server, a single large message could permanently wedge the client's receive buffer: the lobby stopped updating and, in a game, moves from other players were silently discarded while boards drifted apart. No error was shown.
- **Windows: the game no longer stalls waiting for the network** — the client's per-frame receive was blocking on Windows, so the game could hang until the server sent something. *(Fixed by construction; not yet validated on Windows hardware.)*
- **Stale attacks no longer carry into a new game** — starting a different match kept attacks and counters from the previous one, which could land on a board belonging to a player who is no longer in the game.
- **WebSocket messages are sent whole** — a partially-sent message was reported as fully sent, which left browser clients misreading everything that followed on that connection.

## v2.4.28

- **Server crash on simultaneous disconnects fixed** — when several players in the same room dropped at once, the server could keep using a game it had already torn down. On the shipping build this corrupted whichever branch it read next and could write a bogus win to the stats file; under a sanitizer it aborted the process, taking every other room on the server down with it.
- **Server hardened against malformed LAN discovery packets** — a full-length discovery datagram could make the server read past the end of its receive buffer. Well-formed probes are unaffected.
- **Client hardened against malformed data from other players** — out-of-range bubble placements from a peer are now dropped instead of writing outside the board, team numbers are clamped to the valid range instead of indexing off the end of the team color table, and a non-numeric or oversized value in a room-options message no longer terminates the game.
- **Regression tests added** — the server use-after-free, the discovery over-read, and the team-number clamp each have a test that fails without its fix. The two server tests require a sanitizer build and report themselves as skipped otherwise, rather than passing without running.
- **Android releases are now upgradable** — CI previously generated a throwaway signing key on every run, so each release was signed by a different identity and Android refused to install it over the previous version. Releases are now signed with a persistent key held in repository secrets, and a tagged release fails rather than publishing an APK that cannot be upgraded. `versionCode` also advances per release instead of staying pinned. See `docs/ANDROID_SIGNING.md`.

## v2.4.27

- **Desktop game speed default raised to 3×** — new macOS, Linux, and Windows settings now match the browser default; existing saved speed preferences remain unchanged.
- **Round-stats team colors fixed** — winning players in Team Mode now retain their configured team color instead of switching to the generic green winner highlight.
- **All release builds restored** — Linux AppImage, macOS DMG, Windows installer, Android APK, and WebAssembly packages are built, attached to tagged GitHub releases, and deployed to their Itch.io channels.

## v2.4.26

- **20-player battle royale** — rooms can now hold up to 20 players (choose 5, 10, or 20 when creating a room), with new UI to handle the larger player count: an auto-ranked opponent view (the 4 visible mini-boards are kept on whoever's most relevant — targeting you, attacking, in danger, then anyone alive — with manual Tab paging as an override), a slot-relative target picker (keys 1–4 target whoever's shown in that view slot once more than 5 players are alive), a blinking attack-flash border on any board that's actually been hit, kill tracking (a new **KO** column in the round-stats table), a spectate mode for eliminated players (the same 1–4/0 keys pin an opponent's board into view instead of picking a malus target), and a compact 2-column player roster with a lobby room-list `(count/cap)` display for rooms above 5 players.
- **Team Mode** — new game mode for local multiplayer and network game rooms. Players are assigned to teams; malus attacks only go to players on other teams; the round ends when only one team remains alive. Rooms of 5 or fewer players assign teams via a per-player grid row; rooms above 5 players use a dedicated roster instead — press **A** in the game room to open it, with auto-balanced teams as the default and per-player overrides on top.
- **Clear Mode** — new game mode available in local multiplayer and network game rooms. First player to clear their entire board wins the round (last survivor also wins). Defaults to row compression off and malus disabled; both can still be toggled independently by the host.
- **Malus disable setting** — host can now disable malus attacks independently in both local multiplayer and network game rooms.
- **Online lobby and game-room refresh** — adds a persistent chat dock, larger scrollable room cards, an online-player sidebar, and grouped match settings.
- **Fix: classic single-player campaign inheriting a stale chain-reaction flag** — "Play All Levels" and "Pick Start Level" could silently inherit chain reaction being left on from an earlier Random Levels/2P/network session, even though the classic campaign is supposed to always run with it off.

## v2.4.25

- **Dead player's board now freezes mid-round** — in 3–5 player games, when a player is eliminated but the round continues, their board now ices over (and shows the frozen cap) so the elimination is clearly indicated, matching the original Frozen Bubble's `update_lost` behavior.
- **Fix: keyboard aim stuck after using the mouse** — once you aimed with the mouse, the latched mouse angle overrode the launcher every frame, so keyboard/controller aim stopped working until reload (most visible on the itch.io / WASM build). Keyboard/controller aim now reclaims control (mouse re-activates on the next mouse move).

## v2.4.24

- **Post-round stats screen** — after each multiplayer round, a per-player table shows bubbles **Fired**, **Popped**, malus sent (**Atk**) and malus received (**Def**); the round winner's row is highlighted. In network games, each client broadcasts its own round stats via the new `S` GAMEMSG opcode so every player sees exact numbers for everyone (not just themselves).
- **Lobby match summary** — when a network match ends and players return to the lobby, the host posts a summary to the chatroom: rounds played, plus each player's win count and match totals (fired / popped / atk / def).
- **Incoming-malus indicator** — when malus lands on your board, a fading toast above your launcher shows **who sent it and how many** (e.g. `dchau2  +12`). Repeated hits from the same attacker aggregate into one toast.
- **Fullscreen toggle in Settings** — Settings → Keys menu now has a Fullscreen ON/OFF row (desktop only; hidden on WASM). Persists across restarts.
- **Fix: native build broken since the malus-split fix** — `SendMalusToOpponent` referenced `currentSettings.teamMode`, a field that does not exist in this branch's `SetupSettings`, so the native target did not compile. Removed the stray reference.

## v2.4.16

- **Nickname saved** — last used nickname is remembered across restarts on desktop, Android, and web (browser uses localStorage)
- **Lobby player list** — in-lobby screen now shows names of available players (up to 9, with "+N more" for overflow)
- **Nick save fixes** — Set Name field clears before typing; nick saves immediately on confirm, not just on connect

## v2.4.15

- **Sound toggle in Settings** — Settings → Keys menu now has a Sound ON/OFF toggle; disables all music and SFX immediately and persists across restarts

## v2.4.14

- **Xbox controller: continue round fixed** — pressing A on an Xbox controller after a round ends now correctly continues to the next round in local multiplayer

## v2.4.13

- **Single player targeting fixed** — when the "Single player targeting" lobby setting is on, malus now automatically focuses on one opponent instead of splitting; manual keys 1–4 still override the auto-selection
- **Ghost player fix** — reconnecting with the same nickname no longer shows duplicate entries in the server lobby (stale connection is evicted immediately)
- **Android default nickname** — default nickname on Android TV is now `android_user` instead of `unnamed`

## v2.4.12

- **Game Speed setting** — adjustable in Settings → Keys; use LEFT/RIGHT on the "Game Speed" row to set 1.0–5.0×; saved per device to settings.ini
- **Fire TV fixes** — A button now continues to next round after game ends; minimize/resume no longer causes persistent slow speed
- **Malus bubble speed** — increased 25% and now frame-rate-independent

## v2.4.11

- See v2.4.12 (combined release)

## v2.4.10

- **Speed tuned** — native clients (macOS, Linux, Windows, Android TV) run at 1.25× base speed; browser (WebAssembly) runs at 3.0× normalized across all frame rates

## v2.4.9

- **Frame-rate-independent speed** — bubble and launcher movement now scale with delta time on all platforms; browser builds (WebAssembly) run normalized across all frame rates; native builds (macOS, Linux, Windows, Android TV) run at 1.5× speed

## v2.4.8

- **WebAssembly: swap creators fixed** — after a game ends, the server now correctly moves the player's connection back to normal lobby mode (previously left in in-game priority mode), preventing the 5-second in-game timeout and stale duplicate entries when starting a new game with swapped roles
- **WebAssembly: round 2+ sync fixed** — browser client waits for all level-sync messages to arrive before starting each subsequent round (same mechanism as the round 1 fix in v2.4.7); prevents the ~30-second disconnect at the start of round 2
- **Server: version logged at startup** — fb-server now prints its version and protocol to the log on startup, making it easier to confirm which binary is running

## v2.4.7

- **WebAssembly: game start fixed** — browser client now waits for all 40 level-sync messages to queue up before entering the game loop, preventing the immediate disconnect when a native client hosts and the web client joins

## v2.4.6

- **Menu animation fix** — graphics quality icon no longer attempts to load missing frames at startup (off-by-one in frame count guard)
- **macOS startup log fix** — suppressed spurious `[ERROR] [DEBUG] Parameter 'texture' is invalid` messages from SDL Metal renderer initialization

## v2.4.5

- **WebAssembly: join game fixed** — browser client now correctly joins multiplayer game rooms; macOS host can see the web player join
- **WebAssembly: join retry** — JOIN command automatically retries with a name suffix if the nickname is already in use (mirrors the existing CREATE retry behaviour)
- **Game room chat text color** — fixed chat messages appearing yellow on the web client

## v2.4.4

- **WebAssembly: public server list** — itch.io browser version now fetches and displays the public server list on the Net Game screen
- **Net Game loads instantly** — server list fetch and latency probing moved to a background thread (desktop); browser opens the screen immediately
- **WebAssembly: game creation fixed** — CREATE command now waits for server confirmation before entering the game room; automatically retries with a name suffix if the game name is already taken
- **Max colors** — "Colors" option renamed to "Max colors" in all game setup panels (2P, local multiplayer, LAN, net game)
- **Pause animation fix** — pause penguin animation now loads the correct frames (was off-by-one)
- **Stick effect asset fix** — missing `stick_effect_7-mini.png` added; array bounds corrected

## v2.3.1

- **Xbox controller support** — fully working in 1P and 2P local modes; bind any button in Settings → Keys
- **Reset controller defaults** — one-click reset to D-pad + A button layout per player in key bindings
- **Bubble centering fix** — all players now land at the same column when shooting straight up
- **Exit button** — replaced High Scores menu button with an Exit App button
- **Net game manual entry** — added visible Connect button; navigate with UP/DOWN, ENTER to select
- **Net game lobby text color** — fixed text appearing all red after a failed connection attempt
- **False local server in Net Game list** — fixed spurious "Local Server" entry appearing when no server is running
- **Net game keyboard** — keyboard no longer auto-opens when entering the manual IP/port entry screen; press ENTER on a field to open it
- **Android TV delete key** — improved backspace handling for text fields on Android 11+

## v2.3.0

- **Per-player lobby settings grid** — Max colors, Rows collapse, and Aim guide shown as a P1–P5 column grid; host navigates with arrow keys and Enter
- **Aim guide** — trajectory preview toggle per player
- **Row compression toggle** per player — disable rows collapsing for specific players
- **Local multiplayer** — 2 players on controllers (3–5 player local is WIP)
