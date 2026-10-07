#pragma once

// Which game this build is. One engine ships as two games: Frozen Bubble (the
// default, everything in share/) and Boba Buster, whose art lives in a separate
// private repository laid over share/ at build time. CMake's FB_BRAND picks one
// and defines FB_BRAND_BOBA for Boba Buster; see "Brands" in CLAUDE.md.
//
// Everything a player sees that names the game, and the folder settings, the
// account code and replays are kept in, comes from here. The folder differs so
// the two games, installed side by side, never share a settings.ini or an
// account. kBrandShareDir is where an installed copy's art sits under its
// prefix, matching install(DIRECTORY ...) in CMakeLists.txt, so the two
// games' art never overwrite each other there either.

#if defined(FB_BRAND_BOBA)
inline constexpr const char* kBrandName = "Boba Buster";
inline constexpr const char* kBrandWindowTitle = "Boba Buster";
inline constexpr const char* kBrandPrefDir = "boba-buster";
inline constexpr const char* kBrandReplayFilterName = "Boba Buster replay";
inline constexpr const char* kBrandShareDir = "/share/boba-buster";
#else
inline constexpr const char* kBrandName = "Frozen Bubble";
inline constexpr const char* kBrandWindowTitle = "Frozen-Bubble: SDL3";
inline constexpr const char* kBrandPrefDir = "frozen-bubble";
inline constexpr const char* kBrandReplayFilterName = "Frozen Bubble replay";
inline constexpr const char* kBrandShareDir = "/share/frozen-bubble";
#endif
