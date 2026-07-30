# 09 — Independent Final Challenge Audit Notebook

## Scope

Task 12: independent challenge of every confirmed `BUG`/`SEC`/`REL`, of the
audit's improvement set and its dismissals, of cross-subsystem interactions the
earlier gates may have missed, and of the complete-coverage claim itself.

Reviewer: **Claude Opus**, exact model id `claude-opus-5`, dispatched as the
Task 12 independent challenger with no access to the earlier gates' working
context. Inputs were `CLAUDE.md`, the approved design, the plan,
`SDL3_REVIEW_STATUS.md`, `FILE_COVERAGE.md`, `FINDINGS.md`, notebooks 01-08, the
production source at pinned baseline `09d6c7bfcd864a0ad3951b87d16a88dc770392a3`
(`v2.4.27`), and the preserved evidence under `/tmp/fb-sdl3-audit/`. Host,
compiler, and tool versions are unchanged from the Session environment table.

In-scope for the challenge:

- All **72** confirmed defects: BUG-001..051 less the dismissed BUG-012 (50),
  SEC-001..007 (7), REL-001..015 (15).
- All **24** confirmed improvements, judged for concrete change, verification
  method, and regression risk against evidence-backed benefit.
- The **8** notebooks' `Dismissed candidates` sections (43 dismissal bullets)
  plus inline observations that were conceded and set aside without ever
  becoming candidates.
- The **237**-row coverage inventory and its disposition classes.

Out of scope by standing user restriction: security-specific runtime testing.
No exploit attempt, hostile-traffic test, fuzz run, or offensive runtime probe
was performed. Security findings were challenged **statically only**; where a
security claim can be settled only by such testing that is recorded below as an
open limitation, not as a resolution.

## Trust boundaries and invariants

The challenge re-derived the boundaries the earlier gates asserted rather than
inheriting them. Two boundary statements did not survive.

| Boundary | Earlier gate's statement | Challenge result |
|---|---|---|
| Who may push room `OPTIONS:` | Notebook 04's Step-2 trust table (`04-lobby-settings-input.md`, Peer/host room options row) reads "**any room member can emit `SETOPTIONS`**; see SEC-004" | **Wrong.** `server/game.c:405` gates `setoptions` on `if (g->players_conn[0] == fd)` and answers a non-creator with `wn_not_creator` (`:415`). Only the slot-zero creator can make the server emit an `OPTIONS:` push. Notebook 01's own row (`01-server-protocol.md:99`, "`CLOSE`, `START`, `KICK`, `SETOPTIONS` \| Slot-zero creator only") already recorded this, so the two notebooks contradicted each other. Corrected in notebook 04; SEC-004 and SEC-007 re-scoped |
| Which transition call produces the leaked texture | BUG-041 names "`mainmenu.cpp:497`, inside `SetupNewGame`" and "`bubblegame.cpp:1012`" as "the only two `DoSnipIn` producers", and rules out menu return because "`QuitToTitle` clears `firstRenderDone` with no `DoSnipIn`" | **Inverted.** `DoSnipIn` (`transitionmanager.cpp:48-60`) only captures `snapIn`; it calls no `effect()`. The animation — and therefore the leak — is produced by `TakeSnipOut` (`:62-75`), whose sole call site is `bubblegame_render.cpp:1173`, armed by `!firstRenderDone`. `firstRenderDone` is cleared at `bubblegame.cpp:1013`, at `bubblegame.cpp:1363` (**`QuitToTitle`** — the very site the correction used to *exclude* menu return), and by the `bubblegame.h:479` initializer. Clearing the flag is what arms the animation. Corrected in BUG-041 and notebook 05 |
| Server-supplied line length vs client receive buffer | Notebook 02 static-review row (`:153`) concedes "a chunk that would fill the buffer is silently skipped" and disposes of it with "Ordinary server messages fit the buffer … Overflow/flood behavior remains part of the untrusted-input limitation" | **Both halves fail.** `send_line` (`server/net.c:126-146`) emits a single line up to 16383 bytes and `list_games_str` is `[16384]` (`game.c:134`); the client's `recvBuffer` is `BUFFER_SIZE` = 4096 (`networkclient.h:36`, `:213`). The open-player list alone contributes up to 25 bytes per lobby client (`game.c:139-151`), and `max_users` defaults to 255 (`net.c:82`), so a legitimately busy lobby exceeds 4095 with no hostile input. And the skip is not a dropped chunk: `recvBufferLen` is reduced only *inside* the guard it fails, so the state is absorbing. Promoted to **BUG-052** |

Invariants re-derived and found to hold as the earlier gates stated them:

- **Seat-id window.** `next_seat_id` (`server/game.c:233-248`) stays inside
  `['A','z']` and skips live ids; 58 values against a 20-seat cap.
- **Peer message fan-out bounds.** Every `senderId` consumer in
  `src/bubblegame_net.cpp` is a linear scan bounded by
  `currentSettings.playerCount` (12 loops at `:108`, `:202`, `:269`, `:313`,
  `:367`, `:417`, `:456`, `:489`, `:527`, `:600`, `:628`), with an explicit
  "unknown senderId" reject rather than an unchecked index.
- **Board array capacity.** `bubbleArrays[MAX_NET_PLAYERS]`
  (`src/bubblegame.h:549`, `MAX_NET_PLAYERS == 20`, `:251`) matches the server's
  `MAX_PLAYERS_PER_GAME` and the client's `kRoomSizes[3] = {5, 10, 20}`
  (`src/mainmenu_internal.h:35`), so no player-count path indexes past the array.
  `CLAUDE.md:54`'s `bubbleArrays[5]` / "1–5 players" is stale documentation, not
  a code defect — folded into REL-009.
- **Virtual-scancode window.** `IsVirtualScancode` (`src/gamesettings.h:45-47`)
  bounds `[CTRL_SC_BASE, CTRL_SC_BASE + CTRL_SC_COUNT)`, i.e. `[300,400)`, so
  `virtualKeyState[100]` is never indexed out of range. BUG-028/035/036's
  out-of-range consequence therefore lands on `SDL_GetKeyboardState(NULL)[sc]`
  (`:53`), exactly as those findings state.
- **Renderer/audio lifetime across state changes.** `SDL_CreateWindow`
  (`frozenbubble.cpp:135`) and `SDL_CreateRenderer` (`:152`) run once;
  `SDL_DestroyRenderer` (`:198`) runs once, in the destructor. Fullscreen is
  changed with `SDL_SetWindowFullscreen` (`:509`, `mainmenu_input.cpp:606`),
  which does not invalidate textures. There is no renderer-recreation path, so
  no texture can outlive its renderer across a state transition. The brief's
  "renderer/audio teardown during state changes" category yields nothing beyond
  the recorded findings.

## Static review

### Step 2 — challenge of every confirmed defect

