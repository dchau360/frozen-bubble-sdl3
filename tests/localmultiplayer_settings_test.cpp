#include <SDL3_image/SDL_image.h>

#include "localmultiplayer_settings.h"
#include "mainmenu.h"
#include "platform.h"
#include "mainmenu_internal.h"

#include <cstdio>
#include <set>
#include <memory>
#include <string>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct MainMenuTestAccess {
    static std::unique_ptr<MainMenu> Create(const SDL_Renderer* renderer) {
        return std::unique_ptr<MainMenu>(
            new MainMenu(renderer, MainMenu::HeadlessTestTag{}));
    }

    static std::string RenderVictories(MainMenu& menu, int victoriesIndex) {
        menu.showingLocalMPPanel = true;
        menu.runDelay = false;
        menu.localMPMenuIndex = 6;
        menu.localMPVictoriesIndex = victoriesIndex;
        menu.LocalMPPanelRender();
        return menu.lastLocalMPPanelText;
    }

    // Renders the panel with the Attack bubbles row selected and returns the
    // same row dump RenderVictories reads, so the three-state value can be
    // checked as it is actually drawn rather than as the enum alone.
    static std::string RenderAttackMode(MainMenu& menu, AttackMode mode) {
        menu.showingLocalMPPanel = true;
        menu.runDelay = false;
        menu.localMPMenuIndex = kLocalMPRowMalus;
        menu.localMPAttackMode = mode;
        menu.LocalMPPanelRender();
        return menu.lastLocalMPPanelText;
    }

    static void SelectAttackRow(MainMenu& menu, AttackMode mode) {
        menu.showingLocalMPPanel = true;
        menu.runDelay = false;
        menu.localMPMenuIndex = kLocalMPRowMalus;
        menu.localMPAttackMode = mode;
    }

    static AttackMode CurrentAttackMode(const MainMenu& menu) {
        return menu.localMPAttackMode;
    }

    static void SetClearMode(MainMenu& menu, bool on) {
        menu.showingLocalMPPanel = true;
        menu.runDelay = false;
        menu.localMPMenuIndex = kLocalMPRowMode;
        menu.localMPClearMode = !on;   // PressKey toggles it to `on`
    }

    static bool ClearMode(const MainMenu& menu) { return menu.localMPClearMode; }

    // ---- settings guide ----------------------------------------------
    // The page is only reachable through a live game room, so drive it
    // directly: it has to render, scroll within its bounds, and close.
    static void SetHelpTopic(MainMenu& menu, HelpTopic topic) {
        menu.helpTopic = (int)topic;
    }
    static int HelpTopicOf(const MainMenu& menu) { return menu.helpTopic; }
    static int& LocalSelection(MainMenu& menu) { return menu.localMPMenuIndex; }
    static void OpenHelp(MainMenu& menu) {
        menu.showingHelpPanel = true;
        menu.helpScroll = 0;
        menu.helpMenuIndex = kHelpRowClose;
    }
    static void RenderHelp(MainMenu& menu) { menu.HelpPanelRender(); }
    // PressKey above goes straight to LocalMPPanelKey; the guide has its own
    // handler, called earlier than that one from HandleInput.
    static bool PressHelpKey(MainMenu& menu, SDL_Keycode key) {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = key;
        return menu.HelpPanelKey(&event);
    }
    static bool HelpOpen(const MainMenu& menu) { return menu.showingHelpPanel; }
    static int HelpScroll(const MainMenu& menu) { return menu.helpScroll; }
    static int& RoomSelection(MainMenu& menu) { return menu.selectedActionIndex; }

    static void SetVictories(MainMenu& menu, int victoriesIndex) {
        menu.showingLocalMPPanel = true;
        menu.runDelay = false;
        menu.localMPMenuIndex = 6;
        menu.localMPVictoriesIndex = victoriesIndex;
    }

    static int VictoriesIndex(const MainMenu& menu) {
        return menu.localMPVictoriesIndex;
    }

    // ---- panel tap rows ----------------------------------------------
    // Drives HandlePanelTap against a layout published by hand, so the
    // select-then-activate rule can be checked without a live panel -- notably
    // the game room's two-axis grid, which needs a connected server to reach.
    static void BeginRows(MainMenu& menu, int* sel, int* sub = nullptr) {
        menu.BeginPanelTapRows(sel, sub);
    }
    static void AddRow(MainMenu& menu, int index, SDL_Rect rect, int sub = -1,
                       bool splitAdjust = false, SDL_Keycode activateKey = 0) {
        menu.AddPanelTapRow(index, rect, sub, splitAdjust, activateKey);
    }
    static bool Tap(MainMenu& menu, float x, float y) {
        return menu.HandlePanelTap(x, y);
    }
    static size_t RowCount(const MainMenu& menu) {
        return menu.panelTapRows.size();
    }

    static bool PressKey(MainMenu& menu, SDL_Keycode key) {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = key;
        return menu.LocalMPPanelKey(&event);
    }

    static void ConfigureLocalGame(
        MainMenu& menu,
        bool chainReaction,
        bool noCompression,
        bool clearMode,
        AttackMode attackMode,
        bool teamMode) {
        menu.localMPPlayerCount = 4;
        menu.localMPCR = chainReaction;
        menu.localMPNoCompress = noCompression;
        menu.localMPClearMode = clearMode;
        menu.localMPAttackMode = attackMode;
        menu.localMPTeamMode = teamMode;
        menu.localMPVictoriesIndex = 15;
        const int colors[5] = {4, 5, 6, 7, 9};
        const bool aimGuide[5] = {true, false, true, false, true};
        for (int i = 0; i < 5; ++i) {
            menu.playerColorCounts[i] = colors[i];
            menu.localMPAimGuide[i] = aimGuide[i];
        }
    }

    static SetupSettings StartLocalGame(MainMenu& menu, bool& captured) {
        SetupSettings settings;
        captured = false;
        menu.testLocalGameStart = [&](const SetupSettings& started) {
            settings = started;
            captured = true;
        };
        menu.SetupNewGame(7);
        menu.testLocalGameStart = {};
        return settings;
    }
};

