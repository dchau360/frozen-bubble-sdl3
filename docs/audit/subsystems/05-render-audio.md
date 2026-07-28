# 05 — Rendering and Audio Audit Notebook

## Scope

Task 7: rendering, transitions, fonts, surfaces/textures, audio, and repeated SDL resource lifecycle.

## Trust boundaries and invariants

Pending Task 7 investigation.

## Static review

Not started.

## Dynamic evidence

Task 2 baseline only: native Release and retained ASan+UBSan test suites passed.
No interactive transition/render path was exercised.

## Candidates

- **BUG-001 — `TextureEx` failure ordering and startup leaks (suspected
  Medium):** `LoadFromSurface` reads `img->w`/`img->h` before its null check and
  calls SDL operations on a possibly null destination. `LoadEmptyAndApply`
  similarly continues after failed surface/image creation and allocates two
  `SDL_Rect` objects with `new` for each `SDL_BlitSurface` call without an
  owner. `MainMenu` invokes that method three times during setup, establishing
  six small leaked rectangles per menu construction. Task 7 must exercise
  missing/corrupt assets and confirm SDL ownership behavior.
- **IMP-007 — silent `TTFText` copy assignment (suspected improvement):** the
  operator returns `*this` without copying any resource or value member.
  Cppcheck reports three skipped members and clang-tidy reports unhandled
  self-assignment. No call site currently uses assignment; deleting the
  operator or implementing explicit ownership would make the invariant clear.
- **IMP-010 — allocation/load failure policy (suspected improvement):** raw
  precalculation allocations in `shaderstuff.cpp:1221-1226` are dereferenced
  without checks. This overlaps BUG-001's SDL resource failure path but is kept
  as the broader consistency improvement, not a second defect claim.

## Confirmed findings

None.

## Dismissed candidates

- Cppcheck's two dangling-temporary errors at
  `mainmenu_netpanel.cpp:1000,1081` are false positives: each conditional
  `std::string` temporary is bound to a block-scoped `const std::string&`, which
  extends the temporary's lifetime through the subsequent `snprintf`.
- Clang's uninitialized alpha report at `shaderstuff.cpp:764` does not model
  `SDL_GetRGBA`, which fills every `a_[i]` element in the immediately preceding
  4x4 loop. It is dismissed pending Task 7's independent bounds review.

## Coverage

Pending; see [FILE_COVERAGE.md](../FILE_COVERAGE.md).

## Limitations

Not yet assessed.

## Gate conclusion

Open — Task 7 has not started.