Each of the 72 confirmed defects was challenged on five axes: is the cited code
what the finding says it is at the pinned commit; is the path reachable and by
what input; does the claimed impact follow; is the root cause right rather than a
symptom; is the platform applicability correct. Dispositions:

| Disposition | Count | IDs |
|---|---|---|
| **upheld** (citations exact, claim stands as written) | 62 | BUG-001, 003, 004, 005, 006, 007, 008, 009, 010, 011, 013, 014, 015, 016, 017, 018, 019, 020, 021, 022, 023, 024, 025, 026, 027, 028, 029, 030, 031, 032, 033, 034, 035, 036, 037, 038, 039, 040, 042, 043, 044, 045, 046, 047, 048, 049, 050, 051; SEC-003, 006; REL-001, 003, 004, 005, 006, 007, 008, 011, 012, 013, 014, 015 |
| **revised** (defect stands; a stated element corrected) | 9 | BUG-002, BUG-041, SEC-001, SEC-002, SEC-004, SEC-005, SEC-007, REL-002, REL-009 |
| **dismissed** (not a defect) | 0 | — |

No confirmed defect was found to be a false positive. The nine revisions are
recorded in full in `Confirmed findings` below and applied to the registry.

Load-bearing verifications behind the 62 upheld dispositions (one line each,
citations re-derived from the pinned source, not copied from the notebooks):

- **BUG-003** `remove_prio` has exactly one caller, `game.c:838` (the `PART`
  branch). Neither `kick_player` (`:476-488`) nor the creator-left whole-room
  free (`:1056-1073`) releases prio membership, and `process_msg_prio_`'s
  `find_game_by_fd` failure path is `exit(EXIT_FAILURE)` (`:986-987`).
- **BUG-004** `amount_transmitted` (`game.c:66`) is read and zeroed in
  `get_reset_amount_transmitted` (`:917-921`) and incremented nowhere in
  `server/`; the rate consumer is `net.c:604-611`.
- **BUG-005** first win at `game.c:1051-1054`, second at `:1025-1030`
  (`was_running && players_number == 0` records a win for `save_nick`, the
  departing player).
- **BUG-006** one `recv(fd, req, sizeof(req)-1, 0)` at `ws.c:139` after the
  4-byte `MSG_PEEK` at `:134`; a header without `Sec-WebSocket-Key` in that one
  read returns 0 at `:147` with the bytes already consumed.
- **BUG-007** `ws_send` returns the raw `send()` result (`ws.c:202`) and
  `game.c:967` normalises any nonnegative return to `len`.
- **BUG-008** `players_nick[i]` holds owned `strdup` results (`game.c:214`,
  `:274`); the creator-left branch frees only `g` (`:1071-1072`) and `save_nick`
  (`:1083`), leaking every surviving seat's nickname.
- **BUG-009** `is_nick_ok("")` returns 1 (`game.c:612-626`: the length guard is
  `> 10` and the character loop does not execute), and `args` is non-NULL for
  `FB/1.3 CREATE ` (`:662-666`).
- **BUG-010** `conn_recently_active` is `(current_time - last_data_in[fd]) < 2`
  on a whole-second cache (`net.c:247-250`), consumed at `game.c:695`.
- **BUG-011** `start_game` (`game.c:347`) does not test status, so a repeat
  `START` re-enters `real_start_game` and zeroes `players_started[]` (`:342`);
  the renewed `ok_start_game` calls `add_prio` (`:461`), which removes the fd
  from `new_conns` (`net.c:679`) — the list that becomes `conns_prio` at
  `net.c:563` — while `conns` never receives it.
- **BUG-013** `Disconnect` (`networkclient.cpp:185-197`) nulls `currentGame`
  with no `delete` (5 `new GameRoom()` sites, **0** `delete currentGame` in the
  tree) and does not reset `recvBufferLen`.
- **BUG-014** `qSize < 40` on `MessageQueueSize()` (`bubblegame_render.cpp:547`)
  while `ProcessNetworkMessages` routes every `b|`/`N`/`T` into `syncQueue`
  (`bubblegame_net.cpp:645`). The scoping to round 2+ is correct: the
  initial-start twin at `mainmenu_netpanel.cpp:147` counts the same queue but
  runs while `BubbleGame` is not driving `ProcessNetworkMessages`, so nothing
  diverts and the count does reach 40.
- **BUG-016** `bool ok = (select(s + 1, nullptr, &wfds, nullptr, &tv) > 0)`
  (`networkclient.cpp:1582`) with no `SO_ERROR` check.
- **BUG-017** `SendCommand` checks only `sent < 0` (`networkclient.cpp:208-213`).
- **BUG-018** `SetupSettings::clearMode` (`bubblegame.h:267`) has **zero** read
  sites in any `bubblegame*.cpp`; the board-clear win at
  `bubblegame_state.cpp:682-688` declares a multiplayer winner unconditionally.
- **BUG-020** `malusQueue.clear()` occurs only in `ReloadGame`
  (`bubblegame.cpp:1210`), bounded by the *new* `playerCount`; `NewGame`
  (`:337`) never clears it.
- **BUG-021** `SetupSettings` (`bubblegame.h:253-276`) has no
  `continueWhenPlayersLeave` field; the value exists only at `mainmenu.h:202`.
- **BUG-022** the `chainedGroupBubbles` BFS hard-codes `if (r % 2 == 0)`
  (`bubblegame_board.cpp:266-271`) while the two sibling traversals in the same
  function use `GridNeighborOffsets(row, oddswap)` (`:120`, `:199`) with
  `oddswap` derived from `bubbleMap[0].size()`.
- **BUG-023** `showing2PPanel` is assigned only `false` (`mainmenu.cpp:614`;
  declared false at `mainmenu.h:132`), and `selectedMode` is only ever 0, 3, 6,
  or 7 (`mainmenu.cpp:305`, `:387`, `:395`, `:622`), so `SetupNewGame(2)` is
  unreachable. Local MP's `ns7` (`mainmenu.cpp:583-598`) never sets
  `victoriesLimit`.
- **BUG-024** `s` sets `mpStickPending` (`bubblegame_net.cpp:233`); it is applied
  later inside `UpdateSingleBubbles` (`bubblegame_shooter.cpp:503-510`).
- **BUG-026** unbounded `while (optDict == NULL)` (`gamesettings.cpp:168-173`).
- **BUG-027** the repair path opens the user's file with
  `fopen(setPath, "w")` (`gamesettings.cpp:94`) — truncating it — before writing
  any default.
- **BUG-028** `static_cast<SDL_Scancode>(iniparser_getint(...))` ×20
  (`gamesettings.cpp:210-236`) with no validation, reaching
  `SDL_GetKeyboardState(NULL)[sc]` (`gamesettings.h:53`).
- **BUG-029** `if (windowHeight < 480 || windowWidth > 9999) windowHeight = 480;`
  (`gamesettings.cpp:183`).
