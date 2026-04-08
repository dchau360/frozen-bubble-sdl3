cmake_minimum_required(VERSION 3.22)
project(frozen-bubble CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL3 and extension libs built from source in jni/ (git submodules)
set(JNI_DIR "${CMAKE_SOURCE_DIR}/jni")

# Disable SDL3 test programs to speed up build
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_SAMPLES OFF CACHE BOOL "" FORCE)

# SDL3_mixer: disable formats that require vendored external deps
set(SDL3MIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDL3MIXER_VORBIS_VORBISFILE ON CACHE BOOL "" FORCE)
set(SDL3MIXER_OGG ON CACHE BOOL "" FORCE)
set(SDL3MIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_MP3 OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_WAV ON CACHE BOOL "" FORCE)
set(SDL3MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_MOD_XMP OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_MOD_XMP_LITE OFF CACHE BOOL "" FORCE)
set(SDL3MIXER_GME OFF CACHE BOOL "" FORCE)

# SDL3_image: PNG + GIF needed (bubble textures are GIFs)
set(SDL3IMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_BMP OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_GIF ON CACHE BOOL "" FORCE)
set(SDL3IMAGE_JPG OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_LBM OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_PCX OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_PNM OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_QOI OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_SVG OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_TGA OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_XCF OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_XPM OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_XV OFF CACHE BOOL "" FORCE)
set(SDL3IMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDL3IMAGE_VENDORED ON CACHE BOOL "" FORCE)

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
