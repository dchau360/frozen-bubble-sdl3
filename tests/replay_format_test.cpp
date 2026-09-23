// Codec-only tests for the replay format (package R2 -- docs/REPLAY_PLAN.md).
// Deliberately SDL-free: no SDL_Init/window/renderer at all, unlike every
// existing gameplay test, proving replay_format.h/.cpp has zero engine
// dependency. See src/replay_format.h/.cpp and docs/REPLAY_PROGRESS.md.

#include "replay_format.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)

// ---------------------------------------------------------------------------
// Test-value builders (hand-constructed, not derived from a real game).
// ---------------------------------------------------------------------------

static ReplayHeader MakeTestHeader() {
    ReplayHeader h;
    h.formatVersion = kCurrentFormatVersion;
    h.simRulesVersion = kCurrentSimRulesVersion;
    h.buildFingerprint = {'v', '2', '.', '4', '.', '1', '0', '5'};
    h.platformFloatProfile = 1;
    h.featureFlags = 0;
    return h;
}

static RoundStartRecord MakeTestRoundStart() {
    RoundStartRecord rec;
    rec.roundId = 0x11223344u;
    rec.playerCount = 3;
    rec.gameMode = 1;
    rec.victoriesLimit = 5;
    rec.networkGame = 1;
    for (int i = 0; i < kMaxPlayers; ++i) {
        rec.seatIds[i] = static_cast<uint32_t>(1000 + i);
        rec.seatOwned[i] = (i < 3) ? 1 : 0;
        rec.seatTeam[i] = static_cast<uint8_t>(i % 2);
        rec.currentColor[i] = static_cast<uint8_t>(i % 8);
        rec.nextColor[i] = static_cast<uint8_t>((i + 1) % 8);
        rec.startingScore[i] = i * 10;
        rec.startingWins[i] = i;
    }
    rec.levelHash = 0x0102030405060708ull;
    rec.levelLayout = {0xAA, 0xBB, 0xCC, 0xDD, 0x01, 0x02};
    rec.startingBoards[0] = {0x01, 0x02, 0x03};
    rec.startingBoards[1] = {0x04, 0x05};
    rec.gameplayRngState = 0xDEADBEEFu;
    rec.boardGeometryId = 7;
    rec.initialSimStep = 0;
    rec.initialStepDeltaScale = 1.0f;
    return rec;
}

static StepRecord MakeTestStep() {
    StepRecord rec;
    rec.simStep = 42;
    rec.deltaScale = 1.5f;
    rec.gameClockMs = 123456;
    rec.seatId = 2;
    rec.left = 1;
    rec.right = 0;
    rec.center = 1;
    rec.fire = 1;
    rec.firedByMouse = 0;
    rec.mouseAngle = -1.0f;
    rec.inboundEvents = {0x0A, 0x0B, 0x0C};
    return rec;
}

static AssertionRecord MakeTestAssertion() {
    AssertionRecord rec;
    rec.simStep = 100;
    rec.seatId = 1;
    rec.acceptedShotColor = 4;
    rec.acceptedColumn = 6;
    rec.acceptedRow = 3;
    rec.canonicalStateHash = 0x1122334455667788ull;
    return rec;
}

static RoundEndRecord MakeTestRoundEnd() {
    RoundEndRecord rec;
    rec.outcome = 1;
    rec.winningSeatId = 2;
    rec.finalStateHash = 0xCAFEBABEDEADBEEFull;
    rec.durationMs = 654321;
    rec.complete = 1;
    for (int i = 0; i < kMaxPlayers; ++i) {
        rec.finalScore[i] = i * 100;
        rec.finalWins[i] = i % 3;
    }
    return rec;
}

// ---------------------------------------------------------------------------
// Field-by-field equality (no operator== on the structs; this file owns the
// comparison so a future field addition to replay_format.h forces this file
// to be updated deliberately rather than silently comparing fewer fields).
// ---------------------------------------------------------------------------

static bool HeaderEquals(const ReplayHeader &a, const ReplayHeader &b) {
    for (int i = 0; i < 4; ++i) if (a.magic[i] != b.magic[i]) return false;
    return a.formatVersion == b.formatVersion &&
           a.simRulesVersion == b.simRulesVersion &&
           a.buildFingerprint == b.buildFingerprint &&
           a.platformFloatProfile == b.platformFloatProfile &&
           a.featureFlags == b.featureFlags;
}

