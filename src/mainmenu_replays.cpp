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

// The Replays page: R4d. R4a auto-saves every finished round, R4b puts those
// rounds in a rolling on-disk library, and R4c can play one back -- but until
// this file nothing a player could reach connected the three. This is the
// first user-facing screen of the replay feature.
//
// Two screens share the page, the way the team picker overlays the game room:
//
//   * The library list, opened from the CONTROLS & SETTINGS panel's REPLAYS
//     row (kKeyRowReplays). A scrolling entry list with per-row Play/Delete
//     buttons and a pinned library section (Export / Import / keep count).
//     Delete asks for confirmation; lowering the keep count below the number
//     of stored replays asks for confirmation naming the exact count.
//   * The playback viewer, entered by Play. MainMenu::Render() takes a
//     full-bleed branch for it -- ReplayPlayer::Draw() (R4c) plus the
//     controls this file draws on top -- so it never touches FrozenBubble's
//     GameState machine. ESC returns to the list, not the title screen.
//
// Input parity is the point of this package (see CLAUDE.md). Every control
// exists on three paths: arrow keys/D-pad, ENTER/A, and a drawn tap target,
// with the focused choice visibly highlighted and a footer hint spelling the
// keys out. The two confirm dialogs reuse the panel family's shared
// confirmDialogFocusNo flag and its LEFT/RIGHT/TAB + ENTER/ESC focus scheme.

#include "mainmenu_internal.h"
#include "menulist.h"
#include "audiomixer.h"
#include "bubblegame_replay.h"   // kReplayOutcome* (RoundEndRecord::outcome)
#include "gamemode.h"
#include "gamesettings.h"
#include "platform.h"
#include "replay_format.h"
#include "replay_library.h"
#include "replay_player.h"
#include "replay_recorder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr Uint32 kReplaysStatusMs = 2600;

// The viewer's supported rates, copied from ReplayPlayer's own table because
// it does not expose it -- SetSpeed() snaps to the nearest of these four, and
// reading Speed() back is how this labels and cycles them.
constexpr float kReplaySpeeds[] = {0.5f, 1.0f, 2.0f, 4.0f};

// std::filesystem::file_time_type and std::chrono::system_clock have different
// epochs and C++17 offers no conversion between them. The offset between the
// two clocks is a constant, so deriving it from "both nows" is the standard
// workaround; it is accurate to well under the minute this display shows.
std::string FormatReplayMtime(std::filesystem::file_time_type mtime) {
    if (mtime == std::filesystem::file_time_type{}) return "unknown date";
    const auto sysNow = std::chrono::system_clock::now();
    const auto fileNow = std::filesystem::file_time_type::clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::system_clock::duration>(
        mtime - fileNow);
    const std::time_t t = std::chrono::system_clock::to_time_t(sysNow + delta);
    const std::tm *tmp = std::localtime(&t);
    if (!tmp) return "unknown date";
    const std::tm tmv = *tmp;
    char buf[32];
    std::strftime(buf, sizeof(buf), "%b %d  %H:%M", &tmv);
    return buf;
}

// Compact filename stamp for an export suggestion, e.g. 20260921-1403.
std::string FormatReplayFileStamp(std::filesystem::file_time_type mtime) {
    const auto sysNow = std::chrono::system_clock::now();
    const auto fileNow = std::filesystem::file_time_type::clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::system_clock::duration>(
        mtime - fileNow);
    const std::time_t t = std::chrono::system_clock::to_time_t(sysNow + delta);
    const std::tm *tmp = std::localtime(&t);
    if (!tmp) return "replay";
    const std::tm tmv = *tmp;
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M", &tmv);
    return buf;
}

const char *ReplayOutcomeName(uint8_t outcome, bool complete) {
    switch (outcome) {
        case kReplayOutcomeWin:  return "Win";
        case kReplayOutcomeLoss: return "Loss";
        case kReplayOutcomeDraw: return "Draw";
        default: return complete ? "Ended" : "Incomplete";
    }
}

// mm:ss. A zero duration is R4a's "not populated" (CaptureRoundEnd writes 0),
// never a real 0:00 round, so it renders as "unknown" exactly as
// ReplayPlayer::TotalMs() treats it.
std::string FormatReplayDuration(uint32_t ms) {
    if (ms == 0) return "unknown";
    const uint32_t seconds = ms / 1000u;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u:%02u",
                  static_cast<unsigned>(seconds / 60u),
                  static_cast<unsigned>(seconds % 60u));
    return buf;
}

std::string ReplayRowLabel(const ReplayLibrary::ReplayEntry &e) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s  %s %dP  %s%s%s",
                  FormatReplayMtime(e.mtime).c_str(),
                  GameModeName(ClampGameMode(e.gameMode)),
                  static_cast<int>(e.playerCount),
                  ReplayOutcomeName(e.outcome, e.complete),
                  e.imported ? "  [Imported]" : "",
                  e.platformIncompatible ? "  [INCOMPATIBLE]" : "");
    return buf;
}

// menulist.cpp's TruncateToWidth is TU-local, so this repeats its (small,
// stable) logic rather than exporting it from a widget this page does not own.
std::string TruncateToWidth(SDL_Renderer *rend, TTFText &text,
                            const std::string &s, int maxW, int size, int style) {
    if (maxW <= 0) return "";
    text.UpdateStyle(size, style);
    text.UpdateText(rend, s.c_str(), 0);
    if (text.Coords()->w <= maxW) return s;
    std::string truncated = s;
    while (!truncated.empty()) {
        truncated.pop_back();
        const std::string candidate = truncated + "...";
        text.UpdateText(rend, candidate.c_str(), 0);
        if (text.Coords()->w <= maxW) return candidate;
    }
    return "...";
}

