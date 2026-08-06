# Task 2 report: APK asset parity

## Implementation

Added `tools/verify_android_assets.py`, a standalone Python CLI:

```text
python3 tools/verify_android_assets.py --apk <apk> --source <share-directory>
```

It builds relative-path-to-SHA-256 maps for every source file and every
non-directory APK `assets/**` entry, compares missing and unexpected path sets
before hashes on their intersection, and prints sorted diagnostic sections.
It exits zero only when both paths and hashes match.

Added `tests/android_asset_parity_test.py`.  It drives the real CLI using
synthetic ZIP/APK fixtures and covers matching, missing, unexpected, and
changed assets.

## TDD evidence

### RED

Command:

```bash
python3 tests/android_asset_parity_test.py
```

Relevant output before the production script existed:

```text
FFFF
...
can't open file '.../tools/verify_android_assets.py': [Errno 2] No such file or directory
...
Ran 4 tests in 0.053s

FAILED (failures=4)
```

The matching-case test failed with exit code 2 because the requested CLI did
not exist; the three diagnostic-path assertions also failed because no CLI
output was available.  This is the expected missing-production-script RED.

### GREEN

Command:

```bash
python3 tests/android_asset_parity_test.py
```

Output:

```text
....
----------------------------------------------------------------------
Ran 4 tests in 0.141s

OK
```

## Real debug APK result

Command:

```bash
python3 tools/verify_android_assets.py \
  --apk android/app/build/outputs/apk/debug/app-debug.apk \
  --source share
```

Output:

```text
APK assets match source: 3352 files with matching SHA-256 hashes.
```

## Files changed

- `tools/verify_android_assets.py`
- `tests/android_asset_parity_test.py`

## Commit

`c05dcbb5 test(android): verify packaged asset parity`

## Self-review

- Verified the comparator considers only non-directory APK entries under
  `assets/`, strips exactly the `assets/` prefix, and includes every source
  file under the supplied tree.
- Verified sorted missing, unexpected, and mismatched outputs for deterministic
  CI diagnostics.
- Verified the CLI through its command-line boundary rather than importing its
  comparison helpers in tests.
- Ran `git show --check --stat --oneline HEAD`; it reported no whitespace
  errors.
- Re-ran the full focused test suite and the real APK parity command after the
  commit (outputs above).

## Concerns

None.
