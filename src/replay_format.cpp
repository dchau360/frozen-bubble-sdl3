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

#include "replay_format.h"

#include <cstdint>
#include <cstring>

namespace {

// Platform/toolchain buckets for ReplayHeader::platformFloatProfile.
// Deliberately coarse and stable on disk -- a value is only ever added, never
// renumbered, so an old file's profile keeps its meaning. 0 is reserved as
// "no gate applied" (see the header).
constexpr uint32_t kPlatformProfileLinuxX86_64 = 1;
constexpr uint32_t kPlatformProfileMacosArm64 = 2;
constexpr uint32_t kPlatformProfileMacosX86_64 = 3;
constexpr uint32_t kPlatformProfileWindowsX86_64 = 4;
constexpr uint32_t kPlatformProfileAndroidArm64 = 5;
constexpr uint32_t kPlatformProfileAndroidArmv7 = 6;
constexpr uint32_t kPlatformProfileAndroidX86_64 = 7;
constexpr uint32_t kPlatformProfileWasm = 8;
constexpr uint32_t kPlatformProfileLinuxArm64 = 9;

bool IsValidRecordType(uint8_t v) {
    switch (static_cast<RecordType>(v)) {
        case RecordType::RoundStart:
        case RecordType::Step:
        case RecordType::Assertion:
        case RecordType::RoundEnd:
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Writer-side append helpers. All multi-byte fields are little-endian,
// written explicitly byte-by-byte -- never memcpy'd structs, never host byte
// order.
// ---------------------------------------------------------------------------

void AppendU8(std::vector<uint8_t> &buf, uint8_t v) {
    buf.push_back(v);
}

void AppendU16(std::vector<uint8_t> &buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFFu));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}

void AppendU32(std::vector<uint8_t> &buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFFu));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