// A single small filled/outlined button with a centered label -- the same
// visual language as every confirm popup in this panel family, factored out
// because the list's row actions and the viewer's control bar both use it.
void DrawOverlayButton(SDL_Renderer *rend, TTFText &text, const SDL_Rect &r,
                       const char *label, bool gold, bool filled,
                       bool disabled = false) {
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 10, 38, 48, (filled && !disabled) ? 210 : 130);
    { SDL_FRect fr = ToFRect(r); SDL_RenderFillRect(rend, &fr); }
    const SDL_Color edge = (gold && !disabled) ? menulist::kGold : menulist::kMuted;
    SDL_SetRenderDrawColor(rend, edge.r, edge.g, edge.b,
                           disabled ? 90 : (gold ? 255 : 170));
    { SDL_FRect fr = ToFRect(r); SDL_RenderRect(rend, &fr); }
    text.UpdateStyle(13, TTF_STYLE_BOLD);
    text.UpdateColor(disabled ? menulist::kMuted
                              : (gold ? menulist::kGold : menulist::kText),
                     menulist::kTextShadow);
    text.UpdateText(rend, label, 0);
    text.UpdatePosition({r.x + r.w / 2 - text.Coords()->w / 2,
                         r.y + r.h / 2 - text.Coords()->h / 2});
    { SDL_FRect fr = ToFRect(*text.Coords()); SDL_RenderTexture(rend, text.Texture(), nullptr, &fr); }
}

} // namespace

int MainMenu::replaysExportRow() const {
    return static_cast<int>(replaysEntries.size());
}
int MainMenu::replaysImportRow() const {
    return static_cast<int>(replaysEntries.size()) + 1;
}
int MainMenu::replaysKeepCountRow() const {
    return static_cast<int>(replaysEntries.size()) + 2;
}

void MainMenu::SetReplaysStatus(const std::string &text) {
    replaysStatus = text;
    replaysStatusUntilMs = SDL_GetTicks() + kReplaysStatusMs;
}

void MainMenu::OpenReplaysPanel() {
    showingReplaysPanel = true;
    replaysSelection = 0;
    replaysActionIndex = 0;
    replaysStatus.clear();
    replaysStatusUntilMs = 0;
    replaysKeepCountPending = GameSettings::Instance()->replayKeepCount();
    showingReplayDeleteConfirm = false;
    showingReplayKeepCountConfirm = false;
    confirmDialogFocusNo = false;
    RefreshReplaysEntries();
    // Park the cursor on the first entry when there is one; an empty library
    // has no entries, so it starts on Export and the page still reads.
    if (replaysEntries.empty()) replaysSelection = replaysExportRow();
    PlayMenuSFX("menu_selected");
}

void MainMenu::RefreshReplaysEntries() {
    replaysEntries = ReplayLibrary::Instance()->List();
    ClampReplaysSelection();
}

void MainMenu::ClampReplaysSelection() {
    const int last = replaysKeepCountRow();
    replaysSelection = std::clamp(replaysSelection, 0, last);
    if (replaysActionIndex < 0 || replaysActionIndex > 1) replaysActionIndex = 0;
    // A platform-incompatible entry's Play action is dead, so keyboard/gamepad
    // focus must never rest on it: force it to Delete (a real, working action).
    if (replaysSelection >= 0 &&
        replaysSelection < static_cast<int>(replaysEntries.size()) &&
        replaysEntries[replaysSelection].platformIncompatible) {
        replaysActionIndex = 1;
    }
}

void MainMenu::CloseReplaysPanel() {
    showingReplaysPanel = false;
    showingReplayDeleteConfirm = false;
    showingReplayKeepCountConfirm = false;
    confirmDialogFocusNo = false;
    replayActionTaps.clear();
    replayPendingDeleteFilename.clear();
    replayPendingDeleteLabel.clear();
    // Uncommitted keep-count edits are discarded on the way out; only the
    // Apply action commits, so browsing can never change the setting.
    replaysKeepCountPending = GameSettings::Instance()->replayKeepCount();
    replaysEntries.clear();
    PlayMenuSFX("cancel");
}

void MainMenu::BeginReplayDeleteConfirm(int entry) {
    if (entry < 0 || entry >= static_cast<int>(replaysEntries.size())) return;
    replayPendingDeleteFilename = replaysEntries[entry].filename;
    // The raw file name is never player-facing; the row's own summary is what
    // the dialog names the entry by.
    replayPendingDeleteLabel = ReplayRowLabel(replaysEntries[entry]);
    showingReplayDeleteConfirm = true;
    confirmDialogFocusNo = false;
    PlayMenuSFX("menu_selected");
}

void MainMenu::ApplyReplaysKeepCount(int target) {
    target = std::clamp(target, 0, GameSettings::kReplayKeepCountMax);
    GameSettings::Instance()->SetReplayKeepCount(target);
    // Existing() rather than Instance(): production creates the recorder at
    // startup, and this must never bring one into being just to set a number.
    if (ReplayRecorder *recorder = ReplayRecorder::Existing()) {
        recorder->SetKeepCount(target);
    }
    ReplayLibrary::Instance()->ApplyKeepCount(target);
    replaysKeepCountPending = target;
    RefreshReplaysEntries();
    SetReplaysStatus("Replay keep count updated.");
}

void MainMenu::CommitReplaysKeepCount() {
    const int current = GameSettings::Instance()->replayKeepCount();
    const int target = std::clamp(replaysKeepCountPending, 0,
                                  GameSettings::kReplayKeepCountMax);
    if (target == current) return;

    if (target < current) {
        const int wouldDelete = ReplayLibrary::Instance()->EvictionCountFor(target);
        if (wouldDelete > 0) {
            // Deleting is irreversible and names an exact count, so it gets a
            // confirmation. Raising the count never reaches here.
            replaysKeepCountConfirmTarget = target;
            showingReplayKeepCountConfirm = true;
            confirmDialogFocusNo = false;
            PlayMenuSFX("menu_selected");
            return;
        }
    }
    ApplyReplaysKeepCount(target);
}

void MainMenu::ExportSelectedReplay() {
    const int entryCount = static_cast<int>(replaysEntries.size());
    if (replaysSelection < 0 || replaysSelection >= entryCount) {
        SetReplaysStatus("Select a replay to export first.");
        return;
    }
    const ReplayLibrary::ReplayEntry &entry = replaysEntries[replaysSelection];
    std::vector<uint8_t> bytes;
    if (!ReplayLibrary::Instance()->ReadBytes(entry.filename, bytes) || bytes.empty()) {
        SetReplaysStatus("Could not read that replay.");
        return;
    }
    const std::string suggested =
        "frozen-bubble-" + FormatReplayFileStamp(entry.mtime) + ".fbr";
    if (PlatformExportReplayFileBegin(const_cast<SDL_Renderer *>(renderer), suggested, bytes)) {
        replayExportPending = true;
        SetReplaysStatus("Waiting for you to choose a save location...");
    } else {
        // Begin() refuses both "no mechanism on this platform" and "an
        // operation is already in flight"; the return value alone cannot tell
        // the two apart, so this is one honest message covering both.
        SetReplaysStatus("Export is not available right now.");
    }
}