static bool RoundStartEquals(const RoundStartRecord &a, const RoundStartRecord &b) {
    if (a.roundId != b.roundId || a.playerCount != b.playerCount || a.gameMode != b.gameMode ||
        a.victoriesLimit != b.victoriesLimit || a.networkGame != b.networkGame ||
        a.levelHash != b.levelHash || a.levelLayout != b.levelLayout ||
        a.gameplayRngState != b.gameplayRngState || a.boardGeometryId != b.boardGeometryId ||
        a.initialSimStep != b.initialSimStep || a.initialStepDeltaScale != b.initialStepDeltaScale) {
        return false;
    }
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (a.seatIds[i] != b.seatIds[i]) return false;
        if (a.seatOwned[i] != b.seatOwned[i]) return false;
        if (a.seatTeam[i] != b.seatTeam[i]) return false;
        if (a.startingBoards[i] != b.startingBoards[i]) return false;
        if (a.currentColor[i] != b.currentColor[i]) return false;
        if (a.nextColor[i] != b.nextColor[i]) return false;
        if (a.startingScore[i] != b.startingScore[i]) return false;
        if (a.startingWins[i] != b.startingWins[i]) return false;
    }
    return true;
}

static bool StepEquals(const StepRecord &a, const StepRecord &b) {
    return a.simStep == b.simStep && a.deltaScale == b.deltaScale && a.gameClockMs == b.gameClockMs &&
           a.seatId == b.seatId && a.left == b.left && a.right == b.right && a.center == b.center &&
           a.fire == b.fire && a.firedByMouse == b.firedByMouse && a.mouseAngle == b.mouseAngle &&
           a.inboundEvents == b.inboundEvents;
}

static bool AssertionEquals(const AssertionRecord &a, const AssertionRecord &b) {
    return a.simStep == b.simStep && a.seatId == b.seatId && a.acceptedShotColor == b.acceptedShotColor &&
           a.acceptedColumn == b.acceptedColumn && a.acceptedRow == b.acceptedRow &&
           a.canonicalStateHash == b.canonicalStateHash;
}

