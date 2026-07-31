cmake_minimum_required(VERSION 3.13)

project(fb-server C)

# Version comes from the top-level project (REL-004's single source). When this
# directory is configured standalone — `cmake -S server`, which is how the
# server is built without SDL3 for sanitizer runs — there is no parent, so
# CMAKE_PROJECT_VERSION is this project's own and therefore empty. That silently
# baked "fb-server v " into the binary. Recover it by reading the root
# CMakeLists rather than restating the number here, so there is still exactly
# one place to edit it.
if(CMAKE_PROJECT_VERSION)
    set(FB_VERSION "${CMAKE_PROJECT_VERSION}")
else()
    file(READ "${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt" FB_ROOT_CMAKE)
    string(REGEX MATCH "project\\(frozen-bubble-sdl3 VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)"
           FB_VERSION_MATCH "${FB_ROOT_CMAKE}")
    if(CMAKE_MATCH_1)
        set(FB_VERSION "${CMAKE_MATCH_1}")
    else()
        set(FB_VERSION "0.0.0-unknown")
        message(WARNING "Could not read the version from the root CMakeLists.txt")
    endif()
endif()
message(STATUS "fb-server version: ${FB_VERSION}")

set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT (CMAKE_BUILD_TYPE OR DEFINED ENV{CMAKE_BUILD_TYPE}))
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "${DEFAULT_BUILD_TYPE}")
endif()

if (("${CMAKE_C_COMPILER_ID}" MATCHES "GNU") OR ("${CMAKE_C_COMPILER_ID}" MATCHES "Clang"))
    add_compile_options(-Wall -Wextra -pedantic)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0>=2.10)

add_executable(fb-server
    fb-server.c
    game.c
    log.c
    net.c
    tools.c
    stats.c
    ws.c
)

# Inherited from the top-level project(... VERSION ...) rather than restated.
# This reaches users: net.c sends it as the master-server user agent and prints
# it at startup (audit finding REL-004).
#
# CMAKE_PROJECT_VERSION, not PROJECT_VERSION: this directory calls its own
# project(fb-server C) above, which resets PROJECT_VERSION to empty and would
# silently bake in "fb-server v" with no number.
target_compile_definitions(fb-server PUBLIC VERSION="${FB_VERSION}")

target_include_directories(fb-server
    PRIVATE ${GLIB2_INCLUDE_DIRS}
)

target_link_libraries(fb-server
    PkgConfig::GLIB2
)

if(WIN32 OR MINGW)
    target_link_libraries(fb-server ws2_32)
    # Suppress pedantic warnings for the Windows compat stubs
    target_compile_options(fb-server PRIVATE -Wno-pedantic)
endif()
