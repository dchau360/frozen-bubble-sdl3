#ifndef LOCALMULTIPLAYER_SETTINGS_H
#define LOCALMULTIPLAYER_SETTINGS_H

#include "bubblegame.h"

#include <array>

inline constexpr std::array<int, 18> kVictoriesLimits = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 20, 30, 50, 100
};

struct LocalMultiplayerOptions {
    int playerCount = 2;
    bool chainReaction = true;
    bool noCompression = false;
    bool clearMode = false;
    bool disableMalus = false;
    bool teamMode = false;
    int victoriesIndex = 5;
    std::array<int, 5> colors = {8, 8, 8, 8, 8};
    std::array<bool, 5> aimGuide = {};
};

enum class LocalMultiplayerMenuCommand {
    Left,
    Right,
    Enter,
};

bool ApplyLocalMultiplayerVictoriesInput(
    int menuIndex,
    LocalMultiplayerMenuCommand command,
    int& victoriesIndex);

LocalMultiplayerOptions BuildLocalMultiplayerOptions(
    int playerCount,
    bool chainReaction,
    bool noCompression,
    bool clearMode,
    bool disableMalus,
    bool teamMode,
    int victoriesIndex,
    const int colors[5],
    const bool aimGuide[5]);

SetupSettings BuildLocalMultiplayerSettings(
    const LocalMultiplayerOptions& options);

#endif
