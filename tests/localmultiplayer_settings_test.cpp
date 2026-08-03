#include <SDL3_image/SDL_image.h>

#include "localmultiplayer_settings.h"

#include <cstdio>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

int main() {
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

    if (failures == 0) {
        std::printf("local multiplayer settings tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
