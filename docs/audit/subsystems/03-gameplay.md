# 03 — Gameplay Audit Notebook

## Scope

Task 5 reviewed all gameplay implementation files named by the execution plan:
`bubblegame.cpp`, `bubblegame.h`, `bubblegame_internal.h`, the board, shooter,
level, state, input, render, and network translation units, all six
`netview`/`netteams`/`roundstats_color` helper files, and their three tests. The
behavioral comparison covered the relevant board, malus, loss, postgame, and
new-round routines in `bin/frozen-bubble` and the gameplay transport contract in
`lib/Games/FrozenBubble/Net.pm`.

Task 4's confirmed peer-placement boundary, SEC-003, is consumed as an input:
unchecked peer numeric fields can violate the valid gameplay ranges below. Task
5 did not send malformed traffic and does not duplicate that finding.

## Trust boundaries and invariants

- `SetupSettings::playerCount` is an ordinary-flow value in `[1,20]`; Task 4
  traced the room/player mapping that supplies it. `bubbleArrays` has exactly 20
  elements and every ordinary gameplay loop uses the active prefix. Validation
  of menu/config origins remains Task 6.
- Each board owns 13 row vectors. Generated and shipped layouts alternate 8 and
  7 cells; a vector element is one cell holding one `Bubble`, so ordinary state
  has no duplicate object for one coordinate. An empty cell uses `bubbleId=-1`.
- Bubble colors are clamped to 5–8 when a match starts. Local shooter angles are
  clamped to `[0.1, PI-0.1]`. Normal network angles/coordinates are assumed to
  obey the protocol; their missing receiver validation is SEC-003.
- `GridNeighborOffsets(row, oddswap)` defines hex adjacency. `oddswap=0` means
  row 0 has eight cells; `oddswap=1` means row 0 has seven. Row geometry,
  closest-cell selection, grouping, ceiling reachability, and compressor rows
  must all use the same orientation.
- `PlayerState` is `ALIVE`, `LOST`, or `LEFT`. A round may have one winner team,
  one winner player, or no winner when all final survivors die together. Classic
  and Team Mode finish on last survivor/team; only Clear Mode additionally
  finishes on the first empty board.
- Team IDs are 1–`teamCount`, with the maintained UI constraining team count to
  2–5. `AutoBalanceTeam` defensively handles the degenerate count 1; rendering
  relies on the valid 1–5 palette invariant. Task 6 owns upstream option checks.
- `malusQueue` stores award frame numbers; `malusBubbles` owns in-flight attacks;
  `nextColors` supplies the next compressor row. These are per-round transient
  state. Round stats reset at every round, match totals and wins persist until
  the next distinct `NewGame`, and a positive victories limit terminates the
  match as soon as a winner reaches it.
- With the configured native maximum speed multiplier, `deltaScale` is capped
  at 15. A 5 px shot moves at most 75 px per frame, below the narrowest mini-board
  interior of 112 px, so the single wall-reflection update cannot skip two walls.
  That wall bound does not bound collision sampling; the separate trace below
  proves BUG-025.

### Core invariant ledger

The table distinguishes maintained ordinary-flow invariants from peer-supplied
fields whose missing validation is already SEC-003. There is no
`assignedBubbles` container: the similarly named ownership fields are
`SingleBubble::assignedArray` and `MalusBubble::assignedArray`, while the owning
containers are the global `singleBubbles` and `malusBubbles` vectors.