void AppendU64(std::vector<uint8_t> &buf, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

void AppendI32(std::vector<uint8_t> &buf, int32_t v) {
    AppendU32(buf, static_cast<uint32_t>(v));
}

void AppendFloat(std::vector<uint8_t> &buf, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    AppendU32(buf, bits);
}

void AppendBytes(std::vector<uint8_t> &buf, const std::vector<uint8_t> &bytes) {
    AppendU32(buf, static_cast<uint32_t>(bytes.size()));
    buf.insert(buf.end(), bytes.begin(), bytes.end());
}

// Writes a record's [type][length][payload] framing around an
// already-serialized payload.
void AppendFrame(std::vector<uint8_t> &out, RecordType type, const std::vector<uint8_t> &payload) {
    out.push_back(static_cast<uint8_t>(type));
    AppendU32(out, static_cast<uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
}

// ---------------------------------------------------------------------------
// Reader-side bounds-checked helpers. Every one checks remaining buffer
// length against `size` before touching `data[pos]`, and never advances
// `pos` past `size`. None of these throw.
// ---------------------------------------------------------------------------

DecodeResult ReadU8(const uint8_t *data, size_t size, size_t &pos, uint8_t &out) {
    if (size - pos < 1) return DecodeResult::Truncated;
    out = data[pos];
    pos += 1;
    return DecodeResult::Ok;
}

DecodeResult ReadU16(const uint8_t *data, size_t size, size_t &pos, uint16_t &out) {
    if (size - pos < 2) return DecodeResult::Truncated;
    out = static_cast<uint16_t>(data[pos]) | (static_cast<uint16_t>(data[pos + 1]) << 8);
    pos += 2;
    return DecodeResult::Ok;
}

DecodeResult ReadU32(const uint8_t *data, size_t size, size_t &pos, uint32_t &out) {
    if (size - pos < 4) return DecodeResult::Truncated;
    out = static_cast<uint32_t>(data[pos]) |
          (static_cast<uint32_t>(data[pos + 1]) << 8) |
          (static_cast<uint32_t>(data[pos + 2]) << 16) |
          (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;
    return DecodeResult::Ok;
}

DecodeResult ReadU64(const uint8_t *data, size_t size, size_t &pos, uint64_t &out) {
    if (size - pos < 8) return DecodeResult::Truncated;
    out = 0;
    for (int i = 0; i < 8; ++i) {
        out |= static_cast<uint64_t>(data[pos + i]) << (8 * i);
    }
    pos += 8;
    return DecodeResult::Ok;
}

DecodeResult ReadI32(const uint8_t *data, size_t size, size_t &pos, int32_t &out) {
    uint32_t bits = 0;
    DecodeResult r = ReadU32(data, size, pos, bits);
    if (r != DecodeResult::Ok) return r;
    out = static_cast<int32_t>(bits);
    return DecodeResult::Ok;
}

DecodeResult ReadFloat(const uint8_t *data, size_t size, size_t &pos, float &out) {
    uint32_t bits = 0;
    DecodeResult r = ReadU32(data, size, pos, bits);
    if (r != DecodeResult::Ok) return r;
    std::memcpy(&out, &bits, sizeof(out));
    return DecodeResult::Ok;
}

// Length-prefixed byte string. Checks the declared length against the
// max-payload cap and the actual remaining buffer length before copying any
// bytes -- the same ordering the record-framing check uses, applied here too
// since a nested field (build fingerprint, level layout, a board blob,
// inbound-events blob) is exactly as capable of lying about its length as a
// top-level record is.
DecodeResult ReadBytes(const uint8_t *data, size_t size, size_t &pos, std::vector<uint8_t> &out) {
    uint32_t length = 0;
    DecodeResult r = ReadU32(data, size, pos, length);
    if (r != DecodeResult::Ok) return r;
    if (length > kMaxPayloadLength) return DecodeResult::RecordTooLarge;
    if (size - pos < length) return DecodeResult::Truncated;
    out.assign(data + pos, data + pos + length);
    pos += length;
    return DecodeResult::Ok;
}

} // namespace

// ---------------------------------------------------------------------------
// Platform/float compatibility gate
// ---------------------------------------------------------------------------

// Known tradeoff: an exotic or unrecognized platform/arch (armv7/ppc64le/
// s390x Linux, a cross-compiled or embedded target -- anything these
// preprocessor checks don't name) falls back to 0 and is therefore treated
// as compatible, so that platform's real floating-point differences would
// not be caught by this gate. That is deliberate -- refusing playback on a
// platform whose actual risk is unknown would be a false positive, the same
// reasoning that exempts legacy profile == 0 files -- and no such exotic
// platform is known to ship this game today. Linux x86_64 and arm64 (the
// arches third-party distro packagers such as Fedora COPR actually build
// for) are both recognized; see issue #125 for the aarch64 gap this used to
// have.
uint32_t ComputeCurrentPlatformFloatProfile() {
#if defined(__EMSCRIPTEN__)
    // Emscripten also defines __linux__/__unix__, so this must come first.
    return kPlatformProfileWasm;
#elif defined(__ANDROID__)
  #if defined(__aarch64__)
    return kPlatformProfileAndroidArm64;
  #elif defined(__arm__)
    return kPlatformProfileAndroidArmv7;
  #elif defined(__x86_64__)
    return kPlatformProfileAndroidX86_64;
  #else
    return 0;
  #endif
#elif defined(__APPLE__)
  #if defined(__aarch64__) || defined(__arm64__)
    return kPlatformProfileMacosArm64;
  #elif defined(__x86_64__)
    return kPlatformProfileMacosX86_64;
  #else
    return 0;
  #endif
#elif defined(_WIN32)
  #if defined(_M_X64) || defined(__x86_64__)
    return kPlatformProfileWindowsX86_64;
  #else
    return 0;
  #endif
#elif defined(__linux__)
  #if defined(__x86_64__)
    return kPlatformProfileLinuxX86_64;
  #elif defined(__aarch64__)
    return kPlatformProfileLinuxArm64;
  #else
    return 0;
  #endif
#else
    return 0;
#endif
}

bool IsReplayPlatformCompatible(uint32_t recordedProfile) {
    if (recordedProfile == 0) return true;
    return recordedProfile == ComputeCurrentPlatformFloatProfile();
}

// ---------------------------------------------------------------------------
// ReplayWriter
// ---------------------------------------------------------------------------

void ReplayWriter::WriteHeader(const ReplayHeader &header) {
    buffer.insert(buffer.end(), header.magic, header.magic + 4);
    AppendU16(buffer, header.formatVersion);
    AppendU16(buffer, header.simRulesVersion);
    AppendBytes(buffer, header.buildFingerprint);
    AppendU32(buffer, header.platformFloatProfile);
    AppendU32(buffer, header.featureFlags);
}

void ReplayWriter::WriteRoundStart(const RoundStartRecord &record) {
    std::vector<uint8_t> payload;
    AppendU32(payload, record.roundId);
    AppendU8(payload, record.playerCount);
    AppendU8(payload, record.gameMode);
    AppendU32(payload, record.victoriesLimit);
    AppendU8(payload, record.networkGame);
    for (int i = 0; i < kMaxPlayers; ++i) AppendU32(payload, record.seatIds[i]);
    for (int i = 0; i < kMaxPlayers; ++i) AppendU8(payload, record.seatOwned[i]);
    for (int i = 0; i < kMaxPlayers; ++i) AppendU8(payload, record.seatTeam[i]);
    AppendU64(payload, record.levelHash);
    AppendBytes(payload, record.levelLayout);
    for (int i = 0; i < kMaxPlayers; ++i) AppendBytes(payload, record.startingBoards[i]);
    for (int i = 0; i < kMaxPlayers; ++i) AppendU8(payload, record.currentColor[i]);
    for (int i = 0; i < kMaxPlayers; ++i) AppendU8(payload, record.nextColor[i]);
    AppendU32(payload, record.gameplayRngState);
    AppendI32(payload, record.boardGeometryId);
    AppendI32(payload, record.initialSimStep);
    AppendFloat(payload, record.initialStepDeltaScale);
    for (int i = 0; i < kMaxPlayers; ++i) AppendI32(payload, record.startingScore[i]);
    for (int i = 0; i < kMaxPlayers; ++i) AppendI32(payload, record.startingWins[i]);
    AppendFrame(buffer, RecordType::RoundStart, payload);
}

void ReplayWriter::WriteStep(const StepRecord &record) {
    std::vector<uint8_t> payload;
    AppendI32(payload, record.simStep);
    AppendFloat(payload, record.deltaScale);
    AppendU32(payload, record.gameClockMs);
    AppendU32(payload, record.seatId);
    AppendU8(payload, record.left);
    AppendU8(payload, record.right);
    AppendU8(payload, record.center);
    AppendU8(payload, record.fire);
    AppendU8(payload, record.firedByMouse);
    AppendFloat(payload, record.mouseAngle);
    AppendBytes(payload, record.inboundEvents);
    AppendFrame(buffer, RecordType::Step, payload);
}

void ReplayWriter::WriteAssertion(const AssertionRecord &record) {
    std::vector<uint8_t> payload;
    AppendI32(payload, record.simStep);
    AppendU8(payload, record.seatId);
    AppendU8(payload, record.acceptedShotColor);
    AppendI32(payload, record.acceptedColumn);
    AppendI32(payload, record.acceptedRow);
    AppendU64(payload, record.canonicalStateHash);
    AppendFrame(buffer, RecordType::Assertion, payload);
}

void ReplayWriter::WriteRoundEnd(const RoundEndRecord &record) {
    std::vector<uint8_t> payload;
    AppendU8(payload, record.outcome);
    AppendU32(payload, record.winningSeatId);
    AppendU64(payload, record.finalStateHash);
    AppendU32(payload, record.durationMs);
    AppendU8(payload, record.complete);
    for (int i = 0; i < kMaxPlayers; ++i) AppendI32(payload, record.finalScore[i]);
    for (int i = 0; i < kMaxPlayers; ++i) AppendI32(payload, record.finalWins[i]);
    AppendFrame(buffer, RecordType::RoundEnd, payload);
}

// ---------------------------------------------------------------------------
// ReplayReader
// ---------------------------------------------------------------------------

DecodeResult ReplayReader::ReadHeader(ReplayHeader &out) {
    if (size - pos < 4) return DecodeResult::Truncated;
    if (std::memcmp(data + pos, kReplayMagic, 4) != 0) return DecodeResult::BadMagic;
    std::memcpy(out.magic, data + pos, 4);
    pos += 4;

    DecodeResult r = ReadU16(data, size, pos, out.formatVersion);
    if (r != DecodeResult::Ok) return r;
    if (out.formatVersion > kCurrentFormatVersion) return DecodeResult::UnsupportedVersion;

    r = ReadU16(data, size, pos, out.simRulesVersion);
    if (r != DecodeResult::Ok) return r;

    r = ReadBytes(data, size, pos, out.buildFingerprint);
    if (r != DecodeResult::Ok) return r;

    r = ReadU32(data, size, pos, out.platformFloatProfile);
    if (r != DecodeResult::Ok) return r;

    r = ReadU32(data, size, pos, out.featureFlags);
    if (r != DecodeResult::Ok) return r;

    return DecodeResult::Ok;
}

DecodeResult ReplayReader::PeekRecordType(RecordType &out) const {
    if (pos >= size) return DecodeResult::Truncated;
    uint8_t v = data[pos];
    if (!IsValidRecordType(v)) return DecodeResult::BadRecordType;
    out = static_cast<RecordType>(v);
    return DecodeResult::Ok;
}

DecodeResult ReplayReader::ReadRecordFrame(RecordType expected, size_t &payloadStart, uint32_t &payloadLength) {
    if (pos >= size) return DecodeResult::Truncated;
    uint8_t typeByte = data[pos];
    if (!IsValidRecordType(typeByte)) return DecodeResult::BadRecordType;
    if (static_cast<RecordType>(typeByte) != expected) return DecodeResult::BadRecordType;
    pos += 1;

    uint32_t length = 0;
    DecodeResult r = ReadU32(data, size, pos, length);
    if (r != DecodeResult::Ok) return r;

    // Both checks -- against the max-payload cap and against the actual
    // remaining buffer length -- happen here, before any field of the
    // payload is read or any buffer is sized from `length`.
    if (length > kMaxPayloadLength) return DecodeResult::RecordTooLarge;
    if (size - pos < length) return DecodeResult::Truncated;

    payloadStart = pos;
    payloadLength = length;
    return DecodeResult::Ok;
}

DecodeResult ReplayReader::ReadRoundStart(RoundStartRecord &out) {
    size_t payloadStart = 0;
    uint32_t payloadLength = 0;
    DecodeResult r = ReadRecordFrame(RecordType::RoundStart, payloadStart, payloadLength);
    if (r != DecodeResult::Ok) return r;

    r = ReadU32(data, size, pos, out.roundId); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.playerCount); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.gameMode); if (r != DecodeResult::Ok) return r;
    r = ReadU32(data, size, pos, out.victoriesLimit); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.networkGame); if (r != DecodeResult::Ok) return r;
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadU32(data, size, pos, out.seatIds[i]); if (r != DecodeResult::Ok) return r; }
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadU8(data, size, pos, out.seatOwned[i]); if (r != DecodeResult::Ok) return r; }
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadU8(data, size, pos, out.seatTeam[i]); if (r != DecodeResult::Ok) return r; }
    r = ReadU64(data, size, pos, out.levelHash); if (r != DecodeResult::Ok) return r;
    r = ReadBytes(data, size, pos, out.levelLayout); if (r != DecodeResult::Ok) return r;
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadBytes(data, size, pos, out.startingBoards[i]); if (r != DecodeResult::Ok) return r; }
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadU8(data, size, pos, out.currentColor[i]); if (r != DecodeResult::Ok) return r; }
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadU8(data, size, pos, out.nextColor[i]); if (r != DecodeResult::Ok) return r; }
    r = ReadU32(data, size, pos, out.gameplayRngState); if (r != DecodeResult::Ok) return r;
    r = ReadI32(data, size, pos, out.boardGeometryId); if (r != DecodeResult::Ok) return r;
    r = ReadI32(data, size, pos, out.initialSimStep); if (r != DecodeResult::Ok) return r;
    r = ReadFloat(data, size, pos, out.initialStepDeltaScale); if (r != DecodeResult::Ok) return r;
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadI32(data, size, pos, out.startingScore[i]); if (r != DecodeResult::Ok) return r; }
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadI32(data, size, pos, out.startingWins[i]); if (r != DecodeResult::Ok) return r; }

    return DecodeResult::Ok;
}

