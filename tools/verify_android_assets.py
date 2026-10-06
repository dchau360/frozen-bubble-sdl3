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


# assets/ is not ours alone. AGP writes the baseline profile here
# (dexopt/baseline.prof and .profm, from androidx.profileinstaller), and the
# ad SDKs ship their own files (Mintegral's template/ and rv_binddatas.xml,
# BidMachine's bm_networks/, IAB's ad-viewer/, DT Exchange's fyb_*.html,
# InMobi's ia_*.txt, ...). None of those have a copy in share/ to compare
# against, and the list changes with every SDK update. So the check covers
# the game's own part of assets/: every path under one of share/'s top-level
# entries (data/, gfx/, icons/, locale/, snd/) must match exactly, in both
# directions, and anything outside them is reported but not failed. Packaging
# the wrong tree still fails, as missing files; a stray file inside a game
# directory still fails, as unexpected.


def apk_hashes(apk: pathlib.Path) -> dict[str, str]:
    with zipfile.ZipFile(apk) as archive:
        return {
            info.filename.removeprefix("assets/"): sha256(archive.read(info)).hexdigest()
            for info in archive.infolist()
            if info.filename.startswith("assets/") and not info.is_dir()
        }


def is_game_path(path: str, top_level: set[str]) -> bool:
    return path.split("/", 1)[0] in top_level


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify APK assets against a source share directory."
    )
    parser.add_argument("--apk", required=True, type=pathlib.Path)
    parser.add_argument("--source", required=True, type=pathlib.Path)
    arguments = parser.parse_args()

    source = source_hashes(arguments.source)
    top_level = {path.name for path in arguments.source.iterdir()}
    all_packaged = apk_hashes(arguments.apk)
    packaged = {p: h for p, h in all_packaged.items() if is_game_path(p, top_level)}
    others = sorted(all_packaged.keys() - packaged.keys())
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
    if others:
        print(f"Not checked, outside {sorted(top_level)} (library assets): {len(others)} files")
        for path in others:
            print(f"  {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
