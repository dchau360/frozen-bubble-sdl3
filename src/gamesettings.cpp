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

#include "gamesettings.h"
#include "platform.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>

GameSettings *GameSettings::ptrInstance = nullptr;
bool virtualKeyState[CTRL_SC_COUNT] = {};
ControllerInput controllerInputs[5] = {};

void GameSettings::InitPrefPath() {
    if (!prefPath)
        prefPath = SDL_GetPrefPath("", "frozen-bubble");
}

GameSettings::~GameSettings() {
    iniparser_freedict(optDict);
}

void GameSettings::Dispose() {
    SaveSettings();
    // Same fix as HighscoreManager::Dispose(): the explicit destructor call ran
    // the cleanup but left the allocation behind.
    ptrInstance = nullptr;
    delete this;
}

int WriteToIni(dictionary *ini, const char *key, const char *value){
    int a = iniparser_set(ini, key, value);
    if (a != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not write %s %s to ini file!", key, value == NULL ? " header" : "");
    }
    return a;
}

// Accumulates failure rather than overwriting: each call previously assigned its
// own result to `a`, so only the *last* write's status survived and a failure
// partway through was erased by any later success (audit finding BUG-031).
#define EvalIniResult(a,ini,k,v) \
    do { int evalIniRv_ = WriteToIni(ini, k, v); if (evalIniRv_ < 0) (a) = evalIniRv_; } while (0)

bool EnsureDirectoryExists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true; // Directory already exists
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Path exists but is not a directory: %s", path);
            return false;
        }
    }

    // Directory doesn't exist, try to create it
#ifdef _WIN32
    if (mkdir(path) != 0) {
#else
    if (mkdir(path, 0755) != 0) {
#endif
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create directory %s: %s", path, strerror(errno));
        return false;
    }

    SDL_Log("Created configuration directory: %s", path);
    return true;
}

void GameSettings::CreateDefaultSettings()
{
    InitPrefPath();
    // Ensure configuration directory exists
    if (!EnsureDirectoryExists(prefPath)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot create configuration directory. Settings will not be saved.");
        return;
    }

    FILE *setFile;
    char setPath[256];
    int rval = 0;
    snprintf(setPath, sizeof(setPath), "%ssettings.ini", prefPath);

    if((setFile = fopen(setPath, "w")) == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create default settings file at %s: %s", setPath, strerror(errno));
        return;
    }
    fclose(setFile);

    dictionary *dict;
    dict = iniparser_load(setPath);

    while (rval == 0)
    {
        EvalIniResult(rval, dict, "GFX", NULL);
        EvalIniResult(rval, dict, "GFX:Quality", "1");
        EvalIniResult(rval, dict, "GFX:LinearScaling", "false");
        EvalIniResult(rval, dict, "GFX:Fullscreen", "false");
        EvalIniResult(rval, dict, "GFX:WindowWidth", "640");
        EvalIniResult(rval, dict, "GFX:WindowHeight", "480");
        EvalIniResult(rval, dict, "GFX:ColorblindBubbles", "false");
        EvalIniResult(rval, dict, "GFX:ShowFPS", "false");

        EvalIniResult(rval, dict, "Sound", NULL);
        EvalIniResult(rval, dict, "Sound:EnableMusic", "true");
        EvalIniResult(rval, dict, "Sound:EnableSFX", "true");
        EvalIniResult(rval, dict, "Sound:ClassicAF", "false");

        EvalIniResult(rval, dict, "Keys", NULL);
        char defaultSpeedBuf[16];
        snprintf(defaultSpeedBuf, sizeof(defaultSpeedBuf), "%.2f", DEFAULT_SPEED_MULTIPLIER);
        EvalIniResult(rval, dict, "Keys:SpeedMultiplier", defaultSpeedBuf);
#ifdef __WASM_PORT__
        EvalIniResult(rval, dict, "Keys:MouseEnabled", "true");
#else
        EvalIniResult(rval, dict, "Keys:MouseEnabled", "false");
#endif
        EvalIniResult(rval, dict, "Keys:Nickname", "");
        EvalIniResult(rval, dict, "Keys:P1Left", "80");      // SDL_SCANCODE_LEFT
        EvalIniResult(rval, dict, "Keys:P1Right", "79");     // SDL_SCANCODE_RIGHT
        EvalIniResult(rval, dict, "Keys:P1Fire", "82");      // SDL_SCANCODE_UP
        EvalIniResult(rval, dict, "Keys:P1Center", "81");    // SDL_SCANCODE_DOWN
        EvalIniResult(rval, dict, "Keys:P2Left", "27");      // SDL_SCANCODE_X
        EvalIniResult(rval, dict, "Keys:P2Right", "25");     // SDL_SCANCODE_V
        EvalIniResult(rval, dict, "Keys:P2Fire", "6");       // SDL_SCANCODE_C
        EvalIniResult(rval, dict, "Keys:P2Center", "7");     // SDL_SCANCODE_D

        // Leave the loop. Only promote to the "done" sentinel when nothing
        // failed — EvalIniResult now leaves rval negative if any write did, and
        // that has to survive to the check below, which was previously
        // unreachable because this line ran unconditionally (BUG-031).
        if (rval >= 0) rval = 1;
    }

    if (rval < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to populate default settings");
        iniparser_freedict(dict);
        return;
    }

    if((setFile = fopen(setPath, "w+")) == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not write default settings file at %s: %s", setPath, strerror(errno));
        iniparser_freedict(dict);
        return;
    }
    iniparser_dump_ini(dict, setFile);
    fclose(setFile);
    iniparser_freedict(dict);

    SDL_Log("Created default settings file at %s", setPath);
}

