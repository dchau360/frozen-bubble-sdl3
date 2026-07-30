# Frozen Bubble SDL3 — Complete Repository Review

**Audited commit:** `09d6c7bfcd864a0ad3951b87d16a88dc770392a3` (tag `v2.4.27`)
**Audit branch:** `codex/sdl3-complete-audit` — production source unchanged throughout
**Report date:** 2026-07-30
**Final synthesis agent/model:** Claude Opus, model id `claude-opus-5` (Task 13)
**Independent final challenge:** Claude Opus, model id `claude-opus-5`, fresh context (Task 12)
**Host:** macOS 26.5.2, Darwin 25.5.0, arm64; Apple clang 21.0.0; CMake 4.3.4; Ninja 1.13.2

Evidence for every statement below lives in
[`FINDINGS.md`](FINDINGS.md) (the finding registry),
[`FILE_COVERAGE.md`](FILE_COVERAGE.md) (the 237-row file inventory),
[`SDL3_REVIEW_STATUS.md`](SDL3_REVIEW_STATUS.md) (the command-and-evidence ledger),
and the nine subsystem notebooks under [`subsystems/`](subsystems/).

---

## 1. Executive summary and release-readiness assessment

### 1.1 What was audited

Thirteen gated tasks covered the C game server and its TCP/WebSocket protocol,
the native and WASM network clients, gameplay rules and round state, the lobby /
settings / input layer, rendering / transitions / fonts / audio, the native, WASM
and Android platform ports, and the build, packaging, CI, deployment and
operational-documentation surface. Phase 5 was an independent final challenge run
in a fresh context that re-derived, rather than re-read, every quantity the audit
relied on.

**Coverage measured, not asserted.** The pinned commit's in-scope tracked-file
selection is **237** paths. `FILE_COVERAGE.md` carries **237** rows, in exact set
equality with a fresh regeneration of the original selection command (`diff`
produced no output, exit 0), across **42** distinct disposition strings summing to
**237**, with **0** rows in a pending state.

### 1.2 Headline numbers

Every figure in this section was re-derived in Task 13 by a command that measures
the claim; the commands and their exit codes are in §6.

| Quantity | Value |
|---|---|
| Registry rows (findings) | **98** |
| — of which confirmed | **97** |
| — of which dismissed | **1** (BUG-012, ID retired, never recycled) |
| Confirmed **defects** (BUG + SEC + REL) | **73** |
| Confirmed **improvements** (IMP) | **24** |
| In-scope files with a final disposition | **237 / 237** |

**Confirmed defects by severity** (design scale in §2.1):

| Severity | Count | IDs |
|---|---|---|
| **Critical** | **0** | — |
| **High** | **15** | BUG-003, BUG-007, BUG-020, BUG-026, BUG-032, BUG-034, BUG-049, BUG-052, REL-003, SEC-001, SEC-002, SEC-003, SEC-004, SEC-005, SEC-007 |
| **Medium** | **45** | BUG-001, 002, 004, 005, 006, 011, 013, 014, 015, 017, 018, 019, 021, 022, 023, 024, 025, 027, 028, 030, 033, 035, 036, 037, 040, 041, 042, 043, 044, 045, 046, 048; REL-002, 004, 005, 006, 007, 008, 010, 011, 012, 013, 014, 015; SEC-006 |
| **Low** | **13** | BUG-008, 009, 010, 016, 029, 031, 038, 039, 047, 050, 051; REL-001, 009 |
| **Total** | **73** | |

Confidence is **High** on all 98 registry rows; no finding rests on a low- or
medium-confidence inference.

### 1.3 Release-readiness assessment

**This tree is not ready for a release that includes the multiplayer server.**
That judgement rests on three findings, in this order:

1. **BUG-049 (High)** — a heap use-after-free in the server's recursive room
   teardown, at `server/game.c:1051`. It is triggered by *ordinary* simultaneous
   client disconnects — no malformed, fragmented or flooded traffic is required —
   and it reproduced deterministically in **11 of 24** recorded server runs at
   3, 4, 5, 6, 10 and 20 seats. Under ASan the whole server aborts, taking every
   other room with it. **On the uninstrumented Release build that actually ships,
   it does not abort**: the freed bytes silently decide a branch, and one branch
   passes an also-freed nickname to `stats_record_win`, which writes it to disk.
   A public server running this build is expected to misbehave silently under
   normal churn.
2. **SEC-005 (High)** — an unbounded `strstr` over a 128-byte stack buffer that a
   single LAN UDP probe can fill with non-zero bytes (`server/net.c:433-453`).
   The UDP listener exists only under `-l`/`-L`, and **both** documented launch
   paths pass `-l` (`start-server.sh:68`, `docker/Dockerfile:31`), so the
   over-read path is live in every documented deployment of this project.
3. **SEC-004 (High)** — the server relays binary frames verbatim without binding
   the sender's claimed identity to the connection's assigned room seat
   (`server/game.c:962-979`), permitting seat impersonation inside a room.

**The single-player and local-multiplayer client is in materially better shape**,
but three High client defects would each be user-visible on a bad day: BUG-026
(an unwritable preferences file hangs startup in an unbounded retry loop, before
any window appears), BUG-032 (a corrupt highscore or level-history file aborts
the client during construction, because `stoi`/`stof` are called with no
exception handling), and BUG-034 (`FrozenBubble`'s raw members have no
initializers, so the constructor's early-return paths leave them indeterminate
and `RunForEver`/`~FrozenBubble` then dereference them).

**The release pipeline has defects that reach shipped artifacts.** Two tagged
releases produce Android APKs that cannot upgrade one another, for two
independent reasons at once — a literal `versionCode 10` that no workflow step
overrides (REL-004) and a throwaway keystore regenerated per CI run (REL-007).
The macOS DMG is single-architecture and nothing in its name, its itch.io channel
or the README says so (REL-012). Five distinct version strings exist across build
inputs, four of which reach a runtime or artifact surface (REL-004).

**What this report does not say.** It does not claim the codebase is free of
defects. It reports what was examined, what was found, and — explicitly, in §8 —
what could not be checked. Several whole classes of verification were unavailable
or out of scope, and a check that was not run is recorded as a limitation, never
as a pass.

### 1.4 Highest risks

| Risk | Finding | Why it leads |
|---|---|---|
| Silent server memory corruption under ordinary use | BUG-049 | Reproduced 11×; ordinary trigger; **silent** on the shipping Release build |
| Remote stack over-read in every documented deployment | SEC-005 | One LAN packet; both launch paths pass `-l`; static-only, never probed |
| Peer identity is not bound to a room seat | SEC-004 | Verbatim binary relay; enables impersonation of another seat |
| Untrusted peer values reach unchecked indexing | SEC-003, SEC-007 | Board coordinates and `PLAYERTEAM_Pn` reach array indexing unclamped |
| Client goes permanently deaf on a busy public server | BUG-052 | Complete causal chain, **unreproduced**; needs ~165 concurrent lobby clients |
| Startup hang / abort from ordinary bad local files | BUG-026, BUG-032, BUG-034 | Reproduced against unchanged production objects |
| Shipped Android releases cannot upgrade one another | REL-004, REL-007 | Two independent causes; reaches every tagged release |

### 1.5 Material limitations, stated up front

- **No security-specific runtime testing was performed anywhere in this audit**,
  by the user's explicit direction. SEC-001 through SEC-007 are code-supported
  inferences, not observed runtime facts. No exploit attempt, hostile-traffic
  test, fuzz run or offensive probe was made at any gate.
- **Apple ASan cannot detect leaks** on this host (`detect_leaks=1` is rejected;
  every sanitizer run used `detect_leaks=0`). **No leak conclusion in this audit
  rests on a sanitizer pass.** Every leak finding — BUG-001, BUG-008, BUG-013,
  BUG-041, BUG-042 — rests on ownership tables, grep-verified absence of destroy
  sites, and RSS measurement.
- **BUG-052 is unreproduced.** Its causal chain is complete and every element is
  cited at the pinned commit, but no run drove a >4095-byte server line into a
  real client.
- **No browser runtime, no Android device or emulator, no Linux host, no Windows
  host, and no packaged-artifact launch** were exercised. Conclusions for those
  platforms are source traces and artifact analysis.
- **No GitHub Actions workflow was executed**, no container was started, and no
  external network operation was performed.

Full limitation set in §8.

---

## 2. Confirmed defects

### 2.1 Severity scale

- **Critical:** remotely exploitable, major security-boundary failure, or
  widespread unrecoverable corruption.
- **High:** crash, memory corruption, serious multiplayer desynchronization, or a
  shipped platform rendered unusable.
- **Medium:** incorrect gameplay, broken edge case, practically significant
  resource leak, or meaningful portability/release defect.
- **Low:** limited incorrect behavior, weak diagnostics, or minor robustness
  issue.

Severity describes *consequence if reached*. It deliberately does not encode
*likelihood of being reached* — §9 supplies that as a separate ordering key,
because two findings at the same severity can differ by orders of magnitude in
practical reachability.

### 2.2 Evidence classes

Every finding below is tagged with how it is known:

- **[R] Reproduced** — observed at runtime against unchanged production code or a
  production object.
- **[C] Code-supported causal chain** — every element cited at the pinned commit,
  no runtime observation of the composed failure.
- **[A] Artifact-measured** — established from a built artifact or a parsed
  configuration file.

### 2.3 High-severity defects

---

#### BUG-049 — Server heap use-after-free in recursive room teardown · High · Confidence High · **[R]**

- **Platform:** all server platforms (Linux, macOS). The Windows server is not
  built by this project (`CMakeLists.txt:38` excludes `server/` on
  `WIN32 OR MINGW OR EMSCRIPTEN`).
- **Location:** `server/game.c:1051`; recursion entered at `game.c:1044`,
  `game.c:977`, `game.c:982`, `server/net.c:226`; free at `game.c:1033-1034`.
- **Causal path:** `player_part_game_` calls
  `process_msg_prio_(fd, "<id>l\n", 3, g)` at `game.c:1044` to announce a
  departure. If any relay `send()` fails — the ordinary case when two or more
  clients disconnect at about the same time, since the second and later sends to
  an already-closed peer return `EPIPE` — the destination is appended to
  `conn_to_terminate` (`game.c:977`) and `g_list_foreach` (`game.c:982`) calls
  `conn_terminated` (`net.c:226`), which re-enters `player_part_game_` **on the
  same game**. The nested call can run
  `games = g_list_remove(games, g); free(g);` (`game.c:1033-1034`). The outer
  frame then resumes at `game.c:1051` with `if (g->players_number == 1)`, reading
  freed memory. `net.c:226`'s own comment names the recursion; nothing
  re-validates `g` after `:1044`.
- **Reproduction:** deterministic in **11 of 24** recorded server runs, all with
  the identical ASan summary `heap-use-after-free game.c:1051 in
  player_part_game_`, at 3, 4, 5, 6, 10 and 20 seats, in normal / team / clear
  rooms. Minimum reproducing size **3** seats (2 passes). Trigger is ordinary
  socket closure.
- **User impact:** under ASan the entire server aborts, taking every other room
  with it. **Uninstrumented — the configuration that ships — it does not abort**,
  so the freed 4 bytes silently decide a branch; if they read as `1`, the next
  statements pass the also-freed `g->players_nick[0]` to `stats_record_win`,
  which hashes it, `g_strdup`s it and writes it to disk
  (`server/stats.c:157-177`) — a second use-after-free reaching file I/O.
- **Entanglement:** BUG-005's second spurious win is emitted from
  `game.c:1051-1054`, the exact statements this defect makes read freed memory,
  so **BUG-005 cannot be measured until this is fixed**.
- **Proposed correction:** do not free the game object inside a nested teardown
  while an outer frame holds it. Either defer the free to a point where no frame
  holds `g` (mark-and-sweep after the `g_list_foreach` completes), or re-validate
  `g` against the live `games` list after `:1044` and bail out if it is gone.
- **Verification:** the IMP-019 server parser/lifecycle test with a room reaching
  one player and then zero, plus a 3-seat simultaneous-disconnect case, run under
  ASan on Linux where LeakSanitizer also works.
