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

#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

#include <SDL3/SDL.h>
#include <iniparser.h>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

// Virtual scancode base for controller button bindings.
// Virtual scancode = CTRL_SC_BASE + playerSlot * CTRL_SC_PER_PLAYER + button.
//
// The stride was a hardcoded 20, but SDL3 defines 26 gamepad buttons, so any
// button index of 20 or above aliased into the *next* player's slot — one
// player's controller pressing another player's bindings (audit finding
// BUG-036). Derive the stride from SDL so it cannot drift again.
#define CTRL_SC_BASE 300
#define CTRL_SC_PLAYERS 5
#define CTRL_SC_PER_PLAYER ((int)SDL_GAMEPAD_BUTTON_COUNT)
#define CTRL_SC_COUNT (CTRL_SC_PLAYERS * CTRL_SC_PER_PLAYER)

// Shared virtual key state written by frozenbubble.cpp, read by bubblegame.cpp.
// Index = (virtualScancode - CTRL_SC_BASE)
extern bool virtualKeyState[CTRL_SC_COUNT];

// Per-player controller input state written by frozenbubble.cpp HandleControllerEvent.
// bubblegame.cpp ORs these with keyboard state in UpdatePenguin.
struct ControllerInput {
    bool left = false, right = false, fire = false, center = false;
};
extern ControllerInput controllerInputs[5];

inline bool IsVirtualScancode(SDL_Scancode sc) {
    return sc >= CTRL_SC_BASE && sc < (SDL_Scancode)(CTRL_SC_BASE + CTRL_SC_COUNT);
}

// Build a virtual scancode for a controller slot, rejecting out-of-range input.
// Controller slots are assigned from a vector index that is never released, so a
// user who plugs and unplugs pads can reach slot 5+ — which previously produced
// a scancode past virtualKeyState[], and from slot 11 past SDL's 512-entry
// keyboard array (audit finding BUG-035). Returns false rather than producing an
// out-of-bounds index.
inline SDL_Scancode VirtualScancode(int playerSlot, int button) {
    if (playerSlot < 0 || playerSlot >= CTRL_SC_PLAYERS) return SDL_SCANCODE_UNKNOWN;
    if (button < 0 || button >= CTRL_SC_PER_PLAYER) return SDL_SCANCODE_UNKNOWN;
    return (SDL_Scancode)(CTRL_SC_BASE + playerSlot * CTRL_SC_PER_PLAYER + button);
}

// Poll either real keyboard or virtual (controller) key state
inline bool IsKeyPressed(SDL_Scancode sc) {
    if (IsVirtualScancode(sc))
        return virtualKeyState[sc - CTRL_SC_BASE];
    return SDL_GetKeyboardState(NULL)[sc] != 0;
}

struct PlayerKeys {
    SDL_Scancode left, right, fire, center;
};

// A server the player asked to be notified about. Marking one as followed
// registers this device's push token with that server, so it can notify us
// when somebody joins -- including, and mainly, while the app is closed.
struct FollowedServer {
    std::string host;
    int port = 0;
    std::string label;   // display name; falls back to host:port when empty
};

class GameSettings final
{
public:
    void InitPrefPath();
    void ReadSettings();
    void SaveSettings();
    void SetValue(const char *option, const char *value);
    void GetValue();

    const char *prefPath = nullptr; // Initialized lazily via InitPrefPath() after SDL is ready
    int gfxLevel() { return gfxQuality; }
    SDL_Point curResolution() { return {windowWidth, windowHeight}; }
    bool fullscreenMode() { return useFullscreen; }
    bool linearScaling = false;
    bool canPlayMusic() { return playMusic; }
    bool canPlaySFX() { return playSfx; }
    bool useClassicAudio() { return classicSound; }
    bool soundEnabled() { return playMusic && playSfx; }
    void setSoundEnabled(bool on);
    bool colorBlind() { return colorblindBubbles; }
    bool showFpsOverlay() { return showFps; }

    PlayerKeys player1Keys, player2Keys, player3Keys, player4Keys, player5Keys;
    void LoadDefaultKeys();
    void SaveKeys();

    // Restores every setting to its factory value. Rewrites settings.ini from
    // the same code that seeds a first run and reloads it, so the file and the
    // in-memory state cannot end up disagreeing. Callers are responsible for
    // re-applying anything already pushed into a subsystem -- audio mute state
    // and the window's fullscreen flag do not re-read this on their own.
    void ResetToDefaults();

    // Whether mouse/touch aim is on out of the box. Decided at runtime rather
    // than by #ifdef because a single Android APK serves both TV boxes and
    // phones, and the right answer differs between them. Only the *default* is
    // affected: a stored preference always wins, and keyboard and controller
    // aiming keep working whatever this returns.
    static bool DefaultMouseEnabled();

    // Game speed multiplier (1.0–5.0). Persisted per device.
#if defined(__ANDROID__) || defined(__ANDROID_PORT__)
    static constexpr float DEFAULT_SPEED_MULTIPLIER = 2.0f;
#else
    static constexpr float DEFAULT_SPEED_MULTIPLIER = 3.0f;
#endif
    float speedMultiplier = DEFAULT_SPEED_MULTIPLIER;

