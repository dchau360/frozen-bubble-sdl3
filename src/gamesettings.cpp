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

#include "gamesettings.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>

GameSettings *GameSettings::ptrInstance = new GameSettings();
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
    this->~GameSettings();
}

int WriteToIni(dictionary *ini, const char *key, const char *value){
    int a = iniparser_set(ini, key, value);
    if (a != 0) {
        SDL_LogWarn(1, "Could not write %s %s to ini file!", key, value == NULL ? " header" : "");
    }
    return a;
}

#define EvalIniResult(a,ini,k,v) a = WriteToIni(ini, k, v)

bool EnsureDirectoryExists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true; // Directory already exists
        } else {
            SDL_LogError(1, "Path exists but is not a directory: %s", path);
            return false;
        }
    }

    // Directory doesn't exist, try to create it
#ifdef _WIN32
    if (mkdir(path) != 0) {
#else
    if (mkdir(path, 0755) != 0) {
#endif
        SDL_LogError(1, "Failed to create directory %s: %s", path, strerror(errno));
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
        SDL_LogError(1, "Cannot create configuration directory. Settings will not be saved.");
        return;
    }

    FILE *setFile;
    char setPath[256];
    int rval = 0;
    snprintf(setPath, sizeof(setPath), "%ssettings.ini", prefPath);

    if((setFile = fopen(setPath, "w")) == NULL)
    {
        SDL_LogError(1, "Could not create default settings file at %s: %s", setPath, strerror(errno));
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

        EvalIniResult(rval, dict, "Sound", NULL);
        EvalIniResult(rval, dict, "Sound:EnableMusic", "true");
        EvalIniResult(rval, dict, "Sound:EnableSFX", "true");
        EvalIniResult(rval, dict, "Sound:ClassicAF", "false");

        EvalIniResult(rval, dict, "Keys", NULL);
#ifdef __ANDROID__
        EvalIniResult(rval, dict, "Keys:SpeedMultiplier", "1.25");
#else
        EvalIniResult(rval, dict, "Keys:SpeedMultiplier", "2.0");
#endif
        EvalIniResult(rval, dict, "Keys:P1Left", "80");      // SDL_SCANCODE_LEFT
        EvalIniResult(rval, dict, "Keys:P1Right", "79");     // SDL_SCANCODE_RIGHT
        EvalIniResult(rval, dict, "Keys:P1Fire", "82");      // SDL_SCANCODE_UP
        EvalIniResult(rval, dict, "Keys:P1Center", "81");    // SDL_SCANCODE_DOWN
        EvalIniResult(rval, dict, "Keys:P2Left", "27");      // SDL_SCANCODE_X
        EvalIniResult(rval, dict, "Keys:P2Right", "25");     // SDL_SCANCODE_V
        EvalIniResult(rval, dict, "Keys:P2Fire", "6");       // SDL_SCANCODE_C
        EvalIniResult(rval, dict, "Keys:P2Center", "7");     // SDL_SCANCODE_D

        //break while
        rval = 1;
    }

    if (rval < 0) {
        SDL_LogError(1, "Failed to populate default settings");
        iniparser_freedict(dict);
        return;
    }

    if((setFile = fopen(setPath, "w+")) == NULL)
    {
        SDL_LogError(1, "Could not write default settings file at %s: %s", setPath, strerror(errno));
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

    while (optDict == NULL)
    {
        SDL_LogWarn(1, "Settings file failed to load (or doesn't exist). Creating default fallback...");
        CreateDefaultSettings();
        optDict = iniparser_load(setPath);
    }

    gfxQuality = iniparser_getint(optDict, "GFX:Quality", 1);
    linearScaling = iniparser_getboolean(optDict, "GFX:LinearScaling", false);
    useFullscreen = iniparser_getboolean(optDict, "GFX:Fullscreen", false);
    windowWidth = iniparser_getint(optDict, "GFX:WindowWidth", 640);
    windowHeight = iniparser_getint(optDict, "GFX:WindowHeight", 480);
    colorblindBubbles = iniparser_getboolean(optDict, "GFX:ColorblindBubbles", false);
    if (gfxQuality > 3 || gfxQuality < 1) gfxQuality = 3;
    if (windowWidth < 640 || windowWidth > 9999) windowWidth = 640;
    if (windowHeight < 480 || windowWidth > 9999) windowHeight = 480;

    playMusic = iniparser_getboolean(optDict, "Sound:EnableMusic", true);
    playSfx = iniparser_getboolean(optDict, "Sound:EnableSFX", true);
    classicSound = iniparser_getboolean(optDict, "Sound:ClassicAF", false);

#ifdef __ANDROID__
    speedMultiplier = (float)iniparser_getdouble(optDict, "Keys:SpeedMultiplier", 1.25);
#else
    speedMultiplier = (float)iniparser_getdouble(optDict, "Keys:SpeedMultiplier", 2.0);
#endif
    if (speedMultiplier < 1.0f) speedMultiplier = 1.0f;
    if (speedMultiplier > 5.0f) speedMultiplier = 5.0f;

    LoadDefaultKeys();
}

void GameSettings::LoadDefaultKeys()
{
    player1Keys.left = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P1Left", SDL_SCANCODE_LEFT));
    player1Keys.right = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P1Right", SDL_SCANCODE_RIGHT));
    player1Keys.fire = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P1Fire", SDL_SCANCODE_UP));
    player1Keys.center = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P1Center", SDL_SCANCODE_DOWN));

    player2Keys.left = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P2Left", SDL_SCANCODE_X));
    player2Keys.right = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P2Right", SDL_SCANCODE_V));
    player2Keys.fire = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P2Fire", SDL_SCANCODE_C));
    player2Keys.center = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P2Center", SDL_SCANCODE_D));

    // P3: WASD
    player3Keys.left = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P3Left", SDL_SCANCODE_A));
    player3Keys.right = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P3Right", SDL_SCANCODE_D));
    player3Keys.fire = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P3Fire", SDL_SCANCODE_W));
    player3Keys.center = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P3Center", SDL_SCANCODE_S));

    // P4: IJKL
    player4Keys.left = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P4Left", SDL_SCANCODE_J));
    player4Keys.right = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P4Right", SDL_SCANCODE_L));
    player4Keys.fire = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P4Fire", SDL_SCANCODE_I));
    player4Keys.center = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P4Center", SDL_SCANCODE_K));

    // P5: Numpad 4/6/8/5
    player5Keys.left = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P5Left", SDL_SCANCODE_KP_4));
    player5Keys.right = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P5Right", SDL_SCANCODE_KP_6));
    player5Keys.fire = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P5Fire", SDL_SCANCODE_KP_8));
    player5Keys.center = static_cast<SDL_Scancode>(iniparser_getint(optDict, "Keys:P5Center", SDL_SCANCODE_KP_5));
}

void GameSettings::SaveKeys()
{
    char speedBuf[16];
    snprintf(speedBuf, sizeof(speedBuf), "%.2f", speedMultiplier);
    iniparser_set(optDict, "Keys:SpeedMultiplier", speedBuf);

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
    snprintf(setPath, sizeof(setPath), "%ssettings.ini", prefPath);

    if((setFile = fopen(setPath, "w+")) == NULL)
    {
        SDL_LogWarn(1, "Could not save settings to %s: %s", setPath, strerror(errno));
        return;
    }
    iniparser_dump_ini(optDict, setFile);
    fclose(setFile);
    SDL_Log("Settings saved to %s", setPath);
}

void GameSettings::SetValue(const char* option, const char* value)
{
    //update runtime options
    if (strcmp(option, "GFX:Quality") == 0) {
        if (gfxQuality == 1) gfxQuality = 3;
        else gfxQuality--;

        // gfxQuality needs a hot reload
        iniparser_set(optDict, option, std::to_string(gfxQuality).c_str());
        return;
    }
    else if (strcmp(option, "GFX:Fullscreen") == 0) {
        useFullscreen = !useFullscreen;
        iniparser_set(optDict, option, useFullscreen ? "true" : "false");
        return;
    }
    else if (strcmp(option, "GFX:ColorblindBubbles") == 0) {
        colorblindBubbles = (strcmp(value, "true") == 0);
        iniparser_set(optDict, option, value);
        return;
    }

    //update ini file set
    iniparser_set(optDict, option, value);
}