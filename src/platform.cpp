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
#include <SDL3/SDL_dialog.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>
#if defined(_WIN32) || defined(__MINGW32__)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#elif defined(__APPLE__)
#include <sys/stat.h>   // probing for an installed <prefix>/share/frozen-bubble
#include <TargetConditionals.h>  // TARGET_OS_IPHONE, for PlatformTag()
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

char PlatformTag() {
    // Order matters twice over: __WASM_PORT__ first because an Emscripten
    // build also defines __linux__ on a Linux host and would otherwise
    // report itself as one, and the iOS check before the macOS fallback for
    // the same reason -- TARGET_OS_IPHONE is true on a platform where
    // __APPLE__ is true as well.
#if defined(__WASM_PORT__)
    return 'B';
#elif defined(__ANDROID__) || defined(__ANDROID_PORT__)
    return 'A';
#elif defined(_WIN32) || defined(__MINGW32__)
    return 'W';
#elif defined(__APPLE__)
#  if TARGET_OS_IPHONE
    return 'I';
#  else
    return 'M';
#  endif
#else
    return 'L';
#endif
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
// Calls a no-arg static boolean method on FrozenBubbleActivity by name. Goes
// through the Activity object (SDL_GetAndroidActivity() + GetObjectClass())
// rather than FindClass()-by-name, which resolves against the wrong
// classloader when called from a thread the JVM did not create -- the SDL
// game thread this runs on -- and silently returns null there instead of
// throwing anything logged. Shared by androidIsTelevision() and
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

// The community Discord invite. See platform.h for why this is a build-time
// constant and not something a server advertises over the wire.
//
// Use a permanent, never-expiring invite: Discord's default invites expire
// after 7 days, and one that lapses turns every Discord row in the game into a
// dead end for everyone still on that release -- there is no way to fix it
// short of shipping a new build. In Discord: right-click the channel → Invite
// People → Edit invite link → Expire after: Never, Max number of uses: No
// limit.
const char* const kDiscordInviteUrl = "https://discord.gg/uE4dq8fqGW";

#ifdef FROZEN_BUBBLE_TEST_ACCESS
bool testForceDiscordInviteOff = false;
#endif

bool HasDiscordInvite() {
#ifdef FROZEN_BUBBLE_TEST_ACCESS
    if (testForceDiscordInviteOff) return false;
#endif
    return kDiscordInviteUrl != nullptr && kDiscordInviteUrl[0] != '\0';
}

bool OpenDiscordInvite() {
    if (!HasDiscordInvite()) return false;
    // SDL_OpenURL covers every platform this game ships on -- xdg-open/
    // ShellExecute/NSWorkspace on desktop, ACTION_VIEW on Android,
    // openURL: on iOS, window.open in Emscripten -- so there is no
    // per-platform branch to keep in step here.
    if (!SDL_OpenURL(kDiscordInviteUrl)) {
        SDL_Log("OpenDiscordInvite: SDL_OpenURL failed: %s", SDL_GetError());
        return false;
    }
    return true;
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

#if defined(__ANDROID__) || defined(__ANDROID_PORT__)
// Both of these reach static methods on FrozenBubbleActivity through the
// Activity object rather than FindClass()-by-name: FindClass resolves against
// the wrong classloader on the SDL game thread and silently returns null.
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

// ── Replay file export/import seam (R4d, filled in by R4e) ────────────────────
//
// R4d shipped placeholder stubs here; R4e replaces them with the real platform
// dialogs. The state machine is the same everywhere -- Begin() arms an
// operation, a platform-specific completion path writes a terminal result, and
// Poll() consumes that result exactly once -- so the pending-op bookkeeping is
// shared. Only the notification path differs: desktop hands out a pointer to
// the per-op state as SDL dialog userdata and SDL invokes the callback on
// whatever thread its backend uses; WASM's async file reader calls back into
// C++ through EMSCRIPTEN_KEEPALIVE exports. Android reuses desktop's callback/
// userdata shape, but does its byte I/O with SDL_SaveFile()/SDL_LoadFile(),
// which transparently handle the content:// URIs its picker returns; only iOS
// still has no bridge and keeps R4d's unconditional failure.
//
// SDL documents that a dialog callback may run on a different thread than the
// SDL_Show*Dialog call, so the pending result is mutex-guarded even though the
// game itself runs on one thread. That protection is what makes Poll() safe to
// call from the render loop while the OS owns the dialog.

namespace {
struct PendingFileOp {
    std::mutex mutex;
    bool inFlight = false;
    PlatformFileOpStatus result = PlatformFileOpStatus::Idle; // terminal once set
    std::vector<uint8_t> bytes;              // import: loaded bytes
    std::string name;                        // import: suggested name
    std::vector<uint8_t> pendingWriteBytes;  // export: bytes to write on success
};

// 16 MiB is deliberately generous. The closest related constant, 1 MiB
// (kMaxPayloadLength in replay_format.h), bounds a single *record* inside a
// .fbr file, not the whole file; a real multi-record replay is legitimately
// larger, so a tighter cap here could reject a valid import. This only guards
// against a mistakenly-chosen enormous file, not against a real replay.
constexpr size_t kReplayImportMaxBytes = 16u * 1024u * 1024u;
} // namespace

#if defined(__ANDROID__) || defined(__ANDROID_PORT__)

// Declared in platform.h only under FROZEN_BUBBLE_TEST_ACCESS; repeated here so
// the dialog callbacks below can see them in a production build too.
bool WriteReplayBytesToPath(const std::string &path, const std::vector<uint8_t> &bytes);
bool ReadReplayBytesFromPath(const std::string &path, std::vector<uint8_t> &outBytes,
                             size_t maxBytes);

namespace {
PendingFileOp g_exportOp;
PendingFileOp g_importOp;

// The chosen-URI branch of the export dialog. The string SDL hands back is an
// Android Storage Access Framework content:// URI, not a filesystem path --
// SDLActivity's onActivityResult() never resolves it to a real file -- so the
// write itself goes through WriteReplayBytesToPath() below, which asks SDL to
// open that URI. Everything shape-wise (nullptr filelist = error, {nullptr} =
// cancel) is identical to the desktop callback.
void ExportDialogCallback(void *userdata, const char * const *filelist, int filter) {
    (void)filter;
    PendingFileOp *op = static_cast<PendingFileOp *>(userdata);
    std::lock_guard<std::mutex> lock(op->mutex);
    if (filelist == nullptr) {
        SDL_Log("Replay export dialog failed: %s", SDL_GetError());
        op->result = PlatformFileOpStatus::Failed;
        op->inFlight = false;
        return;
    }
    if (filelist[0] == nullptr) {
        op->result = PlatformFileOpStatus::Cancelled;
        op->inFlight = false;
        return;
    }
    op->result = WriteReplayBytesToPath(filelist[0], op->pendingWriteBytes)
                     ? PlatformFileOpStatus::Succeeded
                     : PlatformFileOpStatus::Failed;
    op->inFlight = false;
}

void ImportDialogCallback(void *userdata, const char * const *filelist, int filter) {
    (void)filter;
    PendingFileOp *op = static_cast<PendingFileOp *>(userdata);
    std::lock_guard<std::mutex> lock(op->mutex);
    if (filelist == nullptr) {
        SDL_Log("Replay import dialog failed: %s", SDL_GetError());
        op->result = PlatformFileOpStatus::Failed;
        op->inFlight = false;
        return;
    }
    if (filelist[0] == nullptr) {
        op->result = PlatformFileOpStatus::Cancelled;
        op->inFlight = false;
        return;
    }
    std::vector<uint8_t> bytes;
    if (ReadReplayBytesFromPath(filelist[0], bytes, kReplayImportMaxBytes)) {
        op->bytes = std::move(bytes);
        // A content:// URI has no filesystem name; its last path segment is
        // the document id the provider assigned, which is the closest thing to
        // a suggested name the picker gives back. Desktop's filename() would
        // be empty here, so this only ever differs in cosmetics.
        op->name = std::filesystem::path(filelist[0]).filename().string();
        op->result = PlatformFileOpStatus::Succeeded;
    } else {
        op->result = PlatformFileOpStatus::Failed;
    }
    op->inFlight = false;
}

// Resolves the window a native dialog attaches to, the same way
// SetTextInputAreaLogical() does: from the renderer when one is available,
// else the keyboard-focused window. Android's backend ignores it, but keeping
// the helper identical to desktop avoids a second way of doing the same thing.
SDL_Window *ReplayDialogWindow(SDL_Renderer *renderer) {
    SDL_Window *window = renderer ? SDL_GetRenderWindow(renderer) : nullptr;
    if (window == nullptr) window = SDL_GetKeyboardFocus();
    return window;
}

const SDL_DialogFileFilter kReplayFileFilters[] = {
    {"Frozen Bubble replay", "fbr"},
};
} // namespace

// SDL_SaveFile()/SDL_LoadFile() are thin wrappers over SDL_IOFromFile(), whose
// Android backend resolves a content:// URI through
// Android_JNI_OpenFileDescriptor() -- the only route to a Storage Access
// Framework document. std::ofstream/std::ifstream (the desktop branch's byte
// I/O) cannot open one: they would treat the URI as a literal path and fail.
bool WriteReplayBytesToPath(const std::string &path, const std::vector<uint8_t> &bytes) {
    // SDL_SaveFile opens with mode "wb", which Android_JNI_OpenFileDescriptor()
    // maps to ContentResolver's "wt" (write + truncate) -- the same fresh-file
    // semantics the desktop branch's std::ios::trunc asks for.
    return SDL_SaveFile(path.c_str(), bytes.empty() ? nullptr : bytes.data(),
                        bytes.size());
}

bool ReadReplayBytesFromPath(const std::string &path, std::vector<uint8_t> &outBytes,
                             size_t maxBytes) {
    // Unlike desktop's ifstream + std::ios::ate, SDL_LoadFile() offers no way to
    // learn the size before reading, so the cap is enforced after the load: an
    // oversized file is read in full and then discarded rather than rejected up
    // front. Same end behavior (outBytes untouched, return false), just with the
    // allocation already spent.
    size_t size = 0;
    void *data = SDL_LoadFile(path.c_str(), &size);
    if (data == nullptr) return false;
    if (size > maxBytes) {
        SDL_free(data);
        return false;
    }
    const uint8_t *first = static_cast<const uint8_t *>(data);
    outBytes.assign(first, first + size);
    SDL_free(data);
    return true;
}

bool PlatformExportReplayFileBegin(SDL_Renderer *renderer,
                                   const std::string &suggestedName,
                                   const std::vector<uint8_t> &bytes) {
    {
        std::lock_guard<std::mutex> lock(g_exportOp.mutex);
        if (g_exportOp.inFlight) return false;
        g_exportOp.inFlight = true;
        g_exportOp.result = PlatformFileOpStatus::Idle;
        g_exportOp.pendingWriteBytes = bytes;
        // Kept in the struct rather than reading suggestedName.c_str() later:
        // the dialog is asynchronous, so default_location must outlive this
        // call. The struct lives until the operation finishes.
        g_exportOp.name = suggestedName;
    }

    SDL_ShowSaveFileDialog(&ExportDialogCallback, &g_exportOp,
                           ReplayDialogWindow(renderer), kReplayFileFilters, 1,
                           g_exportOp.name.c_str());
    return true;
}

PlatformFileOpStatus PlatformExportReplayFilePoll() {
    std::lock_guard<std::mutex> lock(g_exportOp.mutex);
    if (g_exportOp.inFlight) return PlatformFileOpStatus::Pending;
    if (g_exportOp.result != PlatformFileOpStatus::Idle) {
        const PlatformFileOpStatus status = g_exportOp.result;
        g_exportOp.result = PlatformFileOpStatus::Idle;
        g_exportOp.pendingWriteBytes.clear();
        return status;
    }
    return PlatformFileOpStatus::Idle;
}

bool PlatformImportReplayFileBegin(SDL_Renderer *renderer) {
    {
        std::lock_guard<std::mutex> lock(g_importOp.mutex);
        if (g_importOp.inFlight) return false;
        g_importOp.inFlight = true;
        g_importOp.result = PlatformFileOpStatus::Idle;
        g_importOp.bytes.clear();
        g_importOp.name.clear();
    }

    SDL_ShowOpenFileDialog(&ImportDialogCallback, &g_importOp,
                           ReplayDialogWindow(renderer), kReplayFileFilters, 1,
                           nullptr, false);
    return true;
}

PlatformFileOpStatus PlatformImportReplayFilePoll(std::vector<uint8_t> &outBytes,
                                                   std::string &outSuggestedName) {
    std::lock_guard<std::mutex> lock(g_importOp.mutex);
    if (g_importOp.inFlight) return PlatformFileOpStatus::Pending;
    if (g_importOp.result != PlatformFileOpStatus::Idle) {
        const PlatformFileOpStatus status = g_importOp.result;
        if (status == PlatformFileOpStatus::Succeeded) {
            outBytes = std::move(g_importOp.bytes);
            outSuggestedName = std::move(g_importOp.name);
        }
        g_importOp.result = PlatformFileOpStatus::Idle;
        g_importOp.bytes.clear();
        g_importOp.name.clear();
        return status;
    }
    return PlatformFileOpStatus::Idle;
}

#elif defined(__IOS_PORT__)

// No document-picker bridge yet (a separate, future package). Byte-identical
// to R4d's stub: the Replays page reports "not yet available on this build"
// rather than presenting a dead button. `renderer` is accepted and ignored so
// the signature matches every other platform.
bool PlatformExportReplayFileBegin(SDL_Renderer *renderer,
                                   const std::string &suggestedName,
                                   const std::vector<uint8_t> &bytes) {
    (void)renderer;
    (void)suggestedName;
    (void)bytes;
    return false;
}

PlatformFileOpStatus PlatformExportReplayFilePoll() {
    return PlatformFileOpStatus::Idle;
}

bool PlatformImportReplayFileBegin(SDL_Renderer *renderer) {
    (void)renderer;
    return false;
}

PlatformFileOpStatus PlatformImportReplayFilePoll(std::vector<uint8_t> &outBytes,
                                                   std::string &outSuggestedName) {
    (void)outBytes;
    (void)outSuggestedName;
    return PlatformFileOpStatus::Idle;
}

#elif defined(__WASM_PORT__)

namespace {
PendingFileOp g_exportOp;
PendingFileOp g_importOp;
} // namespace

// Allocates the buffer JS fills with the chosen file's bytes. Going through an
// exported allocator (rather than Module._malloc from JS) means the WASM build
// does not depend on `_malloc`/`_free` being in EXPORTED_FUNCTIONS -- this
// project's WASM linker flags export ccall and friends, not the allocator.
// OnReplayImportFileReady() owns the buffer and frees it. Test/JS bridge only.
extern "C" EMSCRIPTEN_KEEPALIVE uint8_t *OnReplayImportAlloc(int len) {
    if (len <= 0 || static_cast<size_t>(len) > kReplayImportMaxBytes) return nullptr;
    return static_cast<uint8_t *>(std::malloc(static_cast<size_t>(len)));
}

// Called from JS once FileReader has the whole chosen file's bytes in the
// Emscripten heap. Copies them into the pending-import state and marks the
// operation Succeeded; Poll() picks that up on the next frame. Frees `data`,
// which OnReplayImportAlloc() returned.
extern "C" EMSCRIPTEN_KEEPALIVE void OnReplayImportFileReady(uint8_t *data, int len,
                                                             const char *name) {
    std::lock_guard<std::mutex> lock(g_importOp.mutex);
    if (data && len > 0) g_importOp.bytes.assign(data, data + len);
    else g_importOp.bytes.clear();
    g_importOp.name = (name && name[0]) ? name : "imported.fbr";
    g_importOp.result = PlatformFileOpStatus::Succeeded;
    g_importOp.inFlight = false;
    std::free(data);
}

// The user dismissed the picker with no file. Fired either from the input's
// `cancel` event (recent Chromium/Firefox) or the window-focus fallback below.
extern "C" EMSCRIPTEN_KEEPALIVE void OnReplayImportCancelled() {
    std::lock_guard<std::mutex> lock(g_importOp.mutex);
    g_importOp.result = PlatformFileOpStatus::Cancelled;
    g_importOp.inFlight = false;
}

// Builds a Blob from the export bytes, hands it to a throwaway <a download>
// and clicks it. The object URL is revoked on a short timer so the download
// has time to start first.
EM_JS(void, fb_replay_export_download, (const uint8_t *data, int len, const char *name), {
    var slice = HEAPU8.subarray(data, data + len);
    var blob = new Blob([slice], { type: 'application/octet-stream' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = UTF8ToString(name);
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
});

// A hidden <input type="file"> is created once and reused. `change` reads the
// selected file asynchronously; because the picker can also be dismissed with
// nothing chosen, two cancellation paths run alongside it:
//   * the input's `cancel` event, where the browser supports it, and
//   * a fallback -- a one-shot window `focus` listener armed when the picker
//     opens, which after a short delay reports cancellation if no file has
//     arrived. The delay lets a real file-selection `change` land first.
// `cancel` is not universally reliable, which is why the focus fallback exists
// rather than being optional polish.
EM_JS(void, fb_replay_import_trigger, (), {
    function finish(bytes, name) {
        if (!bytes) {
            Module.ccall('OnReplayImportCancelled', null, [], []);
            return;
        }
        var ptr = Module.ccall('OnReplayImportAlloc', 'number', ['number'],
                               [bytes.length]);
        if (!ptr) {
            Module.ccall('OnReplayImportCancelled', null, [], []);
            return;
        }
        HEAPU8.set(bytes, ptr);
        Module.ccall('OnReplayImportFileReady', null,
                     ['number', 'number', 'string'], [ptr, bytes.length, name]);
    }

    var input = Module['fbReplayImportInput'];
    if (!input) {
        input = document.createElement('input');
        input.type = 'file';
        input.accept = '.fbr';
        input.style.display = 'none';
        document.body.appendChild(input);
        Module['fbReplayImportInput'] = input;

        input.addEventListener('change', function() {
            Module['fbReplayImportHandled'] = true;
            var file = (input.files && input.files.length > 0) ? input.files[0] : null;
            input.value = '';
            if (!file) { finish(null, null); return; }
            var reader = new FileReader();
            reader.onload = function() {
                finish(new Uint8Array(reader.result), file.name);
            };
            reader.onerror = function() { finish(null, null); };
            reader.readAsArrayBuffer(file);
        });

        input.addEventListener('cancel', function() {
            if (!Module['fbReplayImportHandled']) {
                Module['fbReplayImportHandled'] = true;
                finish(null, null);
            }
            input.value = '';
        });
    }

    Module['fbReplayImportHandled'] = false;

    window.addEventListener('focus', function() {
        setTimeout(function() {
            if (!Module['fbReplayImportHandled']) {
                Module['fbReplayImportHandled'] = true;
                finish(null, null);
            }
        }, 300);
    }, { once: true });

    input.click();
});

bool PlatformExportReplayFileBegin(SDL_Renderer *renderer,
                                   const std::string &suggestedName,
                                   const std::vector<uint8_t> &bytes) {
    (void)renderer;   // no native dialog to attach to in the browser
    {
        std::lock_guard<std::mutex> lock(g_exportOp.mutex);
        if (g_exportOp.inFlight) return false;
        g_exportOp.inFlight = true;
        g_exportOp.result = PlatformFileOpStatus::Idle;
    }
    // Single-threaded build: the download is kicked off synchronously and is
    // already complete by the time this returns, so Poll() reports Succeeded
    // on its first call.
    fb_replay_export_download(bytes.empty() ? nullptr : bytes.data(),
                              static_cast<int>(bytes.size()), suggestedName.c_str());
    {
        std::lock_guard<std::mutex> lock(g_exportOp.mutex);
        g_exportOp.result = PlatformFileOpStatus::Succeeded;
        g_exportOp.inFlight = false;
    }
    return true;
}

PlatformFileOpStatus PlatformExportReplayFilePoll() {
    std::lock_guard<std::mutex> lock(g_exportOp.mutex);
    if (g_exportOp.inFlight) return PlatformFileOpStatus::Pending;
    if (g_exportOp.result != PlatformFileOpStatus::Idle) {
        const PlatformFileOpStatus status = g_exportOp.result;
        g_exportOp.result = PlatformFileOpStatus::Idle;
        return status;
    }
    return PlatformFileOpStatus::Idle;
}

bool PlatformImportReplayFileBegin(SDL_Renderer *renderer) {
    (void)renderer;
    {
        std::lock_guard<std::mutex> lock(g_importOp.mutex);
        if (g_importOp.inFlight) return false;
        g_importOp.inFlight = true;
        g_importOp.result = PlatformFileOpStatus::Idle;
        g_importOp.bytes.clear();
        g_importOp.name.clear();
    }
    fb_replay_import_trigger();
    return true;
}

PlatformFileOpStatus PlatformImportReplayFilePoll(std::vector<uint8_t> &outBytes,
                                                   std::string &outSuggestedName) {
    std::lock_guard<std::mutex> lock(g_importOp.mutex);
    if (g_importOp.inFlight) return PlatformFileOpStatus::Pending;
    if (g_importOp.result != PlatformFileOpStatus::Idle) {
        const PlatformFileOpStatus status = g_importOp.result;
        if (status == PlatformFileOpStatus::Succeeded) {
            outBytes = std::move(g_importOp.bytes);
            outSuggestedName = std::move(g_importOp.name);
        }
        g_importOp.result = PlatformFileOpStatus::Idle;
        g_importOp.bytes.clear();
        g_importOp.name.clear();
        return status;
    }
    return PlatformFileOpStatus::Idle;
}

#else // Desktop (Linux/macOS/Windows)

// Declared in platform.h only under FROZEN_BUBBLE_TEST_ACCESS; repeated here so
// the dialog callbacks below can see them in a production build too.
bool WriteReplayBytesToPath(const std::string &path, const std::vector<uint8_t> &bytes);
bool ReadReplayBytesFromPath(const std::string &path, std::vector<uint8_t> &outBytes,
                             size_t maxBytes);

namespace {
PendingFileOp g_exportOp;
PendingFileOp g_importOp;

// The chosen-path branch of the export dialog. This writes to a path the user
// picked outside ReplayLibrary's own sandboxed replays/ directory, so -- unlike
// ReplayLibrary::Write()'s internal atomic temp-then-rename -- no atomic-rename
// ceremony applies here; a plain truncating write is what the user asked for.
void ExportDialogCallback(void *userdata, const char * const *filelist, int filter) {
    (void)filter;
    PendingFileOp *op = static_cast<PendingFileOp *>(userdata);
    std::lock_guard<std::mutex> lock(op->mutex);
    if (filelist == nullptr) {
        SDL_Log("Replay export dialog failed: %s", SDL_GetError());
        op->result = PlatformFileOpStatus::Failed;
        op->inFlight = false;
        return;
    }
    if (filelist[0] == nullptr) {
        op->result = PlatformFileOpStatus::Cancelled;
        op->inFlight = false;
        return;
    }
    op->result = WriteReplayBytesToPath(filelist[0], op->pendingWriteBytes)
                     ? PlatformFileOpStatus::Succeeded
                     : PlatformFileOpStatus::Failed;
    op->inFlight = false;
}

void ImportDialogCallback(void *userdata, const char * const *filelist, int filter) {
    (void)filter;
    PendingFileOp *op = static_cast<PendingFileOp *>(userdata);
    std::lock_guard<std::mutex> lock(op->mutex);
    if (filelist == nullptr) {
        SDL_Log("Replay import dialog failed: %s", SDL_GetError());
        op->result = PlatformFileOpStatus::Failed;
        op->inFlight = false;
        return;
    }
    if (filelist[0] == nullptr) {
        op->result = PlatformFileOpStatus::Cancelled;
        op->inFlight = false;
        return;
    }
    std::vector<uint8_t> bytes;
    if (ReadReplayBytesFromPath(filelist[0], bytes, kReplayImportMaxBytes)) {
        op->bytes = std::move(bytes);
        op->name = std::filesystem::path(filelist[0]).filename().string();
        op->result = PlatformFileOpStatus::Succeeded;
    } else {
        op->result = PlatformFileOpStatus::Failed;
    }
    op->inFlight = false;
}

// Resolves the window a native dialog attaches to, the same way
// SetTextInputAreaLogical() does: from the renderer when one is available,
// else the keyboard-focused window.
SDL_Window *ReplayDialogWindow(SDL_Renderer *renderer) {
    SDL_Window *window = renderer ? SDL_GetRenderWindow(renderer) : nullptr;
    if (window == nullptr) window = SDL_GetKeyboardFocus();
    return window;
}

const SDL_DialogFileFilter kReplayFileFilters[] = {
    {"Frozen Bubble replay", "fbr"},
};
} // namespace

bool WriteReplayBytesToPath(const std::string &path, const std::vector<uint8_t> &bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char *>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    output.flush();
    return output.good();
}

bool ReadReplayBytesFromPath(const std::string &path, std::vector<uint8_t> &outBytes,
                             size_t maxBytes) {
    // std::ios::ate positions at EOF so tellg() reports the size before any of
    // the file is read; an oversized file is rejected without being loaded.
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return false;
    const std::streamoff size = input.tellg();
    if (size < 0) return false;
    if (static_cast<uint64_t>(size) > static_cast<uint64_t>(maxBytes)) return false;
    outBytes.assign(static_cast<size_t>(size), 0);
    input.seekg(0, std::ios::beg);
    if (size > 0) {
        input.read(reinterpret_cast<char *>(outBytes.data()),
                   static_cast<std::streamsize>(size));
        if (!input) {
            outBytes.clear();
            return false;
        }
    }
    return true;
}

bool PlatformExportReplayFileBegin(SDL_Renderer *renderer,
                                   const std::string &suggestedName,
                                   const std::vector<uint8_t> &bytes) {
    {
        std::lock_guard<std::mutex> lock(g_exportOp.mutex);
        if (g_exportOp.inFlight) return false;
        g_exportOp.inFlight = true;
        g_exportOp.result = PlatformFileOpStatus::Idle;
        g_exportOp.pendingWriteBytes = bytes;
        // Kept in the struct rather than reading suggestedName.c_str() later:
        // the dialog is asynchronous, so default_location must outlive this
        // call. The struct lives until the operation finishes.
        g_exportOp.name = suggestedName;
    }

#ifdef FROZEN_BUBBLE_TEST_ACCESS
    if (testReplayFileOpsHeadless) return true;
#endif

    SDL_ShowSaveFileDialog(&ExportDialogCallback, &g_exportOp,
                           ReplayDialogWindow(renderer), kReplayFileFilters, 1,
                           g_exportOp.name.c_str());
    return true;
}

PlatformFileOpStatus PlatformExportReplayFilePoll() {
    std::lock_guard<std::mutex> lock(g_exportOp.mutex);
    if (g_exportOp.inFlight) return PlatformFileOpStatus::Pending;
    if (g_exportOp.result != PlatformFileOpStatus::Idle) {
        const PlatformFileOpStatus status = g_exportOp.result;
        g_exportOp.result = PlatformFileOpStatus::Idle;
        g_exportOp.pendingWriteBytes.clear();
        return status;
    }
    return PlatformFileOpStatus::Idle;
}

bool PlatformImportReplayFileBegin(SDL_Renderer *renderer) {
    {
        std::lock_guard<std::mutex> lock(g_importOp.mutex);
        if (g_importOp.inFlight) return false;
        g_importOp.inFlight = true;
        g_importOp.result = PlatformFileOpStatus::Idle;
        g_importOp.bytes.clear();
        g_importOp.name.clear();
    }

#ifdef FROZEN_BUBBLE_TEST_ACCESS
    if (testReplayFileOpsHeadless) return true;
#endif

    SDL_ShowOpenFileDialog(&ImportDialogCallback, &g_importOp,
                           ReplayDialogWindow(renderer), kReplayFileFilters, 1,
                           nullptr, false);
    return true;
}

PlatformFileOpStatus PlatformImportReplayFilePoll(std::vector<uint8_t> &outBytes,
                                                   std::string &outSuggestedName) {
    std::lock_guard<std::mutex> lock(g_importOp.mutex);
    if (g_importOp.inFlight) return PlatformFileOpStatus::Pending;
    if (g_importOp.result != PlatformFileOpStatus::Idle) {
        const PlatformFileOpStatus status = g_importOp.result;
        if (status == PlatformFileOpStatus::Succeeded) {
            outBytes = std::move(g_importOp.bytes);
            outSuggestedName = std::move(g_importOp.name);
        }
        g_importOp.result = PlatformFileOpStatus::Idle;
        g_importOp.bytes.clear();
        g_importOp.name.clear();
        return status;
    }
    return PlatformFileOpStatus::Idle;
}

#ifdef FROZEN_BUBBLE_TEST_ACCESS
bool testReplayFileOpsHeadless = false;

void TestSimulateExportDialogResult(const char * const *filelist) {
    ExportDialogCallback(&g_exportOp, filelist, 0);
}

void TestSimulateImportDialogResult(const char * const *filelist) {
    ImportDialogCallback(&g_importOp, filelist, 0);
}

void TestResetPlatformReplayFileState() {
    {
        std::lock_guard<std::mutex> lock(g_exportOp.mutex);
        g_exportOp.inFlight = false;
        g_exportOp.result = PlatformFileOpStatus::Idle;
        g_exportOp.bytes.clear();
        g_exportOp.name.clear();
        g_exportOp.pendingWriteBytes.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_importOp.mutex);
        g_importOp.inFlight = false;
        g_importOp.result = PlatformFileOpStatus::Idle;
        g_importOp.bytes.clear();
        g_importOp.name.clear();
        g_importOp.pendingWriteBytes.clear();
    }
}
#endif // FROZEN_BUBBLE_TEST_ACCESS

#endif // platform selection
