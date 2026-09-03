#!/usr/bin/env python3
"""Verify that packaged Android assets exactly match the source share tree."""

from __future__ import annotations

import argparse
import pathlib
import zipfile
from hashlib import sha256


def source_hashes(root: pathlib.Path) -> dict[str, str]:
    return {
        path.relative_to(root).as_posix(): sha256(path.read_bytes()).hexdigest()
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }


# assets/ is not ours alone: AGP writes the baseline profile here too, as
# dexopt/baseline.prof and .profm. Those arrive the moment any dependency
# brings in androidx.profileinstaller -- androidx.fragment does -- and they
# are wanted, since they carry the AOT hints that speed up a cold start.
# They are build-tool output rather than game assets, so this check has no
# source-tree copy to compare them against and skips them by name. The
# prefix is deliberately narrow: the point of this script is to catch the
# asset-deployment pipeline packaging the wrong thing, and widening this
# would blunt that.
BUILD_TOOL_ASSET_PREFIXES = ("dexopt/",)


def apk_hashes(apk: pathlib.Path) -> dict[str, str]:
    with zipfile.ZipFile(apk) as archive:
        return {
            name: sha256(archive.read(info)).hexdigest()
            for info in archive.infolist()
            if info.filename.startswith("assets/") and not info.is_dir()
            and not (name := info.filename.removeprefix("assets/")).startswith(
                BUILD_TOOL_ASSET_PREFIXES)
        }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify APK assets against a source share directory."
    )
    parser.add_argument("--apk", required=True, type=pathlib.Path)
    parser.add_argument("--source", required=True, type=pathlib.Path)
    arguments = parser.parse_args()

    source = source_hashes(arguments.source)
    packaged = apk_hashes(arguments.apk)
    missing = sorted(source.keys() - packaged.keys())
    unexpected = sorted(packaged.keys() - source.keys())
    mismatched = sorted(
        path for path in source.keys() & packaged.keys() if source[path] != packaged[path]
    )

    if missing or unexpected or mismatched:
        if missing:
            print("Missing packaged assets:")
            for path in missing:
                print(f"  {path}")
        if unexpected:
            print("Unexpected packaged assets:")
            for path in unexpected:
                print(f"  {path}")
        if mismatched:
            print("Mismatched packaged assets:")
            for path in mismatched:
                print(f"  {path}")
        return 1

    print(f"APK assets match source: {len(source)} files with matching SHA-256 hashes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
