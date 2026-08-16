# Google Play store listing assets

## Ready to use

- **`feature-graphic.png`** (1024×500) — generated from the game's actual
  logo (`share/gfx/menu/fblogo.png`) and bubble sprites (`share/gfx/balls/`),
  composited on a gradient echoing the title screen's two glass panels.
  Meets Play's feature graphic spec exactly.
- **`screenshot-1-follow-server.png`**, **`screenshot-2-game-room.png`**,
  **`screenshot-3-round-stats.png`** (640×480, 24-bit RGB, no alpha) — real
  captures from a live build (same source as the ones in
  [`docs/screenshots/`](../screenshots), alpha-flattened for Play's
  requirements). Landscape, not portrait: the game is TV/landscape-first
  and these are honest to how it actually looks, which is fine — Play
  doesn't require portrait screenshots, just 2–8 images between 320px and
  3840px per side with an aspect ratio no more extreme than 2:1. All three
  qualify.

## Not ready — needs real source art

- **`icon-512-draft-upscaled-low-quality.png`** — Play's high-res store
  icon needs a 512×512 32-bit PNG. The only source in this repo is
  `share/icons/frozen-bubble-icon-64x64.png` (and a `.ico` that tops out at
  the same 64×64). This file is an 8x Lanczos upscale of that — visibly
  soft/blurry at real size, kept here only so the gap is visible rather
  than silently shipping a bad icon. Don't submit it as-is. It needs either
  the original higher-resolution artwork (if it exists somewhere outside
  this repo) or a redraw.
