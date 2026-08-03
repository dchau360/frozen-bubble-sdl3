#include <SDL3_image/SDL_image.h>

#include "localmultiplayer_settings.h"

#include <algorithm>

SetupSettings BuildLocalMultiplayerSettings(
    const LocalMultiplayerOptions& options) {
    SetupSettings settings;
    settings.playerCount = std::clamp(options.playerCount, 2, 4);
    settings.chainReaction = options.chainReaction;
    settings.randomLevels = true;
    settings.localMultiplayer = true;
    settings.clearMode = options.clearMode;
    settings.disableMalus = options.disableMalus;
    settings.teamMode = options.teamMode;
    settings.teamCount = 2;

    const int victoriesIndex = std::clamp(
        options.victoriesIndex, 0,
        static_cast<int>(kVictoriesLimits.size()) - 1);
    settings.victoriesLimit = kVictoriesLimits[victoriesIndex];

    for (int i = 0; i < 5; ++i) {
        settings.playerTeams[i] = options.teamMode ? (i % 2) + 1 : i + 1;
        settings.playerColors[i] = options.colors[i];
        settings.disableCompression[i] = options.noCompression;
        settings.aimGuide[i] = options.aimGuide[i];
    }
    return settings;
}
