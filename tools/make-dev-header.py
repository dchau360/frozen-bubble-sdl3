#!/usr/bin/env python3
"""Generates the Play Console developer-profile header image.

Play's developer profile "head image" is a fixed 4096x2304 (16:9) banner --
a different slot from the per-app feature graphic (1024x500), so it needs
its own composition rather than a resize of that file.

Built the same way feature-graphic.png was: the game's actual logo
(share/gfx/menu/fblogo.png) and the app-icon master (the two-penguin icy
scene every platform icon derives from) on a gradient echoing the title
screen's two glass panels -- real game art, not stock/generated imagery.

Usage: python3 tools/make-dev-header.py
Writes: docs/store-assets/dev-header-4096x2304.png
"""

import pathlib

from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs/store-assets/dev-header-4096x2304.png"

W, H = 4096, 2304

# The title screen's two glass panels: an icy blue on the left, a warm gold
# on the right. Echoed here as the banner's background gradient.
BLUE = (74, 106, 168)
GOLD = (196, 150, 74)


def make_gradient() -> Image.Image:
    grad = Image.new("RGB", (W, 1), color=0)
    for x in range(W):
        t = x / (W - 1)
        r = round(BLUE[0] + (GOLD[0] - BLUE[0]) * t)
        g = round(BLUE[1] + (GOLD[1] - BLUE[1]) * t)
        b = round(BLUE[2] + (GOLD[2] - BLUE[2]) * t)
        grad.putpixel((x, 0), (r, g, b))
    return grad.resize((W, H))


def main() -> None:
    bg = make_gradient()

    # Barely-there vignette just to keep the flat gradient from looking
    # printed -- strong enough earlier and it muddied the blue-to-gold
    # transition into a flat gray band across the middle.
    vignette = Image.new("L", (W, H), 220)
    vd = ImageDraw.Draw(vignette)
    vd.ellipse((-W * 0.3, -H * 0.7, W * 1.3, H * 1.7), fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(500))
    dark = Image.new("RGB", (W, H), (10, 10, 20))
    bg = Image.composite(bg, dark, vignette)

    # The two-penguin icy scene, same master every platform icon derives
    # from -- placed right of center, large but not cropped, with a soft
    # shadow so it reads as a floating card rather than a pasted square.
    icon = Image.open(ROOT / "share/icons/frozen-bubble-icon-1024x1024.png").convert("RGB")
    icon_size = 1700
    icon = icon.resize((icon_size, icon_size), Image.LANCZOS)

    shadow = Image.new("L", (icon_size + 120, icon_size + 120), 0)
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle((60, 60, icon_size + 60, icon_size + 60), radius=48, fill=140)
    shadow = shadow.filter(ImageFilter.GaussianBlur(50))
    shadow_rgba = Image.new("RGBA", shadow.size, (0, 0, 0, 0))
    shadow_rgba.putalpha(shadow)

    icon_x = W - icon_size - 260
    icon_y = (H - icon_size) // 2
    bg.paste(Image.new("RGB", shadow.size, (0, 0, 0)), (icon_x - 60, icon_y - 60), shadow_rgba)
    bg.paste(icon, (icon_x, icon_y))

    # The game's real wordmark, upscaled with a light blur to soften the
    # source bitmap's native resolution rather than show jagged edges.
    logo = Image.open(ROOT / "share/gfx/menu/fblogo.png").convert("RGBA")
    scale = 6
    logo = logo.resize((logo.width * scale, logo.height * scale), Image.LANCZOS)
    logo = logo.filter(ImageFilter.GaussianBlur(1.5))
    logo_x = 260
    logo_y = (H - logo.height) // 2
    bg.paste(logo, (logo_x, logo_y), logo)

    bg.save(OUT)
    print(f"wrote {OUT} ({bg.size[0]}x{bg.size[1]}, {bg.mode})")


if __name__ == "__main__":
    main()
