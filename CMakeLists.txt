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
#
# iOS additionally needs Objective-C++ for src/platform_ios.mm. It is declared
# here rather than via a later enable_language(OBJCXX): enabling a language after
# project() leaves CMake without the compile and archive rules for it, which
# fails the generate step. CMAKE_SYSTEM_NAME is already readable at this point
# because the iOS build passes it on the command line.
set(FB_LANGUAGES CXX C)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND FB_LANGUAGES OBJCXX)
endif()
project(frozen-bubble-sdl3 VERSION 2.4.56 LANGUAGES ${FB_LANGUAGES})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT (CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "${DEFAULT_BUILD_TYPE}")
endif()

if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))
    add_compile_options(-Wall -Wextra -pedantic -Wno-pointer-arith)
endif()

if(IOS)
    # No system SDL exists for iOS, and a find_package() here would happily match
    # the host's macOS Homebrew build. Compile it from the pinned submodules.
    include(iOSDeps)
elseif(NOT EMSCRIPTEN)
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

# fb-server is a desktop host process. iOS forbids fork/exec inside the app
# sandbox, so nothing could launch it even if it were built and bundled.
if(NOT WIN32 AND NOT MINGW AND NOT EMSCRIPTEN AND NOT IOS)
    add_subdirectory(server)
endif()

# Homebrew prefixes hold *macOS* headers. On an iOS build they would shadow the
# iPhoneOS SDK and produce a binary that links against the wrong platform, so
# this stays macOS-only rather than applying to every APPLE target.
if(APPLE AND NOT IOS)
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

# iOS needs Objective-C++ for two things the portable code cannot express:
# NSURLSession standing in for the desktop popen("curl ..."), and APNs
# registration for follow-a-server notifications. OBJCXX is enabled in
# project() above.
if(IOS)
    set(PLATFORM_EXTRA_SRC src/platform_ios.mm src/push_ios.mm)
else()
    set(PLATFORM_EXTRA_SRC "")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CoreSources.cmake)

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
        -lidbfs.js
        "SHELL:-sENVIRONMENT=web"
        "SHELL:-sSDL3_IMAGE_FORMATS=['png','gif']"
        "SHELL:-sSDL3_MIXER_FORMATS=['ogg']"
        "SHELL:--pre-js ${CMAKE_SOURCE_DIR}/web/persistence.js"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/share@/share"
        "SHELL:-sEXPORTED_RUNTIME_METHODS=['cwrap','ccall','setValue','getValue','UTF8ToString','stringToUTF8','FS']"
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
elseif(IOS)
    # No DATA_DIR: assets resolve from SDL_GetBasePath() inside the .app bundle,
    # and a compile-time path baked in from this machine could only ever be wrong
    # on a device (the same trap REL-008 recorded for installed macOS builds).
    target_compile_definitions(frozen-bubble-sdl3 PRIVATE __IOS_PORT__)