| State | Valid range, sentinel, and units | Owner and reset owner | Writers, transitions, consumers, and adjacent finding |
|---|---|---|---|
| Board cell and position | Row `0..12`; column `0..bubbleMap[row].size()-1` (ordinary rows alternate 8/7); `bubbleId=-1` is empty, otherwise `0..numColors-1`; `Bubble::pos` is integer 640x480 logical-canvas top-left pixels derived from `bubbleOffset`, 32/16 px cell width, and 28/14 px row height. | Each `BubbleArray` owns its 13 row vectors. `NewGame`/`ReloadGame` rebuild active maps through level generation/loading; `RemoveArray` clears active maps. | Local generation and `PlacePlayerBubble` maintain the range. Peer `s`/`m` coordinates bypass it (SEC-003). BUG-020 matters here because a stale attack can retain an inactive array owner after its rows were cleared. |
| Launched/falling projectile | `SingleBubble::assignedArray` must be an active array index `[0,playerCount)`; `bubbleId` uses that array's color domain. `posX/posY` and `oldPosX/oldPosY` are float logical top-left pixels; `pos/oldpos` are their truncated integer collision/render copies. Launch angle is ordinarily `[0.1,PI-0.1]`, size is 32 or 16, and launch displacement is `5*deltaScale` px per frame. Chain destinations use row `0..12`, valid column, with `-1/-1` meaning none. | `singleBubbles` owns all free projectiles across arrays. `LaunchBubble`, destruction, and chain assignment create entries; `ReloadGame` clears them, and `NewGame` clears them after board setup. | `UpdatePosition` stores the prior sample, performs one movement/reflection, then `UpdateSingleBubbles` tests ceiling and occupied bubbles. `shouldClear` ends ownership. Peer fire angle is accepted under SEC-003. Endpoint-only collision sampling at the 75 px cap is BUG-025. |
| In-flight malus | `MalusBubble::assignedArray` must be active; `bubbleId` is `0..numColors-1`; ordinary `cx` is `0..6`, `cy=12`, and `stickY=top_of_cx[cx]+1` in `0..12`. `posX/posY` are float logical top-left pixels and `pos` is the integer copy. | `malusBubbles` is the cross-player owner; a `BubbleArray::malusQueue` owns award-frame integers until generation. `ReloadGame` clears both. | `ProcessMalusQueue` creates and sorts malus, `UpdateSingleBubbles` moves at `2.5*deltaScale` px/frame, recomputes/clamps `stickY`, places it, and erases `shouldClear`. `NewGame` does not clear these owners/counters, which is BUG-020; peer `m`/`M` fields remain SEC-003. |
| Shooter input and launch availability | `shooterLeft`, `shooterRight`, `shooterCenter`, `shooterAction`, `mouseFirePending`, `mpFirePending`, and `mpStickPending` are booleans. `newShoot=true` means no local shot blocks launch; `mouseTargetAngle=-1` is inactive, otherwise the local clamped angle; `pendingAngle` is the deferred peer angle. | Each `BubbleArray` owns its flags. `ResetRoundInputState`, called by both `NewGame` and `ReloadGame`, clears directional/action/pending flags, sets `newShoot=true`, angle sentinel `-1`, peer angle `PI/2`, peer stick fields zero, and enables release suppression. | `UpdatePenguin` samples local input/force-fire, consumes action or `mpFirePending`, calls `LaunchBubble`, then clears action/pending and sets `newShoot=false`. Collision, ceiling, chain, peer-stick, and malus landings restore `newShoot=true`; peer `f`/`s` parsing sets the pending flags. BUG-024 is the adjacent deferred-stick/order failure; SEC-003 owns unchecked peer angle/stick values. |
| Player lifecycle | `PlayerState` is exactly `ALIVE`, `LOST`, or `LEFT`; active player arrays are the prefix `[0,playerCount)`. `lobbyPlayerId=-1` means no mapped lobby seat. | Each array owns its lifecycle. `NewGame` establishes the match; `ReloadGame` restores non-`LEFT` arrays to `ALIVE` and deliberately preserves `LEFT`. | Danger checks and departure messages move players out of `ALIVE`; winner logic counts living players/teams. BUG-019 covers sequential final losses, and BUG-021 covers the separate departure resolution. |
| Target selection | `sendMalusToOne=-1` means split/random; otherwise it is an opponent array index `1..playerCount-1`. `playerTargeting[i]=-1` means no/all target; otherwise it is an active array index. `attackingMe` contains unique opponent array indices. Target messages identify players by nickname, then map back to an array/lobby ID. | `BubbleGame` owns all three. `NewGame` and `ReloadGame` set both sentinel stores and clear the vector. | Local keys call `SetSendMalusToOne`, which writes both the local target and `playerTargeting[0]`. Peer `A` records update `playerTargeting[senderIdx]` and insert/erase `attackingMe`; death/leave clears a selected target and leave erases its attacker entry. Send/render/net-view consumers validate liveness/range, but nickname identity and hostile fields remain under SEC-004/SEC-003. |
| Round/level and timing counts | `curLevel` is the configured level index, ordinarily starting at `startLevel>=1`; winner advance increments it, replay retains it. `roundsPlayed`, `frameCount`, `rFired/rPopped/rSent/rRecv/rKills`, and `m*` totals are nonnegative ordinary-play counts. `roundStatsFinalized` is a once-per-round guard. | `BubbleGame` owns level, round, and frame counts; each array owns its stats. `NewGame` resets round/match counts and the guard; `ReloadGame` resets per-round stats, frame count, and guard while preserving match totals. | Render increments `frameCount`, finalizes once after `gameFinish`, rolls `r*` into `m*`, and increments `roundsPlayed`. BUG-020 is the adjacent defect because `NewGame` omits `frameCount` and other transient resets; remote stats input remains SEC-003. |
| Victories and match termination | `winCount`, `winsP1`, and `winsP2` are nonnegative. `victoriesLimit=0` means unlimited; maintained menus otherwise select `1..12,15,20,30,50,100`. `gameMatchOver` becomes true when an enforced winner count reaches a positive limit. | Each array owns `winCount`; `BubbleGame` owns display counters, limit snapshot, and match-over flag. `NewGame` resets all wins and match-over; `ReloadGame` preserves wins but resets per-round finish/match-over flags before play resumes. | Elimination and normal `F`/local-clear paths increment counters and enforce the limit. BUG-021 omits enforcement on departure, BUG-023 neither propagates nor enforces local 2P limits/canonical `winCount`, and BUG-024 can lose remote clear accounting through stick/`F` order. |

