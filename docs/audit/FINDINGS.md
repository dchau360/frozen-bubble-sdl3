# SDL3 Audit Finding Registry

This registry assigns stable, never-recycled IDs. A suspicion is not a confirmed defect: candidates move from `suspected` to `investigating`, then to either `confirmed` or `dismissed`. Dismissed candidates remain documented in the relevant subsystem notebook.

## Stable ID classes

- `BUG-###`: functional or reliability defect.
- `SEC-###`: security or untrusted-input defect.
- `REL-###`: build, packaging, release, deployment, or portability defect.
- `IMP-###`: suggested improvement, kept separate from defects.

## Severity and prioritization

Defect severities follow the approved design:

- **Critical:** remotely exploitable, major security-boundary failure, or widespread unrecoverable corruption.
- **High:** crash, memory corruption, serious multiplayer desynchronization, or a shipped platform rendered unusable.
- **Medium:** incorrect gameplay, broken edge case, practically significant resource leak, or meaningful portability/release defect.
- **Low:** limited incorrect behavior, weak diagnostics, or minor robustness issue.

Improvements are ranked separately by expected benefit, implementation effort, and regression risk. They are not assigned defect severity without defect evidence. The `Severity/Priority` cell for an improvement records that evidence-backed priority profile.

## Registry

| ID | State | Severity/Priority | Confidence | Gate | Summary | Evidence |
|---|---|---|---|---|---|---|

No findings are registered during bootstrap.