static bool RoundEndEquals(const RoundEndRecord &a, const RoundEndRecord &b) {
    if (a.outcome != b.outcome || a.winningSeatId != b.winningSeatId || a.finalStateHash != b.finalStateHash ||
        a.durationMs != b.durationMs || a.complete != b.complete) {
        return false;
    }
    for (int i = 0; i < kMaxPlayers; ++i) {
        if (a.finalScore[i] != b.finalScore[i]) return false;
        if (a.finalWins[i] != b.finalWins[i]) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Portable byte fixtures -- generated once from these exact test values via
// ReplayWriter and pinned here as literals. The round-trip tests below
// compare freshly-encoded bytes against these literals byte-for-byte, which
// catches a field-order/width/padding change that still happens to encode-
// then-decode back to equal struct values (round-trip alone cannot catch
// that). Do NOT regenerate these from the struct at test time.
// ---------------------------------------------------------------------------

static const std::vector<uint8_t> kHeaderBytes = {
    0x46, 0x42, 0x52, 0x31, 0x01, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x76, 0x32, 0x2e, 0x34, 0x2e, 0x31, 0x30, 0x35, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

static const std::vector<uint8_t> kRoundStartBytes = {
    0x01, 0xc2, 0x01, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x03, 0x01, 0x05,
    0x00, 0x00, 0x00, 0x01, 0xe8, 0x03, 0x00, 0x00, 0xe9, 0x03, 0x00, 0x00,
    0xea, 0x03, 0x00, 0x00, 0xeb, 0x03, 0x00, 0x00, 0xec, 0x03, 0x00, 0x00,
    0xed, 0x03, 0x00, 0x00, 0xee, 0x03, 0x00, 0x00, 0xef, 0x03, 0x00, 0x00,
    0xf0, 0x03, 0x00, 0x00, 0xf1, 0x03, 0x00, 0x00, 0xf2, 0x03, 0x00, 0x00,
    0xf3, 0x03, 0x00, 0x00, 0xf4, 0x03, 0x00, 0x00, 0xf5, 0x03, 0x00, 0x00,
    0xf6, 0x03, 0x00, 0x00, 0xf7, 0x03, 0x00, 0x00, 0xf8, 0x03, 0x00, 0x00,
    0xf9, 0x03, 0x00, 0x00, 0xfa, 0x03, 0x00, 0x00, 0xfb, 0x03, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x01, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
    0x06, 0x00, 0x00, 0x00, 0xaa, 0xbb, 0xcc, 0xdd, 0x01, 0x02, 0x03, 0x00,
    0x00, 0x00, 0x01, 0x02, 0x03, 0x02, 0x00, 0x00, 0x00, 0x04, 0x05, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01,
    0x02, 0x03, 0x04, 0xef, 0xbe, 0xad, 0xde, 0x07, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x0a,
    0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x28,
    0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x46,
    0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x00, 0x64,
    0x00, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x82,
    0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x96, 0x00, 0x00, 0x00, 0xa0,
    0x00, 0x00, 0x00, 0xaa, 0x00, 0x00, 0x00, 0xb4, 0x00, 0x00, 0x00, 0xbe,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x0b,
    0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x0e,
    0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x11,
    0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
};

static const std::vector<uint8_t> kStepBytes = {
    0x02, 0x20, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,
    0x3f, 0x40, 0xe2, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x03, 0x00, 0x00, 0x00, 0x0a, 0x0b,
    0x0c,
};

static const std::vector<uint8_t> kAssertionBytes = {
    0x03, 0x16, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x04, 0x06,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x88, 0x77, 0x66, 0x55, 0x44,
    0x33, 0x22, 0x11,
};

static const std::vector<uint8_t> kRoundEndBytes = {
    0x04, 0xb2, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0xef, 0xbe,
    0xad, 0xde, 0xbe, 0xba, 0xfe, 0xca, 0xf1, 0xfb, 0x09, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x00, 0x2c,
    0x01, 0x00, 0x00, 0x90, 0x01, 0x00, 0x00, 0xf4, 0x01, 0x00, 0x00, 0x58,
    0x02, 0x00, 0x00, 0xbc, 0x02, 0x00, 0x00, 0x20, 0x03, 0x00, 0x00, 0x84,
    0x03, 0x00, 0x00, 0xe8, 0x03, 0x00, 0x00, 0x4c, 0x04, 0x00, 0x00, 0xb0,
    0x04, 0x00, 0x00, 0x14, 0x05, 0x00, 0x00, 0x78, 0x05, 0x00, 0x00, 0xdc,
    0x05, 0x00, 0x00, 0x40, 0x06, 0x00, 0x00, 0xa4, 0x06, 0x00, 0x00, 0x08,
    0x07, 0x00, 0x00, 0x6c, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00,
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void TestRoundTripAndFixtures() {
    // Header
    {
        ReplayHeader h = MakeTestHeader();
        ReplayWriter w;
        w.WriteHeader(h);
        CHECK(w.Bytes() == kHeaderBytes);

        ReplayReader r(w.Bytes());
        ReplayHeader decoded;
        CHECK(r.ReadHeader(decoded) == DecodeResult::Ok);
        CHECK(HeaderEquals(h, decoded));
    }

    // RoundStart
    {
        RoundStartRecord rec = MakeTestRoundStart();
        ReplayWriter w;
        w.WriteRoundStart(rec);
        CHECK(w.Bytes() == kRoundStartBytes);

        ReplayReader r(w.Bytes());
        RoundStartRecord decoded;
        CHECK(r.ReadRoundStart(decoded) == DecodeResult::Ok);
        CHECK(RoundStartEquals(rec, decoded));
    }

    // Step
    {
        StepRecord rec = MakeTestStep();
        ReplayWriter w;
        w.WriteStep(rec);
        CHECK(w.Bytes() == kStepBytes);

        ReplayReader r(w.Bytes());
        StepRecord decoded;
        CHECK(r.ReadStep(decoded) == DecodeResult::Ok);
        CHECK(StepEquals(rec, decoded));
    }

    // Assertion
    {
        AssertionRecord rec = MakeTestAssertion();
        ReplayWriter w;
        w.WriteAssertion(rec);
        CHECK(w.Bytes() == kAssertionBytes);

        ReplayReader r(w.Bytes());
        AssertionRecord decoded;
        CHECK(r.ReadAssertion(decoded) == DecodeResult::Ok);
        CHECK(AssertionEquals(rec, decoded));
    }

    // RoundEnd
    {
        RoundEndRecord rec = MakeTestRoundEnd();
        ReplayWriter w;
        w.WriteRoundEnd(rec);
        CHECK(w.Bytes() == kRoundEndBytes);

        ReplayReader r(w.Bytes());
        RoundEndRecord decoded;
        CHECK(r.ReadRoundEnd(decoded) == DecodeResult::Ok);
        CHECK(RoundEndEquals(rec, decoded));
    }

    // A full stream -- header followed by one of each record type, decoded
    // sequentially in the order written -- proves records compose, not just
    // that each type decodes in isolation.
    {
        ReplayWriter w;
        w.WriteHeader(MakeTestHeader());
        w.WriteRoundStart(MakeTestRoundStart());
        w.WriteStep(MakeTestStep());
        w.WriteAssertion(MakeTestAssertion());
        w.WriteRoundEnd(MakeTestRoundEnd());

        ReplayReader r(w.Bytes());
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::Ok);
        CHECK(HeaderEquals(h, MakeTestHeader()));

        RecordType t;
        CHECK(r.PeekRecordType(t) == DecodeResult::Ok);
        CHECK(t == RecordType::RoundStart);
        RoundStartRecord rs;
        CHECK(r.ReadRoundStart(rs) == DecodeResult::Ok);
        CHECK(RoundStartEquals(rs, MakeTestRoundStart()));

        CHECK(r.PeekRecordType(t) == DecodeResult::Ok);
        CHECK(t == RecordType::Step);
        StepRecord st;
        CHECK(r.ReadStep(st) == DecodeResult::Ok);
        CHECK(StepEquals(st, MakeTestStep()));

        CHECK(r.PeekRecordType(t) == DecodeResult::Ok);
        CHECK(t == RecordType::Assertion);
        AssertionRecord as;
        CHECK(r.ReadAssertion(as) == DecodeResult::Ok);
        CHECK(AssertionEquals(as, MakeTestAssertion()));

        CHECK(r.PeekRecordType(t) == DecodeResult::Ok);
        CHECK(t == RecordType::RoundEnd);
        RoundEndRecord re;
        CHECK(r.ReadRoundEnd(re) == DecodeResult::Ok);
        CHECK(RoundEndEquals(re, MakeTestRoundEnd()));

        // Fully consumed: no more bytes.
        CHECK(r.Remaining() == 0);
    }
}

static void TestDeterministicHash() {
    RoundStartRecord rec = MakeTestRoundStart();
    ReplayWriter w1;
    w1.WriteRoundStart(rec);
    uint64_t h1 = CanonicalHashFnv1a64(w1.Bytes());

    // Same bytes, repeated call -> same hash.
    uint64_t h1Again = CanonicalHashFnv1a64(w1.Bytes());
    CHECK(h1 == h1Again);

    // Independently-constructed-but-equal record -> same hash.
    RoundStartRecord rec2 = MakeTestRoundStart();
    ReplayWriter w2;
    w2.WriteRoundStart(rec2);
    uint64_t h2 = CanonicalHashFnv1a64(w2.Bytes());
    CHECK(h1 == h2);

    // Changing one field flips the hash.
    RoundStartRecord rec3 = MakeTestRoundStart();
    rec3.roundId += 1;
    ReplayWriter w3;
    w3.WriteRoundStart(rec3);
    uint64_t h3 = CanonicalHashFnv1a64(w3.Bytes());
    CHECK(h3 != h1);

    // Changing a deeply-nested field (one byte inside a variable-length
    // per-player blob) also flips the hash.
    RoundStartRecord rec4 = MakeTestRoundStart();
    rec4.startingBoards[1].push_back(0xFF);
    ReplayWriter w4;
    w4.WriteRoundStart(rec4);
    uint64_t h4 = CanonicalHashFnv1a64(w4.Bytes());
    CHECK(h4 != h1);

    // Empty input and a known small vector have stable, non-degenerate
    // values (sanity, not exact literal pinning -- the pinned-bytes test
    // above already covers exact-byte regression).
    CHECK(CanonicalHashFnv1a64(std::vector<uint8_t>{}) == CanonicalHashFnv1a64(std::vector<uint8_t>{}));
    std::vector<uint8_t> a = {1, 2, 3};
    std::vector<uint8_t> b = {1, 2, 4};
    CHECK(CanonicalHashFnv1a64(a) != CanonicalHashFnv1a64(b));
}

static void TestVersionRejection() {
    ReplayHeader h = MakeTestHeader();
    h.formatVersion = static_cast<uint16_t>(kCurrentFormatVersion + 1);
    ReplayWriter w;
    w.WriteHeader(h);

    ReplayReader r(w.Bytes());
    ReplayHeader decoded;
    CHECK(r.ReadHeader(decoded) == DecodeResult::UnsupportedVersion);

    // A recognized version still decodes fine (control case, so the above
    // isn't accidentally always failing for an unrelated reason).
    ReplayHeader okHeader = MakeTestHeader();
    ReplayWriter okWriter;
    okWriter.WriteHeader(okHeader);
    ReplayReader okReader(okWriter.Bytes());
    ReplayHeader okDecoded;
    CHECK(okReader.ReadHeader(okDecoded) == DecodeResult::Ok);
}

static void TestTruncatedInput() {
    ReplayWriter w;
    w.WriteHeader(MakeTestHeader());
    const size_t headerSize = w.Bytes().size();
    w.WriteRoundStart(MakeTestRoundStart());
    const size_t afterRoundStart = w.Bytes().size();
    const size_t roundStartFrameSize = afterRoundStart - headerSize;
    // [type(1)][length(4)][payload(roundStartFrameSize - 5)]
    const size_t payloadSize = roundStartFrameSize - 5;
    const std::vector<uint8_t> &full = w.Bytes();

    // Mid-header: cut inside the header's own fields (well past the 4-byte
    // magic, which must stay intact for this to be a "wrong length", not a
    // "wrong magic", case).
    {
        std::vector<uint8_t> truncated(full.begin(), full.begin() + (headerSize - 3));
        ReplayReader r(truncated);
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::Truncated);
    }

    // Right after a record's type byte: header decodes fully, but only the
    // RoundStart record's type byte (no length bytes at all) follows.
    {
        std::vector<uint8_t> truncated(full.begin(), full.begin() + (headerSize + 1));
        ReplayReader r(truncated);
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::Ok);
        RoundStartRecord rec;
        CHECK(r.ReadRoundStart(rec) == DecodeResult::Truncated);
    }

    // Mid-length-field: type byte plus 2 of the 4 length bytes.
    {
        std::vector<uint8_t> truncated(full.begin(), full.begin() + (headerSize + 3));
        ReplayReader r(truncated);
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::Ok);
        RoundStartRecord rec;
        CHECK(r.ReadRoundStart(rec) == DecodeResult::Truncated);
    }

    // Mid-payload: full type+length frame present, but only half the
    // declared payload bytes actually follow.
    {
        size_t cut = headerSize + 5 + (payloadSize / 2);
        CHECK(cut < afterRoundStart); // sanity: actually a truncation
        std::vector<uint8_t> truncated(full.begin(), full.begin() + cut);
        ReplayReader r(truncated);
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::Ok);
        RoundStartRecord rec;
        CHECK(r.ReadRoundStart(rec) == DecodeResult::Truncated);
    }

    // Every single truncation length from 0 up to (but not including) the
    // full stream must be reported Truncated, BadMagic or BadRecordType --
    // never Ok, and never anything that reads out of bounds (verified for
    // real by running this whole binary under ASan/UBSan, not just by the
    // return-value check here). This is the broad sweep the "several
    // different byte offsets" requirement asks for, beyond the four named
    // offsets above.
    for (size_t cut = 0; cut < afterRoundStart; ++cut) {
        std::vector<uint8_t> truncated(full.begin(), full.begin() + cut);
        ReplayReader r(truncated);
        ReplayHeader h;
        DecodeResult hr = r.ReadHeader(h);
        if (hr != DecodeResult::Ok) {
            CHECK(hr == DecodeResult::Truncated || hr == DecodeResult::BadMagic);
            continue;
        }
        RoundStartRecord rec;
        DecodeResult rr = r.ReadRoundStart(rec);
        CHECK(rr == DecodeResult::Truncated);
    }
}

