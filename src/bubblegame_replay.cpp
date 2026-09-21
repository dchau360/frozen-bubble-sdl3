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

#include "bubblegame_replay.h"

#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "attackmode.h"
#include "gamemode.h"

#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Friend accessor
// ---------------------------------------------------------------------------
// BubbleGame keeps its simulation state private; BubbleGameReplayAccess is the
// one type BubbleGame befriends (bubblegame.h) so this module can read and
// restore it without exposing any of it publicly. A normal test accessor
// cannot be used here: this file compiles into the shipping game too.
struct BubbleGameReplayAccess {
    static const BubbleArray &player(const BubbleGame &g, int idx) { return g.bubbleArrays[idx]; }
    static BubbleArray &player(BubbleGame &g, int idx) { return g.bubbleArrays[idx]; }
    static const SetupSettings &settings(const BubbleGame &g) { return g.currentSettings; }
    static void setSettings(BubbleGame &g, const SetupSettings &s) { g.currentSettings = s; }
    static uint32_t rngState(const BubbleGame &g) { return g.rng.State(); }
    static void seedRng(BubbleGame &g, uint32_t seed) {
        g.rng.Seed(seed);
        g.rngExplicitlySeeded = true;
    }
    static int simStep(const BubbleGame &g) { return g.simStep; }
    static void setSimStep(BubbleGame &g, int v) { g.simStep = v; }
    static float stepDeltaScale(const BubbleGame &g) { return g.stepDeltaScale; }
    static void setStepDeltaScale(BubbleGame &g, float v) { g.stepDeltaScale = v; }
    static uint32_t stepGameClockMs(const BubbleGame &g) { return g.stepGameClockMs; }
    static int mpTrainScore(const BubbleGame &g) { return g.mpTrainScore; }
    static bool mpTrainDone(const BubbleGame &g) { return g.mpTrainDone; }
    static bool modeTimerExpired(const BubbleGame &g) { return g.modeTimerExpired; }
    static bool wasNetworkLeader(const BubbleGame &g) { return g.wasNetworkLeader; }
    static void setWasNetworkLeader(BubbleGame &g, bool v) { g.wasNetworkLeader = v; }
    static bool gameFinish(const BubbleGame &g) { return g.gameFinish; }
    static bool gameWon(const BubbleGame &g) { return g.gameWon; }
    static bool gameLost(const BubbleGame &g) { return g.gameLost; }
    static bool wonByClearing(const BubbleGame &g) { return g.wonByClearing; }
    static bool gameMatchOver(const BubbleGame &g) { return g.gameMatchOver; }
    static int roundWinnerIdx(const BubbleGame &g) { return g.roundWinnerIdx; }
    static void setSessionMode(BubbleGame &g, BubbleGame::SessionMode mode) { g.sessionMode = mode; }
    static const std::vector<BubbleGame::InboundGameEvent> &
    stepInboundEvents(const BubbleGame &g) { return g.stepInboundEvents; }
    static void setStepInboundEvents(BubbleGame &g,
                                     std::vector<BubbleGame::InboundGameEvent> events) {
        g.stepInboundEvents = std::move(events);
    }
};

namespace {

// Board blob format version. Bumped from 1 to 2 when the per-row horizontal
// offset and per-board bubble size were stored explicitly (see
// docs/REPLAY_PROGRESS.md's R5a entry); bumped 2 to 3 by R6a, which appends a
// length-prefixed per-seat nickname so a network replay can reconstruct the
// lobbyPlayerId -> nickname mapping ApplyInboundGameMessage()'s 'g'/'F'
// handlers resolve senders and targets with.
constexpr uint8_t kBoardBlobVersion = 3;

// Cell flag bits in the board blob.
constexpr uint8_t kCellFlagPlayerBubble = 0x01;
constexpr uint8_t kCellFlagFrozen = 0x02;

// Board blob settings flag bits. chainReaction/randomLevels are game-wide but
// duplicated in every seat's blob (cheap, and each blob stays self-describing);
// the rest are genuinely per-seat.
constexpr uint8_t kBoardFlagChainReaction = 0x01;
constexpr uint8_t kBoardFlagRandomLevels = 0x02;
constexpr uint8_t kBoardFlagIsBot = 0x04;
constexpr uint8_t kBoardFlagCompressionDisabled = 0x08;
constexpr uint8_t kBoardFlagAimGuide = 0x10;

// The small "game rules" blob stored in RoundStartRecord::levelLayout. R2's
// envelope has no dedicated field for attackMode/raceTarget/timedSeconds (a
// genuine schema gap, noted in the progress entry), so they ride in the
// opaque levelLayout vector rather than forcing a codec change. mpTraining is
// recorded so a training recording is restored in the same mode; its clock is
// reproduced by the stepGameClockMs seam (R5b), whose timer is
// self-initializing on the first simulated step, so no start value is stored.
constexpr uint8_t kRulesBlobVersion = 1;
constexpr uint8_t kRulesFlagMpTraining = 0x01;
constexpr uint8_t kRulesFlagLocalMultiplayer = 0x02;
constexpr uint8_t kRulesFlagMouseEnabled = 0x04;
// R6b: "this client was the network leader at capture time". A round-level
// fact like attackMode/timedSeconds above, so it rides the rules blob rather
// than being duplicated across every seat's board blob. This is a new bit in
// an existing version's flags byte (the same way the three bits above were
// added), not a format change -- an old decoder ignores the bit (reading
// false) and a new decoder reading an old blob gets false too, so no version
// bump is needed.
constexpr uint8_t kRulesFlagWasNetworkLeader = 0x08;

// Seat-count bounds. A local (non-network) round is capped at the engine's
// local ceiling, kMaxLocalPlayers (src/localmultiplayer_settings.h). A network
// battle-royale room simulates up to MAX_NET_PLAYERS boards (kMaxPlayers in
// replay_format.h, a by-hand copy of it), so a network record is allowed that
// many. Both bounds are applied before any board blob is decoded; the split
// matters because SeatCount() below decides how many seats are captured and
// hashed, so a single shared local cap would silently truncate a wide network
// record to its first five seats (R6c).
constexpr int kMaxLocalSeats = 5;
constexpr int kMaxNetworkSeats = kMaxPlayers;

// ---------------------------------------------------------------------------
// Little-endian append helpers (mirror src/replay_format.cpp's style: explicit
// widths, never a memcpy'd struct, never host byte order).
// ---------------------------------------------------------------------------

void AppendU8(std::vector<uint8_t> &buf, uint8_t v) { buf.push_back(v); }

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

void AppendI32(std::vector<uint8_t> &buf, int32_t v) {
    AppendU32(buf, static_cast<uint32_t>(v));
}

void AppendFloat(std::vector<uint8_t> &buf, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    AppendU32(buf, bits);
}

// ---------------------------------------------------------------------------
// Bounds-checked reader for the board blob. Never reads past `size`; every
// read returns false on a short buffer so a corrupt/truncated blob leaves the
// caller (and the game) untouched rather than reading out of bounds.
// ---------------------------------------------------------------------------

struct BlobReader {
    const uint8_t *data = nullptr;
    size_t size = 0;
    size_t pos = 0;

