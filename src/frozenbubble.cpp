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

#include "frozenbubble.h"
#include "logger.h"
#include "platform.h"
#include <sys/stat.h>

FrozenBubble *FrozenBubble::ptrInstance = NULL;

// Helper function to verify asset directory exists
bool VerifyAssetDirectory(const char* dataDir) {
    struct stat st;
    if (stat(dataDir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Asset directory not found: %s\n"
            "Please ensure the game is installed correctly and DATA_DIR is set properly.",
            dataDir);
        return false;
    }
    return true;
}

const char *formatTime(int time){
    int h = int(time/3600.0);
    int m = int((time-h*3600)/60.0);
    int s = int((time-h*3600)-(m*60));

    static char fm[128];
    size_t offset = 0;
    fm[0] = '\0';
    if (h > 0) {
        offset += snprintf(fm + offset, 128 - offset, "%dh ", h);
    }
    if (m > 0) {
        if (h > 0) {
            offset += snprintf(fm + offset, 128 - offset, "%02dm ", m);
        } else {
            offset += snprintf(fm + offset, 128 - offset, "%dm ", m);
        }
    }
    if (s > 0) {
        if (m > 0) {
            snprintf(fm + offset, 128 - offset, "%02ds", s);
        } else {
            snprintf(fm + offset, 128 - offset, "%ds", s);
        }
    }
    return fm;
}

FrozenBubble *FrozenBubble::Instance()
{
    if(ptrInstance == NULL)
        ptrInstance = new FrozenBubble();
    return ptrInstance;
}

FrozenBubble::FrozenBubble() {
    // Initialize logger first so all subsequent logs are captured
    // Determine log file name based on existing logs (creator, joiner1-4)
    // Supports up to 5 players total
    const char* logFilename = nullptr;
    struct stat st;

    if (stat("frozen-bubble-creator.log", &st) != 0) {
        // creator.log doesn't exist - this is the creator
        logFilename = "frozen-bubble-creator.log";
    } else if (stat("frozen-bubble-joiner1.log", &st) != 0) {
        // creator.log exists but joiner1.log doesn't - this is joiner1
        logFilename = "frozen-bubble-joiner1.log";
    } else if (stat("frozen-bubble-joiner2.log", &st) != 0) {
        // joiner1.log exists but joiner2.log doesn't - this is joiner2
        logFilename = "frozen-bubble-joiner2.log";
    } else if (stat("frozen-bubble-joiner3.log", &st) != 0) {
        // joiner2.log exists but joiner3.log doesn't - this is joiner3
        logFilename = "frozen-bubble-joiner3.log";
    } else {
        // All other logs exist - this is joiner4 (5th player)
        logFilename = "frozen-bubble-joiner4.log";
    }

#ifdef __IOS_PORT__
    // The names above let several desktop clients started from one directory keep
    // separate logs. iOS runs a single instance, and its working directory is the
    // app bundle: writable in the simulator, read-only and signed on a device, so
    // a relative path logs fine under test and then silently stops on hardware.
    // Use the same writable container the settings file already goes to.
    std::string iosLogPath;
    if (const char* pref = SDL_GetPrefPath("", "frozen-bubble")) {
        iosLogPath = std::string(pref) + "frozen-bubble.log";
        logFilename = iosLogPath.c_str();
    }
#endif

    Logger::Initialize(logFilename);

    // Verify asset directory exists before proceeding
#ifndef __ANDROID__
    LogDataDir();
    if (!VerifyAssetDirectory(g_dataDir.c_str())) {
        std::string msg = "Could not find game assets at: " + g_dataDir + "\n\nPlease ensure the game is installed correctly.";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Asset Directory Missing",
            msg.c_str(),
            NULL);
        IsGameQuit = true;
        return;
    }
#endif

    // Init video and audio. On Emscripten, the browser's AudioContext starts
    // suspended until a user gesture; SDL3's emscripten audio driver handles
    // resuming it automatically via navigator.userActivation.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        IsGameQuit = true;
        return;
    }

    gameOptions = GameSettings::Instance();
    gameOptions->ReadSettings();

    SDL_Point resolution = gameOptions->curResolution();
#ifdef __WASM_PORT__
    // Fullscreen via SDL window flags causes a black canvas in browsers.
    // The CSS shell scales the 640x480 canvas to fit the viewport instead.
    SDL_WindowFlags fullscreen = 0;
#else
    SDL_WindowFlags fullscreen = gameOptions->fullscreenMode() ? SDL_WINDOW_FULLSCREEN : 0;
#endif

#if defined(__IOS_PORT__) || defined(__ANDROID__)
    // Screen orientation, decided here rather than in the manifest or the
    // Info.plist, because neither can express it. Must be set before the window
    // is created: both backends read this hint while creating it.
    //
    // On iOS the plist alone is not enough -- SDL intersects the plist's
    // orientations with a mask of its own, derived from the requested window
    // size whenever this hint is unset. The window is 640x480, so SDL decides
    // landscape-only by itself and the plist never gets a say.
    //
    // On Android a single APK serves TV boxes, phones and tablets, so the
    // manifest cannot pick per device; SDL's setRequestedOrientation at
    // window creation overrides whatever the manifest asked for. A TV is
    // landscape hardware and stays landscape. A phone gets portrait, which
    // is how it is already being held. A tablet is screen-large enough that
    // DeviceHasTouchscreen() alone cannot tell it apart from a phone (both
    // report a touchscreen), but it is normally held either way rather than
    // fixed in portrait like a phone -- AndroidIsTablet() checked first
    // gives it the TV's landscape treatment instead of falling into the
    // phone branch and locking out rotation entirely.
    //
    // Naming both orientations would not give a phone free rotation anyway:
    // for a non-resizable window SDL breaks the tie by aspect (see
    // SDLActivity.setOrientationBis), and 640x480 is wider than tall, so
    // "Portrait LandscapeLeft LandscapeRight" resolves right back to landscape.
