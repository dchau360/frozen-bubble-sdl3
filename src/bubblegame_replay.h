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

#ifndef BUBBLEGAME_REPLAY_H
#define BUBBLEGAME_REPLAY_H

// The bridge between real engine state (bubblegame.h) and R2's generic wire
// records (replay_format.h). R2 deliberately knows nothing about BubbleGame;
// this module is the only place that reads/writes the two together.
//
// Scope since R6c: local (non-network) rounds of 1-5 seats, Classic, Race and
// Timed modes, random levels or a predefined level, bots, teams, attack modes
// and single-player training (mpTraining). A recorded round-relative game
// clock (StepRecord::gameClockMs) reproduces Timed's countdown and training's
// two-minute timer. Network rounds of 2-20 seats are accepted (a battle-royale
// room simulates up to MAX_NET_PLAYERS boards): seat 0 is this client's own
// board, and every other seat is either a hosted bot this client simulates
// (owned + board isBot) or a genuine remote peer (unowned + not a bot). The
// ordered inbound game payloads applied each step are captured in
// StepRecord::inboundEvents and replayed through ApplyInboundGameMessage().
// The round start is always a full board snapshot taken after NewGame()
// returns, so it picks up SyncNetworkLevel()'s generated layout just as it
// picks up LoadLevel()/RandomLevel() -- including for round 2+ continuity,
// since capture happens after the board is resolved regardless of how it was
// generated. A recorded wasNetworkLeader flag (in the rules blob) lets
// Playback reproduce Timed mode's leader/joiner verdict timing without a live
// NetworkClient. Clear mode is still rejected.
//
// Known limit of this slice: only round 1 of a match is covered for training.
// NewGame() resets mpTrainStartTime, so a training round captured after a live
// ReloadGame() would restore with a stale training clock this design does not
// handle; local ReloadGame() continuity coverage is Race-only (R5a). Network
// round 2+ continuity is covered (R6b), network Timed's leader verdict
// included.
// Anything that would make playback touch disk, a socket or telemetry is
// handled by SessionMode::Playback alone; this module does not add its own
// suppression.
//
// See docs/REPLAY_PLAN.md (Recording contract) and
// docs/REPLAY_PROGRESS.md (R3, R5a, R5b) for the design record and the exact
// field list this module serializes into each opaque record blob.

#include "replay_format.h"
#include "player_controls.h"

class BubbleGame;

// Outcome byte written into RoundEndRecord::outcome. Opaque to the codec
// (replay_format.h), defined here where the game's own result flags are
// known.
enum : uint8_t {
    kReplayOutcomeIncomplete = 0,
    kReplayOutcomeWin = 1,
    kReplayOutcomeLoss = 2,
    kReplayOutcomeDraw = 3,
};

// Captures every seat's board + queues + RNG state + score + effective
// settings at the moment a round is fully initialized and before its first
// step. startingBoards[seat] carries a self-describing board blob (v2: per-row
// horizontal offset + per-board bubble size, see docs/REPLAY_PROGRESS.md for
// its byte layout); the named fields carry the rest, and levelLayout carries a
// small game-rules blob (attackMode/raceTarget/timedSeconds/mpTraining) that
// R2's envelope has no dedicated field for.
RoundStartRecord CaptureRoundStart(const BubbleGame &game);

// The playback initialization path. Recreates the round's scaffolding via
// NewGame() with the recorded effective settings (which resets geometry and
// clears the shared projectile/malus globals), then overwrites every seat's
// board, queues, RNG state, counters and score with the recorded values.
// Finishes by setting SessionMode::Playback so no external effect can fire.
// A malformed/truncated board blob is rejected safely (logged, nothing
// restored) -- decoding never reads out of bounds. Anything outside this
// slice's validated feature set (>5 local seats, Clear, or a network seat whose
// ownership and board isBot flag disagree) is rejected the same way rather
// than half-restored. Network rounds of 2-20 seats are accepted: seat 0 owned
// and non-bot, every other seat owned iff a hosted bot. Timed and training are
// accepted; their clock is reproduced from the recorded steps, not the wall
// clock.
void RestoreRoundStart(BubbleGame &game, const RoundStartRecord &record);

// R6a: decode a StepRecord::inboundEvents blob and hand the ordered
// (senderId, gameData) pairs to the game so the next AdvancePlaybackStep()
// applies them through ApplyInboundGameMessage() before simulating. The blob
// is bounds-checked and capped; a malformed blob is logged and queues nothing
// (never reads out of bounds). An empty blob clears the queue.
void SetPlaybackInboundEvents(BubbleGame &game, const std::vector<uint8_t> &blob);

// R4c: the control-side sibling of SetPlaybackInboundEvents(). A playback
// driver calls this once per seat per step, before AdvancePlaybackStep(), to
// set exactly what ResolvePlayerControls() would have produced live. Out-of-
// range seats are ignored rather than indexed, so a hostile/tampered
// StepRecord::seatId cannot read past bubbleArrays.
void SetPlaybackControls(BubbleGame &game, int seat, const PlayerControls &controls);

// Reads what ResolvePlayerControls() captured into seatId's lastControls this
// step, plus the step index, recorded deltaScale and recorded round-relative
// game clock. One record per seat per step: callers capture every seat after
// AdvanceSimulation() for the step, then replay all of those records before
// AdvancePlaybackStep() for that step. See docs/REPLAY_PROGRESS.md (R5a, R5b)
// for why one record per seat rather than a packed multi-seat record (R2's
// StepRecord has a single seatId).
StepRecord CaptureStep(const BubbleGame &game, int seatId);

// Final result: outcome, per-seat scores/wins, and the canonical state hash
// at round end.
RoundEndRecord CaptureRoundEnd(const BubbleGame &game);

// Canonical FNV-1a hash over an explicit little-endian serialization of the
// simulation-relevant state. "Compare at every step" uses this: both the live
// run and the replay run compute it after each step and the test asserts
// equality.
//
// Deliberately takes the whole BubbleGame rather than a bare BubbleArray: the
// state that can desync a round also includes the per-round settings (seats,
// teams, attack mode, race target), the gameplay RNG stream, and the
// in-flight SingleBubble projectiles, all of which live at game scope (the
// projectiles in a shared global, actually -- see bubblegame_internal.h).
// Every seat's board is hashed, not just seat 0, so a divergence in any
// seat's board is caught.
//
// Derive-only input state is deliberately NOT hashed: hurryTimer/warnTimer,
// suppressFireUntilRelease, mouseTargetAngle and mouseFirePending are
// advanced/consumed by ResolvePlayerControls(), which playback skips by design
// (the resolved controls are captured in lastControls instead, so a
// hurry-forced shot still fires and a mouse-aimed shot still snaps on replay).
// None affect later physics. On the step where a stick sets gameFinish,
// UpdatePenguin() returns before touching them in BOTH runs, so the live value
// is whatever the live input last set and is not captured; hashing them would
// compare an uncaptured value. Raw pointer/draw-cache fields (Penguin textures,
// SDL_Rect caches, lowGfx) are never hashed either.
uint64_t CaptureCanonicalStateHash(const BubbleGame &game);

#endif // BUBBLEGAME_REPLAY_H
