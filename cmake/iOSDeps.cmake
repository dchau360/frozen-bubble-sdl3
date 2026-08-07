# SDL3 and its satellite libraries, built from source for iOS.
#
# Every other platform finds these through a package manager; iOS has none, and
# the Homebrew SDL3 on a developer's Mac is macOS-only, so find_package() there
# would either fail or -- worse -- succeed and link x86_64/arm64 *macOS* dylibs
# into an iOS binary. The pinned submodules under android/app/jni are the exact
# sources the Android build already compiles, so iOS reuses them instead of
# adding a second ~1 GB copy of the SDL tree.
#
# Everything is static. A static link leaves no embedded .framework bundles in
# the .app, and every embedded framework needs its own signature -- which is
# precisely what an unsigned build cannot supply.

set(FB_SDL_DIR "${CMAKE_SOURCE_DIR}/android/app/jni")

foreach(sub SDL3 SDL3_image SDL3_mixer SDL3_ttf)
    if(NOT EXISTS "${FB_SDL_DIR}/${sub}/CMakeLists.txt")
        message(FATAL_ERROR
            "The iOS build compiles SDL3 from the submodules, and ${sub} is not "
            "checked out. Run:\n"
            "  git submodule update --init --recursive android/app/jni/SDL3 "
            "android/app/jni/SDL3_image android/app/jni/SDL3_mixer android/app/jni/SDL3_ttf")
    endif()
endforeach()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ---- SDL3 core ------------------------------------------------------------
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL      OFF CACHE BOOL "" FORCE)
add_subdirectory("${FB_SDL_DIR}/SDL3" "${CMAKE_BINARY_DIR}/SDL3" EXCLUDE_FROM_ALL)

# ---- SDL3_image -----------------------------------------------------------
# share/ holds png, gif and one bmp. Formats needing a heavyweight external
# codec are off: they are dependencies this game never loads, and each one is
# minutes of build time and megabytes of binary for a decoder nothing calls.
set(SDLIMAGE_VENDORED ON  CACHE BOOL "" FORCE)
set(SDLIMAGE_INSTALL  OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS    OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF     OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL      OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF      OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP     OFF CACHE BOOL "" FORCE)
# PNG stays on, but not via libpng. Building the vendored libpng drags in the
# vendored zlib, whose own CMakeLists renames zconf.h *inside the source tree*
# on any out-of-source build -- and that source tree is the shared submodule the
# Android build also compiles from, so an iOS build would leave it broken.
# SDL_image decodes PNG without libpng; libpng only adds APNG support, which
# nothing here loads.
set(SDLIMAGE_PNG_LIBPNG OFF CACHE BOOL "" FORCE)
add_subdirectory("${FB_SDL_DIR}/SDL3_image" "${CMAKE_BINARY_DIR}/SDL3_image" EXCLUDE_FROM_ALL)

# ---- SDL3_mixer -----------------------------------------------------------
# Music and effects are all Ogg Vorbis. stb_vorbis decodes them from a header
# that ships inside SDL_mixer, so libogg/libvorbis are not built at all; the
# other decoders are off for the same reason as the image codecs above.
set(SDLMIXER_VENDORED ON  CACHE BOOL "" FORCE)
set(SDLMIXER_INSTALL  OFF CACHE BOOL "" FORCE)
set(SDLMIXER_TESTS    OFF CACHE BOOL "" FORCE)
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_STB        ON  CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_TREMOR     OFF CACHE BOOL "" FORCE)
set(SDLMIXER_FLAC     OFF CACHE BOOL "" FORCE)
set(SDLMIXER_GME      OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MOD      OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MP3      OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI     OFF CACHE BOOL "" FORCE)
set(SDLMIXER_OPUS     OFF CACHE BOOL "" FORCE)
set(SDLMIXER_WAVPACK  OFF CACHE BOOL "" FORCE)
add_subdirectory("${FB_SDL_DIR}/SDL3_mixer" "${CMAKE_BINARY_DIR}/SDL3_mixer" EXCLUDE_FROM_ALL)

# ---- SDL3_ttf -------------------------------------------------------------
# HarfBuzz shapes complex scripts; the game draws one Latin font (DroidSans),
# so it buys nothing here and costs a large C++ dependency. plutosvg is for
# colour emoji, which the game never renders.
set(SDLTTF_VENDORED ON  CACHE BOOL "" FORCE)
set(SDLTTF_INSTALL  OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
add_subdirectory("${FB_SDL_DIR}/SDL3_ttf" "${CMAKE_BINARY_DIR}/SDL3_ttf" EXCLUDE_FROM_ALL)
