# The single list of game sources, shared by the root build and the Android
# build.
#
# Android builds through android/app/CMakeLists.txt rather than the root
# CMakeLists.txt, so it used to keep its own copy of this list -- and the copy
# drifted. A new source file would build on every platform except Android, and
# nothing said so until the CI link step failed with undefined symbols. Both
# files now read this one.
#
# Paths are absolute, derived from this file's own location, so it does not
# matter which directory includes it.

set(FB_SRC "${CMAKE_CURRENT_LIST_DIR}/../src")

# Platform-selected sources. The root CMakeLists.txt sets these before
# including this file (WASM needs both network clients; iOS adds its own
# platform code); anything else gets the plain defaults.
if(NOT DEFINED NETWORK_CLIENT_SRC)
    set(NETWORK_CLIENT_SRC ${FB_SRC}/networkclient.cpp)
endif()
if(NOT DEFINED PLATFORM_EXTRA_SRC)
    set(PLATFORM_EXTRA_SRC "")
endif()

# main.cpp is deliberately absent: each target adds its own entry point.
set(FROZEN_BUBBLE_CORE_SOURCES
    ${FB_SRC}/frozenbubble.cpp
    ${FB_SRC}/menubutton.cpp
    ${FB_SRC}/menutheme.cpp
    ${FB_SRC}/mainmenu.cpp
    ${FB_SRC}/mainmenu_input.cpp
    ${FB_SRC}/mainmenu_netpanel.cpp
    ${FB_SRC}/mainmenu_panels.cpp
    ${FB_SRC}/mainmenu_help.cpp
    ${FB_SRC}/mainmenu_server.cpp
    ${FB_SRC}/menulist.cpp
    ${FB_SRC}/localmultiplayer_settings.cpp
    ${FB_SRC}/gamesettings.cpp
    ${FB_SRC}/audiomixer.cpp
    ${FB_SRC}/shaderstuff.cpp
    ${FB_SRC}/bubbleai.cpp
    ${FB_SRC}/bubblegame.cpp
    ${FB_SRC}/bubblegame_board.cpp
    ${FB_SRC}/bubblegame_input.cpp
    ${FB_SRC}/bubblegame_level.cpp
    ${FB_SRC}/bubblegame_net.cpp
    ${FB_SRC}/bubblegame_render.cpp
    ${FB_SRC}/bubblegame_shooter.cpp
    ${FB_SRC}/bubblegame_state.cpp
    ${FB_SRC}/sendGameStats.cpp
    ${FB_SRC}/netbot.cpp
    ${FB_SRC}/netview.cpp
    ${FB_SRC}/netteams.cpp
    ${FB_SRC}/roundstats_color.cpp
    ${FB_SRC}/transitionmanager.cpp
    ${FB_SRC}/ttftext.cpp
    ${FB_SRC}/highscoremanager.cpp
    ${NETWORK_CLIENT_SRC}
    ${FB_SRC}/logger.cpp
    ${FB_SRC}/platform.cpp
    ${PLATFORM_EXTRA_SRC}
)
