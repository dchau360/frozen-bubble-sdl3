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

#ifndef REPLAY_LIBRARY_H
#define REPLAY_LIBRARY_H

// R4b: the on-disk rolling replay library.
//
// R4a's ReplayRecorder captured finished rounds and handed their complete .fbr
// bytes to an injectable sink; nothing in production installed one, so capture
// was inert. This is that sink: it owns <prefPath>replays/, writes each sealed
// recording there atomically under a collision-free internal name, and evicts
// the oldest entries down to GameSettings' Replay:KeepCount.
//
// Scope is deliberately storage only. There is no UI, no viewer and no
// export/import here -- R4c/R4d/R4e own those, and they list/read through this
// class rather than touching files themselves.
//
// Singleton shape follows HighscoreManager: a private constructor, a lazily
// created Instance(), an explicit Dispose(), and no dependency injection. The
// library reads GameSettings::Instance()->prefPath and filesystem state on use
// instead of caching a renderer or a handle, so it has nothing to initialize at
// startup and nothing to tear down beyond the allocation.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class ReplayLibrary final
{
public:
    // One library entry's metadata, decoded from the file's own header/round
    // start/round end records so a future menu can list the library without
    // decoding any step data. mtime is the file's filesystem timestamp -- the
    // replay format deliberately carries no wall-clock field of its own, so
    // this is the only "date" a listing has.
    struct ReplayEntry {
        // Bare file name inside replays/, never a path. Delete()/ReadBytes()
        // take exactly this string.
        std::string filename;
        std::filesystem::file_time_type mtime{};
        uint8_t gameMode = 0;
        uint8_t playerCount = 0;
        uint32_t durationMs = 0;
        uint8_t outcome = 0;
        bool complete = false;
        // Always false until R4e's import path exists to set it. Present now so
        // adding import later does not need another struct/format change.
        bool imported = false;
        // True when this file's recorded platformFloatProfile does not match
        // this build's own (and is nonzero). Set by List()'s per-file header
        // decode. A flagged entry is still listed (never skipped the way a
        // corrupt file is) so the UI can show it and let the player delete it;
        // playback must refuse it.
        bool platformIncompatible = false;
    };

    static ReplayLibrary *Instance();
    void Dispose();
    ReplayLibrary(const ReplayLibrary &) = delete;
    ReplayLibrary &operator=(const ReplayLibrary &) = delete;

    // Every decodable *.fbr in replays/, newest-mtime-first (ties broken by
    // file name, which embeds write order). A file that cannot be decoded is
    // logged and skipped rather than failing the whole list.
    std::vector<ReplayEntry> List() const;

    // ReplayRecorder's sink target. Writes the bytes atomically and then evicts
    // down to the configured keep count. Returns false -- writing nothing --
    // when the keep count is 0 or less, when the bytes are empty, or when the
    // replays directory is unusable. The keep-count check lives here as well as
    // in ReplayRecorder because this class is safe to call directly.
    bool Write(std::vector<uint8_t> bytes);

    // Removes exactly one named entry; evicts nothing else. Refuses any name
    // that could resolve outside replays/ (absolute, a separator, "..").
    bool Delete(const std::string &filename);

    // Full bytes of one entry, for a future viewer. Same name containment rule
    // as Delete().
    bool ReadBytes(const std::string &filename, std::vector<uint8_t> &out) const;

    // --- R4d: keep-count editing from the Replays page ---------------------
    // Write()'s private EvictTo(int) already deletes oldest-mtime-first down
    // to a count, but only as a step after a write. The Replays page changes
    // the keep count with no write happening, so it needs that same effect on
    // demand. These two methods are the whole public surface R4d adds -- the
    // eviction ordering itself is not duplicated, so the preview and the
    // apply can never disagree.

    // How many entries ApplyKeepCount(keep) would delete right now. Computed
    // from the same CollectEntries() ordering EvictTo() uses, not from
    // List().size() arithmetic, so a future change to either cannot make the
    // preview lie. keep <= 0 always reports 0: per REPLAY_PLAN.md, a keep
    // count of 0 means "stop recording new rounds," not "delete everything
    // that's there" -- a settings change must never act as a bulk delete.
    int EvictionCountFor(int keep) const;

    // Applies EvictTo(keep) immediately, independent of any write, for keep >
    // 0. A thin public wrapper around the existing private method, so the
    // page cannot drift from Write()'s own eviction behavior. keep <= 0 is a
    // deliberate no-op here -- same reasoning as EvictionCountFor() above:
    // the keep-count setting reaching 0 only has to stop ReplayRecorder from
    // starting new recordings (its own separate gate), never delete existing
    // ones. Returns nothing: a deletion failure is logged by EvictTo(),
    // exactly as it is after a write.
    void ApplyKeepCount(int keep) const;

private:
    ReplayLibrary() = default;
    ~ReplayLibrary() = default;
    static ReplayLibrary *ptrInstance;

    // Resolves <prefPath>replays and creates it if needed, caching the result
    // for the current pref path. A failure (the path exists as a file, the
    // parent is unwritable) is remembered for that path so every later call is
    // a clean no-op instead of retrying the mkdir forever; a change of
    // prefPath re-resolves.
    bool EnsureReady(std::filesystem::path &dir) const;
    // Validates a bare library file name and joins it onto replays/.
    bool ResolveEntryPath(const std::string &filename, std::filesystem::path &out) const;
    void CollectEntries(const std::filesystem::path &dir,
                        std::vector<ReplayEntry> &out) const;
    bool ReadEntryMetadata(const std::filesystem::path &path, ReplayEntry &entry) const;
    // "replay-<13-digit unix millis>-<4-digit counter>.fbr", collision-checked
    // against the directory. Fixed-width fields make lexical order match write
    // order, which the eviction tie-break relies on when filesystem mtimes are
    // equal (several rounds written inside one mtime tick).
    std::string MakeFilename(const std::filesystem::path &dir);
    void EvictTo(int keep) const;

    mutable bool dirResolved = false;
    mutable bool dirUsable = false;
    mutable std::string resolvedPrefPath;
    mutable std::filesystem::path replaysDir;
    uint32_t nextSequence = 0;
};

#endif // REPLAY_LIBRARY_H