#if defined(__ANDROID__)
    if (AndroidIsTablet()) {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    } else if (DeviceHasTouchscreen()) {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
    } else {
        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    }
#else
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait");
#endif
#endif

    window = SDL_CreateWindow("Frozen-Bubble: SDL3", resolution.x, resolution.y, fullscreen);
    // SDL3: texture scale mode is set per-texture, not globally via hint.
    // Linear scaling will be applied when textures are created.

    if(!window) {
        IsGameQuit = true;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s", SDL_GetError());
    }

    SDL_Surface *icon = SDL_LoadBMP(ASSET("/gfx/pinguins/window_icon_penguin.bmp").c_str());
    if (icon) {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load window icon: %s", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, NULL);
    // Suppress SDL_LOG_CATEGORY_ERROR debug-level messages (SDL Metal renderer emits
    // "Parameter 'texture' is invalid" at DEBUG priority during internal initialization).
    SDL_SetLogPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_WARN);

#ifdef __IOS_PORT__
    // Ask for notification permission and start APNs registration. Done after
    // the window exists so SDL's application delegate -- the one the APNs
    // callbacks are grafted onto -- is already in place. Entirely asynchronous:
    // the token turns up later if it turns up at all, and nothing waits for it.
    IosRegisterForPush();
#endif

#ifndef __WASM_PORT__
    // Present in step with the display instead of free-running on a timer. SDL3
    // defaults this off. Without it the frame limiter in RunOneFrame holds 60 fps
    // by the clock, which drifts against a panel refreshing at ~59.94 Hz and drops
    // or doubles a frame every few seconds. The limiter stays in place regardless:
    // vsync is a request, and some drivers, remote-desktop sessions and headless
    // backends silently ignore it, which would otherwise leave the loop uncapped.
    // Native only — the browser is already paced by requestAnimationFrame, and
    // stacking a second sync source on top of it would only add latency.
    if (renderer && !SDL_SetRenderVSync(renderer, 1)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "VSync unavailable, falling back to the frame limiter: %s", SDL_GetError());
    }
#endif
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    if(!renderer) {
        IsGameQuit = true;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create renderer: %s", SDL_GetError());
    }

    if( !TTF_Init() )
    {
        IsGameQuit = true;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL_ttf: %s", SDL_GetError());
    }

    audMixer = AudioMixer::Instance();
    hiscoreManager = HighscoreManager::Instance(renderer);
    init_effects((char*)g_dataDir.c_str());
    mainMenu = new MainMenu(renderer);
    mainGame = new BubbleGame(renderer);

    // F3 overlay font. White on black so UpdateText's built-in 1px shadow blit
    // keeps it legible over both the pale menu art and the dark playfield.
    fpsText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 12);
    fpsText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    // Initialize game controller support.
    // SDL_INIT_JOYSTICK is needed explicitly on Emscripten (browser Gamepad API)
    // before SDL_INIT_GAMECONTROLLER can detect devices.
    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD);
    int numGamepads = 0;
    SDL_JoystickID *gamepadIds = SDL_GetGamepads(&numGamepads);
    if (gamepadIds) {
        for (int i = 0; i < numGamepads; i++) {
            SDL_Gamepad *gp = SDL_OpenGamepad(gamepadIds[i]);
            if (gp) {
                SDL_JoystickID id = gamepadIds[i];
                ControllerState cs;
                cs.id = id;
                controllers.push_back(cs);
                SDL_Log("Opened controller %d: %s", (int)controllers.size(), SDL_GetGamepadName(gp));
            }
        }
        SDL_free(gamepadIds);
    }
}

