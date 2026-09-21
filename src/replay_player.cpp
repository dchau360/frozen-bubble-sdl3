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

#include "replay_player.h"

#include "audiomixer.h"
#include "bubblegame.h"
#include "bubblegame_replay.h"
#include "platform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace {

constexpr float kReplaySpeeds[] = {0.5f, 1.0f, 2.0f, 4.0f};

// mm:ss, as the viewer's elapsed/total readout. A recording is bounded by the
// transient capture budget long before the minute field overflows two digits,
// so a plain %02u is honest rather than truncating.
std::string FormatClock(uint32_t ms) {
    const uint32_t totalSeconds = ms / 1000u;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02u:%02u",
                  static_cast<unsigned>(totalSeconds / 60u),
                  static_cast<unsigned>(totalSeconds % 60u));
    return std::string(buf);
}

// "0.5x" / "1x" / "2x" / "4x". %g drops the trailing .0 on the integral rates.
std::string FormatSpeed(float speed) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%gx", static_cast<double>(speed));
    return std::string(buf);
}

PlayerControls ControlsFromRecord(const StepRecord &s) {
    PlayerControls c;
    c.left = s.left != 0;
    c.right = s.right != 0;
    c.center = s.center != 0;
    c.fire = s.fire != 0;
    c.firedByMouse = s.firedByMouse != 0;
    c.mouseAngle = s.mouseAngle;
    return c;
}

} // namespace

ReplayPlayer::~ReplayPlayer() = default;

void ReplayPlayer::Clear() {
    game_.reset();
    start_ = RoundStartRecord{};
    groups_.clear();
    assertions_.clear();
    shotIndices_.clear();
    end_.reset();
    cursor_ = 0;
    speed_ = 1.0f;
    accumulator_ = 0.0;
    paused_ = false;
    desynced_ = false;
    firstClockMs_ = 0;
    currentClockMs_ = 0;
    lastClockMs_ = 0;
    renderer_ = nullptr;
}

