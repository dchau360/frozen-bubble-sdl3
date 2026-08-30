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

#ifndef FROZENBUBBLE_H
#define FROZENBUBBLE_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdint.h>
#include <string>
#include <iostream>

#include "gamesettings.h"
#include "audiomixer.h"

#include "highscoremanager.h"

#include "mainmenu.h"
#include "bubblegame.h"

#include "ttftext.h"

enum GameState {
    TitleScreen = 0,
    MainGame = 1,
    LevelEditor = 2,
    Netplay = 3,
    Highscores = 4
};

// What a finger-up's overall movement on a menu panel means, before any of
// it has been turned into a key press.
enum class MenuSwipeGesture { None, Back, Up, Down };

// Classifies a completed touch as a deliberate swipe (go back, move the
// selection) or a plain tap that happened to drift a little while the
// finger was down -- the two look identical as raw (dx, dy); only how far
// they travelled tells them apart.
//
// `onSteppedRow` overrides that distance test to None. A row stepped by
// which half a second tap lands on (MainMenu::IsSteppedRowAt --
// PanelTapRow::splitAdjust; Game speed, Victories limit, bot count, ...)
// needs its *left* half tapped to decrease, and that is exactly the
// direction "swipe left to go back" also claims at a mere 40-unit
// threshold -- one an ordinary tap can cross by drifting a little, especially
// on a narrow phone where a run of device points maps to more than one
// logical unit. There is no equivalent conflict on the right half, which is
// why decreasing a stepped row read as broken on a touchscreen while
// increasing did not: every tap that landed a bit left of where it started
// was read as "go back" before HandlePanelTap ever saw it.
MenuSwipeGesture ClassifyMenuSwipe(float dx, float dy, bool onSteppedRow);

// The window (in SDL_GetTicks() milliseconds) within which two FINGER_UP
// events are treated as one physical release rather than two independent
// taps -- guards against a multi-finger touch or an OS double-event
// re-firing FINGER_UP for what was really one release.
constexpr Uint32 kMenuTapDebounceMs = 200;
bool IsWithinMenuTapDebounce(Uint32 nowMs, Uint32 lastMenuTapMs);

// In a WASM browser build, where SDL_TOUCH_MOUSEID can't be trusted to tell
// a synthesized click from a real one, Emscripten fires a MOUSE_BUTTON_DOWN
// for every real touch's own FINGER_UP -- a moment after it, almost always,
// but "almost always" is not "always": on a busy frame (rendering a full
// screen, GC, whatever else the browser's main thread is doing that tick)
// the echo can lag past any fixed window. A HandleInput that dispatches the
// FINGER_UP tap and then waits a fixed number of milliseconds for that echo
// (the older approach) drops the guard the moment the echo is late, letting
// it dispatch as a second, independent tap -- reactivating whatever row the
// first tap just selected. Found live: the network room's Bots row
// occasionally re-incrementing itself with no further tap, intermittently
// rather than every time, which is exactly what a delayed echo slipping
// past a fixed window looks like.
//
// This tracks the actual state instead of a clock: "does the browser still
// owe this tap its echo". An arbitrarily late echo is still recognized
// whenever it shows up, not just within some window -- and a genuine
// independent mouse click (no touch involved, e.g. a desktop browser)
// never sets `awaiting`, so it always passes through untouched.
struct WasmMouseEchoGuard {
    bool awaiting = false;
    // A new touch beginning means the browser was never going to deliver
    // the previous tap's echo anymore (should never actually be pending
    // this long -- a real device fires it promptly after its own
    // FINGER_UP -- but a leftover flag must not survive to wrongly
    // swallow this new tap's own, unrelated echo).
    void OnFingerDown() { awaiting = false; }
    // Every FINGER_UP that reaches menu dispatch earns the browser's own
    // MOUSE_BUTTON_DOWN echo a moment later, however delayed.
    void OnFingerUpDispatched() { awaiting = true; }
    // Call once per MOUSE_BUTTON_DOWN. True means: this is that echo,
    // consume it and do not dispatch it as an independent tap.
    bool ShouldSwallowMouseDown() {
        if (!awaiting) return false;
        awaiting = false;
        return true;
    }
};

class FrozenBubble
{
public:
    int totalBubbles = 0;
    Uint32 startTime = 0;