- **BUG-030** ordered clamps at `gamesettings.cpp:191-192`; NaN satisfies neither.
- **BUG-031** `SDL_LogWarn(1, ...)` (`gamesettings.cpp:47`) on category
  `SDL_LOG_CATEGORY_ERROR`, whose SDL3 default priority is `ERROR`; and
  `rval` is unconditionally set to 1 at `gamesettings.cpp:137` before the
  `if (rval < 0)` branch at `:140`, making that branch unreachable.
- **BUG-032** four `stoi`/`stof` calls (`highscoremanager.cpp:83`, `:85`, `:87`,
  `:129`) and **zero** `try`/`catch` in the file.
- **BUG-033** `system("pkill -x fb-server 2>/dev/null")`
  (`mainmenu_server.cpp:93`); stubs at `:80-83` and `:173-174`; `portInUse`
  returns false at `:52-53` on `_WIN32`. All three citations exact.
- **BUG-034** six raw members with no initializer (`frozenbubble.h:85-96`); the
  asset-verification early return at `frozenbubble.cpp:110`; `RunForEver`
  dereferences `audMixer` at `:229` before testing `IsGameQuit` at `:242`; the
  destructor dereferences `hiscoreManager`/`audMixer`/`gameOptions` at
  `:207-209`; `main.cpp:29-30` calls `RunForEver` unconditionally.
- **BUG-035** no `SDL_EVENT_GAMEPAD_REMOVED`/`JOYSTICK_REMOVED` handler and no
  `controllers.erase`/`clear` anywhere in `src/frozenbubble.cpp`.
- **BUG-036** stride 20 at `frozenbubble.cpp:386` and `:406` against
  `SDL_GAMEPAD_BUTTON_COUNT` (SDL3 `SDL_gamepad.h:181`) = 26.
- **BUG-037** `netTeamOverrides` (`mainmenu.h:218`) has read/write sites at
  `mainmenu.cpp:560`, `mainmenu_netpanel.cpp:720`, `:891`,
  `mainmenu_input.cpp:140`, `:144` and **no** `clear`/`erase` site.
- **BUG-038** join is `gameIndex = selectedActionIndex - 2` into a freshly
  fetched `GetGameList()` (`mainmenu_input.cpp:1280-1285`); the WASM list is
  rebuilt from `onWebSocketMessage` (`networkclient_wasm.cpp:77`, registered at
  `:188`). Bounds-checked, so a wrong-room join — consistent with Low.
- **BUG-039** `"Key config  Player %d/4"` (`mainmenu_panels.cpp:471`).
- **BUG-040** `SendOptions` takes `[5]` arrays (`networkclient.cpp:521`) and the
  receive loop is `for (int i = 0; i < 5; i++)` (`:1191`).
- **BUG-042** `71 + 97 + 68 + 158 = 394` (`bubblegame.h:55-58`) loaded in
  `LoadPenguin` (`:105-126`); **no** `SDL_DestroyTexture` for any penguin frame
  or `hurryTexture` among the 19 destroy sites in `src/`.
- **BUG-043** `targetingText` (`bubblegame.h:535`) has three use sites
  (`bubblegame_render.cpp:946`, `:956`, `:957`) and **no** `LoadFont`;
  `UpdateText` returns at `ttftext.cpp:48`; the default constructor
  (`ttftext.cpp:22-24`) leaves `coords` indeterminate.
- **BUG-044** `activeSPButtons[i] = IMG_Load(...)` unchecked
  (`mainmenu.cpp:124`), dereferenced at `mainmenu_panels.cpp:198`/`:213`.
- **BUG-045** copy constructor `ttftext.h:52` reinitialises every member; copy
  assignment `ttftext.h:53` is `{ return *this; }`.
- **BUG-046** `if (dest.exists() && dest.length() > 0) return;` at
  `AssetExtractor.java:106`; unconditional marker write at `:68-74`; swallowed
  exceptions at `:98-100` and `:115-117`; early return at `:53-61`. Every line
  number exact.
- **BUG-047** five CWD-relative `stat` probes (`frozenbubble.cpp:82-97`),
  discarded return at `:99`, `fopen`-failure return before
  `SDL_SetLogOutputFunction` (`logger.cpp:96-100`).
- **BUG-048** `SDL_GetPrefPath("", "frozen-bubble")` (`gamesettings.cpp:32`);
  four `fb_nickname` sites — one `getItem` (`mainmenu.cpp:163`) and three
  `setItem` (`mainmenu.cpp:264`, `mainmenu_netpanel.cpp:80`,
  `mainmenu_input.cpp:1507`); `RunForEver` returns at `frozenbubble.cpp:238-239`.
- **BUG-049** every cited line is exact: `game.c:977` append, `:982`
  `g_list_foreach`, `:1033-1034` `g_list_remove`/`free(g)`, `:1044` the relay
  that can recurse, `:1051` the post-relay `g->players_number` read, and
  `net.c:226`'s own comment naming the recursion. The 3-seat minimum follows:
  3→2 after the outer decrement, then two failed relays terminate 2→1 and 1→0,
  freeing `g` before the outer frame resumes at `:1051`. The
  `stats_record_win` continuation is confirmed as a dangling-pointer *argument*
  (`stats.c:157-178` hashes, `g_strdup`s and `stats_save()`s the nick), matching
  the finding's own hedge.
- **BUG-050** `players_in_game` accumulates only in the non-OPEN branch
  (`game.c:172`); `games_open` is computed at `:158` and never emitted;
  `free:` is `conns_nb() - players_in_game - 1` (`:202`).
- **BUG-051** unguarded `level[idx]` (`bubblegame_level.cpp:67`) against the
  guarded sibling `if (idx < 10)` (`highscoremanager.cpp:133`).
- **SEC-003** confirmed through the same peer-field path SEC-007 uses.
- **SEC-006** `charstar_to_int` (`tools.c:46-54`) accumulates
  `number = (number * 10) + (*s - '0')` with no width or overflow check.
- **REL-001** `%zd` against `size_t` at `tools.c:81` and `:91`.
- **REL-003** `MSG_DONTWAIT` is `#define`d to `0` on Windows
  (`socket_compat.h:29-31`); three per-frame `recv(..., MSG_DONTWAIT)` sites
  (`networkclient.cpp:227`, `:825`, `:1277`) run on `sockfd`, and the only
  `ioctlsocket(FIONBIO)` in the tree (`:1567-1568`) is applied to the
  short-lived reachability-probe socket, never to `sockfd`.
- **REL-004** all five strings verified at their exact cited lines:
  `server/CMakeLists.txt:28` `2.2.1`, printed at `fb-server.c:43` and sent at
  `net.c:1160`; `net.c:1098` `2.4.9`; `platform.h:23` `v2.4.26`, rendered at
  `mainmenu_panels.cpp:471`; `android/app/build.gradle:15` `2.4.27` with the
  workflow fallbacks at `build.yml:128`/`:233`; `default.nix:22` `0.1.0`. Plus
  `versionCode 10` at `build.gradle:14` and **zero** `versionCode`/`versionName`
  occurrences in the workflow.
