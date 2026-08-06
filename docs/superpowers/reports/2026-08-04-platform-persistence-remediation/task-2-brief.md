### Task 2: Verify APK asset parity

**Files:**
- Create: `tools/verify_android_assets.py`
- Create: `tests/android_asset_parity_test.py`

**Interfaces:**
- Produces CLI:

```text
python3 tools/verify_android_assets.py --apk <apk> --source <share-directory>
```

- Exit `0` only when the APK's non-directory `assets/**` entries, after
  removing the `assets/` prefix, exactly match source relative paths and
  SHA-256 values. Exit non-zero with separate missing, unexpected, and
  mismatched sections otherwise.

- [ ] **Step 1: Write the failing parity-script tests**

Create synthetic ZIP/APK files with `zipfile.ZipFile` and literal contents.
Cover:

```python
def test_matching_paths_and_hashes_pass()
def test_missing_packaged_asset_fails()
def test_unexpected_packaged_asset_fails()
def test_changed_packaged_asset_fails()
```

Invoke the real CLI through `subprocess.run`; assert exit codes and the exact
relative path in each diagnostic rather than importing and duplicating its
comparison logic.

- [ ] **Step 2: Run the parity tests and verify RED**

Run:

```bash
python3 tests/android_asset_parity_test.py
```

Expected: failure because `tools/verify_android_assets.py` does not exist.

- [ ] **Step 3: Implement the path-and-hash comparison**

Use these data shapes:

```python
def source_hashes(root: pathlib.Path) -> dict[str, str]:
    return {
        path.relative_to(root).as_posix(): sha256(path.read_bytes()).hexdigest()
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }

def apk_hashes(apk: pathlib.Path) -> dict[str, str]:
    with zipfile.ZipFile(apk) as archive:
        return {
            info.filename.removeprefix("assets/"):
                sha256(archive.read(info)).hexdigest()
            for info in archive.infolist()
            if info.filename.startswith("assets/") and not info.is_dir()
        }
```

Compare key sets first, then hashes for the intersection. Print sorted paths so
CI output is deterministic. Do not ignore source files silently; Gradle's
configured source directory is the contract being checked.

- [ ] **Step 4: Run focused tests and the real debug APK parity check**

Run:

```bash
python3 tests/android_asset_parity_test.py
python3 tools/verify_android_assets.py \
  --apk android/app/build/outputs/apk/debug/app-debug.apk \
  --source share
```

Expected: four tests pass and the built APK reports matching asset count and
hashes.

- [ ] **Step 5: Commit Task 2**

```bash
git add tools/verify_android_assets.py tests/android_asset_parity_test.py
git commit -m "test(android): verify packaged asset parity"
```

---