void GameSettings::ReadSettings()
{
    InitPrefPath();
    char setPath[256];
    snprintf(setPath, sizeof(setPath), "%ssettings.ini", prefPath);

    optDict = iniparser_load(setPath);

    // A missing or malformed settings file is repairable: write defaults and
    // reload. An unwritable preferences directory is not, and this used to
    // retry forever -- before any window exists, so the game simply appears to
    // hang instead of reporting a settings problem. Bound the attempts, then
    // carry on with an empty in-memory dictionary: every getter below supplies
    // its own default, so the game starts with default settings rather than not
    // at all.
    constexpr int kMaxSettingsRepairAttempts = 3;
    for (int attempt = 0; optDict == NULL && attempt < kMaxSettingsRepairAttempts; attempt++)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Settings file failed to load (or doesn't exist). Creating default fallback (attempt %d of %d)...",
                    attempt + 1, kMaxSettingsRepairAttempts);
        CreateDefaultSettings();
        optDict = iniparser_load(setPath);
    }
    if (optDict == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Could not read or create '%s' after %d attempts. Starting with default settings; "
                     "changes will not be saved.", setPath, kMaxSettingsRepairAttempts);
        optDict = dictionary_new(0);
    }

    gfxQuality = iniparser_getint(optDict, "GFX:Quality", 1);
    linearScaling = iniparser_getboolean(optDict, "GFX:LinearScaling", false);
    useFullscreen = iniparser_getboolean(optDict, "GFX:Fullscreen", false);
    windowWidth = iniparser_getint(optDict, "GFX:WindowWidth", 640);
    windowHeight = iniparser_getint(optDict, "GFX:WindowHeight", 480);
    colorblindBubbles = iniparser_getboolean(optDict, "GFX:ColorblindBubbles", false);
    showFps = iniparser_getboolean(optDict, "GFX:ShowFPS", false);
    if (gfxQuality > 3 || gfxQuality < 1) gfxQuality = 3;
    if (windowWidth < 640 || windowWidth > 9999) windowWidth = 640;
    if (windowHeight < 480 || windowHeight > 9999) windowHeight = 480;

    playMusic = iniparser_getboolean(optDict, "Sound:EnableMusic", true);
    playSfx = iniparser_getboolean(optDict, "Sound:EnableSFX", true);
    classicSound = iniparser_getboolean(optDict, "Sound:ClassicAF", false);

    speedMultiplier = (float)iniparser_getdouble(
        optDict, "Keys:SpeedMultiplier", DEFAULT_SPEED_MULTIPLIER);
    // NaN compares false against both bounds, so an ini value of "nan" passed
    // straight through these ordered clamps and propagated into deltaScale and
    // from there into every per-frame movement, where all comparisons against
    // it also fail (audit finding BUG-030). Test for NaN explicitly — the
    // self-comparison is the standard form and does not rely on <cmath>.
    if (!(speedMultiplier == speedMultiplier)) speedMultiplier = DEFAULT_SPEED_MULTIPLIER;
    if (speedMultiplier < 1.0f) speedMultiplier = 1.0f;
    if (speedMultiplier > 5.0f) speedMultiplier = 5.0f;

#ifndef __WASM_PORT__
    const char* nick = iniparser_getstring(optDict, "Keys:Nickname", "");
    if (nick) snprintf(savedNickname, sizeof(savedNickname), "%s", nick);
