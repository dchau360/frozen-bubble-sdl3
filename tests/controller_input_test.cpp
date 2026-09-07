#include <SDL3/SDL.h>
#include "frozenbubble.h"
#include "gamesettings.h"

#include <cstdio>

static int failures = 0;
#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    ++failures; } } while (false)

// Pokes at FrozenBubble's private controller-tracking state directly, so this
// test can drive HandleControllerEvent() without standing up a real window,
// audio device, or the full menu/game tree.
struct FrozenBubbleTestAccess {
    static FrozenBubble* create() { return new FrozenBubble(FrozenBubble::HeadlessTestTag{}); }
    static void destroy(FrozenBubble* fb) { delete fb; }
    static void addController(FrozenBubble &fb, SDL_JoystickID id) {
        fb.controllers.push_back(FrozenBubble::ControllerState{});
        fb.controllers.back().id = id;
    }
    static FrozenBubble::ControllerState& controllerAt(FrozenBubble &fb, size_t i) {
        return fb.controllers[i];
    }
    static void dispatch(FrozenBubble &fb, SDL_Event &e) { fb.HandleControllerEvent(&e); }
};

static void drainEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {}
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);

    // --- Mapping table: every press this drives must have a well-defined,
    // symmetric release, since a controller disconnect replays the same
    // mapping with down=false to clean up whatever it left held. ---
    {
        // In-game: an ordinary button drives a per-player virtual scancode.
        auto ordinary = MapControllerButton(2, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, /*inGame=*/true,
                                             false, false, false);
        CHECK(ordinary.kind == ControllerActionKind::VirtualScancode);
        CHECK(ordinary.scancode == VirtualScancode(2, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));

        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_EAST,  true, false, false, false).realKey == SDLK_AC_BACK);
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_START, true, false, false, false).realKey == SDLK_PAUSE);

        // WEST only becomes chat/talk in a network game; otherwise it's just
        // another gameplay button.
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_WEST, true, /*isNetworkGame=*/false, false, false)
              .kind == ControllerActionKind::VirtualScancode);
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_WEST, true, true, /*isChatting=*/true, false)
              .realKey == SDLK_RETURN);
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_WEST, true, true, false, false).realKey == SDLK_T);

        // SOUTH only becomes "continue" once the round has actually finished.
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_SOUTH, true, false, false, /*isGameFinished=*/false)
              .kind == ControllerActionKind::VirtualScancode);
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_SOUTH, true, false, false, true).realKey == SDLK_SPACE);

        // Menu navigation: physical buttons map to real keys.
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_DPAD_LEFT,  false, false, false, false).realKey == SDLK_LEFT);
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false, false, false, false).realKey == SDLK_RIGHT);
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_DPAD_UP,    false, false, false, false).realKey == SDLK_UP);
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_DPAD_DOWN,  false, false, false, false).realKey == SDLK_DOWN);
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_NORTH,      false, false, false, false).realKey == SDLK_F1);
        // A button with no menu meaning does nothing -- and so needs nothing
        // released on disconnect either.
        CHECK(MapControllerButton(0, SDL_GAMEPAD_BUTTON_BACK, false, false, false, false).kind
              == ControllerActionKind::None);

        // The analog stick drives a virtual scancode in-game or while the
        // Keys panel is capturing a binding, a real arrow key otherwise.
        CHECK(MapControllerAxisDirection(1, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true, false)
              == VirtualScancode(1, SDL_GAMEPAD_BUTTON_DPAD_LEFT));
        CHECK(MapControllerAxisDirection(1, SDL_GAMEPAD_BUTTON_DPAD_LEFT, false, true)
              == VirtualScancode(1, SDL_GAMEPAD_BUTTON_DPAD_LEFT));
        CHECK(MapControllerAxisDirection(1, SDL_GAMEPAD_BUTTON_DPAD_LEFT, false, false)
              == SDL_SCANCODE_LEFT);
    }

    // --- Regression: a controller unplugged mid-press, in-game, must not
    // leave gameplay's virtual scancodes latched on with no pad left to
    // release them (BubbleArray reads virtualKeyState[] forever otherwise). ---
    {
        FrozenBubble *fb = FrozenBubbleTestAccess::create();
        fb->currentState = MainGame;
        FrozenBubbleTestAccess::addController(*fb, /*id=*/7);
        auto &cs = FrozenBubbleTestAccess::controllerAt(*fb, 0);

        // A shoulder button held and the stick pushed left, exactly as they'd
        // be mid-press when the pad vanishes -- no BUTTON_UP or centering
        // AXIS_MOTION will ever arrive from a device that is gone.
        SDL_Scancode heldVsc = VirtualScancode(0, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        SDL_Scancode leftVsc = VirtualScancode(0, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        cs.heldButtons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER] = true;
        cs.axisLeftHeld = true;
        virtualKeyState[heldVsc - CTRL_SC_BASE] = true;
        virtualKeyState[leftVsc - CTRL_SC_BASE] = true;
        CHECK(IsKeyPressed(heldVsc));
        CHECK(IsKeyPressed(leftVsc));

        SDL_Event removed{};
        removed.type = SDL_EVENT_GAMEPAD_REMOVED;
        removed.gdevice.which = 7;
        FrozenBubbleTestAccess::dispatch(*fb, removed);

        CHECK(!IsKeyPressed(heldVsc));
        CHECK(!IsKeyPressed(leftVsc));
        // The slot is freed in place (reused by the next hotplug), not erased.
        CHECK(cs.id == 0);
        CHECK(cs.pad == nullptr);
        CHECK(!cs.axisLeftHeld);
        CHECK(!cs.heldButtons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]);

        FrozenBubbleTestAccess::destroy(fb);
    }

    // --- Same scenario outside gameplay: a menu's held input is a real SDL
    // key, not a virtual scancode, and disconnect must release that instead. ---
    {
        FrozenBubble *fb = FrozenBubbleTestAccess::create();
        fb->currentState = TitleScreen;
        FrozenBubbleTestAccess::addController(*fb, /*id=*/9);
        auto &cs = FrozenBubbleTestAccess::controllerAt(*fb, 0);
        cs.axisLeftHeld = true; // analog stick held left while browsing a menu

        drainEvents();
        SDL_Event removed{};
        removed.type = SDL_EVENT_GAMEPAD_REMOVED;
        removed.gdevice.which = 9;
        FrozenBubbleTestAccess::dispatch(*fb, removed);

        bool sawLeftUp = false;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_KEY_UP && e.key.scancode == SDL_SCANCODE_LEFT) sawLeftUp = true;
        }
        CHECK(sawLeftUp);
        CHECK(!cs.axisLeftHeld);

        FrozenBubbleTestAccess::destroy(fb);
    }

    SDL_Quit();
    return failures ? 1 : 0;
}
