# Google Play store listing assets

## Ready to use

- **`feature-graphic.png`** (1024×500) — generated from the game's actual
  logo (`share/gfx/menu/fblogo.png`) and bubble sprites (`share/gfx/balls/`),
  composited on a gradient echoing the title screen's two glass panels.
  Meets Play's feature graphic spec exactly.
- **`screenshot-1-gameplay.png`** … **`screenshot-8-title-menu.png`**
  (24-bit RGB, no alpha; 960×720 except the 640×480 round-stats shot) —
  Play's phone/tablet screenshots, in upload order. Each is a copy of an
  image in [`docs/screenshots/`](../screenshots) (the ones the
  README and the website use), converted to PNG with the alpha flattened.
  Most come from `tests/modern_theme_render_test.cpp`'s `FB_DUMP_DIR`
  dumps, so regenerate them from there rather than from device captures.
  Landscape is fine: Play only needs 2–8 images, 320–3840px a side, no
  more extreme than 2:1. Replaced 2026-10-05 (v2.4.141); the set before
  that predated the modern theme and menus.
  1. `gameplay`: a 1-player level in the modern theme, mid-drop.
  2. `how-to-play`: the How to play page (loaded, next and pocket bubbles).
  3. `two-player-win`: a 2-player round won, its stats above.
  4. `game-room`: an online game room in the modern menu style.
  5. `round-stats`: the post-round stats table with team totals.
  6. `level-cleared`: the level-cleared card with the time bonus.
  7. `local-4player`: four players on one machine, original theme.
  8. `title-menu`: the title screen.

- **`icon-512.png`** (512×512, 24-bit RGB) — Play's high-res store icon. A
  resize of `share/icons/frozen-bubble-icon-1024x1024.png`, the single
  master every platform's app icon now derives from — the loaded red candy
  bubble in the launcher, aimed with a dashed line at the reds in a cluster
  hanging from an ice ceiling, drawn at full size by `tools/app-icon.html`
  (render steps in its header comment). It replaced a crop of two
  original-theme penguins in v2.4.144.
  Regenerate every derived icon at once, this file included, with
  `python3 tools/make-app-icons.py` (see its docstring for the full list
  of outputs, including the `android/app/src/main/res/mipmap-hdpi/
  ic_launcher.png` fallback used by local/Android-Studio builds that skip
  CI's own icon-generation step).


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