#endif

#ifdef __WASM_PORT__
    mouseEnabled = iniparser_getboolean(optDict, "Keys:MouseEnabled", true);
#else
    mouseEnabled = iniparser_getboolean(optDict, "Keys:MouseEnabled", false);
#endif

    LoadDefaultKeys();
}

// Key bindings were cast straight from the ini integer to SDL_Scancode with no
// validation, so a stored 99999 reached PlayerKeys and IsKeyPressed then indexed
// SDL's 512-entry keyboard array far out of bounds (audit finding BUG-028). Two
// ranges are legitimate: a real scancode, or one of the virtual controller
// scancodes IsVirtualScancode recognises. Anything else falls back to the
// binding's own default.
static SDL_Scancode LoadScancode(dictionary* dict, const char* key, SDL_Scancode fallback)
{
    const int raw = iniparser_getint(dict, key, (int)fallback);
    const bool realKey    = raw > 0 && raw < SDL_SCANCODE_COUNT;
    const bool virtualKey = raw >= CTRL_SC_BASE && raw < CTRL_SC_BASE + CTRL_SC_COUNT;
    if (!realKey && !virtualKey) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Ignoring out-of-range key binding %s=%d; using default %d",
                    key, raw, (int)fallback);
        return fallback;
    }
    return static_cast<SDL_Scancode>(raw);
}

void GameSettings::LoadDefaultKeys()
{
    player1Keys.left = LoadScancode(optDict, "Keys:P1Left", SDL_SCANCODE_LEFT);
    player1Keys.right = LoadScancode(optDict, "Keys:P1Right", SDL_SCANCODE_RIGHT);
    player1Keys.fire = LoadScancode(optDict, "Keys:P1Fire", SDL_SCANCODE_UP);
    player1Keys.center = LoadScancode(optDict, "Keys:P1Center", SDL_SCANCODE_DOWN);

    player2Keys.left = LoadScancode(optDict, "Keys:P2Left", SDL_SCANCODE_X);
    player2Keys.right = LoadScancode(optDict, "Keys:P2Right", SDL_SCANCODE_V);
    player2Keys.fire = LoadScancode(optDict, "Keys:P2Fire", SDL_SCANCODE_C);
    player2Keys.center = LoadScancode(optDict, "Keys:P2Center", SDL_SCANCODE_D);

    // P3: WASD
    player3Keys.left = LoadScancode(optDict, "Keys:P3Left", SDL_SCANCODE_A);
    player3Keys.right = LoadScancode(optDict, "Keys:P3Right", SDL_SCANCODE_D);
    player3Keys.fire = LoadScancode(optDict, "Keys:P3Fire", SDL_SCANCODE_W);
    player3Keys.center = LoadScancode(optDict, "Keys:P3Center", SDL_SCANCODE_S);

    // P4: IJKL
    player4Keys.left = LoadScancode(optDict, "Keys:P4Left", SDL_SCANCODE_J);
    player4Keys.right = LoadScancode(optDict, "Keys:P4Right", SDL_SCANCODE_L);
    player4Keys.fire = LoadScancode(optDict, "Keys:P4Fire", SDL_SCANCODE_I);
    player4Keys.center = LoadScancode(optDict, "Keys:P4Center", SDL_SCANCODE_K);

    // P5: Numpad 4/6/8/5
    player5Keys.left = LoadScancode(optDict, "Keys:P5Left", SDL_SCANCODE_KP_4);
    player5Keys.right = LoadScancode(optDict, "Keys:P5Right", SDL_SCANCODE_KP_6);
    player5Keys.fire = LoadScancode(optDict, "Keys:P5Fire", SDL_SCANCODE_KP_8);
    player5Keys.center = LoadScancode(optDict, "Keys:P5Center", SDL_SCANCODE_KP_5);
}

