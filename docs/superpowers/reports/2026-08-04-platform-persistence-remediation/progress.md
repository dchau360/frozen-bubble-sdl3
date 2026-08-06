# SDD ledger — plan: docs/superpowers/plans/2026-08-04-platform-persistence-remediation.md

Branch base: ae29f0bf
Worktree: /Users/dchau/gr/frozen-bubble-sdl3/.worktrees/platform-persistence-remediation
Baseline: Debug build succeeded; CTest 11/11 registered, 9 passed and 2 expected sanitizer-only skips.
Task 1: fix round 1/5 (0 addressed, 1 open — sibling-target symlink bypass remained; commits 867b5f1..9b6f1cd)
Task 1: fix round 2/5 (1 addressed, 0 open — managed-root symlink containment; commits 9b6f1cd..d756daa)
Task 1: minor (deferred): Fix Round 2 report cites a stale source line for the RED assertion; evidence and result remain clear.
Task 1: fix round 3/5 (1 addressed, 0 open — Android debug APK link/build gate; commits d756daa..1d1c7f4)
Task 1: complete (commits ae29f0b..1d1c7f4, review clean)
Task 2: complete (commits 1d1c7f4..c05dcbb, review clean)
Task 3: minor (deferred): DevTools close rejects pending commands but event waiters can linger until their timeout.
Task 3: fix round 1/5 (1 addressed, 0 open — child launch errors now reach awaited cleanup; commits 5de4e1e..03fdccd)
Task 3: complete (commits c05dcbb..03fdccd, review clean)
Task 4: plan deviation (accepted): the brief's literal `ShowFPS = true` cannot
  match reality — `iniparser_dump_ini` lowercases keys, pads to a fixed column,
  and quotes values (`showfps                        = "true"`). The assertion
  now parses that real line via `iniHasKeyValue`; still a real file, no mock.
Task 4: RED verified with the two source fixes stashed — all three pre-dispose
  assertions failed (showfps, highlevelshistory grid, highscores CSV row).
Task 4: GREEN — persistence-save-test passes; full CTest 12 registered, 10
  passed, 2 expected sanitizer-only skips; WASM reload regression PASS against
  the rebuilt artifact carrying the real C++ flush call.
