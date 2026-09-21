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

#include "replay_library.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <system_error>
#include <utility>

#include "gamesettings.h"
#include "platform.h"
#include "replay_format.h"

ReplayLibrary *ReplayLibrary::ptrInstance = nullptr;

ReplayLibrary *ReplayLibrary::Instance() {
    if (ptrInstance == nullptr) ptrInstance = new ReplayLibrary();
    return ptrInstance;
}

void ReplayLibrary::Dispose() {
    if (ptrInstance == nullptr) return;
    delete ptrInstance;
    ptrInstance = nullptr;
}

namespace {

// Writes a complete byte vector to `path` without ever truncating it in place:
// the whole thing is staged as `<path>.tmp` and swapped in with the platform's
// atomic rename. Same shape as HighscoreManager's TU-local writeFileAtomically,
// which cannot be reused directly because it is text-mode and std::string-only.
//
// Deliberately kept here rather than hoisted to platform.*: this is the only
// binary producer today, and platform.h's half (ReplaceFileAtomically) is the
// part with a platform-specific implementation. A second binary writer would
// be the point to promote it.
bool WriteFileAtomically(const std::filesystem::path &path,
                         const std::vector<uint8_t> &bytes) {
    const std::string temp = path.string() + ".tmp";

    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayLibrary: could not stage %s", temp.c_str());
        return false;
    }
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out.good()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayLibrary: short write staging %s", temp.c_str());
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return false;
    }

    return ReplaceFileAtomically(temp, path.string());
}

bool ReadWholeFile(const std::filesystem::path &path, std::vector<uint8_t> &out) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const std::streamsize size = in.tellg();
    if (size < 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    if (size > 0) {
        in.read(reinterpret_cast<char *>(out.data()), size);
        if (!in) return false;
    }
    return true;
}

// A library entry name is a bare file name inside replays/. Reject anything
// with path structure so no name can ever resolve outside the directory:
// absolute paths, either platform's directory separator, a Windows drive/ADS
// colon, and any "..".
bool IsPlainLibraryName(const std::string &name) {
    if (name.empty() || name == "." || name == "..") return false;
    if (name.find('/') != std::string::npos) return false;
    if (name.find('\\') != std::string::npos) return false;
    if (name.find("..") != std::string::npos) return false;
    if (name.find(':') != std::string::npos) return false;
    const std::filesystem::path p(name);
    if (p.is_absolute() || p.has_root_name() || p.has_root_directory()) return false;
    return true;
}

bool IsKnownRecordType(uint8_t value) {
    switch (static_cast<RecordType>(value)) {
        case RecordType::RoundStart:
        case RecordType::Step:
        case RecordType::Assertion:
        case RecordType::RoundEnd:
            return true;
    }
    return false;
}

// Decodes only the metadata records: header, round start, and the trailing
// round end when the file was sealed. Step/assertion payloads are never read.
//
// ReplayReader has no way to skip a record and the codec is frozen, so once the
// header and round start are consumed this walks the remaining
// [type][u32 length][payload] framing by hand -- the layout documented in
// replay_format.h -- to find the round end, then hands that record back to
// ReplayReader for decoding. Only the 5-byte framing is touched by hand; no
// step payload is ever copied.
bool DecodeMetadata(const std::vector<uint8_t> &bytes, ReplayHeader &header,
                    RoundStartRecord &start, RoundEndRecord &end, bool &haveEnd) {
    haveEnd = false;
    ReplayReader reader(bytes);
    if (reader.ReadHeader(header) != DecodeResult::Ok) return false;
    if (reader.ReadRoundStart(start) != DecodeResult::Ok) return false;

    size_t pos = bytes.size() - reader.Remaining();
    while (pos + 5 <= bytes.size()) {
        const uint8_t typeByte = bytes[pos];
        if (!IsKnownRecordType(typeByte)) return false;
        const uint32_t payloadLen =
            static_cast<uint32_t>(bytes[pos + 1]) |
            (static_cast<uint32_t>(bytes[pos + 2]) << 8) |
            (static_cast<uint32_t>(bytes[pos + 3]) << 16) |
            (static_cast<uint32_t>(bytes[pos + 4]) << 24);
        if (payloadLen > kMaxPayloadLength) return false;
        if (bytes.size() - (pos + 5) < payloadLen) return false;  // truncated
        if (static_cast<RecordType>(typeByte) == RecordType::RoundEnd) {
            ReplayReader endReader(bytes.data() + pos, 5 + payloadLen);
            if (endReader.ReadRoundEnd(end) != DecodeResult::Ok) return false;
            haveEnd = true;
            return true;
        }
        pos += 5 + payloadLen;
    }
    // A clean end of stream after a complete record means the file is a valid
    // prefix that was never sealed (no round end). That is listable, just
    // marked incomplete; leftover partial framing is not.
    return pos == bytes.size();
}

} // namespace

