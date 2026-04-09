# WebAssembly Port Implementation Guide

## Overview

This document describes the WebAssembly port of Frozen Bubble SDL3, which allows the game to run in any modern web browser.

> **Note:** The WASM build currently uses SDL2 Emscripten ports, not SDL3.
> Emscripten 5.x ships SDL3 and SDL3_ttf ports but not SDL3_image or SDL3_mixer yet.
> Tracking: [emscripten-core/emscripten#26571](https://github.com/emscripten-core/emscripten/pull/26571) (SDL3_mixer, approved)
> and [emscripten-core/emscripten#24634](https://github.com/emscripten-core/emscripten/pull/24634) (SDL3_image, stalled).
> When both land, this build can be migrated to SDL3.

## Architecture

### Key Differences from Native Builds

1. **No Raw TCP Sockets**: Browsers don't allow raw TCP connections. The port uses WebSocket instead.
2. **No Process Forking**: Cannot host a local server - must connect to external servers.
3. **Virtual Filesystem**: Assets are preloaded into Emscripten's virtual filesystem.
4. **Async Everything**: Network operations are asynchronous due to JavaScript integration.

## Files Modified/Created

### New Files

| File | Purpose |
|------|---------|
| `cmake/Emscripten.cmake` | CMake toolchain file for Emscripten |
| `CMakeListsEmscripten.txt` | Alternative CMakeLists for WASM builds |
| `src/networkclient_wasm.cpp` | WebSocket-based network client |
| `web/index.html` | HTML wrapper page |
| `web/README.md` | User-facing documentation |

### Modified Files

| File | Changes |
|------|---------|
| `src/platform.h` | Added `__WASM_PORT__` handling in ASSET() macro |
| `src/platform.cpp` | Added WebAssembly data dir init (`/share`) |
| `src/mainmenu.cpp` | Disabled server hosting on WASM |
| `src/networkclient.h` | Conditional socket members, WebSocket handle |

## Build Instructions

### Prerequisites

```bash
# Install Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### Build Steps

```bash
cd /Users/dericchau/ai/fb2-port/frozen-bubble-sdl3

# Create build directory
mkdir build-wasm && cd build-wasm

# Configure with Emscripten
emcmake cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Emscripten.cmake ..

# Build
emmake make -j4
```

### Output

- `frozen-bubble-sdl3.html` - Main HTML page
- `frozen-bubble-sdl3.js` - JavaScript glue code
- `frozen-bubble-sdl3.wasm` - WebAssembly binary (~50MB with assets)

## Running Locally

```bash
# Serve via HTTP (required for WebAssembly)
python3 -m http.server 8080

# Open in browser
open http://localhost:8080/frozen-bubble-sdl3.html
```

## Multiplayer Networking

### WebSocket Proxy Required

The game uses the fb-server protocol over WebSocket. Since browsers can't do raw TCP:

```bash
# Install websockify
pip3 install websockify

# Bridge WebSocket (1511) to TCP (1512)
websockify --web 1511 localhost:1512

# Start fb-server on TCP port
./build/server/fb-server -p 1512 -l
```

Players connect to `ws://localhost:1511` from the browser.

### Production Deployment

1. Run `fb-server` on a VPS
2. Run `websockify` in front with TLS termination
3. Use `wss://` (secure WebSocket) for HTTPS

## Platform-Specific Code

### Conditional Compilation

```cpp
#ifdef __WASM_PORT__
    // WebAssembly-specific code
    // - WebSocket networking
    // - Virtual filesystem paths
    // - No fork()/popen()
#else
    // Native code
    // - TCP sockets
    // - Local server hosting
    // - POSIX APIs
#endif
```

### Define Location

`__WASM_PORT__` is defined in:
- `CMakeListsEmscripten.txt` via compiler flags
- Set automatically when using the Emscripten toolchain

## Asset Handling

Assets are preloaded via Emscripten's `--preload-file`:

```
--preload-file share@/share
```

This mounts the `share/` directory at `/share` in the virtual filesystem.

The `ASSET()` macro handles path resolution:

```cpp
// platform.h
inline std::string ASSET(const char* relpath) {
#ifdef __ANDROID__
    if (relpath && relpath[0] == '/') relpath++;
    return std::string(relpath);
#elif defined(__WASM_PORT__)
    return std::string(relpath);  // Already starts with /
#else
    return g_dataDir + relpath;
#endif
}
```

## Network Client Implementation

### Native (TCP)
```cpp
// networkclient.cpp
int sockfd;  // POSIX socket
connect(sockfd, ...);
send(sockfd, ...);
recv(sockfd, ...);
```

### WebAssembly (WebSocket)
```cpp
// networkclient_wasm.cpp
void* websocketSocket;  // WebSocket handle
emscripten_websocket_new(url, ...);
emscripten_websocket_send(ws, data, len);
// Receives via callback: onWebSocketMessage()
```

### Message Queue Pattern

Both implementations use the same message queue interface:

```cpp
// Sending
client->SendCommand("NICK player1");
client->SendGameData("g 1 2 3");

// Receiving (game loop)
while (client->HasMessage()) {
    std::string msg = client->GetNextMessage();
    client->ParseMessage(msg.c_str());
}
```

## Limitations

| Feature | Status | Notes |
|---------|--------|-------|
| Single Player | ✅ Works | Full game support |
| Local Multiplayer | ✅ Works | 2-player on same keyboard |
| LAN Discovery | ❌ Disabled | UDP not available in browsers |
| Public Server List | ⚠️ TODO | Needs emscripten_fetch |
| Network Play | ⚠️ Requires Proxy | websockify needed for TCP↔WebSocket |
| Server Hosting | ❌ Disabled | No fork() in browsers |
| Level Editor | ✅ Works | Can create/edit levels |
| High Scores | ✅ Works | Local storage TODO |

## Browser Support

| Browser | Minimum Version |
|---------|-----------------|
| Chrome | 57+ |
| Firefox | 52+ |
| Safari | 11+ |
| Edge | 16+ |

## Troubleshooting

### "Failed to load .wasm file"
- Must serve via HTTP, not `file://` protocol
- Use `python3 -m http.server 8080`

### Black screen / no assets
- Check browser console for 404 errors
- Verify build completed successfully
- Try clearing browser cache

### WebSocket connection refused
- Ensure websockify is running
- Check that fb-server is listening on the TCP port
- Verify no firewall blocking

### Memory errors
- Increase `TOTAL_MEMORY` in CMake toolchain
- Default is 256MB, can go up to 512MB

## Future Improvements

1. **High Score Persistence**: Use IndexedDB or localStorage
2. **Touch Controls**: Add on-screen buttons for mobile
3. **Public Server Fetch**: Implement emscripten_fetch for server list
4. **PWA Support**: Add service worker for offline play
5. **WebGL Optimizations**: Better texture management
6. **Audio Worklet**: Lower latency sound playback

## Performance Notes

- Initial load: ~5-10 seconds (asset download)
- Runtime: 60 FPS on modern devices
- Memory: ~200MB typical usage
- WASM binary: ~2MB (code) + ~50MB (assets)

## Security Considerations

- No sensitive data stored locally
- WebSocket connections should use wss:// in production
- CORS headers needed for asset loading from CDN
- Consider Content Security Policy for deployment