void MainMenu::ImportReplay() {
    if (PlatformImportReplayFileBegin(const_cast<SDL_Renderer *>(renderer))) {
        replayImportPending = true;
        SetReplaysStatus("Waiting for you to choose a file...");
    } else {
        SetReplaysStatus("Import is not available right now.");
    }
}

void MainMenu::PollReplayFileOps() {
    if (replayExportPending) {
        const PlatformFileOpStatus status = PlatformExportReplayFilePoll();
        if (status == PlatformFileOpStatus::Succeeded) {
            replayExportPending = false;
            SetReplaysStatus("Replay exported.");
        } else if (status == PlatformFileOpStatus::Cancelled) {
            replayExportPending = false;
            SetReplaysStatus("Export cancelled.");
        } else if (status == PlatformFileOpStatus::Failed) {
            replayExportPending = false;
            SetReplaysStatus("Export failed.");
        }
        // Pending: leave the "waiting" status showing, nothing else to do this frame.
    } else if (replayImportPending) {
        std::vector<uint8_t> bytes;
        std::string suggestedName;
        const PlatformFileOpStatus status =
            PlatformImportReplayFilePoll(bytes, suggestedName);
        if (status == PlatformFileOpStatus::Succeeded) {
            replayImportPending = false;
            // Peek the imported header before Write() consumes the bytes, so a
            // mismatched-platform file can be flagged in the status line. This
            // is the same lightweight header-only decode List()'s per-file
            // metadata pass uses. A decode failure here is not an error of its
            // own: List() handles a genuinely corrupt file when it lists it.
            ReplayHeader importedHeader;
            bool headerDecoded = false;
            if (!bytes.empty()) {
                ReplayReader reader(bytes);
                headerDecoded = reader.ReadHeader(importedHeader) == DecodeResult::Ok;
            }
            // Import is an explicit player action: always file the bytes so the
            // entry shows up (badged) and can be deleted, even when it cannot
            // be played here.
            if (bytes.empty() || !ReplayLibrary::Instance()->Write(std::move(bytes))) {
                SetReplaysStatus("That replay could not be imported.");
            } else {
                RefreshReplaysEntries();
                if (headerDecoded &&
                    !IsReplayPlatformCompatible(importedHeader.platformFloatProfile)) {
                    SetReplaysStatus("Imported, but this replay was recorded on a "
                                     "different platform and can't be played here.");
                } else {
                    SetReplaysStatus("Replay imported.");
                }
            }
        } else if (status == PlatformFileOpStatus::Cancelled) {
            replayImportPending = false;
            SetReplaysStatus("Import cancelled.");
        } else if (status == PlatformFileOpStatus::Failed) {
            replayImportPending = false;
            SetReplaysStatus("Import failed.");
        }
    }
}

void MainMenu::ActivateReplayRow() {
    // An export/import dialog is already in flight: ignore a repeat activation
    // rather than letting Begin() silently refuse and flashing a status line.
    if ((replaysSelection == replaysExportRow() || replaysSelection == replaysImportRow()) &&
        (replayExportPending || replayImportPending)) {
        return;
    }
    const int entryCount = static_cast<int>(replaysEntries.size());
    if (replaysSelection >= 0 && replaysSelection < entryCount) {
        if (replaysActionIndex == 0) {
            // Belt-and-suspenders: ClampReplaysSelection() keeps focus off the
            // dead Play action for a platform-incompatible entry, but never
            // start playback even if some path reaches here anyway.
            if (!replaysEntries[replaysSelection].platformIncompatible)
                BeginReplayPlayback(replaysSelection);
        } else {
            BeginReplayDeleteConfirm(replaysSelection);
        }
        return;
    }
    if (replaysSelection == replaysExportRow()) ExportSelectedReplay();
    else if (replaysSelection == replaysImportRow()) ImportReplay();
    else if (replaysSelection == replaysKeepCountRow()) CommitReplaysKeepCount();
}

void MainMenu::BeginReplayPlayback(int entry) {
    if (entry < 0 || entry >= static_cast<int>(replaysEntries.size())) return;
    if (replaysEntries[entry].platformIncompatible) {
        SetReplaysStatus("This replay was recorded on a different platform and "
                         "can't be played here.");
        return;
    }
    std::vector<uint8_t> bytes;
    if (!ReplayLibrary::Instance()->ReadBytes(replaysEntries[entry].filename, bytes) ||
        bytes.empty()) {
        SetReplaysStatus("Could not read that replay.");
        return;
    }
    if (!replayPlayer) replayPlayer.reset(new ReplayPlayer());
    if (!replayPlayer->Load(bytes, renderer)) {
        // A corrupt file that slipped past List()'s lighter validation, or a
        // filesystem race. Never open a blank playback screen: report and stay
        // on the list.
        replayPlayer.reset();
        SetReplaysStatus("This replay could not be played (corrupt or unsupported).");
        return;
    }
    replayPlayer->SetPaused(false);
    replayPlayer->SetSpeed(1.0f);
    playingReplay = true;
    replayPlaybackFocus = 1;  // Pause, the most likely first action
    PlayMenuSFX("menu_selected");
}

void MainMenu::ExitReplayPlayback() {
    playingReplay = false;
    replayPlayer.reset();
    // Back to the library list, refreshed so a file changed underneath us is
    // reflected. Never back to the title screen.
    RefreshReplaysEntries();
    PlayMenuSFX("cancel");
}