bool ReplayLibrary::EnsureReady(std::filesystem::path &dir) const {
    GameSettings *settings = GameSettings::Instance();
    const std::string pref = settings->prefPath ? settings->prefPath : std::string();

    if (!dirResolved || resolvedPrefPath != pref) {
        resolvedPrefPath = pref;
        dirUsable = false;
        replaysDir.clear();
        if (!pref.empty()) {
            replaysDir = std::filesystem::path(pref) / "replays";
            if (EnsureDirectoryExists(replaysDir.string().c_str())) {
                dirUsable = true;
            } else {
                // EnsureDirectoryExists has already logged the reason. Record
                // the failure for this path so every later write is a no-op
                // rather than a repeated mkdir/log.
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "ReplayLibrary: %s is unusable; replay saving is disabled "
                            "for this session", replaysDir.string().c_str());
            }
        }
        dirResolved = true;
    }

    dir = replaysDir;
    return dirUsable;
}

bool ReplayLibrary::ResolveEntryPath(const std::string &filename,
                                     std::filesystem::path &out) const {
    std::filesystem::path dir;
    if (!EnsureReady(dir)) return false;
    if (!IsPlainLibraryName(filename)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayLibrary: refusing unsafe replay name '%s'", filename.c_str());
        return false;
    }
    out = dir / filename;
    return true;
}

bool ReplayLibrary::ReadEntryMetadata(const std::filesystem::path &path,
                                      ReplayEntry &entry) const {
    std::vector<uint8_t> bytes;
    if (!ReadWholeFile(path, bytes)) return false;

    ReplayHeader header;
    RoundStartRecord start;
    RoundEndRecord end;
    bool haveEnd = false;
    if (!DecodeMetadata(bytes, header, start, end, haveEnd)) return false;

    std::error_code ec;
    entry = ReplayEntry{};
    entry.filename = path.filename().string();
    entry.mtime = std::filesystem::last_write_time(path, ec);
    if (ec) entry.mtime = std::filesystem::file_time_type{};
    entry.gameMode = start.gameMode;
    entry.playerCount = start.playerCount;
    if (haveEnd) {
        entry.durationMs = end.durationMs;
        entry.outcome = end.outcome;
        entry.complete = end.complete != 0;
    }
    entry.imported = false;
    entry.platformIncompatible = !IsReplayPlatformCompatible(header.platformFloatProfile);
    return true;
}

void ReplayLibrary::CollectEntries(const std::filesystem::path &dir,
                                   std::vector<ReplayEntry> &out) const {
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayLibrary: could not list %s: %s",
                    dir.string().c_str(), ec.message().c_str());
        return;
    }

    // Manual increment rather than range-for: operator++ can throw on a
    // filesystem error (a directory unlinked mid-scan), and this listing must
    // skip whatever it cannot read rather than propagate.
    const std::filesystem::directory_iterator end;
    for (; it != end; ) {
        const std::filesystem::directory_entry &de = *it;
        std::error_code itemEc;
        if (de.is_regular_file(itemEc) && !itemEc &&
            de.path().extension() == ".fbr") {
            ReplayEntry entry;
            if (ReadEntryMetadata(de.path(), entry)) {
                out.push_back(std::move(entry));
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "ReplayLibrary: skipping unreadable replay %s",
                            de.path().string().c_str());
            }
        }
        it.increment(ec);
        if (ec) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "ReplayLibrary: stopped listing %s: %s",
                        dir.string().c_str(), ec.message().c_str());
            break;
        }
    }
}

std::vector<ReplayLibrary::ReplayEntry> ReplayLibrary::List() const {
    std::vector<ReplayEntry> entries;
    std::filesystem::path dir;
    if (!EnsureReady(dir)) return entries;

    CollectEntries(dir, entries);

    // Newest first. Filesystem mtime is the primary key; the file name, whose
    // leading timestamp/counter is fixed-width and therefore lexical-orderable,
    // breaks ties when several rounds landed in the same mtime tick.
    std::sort(entries.begin(), entries.end(),
              [](const ReplayEntry &a, const ReplayEntry &b) {
                  if (a.mtime != b.mtime) return a.mtime > b.mtime;
                  return a.filename > b.filename;
              });
    return entries;
}

