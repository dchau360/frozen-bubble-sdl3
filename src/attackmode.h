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

#ifndef ATTACKMODE_H
#define ATTACKMODE_H

// Its own header, not a block in bubblegame.h, because both bubblegame.h and
// networkclient.h need the type and bubblegame.h already includes
// networkclient.h -- putting it in either one makes that include cycle.

// What a match does with the attack ("malus") bubbles a big pop earns.
//
// On        -- the original behaviour: every malus you earn is sent straight
//              at your opponents, and every malus sent at you lands.
// Off       -- nobody attacks anybody; the old "Attack bubbles: OFF".
// Canceling -- the malus you earn first pays down whatever is still waiting
//              in your own queue, and only the surplus is sent onward. It can
//              never go negative: cancelling more than you owe just empties
//              the queue and sends nothing. See CheckPossibleDestroy.
//
// The wire format keeps DISABLEMALUS as the original 0/1 and carries the third
// state in a separate MALUSCANCEL field -- see NetworkClient::SendOptions.
enum class AttackMode : int {
    On        = 0,
    Off       = 1,
    Canceling = 2,
};

// Displayed as "Blockable" -- the internal enumerator/function names below
// (Canceling, MalusCancelled, MALUSCANCEL) stay as-is; only the player-facing
// label changed, so nothing about the wire format or the C++ API needed to move.
inline const char* AttackModeName(AttackMode m) {
    return m == AttackMode::Off ? "OFF"
         : m == AttackMode::Canceling ? "Blockable"
         : "ON";
}

// Left/Right on the setting steps On -> Blockable -> Off -> On. Off sits last so
// the two modes that actually send attacks are adjacent.
inline AttackMode NextAttackMode(AttackMode m) {
    return m == AttackMode::On ? AttackMode::Canceling
         : m == AttackMode::Canceling ? AttackMode::Off
         : AttackMode::On;
}
inline AttackMode PrevAttackMode(AttackMode m) {
    return m == AttackMode::On ? AttackMode::Off
         : m == AttackMode::Off ? AttackMode::Canceling
         : AttackMode::On;
}

// How many of the `earned` attack bubbles are absorbed by the `queued` ones
// still waiting to land on the earner's own board. The surplus, earned minus
// this, is what actually gets sent on.
//
// Its own function, rather than a min() inlined at the one call site, so the
// "cannot go negative" rule is stated once and can be tested without building
// a board that produces a real match: the result is clamped to [0, earned] and
// to [0, queued], so neither a bigger pop nor a longer queue can ever turn
// into credit against a future wave.
inline int MalusCancelled(AttackMode mode, int earned, int queued) {
    if (mode != AttackMode::Canceling) return 0;
    if (earned <= 0 || queued <= 0) return 0;
    return earned < queued ? earned : queued;
}

#endif // ATTACKMODE_H