static void TestOversizedOrCorruptedLength() {
    ReplayWriter w;
    w.WriteHeader(MakeTestHeader());
    const size_t headerSize = w.Bytes().size();
    w.WriteRoundStart(MakeTestRoundStart());

    // Corrupt the RoundStart record's length field (the 4 bytes right after
    // its type byte, at headerSize+1..headerSize+4) to a value larger than
    // kMaxPayloadLength. Must be rejected before any read/allocate attempt
    // driven by that length -- i.e. RecordTooLarge, not Truncated, even
    // though the corrupted length also happens to exceed the actual
    // remaining buffer.
    {
        std::vector<uint8_t> corrupted = w.Bytes();
        uint32_t hostileLength = kMaxPayloadLength + 1;
        corrupted[headerSize + 1] = static_cast<uint8_t>(hostileLength & 0xFF);
        corrupted[headerSize + 2] = static_cast<uint8_t>((hostileLength >> 8) & 0xFF);
        corrupted[headerSize + 3] = static_cast<uint8_t>((hostileLength >> 16) & 0xFF);
        corrupted[headerSize + 4] = static_cast<uint8_t>((hostileLength >> 24) & 0xFF);

        ReplayReader r(corrupted);
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::Ok);
        RoundStartRecord rec;
        CHECK(r.ReadRoundStart(rec) == DecodeResult::RecordTooLarge);
    }

    // A hostile length at the absolute cap boundary: kMaxPayloadLength
    // itself is allowed by the cap check, but the buffer doesn't actually
    // contain that many bytes, so this must still be rejected -- as
    // Truncated, distinct from RecordTooLarge -- proving the two checks
    // (cap, then remaining-length) are both independently enforced.
    {
        std::vector<uint8_t> corrupted = w.Bytes();
        uint32_t atCapLength = kMaxPayloadLength;
        corrupted[headerSize + 1] = static_cast<uint8_t>(atCapLength & 0xFF);
        corrupted[headerSize + 2] = static_cast<uint8_t>((atCapLength >> 8) & 0xFF);
        corrupted[headerSize + 3] = static_cast<uint8_t>((atCapLength >> 16) & 0xFF);
        corrupted[headerSize + 4] = static_cast<uint8_t>((atCapLength >> 24) & 0xFF);

        ReplayReader r(corrupted);
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::Ok);
        RoundStartRecord rec;
        CHECK(r.ReadRoundStart(rec) == DecodeResult::Truncated);
    }
}

