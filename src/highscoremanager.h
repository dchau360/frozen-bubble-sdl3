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

#ifndef HIGHSCOREMANAGER_H
#define HIGHSCOREMANAGER_H

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <vector>
#include <array>
#include <map>
#include <string>

#include "gamesettings.h"
#include "ttftext.h"

#define TEXTANIM_TICKSPEED 2

class HighscoreManager final
{
public:
    // A run is locked to whichever input fired its first shot (see
    // BubbleGame::ScoringInputMethod, which this mirrors) and counts toward
    // that table only -- keyboard/gamepad and mouse/touch scores are not
    // comparable to each other, so each keeps its own top 10. The screen can
    // still show both at once (ShowsBoth()), merged; every entry shown
    // carries its input's badge either way.
    enum class InputMethod { Keyboard = 0, Mouse = 1 };

    void ShowScoreScreen(int ls);
    void ShowNewScorePanel(int mode);
    void RenderPanel(void);
    void RenderScoreScreen(void);
    void HandleInput(SDL_Event *e);
    int lastState;

    void AppendToLevels(std::array<std::vector<int>, 10> lvl, int id);
    // Returns true if this is a new high score in `method`'s own table.
    bool CheckAndAddScore(int level, float time, InputMethod method);

    HighscoreManager(const HighscoreManager& obj) = delete;
    void Dispose();
    static HighscoreManager* Instance(SDL_Renderer *rend = nullptr);
private:
    int curMode = 0;  // 0 = levelset tables; ShowNewScorePanel() sets it

    GameSettings *gameSettings;
    SDL_Renderer *rend;

    std::map<int, std::array<std::vector<int>,10>> highscoreLevels;

    // Which of the two tables the score screen shows is a pair of toggles kept
    // in GameSettings::scoreTracks() (bit 0 keyboard/gamepad, bit 1 mouse/
    // touch; both on merges them; each entry has an input badge). Tapping a
    // tab toggles it; LEFT/RIGHT cycles KEYBOARD -> MOUSE/TOUCH -> BOTH.
    // pendingHighscoreTrack is the table the most recent CheckAndAddScore()
    // added a pending (unnamed) entry to -- ShowNewScorePanel()/HandleInput's
    // name-entry flow need it to find and label the right entry, whatever is
    // being browsed at the time.
    bool ShowsTrack(int track) const { return (gameSettings->scoreTracks() >> track) & 1; }
    bool ShowsBoth() const { return gameSettings->scoreTracks() == 3; }
    int pendingHighscoreTrack = 0;

    // What SaveNewHighscores() last put on disk, so a save can skip the table
    // that has not changed. Empty until the first save, which therefore always
    // writes both files. Indexed the same way as the two score vectors below
    // (0 = keyboard/gamepad, 1 = mouse/touch).
    std::string lastSavedHistory;
    std::string lastSavedLevelset[2];

    void SaveNewHighscores();
    void LoadLevelsetHighscores(const char *path, int track);
    void LoadHighscoreLevels(const char *path);
    // Hit-tests a click/tap (already converted to logical 640x480 canvas
    // coordinates) against the two score-screen tab boxes and switches
    // toggles that track if it landed on one. See ScoreTrackTabRect() in the .cpp.
    bool TapScoreTrackTab(float lx, float ly);

    // The WORLD tabs (worldscores.h): this port's server's boards for the
    // same two tracks, next to this device's own tables -- WORLD LEVEL
    // (furthest level) and WORLD POINTS (most points in one life). UP/DOWN
    // cycles MY SCORES -> WORLD LEVEL -> WORLD POINTS, or a tap on a tab picks
    // one; in a world view ENTER (or a tap on the button) opens the same
    // boards as a web page.
    bool viewWorld = false;
    bool viewPoints = false;  // which world board, while viewWorld
    Uint64 worldFetchedAt = 0;
    int ScopeTab() const { return viewWorld ? (viewPoints ? 2 : 1) : 0; }
    void SetScopeTab(int tab);
    void SetViewWorld(bool world);
    void RenderWorldBoard();
    bool TapWorldControls(float lx, float ly);
    void OpenWorldPage();
    void CreateLevelImages();

    SDL_Surface *backgroundSfc, *useBubbles[8];
    SDL_Texture *smallBG[10] = { nullptr }, *highscoresBG, *highscoreFrame, *headerLevelset, *headerMptrain;

    TTF_Font *highscoreFont;
    TTFText panelText, nameInput;
    // Separate from panelText: the score screen's own "KEYBOARD/GAMEPAD" vs
    // "MOUSE/TOUCH" track label and switch hint need their own style/color,
    // and panelText's is shared mutable state that ShowNewScorePanel()/
    // RenderPanel() also rely on staying whatever they last set it to.
    TTFText trackLabelText;

    SDL_Rect voidPanelRct = {(640/2) - (341/2), (480/2) - (280/2), 341, 280};
    SDL_Texture *voidPanelBG;

    std::string newName;
    int textTickWait = TEXTANIM_TICKSPEED;

    bool awaitKeyType = false, showTick = true;

    HighscoreManager(SDL_Renderer *rend);
    ~HighscoreManager();
    static HighscoreManager* ptrInstance;
};

#endif //HIGHSCOREMANAGER_H