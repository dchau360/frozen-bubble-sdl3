# WebAssembly Port of Frozen Bubble (SDL3)

Frozen Bubble runs in any modern browser via WebAssembly, including on **iPhone, Android, and desktop**. No plugins or installs required.

## Touch Controls (iPhone / Mobile)

| Gesture | Action |
|---|---|
| Tap menu button | Select / activate |
| Swipe up / down | Navigate menu |
| Swipe left | Go back (Escape) |
| Tap | Fire bubble (in-game) |
| Drag / slide | Aim (in-game, Mouse/Touch mode) |

## Multiplayer / Network Play

The WASM build connects natively over **WebSockets** — no websockify proxy needed. The server must support WebSocket connections (fb-server does this out of the box).

The default public server `fb.servequake.com:1511` appears pre-selected in the Net Game menu.

> **Fairness note:** Mouse/Touch aim is easier than keyboard-only controls. The host can toggle Mouse/Touch mode in the game room settings, which syncs to all connected players.

## Emscripten port status

The WASM build uses SDL3 via Emscripten ports. Stable Emscripten releases
include SDL3 and SDL3_ttf, but **SDL3_image and SDL3_mixer come from pending
PRs** that have not been merged yet:

| Port | PR | Status |
|---|---|---|
| SDL3_mixer | [emscripten-core/emscripten#26571](https://github.com/emscripten-core/emscripten/pull/26571) | Approved, supports WAV/OGG/MP3 |
| SDL3_image | [emscripten-core/emscripten#24634](https://github.com/emscripten-core/emscripten/pull/24634) | Draft, supports PNG/GIF/JPG |

CI patches its Emscripten SDK with the port files bundled in `tools/ports/`
before building, and local builds need the same step — see
[docs/BUILDING.md](../docs/BUILDING.md#webassembly). Once both PRs land in a
stable release, the patching goes away and `emcmake cmake` works out of the box.

## Building WASM Locally

See [WebAssembly](../docs/BUILDING.md#webassembly) in the build guide.

Output files land in `dist-wasm/`:
- `frozen-bubble-sdl3.html` — HTML wrapper
- `frozen-bubble-sdl3.js` — JS glue code
- `frozen-bubble-sdl3.wasm` — WebAssembly binary

## Serving Locally

COOP/COEP headers are required for audio (SharedArrayBuffer):

```bash
python3 -c "
import http.server
class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy','same-origin')
        self.send_header('Cross-Origin-Embedder-Policy','require-corp')
        super().end_headers()
http.server.HTTPServer(('',8080),H).serve_forever()
" &
open http://localhost:8080/frozen-bubble-sdl3.html
```

## Browser Support

Any browser with WebAssembly and WebSocket support:
- Chrome 57+, Firefox 52+, Safari 11+, Edge 16+
- Mobile Safari (iPhone iOS 11+), Chrome for Android

## Saved Data

Settings, key bindings, level history, and high scores are stored in IndexedDB
and survive a page reload.

What that scope actually means:

- **Per browser profile.** Saves made in one browser are not visible in another,
  and a different profile on the same machine is a different store.
- **Per exact origin.** Scheme, host, and port all count. A game served from
  `https://example.com` and one from `https://www.example.com`, or from a
  different port, keep separate saves. Moving the deployment to a new origin
  leaves the old saves behind.
- **Not synced between devices.** There is no account and no server-side save.
- **Private windows and blocked storage fall back to defaults.** If the browser
  denies persistent storage — a private/incognito window, a quota of zero, or
  site data disabled — the game still runs, but saves last only until the tab is
  closed, and a diagnostic is written to the browser console.

Clearing site data for the origin deletes these saves, as it does for any other
site.

## Limitations

- **No local server hosting** — browsers cannot fork processes; use a remote server
- **Asset preload** — all game assets are bundled into the WASM package and download on first load
