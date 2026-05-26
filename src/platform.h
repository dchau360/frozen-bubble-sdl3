/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#define APP_VERSION "v2.4.20"

#include <string>

// Runtime data directory - set at startup via InitDataDir()
// On desktop: set from DATA_DIR compile-time define
// On Android: set to SDL_GetPrefPath() after asset extraction
extern std::string g_dataDir;

// Returns full path to an asset file.
// Usage: ASSET("/gfx/foo.png").c_str()  or  ASSET("/gfx/foo.png")
inline std::string ASSET(const char* relpath) {
#ifdef __ANDROID__
    // Assets are extracted to g_dataDir by AssetExtractor; use full path.
    return g_dataDir + relpath;
#elif defined(__WASM_PORT__)
    // WebAssembly: assets are preloaded at /share via --preload-file share@/share
    // g_dataDir is "/share", so ASSET("/gfx/foo.png") → "/share/gfx/foo.png"
    return g_dataDir + relpath;
#else
    return g_dataDir + relpath;
#endif
}

// Initialize g_dataDir for the current platform.
// On Android: extracts APK assets to writable storage on first run.
// On other platforms: uses DATA_DIR compile-time define.
void InitDataDir();

#endif // PLATFORM_H