    bool ReadU8(uint8_t &v) {
        if (size - pos < 1) return false;
        v = data[pos++];
        return true;
    }
    bool ReadU16(uint16_t &v) {
        if (size - pos < 2) return false;
        v = static_cast<uint16_t>(data[pos]) | (static_cast<uint16_t>(data[pos + 1]) << 8);
        pos += 2;
        return true;
    }
    bool ReadI32(int32_t &v) {
        if (size - pos < 4) return false;
        uint32_t u = static_cast<uint32_t>(data[pos]) |
                     (static_cast<uint32_t>(data[pos + 1]) << 8) |
                     (static_cast<uint32_t>(data[pos + 2]) << 16) |
                     (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        v = static_cast<int32_t>(u);
        return true;
    }
};

struct DecodedBoard {
    bool chainReaction = false;
    bool randomLevels = false;
    bool isBot = false;
    bool compressionDisabled = false;
    bool aimGuide = false;
    int numColors = 8;
    int numRows = 0;
    // Pixel pitch of one bubble column, and thus of one row
    // (rowSize = bubbleSize * 7 / 8). 32 for a full-size board, 16 for the
    // mini boards local/network seats 1+ use in 3-5 player games.
    int bubbleSize = 32;
    int offsetX = 0, offsetY = 0;
    int leftLimit = 0, rightLimit = 0, topLimit = 0;
    int turnsToCompress = 9, dangerZone = 12, numSeparators = 0;
    struct Cell {
        int bubbleId = -1;
        bool playerBubble = false;
        bool frozen = false;
    };
    struct Row {
        int smallerSep = 0;  // horizontal offset of column 0, before bubbleOffset
        std::vector<Cell> cells;
    };
    std::vector<Row> rows;
    std::vector<int> nextColors;
    std::vector<int> malusQueue;
    std::string nickname;  // v3: lobby nickname for this seat (network replay)
};

// Rebuilds the exact starting board as a self-describing blob. Only the
// fields that cannot be regenerated from the round's settings/geometry are
// stored: the per-board bubble size, each row's actual horizontal offset and
// length, each cell's bubbleId + playerBubble/frozen flags in vector order,
// the nextColors queue and the queued malus timestamps. Cell positions are NOT
// stored (they are derived from row/col + the stored offset/size), but the
// offset IS stored per row: LoadLevel()'s filler rows derive it from the row
// index, not from the row's cell-count parity, so re-deriving it on restore
// would place those rows wrong.
std::vector<uint8_t> EncodeBoardBlob(const BubbleGame &game, int seat) {
    const BubbleArray &p = BubbleGameReplayAccess::player(game, seat);
    const SetupSettings &st = BubbleGameReplayAccess::settings(game);

    const bool isMini = (st.playerCount >= 3 && p.playerAssigned >= 1);
    const int bubbleSize = isMini ? 16 : 32;

    std::vector<uint8_t> buf;
    AppendU8(buf, kBoardBlobVersion);
    uint8_t flags = 0;
    if (st.chainReaction) flags |= kBoardFlagChainReaction;
    if (st.randomLevels) flags |= kBoardFlagRandomLevels;
    if (p.isBot) flags |= kBoardFlagIsBot;
    if (p.compressionDisabled) flags |= kBoardFlagCompressionDisabled;
    if (p.aimGuideEnabled) flags |= kBoardFlagAimGuide;
    AppendU8(buf, flags);
    AppendU8(buf, static_cast<uint8_t>(p.numColors));
    AppendU8(buf, static_cast<uint8_t>(p.bubbleMap.size()));
    AppendU8(buf, static_cast<uint8_t>(bubbleSize));
    AppendI32(buf, p.bubbleOffset.x);
    AppendI32(buf, p.bubbleOffset.y);
    AppendI32(buf, p.leftLimit);
    AppendI32(buf, p.rightLimit);
    AppendI32(buf, p.topLimit);
    AppendI32(buf, p.turnsToCompress);
    AppendI32(buf, p.dangerZone);
    AppendI32(buf, p.numSeparators);
    for (const auto &row : p.bubbleMap) {
        AppendU16(buf, static_cast<uint16_t>(row.size()));
        // The live horizontal offset is column 0's x relative to bubbleOffset.
        // Storing it (rather than re-deriving it from cells % 2) is the whole
        // point of v2 -- LoadLevel's filler rows invert that relationship.
        const int smallerSep = row.empty() ? 0 : (row[0].pos.x - p.bubbleOffset.x);
        AppendU8(buf, static_cast<uint8_t>(smallerSep));
        for (const Bubble &b : row) {
            AppendI32(buf, b.bubbleId);
            uint8_t cellFlags = 0;
            if (b.playerBubble) cellFlags |= kCellFlagPlayerBubble;
            if (b.frozen) cellFlags |= kCellFlagFrozen;
            AppendU8(buf, cellFlags);
        }
    }
    AppendU16(buf, static_cast<uint16_t>(p.nextColors.size()));
    for (int c : p.nextColors) AppendU8(buf, static_cast<uint8_t>(c));
    AppendU16(buf, static_cast<uint16_t>(p.malusQueue.size()));
    for (int m : p.malusQueue) AppendI32(buf, m);
    // v3: the seat's nickname, length-prefixed. A u16 cap keeps a hostile blob
    // from driving a large allocation on decode; real nicks are <= 10 chars.
    AppendU16(buf, static_cast<uint16_t>(p.playerNickname.size()));
    buf.insert(buf.end(), p.playerNickname.begin(), p.playerNickname.end());
    return buf;
}

bool DecodeBoardBlob(const std::vector<uint8_t> &blob, DecodedBoard &out) {
    BlobReader r{blob.data(), blob.size(), 0};
    uint8_t version = 0;
    if (!r.ReadU8(version) || version != kBoardBlobVersion) return false;
    uint8_t flags = 0;
    if (!r.ReadU8(flags)) return false;
    out.chainReaction = (flags & kBoardFlagChainReaction) != 0;
    out.randomLevels = (flags & kBoardFlagRandomLevels) != 0;
    out.isBot = (flags & kBoardFlagIsBot) != 0;
    out.compressionDisabled = (flags & kBoardFlagCompressionDisabled) != 0;
    out.aimGuide = (flags & kBoardFlagAimGuide) != 0;

    uint8_t numColors = 0, numRows = 0, bubbleSize = 0;
    if (!r.ReadU8(numColors) || !r.ReadU8(numRows) || !r.ReadU8(bubbleSize)) return false;
    // Sanity-clamp to the fixed board height so a hostile row count cannot
    // drive an oversized allocation or an out-of-range write below. The bubble
    // size only ever has two legal values.
    if (numRows > 13) return false;
    if (bubbleSize != 16 && bubbleSize != 32) return false;
    out.numColors = numColors;
    out.numRows = numRows;
    out.bubbleSize = bubbleSize;

    int32_t i32 = 0;
    if (!r.ReadI32(i32)) return false; out.offsetX = i32;
    if (!r.ReadI32(i32)) return false; out.offsetY = i32;
    if (!r.ReadI32(i32)) return false; out.leftLimit = i32;
    if (!r.ReadI32(i32)) return false; out.rightLimit = i32;
    if (!r.ReadI32(i32)) return false; out.topLimit = i32;
    if (!r.ReadI32(i32)) return false; out.turnsToCompress = i32;
    if (!r.ReadI32(i32)) return false; out.dangerZone = i32;
    if (!r.ReadI32(i32)) return false; out.numSeparators = i32;

    out.rows.resize(numRows);
    for (int row = 0; row < numRows; ++row) {
        uint16_t count = 0;
        if (!r.ReadU16(count)) return false;
        if (count > 64) return false;  // a real row is at most 8 cells
        uint8_t smallerSep = 0;
        if (!r.ReadU8(smallerSep)) return false;
        out.rows[row].smallerSep = smallerSep;
        out.rows[row].cells.resize(count);
        for (uint16_t col = 0; col < count; ++col) {
            DecodedBoard::Cell cell;
            if (!r.ReadI32(i32)) return false;
            cell.bubbleId = i32;
            uint8_t cellFlags = 0;
            if (!r.ReadU8(cellFlags)) return false;
            cell.playerBubble = (cellFlags & kCellFlagPlayerBubble) != 0;
            cell.frozen = (cellFlags & kCellFlagFrozen) != 0;
            out.rows[row].cells[col] = cell;
        }
    }

    uint16_t nextCount = 0;
    if (!r.ReadU16(nextCount)) return false;
    if (nextCount > 64) return false;
    out.nextColors.resize(nextCount);
    for (uint16_t i = 0; i < nextCount; ++i) {
        uint8_t c = 0;
        if (!r.ReadU8(c)) return false;
        out.nextColors[i] = c;
    }

    uint16_t malusCount = 0;
    if (!r.ReadU16(malusCount)) return false;
    if (malusCount > 4096) return false;
    out.malusQueue.resize(malusCount);
    for (uint16_t i = 0; i < malusCount; ++i) {
        if (!r.ReadI32(i32)) return false;
        out.malusQueue[i] = i32;
    }

    // v3 nickname. Cap at 256 so a hostile length cannot drive an oversized
    // allocation; the bytes are read through the bounds-checked BlobReader.
    uint16_t nickLen = 0;
    if (!r.ReadU16(nickLen)) return false;
    if (nickLen > 256) return false;
    if (r.size - r.pos < nickLen) return false;
    out.nickname.assign(reinterpret_cast<const char *>(r.data + r.pos), nickLen);
    r.pos += nickLen;
    return true;
}

struct DecodedRules {
    bool mpTraining = false;
    bool localMultiplayer = false;
    bool mouseEnabled = false;
    bool wasNetworkLeader = false;
    int attackMode = static_cast<int>(AttackMode::On);
    int raceTarget = kRaceTargetDefault;
    int timedSeconds = kTimedSecondsDefault;
    int botSkill = 1;
};

std::vector<uint8_t> EncodeRulesBlob(const SetupSettings &st, bool wasNetworkLeader) {
    std::vector<uint8_t> buf;
    AppendU8(buf, kRulesBlobVersion);
    uint8_t flags = 0;
    if (st.mpTraining) flags |= kRulesFlagMpTraining;
    if (st.localMultiplayer) flags |= kRulesFlagLocalMultiplayer;
    if (st.mouseEnabled) flags |= kRulesFlagMouseEnabled;
    if (wasNetworkLeader) flags |= kRulesFlagWasNetworkLeader;
    AppendU8(buf, flags);
    AppendU8(buf, static_cast<uint8_t>(st.attackMode));
    AppendI32(buf, st.raceTarget);
    AppendI32(buf, st.timedSeconds);
    AppendU8(buf, static_cast<uint8_t>(st.botSkill));
    return buf;
}

bool DecodeRulesBlob(const std::vector<uint8_t> &blob, DecodedRules &out) {
    BlobReader r{blob.data(), blob.size(), 0};
    uint8_t version = 0;
    if (!r.ReadU8(version) || version != kRulesBlobVersion) return false;
    uint8_t flags = 0;
    if (!r.ReadU8(flags)) return false;
    out.mpTraining = (flags & kRulesFlagMpTraining) != 0;
    out.localMultiplayer = (flags & kRulesFlagLocalMultiplayer) != 0;
    out.mouseEnabled = (flags & kRulesFlagMouseEnabled) != 0;
    out.wasNetworkLeader = (flags & kRulesFlagWasNetworkLeader) != 0;
    uint8_t attackMode = 0;
    if (!r.ReadU8(attackMode)) return false;
    out.attackMode = attackMode;
    int32_t i32 = 0;
    if (!r.ReadI32(i32)) return false; out.raceTarget = i32;
    if (!r.ReadI32(i32)) return false; out.timedSeconds = i32;
    uint8_t botSkill = 0;
    if (!r.ReadU8(botSkill)) return false;
    out.botSkill = botSkill;
    return true;
}

// R6a inbound-events blob layout: u16 event count, then per event an i32
// senderId and a u16 length-prefixed gameData. Decoding is bounds-checked and
// capped so a corrupt/hostile blob cannot drive an oversized allocation or
// read out of bounds.
constexpr uint16_t kMaxInboundEvents = 4096;
constexpr uint16_t kMaxInboundEventLen = 4096;

bool DecodeInboundEvents(const std::vector<uint8_t> &blob,
                         std::vector<BubbleGame::InboundGameEvent> &out) {
    BlobReader r{blob.data(), blob.size(), 0};
    uint16_t count = 0;
    if (!r.ReadU16(count)) return false;
    if (count > kMaxInboundEvents) return false;
    out.clear();
    out.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        int32_t senderId = 0;
        if (!r.ReadI32(senderId)) return false;
        uint16_t len = 0;
        if (!r.ReadU16(len)) return false;
        if (len > kMaxInboundEventLen) return false;
        if (r.size - r.pos < len) return false;
        BubbleGame::InboundGameEvent event;
        event.senderId = senderId;
        event.gameData.assign(reinterpret_cast<const char *>(r.data + r.pos), len);
        r.pos += len;
        out.push_back(std::move(event));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Canonical-state serialization
// ---------------------------------------------------------------------------

void AppendBoardState(std::vector<uint8_t> &buf, const BubbleArray &p) {
    AppendU8(buf, static_cast<uint8_t>(p.bubbleMap.size()));
    for (const auto &row : p.bubbleMap) {
        AppendU16(buf, static_cast<uint16_t>(row.size()));
        for (const Bubble &b : row) {
            AppendI32(buf, b.bubbleId);
            // pos is simulation-relevant (SingleBubble::IsCollision reads the
            // grid bubble's pos), not a draw-only cache. Hashing it is what
            // catches a bad board-position reconstruction (the LoadLevel
            // filler-row bug this package fixes) rather than waiting for the
            // divergence to reach board contents many steps later.
            AppendI32(buf, b.pos.x);
            AppendI32(buf, b.pos.y);
            uint8_t f = 0;
            if (b.playerBubble) f |= kCellFlagPlayerBubble;
            if (b.frozen) f |= kCellFlagFrozen;
            AppendU8(buf, f);
        }
    }
    AppendU16(buf, static_cast<uint16_t>(p.nextColors.size()));
    for (int c : p.nextColors) AppendU8(buf, static_cast<uint8_t>(c));

    AppendI32(buf, p.curLaunch);
    AppendI32(buf, p.nextBubble);
    AppendI32(buf, p.score);
    AppendI32(buf, p.chainLevel);
    AppendU8(buf, static_cast<uint8_t>(p.playerState));
    AppendU8(buf, p.newShoot ? 1 : 0);
    AppendU8(buf, p.mpFirePending ? 1 : 0);
    AppendU8(buf, p.mpStickPending ? 1 : 0);
    AppendFloat(buf, p.pendingAngle);
    AppendI32(buf, p.stickCx);
    AppendI32(buf, p.stickCy);
    AppendI32(buf, p.stickCol);
    AppendFloat(buf, p.shooterSprite.angle);

    AppendI32(buf, p.turnsToCompress);
    AppendI32(buf, p.dangerZone);
    AppendI32(buf, p.numSeparators);
    AppendI32(buf, p.bubbleOffset.x);
    AppendI32(buf, p.bubbleOffset.y);
    AppendI32(buf, p.leftLimit);
    AppendI32(buf, p.rightLimit);
    AppendI32(buf, p.topLimit);

    AppendI32(buf, p.waitPrelight);
    AppendI32(buf, p.prelightTime);
    AppendI32(buf, p.framePrelight);
    AppendI32(buf, p.alertColumn);
    AppendI32(buf, p.explodeWait);
    AppendI32(buf, p.frozenWait);

    AppendU16(buf, static_cast<uint16_t>(p.malusQueue.size()));
    for (int m : p.malusQueue) AppendI32(buf, m);

    AppendI32(buf, p.rFired);
    AppendI32(buf, p.rPopped);
    AppendI32(buf, p.rSent);
    AppendI32(buf, p.rRecv);
    AppendI32(buf, p.rKills);
    AppendI32(buf, p.rBlk);          // AttackMode::Canceling blocks (R5a)
    AppendI32(buf, p.lastAttackerIdx);
    AppendI32(buf, p.winCount);
    AppendU8(buf, p.mpWinner ? 1 : 0);
    AppendU8(buf, p.mpDone ? 1 : 0);

    // Presentation animation timers, but they are advanced by the same step
    // code in both live and replay, so including them makes a divergence
    // visible rather than hiding it.
    AppendU8(buf, p.stickAnimActive ? 1 : 0);
    AppendI32(buf, p.stickAnimFrame);
    AppendI32(buf, p.stickAnimSlowdown);
}

void AppendProjectiles(std::vector<uint8_t> &buf) {
    AppendU32(buf, static_cast<uint32_t>(singleBubbles.size()));
    for (const SingleBubble &s : singleBubbles) {
        AppendI32(buf, s.assignedArray);
        AppendI32(buf, s.bubbleId);
        AppendFloat(buf, s.posX);
        AppendFloat(buf, s.posY);
        AppendFloat(buf, s.oldPosX);
        AppendFloat(buf, s.oldPosY);
        AppendI32(buf, s.pos.x);
        AppendI32(buf, s.pos.y);
        AppendI32(buf, s.oldpos.x);
        AppendI32(buf, s.oldpos.y);
        AppendFloat(buf, s.direction);
        uint8_t flags = 0;
        if (s.falling) flags |= 0x01;
        if (s.launching) flags |= 0x02;
        if (s.exploding) flags |= 0x04;
        if (s.shouldClear) flags |= 0x08;
        AppendU8(buf, flags);
        AppendI32(buf, s.bubbleSize);
        uint8_t chainFlags = 0;
        if (s.chainExists) chainFlags |= 0x01;
        if (s.chainReachedDest) chainFlags |= 0x02;
        if (s.chainGoingUp) chainFlags |= 0x04;
        AppendU8(buf, chainFlags);
        AppendI32(buf, s.chainRow);
        AppendI32(buf, s.chainCol);
        AppendI32(buf, s.chainDest.x);
        AppendI32(buf, s.chainDest.y);
        AppendFloat(buf, s.speedX);
        AppendFloat(buf, s.speedY);
        AppendFloat(buf, s.genSpeed);
        AppendI32(buf, s.waitForFall);
        AppendI32(buf, s.leftLimit);
        AppendI32(buf, s.rightLimit);
        AppendI32(buf, s.topLimit);
    }
    AppendU32(buf, static_cast<uint32_t>(malusBubbles.size()));
    for (const MalusBubble &m : malusBubbles) {
        AppendI32(buf, m.assignedArray);
        AppendI32(buf, m.bubbleId);
        AppendI32(buf, m.cx);
        AppendI32(buf, m.cy);
        AppendI32(buf, m.stickY);
        AppendFloat(buf, m.posX);
        AppendFloat(buf, m.posY);
        AppendI32(buf, m.pos.x);
        AppendI32(buf, m.pos.y);
        uint8_t flags = 0;
        if (m.shouldStick) flags |= 0x01;
        if (m.shouldClear) flags |= 0x02;
        AppendU8(buf, flags);
    }
}

int SeatCount(const BubbleGame &game) {
    const SetupSettings &st = BubbleGameReplayAccess::settings(game);
    int n = st.playerCount;
    // A network battle-royale room genuinely simulates up to MAX_NET_PLAYERS
    // boards (NewGame()/ReloadGame()'s default case builds them all), so
    // capture/hash all of them; a local round keeps the engine's 5-seat
    // ceiling. See kMaxLocalSeats above for why one shared cap would be a bug.
    const int cap = st.networkGame ? kMaxNetworkSeats : kMaxLocalSeats;
    if (n < 1) n = 1;
    if (n > cap) n = cap;
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

RoundStartRecord CaptureRoundStart(const BubbleGame &game) {
    const SetupSettings &st = BubbleGameReplayAccess::settings(game);
    const int players = SeatCount(game);

    RoundStartRecord r;
    r.roundId = 0;
    r.playerCount = static_cast<uint8_t>(players);
    r.gameMode = static_cast<uint8_t>(st.gameMode);
    r.victoriesLimit = static_cast<uint32_t>(st.victoriesLimit);
    r.networkGame = st.networkGame ? 1 : 0;

    std::vector<uint8_t> levelHashInput;
    for (int i = 0; i < players; ++i) {
        const BubbleArray &p = BubbleGameReplayAccess::player(game, i);
        // Stable per-seat identity: the lobby player id for a network round
        // (ApplyInboundGameMessage() resolves a message's sender/target by
        // matching bubbleArrays[i].lobbyPlayerId), or the array index for a
        // local round, whose seats have no lobby id.
        r.seatIds[i] = static_cast<uint32_t>(p.lobbyPlayerId >= 0 ? p.lobbyPlayerId : i);
        // Ownership is exactly what OwnsArray() reports live (every local seat
        // in a non-network game; the local player and hosted bots in a network
        // game). R6a's replay gate accepts only seat 0 owned / seat 1 remote.
        r.seatOwned[i] = game.OwnsArray(p) ? 1 : 0;
        r.seatTeam[i] = static_cast<uint8_t>(st.playerTeams[i]);

        r.startingBoards[i] = EncodeBoardBlob(game, i);
        r.currentColor[i] = static_cast<uint8_t>(p.curLaunch);
        r.nextColor[i] = static_cast<uint8_t>(p.nextBubble);
        r.startingScore[i] = p.score;
        r.startingWins[i] = p.winCount;

        levelHashInput.insert(levelHashInput.end(),
                              r.startingBoards[i].begin(), r.startingBoards[i].end());
    }

    // R6b: record whether this client was the network leader when the round
    // started. UpdateTimedRound()'s leader/joiner verdict asymmetry reads it
    // during Playback, where NetworkClient has no room configured. A local
    // record has no leader concept, so it stays false there.
    bool wasLeader = false;
    if (st.networkGame) {
        NetworkClient *netClient = NetworkClient::Instance();
        wasLeader = netClient && netClient->IsLeader();
    }
    r.levelLayout = EncodeRulesBlob(st, wasLeader);
    levelHashInput.insert(levelHashInput.end(), r.levelLayout.begin(), r.levelLayout.end());
    r.levelHash = CanonicalHashFnv1a64(levelHashInput);

    r.gameplayRngState = BubbleGameReplayAccess::rngState(game);
    // Nonzero distinguishes a recorded full-size local board from an unset
    // record; a later package that adds a second geometry kind must widen it.
    r.boardGeometryId = 1;
    r.initialSimStep = BubbleGameReplayAccess::simStep(game);
    r.initialStepDeltaScale = BubbleGameReplayAccess::stepDeltaScale(game);
    return r;
}

void RestoreRoundStart(BubbleGame &game, const RoundStartRecord &record) {
    // Reject anything this slice has not validated, before touching a single
    // byte of the instance: 1-5 locally-simulated seats, no network; or an
    // R6b/R6c network round of 2-20 seats. Seat 0 is this client's own board,
    // and every other seat is either a hosted bot this client simulates (owned
    // and marked isBot in its board blob) or a genuine remote peer (unowned and
    // not a bot). A locally-resolved mode (Classic, Race or Timed) is required
    // for both. Timed/training rounds end on the recorded stepGameClockMs seam
    // (R5b), not the wall clock; Clear is not exercised here yet. Matching
    // R3's malformed-input posture, an unsupported record is logged and the
    // instance is left exactly as it was.
    const bool networkRecord = record.networkGame != 0;
    // Coarse seat count first, before any board is touched: a local record has
    // 1-5 seats, a network record 2-20 (R6c widened the network bound from
    // R6b's 2-5 to the engine's real battle-royale ceiling). The per-seat
    // ownership/isBot rule that needs decoded boards is applied below, after
    // the decode loop, so a record failing this range check never attempts to
    // decode a board.
    if (networkRecord) {
        if (record.playerCount < 2 || record.playerCount > kMaxNetworkSeats) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "RestoreRoundStart: unsupported network record (playerCount=%u); network needs 2-%d seats",
                         static_cast<unsigned>(record.playerCount), kMaxNetworkSeats);
            return;
        }
    } else if (record.playerCount < 1 || record.playerCount > kMaxLocalSeats) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "RestoreRoundStart: unsupported record (playerCount=%u networkGame=%u); local 1-%d seats only",
                     static_cast<unsigned>(record.playerCount),
                     static_cast<unsigned>(record.networkGame),
                     kMaxLocalSeats);
        return;
    }
    if (record.gameMode != static_cast<uint8_t>(GameMode::Classic) &&
        record.gameMode != static_cast<uint8_t>(GameMode::Race) &&
        record.gameMode != static_cast<uint8_t>(GameMode::Timed)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "RestoreRoundStart: unsupported game mode %u; Classic/Race/Timed only",
                     static_cast<unsigned>(record.gameMode));
        return;
    }

    DecodedRules rules;
    if (!DecodeRulesBlob(record.levelLayout, rules)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "RestoreRoundStart: malformed/missing rules blob (%zu bytes); leaving state",
                     record.levelLayout.size());
        return;
    }

    // Decode every seat's board up front, so a single malformed blob rejects
    // the whole record without half-restoring it (R3's posture).
    const int players = record.playerCount;
    std::vector<DecodedBoard> boards(players);
    for (int i = 0; i < players; ++i) {
        if (!DecodeBoardBlob(record.startingBoards[i], boards[i])) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "RestoreRoundStart: malformed board blob for seat %d (%zu bytes); leaving state",
                         i, record.startingBoards[i].size());
            return;
        }
    }
    // R6b: ownership and bot flags have to agree for every decoded seat.
    // Seat 0 is this client's own board: it must be owned and must not be a
    // hosted bot. Every other seat is either a bot this client hosts (owned
    // and marked isBot) or a genuine remote peer (unowned and not a bot) --
    // an owned non-bot seat would claim we simulate a remote human, and an
    // unowned bot seat would claim our own hosted bot is a stranger. Both are
    // lies about what this client controls, so either rejects the record.
    if (networkRecord) {
        bool bad = record.seatOwned[0] != 1 || boards[0].isBot;
        int badSeat = bad ? 0 : -1;
        for (int i = 1; !bad && i < players; ++i) {
            const bool owned = record.seatOwned[i] != 0;
            if (owned != boards[i].isBot) { bad = true; badSeat = i; }
        }
        if (bad) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "RestoreRoundStart: unsupported network record (seat %d owned=%u bot=%d); "
                         "seat 0 must be owned non-bot and every other seat owned iff a hosted bot",
                         badSeat,
                         static_cast<unsigned>(record.seatOwned[badSeat]),
                         boards[badSeat].isBot ? 1 : 0);
            return;
        }
    }

    // 1) Rebuild the round's scaffolding with NewGame() using the recorded
    //    effective settings. NewGame sets geometry, loads textures, resets
    //    result flags and clears the shared singleBubbles/malusBubbles
    //    globals. Its randomly generated boards are discarded in step 2 -- the
    //    overwrite below is the actual playback initialization, not avoiding
    //    NewGame. No SyncNetworkLevel() runs: networkGame is false.
    SetupSettings setup;
    setup.playerCount = players;
    setup.networkGame = networkRecord;
    setup.gameMode = static_cast<GameMode>(record.gameMode);
    setup.victoriesLimit = static_cast<int>(record.victoriesLimit);
    setup.localMultiplayer = rules.localMultiplayer;
    setup.mouseEnabled = rules.mouseEnabled;
    setup.attackMode = static_cast<AttackMode>(rules.attackMode);
    setup.raceTarget = rules.raceTarget;
    setup.timedSeconds = rules.timedSeconds;
    setup.botSkill = rules.botSkill;
    // A training round is restored in training mode: the two-minute clock and
    // its end condition are reproduced by the recorded stepGameClockMs seam,
    // not read from the wall clock. See docs/REPLAY_PROGRESS.md (R5b).
    setup.mpTraining = rules.mpTraining;
    // chainReaction/randomLevels are game-wide and identical in every seat's
    // blob; take them from seat 0 (validated non-empty by the decode loop).
    setup.chainReaction = boards[0].chainReaction;
    // A network record always rebuilds through LoadLevel(), never
    // SyncNetworkLevel(): playback has no server to sync with and the recorded
    // board blobs are the source of truth. The recorded randomLevels flag is
    // preserved in the blob for informational fidelity but is deliberately not
    // passed to NewGame() for a network record, because NewGame() calls
    // SyncNetworkLevel() unconditionally when randomLevels && networkGame and
    // would QuitToTitle() on failure. The board overwrite below replaces
    // whatever initial board NewGame() produced either way. See
    // docs/REPLAY_PROGRESS.md (R6a/R6b).
    setup.randomLevels = networkRecord ? false : boards[0].randomLevels;
    for (int i = 0; i < players; ++i) {
        setup.playerColors[i] = boards[i].numColors;
        setup.disableCompression[i] = boards[i].compressionDisabled;
        setup.aimGuide[i] = boards[i].aimGuide;
        setup.playerIsBot[i] = boards[i].isBot;
        setup.playerTeams[i] = record.seatTeam[i];
    }

    // Seed before NewGame so its wall-clock reseed is skipped; the recorded
    // RNG state is written back below in any case. Playback mode is entered
    // *before* NewGame so the whole restore runs with external effects off --
    // and, for R4a, so the replay recorder's NewGame hook sees the mode already
    // set and never starts recording a playback. (R4a: this only moves the
    // existing setSessionMode() call earlier; the restored state below is
    // unchanged.)
    BubbleGameReplayAccess::setSessionMode(game, BubbleGame::SessionMode::Playback);
    BubbleGameReplayAccess::seedRng(game, record.gameplayRngState);
    game.NewGame(setup);

    // R6b: restore the recorded leader flag (a round-level rule, so it is set
    // once here rather than per seat). UpdateTimedRound() reads it during
    // Playback; Live ignores it and queries NetworkClient as before.
    BubbleGameReplayAccess::setWasNetworkLeader(game, rules.wasNetworkLeader);

    // 2) Overwrite each generated board and round state with the recording.
    for (int i = 0; i < players; ++i) {
        BubbleArray &p = BubbleGameReplayAccess::player(game, i);
        const DecodedBoard &board = boards[i];
        const int rowSize = board.bubbleSize * 7 / 8;

        for (int row = 0; row < static_cast<int>(p.bubbleMap.size()); ++row) {
            p.bubbleMap[row].clear();
            if (row >= board.numRows) continue;
            const DecodedBoard::Row &src = board.rows[row];
            const int count = static_cast<int>(src.cells.size());
            for (int col = 0; col < count; ++col) {
                Bubble b;
                b.bubbleId = src.cells[col].bubbleId;
                b.playerBubble = src.cells[col].playerBubble;
                b.frozen = src.cells[col].frozen;
                b.pos = {src.smallerSep + board.bubbleSize * col + board.offsetX,
                         rowSize * row + board.offsetY};
                p.bubbleMap[row].push_back(b);
            }
        }

        p.numColors = board.numColors;
        p.bubbleOffset = {board.offsetX, board.offsetY};
        p.leftLimit = board.leftLimit;
        p.rightLimit = board.rightLimit;
        p.topLimit = board.topLimit;
        p.turnsToCompress = board.turnsToCompress;
        p.dangerZone = board.dangerZone;
        p.numSeparators = board.numSeparators;
        p.nextColors = board.nextColors;
        p.malusQueue = board.malusQueue;
        p.curLaunch = record.currentColor[i];
        p.nextBubble = record.nextColor[i];
        p.score = record.startingScore[i];
        p.winCount = record.startingWins[i];
        p.chainLevel = 0;
        p.playerAssigned = i;
        p.playerState = BubbleArray::PlayerState::ALIVE;
        p.newShoot = true;
        p.mpWinner = false;
        p.mpDone = false;
        p.lastControls = PlayerControls{};
        p.compressionDisabled = board.compressionDisabled;
        p.aimGuideEnabled = board.aimGuide;
        p.isBot = board.isBot;
        // Reconstruct the lobby identity ApplyInboundGameMessage() uses to map
        // a message's senderId to a board and a malus/'F' nickname to a target.
        // Network records only: a local round's NewGame() never assigns either
        // field, and R5a/R5b deliberately leave them exactly as produced.
        if (networkRecord) {
            p.lobbyPlayerId = static_cast<int>(record.seatIds[i]);
            p.playerNickname = board.nickname;
        }
    }

    BubbleGameReplayAccess::seedRng(game, record.gameplayRngState);
    BubbleGameReplayAccess::setSimStep(game, record.initialSimStep);
    BubbleGameReplayAccess::setStepDeltaScale(game, record.initialStepDeltaScale);
}

