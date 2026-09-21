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

#ifndef REPLAY_FORMAT_H
#define REPLAY_FORMAT_H

// Generic, versioned binary envelope and record schema for replay files
// (package R2 -- docs/REPLAY_PLAN.md's "Recording contract" table). This is
// the codec only: it does not capture any real BubbleGame/BubbleArray state
// (that is R3's job) and nothing in the engine calls this yet. No SDL/game
// includes on purpose -- same standalone pattern as gameplay_rng.h and
// player_controls.h, so this header can be unit-tested with zero engine
// dependency (see tests/replay_format_test.cpp).
//
// Design notes (see docs/REPLAY_RESEARCH.md for why a versioned, explicit
// codec was chosen over raw struct dumps):
//  - Every multi-byte field is little-endian, written/read explicitly by
//    hand-rolled helpers in replay_format.cpp -- never memcpy'd structs and
//    never host byte order, so files are portable across platforms/compilers.
//  - Record framing after the header is
//    [uint8_t recordType][uint32_t payloadLength][payloadLength bytes].
//  - The reader never throws and never reads past the end of the supplied
//    buffer; every operation returns a DecodeResult.
//  - kMaxPayloadLength caps a single record's declared length so a corrupted
//    or hostile length prefix is rejected before any read/allocate is
//    attempted from it.

#include <cstdint>
#include <vector>

// 4-byte magic identifying a Frozen Bubble Replay file: "FBR1".
inline constexpr uint8_t kReplayMagic[4] = {'F', 'B', 'R', '1'};

// Container/envelope format version (record framing, header layout). This is
// independent of kCurrentSimRulesVersion below -- the byte-level envelope and
// the gameplay simulation it carries can change on separate schedules.
inline constexpr uint16_t kCurrentFormatVersion = 1;

// Gameplay/simulation rules version. Bumped when a change to simulation
// behavior (RNG algorithm, physics, malus rules, etc.) would make an old
// recording replay differently even though the container format itself is
// unchanged.
inline constexpr uint16_t kCurrentSimRulesVersion = 1;

// Reject any record whose declared payload length exceeds this before doing
// any read/allocate with it, so a corrupted or hostile length prefix cannot
// drive an oversized allocation or an out-of-bounds read attempt. 1 MiB is
// generous for a single record (round start/step/assertion/round end) while
// still being a small, fixed cap.
inline constexpr uint32_t kMaxPayloadLength = 1u * 1024u * 1024u;

// Local copy of bubblegame.h's MAX_NET_PLAYERS (src/bubblegame.h:275). This
// header deliberately does not include bubblegame.h (no engine dependency,
// same tradeoff gameplay_rng.h already accepted for its own constants) --
// keep this in sync by hand if the real definition ever changes.
inline constexpr int kMaxPlayers = 20;

// Record type tag, written as the first byte of every record's framing.
// Values are stable on disk -- do not renumber existing entries.
enum class RecordType : uint8_t {
    RoundStart = 1,
    Step = 2,
    Assertion = 3,
    RoundEnd = 4,
    // Checkpoint = 5 -- reserved for R8 (complete restorable simulation
    // state plus the next event cursor and clock/RNG state). Not
    // implemented; the plan's Recording-contract table explicitly defers
    // checkpoints to later. Do not assign 5 to anything else.
};

// Result of a decode operation. The reader never throws; every public
// operation returns one of these instead.
enum class DecodeResult : uint8_t {
    Ok = 0,
    Truncated,          // Buffer ended before the expected data was found.
    BadMagic,           // Header magic did not match kReplayMagic.
    UnsupportedVersion, // Header formatVersion is newer/unrecognized.
    RecordTooLarge,     // Declared payload length exceeds kMaxPayloadLength.
    BadRecordType,      // Record type byte is not one of RecordType's values.
};

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------
// Magic; format version; simulation/rules version; source/build fingerprint;
// platform/float profile; supported feature flags.
struct ReplayHeader {
    uint8_t magic[4] = {kReplayMagic[0], kReplayMagic[1], kReplayMagic[2], kReplayMagic[3]};
    uint16_t formatVersion = kCurrentFormatVersion;
    uint16_t simRulesVersion = kCurrentSimRulesVersion;
    // Length-prefixed build fingerprint (e.g. APP_VERSION plus a build/commit
    // identifier). APP_VERSION alone is insufficient per the plan's Recording
    // contract table, so this is a free-form byte string rather than a fixed
    // version number.
    std::vector<uint8_t> buildFingerprint;
    // Coarse platform/toolchain bucket recorded by the capturing build and
    // checked by IsReplayPlatformCompatible() before playback. 0 means "no
    // gate applied": legacy files and unrecognized build variants, always
    // treated as compatible. See ComputeCurrentPlatformFloatProfile().
    uint32_t platformFloatProfile = 0;
    // Bitfield of supported/required features. No bits are assigned yet.
    uint32_t featureFlags = 0;
};