bool ReplayPlayer::Load(const std::vector<uint8_t> &bytes, const SDL_Renderer *renderer) {
    Clear();
    if (!renderer) return false;

    // Decode every record into locals first: nothing below touches the player
    // until the whole stream has decoded and RestoreRoundStart() has accepted
    // it, so a failure anywhere leaves no partial state behind.
    ReplayReader reader(bytes);
    ReplayHeader header;
    if (reader.ReadHeader(header) != DecodeResult::Ok) return false;

    RoundStartRecord start;
    if (reader.ReadRoundStart(start) != DecodeResult::Ok) return false;

    std::vector<StepRecord> steps;
    std::map<int32_t, uint64_t> assertions;
    std::optional<RoundEndRecord> end;
    while (true) {
        RecordType type;
        const DecodeResult peek = reader.PeekRecordType(type);
        if (peek == DecodeResult::Truncated) break;  // clean end of stream
        if (peek != DecodeResult::Ok) return false;

        if (type == RecordType::Step) {
            StepRecord s;
            if (reader.ReadStep(s) != DecodeResult::Ok) return false;
            steps.push_back(std::move(s));
        } else if (type == RecordType::Assertion) {
            AssertionRecord a;
            if (reader.ReadAssertion(a) != DecodeResult::Ok) return false;
            assertions[a.simStep] = a.canonicalStateHash;
        } else if (type == RecordType::RoundEnd) {
            RoundEndRecord e;
            if (reader.ReadRoundEnd(e) != DecodeResult::Ok) return false;
            end = e;
        } else {
            // A record type the codec does not know (including the reserved-
            // for-R8 Checkpoint value) is not playable here.
            return false;
        }
    }

    // A seat past the round's own playerCount would index an unused or out-of-
    // range board. SetPlaybackControls() guards the final index; rejecting here
    // makes a tampered record fail at Load instead of halfway through playback.
    for (const StepRecord &s : steps) {
        if (s.seatId >= static_cast<uint32_t>(start.playerCount)) return false;
    }

    // Group consecutive records sharing one simStep. CaptureStep() reads
    // simStep/deltaScale/gameClockMs from the game once per step, so every
    // record in a group carries the same three values; take them from the
    // group's first record (the same grouping RunReplay()'s driving loop does).
    std::vector<StepGroup> groups;
    for (const StepRecord &s : steps) {
        if (groups.empty() || groups.back().simStep != s.simStep) {
            StepGroup group;
            group.simStep = s.simStep;
            group.deltaScale = s.deltaScale;
            group.gameClockMs = s.gameClockMs;
            groups.push_back(std::move(group));
        }
        groups.back().seats.push_back(s);
    }

    // Build the playback instance. RestoreRoundStart() enters Playback mode
    // before its NewGame(), and bails without touching the instance on an
    // unsupported record -- so a still-Live session mode is the rejection
    // signal, and the freshly built game is simply discarded.
    auto game = std::make_unique<BubbleGame>(renderer);
    RestoreRoundStart(*game, start);
    if (game->GetSessionMode() != BubbleGame::SessionMode::Playback) return false;

    game_ = std::move(game);
    renderer_ = renderer;
    start_ = std::move(start);
    groups_ = std::move(groups);
    assertions_ = std::move(assertions);
    end_ = end;
    if (!steps.empty()) {
        firstClockMs_ = steps.front().gameClockMs;
        lastClockMs_ = steps.back().gameClockMs;
    }
    currentClockMs_ = firstClockMs_;
    cursor_ = 0;
    speed_ = 1.0f;
    accumulator_ = 0.0;
    paused_ = false;
    desynced_ = false;

    // HUD font/text, loaded once per successful Load. Sized to sit inside the
    // top-left panel without covering the shooter.
    hudLabel_.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    hudLabel_.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
    hudTime_.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    hudTime_.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
    hudSpeed_.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    hudSpeed_.UpdateColor({255, 220, 90, 255}, {0, 0, 0, 255});
    hudPaused_.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    hudPaused_.UpdateColor({255, 90, 90, 255}, {0, 0, 0, 255});

    // R8: record the groups where a shot (a fire control rising 0 -> 1) first
    // appears for any seat, for the shot-navigation buttons/keys. Every seat's
    // fire state is updated every group -- no early break -- so a later seat
    // in the same group is never skipped.
    {
        std::map<uint32_t, bool> lastFire;
        shotIndices_.clear();
        for (size_t i = 0; i < groups_.size(); ++i) {
            bool shotThisGroup = false;
            for (const StepRecord &s : groups_[i].seats) {
                bool &prev = lastFire[s.seatId];
                const bool nowFiring = s.fire != 0;
                if (nowFiring && !prev) shotThisGroup = true;
                prev = nowFiring;
            }
            if (shotThisGroup) shotIndices_.push_back(i);
        }
    }

    return true;
}

void ReplayPlayer::AdvanceFrame() {
    if (!game_ || paused_ || desynced_ || cursor_ >= groups_.size()) return;

    // Fractional accumulator, never a rebuilt deltaScale: 2x runs two recorded
    // steps for every call, each still at its own recorded physics scale. There
    // is no seek/catch-up path, so this loop can never run away.
    accumulator_ += static_cast<double>(speed_);
    while (accumulator_ >= 1.0 && !desynced_ && cursor_ < groups_.size()) {
        ConsumeOneGroup();
        accumulator_ -= 1.0;
    }
}

void ReplayPlayer::ConsumeOneGroup() {
    const StepGroup &group = groups_[cursor_];
    for (const StepRecord &s : group.seats) {
        SetPlaybackControls(*game_, static_cast<int>(s.seatId), ControlsFromRecord(s));
        // R6a's design: only seat 0's record carries the step's applied network
        // payloads (they are a property of the step, not of a seat).
        if (s.seatId == 0) SetPlaybackInboundEvents(*game_, s.inboundEvents);
    }

    game_->AdvancePlaybackStep(group.deltaScale, group.gameClockMs);
    currentClockMs_ = group.gameClockMs;
    ++cursor_;

    // Verify the state the step actually produced against the recorded
    // assertion, and stop for good on the first mismatch. A corrupted or
    // tampered file must fail visibly instead of silently playing a different
    // round.
    const auto it = assertions_.find(group.simStep);
    if (it != assertions_.end() && CaptureCanonicalStateHash(*game_) != it->second) {
        desynced_ = true;
    }
}

void ReplayPlayer::SetPaused(bool paused) {
    paused_ = paused;
}

