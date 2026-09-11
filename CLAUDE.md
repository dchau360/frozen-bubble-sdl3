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

**Tests:**
```bash
ctest --test-dir build --output-on-failure
```
Two server tests exercise memory-safety fixes and need a sanitizer build; on an
ordinary build they report as skipped rather than passing without running. To
actually run them:
```bash
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build-asan --parallel
ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 \
  ctest --test-dir build-asan --output-on-failure
```
`fast_unwind_on_malloc=0` matters here: the default frame-pointer-based
unwinder breaks the moment a leak's call stack passes through SDL3/FreeType/
HarfBuzz (built without `-fsanitize`, so ASan's malloc interceptor still
catches their allocations via its process-global hook, but can't identify
frames inside them without this) — reports come back with unresolvable
`<unknown module>` frames instead of real call sites. The slow unwinder walks
DWARF CFI (`.eh_frame`, present in every one of these libs since C++
exceptions need it) instead, and can identify the module even when it can't
name the function.

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

Serve WASM with COOP/COEP headers (required for audio — without them the page
loads but is silent):
```bash
python3 tools/serve-wasm.py   # finds build-wasm/ or dist-wasm/, prints the URL
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

### Singleton architecture

All major subsystems are singletons accessed via `::Instance()`: `FrozenBubble`, `GameSettings`, `NetworkClient`, `AudioMixer`, `TransitionManager`, `HighscoreManager`. There is no dependency injection — subsystems call each other's `Instance()` directly. `GameSettings` stores INI-parsed settings (key bindings, audio/gfx, speed multiplier, nickname) to `SDL_GetPrefPath()` via the bundled `third_party/iniparser`.

### Rendering

The game renders to a fixed **640×480 logical canvas** (`WINDOW_W`/`WINDOW_H` in `transitionmanager.h`); SDL3 scales this to the actual window. All stored rects are `SDL_Rect` (int), converted with `ToFRect()` (`src/sdl3_compat.h`) when calling SDL3 render functions that require `SDL_FRect`.

Transition effects (plasma, bars, circles, etc.) live in `src/shaderstuff.cpp` — pixel-manipulation routines ported from the original Perl/C source. `TransitionManager` calls `TakeSnipOut`/`DoSnipIn` to apply them between screens.

### Input parity (keyboard/gamepad + touch/mouse)

Every menu feature must be reachable both ways: keyboard/gamepad navigation
(arrow keys/D-pad to move focus, ENTER/A to activate, ESC/B to cancel) *and*
a tap/click directly on the thing. Neither is allowed to be the only path —
this codebase has shipped several bugs from exactly that gap:
- A row rendered but never registered as a tap target (SP panel's toggles
  were only reachable by keyboard until `BeginPanelTapRows`/`AddPanelTapRow`
  was added for them).
- A modal that only ENTER/ESC could answer, with no equivalent tap zones
  (`HandlePanelTap`'s dedicated hit-tests for confirm/prompt popups exist
  because of this).
- A two-button popup where ENTER/ESC could each fire one button, but there
  was no keyboard way to move focus onto the non-default button and no
  visual indicator of which one was focused (`confirmDialogFocusNo` in
  `mainmenu.h` — LEFT/RIGHT/TAB moves focus, ENTER activates whichever is
  focused, and the focused button is drawn highlighted; ESC still cancels
  outright as a shortcut).

When adding a new interactive element (row, toggle, button, popup), check
off all of:
1. It has (or joins) real keyboard/gamepad navigation into and out of it.
2. It is registered as a tap target — via `BeginPanelTapRows`/
   `AddPanelTapRow` for a panel's own row list, or a dedicated hit-test in
   `HandlePanelTap` for a modal that isn't part of a row list.
3. If it's one of several choices (Yes/No, Save/Skip, a toggle's two
   states), keyboard focus between the choices is visible on screen, not
   just inferred from which key does what.
4. A footer hint (`menulist::DrawFooterHint`) spells out the controls if
   they aren't obvious from the UI alone.

### Controller input

Local multiplayer controller input uses a virtual scancode system: physical gamepad buttons are mapped to virtual scancodes starting at `CTRL_SC_BASE` (300), with 20 slots per player. `virtualKeyState[]` and `controllerInputs[5]` globals (in `src/gamesettings.h`) are written by `FrozenBubble::HandleControllerEvent()` and read by `BubbleGame` each frame alongside keyboard state via `IsKeyPressed()`.

### Platform abstraction

`__WASM_PORT__` guards all browser-incompatible code (TCP sockets, server hosting, UDP discovery). `__ANDROID_PORT__`/`__ANDROID__` guards Android asset extraction. The `ASSET(relpath)` macro in `platform.h` prepends `g_dataDir` to asset paths; `g_dataDir` is set at startup by `InitDataDir()`.

Assets live in `share/` (gfx, snd, data, fonts) and are referenced via `ASSET("/gfx/...")`. In WASM they're preloaded at `/share` via Emscripten's `--preload-file`.

### Network protocol

Server is the original `fb-server` (C, in `server/`). Protocol is line-based text over TCP/WebSocket:
- Lobby commands: `NICK`, `LIST`, `CREATE`, `JOIN`, `START`, `PART`, `TALK`, `REPORT <nick> <reason>` (abuse report, appended to a flat file for the operator; never acted on automatically)
- In-game messages: `GAMEMSG` prefix wrapping single-char opcodes — `f` (fire), `s` (stick/place), `g` (malus attack), `m`/`M` (bubble sync), `F` (game over/win), `n` (ready for next round), `l` (player left), `o` (options), `r` (targeting), `S` (round stats sync: `S{fired}:{popped}:{malusSent}:{malusReceived}`, broadcast once per round so all clients can render the post-round stats table)

The leader (game creator) is authoritative for level generation and sends bubble positions to joiners via `b|`/`N`/`T` sync messages during `SyncNetworkLevel()`.

**Production server deployment:** `docker/docker-compose.yml` runs `fb-server` on TCP 1511 (native clients) plus an nginx container that terminates TLS and proxies WebSocket connections on port 443 (browser/WASM clients). See `SetupServer.md` for SSL certificate setup.

**Discord join alerts:** on every player's arrival on the server — the NICK handler's success path, where a connection acquires a name and becomes visible in the lobby, *not* `add_player()` — `server/discordalert.c` fires a best-effort UDP datagram at an optional sidecar (`server/discord-relay/`, `FB_SERVER_DISCORD_RELAY` env var) carrying the player's nick, IP, self-reported geolocation (`geoloc[fd]`, may be empty), and the server's name. The sidecar owns the actual Discord webhook POST — `fb-server` has no TLS stack and must never block on network I/O mid-game. The IP and the geolocation both ride along in the datagram (free to include, fb-server already has them) but `relay.py`'s `build_message()` deliberately posts **neither** — only nick and servername reach Discord. The IP never did; the location came out when the client started advertising a community Discord to players (below), since a channel the game recruits players into is not a place to put every joiner's approximate whereabouts. Stateless: no registry, no persistence, no per-player opt-in — every arrival fires, at the server operator's discretion to run the relay at all. Two things that look like arrivals are suppressed at the call site: a second NICK on the same connection (a rename, not an arrival) and a reconnect that evicted its own stale ghost (a flapping link would otherwise redial the channel on every blink). The servername that lands in the message is whatever `-n` set (`net_servername()`); `-n` caps at 12 chars, `[a-zA-Z0-9.-]` only, and `fb-server` `exit(EXIT_FAILURE)`s on a violation rather than truncating or warning — under `restart: unless-stopped` that's a crash loop, confirmed live when a server's own 14-char domain was handed to `-n` directly. `docker-compose.yml`'s `FB_SERVER_NAME` default and `SetupServer.md`'s guidance both account for this. That 12-char cap is purely an in-game/server-list constraint though — `relay.py`'s `DISCORD_SERVER_NAME` env var overrides the name shown in Discord independent of it, so an operator can advertise a short `-n` in-game while Discord shows the real thing. See `server/discord-relay/README.md` and `SetupServer.md`.

**Discord round-result alerts:** `process_msg_prio_()` (`server/game.c`) sniffs the `F` opcode alongside the server's normal, unrelated relay of that same opcode to every client in the room — a bare `"F"` is a draw, `"F<nick>"` is a win claim, and the server relays either verbatim to clients whether or not this fires. On a hit it fires `discordalert_fire_result_event()` with the room's current roster (`g->players_nick[]`, comma-joined — each name already passed `is_nick_ok()` at NICK/JOIN/CREATE time), the winner claim (or `NULL` for a draw), and the room's `game_mode` (the raw `GAMEMODE:%d` from the last `SETOPTIONS`, defaulting to 0/Classic; `setoptions()` now parses and stores it via `parse_game_mode()` rather than just relaying it verbatim). `g->result_posted` guards against firing twice for the same round and is reset on the `n` opcode ("ready for next round") — the closest thing the wire protocol has to an explicit round boundary. **Trust asymmetry:** unlike the roster, the winner string is lifted straight from a client's raw `F` payload with none of `is_nick_ok()`'s validation behind it (the server has never validated `F`'s payload, alerts or not) — a modified client can claim a win it didn't earn, but it cannot forge a false roster, since that comes from the server's own bookkeeping, not the wire message; a literal `|` in the winner payload is stripped at the extraction point in `game.c` so it can't shift the datagram's later fields. `relay.py` maps `game_mode` to a display name (`_GAME_MODE_NAMES`); a mode value a future client sends that this build doesn't recognize just degrades to no label rather than needing a matching server-side update first. `g->game_id` (a plain monotonic int, assigned once per room at `create_game()` and otherwise inert) rides along purely so the relay can group every round from the same room into one Discord thread instead of a fresh top-level message per round — see `server/discord-relay/relay.py`'s `_room_threads`/`_post_result_via_bot_sync()`. That threading needs a Discord bot token (`DISCORD_BOT_TOKEN` + `DISCORD_CHANNEL_ID`), not just the plain webhook everything else here uses: Discord's webhook API can only create a *new* thread when the webhook's channel is a forum/media channel, which would conflict with sharing an ordinary text channel with join alerts. Leaving those two unset (the default) keeps round-result alerts posting flat via the webhook, unaffected — this is an additive upgrade, not a requirement. Deliberately per-round rather than "true final match" — detecting the latter server-side would mean replicating the client's own `victoriesLimit` win-counting, which is brittle and desyncs if it changes mid-room or players leave/reconnect — and this matches the granularity already surfaced to players via the existing post-round stats table (`S` opcode, above). Deliberately does *not* post the departure-inferred win/loss already tracked in `player_part_game_()`'s stats bookkeeping either: a rage-quit and a genuine loss are indistinguishable there, and publicly misattributing an outcome to a named player would be worse than not posting. Same `DISCORD_SERVER_NAME` override as join alerts applies here too. See `server/discord-relay/README.md` and `SetupServer.md`.

**Round-result lobby broadcast:** the same `F`-opcode sniff above also calls `report_round_result()` (`server/game.c`), unconditionally and independent of whether a Discord relay is configured — this is a separate, in-game feature, not part of the Discord pipe. It announces the round to whoever is currently sitting in the lobby, as ordinary `TALK: Server: ...` lines pushed to `open_players` — the same scope `talk()`'s own server-wide branch uses when its sender isn't seated in a game, so a player off playing in some *other* room never sees it, only someone idling in the lobby does. Two lines: a headline (`"<nick> wins! (win #N this match)"`, `"Team T wins (<roster>)!"` on a team win, or `"a draw."`), then `"Top scorers: <nick> (N), ..."` for up to 5 players who have won at least one round, most wins first — omitted entirely when nobody has won yet. **Win-count, not bubbles popped, is this server's only notion of "score"**: `g->players_wins[]` increments from the same `F` claim already being sniffed, so it needed no new wire data; a per-round points tally would have meant teaching the server to parse the `S` stats opcode it has only ever relayed blindly, which this deliberately avoids. **Team info was already on the wire and unused**: `SETOPTIONS`'s `TEAMCOUNT`/`PLAYERTEAM_P1..P5` fields (`src/networkclient.cpp`'s `SendOptions`) have always reached the server, which only ever relayed them opaquely; `parse_teams()` (mirroring `parse_game_mode()`) now also reads them into `g->team_count`/`g->players_team[]`, capped at slots 0-4 same as `PLAYERTEAM_Pn` itself is — a >5-player room's remaining seats simply carry no team. Both new per-room arrays (`players_wins[]`, `players_team[]`) shift in step with `players_nick[]` in `player_part_game_()` so a mid-match departure can't misattribute either to the wrong remaining player. Same trust posture as the winner claim above: a name in `F` matching no seated player still gets announced, just with no win-count or team to attach, since there is nothing real to report. See `tests/server_round_result_lobby_test.py`.

**Community Discord link (client side, unrelated to the above):** `kDiscordInviteUrl` in `src/platform.cpp` is a build-time constant, surfaced as a "Join our Discord" row on the NET GAME server list and in the online lobby, opened with `SDL_OpenURL()` (which covers all five platforms — no per-platform branch). It is deliberately *not* something a server or the public server list advertises: an invite arriving over the wire would let any operator redirect players to a Discord of their choosing from inside the game's own UI, which is a phishing primitive. Both rows resolve their index through `MainMenu::ServerListDiscordIndex()` / `MainMenu::LobbyDiscordIndex()` so the renderer, the arrow-key bounds and ENTER cannot drift apart; both vanish when the constant is empty, so a fork with no Discord shows no dead button.

### Original Perl source (for verification)

When implementing or debugging game mechanics, compare against the original Perl source:
- `bin/frozen-bubble` (~2500 lines) — main game loop, collision, chain reactions, malus, win conditions
- `lib/Games/FrozenBubble/Net.pm` — original network protocol

Key line references: malus formula (line 958), chain reactions (819–841), win sync `F` message (1943), `real_stick_bubble` (731), living players (600).

### CI / release

`.github/workflows/build.yml` builds Linux (AppImage), macOS (DMG), Windows (NSIS installer), Android (APK), WASM, and a Linux ASan/UBSan sanitizer job (ships no artifact of its own; exists to catch memory-safety regressions) on every push to `main`, on PRs to `main`, and on `v*.*.*` tags. On tag pushes, `release` packages whichever of the five platform builds actually succeeded that run into a GitHub Release, and `deploy-itchio-html5` publishes the WASM build to itch.io via Butler — itch.io gets only the browser build; downloads for the other four platforms are distributed through the GitHub Release (and Google Play for Android) instead.

A failing ASan/UBSan run hard-blocks both: no platform ships, itch.io included, if it catches a memory-safety bug. A single platform build failing on its own (say, Windows) does *not* block the others any more — `release`'s `if:` opts out of the default needs-failure skip (`!cancelled()`) and just omits that platform's file from the release, so the other four still ship and the failed one stays on whatever version its last successful tag published until it's fixed and re-tagged. Only `release` and `deploy-itchio-html5` are tag-gated (`startsWith(github.ref, 'refs/tags/')`), so pushes and PRs build (and, for six jobs, test) without publishing anything.

Android releases are signed with a persistent key held in repository secrets, and a tagged build fails outright rather than shipping an APK that cannot be upgraded — see `docs/ANDROID_SIGNING.md`.

### Cutting a release

Three files carry the version and must be bumped together:

1. `CMakeLists.txt` — `project(... VERSION x.y.z ...)`. Authoritative: the game's
   `APP_VERSION` (shown in the menu), the server's startup log and its
   master-server user agent all derive from it.
2. `android/app/build.gradle` — `versionName`, and `versionCode` must strictly
   increase or Android refuses the upgrade.
3. `.github/workflows/build.yml` — the two off-tag `version="x.y.z"` fallbacks.

`default.nix` carries a copy too, but nothing ships from it. Then update
`CHANGELOG.md`, commit, and push an annotated `vx.y.z` tag — the tag is what
triggers the release and the itch.io deploys.

Automated tests run under `ctest` (`ctest --test-dir build`). Two of them need a sanitizer build and report themselves as skipped otherwise via `SKIP_RETURN_CODE 77`, rather than passing without running. Gameplay itself is still verified manually.
