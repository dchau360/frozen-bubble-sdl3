cmake_minimum_required(VERSION 3.13)

# Single source of truth for the version. Audit finding REL-004 recorded five
# distinct hand-maintained strings across build inputs (2.2.1, 2.4.9, v2.4.26,
# 2.4.27, 0.1.0), four of which reached a runtime or artifact surface — the
# in-game menu was showing v2.4.26 while the release was several versions on.
# Anything that can read CMake now derives its version from here.
#
# Two mirrors cannot: android/app/build.gradle (versionName/versionCode, which
# Android requires to advance per release) and the off-tag fallback in
# .github/workflows/build.yml. Both are listed in the release checklist in
# CLAUDE.md and must be bumped together with this line.
project(frozen-bubble-sdl3 VERSION 2.4.33 LANGUAGES CXX C)

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

# Always build iniparser from the bundled source (vendored iniparser 4.x, MIT).
# Avoids silently preferring any system iniparser — the parser implementation
# the build uses is a property of the repository, not the host (IMP-023).
add_library(iniparser-static STATIC
    "${CMAKE_SOURCE_DIR}/third_party/iniparser/iniparser.c"
    "${CMAKE_SOURCE_DIR}/third_party/iniparser/dictionary.c"
)
target_include_directories(iniparser-static PUBLIC
    "${CMAKE_SOURCE_DIR}/third_party/iniparser"
)

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

set(FROZEN_BUBBLE_CORE_SOURCES
    src/frozenbubble.cpp
    src/menubutton.cpp
    src/mainmenu.cpp
    src/mainmenu_input.cpp
    src/mainmenu_netpanel.cpp
    src/mainmenu_panels.cpp
    src/mainmenu_server.cpp
    src/localmultiplayer_settings.cpp
    src/gamesettings.cpp
    src/audiomixer.cpp
    src/shaderstuff.cpp
    src/bubblegame.cpp
    src/bubblegame_board.cpp
    src/bubblegame_input.cpp
    src/bubblegame_level.cpp
    src/bubblegame_net.cpp
    src/bubblegame_render.cpp
    src/bubblegame_shooter.cpp
    src/bubblegame_state.cpp
    src/netview.cpp
    src/netteams.cpp
    src/roundstats_color.cpp
    src/transitionmanager.cpp
    src/ttftext.cpp
    src/highscoremanager.cpp
    ${NETWORK_CLIENT_SRC}
    src/logger.cpp
    src/platform.cpp
)

add_executable(frozen-bubble-sdl3
    src/main.cpp
    ${FROZEN_BUBBLE_CORE_SOURCES}
    ${RES_FILES}
)

set_property(TARGET frozen-bubble-sdl3 PROPERTY CXX_STANDARD 17)
set_property(TARGET frozen-bubble-sdl3 PROPERTY CXX_EXTENSIONS OFF)

if(EMSCRIPTEN)
    set_target_properties(frozen-bubble-sdl3 PROPERTIES SUFFIX ".html")
    # SDL3 port flags — compile step downloads port headers, link step bundles libs
    target_compile_options(frozen-bubble-sdl3 PRIVATE
        -Wno-experimental
        -sUSE_SDL=3
        -sUSE_SDL_IMAGE=3
        -sUSE_SDL_MIXER=3
        -sUSE_SDL_TTF=3
    )
    target_link_options(frozen-bubble-sdl3 PRIVATE
        --bind
        -Wno-experimental
        -sUSE_SDL=3
        -sUSE_SDL_IMAGE=3
        -sUSE_SDL_MIXER=3
        -sUSE_SDL_TTF=3
        -sALLOW_MEMORY_GROWTH=1
        -sDISABLE_EXCEPTION_CATCHING=0
        -sWASM=1
        -sINITIAL_MEMORY=16777216
        -lwebsocket.js
        "SHELL:-sENVIRONMENT=web"
        "SHELL:-sSDL3_IMAGE_FORMATS=['png','gif']"
        "SHELL:-sSDL3_MIXER_FORMATS=['ogg']"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/share@/share"
        "SHELL:-sEXPORTED_RUNTIME_METHODS=['cwrap','ccall','setValue','getValue','UTF8ToString','stringToUTF8']"
        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/shell.html"
    )
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

# Applies to every platform, including Android and WASM, so the version shown
# in the menu always matches the tag this was built from (REL-004).
target_compile_definitions(frozen-bubble-sdl3
    PRIVATE APP_VERSION=\"v${PROJECT_VERSION}\")

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

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN AND NOT ANDROID)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    add_executable(bubblegame-rules-test
        tests/bubblegame_rules_test.cpp
        ${FROZEN_BUBBLE_CORE_SOURCES}
    )
    set_property(TARGET bubblegame-rules-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET bubblegame-rules-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(bubblegame-rules-test PRIVATE src)
    target_compile_definitions(bubblegame-rules-test PRIVATE
        FROZEN_BUBBLE_TEST_ACCESS
        DATA_DIR="${ASSET_PATH}"
        APP_VERSION="v${PROJECT_VERSION}"
    )
    target_link_libraries(bubblegame-rules-test PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        iniparser-static
    )
    if(WIN32 OR MINGW)
        target_link_libraries(bubblegame-rules-test PRIVATE ws2_32)
    endif()
    add_test(NAME bubblegame-rules-test COMMAND bubblegame-rules-test)

    add_executable(netview-test
        tests/netview_test.cpp
        src/netview.cpp
    )
    set_property(TARGET netview-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET netview-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(netview-test PRIVATE src)
    add_test(NAME netview-test COMMAND netview-test)

    add_executable(netteams-test
        tests/netteams_test.cpp
        src/netteams.cpp
    )
    set_property(TARGET netteams-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET netteams-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(netteams-test PRIVATE src)
    add_test(NAME netteams-test COMMAND netteams-test)

    add_test(NAME net-bots-test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/net_bots_test.py)

    add_executable(roundstats-color-test
        tests/roundstats_color_test.cpp
        src/roundstats_color.cpp
    )
    set_property(TARGET roundstats-color-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET roundstats-color-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(roundstats-color-test PRIVATE src)
    add_test(NAME roundstats-color-test COMMAND roundstats-color-test)

    add_executable(teamclamp-test
        tests/teamclamp_test.cpp
    )
    set_property(TARGET teamclamp-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET teamclamp-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(teamclamp-test PRIVATE src)
    target_link_libraries(teamclamp-test PRIVATE SDL3::SDL3)
    add_test(NAME teamclamp-test COMMAND teamclamp-test)

    if(TARGET fb-server)
        add_test(NAME server-list-cap-test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server_list_cap_test.py $<TARGET_FILE:fb-server>)
        add_test(NAME server-massleave-test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server_massleave_test.py $<TARGET_FILE:fb-server>)
        # Binds the hardcoded UDP discovery port, so it cannot run concurrently
        # with another fb-server; it skips when that port is already in use.
        add_test(NAME server-udp-probe-test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server_udp_probe_test.py $<TARGET_FILE:fb-server>)
        set_tests_properties(server-udp-probe-test PROPERTIES RUN_SERIAL TRUE)
        # Both detect the AddressSanitizer regressions they guard, so they skip
        # on a plain build rather than passing vacuously. Surface that as SKIPPED
        # instead of a green result that never ran anything.
        set_tests_properties(server-massleave-test server-udp-probe-test
            PROPERTIES SKIP_RETURN_CODE 77)
    endif()
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
