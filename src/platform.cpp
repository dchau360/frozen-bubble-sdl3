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

#include "platform.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <system_error>
#if defined(_WIN32) || defined(__MINGW32__)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#elif defined(__APPLE__)
#include <sys/stat.h>   // probing for an installed <prefix>/share/frozen-bubble
#endif

std::string g_dataDir;

#ifdef __ANDROID__
#include <jni.h>
#include <SDL3/SDL_iostream.h>
#include <sys/stat.h>

void InitDataDir() {
    // Assets are extracted from the APK to internal storage by AssetExtractor.java.
    // We read the exact path Java used (FrozenBubbleActivity.sExtractedDataDir) via JNI
    // to avoid mismatches between /data/data/... and /data/user/0/... on some devices.
    JNIEnv* env = (JNIEnv*)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (env && activity) {
        jclass clazz = env->GetObjectClass(activity);
        jfieldID fid = env->GetStaticFieldID(clazz, "sExtractedDataDir", "Ljava/lang/String;");
        if (fid) {
            jstring jstr = (jstring)env->GetStaticObjectField(clazz, fid);
            if (jstr) {
                const char* cstr = env->GetStringUTFChars(jstr, nullptr);
                if (cstr && cstr[0] != '\0') {
                    g_dataDir = std::string(cstr);
                }
                env->ReleaseStringUTFChars(jstr, cstr);
                env->DeleteLocalRef(jstr);
            }
        }
        env->DeleteLocalRef(clazz);
        env->DeleteLocalRef(activity);
    }
    // Fallback if JNI read failed
    if (g_dataDir.empty()) {
        const char* p = SDL_GetAndroidInternalStoragePath();
        g_dataDir = p ? std::string(p) + "/share" : "/data/data/org.frozenbubble/files/share";
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "JNI read of sExtractedDataDir failed, using fallback: %s", g_dataDir.c_str());
    }
    SDL_Log("Android data dir: %s", g_dataDir.c_str());
}

#elif defined(__WASM_PORT__)

// WebAssembly uses Emscripten virtual filesystem
// Assets are preloaded via --preload-file share@/share
void InitDataDir() {
    g_dataDir = "/share";  // Virtual filesystem path
}

#elif defined(__IOS_PORT__)

// iOS bundles are flat: resources sit at the top level of the .app, not under
// Contents/Resources as on macOS, and SDL_GetBasePath() returns that directory
// (with a trailing separator). The bundle is the only possible location — iOS
// has no install prefix to probe and no writable directory outside the app
// container — so unlike desktop there is no DATA_DIR fallback worth keeping.
void InitDataDir() {
    const char* base = SDL_GetBasePath();
    g_dataDir = base ? std::string(base) + "share" : "share";
}

#else // Desktop platforms

void InitDataDir() {
#if defined(_WIN32) || defined(__MINGW32__)
    // On Windows, assets live next to the .exe — derive path at runtime
    // so installed builds work regardless of where the user installs them.
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
        std::string dir(exePath);
        size_t sep = dir.find_last_of("\\/");
        if (sep != std::string::npos) dir = dir.substr(0, sep);
        g_dataDir = dir + "\\share";
        return;
    }
#elif defined(__APPLE__)
    // In a .app bundle SDL_GetBasePath() returns Contents/Resources/.
    // Only use it when inside a bundle (path ends with "Resources/").
    {
        const char* base = SDL_GetBasePath();
        if (base) {
            std::string b(base);
            if (b.size() >= 10 && b.substr(b.size() - 10) == "Resources/") {
                g_dataDir = b + "share";
                return;
            }
            // Not a bundle: an installed build sits at <prefix>/bin, with assets
            // at <prefix>/share/frozen-bubble (see install(DIRECTORY …) in
            // CMakeLists.txt). Without this, macOS fell through to the compiled-in
            // DATA_DIR, which points at the *build machine's* source tree — so an
            // installed copy resolved assets to a path that does not exist on any
            // other machine (audit finding REL-008). Linux and Windows already
            // recover a prefix this way; macOS was the gap.
            size_t bin = b.rfind("/bin/");
            if (bin != std::string::npos) {
                std::string candidate = b.substr(0, bin) + "/share/frozen-bubble";
                struct stat st;
                if (stat(candidate.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    g_dataDir = candidate;
                    return;
                }
            }
        }
    }
#elif defined(__linux__)
    // On Linux, use exe-relative path so AppImage builds work.
    // AppImage mounts at /tmp/.mount_XXXXX — absolute DATA_DIR won't match.
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        std::string dir(exePath);
        // Strip "bin/frozen-bubble-sdl3" → get prefix, append share path
        size_t bin = dir.rfind("/bin/");
        if (bin != std::string::npos) {
            g_dataDir = dir.substr(0, bin) + "/share/frozen-bubble";
            return;
        }
    }