void MainMenu::DrawReplayConfirmDialog(SDL_Renderer *rend, const char *title,
                                       const char *body, const char *yesLabel,
                                       bool danger) {
    SDL_Rect box = {(640 / 2) - 155, (480 / 2) - 96, 310, 192};
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, menulist::kHeaderFill.r, menulist::kHeaderFill.g,
                           menulist::kHeaderFill.b, 245);
    { SDL_FRect fr = ToFRect(box); SDL_RenderFillRect(rend, &fr); }
    SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g,
                           menulist::kEdge.b, menulist::kEdge.a);
    { SDL_FRect fr = ToFRect(box); SDL_RenderRect(rend, &fr); }

    panelText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    panelText.UpdateStyle(15, TTF_STYLE_BOLD);
    panelText.UpdateColor(danger ? menulist::kBad : menulist::kGold,
                          menulist::kTextShadow);
    panelText.UpdateText(rend, title, 290);
    panelText.UpdatePosition({box.x + box.w / 2 - panelText.Coords()->w / 2, box.y + 12});
    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

    panelText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_LEFT);
    panelText.UpdateStyle(13, TTF_STYLE_NORMAL);
    panelText.UpdateColor(menulist::kText, menulist::kTextShadow);
    panelText.UpdateText(rend, body, 286);
    panelText.UpdatePosition({box.x + 12, box.y + 42});
    { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

    replayConfirmYesRect = {box.x + 12, box.y + box.h - 34, 135, 26};
    replayConfirmNoRect = {box.x + box.w - 12 - 135, box.y + box.h - 34, 135, 26};
    // The focused choice is drawn gold, the other muted -- CLAUDE.md's visible
    // focus for a two-choice control.
    const bool yesFocused = !confirmDialogFocusNo;
    auto dialogButton = [&](const SDL_Rect &r, const char *label, bool focused) {
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 10, 38, 48, 210);
        { SDL_FRect fr = ToFRect(r); SDL_RenderFillRect(rend, &fr); }
        const SDL_Color edge = focused ? menulist::kGold : menulist::kMuted;
        SDL_SetRenderDrawColor(rend, edge.r, edge.g, edge.b, 255);
        { SDL_FRect fr = ToFRect(r); SDL_RenderRect(rend, &fr); }
        panelText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_LEFT);
        panelText.UpdateStyle(14, TTF_STYLE_BOLD);
        panelText.UpdateColor(focused ? menulist::kGold : menulist::kText,
                              menulist::kTextShadow);
        panelText.UpdateText(rend, label, 0);
        panelText.UpdatePosition({r.x + r.w / 2 - panelText.Coords()->w / 2,
                                  r.y + r.h / 2 - panelText.Coords()->h / 2});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
    };
    dialogButton(replayConfirmYesRect, yesLabel, yesFocused);
    dialogButton(replayConfirmNoRect, "Cancel", confirmDialogFocusNo);
    panelText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
}