void SetPlaybackInboundEvents(BubbleGame &game, const std::vector<uint8_t> &blob) {
    std::vector<BubbleGame::InboundGameEvent> events;
    if (!DecodeInboundEvents(blob, events)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SetPlaybackInboundEvents: malformed inbound-events blob (%zu bytes); applying nothing",
                     blob.size());
        BubbleGameReplayAccess::setStepInboundEvents(game, {});
        return;
    }
    BubbleGameReplayAccess::setStepInboundEvents(game, std::move(events));
}

void SetPlaybackControls(BubbleGame &game, int seat, const PlayerControls &controls) {
    // The exact private-state write tests/bubblegame_replay_test.cpp used to do
    // through its test-only friend struct, exposed here so the shipping
    // ReplayPlayer can drive a playback without one. Guard the index: Load()
    // validates every decoded seat, but this is the choke point that makes a
    // tampered record memory-safe even if it slips past that.
    if (seat < 0 || seat >= MAX_NET_PLAYERS) return;
    BubbleGameReplayAccess::player(game, seat).lastControls = controls;
}

StepRecord CaptureStep(const BubbleGame &game, int seatId) {
    const BubbleArray &p = BubbleGameReplayAccess::player(game, seatId);
    StepRecord s;
    s.simStep = BubbleGameReplayAccess::simStep(game);
    s.deltaScale = BubbleGameReplayAccess::stepDeltaScale(game);
    // The recorded round-relative game clock (R5b): sampled from the live
    // SDL_GetTicks() once per step, replayed into stepGameClockMs so training's
    // two-minute timer and Timed mode's countdown end at the same step instead
    // of on the viewer's real clock. R3/R5a left this 0.
    s.gameClockMs = BubbleGameReplayAccess::stepGameClockMs(game);
    s.seatId = static_cast<uint32_t>(seatId);
    s.left = p.lastControls.left ? 1 : 0;
    s.right = p.lastControls.right ? 1 : 0;
    s.center = p.lastControls.center ? 1 : 0;
    s.fire = p.lastControls.fire ? 1 : 0;
    s.firedByMouse = p.lastControls.firedByMouse ? 1 : 0;
    s.mouseAngle = p.lastControls.mouseAngle;
    // The step's inbound network payloads are a property of the step, not of
    // any one seat (R2's StepRecord already carries a single seatId). Carry
    // them on seat 0's record only, so they are written/read once per step
    // rather than duplicated across every seat's record for the same step.
    // CaptureStep is always called for every seat after a step, so seat 0's
    // record is present whenever there is a step at all. See
    // SetPlaybackInboundEvents() below for the matching decoder.
    if (seatId == 0) {
        const auto &events = BubbleGameReplayAccess::stepInboundEvents(game);
        std::vector<uint8_t> blob;
        AppendU16(blob, static_cast<uint16_t>(events.size()));
        for (const BubbleGame::InboundGameEvent &event : events) {
            AppendI32(blob, event.senderId);
            const uint16_t len = static_cast<uint16_t>(event.gameData.size());
            AppendU16(blob, len);
            blob.insert(blob.end(), event.gameData.begin(), event.gameData.end());
        }
        s.inboundEvents = std::move(blob);
    }
    return s;
}