void GameSettings::SaveKeys()
{
    // Ensure the [Keys] section header exists in the dict before writing any keys.
    // iniparser_dump_ini only outputs keys under sections that have a header entry.
    iniparser_set(optDict, "Keys", NULL);

    char speedBuf[16];
    snprintf(speedBuf, sizeof(speedBuf), "%.2f", speedMultiplier);
    iniparser_set(optDict, "Keys:SpeedMultiplier", speedBuf);
    iniparser_set(optDict, "Keys:MouseEnabled", mouseEnabled ? "true" : "false");

#ifndef __WASM_PORT__
    iniparser_set(optDict, "Keys:Nickname", savedNickname);
#endif

    iniparser_set(optDict, "Keys:P1Left", std::to_string(player1Keys.left).c_str());
    iniparser_set(optDict, "Keys:P1Right", std::to_string(player1Keys.right).c_str());
    iniparser_set(optDict, "Keys:P1Fire", std::to_string(player1Keys.fire).c_str());
    iniparser_set(optDict, "Keys:P1Center", std::to_string(player1Keys.center).c_str());

    iniparser_set(optDict, "Keys:P2Left", std::to_string(player2Keys.left).c_str());
    iniparser_set(optDict, "Keys:P2Right", std::to_string(player2Keys.right).c_str());
    iniparser_set(optDict, "Keys:P2Fire", std::to_string(player2Keys.fire).c_str());
    iniparser_set(optDict, "Keys:P2Center", std::to_string(player2Keys.center).c_str());

    iniparser_set(optDict, "Keys:P3Left", std::to_string(player3Keys.left).c_str());
    iniparser_set(optDict, "Keys:P3Right", std::to_string(player3Keys.right).c_str());
    iniparser_set(optDict, "Keys:P3Fire", std::to_string(player3Keys.fire).c_str());
    iniparser_set(optDict, "Keys:P3Center", std::to_string(player3Keys.center).c_str());

    iniparser_set(optDict, "Keys:P4Left", std::to_string(player4Keys.left).c_str());
    iniparser_set(optDict, "Keys:P4Right", std::to_string(player4Keys.right).c_str());
    iniparser_set(optDict, "Keys:P4Fire", std::to_string(player4Keys.fire).c_str());
    iniparser_set(optDict, "Keys:P4Center", std::to_string(player4Keys.center).c_str());

    iniparser_set(optDict, "Keys:P5Left", std::to_string(player5Keys.left).c_str());
    iniparser_set(optDict, "Keys:P5Right", std::to_string(player5Keys.right).c_str());
    iniparser_set(optDict, "Keys:P5Fire", std::to_string(player5Keys.fire).c_str());
    iniparser_set(optDict, "Keys:P5Center", std::to_string(player5Keys.center).c_str());

    SaveSettings();
}

void GameSettings::SaveSettings()
{
    InitPrefPath();
    FILE *setFile;
    char setPath[256];
    char tempPath[256];
    snprintf(setPath, sizeof(setPath), "%ssettings.ini", prefPath);
    // Settings are now rewritten on every toggle, so the old truncate-in-place
    // save exposed the real file to a crash far more often. Build the new file
    // beside it and swap it in once it is complete.
    snprintf(tempPath, sizeof(tempPath), "%ssettings.ini.tmp", prefPath);

    if((setFile = fopen(tempPath, "w+")) == NULL)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not save settings to %s: %s", tempPath, strerror(errno));
        return;
    }
    iniparser_dump_ini(optDict, setFile);
    fclose(setFile);
    if (!ReplaceFileAtomically(tempPath, setPath)) return;
    RequestPersistentStorageFlush();
    SDL_Log("Settings saved to %s", setPath);
}

void GameSettings::setSoundEnabled(bool on) {
    playMusic = on;
    playSfx = on;
    iniparser_set(optDict, "Sound:EnableMusic", on ? "true" : "false");
    iniparser_set(optDict, "Sound:EnableSFX", on ? "true" : "false");
    SaveSettings();
}

void GameSettings::SetValue(const char* option, const char* value)
{
    //update runtime options
    if (strcmp(option, "GFX:Quality") == 0) {
        if (gfxQuality == 1) gfxQuality = 3;
        else gfxQuality--;

        // gfxQuality needs a hot reload
        iniparser_set(optDict, option, std::to_string(gfxQuality).c_str());
        SaveSettings();
        return;
    }
    else if (strcmp(option, "GFX:Fullscreen") == 0) {
        useFullscreen = !useFullscreen;
        iniparser_set(optDict, option, useFullscreen ? "true" : "false");
        SaveSettings();
        return;
    }
    else if (strcmp(option, "GFX:ShowFPS") == 0) {
        showFps = !showFps;
        iniparser_set(optDict, option, showFps ? "true" : "false");
        SaveSettings();
        return;
    }
    else if (strcmp(option, "GFX:ColorblindBubbles") == 0) {
        colorblindBubbles = (strcmp(value, "true") == 0);
        iniparser_set(optDict, option, value);
        SaveSettings();
        return;
    }

    //update ini file set
    iniparser_set(optDict, option, value);
    SaveSettings();
}
