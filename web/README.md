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

## Building WASM Locally

See the [Building WASM locally](../README.md#building-wasm-locally) section in the main README.

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

## Limitations

- **No local server hosting** — browsers cannot fork processes; use a remote server
- **Asset preload** — all game assets are bundled into the WASM package and download on first load
