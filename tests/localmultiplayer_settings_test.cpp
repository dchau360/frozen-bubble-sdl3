#include <SDL3_image/SDL_image.h>

#include "localmultiplayer_settings.h"
#include "mainmenu.h"
#include "platform.h"

#include <cstdio>
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
        return menu.panelText.Text();
    }

    static void SetVictories(MainMenu& menu, int victoriesIndex) {
        menu.showingLocalMPPanel = true;
        menu.runDelay = false;
        menu.localMPMenuIndex = 6;
        menu.localMPVictoriesIndex = victoriesIndex;
    }

    static int VictoriesIndex(const MainMenu& menu) {
        return menu.localMPVictoriesIndex;
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
        bool disableMalus,
        bool teamMode) {
        menu.localMPPlayerCount = 4;
        menu.localMPCR = chainReaction;
        menu.localMPNoCompress = noCompression;
        menu.localMPClearMode = clearMode;
        menu.localMPDisableMalus = disableMalus;
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
        4, false, true, false, true, false, 15, colors, aimGuide);
    CHECK(options.playerCount == 4);
    CHECK(!options.chainReaction);
    CHECK(options.noCompression);
    CHECK(!options.clearMode);
    CHECK(options.disableMalus);
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
    CHECK(settings.disableMalus);
    CHECK(!settings.teamMode);
    CHECK(settings.victoriesLimit == 30);
    CHECK(settings.playerColors[2] == 7);
    CHECK(settings.aimGuide[4]);

    std::unique_ptr<MainMenu> menu = MainMenuTestAccess::Create(renderer);

    std::string rendered = MainMenuTestAccess::RenderVictories(*menu, 0);
    CHECK(rendered.find("> Victories limit: none (unlimited)")
        != std::string::npos);
    rendered = MainMenuTestAccess::RenderVictories(*menu, 15);
    CHECK(rendered.find("> Victories limit: 30") != std::string::npos);

    MainMenuTestAccess::SetVictories(*menu, 0);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_LEFT));
    CHECK(MainMenuTestAccess::VictoriesIndex(*menu) == 17);

    MainMenuTestAccess::SetVictories(*menu, 17);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RIGHT));
    CHECK(MainMenuTestAccess::VictoriesIndex(*menu) == 0);

    MainMenuTestAccess::SetVictories(*menu, 17);
    CHECK(MainMenuTestAccess::PressKey(*menu, SDLK_RETURN));
    CHECK(MainMenuTestAccess::VictoriesIndex(*menu) == 0);

    for (int enabledField = 0; enabledField < 5; ++enabledField) {
        const bool chainReaction = enabledField == 0;
        const bool noCompression = enabledField == 1;
        const bool clearMode = enabledField == 2;
        const bool disableMalus = enabledField == 3;
        const bool teamMode = enabledField == 4;
        MainMenuTestAccess::ConfigureLocalGame(
            *menu, chainReaction, noCompression, clearMode, disableMalus,
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
        CHECK(started.disableMalus == disableMalus);
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
