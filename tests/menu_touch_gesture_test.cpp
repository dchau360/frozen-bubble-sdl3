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

// A stepped row (Game speed, Victories limit, bot count, ...) is adjusted by
// which half of the row a second tap lands on -- see PanelTapRow::splitAdjust
// and MainMenu::HandlePanelTap. Its left half is exactly the direction the
// swipe-back gesture (FrozenBubble::HandleInput, SDL_EVENT_FINGER_UP) also
// claims at a small distance threshold, so an ordinary tap that drifts a
// little while decreasing a stepped row could be read as "swipe left to go
// back" before HandlePanelTap ever saw it -- which is why decreasing read as
// broken on a touchscreen while increasing, on the side with no competing
// gesture, did not.
//
// This pins the fix: ClassifyMenuSwipe (the pure decision the finger-up
// handler now delegates to) and MainMenu::IsSteppedRowAt (what tells it a
// stepped row's own left/right split must win) each in isolation.
//
// It also pins a second, related defect found afterward on the itch.io WASM
// build: a browser fires both a real SDL_EVENT_FINGER_UP and a synthesized
// SDL_EVENT_MOUSE_BUTTON_DOWN for one physical tap, and unlike native SDL
// (which tags the synthesized one with SDL_TOUCH_MOUSEID so it can be
// skipped), Emscripten's tagging can't be trusted -- so on WASM both were
// processed, dispatching every menu tap through HandlePanelTap twice with
// two independently-computed coordinates. IsWithinMenuTapDebounce is what
// the mouse path now checks before acting, to recognize that second
// dispatch as the browser's own echo of the tap FINGER_UP already handled.

#include <SDL3/SDL.h>

#include "frozenbubble.h"
#include "mainmenu.h"

#include <cstdio>
#include <memory>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

// Mirrors the minimal slice of MainMenuTestAccess (tests/localmultiplayer_settings_test.cpp)
// this file needs: BeginPanelTapRows and AddPanelTapRow are private, published
// through the same friend declaration that access grants. IsSteppedRowAt
// itself is public -- a caller deciding whether a touch needs HandlePanelTap's
// own left/right split is exactly the production use case, so it needs no
// special access.
struct MainMenuTestAccess {
    static std::unique_ptr<MainMenu> Create(const SDL_Renderer* renderer) {
        return std::unique_ptr<MainMenu>(
            new MainMenu(renderer, MainMenu::HeadlessTestTag{}));
    }
    static void BeginRows(MainMenu& menu, int* sel) {
        menu.BeginPanelTapRows(sel);
    }
    static void AddRow(MainMenu& menu, int index, SDL_Rect rect, bool splitAdjust) {
        menu.AddPanelTapRow(index, rect, -1, splitAdjust, 0);
    }
};

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);

    // --- ClassifyMenuSwipe ---------------------------------------------

    // A clean tap (small movement either way) is never a gesture, regardless
    // of whether it landed on a stepped row.
    CHECK(ClassifyMenuSwipe(2.f, 3.f, false) == MenuSwipeGesture::None);
    CHECK(ClassifyMenuSwipe(2.f, 3.f, true)  == MenuSwipeGesture::None);

    // A clear, deliberate leftward swipe off a stepped row is still "go
    // back" -- the fix must not disable the gesture everywhere, only where
    // it collides with a row's own adjustment.
    CHECK(ClassifyMenuSwipe(-60.f, 2.f, false) == MenuSwipeGesture::Back);

    // The defect this pins: a tap that drifted left by a realistic amount
    // (comfortably past the plain -40 threshold, the way an ordinary tap can
    // on a narrow phone) used to always read as "go back". On a stepped
    // row it must not -- that drift is what decreasing the row looks like.
    CHECK(ClassifyMenuSwipe(-45.f, 2.f, false) == MenuSwipeGesture::Back);
    CHECK(ClassifyMenuSwipe(-45.f, 2.f, true)  == MenuSwipeGesture::None);

    // The same collision exists vertically: a tap's second half of the
    // gesture can drift up or down enough to look like list-scroll
    // navigation. Off a stepped row that's still real navigation; on one,
    // it must yield to the row's own split.
    CHECK(ClassifyMenuSwipe(2.f, 20.f, false)  == MenuSwipeGesture::Down);
    CHECK(ClassifyMenuSwipe(2.f, -20.f, false) == MenuSwipeGesture::Up);
    CHECK(ClassifyMenuSwipe(2.f, 20.f, true)   == MenuSwipeGesture::None);

    // A swipe with real vertical travel doesn't dominate horizontally, so
    // it does not get misread as "go back" -- fabsf(dy) < fabsf(dx) guards
    // this the same way it did before the fix.
    CHECK(ClassifyMenuSwipe(-45.f, 50.f, false) != MenuSwipeGesture::Back);

    // --- MainMenu::IsSteppedRowAt ---------------------------------------

    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(nullptr);
        int selection = 0;
        MainMenuTestAccess::BeginRows(*menu, &selection);
        // A stepped row (e.g. Game speed) and an ordinary one, side by side,
        // as KeysPanelRender would register them via menulist::List.
        MainMenuTestAccess::AddRow(*menu, 1, SDL_Rect{10, 44, 404, 32}, true);
        MainMenuTestAccess::AddRow(*menu, 2, SDL_Rect{10, 76, 404, 32}, false);

        // Anywhere inside the stepped row -- including its left half, where
        // the swipe-back collision lives -- must report true.
        CHECK(menu->IsSteppedRowAt(20.f, 55.f));
        CHECK(menu->IsSteppedRowAt(400.f, 55.f));

        // The plain row looks identical geometrically but is not stepped,
        // so a swipe ending there must still be free to act as a gesture.
        CHECK(!menu->IsSteppedRowAt(20.f, 85.f));

        // Outside every registered row (e.g. the header bar above y=44).
        CHECK(!menu->IsSteppedRowAt(20.f, 10.f));

        // A pure query: asking twice must not change what a real tap there
        // would do (no selection or event side effect to accidentally trip).
        CHECK(menu->IsSteppedRowAt(20.f, 55.f));
    }

    // --- IsWithinMenuTapDebounce -----------------------------------------

    // The exact defect: a synthesized MOUSE_BUTTON_DOWN arriving a few
    // milliseconds after the FINGER_UP for the same physical tap must read
    // as the same gesture, not a second, independent one.
    CHECK(IsWithinMenuTapDebounce(1000, 1000));
    CHECK(IsWithinMenuTapDebounce(1050, 1000));

    // Right at the edge of the window and just past it -- pins the
    // threshold itself rather than only "clearly inside/outside" cases.
    CHECK(IsWithinMenuTapDebounce(1000 + kMenuTapDebounceMs - 1, 1000));
    CHECK(!IsWithinMenuTapDebounce(1000 + kMenuTapDebounceMs, 1000));

    // A genuinely later, independent tap -- e.g. a real desktop-in-browser
    // mouse click with no preceding touch at all, or a second physical tap
    // well after the first -- must not be swallowed.
    CHECK(!IsWithinMenuTapDebounce(5000, 1000));

    if (failures == 0) {
        std::printf("menu touch gesture tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