RoundEndRecord CaptureRoundEnd(const BubbleGame &game) {
    RoundEndRecord e;
    if (BubbleGameReplayAccess::gameWon(game)) {
        e.outcome = kReplayOutcomeWin;
    } else if (BubbleGameReplayAccess::gameLost(game)) {
        e.outcome = kReplayOutcomeLoss;
    } else if (BubbleGameReplayAccess::gameFinish(game)) {
        // A committed multiplayer win (Classic elimination, Race target, ...)
        // sets roundWinnerIdx without gameWon/gameLost (those are solo
        // result-screen flags). Report it as a win so a Race/elimination
        // result is distinguishable from a genuine draw.
        e.outcome = (BubbleGameReplayAccess::wonByClearing(game) ||
                     BubbleGameReplayAccess::roundWinnerIdx(game) >= 0)
                        ? kReplayOutcomeWin
                        : kReplayOutcomeDraw;
    } else {
        e.outcome = kReplayOutcomeIncomplete;
    }
    const int winner = BubbleGameReplayAccess::roundWinnerIdx(game);
    e.winningSeatId = winner >= 0 ? static_cast<uint32_t>(winner) : 0;
    e.finalStateHash = CaptureCanonicalStateHash(game);
    e.durationMs = 0;
    e.complete = BubbleGameReplayAccess::gameFinish(game) ? 1 : 0;
    const int players = SeatCount(game);
    for (int i = 0; i < players; ++i) {
        const BubbleArray &p = BubbleGameReplayAccess::player(game, i);
        e.finalScore[i] = p.score;
        e.finalWins[i] = p.winCount;
    }
    return e;
}