void MainMenu::ReplaysPanelRender() {
    if (!showingReplaysPanel) return;

    // Before anything below reads replaysStatus: a finished platform file
    // operation updates the status/pending flags here.
    PollReplayFileOps();

    // Keep the invariant ClampReplaysSelection() enforces (a
    // platform-incompatible entry's focus never rests on the dead Play action)
    // even when the row was selected by a tap, which sets replaysSelection
    // through the generic panel-tap loop rather than this page's key handler.
    ClampReplaysSelection();

    SDL_Renderer *rend = const_cast<SDL_Renderer *>(renderer);
    menulist::DrawWorldMapBackdrop(rend, netGameBackground);

    BeginPanelTapRows(&replaysSelection, &replaysActionIndex);
    auto tap = [&](int index, const SDL_Rect &rect, int subIndex,
                   bool splitAdjust, SDL_Keycode key) {
        AddPanelTapRow(index, rect, subIndex, splitAdjust, key);
    };

    replayActionTaps.clear();
    replaysDoneRect = SDL_Rect{};
    replayConfirmYesRect = replayConfirmNoRect = SDL_Rect{};
    menulist::DrawHeaderBar(rend, panelText, menulist::kHeaderBar, "REPLAYS",
                            "Done", true, 0,
                            [&](int, const SDL_Rect &r, int, bool, SDL_Keycode) {
                                replaysDoneRect = r;
                            },
                            menulist::kMapFillAlpha);

    const int entryCount = static_cast<int>(replaysEntries.size());

    // --- the scrolling entry list (custom, so every row can carry buttons) --
    constexpr int kEntryRowH = 30;
    const SDL_Rect listViewport = {10, 44, 404, 244};
    // The pinned library section sits below the scroll area, the same way the
    // server list pins "Set name" / "Community" outside its own scrolling rows
    // rather than letting them ride off the bottom of a long list.
    const SDL_Rect pinnedViewport = {10, 296, 404, 128};

    SDL_Color listFill = menulist::kListFill;
    listFill.a = menulist::kMapFillAlpha;
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, listFill.r, listFill.g, listFill.b, listFill.a);
    { SDL_FRect fr = ToFRect(listViewport); SDL_RenderFillRect(rend, &fr); }
    SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g,
                           menulist::kEdge.b, menulist::kEdge.a);
    { SDL_FRect fr = ToFRect(listViewport); SDL_RenderRect(rend, &fr); }

    const int visibleRows = std::max(1, listViewport.h / kEntryRowH);
    const int entrySel = (replaysSelection >= 0 && replaysSelection < entryCount)
                             ? replaysSelection : -1;
    const int maxScroll = std::max(0, entryCount - visibleRows);
    int scrollTop = 0;
    if (entrySel >= 0) scrollTop = std::clamp(entrySel - 1, 0, maxScroll);
    else scrollTop = std::clamp(entryCount - 1, 0, maxScroll);

    SDL_Rect clip = listViewport;
    SDL_SetRenderClipRect(rend, &clip);

    if (entryCount == 0) {
        panelText.UpdateStyle(14, TTF_STYLE_NORMAL);
        panelText.UpdateColor(menulist::kMuted, menulist::kTextShadow);
        panelText.UpdateText(rend, "No replays yet -- finish a round and it will appear here.", 0);
        panelText.UpdatePosition({listViewport.x + 14, listViewport.y + 10});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
    }

    const int shownLast = std::min(entryCount, scrollTop + visibleRows);
    for (int i = scrollTop; i < shownLast; ++i) {
        const ReplayLibrary::ReplayEntry &entry = replaysEntries[i];
        const int y = listViewport.y + (i - scrollTop) * kEntryRowH;
        const bool selected = (replaysSelection == i);

        if (selected) {
            SDL_Rect sel = {listViewport.x + 4, y + 2, listViewport.w - 8, kEntryRowH - 4};
            SDL_SetRenderDrawColor(rend, menulist::kSelFill.r, menulist::kSelFill.g,
                                   menulist::kSelFill.b, menulist::kSelFill.a);
            { SDL_FRect fr = ToFRect(sel); SDL_RenderFillRect(rend, &fr); }
            SDL_SetRenderDrawColor(rend, menulist::kSelEdge.r, menulist::kSelEdge.g,
                                   menulist::kSelEdge.b, menulist::kSelEdge.a);
            { SDL_FRect fr = ToFRect(sel); SDL_RenderRect(rend, &fr); }
        }

        const int btnH = kEntryRowH - 8;
        const SDL_Rect deleteRect = {listViewport.x + listViewport.w - 10 - 62,
                                     y + 4, 62, btnH};
        const SDL_Rect playRect = {deleteRect.x - 6 - 48, y + 4, 48, btnH};
        // A platform-incompatible entry's Play is inert: never gold/highlighted
        // even when focused, and never registered as a tap target. Delete stays
        // fully live so the file can still be removed.
        const bool playDisabled = entry.platformIncompatible;
        DrawOverlayButton(rend, panelText, playRect, "Play",
                          !playDisabled && selected && replaysActionIndex == 0,
                          selected && !playDisabled, playDisabled);
        DrawOverlayButton(rend, panelText, deleteRect, "Delete",
                          selected && replaysActionIndex == 1, selected);
        if (!playDisabled) replayActionTaps.push_back({playRect, i, 0});
        replayActionTaps.push_back({deleteRect, i, 1});

        const int labelMaxW = playRect.x - (listViewport.x + 10) - 6;
        const std::string label = TruncateToWidth(
            rend, panelText, ReplayRowLabel(entry), labelMaxW, 14, TTF_STYLE_NORMAL);
        panelText.UpdateStyle(14, TTF_STYLE_NORMAL);
        panelText.UpdateColor(selected ? menulist::kGold : menulist::kText,
                              menulist::kTextShadow);
        panelText.UpdateText(rend, label.c_str(), 0);
        panelText.UpdatePosition({listViewport.x + 10,
                                  y + (kEntryRowH - panelText.Coords()->h) / 2});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }

        // The whole row is a tap target so a tap on the label selects it, the
        // same select-then-activate dance every other row in this panel family
        // uses. The action buttons above are hit-tested first, so a tap on one
        // of those never reaches here.
        AddPanelTapRow(i, {listViewport.x, y, listViewport.w, kEntryRowH}, -1, false, 0);
    }
    SDL_SetRenderClipRect(rend, nullptr);

    if (entryCount > visibleRows) {
        const int trackX = listViewport.x + listViewport.w - 10;
        const int trackY = listViewport.y + 6;
        const int trackH = listViewport.h - 12;
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 255, 255, 255, 40);
        SDL_FRect trackRect = {(float)trackX, (float)trackY, 4.0f, (float)trackH};
        SDL_RenderFillRect(rend, &trackRect);
        const int thumbH = std::max(16, trackH * visibleRows / entryCount);
        const int thumbY = trackY +
            (maxScroll > 0 ? (trackH - thumbH) * scrollTop / maxScroll : 0);
        SDL_SetRenderDrawColor(rend, menulist::kEdge.r, menulist::kEdge.g,
                               menulist::kEdge.b, 210);
        SDL_FRect thumbRect = {(float)trackX, (float)thumbY, 4.0f, (float)thumbH};
        SDL_RenderFillRect(rend, &thumbRect);
    }

    // --- the pinned library section -----------------------------------------
    menulist::List pinned(pinnedViewport, replaysSelection, menulist::kRowH,
                          menulist::kMapFillAlpha);
    pinned.Header("Library");
    pinned.Row(replaysExportRow(), "Export selected replay",
               entryCount > 0 ? "save a copy" : "", entryCount > 0);
    pinned.Row(replaysImportRow(), "Import replay...", "");
    // splitAdjust registers the "<  N  >" left/right tap halves; the "Apply"
    // suffix is its own tap target sending RETURN, which is the only thing
    // that commits the pending value (so stepping past a lower count deletes
    // nothing until the player asks for it). Same shape as the local
    // multiplayer panel's HELP suffix.
    pinned.Row(replaysKeepCountRow(), "Replay keep count",
               std::to_string(replaysKeepCountPending), true, true, 0,
               SDLK_RETURN, "Apply");
    pinned.End(rend, panelText, nullptr, tap);

    // --- sidebar: details for whatever is focused ---------------------------
    int sy = menulist::DrawSidebarHeader(rend, panelText, menulist::kSidebarFull,
                                         "Replay", menulist::kMapFillAlpha);
    const SDL_Rect &sb = menulist::kSidebarFull;
    auto sidebarLine = [&](const std::string &txt, SDL_Color color, int size = 14) {
        panelText.UpdateStyle(size, TTF_STYLE_NORMAL);
        panelText.UpdateColor(color, menulist::kTextShadow);
        panelText.UpdateText(rend, txt.c_str(), sb.w - 24);
        panelText.UpdatePosition({sb.x + 12, sy});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
        sy += size + 8;
    };

    if (replaysSelection >= 0 && replaysSelection < entryCount) {
        const ReplayLibrary::ReplayEntry &entry = replaysEntries[replaysSelection];
        sidebarLine(FormatReplayMtime(entry.mtime), menulist::kText, 16);
        sidebarLine(std::string(GameModeName(ClampGameMode(entry.gameMode))) + "   " +
                    std::to_string(static_cast<int>(entry.playerCount)) + " players",
                    menulist::kText);
        sidebarLine("Duration: " + FormatReplayDuration(entry.durationMs), menulist::kMuted);
        sidebarLine(std::string("Outcome: ") +
                    ReplayOutcomeName(entry.outcome, entry.complete), menulist::kMuted);
        sidebarLine(entry.complete ? "Complete" : "Incomplete",
                    entry.complete ? menulist::kMuted : menulist::kBad);
        if (entry.imported) sidebarLine("Imported file", menulist::kGold);
        if (entry.platformIncompatible)
            sidebarLine("Incompatible platform", menulist::kBad);
        sidebarLine("Play or Delete with the", menulist::kMuted, 12);
        sidebarLine("buttons on the row.", menulist::kMuted, 12);
    } else if (replaysSelection == replaysKeepCountRow()) {
        sidebarLine("Replay keep count", menulist::kText, 15);
        sidebarLine("How many finished rounds", menulist::kMuted, 13);
        sidebarLine("the library keeps. 0 stops", menulist::kMuted, 13);
        sidebarLine("future recording; replays", menulist::kMuted, 13);
        sidebarLine("already saved are kept.", menulist::kMuted, 13);
        sidebarLine("ENTER or Apply commits it.", menulist::kMuted, 12);
    } else if (replaysSelection == replaysExportRow()) {
        sidebarLine("Export selected", menulist::kText, 15);
        sidebarLine("Save a copy of the selected", menulist::kMuted, 13);
        sidebarLine("replay as a .fbr file you", menulist::kMuted, 13);
        sidebarLine("can share. The original", menulist::kMuted, 13);
        sidebarLine("stays in the library.", menulist::kMuted, 13);
    } else {
        sidebarLine("Import replay", menulist::kText, 15);
        sidebarLine("Open a .fbr file and file", menulist::kMuted, 13);
        sidebarLine("a copy into the library.", menulist::kMuted, 13);
        sidebarLine("Imported files count", menulist::kMuted, 13);
        sidebarLine("toward the keep count.", menulist::kMuted, 13);
    }

    // Transient status: the same non-interactive line R4a established for
    // "Saved to Replays" -- it draws no differently from any other text, so it
    // needs no tap target or footer of its own.
    const Uint32 now = SDL_GetTicks();
    if (!replaysStatus.empty() && replaysStatusUntilMs != 0 && now <= replaysStatusUntilMs) {
        panelText.UpdateStyle(13, TTF_STYLE_BOLD);
        panelText.UpdateColor(menulist::kGold, menulist::kTextShadow);
        panelText.UpdateText(rend, replaysStatus.c_str(), 0);
        const int w = panelText.Coords()->w;
        SDL_Rect box = {320 - w / 2 - 8, 426, w + 16, 20};
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rend, 0, 0, 0, 180);
        { SDL_FRect fr = ToFRect(box); SDL_RenderFillRect(rend, &fr); }
        panelText.UpdatePosition({320 - w / 2, 428});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
    }

    // Confirm dialogs paint last so they cover the page underneath.
    if (showingReplayDeleteConfirm) {
        char body[320];
        std::snprintf(body, sizeof(body),
                      "Delete this replay?\n\n%s\n\nThis cannot be undone.",
                      TruncateToWidth(rend, panelText, replayPendingDeleteLabel, 260, 13,
                                      TTF_STYLE_NORMAL).c_str());
        DrawReplayConfirmDialog(rend, "Delete replay?", body, "Delete", true);
    } else if (showingReplayKeepCountConfirm) {
        const int target = replaysKeepCountConfirmTarget;
        const int count = ReplayLibrary::Instance()->EvictionCountFor(target);
        char title[64];
        std::snprintf(title, sizeof(title), "Delete %d replay%s?", count,
                      count == 1 ? "" : "s");
        char body[288];
        std::snprintf(body, sizeof(body),
                      "Lowering the keep count to %d will\ndelete %d saved replay%s "
                      "right now.\n\nThis cannot be undone.",
                      target, count, count == 1 ? "" : "s");
        DrawReplayConfirmDialog(rend, title, body, "Delete", true);
    }

    const bool confirmShowing = showingReplayDeleteConfirm || showingReplayKeepCountConfirm;
    menulist::DrawFooterHint(rend, panelText,
        confirmShowing ? "LEFT/RIGHT choose   ENTER select   ESC cancel"
                       : "UP/DOWN move   LEFT/RIGHT change   ENTER select/apply   ESC done");
}

