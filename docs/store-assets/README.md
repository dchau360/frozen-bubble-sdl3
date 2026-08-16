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
- **`icon-512.png`** (512×512, 24-bit RGB) — Play's high-res store icon.
  Foreground is the in-game pause-screen penguin portrait
  (`share/gfx/pause_0035.png`, 190×143 native); background is a blurred
  crop of the bright blue/white glass panel from the multiplayer menu
  (`share/gfx/back_multiplayer.png`) — both pieces are real game art, not a
  generic stock image or made-up gradient. This is also now the
  **actual app launcher icon source** —
  `share/icons/frozen-bubble-icon-512x512.png` is the same image, and
  [`.github/workflows/build.yml`](../../.github/workflows/build.yml)'s
  "Generate app icons from source PNG" step now reads from it instead of
  the old 64×64 source, so every mipmap density it generates (48–192px) is
  a downscale from 512 rather than an upscale from 64. The checked-in
  `android/app/src/main/res/mipmap-hdpi/ic_launcher.png` fallback (used by
  local/Android-Studio builds that skip that CI step) was regenerated to
  match.

There's still headroom for a proper redraw or vector source at some point —
this is a crop of one in-game animation frame, not custom-made icon art —
but it's a real, presentable, on-brand icon rather than a blurry stopgap.
