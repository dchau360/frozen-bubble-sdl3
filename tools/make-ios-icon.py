#!/usr/bin/env python3
"""Regenerate the iOS app icon master from the game's own title art.

The output, ios/AppIcon.xcassets/AppIcon.appiconset/AppIcon-1024.png, is
committed -- building the app needs only Xcode's actool, never this script.
Run it when the source art or the framing below changes:

    python3 tools/make-ios-icon.py

Requires Pillow (pip install Pillow). Nothing else in the build does.

Why this crop: every purpose-made icon in the repo tops out at 72x72
(share/icons, android/.../ic_launcher.png), which cannot be upscaled to a
usable 1024 master. The title-screen art is the largest clean source available,
and this square is the close-up of the green penguin in its lower-right panel --
the one element that stays legible at the 60pt the home screen actually renders.
"""

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required: pip install Pillow")

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "share/gfx/menu/back_start.png"
OUT = ROOT / "ios/AppIcon.xcassets/AppIcon.appiconset/AppIcon-1024.png"

# Left, top, size in the 640x480 source. Chosen to centre the penguin's head and
# stop short of the panel's magenta border, which sits just past y=470 and reads
# as a stray pink streak once iOS rounds the corners.
CROP_LEFT, CROP_TOP, CROP_SIZE = 380, 315, 150


def main() -> None:
    if not SOURCE.exists():
        sys.exit(f"source art not found: {SOURCE}")

    art = Image.open(SOURCE)
    box = (CROP_LEFT, CROP_TOP, CROP_LEFT + CROP_SIZE, CROP_TOP + CROP_SIZE)
    icon = art.crop(box)

    # iOS icons must be fully opaque -- an alpha channel is an App Store
    # validation failure and renders unpredictably behind the corner mask. The
    # source region is already opaque (verified: alpha is 255 throughout), so
    # this drops the channel rather than compositing anything over it.
    icon = icon.convert("RGB")

    # The 1024 master is upscaled ~6.8x and is soft at full size. That is
    # accepted deliberately: iOS renders the home-screen icon at 180px and the
    # smallest variants below 60px, all of which are downscales of this crop's
    # native 150px, so what ships on screen stays close to the source pixels.
    icon = icon.resize((1024, 1024), Image.LANCZOS)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    icon.save(OUT, "PNG", optimize=True)
    print(f"wrote {OUT.relative_to(ROOT)} ({OUT.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
