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
- **`screenshot-4-local-2player.png`**, **`screenshot-5-local-4player.png`**,
  **`screenshot-6-net-5player.png`** (800×600, 24-bit RGB) — gameplay
  captures taken on a real Android tablet with `adb screenrecord`/`screencap`,
  cropped to the game viewport (the app renders 4:3 letterboxed inside the
  device's portrait screen, so the status and navigation bars are cropped
  away). The 5-player shot is a live network game against four headless
  `tools/net_bots.py` clients, captured after the long-nickname roster fix in
  `37e0237c` — an earlier capture of the same scene showed a phantom sixth
  player and is not the one checked in here.

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


## Promo video

Play takes a promo video as a **YouTube URL**, not an uploaded file, so the
recording is deliberately not checked in — it would add ~10 MB to the repo for
something Play never reads from here. Regenerate it with the same rig used for
the screenshots:

1. `./build/server/fb-server -p 15111 -z` on the host.
2. `adb reverse tcp:1511 tcp:15111` so the device's `127.0.0.1:1511` (the
   client's default manual-entry host and port) reaches that server — this
   avoids typing an IP into the on-screen field, whose port entry ignores
   `KEYCODE_DEL`.
3. Create a room on the device, then
   `python3 tools/net_bots.py --host 127.0.0.1 --port 15111 --join <nick> --count 4 --fire-interval 2.5 --fire-jitter 0.4`.
   `--join` takes the room creator's nickname **as the server sees it** — the
   server truncates nicknames to 10 characters (`MAX_NICK_LENGTH`), so
   `android_user` must be passed as `android_us`.
4. `adb shell screenrecord --time-limit 75 --bit-rate 8000000 /sdcard/fb5p.mp4`,
   start the game, and play.
5. Crop and letterbox for YouTube:
   `ffmpeg -ss 11 -i raw.mp4 -vf "crop=800:600:0:319,scale=1440:1080:flags=lanczos,pad=1920:1080:240:0:black" -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -an out.mp4`