#endif
    g_dataDir = DATA_DIR;
}

#endif

// Deliberately outside the per-platform #if chain above. Android, WASM and
// desktop each define their own InitDataDir(), so a definition placed inside
// any one of those branches exists only for that platform — putting this in the
// desktop branch compiled fine on macOS and Linux and then failed to link for
// WASM with "undefined symbol: LogDataDir()".
//
// Logs the resolved directory on every platform. When asset loading fails the
// first question is always which directory was chosen, and outside Android that
// was previously invisible (audit finding REL-008).
void LogDataDir() {
    SDL_Log("Asset data dir: %s", g_dataDir.c_str());
}

#ifdef __WASM_PORT__
#include <emscripten.h>

bool WasmHasTouch() {
    static int cached = -1;
    if (cached < 0) {
        cached = EM_ASM_INT({
            return ('ontouchstart' in window || navigator.maxTouchPoints > 0) ? 1 : 0;
        });
    }
    return cached == 1;
}

bool WasmPromptText(const char* title, const char* current, char* out, int outLen) {
    int got = EM_ASM_INT({
        var r = window.prompt(UTF8ToString($0), UTF8ToString($1));
        if (r === null) return 0;
        stringToUTF8(r, $2, $3);
        return 1;
    }, title, current ? current : "", out, outLen);
    return got == 1;
}
#endif

bool ReplaceFileAtomically(const std::string& tempPath,
                           const std::string& finalPath) {
    std::error_code error;
    // std::filesystem::rename replaces an existing destination, and does so as
    // one filesystem operation, so no reader ever observes a partial file.
    std::filesystem::rename(tempPath, finalPath, error);
    if (!error) return true;

    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Could not replace %s with %s: %s",
                finalPath.c_str(), tempPath.c_str(), error.message().c_str());

    std::error_code cleanupError;
    std::filesystem::remove(tempPath, cleanupError);
    return false;
}

void RequestPersistentStorageFlush() {
#ifdef __WASM_PORT__
    EM_ASM({
        if (Module['requestPersistentStorageFlush']) {
            // The JS controller reports the error; consume the rejection here
            // so a C++ fire-and-forget request is never unhandled.
            Module['requestPersistentStorageFlush']().catch(function() {});
        }
    });
#endif
}

#ifdef __ANDROID__
// Calls a no-arg static boolean method on FrozenBubbleActivity by name. Same
// Activity-object route as androidPushToken() below, and for the same
// JNI-classloader reason. Shared by androidIsTelevision() and
// androidIsTablet() below, which differ only in which method they call.
static bool callAndroidStaticBoolMethod(const char *methodName) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return false;   // not cached by caller: try again once it exists

    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetStaticMethodID(cls, methodName, "()Z");
    if (!mid) {
        SDL_Log("callAndroidStaticBoolMethod: %s method not found", methodName);
        env->ExceptionClear();
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return false;
    }

    jboolean result = env->CallStaticBooleanMethod(cls, mid);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return false;
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return result;
}

// Cached: the answer cannot change while the process lives, and one caller
// (the chat row in mainmenu_input.cpp) asks once per frame.
static bool androidIsTelevision() {
    static int cached = -1;
    if (cached >= 0) return cached != 0;
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return false;   // not cached: try again once it exists
    bool result = callAndroidStaticBoolMethod("isTelevision");
    cached = result ? 1 : 0;
    return result;
}

#endif

bool DeviceHasTouchscreen() {
#ifdef __ANDROID__
    // A TV box has no touchscreen, but neither Android nor SDL will say so:
    // Android TV reports android.hardware.touchscreen (Google's own TV
    // emulator does), and SDL_GetTouchDevices() counts virtual input devices,
    // which every Android device has -- so the check below answers "yes" on a
    // TV and the game would pick portrait, touch aim and a soft keyboard for
    // a device driven by a remote. Ask what kind of device this is instead.
    if (androidIsTelevision()) return false;
#endif
    int count = 0;
    SDL_TouchID *devices = SDL_GetTouchDevices(&count);
    SDL_free(devices);
    return count > 0;
}

