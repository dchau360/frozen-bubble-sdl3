# Online tournament mode — proposed design and delivery plan

Status: proposal for review; no implementation changes. The user confirmed a single shuffle into a fixed bracket on 2026-09-11. Other recommendations below remain proposed defaults.

## Requested outcome

At least eight online players enter a tournament, play randomly assigned 1 vs 1 matches in best-of-three series, and can view the bracket and tournament status between rounds.

Terminology: a **round** is one game of Frozen Bubble; a **match** is a best-of-three series; a **stage** is a bracket column such as quarterfinals or semifinals.

## Recommended rules for the first release

- Single elimination, 8–16 human entrants. The 16-player maximum is a proposed first-release limit, not a requirement from the request.
- Registration remains open until the organizer starts with at least eight connected, ready entrants. Lock entrants and gameplay settings at start; no late entrants or substitutions.
- The server shuffles entrants once and records the resulting fixed bracket. Winners follow bracket connections; they are not shuffled again.
- For non-power-of-two counts, fill the next power-of-two bracket with evenly distributed first-stage byes. Random seeding determines who receives them. No empty-versus-empty opening matches.
- All matches are 1 vs 1, first to two wins, including the final. Draws award no win and are replayed, so a series may contain more than three games if draws occur.
- Start with one locked Classic rules preset, identical options for both seats, no teams, no bots, and two victories required. Show the complete preset before registration readiness.
- Run matches within a stage concurrently. Open the next stage when every match in the current stage resolves, providing a shared bracket break.
- Require both players to ready explicitly between games and before their next match; show a five-second countdown after both ready. Opening a bracket or stats view must not itself mark the player ready.
- Proposed ready deadline: 60 seconds after the server requests readiness. If only one player readies, the other forfeits the match; if neither readies, eliminate both and propagate the empty advancement slot. Reset readiness for every game.
- Explicit withdrawal or a detected disconnect forfeits the series in the first release. Reconnecting does not reclaim an eliminated seat. Show this rule before entering; reconnect grace and game-state restoration are separate future work.
- Before start, transfer organizer controls to the earliest remaining entrant if the organizer leaves; cancel when registration is empty. After start, progression is server-managed and survives organizer departure.
- Eliminated players and other connected users can follow bracket updates. Live viewing of another match's boards is outside the first release.
- Keep completed brackets for 30 minutes, then expire them. First release is in-memory: server restart ends tournaments, and reconnecting clients must clear stale tournament state.

## Player experience

1. Add a **Tournaments** entry to the online lobby on supporting servers. List registration, running, and recently completed tournaments with entrant counts and current stage.
2. **Create / join:** show entrant list, rules, ready status, and start eligibility. Organizer can start once all entrants are ready and the minimum is met.
3. **Bracket:** show player names, series scores, stage, and status: waiting, ready, playing, finished, bye, forfeited, or disputed. Highlight the local player's match and route through the bracket.
4. **Play:** assign the player to a reserved two-seat match room and reuse the existing gameplay screen. Display the series score, for example `1–0 · First to 2`.
5. **Between games:** retain round stats and offer **Bracket**, **Ready**, and **Withdraw**. The bracket includes live scores from other matches; returning from it preserves the player's current series.
6. **Between stages:** return to the bracket, showing who advanced and which matches are still playing. After the final, highlight the champion and final result.

Use the existing 640×480 logical canvas. Show an eight-player bracket as three columns; use stage/section navigation for larger brackets rather than shrinking text. Every action needs keyboard/gamepad navigation, visible focus, a direct click/tap target, and footer controls. Withdrawal needs an explicit confirmation with both input paths.

## Architecture and alternatives

**Recommended: tournament coordinator inside fb-server, separate from individual rooms.** It owns registration, bracket, entrants, series scores, readiness, deadlines, and advancement. Each active match uses an ordinary gameplay room restricted to its two assigned entrants. This reuses gameplay and transport while keeping tournament membership alive across room transitions.

