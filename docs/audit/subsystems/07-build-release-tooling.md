# 07 — Build, Release, and Tooling Audit Notebook

## Scope

Task 9: builds, tests, packaging, CI, deployment, tools, dependencies, and operational documentation.

## Trust boundaries and invariants

Pending Task 9 investigation.

## Static review

Not started.

## Dynamic evidence

Task 2 cross-gate baseline evidence only; Task 9's static build/release review
has not started.

- The clean native Release configure/build completed. CTest reported all five
  registered tests passing, but REL-002 later invalidated ownership of the
  server-list test's fixed-port daemon. The other four tests passed again in a
  fresh isolated CTest run; the unchanged server-list assertions passed in a
  supplemental dynamic-port/foreground run that verified the Release child was
  still alive after readiness. The
  warnings-strict result and server candidates are recorded in
  [01-server-protocol.md](01-server-protocol.md).
- The ASan+UBSan Debug configure/build completed. Apple ASan aborted all three
  instrumented C++ tests when the required `detect_leaks=1` option reported
  `detect_leaks is not supported on this platform`. CTest marked both Python
  rows Passed, but only net-bots is accepted; server-list retains REL-002's
  process-ownership limitation.
  The retained-coverage rerun with `detect_leaks=0`, `halt_on_error=1`, and
  UBSan stack traces initially reported 5/5 tests passing, subject to REL-002.
  Fresh verification passed the four unaffected tests plus the isolated
  foreground server-list assertions against the sanitizer binary, with no ASan
  or UBSan diagnostic.
- The sanitizer build also emitted two macOS `sprintf` deprecation warnings at
  `third_party/iniparser/iniparser.c:333,874`. The configure explicitly selected
  this bundled dependency because no system iniparser was found. These are
  classified as vendored dependency noise for Task 9 boundary/version review,
  not project-owned code candidates.
- The compile-database configure succeeded. Cppcheck 2.21.0 checked 43 compile
  commands and exited 0 as configured, writing 2,160 lines / 147,118 bytes to
  `/tmp/fb-sdl3-audit/cppcheck.txt` (SHA-256
  `0eb042110d1cd1576a14abb4135cb679426136db99fe3bd22368d6f159ee6eae`).
  Deduplication found 507 diagnostics: 496 project-owned across 34 check IDs
  and 11 vendored iniparser diagnostics.
- The brief's exact clang-tidy helper command exited 1 because keg-only
  `clang-tidy` was absent from `PATH`; its 86-byte log is retained as
  `clang-tidy-initial.txt`. An explicit-binary retry exited 1 because LLVM 22
  enables no checks without a repository configuration; its 45-byte log is
  `clang-tidy-no-checks.txt`. A broad-check retry then reached project code but
  exited 1 because Homebrew LLVM lacked the Xcode SDK sysroot; its 448,449-byte
  partial log is `clang-tidy-no-sysroot.txt`.
- The final clang-tidy inventory invocation explicitly supplied the binary,
  check families, project header filter, and Xcode 26.5 SDK sysroot. It exited 0
  and wrote 11,670 lines / 1,057,186 bytes to
  `/tmp/fb-sdl3-audit/clang-tidy.txt` (SHA-256
  `973f9d2e6e8242695be3151d2b7aa031e9d6f3480c0088d6edfd2618f8dfd4bd`).
  Deduplication found 598 diagnostics: 547 project-owned across 38 check IDs
  and 51 vendored iniparser diagnostics.
- A deterministic single-worker replay with an absolute project header filter
  exited 0 and wrote 11,670 lines / 1,057,185 bytes to
  `/tmp/fb-sdl3-audit/clang-tidy-repro.txt` (SHA-256
  `faf6915e1bb6cfe54176d92550de24f05fae926f8d201603ff7ef64dade29beb`).
  Its raw ordering/summary byte differed, but its 598 deduplicated diagnostics
  and all 547 project-owned diagnostic records match the primary inventory
  exactly.

## Analyzer triage

Every unique project-owned diagnostic is accounted for below. Counts are
deduplicated by path, line, column, message, and check ID. Candidate IDs group
related evidence; they do not assert that every member of a checker family is a
defect.

### Cppcheck: 496 project diagnostics

| Count | Check IDs | Disposition |
|---:|---|---|
| 169 | `constParameterPointer`, `constParameterReference`, `constVariable`, `constVariablePointer`, `constVariableReference`, `passedByValue`, `returnByReference`, `useStlAlgorithm`, `variableScope`, `functionStatic`, `noExplicitConstructor` | Selective low-priority API/constness work under IMP-008; no behavior failure shown. |
| 181 | `cstyleCast`, `dangerousTypeCast`, `shadowMember`, `shadowVariable` | Selective cast/shadow cleanup under IMP-008; the analyzer supplies no invalid runtime value. |
| 1 | `suspiciousFloatingPointCast` | Numeric-intent review under IMP-006. |
| 74 | `uninitMemberVar`, `uninitMemberVarNoCtor`, `uninitMemberVarPrivate` | IMP-005. Existing constructors/setup paths and the green sanitizer suite prevent bulk defect promotion; assigned gates must prove construction-before-use. |
| 57 | `knownConditionTrueFalse`, `duplicateCondition`, `identicalConditionAfterEarlyExit`, `redundantAssignment`, `redundantCondition`, `redundantContinue`, `redundantInitialization`, `unreadVariable`, `uselessCallsSubstr` | IMP-004 covers the two server dead locals; IMP-009 covers the remaining simplification candidates. The repeated `currentGame` render blocks are independent, not a logic duplicate. |
| 6 | `danglingTemporaryLifetime`, `noOperatorEq`, `operatorEqVarError` | Two temporary reports dismissed by C++ const-reference lifetime extension; `FrozenBubble` is a non-copied singleton; three `TTFText` member reports become IMP-007. |
| 6 | `nullPointerOutOfMemory`, `nullPointerRedundantCheck` | Direct `TextureEx` null-ordering evidence becomes BUG-001; raw allocation/failure consistency becomes IMP-010. |
| 2 | `invalidPrintfArgType_sint` | REL-001. |

