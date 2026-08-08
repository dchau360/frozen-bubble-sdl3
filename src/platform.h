/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
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

// Supplied by CMake from project(... VERSION ...) — see CMakeLists.txt. The
// literal fallback only applies to builds that bypass CMake; keeping it in step
// is not relied on, which is why it is deliberately marked as unknown rather
// than carrying a number that will silently rot (audit finding REL-004).
#ifndef APP_VERSION
#define APP_VERSION "v0.0.0-nocmake"
#endif

#include <SDL3/SDL.h>
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
// Logs the resolved g_dataDir; call after InitDataDir() so asset-load failures
// say which directory was actually chosen.
void LogDataDir();

// Requests that persistent browser storage capture the current virtual
// filesystem state. Native platforms write directly to disk, so this is a no-op.
void RequestPersistentStorageFlush();

// Moves tempPath onto finalPath, replacing it if it exists. Callers write the
// complete new contents to tempPath first, so an interrupted save leaves the
// previous file intact instead of a truncated one. Returns false and removes
// tempPath if the move fails, leaving finalPath untouched.
bool ReplaceFileAtomically(const std::string& tempPath,
                           const std::string& finalPath);

// True when SDL has registered at least one touch device. This is how the game
// tells an Android phone or tablet from an Android TV box: one APK serves both,
// and several defaults are opposite between them (aim mode, screen
// orientation). SDL registers Android touch devices during video init, so the
// answer is available any time after SDL_Init(SDL_INIT_VIDEO) -- and is empty
// before it, so callers must not run earlier than that.
bool DeviceHasTouchscreen();

// Tells the platform where the text field being edited is, so iOS and Android
// can lift the view clear of the on-screen keyboard instead of letting it cover
// the field. The rect is in 640x480 logical canvas coordinates and is converted
// here: SDL wants window coordinates, and on a letterboxed phone screen the two
// are nowhere near each other -- a logical rect passed straight through lands
// far from the field and the platform shifts by the wrong amount, or not at all.
void SetTextInputAreaLogical(SDL_Renderer *renderer, const SDL_Rect &logical);

#ifdef __WASM_PORT__
// True when the browser reports touch capability (phones/tablets).
// SDL3's Emscripten backend has no screen-keyboard support, so touch devices
// need native browser prompts for text entry (see WasmPromptText).
bool WasmHasTouch();

// Show a blocking browser prompt() pre-filled with `current`.
// Returns true and copies the entry into `out` when the user confirms;
// returns false (out untouched) when the user cancels.
bool WasmPromptText(const char* title, const char* current, char* out, int outLen);
#endif

// ── Push notifications for followed servers ──────────────────────────────────
//
// Which push service this build talks to: "ios", "android", or nullptr where
// there is none. The strings match what the server's NOTIFYREG accepts.
// nullptr is the answer for desktop and for the browser build, neither of
// which can receive a notification while closed -- following a server is
// still allowed there, it just registers nothing until the player opens the
// same account on a phone.
const char* PushPlatformName();

// This device's push token, or "" when there isn't one.
//
// Empty is the normal, expected answer today: acquiring a real token needs an
// APNs entitlement (iOS) or a Firebase project (Android), neither of which is
// wired up yet. Every caller must treat "" as "cannot register" and skip the
// send rather than transmitting an empty token the server would reject. Once
// the platform pieces land, this is the single place they report back to.
std::string PushDeviceToken();

#ifdef __IOS_PORT__
// APNs device token as a lowercase hex string, or "" if there isn't one yet.
//
// Registration is asynchronous and can legitimately never complete: an
// unsigned build has no aps-environment entitlement and APNs simply fails,
// and the player can decline the permission prompt. Both are ordinary
// outcomes, reported as "". Defined in push_ios.mm.
std::string IosPushDeviceToken();

// Kick off the permission prompt and APNs registration. Safe to call more than
// once. Defined in push_ios.mm.
void IosRegisterForPush();

// Blocking HTTP GET, returning the response body ("" on any failure).
// Defined in platform_ios.mm. iOS blocks fork/exec inside the app sandbox and
// ships no curl binary, so the desktop popen("curl ...") path returns nothing
// there; this is the iOS counterpart of Android's JNI fetchUrl().
std::string IosFetchUrl(const char* url, int timeoutSeconds);
#endif

#endif // PLATFORM_H