uint64_t CaptureCanonicalStateHash(const BubbleGame &game) {
    std::vector<uint8_t> buf;

    AppendI32(buf, BubbleGameReplayAccess::simStep(game));
    AppendFloat(buf, BubbleGameReplayAccess::stepDeltaScale(game));
    // Round-relative game clock. Absolute modeTimerStart/modeTimerDeadline
    // values are implied by this plus the flags below and are deliberately not
    // hashed separately -- they are not independently meaningful.
    AppendU32(buf, BubbleGameReplayAccess::stepGameClockMs(game));
    // Training score, so a malus-injection/score divergence is caught at the
    // step it happens rather than only at round end.
    AppendI32(buf, BubbleGameReplayAccess::mpTrainScore(game));

    uint8_t gameFlags = 0;
    if (BubbleGameReplayAccess::gameFinish(game)) gameFlags |= 0x01;
    if (BubbleGameReplayAccess::gameWon(game)) gameFlags |= 0x02;
    if (BubbleGameReplayAccess::gameLost(game)) gameFlags |= 0x04;
    if (BubbleGameReplayAccess::wonByClearing(game)) gameFlags |= 0x08;
    if (BubbleGameReplayAccess::gameMatchOver(game)) gameFlags |= 0x10;
    if (BubbleGameReplayAccess::mpTrainDone(game)) gameFlags |= 0x20;
    if (BubbleGameReplayAccess::modeTimerExpired(game)) gameFlags |= 0x40;
    AppendU8(buf, gameFlags);
    AppendI32(buf, BubbleGameReplayAccess::roundWinnerIdx(game));
    AppendU32(buf, BubbleGameReplayAccess::rngState(game));

    // Every live seat's board/state, not just seat 0: a divergence in any
    // seat is then caught at the step it happens rather than only if it later
    // propagates into seat 0 or the shared projectile list.
    const int players = SeatCount(game);
    for (int i = 0; i < players; ++i) {
        AppendBoardState(buf, BubbleGameReplayAccess::player(game, i));
    }
    AppendProjectiles(buf);

    return CanonicalHashFnv1a64(buf);
}
