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

#ifndef REPLAY_RECORDER_H
#define REPLAY_RECORDER_H

// R4a: the production capture driver. R3/R5/R6 built the pure capture/restore
// API in bubblegame_replay.h and proved it in tests, but until this file
// nothing in src/ ever called it -- so a live round was only ever recorded
// from tests. BubbleGame now notifies this singleton at the three points the
// plan's Recording contract names (round start, each completed step, round
// end), and the recorder decides whether to record, captures the round through
// bubblegame_replay.h's API, and hands each sealed recording to an injectable
// sink.
//
// This package deliberately does no file I/O and has no UI: R4b substitutes a
// sink backed by the rolling replay library, R4c adds the viewer. The
// in-flight recording is kept in memory by ReplayWriter (replay_format.h).
//
// Why a singleton rather than a BubbleGame member: the plan's "detached capture
// lifetime" rule is that a *finished* recording outlives the round state it
// came from -- QuitToTitle(), a tournament return, or app exit must not take
// the recording with it. A BubbleGame is reset by the next NewGame()/
// ReloadGame(), so the recorder is the one owner that outlives it, exactly like
// AudioMixer/HighscoreManager/TransitionManager.
//
// The hooks are no-ops unless a recording is active. Production only creates
// the singleton once a later package installs a real sink, so in this package
// the hooks cost nothing at all.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "replay_format.h"

class BubbleGame;

class ReplayRecorder final
{
public:
    // Receives one finished recording's complete .fbr bytes (header through
    // round end). Passed by value so the sink owns them outright: Seal()
    // discards the writer's buffer as soon as the call returns. R4a's test sink
    // collects them; R4b's write path takes this over without any other change
    // to the hooks.
    using Sink = std::function<void(std::vector<uint8_t>)>;

    // Standard subsystem-singleton pair: Instance() creates on first use (the
    // tests and, later, the replay library call this), Existing() returns null
    // if nothing has created it yet. The BubbleGame hooks only ever use
    // Existing(), so they never instantiate a recorder that nothing asked for.
    static ReplayRecorder *Instance();
    static ReplayRecorder *Existing();
    void Dispose();

    // Install/replace the finished-bytes sink. A recorder with no sink records
    // nothing (OnRoundStart() declines before taking the round-start snapshot).
    void SetSink(Sink newSink) { sink = std::move(newSink); }
    bool HasSink() const { return static_cast<bool>(sink); }

    // Rolling-library keep count. 0 or less disables capture outright --
    // OnRoundStart() returns before CaptureRoundStart(), so a disabled recorder
    // has literally zero capture cost rather than recording-then-discarding.
    // Defaults to 5; R4b seeds the real value from GameSettings' Replay:KeepCount
    // INI key (whose clamp and default live there, not here).
    int KeepCount() const { return keepCount; }
    void SetKeepCount(int count) { keepCount = count; }

    // Transient RAM budget for the active recording -- ReplayWriter's growing
    // in-memory byte buffer, which is what "steps x seats" would otherwise let
    // grow without bound on a long round. On overrun the recorder stops
    // capturing and seals what it has marked incomplete; it never drops the
    // starting snapshot and then claims the round is complete. The default
    // (64 MiB) is a conservative placeholder for a full 20-seat battle-royale
    // round and is expected to be replaced with a measured number once R4/R7
    // have real file sizes; the setter exists so this package's test can force
    // the overrun path with a tiny value instead of allocating 64 MiB.
    void SetTransientBudgetBytes(size_t bytes) { transientBudgetBytes = bytes; }
    size_t TransientBudgetBytes() const { return transientBudgetBytes; }

    // --- hooks called from BubbleGame (all no-ops when idle) ----------------
    // End of NewGame()/ReloadGame(), after the board is fully resolved and
    // before the first AdvanceSimulationAtScale(). Seals any still-open
    // recording first (the previous round's state is already gone by this
    // point, so a recording that never reached gameFinish is sealed as
    // incomplete), then starts a fresh one unless capture is disabled or the
    // game is in Playback.
    void OnRoundStart(const BubbleGame &game);
    // End of AdvanceSimulationAtScale(), after the step's mutation is complete.
    // Captures one StepRecord per seat plus the canonical-state assertion in
    // exactly the order tests/bubblegame_replay_test.cpp's CaptureLiveSteps()
    // does, then drives sealing. Reads the step's applied inbound payloads from
    // BubbleGame::AppliedInboundEvents() to recognise a peer's end-of-round
    // 'S' stats sync.
    void OnStep(const BubbleGame &game);

    // Test/diagnostic aid: whether a recording is currently in flight.
    bool IsRecording() const { return active; }

private:
    ReplayRecorder() = default;
    ~ReplayRecorder() = default;
    ReplayRecorder(const ReplayRecorder &) = delete;
    ReplayRecorder &operator=(const ReplayRecorder &) = delete;

    // How long after gameFinish a network result tail stays open waiting for
    // late 'S'/'P'/'F'/'l' traffic, measured in the recorded round-relative
    // game clock (not wall time, so R4b's viewer and this package's tests can
    // reproduce it deterministically). Two seconds is the plan's proposal.
    static constexpr uint32_t kResultTailTimeoutMs = 2000;

    // Seal the in-flight recording: finalise the round-end record, hand the
    // bytes to the sink, and reset. Safe to call when idle.
    void Seal();
    // Discard the in-flight recording without notifying the sink. Used for the
    // defensive Playback guard in OnStep() (a playback must produce neither a
    // recording nor a sink call).
    void Abandon();
    // Drop every per-recording field back to its idle value. Does not touch
    // config (sink/keepCount/budget).
    void ResetRecordingState();

    Sink sink;
    int keepCount = 5;
    size_t transientBudgetBytes = 64u * 1024u * 1024u;

    // In-flight recording.
    bool active = false;
    ReplayWriter writer;
    int seatCount = 0;
    bool network = false;
    bool finishSeen = false;
    bool tailActive = false;
    bool statsComplete = false;
    // Latest end-of-round snapshot. Taken on the step gameFinish becomes true
    // and refreshed on every result-tail step, so a seal triggered later (a
    // round boundary, the timeout) still reports the old round's end state even
    // though NewGame/ReloadGame has already reset the live instance by then.
    RoundEndRecord roundEnd;
    uint32_t finishClockMs = 0;
    // Lobby ids of the non-owned seats whose end-of-round 'S' has not arrived
    // yet. Empty means every expected stat has arrived (true for a local round
    // and for a network round whose other seats are all hosted bots).
    std::vector<int> pendingStatSenders;

    static ReplayRecorder *ptrInstance;
};

#endif // REPLAY_RECORDER_H
