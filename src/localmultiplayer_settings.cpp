#include "localmultiplayer_settings.h"

#include <algorithm>
#include <cstdio>

bool ApplyLocalMultiplayerVictoriesInput(
    int menuIndex,
    LocalMultiplayerMenuCommand command,
    int& victoriesIndex) {
    if (menuIndex != kLocalMPRowVictories) return false;

    if (command == LocalMultiplayerMenuCommand::Left) {
        --victoriesIndex;
        if (victoriesIndex < 0) {
            victoriesIndex = static_cast<int>(kVictoriesLimits.size()) - 1;
        }
    } else {
        ++victoriesIndex;
        if (victoriesIndex >= static_cast<int>(kVictoriesLimits.size())) {
            victoriesIndex = 0;
        }
    }
    return true;
}

void LocalMPTeamSplitLabel(char* out, size_t outSize, int playerCount) {
    if (out == nullptr || outSize == 0) return;
    const int players =
        std::clamp(playerCount, kMinLocalPlayers, kMaxLocalPlayers);
    size_t pos = 0;
    for (int team = 1; team <= 2; ++team) {
        if (team == 2) {
            const int n = snprintf(out + pos, outSize - pos, " vs ");
            if (n < 0 || static_cast<size_t>(n) >= outSize - pos) return;
            pos += static_cast<size_t>(n);
        }
        bool first = true;
        for (int i = 0; i < players; ++i) {
            if (LocalMPTeamOf(i) != team) continue;
            const int n = snprintf(out + pos, outSize - pos, "%sP%d",
                                   first ? "" : "+", i + 1);
            if (n < 0 || static_cast<size_t>(n) >= outSize - pos) return;
            pos += static_cast<size_t>(n);
            first = false;
        }
    }
}

int ClampLocalBotCount(int botCount, int playerCount) {
    const int players = std::clamp(playerCount, kMinLocalPlayers, kMaxLocalPlayers);
    return std::clamp(botCount, 0, players - 1);
}

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
    int botCount,
    int botSkill) {
    LocalMultiplayerOptions options;
    options.playerCount = playerCount;
    options.chainReaction = chainReaction;
    options.noCompression = noCompression;
    options.clearMode = clearMode;
    options.disableMalus = disableMalus;
    options.teamMode = teamMode;
    options.victoriesIndex = victoriesIndex;
    for (int i = 0; i < 5; ++i) {
        options.colors[i] = colors[i];
        options.aimGuide[i] = aimGuide[i];
    }
    options.botCount = ClampLocalBotCount(botCount, playerCount);
    options.botSkill = std::clamp(botSkill, 0, 2);
    return options;
}

SetupSettings BuildLocalMultiplayerSettings(
    const LocalMultiplayerOptions& options) {
    SetupSettings settings;
    settings.playerCount =
        std::clamp(options.playerCount, kMinLocalPlayers, kMaxLocalPlayers);
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

    // Bots take the last slots so player 1 stays human whatever the count.
    const int bots = ClampLocalBotCount(options.botCount, settings.playerCount);
    settings.botSkill = std::clamp(options.botSkill, 0, 2);
    for (int i = 0; i < MAX_NET_PLAYERS; ++i) settings.playerIsBot[i] = false;
    for (int i = settings.playerCount - bots; i < settings.playerCount; ++i)
        settings.playerIsBot[i] = true;
    return settings;
}