## Static review

### Placement, collision, grouping, and compression

The midpoint collision placement, 0.82-radius collision test, ceiling-first
ordering, alternating 8/7 row generation, BFS nearest-free-cell search,
ceiling-reachability pass, group threshold of three including the activator, and
compressor cadence agree with the Perl mechanics for ordinary non-full boards.
The malus formula also agrees: popped neighbors excluding the activator plus
fallen bubbles minus two. Target selection excludes dead players and teammates;
the greater-than-five-survivor one-target rule and the five-or-fewer split use
only living opponents.

The full-board fallback in `GetClosestFreeCell` can return its occupied origin,
but an ordinarily full 13-row board has already crossed the loss line and input
is stopped. No independent ordinary defect was promoted. Peer-chosen duplicate
or out-of-range placement remains SEC-003 and was not exercised.

### Maximum-delta collision trace

The native configured maximum is exact: settings clamp `speedMultiplier` to 5,
and `RunOneFrame` clamps `deltaScale` to `mult*3`, hence 15. For launching
bubbles, `SingleBubble::UpdatePosition` stores the prior sample and performs one
unsubdivided `5*deltaScale` movement before `UpdateSingleBubbles` checks
placement. The consumer order is: update once; if `pos.y<=topLimit`, place by
the ceiling path; otherwise iterate occupied cells and call `IsCollision` on
the new point; on a hit choose the closest adjacent free cell and place. The
ceiling half-space cannot be tunneled by monotonic upward travel, but an
occupied bubble can because no swept segment or substep is tested.

The production geometry gives a deterministic ordinary-state counterexample.
On the full two-player board, use `topLimit=40` and the valid row-2/column-3
bubble at top-left `(450,96)`. A vertical 75 px maximum step takes the launched
bubble from `(450,136)` to `(450,61)`. The collision radius is
`32*0.82=26.24` px, while the old and new endpoint distances are respectively
40 and 35 px, so the actual endpoint predicate returns false even though the
segment crosses `(450,96)`. The first new point remains below the ceiling test
(`61>40`). The following equal step reaches `y=-14`, enters the ceiling branch,
and places in row 0 instead of beside the row-2 target. This proves BUG-025:
maximum native delta can tunnel a shot through an occupied bubble and change
its attachment cell.

Chain assignment is not orientation-consistent. Candidate discovery and the
free neighbor search call `GridNeighborOffsets(..., oddswap)`, but the target
group reservation BFS in `AssignChainReactions` hard-codes standard row parity.
On a flipped grid, `(row 0,col 0)` and `(row 1,col 1)` are adjacent, yet that BFS
does not reserve the second cell. A second falling bubble can consequently
target the same color group. The port also omits the Perl routine's final pass
that removes a chain whose target group falls below two still-attached bubbles
after considering the other chains. These complete source-level differences
prove BUG-022.