// Coarse platform/toolchain bucket that could plausibly affect
// floating-point/trig replay determinism. 0 is reserved: never emitted by
// a real build, and always treated as compatible by
// IsReplayPlatformCompatible() -- every replay captured before this gate
// existed has platformFloatProfile == 0 in its header, and retroactively
// flagging them all as "foreign" would be a false positive, not a real
// cross-platform risk this project has evidence of.
uint32_t ComputeCurrentPlatformFloatProfile();

// True if a replay recorded with `recordedProfile` is safe to play back
// under this build. recordedProfile == 0 (legacy, or a build variant this
// enum doesn't recognize) is always compatible; a nonzero value is
// compatible only with the exact matching current-build profile.
bool IsReplayPlatformCompatible(uint32_t recordedProfile);

// ---------------------------------------------------------------------------
// Round start
// ---------------------------------------------------------------------------
// Round ID; effective settings; roster/stable seat IDs and ownership; actual
// starting boards and current/next color queues; level identity/hash and
// embedded playable layout; gameplay RNG state; geometry and initial
// timers/counters; starting scores/wins.
//
// This is the generic envelope only: field shapes use primitives sized to
// kMaxPlayers, per this package's scope. Populating them with real
// BubbleGame/BubbleArray state is R3's job, not this one.
struct RoundStartRecord {
    uint32_t roundId = 0;

    // Effective settings (post-resolution, per the plan's explicit "capture
    // effective values after settings resolution" requirement).
    uint8_t playerCount = 0;
    uint8_t gameMode = 0;       // Opaque small enum id; game-side meaning is R3's concern.
    uint32_t victoriesLimit = 0;
    uint8_t networkGame = 0;    // bool, stored as a byte for explicit width.

    // Roster / stable seat IDs and ownership, sized to kMaxPlayers.
    uint32_t seatIds[kMaxPlayers] = {};
    uint8_t seatOwned[kMaxPlayers] = {};   // bool per seat: locally owned/controlled.
    uint8_t seatTeam[kMaxPlayers] = {};

    // Level identity/hash and embedded playable layout. The layout itself is
    // variable-length (board dimensions vary), so it is carried as an opaque
    // serialized byte blob -- R3 defines its actual internal shape once real
    // board state is being captured.
    uint64_t levelHash = 0;
    std::vector<uint8_t> levelLayout;

    // Starting boards / color queues, also opaque per-player blobs for the
    // same reason as levelLayout: this package does not know BubbleArray's
    // real shape yet.
    std::vector<uint8_t> startingBoards[kMaxPlayers];
    uint8_t currentColor[kMaxPlayers] = {};
    uint8_t nextColor[kMaxPlayers] = {};

    // Gameplay RNG state (GameplayRng::State(), a plain uint32_t).
    uint32_t gameplayRngState = 0;

    // Geometry and initial timers/counters.
    int32_t boardGeometryId = 0;
    int32_t initialSimStep = 0;
    float initialStepDeltaScale = 0.0f;

    // Starting scores/wins, sized to kMaxPlayers.
    int32_t startingScore[kMaxPlayers] = {};
    int32_t startingWins[kMaxPlayers] = {};
};

// ---------------------------------------------------------------------------
// Step
// ---------------------------------------------------------------------------
// Exact deltaScale; game clock; normalized controls (preferably changes plus
// run lengths); ordered inbound gameplay events when network support is
// enabled.
struct StepRecord {
    int32_t simStep = 0;
    // Exact float bits for deltaScale -- stored as the raw bit pattern via
    // WriteFloat/ReadFloat so the value round-trips exactly rather than
    // being reformatted through text.
    float deltaScale = 0.0f;
    uint32_t gameClockMs = 0;

    // Normalized controls for one seat this step. A "changes plus run
    // lengths" encoding is left to a later package once real capture volume
    // is known; this package stores one resolved PlayerControls-shaped
    // record per step, which is sufficient to prove the codec's framing,
    // hashing and validation.
    uint32_t seatId = 0;
    uint8_t left = 0, right = 0, center = 0, fire = 0, firedByMouse = 0;
    float mouseAngle = -1.0f;

