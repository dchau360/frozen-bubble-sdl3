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

// R4a: see src/replay_recorder.h for the design. This file is the production
// caller of bubblegame_replay.h's capture API. It performs no simulation
// mutation of any kind -- everything it does is a read of game state plus an
// append to its own writer -- so recording cannot change a step's outcome.

#include "replay_recorder.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <exception>
#include <string>

#include "bubblegame.h"
#include "bubblegame_replay.h"
#include "platform.h"

ReplayRecorder *ReplayRecorder::ptrInstance = nullptr;

ReplayRecorder *ReplayRecorder::Instance() {
    if (ptrInstance == nullptr) ptrInstance = new ReplayRecorder();
    return ptrInstance;
}

ReplayRecorder *ReplayRecorder::Existing() {
    return ptrInstance;
}

void ReplayRecorder::Dispose() {
    if (ptrInstance == nullptr) return;
    // Drop any half-open recording rather than hand it to a sink during
    // shutdown; Dispose() is teardown, not a round boundary.
    ptrInstance->active = false;
    delete ptrInstance;
    ptrInstance = nullptr;
}

void ReplayRecorder::ResetRecordingState() {
    active = false;
    writer = ReplayWriter();
    seatCount = 0;
    network = false;
    finishSeen = false;
    tailActive = false;
    statsComplete = false;
    roundEnd = RoundEndRecord();
    finishClockMs = 0;
    pendingStatSenders.clear();
}

namespace {

// Build fingerprint written into every header. The plan calls APP_VERSION alone
// insufficient; this package adds the envelope and simulation-rules versions,
// which are the two facts that actually decide whether a file can be decoded
// and replayed by this build. A source/commit identifier is R4's job once the
// library needs to reject a file from a different build of the same version.
std::vector<uint8_t> BuildFingerprint() {
    std::string text = std::string("frozen-bubble-sdl3 ") + APP_VERSION +
                       " fmt=" + std::to_string(kCurrentFormatVersion) +
                       " sim=" + std::to_string(kCurrentSimRulesVersion);
    return std::vector<uint8_t>(text.begin(), text.end());
}

} // namespace

void ReplayRecorder::OnRoundStart(const BubbleGame &game) {
    // Seal whatever was still open before starting the next round. Reaching
    // here with an active recording means the previous round never sealed:
    // a mid-round quit/restart (no gameFinish), or a network result tail whose
    // round boundary arrived first. NewGame()/ReloadGame() has already reset
    // the live state, so Seal() reports the stored snapshot (or an incomplete
    // marker when the round never finished) rather than re-reading the game.
    if (active) Seal();

    // Zero-capture gate. No sink means nothing would consume a recording, and
    // keep count 0 means the player disabled the library -- in both cases
    // return before CaptureRoundStart() so there is no capture cost at all.
    if (!sink || keepCount <= 0) return;

    // A replay of a replay is not a recording. RestoreRoundStart() puts the
    // instance into Playback before its NewGame(), so this sees the mode
    // already set and declines.
    if (game.GetSessionMode() != BubbleGame::SessionMode::Live) return;

    const RoundStartRecord start = CaptureRoundStart(game);
    const int seats = static_cast<int>(start.playerCount);
    if (seats <= 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayRecorder: round start reported %d seats; not recording", seats);
        return;
    }

    ReplayHeader header;
    header.buildFingerprint = BuildFingerprint();
    header.platformFloatProfile = ComputeCurrentPlatformFloatProfile();

    writer = ReplayWriter();
    writer.WriteHeader(header);
    writer.WriteRoundStart(start);

    active = true;
    seatCount = seats;
    network = start.networkGame != 0;
    finishSeen = false;
    tailActive = false;
    roundEnd = RoundEndRecord();
    finishClockMs = 0;

    // The only stats this client does not already own are the round stats of
    // remote (non-owned) seats, which arrive as 'S'. A local round and a
    // network round made entirely of hosted bots therefore have nothing to wait
    // for, and seal the moment gameplay finishes.
    pendingStatSenders.clear();
    for (int i = 0; i < seats; ++i) {
        if (!start.seatOwned[i]) pendingStatSenders.push_back(static_cast<int>(start.seatIds[i]));
    }
    statsComplete = pendingStatSenders.empty();
}