static void TestMalformedMagicAndBadRecordType() {
    // Malformed magic: flip one byte of an otherwise-valid header.
    {
        ReplayWriter w;
        w.WriteHeader(MakeTestHeader());
        std::vector<uint8_t> corrupted = w.Bytes();
        corrupted[0] = 'X';
        ReplayReader r(corrupted);
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::BadMagic);
    }

    // Bad record type byte: a value that is not any RecordType, including
    // the explicitly-reserved-but-unimplemented Checkpoint (5).
    {
        ReplayWriter w;
        w.WriteHeader(MakeTestHeader());
        const size_t headerSize = w.Bytes().size();
        w.WriteRoundStart(MakeTestRoundStart());

        for (uint8_t badType : {static_cast<uint8_t>(0), static_cast<uint8_t>(5), static_cast<uint8_t>(0xFF)}) {
            std::vector<uint8_t> corrupted = w.Bytes();
            corrupted[headerSize] = badType;
            ReplayReader r(corrupted);
            ReplayHeader h;
            CHECK(r.ReadHeader(h) == DecodeResult::Ok);

            RecordType t;
            CHECK(r.PeekRecordType(t) == DecodeResult::BadRecordType);

            RoundStartRecord rec;
            CHECK(r.ReadRoundStart(rec) == DecodeResult::BadRecordType);
        }
    }

    // A record type byte that IS valid but not the one the caller asked to
    // decode must also be rejected, not silently reinterpreted as the
    // other valid type.
    {
        ReplayWriter w;
        w.WriteHeader(MakeTestHeader());
        w.WriteStep(MakeTestStep()); // valid Step record, not RoundStart
        ReplayReader r(w.Bytes());
        ReplayHeader h;
        CHECK(r.ReadHeader(h) == DecodeResult::Ok);
        RoundStartRecord rec;
        CHECK(r.ReadRoundStart(rec) == DecodeResult::BadRecordType);
    }
}