### Round, winner, departure, and match transitions

`CheckGameState` executes its multiplayer `allClear()` branch without reading
`currentSettings.clearMode`; in fact no gameplay file reads that option. This
makes Classic and Team Mode behave like Clear Mode even though the README and
room UI define last-player/last-team standing semantics (BUG-018).

Network clear resolution has a second ordering defect (BUG-024). A peer's `s`
message only sets `mpStickPending`; `UpdateSingleBubbles` applies it after the
network queue drains. If `s` is available one frame before the following `F`,
the replicated remote board reaches `CheckGameState`, sets `gameFinish`, and
marks the remote winner, but the local-only clear branch does not increment that
remote's `winCount` or check the victories limit. The later `F` is ignored by
its `!gameFinish` guard. If both ordered records were already queued, `F` does
the accounting first. Correct results therefore depend on ordinary TCP/frame
arrival timing.

Both danger sweeps enumerate players and call `HandlePlayerLoss` immediately.
The first of two final simultaneous losses can award a winner and increment
their counters; the loop then processes the second loss. Network play changes
the state to a draw without undoing that award, while local two-player play can
mark and credit both players as winners. The Perl reference deliberately enters
a tentative `finished` state and reconciles peer results for this exact rare
case. BUG-019 is confirmed.

The `l` message reimplements only part of winner resolution instead of using a
shared transition. It checks `livingCount==1` but not the Team Mode condition of
one surviving team, and after awarding a win it never checks `victoriesLimit`.
Additionally, the advertised `CONTINUEGAMEWHENPLAYERSLEAVE` option reaches the
lobby model but has no field in `SetupSettings`, so gameplay cannot honor the
host choice for subsequent rounds. BUG-021 covers this coherent disconnect
transition defect; Task 6 should cross-link the option-propagation origin.

The dedicated local two-player panel similarly displays and changes a victories
limit, but `SetupNewGame(2)` never copies it into `SetupSettings`. Even if a
caller did, the local two-player loss branch updates only `winsP1`/`winsP2`; it
does not increment `winCount` or set `gameMatchOver`. BUG-023 is therefore an
end-to-end inert match option, with its menu half handed to Task 6.

### Reload, reset, and construction

`ReloadGame` clears both free-bubble vectors, active malus queues, chain/score
state, frame time, alerts, per-round stats, and input flags. It preserves match
totals and wins and retains `LEFT`, matching a consecutive-round contract.
`NewGame` is incomplete by comparison: `QuitToTitle` clears board maps only,
and `NewGame` clears `singleBubbles` but not `malusBubbles`, active
`malusQueue`s, frame/chain/score/alert state, or all corresponding transient
counters. Starting a smaller match after leaving one with in-flight malus can
leave an attack assigned to an inactive array whose map was cleared. Once a new
shot drives `UpdateSingleBubbles`, that stale malus can reach
`PlacePlayerBubble` on the empty row vector and crash; same-size transitions can
receive stale attacks instead. BUG-020 is High severity because the ordinary
quit/start sequence reaches invalid indexing.

Cppcheck's uninitialized gameplay members were traced through construction.
Every active array receives geometry, shooter, penguin, texture, player ID, and
launcher values before the current-player loops read them; aggregate-created
`Bubble`, `SingleBubble`, `MalusBubble`, alert, and chat objects initialize the
fields their readers use. Thus the Task 5 slice of IMP-005 is not a current
uninitialized-read defect. Explicit default initializers remain worthwhile
maintenance work, so the improvement is confirmed rather than bulk-promoted as
a bug. Analyzer-reported size/index and pixel conversions are bounded by 13
rows, 8 cells, 20 players, and the 640x480 integer canvas; they support IMP-006
but no Task 5 numeric defect. Dead recursive attachment helpers and redundant
input/control branches confirm the gameplay slice of IMP-009. IMP-008's
constness/API/cast suggestions remain low-priority cleanup without behavior
evidence.

## Dynamic evidence

The three existing pure-helper tests passed in every usable retained lane:

