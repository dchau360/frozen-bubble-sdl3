# Changelog

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