FrozenBubble::~FrozenBubble() {
    if(renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if(window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    // Any of these can still be null: the constructor returns early when SDL,
    // the window or the renderer fails, and this destructor runs regardless.
    if (hiscoreManager) hiscoreManager->Dispose();
    if (audMixer) audMixer->Dispose();
    if (gameOptions) gameOptions->Dispose();

    TTF_Quit();
    SDL_Quit();

    // Shutdown logger last to capture all cleanup logs
    Logger::Shutdown();
}

#ifdef __WASM_PORT__
#include <emscripten.h>
static void wasm_one_frame() {
    FrozenBubble* fb = FrozenBubble::Instance();
    if (fb) fb->RunOneFrame();
}
#endif

uint8_t FrozenBubble::RunForEver()
{
    if(currentState == TitleScreen) audMixer->PlayMusic("intro");

    frameLastTick = SDL_GetTicks();
    frameTicks    = frameLastTick;
    frameDeadline = (float)frameLastTick;

    SDL_Log("RunForEver: starting loop");

#ifdef __WASM_PORT__
    // Emscripten: hand control back to the browser and call RunOneFrame each tick.
    // fps=0 uses requestAnimationFrame (matches display refresh rate).
    emscripten_set_main_loop(wasm_one_frame, 0, 0);
    return 0;
#else
    while(!IsGameQuit) {
        RunOneFrame();
    }
    if (startTime != 0) addictedTime += SDL_GetTicks() - startTime;
    if(addictedTime != 0) printf("Addicted for %s, %d bubbles were launched.", formatTime(addictedTime/1000), totalBubbles);
    // Runs the same shutdown as before and also frees the singleton. Safe here
    // because nothing touches the object afterwards: this returns straight to
    // main(), which only uses the return value.
    ptrInstance = nullptr;
    delete this;
    return 0;
#endif
}

// Fold one frame into the current sampling window and, twice a second, reformat
// the overlay string. Sampling over a window rather than per frame keeps the
// numbers readable; min/max are carried alongside the average because the
// average alone hides alternating short/long frames entirely — a loop delivering
// 2ms and 17ms frames in pairs and one delivering a steady 9.5ms both average
// 9.5ms, and only one of them looks correct on screen.
void FrozenBubble::AccumulateFrameStats(float elapsedMs)
{
    if (fpsFrames == 0) {
        fpsMinMs = elapsedMs;
        fpsMaxMs = elapsedMs;
    } else {
        if (elapsedMs < fpsMinMs) fpsMinMs = elapsedMs;
        if (elapsedMs > fpsMaxMs) fpsMaxMs = elapsedMs;
    }
    fpsFrames++;
    fpsSumMs += elapsedMs;
    fpsSumDelta += deltaScale;

    if (frameTicks - fpsWindowStart < 500) return;
    fpsWindowStart = frameTicks;

    if (fpsSumMs > 0.0f && fpsFrames > 0) {
        const float avgMs = fpsSumMs / (float)fpsFrames;
        const float fps   = 1000.0f / avgMs;
        // Effective speed: deltaScale actually accumulated per 60 frames' worth of
        // wall time. This is the number that is directly comparable across builds
        // — it reads 3.00 on a healthy browser build, and it stays 3.00 on native
        // only if neither the deltaScale clamps nor the frame limiter are
        // distorting things. Divergence from the configured value is the tell.
        const float effective = fpsSumDelta / ((fpsSumMs / 1000.0f) * 60.0f);
#ifdef __WASM_PORT__
        const float configured = 3.0f;  // hardcoded in RunOneFrame for browsers
#else
        const float configured = GameSettings::Instance()->speedMultiplier;
#endif
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "%.1f fps  %.1fms [%.0f-%.0f]\nspeed %.2fx  set %.2fx",
                 fps, avgMs, fpsMinMs, fpsMaxMs, effective, configured);
        fpsOverlayText = buf;
        fpsText.UpdateText(renderer, fpsOverlayText.c_str(), 0);
    }

    fpsFrames = 0;
    fpsSumMs = 0.0f;
    fpsSumDelta = 0.0f;
}

void FrozenBubble::RenderFpsOverlay()
{
    if (!fpsText.Texture()) return;

    SDL_Rect *c = fpsText.Coords();
    fpsText.UpdatePosition({WINDOW_W - c->w - 6, WINDOW_H - c->h - 4});

    // Dim the area under the text so it stays readable over bright artwork.
    SDL_FRect bg = {(float)(c->x - 4), (float)(c->y - 2),
                    (float)(c->w + 8), (float)(c->h + 4)};
    SDL_BlendMode prev = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &prev);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawBlendMode(renderer, prev);

    SDL_FRect fr = ToFRect(*c);
    SDL_RenderTexture(renderer, fpsText.Texture(), nullptr, &fr);
}

void FrozenBubble::RunOneFrame()
{
    frameLastTick = frameTicks;
    frameTicks    = SDL_GetTicks();
    float elapsed = (float)(frameTicks - frameLastTick);
#ifdef __WASM_PORT__
    // In WASM (iPhone/Android browsers) the frame rate can vary widely, so scale
    // all per-frame movement to keep game speed consistent regardless of FPS.
    // On native builds SDL_Delay already caps near 60 fps — no scaling needed.
    // Normalize to 60 fps equivalent, then apply 3.0x speed boost for browsers
    deltaScale = (elapsed / (1000.0f / 60.0f)) * 3.0f;
    if (deltaScale < 0.1f) deltaScale = 0.1f;  // guard against stalled/zero-elapsed frames
    if (deltaScale > 6.0f) deltaScale = 6.0f;  // guard against tab-hidden resume spikes
#else
    // Native (macOS, Linux, Windows, Android): use per-device saved speed multiplier.
    // Adaptive so minimize/resume frame-rate throttling self-corrects.
    {
        float mult = GameSettings::Instance()->speedMultiplier;
        deltaScale = (elapsed / (1000.0f / 60.0f)) * mult;
        if (deltaScale < 0.5f) deltaScale = 0.5f;
        if (deltaScale > mult * 3.0f) deltaScale = mult * 3.0f;  // cap at 3x the base to prevent resume spike
    }
#endif

    if (gameOptions && gameOptions->showFpsOverlay()) AccumulateFrameStats(elapsed);

    // handle input
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_DID_ENTER_FOREGROUND) {
            // Reset frame timer so the first frame after resume doesn't have a
            // huge elapsed, which would make deltaScale spike on native adaptive builds.
            frameTicks = SDL_GetTicks();
            frameLastTick = frameTicks;
            // Same reason for the pacing deadline: it is far in the past after a
            // long suspend, and would otherwise be resynced one frame later.
            frameDeadline = (float)frameTicks;
        }
        if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_TERMINATING ||
            e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
            (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Quit-triggering event: type=0x%x", e.type);
        }
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
            e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION  || e.type == SDL_EVENT_GAMEPAD_ADDED ||
            e.type == SDL_EVENT_JOYSTICK_ADDED ||
            // Removal was never routed here, which is why it was never handled:
            // the handle stayed open and the slot stayed occupied for good.
            e.type == SDL_EVENT_GAMEPAD_REMOVED || e.type == SDL_EVENT_JOYSTICK_REMOVED) {
            HandleControllerEvent(&e);
            continue;
        }
        HandleInput(&e);
    }

    // render
    if(!IsGamePause) {
        // Clear to opaque black explicitly. SDL_RenderClear paints with
        // whatever draw colour was last set, and nothing here resets it
        // between frames, so the letterbox bars took the colour of the last
        // thing drawn -- on a phone, where the bars are a third of the screen,
        // that showed up as a yellow border round the playfield.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (currentState == TitleScreen) mainMenu->Render();
        else if (currentState == MainGame) mainGame->Render();
        else if (currentState == Highscores) {
            if (hiscoreManager->lastState == 1) mainGame->Render();
            hiscoreManager->RenderScoreScreen();
        }
        if (gameOptions && gameOptions->showFpsOverlay()) RenderFpsOverlay();
        SDL_RenderPresent(renderer);
    } else {
        if (currentState == MainGame) {
            mainGame->RenderPaused();
            if (gameOptions && gameOptions->showFpsOverlay()) RenderFpsOverlay();
            SDL_RenderPresent(renderer);
        }
    }