- **REL-005** `git ls-tree -r <pinned> android/app/jni/include/SDL2/ | grep -c
  '^120000'` = **97**.
- **REL-006** central count reproduced: 15 unique `src/*.cpp` in
  `CMakeListsEmscripten.txt` against an effective 28 (root `CMakeLists.txt`'s
  27 explicit entries at `:62-89` plus `networkclient.cpp`), and 28 in
  `android/app/CMakeLists.txt`, set-equal to native.
- **REL-007** `buildTypes.release` (`android/app/build.gradle:43-47`) declares no
  `signingConfig`; the literal password appears at `build.yml:396`, `:397`,
  `:404`, `:406` — four occurrences spanning the two steps the corrected
  citation names (`:390-398` and `:400-413`); the two `frozenbubble` occurrences
  at `:394` and `:405` are the key *alias*, correctly excluded from the count.
- **REL-008** `INSTALLED_ASSET_PATH` is set at `CMakeLists.txt:224` and consumed
  only by the `message(STATUS ...)` at `:225`; `DATA_DIR` comes from
  `ASSET_PATH` at `:140`, defaulted to `${CMAKE_SOURCE_DIR}/share` at `:128`.
- **REL-011** `uses:` = **27**, `@master` = **5** (all
  `josephbmanley/butler-publish-itchio-action`, at `:524`, `:543`, `:562`,
  `:581`, `:600`), commit-pinned = **0**.
- **REL-012** `CMAKE_OSX_ARCHITECTURES|universal|-arch ` = **0** occurrences in
  both `build.yml` and `CMakeLists.txt` (`grep -c` exit 1).
- **REL-013** DLLs = **21** (`grep -oE '[A-Za-z0-9_.+-]+\.dll' | sort -u | wc -l`),
  confirming the Fix Round 1 correction; the workflow's only `env:` block
  (`:14-15`) defines `FORCE_JAVASCRIPT_ACTIONS_TO_NODE24` alone, so
  `${{ env.ANDROID_SDK_ROOT }}` at `:345` interpolates empty.
- **REL-014** `third_party/iniparser` holds exactly **4** tracked files at the
  pinned commit and `grep -ci iniparser COPYING` exits 1 with **0** matches.
- **REL-015** `getenv("HOME")` → `<HOME>/.fb-server/stats.dat`
  (`server/stats.c:87-92`) with a `/var/lib` fallback only when `HOME` is unset,
  and no flag or cwd-relative alternative anywhere in the file.

### Step 3 — challenge of improvement value and scope

All 24 `IMP` entries were read against three rejection criteria: generic cleanup
with no evidence, speculative rewrite, or already satisfied. **None** was
rejected outright, and no `IMP` was promoted to a defect by this gate.

- IMP-001..004, IMP-011, IMP-022, IMP-023 name a specific file/line change with
  a stated consequence; kept.
- IMP-005, IMP-006, IMP-007, IMP-008, IMP-009, IMP-010, IMP-012, IMP-015 are
  the analyzer-family and dead-code entries. Their value rests on the *defects
  already promoted out of them* (BUG-034, BUG-043, BUG-045), which is
  evidence-backed rather than generic. Kept, with the observation that IMP-005
  and IMP-007 carry the highest regression risk of the set (changing
  initialisation and copy semantics on classes with 38 fixed `TTFText` members)
  and that their own registry rows say so.
- IMP-013 **revised**: its site enumeration is incomplete. It names
  `get_pixel`'s own clamp (`shaderstuff.cpp:49`) and the two `set_pixel`
  callers (`:488`, `:1155`), and correctly states that `set_pixel` itself
  (`:41-45`) has no clamp. It omits a fourth site of the identical family —
  `CLAMP((int)points[i].x, 0, mask->w)` / `CLAMP((int)points[i].y, 0, mask->h)`
  at `shaderstuff.cpp:1158`, feeding a read of `mask->pixels`. Added.
- IMP-014, IMP-024 quantify their benefit (2,666,728 bytes; a reproduced
  `CREATE cap21 21` fallback) and name their change. Kept.
- IMP-016..IMP-021 are the test-suite entries. Each names a location, inputs,
  assertions, and a platform matrix, and each assertion traces to a registered
  defect ID. IMP-016's benefit is measured (**0** `ctest`/`pytest` invocations
  across 11 jobs, 5 registered tests) and reproduced here. Kept. One scope
  gap: **IMP-021** claims Linux-only leak coverage for BUG-041 with "ten
  transition animations covering all five effect families with a seeded RNG",
  but a harness that drives `DoSnipIn` will not animate anything — per the
  BUG-041 correction the driver must clear `firstRenderDone` and then call
  `TakeSnipOut`. Recorded against IMP-021 so the test it specifies can actually
  fail.

### Step 4 — search for missed cross-subsystem interactions

Each category the brief names was executed as its own sweep.

| Category | Result |
|---|---|
| Server/client length mismatches | **New defect.** `send_line`'s 16383-byte line ceiling against the client's 4096-byte `recvBuffer`, with an absorbing skip state — **BUG-052** |
| Lobby-to-game option drift | Re-derived the full `SETOPTIONS` key list (`networkclient.cpp:521-541`) against `SetupSettings` (`bubblegame.h:253-276`). Three drifts exist and all three are already registered: `CONTINUEGAMEWHENPLAYERSLEAVE` has no field (BUG-021), `clearMode` has a field with zero readers (BUG-018), `PLAYERTEAM_Pn` is unclamped (SEC-007). `VICTORIESLIMIT` **is** propagated and enforced on the network path (`mainmenu.cpp:548` → `bubblegame_state.cpp:547`, `:625`, `:711`, `bubblegame_net.cpp:468`); only the unreachable local-2P path lacks it (BUG-023). No fourth drift |
| Player-ID mapping | All 12 `senderId` scans bounded by `playerCount`, with explicit unknown-sender rejects. Seat-id allocator bounded. No finding |
| Round resets | `malusQueue` confirmed as BUG-020 states. Match totals (`m*`) persisting across rounds is deliberate and commented (`bubblegame.cpp:1217`). No additional finding |
| Renderer/audio teardown during state changes | Single create/destroy pair; no recreation path. No finding |
| Platform source-list drift | 28 native / 28 Android / 15 Emscripten reproduced; already REL-006 |
| Package/runtime path drift | Already REL-008, BUG-047, BUG-048, REL-006 |
| Settings/input effects on gameplay/network state | `speedMultiplier` → `deltaScale` is BUG-030; key scancodes are BUG-028; `mouseEnabled` is overwritten per-session at `mainmenu.cpp:568` (deliberate). No additional finding |

