# Google Play store listing assets

## Ready to use

- **`feature-graphic.png`** (1024×500) — generated from the game's actual
  logo (`share/gfx/menu/fblogo.png`) and bubble sprites (`share/gfx/balls/`),
  composited on a gradient echoing the title screen's two glass panels.
  Meets Play's feature graphic spec exactly.
- **`play/1-last-board-standing.png`** … **`play/8-touch-pad-keyboard.png`**
  (1920×1080, 16:9) — Play's phone screenshots, in upload order, uploaded
  2026-10-25 with the rename to Frozen Bubble: Battle Royale. Each is a
  caption over one of [`docs/screenshots/`](../screenshots)'s images on a
  navy frame, made by `python3 tools/play-screenshots.py` (captions and
  sources are listed at its top); at 1080px tall they stay eligible for
  Play's promotions. Shot 1's source, `local-5player.png`, is a frame of a
  five-bot local game rendered headless with candy bubbles (the old
  `net-5player.png` still has the classic ones).
  1. Last board standing wins — five boards, one already frozen out.
  2. Online battles, up to 20 players — an online game room.
  3. Pop groups, send attack bubbles — a 1-player level, "3 DROPPED!".
  4. Team up or go solo — Set Teams.
  5. Swap: save a bubble for later — How to play.
  6. 100 classic solo levels — level cleared, with the time bonus.
  7. Climb the weekly rankings — the weekly rankings screen (test data).
  8. Play with touch, pad or keyboard — four players on one machine.

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
