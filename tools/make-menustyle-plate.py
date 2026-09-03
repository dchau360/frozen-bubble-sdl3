#!/usr/bin/env python3
"""
Generates the Classic theme's plate art for the MENU STYLE row.

The other seven title-screen rows ship as pre-rendered PNGs with their label
baked in (share/gfx/menu/txt_<name>_{off,over}.png). MENU STYLE cannot: its
label names the theme currently selected, so it changes at runtime. What the
Classic theme needs from disk is therefore an *empty* plate -- correct border,
correct vertical gradient, no text -- which MenuButton then draws the live
label onto.

Rather than redraw that gradient by hand and hope it matches, this clones it
out of an existing plate: the columns under the animated icon (x 159..199) are
untouched background, so one of them tiled across the middle reproduces the
gradient exactly, while the original's first and last columns keep the border
pixels intact.

Usage:  python3 tools/make-menustyle-plate.py     (needs Pillow)
"""

import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required: pip3 install Pillow")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MENU = os.path.join(ROOT, "share", "gfx", "menu")

# Column to clone the gradient from. Anything in 165..190 is plate background
# in every one of the source plates; 175 sits comfortably clear of both the
# label on the left and the border on the right.
SAMPLE_X = 175
# Border columns to preserve verbatim from the source plate.
EDGE_L, EDGE_R = 4, 4


def build(src_name, dst_name):
    src = os.path.join(MENU, src_name)
    dst = os.path.join(MENU, dst_name)
    im = Image.open(src).convert("RGBA")
    w, h = im.size
    px = im.load()

    out = Image.new("RGBA", (w, h))
    op = out.load()

    for y in range(h):
        fill = px[SAMPLE_X, y]
        for x in range(w):
            if x < EDGE_L or x >= w - EDGE_R:
                op[x, y] = px[x, y]      # keep the real border
            else:
                op[x, y] = fill          # tile the sampled gradient
    out.save(dst)
    print(f"{dst_name}: {w}x{h} from {src_name}")


if __name__ == "__main__":
    build("txt_graphics_off.png", "txt_menustyle_off.png")
    build("txt_graphics_over.png", "txt_menustyle_over.png")
