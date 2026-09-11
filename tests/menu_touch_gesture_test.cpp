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
// two independently-computed coordinates. IsWithinMenuTapDebounce still
// guards FINGER_UP itself against rapid re-fires (multi-finger, OS double
// events), but the mouse path no longer uses it to recognize the browser's
// own echo of a tap FINGER_UP already handled -- a millisecond window can
// only assume the echo arrives promptly, which held under light load but
// not always: reported live as the network room's Bots row occasionally
// re-incrementing itself with no further tap, intermittently rather than
// every time, exactly what a delayed echo slipping past a fixed window
// would produce on a busier frame. awaitingWasmMouseEcho (frozenbubble.cpp)
// replaced it with actual state -- "does the browser still owe this tap's
// echo" -- so an arbitrarily late echo is still recognized whenever it
// shows up, not just within some fixed window.
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
#include <utility>
#include <vector>

#if !defined(__ANDROID__) && !defined(__WASM_PORT__) && !defined(_WIN32) && !defined(__IOS_PORT__)
// For the raw-socket NICK_IN_USE regression test below -- a second,
// unmanaged TCP connection to a real fb-server, used to claim a nickname out
// from under NetworkClient so its async retry-with-suffix path (async
// networking handoff, stage 1b) can be exercised end to end rather than only
// unit-tested against synthetic state.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct TTFTextTestAccess {
    static void ResetTextureCreationCount() { TTFText::testTextureCreationCount = 0; }
    static size_t TextureCreationCount() { return TTFText::testTextureCreationCount; }
};

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
    // The full-screen team picker (mainmenu_teampanel.cpp), which the >5-cap
    // compact roster's per-player rows open (see kRoomRosterTapBase in
    // mainmenu_internal.h): previously reachable only via the [A] hotkey,
    // with no touch path at all.
    static bool TeamsPanelOpen(const MainMenu& menu) { return menu.showingTeamsPanel; }
    static int TeamsCursor(const MainMenu& menu) { return menu.teamsCursorPlayer; }
    static void RenderTeamsPanel(MainMenu& menu) { menu.TeamsPanelRender(); }
    // Drives the >5-cap "every client applies !team:/!teamset: broadcasts"
    // parsing in NetPanelChatDockRender, without the team picker or any of
    // the rest of the room UI it also draws.
    static void RenderChatDock(MainMenu& menu) { menu.NetPanelChatDockRender(); }
    static int TeamOfSlot(const MainMenu& menu, int slot) { return menu.TeamOfSlot(slot); }
    // Centre of the swatch that sets `team` on `slot`, as actually published
    // by the last TeamsPanelRender -- so the tap under test lands on the real
    // layout rather than a coordinate guessed from the drawing code.
    static bool SwatchCenter(const MainMenu& menu, int slot, int team, float* x, float* y) {
        for (const auto& swatch : menu.teamSwatchTaps) {
            if (swatch.slot != slot || swatch.team != team) continue;
            *x = swatch.rect.x + swatch.rect.w * 0.5f;
            *y = swatch.rect.y + swatch.rect.h * 0.5f;
            return true;
        }
        return false;
    }
    static bool PlayerNameCenter(const MainMenu& menu, int slot, float* x, float* y) {
        for (const auto& name : menu.teamPlayerNameTaps) {
            if (name.slot != slot) continue;
            *x = name.rect.x + name.rect.w * 0.5f;
            *y = name.rect.y + name.rect.h * 0.5f;
            return true;
        }
        return false;
    }
    static bool AutoBalanceCenter(const MainMenu& menu, int teamCount, float* x, float* y) {
        for (const auto& button : menu.teamAutoBalanceTaps) {
            if (button.teamCount != teamCount) continue;
            *x = button.rect.x + button.rect.w * 0.5f;
            *y = button.rect.y + button.rect.h * 0.5f;
            return true;
        }
        return false;
    }
    static size_t SwatchCount(const MainMenu& menu) { return menu.teamSwatchTaps.size(); }
    static SDL_Rect DoneRect(const MainMenu& menu) { return menu.teamsDoneRect; }
    // Reopens the page for the second half of the close test without going
    // back through the roster's two-tap dance, which this block already
    // covered above.
    static void SetTeamsPanelOpen(MainMenu& menu, bool on) { menu.showingTeamsPanel = on; }
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
    // Drives the real fork+exec+poll-for-readiness path (handoff item A's
    // StartLocalServer fix), not a stand-in for it.
    static void SetNetworkPort(MainMenu& menu, int port) { menu.networkPort = port; }
    static void CallStartLocalServer(MainMenu& menu) { menu.StartLocalServer(); }
    static void CallStopLocalServer(MainMenu& menu) { menu.StopLocalServer(); }
    static bool IsServerHosting(const MainMenu& menu) { return menu.serverHosting; }
    // Renders the real lobby/game-room action list -- "Create Game Room"
    // when NetworkClient has no current game, or the >5-cap compact roster
    // and friends when it does (see NetworkClientTestAccess below).
    static void RenderLobbyActions(MainMenu& menu) {
        menu.NetPanelLobbyActionsRender();
    }
    // The settings grid's per-cell texture pool (see NetGridCell's
    // declaration comment in mainmenu.h) -- exposed so a test can mark a
    // cell's SDL_Texture with a property and check it survives an unchanged
    // re-render, the same way statspanelcell_cache_test checks the
    // gameplay stats pools.
    static size_t NetGridCellPoolSize(const MainMenu& menu) {
        return menu.netGridCellPool.size();
    }
    static SDL_Texture* NetGridCellTexture(MainMenu& menu, size_t idx) {
        return idx < menu.netGridCellPool.size() ? menu.netGridCellPool[idx].Texture() : nullptr;
    }
    static void SetPlayerColorCount(MainMenu& menu, int slot, int count) {
        menu.playerColorCounts[slot] = count;
    }
    // Async networking handoff, stage 1: the per-frame pump and the
    // background LAN/public-server/geoloc fetches. LAN discovery is the one
    // fetch that's safe to actually run in an automated test -- it's pure
    // local UDP broadcast with a fixed ~1s window, unlike the public-server
    // and geoloc fetches, which hit real internet endpoints and would make
    // ctest's runtime and pass/fail depend on network reachability.
    static void CallPumpNetworkFrame(MainMenu& menu) { menu.PumpNetworkFrame(); }
    static void CallStartLanFetch(MainMenu& menu) { menu.StartLanFetch(); }
    static bool LanFetchInProgress(const MainMenu& menu) { return menu.lanFetchInProgress.load(); }
    // The connecting indicator's cancel affordance (async networking handoff,
    // stage 2e). The rect is written by ServerListPanelRender each frame; a
    // test drives the tap through the real HandlePanelTap so it exercises the
    // same path a finger does, not a private helper.
    static SDL_Rect CancelConnectRect(const MainMenu& menu) { return menu.cancelConnectTapRect; }
    static void SetCancelConnectRect(MainMenu& menu, SDL_Rect r) { menu.cancelConnectTapRect = r; }
    static bool PendingLobbyConnect(const MainMenu& menu) { return menu.pendingLobbyConnect; }
    static void SetPendingLobbyConnect(MainMenu& menu, bool v) { menu.pendingLobbyConnect = v; }
    // Keyboard navigation in the game room goes through MenuUpKey/
    // MenuDownKey/MenuReturnKey, which are private. Stand the menu up as
    // "already in a room" (the roster test does the same) and drive the real
    // Up/Down cycle directly.
    static void EnterNetRoom(MainMenu& menu) {
        menu.showingNetPanel = true;
        menu.networkInLobby = true;
        menu.networkInputMode = 0;
        menu.selectedActionIndex = 0;
    }
    static int SelectedActionIndex(const MainMenu& menu) {
        return menu.selectedActionIndex;
    }
    static void SetSelectedActionIndex(MainMenu& menu, int index) {
        menu.selectedActionIndex = index;
    }
    static void PressUp(MainMenu& menu) { menu.MenuUpKey(); }
    static void PressDown(MainMenu& menu) { menu.MenuDownKey(); }
    static void PressReturn(MainMenu& menu) { menu.MenuReturnKey(); }
    // Drives the real network-game "Start" path (SetupNewGame(4)) and
    // captures the SetupSettings it would have handed to NewGame(), via the
    // same test-hook pattern StartLocalGame uses for local multiplayer.
    // headlessTestMode short-circuits before touching FrozenBubble's real
    // BubbleGame, so this is safe without a live game/network stack.
    static SetupSettings StartNetworkGame(MainMenu& menu, bool& captured) {
        SetupSettings settings;
        captured = false;
        menu.testNetworkGameStart = [&](const SetupSettings& started) {
            settings = started;
            captured = true;
        };
        menu.SetupNewGame(4);
        menu.testNetworkGameStart = {};
        return settings;
    }
    // The lobby sidebar's optional "Join Discord server"/"Tournaments" row
    // indices -- private so the row list and the keyboard-navigation wrap in
    // mainmenu_input.cpp can't drift apart from what's actually drawn (see
    // CLAUDE.md's input-parity section).
    static int LobbyDiscordIndexOf(size_t roomCount) { return MainMenu::LobbyDiscordIndex(roomCount); }
    static int LobbyTournamentIndexOf(const MainMenu& menu, size_t roomCount) {
        return menu.LobbyTournamentIndex(roomCount);
    }
    static int ServerListDiscordIndexOf(const MainMenu& menu) { return menu.ServerListDiscordIndex(); }
    // Tournament bracket/browser panel (mainmenu_tournament.cpp).
    static void CallOpenTournament(MainMenu& menu, int id = 0) { menu.OpenTournament(id); }
    static void RenderTournamentPanel(MainMenu& menu) { menu.TournamentPanelRender(); }
    static bool SendTournamentPanelKey(MainMenu& menu, SDL_Event* e) { return menu.TournamentPanelKey(e); }
    static bool ShowingTournament(const MainMenu& menu) { return menu.showingTournament; }
    static void SetShowingTournament(MainMenu& menu, bool on) { menu.showingTournament = on; }
    static bool TournamentConfirmShowing(const MainMenu& menu) { return menu.tournamentConfirm; }
    static void SetTournamentConfirm(MainMenu& menu, bool on) { menu.tournamentConfirm = on; }
    static int TournamentSelection(const MainMenu& menu) { return menu.tournamentSelection; }
    static void SetTournamentSelection(MainMenu& menu, int index) { menu.tournamentSelection = index; }
    static void SetTournamentViewId(MainMenu& menu, int id) { menu.tournamentViewId = id; }
    static const std::vector<TournamentAction>& TournamentButtons(const MainMenu& menu) {
        return menu.tournamentButtons;
    }
};