    char savedNickname[32] = "";
    bool mouseEnabled = false;  // Mouse/touchscreen aim+fire (player 1)

    // Room-creation ("host") defaults for network games: the game rules a
    // host last configured (chain reactions, attack mode, victories limit,
    // etc.), so the next room this device creates starts from what was last
    // used instead of MainMenu's hardcoded defaults. Loaded by ReadSettings();
    // MainMenu calls SaveHostSettings() every time the host changes one,
    // alongside the SendOptions() call that pushes it to the room's joiners.
    // AttackMode is stored as a plain int (its own underlying type) rather
    // than pulling attackmode.h into this header for one field.
    bool hostChainReactions = true;
    bool hostSinglePlayerTargetting = true;
    int hostVictoriesLimitIndex = 5;
    bool hostClearMode = false;
    int hostAttackMode = 0;
    bool hostTeamMode = false;
    int hostTeamCount = 5;
    int hostBotSkill = 1;
    int hostRoomSizeChoice = 2;
    void SaveHostSettings(bool chainReactions, bool singlePlayerTargetting,
                          int victoriesLimitIndex, bool clearMode, int attackMode,
                          bool teamMode, int teamCount, int botSkill, int roomSizeChoice);

    // Servers the player follows for join notifications. Bounded rather than
    // unbounded because the ini format here stores fixed numbered slots (the
    // same shape as the P1-P5 key bindings above) and because a device that
    // registers with dozens of servers is far more likely to be a mistake than
    // an intent.
    static constexpr int kMaxFollowedServers = 8;
    std::vector<FollowedServer> followedServers;

    void LoadFollowedServers();
    void SaveFollowedServers();
    bool IsServerFollowed(const std::string& host, int port) const;
    // Adds if absent, removes if present. Returns true if the server is
    // followed afterwards. Refuses to add beyond kMaxFollowedServers (returns
    // false without changing anything).
    bool ToggleServerFollowed(const std::string& host, int port,
                              const std::string& label);

    // Players whose chat this device hides. Purely local and purely cosmetic:
    // there is no account system, so a nick is not a durable identity and
    // blocking one cannot be enforced server-side or stop that person playing.
    // What it does do is let someone shut up an abusive player immediately,
    // without waiting on an operator -- which is the part a player actually
    // controls. Bounded for the same reason as followedServers: fixed numbered
    // ini slots, and a list in the hundreds is a bug rather than an intent.
    static constexpr int kMaxBlockedPlayers = 32;

    // The server truncates nicks to 10 characters (see the NICK handler in
    // server/game.c), so that is the longest nick this device will ever be
    // asked to match against. Storing a longer one than the server will ever
    // report back means the block can never fire -- the UI would confirm it
    // and nothing would happen.
    static constexpr size_t kMaxNickLength = 10;

    std::vector<std::string> blockedPlayers;

    void LoadBlockedPlayers();
    void SaveBlockedPlayers();
    bool IsPlayerBlocked(const std::string& nick) const;

    // Nick comparison for blocking, case-insensitive over ASCII.
    //
    // The server's own uniqueness check is case-sensitive, so "Alice" and
    // "alice" can be connected at the same time as two different people --
    // which made a case-sensitive block a one-keystroke bypass: reconnect with
    // the case flipped and you are visible again, while the blocker's UI still
    // says you are blocked. The cost is that a genuinely different "alice"
    // cannot be left visible while "Alice" is blocked, which is much the
    // better trade: nicks are not identities here anyway.
    static bool NicksEqual(const std::string& a, const std::string& b);

    // Trims surrounding whitespace and clamps to kMaxNickLength, so what gets
    // stored is something the server could actually send back.
    static std::string NormalizeNick(const std::string& nick);
    // Adds if absent, removes if present. Returns true if the player is
    // blocked afterwards. Refuses to add beyond kMaxBlockedPlayers (returns
    // false without changing anything).
    bool ToggleBlockedPlayer(const std::string& nick);

    GameSettings(const GameSettings& obj) = delete;
    void Dispose();
    static GameSettings* Instance(){
        if (!ptrInstance)
            ptrInstance = new GameSettings();
        return ptrInstance;
    };
private:
    void CreateDefaultSettings();
    dictionary *optDict;

    // Every one of these is written by ReadSettings() on a normal start, but
    // nothing guarantees that ran: a test harness constructs the singleton
    // directly, and a malformed or partial settings.ini leaves individual
    // keys untouched. Reading an uninitialised bool is undefined behaviour --
    // UBSan flags it as "load of value 190, which is not a valid value for
    // type 'bool'" -- and an indeterminate gfxQuality silently changes which
    // rendering path the game takes. Defaults here match CreateDefaultSettings().
    int gfxQuality = 1, windowWidth = 640, windowHeight = 480;
    bool useFullscreen = false, colorblindBubbles = false;
    bool playMusic = true, playSfx = true, classicSound = false;
    bool showFps = false;

    GameSettings(){};
    ~GameSettings();
    static std::mutex mtx;
    static GameSettings* ptrInstance;
};

#endif //GAMESETTINGS_H