#ifndef __WASM_PORT__
    // On native, cap frame rate manually. In WASM the browser's
    // requestAnimationFrame already limits to the display refresh rate.
    //
    // Sleep against an absolute deadline, not against `elapsed`. `elapsed` is the
    // gap between the previous two frame starts, so it already contains that
    // frame's own sleep; feeding it back in here double-counts and gives
    // D(n) = frameTime - work - D(n-1), a recurrence with eigenvalue -1. It never
    // settles — it alternates a full-length delay with a near-zero one, so frames
    // arrive in short/long pairs at roughly double the intended rate.
    // A deadline also preserves the 0.667ms per frame that SDL_Delay's integer
    // argument would otherwise truncate away.
    frameDeadline += frameTime;
    float nowMs = (float)SDL_GetTicks();
    if (frameDeadline < nowMs) {
        // Ran long (asset load, transition, window drag). Resync to now rather
        // than trying to claw back the missed frames with a burst of zero-delay
        // ones, which is the behavior this fix exists to remove.
        frameDeadline = nowMs;
    } else if (frameDeadline > nowMs) {
        SDL_Delay((Uint32)(frameDeadline - nowMs));
    }
#endif
}

void FrozenBubble::PushKey(SDL_Keycode key, bool down) {
    SDL_Event ev{};
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.down = down;
    ev.key.key = key;
    ev.key.scancode = SDL_GetScancodeFromKey(key, NULL);
    SDL_PushEvent(&ev);
}

void FrozenBubble::PushScancode(SDL_Scancode sc, bool down, bool skipEvent) {
    if (IsVirtualScancode(sc)) {
        virtualKeyState[sc - CTRL_SC_BASE] = down;
        // In-game callers pass skipEvent=true: virtualKeyState is enough, no SDL event needed.
        // Key-binding menu callers pass skipEvent=false so the menu captures the scancode.
        if (skipEvent) return;
    }
    SDL_Event ev{};
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.down = down;
    ev.key.scancode = sc;
    ev.key.key = SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false);
    SDL_PushEvent(&ev);
}