// NetworkClient is a true singleton (NetworkClient::Instance()), and the
// room-roster test below needs to stand one up as "already in a >5-cap Team
// Mode game room" without a real socket. currentGame/playerNick are private
// for every ordinary caller, which only ever reaches them through a real
// server round-trip -- this friend (networkclient.h) is the test-only way in.
struct NetworkClientTestAccess {
    static void SetPlayerNick(NetworkClient& nc, const std::string& nick) {
        nc.playerNick = nick;
    }
    static void SetCurrentGame(NetworkClient& nc, GameRoom* game) {
        nc.currentGame = game;
    }
    static void SetState(NetworkClient& nc, ConnectionState state) {
        nc.state = state;
    }
    // TALK-message count, regardless of whether the (disconnected, in every
    // headless test) socket actually accepted them -- see its declaration in
    // networkclient.h for why this exists.
    static int TalkSendCount(const NetworkClient& nc) { return nc.testTalkSendCount; }
    // Injects a message as if it just arrived over the wire, so a test can
    // drive NetPanelChatDockRender's parsing without a real server echo.
    static void PushChatMessage(NetworkClient& nc, const std::string& nick,
                                const std::string& message) {
        nc.chatMessages.push_back({nick, message, 0});
    }
    // Stands up the lobby room list GetGameList() reads, so a lobby-index
    // test can drive a real row count instead of only the always-empty
    // default.
    static void SetGameList(NetworkClient& nc, std::vector<GameRoom> games) {
        nc.gameList = std::move(games);
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

    // The regression this pins: when a stepped row is pinned to the panel's
    // own bottom edge (the LAN/Net "Set name" section was, at the time --
    // see mainmenu_netpanel.cpp for why it turned out not to actually be a
    // stepped row at all), a full, deliberate edge-to-edge swipe back on an
    // iPhone routinely released right on top of it -- and unconditional
    // suppression on any stepped row broke "swipe back" there entirely, not
    // just the narrow accidental-drift case above. A swipe that travels
    // well past what an ordinary tap can drift is unambiguous: nobody
    // adjusts a stepped row's value by dragging 100+ logical units, only by
    // a stationary second tap, so this must still fire Back even on a
    // stepped row.
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

    // --- "Set name" must activate on tap, not step left/right ------------
    //
    // The actual defect, reported live on itch.io on both iPhone touch and
    // a plain desktop mouse click: the row was registered with
    // splitAdjust=true, which gives it the "<  value  >" look but also
    // means menulist::List::End splits it into two zones that each send
    // SDLK_LEFT or SDLK_RIGHT on a second tap -- never SDLK_RETURN. Neither
    // the LAN nor the Net server-list screen has a LEFT/RIGHT handler at
    // all (there is nothing to step -- the "value" is just the current
    // nickname, on display, not adjustable), so every tap silently did
    // nothing. A literal keyboard Enter still worked throughout, because it
    // reaches MenuReturnKey() directly and never consults a row's
    // activateKey at all -- which is exactly why this was invisible to
    // keyboard testing and only showed up as "the row highlights but
    // tapping it does nothing."
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
        MainMenuTestAccess::SetPublicServers(*menu, {{"host0", 1511, "", 10}});
        MainMenuTestAccess::RenderServerList(*menu, false);
        const int lastIdx = 1 + 1;  // 1 server -> Set name is index 2

        const std::vector<SDL_Rect> rects =
            MainMenuTestAccess::RectsForIndex(*menu, lastIdx);
        CHECK(rects.size() == 1);  // one plain zone, not a left/right split
        if (!rects.empty()) {
            CHECK(MainMenuTestAccess::ActivateKeyAt(*menu, lastIdx, 0) == 0);

            const SDL_Rect& row = rects[0];
            const float x = row.x + row.w * 0.5f;
            const float y = row.y + row.h * 0.5f;

            SDL_PumpEvents();
            for (SDL_Event drain; SDL_PollEvent(&drain); ) {}

            CHECK(menu->HandlePanelTap(x, y));       // select (first tap)
            CHECK(menu->HandlePanelTap(x, y));       // activate (second tap)

            SDL_Event ev;
            CHECK(SDL_PollEvent(&ev) && ev.type == SDL_EVENT_KEY_DOWN);
            CHECK(ev.key.key == SDLK_RETURN);
        }
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

    // --- IsWithinMenuTapDebounce -------------------------------------------
    // Now only backs FINGER_UP's own anti-rapid-refire guard (multi-finger
    // touch, OS double-events) -- WasmMouseEchoGuard below took over the
    // mouse-echo purpose this used to also serve.

    CHECK(IsWithinMenuTapDebounce(1000, 1000));
    CHECK(IsWithinMenuTapDebounce(1050, 1000));

    // Right at the edge of the window and just past it -- pins the
    // threshold itself rather than only "clearly inside/outside" cases.
    CHECK(IsWithinMenuTapDebounce(1000 + kMenuTapDebounceMs - 1, 1000));
    CHECK(!IsWithinMenuTapDebounce(1000 + kMenuTapDebounceMs, 1000));

    // A genuinely later, independent event must not be swallowed.
    CHECK(!IsWithinMenuTapDebounce(5000, 1000));

    // --- WasmMouseEchoGuard -------------------------------------------------
    //
    // The exact defect this replaced: a synthesized MOUSE_BUTTON_DOWN that
    // arrives late (past any fixed millisecond window, because the
    // browser's main thread was busy) used to slip through and dispatch as
    // an independent second tap, re-activating whatever row the first tap
    // had just selected -- reported live as the network room's Bots row
    // occasionally re-incrementing itself with no further tap. This guard
    // tracks state instead of a clock, so lateness alone can't defeat it.
    {
        WasmMouseEchoGuard guard;

        // No touch has happened yet -- a genuine standalone mouse click
        // (desktop-in-browser, no touch involved) must never be swallowed.
        CHECK(!guard.ShouldSwallowMouseDown());

        // A tap dispatches, and the browser owes it an echo. However late
        // that echo arrives, it must still be recognized -- this guard has
        // no notion of elapsed time at all, so "arbitrarily late" and
        // "immediately" are indistinguishable to it by construction.
        guard.OnFingerUpDispatched();
        CHECK(guard.ShouldSwallowMouseDown());

        // Consumed: exactly one echo per tap. A second MOUSE_BUTTON_DOWN
        // right after (no new FINGER_UP in between) is independent input,
        // not a leftover echo, and must be let through.
        CHECK(!guard.ShouldSwallowMouseDown());

        // A new touch starting must clear a still-pending flag -- otherwise
        // a previous tap's (apparently lost) echo would wrongly consume
        // this new, unrelated tap's own upcoming activation.
        guard.OnFingerUpDispatched();
        guard.OnFingerDown();
        CHECK(!guard.ShouldSwallowMouseDown());

        // The ordinary cycle repeats cleanly for a second tap.
        guard.OnFingerDown();
        guard.OnFingerUpDispatched();
        CHECK(guard.ShouldSwallowMouseDown());
        CHECK(!guard.ShouldSwallowMouseDown());
    }

    // --- >5-cap roster: touch reaching per-player team assignment
    //
    // Team assignment was reachable only through the [A] hotkey -- see the
    // comment on that check in MainMenu::HandleInput. A touch-only player in
    // a >5-cap room had no way to reach it at all: the compact
    // roster's rows registered no PanelTapRow, so a tap there just fell
    // through as a miss. This pins both halves of the fix: a roster row tap
    // (kRoomRosterTapBase's branch in HandlePanelTap) opens the full-screen
    // team picker on the row that was actually tapped rather than always row
    // 0, and one tap on a swatch there sets that team -- no blind cycling.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetPlayerNick(*nc, "host");
        GameRoom room;
        room.creator = "host";
        room.maxPlayers = 20;  // >5-cap -- the only cap that uses the compact roster
        room.players.push_back({"host", "", false});
        room.players.push_back({"joiner", "", false});
        room.players.push_back({"p3", "", false});
        room.players.push_back({"p4", "", false});
        room.players.push_back({"p5", "", false});
        room.players.push_back({"p6", "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);

        // currentGame is set, so this renders the game room, not the plain
        // lobby -- same function, see RenderLobbyActions's own comment.
        MainMenuTestAccess::RenderLobbyActions(*menu);

        const std::vector<SDL_Rect> hostRects =
            MainMenuTestAccess::RectsForIndex(*menu, kRoomRosterTapBase + 0);
        const std::vector<SDL_Rect> joinerRects =
            MainMenuTestAccess::RectsForIndex(*menu, kRoomRosterTapBase + 1);
        CHECK(hostRects.size() == 1);
        CHECK(joinerRects.size() == 1);
        if (!hostRects.empty() && !joinerRects.empty()) {
            auto center = [](const SDL_Rect& r) {
                return std::pair<float, float>(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
            };
            const auto [jx, jy] = center(joinerRects[0]);

            CHECK(!MainMenuTestAccess::TeamsPanelOpen(*menu));

            // Select, then confirm -- the same two-tap dance every other row
            // in this cramped roster uses, so the picker never opens from a
            // stray touch aimed somewhere else.
            CHECK(menu->HandlePanelTap(jx, jy));
            CHECK(!MainMenuTestAccess::TeamsPanelOpen(*menu));  // still just highlighted
            CHECK(menu->HandlePanelTap(jx, jy));
            CHECK(MainMenuTestAccess::TeamsPanelOpen(*menu));
            // Opened on the row that was actually tapped (index 1), not
            // always row 0 the way a bare [A] keypress would.
            CHECK(MainMenuTestAccess::TeamsCursor(*menu) == 1);

            // The picker is modal: every tap is now its own, and none reach
            // the roster still registered underneath it.
            SDL_PumpEvents();
            for (SDL_Event drain; SDL_PollEvent(&drain); ) {}

            // Measure the real populated picker before deciding whether its
            // text path merits a cache. The first render fills any cache; an
            // identical second render measures steady-state texture uploads.
            TTFTextTestAccess::ResetTextureCreationCount();
            MainMenuTestAccess::RenderTeamsPanel(*menu);
            const size_t teamPickerColdTextureCreates =
                TTFTextTestAccess::TextureCreationCount();
            TTFTextTestAccess::ResetTextureCreationCount();
            MainMenuTestAccess::RenderTeamsPanel(*menu);
            const size_t teamPickerIdleTextureCreates =
                TTFTextTestAccess::TextureCreationCount();
            std::fprintf(stderr,
                         "team picker texture creates: cold=%zu idle=%zu\n",
                         teamPickerColdTextureCreates, teamPickerIdleTextureCreates);
            // The five remaining uploads are DrawHeaderBar's action measure,
            // title, and action draws plus the footer path. The 50 repeated
            // picker labels themselves must remain cached.
            CHECK(teamPickerIdleTextureCreates <= 5);
            // Host, so every seat's no-team choice plus all five team choices
            // are tappable. All players begin unaffiliated.
            CHECK(MainMenuTestAccess::SwatchCount(*menu) ==
                  room.players.size() * (kMaxTeams + 1));
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 0) == kNoTeam);
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 1) == kNoTeam);

            // A single tap sets that exact team -- no cycling, no second tap.
            // Team 4 specifically: it is neither seat's round-robin default
            // (slot 0 -> 1, slot 1 -> 2), so landing on it can only be this
            // tap's doing.
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 1) != 4);
            float sx = 0, sy = 0;
            CHECK(MainMenuTestAccess::SwatchCenter(*menu, 1, 4, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 1) == 4);
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 0) != 4);  // only the tapped row moved

            // The no-team swatch restores free-agent play directly.
            CHECK(MainMenuTestAccess::SwatchCenter(*menu, 1, kNoTeam, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 1) == kNoTeam);

            // The player's name is a second, larger cycling target. It walks
            // through the same choice order as keyboard navigation, including
            // no-team at the wrap boundary.
            CHECK(MainMenuTestAccess::PlayerNameCenter(*menu, 1, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 1) == 1);
            CHECK(MainMenuTestAccess::SwatchCenter(*menu, 1, 5, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            CHECK(MainMenuTestAccess::PlayerNameCenter(*menu, 1, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 1) == kNoTeam);

            // Auto 3 round-robins every occupied seat and leaves the manual
            // Team 4/5 choices available afterward. This is a >5-cap room,
            // so every seat's change is real wire traffic (no per-slot
            // SETOPTIONS shortcut here) -- but it must still be one combined
            // "!teamset:" message for the whole tap, not one "!team:" per
            // seat (6 seats changing here; ApplyTeamChoicesBatch's batching
            // is what keeps that at 1, see the dedicated flood test below for
            // a room sized to actually reach the server's limit unbatched).
            const int talkBefore = NetworkClientTestAccess::TalkSendCount(*nc);
            CHECK(MainMenuTestAccess::AutoBalanceCenter(*menu, 3, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            CHECK(NetworkClientTestAccess::TalkSendCount(*nc) - talkBefore == 1);
            const int autoThree[] = {1, 2, 3, 1, 2, 3};
            for (int slot = 0; slot < 6; ++slot)
                CHECK(MainMenuTestAccess::TeamOfSlot(*menu, slot) == autoThree[slot]);

            // The NONE button undoes Auto 3 in the same one tap it took to
            // apply it -- every occupied seat back to a free agent, batched
            // into the same single "!teamset:" TALK an Auto button sends
            // rather than one "!team:" per seat.
            const int talkBeforeNone = NetworkClientTestAccess::TalkSendCount(*nc);
            CHECK(MainMenuTestAccess::AutoBalanceCenter(*menu, kNoTeam, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            CHECK(NetworkClientTestAccess::TalkSendCount(*nc) - talkBeforeNone == 1);
            for (int slot = 0; slot < 6; ++slot)
                CHECK(MainMenuTestAccess::TeamOfSlot(*menu, slot) == kNoTeam);

            // Re-apply Auto 3 so the rows below still have something to work
            // from.
            CHECK(MainMenuTestAccess::AutoBalanceCenter(*menu, 3, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            for (int slot = 0; slot < 6; ++slot)
                CHECK(MainMenuTestAccess::TeamOfSlot(*menu, slot) == autoThree[slot]);
            MainMenuTestAccess::RenderTeamsPanel(*menu);
            CHECK(MainMenuTestAccess::SwatchCenter(*menu, 0, 5, &sx, &sy));

            // Nothing was injected as a keypress on the way -- the picker
            // acts directly, so a stale key can't reach the room on close.
            // Only key events are asserted on: rendering pumps the queue and
            // SDL puts its own window/display events there.
            for (SDL_Event ev; SDL_PollEvent(&ev); )
                CHECK(ev.type != SDL_EVENT_KEY_DOWN);

            // Tapping the host's own row from the picker moves the cursor
            // there and sets its team in the same tap, both rows being the
            // host's to change.
            CHECK(MainMenuTestAccess::SwatchCenter(*menu, 0, 4, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            CHECK(MainMenuTestAccess::TeamsCursor(*menu) == 0);
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 0) == 4);

            // ESC closes the page -- the route a keyboard, a gamepad's B
            // (SDLK_AC_BACK), a right-click and a back-swipe all arrive by.
            SDL_Event esc = {};
            esc.type = SDL_EVENT_KEY_DOWN;
            esc.key.key = SDLK_ESCAPE;
            menu->HandleInput(&esc);
            CHECK(!MainMenuTestAccess::TeamsPanelOpen(*menu));

            // And the visible "Done" button does the same, for a player with
            // no keyboard at all. Its rect must be a real drawn one: it comes
            // back from DrawHeaderBar, which does not draw the action (and so
            // publishes no rect) unless it is asked for a real action index --
            // an empty rect here means the button is invisible on screen, and
            // a touch-only player would be stuck on a full-screen page.
            MainMenuTestAccess::SetTeamsPanelOpen(*menu, true);
            MainMenuTestAccess::RenderTeamsPanel(*menu);
            const SDL_Rect done = MainMenuTestAccess::DoneRect(*menu);
            CHECK(done.w > 0 && done.h > 0);
            CHECK(menu->HandlePanelTap(done.x + done.w * 0.5f, done.y + done.h * 0.5f));
            CHECK(!MainMenuTestAccess::TeamsPanelOpen(*menu));
        }

        // Don't leak this fake room into any test that runs after this one.
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // --- >5-cap room whose actual roster is <=5: team choices must survive
    // into the started game --------------------------------------------
    //
    // Reported live: "Create 20 player game / set clear mode / add 4 bots /
    // set auto 3 teams / start game / results don't show teams after one
    // round." A 20-player room only 1 human + 4 bots actually joined is
    // exactly the >5-cap, <=5-actual-players case: the Set Teams UI
    // (TeamOfSlot/ApplyTeamChoicesBatch, mainmenu_teampanel.cpp) decides
    // where to store a choice by room->maxPlayers > 5, so with a 20-cap room
    // it always writes to the nick-keyed netTeamOverrides map, never to the
    // flat netPlayerTeams[] grid -- regardless of how many seats are
    // actually filled. SetupNewGame's own case 4 used to decide which of
    // those two maps to *read back* by playerCount > 5, where playerCount is
    // players.size() (the actual join count, 5 here) -- not > 5, so it read
    // the untouched netPlayerTeams[] grid instead and every team came back
    // kNoTeam. This pins the fix: reading back must key off the same
    // room->maxPlayers > 5 the UI used to store it.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetPlayerNick(*nc, "host");
        GameRoom room;
        room.creator = "host";
        room.maxPlayers = 20;  // the room's cap, not its actual occupancy
        room.players.push_back({"host", "", false});
        room.players.push_back({"bot1", "", false});
        room.players.push_back({"bot2", "", false});
        room.players.push_back({"bot3", "", false});
        room.players.push_back({"bot4", "", false});  // 5 actual players, <= 5
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);

        MainMenuTestAccess::RenderLobbyActions(*menu);
        // Open the picker (two-tap dance) on the host's own row and apply
        // Auto 3, exactly as a player would from the roster.
        const std::vector<SDL_Rect> hostRects =
            MainMenuTestAccess::RectsForIndex(*menu, kRoomRosterTapBase + 0);
        CHECK(hostRects.size() == 1);
        if (!hostRects.empty()) {
            const float hx = hostRects[0].x + hostRects[0].w * 0.5f;
            const float hy = hostRects[0].y + hostRects[0].h * 0.5f;
            CHECK(menu->HandlePanelTap(hx, hy));
            CHECK(menu->HandlePanelTap(hx, hy));
            CHECK(MainMenuTestAccess::TeamsPanelOpen(*menu));
            SDL_PumpEvents();
            for (SDL_Event drain; SDL_PollEvent(&drain); ) {}
            MainMenuTestAccess::RenderTeamsPanel(*menu);

            float sx = 0, sy = 0;
            CHECK(MainMenuTestAccess::AutoBalanceCenter(*menu, 3, &sx, &sy));
            CHECK(menu->HandlePanelTap(sx, sy));
            const int autoThree[] = {1, 2, 3, 1, 2};
            for (int slot = 0; slot < 5; ++slot)
                CHECK(MainMenuTestAccess::TeamOfSlot(*menu, slot) == autoThree[slot]);

            // Close the picker the same way a player would (Done), then
            // start the game and inspect exactly what SetupNewGame(4) built.
            MainMenuTestAccess::RenderTeamsPanel(*menu);
            const SDL_Rect done = MainMenuTestAccess::DoneRect(*menu);
            CHECK(menu->HandlePanelTap(done.x + done.w * 0.5f, done.y + done.h * 0.5f));
            CHECK(!MainMenuTestAccess::TeamsPanelOpen(*menu));

            bool captured = false;
            SetupSettings started =
                MainMenuTestAccess::StartNetworkGame(*menu, captured);
            CHECK(captured);
            CHECK(started.playerCount == 5);
            for (int slot = 0; slot < 5; ++slot)
                CHECK(started.playerTeams[slot] == autoThree[slot]);
        }

        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // --- Keyboard navigation to the header's "Set Teams" button ----------
    //
    // The button (kRoomSetTeamsTapIndex) sits in the players-sidebar header,
    // deliberately outside the GameRoomRow range so it doesn't renumber the
    // rows built positionally below it. That used to mean a keyboard/gamepad
    // player could only reach it via the [A] hotkey, never by navigating to
    // it with Up/Down and pressing Enter -- it is now a stop in the same wrap
    // cycle as the real rows.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetPlayerNick(*nc, "host");
        NetworkClientTestAccess::SetState(*nc, IN_LOBBY);
        GameRoom room;
        room.creator = "host";
        room.maxPlayers = 5;
        room.players.push_back({"host", "", false});
        room.players.push_back({"joiner", "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);

        MainMenuTestAccess::EnterNetRoom(*menu);
        MainMenuTestAccess::RenderLobbyActions(*menu);

        // The button must really be registered, or there is nothing to land on.
        CHECK(!MainMenuTestAccess::RectsForIndex(*menu, kRoomSetTeamsTapIndex).empty());

        // Up from Chat wraps to the header button, which sits above the list.
        MainMenuTestAccess::SetSelectedActionIndex(*menu, 0);
        MainMenuTestAccess::PressUp(*menu);
        CHECK(MainMenuTestAccess::SelectedActionIndex(*menu) == kRoomSetTeamsTapIndex);

        // Down from the button wraps back to Chat.
        MainMenuTestAccess::PressDown(*menu);
        CHECK(MainMenuTestAccess::SelectedActionIndex(*menu) == 0);

        // Down from the host's last real row (Start, index 13 in a 2-player
        // room) now reaches the HELP box first -- it sits between the last
        // row and the header button, the one stop that used to be reachable
        // only by tap or blind F1 (see MenuDownKey/MenuUpKey).
        MainMenuTestAccess::SetSelectedActionIndex(*menu, kRoomStart);
        MainMenuTestAccess::PressDown(*menu);
        CHECK(MainMenuTestAccess::SelectedActionIndex(*menu) == kRoomHelpTapIndex);

        // Down again continues past HELP to the header button -- the same
        // wrap that used to go straight from Start to Chat.
        MainMenuTestAccess::PressDown(*menu);
        CHECK(MainMenuTestAccess::SelectedActionIndex(*menu) == kRoomSetTeamsTapIndex);

        // Enter on the highlighted button opens the picker for host and joiner
        // alike -- the one activation the button's [A]/tap paths already had.
        CHECK(!MainMenuTestAccess::TeamsPanelOpen(*menu));
        MainMenuTestAccess::PressReturn(*menu);
        CHECK(MainMenuTestAccess::TeamsPanelOpen(*menu));

        // Don't leak this fake room into any test that runs after this one.
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // --- Auto-balance flood regression: cycling Auto 2..5 in a <=5-cap room
    // full of bots must not get the host kicked.
    //
    // The server terminates a connection that sends 15 TALK messages inside
    // one minute (server/game.c: amount_talk_flood). ApplyTeamChoice used to
    // send one "!team:<nick>:<n>" TALK per player it touched, unconditionally
    // -- including the host's own change in a <=5-cap room, where
    // SyncRoomOptions() already broadcasts every player's team via
    // SETOPTIONS' PLAYERTEAM_Pn fields and the TALK was pure redundant
    // chatter. Auto-balance applies every occupied seat in one loop, so in a
    // full 5-player room (this test's host + 4 bots) each Auto tap sent 5
    // TALKs -- 3 taps hit the flood limit exactly (5x3=15), which is the
    // "cycle thru Auto 2, 3, 4, 5" sequence a user actually reported getting
    // kicked over.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetPlayerNick(*nc, "host");
        GameRoom room;
        room.creator = "host";
        room.maxPlayers = 5;  // <=5-cap -- the host-intercept/SyncRoomOptions path
        room.players.push_back({"host", "", false});
        room.players.push_back({"bot1", "", false});
        room.players.push_back({"bot2", "", false});
        room.players.push_back({"bot3", "", false});
        room.players.push_back({"bot4", "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);

        MainMenuTestAccess::SetTeamsPanelOpen(*menu, true);
        MainMenuTestAccess::RenderTeamsPanel(*menu);

        const int startCount = NetworkClientTestAccess::TalkSendCount(*nc);
        for (int teamCount = 2; teamCount <= kMaxTeams; ++teamCount) {
            float ax = 0, ay = 0;
            CHECK(MainMenuTestAccess::AutoBalanceCenter(*menu, teamCount, &ax, &ay));
            CHECK(menu->HandlePanelTap(ax, ay));
            MainMenuTestAccess::RenderTeamsPanel(*menu);  // re-publish rects for the next tap
        }
        // Well under the server's 15-TALK/minute flood-kick threshold --
        // in fact exactly zero, since a <=5-cap room's host never needs TALK
        // at all (SyncRoomOptions covers it). This is the count that used to
        // reach 15 by the third tap and get the host disconnected.
        CHECK(NetworkClientTestAccess::TalkSendCount(*nc) - startCount == 0);

        // The actual team assignment is still correct: Auto 5 with 5
        // occupied seats gives each player their own team, in slot order.
        for (int slot = 0; slot < 5; ++slot)
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, slot) == slot + 1);

        // Don't leak this fake room into any test that runs after this one.
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // --- Auto-balance flood regression, >5-cap room: a single Auto tap in a
    // big room must not send one wire message per seat.
    //
    // A >5-cap room has no per-slot team field in SETOPTIONS, so a team
    // change there really does need one "!team:<nick>:<n>" TALK per changed
    // seat -- unlike the <=5-cap case above, this traffic is not redundant.
    // But sending it unbatched does not scale: a 16-player room (host + 15
    // bots) tapping Auto would send 16 separate TALKs from *one* tap, already
    // past the server's 15-TALK/minute flood-kick limit before the user does
    // anything else. ApplyTeamChoicesBatch combines a whole tap's worth of
    // changes into one "!teamset:nick=team,..." message instead.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetPlayerNick(*nc, "host");
        GameRoom room;
        room.creator = "host";
        room.maxPlayers = 20;  // >5-cap -- the "!teamset:" batched-TALK path
        room.players.push_back({"host", "", false});
        for (int i = 1; i < 16; ++i)
            room.players.push_back({"bot" + std::to_string(i), "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);

        MainMenuTestAccess::SetTeamsPanelOpen(*menu, true);
        MainMenuTestAccess::RenderTeamsPanel(*menu);

        const int startCount = NetworkClientTestAccess::TalkSendCount(*nc);
        float ax = 0, ay = 0;
        CHECK(MainMenuTestAccess::AutoBalanceCenter(*menu, 4, &ax, &ay));
        CHECK(menu->HandlePanelTap(ax, ay));
        // One combined message for all 16 seats, not 16 -- comfortably under
        // the flood limit regardless of room size, and the actual count this
        // used to be before batching (one per seat that changed).
        CHECK(NetworkClientTestAccess::TalkSendCount(*nc) - startCount == 1);

        // The assignment underneath is still correct: Auto 4 round-robins
        // teams 1-4 across all 16 seats in slot order.
        for (int slot = 0; slot < 16; ++slot)
            CHECK(MainMenuTestAccess::TeamOfSlot(*menu, slot) == (slot % 4) + 1);

        // Cycling through the rest (2, 3, 5) stays at one message per tap
        // each, never accumulating toward the flood limit the way one
        // message per seat would across several taps.
        for (int teamCount : {2, 3, 5}) {
            const int before = NetworkClientTestAccess::TalkSendCount(*nc);
            CHECK(MainMenuTestAccess::AutoBalanceCenter(*menu, teamCount, &ax, &ay));
            CHECK(menu->HandlePanelTap(ax, ay));
            CHECK(NetworkClientTestAccess::TalkSendCount(*nc) - before == 1);
            MainMenuTestAccess::RenderTeamsPanel(*menu);
        }

        // Don't leak this fake room into any test that runs after this one.
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // --- "!teamset:" receiving side: a >5-cap room's other clients must
    // actually apply the batched message the sender above builds, not just
    // avoid sending too many of them.
    //
    // Every client in a >5-cap room (not just the host) applies team changes
    // straight from chat traffic -- there is no host-authoritative broadcast
    // for P6-20 (see ApplyTeamChoice's comment). This drives that parsing
    // path directly with a synthetic message, standing in for the echo a
    // real server would have relayed back from ApplyTeamChoicesBatch.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetPlayerNick(*nc, "joiner");
        GameRoom room;
        room.creator = "host";
        room.maxPlayers = 20;
        room.players.push_back({"host", "", false});
        room.players.push_back({"joiner", "", false});
        room.players.push_back({"bot1", "", false});
        room.players.push_back({"bot2", "", false});
        room.players.push_back({"bot3", "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);

        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 2) == kNoTeam);
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 3) == kNoTeam);
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 4) == kNoTeam);

        NetworkClientTestAccess::PushChatMessage(*nc, "host", "!teamset:bot1=2,bot2=0,bot3=3");
        MainMenuTestAccess::RenderChatDock(*menu);

        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 2) == 2);     // bot1
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 3) == kNoTeam);  // bot2, explicitly no-team
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 4) == 3);     // bot3
        // Unrelated seats -- host and this client's own row -- are untouched.
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 0) == kNoTeam);
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 1) == kNoTeam);

        // A second batch overwrites rather than merges/leaks state from the
        // first -- bot1 moves off team 2, and a seat the new batch doesn't
        // mention (bot3) keeps its previous value untouched.
        NetworkClientTestAccess::PushChatMessage(*nc, "host", "!teamset:bot1=1,bot2=1");
        MainMenuTestAccess::RenderChatDock(*menu);
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 2) == 1);
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 3) == 1);
        CHECK(MainMenuTestAccess::TeamOfSlot(*menu, 4) == 3);

        // Don't leak this fake room into any test that runs after this one.
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // --- Settings grid per-cell texture caching (handoff item B, menu
    // portion): NetPanelLobbyActionsRender's "ALL / P1..PN" grid used to run
    // every cell (headers, row labels, values) through one shared panelText,
    // so each cell's different text invalidated the previous cell's cached
    // texture every single call -- the same "alternating cells" defeat the
    // gameplay stats table had before StatsPanelCell. NetGridCell fixes it
    // the same way: one persistent pool slot per cell, addressed by call
    // order. This proves an unchanged cell's texture survives a second,
    // otherwise-identical render, and that changing one player's value does
    // not disturb an unrelated cell's cached texture.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetPlayerNick(*nc, "host");
        GameRoom room;
        room.creator = "host";
        room.maxPlayers = 5;  // <=5-cap -- the grid only renders for a real game room
        room.players.push_back({"host", "", false});
        room.players.push_back({"joiner", "", false});
        room.players.push_back({"p3", "", false});
        NetworkClientTestAccess::SetCurrentGame(*nc, &room);

        MainMenuTestAccess::RenderLobbyActions(*menu);

        // 3 players: 1 ("ALL") + 3 (P1..P3) header cells, then 4 rows of
        // (1 label + 1 ALL-value + 3 per-player values) = 4 + 4*5 = 24.
        CHECK(MainMenuTestAccess::NetGridCellPoolSize(*menu) == 24);

        const char* marker = "test_marker";
        SDL_Texture* allHeaderTex = MainMenuTestAccess::NetGridCellTexture(*menu, 0);
        SDL_Texture* rowLabelTex = MainMenuTestAccess::NetGridCellTexture(*menu, 4);  // "Max colors:"
        CHECK(allHeaderTex != nullptr);
        CHECK(rowLabelTex != nullptr);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(allHeaderTex), marker, true);
        SDL_SetBooleanProperty(SDL_GetTextureProperties(rowLabelTex), marker, true);

        // Re-render with nothing in the room changed: every cell's text is
        // identical to last frame, so every cell should keep its texture.
        MainMenuTestAccess::RenderLobbyActions(*menu);
        CHECK(MainMenuTestAccess::NetGridCellPoolSize(*menu) == 24);
        CHECK(SDL_GetBooleanProperty(SDL_GetTextureProperties(
            MainMenuTestAccess::NetGridCellTexture(*menu, 0)), marker, false));
        CHECK(SDL_GetBooleanProperty(SDL_GetTextureProperties(
            MainMenuTestAccess::NetGridCellTexture(*menu, 4)), marker, false));

        // Change one player's color count -- only that one value cell's text
        // actually changes. The "ALL" header and the "Max colors:" row label
        // (neither of which depends on per-player values) must keep their
        // cached textures rather than losing them to a sibling cell's update,
        // which is exactly the bug this pool exists to prevent.
        MainMenuTestAccess::SetPlayerColorCount(*menu, 1, 5);
        MainMenuTestAccess::RenderLobbyActions(*menu);
        CHECK(SDL_GetBooleanProperty(SDL_GetTextureProperties(
            MainMenuTestAccess::NetGridCellTexture(*menu, 0)), marker, false));
        CHECK(SDL_GetBooleanProperty(SDL_GetTextureProperties(
            MainMenuTestAccess::NetGridCellTexture(*menu, 4)), marker, false));

        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
    }

    // --- Online lobby sidebar: "Join Discord server" / "Tournaments" row
    // indices, with and without each, must match what MenuUpKey/MenuDownKey's
    // maxActions count in mainmenu_input.cpp actually navigates to, and both
    // rows must be real tap targets -- the exact input-parity gap CLAUDE.md
    // calls out (a row rendered but never registered as a tap target, or
    // reachable by only one of keyboard/tap). LobbyDiscordIndex(rooms) is
    // always `2 + rooms` when present, and LobbyTournamentIndex(rooms) rides
    // one further out when Discord is also showing -- so with both present,
    // Discord comes first and Tournaments last; drop either one and the
    // other closes the gap rather than leaving a hole. Runs the real
    // NetPanelLobbyActionsRender/MenuUpKey/MenuDownKey against all four
    // combinations, not just the one this build ships with.
    {
        NetworkClient* nc = NetworkClient::Instance();
        NetworkClientTestAccess::SetPlayerNick(*nc, "host");
        NetworkClientTestAccess::SetState(*nc, IN_LOBBY);
        NetworkClientTestAccess::SetCurrentGame(*nc, nullptr);
        std::vector<GameRoom> games;
        games.push_back(GameRoom{});
        games.push_back(GameRoom{});
        games[0].creator = "alice"; games[0].maxPlayers = 5;
        games[1].creator = "bob"; games[1].maxPlayers = 5;
        NetworkClientTestAccess::SetGameList(*nc, games);
        const size_t roomCount = games.size();  // 2

        for (bool discordOn : {true, false}) {
            for (bool tournamentsOn : {true, false}) {
                testForceDiscordInviteOff = !discordOn;
                nc->tournaments.supported = tournamentsOn;

                std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
                MainMenuTestAccess::EnterNetRoom(*menu);
                MainMenuTestAccess::RenderLobbyActions(*menu);

                const int discordIdx = MainMenuTestAccess::LobbyDiscordIndexOf(roomCount);
                const int tourIdx = MainMenuTestAccess::LobbyTournamentIndexOf(*menu, roomCount);
                CHECK((discordIdx >= 0) == discordOn);
                CHECK((tourIdx >= 0) == tournamentsOn);
                if (discordOn && tournamentsOn) {
                    // Discord (2 + rooms) must sit strictly before Tournaments
                    // (2 + rooms + 1) -- the fixed ordering both
                    // NetPanelLobbyActionsRender and the maxActions count
                    // agree on.
                    CHECK(tourIdx == discordIdx + 1);
                }

                // Whichever rows are present must each be a real, separately
                // tappable row -- not merged into, or missing from, the row
                // list a keyboard would also see.
                if (discordIdx >= 0)
                    CHECK(!MainMenuTestAccess::RectsForIndex(*menu, discordIdx).empty());
                if (tourIdx >= 0)
                    CHECK(!MainMenuTestAccess::RectsForIndex(*menu, tourIdx).empty());
                // And no stray row at the "other configuration"'s position:
                // e.g. with Discord off, index 2+roomCount+1 (where
                // Tournaments would sit if Discord were also on) must not
                // silently have Tournaments' row squatting there instead.
                if (!discordOn) {
                    const int wouldBeIdxIfDiscordOn = 2 + (int)roomCount + 1;
                    if (wouldBeIdxIfDiscordOn != tourIdx)
                        CHECK(MainMenuTestAccess::RectsForIndex(*menu, wouldBeIdxIfDiscordOn).empty());
                }

                // Keyboard reach: Up from the very first row (Chat, index 0)
                // wraps all the way around to the last row. The last row is
                // whichever of Tournaments/Discord/last-room-row is actually
                // present, in that priority order.
                const int expectedLast = tourIdx >= 0 ? tourIdx
                                        : discordIdx >= 0 ? discordIdx
                                        : 1 + (int)roomCount;
                MainMenuTestAccess::SetSelectedActionIndex(*menu, 0);
                MainMenuTestAccess::PressUp(*menu);
                CHECK(MainMenuTestAccess::SelectedActionIndex(*menu) == expectedLast);

                // With both rows present, one more Up steps from Tournaments
                // back to Discord -- neither row is an island only reachable
                // by wrapping past it, both are ordinary stops in the cycle.
                if (discordOn && tournamentsOn) {
                    MainMenuTestAccess::PressUp(*menu);
                    CHECK(MainMenuTestAccess::SelectedActionIndex(*menu) == discordIdx);
                }

                // And Down from the last row wraps back to Chat, the same
                // cycle in the other direction.
                MainMenuTestAccess::SetSelectedActionIndex(*menu, expectedLast);
                MainMenuTestAccess::PressDown(*menu);
                CHECK(MainMenuTestAccess::SelectedActionIndex(*menu) == 0);
            }
        }

        // Restore defaults so no later test in this binary inherits them.
        testForceDiscordInviteOff = false;
        nc->tournaments.supported = false;
        NetworkClientTestAccess::SetGameList(*nc, {});
        NetworkClientTestAccess::SetState(*nc, DISCONNECTED);
    }

    // --- Tournament withdrawal confirmation: visible keyboard focus --------
    //
    // CLAUDE.md's input-parity rule specifically calls out a two-button
    // popup where ENTER/ESC could each fire one button but there was no
    // keyboard way to move focus onto the non-default button and no visible
    // indicator of which one was focused. TournamentPanelKey's LEFT/RIGHT/TAB
    // moves tournamentSelection between "Stay" (0) and "Withdraw" (1), ENTER
    // activates whichever is focused, and TournamentPanelRender highlights
    // the focused button (drawSelection background) the same way every other
    // selectable row in this game does -- so this pins that the focus
    // actually moves, that ENTER really does activate the currently
    // highlighted button rather than always the same one, and that ESC still
    // cancels outright as the documented shortcut.
    //
    // TournamentPanelRender rebuilds tournamentButtons to exactly the
    // {"Stay","STAY"},{"Withdraw","LEAVE <id>"} pair whenever tournamentConfirm
    // is set, regardless of what (if any) snapshot the panel is otherwise
    // showing -- so the confirmation state is forced directly, the same state
    // a real WITHDRAW row's RETURN press transitions into
    // (mainmenu_tournament.cpp), isolating the focus/ESC behavior under test
    // from the unrelated registration-flow plumbing that leads into it.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
        NetworkClient* nc = NetworkClient::Instance();
        // TournamentPanelRender bails (and clears showingTournament) unless
        // IsConnected() -- same guard the real lobby is behind when this
        // panel is reachable at all.
        NetworkClientTestAccess::SetState(*nc, IN_LOBBY);

        MainMenuTestAccess::SetShowingTournament(*menu, true);
        MainMenuTestAccess::SetTournamentViewId(*menu, 7);
        MainMenuTestAccess::SetTournamentConfirm(*menu, true);
        MainMenuTestAccess::SetTournamentSelection(*menu, 0);
        MainMenuTestAccess::RenderTournamentPanel(*menu);

        const auto& buttons = MainMenuTestAccess::TournamentButtons(*menu);
        CHECK(buttons.size() == 2);
        CHECK(buttons[0].command == "STAY");
        CHECK(buttons[1].command == "LEAVE 7");

        // Starts focused on "Stay" (index 0) -- the safer default a stray
        // ENTER should land on, matching confirmDialogFocusNo's own
        // Yes/No-default precedent in mainmenu.h.
        CHECK(MainMenuTestAccess::TournamentSelection(*menu) == 0);

        SDL_Event right{}; right.type = SDL_EVENT_KEY_DOWN; right.key.key = SDLK_RIGHT;
        SDL_Event left{};  left.type  = SDL_EVENT_KEY_DOWN; left.key.key  = SDLK_LEFT;
        SDL_Event tab{};   tab.type   = SDL_EVENT_KEY_DOWN; tab.key.key   = SDLK_TAB;
        SDL_Event ret{};   ret.type   = SDL_EVENT_KEY_DOWN; ret.key.key   = SDLK_RETURN;
        SDL_Event esc{};   esc.type   = SDL_EVENT_KEY_DOWN; esc.key.key   = SDLK_ESCAPE;

        // RIGHT/LEFT/TAB all move focus between the two buttons -- the
        // visible highlight TournamentPanelRender draws (i ==
        // tournamentSelection) tracks this same field, so moving it is what
        // makes the focus visible on screen, not just inferred from which
        // key does what.
        CHECK(MainMenuTestAccess::SendTournamentPanelKey(*menu, &right));
        CHECK(MainMenuTestAccess::TournamentSelection(*menu) == 1);  // Withdraw now focused
        CHECK(MainMenuTestAccess::SendTournamentPanelKey(*menu, &left));
        CHECK(MainMenuTestAccess::TournamentSelection(*menu) == 0);  // back to Stay
        CHECK(MainMenuTestAccess::SendTournamentPanelKey(*menu, &tab));
        CHECK(MainMenuTestAccess::TournamentSelection(*menu) == 1);

        // ENTER activates whichever button is actually focused, not always
        // the same one: with Withdraw focused, ENTER's the "LEAVE 7" path,
        // which closes the confirmation and clears the selection back to 0
        // (mainmenu_tournament.cpp's `command.compare(0, 6, "LEAVE ") == 0`
        // branch), never actually calling TournamentCommand() here since no
        // NetworkClient connection is standing -- SendCommand's own
        // DISCONNECTED guard makes that a safe, side-effect-free no-op, and
        // is not what this test is pinning.
        CHECK(MainMenuTestAccess::SendTournamentPanelKey(*menu, &ret));
        CHECK(!MainMenuTestAccess::TournamentConfirmShowing(*menu));
        CHECK(MainMenuTestAccess::TournamentSelection(*menu) == 0);

        // Re-open the confirmation and prove the opposite: with "Stay"
        // focused (the default), ENTER takes the STAY branch instead --
        // dismissing the confirmation without ever reaching "LEAVE 7". If
        // focus were cosmetic only (ENTER always firing one fixed button
        // regardless of tournamentSelection), this and the case above would
        // be indistinguishable; this pins that the highlighted button is the
        // one that actually activates.
        MainMenuTestAccess::SetTournamentConfirm(*menu, true);
        MainMenuTestAccess::SetTournamentSelection(*menu, 0);
        MainMenuTestAccess::RenderTournamentPanel(*menu);
        CHECK(MainMenuTestAccess::SendTournamentPanelKey(*menu, &ret));
        CHECK(!MainMenuTestAccess::TournamentConfirmShowing(*menu));

        // ESC still cancels outright as the documented shortcut, regardless
        // of which button currently has focus.
        MainMenuTestAccess::SetTournamentConfirm(*menu, true);
        MainMenuTestAccess::SetTournamentSelection(*menu, 1);  // Withdraw focused
        MainMenuTestAccess::RenderTournamentPanel(*menu);
        CHECK(MainMenuTestAccess::SendTournamentPanelKey(*menu, &esc));
        CHECK(!MainMenuTestAccess::TournamentConfirmShowing(*menu));
        // ESC on the confirmation only backs out of it, leaving the
        // tournament panel itself open (a second ESC leaves that) -- pins
        // that it doesn't blow past two levels at once.
        CHECK(MainMenuTestAccess::ShowingTournament(*menu));

        // Each button is also its own tap target, not just a keyboard focus
        // stop -- both halves of the input-parity rule, same as every other
        // row this file tests.
        MainMenuTestAccess::SetTournamentConfirm(*menu, true);
        MainMenuTestAccess::SetTournamentSelection(*menu, 0);
        MainMenuTestAccess::RenderTournamentPanel(*menu);
        const std::vector<SDL_Rect> withdrawRects = MainMenuTestAccess::RectsForIndex(*menu, 1);
        CHECK(!withdrawRects.empty());
        if (!withdrawRects.empty()) {
            const SDL_Rect& r = withdrawRects[0];
            const float tx = r.x + r.w * 0.5f, ty = r.y + r.h * 0.5f;

            SDL_PumpEvents();
            for (SDL_Event drain; SDL_PollEvent(&drain); ) {}

            // Same two-tap dance as every other plain row in this file
            // (HandlePanelTap only activates a row that was already
            // selected): starting on "Stay" (index 0), a first tap on
            // "Withdraw" only moves focus there.
            CHECK(menu->HandlePanelTap(tx, ty));
            CHECK(MainMenuTestAccess::TournamentConfirmShowing(*menu));
            CHECK(MainMenuTestAccess::TournamentSelection(*menu) == 1);

            // The second tap, now on the already-selected row, pushes the
            // activation as a key event (same as every other plain row) --
            // pump it back through the real HandleInput to see it actually
            // take effect, not just that the right key was queued.
            CHECK(menu->HandlePanelTap(tx, ty));
            SDL_Event ev;
            CHECK(SDL_PollEvent(&ev) && ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_RETURN);
            menu->HandleInput(&ev);
            CHECK(!MainMenuTestAccess::TournamentConfirmShowing(*menu));
        }

        MainMenuTestAccess::SetShowingTournament(*menu, false);
        NetworkClientTestAccess::SetState(*nc, DISCONNECTED);
    }

