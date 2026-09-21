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

#ifndef REPLAY_PLAYER_H
#define REPLAY_PLAYER_H

// R4c: the playback viewer. R3/R5/R6 built and proved the capture/restore
// engine (bubblegame_replay.h) and R4a/R4b made recordings auto-save into the
// on-disk library, but until this file nothing could play one back outside
// test code: RestoreRoundStart()/AdvancePlaybackStep()/SetPlaybackInboundEvents()
// were only ever driven by a test-only friend struct that reached into
// BubbleGame's private bubbleArrays[].lastControls.
//
// ReplayPlayer is a real, shippable class that turns a decoded .fbr byte stream
// into a running session: it owns exactly one fresh BubbleGame, restores the
// recorded round start into it at Playback session mode, and consumes the
// recorded StepRecords at a caller-chosen rate (0.5x/1x/2x/4x) while verifying
// the recorded per-step canonical state hash. A mismatch stops playback with
// IsDesynced() true rather than silently drifting.
//
// Scope (R4c): load a recording and drive it. No seeking/checkpoints, no
// export/import, no menu row, no key/tap/gamepad binding -- a later package
// (R4d) owns the Replays page and calls this API. Draw() only paints the game
// plus a small non-interactive HUD (REPLAY / mm:ss / speed / PAUSED); it has no
// input path of its own.

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "replay_format.h"
#include "ttftext.h"

class BubbleGame;

class ReplayPlayer
{
public:
    ReplayPlayer() = default;
    ~ReplayPlayer();
    ReplayPlayer(const ReplayPlayer &) = delete;
    ReplayPlayer &operator=(const ReplayPlayer &) = delete;

    // Decode a complete .fbr byte stream and restore its round into a fresh
    // BubbleGame built on `renderer`. Returns false and leaves the player in a
    // clean "not loaded" state -- never a partial one -- on a codec error, a
    // RestoreRoundStart() rejection (unsupported seat count/mode/ownership), or
    // a null renderer. The same loop shape tests/bubblegame_replay_test.cpp's
    // DecodeRecording() proves: ReplayReader::PeekRecordType() in a loop,
    // dispatching on Step/Assertion/RoundEnd, with Truncated as a clean end of
    // stream. A later Load() replaces any previous recording.
    bool Load(const std::vector<uint8_t> &bytes, const SDL_Renderer *renderer);

    // One UI tick. A fractional accumulator consumes `speed` step groups per
    // call: `accumulator += speed; while (accumulator >= 1) consume one group`.
    // Only how many recorded steps run per call changes -- the recorded
    // deltaScale each step replays at never does. A no-op when not loaded,
    // paused, desynced, or already finished.
    void AdvanceFrame();

    void SetPaused(bool paused);
    bool IsPaused() const { return paused_; }

    // Snaps to the nearest of the four supported rates. Those exact values are
    // what the plan specifies; reading back is how a future UI labels them.
    void SetSpeed(float speed);
    float Speed() const { return speed_; }

    // Destroy and rebuild the owned BubbleGame from scratch, re-run
    // RestoreRoundStart() on the fresh instance, and reset the step cursor,
    // accumulator, pause flag and desync flag. Rebuilding rather than rewinding
    // the live instance avoids any question about RestoreRoundStart() being
    // safe to call twice on the same object. Speed is deliberately kept.
    void Restart();

    // R8: jump directly to a step-group index (clamped to [0, groups_.size()]),
    // or to the next/previous "shot" (see shotIndices_). All three rebuild the
    // owned BubbleGame from scratch -- the same approach Restart() already uses
    // -- then replay every step from 0 up to the target through the exact same
    // ConsumeOneGroup() forward playback uses, so a seeked state is byte-
    // identical to what sequential playback would have produced at that step,
    // by construction, and IsDesynced() keeps working unchanged. Audio is muted
    // for the replayed-and-discarded steps (restored after) so a seek does not
    // re-fire every skipped step's sound; ordinary 1x-4x playback never takes
    // this path and is unaffected. A seek always lands paused. No-op when not
    // loaded.
    void SeekToStepIndex(size_t targetIndex);
    void SeekToNextShot();     // no-op if no later shot exists
    void SeekToPreviousShot(); // no-op if no earlier shot exists
    bool HasShots() const { return !shotIndices_.empty(); }

    // Draws the current game state via BubbleGame::Draw() (proven safe
    // headless), then a small non-interactive HUD overlay. A no-op when not
    // loaded.
    void Draw();

    bool IsLoaded() const { return game_ != nullptr; }
    // Every step group consumed. An unloaded player is never "finished".
    bool IsFinished() const { return game_ != nullptr && cursor_ >= groups_.size(); }
    bool IsDesynced() const { return desynced_; }
    // RoundEndRecord::complete, or false when no round-end record was present.
    bool IsRecordingComplete() const { return end_.has_value() && end_->complete != 0; }

    // Elapsed = the last consumed step's game clock minus the first decoded
    // step's. Total = the sealed round's recorded duration when present and
    // nonzero, else the recording's full span (last decoded clock minus first).
    // R4a's CaptureRoundEnd() leaves durationMs at 0 (see
    // docs/REPLAY_PROGRESS.md, R5b), so a zero duration here means "not
    // populated" and falls back rather than reporting a bogus 0:00 total.
    uint32_t ElapsedMs() const;
    uint32_t TotalMs() const;

private:
    // One recorded simulation step: every seat's controls for one simStep, plus
    // the step's shared deltaScale/gameClockMs (CaptureStep() reads all three
    // from the game once per step, so every record in a group agrees).
    struct StepGroup {
        int32_t simStep = 0;
        float deltaScale = 1.0f;
        uint32_t gameClockMs = 0;
        std::vector<StepRecord> seats;
    };

    void Clear();
    void ConsumeOneGroup();
    void DrawHud();

    const SDL_Renderer *renderer_ = nullptr;
    std::unique_ptr<BubbleGame> game_;
    // The decoded round start, kept so Restart() can rebuild without re-reading
    // the original bytes.
    RoundStartRecord start_;
    std::vector<StepGroup> groups_;
    std::map<int32_t, uint64_t> assertions_;
    std::optional<RoundEndRecord> end_;
    // R8: group indices whose controls contain a fire rising edge (0 -> 1) for
    // any seat, populated once per successful Load(). Shot navigation seeks to
    // these rather than to every step.
    std::vector<size_t> shotIndices_;

    size_t cursor_ = 0;
    float speed_ = 1.0f;
    double accumulator_ = 0.0;
    bool paused_ = false;
    bool desynced_ = false;

    uint32_t firstClockMs_ = 0;
    uint32_t currentClockMs_ = 0;
    uint32_t lastClockMs_ = 0;

    // HUD text. TTFText caches its texture and only re-renders when the string
    // changes, so DrawHud() may call UpdateText() every frame (the same
    // recompute-on-change/draw-every-frame idiom as bubblegame_render.cpp's
    // Score/Popped text).
    TTFText hudLabel_;
    TTFText hudTime_;
    TTFText hudSpeed_;
    TTFText hudPaused_;

#ifdef FROZEN_BUBBLE_TEST_ACCESS
    friend struct ReplayPlayerTestAccess;
#endif
};

#endif // REPLAY_PLAYER_H