    // Ordered inbound gameplay events for this step (network support). Left
    // as an opaque byte blob -- R6 (network recording/playback) defines the
    // real per-event shape; this package only proves such a variable-length
    // payload can be framed, hashed and validated safely.
    std::vector<uint8_t> inboundEvents;
};

// ---------------------------------------------------------------------------
// Assertion
// ---------------------------------------------------------------------------
// Accepted shot/color/placement diagnostics and canonical state hashes after
// meaningful changes, plus periodic checks. These verify results; they are
// not applied again as commands.
struct AssertionRecord {
    int32_t simStep = 0;
    uint8_t seatId = 0;
    uint8_t acceptedShotColor = 0;
    int32_t acceptedColumn = 0;
    int32_t acceptedRow = 0;
    // Canonical state hash (FNV-1a 64-bit) over an explicitly serialized
    // byte sequence -- see CanonicalHashFnv1a64 in replay_format.cpp. Never
    // hashes struct padding, pointers, or draw-only caches.
    uint64_t canonicalStateHash = 0;
};

// ---------------------------------------------------------------------------
// Round end
// ---------------------------------------------------------------------------
// Outcome, final stats/hash, duration, complete/aborted status.
struct RoundEndRecord {
    uint8_t outcome = 0;          // Opaque small enum id (win/loss/draw/aborted); game-side meaning is R3's concern.
    uint32_t winningSeatId = 0;
    uint64_t finalStateHash = 0;
    uint32_t durationMs = 0;
    uint8_t complete = 0;         // bool: false for an aborted/incomplete round.
    int32_t finalScore[kMaxPlayers] = {};
    int32_t finalWins[kMaxPlayers] = {};
};

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------
// Appends records to an internal byte buffer and exposes it. Never fails --
// there is nothing in this package that can make encoding a well-formed
// in-memory struct fail; validation lives entirely on the reader side, which
// is the side that faces untrusted/corrupted bytes.
class ReplayWriter {
public:
    void WriteHeader(const ReplayHeader &header);
    void WriteRoundStart(const RoundStartRecord &record);
    void WriteStep(const StepRecord &record);
    void WriteAssertion(const AssertionRecord &record);
    void WriteRoundEnd(const RoundEndRecord &record);

    const std::vector<uint8_t> &Bytes() const { return buffer; }

private:
    std::vector<uint8_t> buffer;
};

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------
// Constructed from a buffer it does not own (caller must keep it alive for
// the reader's lifetime). Decodes sequentially: ReadHeader() first, then
// repeated ReadRecordType()/ReadXxx() calls. Every operation is bounds-
// checked against the supplied length and returns DecodeResult -- never
// throws, never reads past data + size.
class ReplayReader {
public:
    ReplayReader(const uint8_t *data, size_t size) : data(data), size(size), pos(0) {}
    explicit ReplayReader(const std::vector<uint8_t> &bytes) : data(bytes.data()), size(bytes.size()), pos(0) {}

    DecodeResult ReadHeader(ReplayHeader &out);

    // Peeks the next record's type without consuming it. Returns
    // DecodeResult::Truncated if there is no more data (a valid "end of
    // stream" condition for a caller looping over records, not necessarily
    // an error) and DecodeResult::BadRecordType if the byte present is not a
    // known RecordType.
    DecodeResult PeekRecordType(RecordType &out) const;

    DecodeResult ReadRoundStart(RoundStartRecord &out);
    DecodeResult ReadStep(StepRecord &out);
    DecodeResult ReadAssertion(AssertionRecord &out);
    DecodeResult ReadRoundEnd(RoundEndRecord &out);

    // Bytes remaining, for callers that want to loop "while remaining > 0".
    size_t Remaining() const { return size - pos; }

private:
    const uint8_t *data;
    size_t size;
    size_t pos;

    // Reads and validates a record's [type][length] framing, and returns the
    // payload's start offset and length on success. Does not advance pos
    // itself past the payload -- callers must consume exactly the returned
    // payloadLength bytes (or return early on error), since the framing
    // check has already verified that many bytes are present.
    DecodeResult ReadRecordFrame(RecordType expected, size_t &payloadStart, uint32_t &payloadLength);
};

// FNV-1a 64-bit hash over an explicitly-serialized byte sequence. Used as the
// "canonical hash" for AssertionRecord/RoundEndRecord state hashes. No
// external dependency -- there is no existing hash utility in src/ to reuse.
uint64_t CanonicalHashFnv1a64(const uint8_t *data, size_t size);
inline uint64_t CanonicalHashFnv1a64(const std::vector<uint8_t> &bytes) {
    return CanonicalHashFnv1a64(bytes.data(), bytes.size());
}

#endif // REPLAY_FORMAT_H