### Step 5 — audit of the coverage claim itself

Re-derived rather than re-read.

1. **Inventory reconciliation.** Task 1 Step 4's own selection command was
   re-run against the pinned tree (3623 paths) and diffed against the 237 paths
   extracted from `FILE_COVERAGE.md`'s own rows. `rg` matched exactly **237**;
   `diff` produced no output and exited **0**. The inventory claim holds.
2. **Disposition census.** Every row's disposition cell was tabulated: **42**
   distinct strings summing to **237** (Fix Round 1 correction: originally
   miscounted as 41 — the tabulation predated this same gate's own edit to the
   `src/networkclient.cpp` row a few lines below, whose new disposition text is
   a singleton class the pre-edit count never saw; re-verified in Fix Round 1,
   see [Task 12 Fix Round 1 Findings](../SDL3_REVIEW_STATUS.md#task-12-fix-round-1-findings)).
   The largest classes are 97 "Vendored; boundary reviewed — defect confirmed"
   (the REL-005 symlinks), 21 "Complete", 19 "Vendored; boundary reviewed", 15
   "Reviewed; defect confirmed", and 14 "Reviewed; no defect". Every class was
   sampled, including the vendored and binary ones and (in Fix Round 1) the
   `src/networkclient.cpp` singleton, not only the easy ones.
3. **No hidden pending state.** At the time of measurement a case-insensitive
   whole-file search found the word `pending` **once**, inside the
   inventory-rule paragraph that defines it, so the "0 rows carry a pending
   disposition" claim holds. That whole-file phrasing is self-referential —
   writing this note added a second prose occurrence — so both `FILE_COVERAGE.md`'s
   rule paragraph and this notebook now state the check that actually matters:
   a case-insensitive search restricted to the ledger's **disposition cells**
   (`awk -F'|' 'NF>4 && $2 ~ /`/ {print $4}' | grep -ci pending`) returns **0**
   (exit 1). That form stays true no matter what later prose is added.
4. **Link integrity.** All internal links across `FILE_COVERAGE.md`,
   `FINDINGS.md`, `SDL3_REVIEW_STATUS.md` and notebooks 01-09 were resolved
   programmatically against the target files' actual headings: **0** missing
   files and **0** missing anchors out of every Markdown link reference.
5. **Notebook structure.** Notebooks 01-08 each carry exactly the ten required
   headings, once each, in the required order.
6. **Registry integrity.** 97 rows, **0** duplicate IDs, **0** gaps in any
   class (BUG 1-51, SEC 1-7, REL 1-15, IMP 1-24), and states are exactly 96
   `confirmed` + 1 `dismissed` — no `suspected` or `investigating` remains.
7. **Disposition classes actually used.** The design defines five classes. The
   ledger uses three (fully reviewed; reviewed at a boundary; generated/binary
   validated through its source or build) and uses neither *Excluded with a
   recorded reason* nor *Blocked with a recorded limitation* on any row. Blocked
   checks — browser runtime, Android device runtime, Linux/Windows execution,
   packaged-artifact startup, security runtime — are recorded in the status
   file's Limitations instead of on the affected rows. That is defensible (the
   blocks are on *checks*, not on reading the files) but it means a reader of
   `FILE_COVERAGE.md` alone cannot see which rows rest on an unavailable
   platform. Recorded as a limitation, not a defect.
8. **Paths outside the inventory.** Six maintained tracked paths are in the
   design's declared scope yet carry no coverage row, because Task 1 Step 4's
   selection pattern does not match them: `CLAUDE.md`, `CHANGELOG.md`,
   `.gitignore`, `.gitmodules`, `COPYING`, and the four
   `third_party/iniparser/*` files. Five of them are **cited as evidence for
   confirmed findings** — `CLAUDE.md` and `CHANGELOG.md` for REL-009,
   `.gitignore:8` for REL-006, `COPYING` and `third_party/iniparser/` for
   REL-014 — so the audit reviewed them without dispositioning them. The status
   file already records the `CLAUDE.md`/`CHANGELOG.md` half honestly; the other
   four were not recorded. Also asymmetric: the Android duplicate
   `android/app/jni/iniparser/*` has four rows while the primary copy linked
   into every desktop, Windows and WASM artifact has none. Recorded below; not
   promoted, because the design excludes vendored iniparser internals and the
   remaining files are documentation/VCS metadata whose *content* was in fact
   reviewed.

### Step 5b — challenge of the audit's dismissals

The brief's instruction to hold dismissals to the same standard was executed in
two passes.

- **Explicit dismissals.** All 43 bullets in the eight `Dismissed candidates`
  sections were read. Each states a mechanism and traces a consequence rather
  than merely noting the absence of a failure. Sampled in depth: BUG-012 (the
  prio-ingress `FB/` diversion and the `PART`/`remove_prio` branch — confirmed
  at `net.c:355-357` and `game.c:838`), the `idleSPButtons` `SDL_GetTextureSize`
  dismissal (Fix Round 2 disproved its own proposed mechanism, which is the
  right outcome), the `cmake_uninstall.cmake.in` `exec_program` dismissal
  (disproved by running it), and the `netlify.toml` publish-directory
  dismissal (correctly recorded as unresolvable from repository evidence rather
  than resolved in the project's favour). **No explicit dismissal was
  overturned.**
- **Conceded-and-set-aside inline observations.** These never became
  candidates, so the "every candidate is confirmed or dismissed" completion
  condition does not reach them. A bounded language sweep over notebooks 01-08
  found **6** such passages. Five are sound: `02:59` and `02:322`
  (`recvBuffer` byte staleness and the WASM split-line question, both already
  inside BUG-013's scope), `05:255` (`menuText` genuinely has zero references),
  `06:264` (`readBytes` ignoring a short read on a version-marker file),
  `06:863` (surplus COOP/COEP headers, with the consequence traced). The sixth,
  `02:153`, concealed **BUG-052**. This is the single highest-value result of
  the challenge and the reason the class is now named explicitly in the
  Limitations below.

## Dynamic evidence

This gate started **no** process, listener, server, client, proxy, container, or
background job, and killed nothing. Every verification above is a read of the
pinned source or a measurement over tracked files. The one runtime observation
recorded is passive and was made on processes that predate the gate by days.

**REL-002 observed in the wild.** `pgrep -x fb-server` reports the same four
foreign processes Task 10 enumerated and left untouched. One of them is
`fb-server -p 15113 -q -l -z`, launched from
`tools/server_tests/../../build/server/fb-server`, with an elapsed time of
**1 day 21 h 11 m**. `lsof -nP -p 74458 -a -i` shows it holding
`UDP *:1511` and `TCP *:15113`. That is precisely the orphan REL-002 predicts:
`tools/server_tests/test_room_caps.py:113-114` launches the server with a fixed
`PORT = 15113` (`:27`) and `-l` and **without** `-d`, so it daemonizes; the
harness's `server_proc.terminate()` (`:126`) and `kill()` (`:130`) reach only
the launcher. The consequence has a name: `create_udp_server`
(`server/net.c:751-769`) responds to a failed `bind` of UDP 1511 with
`perror("bind UDP 1511"); exit(EXIT_FAILURE)`, so while this orphan lives, every
later `fb-server -l` on the host — including `start-server.sh:68` and the
client's own `StartLocalServer` path — dies at startup. REL-002 therefore moves
from a static harness-hygiene argument to an **observed** defect with a named
denial-of-service-on-self consequence.

The orphan was **not** terminated: it is not this gate's process, it belongs to
a harness run that predates the audit branch's Task 10 baseline, and the audit's
own rule is that foreign processes are left untouched. It is reported for the
operator to clear.

## Candidates

| Candidate | Origin | State | Outcome |
|---|---|---|---|
| Client lobby/in-game receive buffer permanently stops draining once a server line exceeds it | Step 4 length-mismatch sweep; conceded and set aside at `02-network-client-sync.md:153` | confirmed | **BUG-052** |
| `setoptions` has no creator check (implied by notebook 04's trust table) | Step 2 challenge of SEC-004/SEC-007 | dismissed | `server/game.c:405` enforces slot-zero creator authority. The *notebook* was wrong, not the server |
| `SEC-002` reachable in the project's own deployments | Step 2 challenge of SEC-002 | dismissed as stated, retained as a qualification | All three `http_get` call sites are `!quiet`-gated; both documented launch paths pass `-q` |
| `IsVirtualScancode` admits an out-of-range index | Step 4 settings/input sweep | dismissed | `gamesettings.h:45-47` bounds `[300,400)` |
| `playerCount` can exceed the `bubbleArrays` bound | Step 4 player-ID sweep | dismissed | Array is `[MAX_NET_PLAYERS]` = 20, matching the server cap; `CLAUDE.md:54`'s `[5]` is stale text, folded into REL-009 |
| A texture can outlive its renderer across a state transition | Step 4 teardown sweep | dismissed | One create/destroy pair; fullscreen changes do not invalidate textures |

No candidate remains open.

## Confirmed findings

### BUG-052 (new, High) — the client's receive buffer has an absorbing full state

`NetworkClient::ProcessIncomingData` (`src/networkclient.cpp:822-921`) appends
each `recv` under one guard:

```c
if (recvBufferLen + received < BUFFER_SIZE) {
    memcpy(recvBuffer + recvBufferLen, tempBuffer, received);
    recvBufferLen += received;
    ...                 /* the entire parse-and-drain body */
}
return true;
```

There is no `else`. Every statement that *reduces* `recvBufferLen` — `:894`
(in-game path), `:914` and `:916` (lobby path) — lives inside that body. The
constructor sets it to 0 (`:38`) and `Disconnect` does not reset it (BUG-013).
So once `recvBufferLen + received >= BUFFER_SIZE` holds, the body is skipped,
`recvBufferLen` is frozen, and the condition holds for **every** subsequent
`recv` with `received >= 1`. The state is absorbing: the connection is
permanently deaf while the socket stays open and `recv` keeps succeeding, so
`Update`'s read loop (`:810-818`) spins through its 100-iteration cap each frame
discarding everything and reports only "network buffer was filling up".

`BUFFER_SIZE` is **4096** (`src/networkclient.h:36`), and `recvBuffer` is
`char[BUFFER_SIZE]` (`:213`).

The server can emit a single line far larger than that, on the ordinary path:

- `send_line` (`server/net.c:126-146`) formats into `static char buf[16384]` and
  sends up to `sizeof(buf)-1` = **16383** bytes as one line.
- The `LIST` reply is `list_games_str`, `static char [16384]`
  (`server/game.c:134`), sent through `send_line_log` (`net.c:148-152`).
- `list_open_nicks_aux` (`game.c:139-151`) appends `NICK[:GEOLOC],` per
  connection in `open_players`: nick ≤ 10 (`is_nick_ok`, `game.c:615`) plus
  `:` plus geoloc ≤ 13 (`game.c:746-747`) plus `,` — up to **25** bytes each.
- `max_users` defaults to **255** (`server/net.c:82`, `:88`).

255 lobby clients therefore contribute up to 6375 bytes before a single room is
listed; even with no geolocations set, 255 nicks (2805 B) plus 16 open rooms of
20 seats (`games_open == 16` cap at `game.c:773`; ≈ 227 B each) exceeds 4095.
No malformed, oversized, fragmented, or flooded input is required — a
legitimately busy public server, of the kind this project deploys
(`docker/docker-compose.yml`, `README.md`'s public server list), produces it.

**Impact.** The affected client's lobby stops updating and every server push is
discarded, including `GAME_CAN_START`, `OPTIONS:`, `TALK` and `KICKED`; if the
state is `IN_GAME`, every relayed peer frame is discarded, so the boards
diverge with no error. Nothing reports the condition to the user, and only a
reconnect that also reconstructs the object clears it — `Disconnect` does not.

**Not memory corruption.** The guard is a strict `<` and `recvBuffer[recvBufferLen]`
at `:898` is written only when `recvBufferLen <= BUFFER_SIZE - 1`, so no write
leaves the array. This is a functional/reliability defect.

**Severity High**, confidence High: serious multiplayer desynchronization plus a
permanently unusable lobby connection, reached by ordinary traffic. **Not
reproduced** — reproduction needs either ~165 concurrent lobby connections or a
deliberately over-long server line, and the second is barred by the standing
security-runtime restriction while the first was outside this gate's process
budget. Recorded as a limitation, with the causal chain complete.

**Correction shape** (not applied; this audit changes no production source):
grow `recvBuffer` to at least the server's 16384-byte line ceiling *and* give
the guard an `else` that reports and recovers — either by draining complete
lines out of the full buffer first, or by treating an unterminated
buffer-filling line as a protocol error and disconnecting, the way the server
itself does (`net.c:331-334`, `fl_client_nolf` + `conn_terminated`). Reset
`recvBufferLen` in `Disconnect`. **Verification:** a unit test that feeds
`ProcessIncomingData` a 5000-byte line and asserts the next well-formed line is
still parsed — the assertion belongs in IMP-019's `tests/netclient_parse_test.cpp`.

### Revisions applied to existing findings

| ID | Revision | Evidence |
|---|---|---|
| **BUG-002** | Reachability qualified. `sigterm_catcher` (`server/net.c:996-1001`) is unsafe in every configuration through `l0()` and `exit()`, but its DNS/HTTP/allocation members reach only through `unregister_server` (`:1330-1333`), which is gated on `!quiet && !lan_game_mode`. Both documented launch paths pass `-q` **and** `-l` (`start-server.sh:68`, `docker/Dockerfile:31`), so the network half of the unsafe set is not reachable there | `net.c:996-1001`, `:1330-1333`; `start-server.sh:68`; `docker/Dockerfile:31` |
| **BUG-041** | Trigger set corrected. The leak is produced by `TakeSnipOut` → `effect` → `synchro_after`, whose sole call site is `bubblegame_render.cpp:1173`, armed by `!firstRenderDone`. `firstRenderDone` is cleared at `bubblegame.cpp:1013`, at `bubblegame.cpp:1363` (**`QuitToTitle`**), and by the `bubblegame.h:479` initializer. `DoSnipIn` animates nothing. Fix Round 1's exclusion of menu return is therefore backwards: clearing the flag is what arms the animation. Reachability additionally requires `gfxLevel() <= 2` and is compiled out on WASM | `transitionmanager.cpp:48-60`, `:62-75`; `shaderstuff.cpp:66-80`; `bubblegame_render.cpp:1172-1175`; `bubblegame.cpp:1012-1013`, `:1363` |
| **SEC-001** | Reachability qualified. The unchecked `setgid`/`setuid` pair lives inside `daemonize()` and runs only when `-u user` is given. No documented launch path passes `-u`, and the Docker command passes `-d`, which skips `daemonize()` entirely | `server/tools.c:279-288`; `docker/Dockerfile:31`; `start-server.sh:68` |
| **SEC-002** | Reachability qualified **and** impact extended. All three `http_get` call sites are `!quiet`-gated (`net.c:494` `download_blacklisted_IPs`, `:1295` `register_server`, `:1332` `unregister_server`), and both documented launch paths pass `-q`, so the defect is not reachable in this project's own deployments — only for an operator running without `-q`. Where it is reachable the impact is worse than stated: a `Content-Length` whose decimal value wraps to exactly `-1` (e.g. `4294967295`) leaves `size == -1` and `bufsize == size + 1 == 0`, and `recv(sock, ptr, bufsize - (ptr - buf) - 1, 0)` at `net.c:1253` then receives `(size_t)-1` as its length against a zero-size allocation — an attacker-length write past the buffer, not only the allocation-failure `exit()` in `malloc_`. Static argument only; **not** reproduced, per the standing restriction | `net.c:494`, `:1243-1253`, `:1295`, `:1332`; `tools.c:46-54`, `:78-85` |
| **SEC-004** | `OPTIONS:` half corrected. The binary-relay half is upheld exactly — `process_msg_prio_` relays `msg` verbatim and never binds `msg[0]` to `players_id[find_player_number(g, fd)]` (`game.c:962-979`). But `setoptions` **does** enforce slot-zero creator authority (`game.c:405`, rejecting others with `wn_not_creator` at `:415`), so a non-creator cannot make the server emit an `OPTIONS:` push. The client's missing creator check is a defence-in-depth gap, not an independently exploitable leader-impersonation path through `SETOPTIONS` | `server/game.c:401-421`, `:962-979`; `src/mainmenu_netpanel.cpp:110-118` |
| **SEC-005** | Upheld and **strengthened**; reachability established for the first time. The UDP listener exists only under `-l`/`-L` (`net.c:896-900` → `create_udp_server`), which the earlier gate never stated — and **both** documented launch paths pass `-l` (`start-server.sh:68`, `docker/Dockerfile:31`), so the defect is live in every documented deployment of this project | `net.c:433-453`, `:751-769`, `:896-900`; `start-server.sh:68`; `docker/Dockerfile:31` |
| **SEC-007** | Threat model corrected. The defect is unchanged and confirmed — `rcvTeamCount` is clamped 2-5 (`networkclient.cpp:1189-1190`) while `rcvPlayerTeams[i]` is not (`:1191-1195`), the value flows unclamped through `netPlayerTeams` (`mainmenu_netpanel.cpp:126`) into `ns.playerTeams` (`mainmenu.cpp:565`), and gameplay indexes `kTeamColors[5]` with `team - 1` (`bubblegame_render.cpp:87`, `:451`, `:482`; `mainmenu_netpanel.cpp:723`). But the sender must be the **room creator**, not "any room member" as notebook 04 stated | as cited; plus `server/game.c:405` |
| **REL-002** | Strengthened from static to **observed**, with a named consequence. A pre-existing orphan from `tools/server_tests/test_room_caps.py`, 1 d 21 h old, holds `UDP *:1511` and `TCP *:15113`; while it lives, `create_udp_server`'s failed bind makes every later `fb-server -l` on the host `exit(EXIT_FAILURE)` | `tools/server_tests/test_room_caps.py:27`, `:113-114`, `:126`, `:130`; `server/net.c:751-769`; passive `pgrep`/`lsof`/`ps` observation |
| **REL-009** | Extended with a third documented-vs-shipped drift in the same file. `CLAUDE.md:54` states `BubbleArray bubbleArrays[5]` and "one per player (1–5 players)", where `src/bubblegame.h:549` declares `bubbleArrays[MAX_NET_PLAYERS]` (20, `:251`) and `src/mainmenu_internal.h:35` offers room sizes `{5, 10, 20}`. Same file, same class of cost as the CI clause: a reader trusting it would either mis-report an out-of-bounds index or miss a real one | `CLAUDE.md:54`, `:110`; `src/bubblegame.h:251`, `:549`; `src/mainmenu_internal.h:35` |
| **IMP-013** | Site enumeration completed with a fourth clamp of the identical family: `shaderstuff.cpp:1158`'s `CLAMP((int)points[i].x, 0, mask->w)` / `CLAMP(..., 0, mask->h)`, feeding a read of `mask->pixels` | `src/shaderstuff.cpp:1155-1158` |
| **IMP-021** | Scope corrected so its BUG-041 assertion can fail: the transition driver must clear `firstRenderDone` and call `TakeSnipOut`; driving `DoSnipIn` animates nothing | `transitionmanager.cpp:48-60`; `bubblegame_render.cpp:1172-1175` |

## Dismissed candidates

Six candidates this gate raised reached a dismissal; all are in the `Candidates`
table above with the counter-evidence that settled them. The durable record of
findings challenged and **not** sustained, so the final report cannot resurrect
them:

- **No confirmed defect was dismissed.** All 72 survive as defects; nine have a
  corrected element.
- **No explicit dismissal from notebooks 01-08 was overturned.** BUG-012 stays
  dismissed and its ID stays retired.
- The proposition that a non-creator can push room `OPTIONS:` — asserted by
  notebook 04 and implied by SEC-004's summary — is **dismissed** on
  `server/game.c:405`. Any future report claiming "any room member can emit
  `SETOPTIONS`" is contradicted by the pinned source.
- The proposition that `DoSnipIn` produces the transition animation, and that
  menu return is not a BUG-041 trigger, is **dismissed** on
  `transitionmanager.cpp:48-60` and `bubblegame.cpp:1363`.

## Coverage

- **Defects challenged:** 72 of 72 (100%). 62 upheld with citations re-derived
  from the pinned source, 9 revised, 0 dismissed. One — REL-010 — retains a
  premise this gate could not re-derive: certbot's ECDSA-by-default behaviour
  was taken from documentation, not from an installed certbot, and its
  destructive `openssl req -x509` branch remains unexecuted. That was already a
  recorded Task 9 limitation and is unchanged.
- **Improvements challenged:** 24 of 24 (100%). 0 rejected, 2 revised
  (IMP-013, IMP-021).
- **Dismissals challenged:** all 43 explicit bullets read, 4 sampled in depth
  (BUG-012, `idleSPButtons` `SDL_GetTextureSize`, `cmake_uninstall.cmake.in`
  `exec_program`, `netlify.toml` publish-directory — see Step 5b above for the
  one-line record of what was attempted against each and why the dismissal
  held), 0 overturned; plus a bounded sweep of conceded-and-set-aside inline
  observations across notebooks 01-08 — 6 found, 5 sound, 1 promoted to
  BUG-052.
- **Cross-subsystem categories:** all 8 named in brief Step 4 executed; 1
  produced a new defect, 7 produced nothing beyond the existing registry.
- **Coverage claim:** inventory reconciled at 237/237 with an empty `diff`;
  disposition census summing to 237 across 42 strings with every class sampled
  (Fix Round 1: corrected from a miscounted 41 — the census predated this
  gate's own `src/networkclient.cpp` edit; see
  [Task 12 Fix Round 1 Findings](../SDL3_REVIEW_STATUS.md#task-12-fix-round-1-findings));
  0 pending rows; 0 broken links and 0 broken anchors repository-wide across the
  audit documents; notebooks 01-08 structurally conformant; registry with 0
  duplicate IDs and 0 gaps.
- **Registry after this gate:** **98** unique IDs — BUG-001..052, SEC-001..007,
  REL-001..015, IMP-001..024. **Not yet allocated — do not count these as
  findings:** the next free ID in each series is BUG-053, SEC-008, REL-016,
  IMP-025.

## Limitations

- **No security-specific runtime testing**, per the standing user restriction.
  SEC-001 through SEC-007 were challenged **statically only**: no exploit
  attempt, hostile-traffic test, fuzz run, or offensive probe was made.
  SEC-002's extended heap-overflow consequence in particular is a
  code-supported argument, not an observed fact, and settling it would require
  exactly the class of testing that is out of scope. Recorded as an open
  limitation, not resolved.
- **BUG-052 is unreproduced.** Its causal chain is complete and every element
  is cited, but no run drove a >4095-byte server line into a real client.
  Reproduction needs either ~165 concurrent lobby connections or a deliberately
  over-long line; the latter is barred by the restriction above. This is the
  one new finding in the audit with no runtime evidence of its own.
- **The "conceded-and-set-aside inline observation" class was sampled, not
  swept exhaustively.** A bounded language sweep over notebooks 01-08 found 6
  passages of that shape and one of them concealed a High-severity defect. A
  language sweep cannot be complete over ~5,000 lines of prose, so this gate
  cannot assert that no further conceded observation hides a defect. It is
  named here as the residual risk with the highest demonstrated yield in this
  audit.
- **Two gameplay-algorithm findings rest on the earlier gate's reproduction,
  not a fresh one.** BUG-019 and BUG-025 were re-derived only to the level of
  their cited mechanism; their numeric reproductions (the simultaneous-loss
  ordering and the 75 px tunnelling geometry) were accepted from Task 5's
  production-object harness logs rather than re-run.
- **No process, build, or sanitizer run.** This gate compiled nothing and
  launched nothing, so it adds no dynamic or leak evidence of its own. Apple
  ASan's inability to detect leaks is unchanged, so no leak conclusion anywhere
  in this audit rests on a sanitizer pass — BUG-001, BUG-008, BUG-013, BUG-041
  and BUG-042 rest on ownership tables, grep-verified destroy-site absence, and
  RSS measurement.
- **Six in-scope maintained paths carry no coverage row** because Task 1 Step
  4's selection pattern does not match them: `CLAUDE.md`, `CHANGELOG.md`,
  `.gitignore`, `.gitmodules`, `COPYING`, and `third_party/iniparser/*` (4
  files). Five are cited as evidence for REL-006, REL-009 or REL-014. The
  status file already recorded the `CLAUDE.md`/`CHANGELOG.md` half; the rest is
  recorded here. Asymmetry noted: the Android duplicate
  `android/app/jni/iniparser/*` has four rows while the primary copy has none.
- **`FILE_COVERAGE.md` uses three of the design's five disposition classes.**
  Nothing is dispositioned *Excluded* or *Blocked*; blocked checks live in the
  status file's Limitations. A reader of the ledger alone cannot see which rows
  depend on an unavailable platform.
- **A pre-existing foreign `fb-server` orphan holds UDP 1511 and TCP 15113 on
  this host** and was deliberately left running, since it is not this gate's
  process. It is REL-002's own predicted orphan and should be cleared by the
  operator before any LAN-hosting test.
- **`SDL_GAMEPAD_BUTTON_COUNT` was read from the host's Homebrew SDL 3.4.10
  header**, not from the pinned 3.4.4 submodule, when confirming BUG-036's
  20-vs-26 stride. The value is 26 in both families, but the measurement is
  host-derived.

## Gate conclusion

Task 12 is **complete**. The challenge examined all 72 confirmed defects, all
24 improvements, all 43 explicit dismissals, all 8 cross-subsystem categories,
and the complete-coverage claim, and it re-derived every quantity it relied on
with a command that measures the claim rather than trusting a recorded figure.

Results: **0** confirmed defects dismissed, **9** revised, **62** upheld;
**1** new defect registered — **BUG-052** (High) — promoted from an inline
observation that notebook 02 conceded and set aside without ever opening it as a
candidate; **2** improvements revised; **1** cross-notebook contradiction
resolved against the pinned source (notebook 04's "any room member can emit
`SETOPTIONS`" versus notebook 01's correct "slot-zero creator only"); **1**
Fix-Round correction reversed as itself incorrect (BUG-041's trigger set);
**1** finding strengthened from static to observed (REL-002); and **4**
previously unstated reachability qualifications recorded (BUG-002, SEC-001,
SEC-002, SEC-005).

Every issue this gate raised has been applied to the registry, the affected
notebooks, and the coverage ledger. No challenge issue remains open. The
residual risks are recorded above as limitations rather than resolutions: no
security runtime testing was performed, BUG-052 is unreproduced, and the
conceded-inline-observation class was sampled rather than swept.

**Final synthesis (Task 13) is authorized.** The final report must carry
BUG-052 at High, the nine revised findings in their corrected form, the four new
reachability qualifications, and the limitations above verbatim — and it must
not restate the two propositions this gate dismissed.