Client-hosted orchestration would be smaller initially but makes host departure, score disagreement, and unauthorized bracket changes difficult to control. A separate tournament service would support accounts and durable history, but adds deployment and identity infrastructure the first release does not need.

### Server boundaries

- New `server/tournament.c` and `server/tournament.h`: tournament lifecycle and scheduler. Keep the bracket logic testable without live sockets, using injected time and shuffle randomness.
- `server/game.c` / `server/game.h`: narrow adapter for reserved match creation, assignment, start, and cleanup. Tournament registration must not be represented as one large ordinary room.
- `server/net.c`: deliver tournament control traffic in both lobby and in-game priority modes, and drive deadlines without blocking the network loop.
- Identify entrants independently of nicknames and file descriptors; allocate stable IDs per tournament and track their current connection. Use separate tournament, match, and round IDs.
- Restrict entrants to one tournament at a time; while actively entered, reject ordinary room creation/join and unauthorized room option changes. Bracket viewers do not occupy gameplay seats.
- Retire a completed match exactly once. Cleanup must distinguish a normal server-directed return to the bracket from withdrawal, so cleanup cannot produce a second result or forfeit.
- Bound tournament counts, viewers, command rates, message sizes, and retained results using server configuration; check capacity before locking registration and starting a stage.

### Results and readiness

The existing `F` winner message and `n` ready message are insufficient to control a tournament: results have no round ID, the current result guard resets on any `n`, and one client's next-round input readies peers automatically.

Add tournament-specific round reporting and readiness, scoped by tournament/match/round IDs. The server validates assigned sender, active round, legal winner, and phase; deduplicates repeats; and alone commits the series score and advancement. Delayed messages from earlier rounds must have no effect.

Recommended result policy: both assigned clients report the observed outcome. Matching reports commit it. Conflicting reports enter a visible disputed state and do not advance either entrant automatically; an operator resolution/cancellation command is needed for this policy. A missing report times out, with a visible failure reason and the defined disconnect/forfeit policy where applicable. The precise dispute-resolution permissions and reporting deadline should be settled before implementation.

The server remains a gameplay relay, not a full authoritative simulation. These checks protect tournament bookkeeping; they do not constitute cheat-proof gameplay. Do not reuse the lobby announcement's `players_wins` as tournament truth or let raw `F` claims advance the bracket. Tournament announcements should follow committed outcomes.

### Protocol and client state

- Add feature discovery on the existing compatible protocol before enabling tournament actions. Current clients use FB/1.3 and older servers reject a higher minor version, so a global version bump alone would regress ordinary online compatibility.
- Define bounded control commands for list, create, join, leave, ready, start, watch, snapshot, and round report. Specify responses, errors, permissions, and allowed phases in a protocol document before coding handlers.
- Send server snapshots and updates with revision numbers. A fresh snapshot must rebuild the complete bracket; ignore stale updates and request a snapshot after a detected revision gap. Apply multi-line snapshots atomically.
- Include IDs and readiness deadlines in control events. Keep bracket traffic separate from gameplay and bubble-sync queues.
- Shared protocol code lives in `src/networkclient.cpp`; `src/networkclient_wasm.cpp` already delegates shared parsing there. Verify both native TCP and WebSocket framing, including multiple control/gameplay lines arriving in one read.
- Add `src/tournamentstate.h/.cpp` for the client model and snapshot parsing, and `src/tournamentview.h/.cpp` for reusable bracket layout/drawing. `src/mainmenu_tournament.cpp` connects those components to lobby actions.
- Integrate tournament flow through `MainMenu::PumpNetworkFrame()` and existing game transitions. The tournament model must survive `BubbleGame` round resets and ordinary room cleanup.

## Delivery sequence

Each step is an independently reviewable implementation milestone. Add behavior tests before implementing its state changes; use the existing CTest and scripted-server harnesses.

### 1. Rules, protocol contract, and bracket core