void FrozenBubble::HandleControllerEvent(SDL_Event *e) {
    // Hot-plug: open newly connected controllers and assign to next player slot.
    // SDL_JOYDEVICEADDED is a fallback for Emscripten/browser where the Gamepad API
    // may fire the joystick event before the controller mapping is resolved.
    if (e->type == SDL_EVENT_GAMEPAD_ADDED || e->type == SDL_EVENT_JOYSTICK_ADDED) {
        SDL_JoystickID idx = (e->type == SDL_EVENT_GAMEPAD_ADDED) ? e->gdevice.which : e->jdevice.which;
        if (!SDL_IsGamepad(idx)) return;
        // Already tracked: nothing to do, and do not open a second handle.
        for (auto& cs : controllers) if (cs.id == idx) return;

        SDL_Gamepad *gp = SDL_OpenGamepad(idx);
        if (!gp) return;

        // Reuse a slot freed by an earlier removal before growing the vector.
        // Slot index is the player number, and bindings only exist for the
        // first CTRL_SC_PLAYERS of them, so unplugging and replugging a pad used
        // to walk it into a slot whose buttons resolve to SDL_SCANCODE_UNKNOWN
        // (guarded since BUG-035) and silently stop responding.
        int slot = -1;
        for (int i = 0; i < (int)controllers.size(); i++) {
            if (controllers[i].pad == nullptr) { slot = i; break; }
        }
        if (slot < 0) {
            controllers.push_back(ControllerState{});
            slot = (int)controllers.size() - 1;
        }
        controllers[slot] = ControllerState{};
        controllers[slot].id = idx;
        controllers[slot].pad = gp;
        SDL_Log("Controller connected: %s → player %d", SDL_GetGamepadName(gp), slot + 1);
        return;
    }

    if (e->type == SDL_EVENT_GAMEPAD_REMOVED || e->type == SDL_EVENT_JOYSTICK_REMOVED) {
        SDL_JoystickID idx = (e->type == SDL_EVENT_GAMEPAD_REMOVED) ? e->gdevice.which : e->jdevice.which;
        for (int i = 0; i < (int)controllers.size(); i++) {
            if (controllers[i].id != idx) continue;
            if (controllers[i].pad) SDL_CloseGamepad(controllers[i].pad);
            // Freed in place rather than erased: erasing would shift every later
            // controller into a different player slot mid-game.
            controllers[i] = ControllerState{};
            SDL_Log("Controller disconnected from player %d", i + 1);
            break;
        }
        return;
    }

    // Find the ControllerState for this event's joystick ID
    SDL_JoystickID evId = (e->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) ? e->gaxis.which : e->gbutton.which;
    ControllerState *cs = nullptr;
    int playerIdx = 0;
    for (int i = 0; i < (int)controllers.size(); i++) {
        if (controllers[i].id == evId) { cs = &controllers[i]; playerIdx = i; break; }
    }
    if (!cs) return;

    if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        bool down = (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);

        // If the Keys panel is waiting for a binding, emit a virtual scancode
        if (down && mainMenu->IsAwaitingKeyBind()) {
            SDL_Scancode vsc = VirtualScancode(playerIdx, e->gbutton.button);
            PushScancode(vsc, true);
            return;
        }

        // Outside of an active game, always use standard nav keys so menus work.
        // In-game: push virtual scancode into virtualKeyState[] so IsKeyPressed() works.
        // controllerInputs[] approach was wrong: playerIdx (controller slot) != playerAssigned.
        bool inGame = (currentState == MainGame);
        if (inGame) {
            int btn = e->gbutton.button;
            if (btn == SDL_GAMEPAD_BUTTON_EAST)     { PushKey(SDLK_AC_BACK, down); }
            else if (btn == SDL_GAMEPAD_BUTTON_START) { PushKey(SDLK_PAUSE, down); }
            else if (btn == SDL_GAMEPAD_BUTTON_WEST && mainGame->IsNetworkGame()) {
                PushKey(mainGame->IsChatting() ? SDLK_RETURN : SDLK_T, down);
            }
            else if (btn == SDL_GAMEPAD_BUTTON_SOUTH && mainGame->IsGameFinished()) {
                PushKey(SDLK_SPACE, down);
            }
            else {
                SDL_Scancode vsc = VirtualScancode(playerIdx, btn);
                PushScancode(vsc, down, true);
            }
        } else {
            switch (e->gbutton.button) {
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  PushKey(SDLK_LEFT,   down); break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: PushKey(SDLK_RIGHT,  down); break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP:    PushKey(SDLK_UP,     down); break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  PushKey(SDLK_DOWN,   down); break;
                case SDL_GAMEPAD_BUTTON_SOUTH:      PushKey(SDLK_RETURN, down); break;
                case SDL_GAMEPAD_BUTTON_EAST:       PushKey(SDLK_AC_BACK, down); break;
                case SDL_GAMEPAD_BUTTON_START:      PushKey(SDLK_PAUSE,   down); break;
                default: break;
            }
        }
    }

    if (e->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        const Sint16 DEAD = 8000;
        Sint16 val = e->gaxis.value;

        bool inGame = (currentState == MainGame);
        if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            bool wantLeft  = val < -DEAD;
            bool wantRight = val >  DEAD;
            if (inGame) {
                SDL_Scancode scLeft  = VirtualScancode(playerIdx, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
                SDL_Scancode scRight = VirtualScancode(playerIdx, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
                if (wantLeft  != cs->axisLeftHeld)  { PushScancode(scLeft,  wantLeft,  true); cs->axisLeftHeld  = wantLeft;  }
                if (wantRight != cs->axisRightHeld) { PushScancode(scRight, wantRight, true); cs->axisRightHeld = wantRight; }
            } else {
                SDL_Scancode scLeft  = mainMenu->IsAwaitingKeyBind()
                    ? VirtualScancode(playerIdx, SDL_GAMEPAD_BUTTON_DPAD_LEFT)
                    : SDL_SCANCODE_LEFT;
                SDL_Scancode scRight = mainMenu->IsAwaitingKeyBind()
                    ? VirtualScancode(playerIdx, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
                    : SDL_SCANCODE_RIGHT;
                if (wantLeft  && !cs->axisLeftHeld)  { PushScancode(scLeft,  true);  cs->axisLeftHeld  = true;  }
                if (!wantLeft &&  cs->axisLeftHeld)  { PushScancode(scLeft,  false); cs->axisLeftHeld  = false; }
                if (wantRight && !cs->axisRightHeld) { PushScancode(scRight, true);  cs->axisRightHeld = true;  }
                if (!wantRight && cs->axisRightHeld) { PushScancode(scRight, false); cs->axisRightHeld = false; }
            }
        }
        if (e->gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            bool wantUp   = val < -DEAD;
            bool wantDown = val >  DEAD;
            if (inGame) {
                SDL_Scancode scUp   = VirtualScancode(playerIdx, SDL_GAMEPAD_BUTTON_DPAD_UP);
                SDL_Scancode scDown = VirtualScancode(playerIdx, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
                if (wantUp   != cs->axisUpHeld)   { PushScancode(scUp,   wantUp,   true); cs->axisUpHeld   = wantUp;   }
                if (wantDown != cs->axisDownHeld)  { PushScancode(scDown, wantDown, true); cs->axisDownHeld = wantDown; }
            } else {
                SDL_Scancode scUp   = mainMenu->IsAwaitingKeyBind()
                    ? VirtualScancode(playerIdx, SDL_GAMEPAD_BUTTON_DPAD_UP)
                    : SDL_SCANCODE_UP;
                SDL_Scancode scDown = mainMenu->IsAwaitingKeyBind()
                    ? VirtualScancode(playerIdx, SDL_GAMEPAD_BUTTON_DPAD_DOWN)
                    : SDL_SCANCODE_DOWN;
                if (wantUp   && !cs->axisUpHeld)   { PushScancode(scUp,   true);  cs->axisUpHeld   = true;  }
                if (!wantUp  &&  cs->axisUpHeld)   { PushScancode(scUp,   false); cs->axisUpHeld   = false; }
                if (wantDown && !cs->axisDownHeld) { PushScancode(scDown, true);  cs->axisDownHeld = true;  }
                if (!wantDown && cs->axisDownHeld) { PushScancode(scDown, false); cs->axisDownHeld = false; }
            }
        }
    }
}

void FrozenBubble::TouchToLogical(const SDL_Event *e, float *lx, float *ly) const {
    // tfinger is normalized against the window, so scale it back up by the
    // window size and then let the renderer undo the letterbox -- the same
    // conversion the mouse path already gets for free from window coordinates.
    int ww = 0, wh = 0;
    if (window) SDL_GetWindowSize(window, &ww, &wh);
    if (ww <= 0 || wh <= 0 || !renderer) {
        // No window to measure against (headless, or a failed create): fall back
        // to the plain canvas mapping rather than emitting a zeroed coordinate.
        *lx = e->tfinger.x * 640.f;
        *ly = e->tfinger.y * 480.f;
        return;
    }
    SDL_RenderCoordinatesFromWindow(renderer, e->tfinger.x * (float)ww,
                                    e->tfinger.y * (float)wh, lx, ly);
}

MenuSwipeGesture ClassifyMenuSwipe(float dx, float dy, bool onSteppedRow) {
    // A stepped row's own left-half tap ("decrease") is indistinguishable
    // from a deliberate "swipe back" once ordinary touch jitter is added --
    // an ordinary tap can drift 40+ logical units on a narrow phone, past
    // the Back threshold below -- so Back is suppressed on a stepped row.
    // That collision is purely horizontal: a stepped row's own tap only
    // ever reads which *half* (left/right) was touched, never how far up
    // or down the finger moved. Up/Down must not be suppressed the same
    // way -- a stepped row's tap band is a full row tall (~46-56 logical
    // units), far more than the 15-unit Up/Down threshold needs to fire,
    // so gating it on onSteppedRow meant an intentional swipe starting or
    // landing on a stepped row was silently read as a stationary tap
    // instead of a navigation swipe -- and since HandlePanelTap treats a
    // second tap on the row already selected as "activate", that
    // misreading is what changed the row's own value instead of moving
    // the selection off it.
    //
    // But that suppression is only meant to catch an ordinary TAP that
    // wandered a little -- a stepped row is never adjusted by swiping, only
    // by a stationary second tap, so nobody drags well past ordinary jitter
    // just to decrease one. Once a swipe travels meaningfully past the drift
    // an accidental tap can produce, it is unambiguously a deliberate Back
    // gesture and must fire even if it happens to release on a stepped row --
    // found live on itch.io/iPhone once a stepped row happened to be pinned
    // to the panel's own bottom edge (the LAN/Net "Set name" section did,
    // at the time -- it has since turned out that row was never really
    // stepped at all, see the comment where it's registered in
    // mainmenu_netpanel.cpp; the collision this fixes is a real one for any
    // genuinely stepped row in that position): a full, deliberate
    // edge-to-edge swipe back now routinely released right on top of it,
    // and unconditional suppression broke "swipe back" there entirely
    // rather than just the narrow drift case it was written for.
    //
    // The cap was originally 100: comfortably past the ~40-45-unit jitter
    // ceiling above, but that margin turned out to be too generous the other
    // way. Reported live: on the "Bots" row (and every other stepped row --
    // Players, Mode, Victories, Bot skill, per-player colors, all share this
    // same menulist::List machinery) a normal, deliberate swipe-back on an
    // iPhone routinely traveled 60-90 logical units, well short of 100, so it
    // fell through to HandlePanelTap and silently nudged the row's value
    // instead of leaving the screen -- every attempt to back out from a
    // stepped row just changed it again, which read as the screen being
    // stuck. 60 keeps a real margin over the jitter ceiling above while
    // giving an ordinary swipe a realistic chance to clear it.
    constexpr float kBackThreshold = 40.f;
    constexpr float kSteppedRowDriftCap = 60.f;
    if (dx < -kBackThreshold && fabsf(dy) < fabsf(dx)) {
        if (!onSteppedRow || -dx > kSteppedRowDriftCap) return MenuSwipeGesture::Back;
    }
    if (fabsf(dy) > 15.f) return dy < 0 ? MenuSwipeGesture::Up : MenuSwipeGesture::Down;
    return MenuSwipeGesture::None;
}

bool IsWithinMenuTapDebounce(Uint32 nowMs, Uint32 lastMenuTapMs) {
    return nowMs - lastMenuTapMs < kMenuTapDebounceMs;
}

void FrozenBubble::HandleInput(SDL_Event *e) {
    switch(e->type) {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_EVENT_WINDOW_CLOSE_REQUESTED received");
#ifndef __ANDROID__
            // On Android, onDestroy/nativeSendQuit → SDL_EVENT_QUIT is the correct
            // exit path. Window close can fire spuriously during surface
            // lifecycle events and must not quit the game.
            IsGameQuit = true;
#endif
            break;
        }
        case SDL_EVENT_QUIT:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_EVENT_QUIT received");
            IsGameQuit = true;
            break;
        case SDL_EVENT_KEY_DOWN:
            if(e->key.repeat) break;
            switch(e->key.key) {
                case SDLK_AC_BACK:
                case SDLK_ESCAPE:
                {
                    // Only trigger double-back-to-quit when at the root menu (no panels open)
                    if (currentState == TitleScreen && !mainMenu->HasAnyPanelOpen()) {
                        Uint32 now = SDL_GetTicks();
                        if (now - lastBackPressTime < 2000) {
                            IsGameQuit = true;
                        }
                        lastBackPressTime = now;
                    }
                    // Always let mainMenu/mainGame handle it too (closes panels, cancels, etc.)
                    break;
                }
                case SDLK_F3:
                    // Performance overlay. F11 is mute and F12 is fullscreen.
                    gameOptions->SetValue("GFX:ShowFPS", "");
                    // Start a clean sampling window so the first reading is not
                    // an average over however long the overlay was hidden.
                    fpsFrames = 0;
                    fpsSumMs = 0.0f;
                    fpsSumDelta = 0.0f;
                    fpsWindowStart = SDL_GetTicks();
                    break;
                case SDLK_F12:
                    gameOptions->SetValue("GFX:Fullscreen", "");
                    SDL_SetWindowFullscreen(window, gameOptions->fullscreenMode());
                    break;
                case SDLK_PAUSE:
                    CallGamePause();
                    if (currentState == MainGame) {
                        if (!mainGame->playedPause) mainGame->playedPause = false;
                    }
                    break;
            }
            break;
    }

    if (IsGamePause) return;

    auto injectKey = [](SDL_Keycode k) {
        SDL_Event ev = {};
        ev.type = SDL_EVENT_KEY_DOWN;
        ev.key.key = k;
        SDL_PushEvent(&ev);
    };

    if(currentState == Highscores) {
        hiscoreManager->HandleInput(e);
        return;
    }
    if(currentState == TitleScreen) {
        mainMenu->HandleInput(e);

        // Touch/click navigation for menu
        // Buttons: x=89, y=14+i*56, w=202, h=46 in logical (640x480) coords
        // Tracked with an explicit flag rather than a negative sentinel: once
        // touches are mapped through the letterbox, a press that starts in a
        // bar has a legitimately negative logical coordinate, and a sentinel
        // would read that as "no press recorded" and zero out the swipe.
        static float touchStartX = 0.f, touchStartY = 0.f;
        static bool touchStarted = false;
        static Uint32 lastMenuTapMs = 0;
#ifdef __WASM_PORT__
        // See WasmMouseEchoGuard (frozenbubble.h) for what this guards and
        // why it replaced a millisecond-based check for that purpose.
        static WasmMouseEchoGuard wasmMouseEcho;
#endif
        Uint32 nowMs = SDL_GetTicks();
        auto getMenuButtonAt = [](float lx, float ly) -> int {
            if (lx < 89.f || lx > 291.f) return -1;
            for (int i = 0; i < 8; i++) {
                float by = 14.f + i * 56.f;
                if (ly >= by && ly < by + 46.f) return i;
            }
            return -1;
        };

        if (e->type == SDL_EVENT_FINGER_DOWN) {
#ifdef __WASM_PORT__
            wasmMouseEcho.OnFingerDown();
#endif
            TouchToLogical(e, &touchStartX, &touchStartY);
            touchStarted = true;
        } else if (e->type == SDL_EVENT_FINGER_UP) {
            // Debounce: ignore rapid re-fires (multi-finger or OS double events)
            if (IsWithinMenuTapDebounce(nowMs, lastMenuTapMs)) {
                touchStarted = false;
                return;
            }
            lastMenuTapMs = nowMs;
#ifdef __WASM_PORT__
            // This tap is about to dispatch a real menu action below; the
            // browser owes it exactly one synthetic MOUSE_BUTTON_DOWN echo
            // afterward (see that branch). Set unconditionally here, before
            // dispatch, since every path below (button press, swipe, panel
            // tap, or the RETURN fallback) dispatches exactly once.
            wasmMouseEcho.OnFingerUpDispatched();
#endif
            float lx, ly;
            TouchToLogical(e, &lx, &ly);
            float dx = touchStarted ? (lx - touchStartX) : 0.f;
            float dy = touchStarted ? (ly - touchStartY) : 0.f;
            touchStarted = false;
            if (!mainMenu->HasAnyPanelOpen()) {
                int btn = getMenuButtonAt(lx, ly);
                if (btn >= 0) mainMenu->SelectAndPressButton(btn);
            } else {
                // A stepped row (Game speed, Victories limit, ...) is
                // adjusted by which half a tap lands on, and its left half
                // is exactly the direction the swipe-back gesture below also
                // claims -- see ClassifyMenuSwipe for why that conflict is
                // real, not hypothetical. Checked here, ahead of dx/dy, so
                // that row always gets first look at a touch that lands on it.
                switch (ClassifyMenuSwipe(dx, dy, mainMenu->IsSteppedRowAt(lx, ly))) {
                    case MenuSwipeGesture::Back:
                        injectKey(SDLK_ESCAPE);
                        if (mainMenu->IsTextEditActive()) {
                            injectKey(SDLK_ESCAPE); // close keyboard then actually go back
                        }
                        break;
                    case MenuSwipeGesture::Up:
                        injectKey(SDLK_UP);
                        break;
                    case MenuSwipeGesture::Down:
                        injectKey(SDLK_DOWN);
                        break;
                    case MenuSwipeGesture::None:
                        // Real vertical drift (see HandlePanelTap) so a
                        // touch that traveled and fell back onto the row it
                        // started from is not treated as a confirming
                        // second tap on that row.
                        if (!mainMenu->HandlePanelTap(lx, ly, dy)) {
                            // Panels that hit-test their own rows consume the
                            // tap; the rest keep the original
                            // tap-anywhere-to-confirm behaviour.
                            injectKey(SDLK_RETURN);
                        }
                        break;
                }
            }
        } else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_LEFT) {
            // On native SDL3, touch synthesizes MOUSE_BUTTON_DOWN with SDL_TOUCH_MOUSEID — skip it,
            // handled by FINGER_UP above. In WASM, Emscripten may not set SDL_TOUCH_MOUSEID
            // correctly, so that tag can't be trusted there.
#ifndef __WASM_PORT__
            if (e->button.which == SDL_TOUCH_MOUSEID) return;
#else
            // A real touch in a browser still fires both FINGER_UP (handled
            // above, with the letterbox-aware coordinate conversion and the
            // swipe/stepped-row classification this branch does not have)
            // and a synthesized MOUSE_BUTTON_DOWN a moment later for legacy
            // mouse-only code -- processing both dispatches every menu tap
            // and button press twice. For a stepped row (Game speed, ...)
            // that is not just "twice as fast": the second dispatch's own
            // coordinate conversion can disagree with the first's about
            // which half of the row was touched, which is what let a tap
            // aimed at decreasing read as broken while increasing did not.
            //
            // This used to be a millisecond check against lastMenuTapMs
            // (drop a mouse event that follows a tap within a short
            // window). See WasmMouseEchoGuard (frozenbubble.h) for why a
            // fixed window isn't reliable and what replaced it.
            if (wasmMouseEcho.ShouldSwallowMouseDown()) return;
#endif
            float lx, ly;
            SDL_RenderCoordinatesFromWindow(renderer, e->button.x, e->button.y, &lx, &ly);
            if (!mainMenu->HasAnyPanelOpen()) {
                int btn = getMenuButtonAt(lx, ly);
                if (btn >= 0) mainMenu->SelectAndPressButton(btn);
            } else if (!mainMenu->HandlePanelTap(lx, ly)) {
                injectKey(SDLK_RETURN);
            }
        } else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_RIGHT) {
#ifndef __WASM_PORT__
            if (e->button.which == SDL_TOUCH_MOUSEID) return;
#endif
            injectKey(SDLK_ESCAPE);
        }
    }
    if(currentState == MainGame) {
        mainGame->HandleInput(e);

        // Mouse aim: convert window coords to logical (640x480) canvas coords
        if (e->type == SDL_EVENT_MOUSE_MOTION) {
            float lx, ly;
            SDL_RenderCoordinatesFromWindow(renderer, e->motion.x, e->motion.y, &lx, &ly);
            mainGame->HandleMouseAim(lx, ly);
        } else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_LEFT) {
#ifndef __WASM_PORT__
            if (e->button.which == SDL_TOUCH_MOUSEID) return; // handled by FINGER_UP; skip synthesized mouse
#endif
            if (mainGame->IsGameFinished()) {
                float lx, ly;
                SDL_RenderCoordinatesFromWindow(renderer, e->button.x, e->button.y, &lx, &ly);
                if (!mainGame->HandleFinishedTap(lx, ly))
                    injectKey(SDLK_RETURN);
            } else
                mainGame->HandleMouseFire();
        } else if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_RIGHT) {
#ifndef __WASM_PORT__
            if (e->button.which == SDL_TOUCH_MOUSEID) return;
#endif
            injectKey(SDLK_ESCAPE);
        }
        // Touch aim+fire via FINGER events (native only).
        // In WASM, Emscripten generates both FINGER_UP and MOUSE_BUTTON_DOWN for one tap —
        // using both would double-inject SDLK_RETURN and fire a bubble on the new round.
        // WASM uses MOUSE_BUTTON_DOWN exclusively (see handler above).