void ReplayRecorder::OnStep(const BubbleGame &game) {
    if (!active) return;

    // Defensive counterpart to OnRoundStart()'s Playback gate: if any path ever
    // started a recording while the instance was Live and then flipped it to
    // Playback, discard rather than reporting a replay as a real round.
    if (game.GetSessionMode() != BubbleGame::SessionMode::Live) {
        Abandon();
        return;
    }

    // Exactly what tests/bubblegame_replay_test.cpp's CaptureLiveSteps() does:
    // one StepRecord per seat in seat order, then the canonical-state hash.
    std::vector<StepRecord> stepRecords;
    stepRecords.reserve(static_cast<size_t>(seatCount));
    for (int seat = 0; seat < seatCount; ++seat)
        stepRecords.push_back(CaptureStep(game, seat));
    const uint64_t canonicalHash = CaptureCanonicalStateHash(game);

    // Recognise the end-of-round stats sync ('S') so the network result tail
    // can close as soon as every expected remote seat has reported. Applied
    // inbound payloads are only present on the live network path; a local step
    // reports none.
    if (network) {
        for (const BubbleGame::InboundGameEvent &event : game.AppliedInboundEvents()) {
            if (event.gameData.empty() || event.gameData[0] != 'S') continue;
            pendingStatSenders.erase(
                std::remove(pendingStatSenders.begin(), pendingStatSenders.end(), event.senderId),
                pendingStatSenders.end());
        }
    }
    const bool statsNowComplete = pendingStatSenders.empty();

    // On-disk order matches the replay tests' EncodeRecording(): the step's
    // assertion precedes its per-seat StepRecords. Reading is order-tolerant
    // (DecodeRecording() dispatches on record type), but keeping the two
    // producers byte-identical means a recording made here and one made by the
    // test harness cannot differ.
    AssertionRecord assertion;
    assertion.simStep = stepRecords.empty() ? 0 : stepRecords[0].simStep;
    assertion.seatId = 0;
    assertion.acceptedShotColor = 0;
    assertion.acceptedColumn = -1;
    assertion.acceptedRow = -1;
    assertion.canonicalStateHash = canonicalHash;
    writer.WriteAssertion(assertion);
    for (const StepRecord &record : stepRecords) writer.WriteStep(record);

    // Every seat in one step shares the step's game clock; take it once.
    const uint32_t stepClockMs = stepRecords.empty() ? 0u : stepRecords[0].gameClockMs;

    if (!finishSeen && game.IsGameFinished()) {
        // Snapshot the end of the round against the state as of this step.
        finishSeen = true;
        roundEnd = CaptureRoundEnd(game);
        statsComplete = statsNowComplete;

        if (!network) {
            // A local round has no late traffic to wait for: seal on the step
            // gameFinish became true, after this step's records.
            statsComplete = true;
            Seal();
            return;
        }

        // A network round keeps a bounded result tail open for late
        // 'S'/'P'/'F'/'l' traffic.
        tailActive = true;
        finishClockMs = stepClockMs;
        if (statsComplete) {
            // Every expected stat already arrived (e.g. it rode the same step
            // as the finish announcement): nothing left to wait for.
            Seal();
            return;
        }
    } else if (tailActive) {
        // Refresh the end snapshot so a late 'S'/'P'/'l' lands in the sealed
        // recording, then close on whichever of the two tail conditions hits
        // first.
        roundEnd = CaptureRoundEnd(game);
        statsComplete = statsNowComplete;
        if (statsComplete) {
            Seal();
            return;
        }
        if (stepClockMs - finishClockMs >= kResultTailTimeoutMs) {
            // Stats never arrived inside the window. Seal anyway; Seal() marks
            // the missing stats explicitly rather than fabricating them.
            Seal();
            return;
        }
    }

    // Bounded RAM. Checked after writing so the overrun is caught on the very
    // step that crosses the budget. Sealing here -- rather than evicting the
    // starting snapshot and continuing -- is what keeps an overrun round
    // honestly marked incomplete instead of pretending to be whole.
    if (writer.Bytes().size() > transientBudgetBytes) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayRecorder: transient capture budget exceeded (%zu > %zu bytes); "
                    "sealing the round as incomplete",
                    writer.Bytes().size(), transientBudgetBytes);
        Seal();
        return;
    }
}

void ReplayRecorder::Abandon() {
    ResetRecordingState();
}

void ReplayRecorder::Seal() {
    if (!active) return;

    if (!finishSeen) {
        // The round never reached gameFinish (quit/restart mid-round, or a
        // budget overrun): there is no meaningful result to report, so mark it
        // incomplete instead of inventing one from whatever state remains.
        roundEnd = RoundEndRecord();
        roundEnd.outcome = kReplayOutcomeIncomplete;
        roundEnd.complete = 0;
    } else if (!statsComplete) {
        // Gameplay finished but not every expected remote 'S' arrived. The
        // gameplay itself is intact and replayable; the only thing missing is
        // the stats, and the format's way to say so is complete = 0. Never
        // fabricate a zero-filled stats row and call it complete.
        roundEnd.complete = 0;
    }

    writer.WriteRoundEnd(roundEnd);

    if (sink) {
        // A sink failure (R4b's disk write, eventually) must never escape into
        // the game loop. Nothing here can affect play either way.
        try {
            sink(std::vector<uint8_t>(writer.Bytes()));
        } catch (const std::exception &e) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "ReplayRecorder: sink threw while saving a recording: %s", e.what());
        } catch (...) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "ReplayRecorder: sink threw while saving a recording");
        }
    }

    ResetRecordingState();
}