| Evidence | Result |
|---|---|
| Release CTest filter `netview|netteams|roundstats` | 3/3 passed |
| Warnings-strict Debug CTest filter | 3/3 passed |
| Required ASan+UBSan command with `detect_leaks=1` | 0/3; Apple ASan aborted each test because leak detection is unsupported |
| Accepted ASan+UBSan rerun with `detect_leaks=0` | 3/3 passed; no sanitizer/UBSan diagnostic |

`/tmp/fb-sdl3-audit/task5_boundary_harness.cpp` compiles the production pure
helpers plus the actual inline grid-offset implementation with `-Werror`. Its
fixed seed is `0x5d13`. The normal and ASan+UBSan binaries both printed:

```text
task5-boundaries=PASS seed=0x5d13 players=1,2,5,6,20 teams=1..5 colors=5,8 grid=standard,flipped delta=15 rounds=3
```

That first harness is the pure-helper and maximum-delta oracle. A second
fixed-seed harness, `/tmp/fb-sdl3-audit/task5_actual_gameplay_harness.cpp`, links
the existing warnings-strict production object files unchanged and uses a
test-translation-unit-only visibility seam to invoke the actual private
`BubbleGame` methods. It runs headlessly with a null renderer and disabled
audio; it does not copy the transition implementations. The normal and
leak-disabled ASan+UBSan versions both printed:

```text
task5-actual-gameplay=PASS seed=0x5d13 players=1,2,5,6,20 teams=1..5 colors=5,8 grid=standard,flipped rounds=3 classicClear=BUG-018/clear simultaneous=BUG-019
```

The production-object harness calls `CountLivingPlayers`, `CountLivingTeams`,
`RandomLevel`, `ExpandNewLane`, `CheckGameState`, and `HandlePlayerLoss`. It
executes player counts 1, 2, 5, 6, and 20; every team count 1–5 and slot; color
endpoints 5 and 8; both alternating grid orientations; a real Clear Mode win;
three consecutive elimination rounds reaching a victories limit; and the
actual simultaneous-final-loss sequence. It also reproduces two confirmed
defects directly: the same empty board ends Classic as well as Clear Mode
(BUG-018), and the simultaneous-loss sequence retains a credited winner after
entering draw state (BUG-019). Fix Round 1 added the exact maximum-delta case
using the actual `SingleBubble::IsCollision`, `GetClosestFreeCell`, and
`BubbleArray::PlacePlayerBubble` implementations. Both normal and leak-disabled
ASan+UBSan runs proved the endpoint miss and the adjacent-collision versus
row-0-ceiling placement difference without a diagnostic, printing
`maxDeltaTunnel=BUG-025 collisionPlacement=adjacent actualPlacement=ceiling`.
The first harness remains supplemental evidence
for maximum configured delta, page visibility, flipped-grid neighbor semantics,
and the expected batched/departure rules.

## Candidates

All Task 5 candidates are resolved. BUG-018 through BUG-025 are confirmed;
full-board overwrite without a prior missed loss and remote malformed placement
are not separate ordinary candidates. Task 11 Fix Round 1 added one further
candidate from its cross-cutting "unchecked indices" sweep of `src`/`server`
(`grep -rnE '\[(team|scancode|senderId|playerId|slot|idx|index|cellIndex|button|sc)\b[^]]*\]' src server`):
`BubbleGame::LoadLevelset`'s unguarded `level[idx]` write. Confirmed and
resolved below, not left open.

## Confirmed findings

- **BUG-018 (Medium):** multiplayer board clear ignores the Clear Mode flag and
  ends Classic/Team rounds.
- **BUG-019 (Medium):** sequential final-loss handling corrupts simultaneous
  draw/winner state and counters.
- **BUG-020 (High):** distinct-match startup retains malus/transient state and
  can later index an inactive cleared board.
- **BUG-021 (Medium):** disconnect handling bypasses configured continuation,
  one-team-survivor, and victories-limit semantics.
- **BUG-022 (Medium):** chain target reservation uses the wrong flipped-grid
  parity and omits the Perl cross-chain validity pass.
- **BUG-023 (Medium):** the local two-player victories limit is displayed but
  neither propagated nor enforced.
- **BUG-024 (Medium):** remote clear-win accounting depends on whether `F`
  reaches the per-frame queue before replicated stick resolution.
