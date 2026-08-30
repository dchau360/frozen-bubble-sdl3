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
//
// Neither of those was the actual bug, confirmed by driving the real,
// deployed build with a mouse click (which goes through neither gesture
// path at all) and watching the value go the wrong way. menulist::List
// drew a stepped row's "<  value  >" right-aligned, near the row's right
// edge, but split its TAP ROW down the row's own raw geometric middle --
// so on a row with a long label and a short value (Game speed's "<  3.0  >"
// against the whole width of the row), the visible "<" a player would
// naturally tap to decrease sat physically inside what the code called the
// row's right half, and tapping it increased the value instead. This is
// what the last two fixes' own asymmetry ("raising always worked") was
// actually evidence of the whole time. The section below drives a real
// menulist::List render of this exact row and checks where the resulting
// tap boundary actually landed.

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "frozenbubble.h"
#include "gamesettings.h"
#include "mainmenu.h"
#include "mainmenu_internal.h"
#include "platform.h"

#include <cstdio>
#include <memory>
#include <vector>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

// Mirrors the minimal slice of MainMenuTestAccess (tests/localmultiplayer_settings_test.cpp)
// this file needs: BeginPanelTapRows, AddPanelTapRow, KeysPanelRender and
// keyConfigIndex are private, published through the same friend declaration
// that access grants. IsSteppedRowAt and HandlePanelTap are public -- a
// caller deciding whether, and how, a touch should reach a row is exactly
// the production use case, so neither needs special access.
struct MainMenuTestAccess {
    static std::unique_ptr<MainMenu> Create(const SDL_Renderer* renderer) {
        return std::unique_ptr<MainMenu>(
            new MainMenu(renderer, MainMenu::HeadlessTestTag{}));
    }
    static void BeginRows(MainMenu& menu, int* sel) {
        menu.BeginPanelTapRows(sel);
    }
    static void AddRow(MainMenu& menu, int index, SDL_Rect rect, bool splitAdjust,
                       SDL_Keycode activateKey = 0) {
        menu.AddPanelTapRow(index, rect, -1, splitAdjust, activateKey);
    }
    // Renders the real Keys/Settings panel -- the one KeysPanelRender a
    // player actually sees -- so the tap rows under test are whatever
    // menulist::List really registered for the current speedMultiplier,
    // not a hand-built stand-in for it.
    static void RenderKeysPanel(MainMenu& menu, int selectedIndex) {
        menu.showingKeysPanel = true;
        menu.keyConfigIndex = selectedIndex;
        menu.KeysPanelRender();
    }
    static void SelectKeysRow(MainMenu& menu, int index) {
        menu.keyConfigIndex = index;
    }
    // Every registered rect sharing this row index, in registration order
    // (menulist::List now emits two for a stepped row -- see List::End()).
    static std::vector<SDL_Rect> RectsForIndex(const MainMenu& menu, int index) {
        std::vector<SDL_Rect> out;
        for (const auto& row : menu.panelTapRows)
            if (row.index == index) out.push_back(row.rect);
        return out;
    }
    static SDL_Keycode ActivateKeyAt(const MainMenu& menu, int index, size_t which) {
        size_t seen = 0;
        for (const auto& row : menu.panelTapRows) {
            if (row.index != index) continue;
            if (seen == which) return row.activateKey;
            ++seen;
        }
        return 0;
    }
};

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow(
        "menu-touch-gesture-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

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

    // A hand-built row can carry activateKey directly instead of splitAdjust
    // -- the shape menulist::List now actually emits (see List::End()).
    // IsSteppedRowAt has to recognize this shape too, or every List-rendered
    // stepped row stops registering as one the moment List stopped setting
    // splitAdjust, silently reopening the swipe-vs-tap collision above.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(nullptr);
        int selection = 0;
        MainMenuTestAccess::BeginRows(*menu, &selection);
        MainMenuTestAccess::AddRow(*menu, 1, SDL_Rect{10, 44, 300, 32}, false, SDLK_LEFT);
        MainMenuTestAccess::AddRow(*menu, 1, SDL_Rect{310, 44, 104, 32}, false, SDLK_RIGHT);
        CHECK(menu->IsSteppedRowAt(20.f, 55.f));
        CHECK(menu->IsSteppedRowAt(350.f, 55.f));
    }

    // --- menulist::List's stepped-row split, against a real render --------
    //
    // The actual bug: the tap boundary was the row's own raw geometric
    // middle, but "<  value  >" is drawn right-aligned near the row's right
    // edge. On a row with a long label and a short value -- exactly Game
    // speed's shape -- the visible "<" sat inside what the old boundary
    // called the right half, so tapping the one thing that looks like
    // "decrease" increased instead. This drives the real KeysPanelRender,
    // at the real current speedMultiplier, and checks where the boundary
    // menulist::List actually drew it landed -- not a hand-built stand-in.
    {
        GameSettings* gs = GameSettings::Instance();
        gs->speedMultiplier = 3.0f;  // pinned rather than trusting a local settings file

        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::RenderKeysPanel(*menu, kKeyRowSpeed);

        const std::vector<SDL_Rect> rects =
            MainMenuTestAccess::RectsForIndex(*menu, kKeyRowSpeed);
        CHECK(rects.size() == 2);
        // The row's own y, read from the real render rather than guessed --
        // where it falls among KeysPanelRender's other rows is an
        // implementation detail this test has no business hardcoding.
        const float rowY = rects.empty() ? 0.f : rects[0].y + rects[0].h * 0.5f;
        if (rects.size() == 2) {
            const SDL_Rect& left = rects[0];
            const SDL_Rect& right = rects[1];

            // The two halves must still be one uninterrupted row: nothing
            // between them a tap could fall into and hit neither, and
            // nothing sent twice.
            CHECK(left.y == right.y && left.h == right.h);
            CHECK(left.x + left.w == right.x);

            // menulist.h's own kListFull viewport is {10, 44, 404, ...}, so
            // the row spans x:[10, 414) and its OLD, buggy midpoint was
            // exactly 212. The fixed boundary must sit well to the right of
            // that -- close to the row's own right edge, where "<  3.0  >"
            // is actually drawn -- not at the row's raw geometric middle.
            // (This also bounds the headline point picked below: it is
            // always inside `left` by construction, but only meaningfully
            // pins the regression if `left` reaches well past the old 212
            // midpoint -- which these two checks establish independently.)
            CHECK(left.x + left.w > 300);
            CHECK(left.x + left.w < 414);

            CHECK(MainMenuTestAccess::ActivateKeyAt(*menu, kKeyRowSpeed, 0) == SDLK_LEFT);
            CHECK(MainMenuTestAccess::ActivateKeyAt(*menu, kKeyRowSpeed, 1) == SDLK_RIGHT);

            // The headline case: a point just inside the LEFT zone's own
            // right edge -- i.e. right where "<" actually renders, not a
            // guessed pixel that depends on font metrics this test doesn't
            // control. Given the >300 check above, this is always well past
            // the OLD, buggy midpoint of 212, which would have called it
            // the increase half.
            const float headlineX = left.x + left.w - 5.f;

            // Drain whatever's already queued (window setup enqueues its
            // own events, e.g. SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) so the
            // poll below can only see what this tap sequence pushes.
            SDL_PumpEvents();
            for (SDL_Event drain; SDL_PollEvent(&drain); ) {}

            // Genuinely reproduce the two-tap dance: the row starts
            // deselected (RenderKeysPanel left it selected, which would let
            // the first tap double as an adjust too), a first tap only
            // selects, and only the second, on the visible "<", adjusts it.
            MainMenuTestAccess::SelectKeysRow(*menu, kKeyRowLeft);
            CHECK(menu->HandlePanelTap(20.f, rowY));       // select (first tap)
            CHECK(menu->HandlePanelTap(headlineX, rowY));  // adjust (second tap)

            SDL_Event ev;
            CHECK(SDL_PollEvent(&ev) && ev.type == SDL_EVENT_KEY_DOWN);
            CHECK(ev.key.key == SDLK_LEFT);
        }
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
