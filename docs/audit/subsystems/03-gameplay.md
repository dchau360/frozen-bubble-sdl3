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
entering draw state (BUG-019). The first harness remains supplemental evidence
for maximum configured delta, page visibility, flipped-grid neighbor semantics,
and the expected batched/departure rules.

## Candidates

All Task 5 candidates are resolved. BUG-018 through BUG-024 are confirmed;
full-board overwrite without a prior missed loss and remote malformed placement
are not separate ordinary candidates.

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
- **IMP-005, IMP-006, IMP-009:** initialization, numeric-intent, and dead/control
  cleanup are confirmed improvements; no analyzer-only gameplay defect was
  inferred. IMP-008 remains a selective low-priority cleanup family.

## Dismissed candidates

- Active gameplay construction-before-use was proved for the Task 5 slice of
  IMP-005; analyzer warnings alone do not prove an uninitialized read.
- `size_t`-to-`int`, integer-division, and int/float pixel diagnostics operate on
  the bounded board/canvas domains; no overflow or precision-caused rule failure
  was found. They remain explicit-intent work under IMP-006.
- A completely occupied board can exhaust nearest-free search, but ordinary
  state has already crossed row 12 and stopped input. No independent reachable
  overwrite was established.
- High configured delta cannot traverse the narrowest board twice in one frame;
  one rebound remains sufficient. Local aim bounds are applied before launch.
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
  not render, play sound, drive collision animation, invoke `NewGame`/`ReloadGame`
  end to end, or create real multi-client timing. Complete source traces cover
  the promoted reset, disconnect, chain, local-option, and message-order defects.
- The sanitizer host cannot run LeakSanitizer. Leak-enabled failures and the
  accepted leak-disabled ASan+UBSan rerun are both recorded.
- Player-count and option origin validation belongs to Task 6. Task 10 should
  consume the ordinary clear, simultaneous-loss, disconnect, distinct-match,
  and three-round scenarios after remediation.

## Gate conclusion

Complete. Every scoped file and candidate has a disposition, seven gameplay
defects are confirmed with complete causal traces, existing helper tests and
both the pure-helper oracle and production-object boundary harness are recorded,
production code is unchanged, and security runtime work remains explicitly
omitted. The exact next gate is Task 6, Step 1: map menu and room state
transitions.
