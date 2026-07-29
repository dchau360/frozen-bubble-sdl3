# 04 — Lobby, Settings, and Input Audit Notebook

## Scope

Task 6: lobby and menu state, settings/highscore persistence, option validation, keyboard, controller, and mouse input.

Reviewed translation units and headers (6,730 lines total):
`src/mainmenu.cpp` (672), `src/mainmenu.h` (278), `src/mainmenu_internal.h` (44),
`src/mainmenu_input.cpp` (1,669), `src/mainmenu_netpanel.cpp` (1,342),
`src/mainmenu_panels.cpp` (579), `src/mainmenu_server.cpp` (192),
`src/gamesettings.cpp` (331), `src/gamesettings.h` (114),
`src/highscoremanager.cpp` (445), `src/highscoremanager.h` (81),
`src/menubutton.cpp` (174), `src/menubutton.h` (55), and the input/event and
lifecycle paths of `src/frozenbubble.cpp` (644) and `src/frozenbubble.h` (110).

Consumed boundaries: Task 4's lobby option transport (`SETOPTIONS`/`OPTIONS:`,
`GetAndClearPendingOptions`) and Task 5's `SetupSettings` contract. Produced
boundaries for Task 10: the malformed-preferences matrix in
[Dynamic evidence](#dynamic-evidence) and the controller-slot invariants below.

## Trust boundaries and invariants

Four inputs cross into this subsystem, none of them under menu control:

| Boundary | Source | Menu-side entry point | Validation actually performed |
|---|---|---|---|
| Persisted settings | `settings.ini` under `SDL_GetPrefPath("", "frozen-bubble")` — user-editable, sync-shared, or corrupt | `GameSettings::ReadSettings` / `LoadDefaultKeys` | `gfxQuality` clamped 1–3; `windowWidth` clamped; `speedMultiplier` clamped only for ordered comparisons; **no** validation of `windowHeight`'s upper bound, of NaN, or of any `P*` scancode |
| Persisted highscores | `highscores`, `highlevelshistory` in the same directory | `HighscoreManager::LoadLevelsetHighscores` / `LoadHighscoreLevels` | none; `stoi`/`stof` are called directly on file text |
| Peer/host room options | server-relayed `OPTIONS:` push. **Corrected in Task 12:** this row previously read "any room member can emit `SETOPTIONS`". It cannot — `server/game.c:405` gates `setoptions` on `if (g->players_conn[0] == fd)` and answers any other seat with `wn_not_creator` (`:415`), which is what [notebook 01's own row](01-server-protocol.md#static-review) records. Only the **slot-zero room creator** can make the server emit an `OPTIONS:` push; see SEC-004 | `MainMenu::NetPanelRender` → `GetAndClearPendingOptions` | victories limit is mapped through a fixed table; `TEAMCOUNT` clamped 2–5; **no** clamp on `PLAYERTEAM_Pn`; colours only clamped later, inside gameplay |
| SDL input events | keyboard, gamepad hot-plug, mouse, touch, text input | `FrozenBubble::HandleInput` / `HandleControllerEvent`, `MainMenu::HandleInput` | button/slot arithmetic is unchecked in both directions (see BUG-035, BUG-036) |

Invariants the subsystem is written against, and their real status:

1. *A settings file always exists and always parses.* — False; the retry loop
   assumes `CreateDefaultSettings()` can always repair it (BUG-026).
2. *Every value read from the INI is in range.* — False for `WindowHeight`,
   `SpeedMultiplier`, and every key binding (BUG-028, BUG-029, BUG-030).
3. *A failed write is reported.* — False; the only diagnostic is `SDL_LogWarn`
   on category 1, which SDL3 suppresses at default priority (BUG-031).
4. *Virtual scancode = `CTRL_SC_BASE + slot*20 + button`, slot in 0..4, is
   injective and stays inside `virtualKeyState[100]`.* — False in both
   directions: SDL3 defines 26 gamepad buttons, so button ≥ 20 aliases the next
   slot (BUG-036); and the controller slot list never shrinks, so `slot` grows
   without bound (BUG-035).
5. *Per-player room options describe every player.* — False above five slots;
   the wire format stops at `_P5` (BUG-040).
6. *A menu panel that exists is reachable.* — False for the two-player setup
   panel and the network setup panel (BUG-023 extension, IMP-012).

## Static review

### Menu and room state transitions (Step 1)

Top-level state lives in `FrozenBubble::currentState` (`TitleScreen`,
`MainGame`, `LevelEditor`, `Netplay`, `Highscores`). Only three are ever
assigned: `TitleScreen`, `MainGame`, and `Highscores`. `LevelEditor` and
`Netplay` are declared but never assigned anywhere in `src/` (`rg 'LevelEditor'`
matches only the enumerator declaration), and the `editor` menu button has no
branch in `MenuButton::Pressed` — pressing it does nothing at all. The level
editor named in the plan's scope does not exist as reachable code.

`MainMenu` panel state is nine independent booleans plus `networkInputMode`
(0,3,4,5,6,7,8,9,10,11 reachable; 1 and 2 are not). `ShowPanel` is the single
entry point:

| Button | `ShowPanel` | Resulting state |
|---|---|---|
| `1pgame` | 0 | `showingSPPanel`; sub-selection via `press()` → modes 1/5/3/6/7 |
| `2pgame` | 2 | `showingLocalMPPanel`, `selectedMode = 7` — **not** the 2-player panel |
| `langame` | 3 | `showingNetPanel`, `networkInputMode = 7`, LAN discovery + latency probes on the calling thread |
| `netgame` | 5 | `showingNetPanel`, `networkInputMode = 10`, background fetch thread |
| `keys` | 4 | `showingKeysPanel`, `keyConfigPlayer = 1` |
| `highscores` | 6 | `HighscoreManager::ShowScoreScreen(0)` |
| `editor` | — | no handler |

`showing2PPanel` is assigned `false` in exactly two places
(`mainmenu.cpp:614`, `mainmenu_input.cpp:1647`) and `true` nowhere, so
`TPPanelRender`, `TwoPPanelKey`, `twoPlayerCR`, and `twoPlayerVictoriesIndex`
are unreachable and `SetupNewGame(2)` is never called. The same holds for
`showingNetSetupPanel` (never set true), which is the only producer of
`networkInputMode == 2`. This is the menu-side half of BUG-023 and is recorded
there rather than as a new ID.

Selection indices are bounded at their own navigation sites but are **not**
re-initialised on the transitions that change their meaning:
`selectedActionIndex` survives create/part/room-close (only the successful join
path resets it to 0), `currentPlayerCol` survives player-count shrink,
`netTeamOverrides` and `teamOverrideChatCount` survive leaving a room entirely,
and `lastProcessedChatCount` is only reset when the chat vector shrinks. The
override map is keyed by nickname, so a team chosen in one room is silently
re-applied to the same nickname in the next room (BUG-037). No out-of-range
index results, because every consumer re-clamps against `players.size()` capped
at 5, and the join path bounds-checks `gameIndex`.

`ReturnToMenu` clears eight panel booleans, `awaitKp`, `runDelay`, and stops
text input, and stops a hosted server; it does not clear `showingKeysPanel`,
`networkGameStarting`, `pendingLobbyConnect`, `selectedActionIndex`,
`currentPlayerCol`, or the team-override map. `ReturnToNetLobby` empties the
room in place and re-requests the list, which is the correct stale-state fix for
the room card, but leaves the same menu-side selection state.

Player-count-dependent layout is clamped to five everywhere it indexes
(`numPlayers` clamped 1..5 before every grid loop, `myJoinerSlot` searched
inside `players.size()`), while the roster panel for `maxPlayers > 5` iterates
`cap` (≤ 20) rows and indexes `players` only under `pi < players.size()`. No
index defect was found in the >5-cap roster; its defect is in what the options
protocol can express (BUG-040).

### Option serialization and validation (Step 2)

Local option → wire → remote → `SetupSettings` → gameplay was traced for every
option:

| Option | Menu state | Wire | Remote apply | `SetupSettings` | Verdict |
|---|---|---|---|---|---|
| Chain reaction | `chainReactionEnabled` | `CHAINREACTION` | direct bool | `chainReaction` (via `chainReaction` scratch member) | consistent |
| Continue when players leave | `continueWhenPlayersLeave` | `CONTINUEGAMEWHENPLAYERSLEAVE` | `rcvContinueLeave` | **no field exists** | BUG-021 origin confirmed |
| Single-player targetting | `singlePlayerTargetting` | `SINGLEPLAYERTARGETTING` | bool | `singlePlayerTargetting` | consistent |
| Victories limit | `victoriesLimitIndex` (0..17) | value from `vLimits[18]` | reverse-mapped, default index 5 | `victoriesLimit` | index bounded at every write; candidate dismissed |
| Max colours P1-P5 | `playerColorCounts[5]` (5..8) | `NUMCOLORS_P1..P5` | copied unclamped | `playerColors[i]` | gameplay re-clamps 5..8 twice; dismissed |
| Row collapse / aim guide | `playerNoCompress[5]`, `playerAimGuide[5]` | `NOCOMPRESS_*`, `AIMGUIDE_*` | bool | per-player | consistent |
| Mouse aim | `netRoomMouseEnabled` | `MOUSEENABLED` | bool | written into the **global** `GameSettings::mouseEnabled` at game start | persistent side effect of a per-session option |
| Clear / Team mode | `netClearMode`, `netTeamMode` | `CLEARMODE`, `TEAMMODE` | bool | `clearMode`, `teamMode` | consistent; clear-mode entry snapshots and restores compression/malus |
| Team count | `netTeamCount` (fixed 5) | `TEAMCOUNT` | clamped 2..5 | `teamCount` | consistent |
| Player teams | `netPlayerTeams[20]` | `PLAYERTEAM_P1..P5` only | copied **unclamped** | `playerTeams[i]` | SEC-007; slots 6-20 unrepresentable (BUG-040) |
| Room size | `netRoomSizeChoice` → `kRoomSizes[3]` | `CREATE <nick> <n>` | server caps | — | consistent |

Two structural gaps follow directly from the table. First, `SendOptions`'s
signature stops at five players (`const int playerColors[5]`, …,
`const int playerTeams[5]`) while `CreateGame` offers rooms of 5/10/20 and
`GameRoom::maxPlayers` is clamped to 20 — so in a 20-player room the host's
per-player configuration for slots 6-20 cannot be expressed, and
`SetupNewGame(4)` leaves those `SetupSettings` entries at their static defaults
(BUG-040). Second, `GetAndClearPendingOptions` is applied by *every* client,
including the host, with no check that the push originated from the room
creator — the lobby-side consumption of SEC-004. **Task 12 bounded what that
gap can be reached by:** the server itself emits `OPTIONS:` only for the
slot-zero creator (`server/game.c:401-421`), so the missing client-side check is
defence in depth against a hostile *creator* (and, unclaimed, against the
verbatim in-game prio relay), not a path by which an ordinary joiner can
overwrite the room's options.

Team-value flow for SEC-007, exactly:
`NetworkClient::HandlePushMessage` (`src/networkclient.cpp:1191-1195`) parses
`PLAYERTEAM_Pn` with `std::stoi` and stores it unclamped (contrast
`TEAMCOUNT`, clamped on the two preceding lines) →
`MainMenu::NetPanelRender` (`src/mainmenu_netpanel.cpp:126`)
`netPlayerTeams[i] = pt[i]` with no clamp →
`MainMenu::SetupNewGame` case 4 (`src/mainmenu.cpp:565`)
`ns.playerTeams[i] = netPlayerTeams[i]` →
`BubbleGame` render `kTeamColors[currentSettings.playerTeams[i] - 1]`
(`src/bubblegame_render.cpp:87` and `:451`) where `kTeamColors` is a five-element
`constexpr SDL_Color[5]`. The lobby's own renderers clamp (`drawTeamSwatch`
forces 1..5, the fat-row panel forces 1..5), which is why the defect only
materialises after the match starts.

### Settings and highscore persistence (Step 3)

`GameSettings::ReadSettings` is the only loader:

```
optDict = iniparser_load(setPath);
while (optDict == NULL) { CreateDefaultSettings(); optDict = iniparser_load(setPath); }
```

`iniparser_load` returns `NULL` on *any* syntax error, on an over-long line, and
on a file it cannot open (`third_party/iniparser/iniparser.c:840-899`).
`CreateDefaultSettings` returns early without producing a file when the
directory cannot be created or when `fopen(setPath, "w")` fails. The loop has no
attempt counter and no exit, so those two facts compose into an unbounded
startup spin (BUG-026) — reproduced three ways below. When the file *is*
writable the same path silently truncates it and writes defaults, discarding
every stored preference (BUG-027).

Value handling after a successful load:

- `if (windowHeight < 480 || windowWidth > 9999) windowHeight = 480;` — the
  upper-bound test names `windowWidth`, which the preceding line already
  clamped, so the height bound is dead (BUG-029).
- `speedMultiplier` is compared with `<` and `>` only, so NaN passes both
  clamps and reaches `FrozenBubble::RunOneFrame`'s `deltaScale` computation,
  whose own guards are also ordered comparisons (BUG-030).
- `LoadDefaultKeys` casts `iniparser_getint` results straight to `SDL_Scancode`
  with no range check; `iniparser_getint` uses `strtol(str, NULL, 0)`, so it also
  accepts hex and yields 0 for non-numeric text (BUG-028).
- `savedNickname` is filled with `snprintf(..., sizeof(savedNickname), "%s")`,
  which truncates safely at 31 characters.

Writing: `SaveSettings` opens `"w+"` and dumps in place — no temporary file, no
rename, no `fclose`/`ferror` check, so an interrupted write leaves a truncated
settings file, and a failed open produces only the suppressed warning of
BUG-031. `CreateDefaultSettings`'s own error detection is inert for a second
reason: the `while (rval == 0)` body ends with `rval = 1`, which overwrites any
`-1` an `iniparser_set` returned, so the `if (rval < 0)` branch below cannot
execute.

`GameSettings::Dispose()` calls `SaveSettings()` and then `this->~GameSettings()`
explicitly on a heap object that is never freed and whose `ptrInstance` is not
cleared; `HighscoreManager::Dispose()` does the same. Any `Instance()` use after
`FrozenBubble`'s destructor therefore touches a destroyed object, and `optDict`
would be freed twice on a second `Dispose`. `optDict` is also declared without
an initialiser, so on the constructor's early-return paths (below) it is
indeterminate when the destructor frees it.

`HighscoreManager` reads two files with `stoi`/`stof` and no `try`/`catch`
(`highscoremanager.cpp:83-87`, `:129`). Both are called during construction,
which happens inside the `FrozenBubble` constructor, so a corrupt file aborts
the process before the window ever appears (BUG-032). `SaveNewHighscores` writes
both files with no error checking and no atomic replacement.

### Keyboard, controller, and mouse bounds (Step 4)

Event routing: `FrozenBubble::RunOneFrame` diverts every
`SDL_EVENT_GAMEPAD_*`/`SDL_EVENT_JOYSTICK_ADDED` event to
`HandleControllerEvent` and `continue`s, so `MainMenu::HandleInput`'s own
`SDL_EVENT_GAMEPAD_BUTTON_DOWN` translation block is unreachable (IMP-012).
Everything else reaches `HandleInput`, which dispatches by `currentState`.

Controller slot arithmetic:

- `controllers` is a `std::vector<ControllerState>` appended to on
  `SDL_EVENT_GAMEPAD_ADDED`/`SDL_EVENT_JOYSTICK_ADDED`. There is **no**
  `SDL_EVENT_GAMEPAD_REMOVED` handler anywhere in `src/` and no
  `SDL_CloseGamepad` call in `frozenbubble.cpp`, so entries and open handles
  accumulate. SDL issues a fresh `SDL_JoystickID` per connection, so the
  duplicate guard (`cs.id == id`) does not merge a re-plugged pad.
- `playerIdx` is the vector position, unbounded, and is used as
  `CTRL_SC_BASE + playerIdx * 20 + button`. `virtualKeyState` holds
  `CTRL_SC_COUNT = 100` entries and `IsVirtualScancode` gates writes to
  `[300, 400)`, so no write escapes; instead, from the sixth slot onward the
  derived code is ≥ 400, `IsVirtualScancode` is false, and in-game controller
  input silently stops reaching `IsKeyPressed`. **Completing the trace to the
  actual out-of-bounds read (BUG-035) takes one more hop than the write side:**
  the derived code first has to reach a player's stored binding, and the only
  path that stores one is key-bind capture, not the in-game write path above.
  1. **Bind capture emits the derived code.** While the Keys panel awaits a
     binding, `HandleControllerEvent` (`frozenbubble.cpp:384-388`) computes
     `vsc = 300 + playerIdx*20 + button` for whichever gamepad fired the
     button and calls `PushScancode(vsc, true)`.
  2. **`PushScancode`'s raw-event fallback bypasses the virtual-range guard
     entirely.** `PushScancode` (`frozenbubble.cpp:334-347`) only touches
     `virtualKeyState` inside `if (IsVirtualScancode(sc))`; for `vsc ≥ 400`
     (slot 6+) that whole block is skipped, so the function falls straight
     through to its unconditional tail and pushes a genuine
     `SDL_EVENT_KEY_DOWN`/`_UP` `SDL_Event` with `ev.key.scancode = vsc` —
     an `SDL_Scancode` value the enum was never meant to hold, and pushed
     regardless of the caller's `skipEvent` argument, since that check only
     runs inside the now-skipped `if`.
  3. **`KeysPanelKey` stores the raw value with no range check.**
     `MainMenu::KeysPanelKey` (`mainmenu_input.cpp:498-513`) is exactly what
     `awaitKp` routes that synthetic key-down event to; it writes
     `e->key.scancode` straight into `PlayerKeys::left/right/fire/center`
     (case 0-3, `:509-512`) with no bounds check of any kind — the same sink
     BUG-028 reaches from a corrupt INI file, but reached here from a live
     controller instead of a settings file.
  4. **`IsKeyPressed` then indexes `SDL_GetKeyboardState` with that value
     every frame.** `IsKeyPressed` (`gamesettings.h:50-54`) is called once
     per frame per bound key from `bubblegame_shooter.cpp`. Since the stored
     scancode is `≥ 400`, `IsVirtualScancode` is false, so execution falls to
     the `else` branch, `SDL_GetKeyboardState(NULL)[sc]` — indexing SDL's
     real keyboard-state array (length `SDL_SCANCODE_COUNT = 512`, confirmed
     by this gate's `numkeys=512` probe below) with the unbounded, unchecked
     stored value. From the eleventh connected slot the derived code reaches
     `300 + 11*20 = 520 ≥ 512`, so this is an out-of-bounds read of the
     buffer `SDL_GetKeyboardState` returns, executed once per frame for as
     long as the binding survives — not merely a "stops reaching
     `IsKeyPressed`" dead end, which is what the in-game write path
     (`frozenbubble.cpp:395-408`) alone would suggest.
- SDL3 defines 26 gamepad buttons (`SDL_GAMEPAD_BUTTON_COUNT`, with
  `SDL_GAMEPAD_BUTTON_TOUCHPAD = 20`), but the stride is 20, so player 1's
  touchpad button produces exactly player 2's `SOUTH` code (BUG-036).
- `controllerInputs[5]`, documented in CLAUDE.md as written by
  `HandleControllerEvent`, is never written there — only zeroed by `BubbleGame`
  and OR-ed into the shooter flags. It is dead state (IMP-012); an inline
  comment in `frozenbubble.cpp:393` says the approach "was wrong".

Key-binding panel: `keyConfigPlayer` cycles 1..4 in both directions while
`allKeys[5]` and `GameSettings::player5Keys` exist and have defaults, so player
5 can never be bound; the panel's own header prints "Player %d/4" and the local
multiplayer panel caps `localMPPlayerCount` at 4, contradicting `mainmenu.h`'s
"2-5 players" comment and the five-wide arrays (BUG-039). Index bounds inside
the panel are safe: `keyConfigIndex` wraps 0..8 natively and 0..7 on WASM, and
row 8 (fullscreen) is compiled out on WASM together with its handler.

Text fields: `networkHost[256]`, `networkChatInput[256]`, `networkUsername[32]`,
`networkJoinCreator[32]`, and `networkPreNick[32]` all bound their writes by
explicit length checks (`len < 255`, `len < 31`, `len < 15`), and every
`SDL_EVENT_TEXT_INPUT` path re-terminates. Multi-byte UTF-8 is copied byte by
byte with the same bounds, so a truncation can split a code point but cannot
overflow. The chat dock clips display to the last 70 characters.

Mouse/touch: menu hit-testing converts window to logical coordinates through
`SDL_RenderCoordinatesFromWindow` and bounds `btn` to 0..7 before
`SelectAndPressButton`, which re-checks `idx < buttons.size()`. Native builds
skip `SDL_TOUCH_MOUSEID`-synthesised mouse events; WASM intentionally uses the
mouse path only. No index defect found.

### Local server control

`MainMenu::StartLocalServer` runs `system("pkill -x fb-server 2>/dev/null")`
whenever the chosen port looks occupied. That kills every `fb-server` process
the user owns, not just an orphan of this client — Task 2 observed exactly such
a foreign, unrelated `fb-server` (PID 22300, from another checkout) running on
this host, which this code path would have terminated (BUG-033). The child then
`execl`s the first of six relative/absolute candidate paths that is executable,
and `StopLocalServer` sends `SIGTERM` and calls `waitpid(..., WNOHANG)` once, so
a slow child is left unreaped.

### Analyzer triage

90 unique cppcheck records and 79 unique clang-tidy records touch the scoped
files. They map to: 12 + 7 + 2 uninitialised-member records (root cause of
BUG-034, otherwise IMP-005), 23 `redundantAssignment` and 20
`EnumCastOutOfRange` records (dismissed below), 19
`bugprone-narrowing-conversions` (IMP-006), 6
`bugprone-unchecked-string-to-number-conversion` (the `sscanf`/`atoi` geoloc and
`!team:` parsers — all range-checked at their use sites, so IMP-008), 6
`switch-missing-default-case` and 3 `DeadStores` (IMP-009), 2
`insecureAPI.rand` (menu candy/highscore picture selection only), and the two
substantive singletons `bugprone-command-processor` (BUG-033) and
`bugprone-throwing-static-initialization` (`ptrInstance = new GameSettings()`
at namespace scope — recorded under IMP-012). No analyzer record was promoted
without an independent semantic trace.

## Dynamic evidence

All runtime work used isolated preference homes. `HOME` alone is **not**
sufficient on macOS: with `HOME` redirected, `SDL_GetPrefPath` still resolved to
`/Users/dchau/Library/Application Support/frozen-bubble/`, and the harness's
built-in isolation gate refused to continue (exit 4) before opening any file.
Adding `CFFIXED_USER_HOME` redirected the path correctly. Every subsequent run
asserted `ISOLATION=OK` (12/12 matrix cases per build) before touching a file,
and ran with its working directory inside the scenario home. The user's three
real preference files were hashed before the work and re-verified byte-identical
afterwards.

**Harness.** `/tmp/fb-sdl3-audit/task6/task6_settings_harness.cpp` is a test-only
translation unit that links the **unchanged** production object
`gamesettings.cpp.o` (from `build-audit-werror` and `build-audit-sanitize`) plus
the bundled iniparser and SDL3, and calls the real
`ReadSettings`/`LoadDefaultKeys`/`SaveKeys`/`SaveSettings`. Production sources
were not modified.

Matrix result (identical in the warnings-strict and ASan+UBSan builds except
where noted; logs: `/tmp/fb-sdl3-audit/task6/matrix-normal.log`,
`matrix-sanitize.log`):

| Case | Fixture | Observed behaviour | Finding |
|---|---|---|---|
| A | no directory, no file | defaults created and saved; `gfxQuality=1`, 640×480, speed 3.00 | baseline |
| B | zero-byte `settings.ini` | loads as an empty dictionary; all defaults used; file left at 0 bytes, never repaired | benign, recorded |
| C | syntax-error line | `iniparser: syntax error … (3)`; file rewritten with 30 default lines; the stored `Quality = 2` and `Fullscreen = true` are gone | BUG-027 |
| D | only `[GFX] Quality = 2` | per-key defaults for everything absent; `gfxQuality=2` honoured | correct |
| E | `Quality=99`, `WindowWidth=-5`, `WindowHeight=999999`, `Fullscreen=maybe`, `SpeedMultiplier=100000` | quality→3, width→640, speed→5.0, **height stays 999999** | BUG-029 |
| F | `Quality=abc`, `WindowWidth=xyz`, `WindowHeight=0x7fffffff`, `SpeedMultiplier=nan`, `P1Left=99999` | height = 2147483647; `speedMultiplier=nan isnan=1`; `p1.left=99999`; sanitized build aborted with UBSan `load of value 99999, which is not a valid value for type 'SDL_Scancode'` (SIGABRT) | BUG-028, BUG-029, BUG-030 |
| G | 4,000-character line | `iniparser: input line too long`; whole file replaced by defaults | BUG-027 |
| G2 | 200-character nickname | truncated to 31 characters, saved back cleanly | safe |
| H | read-only but valid file | load succeeds; `SaveKeys` returns normally; file unchanged; **no diagnostic of any kind** on stdout or stderr | BUG-031 |
| I | read-only **and** malformed | `ReadSettings` never returns — repeating syntax-error/permission-denied pairs until the 10 s (20 s sanitized) kill | BUG-026 |
| J | pref directory `chmod 555`, no file | same unbounded loop | BUG-026 |
| K | `Library/Application Support` `chmod 555`, app dir absent | `SDL_GetPrefPath` still returns a path, `mkdir` fails, same unbounded loop | BUG-026 |

**Full-client runs.** The sanitized production binary
`build-audit-sanitize/frozen-bubble-sdl3` was run with `SDL_VIDEODRIVER=dummy`,
`SDL_AUDIODRIVER=dummy`, an isolated preference home, and a 12 s kill:

- clean home: created defaults, logged both missing highscore files, reached
  `RunForEver: starting loop`, and was killed by the timeout (expected — it is
  an interactive game). It wrote `frozen-bubble-creator.log` into the working
  directory, not the preference directory.
- `highscores` containing `notanumber,bob,12.5,3`: exit −6,
  `libc++abi: terminating due to uncaught exception of type std::invalid_argument: stoi: no conversion`
  (BUG-032).
- `highlevelshistory` containing `1 2 x 4`: exit −6, identical abort (BUG-032).
- read-only malformed `settings.ini`: no window, no exit, repeating
  permission-denied/syntax-error pairs until the 12 s kill (BUG-026 in the
  shipped binary, not only in the harness).
- binary copied into `…/Contents/Resources/` so `InitDataDir` derives a
  non-existent asset directory: exit −6 with
  `UndefinedBehaviorSanitizer: member call on misaligned address 0xbebebebebebebebe for type 'AudioMixer'`
  at `frozenbubble.cpp:228` called from `main.cpp:30` — the constructor's
  early-return path leaving `audMixer` indeterminate (BUG-034).

**Supporting probe.** `SDL_GetKeyboardState(&n)` reports `numkeys=512`
(`SDL_SCANCODE_COUNT=512`), fixing the exact bound that BUG-028 and BUG-035
exceed.

No listener, server, socket, browser, or peer traffic was created; no security
scenario was executed.

## Candidates

Every candidate raised in this gate is dispositioned. Confirmed candidates
became BUG-026…BUG-040, SEC-007, and IMP-012; the rest are in
[Dismissed candidates](#dismissed-candidates). Cross-owner candidates inherited
from earlier gates:

| Inherited item | Task 6 resolution |
|---|---|
| BUG-021 (disconnect option origin) | Confirmed at the origin: `continueWhenPlayersLeave` is edited, rendered, and transmitted, but `SetupSettings` has no corresponding field, so no menu change can reach gameplay. Recorded against BUG-021, no new ID. |
| BUG-023 (local two-player victory limit) | Confirmed and extended: the limit is not propagated *and* the panel that edits it is unreachable, because `showing2PPanel` is never set true and `SetupNewGame(2)` is never called. Recorded against BUG-023. |
| SEC-004 (unbound player/leader identity) | Lobby-side consumption confirmed: `GetAndClearPendingOptions` applies any `OPTIONS:` push unconditionally, including on the host, with no creator check. Recorded against SEC-004; the unclamped team value it carries is SEC-007. **Task 12 correction:** the server does enforce creator authority on `SETOPTIONS` (`server/game.c:405`), so the sender of a hostile `OPTIONS:` push must be the room creator. This row's earlier framing, and the trust-boundary row above it, wrongly implied any room member could send one. |
| Task 4 option-propagation paths | Fully traced in the Step 2 table; the surviving defects are SEC-007 and BUG-040. |
| IMP-005 (default member initialisation) | Task 6 slice split: the `FrozenBubble` pointer members are promoted to the defect BUG-034; `GameSettings`' ten uninitialised private members and the `HighscoreData`/`MainMenu` scalars remain improvement-level under IMP-005. |

## Confirmed findings

| ID | Severity | Summary | Primary evidence |
|---|---|---|---|
| BUG-026 | High | `ReadSettings`'s `while (optDict == NULL)` retry never terminates when `CreateDefaultSettings` cannot produce a loadable file | matrix cases I/J/K and the read-only-malformed full-client run |
| BUG-027 | Medium | Any INI parse error silently discards every persisted setting and rewrites defaults | matrix cases C and G |
| BUG-028 | Medium | Key bindings are cast from unvalidated INI integers to `SDL_Scancode`; 99999 survives into `PlayerKeys` and `IsKeyPressed` then indexes a 512-entry array | matrix case F, UBSan invalid-enum load, `numkeys=512` probe |
| BUG-029 | Low | The `WindowHeight` upper-bound test names `windowWidth`, so out-of-range heights are accepted | matrix cases E (999999) and F (2147483647) |
| BUG-030 | Medium | A NaN `SpeedMultiplier` passes both ordered clamps and propagates into `deltaScale` and all per-frame movement | matrix case F (`isnan=1`) plus the `RunOneFrame` clamp trace |
| BUG-031 | Low | Settings-write failures are reported only by `SDL_LogWarn` on category 1, suppressed at SDL3's default priority; `CreateDefaultSettings`'s `rval` check is inert | matrix case H produced no diagnostic at all |
| BUG-032 | High | Highscore and level-history files are parsed with `stoi`/`stof` without exception handling, aborting the client during construction | two full-client runs, exit −6, uncaught `std::invalid_argument` |
| BUG-033 | Medium | Hosting a LAN server runs `system("pkill -x fb-server")`, killing every `fb-server` the user owns | `mainmenu_server.cpp:93`; Task 2 observed an unrelated `fb-server` on this host |
| BUG-034 | High | `FrozenBubble`'s raw members have no initialisers and the constructor's early-return paths leave them indeterminate; `RunForEver` and the destructor then use them | UBSan `member call on misaligned address 0xbebebebebebebebe` at `frozenbubble.cpp:228` |
| BUG-035 | Medium | Controller slots are never released and the slot index is unbounded: from slot 6 in-game controller input silently stops, and from slot 11 the derived scancode passes the 512-entry keyboard array | Full four-hop trace: `frozenbubble.cpp:384-388` (bind-capture emits the derived code) → `frozenbubble.cpp:334-347` (`PushScancode`'s raw-event fallback pushes a real `SDL_Event` once the code leaves `IsVirtualScancode`'s `[300,400)` range) → `mainmenu_input.cpp:498-513` (`KeysPanelKey` stores `e->key.scancode` into `PlayerKeys` unchecked) → `gamesettings.h:50-54` (`IsKeyPressed` falls to the unguarded `SDL_GetKeyboardState(NULL)[sc]` index every frame); no `GAMEPAD_REMOVED` handler; `numkeys=512` probe |
| BUG-036 | Medium | The 20-code stride per player is smaller than SDL3's 26 gamepad buttons, so button ≥ 20 aliases the next player's slot | `SDL_GAMEPAD_BUTTON_TOUCHPAD = 20` versus `CTRL_SC_BASE + slot*20` |
| BUG-037 | Medium | Room-scoped lobby state (`netTeamOverrides`, chat scan counters, `selectedActionIndex`, `currentPlayerCol`) is never reset on part/join, so a previous room's nickname-keyed team overrides apply in the next room | no `netTeamOverrides.clear()` exists; `ReturnToNetLobby`/`MenuEscapeKey` traces |
| BUG-038 | Low | On WASM the game list is rebuilt inside the WebSocket callback between frames, while the join action is an index into that list, so ENTER can join a room other than the highlighted one | `networkclient_wasm.cpp:112-127` parses inline; native frame order makes this safe (see dismissals) |
| BUG-039 | Low | Local multiplayer is structured and documented for five players, but the setup panel caps at four and the key-config screen cycles players 1-4, so player 5's bindings are unreachable | `LocalMPPanelKey` wrap, `KeysPanelKey` wrap, `mainmenu.h` "2-5 players" |
| BUG-040 | Medium | Per-player room options are only expressible for slots 1-5, so in 10/20-player rooms slots 6-20 silently ignore host configuration | `SendOptions` signature and format string; `SetupNewGame` case 4 loops `i < 5` |
| SEC-007 | High | `PLAYERTEAM_Pn` from a peer `OPTIONS:` push is never clamped through the lobby into `SetupSettings.playerTeams`, and gameplay indexes the five-element `kTeamColors` with `team - 1` | five-hop trace in Step 2; cross-links SEC-003 (parsing) and SEC-004 (who may send it) |
| IMP-012 | Medium benefit / Low effort / Low risk | Remove or wire up the menu's unreachable code: the two-player panel, the network setup panel and `networkInputMode == 2`, the `editor` button with no handler, `LevelEditor`/`Netplay` states, the vestigial `selectedGameIndex`, `controllerInputs[5]`, `MainMenu::HandleInput`'s diverted gamepad branch, and the throwing static `GameSettings` initialiser | reachability traces in Step 1 and Step 4 |

## Dismissed candidates

- **`victoriesLimitIndex` out of range for `vLimits[18]`.** Dismissed: every
  write site wraps within 0..17 (`mainmenu_input.cpp:1000-1006`, `:1156-1157`,
  `:791-795`) and the option-receive path assigns the default index 5 before
  searching for a match, so no unmatched remote value can leave the index out of
  range.
- **Remote `NUMCOLORS_Pn` reaching bubble-colour indexing.** Dismissed: both
  gameplay consumers re-clamp to 5..8 (`bubblegame.cpp:392-393` and `:899-900`),
  which also answers the Task 5 handoff about upstream colour validation.
- **Index-based room join racing the server list on native builds.** Dismissed:
  `RunOneFrame` pumps all input before `NetPanelRender` calls
  `NetworkClient::Update`, so the list used by the key handler is exactly the
  list the previous frame rendered. The WASM callback path does not have this
  ordering, which is why BUG-038 is scoped to WASM only.
- **cppcheck `danglingTemporaryLifetime` at `mainmenu_netpanel.cpp:1000` and
  `:1081`.** Dismissed: the conditional operator yields a `std::string`
  prvalue, which is materialised and lifetime-extended by the `const
  std::string&` binding for the reference's full scope; `.c_str()` is used
  inside that scope.
- **clang-analyzer `core.CallAndMessage` null `currentGame` at
  `mainmenu_input.cpp:1166/1178/1189/1200`.** Dismissed: `GameRoomHostReturn`
  has exactly one call site, inside `else if (currentGame)`. The null-tolerant
  expression at line 1109 is misleading but unreachable — folded into IMP-009's
  cleanup class.
- **20 `clang-analyzer-optin.core.EnumCastOutOfRange` casts in
  `frozenbubble.cpp`.** Dismissed as a cast defect: `SDL_Scancode`'s largest
  enumerator is 512, so its value range covers 0..1023 and the 300..399 casts
  are well defined. The real defects in that arithmetic are BUG-035 and
  BUG-036, which are recorded on their own evidence.
- **cppcheck `duplicateCondition` at `mainmenu_netpanel.cpp:675`.** Dismissed:
  the two `if (currentGame)` blocks are sequential and render different
  regions (settings grid, then player sidebar).
- **Over-long nickname overflowing `savedNickname`/`networkPreNick`.**
  Dismissed: matrix case G2 shows a 200-character stored nickname truncated to
  31 characters with no overflow, and every input path bounds its writes.
- **Empty `settings.ini` treated as corrupt.** Dismissed: iniparser returns an
  empty dictionary rather than `NULL`, so case B runs on per-key defaults
  without triggering the reset of BUG-027. Recorded as observed behaviour only.

## Coverage

All fifteen files named by the Task 6 brief received a final disposition; see
[FILE_COVERAGE.md](../FILE_COVERAGE.md). `src/mainmenu_netpanel.cpp` already
carried Task 4's network-boundary disposition and now carries the Task 6 UI and
option-consumption disposition as well. `src/frozenbubble.cpp` and
`src/frozenbubble.h` are complete for input, event routing, and object
lifecycle; their rendering and platform aspects remain owned by Tasks 7-8.
No file in scope is left pending.

Supporting files read as evidence but owned elsewhere:
`third_party/iniparser/iniparser.c` (parse-failure semantics, Task 9 boundary),
`src/networkclient.cpp` option parser (Task 4), `src/bubblegame*.cpp` option
consumers (Task 5), `src/platform.cpp` data-directory resolution (Task 8).

## Limitations

- macOS `SDL_GetPrefPath` ignores `HOME`; isolation required
  `CFFIXED_USER_HOME`. During the first isolation probe the call therefore
  resolved to the user's real preference directory. The harness's isolation gate
  aborted before opening any file, the directory and its three files pre-existed
  (verified by listing and timestamps), and all three were re-verified
  byte-identical after the gate. No other run resolved outside a scenario home.
- Only the settings subsystem could be exercised through a linked production
  object. `HighscoreManager` needs a renderer and TTF, so its defects were
  reproduced through the full client instead; its save path and the
  `AppendToLevels`/`CheckAndAddScore` flows were not exercised at runtime.
- All full-client runs used the dummy video and audio drivers and were killed by
  a timeout at the title screen. No menu navigation, panel transition, key
  rebinding, controller event, room creation, or lobby interaction was driven at
  runtime; every Step 1, Step 2, and Step 4 conclusion is a source trace.
- Per the user's scope restriction, no security-specific runtime testing was
  performed. SEC-007 and the SEC-004 lobby-side consumption were **not**
  reproduced with hostile traffic; they are code-supported inferences. The
  omitted checks (forged `OPTIONS:` from a non-creator, out-of-range team
  values, oversized rooms) remain a final-audit limitation, not a pass.
- No server was started and no connection was opened, so every network lobby
  transition — connect, nickname exchange, create, join, part, room close,
  game start, and return — was reviewed statically only. The LAN discovery,
  public server fetch, latency probe, and `StartLocalServer`/`StopLocalServer`
  paths were likewise not executed; BUG-033 is a source proof, deliberately not
  reproduced because reproducing it means killing processes on this host.
- BUG-035 and BUG-036 were not reproduced: doing so needs six to eleven physical
  gamepad connect/disconnect cycles, which this environment cannot supply. Both
  rest on complete arithmetic traces against SDL3's published constants.
- WASM behaviour (BUG-038, the WASM-only key-config row count, the localStorage
  nickname path) was not executed; no browser runtime was available in this
  gate.
- Windows and Android menu/input paths (`_WIN32` server-hosting stubs, Android
  IAP hotkeys, `SDL_SendAndroidMessage`) were reviewed statically only.

## Gate conclusion

Complete. Every scoped file has a final disposition, every candidate is
confirmed or dismissed with recorded evidence, and the four inherited
cross-owner items (BUG-021, BUG-023, SEC-004, IMP-005) are resolved on their
menu/settings side and cross-linked without recycling IDs. Fifteen defects, one
security finding, and one improvement were promoted; eight of them (BUG-026
through BUG-032 and BUG-034) are reproduced at runtime against unchanged
production code under an isolated preference home, per the persistence matrix
and full-client run table above, and the user's real preferences were verified
untouched. Security runtime work
and all live-server menu transitions remain explicitly omitted. The exact next
gate is Task 7, Step 1: build an SDL resource ownership table.
