# Building from source

Prebuilt binaries for all five platforms are on the
[releases page](https://github.com/dchau360/frozen-bubble-sdl3/releases/latest) —
you only need this if you want to modify the game or build for a platform we
don't ship.

## 1. Clone

```bash
git clone https://github.com/dchau360/frozen-bubble-sdl3.git
cd frozen-bubble-sdl3
```

## 2. Install dependencies

**macOS (Homebrew):**
```bash
brew install sdl3 sdl3_image sdl3_mixer sdl3_ttf cmake ninja
```

If SDL3_mixer isn't in Homebrew yet, build it from source:
```bash
git clone https://github.com/libsdl-org/SDL_mixer.git --branch release-3.2.0
cd SDL_mixer && cmake -B build -G Ninja && cmake --build build && sudo cmake --install build
```

**Ubuntu / Debian** — SDL3 is not in apt on most distros yet, so build it:
```bash
git clone https://github.com/libsdl-org/SDL.git --branch release-3.4.4
cmake -S SDL -B SDL/build -G Ninja && cmake --build SDL/build && sudo cmake --install SDL/build
```
Repeat for SDL3_image, SDL3_mixer and SDL3_ttf. The exact release tags CI uses
are in [`.github/workflows/build.yml`](../.github/workflows/build.yml).

**Windows (MSYS2 MinGW64):**
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-SDL3 mingw-w64-x86_64-SDL3_image \
          mingw-w64-x86_64-SDL3_mixer mingw-w64-x86_64-SDL3_ttf
```

`iniparser` is bundled in `third_party/` — no separate install needed.

## 3. Build and run

```bash
cmake -B build -G Ninja
cmake --build build --parallel
./build/frozen-bubble-sdl3
```

The server binary (`fb-server`) is built automatically alongside the game on
Linux and macOS.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Two server tests exercise memory-safety fixes and need a sanitizer build. On an
ordinary build they report as skipped rather than passing without running:

```bash
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS=-fsanitize=address,undefined \
  -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

**Apple's AddressSanitizer has no LeakSanitizer**, so a clean sanitizer run on
macOS proves nothing about leaks — the Linux CI job is the gate for those. To
check locally on macOS, use `leaks` against an ordinary build instead:

```bash
MallocStackLogging=1 leaks --atExit -- ./build/<test-binary>
```

## WebAssembly

The WASM build needs the Emscripten SDK patched with SDL3_image and SDL3_mixer
port files, because those two ports are still unmerged upstream — see
[Emscripten port status](../web/README.md#emscripten-port-status) for what is
pending and when this step goes away.

1. **Install Emscripten** (v4.0.8+):
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk && ./emsdk install latest && ./emsdk activate latest
   source emsdk_env.sh
   ```

2. **Patch the SDK** with the port files bundled in this repo:
   ```bash
   PORTS="$(dirname $(which emcc))/tools/ports"
   SETTINGS_JS="$(dirname $(which emcc))/src/settings.js"
   SETTINGS_PY="$(dirname $(which emcc))/tools/settings.py"

   cp tools/ports/sdl3_mixer.py "$PORTS/sdl3_mixer.py"
   cp tools/ports/sdl3_image.py "$PORTS/sdl3_image.py"

   # Register the new settings variables
   sed -i '/SDL2_MIXER_FORMATS/a\var SDL3_IMAGE_FORMATS = [];\nvar SDL3_MIXER_FORMATS = [];' "$SETTINGS_JS"
   sed -i "s/'SDL2_MIXER_FORMATS'/'SDL2_MIXER_FORMATS', 'SDL3_IMAGE_FORMATS', 'SDL3_MIXER_FORMATS'/" "$SETTINGS_PY"

   # Suppress the SDL3 experimental warning, which breaks -Werror
   sed -i "s/diagnostics.warning('experimental'/# diagnostics.warning('experimental'/" "$PORTS/sdl3.py"
   ```

3. **Build:**
   ```bash
   emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
   cmake --build build-wasm --parallel
   ```

4. **Serve.** Audio needs COOP/COEP headers — without them the page loads but
   stays silent. `tools/serve-wasm.py` sets them, finds `build-wasm/` or
   `dist-wasm/` on its own, and prints the URL:
   ```bash
   python3 tools/serve-wasm.py
   ```

To check that browser saves survive a reload, run the same gate CI does:

```bash
node tools/test-wasm-persistence.mjs build-wasm
```

## Android

See [`android/SETUP.md`](../android/SETUP.md). In short:

```bash
git submodule update --init --recursive android/app/jni/SDL3*
cd android && ./gradlew assembleRelease
```

Release signing uses a persistent key from repository secrets — a tagged build
fails rather than shipping an APK that cannot be upgraded over the previous
one. See [ANDROID_SIGNING.md](ANDROID_SIGNING.md).
