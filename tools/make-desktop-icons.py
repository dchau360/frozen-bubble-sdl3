#!/usr/bin/env python3
"""Regenerate the Windows/Linux/WASM icons from the macOS window icon art.

macOS already gets this look for free: SDL_SetWindowIcon() loads
share/gfx/pinguins/window_icon_penguin.bmp directly at runtime (see
src/frozenbubble.cpp), and on macOS that call also sets the Dock icon since
the app has no separate .icns/MACOSX_BUNDLE_ICON_FILE. This script carries
the same source to the other three platforms that can actually use a 48x48
image without upscaling into something blurry:

  - share/icons/frozen-bubble.ico   (Windows: compiled into the .exe via
    share/icons/fb.rc, tops out at 64x64)
  - share/icons/frozen-bubble-icon-64x64.png (Linux: AppImage's
    hicolor/64x64/apps icon, see .github/workflows/build.yml)
  - web/favicon.png (WASM: browser-tab icon for the itch.io/browser build,
    served at its native 48x48 -- no resizing needed)

iOS is deliberately NOT covered here -- it needs a 1024x1024 App Store
master, and this 48x48 source upscales far worse than the crop
tools/make-ios-icon.py already uses for that. Android similarly generates
its launcher icons from the larger share/icons/frozen-bubble-icon-512x512.png
master, not from this file.

Requires Pillow (pip install Pillow). Run it when the source art changes:

    python3 tools/make-desktop-icons.py
"""

import base64
import io
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required: pip install Pillow")

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "share/gfx/pinguins/window_icon_penguin.bmp"

ICO_OUT = ROOT / "share/icons/frozen-bubble.ico"
ICO_SIZES = [16, 32, 48, 64]  # matches the set already baked into the .ico

LINUX_OUT = ROOT / "share/icons/frozen-bubble-icon-64x64.png"

# Also kept on disk (git-tracked) as the plain-file source of truth, but the
# WASM build's packaging step (.github/workflows/build.yml "Package WASM
# output") only copies a fixed list of emcc outputs into dist-wasm/ -- a
# favicon.png dropped next to them here wouldn't ship. Inlining it as a data
# URI into web/shell.html instead means the favicon travels inside the one
# file that step already copies, with nothing else to keep in sync.
FAVICON_OUT = ROOT / "web/favicon.png"
SHELL_HTML = ROOT / "web/shell.html"
FAVICON_LINK_RE = re.compile(
    r'  <link rel="icon" type="image/png" href="data:image/png;base64,[^"]*">\n'
)


def main() -> None:
    if not SOURCE.exists():
        sys.exit(f"source art not found: {SOURCE}")

    art = Image.open(SOURCE).convert("RGB")

    # Pillow's ICO encoder silently drops any requested size bigger than the
    # base image it's given (source: 48x48), which is why an earlier pass of
    # this script produced a .ico missing 64x64 entirely. Upscale to the
    # largest requested size first so every entry in ICO_SIZES actually gets
    # written.
    ico_base = art.resize((max(ICO_SIZES), max(ICO_SIZES)), Image.LANCZOS)
    ico_base.save(ICO_OUT, sizes=[(s, s) for s in ICO_SIZES])
    print(f"wrote {ICO_OUT} ({', '.join(f'{s}x{s}' for s in ICO_SIZES)})")

    art.resize((64, 64), Image.LANCZOS).save(LINUX_OUT)
    print(f"wrote {LINUX_OUT} (64x64)")

    art.save(FAVICON_OUT)
    print(f"wrote {FAVICON_OUT} ({art.width}x{art.height}, native size)")

    buf = io.BytesIO()
    art.save(buf, format="PNG")
    b64 = base64.b64encode(buf.getvalue()).decode("ascii")
    link = f'  <link rel="icon" type="image/png" href="data:image/png;base64,{b64}">\n'

    html = SHELL_HTML.read_text(encoding="utf-8")
    if FAVICON_LINK_RE.search(html):
        html = FAVICON_LINK_RE.sub(link, html)
    else:
        html = html.replace("  <title>Frozen Bubble</title>\n",
                             "  <title>Frozen Bubble</title>\n" + link, 1)
    SHELL_HTML.write_text(html, encoding="utf-8")
    print(f"updated {SHELL_HTML} (inlined favicon)")


if __name__ == "__main__":
    main()
