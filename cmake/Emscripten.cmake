# Emscripten CMake toolchain file for Frozen Bubble SDL3 WebAssembly port
# Usage: mkdir build-wasm && cd build-wasm && emcmake cmake .. && make

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_VERSION 1)

# Tell CMake to use Emscripten's toolchain
set(CMAKE_C_COMPILER "emcc")
set(CMAKE_CXX_COMPILER "em++")

# CMake generator settings
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Emscripten-specific flags
set(EMSCRIPTEN_FLAGS "--bind -s USE_SDL=3 -s USE_SDL_IMAGE=3 -s USE_SDL_MIXER=3 -s USE_SDL_TTF=3")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s SDL3_IMAGE_FORMATS=['png']")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s SDL3_MIXER_FORMATS=['ogg']")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s WASM=1")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s ENVIRONMENT='web'")
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s TOTAL_MEMORY=268435456")

# Asset preloading - all game assets bundled into virtual filesystem
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} --preload-file ${CMAKE_SOURCE_DIR}/share@/share")

# Exported functions for JavaScript interop
set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} -s EXPORTED_RUNTIME_METHODS=['cwrap','ccall','setValue','getValue','UTF8ToString','stringToUTF8']")

# Shell file for custom HTML template (optional)
# set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS} --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EMSCRIPTEN_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EMSCRIPTEN_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EMSCRIPTEN_FLAGS}")
