#!/usr/bin/env python3
"""Behavioral tests for the Android APK asset-parity CLI."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile
import unittest
import zipfile


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = REPOSITORY_ROOT / "tools" / "verify_android_assets.py"


class AndroidAssetParityTest(unittest.TestCase):
    def make_source(self, root: pathlib.Path) -> pathlib.Path:
        source = root / "share"
        (source / "gfx").mkdir(parents=True)
        (source / "gfx" / "bubble.png").write_bytes(b"blue bubble")
        (source / "data").mkdir()
        (source / "data" / "levels.txt").write_bytes(b"level one\n")
        return source

    def make_apk(self, root: pathlib.Path, assets: dict[str, bytes]) -> pathlib.Path:
        apk = root / "app-debug.apk"
        with zipfile.ZipFile(apk, "w") as archive:
            for path, contents in assets.items():
                archive.writestr(f"assets/{path}", contents)
        return apk

    def run_checker(self, apk: pathlib.Path, source: pathlib.Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SCRIPT), "--apk", str(apk), "--source", str(source)],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_matching_paths_and_hashes_pass(self) -> None:
        """A correct package exits successfully."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            source = self.make_source(root)
            apk = self.make_apk(
                root,
                {"gfx/bubble.png": b"blue bubble", "data/levels.txt": b"level one\n"},
            )

            result = self.run_checker(apk, source)

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_missing_packaged_asset_fails(self) -> None:
        """Omitting a source asset reports its relative path."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            source = self.make_source(root)
            apk = self.make_apk(root, {"gfx/bubble.png": b"blue bubble"})

            result = self.run_checker(apk, source)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("data/levels.txt", result.stdout)

    def test_unexpected_packaged_asset_fails(self) -> None:
        """An asset absent from share reports its relative path."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            source = self.make_source(root)
            apk = self.make_apk(
                root,
                {
                    "gfx/bubble.png": b"blue bubble",
                    "data/levels.txt": b"level one\n",
                    "extra.txt": b"not in share",
                },
            )

            result = self.run_checker(apk, source)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("extra.txt", result.stdout)

    def test_changed_packaged_asset_fails(self) -> None:
        """Different asset bytes report the affected relative path."""
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = pathlib.Path(temporary_directory)
            source = self.make_source(root)
            apk = self.make_apk(
                root,
                {"gfx/bubble.png": b"red bubble", "data/levels.txt": b"level one\n"},
            )

            result = self.run_checker(apk, source)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("gfx/bubble.png", result.stdout)


if __name__ == "__main__":
    unittest.main()