- [x] Confirm a single shuffle into a fixed bracket.
- [ ] Confirm 8–16 cap, byes, stage barriers, disconnect rule, and dispute policy; freeze the Classic preset.
- [ ] Write the control protocol and state transition tables, including invalid-command responses and terminal states.
- [ ] Add the isolated server tournament model and CMake test target.
- [ ] Test start rejection at seven players; 8, 9, 12, and 16 entrant brackets; deterministic shuffle under injected randomness; balanced byes; single advancement; and champion selection.

### 2. Registration and live bracket distribution

- [ ] Add discovery, registration, watch/unwatch, start, snapshots, and revisioned updates in the server and shared client parser.
- [ ] Test duplicate joins, capacity limits, membership exclusivity, organizer departure, late joins, unsupported clients, malformed messages, and stale/partial snapshots.
- [ ] Verify a new client can still play ordinary games on an older server, and an older client can still play ordinary games on the new server.

### 3. Reserved matches and best-of-three control

- [ ] Add room adapter functions and server-driven two-player assignments; block unassigned joins and option changes.
- [ ] Add explicit round IDs, result reports, server scores, both-player readiness, and countdown/start events.
- [ ] Update `src/bubblegame_net.cpp`, `src/bubblegame_state.cpp`, `src/bubblegame_render.cpp`, `src/bubblegame_input.cpp`, and associated headers to follow tournament control events while retaining existing ordinary-game behavior.
- [ ] Test 2–0 and 2–1 series, draws, duplicate/stale reports, unassigned senders, conflicting outcomes, readiness timeouts, concurrent match isolation, and no extra game after two wins.

### 4. Tournament lobby, bracket, and intermission screens

- [ ] Add the reusable bracket view and lobby panel; integrate with `src/mainmenu.h`, `src/mainmenu.cpp`, `src/mainmenu_input.cpp`, and the panel tap handler where it is defined.
- [ ] Wire stats/bracket/ready/withdraw controls and winner/loser returns without losing tournament state.
- [ ] Test navigation and tap routing; manually inspect an eight-player and sixteen-player bracket at the native logical size and on a phone-sized display.
- [ ] Verify the user can view bracket status after every game, including the deciding game, and while waiting for other matches.

### 5. Failure handling and complete tournament integration

- [ ] Add deadline processing, withdrawal/disconnect forfeits, dispute resolution, empty-slot propagation, expiry, and resource cleanup.
- [ ] Exercise disconnects during registration, countdown, play, result reporting, and intermission; include both players leaving and connection descriptor reuse.
- [ ] Run an eight-client tournament through four quarterfinals, two semifinals, and a final; assert seven completed matches, one champion, consistent snapshots, and no orphaned rooms or entrants.
- [ ] Repeat with non-power-of-two entrants, 16 entrants, mixed native/WebSocket clients, and two independent tournaments sharing a server.

### 6. Release validation and documentation

- [ ] Register new tests in `CMakeLists.txt`; run normal CTest and the repository's full ASan/UBSan build and CTest commands from `CLAUDE.md`.
- [ ] Build native and WASM clients; manually verify browser sync, touch, keyboard, and gamepad flows. Run the existing platform CI matrix before release.
- [ ] Confirm ordinary online games, teams, lobby broadcasts, and existing Discord alerts retain their intended behavior. Tournament results must not double-post from both legacy and committed-result paths.
- [ ] Document rules and server limits in `README.md` / `SetupServer.md`, the protocol in `docs/`, and the feature in `CHANGELOG.md`.
- [ ] Deploy server support before exposing client tournament actions. Treat release/version changes as a separate authorized release step.

## Acceptance criteria

Eight players can join, receive randomized 1 vs 1 assignments, finish best-of-three series, inspect live bracket status between every game and stage, and produce exactly one champion. Both input families work, unsupported servers retain ordinary play, and duplicate results or departures cannot advance a player twice.

Deferred: double elimination, ranking-based seeding, accounts, prizes, cross-server tournaments, live board spectators, reconnect restoration, persistent history, and Discord bracket integration.