DecodeResult ReplayReader::ReadStep(StepRecord &out) {
    size_t payloadStart = 0;
    uint32_t payloadLength = 0;
    DecodeResult r = ReadRecordFrame(RecordType::Step, payloadStart, payloadLength);
    if (r != DecodeResult::Ok) return r;

    r = ReadI32(data, size, pos, out.simStep); if (r != DecodeResult::Ok) return r;
    r = ReadFloat(data, size, pos, out.deltaScale); if (r != DecodeResult::Ok) return r;
    r = ReadU32(data, size, pos, out.gameClockMs); if (r != DecodeResult::Ok) return r;
    r = ReadU32(data, size, pos, out.seatId); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.left); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.right); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.center); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.fire); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.firedByMouse); if (r != DecodeResult::Ok) return r;
    r = ReadFloat(data, size, pos, out.mouseAngle); if (r != DecodeResult::Ok) return r;
    r = ReadBytes(data, size, pos, out.inboundEvents); if (r != DecodeResult::Ok) return r;

    return DecodeResult::Ok;
}

DecodeResult ReplayReader::ReadAssertion(AssertionRecord &out) {
    size_t payloadStart = 0;
    uint32_t payloadLength = 0;
    DecodeResult r = ReadRecordFrame(RecordType::Assertion, payloadStart, payloadLength);
    if (r != DecodeResult::Ok) return r;

    r = ReadI32(data, size, pos, out.simStep); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.seatId); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.acceptedShotColor); if (r != DecodeResult::Ok) return r;
    r = ReadI32(data, size, pos, out.acceptedColumn); if (r != DecodeResult::Ok) return r;
    r = ReadI32(data, size, pos, out.acceptedRow); if (r != DecodeResult::Ok) return r;
    r = ReadU64(data, size, pos, out.canonicalStateHash); if (r != DecodeResult::Ok) return r;

    return DecodeResult::Ok;
}