static void TestPlatformFloatProfile() {
    const uint32_t current = ComputeCurrentPlatformFloatProfile();
    // Stability: two calls agree.
    CHECK(ComputeCurrentPlatformFloatProfile() == current);
    // This test binary always builds for a desktop platform (Linux x86_64/
    // arm64, macOS x86_64/arm64, or Windows x86_64), all of which the gate's
    // preprocessor checks recognize; 0 would mean it recognized nothing.
    CHECK(current != 0);
    // The current profile and the legacy sentinel are always compatible.
    CHECK(IsReplayPlatformCompatible(current));
    CHECK(IsReplayPlatformCompatible(0));
    // A different known bucket is refused. Pick one of the 8 defined
    // constants that isn't the current build's, without hardcoding which
    // platform this binary was built for.
    const uint32_t other = (current == 1) ? 2u : 1u;
    CHECK(other != current);
    CHECK(!IsReplayPlatformCompatible(other));
}

int main() {
    TestRoundTripAndFixtures();
    TestDeterministicHash();
    TestVersionRejection();
    TestTruncatedInput();
    TestOversizedOrCorruptedLength();
    TestMalformedMagicAndBadRecordType();
    TestPlatformFloatProfile();

    std::cout << "replay format tests passed\n";
    return 0;
}