int main() {
    SDL_SetEnvironmentVariable(
        SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow(
        "localmultiplayer-settings-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        if (window != nullptr) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // The active victories row owns index 6 and wraps left from unlimited.
    int victoriesIndex = 0;
    CHECK(ApplyLocalMultiplayerVictoriesInput(
        6, LocalMultiplayerMenuCommand::Left, victoriesIndex));
    CHECK(victoriesIndex == 17);

    // Both forward commands wrap the last finite limit back to unlimited.
    victoriesIndex = 17;
    CHECK(ApplyLocalMultiplayerVictoriesInput(
        6, LocalMultiplayerMenuCommand::Right, victoriesIndex));
    CHECK(victoriesIndex == 0);
    victoriesIndex = 17;
    CHECK(ApplyLocalMultiplayerVictoriesInput(
        6, LocalMultiplayerMenuCommand::Enter, victoriesIndex));
    CHECK(victoriesIndex == 0);

    // Adjacent active rows do not mutate the victories selection.
    victoriesIndex = 4;
    CHECK(!ApplyLocalMultiplayerVictoriesInput(
        5, LocalMultiplayerMenuCommand::Right, victoriesIndex));
    CHECK(victoriesIndex == 4);
    CHECK(!ApplyLocalMultiplayerVictoriesInput(
        7, LocalMultiplayerMenuCommand::Left, victoriesIndex));
    CHECK(victoriesIndex == 4);

    // This is the pure production path used by SetupNewGame(7). Distinct field
    // values make omissions and swaps observable without constructing MainMenu.
    const int colors[5] = {5, 6, 7, 8, 5};
    const bool aimGuide[5] = {true, false, true, false, true};
    LocalMultiplayerOptions options = BuildLocalMultiplayerOptions(
        4, false, true, false, AttackMode::Off, false, 15, colors, aimGuide);
    CHECK(options.playerCount == 4);
    CHECK(!options.chainReaction);
    CHECK(options.noCompression);
    CHECK(!options.clearMode);
    CHECK(options.attackMode == AttackMode::Off);
    CHECK(!options.teamMode);
    CHECK(options.victoriesIndex == 15);
    CHECK(options.colors[0] == 5 && options.colors[1] == 6
        && options.colors[2] == 7 && options.colors[3] == 8
        && options.colors[4] == 5);
    CHECK(options.aimGuide[0] && !options.aimGuide[1]
        && options.aimGuide[2] && !options.aimGuide[3]
        && options.aimGuide[4]);

    SetupSettings settings = BuildLocalMultiplayerSettings(options);
    CHECK(settings.localMultiplayer);
    CHECK(settings.playerCount == 4);
    CHECK(!settings.chainReaction);
    CHECK(settings.disableCompression[3]);
    CHECK(!settings.clearMode);
    CHECK(settings.attackMode == AttackMode::Off);
    CHECK(!settings.teamMode);
    CHECK(settings.victoriesLimit == 30);
    CHECK(settings.playerColors[2] == 7);
    CHECK(settings.aimGuide[4]);

    std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

    // localMPMenuIndex is fixed at 6 (kLocalMPRowVictories) by the helper
    // above. lastLocalMPPanelText is a plain "Label: Value" dump of every
    // row menulist::List was fed that frame (see LocalMPPanelRender's row()
    // lambda) -- selection itself drives which row menulist highlights on
    // screen, not this text, so the check is just that the row's content is
    // what SetVictories() asked for.
    std::string rendered = MainMenuTestAccess::RenderVictories(*menu, 0);
    CHECK(rendered.find("Victories limit: none (unlimited)") != std::string::npos);
    rendered = MainMenuTestAccess::RenderVictories(*menu, 15);
    CHECK(rendered.find("Victories limit: 30") != std::string::npos);

    // Attack bubbles is three-state now, so the row has to render all three
    // and Left/Right have to walk the cycle in opposite directions.
    rendered = MainMenuTestAccess::RenderAttackMode(*menu, AttackMode::On);
    CHECK(rendered.find("Attack bubbles: ON") != std::string::npos);
    rendered = MainMenuTestAccess::RenderAttackMode(*menu, AttackMode::Off);
    CHECK(rendered.find("Attack bubbles: OFF") != std::string::npos);
    rendered = MainMenuTestAccess::RenderAttackMode(*menu, AttackMode::Canceling);
    CHECK(rendered.find("Attack bubbles: Blockable") != std::string::npos);

    MainMenuTestAccess::SelectAttackRow(*menu, AttackMode::On);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RIGHT));
    CHECK(MainMenuTestAccess::CurrentAttackMode(*menu) == AttackMode::Canceling);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RIGHT));
    CHECK(MainMenuTestAccess::CurrentAttackMode(*menu) == AttackMode::Off);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RIGHT));
    CHECK(MainMenuTestAccess::CurrentAttackMode(*menu) == AttackMode::On);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_LEFT));
    CHECK(MainMenuTestAccess::CurrentAttackMode(*menu) == AttackMode::Off);

    // Clear Mode forces attacks off and restores the previous choice on the
    // way back out -- including Blockable, which the old boolean could not hold.
    MainMenuTestAccess::SelectAttackRow(*menu, AttackMode::Canceling);
    MainMenuTestAccess::SetClearMode(*menu, true);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RIGHT));
    CHECK(MainMenuTestAccess::ClearMode(*menu));
    CHECK(MainMenuTestAccess::CurrentAttackMode(*menu) == AttackMode::Off);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RIGHT));   // leave Clear Mode
    CHECK(!MainMenuTestAccess::ClearMode(*menu));
    CHECK(MainMenuTestAccess::CurrentAttackMode(*menu) == AttackMode::Canceling);

    // ---- settings guide ------------------------------------------------
    {
        MainMenuTestAccess::OpenHelp(*menu);
        MainMenuTestAccess::RenderHelp(*menu);   // must not crash on a headless renderer
        CHECK(MainMenuTestAccess::HelpOpen(*menu));

        // Up at the top stays at the top rather than scrolling to a negative
        // line, which would index off the front of the page.
        CHECK(MainMenuTestAccess::PressHelpKey(*menu, SDLK_UP));
        CHECK(MainMenuTestAccess::HelpScroll(*menu) == 0);

        CHECK(MainMenuTestAccess::PressHelpKey(*menu, SDLK_DOWN));
        CHECK(MainMenuTestAccess::HelpScroll(*menu) == 1);
        CHECK(MainMenuTestAccess::PressHelpKey(*menu, SDLK_PAGEDOWN));
        CHECK(MainMenuTestAccess::HelpScroll(*menu) == 13);
        MainMenuTestAccess::RenderHelp(*menu);   // clamps to the page length
        CHECK(MainMenuTestAccess::HelpScroll(*menu) >= 0);
        CHECK(MainMenuTestAccess::PressHelpKey(*menu, SDLK_HOME));
        CHECK(MainMenuTestAccess::HelpScroll(*menu) == 0);

        // Paging far past the end clamps instead of running off it.
        for (int i = 0; i < 40; ++i) MainMenuTestAccess::PressHelpKey(*menu, SDLK_PAGEDOWN);
        MainMenuTestAccess::RenderHelp(*menu);
        CHECK(MainMenuTestAccess::HelpScroll(*menu) > 0);

        // Left/Right are swallowed: the room underneath must not step a
        // setting the player cannot see.
        const int before = MainMenuTestAccess::HelpScroll(*menu);
        CHECK(MainMenuTestAccess::PressHelpKey(*menu, SDLK_LEFT));
        CHECK(MainMenuTestAccess::PressHelpKey(*menu, SDLK_RIGHT));
        CHECK(MainMenuTestAccess::HelpScroll(*menu) == before);

        // Closing releases both HELP boxes' fake indices -- neither is a real
        // row, so stepping from one would walk off the end of its list.
        MainMenuTestAccess::RoomSelection(*menu) = kRoomHelpTapIndex;
        MainMenuTestAccess::LocalSelection(*menu) = kLocalMPHelpTapIndex;
        CHECK(MainMenuTestAccess::PressHelpKey(*menu, SDLK_ESCAPE));
        CHECK(!MainMenuTestAccess::HelpOpen(*menu));
        CHECK(MainMenuTestAccess::RoomSelection(*menu) == kRoomMalus);
        CHECK(MainMenuTestAccess::LocalSelection(*menu) == kLocalMPRowBotSkill);

        // The local page is a different page, not the online one rescrolled:
        // it has its own length, so a scroll position valid for one is not
        // automatically valid for the other.
        MainMenuTestAccess::OpenHelp(*menu);
        MainMenuTestAccess::SetHelpTopic(*menu, HelpTopic::LocalMultiplayer);
        CHECK(MainMenuTestAccess::HelpTopicOf(*menu) == (int)HelpTopic::LocalMultiplayer);
        MainMenuTestAccess::RenderHelp(*menu);
        for (int i = 0; i < 40; ++i) MainMenuTestAccess::PressHelpKey(*menu, SDLK_PAGEDOWN);
        MainMenuTestAccess::RenderHelp(*menu);          // clamps to the local page
        const int localMax = MainMenuTestAccess::HelpScroll(*menu);
        CHECK(localMax > 0);

        MainMenuTestAccess::SetHelpTopic(*menu, HelpTopic::OnlineRoom);
        for (int i = 0; i < 40; ++i) MainMenuTestAccess::PressHelpKey(*menu, SDLK_PAGEDOWN);
        MainMenuTestAccess::RenderHelp(*menu);
        CHECK(MainMenuTestAccess::HelpScroll(*menu) > 0);

        MainMenuTestAccess::PressHelpKey(*menu, SDLK_ESCAPE);
        CHECK(!MainMenuTestAccess::HelpOpen(*menu));
    }

    // The local panel's Up/Down must fold the HELP box's fake index back onto
    // a real row rather than stepping away from it into nothing.
    {
        MainMenuTestAccess::SelectAttackRow(*menu, AttackMode::On);
        MainMenuTestAccess::LocalSelection(*menu) = kLocalMPHelpTapIndex;
        CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_UP));
        CHECK(MainMenuTestAccess::LocalSelection(*menu) == kLocalMPRowBotSkill - 1);
        MainMenuTestAccess::LocalSelection(*menu) = kLocalMPHelpTapIndex;
        CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_DOWN));
        CHECK(MainMenuTestAccess::LocalSelection(*menu) == kLocalMPRowBotSkill + 1);
    }

    // The Bot skill row still reports its value with the HELP box beside it.
    {
        std::string withHelp = MainMenuTestAccess::RenderAttackMode(*menu, AttackMode::On);
        CHECK(withHelp.find("Bot skill: Med") != std::string::npos);
    }

    MainMenuTestAccess::SetVictories(*menu, 0);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_LEFT));
    CHECK(MainMenuTestAccess::VictoriesIndex(*menu) == 17);

    MainMenuTestAccess::SetVictories(*menu, 17);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RIGHT));
    CHECK(MainMenuTestAccess::VictoriesIndex(*menu) == 0);

    MainMenuTestAccess::SetVictories(*menu, 17);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RETURN));
    CHECK(MainMenuTestAccess::VictoriesIndex(*menu) == 0);

    // ---- panel row tapping ------------------------------------------------
    {
        int selection = 0;
        MainMenuTestAccess::BeginRows(*menu, &selection);
        MainMenuTestAccess::AddRow(*menu, 0, {100, 100, 200, 20});
        MainMenuTestAccess::AddRow(*menu, 1, {100, 120, 200, 20});
        // A row the panel chose not to draw registers nothing, so a tap where it
        // would have been falls through instead of selecting an invisible row.
        MainMenuTestAccess::AddRow(*menu, 2, {100, 140, 0, 0});
        CHECK(MainMenuTestAccess::RowCount(*menu) == 2);

        // A tap outside every row IS consumed, now that this panel has
        // registered rows at all. This assertion used to be the other way
        // round -- a miss fell through so the caller could apply its
        // tap-anywhere-to-confirm fallback -- and that fallback is what the
        // caller implements by injecting RETURN, which activates whatever
        // row is currently SELECTED rather than doing nothing.
        //
        // On a panel whose rows are small that turns a near-miss into an
        // unintended activation of the selected row, with no way out:
        // reported live on the network game room's 18-logical-unit bot rows,
        // where with "Bots" selected essentially every tap aimed elsewhere
        // missed, fell through, and cycled the bot count again instead of
        // moving the selection. See MainMenu::HandlePanelTap.
        //
        // A miss must still never move the selection either way.
        CHECK(MainMenuTestAccess::Tap(*menu, 50.f, 50.f));
        CHECK(MainMenuTestAccess::Tap(*menu, 150.f, 145.f));
        CHECK(selection == 0);

        // First tap on an unselected row only moves the selection.
        CHECK(MainMenuTestAccess::Tap(*menu, 150.f, 125.f));
        CHECK(selection == 1);
        // Second tap on the same row activates: it is consumed, and the Return
        // it pushes is what every panel's own handler acts on.
        SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
        CHECK(MainMenuTestAccess::Tap(*menu, 150.f, 125.f));
        CHECK(selection == 1);
        SDL_Event pushed{};
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 1);
        CHECK(pushed.key.key == SDLK_RETURN);

        // Two-axis rows (the game room's per-player grid): moving along a row
        // to a different column is still a selection, not an activation, so a
        // cell is never changed by the tap that first reaches it.
        int gridRow = 8, gridCol = 0;
        MainMenuTestAccess::BeginRows(*menu, &gridRow, &gridCol);
        MainMenuTestAccess::AddRow(*menu, 8, {100, 200, 40, 16}, 0);
        MainMenuTestAccess::AddRow(*menu, 8, {140, 200, 40, 16}, 1);
        MainMenuTestAccess::AddRow(*menu, 9, {100, 216, 40, 16}, 0);

        SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
        CHECK(MainMenuTestAccess::Tap(*menu, 150.f, 205.f));  // same row, col 1
        CHECK(gridRow == 8);
        CHECK(gridCol == 1);
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 0);

        // Now that the cell is selected, tapping it again activates it.
        CHECK(MainMenuTestAccess::Tap(*menu, 150.f, 205.f));
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 1);
        CHECK(pushed.key.key == SDLK_RETURN);

        // Stepped rows (game speed) have no Return behaviour to borrow -- the
        // panel's Return handler does nothing for them -- so the half of the
        // row that was tapped picks a direction instead. Without this the row
        // cannot be changed at all on a device with no keyboard.
        int stepRow = 0;
        MainMenuTestAccess::BeginRows(*menu, &stepRow);
        MainMenuTestAccess::AddRow(*menu, 0, {100, 300, 200, 20}, -1, true);
        MainMenuTestAccess::AddRow(*menu, 1, {100, 320, 200, 20});

        // Selecting it first still takes a tap, as for any other row.
        SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
        CHECK(MainMenuTestAccess::Tap(*menu, 150.f, 325.f));
        CHECK(stepRow == 1);
        CHECK(MainMenuTestAccess::Tap(*menu, 150.f, 305.f));
        CHECK(stepRow == 0);
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 0);

        // Left half steps down.
        CHECK(MainMenuTestAccess::Tap(*menu, 120.f, 305.f));
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 1);
        CHECK(pushed.key.key == SDLK_LEFT);

        // Right half steps up.
        CHECK(MainMenuTestAccess::Tap(*menu, 280.f, 305.f));
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 1);
        CHECK(pushed.key.key == SDLK_RIGHT);

        // The neighbouring ordinary row is unaffected: it still activates with
        // Return wherever it is touched.
        CHECK(MainMenuTestAccess::Tap(*menu, 120.f, 325.f));
        CHECK(stepRow == 1);
        CHECK(MainMenuTestAccess::Tap(*menu, 120.f, 325.f));
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 1);
        CHECK(pushed.key.key == SDLK_RETURN);

        // A server row carries two targets: the narrow follow star at its left
        // edge, and the rest of the row which connects. The star is registered
        // first so the narrower band wins the hit test -- registered the other
        // way round, the full-width row would swallow every tap and the star
        // would be unreachable by touch.
        int serverRow = 0;
        MainMenuTestAccess::BeginRows(*menu, &serverRow);
        MainMenuTestAccess::AddRow(*menu, 1, {100, 400, 40, 20}, -1, false, SDLK_F);
        MainMenuTestAccess::AddRow(*menu, 1, {100, 400, 400, 20});
        MainMenuTestAccess::AddRow(*menu, 2, {100, 420, 400, 20});

        // First tap still only selects, star or not.
        SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
        CHECK(MainMenuTestAccess::Tap(*menu, 110.f, 405.f));
        CHECK(serverRow == 1);
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 0);

        // Second tap on the star toggles following rather than connecting.
        CHECK(MainMenuTestAccess::Tap(*menu, 110.f, 405.f));
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 1);
        CHECK(pushed.key.key == SDLK_F);

        // The same row tapped past the star still connects.
        CHECK(MainMenuTestAccess::Tap(*menu, 300.f, 405.f));
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 1);
        CHECK(pushed.key.key == SDLK_RETURN);

        // A row with no star is unaffected.
        CHECK(MainMenuTestAccess::Tap(*menu, 110.f, 425.f));
        CHECK(serverRow == 2);
        CHECK(MainMenuTestAccess::Tap(*menu, 110.f, 425.f));
        CHECK(SDL_PeepEvents(&pushed, 1, SDL_GETEVENT,
                             SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN) == 1);
        CHECK(pushed.key.key == SDLK_RETURN);

        // Beginning a new panel's rows drops the previous panel's, so a tap
        // cannot land on a row that is no longer on screen.
        MainMenuTestAccess::BeginRows(*menu, nullptr);
        CHECK(MainMenuTestAccess::RowCount(*menu) == 0);
        CHECK(!MainMenuTestAccess::Tap(*menu, 150.f, 205.f));
        SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
    }

    for (int enabledField = 0; enabledField < 5; ++enabledField) {
        const bool chainReaction = enabledField == 0;
        const bool noCompression = enabledField == 1;
        const bool clearMode = enabledField == 2;
        const AttackMode attackMode =
            enabledField == 3 ? AttackMode::Off : AttackMode::On;
        const bool teamMode = enabledField == 4;
        MainMenuTestAccess::ConfigureLocalGame(
            *menu, chainReaction, noCompression, clearMode, attackMode,
            teamMode);

        bool captured = false;
        SetupSettings started =
            MainMenuTestAccess::StartLocalGame(*menu, captured);
        CHECK(captured);
        CHECK(started.localMultiplayer);
        CHECK(started.playerCount == 4);
        CHECK(started.chainReaction == chainReaction);
        CHECK(started.disableCompression[0] == noCompression);
        CHECK(started.disableCompression[4] == noCompression);
        CHECK(started.clearMode == clearMode);
        CHECK(started.attackMode == attackMode);
        CHECK(started.teamMode == teamMode);
        CHECK(started.victoriesLimit == 30);
        CHECK(started.playerColors[0] == 4);
        CHECK(started.playerColors[1] == 5);
        CHECK(started.playerColors[2] == 6);
        CHECK(started.playerColors[3] == 7);
        CHECK(started.playerColors[4] == 9);
        CHECK(started.aimGuide[0]);
        CHECK(!started.aimGuide[1]);
        CHECK(started.aimGuide[2]);
        CHECK(!started.aimGuide[3]);
        CHECK(started.aimGuide[4]);
    }

    // --- Bots -------------------------------------------------------------
    // Bots take the highest slots so player 1 is always a person, and a game
    // of nothing but bots is not a game.
    {
        CHECK(ClampLocalBotCount(0, 4) == 0);
        CHECK(ClampLocalBotCount(3, 4) == 3);
        CHECK(ClampLocalBotCount(4, 4) == 3);   // one seat always stays human
        CHECK(ClampLocalBotCount(9, 2) == 1);
        CHECK(ClampLocalBotCount(-2, 3) == 0);
        // A five-player game can hold four bots, one short of every seat.
        CHECK(ClampLocalBotCount(4, 5) == 4);
        CHECK(ClampLocalBotCount(5, 5) == 4);
        // Past the seat cap the clamp still answers for the largest game
        // that exists, rather than scaling with a count that cannot happen.
        CHECK(ClampLocalBotCount(9, 99) == kMaxLocalPlayers - 1);

        LocalMultiplayerOptions options;
        options.playerCount = 4;
        options.botCount = 2;
        options.botSkill = 2;
        const SetupSettings settings = BuildLocalMultiplayerSettings(options);
        CHECK(!settings.playerIsBot[0]);
        CHECK(!settings.playerIsBot[1]);
        CHECK(settings.playerIsBot[2]);
        CHECK(settings.playerIsBot[3]);
        CHECK(!settings.playerIsBot[4]);
        CHECK(settings.botSkill == 2);

        // An over-large count is trimmed rather than taken literally, so the
        // human slot survives.
        options.botCount = 4;
        const SetupSettings crowded = BuildLocalMultiplayerSettings(options);
        CHECK(!crowded.playerIsBot[0]);
        CHECK(crowded.playerIsBot[1]);
        CHECK(crowded.playerIsBot[2]);
        CHECK(crowded.playerIsBot[3]);

        // No bots means no bot flags anywhere.
        options.botCount = 0;
        const SetupSettings solo = BuildLocalMultiplayerSettings(options);
        for (int i = 0; i < MAX_NET_PLAYERS; ++i) CHECK(!solo.playerIsBot[i]);

        // Skill is clamped to the three the AI actually implements.
        options.botSkill = 47;
        CHECK(BuildLocalMultiplayerSettings(options).botSkill == 2);
        options.botSkill = -3;
        CHECK(BuildLocalMultiplayerSettings(options).botSkill == 0);
    }

    // --- AdjustLocalBotCount ------------------------------------------------
    // Stepping Bots past the current cap grows Players to fit instead of
    // refusing (or wrapping) -- previously the only way to get a 2nd bot was
    // to raise Players to 3 by hand first, a 3rd to raise it to 4, and so on.
    {
        // Growing from the minimum, one bot at a time, each one past the
        // previous cap: every step raises playerCount right along with it.
        int bots = 0, players = kMinLocalPlayers;
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Right, bots, players);
        CHECK(bots == 1); CHECK(players == 2);   // room already existed
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Right, bots, players);
        CHECK(bots == 2); CHECK(players == 3);   // grew to fit
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Right, bots, players);
        CHECK(bots == 3); CHECK(players == 4);
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Right, bots, players);
        CHECK(bots == 4); CHECK(players == kMaxLocalPlayers);
        // Enter behaves exactly like Right (matches every other row's
        // Enter-cycles-forward convention). At the true ceiling -- five
        // players, four bots -- it wraps back to zero bots without touching
        // Players, same as it always has.
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Enter, bots, players);
        CHECK(bots == 0); CHECK(players == kMaxLocalPlayers);

        // Shrinking back down, one at a time: since every one of those seats
        // existed only to fit a bot, each step also lowers Players, landing
        // back at the minimum with zero bots -- "0 bots still shows 2
        // players" holds no matter how you got there.
        bots = 4; players = kMaxLocalPlayers;
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Left, bots, players);
        CHECK(bots == 3); CHECK(players == 4);
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Left, bots, players);
        CHECK(bots == 2); CHECK(players == 3);
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Left, bots, players);
        CHECK(bots == 1); CHECK(players == 2);
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Left, bots, players);
        CHECK(bots == 0); CHECK(players == kMinLocalPlayers);   // floor, not below
        // From zero, Left wraps up to the current cap without touching
        // Players -- unchanged from before this feature existed.
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Left, bots, players);
        CHECK(bots == 1); CHECK(players == kMinLocalPlayers);

        // A spare human seat (Players raised on its own, beyond what the
        // bots need) is not swallowed by shrinking bots back down: Right
        // only grows Players when there is no room left, so adding a bot
        // where a human-only seat already exists just fills that seat.
        bots = 0; players = 4;
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Right, bots, players);
        CHECK(bots == 1); CHECK(players == 4);   // fit in the spare seat
        // Removing it again leaves that deliberate player count alone too,
        // since bots never occupied every non-P1 seat.
        AdjustLocalBotCount(LocalMultiplayerMenuCommand::Left, bots, players);
        CHECK(bots == 0); CHECK(players == 4);
    }

    // --- Five seats -------------------------------------------------------
    // Five is the largest local game, and the layout it lands on (NewGame's
    // case 5) is the same one a five-player network room uses.
    {
        LocalMultiplayerOptions options;
        options.playerCount = 5;
        const SetupSettings settings = BuildLocalMultiplayerSettings(options);
        CHECK(settings.playerCount == 5);
        CHECK(settings.localMultiplayer);

        // Anything past the cap is trimmed to it rather than reaching a
        // layout that was never authored.
        options.playerCount = 6;
        CHECK(BuildLocalMultiplayerSettings(options).playerCount == kMaxLocalPlayers);
        options.playerCount = 1;
        CHECK(BuildLocalMultiplayerSettings(options).playerCount == kMinLocalPlayers);

        // Team Mode splits odd slots against even, so the fifth player joins
        // player 1's side -- and the label has to say so, or it would promise
        // a pairing the game does not play.
        options.playerCount = 5;
        options.teamMode = true;
        const SetupSettings teams = BuildLocalMultiplayerSettings(options);
        CHECK(teams.playerTeams[0] == 1 && teams.playerTeams[2] == 1 &&
              teams.playerTeams[4] == 1);
        CHECK(teams.playerTeams[1] == 2 && teams.playerTeams[3] == 2);
        for (int i = 0; i < 5; ++i)
            CHECK(teams.playerTeams[i] == LocalMPTeamOf(i));

        char label[48];
        LocalMPTeamSplitLabel(label, sizeof(label), 5);
        CHECK(std::string(label) == "P1+P3+P5 vs P2+P4");
        LocalMPTeamSplitLabel(label, sizeof(label), 4);
        CHECK(std::string(label) == "P1+P3 vs P2+P4");
        LocalMPTeamSplitLabel(label, sizeof(label), 2);
        CHECK(std::string(label) == "P1 vs P2");
        // A buffer too small must truncate, never run past its end.
        char tiny[6];
        LocalMPTeamSplitLabel(tiny, sizeof(tiny), 5);
        CHECK(std::string(tiny).size() < sizeof(tiny));
    }

    // The panel and the key handler both index rows through these helpers, so
    // the two can only disagree if the helpers themselves are wrong.
    {
        CHECK(LocalMPAimGuideRow(0) == kLocalMPFirstPlayerRow);
        CHECK(LocalMPColorsRow(0, 4) == kLocalMPFirstPlayerRow + 4);
        CHECK(LocalMPStartRow(4) == kLocalMPFirstPlayerRow + 8);
        // The rows a fifth player adds land past the fourth player's, and
        // Start moves down with them.
        CHECK(LocalMPColorsRow(0, 5) == kLocalMPFirstPlayerRow + 5);
        CHECK(LocalMPAimGuideRow(4) < LocalMPColorsRow(0, 5));
        CHECK(LocalMPColorsRow(4, 5) < LocalMPStartRow(5));
        CHECK(LocalMPStartRow(5) == kLocalMPFirstPlayerRow + 10);
        // Every row index in a 4-player panel is distinct and contiguous.
        std::set<int> rows;
        for (int i = 0; i <= LocalMPStartRow(4); ++i) rows.insert(i);
        CHECK((int)rows.size() == LocalMPStartRow(4) + 1);
        CHECK(kLocalMPRowBots < kLocalMPRowBotSkill);
        CHECK(kLocalMPRowBotSkill < kLocalMPFirstPlayerRow);
        // Victories input still answers only for its own row.
        int victories = 3;
        CHECK(!ApplyLocalMultiplayerVictoriesInput(
            kLocalMPRowBots, LocalMultiplayerMenuCommand::Right, victories));
        CHECK(victories == 3);
        CHECK(ApplyLocalMultiplayerVictoriesInput(
            kLocalMPRowVictories, LocalMultiplayerMenuCommand::Right, victories));
        CHECK(victories == 4);
    }

    menu.reset();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    if (failures == 0) {
        std::printf("local multiplayer settings tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