std::string ReplayLibrary::MakeFilename(const std::filesystem::path &dir) {
    using namespace std::chrono;
    const auto nowMs =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    // The counter disambiguates writes inside one millisecond; the exists()
    // check disambiguates a name left by a previous process (a restart inside
    // the same millisecond reuses counter values). Both fields are fixed width
    // so lexical order matches write order.
    for (int attempt = 0; attempt < 1000; ++attempt) {
        char name[64];
        std::snprintf(name, sizeof(name), "replay-%013lld-%04u.fbr",
                      static_cast<long long>(nowMs),
                      static_cast<unsigned>(nextSequence++));
        std::error_code ec;
        const bool exists = std::filesystem::exists(dir / name, ec);
        if (ec) continue;              // could not verify; try the next name
        if (!exists) return name;
    }
    return {};
}

bool ReplayLibrary::Write(std::vector<uint8_t> bytes) {
    // Independent of ReplayRecorder: keep count 0 means no file, whatever
    // caller reaches this.
    const int keep = GameSettings::Instance()->replayKeepCount();
    if (keep <= 0) return false;
    if (bytes.empty()) return false;

    std::filesystem::path dir;
    if (!EnsureReady(dir)) return false;

    const std::string name = MakeFilename(dir);
    if (name.empty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayLibrary: could not find a free replay filename");
        return false;
    }

    if (!WriteFileAtomically(dir / name, bytes)) return false;

    // Eviction runs only after the new file is durably renamed into place, so a
    // crash mid-eviction can never lose the round that just finished.
    EvictTo(keep);
    RequestPersistentStorageFlush();
    return true;
}

void ReplayLibrary::EvictTo(int keep) const {
    std::filesystem::path dir;
    if (!EnsureReady(dir)) return;

    std::vector<ReplayEntry> entries;
    CollectEntries(dir, entries);
    if (static_cast<int>(entries.size()) <= keep) return;

    // Oldest first -- the exact reverse of List()'s ordering, including the
    // same tie-break so the choice is deterministic when mtimes collide.
    std::sort(entries.begin(), entries.end(),
              [](const ReplayEntry &a, const ReplayEntry &b) {
                  if (a.mtime != b.mtime) return a.mtime < b.mtime;
                  return a.filename < b.filename;
              });

    const int over = static_cast<int>(entries.size()) - keep;
    for (int i = 0; i < over; ++i) {
        std::error_code ec;
        std::filesystem::remove(dir / entries[i].filename, ec);
        if (ec) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "ReplayLibrary: could not evict %s: %s",
                        entries[i].filename.c_str(), ec.message().c_str());
        }
    }
}

bool ReplayLibrary::Delete(const std::string &filename) {
    std::filesystem::path path;
    if (!ResolveEntryPath(filename, path)) return false;

    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayLibrary: no such replay %s", filename.c_str());
        return false;
    }
    if (!std::filesystem::remove(path, ec) || ec) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "ReplayLibrary: could not delete %s: %s",
                    filename.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

bool ReplayLibrary::ReadBytes(const std::string &filename,
                              std::vector<uint8_t> &out) const {
    std::filesystem::path path;
    if (!ResolveEntryPath(filename, path)) return false;
    return ReadWholeFile(path, out);
}

int ReplayLibrary::EvictionCountFor(int keep) const {
    // 0 (or below) means "stop recording new rounds," never "delete what's
    // already there" -- see the header comment. Nothing would be evicted, so
    // the preview must say 0, not the library's full size.
    if (keep <= 0) return 0;

    std::filesystem::path dir;
    if (!EnsureReady(dir)) return 0;

    // The same collection EvictTo() sorts and trims, so the number of files
    // it would remove is exactly what this preview reports.
    std::vector<ReplayEntry> entries;
    CollectEntries(dir, entries);
    return std::max(0, static_cast<int>(entries.size()) - keep);
}

void ReplayLibrary::ApplyKeepCount(int keep) const {
    // A settings change never bulk-deletes: 0 (or below) only has to stop
    // future recording, which is ReplayRecorder's own separate gate.
    if (keep <= 0) return;
    EvictTo(keep);
}