#ifndef __WASM_PORT__
        // Press position is kept so a release can be told apart from a tap.
        // Touch has no other way out of a round: Escape, gamepad B and Android's
        // back button all reach QuitToTitle, and iOS has none of them.
        static float gameTouchStartX = 0.f, gameTouchStartY = 0.f;
        static bool gameTouchStarted = false;
        if (e->type == SDL_EVENT_FINGER_DOWN) {
            TouchToLogical(e, &gameTouchStartX, &gameTouchStartY);
            gameTouchStarted = true;
        } else if (e->type == SDL_EVENT_FINGER_MOTION) {
            float lx, ly;
            TouchToLogical(e, &lx, &ly);
            mainGame->HandleMouseAim(lx, ly);
        } else if (e->type == SDL_EVENT_FINGER_UP) {
            float lx, ly;
            TouchToLogical(e, &lx, &ly);
            bool back = gameTouchStarted &&
                        mainGame->IsTouchBackSwipe(gameTouchStartX, gameTouchStartY, lx, ly);
            gameTouchStarted = false;
            if (back) {
                // Deliberately ahead of the finished-game branch: a round that
                // has ended still needs a way out that is not "start another".
                injectKey(SDLK_ESCAPE);
            } else if (mainGame->IsGameFinished()) {
                if (!mainGame->HandleFinishedTap(lx, ly))
                    injectKey(SDLK_RETURN); // tap to continue after round
            } else {
                mainGame->HandleMouseAim(lx, ly);
                mainGame->HandleMouseFire();
            }
        }
#endif
    }
}