void ReplayPlayer::SetSpeed(float speed) {
    if (!std::isfinite(speed)) speed = 1.0f;
    float best = kReplaySpeeds[0];
    float bestDistance = std::fabs(speed - kReplaySpeeds[0]);
    for (const float candidate : kReplaySpeeds) {
        const float distance = std::fabs(speed - candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }
    speed_ = best;
}

void ReplayPlayer::Restart() {
    if (!renderer_ || !game_) return;

    auto fresh = std::make_unique<BubbleGame>(renderer_);
    RestoreRoundStart(*fresh, start_);
    if (fresh->GetSessionMode() != BubbleGame::SessionMode::Playback) return;
    game_ = std::move(fresh);

    cursor_ = 0;
    accumulator_ = 0.0;
    paused_ = false;
    desynced_ = false;
    currentClockMs_ = firstClockMs_;
}

void ReplayPlayer::SeekToStepIndex(size_t targetIndex) {
    if (!game_) return;
    if (targetIndex > groups_.size()) targetIndex = groups_.size();

    // R8: mute audio while the discarded catch-up steps replay, then restore.
    // AudioMixer::MuteAll()'s argument is inverted from its name: enable=false
    // mutes and enable=true unmutes (see bubblegame_input.cpp's F11 handler,
    // which does exactly this save/restore dance).
    AudioMixer *mixer = AudioMixer::Instance();
    const bool wasHalted = mixer->IsHalted();
    if (!wasHalted) mixer->MuteAll();

    auto fresh = std::make_unique<BubbleGame>(renderer_);
    RestoreRoundStart(*fresh, start_);
    if (fresh->GetSessionMode() != BubbleGame::SessionMode::Playback) {
        if (!wasHalted) mixer->MuteAll(true);
        return;
    }
    game_ = std::move(fresh);

    cursor_ = 0;
    accumulator_ = 0.0;
    desynced_ = false;
    currentClockMs_ = firstClockMs_;

    while (cursor_ < targetIndex && !desynced_) ConsumeOneGroup();

    paused_ = true;
    if (!wasHalted) mixer->MuteAll(true);
}

void ReplayPlayer::SeekToNextShot() {
    if (!game_) return;
    for (const size_t index : shotIndices_) {
        if (index > cursor_) {
            SeekToStepIndex(index);
            return;
        }
    }
}

void ReplayPlayer::SeekToPreviousShot() {
    if (!game_) return;
    for (auto it = shotIndices_.rbegin(); it != shotIndices_.rend(); ++it) {
        if (*it < cursor_) {
            SeekToStepIndex(*it);
            return;
        }
    }
}

void ReplayPlayer::Draw() {
    if (!game_) return;
    game_->Draw();
    DrawHud();
}

void ReplayPlayer::DrawHud() {
    SDL_Renderer *rend = const_cast<SDL_Renderer *>(renderer_);
    if (!rend) return;

    const std::string timeText = FormatClock(ElapsedMs()) + " / " + FormatClock(TotalMs());
    const std::string speedText = FormatSpeed(speed_);

    hudLabel_.UpdateText(rend, "REPLAY", 0);
    hudTime_.UpdateText(rend, timeText.c_str(), 0);
    hudSpeed_.UpdateText(rend, speedText.c_str(), 0);
    if (paused_) hudPaused_.UpdateText(rend, "PAUSED", 0);

    // Translucent backing panel so the readout survives any board underneath.
    // Blend mode is returned to NONE immediately, matching the game's own
    // transient rect draws.
    SDL_FRect panel{6.f, 6.f, 190.f, 62.f};
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 160);
    SDL_RenderFillRect(rend, &panel);
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);

    auto blit = [rend](TTFText &text, SDL_Point pos) {
        text.UpdatePosition(pos);
        if (text.Texture()) {
            SDL_FRect fr = ToFRect(*text.Coords());
            SDL_RenderTexture(rend, text.Texture(), nullptr, &fr);
        }
    };
    blit(hudLabel_, {14, 10});
    blit(hudTime_, {14, 30});
    blit(hudSpeed_, {14, 50});
    if (paused_) blit(hudPaused_, {110, 50});
}

uint32_t ReplayPlayer::ElapsedMs() const {
    if (!game_) return 0;
    return currentClockMs_ >= firstClockMs_ ? currentClockMs_ - firstClockMs_ : 0;
}

uint32_t ReplayPlayer::TotalMs() const {
    if (!game_) return 0;
    if (end_ && end_->durationMs != 0) return end_->durationMs;
    return lastClockMs_ >= firstClockMs_ ? lastClockMs_ - firstClockMs_ : 0;
}
