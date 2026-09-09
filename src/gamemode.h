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

#ifndef GAMEMODE_H
#define GAMEMODE_H

#include <cstddef>

// Its own header for the same reason attackmode.h is: bubblegame.h and
// networkclient.h both need the type, and bubblegame.h already includes
// networkclient.h -- defining it in either one makes that include cycle.

// What ends a multiplayer round.
//
// Classic -- the original: the last player (or team) still alive wins.
// Clear   -- win by emptying your own board. Forces row collapse and attacks
//            off, since neither has any meaning when nobody is being pushed
//            down; the menu snapshots your previous values and restores them
//            when you leave the mode.
// Race    -- first player to pop `raceTarget` bubbles wins the round outright.
//            Dying still eliminates you, and if everyone but one player dies
//            before anybody reaches the target, that survivor wins the way
//            they would in Classic.
// Timed   -- when `timedSeconds` runs out, whoever has popped the most bubbles
//            wins. Dying freezes your count where it stood rather than
//            removing you from the reckoning. An exact tie at the top is a
//            draw and credits nobody a win.
//
// Race and Timed deliberately leave row collapse and the Attack bubbles
// setting alone: being attacked while racing is a real counter-play, and a
// board that never rises would turn either mode into an untimed drill.
enum class GameMode : int {
    Classic = 0,
    Clear   = 1,
    Race    = 2,
    Timed   = 3,
};

// The wire keeps CLEARMODE as the original 0/1 and carries the two new modes
// in a separate GAMEMODE field -- see NetworkClient::SendOptions. A client
// built before this existed finds no GAMEMODE key, reads CLEARMODE, and plays
// Classic in a Race or Timed room. That is a rule difference, not a desync:
// the round still ends for everyone on the same 'F', it just isn't the end
// that client was expecting. Same degradation MALUSCANCEL already documents.
inline const char* GameModeName(GameMode m) {
    return m == GameMode::Clear ? "Clear"
         : m == GameMode::Race  ? "Race"
         : m == GameMode::Timed ? "Timed"
         : "Classic";
}

// Left/Right on the Mode row steps Classic -> Clear -> Race -> Timed -> Classic.
inline GameMode NextGameMode(GameMode m) {
    return m == GameMode::Classic ? GameMode::Clear
         : m == GameMode::Clear   ? GameMode::Race
         : m == GameMode::Race    ? GameMode::Timed
         : GameMode::Classic;
}
inline GameMode PrevGameMode(GameMode m) {
    return m == GameMode::Classic ? GameMode::Timed
         : m == GameMode::Timed   ? GameMode::Race
         : m == GameMode::Race    ? GameMode::Clear
         : GameMode::Classic;
}

// Anything outside the enum (a malformed OPTIONS push, a settings file edited
// by hand) reads as Classic rather than indexing off the end of the name
// table. Called at every trust boundary that turns an int back into a mode.
inline GameMode ClampGameMode(int raw) {
    return raw == (int)GameMode::Clear ? GameMode::Clear
         : raw == (int)GameMode::Race  ? GameMode::Race
         : raw == (int)GameMode::Timed ? GameMode::Timed
         : GameMode::Classic;
}

// Clear Mode is the only mode that overrides the two rules below; see the
// enum comment on why Race and Timed deliberately do not. Asked as functions
// rather than compared against GameMode::Clear at each of the four menu sites
// so the rule is stated once.
inline bool GameModeForcesNoCompression(GameMode m) { return m == GameMode::Clear; }
inline bool GameModeForcesAttacksOff(GameMode m)    { return m == GameMode::Clear; }

// True when the mode ranks players by how many bubbles they popped -- which is
// also exactly when the round can end without anyone dying, and so when the
// live popped counts have to be synced every shot rather than once at the end.
inline constexpr bool GameModeCountsPops(GameMode m) {
    return m == GameMode::Race || m == GameMode::Timed;
}

// Selectable values for the two rows that appear under Mode. Both are stepped
// rows holding an index into these tables, the same shape kVictoriesLimits
// already uses, so a value can never land between two steps.
inline constexpr int kRaceTargets[] = {10, 20, 25, 30, 40, 50, 75, 100, 150, 200};
inline constexpr int kTimedSeconds[] = {15, 30, 45, 60, 90, 120, 180, 300};

inline constexpr int kRaceTargetDefaultIndex = 5;   // 50 bubbles
inline constexpr int kTimedSecondsDefaultIndex = 1; // 30 seconds

inline constexpr int kRaceTargetDefault = kRaceTargets[kRaceTargetDefaultIndex];
inline constexpr int kTimedSecondsDefault = kTimedSeconds[kTimedSecondsDefaultIndex];

// Index -> value, and value -> nearest index. The reverse direction is what a
// saved setting or an OPTIONS push comes back as: peers exchange the value
// itself (a room set to 50 means 50 to every client, whatever their build's
// table looks like), so the index has to be recovered rather than trusted.
inline int RaceTargetAt(int index) {
    if (index < 0) index = 0;
    if (index >= (int)std::size(kRaceTargets)) index = (int)std::size(kRaceTargets) - 1;
    return kRaceTargets[index];
}
inline int TimedSecondsAt(int index) {
    if (index < 0) index = 0;
    if (index >= (int)std::size(kTimedSeconds)) index = (int)std::size(kTimedSeconds) - 1;
    return kTimedSeconds[index];
}
inline int RaceTargetIndexOf(int value) {
    for (size_t i = 0; i < std::size(kRaceTargets); i++)
        if (kRaceTargets[i] >= value) return (int)i;
    return (int)std::size(kRaceTargets) - 1;
}
inline int TimedSecondsIndexOf(int value) {
    for (size_t i = 0; i < std::size(kTimedSeconds); i++)
        if (kTimedSeconds[i] >= value) return (int)i;
    return (int)std::size(kTimedSeconds) - 1;
}

// Bounds for a target/duration that arrived over the wire rather than off a
// menu row. Deliberately a plain clamp and not a snap to the nearest table
// step: the room's value is authoritative for everyone in it, so a peer whose
// build ships a different set of steps has to honour the number as sent. Only
// the unplayable extremes are refused -- a target of 0 is a round nobody can
// ever win, and neither end of the clock is a game.
inline constexpr int kRaceTargetMin = 1,   kRaceTargetMax = 1000;
inline constexpr int kTimedSecondsMin = 5, kTimedSecondsMax = 3600;

inline int ClampRaceTarget(int value) {
    return value < kRaceTargetMin ? kRaceTargetMin
         : value > kRaceTargetMax ? kRaceTargetMax : value;
}
inline int ClampTimedSeconds(int value) {
    return value < kTimedSecondsMin ? kTimedSecondsMin
         : value > kTimedSecondsMax ? kTimedSecondsMax : value;
}

// Steps either row, wrapping at both ends like every other stepped row on the
// panel. `count` is the table size; shared so the two rows cannot drift apart.
inline int StepModeValueIndex(int index, int count, bool forward) {
    if (count <= 0) return 0;
    if (forward) return index >= count - 1 ? 0 : index + 1;
    return index <= 0 ? count - 1 : index - 1;
}

#endif // GAMEMODE_H
