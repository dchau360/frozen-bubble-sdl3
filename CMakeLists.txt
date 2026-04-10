cmake_minimum_required(VERSION 3.13)

project(frozen-bubble-sdl3 CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT (CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "${DEFAULT_BUILD_TYPE}")
endif()

if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))
    add_compile_options(-Wall -Wextra -pedantic -Wno-pointer-arith)
endif()

if(NOT EMSCRIPTEN)
    find_package(SDL3 REQUIRED)
    find_package(SDL3_image REQUIRED)
    find_package(SDL3_mixer REQUIRED)
    find_package(SDL3_ttf REQUIRED)
endif()

find_package(iniparser QUIET COMPONENTS static)
if(NOT iniparser_FOUND OR NOT TARGET iniparser-static)
    message(STATUS "iniparser system package not found, building from bundled source")
    add_library(iniparser-static STATIC
        "${CMAKE_SOURCE_DIR}/third_party/iniparser/iniparser.c"
        "${CMAKE_SOURCE_DIR}/third_party/iniparser/dictionary.c"
    )
    target_include_directories(iniparser-static PUBLIC
        "${CMAKE_SOURCE_DIR}/third_party/iniparser"
    )
endif()

# REQUIRED on find_package handles missing libraries

if(NOT WIN32 AND NOT MINGW AND NOT EMSCRIPTEN)
    add_subdirectory(server)
endif()

if(APPLE)
    include_directories("/opt/homebrew/include")
    include_directories("/usr/local/include")
endif()

set(RES_FILES "")
if(WIN32 OR MINGW)
    set(RES_FILES share/icons/fb.rc)
    set(CMAKE_RC_COMPILER_INIT windres)
    ENABLE_LANGUAGE(RC)
endif()

if(EMSCRIPTEN)
    # WASM: compile both files; networkclient.cpp guards TCP code with #ifndef __WASM_PORT__
    set(NETWORK_CLIENT_SRC src/networkclient.cpp src/networkclient_wasm.cpp)
else()
    set(NETWORK_CLIENT_SRC src/networkclient.cpp)
endif()

add_executable(frozen-bubble-sdl3
    src/frozenbubble.cpp
    src/main.cpp
    src/menubutton.cpp
    src/mainmenu.cpp
    src/gamesettings.cpp
    src/audiomixer.cpp
    src/shaderstuff.cpp
    src/bubblegame.cpp
    src/transitionmanager.cpp
    src/ttftext.cpp
    src/highscoremanager.cpp
    ${NETWORK_CLIENT_SRC}
    src/logger.cpp
    src/platform.cpp
    ${RES_FILES}
)

set_property(TARGET frozen-bubble-sdl3 PROPERTY CXX_STANDARD 17)
set_property(TARGET frozen-bubble-sdl3 PROPERTY CXX_EXTENSIONS OFF)

if(EMSCRIPTEN)
    set_target_properties(frozen-bubble-sdl3 PROPERTIES SUFFIX ".html")
endif()

if(NOT DEFINED ASSET_PATH)
    set(ASSET_PATH ${CMAKE_SOURCE_DIR}/share)
endif()

message("Asset path is: ${ASSET_PATH}")

if(ANDROID)
    # Android TV build configuration
    target_compile_definitions(frozen-bubble-sdl3 PRIVATE __ANDROID_PORT__)
    # DATA_DIR not needed on Android - assets loaded via asset manager
elseif(EMSCRIPTEN)
    target_compile_definitions(frozen-bubble-sdl3 PRIVATE __WASM_PORT__ DATA_DIR=\"/share\")
else()
    target_compile_definitions(frozen-bubble-sdl3 PRIVATE DATA_DIR=\"${ASSET_PATH}\")
endif()

# SDL3 uses modern cmake targets — include dirs come from target properties

if(NOT EMSCRIPTEN)
    target_link_libraries(frozen-bubble-sdl3 PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
    )
endif()
target_link_libraries(frozen-bubble-sdl3 PRIVATE iniparser-static)

if(WIN32 OR MINGW)
    target_link_libraries(frozen-bubble-sdl3 PRIVATE ws2_32)
endif()

# Installation configuration
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if(APPLE)
        set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install path prefix" FORCE)
    elseif(UNIX)
        set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install path prefix" FORCE)
    endif()
endif()

# Install executable
install(TARGETS frozen-bubble-sdl3
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

# Install game assets
install(DIRECTORY ${CMAKE_SOURCE_DIR}/share/
    DESTINATION share/frozen-bubble
    COMPONENT assets
    PATTERN ".DS_Store" EXCLUDE
    PATTERN "*.md" EXCLUDE
)

# Set asset path for installed version
if(CMAKE_INSTALL_PREFIX)
    set(INSTALLED_ASSET_PATH "${CMAKE_INSTALL_PREFIX}/share/frozen-bubble")
    message(STATUS "Installed assets will be at: ${INSTALLED_ASSET_PATH}")
endif()

# Add uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()
