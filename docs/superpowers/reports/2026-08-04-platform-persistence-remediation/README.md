# Platform persistence remediation — development record

Working record for
[the plan](../../plans/2026-08-04-platform-persistence-remediation.md) and
[the design](../../specs/2026-08-04-platform-persistence-design.md), closing
BUG-046 (Android asset extraction) and BUG-048 (WASM persistence).

Preserved from the `.worktrees/platform-persistence-remediation` worktree, which
held the only copy. These files are a historical record of how the work went —
they are not maintained, and where they disagree with the code, the code wins.

## What is here

- `progress.md` — the task ledger, including deferred minors.
- `task-N-brief.md` / `task-N-report.md` — the brief given for each task and the
  report back.
- `review-<sha>..<sha>.diff` — the exact review package for each fix round. Every
  SHA is on `main`, so these are reproducible with `git diff <sha>..<sha>`; they
  are kept because they record *what was reviewed at the time*.

## Where the record is incomplete

The ledger and reports cover Tasks 1–3 only. They stop mid-effort, and reading
them alone would understate what shipped:

- **Task 4** has a brief but no report. It was completed as `27927781`.
- **Tasks 5 and 6** have no brief or report. Both were completed directly,
  without the subagent workflow that produced the earlier artifacts —
  Task 5 as `8cf21f8a`, Task 6 as `f50c8f07`.
- The **final review gate** was run inline rather than by a dispatched reviewer.

## What shipped after the recorded tasks

| Commit | What |
|---|---|
| `27927781` | Task 4 — persist every settings and highscore mutation eagerly |
| `eabe89a1` | Save settings and highscores atomically |
| `885f46a4` | Save only the highscore table that changed |
| `e1fd6cd8` | Stop highscore saves inventing levels; fix the `Dispose()` leaks |
| `8cf21f8a` | Task 5 — run the Android and WASM persistence gates in CI |
| `f50c8f07` | Task 6 — close the findings in the audit ledger |
| `4ab0ad70` | Free the remaining singletons on shutdown |
| `d7165494` | Fail fast when the DevTools socket closes (Task 3 deferred minor) |

Two things deliberately not done, both recorded rather than silently dropped:

- **No fsync anywhere.** Saves are atomic — a crash cannot truncate a file —
  but not durable against power loss. Declined on the game side because macOS
  `F_FULLFSYNC` measured 8.09ms per save against 0.13ms for plain `fsync`,
  enough to hitch a 16.7ms frame; declined on the Android side as well. The
  Android marker is therefore written without one, so a power loss between
  extraction and the marker write can leave the marker pointing at an
  incompletely written asset tree.
- **No Android device or emulator run.** The runtime path has never executed.
  The procedure is recorded, unchecked, in
  [MANUAL_TEST_CHECKLIST.md](../../../MANUAL_TEST_CHECKLIST.md).
