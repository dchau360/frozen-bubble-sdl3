cmake_minimum_required(VERSION 3.13)

project(fb-server C)

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
)

target_compile_definitions(fb-server PUBLIC VERSION="2.2.1")

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
