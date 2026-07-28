# 02 — Network Client and Synchronization Audit Notebook

## Scope

Task 4: native/WASM network clients, message queues, protocol parsing, multiplayer identity, and synchronization.

## Trust boundaries and invariants

Pending Task 4 investigation.

## Static review

Not started.

## Dynamic evidence

Task 2 baseline only: Release and retained ASan+UBSan CTest runs passed the
registered network bot test. Task 4 protocol and synchronization investigation
has not started.

## Candidates

- **SEC-003 — unvalidated peer numeric fields reach board indexing (suspected
  High):** broad clang-tidy identified 27 unchecked string-to-number sites. A
  targeted review separates generic/local parsing from the peer-facing subset:
  `src/bubblegame_net.cpp` accepts transmitted `s`, `m`, `M`, and statistics
  fields, and `src/networkclient.cpp` accepts sync fields. In particular, the
  `M` path passes peer-controlled `stickY` and `cx` to
  `BubbleArray::PlacePlayerBubble`, which indexes `bubbleMap[row][col]` without
  a bounds check. The `s` path similarly stores coordinates later used by the
  shooter. Task 4 must reproduce malformed-message behavior and map every
  peer-facing field before confirmation.

## Confirmed findings

None.

## Dismissed candidates

None.

## Coverage

Pending; see [FILE_COVERAGE.md](../FILE_COVERAGE.md).

## Limitations

- Task 2 ran existing tests and analyzers only; it did not inject malformed
  network frames. SEC-003 therefore remains suspected despite a direct static
  index path.

## Gate conclusion

Open — Task 4 has not started.
