#!/usr/bin/env python3
"""Regenerate every platform's app icon from one master source.

  share/icons/frozen-bubble-icon-1024x1024.png

is the single source of truth: a 1024x1024, fully opaque RGB PNG. Every
other icon in the tree is a resize of it, written by this script:

  - ios/AppIcon.xcassets/AppIcon.appiconset/AppIcon-1024.png (iOS: re-saved
    at the same 1024x1024 -- actool compiles this into Assets.car at build
    time, see tools/ios-appicon.sh)
  - share/icons/frozen-bubble-icon-512x512.png (Android: CI's "Generate app
    icons from source PNG" step resizes this down further for every mipmap
    density; also Play Store's high-res store listing icon, kept identical
    per docs/store-assets/README.md)
  - docs/store-assets/icon-512.png (same 512, see above)
  - android/app/src/main/res/mipmap-hdpi/ic_launcher.png (a checked-in
    fallback for local/Android-Studio builds that skip the CI step above)
  - share/icons/frozen-bubble-icon-64x64.png (Linux: AppImage's
    hicolor/64x64/apps icon, see .github/workflows/build.yml)
  - share/icons/frozen-bubble.ico (Windows: compiled into the .exe via
    share/icons/fb.rc, sizes 16/32/48/64)
  - share/gfx/pinguins/window_icon_penguin.bmp (macOS/Linux/Windows window
    icon, loaded at runtime by SDL_SetWindowIcon() in frozenbubble.cpp --
    on macOS, absent a bundled .icns, this is also what shows in the Dock)
  - web/favicon.png, inlined into web/shell.html (WASM: browser-tab icon)

macOS's actual .app bundle icon (a .icns) is generated in CI instead of
here -- iconutil, which builds one, only exists on macOS, and the DMG job
already runs on a macOS runner. See the "Build .icns and app bundle" step
in .github/workflows/build.yml; it resizes the same 1024 master.

Requires Pillow (pip install Pillow). Run it when the master image changes:

    python3 tools/make-app-icons.py
"""

import base64
import io
import re
import shutil
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required: pip install Pillow")

ROOT = Path(__file__).resolve().parent.parent
MASTER = ROOT / "share/icons/frozen-bubble-icon-1024x1024.png"

IOS_OUT = ROOT / "ios/AppIcon.xcassets/AppIcon.appiconset/AppIcon-1024.png"

ANDROID_OUT = ROOT / "share/icons/frozen-bubble-icon-512x512.png"
STORE_OUT = ROOT / "docs/store-assets/icon-512.png"

# CI's "Generate app icons from source PNG" step (.github/workflows/build.yml)
# derives every mipmap density from ANDROID_OUT at build time. This one
# density is also checked in as a fallback for local/Android-Studio builds
# that skip that step -- keep it in sync by hand here.
ANDROID_HDPI_FALLBACK = ROOT / "android/app/src/main/res/mipmap-hdpi/ic_launcher.png"

LINUX_OUT = ROOT / "share/icons/frozen-bubble-icon-64x64.png"

ICO_OUT = ROOT / "share/icons/frozen-bubble.ico"
ICO_SIZES = [16, 32, 48, 64]

WINDOW_ICON_OUT = ROOT / "share/gfx/pinguins/window_icon_penguin.bmp"
WINDOW_ICON_SIZE = 48

FAVICON_OUT = ROOT / "web/favicon.png"
SHELL_HTML = ROOT / "web/shell.html"
FAVICON_LINK_RE = re.compile(
    r'  <link rel="icon" type="image/png" href="data:image/png;base64,[^"]*">\n'
)


def main() -> None:
    if not MASTER.exists():
        sys.exit(f"master icon not found: {MASTER}")

    art = Image.open(MASTER).convert("RGB")
    if art.size != (1024, 1024):
        sys.exit(f"master icon must be 1024x1024, got {art.width}x{art.height}")

    # iOS: re-save rather than copy, so a change to PNG optimization settings
    # here always reaches every derived file the same way.
    art.save(IOS_OUT, "PNG", optimize=True)
    print(f"wrote {IOS_OUT.relative_to(ROOT)} (1024x1024)")

    art.resize((512, 512), Image.LANCZOS).save(ANDROID_OUT)
    print(f"wrote {ANDROID_OUT.relative_to(ROOT)} (512x512)")
    shutil.copyfile(ANDROID_OUT, STORE_OUT)
    print(f"wrote {STORE_OUT.relative_to(ROOT)} (copy of the 512x512 above)")

    art.resize((72, 72), Image.LANCZOS).save(ANDROID_HDPI_FALLBACK)
    print(f"wrote {ANDROID_HDPI_FALLBACK.relative_to(ROOT)} (72x72)")

    art.resize((64, 64), Image.LANCZOS).save(LINUX_OUT)
    print(f"wrote {LINUX_OUT.relative_to(ROOT)} (64x64)")

    # Pillow's ICO encoder silently drops any requested size bigger than the
    # base image it's given, so upscale to the largest requested size first --
    # here that's a downscale from 1024, which is the opposite problem and
    # harmless.
    ico_base = art.resize((max(ICO_SIZES), max(ICO_SIZES)), Image.LANCZOS)
    ico_base.save(ICO_OUT, sizes=[(s, s) for s in ICO_SIZES])
    print(f"wrote {ICO_OUT.relative_to(ROOT)} ({', '.join(f'{s}x{s}' for s in ICO_SIZES)})")

    window_icon = art.resize((WINDOW_ICON_SIZE, WINDOW_ICON_SIZE), Image.LANCZOS)
    window_icon.save(WINDOW_ICON_OUT)
    print(f"wrote {WINDOW_ICON_OUT.relative_to(ROOT)} ({WINDOW_ICON_SIZE}x{WINDOW_ICON_SIZE})")

    # Native size, matching the window icon above -- the browser tab is too
    # small for anything larger to read differently anyway.
    window_icon.save(FAVICON_OUT)
    print(f"wrote {FAVICON_OUT.relative_to(ROOT)} ({WINDOW_ICON_SIZE}x{WINDOW_ICON_SIZE})")

    buf = io.BytesIO()
    window_icon.save(buf, format="PNG")
    b64 = base64.b64encode(buf.getvalue()).decode("ascii")
    link = f'  <link rel="icon" type="image/png" href="data:image/png;base64,{b64}">\n'

    html = SHELL_HTML.read_text(encoding="utf-8")
    if FAVICON_LINK_RE.search(html):
        html = FAVICON_LINK_RE.sub(link, html)
    else:
        html = html.replace("  <title>Frozen Bubble</title>\n",
                             "  <title>Frozen Bubble</title>\n" + link, 1)
    SHELL_HTML.write_text(html, encoding="utf-8")
    print(f"updated {SHELL_HTML.relative_to(ROOT)} (inlined favicon)")


if __name__ == "__main__":
    main()
