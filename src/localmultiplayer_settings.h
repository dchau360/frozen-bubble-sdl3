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
    // Bots take the last slots, so player 1 is always a person. Capped at
    // playerCount - 1: a game of nothing but bots is a screensaver, not a game.
    int botCount = 0;
    int botSkill = 1;
};

// Row indices in the local multiplayer setup panel. Two files walk this list
// -- one draws it, one handles keys -- and the per-player rows sit at an
// offset that depends on the player count, so the layout lives here rather
// than as matching magic numbers in both.
inline constexpr int kLocalMPRowPlayers      = 0;
inline constexpr int kLocalMPRowChain        = 1;
inline constexpr int kLocalMPRowCollapse     = 2;
inline constexpr int kLocalMPRowMode         = 3;
inline constexpr int kLocalMPRowMalus        = 4;
inline constexpr int kLocalMPRowTeam         = 5;
inline constexpr int kLocalMPRowVictories    = 6;
inline constexpr int kLocalMPRowBots         = 7;
inline constexpr int kLocalMPRowBotSkill     = 8;
inline constexpr int kLocalMPFirstPlayerRow  = 9;

inline constexpr int LocalMPAimGuideRow(int playerIndex) {
    return kLocalMPFirstPlayerRow + playerIndex;
}
inline constexpr int LocalMPColorsRow(int playerIndex, int playerCount) {
    return kLocalMPFirstPlayerRow + playerCount + playerIndex;
}
inline constexpr int LocalMPStartRow(int playerCount) {
    return kLocalMPFirstPlayerRow + 2 * playerCount;
}

inline const char* LocalMPBotSkillName(int skill) {
    return skill <= 0 ? "easy" : skill == 1 ? "normal" : "hard";
}

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
    const bool aimGuide[5],
    int botCount = 0,
    int botSkill = 1);

// Bots fill the highest player slots. Returns how many the given player count
// can actually take, which is one fewer than the players in the game.
int ClampLocalBotCount(int botCount, int playerCount);

SetupSettings BuildLocalMultiplayerSettings(
    const LocalMultiplayerOptions& options);

#endif