- **BUG-025 (Medium):** the maximum native frame step can cross an occupied
  bubble while both sampled endpoints remain outside the collision radius, so
  the shot can attach at the ceiling or a later bubble instead.
- **IMP-005, IMP-006, IMP-009:** initialization, numeric-intent, and dead/control
  cleanup are confirmed improvements; no analyzer-only gameplay defect was
  inferred. IMP-008 remains a selective low-priority cleanup family.
- **BUG-051 (Low, Task 11 Fix Round 1):** `BubbleGame::LoadLevelset`
  (`src/bubblegame_level.cpp:38-70`) writes `level[idx]` into a fixed
  `std::array<std::vector<int>, 10>` with no check that `idx < 10` before the
  write (`:67-69`); `std::array::operator[]` does not bounds-check, so a level
  block with more than 10 non-blank lines is an out-of-bounds write. The
  sibling loader for the identical file format,
  `HighscoreManager::LoadLevelsetHighscores` (`src/highscoremanager.cpp:132-134`),
  guards the same write with `if (idx < 10)`; this one does not. Low, not
  higher, because the only caller passes the bundled `ASSET("/data/levels")`
  (`src/bubblegame.cpp:932`), a Python line-count of which confirms all 100
  level blocks have exactly 10 lines (`min == max == 10`), and no
  `SaveLevelset`/Level-Editor write path exists in the pinned source to author
  a bad block — so the shipped asset cannot trigger it today.

## Dismissed candidates

- Active gameplay construction-before-use was proved for the Task 5 slice of
  IMP-005; analyzer warnings alone do not prove an uninitialized read.
- `size_t`-to-`int`, integer-division, and int/float pixel diagnostics operate on
  the bounded board/canvas domains; no overflow or precision-caused rule failure
  was found. They remain explicit-intent work under IMP-006.
- A completely occupied board can exhaust nearest-free search, but ordinary
  state has already crossed row 12 and stopped input. No independent reachable
  overwrite was established.
- High configured delta cannot traverse the narrowest board twice in one frame,
  so one rebound remains sufficient; that bound does not prevent the separately
  confirmed endpoint-collision tunnel in BUG-025. Local aim bounds are applied
  before launch.
- Ordinary malus counts and score increments are nonnegative and bounded per
  board event. Reaching signed match-total overflow requires infeasible play;
  hostile numeric traffic belongs to SEC-003 and was not tested.

## Coverage

All 19 assigned source/helper/test rows have final dispositions in
[FILE_COVERAGE.md](../FILE_COVERAGE.md). `bubblegame_net.cpp` is now jointly
covered by Tasks 4 and 5. The Perl sources are behavioral references outside the
maintained inventory and were not treated as production owners.

## Limitations

- Per user direction, Task 5 generated no malformed or hostile peer placement,
  option, malus, stats, or winner traffic. SEC-003 is consumed as a static
  invariant; omitted security/runtime cases are not passes.
- No graphical or interactive game client was launched. The headless harness
  executes actual production gameplay objects and transition methods, but does
  not render, play sound, invoke `SingleBubble::UpdatePosition` (it would create
  the graphical/preferences-owning `FrozenBubble` singleton), drive collision
  animation, invoke `NewGame`/`ReloadGame` end to end, or create real
  multi-client timing. BUG-025 does not remain open on that omission: the exact
  production movement formula/order is statically complete and the linked
  production predicate, selector, and placement primitive reproduce its
  distinct outcomes. Complete source traces cover the other promoted defects.
- The sanitizer host cannot run LeakSanitizer. Leak-enabled failures and the
  accepted leak-disabled ASan+UBSan rerun are both recorded.
- Player-count and option origin validation belongs to Task 6. Task 10 should
  consume the ordinary clear, simultaneous-loss, disconnect, distinct-match,
  and three-round scenarios after remediation.

## Gate conclusion

Complete. Every scoped file and candidate has a disposition, nine gameplay
defects are confirmed with complete causal traces (the ninth, BUG-051, added
in Task 11 Fix Round 1 from the cross-cutting unchecked-indices sweep),
existing helper tests and both the pure-helper oracle and production-object
boundary harness are recorded, production code is unchanged, and security
runtime work remains explicitly omitted. The exact next gate is Task 6, Step 1:
map menu and room state transitions.
