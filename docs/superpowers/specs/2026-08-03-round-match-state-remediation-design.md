# Round and Match State Remediation Design

**Date:** 2026-08-03

**Scope:** BUG-018, BUG-019, BUG-021, BUG-022, BUG-023, BUG-024, and
BUG-025 from `docs/audit/FINDINGS.md`.

## Goal

Make round completion, match completion, player departure, chain targeting,
and high-delta collision behavior deterministic across local and network
multiplayer, while preserving the existing protocol and the original Perl
gameplay rules.

## Current defects and root causes

The round-state defects share one structural cause: winner resolution and win
accounting are implemented independently in `HandlePlayerLoss`,
`CheckGameState`, the network `F` handler, and the network `l` handler. Those
paths do not apply the same team, victories-limit, departure, or idempotency
rules.

- BUG-018: `SetupSettings::clearMode` is propagated but multiplayer gameplay
  never reads it. `CheckGameState` treats every empty multiplayer board as a
  win.
- BUG-019: danger-zone sweeps call `HandlePlayerLoss` and resolve the round
  after each player. The first of two simultaneous final losses can therefore
  receive a win before the second loss turns the round into a draw.
- BUG-021: `CONTINUEGAMEWHENPLAYERSLEAVE` stops at the menu/network model
  because `SetupSettings` has no corresponding field. The network `l` handler
  also has its own partial winner implementation, omitting team and match-end
  rules.
- BUG-022: one reservation traversal in `AssignChainReactions` hard-codes
  normal row parity, and the port omits the Perl implementation's final
  cross-chain validity pass.
- BUG-023: the victories-limit control lives in an unreachable legacy
  two-player panel. The active local multiplayer setup does not propagate a
  limit, and the local two-player loss path does not maintain canonical
  `winCount` or `gameMatchOver` state.
- BUG-024: remote stick placement is deferred until after the network queue is
  drained. A replicated board can therefore finish before or after its `F`
  message, and the two paths do not perform the same accounting.
- BUG-025: a launched bubble advances by one unbroken `5 * deltaScale` step and
  collision checks only the endpoint. At the native maximum the 75-pixel step
  can pass through the 26.24-pixel collision radius of an occupied bubble.

## Design

### 1. Central round-outcome resolution

`BubbleGame` will gain a single private, idempotent outcome path used by local
losses, network losses, clear wins, remote `F` messages, and departures. The
path will separate three operations that are currently interleaved:

1. mutate a player's lifecycle (`ALIVE` to `LOST` or `LEFT`);
2. inspect the complete player/team state and decide whether play continues,
   a player/team won, or the round is a draw;
3. commit one outcome, including winner flags, animations, score counters,
   victories-limit enforcement, panel state, and any required `F` send.

The outcome commit is idempotent. A second observation of the same winner does
not increment counters again. If a remote `F` commits a winner before its
deferred stick clears the replicated board, the later clear observation may
add the clear-win presentation metadata, but cannot add another victory.
Conflicting candidates discovered by one batched danger sweep resolve as a
draw before the outcome is committed, so there is no transient credit to undo.

For Team Mode, every still-alive member of the winning team receives
`mpWinner` and one canonical `winCount` increment. The existing two-position
HUD counters remain presentation state: player 0 increments `winsP1`, and any
other representative winner increments `winsP2` once per round.

Single-player campaign and multiplayer-training outcomes remain on their
existing paths. The shared multiplayer resolver applies when `playerCount >=
2`.

### 2. Batched danger-zone loss detection

Both danger sweeps currently resolve after each loss. They will instead:

1. collect every `ALIVE` player whose board is in danger;
2. apply each loss's lifecycle, animation, kill attribution, targeting, and
   view-ranking side effects;
3. call the shared outcome resolver once after the batch.

This makes two final losses observed in one sweep a draw before any win is
credited. `HandlePlayerLoss` will no longer independently award a winner.

The duplicate sweep sites in `CheckGameState` and the render loop will call the
same batch helper so they cannot drift.

### 3. Mode-aware board clearing

For a multiplayer board, `allClear()` will end the round only when
`currentSettings.clearMode` is true. Classic and Team Mode continue until the
normal last-player or last-team condition is met.

In Clear Mode, clearing any local or replicated board submits the same winner
to the shared resolver. Only the locally owned network board sends `F`; a
replicated remote board performs local accounting without echoing another
notification. This makes `s`-then-`F`, `F`-then-`s`, and both-in-one-frame
orders produce identical counters, match limits, and clear-win presentation.

### 4. Departure and continuation semantics

`SetupSettings` will gain `continueWhenPlayersLeave`, defaulting to `true` to
match the current lobby default. Network game setup will copy the negotiated
menu value into it.

The `l` handler will only identify the departing array, mark it `LEFT`, update
connection/target/view state, and invoke the shared outcome resolver. It will
not contain its own winner accounting.

Departure semantics will follow the Perl contract:

- A departure does not automatically stop an otherwise undecided round.
- If the departure or a later loss leaves one player or one team alive, that
  player/team wins the round.
- When continuation is disabled and anyone has left, the round winner is
  displayed but the departed round does not add to victory counters; the match
  ends after that round.
