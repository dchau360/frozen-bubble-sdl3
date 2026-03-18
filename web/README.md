# WebAssembly Port of Frozen Bubble SDL2

This port allows Frozen Bubble to run in any modern web browser using WebAssembly.

## Build Requirements

1. **Emscripten SDK** (emsdk) - Install from https://emscripten.org/docs/getting_started/downloads.html

```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

2. **SDL2 Emscripten ports** - Auto-downloaded by Emscripten during build

## Building

```bash
# Navigate to project root
cd /Users/dericchau/ai/fb2-port/frozen-bubble-sdl2

# Create build directory
mkdir build-wasm && cd build-wasm

# Configure with Emscripten CMake
emcmake cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Emscripten.cmake ..

# Build
emmake make

# Or build with optimizations
emmake make -j4
```

## Output

The build produces:
- `frozen-bubble-sdl2.html` - HTML wrapper page
- `frozen-bubble-sdl2.js` - JavaScript glue code
- `frozen-bubble-sdl2.wasm` - WebAssembly binary with game logic

## Running

You need a local web server to serve the files (browsers require HTTP for WebAssembly):

```bash
# Python 3
python3 -m http.server 8080

# Or Node.js
npx http-server -p 8080
```

Then open: `http://localhost:8080/frozen-bubble-sdl2.html`

## Multiplayer (Network Play)

Browsers cannot use raw TCP sockets. The WebAssembly port uses WebSocket to communicate with the game server.

### WebSocket Proxy Setup

You need `websockify` to bridge WebSocket to TCP for the fb-server:

```bash
# Install websockify
pip3 install websockify

# Start websockify (bridges ws://localhost:1511 to tcp://localhost:1512)
websockify --web 1511 localhost:1512

# In another terminal, start fb-server
./build/server/fb-server -p 1512 -l
```

Then connect to `ws://localhost:1511` from the game.

### Production Deployment

For production, you would:
1. Run fb-server on a VPS
2. Run websockify in front of it
3. Use wss:// (WebSocket Secure) for HTTPS deployments

## Limitations

- **No local server hosting**: The "Host a server" menu option is disabled in WebAssembly builds since browsers cannot fork processes
- **WebSocket-only networking**: Requires websockify proxy to communicate with TCP servers
- **Asset preload time**: All game assets (~50MB) are downloaded upfront as part of the WASM bundle

## Browser Support

- Chrome 57+
- Firefox 52+
- Safari 11+
- Edge 16+

All modern browsers with WebAssembly support should work.

## Controls

Same as desktop version:
- Arrow keys: Move player
- Space/Enter: Shoot bubble / Confirm
- Escape: Back / Pause

## Troubleshooting

### "Failed to load frozen-bubble-sdl2.wasm"
Make sure you're serving via HTTP server, not opening .html file directly.

### "WebSocket connection failed"
Ensure websockify is running and bridging to the correct port.

### Game loads but shows black screen
Check browser console for errors. May be missing assets or WebGL issue.

## Development

To rebuild quickly during development:

```bash
cd build-wasm
emmake make -j4
```

For debug builds with symbols:

```bash
emcmake cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake/Emscripten.cmake ..
emmake make
```
