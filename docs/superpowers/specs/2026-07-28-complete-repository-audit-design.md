# Complete SDL3 Repository Audit Design

**Date:** 2026-07-28

**Objective:** Perform a complete, evidence-backed, read-only audit of the
maintained Frozen Bubble SDL3 project, report confirmed defects, and recommend
prioritized improvements without changing production source code.

## Scope

The audit covers all maintained project code and integration surfaces:

- Native SDL3 application initialization and platform integration
- Settings, keyboard, mouse, and controller input
- Rendering, transitions, fonts, textures, surfaces, and audio
- Gameplay state, bubble placement, targeting, malus, win/loss, and rounds
- Native and WASM network clients and multiplayer synchronization
- Lobby, room configuration, teams, spectators, and post-round flows
- The C game server, TCP protocol, WebSocket framing, and room lifecycle
- WASM and Android ports
- Build systems, tests, tools, packaging, CI, deployment, and operational docs

The original Perl implementation is a behavioral reference for intended
gameplay and protocol semantics, not an independently maintained product to
audit. Vendored SDL and iniparser internals are excluded; their use, build
configuration, and project-facing integration boundaries remain in scope.

## Audit Authority

The audit is report-first and source-read-only. It may create and update audit
documentation, temporary build directories, logs, corpora, and test artifacts.
It must not fix or refactor production code before the complete report is
delivered and follow-up work is explicitly approved.

The audit must distinguish confirmed defects from suggested improvements.
Suspicions must not appear as confirmed findings without a reproduction or a
complete causal argument supported by code and runtime evidence.

## Agent and Model Portability

The process is agent- and model-neutral. Each session records the agent, model
when known, operating environment, and relevant tool versions, but no vendor or
model is an audit gate.

Use the strongest available coding/reasoning model for deep review. Use a fresh
context or independent reviewer for the final challenge pass. In Codex, the
recommended configuration at the time this design was written is GPT-5.6 Sol
with max reasoning, with Pro mode for the final challenge pass when available.
Claude Code and other agents should use their strongest appropriate available
model instead.

## Repository Instruction Setup

Before audit execution begins, replace the standalone `AGENTS.md` file with a
relative symlink to `CLAUDE.md`:

```text
AGENTS.md -> CLAUDE.md
```

The files currently differ only in their heading and tool-specific introductory
sentence. After conversion, `CLAUDE.md` is the single authoritative repository
instruction file for both Codex and Claude Code.

## Audit Artifacts

Create tracked documents under `docs/audit/`:

- `SDL3_REVIEW_STATUS.md`: canonical resumable handoff and current state
- `subsystems/<name>.md`: evidence notebook for each subsystem
- `SDL3_COMPLETE_REVIEW.md`: final reader-facing report

Another agent must be able to resume by reading only the repository instruction
file and `docs/audit/SDL3_REVIEW_STATUS.md`.

### Status File Requirements

Update the status file after every meaningful investigation. It records:

- Audited Git commit and whether the worktree differs from that baseline
- Agent/model when known, host platform, compiler, SDK, and analysis tool versions
- Current phase, active subsystem, and exact next action
- Completed and pending subsystem gates
- Commands already run and their results or artifact paths
- Active candidate findings and their current state
- Confirmed finding IDs
- Dismissed candidates and links to the evidence notebooks
- Platform or environment limitations
- Temporary services, ports, processes, and cleanup state

### Subsystem Notebook Requirements

Each subsystem notebook records:

- In-scope files and functions
- Cross-file invariants and trust boundaries
- Static review performed
- Dynamic, sanitizer, protocol, or manual checks performed
- Candidate findings and investigation state
- Confirmed findings with evidence
- Dismissed false positives and dismissal evidence
- Coverage gaps and environmental limitations
- Gate conclusion and exact follow-up work, if any

### Stable Finding IDs

Use stable, never-recycled identifiers:

- `BUG-###`: functional or reliability defect
- `SEC-###`: security or untrusted-input defect
- `REL-###`: build, packaging, release, deployment, or portability defect
- `IMP-###`: suggested improvement

Each confirmed finding includes severity, confidence, affected platforms and
files, evidence, reproduction or causal reasoning, user impact, proposed
correction, and verification strategy.

## Coverage Standard

"Complete" means measurable repository coverage, not a claim that no unknown
bug can exist. Generate an explicit inventory of every maintained in-scope
file. Give each file one disposition:

- Fully reviewed
- Reviewed at an integration boundary
- Generated or platform-derived and validated through its source or build
- Excluded with a recorded reason
- Blocked with a recorded limitation