bool MainMenu::ReplaysPanelKey(SDL_Event *e) {
    if (!showingReplaysPanel) return false;

    // The two confirm dialogs are modal over the list and share the panel
    // family's focus flag. LEFT/RIGHT/TAB moves the highlight, ENTER activates
    // whichever button is focused, ESC/AC_BACK/other-ENTER cancels outright.
    if (showingReplayDeleteConfirm || showingReplayKeepCountConfirm) {
        if (e->key.key == SDLK_LEFT || e->key.key == SDLK_RIGHT || e->key.key == SDLK_TAB) {
            confirmDialogFocusNo = !confirmDialogFocusNo;
            PlayMenuSFX("menu_change");
        } else if (e->key.key == SDLK_RETURN && !confirmDialogFocusNo) {
            if (showingReplayDeleteConfirm) {
                const std::string filename = replayPendingDeleteFilename;
                showingReplayDeleteConfirm = false;
                confirmDialogFocusNo = false;
                if (!filename.empty() && ReplayLibrary::Instance()->Delete(filename)) {
                    RefreshReplaysEntries();
                    SetReplaysStatus("Replay deleted.");
                } else {
                    SetReplaysStatus("Could not delete that replay.");
                }
                PlayMenuSFX("typewriter");
            } else {
                showingReplayKeepCountConfirm = false;
                confirmDialogFocusNo = false;
                ApplyReplaysKeepCount(replaysKeepCountConfirmTarget);
                PlayMenuSFX("typewriter");
            }
        } else if (e->key.key == SDLK_RETURN || e->key.key == SDLK_ESCAPE ||
                   e->key.key == SDLK_AC_BACK) {
            // Cancelled: the keep-count pending value reverts to the persisted
            // setting so the row cannot show a value that was never applied.
            showingReplayDeleteConfirm = false;
            showingReplayKeepCountConfirm = false;
            confirmDialogFocusNo = false;
            replaysKeepCountPending = GameSettings::Instance()->replayKeepCount();
            PlayMenuSFX("menu_change");
        }
        return true;
    }

    const int entryCount = static_cast<int>(replaysEntries.size());
    const int lastRow = replaysKeepCountRow();
    switch (e->key.key) {
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
            CloseReplaysPanel();
            return true;
        case SDLK_UP:
            replaysSelection = (replaysSelection <= 0) ? lastRow : replaysSelection - 1;
            ClampReplaysSelection();
            PlayMenuSFX("menu_change");
            return true;
        case SDLK_DOWN:
            replaysSelection = (replaysSelection >= lastRow) ? 0 : replaysSelection + 1;
            ClampReplaysSelection();
            PlayMenuSFX("menu_change");
            return true;
        case SDLK_LEFT:
        case SDLK_RIGHT: {
            const bool forward = (e->key.key == SDLK_RIGHT);
            if (replaysSelection >= 0 && replaysSelection < entryCount) {
                // Entry rows have two actions; the focused one is drawn gold.
                // ClampReplaysSelection() then keeps a platform-incompatible
                // entry's focus off the dead Play action.
                replaysActionIndex = replaysActionIndex == 0 ? 1 : 0;
                ClampReplaysSelection();
                PlayMenuSFX("menu_change");
            } else if (replaysSelection == replaysKeepCountRow()) {
                int next = replaysKeepCountPending + (forward ? 1 : -1);
                next = std::clamp(next, 0, GameSettings::kReplayKeepCountMax);
                if (next != replaysKeepCountPending) {
                    replaysKeepCountPending = next;
                    PlayMenuSFX("menu_change");
                }
            }
            return true;
        }
        case SDLK_RETURN:
            ActivateReplayRow();
            return true;
        default:
            // Modal: nothing behind this page sees a key while it is open.
            return true;
    }
}