- **Evidence:** [Task 10 confirmed findings](subsystems/08-dynamic-integration.md#confirmed-findings), [BUG-049 reproduction record](subsystems/08-dynamic-integration.md#bug-049-reproduction-record)

---

#### SEC-005 — LAN probe reaches an unbounded `strstr` past a stack buffer · High · Confidence High · **[C]**

- **Platform:** all server platforms, under `-l`/`-L`.
- **Location:** `server/net.c:433-446` (fill), `:453` (`strstr`), `:751-769`
  (`create_udp_server`), `:896-900` (listener creation);
  `start-server.sh:68`; `docker/Dockerfile:31`.
- **Causal path:** a 128-byte LAN probe overwrites every zero in `msg[128]`, after
  which `strstr(msg, ok_input_end)` can read beyond the stack buffer. The
  preceding `strncmp` is bounded; the `strstr` is not.
- **Reachability (established in the final challenge, and it *strengthens* the
  finding):** the UDP listener exists only under `-l`/`-L`, and **both**
  documented launch paths pass `-l` — `start-server.sh:68` and
  `docker/Dockerfile:31` — so the defect is live in every documented deployment
  of this project.
- **User impact:** out-of-bounds read on the server stack, reachable by any host
  that can send one UDP datagram to the discovery port.
- **Status of evidence:** **static only. No probe was sent**, per the standing
  security-runtime restriction. This is a code-supported inference, not an
  observed fact, and no claim is made about exploitability beyond the over-read.
- **Proposed correction:** guarantee NUL-termination of `msg` before any
  string-search call, or replace the `strstr` with a length-bounded search over
  the number of bytes actually received.
- **Verification:** IMP-019's `tests/server_parse_test.c` case "a 128-byte LAN
  probe with no NUL", asserted to reject without indexing outside its buffer.
- **Evidence:** [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace), [Task 12 reachability qualifications](subsystems/01-server-protocol.md#task-12-reachability-qualifications)

---

#### SEC-004 — Sender identity is not bound to the connection's room seat · High · Confidence High · **[C]**

- **Platform:** all server platforms; consumed by all clients.
- **Location:** `server/game.c:962-979`.
- **Causal path:** `process_msg_prio_` relays `msg` verbatim and never binds
  `msg[0]` to `players_id[find_player_number(g, fd)]`, so a room member can emit
  frames claiming another seat's identity, and hostile downstream fields ride
  along in relayed frames.
- **Scope correction carried forward:** the `OPTIONS:` half of the original
  finding is **superseded and is not restated here as true**. `setoptions` **does**
  enforce slot-zero creator authority (`server/game.c:405`, rejecting others with
  `wn_not_creator` at `:415`), so a non-creator cannot make the server emit an
  `OPTIONS:` push. The client-side missing creator check in
  `GetAndClearPendingOptions` is real but is defence-in-depth, not an
  independently exploitable leader-impersonation path through `SETOPTIONS`.
- **User impact:** seat impersonation within a room; a peer can attribute actions
  to another player.
- **Status of evidence:** static only; no forged frame was sent.
- **Proposed correction:** overwrite the sender byte server-side with the
  authoritative `players_id[find_player_number(g, fd)]` before relaying, rather
  than trusting the client-supplied byte.
- **Verification:** IMP-019 case "a frame whose claimed sender is not the
  connection's seat", asserting the relayed frame carries the server's seat id.
- **Evidence:** [Authorization review](subsystems/01-server-protocol.md#authorization-and-room-lifecycle), [Task 12 revisions](subsystems/09-final-challenge.md#revisions-applied-to-existing-findings)

---

#### SEC-002 — Untrusted `Content-Length` reaches signed overflow and allocation sizing · High · Confidence High · **[C]**

- **Platform:** all server platforms, only when running **without** `-q`.
- **Location:** `server/net.c:494`, `:1243-1253`, `:1295`, `:1332`;
  `server/tools.c:46-54`, `:78-85`.
- **Reachability:** all three `http_get` call sites are `!quiet`-gated, and both
  documented launch paths pass `-q`, so the defect is **not** reachable in this
  project's own deployments — only for an operator running without `-q`.
- **Extended impact (recorded in the final challenge):** a `Content-Length` whose
  decimal value wraps to exactly `-1` (e.g. `4294967295`) leaves `size == -1` and
  `bufsize == size + 1 == 0`, so
  `recv(sock, ptr, bufsize - (ptr - buf) - 1, 0)` at `net.c:1253` receives
  `(size_t)-1` as its length against a zero-size allocation — an attacker-length
  write past the buffer, not only the allocation-failure `exit()` in `malloc_`.
- **Status of evidence:** **the extended heap-overflow consequence is a static
  argument only and is unresolved, not resolved.** Settling it would require
  exactly the class of testing that is out of scope. No exploit attempt was made.
- **Proposed correction:** parse `Content-Length` into an unsigned type with an
  explicit range check against a sane ceiling before it reaches any allocation or
  `recv` length computation; reject on overflow rather than continuing.
- **Verification:** IMP-019 case "`Content-Length` at `INT_MAX`", extended with a
  value that wraps to `-1`, asserting rejection with no allocation and no `recv`.
- **Evidence:** [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace), [Task 12 revisions](subsystems/09-final-challenge.md#revisions-applied-to-existing-findings)

---

#### SEC-003 — Peer-controlled coordinates and numeric fields reach unchecked indexing · High · Confidence High · **[C]**

- **Platform:** native and WASM clients.
- **Location:** peer-message handling in `src/networkclient.cpp` /
  `src/bubblegame_net.cpp`; board indexing in the gameplay layer.
- **Causal path:** peer-supplied bubble coordinates and numeric fields reach
  unchecked board indexing and exception-throwing option parsing.
- **User impact:** out-of-bounds board access or an uncaught exception driven by a
  hostile or malfunctioning peer.
- **Status of evidence:** static only; no hostile traffic was sent at any gate.
- **Proposed correction:** clamp or reject every peer-derived index against the
  board's real bounds at the parse boundary, before it reaches gameplay state.
- **Verification:** IMP-019 case "an off-board coordinate", asserting rejection
  without indexing outside the board.
- **Evidence:** [Peer-message proof](subsystems/02-network-client-sync.md#peer-messages-and-round-flow)

---

#### SEC-007 — Peer `PLAYERTEAM_Pn` is never clamped before indexing `kTeamColors` · High · Confidence High · **[C]**

- **Platform:** native and WASM clients.
- **Location:** `src/networkclient.cpp:1189-1190` (team count *is* clamped 2-5),
  `:1191-1195` (`rcvPlayerTeams[i]` is **not**);
  `src/mainmenu_netpanel.cpp:126`; `src/mainmenu.cpp:565`;
  `src/bubblegame.h:280` (`kTeamColors[5]`); indexed `[team - 1]` at
  `src/bubblegame_render.cpp:87`, `:451`, `:482` and
  `src/mainmenu_netpanel.cpp:723`.
- **Threat-model correction carried forward:** the sender must be the **room
  creator**, because `server/game.c:405` gates `setoptions` on slot zero. The
  earlier "any room member" characterization is **superseded and is not restated
  here as true**. This narrows the threat model without eliminating the defect —
  a hostile or buggy room creator still reaches it.
- **User impact:** out-of-bounds read of a 5-element colour array driven by an
  unclamped peer value.
- **Status of evidence:** static only; the forged-`OPTIONS` and
  out-of-range-team checks were omitted by user direction and are a limitation,
  not a pass.
- **Proposed correction:** clamp `rcvPlayerTeams[i]` to 1-5 at
  `networkclient.cpp:1191-1195`, alongside the clamp the adjacent team count
  already receives.
- **Verification:** IMP-019 case "a `PLAYERTEAM_Pn` outside 1-5".
- **Evidence:** [Option table](subsystems/04-lobby-settings-input.md#option-serialization-and-validation-step-2), [Task 12 revisions](subsystems/09-final-challenge.md#revisions-applied-to-existing-findings)

---

#### SEC-001 — `setgid`/`setuid` failures are ignored in the daemon · High · Confidence High · **[C]**

- **Platform:** server, only when `-u user` is given.
- **Location:** `server/tools.c:279-288`, inside `daemonize()`.
- **Reachability:** no documented launch path passes `-u`, and
  `docker/Dockerfile:31`'s `-d` skips `daemonize()` entirely.
- **User impact:** the daemon can continue running with unintended privileges
  after a failed privilege drop.
- **Status of evidence:** static only; fault injection was omitted by user
  direction.
- **Proposed correction:** check both return values and abort startup on failure.
- **Verification:** IMP-019 case "a `setgid` stub returning failure", asserting
  the process does not continue.
- **Evidence:** [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions), [Task 12 reachability qualifications](subsystems/01-server-protocol.md#task-12-reachability-qualifications)

---

#### BUG-052 — The client's receive buffer has an absorbing full state · High · Confidence High · **[C, unreproduced]**

- **Platform:** native client (`src/networkclient.cpp`).
- **Location:** guard `if (recvBufferLen + received < BUFFER_SIZE)` at
  `src/networkclient.cpp:843` with **no `else`**; the only statements that reduce
  `recvBufferLen` are `:894` (in-game) and `:914`/`:916` (lobby), all inside that
  guard's body. `BUFFER_SIZE` is **4096** (`src/networkclient.h:36`).
- **Causal path:** once the buffer fills, the guard fails for every later `recv`
  with `received >= 1`, and because every drain statement lives inside the guard
  body, nothing can ever reduce `recvBufferLen` again. `Update`'s read loop
  (`:810-818`) then spins its 100-iteration cap each frame discarding everything,
  reporting only "network buffer was filling up".
- **The wedge is permanent for the process lifetime.** The constructor sets
  `recvBufferLen` to 0 (`:38`); **`Disconnect` never resets it** (verified: the
  `Disconnect` body contains no `recvBufferLen` reference), and the
  `NetworkClient` singleton is never destroyed — **`Dispose()` has zero callers**
  (verified: one occurrence in `src`/`server` at the pinned commit, the
  definition itself at `networkclient.cpp:60`). Reconnecting does not clear it;
  only restarting the process does.
- **No malformed input is required.** The server emits single lines up to
  **16383** bytes: `send_line` formats into `static char buf[16384]` and sends
  `sizeof(buf)-1` (`server/net.c:126-146`), and the `LIST` reply is the
  `static char [16384]` `list_games_str` (`server/game.c:134`).
  `list_open_nicks_aux` (`game.c:139-151`) appends `NICK[:GEOLOC],` per lobby
  connection — nick ≤ 10 (`game.c:615`), geoloc ≤ 13 (`:746-747`), i.e. up to
  **25** bytes each — against a `max_users` default of **255**
  (`server/net.c:82`). Roughly **165** concurrent lobby clients suffice; even with
  no geolocations, 255 nicks (2,805 B) plus 16 open rooms of 20 seats (the
  `games_open == 16` cap, `game.c:773`) exceeds 4,095.
- **User impact:** the lobby stops updating and every server push is dropped —
  `GAME_CAN_START`, `OPTIONS:`, `TALK`, `KICKED`; in `IN_GAME` state every relayed
  peer frame is dropped, so boards diverge with no error shown.
- **Not memory corruption:** the guard is a strict `<` and `recvBuffer[recvBufferLen]`
  (`:898`) stays inside the array.
- **Status of evidence: NOT REPRODUCED.** Reproduction needs either ~165
  concurrent lobby connections or a deliberately over-long server line, and the
  latter is barred by the standing security-runtime restriction. This is the one
  finding registered in this audit with **no runtime evidence of its own**. Its
  practical likelihood therefore depends on a lobby population this project has
  probably never had — see §9.
- **Proposed correction:** grow `recvBuffer` to at least the server's 16,384-byte
  line ceiling **and** give the guard an `else` that reports and recovers — drain
  complete lines out of the full buffer, or treat an unterminated buffer-filling
  line as a protocol error and disconnect the way the server itself does
  (`net.c:331-334`) — and reset `recvBufferLen` in `Disconnect`.
- **Verification:** feed `ProcessIncomingData` a 5,000-byte line and assert the
  next well-formed line is still parsed (belongs in IMP-019's
  `tests/netclient_parse_test.cpp`).
- **Origin, which is itself a finding about the audit:** this came from an
  observation notebook 02 conceded at `:153` ("a chunk that would fill the buffer
  is silently skipped") and set aside as "Ordinary server messages fit the buffer
  … Overflow/flood behavior remains part of the untrusted-input limitation",
  without ever opening it as a candidate. Both halves of that disposition were
  falsified. See §8.4.
- **Evidence:** [BUG-052 in the final challenge](subsystems/09-final-challenge.md#bug-052-new-high--the-clients-receive-buffer-has-an-absorbing-full-state)

---

#### BUG-003 — Kick and post-start room closure leave live fds in priority mode with no game · High · Confidence High · **[C]**

- **Location:** `server/game.c`; `remove_prio` has exactly one caller, `game.c:838`.
- **Impact:** the affected fd's next binary line reaches a fatal
  `exit(EXIT_FAILURE)`, terminating the server for every connected user.
- **Correction:** remove priority membership on every path that removes a player
  from a game, not only `PART`.
- **Verification:** IMP-019 cases "a room kick and a post-start close".
- **Evidence:** [Lifecycle proof](subsystems/01-server-protocol.md#authorization-and-room-lifecycle)

---

#### BUG-007 — Blocking single-send output with no queue or deadline; short WebSocket frames reported complete · High · Confidence High · **[C]**

- **Location:** `server/net.c` output path; WebSocket relay.
- **Impact:** a slow or stalled peer blocks the server; a short frame is treated
  as a complete payload, corrupting the relayed stream.
- **Correction:** add an output queue with a deadline, and loop short sends to
  completion rather than reporting any nonnegative return as success.
- **Verification:** IMP-019 case "a short send return".
- **Evidence:** [Final candidate disposition](subsystems/01-server-protocol.md#candidates)

---

#### BUG-020 — Distinct-match startup retains malus and transient round state · High · Confidence High · **[C]**

- **Location:** gameplay reset path.
- **Impact:** after a player-count shrink, a stale malus can index an inactive
  cleared board.
- **Correction:** clear malus and transient round state on every match start, not
  only on round reload.
- **Verification:** IMP-018 assertion "no malus survives a match transition into
  a cleared board".
- **Evidence:** [Reset-path proof](subsystems/03-gameplay.md#reload-reset-and-construction)

---

#### BUG-026 — `ReadSettings` retries in an unbounded loop, hanging startup · High · Confidence High · **[R]**

- **Location:** `src/gamesettings.cpp` `ReadSettings` /
  `CreateDefaultSettings` / `iniparser_load`.
- **Impact:** an unwritable or unrepairable preferences file hangs startup
  **before any window appears** — the user sees nothing at all.
- **Reproduction:** reproduced at runtime against unchanged production code under
  an isolated preference home (persistence matrix cases I/J/K).
- **Correction:** bound the retry count and fail forward to in-memory defaults
  with a visible diagnostic.
- **Verification:** IMP-017 assertion "`ReadSettings` terminates in bounded time".
- **Evidence:** [Persistence matrix](subsystems/04-lobby-settings-input.md#dynamic-evidence)

---

#### BUG-032 — Highscore and level-history parsing aborts the client on a corrupt file · High · Confidence High · **[R]**

- **Location:** `src/highscoremanager.cpp`, `stoi`/`stof` with no exception
  handling.
- **Impact:** a corrupt file aborts the client during construction.
- **Reproduction:** reproduced as a full-client abort.
- **Correction:** wrap the conversions, or use `std::from_chars`, and treat a
  malformed record as a reset-to-empty rather than a fatal error.
- **Verification:** IMP-017 assertion "a corrupt highscore file leaves the object
  constructed".
- **Evidence:** [Full-client runs](subsystems/04-lobby-settings-input.md#dynamic-evidence)

---

#### BUG-034 — `FrozenBubble`'s raw members are uninitialized on early-return paths · High · Confidence High · **[R]**

- **Location:** `src/frozenbubble.h` / `.cpp`.
- **Impact:** the constructor's early-return paths leave raw members
  indeterminate, and `RunForEver`/`~FrozenBubble` then dereference them.
- **Reproduction:** reproduced as a full-client abort. Task 7 separately verified
  that a *constructed* `AudioMixer` is failure-safe, so the defect is entirely
  the construction path.
- **Correction:** default-initialize every raw member at its declaration.
- **Verification:** IMP-017 (constructed-object assertions) and IMP-020's
  packaged-artifact startup case.
- **Evidence:** [Full-client runs](subsystems/04-lobby-settings-input.md#dynamic-evidence), [Task 7 cross-link](subsystems/05-render-audio.md#confirmed-findings)

---

#### REL-003 — Windows socket typing and nonblocking-mode gaps · High · Confidence High · **[C]**

- **Location:** `src/socket_compat.h`, client receive path;
  `CMakeLists.txt:38`, `:155-157`.
- **Scope, closed in Task 8:** `CMakeLists.txt:38` excludes `server/` on
  `WIN32 OR MINGW OR EMSCRIPTEN`, so this project's build can **never** produce
  the Windows server that `server/win32_compat.h` exists to support. Only the
  Windows **client** ships. The client-side `MSG_DONTWAIT` no-op and the missing
  `FIONBIO` call are therefore the whole shipped Windows socket surface.
- **Impact:** the Windows client never enables nonblocking mode before its
  per-frame receive, so the frame loop can block on the socket.
- **Status of evidence:** static portability proof — **Task 8 executed on no
  Windows host**. This is a limitation, not a pass.
- **Correction:** call `ioctlsocket(fd, FIONBIO, &one)` on Windows and stop
  relying on `MSG_DONTWAIT`, which is a no-op there.
- **Verification:** IMP-020 packaged smoke on Windows plus IMP-019's client half.
- **Evidence:** [Client extension](subsystems/02-network-client-sync.md#lobby-response-and-reachability-handling), [Task 8 extensions](subsystems/06-platform-ports.md#confirmed-findings)

---

### 2.4 Medium-severity defects

All 45 carry confidence **High**. Full text, citations and cross-gate history for
each are in [`FINDINGS.md`](FINDINGS.md); the notebook link in each row is the
primary evidence.

| ID | Ev | Summary | Proposed correction | Verification | Evidence |
|---|---|---|---|---|---|
| BUG-001 | **[R]** | `TextureEx` dereferences failed surface loads before checking; leaks 2 heap `SDL_Rect` per `InitCandy`; reassigns surfaces without destroying the previous ones | Check the load before dereferencing; own the rects; destroy before reassigning | IMP-021 missing-candy-asset case | [Task 7](subsystems/05-render-audio.md#confirmed-findings) |
| BUG-002 | **[C]** | SIGTERM handler calls logging, allocation, DNS/HTTP, socket and exit routines outside the async-signal-safe set. `l0()`/`exit()` are unsafe in every configuration; the DNS/HTTP/allocation members reach only via `unregister_server`, gated `!quiet && !lan_game_mode`, and both launch paths pass `-q` and `-l` | Set a flag in the handler; do the work in the event loop | IMP-019 lifecycle case | [Task 12 revisions](subsystems/09-final-challenge.md#revisions-applied-to-existing-findings) |
| BUG-004 | **[C]** | The documented upload admission limit is inert: `amount_transmitted` is initialized, read and reset but never incremented | Increment it on every transmission | IMP-019 "upload exceeding the admission limit" | [Static review](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| BUG-005 | **[C]** | A sole survivor receives a win when the room reaches one player and another when that player later leaves | Emit at most one win per round | IMP-019 "a room reaching one player then zero" — **blocked by BUG-049** | [Lifecycle proof](subsystems/01-server-protocol.md#authorization-and-room-lifecycle) |
| BUG-006 | **[C]** | WebSocket upgrade assumes one `recv()` holds the full HTTP header; a legal fragmented header is consumed then misclassified as plain TCP | Accumulate until the header terminator is seen | IMP-019 "handshake split across two buffers" | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| BUG-011 | **[C]** | Repeated `START` resets readiness while peers remain priority; renewed acknowledgement removes already-priority fds from the replacement polling list | Make `START` idempotent w.r.t. priority membership | IMP-019 "`START` repeated while peers are priority" | [Candidates](subsystems/01-server-protocol.md#candidates) |
| BUG-013 | **[C]** | Disconnect/part paths leak `currentGame` and retain native partial input or WASM sync/session state that can contaminate reconnection | Free and fully reset per-connection state on disconnect | IMP-019 "a part/rejoin cycle" | [Connection lifecycle](subsystems/02-network-client-sync.md#connection-and-room-lifecycle) |
| BUG-014 | **[C]** | WASM round 2+ waits for 40 entries in `messageQueue` after routing all sync traffic into `syncQueue`, forcing the five-second timeout | Wait on `syncQueue` for sync traffic | IMP-019 "round-2 sync arriving before `SyncNetworkLevel`" | [Round-flow proof](subsystems/02-network-client-sync.md#peer-messages-and-round-flow) |
| BUG-015 | **[R]** | Native and WASM command state machines omit ordinary rejection types and do not reliably correlate success with the pending create/join operation | Correlate replies with the pending request; handle all rejection types | IMP-019 "out-of-order lobby response" | [Response handling](subsystems/02-network-client-sync.md#lobby-response-and-reachability-handling) |
| BUG-017 | **[C]** | Native TCP setup searches each greeting `recv` independently; outbound paths accept short sends as complete | Accumulate across reads; loop short sends | IMP-019 "greeting split across `recv` calls" | [Stream semantics](subsystems/02-network-client-sync.md#native-tcp-stream-semantics) |
| BUG-018 | **[R]** | Multiplayer board clear ignores the Clear Mode flag and ends Classic and Team Mode rounds | Honour the flag on board clear | IMP-018 "Clear Mode honoured on board clear" | [Winner-state proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-019 | **[R]** | Sequential final-loss handling can credit a winner or both players before resolving a simultaneous draw | Resolve all losses for the frame before crediting | IMP-018 "simultaneous final loss resolves as a draw" | [Simultaneous-loss proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-021 | **[C]** | Disconnect handling bypasses configured continuation, one-team-survivor and victories-limit semantics; `CONTINUEGAMEWHENPLAYERSLEAVE` has no `SetupSettings` field | Add the field and honour it | IMP-018 "departures respect configured continuation" | [Departure-state proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-022 | **[C]** | Chain target reservation uses standard parity on flipped grids and omits the reference cross-chain validity pass | Use flipped-grid parity; restore the validity pass | IMP-018 "chain targeting uses flipped-grid parity" | [Chain-reaction proof](subsystems/03-gameplay.md#placement-collision-grouping-and-compression) |
| BUG-023 | **[C]** | The local two-player victories limit is neither propagated nor enforced; the panel that edits it is unreachable (`showing2PPanel` never set true, `SetupNewGame(2)` never called) | Propagate and enforce, or remove the dead panel | IMP-018 "victory limits" | [Local match proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-024 | **[C]** | Remote clear-win accounting depends on whether `F` is queued before deferred replicated-stick resolution | Make accounting order-independent | IMP-018 "clear-win accounting is independent of `F`/stick ordering" | [Clear-order proof](subsystems/03-gameplay.md#round-winner-departure-and-match-transitions) |
| BUG-025 | **[R]** | Maximum native delta can move a launched bubble completely through an occupied bubble between endpoint-only collision samples | Sub-step the collision sample by the bubble radius | IMP-018 "a maximum-delta step cannot pass through an occupied cell" | [Maximum-delta proof](subsystems/03-gameplay.md#maximum-delta-collision-trace) |
| BUG-027 | **[R]** | Any INI syntax error or over-long line makes the loader silently truncate the file and rewrite defaults, discarding every stored preference | Preserve unaffected keys; report the bad line | IMP-017 "a syntax error preserves other stored keys" | [Persistence matrix](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| BUG-028 | **[R]** | Key bindings are cast from unvalidated INI integers to `SDL_Scancode`; a stored 99999 survives into `PlayerKeys`, and `IsKeyPressed` then indexes SDL's 512-entry keyboard state | Validate `< SDL_SCANCODE_COUNT` at load | IMP-017 "every scancode reaching `PlayerKeys` is `< SDL_SCANCODE_COUNT`" | [Persistence matrix](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| BUG-030 | **[R]** | A NaN `SpeedMultiplier` passes both ordered clamps and propagates into `deltaScale` and every per-frame movement | Reject non-finite values explicitly | IMP-017 "`SpeedMultiplier` is finite" | [Persistence matrix](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| BUG-033 | **[C]** | Hosting a LAN server runs `system("pkill -x fb-server")`, terminating every `fb-server` the user owns. macOS/Linux only; the return value is discarded; `pkill` is not POSIX-mandated; the filter is by name with no ownership or parentage test | Track the child PID and signal only it | IMP-017 seam over the process-control call | [Local server control](subsystems/04-lobby-settings-input.md#local-server-control) |
| BUG-035 | **[C]** | Controller slots are never released and the slot index is unbounded: from slot 6 controller input silently stops, from slot 11 the derived scancode passes SDL's 512-entry array | Release slots on removal; bound the index | IMP-017 hot-plug cycles | [Input bounds](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| BUG-036 | **[C]** | The 20-scancode stride per player is smaller than SDL3's 26 gamepad buttons, so a button index ≥ 20 aliases the next player's slot | Widen the stride to `SDL_GAMEPAD_BUTTON_COUNT` | IMP-017 "the 20-vs-26 stride" | [Input bounds](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| BUG-037 | **[C]** | Room-scoped lobby state, including the nickname-keyed team-override map and chat scan counters, is never reset on part/join | Reset room-scoped state on every room transition | IMP-019 "a part/rejoin cycle" | [Transition map](subsystems/04-lobby-settings-input.md#menu-and-room-state-transitions-step-1) |
| BUG-040 | **[C]** | Per-player room options are expressible only for slots 1-5, so 10- and 20-player rooms silently ignore host configuration for slots 6-20 | Extend the option encoding to the real seat cap | IMP-019 room-size cases | [Option table](subsystems/04-lobby-settings-input.md#option-serialization-and-validation-step-2) |
| BUG-041 | **[R]** | Every transition animation frame leaks one 640×480 texture: `synchro_after` drops the texture it creates and `transitionTexture` is never assigned, so the destroy guard never fires. ~1.2 MB/frame, ~40-50 MB per animation | Assign and destroy the texture, or pass by reference | IMP-021 transition driver | [Task 7 dynamic evidence](subsystems/05-render-audio.md#dynamic-evidence) |
| BUG-042 | **[C]** | Every `NewGame` reloads 394 penguin animation textures per player plus each player's `hurryTexture` over the prior pointers, with no destroy site anywhere in the tree | Destroy before reload, or load once | IMP-021 "no leak across repeated game starts" | [Task 7](subsystems/05-render-audio.md#confirmed-findings) |
| BUG-043 | **[R]** | The multiplayer targeting indicator can never render: `targetingText` receives no `LoadFont`, so `UpdateText` early-returns before `outTexture` and `coords.w/h` exist, and rendering passes a null texture through an indeterminate rect | Load the font, or remove the render call | IMP-021 "the targeting indicator has a font before it is rendered" | [Task 7](subsystems/05-render-audio.md#confirmed-findings) |
| BUG-044 | **[R]** | `MainMenu` stores five `IMG_Load` results into `activeSPButtons[]` unchecked and `SPPanelRender` dereferences them, so a missing single-player button asset crashes the client when the panel opens | Check each load; degrade gracefully | IMP-021 "a missing asset yields a diagnostic and clean exit" | [Task 7](subsystems/05-render-audio.md#confirmed-findings) |
| BUG-045 | **[R]** | Levelset highscore rows lose their rendered text when stored: `TTFText`'s copy constructor copies nothing and its copy assignment is a no-op, so `push_back` discards the font/texture just installed | Give `TTFText` real copy semantics or store by handle | IMP-021 "a stored `TTFText` retains its texture" | [Task 7](subsystems/05-render-audio.md#confirmed-findings) |
| BUG-046 | **[C]** | `AssetExtractor.extractFile` skips any destination that already exists with non-zero length, with no content/size/timestamp comparison, while `extractAll` writes the version marker unconditionally after swallowing every exception. A truncated file survives every later launch and **a version bump does not repair it**; an app update never refreshes a changed asset | Compare content or size; write the marker only on success | IMP-020 Android install-over-install case | [Task 8](subsystems/06-platform-ports.md#bug-046--extractfiles-exists-and-non-empty-skip-caches-a-partial-extraction-forever-and-never-refreshes-a-changed-asset) |
| BUG-048 | **[A]** | WASM settings and highscores are written to volatile MEMFS: measured on the linked artifact, `IDBFS` **0**, `syncfs` **4** (all inside `FS.syncfs`'s own definition), `localStorage` **4** (all `fb_nickname`). Key bindings, audio/graphics settings, speed multiplier, mouse mode and both highscore files reset on every page load; only the nickname persists | Mount IDBFS and `syncfs` on write | IMP-020 "a headless browser reload preserves settings" | [Task 8](subsystems/06-platform-ports.md#bug-048--wasm-settings-and-highscores-are-written-to-volatile-memory) |
| SEC-006 | **[C]** | Arbitrarily long peer digit runs execute unchecked signed overflow in `charstar_to_int()` before protocol/cap validation | Bound the digit run and range-check before use | IMP-019 "a 40-digit numeric field" | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| REL-002 | **[R]** | Both room-cap harnesses daemonize onto fixed ports, can test unrelated listeners, and tear down only the launcher; the tools harness also binds UDP 1511. Because `create_udp_server` answers a failed bind with `exit(EXIT_FAILURE)`, a surviving orphan denies LAN hosting to **every** later `fb-server -l` on the host | Bind port 0, read the port back, run in the foreground, terminate in a `finally` | IMP-016 once CI runs the suite | [Harness boundary](subsystems/01-server-protocol.md#build-operations-and-harness-boundary) |
| REL-004 | **[A]** | Version metadata is hand-maintained and unlinked from the tag: **five** distinct strings across build inputs (`2.2.1`, `2.4.9`, `v2.4.26`, `2.4.27`, `0.1.0`), four reaching a runtime or artifact surface. `versionCode` is the literal `10` with no workflow override, and `share/icons/fb.rc` has **no** `VERSIONINFO` | Derive every version from the tag at configure time | IMP-016 one-version assertion | [Task 9 version trace](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| REL-005 | **[A]** | 97 tracked entries under `android/app/jni/include/SDL2/` are git mode `120000` symlinks into an absolute private path; all 97 dangle in every clone and nothing references them | Delete them | IMP-016 "no tracked dangling symlink" | [Task 8](subsystems/06-platform-ports.md#rel-005--97-dangling-absolute-symlinks-are-tracked-under-androidappjniincludesdl2) |
| REL-006 | **[A]** | Stale platform build files and self-contradicting port docs: `CMakeListsEmscripten.txt` lists 15 sources where the effective set is 28 and selects SDL2 ports; `WASM_PORT.md`, `android/SETUP.md` and `web/index.html` describe an SDL2 build that no longer exists; `default.nix` cannot build for three independent reasons | Delete or regenerate each; make CI configure every declared build file | IMP-016 "every declared build file configures" | [Task 9 build parity](subsystems/07-build-release-tooling.md#build-definition-parity-step-1) |
| REL-007 | **[A]** | Release APK signing: local `assembleRelease` emits `app-release-unsigned.apk` (`signingReport`: `Config: null`); CI generates a fresh keystore per run with a literal password appearing four times across two consecutive public workflow steps; `gradle-wrapper.properties` pins no `distributionSha256Sum` | Use a secret-held stable keystore; pin the distribution hash | IMP-020 "signed by a stable secret-held certificate" | [Task 9 signing trace](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| REL-008 | **[R]** | An installed macOS build resolves assets to the build machine's source tree: `INSTALLED_ASSET_PATH` is computed and unused, and `platform.cpp:109-121` recovers a prefix only for `.app` bundles. **No shipped artifact is affected** — the three shipped layouts all take handled paths | Use `INSTALLED_ASSET_PATH`; add a generic prefix fallback | IMP-020 "`g_dataDir` resolves inside the package" | [Task 8](subsystems/06-platform-ports.md#rel-008--an-installed-macos-build-resolves-assets-to-the-build-machines-source-tree) |
| REL-010 | **[R]** | `docker/setup.sh:23`'s `key_ok` is `openssl rsa -check`, which accepts only RSA keys, while `SetupServer.md` sends the operator to certbot — ECDSA by default since 2.0. The `else` branch then truncates **both** the operator's certificate and private key with a self-signed `CN=localhost` pair, with no message. The documented renewal block also copies to a path that does not exist from its own `cd` | Accept EC keys (`openssl pkey`); never overwrite existing material without confirmation; fix the renewal paths | IMP-020 "`setup.sh` leaves a pre-existing ECDSA key byte-identical" | [Task 9 docs review](subsystems/07-build-release-tooling.md#operational-documentation-against-actual-commands-step-6) |
| REL-011 | **[A]** | Build inputs are unpinned and SDL3 versions diverge: **27** `uses:` references, **0** commit-pinned, **5** on the mutable branch `@master` — all five the itch.io publish action, each receiving `secrets.BUTLER_CREDENTIALS`. WASM ships SDL3 **3.4.2** / SDL3_image **3.2.4** where Linux/Windows/Android agree on 3.4.4 / 3.4.2; macOS ships whatever Homebrew has | Commit-pin every action; pin SDL3 versions; checksum downloads | IMP-016 "every `uses:` commit-pinned" | [Task 9 pinning review](subsystems/07-build-release-tooling.md#dependency-and-action-pinning-step-2) |
| REL-012 | **[A]** | The macOS DMG is single-architecture and nothing says so: zero `CMAKE_OSX_ARCHITECTURES`/`-arch`/universal settings anywhere, shipping as `frozen-bubble-macos.dmg` to the itch.io channel `osx` while `README.md:6` advertises plain "macOS". **Which** architecture is excluded was not determined, because no workflow was executed | Build universal, or name the architecture | IMP-020 "the DMG contains the expected architectures" | [Task 9 artifact flow](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| REL-013 | **[A]** | Two CI steps cannot signal failure or cannot take effect: the Windows loop copies **21** named DLLs with `|| true`, so a missing `SDL3.dll`/`libstdc++-6.dll`/`libwinpthread-1.dll` still yields a green job and a shipped installer; `Cache NDK` interpolates an undefined `env.ANDROID_SDK_ROOT` to empty, so the cache never hits | Drop `|| true`; add a dependency walk and a smoke launch; fix the cache path | IMP-020 "a dependency walk finds no unresolved import" | [Task 9 artifact flow](subsystems/07-build-release-tooling.md#release-version-signing-and-artifact-flow-step-3) |
| REL-014 | **[A]** | The vendored iniparser ships in every artifact with no licence, version or provenance: `third_party/iniparser/` holds four source files and no `LICENSE`/`COPYING`/`README`/version marker, `COPYING` does not mention it, and `android/app/jni/iniparser/` is a second copy with the same gap. Upstream is MIT, whose one condition is that the notice accompany all copies | Add the upstream licence and record the version | IMP-016 "a licence file per vendored dependency" | [Task 9 IMP-008 closure](subsystems/07-build-release-tooling.md#imp-008-closure) |
| REL-015 | **[R]** | `fb-server` derives its stats path from `$HOME` unconditionally — `stats_init()` builds `<HOME>/.fb-server/stats.dat` with no flag, no cwd-relative fallback and no override other than `HOME` itself — making sandboxed or CI testing of the server structurally unsafe against the operator's real host state | Add `--stats-file`, honour a dedicated variable, or default cwd-relative as `joiners.log` already does | IMP-016/IMP-019 server jobs, once a relocation mechanism exists | [Task 10 Fix Round 1](subsystems/08-dynamic-integration.md#new-reliabilitydeployment-defect-fix-round-1) |

**REL-015 is also an audit limitation, disclosed in §8.3**: because no relocation
mechanism exists, Task 10's own 24 server launches read from and wrote to the
operator's real `~/.fb-server/stats.dat`.

### 2.5 Low-severity defects

| ID | Ev | Summary | Proposed correction | Evidence |
|---|---|---|---|---|
| BUG-008 | **[C]** | Creator-led whole-room closure frees the game but leaks every surviving seat's independently owned room nickname | Free each seat's nickname on closure | [Ownership proof](subsystems/01-server-protocol.md#allocation-owners-and-destruction-paths) |
| BUG-009 | **[C]** | Empty nickname validation succeeds, so `CREATE ` allocates an empty room that known LIST parsers cannot enumerate | Reject empty nicknames | [Length trace](subsystems/01-server-protocol.md#network-derived-length-and-index-trace) |
| BUG-010 | **[C]** | A same-name claimant can terminate a still-connected open peer once its cached whole-second receive-activity delta reaches two | Require a longer idle window, or verify liveness | [Candidates](subsystems/01-server-protocol.md#candidates) |
| BUG-016 | **[C]** | Native reachability treats a writable nonblocking connect as success without checking `SO_ERROR`, so refused endpoints appear online | Check `SO_ERROR` after the writable event | [Response handling](subsystems/02-network-client-sync.md#lobby-response-and-reachability-handling) |
| BUG-029 | **[R]** | The `WindowHeight` upper-bound guard tests `windowWidth`, so out-of-range heights are accepted verbatim | Test the height | [Persistence matrix](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| BUG-031 | **[R]** | Settings-write failures are reported only through `SDL_LogWarn` on category 1, which SDL3 suppresses by default; `CreateDefaultSettings`'s own error branch is unreachable | Report on a category SDL does not suppress | [Persistence matrix](subsystems/04-lobby-settings-input.md#dynamic-evidence) |
| BUG-038 | **[C]** | On WASM the game list is rebuilt inside the WebSocket callback between frames while joining is an index into that list, so a join can target a different room | Join by room id, not list index | [Transition map](subsystems/04-lobby-settings-input.md#menu-and-room-state-transitions-step-1) |
| BUG-039 | **[C]** | Local multiplayer is structured and documented for five players, but the setup panel caps at four and key configuration cycles players 1-4, leaving player 5's bindings unreachable | Extend the panel and the key-config cycle to 5 | [Input bounds](subsystems/04-lobby-settings-input.md#keyboard-controller-and-mouse-bounds-step-4) |
| BUG-047 | **[R]** | The log path is CWD-relative and its name counts launches rather than players; `Logger::Initialize`'s failure return is discarded, and on a read-only CWD all file diagnostics vanish silently | Use a writable per-user path; honour the failure return | [Task 8 dynamic evidence](subsystems/06-platform-ports.md#dynamic-evidence) |
| BUG-050 | **[R]** | `LIST` reports a `free:` count that contradicts the open-player list in the same message. Impact is confined to third-party/legacy clients and operators, because the shipped client never reads `free:` | Exclude seated players from `free:` | [Task 10](subsystems/08-dynamic-integration.md#confirmed-findings) |
| BUG-051 | **[C]** | `LoadLevelset` writes `level[idx]` into a `std::array<..,10>` with **no bound check**, unlike its sibling loader which guards the identical write. Not currently reachable: all 100 shipped level blocks have exactly 10 lines and no level-editor write path exists | Add the `idx < 10` guard the sibling already has | [Gameplay](subsystems/03-gameplay.md#confirmed-findings) |
| REL-001 | **[C]** | Server OOM diagnostics pass `size_t` to the signed `%zd` conversion | Use `%zu` | [Static disposition](subsystems/01-server-protocol.md#task-2-candidate-dispositions) |
| REL-009 | **[A]** | Maintained documentation contradicts the shipped system in three places: `CLAUDE.md`'s CI section (0 of 11 jobs carry `if: false`; `release` needs all five build jobs and attaches 5 files), `README.md:274-279`'s public-server-list format, and `CLAUDE.md:54`'s `bubbleArrays[5]` / "1-5 players" against a 20-element array | Correct all three | [Task 9 docs review](subsystems/07-build-release-tooling.md#operational-documentation-against-actual-commands-step-6) |

REL-009's cost was not cosmetic: the `CLAUDE.md` CI drift caused this audit's own
Task 8 to under-weight four shipped platforms until Task 9 measured the workflow
directly.

---

## 3. Security and protocol assessment

### 3.1 Standing restriction — read this before reading the findings

**The user explicitly skipped security-specific runtime testing.** No gate in this
audit sent hostile traffic, attempted an exploit, ran a fuzzer, or performed any
offensive runtime probe. **SEC-001 through SEC-007 are static, code-supported
inferences.** They are carried forward here with that limitation intact, neither
softened nor treated as resolved.

Concretely omitted, at the gates that would have owned them: Task 3's whole
runtime/security matrix; Task 6's forged-`OPTIONS` and out-of-range-team checks;
Task 9's supply-chain and credential-exposure checks; and **six of Task 10's 41
matrix rows** — fragmentation/coalescing, mid-frame disconnect, frames claiming a
foreign player id, duplicated `n`/`F`/`S` frames, reordered `b`/`N`/`T` sync
frames, and bounded flooding. Each is a limitation. **None is a pass.**

### 3.2 Trust boundaries

| Boundary | Trusted? | Findings on it |
|---|---|---|
| LAN UDP discovery datagram → `server/net.c` | **No** | SEC-005 |
| Remote TCP/WebSocket line → server command parser | **No** | BUG-006, BUG-009, SEC-006 |
| Master-server HTTP response → server | **No** | SEC-002 |
| Room member → relayed binary frame → peers | **No** | SEC-004 |
| Room **creator** → `SETOPTIONS` → peers' `SetupSettings` | **No** | SEC-007 |
| Peer game frame → client board indexing | **No** | SEC-003 |
| Local INI / highscore / level files → client | **No** | BUG-026..032, BUG-051 |
| Bundled `share/` assets → client | Yes, and unchecked | BUG-001, BUG-044 |

### 3.3 Two propositions this audit examined and rejected

Both were asserted by earlier gates, and both were **disproved** against the
pinned source in the independent final challenge. They are recorded here so this
report cannot silently resurrect them:

1. **"A non-creator may send `OPTIONS:`/`SETOPTIONS`."** **False.**
   `server/game.c:405` gates `setoptions` on slot zero (`if (g->players_conn[0]
   == fd)`), rejecting others with `wn_not_creator` at `:415` — verified at the
   pinned commit. This narrows SEC-007's threat model to a hostile or buggy room
   creator. Notebook 04's trust table asserted the opposite and contradicted
   notebook 01's own correct row; it has been corrected.
2. **"`DoSnipIn` produces the transition animation."** **False.**
   `transitionmanager.cpp:48-60` shows `DoSnipIn` captures `snapIn` and calls no
   `effect()` — it animates nothing. The BUG-041 leak is produced by
   `TakeSnipOut` (`:62-75`), whose sole call site is
   `bubblegame_render.cpp:1173`, armed by `!firstRenderDone`. **`QuitToTitle`
   (`bubblegame.cpp:1363`) clears that flag and therefore *is* a trigger** —
   this reversed an earlier Task 7 correction that was itself wrong.

### 3.4 Protocol observations that are not defects

Recorded so they are not re-investigated: the `LIST` open-player list's trailing
comma is by design and parses correctly; `games:0` beside an open room is
consistent with `games:` being defined as *running* games; seat ids in a 20-seat
room were measured as 20 distinct contiguous values with no collision and none
landing on a framing byte; and mid-game reconnect refusal is deliberate design,
not a defect. Full text in
[notebook 08's dismissals](subsystems/08-dynamic-integration.md#dismissed-candidates).

### 3.5 Residual security risk

Because no security runtime testing was performed, the honest statement of
residual risk is: **the seven SEC findings describe reachable code paths whose
consequences were derived, not observed, and the audit cannot bound what
additional consequences hostile traffic would produce.** SEC-002's extended
heap-overflow consequence is specifically **open and unresolved**. Any future
remediation should treat the SEC set as a starting point for the security testing
this audit did not perform, not as an exhaustive inventory.

---

## 4. Platform, build, and release assessment

### 4.1 Platform status — what was and was not exercised

| Platform | Build | Runtime | Status |
|---|---|---|---|
| macOS arm64 (native) | Configured and built clean | Dummy video/audio drivers only; production-object harnesses | **Partially exercised.** No Metal/GPU renderer, real window, fullscreen toggle, live resize or real audio device. No human-driven menu-to-game session at any gate |
| Linux | Not built | Not run | **Unavailable — not a pass.** `/proc/self/exe` prefix recovery and the AppImage layout are source traces plus CI job definitions |
| Windows | Not built | Not run | **Unavailable — not a pass.** `GetModuleFileNameA` branch, its failure fallback and the packaged DLL layout were read from the workflow only |
| WASM | **Complete link**, all four artifacts, against a disposable port-patched Emscripten copy | **No browser runtime** | **Build passed; runtime unavailable — not a pass.** No page rendered, no frame drawn, no console output collected |
| Android | **Three-ABI release APK built locally**, zero tracked-file drift | **No device or emulator** | **Build passed; runtime unavailable — not a pass.** BUG-046, extraction ordering, teardown, TV-remote input and the ad/billing flows are code-supported inferences |

### 4.2 Build system

The native build configures and builds cleanly, but **the Release build is not
warning-clean**: AppleClang emitted **51** server warning instances from **27**
unique locations. The strict Debug build cannot complete until IMP-001 through
IMP-004 are resolved; its subsequent 3/5 not-run CTest result is a downstream
missing-executable consequence, not an independent defect.

Source-list parity across the build definitions was measured: the root
`CMakeLists.txt` and `android/app/CMakeLists.txt` agree exactly at 28 `src/*.cpp`
entries; `CMakeListsEmscripten.txt` lists 15 and is stale (REL-006);
`default.nix` cannot build at all (REL-006). Warning flags reach the native,
Windows, WASM and server builds but **not** the Android build (IMP-022).

### 4.3 CI and release

`.github/workflows/build.yml` was parsed with Ruby Psych (exit 0): **11** jobs,
**0** carrying `if: false`, `release` `needs` all five build jobs and attaches
**5** files. This directly contradicts `CLAUDE.md`'s CI section, which claims four
of five platform jobs are disabled — registered as REL-009.

**Automated testing is effectively absent.** Measured: **0** occurrences of
`ctest`, `BUILD_TESTING`, `--target test`, `gradlew test` or `pytest` across the
workflow — the only file in `.github/workflows/`. Compilation is the entire
automated gate for 11 jobs, while **5** CTest tests sit registered and unrun.
This is the single highest-leverage improvement in the report (IMP-016).

**No workflow was executed.** Every CI conclusion is a reading of the workflow
text against documented Actions semantics. Specifically unexamined: which
architecture `macos-latest` resolves to, whether `upload-artifact` preserves the
AppImage executable bit, which Emscripten release `latest` resolves to, whether
all 21 named MinGW DLLs exist on the runner, and the default `GITHUB_TOKEN`
scope. These are **unexamined, not passed.**

---

## 5. Suggested improvements

**Improvements are not defects.** None of the 24 entries below is assigned a
defect severity, because none has defect evidence of its own. Each is ranked by
expected benefit, implementation effort, and regression risk. Where an
improvement's investigation *did* surface a real defect, that defect was promoted
to its own ID and appears in §2 — IMP-005 → BUG-034 and BUG-043, IMP-007 →
BUG-045, IMP-008 → REL-014 and IMP-023.

### 5.1 Priority 1 — high benefit

| ID | Benefit / Effort / Risk | Target | Change | Verification |
|---|---|---|---|---|
| **IMP-016** | High / Low / Low | `.github/workflows/build.yml` | Run the registered checks in CI: a `test` job (Ubuntu 22.04 Debug, `ctest --output-on-failure`, all 5 passing once REL-002's isolation lands), made a `needs` of `release`. Add the repository-hygiene assertions this audit had to make by hand — no tracked dangling symlink, one version value, every declared build file configures, a licence per vendored dependency, every `uses:` commit-pinned, no step that cannot fail | The job itself |
| **IMP-017** | High / Medium / Low | `tests/gamesettings_test.cpp` | Settings, persistence and input-bounds regression tests over isolated `CFFIXED_USER_HOME`, a read-only preferences directory, INI syntax errors, `Key=99999`, `WindowHeight=100000`, `SpeedMultiplier=nan`, truncated highscore files | Asserts BUG-026..032, BUG-034, BUG-035, BUG-036, BUG-039, BUG-040, BUG-033 |
| **IMP-018** | High / Medium / Medium | `tests/bubblegame_rules_test.cpp` | Promote Task 5's production-object gameplay harness into CTest — it already reproduced BUG-018, BUG-019 and BUG-025 against unchanged production objects and is simply unregistered | Asserts BUG-018..025 |
| **IMP-019** | High / High / Low | `tests/server_parse_test.c`, `tests/netclient_parse_test.cpp` | Protocol and parser unit tests. **Inputs are well-formed and boundary-shaped only — hostile traffic stays outside the audit's scope** | Each parse either succeeds with the correct value or is rejected without indexing outside its buffer, and no path calls `exit()` |
| **IMP-020** | High / Medium / Low | A `package-smoke` job per platform | Packaged-artifact smoke tests. **Every finding in this class was established statically or at harness level because no gate could launch a packaged artifact** | Asserts REL-003, REL-008, BUG-034, BUG-047, BUG-046, BUG-048, REL-013, REL-007, REL-004, REL-012, REL-010 |
| **IMP-005** | High / Medium / Medium | Client headers | Default-initialize state-bearing C++ members. Its one reachable use-before-initialization in Task 6 became BUG-034; its render-slice instance became BUG-043 | Compiles + IMP-017 |

### 5.2 Priority 2 — medium benefit

| ID | Benefit / Effort / Risk | Summary |
|---|---|---|
| IMP-021 | Medium / Medium / Low | Resource-lifetime regression job **on Linux only — the one platform where LeakSanitizer works.** Its transition driver must clear `firstRenderDone` and call `TakeSnipOut`; driving `DoSnipIn` animates nothing, so as originally specified its BUG-041 assertion could not fail |
| IMP-001 | Medium / Low / Low | Strict C prototypes for the six no-argument server functions |
| IMP-002 | Medium / Low / Low | Make seven server length/index comparisons type-consistent (their reviewed bounds do not establish a defect) |
| IMP-004 | Medium / Low / Low | Remove dead `today`; restore intended state use for `was_playing` while fixing BUG-005 |
| IMP-006 | Medium / High / Medium | Make bounded gameplay/render numeric conversions and integer-division intent explicit |
| IMP-007 | Medium / Low / Low | Replace `TTFText`'s silent no-op copy assignment and reset-to-empty copy constructor, and fix the `Dispose()`/destructor singleton pattern that leaves `ptrInstance` dangling. The copy half **is** tripped in production — promoted to BUG-045 |
| IMP-009 | Medium / Medium / Low | Simplify redundant branches, dead stores, unused attachment helpers and missing-default control flow |
| IMP-010 | Medium / Medium / Medium | Handle raw allocation and asset-load failure consistently — six subsystems currently follow six different policies |
| IMP-011 | Medium / Medium / Low | Replace the synchronous 50 ms accept-time WebSocket sniff with event-loop upgrade state |
| IMP-012 | Medium / Low / Low | Remove or wire up the menu's unreachable code (two-player panel, network setup panel, `editor` button, `LevelEditor`/`Netplay` states, `menuText`, and more) |
| IMP-013 | Medium / Low / Low | Fix the off-by-one clamp bounds guarding the pixel helpers at **four** sites — `shaderstuff.cpp:49`, `:488`, `:1155` and `:1158`. ASan-proven read; unreachable with shipped assets but latent if a mask asset changes |
| IMP-014 | Medium / Low / Low | Exclude the duplicate `libpng.so` and unused `libvorbisenc.so` from the APK — 2,666,728 bytes across three ABIs of a 37,290,226-byte APK |
| IMP-015 | Medium / Low / Low | Remove the dead platform layer (`extractAssets()` placeholder, unused `_WIN32` mkdir macro, `bzero`/`<iconv.h>` for block-commented code, `AdsManager.init()`, `__ANDROID_PORT__`) |
| IMP-022 | Medium / Low / Low | Unify warning configuration — `android/app/CMakeLists.txt` applies **none**, including on the 32-bit `armeabi-v7a` ABI where narrowing diagnostics matter most |
| IMP-023 | Medium / Low / Low | Constrain the iniparser dependency boundary: `find_package(iniparser QUIET COMPONENTS static)` silently prefers any system copy, with no version constraint |
| IMP-024 | Medium / Low / Low | Validate and diagnose the `CREATE` room-cap argument instead of silently substituting 5. **Not a defect** because the shipped client offers only `{5, 10, 20}`, but the server is a public endpoint |

### 5.3 Priority 3 — low benefit or closed

| ID | Benefit / Effort / Risk | Summary |
|---|---|---|
| IMP-003 | Low / Low / Low | Mark or remove six intentionally unused GLib callback/signal parameters |
| IMP-008 | Low / High / Low | **Closed in Task 9.** Selective constness/API/cast/shadowing cleanup. Neither analyzer inspects CMake/YAML/shell/Python/Nix/Markdown, so 0 diagnostics of any IMP-008 family have a path among Task 9's files. No defect was promoted from any IMP-008 family; the vendored-dependency boundary it deferred produced IMP-023 and REL-014 |

---

## 6. Test and analysis commands with results

### 6.1 Task 13 verification commands

Every headline quantity in this report was re-derived here. One top-level command
per row; the exit code is that command's own real integer exit status. `grep`,
`rg` and `lsof` exit **1** on no match, and that is recorded as 1, never
normalized.

| # | Command | Exit | Result |
|---|---|---|---|
| 1 | `grep -cE '^\| (BUG\|SEC\|REL\|IMP)-[0-9]{3} \|' docs/audit/FINDINGS.md` | 0 | **98** registry table rows |
| 2 | `grep -oE '^\| (BUG\|SEC\|REL\|IMP)-[0-9]{3} \|' docs/audit/FINDINGS.md \| grep -oE '(BUG\|SEC\|REL\|IMP)' \| sort \| uniq -c` | 0 | **52** BUG, **24** IMP, **15** REL, **7** SEC |
| 3 | `awk -F'\|' '/^\| (BUG\|SEC\|REL\|IMP)-[0-9]{3} \|/ {…print $3}' FINDINGS.md \| sort \| uniq -c` | 0 | **97** confirmed, **1** dismissed |
| 4 | `awk -F'\|' '…{gsub…; print $2}' FINDINGS.md \| sort \| uniq -d` | 0 | No output — **0 duplicate IDs** |
| 5 | `awk -F'\|' '/^\| (BUG\|SEC\|REL)-…/ {if($3=="confirmed") print $4}' FINDINGS.md \| sort \| uniq -c` | 0 | **15** High, **45** Medium, **13** Low (0 Critical); total **73** |
| 6 | `awk -F'\|' '…{print $5}' FINDINGS.md \| sort \| uniq -c` | 0 | **98 High** — every row is High confidence |
| 7 | `grep -cE '^\| \`[^\`]+\` \|' docs/audit/FILE_COVERAGE.md` | 0 | **237** coverage rows |
| 8 | `awk -F'\|' '…{print $4}' FILE_COVERAGE.md \| sort \| uniq -c \| awk '{s+=$1;n++} END{print n,s}'` | 0 | **42** distinct dispositions summing to **237** |
| 9 | `git ls-tree -r --name-only <pinned> \| rg '<Task 1 Step 4 pattern>' \| sort > inv.txt` | 0 | **237** paths regenerated from the pinned tree |
| 10 | `diff <(coverage paths, sorted) inv.txt` | 0 | **No output** — exact set equality |
| 11 | `awk -F'\|' '…{print $4}' FILE_COVERAGE.md \| grep -ci pending` | **1** | **0** disposition cells contain `pending` (exit 1 = no match, recorded truthfully) |
| 12 | `for f in docs/audit/subsystems/*.md; do … grep -E '^## ' …; done` | 0 | All **9** notebooks carry exactly the 10 required headings, once each, in order |
| 13 | `for f in …0[1-8]*.md; do awk '…Dismissed candidates…' \| wc -l; done` | 0 | **43** bullet-form dismissals, all in notebooks 01-07 |
| 14 | `awk '…Dismissed candidates…' 08-dynamic-integration.md \| wc -l` (paragraph form) | 0 | **4** further dismissals in notebook 08 → **47** total |
| 15 | `python3` — reconcile Task 12's upheld enumeration against the 72 confirmed defects | 0 | **62 enumerated + 9 revised = 71 ≠ 72; REL-010 missing.** See §6.3 |
| 16 | `python3` — count load-bearing verification bullets vs upheld IDs | 0 | **58** bullets for **63** upheld; BUG-001, BUG-015, BUG-019, BUG-025, REL-010 have none |
| 17 | `python3` — scan all `docs/audit/**.md` for duplicate heading anchors | 0 | 1 file with a duplicate (`task-10-fix-round-1`, ×2) — **fixed**, see §6.3 |
| 18 | `python3` — rescan for duplicate anchors after the fix | 0 | **0** files with duplicate anchors |
| 19 | `grep -rn "task-10-fix-round-1" docs/audit/` | **1** | No inbound links to the ambiguous anchor — the duplicate was latent, not live |
| 20 | `git grep -cn "NetworkClient…Dispose()" <pinned> -- src server` | 0 | **1** occurrence — the definition at `networkclient.cpp:60`. **`Dispose()` has zero callers** |
| 21 | `git show <pinned>:src/networkclient.cpp \| awk '/^void NetworkClient::Disconnect/,/^}/' \| grep -n recvBufferLen` | **1** | No match — **`Disconnect` never resets `recvBufferLen`** |
| 22 | `git show <pinned>:src/networkclient.cpp \| sed -n '843p'` | 0 | `if (recvBufferLen + received < BUFFER_SIZE) {` — BUG-052's guard, verbatim |
| 23 | `git show <pinned>:src/networkclient.h \| sed -n '36p'` | 0 | `#define BUFFER_SIZE 4096` |
| 24 | `git show <pinned>:server/game.c \| sed -n '405p;415p'` | 0 | `if (g->players_conn[0] == fd) {` / `send_line_log(fd, wn_not_creator, "SETOPTIONS");` — **creator-only, confirmed** |
| 25 | `git show <pinned>:src/transitionmanager.cpp \| sed -n '48,75p'` | 0 | `DoSnipIn` calls no `effect()`; `TakeSnipOut` does — **confirmed** |
| 26 | `pgrep -alf fb-server` | 0 | **3** foreign `fb-server` processes; **PID 74458 is gone** — see §6.2 |
| 27 | `lsof -nP -iUDP:1511` | **1** | No match — **UDP 1511 is now free** |
| 28 | `lsof -nP -iTCP:15113 -sTCP:LISTEN` | **1** | No match — no listener on 15113 |
| 29 | `ps -o pid,etime,command -p 22293,22300,76361` | 0 | Elapsed 4d11h, 4d11h, 4d01h — all predate the audit |
| 30 | `pgrep -alf 'frozen-bubble-sdl3\|task1[0-9]\|scenario.py\|run_case.sh\|websockify'` | **1** | No match — **no audit-owned process remains** |
| 31 | `rg -c -n 'TB[D]\|TO[D]O\|investigating\|suspected' docs/audit --glob '!SDL3_COMPLETE_REVIEW.md'` | 0 | **13** hits outside this report, all judged and retained; **0** are unresolved work markers — see §6.4 |
| 32 | `git diff --check` | 0 | No whitespace or conflict-marker errors |
| 33 | `python3` — every finding ID cited in this report exists exactly once in `FINDINGS.md` | 0 | See §6.4 |
| 34 | `python3` — every relative link and anchor in this report resolves to exactly one target | 0 | See §6.4 |

### 6.2 Process and listener state at report time

Checked passively; **Task 13 launched nothing and killed nothing.**

Three `fb-server` processes are running, all foreign to this audit and all
predating it by days: PIDs 22293 and 22300 (ports 15511/15512, from a *different*
repository, 4 d 11 h) and PID 76361 (port 15998, 4 d 01 h). These are three of the
four Task 10 and Task 12 enumerated before and after their own runs.

**The fourth has exited.** Task 12 recorded PID 74458 —
`tools/server_tests/../../build/server/fb-server -p 15113 -q -l -z`, then 1 d 21 h
old — holding `UDP *:1511` and `TCP *:15113`, and deliberately left it untouched
because it was not the audit's process. At report time `lsof -nP -iUDP:1511` and
`lsof -nP -iTCP:15113 -sTCP:LISTEN` both exit **1** with no output, and the PID is
absent from `pgrep`. It was not killed by this audit; it ended on its own.

**That orphan is worth recording as REL-002 observed in the wild.** It was
launched by `tools/server_tests/test_room_caps.py`, which uses the fixed
`PORT = 15113` and passes `-l` without `-d`, so the server daemonizes and the
harness's `terminate()`/`kill()` reach only the launcher. While such an orphan
lives, `create_udp_server`'s `perror`/`exit(EXIT_FAILURE)` on a failed UDP 1511
bind makes **every** later `fb-server -l` on the host die at startup — including
`start-server.sh` and the client's own `StartLocalServer` path. It survived
roughly two days before exiting.

No audit-owned server, client, harness, listener, proxy or background process
remains (command 30, exit 1).

### 6.3 Inconsistencies found in Task 13 and how they were resolved

Task 13 re-derived rather than trusted, and found three problems in the artifacts
it was synthesizing. All three are resolved; none is papered over.

**(a) Task 12's defect-disposition partition did not sum to its own total.
Corrected 62 → 63.**

The disposition table in `subsystems/09-final-challenge.md` enumerated **62**
upheld IDs against **9** revised and **0** dismissed — a partition summing to
**71**, while the same gate states in four places that it challenged **72**
confirmed defects. Re-deriving the set difference identified the missing ID
as **REL-010**, absent from *both* enumerations.

This is a plain enumeration omission, not two figures counting different things.
The evidence that REL-010 belongs in the upheld row: (i) the same notebook's
`Coverage` section names REL-010 explicitly as challenged, recording that it
retains an un-re-derived certbot premise (`subsystems/09-final-challenge.md:594`);
(ii) its registry row's **Evidence** column carries no `[Task 12 revisions]`
link, unlike all nine actually-revised entries — `grep -n '^| REL-010 |'
docs/audit/FINDINGS.md | grep -c 'Task 12 revisions\]'` exits **1** with output
`0`, while `grep -nE '^\| (BUG|SEC|REL|IMP)-[0-9]{3} \|.*\[Task 12 revisions\]'
docs/audit/FINDINGS.md | wc -l` counts exactly **9** rows, none of them REL-010.
(An earlier draft of this correction cited the **Gate** column instead —
"unchanged `confirmed | Medium | High | Task 9`" — but that test is false:
REL-009, one of the nine revised entries, also carries Gate `Task 9`
(`FINDINGS.md:104`); the Gate column does not distinguish REL-010 from the
revised set. The Evidence column does.); (iii) it is not dismissed. Not-revised
and not-dismissed and challenged ⇒ upheld. The correct count is **63**, and
63 + 9 + 0 = 72.

Root cause: REL-010 is the one upheld defect whose disposition carries a
qualification, and it was written up in `Coverage` prose instead of being carried
into the table. The wrong figure had propagated to **five** places — the
disposition table, two further sentences in notebook 09, the status file's
gate-checklist row, the status file's Task 12 closure provenance, and
`FINDINGS.md`'s Task 12 paragraph. All are corrected, each with an inline note
recording what the figure was and why it was wrong, following the correction
convention this audit already used for the 41 → 42 disposition census and the
20 → 21 DLL count.

**(b) "43 explicit dismissal bullets" and "47 explicit dismissals" count
different things. Both are now stated.**

The 43 figure is a count of **bullet-form** entries (`^- `) in the
`Dismissed candidates` sections, and all 43 live in notebooks **01-07**.
Notebook 08 formats its **4** dismissals as bold paragraphs rather than bullets —
duplicate seat ids, the `LIST` trailing comma, `games:0` beside an open room, and
mid-game reconnect refusal — so a bullet-based count cannot see them. The number
of explicit dismissals across notebooks 01-08 is therefore **47**, of which **43**
are evidenced as read. Whether notebook 08's four were also read is not recorded
either way, so they are carried forward as *challenged-status-unevidenced* rather
than as challenged. This is a smaller residual than it sounds — all four are
protocol observations reproduced at runtime in Task 10 — but it is stated rather
than smoothed. Separately, Task 10's gate-checklist row says "three candidates
dismissed" where its notebook records four; the notebook is the evidence-bearing
document.

**(c) A duplicate heading anchor in the status file. Fixed.**

`SDL3_REVIEW_STATUS.md` carried two identical `### Task 10 Fix Round 1`
headings, producing an ambiguous `#task-10-fix-round-1` anchor — the same defect
class the audit already fixed once for Task 9. No document linked to it (command
19, exit 1), so it was latent rather than live. Disambiguated into
`### Task 10 Fix Round 1 Findings` and `### Task 10 Fix Round 1 Ledger`,
following the Task 9 precedent. A rescan finds **0** files with duplicate
anchors.

**(d) Noted, not corrected:** notebook 09's load-bearing verification list is
**58** one-line entries, not one per upheld ID as its introductory sentence
implied. The five upheld IDs without an entry are BUG-001, BUG-015, BUG-019,
BUG-025 and REL-010. BUG-019, BUG-025 and REL-010 are covered by explicit
`Limitations` entries recording that they were re-derived only to the level of
their cited mechanism; **BUG-001 and BUG-015 have no such entry, so their
re-derivation in the final challenge is asserted rather than evidenced.** The
sentence has been corrected to state 58; the two under-evidenced re-derivations
are carried into §8.4 as a residual risk.

### 6.4 Step 5 validation

**Marker sweep.** `rg -c -n 'TB[D]|TO[D]O|investigating|suspected' docs/audit
--glob '!SDL3_COMPLETE_REVIEW.md'` (exit 0) returns **13** hits, distributed
`FINDINGS.md` 1, `subsystems/09-final-challenge.md` 1,
`subsystems/07-build-release-tooling.md` 3, `SDL3_REVIEW_STATUS.md` 8.
**Zero are unresolved work markers.** The brief's check is not a mechanical
zero-requirement, so each hit was judged, not assumed clean. (A prior published
draft of this section stated **11**, with `SDL3_REVIEW_STATUS.md` at 6 — wrong.
Re-deriving it found 15 at first, because correcting the false 11 into a longer
explanation itself added two more hits to `SDL3_REVIEW_STATUS.md`; that
explanation was then rewritten to describe the added categories without
repeating the swept-for words, which brought the true, stable count to 13 —
`SDL3_REVIEW_STATUS.md` at 8. See the recursion note below for why this number
moves and this report's own text does not.)

The 13 split into two kinds:

- **5 are evidence about the *project's* own disclosed or actual `TODO`
  markers, not the audit's.** `07-build-release-tooling.md`'s three hits record
  that `README.md:339-340` lists macOS and Windows code signing as the
  *project's own* disclosed open TODOs — evidence for REL-007's scope.
  `SDL3_REVIEW_STATUS.md` contributes two more of the same kind: its Task 11
  gate-checklist row noting "`TODO`/`FIXME`/`XXX`/`HACK` markers exist only
  inside vendored `org/libsdl/app/` files" and its Task 11 closure-provenance
  ledger row recording the same `grep -rnE 'TODO|FIXME|XXX|HACK' src server
  tools tests android/app/src/main/java` sweep of *project source* — neither is
  a sweep of `docs/audit`, and neither is outstanding audit work.
- **8 are this audit's own convention text or self-referential narration about
  a sweep's result, not outstanding work.** `FINDINGS.md:3` is the candidate
  lifecycle rule's own definition (`suspected -> investigating ->
  confirmed/dismissed`) — retained, because deleting the convention would
  remove the definition that makes every other state meaningful.
  `09-final-challenge.md:381` is one such assertion. The remaining six are all
  in `SDL3_REVIEW_STATUS.md`: the Task 11 gate-checklist row and its Step 3
  candidate-registry paragraph, both stating that a `suspected`/`investigating`
  sweep of the registry or of `docs/audit` came back clean; two earlier ledger
  rows checking the registry's `confirmed`/`dismissed` disposition column and
  noting no row sits in `suspected` or `investigating`; one still-earlier
  ledger row recording that only `FINDINGS.md`'s own rule definition remained
  after a prior `suspected`/`investigating` grep; and, unavoidably, the ledger
  row for this very command (row #31) — each is a record of a sweep, not a
  marker left by one.

**Recursion, stated explicitly.** This report is excluded from the command by
`--glob '!SDL3_COMPLETE_REVIEW.md'`, but `SDL3_REVIEW_STATUS.md` is not, and it
necessarily narrates this sweep in the words the sweep searches for. Every edit
to that narration can change `SDL3_REVIEW_STATUS.md`'s hit count again — this
correction's own drafting did so twice before landing on 13, as recorded above
— so a "re-run and write the number" fix is stale the moment its own prose is
committed. **13** was re-verified against the tree actually committed for this
correction, not only the working tree beforehand; if it has since moved, the
committed ledger row (§6.4, row #31, and the status file's own copy) carries
whatever superseding note was needed to keep the three in agreement.

The check that does not move under this recursion, because it is
machine-readable rather than narrated, is the one that actually matters: **no
registry row is in a `suspected` or `investigating` state** — command 3
measures **97 confirmed + 1 dismissed = 98**, with no third state.

**`git diff --check`** exits **0** with no output.

**Finding-ID integrity.** Every `BUG-`/`SEC-`/`REL-`/`IMP-` ID cited in this
report exists **exactly once** as a registry table row in `FINDINGS.md`
(command 33). No report ID is absent from the registry, and no ID is cited that
the registry does not define.

**Link and anchor integrity.** Every relative link in this report resolves to an
existing file, and every anchor resolves to **exactly one** heading in its target
(command 34). This was checked against real headings rather than assumed, because
this file set has carried ambiguous duplicate headings before — one such
duplicate was found and fixed in this very task (§6.3(c)).

**Severity totals.** The report's §1.2 table (15 High / 45 Medium / 13 Low /
0 Critical, total 73) is the direct output of command 5 over the registry. The
per-severity ID lists in §1.2 are the same command's per-ID output.

**Coverage completeness.** All **237** rows carry a final disposition; **0**
disposition cells contain `pending` (command 11, exit 1). The 237 paths are in
exact set equality with a fresh regeneration of the original selection command
(command 10, exit 0, empty diff).

---

## 7. Complete file-coverage appendix

The full 237-row ledger — one row per in-scope tracked path, with gate,
disposition, evidence link and notes — is
[`FILE_COVERAGE.md`](FILE_COVERAGE.md). Summary:

| Property | Value |
|---|---|
| In-scope tracked paths at the pinned commit | **237** |
| Rows in the ledger | **237** |
| Set equality with a fresh regeneration | **exact** (`diff` empty, exit 0) |
| Distinct disposition strings | **42**, summing to **237** |
| Rows in a pending state | **0** |

### 7.1 Two honest qualifications on the coverage claim

1. **Three of the design's five disposition classes are used.** No row is
   dispositioned *Excluded with a recorded reason* or *Blocked with a recorded
   limitation*. Blocked **checks** — browser runtime, Android device runtime,
   Linux/Windows execution, packaged-artifact startup, security runtime — are
   recorded in §8 instead. That is defensible, because the blocks are on checks
   rather than on reading the files, but **a reader of the ledger alone cannot
   see which rows rest on an unavailable platform.**
2. **Six maintained tracked paths inside the design's stated scope carry no
   row**, because the original selection pattern does not match them:
   `CLAUDE.md`, `CHANGELOG.md`, `.gitignore`, `.gitmodules`, `COPYING`, and the
   four `third_party/iniparser/*` files. Five are *cited as evidence for
   confirmed findings* — `CLAUDE.md` and `CHANGELOG.md` for REL-009,
   `.gitignore:8` for REL-006, `COPYING` and `third_party/iniparser/` for
   REL-014 — so the audit reviewed their content without dispositioning them.
   Note the asymmetry: the Android duplicate `android/app/jni/iniparser/*` has
   four rows while the primary copy — the one statically linked into every
   desktop, Windows and WASM artifact — has none.

### 7.2 Challenged and dismissed — preserved so they are not resurrected

**One registry-level dismissal.** **BUG-012** (priority ingress diverting `FB/`
text to the command parser) is dismissed, and **its ID is retired and never
recycled.** The final challenge re-examined it and confirmed the dismissal at
`net.c:355-357` and `game.c:838`.

**Forty-seven notebook-level dismissals** are recorded across notebooks 01-08
with the counter-evidence that ruled each out, so a future reader does not repeat
the work. Four are worth naming because they were disproved *by running the
command that doubted them*, which is the strongest form of dismissal in this
audit:

- `cmake/Emscripten.cmake` "breaks the documented configure" — **disproved**: the
  documented command succeeds.
- `cmake_uninstall.cmake.in` uses a "removed" CMake command — **disproved**:
  `exec_program` is deprecated, not removed (exit 0, `CMP0153` warning only).
- The `idleSPButtons` indeterminate-rect mechanism — **disproved**:
  `SDL_GetTextureSize` zeroes its outputs before rejecting a null texture, so the
  rect is deterministic zeros and nothing draws.
- `-sDISABLE_EXCEPTION_CATCHING=0` is link-only so `try`/`catch` miscompiles —
  **disproved**: there are **zero** live `catch` handlers in the whole tree.

One dismissal is explicitly *not* resolved in the project's favour:
`netlify.toml`'s effective publish directory depends on Netlify site settings
outside the repository, so it is **unresolvable from repository evidence** and was
dismissed on that basis rather than cleared.

---

## 8. Known limitations and residual risks

**Every item here is a limitation, not a pass.** Where a platform or runtime check
could not be performed, this report says so rather than implying it succeeded.

### 8.1 Scope restrictions set by the user

- **No security-specific runtime testing anywhere in this audit.** SEC-001
  through SEC-007 remain code-supported inferences. **SEC-002's extended
  consequence — a `Content-Length` wrapping to exactly `-1` yielding
  `bufsize == 0` and a `recv` length of `(size_t)-1` at `server/net.c:1253`, i.e.
  an attacker-length write past a zero-size allocation — is an open limitation,
  not a resolution.** Settling it would require exactly the class of testing that
  is out of scope.
- Six of Task 10's 41 integration matrix rows were omitted for this reason
  (fragmentation/coalescing, mid-frame disconnect, foreign-player-id frames,
  duplicated `n`/`F`/`S`, reordered `b`/`N`/`T`, bounded flooding).

### 8.2 Environment limitations

- **Apple ASan cannot detect leaks.** `detect_leaks=1` is rejected on this host;
  every sanitizer run in this audit used `detect_leaks=0`. **No leak conclusion
  anywhere in this audit rests on a sanitizer pass** — BUG-001, BUG-008, BUG-013,
  BUG-041 and BUG-042 all rest on ownership tables, grep-verified absence of
  destroy sites, and RSS measurement. IMP-021 exists specifically to move leak
  verification to Linux, where LeakSanitizer works.
- **No browser runtime.** The WASM result is a successful build and artifact
  analysis only: no page rendered, no frame drawn, no console output collected.
- **No Android device or emulator.** The APK was built and analyzed statically.
- **No Linux or Windows host.** Only macOS packaged-path cases were run.
- **No packaged artifact was ever launched**, and dynamic-library independence
  was not confirmed. What ran instead was a harness linking unchanged production
  `platform.cpp.o` and `logger.cpp.o`; it constructs no `FrozenBubble`, creates
  no window, loads no asset and opens no real preference file.
- **No GitHub Actions workflow was executed**, no container was started, and no
  external network operation was performed. Every CI, Docker and dependency
  conclusion is a reading of a file.
- **Rendering ran exclusively on the dummy video driver's software renderer** —
  no Metal/GPU renderer, real window, fullscreen toggle, live resize or real
  audio device.
- **No gate ever drove the shipped client through its menus by hand.** SDL's
  dummy video driver accepts no injected input and `src/main.cpp:27` takes no
  arguments, so client coverage is a whole-program startup/shutdown smoke plus
  production-object harnesses — which together do not equal one human-driven
  session. Four Task 10 manual visual/audio rows (clear-win banner and sound,
  spectator pinning, >5-player mini-board paging, malus/attack visuals) were not
  performed for the same reason.
- `python3 -c "import yaml"` is unavailable, so the workflow was confirmed by a
  single YAML implementation (Ruby Psych) rather than two.
- `SDL_GAMEPAD_BUTTON_COUNT` was read from the host's Homebrew SDL 3.4.10 header,
  not the pinned 3.4.4 submodule, when BUG-036 was confirmed. The value is 26 in
  both families, but the measurement is host-derived.

### 8.3 A limitation of the audit's own conduct, disclosed

**REL-015 made the audit's server testing unsafe against the operator's real host
state, and it was found in review rather than prevented.** `fb-server` derives
its stats-file path from `$HOME` unconditionally — `stats_init()`
(`server/stats.c:82-91`) builds `<HOME>/.fb-server/stats.dat` with no flag, no
cwd-relative fallback and no override other than `HOME` itself. Unlike
`joiners.log`, which is opened relative to the working directory and is therefore
isolated by `chdir()`, this path ignores the working directory entirely.

Task 10's `run_case.sh` `cd`s into a per-case scratch directory before every
launch but **never set `HOME`**, so all **24** `fb-server` instances it started
read from and wrote to the operator's real `/Users/dchau/.fb-server/stats.dat`.
Confirmed forensically: that file's mtime matches the last matrix row's server log
to the second, and it contains nick-prefixed entries only Task 10's own
nick-generation scheme produces.

**This is disclosed as a limitation of the audit, and separately registered as a
defect of the product (REL-015, Medium)** — the product defect is that no
relocation mechanism exists, which is what made the audit's isolation impossible
to achieve.

Other conduct notes, for completeness: Task 6's first isolation probe resolved to
the user's real preference directory before `CFFIXED_USER_HOME` was applied — the
harness gate aborted before opening a file, and all three files were verified
byte-identical afterwards. BUG-033 was deliberately **not** reproduced, because
doing so kills processes on this host.

### 8.4 Residual risks

1. **The highest-yield residual risk in this audit is the class of observation a
   notebook concedes inline and then sets aside without ever opening it as a
   candidate.** The audit's completion condition is "every *candidate* is
   confirmed or dismissed" — an observation that never becomes a candidate is not
   covered by it. A bounded language sweep over notebooks 01-08 found **6** such
   passages: five were sound, and one (`02-network-client-sync.md:153`) concealed
   **BUG-052, a High-severity defect**. A language sweep cannot be complete over
   roughly 5,000 lines of notebook prose, so this stands as the residual risk with
   the highest demonstrated yield.
2. **BUG-052 is unreproduced** and is the one finding in this audit with no
   runtime evidence of its own.
3. **Two upheld findings' final-challenge re-derivations are asserted rather than
   evidenced.** BUG-001 and BUG-015 have no one-line verification entry in the
   final-challenge notebook and no compensating limitation entry (§6.3(d)). Both
   retain reproductions from their originating gates, so the underlying findings
   are not in doubt; what is missing is the independent re-derivation.
4. **Four explicit dismissals in notebook 08 fall outside the final challenge's
   bullet-based dismissal count** (§6.3(b)), so their challenged status is
   unevidenced.
5. **BUG-019 and BUG-025's numeric reproductions were accepted, not re-run**, in
   the final challenge — the simultaneous-loss ordering and the 75 px tunnelling
   geometry come from Task 5's production-object harness logs.
6. **REL-010 retains an unverified premise**: certbot's ECDSA-by-default
   behaviour is taken from documentation, and the destructive `openssl req -x509`
   branch was never executed against a real certificate pair.
7. **BUG-049's `stats_record_win` dangling-pointer consequence is a
   code-supported argument, not a reproduction.** No attempt was made to steer
   the freed allocation.
8. **Coverage is measured over the file inventory, not over behaviour.** 237/237
   files have a disposition; that is a statement about review breadth, not a
   guarantee that every defect in those files was found.

**This report does not claim the absence of undiscovered defects.** It reports
what was examined, by what method, with what evidence, and what could not be
examined.

---

## 9. Recommended remediation order

Severity is the primary key, but severity alone would flatten a distinction this
audit measured and should not lose: **two findings at the same severity can differ
by orders of magnitude in how likely they are to be reached.** The clearest case
is SEC-005 and BUG-052, both High:

- **SEC-005** is reachable by **any single LAN UDP packet**, against a listener
  that **both** documented launch paths enable. Every documented deployment of
  this project is exposed today.
- **BUG-052** needs roughly **165 concurrent lobby connections** — a lobby
  population this project has very likely never had — or a deliberately
  over-long server line. Its consequence is severe and permanent for the process
  lifetime, but its practical likelihood on this project's actual deployments is
  low.

Both are High. They are not equally urgent. The ordering below is therefore
severity-first, then practical likelihood, then blast radius.

### Tier 1 — fix before any release that includes the server

| Order | ID | Sev | Why first |
|---|---|---|---|
| 1 | **BUG-049** | High | Ordinary trigger, reproduced 11×, **silent on the shipping build**, and it blocks measuring BUG-005 |
| 2 | **SEC-005** | High | One LAN packet; live in **every** documented deployment |
| 3 | **SEC-004** | High | Seat identity unbound on the verbatim relay path; enables impersonation inside any room |
| 4 | **SEC-003**, **SEC-007** | High | Untrusted peer values reach unchecked indexing; both are small, well-localized clamps |
| 5 | **BUG-003**, **BUG-007** | High | Server-wide `exit(EXIT_FAILURE)` and unbounded blocking output |

### Tier 2 — fix before the next client release

| Order | ID | Sev | Why |
|---|---|---|---|
| 6 | **BUG-026** | High | Hangs startup before any window appears — the worst possible failure presentation |
| 7 | **BUG-032**, **BUG-034** | High | Abort on ordinary bad local files / uninitialized members on early-return paths |
| 8 | **BUG-020** | High | Stale malus indexing an inactive cleared board after a player-count shrink |
| 9 | **SEC-002**, **SEC-001** | High | Both are `!quiet`/`-u`-gated and **not** reachable in this project's documented deployments — real, but lower practical urgency than the rest of Tier 1/2. SEC-002's extended impact is unresolved |
| 10 | **BUG-052** | High | Severe and permanent when reached, but needs ~165 concurrent lobby clients. Fix alongside BUG-013, which shares the reset gap |
| 11 | **REL-003** | High | Windows client blocks on its per-frame receive; **unverified on Windows** |

### Tier 3 — release-pipeline integrity

Fix as a group: **REL-004** and **REL-007** together (two independent reasons a
shipped APK cannot upgrade another), then **REL-013**, **REL-011**, **REL-012**,
**REL-010**, **REL-005**, **REL-006**, **REL-014**, **REL-002**, **REL-015**,
**REL-008**. **REL-009** last, but do not skip it — stale documentation misled
this audit's own Task 8.

### Tier 4 — gameplay, lifetime and platform Mediums

The 32 remaining Medium BUG entries. Suggested grouping by shared root cause:

- **Lifetime/ownership:** BUG-001, BUG-041, BUG-042, BUG-043, BUG-044, BUG-045
  (all cluster on `TextureEx`/`TTFText` ownership and unchecked asset loads).
- **Round and match state:** BUG-018, BUG-019, BUG-020, BUG-021, BUG-022,
  BUG-023, BUG-024, BUG-025.
- **Persistence and input:** BUG-027, BUG-028, BUG-030, BUG-033, BUG-035,
  BUG-036.
- **Protocol and lobby:** BUG-002, BUG-004, BUG-005, BUG-006, BUG-011, BUG-013,
  BUG-014, BUG-015, BUG-017, BUG-037, BUG-040, SEC-006.
- **Platform:** BUG-046, BUG-048.

### Tier 5 — Lows, then improvements

The 13 Low defects, then the improvement roadmap in §5.

### A note on sequencing the improvements

**IMP-016 should land early, not last.** Every defect above is currently
unguarded by any automated check — compilation is the entire CI gate, and five
CTest tests sit registered and unrun. Landing IMP-016 first means every
subsequent fix arrives with a regression test that actually runs, and IMP-017
through IMP-021 are written specifically to assert the defects in Tiers 1-4.
**IMP-021 must run on Linux**, because Apple ASan cannot detect leaks and the
leak findings (BUG-001, BUG-041, BUG-042) cannot otherwise be verified.

### Before remediation begins

This report is the deliverable of a **read-only** audit. Production source is
unchanged at the pinned commit, and **no remediation has been started**. The
choice of what to fix, in what order, and on what branch is the user's to make
after reading this.