Candidate findings move through:

```text
suspected -> investigating -> confirmed
                           -> dismissed
```

Dismissed candidates remain in the notebook with the evidence that ruled them
out so future agents do not repeat the same work.

A subsystem gate cannot close while it has an unexplained failure, unresolved
candidate, unrecorded coverage gap, or in-scope file without a disposition.

## Audit Strategy

Use a subsystem-gated audit with risk-first ordering inside each subsystem.
This preserves interaction-aware review while making completion and handoff
objective.

### Phase 1: Baseline and Audit Setup

1. Create the `AGENTS.md` symlink and initial audit artifacts.
2. Pin the audited Git commit and record the worktree state.
3. Inventory in-scope files and available platform toolchains.
4. Record compiler, build system, SDK, and analysis tool versions.
5. Run clean native builds and the existing test suite.
6. Establish sanitizer and static-analysis builds without changing production
   behavior.

### Phase 2: Risk-First Subsystem Review

Review and close subsystem gates in this order:

1. C server, TCP/WebSocket framing, and untrusted protocol input
2. Native/WASM clients and multiplayer synchronization
3. Gameplay state, placement, targeting, malus, win/loss, and round transitions
4. Lobby, room configuration, teams, settings, and input
5. Rendering, transitions, fonts, surfaces/textures, and audio lifecycle
6. Native, WASM, Android, and filesystem/platform abstraction
7. Build, packaging, CI, deployment, tools, and operational documentation

Within a subsystem, inspect high-impact trust boundaries and state transitions
first, then complete the full file inventory before closing the gate.

### Phase 3: Dynamic and Adversarial Validation

Run applicable non-destructive checks, including:

- ASan and UBSan builds; other sanitizers where supported and informative
- Malformed, oversized, truncated, coalesced, and fragmented protocol messages
- TCP/WebSocket connection churn and room creator/member lifecycle cases
- Player-count and index boundaries from 1 through 20
- Multiple rounds, teams, spectators, targeting, and clear mode
- Missing, malformed, or unwritable settings and asset paths
- Repeated initialization, shutdown, renderer/audio recreation, and ownership checks
- Release artifact and platform configuration validation where toolchains exist

Test harnesses created only for investigation remain outside tracked production
sources unless separately approved. Record unavailable platform checks as
coverage limitations, never as passes.

### Phase 4: Cross-Platform and Behavioral Comparison

Compare native, WASM, and Android implementations for semantic drift. Use the
Perl implementation to resolve intended gameplay/protocol behavior when the C++
port is ambiguous. Document intentional differences and unexplained divergence.

### Phase 5: Independent Final Challenge

Use a fresh context or independent reviewer to:

- Revisit high-risk invariants and subsystem interactions
- Challenge every confirmed finding for false-positive risk
- Recheck severity and confidence assignments
- Search for missed consequences of shared state, ownership, and platform guards
- Verify every in-scope file has a final disposition
- Verify every command result and coverage limitation is represented accurately

Only after this challenge may the complete report be finalized.

## Severity and Prioritization

Defect severities are:

- **Critical:** remotely exploitable, major security-boundary failure, or
  widespread unrecoverable corruption
- **High:** crash, memory corruption, serious multiplayer desynchronization, or
  a shipped platform rendered unusable
- **Medium:** incorrect gameplay, broken edge case, practically significant
  resource leak, or meaningful portability/release defect
- **Low:** limited incorrect behavior, weak diagnostics, or minor robustness issue

Improvements are ranked separately by expected benefit, implementation effort,
and regression risk. They are not labeled as bugs without defect evidence.

## Final Report

`SDL3_COMPLETE_REVIEW.md` contains:

1. Executive summary and release-readiness assessment
2. Confirmed defects ordered by severity
3. Security and protocol assessment
4. Platform, build, and release assessment
5. Suggested improvements prioritized separately
6. Test and analysis commands with results
7. Complete file-coverage appendix
8. Known limitations and residual risks
9. Recommended remediation order

The report must separate observed facts, code-supported inferences, untested
risks, and recommendations. It must not imply that an unavailable platform or
scenario passed testing.

## Completion Conditions

The audit is complete only when:

- Every subsystem gate is closed
- Every in-scope file has a disposition
- Every candidate is confirmed or dismissed
- Every confirmed finding has evidence and a verification strategy
- Every improvement has benefit, effort, and regression-risk estimates
- All test results and limitations are recorded
- The independent final challenge is complete
- The status file points to the final report and has no remaining audit action