bool MainMenu::HandleReplaysPanelTap(float lx, float ly) {
    if (!showingReplaysPanel) return false;

    auto hit = [&](const SDL_Rect &r) {
        return r.w > 0 && r.h > 0 && lx >= r.x && lx < r.x + r.w &&
               ly >= r.y && ly < r.y + r.h;
    };

    // Confirm dialogs first, and consuming every tap while they show -- a miss
    // must not reach the row list behind them.
    if (showingReplayDeleteConfirm || showingReplayKeepCountConfirm) {
        SDL_Keycode key = SDLK_UNKNOWN;
        if (hit(replayConfirmYesRect)) { confirmDialogFocusNo = false; key = SDLK_RETURN; }
        else if (hit(replayConfirmNoRect)) key = SDLK_ESCAPE;
        if (key != SDLK_UNKNOWN) {
            SDL_Event ev = {};
            ev.type = SDL_EVENT_KEY_DOWN;
            ev.key.key = key;
            SDL_PushEvent(&ev);
        }
        return true;
    }

    if (hit(replaysDoneRect)) {
        CloseReplaysPanel();
        return true;
    }

    // Per-row Play/Delete buttons: a direct single tap activates, the way the
    // team picker's swatches do -- a full-width button is not the easy-to-miss
    // target the select-then-activate dance exists for.
    for (const ReplayActionTap &button : replayActionTaps) {
        if (!hit(button.rect)) continue;
        replaysSelection = button.entry;
        replaysActionIndex = button.action;
        if (button.action == 0) BeginReplayPlayback(button.entry);
        else BeginReplayDeleteConfirm(button.entry);
        return true;
    }

    // Not one of this page's own buttons: fall through to the generic
    // panelTapRows loop, which focuses/activates the registered rows (entry
    // bodies, and the pinned Export/Import/keep-count rows).
    return false;
}

// --- playback viewer --------------------------------------------------------

void MainMenu::ReplayPlaybackRender() {
    if (!replayPlayer) return;
    replayPlayer->Draw();
    SDL_Renderer *rend = const_cast<SDL_Renderer *>(renderer);

    const bool ended = replayPlayer->IsFinished() || replayPlayer->IsDesynced();

    // Control bar across the bottom, above the footer hint.
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 165);
    { SDL_FRect fr = {0.f, 396.f, 640.f, 56.f}; SDL_RenderFillRect(rend, &fr); }

    // Two narrower shot-navigation buttons flank the existing 4-button row,
    // keeping the whole 6-button row centered inside the control bar. The main
    // four keep the exact same rects they had before the flanking buttons were
    // added (x0 == 116).
    const int btnH = 26, y = 404, gap = 8, btnW = 96, shotBtnW = 84;
    const int rowW = 2 * shotBtnW + 4 * btnW + 5 * gap;
    const int rowX = (640 - rowW) / 2;
    replayPrevShotRect = {rowX, y, shotBtnW, btnH};
    replayPauseRect = {rowX + shotBtnW + gap, y, btnW, btnH};
    replaySpeedRect = {rowX + shotBtnW + gap + (btnW + gap), y, btnW, btnH};
    replayRestartRect = {rowX + shotBtnW + gap + 2 * (btnW + gap), y, btnW, btnH};
    replayExitRect = {rowX + shotBtnW + gap + 3 * (btnW + gap), y, btnW, btnH};
    replayNextShotRect = {rowX + shotBtnW + gap + 4 * (btnW + gap), y, shotBtnW, btnH};

    const bool paused = replayPlayer->IsPaused();
    // Each button draws gold when it currently holds keyboard/gamepad focus
    // (replayPlaybackFocus), same visual language confirmDialogFocusNo uses
    // elsewhere in this panel family -- so UP/DOWN+ENTER navigation has a
    // visible cursor, not just an inferred one.
    DrawOverlayButton(rend, panelText, replayPrevShotRect, "< Shot",
                      replayPlaybackFocus == 0, true);
    DrawOverlayButton(rend, panelText, replayPauseRect, paused ? "Play" : "Pause",
                      replayPlaybackFocus == 1, true);
    char speedLabel[16];
    std::snprintf(speedLabel, sizeof(speedLabel), "Speed %gx",
                  static_cast<double>(replayPlayer->Speed()));
    DrawOverlayButton(rend, panelText, replaySpeedRect, speedLabel,
                      replayPlaybackFocus == 2, true);
    DrawOverlayButton(rend, panelText, replayRestartRect, "Restart",
                      replayPlaybackFocus == 3, true);
    // Exit also draws gold once the recording has ended, so the way back is
    // the thing that stands out exactly when a player needs it.
    DrawOverlayButton(rend, panelText, replayExitRect, "Exit",
                      ended || replayPlaybackFocus == 4, true);
    DrawOverlayButton(rend, panelText, replayNextShotRect, "Shot >",
                      replayPlaybackFocus == 5, true);

    // A finished or desynced recording does not auto-exit -- the player needs
    // an explicit way back -- but it is labelled so they know ENTER works.
    if (ended) {
        const char *note = replayPlayer->IsDesynced()
            ? "Replay desynced -- ENTER or Exit to return"
            : "Replay finished -- ENTER or Exit to return";
        panelText.UpdateStyle(14, TTF_STYLE_BOLD);
        panelText.UpdateColor(replayPlayer->IsDesynced() ? menulist::kBad : menulist::kGold,
                              menulist::kTextShadow);
        panelText.UpdateText(rend, note, 0);
        panelText.UpdatePosition({320 - panelText.Coords()->w / 2, 370});
        { SDL_FRect fr = ToFRect(*panelText.Coords()); SDL_RenderTexture(rend, panelText.Texture(), nullptr, &fr); }
    }

    menulist::DrawFooterHint(rend, panelText,
        "L/R+ENTER select   UP/DN speed   SPACE pause   R restart   ESC exit");
}