DecodeResult ReplayReader::ReadRoundEnd(RoundEndRecord &out) {
    size_t payloadStart = 0;
    uint32_t payloadLength = 0;
    DecodeResult r = ReadRecordFrame(RecordType::RoundEnd, payloadStart, payloadLength);
    if (r != DecodeResult::Ok) return r;

    r = ReadU8(data, size, pos, out.outcome); if (r != DecodeResult::Ok) return r;
    r = ReadU32(data, size, pos, out.winningSeatId); if (r != DecodeResult::Ok) return r;
    r = ReadU64(data, size, pos, out.finalStateHash); if (r != DecodeResult::Ok) return r;
    r = ReadU32(data, size, pos, out.durationMs); if (r != DecodeResult::Ok) return r;
    r = ReadU8(data, size, pos, out.complete); if (r != DecodeResult::Ok) return r;
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadI32(data, size, pos, out.finalScore[i]); if (r != DecodeResult::Ok) return r; }
    for (int i = 0; i < kMaxPlayers; ++i) { r = ReadI32(data, size, pos, out.finalWins[i]); if (r != DecodeResult::Ok) return r; }

    return DecodeResult::Ok;
}

// ---------------------------------------------------------------------------
// Canonical hash
// ---------------------------------------------------------------------------

uint64_t CanonicalHashFnv1a64(const uint8_t *data, size_t size) {
    constexpr uint64_t kOffsetBasis = 14695981039346656037ull;
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t hash = kOffsetBasis;
    for (size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= kPrime;
    }
    return hash;
}
