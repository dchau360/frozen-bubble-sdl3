#ifndef LOCALMULTIPLAYER_SETTINGS_H
#define LOCALMULTIPLAYER_SETTINGS_H

#include "bubblegame.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

inline constexpr std::array<int, 18> kVictoriesLimits = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 20, 30, 50, 100
};

// Seats a local game can hold. Five is the engine's own ceiling, not an
// arbitrary one: NewGame's case 5 is the last hand-authored layout (one full
// board centre, four minis in the corners), and everything a local seat needs
// is sized to match -- player1Keys..player5Keys, controllerInputs[5], and
// CTRL_SC_PLAYERS. Above this a game has to be a network room, which pages
// opponent boards instead of showing them all.
inline constexpr int kMinLocalPlayers = 2;
inline constexpr int kMaxLocalPlayers = 5;

struct LocalMultiplayerOptions {
    int playerCount = 2;
    bool chainReaction = true;
    bool noCompression = false;
    GameMode gameMode = GameMode::Classic;
    // Indices into kRaceTargets / kTimedSeconds (gamemode.h), not the values
    // themselves: these ride straight off the stepped menu rows, and an index
    // cannot land between two steps the way a free-running int could.
    int raceTargetIndex = kRaceTargetDefaultIndex;
    int timedSecondsIndex = kTimedSecondsDefaultIndex;
    AttackMode attackMode = AttackMode::On;
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
// Race and Timed each insert one row directly under Mode -- the pop target,
// or the duration. Only one of the two is ever on screen, and only in its own
// mode, so they share the single slot at kLocalMPRowMode + 1 rather than each
// reserving one and leaving a gap in the other three modes.
inline constexpr int kLocalMPRowModeValue    = 4;

// Whether that slot is occupied. Every row below Mode shifts by this, so the
// panel that draws the list and the handler that reads keys must both ask
// instead of assuming -- the constants below are the layout *without* the
// extra row, and the accessors after them are the only correct way to name a
// row once a mode is known.
inline constexpr int LocalMPRowsAfterMode(GameMode mode) {
    return GameModeCountsPops(mode) ? 1 : 0;
}

inline constexpr int kLocalMPRowMalus        = 4;
inline constexpr int kLocalMPRowTeam         = 5;
inline constexpr int kLocalMPRowVictories    = 6;
inline constexpr int kLocalMPRowBots         = 7;
inline constexpr int kLocalMPRowBotSkill     = 8;
inline constexpr int kLocalMPFirstPlayerRow  = 9;

inline constexpr int LocalMPRowMalus(GameMode mode) {
    return kLocalMPRowMalus + LocalMPRowsAfterMode(mode);
}
inline constexpr int LocalMPRowTeam(GameMode mode) {
    return kLocalMPRowTeam + LocalMPRowsAfterMode(mode);
}
inline constexpr int LocalMPRowVictories(GameMode mode) {
    return kLocalMPRowVictories + LocalMPRowsAfterMode(mode);
}
inline constexpr int LocalMPRowBots(GameMode mode) {
    return kLocalMPRowBots + LocalMPRowsAfterMode(mode);
}
inline constexpr int LocalMPRowBotSkill(GameMode mode) {
    return kLocalMPRowBotSkill + LocalMPRowsAfterMode(mode);
}
inline constexpr int LocalMPFirstPlayerRow(GameMode mode) {
    return kLocalMPFirstPlayerRow + LocalMPRowsAfterMode(mode);
}
inline constexpr int LocalMPAimGuideRow(int playerIndex, GameMode mode) {
    return LocalMPFirstPlayerRow(mode) + playerIndex;
}
inline constexpr int LocalMPColorsRow(int playerIndex, int playerCount, GameMode mode) {
    return LocalMPFirstPlayerRow(mode) + playerCount + playerIndex;
}
inline constexpr int LocalMPStartRow(int playerCount, GameMode mode) {
    return LocalMPFirstPlayerRow(mode) + 2 * playerCount;
}

inline const char* LocalMPBotSkillName(int skill) {
    return skill <= 0 ? "Low" : skill == 1 ? "Med" : "High";
}

// Lowercase form used in a network room bot's own nickname (LobbyBotNick
// below) rather than LocalMPBotSkillName's capitalized display label -- a
// nickname reads as one lowercase token ("bot1-low"), not a settings-row
// value.
inline const char* LocalMPBotSkillNickSuffix(int skill) {
    return skill <= 0 ? "low" : skill == 1 ? "med" : "high";
}

// A network room bot's own nickname (SyncLobbyBots, mainmenu_netpanel.cpp).
// `seatNumber` is 1-based (the room's first bot is bot1, not bot0) and
// stays the only thing distinguishing two bots at the same skill -- the
// server truncates nicknames to ten characters and matches the roster by
// nickname, and the longest form this produces, "bot1-high", is 9 of those
// 10, leaving no room for anything else.
inline std::string LobbyBotNick(int seatNumber, int skill) {
    char nick[16];
    snprintf(nick, sizeof(nick), "bot%d-%s", seatNumber, LocalMPBotSkillNickSuffix(skill));
    return nick;
}

// Which team a local player slot belongs to (1-based team, 0-based slot):
// odd slots on team 1, even on team 2. The setup panel's "Team Mode" label
// and the teams the game actually starts with both derive from this, so the
// label cannot claim a pairing the game does not use.
inline constexpr int LocalMPTeamOf(int playerIndex) {
    return (playerIndex % 2) + 1;
}

// Writes the team split for `playerCount` players, e.g. "P1+P3+P5 vs P2+P4".
// Needs room for kMaxLocalPlayers slots; 48 bytes is comfortably enough.
void LocalMPTeamSplitLabel(char* out, size_t outSize, int playerCount);

enum class LocalMultiplayerMenuCommand {
    Left,
    Right,
    Enter,
};

// Takes the mode because the Victories row's index moves with it -- Race and
// Timed push it down one. Callers pass the mode they are drawing rather than a
// row number, so the two files cannot disagree about where the row is.
bool ApplyLocalMultiplayerVictoriesInput(
    int menuIndex,
    LocalMultiplayerMenuCommand command,
    int& victoriesIndex,
    GameMode mode = GameMode::Classic);

// Steps the Bots row, growing or shrinking the total player count to fit
// instead of capping bots at whatever player count happened to be set
// already -- previously the only way to get a 2nd bot was to first raise
// Players to 3 by hand, a 3rd to raise it to 4, and so on.
//
// Right/Enter: while there is already room (botCount below the current
// cap of playerCount - 1), just add a bot. At the cap, grow playerCount
// by one first (up to kMaxLocalPlayers) so the new bot has a seat; only
// once playerCount is already at its ceiling does this wrap back to 0,
// same as before.
//
// Left: while a spare human-only seat exists (botCount below the current
// cap), just remove a bot, leaving playerCount alone -- a deliberate
// human seat someone added on top of their bots should not vanish just
// because they later turned a bot back off. Only when bots occupy every
// non-P1 seat (botCount == playerCount - 1, i.e. this playerCount exists
// purely to fit that many bots) does removing one also shrink the game,
// so repeatedly pressing Left lands back at kMinLocalPlayers with zero
// bots -- matching how a player who only ever touches this row got here.
// From 0 this still wraps up to the current cap without touching
// playerCount, same as before.
void AdjustLocalBotCount(
    LocalMultiplayerMenuCommand command,
    int& botCount,
    int& playerCount);

LocalMultiplayerOptions BuildLocalMultiplayerOptions(
    int playerCount,
    bool chainReaction,
    bool noCompression,
    GameMode gameMode,
    AttackMode attackMode,
    bool teamMode,
    int victoriesIndex,
    const int colors[5],
    const bool aimGuide[5],
    int botCount = 0,
    int botSkill = 1,
    int raceTargetIndex = kRaceTargetDefaultIndex,
    int timedSecondsIndex = kTimedSecondsDefaultIndex);

// Bots fill the highest player slots. Returns how many the given player count
// can actually take, which is one fewer than the players in the game.
int ClampLocalBotCount(int botCount, int playerCount);

SetupSettings BuildLocalMultiplayerSettings(
    const LocalMultiplayerOptions& options);

#endif