elseif(EMSCRIPTEN)
    target_compile_definitions(frozen-bubble-sdl3 PRIVATE __WASM_PORT__ DATA_DIR=\"/share\")
else()
    target_compile_definitions(frozen-bubble-sdl3 PRIVATE DATA_DIR=\"${ASSET_PATH}\")
endif()

# Covers every platform built through this file, so the version shown in the
# menu matches the tag this was built from (REL-004). Android is the exception:
# it builds through android/app/CMakeLists.txt, which sets APP_VERSION itself
# from build.gradle's versionName.
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

if(IOS)
    # UserNotifications is not among the frameworks SDL3 pulls in, so push_ios.mm
    # needs it named explicitly or the link fails on UNUserNotificationCenter.
    target_link_libraries(frozen-bubble-sdl3 PRIVATE "-framework UserNotifications")

    set(FB_IOS_EXECUTABLE "FrozenBubble")
    set(FB_IOS_BUNDLE_ID "org.frozenbubble.sdl3" CACHE STRING
        "iOS bundle identifier")
    configure_file("${CMAKE_SOURCE_DIR}/cmake/iOSInfo.plist.in"
                   "${CMAKE_BINARY_DIR}/Info.plist" @ONLY)

    # Emitted next to the .app so signing can point at it. Generated rather than
    # checked in only because the APNs environment has to be switchable: a token
    # minted against the sandbox is rejected by production and vice versa.
    set(FB_IOS_APS_ENVIRONMENT "development" CACHE STRING
        "aps-environment entitlement value: development or production")
    configure_file("${CMAKE_SOURCE_DIR}/cmake/FrozenBubble.entitlements.in"
                   "${CMAKE_BINARY_DIR}/FrozenBubble.entitlements" @ONLY)

    set_target_properties(frozen-bubble-sdl3 PROPERTIES
        OUTPUT_NAME "${FB_IOS_EXECUTABLE}"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
        # Unsigned by request. Left explicit so an Xcode-generator build does not
        # quietly pick up a development identity and produce an artifact that
        # only installs on the machine that built it.
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    )

    # An iOS bundle is flat -- resources live at the top level of the .app, not
    # under Contents/Resources -- which is exactly what TARGET_BUNDLE_CONTENT_DIR
    # expands to here. copy_directory_if_different keeps incremental builds from
    # recopying 3000+ sprites on every link.
    add_custom_command(TARGET frozen-bubble-sdl3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${CMAKE_SOURCE_DIR}/share"
                "$<TARGET_BUNDLE_CONTENT_DIR:frozen-bubble-sdl3>/share"
        COMMENT "Copying game assets into the app bundle"
        VERBATIM)

    # The home-screen icon has to be a compiled Assets.car; iOS 11 and later
    # ignore loose PNGs in the bundle. Only actool can build one, so it runs here
    # rather than being checked in.
    if(CMAKE_OSX_SYSROOT MATCHES "Simulator")
        set(FB_IOS_ACTOOL_PLATFORM "iphonesimulator")
    else()
        set(FB_IOS_ACTOOL_PLATFORM "iphoneos")
    endif()
    add_custom_command(TARGET frozen-bubble-sdl3 POST_BUILD
        COMMAND "${CMAKE_SOURCE_DIR}/tools/ios-appicon.sh"
                "${CMAKE_SOURCE_DIR}/ios/AppIcon.xcassets"
                "$<TARGET_BUNDLE_CONTENT_DIR:frozen-bubble-sdl3>"
                "${FB_IOS_ACTOOL_PLATFORM}"
                "${CMAKE_OSX_DEPLOYMENT_TARGET}"
                "${CMAKE_BINARY_DIR}/appicon"
        COMMENT "Compiling the app icon asset catalog"
        VERBATIM)
endif()

include(CTest)
# The test executables are host tools: ctest would have to run them on a device.
if(BUILD_TESTING AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT IOS)
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

    add_executable(bubbleai-test
        tests/bubbleai_test.cpp
        ${FROZEN_BUBBLE_CORE_SOURCES}
    )
    set_property(TARGET bubbleai-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET bubbleai-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(bubbleai-test PRIVATE src)
    target_compile_definitions(bubbleai-test PRIVATE
        FROZEN_BUBBLE_TEST_ACCESS
        DATA_DIR="${ASSET_PATH}"
        APP_VERSION="v${PROJECT_VERSION}"
    )
    target_link_libraries(bubbleai-test PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        iniparser-static
    )
    if(WIN32 OR MINGW)
        target_link_libraries(bubbleai-test PRIVATE ws2_32)
    endif()
    add_test(NAME bubbleai-test COMMAND bubbleai-test)

    add_executable(bot-play-test
        tests/bot_play_test.cpp
        ${FROZEN_BUBBLE_CORE_SOURCES}
    )
    set_property(TARGET bot-play-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET bot-play-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(bot-play-test PRIVATE src)
    target_compile_definitions(bot-play-test PRIVATE
        FROZEN_BUBBLE_TEST_ACCESS
        DATA_DIR="${ASSET_PATH}"
        APP_VERSION="v${PROJECT_VERSION}"
    )
    target_link_libraries(bot-play-test PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        iniparser-static
    )
    if(WIN32 OR MINGW)
        target_link_libraries(bot-play-test PRIVATE ws2_32)
    endif()
    add_test(NAME bot-play-test COMMAND bot-play-test)

    add_executable(netbot-test
        tests/netbot_test.cpp
        src/netbot.cpp
    )
    set_property(TARGET netbot-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET netbot-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(netbot-test PRIVATE src)
    target_link_libraries(netbot-test PRIVATE SDL3::SDL3)
    if(WIN32 OR MINGW)
        target_link_libraries(netbot-test PRIVATE ws2_32)
    endif()
    add_test(NAME netbot-test COMMAND netbot-test)

    add_executable(persistence-save-test
        tests/persistence_save_test.cpp
        ${FROZEN_BUBBLE_CORE_SOURCES}
    )
    set_property(TARGET persistence-save-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET persistence-save-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(persistence-save-test PRIVATE src)
    target_compile_definitions(persistence-save-test PRIVATE
        FROZEN_BUBBLE_TEST_ACCESS
        DATA_DIR="${ASSET_PATH}"
        APP_VERSION="v${PROJECT_VERSION}"
    )
    target_link_libraries(persistence-save-test PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        iniparser-static
    )
    if(WIN32 OR MINGW)
        target_link_libraries(persistence-save-test PRIVATE ws2_32)
    endif()
    add_test(NAME persistence-save-test COMMAND persistence-save-test)

    add_executable(localmultiplayer-settings-test
        tests/localmultiplayer_settings_test.cpp
        ${FROZEN_BUBBLE_CORE_SOURCES}
    )
    set_property(TARGET localmultiplayer-settings-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET localmultiplayer-settings-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(localmultiplayer-settings-test PRIVATE src)
    target_compile_definitions(localmultiplayer-settings-test PRIVATE
        FROZEN_BUBBLE_TEST_ACCESS
        DATA_DIR="${ASSET_PATH}"
        APP_VERSION="v${PROJECT_VERSION}"
    )
    target_link_libraries(localmultiplayer-settings-test PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        iniparser-static
    )
    if(WIN32 OR MINGW)
        target_link_libraries(localmultiplayer-settings-test PRIVATE ws2_32)
    endif()
    add_test(NAME localmultiplayer-settings-test COMMAND localmultiplayer-settings-test)

    add_executable(localmultiplayer-header-compile-test
        tests/localmultiplayer_header_compile_test.cpp
        src/localmultiplayer_settings.cpp
    )
    set_property(TARGET localmultiplayer-header-compile-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET localmultiplayer-header-compile-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(localmultiplayer-header-compile-test PRIVATE src)
    target_link_libraries(localmultiplayer-header-compile-test PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        iniparser-static
    )
    add_test(NAME localmultiplayer-header-compile-test
        COMMAND localmultiplayer-header-compile-test)

    add_executable(touch-letterbox-test
        tests/touch_letterbox_test.cpp
    )
    set_property(TARGET touch-letterbox-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET touch-letterbox-test PROPERTY CXX_EXTENSIONS OFF)
    target_link_libraries(touch-letterbox-test PRIVATE SDL3::SDL3)
    add_test(NAME touch-letterbox-test COMMAND touch-letterbox-test)
    # Reports itself skipped where no video backend can be created at all,
    # rather than passing without having checked anything.
    set_tests_properties(touch-letterbox-test PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(menu-touch-gesture-test
        tests/menu_touch_gesture_test.cpp
        ${FROZEN_BUBBLE_CORE_SOURCES}
    )
    set_property(TARGET menu-touch-gesture-test PROPERTY CXX_STANDARD 17)
    set_property(TARGET menu-touch-gesture-test PROPERTY CXX_EXTENSIONS OFF)
    target_include_directories(menu-touch-gesture-test PRIVATE src)
    target_compile_definitions(menu-touch-gesture-test PRIVATE
        FROZEN_BUBBLE_TEST_ACCESS
        DATA_DIR="${ASSET_PATH}"
        APP_VERSION="v${PROJECT_VERSION}"
    )
    target_link_libraries(menu-touch-gesture-test PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        iniparser-static
    )
    if(WIN32 OR MINGW)
        target_link_libraries(menu-touch-gesture-test PRIVATE ws2_32)
    endif()
    add_test(NAME menu-touch-gesture-test COMMAND menu-touch-gesture-test)

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
        add_test(NAME server-bot-cap-test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server_bot_cap_test.py $<TARGET_FILE:fb-server>)
        add_test(NAME server-massleave-test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server_massleave_test.py $<TARGET_FILE:fb-server>)
        # Follow-a-server notifications: registration, the join hook, cooldown,
        # unregistration and on-disk persistence, with a UDP socket standing in
        # for the relay sidecar.
        add_test(NAME server-notify-test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/server_notify_test.py $<TARGET_FILE:fb-server>)
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

# Nothing to install on iOS: the deliverable is the .app bundle itself, which
# already carries its assets, and CMake rejects a MACOSX_BUNDLE target given a
# RUNTIME destination anyway.
if(NOT IOS)
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
endif()

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
