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
#include "menulist.h"
#include "networkclient.h"
#include "platform.h"

#include <cstdio>
#include <memory>
#include <string>
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
    static size_t RowCount(const MainMenu& menu) {
        return menu.panelTapRows.size();
    }
    // Whether any registered row actually covers a point -- lets a test
    // assert "this really is a miss" instead of trusting a guessed
    // coordinate to stay outside every row as layouts change.
    static bool AnyRowCovers(const MainMenu& menu, float x, float y) {
        for (const auto& row : menu.panelTapRows) {
            if (x < row.rect.x || x >= row.rect.x + row.rect.w) continue;
            if (y < row.rect.y || y >= row.rect.y + row.rect.h) continue;
            return true;
        }
        return false;
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
    // Renders the real LAN/Net server list panel with a caller-chosen public
    // server list, so the "Set name" section's position under test is
    // whatever ServerListPanelRender really lays out for that server count,
    // not a hand-built stand-in for it.
    static void SetPublicServers(MainMenu& menu, std::vector<ServerInfo> servers) {
        menu.publicServers = std::move(servers);
    }
    static void RenderServerList(MainMenu& menu, bool isLAN) {
        menu.ServerListPanelRender(isLAN);
    }
    // Renders the real lobby action list -- "Create Game Room" among them --
    // with no current game, matching a freshly-connected client.
    static void RenderLobbyActions(MainMenu& menu) {
        menu.NetPanelLobbyActionsRender();
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

    // The regression this pins: once "Set name" (a stepped row) started
    // being pinned to the panel's own bottom edge on every LAN/Net screen,
    // a full, deliberate edge-to-edge swipe back on an iPhone routinely
    // released right on top of it -- and unconditional suppression on any
    // stepped row broke "swipe back" there entirely, not just the narrow
    // accidental-drift case above. A swipe that travels well past what an
    // ordinary tap can drift is unambiguous: nobody adjusts a stepped row's
    // value by dragging 100+ logical units, only by a stationary second
    // tap, so this must still fire Back even on a stepped row.
    CHECK(ClassifyMenuSwipe(-300.f, 2.f, true) == MenuSwipeGesture::Back);

    // The regression that pin above didn't catch: an edge-to-edge swipe is
    // unambiguous, but most real swipe-back attempts on a phone aren't
    // edge-to-edge -- reported live on the "Bots" row (a stepped row, same
    // as every other Players/Mode/Victories/Bot-skill/per-player-color row):
    // a normal deliberate swipe-back travels something like 60-90 logical
    // units, comfortably past the ~40-45-unit jitter ceiling above but well
    // short of the old 100-unit cap, so it fell through to HandlePanelTap
    // and nudged the row's value instead of leaving the screen -- every
    // attempt to back out just changed "Bots" again, which is what made the
    // screen feel stuck rather than merely slow to back out of.
    CHECK(ClassifyMenuSwipe(-70.f, 2.f, true) == MenuSwipeGesture::Back);

    // Unlike Back, Up/Down has no collision with a stepped row's own tap --
    // that tap only ever reads which horizontal half was touched, never
    // vertical travel -- so a real vertical swipe must still register as
    // navigation even when the release point sits on a stepped row. A
    // stepped row's tap band is a full row tall, far more than the 15-unit
    // Up/Down threshold needs to trigger, so suppressing this case the same
    // way Back is suppressed used to silently swallow an intentional swipe
    // off (or onto) a stepped row as a stationary tap -- and since a second
    // tap on the row already selected activates it, that swallowed swipe
    // ended up changing the row's own value instead of moving off it.
    CHECK(ClassifyMenuSwipe(2.f, 20.f, false)  == MenuSwipeGesture::Down);
    CHECK(ClassifyMenuSwipe(2.f, -20.f, false) == MenuSwipeGesture::Up);
    CHECK(ClassifyMenuSwipe(2.f, 20.f, true)   == MenuSwipeGesture::Down);
    CHECK(ClassifyMenuSwipe(2.f, -20.f, true)  == MenuSwipeGesture::Up);

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

            // The defect this pins: an undershot vertical swipe (real
            // travel, but short of ClassifyMenuSwipe's own Up/Down
            // threshold) that releases back on the row it started from --
            // already selected, since it barely moved -- must not activate
            // that row. It is still consumed (true): the tap landed on a
            // real row, so the caller must not fall back to
            // tap-anywhere-confirms either.
            SDL_PumpEvents();
            for (SDL_Event drain; SDL_PollEvent(&drain); ) {}
            CHECK(menu->HandlePanelTap(headlineX, rowY, 20.f) == true);
            CHECK(!SDL_PollEvent(&ev));  // nothing pushed -- no LEFT, no RETURN

            // A near-stationary release (ordinary tap jitter, not a failed
            // swipe) on the same already-selected row must still activate
            // normally -- this guard must not eat real taps.
            CHECK(menu->HandlePanelTap(headlineX, rowY, 3.f) == true);
            CHECK(SDL_PollEvent(&ev) && ev.type == SDL_EVENT_KEY_DOWN);
            CHECK(ev.key.key == SDLK_LEFT);
        }
    }

    // --- NetPanelLobbyActionsRender: "Create Game Room" is touch-adjustable
    // ---------------------------------------------------------------------
    //
    // Room size (kRoomSizes[netRoomSizeChoice]) used to be keyboard
    // Left/Right only: a plain splitAdjust row's L/R split runs across the
    // row's own raw midpoint, which for "Create Game Room  <  20 players  >"
    // sits inside the "Create Game Room" label itself, so a tap meant to
    // select/activate the row could instead silently step the room size (or
    // vice versa) depending on which half of the label it landed on -- the
    // same class of bug fixed earlier for Game speed, but with no safe tap
    // target left for the row's own primary action at all. labelActivateKey
    // fixes it by confining the L/R split to the drawn value block itself,
    // leaving the label its own tap zone that still creates the room. This
    // drives the real lobby render and checks the row now has all three
    // zones, left-to-right, in the right order.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::RenderLobbyActions(*menu);

        const std::vector<SDL_Rect> rects = MainMenuTestAccess::RectsForIndex(*menu, 1);
        CHECK(rects.size() == 3);
        if (rects.size() == 3) {
            CHECK(MainMenuTestAccess::ActivateKeyAt(*menu, 1, 0) == SDLK_RETURN);
            CHECK(MainMenuTestAccess::ActivateKeyAt(*menu, 1, 1) == SDLK_LEFT);
            CHECK(MainMenuTestAccess::ActivateKeyAt(*menu, 1, 2) == SDLK_RIGHT);

            // Label zone, then the value's left half, then its right half --
            // one uninterrupted row, nothing to fall between and hit none of
            // the three.
            CHECK(rects[0].x == menulist::kListDocked.x);
            CHECK(rects[0].x + rects[0].w == rects[1].x);
            CHECK(rects[1].x + rects[1].w == rects[2].x);
        }
    }

    // --- ServerListPanelRender: "Set name" pinned to the panel's own
    // bottom, not wherever the server list happens to end ---------------
    //
    // "Set name" used to be the row list's own last row, so with few public
    // servers it landed right under them near the top of an otherwise-empty
    // panel instead of reading as anchored to the screen (reported live:
    // wanted "at the very bottom, in its own section"). It is now a second,
    // fixed-position menulist::List rendered below the scrollable server
    // list. This drives the real ServerListPanelRender with two different
    // server counts and checks that the section's position does not move
    // with the content above it, and that its bottom edge lines up with the
    // shared panel's own bottom edge (menulist::kListFull).
    {
        auto setNameRowY = [&](int serverCount) -> int {
            std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
            std::vector<ServerInfo> servers;
            for (int i = 0; i < serverCount; i++) {
                servers.push_back({"host" + std::to_string(i), 1511, "", 10});
            }
            MainMenuTestAccess::SetPublicServers(*menu, servers);
            MainMenuTestAccess::RenderServerList(*menu, false);
            int lastIdx = 1 + serverCount;
            const std::vector<SDL_Rect> rects =
                MainMenuTestAccess::RectsForIndex(*menu, lastIdx);
            return rects.empty() ? -1 : rects[0].y;
        };

        const int yFewServers = setNameRowY(1);
        const int yManyServers = setNameRowY(5);
        CHECK(yFewServers >= 0 && yManyServers >= 0);

        // The defect this pins: as an ordinary last row, this y would shift
        // with how many rows came before it. Pinned to its own section, it
        // must not move at all between a 1-server and a 5-server list.
        CHECK(yFewServers == yManyServers);

        // And that fixed position must actually be the panel's own bottom
        // edge -- not just some other constant a future refactor could drift
        // away from the visible bottom without this test noticing.
        CHECK(yFewServers + menulist::kRowH == menulist::kListFull.y + menulist::kListFull.h);
    }

    // --- A tap that lands on no row, on a panel that hit-tests its rows ---
    //
    // The defect this pins, and the one actually reported: the caller
    // (FrozenBubble::HandleInput, SDL_EVENT_FINGER_UP) falls back to
    // "tap anywhere confirms" whenever HandlePanelTap returns false, by
    // injecting RETURN -- and RETURN activates whatever row is currently
    // SELECTED. So on a panel that does hit-test its own rows, a tap that
    // MISSED every row did not do nothing: it re-fired the selected row.
    //
    // On the network game room's bot rows that is a trap with no way out.
    // Those rows are 18 logical units tall (mainmenu_netpanel.cpp, the band
    // under "ESC Leave room"), which on a phone-sized canvas is a couple of
    // millimetres -- so with "Bots" selected, most taps aimed at anything
    // else miss every row, fall through here, and inject RETURN, which
    // cycles the bot count again (mainmenu_input.cpp, kRoomBots). Every
    // attempt to move somewhere else just changed the bot count instead,
    // which is exactly what "I can't navigate out of that location" is.
    // The same fall-through does the same thing on the local-multiplayer
    // panel's own Bots row.
    //
    // A miss on such a panel must be consumed and do nothing. Panels that
    // register no rows at all keep the tap-anywhere-confirms behaviour --
    // that is what the panelTapRows.empty() half of the fix preserves, and
    // what the second block below pins.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::RenderLobbyActions(*menu);
        CHECK(MainMenuTestAccess::RowCount(*menu) > 0);

        // Asserted, not assumed: if a layout change ever puts a row here,
        // this test says so rather than silently testing nothing.
        const float missX = 600.f, missY = 474.f;
        CHECK(!MainMenuTestAccess::AnyRowCovers(*menu, missX, missY));

        SDL_PumpEvents();
        for (SDL_Event drain; SDL_PollEvent(&drain); ) {}

        CHECK(menu->HandlePanelTap(missX, missY) == true);  // consumed
        SDL_Event ev;
        CHECK(!SDL_PollEvent(&ev));  // and nothing activated
    }

    // The other side of that fix: a panel that hit-tests nothing (no rows
    // registered this frame) must still let the caller treat a tap as
    // "confirm", or every such screen becomes untappable.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(nullptr);
        int selection = 0;
        MainMenuTestAccess::BeginRows(*menu, &selection);
        CHECK(MainMenuTestAccess::RowCount(*menu) == 0);
        CHECK(menu->HandlePanelTap(100.f, 100.f) == false);
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