### clang-tidy: 547 project diagnostics

| Count | Check IDs | Disposition |
|---:|---|---|
| 27 | `clang-diagnostic-sign-compare`, `clang-diagnostic-strict-prototypes`, `clang-diagnostic-unused-parameter`, `clang-diagnostic-unused-variable` | Exact compiler-location inventory already retained as IMP-001 through IMP-004. |
| 268 | `bugprone-implicit-widening-of-multiplication-result`, `bugprone-integer-division`, `bugprone-misplaced-widening-cast`, `bugprone-narrowing-conversions` | IMP-006; most are legacy render/gameplay arithmetic, so semantic gates must distinguish intended truncation from defects. |
| 121 | `bugprone-assignment-in-if-condition`, `bugprone-branch-clone`, `bugprone-easily-swappable-parameters`, `bugprone-macro-parentheses`, `bugprone-reserved-identifier`, `bugprone-suspicious-string-compare`, `bugprone-switch-missing-default-case`, `bugprone-throwing-static-initialization`, `bugprone-unhandled-self-assignment`, `bugprone-unsafe-functions`, `bugprone-unused-return-value`, `clang-analyzer-optin.performance.Padding`, `clang-analyzer-security.insecureAPI.bzero`, `performance-enum-size`, `performance-move-const-arg`, `performance-unnecessary-value-param`, `portability-avoid-pragma-once` | IMP-007 through IMP-009 where applicable. Truth-valued string compares, bounded nonblocking `connect` calls, and supported `#pragma once` are dismissed; no raw promotion. |
| 27 | `bugprone-unchecked-string-to-number-conversion` | Fifteen peer-facing sync/game sites contribute to SEC-003; twelve server-list, geolocation, stats-file, and UI parsing sites remain validation improvements under IMP-008. |
| 16 | `bugprone-signal-handler` | BUG-002. |
| 5 | `clang-analyzer-core.CallAndMessage` | Four null-current-game paths are infeasible because the only call is guarded by `currentGame && isHost`; SDL fills all 16 alpha values in the fifth path. Dismissed with source evidence. |
| 1 | `clang-analyzer-cplusplus.NewDeleteLeaks` | BUG-001. |
| 10 | `clang-analyzer-deadcode.DeadStores` | IMP-004 and IMP-009. |
| 20 | `clang-analyzer-optin.core.EnumCastOutOfRange` | Dismissed: values 300+ intentionally implement the documented virtual-scancode namespace and are never sent to SDL as physical enum values. |
| 3 | `clang-analyzer-security.ArrayBound` | SEC-002. |
| 30 | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling` | Dismissed over-broad macOS Annex K suggestions; concrete bounds issues are separately retained. |
| 2 | `clang-analyzer-security.insecureAPI.UncheckedReturn` | SEC-001. |
| 13 | `bugprone-random-generator-seed`, `clang-analyzer-security.insecureAPI.rand` | Dismissed as non-cryptographic cosmetic/gameplay randomness; no security decision uses these values. |
| 1 | `clang-analyzer-security.insecureAPI.strcpy` | Dismissed: `strcat` is preceded by an exact combined-length check against the destination array. |
| 3 | `bugprone-command-processor` | Dismissed as injection reports: `system` uses a fixed literal and both `popen` paths receive compile-time URL literals. Process ownership/return handling remains eligible for Task 6/9 review. |

## Candidates

- **IMP-005:** default initialization and construction-before-use invariants.
- **IMP-006:** explicit numeric conversion and integer-division intent.
- **IMP-007:** explicit `TTFText` copy/ownership semantics.
- **IMP-008:** selective constness, cast, shadow, API, parser, and portability cleanup.
- **IMP-009:** dead-store, redundant-control-flow, and switch-default cleanup.
- **IMP-010:** consistent raw allocation and asset-load failure handling.
- **REL-002:** make `server_list_cap_test.py` own a foreground server on an
  isolated verified port so CTest cannot leak or false-pass against another
  listener.

## Confirmed findings

None.

## Dismissed candidates

Checker-family dismissals and their evidence are recorded in the analyzer tables
above. Vendored iniparser diagnostics are excluded from project-source findings
but retained for Task 9's dependency boundary review.

## Coverage

Pending; see [FILE_COVERAGE.md](../FILE_COVERAGE.md).

## Limitations

- LeakSanitizer is unavailable in Apple ASan on this macOS arm64 host. Address
  and undefined-behavior instrumentation are retained; leak coverage is absent.
- The two bundled iniparser diagnostics were classified at the dependency
  boundary only; vendored internals remain excluded from the project-owned
  source audit.
- The registered server-list test is not process/port isolated on POSIX. Task 2
  used an in-memory dynamic-port/foreground substitution with a live-child
  ownership assertion for trustworthy binary coverage and did not alter
  production or test code.

## Gate conclusion

Open — Task 9 has not started.