#if !defined(__ANDROID__) && !defined(__WASM_PORT__) && !defined(_WIN32) && !defined(__IOS_PORT__)
    // --- StartLocalServer polls for readiness instead of sleeping a fixed
    // second (handoff item A). Drives the real fork+exec+poll path against
    // the actual fb-server binary this build produced, and checks both that
    // it returns well under the old fixed delay and that the port is
    // genuinely accepting connections by the time it returns -- not just
    // that it didn't crash.
    {
        std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);
        const int testPort = 15519;  // Distinct from the Python server tests' 15512/15513/15517.
        MainMenuTestAccess::SetNetworkPort(*menu, testPort);

        Uint64 start = SDL_GetTicks();
        MainMenuTestAccess::CallStartLocalServer(*menu);
        Uint64 elapsedMs = SDL_GetTicks() - start;

        CHECK(MainMenuTestAccess::IsServerHosting(*menu));
        // fork+exec+bind+listen for this server is normally single-digit to
        // low-double-digit milliseconds; 900ms leaves comfortable headroom
        // while still failing if this ever regresses back to a fixed-delay
        // wait (the old behavior always took >=1000ms here).
        CHECK(elapsedMs < 900);
        if (elapsedMs >= 900)
            std::fprintf(stderr, "  (StartLocalServer took %llu ms)\n",
                         (unsigned long long)elapsedMs);
        // Not just "didn't crash" -- something is actually listening on the
        // port by the time the call returns.
        CHECK(portInUse(testPort));

        // --- NetworkClient::Connect against this same real server (handoff
        // item A): the connect() call and the socket's blocking mode both
        // changed (non-blocking + select()-bounded connect on POSIX too, not
        // just Windows), so this is the one existing gap in coverage that
        // actually matters -- every other test here fakes NetworkClient's
        // state directly and never opens a real socket. A successful
        // localhost connect should complete in well under the new 5-second
        // connect deadline (it was previously bounded only by the OS's own
        // connect timeout on a plain blocking socket).
        {
            NetworkClient* nc = NetworkClient::Instance();
            // NetworkClient is a true singleton shared by every block in this
            // file; an earlier one fakes state (line ~1010) to IN_LOBBY and
            // never resets it, since nothing before this block cared. This
            // one calls the real Connect(), which refuses to run unless
            // state is DISCONNECTED, so force a clean starting point here.
            NetworkClientTestAccess::SetState(*nc, DISCONNECTED);
            CHECK(nc->GetState() == DISCONNECTED);

            // Connect() is kickoff-only as of the async networking handoff
            // (stage 2a-2c): it starts the attempt and returns, and the state
            // machine is advanced by Update() once per frame, exactly as
            // MainMenu::PumpNetworkFrame() does it. So "did it connect" is a
            // question you answer by pumping, not by reading the return value.
            // Frame-stall assertions for this live in netconnect-test, which
            // can script the awkward peers; here the point is just that a real
            // localhost server still ends up CONNECTED.
            Uint64 connectStart = SDL_GetTicks();
            bool started = nc->Connect("127.0.0.1", testPort);
            CHECK(started);
            while (nc->IsConnecting() && SDL_GetTicks() - connectStart < 5000) {
                nc->Update();
                SDL_Delay(5);
            }
            Uint64 connectMs = SDL_GetTicks() - connectStart;

            CHECK(nc->IsConnected());
            CHECK(nc->GetState() == CONNECTED);
            CHECK(connectMs < 3000);  // Well under the deadlines on localhost.
            if (!nc->IsConnected() || connectMs >= 3000)
                std::fprintf(stderr, "  (Connect to real localhost server: state=%d, %llu ms)\n",
                             nc->GetState(), (unsigned long long)connectMs);

            nc->Disconnect();
            CHECK(nc->GetState() == DISCONNECTED);
        }

        MainMenuTestAccess::CallStopLocalServer(*menu);
        CHECK(!MainMenuTestAccess::IsServerHosting(*menu));

        // --- A refused connection (nothing listening, same port just
        // stopped above) must fail fast via SO_ERROR after connect()
        // reports ECONNREFUSED, not sit out the full 5-second timeout --
        // that only bounds a genuinely unresponsive peer, not an actively
        // refused one.
        {
            NetworkClient* nc = NetworkClient::Instance();
            Uint64 refuseStart = SDL_GetTicks();
            // Kickoff still succeeds -- a refusal is discovered by the state
            // machine on a later frame, not by Connect() itself (see the
            // block above on the stage 2a-2c contract change).
            CHECK(nc->Connect("127.0.0.1", testPort));
            while (nc->IsConnecting() && SDL_GetTicks() - refuseStart < 5000) {
                nc->Update();
                SDL_Delay(5);
            }
            Uint64 refuseMs = SDL_GetTicks() - refuseStart;

            CHECK(!nc->IsConnected());
            CHECK(nc->GetState() == DISCONNECTED);
            CHECK(refuseMs < 2000);
            if (nc->IsConnected() || refuseMs >= 2000)
                std::fprintf(stderr, "  (Connect to refused port: state=%d, %llu ms)\n",
                             nc->GetState(), (unsigned long long)refuseMs);
        }

        // --- LAN discovery now runs on a background thread instead of
        // blocking the caller for its fixed ~1s broadcast window plus a
        // per-server MeasureLatency probe (async networking handoff, stage
        // 1d) -- exercised through ShowPanel/case 3, the SDLK_R refresh, and
        // the "Host a server" rescan, all of which now call StartLanFetch()
        // instead of DiscoverLANServers() inline. The kickoff call itself
        // must return near-instantly; the actual ~1s scan happens off-thread.
        {
            Uint64 kickoffStart = SDL_GetTicks();
            MainMenuTestAccess::CallStartLanFetch(*menu);
            Uint64 kickoffMs = SDL_GetTicks() - kickoffStart;

            CHECK(kickoffMs < 100);
            CHECK(MainMenuTestAccess::LanFetchInProgress(*menu));
            if (kickoffMs >= 100)
                std::fprintf(stderr, "  (StartLanFetch kickoff took %llu ms)\n",
                             (unsigned long long)kickoffMs);

            // PumpNetworkFrame() must never block waiting on this -- it just
            // drives NetworkClient::Update()/bot servicing, unrelated to the
            // fetch thread. Confirm it returns promptly while the fetch is
            // still running.
            Uint64 pumpStart = SDL_GetTicks();
            MainMenuTestAccess::CallPumpNetworkFrame(*menu);
            Uint64 pumpMs = SDL_GetTicks() - pumpStart;
            CHECK(pumpMs < 100);

            // Wait for the real scan to finish (its own ~1s window), well
            // under the fixed old synchronous cost of a full scan run twice.
            Uint64 waitStart = SDL_GetTicks();
            while (MainMenuTestAccess::LanFetchInProgress(*menu) &&
                   SDL_GetTicks() - waitStart < 2500) {
                SDL_Delay(20);
            }
            CHECK(!MainMenuTestAccess::LanFetchInProgress(*menu));
        }

        // --- NetworkClient::SendNick's NICK_IN_USE retry now runs
        // end-to-end through HandleServerResponse's async pending-flag path
        // on native too (async networking handoff, stage 1b) -- previously
        // a blocking 20-retry loop with its own inline SDL_Delay(50)s. A
        // second, raw (unmanaged) TCP connection claims the nickname first,
        // so the singleton NetworkClient's own SendNick() is forced into a
        // real collision against the real server, not a synthetic one.
        {
            const int nickTestPort = 15522;  // distinct from every other port used in this file
            MainMenuTestAccess::SetNetworkPort(*menu, nickTestPort);
            MainMenuTestAccess::CallStartLocalServer(*menu);
            CHECK(MainMenuTestAccess::IsServerHosting(*menu));

            int rawSock = socket(AF_INET, SOCK_STREAM, 0);
            CHECK(rawSock >= 0);
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(nickTestPort);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            CHECK(connect(rawSock, (struct sockaddr*)&addr, sizeof(addr)) == 0);
            const char* claimCmd = "FB/1.3 NICK claimed\n";
            CHECK(send(rawSock, claimCmd, strlen(claimCmd), 0) == (ssize_t)strlen(claimCmd));
            // Give the server a moment to process the claim before the real
            // client's NICK arrives -- TCP ordering across two independent
            // connections isn't guaranteed the way it is within one.
            SDL_Delay(100);

            NetworkClient* nc = NetworkClient::Instance();
            NetworkClientTestAccess::SetState(*nc, DISCONNECTED);
            // Kickoff plus pump -- Connect() no longer completes on its own
            // (async networking handoff, stage 2a-2c).
            CHECK(nc->Connect("127.0.0.1", nickTestPort));
            const Uint64 nickConnectStart = SDL_GetTicks();
            while (nc->IsConnecting() && SDL_GetTicks() - nickConnectStart < 5000) {
                nc->Update();
                SDL_Delay(5);
            }
            CHECK(nc->GetState() == CONNECTED);

            CHECK(nc->SendNick("claimed"));
            // No assertion on GetPlayerNick()/IsPendingNick() immediately
            // after this call. When this test was written, SendCommand() did
            // its own inline 100ms select()+recv(), which on a fast localhost
            // round trip reliably drove the whole NICK_IN_USE -> retry -> OK
            // sequence to completion recursively inside this very call, so
            // pendingNick could already be false before SendNick() returned.
            // Stage 1c has since deleted that inline read, so the sequence now
            // resolves over the polled frames below instead. The test asserted
            // neither timing then and asserts neither now -- that indifference
            // is exactly why it kept passing across the change; only the
            // eventual outcome matters.
            Uint64 waitStart = SDL_GetTicks();
            while (nc->IsPendingNick() && SDL_GetTicks() - waitStart < 2000) {
                nc->Update();
                SDL_Delay(10);
            }
            CHECK(!nc->IsPendingNick());
            // The collision must have actually been resolved by a retry, not
            // silently accepted or silently given up on.
            CHECK(nc->GetPlayerNick() == "claimed2");
            if (nc->IsPendingNick() || nc->GetPlayerNick() != "claimed2")
                std::fprintf(stderr, "  (SendNick NICK_IN_USE retry: pending=%d, final nick='%s')\n",
                             nc->IsPendingNick(), nc->GetPlayerNick().c_str());

            nc->Disconnect();
            close(rawSock);
            MainMenuTestAccess::CallStopLocalServer(*menu);
        }

        // --- Cancelling a connect must be reachable by tap, not only by ESC
        // (async networking handoff, stage 2e; CLAUDE.md's input-parity rule).
        // Connecting can now take seconds, so "get me out of this" has to work
        // for a phone player with no ESC key -- this repo has shipped several
        // bugs from exactly the gap where something was reachable one way
        // only, which is why the rule exists.
        {
            NetworkClient* nc = NetworkClient::Instance();
            NetworkClientTestAccess::SetState(*nc, DISCONNECTED);

            // Stand up the state the connecting indicator renders in, and the
            // rect it publishes, without needing a live half-open socket.
            const SDL_Rect cancelRect{420, 300, 200, 24};
            MainMenuTestAccess::SetCancelConnectRect(*menu, cancelRect);
            MainMenuTestAccess::SetPendingLobbyConnect(*menu, true);

            // A tap inside the rect goes through the real HandlePanelTap, the
            // same entry point a finger reaches.
            const float cancelMidX = cancelRect.x + cancelRect.w * 0.5f;
            const float cancelMidY = cancelRect.y + cancelRect.h * 0.5f;
            const bool consumed = menu->HandlePanelTap(cancelMidX, cancelMidY, 0.f);
            CHECK(consumed);  // must not fall through to the row underneath
            CHECK(!MainMenuTestAccess::PendingLobbyConnect(*menu));

            // And a tap outside it must not cancel anything -- otherwise the
            // rect would be swallowing taps meant for the server list.
            MainMenuTestAccess::SetPendingLobbyConnect(*menu, true);
            menu->HandlePanelTap((float)(cancelRect.x - 40), cancelMidY, 0.f);
            CHECK(MainMenuTestAccess::PendingLobbyConnect(*menu));

            // Zeroed rect (the indicator is not showing) must be inert, not a
            // hit at the origin.
            MainMenuTestAccess::SetCancelConnectRect(*menu, SDL_Rect{0, 0, 0, 0});
            MainMenuTestAccess::SetPendingLobbyConnect(*menu, true);
            menu->HandlePanelTap(0.f, 0.f, 0.f);
            CHECK(MainMenuTestAccess::PendingLobbyConnect(*menu));

            MainMenuTestAccess::SetPendingLobbyConnect(*menu, false);
        }
    }
#endif

    if (failures == 0) {
        std::printf("menu touch gesture tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