- When continuation is enabled, a valid survivor win is credited normally and
  the victories limit is enforced.
- Even with continuation enabled, a new round cannot start with fewer than two
  connected opponents, or, in Team Mode, fewer than two connected teams. That
  condition ends the match after the current round.

`LEFT` remains preserved by `ReloadGame`, as it is today.

### 5. Active local victories-limit control

The active Local Multiplayer panel will add a victories-limit row using the
same values as network play:

`unlimited, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 20, 30, 50, 100`.

The new row participates in keyboard/controller navigation for all supported
local player counts. `SetupNewGame(7)` copies the selected value into
`SetupSettings::victoriesLimit`. Because all local multiplayer outcomes use
the shared resolver, two-player and three/four-player matches enforce it the
same way.

The unreachable `showing2PPanel`, `TPPanelRender`, `TwoPPanelKey`, legacy
two-player fields, and `SetupNewGame(2)` path will be removed. This avoids two
independent local setup screens and makes the active panel the only owner of
local match settings.

### 6. Chain-target parity and validity

Every chain traversal will use `GridNeighborOffsets(row, oddswap)`, with
`oddswap` derived once from the board orientation. The hard-coded normal-parity
branch in the chained-group reservation BFS will be removed.

After provisional targets are assigned, a final pass will mirror the Perl
cross-chain check. For each assigned falling bubble, it will:

1. derive the same-color board group connected through its virtual destination;
2. derive the groups consumed by every other provisional chain;
3. recompute root reachability with those other groups removed;
4. retain the chain only if at least two bubbles from its intended target group
   remain attached.

Cancelled chains clear their destination/reservation state and resume ordinary
falling behavior. The check will use board positions rather than pointer
identity so it remains stable while `singleBubbles` is iterated.

### 7. Bounded launch movement

Launched-bubble movement will be divided into bounded substeps no larger than
the active bubble's collision radius (`bubbleSize * 0.82`). Each substep will
reuse the current wall reflection, ceiling, occupied-bubble collision, stick,
and network-send behavior. The sum of substeps remains exactly the original
`5 * deltaScale` frame displacement, so configured game speed is unchanged.

Collision handling stops the remaining substeps at the first ceiling or board
contact. `oldPosX`/`oldPosY` will describe the immediately preceding substep so
`GetClosestFreeCell` continues to use a contact-local midpoint. Mini-board
bubbles use their smaller radius automatically.

The aim guide will use equivalent bounded sampling so its predicted wall and
collision path stays consistent with the real shot.

## Testing strategy

The audit's temporary gameplay harness is no longer present, so its production
object coverage will be recreated as a permanent CTest target,
`bubblegame-rules-test`, in `tests/bubblegame_rules_test.cpp`. The test will
exercise production `BubbleGame`, `BubbleArray`, and `SingleBubble` behavior
through a test-translation-unit visibility seam; it will not copy gameplay
algorithms into test helpers.

Regression cases will cover:

- BUG-018: the same empty multiplayer board wins in Clear Mode and does not
  finish Classic or Team Mode;
- BUG-019: two final danger losses in one batch produce a draw with no
  `winCount`, `winsP1`, or `winsP2` increment;
- BUG-021: departures with continuation on/off, one surviving team, insufficient
  connected opponents/teams, and a departure win reaching the victories limit;
- BUG-022: flipped-grid adjacency reserves the complete target group, and a
  provisional chain invalidated by another chain is cancelled;
- BUG-023: local two-player and four-player wins increment canonical counters
  and end the match exactly at the configured limit;
- BUG-024: `s` before `F`, `F` before `s`, and both queued together produce the
  same single remote victory and clear-win metadata;
- BUG-025: the recorded maximum-delta vertical shot attaches beside the row-2
  target instead of reaching the ceiling, for normal and mini geometry;
- preservation cases: ordinary single losses, team-shared wins, duplicate `F`
  notifications, unlimited matches, and three consecutive rounds.

Each implementation change follows test-driven development: add the narrow
failing regression, run it against the current code to confirm the expected
failure, make the minimal production change, and rerun both the focused test
and the full suite. The final gate is a Debug build and CTest run; an
ASan+UBSan build runs the new target and the existing sanitizer-dependent
server tests. Manual play verifies local panel navigation, winner presentation,
and representative Classic, Clear, Team, departure, draw, and maximum-speed
matches.

## Documentation and ledger updates

After verification:

- update `docs/audit/REMEDIATION_STATUS.md` counts, fixed IDs, and suggested
  next order;
- record any visual or multi-client cases not executed in
  `docs/MANUAL_TEST_CHECKLIST.md` instead of implying they were verified;
- update `CHANGELOG.md` with the user-visible round/match corrections.

## Constraints

- Do not change the line-based network protocol or require synchronized client
  upgrades.
- Preserve `LEFT` players across consecutive rounds.
- Preserve the existing meaning of `victoriesLimit == 0` as unlimited.
- Preserve the configured game speed and logical 640x480 geometry.
- Keep changes limited to this defect cluster and its regression coverage; do
  not fold unrelated gameplay or menu cleanup into the work.
