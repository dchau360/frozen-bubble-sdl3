cmake_minimum_required(VERSION 3.13)

project(frozen-bubble-sdl2 CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT (CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "${DEFAULT_BUILD_TYPE}")
endif()

if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang"))
    add_compile_options(-Wall -Wextra -pedantic -Wno-pointer-arith)
endif()

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_ttf REQUIRED)

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

if(NOT SDL2_image_FOUND)
    message(FATAL_ERROR "SDL2_image not found")
endif()

if(NOT SDL2_mixer_FOUND)
    message(FATAL_ERROR "SDL2_mixer not found")
endif()

if(NOT SDL2_ttf_FOUND)
    message(FATAL_ERROR "SDL2_ttf not found")
endif()

if(UNIX OR APPLE)
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

add_executable(frozen-bubble-sdl2
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
    src/networkclient.cpp
    src/logger.cpp
    src/platform.cpp
    ${RES_FILES}
)

set_property(TARGET frozen-bubble-sdl2 PROPERTY CXX_STANDARD 17)
set_property(TARGET frozen-bubble-sdl2 PROPERTY CXX_EXTENSIONS OFF)

if(NOT DEFINED ASSET_PATH)
    set(ASSET_PATH ${CMAKE_SOURCE_DIR}/share)
endif()

message("Asset path is: ${ASSET_PATH}")

if(ANDROID)
    # Android TV build configuration
    target_compile_definitions(frozen-bubble-sdl2 PRIVATE __ANDROID_PORT__)
    # DATA_DIR not needed on Android - assets loaded via asset manager
else()
    target_compile_definitions(frozen-bubble-sdl2 PRIVATE DATA_DIR=\"${ASSET_PATH}\")
endif()

target_include_directories(frozen-bubble-sdl2
    SYSTEM
    PRIVATE ${SDL2_INCLUDE_DIRS}
    PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS}
    PRIVATE ${SDL2_MIXER_INCLUDE_DIRS}
    PRIVATE ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(frozen-bubble-sdl2 PRIVATE
    SDL2::SDL2
    iniparser-static
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf
)

if(WIN32 OR MINGW)
    target_link_libraries(frozen-bubble-sdl2 PRIVATE ws2_32)
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
install(TARGETS frozen-bubble-sdl2
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