// Fires whichever control replayPlaybackFocus is currently on -- the same
// action ENTER takes, and what a tap on that same button does. Shared so
// keyboard/gamepad-focus activation and the direct hotkeys stay in sync.
void MainMenu::ActivateReplayPlaybackFocus() {
    switch (replayPlaybackFocus) {
        case 0:
            replayPlayer->SeekToPreviousShot();
            PlayMenuSFX("menu_change");
            break;
        case 1:
            replayPlayer->SetPaused(!replayPlayer->IsPaused());
            PlayMenuSFX("menu_change");
            break;
        case 2: {
            int index = 0;
            for (int i = 0; i < (int)std::size(kReplaySpeeds); ++i) {
                if (std::fabs(replayPlayer->Speed() - kReplaySpeeds[i]) < 0.001f) {
                    index = i;
                    break;
                }
            }
            replayPlayer->SetSpeed(kReplaySpeeds[(index + 1) % (int)std::size(kReplaySpeeds)]);
            PlayMenuSFX("menu_change");
            break;
        }
        case 3:
            replayPlayer->Restart();
            PlayMenuSFX("menu_selected");
            break;
        case 4:
            ExitReplayPlayback();
            break;
        case 5:
            replayPlayer->SeekToNextShot();
            PlayMenuSFX("menu_change");
            break;
        default:
            break;
    }
}

bool MainMenu::ReplayPlaybackKey(SDL_Event *e) {
    if (!playingReplay || !replayPlayer) return false;

    switch (e->key.key) {
        case SDLK_ESCAPE:
        case SDLK_AC_BACK:
            ExitReplayPlayback();
            return true;
        case SDLK_LEFT:
        case SDLK_RIGHT: {
            // Horizontal row of buttons -> horizontal keys move the cursor
            // along it, matching the confirmDialogFocusNo convention this
            // panel family already uses for LEFT/RIGHT-moves-focus.
            const int count = 6;  // PrevShot, Pause, Speed, Restart, Exit, NextShot
            replayPlaybackFocus = (e->key.key == SDLK_RIGHT)
                ? (replayPlaybackFocus + 1) % count
                : (replayPlaybackFocus + count - 1) % count;
            PlayMenuSFX("menu_change");
            return true;
        }
        case SDLK_SPACE:
            replayPlayer->SetPaused(!replayPlayer->IsPaused());
            PlayMenuSFX("menu_change");
            return true;
        case SDLK_UP:
        case SDLK_DOWN: {
            // UP/DOWN adjusts the currently-focused control's value -- today
            // that's only the Speed button; on any other button this is a
            // no-op, same as LEFT/RIGHT doing nothing on a non-adjustable
            // row elsewhere in this menu family.
            if (replayPlaybackFocus != 2) return true;
            int index = 0;
            for (int i = 0; i < (int)std::size(kReplaySpeeds); ++i) {
                if (std::fabs(replayPlayer->Speed() - kReplaySpeeds[i]) < 0.001f) {
                    index = i;
                    break;
                }
            }
            const int count = (int)std::size(kReplaySpeeds);
            index = (e->key.key == SDLK_UP) ? (index + 1) % count
                                            : (index + count - 1) % count;
            replayPlayer->SetSpeed(kReplaySpeeds[index]);
            PlayMenuSFX("menu_change");
            return true;
        }
        case SDLK_R:
            replayPlayer->Restart();
            PlayMenuSFX("menu_selected");
            return true;
        case SDLK_COMMA:
            replayPlayer->SeekToPreviousShot();
            PlayMenuSFX("menu_change");
            return true;
        case SDLK_PERIOD:
            replayPlayer->SeekToNextShot();
            PlayMenuSFX("menu_change");
            return true;
        case SDLK_RETURN:
            // Once the recording has ended, ENTER always exits -- consistent
            // with the on-screen note below, regardless of which button
            // happens to have focus. Otherwise it activates whatever the
            // UP/DOWN cursor is currently on.
            if (replayPlayer->IsFinished() || replayPlayer->IsDesynced()) {
                ExitReplayPlayback();
            } else {
                ActivateReplayPlaybackFocus();
            }
            return true;
        default:
            // Modal: the whole viewer owns the keyboard while it is up.
            return true;
    }
}

bool MainMenu::HandleReplayPlaybackTap(float lx, float ly) {
    if (!playingReplay || !replayPlayer) return false;

    auto hit = [&](const SDL_Rect &r) {
        return r.w > 0 && r.h > 0 && lx >= r.x && lx < r.x + r.w &&
               ly >= r.y && ly < r.y + r.h;
    };

    // A tap also moves the keyboard/gamepad focus cursor onto the button it
    // hit, so a mixed tap-then-keyboard session stays consistent with what's
    // drawn highlighted (same ordering as ReplayPlaybackRender's rects: 0=
    // PrevShot, 1=Pause, 2=Speed, 3=Restart, 4=Exit, 5=NextShot).
    if (hit(replayPrevShotRect)) { replayPlaybackFocus = 0; ActivateReplayPlaybackFocus(); return true; }
    if (hit(replayPauseRect))    { replayPlaybackFocus = 1; ActivateReplayPlaybackFocus(); return true; }
    if (hit(replaySpeedRect))    { replayPlaybackFocus = 2; ActivateReplayPlaybackFocus(); return true; }
    if (hit(replayRestartRect))  { replayPlaybackFocus = 3; ActivateReplayPlaybackFocus(); return true; }
    if (hit(replayExitRect))     { replayPlaybackFocus = 4; ActivateReplayPlaybackFocus(); return true; }
    if (hit(replayNextShotRect)) { replayPlaybackFocus = 5; ActivateReplayPlaybackFocus(); return true; }

    // Tap-anywhere-to-return only once the recording has ended: a stray tap
    // must not end a replay the player is still watching.
    if (replayPlayer->IsFinished() || replayPlayer->IsDesynced()) {
        ExitReplayPlayback();
        return true;
    }
    // Modal: a miss is consumed, never passed to whatever the menu was
    // showing before playback started.
    return true;
}