    // Delta-time scale: 1.0 at 60 fps, 2.0 at 30 fps, etc.
    // All per-frame movement amounts are multiplied by this so the game
    // runs at the same real-time speed regardless of frame rate.
    float deltaScale = 1.0f;
    void CallMenuReturn() { mainMenu->ReturnToMenu(); };
    void CallNetLobbyReturn() { mainMenu->ReturnToNetLobby(); };
    void CallGameQuit() { IsGameQuit = true; };
    void CallGamePause() { IsGamePause = !IsGamePause; };

    uint8_t RunForEver();
    void RunOneFrame();
    GameState currentState = TitleScreen;

    FrozenBubble(const FrozenBubble& obj) = delete;
    static FrozenBubble* Instance();
    BubbleGame* bubbleGame() { return mainGame; };
private:
    int addictedTime = 0;
    bool IsGameQuit = false, IsGamePause = false;
    Uint32 lastBackPressTime = 0;

    // Per-controller state: joystick instance ID + axis hold flags
    struct ControllerState {
        SDL_JoystickID id = 0;
        // The open handle, so it can actually be closed on removal. It was
        // previously opened and dropped, leaking one gamepad per hotplug.
        // A slot with pad == nullptr is free and gets reused by the next
        // controller, which keeps the vector index (the player slot) stable for
        // everyone else instead of growing past the 5 slots bindings exist for.
        SDL_Gamepad *pad = nullptr;
        bool axisLeftHeld = false, axisRightHeld = false;
        bool axisUpHeld   = false, axisDownHeld  = false;
    };
    std::vector<ControllerState> controllers;

    void HandleControllerEvent(SDL_Event *e);
    static void PushKey(SDL_Keycode key, bool down);
    static void PushScancode(SDL_Scancode sc, bool down, bool skipEvent = false);
    // Initialized here rather than only in the constructor: the constructor has
    // early-return paths (a failed SDL init, a window or renderer that will not
    // create), and RunForEver and ~FrozenBubble still run afterwards and
    // dereference these. Without initializers those paths read indeterminate
    // pointers; nullptr at least makes the guards and SDL_Destroy* calls safe.
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;

    GameSettings *gameOptions = nullptr;
    AudioMixer *audMixer = nullptr;

    MainMenu *mainMenu = nullptr;
    BubbleGame *mainGame = nullptr;

    HighscoreManager *hiscoreManager = nullptr;

    void HandleInput(SDL_Event *e);

    // Maps a finger event onto the 640x480 logical canvas. SDL normalizes
    // tfinger.x/y against the *window*, not the canvas, so scaling them by
    // 640/480 is only correct when the window happens to be exactly 4:3 --
    // any other aspect leaves letterbox bars that the naive form counts as
    // playfield, displacing every tap by the width of a bar. Phones are never
    // 4:3, and in portrait the error is larger than the canvas is tall.
    void TouchToLogical(const SDL_Event *e, float *lx, float *ly) const;

    // ---- F3 performance overlay ----------------------------------------
    // Toggled by F3, persisted as GFX:ShowFPS. Draws bottom-right over every
    // screen. Reports frame pacing *and* effective game speed, because the two
    // fail independently: a wrong speedMultiplier changes speed while fps stays
    // a healthy 60, and a broken frame limiter changes fps while the intended
    // speed is preserved by deltaScale. One number cannot show both.
    TTFText fpsText;
    void AccumulateFrameStats(float elapsedMs);
    void RenderFpsOverlay();

    unsigned int fpsWindowStart = 0;   // ms, start of the current sampling window
    int   fpsFrames = 0;               // frames counted in this window
    float fpsSumMs = 0.0f;             // wall time accumulated in this window
    float fpsMinMs = 0.0f;             // shortest / longest frame seen, to expose
    float fpsMaxMs = 0.0f;             //   pacing jitter that an average hides
    float fpsSumDelta = 0.0f;          // summed deltaScale, for effective speed
    std::string fpsOverlayText;        // last formatted overlay string

    // Frame timing — used by RunOneFrame (persist across calls in WASM)
    unsigned int frameTicks = 0;
    unsigned int frameLastTick = 0;
    float frameTime = 1000.0f / 60.0f;
    // Absolute time the current frame should end, advanced by frameTime each
    // frame. Kept in float so the fractional part of a 60 fps frame is not lost.
    // Native only; WASM is paced by requestAnimationFrame.
    float frameDeadline = 0.0f;

    static FrozenBubble* ptrInstance;
    FrozenBubble();
    ~FrozenBubble();
};

#endif // FROZENBUBBLE_H