void SetTextInputAreaLogical(SDL_Renderer *renderer, const SDL_Rect &logical) {
    SDL_Window *window = renderer ? SDL_GetRenderWindow(renderer) : SDL_GetKeyboardFocus();
    if (window == nullptr) return;
    if (renderer == nullptr) {
        // No renderer to convert with: pass the rect through rather than
        // dropping the hint entirely. Correct wherever the window is 4:3.
        SDL_SetTextInputArea(window, &logical, 0);
        return;
    }
    float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
    SDL_RenderCoordinatesToWindow(renderer, (float)logical.x, (float)logical.y, &x0, &y0);
    SDL_RenderCoordinatesToWindow(renderer, (float)(logical.x + logical.w),
                                  (float)(logical.y + logical.h), &x1, &y1);
    SDL_Rect windowRect = { (int)x0, (int)y0, (int)(x1 - x0), (int)(y1 - y0) };
    SDL_SetTextInputArea(window, &windowRect, 0);
}

const char* PushPlatformName() {
#if defined(__IOS_PORT__)
    return "ios";
#elif defined(__ANDROID__) || defined(__ANDROID_PORT__)
    return "android";
#else
    return nullptr;
#endif
}

#if defined(__ANDROID__) || defined(__ANDROID_PORT__)
// Calls FrozenBubbleActivity.getPushToken() -> String, which just delegates to
// PushManager.getToken(). Blocking, so it must not run on the UI thread; the
// SDL thread this is called from is not the UI thread.
//
// Goes through the Activity object (SDL_GetAndroidActivity() +
// GetObjectClass()) rather than FindClass("org/frozenbubble/PushManager") by
// name -- the exact same shape androidFetchUrl() in networkclient.cpp already
// uses. FindClass-by-name resolves against the wrong classloader when called
// from a thread the JVM did not create, which the SDL game thread is; it
// silently returns null there instead of throwing anything logged. Looking up
// a method on an already-valid jobject has no such problem, which is why
// PushManager itself is reached only through this one-line wrapper.
static std::string androidPushToken() {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return "";

    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetStaticMethodID(cls, "getPushToken", "()Ljava/lang/String;");
    if (!mid) {
        SDL_Log("androidPushToken: getPushToken method not found");
        env->ExceptionClear();
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return "";
    }

    jstring jresult = (jstring)env->CallStaticObjectMethod(cls, mid);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return "";
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);

    if (jresult == nullptr) return "";
    const char *chars = env->GetStringUTFChars(jresult, nullptr);
    std::string result(chars ? chars : "");
    env->ReleaseStringUTFChars(jresult, chars);
    env->DeleteLocalRef(jresult);
    return result;
}
#endif

std::string PushDeviceToken() {
#if defined(__IOS_PORT__)
    // Defined in push_ios.mm. Returns "" until APNs hands back a token, which
    // is asynchronous and needs the aps-environment entitlement -- an unsigned
    // build never gets one.
    return IosPushDeviceToken();
#elif defined(__ANDROID__) || defined(__ANDROID_PORT__)
    return androidPushToken();
#else
    // Desktop and browser builds have no push service to register with.
    // Following a server is still allowed and still persists; it simply
    // registers nothing from here.
    return std::string();
#endif
}

#if defined(__ANDROID__) || defined(__ANDROID_PORT__)
// Both of these reach static methods on FrozenBubbleActivity through the
// Activity object rather than FindClass()-by-name, for the reason spelled out
// above androidPushToken(): FindClass resolves against the wrong classloader
// on the SDL game thread and silently returns null.
//
// Deliberately uncached, unlike androidIsTelevision(): a price arrives
// asynchronously from Play some time after startup, and the entitlement
// changes the moment somebody buys. Caching either would show a stale row.
bool AdsRemoved() {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return false;

    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetStaticMethodID(cls, "adsRemoved", "()Z");
    if (!mid) {
        env->ExceptionClear();
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return false;
    }
    jboolean result = env->CallStaticBooleanMethod(cls, mid);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result = JNI_FALSE;
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return result != JNI_FALSE;
}

std::string AdsPrice(int productIndex) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return std::string();

    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetStaticMethodID(cls, "adsPrice", "(I)Ljava/lang/String;");
    if (!mid) {
        env->ExceptionClear();
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return std::string();
    }
    jstring jresult = (jstring)env->CallStaticObjectMethod(cls, mid, (jint)productIndex);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(activity);
        return std::string();
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);

    if (jresult == nullptr) return std::string();
    const char *chars = env->GetStringUTFChars(jresult, nullptr);
    std::string result(chars ? chars : "");
    env->ReleaseStringUTFChars(jresult, chars);
    env->DeleteLocalRef(jresult);
    return result;
}
#endif
