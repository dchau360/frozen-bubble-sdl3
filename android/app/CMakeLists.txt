cmake_minimum_required(VERSION 3.22)
project(frozen-bubble CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL3 and extension libs built from source in jni/ (git submodules)
set(JNI_DIR "${CMAKE_SOURCE_DIR}/jni")

# Disable SDL3 test programs to speed up build
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)

# SDL3_mixer: only WAV + OGG Vorbis, disable everything else
set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE ON CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_STB OFF CACHE BOOL "" FORCE)
set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MP3 OFF CACHE BOOL "" FORCE)
set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDLMIXER_WAVE ON CACHE BOOL "" FORCE)
set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)

# SDL3_image: PNG + GIF only (bubble textures are GIFs)
set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDLIMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDLIMAGE_GIF ON CACHE BOOL "" FORCE)
set(SDLIMAGE_ANI OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_BMP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JPG OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PCX OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PNM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TGA OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XV OFF CACHE BOOL "" FORCE)

# SDL3_ttf: vendored freetype + harfbuzz; no SVG needed
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)

add_subdirectory("${JNI_DIR}/SDL3"       SDL3_build)
add_subdirectory("${JNI_DIR}/SDL3_image" SDL3_image_build)
add_subdirectory("${JNI_DIR}/SDL3_mixer" SDL3_mixer_build)
add_subdirectory("${JNI_DIR}/SDL3_ttf"   SDL3_ttf_build)

# iniparser static lib (vendored in jni/iniparser)
add_library(iniparser STATIC
    "${JNI_DIR}/iniparser/iniparser.c"
    "${JNI_DIR}/iniparser/dictionary.c"
)
target_include_directories(iniparser PUBLIC "${JNI_DIR}/iniparser")

# Game as shared library (Android requires this)
add_library(main SHARED
    ../../src/frozenbubble.cpp
    ../../src/main.cpp
    ../../src/menubutton.cpp
    ../../src/mainmenu.cpp
    ../../src/gamesettings.cpp
    ../../src/audiomixer.cpp
    ../../src/shaderstuff.cpp
    ../../src/bubblegame.cpp
    ../../src/transitionmanager.cpp
    ../../src/ttftext.cpp
    ../../src/highscoremanager.cpp
    ../../src/networkclient.cpp
    ../../src/logger.cpp
    ../../src/platform.cpp
)

target_compile_definitions(main PRIVATE
    __ANDROID__
    DATA_DIR=""
)

target_include_directories(main PRIVATE
    ../../src
)

target_link_libraries(main
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_mixer::SDL3_mixer
    SDL3_ttf::SDL3_ttf
    iniparser
    android
    log
    GLESv1_CM
    GLESv2
    EGL
    atomic
)
